#include "limemath.h"
#include <math.h>

void lime_identity(float *m)
{
    for (int i = 0; i < 16; i++) m[i] = 0.0f;
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

void lime_perspective(float *m, float fov, float aspect,
                      float z_near, float z_far)
{
    /* Verbatim from CreatePerspectiveMatrix (armv7 0x0005e0a0). The double
     * intermediate is the engine's, not ours -- it computes in double and
     * narrows once at the end. */
    double f64 = sin((double)fov) / (1.0 - cos((double)fov));
    float  f   = (float)f64;

    for (int i = 0; i < 16; i++) m[i] = 0.0f;

    m[0]  = f / aspect;
    m[5]  = f;
    m[10] = (z_far + z_near) / (z_near - z_far);
    m[11] = 2.0f * z_far * z_near / (z_near - z_far);
    m[14] = -1.0f;
}

void lime_rot_y(float *m, float angle)
{
    /* RotMatrixY, verbatim: m[0]=cos, m[2]=-sin, m[8]=sin, m[10]=cos */
    float c = cosf(angle), s = sinf(angle);
    lime_identity(m);
    m[0]  =  c;
    m[2]  = -s;
    m[8]  =  s;
    m[10] =  c;
}

void lime_to_gl(const float *src, float *dst)
{
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            dst[c * 4 + r] = src[r * 4 + c];
}
