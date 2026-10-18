"""Dump every part of Scorpion's animation streams, with frame names.

_character_anitabs1 indexes by character; Scorpion is 18 and lands on
_nj_ani_data (0x0015978c). The first 0x170 bytes are 92 stream pointers.
A stream is a word list; 0 ends a PART, a word above 0x12 is a frame id
(global, mapped through SCORPIONFRAMES.bin), and the small words are ops:
1 = jump (takes the next word as a target address).
"""
import struct, sys

BIN = 'E:/umk3repo/work/UMK3.armv7'
FL = 'E:/MK3 PROJECT/OUTPUT/SCORPION/SCORPIONFRAMES.bin'
NM = 'E:/MK3 PROJECT/OUTPUT/SCORPION/scorpionframes.txt'
BASE = -0x1000
TAB = 0x0015978c

d = open(BIN, 'rb').read()
fl = open(FL, 'rb').read()
fmap = struct.unpack('<%dH' % (len(fl) // 2), fl)
names = [l.strip() for l in open(NM, encoding='utf-8', errors='replace')]

ptrs = [struct.unpack_from('<I', d, TAB + BASE + 4 * i)[0] for i in range(92)]


def sc(g):
    return fmap[g] if g < len(fmap) else -1


def nm(s):
    return names[s] if 0 <= s < len(names) else '?'


def parts(ani, want=9):
    """Split a stream into parts at its zero words. Returns
    [(part_start_va, [(va, word), ...]), ...]."""
    p = ptrs[ani]
    out, cur, start, k = [], [], p, 0
    while len(out) < want and k < 600:
        w = struct.unpack_from('<I', d, p + BASE + 4 * k)[0]
        if w == 0:
            out.append((start, cur))
            cur = []
            start = p + 4 * (k + 1)
        else:
            cur.append((p + 4 * k, w))
        k += 1
    return out


def render(pt):
    """A part as a readable sequence; jumps resolved to the frame they land on."""
    outs, j = [], 0
    while j < len(pt):
        va, w = pt[j]
        if w == 1 and j + 1 < len(pt):
            tgt = pt[j + 1][1]
            tw = struct.unpack_from('<I', d, tgt + BASE)[0]
            s = sc(tw)
            outs.append('->%d(%s)' % (s, nm(s)))
            j += 2
        elif w > 0x12:
            s = sc(w)
            outs.append('%d(%s)' % (s, nm(s)))
            j += 1
        else:
            outs.append('op%d' % w)
            j += 1
    return ' '.join(outs)


def follow(ani, skips):
    """Which part you land on after `skips` zero-terminators, and its frames.

    This is what find_ani_part2 + N*find_part2 does: get_char_ani parks the
    cursor at the head, and each find_part2 walks past one zero. After
    `skips` of them the cursor sits at the start of part `skips + 1`.
    """
    ps = parts(ani, skips + 2)
    if skips >= len(ps):
        return None
    return ps[skips]


if __name__ == '__main__':
    want = [int(a) for a in sys.argv[1:]] or list(range(0, 30))
    for ani in want:
        print('=== ani %2d  stream 0x%08x' % (ani, ptrs[ani]))
        for i, (a, pt) in enumerate(parts(ani)):
            if not pt:
                print('    parte %d @0x%08x: (vacia)' % (i + 1, a))
                continue
            print('    parte %d @0x%08x: %s' % (i + 1, a, render(pt)))
