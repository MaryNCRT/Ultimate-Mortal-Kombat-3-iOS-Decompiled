#!/usr/bin/env python3
"""cd.py -- compact annotated disassembly for hand transcription.

    python tools/cd.py fn [fn ...]

Like dumpfn.py but terse, with every pc-relative literal resolved: the value of
`ldr rX, [pc, #k]` is read from the image (Thumb alignment: Align(addr+4,4)+k),
and a following `add rX, pc` turns it into the address it points at, which is
then named from the symbol table -- a function, a global, or a __DATA pointer
slot (shown as `slot -> name`, the name of what the slot holds).

The point is tokens: one line per instruction, no addresses beyond the low five
hex digits, no blank lines, no literal-pool junk after the function's end.
"""
import os
import re
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import umk3paths  # noqa: E402

_DATA = None
_SYM = None


def image():
    global _DATA
    if _DATA is None:
        _DATA = open(umk3paths.require_slice(), "rb").read()
    return _DATA


def rd(a):
    d = image()
    return struct.unpack("<I", d[a - 0x1000:a - 0x1000 + 4])[0]


def symbols():
    global _SYM
    if _SYM is None:
        _SYM = {}
        path = os.path.join(HERE, "..", "work", "symbols.txt")
        for ln in open(path, encoding="utf-8", errors="replace"):
            p = ln.split()
            if len(p) >= 5 and p[1] == "SECT":
                _SYM.setdefault(int(p[0], 16), p[-1].lstrip("_"))
    return _SYM


def name_of(a):
    s = symbols()
    if a in s:
        return s[a]
    if (a & ~1) in s:
        return s[a & ~1]
    if (a - 1) in s:
        return s[a - 1]
    return None


def describe(a):
    n = name_of(a)
    if n:
        return n
    if 0xf0000 <= a < 0x100000:
        v = rd(a)
        return "slot->%s" % (name_of(v) or hex(v))
    return hex(a)


def main(argv):
    for fn in argv[1:]:
        out = subprocess.run([sys.executable, os.path.join(HERE, "dumpfn.py"), fn],
                             capture_output=True, text=True).stdout.split("\n")
        head = [l for l in out if "bytes" in l][:1]
        print("== " + (head[0].split("  ")[0] + " " + " ".join(head[0].split()[1:3]) if head else fn))
        rows = []
        pend = {}                       # reg -> literal value
        hm = re.search(r"0x([0-9a-f]+)\s+(\d+) bytes", " ".join(head))
        end = int(hm.group(1), 16) + int(hm.group(2)) if hm else 1 << 40
        for l in out:
            m = re.match(r"0x([0-9a-f]+)\s+(\S+)\s*(.*)", l)
            if not m:
                continue
            a, mn, ops = int(m.group(1), 16), m.group(2), m.group(3).split(";")[0].rstrip()
            if a >= end:
                break
            note = ""
            lm = re.match(r"(\w+), \[pc, #(0x[0-9a-f]+|\d+)\]", ops)
            if mn.startswith("ldr") and lm:
                lit = ((a + 4) & ~3) + int(lm.group(2), 0)
                v = rd(lit)
                pend[lm.group(1)] = v
                note = "= 0x%x" % v
            am = re.match(r"(\w+), pc$", ops)
            if mn == "add" and am and am.group(1) in pend:
                tgt = (pend[am.group(1)] + a + 4) & 0xffffffff
                note = "-> " + describe(tgt)
            if mn.startswith("bl") and ";" in l:
                note = l.split(";")[1].strip()
                ops = ops.split("#")[0] + "#" + ops.split("#")[-1] if "#" in ops else ops
            rows.append((a & 0xfffff, mn, ops, note))
            if mn in ("bx", "pop") and "lr" in ops + "pc" and False:
                ended = True
        for ln in (rows if RAW else fold(rows)):
            print(ln if isinstance(ln, str) else
                  "%05x %s %s%s" % (ln[0], ln[1], ln[2], ("   ; " + ln[3]) if ln[3] else ""))
        print()


RAW = "--raw" in sys.argv


def fold(rows):
    """Collapse the boilerplate every frame-handler repeats into one line:

        TOKCHK obj=rX tok=rY      the refusal prologue (frame+1 token read)
        INSTALL rH [tok=rZ]       handler into frame[frame], token slot above cleared

    and drop the push/pop/return-value plumbing. Anything that does not match
    exactly is left as the raw instruction."""
    out = []
    i = 0
    rows = [r for r in rows
            if not (r[1] in ("nop",) or (r[1] == "mov" and re.match(r"r0, r[4-9]$", r[2]) and False))]
    n = len(rows)

    def at(k):
        return rows[i + k] if i + k < n else (0, "", "", "")

    while i < n:
        a = at(0)
        # INSTALL: ldr r3,[T,#0xa4]; (mov r0,K)?; lsls r3,r3,#3; adds r3,r3,T; str H,[r3,#4];
        #          ldr r3,[T,#0xa4]; adds r3,#1; str Z,[T,r3,lsl #3]
        j = i
        seq = []
        while j < n and len(seq) < 9:
            if rows[j][1] == "mov" and re.match(r"r0, r\d+$", rows[j][2]):
                j += 1
                continue
            seq.append(rows[j])
            j += 1
            if len(seq) == 7:
                break
        if len(seq) == 7:
            m1 = re.match(r"r3, \[(\w+), #0xa4\]$", seq[0][2])
            ok = (seq[0][1].startswith("ldr") and m1 and seq[1][1].startswith("lsl")
                  and re.match(r"r3, r3, #3$", seq[1][2]) and seq[2][1] == "adds"
                  and re.match(r"r3, r3, %s$" % m1.group(1), seq[2][2])
                  and seq[3][1] == "str" and re.match(r"(\w+), \[r3, #4\]$", seq[3][2])
                  and seq[4][1].startswith("ldr") and seq[4][2] == seq[0][2]
                  and seq[5][1] == "adds" and seq[5][2] == "r3, #1"
                  and seq[6][1].startswith("str"))
            if ok:
                h = re.match(r"(\w+), \[r3, #4\]$", seq[3][2]).group(1)
                out.append("%05x INSTALL %s" % (seq[0][0], h))
                i = j
                continue
        # TOKCHK: ldr r3,[T,#0xa4]; ..; ldr O,[T,#0x108]; adds r3,#1; ldr K,[T,r3,lsl #3]; cbnz/cbz K
        if a[1].startswith("ldr") and re.match(r"r\d+, \[r0, #0xa4\]$", a[2]):
            win = rows[i:i + 7]
            txt = [(w[1], w[2]) for w in win]
            o = k = None
            for w in win:
                m = re.match(r"(\w+), \[r0, #0x108\]$", w[2])
                if w[1].startswith("ldr") and m:
                    o = m.group(1)
                m = re.match(r"(\w+), \[r0, r3, lsl #3\]$", w[2])
                if w[1].startswith("ldr") and m:
                    k = m.group(1)
            if o and k:
                cut = None
                for t, w in enumerate(win):
                    if w[1] in ("cbnz", "cbz", "cmp", "bne", "beq") and k in w[2]:
                        cut = t
                        break
                if cut is not None:
                    out.append("%05x TOKCHK obj=%s tok=%s   (%s)" % (a[0], o, k, win[cut][1]))
                    i += cut + 1
                    continue
        out.append(a)
        i += 1
    return [o for o in out if not (not isinstance(o, str) and o[1] in ("pop", "bx") and "lr" in o[2] + "pc")
            and not (not isinstance(o, str) and o[1] == "mvn" and o[2] == "r0, #2")]


if __name__ == "__main__":
    main([a for a in sys.argv if a != '--raw'])
