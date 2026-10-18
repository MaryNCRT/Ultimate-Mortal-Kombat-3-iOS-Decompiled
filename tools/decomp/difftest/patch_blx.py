#!/usr/bin/env python3
"""patch_blx.py <in recompiled.c> <out recompiled.c>

The recompiler cannot translate `blx rN` (a call through a register) and
emits an abort for it. For the differential test that is the whole point of
the function -- `t_stance_wait_yes` calls the probe it finds in obj+0x48 -- so
here every such abort becomes a call through `arm_dispatch`, which the harness
resolves against the table of recompiled functions.
"""
import re
import sys

src = open(sys.argv[1], encoding="utf-8", errors="replace").read()
pat = re.compile(
    r"(/\* ([0-9a-f]{8})\s+blx (\w+) \*/\n)\s*arm_unimplemented\(\"[^\"]*\", 0x[0-9a-f]+, \"blx indirecto\"\);")


def repl(m):
    reg = m.group(3)
    n = {"sb": 9, "sl": 10, "fp": 11, "ip": 12, "lr": 14}.get(reg)
    if n is None:
        n = int(reg[1:])
    ret = int(m.group(2), 16) + 2
    return "%s    ctx->r[LR] = 0x%08xu | 1u; arm_dispatch(ctx, ctx->r[%d]);" % (m.group(1), ret, n)


out, n = pat.subn(repl, src)
if n:
    inc = '#include "recompiled.h"\n'
    out = out.replace(inc, inc + "void arm_dispatch(arm_ctx *ctx, uint32_t target);\n", 1)
open(sys.argv[2], "w", encoding="utf-8").write(out)
print("%s: %d indirect calls patched" % (sys.argv[1], n))
