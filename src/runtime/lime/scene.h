/*
 * scene.h -- the `.scene` scene graph, for the native runtime.
 *
 * The format is specified in docs/SCENE-FORMAT.md, derived from
 * LIME_LoadScene (0x0005f0ac). This file carries the part of that loader the
 * document previously left open: the 40-byte tail record, which is the
 * placement of every object in the stage.
 *
 * A stage `.meshset` holds its objects in LOCAL space. Drawing them all at the
 * origin -- which an earlier demo did -- piles a graveyard into one heap. The
 * `.scene` is what puts them where they belong.
 */
#ifndef LIME_SCENE_H
#define LIME_SCENE_H

#include <stdbool.h>
#include <stdint.h>

/* The in-memory matrix the loader builds, 32 bytes: the QSTMATRIX of
 * decomp/lime/lime.h, which ConvertQSTMatrixtoPCMatrix consumes. */
typedef struct {
    int16_t q[4];                   /* +0x00  x, y, z, w -- w LAST */
    float   scale[3];               /* +0x08 */
    float   translation[3];         /* +0x14 */
} LimeQST;

/* 8 bytes. Built per visible track record, not read from the file. */
typedef struct {
    float    alpha;                 /* +0x00  the track value */
    uint8_t  mesh_index;            /* +0x04  LIME_FindMeshByName, 0xFF none */
    uint8_t  field5;                /* +0x05  track +0x04, narrowed to a byte */
    uint16_t palette_index;         /* +0x06  track +0x08 */
} LimeSceneKey;

#define LIME_SCENE_HIDDEN 0xFFFFu

typedef struct {
    char           name[64];
    LimeSceneKey  *keys;
    uint16_t      *stream;          /* one per frame; LIME_SCENE_HIDDEN = off */
    int32_t        num_keys;
} LimeSceneNode;

typedef struct {
    LimeSceneNode *nodes;
    int32_t        num_nodes;       /* numObjects */
    int32_t        num_frames;      /* count2 -- the stream modulus */
    LimeQST       *palette;
    int32_t        palette_size;    /* count3 */
    float          scale;           /* SCENEINFO+0x60; 1.0 without `.offsets` */
} LimeScene;

/* Stands in for LIME_FindMeshByName: return the mesh index for `name`, or -1.
 * Pass NULL to leave every mesh_index at 0xFF. */
typedef int (*LimeFindMeshFn)(const char *name, void *user);

bool lime_scene_load(const char *path, LimeScene *out,
                     LimeFindMeshFn find, void *user);
void lime_scene_free(LimeScene *s);

/* The QSTMATRIX -> column-major GL matrix conversion that
 * ConvertQSTMatrixtoPCMatrix performs (decomp/lime/LIMEDS_Misc.c), which is
 * verified against the original at bit precision. Transcribed here, constant
 * and association included, so the runtime need not pull in the decompiled
 * tree; tests/test_scene_qst.c holds the two side by side. */
void lime_qst_matrix(const LimeQST *src, float *dst /* [16] */);

/* The key active on `frame`, or NULL when the node is hidden. */
const LimeSceneKey *lime_scene_key(const LimeScene *s, int node, int frame);

#endif
