#!/usr/bin/env python3
"""factdiff.py -- put the two fact lists side by side and name what differs.

`facts_asm.py` reads the machine transcription of a function; `facts_c.py`
reads ours. Neither knows about the other. This compares them and reports, per
function, the things a transcription gets wrong.

## Multisets, not sequences

The two are compared as MULTISETS rather than in order, on purpose. The
readable C is allowed to say the same thing in a different order -- an
`if/else` where the binary has a branch and a fall-through, a `goto pop` where
the binary repeats the tail four times -- and comparing sequences would report
every one of those as a difference. What cannot legitimately differ is WHICH
stores happen, WITH WHAT values, to WHICH routines control passes, and what
comes back.

So four comparisons:

    stores    (offset, value) where both sides resolved the value
    handlers  the routines installed into the frame
    tokens    the state numbers written into the frame
    calls     which routines are called, and how many times

## `?` is not a disagreement

Either side reports `?` when it will not guess. A pair where one side is `?`
is counted as UNCHECKED and reported separately rather than folded into either
column, because calling it agreement would be the same overclaiming the tools
were built to avoid. The summary prints that count; a function with many of
them has been checked less than its verdict suggests.

## THE FIRST RUN OF A CHECKER MEASURES THE CHECKER

Read this before trusting a number out of here, and before improving it.

On `joy.c` the verdict went 17 -> 22 -> 42 -> 58 -> 67 of 73 across five
rounds, and not one line of decomp changed in between. Every jump was a false
positive in the readers:

    a local reported as a fact       `below`, `carried`, `sel` are names this
                                     project invented for a value the binary
                                     keeps in a register; they are `?` now
    one side`s unknown made a diff   the binary fetches some handlers through
                                     a pointer table and will not follow it;
                                     that is not evidence against ours
    calls only matched as statements every call inside an `if` was invisible
    `if` counted as a call           `if (obj->field5c != 0)` matched
    mk3_push_handler unrecognised    despite the name it installs, so every
                                     retraction proc read as installing nothing
    calls needing `obj` first        random32() and MKEvent_Add() invisible
    names needing a lowercase start  MKEvent_Add, ReallyKillProjectile too

So: a difference this tool reports is a QUESTION, not a verdict. Read the
disassembly before touching the decomp, every time. The failure that matters
here is not missing a defect -- it is reporting one that is not there and
sending somebody to "fix" correct code.

## Known gaps, in the order they are worth closing

1. **`plyrthread` and anything its size.** 95 of its facts come back `?` and
   the readers give up on its length. A function this big needs the diff done
   per BLOCK rather than per function; as it stands its verdict means little.
2. **Loops.** `for (i = 0; i < 5; i++) find_part2(obj);` is one call here and
   five in the binary. Two of joy.c`s six remaining differences are exactly
   this and neither is a defect. A count taken from the loop bound would fix
   both.
3. **Stores through a global.** `*(uint32_t *)(G_BYTES + 0x378 + p * 4) = 0`
   is not read at all on the C side, so the turbo bar and everything else
   reached through `G` or `H` goes unchecked.
4. **Values that are expressions.** `obj->field1c = obj->field48;` is `?`
   on both sides. Following one level of copy would recover a good many.
5. **Jump tables.** A `switch` the compiler emitted as `tbb` -- a byte table
   of branch offsets indexed by a register -- has every case branching into
   ONE shared store, so the asm side reports a single handler with an
   unresolved value and the readable C reports one per case. Every one of
   those cases then comes out as "the C has a handler the binary does not".
   `t_background_death` in mkreact.c is the first; a reading of the table
   itself would fix it, and until then such a function has to say in its
   banner that it was checked by hand.
6. **Flow facts.** After `cbz r6`, r6 is zero on that path -- the binary
   gets a constant from a branch rather than from a load, and the asm reader
   does not. Several `?` on the asm side are this one case.
7. **Calls to imported functions, and the string literals that quote them.**
   `facts_asm.py`'s `RE_CALL` only matches `func_XXXXXXXX_name(ctx);` --
   recomp.py's shape for a call to a function it found in the symbol table
   with an address. A call to an unresolved import comes out as
   `stub_printf(ctx);` instead (no address, no `func_` prefix) and this
   reader never sees it, so a real `printf(...)` in the readable C reports
   as "the C has a call the binary does not" even when the call is right.
   `facts_c.py`'s `RE_CALL` has the matching problem from the other side: it
   is a bare `name` + whitespace + `(` scan with no idea what is a string
   literal, so a
   format string that happens to spell the function's own name --
   `printf("seq_lookup( %d, %d, %d );\n", ...)` inside `seq_lookup` itself --
   reads as a call to `seq_lookup`. `playback.c`'s `seq_lookup` is the first
   function in this project to call an imported C library function, which is
   why neither gap had shown up before it.

## What it will never do

Prove the English in a banner. The Godot port`s claim that the engine
interpolates between consecutive frames was false, cost a day, and no
instruction-level check would have caught it. A sentence about what the
engine does needs an address beside it and the address needs reading.

Usage:
    python factdiff.py <recompiled.c> <readable.c> [function]
    python factdiff.py --summary <recompiled.c> <readable.c>
"""
import collections
import re
import sys
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import facts_asm                                             # noqa: E402
import facts_c                                               # noqa: E402


def asm_sets(fs):
    """The asm facts, bucketed for comparison.

    The last element counts what this side declined to resolve, PER BUCKET:
    {"store": n, "handler": n, "token": n}. A `?` stored into a frame's handler
    word excuses a handler and nothing else. It used to be one shared count,
    and that let an unresolved field store excuse a wrong handler -- which is
    how mkreact.c's t_b_weak_silent verified while installing the wrong routine.
    """
    stores, handlers, tokens, calls = [], [], [], []
    unk = {"store": 0, "handler": 0, "token": 0}
    for f in fs:
        if f[0] == "store" and len(f) > 3 and f[3] == "r13":
            continue        # an outgoing stack argument (`str r3, [sp]`), not a field
        if f[0] == "store":
            off, val = f[1], f[2]
            if val == "?":
                unk["handler" if off == "0x4" else "store"] += 1
                if off != "0x4":
                    unk.setdefault("store_offs", []).append(off)
                continue
            nm = facts_asm.name_of(val)
            # a handler is a store of a routine's address into the frame
            if nm and off == "0x4":
                handlers.append(nm)
            else:
                stores.append((off, val))
        elif f[0] == "token":
            if f[1] == "?":
                unk["token"] += 1
            else:
                tokens.append(f[1])
        elif f[0] == "call":
            calls.append(f[1])
    return stores, handlers, tokens, calls, unk


def c_sets(fs):
    """Ours, bucketed the same way, with the same per-bucket unknowns."""
    stores, handlers, tokens, calls, rets = [], [], [], [], []
    unk = {"store": 0, "handler": 0, "token": 0}
    for f in fs:
        if f[0] == "store":
            off, val = f[1], f[2]
            if val == "?":
                unk["store"] += 1
                unk.setdefault("store_offs", []).append(off)
                continue
            # A literal that happens to equal a symbol's address reads as that
            # symbol on the binary side (0x8000c is also `___tcf_0`, an EA SDK
            # routine) -- spell ours the same way so the two can meet.
            if val.startswith("0x"):
                val = facts_asm.name_of(val) or val
            stores.append((off, val))
        elif f[0] == "handler":
            if f[1] == "?":
                unk["handler"] += 1
            else:
                handlers.append(f[1])
        elif f[0] == "token":
            # a token read out of a variable (`GLBL_joy_entry`) is a value
            # neither side knows, not a name to match
            if f[1] == "?" or not f[1].startswith("0x"):
                unk["token"] += 1
            else:
                tokens.append(f[1])
        elif f[0] == "call":
            calls.append(f[1])
        elif f[0] == "ret" and f[1] != "?":
            rets.append(f[1])
    return stores, handlers, tokens, calls, unk, rets


RE_RCONST = re.compile(r"ctx->r\[(\d+)\] = (~?)\(?(0x[0-9a-f]+)u\)?;")
RE_RCOPY = re.compile(r"ctx->r\[0\] = ctx->r\[(\d+)\];")


RE_RMOV = re.compile(r"ctx->r\[(\d+)\] = ctx->r\[(\d+)\];")
RE_RARITH = re.compile(r"ctx->r\[(\d+)\] = ctx->r\[(\d+)\] ([+-]) (0x[0-9a-f]+)u;")
RE_RARITH_F = re.compile(r"_a = ctx->r\[(\d+)\], _b = (0x[0-9a-f]+)u, _r = _a ([+-]) _b;"
                         r"\s*ctx->r\[(\d+)\] = _r;")


def asm_ret_consts(lines, reg="0"):
    """Every constant the binary can hand back in r0.

    The recompiled function has one physical exit, so its `ret` fact is `?`.
    This is a flow-insensitive over-approximation instead: every constant any
    register can hold -- loaded, moved, or an immediate added to or taken from
    one -- and r0's share of that. Loose on purpose: it only has to catch a
    return value the binary can never produce, like the -2 that `mvn r0, #2`
    (which is -3) was once transcribed as. `t_combj` returns 0x7f3 - 0x7f0;
    `t_lkzap5` returns field5c + 0x10 where field5c is 0.
    """
    text = "\n".join(lines)
    held = collections.defaultdict(set)
    for m in RE_RCONST.finditer(text):
        v = int(m.group(3), 16)
        held[m.group(1)].add((~v & 0xffffffff) if m.group(2) else v)
    arith = [(m.group(1), m.group(2), m.group(3), int(m.group(4), 16))
             for m in RE_RARITH.finditer(text)]
    arith += [(m.group(4), m.group(1), m.group(3), int(m.group(2), 16))
              for m in RE_RARITH_F.finditer(text)]
    movs = [(m.group(1), m.group(2)) for m in RE_RMOV.finditer(text)]
    for _ in range(3):
        for d, src, op, k in arith:
            base = held[src] | {0}          # an unknown that a branch proved 0
            held[d] |= {((v + k) if op == "+" else (v - k)) & 0xffffffff
                        for v in base}
        for d, src in movs:
            held[d] |= held[src]
    if reg == "*":
        return set().union(*held.values()) if held else set()
    return set(held[reg])


def compare_stores(a, b, out, unk_bin, unk_c):
    """(offset, value) stores, where an unknown only excuses its own offset.

    A store the binary could not resolve at +0x30 used to excuse any C store
    anywhere -- t_b_punch wrote t_cc_block_avoid_corner into +0x38 where the
    binary writes t_cc_punch, and two unresolved zeros at +0x30/+0x34 paid for
    it. Now an unresolved store absorbs one disagreement at the same offset.
    """
    # Sets, not counts: a shared tail stores once in the binary and once per
    # path in the C (t_back_to_the_fight's `fieldfc = 1`), which is structure,
    # not behaviour. What must hold is that each side's (offset, value) pairs
    # exist on the other, or an unknown at that very offset excuses them.
    ca = collections.Counter(set(a))
    cb = collections.Counter(set(b))
    only_bin, only_c = ca - cb, cb - ca
    ub = collections.Counter({o: 10 ** 6 for o in unk_bin})
    uc = collections.Counter({o: 10 ** 6 for o in unk_c})
    bad = 0
    for (off, val), n in sorted(only_bin.items(), key=lambda x: str(x[0])):
        take = min(n, uc[off]); uc[off] -= take; n -= take
        if n > 0:
            out.append("    %-9s binario tiene %s  x%d  y el C no"
                       % ("store", (off, val), n))
            bad += 1
    for (off, val), n in sorted(only_c.items(), key=lambda x: str(x[0])):
        take = min(n, ub[off]); ub[off] -= take; n -= take
        if n > 0:
            out.append("    %-9s el C tiene   %s  x%d  y el binario no"
                       % ("store", (off, val), n))
            bad += 1
    return bad


def compare(a, b, label, out, slack_bin=0, slack_c=0):
    """One bucket, as a multiset difference in both directions.

    `slack` is how many facts the other side declined to resolve. An item the
    binary could not read is not evidence that ours is wrong, so that many
    unmatched items are absorbed rather than reported. Without this every
    handler fetched through a pointer table -- which the binary loads in two
    steps and this reader will not follow -- came out as ours being invented.
    """
    ca, cb = collections.Counter(a), collections.Counter(b)
    only_bin = ca - cb
    only_c = cb - ca
    for _ in range(slack_c):
        if not only_bin:
            break
        k = next(iter(only_bin))
        only_bin[k] -= 1
        if only_bin[k] <= 0:
            del only_bin[k]
    for _ in range(slack_bin):
        if not only_c:
            break
        k = next(iter(only_c))
        only_c[k] -= 1
        if only_c[k] <= 0:
            del only_c[k]
    for k, n in sorted(only_bin.items(), key=lambda x: str(x[0])):
        out.append("    %-9s binario tiene %s  x%d  y el C no" % (label, k, n))
    for k, n in sorted(only_c.items(), key=lambda x: str(x[0])):
        out.append("    %-9s el C tiene   %s  x%d  y el binario no"
                   % (label, k, n))
    return len(only_bin) + len(only_c)


_IMAGE = None


def image_loaded(name):
    """Every routine the function loads, pointer slots resolved through the image.

    Borrowed from handlercheck.py, and only asked for when the handler bucket
    disagrees, because it runs dumpfn.py once per function.
    """
    global _IMAGE
    import handlercheck
    if _IMAGE is None:
        _IMAGE = (handlercheck.load_image(), handlercheck.load_names())
    return handlercheck.loaded_by(name, *_IMAGE)


RE_HEXLIT = re.compile(r"\b(0x[0-9a-f]+)u\b")


def compare_by_name(a, b, label, out, allowed):
    """Handlers and tokens, compared as SETS.

    The binary shares tails: two paths load different handlers into one
    register and jump to a single physical store, which the machine reader
    sees as one `?`. The C writes each path out. Counting stores can never
    match that, and the counts were what the old shared slack papered over --
    along with real mistakes. So instead:

        every concrete value the binary stores must appear in the C, and
        every value the C stores must be one the binary could produce
        (`allowed`: routines it loads, or constants in its code).

    A wrong routine or a wrong token number still fails. 0x0 is the cleared
    slot every install writes and is not compared.
    """
    sa = set(a) - {"0x0"}
    sc = set(b) - {"0x0"}
    bad = 0
    for k in sorted(sa - sc):
        out.append("    %-9s binario tiene %s  y el C no" % (label, k))
        bad += 1
    extra = sc - sa                    # only these need the (slow) image
    for k in sorted(extra - allowed() if extra else ()):
        out.append("    %-9s el C tiene   %s  y el binario no lo produce" % (label, k))
        bad += 1
    return bad


# Where a store lands, from the machine reader's `origin` of its base: the
# object is whatever `thread->proc` (0x108) loaded, its header is word 0 of
# that. Only these two are told apart -- they are the pair a transcription
# confuses, `obj->x` against `obj->field00->x`.
ASM_BASE = {"[r0+0x108]": "MK3OBJ", "[[r0+0x108]+0x0]": "MK3OBJPROC",
            "[[r0+0x108]+0x8]": "MK3OBJ.field08"}


def compare_bases(afacts, cfacts, out):
    """Which struct each written offset lands in, compared as sets.

    The store bucket compares (offset, value) and never looked at where the
    write goes, so `obj->field00->field30 = 0` passed for a binary that writes
    `obj->field30 = 0` -- mkreact.c's t_b_weak_silent, and the five move-table
    functions in mkboss.c. The value does not matter here, so computed stores
    are checked too: for every offset the binary writes into the object or its
    header, the C must write that offset into the same one. Sets, not counts,
    for the same shared-tail reason as handlers. Offsets only the C can place
    with certainty (tag `?` otherwise) take part.
    """
    bin_where = collections.defaultdict(set)
    for f in afacts:
        if f[0] == "store" and len(f) > 3 and f[3] in ASM_BASE:
            bin_where[f[1]].add(ASM_BASE[f[3]])
    c_where = collections.defaultdict(set)
    c_unsure = set()
    for f in cfacts:
        if f[0] == "store" and len(f) > 3:
            if f[3] == "?":
                c_unsure.add(f[1])
            elif f[3] in ("MK3OBJ", "MK3OBJPROC", "MK3OBJ.field08"):
                c_where[f[1]].add(f[3])
    bad = 0
    for off in sorted(bin_where, key=lambda x: int(x, 16)):
        if off in c_unsure or off not in c_where:
            continue
        for want in sorted(bin_where[off] - c_where[off]):
            out.append("    %-9s +%s va a %s en el binario; el C lo escribe en %s"
                       % ("base", off, want, "/".join(sorted(c_where[off]))))
            bad += 1
    return bad


def check(name, afacts, cfacts, alines=None):
    """One function: a verdict, the lines explaining it, and the unchecked."""
    a = asm_sets(afacts)
    c = c_sets(cfacts)
    au, cu = a[4], c[4]
    out, n = [], 0
    n += compare_stores(a[0], c[0], out, au.get("store_offs", []),
                        cu.get("store_offs", []))
    n += compare_bases(afacts, cfacts, out)
    lines = alines or []

    def routines():
        return image_loaded(name) | set(a[1])

    def constants():
        # literals, plus anything a register can hold after add/sub of an
        # immediate: t_scorpion_hell compares a token against 0x2ad - 0xe
        lit = {hex(int(x, 16)) for x in RE_HEXLIT.findall("\n".join(lines))}
        return lit | {hex(v) for v in asm_ret_consts(lines, "*")} | set(a[2])

    n += compare_by_name(a[1], c[1], "handler", out, routines)
    n += compare_by_name(a[2], c[2], "token", out, constants)
    # Leading underscores are the one spelling the two sides disagree on:
    # the binary's `__do_winner_char` is `_do_winner_char` in C and
    # `do_winner_char` in the transcription.
    n += compare([x.lstrip("_") for x in a[3]], [x.lstrip("_") for x in c[3]],
                 "call", out)
    # Return values: a non-zero constant the C returns must be one the binary
    # can put in r0. Zero is exempt -- the binary usually returns a zero it
    # already holds (the token) rather than loading one.
    if alines is not None:
        possible = asm_ret_consts(alines)
        for r in sorted(set(c[5])):
            v = int(r, 16) & 0xffffffff
            if v and v not in possible:
                out.append("    %-9s el C devuelve %s y el binario nunca lo pone en r0"
                           % ("ret", r))
                n += 1
    return n, out, (sum(v for k, v in au.items() if k != "store_offs")
                    + sum(v for k, v in cu.items() if k != "store_offs"))


def load_waivers():
    """{(file, function): reason} from factdiff_waivers.txt.

    A waiver is a function the tool CANNOT check -- a structure it does not
    model, a type it cannot place -- that a person has read against the
    disassembly. It is printed every run and never counted as a pass.
    Format, one per line:   file.c : function : reason
    """
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                        "factdiff_waivers.txt")
    out = {}
    if os.path.exists(path):
        for ln in open(path, encoding="utf-8"):
            ln = ln.strip()
            if ln and not ln.startswith("#"):
                parts = [x.strip() for x in ln.split(":", 2)]
                if len(parts) == 3:
                    out[(parts[0], parts[1])] = parts[2]
    return out


def main(argv):
    args = [x for x in argv[1:] if not x.startswith("--")]
    summary = "--summary" in argv
    rec, src = args[0], args[1]
    want = args[2] if len(args) > 2 else None

    afns = facts_asm.split_functions(
        open(rec, encoding="utf-8", errors="replace").read())
    maps = facts_c.offsets()
    cfns = facts_c.split_functions(src)

    clean = diff = missing = waived = 0
    WAIVERS = load_waivers()
    unchecked_total = 0
    for name in sorted(cfns):
        if want and name != want:
            continue
        if name not in afns and name.lstrip("_") in afns:
            afns[name] = afns[name.lstrip("_")]
        if name not in afns:
            missing += 1
            if not summary:
                print("??  %-28s no esta en la transcripcion" % name)
            continue
        n, lines, unk = check(name,
                              facts_asm.facts(afns[name]),
                              facts_c.facts_of(cfns[name], maps),
                              afns[name])
        unchecked_total += unk
        if n == 0:
            clean += 1
            if not summary:
                print("OK  %-28s (%d sin comprobar)" % (name, unk))
        elif (os.path.basename(src), name) in WAIVERS:
            # known and explained, listed by hand in factdiff_waivers.txt:
            # not a pass, and not counted as one
            waived += 1
            print("~~  %-28s exenta: %s" % (name, WAIVERS[(os.path.basename(src), name)]))
        else:
            diff += 1
            print("!!  %-28s %d diferencia(s), %d sin comprobar"
                  % (name, n, unk))
            for ln in lines:
                print(ln)

    total = clean + diff + missing
    print()
    print("%d funciones: %d coinciden, %d con diferencias, %d sin transcripcion"
          % (total, clean, diff, missing))
    if waived:
        print("%d exentas (tools/factdiff_waivers.txt): no cuentan como pasadas"
              % waived)
    print("%d hechos quedaron sin comprobar (un lado dijo '?')"
          % unchecked_total)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
