/*
 * lime/common/RenderScene.cpp -- the scene graph and its list.
 *
 * Recovered from the armv6 slice. Addresses below are armv6.
 *
 * Scenes live on a singly linked list threaded through SCENEINFO+0x90, with
 * the head in a global. Several of the small functions here exist only to walk
 * it, and between them they pin down the layout that
 * docs/SCENE-FORMAT.md derived from the loader.
 */

#include <math.h>
#include <string.h>
#include <stdio.h>
#include "lime.h"


/* -------------------------------------------------- GetMatrixFromPalette
 *
 * armv6 0x00081b2c, 8 bytes.  __Z20GetMatrixFromPalettelP9SCENEINFO
 *
 * Indexes the scene's tail array. **32-byte stride**, which independently
 * confirms the in-memory tail record size that SCENE-FORMAT.md derived from
 * LIME_LoadScene: 40 bytes on disk, 32 in memory.
 */
void *GetMatrixFromPalette(long index, SCENEINFO *scene)
{
    return (char *)scene->tail + index * 32;   /* SCENEINFO+0x7c */
}


/* --------------------------------------------------- ClearTranspMeshList
 *
 * armv6 0x00081aa4, 16 bytes.  __Z19ClearTranspMeshListv
 *
 * Empties the transparent-mesh list by dropping its head. The engine collects
 * translucent meshes during traversal and draws them after the opaque pass --
 * the ordinary fixed-function answer to sorting, and the only sorting this
 * renderer does.
 */
void ClearTranspMeshList(void)
{
    /* The COUNT, not the list. An earlier pass assigned zero to the array
     * itself, which is not even valid C -- it only survived because this file
     * had never been compiled. The symbol table names the counter
     * _NumTranspMeshes, and resetting it is the whole of the operation: the
     * 255 slots are overwritten in place on the next frame. */
    g_transpMeshCount = 0;
}


/* ------------------------------------------------------- LIME_SceneExists
 *
 * armv6 0x00081b3c, 40 bytes.
 *
 * Walks the scene list looking for a specific SCENEINFO. Returns it, or NULL.
 * The list is threaded through **+0x90**.
 *
 * Worth having because the loader reference-counts scenes: something has to
 * answer "is this pointer still live" before a caller trusts it.
 */
SCENEINFO *LIME_SceneExists(SCENEINFO *scene)
{
    SCENEINFO *s = g_sceneList;

    while (s != NULL) {
        if (s == scene)
            return s;
        s = s->next;                           /* SCENEINFO+0x90 */
    }
    return NULL;
}


/* ---------------------------------------------------- GetScenePointingTo
 *
 * armv7 0x0005ef4c, 26 bytes.  __Z18GetScenePointingToP9SCENEINFO
 *
 *      s = ScenesHead
 *      while (s) { if (s->next == scene) return s;  s = s->next; }
 *      return NULL
 *
 * Finds the node whose `next` is the given scene -- its predecessor. **When
 * there is none it returns NULL**, and that includes the scene being the head
 * itself, which is how LIME_FreeScene knows to move the head instead.
 *
 * The armv6-era transcription returned the LAST node when nothing matched.
 * Freeing the head scene then wrote `last->next = head->next` and closed the
 * list into a ring; the next walk (LIME_SceneExists on the following free in
 * Task_GameDestroy) never ended. That was the black screen after the last
 * round of 0.0.1/0.0.2 on stage 0 (TrainDie1Scene).
 */
SCENEINFO *GetScenePointingTo(SCENEINFO *scene)
{
    SCENEINFO *s = g_sceneList;

    while (s != NULL) {
        if (s->next == scene)
            return s;
        s = s->next;
    }
    return NULL;
}


/* ----------------------------------------------- LIME_LoadSceneWithTextures
 *
 * armv6 0x00082384, 32 bytes.
 *
 * Loads a scene and then its mesh set's textures, in one call. The two-stage
 * split exists because a scene can be loaded without its textures -- the
 * loader takes flags this wrapper hard-codes to zero.
 */
SCENEINFO *LIME_LoadSceneWithTextures(const char *filename)
{
    SCENEINFO *scene = LIME_LoadScene(filename, 0, 0, 0);

    if (scene != NULL && scene->meshset != NULL)   /* SCENEINFO+0x80 */
        LIME_LoadMeshSetTextures(scene->meshset, 0);

    return scene;
}


/* ------------------------------------------------- LIME_GetSceneFromFilename
 *
 * armv6 0x00081bb4, 44 bytes.
 *
 * Looks a scene up by name, walking the same +0x90 list as LIME_SceneExists.
 *
 * It calls `strcmp(scene, name)` with the SCENEINFO pointer **as the string**,
 * which is the second function to give this away -- IsWhirlwindScene does the
 * same with strstr. **The scene name is the first field of SCENEINFO**, at
 * offset 0, needing no dereference.
 *
 * This is what makes scenes reference-counted rather than reloaded: the loader
 * calls this first and, on a hit, only bumps the count at +0x40.
 */
SCENEINFO *LIME_GetSceneFromFilename(const char *filename)
{
    SCENEINFO *s = g_sceneList;

    while (s != NULL) {
        if (strcmp((const char *)s, filename) == 0)
            return s;
        s = s->next;                                 /* +0x90 */
    }
    return NULL;
}


/* ---------------------------------------------------------------- AddScene
 *
 * armv6 0x00081c00, 72 bytes.
 *
 * Allocates a scene and links it into the global list.
 *
 * Two concrete facts fall out:
 *
 *  - **SCENEINFO is 0x94 = 148 bytes.** That is the literal passed to
 *    `limeMalloc`, and it is consistent with the field offsets the format work
 *    already established, the highest of which is `next` at +0x90.
 *  - The very first thing done with the new block is
 *    **`strcpy(scene, name)`** -- the name is written to offset 0 with no
 *    field access at all. Third independent confirmation that SCENEINFO begins
 *    with its own filename, after LIME_GetSceneFromFilename and
 *    IsWhirlwindScene both handed the struct pointer straight to a string
 *    function.
 */
SCENEINFO *AddScene(const char *name)
{
    SCENEINFO *scene = (SCENEINFO *)limeMalloc("scene",
                                               sizeof(SCENEINFO));  /* 0x94 in the image */

    strcpy((char *)scene, name);        /* the name IS the first field */

    scene->next = g_sceneList;
    g_sceneList = scene;
    return scene;
}


/* ------------------------------------------------------ LIME_SetSceneTextures
 *
 * armv6 0x00081d54, 48 bytes.
 *
 * Binds one named mesh's texture into a caller-supplied array, indexed by the
 * mesh's own position in its set.
 *
 * `LIME_FindMeshByName` returning **-1** is the miss case and is checked with
 * `cmn r0, #1` -- so a name that is not in the set is silently ignored rather
 * than being an error. Worth knowing: a typo in a scene's object name loses a
 * texture and reports nothing, which is the same class of silent coupling as
 * IsWhirlwindScene matching a filename substring.
 */
/* armv7 0x0005f07c, 46 bytes. The armv6 reading above took one name; the
 * armv7 function takes a TABLE -- { mesh name, TEXTURE ** } pairs ended by
 * a NULL name -- and for every name the set holds, stores the texture that
 * entry points at:
 *
 *      for (e = table; e->name; e++)
 *          if ((i = LIME_FindMeshByName(set, e->name)) != -1)
 *              out[i] = *e->texture;
 *
 * HUDANIM_Render calls it with FIGHT.meshset and one of the
 * *_MeshAndTexture tables; with the one-name body the overlay's textures
 * were never set and "FIGHT" drew white. */
void LIME_SetSceneTextures(MESHSETINFO *set, const void *table, TEXTURE **out)
{
    const uint32_t *e = (const uint32_t *)table;    /* 8 bytes an entry */

    for (; e[0] != 0; e += 2) {
        int index = LIME_FindMeshByName(set, (const char *)(uintptr_t)e[0]);
        if (index != -1)
            out[index] = *(TEXTURE *P32 *)(uintptr_t)e[1];
    }
}


/* ----------------------------------------------------------- LIME_FreeScene
 *
 * armv7 0x0005efe0, 152 bytes (first read from armv6 0x00081c64).
 *
 * **The reference counting, in code.** This is what docs/SCENE-FORMAT.md
 * inferred from the loader, now visible from the other end:
 *
 *   1. `LIME_SceneExists` first -- a pointer that is no longer on the list is
 *      ignored rather than double-freed;
 *   2. **decrement the count at +0x40**, and return if anything still holds it;
 *   3. only then unlink and release.
 *
 * So `+0x40` is confirmed as the reference count, and loading the same scene
 * twice really does hand back the same object.
 *
 * The unlink is the reason `GetScenePointingTo` exists: with no back pointer,
 * removal has to walk the list to find the predecessor and then copy
 * `scene->next` over `prev->next`.
 *
 * One coupling worth recording: **freeing a scene calls `LIME_KillSliders`**,
 * which clears the debug overlay's six slider windows. Debug UI and scene
 * lifetime are tied together in the retail binary, not compiled apart.
 */
void LIME_FreeEvents(SCENEEVENTS *events);    /* Events.c, 0xa44c4 */

void LIME_FreeScene(SCENEINFO *scene)
{
    SCENEINFO *prev;
    long i;

    if (LIME_SceneExists(scene) == NULL)                /* 0x5efe6 */
        return;

    scene->refCount--;                                  /* +0x40 */
    if (scene->refCount != 0)
        return;

    LIME_KillSliders();

    /* armv7 0x5effc-0x5f074: unlink, moving the head when there is no
     * predecessor -- the armv6 reading stopped after the unlink and freed
     * nothing, so every scene leaked and the head was never moved. */
    prev = GetScenePointingTo(scene);
    if (prev != NULL)
        prev->next = scene->next;                       /* +0x90 */
    else
        g_sceneList = g_sceneList->next;                /* ScenesHead 0x17175c */

    limeFree(scene->field4c);                           /* +0x4c */
    limeFree(scene->tail);                              /* +0x7c */
    if (scene->events != NULL)                          /* +0x84 */
        LIME_FreeEvents((SCENEEVENTS *)scene->events);
    if (scene->meshset != NULL)                         /* +0x80 */
        LIME_FreeMeshSet(scene->meshset);

    for (i = 0; i < scene->nodeCount; i++) {            /* +0x48 */
        limeFree(scene->nodeKeys[i]);                   /* +0x88 */
        limeFree(scene->nodeStream[i]);                 /* +0x8c */
    }
    limeFree(scene->nodeKeys);
    limeFree(scene->nodeStream);
    limeFree(scene);
}


/* ------------------------------------------------------------ LIME_LoadScene
 *
 * armv6 0x00081da0, 1508 bytes.  **Structurally complete -- it maps SCENEINFO.**
 *
 * The entry point for loading anything the renderer draws. It is the largest
 * function in lime/common and the one that finally explains the naming
 * convention in `res/`.
 *
 * ## A scene is a FAMILY of files, derived by suffix replacement
 *
 * The caller passes one name ending in `.scene`. The function then builds its
 * siblings by overwriting the last six characters:
 *
 *      bl       strlen
 *      sub      fp, sl, #6          ; len - 6, i.e. the start of ".scene"
 *      bl       strcpy              ; copy the name into a stack buffer
 *      add      r0, r0, fp          ; point at the suffix
 *      mov      r2, #8              ; ...or #9
 *      bl       memcpy              ; overwrite it
 *
 * `#6` is exactly the length of `.scene`. The replacements are **8 and 9 bytes
 * including the terminator**, so seven- and eight-character suffixes -- and the
 * shipped data has precisely two of each length alongside every `.scene`:
 *
 * | bytes | suffix | contents |
 * |---:|---|---|
 * | 8 | `.events` | effect tracks |
 * | 9 | `.meshset` | geometry |
 * | 9 | `.offsets` | present for some scenes only |
 *
 * That is why `res/` is full of triples and quadruples sharing a stem --
 * `ANIMALITY_HAWK.scene`, `.meshset`, `.events`, `.offsets`. **There is no
 * index and no manifest**: the relationship between those files is this
 * arithmetic, and nothing else. A repacker that renames one file breaks the
 * set silently.
 *
 * The `.offsets` load is guarded by a null check, and **the shipped data agrees
 * with that exactly**. Counted in `res/`:
 *
 *      .scene     547
 *      .events    545       <- essentially one per scene
 *      .meshset   605       <- more; some are loaded without a scene
 *      .offsets    74       <- 474 of the 547 scenes have none
 *
 * A near-1:1 ratio for `.events` and a small minority for `.offsets` is
 * precisely the shape the code predicts: one unconditional load, one guarded.
 * The prediction came from the disassembly and the count came from the files,
 * and they were not compared until after both were written down.
 *
 * ## Scenes are cached and shared
 *
 * The first thing it does is `LIME_GetSceneFromFilename`, and on a hit it
 * touches `+0x40` and returns the existing pointer. On a miss it calls
 * `AddScene` to register the new one. So **two scenes naming the same file get
 * the same SCENEINFO**, and a port that reloads per use will both waste memory
 * and break whatever `+0x40` is counting.
 *
 * ## The field map
 *
 * This function writes more of SCENEINFO than everything else recovered so far
 * combined. Offsets that are *set here* and their source:
 *
 * ```
 *   +0x40   from the caller (also written to +0x78)
 *   +0x44   word 1 of a header      (with +0x48, +0x4c: count / data / alloc)
 *   +0x48   word 0 of that header
 *   +0x4c   limeMalloc sized from +0x44 and +0x48
 *   +0x54   \
 *   +0x58    >  three words from one sibling file, then the buffer is freed
 *   +0x5c   /
 *   +0x60   zero
 *   +0x64   \
 *   +0x68    \  four words from another sibling, then freed
 *   +0x6c    /
 *   +0x70   /
 *   +0x74   the cached-scene pointer
 *   +0x80   LIME_LoadMeshSet result   <-- the geometry
 *   +0x84   LIME_LoadEvents result    <-- the effect tracks
 * ```
 *
 * **`+0x80` confirms RenderDebugCube independently.** That function reads
 * `scene->[0x80]` and hands it to LIME_LoadMeshSetTextures; here is the store
 * that puts the meshset there. Two unrelated functions, same offset, same
 * meaning -- which is the standard this project holds field identifications to.
 *
 * `+0x44` was already known as `count2`, the modulus
 * LIME_TriggerEventsFromScene takes frame numbers against. Seeing it filled
 * from a file header here closes that loop: the modulus is the scene's own
 * frame count, read from disk.
 *
 * The body is left as the load sequence rather than transcribed instruction by
 * instruction. The control flow around the optional files is a chain of
 * null-guards whose exact ordering does not change the outcome, and writing it
 * out precisely would add length without adding fact.
 */
/* arg2 is a texture-base NAME, not a number: Players.c passes `texBase`
 * here and every other caller passes 0. Declared `int`, the pointer was
 * truncated to 32 bits at every call -- harmless only for as long as
 * nothing in here reads it, which is not a property to depend on. */
/* Two file names built in globals, not on the stack (0x002c3ec8, 0x002c3f08:
 * `ldr r10, =_OffsetsFilename` / `=_LightsFilename`), and a running total of
 * node-frames loaded (0x00171778, `mla r3, nodes, frames, r3`). Nothing in
 * this tree reads the total; it is kept because the original keeps it. */
char OffsetsFilename[0x40];
char LightsFilename[0x40];
long NumTotalNodes;

/* 0x0005f464: a frame whose track value is not above this is not a key. */
#define SCENE_KEY_THRESHOLD 0.03f

SCENEINFO *LIME_LoadScene(const char *filename, int arg1,
                          const char *arg2, int arg3)
{
    /* arg3 is a per-frame byte mask (`ldrsb r3, [frame, r8]`; a zero byte
     * skips the frame), handed on to LIME_LoadEvents with arg2. arg1 is never
     * read: r1 is overwritten for the "loading %s" print before any use. */
    const signed char *mask = (const signed char *)(uintptr_t)(unsigned)arg3;
    SCENEINFO *scene;
    char meshsetName[0x40];             /* sp+0x34 */
    char eventsName[0x40];              /* sp+0x74 */
    const uint8_t *data, *obj;
    const long *side;
    size_t stem;
    long nodes, frames, node, k, nkeys, count3;
    (void)arg1;

    scene = LIME_GetSceneFromFilename(filename);
    if (scene != NULL) {
        scene->refCount++;              /* +0x40 */
        return scene;
    }

    /* 0x5f0dc: a print of "loading %s" (0x001715f8); a no-op in retail. */
    stem = strlen(filename) - 6;        /* drop ".scene" */

    strcpy(eventsName, filename);
    memcpy(eventsName + stem, ".events", 8);
    strcpy(meshsetName, filename);
    memcpy(meshsetName + stem, ".meshset", 9);
    strcpy(OffsetsFilename, filename);
    memcpy(OffsetsFilename + stem, ".offsets", 9);
    sprintf(LightsFilename, "STATICLIGHTING/%s", filename);
    memcpy(LightsFilename + strlen(LightsFilename) - 6, ".lights", 8);

    data = (const uint8_t *)limeLoadFile(filename);
    if (data == NULL)
        return NULL;
    scene = AddScene(filename);
    if (scene == NULL)
        return NULL;

    /* 0x5f172..0x5f194: the defaults, before either sibling file. */
    scene->posX = scene->posY = scene->posZ = 0.0f;     /* +0x54..+0x5c */
    scene->field68 = scene->field6c = scene->field70 = 0;
    scene->events = NULL;                               /* +0x84 */
    scene->refCount = 1;                                /* +0x40 */
    {
        float m180 = -180.0f;           /* 0xc3340000, literal 0x5f48c */
        memcpy(&scene->field64, &m180, 4);
    }
    scene->field74 = NULL;
    scene->field78 = 1;
    scene->scale = 1.0f;                                /* +0x60 */

    /* `.offsets`: three words, the scene origin. */
    side = (const long *)limeLoadFile(OffsetsFilename);
    if (side != NULL) {
        memcpy(&scene->posX, &side[0], 4);
        memcpy(&scene->posY, &side[1], 4);
        memcpy(&scene->posZ, &side[2], 4);
        limeFree((void *)side);
    }
    /* `STATICLIGHTING/<name>.lights`: four words at +0x64..+0x70. */
    side = (const long *)limeLoadFile(LightsFilename);
    if (side != NULL) {
        scene->field64 = side[0];
        scene->field68 = side[1];
        scene->field6c = side[2];
        scene->field70 = side[3];
        limeFree((void *)side);
    }

    /* `vldr s14, [r4, #0x70]; vcvt.s32.f32`: +0x70 is a float, and its
     * integer part is LIME_LoadMeshSet's lighting argument. */
    {
        float lit;
        memcpy(&lit, &scene->field70, 4);
        scene->meshset = LIME_LoadMeshSet(meshsetName, (int)lit);
    }
    /* A scene without its meshset or events does not return: the original
     * branches to itself (`b .` at 0x5f202 and 0x5f200), as AddToTranspMeshList
     * does. The message is ours, so the session log says why it stopped. */
    if (scene->meshset == NULL) {
        fprintf(stderr, "LIME_LoadScene: no %s -- the original hangs here\n",
                meshsetName);
        for (;;) { }
    }
    scene->events = LIME_LoadEvents(eventsName, (long)(uintptr_t)arg2, arg3);
    if (scene->events == NULL) {
        fprintf(stderr, "LIME_LoadScene: no %s -- the original hangs here\n",
                eventsName);
        for (;;) { }
    }

    /* The header: node count, then frame count. */
    nodes  = ((const long *)data)[0];
    frames = ((const long *)data)[1];
    scene->nodeCount = nodes;                           /* +0x48 */
    scene->count2    = (int)frames;                     /* +0x44 */

    scene->field4c = limeMalloc("scene_names", nodes << 6);
    if (scene->field4c == NULL)
        return NULL;
    scene->nodeKeys   = (SCENENODEKEY *P32 *)limeMalloc("scene_nodes", nodes << 2);
    scene->nodeStream = (uint16_t *P32 *)limeMalloc("crunchedI_container",
                                                nodes << 2);
    NumTotalNodes += nodes * frames;

    /* Each object: a 64-byte name, then one 12-byte track a frame --
     * +0 the value (alpha), +4 a word narrowed to key+5, +8 the palette
     * index. Only frames whose value is above 0.03 (and that the mask lets
     * through) become keys; every other frame's stream entry stays 0xffff. */
    obj = data + 8;
    for (node = 0; node < nodes; node++) {
        char *name = (char *)scene->field4c + (node << 6);
        const uint8_t *trk = obj + 0x40;
        SCENENODEKEY *key;
        uint16_t *strm;

        memcpy(name, obj, 0x40);

        nkeys = 0;
        for (k = 0; k < frames; k++) {
            float v;
            memcpy(&v, trk + k * 12, 4);
            if (mask != NULL && mask[k] == 0)
                continue;
            if (v > SCENE_KEY_THRESHOLD)
                nkeys++;
        }

        scene->nodeStream[node] = (uint16_t *)limeMalloc("scenenode_find",
                                                         frames << 1);
        scene->nodeKeys[node] = (SCENENODEKEY *)limeMalloc("scenenodes_i",
                                                           nkeys << 3);
        key = scene->nodeKeys[node];
        if (key == NULL)
            return NULL;
        strm = scene->nodeStream[node];

        nkeys = 0;
        for (k = 0; k < frames; k++, trk += 12) {
            float v;
            int mesh;

            memcpy(&v, trk, 4);
            strm[k] = 0xffff;
            if (mask != NULL && mask[k] == 0)
                continue;
            if (!(v > SCENE_KEY_THRESHOLD))
                continue;

            key->alpha = v;
            mesh = LIME_FindMeshByName(scene->meshset, name);
            key->meshIndex = (uint8_t)mesh;
            /* `meshes[(uint8_t)mesh]->visible = 1`, so that
             * LIME_FreeNonVisibleMeshes below keeps it. The original does
             * not check for a miss (-1 becomes index 255); the bound is ours. */
            if ((uint8_t)mesh < scene->meshset->numMeshes)
                scene->meshset->meshes[(uint8_t)mesh]->visible = 1;
            key->field05 = (uint8_t)*(const long *)(trk + 4);
            memcpy(&key->paletteIndex, trk + 8, 2);
            key++;
            strm[k] = (uint16_t)nkeys++;
        }
        obj += 0x40 + frames * 12;
    }

    /* The palette: a count, then 40-byte records. The rotation's four floats
     * are scaled by 32767.0 (literal 0x5f468) and truncated to int16; the six
     * words after them are copied as they are. 32 bytes each in memory. */
    count3 = *(const long *)obj;
    scene->tail = limeMalloc("SceneMtxPalette", count3 << 5);
    if (scene->tail == NULL)
        return NULL;
    {
        const uint8_t *r = obj;
        uint8_t *d = (uint8_t *)scene->tail;
        long i;
        for (i = 0; i < count3; i++, r += 40, d += 32) {
            int q;
            for (q = 0; q < 4; q++) {
                float f;
                int16_t s;
                memcpy(&f, r + 4 + q * 4, 4);
                s = (int16_t)(int)(f * 32767.0f);
                memcpy(d + q * 2, &s, 2);
            }
            memcpy(d + 8, r + 0x14, 24);
        }
    }

    limeFree((void *)data);
    LIME_FreeNonVisibleMeshes(scene->meshset);
    return scene;
}


/* ------------------------------------------------------- AddToTranspMeshList
 *
 * armv6 0x00081ab8, 116 bytes.  **Complete.**
 *
 * Defers a transparent mesh instead of drawing it: the opaque pass records it
 * here and FlushTranspMeshList draws the whole batch afterwards.
 *
 * ## The record is 48 bytes, and the compiler says so twice
 *
 *      lsl  r3, r4, #6             ; index * 64
 *      sub  r3, r3, r4, lsl #4     ; minus index * 16   ->  index * 48
 *
 * One multiply turned into two shifts and a subtract, the same trick
 * LIME_LoadBones uses. FlushTranspMeshList walks the identical array with a
 * plain `add r6, r6, #0x30`, so both sides agree on 0x30.
 *
 * ```
 *   +0x00   two words copied from the SCENENODE
 *   +0x04     (a byte at +0x05 is read back as a flag when flushing)
 *   +0x08   the QSTMATRIX, 32 bytes, copied verbatim by two ldm/stm pairs
 *   +0x28   MESHSETINFO *
 *   +0x2c   the fifth argument
 * ```
 *
 * ## Overflowing the list HANGS the game
 *
 * This is the part worth stopping on:
 *
 *      add   r3, r4, #1
 *      cmp   r3, #0xff
 *      str   r3, [r5]
 *      pople {r4, r5, r7, pc}      ; <= 255: return normally
 *      b     #0x81b20              ; otherwise branch to ITSELF
 *
 * `b #0x81b20` is at address `0x81b20`. **It is an unconditional branch to its
 * own address** -- an infinite loop with interrupts still on. The 256th
 * transparent mesh in a frame does not wrap, does not drop, and does not
 * crash. It locks the game solid.
 *
 * That is almost certainly a debug assert whose reporting half was stripped in
 * the retail build, exactly like `LIME_printf` and `RenderAxesLines` -- the
 * check survived and the message did not. The effect on a shipped device is the
 * same either way.
 *
 * **For the port**: the limit is 255 per frame and it must be enforced
 * somewhere visible. A widescreen or higher-resolution port that draws more of
 * a stage at once moves closer to this ceiling, not further from it, so
 * silently raising the array size is the right fix and dropping the check is
 * not -- if it can be hit, it needs to be seen.
 */
void AddToTranspMeshList(MESHSETINFO *meshset, const SCENENODE *node,
                         const QSTMATRIX *qst, long arg3, long arg4)
{
    TRANSPMESH *slot = &g_transpMeshList[g_transpMeshCount];   /* stride 0x30 */

    /* The first two words come straight off the node. FlushTranspMeshList then
     * reads a float out of word0 and a byte out of word1, so they are copied as
     * words here and named at the point of use. */
    ((uint32_t *)slot)[0] = ((const uint32_t *)node)[0];
    ((uint32_t *)slot)[1] = ((const uint32_t *)node)[1];
    memcpy(&slot->qst, qst, 32);        /* +0x08, two ldm/stm pairs */
    slot->meshset = meshset;            /* +0x28 */

    /* **The FIFTH argument lands at +0x2c, and it is the mesh index.**
     * LIME_RenderScene sets it from ldrb.w sl, [r6, #4] and passes the
     * clamped frame as the fourth. An earlier body here had the two the other
     * way round, which put a frame number where FlushTranspMeshList indexes
     * meshes[]. */
    slot->meshIndex = arg4;             /* +0x2c */
    (void)arg3;                         /* the clamped frame; nothing reads it */

    g_transpMeshCount++;
    if (g_transpMeshCount > 0xff)
        for (;;) { }                    /* b . -- the retail build hangs here */
}


/* ----------------------------------------------------- FlushTranspMeshList
 *
 * armv6 0x000825d0, 528 bytes.  **Structurally complete.**
 *
 * Draws everything AddToTranspMeshList collected, then the frame is done.
 *
 * ## The transparency model, in the first two instructions
 *
 *      bl  _limeEnableAlphaBlending_Additive
 *      bl  _limeDisableDepthWrites
 *
 * **Additive blending with depth writes off** -- and that single choice
 * explains the whole design. Additive blending is commutative: `a + b + c`
 * gives the same pixel in any order. So the list is drawn **in insertion order
 * with no depth sort anywhere**, because it does not need one.
 *
 * This is why a fixed 255-entry array with no ordering is adequate rather than
 * naive. It is also the thing most likely to be "improved" by mistake in a
 * port: adding a back-to-front sort costs time and changes nothing, while
 * switching to standard alpha blending to make smoke look denser makes the
 * result **order-dependent** and the absence of a sort becomes a real bug.
 *
 * Depth *testing* is left on -- only writes are disabled -- so transparent
 * meshes are still occluded by opaque geometry but never occlude each other.
 *
 * ## Per item
 *
 * Push the matrix stack, expand the stored QST through
 * `ConvertQSTMatrixtoPCMatrix` -- reading from **`item + 8`**, which confirms
 * the offset AddToTranspMeshList writes it to -- check the flag byte at
 * `item + 5`, call `LIME_RenderMesh`, then `LIME_PopMatrix(1)`.
 *
 * One push and one pop per item, so a mesh that returns early still leaves the
 * stack balanced.
 *
 * The branch structure around the flag byte and the two texture pointers is not
 * transcribed; the loop body has several early exits whose ordering does not
 * change what is drawn, and the body below leaves them out rather than guess
 * their order.
 */

int SceneRenderAlwaysTrans;             /* 0x00171760 */
int SkipFrame86;                        /* 0x00171774 */
float SceneTint[4] = { 1.0f, 1.0f, 1.0f, 1.0f };  /* 0x00171764, 16 bytes */
float m44[16];                          /* 0x002c3e88, the scratch matrix */

static int name_is(const char *s, const char *prefix)
{
    while (*prefix)
        if (*s++ != *prefix++)
            return 0;
    return 1;
}

/* The bone path shared by both renderers (0x5fc86 and 0x5f704): bone `b`
 * of a skin's 3x4 palette, widened to 4x4 around an identity, multiplied
 * onto m44. This is how a scene mesh rides a fighter's bone -- Kung Lao's
 * hat, a ponytail. */
static void scene_attach_to_bone(const SKINMATRIX43 *bones, int b)
{
    const float *s = (const float *)((const char *)bones + b * 48);
    float t[16], out[16];

    limeMatrixLoadIdentity(t);
    t[0] = s[0];  t[1] = s[1];  t[2] = s[2];
    t[4] = s[3];  t[5] = s[4];  t[6] = s[5];
    t[8] = s[6];  t[9] = s[7];  t[10] = s[8];
    t[12] = s[9]; t[13] = s[10]; t[14] = s[11];
    limeMatrixMult(m44, t, out);
    memcpy(m44, out, sizeof(out));
}

/* armv7 0x0005f640, transcribed whole.
 *
 * The tint is `_SceneTint` (0x00171764, 1.0f 1.0f 1.0f in the image); the
 * earlier body used a stand-in array nothing wrote, so every translucent
 * mesh was drawn black. And the field05 path -- a mesh carried by a bone of
 * the skin it is drawn with, `matrix[field05]` multiplied onto the key's
 * matrix -- was not written; it is now.
 *
 * It does not clear the list: LIME_RenderScene does that on entry when it
 * is the flushing call (0x5fc80). The ClearTranspMeshList at the end is
 * kept from the earlier body, where an oracle run showed the list empty
 * after a flush; with every flush preceded by that clear it changes
 * nothing. */
void FlushTranspMeshList(TEXTURE *texture, const SKINMATRIX43 *matrix)
{
    int i;

    limeEnableAlphaBlending_Additive();
    limeDisableDepthWrites();

    for (i = 0; i < g_transpMeshCount; i++) {
        TRANSPMESH  *item = &g_transpMeshList[i];
        MESHSETINFO *set  = (MESHSETINFO *)item->meshset;
        MESHINFO    *mesh;

        LIME_PushMatrix();
        ConvertQSTMatrixtoPCMatrix(&item->qst, m44);
        if (item->field05 != 0)
            scene_attach_to_bone(matrix, item->field05);

        /* The translation goes through glTranslatef and is zeroed before
         * the multiply: 0x5f6be..0x5f6d2. */
        glTranslatef(m44[12], m44[13], m44[14]);
        m44[12] = m44[13] = m44[14] = 0.0f;
        glMultMatrixf(m44);

        glColor4f(SceneTint[0], SceneTint[1], SceneTint[2], item->alpha);
        limeDisableDepthWrites();

        mesh = set->meshes[item->meshIndex];
        if (mesh->field48 != 0)
            LIME_RenderMesh(set, (int)item->meshIndex, texture, NULL, 0);
        else
            LIME_RenderMesh(set, (int)item->meshIndex, mesh->texture, NULL, 0);

        LIME_PopMatrix(1);
    }

    limeDisableAlphaBlending();
    limeEnableDepthWrites();
    ClearTranspMeshList();
}


/* ----------------------------------------------------------- LIME_RenderScene
 *
 * armv6 0x000827e0, 1896 bytes.  **Structurally complete.**
 *
 * Draws a whole scene for one frame. The largest function in this file, and it
 * differs from LIME_RenderSceneOverrideTextures in one substantial way: **it
 * interpolates between two keys, and the override version does not.**
 *
 * ## Two frames, two palette matrices, one blend
 *
 *      ldrh r0, [r6, #6]            ; key A's palette index
 *      bl   GetMatrixFromPalette
 *      ldrh r0, [r1, #6]            ; key B's
 *      bl   GetMatrixFromPalette
 *      bl   LerpQSTMatrix
 *
 * `__modsi3` is called **twice** in the prologue -- once per frame -- and the
 * stream is read at two offsets (`ldrh r3, [r3, r2]` and `ldrh r3, [ip, r2]`).
 * So the scene walks to the key for this frame and the key for the next, pulls
 * a QST matrix for each out of the palette, and blends them.
 *
 * That names a field the override path never touches: **`SCENENODEKEY+0x06` is
 * a palette index**, a `uint16` handed straight to GetMatrixFromPalette. The
 * 8-byte key is therefore alpha, mesh index, and palette index -- everything a
 * node needs for one keyframe, in eight bytes.
 *
 * `LerpQSTMatrix` re-quantises to `int16` at every element (see LIMEDS_Misc.c),
 * so the quantisation is part of how scene animation looks. A port that blends
 * in float throughout produces visibly smoother motion than the original, which
 * sounds like an improvement and is a behaviour change.
 *
 * ## Transparent nodes are deferred, not drawn
 *
 * Nodes needing blending go to `AddToTranspMeshList` instead of being drawn
 * here, which is what fills the list `FlushTranspMeshList` later empties. The
 * opaque state is restored on both exits, exactly as in the override version.
 *
 * ## The EVENT test again
 *
 * `cmp r3, #0x45` on the mesh name, same as the override path -- markers are
 * skipped rather than drawn. Two independent functions carrying the same
 * convention.
 *
 * ## What is not written out
 *
 * The body below covers the walk, the two-key blend and the transparent
 * deferral. It does **not** transcribe the flag handling around `LIME_printf`
 * and the second `cmp ip, #1` in the prologue: those select between paths that
 * were not traced, and the argument they test is not identified. Guessing which
 * branch a flag selects is how the previous attempt at this file's other
 * renderer went wrong.
 */
/* armv7 0x0005f7a4, transcribed whole. The earlier body (from armv6) wrote
 * only the transparent deferral and left the opaque draw "described, not
 * written" -- which is why every arena, and every attachment (hair, hats),
 * drew nothing.
 *
 * Per node, on frame `fa` (and `fb`, blended by `blend`):
 *
 *   - no keys, a hidden frame, node 1 while _SkipFrame86 is set, or a mesh
 *     named EVENT...: nothing drawn, opaque state restored;
 *   - alpha >= 0.97 and not the flushing call (arg8 == 0): DRAWN HERE --
 *     the key's matrix (blended when `blend` is nonzero, and multiplied by
 *     a bone of `flushMatrix` when the key's byte 5 names one), the
 *     material rules of the mesh's name (ALPHA..., ATST...), the scene
 *     tint with the key's alpha, back-face culling, and one of three
 *     textures: `flushTexture` when the mesh's +0x48 says so, `arg10` when
 *     its +0x4c does and one is given, else its own;
 *   - alpha < 0.97 on the flushing call (arg8 == 1): deferred to the
 *     transparent list, drawn by FlushTranspMeshList at the end.
 *
 * A stream entry of 0xffff on frame `fb` makes `fb` equal `fa` from that
 * node on (0x5f934: the register is overwritten, not a copy). */
void LIME_RenderScene(long arg1, SCENEINFO *scene,
                      long frameA, long frameB, float blend,
                      long arg6, long arg7,
                      long flush, TEXTURE *flushTexture, long arg10,
                      const SKINMATRIX43 *flushMatrix)
{
    MESHSETINFO *set;
    long node, fa, fb, n;

    (void)arg6; (void)arg7;

    if (scene == NULL)
        return;

    set = scene->meshset;
    n = scene->count2;
    fa = frameA % n;
    if (fa < 0) fa = 0;
    if (fa >= n) fa = n;
    glScalef(scene->scale, scene->scale, scene->scale);
    if (flush == 1)
        ClearTranspMeshList();
    LIME_printf((int)arg1, "", scene, fa, scene->nodeCount);

    if (scene->nodeCount != 0) {
        fb = frameB % n;
        if (fb < 0) fb = 0;
        if (fb >= n) fb = n;

        for (node = 0; node < scene->nodeCount; node++) {
            SCENENODEKEY *keys = scene->nodeKeys[node];
            uint16_t     *strm = scene->nodeStream[node];
            SCENENODEKEY *ka, *kb;
            MESHINFO     *mesh;
            const char   *name;
            int           mi;

            if (keys == NULL) {
                LIME_printf((int)arg1, "", node);
                goto restore;
            }
            if (strm[fb] == SCENE_NODE_HIDDEN)
                fb = fa;
            if (strm[fa] == SCENE_NODE_HIDDEN)
                goto restore;

            ka = &keys[strm[fa]];
            mi = ka->meshIndex;
            mesh = set->meshes[mi];
            name = mesh->meshName;
            if (node == 1 && SkipFrame86 != 0)
                goto restore;
            if (name_is(name, "EVENT")) {
                LIME_printf((int)arg1, "", node);
                goto restore;
            }
            kb = &keys[strm[fb]];
            LIME_printf((int)arg1, "", node, name, (double)ka->alpha);

            if (ka->alpha >= SCENE_OPAQUE_ALPHA && flush == 0) {
                limeDisableAlphaBlending();
                limeEnableDepthWrites();
                if (SceneRenderAlwaysTrans != 0)
                    limeEnableAlphaBlending_Basic();
                LIME_printf((int)arg1, "", mesh->texture);
                LIME_PushMatrix();

                if (blend == 0.0f) {
                    ConvertQSTMatrixtoPCMatrix((const QSTMATRIX *)
                        GetMatrixFromPalette(ka->paletteIndex, scene), m44);
                } else {
                    QSTMATRIX q;
                    LerpQSTMatrix(GetMatrixFromPalette(ka->paletteIndex, scene),
                                  GetMatrixFromPalette(kb->paletteIndex, scene),
                                  blend, &q);
                    ConvertQSTMatrixtoPCMatrix(&q, m44);
                }
                if (ka->field05 != 0)
                    scene_attach_to_bone(flushMatrix, ka->field05);

                glTranslatef(m44[12], m44[13], m44[14]);
                m44[12] = m44[13] = m44[14] = 0.0f;
                glMultMatrixf(m44);

                if (name_is(name, "ALPHA")) {
                    limeEnableAlphaBlending_Basic();
                    limeDisableDepthWrites();
                } else {
                    limeDisableAlphaBlending();
                    if (name_is(name, "ATST")) {
                        limeDisableAlphaBlending();
                        glAlphaFunc(GL_GREATER, 0.9f);  /* 0x3f666666 */
                        glEnable(GL_ALPHA_TEST);
                    }
                }
                glColor4f(SceneTint[0], SceneTint[1], SceneTint[2], ka->alpha);
                glEnable(GL_CULL_FACE);

                {
                    float lit;
                    TEXTURE *tex;
                    memcpy(&lit, &scene->field70, 4);
                    if (mesh->field48 != 0)
                        tex = flushTexture;
                    else if (arg10 != 0 && mesh->field4c != 0)
                        tex = (TEXTURE *)(uintptr_t)arg10;
                    else
                        tex = mesh->texture;
                    LIME_RenderMesh(set, mi, tex, NULL, (long)(int)lit);
                }

                limeEnableDepthWrites();
                glDisable(GL_ALPHA_TEST);
                LIME_PopMatrix(1);
                continue;
            }

            if (ka->alpha < SCENE_OPAQUE_ALPHA && flush == 1) {
                if (blend == 0.0f) {
                    AddToTranspMeshList(set, (const SCENENODE *)ka,
                        (const QSTMATRIX *)GetMatrixFromPalette(ka->paletteIndex,
                                                                scene),
                        fa, mi);
                } else {
                    QSTMATRIX q;
                    uint8_t   tmp[8];
                    float     alpha = ka->alpha;

                    memset(tmp, 0, sizeof(tmp));
                    memcpy(tmp, &alpha, sizeof(alpha));
                    tmp[5] = ka->field05;
                    LerpQSTMatrix(GetMatrixFromPalette(ka->paletteIndex, scene),
                                  GetMatrixFromPalette(kb->paletteIndex, scene),
                                  blend, &q);
                    AddToTranspMeshList(set, (const SCENENODE *)tmp, &q, fa, mi);
                }
            }
            continue;

        restore:
            limeDisableAlphaBlending();
            limeEnableDepthWrites();
        }
    }

    if (flush == 1)
        FlushTranspMeshList(flushTexture, flushMatrix);
}


/* ------------------------------------------- LIME_RenderSceneOverrideTextures
 *
 * armv7 0x0005f4d4, 364 bytes.
 *
 * The same walk with a caller-supplied texture per mesh. This is how one scene
 * asset serves several characters or several palettes.
 *
 * ## Its argument list is not the same shape as LIME_RenderScene
 *
 *      mov  r8, r0            ; arg1 IS the scene here
 *      str  r1, [sp, #4]      ; arg2, kept for the loop
 *      mov  r0, r2            ; arg3 is the frame
 *      blx  ___modsi3
 *
 * So the scene comes first and the frame is still third. And arg2 is not one
 * texture but a TABLE of them, indexed by the same byte that selects the mesh:
 *
 *      ldr   r3, [sp, #4]           ; arg2
 *      ldr.w r2, [r3, r6, lsl #2]   ; textures[meshIndex]
 *      bl    _LIME_RenderMesh
 *
 * An earlier version of this file declared it as
 * `(scene, frame, TEXTURE *replacement, flags)` and passed a single texture.
 * See docs/RENDERSCENE-SIGNATURE.md for how the real list was measured.
 *
 * ## The palette index comes from the KEY, not from the frame
 *
 *      ldrh r0, [r5, #6]            ; key->paletteIndex
 *      bl   _GetMatrixFromPalette
 *
 * The previous body passed the frame number. That agrees whenever a scene lays
 * its keys out one per frame in order -- which is exactly the scene a casual
 * test would build, and wrong for every scene that reuses a pose. The whole
 * point of the two-level table is that poses repeat.
 *
 * ## What is deliberately not written out
 *
 * A global at `0x00112222 + pc` gates an alternate path at 0x5f608, and the
 * alpha test here is against **1.0f** where LIME_RenderScene uses 0.97.
 * Neither is transcribed further, because neither was followed.
 */
/* armv7 0x0005f4d4, transcribed whole. What the armv6 reading had left
 * open:
 *
 *   - the gate at 0x5f53e is _SceneRenderAlwaysTrans: when it is set (the
 *     FIGHT / FINISH HIM overlay sets it around this one call), every node
 *     is drawn with basic alpha blending -- the texture's own alpha;
 *   - a node whose key alpha is 0 is skipped, one at exactly 1.0 restores the
 *     opaque state first; nothing calls glColor4f;
 *   - the mesh is found BY NAME (LIME_FindMeshByName on the key's mesh's
 *     name), and the texture is textures[that index];
 *   - a node with no keys is skipped without restoring state. */
int g_overrideBlendFlag;                /* kept for the differential tests */

void LIME_RenderSceneOverrideTextures(SCENEINFO *scene, TEXTURE **textures,
                                      long frame)
{
    MESHSETINFO *set = scene->meshset;              /* +0x80 */
    long n = scene->count2, f, node;

    f = frame % n;
    if (f < 0) f = 0;
    if (f > n) f = n;

    glScalef(scene->scale, scene->scale, scene->scale);

    for (node = 0; node < scene->nodeCount; node++) {
        SCENENODEKEY *keys = scene->nodeKeys[node];
        uint16_t     *strm = scene->nodeStream[node];
        SCENENODEKEY *key;
        const char   *name;
        float m[16];
        int index;

        if (keys == NULL)
            continue;
        if (strm[f] == SCENE_NODE_HIDDEN)
            goto restore;

        key  = &keys[strm[f]];
        name = set->meshes[key->meshIndex]->meshName;
        if (name[0] == 'E' && name[1] == 'V' && name[2] == 'E' &&
            name[3] == 'N' && name[4] == 'T')
            goto restore;

        index = LIME_FindMeshByName(set, name);
        if (key->alpha == 0.0f)
            goto restore;
        if (key->alpha == 1.0f) {
            limeDisableAlphaBlending();
            limeEnableDepthWrites();
        }
        if (SceneRenderAlwaysTrans != 0)
            limeEnableAlphaBlending_Basic();
        if (index == -1) {
            printf("Can't find mesh match on %s.\n", name);  /* 0x001716a8 */
            continue;
        }

        LIME_PushMatrix();
        ConvertQSTMatrixtoPCMatrix(
            (const QSTMATRIX *)GetMatrixFromPalette(key->paletteIndex, scene), m);
        glMultMatrixf(m);
        LIME_RenderMesh(set, index, textures[index], NULL, 0);
        LIME_PopMatrix(1);
        continue;

    restore:
        limeDisableAlphaBlending();
        limeEnableDepthWrites();
    }
}


/* ------------------------------------------------------------ LIME_SceneMeshSet
 *
 * The scene's meshset, by name.
 *
 * Not in the original: gamecode reached the field directly, as `scene[0x80/4]`,
 * because on the device that arithmetic was right. Here it is not -- SCENEINFO
 * opens with a 64-byte name and several pointers, so the host puts `meshset`
 * somewhere else entirely, and the old spelling handed the caller the
 * characters of "KUNGLAO_" as an address.
 *
 * It lives here rather than in Players.c because this is the file that states
 * the layout, and gamecode including lime.h collides with the declarations it
 * keeps locally.
 */
void *LIME_SceneMeshSet(void *scene)
{
    return scene ? ((SCENEINFO *)scene)->meshset : NULL;
}
