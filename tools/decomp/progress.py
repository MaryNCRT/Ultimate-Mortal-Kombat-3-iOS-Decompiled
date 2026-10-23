"""Honest progress count for the decompiled modules.

A function counts as done only if `decomp/` holds a real body for it.

Three modules are counted, each the same way: `lime/common`, `gamecode`, and
`gamecode/logic`. They are separated because they are separate rows in the
README's table and separate bodies of work -- the fight engine is eight times
the size of the rest of gamecode and nothing useful is learned by averaging
them together.

## How a symbol is matched to a C function

By its **Itanium length prefix**, not by trimming type codes off the end.
`__Z36CreateMatrixPaletteForGeneratingMeshPclllfP9BONESINFO` says the identifier
is 36 characters long, so it is exactly `CreateMatrixPaletteForGeneratingMesh`
and everything after it is argument encoding. That is unambiguous.

The earlier version guessed instead, stripping suffixes with a regexp, and it
mismatched anything whose type codes it had not anticipated -- `Pclllf` and
`PhPcfl11limeVECTOR3` among them. It undercounted by two. An older variant of
the same bug undercounted by seven and put three wrong figures in the README.

## The overall figure is measured, not asserted

Five of the eight blocks in the README's table are judgement calls about work
that is not function-counted -- analysis, tooling, format specifications, the
platform rewrite, the SDK stubs. Those stay as numbers a person maintains.

The other three are counted from the tree on every run. They used to be
hardcoded here too, and `gamecode` sat at 0% and `gamecode/logic` at 4% long
after both had real verified bodies committed. A progress script that has to be
edited by hand to reflect progress is a progress script that will be wrong, and
this one is specifically the place this project put its "suspect the counter"
rule.

## What is deliberately excluded

  lime_unimplemented.c  -- bodies that only call abort(). They exist so the
                           linker is satisfied; counting them reports work that
                           has not been done. This script counted them once and
                           jumped from 88 to 93 with nothing written, which is
                           the failure the ENCARGO warns about.
  lime_globals.c        -- data, not functions.

Reads work/func-to-file.txt, or UMK3_FUNC_TO_FILE if that is set.
"""
import collections
import io
import os
import sys
import re

# tools/decomp/progress.py -> the repository: three levels up. Four (from
# before the move into tools/decomp/) landed above the repository and counted
# every module 0%.
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
os.chdir(ROOT)

EXCLUDE = {"lime_unimplemented.c", "lime_globals.c"}

FUNC_TO_FILE = (os.environ.get("UMK3_FUNC_TO_FILE")
                or os.path.join("work", "func-to-file.txt"))

# The five blocks that are not function-counted, as the README orders them.
# These are estimates a person maintains; the three measured ones are inserted
# between them by main().
JUDGEMENT = {
    "analysis": 100.0,      # binary analysis and source-tree mapping
    "tooling": 100.0,       # tooling and the verification oracle
    "formats": 100.0,       # asset format specifications
    "platform": 90.0,       # native PC platform layer, recounted 2026-10-15: all 49 lime.m C entry points (43 real, 6 nothing to do on a PC) and the ~112 Objective-C roles replaced by the Win32 backend; left: SDL2 parity (pads, launcher), in-game ad windows
    "sdkstubs": 100.0,      # EA SDK boundary: 27 fn + LocaleManager, runtime/gamecode_stubs.c
}


def plain_name(sym):
    """Recover the C identifier from a symbol, mangled or not."""
    s = sym.lstrip("_")
    m = re.match(r"Z(\d+)(.*)", s)
    if m:                                   # C++: the length prefix is exact
        return m.group(2)[:int(m.group(1))]
    return s                                # C: already plain


def defined_functions(root):
    """Every function with a real body under `root`, at any depth."""
    names = set()
    if not os.path.isdir(root):
        return names
    for dirpath, _dirs, files in os.walk(root):
        for fn in sorted(files):
            if not fn.endswith(".c") or fn in EXCLUDE:
                continue
            text = io.open(os.path.join(dirpath, fn),
                           encoding="utf-8", errors="ignore").read()
            # `[^;{}]` and not `[^;]`: that class spans newlines, so a
            # greedy run starting on some earlier line could reach past a
            # whole function body and swallow the NEXT definition inside
            # its own match, which findall then never reports. Barring
            # braces stops it at the first body it meets. This silently
            # lost FE_Task_VS_Screen_Destroy and understated the one
            # number this tool exists to publish.
            for n in re.findall(
                    r'^[A-Za-z_][\w \*]*?\**\s*(\w+)\s*\([^;{}]*\)\s*\{',
                    text, re.M):
                # Both forms. `plain_name` strips leading underscores off the
                # SYMBOL, so a C definition that keeps them -- as
                # `__GLOBAL__I_InGameLevelSelect` must, because that is its real
                # name -- would never match and the function would be counted as
                # outstanding forever. One function, silently undercounted, in a
                # tool whose whole job is not to be silently wrong.
                names.add(n)
                names.add(n.lstrip("_"))
    return names


# `gamecode/limeMPGameSpecific.m` is the multiplayer layer, and CLAUDE.md puts
# it in the STUB bucket rather than the decompile one -- along with
# limeMPSession and modalAlert. Counting its 48 functions as work outstanding
# would report a debt this project has decided not to pay. 339 - 48 = 291,
# which is the figure the README has always quoted.
NOT_DECOMPILED = ("limeMPGameSpecific",)


def rows_for(marker, exclude=None):
    """(name, source file) for every function whose translation unit matches."""
    out = []
    for line in io.open(FUNC_TO_FILE, encoding="utf-8", errors="ignore"):
        p = line.split()
        if len(p) < 3 or not p[0].startswith("0x"):
            continue
        src = p[2].replace("\\", "/")
        if marker not in src:
            continue
        if exclude and exclude in src:
            continue
        if any(skip in src for skip in NOT_DECOMPILED):
            continue
        out.append((plain_name(p[1]), src.rsplit("/", 1)[-1]))
    return out


def tally(rows, defined):
    """(done, total, per-file breakdown)."""
    by = collections.defaultdict(lambda: [0, 0])
    for name, src in rows:
        by[src][0] += 1
        if name in defined:
            by[src][1] += 1
    done = sum(k for _n, k in by.values())
    total = sum(n for n, _k in by.values())
    return done, total, by


def bar(pct, width=10):
    filled = int(round(pct / 100.0 * width))
    return "#" * filled + "." * (width - filled)



# ---------------------------------------------------------------- publishing

DOCS = ("README.md", "README.es.md", "docs/PROGRESS.md", "docs/HANDOFF.md")

# The three rows this script counts. The row is found by the backticked path at
# its head, because the two READMEs word the label differently.
COUNTED = ("lime/common", "gamecode", "gamecode/logic")

# The two block characters the bars are drawn from. `bar()` above is the
# terminal's, in ASCII; the documents use these, so the published bars are
# drawn separately rather than by reusing it.
FULL, LIGHT = "\u2588", "\u2591"


def docbar(pct, width=40):
    """A bar in block characters, as the documents draw it."""
    filled = int(round(pct / 100.0 * width))
    return FULL * filled + LIGHT * (width - filled)


def es(text):
    """A figure in Spanish: decimal comma, thousands dot."""
    return text.replace(",", "\x00").replace(".", ",").replace("\x00", ".")


def publish(overall, counted):
    """Rewrite the bar, the counted rows and the headline in the four docs.

    Line by line, deliberately. A table row matched with a spanning pattern
    once ran from one row to a pipe three thousand lines further down and took
    the document with it; nothing here looks past the end of a line.
    """
    root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    touched = []

    bar_line = re.compile("^[" + FULL + LIGHT + r"]{20,}[ 	]+[0-9]+[.,][0-9]+%\s*$")
    headline = re.compile(r"^\*\*[0-9]+[.,][0-9]+% (of the total estimated"
                          r" effort|del esfuerzo total estimado)")

    for name in DOCS:
        path = os.path.join(root, name)
        if not os.path.exists(path):
            continue

        spanish = name.endswith(".es.md")
        figure = "%.2f" % overall
        shown = es(figure) if spanish else figure

        out, changed = [], False
        for line in io.open(path, encoding="utf-8").read().split("\n"):
            new = line

            if bar_line.match(line):
                new = "%s  %s%%" % (docbar(overall, 40), shown)

            elif headline.match(line):
                new = headline.sub(
                    lambda m: "**%s%% %s" % (shown, m.group(1)), line)

            elif line.startswith("| `"):
                # `| label | weight | done | bar |` -- five pieces once split,
                # the first and last empty.
                cells = line.split("|")
                if len(cells) == 6:
                    key = cells[1].strip().split("`")
                    if len(key) > 1 and key[1] in counted:
                        done, total, pct = counted[key[1]]
                        if pct >= 100.0:
                            cell = "**100%**"
                        else:
                            f = "%.2f" % pct
                            cell = "%s%% (%d)" % (es(f) if spanish else f, done)
                        cells[3] = " %s " % cell
                        cells[4] = " `%s` " % docbar(pct, 10)
                        new = "|".join(cells)

            changed = changed or new != line
            out.append(new)

        if changed:
            io.open(path, "w", encoding="utf-8",
                    newline="").write("\n".join(out))
            touched.append(name)

    return touched


def main():
    if not os.path.isfile(FUNC_TO_FILE):
        raise SystemExit("func-to-file.txt not found: %s\n"
                         "Set UMK3_FUNC_TO_FILE or put a copy in work/."
                         % FUNC_TO_FILE)

    lime_def = defined_functions(os.path.join("src", "decomp", "lime"))
    game_def = defined_functions(os.path.join("src", "decomp", "gamecode"))

    modules = [
        ("lime/common", tally(rows_for("/lime/common/"), lime_def)),
        ("gamecode", tally(rows_for("/gamecode/", "/gamecode/logic/"), game_def)),
        ("gamecode/logic", tally(rows_for("/gamecode/logic/"), game_def)),
    ]

    pcts = {}
    counted = {}
    for title, (done, total, by) in modules:
        print("\n  %s" % title)
        for src in sorted(by):
            n, k = by[src]
            # Every file, including the ones with nothing written yet. Skipping
            # the zeroes made the rows sum to less than the TOTAL underneath
            # them -- gamecode/logic showed 2,092 across its files against a
            # total of 2,172 -- and hid seven source files that had never been
            # started. A progress table that cannot be added up is worse than
            # no table.
            print("    %-26s %3d/%-5d" % (src, k, n))
        pct = 100.0 * done / total if total else 0.0
        pcts[title] = pct
        counted[title] = (done, total, pct)
        print("    %-26s %3d/%-5d = %.2f%%  %s"
              % ("TOTAL", done, total, pct, bar(pct)))

    weights = [
        (4, JUDGEMENT["analysis"]),
        (8, JUDGEMENT["tooling"]),
        (8, JUDGEMENT["formats"]),
        (12, pcts["lime/common"]),
        (18, pcts["gamecode"]),
        (28, pcts["gamecode/logic"]),
        (17, JUDGEMENT["platform"]),
        (5, JUDGEMENT["sdkstubs"]),
    ]
    overall = sum(w * v / 100.0 for w, v in weights)
    print("\n  overall: %.2f%%  %s" % (overall, bar(overall, 40)))
    print("  (three rows measured from the tree, five maintained by hand --"
          " see the docstring)")

    if "--write" in sys.argv:
        touched = publish(overall, counted)
        print("\n  wrote: %s" % (", ".join(touched) if touched
                                  else "nothing changed"))


if __name__ == "__main__":
    main()
