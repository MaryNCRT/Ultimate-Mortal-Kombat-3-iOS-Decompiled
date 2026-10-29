/*
 * lime_platform.c — the host side of the engine's platform API.
 *
 * The decompiled engine in decomp/lime/ calls twelve symbols it does not own:
 * six GL ES 1.1 entry points and six lime file/memory/texture calls. On the
 * device those came from iOS and from EA's own allocator. Here they come from
 * the host, and this file is the whole of that boundary.
 *
 * ## Why this is not the same as the stubs in arm_runtime.c
 *
 * `arm_runtime.c` already implements `limeLoadFile` and `limeFileSize`, but as
 * `stub_*(arm_ctx *)` operating on **guest** memory: they allocate inside the
 * emulated address space and return a guest address. That is exactly right for
 * the recompiled oracle, which runs in that address space.
 *
 * The clean decompiled C runs natively and needs **host** pointers. So the two
 * cannot share an implementation, and forwarding one to the other would mean
 * translating addresses on every call. They read the same files from the same
 * asset root and are otherwise independent, which is the point: the differential
 * tests compare two genuinely separate implementations rather than one calling
 * the other.
 *
 * ## The GL entries are no-ops
 *
 * Every function here that touches GL does nothing. That is deliberate and it
 * is enough for the differential tests, which compare loaders and maths --
 * nothing that draws. A port that renders must replace these; they are not a
 * rendering layer and are not trying to be.
 *
 * The matrix stack is the one place where "does nothing" could hide a bug, so
 * pushes and pops are counted and `lime_platform_matrix_depth()` exposes the
 * balance. LIME_PopMatrix takes a count rather than popping one, and a test
 * that leaves the stack unbalanced is worth catching even when no pixels exist.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <malloc.h>
#endif
#include <stdint.h>

#include "arm_runtime.h"
#include "mods.h"

void *limeMalloc(const char *tag, size_t bytes);
void  limeFree(void *p);
long  lime_heap_check(const char *where);
#include "../decomp/lime/lime.h"
#include "skin_mats.h"
extern void (*lime_port_skin_loaded)(const void *skin, const char *filename);
extern void (*lime_port_skin_parts_freed)(const void *skin);
extern float (*lime_port_root_y)(const void *bones);
extern void (*lime_port_bones_freed)(const void *bones);
extern int  (*lime_port_mesh_parts)(const unsigned short *indices, long nfaces,
                                    unsigned tex_name);


/* The engine passes paths relative to the bundle ("FLOOR.meshset",
 * "STATICLIGHTING/FLOOR.lighting"), so they are resolved against a root the
 * caller sets. Kept separate from arm_runtime's copy on purpose: a test can
 * point the two at different trees to prove neither is reading the other's. */
static char g_lime_asset_root[512] = "";

/* The root is readable too: Info.plist is opened from it directly. */
const char *lime_platform_asset_root(void)
{
    return g_lime_asset_root;
}

void lime_platform_set_asset_root(const char *path)
{
    snprintf(g_lime_asset_root, sizeof(g_lime_asset_root), "%s", path);
    mods_init(path);
    lime_port_skin_loaded = port_skin_mats_loaded;      /* skin_mats.c */
    lime_port_skin_parts_freed = port_skin_mats_forget;
    lime_port_root_y = port_skin_root_y;               /* the proportion trick */
    lime_port_bones_freed = port_skin_bones_forget;
    lime_port_mesh_parts = port_skin_mats_draw;
}

/* The port reads its own `res` folder and nothing outside it. The engine asks
 * for most files by bare name ("FLOOR.meshset") and a few with the bundle's
 * "res/" in front ("res/audio/x.wav"); that prefix is the root itself. The
 * bundle files the game needs from beside res/ on device -- Info.plist -- are
 * copied into res/ when the game folder is set up (launcher/setup_res.ps1). */
static void resolve(const char *rel, char *out, size_t n)
{
    if (g_lime_asset_root[0] == '\0') {
        snprintf(out, n, "%s", rel);
        return;
    }
    if (strncmp(rel, "res/", 4) == 0 || strncmp(rel, "res\\", 4) == 0)
        rel += 4;
    /* port: a mod's file of the same name first (mods.c) */
    if (mods_find(rel, out, n))
        return;
    snprintf(out, n, "%s/%s", g_lime_asset_root, rel);
}

/* For the rest of the platform layer (lime_menu.c's sounds and saves). */
void lime_platform_resolve(const char *rel, char *out, size_t n)
{
    resolve(rel, out, n);
}


/* ------------------------------------------------------------------ files */

size_t limeFileSize(const char *path)
{
    char full[1100];
    FILE *f;
    long n;

    resolve(path, full, sizeof(full));
    f = fopen(full, "rb");
    if (!f)
        return 0;                       /* absent reads as zero, as on device */

    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fclose(f);
    return (n > 0) ? (size_t)n : 0;
}

/* Returns a buffer the caller owns and must limeFree(). NULL on any failure --
 * the engine checks for it everywhere, and LIME_LoadEvents in particular treats
 * an empty file and an absent one identically, so this does too. */
void *limeLoadFile(const char *path)
{
    char full[1100];
    FILE *f;
    long n;
    void *p;

    if (getenv("LIME_HEAP_CHECK"))
        lime_heap_check(path);
    if (getenv("LIME_TRACE_FILES"))
        fprintf(stderr, "[load] %s ...", path);
    resolve(path, full, sizeof(full));
    f = fopen(full, "rb");
    if (getenv("LIME_TRACE_FILES"))
        fprintf(stderr, " %s\n", f ? "open" : "MISSING");
    if (!f)
        return NULL;

    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0) {
        fclose(f);
        return NULL;
    }

    /* **One byte more than the file, zeroed.**
     *
     * This is not defensive padding, it reproduces a guarantee the original
     * depends on without stating. On the device the buffer comes from an
     * allocator carving up memory that starts zeroed, so the byte just past a
     * loaded file is reliably NUL -- and the engine's text parsers lean on it.
     * GetNextLine scans for NUL, CR or LF with no length bound at all, so a
     * file whose last line has no trailing newline runs straight off the end.
     *
     * res/nolight.txt is exactly such a file. Handing it a bare malloc buffer
     * made IsTextureFullBright read into uninitialised memory and the whole
     * differential test segfaulted on its first meshset.
     *
     * The extra byte is not visible to callers: limeFileSize still reports the
     * real length, and the engine never looks past it deliberately. */
    /* Through limeMalloc, not calloc: the engine frees this with limeFree, and
     * an allocation and its release have to come from the same allocator. They
     * did not, and it went unnoticed while limeFree was a bare free() -- the
     * guarded allocator read a header that had never been written and walked
     * off the end of memory on the first frame list it loaded. */
    p = limeMalloc("loadfile", (size_t)n + 1);
    if (p)
        memset(p, 0, (size_t)n + 1);
    if (p && fread(p, 1, (size_t)n, f) != (size_t)n) {
        limeFree(p);
        p = NULL;
    }
    fclose(f);
    return p;
}


/* ----------------------------------------------------------------- memory
 *
 * The original takes a tag as its first argument -- "meshsethandle", "skin",
 * "bones", "font" -- which fed EA's own pool accounting. Nothing here uses it,
 * but it is kept in the signature because it is in the binary and because it is
 * genuinely useful: the tags name what every allocation in the engine is for.
 */
/* Guard bytes after every block, and a header carrying the tag, so an overrun
 * can be attributed instead of merely detected. Windows reports heap damage at
 * the next allocation, by which point the culprit is gone; this reports it
 * with the name the engine gave the allocation.
 *
 * The cost is 48 bytes and a memcmp per block. That is nothing next to an
 * afternoon spent bisecting a SIGTRAP inside ntdll. */
#define LIME_GUARD_BYTES 16
static const unsigned char LIME_CANARY[LIME_GUARD_BYTES] = {
    0xA5, 0x5A, 0xC3, 0x3C, 0xA5, 0x5A, 0xC3, 0x3C,
    0xA5, 0x5A, 0xC3, 0x3C, 0xA5, 0x5A, 0xC3, 0x3C,
};

typedef struct lime_block {
    struct lime_block *next, *prev;
    const char        *tag;
    size_t             bytes;
} lime_block;

static lime_block *g_blocks;
static long        g_live, g_overruns;

static int block_intact(const lime_block *b)
{
    const unsigned char *tail = (const unsigned char *)(b + 1) + b->bytes;
    return memcmp(tail, LIME_CANARY, LIME_GUARD_BYTES) == 0;
}

/* With LIME_HEAP_CHECK set, every allocation validates every live block first.
 * That turns "the heap is damaged" into "the heap was damaged before this
 * call", and the call is on the stack -- which is the whole difference between
 * a report and a lead. Quadratic, and irrelevant at these counts. */
static int heap_paranoid(void)
{
    static int on = -1;
    if (on < 0)
        on = getenv("LIME_HEAP_CHECK") != NULL;
    return on;
}

long lime_heap_check(const char *where);


/* ### Every block lives below 4 GB
 *
 * The binary is 32-bit, and the gamecode keeps its structures at the binary's
 * layout: a front-end character is 0x668 bytes with its ANIMATEDCHARACTER
 * pointer in the 4-byte word at +0x04 (`Load1Character`, armv7 0x0005ccec),
 * and its texture in the word at +0x528. A 64-bit malloc pointer does not fit.
 * Measured: `Load1Character` was handed 0x0000019461D16720 and the slot kept
 * 0x61D16720, so the fighters on the select screen pointed at nothing.
 *
 * So the allocations come from one region reserved below 4 GB, and every
 * pointer limeMalloc returns fits a 32-bit word exactly as it did on the
 * device. The gamecode stays at the binary's layout and no slot has to learn
 * about 64-bit hosts.
 *
 * The allocator is the simplest that frees: power-of-two size classes, one
 * free list each, bump allocation from the region when a list is empty, pages
 * committed as the bump pointer reaches them. The device had 128 MB; the
 * region is up to 2 GB, so doubling every block still leaves room.
 *
 * A host that cannot reserve low memory falls back to malloc and says so once.
 * Everything still runs; a slot that truncates a pointer is then the next
 * thing to look at. */
#ifdef _WIN32
/* SIZE_T is ULONG_PTR: unsigned long on i686, where size_t is unsigned int,
 * so size_t would clash with <memoryapi.h> when something pulls it in. */
#ifdef _WIN64
typedef unsigned long long low_size_t;
#else
typedef unsigned long low_size_t;
#endif
__declspec(dllimport) void *__stdcall VirtualAlloc(void *addr, low_size_t size,
                                                    unsigned long type,
                                                    unsigned long protect);
#define LOW_MEM_COMMIT  0x00001000
#define LOW_MEM_RESERVE 0x00002000
#define LOW_PAGE_RW     0x04
#else
#include <sys/mman.h>
#endif

#define LOW_CLASSES   32
#define LOW_MIN_SHIFT 6                 /* 64 bytes: the header plus guard */
#define LOW_COMMIT    (4u << 20)

static unsigned char *g_low_base, *g_low_top, *g_low_committed, *g_low_end;
static void          *g_low_free[LOW_CLASSES];
static int            g_low_state;      /* 0 untried, 1 low, -1 malloc */

#ifdef UMK3_LOW32
void *umk3_reserve_low(size_t size);    /* savestate.c */
#endif

static void low_reserve(void)
{
#ifdef UMK3_LOW32
    /* the 64-bit build: below 4 GB, beside the save states' arena
     * (runtime/savestate.c): the largest that fits. Reserved, committed as
     * it is used. 768 MB while it had to stay below 2 GB; the device had
     * 128 MB, and a fight with SMOOTH MODELS uses ~386 MB in all. */
    static const size_t sizes[] = { 2048u << 20, 1536u << 20, 1024u << 20, 768u << 20,
                                    640u << 20, 512u << 20, 384u << 20, 256u << 20,
                                    128u << 20 };
#else
    static const size_t sizes[] = { 1u << 30, 512u << 20, 256u << 20 };
    const uintptr_t lim = 0xFFFF0000u;
#endif
    size_t s;

    g_low_state = -1;
    /* A 32-bit host (the i686 game) already hands out pointers that fit a
     * 32-bit word, and the search below is written for a 64-bit address
     * space: on i686 `at + sizes[s]` wraps past 4 GB, the bound never fails
     * and, when no 1 GB hole is free, the loop never ends. Plain malloc. */
    if (sizeof(void *) <= 4)
        return;
    for (s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++) {
#if defined(_WIN32) && defined(UMK3_LOW32)
        /* the first free range below 4 GB that holds it (savestate.c) */
        g_low_base = (unsigned char *)umk3_reserve_low(sizes[s]);
#elif defined(_WIN32)
        uintptr_t at;
        /* Above the first 256 MB, where the loader and the CRT sit, and
         * wholly under 4 GB. */
        for (at = 0x10000000u; at + sizes[s] <= lim; at += 0x4000000u) {
            void *p = VirtualAlloc((void *)at, sizes[s], LOW_MEM_RESERVE,
                                   LOW_PAGE_RW);
            if (p) {
                g_low_base = (unsigned char *)p;
                break;
            }
        }
#elif defined(MAP_32BIT)
        {
            void *p = mmap(NULL, sizes[s], PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
            if (p != MAP_FAILED)
                g_low_base = (unsigned char *)p;
        }
#endif
        if (g_low_base) {
            printf("lime_heap: %u MB\n", (unsigned)(sizes[s] >> 20));
            g_low_top = g_low_committed = g_low_base;
            g_low_end = g_low_base + sizes[s];
            g_low_state = 1;
            return;
        }
    }
    fprintf(stderr, "lime_heap: no memory below 4 GB; pointers stored in "
                    "32-bit slots will be truncated\n");
}

/* The region reserved now rather than at the first block: the 64-bit
 * build calls this first thing (game_main.c). */
void lime_low_init(void)
{
    if (g_low_state == 0)
        low_reserve();
}

static int low_class(size_t total)
{
    int c = LOW_MIN_SHIFT;
    while (c < LOW_CLASSES && ((size_t)1 << c) < total)
        c++;
    return c;
}

static void *low_alloc(size_t total)
{
    int c;
    size_t size;
    void *p;

    if (g_low_state == 0)
        low_reserve();
    if (g_low_state < 0)
        return malloc(total);

    c = low_class(total);
    if (c >= LOW_CLASSES)
        return NULL;
    if (g_low_free[c]) {
        p = g_low_free[c];
        g_low_free[c] = *(void **)p;
        return p;
    }

    size = (size_t)1 << c;
    if ((size_t)(g_low_end - g_low_top) < size)
        return NULL;
    while (g_low_committed < g_low_top + size) {
#ifdef _WIN32
        size_t step = (size_t)(g_low_end - g_low_committed);
        if (step > LOW_COMMIT)
            step = LOW_COMMIT;
        if (!VirtualAlloc(g_low_committed, step, LOW_MEM_COMMIT, LOW_PAGE_RW))
            return NULL;
        g_low_committed += step;
#else
        g_low_committed = g_low_end;    /* mmap already backs the region */
#endif
    }
    p = g_low_top;
    g_low_top += size;
    return p;
}

static void low_free(void *p, size_t total)
{
    if (g_low_state < 0) {
        free(p);
        return;
    }
    {
        int c = low_class(total);
        *(void **)p = g_low_free[c];
        g_low_free[c] = p;
    }
}

void *limeMalloc(const char *tag, size_t bytes)
{
    lime_block *b;

    if (heap_paranoid())
        lime_heap_check(tag);

    /* Port: in a rollback fight the engine's blocks come from the save
     * states' arena, which a rollback puts back with the rest of the game;
     * this region and its free lists it does not (runtime/rollback.c). Such
     * a block is not on g_blocks, which is not put back either. */
    if (st_rb_running()) {
        b = st_malloc(sizeof(*b) + bytes + LIME_GUARD_BYTES);
        if (b != NULL && st_owns(b)) {
            b->tag   = tag ? tag : "(untagged)";
            b->bytes = bytes;
            b->prev  = b->next = NULL;
            memcpy((unsigned char *)(b + 1) + bytes, LIME_CANARY,
                   LIME_GUARD_BYTES);
            return b + 1;
        }
        st_free(b);
    }

    b = low_alloc(sizeof(*b) + bytes + LIME_GUARD_BYTES);

    if (b == NULL)
        return NULL;

    b->tag   = tag ? tag : "(untagged)";
    b->bytes = bytes;
    memcpy((unsigned char *)(b + 1) + bytes, LIME_CANARY, LIME_GUARD_BYTES);

    b->prev = NULL;
    b->next = g_blocks;
    if (g_blocks)
        g_blocks->prev = b;
    g_blocks = b;
    g_live++;

    return b + 1;
}

void limeFree(void *p)
{
    lime_block *b;

    if (p == NULL)
        return;                         /* free(NULL) is defined; so is this */

    b = (lime_block *)p - 1;
    if (!block_intact(b)) {
        fprintf(stderr, "lime_heap: OVERRUN past \"%s\" (%lu bytes)\n",
                b->tag, (unsigned long)b->bytes);
        g_overruns++;
    }

    /* Port: an arena block (above) goes back to the arena. One from this
     * region is kept while a rollback fight runs: a frame run again would
     * free it a second time. */
    if (st_owns(b)) {
        st_free(b);
        return;
    }
    if (st_rb_running())
        return;

    if (b->prev) b->prev->next = b->next; else g_blocks = b->next;
    if (b->next) b->next->prev = b->prev;
    g_live--;

    low_free(b, sizeof(*b) + b->bytes + LIME_GUARD_BYTES);
}

/* Walk every live allocation. Returns the number that are damaged and names
 * each one, so a caller can check between loader steps and find out not just
 * that memory was corrupted but by which allocation. */
long lime_heap_check(const char *where)
{
    const lime_block *b;
    long bad = 0;

    /* The Windows CRT can check allocations outside our guarded allocator.
     * Other platforms have no portable equivalent, so their checks below
     * cover the allocations owned by this layer. */
#ifdef _WIN32
    if (_heapchk() != _HEAPOK) {
        fprintf(stderr, "lime_heap: %s: the CRT heap is damaged\n",
                where ? where : "check");
        bad++;
    }
#endif

    for (b = g_blocks; b; b = b->next) {
        if (!block_intact(b)) {
            fprintf(stderr, "lime_heap: %s: \"%s\" (%lu bytes) is overrun\n",
                    where ? where : "check", b->tag,
                    (unsigned long)b->bytes);
            bad++;
        }
    }
    return bad;
}

long lime_heap_live(void)     { return g_live; }
long lime_heap_overruns(void) { return g_overruns; }


/* --------------------------------------------------------------- textures
 *
 * Headless: the loaders are exercised, the GL objects are not. A non-NULL
 * handle is returned so that null-checks in the engine take the path they take
 * on device -- returning NULL here would silently steer every caller down its
 * failure branch and make a passing test meaningless.
 */
#ifndef UMK3_REAL_GL   /* headless; see runtime/draw_gl.c for the windowed half */
/* A whole TEXTURE, zeroed: the mesh path reads ->field50 at +0x50, and the
 * int this used to be ended four bytes in. */
static TEXTURE g_texture_sentinel;

TEXTURE *limeLoadTexture(const char *path, int a, int b)
{
    (void)path; (void)a; (void)b;
    return &g_texture_sentinel;
}

void limeDeleteTexture(TEXTURE *tex)
{
    (void)tex;
}
#endif  /* !UMK3_REAL_GL */


/* --------------------------------------------------------- the GL surface
 *
 * No-ops, with the matrix stack depth tracked so an unbalanced push/pop is
 * still visible without a renderer.
 */
static int g_matrix_depth;

int lime_platform_matrix_depth(void)
{
    return g_matrix_depth;
}

#ifndef UMK3_REAL_GL   /* headless; see runtime/draw_gl.c for the windowed half */
void LIME_GLAPI glPushMatrix(void)   { g_matrix_depth++; }
void LIME_GLAPI glPopMatrix(void)    { if (g_matrix_depth > 0) g_matrix_depth--; }
void LIME_GLAPI glLoadIdentity(void) { }

void LIME_GLAPI glMatrixMode(unsigned mode)                  { (void)mode; }
void LIME_GLAPI glTranslatef(float x, float y, float z)      { (void)x; (void)y; (void)z; }
void LIME_GLAPI glMultMatrixf(const float *m)                { (void)m; }
void LIME_GLAPI glScalef(float x, float y, float z)          { (void)x; (void)y; (void)z; }


#endif  /* !UMK3_REAL_GL */
/* ------------------------------------------------------------ blend state
 *
 * Real symbols in the binary (_limeEnableAlphaBlending_Additive and friends),
 * but they are GL state and therefore ours to provide rather than to
 * decompile -- they live in the iOS layer, not in lime/common.
 *
 * The mode is recorded rather than ignored. Which one is active is the
 * difference between a transparent list that may go unsorted and one that may
 * not, so a test can assert on it even with no renderer behind it.
 */
static int g_blend_mode;    /* 0 none, 1 additive, 2 basic */

int lime_platform_blend_mode(void)          { return g_blend_mode; }

#ifndef UMK3_REAL_GL   /* headless; see runtime/draw_gl.c for the windowed half */
void limeEnableAlphaBlending_Additive(void) { g_blend_mode = 1; }
void limeEnableAlphaBlending_Basic(void)    { g_blend_mode = 2; }
void limeDisableAlphaBlending(void)         { g_blend_mode = 0; }

#endif  /* !UMK3_REAL_GL */
static int g_depth_writes = 1;

int lime_platform_depth_writes(void)        { return g_depth_writes; }

/* armv7 0x00065e54 / 0x00065e60: glDepthMask(1) / glDepthMask(0). The
 * windowed build issues them in draw_gl.c; this half records them. */
#ifndef UMK3_REAL_GL   /* headless; see runtime/draw_gl.c for the windowed half */
void limeEnableDepthWrites(void)            { g_depth_writes = 1; }
void limeDisableDepthWrites(void)           { g_depth_writes = 0; }
#endif  /* !UMK3_REAL_GL */


/* ------------------------------------------------------------- sprites
 *
 * limeDrawSprite is a real symbol in the binary but it lives in the iOS layer,
 * not in lime/common, so it belongs here rather than in decomp/.
 *
 * It records its last call instead of discarding it. A font test has no
 * renderer to inspect, but it can assert on the UV rectangle and the page a
 * glyph was drawn from -- which is exactly the part of limeDrawFONT worth
 * checking, and the part that would otherwise need eyes on a screen.
 */
static struct {
    const TEXTURE *page;
    float x, y, w, h;
    float u, v, du, dv;
    long  count;
} g_last_sprite;

/* Ten arguments; the tenth is the colour every caller in gamecode passes and
 * this signature used to drop. Nothing draws here, so it is not read -- but the
 * shape has to match or every argument after the fourth is misplaced. */
#ifndef UMK3_REAL_GL   /* headless; see runtime/draw_gl.c for the windowed half */
/* draw_gl.c's edge clamp; the headless build draws nothing */
int  lime_port_edge_clamp(const float *uv, int stride,
                          const unsigned short *idx, int n)
{ (void)uv; (void)stride; (void)idx; (void)n; return 0; }
void lime_port_edge_restore(int clamped) { (void)clamped; }
/* draw_gl.c's loading budget: headless, one step a frame as on the device */
int  lime_port_load_more(int first) { (void)first; return 0; }
void lime_port_load_done(const char *what) { (void)what; }

void limeDrawSprite(TEXTURE *page, float x, float y, float w, float h,
                    float u, float v, float du, float dv,
                    const float *colour)
{
    (void)colour;
    g_last_sprite.page = page;
    g_last_sprite.x = x;   g_last_sprite.y = y;
    g_last_sprite.w = w;   g_last_sprite.h = h;
    g_last_sprite.u = u;   g_last_sprite.v = v;
    g_last_sprite.du = du; g_last_sprite.dv = dv;
    g_last_sprite.count++;
}

long lime_platform_sprite_count(void) { return g_last_sprite.count; }

void lime_platform_last_sprite(float *out8)
{
    out8[0] = g_last_sprite.x;  out8[1] = g_last_sprite.y;
    out8[2] = g_last_sprite.w;  out8[3] = g_last_sprite.h;
    out8[4] = g_last_sprite.u;  out8[5] = g_last_sprite.v;
    out8[6] = g_last_sprite.du; out8[7] = g_last_sprite.dv;
}

/* The rotated, top-left-anchored sprite limeDrawFONTAtAngle draws through.
 * Shares the recorder above so a test can inspect either path. */
void limeDrawRotSpriteFromTopLeft(TEXTURE *page, float x, float y,
                                  float w, float h, float u, float v,
                                  float du, float dv, float angle,
                                  const float *colour)
{
    (void)angle;
    limeDrawSprite(page, x, y, w, h, u, v, du, dv, colour);
}
#endif  /* !UMK3_REAL_GL */


/* ------------------------------------------------ the rest of the GL surface
 *
 * The draw calls the mesh renderers use. All no-ops except glDrawElements,
 * which counts: a test with no renderer can still assert that a mesh was drawn
 * once rather than never or twice, and that is most of what the render path
 * gets wrong.
 */
static long g_draw_calls;
static long g_draw_indices;

long lime_platform_draw_calls(void)   { return g_draw_calls; }
long lime_platform_draw_indices(void) { return g_draw_indices; }

#ifndef UMK3_REAL_GL   /* headless; see runtime/draw_gl.c for the windowed half */
void LIME_GLAPI glDrawElements(unsigned mode, int count, unsigned type, const void *idx)
{
    (void)mode; (void)type; (void)idx;
    g_draw_calls++;
    g_draw_indices += count;
}

void LIME_GLAPI glEnable(unsigned cap)                       { (void)cap; }
void LIME_GLAPI glDisable(unsigned cap)                      { (void)cap; }
void LIME_GLAPI glEnableClientState(unsigned a)              { (void)a; }
void LIME_GLAPI glDisableClientState(unsigned a)             { (void)a; }
void glClientActiveTexture(unsigned u)            { (void)u; }
void glActiveTexture(unsigned u)                  { (void)u; }
void LIME_GLAPI glBindTexture(unsigned t, unsigned n)        { (void)t; (void)n; }
void LIME_GLAPI glTexEnvf(unsigned t, unsigned p, float v)   { (void)t; (void)p; (void)v; }
void LIME_GLAPI glVertexPointer(int s, unsigned t, int st, const void *p)   { (void)s; (void)t; (void)st; (void)p; }
void LIME_GLAPI glTexCoordPointer(int s, unsigned t, int st, const void *p) { (void)s; (void)t; (void)st; (void)p; }
void LIME_GLAPI glColorPointer(int s, unsigned t, int st, const void *p)    { (void)s; (void)t; (void)st; (void)p; }
void LIME_GLAPI glColor4f(float r, float g, float b, float a){ (void)r; (void)g; (void)b; (void)a; }
void LIME_GLAPI glDepthMask(unsigned char flag)              { (void)flag; }
void LIME_GLAPI glShadeModel(unsigned mode)                  { (void)mode; }
void LIME_GLAPI glCullFace(unsigned mode)                    { (void)mode; }

#endif  /* !UMK3_REAL_GL */