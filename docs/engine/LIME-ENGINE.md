# The LIME engine

EA Mobile's in-house 3D engine, as used by the 2011 iOS build of UMK3. This is
the consolidated reference; each claim says how it is known.

- **verified** — passed a mechanical test (differential run, corpus walk, or
  disassembly of the shipped binary)
- **derived** — read off the disassembly, not independently tested
- **unconfirmed** — hypothesis

---

## Identity

The engine splits into `lime/common` (portable, 109 functions) and
`lime/iphone` (platform, 229 functions) — the natural seam for a port. The
build path survives in the binary **[verified]**:

```
/BuildServerX/reactive/mortalkombat_iphone/xcode/umk3_iphone_en/../../src/lime/
```

What "LIME" stands for, and whether other EA Mobile titles used it, is
**unconfirmed**. So is what the `DS_` / `LIMEDS_` prefix means.

### Naming conventions **[verified]**

`LIME_*` is the public API (`LIME_LoadMeshSet`, `LIME_LoadEvents`,
`LIME_LoadScene`, `LIME_LoadSkin`, `LIME_FindMeshByName`, …). `lime*` are
helpers (`limeMalloc`, `limeLoadFile`, `limeMatrixMult`, …).

**`limeMalloc(const char *tag, size_t size)` tags every allocation with a
readable string**, and those strings survive in the binary. `skin_indexes`,
`skin_mweights`, `meshset_meshes` and friends name the buffers for you. Reading
the tag at each call site is the cheapest way to name an unknown struct, and it
is how `.skin` was solved quickly.

---

## What compiled this, and why the code looks the way it does

From the shipped `Info.plist` **[verified]**:

| Key | Value | |
|---|---|---|
| `DTCompiler` | **4.2** | GCC 4.2 generation — **not clang** |
| `DTXcode` / `DTXcodeBuild` | 0400 / 4A1006 | Xcode 4.0 |
| `DTSDKName` | iphoneos4.3 | |
| `BuildMachineOSBuild` | 10J567 | Mac OS X 10.6 Snow Leopard |
| `MinimumOSVersion` | 3.0 | back to the original iPhone |

GCC 4.2 rather than clang matters when reading generated code: its ARM backend
has its own idioms, and that is the compiler to compare against when a sequence
looks strange.

### Why there is NEON in scalar maths

This is the explanation behind the project's biggest technical obstacle, and it
is not a quirk of EA's code.

On the **Cortex-A8** — the chip in the iPhone 3GS and iPhone 4 — the VFP unit
is *not pipelined*, while NEON is. Scalar float instructions therefore issue
one after another with no overlap, and **doing scalar maths with 2-lane NEON
was measurably faster even though one lane went to waste**. It was standard
practice at the time.

So `vmul.f32 d6, d6, d7` in `Matrix.cpp` and `limeVector.cpp` is the *normal*
output for this hardware, and Ghidra's failure to model it is a failure against
ordinary period code rather than against something exotic.

It also explains why the armv6 slice is clean: NEON arrived with Cortex-A8 and
armv7. The ARM11 chips that armv6 targets have VFPv2 and **no NEON at all**, so
the compiler had no choice but to emit scalar VFP there.

*(Source: Texas Instruments' wiki on using NEON and VFPv3 on Cortex-A8.)*

### Other period details that explain the binary

- **Soft-float ABI** — floats in integer registers. Already verified here, and
  the reason Ghidra emits `Unknown calling convention`: it expects hard-float
  with arguments in `s0`–`s15`.
- **Thumb by default** — Apple compiled for Thumb for code density, matching
  this binary being 100% Thumb but for two functions.
- **PVRTC** — the PowerVR compressed texture format every iPhone of the era
  used, hence `PVRTexture.m` and the `.pvr` assets.
- **Precomputed lighting** — per-vertex lighting was expensive on Cortex-A8 and
  PowerVR SGX, which is why LIME bakes it into `.lighting` files and the mesh
  loader discards vertex normals.
- **armv6 + armv7 in one binary** — standard practice to cover the original
  iPhone through the iPhone 4 from a single `.ipa`.


---

## The two slices are two different compilers' worth of information

The fat binary carries **armv6** (2,653,792 bytes, at offset `0x1000`) and
**armv7** (2,348,400 bytes, at `0x289000`). The project works on armv7, and
that turns out to have been costing us.

ARMv7 has NEON, and EA's compiler used **2-lane packed NEON to do scalar float
maths**. Ghidra models those as opaque vector intrinsics and loses the
arithmetic entirely — its `_Len` returns an uninitialised variable and still
compiles. See [METHODOLOGY.md](METHODOLOGY.md).

**ARMv6 has no NEON.** The armv6 slice is a second, independent compilation of
the same source in plain scalar VFP:

```
armv7 (NEON — Ghidra loses this)   armv6 (scalar VFP — decompiles correctly)
  vldr      s12, [r0]                vldr      s15, [r0, #4]
  vldr      s14, [r0, #4]            vldr      s13, [r0]
  vmul.f32  d6, d6, d6               vldr      s14, [r0, #8]
  vmul.f32  d7, d7, d7               vmul.f32  s15, s15, s15   ; y*y
  vldr      s10, [r0, #8]            vmla.f32  s15, s13, s13   ; += x*x
  vadd.f32  d6, d6, d7               vmla.f32  s15, s14, s14   ; += z*z
  vmul.f32  d7, d5, d5               vsqrt.f32 s15, s15
  vadd.f32  d7, d6, d7               vmov      r0, s15
  vsqrt.f32 s14, s14                 bx        lr
  vmov      r0, s14
  bx        lr
```

The armv6 column is `sqrtf(x*x + y*y + z*z)` written plainly. **[verified]**

`python tools/slices.py neon <armv7> <armv6> <func-to-file.txt>` measures the
reach:

| | functions using packed `.f32` on D/Q |
|---|---:|
| armv7 | 153 |
| armv6 | 58 |
| **armv7 only** | **107** |

Those 107 are readable in armv6 and not in armv7. By file:

| File | Functions | | File | Functions |
|---|---:|---|---|---:|
| `FrontEnd.cpp` | 39 | | `LIMEDS_Misc.cpp` | 3 |
| `GameCode.cpp` | 22 | | `Players.cpp` | 3 |
| `RenderSkinned.cpp` | 7 | | `mkzap.c` | 3 |
| `lime.m` | 6 | | `Events.cpp` | 2 |
| `Matrix.cpp` | 4 | | `limeVector.cpp` | 2 |
| `limeFont.cpp` | 4 | | `RenderMesh.cpp` | 2 |

**Two different numbers, often confused.** **32 of the 109** engine functions
(29%) use packed NEON; **25** of those (23%) are the ones armv6 fixes, the
other 7 using it in both slices. The long-quoted "29 of 109, 27%" was
essentially right about the affected set — a correction here that reported 23%
had measured the fixable subset instead.

**And the engine is not where most of it is.** `FrontEnd.cpp` and
`GameCode.cpp` together account for 61 of the 107 fixable cases — more than
half the problem is in game code, not in `lime/common`.

### Two traps when reading the armv6 slice

1. **Much of armv6 is ARM, not Thumb.** `_Len` is Thumb in armv7 and ARM in
   armv6.
2. **The Thumb flag is `N_ARM_THUMB_DEF` (`0x0008`) in `n_desc`**, not bit 0 of
   the symbol value — and in this binary it is set on the STABS entry (type 30)
   and *not* on the plain one (type 36). Reading only one of them disassembles
   Thumb as ARM and produces confident garbage. `tools/slices.py` ORs the flag
   across every entry for an address.

---

## Maths conventions — all **[verified]**

Through the differential oracle: 40,006 cases on `Matrix.cpp`, 20,013 on
`limeVector.cpp`, zero divergences.

- 4×4 float matrices, **row-major**
- `limeMatrixCopy(src, dst)` — source first
- `limeMatrixMult(a, b, out)` — output third (`r2`)
- `limeScaleMatrixXYZ` scales **columns**, not rows
- `limeMatrix3x4RotateSkin(m, vin, vout)`: `out[j] = Σᵢ vin[i]·m[i*4+j]`, no
  translation; `RotVector` is a tail-call alias
- `RotMatrixX`: `m[5]=cos, m[6]=sin, m[9]=-sin, m[10]=cos`
- `RotMatrixY`: `m[0]=cos, m[2]=-sin, m[8]=sin, m[10]=cos`
- `RotMatrixZ`: `m[0]=cos, m[1]=sin, m[4]=-sin, m[5]=cos`
- `CreatePerspectiveMatrix(m, fov, aspect, zNear, zFar)` (`0x0005e0a0`) — fifth
  argument on the stack. `f = sin(fov)/(1−cos(fov)) = cot(fov/2)`, computed in
  **double** and narrowed at the end. `fov` is the full **vertical** field of
  view. **`aspect` divides the X term only — the single widescreen hook.**

**AAPCS soft-float**: floats travel in integer registers (float in `r0`, double
in `r0:r1`). Ghidra does not know this and emits
`/* WARNING: Unknown calling convention */`.

---

## In-memory structures

| Struct | Size | Known fields | |
|---|---:|---|---|
| `MESHSETINFO` | 76 | `char name[64]`, `texturesLoaded`@0x40, `numMeshes`@0x44, `MESHINFO **meshes`@0x48 | verified |
| `MESHINFO` | 88 | `numVerts`@0x00, `numFaces`@0x04, `boundsRadius`@0x10, `verts`@0x18, `indices`, `vertLight` | verified |
| `LIMEVERTEX` | 16 | `int16 x,y,z` + 2 undefined bytes + `float u,v`; 26 on disk | verified |
| `SKININFO` | 48 | `next`@0 — linked list of at most two | verified |
| `BONESINFO` | 8 | `bones*`, `numBones` | verified |
| `BONE` | 56 | 25 on disk | verified |
| `SCENEEVENTS` | 8 | count@0, tracks@4 | verified |
| `SCENEEVENTTRACK` | 216 | `numEntries`@0x00, flag@0x04, entries ptr@0xd4 | verified |

`SKINMATRIX43`, `BONEANIMFRAME`, `MATRIX43`, `limeVECTOR3` and `limeVECTOR2`
show up in the mangled names but are not declared yet. Adding them to
`signatures/structs.txt` should pay off the way `MESHINFO` did.

---

## Asset formats

Every format needed to draw an animated character is solved. See
[MESHSET-FORMAT.md](MESHSET-FORMAT.md), [SKIN-FORMAT.md](SKIN-FORMAT.md) and
[EVENTS-FORMAT.md](EVENTS-FORMAT.md).

| Format | Status | |
|---|---|---|
| `.meshset` | ✅ 3 variants, 7,327 meshes byte-for-byte | verified |
| `.lighting` | ✅ path is `STATICLIGHTING/<name>.lighting` | verified |
| `.skin` | ✅ 29/29 exact | verified |
| `.bones` | ✅ 27/29 (`4 + N*25`) | verified |
| `.skinanim` | ✅ 28/29 | verified |
| `.events` | ✅ 544/545, `268 + N*56` | verified |
| `.scene` | ⬜ `LIME_LoadScene` (`0x0005f0ac`) | — |
| `frames.x`, `moves_data.x` | ⬜ game data, not LIME | — |

Two semantic questions remain open even where the layout is settled: what the
24 and 6 bytes per vertex in `.skin` contain, and why its `indexes` decode
negative.

---

## Render

Double renderer: `ES1Renderer.m` (GL ES 1.1) and `ES2Renderer.m` (GL ES 2.0),
using Apple's standard template that tries ES2 and falls back to ES1. Textures
are PVRTC.

**Per-vertex lighting is precomputed** in the `.lighting` files, which is why
the mesh loader discards vertex normals — the last 12 bytes of each 26-byte
on-disk vertex.

For the native port, **start from `ES2Renderer`**: ES 2.0 is already
programmable and maps almost 1:1 to OpenGL 3.3 core, whereas ES 1.1's fixed
pipeline would have to be emulated wholesale. touchHLE only implements ES 1.1
and returns `nil` for `kEAGLRenderingAPIOpenGLES2`, which is why the game falls
back to ES1 *there* — that is an emulator limitation and must not dictate the
port's architecture.

---

## Audio is not EA's code

**`lime/iphone/Finch/` is a vendored copy of
[zoul/Finch](https://github.com/zoul/Finch), an open-source OpenAL sound engine
under the MIT licence. [verified]**

All seven source files appear in the binary's STABS paths, and all seven
classes are present with the pre-refactor names:

| Class | Methods | | Class | Methods |
|---|---:|---|---|---:|
| `Finch` | 11 | | `Reporter` | 5 |
| `Sound` | 15 | | `Decoder` | 2 |
| `Sample` | 14 | | `PCMDecoder` | 2 |
| `RevolverSound` | 6 | | | |

55 Objective-C methods plus the `_FinchEngine` symbol — 56, matching the count
attributed to `Finch/` in the source tree. The upstream project today uses `FI`
prefixes (`FISound`, `FISoundEngine`), a later refactor, so the version
vendored here is from roughly 2010.

**Consequence: 56 of the 229 platform-layer functions (24%) do not need
reverse engineering.** The upstream source is readable, documented and legally
reusable. Match the class names against the repository history to find the
contemporary commit and use it as the port's audio backend.

Provenance is clean: public repository, MIT licence, unrelated to any leaked
source.

### The renderers are Apple's sample code

`ES1Renderer.m` and `ES2Renderer.m` are Apple's `GLES2Sample` template,
essentially unmodified. The method sets match exactly **[verified]**:

| Class | Methods in the binary |
|---|---|
| `ES1Renderer` (4) | `init`, `render`, `resizeFromLayer:`, `dealloc` |
| `ES2Renderer` (8) | those four plus `compileShader:type:file:`, `linkProgram:`, `loadShaders`, `validateProgram:` |

That is another **12 functions that need no reverse engineering** — Apple
publishes the source.

It also resolves a loose end in the import table. Both spellings of the
framebuffer calls are imported — `glGenFramebuffers` *and*
`glGenFramebuffersOES`, five such pairs. Not a curiosity: Apple's ES 1.1
template uses the `OES` extension names and the ES 2.0 path uses the core
names, so each renderer brings its own set.


### `GBMusicTrack.m` — **unconfirmed**, and do not assume it is reusable

11 Objective-C methods in the binary (`initWithPath:`, `play`, `pause`,
`setGain:`, `setRepeat:`, `readPacketsIntoBuffer:`,
`playBackIsRunningStateChanged`, `callbackForBuffer:`,
`postTrackFinishedPlayingNotification:`, `close`, `dealloc`).

The name and method set match a compressed-music player built on
AudioToolbox/AudioQueue that circulated widely on iOS game-development forums
around 2009–2010. **But no canonical repository or licence could be found**,
which puts it in a different category from Finch: it is probably third-party
code, and that is a reason to *understand* it quickly, not a licence to copy
it. Treat the 13 functions attributed to this file as still needing a native
reimplementation until someone identifies the source and its terms.

### How much of the platform layer is actually ours to write

`lime/iphone` is **229 functions [verified]**, and two blocks of it are not
EA's code:

| Module | Functions | |
|---|---:|---|
| `Finch/` | 56 | zoul/Finch, MIT — confirmed |
| `ES1Renderer.m` + `ES2Renderer.m` | 12 | Apple `GLES2Sample` — confirmed |
| **Left to write** | **161** | |
| `GBMusicTrack.m` | 13 | unconfirmed; counted in the 161 for now |

**68 of 229 — 30%.** A wider claim has been made for this, counting
`Reachability.m` and the `SBJson*` family and arriving at "over 60%". Those are
real third-party libraries, but **they are not in the platform layer**:
`Reachability.m` (12 fn) lives in `EA_SDK/support` and `SBJSON.m`,
`SBJsonParser.mm` and `SBJsonWriter.mm` (43 fn) live in `JSON/`. Both belong to
the EA SDK tree, which this port deletes or stubs wholesale — recognising them
as third-party changes nothing, because none of it was going to be written
anyway.

The operational rule survives the correction intact, and it is the cheapest
scope reduction available: **before decompiling any platform module, search the
class name together with "iOS 2010". If a known library or Apple sample turns
up, there is nothing to reverse — find the period version and read it.**

---

## Two execution models in one binary

Worth not confusing:

- **LIME is ordinary C++.** Normal calls, normal stack. The differential oracle
  works here — it is what validated `Matrix.cpp` and `limeVector.cpp`.
- **`gamecode/logic` is not.** It is cooperative multitasking with per-process
  stacks and stack jumping, inherited from the arcade original's TMS34010.
  **The oracle cannot follow it, by design rather than for want of work** — see
  [METHODOLOGY.md](METHODOLOGY.md).

  This is established from our own binary, not from anyone's reading of the
  arcade source: **1,172 `t_` functions, 139 `q_`, 105 `c_`**, plus
  `_reset_proc_stack`, `_UnstackSwitches`, `_stack_switch_bits`, `_DoSwitchJump`
  and `_SwitchQueue`. Names circulating in third-party notes — `process_sleep`,
  `proc_switch_counter` — **do not appear in this binary's symbol table** and
  are not used here.

---

## Still unknown

- What "LIME" stands for; whether other EA Mobile titles used it
- What the `DS_` / `LIMEDS_` subsystem is
- The scene graph (`.scene`)
- What the 24 and 6 bytes per vertex in `.skin` hold
- Whether `.events` really is a bone/slot-anchored effect system — the
  `UNASSIGNED` slot names and RGBA colours suggest it, but nothing confirms it
- The provenance and licence of `GBMusicTrack.m`

---

## The GL surface the port has to implement

Measured, not guessed: [`tools/glsurface.py`](../tools/glsurface.py) walks every
named function in the armv6 slice and counts calls that land on a GL import
stub. **77 distinct entry points**, all OpenGL ES 1.1 fixed function plus the
`OES` framebuffer extensions.

This is the concrete specification for the native platform layer. Anything not
on this list does not need writing.

| Source file | Calls | Distinct | What it drives |
|---|---:|---:|---|
| `lime.m` | 95 | 23 | the iOS platform layer — context, clears, viewport, 2D |
| `GameCode.cpp` | 78 | 12 | scene transforms: cull, scale, translate, rotate, matrix stack |
| `RenderMesh.cpp` | 71 | 18 | mesh drawing: pointers, texture units, `glDrawElements` |
| `LIMEDS_Misc.cpp` | 15 | 6 | matrix stack helpers |
| `FrontEnd.cpp` | 14 | 6 | menus |
| `Players.cpp` | 11 | 6 | per-player transforms |
| `RenderScene.cpp` | 11 | 6 | scene-graph placement |
| `Events.cpp` | 8 | 4 | event-driven transforms |
| `HudAnim.cpp` | 4 | 4 | HUD |

The heaviest single calls across the binary are `glScalef` (24 sites),
`glDisable` (23), `glEnableClientState` (22), `glEnable` (21) and `glTranslatef`
(20) — a fixed-function pipeline driven almost entirely through the matrix stack
and client-state toggles, with **three texture units** in `RenderMesh.cpp`.

There is no shader anywhere. Mapping this to modern OpenGL means writing a small
fixed-function emulation — a matrix stack, a texture-env combiner, and vertex
array bindings — rather than porting shader code, which is why
[the ES2 renderer path is the better starting point](#) for the port even though
the engine itself is ES1.

### The bug that made this tool worth writing twice

The first version reported **one** GL call in the entire binary. It passed bare
`0x...` addresses to `disasm`, which assumes Thumb for a numeric target — and
the armv6 slice is ARM throughout, so it disassembled garbage and found nothing.
Garbage that produces a plausible-looking empty result rather than an error is
the failure mode this project keeps meeting. Disassembling by *name*, which
carries the correct instruction-set flag, gives 77.


---

## There is no shader path. The ES2 renderer is dead template code.

The binary imports **twelve GL ES 2.0 shader entry points** — `glCreateShader`,
`glCompileShader`, `glLinkProgram`, `glUseProgram`, `glVertexAttribPointer` and
friends — and it contains an `ES2Renderer` class with a
`compileShader:type:file:` method. Taken at face value that looks like a second,
programmable render path worth investigating.

It is not. **Nothing ever runs it**, and four independent checks say so:

| Check | Result |
|---|---|
| Shader source strings in the binary | **none** — no `gl_FragColor`, `gl_Position`, `attribute`, `uniform`, `varying`, `precision` |
| `.vsh` / `.fsh` / `.glsl` files in the app bundle | **none shipped** |
| What `compileShader:type:file:` reads from | a **file** — which does not exist |
| Fixed-function calls, by contrast | `glBindTexture` ×19, `glVertexPointer` ×14, live throughout `lime/common` |

`ES1Renderer` and `ES2Renderer` are both present because that is exactly what
Apple's "OpenGL ES Application" Xcode template shipped in 2010–2011: two
renderer classes and a runtime pick between them. EA kept the template, wrote
the game against fixed function, and left the ES2 half unused. The shader
imports are in the symbol table because the app links the whole OpenGLES
framework, not because anything calls them.

**So: do not decompile `ES2Renderer` hoping to find the real renderer.** It is
boilerplate. The engine is GL ES 1.1 fixed function, and that claim is now
proven rather than assumed.

This does not change the recommendation for *our* port, which is a separate
question — targeting a programmable pipeline is still the right call for
desktop GL, because emulating the fixed-function pipeline by hand is the larger
job. The point here is only that the original binary offers no shader code to
start from.

### How to check calls like this yourself

`tools/stubs.py` resolves the 733 import stubs to names, which is what turned a
wall of `bl #0x127xxx` into readable calls:

```bash
python tools/stubs.py OUTPUT/armv6/UMK3.armv6 --grep gl
```

**Always scan with a control group.** The first version of the call-site scan
reported zero shader callers — and also zero `glBindTexture` callers, which were
sitting in a disassembly already read by eye. The scanner was mixing file
offsets with virtual addresses; the control group caught it, and the "no shader
callers" result would otherwise have been published for the right conclusion
via broken arithmetic. A negative result that cannot detect a known positive is
not evidence of anything.

---

## The DS Ultimate Mortal Kombat is not an ancestor of this build

Worth checking, because the engine invites it: functions are named `LIMEDS_*`,
`ConvertDSMatrixtoPCMatrix` scales by **1/4096** — the Nintendo DS 1.3.12
fixed-point format — and `QSTMATRIX` carries the handheld-era packing this
project keeps running into.

So a copy of **Ultimate Mortal Kombat (Nintendo DS, 2007)** was examined against
this build. It shares nothing.

| check | result |
|---|---|
| `LIME` / `lime` strings | the four "lime" hits are **`limei`** — the character Li Mei |
| `.meshset`, `.scene`, `.events`, `.bones`, `.skin` | **none present** |
| `/BuildServerX/` source paths | none |
| Asset formats | `.sec`, `.fsm`, `.xmb`, `.pbm`, `.pal`, `.vx`, `.sdat` — a different pipeline entirely |
| iOS animation frame names (`KNDUCK1`, …) | **0 of 355** appear |
| `moves_data`, `frames.x` | absent |

The roster gives it away on its own: `ending_limei.sec` and `body_kobra_alt.sec`
are Li Mei and Kobra, who are *Deadly Alliance* and *Deception* characters, not
the UMK3 arcade roster. The ROM is Midway's own DS engine carrying an arcade
port plus Puzzle Kombat, published four years before EA Mobile's iOS build.

**`LIMEDS` means EA's LIME engine targeting the DS**, which is a different
lineage from Midway shipping a DS game. The 1/4096 constant tells us the format
LIME's earlier target used; it does not point at this cartridge.

So: it cannot resolve the open field layouts, and it is not a second source for
the arcade data tables in issue #11. The arcade reference remains MAME and our
own symbol table. Checking cost twenty minutes and is recorded here so it costs
nobody else the same.

