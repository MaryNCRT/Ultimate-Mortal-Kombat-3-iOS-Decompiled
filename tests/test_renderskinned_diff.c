/*
 * test_renderskinned_diff.c — clean RenderSkinned.c against the oracle.
 *
 * The skinning maths: the code that turns an animation frame plus a skeleton
 * into a posed character. Every function covered here is pure arithmetic over
 * memory the caller owns, which is why it can be verified at all — no function
 * pointers, no indirect branches, and the recompiler translates all of it with
 * zero unsupported instructions.
 *
 * It is also the code with the most to lose from being subtly wrong. A loader
 * that is off produces a visible mess; a pose that is off produces a character
 * that looks almost right, and nobody finds it by looking.
 *
 * ## What is swept, and why those inputs
 *
 *  - `MatrixIdentity2` — trivial, but it pins the stride-3 layout that
 *    everything else assumes.
 *  - `MatrixMul2` — random matrices, INCLUDING the translation row. The
 *    rotation part would agree under a plain 4x3 multiply; only the fourth row
 *    tells the two implementations apart, so it is the row that matters.
 *  - `GetMFromQuat2` — unit quaternions AND deliberately non-unit ones. The
 *    engine does not renormalise (see GetSlerpedQ), so non-unit input is a real
 *    case rather than an abusive one.
 *  - `GetSlerpedQ` — pairs chosen to straddle the hemisphere flip: a positive
 *    dot, a negative dot, and pairs at exactly zero, which is the boundary the
 *    `vnegls` predicate includes and a `< 0` version would miss.
 *  - `Xform2` — a zero weight (the early return), and non-zero weights against
 *    a destination that is already non-zero, because the function ACCUMULATES
 *    and a version that assigns would pass every test starting from zero.
 *
 * ## Tolerance
 *
 * Exact bit equality everywhere. None of these call libm — the quaternion
 * expansion is multiply-add only — so there is no reason to allow a ULP, and
 * allowing one would hide exactly the kind of drift worth catching.
 */

#include "arm_runtime.h"
#include "renderskinned.h"              /* oracle: tools/armrecomp */
#include "../src/decomp/lime/lime.h"        /* clean C */

#include <math.h>
#include <stdio.h>
#include <string.h>

#define RAM_SIZE   (1u << 20)
#define STACK_TOP  0x000F0000u
#define G_A        0x00001000u
#define G_B        0x00001100u
#define G_C        0x00001200u
#define G_D        0x00001300u
#define G_E        0x00001400u

static int  g_fail  = 0;
static long g_cases = 0;

static void ctx_reset(arm_ctx *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->r[SP] = STACK_TOP;
}

static void put_f(uint32_t a, const float *v, int n)
{
    for (int i = 0; i < n; i++) MEM_ST32(a + 4u * (uint32_t)i, F32_U32(v[i]));
}

static void get_f(uint32_t a, float *v, int n)
{
    for (int i = 0; i < n; i++) v[i] = U32_F32(MEM_LD32(a + 4u * (uint32_t)i));
}

static int same_bits(float a, float b)
{
    if (a != a && b != b) return 1;             /* NaN == NaN, for this purpose */
    return memcmp(&a, &b, 4) == 0;
}

static void cmp_f(const char *what, const float *clean, const float *orc, int n)
{
    g_cases++;
    for (int i = 0; i < n; i++) {
        if (!same_bits(clean[i], orc[i])) {
            printf("  DIVERGE %s  element %d: clean=%.9g  oracle=%.9g\n",
                   what, i, clean[i], orc[i]);
            g_fail++;
            return;
        }
    }
}

/* A small deterministic generator. Deliberately not rand(): the same cases must
 * run on every machine, or a divergence someone reports cannot be reproduced. */
static uint32_t g_seed = 0x13579BDFu;

static float nextf(float lo, float hi)
{
    g_seed = g_seed * 1664525u + 1013904223u;
    float t = (float)((g_seed >> 8) & 0xFFFFFF) / (float)0xFFFFFF;
    return lo + t * (hi - lo);
}


/* ------------------------------------------------------------ MatrixIdentity2 */

static void test_identity(void)
{
    arm_ctx ctx;
    SKINMATRIX43 clean;
    float orc[12];

    for (int i = 0; i < 12; i++) {
        ((float *)&clean)[i] = nextf(-9.0f, 9.0f);
        orc[i] = ((float *)&clean)[i];
    }

    MatrixIdentity2(&clean);

    ctx_reset(&ctx);
    put_f(G_A, orc, 12);
    ctx.r[0] = G_A;
    func_0005ff6c_Z15MatrixIdentity2P12SKINMATRIX43(&ctx);
    get_f(G_A, orc, 12);

    cmp_f("MatrixIdentity2", (const float *)&clean, orc, 12);
}


/* ---------------------------------------------------------------- MatrixMul2 */

static void test_matrixmul(void)
{
    arm_ctx ctx;
    SKINMATRIX43 a, b, clean;
    float fa[12], fb[12], orc[12];

    for (int i = 0; i < 12; i++) {
        fa[i] = nextf(-3.0f, 3.0f);
        fb[i] = nextf(-3.0f, 3.0f);
    }
    memcpy(&a, fa, 48);
    memcpy(&b, fb, 48);
    memset(&clean, 0xCD, sizeof(clean));    /* poison: nothing may be inherited */

    MatrixMul2(&a, &b, &clean);

    ctx_reset(&ctx);
    put_f(G_A, fa, 12);
    put_f(G_B, fb, 12);
    for (int i = 0; i < 12; i++) MEM_ST32(G_C + 4u * (uint32_t)i, 0xCDCDCDCDu);
    ctx.r[0] = G_A; ctx.r[1] = G_B; ctx.r[2] = G_C;
    func_0005fd90_Z10MatrixMul2P12SKINMATRIX43S0_S0_(&ctx);
    get_f(G_C, orc, 12);

    cmp_f("MatrixMul2", (const float *)&clean, orc, 12);
}


/* ------------------------------------------------------------- GetMFromQuat2 */

static void test_quat(int unit)
{
    arm_ctx ctx;
    BONEANIMFRAME q;
    SKINMATRIX43 clean;
    float fq[5], orc[12];

    fq[0] = nextf(-1.0f, 1.0f);
    fq[1] = nextf(-1.0f, 1.0f);
    fq[2] = nextf(-1.0f, 1.0f);
    fq[3] = nextf(-1.0f, 1.0f);
    fq[4] = 0.0f;

    if (unit) {
        float n = sqrtf(fq[0]*fq[0] + fq[1]*fq[1] + fq[2]*fq[2] + fq[3]*fq[3]);
        if (n > 1e-6f) { fq[0]/=n; fq[1]/=n; fq[2]/=n; fq[3]/=n; }
    }

    memcpy(&q, fq, sizeof(fq));
    memset(&clean, 0xCD, sizeof(clean));

    GetMFromQuat2(&q, &clean);

    ctx_reset(&ctx);
    put_f(G_A, fq, 5);
    for (int i = 0; i < 12; i++) MEM_ST32(G_B + 4u * (uint32_t)i, 0xCDCDCDCDu);
    ctx.r[0] = G_A; ctx.r[1] = G_B;
    func_0005ff8c_Z13GetMFromQuat2P13BONEANIMFRAMEP12SKINMATRIX43(&ctx);
    get_f(G_B, orc, 12);

    cmp_f(unit ? "GetMFromQuat2(unit)" : "GetMFromQuat2(non-unit)",
          (const float *)&clean, orc, 12);
}


/* --------------------------------------------------------------- GetSlerpedQ */

static void test_slerp(float t, int force_dot)
{
    arm_ctx ctx;
    BONEANIMFRAME a, b, clean;
    float fa[5], fb[5], orc[5];

    for (int i = 0; i < 4; i++) {
        fa[i] = nextf(-1.0f, 1.0f);
        fb[i] = nextf(-1.0f, 1.0f);
    }
    fa[4] = fb[4] = 0.0f;

    if (force_dot < 0) {                /* make the dot product negative */
        for (int i = 0; i < 4; i++) fb[i] = -fa[i];
    } else if (force_dot == 0) {        /* and exactly zero: the vnegls boundary */
        fb[0] = -fa[1]; fb[1] = fa[0]; fb[2] = -fa[3]; fb[3] = fa[2];
    }

    memcpy(&a, fa, sizeof(fa));
    memcpy(&b, fb, sizeof(fb));
    memset(&clean, 0xCD, sizeof(clean));

    GetSlerpedQ(&a, &b, t, &clean);

    ctx_reset(&ctx);
    put_f(G_A, fa, 5);
    put_f(G_B, fb, 5);
    for (int i = 0; i < 5; i++) MEM_ST32(G_C + 4u * (uint32_t)i, 0xCDCDCDCDu);
    ctx.r[0] = G_A; ctx.r[1] = G_B; ctx.r[2] = F32_U32(t); ctx.r[3] = G_C;
    func_0006016c_Z11GetSlerpedQP13BONEANIMFRAMES0_fS0_(&ctx);
    get_f(G_C, orc, 5);

    cmp_f("GetSlerpedQ", (const float *)&clean, orc, 5);
}


/* -------------------------------------------------------------------- Xform2 */

static void test_xform(float w, int seed_dest)
{
    arm_ctx ctx;
    limeVECTOR3 vin, clean;
    SKINMATRIX43 m;
    float fv[3], fm[12], fd[3], orc[3];

    for (int i = 0; i < 3; i++)  fv[i] = nextf(-5.0f, 5.0f);
    for (int i = 0; i < 12; i++) fm[i] = nextf(-2.0f, 2.0f);

    /* A destination that is already non-zero. Xform2 ACCUMULATES, so a version
     * that assigned instead would agree with the original on every case that
     * started from zero -- which is why none of these do. */
    for (int i = 0; i < 3; i++) fd[i] = seed_dest ? nextf(-4.0f, 4.0f) : 0.0f;

    memcpy(&vin, fv, sizeof(fv));
    memcpy(&m, fm, sizeof(fm));
    memcpy(&clean, fd, sizeof(fd));

    Xform2(NULL, &vin, NULL, &clean, &m, w);

    ctx_reset(&ctx);
    put_f(G_A, fv, 3);
    put_f(G_B, fd, 3);
    put_f(G_C, fm, 12);
    ctx.r[0] = 0; ctx.r[1] = G_A; ctx.r[2] = 0; ctx.r[3] = G_B;
    /* args five and six travel on the stack under the soft-float ABI */
    MEM_ST32(ctx.r[SP] + 0u, G_C);
    MEM_ST32(ctx.r[SP] + 4u, F32_U32(w));
    func_0005fce4_Z6Xform2P11limeVECTOR3S0_S0_S0_P12SKINMATRIX43f(&ctx);
    get_f(G_B, orc, 3);

    cmp_f(w == 0.0f ? "Xform2(w=0)" : "Xform2", (const float *)&clean, orc, 3);
}


int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    arm_mem_init(RAM_SIZE);

    printf("=== clean RenderSkinned.c vs the recompiled original ===\n\n");

    for (int i = 0; i < 2000; i++) test_identity();
    for (int i = 0; i < 4000; i++) test_matrixmul();

    for (int i = 0; i < 3000; i++) test_quat(1);
    for (int i = 0; i < 3000; i++) test_quat(0);

    /* t across the range, and both sides of the hemisphere flip */
    for (int i = 0; i <= 20; i++) {
        float t = (float)i / 20.0f;
        for (int k = 0; k < 60; k++) {
            test_slerp(t, +1);
            test_slerp(t, -1);
            test_slerp(t,  0);      /* dot == 0: the boundary vnegls includes */
        }
    }

    for (int i = 0; i < 2000; i++) test_xform(nextf(0.05f, 1.0f), 1);
    for (int i = 0; i < 500;  i++) test_xform(0.0f, 1);   /* the early return */
    for (int i = 0; i < 500;  i++) test_xform(nextf(0.05f, 1.0f), 0);

    printf("\ncases compared: %ld    divergences: %d\n", g_cases, g_fail);
    printf("%s\n", g_fail ? "RESULT: FAIL"
                          : "RESULT: the clean skinning maths matches the original");

    arm_mem_free();
    return g_fail ? 1 : 0;
}
