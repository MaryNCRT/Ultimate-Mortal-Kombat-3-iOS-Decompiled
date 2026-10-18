/*
 * test_skinning_armv7_diff.c -- the per-frame skinning chain against the
 * recompiled armv7 original.
 *
 *      GenerateMatrices -> CreateMatrixPaletteForGeneratingMesh
 *                       -> UnpackAnimFrame, LerpVector3, GetSlerpedQ,
 *                          CreateMatrixPaletteRecurse2      (0x60048..0x60374)
 *      DrawSkinnedMesh2, flags = 0, both keepFloats paths    (0x608d8)
 *
 * The clean side and the oracle are given the same skeleton, animation, skin
 * and palette, each in its own memory, and every output is compared bit for
 * bit: MatrixPalette2, Root_Trans0, DecompAnimFrames0, the cursors,
 * SkinnedVerts, the positions, the unwelded UVs, RenderIndexes and the
 * returned vertex count. The oracle runs against the real image (UMK3_SLICE)
 * so its globals sit at their own addresses.
 *
 * The lit path (flags != 0) is not covered: it reads the light set-up, which
 * the clean side still keeps under different names.
 *
 * Build (i686, as the game):
 *   python tools/armrecomp/recomp.py UMK3.armv7 --file RenderSkinned.cpp \
 *       --out build/rc --name renderskinned
 *   python tools/armrecomp/recomp.py UMK3.armv7 --file limeVector.cpp \
 *       --out build/rc --name limevector
 *   i686-w64-mingw32-gcc -std=gnu11 -O1 -I runtime -I build/rc -I decomp/lime \
 *       tests/test_skinning_armv7_diff.c tests/lime_unused_stubs.c \
 *       decomp/lime/RenderSkinned.c decomp/lime/limeVector.c \
 *       decomp/lime/lime_globals.c build/rc/renderskinned.c \
 *       build/rc/renderskinned_shims.c build/rc/limevector.c \
 *       build/rc/limevector_shims.c runtime/arm_runtime.c -lm
 *   UMK3_SLICE=<UMK3.armv7> ./a.exe
 */
#include "arm_runtime.h"
#include "renderskinned.h"
#include "../src/decomp/lime/lime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RAM_SIZE   (16u << 20)
#define STACK_TOP  0x00F00000u
#define G_BASE     0x00800000u      /* test data, clear of the image */

/* the original's own addresses */
#define O_PALETTE   0x002c3f48u
#define O_SKINNED   0x002c5b68u
#define O_RINDEX    0x0036a820u
#define O_FRAMES0   0x0036f640u
#define O_ROOT0     0x00370db0u
#define O_SRC0      0x00370dc8u
#define O_COUNT     0x00370dd0u
#define O_DST2      0x00370dd4u

static int  g_fail;
static long g_cases;
static uint32_t g_seed = 0x2468ace1u;

static uint32_t nextu(void) { g_seed = g_seed * 1664525u + 1013904223u; return g_seed >> 8; }
static float nextf(float lo, float hi) { return lo + (hi - lo) * (float)(nextu() & 0xFFFFu) / 65535.0f; }

static void cmp_bytes(const char *what, const void *clean, uint32_t orc, size_t n)
{
    g_cases++;
    if (memcmp(clean, g_ram + orc, n) != 0) {
        size_t i;
        const unsigned char *c = clean;
        for (i = 0; i < n && c[i] == g_ram[orc + i]; i++)
            ;
        if (g_fail < 20)
            printf("  DIVERGE %s at byte %lu of %lu: clean %02x oracle %02x\n",
                   what, (unsigned long)i, (unsigned long)n, c[i], g_ram[orc + i]);
        g_fail++;
    }
}

static void cmp_word(const char *what, uint32_t clean, uint32_t orc)
{
    g_cases++;
    if (clean != orc) {
        if (g_fail < 20)
            printf("  DIVERGE %s: clean %08x oracle %08x\n", what, clean, orc);
        g_fail++;
    }
}

static void call(arm_ctx *ctx, void (*fn)(arm_ctx *))
{
    fn(ctx);
}

/* ------------------------------------------------------------- the pose */

#define MAXB 60

static void test_pose(void)
{
    static BONE      h_bones[MAXB];
    static BONESINFO h_info;
    static unsigned char h_data[3 * (0x10 + MAXB * 20)];
    long nb = 1 + (long)(nextu() % (MAXB - 1));
    long stride = 0x10 + nb * 20;
    long fa = (long)(nextu() % 3), fb = (long)(nextu() % 3);
    float t = nextf(0.0f, 1.0f);
    uint32_t g_bones = G_BASE, g_info = G_BASE + 0x4000, g_data = G_BASE + 0x5000;
    long i, k;
    arm_ctx ctx;

    memset(h_bones, 0, sizeof(h_bones));
    /* a tree that reaches every bone: bone i>0 hangs off an earlier one */
    for (i = 0; i < nb; i++) {
        h_bones[i].x = nextf(-3.0f, 3.0f);
        h_bones[i].y = nextf(-3.0f, 3.0f);
        h_bones[i].z = nextf(-3.0f, 3.0f);
    }
    for (i = 1; i < nb; i++) {
        long p = (long)(nextu() % (unsigned long)i);
        if (h_bones[p].numChildren < 9) {
            /* leave the occasional NULL slot in the middle */
            if ((nextu() & 7) == 0 && h_bones[p].numChildren < 8)
                h_bones[p].children[h_bones[p].numChildren++] = NULL;
            h_bones[p].children[h_bones[p].numChildren++] = &h_bones[i];
        } else {
            nb = i;                     /* stop: keep every bone reachable */
            break;
        }
    }
    stride = 0x10 + nb * 20;
    for (i = 0; i < 3 * stride; i++)
        h_data[i] = 0;
    for (i = 0; i < 3; i++) {
        float *f = (float *)(h_data + i * stride);
        for (k = 1; k < 4 + nb * 5; k++)
            f[k] = nextf(-1.0f, 1.0f);
    }
    h_info.bones = &h_bones[0];
    h_info.numBones = (int)nb;

    /* mirror into the oracle's memory */
    for (i = 0; i < nb; i++) {
        uint32_t b = g_bones + (uint32_t)i * 0x38u;
        MEM_ST32(b + 0x00, (uint32_t)h_bones[i].numChildren);
        MEM_ST32(b + 0x04, F32_U32(h_bones[i].x));
        MEM_ST32(b + 0x08, F32_U32(h_bones[i].y));
        MEM_ST32(b + 0x0c, F32_U32(h_bones[i].z));
        MEM_ST32(b + 0x10, 0);
        for (k = 0; k < 9; k++) {
            BONE *c = h_bones[i].children[k];
            MEM_ST32(b + 0x14 + 4u * (uint32_t)k,
                     c ? g_bones + (uint32_t)(c - h_bones) * 0x38u : 0u);
        }
    }
    MEM_ST32(g_info + 0, g_bones);
    MEM_ST32(g_info + 4, (uint32_t)nb);
    memcpy(g_ram + g_data, h_data, (size_t)(3 * stride));

    memset(MatrixPalette2, 0, sizeof(MatrixPalette2));
    memset(g_ram + O_PALETTE, 0, sizeof(MatrixPalette2));

    GenerateMatrices((char *)h_data, &h_info, fa, fb, t, stride);

    memset(&ctx, 0, sizeof(ctx));
    ctx.r[SP] = STACK_TOP;
    ctx.r[0] = g_data; ctx.r[1] = g_info; ctx.r[2] = (uint32_t)fa; ctx.r[3] = (uint32_t)fb;
    MEM_ST32(STACK_TOP + 0, F32_U32(t));
    MEM_ST32(STACK_TOP + 4, (uint32_t)stride);
    call(&ctx, func_00060358_Z16GenerateMatricesPcP9BONESINFOllfl);

    cmp_bytes("MatrixPalette2", MatrixPalette2, O_PALETTE, (size_t)nb * 48);
    cmp_bytes("Root_Trans0", &Root_Trans0, O_ROOT0, 12);
    cmp_bytes("DecompAnimFrames0", DecompAnimFrames0, O_FRAMES0, (size_t)nb * 20);
    cmp_word("MatrixSourceCount", (uint32_t)MatrixSourceCount, MEM_LD32(O_COUNT));
    cmp_word("MatrixDst2 offset", (uint32_t)((char *)MatrixDst2 - (char *)MatrixPalette2),
             MEM_LD32(O_DST2) - O_PALETTE);
    cmp_word("MatrixSource0 offset",
             (uint32_t)((char *)MatrixSource0 - (char *)DecompAnimFrames0),
             MEM_LD32(O_SRC0) - O_FRAMES0);
}

/* ------------------------------------------------------------- the skin */

#define MAXV 48
#define MAXF 40

static void test_skin(long keepFloats, long uvCountZero)
{
    static SKININFO h_skin;
    static float    h_A[MAXV * 12], h_B[MAXV * 12], h_w[MAXV * 4];
    static unsigned char h_idx[MAXV * 4];
    static uint16_t h_tri[MAXF * 3];
    static float    h_uv[MAXF * 6];
    static unsigned char h_pos[5000 * 12];
    static float    h_outuv[5000 * 2];
    static unsigned char h_col[5000];
    static const float uvset[4] = { 0.0f, 0.25f, 0.5f, 0.75f };
    long nv = 1 + (long)(nextu() % MAXV);
    long nf = 1 + (long)(nextu() % MAXF);
    long uvCount = uvCountZero ? 0 : nv + 3 * nf;
    long clean, i, k;
    uint32_t gS = G_BASE + 0x10000, gA = gS + 0x100, gB = gA + MAXV * 48,
             gW = gB + MAXV * 48, gI = gW + MAXV * 16, gT = gI + MAXV * 4,
             gU = gT + MAXF * 6 + 2, gP = gU + MAXF * 24 + 8,
             gO = gP + 5000 * 12, gC = gO + 5000 * 8;
    arm_ctx ctx;

    gU = (gU + 3u) & ~3u;
    gP = (gP + 3u) & ~3u;

    for (i = 0; i < nv * 12; i++) { h_A[i] = nextf(-2.0f, 2.0f); h_B[i] = nextf(-1.0f, 1.0f); }
    for (i = 0; i < nv * 4; i++) {
        h_w[i] = nextf(0.0f, 1.0f);
        h_idx[i] = (nextu() % 5 == 0) ? 0xFF : (unsigned char)(nextu() % 150);
    }
    for (i = 0; i < nf * 3; i++) {
        h_tri[i] = (uint16_t)(nextu() % (unsigned long)nv);
        h_uv[i * 2 + 0] = uvset[nextu() & 3];
        h_uv[i * 2 + 1] = uvset[nextu() & 3];
    }
    for (i = 0; i < 150 * 12; i++)
        ((float *)MatrixPalette2)[i] = nextf(-1.5f, 1.5f);

    memset(&h_skin, 0, sizeof(h_skin));
    h_skin.numMatrices = (int)nv;
    h_skin.numVerts    = (int)nf;
    h_skin.matricesA   = (SKINMATRIX43 *)h_A;
    h_skin.matricesB   = (SKINMATRIX43 *)h_B;
    h_skin.indexes     = (int32_t *)h_idx;
    h_skin.weights     = h_w;
    h_skin.vertExtra   = h_tri;
    h_skin.uvs         = h_uv;

    memset(h_pos, 0, sizeof(h_pos));
    memset(h_outuv, 0x55, sizeof(h_outuv));
    memset(h_col, 0, sizeof(h_col));
    memset(SkinnedVerts, 0, 24 * MAXV);
    memset(RenderIndexes, 0, sizeof(RenderIndexes));

    /* oracle memory */
    MEM_ST32(gS + 0x00, 0);
    MEM_ST32(gS + 0x04, (uint32_t)nv);
    MEM_ST32(gS + 0x08, (uint32_t)nf);
    MEM_ST32(gS + 0x14, gA);
    MEM_ST32(gS + 0x18, gT);
    MEM_ST32(gS + 0x1c, gU);
    MEM_ST32(gS + 0x20, gI);
    MEM_ST32(gS + 0x24, gW);
    MEM_ST32(gS + 0x28, gB);
    memcpy(g_ram + gA, h_A, (size_t)nv * 48);
    memcpy(g_ram + gB, h_B, (size_t)nv * 48);
    memcpy(g_ram + gW, h_w, (size_t)nv * 16);
    memcpy(g_ram + gI, h_idx, (size_t)nv * 4);
    memcpy(g_ram + gT, h_tri, (size_t)nf * 6);
    memcpy(g_ram + gU, h_uv, (size_t)nf * 24);
    memcpy(g_ram + O_PALETTE, MatrixPalette2, sizeof(MatrixPalette2));
    memset(g_ram + gP, 0, 5000 * 12);
    memset(g_ram + gO, 0x55, 5000 * 8);
    memset(g_ram + gC, 0, 5000);
    memset(g_ram + O_SKINNED, 0, 24 * MAXV);
    memset(g_ram + O_RINDEX, 0, sizeof(RenderIndexes));

    clean = DrawSkinnedMesh2(&h_skin, 0, 0, 0, (limeVECTOR3 *)h_pos,
                             (limeVECTOR2 *)h_outuv, h_col, uvCount, keepFloats);

    memset(&ctx, 0, sizeof(ctx));
    ctx.r[SP] = STACK_TOP;
    ctx.r[0] = gS; ctx.r[1] = 0; ctx.r[2] = 0; ctx.r[3] = 0;
    MEM_ST32(STACK_TOP + 0x00, gP);
    MEM_ST32(STACK_TOP + 0x04, gO);
    MEM_ST32(STACK_TOP + 0x08, gC);
    MEM_ST32(STACK_TOP + 0x0c, (uint32_t)uvCount);
    MEM_ST32(STACK_TOP + 0x10, (uint32_t)keepFloats);
    call(&ctx, func_000608d8_Z16DrawSkinnedMesh2P8SKININFOjjlP11limeVECTOR3P11limeVECTOR2Phll);

    cmp_word("DrawSkinnedMesh2 return", (uint32_t)clean, ctx.r[0]);
    cmp_bytes("SkinnedVerts", SkinnedVerts, O_SKINNED,
              (size_t)nv * (keepFloats ? 24 : 6));
    cmp_bytes("outPos", h_pos, gP, (size_t)(nv + 3 * nf) * (keepFloats ? 12 : 6));
    cmp_bytes("outUV", h_outuv, gO, (size_t)(uvCountZero ? 5000 : nv + 3 * nf) * 8);
    cmp_bytes("RenderIndexes", RenderIndexes, O_RINDEX, (size_t)nf * 6);
    (void)k;
}

int main(void)
{
    const char *slice = getenv("UMK3_SLICE");
    long i;

    setvbuf(stdout, NULL, _IONBF, 0);
    arm_mem_init(RAM_SIZE);
    if (slice == NULL || arm_load_image(slice) != 0) {
        printf("set UMK3_SLICE to the armv7 slice\n");
        return 2;
    }

    printf("=== armv7 skinning chain vs the recompiled original ===\n\n");

    for (i = 0; i < 3000; i++) test_pose();
    for (i = 0; i < 2000; i++) test_skin(0, 0);
    for (i = 0; i < 1000; i++) test_skin(1, 0);
    for (i = 0; i < 300;  i++) test_skin(0, 1);

    printf("\ncases compared: %ld    divergences: %d\n", g_cases, g_fail);
    printf("%s\n", g_fail ? "RESULT: FAIL" : "RESULT: PASS");
    return g_fail ? 1 : 0;
}
