"""dispatch.py -- map a compiled switch's token dispatch, exactly.

    python tools/dispatch.py <function> [max-token]

Most state machines in `gamecode/logic` open with a dispatch on a token, and
the compiler turns that into a balanced binary search: compare, branch,
subtract a constant, compare again, three or four levels deep, with the bodies
interleaved between the branches. Reading one of those by eye gets a case wrong
sooner or later and **nothing downstream would ever notice** -- a token mapped
to the wrong body is a move that quietly never happens.

So this does not read it. It RUNS it.

A dispatch tree is made of four instruction classes: a constant into a
register, add or subtract a constant, compare, and branch. Nothing else. This
walks the listing executing exactly those and stops at the first instruction
outside the set -- which is, by definition, the body. Feed it every token in a
range and the result is the complete token -> body map, derived rather than
transcribed.

It refuses rather than guesses: an unknown compare or a runaway loop is
reported as such, not silently dropped.

The same idea proved `mk3_remap` over all 131,072 of its inputs, and mapped
`plyrthread`'s 43 tokens onto 36 bodies. Any function whose dispatch touches no
memory can get the same treatment.

## What it does NOT do

It is a starting point, not a substitute for reading the function.

  - A token whose branch lands directly on a shared instruction is reported in
    an "everything else" group along with the genuine default. Checked against
    the hand-decompiled `t_kitana_kiss`, this finds four of its six states and
    puts the other two in a catch-all -- so **a catch-all with a small count is
    a place to go and read, not a proven default.**
  - `t_do_zap` and the other table-indexed dispatchers are refused outright:
    they index an array of handlers rather than compare, and there is no tree to
    walk.
  - It never enters a body, so it says nothing about what a state does.

Use it to get the case list right and to find states a reading missed. The
bodies are still read one at a time.
"""

import collections
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))

CONST = ("movw", "mov.w", "movs", "mov")
ADDSUB = ("adds", "add.w", "subs", "sub.w")
CMP = ("cmp", "cmp.w")
BR = ("b", "beq", "bne", "bgt", "ble", "bge", "blt", "bhi", "bls")


def listing(func):
    out = subprocess.check_output(
        [sys.executable, os.path.join(HERE, "dumpfn.py"), func],
        cwd=os.path.dirname(HERE) or ".").decode("utf-8", "replace")
    ins, order = {}, []
    for line in out.splitlines():
        m = re.match(r"0x([0-9a-fA-F]{8})\s+(\S+)\s*(.*)", line.strip())
        if m:
            a = int(m.group(1), 16)
            ins[a] = (m.group(2), m.group(3).strip())
            order.append(a)
    order.sort()
    return ins, {order[i]: order[i + 1] for i in range(len(order) - 1)}, order


def run(ins, nxt, start, seed, token, tokreg="r2"):
    """Follow the tree for one token. Returns (kind, address)."""
    r = dict(seed)
    r[tokreg] = token
    pc, z, n, c = start, None, None, None

    for _ in range(1000):
        if pc not in ins:
            return ("off-end", pc)
        m, o = ins[pc]
        p = [x.strip() for x in o.split(",")]
        base = m[:-2] if m.endswith(".w") else m

        if m in CONST and len(p) == 2 and p[1].startswith("#"):
            r[p[0]] = int(p[1][1:], 0)
        elif m in ADDSUB and len(p) == 2 and p[1].startswith("#") and p[0] in r:
            d = int(p[1][1:], 0)
            r[p[0]] += d if m.startswith("add") else -d
        elif m in ADDSUB and len(p) == 3 and p[2].startswith("#") and p[1] in r:
            d = int(p[2][1:], 0)
            r[p[0]] = r[p[1]] + (d if m.startswith("add") else -d)
        elif m in CMP and len(p) == 2:
            a = r.get(p[0])
            b = int(p[1][1:], 0) if p[1].startswith("#") else r.get(p[1])
            if a is None or b is None:
                return ("unknown-cmp", pc)
            z, n, c = (a == b), (a < b), (a >= b)
        elif base in BR and o.startswith("#"):
            take = {"b": True, "beq": z, "bne": not z,
                    "bgt": (not z and not n), "ble": (z or n),
                    "bge": not n, "blt": n,
                    "bhi": (c and not z), "bls": (not c or z)}[base]
            pc = int(o[1:], 0) if take else nxt.get(pc)
            continue
        else:
            return ("body", pc)             # the first real instruction
        pc = nxt.get(pc)
        if pc is None:
            return ("off-end", 0)
    return ("loop", pc)


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    func = sys.argv[1]
    hi = int(sys.argv[2], 0) if len(sys.argv) > 2 else 0x900

    ins, nxt, order = listing(func)
    if not ins:
        sys.exit("no listing for %s" % func)

    # Find the register the token arrives in. Every dispatcher in this
    # directory loads it with the frame idiom -- `ldr rX, [thread, rN, lsl #3]`
    # -- so the destination of the first such load is the token. Falling back to
    # r2 covers the handful the compiler happened to spell differently.
    tokreg = None
    for a in order:
        m, o = ins[a]
        mm = re.match(r"(r\d+|ip|lr),\s*\[\w+,\s*\w+,\s*lsl\s*#3\]", o)
        if m.startswith("ldr") and mm:
            tokreg = mm.group(1)
            break
    if tokreg is None:
        tokreg = "r2"

    # Start at the first compare against it, carrying whatever constants the
    # instructions before it had already put in registers.
    seed, start = {}, None
    for a in order:
        m, o = ins[a]
        p = [x.strip() for x in o.split(",")]
        if m in CONST and len(p) == 2 and p[1].startswith("#"):
            seed[p[0]] = int(p[1][1:], 0)
        elif m in CMP and p and p[0] == tokreg:
            start = a
            break
    if start is None:
        sys.exit("%s: found no compare against %s -- its dispatch is not a "
                 "compare tree, or the token is somewhere this cannot see. "
                 "Refusing rather than guessing." % (func, tokreg))

    by = collections.defaultdict(list)
    for t in range(hi):
        by[run(ins, nxt, start, seed, t, tokreg)].append(t)

    bodies = {k: v for k, v in by.items() if len(v) <= 16}
    rest = {k: v for k, v in by.items() if len(v) > 16}

    print("%s: %d tokens over %d bodies\n" % (func, sum(len(v) for v in bodies.values()),
                                              len(bodies)))
    for k, v in sorted(bodies.items(), key=lambda kv: kv[0][1]):
        print("  0x%08x  %-8s %s" % (k[1], k[0],
                                     " ".join("0x%x" % x for x in v)))
    for k, v in sorted(rest.items(), key=lambda kv: kv[0][1]):
        print("\n  0x%08x  %-8s everything else (%d tokens)" % (k[1], k[0], len(v)))


if __name__ == "__main__":
    main()
