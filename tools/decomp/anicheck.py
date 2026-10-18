#!/usr/bin/env python3
"""anicheck.py -- does the Godot port play the frames the binary plays?

`factdiff.py` checks the decompiled C against the instructions. This checks the
other half: the animation TABLES the port ships against the streams they were
taken from. Same discipline, different pair.

A fighter's animation is not a clip. `_nj_ani_data` (0x0015978c) holds 92 stream
pointers, and a stream is a word list split into PARTS by zero terminators: part
one is the swing, part two is usually the return, and the procs walk the rest
with a cursor. So a check has to compare parts, not ranges.

What it compares, per animation the port uses:

    the swing      ANI[id] in umk3_scorpion_ani.gd against part 1
    the return     ANI_TAIL[id] in umk3_fight.gd against part 2
    the punches    PUNCH_PART in umk3_fight.gd against parts 1..7 of 14 and 15

The punches get their own table because they are the only ones that re-seat the
cursor instead of carrying on: `t_joy_un_hi_punch1` walks one zero and
`t_unhip1` two more, landing on part 4, while its twin lands on part 5. Getting
that wrong is what made the port play a second jab where a retraction belongs,
and it is the reason this file exists.

A mismatch here is the same kind of question a factdiff mismatch is: read the
stream before changing the table.

Usage:
    python anicheck.py
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import aniparts                                              # noqa: E402

GODOT = "E:/MK3 GODOT/nuevo-proyecto-de-juego/umk3"
ANI_GD = os.path.join(GODOT, "umk3_scorpion_ani.gd")
FIGHT_GD = os.path.join(GODOT, "umk3_fight.gd")

# ops that carry an operand word, so the word after them is not a frame
OPS_WITH_ARG = (3, 4, 6, 8, 17)


def frames_of(part):
    """The frames a part actually DISPLAYS, jumps followed.

    A jump is not a skip. Op 1 takes the next word as an address and the
    interpreter carries on THERE, so the frames after a jump inside the part
    are never reached, and the frames at the destination are. The four
    cross-over parts of the two punches all end in one, and reading them as
    skips made every one of them look a frame short.
    """
    out, j = [], 0
    while j < len(part):
        _, w = part[j]
        if w == 1 and j + 1 < len(part):
            out.extend(follow(part[j + 1][1]))
            return out                    # control left; nothing after is read
        elif w > 0x12:
            s = aniparts.sc(w)
            if 0 <= s < 60000:            # 0xffff means "this character lacks it"
                out.append(s)
            j += 1
        else:
            j += 2 if w in OPS_WITH_ARG else 1
    return out


def follow(target):
    """Read frames from a jump target up to the zero that ends its part."""
    out = []
    va = target
    for _ in range(64):
        w = aniparts.struct.unpack_from("<I", aniparts.d, va - 0x1000)[0]
        if w == 0:
            break
        if w > 0x12:
            sc = aniparts.sc(w)
            if 0 <= sc < 60000:
                out.append(sc)
            va += 4
        elif w == 1:
            va = aniparts.struct.unpack_from("<I", aniparts.d,
                                             va + 4 - 0x1000)[0]
        else:
            va += 8 if w in OPS_WITH_ARG else 4
    return out


def read_ani_table():
    """id -> [frames] out of the generated animation table."""
    rows = {}
    text = open(ANI_GD, encoding="utf-8", errors="replace").read()
    for m in re.finditer(r'\["([A-Z0-9]+)",\s*(?:true|false),\s*'
                         r'\[([0-9,\s]*)\]\],\s*#\s*(\d+)', text):
        name, frames, idx = m.groups()
        rows[int(idx)] = (name, [int(x) for x in frames.split(",") if x.strip()])
    return rows


def read_block(text, const):
    """The text between `const NAME := {` and its closing brace."""
    i = text.find("const %s := {" % const)
    if i < 0:
        return ""
    j = text.find("\n}", i)
    return text[i:j]


def read_tails():
    """id -> [frames] out of ANI_TAIL."""
    text = open(FIGHT_GD, encoding="utf-8", errors="replace").read()
    blk = read_block(text, "ANI_TAIL")
    out = {}
    for m in re.finditer(r"^\s*(\d+):\s*\[([0-9,\s]*)\]", blk, re.M):
        out[int(m.group(1))] = [int(x) for x in m.group(2).split(",")
                                if x.strip()]
    return out


def read_punch_parts():
    """"H1".. -> [frames] out of PUNCH_PART."""
    text = open(FIGHT_GD, encoding="utf-8", errors="replace").read()
    blk = read_block(text, "PUNCH_PART")
    out = {}
    for m in re.finditer(r'"([HL]\d)":\s*\{"f":\s*\[([0-9,\s]*)\]', blk):
        out[m.group(1)] = [int(x) for x in m.group(2).split(",") if x.strip()]
    return out


def main():
    ani = read_ani_table()
    tails = read_tails()
    punch = read_punch_parts()

    bad = ok = 0

    print("== el golpe: ANI[id] contra la parte 1 del stream")
    for aid in sorted(ani):
        if aid not in tails and aid not in (14, 15):
            continue                      # only the attacks carry a tail
        name, have = ani[aid]
        parts = aniparts.parts(aid, 3)
        want = frames_of(parts[0][1]) if parts else []
        mark = "OK " if have == want else "!! "
        if have != want:
            bad += 1
        else:
            ok += 1
        print("  %s ani %-3d %-14s %s" % (mark, aid, name, have))
        if have != want:
            print("        el binario dice %s" % want)

    print()
    print("== la retraccion: ANI_TAIL[id] contra la parte 2")
    for aid in sorted(tails):
        parts = aniparts.parts(aid, 3)
        want = frames_of(parts[1][1]) if len(parts) > 1 else []
        have = tails[aid]
        mark = "OK " if have == want else "!! "
        if have != want:
            bad += 1
        else:
            ok += 1
        print("  %s ani %-3d %s" % (mark, aid, have))
        if have != want:
            print("        el binario dice %s" % want)

    print()
    print("== los punetazos: PUNCH_PART contra las partes de 14 y 15")
    for key in sorted(punch):
        stream = 14 if key[0] == "H" else 15
        idx = int(key[1]) - 1
        parts = aniparts.parts(stream, idx + 2)
        want = frames_of(parts[idx][1]) if idx < len(parts) else []
        have = punch[key]
        mark = "OK " if have == want else "!! "
        if have != want:
            bad += 1
        else:
            ok += 1
        print("  %s %-3s %s" % (mark, key, have))
        if have != want:
            print("        el binario dice %s" % want)

    print()
    print("%d tablas comprobadas: %d coinciden, %d no" % (ok + bad, ok, bad))
    return 0


if __name__ == "__main__":
    sys.exit(main())
