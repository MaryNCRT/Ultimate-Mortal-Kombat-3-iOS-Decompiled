/*
 * test_rendermesh_gl_diff.c — LIME_RenderMeshSingle by its GL call stream.
 *
 * ## Why this exists
 *
 * `tests/test_rendermesh_diff.c` already runs the clean mesh-set LOADER against
 * 590 real files and 7,327 meshes with zero divergences. It never touches the
 * draw. So the draw sat unchecked, and when the scene renderers were finally
 * driven (see docs/RENDERSCENE-SIGNATURE.md) every remaining divergence turned
 * out to be inside here rather than in the code being tested.
 *
 * A loader test and a draw test are not substitutes for one another. The file
 * was listed as verified and half of it was.
 *
 * ## What the cases are chosen against
 *
 * The draw has three gates that select genuinely different call streams, so
 * every combination is driven rather than a representative one:
 *
 *  - **fullBright**, the cached IsTextureFullBright answer at MESHINFO+0x50.
 *    It skips the vertex-colour work entirely rather than computing and
 *    ignoring it, which is the whole point of the res/nolight.txt mechanism.
 *  - **the texture's own flag at TEXTURE+0x50**, which gates the same work
 *    from the other side.
 *  - **a NULL texture**, which a mesh with no material hits on every frame.
 *
 * Alpha is swept too, because it reaches CreateFadedRGBS and from there the
 * colour array -- a path that only exists when the first two gates are open.
 *
 * ## What a pass proves
 *
 * Every enum, count, stride and type is compared exactly. The array pointers
 * are compared as null vs non-null only -- their lengths are not knowable from
 * the call -- and the count of those is reported rather than left implicit.
 */

#include "arm_runtime.h"
#include "gl_trace.h"
#include "rendermesh.h"                 /* oracle: tools/armrecomp */
#include "../src/decomp/lime/lime.h"        /* clean C */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RAM_SIZE   (16u << 20)
#define STACK_TOP  0x007F0000u

#define G_MESH     0x00700000u
#define G_VERTS    0x00710000u
#define G_INDICES  0x00720000u
#define G_VLIGHT   0x00730000u
#define G_TEX0     0x00740000u
#define G_TEX1     0x00750000u

#define MESH_VERTS 6
#define MESH_FACES 4

static int  g_fail  = 0;
static long g_cases = 0;
static int  g_ptr_only = 0;

/* host side */
static MESHINFO   h_mesh;
static LIMEVERTEX h_verts[MESH_VERTS];
static uint16_t   h_indices[MESH_FACES * 3];
static uint8_t    h_vertLight[MESH_VERTS];
static TEXTURE    h_tex0, h_tex1;
static char       h_name[64];
static char       h_texName[64];

static void ctx_reset(arm_ctx *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->r[SP] = STACK_TOP;
}

static uint32_t g_seed = 0x13572468u;
static uint32_t nextu(void) { g_seed = g_seed * 1664525u + 1013904223u; return g_seed; }

static uint32_t f2u(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }

/* Build the same mesh on both sides. The vertex is sixteen bytes: three int16
 * positions, two bytes of padding, then two floats of UV -- so the position
 * and texcoord arrays are the SAME buffer read at two offsets with one stride,
 * which is exactly what the original tells GL. */
static void build(int fullBright, int texFlag0, int texFlag1)
{
    int i;
    uint32_t a;

    memset(&h_mesh, 0, sizeof(h_mesh));
    for (a = 0; a < 0x80u; a += 4u) MEM_ST32(G_MESH + a, 0u);

    for (i = 0; i < MESH_VERTS; i++) {
        uint32_t g = G_VERTS + 16u * (uint32_t)i;
        h_verts[i].x   = (int16_t)nextu();
        h_verts[i].y   = (int16_t)nextu();
        h_verts[i].z   = (int16_t)nextu();
        h_verts[i].pad = 0;
        h_verts[i].u   = (float)(int)(nextu() % 1000u) / 1000.0f;
        h_verts[i].v   = (float)(int)(nextu() % 1000u) / 1000.0f;

        MEM_ST16(g + 0u, (uint16_t)h_verts[i].x);
        MEM_ST16(g + 2u, (uint16_t)h_verts[i].y);
        MEM_ST16(g + 4u, (uint16_t)h_verts[i].z);
        MEM_ST16(g + 6u, 0u);
        MEM_ST32(g + 8u,  f2u(h_verts[i].u));
        MEM_ST32(g + 12u, f2u(h_verts[i].v));

        h_vertLight[i] = (uint8_t)(nextu() & 0xFFu);
        MEM_ST8(G_VLIGHT + (uint32_t)i, h_vertLight[i]);
    }

    for (i = 0; i < MESH_FACES * 3; i++) {
        h_indices[i] = (uint16_t)(i % MESH_VERTS);
        MEM_ST16(G_INDICES + 2u * (uint32_t)i, h_indices[i]);
    }

    strcpy(h_name, "TORSO");
    strcpy(h_texName, "torso.pvr");

    h_mesh.numVerts     = MESH_VERTS;
    h_mesh.numFaces     = MESH_FACES;
    h_mesh.boundsRadius = 2.5f;
    h_mesh.verts        = h_verts;
    h_mesh.indices      = h_indices;
    h_mesh.vertLight    = h_vertLight;
    h_mesh.meshName     = h_name;
    h_mesh.textureName  = h_texName;
    h_mesh.fullBright   = fullBright;
    h_mesh.visible      = 1;

    MEM_ST32(G_MESH + 0x00u, (uint32_t)MESH_VERTS);
    MEM_ST32(G_MESH + 0x04u, (uint32_t)MESH_FACES);
    MEM_ST32(G_MESH + 0x10u, f2u(2.5f));
    MEM_ST32(G_MESH + 0x18u, G_VERTS);
    MEM_ST32(G_MESH + 0x1Cu, G_INDICES);
    MEM_ST32(G_MESH + 0x24u, G_VLIGHT);
    MEM_ST32(G_MESH + 0x50u, (uint32_t)fullBright);
    MEM_ST32(G_MESH + 0x54u, 1u);

    memset(&h_tex0, 0, sizeof(h_tex0));
    memset(&h_tex1, 0, sizeof(h_tex1));
    h_tex0.name = 0x2001u; h_tex0.field50 = texFlag0;
    h_tex1.name = 0x2002u; h_tex1.field50 = texFlag1;

    for (a = 0; a < 0x60u; a += 4u) { MEM_ST32(G_TEX0 + a, 0u); MEM_ST32(G_TEX1 + a, 0u); }
    MEM_ST32(G_TEX0 + 0x40u, 0x2001u); MEM_ST32(G_TEX0 + 0x50u, (uint32_t)texFlag0);
    MEM_ST32(G_TEX1 + 0x40u, 0x2002u); MEM_ST32(G_TEX1 + 0x50u, (uint32_t)texFlag1);
}

static void run(const char *what, int useTex0, int useTex1, float alpha, long flags)
{
    arm_ctx ctx;

    glt_reset();

    glt_select(&glt_clean);
    LIME_RenderMeshSingle(&h_mesh,
                          useTex0 ? &h_tex0 : NULL,
                          useTex1 ? &h_tex1 : NULL,
                          alpha, flags);

    glt_select(&glt_oracle);
    ctx_reset(&ctx);
    ctx.r[0] = G_MESH;
    ctx.r[1] = useTex0 ? G_TEX0 : 0u;
    ctx.r[2] = useTex1 ? G_TEX1 : 0u;
    ctx.r[3] = f2u(alpha);              /* soft-float: the float rides in r3 */
    MEM_ST32(ctx.r[SP], (uint32_t)flags);
    func_0005e5cc_Z21LIME_RenderMeshSingleP8MESHINFOP7TEXTURES2_fl(&ctx);

    g_cases++;
    g_fail += glt_compare(what, &g_ptr_only);
}

int main(void)
{
    int fb, t0f, t1f, useT0, useT1;
    char lbl[128];

    setvbuf(stdout, NULL, _IONBF, 0);
    arm_mem_init(RAM_SIZE);

    /* The vertex-colour scratch buffer is a global the engine allocates once
     * at startup; nothing in the draw path creates it. The oracle has the
     * binary's own, so the clean side needs one too or glColorPointer gets a
     * NULL where the original passes a buffer -- which is a divergence about
     * the harness, not about the code. Four bytes per vertex, RGBA. */
    g_vertexColourScratch = malloc(4u * MESH_VERTS);
    if (g_vertexColourScratch == NULL)
        return 2;

    printf("=== clean LIME_RenderMeshSingle vs the recompiled original ===\n");
    printf("compared by GL call stream, not by loaded data. The loader test\n");
    printf("(test_rendermesh_diff) covers the other half of this file.\n\n");

    for (fb = 0; fb < 2; fb++)
        for (useT0 = 0; useT0 < 2; useT0++)
            for (useT1 = 0; useT1 < 2; useT1++)
                for (t0f = 0; t0f < 2; t0f++)
                    for (t1f = 0; t1f < 2; t1f++) {
                        static const float A[3] = { 1.0f, 0.5f, 0.0f };
                        int k;
                        build(fb, t0f, t1f);
                        for (k = 0; k < 3; k++) {
                            snprintf(lbl, sizeof(lbl),
                                     "fb=%d t0=%d(%d) t1=%d(%d) alpha=%.1f",
                                     fb, useT0, t0f, useT1, t1f, A[k]);
                            run(lbl, useT0, useT1, A[k], 0);
                            snprintf(lbl, sizeof(lbl),
                                     "fb=%d t0=%d(%d) t1=%d(%d) alpha=%.1f FLAGS",
                                     fb, useT0, t0f, useT1, t1f, A[k]);
                            run(lbl, useT0, useT1, A[k], 1);
                        }
                    }

    /* a mesh with nothing in it: the degenerate case a scene hits whenever a
     * node points at a mesh the loader dropped */
    build(0, 0, 0);
    h_mesh.numVerts = 0; h_mesh.numFaces = 0;
    MEM_ST32(G_MESH + 0x00u, 0u);
    MEM_ST32(G_MESH + 0x04u, 0u);
    run("empty mesh", 1, 0, 1.0f, 0);

    printf("\ncases compared: %ld    divergences: %d\n", g_cases, g_fail);
    printf("pointer args checked only for nullness: %d\n", g_ptr_only);
    printf("%s\n", g_fail ? "RESULT: FAIL"
                          : "RESULT: the clean mesh draw matches the original");

    arm_mem_free();
    return g_fail ? 1 : 0;
}
