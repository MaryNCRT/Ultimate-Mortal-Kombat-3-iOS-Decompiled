#include "light.h"
#include <math.h>

/* The values the binary ships with, read out of the globals at 0x001bb874.
 * They are NOT unit length -- they are authored in world units, like
 * positions, which is exactly why NormaliseLDirs has to exist. */
static float g_dir0[3] = {  68.0f,  462.0f,  247.0f };
static float g_dir1[3] = {   0.0f, -300.0f,  -50.0f };

/* Light 0 ships with a power of zero, so as initialised it contributes
 * nothing and only light 1 does any work. Either it is switched on at runtime
 * or it is a disabled experiment; the initialiser alone cannot tell them
 * apart, so the shipped values are reproduced here unchanged.
 *
 * For the slice that would leave one hard light and pitch-black backs, so
 * g_fill exists purely as a viewer affordance and is not part of the engine. */
static float g_power0 = 0.0f,  g_exp0 = 0.8f;
static float g_power1 = 2.0f,  g_exp1 = 3.5f;
static float g_fill   = 0.22f;   /* viewer only -- see above */

void lime_light_init(void)
{
    /* NormaliseLDirs: reciprocal square root in double, narrowed to float. */
    double k;

    k = 1.0 / sqrt((double)(g_dir0[0]*g_dir0[0] +
                            g_dir0[1]*g_dir0[1] +
                            g_dir0[2]*g_dir0[2]));
    for (int i = 0; i < 3; i++) g_dir0[i] *= (float)k;

    k = 1.0 / sqrt((double)(g_dir1[0]*g_dir1[0] +
                            g_dir1[1]*g_dir1[1] +
                            g_dir1[2]*g_dir1[2]));
    for (int i = 0; i < 3; i++) g_dir1[i] *= (float)k;
}

float lime_light_vert(float nx, float ny, float nz)
{
    float l = 0.0f;                     /* no ambient -- literal 0.0f */
    float d;

    d = -(nx * g_dir0[0] + ny * g_dir0[1] + nz * g_dir0[2]);
    if (d < 0.0f) d = 0.0f;
    l += g_power0 * (float)pow((double)d, (double)g_exp0);

    d = -(nx * g_dir1[0] + ny * g_dir1[1] + nz * g_dir1[2]);
    if (d < 0.0f) d = 0.0f;
    l += g_power1 * (float)pow((double)d, (double)g_exp1);

    l += g_fill;                        /* viewer affordance, not the engine */

    if (l > 1.0f) l = 1.0f;             /* ceiling, verified at 0x83c04 */
    return l;
}
