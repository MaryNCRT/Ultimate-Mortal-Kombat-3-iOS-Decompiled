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

#define ST_RESERVE   (768u << 20)       /* address space, committed lazily */
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
void *umk3_reserve_low(size_t size)
{
    MEMORY_BASIC_INFORMATION mi;
    uintptr_t q = 0x10000;

    while (q < 0x7FFF0000u && VirtualQuery((void *)q, &mi, sizeof mi)) {
        uintptr_t at = ((uintptr_t)mi.BaseAddress + 0xFFFF) & ~(uintptr_t)0xFFFF;
        uintptr_t end = (uintptr_t)mi.BaseAddress + mi.RegionSize;

        if (mi.State == MEM_FREE && at + size <= end && at + size <= 0x7FFF0000u) {
            void *p = VirtualAlloc((void *)at, size, MEM_RESERVE, PAGE_READWRITE);
            if (p)
                return p;
        }
        q = end;
    }
    return 0;
}
#endif

static int arena_init(void)
{
    if (g_base)
        return 1;
    if (g_failed)
        return 0;
#ifdef UMK3_LOW32
    g_base = (uint8_t *)umk3_reserve_low(ST_RESERVE);
    if (!g_base)
        fprintf(stderr, "savestate: no room below 2 GB for the arena\n");
#else
    g_base = (uint8_t *)VirtualAlloc(NULL, ST_RESERVE, MEM_RESERVE,
                                     PAGE_READWRITE);
#endif
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

/* The exe's .umkstd and .umkstb: the game files' globals and statics. */
static int state_sections(uint8_t **p, uint32_t *n, int max)
{
    uint8_t *img = (uint8_t *)GetModuleHandleA(NULL);
    IMAGE_NT_HEADERS *nt =
        (IMAGE_NT_HEADERS *)(img + ((IMAGE_DOS_HEADER *)img)->e_lfanew);
    IMAGE_SECTION_HEADER *s = IMAGE_FIRST_SECTION(nt);
    int i, k = 0;

    for (i = 0; i < nt->FileHeader.NumberOfSections && k < max; i++, s++)
        if (!memcmp(s->Name, ".umkstd", 7) || !memcmp(s->Name, ".umkstb", 7)) {
            p[k] = img + s->VirtualAddress;
            n[k] = s->Misc.VirtualSize;
            k++;
        }
    return k;
}

static struct {
    uint8_t *buf;
    uint32_t cap, used, arena;
} g_slot[ST_SLOTS];

/* The arena reserved now rather than at the first block: the 64-bit build
 * calls this first thing, before the graphics driver takes the low memory. */
void st_init(void)
{
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

int st_save(int slot, const char **why)
{
    uint8_t *p[2];
    uint32_t n[2], need, at;
    int k, i;

    *why = "";
    if (slot < 0 || slot >= ST_SLOTS)
        return *why = "no such slot", -1;
    if (!st_available())
        return *why = g_failed ? "the game's memory overflowed its arena"
                               : "this build has no state sections", -1;
    k = state_sections(p, n, 2);
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

int st_load(int slot, const char **why)
{
    uint8_t *p[2];
    uint32_t n[2], at;
    int k, i;

    *why = "";
    if (!st_has(slot))
        return *why = "that slot is empty", -1;
    if (!st_available())
        return *why = "save states are off", -1;
    k = state_sections(p, n, 2);
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

#else   /* no arena elsewhere yet: the C library's heap, and no states */

void *st_malloc(size_t n)                 { return malloc(n); }
void *st_calloc(size_t n, size_t s)       { return calloc(n, s); }
void *st_realloc(void *p, size_t n)       { return realloc(p, n); }
void  st_free(void *p)                    { free(p); }
void *st_realloc_any(void *p, size_t n)   { return realloc(p, n); }
void  st_free_any(void *p)                { free(p); }
int   st_available(void)                  { return 0; }
int   st_has(int slot)                    { (void)slot; return 0; }
unsigned long st_size(int slot)           { (void)slot; return 0; }
int   st_save(int slot, const char **why)
{ (void)slot; *why = "save states need the Windows build"; return -1; }
int   st_load(int slot, const char **why)
{ (void)slot; *why = "save states need the Windows build"; return -1; }

#endif
