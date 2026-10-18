"""
Parser and validator for the `.scene` format.

Derived from `LIME_LoadScene` (`0x0005f0ac`) in the armv7 slice. Every stride
below comes from the loader's own arithmetic rather than from a formula fitted
to file sizes — see docs/SCENE-FORMAT.md for the instruction-level derivation.

    int32  numObjects
    int32  count2
    per object, numObjects times:
        byte   object[64]          // starts with a name string
        byte   track[count2][12]    // three floats each
    int32  count3
    byte   tail[count3][40]

`python scene.py validate <res dir>` walks 545 of 547 files to their exact last
byte, and the walk depends on three independently varying counts — numObjects
spans 0-201 across 63 distinct values, count2 spans 2-4802 across 74, count3
spans 0-9068 across 175. A wrong layout cannot survive that.

The two exceptions are `ROBO1_STANDARD` and `ROBO2_STANDARD`, the same pair that
breaks `.bones` (a 24-byte bone rather than 25) and uses the unindexed
`.meshset` variant. Whatever is different about those two characters' assets is
consistent across formats and is its own question.

Usage:
  python scene.py validate <res dir>
  python scene.py dump     <file.scene>
  python scene.py xforms   <file.scene> [frame]   each object's rotation, scale, position
"""

import glob
import os
import struct
import sys

OBJECT_SIZE = 64        # 0x40, from `add.w fp, fp, #0x40` at 0x0005f28e
TRACK_SIZE = 12         # from `c*16 - c*4` at 0x0005f3a0
TAIL_SIZE = 40          # 0x28, from `ldr r3, [r1, #0x28]!` at 0x0005f444
NAME_LEN = 24           # generous; the string is NUL-terminated well inside


def _cstr(buf, off, limit=NAME_LEN):
    raw = buf[off:off + limit]
    end = raw.find(b"\0")
    return (raw[:end] if end >= 0 else raw).decode("latin-1")


class Scene(object):
    __slots__ = ("num_objects", "count2", "objects", "count3", "tail")

    class Object(object):
        __slots__ = ("raw", "tracks", "name")

        def __init__(self, raw, tracks):
            self.raw = raw
            self.tracks = tracks
            self.name = _cstr(raw, 0)

        def track(self, i):
            """(value, field5, palette index) for track record i.

            +0 and +4 are floats; +8 is the uint16 index into the transform
            palette (`key.paletteIndex`, 0x0005f464) -- read as a float it
            prints as a denormal like 1.4e-45, which is the integer 1.
            """
            off = i * TRACK_SIZE
            v, f5 = struct.unpack_from("<2f", self.tracks, off)
            idx = struct.unpack_from("<H", self.tracks, off + 8)[0]
            return v, f5, idx

    def __init__(self, data):
        if len(data) < 8:
            raise ValueError("shorter than the 8-byte header")
        self.num_objects, self.count2 = struct.unpack_from("<ii", data, 0)
        if self.num_objects < 0 or self.count2 < 0:
            raise ValueError("negative header counts: %d, %d"
                             % (self.num_objects, self.count2))

        pos = 8
        self.objects = []
        stride = OBJECT_SIZE + self.count2 * TRACK_SIZE
        for _ in range(self.num_objects):
            if pos + stride > len(data):
                raise ValueError("truncated object at %d" % pos)
            raw = data[pos:pos + OBJECT_SIZE]
            tracks = data[pos + OBJECT_SIZE:pos + stride]
            self.objects.append(Scene.Object(raw, tracks))
            pos += stride

        if pos + 4 > len(data):
            raise ValueError("count3 past the end at %d" % pos)
        self.count3 = struct.unpack_from("<i", data, pos)[0]
        pos += 4
        if self.count3 < 0:
            raise ValueError("negative count3 %d" % self.count3)
        size = self.count3 * TAIL_SIZE
        self.tail = data[pos:pos + size]
        pos += size

        if pos != len(data):
            raise ValueError("landed at %d, file is %d bytes" % (pos, len(data)))

    def palette(self, i):
        """Transform palette entry i (`SceneMtxPalette`), 40 bytes on disk:

            +0x00  float q[4]       rotation quaternion, x y z, w LAST
                                    (the loader scales it by 32767.0 into int16)
            +0x10  float scale[3]
            +0x1c  float translation[3]

        Returned as (q, scale, translation). An earlier reading split the
        record as four floats and five ints -- 36 of its 40 bytes -- which
        dropped the translation's z. See docs/SCENE-FORMAT.md.
        """
        off = i * TAIL_SIZE
        v = struct.unpack_from("<10f", self.tail, off)
        return v[0:4], v[4:7], v[7:10]

    def transform(self, obj, frame=0):
        """(q, scale, translation) of an object at a frame, through its track."""
        _, _, idx = obj.track(frame)
        return self.palette(idx)


def load(path):
    with open(path, "rb") as fh:
        return Scene(fh.read())


def validate(res_dir):
    files = sorted(glob.glob(os.path.join(res_dir, "**", "*.scene"),
                             recursive=True))
    ok = 0
    failures = []
    objects = tracks = tails = 0
    for fn in files:
        try:
            sc = load(fn)
        except ValueError as e:
            failures.append((os.path.basename(fn), str(e)))
            continue
        ok += 1
        objects += sc.num_objects
        tracks += sc.num_objects * sc.count2
        tails += sc.count3

    print("files parsed:     %5d of %d" % (ok, len(files)))
    print("objects walked:   %5d" % objects)
    print("track records:    %5d" % tracks)
    print("tail records:     %5d" % tails)
    print("failures:         %5d" % len(failures))
    for name, why in failures:
        print("   %-32s %s" % (name, why))
    return ok == len(files)


def dump(path):
    sc = load(path)
    print("%s" % os.path.basename(path))
    print("  numObjects %d   count2 %d   count3 %d"
          % (sc.num_objects, sc.count2, sc.count3))
    for i, obj in enumerate(sc.objects[:12]):
        print("  object %2d  %-20r  %d track records" % (i, obj.name, sc.count2))
        for j in range(min(sc.count2, 3)):
            print("       %s" % (obj.track(j),))
    if sc.count3:
        print("  palette: %d transforms of %d bytes" % (sc.count3, TAIL_SIZE))
        for i in range(min(sc.count3, 3)):
            q, sc3, t = sc.palette(i)
            print("       q %s  scale %s  pos %s" % (q, sc3, t))


def xforms(path, frame=0):
    """Every object's placement at a frame: what a 3D tool needs to rebuild
    the stage from its .meshset (objects are named after their meshes)."""
    sc = load(path)
    for obj in sc.objects:
        if not sc.count2:
            continue
        v, _, _ = obj.track(frame)
        q, scale, t = sc.transform(obj, frame)
        print("%-20s q(xyzw) %s  scale %s  pos %s%s" % (
            obj.name, tuple(round(x, 4) for x in q),
            tuple(round(x, 4) for x in scale), tuple(round(x, 2) for x in t),
            "" if v > 0.03 else "  (hidden on this frame)"))


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(1)
    cmd, path = sys.argv[1], sys.argv[2]
    if cmd == "validate":
        sys.exit(0 if validate(path) else 1)
    elif cmd == "dump":
        dump(path)
    elif cmd == "xforms":
        xforms(path, int(sys.argv[3]) if len(sys.argv) > 3 else 0)
    else:
        print(__doc__)
        sys.exit(1)
