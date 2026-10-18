/*
 * test_limevector_diff.c — comparacion diferencial: C limpio contra oraculo.
 *
 * Este es el test que cierra el bucle del proyecto. No comprueba que el C
 * decompilado "parezca" correcto: le mete las MISMAS entradas al codigo
 * recompilado (fiel por construccion) y al C limpio escrito a mano, y compara
 * las salidas. Si divergen, el C limpio esta mal.
 *
 * Se barre un rango amplio de entradas, incluyendo los casos raros: cero,
 * negativos, magnitudes muy grandes y muy pequenas.
 */

#include "arm_runtime.h"
#include "limevector.h"      /* oraculo: salida de tools/armrecomp */
#include "../src/decomp/lime/lime.h"   /* C limpio decompilado */

#include <math.h>
#include <stdio.h>
#include <string.h>

#define RAM_SIZE   (1u << 20)
#define STACK_TOP  0x000F0000u
#define VEC_A      0x00002000u

static int g_fail = 0;
static int g_cases = 0;

/* ---- lado del oraculo ---- */

static void oracle_setup(arm_ctx *ctx, float x, float y, float z)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->r[SP] = STACK_TOP;
    MEM_ST32(VEC_A + 0, F32_U32(x));
    MEM_ST32(VEC_A + 4, F32_U32(y));
    MEM_ST32(VEC_A + 8, F32_U32(z));
    ctx->r[0] = VEC_A;
}

static float oracle_len(float x, float y, float z)
{
    arm_ctx ctx;
    oracle_setup(&ctx, x, y, z);
    func_00061078_Len(&ctx);
    return U32_F32(ctx.r[0]);
}

static void oracle_normalise(float x, float y, float z, limeVECTOR3 *out)
{
    arm_ctx ctx;
    oracle_setup(&ctx, x, y, z);
    func_000610a4_Normalise(&ctx);
    out->x = U32_F32(MEM_LD32(VEC_A + 0));
    out->y = U32_F32(MEM_LD32(VEC_A + 4));
    out->z = U32_F32(MEM_LD32(VEC_A + 8));
}

/* ---- comparacion ---- */

static int same(float a, float b)
{
    if (a != a && b != b) return 1;            /* dos NaN cuentan como iguales */
    float d = a - b;
    if (d < 0) d = -d;
    float scale = fabsf(a) > fabsf(b) ? fabsf(a) : fabsf(b);
    return d <= (scale > 1.0f ? scale * 1e-6f : 1e-6f);
}

static void compare(float x, float y, float z)
{
    g_cases++;

    limeVECTOR3 v = { x, y, z };
    float clean = Len(&v);
    float oracle = oracle_len(x, y, z);
    if (!same(clean, oracle)) {
        printf("  DIVERGE Len(%g, %g, %g): limpio=%.9g  oraculo=%.9g\n",
               x, y, z, clean, oracle);
        g_fail++;
    }

    limeVECTOR3 c = { x, y, z }, o;
    Normalise(&c);
    oracle_normalise(x, y, z, &o);
    if (!same(c.x, o.x) || !same(c.y, o.y) || !same(c.z, o.z)) {
        printf("  DIVERGE Normalise(%g, %g, %g):\n", x, y, z);
        printf("      limpio  = (%.9g, %.9g, %.9g)\n", c.x, c.y, c.z);
        printf("      oraculo = (%.9g, %.9g, %.9g)\n", o.x, o.y, o.z);
        g_fail++;
    }
}

int main(void)
{
    arm_mem_init(RAM_SIZE);
    printf("=== diferencial: decomp/lime/limeVector.c contra el oraculo ===\n\n");

    /* casos con nombre */
    static const float named[][3] = {
        { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 },
        { 3, 4, 0 }, { 1, 2, 2 }, { -5, 0, 12 }, { -1, -1, -1 },
        { 1e-8f, 0, 0 }, { 1e8f, 1e8f, 1e8f }, { 1e-20f, 1e-20f, 1e-20f },
        { 0.5f, -0.5f, 0.70710678f }, { 1234.5f, -6789.0f, 42.0f },
    };
    for (size_t i = 0; i < sizeof(named) / sizeof(named[0]); i++) {
        compare(named[i][0], named[i][1], named[i][2]);
    }

    /* barrido pseudoaleatorio determinista */
    uint32_t seed = 0x9E3779B9u;
    for (int i = 0; i < 20000; i++) {
        float c[3];
        for (int k = 0; k < 3; k++) {
            seed = seed * 1664525u + 1013904223u;
            c[k] = ((float)(int32_t)seed / 2147483648.0f) * 1000.0f;
        }
        compare(c[0], c[1], c[2]);
    }

    printf("casos comparados: %d    divergencias: %d\n", g_cases, g_fail);
    printf("%s\n", g_fail ? "RESULTADO: FALLO"
                          : "RESULTADO: el C limpio se comporta igual que el original");
    arm_mem_free();
    return g_fail ? 1 : 0;
}
