"""
check_finisher_ranges.py -- the FATALITY TRAINER's ranges checked one by one
against the predicates' own constants.

The trainer measures a finisher's range by asking the game's predicate at
every distance (src/runtime/fatal_trainer.c). This puts that next to what the
decompiled predicate says: the band it stores in 0x30 / 0x34 before
q_fatal_dist or q_animal_dist (min <= d <= max, moves.c), or the one bound it
tests itself (q_close_friend: d <= 0x70 ...).

    UMK3_FT="scorpion,kano,5,0" UMK3_FT_VERIFY=1 UMK3_SHOT=500 umk3-game64.exe > v.log
    python tools/check_finisher_ranges.py v.log <build/logic_tables.c>

A line per finisher: fighter, kind, table[entry], predicate, the range the
game accepted, what the code says, and OK / CHECK. "-" where the game said
no at every distance in that window (a mercy first, the stage, a block).
"""

import re
import sys
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
KINDS = ["FATALITY 1", "FATALITY 2", "STAGE", "ANIMALITY", "MERCY", "FRIENDSHIP", "BABALITY"]
NAMES = ["KANO", "SONYA", "JAX", "NIGHTWOLF", "SUBZERO", "STRYKER", "SINDEL", "SEKTOR",
         "CYRAX", "KUNGLAO", "KABAL", "SHEEVA", "SHANGTSUNG", "LIUKANG", "SMOKE", "KITANA",
         "JADE", "MILEENA", "SCORPION", "REPTILE", "ERMAC", "OLDSUBZERO", "OLDSMOKE", "NOOB"]


def tables(path):
    """table name -> [predicate name per entry]"""
    out = {}
    text = open(path, encoding="utf-8", errors="replace").read()
    for m in re.finditer(r"uintptr_t (sm_[a-z0-9_]+)\[\d+\][^=]*= \{(.*?)\n\};", text, re.S):
        preds = []
        lines = [ln for ln in m.group(2).split("\n") if ln.strip()]
        # 18 words an entry, written six to a line: its first line every third
        for ln in lines[0::3]:
            w = [x.strip() for x in ln.split(",") if x.strip()]
            if len(w) < 6:
                break                   # the terminator
            r = re.match(r"UMK3_REL\((\w+)", w[3])
            preds.append(r.group(1) if r else None)
        out[m.group(1)] = preds
    return out


def bodies():
    """predicate -> its body, from the decompiled logic"""
    out = {}
    d = os.path.join(ROOT, "src", "decomp", "gamecode", "logic")
    for fn in os.listdir(d):
        if fn.endswith(".c"):
            t = open(os.path.join(d, fn), encoding="utf-8", errors="replace").read()
            for m in re.finditer(r"^(?:static\s+)?void\s+(q_[a-z0-9_]+)\(MK3OBJ \*obj\)\n\{(.*?)\n\}", t,
                                 re.S | re.M):
                out.setdefault(m.group(1), m.group(2))
    return out


def num(s):
    s = s.strip().rstrip("u").rstrip("U")
    if "+" in s:
        return sum(num(x) for x in s.split("+"))
    return int(s, 0)


def says(pred, b, depth=0):
    """what the code says the range is: (lo, hi), hi None = no bound"""
    if pred is None:
        return (0, None, "none")
    body = b.get(pred)
    if body is None:
        return None
    lo = re.search(r"field30 = ([0-9xa-fA-F+ ]+);", body)
    hi = re.search(r"field34 = ([0-9xa-fA-F+ ]+);", body)
    if lo and hi and ("q_fatal_dist" in body or "q_animal_dist" in body):
        return (num(lo.group(1)), num(hi.group(1)), "band")
    m = re.search(r"field28 > (0x[0-9a-f]+|\d+)\)\s*\n\s*q_no", body)
    if m:
        return (0, num(m.group(1)), "max")
    m = re.search(r"field28 <= (0x[0-9a-f]+|\d+)\)\s*\n\s*q_no", body)
    if m:
        return (num(m.group(1)) + 1, None, "min")
    if depth < 3:
        for callee in re.findall(r"\b(q_[a-z0-9_]+)\(obj\)", body):
            if callee in ("q_no", "q_yes", "q_fatality_req", "q_mercy", "q_friend"):
                continue
            r = says(callee, b, depth + 1)
            if r and r[2] != "none":
                return r
    return (0, None, "none")


def main():
    log, lt = sys.argv[1], sys.argv[2]
    t = tables(lt)
    b = bodies()
    bad = 0
    print("%-11s %-10s %-16s %-22s %-11s %-11s %s" % ("FIGHTER", "KIND", "ENTRY", "PREDICATE",
                                                       "GAME", "CODE", ""))
    for line in open(log, encoding="utf-8", errors="replace"):
        m = re.match(r"verify: (\d+) (\d+) (\S+) (-?\d+) range (-?\d+) (-?\d+) bound (-?\d+) (-?\d+)", line)
        if not m:
            continue
        ch, kind, tab, idx, lo, hi = (int(m.group(1)), int(m.group(2)), m.group(3), int(m.group(4)),
                                      int(m.group(5)), int(m.group(6)))
        pred = t.get(tab, [None] * 99)[idx] if idx >= 0 and tab in t else None
        c = says(pred, b)
        game = "-" if lo < 0 else ("any" if lo == 0 and hi >= 480 else
                                   "%d+" % lo if hi >= 480 else "%d-%d" % (lo, hi))
        if c is None:
            code, verdict = "?", "CHECK"
        else:
            clo, chi, how = c
            code = ("any" if how == "none" else "%d+" % clo if chi is None else "%d-%d" % (clo, chi))
            if lo < 0:
                verdict = "(not asked)"     # refused at every distance in this window
            elif how == "none":
                verdict = "OK" if lo == 0 and hi >= 480 else "CHECK"
            else:
                verdict = "OK" if lo == clo and (hi >= 480 if chi is None else hi == chi) else "CHECK"
        bad += verdict == "CHECK"
        print("%-11s %-10s %-16s %-22s %-11s %-11s %s" % (NAMES[ch] if ch < len(NAMES) else ch,
                                                          KINDS[kind], "%s[%d]" % (tab, idx),
                                                          pred or "-", game, code, verdict))
    print("\n%d to check" % bad)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
