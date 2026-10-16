/*
 * RenderMesh.c — src/lime/common/RenderMesh.cpp (mesh loading)
 *
 * Hand-written from the disassembly of the armv7 slice (0x0005e2b8,
 * 0x0005e7f4, 0x0005ea34) and verified against the oracle: the original
 * LIME_LoadMeshSet, statically recompiled, was run over the game's real data
 * and this implementation reproduces it — see tests/test_rendermesh_diff.c.
 *
 * The format itself is documented in docs/MESHSET-FORMAT.md, and that
 * specification was itself checked by running EA's loader over 590 files.
 *
 * Difference from the original, deliberately: the original allocates through
 * limeMalloc and reads through limeLoadFile, both of which are iOS platform
 * layer. Here allocation is plain malloc and file access goes through
 * limeLoadFile(), which the platform layer provides. Behaviour is identical;
 * only the plumbing changed.
 */

#include <math.h>
#include <string.h>
#include <stdio.h>
#include "lime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Header sizes and strides, all verified against the real files. */
#define MESH_HEADER_SIZE   140   /* name[64] + texture[64] + verts + faces + radius */
#define FACE_STRIDE          6   /* 3 x uint16 */
#define DISK_VERT_STRIDE    26   /* int16 x,y,z + float u,v + 12 discarded bytes */

/*
 * Reads a fixed 64-byte field and advances the cursor.
 *
 * The original calls Read64CharsFromMem, which copies exactly 64 bytes and
 * returns the advanced pointer. The stored string is not guaranteed to be
 * terminated within those 64 bytes, so the destination buffer is 65 and the
 * terminator is written explicitly.
 */
static const uint8_t *read_64_chars(const uint8_t *src, char *dst)
{
    memcpy(dst, src, 64);
    dst[64] = '\0';
    return src + 64;
}

void LIME_FreeMeshSet(MESHSETINFO *set)
{
    if (!set) {
        return;
    }
    if (set->meshes) {
        for (int i = 0; i < set->numMeshes; i++) {
            MESHINFO *m = set->meshes[i];
            if (!m) {
                continue;
            }
            /* Same order the original frees in. */
            limeFree(m->meshName);
            limeFree(m->textureName);
            limeFree(m->verts);
            limeFree(m->indices);
            if (m->vertLight) {
                limeFree(m->vertLight);
            }
            limeFree(m);
            set->meshes[i] = NULL;
        }
        limeFree(set->meshes);
    }
    limeFree(set);
}

/*
 * Returns the INDEX of the first mesh whose name contains `name`, or -1.
 *
 * Note it returns an index, not a pointer — easy to get wrong, since every
 * other function in this file deals in pointers. The original (armv7
 * 0x5e2b8) matches with StringInString, which despite its name is an EXACT
 * comparison. This used strstr, so the Waterfront's floor node "Object04"
 * found mesh "Object040" first: the floor was never marked visible,
 * LIME_FreeNonVisibleMeshes freed it, and the stage had a black hole where
 * the fighters stand.
 */
int StringInString(const char *a, const char *b);

int LIME_FindMeshByName(const MESHSETINFO *set, const char *name)
{
    if (!set || !set->meshes) {
        return -1;
    }
    for (int i = 0; i < set->numMeshes; i++) {
        const MESHINFO *m = set->meshes[i];
        if (m && m->meshName && StringInString(m->meshName, name)) {
            return i;
        }
    }
    return -1;
}

/*
 * Loads a .meshset.
 *
 * `useLighting` asks for the matching per-vertex lighting file, which lives at
 * STATICLIGHTING/<name>.lighting — the extension is swapped, and the folder
 * prefix is easy to miss.
 *
 * Careful with the lighting buffer: the original only fills it with 0xFF when
 * lighting was REQUESTED and the file turned out to be missing. With
 * useLighting == 0 the branch at 0x0005ebae skips the memset entirely and the
 * buffer is left uninitialised. That is reproduced here rather than tidied up,
 * because callers may well depend on it.
 *
 * Only variant A is handled, which is what the original handles too: pointing
 * it at the 25 scenery files (variants B and C) makes it read nonsense counts.
 * Those go through a different engine path.
 */
MESHSETINFO *LIME_LoadMeshSet(const char *filename, int useLighting)
{
    size_t light_size = 0;
    (void)light_size;   /* the loader sizes from the header, not the file */
    uint8_t *light = NULL;

    if (useLighting) {
        char lightpath[512];
        snprintf(lightpath, sizeof(lightpath), "STATICLIGHTING/%s", filename);
        size_t n = strlen(lightpath);
        if (n >= 8) {                       /* replace ".meshset" */
            memcpy(lightpath + n - 8, ".lighting", 10);
        }
        light_size = limeFileSize(lightpath);
        light = (uint8_t *)limeLoadFile(lightpath);
    }

    size_t data_size = limeFileSize(filename);
    (void)data_size;
    uint8_t *data = (uint8_t *)limeLoadFile(filename);
    if (!data) {
        limeFree(light);
        return NULL;
    }

    /* Every block below comes from limeMalloc, as in the original: they are
     * released by LIME_FreeSingleMesh / LIME_FreeMeshSet with limeFree
     * (0x5e86c, 0x5e7f4). A host malloc here made the first scene load that
     * dropped an unused mesh free a block limeFree had never seen. */
    MESHSETINFO *set = (MESHSETINFO *)limeMalloc("meshsethandle",
                                                 sizeof(MESHSETINFO));
    if (set)
        memset(set, 0, sizeof(MESHSETINFO));
    if (!set) {
        limeFree(data);
        limeFree(light);
        return NULL;
    }

    snprintf(set->name, sizeof(set->name), "%s", filename);
    set->texturesLoaded = 0;

    int32_t count;
    memcpy(&count, data, 4);
    set->numMeshes = count;
    set->meshes = (MESHINFO *P32 *)limeMalloc("meshset_meshes",
                                          (size_t)(count > 0 ? count : 1) *
                                          sizeof(MESHINFO *));
    if (set->meshes)
        memset(set->meshes, 0, (size_t)(count > 0 ? count : 1) *
                               sizeof(MESHINFO *));
    if (!set->meshes) {
        limeFree(set);
        limeFree(data);
        limeFree(light);
        return NULL;
    }

    const uint8_t *p = data + 4;
    const uint8_t *light_cursor = light;

    for (int i = 0; i < count; i++) {
        MESHINFO *m = (MESHINFO *)limeMalloc("mesh", sizeof(MESHINFO));
        if (m)
            memset(m, 0, sizeof(MESHINFO));
        if (!m) {
            break;
        }
        set->meshes[i] = m;

        m->meshName = (char *)limeMalloc("meshname", 65);
        m->textureName = (char *)limeMalloc("texturename", 65);
        if (!m->meshName || !m->textureName) {
            break;
        }
        p = read_64_chars(p, m->meshName);
        p = read_64_chars(p, m->textureName);

        memcpy(&m->numVerts, p, 4);
        memcpy(&m->numFaces, p + 4, 4);
        memcpy(&m->boundsRadius, p + 8, 4);
        p += 12;

        m->fullBright = IsTextureFullBright(m->textureName);

        /* Indices are copied verbatim: 6 bytes per triangle. */
        size_t index_bytes = (size_t)m->numFaces * FACE_STRIDE;
        m->indices = (uint16_t *)limeMalloc("meshindices",
                                            index_bytes ? index_bytes : 1);
        if (!m->indices) {
            break;
        }
        memcpy(m->indices, p, index_bytes);
        p += index_bytes;

        m->verts = (LIMEVERTEX *)limeMalloc("meshverts", sizeof(LIMEVERTEX) *
                                        (size_t)(m->numVerts ? m->numVerts : 1));
        m->vertLight = (uint8_t *)limeMalloc("meshlight",
                                       (size_t)(m->numVerts ? m->numVerts : 1));
        if (!m->verts || !m->vertLight) {
            break;
        }

        if (useLighting) {
            if (light_cursor) {
                memcpy(m->vertLight, light_cursor, (size_t)m->numVerts);
                light_cursor += m->numVerts;
            } else {
                memset(m->vertLight, 0xFF, (size_t)m->numVerts);
            }
        }
        /* useLighting == 0: left uninitialised, as the original does. */

        /* 26 bytes on disk become 16 in memory; the last 12 are dropped.
         * u and v start at offset 6, so they are NOT 4-byte aligned — memcpy
         * rather than a cast, or this breaks on ARM. */
        for (int v = 0; v < m->numVerts; v++) {
            const uint8_t *src = p + (size_t)v * DISK_VERT_STRIDE;
            LIMEVERTEX *dst = &m->verts[v];
            memcpy(&dst->x, src + 0, 2);
            memcpy(&dst->y, src + 2, 2);
            memcpy(&dst->z, src + 4, 2);
            dst->pad = 0;
            memcpy(&dst->u, src + 6, 4);
            memcpy(&dst->v, src + 10, 4);
        }
        p += (size_t)m->numVerts * DISK_VERT_STRIDE;
    }

    limeFree(data);
    limeFree(light);
    return set;
}


/* --------------------------------------------------------- LIME_RenderMesh
 *
 * armv6 0x0008104c, 20 bytes.
 *
 * Picks one mesh out of a set by index and forwards to the single-mesh
 * renderer with an alpha of 1.0. The mesh pointer array lives at
 * MESHSETINFO+0x48.
 *
 * It is a tail call, so the whole function is four loads and a branch -- the
 * only thing it really contributes is the constant 1.0f, which is how the
 * opaque path and the fade path share one renderer.
 *
 * **It takes five arguments**, and this was first written with four, with the
 * fifth read as a hardcoded zero. The binary forwards it:
 *
 *      ldr r1, [sp, #0xc]      ; the caller's fifth argument
 *      str r1, [sp]            ; which becomes RenderMeshSingle's fifth
 *
 * After `push {r7, lr}` and `sub sp, #4` the stack pointer sits twelve bytes
 * below where the caller left it, so `[sp, #0xc]` is the first stacked
 * argument and not a local. Reading it as a local is what turned a value
 * passed straight through into a constant. `tools/protos.py` found it: the
 * callers in GameCode.c had always declared five.
 */
void LIME_RenderMesh(MESHSETINFO *set, int index,
                     TEXTURE *tex0, TEXTURE *tex1, long flags)
{
    MESHINFO *mesh = set->meshes[index];       /* MESHSETINFO+0x48 */
    LIME_RenderMeshSingle(mesh, tex0, tex1, 1.0f, flags);
}


/* ---------------------------------------------------- Read64CharsFromMem
 *
 * armv6 0x00081238, 36 bytes.  __Z18Read64CharsFromMemPcS_
 *
 * Copies a fixed 64-byte field out of a buffer and returns the advanced
 * cursor. This is how the mesh-set loader walks the name and texture fields,
 * and it is why both are exactly 64 bytes in the format.
 *
 * Note the copy runs the opposite way from the argument order: `src` is the
 * first parameter and `dst` the second.
 */
char *Read64CharsFromMem(char *src, char *dst)
{
    memcpy(dst, src, 64);
    return src + 64;
}


/* ---------------------------------------------------------- RenderAxesLines
 *
 * armv6 0x00080a6c, 12 bytes.
 *
 * **Compiled away.** The body writes its three arguments to a stack slot it
 * immediately discards, then returns -- what is left of a debug helper whose
 * contents were behind a preprocessor switch that shipped off.
 *
 * Kept here because an empty function is a finding: anything expecting axis
 * gizmos from the retail binary will not get them.
 */
void RenderAxesLines(float x, float y, float z)
{
    (void)x; (void)y; (void)z;
}


/* ---------------------------------------------------------- StringInString
 *
 * armv7 0x0005e27c, 60 bytes.
 *
 * Not a substring search, whatever the name says: it walks both strings
 * together and answers 1 only when they end at the same place with every
 * byte equal -- strcmp(a, b) == 0, two empty strings included:
 *
 *      a[0] == 0            -> b[0] == 0
 *      b[0] == 0            -> 0
 *      a[i] != b[i]         -> 0
 *      a ends at i          -> b[i] == 0
 *
 * The armv6 reading made it a prefix test ("SKULL" matched "SKULL3").
 */
int StringInString(const char *a, const char *b)
{
    int i = 0;

    if (a[0] == 0)
        return b[0] == 0;
    if (b[0] == 0 || a[0] != b[0])
        return 0;
    for (;;) {
        i++;
        if (a[i] == 0)
            return b[i] == 0;
        if (b[i] == 0 || b[i] != a[i])
            return 0;
    }
}


/* -------------------------------------------------- LIME_FreeMeshSetTextures
 *
 * armv6 0x00081808, 48 bytes.
 *
 * Releases the texture each mesh in a set holds at MESHINFO+0x44. Guarded
 * three ways before it touches anything: the set, its flag at +0x40, and its
 * count at +0x44 must all be non-zero.
 *
 * The mesh pointer array is at MESHSETINFO+0x48, the same field
 * LIME_RenderMesh indexes.
 */
void LIME_FreeMeshSetTextures(MESHSETINFO *set)
{
    int i;

    if (set == NULL || set->texturesLoaded == 0)     /* +0x40 */
        return;
    if (set->numMeshes == 0)                         /* +0x44 */
        return;

    for (i = 0; i < set->numMeshes; i++) {
        MESHINFO *mesh = set->meshes[i];             /* +0x48 */
        if (mesh != NULL && mesh->texture != NULL)   /* +0x44 */
            limeDeleteTexture(mesh->texture);
    }
}


/* ------------------------------------------------- LIME_FreeNonVisibleMeshes
 *
 * armv6 0x000811d8, 52 bytes.
 *
 * Walks the set and drops every mesh whose flag at **MESHINFO+0x54** is clear.
 * A memory optimisation with a sharp edge: once a mesh is freed this way it is
 * gone, so whatever sets that flag has to have run first and be right.
 */
void LIME_FreeNonVisibleMeshes(MESHSETINFO *set)
{
    int i;

    if (set == NULL || set->numMeshes == 0)
        return;

    for (i = 0; i < set->numMeshes; i++) {
        MESHINFO *mesh = set->meshes[i];
        if (mesh->visible == 0)                      /* +0x54 */
            LIME_FreeSingleMesh(set, i);             /* mov r0, set; mov r1, i */
    }
}


/* ------------------------------------------------------- LIME_FreeSingleMesh
 *
 * armv6 0x0008112c, 120 bytes.
 *
 * Frees one mesh's vertex data, by index into the set.
 *
 * **It calls `printf` on the way**, unconditionally, with the mesh pointer and
 * the block at MESHINFO+0x3c. So unlike `LIME_printf` and `RenderAxesLines` --
 * both compiled away -- this one really does write to stdout in the retail
 * binary, which is worth knowing when reading a capture from the emulator.
 *
 * The mesh is reached through the set's array at MESHSETINFO+0x48 rather than
 * passed in.
 *
 * **It frees six allocations, not one.** An earlier pass recorded only the
 * first, which left this function leaking five blocks per mesh. The binary:
 *
 *      +0x3c   meshName
 *      +0x40   textureName
 *      +0x18   verts
 *      +0x1c   indices
 *      +0x24   vertLight   -- the only one null-checked, so it is optional
 *      then the MESHINFO itself
 *
 * Six owned pointers released in a fixed order, exactly like LIME_FreeSkin
 * releasing its six SKININFO arrays. A missed field leaks; a phantom one
 * crashes.
 *
 * Note it **re-reads `set->meshes[index]` before every free** rather than
 * caching the pointer -- reloaded from memory each time in the disassembly.
 * Harmless, and preserved here so the shape matches.
 */
void LIME_FreeSingleMesh(MESHSETINFO *set, int index)
{
    MESHINFO *mesh;

    if (set == NULL)
        return;
    mesh = set->meshes[index];           /* +0x48 */
    if (mesh == NULL)
        return;

    limeFree(set->meshes[index]->meshName);      /* +0x3c */
    limeFree(set->meshes[index]->textureName);   /* +0x40 */
    limeFree(set->meshes[index]->verts);         /* +0x18 */
    limeFree(set->meshes[index]->indices);       /* +0x1c */

    if (set->meshes[index]->vertLight != NULL)   /* +0x24, guarded */
        limeFree(set->meshes[index]->vertLight);

    limeFree(set->meshes[index]);
    set->meshes[index] = NULL;          /* 0x5e8d8: str r3(=0), [meshes, i, lsl #2] */
}


/* --------------------------------------------------- IsTextureFullBrightPath
 *
 * armv6 0x00081740, 128 bytes.
 *
 * Takes a path and tests the **filename**, not the path: it walks backwards
 * from the end looking for `/` (0x2f) or `\` (0x5c) and starts from whichever
 * it finds.
 *
 * That it checks both separators is the interesting part -- this is iOS code,
 * where `\` never appears in a path. It is a leftover from the Windows-hosted
 * toolchain the assets came through, and it is harmless but tells you where the
 * function was written.
 *
 * Full-bright textures bypass the lighting model entirely; see
 * docs/LIGHTING.md.
 */
int IsTextureFullBrightPath(const char *path)
{
    const char *name = path;
    size_t n = strlen(path);
    size_t i;

    for (i = n; i > 0; i--) {
        char c = path[i - 1];
        if (c == '/' || c == '\\') {
            name = path + i;
            break;
        }
    }
    return IsTextureFullBright(name);
}


/* ------------------------------------------------------------- GetNextLine
 *
 * armv6 0x00080a04, 92 bytes.
 *
 * Copies one line out of a text buffer and returns the cursor past it.
 *
 * The terminator set is worth noting: it stops on `\0` **and** on `\r`
 * (0x0d), and treats `\n` (0x0a) as the line break. So it handles CRLF by
 * ending the line at the CR and stepping over the LF -- again, the shape of
 * code written against Windows-authored text files.
 */
const char *GetNextLine(const char *src, char *dst)
{
    while (*src != 0 && *src != 0x0d && *src != 0x0a)
        *dst++ = *src++;
    *dst = 0;
    return src + 1;
}


/* --------------------------------------------------- CreateFadedLookupTable
 *
 * armv7 0x0005e14c, 68 bytes.  __Z22CreateFadedLookupTablev
 *
 *      HaveFadeTable = 1
 *      for level in 0..511:
 *          ScaleTable[level][0] = 0
 *          for j in 1..255:  ScaleTable[level][j] = (level * j) >> 8
 *
 * Row `level` scales a byte by level/256, so rows past 256 brighten and the
 * table spans 0 to almost 2x. The running product is accumulated (`add r2,
 * ip` at 0x5e180) and shifted arithmetically; it never exceeds 511 * 255, so
 * the byte store keeps the value whole for every row below 257 and wraps
 * above it, as on the device.
 *
 * The version this replaces wrote `dst[i] = i` into 256 rows of a table under
 * an invented name that nothing allocated.
 */
void CreateFadedLookupTable(void)
{
    long level, j;

    HaveFadeTable = 1;

    for (level = 0; level < 0x200; level++) {
        uint8_t *row = ScaleTable + level * 256;
        long acc = level;

        row[0] = 0;
        for (j = 1; j < 0x100; j++) {
            row[j] = (uint8_t)(acc >> 8);
            acc += level;
        }
    }
}


/* ------------------------------------------------------- IsTextureFullBright
 *
 * armv6 0x0008125c, 356 bytes.  **Complete.**
 *
 * Answers "should this texture skip lighting?" -- and the answer comes from a
 * **plain text file the artists edited**, not from anything compiled in.
 *
 * The file is `res/nolight.txt`, and it documents itself:
 *
 *     # List of files that we want to be fullbright below
 *     # comment out files using the # character as the first char in a line
 *     # Always use .??? as the file extension, no directories
 *
 * Forty entries ship in the retail build, and they are almost all
 * `<CHARACTER>_DIFFUSE_ICE` -- the frozen version of every fighter -- plus
 * blood splats, vomit, the acid of Reptile and the snowman. Which is exactly
 * right: a frozen body glows from inside, and blood should not take a
 * directional highlight.
 *
 * **This is the most moddable thing found in the engine so far.** Adding a
 * texture name to a text file changes the lighting, with no rebuild. Our port
 * must read this file rather than bake the list in, or that capability is lost.
 *
 * How it works:
 *
 *  - `FullBrightLoaded` guards a one-time parse; the first call loads the file,
 *    walks it with `GetNextLine`, and frees the buffer when it reaches the end;
 *  - a line is skipped if its first byte is `0x23` (a hash) **or zero**, so
 *    blank lines are comments too;
 *  - each accepted line is copied into a table with a **stride of 0x40** and
 *    the name at **+4**, the count living at the table base. The copy is four
 *    unrolled `ldm`/`stm` pairs -- 64 bytes, fixed -- so a name has 60 bytes of
 *    room. The longest shipped entry is 26 characters.
 *  - lookup is a linear `strstr` down the table -- a **substring** test, not an
 *    equality test. Forty entries per texture, not worth indexing at this size.
 *
 * `_FullBrightLoaded` (`__DATA,__data`) and `_TheFullBrightInfo`
 * (`__DATA,__common`) are both in the symbol table under those names, so the
 * caching and the table are the original design and not an inference.
 *
 * **`_IsTextureFullBrightPath` is not a third global.** An earlier pass here
 * listed it as one and described the file path as configurable. It is not: the
 * symbol lives in `__TEXT,__text`, so it is a **function** -- the wrapper below
 * that strips a directory prefix and calls this. The path itself is a literal.
 * Checking the section a symbol lives in takes one grep and would have caught
 * that before it was written down.
 *
 * ## The `.???` convention, settled
 *
 * An earlier pass through this function called the comparison a `strcmp` and
 * left the wildcard as an open question. Resolving the import stubs corrected
 * both halves.
 *
 * The call is **`strstr`**, and `GetNextLine` stores each line verbatim -- it
 * copies until NUL, CR or LF and terminates, so the `.???` really is in the
 * table. For `strstr(name, entry)` to ever match, **the texture names the
 * engine passes in must contain `.???` literally**.
 *
 * They do. **591 of the 605 shipped `.meshset` files contain the string
 * `.???`**: texture names are stored inside the geometry with that extension
 * already in place, which is why the header of `nolight.txt` tells artists to
 * write them the same way. Nothing normalises anything at runtime -- the
 * convention is baked into the data on both sides.
 *
 * That it is `strstr` rather than an equality test has a consequence for
 * modders worth stating: **an entry matches any texture name containing it**.
 * Putting `ICE` in `nolight.txt` would unlight every texture with `ICE`
 * anywhere in its name. The shipped file always writes complete names, so the
 * behaviour never shows, but it is the difference between a list of names and
 * a list of patterns.
 */
int IsTextureFullBright(const char *name)
{
    int i;

    if (!FullBrightLoaded) {
        const char *data = limeLoadFile(NOLIGHT_FILE);
        const char *p;
        const char *end;

        /* **128 bytes, not 64.** GetNextLine has no length bound at all -- it
         * copies until NUL, CR or LF -- so this buffer has to be as large as
         * the longest line the file can contain. The prologue of the original
         * does `sub sp, sp, #0x80`, and that is why: res/nolight.txt opens with
         * comment lines of about seventy characters, and a 64-byte buffer
         * smashes the stack on the first one.
         *
         * The table entries are still 64 bytes; only the scratch line is
         * bigger. A line longer than 128 would still overflow, in the original
         * exactly as here. */
        char line[0x80];

        if (data == NULL)
            return 0;

        FullBrightLoaded = 1;
        end = data + limeFileSize(NOLIGHT_FILE);

        for (p = data; end > p; ) {
            p = GetNextLine(p, line);
            if (line[0] == 0x23 || line[0] == 0)
                continue;               /* comments and blanks alike */

            /* No bounds check here, and none in the binary either. The table
             * holds FULLBRIGHT_MAX entries; past that this writes through the
             * end of it and into the next symbol. See lime.h. */
            memcpy(TheFullBrightInfo.names[TheFullBrightInfo.count],
                   line, 64);           /* four ldm/stm pairs = 64 bytes */
            TheFullBrightInfo.count++;
        }

        limeFree((void *)data);
    }

    for (i = 0; i < TheFullBrightInfo.count; i++)
        if (strstr(name, TheFullBrightInfo.names[i]) != NULL)
            return 1;

    return 0;
}


/* --------------------------------------------------------- RenderDebugCube
 *
 * armv6 0x00081a40, 100 bytes.  **Complete -- and it does not render.**
 *
 * The name promises drawing; the body only *loads*. It checks a debug enable
 * flag, returns immediately when clear, and otherwise loads a named scene once
 * and caches the pointer in a global. The drawing half went the way of
 * `RenderAxesLines` and `LIME_printf` -- compiled out of the retail build --
 * leaving the lazy loader behind.
 *
 * That makes three functions in this engine whose bodies are gone but whose
 * *scaffolding* survives, which is a useful pattern to recognise: a function
 * that is suspiciously short for its name has usually been half-stripped, not
 * simplified.
 *
 * It is a **tail call**, which is why the last instruction is a branch rather
 * than a return:
 *
 *      ldr  r0, [r0, #0x80]
 *      pop  {r4, r5, r7, lr}
 *      b    #0x8186c              ; -> _LIME_LoadMeshSetTextures
 *
 * **That `+0x80` is new information about SCENEINFO.** The scene pointer
 * returned by LIME_LoadScene is dereferenced at offset 0x80 and the result is
 * handed straight to LIME_LoadMeshSetTextures, so **SCENEINFO+0x80 is the
 * meshset the scene owns**. Nothing else recovered so far reaches that field.
 *
 * The load is guarded twice -- once on the debug flag, once on the cached
 * pointer being null -- so it costs a compare per call after the first.
 */
void RenderDebugCube(void)
{
    if (!g_debugEnabled)
        return;

    if (g_debugCubeScene != NULL)
        return;                         /* already loaded */

    g_debugCubeScene = LIME_LoadScene(DEBUG_CUBE_SCENE, 1, 0, 0);
    LIME_LoadMeshSetTextures(g_debugCubeScene->meshset, 0);   /* +0x80 */
}


/* ------------------------------------------------- LIME_LoadMeshSetTextures
 *
 * armv6 0x0008186c, 468 bytes.  **Structurally complete.**
 *
 * Resolves the textures a meshset names into loaded TEXTURE pointers.
 *
 * ## The meshset's texture table
 *
 *      ldr  r2, [r6, #0x44]        ; count
 *      ldr  r3, [r6, #0x48]        ; array base
 *      ldr  r5, [r3, r8, lsl #2]   ; pointers, 4 bytes apart
 *
 * So `MESHSETINFO+0x44` is a count and `+0x48` an array of pointers, walked
 * with a null check per entry -- **holes in the table are legal**, not an
 * error. Each entry gets a zero written at its own `+0x48` before anything
 * else, which is the per-texture slot being cleared before it is filled.
 *
 * `MESHSETINFO+0x40` short-circuits the whole function when non-zero: textures
 * are resolved **once per meshset**, not once per use. That pairs with the
 * scene cache in LIME_LoadScene -- the engine is careful about this in two
 * separate places, so a port that re-resolves per draw is fighting the design.
 *
 * ## The second argument builds THREE name variants
 *
 * When the suffix argument is non-null, three names are composed into three
 * 0x80-byte stack buffers before the table is walked:
 *
 *      add  r0, sp, #0x180  ; buffer 1
 *      mov  r2, sl          ; the suffix
 *      bl   <3-arg string build>
 *      add  r0, sp, #0x100  ; buffer 2
 *      ...
 *      add  r0, sp, #0x80   ; buffer 3
 *
 * Three different format strings, one suffix, three candidate filenames. That
 * is a **texture variant mechanism**: the same meshset drawn with a different
 * suffix resolves to a different set of texture files.
 *
 * What the three formats are is not read out here -- they are PC-relative
 * literals in a pool this pass could not resolve cleanly, and the project has
 * a standing rule against stating constants it could not pin down. The
 * *shape* is solid: three candidates, tried per texture, selected by a
 * caller-supplied suffix.
 *
 * This is worth following up. Alternate costumes, the frozen and the burning
 * variants of a fighter, and the palette-swapped ninjas are all the same
 * geometry with different textures, and this is the only mechanism recovered
 * so far that could express that. See docs/HIDDEN-CONTENT.md for what is
 * already known to ship unused.
 *
 * The three format strings are not written out. They are PC-relative literals
 * in a pool this pass could not resolve, and the project does not state
 * constants it could not pin down -- so the body composes the candidates
 * through a helper whose contents are left for whoever resolves them.
 */
/* armv7 0x0005ed34, transcribed whole. The earlier body loaded nothing when
 * given a suffix -- "three candidate filenames are composed" was described
 * and not written -- so a fighter's scene meshes (Kung Lao's hat) had no
 * texture at all.
 *
 * The suffix is the fighter's texture base. The three names it builds are
 * searched for INSIDE each mesh's texture name (strstr), and only set flags:
 *
 *      "<suffix>_DIFF"      -> +0x48: draw with the fighter's own skin texture
 *      "<suffix>BABY"       -> +0x4c: draw with the caller's second texture
 *      "<suffix>BABILITY"   -> +0x4c
 *
 * Every mesh, suffix or not, then loads its own texture -- the "_LOW"
 * variant first when UseLOWAssets is set (the name up to its first '.',
 * then "_LOW.???"), the plain name if that fails. A set already loaded
 * prints "skipping textures for %s" and does nothing. */
extern long UseLOWAssets;               /* 0x0010df10, via slot 0xf362c */

void LIME_LoadMeshSetTextures(MESHSETINFO *meshset, const char *suffix)
{
    char diff[0x80], baby[0x80], bability[0x80], low[0x80];
    int i;

    if (meshset == NULL)
        return;
    if (meshset->texturesLoaded != 0) {         /* +0x40 */
        printf("skipping textures for %s", meshset->name);
        return;
    }
    if (suffix != NULL) {
        sprintf(diff, "%s_DIFF", suffix);
        sprintf(baby, "%sBABY", suffix);
        sprintf(bability, "%sBABILITY", suffix);
    }

    for (i = 0; i < meshset->numMeshes; i++) {
        MESHINFO *mesh = meshset->meshes[i];    /* +0x48 */
        const char *name;

        if (mesh == NULL)
            continue;                   /* freed by LIME_FreeNonVisibleMeshes */
        mesh->field48 = 0;
        mesh->field4c = 0;
        name = mesh->textureName;
        if (suffix != NULL) {
            if (strstr(name, diff))
                mesh->field48 = 1;
            if (strstr(name, baby))
                mesh->field4c = 1;
            if (strstr(name, bability))
                mesh->field4c = 1;
        }

        if (UseLOWAssets != 0) {
            char *p;
            strcpy(low, name);
            for (p = low; *p != 0 && *p != '.'; p++)
                ;
            memcpy(p, "_LOW.???", 9);
            mesh->texture = limeLoadTexture(low, 0, 0);
            if (mesh->texture != NULL)
                continue;
        }
        mesh->texture = limeLoadTexture(name, 0, 0);
    }
    meshset->texturesLoaded = 1;
}


/* ---------------------------------------------------------- CreateFadedRGBS
 *
 * armv7 0x0005e198, 210 bytes.
 * __Z15CreateFadedRGBSPhPcfl11limeVECTOR3
 *
 *      if (!HaveFadeTable) CreateFadedLookupTable()
 *      row = (int)(level * 255)            vcvt.s32.f32: toward zero
 *      row <  0     -> 0
 *      row >= 0x200 -> 0x1ff               (literal 0x1ff00 = row 511 * 256)
 *      for each vertex i:
 *          v = (float)src[i]               ONE light byte a vertex
 *          r = min((int)(v + offset.x), 255)
 *          g = min((int)(v + offset.y), 255)
 *          b = min((int)(v + offset.z), 255)
 *          dst[4i..4i+3] = ScaleTable[row][r], [g], [b], 0xFF
 *
 * The output is RGBA, four bytes a vertex, which is what glColorPointer(4,
 * GL_UNSIGNED_BYTE) then reads. There is no low clamp on the channel: an
 * offset that takes a channel negative indexes before the start of the row,
 * exactly as the original does.
 *
 * The version this replaces was read from armv6: it read three source bytes a
 * vertex, wrote three, skipped the table, and returned early when the table
 * did not exist instead of building it.
 */
static int cfrgbs_s32(float s)
{
    /* vcvt.s32.f32: toward zero, saturating, NaN -> 0 */
    if (s != s)
        return 0;
    if (s >= 2147483648.0f)
        return 0x7FFFFFFF;
    if (s < -2147483648.0f)
        return (int)0x80000000u;
    return (int)s;
}

void CreateFadedRGBS(const uint8_t *src, uint8_t *dst, float level, long count,
                     limeVECTOR3 offset)
{
    const uint8_t *row;
    long i;
    int  n;

    if (HaveFadeTable == 0)
        CreateFadedLookupTable();

    n = cfrgbs_s32(level * 255.0f);
    if (n < 0)
        n = 0;
    if (n >= 0x200)
        n = 0x1ff;
    row = ScaleTable + n * 256;

    for (i = 0; i < count; i++) {
        float v = (float)(int)src[i];
        int r = cfrgbs_s32(offset.x + v);
        int g = cfrgbs_s32(offset.y + v);
        int b = cfrgbs_s32(offset.z + v);

        if (r >= 0xff) r = 0xff;
        if (g >= 0xff) g = 0xff;
        if (b >= 0xff) b = 0xff;

        dst[0] = row[r];
        dst[1] = row[g];
        dst[2] = row[b];
        dst[3] = 0xFF;
        dst += 4;
    }
}


/* ------------------------------------------------- LIME_RenderMeshSingleIndexed
 *
 * armv7 0x0005e358, 628 bytes.
 * _LIME_RenderMeshSingleIndexed (unmangled in the symbol table).
 *
 * Draws one skinned frame of a character. Transcribed from the armv7 slice;
 * the body this replaces was read from armv6 and took a MESHINFO, so it fed
 * `MESHINFO.verts` (+0x18) to glVertexPointer. The argument is not a
 * MESHINFO: it is the 88-byte frame record LoadAnimatedCharacter builds
 * (`animatedcharacter_meshbase`, MESHREC in Players.c), whose +0x18 is the
 * flag IsAFrameVisible tests. This function never reads +0x18.
 *
 * The record holds two halves, chosen by the fifth argument ([sp, #0x4c],
 * 0x5e376) -- the body and the second skin:
 *
 *      half 0:  +0x00 verts  +0x04 faces  +0x1c indices  +0x20 uvs  +0x24 light
 *      half 1:  +0x08 verts  +0x0c faces  +0x2c indices  +0x30 uvs  +0x34 light
 *
 * Positions are not in the record at all: they are RenderVerts, the int16
 * triples DrawSkinnedMesh2 just wrote, scaled back by 1 / VertScale.
 *
 * Soft-float AAPCS: `alpha` arrives in r2 and goes to CreateFadedRGBS in r2
 * untouched. `fade` (r3) is scaled by 255 into a by-value limeVECTOR3.
 *
 * The untextured branch (the shadow pass) draws black with GL_REPLACE and
 * texturing off. `glActiveTexture(0x1702)` at 0x5e466 is in the original --
 * GL_TEXTURE, an invalid enum there, which GL ignores -- and is kept.
 */
extern long limeRenderedPolyCount;
/* Port, not in the binary: draw_gl.c's edge clamp */
int  lime_port_edge_clamp(const float *uv, int stride,
                          const unsigned short *idx, int n);
void lime_port_edge_restore(int clamped);
extern limeVECTOR3 *RenderVerts;

void LIME_RenderMeshSingleIndexed(void *rec, TEXTURE *tex, float alpha,
                                  const limeVECTOR3 *fade, long second)
{
    const char *r = (const char *)rec;
    long  nverts, nfaces;
    const void *indices, *uvs;
    const uint8_t *light;
    float s;

    glShadeModel(0x1d01);               /* GL_SMOOTH, before the fifth argument */

    if (second == 0) {
        light   = *(const uint8_t *P32 const *)(r + 0x24);
        uvs     = *(const void *P32 const *)(r + 0x20);
        nfaces  = *(const long *)(r + 0x04);
        nverts  = *(const long *)(r + 0x00);
        indices = *(const void *P32 const *)(r + 0x1c);
    } else {
        light   = *(const uint8_t *P32 const *)(r + 0x34);
        uvs     = *(const void *P32 const *)(r + 0x30);
        nfaces  = *(const long *)(r + 0x0c);
        nverts  = *(const long *)(r + 0x08);
        indices = *(const void *P32 const *)(r + 0x2c);
    }

    limeRenderedPolyCount += nfaces;

    glClientActiveTexture(GL_TEXTURE0);
    glActiveTexture(GL_TEXTURE0);
    glDisableClientState(GL_NORMAL_ARRAY);
    glEnableClientState(GL_VERTEX_ARRAY);

    s = 1.0f / VertScale;
    glScalef(s, s, s);

    if (tex == NULL) {
        glBindTexture(GL_TEXTURE_2D, 0);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glDisableClientState(GL_COLOR_ARRAY);
        glVertexPointer(3, GL_SHORT, 0, RenderVerts);
        glColor4f(0.0f, 0.0f, 0.0f, 1.0f);
        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, (float)GL_REPLACE);
        glDisable(GL_TEXTURE_2D);
    } else {
        limeVECTOR3 f;

        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glTexCoordPointer(2, GL_FLOAT, 0, uvs);
        glBindTexture(GL_TEXTURE_2D, tex->name);      /* TEXTURE+0x40 */
        glEnableClientState(GL_COLOR_ARRAY);

        f.x = fade->x * 255.0f;
        f.y = fade->y * 255.0f;
        f.z = fade->z * 255.0f;
        CreateFadedRGBS(light, TempRGBS, alpha, nverts, f);
        glColorPointer(4, GL_UNSIGNED_BYTE, 0, TempRGBS);

        glVertexPointer(3, GL_SHORT, 0, RenderVerts);
        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, (float)GL_MODULATE);
        glEnable(GL_TEXTURE_2D);
    }

    {   /* Port, not in the binary: the edge clamp (draw_gl.c) */
        const int clamp = tex ? lime_port_edge_clamp((const float *)uvs, 2,
                                   (const unsigned short *)indices,
                                                     nfaces * 3) : 0;
        glDrawElements(GL_TRIANGLES, nfaces * 3, GL_UNSIGNED_SHORT, indices);
        lime_port_edge_restore(clamp);
    }

    glClientActiveTexture(GL_TEXTURE1);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glActiveTexture(0x1702);            /* sic -- see the header */
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glBindTexture(GL_TEXTURE_2D, 0);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, (float)GL_REPLACE);
    glDisable(GL_TEXTURE_2D);

    glClientActiveTexture(GL_TEXTURE0);
    glActiveTexture(GL_TEXTURE0);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glBindTexture(GL_TEXTURE_2D, 0);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, (float)GL_REPLACE);
    glEnable(GL_TEXTURE_2D);
    glDisable(GL_CULL_FACE);
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
}


/* ------------------------------------------------------- LIME_RenderMeshSingle
 *
 * armv6 0x00080da4, 680 bytes.  **Structurally complete.**
 *
 * Draws one mesh with up to two textures. The last function in this file to be
 * read, and the one that finally names a TEXTURE field.
 *
 * ## TEXTURE+0x40 is the GL handle
 *
 *      ldr r1, [r5, #0x40]
 *      bl  _glBindTexture
 *
 * The value at `+0x40` of a TEXTURE goes straight into `glBindTexture` as the
 * name. `LIME_RenderMeshSingleIndexed` does the same at the same offset, so two
 * functions agree -- which is the standard a field identification has to meet
 * here, and the first thing recovered about the inside of a TEXTURE. Everything
 * else about that struct is still opaque, and `lime.h` keeps it that way.
 *
 * Both textures are also tested at `+0x50` before the draw, and the two tests
 * gate different branches. `+0x50` is where MESHINFO caches its
 * `IsTextureFullBright` answer, so the same offset holding a lighting flag on a
 * texture is plausible -- and plausible is not established, so it is recorded
 * by offset and not named.
 *
 * ## The mesh's scale is pushed and popped around the draw
 *
 *      vldr s15, [r4, #0x10]
 *      bl   _glPushMatrix
 *      bl   _glScalef
 *      bl   _glDrawElements
 *      bl   _glPopMatrix
 *
 * So a mesh carries its own scale at `+0x10` and it is applied for exactly the
 * duration of its draw. Nothing downstream inherits it, and nothing upstream
 * has to know about it.
 *
 * Note this calls **`glPushMatrix` directly, not `LIME_PushMatrix`**. The
 * wrapper exists precisely so the platform layer has one place to intercept,
 * and this function bypasses it. A port that hooks only the LIME_ wrappers will
 * miss this push and pop, and the mismatch shows up as a matrix stack that
 * drifts by one somewhere else entirely.
 *
 * `glDepthMask` is likewise called directly rather than through
 * `limeEnableDepthWrites`. Two engines' worth of abstraction and the hot path
 * goes round both.
 *
 * ## Texture units
 *
 * `glClientActiveTexture` and `glActiveTexture` appear in pairs, before the
 * draw and again after, with the client array state enabled and disabled around
 * them. The teardown is what leaves the units in a known state for the next
 * draw -- the same discipline FlushTranspMeshList relies on.
 *
 * ## The `+0x50` tests select the lit path
 *
 * `MESHINFO+0x50` is `fullBright` -- the cached `IsTextureFullBright` answer --
 * and the texture is tested at the same offset. When either says full-bright the
 * function branches **past the colour work entirely**:
 *
 *      ldr r3, [r5, #0x50]      ; the texture's flag
 *      ...
 *      ldr r3, [r4, #0x50]      ; the mesh's fullBright
 *      cmp r3, #0
 *      bne #0x80f9c             ; -> skip the lighting
 *
 * The lit path reads `mesh->vertLight` at `+0x24`, runs it through
 * `CreateFadedRGBS`, and hands the **result** to `glColorPointer`. So a
 * full-bright surface does not merely ignore its vertex colours -- it never
 * computes them, which is the saving the whole `res/nolight.txt` mechanism
 * exists for.
 *
 * That call is also what settles CreateFadedRGBS's argument order: the mesh's
 * light bytes go in first and the buffer drawn from comes out second.
 *
 * ## Teardown is two units, and it is not symmetric with setup
 *
 * Setup touches one texture unit. Teardown walks **two**, each with its own
 * `glClientActiveTexture` / `glActiveTexture` / `glColor4f`, then six
 * `glDisable` and `glDisableClientState` calls.
 *
 * The asymmetry is deliberate: the function leaves both units clean regardless
 * of how many it used, so the next draw starts from a known state whichever path
 * this one took. A port that mirrors setup in teardown leaves unit 1 configured
 * after a single-texture mesh, and the next multi-texture draw inherits it.
 *
 * **Why the GL enums are not all named, corrected.** An earlier note here said
 * they resolve to addresses rather than values. That is wrong for most of
 * them -- `tools/annotate.py` reads the literal pools and they come out as
 * clean GL constants.
 *
 * The real obstacle is **pairing**. The compiler interleaves the loads with
 * the calls: in this function a `GL_VERTEX_ARRAY` is loaded immediately
 * before a `glClientActiveTexture`, which does not take that enum -- the
 * value belongs to a later call and the register was simply free. So the
 * constants present are known and which call consumes each one is not, and
 * the body names an enum only where the call admits exactly one.
 */
/* A global counter the function bumps on entry, before it does anything else:
 *
 *      ldr r1, [r3]           ; -> the counter
 *      ldr r3, [r0, #4]       ; mesh->numFaces
 *      ldr r2, [r1]
 *      add r3, r2
 *      str r3, [r1]
 *
 * Triangles drawn this frame, near enough. Nothing here reads it back, so it
 * is kept only so the call stream and the side effects both match. */
long g_meshFaceCounter;

void LIME_RenderMeshSingle(MESHINFO *mesh, TEXTURE *t0, TEXTURE *t1,
                           float alpha, long flags)
{
    int   texNotFullBright;
    float scale;

    /* `alpha` is genuinely unread. CreateFadedRGBS below gets a literal 1.0f
     * (`mov.w r2, #0x3f800000`), not this argument -- an earlier body passed
     * `alpha` through and it never had any effect on the original.
     *
     * `t1` is unread too. Both stay in the signature because the mangled name
     * demands them: __Z21LIME_RenderMeshSingleP8MESHINFOP7TEXTURES2_fl. */
    (void)t1; (void)alpha;

    /* The original has no NULL guard -- `ldr.w sl, [r0, #0x18]` dereferences
     * the mesh in the third instruction. This one is ours, and it makes us
     * survive an input that would take the retail build down. */
    if (mesh == NULL)
        return;

    g_meshFaceCounter += mesh->numFaces;

    /* A NULL texture is not an early exit. `beq #0x5e7ac` lands on
     * `movs r2, #1; b #0x5e600`, which is the same value the test below would
     * have computed from a texture whose flag is clear -- so no texture reads
     * as "not full-bright" and the walk continues. */
    texNotFullBright = (t0 == NULL) ? 1 : (t0->field50 == 0);

    /* **Four conditions, and `flags` is one of them.**
     *
     *      0x5e600  ldr  r3, [sp, #0x3c]   ; flags -- the FIFTH argument
     *      0x5e602  cbz  r3, #0x5e61a
     *      0x5e604  ldr  r3, [r4, #0x24]   ; mesh->vertLight
     *      0x5e606  cbz  r3, #0x5e61a
     *      0x5e608  ldr  r3, [r4, #0x50]   ; mesh->fullBright
     *      0x5e60c  ite  ne / andeq r3, r2, #1
     *
     * An earlier body here wrote `(void)flags;` and gated only on fullBright
     * and the texture. That inverted the common case: LIME_RenderMesh calls
     * this with flags = 0, so the colour path should NEVER run from there --
     * and the old body ran it every time. Driving 97 combinations, the
     * original took this branch in exactly none of them, which is what sent
     * the reading back to the prologue. */
    if (flags != 0 && mesh->vertLight != NULL && mesh->fullBright == 0 &&
        texNotFullBright) {
        glEnableClientState(GL_COLOR_ARRAY);
        limeVECTOR3 amb;
        memcpy(&amb, StaticMeshAmbient, sizeof(amb));   /* 0x5e770 ldm */
        CreateFadedRGBS(mesh->vertLight,        /* +0x24, the SOURCE */
                        g_vertexColourScratch,
                        1.0f,                   /* a literal, not `alpha` */
                        mesh->numVerts,
                        amb);
        glColorPointer(4, GL_UNSIGNED_BYTE, 0, g_vertexColourScratch);
    } else {
        glDisableClientState(GL_COLOR_ARRAY);
    }

    /* An earlier body had `glDisableClientState(GL_NORMAL_ARRAY)` here. The
     * original enables GL_VERTEX_ARRAY -- 0x8074, not 0x8075. That is exactly
     * the mis-pairing the old comment in this file warned about and then made:
     * the constants present were known, which one each call took was not. */
    glEnableClientState(GL_VERTEX_ARRAY);

    glClientActiveTexture(GL_TEXTURE0);
    glActiveTexture(GL_TEXTURE0);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    /* **Stride 16, and the texcoords start eight bytes into the vertex.**
     * `add.w r3, sl, #8` with `movs r2, #0x10`. Position and UV are the SAME
     * buffer read at two offsets -- which is what makes LIMEVERTEX sixteen
     * bytes rather than two arrays. A stride of 0 would have GL read the UVs
     * as tightly packed and walk off the end. */
    glTexCoordPointer(2, GL_FLOAT, 16, &mesh->verts[0].u);

    /* Bound whether or not there is a texture: the NULL case falls to
     * 0x5e7b0, which binds r5 -- and r5 is the null pointer -- then rejoins.
     * So a mesh with no material still binds 0 and still enables GL_TEXTURE_2D. */
    glBindTexture(GL_TEXTURE_2D, (t0 != NULL) ? t0->name : 0u);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, (float)GL_MODULATE);
    glEnable(GL_TEXTURE_2D);

    /* **GL_SHORT, not GL_FLOAT.** Positions are the three int16 at the front
     * of LIMEVERTEX; lime.h has said so since the loader was written. An
     * earlier body declared them float, which would make GL read two vertices
     * as one and produce geometry that is wrong rather than absent. */
    glVertexPointer(3, GL_SHORT, 16, mesh->verts);

    /* **The reciprocal.** `vmov s12, 1.0f` then `vdiv.f32 s16, s12, s14` with
     * s14 = mesh->[0x10]. So the field is a DIVISOR, not a scale -- which fits
     * the int16 positions needing to come back down to model space. An earlier
     * body passed the field straight to glScalef and got 2.5 where the
     * original produces 0.4. */
    scale = 1.0f / mesh->boundsRadius;

    glPushMatrix();                     /* direct, NOT LIME_PushMatrix */
    glScalef(scale, scale, scale);
    {   /* Port, not in the binary: the edge clamp (draw_gl.c) */
        const int clamp = t0 ? lime_port_edge_clamp(&mesh->verts[0].u, 4,
                                   mesh->indices, mesh->numFaces * 3) : 0;
        glDrawElements(GL_TRIANGLES, mesh->numFaces * 3,
                       GL_UNSIGNED_SHORT, mesh->indices);   /* +0x1c */
        lime_port_edge_restore(clamp);
    }
    glPopMatrix();

    glDepthMask(1);                     /* direct, not limeEnableDepthWrites */

    /* **Unit 1 first, then unit 0.** The order is not arbitrary: leaving unit
     * 0 selected is what the next draw expects, so the teardown walks
     * backwards and finishes on the unit setup will use. An earlier body had
     * these the other way round and left unit 1 current. */
    glClientActiveTexture(GL_TEXTURE1);
    glActiveTexture(GL_TEXTURE1);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glClientActiveTexture(GL_TEXTURE0);
    glActiveTexture(GL_TEXTURE0);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glBindTexture(GL_TEXTURE_2D, 0);

    /* GL_REPLACE on the way out, GL_MODULATE on the way in. The literal at
     * 0x45f00800 is 7681.0f and 7681 is 0x1E01. Both enums arrive as FLOATS
     * because the entry point is glTexEnvf. */
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, (float)GL_REPLACE);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_CULL_FACE);            /* an earlier body dropped this */
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, (float)GL_REPLACE);
}
