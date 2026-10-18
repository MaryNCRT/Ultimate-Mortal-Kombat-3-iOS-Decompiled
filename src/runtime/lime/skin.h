/*
 * skin.h — skeleton, skin and animation, for the native runtime.
 *
 * The formats are specified in docs/SKIN-FORMAT.md, derived from
 * LIME_LoadBones, LIME_LoadSkin1, UnpackAnimFrame and DrawSkinnedMesh2. The
 * maths this file performs is the same maths decomp/lime/RenderSkinned.c
 * carries, and that file is verified against the recompiled original over
 * 18,780 cases -- MatrixMul2, GetMFromQuat2, GetSlerpedQ and Xform2 all
 * exactly.
 *
 * What is NOT verified is the skeleton walk. `CreateMatrixPaletteRecurse2` is
 * marked *structurally complete* in that file: its shape is read from the
 * disassembly and no differential test drives it, because it is stateful
 * across four globals rather than a function of its arguments. The walk here
 * follows the same reading. It produces a character that stands up and moves,
 * which is evidence and not proof.
 */
#ifndef LIME_SKIN_H
#define LIME_SKIN_H

#include <stdbool.h>
#include <stdint.h>

#define LIME_MAX_BONES 128

/* ---------------------------------------------------------------- .bones */

typedef struct {
    float   offset[3];              /* from the parent */
    int8_t  child[9];               /* -1 marks an unused slot */
    int32_t num_children;
} LimeBone;

typedef struct {
    LimeBone *bones;
    int32_t   count;
    int32_t   root;                 /* the one bone nobody claims as a child */
} LimeSkeleton;

bool lime_bones_load(const char *path, LimeSkeleton *out);
void lime_bones_free(LimeSkeleton *s);

/* ----------------------------------------------------------------- .skin */

typedef struct {
    int32_t   num_matrices;         /* skinned VERTICES, despite the name */
    int32_t   num_verts;            /* TRIANGLES, despite the name */

    uint32_t *indexes;              /* N: four packed bone bytes, 0xFF unused */
    float    *weights;              /* N*4 */
    float    *a;                    /* N*12: four vec3, position, pre-weighted */
    float    *b;                    /* N*12: four vec3, normal, pre-weighted */
    uint16_t *tri;                  /* M*3: indices into the skinned vertices */
    float    *uv;                   /* M*6: three UV pairs, one per corner */
} LimeSkinBlock;

typedef struct {
    LimeSkinBlock block[2];
    int           count;
} LimeSkin;

bool lime_skin_load(const char *path, LimeSkin *out);
void lime_skin_free(LimeSkin *s);

/* ------------------------------------------------------------- .skinanim */

typedef struct {
    uint8_t *data;
    int32_t  num_frames;
    int32_t  frame_size;
    int32_t  num_bones;             /* (frame_size - 16) / 20 */
    int32_t  header;                /* 12, or 16 for the odd file */
} LimeAnim;

bool lime_anim_load(const char *path, LimeAnim *out);
void lime_anim_free(LimeAnim *a);

/* --------------------------------------------------------------- posing */

/* A palette entry: a 3x3 rotation at stride 3 and a translation, which is the
 * SKINMATRIX43 layout lime.h documents -- nine floats then three. */
typedef struct {
    float m[9];
    float t[3];
} LimePalette;

/* Build the palette for one interpolated moment between two animation frames.
 * `t` in 0..1. Returns false if the frames are out of range. */
bool lime_pose(const LimeSkeleton *sk, const LimeAnim *anim,
               int frame_a, int frame_b, float t,
               LimePalette *palette /* [sk->count] */);

/* Skin one block's vertices into `pos` (N*3) and `light` (N greys, 0..255). */
void lime_skin_apply(const LimeSkinBlock *blk, const LimePalette *palette,
                     int num_bones, float *pos, unsigned char *light);

#endif
