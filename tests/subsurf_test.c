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
 *   - the surface stays near the cube: the correction pass (umk3_subsurf.c)
 *     undoes Catmull-Clark's shrink, so it may bulge a little, never far,
 *     and a corner of the cube is met within a quarter of the side
 *   - UVs and the per-corner value stay inside the range they started in
 *   - without the weld the cube stays open along its seams
 *
 * The cube above is made with the default sharp edges (80 degrees): its
 * right angles are kept. And SHARP EDGES (umk_subsurf_set_sharp), on
 * synthetic meshes:
 *   A  a cube with every edge sharp keeps its corners and edges
 *   B  a tube of 22.5-degree facets has no sharp edge at 45: the same
 *      surface as with none
 *   C  the threshold: a capped tube's sharp edges fall as it rises, and the
 *      shape changes with them
 *   D  levels 1 to 3: the cube's corner stays put at every level
 *   E  three sharp edges at a corner: the corner holds; strength 0.5 lies
 *      between none and full, and changing it on a mesh in use gives what a
 *      new mesh would
 *   F  an open, folded sheet: its fold is sharp, its rim untouched
 *   G  a degenerate triangle and an edge of three triangles: no crash, no
 *      NaN
 *   H  180 degrees and strength 0 give the same surface: the subdivision as
 *      it was before sharp edges
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
			if (fabsf(p[i * 3 + k]) > 1.5f)
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
	CHECK(outside == 0, "level %d: %d coordinates far outside the cube", levels, outside);
	{   /* the corner (1,1,1) is met closely after the correction */
		float best = 1e9f;
		for (i = 0; i < nv; i++) {
			float dx = p[i * 3] - 1.0f, dy = p[i * 3 + 1] - 1.0f, dz = p[i * 3 + 2] - 1.0f;
			float dd = sqrtf(dx * dx + dy * dy + dz * dz);
			if (dd < best) best = dd;
		}
		CHECK(best < 0.5f, "level %d: the corner is %.3f away", levels, best);
		printf("level %d: corner (1,1,1) met within %.3f\n", levels, best);
	}
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


/* ------------------------------------------------------------ sharp edges */

/* a mesh of triangles with a UV per corner */
struct mesh { int nv, nt; float pos[600 * 3]; unsigned short tri[600 * 3]; float uv[600 * 3 * 2]; };

static int add_v(struct mesh *m, float x, float y, float z)
{
	m->pos[m->nv * 3] = x; m->pos[m->nv * 3 + 1] = y; m->pos[m->nv * 3 + 2] = z;
	return m->nv++;
}

static void add_t(struct mesh *m, int a, int b, int c)
{
	int k;
	m->tri[m->nt * 3] = (unsigned short)a; m->tri[m->nt * 3 + 1] = (unsigned short)b; m->tri[m->nt * 3 + 2] = (unsigned short)c;
	for (k = 0; k < 3; k++) {
		m->uv[(m->nt * 3 + k) * 2] = (float)k * 0.5f;
		m->uv[(m->nt * 3 + k) * 2 + 1] = (float)(k == 2);
	}
	m->nt++;
}

/* a tube of n facets, radius 1, from z -1 to 1; caps: fans closing both ends */
static void make_tube(struct mesh *m, int n, int caps)
{
	int i;
	memset(m, 0, sizeof *m);
	for (i = 0; i < n; i++) {
		float a = 6.2831853f * (float)i / (float)n;
		add_v(m, cosf(a), sinf(a), -1.0f);
		add_v(m, cosf(a), sinf(a), 1.0f);
	}
	for (i = 0; i < n; i++) {
		int a = i * 2, b = ((i + 1) % n) * 2;
		add_t(m, a, b, b + 1);
		add_t(m, a, b + 1, a + 1);
	}
	if (caps) {
		int lo = add_v(m, 0, 0, -1.0f), hi = add_v(m, 0, 0, 1.0f);
		for (i = 0; i < n; i++) {
			int a = i * 2, b = ((i + 1) % n) * 2;
			add_t(m, lo, b, a);
			add_t(m, hi, a + 1, b + 1);
		}
	}
}

static UmkSubsurf *made(int levels, const struct mesh *m, float angle, float strength)
{
	UmkSubsurf *s = umk_subsurf_new(levels, UMK_WELD_DISTANCE_CM, m->nv, m->pos, m->nt, m->tri, m->uv);
	if (s) {
		umk_subsurf_set_sharp(s, angle, strength);
		umk_subsurf_eval(s, m->pos, NULL);
	}
	return s;
}

static float nearest(const UmkSubsurf *s, float x, float y, float z)
{
	const float *p = umk_subsurf_pos(s);
	float best = 1e9f;
	int i;
	for (i = 0; i < umk_subsurf_num_verts(s); i++) {
		float dx = p[i * 3] - x, dy = p[i * 3 + 1] - y, dz = p[i * 3 + 2] - z;
		float d = sqrtf(dx * dx + dy * dy + dz * dz);
		if (d < best) best = d;
	}
	return best;
}

static int all_finite(const UmkSubsurf *s)
{
	const float *p = umk_subsurf_pos(s);
	int i;
	for (i = 0; i < umk_subsurf_num_verts(s) * 3; i++)
		if (!isfinite(p[i]))
			return 0;
	return 1;
}

/* the largest difference between two surfaces of the same topology */
static float differ(const UmkSubsurf *a, const UmkSubsurf *b)
{
	const float *p = umk_subsurf_pos(a), *q = umk_subsurf_pos(b);
	float m = 0;
	int i;
	if (umk_subsurf_num_verts(a) != umk_subsurf_num_verts(b))
		return 1e9f;
	for (i = 0; i < umk_subsurf_num_verts(a) * 3; i++)
		if (fabsf(p[i] - q[i]) > m)
			m = fabsf(p[i] - q[i]);
	return m;
}

static void sharp_tests(void)
{
	static struct mesh cube, tube, capped, sheet, bad;
	UmkSubsurf *a, *b, *c;
	int i, lv;

	/* the cube of the checks above, as a mesh */
	memset(&cube, 0, sizeof cube);
	cube.nv = 24;
	cube.nt = 12;
	memcpy(cube.pos, g_pos, sizeof g_pos);
	memcpy(cube.tri, g_tri, sizeof g_tri);
	memcpy(cube.uv, g_uv, sizeof g_uv);

	/* A: every edge of the cube is 90 degrees; at 45 they are all sharp */
	a = made(2, &cube, 45.0f, 1.0f);
	b = made(2, &cube, 180.0f, 1.0f);
	CHECK(a && b, "A: no result");
	if (a && b) {
		float ca = nearest(a, 1, 1, 1), cb = nearest(b, 1, 1, 1);
		float ea = nearest(a, 1, 1, 0), eb = nearest(b, 1, 1, 0);
		CHECK(umk_subsurf_sharp_edges(a) == 12, "A: %d sharp edges, want the cube's 12", umk_subsurf_sharp_edges(a));
		CHECK(ca < 0.02f && ca < cb * 0.25f, "A: corner %.4f sharp vs %.4f smooth", ca, cb);
		CHECK(ea < 0.02f && ea < eb * 0.25f, "A: edge middle %.4f sharp vs %.4f smooth", ea, eb);
		printf("A: corner %.4f, edge middle %.4f (no sharp edges: %.4f, %.4f)\n", ca, ea, cb, eb);
	}
	umk_subsurf_free(a);
	umk_subsurf_free(b);

	/* B: 16 facets turn by 22.5 degrees: nothing sharp at 45, and the
	 * surface is the one without sharp edges */
	make_tube(&tube, 16, 0);
	a = made(2, &tube, 45.0f, 1.0f);
	b = made(2, &tube, 180.0f, 1.0f);
	CHECK(a && b, "B: no result");
	if (a && b) {
		CHECK(umk_subsurf_sharp_edges(a) == 0, "B: %d sharp edges on a smooth tube", umk_subsurf_sharp_edges(a));
		CHECK(differ(a, b) == 0.0f, "B: the smooth tube changed by %g", differ(a, b));
		printf("B: smooth tube, 0 sharp edges, the same surface\n");
	}
	umk_subsurf_free(a);
	umk_subsurf_free(b);

	/* C: the capped tube: 22.5 between facets, 90 at the rims */
	make_tube(&capped, 16, 1);
	{
		static const float thr[4] = { 10.0f, 45.0f, 100.0f, 180.0f };
		int n[4];
		UmkSubsurf *r[4];
		for (i = 0; i < 4; i++) {
			r[i] = made(2, &capped, thr[i], 1.0f);
			n[i] = r[i] ? umk_subsurf_sharp_edges(r[i]) : -1;
		}
		CHECK(n[0] == 48 && n[1] == 32 && n[2] == 0 && n[3] == 0,
		      "C: sharp edges %d %d %d %d at 10, 45, 100, 180; want 48 32 0 0", n[0], n[1], n[2], n[3]);
		CHECK(r[0] && r[1] && differ(r[0], r[1]) > 1e-3f, "C: 10 and 45 gave the same shape");
		CHECK(r[1] && r[2] && differ(r[1], r[2]) > 1e-3f, "C: 45 and 100 gave the same shape");
		CHECK(r[2] && r[3] && differ(r[2], r[3]) == 0.0f, "C: 100 and 180 differ");
		printf("C: sharp edges %d, %d, %d, %d at 10, 45, 100, 180 degrees\n", n[0], n[1], n[2], n[3]);
		for (i = 0; i < 4; i++)
			umk_subsurf_free(r[i]);
	}

	/* D: the corner at every level */
	for (lv = 1; lv <= 3; lv++) {
		a = made(lv, &cube, 45.0f, 1.0f);
		CHECK(a != NULL, "D: level %d no result", lv);
		if (a) {
			float d = nearest(a, 1, 1, 1);
			CHECK(d < 0.02f && all_finite(a), "D: level %d corner %.4f", lv, d);
			printf("D: level %d, corner within %.4f\n", lv, d);
		}
		umk_subsurf_free(a);
	}

	/* E: three sharp edges meet at each corner of the cube; half the
	 * strength lies between none and all of it -- and changing it on a
	 * mesh already evaluated (the slider) gives what a new one would */
	a = made(2, &cube, 45.0f, 0.0f);
	b = made(2, &cube, 45.0f, 0.5f);
	c = made(2, &cube, 45.0f, 1.0f);
	CHECK(a && b && c, "E: no result");
	if (a && b && c) {
		float d0 = nearest(a, 1, 1, 1), d5 = nearest(b, 1, 1, 1), d1 = nearest(c, 1, 1, 1);
		CHECK(d1 < d5 && d5 < d0, "E: corner %.4f / %.4f / %.4f at strength 1 / 0.5 / 0", d1, d5, d0);
		umk_subsurf_set_sharp(a, 45.0f, 1.0f);
		umk_subsurf_eval(a, cube.pos, NULL);
		CHECK(differ(a, c) < 1e-6f, "E: changed to full strength, differs by %g from a new one", differ(a, c));
		printf("E: corner %.4f / %.4f / %.4f at strength 1 / 0.5 / 0\n", d1, d5, d0);
	}
	umk_subsurf_free(a);
	umk_subsurf_free(b);
	umk_subsurf_free(c);

	/* F: two squares folded by 100 degrees along x = 0: an open sheet */
	memset(&sheet, 0, sizeof sheet);
	{
		float t = 100.0f * 3.14159265f / 180.0f;
		int v0 = add_v(&sheet, 0, -1, 0), v1 = add_v(&sheet, 0, 1, 0);
		int v2 = add_v(&sheet, 1, 1, 0), v3 = add_v(&sheet, 1, -1, 0);
		int v4 = add_v(&sheet, cosf(t), 1, sinf(t)), v5 = add_v(&sheet, cosf(t), -1, sinf(t));
		add_t(&sheet, v0, v3, v2); add_t(&sheet, v0, v2, v1);
		add_t(&sheet, v0, v1, v4); add_t(&sheet, v0, v4, v5);
	}
	a = made(2, &sheet, 70.0f, 1.0f);
	b = made(2, &sheet, 120.0f, 1.0f);
	CHECK(a && b, "F: no result");
	if (a && b) {
		CHECK(umk_subsurf_sharp_edges(a) == 1 && umk_subsurf_sharp_edges(b) == 0,
		      "F: %d / %d sharp edges at 70 / 120, want 1 / 0", umk_subsurf_sharp_edges(a), umk_subsurf_sharp_edges(b));
		CHECK(all_finite(a) && all_finite(b), "F: not finite");
		CHECK(nearest(a, 0, 0, 0) <= nearest(b, 0, 0, 0), "F: the fold was not kept");
		printf("F: open sheet, fold sharp at 70, not at 120\n");
	}
	umk_subsurf_free(a);
	umk_subsurf_free(b);

	/* G: a fin of three triangles on one edge, and one with no area */
	memset(&bad, 0, sizeof bad);
	{
		int e0 = add_v(&bad, 0, 0, 0), e1 = add_v(&bad, 0, 0, 1);
		int p0 = add_v(&bad, 1, 0, 0.5f), p1 = add_v(&bad, -1, 0.2f, 0.5f), p2 = add_v(&bad, 0, -1, 0.5f);
		int d = add_v(&bad, 2, 2, 2);
		add_t(&bad, e0, e1, p0); add_t(&bad, e1, e0, p1); add_t(&bad, e0, e1, p2);
		add_t(&bad, p0, d, d);
		add_t(&bad, p0, p1, d);
	}
	a = made(2, &bad, 0.0f, 1.0f);
	if (a) {
		CHECK(all_finite(a), "G: NaN or infinity in the result");
		printf("G: non-manifold and degenerate: %d sharp edges, all finite\n", umk_subsurf_sharp_edges(a));
	} else
		printf("G: non-manifold and degenerate: refused (NULL), no crash\n");
	umk_subsurf_free(a);

	/* H: no sharp edge by angle or by strength: the same surface */
	a = made(2, &cube, 180.0f, 1.0f);
	b = made(2, &cube, 45.0f, 0.0f);
	CHECK(a && b && differ(a, b) == 0.0f, "H: 180 degrees and strength 0 differ");
	CHECK(a && umk_subsurf_sharp_edges(a) == 0 && b && umk_subsurf_sharp_edges(b) == 0, "H: sharp edges where none should be");
	printf("H: 180 degrees and strength 0, the same surface\n");
	umk_subsurf_free(a);
	umk_subsurf_free(b);
}

int main(void)
{
	UmkSubsurf *s;

	make_cube();
	run(1);
	run(2);
	sharp_tests();

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
