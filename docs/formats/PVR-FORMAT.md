# `.pvr` textures

1,400 files, and the format is narrower than "PVR" suggests. Measured across
every one of them, not assumed.

## What the game actually ships

| | |
|---|---:|
| Valid PVR files | **1,400 of 1,400** |
| PVRTC **2bpp** | 725 (51.8%) |
| PVRTC **4bpp** | 675 (48.2%) |
| Any other pixel format | **none** |
| Mipmap levels | **0 — every file** |
| Non-power-of-two dimensions | **none** |
| With alpha | 320 |

Dimensions are 256×256 (784), 512×512 (268), 128×128 (157), 1024×1024 (153),
and a handful of 64×64 and 32×32. All square, all power of two.

## Header

The **legacy PVR v2** container — 52-byte header, `PVR!` tag — not the PVR v3
format that modern tooling defaults to:

```c
uint32 headerLength;   /* 52 */
uint32 height, width;
uint32 numMipmaps;     /* always 0 here */
uint32 flags;          /* low byte is the pixel format: 24 = PVRTC2, 25 = PVRTC4 */
uint32 dataLength;
uint32 bitsPerPixel;   /* 2 or 4 */
uint32 bitmaskRed, bitmaskGreen, bitmaskBlue, bitmaskAlpha;
uint32 pvrTag;         /* 'PVR!' = 0x21525650 */
uint32 numSurfs;       /* 1 */
```

## What this means for the port

**The decoder is a small, closed job.** Not "support PVR" — implement PVRTC1
at 2bpp and 4bpp, square, power-of-two, single surface, no mipmaps. Nothing in
the shipped data needs anything else, so anything else can be rejected loudly
rather than guessed at.

**It has to be a CPU decoder.** PVRTC is a PowerVR hardware format; desktop
GPUs do not have the extension. This is exactly what fails inside touchHLE —
`glError 0x0500`/`0x0503` on compressed texture upload, the cause of the
babality model sometimes not drawing. Decoding on the CPU and uploading plain
RGBA sidesteps it entirely.

**Zero mipmaps is a decision to inherit or overturn deliberately.** The
original shipped none, presumably to save space on a 2011 device. A PC port can
generate them at load time and will look better in motion for it — but that is
a change from the original, and worth being explicit about rather than silently
doing.

## Tooling: Noesis

**[Noesis](https://richwhitehouse.com/index.php?content=inc_projects.php&showproject=91)**
by Rich Whitehouse converts these to ordinary image formats, and is what
[ermaccer's write-up](https://ermaccer.github.io/posts/umk3iosmeshsettool/)
recommends alongside its `.meshset` converter — drag the mesh onto the tool,
convert the textures with Noesis.

Use it. It is the fastest way to see what the game's texture data actually is,
and it batch-converts, so the whole 1,400-file set can be turned into PNGs in
one pass.

### And it is the oracle for our own decoder

This is the more valuable use, and it is the method this project already runs
on everywhere else: **two independent paths from the same data, accepted only
when they agree.**

1. Convert a `.pvr` with Noesis → reference PNG.
2. Decode the same `.pvr` with our own PVRTC decoder → our RGBA.
3. Compare pixel by pixel.

PVRTC is a lossy block format with non-obvious interpolation between block
corners, so "it looks about right" is exactly the kind of judgement that hides
a bug for a year. A per-pixel diff against an independent implementation turns
that into a number. 1,400 files, two bit depths, 320 of them with alpha — that
is a real corpus, not a smoke test.

Noesis also scripts in Python, so the reference pass can be automated rather
than done by hand.

### One limit worth stating

Noesis is closed-source, Windows-only freeware with no stated licence, so it is
a **development tool, not a dependency**: it cannot be redistributed with the
port or embedded in a build. The PVRTC format is publicly documented by
Imagination Technologies and the decoder we ship has to be ours. That is a
statement about what goes in the repository, not a reason to avoid the tool —
the same way Ghidra is essential here and ships with nothing.

---

## Block geometry — verified

`python tools/pvr.py validate <app dir>` → **1,400 of 1,400 exact, 0
mismatches.**

PVRTC1 packs pixels into 8-byte blocks — 4×4 pixels at 4bpp, 8×4 at 2bpp — so
the compressed payload must be exactly

```
ceil(width / bw) * ceil(height / bh) * 8
```

and the header's own `dataLength` has to agree. It does, for every file, at
both bit depths. The file also carries at least the payload it declares in
every case.

That is the same standard `.meshset` and `.skin` were held to: the arithmetic
comes out exact across the whole corpus rather than approximately right on most
of it. **A decoder can now be built on this block layout without wondering
whether the layout is the bug.**

`tools/pvr.py` deliberately stops here. It reads headers and establishes
geometry; it does not decode pixels, because decoding is the part that needs an
independent reference and there is nothing yet to check against.

---

## The oracle was in the bundle

The plan was to convert with Noesis and diff against that. It turned out not to
be needed: **the game ships 38 textures twice**, as `NAME.PNG` *and*
`NAME.pvr`. The PNG is the uncompressed source the PVR was built from — a
reference implementation that costs nothing, needs no download, and is EA's own
data rather than a third party's reading of it.

`tools/pvrtc_diff.py` compares the decoder's output against those PNGs channel
by channel. PVRTC is lossy, so a correct decoder will not score zero, but it
should land within a few units out of 255.

## The decoder works

`tools/pvrtc_diff.py` — **1.5% mean error over valid references.**

| | independent assets | mean error | |
|---|---:|---:|---|
| PVRTC **2bpp** | 2 | **1.59 / 255** | 0.6% |
| PVRTC **4bpp** | 2 | **6.07 / 255** | 2.4% |
| **overall** | 4 | **3.83 / 255** | **1.5%** |

PVRTC is lossy, so zero is impossible. What matters is whether the residual is
compression or a bug — and there is a test that answers that.

### The residual tracks the image, not the blocks

Binning error by the reference image's local gradient:

| gradient | `FE_MAINLOGO_EN` | `LIGHTNING` | `VORTEX1` (2bpp) |
|---|---:|---:|---:|
| 0–31 (flat) | 4.75 | 4.79 | **1.63** |
| 96–127 | 8.05 | — | — |
| 128–159 | 9.26 | — | — |
| 224–255 (hard edge) | **30.33** | **50.89** | — |

**That is the signature of block compression.** A 4×4 block holding two endpoint
colours physically cannot represent an edge inside itself, so error must rise
with detail. Smooth content decodes almost exactly; hard edges do not.

A *decode* bug would behave the opposite way: it would track **block position**
rather than image content. That was measured separately and comes out flat —
74.8 to 75.5 across all sixteen positions of a 4×4 block. Two independent
measurements, pointing the same way.

`python tools/pvrtc_diff.py --gradient` reproduces this.

---

## The bug was in the references, not the decoder

This is the part worth remembering.

The decoder was scored at **19.0%**, then 12.2%, then 5.5% after two genuine
fixes. Five further rounds of hypothesis-and-measure found nothing — fourteen
variations all scored worse. The dead-end map is below and is still useful.

Then the reference images were rendered side by side and looked at. **Three of
the thirteen pairs are not pairs**:

| Pair | Scored | What it actually is |
|---|---:|---|
| `FE_METAL_BG` | 74.98 | The PNG frames the art differently — content in the top ~60% of the square, PVR fills it entirely |
| `MYBLOOD1` | 34.89 | The PNG is the unprocessed source with a **magenta chroma key**; ours is grey, same shape exactly |
| `MYBLOOD2` | 22.89 | Same |

`FE_METAL_BG` alone dragged the 4bpp mean from 6.07 to 16.30, and the overall
figure from 3.83 to 14.00. **The decoder had been essentially correct since the
second fix**, and three rounds were spent hunting a bug that lived in the test
data.

The lesson is the one this project keeps relearning, and it has now cost time
in three separate places — the touchHLE gamepad coordinates, MAME's door
interlock, and here: **look at the picture.** Each time, the numbers supported
an indefinite hunt and one glance ended it.

`tools/pvrtc_diff.py` now carries the invalid pairs in an `INVALID` table with
the reason for each, so nobody re-includes them.

### Eliminated by measurement — still worth keeping

Fourteen hypotheses, every one scoring worse than the code as written, against
the then-baseline of 14.06:

| Hypothesis | Score |
|---|---:|
| Morton order with x in the low bit | 68.78 |
| Endpoint extraction swapped instead of the blend | 31.26 |
| Column-major modulation indexing | 17.37 |
| Bilinear with no half-block offset | 17.80 |
| Bilinear offset `+bw/2` | 23.95 |
| Bilinear offset `-bw/2 + 0.5px` | 14.58 |
| Endpoint A blue as bits 0-3 | 15.54 |
| A blue 0-3 and translucent blue 0-2 | 15.81 |
| Modulation weights reversed | 36.43 |
| `3/8` and `5/8` swapped | 17.02 |
| No punch-through mode | 16.33 |
| Modulation rows reversed | 20.33 |
| Modulation columns reversed | 20.21 |
| Both reversed / full reverse | 22.62 |

Morton order, endpoint colour unpacking, the bilinear offset convention, the
weight table and the modulation bit ordering are all **confirmed correct** — by
being worse every other way. That was real work even though the remaining error
turned out not to be theirs.

### The two fixes that mattered

1. **Colour B is the low 16 bits, unshifted.** The modulation flag occupies bit
   0 and *shares* it with blue's least significant bit rather than displacing
   the field. Shifting corrupted every channel of B. 19.0% → 12.2%.
2. **The blend direction was inverted** — the modulation weight selects the
   opposite endpoint from the one assumed. 12.2% → 5.5%.

A third, marginal: endpoint A's blue reads as 5 bits (0–4) rather than 4 (1–4).

---

## Remaining caveat, stated plainly

Four independent assets is a thin corpus — two per bit depth. The evidence that
the decoder is correct is strong (gradient correlation, block-position
flatness, pixel-exact output wherever a block's endpoints are equal) but it is
not the 1,400-file exhaustive check the container and block geometry got.

**Before shipping a C port of this, widen the reference set.** The `*_VERSUS`
family gives 25 more pairs at 512×512 PNG against 256×256 PVR — downscaling the
PNG would make them comparable, with resampling error folded in but still
useful as a cross-check.



---

## The other half of the textures are PNG

The loader in this port read only `.pvr` for a long time, and that quietly cost
most of the game's stages. **183 of the shipped textures exist ONLY as PNG**, and
they are not incidental ones: the list is
`BALCONY_COMPLETEMAP`, `BELLTOWER_COMPLETEMAP`, `BRIDGE_COMPLETEMAP`,
`CAVE_COMPLETEMAP`, ... — the stage atlases. A stage whose atlas is a PNG drew
untextured, which looked like a broken stage rather than a missing decoder.

221 PNG files ship. They are uniform and easy:

| bit depth | colour type | interlace | count |
|---|---|---|---:|
| 8 | 6 (RGBA) | none | 364 |
| 8 | 2 (RGB) | none | 64 |
| 8 | 3 (palette, with `PLTE` + `tRNS`) | none | 14 |

(The counts total more than 221 because the survey matched `*.PNG` and `*.png`
on a case-insensitive filesystem and saw each file twice — worth saying, because
that double count is exactly the kind of thing that turns into a fake finding.)

`runtime/lime/png.c` decodes them, and it carries its own DEFLATE: the project
ships no third-party code and there is no zlib on the build host, so the stored,
fixed-Huffman and dynamic-Huffman block types are written out.

**Verification.** All 221 files decode with zero failures — but decoding without
error is not decoding correctly, so fifteen of them, covering all three colour
types, were compared **byte for byte** against an independent reference decoder
written in Python against the standard library's `zlib`. 15 of 15 identical.
