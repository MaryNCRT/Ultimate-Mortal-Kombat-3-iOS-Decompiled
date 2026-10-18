/*
 * test_limeds_camera_gl_diff.c -- LIMEDS_Set3dMode and
 * LIMEDS_SetCameraOrientation against the recompiled original, by their GL
 * call stream and the globals they write.
 *
 * ## Why this exists
 *
 * Both functions were rewritten to make the main menu's vortex draw (PR #40)
 * and both were marked draft: the fix was checked on screen, not against the
 * binary. They are pure matrix work handed to GL, which is exactly what
 * tests/gl_trace.c compares best -- every glMultMatrixf matrix bit for bit.
 *
 * ## What the cases are chosen against
 *
 * LIMEDS_Set3dMode reads six globals. Each one that changes the output is
 * swept: sideways or not (which picks limeScreenWidth or limeScreenHeight for
 * the scale), the screen and device sizes (the scale is their ratio), and an
 * arbitrary limeSidewaysMat (it is multiplied in last, so a body that skipped
 * or reordered it differs). It also writes `ratio` and limePerspectiveMatrix,
 * which are compared directly.
 *
 * LIMEDS_SetCameraOrientation is gluLookAt by hand. The ways it can be wrong
 * are the rows of the basis and the order of the normalisations, so:
 *  - random cameras, where the true up differs from the given up -- a body
 *    that used the given up instead of F x side agrees only when they match;
 *  - an up vector of length != 1, because the true up is computed from the
 *    side BEFORE the side is normalised, and that order shows only then;
 *  - degenerate input: eye == centre (F = 0), up parallel to F (side = 0),
 *    up = 0 -- the two `len != 0` guards;
 *  - axis-aligned cameras, where a transposed matrix still looks plausible.
 *
 * ## What a pass proves, and what it does not
 *
 * The port-only hook limePortDisplayRotation (runtime/draw_gl.c) is not in
 * the binary, so it is a no-op here: this compares everything the original
 * does and nothing the port adds.
 *
 * Checked the other way too: built against the pre-fix LIMEDS_Misc.c it
 * diverges on 7,011 of its 7,012 cases, so a pass is not the test being blind.
 *
 * ## Build
 *
 *   python tools/armrecomp/recomp.py work/UMK3.armv7 --file LIMEDS_Misc.cpp
 *       --out recompiled --name limedsmisc --with-deps
 *   gcc -std=gnu11 -O1 -I runtime -I tests -I recompiled
 *       tests/test_limeds_camera_gl_diff.c tests/gl_trace.c
 *       decomp/lime/LIMEDS_Misc.c decomp/lime/Matrix.c recompiled/limedsmisc.c
 *       runtime/arm_runtime.c recompiled/limedsmisc_shims.c
 *       -Wl,--allow-multiple-definition -lm -o build/test_limeds_camera_gl_diff
 *   UMK3_SLICE=work/UMK3.armv7 build/test_limeds_camera_gl_diff
 */

#include "arm_runtime.h"
#include "gl_trace.h"
#include "limedsmisc.h"                 /* oracle: tools/armrecomp */
#include "../src/decomp/lime/lime.h"        /* clean C */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RAM_SIZE   (8u << 20)
#define STACK_TOP  0x007F0000u

/* the globals, at their addresses in the armv7 slice (work/symbols.txt) */
#define G_RATIO        0x0017146cu
#define G_SIDEWAYS     0x00171ad8u
#define G_DEV_HEIGHT   0x00171ae8u
#define G_SCREEN_W     0x00171aecu
#define G_SCREEN_H     0x00171af0u
#define G_PERSPECTIVE  0x00391f8cu
#define G_SIDEWAYS_MAT 0x00391fd4u

/* the clean side's copies */
float ratio;
float limePerspectiveMatrix[16];
float limeSidewaysMat[16];
int   limeDeviceSideways;
int   limeDeviceHeight;
int   limeScreenWidth;
int   limeScreenHeight;

void limePortDisplayRotation(void) { }  /* port only; see the header */

void LIMEDS_Set3dMode(void);
void LIMEDS_SetCameraOrientation(float eyeX, float eyeY, float eyeZ,
                                 float centerX, float centerY, float centerZ,
                                 float upX, float upY, float upZ);

/* the one import gl_trace.c does not record yet */
void stub_auto_glLoadIdentity(arm_ctx *ctx) { (void)ctx; glLoadIdentity(); }

static long g_cases, g_fail;
static int  g_ptr_only;

static uint32_t g_seed = 0x2468ace1u;
static uint32_t nextu(void) { g_seed = g_seed * 1664525u + 1013904223u; return g_seed; }
static float    nextf(float lo, float hi) { return lo + (hi - lo) * (float)(nextu() >> 8) / 16777216.0f; }
static uint32_t f2u(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }

static void ctx_reset(arm_ctx *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->r[SP] = STACK_TOP;
    ctx->r[LR] = 0;
}

/* ---------------------------------------------------------- Set3dMode */

static void set3d_case(const char *what, int sideways, int sw, int sh, int dh,
                       const float *sidemat)
{
    arm_ctx ctx;
    int i, n, bad = 0;

    limeDeviceSideways = sideways;
    limeScreenWidth = sw; limeScreenHeight = sh; limeDeviceHeight = dh;
    memcpy(limeSidewaysMat, sidemat, sizeof limeSidewaysMat);
    memset(limePerspectiveMatrix, 0, sizeof limePerspectiveMatrix);
    ratio = 0.0f;

    MEM_ST32(G_SIDEWAYS, (uint32_t)sideways);
    MEM_ST32(G_SCREEN_W, (uint32_t)sw);
    MEM_ST32(G_SCREEN_H, (uint32_t)sh);
    MEM_ST32(G_DEV_HEIGHT, (uint32_t)dh);
    MEM_ST32(G_RATIO, 0u);
    for (i = 0; i < 16; i++) {
        MEM_ST32(G_SIDEWAYS_MAT + 4u * (uint32_t)i, f2u(sidemat[i]));
        MEM_ST32(G_PERSPECTIVE + 4u * (uint32_t)i, 0u);
    }

    glt_reset();
    glt_select(&glt_clean);
    LIMEDS_Set3dMode();
    glt_select(&glt_oracle);
    ctx_reset(&ctx);
    func_0005d944_LIMEDS_Set3dMode(&ctx);

    n = glt_compare(what, &g_ptr_only);
    for (i = 0; i < 16; i++)
        if (f2u(limePerspectiveMatrix[i]) != MEM_LD32(G_PERSPECTIVE + 4u * (uint32_t)i)) {
            printf("  %s: limePerspectiveMatrix[%d] clean %08x oracle %08x\n", what, i,
                   f2u(limePerspectiveMatrix[i]), MEM_LD32(G_PERSPECTIVE + 4u * (uint32_t)i));
            bad++;
        }
    if (f2u(ratio) != MEM_LD32(G_RATIO)) {
        printf("  %s: ratio clean %08x oracle %08x\n", what, f2u(ratio), MEM_LD32(G_RATIO));
        bad++;
    }
    g_cases++;
    if (n || bad) g_fail++;
}

/* -------------------------------------------------- SetCameraOrientation */

static void camera_case(const char *what, const float a[9])
{
    arm_ctx ctx;
    int i, n;

    glt_reset();
    glt_select(&glt_clean);
    LIMEDS_SetCameraOrientation(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8]);

    glt_select(&glt_oracle);
    ctx_reset(&ctx);
    /* AAPCS soft-float: four words in r0-r3, the other five on the stack */
    for (i = 0; i < 4; i++) ctx.r[i] = f2u(a[i]);
    for (i = 4; i < 9; i++) MEM_ST32(STACK_TOP + 4u * (uint32_t)(i - 4), f2u(a[i]));
    func_0005d798_LIMEDS_SetCameraOrientation(&ctx);

    n = glt_compare(what, &g_ptr_only);
    g_cases++;
    if (n) {
        g_fail++;
        if (g_fail < 4) {
            glt_dump(&glt_clean, "clean", 8);
            glt_dump(&glt_oracle, "oracle", 8);
        }
    }
}

int main(void)
{
    const char *slice = getenv("UMK3_SLICE");
    static const float ident[16] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
    float sm[16], a[9];
    int i, k;

    setvbuf(stdout, NULL, _IONBF, 0);
    arm_mem_init(RAM_SIZE);
    if (!slice) slice = "work/UMK3.armv7";
    if (arm_load_image(slice) != 0) {
        fprintf(stderr, "cannot load the armv7 slice %s (set UMK3_SLICE)\n", slice);
        return 2;
    }

    printf("=== LIMEDS_Set3dMode / LIMEDS_SetCameraOrientation vs the oracle ===\n\n");

    /* ---- Set3dMode ---- */
    {
        /* RotMatrixZ(-pi/2), what limeBegin writes when sideways */
        static const float rotz[16] = { 0,-1,0,0, 1,0,0,0, 0,0,1,0, 0,0,0,1 };
        set3d_case("3d: upright, identity, 320x480/480", 0, 320, 480, 480, ident);
        set3d_case("3d: sideways, rotZ, 480x320/480", 1, 480, 320, 480, rotz);
        set3d_case("3d: sideways, rotZ, 960x640/480", 1, 960, 640, 480, rotz);
        set3d_case("3d: upright, rotZ, 768x1024/1024", 0, 768, 1024, 1024, rotz);
        for (k = 0; k < 2000; k++) {
            for (i = 0; i < 16; i++) sm[i] = nextf(-2.0f, 2.0f);
            set3d_case("3d: random", (int)(nextu() & 1),
                       64 + (int)(nextu() % 2048), 64 + (int)(nextu() % 2048),
                       64 + (int)(nextu() % 2048), sm);
        }
    }

    /* ---- SetCameraOrientation ---- */
    {
        static const float fixed[][9] = {
            { 0,0,0,   0,0,-1,  0,1,0 },        /* the GL default view */
            { 0,0,5,   0,0,0,   0,1,0 },
            { 1,2,3,   4,5,6,   0,0,1 },
            { 0,0,0,   0,0,0,   0,1,0 },        /* eye == centre: F = 0 */
            { 0,0,0,   0,5,0,   0,1,0 },        /* up parallel to F: side = 0 */
            { 0,0,0,   1,0,0,   0,0,0 },        /* up = 0 */
            { 0,10,20, 0,0,0,   0,3,0 },        /* up not unit length */
            { 7,0,0,   0,0,0,   0,0,-2 },
        };
        for (k = 0; k < (int)(sizeof fixed / sizeof fixed[0]); k++)
            camera_case("cam: fixed", fixed[k]);
        for (k = 0; k < 5000; k++) {
            for (i = 0; i < 9; i++) a[i] = nextf(-500.0f, 500.0f);
            if (k & 1) { a[6] *= 0.001f; a[8] *= 0.001f; }  /* up mostly +y-ish or small */
            camera_case("cam: random", a);
        }
    }

    printf("\n%ld cases, %ld diverged; %d pointer arguments checked for nullness only\n",
           g_cases, g_fail, g_ptr_only);
    return g_fail ? 1 : 0;
}
