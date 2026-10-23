/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Stand-in for the part of Blender's BLI_math.h the CCGSubSurf files use.
 * normalize_v3 is Blender's (math_vector_inline.c, 2.79b): scale to unit
 * length and return the old length, or zero the vector when it is too short. */
#ifndef UMK3_SHIM_BLI_MATH_H
#define UMK3_SHIM_BLI_MATH_H

#include <math.h>

static inline float normalize_v3(float n[3])
{
	const float d = n[0] * n[0] + n[1] * n[1] + n[2] * n[2];

	if (d > 1.0e-35f) {
		const float len = sqrtf(d);
		const float mul = 1.0f / len;

		n[0] *= mul;
		n[1] *= mul;
		n[2] *= mul;
		return len;
	}
	n[0] = n[1] = n[2] = 0.0f;
	return 0.0f;
}

#endif
