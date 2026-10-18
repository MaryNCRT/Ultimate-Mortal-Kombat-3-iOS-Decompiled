#!/usr/bin/env python3
"""mkdata.py -- recover the initial value of every global from the binary.

`mkglobals.py` gives each global storage. Storage is not enough: 498 of the 779
globals it emits live in `__DATA,__data`, which is the section for data that is
**initialised in the image**, and giving those zero is not a neutral default --
it is the wrong value. A zeroed `IdlesPerPlayer` is a null frame list. A zeroed
`SoundList` is a list of unnamed sounds. A zeroed button table is a menu whose
every control sits at the origin.

So this reads what the linker actually put there.

    vmaddr = file offset + 0x1000

is the whole mapping, verified for this binary, and it means the bytes behind
any global are `binary[addr - 0x1000 : ...]`. The extent comes from the symbol
table the same way `mkglobals.py` measures arrays: the gap to the next symbol.

## Why this emits C and not a blob

Copying the bytes into memory at startup would run, and it would be useless.
A word in this data is one of two things and the difference does not survive a
`memcpy`:

    0x3f800000      the float 1.0
    0x0017118c      the ADDRESS of Player_KANO_Idles

A 32-bit image's pointers are 4 bytes; the host's are 8. Blit the table and
every pointer in it is garbage. Emit `&Player_KANO_Idles` and the host linker
computes the right address for its own word size -- and the output says what
the value MEANS, which a hex dump never does.

That is also why a word that looks like an address inside an integer-typed
global is reported rather than emitted: it is the signature of a declaration
that has the wrong type, and `IdlesPerPlayer` -- declared `long *[]` when the
first word of each pair is a float -- was found exactly this way.

## What it does not do

A struct with no body in the decomp cannot be initialised, because the field
offsets are unknown; `BUTTONNEW` is forward-declared only, and its 26 globals
are listed at the end of the run for whoever writes that struct. Multi-
dimensional arrays are emitted flat only when C allows it. Everything skipped is
counted and named, never silently dropped.
"""

import re
import struct
import sys

VM_BIAS = 0x1000

# Sections holding data that is initialised in the image. `__common` and `__bss`
# are zero by definition -- nothing to recover, and reading them would be reading
# whatever the file has at an address the loader never copies from.
INIT_SECTIONS = ("__DATA,__data", "__DATA,__const", "__TEXT,__const",
                 "__TEXT,__cstring")

SCALAR_SIZE = {
    "char": 1, "signed char": 1, "unsigned char": 1,
    "short": 2, "unsigned short": 2,
    "int": 4, "unsigned int": 4, "long": 4, "unsigned long": 4,
    "float": 4,
}

# `long` is 4 bytes in the image and 4 bytes under mingw, which is why the
# decomp spells word-sized things `long` throughout. Pointers are the one width
# that differs (4 vs 8), and pointers are exactly what this emits symbolically.


class Image(object):
    """The binary plus its symbol table, addressable by name."""

    def __init__(self, binary_path, symbols_path):
        with open(binary_path, "rb") as fh:
            self.data = fh.read()

        self.addr = {}          # name -> vmaddr
        self.sect = {}          # name -> section
        by_addr = {}
        for line in open(symbols_path, encoding="utf-8", errors="replace"):
            p = line.split()
            # STAB lines repeat every symbol with address 0 and section <abs>;
            # only the SECT lines carry a real address.
            if len(p) < 5 or not p[0].startswith("0x") or p[1] != "SECT":
                continue
            name = p[-1]
            if not name.startswith("_"):
                continue
            name = name[1:]
            # A file-static in a C++ translation unit is mangled `_ZL<len>
            # <name>`. GameCode.cpp's `TaskFunctionList` -- the ten-entry table
            # GameCodeMain dispatches the whole game through -- is
            # `__ZL16TaskFunctionList`, and under that spelling it was never
            # found, so the table came out empty.
            m = re.match(r"_ZL(\d+)(\w+)$", name)
            if m and len(m.group(2)) == int(m.group(1)):
                name = m.group(2)
            a = int(p[0], 16)
            if name in self.addr and p[1] != "SECT":
                continue
            self.addr[name] = a
            self.sect[name] = p[3]
            by_addr.setdefault(a, []).append(name)

        # A function-local `static` is emitted as `<name>.<number>` --
        # DrawControls' nine knob offsets are `_JoyOffset.11128` in
        # __TEXT,__const -- and under that spelling the decomp's `JoyOffset`
        # was never found, so the table came out zero and the on-screen stick
        # never moved. The plain name is added as an alias, unless a real
        # symbol already has it.
        for name in list(self.addr):
            m = re.match(r"(\w+)\.\d+$", name)
            if m and m.group(1) not in self.addr:
                base = m.group(1)
                self.addr[base] = self.addr[name]
                self.sect[base] = self.sect[name]
                by_addr.setdefault(self.addr[name], []).append(base)

        # One address can carry several names. Prefer the shortest, then
        # alphabetical: it is stable across runs and reads as the real name
        # rather than an alias.
        self.name_at = dict((a, sorted(ns, key=lambda s: (len(s), s))[0])
                            for a, ns in by_addr.items())

        self.extent = self._extents()
        self._sorted = sorted(self.name_at.items())
        # __common is zero-fill: its symbols lie past the end of the file, so
        # is_address() says no to them. Pointers INTO a symbol are judged by
        # the symbol table's own range instead.
        self.sym_end = max(a + self.extent.get(n, 0) for a, n in self._sorted)

    def containing(self, word):
        """The symbol whose extent holds `word` past its start, and the offset.

        `HUDANIM_ttl` points its second and third entries at `HUDTextures + 4`
        and `+ 8`, and `BloodTexturesToLoad` walks `BloodTextures` the same
        way. Neither address starts a symbol, so the table used to stop at
        the first of them and the rest came out zero."""
        import bisect
        i = bisect.bisect_right(self._sorted, (word, "￿")) - 1
        if i < 0:
            return None, 0
        a, name = self._sorted[i]
        off = word - a
        if 0 < off < self.extent.get(name, 0):
            return name, off
        return None, 0

    def _extents(self):
        """Each data symbol's size: the gap to the next symbol in its section."""
        per_section = {}
        for name, a in self.addr.items():
            per_section.setdefault(self.sect[name], set()).add((a, name))

        out = {}
        for entries in per_section.values():
            ordered = sorted(entries)
            for i, (a, name) in enumerate(ordered):
                j = i
                while j < len(ordered) and ordered[j][0] == a:
                    j += 1
                if j < len(ordered):
                    out[name] = max(out.get(name, 0), ordered[j][0] - a)
        return out

    def initialised(self, name):
        return self.sect.get(name) in INIT_SECTIONS

    def bytes_of(self, name, size=None):
        if name not in self.addr:
            return None
        off = self.addr[name] - VM_BIAS
        size = self.extent.get(name, 4) if size is None else size
        if off < 0 or off + size > len(self.data):
            return None
        return self.data[off:off + size]

    def is_address(self, word):
        """True if a word plausibly points into the image.

        The test is just "inside the mapped file", and it separates cleanly here
        because the image is 2.3 MB: the largest address is about 0x24D000,
        while a float's bits start at 0x3F000000 for 0.5 and go up. There is no
        overlap between the two ranges, so a float is never read as a pointer.
        """
        return VM_BIAS <= word < VM_BIAS + len(self.data)

    def cstring_at(self, addr, limit=512):
        """A NUL-terminated printable string at `addr`, or None."""
        off = addr - VM_BIAS
        if off < 0 or off >= len(self.data):
            return None
        end = self.data.find(b"\0", off, off + limit)
        if end < 0:
            return None
        raw = self.data[off:end]
        if not all(32 <= b < 127 or b in (9, 10, 13) for b in raw):
            return None
        return raw.decode("ascii")


def c_string(raw):
    """A C string literal for `raw`, escaped so it survives a compiler."""
    out = []
    for ch in raw:
        if ch == '"':
            out.append('\\"')
        elif ch == "\\":
            out.append("\\\\")
        elif ch == "\t":
            out.append("\\t")
        elif ch == "\n":
            out.append("\\n")
        elif ch == "\r":
            out.append("\\r")
        else:
            out.append(ch)
    return '"%s"' % "".join(out)


def word_literal(img, word, want_pointer, warn, owner, quiet=False,
                 pointee="char", known=None, demand=None,
                 functions=None):
    """Spell one 32-bit word, as a pointer if the declared type wants one.

    With `quiet`, a word that cannot be spelled as a pointer returns None
    instead of warning. The caller uses that as a terminator: an array of
    pointers ends at the first word that is not one.

    `pointee` is the declared type behind the pointer and `known` the set of
    names that exist in the generated file. Both are limits on what can honestly
    be written: an address whose symbol nothing declares cannot be named, and
    text belongs only in a pointer to char.
    """
    if want_pointer:
        if word == 0:
            return "NULL"
        # A symbol before a string. Both can describe the same address, and the
        # symbol is the one that is always right: `TEXTURETOLOAD.dest` points at
        # a `TEXTURE *` variable whose first bytes happened to be printable, so
        # trying text first spelled a pointer-to-pointer as a string literal and
        # the compiler rejected it. String constants in `__TEXT,__cstring`
        # carry no symbol of their own, so they still come out as text.
        target = img.name_at.get(word)
        if target and (known is None or target in known):
            return "(void *)&%s" % target
        # A function pointer carries the Thumb bit in bit 0: `blx` reads it to
        # decide which instruction set to enter. Mask it before asking the
        # symbol table, which stores the even address.
        if word & 1:
            fn = img.name_at.get(word & ~1)
            if fn and img.sect.get(fn) == "__TEXT,__text":
                if known is None or fn in known:
                    return fn
                if functions is not None:
                    functions.add(fn)
                    return fn
        if target and demand is not None and pointee in SCALAR_SIZE:
            # A real symbol that no decompiled file declares. `IdlesPerPlayer`
            # points at `Player_KANO_Idles` and twenty-five more like it, and
            # none of them is named anywhere in the decomp -- the front end only
            # ever reaches them through this table, so nothing had to name them.
            #
            # Everything needed to write one down is nonetheless known: the
            # field that points at it gives the element type, the gap to the
            # next symbol gives the length, and the image gives the bytes. So
            # the symbol is asked for here and defined alongside the table.
            demand[target] = pointee
            return "(void *)&%s" % target
        # Only a pointer to char can hold text. `BloodTextures` is an address
        # with a symbol nothing in the decomp declares and bytes that happen to
        # be printable -- naming it would not compile and quoting it would put a
        # string in a `TEXTURE **`. Neither is true, so neither is written.
        if pointee == "char":
            s = img.cstring_at(word)
            if s is not None:
                return c_string(s)
        # Inside a known symbol, past its start: a pointer to an element.
        # Tried after text, because anonymous strings in __data sit inside
        # whatever symbol precedes them.
        if target is None and VM_BIAS <= word < img.sym_end:
            inner, off = img.containing(word)
            if inner and (known is None or inner in known):
                return "(void *)((char *)&%s + %d)" % (inner, off)
        # An address with no symbol and no string: emitting a number here would
        # be a pointer to nothing on the host, so say so instead.
        if quiet:
            return None
        warn.setdefault(owner, set()).add("unresolved address 0x%08x" % word)
        return "NULL"

    if img.is_address(word) and img.name_at.get(word):
        # A word that IS a symbol's address sitting in an integer field. The
        # value cannot be carried across word sizes, and the declaration is
        # probably wrong. Report; emit the number so the file still compiles.
        warn.setdefault(owner, set()).add(
            "holds the address of %s in a non-pointer field"
            % img.name_at[word])
    return "0x%08x" % word if word > 9 else str(word)


def float_literal(bits):
    """`bits` as a C float literal, always with a decimal point.

    `%g` prints -51.0 as "-51", and "-51f" is not a float in C, it is an
    integer with a stray suffix -- which the compiler says in exactly those
    words. The point has to be there before the f.
    """
    v = struct.unpack("<f", struct.pack("<I", bits))[0]
    if v != v or v in (float("inf"), float("-inf")):
        return "0.0f  /* 0x%08x */" % bits
    text = "%.9g" % v
    if "." not in text and "e" not in text and "n" not in text:
        text += ".0"
    return text + "f"


def scalar_values(img, raw, typ):
    """Decode `raw` as a list of C literals for scalar type `typ`."""
    size = SCALAR_SIZE[typ]
    n = len(raw) // size
    if typ == "float":
        return [float_literal(w) for w in struct.unpack("<%dI" % n, raw[:4 * n])]
    fmt = {1: "b", 2: "h", 4: "i"}[size]
    if typ.startswith("unsigned"):
        fmt = fmt.upper()
    return [str(v) for v in struct.unpack("<%d%s" % (n, fmt), raw[:size * n])]


def trim(values, zero="0"):
    """Drop the trailing zeros; C fills the rest of an array with them anyway.

    This is not cosmetic. `SoundListUniqueNames` is 32 KB of mostly nothing and
    written out in full it is 8,000 lines of `0,`.
    """
    end = len(values)
    while end > 0 and values[end - 1] in (zero, "0", "0.0f", "NULL"):
        end -= 1
    return values[:end]


def wrap(values, per_line=8, indent="    "):
    lines = []
    for i in range(0, len(values), per_line):
        lines.append(indent + " ".join(v + "," for v in values[i:i + per_line]))
    return "\n".join(lines)


# A struct body the decomp states, so its fields can be filled one at a time.
# The tag is optional: `typedef struct TEXTURETOLOAD { ... } TEXTURETOLOAD;` and
# the anonymous form both appear, and matching only the anonymous one quietly
# left four struct types without a layout.
STRUCT_BODY = re.compile(
    r"typedef\s+struct\s*(?:[A-Za-z_]\w*\s*)?\{(?P<body>[^{}]*)\}\s*"
    r"(?P<name>\w+)\s*;", re.S)

STRUCT_TAG_BODY = re.compile(
    r"(?<!typedef )\bstruct\s+(?P<name>[A-Za-z_]\w*)\s*\{(?P<body>[^{}]*)\}\s*;",
    re.S)

# `type stars name;` or `type stars name[n];` -- one field.
FIELD = re.compile(
    r"^\s*(?P<type>(?:const\s+|unsigned\s+|signed\s+|struct\s+)*[A-Za-z_]\w*)\s+"
    r"(?P<stars>\**)\s*(?P<name>\w+)\s*(?:\[(?P<count>[^\]]*)\])?\s*$")


def _strip_comments(text):
    return re.sub(r"/\*.*?\*/", "", text, flags=re.S)


def struct_layouts(sources):
    """{struct name: [(field type, is pointer)]}, one entry per 4-byte word.

    A field the pattern does not recognise makes the whole struct unusable.
    That is the point: `MOVESLISTENTRY` opens with `MOVESSECTION section[2]`,
    and skipping the line it could not parse produced a three-word layout for a
    thirteen-word struct -- an initialiser that compiles, runs, and is wrong in
    every entry. A struct is either understood completely or not at all.

    Nested structs are expanded when their own layout is already known, which is
    why this iterates to a fixed point rather than making one pass.
    """
    bodies = {}
    for text in sources:
        # Comments first, and over the whole file rather than per body. The
        # decomp explains itself, and one of those explanations is a block
        # comment in RenderSkinned.c that draws TEXTURETOLOAD out field by
        # field. It matched as a definition and overwrote the real one with
        # prose, so the struct silently lost its layout. A comment is not code.
        for m in STRUCT_BODY.finditer(_strip_comments(text)):
            bodies[m.group("name")] = m.group("body")
        # `struct BUTTONNEW { ... };` with the typedef elsewhere (FrontEnd.c
        # forward-declares it and gives the body later) is the same layout.
        for m in STRUCT_TAG_BODY.finditer(_strip_comments(text)):
            bodies.setdefault(m.group("name"), m.group("body"))

    out = {}
    for _ in range(len(bodies) + 1):
        progress = False
        for name, body in bodies.items():
            if name in out:
                continue
            fields, ok = [], True
            pack, packed = [], 0        # sub-word fields filling one word
            for stmt in body.split(";"):
                if not stmt.strip():
                    continue
                f = FIELD.match(stmt)
                if not f:
                    ok = False
                    break
                typ = re.sub(r"^(?:const|struct)\s+", "",
                             f.group("type").strip()).strip()
                ptr = bool(f.group("stars"))
                try:
                    count = int(f.group("count") or "1", 0)
                except ValueError:
                    ok = False
                    break
                if (not ptr and typ in SCALAR_SIZE and SCALAR_SIZE[typ] < 4
                        and count == 1):
                    # `BUTTONNEW` has `short thickness; short pad;` at 0x0c:
                    # two halves of one word. Collect them until they fill
                    # it; a struct where they do not is not understood.
                    pack.append(typ)
                    packed += SCALAR_SIZE[typ]
                    if packed == 4:
                        fields.append((("pack",) + tuple(pack), False))
                        pack, packed = [], 0
                    elif packed > 4:
                        ok = False
                        break
                    continue
                if packed:
                    ok = False          # a partial word before a full one
                    break
                if ptr:
                    fields.extend([(typ, True)] * count)
                elif typ in SCALAR_SIZE and SCALAR_SIZE[typ] == 4:
                    fields.extend([(typ, False)] * count)
                elif typ in out:
                    fields.extend(out[typ] * count)
                else:
                    ok = False          # sub-word field, or a struct not yet known
                    break
            if ok and fields and not packed:
                out[name] = fields
                progress = True
        if not progress:
            break
    return out


def defines(sources):
    """`#define NAME <integer>` from the decomp, for array dimensions."""
    out = {}
    pat = re.compile(r"^#define\s+(\w+)\s+\(?\s*(-?\d+|0x[0-9a-fA-F]+)\s*\)?\s*$",
                     re.M)
    for text in sources:
        for m in pat.finditer(text):
            out[m.group(1)] = int(m.group(2), 0)
    return out


def inner_dim(dims, defs):
    """The last dimension of `dims` as a number, or None."""
    parts = re.findall(r"\[([^\]]*)\]", dims)
    if not parts:
        return None
    last = parts[-1].strip()
    if not last:
        return None
    try:
        return int(last, 0)
    except ValueError:
        return defs.get(last)


def initialiser(img, name, typ, stars, dims, layouts, warn, defs=None,
                known=None, demand=None, functions=None):
    """A C initialiser for one global, or None if it cannot be spelled.

    Returns the text that goes after `=`. `dims` is the declared dimensions as
    written, which decides whether a flat brace list is legal C.
    """
    defs = defs or {}
    base = re.sub(r"^const\s+", "", typ).strip()

    if not img.initialised(name):
        return None                          # __common / __bss: zero is right
    raw = img.bytes_of(name)
    if not raw or not any(raw):
        return None                          # zero already

    ndims = dims.count("[")

    # ---- an array of pointers: strings and symbols -------------------------
    if stars and ndims == 1:
        words = struct.unpack("<%dI" % (len(raw) // 4),
                              raw[:4 * (len(raw) // 4)])
        vals = []
        for w in words:
            cell = word_literal(img, w, True, warn, name, quiet=True,
                                pointee=base, known=known,
                                demand=demand, functions=functions)
            if cell is None:
                # The first word that is not a pointer is where the array ends.
                # A measured extent is an UPPER bound -- it runs to the next
                # symbol, and a string table with no symbol on its first string
                # gets swallowed into the array in front of it. `Destination-
                # Master` measured 23 entries past its end this way, and every
                # one of them was the bytes of the very strings it points at.
                break
            vals.append(cell)
        vals = trim(vals, zero="NULL")
        if not vals:
            return None
        return "{\n%s\n}" % wrap(vals, 4)

    if stars:
        return None                          # a slot: handled by the caller

    # ---- a struct with a stated body ---------------------------------------
    if base in layouts:
        fields = layouts[base]
        stride = 4 * len(fields)
        blank = "{ %s }" % ", ".join(
            "NULL" if f[1] else
            (", ".join("0" for _ in f[0][1:]) if isinstance(f[0], tuple)
             else "0")
            for f in fields)
        entries = []
        for off in range(0, len(raw) - stride + 1, stride):
            chunk = struct.unpack("<%dI" % len(fields), raw[off:off + stride])
            cells, ended = [], False
            for (ftype, fptr), w in zip(fields, chunk):
                if fptr:
                    # `ftype`, not `base`: the pointee is the FIELD's type.
                    # Passing the struct's own name made every pointer field
                    # unspellable, and the terminator then read that as "the
                    # table ends at entry zero" -- a correct rule fed a wrong
                    # type emptied the table instead of failing loudly.
                    cell = word_literal(img, w, True, warn, name, quiet=True,
                                        pointee=ftype, known=known,
                                        demand=demand,
                                        functions=functions)
                    if cell is None:
                        # A pointer field holding something that is not a
                        # pointer means the table ended before here. The extent
                        # is the gap to the next SYMBOL, and a string blob with
                        # no symbol on it gets counted into the table in front
                        # of it: `st_lia` measured 55 entries and 25 of them
                        # were the bytes of its own sound names -- 0x6f626f52
                        # is "Robo", read as an address.
                        ended = True
                        break
                    cells.append(cell)
                elif isinstance(ftype, tuple):
                    # A packed word, low bytes first, as the image lays it out.
                    shift = 0
                    for part in ftype[1:]:
                        bits = 8 * SCALAR_SIZE[part]
                        v = (w >> shift) & ((1 << bits) - 1)
                        if not part.startswith("unsigned") and v >> (bits - 1):
                            v -= 1 << bits
                        cells.append(str(v))
                        shift += bits
                elif ftype == "float":
                    cells.append(float_literal(w))
                else:
                    cells.append(word_literal(img, w, False, warn, name))
            if ended:
                break
            entries.append("{ %s }" % ", ".join(cells))
        while entries and entries[-1] == blank:
            entries.pop()
        if not entries:
            return None
        if ndims == 0:
            return entries[0]
        return "{\n%s\n}" % "\n".join("    %s," % e for e in entries)

    if base not in SCALAR_SIZE:
        warn.setdefault(name, set()).add("no layout for type `%s`" % base)
        return None

    # ---- a scalar -----------------------------------------------------------
    if ndims == 0:
        word = struct.unpack("<I", raw[:4].ljust(4, b"\0"))[0]
        if base == "float":
            return float_literal(word)
        return word_literal(img, word, False, warn, name)

    if base == "char" and ndims == 1:
        s = img.cstring_at(img.addr[name])
        if s is not None and len(s) + 1 <= len(raw):
            return c_string(s)

    vals = trim(scalar_values(img, raw, base),
                zero="0.0f" if base == "float" else "0")
    if not vals:
        return None

    # ---- more than one dimension -------------------------------------------
    if ndims > 1:
        # A flat list is not legal C for `long X[R][C]`, and inventing the row
        # split would be a guess -- except that the declaration states the inner
        # dimension, so it is not a guess, it is arithmetic.
        cols = inner_dim(dims, defs)
        if not cols:
            warn.setdefault(name, set()).add(
                "%d dimensions, inner extent unknown" % ndims)
            return None
        if ndims > 2:
            warn.setdefault(name, set()).add("%d dimensions" % ndims)
            return None
        while len(vals) % cols:
            vals.append("0.0f" if base == "float" else "0")
        rows = ["    { %s }," % ", ".join(vals[i:i + cols])
                for i in range(0, len(vals), cols)]
        return "{\n%s\n}" % "\n".join(rows)

    return "{\n%s\n}" % wrap(vals, 4 if base == "float" else 8)


def report(warn, out=sys.stderr):
    if not warn:
        return
    print("mkdata: %d globals need a look:" % len(warn), file=out)
    for name in sorted(warn):
        for why in sorted(warn[name]):
            print("    %-34s %s" % (name, why), file=out)


def synthesise(img, name, pointee, layouts, warn, defs, known, demand):
    """Declaration and definition for a symbol only a pointer names.

    The element type comes from the pointer that reached it, the length from
    the symbol table, and the contents from the image. Nothing here is invented:
    a `const long *` field pointing at a symbol 40 bytes long makes a
    `long name[10]`, and the ten values are read, not guessed.
    """
    size = img.extent.get(name)
    if not size or pointee not in SCALAR_SIZE:
        return None
    count = size // SCALAR_SIZE[pointee]
    if count < 1:
        return None
    dims = "[%d]" % count
    text = initialiser(img, name, pointee, "", dims, layouts, warn, defs,
                       known, demand)
    decl = "extern %s %s%s;" % (pointee, name, dims)
    body = "%s %s%s%s;" % (pointee, name, dims,
                           (" = %s" % text) if text else "")
    return decl, body


def image_stride(typ, layouts):
    """Bytes one `typ` occupies in the image, or None if the host's will do.

    Every field in a recovered layout is one 32-bit word, so the stride is just
    the field count times four -- and it is the number to divide a measured
    extent by, because the extent was measured in the image.
    """
    fields = layouts.get(typ)
    return 4 * len(fields) if fields else None


def function_table(img, name, warn, known, functions):
    """A brace list of function names for a table of code pointers.

    Returns (count, text) or None. `count` is the entry count in the IMAGE,
    where a pointer is four bytes -- the host's are eight, so the declared
    length has to come from here and not from a sizeof.
    """
    raw = img.bytes_of(name)
    if not raw or not any(raw):
        return None
    import struct as _s
    words = _s.unpack("<%dI" % (len(raw) // 4), raw[:4 * (len(raw) // 4)])
    cells, seen_any = [], False
    for w in words:
        if w == 0:
            cells.append("NULL")
            continue
        fn = img.name_at.get(w & ~1) if (w & 1) else img.name_at.get(w)
        if fn and img.sect.get(fn) == "__TEXT,__text" and fn in known:
            cells.append(fn)
            seen_any = True
        else:
            cells.append("NULL")
            if fn:
                warn.setdefault(name, set()).add(
                    "entry points at %s, which is not in the tree" % fn)
    while cells and cells[-1] == "NULL":
        cells.pop()
    if not seen_any or not cells:
        return None
    body = "\n".join("    %s," % c for c in cells)
    return len(words), "{\n%s\n}" % body
