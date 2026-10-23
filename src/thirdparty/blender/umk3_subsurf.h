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

/* One evaluation: nverts x 3 positions, and optionally one float per
 * triangle corner (ntris x 3, e.g. the light value) that is carried the way
 * Blender carries loop data through a subdivision. 0 on failure. */
int umk_subsurf_eval(UmkSubsurf *s, const float *pos, const float *corner_attr);

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
