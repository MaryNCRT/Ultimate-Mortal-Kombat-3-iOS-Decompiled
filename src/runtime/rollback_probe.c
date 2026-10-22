/*
 * rollback_probe.c -- step 1 of rollback netcode for ONLINE: measure before
 * building. A port feature, not in the binary. Nothing here changes how a
 * fight plays; it only watches the fight step (UpdateArcadeCode's
 * mk3_update) and, when replaying, hands it recorded sticks.
 *
 * Three switches, read once, all off by default:
 *
 *   UMK3_ROLLBACK_PROBE=1   Windows: one tick in 30, which 4 KB pages of the
 *                           state it changed and how long a whole save takes;
 *                           every 300 ticks a sync test --
 *                           save, step, load, step the same tick again, and
 *                           the two results must be the same bytes. A summary
 *                           goes to the log every 600 ticks.
 *   UMK3_REPLAY_RECORD=f    the first fight after launch: its setup, every
 *                           tick's sticks and a hash of what the tick did, to
 *                           file f beside the exe.
 *   UMK3_REPLAY_PLAY=f      the first fight after launch runs file f's setup
 *                           and sticks, and compares each tick's hash with the
 *                           recorded one. A record from another build (32/64
 *                           bit, Windows/Linux) tells whether they agree.
 *   UMK3_REPLAY_PAGES=1     Windows, while recording or replaying: a hash of
 *                           every 4 KB page of the state every 120 ticks, to
 *                           f.rec64.pages / f.play64.pages, so a tool can name
 *                           the first variable two runs of one exe disagree on.
 *
 * Recording and replaying hold the frame step at 1 (limeBegin), so the
 * number of ticks in each frame does not depend on the clock.
 *
 * The hashes cross builds, so they read nothing that holds an address: the
 * fighters' sprite list without its links, the health, the event queue
 * (after the step), and the fighters' and camera's positions (at the end of
 * the tick, as floats, to see whether the float maths agrees).
 *
 * Not a state file: what it keeps must survive the loads it does.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#include <unistd.h>
#endif

#include "savestate.h"

extern int32_t  rand_seed;              /* lime_menu.c: limeRand's */
extern uint32_t umk3_rand_seed;         /* fight_runtime.c: the engine's rand() */

#define RB_MAGIC      "UMK3RPL3"
#define RB_EVENT_WORDS 21               /* MKEventQueue: 0x54 bytes */
#define RB_SETUP_MAX  24
#define RB_SYNC_EVERY 300
#define RB_REPORT     600
#define RB_SAMPLE     30
#define RB_REGIONS    3
#define RB_PAGES_EVERY 120
#define RB_PARTS      7

static const char *const g_region_name[RB_REGIONS] = {
    ".umkstd", ".umkstb", "arena"
};

static int g_read;                      /* the switches were read */
static int g_probe;                     /* UMK3_ROLLBACK_PROBE */
static int g_mode;                      /* 0 off, 1 record, 2 replay */
static char g_file[1024];
static FILE *g_f;
static int g_active;                    /* a recorded / replayed fight is on */
static int g_done;                      /* the one fight is over */

static uint32_t g_tick;
static int32_t g_joy[2];
/* A tick's result in parts: hashes of the sprite list, the event queue and
 * the three positions, and the two healths as they are. */
static const char *const g_part_name[RB_PARTS] = {
    "sprites", "health 1", "health 2", "events", "P1 position",
    "P2 position", "camera"
};
static uint32_t g_part[RB_PARTS];       /* this tick's */
static uint32_t g_want[RB_PARTS];       /* replay: the recorded ones */
static uint32_t g_ev[RB_EVENT_WORDS];   /* the event queue as it is */
static uint32_t g_want_ev[RB_EVENT_WORDS];
static uint32_t g_bad[RB_PARTS], g_first[RB_PARTS];
static uint32_t g_bad_ticks;

static int g_pages_on;                  /* UMK3_REPLAY_PAGES */
static FILE *g_pf;

/* the probe's numbers since the last report, and for the whole fight */
static int g_have_prev;                 /* probe slot 0 holds the last tick */
static int g_trial;                     /* a sync test's trial step ran */
static uint64_t g_trial_hash;
static uint32_t g_ticks, g_saves, g_sync_runs, g_sync_bad;
static double g_save_ms, g_save_max, g_diff_ms;
static uint64_t g_pages, g_pages_max, g_region_pages[RB_REGIONS];
static uint8_t *g_ever[RB_REGIONS];     /* per page: ever changed in a tick */
static uint32_t g_ever_n[RB_REGIONS];

/* --------------------------------------------------------------- helpers */

static double now_ms(void)
{
#ifdef _WIN32
    static LARGE_INTEGER f;
    LARGE_INTEGER c;

    if (!f.QuadPart)
        QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart * 1000.0 / (double)f.QuadPart;
#else
    struct timespec t;

    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
#endif
}

/* `name` in the exe's folder; a name with a slash is refused, as
 * UMK3_PROFILES does, so the game keeps to its own folder. */
static int beside_exe(const char *name, char *out, size_t n)
{
    char *sl;

    if (!name || !*name || strchr(name, '/') || strchr(name, '\\'))
        return 0;
#ifdef _WIN32
    if (!GetModuleFileNameA(NULL, out, (DWORD)n))
        return 0;
    sl = strrchr(out, '\\');
#else
    {
        ssize_t k = readlink("/proc/self/exe", out, n - 1);
        if (k <= 0)
            return 0;
        out[k] = 0;
    }
    sl = strrchr(out, '/');
#endif
    if (!sl || (size_t)(sl + 1 - out) + strlen(name) + 1 > n)
        return 0;
    strcpy(sl + 1, name);
    return 1;
}

static void replay_finish(const char *why);

static void at_exit(void)
{
    if (g_active)
        replay_finish("the game closed");
}

static void read_switches(void)
{
    const char *e;

    if (g_read)
        return;
    g_read = 1;
    atexit(at_exit);
    e = getenv("UMK3_ROLLBACK_PROBE");
    g_probe = e && *e && *e != '0';
    if (g_probe && !st_available()) {
        printf("rollback probe: save states are off in this build, "
               "no page or sync measurements\n");
        g_probe = 0;
    }
    e = getenv("UMK3_REPLAY_PAGES");
    g_pages_on = e && *e && *e != '0';
    if (beside_exe(getenv("UMK3_REPLAY_PLAY"), g_file, sizeof g_file))
        g_mode = 2;
    else if (beside_exe(getenv("UMK3_REPLAY_RECORD"), g_file, sizeof g_file))
        g_mode = 1;
    if (g_probe || g_mode)
        printf("rollback probe: %s%s%s\n", g_probe ? "measuring" : "",
               g_probe && g_mode ? ", " : "",
               g_mode == 1 ? "recording the first fight"
               : g_mode == 2 ? "replaying the first fight" : "");
}

static uint32_t fnv(const uint32_t *w, int n)
{
    uint32_t h = 0x811c9dc5u;
    int i;

    for (i = 0; i < n; i++) {
        h = (h ^ (w[i] & 0xff)) * 0x01000193u;
        h = (h ^ ((w[i] >> 8) & 0xff)) * 0x01000193u;
        h = (h ^ ((w[i] >> 16) & 0xff)) * 0x01000193u;
        h = (h ^ (w[i] >> 24)) * 0x01000193u;
    }
    return h;
}

static int wr32(uint32_t v)
{
    unsigned char b[4] = { (unsigned char)v, (unsigned char)(v >> 8),
                           (unsigned char)(v >> 16), (unsigned char)(v >> 24) };
    return fwrite(b, 1, 4, g_f) == 4;
}

static void wr32f(FILE *f, uint32_t v)
{
    unsigned char b[4] = { (unsigned char)v, (unsigned char)(v >> 8),
                           (unsigned char)(v >> 16), (unsigned char)(v >> 24) };
    fwrite(b, 1, 4, f);
}

static int rd32(uint32_t *v)
{
    unsigned char b[4];

    if (fread(b, 1, 4, g_f) != 4)
        return 0;
    *v = b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24;
    return 1;
}

static uint32_t build_bits(void)
{
#ifdef _WIN32
    return (sizeof(void *) == 8 ? 1u : 0u) | 2u;
#else
    return sizeof(void *) == 8 ? 1u : 0u;
#endif
}

static const char *build_name(uint32_t b)
{
    static const char *const n[4] = {
        "Linux 32-bit", "Linux 64-bit", "Windows 32-bit", "Windows 64-bit"
    };
    return n[b & 3];
}

static void replay_finish(const char *why)
{
    int i;

    if (g_mode == 2) {
        printf("replay: %s after %u ticks -- %s\n", why, g_tick,
               g_bad_ticks ? "it DIFFERS from the recording"
                           : "the same as the recording");
        for (i = 0; i < RB_PARTS; i++)
            if (g_bad[i])
                printf("replay:   %-11s differs in %u ticks, first %u\n",
                       g_part_name[i], g_bad[i], g_first[i]);
    } else if (g_mode == 1)
        printf("replay: recorded %u ticks to %s\n", g_tick, g_file);
    if (g_f)
        fclose(g_f);
    g_f = NULL;
    if (g_pf)
        fclose(g_pf);
    g_pf = NULL;
    g_active = 0;
    g_done = 1;
    fflush(stdout);
}

/* ------------------------------------------------------------ the probe */

static void count_page(int r, uint32_t off, void *ctx)
{
    uint32_t pg = off >> 12;

    (void)ctx;
    g_region_pages[r]++;
    if (pg < g_ever_n[r])
        g_ever[r][pg] = 1;
}

/* Ranges of pages that ever changed, largest first, at most `max` lines. */
static void report_ranges(int max)
{
    struct { int r; uint32_t a, b; } top[64];
    int ntop = 0, r, i, j;

    if (max > 64)
        max = 64;
    for (r = 0; r < RB_REGIONS; r++) {
        uint32_t p = 0;

        while (p < g_ever_n[r]) {
            uint32_t q;

            if (!g_ever[r][p]) {
                p++;
                continue;
            }
            for (q = p; q < g_ever_n[r] && g_ever[r][q]; q++)
                ;
            /* keep the largest `max` */
            for (i = 0; i < ntop && top[i].b - top[i].a >= q - p; i++)
                ;
            if (i < max) {
                for (j = (ntop < max ? ntop : max - 1); j > i; j--)
                    top[j] = top[j - 1];
                top[i].r = r;
                top[i].a = p;
                top[i].b = q;
                if (ntop < max)
                    ntop++;
            }
            p = q;
        }
    }
    for (i = 0; i < ntop; i++)
        printf("rollback probe:   %-7s +0x%08x .. +0x%08x  %6.1f KB\n",
               g_region_name[top[i].r], top[i].a << 12, top[i].b << 12,
               (top[i].b - top[i].a) * 4.0);
}

static void report(void)
{
    unsigned long sz[RB_REGIONS];
    uint32_t ever[RB_REGIONS] = { 0, 0, 0 };
    int r;
    uint32_t p;

    for (r = 0; r < RB_REGIONS; r++) {
        sz[r] = st_probe_region_size(r);
        for (p = 0; p < g_ever_n[r]; p++)
            ever[r] += g_ever[r][p];
    }
    printf("rollback probe: tick %u -- state %.1f MB (.umkstd %.2f, .umkstb %.2f, "
           "arena %.2f)\n", g_tick,
           (sz[0] + sz[1] + sz[2]) / 1048576.0, sz[0] / 1048576.0,
           sz[1] / 1048576.0, sz[2] / 1048576.0);
    if (g_ticks)
        printf("rollback probe:   whole save %.2f ms avg, %.2f max; page compare "
               "%.2f ms avg (%u samples)\n"
               "rollback probe:   pages changed in a tick %.1f avg (%.1f KB), %llu max"
               " -- .umkstd %.1f, .umkstb %.1f, arena %.1f\n",
               g_saves ? g_save_ms / g_saves : 0.0, g_save_max,
               g_diff_ms / g_ticks, g_ticks,
               (double)g_pages / g_ticks, g_pages * 4.0 / g_ticks,
               (unsigned long long)g_pages_max,
               (double)g_region_pages[0] / g_ticks,
               (double)g_region_pages[1] / g_ticks,
               (double)g_region_pages[2] / g_ticks);
    printf("rollback probe:   pages ever changed: .umkstd %u, .umkstb %u, arena %u"
           " (%.1f KB in all)\n", ever[0], ever[1], ever[2],
           (ever[0] + ever[1] + ever[2]) * 4.0);
    printf("rollback probe:   sync tests %u, failed %u\n", g_sync_runs, g_sync_bad);
    report_ranges(24);
    fflush(stdout);
    g_ticks = g_saves = 0;
    g_save_ms = g_save_max = g_diff_ms = 0;
    g_pages = g_pages_max = 0;
    memset(g_region_pages, 0, sizeof g_region_pages);
}

static void grow_ever(void)
{
    int r;

    for (r = 0; r < RB_REGIONS; r++) {
        uint32_t n = (uint32_t)((st_probe_region_size(r) + 4095) >> 12);

        if (n > g_ever_n[r]) {
            uint8_t *q = (uint8_t *)realloc(g_ever[r], n);
            if (!q)
                continue;
            memset(q + g_ever_n[r], 0, n - g_ever_n[r]);
            g_ever[r] = q;
            g_ever_n[r] = n;
        }
    }
}

static void print_page(int r, uint32_t off, void *ctx)
{
    int *left = (int *)ctx;

    if (*left > 0)
        printf("rollback probe:   differs: %s +0x%08x\n", g_region_name[r], off);
    (*left)--;
}

/* --------------------------------------------------------------- the API */

int rb_fixed_step(void)
{
    return g_active;
}

/* At Task_GameInit's start, before anything loads. `setup` is the fight's:
 * the two models, the level, the AI, the game mode, the settings. Recording
 * writes it; replaying puts the recorded one in and returns 1 (the caller
 * copies it back), and both seed the two generators the same. */
int rb_fight_setup(int32_t *setup, int n)
{
    uint32_t i, m, b, v;
    char magic[8];

    read_switches();
    if (g_active)
        replay_finish("a new fight began");
    if (!g_mode || g_done || n > RB_SETUP_MAX)
        return 0;

    g_tick = 0;
    if (g_pages_on && st_available()) {
        char pf[1100];

        snprintf(pf, sizeof pf, "%s.%s%d.pages", g_file,
                 g_mode == 1 ? "rec" : "play", sizeof(void *) == 8 ? 64 : 32);
        g_pf = fopen(pf, "wb");
        if (g_pf)
            printf("replay: page hashes every %d ticks to %s\n",
                   RB_PAGES_EVERY, pf);
    }
    if (g_mode == 1) {
        g_f = fopen(g_file, "wb");
        if (!g_f) {
            printf("replay: cannot write %s\n", g_file);
            g_done = 1;
            return 0;
        }
        fwrite(RB_MAGIC, 1, 8, g_f);
        wr32(build_bits());
        wr32((uint32_t)n);
        for (i = 0; i < (uint32_t)n; i++)
            wr32((uint32_t)setup[i]);
        wr32((uint32_t)rand_seed);
        wr32(umk3_rand_seed);
        printf("replay: recording to %s (%s)\n", g_file, build_name(build_bits()));
        g_active = 1;
        return 0;
    }

    g_f = fopen(g_file, "rb");
    if (!g_f || fread(magic, 1, 8, g_f) != 8 || memcmp(magic, RB_MAGIC, 8)
        || !rd32(&b) || !rd32(&m) || m != (uint32_t)n) {
        printf("replay: %s is missing or not a replay of this version\n", g_file);
        if (g_f)
            fclose(g_f);
        g_f = NULL;
        g_done = 1;
        return 0;
    }
    for (i = 0; i < m; i++) {
        rd32(&v);
        if ((int32_t)v != setup[i])
            printf("replay: setup word %u was %d, now %d -- using the recorded one\n",
                   i, (int)v, (int)setup[i]);
        setup[i] = (int32_t)v;
    }
    rd32(&v);
    rand_seed = (int32_t)v;
    rd32(&umk3_rand_seed);
    printf("replay: playing %s, recorded on %s, here %s\n", g_file,
           build_name(b), build_name(build_bits()));
    fflush(stdout);
    g_active = 1;
    return 1;
}

/* Before the tick's mk3_update. Replaying swaps in the recorded sticks.
 * Returns 1 when the sync test wants a trial step first: the caller steps,
 * calls rb_trial_done, and steps again for real. */
int rb_step_begin(int32_t joy[2])
{
    uint32_t a, b;

    read_switches();
    if (g_active && g_mode == 2) {
        int i, ok = rd32(&a) && rd32(&b);

        for (i = 0; ok && i < RB_PARTS; i++)
            ok = rd32(&g_want[i]);
        for (i = 0; ok && i < RB_EVENT_WORDS; i++)
            ok = rd32(&g_want_ev[i]);
        if (!ok)
            replay_finish("the recording ended");
        else {
            joy[0] = (int32_t)a;
            joy[1] = (int32_t)b;
        }
    }
    g_joy[0] = joy[0];
    g_joy[1] = joy[1];

    g_trial = 0;
    if (g_probe && g_tick % RB_SYNC_EVERY == RB_SYNC_EVERY - 1
        && st_probe_save(1) == 0) {
        g_trial = 1;
        return 1;
    }
    return 0;
}

/* After the trial step: keep its result, put the tick's start back. */
void rb_trial_done(void)
{
    g_trial_hash = st_probe_hash();
    st_probe_save(2);
    st_probe_load(1);
}

/* After the real step. `w` is the step's result in words that hold no
 * address (GameCode.c, rb_sim_words). */
void rb_step_end(const uint32_t *w, int n)
{
    /* GameCode.c's rb_sim_end: 24 words of sprites, the two healths, then
     * the event queue */
    g_part[0] = fnv(w, 24);
    g_part[1] = w[24];
    g_part[2] = w[25];
    g_part[3] = fnv(w + 26, n - 26);
    memset(g_ev, 0, sizeof g_ev);
    memcpy(g_ev, w + 26, sizeof(uint32_t) * (n - 26 < RB_EVENT_WORDS
                                             ? n - 26 : RB_EVENT_WORDS));

    if (g_trial) {
        g_sync_runs++;
        if (st_probe_hash() != g_trial_hash) {
            int left = 16;
            unsigned long k;

            g_sync_bad++;
            printf("rollback probe: SYNC TEST FAILED at tick %u: the same tick "
                   "stepped twice from the same state gave different memory\n",
                   g_tick);
            k = st_probe_diff(2, print_page, &left);
            printf("rollback probe:   %lu pages differ\n", k);
        }
    }

}

/* The page hashes of the whole state, for a tool to compare two runs of
 * one exe: the tick, then per region its page count and the hashes. */
static void write_pages(void)
{
    static uint64_t *h;
    static unsigned long cap;
    unsigned long n;
    int r;

    wr32f(g_pf, g_tick);
    for (r = 0; r < RB_REGIONS; r++) {
        n = (st_probe_region_size(r) + 4095) >> 12;
        if (n > cap) {
            uint64_t *q = (uint64_t *)realloc(h, n * sizeof *h);
            if (q) {
                h = q;
                cap = n;
            }
        }
        n = st_probe_page_hashes(r, h, n < cap ? n : cap);
        wr32f(g_pf, (uint32_t)n);
        fwrite(h, sizeof *h, n, g_pf);
    }
    fflush(g_pf);
}

/* At the end of the tick, after the camera. `w`: the fighters' and the
 * camera's positions, as the floats' bits. */
void rb_tick_end(const uint32_t *w, int n)
{
    int i, bad = 0;

    (void)n;
    g_part[4] = fnv(w, 3);
    g_part[5] = fnv(w + 3, 3);
    g_part[6] = fnv(w + 6, 3);

    if (g_active && g_mode == 1) {
        wr32((uint32_t)g_joy[0]);
        wr32((uint32_t)g_joy[1]);
        for (i = 0; i < RB_PARTS; i++)
            wr32(g_part[i]);
        for (i = 0; i < RB_EVENT_WORDS; i++)
            wr32(g_ev[i]);
    } else if (g_active && g_mode == 2) {
        for (i = 0; i < RB_PARTS; i++)
            if (g_part[i] != g_want[i]) {
                if (!g_bad[i])
                    g_first[i] = g_tick;
                g_bad[i]++;
                bad = 1;
            }
        if (bad && g_bad_ticks++ < 8) {
            printf("replay: tick %u DIFFERS (sticks %08x %08x):", g_tick,
                   (unsigned)g_joy[0], (unsigned)g_joy[1]);
            for (i = 0; i < RB_PARTS; i++)
                if (g_part[i] != g_want[i]) {
                    if (i == 1 || i == 2)
                        printf(" %s %d, recorded %d;", g_part_name[i],
                               (int)g_part[i], (int)g_want[i]);
                    else
                        printf(" %s;", g_part_name[i]);
                }
            printf("\n");
            /* the queue: a count, then ten records of two words */
            for (i = 0; i < RB_EVENT_WORDS; i++)
                if (g_ev[i] != g_want_ev[i]) {
                    if (i == 0)
                        printf("replay:   event count %u, recorded %u\n",
                               g_ev[0], g_want_ev[0]);
                    else
                        printf("replay:   event %d word %d: %08x, recorded %08x\n",
                               (i - 1) / 2, (i - 1) % 2, g_ev[i], g_want_ev[i]);
                }
            fflush(stdout);
        }
    }
    if (g_active && g_pf && g_tick % RB_PAGES_EVERY == 0)
        write_pages();

    if (g_active && g_mode == 1 && g_tick % 60 == 59)
        fflush(g_f);

    /* Every tick would cost two whole copies of the state: a sample, one
     * tick in RB_SAMPLE -- saved at its end, compared at the next one's. */
    if (g_probe) {
        double t0, t1;
        unsigned long k;

        if (g_tick % RB_SAMPLE == 0) {
            grow_ever();
            t0 = now_ms();
            g_have_prev = st_probe_save(0) == 0;
            t1 = now_ms();
            if (g_have_prev) {
                g_saves++;
                g_save_ms += t1 - t0;
                if (t1 - t0 > g_save_max)
                    g_save_max = t1 - t0;
            }
        } else if (g_tick % RB_SAMPLE == 1 && g_have_prev) {
            t0 = now_ms();
            k = st_probe_diff(0, count_page, NULL);
            t1 = now_ms();
            g_have_prev = 0;
            g_ticks++;
            g_pages += k;
            if (k > g_pages_max)
                g_pages_max = k;
            g_diff_ms += t1 - t0;
        }
        if (g_tick % RB_REPORT == RB_REPORT - 1)
            report();
    }
    g_tick++;
}
