/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Stand-in for Blender's BKE_ccg.h (2.79b): the CCGKey that CCGSubSurf.c's
 * CCG_key fills in, with the same fields. */
#ifndef UMK3_SHIM_BKE_CCG_H
#define UMK3_SHIM_BKE_CCG_H

struct CCGSubSurf;

typedef struct CCGKey {
	int level;

	/* number of bytes in each element (one float per layer, plus
	 * three floats for normals if enabled) */
	int elem_size;

	/* number of elements along each side of grid */
	int grid_size;
	/* number of elements in the grid (grid size squared) */
	int grid_area;
	/* number of bytes in each grid (grid_area * elem_size) */
	int grid_bytes;

	/* currently always the last three floats, unless normals are
	 * disabled */
	int normal_offset;

	/* offset in bytes of mask value; only valid if 'has_mask' is
	 * true */
	int mask_offset;

	int num_layers;
	int has_normals;
	int has_mask;
} CCGKey;

void CCG_key(CCGKey *key, const struct CCGSubSurf *ss, int level);
void CCG_key_top_level(CCGKey *key, const struct CCGSubSurf *ss);

int BKE_ccg_gridsize(int level);
int BKE_ccg_factor(int low_level, int high_level);

#endif
