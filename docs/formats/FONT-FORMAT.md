# The font metrics format

Recovered from `limeCreateFONT` (armv6 `0x000af83c`) in the **armv6 slice**.
C in [`decomp/lime/limeFont.c`](../decomp/lime/limeFont.c).

A font is **two texture atlases and one metrics file**. The atlases are ordinary
textures loaded through `limeLoadTexture` with mode 2; everything structural
lives in the metrics file, and that file has **no magic number and no version
field**.

---

## Layout

```
offset  size            meaning
------  --------------  --------------------------------------------------
0       1               glyph count, low 8 bits
1       1               bit 0     : SIMPLE flag (stored inverted in the FONT)
                        bits 1..7 : glyph count high bits, read SIGNED then >>1
2       1               unidentified, kept at FONT+0x08
3       numGlyphs       character codes, one byte each

  --- if SIMPLE ---
        1               a single fallback advance for every glyph

  --- if not SIMPLE ---
        numGlyphs * W   metric array A          -> FONT+0x1c
        numGlyphs * W   ADVANCE WIDTH           -> FONT+0x20
        numGlyphs * W   metric array B          -> FONT+0x24

W is 1 or 2 -- see "The width is not in the file" below.
```

## The glyph count is packed across a stolen bit

```c
n = data[0] + (((int8_t)data[1] >> 1) << 8);
simple = ((data[1] & 1) != 0) ? 0 : 1;
```

The low bit of byte 1 is the flag, and the rest of the byte is the high half of
a **signed** count shifted down by one. Reading byte 1 as an unsigned count high
byte gives the right answer for every font under 128 glyphs and the wrong one
above it — which is a bug that passes every test on a Latin character set and
fails on Korean. The retail build ships Korean.

## The width is not in the file

The three metric arrays are read as **signed bytes or as halfwords depending on
an argument to `limeCreateFONT`**, not on anything in the data:

```
ldrsbeq  r3, [r5, r6]       ; narrow: signed byte
ldrhne   r1, [r5, r6]       ; wide:   halfword
addeq    r6, r6, #1
addne    r6, r6, #2
```

Both paths store `int16` at the destination. So **a metrics file does not
describe its own layout** — load it with the wrong flag and it parses cleanly
and produces garbage. Any external tool needs that flag from the call site.

## The code table exists at two widths

The character codes are read once as bytes into `FONT+0x48`, and widened in the
same loop into `FONT+0x4c` as `{code, 0}` pairs — the identical codes as 16-bit
values. `limeGetStringWidth` searches one or the other depending on whether the
string turned out to be UTF-16.

That matters because **the encoding is detected at runtime, from a `0xFF 0xFE`
BOM**, inside the string functions themselves:

```
ldrsb    r3, [sl]
cmn      r3, #1              ; == 0xFF ?
ldrsbeq  r3, [sl, #1]
cmneq    r3, #2              ; == 0xFE ?
```

Both encodings travel as `const char *`. A port cannot decide from the type.

## Two fallbacks worth keeping

| Field | Value | When |
|---|---|---|
| `FONT+0x2c` | read from the file | SIMPLE fonts: one advance for all glyphs |
| `FONT+0x10` | the constant **8** | glyph count is zero |

`FONT+0x10` is written before the file is even opened, so **a font that fails to
load still measures text**, at 8 units per character. Menus come out misaligned
instead of collapsing to a point. Worth reproducing: it is the difference
between a visibly wrong screen and an invisible one.

## Measurement is scaled, not summed

`limeGetStringWidth` accumulates `FONT+0x20` as integers, then converts once and
multiplies by `FONT+0x14`, a value passed in at creation:

```
vcvt.f32.s32 s14, s15
vldr         s15, [r4, #0x14]
vmul.f32     s15, s14, s15
```

Metrics are stored in authoring units and scaled at measure time.

---

## What is still open

- **Byte 2 of the header** is kept at `FONT+0x08` and nothing decompiled so far
  reads it.
- **Two of the three metric arrays** (`FONT+0x1c` and `FONT+0x24`) are unused by
  the measurement path. For a texture-atlas font the natural pair is glyph
  position and glyph size, but neither function recovered so far touches them,
  so they are recorded by offset and deliberately left unnamed. `limeDrawFONT`
  is where they will resolve.

---

## The second atlas is the overflow of the first

```
vcmp.f32 s13, s11        ; atlas height vs the glyph's y
movhi    r3, #0x50       ; still inside  -> texture0
movls    r3, #0x54       ; past the end  -> texture1
ldr      r0, [r4, r3]
```

The second texture is not a style or a fallback. Glyphs are laid out **downward**
and one that would run past the bottom of atlas 0 lives in atlas 1 instead, at a
y that keeps counting. That is why `limeCreateFONT` takes two texture names and
loads the second only when given one: a font small enough to fit needs no second
page.

A port must reproduce the comparison, not merely load both textures. Binding
atlas 0 for everything renders the tail of a large character set as garbage from
the wrong page — which on this build means the Korean text and nothing else,
so it would pass every test run in English.

## Two width routines, one difference

`limeGetStringWidth` detects the encoding from a `0xFF 0xFE` BOM and searches
the byte table at `+0x48` or the two-byte table at `+0x4c` accordingly.
`limeGetStringWidthUCNoHeader` never tests: it uses `+0x4c` always, for text
whose encoding the caller already knows and where a BOM would be an intruder
rather than a marker.

**That is why the codes are stored twice at load** — one table per encoding,
chosen per string.

Both abandon the search on a space (`0x20`) and return "not found" deliberately,
so a space takes the fallback advance instead of a glyph width. An atlas has no
cell for it and drawing one would emit a stray quad.

Both accumulate in **integers** and convert once at the end, multiplying by
`+0x14`. Rounding happens after the sum, not per glyph; a port that scales each
advance individually accumulates a different error.

