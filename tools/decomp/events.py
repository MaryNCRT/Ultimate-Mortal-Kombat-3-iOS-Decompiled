"""
Parser and validator for the .events format.

Derived from the disassembly of LIME_LoadEvents (0x000a477c in the armv7 slice).

    int32  numTracks
    TRACK  tracks[numTracks]        // variable length

    TRACK:
      268 bytes header
        +0x000  char  name[64]      // uppercased at load time, not retained
        +0x040  int32 field[12]
        +0x070  char  name2[64]
        +0x0b0  int32 field[6]
        +0x0c8  char  slot[64]      // "MUZZLEFLASH", "UNASSIGNED", ...
        +0x108  int32 numEntries
      numEntries * 56 bytes of entries
        +0x000  int32
        +0x004  int32
        +0x008  byte  blob[48]

Both strides come from the loader's own pointer arithmetic, not from the file
walk (see docs/EVENTS-FORMAT.md for the instruction-level derivation):

  - 0x000a4934 builds the entry base as (cursor+0x5c)+0xb0 = cursor+0x10c,
    so the header is 268 bytes.
  - 0x000a49be advances the cursor by numEntries*64 - numEntries*8 =
    numEntries*56, so an entry is 56 bytes.
  - 0x000a490e reads numEntries as [r5,#0x9c] where r5 = cursor+0x6c
    (set at 0x000a4880), so numEntries lives at cursor+0x108.

Caveat that matters when reading the validation output: numEntries is 1 in
every track of every shipped file. A file walk therefore cannot distinguish
this split from any other split of 324, and is NOT by itself evidence for the
layout. `validate` reports how many tracks had numEntries != 1 for exactly
this reason -- the first such track would be the one that tests the split.

Usage:
  python events.py validate <res dir>
  python events.py dump     <file.events>
"""

import glob
import os
import struct
import sys

TRACK_HEADER = 268      # 0x10c on disk; the in-memory SCENEEVENTTRACK is 216
ENTRY_SIZE = 56         # 0x38 on disk; the in-memory entry is 68
NUM_ENTRIES_OFFSET = 0x108
NAME_OFFSET = 0x000
NAME2_OFFSET = 0x070
SLOT_OFFSET = 0x0c8
NAME_LEN = 64


def _cstr(buf, off, length=NAME_LEN):
    raw = buf[off:off + length]
    end = raw.find(b"\0")
    if end >= 0:
        raw = raw[:end]
    return raw.decode("latin-1")


class Events(object):
    __slots__ = ("num_tracks", "tracks", "bare_entries")

    class Track(object):
        __slots__ = ("header", "num_entries", "entries",
                     "name", "name2", "slot")

        def __init__(self, header, num_entries, entries):
            self.header = header
            self.num_entries = num_entries
            self.entries = entries
            self.name = _cstr(header, NAME_OFFSET)
            self.name2 = _cstr(header, NAME2_OFFSET)
            self.slot = _cstr(header, SLOT_OFFSET)

        def entry(self, i):
            """(int32, int32, 48-byte blob) for entry i."""
            off = i * ENTRY_SIZE
            a, b = struct.unpack_from("<ii", self.entries, off)
            return a, b, self.entries[off + 8:off + ENTRY_SIZE]

    def __init__(self, data):
        self.num_tracks = struct.unpack_from("<i", data, 0)[0]
        self.tracks = []
        self.bare_entries = None
        pos = 4

        # Second variant: a bare entry array with no track headers at all.
        # `CUTUP_BY_REPTILE_STRYKER.events` is the only file that uses it —
        # `int32 flag; int32 numEntries;` then entries of the same 56 bytes,
        # and 8 + 111*56 lands exactly on its last byte. Its entries carry the
        # same sequential first int32 the normal ones do (11, 12, 13...), which
        # is what identifies them as entries rather than something else.
        #
        # It is tried only when the normal walk cannot work, so a well-formed
        # file is never reinterpreted.
        if len(data) >= 8:
            flag, count = struct.unpack_from("<2i", data, 0)
            normal_ok = (self.num_tracks >= 0
                         and 4 + self.num_tracks * TRACK_HEADER <= len(data))
            if not normal_ok or (flag == 1 and 8 + count * ENTRY_SIZE == len(data)
                                 and 0 < count < 100000):
                if 8 + count * ENTRY_SIZE == len(data) and 0 < count < 100000:
                    self.bare_entries = data[8:]
                    self.num_tracks = 0
                    return
        for _ in range(self.num_tracks):
            header = data[pos:pos + TRACK_HEADER]
            if len(header) < TRACK_HEADER:
                raise ValueError("truncated track header at %d" % pos)
            num_entries = struct.unpack_from("<i", data,
                                             pos + NUM_ENTRIES_OFFSET)[0]
            if num_entries < 0 or num_entries > 100000:
                raise ValueError("implausible numEntries %d at %d"
                                 % (num_entries, pos))
            esize = num_entries * ENTRY_SIZE
            entries = data[pos + TRACK_HEADER:pos + TRACK_HEADER + esize]
            if len(entries) < esize:
                raise ValueError("truncated entries at %d" % pos)
            self.tracks.append(Events.Track(header, num_entries, entries))
            pos += TRACK_HEADER + esize
        if pos != len(data):
            raise ValueError("landed at %d, file is %d bytes" % (pos, len(data)))


    def num_bare(self):
        """How many bare entries this file holds, 0 for the normal variant."""
        return 0 if self.bare_entries is None else len(self.bare_entries) // ENTRY_SIZE

    def bare_entry(self, i):
        """(int32, int32, 48-byte blob) for a bare-variant entry."""
        off = i * ENTRY_SIZE
        a, b = struct.unpack_from("<ii", self.bare_entries, off)
        return a, b, self.bare_entries[off + 8:off + ENTRY_SIZE]


def load(path):
    with open(path, "rb") as f:
        return Events(f.read())


def validate(res_dir):
    files = sorted(glob.glob(os.path.join(res_dir, "**", "*.events"),
                             recursive=True))
    ok = 0
    exceptions = []
    total_tracks = 0
    varying = []            # tracks whose numEntries is not 1
    empty_files = 0
    bare = []
    slots = {}
    for fn in files:
        try:
            ev = load(fn)
        except ValueError as e:
            exceptions.append((os.path.basename(fn), str(e)))
            continue
        ok += 1
        total_tracks += ev.num_tracks
        if ev.num_bare():
            bare.append((os.path.basename(fn), ev.num_bare()))
        elif ev.num_tracks == 0:
            empty_files += 1
        for i, t in enumerate(ev.tracks):
            if t.num_entries != 1:
                varying.append((os.path.basename(fn), i, t.num_entries))
            slots[t.slot] = slots.get(t.slot, 0) + 1

    print("files parsed:        %5d" % ok)
    print("files total:         %5d" % len(files))
    print("  of which empty:    %5d  (numTracks == 0, 4 bytes)" % empty_files)
    for name, n in bare:
        print("  bare-entry variant:      %s, %d entries, no track headers" % (name, n))
    print("tracks walked:       %5d" % total_tracks)
    print("exceptions:          %5d" % len(exceptions))
    for name, why in exceptions:
        print("  %s  %s" % (name, why))

    print("")
    print("tracks with numEntries != 1: %d" % len(varying))
    if not varying:
        print("  NOTE: numEntries is 1 everywhere, so every track is exactly")
        print("  324 bytes and the file walk cannot distinguish 268+56 from")
        print("  any other split of 324. The split rests on the loader")
        print("  arithmetic, not on this walk. See docs/EVENTS-FORMAT.md.")
    else:
        for name, i, n in varying[:20]:
            print("  %s track %d: numEntries=%d   <- tests the split" %
                  (name, i, n))

    print("")
    print("slot names seen (%d distinct):" % len(slots))
    for s, n in sorted(slots.items(), key=lambda kv: -kv[1])[:12]:
        print("  %-24s %5d" % (repr(s), n))
    return ok == len(files)


def dump(path):
    ev = load(path)
    print("numTracks: %d" % ev.num_tracks)
    for i, t in enumerate(ev.tracks):
        print("  track %2d  name=%-20r slot=%-14r numEntries=%d  (%d bytes)"
              % (i, t.name, t.slot, t.num_entries,
                 TRACK_HEADER + t.num_entries * ENTRY_SIZE))
        for j in range(t.num_entries):
            a, b, blob = t.entry(j)
            words = struct.unpack_from("<12i", blob, 0)
            print("      entry %d: %d, %d, %s" % (j, a, b, list(words)))


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(1)
    cmd = sys.argv[1]
    path = sys.argv[2]
    if cmd == "validate":
        sys.exit(0 if validate(path) else 1)
    elif cmd == "dump":
        dump(path)
    else:
        print(__doc__)
        sys.exit(1)
