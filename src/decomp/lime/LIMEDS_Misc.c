/*
 * lime/common/LIMEDS_Misc.cpp -- the matrix-stack helpers.
 *
 * Recovered from the armv6 slice. Addresses below are armv6.
 *
 * The "DS" prefix survives from an earlier target; these are thin wrappers
 * over the fixed-function matrix stack, which is how this engine does all of
 * its transform work. See docs/LIME-ENGINE.md for the full GL surface.
 */

#include <math.h>
#include <string.h>
#include <stdio.h>
#include "lime.h"


/* --------------------------------------------------------- LIME_PushMatrix
 *
 * armv6 0x00080124, 12 bytes.
 *
 * Exactly glPushMatrix, wrapped. The wrapper exists so the platform layer has
 * a single place to intercept, which is precisely what a port needs when the
 * fixed-function stack has to be emulated on a core profile.
 */
void LIME_PushMatrix(void)
{
    glPushMatrix();
}


/* ---------------------------------------------------------- LIME_PopMatrix
 *
 * armv6 0x000800fc, 24 bytes.
 *
 * Pops `count` levels, not one. Passing zero is a no-op rather than an error
 * -- the early return is the first thing the function does.
 */
void LIME_PopMatrix(int count)
{
    int i;

    if (count == 0)
        return;

    for (i = 0; i < count; i++)
        glPopMatrix();
}


/* ----------------------------------------------- LIMEDS_SetObjectOrientation
 *
 * armv6 0x0007ff50, 36 bytes.
 *
 * Places an object: translate to its position, then apply its orientation
 * matrix.
 *
 * **Note what this does NOT do.** It hands `matrix` straight to glMultMatrixf
 * with no transpose, whereas CreatePerspectiveMatrix produces a layout that
 * has to be transposed before GL will accept it -- it puts the perspective
 * term at m[11] and the -1 at m[14], which is those two swapped relative to
 * GL's column-major order.
 *
 * So the engine is not consistent across all of its matrices, and a port must
 * not apply one blanket convention to every 4x4 it meets. Which matrices are
 * stored in which order is not yet fully mapped; the projection needs the
 * transpose and the object matrices here do not.
 */
void LIMEDS_SetObjectOrientation(const float *matrix, const limeVECTOR3 *pos)
{
    glMatrixMode(GL_MODELVIEW);                /* 0x1700 */
    glTranslatef(pos->x, pos->y, pos->z);
    glMultMatrixf(matrix);
}


/* ------------------------------------------------- ConvertDSMatrixtoPCMatrix
 *
 * armv6 0x0007fb30, 240 bytes.
 *
 * Converts a matrix from the engine's older fixed-point form into the float
 * 4x4 the renderer uses.
 *
 * **The "DS" in this file is Nintendo DS.** The source is `int32` and every
 * element is multiplied by the literal at 0x0007fc40, which is `0x39800000` --
 * exactly **1/4096**, the DS's 1.3.12 fixed-point scale. That fits what the
 * project already knew about this codebase's ancestry: `EA_SDK/microedition/`
 * carries Java ME classes, so the lineage runs handheld to phone to iOS, and
 * this function is the seam where the old numeric format is still being paid
 * for at runtime.
 *
 * It also settles a layout question. The source is packed three floats at a
 * time while the destination gets **an explicit zero written at +0x0c and
 * +0x1c** -- the fourth column of each row. So the PC matrix is **row-major
 * 4x4 with w = 0 per row**, which is the same convention
 * CreatePerspectiveMatrix uses and the opposite of what glLoadMatrixf expects.
 *
 * (Note this is *not* the convention LIMEDS_SetObjectOrientation's argument
 * follows -- see the comment there. The engine genuinely mixes them.)
 */
void ConvertDSMatrixtoPCMatrix(const int32_t *src, float *dst)
{
    const float k = 1.0f / 4096.0f;     /* literal 0x39800000 */
    int row, col;

    for (row = 0; row < 4; row++)
        for (col = 0; col < 3; col++)
            dst[row * 4 + col] = (float)src[row * 3 + col] * k;

    /* **Three zeros and a one, not four zeros.**
     *
     * The fourth column of the first three rows is zeroed, and m[15] is 1.0f --
     * the homogeneous coordinate. An earlier version of this function zeroed all
     * four in one loop, which is the tidier-looking code and is wrong: it
     * produces a matrix whose last row is all zero, and anything that multiplies
     * by it loses the translation entirely.
     *
     * tests/test_limedsmisc_diff.c caught it on its first run. The function had
     * been in this repo, read and re-read, since the module was first written. */
    dst[3] = 0.0f;
    dst[7] = 0.0f;
    dst[11] = 0.0f;
    dst[15] = 1.0f;
}


/* ---------------------------------------------------------------------------
 * QSTMATRIX -- Quaternion, Scale, Translation, in 16-bit fixed point
 *
 * The two functions below give the type its meaning. `ConvertQSTMatrixtoPCMatrix`
 * reads **four consecutive int16** at +0x00, +0x02, +0x04 and +0x06, converts
 * them to double, and immediately starts squaring and cross-multiplying them --
 * which is quaternion-to-matrix arithmetic and nothing else. So the Q in QST is
 * literal.
 *
 * The scale is the double at 0x0007ff40, and it is **not** 1/32767 -- it is
 * `3.0518509447574615e-05`, whose reciprocal is 32767.000030516647. A
 * differential test at bit precision is what separated the two; by eye they are
 * the same number. See the note in ConvertQSTMatrixtoPCMatrix below.
 *
 * The engine has four fixed-point conventions and they are easy to mix up:
 *
 *      int16  * 3.0518509447574615e-05   QST quaternions  (measured)
 *      int16  * 1/32767                  .meshset vertex positions (see below)
 *      int32  * 1/4096                   DS matrices
 *      uint16 * 1/65536                  .skin weights
 *
 * **The `.meshset` constant is a separate one and has not been re-checked
 * against its own literal.** It was derived by fitting the shipped data, which
 * validated against 604 of 605 files -- strong evidence for a value very close
 * to 1/32767, and not the same thing as reading the eight bytes. Anyone touching
 * that path should read the literal, now that this one turned out to differ.
 *
 * Getting one wrong produces geometry that is subtly the wrong size rather than
 * obviously broken, which is the worst kind of wrong.
 */


/* ------------------------------------------------------------ LerpQSTMatrix
 *
 * armv6 0x0007fc44, 344 bytes.  **Complete.**
 *
 * Blends two QST matrices by `t`, and it is fully unrolled -- no loop, ten
 * elements written one after another.
 *
 * ## It treats the two halves differently
 *
 * ```
 *   ldrsh / strh   0x00 0x02 0x04 0x06        the quaternion, as int16
 *   vldr  / vstr   0x08 0x0c 0x10 0x14 0x18 0x1c   scale and translation, float
 * ```
 *
 * Four plus six, exactly matching QSTMATRIX. **So the re-quantisation applies
 * to the quaternion only** -- the scale and translation are blended in float and
 * stay float. This file used to say "re-quantised to int16 at every element",
 * which was true of the half it had looked at and wrong about the rest.
 *
 * The quaternion round-trip is real and worth reproducing: `ldrsh` up,
 * `vcvt.f32.s32`, the blend, `vcvt.s32.f32` and `strh` back down, per element.
 * A port that keeps the whole thing in float produces visibly smoother rotation
 * than the original, which sounds like an improvement and is a behaviour change.
 *
 * ## The blend
 *
 *      vsub.f32 s14, s15, s12       ; 1 - t, once, before the elements
 *      vmul.f32 s15, s12, s15       ; t * b
 *      vmla.f32 s15, s13, s14       ; + a * (1 - t)
 *
 * So `out = b*t + a*(1-t)`, which means **`t = 0` yields `a`** -- the same
 * direction as LerpVector3 and GetSlerpedQ, and the third function in this
 * engine to blend that way round.
 */
void LerpQSTMatrix(const QSTMATRIX *a, const QSTMATRIX *b, float t,
                   QSTMATRIX *out)
{
    const float u = 1.0f - t;           /* computed once, before the elements */
    int i;

    /* the quaternion: up to float, blended, and back DOWN to int16 */
    for (i = 0; i < 4; i++)
        out->q[i] = (int16_t)((float)b->q[i] * t + (float)a->q[i] * u);

    /* scale and translation: float throughout, no quantisation */
    for (i = 0; i < 3; i++)
        out->scale[i] = b->scale[i] * t + a->scale[i] * u;
    for (i = 0; i < 3; i++)
        out->translation[i] = b->translation[i] * t + a->translation[i] * u;
}


/* ------------------------------------------------- ConvertQSTMatrixtoPCMatrix
 *
 * armv6 0x0007fd90, 416 bytes.  **Not fully decompiled.**
 *
 * Expands a QST matrix into the float 4x4 the renderer uses.
 *
 * What is established: it reads four `int16` from +0x00 through +0x06, scales
 * each by the literal at 0x0007ff40 (**not** 1/32767 -- see below), and works
 * in **double** -- `vcvt.f64.s32`, `vmul.f64` --
 * before narrowing. The immediate squaring (`vmul.f64 d1, d4, d4`) is the
 * quaternion-to-matrix expansion, the same shape as `GetMFromQuat2` in
 * RenderSkinned.cpp but at higher precision.
 *
 * Computing in double here is consistent with the rest of the engine's
 * accuracy-sensitive code -- `CreatePerspectiveMatrix` and `NormaliseLDirs` do
 * the same and narrow once at the end.
 *
 * ## The scale and translation, settled
 *
 * The rotation is the textbook expansion -- `m[0] = 1 - 2y² - 2z²`,
 * `m[1] = 2xy + 2wz`, and so on -- computed in double and narrowed once. Then
 * each **row** is multiplied by a float read from the source:
 *
 *      vldr   s15, [r0, #0x08]   ; multiplies *m, m[1], m[2]
 *      vldr   s15, [r0, #0x0c]   ; multiplies m[4], m[5], m[6]
 *      vldr   s15, [r0, #0x10]   ; multiplies m[8], m[9], m[10]
 *
 * So the S in QST is a **per-axis scale applied to rows, not a uniform
 * factor**, and those three fields are `float` rather than the fixed point the
 * quaternion uses. Scaling rows rather than columns is what makes it a scale in
 * the object's own frame.
 *
 * The translation is copied **verbatim**, as words with no conversion:
 *
 *      ldr r3, [r0, #0x14] -> str r3, [r1, #0x30]
 *      ldr r3, [r0, #0x18] -> str r3, [r1, #0x34]
 *      ldr r3, [r0, #0x1c] -> str r3, [r1, #0x38]
 *      mov r3, #0x3f800000 -> str r3, [r1, #0x3c]     ; 1.0f
 *
 * ## And that lands it in GL's layout
 *
 * `m[12]`, `m[13]`, `m[14]` with `1.0f` at `m[15]` is exactly where
 * `glMultMatrixf` expects a translation. So this result is **GL-ready and needs
 * no transpose** -- which is why FlushTranspMeshList and
 * LIME_RenderSceneOverrideTextures hand it straight to GL.
 *
 * Contrast ConvertDSMatrixtoPCMatrix in this same file, which writes explicit
 * zeros at `+0x0c` and `+0x1c` and produces the row-major form that does need
 * transposing. Two conversion functions, two conventions, one file. The rule
 * this project records holds: matrices built from a rotation here arrive
 * GL-ready, and matrices converted from the older fixed-point formats do not.
 */
void ConvertQSTMatrixtoPCMatrix(const QSTMATRIX *src, float *dst)
{
    /* **Not 1/32767.** The literal at 0x0007ff40 is the double
     * 3.0518509447574615e-05, whose reciprocal is 32767.000030516647 -- close
     * enough to 1/32767 that this project has written it that way for a long
     * time, and different enough to diverge in the last bits of every
     * quaternion-derived element.
     *
     * tests/test_limedsmisc_diff.c is what made the difference visible: about
     * four ULP, uniform across the whole matrix, which is the signature of a
     * wrong constant rather than a wrong formula. Reading the eight bytes
     * settled it in one step after the association had been checked and
     * cleared. */
    const double k = 3.0518509447574615e-05;    /* the literal at 0x0007ff40 */
    double x = (double)src->q[0] * k;
    double y = (double)src->q[1] * k;
    double z = (double)src->q[2] * k;
    double w = (double)src->q[3] * k;

    double xx = x * x, yy = y * y, zz = z * z;
    double xy = x * y, xz = x * z, yz = y * z;
    double wx = w * x, wy = w * y, wz = w * z;

    /* the fourth column, written as literal zeros */
    dst[3] = 0.0f;  dst[7] = 0.0f;  dst[11] = 0.0f;

    /* **The association matters.** Each product is doubled ONCE into its own
     * value, and the diagonal subtracts the doubled pair as a single term:
     *
     *      vadd.f64 d7, d1, d6      ; 2y^2 + 2z^2
     *      vsub.f64 d7, d5, d7      ; 1.0 - that, in ONE subtraction
     *
     * Written as `1.0 - (yy+yy) - (zz+zz)` -- two subtractions -- the result
     * differs in the last bits, and tests/test_limedsmisc_diff.c fails on
     * exactly the diagonal elements. Floating point is not associative, and a
     * differential test at bit precision is what makes that visible instead of
     * invisible. */
    {
        double xx2 = xx + xx, yy2 = yy + yy, zz2 = zz + zz;
        double xy2 = xy + xy, xz2 = xz + xz, yz2 = yz + yz;
        double wx2 = wx + wx, wy2 = wy + wy, wz2 = wz + wz;

        dst[0]  = (float)(1.0 - (yy2 + zz2));
        dst[1]  = (float)(xy2 + wz2);
        dst[2]  = (float)(xz2 - wy2);

        dst[4]  = (float)(xy2 - wz2);
        dst[5]  = (float)(1.0 - (xx2 + zz2));
        dst[6]  = (float)(yz2 + wx2);

        dst[8]  = (float)(xz2 + wy2);
        dst[9]  = (float)(yz2 - wx2);
        dst[10] = (float)(1.0 - (xx2 + yy2));
    }

    /* each ROW scaled by its own axis factor */
    dst[0] *= src->scale[0];  dst[1] *= src->scale[0];  dst[2]  *= src->scale[0];
    dst[4] *= src->scale[1];  dst[5] *= src->scale[1];  dst[6]  *= src->scale[1];
    dst[8] *= src->scale[2];  dst[9] *= src->scale[2];  dst[10] *= src->scale[2];

    /* translation straight into GL's bottom row */
    dst[12] = src->translation[0];
    dst[13] = src->translation[1];
    dst[14] = src->translation[2];
    dst[15] = 1.0f;
}


/* ---------------------------------------------------------- LIMEDS_Set3dMode
 *
 * armv7 0x0005d944, 216 bytes (armv6 0x00080134).  **Verified**:
 * tests/test_limeds_camera_gl_diff.c, 2,004 cases, GL call stream and the
 * globals it writes bit-identical to the recompiled original.
 *
 * Resets both matrix stacks, then builds the projection:
 *
 *      ratio = 0.6f                                        0x3f19999a
 *      CreatePerspectiveMatrix(limePerspectiveMatrix,
 *                              0.436331958f,               0x3edf66e7, 25 deg
 *                              0.6f, 1.0f, 400.0f)         aspect, near, far
 *      limeScaleMatrix(limePerspectiveMatrix,
 *                      (sideways ? limeScreenWidth : limeScreenHeight)
 *                      / limeDeviceHeight)
 *      glMultMatrixf(limePerspectiveMatrix)
 *      glMultMatrixf(limeSidewaysMat)
 *
 * The three GL calls are the stubs at 0xdda7c / 0xdda88 / 0xdda94, which
 * resolve through consecutive `__la_symbol_ptr` slots to glLoadIdentity,
 * glMatrixMode and glMultMatrixf -- and the call pattern agrees
 * (`mov r0,#0x1700; blx dda88; blx dda7c`).
 *
 * **The 0.6 aspect is for the PORTRAIT panel.** The iPhone's GL surface is
 * 320x480 and `limeBegin` writes `limeSidewaysMat = RotMatrixZ(-pi/2)` when
 * the device is sideways, so the landscape game is turned onto it here, after
 * the perspective, exactly as `limeSet2DDrawing` turns its ortho.
 *
 * Until this was transcribed the function stopped after the two
 * `glLoadIdentity` calls, so every 3D draw in the front end -- the vortex
 * behind the main menu above all -- went through an IDENTITY projection: no
 * perspective, z clipped at +-1, and the spinning shells collapsed into flat
 * streaks along the bottom of the screen.
 *
 * `limePortDisplayRotation` is not in the binary. It is the port's stand-in
 * for the phone being held sideways; see its definition in runtime/draw_gl.c.
 */
extern float ratio;                             /* 0x0017146c */
extern float limePerspectiveMatrix[16];         /* 0x00391f8c */
extern float limeSidewaysMat[16];               /* 0x00391fd4 */
extern int   limeDeviceSideways;                /* 0x00171ad8 */
extern int   limeDeviceHeight;                  /* 0x00171ae8 */
extern int   limeScreenWidth;
extern int   limeScreenHeight;

void limePortDisplayRotation(void);

void LIMEDS_Set3dMode(void)
{
    float *m = limePerspectiveMatrix;
    int    s;

    glMatrixMode(GL_MODELVIEW);         /* 0x1700 */
    glLoadIdentity();
    glMatrixMode(GL_PROJECTION);        /* 0x1701 */
    glLoadIdentity();
    limePortDisplayRotation();          /* port only -- not in the binary */

    {
        /* Port: the launcher's aspect= (runtime/game_main.c). The iPhone
         * build's 0.6 is drawn for 3:2 and stretches anything wider; 3:2
         * keeps it untouched. 4:3 is the iPad build's own (UMK3 for iPad
         * 1.2.56, _LIMEDS_Set3dMode at 0x5d99c): aspect 0x3f3f7cee and,
         * after the panel scale, a second limeScaleMatrix by 0x3f9f928e --
         * the iPhone's vertical view at true proportions, the sides
         * narrower. Wider than 3:2 is the same rule, not in either binary:
         * k = (h / w) / 0.6, aspect 0.6k, then scale k -- the vertical view
         * unchanged and the extra width shows more of the stage. */
        float wide = limeScreenHeight > 0
                   ? (float)limeScreenWidth / (float)limeScreenHeight : 1.5f;
        float asp = 0.6f, k = 0.0f;

        if (wide > 1.32f && wide < 1.35f) {
            asp = 0.748000026f;                         /* 0x3f3f7cee */
            k   = 1.24665999f;                          /* 0x3f9f928e */
        } else if (wide < 1.49f || wide > 1.51f) {
            k   = (1.0f / wide) / 0.6f;
            asp = 0.6f * k;
        }
        ratio = asp;
        CreatePerspectiveMatrix(m, 0.436331958f, asp, 1.0f, 400.0f);

        s = limeDeviceSideways ? limeScreenWidth : limeScreenHeight;
        limeScaleMatrix(m, (float)s / (float)limeDeviceHeight);
        if (k != 0.0f)
            limeScaleMatrix(m, k);
    }

    glMultMatrixf(limePerspectiveMatrix);
    glMultMatrixf(limeSidewaysMat);
}


/* ---------------------------------------------- LIMEDS_SetCameraOrientation
 *
 * armv6 0x0007ff84, armv7 0x0005d798.  **Verified**: rows corrected against the
 * armv7 disassembly and checked by tests/test_limeds_camera_gl_diff.c, 5,008
 * cases including the degenerate ones, every glMultMatrixf matrix
 * bit-identical to the recompiled original.
 *
 * This is **gluLookAt**, written out by hand. The engine never links GLU, so
 * the view matrix is built here and this is the only place the camera basis
 * exists.
 *
 * **It takes nine floats, not three vectors.** The prologue does
 * `stm sp, {r0, r1, r2}` and then immediately reads them back with `vldr` --
 * the first three arguments are float bit patterns arriving in core registers
 * under the soft-float ABI, not pointers. The other six come off the caller
 * stack at +0x7c through +0x90. So the grouping into eye, target and up below
 * is a reading of how they are used, not of how they are declared:
 *
 *      vsub.f32 s8, s22, s16       arg0 - arg3
 *      vsub.f32 s7, s23, s17       arg1 - arg4
 *      vsub.f32 s6, s24, s18       arg2 - arg5
 *
 * -- three componentwise subtractions in a row is a vector difference, which
 * makes 0..2 and 3..5 two points, and 6..8 the remaining direction.
 *
 * The rest is unmistakable: sum the squares, `vsqrt.f32`, divide. The divides
 * are `vdivne`, predicated on the length compare, so **a zero length silently
 * leaves the vector unnormalised rather than producing NaN**. That is worth
 * reproducing exactly; a port that divides unconditionally turns a degenerate
 * camera into a black screen instead of a stuck one.
 *
 * Note it does **not** call `Normalise` or `Len` from limeVector.cpp even
 * though both exist -- the normalisation and the cross product are inlined
 * here. There is no cross-product symbol anywhere in the binary, so every
 * cross product in this engine is written out by hand at its use site.
 *
 * The eye position is negated up front (`vneg.f32` on all three) and used at
 * the very end, which gives away the whole structure:
 *
 *      glMultMatrixf(basis);
 *      glTranslatef(-eye.x, -eye.y, -eye.z);
 *
 * -- rotate the world into the camera, then move it back. Standard view
 * matrix construction.
 *
 * **The basis is written down columns, not across rows.** The three components
 * of the first vector go to +0x00, +0x10 and +0x20 of the matrix; the second
 * to +0x04, +0x14, +0x24. Writing a basis transposed *is* inverting it for a
 * pure rotation, which is exactly what a view matrix needs -- so the matrix
 * arrives at glMultMatrixf already in the layout GL wants, and needs no
 * transpose.
 *
 * That resolves the convention question this file raises twice elsewhere.
 * ConvertDSMatrixtoPCMatrix produces row-major and needs transposing; this one
 * and LIMEDS_SetObjectOrientation do not. The rule is not per-function
 * whim -- **matrices built from basis vectors here are stored GL-ready, and
 * matrices converted from the old fixed-point formats are not.**
 *
 * The remaining fields are written as literal zeros (+0x0c, +0x1c, +0x2c for
 * the fourth column, +0x30, +0x34, +0x38 for the bottom row) with 1.0f at
 * +0x3c, so the translation genuinely is not in the matrix -- it is the
 * glTranslatef that follows.
 *
 * Only two normalised vectors are stored, at +0x00/+0x10/+0x20 and
 * +0x04/+0x14/+0x24. The third column of the basis is among the zeroed slots.
 * That is unusual enough to flag rather than quietly complete: it is recorded
 * as written, and the third basis vector is left out rather than invented.
 */
void LIMEDS_SetCameraOrientation(float eyeX, float eyeY, float eyeZ,
                                 float atX,  float atY,  float atZ,
                                 float upX,  float upY,  float upZ)
{
    float m[16];
    float fx, fy, fz, rx, ry, rz, ux, uy, uz, len;

    glMatrixMode(GL_MODELVIEW);         /* 0x1700 */
    glLoadIdentity();

    fx = eyeX - atX;                    /* note the direction: eye - target */
    fy = eyeY - atY;
    fz = eyeZ - atZ;

    len = sqrtf(fx * fx + fy * fy + fz * fz);
    if (len != 0.0f) {                  /* vdivne -- zero leaves it alone */
        fx /= len; fy /= len; fz /= len;
    }

    rx = upY * fz - upZ * fy;           /* side = up x F, inlined as in the ROM */
    ry = upZ * fx - upX * fz;
    rz = upX * fy - upY * fx;

    /* true up = F x side -- from the side vector BEFORE it is normalised
     * (0x5d848..0x5d878 use s8/s10/s12 ahead of the vdivne at 0x5d886). */
    ux = fy * rz - fz * ry;
    uy = fz * rx - fx * rz;
    uz = fx * ry - fy * rx;

    len = sqrtf(rx * rx + ry * ry + rz * rz);
    if (len != 0.0f) {
        rx /= len; ry /= len; rz /= len;
    }

    len = sqrtf(ux * ux + uy * uy + uz * uz);
    if (len != 0.0f) {
        ux /= len; uy /= len; uz /= len;
    }

    /* Rows side / up / F, stored at sp+0x10 (armv7 0x5d88e..0x5d8e8):
     *
     *      0x10 0x20 0x30   side      m[0] m[4] m[8]
     *      0x14 0x24 0x34   up        m[1] m[5] m[9]
     *      0x18 0x28 0x38   F         m[2] m[6] m[10]
     *
     * This used to put F on the second row and ZEROS on the third, with no up
     * vector at all. Every eye-space z came out 0, so w was 0 and anything
     * drawn through SetToUseCamera -- the main menu's vortex among it --
     * collapsed. */
    m[0] = rx;   m[4] = ry;   m[8]  = rz;   m[12] = 0.0f;
    m[1] = ux;   m[5] = uy;   m[9]  = uz;   m[13] = 0.0f;
    m[2] = fx;   m[6] = fy;   m[10] = fz;   m[14] = 0.0f;
    m[3] = 0.0f; m[7] = 0.0f; m[11] = 0.0f; m[15] = 1.0f;

    glMultMatrixf(m);
    glTranslatef(-eyeX, -eyeY, -eyeZ);
}
