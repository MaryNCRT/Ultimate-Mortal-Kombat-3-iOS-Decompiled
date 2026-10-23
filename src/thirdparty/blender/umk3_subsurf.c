/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * umk3_subsurf.c -- Blender's Weld and Subdivision Surface modifiers for the
 * game's triangle meshes. See umk3_subsurf.h for the interface.
 *
 * Where each part comes from (Blender sources, GPL-2.0-or-later):
 *
 *   Weld, "All" mode, Blender 4.2.0
 *     blenlib/intern/kdtree_impl.h        BLI_kdtree_3d_new/insert/balance,
 *                                          kdtree_order, deduplicate_recursive,
 *                                          BLI_kdtree_3d_calc_duplicates_fast
 *     geometry/intern/mesh_merge_by_distance.cc
 *                                          mesh_merge_by_distance_all: the
 *                                          duplicate map with use_index_order;
 *                                          create_merged_mesh: a vertex that
 *                                          merges takes the mean of its group,
 *                                          a face left with a repeated vertex
 *                                          is removed, and of faces with the
 *                                          same vertices the first is kept
 *                                          (poly_find_doubles)
 *
 *   Subdivision Surface, Catmull-Clark, Blender 2.79b
 *     blenkernel/intern/CCGSubSurf*.c     unchanged, in this folder
 *     blenkernel/intern/subsurf_ccg.c     _getSubSurf, ss_sync_from_uv,
 *                                          ss_sync_ccg_from_derivedmesh,
 *                                          get_ss_weights, getFaceIndex,
 *                                          set_subsurf_legacy_uv (the quad
 *                                          corner order a, d, c, b)
 *     blenkernel/intern/mesh_mapping.c    BKE_mesh_uv_vert_map_create
 *     blenlib/intern/math_geom.c          cross_poly_v2
 *
 * The UV mesh is subdivided on its own, as Blender's subsurf does with
 * "Subdivide UVs", and per-corner data (the light) is carried with the face
 * weights Blender uses for loop custom data.
 *
 * What is not Blender: the glue -- arrays in, arrays out, vertices shared
 * between grids by their CCG element, triangles from the grid quads -- and
 * that the topology is synced once and only positions change per call. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "MEM_guardedalloc.h"
#include "BLI_utildefines.h"
#include "CCGSubSurf.h"
#include "umk3_subsurf.h"

#define SWAP(type, a, b) { type sw_ap; sw_ap = (a); (a) = (b); (b) = sw_ap; }
#define POINTER_FROM_INT(i) ((void *)(intptr_t)(i))
#define POINTER_AS_INT(p)   ((int)(intptr_t)(p))

/* BKE_mesh_mapping.h */
#define STD_UV_CONNECT_LIMIT 0.0001f

/* ======================================================= kdtree_impl.h, 3D */

#define KD_DIMS 3
#define KD_NODE_UNSET ((unsigned)-1)
#define KD_NODE_ROOT_IS_INIT ((unsigned)-2)

typedef struct KDTreeNode_head {
	unsigned left, right;
	float co[KD_DIMS];
	int index;
} KDTreeNode_head;

typedef struct KDTreeNode {
	unsigned left, right;
	float co[KD_DIMS];
	int index;
	unsigned d; /* range is only (0..KD_DIMS - 1) */
} KDTreeNode;

typedef struct KDTree {
	KDTreeNode *nodes;
	unsigned nodes_len;
	unsigned root;
	int max_node_index;
} KDTree;

static void copy_vn_vn(float v0[KD_DIMS], const float v1[KD_DIMS])
{
	for (unsigned j = 0; j < KD_DIMS; j++) {
		v0[j] = v1[j];
	}
}

static float square_f(float a)
{
	return a * a;
}

static float len_squared_vnvn(const float v0[KD_DIMS], const float v1[KD_DIMS])
{
	float d = 0.0f;
	for (unsigned j = 0; j < KD_DIMS; j++) {
		d += square_f(v0[j] - v1[j]);
	}
	return d;
}

static KDTree *kdtree_new(unsigned nodes_len_capacity)
{
	KDTree *tree;

	tree = MEM_mallocN(sizeof(KDTree), "KDTree");
	tree->nodes = MEM_mallocN(sizeof(KDTreeNode) * nodes_len_capacity, "KDTreeNode");
	tree->nodes_len = 0;
	tree->root = KD_NODE_ROOT_IS_INIT;
	tree->max_node_index = -1;
	return tree;
}

static void kdtree_free(KDTree *tree)
{
	if (tree) {
		MEM_freeN(tree->nodes);
		MEM_freeN(tree);
	}
}

static void kdtree_insert(KDTree *tree, int index, const float co[KD_DIMS])
{
	KDTreeNode *node = &tree->nodes[tree->nodes_len++];

	/* NOTE: array isn't calloc'd,
	 * need to initialize all struct members */

	node->left = node->right = KD_NODE_UNSET;
	copy_vn_vn(node->co, co);
	node->index = index;
	node->d = 0;
	tree->max_node_index = MAX2(tree->max_node_index, index);
}

static unsigned kdtree_balance(KDTreeNode *nodes, unsigned nodes_len, unsigned axis, const unsigned ofs)
{
	KDTreeNode *node;
	float co;
	unsigned left, right, median, i, j;

	if (nodes_len <= 0) {
		return KD_NODE_UNSET;
	}
	else if (nodes_len == 1) {
		return 0 + ofs;
	}

	/* Quick-sort style sorting around median. */
	left = 0;
	right = nodes_len - 1;
	median = nodes_len / 2;

	while (right > left) {
		co = nodes[right].co[axis];
		i = left - 1;
		j = right;

		while (1) {
			while (nodes[++i].co[axis] < co) { /* pass */
			}
			while (nodes[--j].co[axis] > co && j > left) { /* pass */
			}

			if (i >= j) {
				break;
			}

			SWAP(KDTreeNode_head, *(KDTreeNode_head *)&nodes[i], *(KDTreeNode_head *)&nodes[j]);
		}

		SWAP(KDTreeNode_head, *(KDTreeNode_head *)&nodes[i], *(KDTreeNode_head *)&nodes[right]);
		if (i >= median) {
			right = i - 1;
		}
		if (i <= median) {
			left = i + 1;
		}
	}

	/* Set node and sort sub-nodes. */
	node = &nodes[median];
	node->d = axis;
	axis = (axis + 1) % KD_DIMS;
	node->left = kdtree_balance(nodes, median, axis, ofs);
	node->right = kdtree_balance(
	        nodes + median + 1, (nodes_len - (median + 1)), axis, (median + 1) + ofs);

	return median + ofs;
}

static void kdtree_balance_tree(KDTree *tree)
{
	if (tree->root != KD_NODE_ROOT_IS_INIT) {
		for (unsigned i = 0; i < tree->nodes_len; i++) {
			tree->nodes[i].left = KD_NODE_UNSET;
			tree->nodes[i].right = KD_NODE_UNSET;
		}
	}

	tree->root = kdtree_balance(tree->nodes, tree->nodes_len, 0, 0);
}

/**
 * Use when we want to loop over nodes ordered by index.
 * Requires indices to be aligned with nodes.
 */
static int *kdtree_order(const KDTree *tree)
{
	const KDTreeNode *nodes = tree->nodes;
	const size_t bytes_num = sizeof(int) * (size_t)(tree->max_node_index + 1);
	int *order = MEM_mallocN(bytes_num, __func__);
	memset(order, -1, bytes_num);
	for (unsigned i = 0; i < tree->nodes_len; i++) {
		order[nodes[i].index] = (int)i;
	}
	return order;
}

struct DeDuplicateParams {
	/* Static */
	const KDTreeNode *nodes;
	float range;
	float range_sq;
	int *duplicates;
	int *duplicates_found;

	/* Per Search */
	float search_co[KD_DIMS];
	int search;
};

static void deduplicate_recursive(const struct DeDuplicateParams *p, unsigned i)
{
	const KDTreeNode *node = &p->nodes[i];
	if (p->search_co[node->d] + p->range <= node->co[node->d]) {
		if (node->left != KD_NODE_UNSET) {
			deduplicate_recursive(p, node->left);
		}
	}
	else if (p->search_co[node->d] - p->range >= node->co[node->d]) {
		if (node->right != KD_NODE_UNSET) {
			deduplicate_recursive(p, node->right);
		}
	}
	else {
		if ((p->search != node->index) && (p->duplicates[node->index] == -1)) {
			if (len_squared_vnvn(node->co, p->search_co) <= p->range_sq) {
				p->duplicates[node->index] = (int)p->search;
				*p->duplicates_found += 1;
			}
		}
		if (node->left != KD_NODE_UNSET) {
			deduplicate_recursive(p, node->left);
		}
		if (node->right != KD_NODE_UNSET) {
			deduplicate_recursive(p, node->right);
		}
	}
}

/* BLI_kdtree_3d_calc_duplicates_fast, with use_index_order as
 * mesh_merge_by_distance_all passes it (true). */
static int kdtree_calc_duplicates_fast(const KDTree *tree, const float range, int *duplicates)
{
	int found = 0;
	struct DeDuplicateParams p;

	memset(&p, 0, sizeof(p));
	p.nodes = tree->nodes;
	p.range = range;
	p.range_sq = square_f(range);
	p.duplicates = duplicates;
	p.duplicates_found = &found;

	{
		int *order = kdtree_order(tree);
		for (int i = 0; i < tree->max_node_index + 1; i++) {
			const int node_index = order[i];
			if (node_index == -1) {
				continue;
			}
			const int index = i;
			if (duplicates[index] == -1 || duplicates[index] == index) {
				p.search = index;
				copy_vn_vn(p.search_co, tree->nodes[node_index].co);
				int found_prev = found;
				deduplicate_recursive(&p, tree->root);
				if (found != found_prev) {
					/* Prevent chains of doubles. */
					duplicates[index] = index;
				}
			}
		}
		MEM_freeN(order);
	}
	return found;
}

/* ===================================================== math_geom.c, 2.79b */

static float cross_poly_v2(const float verts[][2], unsigned int nr)
{
	unsigned int a;
	float cross;
	const float *co_curr, *co_prev;

	/* The Trapezium Area Rule */
	co_prev = verts[nr - 1];
	co_curr = verts[0];
	cross = 0.0f;
	for (a = 0; a < nr; a++) {
		cross += (co_curr[0] - co_prev[0]) * (co_curr[1] + co_prev[1]);
		co_prev = co_curr;
		co_curr += 2;
	}

	return cross;
}

/* ================================================== mesh_mapping.c, 2.79b
 *
 * BKE_mesh_uv_vert_map_create for a mesh of triangles (totloop 3 for every
 * poly, loopstart = 3 * poly), selected = false, use_winding = true -- the
 * call ss_sync_from_uv makes. */

typedef struct UvMapVert {
	struct UvMapVert *next;
	unsigned int f;
	unsigned char tfindex, separate, flag;
} UvMapVert;

typedef struct UvVertMap {
	struct UvMapVert **vert;
	struct UvMapVert *buf;
} UvVertMap;

static void uv_vert_map_free(UvVertMap *vmap)
{
	if (vmap) {
		if (vmap->vert) MEM_freeN(vmap->vert);
		if (vmap->buf) MEM_freeN(vmap->buf);
		MEM_freeN(vmap);
	}
}

static UvVertMap *uv_vert_map_create(const int *loop_v, const float (*mloopuv)[2],
                                     unsigned int totpoly, unsigned int totvert,
                                     const float limit[2])
{
	UvVertMap *vmap;
	UvMapVert *buf;
	unsigned int a;
	int i, totuv, nverts;
	bool *winding;

	totuv = (int)totpoly * 3;

	if (totuv == 0)
		return NULL;

	vmap = (UvVertMap *)MEM_callocN(sizeof(*vmap), "UvVertMap");
	buf = vmap->buf = (UvMapVert *)MEM_callocN(sizeof(*vmap->buf) * (size_t)totuv, "UvMapVert");
	vmap->vert = (UvMapVert **)MEM_callocN(sizeof(*vmap->vert) * totvert, "UvMapVert*");
	winding = MEM_callocN(sizeof(*winding) * totpoly, "winding");

	if (!vmap->vert || !vmap->buf || !winding) {
		if (winding) MEM_freeN(winding);
		uv_vert_map_free(vmap);
		return NULL;
	}

	for (a = 0; a < totpoly; a++) {
		float tf_uv[3][2];

		nverts = 3;

		for (i = 0; i < nverts; i++) {
			buf->tfindex = (unsigned char)i;
			buf->f = a;
			buf->separate = 0;
			buf->next = vmap->vert[loop_v[a * 3 + i]];
			vmap->vert[loop_v[a * 3 + i]] = buf;

			tf_uv[i][0] = mloopuv[a * 3 + i][0];
			tf_uv[i][1] = mloopuv[a * 3 + i][1];

			buf++;
		}

		winding[a] = cross_poly_v2((const float (*)[2])tf_uv, (unsigned int)nverts) > 0;
	}

	/* sort individual uvs for each vert */
	for (a = 0; a < totvert; a++) {
		UvMapVert *newvlist = NULL, *vlist = vmap->vert[a];
		UvMapVert *iterv, *v, *lastv, *next;
		const float *uv, *uv2;

		while (vlist) {
			v = vlist;
			vlist = vlist->next;
			v->next = newvlist;
			newvlist = v;

			uv = mloopuv[v->f * 3 + v->tfindex];
			lastv = NULL;
			iterv = vlist;

			while (iterv) {
				next = iterv->next;

				uv2 = mloopuv[iterv->f * 3 + iterv->tfindex];

				if (fabsf(uv[0] - uv2[0]) < limit[0] && fabsf(uv[1] - uv2[1]) < limit[1] &&
				    (winding[iterv->f] == winding[v->f]))
				{
					if (lastv) lastv->next = next;
					else vlist = next;
					iterv->next = newvlist;
					newvlist = iterv;
				}
				else
					lastv = iterv;

				iterv = next;
			}

			newvlist->separate = 1;
		}

		vmap->vert[a] = newvlist;
	}

	MEM_freeN(winding);

	return vmap;
}

/* ==================================================== subsurf_ccg.c, 2.79b */

static CCGSubSurf *getSubSurf(int subdivLevels, int numLayers)
{
	CCGMeshIFC ifc;
	CCGSubSurf *ccgSS;

	/* (subdivLevels == 0) is not allowed */
	subdivLevels = MAX2(subdivLevels, 1);

	/* no aging, no arena, no normals, no mask */
	ifc.vertUserSize = ifc.edgeUserSize = ifc.faceUserSize = 8;
	ifc.numLayers = numLayers;
	ifc.vertDataSize = sizeof(float) * numLayers;
	ifc.simpleSubdiv = 0;

	ccgSS = ccgSubSurf_new(&ifc, subdivLevels, NULL, NULL);
	ccgSubSurf_setCalcVertexNormals(ccgSS, 0, 0);

	return ccgSS;
}

/* getFaceIndex, with the element bases taken from the handles this file
 * gives them (vertex i, edge i, face i, in order) instead of user data. */
typedef struct ElemBases {
	int numVerts;
	int edgeBase0;              /* numVerts */
	int *faceBase;              /* per face handle */
} ElemBases;

static int vertIndex(CCGVert *v)
{
	return POINTER_AS_INT(ccgSubSurf_getVertVertHandle(v));
}

static int edgeBase(const ElemBases *b, CCGEdge *e, int edgeSize)
{
	return b->edgeBase0 + POINTER_AS_INT(ccgSubSurf_getEdgeEdgeHandle(e)) * (edgeSize - 2);
}

static int getFaceIndex(const ElemBases *b, CCGFace *f, int S, int x, int y, int edgeSize, int gridSize)
{
	int faceBase = b->faceBase[POINTER_AS_INT(ccgSubSurf_getFaceFaceHandle(f))];
	int numVerts = ccgSubSurf_getFaceNumVerts(f);

	if (x == gridSize - 1 && y == gridSize - 1) {
		CCGVert *v = ccgSubSurf_getFaceVert(f, S);
		return vertIndex(v);
	}
	else if (x == gridSize - 1) {
		CCGVert *v = ccgSubSurf_getFaceVert(f, S);
		CCGEdge *e = ccgSubSurf_getFaceEdge(f, S);
		int eb = edgeBase(b, e, edgeSize);
		if (v == ccgSubSurf_getEdgeVert0(e)) {
			return eb + (gridSize - 1 - y) - 1;
		}
		else {
			return eb + (edgeSize - 2 - 1) - ((gridSize - 1 - y) - 1);
		}
	}
	else if (y == gridSize - 1) {
		CCGVert *v = ccgSubSurf_getFaceVert(f, S);
		CCGEdge *e = ccgSubSurf_getFaceEdge(f, (S + numVerts - 1) % numVerts);
		int eb = edgeBase(b, e, edgeSize);
		if (v == ccgSubSurf_getEdgeVert0(e)) {
			return eb + (gridSize - 1 - x) - 1;
		}
		else {
			return eb + (edgeSize - 2 - 1) - ((gridSize - 1 - x) - 1);
		}
	}
	else if (x == 0 && y == 0) {
		return faceBase;
	}
	else if (x == 0) {
		S = (S + numVerts - 1) % numVerts;
		return faceBase + 1 + (gridSize - 2) * S + (y - 1);
	}
	else if (y == 0) {
		return faceBase + 1 + (gridSize - 2) * S + (x - 1);
	}
	else {
		return faceBase + 1 + (gridSize - 2) * numVerts + S * (gridSize - 2) * (gridSize - 2) + (y - 1) * (gridSize - 2) + (x - 1);
	}
}

static void elem_bases_init(ElemBases *b, int numVerts, int numEdges, int numFaces,
                            const int *faceLen, int edgeSize, int gridSize)
{
	int i, base;

	b->numVerts = numVerts;
	b->edgeBase0 = numVerts;
	b->faceBase = MEM_mallocN(sizeof(int) * (size_t)(numFaces > 0 ? numFaces : 1), "faceBase");
	base = numVerts + numEdges * (edgeSize - 2);
	for (i = 0; i < numFaces; i++) {
		int n = faceLen ? faceLen[i] : 3;
		b->faceBase[i] = base;
		base += 1 + n * (gridSize - 2) + n * (gridSize - 2) * (gridSize - 2);
	}
}

/* get_ss_weights, for one face length (built x-major, read y-major, both as
 * Blender does -- see umk_subsurf_eval). */
static float *get_ss_weights(int gridCuts, int faceLen)
{
	int x, y, i, j;
	float *w, *w0, w1, w2, w4, fac, fac2, fx, fy;

	w0 = w = MEM_callocN(sizeof(float) * faceLen * faceLen * (gridCuts + 2) * (gridCuts + 2), "weight table alloc");
	fac = 1.0f / (float)faceLen;

	for (i = 0; i < faceLen; i++) {
		for (x = 0; x < gridCuts + 2; x++) {
			for (y = 0; y < gridCuts + 2; y++) {
				fx = 0.5f - (float)x / (float)(gridCuts + 1) / 2.0f;
				fy = 0.5f - (float)y / (float)(gridCuts + 1) / 2.0f;

				fac2 = faceLen - 4;
				w1 = (1.0f - fx) * (1.0f - fy) + (-fac2 * fx * fy * fac);
				w2 = (1.0f - fx + fac2 * fx * -fac) * (fy);
				w4 = (fx) * (1.0f - fy + -fac2 * fy * fac);

				/* these values aren't used for tri's and cause divide by zero */
				if (faceLen > 3) {
					fac2 = 1.0f - (w1 + w2 + w4);
					fac2 = fac2 / (float)(faceLen - 3);
					for (j = 0; j < faceLen; j++) {
						w[j] = fac2;
					}
				}

				w[i] = w1;
				w[(i - 1 + faceLen) % faceLen] = w2;
				w[(i + 1) % faceLen] = w4;

				w += faceLen;
			}
		}
	}

	return w0;
}

/* ================================================================== glue */

typedef struct EdgeKey {
	int v0, v1;
} EdgeKey;

struct UmkSubsurf {
	int levels;

	/* the weld */
	int nverts;                 /* input */
	int *weld_map;              /* input vertex -> welded vertex */
	int nwelded;
	int *group_count;           /* welded vertex -> members */
	int merged;

	/* the welded triangles that survive */
	int nfaces;
	int *face_src;              /* kept face -> input triangle */
	int *face_v;                /* x 3, welded vertices */
	int nedges;
	EdgeKey *edges;
	float *edge_crease;         /* per edge, Blender's crease * creaseFactor */
	float *fit_base;            /* scratch, nwelded x 3 */
	int ntris;                  /* input triangles */
	float *last_pos, *last_attr;/* the last call's input, to skip a repeat */
	int have_last, last_had_attr;

	CCGSubSurf *ss;             /* positions, 3 layers */
	CCGSubSurf *uvss;           /* UVs, 2 layers, synced once */
	float *weights;             /* get_ss_weights for triangles */
	int edgeSize, gridSize;

	/* the output, in input-face order */
	int out_nverts, out_ntris;
	int *out_ref;               /* x 4: face, S, x, y of each output vertex */
	float *out_pos, *out_uv, *out_attr;
	unsigned int *out_tri;
	float *welded_pos;          /* scratch, nwelded x 3 */
	CCGFace **faceMap;          /* positions ss, by face handle */
	CCGFace **uvFaceMap;
};

static int edge_lookup_or_add(EdgeKey **edges, int *n, int *cap, int **hash, int *hcap, int a, int b)
{
	unsigned h;
	int v0 = a < b ? a : b, v1 = a < b ? b : a;

	if (*n * 2 >= *hcap) {
		int i, ncap = *hcap ? *hcap * 2 : 1024;
		int *nh = MEM_mallocN(sizeof(int) * (size_t)ncap, "edge hash");
		for (i = 0; i < ncap; i++) nh[i] = -1;
		for (i = 0; i < *n; i++) {
			unsigned k = ((unsigned)(*edges)[i].v0 * 2654435761u ^ (unsigned)(*edges)[i].v1 * 40503u) & (unsigned)(ncap - 1);
			while (nh[k] != -1) k = (k + 1) & (unsigned)(ncap - 1);
			nh[k] = i;
		}
		if (*hash) MEM_freeN(*hash);
		*hash = nh;
		*hcap = ncap;
	}
	h = ((unsigned)v0 * 2654435761u ^ (unsigned)v1 * 40503u) & (unsigned)(*hcap - 1);
	while ((*hash)[h] != -1) {
		EdgeKey *e = &(*edges)[(*hash)[h]];
		if (e->v0 == v0 && e->v1 == v1)
			return (*hash)[h];
		h = (h + 1) & (unsigned)(*hcap - 1);
	}
	if (*n == *cap) {
		*cap = *cap ? *cap * 2 : 1024;
		*edges = MEM_reallocN(*edges, sizeof(EdgeKey) * (size_t)*cap);
	}
	(*edges)[*n].v0 = v0;
	(*edges)[*n].v1 = v1;
	(*hash)[h] = *n;
	return (*n)++;
}

/* Only the positions changed: CCG's partial sync (ccgSubSurf_initPartialSync,
 * the path Blender's own edit-mode updates take) re-sends the vertices; the
 * edges and faces of the full sync stay, so does the face map, and nothing
 * is allocated. An unchanged vertex is skipped by syncVert itself. */
static int sync_positions_partial(UmkSubsurf *s)
{
	int i;

	if (ccgSubSurf_initPartialSync(s->ss) != eCCGError_None)
		return 0;
	for (i = 0; i < s->nwelded; i++) {
		CCGVert *v;
		ccgSubSurf_syncVert(s->ss, POINTER_FROM_INT(i), s->welded_pos + i * 3, 0, &v);
	}
	return ccgSubSurf_processSync(s->ss) == eCCGError_None;
}

/* ss_sync_ccg_from_derivedmesh: the full sync, once, when the mesh is made. */
static int sync_positions(UmkSubsurf *s)
{
	CCGVertHDL fVerts[3];
	int i, j;

	ccgSubSurf_initFullSync(s->ss);

	for (i = 0; i < s->nwelded; i++) {
		CCGVert *v;
		ccgSubSurf_syncVert(s->ss, POINTER_FROM_INT(i), s->welded_pos + i * 3, 0, &v);
	}

	for (i = 0; i < s->nedges; i++) {
		CCGEdge *e;
		/* ss_sync_ccg_from_derivedmesh: me->crease * creaseFactor / 255 --
		 * the open edges at 255, the rest at 0 (see umk_subsurf_new) */
		ccgSubSurf_syncEdge(s->ss, POINTER_FROM_INT(i), POINTER_FROM_INT(s->edges[i].v0),
		                    POINTER_FROM_INT(s->edges[i].v1), s->edge_crease[i], &e);
	}

	for (i = 0; i < s->nfaces; i++) {
		CCGFace *f;

		for (j = 0; j < 3; j++) {
			fVerts[j] = POINTER_FROM_INT(s->face_v[i * 3 + j]);
		}

		if (ccgSubSurf_syncFace(s->ss, POINTER_FROM_INT(i), 3, fVerts, &f) == eCCGError_InvalidValue) {
			return 0;
		}
	}

	ccgSubSurf_processSync(s->ss);
	return 1;
}

/* ss_sync_from_uv, for triangles. Returns the number of UV vertices. */
static int sync_uvs(UmkSubsurf *s, const float (*face_uv)[2])
{
	UvVertMap *vmap;
	UvMapVert *v;
	float limit[2];
	int *loop_v, *loop_uvvert;
	int i, j, seam, nuv = 0;
	EdgeKey *edges = NULL;
	int nedges = 0, cap = 0, *hash = NULL, hcap = 0;
	float uv[3] = {0.0f, 0.0f, 0.0f}; /* only first 2 values are written into */

	limit[0] = limit[1] = STD_UV_CONNECT_LIMIT;

	loop_v = MEM_mallocN(sizeof(int) * (size_t)s->nfaces * 3, "loop_v");
	for (i = 0; i < s->nfaces * 3; i++)
		loop_v[i] = s->face_v[i];

	vmap = uv_vert_map_create(loop_v, face_uv, (unsigned)s->nfaces, (unsigned)s->nwelded, limit);
	if (!vmap) {
		MEM_freeN(loop_v);
		return 0;
	}

	/* the UV vertex of each loop: the loop that starts its "separate" run */
	loop_uvvert = MEM_mallocN(sizeof(int) * (size_t)s->nfaces * 3, "loop_uvvert");

	ccgSubSurf_initFullSync(s->uvss);

	/* create vertices */
	for (i = 0; i < s->nwelded; i++) {
		if (!vmap->vert[i])
			continue;

		for (v = vmap->vert[i]->next; v; v = v->next)
			if (v->separate)
				break;

		seam = (v != NULL);

		for (v = vmap->vert[i]; v; v = v->next) {
			if (v->separate) {
				CCGVert *ssv;
				int loopid = (int)v->f * 3 + v->tfindex;
				UvMapVert *w;

				uv[0] = face_uv[loopid][0];
				uv[1] = face_uv[loopid][1];

				ccgSubSurf_syncVert(s->uvss, POINTER_FROM_INT(nuv), uv, seam, &ssv);

				/* get_face_uv_map_vert: every loop of this run maps here */
				loop_uvvert[loopid] = nuv;
				for (w = v->next; w && !w->separate; w = w->next)
					loop_uvvert[w->f * 3 + w->tfindex] = nuv;
				nuv++;
			}
		}
	}

	/* create edges */
	for (i = 0; i < s->nfaces; i++) {
		int jn;
		for (j = 0, jn = 2; j < 3; jn = j++) {
			int v0 = loop_uvvert[i * 3 + jn];
			int v1 = loop_uvvert[i * 3 + j];
			int before = nedges;
			int e = edge_lookup_or_add(&edges, &nedges, &cap, &hash, &hcap, v0, v1);

			if (nedges != before) {
				CCGEdge *ce;
				/* crease = ccgSubSurf_getEdgeCrease(orige): 0, no creases */
				ccgSubSurf_syncEdge(s->uvss, POINTER_FROM_INT(e), POINTER_FROM_INT(v0),
				                    POINTER_FROM_INT(v1), 0.0f, &ce);
			}
		}
	}

	/* create faces */
	for (i = 0; i < s->nfaces; i++) {
		CCGVertHDL fverts[3];
		CCGFace *f;

		for (j = 0; j < 3; j++)
			fverts[j] = POINTER_FROM_INT(loop_uvvert[i * 3 + j]);
		ccgSubSurf_syncFace(s->uvss, POINTER_FROM_INT(i), 3, fverts, &f);
	}

	uv_vert_map_free(vmap);
	ccgSubSurf_processSync(s->uvss);

	if (hash) MEM_freeN(hash);
	if (edges) MEM_freeN(edges);
	MEM_freeN(loop_uvvert);
	MEM_freeN(loop_v);
	return nuv;
}

static void face_maps(CCGSubSurf *ss, CCGFace **map)
{
	CCGFaceIterator fi;

	for (ccgSubSurf_initFaceIterator(ss, &fi); !ccgFaceIterator_isStopped(&fi); ccgFaceIterator_next(&fi)) {
		CCGFace *f = ccgFaceIterator_getCurrent(&fi);
		map[POINTER_AS_INT(ccgSubSurf_getFaceFaceHandle(f))] = f;
	}
}

/* The output vertices: one per pair (position element, UV element), found
 * through getFaceIndex on both meshes; and the triangles of every grid quad,
 * a, d, c, b as set_subsurf_legacy_uv reads them. */
static int build_output(UmkSubsurf *s, int nuv)
{
	ElemBases pb, ub;
	int gridSize = s->gridSize, edgeSize = s->edgeSize, gridFaces = gridSize - 1;
	int64_t keymax;
	int i, S, x, y, n = 0, t = 0, cap, hcap;
	int64_t *hkey;
	int *hval, *idx;

	elem_bases_init(&pb, s->nwelded, s->nedges, s->nfaces, NULL, edgeSize, gridSize);
	elem_bases_init(&ub, nuv, ccgSubSurf_getNumEdges(s->uvss), s->nfaces, NULL, edgeSize, gridSize);
	keymax = (int64_t)ub.faceBase[0] + (int64_t)s->nfaces * (1 + 3 * (gridSize - 2) + 3 * (gridSize - 2) * (gridSize - 2)) + 1;

	cap = s->nfaces * 3 * gridSize * gridSize;
	hcap = 1;
	while (hcap < cap * 2) hcap <<= 1;
	hkey = MEM_mallocN(sizeof(int64_t) * (size_t)hcap, "out key");
	hval = MEM_mallocN(sizeof(int) * (size_t)hcap, "out val");
	for (i = 0; i < hcap; i++) hkey[i] = -1;

	s->out_ref = MEM_mallocN(sizeof(int) * 4 * (size_t)cap, "out_ref");
	s->out_tri = MEM_mallocN(sizeof(unsigned int) * 6 * (size_t)s->nfaces * 3 * gridFaces * gridFaces, "out_tri");
	idx = MEM_mallocN(sizeof(int) * (size_t)gridSize * gridSize, "grid idx");

	for (i = 0; i < s->nfaces; i++) {
		CCGFace *f = s->faceMap[i], *uf = s->uvFaceMap[i];

		for (S = 0; S < 3; S++) {
			for (y = 0; y < gridSize; y++) {
				for (x = 0; x < gridSize; x++) {
					int64_t key = (int64_t)getFaceIndex(&pb, f, S, x, y, edgeSize, gridSize) * keymax
					            + getFaceIndex(&ub, uf, S, x, y, edgeSize, gridSize);
					unsigned h = (unsigned)((uint64_t)key * 0x9E3779B97F4A7C15ull >> 32) & (unsigned)(hcap - 1);

					while (hkey[h] != -1 && hkey[h] != key)
						h = (h + 1) & (unsigned)(hcap - 1);
					if (hkey[h] == -1) {
						hkey[h] = key;
						hval[h] = n;
						s->out_ref[n * 4 + 0] = i;
						s->out_ref[n * 4 + 1] = S;
						s->out_ref[n * 4 + 2] = x;
						s->out_ref[n * 4 + 3] = y;
						n++;
					}
					idx[y * gridSize + x] = hval[h];
				}
			}

			for (y = 0; y < gridFaces; y++) {
				for (x = 0; x < gridFaces; x++) {
					unsigned a = (unsigned)idx[(y + 0) * gridSize + x + 0];
					unsigned b = (unsigned)idx[(y + 0) * gridSize + x + 1];
					unsigned c = (unsigned)idx[(y + 1) * gridSize + x + 1];
					unsigned d = (unsigned)idx[(y + 1) * gridSize + x + 0];

					s->out_tri[t++] = a; s->out_tri[t++] = d; s->out_tri[t++] = c;
					s->out_tri[t++] = a; s->out_tri[t++] = c; s->out_tri[t++] = b;
				}
			}
		}
	}

	s->out_nverts = n;
	s->out_ntris = t / 3;
	s->out_pos = MEM_mallocN(sizeof(float) * 3 * (size_t)n, "out_pos");
	s->out_uv = MEM_mallocN(sizeof(float) * 2 * (size_t)n, "out_uv");
	s->out_attr = MEM_callocN(sizeof(float) * (size_t)n, "out_attr");

	/* the UVs do not change: read them once */
	for (i = 0; i < n; i++) {
		const int *r = s->out_ref + i * 4;
		const float *uv = ccgSubSurf_getFaceGridData(s->uvss, s->uvFaceMap[r[0]], r[1], r[2], r[3]);
		s->out_uv[i * 2 + 0] = uv[0];
		s->out_uv[i * 2 + 1] = uv[1];
	}

	MEM_freeN(idx);
	MEM_freeN(hkey);
	MEM_freeN(hval);
	MEM_freeN(pb.faceBase);
	MEM_freeN(ub.faceBase);
	return n > 0;
}

static void weld_positions(UmkSubsurf *s, const float *pos)
{
	int i;

	memset(s->welded_pos, 0, sizeof(float) * 3 * (size_t)s->nwelded);
	for (i = 0; i < s->nverts; i++) {
		float *d = s->welded_pos + s->weld_map[i] * 3;
		d[0] += pos[i * 3 + 0];
		d[1] += pos[i * 3 + 1];
		d[2] += pos[i * 3 + 2];
	}
	/* create_merged_mesh with do_mix_vert_data: the mean of the group */
	for (i = 0; i < s->nwelded; i++) {
		float k = 1.0f / (float)s->group_count[i];
		s->welded_pos[i * 3 + 0] *= k;
		s->welded_pos[i * 3 + 1] *= k;
		s->welded_pos[i * 3 + 2] *= k;
	}
}

UmkSubsurf *umk_subsurf_new(int levels, float weld_dist,
                            int nverts, const float *pos,
                            int ntris, const unsigned short *tri,
                            const float *corner_uv)
{
	UmkSubsurf *s;
	int *dup, i, j, n;
	float (*face_uv)[2];
	int *linked_off, *linked;       /* faces by welded vertex, for doubles */
	unsigned char *dead, *in_ctx, *face_ctx;
	EdgeKey *edges = NULL;
	int nedges = 0, cap = 0, *hash = NULL, hcap = 0, nuv;

	if (levels < 1 || nverts <= 0 || ntris <= 0)
		return NULL;

	s = MEM_callocN(sizeof(*s), "UmkSubsurf");
	s->levels = levels;
	s->nverts = nverts;

	/* ---- the weld: mesh_merge_by_distance_all ---- */
	dup = MEM_mallocN(sizeof(int) * (size_t)nverts, "vert_dest_map");
	for (i = 0; i < nverts; i++)
		dup[i] = -1;                    /* OUT_OF_CONTEXT */
	if (weld_dist > 0.0f) {
		KDTree *tree = kdtree_new((unsigned)nverts);
		for (i = 0; i < nverts; i++)
			kdtree_insert(tree, i, pos + i * 3);
		kdtree_balance_tree(tree);
		s->merged = kdtree_calc_duplicates_fast(tree, weld_dist, dup);
		kdtree_free(tree);
	}

	/* surviving vertices keep their order; a merged one joins its target */
	/* in the weld's context: a vertex it merged, or merged others into
	 * (vert_dest_map != OUT_OF_CONTEXT) -- only faces touching one are
	 * looked at for doubles, as weld_poly_loop_ctx_alloc does */
	in_ctx = MEM_callocN((size_t)nverts, "in_ctx");
	for (i = 0; i < nverts; i++)
		in_ctx[i] = dup[i] != -1;

	s->weld_map = MEM_mallocN(sizeof(int) * (size_t)nverts, "weld_map");
	n = 0;
	for (i = 0; i < nverts; i++)
		if (dup[i] == -1 || dup[i] == i)
			s->weld_map[i] = n++;
	for (i = 0; i < nverts; i++)
		if (!(dup[i] == -1 || dup[i] == i))
			s->weld_map[i] = s->weld_map[dup[i]];
	s->nwelded = n;
	s->group_count = MEM_callocN(sizeof(int) * (size_t)n, "group_count");
	for (i = 0; i < nverts; i++)
		s->group_count[s->weld_map[i]]++;
	MEM_freeN(dup);

	/* ---- faces: a repeated vertex kills the face ---- */
	s->face_src = MEM_mallocN(sizeof(int) * (size_t)ntris, "face_src");
	s->face_v = MEM_mallocN(sizeof(int) * 3 * (size_t)ntris, "face_v");
	dead = MEM_callocN((size_t)ntris, "dead");
	for (i = 0; i < ntris; i++) {
		int a = s->weld_map[tri[i * 3 + 0]];
		int b = s->weld_map[tri[i * 3 + 1]];
		int c = s->weld_map[tri[i * 3 + 2]];
		if (a == b || b == c || c == a)
			dead[i] = 1;
	}

	/* ---- poly_find_doubles: of faces with the same vertices the first
	 * stays (for triangles, the same set in any order) -- among the faces in
	 * the weld's context only. A game model has faces drawn both ways
	 * (cloth: the same three corners, front and back); outside the weld
	 * they are kept, as Blender keeps them. Removing them all opened holes
	 * in the fighters (Mary). ---- */
	face_ctx = MEM_callocN((size_t)ntris, "face_ctx");
	for (i = 0; i < ntris; i++)
		face_ctx[i] = in_ctx[tri[i * 3 + 0]] || in_ctx[tri[i * 3 + 1]] || in_ctx[tri[i * 3 + 2]];
	linked_off = MEM_callocN(sizeof(int) * (size_t)(n + 1), "linked_off");
	for (i = 0; i < ntris; i++)
		if (!dead[i])
			for (j = 0; j < 3; j++)
				linked_off[s->weld_map[tri[i * 3 + j]] + 1]++;
	for (i = 0; i < n; i++)
		linked_off[i + 1] += linked_off[i];
	linked = MEM_mallocN(sizeof(int) * (size_t)(linked_off[n] > 0 ? linked_off[n] : 1), "linked");
	{
		int *fill = MEM_mallocN(sizeof(int) * (size_t)(n > 0 ? n : 1), "fill");
		memcpy(fill, linked_off, sizeof(int) * (size_t)n);
		for (i = 0; i < ntris; i++)     /* ascending face order per vertex */
			if (!dead[i])
				for (j = 0; j < 3; j++) {
					int v = s->weld_map[tri[i * 3 + j]];
					linked[fill[v]++] = i;
				}
		MEM_freeN(fill);
	}
	for (i = 0; i < ntris; i++) {
		int a, b, c, k;
		if (dead[i] || !face_ctx[i])
			continue;
		a = s->weld_map[tri[i * 3 + 0]];
		b = s->weld_map[tri[i * 3 + 1]];
		c = s->weld_map[tri[i * 3 + 2]];
		for (k = linked_off[a]; k < linked_off[a + 1]; k++) {
			int g = linked[k], ga, gb, gc;
			if (g <= i || dead[g] || !face_ctx[g])
				continue;
			ga = s->weld_map[tri[g * 3 + 0]];
			gb = s->weld_map[tri[g * 3 + 1]];
			gc = s->weld_map[tri[g * 3 + 2]];
			if ((ga == a || ga == b || ga == c) && (gb == a || gb == b || gb == c)
			    && (gc == a || gc == b || gc == c))
				dead[g] = 1;
		}
	}
	MEM_freeN(linked);
	MEM_freeN(linked_off);
	MEM_freeN(face_ctx);
	MEM_freeN(in_ctx);

	face_uv = MEM_mallocN(sizeof(float) * 6 * (size_t)ntris, "face_uv");
	for (i = 0; i < ntris; i++) {
		if (dead[i])
			continue;
		for (j = 0; j < 3; j++) {
			s->face_v[s->nfaces * 3 + j] = s->weld_map[tri[i * 3 + j]];
			face_uv[s->nfaces * 3 + j][0] = corner_uv[(i * 3 + j) * 2 + 0];
			face_uv[s->nfaces * 3 + j][1] = corner_uv[(i * 3 + j) * 2 + 1];
		}
		s->face_src[s->nfaces++] = i;
	}
	MEM_freeN(dead);

	if (s->nfaces == 0) {
		MEM_freeN(face_uv);
		umk_subsurf_free(s);
		return NULL;
	}

	/* the mesh's edges, in first-seen order */
	for (i = 0; i < s->nfaces; i++)
		for (j = 0; j < 3; j++)
			edge_lookup_or_add(&edges, &nedges, &cap, &hash, &hcap,
			                   s->face_v[i * 3 + j], s->face_v[i * 3 + (j + 1) % 3]);
	if (hash) MEM_freeN(hash);
	s->edges = edges;
	s->nedges = nedges;

	/* Port, not Blender's choice of data: an edge with one face -- the rim
	 * of a piece that sits over another, Kano's eye plate, a helmet part --
	 * gets crease 1.0 (255), Blender's own way to keep an edge where it is.
	 * Without it Catmull-Clark pulls the rim in and what is under the piece
	 * shows through (Mary: Kano, Sektor). creaseFactor is the level, as in
	 * ss_sync_ccg_from_derivedmesh. */
	{
		int *uses = MEM_callocN(sizeof(int) * (size_t)(nedges > 0 ? nedges : 1), "edge uses");
		int *eh = NULL, ecap = 0, ehcap = 0, en = nedges;
		EdgeKey *ek = MEM_mallocN(sizeof(EdgeKey) * (size_t)(nedges > 0 ? nedges : 1), "edge copy");

		memcpy(ek, edges, sizeof(EdgeKey) * (size_t)nedges);
		ecap = nedges;
		en = 0;
		for (i = 0; i < s->nfaces; i++)
			for (j = 0; j < 3; j++)
				uses[edge_lookup_or_add(&ek, &en, &ecap, &eh, &ehcap,
				                        s->face_v[i * 3 + j], s->face_v[i * 3 + (j + 1) % 3])]++;
		s->edge_crease = MEM_mallocN(sizeof(float) * (size_t)(nedges > 0 ? nedges : 1), "edge_crease");
		for (i = 0; i < nedges; i++)
			s->edge_crease[i] = uses[i] == 1 ? (float)levels : 0.0f;
		if (eh) MEM_freeN(eh);
		MEM_freeN(ek);
		MEM_freeN(uses);
	}
	s->fit_base = MEM_mallocN(sizeof(float) * 3 * (size_t)(s->nwelded > 0 ? s->nwelded : 1), "fit_base");
	s->ntris = ntris;
	s->last_pos = MEM_mallocN(sizeof(float) * 3 * (size_t)nverts, "last_pos");
	s->last_attr = MEM_mallocN(sizeof(float) * 3 * (size_t)ntris, "last_attr");

	/* ---- the two subdivision surfaces ---- */
	s->ss = getSubSurf(levels, 3);
	s->uvss = getSubSurf(levels, 2);
	s->edgeSize = ccgSubSurf_getEdgeSize(s->ss);
	s->gridSize = ccgSubSurf_getGridSize(s->ss);
	s->weights = get_ss_weights(s->gridSize - 2, 3);
	s->welded_pos = MEM_mallocN(sizeof(float) * 3 * (size_t)s->nwelded, "welded_pos");
	s->faceMap = MEM_callocN(sizeof(CCGFace *) * (size_t)s->nfaces, "facemap");
	s->uvFaceMap = MEM_callocN(sizeof(CCGFace *) * (size_t)s->nfaces, "facemapuv");

	weld_positions(s, pos);
	nuv = sync_uvs(s, (const float (*)[2])face_uv);
	MEM_freeN(face_uv);
	if (nuv == 0 || !sync_positions(s)) {
		umk_subsurf_free(s);
		return NULL;
	}
	face_maps(s->ss, s->faceMap);
	face_maps(s->uvss, s->uvFaceMap);
	for (i = 0; i < s->nfaces; i++)
		if (!s->faceMap[i] || !s->uvFaceMap[i]) {
			umk_subsurf_free(s);
			return NULL;
		}

	if (!build_output(s, nuv)) {
		umk_subsurf_free(s);
		return NULL;
	}
	return s;
}

int umk_subsurf_eval(UmkSubsurf *s, const float *pos, const float *corner_attr)
{
	int i, j;
	const int gs = s->gridSize;

	/* the same pose as last time (a pause, a held frame): nothing to do */
	if (s->have_last && memcmp(s->last_pos, pos, sizeof(float) * 3 * (size_t)s->nverts) == 0
	    && (corner_attr == NULL) == (s->last_had_attr == 0)
	    && (corner_attr == NULL || memcmp(s->last_attr, corner_attr, sizeof(float) * 3 * (size_t)s->ntris) == 0))
		return 1;

	weld_positions(s, pos);
	if (!sync_positions_partial(s))
		return 0;

	/* Port, not Blender: Catmull-Clark shrinks a closed piece, and where
	 * two pieces of a fighter overlap (Jax's head in his neck, Sektor's
	 * helmet) the shrink opens a gap. One correction pass: each control
	 * point moved by how far its subdivided self fell short of it, and the
	 * surface subdivided again -- the same CCG, so the result still passes
	 * (nearly) through the model's own vertices. */
	memcpy(s->fit_base, s->welded_pos, sizeof(float) * 3 * (size_t)s->nwelded);
	for (i = 0; i < s->nwelded; i++) {
		CCGVert *v = ccgSubSurf_getVert(s->ss, POINTER_FROM_INT(i));
		const float *lim;

		if (v == NULL)
			continue;
		lim = ccgSubSurf_getVertData(s->ss, v);
		for (j = 0; j < 3; j++)
			s->welded_pos[i * 3 + j] = s->fit_base[i * 3 + j] * 2.0f - lim[j];
	}
	if (!sync_positions_partial(s))
		return 0;

	memcpy(s->last_pos, pos, sizeof(float) * 3 * (size_t)s->nverts);
	s->last_had_attr = corner_attr != NULL;
	if (corner_attr)
		memcpy(s->last_attr, corner_attr, sizeof(float) * 3 * (size_t)s->ntris);
	s->have_last = 1;

	for (i = 0; i < s->out_nverts; i++) {
		const int *r = s->out_ref + i * 4;
		const float *co = ccgSubSurf_getFaceGridData(s->ss, s->faceMap[r[0]], r[1], r[2], r[3]);

		s->out_pos[i * 3 + 0] = co[0];
		s->out_pos[i * 3 + 1] = co[1];
		s->out_pos[i * 3 + 2] = co[2];

		if (corner_attr) {
			/* read as subsurf_ccg.c reads it for loop data:
			 * w + S * numVerts * g2_wid * g2_wid + (y * g2_wid + x) * numVerts */
			const float *w = s->weights + ((r[1] * gs + r[3]) * gs + r[2]) * 3;
			const float *a = corner_attr + s->face_src[r[0]] * 3;
			float v = 0.0f;
			for (j = 0; j < 3; j++)
				v += w[j] * a[j];
			s->out_attr[i] = v;
		}
	}
	return 1;
}

int umk_subsurf_num_verts(const UmkSubsurf *s) { return s->out_nverts; }
const float *umk_subsurf_pos(const UmkSubsurf *s) { return s->out_pos; }
const float *umk_subsurf_uv(const UmkSubsurf *s) { return s->out_uv; }
const float *umk_subsurf_attr(const UmkSubsurf *s) { return s->out_attr; }
int umk_subsurf_num_tris(const UmkSubsurf *s) { return s->out_ntris; }
const unsigned int *umk_subsurf_tris(const UmkSubsurf *s) { return s->out_tri; }
int umk_subsurf_welded(const UmkSubsurf *s) { return s->merged; }

void umk_subsurf_free(UmkSubsurf *s)
{
	if (!s)
		return;
	if (s->ss) ccgSubSurf_free(s->ss);
	if (s->uvss) ccgSubSurf_free(s->uvss);
	if (s->weld_map) MEM_freeN(s->weld_map);
	if (s->group_count) MEM_freeN(s->group_count);
	if (s->face_src) MEM_freeN(s->face_src);
	if (s->face_v) MEM_freeN(s->face_v);
	if (s->edges) MEM_freeN(s->edges);
	if (s->edge_crease) MEM_freeN(s->edge_crease);
	if (s->fit_base) MEM_freeN(s->fit_base);
	if (s->last_pos) MEM_freeN(s->last_pos);
	if (s->last_attr) MEM_freeN(s->last_attr);
	if (s->weights) MEM_freeN(s->weights);
	if (s->out_ref) MEM_freeN(s->out_ref);
	if (s->out_pos) MEM_freeN(s->out_pos);
	if (s->out_uv) MEM_freeN(s->out_uv);
	if (s->out_attr) MEM_freeN(s->out_attr);
	if (s->out_tri) MEM_freeN(s->out_tri);
	if (s->welded_pos) MEM_freeN(s->welded_pos);
	if (s->faceMap) MEM_freeN(s->faceMap);
	if (s->uvFaceMap) MEM_freeN(s->uvFaceMap);
	MEM_freeN(s);
}
