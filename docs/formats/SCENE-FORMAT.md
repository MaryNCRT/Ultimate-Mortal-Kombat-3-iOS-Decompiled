# The `.scene` format

The scene graph: which objects make up a stage or an effect, their animation
tracks, and a trailing table. 547 files ship with the game.

Derived from `LIME_LoadScene` (`0x0005f0ac`) in the armv7 slice.
Parser and validator: [`tools/scene.py`](../tools/scene.py).

**Status: solved.** `python tools/scene.py validate <res dir>` walks **545 of
547 files** to their exact last byte — 7,254 objects, 1,664,493 track records,
152,306 tail records. The two exceptions are discussed at the end.

```
int32   numObjects
int32   count2
per object, numObjects times:
    byte  object[64]            // begins with a name string
    byte  track[count2][12]     // three floats each
int32   count3
byte    tail[count3][40]
```

---

## Every stride comes from the loader

Not from a formula fitted to file sizes. That distinction is the whole point,
and an earlier attempt at this format produced a formula matching 71 of 92
single-object files — a near-miss that looked like a solution and was not.

**The header is two int32** (`0x0005f204`):

```
ldr   r0, [r6], #4       ; numObjects, cursor advances 4
str   r0, [r4, #0x48]
ldr   r1, [r1, #4]       ; count2
str   r1, [r4, #0x44]
add.w fp, r6, #4         ; cursor now at +8
```

**The object is 64 bytes** (`0x0005f28e`), copied verbatim rather than
scattered field by field the way `.events` is:

```
blx   memcpy             ; 64 bytes into objects[i]
add.w fp, fp, #0x40      ; cursor advances 64
```

**Each object carries `count2` track records of 12 bytes** (`0x0005f3a0`):

```
ldr  r0, [sp, #0x1c]     ; count2
lsls r2, r0, #2          ; count2*4
lsls r3, r0, #4          ; count2*16
subs r3, r3, r2          ; count2*12
add  fp, r3              ; cursor advances past this object's tracks
```

The inner loop reads a float at `[sl, #0x40]` stepping `sl` by `0xc`, and
`0x0005f3bc` resets `sl` to the advanced cursor for the next object. So the
tracks are **per object**, giving `numObjects × count2` records in total.

**Then a third count, read from the file** (`0x0005f3c4`):

```
ldr.w r6, [fp]           ; count3
lsls  r1, r6, #5         ; count3 * 32   <- in-memory record is 32 bytes
bl    limeMalloc
```

**And the tail records are 40 bytes on disk** (`0x0005f444`) — the giveaway is
the pre-indexed load with writeback at the end of the copy sequence:

```
ldr r3, [r1, #0x28]!     ; read, then advance r1 by 0x28
```

The first tail field is read at `+4` rather than `+0`, because `+0` holds the
`count3` that precedes the array.

---

## Why the walk is evidence

By this project's own rule, a file walk proves nothing unless it depended on
values that varied. Across the 545 files that parse:

| | distinct values | range |
|---|---:|---|
| `numObjects` | 63 | 0 – 201 |
| `count2` | 74 | 2 – 4,802 |
| `count3` | 175 | 0 – 9,068 |

Three independently varying counts, each governing a different array, and the
walk still lands on the exact last byte of 545 files. A wrong layout does not
survive that.

---

## What the data is

The objects are **scene graph nodes exported from a modelling package**. Their
names give it away — `BALCONY_LEVEL_SCENE.scene` holds `Plane002`, `Box004`,
`Box03`, which are 3ds Max defaults. Other files use authored names like
`Text001` and `Helix001`.

The per-object 12-byte tracks are three floats, overwhelmingly `1.0, 0.0, 0.0`
in the files inspected — consistent with per-frame scale or visibility that is
mostly inert. The loader compares the first float against a constant and only
acts when it exceeds it, then calls `LIME_FindMeshByName` with the object's
name, which is how a scene node is bound to geometry.

`AXEFIRE.scene` shows non-trivial tracks: `(5, 0.7209), (6, 0.6903),
(7, 0.6606), (8, 0.6309)` — an index rising while a value falls, which is a
keyframed fade.

### The 40-byte tail records are the PLACEMENT

They were described here as "four floats the loader scales and narrows to
`int16`, followed by five `int32`s. Their meaning is not yet established." It is
established now, and the buffer the loader allocates for them says it outright:

```
0x0005f3c8  ldr  r0, [pc, #0xe4]      ; "SceneMtxPalette"
0x0005f3ca  lsls r1, r6, #5           ; count3 * 32
0x0005f3ce  bl   _limeMalloc
0x0005f3d2  str  r0, [r4, #0x7c]      ; SCENEINFO+0x7c
```

32 bytes per record is exactly `QSTMATRIX`, and the copy loop maps the disk
record onto it field for field:

| disk `+` | | in-memory `+` | field |
|---|---|---|---|
| `0x00`–`0x0f` | `x 32767.0`, `vcvt.s32.f32` | `0x00` | `int16 q[4]` — x, y, z, **w last** |
| `0x10`–`0x1b` | copied verbatim | `0x08` | `float scale[3]` |
| `0x1c`–`0x27` | copied verbatim | `0x14` | `float translation[3]` |

The scale is the literal `32767.0` at `0x0005f468`, and `vcvt.s32.f32` rounds
toward zero, which is what a C float-to-int conversion does.

**Why this is not a guess.** Across Graveyard's 58 records every quaternion has
`|q| = 1.00000`, and the largest single component is exactly `1.0` — so the
narrowing lands on 32767 and never overflows the `int16`. Read one float
earlier or later and neither holds. The range also closes on the file's last
byte to the byte.

Node 0 is `q = (-0.7071, 0, 0, 0.7071)`: a -90 degrees rotation about X, which is
the Z-up to Y-up conversion the engine applies at the root.

`GetMatrixFromPalette` indexes this array, and the index comes from
`SCENENODEKEY+0x06` — from the KEY, not from the frame. See
[RENDERSCENE-SIGNATURE.md](RENDERSCENE-SIGNATURE.md).

### The node keys are built at load, not read

`count3` is the palette size and is unrelated to `numObjects` — they match in 90
of 547 files and differ in 455. What binds them is the per-object track array:

```
for each object i:
    for each track k in 0..count2-1:
        stream[i][k] = 0xFFFF                       ; strh -1
        if track[k].value <= 0.03: continue         ; 0x0005f464
        key.alpha        = track[k] +0x00
        key.meshIndex    = LIME_FindMeshByName(object name)   ; byte, 0xFF = miss
        key.field5       = track[k] +0x04
        key.paletteIndex = track[k] +0x08           ; uint16
        stream[i][k] = keyCount++
```

So a track record with a value above `0.03` becomes a visible key on that frame,
and `stream[node][frame]` is what `LIME_RenderScene` reads to find it. The
arrays are the `scene_nodes` and `scenenodes_i` allocations at `SCENE+0x88` and
`+0x8c`.

`runtime/lime/scene.c` implements all of this.

---

## A scene is the root of a file group

The loader strips the last 6 characters of the filename — exactly `.scene` —
builds variants, and calls `limeLoadFile` three more times plus
`LIME_LoadEvents` once. That explains something that had gone unremarked: the
`.events` corpus is the same size as the scene corpus because **every scene
owns one**.

Scenes are also **cached and reference-counted**. `LIME_GetSceneFromFilename`
(`0x0005ef6c`) runs first and, on a hit, the loader just increments the count at
`+0x40` and returns. Loading the same scene twice does not reparse it.

### The in-memory `SCENE`

| Offset | Contents |
|---|---|
| `+0x40` | reference count |
| `+0x44` | `count2` |
| `+0x48` | `numObjects` |
| `+0x4c` | objects array, `numObjects × 64` |
| `+0x54`…`+0x70` | seven pointers, from the three extra `limeLoadFile` calls |
| `+0x7c` | tail array, `count3 × 32` |
| `+0x84` | the scene's `.events` |
| `+0x88`, `+0x8c` | pointer arrays, `numObjects × 4` each |

---

## The two files that do not parse

`ROBO1_STANDARD.scene` (8 bytes — the header alone, with no `count3`) and
`ROBO2_STANDARD.scene` (13,904 bytes; the walk overshoots to 153,908).

**This is the same pair that differs in every other format**, and it is now
understood: they are a **different export variant**. `.bones` uses a 24-byte
bone rather than 25 — both divide exactly, 29 of 29 across the corpus — `.skin`
omits the leading block count, and neither ships `.events` or `.lighting`.
`ROBO2_STANDARD.skin` is exactly four bytes shorter than
`SEKTOR_STANDARD.skin`, and its first 1,276 bytes are byte-identical once that
count is removed.

So the `.scene` failure is not a parser bug either. Their scene files are
stubs — ROBO1's is the 8-byte header alone. See
[PROGRESS.md](PROGRESS.md#robo1-and-robo2-one-export-variant-not-four-parser-bugs).
