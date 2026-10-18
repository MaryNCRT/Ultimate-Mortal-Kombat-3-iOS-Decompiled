#!/usr/bin/env python3
"""protos.py -- every declaration must match the definition it refers to.

`check.sh` compiles each file on its own, so a prototype that contradicts a
definition in ANOTHER file is invisible to it: both compile, and only the
linker would notice -- and only for the return type, never for the arguments.
That is how `rsnd_func` came to have three different prototypes in three
files, one of which passed a pointer where the definition took an integer.

The definition wins. It was read from the disassembly; a declaration is
somebody's note about a function they were calling.

    python tools/protos.py [--quiet]

Exit status is the number of disagreements, so it can gate a build.
"""

import glob
import io
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TYPE = r"(?:long|void|int|unsigned|uint\d+_t|int\d+_t|bool|float|char|struct \w+|MK3\w+|SWITCHQUEUE)"

DEF = re.compile(r"^(%s[\w \*]*?)\s(\w+)\(([^;{}]*)\)\s*\{" % TYPE, re.M)
DECL = re.compile(r"^(%s[\w \*]*?)\s(\w+)\(([^;{}]*)\);$" % TYPE, re.M)


_ALIAS = None


def aliases():
    """Simple `typedef base name;` pairs found anywhere in the tree.

    `HUDANIM` is `typedef int HUDANIM;` in HudAnim.c and `EPLAYER` is
    `typedef int EPLAYER;` in Players.c. A declaration in another file writes
    `int`, because the typedef is not visible there and copying it would be a
    second definition of one name. Those are the same type, and reporting them
    is noise that buries the real disagreements.
    """
    global _ALIAS
    if _ALIAS is not None:
        return _ALIAS
    _ALIAS = {}
    for f in (glob.glob(os.path.join(ROOT, "src", "decomp", "**", "*.c"), recursive=True) +
              glob.glob(os.path.join(ROOT, "src", "decomp", "**", "*.h"), recursive=True)):
        text = io.open(f, encoding="utf-8", errors="replace").read()
        for base, name in re.findall(
                r"^typedef\s+(unsigned\s+\w+|signed\s+\w+|\w+)\s+(\w+);$",
                text, re.M):
            _ALIAS[name] = base
    return _ALIAS


def norm(ty):
    """One spelling for one type.

    `struct MK3THREAD *` and `MK3THREAD *` are the same type through a
    typedef, and both spellings are in the tree. `void *` standing in for a
    pointer to a named struct is this codebase's convention for "the caller
    does not need the layout" -- deliberate, so it is not a disagreement.
    """
    ty = ty.replace("struct ", "")
    if ty.endswith("*"):
        return "PTR"
    seen = 0
    while ty in aliases() and seen < 4:      # typedefs can chain
        ty = aliases()[ty]
        seen += 1
    return ty


def params(s):
    """A parameter list with the names taken off, so only the types compare.

    `MK3OBJ *obj` and `MK3OBJ *o` are the same prototype. `(void)` and `()`
    are not distinguished either -- C says one means no arguments and the
    other means unspecified, and nothing here relies on the difference.
    """
    s = s.strip()
    if s in ("", "void"):
        return "void"
    out = []
    for p in s.split(","):
        p = p.strip()
        p = re.sub(r"\bconst\b", "", p).strip()
        if "(*" in p.replace(" ", ""):
            # A function pointer carries its name INSIDE the declarator, so
            # the trailing-identifier rule below would cut a piece of the type
            # off instead. `void (*what)(MK3OBJ *)` and `void (*fn)(MK3OBJ *)`
            # are one type under two spellings.
            out.append(re.sub(r"\(\s*\*\s*\w*\s*\)", "(*)",
                              re.sub(r"\s+", "", p)))
            continue
        # Drop a trailing identifier, keeping any stars with the type.
        p = re.sub(r"\s*\b[A-Za-z_]\w*\s*(\[\s*\])?$", "", p) or p
        out.append(norm(re.sub(r"\s+", " ", p).replace(" *", "*").strip()))
    return ", ".join(out)


def fix(bad):
    """Rewrite each declaration's RETURN TYPE to the definition's.

    Only the return type. A parameter list that normalises equal is already
    fine, and one that does not needs a person -- rewriting it here could
    drag a struct into a file that has no declaration for it.
    """
    edits = {}
    for name, f, grt, gpl, df, rt, pl in bad:
        edits.setdefault(f, []).append((name, grt, rt))
    for f, items in sorted(edits.items()):
        t = io.open(f, encoding="utf-8").read()
        n = 0
        for name, grt, rt in items:
            pat = re.compile(r"^%s(\s[\w \*]*?\s?%s\([^;{}]*\);)$"
                             % (re.escape(grt), re.escape(name)), re.M)
            t, k = pat.subn(lambda m: rt + m.group(1), t)
            n += k
        io.open(f, "w", encoding="utf-8", newline="").write(t)
        print("  %-46s %d return types corrected" % (os.path.relpath(f, ROOT), n))


def main(argv):
    quiet = "--quiet" in argv
    defs = {}
    for f in sorted(glob.glob(os.path.join(ROOT, "src", "decomp", "**", "*.c"),
                              recursive=True)):
        t = io.open(f, encoding="utf-8", errors="replace").read()
        for m in DEF.finditer(t):
            defs[m.group(2)] = (m.group(1).strip(), params(m.group(3)), f)

    bad = []
    files = sorted(glob.glob(os.path.join(ROOT, "src", "decomp", "**", "*.c"),
                             recursive=True))
    files += sorted(glob.glob(os.path.join(ROOT, "src", "decomp", "**", "*.h"),
                              recursive=True))
    for f in files:
        t = io.open(f, encoding="utf-8", errors="replace").read()
        for m in DECL.finditer(t):
            name = m.group(2)
            if name not in defs:
                continue
            rt, pl, df = defs[name]
            if df == f:
                continue
            got_rt, got_pl = norm(m.group(1).strip()), params(m.group(3))
            if (norm(got_rt), got_pl) != (norm(rt), pl):
                bad.append((name, f, got_rt, got_pl, df, rt, pl))

    if not quiet:
        for name, f, grt, gpl, df, rt, pl in bad:
            print("  %-24s %s" % (name, os.path.relpath(f, ROOT)))
            print("      declared  %s %s(%s)" % (grt, name, gpl))
            print("      defined   %s %s(%s)   in %s"
                  % (rt, name, pl, os.path.relpath(df, ROOT)))
    if "--fix" in argv:
        fix(bad)
        return 0
    print("  prototypes: %d disagree with their definition" % len(bad))
    return len(bad)


if __name__ == "__main__":
    sys.exit(min(main(sys.argv), 250))
