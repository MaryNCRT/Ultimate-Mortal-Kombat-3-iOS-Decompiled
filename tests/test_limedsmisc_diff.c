/*
 * test_limedsmisc_diff.c — clean LIMEDS_Misc.c against the oracle.
 *
 * The three matrix conversions the renderers depend on:
 *
 *   ConvertDSMatrixtoPCMatrix    int32 x 1/4096   -> a row-major float 4x4
 *   ConvertQSTMatrixtoPCMatrix   quaternion+S+T   -> a GL-ready float 4x4
 *   LerpQSTMatrix                two QSTs blended -- the quaternion re-quantised
 *                                to int16, the six floats left as floats
 *
 * ## Why this test exists
 *
 * `ConvertQSTMatrixtoPCMatrix` was written from a single reading of the
 * disassembly and has never been run. It is called for every transparent mesh
 * in every frame, and its failure mode is not a crash: a wrong sign or a
 * transposed pair produces geometry that is *nearly* right, which is the kind
 * of wrong nobody finds by looking.
 *
 * ## What each sweep is chosen to catch
 *
 *  - **The DS conversion** gets values spanning the full int32 range, not just
 *    small ones. The scale is 1/4096, so a plausible-looking wrong shift shows
 *    up only at magnitudes where the exponent moves.
 *
 *  - **The QST conversion** gets unit AND non-unit quaternions, negative scales,
 *    and a zero scale. Row-scaling and column-scaling agree on a uniform scale
 *    and disagree the moment the three axes differ, so every case here uses
 *    three different values.
 *
 *  - **LerpQSTMatrix** is swept across `t` including both endpoints, and its
 *    two halves are compared separately. It re-quantises the QUATERNION to
 *    int16 and leaves scale and translation in float, so comparing the whole
 *    struct as one type would hide a mistake in either half.
 *
 * Exact bit equality throughout, and it earned its keep three times on the
 * first run: ConvertDSMatrixtoPCMatrix was writing zero at m[15] instead of
 * 1.0f, the QST scale constant was 1/32767 where the binary holds
 * 3.0518509447574615e-05, and LerpQSTMatrix was modelling all 32 bytes as
 * int16. None of the three is visible by reading.
 */

#include "arm_runtime.h"
#include "limedsmisc.h"                 /* oracle: tools/armrecomp */
#include "../src/decomp/lime/lime.h"        /* clean C */

#include <math.h>
#include <stdio.h>
#include <string.h>

#define RAM_SIZE   (1u << 20)
#define STACK_TOP  0x000F0000u
#define G_SRC      0x00001000u
#define G_DST      0x00001200u
#define G_B        0x00001400u

static int  g_fail  = 0;
static long g_cases = 0;

static void ctx_reset(arm_ctx *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->r[SP] = STACK_TOP;
}

static int same_bits(float a, float b)
{
    if (a != a && b != b) return 1;
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

static void cmp_i16(const char *what, const int16_t *clean, const int16_t *orc, int n)
{
    g_cases++;
    for (int i = 0; i < n; i++) {
        if (clean[i] != orc[i]) {
            printf("  DIVERGE %s  element %d: clean=%d  oracle=%d\n",
                   what, i, clean[i], orc[i]);
            g_fail++;
            return;
        }
    }
}

/* Deterministic, so a divergence someone reports can be reproduced. */
static uint32_t g_seed = 0x0BADC0DEu;

static uint32_t nextu(void)
{
    g_seed = g_seed * 1664525u + 1013904223u;
    return g_seed;
}

static int32_t next_i32(int32_t lo, int32_t hi)
{
    return lo + (int32_t)(nextu() % (uint32_t)(hi - lo + 1));
}

static float nextf(float lo, float hi)
{
    float t = (float)((nextu() >> 8) & 0xFFFFFF) / (float)0xFFFFFF;
    return lo + t * (hi - lo);
}


/* --------------------------------------------------- ConvertDSMatrixtoPCMatrix */

static void test_ds(int32_t lo, int32_t hi)
{
    arm_ctx ctx;
    int32_t src[12];
    float clean[16], orc[16];

    for (int i = 0; i < 12; i++) src[i] = next_i32(lo, hi);

    memset(clean, 0xCD, sizeof(clean));
    ConvertDSMatrixtoPCMatrix(src, clean);

    ctx_reset(&ctx);
    for (int i = 0; i < 12; i++) MEM_ST32(G_SRC + 4u * (uint32_t)i, (uint32_t)src[i]);
    for (int i = 0; i < 16; i++) MEM_ST32(G_DST + 4u * (uint32_t)i, 0xCDCDCDCDu);
    ctx.r[0] = G_SRC; ctx.r[1] = G_DST;
    func_0005d38c_ConvertDSMatrixtoPCMatrix(&ctx);
    for (int i = 0; i < 16; i++) orc[i] = U32_F32(MEM_LD32(G_DST + 4u * (uint32_t)i));

    cmp_f("ConvertDSMatrixtoPCMatrix", clean, orc, 16);
}


/* -------------------------------------------------- ConvertQSTMatrixtoPCMatrix */

static void fill_qst(QSTMATRIX *q, int unit, int zero_scale)
{
    float x = nextf(-1.0f, 1.0f), y = nextf(-1.0f, 1.0f);
    float z = nextf(-1.0f, 1.0f), w = nextf(-1.0f, 1.0f);

    if (unit) {
        float n = sqrtf(x*x + y*y + z*z + w*w);
        if (n > 1e-6f) { x/=n; y/=n; z/=n; w/=n; }
    }

    q->q[0] = (int16_t)(x * 32767.0f);
    q->q[1] = (int16_t)(y * 32767.0f);
    q->q[2] = (int16_t)(z * 32767.0f);
    q->q[3] = (int16_t)(w * 32767.0f);

    /* three DIFFERENT axes every time: a uniform scale cannot tell row-scaling
     * from column-scaling, and that is exactly the mistake worth catching */
    q->scale[0] = zero_scale ? 0.0f : nextf(-3.0f, 3.0f);
    q->scale[1] = nextf(0.1f, 4.0f);
    q->scale[2] = nextf(-4.0f, -0.1f);

    q->translation[0] = nextf(-500.0f, 500.0f);
    q->translation[1] = nextf(-500.0f, 500.0f);
    q->translation[2] = nextf(-500.0f, 500.0f);
}

static void test_qst(int unit, int zero_scale)
{
    arm_ctx ctx;
    QSTMATRIX q;
    float clean[16], orc[16];

    fill_qst(&q, unit, zero_scale);

    memset(clean, 0xCD, sizeof(clean));
    ConvertQSTMatrixtoPCMatrix(&q, clean);

    ctx_reset(&ctx);
    for (unsigned i = 0; i < sizeof(q); i += 4u)
        MEM_ST32(G_SRC + i, *(const uint32_t *)((const char *)&q + i));
    for (int i = 0; i < 16; i++) MEM_ST32(G_DST + 4u * (uint32_t)i, 0xCDCDCDCDu);
    ctx.r[0] = G_SRC; ctx.r[1] = G_DST;
    func_0005d5d4_ConvertQSTMatrixtoPCMatrix(&ctx);
    for (int i = 0; i < 16; i++) orc[i] = U32_F32(MEM_LD32(G_DST + 4u * (uint32_t)i));

    cmp_f(unit ? "ConvertQST(unit)" : "ConvertQST(non-unit)", clean, orc, 16);
}


/* ---------------------------------------------------------------- LerpQSTMatrix */

static void test_lerp(float t)
{
    arm_ctx ctx;
    QSTMATRIX a, b, clean, orc;

    /* a whole QSTMATRIX: four int16 and six floats, blended differently */
    fill_qst(&a, 0, 0);
    fill_qst(&b, 0, 0);

    memset(&clean, 0xCD, sizeof(clean));
    LerpQSTMatrix(&a, &b, t, &clean);

    ctx_reset(&ctx);
    for (unsigned i = 0; i < sizeof(a); i += 4u) {
        MEM_ST32(G_SRC + i, *(const uint32_t *)((const char *)&a + i));
        MEM_ST32(G_B   + i, *(const uint32_t *)((const char *)&b + i));
        MEM_ST32(G_DST + i, 0xCDCDCDCDu);
    }
    ctx.r[0] = G_SRC; ctx.r[1] = G_B; ctx.r[2] = F32_U32(t); ctx.r[3] = G_DST;
    func_0005d47c_LerpQSTMatrix(&ctx);
    for (unsigned i = 0; i < sizeof(orc); i += 4u)
        *(uint32_t *)((char *)&orc + i) = MEM_LD32(G_DST + i);

    /* the quaternion is quantised, the rest is not -- compare both halves */
    cmp_i16("LerpQSTMatrix.q", clean.q, orc.q, 4);
    cmp_f("LerpQSTMatrix.scale", clean.scale, orc.scale, 3);
    cmp_f("LerpQSTMatrix.translation", clean.translation, orc.translation, 3);
}


int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    arm_mem_init(RAM_SIZE);

    printf("=== clean LIMEDS_Misc.c vs the recompiled original ===\n\n");

    /* small values, then the full int32 range: 1/4096 hides a wrong shift at
     * small magnitudes and exposes it once the exponent has to move */
    for (int i = 0; i < 3000; i++) test_ds(-4096, 4096);
    for (int i = 0; i < 3000; i++) test_ds(-(1 << 30), (1 << 30));

    for (int i = 0; i < 3000; i++) test_qst(1, 0);
    for (int i = 0; i < 3000; i++) test_qst(0, 0);
    for (int i = 0; i < 500;  i++) test_qst(1, 1);   /* a zero axis */

    for (int i = 0; i <= 20; i++) {
        float t = (float)i / 20.0f;                 /* both endpoints included */
        for (int k = 0; k < 150; k++) test_lerp(t);
    }

    printf("\ncases compared: %ld    divergences: %d\n", g_cases, g_fail);
    printf("%s\n", g_fail ? "RESULT: FAIL"
                          : "RESULT: the clean matrix conversions match the original");

    arm_mem_free();
    return g_fail ? 1 : 0;
}
