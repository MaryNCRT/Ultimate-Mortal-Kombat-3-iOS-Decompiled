/*
 * test_rendermesh_diff.c — clean RenderMesh.c against the oracle.
 *
 * Runs both loaders over every .meshset the game ships and compares what each
 * one produced: counts, names, bounds, indices, vertices and the per-vertex
 * lighting buffer. The oracle is the original _LIME_LoadMeshSet, statically
 * recompiled, so agreement means the hand-written version reproduces EA's
 * loader on EA's own data.
 *
 * Two things are deliberately not compared:
 *
 *  - `fullBright` WAS excluded, because this test used to implement
 *    IsTextureFullBright by delegating to the recompiled one, which made
 *    comparing it circular. That is no longer true: the clean side has its own
 *    implementation and this file supplies none, so the field is now compared
 *    like any other.
 *
 *  - `vertLight` when useLighting == 0, because the original leaves that
 *    buffer uninitialised (the branch at 0x0005ebae skips the memset). There
 *    is no defined value to compare against, so the test loads with
 *    useLighting == 1, which is the path that actually fills it.
 */

#include "arm_runtime.h"
#include "rendermesh.h"
#include "../src/decomp/lime/lime.h"

void lime_platform_set_asset_root(const char *path);

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RAM_SIZE   (64u << 20)
#define STACK_TOP  0x03F00000u
#define PATH_BUF   0x00700000u

/* Guest-side MESHSETINFO / MESHINFO offsets (verified). */
#define MS_NUMMESHES  0x44
#define MS_MESHES     0x48
#define MI_NUMVERTS   0x00
#define MI_NUMFACES   0x04
#define MI_RADIUS     0x10
#define MI_VERTS      0x18
#define MI_INDICES    0x1C
#define MI_VERTLIGHT  0x24
#define MI_MESHNAME   0x3C
#define MI_TEXNAME    0x40

static char g_root[512];
static int g_fail = 0;
static long g_files = 0, g_meshes = 0, g_skipped = 0, g_short_light = 0;

/* Per-file tracking of how much of the .lighting buffer is actually defined. */
static size_t g_light_total = 0, g_light_used = 0;
static int g_light_warned = 0;

/* ---- platform hooks the clean loader depends on ---- */

void *lime_load_file(const char *path, size_t *out_size)
{
    char full[1100];
    snprintf(full, sizeof(full), "%s/%s", g_root, path);
    FILE *f = fopen(full, "rb");
    if (!f) {
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0) {
        fclose(f);
        return NULL;
    }
    void *buf = malloc((size_t)n);
    if (buf && fread(buf, 1, (size_t)n, f) != (size_t)n) {
        free(buf);
        buf = NULL;
    }
    fclose(f);
    if (buf && out_size) {
        *out_size = (size_t)n;
    }
    return buf;
}

/* IsTextureFullBright used to be defined here, delegating to the recompiled
 * original, because the clean side had no body for it. It has one now -- see
 * decomp/lime/RenderMesh.c -- so this file no longer supplies one and the two
 * implementations are genuinely independent.
 *
 * They read the same res/nolight.txt: the oracle through guest RAM, the clean
 * side through runtime/lime_platform.c on the host. Neither calls the other. */

/* ---- oracle side ---- */

static uint32_t oracle_load(const char *rel, int useLighting)
{
    arm_ctx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.r[SP] = STACK_TOP;
    size_t n = strlen(rel) + 1;
    memcpy(g_ram + PATH_BUF, rel, n);
    ctx.r[0] = PATH_BUF;
    ctx.r[1] = (uint32_t)useLighting;
    func_0005ea34_LIME_LoadMeshSet(&ctx);
    return ctx.r[0];
}

static void fail(const char *file, const char *what)
{
    printf("  DIVERGES %-38s %s\n", file, what);
    g_fail++;
}

/* Variant A check: the original only understands this layout. */
static int is_variant_a(const unsigned char *raw, long sz)
{
    if (sz < 4) return 0;
    int32_t num;
    memcpy(&num, raw, 4);
    if (num <= 0 || num > 4096) return 0;
    size_t off = 4;
    for (int i = 0; i < num; i++) {
        if (off + 140 > (size_t)sz) return 0;
        int32_t nv, nf;
        memcpy(&nv, raw + off + 128, 4);
        memcpy(&nf, raw + off + 132, 4);
        if (nv < 0 || nf < 0 || nv > 4000000 || nf > 4000000) return 0;
        off += 140 + (size_t)nf * 6 + (size_t)nv * 26;
        if (off > (size_t)sz) return 0;
    }
    return 1;
}

static void check(const char *fname)
{
    size_t sz = 0;
    unsigned char *raw = (unsigned char *)lime_load_file(fname, &sz);
    if (!raw) {
        return;
    }
    if (!is_variant_a(raw, (long)sz)) {
        g_skipped++;
        free(raw);
        return;
    }
    free(raw);

    /* How many lighting bytes this file actually has to offer. */
    {
        char lp[600];
        snprintf(lp, sizeof(lp), "STATICLIGHTING/%.*s.lighting",
                 (int)(strlen(fname) - 8), fname);
        size_t ls = 0;
        void *lb = lime_load_file(lp, &ls);
        g_light_total = lb ? ls : 0;
        free(lb);
        g_light_used = 0;
        g_light_warned = 0;
    }

    guest_heap_init();
    uint32_t orc = oracle_load(fname, 1);
    MESHSETINFO *cln = LIME_LoadMeshSet(fname, 1);

    if (!orc || !cln) {
        fail(fname, "one of the two loaders returned NULL");
        LIME_FreeMeshSet(cln);
        return;
    }
    g_files++;

    int32_t on = (int32_t)MEM_LD32(orc + MS_NUMMESHES);
    if (on != cln->numMeshes) {
        fail(fname, "numMeshes");
        LIME_FreeMeshSet(cln);
        return;
    }

    uint32_t omeshes = MEM_LD32(orc + MS_MESHES);
    for (int i = 0; i < on; i++) {
        uint32_t om = MEM_LD32(omeshes + 4u * (uint32_t)i);
        MESHINFO *cm = cln->meshes[i];
        if (!om || !cm) { fail(fname, "null mesh pointer"); break; }

        if ((int32_t)MEM_LD32(om + MI_NUMVERTS) != cm->numVerts) { fail(fname, "numVerts"); break; }
        if ((int32_t)MEM_LD32(om + MI_NUMFACES) != cm->numFaces) { fail(fname, "numFaces"); break; }

        float orad = U32_F32(MEM_LD32(om + MI_RADIUS));
        if (memcmp(&orad, &cm->boundsRadius, 4) != 0) { fail(fname, "boundsRadius"); break; }

        char nm[80], tx[80];
        guest_read_cstr(MEM_LD32(om + MI_MESHNAME), nm, sizeof(nm));
        guest_read_cstr(MEM_LD32(om + MI_TEXNAME), tx, sizeof(tx));
        if (strcmp(nm, cm->meshName) != 0) { fail(fname, "meshName"); break; }
        if (strcmp(tx, cm->textureName) != 0) { fail(fname, "textureName"); break; }

        uint32_t oi = MEM_LD32(om + MI_INDICES);
        if (memcmp(g_ram + oi, cm->indices, (size_t)cm->numFaces * 6) != 0) {
            fail(fname, "indices"); break;
        }

        /*
         * Compared field by field, not with a memcmp of the struct: the
         * original never writes the two padding bytes at offset 6 (the copy
         * loop at 0x0005ebcc stores x, y, z, u and v and nothing else), so
         * they hold whatever the allocator left behind. Undefined bytes are
         * not part of the contract and must not be compared.
         */
        uint32_t ov = MEM_LD32(om + MI_VERTS);
        int vbad = 0;
        for (int v = 0; v < cm->numVerts && !vbad; v++) {
            uint32_t o = ov + 16u * (uint32_t)v;
            const LIMEVERTEX *c = &cm->verts[v];
            if ((int16_t)MEM_LD16(o + 0) != c->x ||
                (int16_t)MEM_LD16(o + 2) != c->y ||
                (int16_t)MEM_LD16(o + 4) != c->z) {
                vbad = 1;
                break;
            }
            float ou = U32_F32(MEM_LD32(o + 8));
            float ovv = U32_F32(MEM_LD32(o + 12));
            if (memcmp(&ou, &c->u, 4) != 0 || memcmp(&ovv, &c->v, 4) != 0) {
                vbad = 1;
            }
        }
        if (vbad) { fail(fname, "vertices"); break; }

        /*
         * Only the bytes the .lighting file actually provides are compared.
         *
         * KANO_STANDARD.lighting is one byte shorter than the mesh set needs
         * (42,867 bytes for 42,868 vertices). That is a defect in the asset
         * EA shipped, not a misreading of the format — the retail game reads
         * one byte past the end of that buffer too. Both implementations do
         * the same thing; only the garbage they pick up differs, and undefined
         * bytes are not part of the contract.
         */
        uint32_t ol = MEM_LD32(om + MI_VERTLIGHT);
        size_t defined = (size_t)cm->numVerts;
        if (g_light_total && g_light_used + defined > g_light_total) {
            size_t avail = (g_light_used < g_light_total)
                         ? g_light_total - g_light_used : 0;
            if (!g_light_warned) {
                printf("  note: %s — .lighting is %zu byte(s) short of the "
                       "vertex count; comparing only the defined bytes\n",
                       fname, defined - avail);
                g_light_warned = 1;
                g_short_light++;
            }
            defined = avail;
        }
        g_light_used += (size_t)cm->numVerts;

        if (defined && memcmp(g_ram + ol, cm->vertLight, defined) != 0) {
            fail(fname, "vertLight"); break;
        }

        g_meshes++;
    }

    /* Exercise the clean free path; a leak or double free shows up here. */
    LIME_FreeMeshSet(cln);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        printf("usage: %s <res dir> [armv7 slice]\n", argv[0]);
        return 2;
    }
    /* Unbuffered: if this test dies partway, the output up to that point is
     * what tells you where. Buffered stdout on a segfault gives you nothing at
     * all, which cost a debugging round the first time it happened. */
    setvbuf(stdout, NULL, _IONBF, 0);

    snprintf(g_root, sizeof(g_root), "%s", argv[1]);
    const char *image = (argc >= 3) ? argv[2]
                                    : "E:/MK3 PROJECT/OUTPUT/armv7/UMK3.armv7";

    arm_mem_init(RAM_SIZE);
    arm_set_asset_root(g_root);
    /* The clean side resolves paths through its own host platform layer, so it
     * needs the root too -- the two are deliberately separate variables so a
     * test can point them at different trees and prove neither is reading the
     * other's files. */
    lime_platform_set_asset_root(g_root);
    if (arm_load_image(image) != 0) {
        printf("could not map the binary image: %s\n", image);
        return 2;
    }

    printf("=== clean RenderMesh.c vs the recompiled original ===\n");
    printf("assets: %s\n\n", g_root);

    DIR *d = opendir(g_root);
    if (!d) {
        printf("could not open %s\n", g_root);
        return 2;
    }
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        size_t L = strlen(e->d_name);
        if (L > 8 && strcmp(e->d_name + L - 8, ".meshset") == 0) {
            check(e->d_name);
        }
    }
    closedir(d);

    printf("\nfiles compared:   %ld\n", g_files);
    printf("meshes compared:  %ld\n", g_meshes);
    printf("skipped (variant B/C, different engine path): %ld\n", g_skipped);
    printf("files with a short .lighting asset: %ld\n", g_short_light);
    printf("divergences:      %d\n", g_fail);
    printf("%s\n", g_fail ? "RESULT: FAIL"
                          : "RESULT: the clean loader matches the original");
    arm_mem_free();
    return g_fail ? 1 : 0;
}
