# The `.skin` format

Skinning data: what binds a character's vertices to its skeleton. Together with
`.bones` and `.skinanim` this is what makes the fighters move, so it is the
format that stood between a static model viewer and an actual game.

Derived from the disassembly of `LIME_LoadSkin` (`0x00060650`) and
`LIME_LoadSkin1` (`0x000604c0`) in the armv7 slice, and validated against every
`.skin` file the game ships.

> **Validation: 29 of 29 files parse with the walk landing exactly on the last
> byte.** 28,315 skinning matrices, 54,413 vertices. There is no length field
> anywhere in the file, so an exact landing on every file is strong evidence
> that the layout is right rather than merely plausible.
>
> Reproduce with `python tools/skin.py validate <res dir>`.

---

## 1. Overall structure

```
int32   blockCount          // 1 or 2
BLOCK   blocks[blockCount]
```

`LIME_LoadSkin` reads the count, calls `LIME_LoadSkin1` once per block, and
chains the results through `SKININFO+0x00`. The code only ever tests for
`== 2`, so two is the practical maximum — a character with a second skin, such
as a detachable part.

### The two odd files

`ROBO1_STANDARD.skin` and `ROBO2_STANDARD.skin` have **no leading count**:
their first `int32` is already the matrix count, and the file is a single bare
block.

These are the same two files that use the unindexed variant of `.meshset`, so
they clearly came out of a different export path. A reader should try the
counted layout first and fall back to a bare single block if the walk does not
land exactly — which is what `tools/skin.py` does.

---

## 2. A block

```
int32   numMatrices         // N — skinning matrix entries
int32   numVerts            // M — vertices

uint32  indexes[N]                  // copied verbatim
uint16  weights[N * 4]              // each scaled by 1/65536 on load
struct { MATRIX43 a; MATRIX43 b; }  entries[N]   // 96 bytes each
byte    vertData[M * 24]            // copied verbatim
byte    vertExtra[M * 6]            // copied verbatim
```

Total size of a block: `8 + N*108 + M*30`.

`MATRIX43` is 12 floats — 48 bytes. The name comes from the binary itself: the
mangled symbol `__Z6Xform2P11limeVECTOR3S0_S0_S0_P12SKINMATRIX43f` decodes to
`Xform2(limeVECTOR3*, …, SKINMATRIX43*, float)`.

### Weights are 16-bit fixed point

The loader converts each `uint16` to float and multiplies by the constant at
`0x00060634`, which is `0x37800000` — exactly `1/65536`.

That the encoding is right is easy to confirm by looking at the values: the
first entries of `SCORPION_STANDARD.skin` decode to `0.99998, 0, 0, 0`, which
is one bone at full influence and three unused slots. Four weights per matrix
entry is the usual skinning arrangement.

### The buffers name themselves

`limeMalloc`'s first argument is a debug tag, and the strings survive in the
binary. They are the best evidence available for what each buffer is:

| Destination | Size | Tag in the binary |
|---|---|---|
| `SKININFO+0x20` | `N × 4` | `skin_indexes` |
| `SKININFO+0x24` | `N × 16` | `skin_mweights` |
| `SKININFO+0x14` | `N × 48` | (matrix array) |
| `SKININFO+0x28` | `N × 48` | `skin_normals` |
| `SKININFO+0x1C` | `M × 24` | `skin_uvs` |
| `SKININFO+0x18` | `M × 6` | (per-vertex, 6 bytes) |

Two things about that table were long misread, and both are settled in
[§7](#7-the-skinning-algorithm-in-full) and
[§10](#10-the-two-per-vertex-blocks-they-are-per-triangle):

- The `skin_uvs` tag sits on a **24 bytes per vertex** buffer, which is three
  times what a UV pair needs — because it is **three UV pairs, one per triangle
  corner**. The tag was accurate all along; `M` counts triangles, not vertices.
- `indexes` decode as large unsigned values like `0xFFFFFF2E`, which looked like
  negative numbers. They are not signed at all: each word is **four packed bone
  indices, one byte each**, and `0xFF` marks an unused influence slot. An unused
  fourth slot puts `0xFF` in the top byte and sets the sign bit.

---

## 3. `SKININFO` in memory

48 bytes — `limeMalloc(tag, 0x30)` in `LIME_LoadSkin`.

| Offset | Field | Notes |
|---|---|---|
| `0x00` | `SKININFO *next` | second block, or `NULL`; set to 0 before loading |
| `0x04` | `int numMatrices` | N |
| `0x08` | `int numVerts` | M |
| `0x14` | `MATRIX43 *matricesA` | first matrix of each entry |
| `0x18` | `void *vertExtra` | `M × 6` bytes |
| `0x1C` | `void *vertData` | `M × 24` bytes |
| `0x20` | `int32 *indexes` | N entries |
| `0x24` | `float *weights` | `N × 4` floats |
| `0x28` | `MATRIX43 *matricesB` | second matrix of each entry |

Offsets `0x0C`, `0x10` and `0x2C` are not written by the loader.

Note the ordering trap: within each 96-byte entry the **first** matrix goes to
`+0x14` and the **second** to `+0x28`, but the loader allocates `+0x14` first
and `+0x28` second. Reading the allocation order as the storage order gets the
two arrays backwards.

---

## 4. The `.bones` format

The skeleton itself. From `LIME_LoadBones` (`0x000603f0`).

```
int32  numBones (N)
BONE   bones[N]        // 25 bytes each
```

Total: `4 + N*25`. **All 29 files parse, and all 29 yield a well-formed
tree** — see [§8](#8-bones-on-disk-versus-in-memory).

`ROBO1_STANDARD.bones` and `ROBO2_STANDARD.bones` use **24 bytes** per bone
instead of 25 — the same two files that are the odd ones out in both
`.meshset` and `.skin`. At this point it is safe to treat those two as having
come out of a different exporter entirely.

### A bone on disk — 25 bytes

| Offset | Type | Meaning |
|---|---|---|
| `0x00` | `int32` | number of used entries in the link array below |
| `0x04` | `float` | offset X |
| `0x08` | `float` | offset Y |
| `0x0C` | `float` | offset Z |
| `0x10` | `int8[9]` | **nine child indices**; `-1` (`0xFF`) = unused slot |

An earlier version of this document read the first byte as a parent link and
`0x00` as a child count. **Both readings are wrong**, and the tree test in
[§8](#8-bones-on-disk-versus-in-memory) is what disproves them: treating all
nine slots as children yields exactly one root, every index in range and no bone
claimed by two parents, across all 29 files. A parent stored in slot 0 would
double-count immediately.

From `SCORPION_STANDARD.bones`:

```
bone 0:  word0=2   offset=(0.043, 106.068, -0.005)   links = FF 01 FF FF FF FF FF FF FF
bone 1:  word0=2   offset=(0, 0, 0)                  links = 02 2D FF FF FF FF FF FF FF
bone 2:  word0=3   offset=(10.740, 4.117, 0)         links = 03 25 29 FF FF FF FF FF FF
```

Bone 0 is the root and has one child, bone 1. Bone 1's children are 2 and `0x2D`
(45); bone 2's are 3, `0x25` (37) and `0x29` (41) — spine and two limbs
branching, which is what a skeleton looks like. Note bone 0 leaves slot 0 empty
and fills slot 1, so the slots are **not** packed from the front.

`word0` is **not** the child count: bone 0 has one child and reads 2. Its
meaning is still open, and nothing needs it.

### In memory

`BONESINFO` is 8 bytes — `limeMalloc(tag, 8)`:

| Offset | Field |
|---|---|
| `0x00` | `BONE *bones` |
| `0x04` | `int numBones` |

`BONE` is **56 bytes** — the allocation is `N*64 − N*8`. The loader copies the
first 16 bytes of each disk record verbatim and then writes a pointer at
`+0x14`, computed as `bones + index * 56`: the link indices are turned into
real pointers at load time.

---

## 5. The `.skinanim` format

The animation itself. From `UnpackAnimFrame` (`0x0006012c`).

```
float  scale                 // always 1.0 in the shipped data
int32  numFrames
int32  frameSize
FRAME  frames[numFrames]
```

Total: `12 + numFrames * frameSize`. **Validated on 28 of the 29 files**,
9,590 animation frames in total.

Reproduce with `python tools/skin.py validate-anim <res dir>`.

### A frame

```
int32        tag             // not read by UnpackAnimFrame
limeVECTOR3  rootPosition    // three floats
BONEANIMFRAME bones[N]       // 20 bytes each
```

So `frameSize == 16 + N*20`, and the bone count follows from it.

**Read `frameSize` from the header. Never compute it from the matching
`.bones` file.** Doing so looks correct on most characters and then fails on
ROBO1 and ROBO2, whose animations carry a different bone count than their
skeletons — ROBO1's animation has 20 bones where its `.bones` declares 25.
That mistake cost an iteration here.

`BONEANIMFRAME` is 20 bytes: five floats. `GetSlerpedQ` and `GetMFromQuat2`
operate on it, so four of them are a quaternion; the fifth is not yet
identified.

### The one file that does not fit

`SINDEL_STANDARD.skinanim` reads a second float where the frame count should
be. Treating the header as 16 bytes gives `frameSize = 1256`, which is exactly
`16 + 62*20` for Sindel's 62 bones, and `16 + 844*1256` is exactly the file
size — but the header's own count says 422, which is half of 844. Two streams
of 422, or one of 844 with a longer header; the arithmetic works either way and
the disassembly has not been read far enough to say which. **Unresolved.**

---

## 6. How the runtime consumes all this

Recovered from the **armv6 slice** of `RenderSkinned.cpp`, where the same source
compiles to plain scalar VFP instead of the packed NEON that makes the armv7
build unreadable. Decompiled C: [`decomp/lime/RenderSkinned.c`](../decomp/lime/RenderSkinned.c).

### `SKINMATRIX43` is 9 + 3, and the rotation stride is 3

```c
float m[9];     /* rotation, row-major, m[row*3 + col] */
float t[3];     /* translation */
```

Despite the name, the rotation rows are packed tight at stride 3, not 4.
`Xform2` reads byte offsets `0x00`–`0x20` and never past; the same struct's
`0x24`/`0x28`/`0x2c` receive the bone's position.

### The quaternion is `(x, y, z, w)` — w last

`GetMFromQuat2` (armv6 `0x00083188`) is the ordinary conversion, and matching
its nine outputs against the standard formula pins the component order. This is
the single fact a `.bones` reader most has to get right, and it is now settled
rather than guessed.

### A vertex accumulates, and the weight is not a blend factor

`Xform2` (armv6 `0x00082f48`) does:

```c
vout->x += x*m[0] + y*m[3] + z*m[6];   /* and likewise y, z */
```

Three consequences, each of which would be easy to get wrong from the signature:

- **It accumulates.** A vertex driven by three bones is transformed three times
  into the same destination; the sum is the skinned position.
- **Only the rotation applies.** `m[9..11]` is never read here. Bone translation
  enters earlier, when `MatrixMul2` composes a bone with its parent.
- **The weight is only tested against zero**, then discarded. The caller passes
  a matrix already scaled by the weight. Treating `w` as a blend factor at this
  point would double-apply it.

### The palette is built depth-first, 48 bytes per bone

`CreateMatrixPaletteRecurse2` (armv6 `0x00083240`) walks the tree on three
global cursors: an animation-frame cursor stepping **20 bytes per bone**, a
second cursor in lockstep, and a palette write cursor stepping **48 bytes**. So
`BONEANIMFRAME` is 20 bytes — a 16-byte quaternion plus one more word — and the
frames are stored in the same depth-first order as the bones.

**The root bone's translation comes from a global, not from the file.** A
counter gates it so this fires exactly once, on the first bone visited. That
global is where the character's world position enters the skeleton, and it means
a `.bones` root translation is not what gets used at runtime.

---

## 7. The skinning algorithm, in full

From `DrawSkinnedMesh2` (armv6 `0x00083d10`, 2,152 bytes). One iteration per
vertex, four bone influences each.

### `indexes` is four packed bytes, not an integer

The loop reads `idx[0]`…`idx[3]` with `ldrb` and advances by 4 per vertex. Each
byte is a bone index; **`0xFF` means the slot is unused**.

This retires the "why are the values negative" question above: they never were
negative. An unused fourth influence puts `0xFF` in the top byte, which sets the
sign bit of the word. Reading them as `int32` was the mistake.

### `matricesA` is positions, `matricesB` is normals

Each is **four `vec3`s, one per influence** — 12 floats, 48 bytes, which is why
they looked like 4×3 matrices. A feeds the position path, B feeds `Xform2` and
the result goes straight to `Normalise`.

The earlier guess from the data — B unit-length, A looking like a coordinate —
was right, and is now confirmed from the code rather than inferred.

### The vectors are pre-multiplied by their weight

Which is the piece that makes everything else make sense. The rotation needs no
weight because it is already baked into A and B; only the translation term
carries an explicit `w`. It is also why `Xform2` ignores its weight beyond the
zero test — applying it there would double it.

### The maths

```
pos = Σᵢ ( A[i]·M₃ₓ₃ + w[i]·T )        translation included
nrm = Σᵢ ( B[i]·M₃ₓ₃ )                 rotation only, then Normalise
col = LightVert(nrm) → one grey byte, R=G=B, alpha 0xFF
```

`M₃ₓ₃` is the palette entry for that influence's bone, at **48-byte stride**.

**Vertex colour is the lit skinned normal.** That is where these characters'
shading comes from; there is no per-pixel lighting anywhere.

### `num_matrices` is a vertex count

The loop terminates on `SKININFO+4` and advances one entry per iteration:
`indexes` by 4, `weights` by 16, `matricesA` by 48. So Kano's "844 matrices" are
844 skinned vertices.

---

## 8. `.bones` on disk versus in memory

`LIME_LoadBones` (armv6 `0x000836f8`) converts one to the other, and the
conversion is where the hierarchy comes from.

**The in-memory bone is 56 bytes**, allocated as `numBones*64 - numBones*8`.
The on-disk record is 25 (or 24 — see the variant note in §4), so the two are
not the same shape and a `.bones` reader cannot simply cast.

**A bone has up to nine children**, which is what makes the memory record that
size. The conversion loop runs nine times (`cmp r0, #9`), reading a signed byte
per iteration and writing a pointer, so memory is `0x14 + 9*4 = 0x38 = 56`.

```
int32   word0                   /* not a child count -- see below */
float   x, y, z
int8    child[9]                /* -1 = empty slot */
```

`16 + 9 = 25`, which is the record size, and the loop advances the disk cursor
by exactly `0x19`. The 24-byte ROBO variant is the same record with **eight**
slots.

| Disk | Memory | Meaning |
|---|---|---|
| `+0x00`…`+0x0f` | `+0x00`…`+0x0f` | four words, copied verbatim |
| `+0x04`, `+0x08`, `+0x0c` | same | x, y, z — the bone's offset from its parent |
| `+0x10`…`+0x18` | `+0x14`…`+0x34` | nine signed child indices → nine pointers, `base + index*56` |

### The tree is verified, not assumed

Across all 29 files: **exactly one root, every index in range, no bone claimed
by two parents**. Kano's 51 bones yield exactly 50 filled slots — every bone but
the root is somebody's child, once. Tree depth runs 10 to 17.

And it reads as anatomy. Kano's bone 1 sits at the origin with children
`[2, 45, 48]` — hips branching to spine and two legs. Bone 6 has `[7, 9, 23]` —
neck and two arms. **Bone 12 has five children**, `[13, 15, 17, 19, 21]`: a hand
and its fingers.

### `word0` is not the child count

The obvious reading fails immediately and consistently: bone 0 has `word0 = 2`
with one child, and across Kano the totals are 64 against 50. Whatever it
counts, it is not children — and the tree does not need it, since the index
bytes carry the structure on their own. Its meaning is still open.

## 9. The animation frame

`.skinanim` frames are `float; float root[3];` then **20 bytes per bone**, of
which the first 16 are a unit quaternion as plain `float32`. Not packed, despite
`UnpackAnimFrame`'s name: `|q|` measures 1.0000 for every bone tested. The
remaining 4 bytes are unidentified.

Two independent cross-checks land exactly:

- Kano's frame-0 root position is `(-12.4313, 84.8384, ~0)`, which is **bone 0's
  offset in the `.bones` file** to the decimal. Two different files agreeing.
- `51 bones × 20 + 16 = 1036`, which is the header's own `frameSize`.

**The rest pose does not live in `.bones`.** Bone axes point along local X — a
Character Studio convention — so with identity rotations a skeleton unrolls into
a line 146 units long instead of standing up. The pose comes from a frame.

---

## 10. The two per-vertex blocks: they are per **triangle**

This is what `num_verts` in the header actually counts. It is **not** a vertex
count — it is a triangle count, and it matches the `.meshset` face count for the
same character exactly. Kano: 1,602 in both, against 844 real vertices.

```
vert_extra   6 bytes    three uint16 indices into the skinned positions
vert_data   24 bytes    three UV pairs, one per triangle corner
```

Which also explains the loader's `skin_uvs` tag on a 24-byte block — a UV pair
needs 8 bytes, and this is three of them.

Verified across all 30 skin blocks: every index lands inside `num_matrices`, the
maximum is always exactly `num_matrices - 1` so no vertex goes unused, and
**there is not one degenerate triangle in the entire game**.

The UVs are the clincher. They are stored per corner, so a shared vertex appears
in several triangles — and it gets the same UV each time. Kano's triangles 0 and
1 both give index 4 the UV `(0.513, 0.142)` and index 6 `(0.521, 0.163)`. A
wrong layout does not produce that agreement.

With this, a character is fully specified: skinned positions from §7, topology
and UVs from here. [`tools/pose.py`](../tools/pose.py) renders one.

## 11. Characters have a rig **and** shape keys

Two animation systems, not one, and only the first is skeletal.

**The rig** is `.bones` + `.skinanim`, documented above: a real skeleton, 10 to
17 levels deep, one quaternion per bone per frame.

**The shape keys** live in the `.meshset` as numbered mesh sequences sharing a
stem — `SmokeBull001`…`SmokeBull014`, `Witch_SKIN001`…`Witch_SKIN040`. Across
the 29 characters there are **115 such sequences, and 80 of them have
byte-identical vertex and face counts**, which is what makes them morph targets
rather than separate models. Vertex deltas run 0.2 to 1.2 units.

| Character | Sequence | Frames | Verts | What it is |
|---|---|---:|---:|---|
| Mileena | `Witch_SKIN` | 40 | 1,149 | her transformation |
| Sindel | `sindel` / `hair` | 29 / 31 | 1,439 / 502 | the hair fatality, hair on its own track |
| Smoke | `SmokeBull` | 14 | 1,117 | animality |
| Nightwolf | `Raiden` | 11 | 1,573 | fatality |
| Cyrax | `NET` | 14 | 282 | the net special |
| Sub-Zero | `iceshower` | 13 | 91 | freeze effect |
| Jax | `jax_low` | 12 | 1,523 | body morph |
| Kano | `laser_` | 12 | 8 | eye laser |
| Noob Saibot | `ScorpionPenguin` | 12 | — | babality |
| Jade | `JadeCat` | 11 | — | animality |

The named one-offs follow the same pattern: `_BZZ` and `_BZZFreeze` for
electrocution and freezing, `_shell` for the shatterable husk, `RipSkel` for the
skin-rip, `SCMASKOFF` for Scorpion's mask.

**Why this matters for the animation work.** A skeleton cannot turn a man into a
bull, and it cannot swing hair convincingly. Anything the rig cannot express —
transformations, dismemberment, secondary motion, projectile effects — is done
by swapping whole meshes frame by frame. Any playback implementation needs both
paths.

### The link to `.events`

One sequence is called **`EVENT_bzz_smoke`** (Mileena, 17 frames, identical
topology). The `EVENT_` prefix ties morph sequences directly to the
[`.events` format](EVENTS-FORMAT.md), which is already solved to 545 of 545
files. That is the most promising thread for working out **what triggers a
sequence and when** — the piece still missing from playback.

## 12. What is still open

- What `word0` of a bone record counts.
- The fifth word of each animation frame entry.
- The fifth word of `BONEANIMFRAME`, copied with the quaternion but not
  consumed by anything decompiled so far.
- The output strides at the loop tail — 24, 48 and 6 bytes per vertex on three
  separate cursors. The 24-byte one carries position and colour; the other two
  are unaccounted for, and the 6-byte one is the likely home of the UVs the
  signature promises.

### One shortcut, tried and ruled out

Since `matricesA` holds the vertex once per influence and the weights are
already baked in, it is tempting to think `Σᵢ A[i]` might approximate the bind
pose without needing the palette at all. It does not: plotted as a point cloud
for Kano's 844 vertices it is a diffuse blob with no silhouette, because each
`A[i]` lives in **its own bone's local space** and summing across different
spaces is meaningless.

Cheap test, clear answer, and worth recording so nobody tries it twice. Posing a
character genuinely requires the matrix palette, which requires the bone tree
above plus rotations from a `.skinanim` frame.

The **layout** was already settled — every file in the game walks to its exact
last byte — and now the **interpretation** is settled too, for everything the
[mesh viewer](MESH-VIEWER.md) needs in order to pose a character.


---

## The rotation blend is not a slerp

`GetSlerpedQ` (armv6 `0x00083408`) is 144 bytes and its name is a lie. There is
**no `acos`, no `sin`, no division and no renormalisation** anywhere in it. What
it does is a dot product, a conditional sign flip, and a straight linear blend:

```
vcmpe.f32  s15, #0
vnegls.f32 s12, s12       ; dot <= 0 -> negate all four components
vnegls.f32 s11, s11
vnegls.f32 s10, s10
vnegls.f32 s9,  s9
vmul.f32   s15, s12, s13  ; then a plain lerp
vmla.f32   s15, s5,  s14
```

This is the cheap substitute every 2011 phone engine used, and it is not even
the normalised variant — **the result is not a unit quaternion**. A lerp between
two unit quaternions shortens as the angle widens, so rotation speed varies
through the blend and the resulting matrix shrinks slightly. At fighting-game
frame rates and blend windows nobody sees it, and the engine saves a
transcendental per bone per frame.

### Both directions are traps for a port

| Change | What happens |
|---|---|
| Substituting a real slerp | "Fixes" nothing visible, changes every in-between pose |
| Dropping the sign flip | Blends across a quaternion negation take the long way round — a limb snaps through the body |

Keep the flip, keep the lerp.

The comparison is `vcmpe` with the negate predicated `ls`, so **the flip fires at
`dot == 0` as well as below it**. Writing `< 0` instead of `<= 0` differs only on
exactly-orthogonal frames, which is precisely the kind of difference that is
never noticed and never reproducible.

### The blend direction, confirmed twice

`out = b*t + a*(1-t)` — the same reversed convention `LerpVector3` uses, so
**`t = 0` returns the second argument**. Two independent functions now agree on
it, and it is still the opposite of what the argument order suggests. This has
already cost the project once; it is written down here so it does not cost it
again.
