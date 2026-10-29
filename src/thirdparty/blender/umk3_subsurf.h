/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * umk3_subsurf -- Blender's Weld and Subdivision Surface (Catmull-Clark)
 * modifiers, for a triangle mesh with one UV per triangle corner: the shape
 * the game's skinned fighters have (docs/formats/SKIN-FORMAT.md section 10).
 *
 * Built from Blender's own code, see umk3_subsurf.c for which part comes
 * from where. Plain C, no game or platform dependency: the game links it,
 * and the launcher's model viewer runs the same file as WebAssembly. */
#ifndef UMK3_SUBSURF_H
#define UMK3_SUBSURF_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct UmkSubsurf UmkSubsurf;

/* Blender's Weld modifier default, 0.001 m. The game's models are in
 * centimetres (a fighter is about 180 units tall), so 0.1 units. */
#define UMK_WELD_DISTANCE_CM 0.1f

/* The topology, fixed for the mesh's life:
 *   levels       subdivision levels, 1 or more (Blender's "Levels Viewport")
 *   weld_dist    merge distance, in the units of `pos`; 0 = no weld
 *   pos          nverts x 3 floats: the positions the weld is measured on
 *   tri          ntris x 3 vertex indices
 *   corner_uv    ntris x 3 x 2 floats, one UV per triangle corner
 * Returns NULL when the mesh has no triangle left to subdivide. */
UmkSubsurf *umk_subsurf_new(int levels, float weld_dist,
                            int nverts, const float *pos,
                            int ntris, const unsigned short *tri,
                            const float *corner_uv);

/* SHARP EDGES (port, not Blender's modifier): an interior edge whose two
 * triangles turn by more than `angle_deg` -- the angle between their
 * normals, 0 flat, 180 folded back on itself; an edge within 0.001 degree
 * of the threshold counts as at it, and at it is not sharp, so rounding in
 * acos cannot flip a right angle at 90 -- is kept by Catmull-Clark: Blender's edge crease, `strength` of
 * it (0..1; 0 the plain smoothing, 1 a crease that holds at every level,
 * between, Blender's semi-sharp crease). Where two or more sharp edges meet,
 * CCG's own crease and corner rules place the vertex. The angle is measured
 * once, on the positions the mesh was made with, so an edge does not change
 * class while a fighter moves. An open edge keeps its crease of 1 whatever
 * these say (umk_subsurf_new); an edge of three or more triangles, or of a
 * degenerate one, is never sharp. 180 or a strength of 0 is the subdivision
 * as it was before this existed.
 *
 * The defaults: the fighters are low-poly and organic -- over their 74,646
 * interior edges 38% turn by more than 30 degrees (Blender's auto smooth
 * angle, which would make a third of every body blocky), 17% by more than
 * 60, 9.6% by more than 80, 4.3% by more than 90. 80 keeps the folds of
 * armour plates, belts, shoulder pads and fingers, right angles included,
 * and smooths the rest. */
#define UMK_SHARP_ANGLE_DEFAULT     80.0f
#define UMK_SHARP_STRENGTH_DEFAULT  1.0f

/* Made with the defaults; this changes them (clamped to 0..180 and 0..1),
 * and the next umk_subsurf_eval subdivides with the new creases. */
void umk_subsurf_set_sharp(UmkSubsurf *s, float angle_deg, float strength);

/* How many interior edges are sharp now, for the log and the tests. */
int umk_subsurf_sharp_edges(const UmkSubsurf *s);

/* The correction pass (umk_subsurf_eval) per vertex: `w` is nverts floats,
 * 0..1, how much of it each input vertex gets (a welded vertex takes the
 * least of its members); NULL, all of it, as made. The pass moves the
 * control points so the surface passes through the model's vertices; between
 * them a convex part then bulges a little past the model's faces, and where a
 * rigid piece sits tight on the skin -- Kung Lao's hat on his head -- the
 * skin came through it. 0 there leaves Catmull-Clark's own surface, which
 * stays inside the model's (Mary: the hat, measured: 45 points of the head
 * through it at level 2, none with 0 under it). */
void umk_subsurf_set_fit(UmkSubsurf *s, const float *w);

/* One evaluation: nverts x 3 positions, and optionally one float per
 * triangle corner (ntris x 3, e.g. the light value) that is carried the way
 * Blender carries loop data through a subdivision. 0 on failure. */
int umk_subsurf_eval(UmkSubsurf *s, const float *pos, const float *corner_attr);
/* A pose subdivided before, its output positions put back: the next
 * umk_subsurf_eval with the same `pos` only spreads the attribute. */
int  umk_subsurf_is_last(const UmkSubsurf *s, const float *pos);
void umk_subsurf_restore(UmkSubsurf *s, const float *pos, const float *out_pos);

/* The result of the last evaluation. Vertices are unique per position and
 * UV; triangles keep the winding of the face they came from. */
int                 umk_subsurf_num_verts(const UmkSubsurf *s);
const float        *umk_subsurf_pos(const UmkSubsurf *s);    /* x 3 */
const float        *umk_subsurf_uv(const UmkSubsurf *s);     /* x 2 */
const float        *umk_subsurf_attr(const UmkSubsurf *s);   /* x 1 */
int                 umk_subsurf_num_tris(const UmkSubsurf *s);
const unsigned int *umk_subsurf_tris(const UmkSubsurf *s);   /* x 3 */

/* How many vertices the weld merged, for the log. */
int umk_subsurf_welded(const UmkSubsurf *s);

void umk_subsurf_free(UmkSubsurf *s);

#ifdef __cplusplus
}
#endif

#endif
