/*
 * lime.h — tipos del motor LIME.
 *
 * Los nombres de struct son los REALES: salen de los simbolos C++ mangled que
 * el binario conserva sin stripping. Por ejemplo
 *
 *   __Z21LIME_RenderMeshSingleP8MESHINFOP7TEXTURES2_fl
 *     -> LIME_RenderMeshSingle(MESHINFO*, TEXTURE*, TEXTURE*, float, long)
 *   __Z6Xform2P11limeVECTOR3S0_S0_S0_P12SKINMATRIX43f
 *     -> Xform2(limeVECTOR3*, ..., SKINMATRIX43*, float)
 *
 * Los offsets de MESHINFO / MESHSETINFO estan verificados contra el
 * desensamblado de _LIME_LoadMeshSet: ver OUTPUT/meshset-format.md seccion 6.
 */

#ifndef LIME_H
#define LIME_H

#include "lime_glapi.h"
#include <stdint.h>
#include <stddef.h>

/* ------------------------------------------------------------------ */
/* Vectores                                                             */
/* ------------------------------------------------------------------ */

typedef struct {
    float x, y, z;
} limeVECTOR3;

typedef struct {
    float u, v;
} limeVECTOR2;

/* Matriz 4x4 en orden FILA-MAYOR. Verificado, no supuesto: los tests del
 * oraculo comprueban A*I == A y RotZ(a)*RotZ(b) == RotZ(a+b). */
typedef float limeMATRIX44[16];

/* ------------------------------------------------------------------ */
/* Mallas                                                               */
/* ------------------------------------------------------------------ */

/* Vertice tal y como queda en memoria tras cargar: 16 bytes.
 * En disco ocupa 26 y el loader descarta 12 (probablemente la normal). */
typedef struct {
    int16_t x, y, z;    /* posicion, hay que dividir entre 32767.0f */
    int16_t pad;
    float   u, v;
} LIMEVERTEX;

/* Una malla. El original la reserva con limeMalloc(tag, 0x58). */
/* Two fields, both from two functions. LIME_RenderMeshSingle and
 * LIME_RenderMeshSingleIndexed each read `+0x40` and hand it to glBindTexture,
 * and each tests `+0x50` alongside MESHINFO.fullBright to choose between the lit
 * and unlit paths.
 *
 * `+0x50` is left numbered. It sits at the same offset as MESHINFO's fullBright
 * and is tested in the same breath, which is suggestive and is not two
 * independent sightings of a meaning. Everything between and after is unmapped
 * padding, sized to reach the fields rather than measured. */
typedef struct TEXTURE {
    uint8_t  _pad00[0x40];
    unsigned name;               /* 0x40  the GL texture name */
    uint8_t  _pad44[0x0c];
    int      field50;            /* 0x50  gates the lit path, with the mesh's */
} TEXTURE;

typedef struct MESHINFO {
    int         numVerts;        /* 0x00 */
    int         numFaces;        /* 0x04 */
    uint8_t     _pad08[8];       /* 0x08 */
    float       boundsRadius;    /* 0x10 */
    uint8_t     _pad14[4];       /* 0x14 */
    LIMEVERTEX *P32 verts;           /* 0x18  numVerts * 16 bytes */
    uint16_t   *P32 indices;         /* 0x1c  numFaces * 3 uint16 */
    uint8_t     _pad20[4];       /* 0x20 */
    uint8_t    *P32 vertLight;       /* 0x24  un byte de luz por vertice */
    uint8_t     _pad28[20];      /* 0x28 */
    char       *P32 meshName;        /* 0x3c  buffer de 64 */
    char       *P32 textureName;     /* 0x40  buffer de 64 */
    TEXTURE    *P32 texture;         /* 0x44  LIME_FreeMeshSetTextures reads this
                                  *       and passes it to limeDeleteTexture */
    int         field48;        /* 0x48  FlushTranspMeshList: nonzero means
                                  *       use the CALLER{}s texture, zero means
                                  *       use this mesh own at +0x44 */
    int         field4c;         /* 0x4c  LIME_RenderScene: nonzero means
                                  *       the caller's second texture */
    int         fullBright;      /* 0x50  IsTextureFullBright(textureName) */
    int         visible;         /* 0x54  LIME_FreeNonVisibleMeshes tests == 0 */
} MESHINFO;

/* Un archivo .meshset cargado. limeMalloc(tag, 0x4C).
 * El tag que usa el original es literalmente "meshsethandle". */
typedef struct MESHSETINFO {
    char       name[64];         /* 0x00  se copia el nombre del archivo */
    int        texturesLoaded;   /* 0x40 */
    int        numMeshes;        /* 0x44 */
    MESHINFO *P32 *P32 meshes;           /* 0x48  tag "meshset_meshes" */
} MESHSETINFO;


/* ==================================================================== */
/* Engine types                                                         */
/*                                                                      */
/* Every offset below is one that docs/SKIN-FORMAT.md, docs/EVENTS-      */
/* FORMAT.md or a comment in decomp/lime/ sources derives from the            */
/* disassembly. Types whose layout is NOT established are declared       */
/* opaque rather than given plausible fields -- the code only passes     */
/* those by pointer, and inventing a field here would corrupt every      */
/* function that touched it.                                            */
/* ==================================================================== */

/* A 4x3 affine matrix: three rows of rotation, one row of translation, all
 * stride 3. Confirmed four independent ways -- MatrixIdentity2 writes 1.0f at
 * m[0], m[4], m[8]; MatrixMul2 walks a row of `a` against a column of `b` at
 * 0, 0xc, 0x18; and MatrixMul2's fourth row ADDS b[9..11] rather than
 * multiplying, which is what makes it a translation. */
typedef struct SKINMATRIX43 {
    float m[9];                  /* rotation, row-major, m[row*3 + col] */
    float t[3];                  /* translation -- the last three floats */
} SKINMATRIX43;

/* Quaternion, Scale, Translation in int16 fixed point (x 1/32767).
 * ConvertQSTMatrixtoPCMatrix reads four int16 at +0x00..+0x06 and immediately
 * squares and cross-multiplies them, which is quaternion-to-matrix and nothing
 * else -- so the Q is literal. The full element count is what LerpQSTMatrix
 * iterates. */
/* Quaternion, Scale, Translation -- and only the quaternion is fixed point.
 * Its scale is the double 3.0518509447574615e-05, NOT 1/32767; see
 * ConvertQSTMatrixtoPCMatrix.
 * ConvertQSTMatrixtoPCMatrix reads all ten fields and settles every one:
 *
 *      ldrsh  r3, [r0]          x4, scaled into double by the literal
 *      vldr   s15, [r0, #0x08]  a FLOAT, multiplying row 0
 *      vldr   s15, [r0, #0x0c]  row 1
 *      vldr   s15, [r0, #0x10]  row 2
 *      ldr    r3, [r0, #0x14]   x3, copied verbatim to m[12..14]
 *
 * 32 bytes exactly, which is what AddToTranspMeshList copies per entry.
 *
 * **The scale and translation are floats, not int16.** An earlier version of
 * this header modelled the whole struct as an int16 array, which would have put
 * every field after the quaternion at the wrong offset. */
typedef struct QSTMATRIX {
    int16_t q[4];                /* 0x00  x, y, z, w -- w LAST */
    float   scale[3];            /* 0x08  per-axis, multiplying each ROW */
    float   translation[3];      /* 0x14  copied straight into m[12..14] */
} QSTMATRIX;

/* LerpQSTMatrix blends a whole QSTMATRIX, and it treats the two halves
 * differently -- which is what settles the question this comment used to leave
 * open. It reads int16 at 0x00, 0x02, 0x04 and 0x06, and FLOATS at 0x08, 0x0c,
 * 0x10, 0x14, 0x18 and 0x1c. Four plus six, exactly the struct above.
 *
 * So the re-quantisation this project documents applies to the QUATERNION only.
 * Scale and translation are blended in float and stay float. An earlier
 * QST_ELEMENTS of 16 modelled the whole 32 bytes as int16 and was wrong. */

/* One key in a bone animation track. GetSlerpedQ reads four floats at
 * +0x00..+0x0c as the rotation and writes 1.0f at +0x10 of its output. */
typedef struct BONEANIMFRAME {
    float q[4];                  /* 0x00  (x, y, z, w) -- w LAST */
    float field10;               /* 0x10  set to 1.0f unconditionally */
} BONEANIMFRAME;

/* A skeleton bone, 25 bytes on disk and 56 in memory. LIME_LoadBones turns the
 * nine signed child indices into pointers as `bones + index * 56`. */
typedef struct BONE {
    int32_t       numChildren;   /* 0x00 */
    float         x, y, z;       /* 0x04  offset from the parent */
    uint8_t       _pad10[4];     /* 0x10  the disk record holds the indices here */
    struct BONE  *P32 children[9];   /* 0x14  nine slots, NULL where the index was -1 */
} BONE;

typedef struct BONESINFO {
    BONE *P32 bones;                 /* 0x00 */
    int   numBones;              /* 0x04 */
} BONESINFO;

/* A loaded .skin. The six arrays are exactly the six LIME_FreeSkin releases --
 * allocation side and release side agree, which is the standard this project
 * holds field identifications to.
 *
 * Note +0x14 and +0x28: the FIRST matrix of each entry goes to +0x14 and the
 * second to +0x28, but the loader ALLOCATES +0x14 first and +0x28 second.
 * Reading allocation order as storage order gets these backwards. */
typedef struct SKININFO {
    struct SKININFO *P32 next;       /* 0x00  a second block, or NULL */
    int           numMatrices;   /* 0x04  N */
    int           numVerts;      /* 0x08  M */
    uint8_t       _pad0c[8];     /* 0x0c */
    SKINMATRIX43 *P32 matricesA;     /* 0x14  N x 48 */
    void         *P32 vertExtra;     /* 0x18  M x 6 */
    void         *P32 uvs;           /* 0x1c  M x 24 */
    int32_t      *P32 indexes;       /* 0x20  N x 4 */
    float        *P32 weights;       /* 0x24  N x 16 */
    SKINMATRIX43 *P32 matricesB;     /* 0x28  N x 48 */
} SKININFO;

/* One live effect in the runtime pool: 192 slots of 248 bytes, confirmed three
 * ways. State 0 is free, > 0 is live, and NEGATIVE is dying -- LIME_UpdateEvents
 * counts negatives UP to zero, so a killed event at -2 gets a two-frame grace
 * period before the slot is reused. */
#define EVENT_SLOTS      192
#define EVENT_STRIDE     0xF8    /* 248 bytes IN THE BINARY -- see below */

/* **A warning about every stride in this header.**
 *
 * The sizes recorded here are the original's, and the original is 32-bit ARM
 * where a pointer is 4 bytes. Compiled for a 64-bit host every struct holding
 * pointers is larger: EVENT is 248 there and 256 here, BONE is 56 and 96,
 * MESHINFO is 88 and 120.
 *
 * That is fine, and it is not a layout error -- the field ORDER is what these
 * definitions describe, and nothing in the clean C reaches into them by offset.
 * But it means a hard-coded stride is only ever correct for memory the engine
 * treats as raw bytes, such as a loaded file buffer. **Never step an array of
 * host structs by the binary's byte stride.** LIME_UpdateEvents did exactly
 * that, walked off the end of a 192-slot array, and segfaulted with no output
 * because stdout was still buffered. Index the array and let the compiler size
 * the step. */
#define EVENT_KILL_VALUE 100.0f  /* +0xa4 and +0xe4 on a kill: 0x42c80000, the
                                  * literal at 0xa4354 and 0xa446c */

typedef struct SCENEEVENTTRACK SCENEEVENTTRACK;

typedef struct EVENT {
    int              state;      /* 0x00 */
    float            cursor;     /* 0x04 */
    int              frameA;     /* 0x08 */
    int              frameB;     /* 0x0c */
    struct SCENEINFO *P32 scene;     /* 0x10  `ldr ip, [r4, #0x10]` then
                                  *       `ldr r3, [ip, #0x44]` for count2 */
    SCENEEVENTTRACK *P32 track;      /* 0x14 */
    float            color[4];   /* 0x18  track +0x08..+0x14, RGBA */
    float            step;       /* 0x28  added to the cursor each frame;
                                  *       track +0xc4 */
    int              repeat;     /* 0x2c  -1 loops forever; track +0x70 */
    int              repeat2;    /* 0x30  a second counter; track +0x1c */
    int              field34;    /* 0x34  track +0x20 */
    int              delay;      /* 0x38  ticks down before the event starts;
                                  *       track +0x6c */
    /* int32_t, not long. The binary's field is 4 bytes; `long` is 4 on MinGW
     * and 8 on Linux, so writing `long` here would give the struct a different
     * field order on the two hosts for no reason. `long` stays in the SIGNATURE
     * of KillAlleventsWithGroup, where the mangled name (...Groupl) demands it. */
    int32_t          group;      /* 0x3c  track +0x7c */
    int              field40;    /* 0x40  trigger argument 5; passed to
                                  *       LIME_TriggerEventsFromScene, and gates
                                  *       the scene translate */
    int              field44;    /* 0x44  trigger argument 11; gates glCullFace */
    int              field48;    /* 0x48  trigger argument 8; a gate in
                                  *       LIME_RenderEvents */
    int              field4c;    /* 0x4c  track +0x24, zeroed for a shadow;
                                  *       compared against 1: a mode */
    float            offX;       /* 0x50  the event's own offset, applied */
    float            offY;       /* 0x54  after the scene's position -- from */
    float            offZ;       /* 0x58  MasterEventOffsets, or zero */
    int              offsetId;   /* 0x5c  track +0xc0, a MasterEventOffsets row */
    int              offsetId2;  /* 0x60  trigger argument 6, a second row
                                  *       added in, -1 for none */
    float           *P32 follow;     /* 0x64  trigger argument 3, the matrix the
                                  *       event follows */
    float            world[16];  /* 0x68  limeMatrixMult(m2, m1); its last
                                  *       element, +0xa4, is set on a kill */
    float            local[16];  /* 0xa8  a copy of m2; last element +0xe4 */
    /* Both handed to LIME_RenderScene, and the call site names them: +0xe8
     * lands in argument 9, which FlushTranspMeshList takes as its TEXTURE *,
     * and +0xec lands in argument 10, which nothing reads. */
    TEXTURE         *P32 flushTexture; /* 0xe8  trigger argument 9 */
    long             fieldEC;      /* 0xec  trigger argument 10 */
    int              isWhirlwind; /* 0xf0  IsWhirlwindScene(scene), decided once
                                   *       at spawn */
    void            *P32 obj;        /* 0xf4  LastGObj at spawn: the fight object
                                  *       KillIllegalWhirlwinds tests */
} EVENT;

/* The layout above is the binary's, offset for offset, on a 32-bit host --
 * which is the only host the fight runs on (see runtime/game_main.c). The
 * earlier version had a 0x64-byte pad where 0x48 belonged, so every field
 * past +0x58 sat 28 bytes late and the pool stepped 276 bytes a slot. */
#if UINTPTR_MAX == 0xffffffffu
typedef char event_layout_0xa4[offsetof(EVENT, world) + 60 == 0xa4 ? 1 : -1];
typedef char event_layout_0xf4[offsetof(EVENT, obj) == 0xf4 ? 1 : -1];
typedef char event_size_0xf8[sizeof(EVENT) == EVENT_STRIDE ? 1 : -1];
#endif

/* A track loaded from a .events file: 216 bytes, spelled out by LIME_LoadEvents
 * as count*32 - count*8 = count*24, times 8, plus the 24. Names are uppercased
 * in place at load across a 64-byte field, which is why nothing here ever needs
 * a case-insensitive compare. */
#define SCENEEVENTTRACK_STRIDE 216

struct SCENEEVENTTRACK {
    uint8_t _pad00[4];
    struct SCENEINFO *P32 scene;     /* 0x04  read as one by LIME_FreeEvents and by
                                  *       LIME_TriggerEvent -- two functions */
    float   f08, f0c, f10, f14;  /* 0x08  set to 1.0f by LIME_PlayFBXAtPos */
    uint8_t _pad18[4];
    int     v1c, v20, v24;       /* 0x1c  zeroed together */
    uint8_t _pad28[0x48];
    int     f70;                 /* 0x70 */
    uint8_t _pad74[4];
    int     f78;                 /* 0x78 */
    int     flag7c;              /* 0x7c  diverts LIME_TriggerEventFromSceneH */
    uint8_t _pad80[0x1c];
    int     field9c;             /* 0x9c  count for the +0xd4 array */
    uint8_t _pada0[0x24];
    float   fc4;                 /* 0xc4  set to 1.0f */
    uint8_t _padc8[8];
    int     maxInstances;        /* 0xd0  -1 disables the cap */
    void   *P32 arrayD4;             /* 0xd4  field9c x 68 bytes */
    uint8_t _padd8[SCENEEVENTTRACK_STRIDE - 0xd8];
};

typedef struct EVENTSINFO {
    int              count;      /* 0x00 */
    SCENEEVENTTRACK *P32 tracks;     /* 0x04 */
} EVENTSINFO;

/* Deferred transparent draws: 48 bytes, written as index*64 - index*16 and
 * walked back with a plain add #0x30. */
#define TRANSPMESH_MAX 255

/* FlushTranspMeshList reads five things out of this, and driving it named
 * three that were previously anonymous: the float at +0x00 is the ALPHA and
 * goes to glColor4f, the byte at +0x05 selects between two transform paths,
 * and +0x2c is the MESH INDEX into meshset->meshes rather than a spare
 * argument. See docs/RENDERSCENE-SIGNATURE.md. */
typedef struct TRANSPMESH {
    float        alpha;          /* 0x00  ldr r3, [r6] -> glColor4f 4th arg */
    uint8_t      _pad04;         /* 0x04 */
    uint8_t      field05;        /* 0x05  ldrb r3, [r6, #5]; cbnz -> 0x5f704 */
    uint8_t      _pad06[2];      /* 0x06 */
    QSTMATRIX    qst;            /* 0x08  32 bytes, copied verbatim */
    void        *P32 meshset;        /* 0x28 */
    long         meshIndex;      /* 0x2c  ldr r2, [r3, r1, lsl #2] */
} TRANSPMESH;

/* One debug text window -- and it is **53,024 bytes**, which this project spent
 * a long time unable to state.
 *
 * The size could not be read from the disassembly: the literal pools around
 * DS_DebugWin.c disassemble as `0xe12fff1e`, which is `bx lr` being read as
 * data, so the multiply's operand never resolved. The comment there said so and
 * refused to guess.
 *
 * The recompiler resolved it. In the generated oracle the same instruction is
 * `movw r3, #0xcf20` followed by `mul r0, r0, r3` -- 0xcf20 = 53,024 -- and
 * running ClearDebugWindow(0) and ClearDebugWindow(1) against a poisoned arena
 * confirms it: window 1's writes land at exactly +0xcf20.
 *
 * The layout falls out of the same measurement. Clearing one window touches 102
 * words: the two cursors, then a pair inside each of **50 line records of 0x420
 * bytes** starting at +0x18. Fifty is the line count already documented from
 * DW_NewLine's `0x31` bound, arrived at independently.
 *
 *      0x18 + 50 * 0x420 = 0xCF18, and the record is 0xCF20.
 *
 * A line being 1,056 bytes is worth pausing on: this is a debug overlay that
 * reserves 53 KB per window and sixty-two of them, 3.2 MB. It was never in a
 * shipped frame -- LIME_printf compiles to nothing -- and the memory was
 * reserved anyway.
 *
 * **Sixty-two, measured.** LIME_InitDebugWindow (armv7 0x000a7d88) loops to
 * `cmp r5, #0x3e`, and `_DebugWindows` at 0x00392024 runs to `_UpdateIndex` at
 * 0x006b49e4: 0x3229c0 bytes, which is 0x3e * 0xcf20 exactly. This said 16. */
#define DEBUG_WINDOWS      0x3e
#define DEBUG_LINES        0x31          /* 49 is the last valid index */
#define DEBUG_LINE_STRIDE  0x420         /* 1,056 bytes per line */
#define DEBUG_WINDOW_SIZE  0xCF20        /* 53,024 bytes per window */

typedef struct DEBUGLINE {
    int     field00;             /* +0x00  cleared with `str`  -- a word */
    uint8_t _pad04[0x1c];
    uint8_t flag20;              /* +0x20  cleared with `strb` -- ONE BYTE */
    uint8_t _pad21[DEBUG_LINE_STRIDE - 0x21];
} DEBUGLINE;

typedef struct DEBUGWINDOW {
    int       column;            /* 0x00 */
    int       line;              /* 0x04 */
    uint8_t   _pad08[0x10];
    DEBUGLINE lines[50];         /* 0x18, 50 x 0x420 */
    uint8_t   _padtail[DEBUG_WINDOW_SIZE - 0x18 - 50 * DEBUG_LINE_STRIDE];
} DEBUGWINDOW;

/* Layout not established -- these are only ever passed by pointer. */
/* One keyframe of a scene node: eight bytes, reached by index out of the node's
 * own stream. LIME_RenderSceneOverrideTextures reads the alpha as a float from
 * +0x00 (`vldr s15, [r6]`) and the mesh index as a byte from +0x04
 * (`ldrb r3, [r6, #4]`), then looks the mesh up in the meshset's array. */
/* Still opaque: AddToTranspMeshList takes one and copies two words out of it,
 * which is not enough to lay out. Distinct from SCENENODEKEY below. */
typedef struct SCENENODE     SCENENODE;

/* The stream value that means "draw nothing this frame". Compared against a
 * PC-relative literal this pass did not resolve, so the name stands in for the
 * constant rather than a guessed number. */
#define SCENE_NODE_HIDDEN 0xFFFFu

/* The cutoff LIME_RenderScene uses to decide a mesh is opaque. Read from the
 * literal at 0x0005fb14, NOT assumed: an earlier body tested alpha against
 * zero and deferred anything non-zero to the transparent list. The real rule
 * is that a mesh at 0.97 or above is drawn in the opaque pass.
 * LIME_RenderSceneOverrideTextures uses 1.0f instead -- the two do not share
 * this threshold. */
#define SCENE_OPAQUE_ALPHA 0.97f

typedef struct SCENENODEKEY {
    float    alpha;              /* 0x00  vldr s14, [r6] -- and compared against
                                  *       0.97f, not against zero. See
                                  *       docs/RENDERSCENE-SIGNATURE.md */
    uint8_t  meshIndex;          /* 0x04  ldrb, indexes MESHSETINFO.meshes */
    uint8_t  field05;            /* 0x05  READ, not padding: `ldrb r3, [r6, #5]`
                                  *       and copied into the temporary node
                                  *       LIME_RenderScene hands to
                                  *       AddToTranspMeshList. An earlier pass
                                  *       called this _pad05 on no evidence
                                  *       beyond it being unaccounted for.
                                  *       What it MEANS is not established. */
    uint16_t paletteIndex;       /* 0x06  ldrh, -> GetMatrixFromPalette */
} SCENENODEKEY;

/* The .events block as its consumers name it. Same pair EVENTSINFO holds; the
 * two names come from different call sites in the original. */
typedef struct SCENEEVENTS {
    long  numTracks;             /* 0x00 */
    void *P32 tracks;                /* 0x04  numTracks x 216 bytes */
} SCENEEVENTS;

/* A game object. Only the frame number is established here -- IsOnWWFrame reads
 * it at +0x08 and sign-extends from uint16. Everything else is gamecode's and
 * has not been mapped. */
typedef struct Mk3Obj_t {
    uint8_t  _pad00[8];
    uint16_t frame;              /* 0x08 */
} Mk3Obj_t;

/* A declarative texture manifest: a NULL-terminated array of 8-byte entries.
 * A module lists what it needs and where to put it, and one call fills them in.
 * The stride shows up as a pre-indexed load with writeback, `ldr r0, [r4, #8]!`. */
typedef struct TEXTURETOLOAD {
    const char *P32 name;            /* 0x00  NULL ends the list */
    TEXTURE   *P32 *P32 dest;            /* 0x04  where the handle goes */
} TEXTURETOLOAD;

/* A font: two texture atlases and a metrics file. See docs/FONT-FORMAT.md --
 * the metric arrays at +0x1c and +0x24 are deliberately unnamed because nothing
 * decompiled so far reads them. limeDrawFONT will settle them. */
typedef struct FONT {
    uint8_t   _pad00[4];
    int       simple;            /* 0x04  stored INVERTED from the file flag */
    int       glyphHeight;       /* 0x08  header byte 2 -- ONE height for every
                                  *       glyph, which is why it is in the
                                  *       header and not in a per-glyph array */
    int       spacing;           /* 0x0c  added once per character by both
                                  *       width routines -- inter-character
                                  *       spacing, set from limeCreateFONT */
    int       fallbackAdvance;   /* 0x10  the constant 8 */
    /* A FLOAT. Both width routines read it with `vldr s15, [r4, #0x14]`, which
     * reinterprets the bits -- it does not convert an integer. An earlier
     * version of this header typed it `int` and the clean C cast it, which
     * gives the right answer only when the caller happened to pass a small
     * whole number. */
    float     field14;           /* 0x14  the scale applied at measure time */
    int16_t   numGlyphs;         /* 0x18 */
    uint8_t   _pad1a[2];
    /* The atlas rectangle of each glyph. limeDrawFONT settles all three by what
     * it divides them BY on the way to limeDrawSprite: atlasU and glyphWidth are
     * both normalised by +0x34, atlasV by +0x38. Two share a divisor, so two
     * share an axis. */
    int16_t  *P32 atlasU;            /* 0x1c  x position in the atlas */
    int16_t  *P32 atlasV;            /* 0x20  y position in the atlas */
    int16_t  *P32 glyphWidth;        /* 0x24  per-glyph width, and the advance
                                  *       limeGetStringWidth accumulates */
    /* Optional per-glyph kerning, one SIGNED byte each, added to the width when
     * the pointer is non-null. limeGetStringWidth is the only reader:
     *     ldr     r3, [r4, #0x28]
     *     cmp     r3, #0
     *     ldrsbne r3, [r3, r0]
     *     addne   r8, r8, r3
     * Nothing in limeCreateFONT was seen to allocate it, so a font that does not
     * carry kerning leaves it null and every glyph keeps its plain width. */
    int8_t   *P32 kerning;           /* 0x28 */
    int       defaultAdvance;    /* 0x2c  simple fonts only */
    uint8_t   _pad30[4];
    /* The atlas dimensions, and they are the OPPOSITE way round from an earlier
     * naming here. +0x34 divides the horizontal metrics and +0x38 the vertical,
     * which is what fixes them: the divisor names the axis. */
    float     atlasWidth;        /* 0x34 */
    float     atlasHeight;       /* 0x38 */
    /* Added to the width of a character that is NOT in the table, on top of
     * the fallback advance. Only the not-found branch reads it:
     *     ldr r3, [r4, #0x10]   ; the fallback
     *     ldr r2, [r4, #0x3c]   ; and this
     *     add r3, r3, r2
     * so a font can make unknown characters wider than the plain fallback
     * without touching the fallback itself. */
    int       extraUnknown;      /* 0x3c */
    /* limeDrawFONT (armv7 0x7e5b8): at the +0x44-th character not in the
     * table, +0x40 more is added. Both are zeroed by every call that is not
     * alignment 3. The earlier 12-byte pad here put `codes` at 0x4c. */
    int       extraAtCount;      /* 0x40 */
    int       countForExtra;     /* 0x44 */
    uint8_t  *P32 codes;             /* 0x48  one byte per glyph */
    int16_t  *P32 codesW;            /* 0x4c  the same codes widened to 16 bits */
    TEXTURE  *P32 texture0;          /* 0x50 */
    TEXTURE  *P32 texture1;          /* 0x54 */
} FONT;

/* ------------------------------------------------------------------ */
/* Engine globals                                                       */
/* ------------------------------------------------------------------ */

/* The name is the binary's own: _SceneEvents in __DATA, 0x00379cc0, exactly
 * 47,616 bytes -- 192 * 248. An earlier pass called this g_events, which was an
 * invention sitting next to a symbol table that had the real name all along. */
extern EVENT            SceneEvents[EVENT_SLOTS];
extern SCENEEVENTTRACK  g_fbxScratchTrack;   /* reused every LIME_PlayFBXAtPos */
extern limeMATRIX44     g_fbxScratchMatrix;
/* 80-byte rows: a 64-byte name, then x, y, z at +0x40, and a spare word.
 * The file holds them at 76 bytes a row (LIME_LoadMasterEventOffsets). */
extern char            *MasterEventOffsets;     /* 0x00177818 */
extern int              NumMasterEventOffsets;  /* 0x00177814 */
extern const char      *MasterOffsetsFilename;  /* 0x00177830 */
/* Declared with the struct tag: the full SCENEINFO definition lives further
 * down, after MESHSETINFO, and this block precedes it. */
struct SCENEINFO;
extern struct SCENEINFO *g_sceneList;

extern TRANSPMESH       g_transpMeshList[TRANSPMESH_MAX];
extern float            SceneTint[4];   /* 16 bytes: LIME_RenderEvents copies an RGBA in */
extern float            m44[16];
extern int              SceneRenderAlwaysTrans, SkipFrame86;
extern int              g_transpMeshCount;

/* The binary's own names: _DebugWindows is the ARRAY, and _DS_DebugWindowOn is
 * the enable flag. An earlier pass called them g_debugWindows and
 * g_debugWindowEnabled, which were inventions sitting next to a symbol table
 * that had both.
 *
 * It was also declared a pointer, read from ClearDebugWindow's `ldr r1, [r3]`.
 * That load is the non-lazy pointer slot at 0x00178bd4, whose contents are
 * 0x00392024 -- the symbol's own address, in __DATA,__common. There is no
 * second indirection. As a pointer nothing ever set it, and the first
 * ClearDebugWindow the character select makes segfaulted on NULL. */
extern DEBUGWINDOW      DebugWindows[DEBUG_WINDOWS];
extern int              DS_DebugWindowOn;

/* Lighting: two directional lights, monochrome, no ambient. See LIGHTING.md.
 * Held as bare float[3] rather than limeVECTOR3 -- NormaliseLDirs indexes them
 * with [0], [1], [2]. */
extern float            g_lightDir0[3], g_lightDir1[3];

/* Scales the lit value into a 0..255 grey byte in DrawSkinnedMesh2. The literal
 * is in a pool this pass did not resolve, so the name stands in for it rather
 * than a guessed number. */
extern const float      LIGHT_SCALE;

/* Skinning state, under the binary's names; see lime_globals.c for where each
 * lives and RenderSkinned.c for who writes it. */
extern SKINMATRIX43     MatrixPalette2[150];
extern unsigned char    SkinnedVerts[360000];
extern uint16_t         RenderIndexes[10000];
extern BONEANIMFRAME    DecompAnimFrames0[150];
extern BONEANIMFRAME    DecompAnimFrames1[150];
extern limeVECTOR3      Root_Trans0, Root_Trans1;
extern BONEANIMFRAME   *MatrixSource0;
extern BONEANIMFRAME   *MatrixSource1;
extern long             MatrixSourceCount;
extern SKINMATRIX43    *MatrixDst2;
extern unsigned char    TempRGBS[32000];
extern float            VertScale;

long   DrawSkinnedMesh2(SKININFO *skin, unsigned a, unsigned b, long flags,
                        limeVECTOR3 *outPos, limeVECTOR2 *outUV,
                        unsigned char *outCol, long uvCount, long keepFloats);
void   GenerateMatrices(char *data, BONESINFO *bones, long frameA,
                        long frameB, float t, long stride);
void   CreateMatrixPaletteForGeneratingMesh(char *data, long stride,
                                            long frameA, long frameB,
                                            float t, BONESINFO *bones);
void   LIME_RenderMeshSingleIndexed(void *rec, TEXTURE *tex, float alpha,
                                    const limeVECTOR3 *fade, long second);

void   UnpackAnimFrame(const uint8_t *src, BONEANIMFRAME *out,
                       limeVECTOR3 *pos, long numBones);
void   LerpVector3(const limeVECTOR3 *a, const limeVECTOR3 *b, float t,
                   limeVECTOR3 *out);
void   CreateMatrixPaletteRecurse2(BONE *bone, SKINMATRIX43 *parent);
extern float            g_lightPower0, g_lightPower1;
extern float            g_lightExp0, g_lightExp1;


/* GL enums the decompiled code names directly. */
#ifndef GL_MODELVIEW
#define GL_MODELVIEW  0x1700
#endif
#ifndef GL_PROJECTION
#define GL_PROJECTION 0x1701
#endif


/* ------------------------------------------------------------------ */
/* limeVector.cpp                                                       */
/* ------------------------------------------------------------------ */

float Len(const limeVECTOR3 *v);
void  Normalise(limeVECTOR3 *v);

/* ------------------------------------------------------------------ */
/* Matrix.cpp                                                           */
/* ------------------------------------------------------------------ */

void limeMatrixLoadIdentity(float *m);
void limeMatrixCopy(const float *src, float *dst);
void limeMatrixMult(const float *a, const float *b, float *out);
void RotMatrixX(float *m, float angle);
void RotMatrixY(float *m, float angle);
void RotMatrixZ(float *m, float angle);
void limeScaleMatrix(float *m, float scale);
void limeScaleMatrixXYZ(float *m, float sx, float sy, float sz);

/* Rota un vector por la 3x3, ignorando la traslacion:
 *   out[j] = suma_i vin[i] * m[i*4 + j] */
void limeMatrix3x4RotateSkin(const float *m, const limeVECTOR3 *vin,
                             limeVECTOR3 *vout);
void RotVector(const float *m, const limeVECTOR3 *vin, limeVECTOR3 *vout);

/* Punto de enganche del widescreen: `aspect` divide el termino X de la
 * proyeccion, asi que cambiarlo ensancha el campo de vision horizontal sin
 * tocar el vertical. Verificado por los tests del oraculo. */
void CreatePerspectiveMatrix(float *m, float fov, float aspect,
                             float zNear, float zFar);

/* ------------------------------------------------------------------ */
/* RenderMesh.cpp                                                       */
/* ------------------------------------------------------------------ */

MESHSETINFO *LIME_LoadMeshSet(const char *filename, int useLighting);
void         LIME_FreeNonVisibleMeshes(MESHSETINFO *set);
void         LIME_FreeMeshSet(MESHSETINFO *set);

/* Returns the INDEX of the first mesh whose name CONTAINS `name`, or -1.
 * An index, not a pointer, and a substring match, not an exact one — both
 * are easy to get wrong and both are verified. */
int          LIME_FindMeshByName(const MESHSETINFO *set, const char *name);

/* The second argument is real: RenderDebugCube passes 0 for it, and when it is
 * non-null the function composes three candidate texture names from it. The
 * single-argument form here was stale and only surfaced when the file was
 * first compiled. */
void         LIME_LoadMeshSetTextures(MESHSETINFO *set, const char *suffix);
void         LIME_FreeMeshSetTextures(MESHSETINFO *set);

/* Non-zero if the texture should be drawn without lighting. The original
 * decides by looking the name up in a data file. */
/* The full-bright opt-out list. A literal in the binary, not a variable --
 * see the note on _IsTextureFullBrightPath in RenderMesh.c. */
#define NOLIGHT_FILE "nolight.txt"


/* _TheFullBrightInfo occupies 4100 bytes in __DATA,__common -- the distance to
 * the next symbol, _NumTranspMeshes. That is exactly 4 + 64 * 64, so the table
 * holds **64 entries of 64 bytes** after a leading count, and `names[i]` lands
 * at `base + 4 + i * 64`, which is how the code addresses it.
 *
 * The retail nolight.txt uses 40 of the 64. **The parser has no bounds check**:
 * it copies and increments per accepted line with nothing testing the count, so
 * a modder adding 25 more entries writes straight through the end of the table
 * and into _NumTranspMeshes, the transparent-mesh counter. See docs/LIGHTING.md. */
#define FULLBRIGHT_MAX 64

typedef struct FULLBRIGHTINFO {
    int  count;                        /* +0x00 */
    char names[FULLBRIGHT_MAX][64];    /* +0x04, stride 0x40 */
} FULLBRIGHTINFO;

extern FULLBRIGHTINFO TheFullBrightInfo;
extern int            FullBrightLoaded;

/* RenderMesh.cpp: the fade lookup table CreateFadedLookupTable builds, 512
 * rows of 256 bytes, and the flag that says it exists. */
extern int      HaveFadeTable;
extern uint8_t  ScaleTable[0x200 * 256];
void CreateFadedLookupTable(void);

/* The debug overlay switch and the lazily loaded debug cube (RenderDebugCube). */
extern int   g_debugEnabled;
/* Built by LIME_LoadScene, which writes more of this structure than everything
 * else recovered so far combined. Fields are named only where two independent
 * functions agree; the rest keep their offsets as names rather than acquire a
 * plausible-sounding label.
 *
 * The leading name[64] is why LIME_LoadScene's first field write lands at 0x40:
 * three separate functions show a SCENEINFO beginning with its own filename. */
typedef struct SCENEINFO {
    char         name[64];       /* 0x00 */
    int          refCount;       /* 0x40  incremented on a cache hit,
                                  *       decremented by LIME_FreeScene */
    int          count2;         /* 0x44  the modulus LIME_TriggerEventsFromScene
                                  *       takes frame numbers against */
    long         nodeCount;      /* 0x48  the scene's node count, walked by
                                  *       both renderers */
    void        *P32 field4c;        /* 0x4c  limeMalloc sized from 0x44 and 0x48 */
    uint8_t      _pad50[4];      /* 0x50 */
    float        posX;           /* 0x54  fed straight to glTranslatef by     */
    float        posY;           /* 0x58  LIME_RenderEvents -- the scene origin */
    float        posZ;           /* 0x5c  an effect is placed relative to      */
    float        scale;          /* 0x60  handed to glScalef before the walk */
    int          field64;        /* 0x64  four words from another sibling */
    int          field68;        /* 0x68 */
    int          field6c;        /* 0x6c */
    int          field70;        /* 0x70 */
    void        *P32 field74;        /* 0x74 */
    int          field78;        /* 0x78 */
    void        *P32 tail;           /* 0x7c  GetMatrixFromPalette indexes this at
                                  *       32 bytes a step */
    MESHSETINFO *P32 meshset;        /* 0x80  LIME_LoadMeshSet result -- confirmed
                                  *       independently by RenderDebugCube */
    void        *P32 events;         /* 0x84  LIME_LoadEvents result */

    /* **The scene's animation, and it is a compressed two-level table.**
     *
     * Both are allocated by LIME_LoadScene (`str r0, [r4, #0x88]` at 0x82028 and
     * `[r4, #0x8c]` at 0x82040) and both are read per node per frame by the two
     * scene renderers:
     *
     *      ldr  r1, [sl, r3]        ; keys[node]     -- r3 is +0x88
     *      ldr  r3, [sl, r0]        ; stream[node]   -- r0 is +0x8c
     *      ldrh r3, [r2, r3]        ; stream[node][frame], a uint16
     *      lsl  r3, r3, #3
     *      add  r6, r3, r1          ; -> keys[node][index]
     *
     * So a node does not carry a key per frame. It carries a **stream of uint16
     * indices**, one per frame, into a **shared array of 8-byte keys**. Frames
     * that repeat a pose cost two bytes instead of eight, which is the whole
     * point -- and it means a port cannot walk keys by frame number.
     *
     * The cursor over both arrays steps FOUR bytes per node, so each is an array
     * of pointers indexed by node. */
    SCENENODEKEY *P32 *P32 nodeKeys;     /* 0x88  [node] -> the key array */
    uint16_t     *P32 *P32 nodeStream;   /* 0x8c  [node] -> one uint16 per frame */
    struct SCENEINFO *P32 next;      /* 0x90  the scene cache is a linked list --
                                  *       AddScene and LIME_GetSceneFromFilename
                                  *       both walk this offset */
} SCENEINFO;

extern SCENEINFO *g_debugCubeScene;
#define DEBUG_CUBE_SCENE "debugcube.scene"

void       *GetMatrixFromPalette(long index, SCENEINFO *scene);
void        AddToTranspMeshList(MESHSETINFO *meshset, const SCENENODE *node,
                                const QSTMATRIX *qst, long a3, long a4);
void        ClearTranspMeshList(void);
void LIME_GLAPI glScalef(float x, float y, float z);
SCENEINFO *LIME_LoadScene(const char *filename, int a, const char *b, int c);
void LIME_RenderMeshSingle(MESHINFO *mesh, TEXTURE *t0, TEXTURE *t1,
                           float alpha, long flags);
void LIME_FreeSingleMesh(MESHSETINFO *set, int index);

int          IsTextureFullBright(const char *textureName);

/* ------------------------------------------------------------------ */
/* Provided by the platform layer                                       */
/* ------------------------------------------------------------------ */

/* Reads a file whole. Returns a buffer the caller owns and must limeFree(), or
 * NULL.
 *
 * **One argument, with the size fetched separately.** This header previously
 * declared `limeLoadFile(path, size_t *out)` -- the shape a PC runtime would
 * prefer -- and that was the only place in the tree using it. The binary has
 * the one-argument form with a separate limeFileSize(), and
 * runtime/arm_runtime.c already implements it that way as stub_limeLoadFile /
 * stub_limeFileSize. The decompiled code describes the original, so the
 * original signature wins and the runtime needs no change. */
void  *limeLoadFile(const char *path);
size_t limeFileSize(const char *path);
void   limeFree(void *p);
void  *limeMalloc(const char *tag, size_t bytes);
TEXTURE *limeLoadTexture(const char *path, int a, int b);
void   MatrixMul2(const SKINMATRIX43 *a, const SKINMATRIX43 *b, SKINMATRIX43 *out);
void   MatrixIdentity2(SKINMATRIX43 *m);
void   GetMFromQuat2(const BONEANIMFRAME *q, SKINMATRIX43 *out);
void   GetSlerpedQ(const BONEANIMFRAME *a, const BONEANIMFRAME *b,
                   float t, BONEANIMFRAME *out);
void   Xform2(const limeVECTOR3 *unused0, const limeVECTOR3 *vin,
              const limeVECTOR3 *unused2, limeVECTOR3 *vout,
              const SKINMATRIX43 *m, float w);
void   NormaliseLDirs(void);
/* The mangled name is __Z9LightVertP11limeVECTOR3S0_ -- two limeVECTOR3* -- but
 * the recovered body writes through a bare float*. Same three floats either
 * way; the definition's shape is kept. */
void   LightVert(const limeVECTOR3 *normal, float *out);
void   DS_ScrollLines(DEBUGWINDOW *win);
void   CreateMatrixPaletteForGeneratingMesh(char *a, long b, long c, long d,
                                            float e, BONESINFO *bones);
const char *LIME_LoadSkin1(const char *data, SKININFO *skin);

/* The matrix-stack wrappers and the blend-state helpers the renderers call.
 * All real symbols in the binary; the blend pair is what makes the transparent
 * list order-independent -- see FlushTranspMeshList. */
void   LIME_PushMatrix(void);
void   LIME_PopMatrix(int count);
/* Signature taken from the existing definition in RenderMesh.c, which was
 * recovered from the binary -- an earlier guess here had four different
 * parameters and only the compiler caught it. */
/* The scratch buffer LIME_RenderMeshSingle fills with faded vertex colours and
 * then draws from. Its symbol was not resolved -- the pointer arrives in r8 from
 * earlier in the function -- so this name stands in for it, the same way
 * LIGHT_SCALE stands in for an unresolved literal. */
extern uint8_t *g_vertexColourScratch;
extern float StaticMeshAmbient[3];     /* 0x002bfe74 */

void   LIME_RenderMesh(MESHSETINFO *set, int index, TEXTURE *tex0, TEXTURE *tex1, long flags);
void   ConvertQSTMatrixtoPCMatrix(const QSTMATRIX *src, float *dst);
void   LerpQSTMatrix(const QSTMATRIX *a, const QSTMATRIX *b, float t,
                     QSTMATRIX *out);
void   limeEnableAlphaBlending_Additive(void);
void   limeEnableAlphaBlending_Basic(void);
void   limeDisableAlphaBlending(void);
void   limeEnableDepthWrites(void);
void   limeDisableDepthWrites(void);
/* **The argument lists here were measured, not read.** Both were previously
 * declared with the scene first and a `flags` word last; driving them showed
 * the scene is the SECOND argument of LIME_RenderScene and that neither takes
 * anything resembling `flags`. Full derivation, including the experiment that
 * settled it, is in docs/RENDERSCENE-SIGNATURE.md.
 *
 * Names are given only where the use is established. `arg1`, `arg6`, `arg7`
 * and `arg10` are numbered rather than named because nothing yet says what
 * they are, and arg6/arg7 are never read at all. */
void   LIME_RenderScene(long arg1, SCENEINFO *scene,
                        long frameA, long frameB, float blend,
                        long arg6, long arg7,
                        long flush, TEXTURE *flushTexture, long arg10,
                        const SKINMATRIX43 *flushMatrix);
void   LIME_RenderSceneOverrideTextures(SCENEINFO *scene, TEXTURE **textures,
                                        long frame);
void LIME_GLAPI glCullFace(unsigned mode);
#ifndef GL_BACK
#define GL_BACK 0x0405
#endif
void   RenderDebugCube(void);
void   ConvertDSMatrixtoPCMatrix(const int32_t *src, float *dst);
void   LIME_printf(int window, const char *fmt, ...);
int    GetFreeEvent(void);
int    LIME_CountActiveEvents(void);
void   KillAlleventsWithGroup(long group);
void   LIME_UpdateEvents(void);
void   LIME_KillSliders(void);
float  limeGetStringWidth(const FONT *font, const char *text);
float  limeGetStringWidthUCNoHeader(const FONT *font, const char *text);
void   ClearDebugWindow(int index);
void   DW_NewLine(DEBUGWINDOW *win);
void   limeDrawSprite(TEXTURE *page, float x, float y, float w, float h,
                      float u, float v, float du, float dv,
                      const float *colour);
void   limeDrawRotSpriteFromTopLeft(TEXTURE *page, float x, float y,
                                    float w, float h, float u, float v,
                                    float du, float dv, float angle,
                                    const float *colour);
int    FindIdInMasterOffsets(const char *name);
struct SCENEINFO *LIME_SceneExists(struct SCENEINFO *scene);
void   LIME_FreeScene(struct SCENEINFO *scene);
EVENTSINFO *LIME_LoadEvents(const char *filename, long a, long b);
int    IsWhirlwindScene(struct SCENEINFO *scene);
int    CountEventsMatching(SCENEEVENTTRACK *track, limeMATRIX44 *matrix);
/* **Pointers typed as pointers, not as `long`.**
 *
 * On 32-bit ARM a `long` and a pointer are both four bytes, so the original
 * passes scene, matrix and texture pointers through slots that disassemble as
 * plain words and nothing is lost. On a 64-bit host `long` is four bytes under
 * MinGW and eight under Linux while a pointer is always eight, so writing them
 * as `long` here TRUNCATES -- silently, and only on the arguments that happen to
 * be addresses.
 *
 * Same family as the EVENT stride: a width that is invisible in the binary
 * because everything is four bytes there. `long` stays where the mangled name
 * says the parameter really is one. */
int    LIME_TriggerEventFromSceneH(struct SCENEINFO *scene,
                                   SCENEEVENTTRACK *track,
                                   limeMATRIX44 *m1, limeMATRIX44 *m2,
                                   long a4, long offsetId2, long zeroOffset,
                                   long a7, TEXTURE *tex0, TEXTURE *tex1,
                                   long a10);
int    AddNewID(const char *name);
int    LIME_TriggerEvent(SCENEEVENTTRACK *track, limeMATRIX44 *m1,
                         limeMATRIX44 *m2, long a4, long offsetId2, long a7,
                         TEXTURE *tex0, TEXTURE *tex1, long a10);
void   LIME_TriggerEventsFromScene(struct SCENEINFO *scene, int frame,
                                   limeMATRIX44 *m, long a4, long offsetId2,
                                   long a7, TEXTURE *tex0, TEXTURE *tex1);
long   LIME_TriggerEventsFromSceneOffsetIfFollowing(long player, long printed,
                                   struct SCENEINFO *scene, long frame,
                                   limeMATRIX44 *m, limeMATRIX44 *mFollow,
                                   long a4, long a7, TEXTURE *tex0,
                                   TEXTURE *tex1, long a10);
/* Gamecode globals the event spawner reads (LIME_TriggerEventFromSceneH
 * 0xa4c44 and 0xa4d92, both through non-lazy slots). */
extern void  *LastGObj;                 /* 0x00150eb0 */
extern float  ShadowOffset;             /* 0x0014dfc8 */


/* GL ES 1.1 fixed function. Declared here rather than pulled from a GL header so
 * the decompiled engine builds standalone; runtime/ supplies the real ones. */
/* The GL ES 1.1 surface the renderers use. Every name here is a real import --
 * tools/stubs.py resolves all of them in the binary's stub table -- and the
 * enums are the standard GL values, which is the one place this project takes a
 * constant from a specification rather than from a literal pool. */
#ifndef GL_TEXTURE_2D
#define GL_TEXTURE_2D           0x0DE1
#define GL_TRIANGLES            0x0004
#define GL_FLOAT                0x1406
#define GL_UNSIGNED_BYTE        0x1401
#define GL_SHORT                0x1402
#define GL_UNSIGNED_SHORT       0x1403
#define GL_FLAT                 0x1D00
#define GL_VERTEX_ARRAY         0x8074
#define GL_NORMAL_ARRAY         0x8075
#define GL_COLOR_ARRAY          0x8076
#define GL_TEXTURE_COORD_ARRAY  0x8078
#define GL_TEXTURE0             0x84C0
#define GL_TEXTURE1             0x84C1
#define GL_TEXTURE_ENV          0x2300
#define GL_TEXTURE_ENV_MODE     0x2200
#define GL_MODULATE             0x2100
#define GL_REPLACE              0x1E01
#define GL_CULL_FACE            0x0B44
#define GL_ALPHA_TEST           0x0BC0
#define GL_GREATER              0x0204
#endif

/* The GL entry points the engine calls, spelled with plain C types because
 * the decompilation has no GL header to lean on. A build that links a real
 * GL gets them from <GL/gl.h> instead, where the same functions are
 * declared with GLenum and GLboolean -- and `void glDepthMask(int)` against
 * `void glDepthMask(GLboolean)` is a conflict, not a duplicate. */
#ifndef UMK3_REAL_GL
void LIME_GLAPI glEnable(unsigned cap);
void LIME_GLAPI glAlphaFunc(unsigned func, float ref);
void LIME_GLAPI glDisable(unsigned cap);
void LIME_GLAPI glEnableClientState(unsigned array);
void LIME_GLAPI glDisableClientState(unsigned array);
void glClientActiveTexture(unsigned unit);
void glActiveTexture(unsigned unit);
void LIME_GLAPI glBindTexture(unsigned target, unsigned name);
void LIME_GLAPI glTexEnvf(unsigned target, unsigned pname, float param);
void LIME_GLAPI glVertexPointer(int size, unsigned type, int stride, const void *p);
void LIME_GLAPI glTexCoordPointer(int size, unsigned type, int stride, const void *p);
void LIME_GLAPI glColorPointer(int size, unsigned type, int stride, const void *p);
void LIME_GLAPI glDrawElements(unsigned mode, int count, unsigned type, const void *idx);
void LIME_GLAPI glColor4f(float r, float g, float b, float a);
void LIME_GLAPI glDepthMask(unsigned char flag);   /* GLboolean, not int -- the real
                                           header says so and the two
                                           declarations have to agree */
void LIME_GLAPI glShadeModel(unsigned mode);

void LIME_GLAPI glPushMatrix(void);
void LIME_GLAPI glPopMatrix(void);
void LIME_GLAPI glLoadIdentity(void);
void LIME_GLAPI glMatrixMode(unsigned mode);
void LIME_GLAPI glTranslatef(float x, float y, float z);
void LIME_GLAPI glMultMatrixf(const float *m);
#else
/* A real GL supplies all of the above. Two exceptions: the multitexture calls
 * arrived in GL 1.3 and Windows' opengl32 exports 1.1, so runtime/draw_gl.c
 * provides them and they are declared here. */
#include "platform/gl.h"
void glClientActiveTexture(unsigned unit);
void glActiveTexture(unsigned unit);
#endif  /* !UMK3_REAL_GL */
void   limeDeleteTexture(TEXTURE *tex);

/* GetNextLine copies one line into `dst`, stopping at NUL, CR or LF, handles
 * CRLF, and returns the cursor for the next line. */
const char *GetNextLine(const char *src, char *dst);

#endif /* LIME_H */
