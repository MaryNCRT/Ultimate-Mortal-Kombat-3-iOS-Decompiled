#!/usr/bin/env python3
"""moves.py -- extract the move-input tables from the armv7 slice.

The in-game Moves Info screen does not build its input sequences at runtime.
`MovesList` (armv7 `0x0001ed4c`) hands `DrawMoveListIcons` a pointer straight
into a `__DATA` table, one per character per button layout:

    _Kano_Moves5    the five-button layout
    _Kano_Moves6    the six-button layout

Every table is an array of **64-byte rows, sixteen `int32` each, terminated by
-1**. So the whole roster's move notation is static data with symbols on it, and
this script just reads it out. No emulator capture is needed -- see issue #5,
which was written before that was known.

    python tools/moves.py work/UMK3.armv7             # every table
    python tools/moves.py work/UMK3.armv7 Kano        # one character
    python tools/moves.py work/UMK3.armv7 --json      # machine-readable

## The alphabet

Values are what `DrawMoveListIcons` draws, and it splits them in two:

    0 .. 15    a cell of the 4x4 MOVES_ICONS.PNG atlas -- sixteen values,
               sixteen cells, one each
    16 .. 22   TEXT -- four translated words and "(", ") ", "/"
    > 22       falls back to cell 0

so **16 and up are punctuation, not inputs**. A decoder has to cut the alphabet
at 15. See docs/MOVES-TABLES.md.
"""

import json
import struct
import sys

VM_BIAS = 0x1000
ROW_BYTES = 64
ROW_INTS = 16

# Value -> what DrawMoveListIcons draws for it. The icons are named by their
# atlas cell because naming them by meaning needs MOVES_ICONS.PNG, which is the
# user's asset and is not in this repo.
ALPHABET = {
    16: "text(0x3a8)", 17: "text(0x3a9)",
    18: "(",           19: ") ",           20: "/",
    21: "text(0x3aa)", 22: "text(0x3f9)",
}

# value -> (u, v) on the 4x4 atlas, straight out of DrawMoveListIcons.
ICON_CELL = {
    0:  (0.00, 0.00), 1:  (0.25, 0.00), 2:  (0.50, 0.00), 3:  (0.75, 0.00),
    4:  (0.00, 0.25), 5:  (0.25, 0.25), 9:  (0.50, 0.25), 8:  (0.75, 0.25),
    13: (0.00, 0.50), 7:  (0.25, 0.50), 6:  (0.50, 0.50), 14: (0.75, 0.50),
    12: (0.00, 0.75), 11: (0.25, 0.75), 10: (0.50, 0.75), 15: (0.75, 0.75),
}


def load_symbols(path="work/symbols.txt"):
    """name -> address, for the SECT (real) entries only."""
    syms = {}
    try:
        fh = open(path, encoding="utf-8", errors="replace")
    except OSError:
        return syms
    with fh:
        for line in fh:
            parts = line.split()
            if len(parts) < 5 or not parts[0].startswith("0x"):
                continue
            if "SECT" not in line:
                continue
            try:
                addr = int(parts[0], 16)
            except ValueError:
                continue
            if addr and parts[-1].startswith("_"):
                syms.setdefault(parts[-1], addr)
    return syms


def move_tables(syms):
    """[(name, start, end)] for every _*_Moves5 / _*_Moves6, in address order."""
    found = [(a, n) for n, a in syms.items()
             if n.endswith("_Moves5") or n.endswith("_Moves6")]
    found.sort()

    # Each table runs to the next symbol of ANY kind, so the end has to come
    # from the full symbol list rather than from the move symbols alone.
    everything = sorted(set(syms.values()))
    out = []
    for addr, name in found:
        i = everything.index(addr)
        end = everything[i + 1] if i + 1 < len(everything) else addr + ROW_BYTES
        out.append((name[1:], addr, end))
    return out


# _MovesListTab, 0x0010d918: THIRTEEN words per character, indexed by the
# character number (`r5 * 13` in MovesList). Three sections:
#
#     [0]  rows in section 1     [5]  rows in section 2     [10] rows in section 3
#     [1]  GameText id base A    [6]  id base A             [11] Moves5
#     [2]  GameText id base B    [7]  id base B             [12] Moves6
#     [3]  Moves5                [8]  Moves5
#     [4]  Moves6               [9]  Moves6
#
# Section 3 carries no id bases. The three counts sum to the table's row count,
# which is what `--check` verifies against the symbol gaps.
MOVESLISTTAB = 0x0010D918
TAB_WORDS = 13

# The order is the roster order; see docs/ROSTER.md. Generic is not in the tab.
TAB_ORDER = [
    "Kano", "Sonya", "Jax", "NightWolf", "SZ", "Stryker", "Sindel", "Sektor",
    "Cyrax", "KL", "Kabal", "Sheeva", "ST", "LK", "Smoke", "Kitana", "Jade",
    "Mileena", "Scorpion", "Reptile", "Ermac", "Classic_SZ", "Humansmoke",
]


def read_tab(data):
    """character name -> [(rows, idA, idB), (rows, idA, idB), (rows, None, None)]"""
    out = {}
    for i, name in enumerate(TAB_ORDER):
        w = struct.unpack_from("<%dI" % TAB_WORDS, data,
                               MOVESLISTTAB + i * TAB_WORDS * 4 - VM_BIAS)
        out[name] = [(w[0], w[1], w[2]), (w[5], w[6], w[7]), (w[10], None, None)]
    return out


def read_rows(data, start, end):
    rows = []
    for off in range(start, end, ROW_BYTES):
        words = struct.unpack_from("<%di" % ROW_INTS, data, off - VM_BIAS)
        try:
            n = words.index(-1)
        except ValueError:
            n = ROW_INTS
        rows.append(list(words[:n]))
    return rows


def describe(value):
    if value in ALPHABET:
        return ALPHABET[value]
    if value in ICON_CELL:
        return "icon%d" % value
    return "?%d(->icon0)" % value


def main(argv):
    if len(argv) < 2:
        sys.exit(__doc__.strip().splitlines()[0]
                 + "\n\nusage: moves.py <binary> [character] [--json]")

    data = open(argv[1], "rb").read()
    want = next((a for a in argv[2:] if not a.startswith("-")), None)
    as_json = "--json" in argv

    tables = move_tables(load_symbols())
    if not tables:
        sys.exit("no _*_Moves5 / _*_Moves6 symbols found -- is work/symbols.txt "
                 "present and does it match this binary?")

    result = {}
    for name, start, end in tables:
        if want and want.lower() not in name.lower():
            continue
        result[name] = read_rows(data, start, end)

    if as_json:
        json.dump(result, sys.stdout, indent=1)
        print()
        return

    tab = read_tab(data)

    for name, rows in result.items():
        base = name.rsplit("_Moves", 1)[0]
        sections = tab.get(base)

        print("=== %s  (%d rows)" % (name, len(rows)))

        if sections is None:
            for i, row in enumerate(rows):
                print("  %2d  %-34s %s"
                      % (i, " ".join(str(v) for v in row),
                         " ".join(describe(v) for v in row)))
            print()
            continue

        i = 0
        for s_no, (count, id_a, id_b) in enumerate(sections, 1):
            if count == 0:
                continue
            if id_a is None:
                print("  -- section %d, %d rows (no text ids)" % (s_no, count))
            else:
                print("  -- section %d, %d rows, GameText 0x%x.. and 0x%x.."
                      % (s_no, count, id_a, id_b))
            for k in range(count):
                if i >= len(rows):
                    print("     (table ran out)")
                    break
                row = rows[i]
                label = ("0x%x" % (id_a + k)) if id_a is not None else "-"
                print("  %2d  %-8s %-30s %s"
                      % (i, label, " ".join(str(v) for v in row),
                         " ".join(describe(v) for v in row)))
                i += 1
        if i != len(rows):
            print("  !! %d rows in the table, %d accounted for by MovesListTab"
                  % (len(rows), i))
        print()


if __name__ == "__main__":
    main(sys.argv)
