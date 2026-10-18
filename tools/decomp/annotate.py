#!/usr/bin/env python3
"""
annotate.py -- disassemble a function with everything resolved.

Raw disassembly of this binary is unreadable in five separate ways, and each
needs a different lookup:

  1. `bl #0x127974`          an import stub      -> tools/stubs.py has the name
  2. `bl #0x55388`           an INTERNAL call    -> the symbol table has it
  3. `ldr r0, [pc, #0x2c]`   a literal pool      -> read the four bytes
  4. `ldr rN, [pc, #M]` then `add rN, pc`        -> a PC-RELATIVE ADDRESS
  5. ...then `ldr rM, [rN]`                      -> that address is a POINTER
                                                    SLOT and the value it holds
                                                    is what the code is after

The fourth and fifth are what this tool exists for, and they are why an earlier
version of it was nearly useless. It only understood the three-operand
`add rN, pc, rN`; the compiler emits the two-operand `add rN, pc` almost
everywhere, so the interesting addresses came out as bare literals like
`0x000d6dfa` and got resolved by hand, one at a time, in every batch.

The fifth case is the same hand work one level deeper. `getTransferableFlags`
computes 0x000f357c, which the symbol table calls `___utf16_string_11+0x4e7a`
and which is therefore not a string at all -- it is a slot holding 0x0038c1fc,
and THAT is `_G`, the game state. Three lookups deep, and only the last one
means anything.

    python tools/annotate.py work/UMK3.armv7 _getTransferableFlags
    python tools/annotate.py work/UMK3.armv7 _LIME_RenderMeshSingle --calls

`--calls` prints only the lines that resolved to something, which is usually
what you want first.

The symbol table comes from $UMK3_SYMBOLS, or work/symbols.txt.

## An exact symbol match is worth more than a near one

`_G` and `___utf16_string_11+0x4e7a` are both "the symbol at that address", but
only the first is a fact about the variable; the second means the address fell
between two symbols and the nearest preceding one is being reported. Near
matches are marked `(near)` and should be treated as a hint, not a name.

## Literals are tracked, and invalidated

A register's literal is followed across `mov rD, rS`, so the common shape --
hoist a base out of a loop, copy it in, `add rN, pc` -- resolves. Just as
importantly, **any other write to a register clears it**. Before that, a stale
literal survived a reload and the next `add` resolved against it, naming an
address that had nothing to do with it.

## One thing this tool still cannot do for you

It annotates each instruction where it stands. It does **not** pair a literal
with the call that consumes it, because the compiler interleaves them: in
`LIME_RenderMeshSingle` a `GL_VERTEX_ARRAY` is loaded immediately before a
`glClientActiveTexture`, which does not take that enum. The value belongs to a
later call and the register was simply free.

So read the annotations as "these constants appear in this function", not as
"this call takes this argument". Getting that wrong writes a confident and
false comment, which is the failure mode this whole project is built to avoid.
The way to settle a pairing is to RUN the function -- see tests/gl_trace.c.
"""
import bisect
import os
import re
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "OUTPUT", "tools"))
import macho  # noqa: E402

DISASM = os.path.join(HERE, "disasm.py")
if not os.path.isfile(DISASM):
    DISASM = os.path.join(HERE, "..", "OUTPUT", "tools", "disasm.py")

VA_BASE = 0x1000        # vmaddr = file offset + 0x1000 in the extracted slices

# The GL ES 1.1 enums this engine actually uses. Only names that appear in the
# binary's own import list are here -- a constant is named when it is one of
# these and left as a number otherwise.
GL_ENUMS = {
    0x0001: "GL_ONE",            0x0004: "GL_TRIANGLES",
    0x0302: "GL_SRC_ALPHA",      0x0303: "GL_ONE_MINUS_SRC_ALPHA",
    0x0405: "GL_BACK",           0x0b44: "GL_CULL_FACE",
    0x0b71: "GL_DEPTH_TEST",     0x0be2: "GL_BLEND",
    0x0de1: "GL_TEXTURE_2D",     0x1400: "GL_BYTE",
    0x1401: "GL_UNSIGNED_BYTE",  0x1402: "GL_SHORT",
    0x1403: "GL_UNSIGNED_SHORT", 0x1406: "GL_FLOAT",
    0x1700: "GL_MODELVIEW",      0x1701: "GL_PROJECTION",
    0x1702: "GL_TEXTURE",        0x1d00: "GL_SMOOTH",
    0x1d01: "GL_FLAT",           0x1e01: "GL_REPLACE",
    0x2100: "GL_MODULATE",       0x2200: "GL_TEXTURE_ENV_MODE",
    0x2300: "GL_TEXTURE_ENV",    0x8074: "GL_VERTEX_ARRAY",
    0x8075: "GL_NORMAL_ARRAY",   0x8076: "GL_COLOR_ARRAY",
    0x8078: "GL_TEXTURE_COORD_ARRAY",
    0x84c0: "GL_TEXTURE0",       0x84c1: "GL_TEXTURE1",
    0x84c2: "GL_TEXTURE2",
}


# ------------------------------------------------------------- symbol table

class Symbols(object):
    """Address -> name, with nearest-preceding lookup."""

    def __init__(self, path):
        self.addrs = []
        self.names = []
        if not path or not os.path.isfile(path):
            return
        rows = []
        with open(path, encoding="utf-8", errors="ignore") as fh:
            for line in fh:
                p = line.split()
                if len(p) < 2:
                    continue
                try:
                    a = int(p[0], 16)
                except ValueError:
                    continue
                rows.append((a, p[-1]))
        rows.sort()
        self.addrs = [a for a, _ in rows]
        self.names = [n for _, n in rows]

    def lookup(self, addr):
        """Return (name, exact)."""
        if not self.addrs:
            return (None, False)
        i = bisect.bisect_right(self.addrs, addr) - 1
        if i < 0:
            return (None, False)
        base, name = self.addrs[i], self.names[i]
        if base == addr:
            return (name, True)
        # A long way past the last symbol is not a match, it is the end of the
        # table. 64 KiB is generous for one object and short enough that
        # nothing lands on it by accident.
        if addr - base > 0x10000:
            return (None, False)
        return ("%s+0x%x" % (name, addr - base), False)

    def describe(self, addr):
        name, exact = self.lookup(addr)
        if name is None:
            return "0x%08x" % addr
        return "0x%08x %s%s" % (addr, name, "" if exact else "  (near)")


# ------------------------------------------------------------------ the file

def load(path):
    with open(path, "rb") as fh:
        data = fh.read()
    return data, macho.MachO(data).stub_map()


def read_u32(data, va):
    off = va - VA_BASE
    if off < 0 or off + 4 > len(data):
        return None
    return struct.unpack("<I", data[off:off + 4])[0]


def read_cstr(data, va, limit=72):
    off = va - VA_BASE
    if off < 0 or off >= len(data):
        return None
    out = []
    while off < len(data) and len(out) < limit:
        b = data[off]
        if b == 0:
            break
        if b < 0x20 or b > 0x7E:
            return None
        out.append(chr(b))
        off += 1
    return "".join(out) if len(out) >= 3 else None


def resolve_symbol(name, symbols_path):
    """Accept a plain C name and find its mangled form in the symbol table."""
    if not symbols_path or not os.path.isfile(symbols_path):
        return name
    want = name.lstrip("_")
    best = None
    with open(symbols_path, encoding="utf-8", errors="ignore") as fh:
        for line in fh:
            p = line.split()
            if len(p) < 2:
                continue
            sym = p[-1]
            s2 = sym.lstrip("_")
            m = re.match(r"Z(\d+)(.*)", s2)
            plain = m.group(2)[:int(m.group(1))] if m else s2
            if plain == want:
                best = sym
                if m:
                    break
    return best or name


# ------------------------------------------------------------------ the work

RE_LDR_PC = re.compile(
    r"0x([0-9a-f]+)\s+ldr[.\w]*\s+(\w+), \[pc, #(?:0x)?[0-9a-f]+\]\s*;\s*=0x([0-9a-f]+)")
RE_ADD_PC2 = re.compile(r"0x([0-9a-f]+)\s+add[.\w]*\s+(\w+), pc\s*$")
RE_ADD_PC3 = re.compile(r"0x([0-9a-f]+)\s+add[.\w]*\s+(\w+), pc, (\w+)\s*$")
RE_MOV = re.compile(r"0x[0-9a-f]+\s+mov[.\w]*\s+(\w+), (\w+)\s*$")
# The destination register of any instruction: the first operand. Used to
# INVALIDATE a tracked literal, never to create one.
RE_DEST = re.compile(r"0x[0-9a-f]+\s+([a-z][a-z0-9.]*)\s+(\w+)[,\s]")
RE_LDR_REG = re.compile(r"0x([0-9a-f]+)\s+ldr[.\w]*\s+(\w+), \[(\w+)\]\s*$")
RE_CALL = re.compile(r"\b(?:bl|blx)\s+#0x([0-9a-f]+)")


def annotate(binary, func, symbols_path, calls_only=False):
    data, stubs = load(binary)
    syms = Symbols(symbols_path)
    mangled = resolve_symbol(func, symbols_path)

    out = subprocess.run([sys.executable, DISASM, binary, mangled],
                         capture_output=True, text=True).stdout.splitlines()
    if not out:
        raise SystemExit("no disassembly for %s" % func)

    print(out[0])
    lines = out[1:]

    literals = {}       # register -> the literal it was last loaded with
    addresses = {}      # register -> a resolved PC-relative address
    notes = []

    for line in lines:
        note = []

        # ---- calls: import stubs first, then the binary's own functions ----
        m = RE_CALL.search(line)
        if m and "->" not in line:
            target = int(m.group(1), 16)
            name = stubs.get(target)
            if name is None:
                name, _ = syms.lookup(target)
            if name is None:
                name, _ = syms.lookup(target & ~1)      # Thumb bit
            if name:
                note.append("-> %s" % name)

        # ---- keep the tracked literals HONEST --------------------------
        #
        # This used to track only `ldr rN, [pc, ...]` and never clear it, so a
        # register reloaded by anything else still carried its OLD literal and
        # the next `add rN, pc` resolved against it -- producing a confident
        # `_KodeSelector+0x8 (near)` for an address that is really
        # `_KodeSelectorParticle`. A wrong name is worse than no name: it reads
        # like an answer.
        #
        # So every instruction now clears its destination first, and only the
        # two forms below put anything back.
        # The three forms the handlers below interpret set the tracking
        # themselves and must not be cleared out from under them.
        interpreted = (RE_LDR_PC.search(line) or RE_ADD_PC2.search(line)
                       or RE_ADD_PC3.search(line) or RE_MOV.match(line.strip()))
        m = RE_DEST.match(line.strip())
        if m and not interpreted and m.group(1) not in (
                "cmp", "cmn", "tst", "teq", "str", "strb", "strh", "strd",
                "stm", "stmia", "push", "b", "bl", "blx", "bx", "cbz", "cbnz"):
            literals.pop(m.group(2), None)
            addresses.pop(m.group(2), None)

        # ---- mov rD, rS : the literal travels with the copy ----
        #
        # The compiler hoists a base out of a loop and copies it in, so the
        # `add rN, pc` inside the loop is fed by a `mov`, not by a load. The
        # docstring called this a known gap; it is closed.
        m = RE_MOV.match(line.strip())
        if m:
            dst, src = m.group(1), m.group(2)
            if src in literals:
                literals[dst] = literals[src]
            if src in addresses:
                addresses[dst] = addresses[src]

        # ---- a literal pool load ----
        m = RE_LDR_PC.search(line)
        if m:
            reg, val = m.group(2), int(m.group(3), 16)
            literals[reg] = val
            addresses.pop(reg, None)
            if val in GL_ENUMS:
                note.append(GL_ENUMS[val])
            elif val < 0x10000:
                note.append("constant %d" % val)

        # ---- add rN, pc  /  add rN, pc, rN : a PC-relative ADDRESS ----
        m = RE_ADD_PC2.search(line) or RE_ADD_PC3.search(line)
        if m:
            here, reg = int(m.group(1), 16), m.group(2)
            if reg in literals:
                target = (here + 4 + literals[reg]) & 0xFFFFFFFF
                addresses[reg] = target
                text = read_cstr(data, target)

                # A target with NO exact symbol whose word IS one is a GOT-like
                # indirection slot: `add rN, pc` then `ldr rN, [rN]`. Naming it
                # here is safe in a way chasing a real symbol is not -- there is
                # no variable being read, only a relocation being followed, and
                # the alternative is resolving it by hand every single time.
                _n, exact = syms.lookup(target)
                if not exact:
                    word = read_u32(data, target)
                    if word is not None:
                        pointee, pexact = syms.lookup(word)
                        if pexact:
                            note.append("SLOT -> %s" % pointee)

                note.append("ADDRESS %s%s"
                            % (syms.describe(target),
                               (' "%s"' % text) if text else ""))

        # ---- ldr rM, [rN] where rN holds such an address: a POINTER SLOT ----
        #
        # **Exactly one level, and only on an exact symbol match.**
        #
        # Chasing further is not a deeper lookup, it is reading a VARIABLE out
        # of the file image. `switchToTask` loads &CurrentTask from a slot and
        # then loads CurrentTask itself; the second value is that variable's
        # initial contents, which happen to be 0. An earlier version chased it
        # and reported "SLOT holds 0x00000000 _zoomedoutweight" -- a real
        # symbol name attached to a number that has nothing to do with it,
        # which is precisely the kind of thing that ends up in a comment.
        #
        # Requiring an exact match is the other half of the guard: a nearest-
        # preceding hit on a data word is a coincidence, not a name.
        m = RE_LDR_REG.search(line)
        if m:
            dst, src = m.group(2), m.group(3)
            # Read the source BEFORE clearing the destination: `ldr r3, [r3]`
            # is the common form and they are the same register. Popping first
            # made the tool silently stop resolving exactly the case it was
            # written for, and it took running it over every previously
            # hand-derived answer to notice -- fourteen of fifteen still
            # matched, which is the kind of pass rate that hides a bug.
            base = addresses.get(src)
            addresses.pop(dst, None)
            literals.pop(dst, None)
            if base is not None:
                held = read_u32(data, base)
                if held:
                    name, exact = syms.lookup(held & ~1)
                    if exact:
                        note.append("SLOT holds 0x%08x %s" % (held, name))
                    else:
                        note.append("SLOT holds 0x%08x (no exact symbol)" % held)

        # ---- mov/movw immediates that happen to be GL enums ----
        m = re.search(r"\b(?:mov|movw)[.\w]*\s+(\w+), #(?:0x)?([0-9a-f]+)", line)
        if m:
            val = int(m.group(2), 16)
            if val in GL_ENUMS:
                note.append(GL_ENUMS[val])

        notes.append("   ; " + "  ".join(note) if note else "")

    for line, note in zip(lines, notes):
        # `--calls` keeps anything that resolved, including the calls disasm.py
        # had already named -- those are the most interesting lines in the
        # function and dropping them made the flag useless on any function
        # whose calls were all already known.
        if calls_only and not note and "->" not in line:
            continue
        print(line.rstrip() + note)


def main(argv):
    if len(argv) < 3:
        print(__doc__.strip())
        return 1
    symbols_path = os.environ.get("UMK3_SYMBOLS") or os.path.join(
        os.path.dirname(HERE), "work", "symbols.txt")
    annotate(argv[1], argv[2], symbols_path, "--calls" in argv[3:])
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
