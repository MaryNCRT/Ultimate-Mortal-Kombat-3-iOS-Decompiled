/*
 * stubs_extra.c -- the few imports the recompiled gamecode calls that the
 * generated shims leave aborting. Linked FIRST, so these definitions win over
 * the aborting ones in the generated recompiled_shims.c files.
 */
#include <math.h>
#include <stdint.h>
#include "arm_runtime.h"

/* signed 32-bit division, the compiler's __divsi3: r0 / r1 -> r0 */
void stub_auto_divsi3(arm_ctx *ctx)
{
    int32_t a = (int32_t)ctx->r[0], b = (int32_t)ctx->r[1];
    ctx->r[0] = b ? (uint32_t)(a / b) : 0u;
}

void stub_auto_fflush(arm_ctx *ctx)
{
    ctx->r[0] = 0;
}

static uint32_t g_rand = 12345u;

/* the harness reseeds before each side runs, so both see the same sequence */
void stubs_reseed(uint32_t seed)
{
    g_rand = seed;
}

void stub_auto_rand(arm_ctx *ctx)
{
    g_rand = g_rand * 1103515245u + 12345u;
    ctx->r[0] = (g_rand >> 16) & 0x7fff;
}

/* the C side's rand(): the same generator, so both sides draw alike */
int rand(void)
{
    g_rand = g_rand * 1103515245u + 12345u;
    return (int)((g_rand >> 16) & 0x7fff);
}

/* atan2(double y, double x): y in r0:r1, x in r2:r3, result in r0:r1 */
void stub_auto_atan2(arm_ctx *ctx)
{
    union { double d; uint32_t w[2]; } y, x, r;
    y.w[0] = ctx->r[0]; y.w[1] = ctx->r[1];
    x.w[0] = ctx->r[2]; x.w[1] = ctx->r[3];
    r.d = atan2(y.d, x.d);
    ctx->r[0] = r.w[0]; ctx->r[1] = r.w[1];
}
