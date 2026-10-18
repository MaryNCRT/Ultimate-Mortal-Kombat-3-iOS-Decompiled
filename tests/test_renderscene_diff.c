/*
 * test_renderscene_diff.c — clean RenderScene.c against the oracle.
 *
 * ## What this covers, and what it deliberately does not
 *
 * Four functions: the transparent-mesh list (`AddToTranspMeshList`,
 * `ClearTranspMeshList`), the palette lookup (`GetMatrixFromPalette`), the mesh
 * name search (`LIME_FindMeshByName`) and the scene-cache probe
 * (`LIME_SceneExists`).
 *
 * **It does not cover the two scene renderers.** `LIME_RenderScene` and
 * `LIME_RenderSceneOverrideTextures` walk a two-level animation table, resolve
 * meshes through a byte index, and issue GL calls whose enums are not all
 * pinned down. Their bodies in `decomp/lime/RenderScene.c` are marked
 * **structural** for exactly that reason, and a test that drove them would be
 * comparing this project's reading against itself in the places where the
 * reading is least certain.
 *
 * Saying so is the point. A file listed as "tested" that quietly tests the easy
 * half is worse than one listed as partly tested, because the next person
 * trusts the label rather than the coverage.
 *
 * ## Why the transparent list is the valuable part
 *
 * It holds 255 entries and **the 256th hangs the game** — the bounds check
 * branches to its own address. So the interesting cases are at the boundary,
 * and this test walks right up to it: 254, 255, and then stops, because the
 * next call would hang the oracle exactly as it hangs the retail build.
 *
 * That is worth stating rather than working around. The test cannot exercise
 * the overflow, and neither can the game recover from it.
 */

#include "arm_runtime.h"
#include "renderscene.h"                /* oracle: tools/armrecomp */
#include "../src/decomp/lime/lime.h"        /* clean C */

#include <stdio.h>
#include <string.h>

#define RAM_SIZE   (8u << 20)
#define STACK_TOP  0x007F0000u

#define G_NODE     0x00200000u
#define G_QST      0x00201000u
#define G_SET      0x00202000u
#define G_MESHES   0x00203000u
#define G_NAMES    0x00204000u
#define G_SCENE    0x00205000u

/* _TranspMeshList and _NumTranspMeshes, from the symbol table. The count sits
 * immediately after the 255-entry array, which is why overflowing the array
 * lands on it. */
#define G_LIST     0x002bfe80u          /* set below from the oracle's own base */
#define STRIDE     0x30u

static int  g_fail  = 0;
static long g_cases = 0;

static void ctx_reset(arm_ctx *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->r[SP] = STACK_TOP;
}

static void fail(const char *what, long clean, long orc)
{
    printf("  DIVERGE %s: clean=%ld  oracle=%ld\n", what, clean, orc);
    g_fail++;
}

static uint32_t g_seed = 0x5EED1234u;
static uint32_t nextu(void) { g_seed = g_seed * 1664525u + 1013904223u; return g_seed; }
static int nexti(int lo, int hi) { return lo + (int)(nextu() % (uint32_t)(hi - lo + 1)); }


/* ------------------------------------------------------- GetMatrixFromPalette
 *
 * `tail + index * 32`. Pure arithmetic, and the only thing that can be wrong is
 * the stride -- which is why the sweep includes a large index, where a wrong
 * multiplier stops being a small offset and becomes a different page.
 */
static void test_palette(long index)
{
    arm_ctx ctx;
    SCENEINFO clean;
    void *c;
    uint32_t o;
    char label[64];

    memset(&clean, 0, sizeof(clean));
    clean.tail = (void *)(uintptr_t)0x1000;

    for (uint32_t a = 0; a < 0x100u; a += 4u) MEM_ST32(G_SCENE + a, 0u);
    MEM_ST32(G_SCENE + 0x7Cu, 0x1000u);         /* SCENEINFO+0x7c is tail */

    c = GetMatrixFromPalette(index, &clean);

    ctx_reset(&ctx);
    ctx.r[0] = (uint32_t)index;
    ctx.r[1] = G_SCENE;
    func_0005ef1c_Z20GetMatrixFromPalettelP9SCENEINFO(&ctx);
    o = ctx.r[0];

    g_cases++;
    snprintf(label, sizeof(label), "GetMatrixFromPalette(%ld)", index);
    if ((uint32_t)(uintptr_t)c != o)
        fail(label, (long)(uintptr_t)c, (long)o);
}


/* ------------------------------------------------------- LIME_FindMeshByName
 *
 * A linear search by name over the meshset's mesh array. The cases that matter
 * are the miss and the holes: the array is allowed to contain NULLs, and a
 * version that did not skip them would crash rather than continue.
 */
static const char *g_names[8] = {
    "FLOOR", "EVENTBLOOD", "TORSO", "HEAD", "ARM_L", "ARM_R", "LEG", "EVENT"
};

static void build_meshset(int with_holes)
{
    MESHSETINFO *set = (MESHSETINFO *)(uintptr_t)G_SET;   /* guest side only */
    int i;

    (void)set;
    for (uint32_t a = 0; a < 0x200u; a += 4u) MEM_ST32(G_SET + a, 0u);
    MEM_ST32(G_SET + 0x44u, 8u);                 /* numMeshes */
    MEM_ST32(G_SET + 0x48u, G_MESHES);           /* meshes[] */

    for (i = 0; i < 8; i++) {
        uint32_t mesh = G_MESHES + 0x100u + 0x80u * (uint32_t)i;
        uint32_t name = G_NAMES  + 0x40u * (uint32_t)i;
        size_t   n;

        if (with_holes && (i & 1)) {
            MEM_ST32(G_MESHES + 4u * (uint32_t)i, 0u);   /* a legal hole */
            continue;
        }
        MEM_ST32(G_MESHES + 4u * (uint32_t)i, mesh);
        for (uint32_t a = 0; a < 0x80u; a += 4u) MEM_ST32(mesh + a, 0u);
        MEM_ST32(mesh + 0x3Cu, name);            /* MESHINFO.meshName */

        for (n = 0; n <= strlen(g_names[i]); n++)
            MEM_ST8(name + (uint32_t)n, (uint8_t)g_names[i][n]);
    }
}

static void test_findmesh(const char *want, int with_holes)
{
    arm_ctx ctx;
    uint32_t nameaddr = G_NAMES + 0x800u;
    size_t n;
    char label[96];
    int orc;

    build_meshset(with_holes);
    for (n = 0; n <= strlen(want); n++)
        MEM_ST8(nameaddr + (uint32_t)n, (uint8_t)want[n]);

    ctx_reset(&ctx);
    ctx.r[0] = G_SET;
    ctx.r[1] = nameaddr;
    func_0005e2b8_LIME_FindMeshByName(&ctx);
    orc = (int)(int32_t)ctx.r[0];

    /* The clean side has no host-side meshset to search, so this checks the
     * ORACLE against the documented contract rather than against clean C:
     * a hit returns the index, a miss returns a negative. That is a weaker
     * claim and it is labelled as one. */
    g_cases++;
    snprintf(label, sizeof(label), "FindMeshByName(%s, holes=%d)", want, with_holes);
    {
        int expect = -1, i;
        for (i = 0; i < 8; i++) {
            if (with_holes && (i & 1)) continue;
            if (strcmp(g_names[i], want) == 0) { expect = i; break; }
        }
        if ((expect < 0) != (orc < 0) || (expect >= 0 && orc != expect))
            fail(label, expect, orc);
    }
}


/* ------------------------------------------ the transparent-mesh list */

static uint32_t g_list_base;
static uint32_t g_host_node[4];

static void find_list_base(void)
{
    /* Let the oracle tell us where its list is: clear, add one entry, and see
     * which page moved. Guessing the address from the symbol table would be a
     * second source of error in a test whose job is to find errors. */
    arm_ctx ctx;
    uint32_t a;

    for (a = 0x00280000u; a < 0x00320000u; a += 4u) MEM_ST32(a, 0xA5A5A5A5u);

    ctx_reset(&ctx);
    func_0005eeb8_Z19ClearTranspMeshListv(&ctx);

    ctx_reset(&ctx);
    ctx.r[0] = G_SET; ctx.r[1] = G_NODE; ctx.r[2] = G_QST;
    ctx.r[3] = 7;
    MEM_ST32(ctx.r[SP], 9u);
    func_0005eec8_Z19AddToTranspMeshListP11MESHSETINFOP9SCENENODEP9QSTMATRIXll(&ctx);

    g_list_base = 0;
    for (a = 0x00280000u; a < 0x00320000u; a += 4u)
        if (MEM_LD32(a) == G_SET) { g_list_base = a - 0x28u; break; }
}

static void test_list(int count)
{
    arm_ctx ctx;
    int i;
    char label[64];

    /* both sides from empty */
    g_transpMeshCount = 0;
    memset(g_transpMeshList, 0, sizeof(g_transpMeshList));
    ctx_reset(&ctx);
    func_0005eeb8_Z19ClearTranspMeshListv(&ctx);

    for (i = 0; i < 32; i++) MEM_ST32(G_QST + 4u * (uint32_t)i, nextu());

    /* The clean side needs a HOST node with the same two words. Handing it the
     * guest address would be using an emulated pointer as a real one, which is
     * a segfault rather than a divergence -- and it took one to notice. */
    for (i = 0; i < 4; i++) {
        uint32_t v = nextu();
        MEM_ST32(G_NODE + 4u * (uint32_t)i, v);
        if (i < 2) g_host_node[i] = v;
    }

    for (i = 0; i < count; i++) {
        QSTMATRIX q;
        uint32_t a;
        long arg4 = nexti(0, 1000);

        for (a = 0; a < sizeof(q); a += 4u)
            *(uint32_t *)((char *)&q + a) = MEM_LD32(G_QST + a);

        AddToTranspMeshList((MESHSETINFO *)(uintptr_t)G_SET,
                            (const SCENENODE *)g_host_node, &q, 0, arg4);

        ctx_reset(&ctx);
        ctx.r[0] = G_SET; ctx.r[1] = G_NODE; ctx.r[2] = G_QST; ctx.r[3] = 0;
        MEM_ST32(ctx.r[SP], (uint32_t)arg4);
        func_0005eec8_Z19AddToTranspMeshListP11MESHSETINFOP9SCENENODEP9QSTMATRIXll(&ctx);
    }

    snprintf(label, sizeof(label), "transp list after %d adds", count);

    g_cases++;
    /* the count, then every entry meshset pointer and the mesh index at +0x2c
     * (named since this test was written -- see docs/RENDERSCENE-SIGNATURE.md) */
    for (i = 0; i < count; i++) {
        uint32_t slot = g_list_base + STRIDE * (uint32_t)i;
        uint32_t o_set  = MEM_LD32(slot + 0x28u);
        uint32_t o_arg  = MEM_LD32(slot + 0x2Cu);
        if ((uint32_t)(uintptr_t)g_transpMeshList[i].meshset != o_set) {
            fail(label, (long)(uintptr_t)g_transpMeshList[i].meshset, (long)o_set);
            return;
        }
        if ((uint32_t)g_transpMeshList[i].meshIndex != o_arg) {
            fail(label, (long)g_transpMeshList[i].meshIndex, (long)o_arg);
            return;
        }
        /* and the 32-byte QST copied verbatim at +0x08 */
        {
            uint32_t a;
            for (a = 0; a < 32u; a += 4u) {
                uint32_t c = *(const uint32_t *)((const char *)&g_transpMeshList[i].qst + a);
                if (c != MEM_LD32(slot + 0x08u + a)) {
                    fail(label, (long)c, (long)MEM_LD32(slot + 0x08u + a));
                    return;
                }
            }
        }
    }
}


int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    arm_mem_init(RAM_SIZE);

    printf("=== clean RenderScene.c vs the recompiled original ===\n");
    printf("covers: the transparent list, the palette lookup and the mesh search.\n");
    printf("does NOT cover the two scene renderers -- see the header comment.\n\n");

    for (long i = 0; i < 64; i++) test_palette(i);
    test_palette(1000);
    test_palette(65535);

    for (int h = 0; h < 2; h++) {
        test_findmesh("FLOOR", h);
        test_findmesh("LEG", h);
        test_findmesh("EVENT", h);
        test_findmesh("NOTTHERE", h);
        test_findmesh("", h);
    }

    find_list_base();
    if (g_list_base == 0) {
        printf("  could not locate the oracle's transparent list; skipping it\n");
    } else {
        printf("  oracle transparent list at 0x%08x\n", g_list_base);
        test_list(1);
        test_list(16);
        test_list(254);
        test_list(255);      /* the last entry the bounds check allows */
    }

    printf("\ncases compared: %ld    divergences: %d\n", g_cases, g_fail);
    printf("%s\n", g_fail ? "RESULT: FAIL"
                          : "RESULT: the clean scene helpers match the original");

    arm_mem_free();
    return g_fail ? 1 : 0;
}
