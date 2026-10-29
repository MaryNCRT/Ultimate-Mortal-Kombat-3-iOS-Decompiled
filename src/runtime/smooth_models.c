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
 * from the plain skin, unsmoothed.
 *
 * With it off, the plain skin is tucked under what its bones carry (Kung
 * Lao's hat, port_smooth_tuck): the original model's head comes through his
 * hat in 4 points. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../decomp/lime/lime.h"
#include "../thirdparty/blender/umk3_subsurf.h"
#include "smooth_models.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <pthread.h>
#endif

int port_smooth_models;                 /* 0 off, else the level */
/* SHARP EDGES (umk3_subsurf.h umk_subsurf_set_sharp): umk3.ini
 * sharp_edge_angle= (degrees) and sharp_edge_strength= (percent), the
 * launcher's two sliders, read at start (game_main.c); compared for every
 * mesh drawn, so a mesh made before a change is brought to it */
float port_sharp_angle = UMK_SHARP_ANGLE_DEFAULT;
float port_sharp_strength = UMK_SHARP_STRENGTH_DEFAULT;

void CreateFadedRGBS(const uint8_t *src, uint8_t *dst, float level, long count,
                     limeVECTOR3 offset);
int  lime_port_edge_clamp(const float *uv, int stride,
                          const unsigned short *idx, int n);
void lime_port_edge_restore(int clamped);
int  rb_nodraw(void);
int  rgl_shadow_active(void);           /* render/rgl.h, REAL-TIME SHADOWS */
void rgl_shadow_proxy(int mode);
extern long limeRenderedPolyCount;
static long g_frame_tris;               /* smoothed triangles this frame */

void port_smooth_frame(void)
{
    g_frame_tris = 0;
}
extern void (*lime_port_skin_freed)(const void *skin);   /* RenderSkinned.c */

#define SMOOTH_CACHE 32
#define SMOOTH_POSES 8                  /* subdivided poses kept a mesh */
#define SMOOTH_FRAME_TRIS 250000        /* a frame's smoothed triangles: a fighter at level 2 is ~60,000, so the two fighters and a third body; past it, copies are drawn as the model is */

extern limeVECTOR3 *RenderVerts;        /* int16 triples, as drawn */

/* What covers each skin (port_smooth_cover_mesh): a box per piece, in the
 * skin's own units (RenderVerts), kept a quarter of a second -- the scene is
 * drawn after the skin, so each is posed again with this frame's bones
 * before the skin is drawn (cover_repose). */
#define COVER_MAX 16
#define COVER_MARGIN_CM 2.0f
#define COVER_VERTS 1024                /* a bigger piece keeps only its box */
#define TUCK_DEPTH_CM 3.0f              /* deeper is not the skin poking out */
#define TUCK_INSIDE_CM 0.3f
static struct cover {
    const void *skin, *mesh;
    float lo[3], hi[3];
    double at;
    float key[16], scale;               /* the piece's key matrix and bone: posed */
    int bone;                           /* again with the skin's own bones */
    int nv;                             /* the piece's vertices below, or 0 */
    float v[COVER_VERTS * 3], c[3];     /* in the skin's units, their centre */
} g_cover[COVER_MAX];
static const void *g_cover_owner;
double plat_time(void);

typedef struct {
    const void     *skin;
    uint32_t        hash;               /* the record's indices and UVs */
    int             nverts, nfaces, levels;
    UmkSubsurf     *s;
    float           sharp_a, sharp_s;   /* what umk_subsurf_set_sharp has */
    float          *fitw;               /* nverts, umk_subsurf_set_fit's */
    int             covered;            /* vertices under a piece, logged once */
    int             tucked;             /* tucked under it, logged once */
    float          *pos;                /* nverts x 3, scratch */
    float          *attr;               /* nfaces x 3, scratch */
    uint8_t        *light, *rgba;       /* per output vertex */
    unsigned        used;
    /* the build, on a thread of its own (a fighter's takes ~200 ms: the
     * character select froze as one came in); the plain skin is drawn
     * until it is done */
    void           *job;                /* its thread, NULL none */
    int             job_over;           /* Linux: the thread has finished */
    struct {                            /* pose_restore_or_eval's */
        uint32_t    h;
        unsigned    used;
        float      *out;                /* the subdivided positions */
    }               pose[SMOOTH_POSES];
    UmkSubsurf     *built;              /* what it made */
    float           weld;
    uint16_t       *jidx;               /* its own copies of the record */
    float          *jpos, *jcu;
} smooth_entry;

static smooth_entry g_cache[SMOOTH_CACHE];
static unsigned g_tick;

static void build(smooth_entry *e)
{
    e->built = umk_subsurf_new(e->levels, e->weld, e->nverts, e->jpos, e->nfaces, e->jidx, e->jcu);
}

#ifdef _WIN32
static DWORD WINAPI build_job(LPVOID p)
{
    build((smooth_entry *)p);
    return 0;
}
#else
/* Linux: a thread too (it was built here and now, the game held ~200 ms) */
static void *build_job(void *p)
{
    build((smooth_entry *)p);
    __sync_synchronize();
    ((smooth_entry *)p)->job_over = 1;
    return NULL;
}
#endif

/* the build started (1), or done here and now (0) */
static int build_start(smooth_entry *e)
{
#ifdef _WIN32
    HANDLE t = CreateThread(NULL, 1 << 20, build_job, e, 0, NULL);
    if (t) {
        e->job = t;
        return 1;
    }
#else
    pthread_t *t = (pthread_t *)malloc(sizeof *t);
    e->job_over = 0;
    if (t && pthread_create(t, NULL, build_job, e) == 0) {
        e->job = t;
        return 1;
    }
    free(t);
#endif
    build(e);
    return 0;
}

/* 1 when the build is over (or there is none), waiting for it if `wait` */
static int build_done(smooth_entry *e, int wait)
{
#ifdef _WIN32
    if (e->job) {
        if (WaitForSingleObject((HANDLE)e->job, wait ? INFINITE : 0) != WAIT_OBJECT_0)
            return 0;
        CloseHandle((HANDLE)e->job);
        e->job = NULL;
    }
#else
    if (e->job) {
        if (!wait && !*(volatile int *)&e->job_over)
            return 0;
        pthread_join(*(pthread_t *)e->job, NULL);
        free(e->job);
        e->job = NULL;
    }
#endif
    return 1;
}

static void entry_free(smooth_entry *e)
{
    int i;

    build_done(e, 1);
    umk_subsurf_free(e->built);
    free(e->jidx);
    free(e->jpos);
    free(e->jcu);
    free(e->fitw);
    umk_subsurf_free(e->s);
    free(e->pos);
    free(e->attr);
    free(e->light);
    free(e->rgba);
    for (i = 0; i < SMOOTH_POSES; i++)
        free(e->pose[i].out);
    memset(e, 0, sizeof *e);
}

void port_smooth_cover_owner(const void *skin)
{
    g_cover_owner = skin;
}

/* the piece where its bone holds it: the key matrix onto the bone's (as
 * RenderScene's scene_attach_to_bone), its box and vertices in the skin's
 * units. Posed again with the skin's bones before the skin is drawn
 * (cover_repose), so a moving fighter's head stays under it this frame,
 * not the one before (the scene is drawn after the skin) */
static void cover_pose(struct cover *c, const void *bones)
{
    const MESHINFO *mesh = (const MESHINFO *)c->mesh;
    const float *b = (const float *)((const char *)bones + c->bone * 48);
    float t[16] = { 0 }, m[16], k;
    int i, j;

    t[0] = b[0];  t[1] = b[1];  t[2] = b[2];
    t[4] = b[3];  t[5] = b[4];  t[6] = b[5];
    t[8] = b[6];  t[9] = b[7];  t[10] = b[8];
    t[12] = b[9]; t[13] = b[10]; t[14] = b[11]; t[15] = 1.0f;
    limeMatrixMult(c->key, t, m);
    /* the piece's int16 positions over its radius (LIME_RenderMeshSingle's
     * glScalef), through the key and bone matrix, the scene's scale on all
     * of it (LIME_RenderScene's glScalef), into the skin's units */
    k = c->scale * VertScale;
    for (j = 0; j < 3; j++) {
        c->lo[j] = 1e30f;
        c->hi[j] = -1e30f;
        c->c[j] = 0.0f;
    }
    c->nv = mesh->numVerts <= COVER_VERTS && mesh->indices ? mesh->numVerts : 0;
    for (i = 0; i < mesh->numVerts; i++) {
        const float x = mesh->verts[i].x / mesh->boundsRadius;
        const float y = mesh->verts[i].y / mesh->boundsRadius;
        const float z = mesh->verts[i].z / mesh->boundsRadius;
        for (j = 0; j < 3; j++) {
            float v = (m[j] * x + m[4 + j] * y + m[8 + j] * z + m[12 + j]) * k;
            if (v < c->lo[j]) c->lo[j] = v;
            if (v > c->hi[j]) c->hi[j] = v;
            if (c->nv) {
                c->v[i * 3 + j] = v;
                c->c[j] += v / (float)c->nv;
            }
        }
    }
    for (j = 0; j < 3; j++) {
        c->lo[j] -= COVER_MARGIN_CM * VertScale;
        c->hi[j] += COVER_MARGIN_CM * VertScale;
    }
}

void port_smooth_cover_mesh(const void *mesh_p, const float *key, int bone, const void *bones,
                            float scene_scale)
{
    const MESHINFO *mesh = (const MESHINFO *)mesh_p;
    const double now = plat_time();
    struct cover *c = NULL;
    int i;

    if (!g_cover_owner || !bones || bone < 0 || bone >= 150 || !mesh || mesh->numVerts <= 0
        || mesh->boundsRadius == 0.0f)
        return;
    for (i = 0; i < COVER_MAX && !c; i++)
        if (g_cover[i].skin == g_cover_owner && g_cover[i].mesh == mesh)
            c = &g_cover[i];
    for (i = 0; i < COVER_MAX && !c; i++)
        if (!g_cover[i].skin || now - g_cover[i].at > 0.25)
            c = &g_cover[i];
    if (!c)
        return;
    c->skin = g_cover_owner;
    c->mesh = mesh;
    c->at = now;
    memcpy(c->key, key, sizeof c->key);
    c->bone = bone;
    c->scale = scene_scale;
    cover_pose(c, bones);
}

/* the bones the skin was just posed with (lime_globals.c) */

/* this skin's pieces posed with its bones of this frame */
static void cover_repose(const void *skin)
{
    const double now = plat_time();
    int k;

    for (k = 0; k < COVER_MAX; k++)
        if (g_cover[k].skin == skin && now - g_cover[k].at <= 0.25)
            cover_pose(&g_cover[k], (const void *)MatrixPalette2);
}

/* The correction's share for this frame: none for a vertex inside a piece
 * that covers the skin, all of it elsewhere (and NULL when nothing covers
 * it, the surface as made). */
static void cover_fit(smooth_entry *e)
{
    const double now = plat_time();
    int i, k, any = 0, n = 0;

    for (k = 0; k < COVER_MAX; k++)
        if (g_cover[k].skin == e->skin && now - g_cover[k].at <= 0.25)
            any = 1;
    if (!any) {
        umk_subsurf_set_fit(e->s, NULL);
        return;
    }
    if (!e->fitw && !(e->fitw = malloc(sizeof(float) * (size_t)e->nverts)))
        return;
    for (i = 0; i < e->nverts; i++) {
        const float *p = e->pos + i * 3;
        e->fitw[i] = 1.0f;
        for (k = 0; k < COVER_MAX; k++) {
            const struct cover *c = &g_cover[k];
            if (c->skin != e->skin || now - c->at > 0.25)
                continue;
            if (p[0] >= c->lo[0] && p[0] <= c->hi[0] && p[1] >= c->lo[1] && p[1] <= c->hi[1]
                && p[2] >= c->lo[2] && p[2] <= c->hi[2]) {
                e->fitw[i] = 0.0f;
                n++;
                break;
            }
        }
    }
    if (n && !e->covered)
        printf("smooth models: %d vertices of a skin under a piece on its bones, no correction there\n", n);
    e->covered = n;
    umk_subsurf_set_fit(e->s, e->fitw);
}

/* Where the ray from the piece's centre to p last leaves the piece, as a
 * share of the way (Moller-Trumbore on each triangle), or -1 when it does
 * not: p is then inside it or beside it. */
static float cover_leaves(const struct cover *c, const MESHINFO *mesh, const float *p)
{
    const float d[3] = { p[0] - c->c[0], p[1] - c->c[1], p[2] - c->c[2] };
    float best = -1.0f;
    int f;

    for (f = 0; f < mesh->numFaces; f++) {
        const uint16_t *t = mesh->indices + f * 3;
        const float *a, *b, *e;
        float e1[3], e2[3], h[3], s[3], q[3], det, u, v, w;

        if (t[0] >= c->nv || t[1] >= c->nv || t[2] >= c->nv)
            continue;
        a = c->v + t[0] * 3;
        b = c->v + t[1] * 3;
        e = c->v + t[2] * 3;
        e1[0] = b[0] - a[0]; e1[1] = b[1] - a[1]; e1[2] = b[2] - a[2];
        e2[0] = e[0] - a[0]; e2[1] = e[1] - a[1]; e2[2] = e[2] - a[2];
        h[0] = d[1] * e2[2] - d[2] * e2[1];
        h[1] = d[2] * e2[0] - d[0] * e2[2];
        h[2] = d[0] * e2[1] - d[1] * e2[0];
        det = e1[0] * h[0] + e1[1] * h[1] + e1[2] * h[2];
        if (det > -1e-9f && det < 1e-9f)
            continue;
        s[0] = c->c[0] - a[0]; s[1] = c->c[1] - a[1]; s[2] = c->c[2] - a[2];
        u = (s[0] * h[0] + s[1] * h[1] + s[2] * h[2]) / det;
        if (u < 0.0f || u > 1.0f)
            continue;
        q[0] = s[1] * e1[2] - s[2] * e1[1];
        q[1] = s[2] * e1[0] - s[0] * e1[2];
        q[2] = s[0] * e1[1] - s[1] * e1[0];
        v = (d[0] * q[0] + d[1] * q[1] + d[2] * q[2]) / det;
        if (v < 0.0f || u + v > 1.0f)
            continue;
        w = (e2[0] * q[0] + e2[1] * q[1] + e2[2] * q[2]) / det;
        if (w > 0.0f && w < 1.0f && w > best)
            best = w;
    }
    return best;
}

/* The plain skin under a piece on its bones: a vertex in the piece's box
 * past its surface, seen from its centre, by less than TUCK_DEPTH_CM, goes
 * back TUCK_INSIDE_CM inside it, in RenderVerts before the original draw.
 * Measured on Kung Lao in the rest pose (WORK tuck.mjs): the 4 points
 * through the hat, at most 0.24 cm past it along that line, and none else. */
/* in the skin's units, float positions (the smoothed draw's) or the
 * int16 RenderVerts (the plain one), whichever is given */
static int tuck_at(const void *skin, int nverts, float *fp, int16_t *rv, float inside_cm,
                   float depth_cm)
{
    const double now = plat_time();
    int i, k, n = 0;

    for (k = 0; k < COVER_MAX; k++) {
        const struct cover *c = &g_cover[k];
        const MESHINFO *mesh = (const MESHINFO *)c->mesh;

        if (c->skin != skin || now - c->at > 0.25 || !c->nv)
            continue;
        for (i = 0; i < nverts; i++) {
            float p[3], d[3], len, w, back;
            int j;

            for (j = 0; j < 3; j++)
                p[j] = fp ? fp[i * 3 + j] : rv[i * 3 + j];
            if (p[0] < c->lo[0] || p[0] > c->hi[0] || p[1] < c->lo[1] || p[1] > c->hi[1]
                || p[2] < c->lo[2] || p[2] > c->hi[2])
                continue;
            if ((w = cover_leaves(c, mesh, p)) < 0.0f)
                continue;
            for (j = 0; j < 3; j++)
                d[j] = p[j] - c->c[j];
            len = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
            if (len <= 0.0f || (1.0f - w) * len > depth_cm * VertScale)
                continue;
            back = w - inside_cm * VertScale / len;
            if (back < 0.0f)
                back = 0.0f;
            for (j = 0; j < 3; j++) {
                float v = c->c[j] + d[j] * back;
                if (fp)
                    fp[i * 3 + j] = v;
                else
                    rv[i * 3 + j] = (int16_t)(v < 0.0f ? v - 0.5f : v + 0.5f);
            }
            n++;
        }
    }
    return n;
}

static void tuck(const void *skin, int nverts)
{
    static int logged;
    const int n = tuck_at(skin, nverts, NULL, (int16_t *)(void *)RenderVerts, TUCK_INSIDE_CM,
                          TUCK_DEPTH_CM);

    if (n && !logged) {
        printf("smooth models: %d vertices of a skin tucked under a piece on its bones\n", n);
        logged = 1;
    }
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

/* the build taken in: sharp edges set, the buffers made, logged */
static void adopt(smooth_entry *e)
{
    e->s = e->built;
    e->built = NULL;
    free(e->jidx);
    free(e->jpos);
    free(e->jcu);
    e->jidx = NULL;
    e->jpos = e->jcu = NULL;
    if (e->s) {
        int n;

        umk_subsurf_set_sharp(e->s, port_sharp_angle, port_sharp_strength);
        e->sharp_a = port_sharp_angle;
        e->sharp_s = port_sharp_strength;
        n = umk_subsurf_num_verts(e->s);
        e->light = malloc((size_t)n);
        e->rgba = malloc((size_t)n * 4);
        printf("smooth models: mesh %d verts %d tris -> level %d, %d welded, %d verts %d tris, "
               "%d sharp edges (%.0f deg, %.0f%%)\n",
               e->nverts, e->nfaces, e->levels, umk_subsurf_welded(e->s), n,
               umk_subsurf_num_tris(e->s), umk_subsurf_sharp_edges(e->s),
               port_sharp_angle, port_sharp_strength * 100.0f);
        if (!e->light || !e->rgba) {
            umk_subsurf_free(e->s);
            e->s = NULL;
        }
    } else
        printf("smooth models: mesh %d verts %d tris not smoothed\n", e->nverts, e->nfaces);
}

static smooth_entry *start_entry(smooth_entry *e, const void *skin, uint32_t hash,
                                 int nverts, int nfaces, const uint16_t *idx,
                                 const float *ruv)
{
    int i;

    e->skin = skin;
    e->hash = hash;
    e->nverts = nverts;
    e->nfaces = nfaces;
    e->levels = port_smooth_models;
    e->used = ++g_tick;
    e->weld = UMK_WELD_DISTANCE_CM * VertScale;
    e->pos = malloc(sizeof(float) * 3 * (size_t)nverts);
    e->attr = malloc(sizeof(float) * 3 * (size_t)nfaces);
    e->jpos = malloc(sizeof(float) * 3 * (size_t)nverts);
    e->jidx = malloc(sizeof(uint16_t) * 3 * (size_t)nfaces);
    e->jcu = malloc(sizeof(float) * 6 * (size_t)nfaces);
    if (!e->pos || !e->attr || !e->jpos || !e->jidx || !e->jcu)
        return NULL;

    record_positions(nverts, e->jpos);
    memcpy(e->jidx, idx, sizeof(uint16_t) * 3 * (size_t)nfaces);
    for (i = 0; i < nfaces * 3; i++) {
        e->jcu[i * 2 + 0] = ruv[idx[i] * 2 + 0];
        e->jcu[i * 2 + 1] = ruv[idx[i] * 2 + 1];
    }
    if (build_start(e))
        return NULL;                    /* the plain skin until it is done */
    adopt(e);
    return e->s ? e : NULL;
}

static smooth_entry *lookup(const void *skin, int nverts, int nfaces,
                            const uint16_t *idx, const float *ruv)
{
    const uint32_t hash = record_hash(idx, nfaces, ruv, nverts);
    smooth_entry *e = NULL;
    int i, oldest = 0;

    for (i = 0; i < SMOOTH_CACHE; i++) {
        smooth_entry *c = &g_cache[i];
        if (c->skin == skin && c->hash == hash && c->nverts == nverts && c->nfaces == nfaces) {
            if (c->levels == port_smooth_models) {
                c->used = ++g_tick;
                if (c->job) {
                    if (!build_done(c, 0))
                        return NULL;    /* still building: the plain skin */
                    adopt(c);
                }
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
    return start_entry(e, skin, hash, nverts, nfaces, idx, ruv);
}

/* One mesh is often drawn several times a frame in poses of the frames
 * before (the trail of copies in Scorpion's fatality): each pose seen is
 * kept, its subdivided positions, so seeing it again costs a copy and the
 * light spread again instead of subdividing it twice over. `h` is a hash
 * of the positions handed to the subdivision. */
static uint32_t pose_hash(const float *p, int n)
{
    const uint32_t *w = (const uint32_t *)(const void *)p;
    uint32_t h = 2166136261u;
    int i;

    for (i = 0; i < n * 3; i++)
        h = (h ^ w[i]) * 16777619u;
    return h;
}

static int pose_restore_or_eval(smooth_entry *e)
{
    const int n = umk_subsurf_num_verts(e->s);
    const uint32_t h = pose_hash(e->pos, e->nverts);
    int i, slot = -1;

    for (i = 0; i < SMOOTH_POSES; i++)
        if (e->pose[i].out && e->pose[i].h == h) {
            slot = i;
            break;
        }
    if (slot >= 0 && !umk_subsurf_is_last(e->s, e->pos))
        umk_subsurf_restore(e->s, e->pos, e->pose[slot].out);
    if (!umk_subsurf_eval(e->s, e->pos, e->attr))
        return 0;
    if (slot >= 0) {
        e->pose[slot].used = ++g_tick;
        return 1;
    }
    /* keep it, in the slot seen longest ago */
    slot = 0;
    for (i = 1; i < SMOOTH_POSES; i++)
        if (e->pose[i].used < e->pose[slot].used)
            slot = i;
    if (e->pose[slot].out == NULL)
        e->pose[slot].out = malloc(sizeof(float) * 3 * (size_t)n);
    if (e->pose[slot].out == NULL)
        return 1;
    memcpy(e->pose[slot].out, umk_subsurf_pos(e->s), sizeof(float) * 3 * (size_t)n);
    e->pose[slot].h = h;
    e->pose[slot].used = ++g_tick;
    return 1;
}

/* LIME_RenderMeshSingleIndexed's state calls, around the smoothed arrays. */
static void draw(smooth_entry *e, TEXTURE *tex, float alpha, const limeVECTOR3 *fade,
                 const uint16_t *idx)
{
    const UmkSubsurf *s = e->s;
    const int nverts = umk_subsurf_num_verts(s), ntris = umk_subsurf_num_tris(s);
    const float *uvs = umk_subsurf_uv(s);
    float sc;

    glShadeModel(0x1d01);               /* GL_SMOOTH */
    limeRenderedPolyCount += ntris;
    g_frame_tris += ntris;

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
        const int proxy = rgl_shadow_active() && !rb_nodraw();
        if (proxy) {                    /* REAL-TIME SHADOWS: the base mesh casts */
            glVertexPointer(3, GL_FLOAT, 0, e->pos);
            rgl_shadow_proxy(1);
            glDrawElements(GL_TRIANGLES, e->nfaces * 3, GL_UNSIGNED_SHORT, idx);
            glVertexPointer(3, GL_FLOAT, 0, umk_subsurf_pos(s));
            rgl_shadow_proxy(2);
        }
        if (!rb_nodraw())
            glDrawElements(GL_TRIANGLES, ntris * 3, GL_UNSIGNED_INT, umk_subsurf_tris(s));
        if (proxy)
            rgl_shadow_proxy(0);
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

    if (skin == NULL || rec == NULL || second == 2)
        return 0;                       /* the shadow is not smoothed */
    if (!second)
        cover_repose(skin);             /* the hat where it is this frame */
    if (!port_smooth_models) {
        if (!second)                    /* what the bones carry covers the body */
            tuck(skin, (int)*(const long *)r);
        return 0;
    }
    lime_port_skin_freed = port_smooth_forget;

    /* A frame's smoothed triangles have a budget: a fatality that draws the
     * same body ~120 times a frame (the copies in Scorpion's) asked for
     * 7 million smoothed triangles, each transformed on the CPU, and the
     * game fell to one frame a second. Past it the copies are drawn as the
     * model is; a fight's fighters are far below it. Counted per frame on
     * the screen, not in limeRenderedPolyCount: the game's tick clears that,
     * and a slow frame runs several ticks between its draws, so the budget
     * never filled (measured: ~150 copies of 180,000 vertices a frame,
     * 1 GB sent, ~900 ms frames). */
    if (g_frame_tris > SMOOTH_FRAME_TRIS) {
        if (!second)
            tuck(skin, (int)*(const long *)r);
        return 0;
    }

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
    if (e == NULL) {
        if (!second)                    /* the plain skin meanwhile, tucked too */
            tuck(skin, (int)nverts);
        return 0;
    }

    if (e->sharp_a != port_sharp_angle || e->sharp_s != port_sharp_strength) {
        umk_subsurf_set_sharp(e->s, port_sharp_angle, port_sharp_strength);
        e->sharp_a = port_sharp_angle;
        e->sharp_s = port_sharp_strength;
    }
    record_positions(e->nverts, e->pos);
    /* under the hat before it is smoothed, as the model viewer does
     * (mods/tuck.js): the head no longer comes through it */
    if (!second && (i = tuck_at(skin, e->nverts, e->pos, NULL, TUCK_INSIDE_CM,
                                TUCK_DEPTH_CM)) != 0 && !e->tucked) {
        printf("smooth models: %d vertices of a skin tucked under a piece on its bones\n", i);
        e->tucked = 1;
    }
    cover_fit(e);
    for (i = 0; i < e->nfaces * 3; i++)
        e->attr[i] = (float)light[idx[i]];
    if (!pose_restore_or_eval(e))
        return 0;

    n = umk_subsurf_num_verts(e->s);
    {
        const float *a = umk_subsurf_attr(e->s);
        for (i = 0; i < n; i++) {
            float v = a[i] + 0.5f;
            e->light[i] = (uint8_t)(v <= 0.0f ? 0 : v >= 255.0f ? 255 : (int)v);
        }
    }
    draw(e, tex, alpha, fade, idx);
    return 1;
}

