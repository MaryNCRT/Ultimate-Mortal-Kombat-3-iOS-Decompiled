/*
 * .meshset loader.
 *
 * Format from docs/MESHSET-FORMAT.md, derived from LIME_LoadMeshSet
 * (armv7 0x0005ea34) and validated against 604 of 605 shipped files with the
 * walk landing on the exact final offset.
 */
#ifndef LIME_MESHSET_H
#define LIME_MESHSET_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    float x, y, z;
    float u, v;
} LimeVertex;

typedef struct {
    char        name[64];
    char        texture[64];
    int32_t     num_verts;
    int32_t     num_faces;
    float       radius;
    char        variant;        /* 'A', 'B' or 'C' -- see the format doc */
    uint16_t   *indices;        /* num_faces * 3, or NULL for variant C */
    LimeVertex *verts;          /* num_verts, or num_faces*3 for variant C */
    int32_t     vert_count;     /* what verts actually holds */
} LimeMesh;

typedef struct {
    int32_t   num_meshes;
    LimeMesh *meshes;
} LimeMeshSet;

/* Loads and takes ownership of nothing -- the file is read and closed. */
bool lime_meshset_load(const char *path, LimeMeshSet *out);
void lime_meshset_free(LimeMeshSet *ms);

#endif
