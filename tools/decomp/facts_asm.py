#!/usr/bin/env python3
"""facts_asm.py -- pull the checkable facts out of a recompiled function.

`TOOLS/armrecomp/recomp.py` turns a function into C one instruction per
statement, with the original instruction above each line as a comment. That
output is unreadable on purpose -- it is a transcription, not a decompilation
-- and that is exactly what makes it trustworthy: it reports what it cannot
translate instead of emitting something plausible.

This reads that transcription and reports the handful of things a HUMAN
transcription gets wrong:

    store   an offset and the value written there
    call    a routine, in the order it is reached
    const   a literal the body actually uses
    branch  a condition and where it goes
    ret     whether r0 is live at the return

Nothing else. It is deliberately not trying to understand the function; the
readable decomp is where understanding lives. This only answers "do the
numbers, the names and the arrows match".

## How it resolves a value

A straight-line pass that remembers which registers hold a known constant.
`ctx->r[10] = 0x3u` makes r10 known; `MEM_ST32((ctx->r[3] + 88), ctx->r[10])`
then reports `store +0x58 = 3`. Anything it cannot follow -- a value out of
memory, a register written by a call -- becomes `?` rather than a guess, the
same discipline `pushfn.py` and `leaffn.py` keep. A `?` is a fact too: it says
this one still needs a human.

Every label resets the table, because a register that was known at the top of
a block need not be known on the path that jumps into it.

## What it does not read yet

  * a constant that comes from a BRANCH rather than a load. After `cbz r6`
    the register is zero on the taken path, and this reader does not know it.
    Several `?` are only this.
  * a handler fetched through a pointer table: `ldr r3,[pc]; add r3,pc;
    ldr r2,[r3]` is two hops and this follows one. Those come back `?`, which
    is correct, but it means the arrow is unchecked.
  * anything past a label. The table is cleared at every join point because a
    register known at the top of a block need not be known on the path that
    jumps in. Correct, and it costs coverage; a real dataflow pass would
    recover it.

## The mistake this file already made once

The first version tracked only immediate loads, so `adds r3, #2` left a stale
constant behind and it reported `field48 = 1` where the answer is 3 -- a
WRONG FACT STATED WITH CONFIDENCE, which is the exact thing the tool exists
to catch. The rule that came out of it, and that any change here must keep:

    any write to a register this reader does not recognise CLEARS what it
    thought it knew about that register

`?` is a fact. A wrong number is not.

Usage:
    python facts_asm.py <recompiled.c> [function-name]
"""
import re
import sys

# ctx->r[N] = 0x1234u;
RE_SETC = re.compile(r"^\s*ctx->r\[(\d+)\] = (0x[0-9a-fA-F]+)u;")
# ctx->r[N] = ctx->r[M];
RE_MOV = re.compile(r"^\s*ctx->r\[(\d+)\] = ctx->r\[(\d+)\];")
# ctx->r[N] = MEM_LD32((ctx->r[M] + OFF));
RE_LD = re.compile(r"^\s*ctx->r\[(\d+)\] = MEM_LD32\(\(ctx->r\[(\d+)\]"
                   r"(?: \+ (-?\d+))?\)\);")
# MEM_ST32((ctx->r[N] + OFF), ctx->r[M]);
RE_ST = re.compile(r"^\s*MEM_ST32\(\(ctx->r\[(\d+)\](?: \+ (-?\d+))?\),"
                   r"\s*ctx->r\[(\d+)\]\);")
# MEM_ST32((ctx->r[5] + (ctx->r[3] << 3)), ctx->r[8]);
#
# The STATE TOKEN. `mk3_frame(thread, N)` is `thread + N * 8`, so a store
# indexed by a register shifted left three is a write into the frame array at
# a computed level -- which in this codebase is always the token slot. The
# value is what matters and the index is not something this tool resolves, so
# it is reported as a token rather than as a store at a known offset.
RE_STIDX = re.compile(r"^\s*MEM_ST32\(\(ctx->r\[(\d+)\] \+ "
                      r"\(ctx->r\[(\d+)\] << 3\)\), ctx->r\[(\d+)\]\);")
# func_0003f00d_name(ctx);
RE_CALL = re.compile(r"^\s*func_[0-9a-fA-F]+_([A-Za-z_][A-Za-z_0-9]*)\(ctx\);")
# An imported routine with no hand-written shim: recomp.py emits
# "stub_auto_<name>(ctx);" (see armrecomp/recomp.py's STUBS handling). The
# decompiled C calls it by its real name, so this is captured separately
# and the prefix is stripped before comparison.
RE_CALL_STUB = re.compile(r"^\s*stub_(?:auto_)?([A-Za-z_][A-Za-z_0-9]*)\(ctx\);")  # also hand-written shims, e.g. stub_printf
# if (ctx->zf) goto L_00030e0c;
RE_BR = re.compile(r"^\s*if \((.+?)\) goto (L_[0-9a-fA-F]+);")
RE_GOTO = re.compile(r"^\s*goto (L_[0-9a-fA-F]+);")
RE_LABEL = re.compile(r"^(L_[0-9a-fA-F]+):")
# ctx->r[N] = ~(0x2u);   -- mvn, the -3 return in this codebase
RE_MVN = re.compile(r"^\s*ctx->r\[(\d+)\] = ~\((0x[0-9a-fA-F]+)u\);")
# ctx->r[N] = ctx->r[M] + 0x2u;   -- add with an immediate
RE_ADDI = re.compile(r"^\s*ctx->r\[(\d+)\] = ctx->r\[(\d+)\] \+ "
                     r"(0x[0-9a-fA-F]+)u;")
# { uint32_t _a = ctx->r[3], _b = 0x2u, _r = _a + _b;   -- adds/subs, flag form
RE_FLAGOP = re.compile(r"^\s*\{ uint32_t _a = ctx->r\[(\d+)\], "
                       r"_b = (0x[0-9a-fA-F]+)u, _r = _a ([-+]) _b;")
# ctx->r[N] = _r;   -- the second half of the flag form
RE_TAKE_R = re.compile(r"^\s*ctx->r\[(\d+)\] = _r;")
# ANY other write to a register. Whatever it is, we stop claiming to know
# that register -- a stale constant reported as certain is the exact mistake
# this tool exists to catch, and it would be humiliating to make it here.
RE_ANYWRITE = re.compile(r"ctx->r\[(\d+)\]\s*=")
# the header line recomp.py writes for each function
RE_FUNC = re.compile(r"^void func_([0-9a-fA-F]+)_([A-Za-z_][A-Za-z_0-9]*)"
                     r"\(arm_ctx \*ctx\)")
RE_ORIG = re.compile(r"^\s*/\* ([0-9a-fA-F]{8})\s+(.*?) \*/\s*$")


def split_functions(text):
    """The recompiled file, as {name: [line, ...]}."""
    out, name, buf, depth, started = {}, None, [], 0, False
    for line in text.splitlines():
        m = RE_FUNC.match(line)
        if m and name is None:
            name = m.group(2)
            buf, depth, started = [], 0, False
            continue
        if name is None:
            continue
        depth += line.count("{") - line.count("}")
        if "{" in line:
            started = True
        buf.append(line)
        if started and depth <= 0:
            out[name] = buf
            name = None
    return out


def signed(v):
    return v - (1 << 32) if v >= (1 << 31) else v


# --- addresses back to names -------------------------------------------
#
# A handler stored into the frame is the ARROW of a state machine, and an
# arrow is the thing this project got backwards in t_jmp4 and t_jmp5. The
# value lands as a Thumb address -- the low bit set -- so both spellings go
# in the table and the lookup tries the number as written and with bit 0
# cleared.
_SYMS = {}


def load_symbols(path="E:/umk3repo/work/symbols.txt"):
    if _SYMS:
        return _SYMS
    try:
        fh = open(path, encoding="utf-8", errors="replace")
    except OSError:
        return _SYMS
    with fh:
        for line in fh:
            parts = line.split()
            if len(parts) < 2 or not parts[0].startswith("0x"):
                continue
            if "SECT" not in line:
                continue
            name = parts[-1]
            try:
                addr = int(parts[0], 16)
            except ValueError:
                continue
            _SYMS.setdefault(addr, name.lstrip("_"))
    return _SYMS


def name_of(val):
    """A constant as a symbol name when it is one, else None."""
    syms = load_symbols()
    if not syms:
        return None
    if isinstance(val, str):
        try:
            n = int(val, 16)
        except ValueError:
            # already a name -- resolving twice is harmless and happens
            # because the caller may be looking at a value this module has
            # itself already turned into a symbol.
            return val if val in syms.values() else None
    else:
        n = val
    return syms.get(n) or syms.get(n & ~1)


RE_PRED = re.compile(r"^\s*if \(.*\) \{ (.*?;)\s*(?:/\*.*?\*/)?\s*\}\s*$")
RE_REGWRITE = re.compile(r"ctx->r\[(\d+)\]\s*=[^=]")


def single_defs(lines):
    """Callee-saved registers (r4-r11) written exactly once in the body.

    The prologue loads `thread->proc` into one of these and nothing writes it
    again, so where it came from is still true after every branch. Every
    other register's origin is forgotten at a label, as before.
    """
    count = {}
    for line in lines:
        for g in RE_REGWRITE.finditer(line):
            r = int(g.group(1))
            count[r] = count.get(r, 0) + 1
    return {r for r, n in count.items() if 4 <= r <= 11 and n == 1}


def facts(lines):
    """One function's facts, in the order the instructions reach them."""
    known = {}          # register -> constant
    origin = {}         # register -> a short note on where it came from
    out = []
    keep = single_defs(lines)
    entry = set()       # registers defined before the first branch or label
    in_entry = True
    addr = None
    pend_reg = None     # the flag-setting add/sub form spans two lines
    pend_val = None

    cond_regs = set()   # registers last written under an IT predicate
    for line in lines:
        m = RE_ORIG.match(line)
        if m:
            addr = m.group(1)
            continue

        # A predicated instruction -- `itt le; ldrle r3, [pc, #x]; strle r3,
        # [r5, #0x40]` -- comes out as `if (cond) { stmt; }`, and every
        # pattern below is anchored to a bare statement, so the whole IT block
        # used to vanish: t_r_duck_punch's conditional field40 = 0x30007 read
        # as a store the binary never makes. Read the statement inside. A
        # register it writes is trusted only until the next unconditional
        # line, so a value that may not have been loaded never reaches a fact
        # outside its own block.
        pm = RE_PRED.match(line)
        if pm:
            line = pm.group(1)
            for g in RE_REGWRITE.finditer(line):
                cond_regs.add(int(g.group(1)))
        elif cond_regs and not RE_LABEL.match(line):
            for r in cond_regs:
                known.pop(r, None)
                origin.pop(r, None)
            cond_regs = set()

        if RE_LABEL.match(line):
            # A join point: nothing survives it that we can prove -- except
            # where a register came from, when that is settled: written once
            # in the whole body, or defined in the entry block (which reaches
            # every label) and not written since.
            in_entry = False
            known.clear()
            for r in list(origin):
                if r not in keep and r not in entry:
                    del origin[r]
            continue

        if in_entry and (RE_BR.match(line) or RE_GOTO.match(line)):
            in_entry = False
        for g in RE_REGWRITE.finditer(line):
            r = int(g.group(1))
            if in_entry:
                entry.add(r)
            else:
                entry.discard(r)

        m = RE_SETC.match(line)
        if m:
            known[int(m.group(1))] = int(m.group(2), 16)
            origin[int(m.group(1))] = "imm"
            continue

        m = RE_MVN.match(line)
        if m:
            known[int(m.group(1))] = (~int(m.group(2), 16)) & 0xffffffff
            origin[int(m.group(1))] = "mvn"
            continue

        m = RE_ADDI.match(line)
        if m:
            d, b = int(m.group(1)), int(m.group(2))
            if b in known:
                known[d] = (known[b] + int(m.group(3), 16)) & 0xffffffff
                origin[d] = "imm"
            else:
                known.pop(d, None)
                origin.pop(d, None)
            continue

        m = RE_FLAGOP.match(line)
        if m:
            b = int(m.group(1))
            pend_reg = b
            pend_val = None
            if b in known:
                k = int(m.group(2), 16)
                pend_val = ((known[b] + k) if m.group(3) == "+"
                            else (known[b] - k)) & 0xffffffff
            continue

        m = RE_TAKE_R.match(line)
        if m:
            d = int(m.group(1))
            if pend_val is not None:
                known[d] = pend_val
                origin[d] = "imm"
            else:
                known.pop(d, None)
                origin.pop(d, None)
            pend_val, pend_reg = None, None
            continue

        m = RE_MOV.match(line)
        if m:
            d, s = int(m.group(1)), int(m.group(2))
            if s in known:
                known[d] = known[s]
                origin[d] = origin.get(s, "mov")
            else:
                known.pop(d, None)
                origin[d] = origin.get(s, "r%d" % s)
            continue

        m = RE_LD.match(line)
        if m:
            d, b = int(m.group(1)), int(m.group(2))
            off = int(m.group(3) or 0)
            known.pop(d, None)
            origin[d] = "[%s+0x%x]" % (origin.get(b, "r%d" % b), off)
            continue

        m = RE_ST.match(line)
        if m:
            b, v = int(m.group(1)), int(m.group(3))
            off = int(m.group(2) or 0)
            val = ("0x%x" % known[v]) if v in known else "?"
            # A value that IS a routine is reported by its name, wherever it
            # is stored. `obj->field34 = t_angle_jump_call` lands at 0x34, not
            # at the frame's 0x4, and comparing 0x30789 against the name it
            # spells would report a difference that is not one.
            if val != "?":
                nm = name_of(val)
                if nm:
                    val = nm
            out.append(("store", "0x%x" % off, val,
                        origin.get(b, "r%d" % b), addr))
            continue

        m = RE_STIDX.match(line)
        if m:
            v = int(m.group(3))
            out.append(("token", ("0x%x" % known[v]) if v in known else "?",
                        addr))
            continue

        m = RE_CALL.match(line) or RE_CALL_STUB.match(line)
        if m:
            out.append(("call", m.group(1), addr))
            # A call clobbers the scratch registers; forget them rather than
            # carry a stale constant past it.
            for r in (0, 1, 2, 3, 12):
                known.pop(r, None)
                origin.pop(r, None)
            continue

        m = RE_BR.match(line)
        if m:
            out.append(("branch", m.group(1).strip(), m.group(2), addr))
            continue

        m = RE_GOTO.match(line)
        if m:
            out.append(("goto", m.group(1), addr))
            continue

        if "return" in line and ";" in line:
            out.append(("ret", ("0x%x" % known[0]) if 0 in known else "?",
                        addr))
            continue

        # Nothing above recognised this line. If it writes a register at all,
        # forget that register: reporting a stale value as a fact is worse
        # than reporting nothing.
        for g in RE_ANYWRITE.finditer(line):
            r = int(g.group(1))
            known.pop(r, None)
            origin.pop(r, None)

    return out


def consts(fs):
    """The literals a body actually writes or returns, as a sorted list."""
    seen = []
    for f in fs:
        if f[0] == "store" and f[2] != "?":
            seen.append(f[2])
        if f[0] == "ret" and f[1] != "?":
            seen.append(f[1])
    return sorted(seen)


def main(argv):
    text = open(argv[1], encoding="utf-8", errors="replace").read()
    fns = split_functions(text)
    want = argv[2] if len(argv) > 2 else None
    for name in sorted(fns):
        if want and name != want:
            continue
        fs = facts(fns[name])
        print("=== %s" % name)
        for f in fs:
            if f[0] == "store":
                tag = ""
                if f[2] != "?":
                    nm = name_of(f[2])
                    if nm:
                        tag = "  -> %s" % nm
                print("    %-8s %-6s = %-12s base %-22s @%s%s"
                      % (f[0], f[1], f[2], f[3], f[4], tag))
            elif f[0] == "call":
                print("    %-8s %s  @%s" % (f[0], f[1], f[2]))
            elif f[0] == "branch":
                print("    %-8s %-22s -> %s  @%s" % (f[0], f[1], f[2], f[3]))
            elif f[0] == "token":
                print("    %-8s %s  @%s" % (f[0], f[1], f[2]))
            else:
                print("    %-8s %s  @%s" % (f[0], f[1], f[2]))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
