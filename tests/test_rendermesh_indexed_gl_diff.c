/*
 * test_rendermesh_indexed_gl_diff.c -- LIME_RenderMeshSingleIndexed (armv7
 * 0x5e358) by its GL call stream, against the recompiled original.
 *
 * The argument is the 88-byte frame record LoadAnimatedCharacter builds, not a
 * MESHINFO; positions come from RenderVerts. Both halves of the record (the
 * fifth argument), a NULL texture (the shadow pass) and a real one, several
 * alphas and fade vectors are driven. Every GL enum, count, stride and type
 * is compared exactly, array pointers by nullness; the faded colours in
 * TempRGBS and the running limeRenderedPolyCount are compared byte for byte.
 *
 * The oracle runs against the real image (UMK3_SLICE) so that its slots --
 * limeRenderedPolyCount, RenderVerts, VertScale, TempRGBS -- resolve.
 *
 * Build (i686): the RenderMesh.cpp oracle (tools/armrecomp/recomp.py
 * --file RenderMesh.cpp --name rendermesh), tests/gl_trace.c,
 * decomp/lime/RenderMesh.c and lime_globals.c, runtime/arm_runtime.c.
 */
#include "arm_runtime.h"
#include "gl_trace.h"
#include "rendermesh.h"
#include "../src/decomp/lime/lime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RAM_SIZE   (16u << 20)
#define STACK_TOP  0x00F00000u
#define G_REC      0x00800000u
#define G_LIGHT    0x00801000u
#define G_UV       0x00802000u
#define G_IDX      0x00803000u
#define G_TEX      0x00804000u
#define G_FADE     0x00805000u

#define O_TEMPRGBS   0x00298174u
#define O_POLYCOUNT  0x00171be4u

#define NV 24
#define NF 16

long         limeRenderedPolyCount;
static short h_verts[NV * 3];
limeVECTOR3 *RenderVerts = (limeVECTOR3 *)h_verts;

static int  g_fail;
static long g_cases;
static int  g_ptr_only;
static uint32_t g_seed = 0x0badf00du;
static uint32_t nextu(void) { g_seed = g_seed * 1664525u + 1013904223u; return g_seed >> 8; }
static float nextf(float lo, float hi) { return lo + (hi - lo) * (float)(nextu() & 0xFFFFu) / 65535.0f; }

static unsigned char h_rec[0x58];
static uint8_t  h_light[2][NV];
static float    h_uv[2][NV * 2];
static uint16_t h_idx[2][NF * 3];
static TEXTURE  h_tex;

static void build(void)
{
    int h, i;

    memset(h_rec, 0, sizeof(h_rec));
    for (h = 0; h < 2; h++) {
        long nv = 1 + (long)(nextu() % NV), nf = 1 + (long)(nextu() % NF);
        uint32_t off = h ? 8u : 0u;
        uint32_t gl = G_LIGHT + 0x100u * (uint32_t)h, gu = G_UV + 0x200u * (uint32_t)h,
                 gi = G_IDX + 0x200u * (uint32_t)h;

        for (i = 0; i < NV; i++) {
            h_light[h][i] = (uint8_t)nextu();
            h_uv[h][2 * i] = nextf(0, 1);
            h_uv[h][2 * i + 1] = nextf(0, 1);
        }
        for (i = 0; i < NF * 3; i++)
            h_idx[h][i] = (uint16_t)(nextu() % NV);
        memcpy(g_ram + gl, h_light[h], NV);
        memcpy(g_ram + gu, h_uv[h], sizeof(h_uv[h]));
        memcpy(g_ram + gi, h_idx[h], sizeof(h_idx[h]));

        *(long *)(h_rec + 0x00 + off) = nv;
        *(long *)(h_rec + 0x04 + off) = nf;
        *(void **)(h_rec + 0x1c + 0x10 * h) = h_idx[h];
        *(void **)(h_rec + 0x20 + 0x10 * h) = h_uv[h];
        *(void **)(h_rec + 0x24 + 0x10 * h) = h_light[h];
        MEM_ST32(G_REC + 0x00 + off, (uint32_t)nv);
        MEM_ST32(G_REC + 0x04 + off, (uint32_t)nf);
        MEM_ST32(G_REC + 0x1c + 0x10u * (uint32_t)h, gi);
        MEM_ST32(G_REC + 0x20 + 0x10u * (uint32_t)h, gu);
        MEM_ST32(G_REC + 0x24 + 0x10u * (uint32_t)h, gl);
    }
    /* +0x18 and +0x28: the visibility flags, never read here */
    *(long *)(h_rec + 0x18) = 1;
    *(long *)(h_rec + 0x28) = 1;
    MEM_ST32(G_REC + 0x18, 1);
    MEM_ST32(G_REC + 0x28, 1);

    memset(&h_tex, 0, sizeof(h_tex));
    h_tex.name = 0x3003u;
    for (i = 0; i < 0x60; i += 4) MEM_ST32(G_TEX + (uint32_t)i, 0);
    MEM_ST32(G_TEX + 0x40, 0x3003u);
}

static void run(const char *what, int useTex, long second, float alpha,
                const limeVECTOR3 *fade)
{
    arm_ctx ctx;

    memset(TempRGBS, 0, 4 * NV);
    memset(g_ram + O_TEMPRGBS, 0, 4 * NV);
    limeRenderedPolyCount = 7;
    MEM_ST32(O_POLYCOUNT, 7);

    glt_reset();

    glt_select(&glt_clean);
    LIME_RenderMeshSingleIndexed(h_rec, useTex ? &h_tex : NULL, alpha, fade,
                                 second);

    glt_select(&glt_oracle);
    memset(&ctx, 0, sizeof(ctx));
    ctx.r[SP] = STACK_TOP;
    MEM_ST32(G_FADE + 0, F32_U32(fade->x));
    MEM_ST32(G_FADE + 4, F32_U32(fade->y));
    MEM_ST32(G_FADE + 8, F32_U32(fade->z));
    ctx.r[0] = G_REC;
    ctx.r[1] = useTex ? G_TEX : 0u;
    ctx.r[2] = F32_U32(alpha);
    ctx.r[3] = G_FADE;
    MEM_ST32(STACK_TOP, (uint32_t)second);
    func_0005e358_LIME_RenderMeshSingleIndexed(&ctx);

    g_cases++;
    g_fail += glt_compare(what, &g_ptr_only);

    g_cases++;
    if (memcmp(TempRGBS, g_ram + O_TEMPRGBS, 4 * NV) != 0) {
        printf("  DIVERGE %s: TempRGBS\n", what);
        g_fail++;
    }
    g_cases++;
    if ((uint32_t)limeRenderedPolyCount != MEM_LD32(O_POLYCOUNT)) {
        printf("  DIVERGE %s: limeRenderedPolyCount %ld vs %u\n", what,
               limeRenderedPolyCount, MEM_LD32(O_POLYCOUNT));
        g_fail++;
    }
}

int main(void)
{
    const char *slice = getenv("UMK3_SLICE");
    char lbl[96];
    int n, t, s, a;

    setvbuf(stdout, NULL, _IONBF, 0);
    arm_mem_init(RAM_SIZE);
    if (slice == NULL || arm_load_image(slice) != 0) {
        printf("set UMK3_SLICE to the armv7 slice\n");
        return 2;
    }

    printf("=== clean LIME_RenderMeshSingleIndexed vs the recompiled original ===\n\n");

    for (n = 0; n < 200; n++) {
        static const float A[4] = { 1.0f, 0.65f, 0.3f, 0.0f };
        limeVECTOR3 fade;

        build();
        /* Non-negative: a channel below zero indexes before its ScaleTable
         * row, and before row 0 that is memory the two sides do not share. */
        fade.x = nextf(0, 1); fade.y = nextf(0, 1); fade.z = nextf(0, 1);
        for (t = 0; t < 2; t++)
            for (s = 0; s < 2; s++)
                for (a = 0; a < 4; a++) {
                    snprintf(lbl, sizeof lbl, "case %d tex=%d half=%d alpha=%.2f",
                             n, t, s, A[a]);
                    run(lbl, t, s, A[a], &fade);
                }
    }

    printf("\ncases compared: %ld    divergences: %d\n", g_cases, g_fail);
    printf("pointer args checked only for nullness: %d\n", g_ptr_only);
    printf("%s\n", g_fail ? "RESULT: FAIL" : "RESULT: PASS");
    arm_mem_free();
    return g_fail ? 1 : 0;
}
