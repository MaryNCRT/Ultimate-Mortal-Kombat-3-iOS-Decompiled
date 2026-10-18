"""
Extract the game's finisher catalogue: what fires, on whom, and at which frame.

This needs no reverse engineering left over -- it is the payoff of two formats
that are already solved. An `.events` track carries a **name**, a **slot**, and
entries whose first int32 is a **frame index into that character's own
`.skinanim`** (see docs/EVENTS-FORMAT.md). Put those together and the fatality,
babality and death catalogue reads straight out of the data.

Animalities do not appear here. They are morph sequences in the `.meshset`
instead -- `SmokeBull`, `JadeCat` -- because a skeleton cannot turn a man into
a bull. `--morphs` lists those.

Usage:
  python finishers.py <res dir>                 the catalogue
  python finishers.py <res dir> --character KANO
  python finishers.py <res dir> --morphs        the morph-target sequences
  python finishers.py <res dir> --csv out.csv
"""

import collections
import csv
import glob
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import events                                          # noqa: E402
import meshset                                         # noqa: E402
import skin                                            # noqa: E402


DEATH = ("DEATH", "DECAP", "FATAL", "RIP", "SLICE", "DICE", "SQUASH",
         "CUTUP", "SHATTER", "EXPLO", "BURN", "HEAD", "BLOOD", "KOD")


def classify(slot):
    u = slot.upper()
    if "BABALIT" in u:
        return "babality"
    if "ANIMALIT" in u:
        return "animality"
    if "FRIEND" in u:
        return "friendship"
    if any(w in u for w in DEATH):
        return "fatality/death"
    if any(w in u for w in ("VICTORY", "WIN")):
        return "victory"
    return "effect"


def catalogue(res_dir):
    """Every assigned event slot, with its frame index where a stream exists."""
    rows = []
    for path in sorted(glob.glob(os.path.join(res_dir, "*.events"))):
        stem = os.path.basename(path)[:-7]
        try:
            ev = events.load(path)
        except Exception:                               # noqa: BLE001
            continue

        # frame indices only mean something when the file has its own stream
        anim = os.path.join(res_dir, stem + ".skinanim")
        nframes = None
        if os.path.exists(anim):
            try:
                data = open(anim, "rb").read()
                _s, nf, _fs, _nb, _e, hdr = skin.parse_anim(data)
                # SINDEL's file holds two takes; the header count is the real
                # one -- established from the event indices themselves.
                nframes = nf
            except Exception:                           # noqa: BLE001
                pass

        for t in ev.tracks:
            slot = t.slot.strip()
            if not slot or slot == "UNASSIGNED":
                continue
            for i in range(t.num_entries):
                frame = t.entry(i)[0]
                rows.append({
                    "source": stem,
                    "slot": slot,
                    "kind": classify(slot),
                    "spawns": t.name.strip(),
                    "frame": frame,
                    "of": nframes if nframes else "",
                    "at_pct": ("%.0f%%" % (100.0 * frame / nframes)) if nframes else "",
                })
    return rows


def morph_sequences(res_dir, minimum=4):
    """Numbered mesh runs sharing a stem -- the shape-key animations."""
    out = []
    for path in sorted(glob.glob(os.path.join(res_dir, "*.meshset"))):
        stem = os.path.basename(path)[:-8]
        try:
            ms, _end = meshset.parse(open(path, "rb").read())
        except Exception:                               # noqa: BLE001
            continue
        seq = collections.defaultdict(list)
        for m in ms:
            mm = re.match(r"^(.*?)(\d+)$", m.name)
            if mm:
                seq[mm.group(1).strip()].append((int(mm.group(2)),
                                                 len(m.verts), m.num_faces))
        for name, items in seq.items():
            if len(items) < minimum:
                continue
            items.sort()
            identical = (len({i[1] for i in items}) == 1
                         and len({i[2] for i in items}) == 1)
            out.append((stem, name, len(items), items[0][1], identical))
    return out


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 1
    res = argv[1]
    want = None
    for i, a in enumerate(argv):
        if a == "--character" and i + 1 < len(argv):
            want = argv[i + 1].upper()

    if "--morphs" in argv:
        rows = morph_sequences(res)
        rows.sort(key=lambda r: -r[2])
        print("%-26s %-24s %5s %7s  %s"
              % ("file", "sequence", "count", "verts", "topology"))
        for stem, name, n, verts, ident in rows:
            if want and want not in stem.upper():
                continue
            print("%-26s %-24s %5d %7d  %s"
                  % (stem, name, n, verts,
                     "identical -> morph target" if ident else "varies"))
        print("\n%d sequences; %d with identical topology"
              % (len(rows), sum(1 for r in rows if r[4])))
        return 0

    rows = catalogue(res)
    if want:
        rows = [r for r in rows if want in r["source"].upper()
                or want in r["slot"].upper()]

    for out in [a for i, a in enumerate(argv)
                if i and argv[i - 1] == "--csv"]:
        with open(out, "w", newline="", encoding="utf-8") as fh:
            w = csv.DictWriter(fh, fieldnames=list(rows[0].keys()))
            w.writeheader()
            w.writerows(rows)
        print("wrote %s (%d rows)" % (out, len(rows)))
        return 0

    by_kind = collections.Counter(r["kind"] for r in rows)
    print("%d catalogued events\n" % len(rows))
    for kind, n in by_kind.most_common():
        print("  %-16s %4d" % (kind, n))

    print("\n%-22s %-30s %-22s %7s %6s"
          % ("source", "slot", "spawns", "frame", "at"))
    for r in sorted(rows, key=lambda r: (r["kind"], r["source"])):
        if r["kind"] in ("effect", "victory"):
            continue
        print("%-22s %-30s %-22s %7s %6s"
              % (r["source"][:22], r["slot"][:30], r["spawns"][:22],
                 r["frame"], r["at_pct"]))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
