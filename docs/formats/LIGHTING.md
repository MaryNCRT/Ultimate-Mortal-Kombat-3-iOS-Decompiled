# Lighting

There is no lighting hardware in play. The engine targets **OpenGL ES 1.1 fixed
function** -- [77 entry points, no shaders anywhere](LIME-ENGINE.md) -- and it
never enables GL lighting. Everything below is computed on the CPU, per vertex,
and handed to GL as vertex colour.

Recovered from the **armv6 slice**, where the same source compiles to plain
scalar VFP. C in [`decomp/lime/RenderSkinned.c`](../decomp/lime/RenderSkinned.c).

---

## The whole model, in one function

`LightVert` (armv6 `0x00083ab8`) is the entire lighting model of the game:

```c
l = 0.0f;                                   /* no ambient */
d = -dot(N, L0);  if (d < 0) d = 0;
l += power0 * pow(d, exp0);
d = -dot(N, L1);  if (d < 0) d = 0;
l += power1 * pow(d, exp1);
if (l > 1.0f) l = 1.0f;
```

**Two directional lights. No ambient. A power falloff on each. Clamped to 1.**

### Three things that are easy to get wrong

**There is no ambient term.** The accumulator starts at a literal `0.0f` --
verified as `0x00000000` at `0x83c00`. A surface facing away from both lights is
fully black, with nothing to lift it.

**The dot product is negated.** `-dot(N, L)` means the stored light vectors
point *from the surface toward the light*, the opposite of the usual convention.
Reimplementing this with the sign the other way lights the model inside out.

**Each light is raised to a power**, through a genuine `libm` `pow()` call --
two of them, per vertex, per frame. On a 2011 iPhone that is a striking choice,
and it is what gives these characters their hard, almost rim-lit falloff rather
than a soft Lambert ramp. A port that substitutes a plain `max(0, dot)` will
look visibly wrong.

### The result is monochrome

`LightVert` returns **one float**, and `DrawSkinnedMesh2` writes it as
`R = G = B` with alpha `0xFF`. Lighting is therefore a **grey multiplier over
the texture** and can never tint. Every colour a character has comes from its
diffuse map.

## The lights themselves, with their actual values

`NormaliseLDirs` (armv6 `0x00083c38`) makes both directions unit length once, at
startup, computing the reciprocal square root in **double** and narrowing to
float -- the same accuracy habit as `CreatePerspectiveMatrix`.

The globals sit in one contiguous block at `0x001bb874`, and these are the
values the binary ships with:

| | direction | exponent | power |
|---|---|---:|---:|
| light 0 | `(68, 462, 247)` | 0.8 | **0.0** |
| light 1 | `(0, -300, -50)` | 3.5 | 2.0 |

Two things fall out. The directions are **not unit length** -- they are authored
in world units, like positions, which is exactly why `NormaliseLDirs` has to
exist. And **light 0 ships with a power of zero**, so as initialised it
contributes nothing: only light 1 is doing any work. Either the second light is
switched on at runtime or it is a disabled experiment; the initialiser alone
cannot tell them apart.

There are no point lights, no spot lights and no attenuation.

## Shadows: one drop shadow, and nothing else

The engine has no shadow volumes, no shadow maps and no projected-texture
shadows -- there is not a single shadow texture or shadow mesh in the game's
assets. What it does have is a **single character drop shadow**, and the
evidence is three separate traces:

- **`Shadow HeightFrom Ground`** -- an entry in the in-game debug menu, sitting
  between the camera margin tweakables. So the shadow's height above the floor
  is a tuned constant someone was adjusting live.
- **`_ShadowOffset`** -- a global named alongside `_SceneX`, `_SceneY`,
  `_SceneZ`, `_SceneScale` and `_CamTrackToPlayer`, i.e. in the scene placement
  family.
- **`_clear_shadow_bit`** (`0x00054f60`, `gamecode/logic/other.c`) -- a
  per-player state flag, in the same family as `_set_inviso`, `_half_damage`,
  `_set_quarter_damage` and `_set_noscroll`. So a fighter can have its shadow
  turned off as a gameplay state, not just a rendering one.

With no shadow asset anywhere, the shadow is drawn procedurally -- most likely a
flattened copy of the character projected to a fixed ground height, which is
what "height from ground" as the only tunable implies.

## `.lighting`: baked per-vertex light, still encoded

**341 `.lighting` files ship, not thirteen.** An earlier version of this page
said thirteen because it only listed the top of `res/`; the other 328 live in
**`res/STATICLIGHTING/`**, a directory nobody had opened, alongside 455 `.lights`
files -- 796 entries and 20.7 MB in total.

The thirteen at the top level are the per-character animation bakes analysed
below. The `STATICLIGHTING/` entries are per-object: of 499 distinct stems,
**463 match a `.meshset` and a `.scene` by name**, so they are baked lighting for
individual scene objects rather than for animated characters.

The thirteen character bakes cover 13 of the 29 fighters, consistent with the
palette-swap characters sharing a rig and therefore a bake.

Their sizes scale with **vertices x animation frames**, at roughly 1.3 to 2.0
bytes each:

| Character | bytes | verts | frames | bytes per vertex-frame |
|---|---:|---:|---:|---:|
| Ermac | 919,240 | 1,292 | 356 | 1.999 |
| Scorpion | 869,768 | 1,278 | 344 | 1.978 |
| Liu Kang | 568,856 | 829 | 347 | 1.978 |
| Shao Kahn | 282,310 | 1,797 | 122 | 1.288 |

That is the shape of **precomputed per-vertex lighting for every frame of
animation** -- a prelight bake. The ratio is not constant, so there is a header,
padding, or compression involved.

**The encoding is not decoded.** The bytes are heavily skewed -- 62% zeros, 7%
`0xFF`, with small values dominating the remainder and all 256 present -- which
looks like delta coding or a simple compression rather than a flat array. Saying
more than that would be guessing.

If the bake is what it appears to be, then `LightVert` is the *fallback* path
and most characters are lit from the table instead. Establishing which runs when
means decompiling the loader, and is the natural next step here.

## Where else lighting shows up

- **`.lighting` files** ship next to `.meshset`s and are documented in
  [MESHSET-FORMAT.md](MESHSET-FORMAT.md). They are per-mesh, and the loader
  reads them alongside geometry.
- **`IsTextureFullBright`** and `IsTextureFullBrightPath` in `RenderMesh.cpp`
  mark textures that bypass shading entirely -- the fixed-function equivalent of
  an unlit material. Not yet decompiled.
- **`CreateFadedLookupTable`** and `CreateFadedRGBS` in the same file build a
  precomputed fade ramp. Not yet decompiled, and the obvious next thing to read.
- **`glShadeModel`** appears once, in `RenderMesh.cpp`, so smooth versus flat
  shading is selected per draw.

## What this means for the port

The good news for a native rewrite is that **none of this needs a shader to
reproduce**, and none of it depends on fixed-function GL lighting. It is
arithmetic over a normal, and it can be lifted verbatim into a vertex shader or
kept on the CPU exactly as it is.

The trap is the two unusual choices above -- the negated dot and the `pow()`
falloff. Both are the kind of thing a reimplementation tidies away by accident,
and both change how every character in the game looks.

---

## The opt-out list is a text file the artists edited

Some surfaces must not be lit at all, and the engine decides which ones by
**reading a plain text file at runtime**. `IsTextureFullBright`
(armv6 `0x0008125c`) loads `res/nolight.txt`, and the file explains its own
format in its header:

```
# List of files that we want to be fullbright below
# comment out files using the # character as the first char in a line
# Always use .??? as the file extension, no directories
```

Forty entries ship in the retail build. Almost all of them are
`<CHARACTER>_DIFFUSE_ICE` -- the frozen variant of every fighter on the
roster -- and the rest are blood splats, vomit, the acid of Reptile, and the
snowman. Which is exactly the right list: a frozen body glows from inside, and
blood should not pick up a directional highlight.

**This is the most moddable thing found in the engine so far.** Adding a line
to a text file changes how a texture is lit, with no rebuild and no tooling.
Our port must read this file rather than bake the list into code, or that
capability is lost in translation.

### How the lookup works

| | |
|---|---|
| Cache | `FullBrightLoaded` guards a one-time parse; the buffer is freed after |
| Parse | `GetNextLine` per line; first byte `#` **or** `\0` means skip |
| Table | stride `0x40`, name at `+4`, count at the base -- 60 bytes per name |
| Lookup | linear `strstr` — a **substring** test, not equality; 40 entries is not worth indexing |
| Globals | `_FullBrightLoaded` (`__DATA,__data`), `_TheFullBrightInfo` (`__DATA,__common`) |

Both global names are in the symbol table verbatim, so the caching and the table
are original design rather than inference.

**Correction.** An earlier version of this page listed `_IsTextureFullBrightPath`
as a third global and said the file path was configurable, so a mod could point
it elsewhere. That was wrong. The symbol lives in `__TEXT,__text` — it is a
**function**, a thin wrapper that strips a directory prefix before calling
`IsTextureFullBright`. The filename is a literal and is not configurable.
Checking which section a symbol lives in takes one grep, and it is the
difference between a documented fact and an invented feature.

### The `.???` convention, settled

This was left as an open question when the function was first read, on the
assumption the call was a `strcmp`. Resolving the import stubs corrected it: the
call is **`strstr`**, a substring test.

`GetNextLine` stores each line verbatim — it copies until NUL, CR or LF — so
`.???` really is in the table. For `strstr(name, entry)` to match, the texture
names passed in must contain `.???` literally, and they do: **591 of the 605
shipped `.meshset` files contain that string**. Texture names are stored inside
the geometry with the wildcard extension already in place, which is why the
header of `nolight.txt` instructs artists to write them the same way. Nothing
normalises anything at runtime — the convention is baked into the data on both
sides.

One consequence for modders: because it is `strstr`, **an entry matches any
texture name containing it**. Putting `ICE` in the file would unlight every
texture with `ICE` anywhere in its name. The shipped file always writes complete
names so the behaviour never surfaces, but it is the difference between a list
of names and a list of patterns.


---

## …and then GL is told not to interpolate it

`LIME_RenderMeshSingleIndexed` opens with `glShadeModel` and the constant it
loads is `0x1d01` — **`GL_FLAT`**.

So the model above is computed **per vertex on the CPU** and then displayed
**per face**: each triangle takes the colour of its provoking vertex and GL
interpolates nothing. That is why the shading in this game reads as faceted
rather than soft, and it is not a limitation of the lighting maths — it is a
deliberate second decision on top of it.

A port that leaves the GL default (`GL_SMOOTH`) in place gets rounder, softer
shading than the original on every surface. It looks better in a screenshot and
it is wrong.

The vertex colours themselves come from `CreateFadedRGBS`, which feeds
`glColorPointer` in the same function — so the signed per-channel offset that
routine applies lands on **vertex colours, not texels**.
