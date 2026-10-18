"""
Play a character's animation: render a range of frames to an animated GIF.

The last piece of the asset work, and the one that proves the rest. Everything
it needs was established separately:

    .bones      the skeleton                       docs/SKIN-FORMAT.md
    .skinanim   one quaternion per bone per frame  docs/SKIN-FORMAT.md
    .skin       influences, topology, UVs          docs/SKIN-FORMAT.md
    framelists  the name of every frame            docs/FRAMELISTS.md
    .events     what fires, and when               docs/EVENTS-FORMAT.md

A `.skinanim` is one long stream holding every animation the character has, so
"play frames 70-90" only means something because `res/framelists/` names each
frame. Clips are found by grouping consecutive frames whose names share a stem:
`KNWALK1`, `KNWALK2`, `KNWALK3` is a clip.

Usage:
  python animate.py <CHARACTER> <out.gif> [--clip WALK] [--from 70 --to 90]
  python animate.py <CHARACTER> --list           name every clip it can find
"""

import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pose                                            # noqa: E402
import meshview                                        # noqa: E402
import events as events_mod                            # noqa: E402

try:
    from PIL import Image
except ImportError:
    raise SystemExit("Pillow is required: pip install Pillow")

RES = os.environ.get("UMK3_RES", "res")


def frame_names(res_dir, character):
    """The character's frame list, one name per animation frame.

    Line N+1 is frame N -- established by matching .events frame indices
    against the list; see docs/FRAMELISTS.md.
    """
    short = character.replace("_STANDARD", "").lower()
    path = os.path.join(res_dir, "framelists", short + "frames.txt")
    if not os.path.exists(path):
        return []
    out = []
    for line in open(path, encoding="utf-8", errors="ignore"):
        line = line.strip()
        name = line.split("//")[0].strip()
        flags = line.split("//")[1].strip() if "//" in line else ""
        out.append((name, flags))
    return out


def find_clips(names, minimum=2):
    """Group consecutive frames whose names share a stem and count up.

    `KNWALK1, KNWALK2, KNWALK3` is a clip; a lone `SPINNER2` between two
    unrelated names is not. Padding entries -- literally `x`, `x2`, `xxx` --
    are skipped.
    """
    clips = []
    cur_stem, start = None, 0
    for i, (name, _flags) in enumerate(names + [("", "")]):
        m = re.match(r"^(.*?)(\d+)$", name)
        stem = m.group(1) if m else None
        if stem is not None and re.fullmatch(r"x+", stem, re.I):
            stem = None                                 # padding
        if stem != cur_stem:
            if cur_stem and i - start >= minimum:
                clips.append((cur_stem, start, i - 1))
            cur_stem, start = stem, i
    return clips


def events_by_frame(res_dir, character):
    """frame index -> list of (slot, spawned thing)."""
    path = os.path.join(res_dir, character + ".events")
    hits = {}
    if not os.path.exists(path):
        return hits
    try:
        ev = events_mod.load(path)
    except Exception:                                   # noqa: BLE001
        return hits
    for t in ev.tracks:
        slot = t.slot.strip()
        for i in range(t.num_entries):
            f = t.entry(i)[0]
            hits.setdefault(f, []).append((slot, t.name.strip()))
    return hits


def render_range(res_dir, character, lo, hi, size=340, yaw=0.26, step=1):
    num, offsets, children, root = pose.load_bones(
        os.path.join(res_dir, character + ".bones"))
    anim = os.path.join(res_dir, character + ".skinanim")
    skin_path = os.path.join(res_dir, character + ".skin")
    texture = "%s_DIFFUSE.pvr" % character.split("_")[0]

    frames = []
    for f in range(lo, hi + 1, step):
        root_pos, quats, _n = pose.load_frame(anim, f)
        palette = pose.build_palette(num, offsets, children, root,
                                     root_pos, quats)
        pts = pose.skin_vertices(skin_path, palette, num)
        mesh = pose.build_mesh(skin_path, pts, texture)
        img, _st = meshview.render([mesh], res_dir, size=size, yaw=yaw,
                                   pitch=0.0, fit=0.80)
        frames.append(Image.fromarray(img))
    return frames


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 1
    character = argv[1]
    if not character.endswith("_STANDARD"):
        character += "_STANDARD"

    names = frame_names(RES, character)
    if not names:
        print("no frame list for %s" % character)
        return 1
    clips = find_clips(names)

    if "--list" in argv:
        fired = events_by_frame(RES, character)
        print("%s: %d frames, %d clips\n" % (character, len(names), len(clips)))
        print("%-26s %7s %7s %6s  %s" % ("clip", "from", "to", "len", "notes"))
        for stem, a, b in clips:
            flags = {names[i][1] for i in range(a, b + 1) if names[i][1]}
            ev = [s for i in range(a, b + 1) for s, _ in fired.get(i, [])]
            note = " ".join(sorted(flags))
            if ev:
                note += "  events: " + ", ".join(sorted(set(ev))[:2])
            print("%-26s %7d %7d %6d  %s" % (stem, a, b, b - a + 1, note))
        return 0

    lo = hi = None
    for i, a in enumerate(argv):
        if a == "--from" and i + 1 < len(argv):
            lo = int(argv[i + 1])
        elif a == "--to" and i + 1 < len(argv):
            hi = int(argv[i + 1])
        elif a == "--clip" and i + 1 < len(argv):
            want = argv[i + 1].upper()
            match = [c for c in clips if want in c[0].upper()]
            if not match:
                print("no clip matching %r -- try --list" % want)
                return 1
            _stem, lo, hi = max(match, key=lambda c: c[2] - c[1])

    if lo is None:
        # default: the stance, centred on the medoid frame
        mid = pose.find_idle_frame(os.path.join(RES, character + ".skinanim"),
                                   *pose.load_bones(
                                       os.path.join(RES, character + ".bones")))
        lo, hi = max(0, mid - 4), mid + 4

    out = [a for a in argv[2:] if a.endswith(".gif")]
    out = out[0] if out else "anim.gif"
    print("%s: frames %d-%d  (%s ... %s)"
          % (character, lo, hi, names[lo][0], names[min(hi, len(names) - 1)][0]))
    frames = render_range(RES, character, lo, hi)
    frames[0].save(out, save_all=True, append_images=frames[1:],
                   duration=90, loop=0, optimize=True)
    print("  -> %s   %d frames" % (out, len(frames)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
