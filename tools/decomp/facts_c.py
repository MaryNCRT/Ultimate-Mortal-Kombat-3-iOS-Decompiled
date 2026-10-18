#!/usr/bin/env python3
"""facts_c.py -- pull the SAME facts out of the readable decomp.

`facts_asm.py` reads the machine transcription and reports stores, calls,
handler targets and constants. This reads our own C and reports the same
things in the same vocabulary, so `factdiff.py` can put the two lists side by
side without either side knowing about the other.

## Why this one can be simple

The house style is regular, and that regularity is the whole reason a checker
is cheap here:

    obj->field1c = 3;                     a store into MK3OBJ at 0x1c
    obj->field00->field58 = 3;            a store into MK3OBJPROC at 0x58
    thread->fieldfc = 1;                  a store into MK3THREAD at 0xfc
    group_sound(obj);                     a call
    mk3_install(thread, (MK3THREADFUNC)t_jhp5)        a handler
    mk3_frame(thread, thread->frame)[1] = ...t_jhp5;  the same, spelled out
    *mk3_frame(thread, thread->frame + 1) = 0x807;    a state token
    return -3;                            the refusal

`mk3logic.h` supplies the offsets. A field spelled `fieldNN` carries its own
offset in its name; the ones with real names -- `a10`, `him`, `slave`, `frame`
-- carry it in the `/* 0xNN` comment beside them. Both are read, per struct,
because 0x44 is `a10` in MK3OBJ and `p_hit` in MK3OBJPROC and confusing the
two would invent a difference that is not there.

## What it deliberately does not do

It does not evaluate expressions. `obj->field1c = obj->field48;` reports a
store of `?`, not of whatever field48 held, for the same reason `facts_asm.py`
reports `?` rather than carrying a stale constant: a wrong fact stated with
confidence is worse than no fact. The diff treats `?` on either side as "not
compared" and says so in its summary, so the pair that cannot be checked is
visible rather than silently counted as agreement.

## What it does not read yet

Each of these shows up as a phantom difference until it is fixed, so anyone
adding to this file should check the list before chasing a report:

  * a loop. `for (i = 0; i < 5; i++) find_part2(obj);` reports ONE call where
    the binary has five unrolled. Reading the bound would fix it.
  * a store through a global: `*(uint32_t *)(G_BYTES + 0x378 + p * 4) = 0`.
    Nothing reached through `G` or `H` is checked at all.
  * a value that is an expression rather than a literal or a name. Following
    one level of copy -- `obj->field1c = obj->field48;` -- would recover a
    good many of the `?`.
  * a store statement split across more than one physical line. `RE_STORE`
    is anchored on one line including its own `;`, so `obj->field5c =` with
    the right-hand side starting on the NEXT line matched nothing at all --
    not even a `?` -- and the store vanished from the C's own fact list
    with no trace. `facts_of` now joins forward until it sees a `;` before
    giving up on a line that looks like a store's left side. Found in
    `proj_onscreen_test`/`proj_onscreen_test_unsafe` (mkzap.c).
  * `X = (cond) ? A : B;`. Once the line-join above stopped losing it
    entirely, this still read as one `value()` on the whole expression --
    `?`, always, one unit of slack. That undercounts a real two-way branch:
    `proj_onscreen_test` compiles to two actual `str`s (0 and 1, on two
    paths) and needed two units to clear. But `is_he_motaro`, same C shape
    one file over, compiles to ONE conditional-move with no branch at all --
    a single unresolved store on the binary side. Nothing in the C syntax
    says which the compiler chose, so a ternary-valued store now emits `?`
    TWICE (`RE_TERNARY`), giving up to two units of slack either way rather
    than asserting 0 and 1 as if the branch were guaranteed.

## The rule that keeps it honest

A name is a fact only when the symbol table has it. Everything else is `?`.
This file used to report `below` and `carried` -- locals invented here to
hold what the binary keeps in a register -- as though they were values, and
every one came out as a difference against a binary that has no such thing.

Usage:
    python facts_c.py <file.c> [function-name]
"""
import os
import re
import sys

HDR = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                   "..", "..", "src", "decomp", "gamecode", "logic", "mk3logic.h")

# --- the offset map, out of the header ---------------------------------
RE_STRUCT = re.compile(r"typedef struct (MK3[A-Z0-9]*)\s*\{")
RE_END = re.compile(r"^\}\s*(MK3[A-Z0-9]*);")
# a member line, with or without a trailing /* 0xNN comment
RE_MEMBER = re.compile(r"^\s+[A-Za-z_][A-Za-z_0-9 ]*?\s+\**"
                       r"([a-z_][a-z_0-9]*)\s*(?:\[[^\]]*\])?\s*;"
                       r"(?:\s*/\*\s*(0x[0-9a-f]+))?")


def offsets():
    """{struct name: {member: offset}} out of mk3logic.h."""
    out, cur = {}, None
    try:
        fh = open(HDR, encoding="utf-8", errors="replace")
    except OSError:
        return out
    with fh:
        for line in fh:
            m = RE_STRUCT.search(line)
            if m:
                cur = m.group(1)
                out.setdefault(cur, {})
                continue
            m = RE_END.match(line)
            if m:
                cur = None
                continue
            if cur is None:
                continue
            m = RE_MEMBER.match(line)
            if not m:
                continue
            name, off = m.group(1), m.group(2)
            if name.startswith("_pad"):
                continue
            if off is not None:
                out[cur][name] = int(off, 16)
            elif re.fullmatch(r"field[0-9a-f]{2}", name):
                # the name IS the offset, which is the point of the convention
                out[cur][name] = int(name[5:], 16)
    return out


# --- the statements we recognise ---------------------------------------
RE_FUNCDEF = re.compile(r"^(?:long|void|int32_t|uint32_t)\s+\**"
                        r"([A-Za-z_][A-Za-z_0-9]*)\s*\([^;]*\)\s*$")
# obj->field1c = 3;   /  obj->field00->field58 = 3;   /  thread->fieldfc = 1;
RE_STORE = re.compile(r"^\s*([a-z_][a-z_0-9]*)"
                      r"((?:->[a-z_][a-z_0-9]*)+)\s*=\s*([^;]+);")
# The same left-hand side, with no `;` yet on this line -- the right-hand
# side (or all of it) is still to come. Used only to decide whether to join
# forward; RE_STORE itself runs on the joined text.
RE_STORE_OPEN = re.compile(r"^\s*([a-z_][a-z_0-9]*)"
                           r"((?:->[a-z_][a-z_0-9]*)+)\s*=\s*[^;]*$")
# `(cond) ? A : B` at the top level of a store's value -- the two literal
# stores an if/else compiles to, written as one C expression. Only A and B
# matter here; `cond` is never evaluated, same as everywhere else in this
# file.
RE_TERNARY = re.compile(r"^\(.*\)\s*\?\s*([^:?]+?)\s*:\s*([^:?]+?)$")
# A call taking the object, WHEREVER it appears -- `group_sound(obj);` on
# its own, and `if (am_i_facing_him(obj))` inside a condition. Matching
# only bare statements missed every call the file makes inside an `if`,
# which came out as dozens of phantom differences.
RE_CALL = re.compile(r"\b([A-Za-z_][A-Za-z_0-9]*)\s*\(")
RE_CCOMMENT = re.compile(r"/\*.*?\*/")
RE_CSTRING = re.compile(r'"(?:\\.|[^"\\])*"')
# This binary has no hardware integer divide: every `/` on a signed value
# the compiler could not turn into a shift compiles to a runtime call
# (`___divsi3`, `___udivsi3` for unsigned, `___modsi3`/`___umodsi3` for `%`).
# The house convention (see mkprop.c) is to write the division plainly and
# mark it with a trailing comment naming the call, rather than spell out a
# call that does not exist in the source -- so that comment is what is
# matched here, not the `/` itself.
RE_DIVMOD_CALL = re.compile(r"/\*\s*_+((?:u?div|u?mod)si3)\s*\*/")
# mk3_install(thread, (MK3THREADFUNC)t_jhp5)  and the plyr_install spelling
# handler = (MK3THREADFUNC)t_d_block;
#
# A routine picked into a local and installed through it later --
# `mk3_install(thread, handler)`. The install line names no routine, so
# without this every name chosen this way was invisible, and a wrong one
# verified: t_mc_flipk_away installed t_motaro_slided where the binary loads
# t_d_block, and nothing said so.
RE_HANDLER_LOCAL = re.compile(r"^\s*\w+\s*=\s*\(MK3THREADFUNC\)\s*"
                              r"([A-Za-z_][A-Za-z_0-9]*)\s*;")
# mk3_install(thread, handler) -- the install itself, with its token clear.
RE_INSTALL_LOCAL = re.compile(r"mk3_(?:install|push_handler)\s*\(\s*thread\s*,"
                              r"\s*[A-Za-z_][A-Za-z_0-9]*\s*\)")
RE_INSTALL = re.compile(r"mk3_(?:install|push_handler)\s*\(\s*thread\s*,\s*"
                        r"\(MK3THREADFUNC\)\s*([A-Za-z_][A-Za-z_0-9]*)")
RE_PLYRINST = re.compile(r"plyr_install\s*\([^,]+,[^,]+,\s*"
                         r"\(const void \*\)\s*([A-Za-z_][A-Za-z_0-9]*)")
# mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_punch_sleep;
RE_HANDLER = re.compile(r"mk3_frame\([^)]*\)\[1\]\s*=\s*"
                        r"(?:\(uint32_t\)\s*)?(?:\(uintptr_t\)\s*)?"
                        r"([A-Za-z_][A-Za-z_0-9]*)\s*;")
RE_HANDLER2 = re.compile(r"mk3_frame\([^)]*\)\[1\]\s*=$")
RE_BARENAME = re.compile(r"^\s*\(uint32_t\)\(uintptr_t\)"
                         r"([A-Za-z_][A-Za-z_0-9]*)\s*;")
# *(uint32_t *)((char *)obj->field00 + 0x58) = 1;
#
# A field the header had no name for when the function was written. mkstat.c
# spells several this way and every one of them read as a store the binary
# makes and we do not, which is the most alarming shape a difference can take
# and was nothing.
# `*(uint32_t *)(uintptr_t)obj->field34 = 0x28;`, `*(uint32_t *)(void *)
# (G_BYTES + 0x388 + p * 4) = 0x28;` -- a word stored through a computed
# pointer. The offset is the first constant added to the base (0 when there is
# none); the binary folds any index into the base register the same way.
# Words only, because the machine reader only reports word stores.
RE_DEREFSTORE = re.compile(r"^\s*\*\((?:u?int32_t|long)\s*\*\)(?P<lhs>[^=;]*?)\s*"
                           r"(?P<op>[-+|&^]?=)(?!=)\s*(?P<val>[^;]+);")
RE_FIRSTADD = re.compile(r"\+\s*(0x[0-9a-fA-F]+|\d+)\b")
# `*left = v;` through a parameter declared as a word pointer. Offset 0 of
# whatever it points at; the value is whatever the C computed.
RE_PLAINDEREF = re.compile(r"^\s*\*(?P<p>[A-Za-z_]\w*)\s*=(?!=)\s*(?P<v>[^;]+);")
RE_WORDPTR_DECL = re.compile(r"\b(?:int|long|u?int32_t)\s*\*\s*([A-Za-z_]\w*)")
RE_ARGSTORE = re.compile(r"^\s*\*mk3_arg\(\s*thread\s*,[^)]*\)\s*=\s*([^;]+);")
RE_RAWSTORE = re.compile(r"^\s*\*\(uint32_t \*\)\s*\(\(char \*\)\s*"
                         r"([a-z_][a-z_0-9]*(?:->[a-z_][a-z_0-9]*)*)\s*\+\s*"
                         r"(0x[0-9a-fA-F]+)\)\s*=\s*([^;]+);")
# *mk3_frame(thread, thread->frame + 1) = 0x807;
RE_TOKEN = re.compile(r"^\s*\*mk3_frame\([^)]*\)\s*=\s*([^;]+);")
RE_RETURN = re.compile(r"^\s*return\s+([^;]+);")
RE_IF = re.compile(r"^\s*(?:\}\s*else\s+)?if\s*\((.+)\)")

HEXNUM = re.compile(r"^(?:0x[0-9a-fA-F]+|\d+)$")
# only literals, the four operators and brackets -- nothing that could
# name anything, so the eval below cannot reach out of itself
ARITH = re.compile(r"[\s0-9xa-fA-F+\-*()]+")

# `if (obj->field5c != 0)` matches the call pattern and is not a call. So do
# the frame accessors, which are how the readable C spells a field rather
# than anything the binary branches to.
NOT_A_CALL = frozenset((
    "if", "while", "for", "switch", "return", "sizeof",
    "mk3_frame", "mk3_arg", "mk3_install", "mk3_push_handler", "mk3_unwind",
    "plyr_install", "plyr_park1",
    # casts and macros that look like a call in C and are not a call in ARM
    "uint32_t", "uintptr_t", "int32_t", "int16_t", "uint16_t", "uint8_t",
    "long", "void", "const", "MK3THREADFUNC", "MK3OBJ", "MK3OBJPROC",
    "MK3THREAD", "G_BYTES",
    # accessor macros for the half-word fields: they compile to a ldrh/strh,
    # not to anything the binary branches to
    "MK3_FIELD12", "MK3_SET_FIELD12", "MK3_FIELD0E", "MK3_SET_FIELD0E",
    "MK3_FIELD0E_S", "MK3_FIELD12_S", "G_SWITCH_COUNTER",
    # a global holding a function pointer: an indirect call in the binary
    "mk3_getbbox_cb",
))


# `extern long *RoundParam;` / `extern int32_t table[];` -- a global whose
# elements are words, so `RoundParam[2] = 0` is a store at +0x8 of it.
RE_WORDARRAY = re.compile(r"^\s*extern\s+(?:const\s+)?(?:long|int32_t|uint32_t)\s+"
                          r"(?:\*\s*([A-Za-z_]\w*)\s*;|([A-Za-z_]\w*)\s*\[\s*\]\s*;)")
# `g[0x368 / 4] = v;` -- a word of a byte-offset-in-words view of something
RE_ARRDIV = re.compile(r"^\s*(\w+)\[\s*\(?\s*(0x[0-9a-fA-F]+|\d+)[^\]]*?/\s*4\s*\]\s*=(?!=)\s*([^;]+);")
RE_ARRSTORE = re.compile(r"^\s*([A-Za-z_]\w*)\[(\d+|0x[0-9a-fA-F]+)\]\s*=\s*([^;]+);")
WORD_ARRAYS = set()


RE_STATIC_HELPER = re.compile(
    r"^static\s+[\w\s\*]+?\b(\w+)\(([^)]*(?:\([^)]*\)[^)]*)*)\)\s*\n\{\n(.*?)\n\}\n",
    re.S | re.M)


def _split_top(text):
    parts, depth, cur = [], 0, ""
    for ch in text:
        if ch in "([":
            depth += 1
        elif ch in ")]":
            depth -= 1
        if ch == "," and depth == 0:
            parts.append(cur.strip())
            cur = ""
        else:
            cur += ch
    if cur.strip():
        parts.append(cur.strip())
    return parts


def _param_name(decl):
    m = re.search(r"\(\s*\*\s*(\w+)\s*\)", decl)
    if m:
        return m.group(1)
    m = re.search(r"(\w+)\s*(?:\[\s*\w*\s*\])?\s*$", decl)
    return m.group(1) if m else None


def inlined_text(path):
    """The source with `{ return helper(a, b); }` wrappers expanded to the
    static helper's body, parameters replaced by the arguments. A checker that
    reads one function at a time cannot see through the helper otherwise: the
    stores and handlers live in it, the binary has them in the wrapper."""
    text = open(path, encoding="utf-8", errors="replace").read()
    helpers = {}
    for m in RE_STATIC_HELPER.finditer(text):
        names = [_param_name(d) for d in _split_top(m.group(2))]
        if all(names) and m.group(2).strip() != "void":
            helpers[m.group(1)] = (names, m.group(3))
    if not helpers:
        return text

    def wrap(m):
        name = m.group(2)
        if name not in helpers:
            return m.group(0)
        names, body = helpers[name]
        args = _split_top(m.group(3))
        if len(args) != len(names):
            return m.group(0)
        for n, a in zip(names, args):
            a = re.sub(r"^\(\s*[A-Z][A-Z0-9_]*FUNC\w*\s*\)\s*", "", a)
            if not re.fullmatch(r"[\w.>-]+|\(\w+\)\s*\w+", a):
                a = "(" + a + ")"
            body = re.sub(r"\b%s\b" % re.escape(n), lambda _m, a=a: a, body)
        return m.group(1) + "\n{\n" + body + "\n}\n"

    return re.sub(r"(^[A-Za-z][^\n(]*\([^)\n]*\))\n\{\n\s*return\s+(\w+)\(([^;]*)\);\s*\n\}\n",
                  wrap, text, flags=re.M)


def split_functions(path):
    """{name: [line, ...]} for every function body in one .c file."""
    out, name, buf, depth, started = {}, None, [], 0, False
    prev = ""
    text = inlined_text(path)
    with open(path, encoding="utf-8", errors="replace") as fh:
        fh = iter(text.split("\n"))
        for line in text.split("\n"):
            wm = RE_WORDARRAY.match(line)
            if wm:
                WORD_ARRAYS.add(wm.group(1) or wm.group(2))
        for line in fh:
            line = line.rstrip("\n")
            if name is None:
                m = RE_FUNCDEF.match(prev) if line.strip() == "{" else None
                if m:
                    # the signature rides along as a comment: nothing reads
                    # comments for facts, and foreign_roots() needs the
                    # parameters' types
                    name, buf, depth, started = m.group(1), [], 0, False
                    buf.append("/* SIG " + prev.strip() + " */")
                prev = line
                if name is None:
                    continue
            depth += line.count("{") - line.count("}")
            if "{" in line:
                started = True
            buf.append(line)
            if started and depth <= 0:
                out[name] = buf
                name = None
            prev = line
    return out


def value(expr, consts=None):
    """A literal as itself, a name as itself, anything else as `?`."""
    e = expr.strip().rstrip(";").strip()
    # `0x14d9u` -- an unsigned suffix is still the same literal
    e = re.sub(r"^((?:0[xX][0-9a-fA-F]+|\d+))[uUlL]+$", r"\1", e)
    # strip the casts the house style sprinkles about
    for cast in ("(uint32_t)(uintptr_t)", "(uint32_t)", "(uintptr_t)",
                 "(long)", "(int32_t)", "(MK3THREADFUNC)"):
        while e.startswith(cast):
            e = e[len(cast):].strip()
    if HEXNUM.match(e):
        return hex(int(e, 0))
    m = re.fullmatch(r"\(uint32_t\)-(\d+)", e) or re.fullmatch(r"-(\d+)", e)
    if m:
        return hex(((-int(m.group(1))) & 0xffffffff))
    # `((0xa + 6) - 0xd) + 0x14` is 23. The house style writes these sums out
    # so the binary's shared-literal habit stays visible -- `adds r3, #6` on a
    # register that already holds 10 -- and collapsing them in the source
    # would lose the reading. So the reader does the sum.
    if ARITH.fullmatch(e):
        try:
            return hex(eval(e, {"__builtins__": {}}, {}) & 0xffffffff)
        except Exception:
            return "?"
    if re.fullmatch(r"[A-Za-z_][A-Za-z_0-9]*", e):
        # A bare name is a fact only when it names something the symbol table
        # knows. `t_jhp5` is a routine and comparable; `below`, `carried`,
        # `sel` and `flags` are locals this file invented to hold a value the
        # binary keeps in a register, and reporting one as a fact invents a
        # difference that is not there. They become `?`, which is what they
        # are: not checkable from here.
        return e if is_symbol(e) else "?"
    return "?"


_SYMNAMES = set()


def is_symbol(name):
    """Does the binary have a routine or datum by this name?"""
    if not _SYMNAMES:
        try:
            fh = open("E:/umk3repo/work/symbols.txt", encoding="utf-8",
                      errors="replace")
        except OSError:
            return False
        with fh:
            for line in fh:
                if "SECT" not in line:
                    continue
                parts = line.split()
                if parts:
                    _SYMNAMES.add(parts[-1].lstrip("_"))
    return name in _SYMNAMES


def struct_of(chain, maps):
    """Which struct a `->a->b` chain lands in, by walking the header.

    Only the shapes the file actually uses are followed: an object, its
    header at field00, and the thread. Anything else reports None and its
    stores come out with an unknown base rather than a guessed one.
    """
    parts = [p for p in chain.split("->") if p]
    return parts


# `next = 0x39a;` -- a literal assigned to a local that is later stored as a
# token (`*mk3_frame(...) = next;`). A state machine that picks its next state
# into a local wrote every token this way, and each one read as `?`.
RE_ASSIGN_LIT = re.compile(r"^\s*(?:\}\s*else\s*)?(?:\{\s*)?([A-Za-z_]\w*)\s*=\s*"
                           r"(0x[0-9a-fA-F]+|\d+)\s*;")
RE_IDENT = re.compile(r"^[A-Za-z_]\w*$")


# `MK3OBJPROC *proc = obj->field00;` -- a local naming a struct the function
# writes through. Resolved back to `thread->proc` so a store can be placed.
RE_ALIAS = re.compile(r"\b(?:struct\s+)?(MK3OBJ|MK3OBJPROC|MK3THREAD)\s*\*\s*"
                      r"([A-Za-z_]\w*)\s*=\s*(?:\([^()]*\)\s*)*"
                      r"([A-Za-z_]\w*(?:->[A-Za-z_]\w*)*)\s*;")


def aliases(lines):
    """{local: canonical path}; the object is `P`, the thread `thread`."""
    out = {"obj": "P", "thread": "thread"}
    for line in lines:
        m = RE_ALIAS.search(line)
        if m:
            out[m.group(2)] = canon(m.group(3), out)
    return out


def canon(expr, al):
    parts = expr.split("->")
    head = al.get(parts[0], "?" + parts[0])
    path = "->".join([head] + parts[1:])
    return "P" if path == "thread->proc" else path


def struct_tag(path):
    """Which struct a base path is, when that is certain; `?` otherwise."""
    return {"P": "MK3OBJ", "P->field00": "MK3OBJPROC",
            "P->field08": "MK3OBJ.field08",
            "thread": "MK3THREAD"}.get(path, "?")


RE_DECL = re.compile(r"\b(?:struct\s+)?([A-Z][A-Z0-9_]*)\s*\*\s*([A-Za-z_]\w*)")
MK3_TYPES = {"MK3OBJ", "MK3OBJPROC", "MK3THREAD"}


def foreign_roots(lines):
    """Pointers declared as some struct other than the three MK3 ones.

    `MK3PLAYBACK *p` -- a store through `p->field08` was placed with MK3OBJ's
    offsets, a guess at the wrong struct entirely, and compared.
    """
    out = set()
    for line in lines:
        for m in RE_DECL.finditer(line):
            if m.group(1) not in MK3_TYPES:
                out.add(m.group(2))
    return out


def calls_in(line, fnlocals=()):
    """The call facts on one line -- for the branches that `continue` before
    the general call scan, which silently dropped every call on a line that
    was also a store: `*(uint32_t *)(G_BYTES + 0x474) = (random32() >> 9) & 1u;`
    lost its random32."""
    code = RE_CSTRING.sub('""', RE_CCOMMENT.sub(" ", line)).split("//", 1)[0]
    return [("call", m.group(1)) for m in RE_CALL.finditer(code)
            if m.group(1) not in NOT_A_CALL and m.group(1) not in fnlocals]


def token_locals(lines):
    """The locals a function stores into the frame's token word."""
    names = set()
    for line in lines:
        m = RE_TOKEN.match(line)
        if m and RE_IDENT.match(m.group(1).strip()):
            names.add(m.group(1).strip())
    return names


# `*(uint32_t *)((char *)obj->field00 + 0x34) =` with the value below it
RE_DEREF_OPEN = re.compile(r"^\s*\*(?:\((?:u?int(?:8|16|32)_t|long)\s*\*\)|mk3_frame\().*=\s*$")
RE_INSTALL_OPEN = re.compile(r"mk3_(?:install|push_handler)\s*\(\s*thread\s*,\s*$")


def join_installs(lines):
    """`return mk3_install(thread,` with the routine on the next line.

    The reader works a line at a time, so a call split after its first
    argument named no routine at all -- t_lao_angle_hit's install of
    t_lao_angle_blocked read as missing. Rejoin those before reading.
    """
    out, i = [], 0
    while i < len(lines):
        line = lines[i]
        if ((RE_INSTALL_OPEN.search(line.rstrip()) or RE_DEREF_OPEN.match(line))
                and i + 1 < len(lines)):
            out.append(line.rstrip() + " " + lines[i + 1].strip())
            i += 2
            continue
        # a word store through a cast pointer whose value runs on over
        # several lines: join until the `;`
        if (re.match(r"^\s*\*\((?:u?int32_t|long)\s*\*\)", line)
                and "=" in line
                and not RE_CCOMMENT.sub("", line).split("//")[0].rstrip().endswith(";")):
            joined, j = line.rstrip(), i + 1
            while j < len(lines) and not joined.endswith(";") and j < i + 8:
                joined += " " + lines[j].strip()
                j += 1
            if joined.endswith(";"):
                out.append(joined)
                i = j
                continue
        out.append(line)
        i += 1
    return out


def facts_of(lines, maps):
    """One function's facts, in source order."""
    lines = join_installs(lines)
    out = []
    tlocals = token_locals(lines)
    al = aliases(lines)
    foreign = foreign_roots(lines)
    wordptrs = {m.group(1) for ln in lines for m in RE_WORDPTR_DECL.finditer(ln)}
    # locals that hold a function: `MK3THREADLONGFUNC f = ...; r = f(t);` is
    # an indirect call, which the machine reader does not list by name
    fnlocals = {m.group(1) for ln in lines
                for m in re.finditer(r"\b[A-Z][A-Z0-9_]*(?:FUNC|XFER)\w*\s+([A-Za-z_]\w*)\s*=", ln)}
    # `void (*fn)(X *);` -- a local function pointer: an indirect call
    fnlocals |= {m.group(1) for ln in lines
                 for m in re.finditer(r"\(\s*\*\s*([A-Za-z_]\w*)\s*\)\s*\(", ln)}
    for i, line in enumerate(lines):
        s = line.strip()
        m = RE_ASSIGN_LIT.match(line)
        if m and m.group(1) in tlocals:
            out.append(("token", value(m.group(2))))
            continue
        # A token store also starts with `*`, so it is recognised BEFORE the
        # comment filter rather than after it. That ordering was a real bug:
        # every state token in the file was silently dropped.
        m = RE_TOKEN.match(line)
        if m:
            tern = RE_TERNARY.match(m.group(1).strip())
            if tern:
                # `(token == 0) ? 0x1f5 : 0x1ff` -- two token stores
                out.append(("token", value(tern.group(1))))
                out.append(("token", value(tern.group(2))))
            else:
                out.append(("token", value(m.group(1))))
            continue
        # A raw-offset store starts with `*` too, so it joins the token
        # recogniser ABOVE the comment filter. Both were being eaten.
        # `*mk3_arg(thread, cur) = saved;` -- a push onto the thread's
        # argument stack, which the binary writes as `str rX, [rY, #0xa8]`
        # with the cursor folded into rY. Starts with `*`, so it is caught
        # here, above the comment filter, like the token store.
        m = RE_PLAINDEREF.match(line)
        if m and m.group("p") in wordptrs:
            out.append(("store", "0x0", value(m.group("v")), "?"))
            out.extend(calls_in(line, fnlocals))
            continue
        m = RE_ARRDIV.match(line)
        if m and m.group(1) in wordptrs:
            out.append(("store", hex(int(m.group(2), 0)), value(m.group(3)), "?"))
            out.extend(calls_in(line, fnlocals))
            continue
        m = RE_ARRSTORE.match(line)
        if m and m.group(1) in WORD_ARRAYS:
            out.append(("store", hex(4 * int(m.group(2), 0)), value(m.group(3)), "?"))
            out.extend(calls_in(line, fnlocals))
            continue
        m = RE_ARGSTORE.match(line)
        if m:
            out.append(("store", "0xa8", value(m.group(1)), "MK3THREAD"))
            out.extend(calls_in(line, fnlocals))
            continue
        m = RE_RAWSTORE.match(line)
        if not m:
            dm = RE_DEREFSTORE.match(line)
            if dm and not line.lstrip().startswith("*mk3_"):
                fa = RE_FIRSTADD.search(dm.group("lhs"))
                off = int(fa.group(1), 0) if fa else 0
                val = value(dm.group("val")) if dm.group("op") == "=" else "?"
                out.append(("store", hex(off), val, "?"))
                out.extend(calls_in(line, fnlocals))
                continue
        if m:
            base, off, val = m.group(1), m.group(2), m.group(3)
            # resolved through the aliases like any other store: `proc` here
            # is usually `obj->field00`, which the text alone does not say
            if base.split("->")[0] in foreign:
                continue
            st = struct_tag(canon(base, al))
            out.append(("store", hex(int(off, 16)), value(val), st))
            continue
        if not s or s.startswith("*") or s.startswith("/*"):
            continue

        m = RE_HANDLER_LOCAL.match(line)
        if m:
            out.append(("handler", m.group(1)))
            continue
        if RE_INSTALL_LOCAL.search(line) and not RE_INSTALL.search(line):
            out.append(("token", "0x0"))
            continue

        m = RE_INSTALL.search(line) or RE_PLYRINST.search(line)
        if m:
            out.append(("handler", m.group(1)))
            # mk3_install / mk3_push_handler both end by zeroing the NEXT
            # frame's token (see mk3logic.h) -- a real store the binary
            # makes that this call hides inside itself. Emitting it here
            # keeps a function that only ever calls the helper (never
            # spelling the token store out by hand) from reading as
            # "the binary has a token 0x0 the C does not".
            out.append(("token", "0x0"))
            continue

        m = RE_HANDLER.search(line)
        if m:
            # `mk3_frame(...)[1] = below;` is the stack surgery putting a
            # SAVED handler back. The name is a local, the value is whatever
            # was there, and neither side can compare it.
            nm = m.group(1)
            out.append(("handler", nm if is_symbol(nm) else "?"))
            continue
        if RE_HANDLER2.search(line.rstrip()):
            nxt = lines[i + 1] if i + 1 < len(lines) else ""
            m = RE_BARENAME.match(nxt)
            if m:
                out.append(("handler", m.group(1)))
            continue

        # Calls are scanned FIRST and without consuming the line, because a
        # call usually lives inside something else: `if (is_he_airborn(obj))`
        # is a branch AND a call, and handling the branch first swallowed
        # every one of them.
        # ...but not inside a comment or a string: `/* max(v, 0) */` beside a
        # bic, and seq_lookup's own name in its printf format, both read as
        # calls the binary does not make. (The divide helpers below are found
        # BY their comments, so they still scan the raw line.)
        code = RE_CSTRING.sub('""', RE_CCOMMENT.sub(" ", line))
        code = code.split("//", 1)[0]
        for cm in RE_CALL.finditer(code):
            nm = cm.group(1)
            if nm not in NOT_A_CALL and nm not in fnlocals:
                out.append(("call", nm))

        for dm in RE_DIVMOD_CALL.finditer(line):
            out.append(("call", dm.group(1)))

        m = RE_STORE.match(line)
        if not m and RE_STORE_OPEN.match(line):
            # The right-hand side runs on to the next line(s) -- join
            # forward until a `;` shows up, rather than lose the store
            # entirely the way it did before this joined.
            joined = line
            j = i + 1
            while ";" not in joined and j < len(lines):
                joined += " " + lines[j].strip()
                j += 1
            m = RE_STORE.match(joined)
        if m and m.group(1) in foreign:
            continue                    # a struct we cannot place
        if m:
            root, chain, val = m.group(1), m.group(2), m.group(3)
            members = [p for p in chain.split("->") if p]
            last = members[-1]
            # Which struct the LAST hop lands in: resolved through the
            # function's own aliases when it can be, guessed from the chain's
            # length when it cannot -- and then tagged `?`, so the base check
            # leaves it alone. The guess alone once called `proc->field64`
            # (proc = obj->field00) an object store, and `obj->field08->x`
            # (another object) a header store.
            where = canon("->".join([root] + members[:-1]), al)
            tag = struct_tag(where)
            if tag != "?":
                st = tag
            elif root == "thread" and len(members) == 1:
                st = "MK3THREAD"
            elif len(members) == 1:
                st = "MK3OBJ"
            else:
                st = "MK3OBJPROC"
            off = maps.get(st, {}).get(last)
            if off is None:
                # try the other two rather than drop the fact entirely
                for alt in ("MK3OBJ", "MK3OBJPROC", "MK3THREAD"):
                    if last in maps.get(alt, {}):
                        st, off = alt, maps[alt][last]
                        break
            if off is not None:
                if RE_TERNARY.match(val.strip()):
                    # `(cond) ? A : B` -- an if/else compiles to two literal
                    # stores (confirmed by hand for this exact shape in
                    # proj_onscreen_test/_unsafe, mkzap.c) or to one
                    # conditional-move the disassembly reads as a single
                    # unresolved store (is_he_motaro, same file, same C
                    # shape) -- and nothing in the C alone says which. Two
                    # `?` gives the binary side slack for either one real
                    # store or two, rather than guessing.
                    out.append(("store", hex(off), "?", tag))
                    out.append(("store", hex(off), "?", tag))
                else:
                    out.append(("store", hex(off), value(val), tag))
            continue

        m = RE_RETURN.match(line)
        if m:
            out.append(("ret", value(m.group(1))))
            continue

        m = RE_IF.match(line)
        if m:
            out.append(("branch", m.group(1).strip()))
            continue

    return out


def main(argv):
    maps = offsets()
    fns = split_functions(argv[1])
    want = argv[2] if len(argv) > 2 else None
    for name in sorted(fns):
        if want and name != want:
            continue
        print("=== %s" % name)
        for f in facts_of(fns[name], maps):
            if f[0] == "store":
                print("    %-8s %-6s = %-22s in %s" % (f[0], f[1], f[2], f[3]))
            else:
                print("    %-8s %s" % (f[0], f[1]))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
