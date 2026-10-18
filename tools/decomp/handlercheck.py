#!/usr/bin/env python3
"""handlercheck.py -- do the handlers our C names match the ones the binary loads?

`factdiff.py` cannot answer this for a large class of handlers. A handler the
binary fetches through a __DATA pointer slot (`ldr r3,[pc,#n]; add r3,pc;
ldr r2,[r3]`) comes out of the machine transcription as `?`, and a `?` is
slack, not a disagreement. So a slot resolved to the wrong routine by hand --
or two slots read the wrong way round -- still verifies.

That happened. In mkboss.c, slots 0x000f3418 and 0x000f3428 were once
labelled `t_motaro_slided` and `t_d_block`; they hold `t_d_block` and
`t_return_to_beware`, and four functions installed the wrong routines. Slot
0x000f33fc was read as `t_d_beware` in five more; it holds `t_d_stance_pause`.
Every one of them came back OK from landfn.sh.

This reads each function's disassembly, resolves every address it loads --
dereferencing pointer slots through the image -- and every `bl` target, and
checks two things against the C body:

    forward   each routine the binary loads is named somewhere in the C
    reverse   each routine the C installs, pushes or casts to a pointer is
              one the binary actually loads

A hit is not proof of a bug -- a comment mentioning a name satisfies the
forward check, and a literal that travels further than an `ldr` and a
`mov` before its `add pc` is still missed. Check a hit against the
disassembly before changing anything.

Usage:
    python tools/handlercheck.py mkboss.c [function ...]
"""
import os
import re
import struct
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import umk3paths                                             # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
LOGIC = os.path.join(HERE, "..", "..", "src", "decomp", "gamecode", "logic")

# The __DATA pointer slots the gamecode fetches handlers through all live in
# this window; an address inside it is dereferenced, anything else is taken
# as a routine's own address.
SLOT_LO, SLOT_HI = 0xf3000, 0xf4000


def load_image():
    data = open(umk3paths.require_slice(), "rb").read()
    return lambda a: struct.unpack("<I", data[a - 0x1000:a - 0x1000 + 4])[0]


def load_names():
    names = {}
    with open(umk3paths.func_to_file(), encoding="utf-8") as fh:
        for line in fh:
            p = line.split()
            if len(p) >= 2 and p[0].startswith("0x"):
                names[int(p[0], 16)] = p[1].lstrip("_")
    return names


def c_bodies(src):
    out = {}
    for m in re.finditer(r"^(?:long|void|int32_t|uint32_t)\s+(\w+)\s*"
                         r"\([^;{]*\)\s*\{", src, re.M):
        i, depth = m.end(), 1
        while depth:
            depth += {"{": 1, "}": -1}.get(src[i], 0)
            i += 1
        out[m.group(1)] = src[m.start():i]
    return out


RE_LDR = re.compile(r"0x([0-9a-f]+)\s+ldr(?:\.w)?\s+(r\d+), \[pc, #0x([0-9a-f]+)\]")
RE_MOV = re.compile(r"0x[0-9a-f]+\s+mov\s+(r\d+), (r\d+)$")
RE_ADDPC = re.compile(r"0x([0-9a-f]+)\s+add\s+(r\d+), pc")


def split_pairs(out, word):
    """The `ldr rX,[pc,#n] ... add rX,pc` pairs dumpfn.py leaves alone.

    dumpfn resolves the pair only when it can see it whole. When the
    compiler parks the literal in another register first -- `ldr r6, ...;
    mov r1, r6; add r1, pc` ahead of a NewThread -- it prints nothing, and
    the routine reads as never loaded. Follow the literal through `mov`.
    The `add` uses its own pc, unaligned (see the Thumb literal memory).
    """
    lines = out.splitlines()
    lit, found = {}, []
    for i, ln in enumerate(lines):
        ln = ln.strip()
        m = RE_LDR.match(ln)
        if m:
            a = int(m.group(1), 16)
            lit[m.group(2)] = word(((a + 4) & ~3) + int(m.group(3), 16))
            continue
        m = RE_MOV.match(ln)
        if m and m.group(2) in lit:
            lit[m.group(1)] = lit[m.group(2)]
            continue
        m = RE_ADDPC.match(ln)
        if m:
            resolved = i + 1 < len(lines) and "->" in lines[i + 1]
            if not resolved and m.group(2) in lit:
                found.append((lit[m.group(2)] + int(m.group(1), 16) + 4)
                             & 0xffffffff)
    return found


def loaded_by(fn, word, names):
    out = subprocess.run([sys.executable, os.path.join(HERE, "dumpfn.py"), fn],
                         capture_output=True, text=True).stdout
    loaded = set()
    addrs = [int(a, 16) for a in re.findall(r"-> 0x([0-9a-f]{8})", out)]
    addrs += split_pairs(out, word)
    for a in addrs:
        tgt = (word(a) if SLOT_LO <= a < SLOT_HI else a) & ~1
        if tgt in names:
            loaded.add(names[tgt])
    for a in re.findall(r"bl\s+#0x([0-9a-f]+)", out):
        if int(a, 16) in names:
            loaded.add(names[int(a, 16)])
    return loaded


def main(argv):
    if len(argv) < 2:
        sys.exit(__doc__)
    src = open(os.path.join(LOGIC, argv[1]), encoding="utf-8").read()
    want = set(argv[2:])
    word, names = load_image(), load_names()
    known = set(names.values())
    bad = 0
    bodies = c_bodies(src)
    for fn, body in bodies.items():
        if want and fn not in want:
            continue
        loaded = loaded_by(fn, word, names)
        for nm in sorted(loaded - {fn}):
            if not re.search(r"\b%s\b" % re.escape(nm), body):
                print("%-28s binary loads %-28s -- the C never names it"
                      % (fn, nm))
                bad += 1
        used = set(re.findall(r"\(MK3THREADFUNC\)\s*(\w+)", body))
        used |= set(re.findall(r"\(uintptr_t\)\s*(\w+)", body))
        for nm in sorted((used & known) - loaded):
            print("%-28s C installs   %-28s -- the binary never loads it"
                  % (fn, nm))
            bad += 1
    n = len(want) if want else len(bodies)
    print("%d functions, %d mismatches" % (n, bad))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
