/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * smooth_models.c -- SMOOTH MODELS (umk3.ini smooth_models=0/1/2, the
 * options menu). Port, not in the binary.
 *
 * A fighter is drawn by LIME_RenderMeshSingleIndexed from the frame record
 * LoadAnimatedCharacter baked (indices, UVs, the frame's light) and the
 * positions DrawSkinnedMesh2 just skinned. With this on, the same skin goes
 * through Blender's Weld and Subdivision Surface modifiers first
 * (src/thirdparty/blender/umk3_subsurf.c) and the smoothed mesh is drawn
 * instead, with the same state calls.
 *
 *   everything  from what that draw uses: the record's triangles (its
 *               indices), its UVs and light per record vertex, and RenderVerts
 *               (int16, one per record vertex) just skinned -- so each corner
 *               keeps exactly the UV and light the original gives it
 *   light       carried through the subdivision as Blender carries loop data
 *
 * The weld is measured on the first pose a skin is drawn in, at Blender's
 * default distance (0.001 m: 0.1 of the game's centimetre units).
 *
 * The shadow is the body flattened (RenderAnimatedCharacter): it is drawn
 * from the plain skin, unsmoothed. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../decomp/lime/lime.h"
#include "../thirdparty/blender/umk3_subsurf.h"
#include "smooth_models.h"

int port_smooth_models;                 /* 0 off, else the level */

void CreateFadedRGBS(const uint8_t *src, uint8_t *dst, float level, long count,
                     limeVECTOR3 offset);
int  lime_port_edge_clamp(const float *uv, int stride,
                          const unsigned short *idx, int n);
void lime_port_edge_restore(int clamped);
int  rb_nodraw(void);
extern long limeRenderedPolyCount;
extern void (*lime_port_skin_freed)(const void *skin);   /* RenderSkinned.c */

#define SMOOTH_CACHE 8

extern limeVECTOR3 *RenderVerts;        /* int16 triples, as drawn */

typedef struct {
    const void     *skin;
    uint32_t        hash;               /* the record's indices and UVs */
    int             nverts, nfaces, levels;
    UmkSubsurf     *s;
    float          *pos;                /* nverts x 3, scratch */
    float          *attr;               /* nfaces x 3, scratch */
    uint8_t        *light, *rgba;       /* per output vertex */
    unsigned        used;
} smooth_entry;

static smooth_entry g_cache[SMOOTH_CACHE];
static unsigned g_tick;

static void entry_free(smooth_entry *e)
{
    umk_subsurf_free(e->s);
    free(e->pos);
    free(e->attr);
    free(e->light);
    free(e->rgba);
    memset(e, 0, sizeof *e);
}

void port_smooth_forget(const void *skin)
{
    int i;

    for (i = 0; i < SMOOTH_CACHE; i++)
        if (g_cache[i].skin == skin)
            entry_free(&g_cache[i]);
}

static void record_positions(int nverts, float *out)
{
    const int16_t *p = (const int16_t *)(const void *)RenderVerts;
    int i;

    for (i = 0; i < nverts * 3; i++)
        out[i] = (float)p[i];
}

/* FNV-1a over the record's indices and UVs: frames that share a mesh share
 * an entry, whichever arrays hold it */
static uint32_t record_hash(const uint16_t *idx, int nfaces, const float *uv, int nverts)
{
    const uint8_t *b;
    uint32_t h = 2166136261u;
    size_t i, n;

    b = (const uint8_t *)idx;
    n = (size_t)nfaces * 6;
    for (i = 0; i < n; i++)
        h = (h ^ b[i]) * 16777619u;
    b = (const uint8_t *)uv;
    n = (size_t)nverts * 8;
    for (i = 0; i < n; i++)
        h = (h ^ b[i]) * 16777619u;
    return h;
}

static smooth_entry *lookup(const void *skin, int nverts, int nfaces,
                            const uint16_t *idx, const float *ruv)
{
    const uint32_t hash = record_hash(idx, nfaces, ruv, nverts);
    smooth_entry *e = NULL;
    float *cu;
    int i, oldest = 0;

    for (i = 0; i < SMOOTH_CACHE; i++) {
        smooth_entry *c = &g_cache[i];
        if (c->skin == skin && c->hash == hash && c->nverts == nverts && c->nfaces == nfaces) {
            if (c->levels == port_smooth_models) {
                c->used = ++g_tick;
                return c->s ? c : NULL;
            }
            entry_free(c);              /* the level changed */
            e = c;
            break;
        }
        if (g_cache[i].used < g_cache[oldest].used)
            oldest = i;
    }
    if (e == NULL) {
        e = &g_cache[oldest];
        if (e->skin)
            entry_free(e);
    }

    e->skin = skin;
    e->hash = hash;
    e->nverts = nverts;
    e->nfaces = nfaces;
    e->levels = port_smooth_models;
    e->used = ++g_tick;
    e->pos = malloc(sizeof(float) * 3 * (size_t)nverts);
    e->attr = malloc(sizeof(float) * 3 * (size_t)nfaces);
    cu = malloc(sizeof(float) * 6 * (size_t)nfaces);
    if (!e->pos || !e->attr || !cu) {
        free(cu);
        return NULL;
    }

    record_positions(nverts, e->pos);
    for (i = 0; i < nfaces * 3; i++) {
        cu[i * 2 + 0] = ruv[idx[i] * 2 + 0];
        cu[i * 2 + 1] = ruv[idx[i] * 2 + 1];
    }
    e->s = umk_subsurf_new(e->levels, UMK_WELD_DISTANCE_CM * VertScale,
                           nverts, e->pos, nfaces, idx, cu);
    free(cu);
    if (e->s) {
        int n = umk_subsurf_num_verts(e->s);
        e->light = malloc((size_t)n);
        e->rgba = malloc((size_t)n * 4);
        printf("smooth models: mesh %d verts %d tris -> level %d, %d welded, %d verts %d tris\n",
               nverts, nfaces, e->levels, umk_subsurf_welded(e->s), n,
               umk_subsurf_num_tris(e->s));
        if (!e->light || !e->rgba) {
            umk_subsurf_free(e->s);
            e->s = NULL;
        }
    } else
        printf("smooth models: mesh %d verts %d tris not smoothed\n", nverts, nfaces);
    return e->s ? e : NULL;
}

/* LIME_RenderMeshSingleIndexed's state calls, around the smoothed arrays. */
static void draw(smooth_entry *e, TEXTURE *tex, float alpha, const limeVECTOR3 *fade)
{
    const UmkSubsurf *s = e->s;
    const int nverts = umk_subsurf_num_verts(s), ntris = umk_subsurf_num_tris(s);
    const float *uvs = umk_subsurf_uv(s);
    float sc;

    glShadeModel(0x1d01);               /* GL_SMOOTH */
    limeRenderedPolyCount += ntris;

    glClientActiveTexture(GL_TEXTURE0);
    glActiveTexture(GL_TEXTURE0);
    glDisableClientState(GL_NORMAL_ARRAY);
    glEnableClientState(GL_VERTEX_ARRAY);

    sc = 1.0f / VertScale;
    glScalef(sc, sc, sc);

    if (tex == NULL) {
        glBindTexture(GL_TEXTURE_2D, 0);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glDisableClientState(GL_COLOR_ARRAY);
        glVertexPointer(3, GL_FLOAT, 0, umk_subsurf_pos(s));
        glColor4f(0.0f, 0.0f, 0.0f, 1.0f);
        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, (float)GL_REPLACE);
        glDisable(GL_TEXTURE_2D);
    } else {
        limeVECTOR3 f;

        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glTexCoordPointer(2, GL_FLOAT, 0, uvs);
        glBindTexture(GL_TEXTURE_2D, tex->name);
        glEnableClientState(GL_COLOR_ARRAY);

        f.x = fade->x * 255.0f;
        f.y = fade->y * 255.0f;
        f.z = fade->z * 255.0f;
        CreateFadedRGBS(e->light, e->rgba, alpha, nverts, f);
        glColorPointer(4, GL_UNSIGNED_BYTE, 0, e->rgba);

        glVertexPointer(3, GL_FLOAT, 0, umk_subsurf_pos(s));
        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, (float)GL_MODULATE);
        glEnable(GL_TEXTURE_2D);
    }

    {
        const int clamp = tex ? lime_port_edge_clamp(uvs, 2, NULL, nverts) : 0;
        if (!rb_nodraw())
            glDrawElements(GL_TRIANGLES, ntris * 3, GL_UNSIGNED_INT, umk_subsurf_tris(s));
        lime_port_edge_restore(clamp);
    }

    glClientActiveTexture(GL_TEXTURE1);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glActiveTexture(0x1702);            /* as LIME_RenderMeshSingleIndexed */
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

int port_smooth_draw(const void *skin, const void *rec, long second,
                     void *tex_p, float alpha, const void *fade_p)
{
    TEXTURE *tex = (TEXTURE *)tex_p;
    const limeVECTOR3 *fade = (const limeVECTOR3 *)fade_p;
    const char *r = (const char *)rec;
    const uint16_t *idx;
    const float *ruv;
    const uint8_t *light;
    smooth_entry *e;
    long nverts, nfaces;
    int i, n;

    if (!port_smooth_models || skin == NULL || rec == NULL || second == 2)
        return 0;                       /* the shadow is not smoothed */
    lime_port_skin_freed = port_smooth_forget;

    /* LIME_RenderMeshSingleIndexed's own reads */
    nverts = *(const long *)(r + (second ? 0x08 : 0x00));
    nfaces = *(const long *)(r + (second ? 0x0c : 0x04));
    idx    = *(const uint16_t *P32 const *)(r + (second ? 0x2c : 0x1c));
    ruv    = *(const float *P32 const *)(r + (second ? 0x30 : 0x20));
    light  = *(const uint8_t *P32 const *)(r + (second ? 0x34 : 0x24));
    if (idx == NULL || ruv == NULL || light == NULL || nverts <= 0 || nfaces <= 0)
        return 0;
    for (i = 0; i < nfaces * 3; i++)
        if (idx[i] >= nverts)
            return 0;                   /* not a mesh this can read */

    e = lookup(skin, (int)nverts, (int)nfaces, idx, ruv);
    if (e == NULL)
        return 0;

    record_positions(e->nverts, e->pos);
    for (i = 0; i < e->nfaces * 3; i++)
        e->attr[i] = (float)light[idx[i]];
    if (!umk_subsurf_eval(e->s, e->pos, e->attr))
        return 0;

    n = umk_subsurf_num_verts(e->s);
    {
        const float *a = umk_subsurf_attr(e->s);
        for (i = 0; i < n; i++) {
            float v = a[i] + 0.5f;
            e->light[i] = (uint8_t)(v <= 0.0f ? 0 : v >= 255.0f ? 255 : (int)v);
        }
    }
    draw(e, tex, alpha, fade);
    return 1;
}
