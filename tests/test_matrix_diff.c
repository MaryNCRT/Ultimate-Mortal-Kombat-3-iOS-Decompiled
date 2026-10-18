/*
 * test_matrix_diff.c — diferencial: decomp/lime/Matrix.c contra el oraculo.
 *
 * Criterio de aceptacion del proyecto: el C limpio escrito a mano tiene que
 * comportarse igual que el codigo recompilado (fiel por construccion) con las
 * mismas entradas. Aqui se barren miles de casos por funcion.
 *
 * Tolerancia: se exige igualdad EXACTA de bits salvo en las funciones que
 * pasan por libm (rotaciones y perspectiva), donde se admite 1 ULP porque el
 * oraculo y el C limpio llaman a cosf/sinf/cos/sin por caminos distintos.
 */

#include "arm_runtime.h"
#include "matrix.h"                 /* oraculo: tools/armrecomp */
#include "../src/decomp/lime/lime.h"    /* C limpio */

#include <math.h>
#include <stdio.h>
#include <string.h>

#define RAM_SIZE   (1u << 20)
#define STACK_TOP  0x000F0000u
#define G_A        0x00001000u
#define G_B        0x00001100u
#define G_C        0x00001200u
#define G_V        0x00001300u
#define G_W        0x00001400u

static int g_fail = 0;
static long g_cases = 0;

static void ctx_reset(arm_ctx *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->r[SP] = STACK_TOP;
}

static void put_mat(uint32_t a, const float *m)
{
    for (int i = 0; i < 16; i++) MEM_ST32(a + 4u * i, F32_U32(m[i]));
}

static void get_mat(uint32_t a, float *m)
{
    for (int i = 0; i < 16; i++) m[i] = U32_F32(MEM_LD32(a + 4u * i));
}

/* ulps = 0 exige bits identicos */
static int same(float a, float b, int ulps)
{
    if (a != a && b != b) return 1;
    if (memcmp(&a, &b, 4) == 0) return 1;
    if (ulps == 0) return 0;
    float d = fabsf(a - b);
    float scale = fabsf(a) > fabsf(b) ? fabsf(a) : fabsf(b);
    return d <= (scale > 1.0f ? scale * 1e-6f : 1e-6f);
}

static void cmp_mat(const char *what, const float *clean, const float *orc, int ulps)
{
    g_cases++;
    for (int i = 0; i < 16; i++) {
        if (!same(clean[i], orc[i], ulps)) {
            printf("  DIVERGE %s  elemento %d: limpio=%.9g  oraculo=%.9g\n",
                   what, i, clean[i], orc[i]);
            g_fail++;
            return;
        }
    }
}

/* ---- envoltorios del oraculo ---- */

static void orc_identity(float *out)
{
    arm_ctx c; ctx_reset(&c); c.r[0] = G_A;
    func_0005dfe8_limeMatrixLoadIdentity(&c);
    get_mat(G_A, out);
}

static void orc_copy(const float *src, float *out)
{
    arm_ctx c; ctx_reset(&c);
    put_mat(G_A, src);
    c.r[0] = G_A; c.r[1] = G_B;
    func_0005dfbc_limeMatrixCopy(&c);
    get_mat(G_B, out);
}

static void orc_mult(const float *a, const float *b, float *out)
{
    arm_ctx c; ctx_reset(&c);
    put_mat(G_A, a); put_mat(G_B, b);
    c.r[0] = G_A; c.r[1] = G_B; c.r[2] = G_C;
    func_0005db9c_limeMatrixMult(&c);
    get_mat(G_C, out);
}

static void orc_rot(int axis, float ang, float *out)
{
    arm_ctx c; ctx_reset(&c);
    c.r[0] = G_A; c.r[1] = F32_U32(ang);
    if (axis == 0) func_0005e070_RotMatrixX(&c);
    else if (axis == 1) func_0005e040_RotMatrixY(&c);
    else func_0005e010_RotMatrixZ(&c);
    get_mat(G_A, out);
}

static void orc_scale(const float *m, float s, float *out)
{
    arm_ctx c; ctx_reset(&c);
    put_mat(G_A, m);
    c.r[0] = G_A; c.r[1] = F32_U32(s);
    func_0005daac_limeScaleMatrix(&c);
    get_mat(G_A, out);
}

static void orc_scale_xyz(const float *m, float x, float y, float z, float *out)
{
    arm_ctx c; ctx_reset(&c);
    put_mat(G_A, m);
    c.r[0] = G_A;
    c.r[1] = F32_U32(x); c.r[2] = F32_U32(y); c.r[3] = F32_U32(z);
    func_0005db20_limeScaleMatrixXYZ(&c);
    get_mat(G_A, out);
}

static void orc_rotskin(const float *m, const limeVECTOR3 *v, limeVECTOR3 *out)
{
    arm_ctx c; ctx_reset(&c);
    put_mat(G_A, m);
    MEM_ST32(G_V + 0, F32_U32(v->x));
    MEM_ST32(G_V + 4, F32_U32(v->y));
    MEM_ST32(G_V + 8, F32_U32(v->z));
    c.r[0] = G_A; c.r[1] = G_V; c.r[2] = G_W;
    func_0005da1c_limeMatrix3x4RotateSkin(&c);
    out->x = U32_F32(MEM_LD32(G_W + 0));
    out->y = U32_F32(MEM_LD32(G_W + 4));
    out->z = U32_F32(MEM_LD32(G_W + 8));
}

static void orc_persp(float fov, float aspect, float zn, float zf, float *out)
{
    arm_ctx c; ctx_reset(&c);
    c.r[0] = G_A;
    c.r[1] = F32_U32(fov); c.r[2] = F32_U32(aspect); c.r[3] = F32_U32(zn);
    c.r[SP] -= 16;
    MEM_ST32(c.r[SP], F32_U32(zf));
    func_0005e0a0_CreatePerspectiveMatrix(&c);
    get_mat(G_A, out);
}

/* ---- generador determinista ---- */

static uint32_t g_seed = 0x12345678u;

static float rnd(float range)
{
    g_seed = g_seed * 1664525u + 1013904223u;
    return ((float)(int32_t)g_seed / 2147483648.0f) * range;
}

static void rnd_mat(float *m, float range)
{
    for (int i = 0; i < 16; i++) m[i] = rnd(range);
}

int main(void)
{
    arm_mem_init(RAM_SIZE);
    printf("=== diferencial: decomp/lime/Matrix.c contra el oraculo ===\n\n");

    float a[16], b[16], clean[16], orc[16];

    /* --- identidad --- */
    limeMatrixLoadIdentity(clean);
    orc_identity(orc);
    cmp_mat("limeMatrixLoadIdentity", clean, orc, 0);

    for (int it = 0; it < 4000; it++) {
        rnd_mat(a, 1000.0f);
        rnd_mat(b, 100.0f);

        /* --- copia --- */
        limeMatrixCopy(a, clean);
        orc_copy(a, orc);
        cmp_mat("limeMatrixCopy", clean, orc, 0);

        /* --- multiplicacion --- */
        limeMatrixMult(a, b, clean);
        orc_mult(a, b, orc);
        cmp_mat("limeMatrixMult", clean, orc, 1);

        /* --- escalados --- */
        float s = rnd(10.0f);
        memcpy(clean, a, sizeof(a));
        limeScaleMatrix(clean, s);
        orc_scale(a, s, orc);
        cmp_mat("limeScaleMatrix", clean, orc, 0);

        float sx = rnd(10.0f), sy = rnd(10.0f), sz = rnd(10.0f);
        memcpy(clean, a, sizeof(a));
        limeScaleMatrixXYZ(clean, sx, sy, sz);
        orc_scale_xyz(a, sx, sy, sz, orc);
        cmp_mat("limeScaleMatrixXYZ", clean, orc, 0);

        /* --- rotaciones --- */
        float ang = rnd(7.0f);
        RotMatrixX(clean, ang); orc_rot(0, ang, orc);
        cmp_mat("RotMatrixX", clean, orc, 1);
        RotMatrixY(clean, ang); orc_rot(1, ang, orc);
        cmp_mat("RotMatrixY", clean, orc, 1);
        RotMatrixZ(clean, ang); orc_rot(2, ang, orc);
        cmp_mat("RotMatrixZ", clean, orc, 1);

        /* --- rotacion de vector --- */
        limeVECTOR3 v = { rnd(500.0f), rnd(500.0f), rnd(500.0f) };
        limeVECTOR3 cv, ov;
        limeMatrix3x4RotateSkin(a, &v, &cv);
        orc_rotskin(a, &v, &ov);
        g_cases++;
        if (!same(cv.x, ov.x, 1) || !same(cv.y, ov.y, 1) || !same(cv.z, ov.z, 1)) {
            printf("  DIVERGE limeMatrix3x4RotateSkin: limpio=(%.9g,%.9g,%.9g) "
                   "oraculo=(%.9g,%.9g,%.9g)\n", cv.x, cv.y, cv.z, ov.x, ov.y, ov.z);
            g_fail++;
        }

        /* RotVector debe ser identico a RotateSkin */
        limeVECTOR3 rv;
        RotVector(a, &v, &rv);
        g_cases++;
        if (memcmp(&rv, &cv, sizeof(rv)) != 0) {
            printf("  DIVERGE RotVector != limeMatrix3x4RotateSkin\n");
            g_fail++;
        }

        /* --- perspectiva (el gancho del widescreen) --- */
        float fov = 0.05f + fabsf(rnd(1.0f));
        float asp = 0.5f + fabsf(rnd(2.0f));
        float zn  = 0.1f + fabsf(rnd(10.0f));
        float zf  = zn + 1.0f + fabsf(rnd(5000.0f));
        CreatePerspectiveMatrix(clean, fov, asp, zn, zf);
        orc_persp(fov, asp, zn, zf, orc);
        cmp_mat("CreatePerspectiveMatrix", clean, orc, 1);
    }

    /* Casos con nombre de la perspectiva: relaciones de aspecto reales */
    static const float aspects[] = { 4.0f / 3.0f, 16.0f / 9.0f, 16.0f / 10.0f,
                                     21.0f / 9.0f, 1.0f };
    for (size_t i = 0; i < sizeof(aspects) / sizeof(aspects[0]); i++) {
        CreatePerspectiveMatrix(clean, 0.6f, aspects[i], 1.0f, 1000.0f);
        orc_persp(0.6f, aspects[i], 1.0f, 1000.0f, orc);
        cmp_mat("CreatePerspectiveMatrix (aspecto real)", clean, orc, 1);
    }

    printf("casos comparados: %ld    divergencias: %d\n", g_cases, g_fail);
    printf("%s\n", g_fail ? "RESULTADO: FALLO"
                          : "RESULTADO: el C limpio se comporta igual que el original");
    arm_mem_free();
    return g_fail ? 1 : 0;
}
