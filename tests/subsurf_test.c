/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * subsurf_test.c -- src/thirdparty/blender/umk3_subsurf.c on a mesh shaped
 * like the game's: triangles, one UV per corner, and vertices duplicated at
 * the seams the way an exporter leaves them.
 *
 *   zig cc -std=gnu99 -O2 tests/subsurf_test.c src/thirdparty/blender/umk3_subsurf.c
 *       src/thirdparty/blender/CCGSubSurf.c src/thirdparty/blender/CCGSubSurf_legacy.c
 *       src/thirdparty/blender/CCGSubSurf_util.c -I src/thirdparty/blender
 *       -I src/thirdparty/blender/shim -o subsurf_test.exe
 *
 * Checks, at levels 1 and 2:
 *   - the weld merges the 16 seam duplicates of the cube (24 vertices -> 8)
 *   - the result is closed: every edge has exactly two triangles (no holes)
 *   - every triangle faces the way the face it came from faces
 *   - every point lies inside the cube (Catmull-Clark shrinks a cube)
 *   - UVs and the per-corner value stay inside the range they started in
 *   - without the weld the cube stays open along its seams
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "umk3_subsurf.h"

static int g_fail;

#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); g_fail++; } } while (0)

/* A cube of side 2, each face its own four vertices (24), two triangles per
 * face, counter-clockwise seen from outside. */
static float g_pos[24 * 3];
static unsigned short g_tri[12 * 3];
static float g_uv[12 * 3 * 2];
static float g_attr[12 * 3];

static void make_cube(void)
{
	static const int axes[6][3] = {          /* normal axis, sign, u axis, v axis */
		{0, 1, 0}, {0, -1, 0}, {1, 1, 0}, {1, -1, 0}, {2, 1, 0}, {2, -1, 0}
	};
	int f, k, t = 0;

	for (f = 0; f < 6; f++) {
		int n = axes[f][0], s = axes[f][1];
		int u = (n + 1) % 3, v = (n + 2) % 3;
		static const float c[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
		for (k = 0; k < 4; k++) {
			float *p = g_pos + (f * 4 + k) * 3;
			p[n] = (float)s;
			p[u] = c[k][0];
			p[v] = s > 0 ? c[k][1] : -c[k][1];
		}
		/* two triangles, wound so the normal points along +s on axis n */
		{
			static const int q[6] = {0, 1, 2, 0, 2, 3};
			for (k = 0; k < 6; k++) {
				int corner = q[k];
				g_tri[t * 3 + (k % 3)] = (unsigned short)(f * 4 + corner);
				g_uv[(t * 3 + (k % 3)) * 2 + 0] = (c[corner][0] + 1.0f) * 0.5f;
				g_uv[(t * 3 + (k % 3)) * 2 + 1] = (c[corner][1] + 1.0f) * 0.5f;
				g_attr[t * 3 + (k % 3)] = (float)(corner * 50 + 25);
				if (k % 3 == 2)
					t++;
			}
		}
	}
}

static void face_normal(const float *a, const float *b, const float *c, float *n)
{
	float e1[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
	float e2[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
	n[0] = e1[1] * e2[2] - e1[2] * e2[1];
	n[1] = e1[2] * e2[0] - e1[0] * e2[2];
	n[2] = e1[0] * e2[1] - e1[1] * e2[0];
}

/* open edges of a triangle list, by position (output vertices are split per
 * UV, so topology is checked on positions, the way a viewer sees holes) */
static int open_edges(int nv, const float *pos, int nt, const unsigned *tri)
{
	int *id = malloc(sizeof(int) * (size_t)nv), i, j, n = 0, open = 0;
	int *ea, *eb, ne = 0;

	for (i = 0; i < nv; i++) {
		id[i] = -1;
		for (j = 0; j < i; j++)
			if (fabsf(pos[i * 3] - pos[j * 3]) < 1e-5f && fabsf(pos[i * 3 + 1] - pos[j * 3 + 1]) < 1e-5f
			    && fabsf(pos[i * 3 + 2] - pos[j * 3 + 2]) < 1e-5f) {
				id[i] = id[j];
				break;
			}
		if (id[i] < 0)
			id[i] = n++;
	}
	ea = malloc(sizeof(int) * (size_t)nt * 3);
	eb = malloc(sizeof(int) * (size_t)nt * 3);
	for (i = 0; i < nt; i++)
		for (j = 0; j < 3; j++) {
			int a = id[tri[i * 3 + j]], b = id[tri[i * 3 + (j + 1) % 3]];
			ea[ne] = a < b ? a : b;
			eb[ne] = a < b ? b : a;
			ne++;
		}
	for (i = 0; i < ne; i++) {
		int count = 0;
		for (j = 0; j < ne; j++)
			if (ea[j] == ea[i] && eb[j] == eb[i])
				count++;
		if (count != 2)
			open++;
	}
	free(id);
	free(ea);
	free(eb);
	return open;
}

static void run(int levels)
{
	UmkSubsurf *s = umk_subsurf_new(levels, UMK_WELD_DISTANCE_CM, 24, g_pos, 12, g_tri, g_uv);
	int i, nv, nt, bad_wind = 0, outside = 0, bad_uv = 0, bad_attr = 0;
	const float *p, *uv, *at;
	const unsigned *t;

	CHECK(s != NULL, "level %d: no result", levels);
	if (!s)
		return;
	CHECK(umk_subsurf_welded(s) == 16, "level %d: welded %d, want 16", levels, umk_subsurf_welded(s));
	CHECK(umk_subsurf_eval(s, g_pos, g_attr), "level %d: eval failed", levels);

	nv = umk_subsurf_num_verts(s);
	nt = umk_subsurf_num_tris(s);
	p = umk_subsurf_pos(s);
	uv = umk_subsurf_uv(s);
	at = umk_subsurf_attr(s);
	t = umk_subsurf_tris(s);

	CHECK(nt == 12 * 3 * (1 << (2 * (levels - 1))) * 2, "level %d: %d triangles", levels, nt);

	for (i = 0; i < nv; i++) {
		int k;
		for (k = 0; k < 3; k++)
			if (fabsf(p[i * 3 + k]) > 1.0f + 1e-5f)
				outside++;
		if (uv[i * 2] < -1e-5f || uv[i * 2] > 1.00001f || uv[i * 2 + 1] < -1e-5f || uv[i * 2 + 1] > 1.00001f)
			bad_uv++;
		if (at[i] < 25.0f - 1e-3f || at[i] > 175.0f + 1e-3f)
			bad_attr++;
	}
	/* winding: the output triangle's normal points away from the centre,
	 * as the cube's faces do */
	for (i = 0; i < nt; i++) {
		float n[3], c[3];
		const float *a = p + t[i * 3] * 3, *b = p + t[i * 3 + 1] * 3, *d = p + t[i * 3 + 2] * 3;
		face_normal(a, b, d, n);
		c[0] = a[0] + b[0] + d[0];
		c[1] = a[1] + b[1] + d[1];
		c[2] = a[2] + b[2] + d[2];
		if (n[0] * c[0] + n[1] * c[1] + n[2] * c[2] <= 0.0f)
			bad_wind++;
	}
	CHECK(outside == 0, "level %d: %d coordinates outside the cube", levels, outside);
	CHECK(bad_wind == 0, "level %d: %d triangles wound inwards", levels, bad_wind);
	CHECK(bad_uv == 0, "level %d: %d UVs out of [0,1]", levels, bad_uv);
	CHECK(bad_attr == 0, "level %d: %d corner values out of range", levels, bad_attr);
	{
		int open = open_edges(nv, p, nt, t);
		CHECK(open == 0, "level %d: %d open edges", levels, open);
	}
	printf("level %d: %d vertices, %d triangles\n", levels, nv, nt);
	umk_subsurf_free(s);
}

int main(void)
{
	UmkSubsurf *s;

	make_cube();
	run(1);
	run(2);

	/* without the weld the 6 faces stay apart: open along every seam */
	s = umk_subsurf_new(1, 0.0f, 24, g_pos, 12, g_tri, g_uv);
	CHECK(s != NULL, "no weld: no result");
	if (s) {
		int open;
		umk_subsurf_eval(s, g_pos, NULL);
		open = open_edges(umk_subsurf_num_verts(s), umk_subsurf_pos(s),
		                  umk_subsurf_num_tris(s), umk_subsurf_tris(s));
		CHECK(open > 0, "no weld: the cube came out closed");
		printf("no weld: %d open edges (expected: the seams)\n", open);
		umk_subsurf_free(s);
	}

	printf(g_fail ? "%d check(s) failed\n" : "all checks passed\n", g_fail);
	return g_fail != 0;
}
