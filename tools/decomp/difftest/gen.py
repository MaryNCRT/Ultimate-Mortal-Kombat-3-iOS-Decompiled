#!/usr/bin/env python3
"""gen.py -- build the pieces of the differential test for one decomp file.

    python tools/difftest/gen.py <stem> <rc_dir> <out_dir> [fn ...]

`factdiff.py` compares what two functions *say* (stores, handlers, calls,
tokens). It cannot tell which return value goes with which path, nor catch a
wrong argument order or a wrong condition on a value it cannot fold. This
runs both: the recompiled ARM oracle and the decompiled C, on the SAME memory
(a 32-bit process, the binary's data image mapped at its own addresses), for
many randomised states, and compares what each leaves behind.

What this writes into <out_dir>:

  gen_tests.c    the list of functions under test, with the token values and
                 comparison constants found in each one's ARM code
  gen_shims.c    for every function the decomp calls but does not define, a C
                 shim that runs the oracle's version of it
  gen_addrmap.c  native function address <-> ARM address, so handler addresses
                 written by either side compare equal
  gen_vars.c     the pointer variables (G, H, Plyr ...) that the decomp reads as
                 `extern T *x` but the binary keeps as arrays
  gen_abs.c      absolute symbols for data the decomp names as arrays (tables, funcs_N)
"""
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
TOOLCHAIN = os.path.expanduser("~/.local/share/retcomm/toolchains/cmake-clang-v1/latest/bin")
CC = os.path.join(TOOLCHAIN, "i686-w64-mingw32-gcc.exe")
NM = os.path.join(TOOLCHAIN, "llvm-nm.exe")

LOGIC = os.path.join(REPO, "src", "decomp", "gamecode", "logic")


def read_symbols():
    """{name: addr} for data symbols, plus sizes from the gap to the next one."""
    ents = []
    for ln in open(os.path.join(REPO, "work", "symbols.txt"), encoding="utf-8", errors="replace"):
        p = ln.split()
        if len(p) >= 5 and p[1] == "SECT":
            ents.append((int(p[0], 16), p[-1].lstrip("_")))
    ents.sort()
    syms, size = {}, {}
    for i, (a, n) in enumerate(ents):
        n2 = n.replace(".", "_")
        syms.setdefault(n2, a)
        nxt = next((b for b, _ in ents[i + 1:] if b > a), a + 4)
        size.setdefault(n2, nxt - a)
    return syms, size


def decomp_functions(path):
    """{name: kind} for every `long f(MK3THREAD *)` / `void f(MK3OBJ *)` defined."""
    src = open(path, encoding="utf-8", errors="replace").read()
    out = {}
    voids = set()
    for m in re.finditer(r"^(?:static\s+)?(long|void|int32_t|uint32_t)\s+(\w+)\(\s*(?:struct\s+)?(MK3THREAD|MK3OBJ)\s*\*\s*\w+\s*\)\s*\n?\s*\{", src, re.M):
        out[m.group(2)] = "thread" if m.group(3) == "MK3THREAD" else "obj"
        if m.group(1) == "void":
            voids.add(m.group(2))
    decomp_functions.voids = voids      # void: r0 after the call is not a result
    return out, src


def pointer_externs(*paths):
    """Names declared `extern T *name;` -- variables that hold an address."""
    out = set()
    for p in paths:
        txt = open(p, encoding="utf-8", errors="replace").read()
        for m in re.finditer(r"^\s*extern\s+[\w\s]+?\*\s*(\w+)\s*;", txt, re.M):
            out.add(m.group(1))
    return out


def oracle_functions(rc_root, only=None):
    """{name: (addr, cname)} from every recompiled.h under rc_root (or one dir)"""
    out = {}
    dirs = [only] if only else sorted(os.listdir(rc_root))
    for d in dirs:
        h = os.path.join(rc_root, d, "recompiled.h")
        if not os.path.exists(h):
            continue
        for m in re.finditer(r"void func_([0-9a-f]{8})_(\w+)\(arm_ctx \*ctx\);", open(h).read()):
            out[m.group(2)] = (int(m.group(1), 16), "func_%s_%s" % (m.group(1), m.group(2)))
    return out


def scan_constants(rc_text, fname):
    """tokens (movw/mov.w >= 0x100) and cmp immediates in one recompiled body"""
    i = rc_text.find("void %s(arm_ctx *ctx)\n{" % fname)
    if i < 0:
        return [], []
    j = rc_text.find("\n}\n", i)
    body = rc_text[i:j]
    toks, imms = set(), set()
    for m in re.finditer(r"/\* [0-9a-f]{8}\s+(movw|mov\.w|movs|mov|cmp\.w|cmp|cmn)\s+\w+,\s*#(-?0x[0-9a-fA-F]+|-?\d+)", body):
        mn, v = m.group(1), int(m.group(2), 0) & 0xffffffff
        if mn in ("movw", "mov.w", "mov", "movs") and 0x100 <= v <= 0xffff:
            toks.add(v)
        if mn.startswith("cmp") or mn == "cmn":
            imms.add(v)
    return sorted(toks), sorted(imms)


# variables the binary holds a POINTER in (the decomp reads their value), so the
# value is the slot's contents, not the symbol address: name -> contents
SLOT_VARS = {"txt_tie": 0x174b78}


def main(argv):
    stem, rc_dir, out_dir = argv[1], argv[2], argv[3]     # rc_dir: the root holding one dir per file
    only = set(argv[4:])
    os.makedirs(out_dir, exist_ok=True)
    src_path = os.path.join(LOGIC, stem + ".c")
    dfuncs, src_text = decomp_functions(src_path)
    code_text = re.sub(r"/\*.*?\*/|//[^\n]*", "", src_text, flags=re.S)   # comments say "&G + 0x440"
    ofuncs = oracle_functions(rc_dir)
    own = oracle_functions(rc_dir, stem)
    rc_text = open(os.path.join(rc_dir, stem, "recompiled.c"), encoding="utf-8", errors="replace").read()
    tests = [n for n in dfuncs if n in own]
    syms, sizes = read_symbols()
    ptr_vars = pointer_externs(os.path.join(LOGIC, "mk3logic.h"), src_path)

    # compile the decomp to see what it needs
    obj = os.path.join(out_dir, stem + ".o")
    subprocess.run([CC, "-std=gnu11", "-O1", "-fno-strict-aliasing", "-w", "-c", "-I", LOGIC, src_path, "-o", obj], check=True)
    nm = subprocess.run([NM, obj], capture_output=True, text=True).stdout
    undef = sorted({l.split()[-1].lstrip("_") for l in nm.splitlines() if " U " in l})
    defined = {l.split()[-1].lstrip("_") for l in nm.splitlines() if re.search(r" [TtDdBbRrCc] ", l)}

    # `extern T name[];            /* 0xADDR */`               the table is at ADDR
    # `extern T *name;             /* slot 0xADDR */`          the variable is at ADDR
    # `extern T *name;             /* pointer slot -> 0xADDR */` / `/* 0xADDR */`
    #                                                          the variable HOLDS ADDR
    commented = {}
    hdr_text = open(os.path.join(LOGIC, "mk3logic.h"), encoding="utf-8", errors="replace").read()
    for m in re.finditer(r"^\s*extern\s+[\w\s]+?(\**)\s*(\w+)\s*(\[\s*\w*\s*\])?\s*;\s*/\*\s*(slot\s+|pointer slot\s*->\s*)?0x([0-9a-fA-F]+)",
                         hdr_text + "\n" + src_text, re.M):
        star, name, arr, how, addr = m.groups()
        if arr or (how or "").strip() == "slot":
            commented.setdefault(name, ("abs", int(addr, 16)))
        elif star:
            commented.setdefault(name, ("pvar", int(addr, 16)))

    shims, defsyms, pvars, unresolved = [], [], [], []
    for u in undef:
        if u in ofuncs and u not in dfuncs:
            shims.append(u)
        elif u in ptr_vars and u in syms and sizes.get(u, 4) > 8 \
                and not re.search(r"(?<![&\s\w])&%s\b|[=(,]\s*&\s*%s\b"   # address-of, not `&&` or a bitwise `a & b`
                                  % (re.escape(u), re.escape(u)), code_text):
            # (`&bt_null` takes the symbol's own address: the table, as in
            # the binary -- so it is an absolute symbol, not a variable)
            pvars.append(u)
        elif u in syms:
            defsyms.append(u)
        elif u in ("memset", "memcpy", "memcmp", "memmove", "strlen", "rand", "atan2",
                   "printf", "fprintf", "puts", "fflush"):
            # the C library: linked from libc. Setting one to 0 also breaks
            # the harness's own printf/fflush (mkstat, playback segfaulted)
            pass
        elif u in commented:
            # no symbol in the binary (an anonymous table, a literal-pool
            # slot): the declaration's own comment gives the address
            kind, addr = commented[u]
            syms[u] = addr
            (pvars if kind == "pvar" else defsyms).append(u)
        else:
            unresolved.append(u)
    if unresolved:
        print("unresolved symbols (will be 0):", " ".join(unresolved))

    # ---- gen_shims.c
    with open(os.path.join(out_dir, "gen_shims.c"), "w") as f:
        f.write('/* generated by gen.py */\n#include "difftest.h"\n\n')
        for n in shims:
            f.write("void %s(arm_ctx *ctx);\n" % ofuncs[n][1])
        f.write("\n")
        for n in shims:
            f.write("uint32_t %s(uint32_t a, uint32_t b, uint32_t c, uint32_t d)\n{\n"
                    "    return oracle_call(%s, a, b, c, d);\n}\n\n" % (n, ofuncs[n][1]))
        f.write("const int g_nshims = %d;\n" % len(shims))

    # ---- gen_vars.c : pointer variables holding the binary's array addresses
    with open(os.path.join(out_dir, "gen_vars.c"), "w") as f:
        f.write("/* generated by gen.py */\n#include <stdint.h>\n\n")
        for n in pvars:
            f.write("void *%s = (void *)0x%08x;\n" % (n, SLOT_VARS.get(n, syms[n])))
        for n in unresolved:
            f.write("/* unresolved: %s */\nuint32_t %s_unresolved_dummy;\n" % (n, n))

    # ---- gen_abs.c : data symbols at their binary addresses (absolute symbols)
    with open(os.path.join(out_dir, "gen_abs.c"), "w") as f:
        f.write("/* generated by gen.py */\n")
        for n in defsyms:
            f.write('__asm__(".globl _%s\\n.set _%s, 0x%x");\n' % (n, n, syms[n]))
        for n in unresolved:
            f.write('__asm__(".globl _%s\\n.set _%s, 0x0");\n' % (n, n))

    # ---- gen_addrmap.c
    with open(os.path.join(out_dir, "gen_addrmap.c"), "w") as f:
        f.write('/* generated by gen.py */\n#include "difftest.h"\n\n')
        names = [n for n in dfuncs if n in ofuncs] + shims
        for n in names:
            f.write("extern void %s();\n" % n)
        f.write("const AddrMap g_addrmap[] = {\n")
        seen = set()
        for n in names:
            if n in seen:
                continue
            seen.add(n)
            f.write('    { (uint32_t)(uintptr_t)&%s, 0x%08x, "%s" },\n' % (n, (own.get(n) or ofuncs[n])[0] | 1, n))
        f.write("};\nconst int g_naddr = %d;\n" % len(seen))

    # ---- gen_oracle.c : every recompiled function by ARM address, for `blx rN`
    with open(os.path.join(out_dir, "gen_oracle.c"), "w") as f:
        f.write('/* generated by gen.py */\n#include "difftest.h"\n\n')
        ents = sorted({(a, c) for a, c in ofuncs.values()})
        for a, c in ents:
            f.write("void %s(arm_ctx *ctx);\n" % c)
        f.write("const OracleEnt g_oracle[] = {\n")
        for a, c in ents:
            f.write("    { 0x%08x, %s },\n" % (a, c))
        f.write("};\nconst int g_noracle = %d;\n" % len(ents))

    # ---- gen_tests.c
    with open(os.path.join(out_dir, "gen_tests.c"), "w") as f:
        f.write('/* generated by gen.py */\n#include "difftest.h"\n#include "recompiled.h"\n\n')
        for n in tests:
            f.write("extern void %s();\n" % n)
        f.write("const Test g_tests[] = {\n")
        for n in tests:
            toks, imms = scan_constants(rc_text, own[n][1])
            toks = toks[:12]
            imms = imms[:40]
            f.write('    { "%s", (void *)%s, %s, %d, %d, { %s }, %d, { %s }, %d },\n' % (
                n, n, own[n][1], 0 if dfuncs[n] == "thread" else 1,
                len(toks), ", ".join("0x%x" % t for t in toks) or "0",
                len(imms), ", ".join("0x%x" % t for t in imms) or "0",
                1 if n in decomp_functions.voids else 0))
        f.write("};\nconst int g_ntests = %d;\n" % len(tests))
    print("%d tests, %d shims, %d defsyms, %d pointer vars" % (len(tests), len(shims), len(defsyms), len(pvars)))


if __name__ == "__main__":
    main(sys.argv)
