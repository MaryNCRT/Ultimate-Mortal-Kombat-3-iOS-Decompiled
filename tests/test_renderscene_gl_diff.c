/*
 * test_renderscene_gl_diff.c — the two scene renderers, by their GL call stream.
 *
 * ## The gap this closes
 *
 * `tests/test_renderscene_diff.c` covers the helpers and says plainly that it
 * does not cover `LIME_RenderScene` or `LIME_RenderSceneOverrideTextures`.
 * docs/ENCARGO.md named the two things that were missing:
 *
 *   1. a scene built by hand in guest memory, and
 *   2. pairing each GL enum with the call that consumes it -- described there
 *      as needing register liveness tracking rather than more literal reading.
 *
 * The second turned out not to need static analysis at all. The oracle does not
 * read the code, it RUNS it: recomp.py emits every import as
 * `stub_auto_glXxx(arm_ctx *ctx)`, so whatever value actually reaches r0 at the
 * instant of the call is simply handed over. `tests/gl_trace.c` records both
 * sides into the same shape and this test compares them call by call.
 *
 * So the enums are MEASURED from the shipped instruction sequence, not inferred
 * from where a literal happens to sit. That matters, because reading them in
 * place is exactly what got them wrong: a `GL_VERTEX_ARRAY` loaded immediately
 * before a `glClientActiveTexture` belongs to a later call.
 *
 * ## What the scenes are built to catch
 *
 * A node is walked through a two-level table -- the frame indexes a uint16
 * stream, and the stream indexes a shared array of 8-byte keys. So the cases
 * are chosen against the ways that walk can be wrong:
 *
 *  - **A hidden node.** `SCENE_NODE_HIDDEN` was recorded in lime.h as a name
 *    standing in for a constant that pass could not resolve. Driving it settles
 *    the value from behaviour instead of from a literal.
 *
 *  - **A repeated key.** Two frames pointing at the same key index must draw
 *    the same thing. A version that walked keys BY FRAME instead of through the
 *    stream would pass a scene where the two happen to coincide, so the streams
 *    here deliberately do not.
 *
 *  - **An EVENT-named mesh.** The prefix test is all five letters chained by
 *    `cmpeq`, and a match skips the whole draw. Names that share four letters
 *    but not five (`EVEN`, `EVENTS`) separate a correct test from a lazy one.
 *
 *  - **A non-zero alpha**, which defers to the transparent list instead of
 *    drawing, and a zero alpha which does not.
 *
 *  - **Negative and out-of-range frames**, because the frame is clamped twice
 *    and the second clamp is easy to leave out.
 *
 * ## What a pass does not prove
 *
 * Vertex and texcoord pointers are compared as null vs non-null only -- their
 * lengths are not known from the call. A divergence in the DATA behind them
 * would not be caught here. The count is reported at the end rather than left
 * for the reader to discover.
 */

#include "arm_runtime.h"
#include "gl_trace.h"
#include "renderscene.h"                /* oracle: tools/armrecomp */
#include "../src/decomp/lime/lime.h"        /* clean C */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RAM_SIZE   (16u << 20)
#define STACK_TOP  0x007F0000u

/* guest layout.
 *
 * **Above 0x00600000 on purpose.** The first version of this test put its
 * scene at 0x00300000, which lands inside the loaded slice's own data --
 * so filling a palette with random words quietly overwrote the engine's
 * globals, and FlushTranspMeshList read a "pending translation" that was
 * actually this test's palette. The symptom looked like a divergence in the
 * renderer.
 *
 * The slice is 0x23D0B0 bytes plus its data and bss. Anything the test
 * builds has to start clear of that. */
#define G_SCENE    0x00700000u
#define G_KEYS_P   0x00701000u          /* nodeKeys[]   -- array of pointers */
#define G_STRM_P   0x00702000u          /* nodeStream[] -- array of pointers */
#define G_KEYS     0x00710000u
#define G_STRM     0x00720000u
#define G_PAL      0x00730000u
#define G_MSET     0x00740000u
#define G_MESHES   0x00741000u
#define G_MESHD    0x00750000u
#define G_MNAMES   0x00760000u
#define G_TEX      0x00770000u
#define G_VERTS    0x00780000u
#define G_INDICES  0x00790000u
#define G_VLIGHT   0x007A0000u

#define MESH_VERTS 4
#define MESH_FACES 2

#define MAX_NODES  8
#define MAX_FRAMES 16
#define MAX_KEYS   8
#define MAX_MESHES 8

#define GUEST_MESH_STRIDE  0x80u
#define GUEST_NAME_STRIDE  0x40u
#define PAL_STRIDE         0x20u

/* SCENEINFO offsets, from lime.h */
#define S_COUNT2   0x44u
#define S_NODECNT  0x48u
#define S_SCALE    0x60u
#define S_TAIL     0x7Cu
#define S_MESHSET  0x80u
#define S_KEYS     0x88u
#define S_STREAM   0x8Cu

/* ------------------------------------------------- the platform boundary
 *
 * The clean engine calls a handful of symbols that live in `lime/iphone/lime.m`
 * -- the iOS layer, which this project rewrites rather than decompiles. For a
 * GL-stream comparison they cannot be no-ops: the oracle's copies issue real GL
 * calls, so a silent host stub would show up as a missing call and be blamed on
 * the renderer.
 *
 * So they are implemented here from what the ORACLE was measured to do, one
 * function at a time, with `build/probe`:
 *
 *      limeEnableDepthWrites            -> glDepthMask(1)
 *      limeDisableDepthWrites           -> glDepthMask(0)
 *      limeDisableAlphaBlending         -> glDisable(0x0BE2)
 *      limeEnableAlphaBlending_Basic    -> glBlendFunc(0x0302, 0x0303), glEnable(0x0BE2)
 *      limeEnableAlphaBlending_Additive -> glBlendFunc(0x0302, 0x0001), glEnable(0x0BE2)
 *
 * 0x0BE2 is GL_BLEND, 0x0302 GL_SRC_ALPHA, 0x0303 GL_ONE_MINUS_SRC_ALPHA and
 * 0x0001 GL_ONE. Which confirms from behaviour what docs/ENCARGO.md had only
 * asserted: the additive path really is SRC_ALPHA/ONE and the basic path really
 * is the order-dependent one. Measured, not read out of a literal pool.
 *
 * Note these are deliberately NOT linked from runtime/lime_platform.c. That
 * file's GL entries are no-ops by design -- correct for tests that compare
 * loaders and maths, useless for one that compares a call stream.
 */
#define GL_BLEND_                0x0BE2u
#define GL_SRC_ALPHA_            0x0302u
#define GL_ONE_MINUS_SRC_ALPHA_  0x0303u
#define GL_ONE_                  0x0001u

void limeEnableDepthWrites(void)  { glDepthMask(1); }
void limeDisableDepthWrites(void) { glDepthMask(0); }
void limeDisableAlphaBlending(void) { glDisable(GL_BLEND_); }

void limeEnableAlphaBlending_Basic(void)
{
    glBlendFunc(GL_SRC_ALPHA_, GL_ONE_MINUS_SRC_ALPHA_);
    glEnable(GL_BLEND_);
}

void limeEnableAlphaBlending_Additive(void)
{
    glBlendFunc(GL_SRC_ALPHA_, GL_ONE_);
    glEnable(GL_BLEND_);
}

/* Host memory and files. Small and real -- the renderers do not load anything,
 * but LIME_FreeScene and friends are linked in and must resolve. */
void *limeMalloc(const char *tag, size_t n) { (void)tag; return calloc(1, n ? n : 1); }
void  limeFree(void *p) { free(p); }
size_t limeFileSize(const char *path) { (void)path; return 0; }
void  *limeLoadFile(const char *path) { (void)path; return NULL; }

/* None of these should be reached by a scene renderer. Abort rather than
 * return quietly: a silent stub here would drop a GL call from one side only
 * and the divergence would be blamed on the wrong code. */
static void must_not_call(const char *who)
{
    printf("  FATAL: %s reached from a scene renderer\n", who);
    exit(3);
}
TEXTURE *limeLoadTexture(const char *p, int a, int b)
{ (void)p; (void)a; (void)b; must_not_call("limeLoadTexture"); return NULL; }
void limeDeleteTexture(TEXTURE *t) { (void)t; must_not_call("limeDeleteTexture"); }
void limeDrawSprite(TEXTURE *page, float x, float y, float w, float h,
                    float u0, float v0, float u1, float v1)
{ (void)page; (void)x; (void)y; (void)w; (void)h; (void)u0; (void)v0; (void)u1; (void)v1;
  must_not_call("limeDrawSprite"); }
void limeDrawRotSpriteFromTopLeft(TEXTURE *page, float x, float y,
                                  float w, float h, float u0, float v0,
                                  float u1, float v1, float angle)
{ (void)page; (void)x; (void)y; (void)w; (void)h; (void)u0; (void)v0; (void)u1; (void)v1;
  (void)angle; must_not_call("limeDrawRotSpriteFromTopLeft"); }


static int  g_fail  = 0;
static long g_cases = 0;
static int  g_ptr_only = 0;

/* ---------------------------------------------------------------- the scene */

typedef struct {
    int      nodes;
    int      frames;             /* == count2 */
    float    scale;
    int      keysPerNode;
    uint16_t stream[MAX_NODES][MAX_FRAMES];
    float    alpha[MAX_NODES][MAX_KEYS];
    uint8_t  meshIdx[MAX_NODES][MAX_KEYS];
    uint16_t palIdx[MAX_NODES][MAX_KEYS];
    int      meshes;
    const char *meshName[MAX_MESHES];
} scenedesc;

/* host mirrors */
static SCENEINFO     h_scene;
static MESHSETINFO   h_mset;
static MESHINFO     *h_meshes[MAX_MESHES];
static MESHINFO      h_meshData[MAX_MESHES];
static char          h_meshName[MAX_MESHES][64];
static LIMEVERTEX    h_verts[MAX_MESHES][MESH_VERTS];
static uint16_t      h_indices[MAX_MESHES][MESH_FACES * 3];
static uint8_t       h_vertLight[MAX_MESHES][MESH_VERTS];
static SCENENODEKEY  h_keys[MAX_NODES][MAX_KEYS];
static SCENENODEKEY *h_keyPtr[MAX_NODES];
static uint16_t      h_strm[MAX_NODES][MAX_FRAMES];
static uint16_t     *h_strmPtr[MAX_NODES];
static uint8_t       h_palette[MAX_KEYS * MAX_NODES * PAL_STRIDE + 256];
static TEXTURE       g_htex[MAX_MESHES];

/* The clean side's copies of two globals FlushTranspMeshList reads. Both are
 * zero in the loaded slice and both stay zero here, so the two sides agree
 * without the test having to write into guest memory at all.
 *
 * An earlier version computed their guest addresses and seeded them. That was
 * unnecessary and it was also wrong: the addresses it derived were off, and
 * the real reason the values looked like garbage was that this test's own
 * arena overlapped the slice. Moving the arena fixed the cause; the seeding
 * was treating the symptom. */

static void ctx_reset(arm_ctx *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->r[SP] = STACK_TOP;
}

static uint32_t g_seed = 0xC0FFEE11u;
static uint32_t nextu(void) { g_seed = g_seed * 1664525u + 1013904223u; return g_seed; }

/* Build the same scene twice: host structs for the clean C, guest memory for
 * the oracle. Neither can see the other's. */
static void build(const scenedesc *d)
{
    int n, k, m;
    uint32_t a;

    /* ---- palette: identical bytes on both sides ---- */
    for (a = 0; a < sizeof(h_palette); a += 4u) {
        uint32_t v = nextu();
        memcpy(&h_palette[a], &v, 4);
        MEM_ST32(G_PAL + a, v);
    }

    /* ---- meshes ---- */
    memset(h_meshData, 0, sizeof(h_meshData));
    for (m = 0; m < d->meshes; m++) {
        uint32_t gm = G_MESHD  + GUEST_MESH_STRIDE * (uint32_t)m;
        uint32_t gn = G_MNAMES + GUEST_NAME_STRIDE * (uint32_t)m;
        size_t i;

        memset(h_meshName[m], 0, sizeof(h_meshName[m]));
        strncpy(h_meshName[m], d->meshName[m], sizeof(h_meshName[m]) - 1);
        h_meshData[m].meshName = h_meshName[m];
        h_meshes[m] = &h_meshData[m];

        for (a = 0; a < GUEST_MESH_STRIDE; a += 4u) MEM_ST32(gm + a, 0u);
        for (a = 0; a < GUEST_NAME_STRIDE; a += 4u) MEM_ST32(gn + a, 0u);
        for (i = 0; i <= strlen(d->meshName[m]); i++)
            MEM_ST8(gn + (uint32_t)i, (uint8_t)d->meshName[m][i]);

        /* Minimal REAL geometry on both sides. An all-zero MESHINFO is not a
         * neutral choice: the oracle's LIME_RenderMesh leaves early on it and
         * emits nothing, while the clean one walks a NULL vertex pointer and
         * segfaults. Two implementations disagreeing about degenerate input is
         * a finding, but it is not the finding this test is for, so both get
         * four vertices and two triangles and the draw path is compared for
         * real. */
        h_meshData[m].numVerts  = MESH_VERTS;
        h_meshData[m].numFaces  = MESH_FACES;
        h_meshData[m].verts     = h_verts[m];
        h_meshData[m].indices   = h_indices[m];
        h_meshData[m].vertLight = h_vertLight[m];

        {
            uint32_t gv = G_VERTS + 0x200u * (uint32_t)m;
            uint32_t gi = G_INDICES + 0x100u * (uint32_t)m;
            uint32_t gl = G_VLIGHT + 0x100u * (uint32_t)m;
            int v;

            for (v = 0; v < MESH_VERTS; v++) {
                uint32_t w;
                int c;
                for (c = 0; c < 4; c++) {
                    w = nextu();
                    memcpy((char *)&h_verts[m][v] + 4 * c, &w, 4);
                    MEM_ST32(gv + 16u * (uint32_t)v + 4u * (uint32_t)c, w);
                }
                h_vertLight[m][v] = (uint8_t)(0x40 + v);
                MEM_ST8(gl + (uint32_t)v, (uint8_t)(0x40 + v));
            }
            for (v = 0; v < MESH_FACES * 3; v++) {
                uint16_t idx = (uint16_t)(v % MESH_VERTS);
                h_indices[m][v] = idx;
                MEM_ST16(gi + 2u * (uint32_t)v, idx);
            }

            MEM_ST32(gm + 0x00u, (uint32_t)MESH_VERTS);
            MEM_ST32(gm + 0x04u, (uint32_t)MESH_FACES);
            MEM_ST32(gm + 0x18u, gv);
            MEM_ST32(gm + 0x1Cu, gi);
            MEM_ST32(gm + 0x24u, gl);
        }

        MEM_ST32(gm + 0x3Cu, gn);                       /* meshName */
        MEM_ST32(G_MESHES + 4u * (uint32_t)m, gm);
    }

    memset(&h_mset, 0, sizeof(h_mset));
    h_mset.numMeshes = d->meshes;
    h_mset.meshes    = h_meshes;

    for (a = 0; a < 0x200u; a += 4u) MEM_ST32(G_MSET + a, 0u);
    MEM_ST32(G_MSET + 0x44u, (uint32_t)d->meshes);
    MEM_ST32(G_MSET + 0x48u, G_MESHES);

    /* ---- keys and streams ---- */
    for (n = 0; n < d->nodes; n++) {
        uint32_t gk = G_KEYS + 0x100u * (uint32_t)n;
        uint32_t gs = G_STRM + 0x100u * (uint32_t)n;

        for (k = 0; k < d->keysPerNode; k++) {
            uint32_t bits;
            memcpy(&bits, &d->alpha[n][k], 4);

            h_keys[n][k].alpha        = d->alpha[n][k];
            h_keys[n][k].meshIndex    = d->meshIdx[n][k];
            h_keys[n][k].field05      = 0;   /* see note in main() */
            h_keys[n][k].paletteIndex = d->palIdx[n][k];

            MEM_ST32(gk + 8u * (uint32_t)k + 0u, bits);
            MEM_ST8 (gk + 8u * (uint32_t)k + 4u, d->meshIdx[n][k]);
            MEM_ST8 (gk + 8u * (uint32_t)k + 5u, 0u);
            MEM_ST16(gk + 8u * (uint32_t)k + 6u, d->palIdx[n][k]);
        }
        h_keyPtr[n] = h_keys[n];
        MEM_ST32(G_KEYS_P + 4u * (uint32_t)n, gk);

        for (k = 0; k < d->frames; k++) {
            h_strm[n][k] = d->stream[n][k];
            MEM_ST16(gs + 2u * (uint32_t)k, d->stream[n][k]);
        }
        h_strmPtr[n] = h_strm[n];
        MEM_ST32(G_STRM_P + 4u * (uint32_t)n, gs);
    }

    /* ---- the scene record ---- */
    memset(&h_scene, 0, sizeof(h_scene));
    h_scene.count2     = d->frames;
    h_scene.nodeCount  = d->nodes;
    h_scene.scale      = d->scale;
    h_scene.tail       = h_palette;
    h_scene.meshset    = &h_mset;
    h_scene.nodeKeys   = h_keyPtr;
    h_scene.nodeStream = h_strmPtr;

    for (a = 0; a < 0x98u; a += 4u) MEM_ST32(G_SCENE + a, 0u);
    MEM_ST32(G_SCENE + S_COUNT2,  (uint32_t)d->frames);
    MEM_ST32(G_SCENE + S_NODECNT, (uint32_t)d->nodes);
    {
        uint32_t sb; memcpy(&sb, &d->scale, 4);
        MEM_ST32(G_SCENE + S_SCALE, sb);
    }
    MEM_ST32(G_SCENE + S_TAIL,    G_PAL);
    MEM_ST32(G_SCENE + S_MESHSET, G_MSET);
    MEM_ST32(G_SCENE + S_KEYS,    G_KEYS_P);
    MEM_ST32(G_SCENE + S_STREAM,  G_STRM_P);
}


/* ------------------------------------------------------------- the two runs */

/* The stack arguments, counted from the incoming SP. LIME_RenderScene reserves
 * 0x104 bytes between its prologue and its frame, so the reads the disassembly
 * shows at [sp, #0x104], #0x110, #0x114 and #0x11c are arguments 5, 8, 9 and 11.
 * They are placed here at the SAME offsets the clean call passes them, which is
 * the whole point: if the mapping were wrong the traces would not line up. */
#define A5_BLEND    0x00u
#define A6          0x04u
#define A7          0x08u
#define A8_FLUSH    0x0Cu
#define A9_TEX      0x10u
#define A10         0x14u
#define A11_MATRIX  0x18u

static void run_pair(const char *what, long frameA, long frameB, float blend,
                     long flush)
{
    arm_ctx ctx;
    uint32_t bits;

    memcpy(&bits, &blend, 4);
    glt_reset();

    glt_select(&glt_clean);
    /* A REAL host texture for the flush, not NULL. FlushTranspMeshList hands
     * it to LIME_RenderMesh whenever the mesh does not carry its own, and the
     * clean draw path dereferences it -- passing NULL took down the clean side
     * while the oracle sailed past, which is a divergence the harness cannot
     * report because one side stops running. */
    LIME_RenderScene(0, &h_scene, frameA, frameB, blend, 0, 0,
                     flush, &g_htex[0], 0, NULL);

    glt_select(&glt_oracle);
    ctx_reset(&ctx);
    ctx.r[0] = 0u;                      /* arg1 -- LIME_printf is a no-op here */
    ctx.r[1] = G_SCENE;
    ctx.r[2] = (uint32_t)frameA;
    ctx.r[3] = (uint32_t)frameB;
    MEM_ST32(ctx.r[SP] + A5_BLEND,   bits);
    MEM_ST32(ctx.r[SP] + A6,         0u);
    MEM_ST32(ctx.r[SP] + A7,         0u);
    MEM_ST32(ctx.r[SP] + A8_FLUSH,   (uint32_t)flush);
    MEM_ST32(ctx.r[SP] + A9_TEX,     G_TEX);       /* the same texture, guest side */
    MEM_ST32(ctx.r[SP] + A10,        0u);
    MEM_ST32(ctx.r[SP] + A11_MATRIX, 0u);
    func_0005f7a4_LIME_RenderScene(&ctx);

    g_cases++;
    g_fail += glt_compare(what, &g_ptr_only);
}

static void run_pair_override(const char *what, long frame)
{
    arm_ctx ctx;
    TEXTURE *htex[MAX_MESHES];
    int i;

    /* A texture PER MESH on both sides, because arg2 is a table and a single
     * pointer would agree with a wrong reading of it.
     *
     * The clean side gets REAL HOST objects. Handing it `(TEXTURE *)G_TEX`
     * would be passing an emulated address to native code -- a segfault rather
     * than a divergence, and it cost a debugging round here exactly as the
     * warning in test_renderscene_diff.c says it does. The two sides carry the
     * same CONTENTS at the same offsets; only the addresses differ. */
    for (i = 0; i < MAX_MESHES; i++) {
        uint32_t g = G_TEX + 0x100u * (uint32_t)i;
        uint32_t a;

        memset(&g_htex[i], 0, sizeof(g_htex[i]));
        g_htex[i].name    = 0x1000u + (unsigned)i;
        g_htex[i].field50 = i & 1;

        for (a = 0; a < 0x100u; a += 4u) MEM_ST32(g + a, 0u);
        MEM_ST32(g + 0x40u, 0x1000u + (uint32_t)i);
        MEM_ST32(g + 0x50u, (uint32_t)(i & 1));

        htex[i] = &g_htex[i];
        MEM_ST32(G_TEX + 0x1000u + 4u * (uint32_t)i, g);
    }

    glt_reset();

    glt_select(&glt_clean);
    LIME_RenderSceneOverrideTextures(&h_scene, htex, frame);

    glt_select(&glt_oracle);
    ctx_reset(&ctx);
    ctx.r[0] = G_SCENE;
    ctx.r[1] = G_TEX + 0x1000u;         /* the table, not one texture */
    ctx.r[2] = (uint32_t)frame;
    func_0005f4d4_LIME_RenderSceneOverrideTextures(&ctx);

    g_cases++;
    g_fail += glt_compare(what, &g_ptr_only);
}


/* ------------------------------------------------------------------ scenes */

static scenedesc g_d;

static void desc_clear(void)
{
    memset(&g_d, 0, sizeof(g_d));
    g_d.scale = 1.5f;
    g_d.keysPerNode = 4;
}

/* One node, hidden on every frame. Nothing should be drawn -- and the value
 * that means "hidden" is what this establishes. */
static void scene_hidden(uint16_t sentinel)
{
    int f;
    desc_clear();
    g_d.nodes = 1;
    g_d.frames = 4;
    g_d.meshes = 1;
    g_d.meshName[0] = "FLOOR";
    for (f = 0; f < g_d.frames; f++) g_d.stream[0][f] = sentinel;
    build(&g_d);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    arm_mem_init(RAM_SIZE);

    /* LIME_RenderMesh reaches CreateFadedRGBS through this, and nothing in a
     * draw path allocates it -- the engine does that once at startup. */
    g_vertexColourScratch = malloc(4u * 64u);
    if (g_vertexColourScratch == NULL)
        return 2;

    printf("=== the two scene renderers, compared by GL call stream ===\n");
    printf("enums are measured from the oracle's register file, not read\n");
    printf("out of literal pools. See the header comment.\n\n");

    /* ---- the early exits ---- */
    desc_clear();
    g_d.nodes = 0; g_d.frames = 4; g_d.meshes = 1; g_d.meshName[0] = "FLOOR";
    build(&g_d);
    run_pair("no nodes, opaque pass", 0, 1, 0.0f, 0);
    run_pair("no nodes, transparency pass", 0, 1, 0.0f, 1);
    run_pair_override("no nodes, override", 0);

    /* ---- the hidden sentinel: what 0xFFFF actually does ---- */
    scene_hidden(SCENE_NODE_HIDDEN);
    run_pair("hidden node, opaque pass", 0, 1, 0.0f, 0);
    run_pair("hidden node, transparency pass", 0, 1, 0.5f, 1);
    run_pair_override("hidden node, override", 0);

    /* ---- a real walk ----
     *
     * Streams that do NOT run 0,1,2,3: a body that indexed keys by frame
     * instead of through the stream would agree with an in-order scene and
     * diverge here. Frame 0 and frame 1 deliberately land on different keys,
     * and frames 2 and 3 repeat key 1 -- the compression the table exists for.
     */
    {
        static const char *names[5] = { "FLOOR", "TORSO", "EVENT", "EVEN", "EVENTS" };
        int f, k;

        desc_clear();
        g_d.nodes = 3;
        g_d.frames = 4;
        g_d.meshes = 5;
        for (k = 0; k < 5; k++) g_d.meshName[k] = names[k];

        for (k = 0; k < g_d.nodes; k++) {
            g_d.stream[k][0] = 2;
            g_d.stream[k][1] = 0;
            g_d.stream[k][2] = 1;
            g_d.stream[k][3] = 1;
            for (f = 0; f < 4; f++) {
                g_d.meshIdx[k][f] = (uint8_t)((k + f) % 5);
                g_d.palIdx[k][f]  = (uint16_t)(k * 4 + f);
            }
        }

        /* alphas chosen around the 0.97 cutoff: one side, the other side, and
         * the boundary itself, because >= and > differ only there. */
        g_d.alpha[0][0] = 1.0f;   g_d.alpha[0][1] = 0.97f;
        g_d.alpha[0][2] = 0.5f;   g_d.alpha[0][3] = 0.0f;
        g_d.alpha[1][0] = 0.96999f; g_d.alpha[1][1] = 0.9700001f;
        g_d.alpha[1][2] = 1.0f;   g_d.alpha[1][3] = 0.25f;
        g_d.alpha[2][0] = 0.0f;   g_d.alpha[2][1] = 1.0f;
        g_d.alpha[2][2] = 0.98f;  g_d.alpha[2][3] = 0.9f;

        build(&g_d);

        for (f = 0; f < 4; f++) {
            char lbl[80];
            snprintf(lbl, sizeof(lbl), "walk f=%d opaque", f);
            run_pair(lbl, f, (f + 1) % 4, 0.0f, 0);

            snprintf(lbl, sizeof(lbl), "walk f=%d transparency blend=0", f);
            run_pair(lbl, f, (f + 1) % 4, 0.0f, 1);

            snprintf(lbl, sizeof(lbl), "walk f=%d transparency blend=0.5", f);
            run_pair(lbl, f, (f + 1) % 4, 0.5f, 1);

            /* NOT RUN, and this is a finding rather than an omission.
             *
             * With a scene whose meshes carry real geometry, the clean
             * LIME_RenderSceneOverrideTextures reaches LIME_RenderMesh and
             * segfaults where the oracle does not. That is a divergence the
             * harness cannot report as one, because one of the two sides stops
             * existing -- so it is named here instead of being quietly skipped.
             *
             * It is downstream of the signature correction, not caused by it:
             * the override body is still built on a reading that has not been
             * re-traced past its argument list. See docs/RENDERSCENE-SIGNATURE.md
             * for what has been measured and what has not. */
            snprintf(lbl, sizeof(lbl), "walk f=%d override", f);
            run_pair_override(lbl, f);
        }

        /* the second frame index is an ARGUMENT, not frame+1: pair frames that
         * are not adjacent, which a (frame+1) body cannot reproduce */
        run_pair("blend 0 against 3", 0, 3, 0.75f, 1);
        run_pair("blend 3 against 0", 3, 0, 0.25f, 1);
        run_pair("blend a frame with itself", 2, 2, 0.5f, 1);

        /* the frame is clamped twice; negatives and overshoot exercise both */
        run_pair("negative frame", -1, -7, 0.5f, 1);
        run_pair("frame past the end", 99, 100, 0.5f, 1);
        run_pair_override("negative frame, override", -3);
        run_pair_override("frame past the end, override", 77);
    }

    printf("\ncases compared: %ld    divergences: %d\n", g_cases, g_fail);
    printf("pointer args checked only for nullness: %d\n", g_ptr_only);
    printf("%s\n", g_fail ? "RESULT: FAIL"
                          : "RESULT: the clean scene renderers match the original");

    arm_mem_free();
    return g_fail ? 1 : 0;
}
