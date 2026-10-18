# Bugs in the shipped game

A port can be better than the original. This is the list of defects found in
EA's 2011 release that the native port should fix, with the evidence for each.

Everything here was found mechanically, by cross-checking what the data
references against what the data contains. None of it required running the game.

---

## Broken texture references

**Nine distinct texture names are referenced by meshes and do not exist in the
game.** They are art-pipeline typos: misspellings, truncations, and one stray
3ds Max "- Copy". In every case the *correct* texture ships — so these are
one-word fixes, not missing assets.

| Broken reference | Uses | The texture that actually ships | Match |
|---|---:|---|---:|
| `SINDEL_DIFF` | 25 | `SINDEL_DIFFUSE` | 88% |
| `CYRAX_DIFFUSE - COPY` | 19 | `CYRAX_DIFFUSE` | 79% |
| `SONIA DIFF` | 3 | `SONYA_DIFFUSE` | 70% |
| `NIGHTWOLF_DIFF` | 2 | `NIGHTWOLF_DIFFUSE` | 90% |
| `KITANA_DIFUSE` | 1 | `KITANA_DIFFUSE` | 96% |
| `WINDOW_GLOW` | 1 | `WINDOWGLOW2` | 91% |
| `PLAYERPORTRAITS` | 1 | — portrait atlas | 76% |
| `MKFIGHT_MESSAGES` | 16 | **nothing similar ships** | — |
| `WINDOWS` | 1 | uncertain | 67% |

`SONIA DIFF` is worth pausing on: the artist misspelled the character's name
*and* truncated "DIFFUSE", in a texture slot, and it shipped.

`NOTEXTURE` is excluded from the list — it is referenced 1,568 times and is the
engine's intentional placeholder. It has [its own defect](../../issues/7): the
fallback path builds a malformed filename, so the placeholder itself never
loads.

### What it looks like

![Sindel's morph target, shipped versus fixed](img/bug-sindel-texture.png)

<sub>Left: `sindel`, the base mesh, correct. Centre: `sindel001`, a morph target
of the same body, **as the game ships it** — untextured. Right: the same mesh
with `SINDEL_DIFF` corrected to `SINDEL_DIFFUSE`.</sub>

The base mesh names its texture correctly; **the morph sequence does not.**
Sindel's morph targets are her hair-whip fatality, so in the original game her
body loses its texture partway through her own finisher. Twenty-five meshes are
affected.

Cyrax is the same story with `CYRAX_DIFFUSE - COPY` across 19 meshes, in
`CUTUP1_CYRAX` — a fatality scene.

### How the port should fix it

Resolve texture names case-insensitively, then fall back to the nearest shipping
name when the exact one is missing. A fixed correction table is safer than fuzzy
matching at runtime, and the table is short — the nine rows above.

---

## Dead data: SINDEL's animation stream is stored twice

`SINDEL_STANDARD.skinanim` contains **844 frames while its header declares
422**. The two halves differ in 47 bytes out of 530,032, so they are two
near-identical takes rather than a duplicated buffer.

This project could not previously say which count was authoritative. The
[`.events` frame indices settle it](EVENTS-FORMAT.md): her highest event index
is 416, which is 98.6% of the declared 422 — exactly where every other
character's sits — and only 49.3% of 844, which would make her the sole outlier
in the corpus.

**The header is right and the second half is never read.** It is roughly 265 KB
of an app that shipped on a 2011 iPhone, wasted.

---

## `DrawVortex3D` pushes a matrix it never pops

`DrawVortex3D` (armv7 `0x000078d8`) contains exactly **one `glPushMatrix` and
zero `glPopMatrix`** — counted across the whole disassembly of the function, not
inferred from reading up to the return.

```c
glPushMatrix();                     /* never popped */
glScalef(VortexScale, VortexScale, VortexScale);
... five RenderAMesh calls ...
limeEnableAlphaBlending_Basic();
```

This is the tower background, redrawn every frame while that screen is up. The
GL ES 1.1 modelview stack is only guaranteed to be 16 deep, so within a second
of gameplay the push starts failing with `GL_STACK_OVERFLOW`, and from then on
`glScalef` multiplies onto whatever matrix is current instead of onto a fresh
copy.

### Why nobody ever saw it

`_VortexScale` (0x00101754) is **exactly 1.0**. The matrix that leaks is the
identity, and compounding the identity changes nothing. The bug is real,
reachable and permanent, and it is invisible for the single reason that the one
value it would corrupt happens to be the neutral one.

### What the port should do

Add the `glPopMatrix`. But add it **deliberately**, and record that the original
did not have it — because a port that also gives `_VortexScale` a non-1.0 value
without adding the pop gets a vortex that grows or shrinks without bound, and
the cause would be very hard to find from the symptom.

Note this is the opposite discipline from `RenderAMesh` and `RenderPlayer`,
which both restore `glCullFace(GL_BACK)` unconditionally on the way out and
match every push with a pop. The engine's own drawing code is careful; this
front-end screen is not.

---

## "Reset all data" clears the save in memory but never writes it

`FE_Task_ResetAllDataConfirmation` (armv7 `0x00015cbc`) performs the factory
reset as four reset/write pairs — and one reset with no write:

```c
Reset_Stats();              Write_Stats();
Reset_SaveData();           /* nothing */
achievementsReset();        Write_AchievementsData();
ResetSettingsData();        Write_SettingsData();
Reset_PresetButtonData();   Write_PresetButtonData();
```

Every other subsystem is flushed to disk immediately. `savedata` is not.

### What the player sees

Nothing, most of the time. The in-memory state is correct, so the game behaves
as reset, and the next ordinary `Write_SaveData()` — end of a match, entering
survival, a ladder advancing — persists the cleared values. The bug only bites
if the app is killed between the reset and that next write, at which point the
old ladder progress, win streak and unlock flags come back.

That window is easy to hit on iOS, where the OS terminates backgrounded apps
without warning, and "I reset my data and it came back" is exactly the kind of
report that gets filed as "didn't actually tap confirm".

### What the port should do

Call `Write_SaveData()` after `Reset_SaveData()`. **Record that the original did
not**, because it is a behaviour change: a port that adds the write and a save
file that survives a reset are no longer bug-compatible, and if anyone is ever
comparing against retail this is one of the places they will differ.

The same function has a smaller asymmetry worth carrying across deliberately
rather than by accident: it sets `*Player2NumButtons = 5` and leaves
`Player1NumButtons` untouched, so a factory reset does not restore player one's
control scheme.

---

## Method

Both findings came from the same approach: take every name the data references
and check it against every name the data provides. `.events` track names came
out clean — **1,547 references, all resolving** — which is what makes the
texture failures stand out as real rather than as a parser artefact.

Reproduce with the checks in [`tools/`](../tools); the texture scan is a short
walk over every `.meshset`'s texture field against the file listing.


---

## The 256th transparent mesh locks the game

`AddToTranspMeshList` (armv6 `0x00081ab8`) collects transparent meshes into a
fixed 255-entry array. The bounds check is real, and this is what it does when
it fails:

```
add   r3, r4, #1
cmp   r3, #0xff
str   r3, [r5]
pople {r4, r5, r7, pc}      ; <= 255: return normally
b     #0x81b20              ; otherwise branch to ITSELF
```

The instruction at `0x81b20` **is** `b #0x81b20`. It branches to its own
address, with interrupts still enabled. So the 256th transparent mesh in a
single frame does not wrap the index, does not drop the mesh, and does not
crash — the game **hangs**, responsive to nothing, with the last frame still on
screen.

This is almost certainly a debug assert whose reporting half was stripped for
release, the same way `LIME_printf` and `RenderAxesLines` lost their bodies
while keeping their scaffolding. The check survived; the message that would have
told you why did not.

### Can it be reached on a real device?

Not established. Counting the transparent meshes a busy stage actually emits per
frame needs the render path traced end to end, which is not finished. What *is*
established is that the ceiling is 255, that the failure mode is a hard lock,
and that nothing between here and there reports anything.

### What the port should do

Raise the array and keep the check. A widescreen or higher-resolution port draws
**more** of a stage at once, so it moves toward this ceiling rather than away
from it. Deleting the check to be safe is the wrong instinct: if the limit can
be reached, the one thing worse than hitting it is hitting it silently.

---

## The name font's character table is copied by a hard-coded length

`Task_LoadGeneralData` (armv7 `0x00023910`) builds the font used for player
names and the on-screen debug text, then overwrites the code table that
`limeCreateFONT` had just read out of `namefont.ft2`:

```
ldr     r0, [r4, #0x48]     ; NameFont.codes, allocated for numGlyphs entries
movs    r2, #0x58           ; 88 bytes -- a literal
blx     _memcpy
```

`NameFont.codes` is allocated by `limeCreateFONT` with **one byte per glyph in
the .ft2 file**. The copy is 88 bytes regardless. A `namefont.ft2` carrying
fewer than 88 glyphs overruns a heap allocation by the difference, silently.

The shipped `namefont.ft2` evidently has at least 88, or the game would not
have survived its own QA — this is a latent bug, not an observed one. It
matters to the port for two reasons: an asset pipeline that rebuilds the font
can trip it, and so can a translation that ships a smaller name font.

The widening loop immediately after is the counter-example of how to do it —
it bounds itself on `NameFont.numGlyphs`, the value read from the file:

```c
for (i = 0; i < NameFont.numGlyphs; i++)
    NameFont.codesW[i] = NameFont.codes[i];
```

### What the port should do

Copy `min(sizeof NameFontCodes, NameFont.numGlyphs)` bytes, and say something
when the font is short rather than quietly drawing the wrong glyphs.

---

## The loading screen's percentage buffer only fits a positive number

`Task_LoadingScreen_DRAWSCREEN` (armv7 `0x0001d178`) formats the progress
number into an **eight-byte** stack buffer at `sp+0x20`, with the line count it
uses immediately after it at `sp+0x28`:

```
ldr     r2, [sp, #0x1c]     ; percent
add     r0, sp, #0x20       ; the buffer
cmp     r2, #0x64
it      ge
movge   r2, #0x64           ; clamped at the TOP only
blx     _sprintf            ; "%d%%"
```

Eight bytes is exactly enough for `"100%"`. The clamp guards the top end and
nothing guards the bottom: a negative percentage formats as up to twelve
characters and writes straight over the line count that the same function has
already used, and past it.

Whether a caller can pass a negative value is not established — the two callers
found so far compute a ratio of bytes loaded. It is a latent bug, and it is one
line of stack away from the kind of corruption that presents as an unrelated
crash much later.

### What the port should do

`decomp/gamecode/GameCode.c` declares the buffer as sixteen bytes and says why
in a comment. That is identical for every value the game actually passes and
not a stack overwrite for the ones it does not.

---

## The Karnage feed post is still written and still flagged as sent

`FE_Task_Karnage_Summary` (armv7 `0x0000ffbc`) does this every frame:

```c
feedPosted = 1;
usprintf(<128 bytes of stack at sp+0x40>, GameTextNoHeader(0x120), KarnageScore);
```

`feedPosted` is set unconditionally, and the buffer the message is formatted
into is **never read again** — no later instruction in the function touches
`sp+0x40`. The text is built and dropped.

This is not a crash, and it costs only a `usprintf` per frame. It matters
because `feedPosted` is a lie: anything that consults it to decide whether the
score was shared will believe it was, on every frame this screen is on, with or
without a network. The most likely history is a Facebook feed post whose
posting call was removed for the release while the string building and the flag
stayed.

### What the port should do

Delete both. They are recorded here so that deleting them is a decision taken
with the evidence rather than an unexplained divergence from the original.
