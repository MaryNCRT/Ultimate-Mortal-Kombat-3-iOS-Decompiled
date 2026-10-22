/*
 * savestate.c -- the game's own heap and the debug menu's save states. See
 * savestate.h for the idea. A port feature, not in the binary.
 *
 * The arena: one address range reserved at start (committed as it grows),
 * power-of-two size classes with a free list each, and every piece of
 * bookkeeping -- the top, the free lists -- in a header at its base. Blocks
 * never move and the arena never shrinks, so copying [base, top) and copying
 * it back is a whole, consistent heap at another moment.
 *
 * Not a state file itself: its own statics (the slots, how much is
 * committed) must survive a load.
 */
#include "savestate.h"

#undef malloc
#undef calloc
#undef realloc
#undef free

#include <stdio.h>
#include <stdint.h>

#ifdef _WIN32
#include <windows.h>

/* The arena's address space, committed lazily: the largest of the sizes
 * in arena_init that fits (in the 64-bit build below 2 GB, beside
 * limeMalloc's region) */
static uint32_t g_reserve;
#define ST_RESERVE   g_reserve
#define ST_COMMIT    (4u << 20)         /* commit step */
#define ST_HDR       16u                /* per block, keeps 16-byte alignment */
#define ST_MIN_CLASS 4                  /* 16 bytes */
#define ST_CLASSES   25                 /* 2^4 .. 2^28 = 256 MB */
#define ST_MAGIC     0x5354u            /* "ST", in use */
#define ST_FREED     0x4652u            /* "FR", on a free list */

struct st_head {                        /* at the arena's base; in the state */
    uint32_t top;                       /* bytes used, from base */
    uint32_t free_list[ST_CLASSES];     /* offsets, 0 = empty */
};

struct st_block {                       /* ST_HDR bytes before each block */
    uint16_t magic;
    uint16_t cls;
    uint32_t next;                      /* while free: the next free offset */
    uint32_t pad[2];
};

static uint8_t *g_base;
static uint32_t g_committed;
static int      g_failed;               /* the arena ran out once: no states */

static struct st_head *head(void) { return (struct st_head *)g_base; }

#ifdef UMK3_LOW32
/* The 64-bit build: the game keeps its pointers in 32-bit words, and in
 * longs, which sign-extend, so its memory has to sit below 2 GB. Reserves
 * `size` bytes in the first free range there big enough; 0 if none.
 * Shared with limeMalloc's region (lime_platform.c). */
static void *reserve_low(size_t size, DWORD extra)
{
    MEMORY_BASIC_INFORMATION mi;
    uintptr_t q = 0x10000;

    while (q < 0x7FFF0000u && VirtualQuery((void *)q, &mi, sizeof mi)) {
        uintptr_t at = ((uintptr_t)mi.BaseAddress + 0xFFFF) & ~(uintptr_t)0xFFFF;
        uintptr_t end = (uintptr_t)mi.BaseAddress + mi.RegionSize;

        if (mi.State == MEM_FREE && at + size <= end && at + size <= 0x7FFF0000u) {
            void *p = VirtualAlloc((void *)at, size, MEM_RESERVE | extra,
                                   PAGE_READWRITE);
            if (p)
                return p;
        }
        q = end;
    }
    return 0;
}

void *umk3_reserve_low(size_t size)
{
    return reserve_low(size, 0);
}

/* A block below 2 GB that is never freed, outside every state: what the
 * game keeps in a 32-bit word and a save state must not take back (the
 * texture cache, draw_gl.c). The C library's heap lands anywhere in the
 * 64-bit space -- above 4 GB on some runs, as address randomisation has it. */
void *umk3_alloc_low(size_t n)
{
    static uint8_t *cur, *end;
    uint8_t *p;

    n = (n + 15) & ~(size_t)15;
    if (!cur || (size_t)(end - cur) < n) {
        size_t chunk = n > (16u << 20) ? n : (16u << 20);

        cur = (uint8_t *)reserve_low(chunk, 0);
        if (!cur || !VirtualAlloc(cur, chunk, MEM_COMMIT, PAGE_READWRITE)) {
            cur = end = NULL;
            return NULL;
        }
        end = cur + chunk;
    }
    p = cur;
    cur += n;
    return p;
}
#endif

static int arena_init(void)
{
    if (g_base)
        return 1;
    if (g_failed)
        return 0;
    {
        static const uint32_t sizes[] = { 1024u << 20, 896u << 20, 768u << 20,
                                           512u << 20, 384u << 20 };
        unsigned i;

        for (i = 0; !g_base && i < sizeof sizes / sizeof sizes[0]; i++) {
            g_reserve = sizes[i];
#ifdef UMK3_LOW32
            g_base = (uint8_t *)reserve_low(g_reserve, MEM_WRITE_WATCH);
#else
            g_base = (uint8_t *)VirtualAlloc(NULL, g_reserve,
                                             MEM_RESERVE | MEM_WRITE_WATCH,
                                             PAGE_READWRITE);
#endif
        }
        if (g_base)
            printf("savestate: arena of %u MB\n", g_reserve >> 20);
        else
            fprintf(stderr, "savestate: no room for the arena\n");
    }
    if (!g_base) {
        g_failed = 1;
        return 0;
    }
    if (!VirtualAlloc(g_base, ST_COMMIT, MEM_COMMIT, PAGE_READWRITE)) {
        g_failed = 1;
        return 0;
    }
    g_committed = ST_COMMIT;
    memset(g_base, 0, sizeof(struct st_head));
    head()->top = (sizeof(struct st_head) + 15u) & ~15u;
    return 1;
}

static int in_arena(const void *p)
{
    return g_base && (const uint8_t *)p >= g_base
        && (const uint8_t *)p < g_base + ST_RESERVE;
}

static int class_of(size_t n)
{
    int c = ST_MIN_CLASS;
    while (c < ST_MIN_CLASS + ST_CLASSES - 1 && ((size_t)1 << c) < n)
        c++;
    return ((size_t)1 << c) >= n ? c : -1;
}

void *st_malloc(size_t n)
{
    int c, i;
    uint32_t off, need;
    struct st_block *b;

    if (!arena_init())
        return malloc(n);
    c = class_of(n ? n : 1);
    if (c < 0)
        goto fallback;
    i = c - ST_MIN_CLASS;
    off = head()->free_list[i];
    if (off) {
        b = (struct st_block *)(g_base + off - ST_HDR);
        head()->free_list[i] = b->next;
        b->next = 0;
        b->magic = ST_MAGIC;
        return g_base + off;
    }
    need = ST_HDR + (1u << c);
    if ((uint64_t)head()->top + need > ST_RESERVE)
        goto fallback;
    while (head()->top + need > g_committed) {
        uint32_t step = ST_COMMIT;
        if (head()->top + need > g_committed + step)
            step = (head()->top + need - g_committed + ST_COMMIT - 1)
                   / ST_COMMIT * ST_COMMIT;
        if (!VirtualAlloc(g_base + g_committed, step, MEM_COMMIT,
                          PAGE_READWRITE))
            goto fallback;
        g_committed += step;
    }
    b = (struct st_block *)(g_base + head()->top);
    b->magic = ST_MAGIC;
    b->cls = (uint16_t)c;
    b->next = 0;
    head()->top += need;
    return (uint8_t *)b + ST_HDR;

fallback:
    if (!g_failed)
        fprintf(stderr, "savestate: arena full at %u bytes; save states off\n",
                head()->top);
    g_failed = 1;
    return malloc(n);
}

void *st_calloc(size_t n, size_t size)
{
    void *p;

    if (size && n > (size_t)-1 / size)
        return NULL;
    p = st_malloc(n * size);
    if (p)
        memset(p, 0, n * size);
    return p;
}

static size_t block_size(const void *p)
{
    const struct st_block *b =
        (const struct st_block *)((const uint8_t *)p - ST_HDR);
    return (size_t)1 << b->cls;
}

void st_free(void *p)
{
    struct st_block *b;
    int i;

    if (!p)
        return;
    if (!in_arena(p)) {
        free(p);
        return;
    }
    b = (struct st_block *)((uint8_t *)p - ST_HDR);
    if (b->magic != ST_MAGIC)
        return;                         /* not ours, or freed twice */
    b->magic = ST_FREED;
    i = b->cls - ST_MIN_CLASS;
    b->next = head()->free_list[i];
    head()->free_list[i] = (uint32_t)((uint8_t *)p - g_base);
}

void *st_realloc(void *p, size_t n)
{
    void *q;
    size_t old;

    if (!p)
        return st_malloc(n);
    if (!in_arena(p))
        return realloc(p, n);
    old = block_size(p);
    if (n <= old)
        return p;
    q = st_malloc(n);
    if (!q)
        return NULL;
    memcpy(q, p, old);
    st_free(p);
    return q;
}

void st_free_any(void *p)                  { st_free(p); }

void *st_realloc_any(void *p, size_t n)
{
    void *q;
    size_t old;

    if (!p || !in_arena(p))
        return realloc(p, n);
    old = block_size(p);                    /* from the arena: stays there */
    if (n <= old)
        return p;
    q = st_malloc(n);
    if (!q)
        return NULL;
    memcpy(q, p, old);
    st_free(p);
    return q;
}

/* ------------------------------------------------------------ the states */

/* The exe's .umkstd and .umkstb, the game files' globals and statics, then
 * .umkntd and .umkntb, the ones a rollback leaves alone (UMK3_NO_ROLLBACK).
 * `max` 2 asks for the first two only. */
static int state_sections(uint8_t **p, uint32_t *n, int max)
{
    uint8_t *img = (uint8_t *)GetModuleHandleA(NULL);
    IMAGE_NT_HEADERS *nt =
        (IMAGE_NT_HEADERS *)(img + ((IMAGE_DOS_HEADER *)img)->e_lfanew);
    IMAGE_SECTION_HEADER *s;
    static const char *const name[4] = {
        ".umkstd", ".umkstb", ".umkntd", ".umkntb"
    };
    int i, j, k = 0;

    /* .umkstd first, whatever order the linker put them in */
    for (j = 0; j < 4 && k < max; j++) {
        s = IMAGE_FIRST_SECTION(nt);
        for (i = 0; i < nt->FileHeader.NumberOfSections; i++, s++)
            if (!memcmp(s->Name, name[j], 7)) {
                p[k] = img + s->VirtualAddress;
                n[k] = s->Misc.VirtualSize;
                k++;
                break;
            }
    }
    return k;
}

/* The menu's slots, then the rollback probe's own (rollback_probe.c). */
#define ST_PROBE_SLOTS 3

static struct {
    uint8_t *buf;
    uint32_t cap, used, arena;
} g_slot[ST_SLOTS + ST_PROBE_SLOTS];

/* The arena reserved now rather than at the first block: the 64-bit build
 * calls this first thing, before the graphics driver takes the low memory. */
void st_init(void)
{
#ifdef UMK3_LOW32
    umk3_alloc_low(0);                  /* the texture cache's first chunk */
#endif
    arena_init();
}

int st_available(void)
{
    uint8_t *p[2];
    uint32_t n[2];

    return g_base && !g_failed && state_sections(p, n, 2) == 2;
}

int st_has(int slot)
{
    return slot >= 0 && slot < ST_SLOTS && g_slot[slot].used != 0;
}

unsigned long st_size(int slot)
{
    return st_has(slot) ? g_slot[slot].used : 0;
}

static int save_slot(int slot, const char **why);
static int load_slot(int slot, const char **why);

int st_save(int slot, const char **why)
{
    *why = "";
    if (slot < 0 || slot >= ST_SLOTS)
        return *why = "no such slot", -1;
    return save_slot(slot, why);
}

int st_load(int slot, const char **why)
{
    *why = "";
    if (!st_has(slot))
        return *why = "that slot is empty", -1;
    return load_slot(slot, why);
}

static int save_slot(int slot, const char **why)
{
    uint8_t *p[4];
    uint32_t n[4], need, at;
    int k, i;

    if (!st_available())
        return *why = g_failed ? "the game's memory overflowed its arena"
                               : "this build has no state sections", -1;
    k = state_sections(p, n, 4);
    need = head()->top;
    for (i = 0; i < k; i++)
        need += n[i];
    if (g_slot[slot].cap < need) {
        if (g_slot[slot].buf)
            VirtualFree(g_slot[slot].buf, 0, MEM_RELEASE);
        g_slot[slot].buf = (uint8_t *)VirtualAlloc(NULL, need,
                                MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        g_slot[slot].cap = g_slot[slot].buf ? need : 0;
        g_slot[slot].used = 0;
        if (!g_slot[slot].buf)
            return *why = "not enough memory for the copy", -1;
    }
    at = 0;
    for (i = 0; i < k; i++) {
        memcpy(g_slot[slot].buf + at, p[i], n[i]);
        at += n[i];
    }
    memcpy(g_slot[slot].buf + at, g_base, head()->top);
    g_slot[slot].arena = head()->top;
    g_slot[slot].used = at + head()->top;
    return 0;
}

static int load_slot(int slot, const char **why)
{
    uint8_t *p[4];
    uint32_t n[4], at;
    int k, i;

    if (!g_slot[slot].used)
        return *why = "that slot is empty", -1;
    if (!st_available())
        return *why = "save states are off", -1;
    k = state_sections(p, n, 4);
    at = 0;
    for (i = 0; i < k; i++) {
        memcpy(p[i], g_slot[slot].buf + at, n[i]);
        at += n[i];
    }
    /* the arena only grows: what lies past the state's top is free space
     * for it now, and stays committed */
    memcpy(g_base, g_slot[slot].buf + at, g_slot[slot].arena);
    return 0;
}

/* ------------------------------------------------------ rollback's */

/* A rollback snapshot is .umkstd and .umkstb whole, and of the arena only
 * the pages written since st_rb_begin: GetWriteWatch names them, they join
 * `g_rb_track` for good, and a page a snapshot does not hold was not yet
 * written when it was taken, so the copy made at the start is its content. */
#define RB_PAGE 4096u

struct rb_slot {
    uint8_t  *sect;                     /* .umkstd then .umkstb */
    uint32_t *page;                     /* arena pages held, in order */
    uint8_t  *data;                     /* their contents */
    uint32_t  npages, cap, top;
    int       used;
};

static struct rb_slot *g_rb;
static int       g_rb_n;
static uint32_t  g_rb_sect;             /* bytes of the two sections */
static uint8_t  *g_rb_base;             /* the arena at st_rb_begin */
static uint32_t  g_rb_base_size;
static uint8_t  *g_rb_track;            /* per arena page: written since */
static uint32_t *g_rb_list;             /* the tracked pages, in order */
static uint32_t  g_rb_ntrack;

/* The pieces of the two sections a snapshot holds: all of them but the
 * range st_rb_skip names (the loaded fighters, which a fight only reads). */
static struct { uint8_t *p; uint32_t n; } g_rb_piece[3];
static int       g_rb_npieces;
static uint8_t  *g_rb_skip;
static uint32_t  g_rb_skip_n;

void st_rb_skip(const void *p, unsigned long n)
{
    g_rb_skip = (uint8_t *)p;
    g_rb_skip_n = (uint32_t)n;
}

static void rb_pieces(uint8_t *p[2], uint32_t sz[2])
{
    int i;

    g_rb_npieces = 0;
    g_rb_sect = 0;
    for (i = 0; i < 2; i++) {
        uint8_t *a = p[i], *e = p[i] + sz[i];
        uint8_t *sa = g_rb_skip, *se = g_rb_skip + g_rb_skip_n;

        if (g_rb_skip_n && sa >= a && se <= e) {
            if (sa > a) {
                g_rb_piece[g_rb_npieces].p = a;
                g_rb_piece[g_rb_npieces++].n = (uint32_t)(sa - a);
            }
            a = se;
        }
        if (e > a) {
            g_rb_piece[g_rb_npieces].p = a;
            g_rb_piece[g_rb_npieces++].n = (uint32_t)(e - a);
        }
    }
    for (i = 0; i < g_rb_npieces; i++)
        g_rb_sect += g_rb_piece[i].n;
}

static void rb_sect_copy(uint8_t *buf, int save)
{
    uint32_t at = 0;
    int i;

    for (i = 0; i < g_rb_npieces; i++) {
        if (save)
            memcpy(buf + at, g_rb_piece[i].p, g_rb_piece[i].n);
        else
            memcpy(g_rb_piece[i].p, buf + at, g_rb_piece[i].n);
        at += g_rb_piece[i].n;
    }
}

void st_rb_end(void)
{
    int i;

    for (i = 0; i < g_rb_n; i++) {
        VirtualFree(g_rb[i].sect, 0, MEM_RELEASE);
        free(g_rb[i].page);
        free(g_rb[i].data);
    }
    free(g_rb);
    g_rb = NULL;
    g_rb_n = 0;
    if (g_rb_base)
        VirtualFree(g_rb_base, 0, MEM_RELEASE);
    g_rb_base = NULL;
    free(g_rb_track);
    free(g_rb_list);
    g_rb_track = NULL;
    g_rb_list = NULL;
    g_rb_ntrack = 0;
}

int st_rb_begin(int n)
{
    uint8_t *p[2];
    uint32_t sz[2];
    int i;

    st_rb_end();
    if (!st_available() || state_sections(p, sz, 2) != 2 || n < 1)
        return -1;
    rb_pieces(p, sz);
    g_rb = (struct rb_slot *)calloc((size_t)n, sizeof *g_rb);
    g_rb_track = (uint8_t *)calloc(ST_RESERVE / RB_PAGE, 1);
    g_rb_list = (uint32_t *)malloc(ST_RESERVE / RB_PAGE * sizeof *g_rb_list);
    g_rb_base_size = g_committed;
    g_rb_base = (uint8_t *)VirtualAlloc(NULL, g_rb_base_size,
                                        MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!g_rb || !g_rb_track || !g_rb_list || !g_rb_base)
        return st_rb_end(), -1;
    g_rb_n = n;
    for (i = 0; i < n; i++) {
        g_rb[i].sect = (uint8_t *)VirtualAlloc(NULL, g_rb_sect,
                                MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if (!g_rb[i].sect)
            return st_rb_end(), -1;
    }
    memcpy(g_rb_base, g_base, g_rb_base_size);
    ResetWriteWatch(g_base, ST_RESERVE);
    return 0;
}

int st_rb_running(void) { return g_rb_n > 0; }
int st_owns(const void *p) { return in_arena(p); }

unsigned long st_rb_size(void)
{
    return g_rb_sect + g_rb_ntrack * RB_PAGE;
}

/* The arena pages written since the last call join the tracked ones. */
static void rb_watch(void)
{
    static void *hit[4096];
    ULONG_PTR count;
    DWORD gran;
    ULONG_PTR i;

    do {
        count = sizeof hit / sizeof hit[0];
        if (GetWriteWatch(WRITE_WATCH_FLAG_RESET, g_base, ST_RESERVE, hit,
                          &count, &gran) != 0)
            return;
        for (i = 0; i < count; i++) {
            uint32_t pg = (uint32_t)(((uint8_t *)hit[i] - g_base) / RB_PAGE);

            if (!g_rb_track[pg]) {
                g_rb_track[pg] = 1;
                g_rb_list[g_rb_ntrack++] = pg;
            }
        }
    } while (count == sizeof hit / sizeof hit[0]);
}

int st_rb_save(int slot)
{
    struct rb_slot *s;
    uint8_t *p[2];
    uint32_t sz[2], i;

    if (slot < 0 || slot >= g_rb_n || state_sections(p, sz, 2) != 2)
        return -1;
    s = &g_rb[slot];
    rb_sect_copy(s->sect, 1);

    rb_watch();
    if (g_rb_ntrack > s->cap) {
        uint32_t cap = g_rb_ntrack + 64;
        uint32_t *pg = (uint32_t *)realloc(s->page, cap * sizeof *pg);
        uint8_t *d;

        if (!pg)
            return -1;
        s->page = pg;
        d = (uint8_t *)realloc(s->data, (size_t)cap * RB_PAGE);
        if (!d)
            return -1;
        s->data = d;
        s->cap = cap;
    }
    for (i = 0; i < g_rb_ntrack; i++) {
        s->page[i] = g_rb_list[i];
        memcpy(s->data + (size_t)i * RB_PAGE, g_base + g_rb_list[i] * RB_PAGE,
               RB_PAGE);
    }
    s->npages = g_rb_ntrack;
    s->top = head()->top;
    s->used = 1;
    return 0;
}

int st_rb_load(int slot)
{
    struct rb_slot *s;
    uint8_t *p[2];
    uint32_t sz[2], i, pg;

    if (slot < 0 || slot >= g_rb_n || !g_rb[slot].used
        || state_sections(p, sz, 2) != 2)
        return -1;
    s = &g_rb[slot];
    rb_sect_copy(s->sect, 0);

    rb_watch();
    /* the slot's pages are the first s->npages of the list, in its order */
    for (i = 0; i < g_rb_ntrack; i++) {
        pg = g_rb_list[i];
        if (i < s->npages)
            memcpy(g_base + pg * RB_PAGE, s->data + (size_t)i * RB_PAGE, RB_PAGE);
        else if (pg * RB_PAGE < g_rb_base_size)
            memcpy(g_base + pg * RB_PAGE, g_rb_base + pg * RB_PAGE, RB_PAGE);
        else
            memset(g_base + pg * RB_PAGE, 0, RB_PAGE);
    }
    rb_watch();                         /* what the load itself wrote */
    return 0;
}

/* ------------------------------------------------- the rollback probe's */

int st_probe_save(int k)
{
    const char *why;
    return k >= 0 && k < ST_PROBE_SLOTS ? save_slot(ST_SLOTS + k, &why) : -1;
}

int st_probe_load(int k)
{
    const char *why;
    return k >= 0 && k < ST_PROBE_SLOTS ? load_slot(ST_SLOTS + k, &why) : -1;
}

/* Region r of the live state: 0 .umkstd, 1 .umkstb, 2 the arena up to its top. */
static int live_region(int r, const uint8_t **p, uint32_t *n)
{
    uint8_t *sp[2];
    uint32_t sn[2];

    if (!st_available())
        return 0;
    if (r == 2) {
        *p = g_base;
        *n = head()->top;
        return 1;
    }
    if (r < 0 || r > 2 || state_sections(sp, sn, 2) != 2)
        return 0;
    *p = sp[r];
    *n = sn[r];
    return 1;
}

unsigned long st_probe_region_size(int r)
{
    const uint8_t *p;
    uint32_t n;
    return live_region(r, &p, &n) ? n : 0;
}

/* Every 4 KB page of the live state that differs from probe slot k, by
 * region and offset; the arena's pages past the copy's top count as
 * different. Returns how many. */
unsigned long st_probe_diff(int k, void (*page)(int r, uint32_t off, void *ctx),
                            void *ctx)
{
    const uint8_t *p, *q;
    uint32_t n, have, off, len;
    unsigned long count = 0;
    int r;

    if (k < 0 || k >= ST_PROBE_SLOTS || !g_slot[ST_SLOTS + k].used)
        return 0;
    q = g_slot[ST_SLOTS + k].buf;
    for (r = 0; r < 3; r++) {
        if (!live_region(r, &p, &n))
            return count;
        have = r == 2 ? g_slot[ST_SLOTS + k].arena : n;
        for (off = 0; off < n; off += 4096) {
            len = n - off < 4096 ? n - off : 4096;
            if (off + len > have || memcmp(p + off, q + off, len)) {
                count++;
                if (page)
                    page(r, off, ctx);
            }
        }
        q += have;
    }
    return count;
}

/* A hash of each 4 KB page of live region r, at most `max`; how many. */
unsigned long st_probe_page_hashes(int r, uint64_t *out, unsigned long max)
{
    const uint8_t *p;
    uint32_t n, off, i, len;
    unsigned long k = 0;
    uint64_t h, w;

    if (!live_region(r, &p, &n))
        return 0;
    for (off = 0; off < n && k < max; off += 4096) {
        len = n - off < 4096 ? n - off : 4096;
        h = 0xcbf29ce484222325ull;
        for (i = 0; i + 8 <= len; i += 8) {
            memcpy(&w, p + off + i, 8);
            h = (h ^ w) * 0x100000001b3ull;
        }
        for (; i < len; i++)
            h = (h ^ p[off + i]) * 0x100000001b3ull;
        out[k++] = h;
    }
    return k;
}

/* A hash of the whole live state (one process only: it holds addresses). */
uint64_t st_probe_hash(void)
{
    uint64_t h = 0xcbf29ce484222325ull, w;
    const uint8_t *p;
    uint32_t n, i;
    int r;

    for (r = 0; r < 3; r++) {
        if (!live_region(r, &p, &n))
            break;
        for (i = 0; i + 8 <= n; i += 8) {
            memcpy(&w, p + i, 8);
            h = (h ^ w) * 0x100000001b3ull;
        }
        for (; i < n; i++)
            h = (h ^ p[i]) * 0x100000001b3ull;
    }
    return h;
}

#else   /* no arena elsewhere yet: the C library's heap, and no states */

void *st_malloc(size_t n)                 { return malloc(n); }
void *st_calloc(size_t n, size_t s)       { return calloc(n, s); }
void *st_realloc(void *p, size_t n)       { return realloc(p, n); }
void  st_free(void *p)                    { free(p); }
void *st_realloc_any(void *p, size_t n)   { return realloc(p, n); }
void  st_free_any(void *p)                { free(p); }
void  st_init(void)                       {}
int   st_available(void)                  { return 0; }
int   st_has(int slot)                    { (void)slot; return 0; }
unsigned long st_size(int slot)           { (void)slot; return 0; }
int   st_save(int slot, const char **why)
{ (void)slot; *why = "save states need the Windows build"; return -1; }
int   st_load(int slot, const char **why)
{ (void)slot; *why = "save states need the Windows build"; return -1; }
int   st_probe_save(int k)                 { (void)k; return -1; }
int   st_probe_load(int k)                 { (void)k; return -1; }
unsigned long st_probe_region_size(int r) { (void)r; return 0; }
unsigned long st_probe_diff(int k, void (*page)(int, uint32_t, void *), void *ctx)
{ (void)k; (void)page; (void)ctx; return 0; }
uint64_t st_probe_hash(void)              { return 0; }
unsigned long st_probe_page_hashes(int r, uint64_t *out, unsigned long max)
{ (void)r; (void)out; (void)max; return 0; }
int   st_rb_begin(int n)                   { (void)n; return -1; }
int   st_rb_save(int slot)                 { (void)slot; return -1; }
int   st_rb_load(int slot)                 { (void)slot; return -1; }
void  st_rb_end(void)                      {}
void  st_rb_skip(const void *p, unsigned long n) { (void)p; (void)n; }
int   st_rb_running(void)                  { return 0; }
int   st_owns(const void *p)               { (void)p; return 0; }
unsigned long st_rb_size(void)            { return 0; }

#endif
