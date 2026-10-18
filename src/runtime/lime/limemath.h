/*
 * The engine's own matrix maths, ported from the verified decompilation in
 * decomp/lime/Matrix.c.
 *
 * **The engine stores matrices row-major.** CreatePerspectiveMatrix puts the
 * perspective term at m[11] and the -1 at m[14]; OpenGL's glLoadMatrixf expects
 * column-major, which is those two swapped. Passing an engine matrix straight
 * to GL renders nothing. lime_to_gl() does the transpose.
 */
#ifndef LIME_MATH_H
#define LIME_MATH_H

/* f = sin(fov)/(1 - cos(fov)) = cot(fov/2), computed in double and narrowed.
 * `fov` is the FULL VERTICAL field of view in radians, and **aspect divides
 * the X term only** -- the single line a widescreen patch touches. */
void lime_perspective(float *m, float fov, float aspect,
                      float z_near, float z_far);

void lime_identity(float *m);
void lime_rot_y(float *m, float angle);

/* Row-major (engine) -> column-major (GL). */
void lime_to_gl(const float *src, float *dst);

#endif
