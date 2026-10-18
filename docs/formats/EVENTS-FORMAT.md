# The `.events` format

Effect tracks attached to scenes: a named slot, a colour, and a block of
per-event data. 545 files ship with the game.

Derived from `LIME_LoadEvents` (`0x000a477c`) in the armv7 slice.
Parser and validator: [`tools/events.py`](../tools/events.py).

**Status: solved.** 544 of 545 files walk to their exact last byte, the split
is provably unique across the corpus, and the on-disk header is accounted for
byte by byte from the loader's own loads. The one exception is discussed at the
end.

```
int32   numTracks
TRACK   tracks[numTracks]           // variable length

TRACK:
  268 bytes header                  // 0x10c
    +0x000  char  name[64]          // uppercased at load, not retained
    +0x040  int32 field[12]
    +0x070  char  name2[64]
    +0x0b0  int32 field[6]
    +0x0c8  char  slot[64]          // "MUZZLEFLASH", "UNASSIGNED", ...
    +0x108  int32 numEntries
  numEntries * 56 bytes of entries  // 0x38
    +0x000  int32
    +0x004  int32
    +0x008  byte  blob[48]
```

---

## Where the two strides come from

Not from the file walk. From the loader's pointer arithmetic.

**The track array is `numTracks * 216`.** At `0x000a47ec`:

```
lsls r3, r1, #3      ; n*8
lsls r1, r1, #5      ; n*32
subs r1, r1, r3      ; n*24
lsls r3, r1, #3      ; n*192
adds r1, r1, r3      ; n*216      <- in-memory SCENEEVENTTRACK
bl   _limeMalloc
```

The outer loop confirms it independently: at `0x000a49e2` the running in-memory
offset advances by `0xd8` = 216 per track.

**The on-disk header is 268.** At `0x000a48b0` the loader keeps `cursor+0x5c`,
and at `0x000a4934` it forms the entry base:

```
ldr  r2, [sp, #0x58]   ; cursor + 0x5c
adds r2, #0xb0         ; cursor + 0x10c   <- entries start here
```

`0x10c` = 268.

**An entry is 56 bytes.** At `0x000a49be` the disk cursor advances past the
entries:

```
ldr r3, [sp, #0x20]    ; numEntries*64   (computed at 0x000a4914)
lsls r1, r2, #3        ; numEntries*8
rsb r1, r1, r3         ; numEntries*56
adds r2, r2, r1        ; cursor + 0x10c + numEntries*56
```

**`numEntries` is at `+0x108`.** At `0x000a490e`:

```
ldr.w r5, [r5, #0x9c]
```

The `r5` being indexed here is the one set at `0x000a4880` —
`add.w r5, r1, #0x28` with `r1 = cursor+0x44` after the post-indexed load at
`0x000a487c` — so `r5 = cursor + 0x6c`, and the field sits at
`0x6c + 0x9c = 0x108`.

> This is worth stating explicitly because an earlier draft of this document
> derived the offset from the `r5` set at `0x000a4944` (`cursor + 0x114`),
> which gives `0x1b0` and does not match. `0x000a4944` is later in address
> order and belongs to the inner entry loop; it is a different live range of
> the same register.

---

## The header, byte for byte

Every one of the 268 bytes is accounted for by a load in the copy sequence at
`0x000a4876`–`0x000a490a`. The mapping is contiguous with no gaps:

| Disk | Size | → in-memory track | Instruction |
|---|---:|---|---|
| `+0x000` | 64 | *(not retained)* | `memcpy` to stack, uppercased, 7-byte suffix appended |
| `+0x040` | 4 | `+0x08` | `ldr r3, [r1], #4` |
| `+0x044` | 4 | `+0x0c` | `ldr r3, [r2, #4]` |
| `+0x048` | 4 | `+0x10` | `ldr r3, [r1, #4]` |
| `+0x04c` | 4 | `+0x14` | `ldr r3, [r1, #8]` |
| `+0x050` | 4 | `+0x18` | `ldr r3, [r1, #0xc]` |
| `+0x054` | 4 | `+0x1c` | `ldr r3, [r1, #0x10]` |
| `+0x058` | 4 | `+0x20` | `ldr r3, [r1, #0x14]` |
| `+0x05c` | 4 | `+0x6c` | `ldr r3, [r1, #0x18]` |
| `+0x060` | 4 | `+0xc8` | `ldr r3, [r1, #0x1c]` |
| `+0x064` | 4 | `+0xcc` | `ldr r3, [r1, #0x20]` |
| `+0x068` | 4 | `+0x68` | `ldr r3, [r1, #0x24]` |
| `+0x06c` | 4 | `+0x24` | `ldr r3, [r1, #0x28]` |
| `+0x070` | 64 | `+0x28` | `memcpy(track+0x28, cursor+0x70, 0x40)` |
| `+0x0b0` | 4 | `+0xc4` | `ldr r3, [r5, #0x44]` |
| `+0x0b4` | 4 | `+0x70` | `ldr r3, [r5, #0x48]` |
| `+0x0b8` | 4 | `+0xd0` | `ldr r3, [r5, #0x4c]` |
| `+0x0bc` | 4 | `+0x74` | `ldr r3, [r5, #0x50]` |
| `+0x0c0` | 4 | `+0x78` | `ldr r3, [r5, #0x54]` |
| `+0x0c4` | 4 | `+0x7c` | `ldr r3, [r5, #0x58]` |
| `+0x0c8` | 64 | `+0x80` | `memcpy(track+0x80, cursor+0xc8, 0x40)` |
| `+0x108` | 4 | `+0x00` | `numEntries` |
| **268** | | | |

### There is no "discarded" data

268 on disk against a 216-byte struct looks like the loader throws 52 bytes
away. It does not.

The 64-byte name at `+0x000` is **used but not stored**: it is copied to a
stack buffer, uppercased in place by the loop at `0x000a484c`, and has a
7-byte suffix appended at `0x000a486a`. It never reaches the track.

That leaves `268 - 64 = 204` bytes of file data landing in the struct. The
struct's remaining 12 bytes are its own: a flag at `+0x04` (zeroed at
`0x000a49d2`), 4 bytes of padding at `+0xc0`, and the entries pointer at
`+0xd4` (written at `0x000a4924`). `204 + 12 = 216`. Exact.

### The entry, likewise

`numEntries * 68` is allocated at `0x000a4914` (`n*64 + n*4`), and the inner
loop advances by `0x44` = 68 in memory and `0x38` = 56 on disk
(`adds r5, #0x38` at `0x000a4990`). The 56 disk bytes are fully consumed:

| Disk | Size | → in-memory entry |
|---|---:|---|
| `+0x00` | 4 | `+0x00` (`ldr r3, [r5, #-0x8]`) |
| `+0x04` | 4 | `+0x04` (`ldr r3, [r5, #-0x4]`) |
| `+0x08` | 48 | `+0x08` (`memcpy(entry+8, cursor+0x114, 0x30)`) |
| **56** | | |

---

## The corpus check, and a correction

An earlier audit of this format concluded that the file walk was **circular
evidence**, on the grounds that `numEntries` was 1 in 211 of 212 tracks
examined, which would make every track exactly 324 bytes and every file
`4 + numTracks*324` — and with a fixed record size any split of 324 lands
exactly.

**That was measured on a subset. It does not hold over the full corpus.**

Across all 545 files, 1,547 tracks:

| `numEntries` | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 16 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| tracks | 46 | 1,444 | 5 | 29 | 6 | 8 | 4 | 2 | 2 | 1 |

**103 of 1,547 tracks — 6.7% — do not have `numEntries == 1`**, spread over ten
distinct values. The walk therefore does depend on a value that varies, which
is the standard `.meshset` and `.skin` were held to.

The `4 + numTracks*324` formula fails exactly where it should: the
`CUTUP_BY_LAO_*` family carries three entries in its first track and comes out
112 bytes (two entries) larger than that formula predicts.

And the split is **unique**. Brute-forcing every header size from 268 to 516 in
steps of 4, against every entry size from 0 to 196, gives exactly one pair that
walks all 544 files to their last byte:

```
header = 268 (0x10c)   entry = 56
```

So the layout now rests on two independent legs: the loader arithmetic above,
and a corpus walk that a wrong split cannot survive. `tools/events.py validate`
reports the `numEntries != 1` count on every run, so the distinction stays
visible rather than being taken on trust.

---

## What the data appears to mean

Not yet decompiled, but the field contents are suggestive:

- **`+0x040` is an RGBA colour.** The four floats read `1.0, 1.0, 1.0,
  0.7843137`, and `0.7843137 = 200/255` — an 8-bit alpha of 200.
- **`+0x0c8` is a named binding slot.** 99 distinct values across the corpus,
  dominated by `UNASSIGNED` (1,216 tracks) with `MUZZLEFLASH`, `DECAPITATION`,
  `BABALITY`, `ICETHUD_DEATH_ANIMATION`, `SLICEANDDICE_JAX` and similar making
  up the rest. A slot name that is often deliberately empty reads like an
  attachment point, not a generic event tag.
- **The first `int32` of each entry is a sequential id.** Consecutive tracks in
  one file give 291, 292, 293, 294, with values clustered around 4096/4095
  (`0x1000`/`0xFFF`) and a repeated constant of −58686.

Taken together this looks like **effects anchored to bones or slots** rather
than a general-purpose event track.

---

## The one file that does not parse

`CUTUP_BY_REPTILE_STRYKER.events` — 6,224 bytes, walk stops at 272.

It declares `numTracks = 1` and `numEntries = 0`, which accounts for exactly
`4 + 268 = 272` bytes and leaves 5,952 unexplained. The trailing length divides
evenly by nothing plausible (56, 68, 216, 268, 324 and 436 all leave a
remainder).

The previous hypothesis was that it embeds a SCENE loaded through
`LIME_LoadScene`. **That remains unproven**, and the header argues against the
file being an events file at all: the name field at `+0x000` contains a single
junk byte (`'o'`) rather than a name, and the slot field at `+0x0c8` is empty.
Every other file has a plausible name and slot. The likeliest reading is that
this file is a different format that kept the `.events` extension, but nobody
has confirmed it either way, and it is not blocking anything.


---

## What an entry's first field is: a frame index

**This is the link between `.events` and the animation stream**, and it is what
makes the format useful rather than merely parsed.

Each track carries a name, a slot, and entries whose first `int32` is a **frame
index into the character's own `.skinanim`**. Kano's track `babality_Kano`
(slot `KANO_BABALITY`) sits at frame 311; `kano_decap` (slot `DECAPITATION`) at
319 and 320; `explosives` (slot `KANO_EXPLOSIVES VEST`) at 336.

### The evidence

| Test | Result |
|---|---|
| Every index inside its character's frame count | **26 of 26 characters, zero out of range** |
| Correlation between frame count and highest index | **r = 0.9993** (n = 26) |
| Where the highest index falls in the stream | 96% of the way through, on average |

An arbitrary identifier would not track stream length at 0.999. And the 96%
figure says something on its own: **the finishers live at the end of the
stream**. The ordering runs basic moves, then specials, then deaths and
fatalities — which is the segmentation the animation work needs.

### Sindel confirms it, and gets resolved in passing

`SINDEL_STANDARD.skinanim` holds 844 frames while its header declares 422 — a
discrepancy this project recorded but could not explain, noting only that the
two halves differ in 47 bytes out of 530,032.

Her highest event index is 416. Against the file that is 49.3%, the only outlier
in the corpus. **Against the declared 422 it is 98.6%**, exactly in line with
everyone else.

So the header is right and the second half is dead weight: **the game only ever
uses the first 422 frames.** A question that stood open through the whole
`.skinanim` investigation falls out of an unrelated measurement.

### What the rest of the entry is

The remaining twelve values read as a 3x3 matrix in 1/4096 fixed point followed
by three translation terms — `4096, 0, 0, 0, 4095, 0, 0, 0, 4095` is identity,
and it is by far the most common. Kano's five `smokeparticle` tracks all sit at
frame 356 with genuinely different rotations, which is what placing five puffs
of smoke around one event looks like.

So an entry reads: **at frame N, spawn this named thing with this transform.**

---

## The slot catalogue

A track's `slot` string names what the event *is*. Across the 545 `.events`
files there are **98 distinct assigned slots** (plus `UNASSIGNED`), and they
sort themselves:

| Kind | Slots | Examples |
|---|---:|---|
| Fatality / death | 58 | `DECAPITATION` (×50), `ICETHUD_DEATH_ANIMATION` (×25), `SLICEANDDICE_JAX` (×24), `BACKBREAKER_BLOOD`, `FATALITY2 MUSHROOM` |
| Babality | 9 | `BABALITY`, `SCORPION_BABALITY`, `ROBOT_BABALITY`, `SUBZERO_BABALITY` |
| Effects and victory | 31 | `FREEZE_MIST`, `MUZZLEFLASH`, `IN_VICTORY_AXEFIRE`, `RINGTOSS COLLISION FX` |

Combined with the frame index above, this makes the game's finisher catalogue
**machine-readable**: what fires, on which character, and at which frame of
their animation. Extracting it needs no further reverse engineering.

Animalities do not appear as slots. They are
[morph sequences in the `.meshset`](SKIN-FORMAT.md) instead — `SmokeBull`,
`JadeCat` — which is consistent with them being whole-body transformations the
skeleton cannot express.
