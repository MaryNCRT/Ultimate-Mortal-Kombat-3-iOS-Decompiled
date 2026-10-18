# The `.meshset` format — UMK3 iOS (LIME engine)

Binary format specification, derived from the **disassembly of `_LIME_LoadMeshSet`** (`0x0005ea34`, armv7 slice, Thumb-2) and **validated against all 605 `.meshset` files** in the game bundle.

> **Validation status:** 604 of 605 files parse with an exact final offset. The remaining one, `LAVALEVEL0.meshset`, is 0 bytes.
> Totals: **7,343 meshes, 2,970,529 vertices, 2,828,085 triangles.**
>
> Every offset and size in §6 was independently confirmed field by field against Ghidra's decompilation of the loader.

Reader implementation: [`tools/meshset.py`](../tools/meshset.py) — supports `validate`, `dump`, and `obj` export.

---

## 1. Overall structure

```
int32   numMeshes
MESH    meshes[numMeshes]      // consecutive records, no offset table
char[]  text_block             // optional, vestigial (see §5)
```

There is no magic number, no version field, and no table of contents. Records are read sequentially. The only practical integrity check is that the final offset, after walking every mesh, lands exactly on the text block or at EOF.

---

## 2. Three variants

Not all files use the same layout. Three exist, distinguishable by trial fit — parse and check that the final offset is exact:

| Variant | Files | Header | Indices | Vertex | Position encoding |
|---|---|---|---|---|---|
| **A** — int16 indexed | 579 | 140 B | yes, `numFaces` × 6 B | 26 B | `int16 / 32767` |
| **B** — float indexed | 20 | 136 B | yes, `numFaces` × 6 B | 20 B | `float`, world units |
| **C** — float non-indexed | 5 | 136 B | **no** | 20 B | `float`, world units |

- **A** is what `_LIME_LoadMeshSet` handles: characters, fatalities, effects, props. The normal case.
- **B** and **C** are static stage geometry, exported through a different toolchain path: `BALCONY_LEVEL*`, `STREET_LEVEL*`, `SUBWAY_LEVEL*`, `TEMPLEBACKGROUND_00`, `TEMPLEFOREGROUND_00` (B), and `LAVALEVEL1-3`, `ROBO1_STANDARD`, `ROBO2_STANDARD` (C).
- **C has no index buffer.** It stores `numFaces * 3` already-expanded vertices as a triangle list. The header's `numVerts` field holds the count *before* duplication and does not describe any buffer in the file — do not use it to advance the read pointer.

> The community tool [ermaccer/UMK3IOS.MeshSetTool](https://github.com/ermaccer/UMK3IOS.MeshSetTool) implements variant A. Our reader was checked against it and agrees; variants B and C were not previously documented.

---

## 3. Mesh header

### Variant A (140 bytes)

| Offset | Type | Field | Notes |
|---|---|---|---|
| `0x00` | `char[64]` | `meshName` | null-padded, e.g. `Plane01` |
| `0x40` | `char[64]` | `textureName` | e.g. `PLATFORM_CMAP.???` — the literal `???` extension is resolved by the engine |
| `0x80` | `int32` | `numVerts` | |
| `0x84` | `int32` | `numFaces` | triangles |
| `0x88` | `float` | `boundsRadius` | bounding volume radius, scene units |

### Variants B and C (136 bytes)

Identical, but **without** the trailing float — the header ends at `numFaces`.

---

## 4. Geometry data

### Indices (variants A and B)

`numFaces` triangles, each `uint16 a, b, c` — **6 bytes per triangle**. Copied to memory with a straight `memcpy` of `6 * numFaces` bytes, with no remapping.

### Vertices — variant A (26 bytes on disk → 16 in memory)

| Offset | Type | Field | Used by engine? |
|---|---|---|---|
| `0x00` | `int16` | `x` | yes → `x / 32767.0` |
| `0x02` | `int16` | `y` | yes |
| `0x04` | `int16` | `z` | yes |
| `0x06` | `float` | `u` | yes (**unaligned** read) |
| `0x0A` | `float` | `v` | yes (unaligned) |
| `0x0E` | `float[3]` | — | **discarded by the loader** |

The copy loop in the binary (`0x0005ebcc`–`0x0005ebf0`) advances `0x1a` = 26 bytes in the source and `0x10` = 16 in the destination, transferring only the first 14 bytes. The remaining 12 are almost certainly the **vertex normal**: present in the file, but not loaded by the iOS renderer, which resolves lighting from the precomputed `.lighting` files instead (§6).

**Alignment warning:** `u` and `v` start at offset 6, so they are **not 4-byte aligned**. Harmless on x86/x64; on ARM you must read them bytewise or via `memcpy`.

In memory the vertex becomes `int16 x, y, z` + 2 bytes padding + `float u, v` = 16 bytes.

### Vertices — variants B and C (20 bytes)

`float x, y, z, u, v`. Positions in world units, **unscaled** (values on the order of ±5000). Everything 4-byte aligned.

---

## 5. Trailing text block

Almost every file ends with a fragment of C source that the exporter left behind — a vestige of an earlier pipeline where meshes were compiled in as static data rather than loaded at runtime:

```c
//=====================================================
// Ptr to each MESHINFO block...
//=====================================================
MESHINFO *Mesh__ptrs[]={
	&MeshInfo__00,
};

//=====================================================
// Mesh set info block for this FBX file.
//=====================================================
MESHSETINFO MeshSet_= {
	1,	// Number of meshes in this set.
	&Mesh__ptrs[0]	// List of ptrs to each mesh.
};
```

The loader never reads it — by the time it gets there, every mesh has been consumed. But it is extremely useful as an end marker when validating a parser, it confirms the assets originated as **FBX** files, and it hands over the engine's real struct names: `MESHINFO` and `MESHSETINFO`.

---

## 6. In-memory structures

Exact sizes, taken from the `_limeMalloc` calls in the disassembly. That function's signature is `limeMalloc(const char *tag, size_t size)`, and the tags survive as readable strings — `"meshsethandle"`, `"meshset_meshes"` — which independently corroborate the field naming.

### `MESHSETINFO` — `0x4C` (76) bytes

| Offset | Field | Notes |
|---|---|---|
| `0x00` | `char name[64]` | the filename, copied verbatim |
| `0x40` | `int texturesLoaded` | set to 0 on load |
| `0x44` | `int numMeshes` | |
| `0x48` | `MESHINFO **meshes` | array of `numMeshes` pointers |

### `MESHINFO` — `0x58` (88) bytes

| Offset | Field | Notes |
|---|---|---|
| `0x00` | `int numVerts` | |
| `0x04` | `int numFaces` | |
| `0x10` | `float boundsRadius` | third header field |
| `0x18` | `Vertex *verts` | `16 * numVerts` bytes |
| … | `uint16 *indices` | `6 * numFaces` bytes |
| … | `uint8 *vertLight` | per-vertex lighting, see below |

---

## 7. Lighting (`.lighting`)

Per-vertex lighting is precomputed and stored alongside the mesh, one byte per vertex.

**The path has a folder prefix.** The loader builds it as `sprintf(buf, "STATICLIGHTING/%s", file)` and then swaps the `.meshset` extension for `.lighting` — so the actual path is `STATICLIGHTING/<name>.lighting`.

**The fallback fill is conditional.** If lighting was requested and the file is missing, the buffer is filled with `0xFF`. But with `useLighting == 0`, the branch at `0x0005ebae` skips the `memset` entirely and **the buffer is left uninitialized**.

Both of these were errors in an earlier version of this document, found by running the real loader.

---

## 8. How this was verified

The specification was not checked against our own reader — that would only prove we are self-consistent. It was checked by running **EA's actual loader**, statically recompiled, over **the game's real data**, and comparing what it left in memory against what this document says:

```
files loaded:            590
skipped (variants B/C):   14
meshes checked:        7,326
vertices:          2,950,669
triangles:         2,810,730
mismatches:                1     (KANO_STANDARD.meshset, vertex lighting)
```

Agreement was byte-for-byte on mesh counts, copied names, vertex counts, face counts, bounding radii, the full index buffer and the full vertex buffer, plus per-vertex lighting on 7,325 of 7,326 meshes.

Two corrections came out of this exercise:

1. **`_LIME_LoadMeshSet` only understands variant A.** Given a variant B or C stage file, it reads nonsensical counts and attempts to allocate hundreds of megabytes. This confirms those files are loaded by a different engine path.
2. **The `vertLight` fill is conditional**, as described in §7.

That is the strongest form of verification available here: not two of our own readers agreeing with each other, but our understanding checked against the original code running on the original data.

---

## 9. The companion formats

These accompany `.meshset`. All but one are now solved:

| Extension | Purpose | Status |
|---|---|---|
| `.skin` | skinning weights | ✅ 29/29 — [SKIN-FORMAT.md](SKIN-FORMAT.md) |
| `.bones` | skeleton hierarchy | ✅ 27/29 (ROBO1/ROBO2 use a 24-byte bone) |
| `.skinanim` | skeletal animation | ✅ 28/29 ([issue #2](../../issues/2) for SINDEL) |
| `.events` | effect tracks | ✅ 544/545 — [EVENTS-FORMAT.md](EVENTS-FORMAT.md) |
| `.scene` | scene graph | ✅ 545/547 — [SCENE-FORMAT.md](SCENE-FORMAT.md), including the placement records |

Geometry, skinning, skeleton and animation are all readable against the shipped
data, so there is enough here to draw an animated character. `.scene` — how a
stage is assembled from its pieces — is solved too, including the placement
records: see [SCENE-FORMAT.md](SCENE-FORMAT.md).

---

## The `/ 32767` scale was wrong — it is `boundsRadius`

This section used to warn that the divisor had been derived by **fitting the
shipped data** rather than read out of the loader, and that it had not been
re-measured. It has been measured now, and it was wrong.

There is no 32767 in the draw path at all. `LIME_RenderMeshSingle` hands the
`int16` positions to GL untouched and divides with a `glScalef` built from the
mesh's OWN header field:

```
glVertexPointer(3, GL_SHORT, 16, mesh->verts)
vmov     s12, 1.0f
vdiv.f32 s16, s12, s14        ; s14 = MESHINFO+0x10, boundsRadius
glScalef(s16, s16, s16)
```

So model space is `int16 / boundsRadius`, per mesh.

**A flat divisor is not merely imprecise, it is wrong RELATIVE to other meshes.**
Graveyard carries `316.2` on its gravestones, `23.1` on its ground and `16.4` on
its moon. Dividing all three by one constant renders them all at the same size
and none at the right one; the stage comes out as a pile of unit-sized objects
scattered across thousands of units of empty space. With the real divisor a
gravestone is 74 wide and 104 tall, the ground plane is 2,839 across and flat,
and the fighter standing on it is 212.5 tall — so a gravestone comes up to his
waist, which is exactly what the shipped game looks like.

The warning was the right instinct and the fitted figure survived a long time
because it produced tidy `[-1,1]` numbers. Tidy is not the same as correct.

`runtime/lime/meshset.c` divides by `boundsRadius`.
