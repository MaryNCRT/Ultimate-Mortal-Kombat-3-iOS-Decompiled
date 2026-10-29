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
 * REPLAYS (the launcher's page of that name): with none of the switches,
 * every fight but an online one is recorded the same way to
 * replays/recent/<date>_<p1>_vs_<p2>.umkr beside the exe, and only the
 * newest RB_RECENT stay (umk3.ini replays=0 turns it off). The launcher keeps
 * one for good by moving it to replays/saved/. `umk3-game --replay <file>`
 * (a path inside the exe's folder) goes straight into that fight and plays
 * it with the player below (rp_*): pause, a frame at a time, five seconds
 * back or on, speed, a bar to click. Going back loads the nearest keyframe
 * -- a rollback snapshot (st_rb_*) taken every so many ticks -- and runs the
 * ticks between silent and undrawn. A tick whose result differs from the
 * recorded one goes to the log, as with UMK3_REPLAY_PLAY.
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
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#ifndef _WIN32
#include <sys/stat.h>
#include <dirent.h>
#endif

#include "savestate.h"
#include "rollback.h"

extern int32_t  rand_seed;              /* lime_menu.c: limeRand's */
extern uint32_t umk3_rand_seed;         /* fight_runtime.c: the engine's rand() */

#define RB_MAGIC      "UMK3RPL4"
/* RPL3: the same, but setup[1] may not be the rival fought (rb_fight_rival) */
#define RB_MAGIC_OLD  "UMK3RPL3"
#define RB_MAGIC_OK(m) (!memcmp((m), RB_MAGIC, 8) || !memcmp((m), RB_MAGIC_OLD, 8))
#define RB_EVENT_WORDS 21               /* MKEventQueue: 0x54 bytes */
#define RB_SETUP_MAX  24
#define RB_SYNC_EVERY 300
#define RB_REPORT     600
#define RB_SAMPLE     30
#define RB_REGIONS    3
#define RB_PAGES_EVERY 120
#define RB_PARTS      7
#define RB_TICK_BYTES (4 * (2 + RB_PARTS + RB_EVENT_WORDS))
#define RB_RECENT     5                 /* replays/recent keeps the newest */
#define RB_MIN_TICKS  60                /* a shorter recording is not kept */

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

/* REPLAYS */
static int g_auto_want = 1;             /* umk3.ini replays= (rb_replay_auto) */
static int g_auto;                      /* recording every fight */
static char g_play_rel[512];            /* --replay */
static long g_data_at;                  /* the file's first tick */
static uint32_t g_total;                /* replaying: the ticks in the file */
static int g_have_want;                 /* this tick's recorded result was read */
static int g_reported;                  /* the end's summary is in the log */
static int g_switching;                 /* on the way to another replay (rl_pick) */

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

/* `rel` in the exe's folder. Only a path that stays inside it: no "..", no
 * drive, not from the root, so the game keeps to its own folder. */
static int in_exe_dir(const char *rel, char *out, size_t n)
{
    char *sl;

    if (!rel || !*rel || strstr(rel, "..") || strchr(rel, ':')
        || rel[0] == '/' || rel[0] == '\\')
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
    if (!sl || (size_t)(sl + 1 - out) + strlen(rel) + 1 > n)
        return 0;
    strcpy(sl + 1, rel);
    return 1;
}

/* `name` in the exe's folder; a name with a slash is refused, as
 * UMK3_PROFILES does. */
static int beside_exe(const char *name, char *out, size_t n)
{
    if (!name || strchr(name, '/') || strchr(name, '\\'))
        return 0;
    return in_exe_dir(name, out, n);
}

static void make_dir(const char *p)
{
#ifdef _WIN32
    CreateDirectoryA(p, NULL);
#else
    mkdir(p, 0755);
#endif
}

/* The .umkr files of replays/<folder>, oldest first (the game's own names
 * begin with the date; others sort by name), at most `max`. */
#define RB_NAME 160

static int scan_replays(const char *folder, char names[][RB_NAME], int max)
{
    char dir[1024], p[1200], rel[64];
    int n = 0, i, j;

    snprintf(rel, sizeof rel, "replays/%s", folder);
    if (!in_exe_dir(rel, dir, sizeof dir))
        return 0;
#ifdef _WIN32
    {
        WIN32_FIND_DATAA fd;
        HANDLE h;

        snprintf(p, sizeof p, "%s\\*.umkr", dir);
        h = FindFirstFileA(p, &fd);
        if (h == INVALID_HANDLE_VALUE)
            return 0;
        do
            if (n < max && strlen(fd.cFileName) < RB_NAME)
                strcpy(names[n++], fd.cFileName);
        while (FindNextFileA(h, &fd));
        FindClose(h);
    }
#else
    {
        DIR *d = opendir(dir);
        struct dirent *e;

        if (!d)
            return 0;
        while ((e = readdir(d)) != NULL) {
            size_t k = strlen(e->d_name);
            if (n < max && k > 5 && k < RB_NAME && !strcmp(e->d_name + k - 5, ".umkr"))
                strcpy(names[n++], e->d_name);
        }
        closedir(d);
    }
#endif
    for (i = 1; i < n; i++)
        for (j = i; j > 0 && strcmp(names[j - 1], names[j]) > 0; j--) {
            char t[RB_NAME];
            strcpy(t, names[j]);
            strcpy(names[j], names[j - 1]);
            strcpy(names[j - 1], t);
        }
    return n;
}

/* replays/recent: the oldest go until RB_RECENT are left. */
static void keep_recent(void)
{
    char names[64][RB_NAME], p[1200], dir[1024];
    int n = scan_replays("recent", names, 64), i;

    if (!in_exe_dir("replays/recent", dir, sizeof dir))
        return;
    for (i = 0; i < n - RB_RECENT; i++) {
        snprintf(p, sizeof p, "%s/%s", dir, names[i]);
        remove(p);
    }
}

/* The setup's fighter as a file name's piece: "sub-zero", "kunglao". */
extern const char *CharacterNames[26];

static void name_piece(char *out, int n, int32_t who)
{
    const char *s = who >= 0 && who < 26 ? CharacterNames[who] : "unknown";
    int k = 0;

    for (; *s && k < n - 1; s++) {
        int c = (unsigned char)*s;
        if (c >= 'A' && c <= 'Z')
            c += 32;
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')
            out[k++] = (char)c;
    }
    out[k] = 0;
}

/* game_main.c: umk3.ini's replays= and --replay, before the first fight */
void rb_replay_auto(int on) { g_auto_want = on; }

void rb_replay_open(const char *rel)
{
    snprintf(g_play_rel, sizeof g_play_rel, "%s", rel);
}

/* The setup words --replay's fight is started from: the two fighters and
 * the level (rb_setup puts the rest back). 0 if the file is not a replay. */
int rb_replay_peek(int32_t *c1, int32_t *c2, int32_t *level)
{
    char path[1024], magic[8];
    unsigned char b[4 * 8];
    FILE *f;
    int ok;

    if (!in_exe_dir(g_play_rel, path, sizeof path) || !(f = fopen(path, "rb")))
        return 0;
    ok = fread(magic, 1, 8, f) == 8 && RB_MAGIC_OK(magic)
         && fread(b, 1, sizeof b, f) == sizeof b;
    fclose(f);
    if (!ok)
        return 0;
    /* the build, the count, then setup[0] [1] [2] .. [5] */
#define W(i) (int32_t)(b[4 * (i)] | b[4 * (i) + 1] << 8 | b[4 * (i) + 2] << 16 \
                       | (uint32_t)b[4 * (i) + 3] << 24)
    *c1 = W(2);
    *c2 = W(3);
    *level = W(7);
#undef W
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
    if (g_play_rel[0] && in_exe_dir(g_play_rel, g_file, sizeof g_file))
        g_mode = 2;
    else if (beside_exe(getenv("UMK3_REPLAY_PLAY"), g_file, sizeof g_file))
        g_mode = 2;
    else if (beside_exe(getenv("UMK3_REPLAY_RECORD"), g_file, sizeof g_file))
        g_mode = 1;
    else if (g_auto_want)
        g_mode = g_auto = 1;
    if (g_probe || g_mode)
        printf("rollback probe: %s%s%s\n", g_probe ? "measuring" : "",
               g_probe && g_mode ? ", " : "",
               g_auto ? "every fight to replays/recent"
               : g_mode == 1 ? "recording the first fight"
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

/* The rival is only known later than the setup is written: Arcade's VS
 * screen (FE_Task_VS_Screen_Init) runs inside the fight's loading and puts
 * the ladder's opponent -- or a random one -- in Character2 and clears
 * Character2Override. The setup kept the opponent of the screen before, so
 * a replay named and loaded the wrong one (Mary: "liukang_vs_stryker" was a
 * fight with Kitana). Task_GameInit step 42, once player two is decided:
 * recording, setup[1] is written again with the real one and the file
 * takes its name at the end; replaying, the recorded one is put back. */
static int32_t g_rival = -1;            /* replaying: setup[1] as recorded */
static int32_t g_rec_rival = -1;        /* recording: the real one */

void rb_fight_rival(long *c2)
{
    if (!g_active || !g_f)
        return;
    if (g_mode == 1) {
        long at = ftell(g_f);
        fseek(g_f, 8 + 4 + 4 + 4 * 1, SEEK_SET);
        wr32((uint32_t)*c2);
        fseek(g_f, at, SEEK_SET);
        g_rec_rival = (int32_t)*c2;
    } else if (g_mode == 2 && g_rival >= 0 && g_rival < 26 && *c2 != g_rival) {
        printf("replay: player two was %ld, the recorded %d\n", *c2, (int)g_rival);
        *c2 = g_rival;
    }
}

/* the recording's name with its real rival: ".._vs_<old>.umkr" */
static void rename_rival(void)
{
    char piece[32], to[1024], *vs;
    if (g_rec_rival < 0 || !(vs = strstr(g_file, "_vs_")))
        return;
    name_piece(piece, sizeof piece, g_rec_rival);
    snprintf(to, sizeof to, "%.*s_vs_%s.umkr", (int)(vs - g_file), g_file, piece);
    if (strcmp(to, g_file) != 0 && rename(g_file, to) == 0) {
        printf("replay: renamed to %s\n", to);
        snprintf(g_file, sizeof g_file, "%s", to);
    }
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
    } else if (g_mode == 1 && g_f)
        printf("replay: recorded %u ticks to %s\n", g_tick, g_file);
    if (g_f)
        fclose(g_f);
    g_f = NULL;
    if (g_pf)
        fclose(g_pf);
    g_pf = NULL;
    g_active = 0;
    g_done = !g_auto;                   /* REPLAYS records every fight */
    if (g_auto && g_mode == 1) {
        if (g_tick < RB_MIN_TICKS)
            remove(g_file);
        else
            rename_rival();
        keep_recent();
    }
    g_rec_rival = -1;
    fflush(stdout);
}

/* Task_GameDestroy: the fight is over, its recording is whole. */
void rb_replay_fight_over(void)
{
    if (g_active && g_mode == 1)
        replay_finish("the fight ended");
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
    g_switching = 0;
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
    if (g_mode == 1 && g_auto) {
        char dir[1024], a[32], b2[32], stamp[32];
        time_t now = time(NULL);
        struct tm *tm = localtime(&now);

        if (n < 7 || setup[6] == 1)     /* ONLINE: rollback.c's own inputs */
            return 0;
        if (!in_exe_dir("replays", dir, sizeof dir))
            return 0;
        make_dir(dir);
        in_exe_dir("replays/recent", dir, sizeof dir);
        make_dir(dir);
        strftime(stamp, sizeof stamp, "%Y%m%d-%H%M%S", tm);
        name_piece(a, sizeof a, setup[0]);
        name_piece(b2, sizeof b2, setup[1]);
        snprintf(g_file, sizeof g_file, "%s/%s_%s_vs_%s.umkr", dir, stamp, a, b2);
    }
    if (g_mode == 1) {
        g_f = fopen(g_file, "wb");
        if (!g_f) {
            printf("replay: cannot write %s\n", g_file);
            g_done = !g_auto;
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
    if (!g_f || fread(magic, 1, 8, g_f) != 8 || !RB_MAGIC_OK(magic)
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
    g_rival = n > 1 && !memcmp(magic, RB_MAGIC, 8) ? setup[1] : -1;
    rd32(&v);
    rand_seed = (int32_t)v;
    rd32(&umk3_rand_seed);
    g_data_at = ftell(g_f);
    fseek(g_f, 0, SEEK_END);
    g_total = (uint32_t)((ftell(g_f) - g_data_at) / RB_TICK_BYTES);
    fseek(g_f, g_data_at, SEEK_SET);
    printf("replay: playing %s (%u ticks), recorded on %s, here %s\n", g_file,
           g_total, build_name(b), build_name(build_bits()));
    fflush(stdout);
    g_done = 1;                         /* the one fight */
    g_reported = 0;
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
    g_have_want = 0;
    if (g_switching) {
        /* leaving for another replay's fight: nobody plays this one */
        joy[0] = joy[1] = 0;
    } else if (g_active && g_mode == 2 && g_play_rel[0] && g_tick >= g_total) {
        /* the player holds the last frame: past the end, the sticks rest */
        joy[0] = joy[1] = 0;
    } else if (g_active && g_mode == 2) {
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
            g_have_want = 1;
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
    } else if (g_active && g_mode == 2 && g_have_want) {
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

/* ------------------------------------------------------------ the player */

/* --replay's fight, watched: the main loop calls rp_frame where it would
 * call GameCodeMain, and rp_keys / rp_ui with the player's keys and mouse. */

extern int CurrentTask;                 /* GameCode.c: 6 is the fight */
extern char PreloadedCharacters[41148][0x5f4];  /* read only by a fight */
void rb_silent(int quiet, int nodraw);  /* rollback.c */

#define RP_KEYS_MAX  48                 /* keyframes, the last slot for holds */
#define RP_JUMP      300                /* ticks: five seconds */
#define RP_BUDGET_MS 20.0               /* silent ticks a frame while seeking */
#define RP_NONE      0xffffffffu

static const int k_speed_num[5] = { 1, 1, 1, 2, 4 };
static const int k_speed_div[5] = { 4, 2, 1, 1, 1 };

static int      g_rp_snap;              /* 0 not yet, 1 keyframes, -1 none */
static int      g_rp_nkeys, g_rp_slots, g_rp_every;
static uint32_t g_rp_key[RP_KEYS_MAX];
static uint32_t g_rp_target = RP_NONE;
static int      g_rp_paused, g_rp_step, g_rp_speed = 2, g_rp_sub;

int rp_playing(void)
{
    return g_mode == 2 && g_active && g_play_rel[0];
}

/* The file's place and the tick count together: what a loaded snapshot
 * does not put back, since this file is not part of the state. */
static void rp_set_tick(uint32_t t)
{
    g_tick = t;
    fseek(g_f, g_data_at + (long)t * RB_TICK_BYTES, SEEK_SET);
}

static void rp_begin(void)
{
    g_rp_snap = -1;
    if (!st_available())
        return;
    g_rp_every = (int)(g_total / (RP_KEYS_MAX - 2)) + 1;
    if (g_rp_every < 120)
        g_rp_every = 120;
    g_rp_slots = (int)(g_total / (uint32_t)g_rp_every) + 2;
    if (g_rp_slots > RP_KEYS_MAX - 1)
        g_rp_slots = RP_KEYS_MAX - 1;
    st_rb_skip(PreloadedCharacters, sizeof PreloadedCharacters);
    if (st_rb_begin(g_rp_slots + 1) != 0) {
        printf("replay: no keyframes -- the player cannot go back\n");
        return;
    }
    g_rp_snap = 1;
    g_rp_nkeys = 0;
    printf("replay: a keyframe every %d ticks, %d at most\n", g_rp_every, g_rp_slots);
}

/* A keyframe once the tick reaches the next one's place. */
static void rp_keyframe(void)
{
    if (g_rp_snap != 1 || g_rp_nkeys >= g_rp_slots
        || g_tick < (uint32_t)g_rp_nkeys * (uint32_t)g_rp_every)
        return;
    if (st_rb_save(g_rp_nkeys) == 0)
        g_rp_key[g_rp_nkeys++] = g_tick;
}

static void rp_run(void (*frame)(void), int quiet, int nodraw)
{
    rb_silent(quiet, nodraw);
    frame();
    rb_silent(0, 0);
}

/* Draws the next frame and puts the state back: a still picture. */
static void rp_hold(void (*frame)(void))
{
    uint32_t t = g_tick;

    if (g_rp_snap != 1 || st_rb_save(g_rp_slots) != 0)
        return;
    rp_run(frame, 1, 0);
    st_rb_load(g_rp_slots);
    rp_set_tick(t);
}

/* To tick `t`: back to the nearest keyframe at or before it when it is
 * behind (or a keyframe is nearer than where the fight is), then on, silent
 * and undrawn, a few milliseconds a frame (rp_frame). */
static void rp_seek(uint32_t t)
{
    int k;

    if (t > g_total)
        t = g_total;
    if (g_rp_snap == 1 && g_rp_nkeys) {
        for (k = g_rp_nkeys - 1; k > 0 && g_rp_key[k] > t; k--)
            ;
        if (g_rp_key[k] <= t && (t < g_tick || g_rp_key[k] > g_tick)
            && st_rb_load(k) == 0)
            rp_set_tick(g_rp_key[k]);
    }
    g_rp_target = t > g_tick ? t : RP_NONE;
    g_rp_step = 0;
}

static void rp_report_end(void)
{
    int i;

    if (g_reported)
        return;
    g_reported = 1;
    printf("replay: the end, %u ticks -- %s\n", g_total,
           g_bad_ticks ? "it DIFFERS from the recording"
                       : "the same as the recording");
    for (i = 0; i < RB_PARTS; i++)
        if (g_bad[i])
            printf("replay:   %-11s differs in %u ticks, first %u\n",
                   g_part_name[i], g_bad[i], g_first[i]);
    fflush(stdout);
}

void rp_frame(void (*frame)(void))
{
    double t0;
    int i;

    if (CurrentTask != 6) {
        frame();
        return;
    }
    if (!g_rp_snap)
        rp_begin();

    if (g_rp_target != RP_NONE) {       /* on to the tick asked for */
        t0 = now_ms();
        while (g_tick < g_rp_target && g_tick < g_total
               && now_ms() - t0 < RP_BUDGET_MS && CurrentTask == 6) {
            rp_keyframe();
            rp_run(frame, 1, 1);
        }
        if (g_tick >= g_rp_target || g_tick >= g_total)
            g_rp_target = RP_NONE;
        rp_hold(frame);
        return;
    }
    if (g_tick >= g_total) {
        g_rp_paused = 1;
        rp_report_end();
    }
    if (g_rp_paused) {
        if (g_rp_step && g_tick < g_total) {
            g_rp_step = 0;
            rp_keyframe();
            rp_run(frame, 0, 0);
        } else
            rp_hold(frame);
        return;
    }
    if (++g_rp_sub % k_speed_div[g_rp_speed]) {     /* slow motion */
        rp_hold(frame);
        return;
    }
    for (i = 1; i < k_speed_num[g_rp_speed] && g_tick < g_total; i++) {
        rp_keyframe();
        rp_run(frame, 1, 1);
    }
    rp_keyframe();
    rp_run(frame, 0, 0);
}

/* ------------------------------------------------- the player's list */

/* The list the panel's REPLAYS button opens: the last fights, then the kept
 * ones, newest first. Picking another leaves this fight for that one's, in
 * the same game (rl_pick). */

#define RL_RECENT 5
#define RL_SAVED  8

static char g_rl_rel[RL_RECENT + RL_SAVED][RB_NAME + 16];
static char g_rl_label[RL_RECENT + RL_SAVED][48];
static int  g_rl_n, g_rl_recent, g_rl_open;

/* "20261029-120102_sub-zero_vs_kitana.umkr" -> "SUB-ZERO VS KITANA  10-29 12:01";
 * another name, itself without ".umkr". */
static void rl_label(char *out, int n, const char *file)
{
    char a[40], b[40];
    unsigned d, t;

    if (sscanf(file, "%8u-%6u_%39[^_]_vs_%39[^.].umkr", &d, &t, a, b) == 4)
        snprintf(out, (size_t)n, "%s VS %s  %02u-%02u %02u:%02u", a, b,
                 d / 100 % 100, d % 100, t / 10000, t / 100 % 100);
    else {
        snprintf(out, (size_t)n, "%s", file);
        if (strlen(out) > 5 && !strcmp(out + strlen(out) - 5, ".umkr"))
            out[strlen(out) - 5] = 0;
    }
}

static void rl_scan(void)
{
    char names[64][RB_NAME];
    int n, i, k;

    g_rl_n = 0;
    n = scan_replays("recent", names, 64);
    for (i = n - 1, k = 0; i >= 0 && k < RL_RECENT; i--, k++) {
        snprintf(g_rl_rel[g_rl_n], sizeof g_rl_rel[0], "replays/recent/%s", names[i]);
        rl_label(g_rl_label[g_rl_n++], sizeof g_rl_label[0], names[i]);
    }
    g_rl_recent = g_rl_n;
    n = scan_replays("saved", names, 64);
    for (i = n - 1, k = 0; i >= 0 && k < RL_SAVED; i--, k++) {
        snprintf(g_rl_rel[g_rl_n], sizeof g_rl_rel[0], "replays/saved/%s", names[i]);
        rl_label(g_rl_label[g_rl_n++], sizeof g_rl_label[0], names[i]);
    }
}

/* The list for drawing: its labels, how many are recent, which is playing
 * (-1 none); -1 when it is closed. */
int rp_list(const char **labels, int max, int *n_recent, int *current)
{
    int i;

    *current = -1;
    *n_recent = g_rl_recent;
    if (!g_rl_open || !rp_playing())
        return -1;
    for (i = 0; i < g_rl_n && i < max; i++) {
        labels[i] = g_rl_label[i];
        if (!strcmp(g_rl_rel[i], g_play_rel))
            *current = i;
    }
    return i;
}

/* Another replay picked: this one is over, the next is read when its fight
 * begins (rb_fight_setup). While the game leaves this fight and starts that
 * one, the sticks rest and rp_switching says so. */
int rp_switching(void)
{
    return g_switching;
}

/* A row of the list picked: the one playing closes the list; another one,
 * a replay, ends this one (1) -- the caller starts its fight. */
static int rl_pick(int i)
{
    char was[sizeof g_play_rel];
    int32_t c1, c2, lv;

    if (i < 0 || i >= g_rl_n)
        return 0;
    g_rl_open = 0;
    if (!strcmp(g_rl_rel[i], g_play_rel))
        return 0;
    memcpy(was, g_play_rel, sizeof was);
    rb_replay_open(g_rl_rel[i]);
    if (!rb_replay_peek(&c1, &c2, &lv) || c1 < 0 || c1 >= 26 || c2 < 0
        || c2 >= 26 || !in_exe_dir(g_play_rel, g_file, sizeof g_file)) {
        printf("replay: %s is not a replay\n", g_rl_rel[i]);
        rb_replay_open(was);
        in_exe_dir(g_play_rel, g_file, sizeof g_file);
        return 0;
    }
    printf("replay: switching to %s\n", g_play_rel);
    if (g_active)
        replay_finish("switched to another replay");
    if (g_rp_snap == 1)
        st_rb_end();
    g_rp_snap = 0;
    g_rp_target = RP_NONE;
    g_rp_paused = g_rp_step = g_rp_sub = 0;
    g_rp_speed = 2;
    memset(g_bad, 0, sizeof g_bad);
    memset(g_first, 0, sizeof g_first);
    g_bad_ticks = 0;
    g_done = 0;
    g_switching = 1;
    return 1;
}

/* The playing replay's label without its date, for the status line. */
const char *rp_title(void)
{
    static char t[48];
    const char *s = strrchr(g_play_rel, '/');
    char *date;

    rl_label(t, sizeof t, s ? s + 1 : g_play_rel);
    if ((date = strstr(t, "  ")) != NULL)
        *date = 0;
    t[34] = 0;
    return t;
}

/* One of the player's buttons (rollback.h, RPB_*), from a key or a click. */
void rp_press(int id)
{
    if (!rp_playing())
        return;
    switch (id) {
    case RPB_LIST:
        g_rl_open = !g_rl_open;
        if (g_rl_open)
            rl_scan();
        break;
    case RPB_START:
        rp_seek(0);
        break;
    case RPB_BACK5:
        rp_seek(g_tick > RP_JUMP ? g_tick - RP_JUMP : 0);
        break;
    case RPB_FRAME_BACK:
        g_rp_paused = 1;
        rp_seek(g_tick ? g_tick - 1 : 0);
        break;
    case RPB_PLAY:
        if (g_tick >= g_total) {        /* at the end: again from the start */
            rp_seek(0);
            g_rp_paused = 0;
        } else
            g_rp_paused = !g_rp_paused;
        break;
    case RPB_FRAME_FWD:
        g_rp_paused = 1;
        g_rp_step = 1;
        break;
    case RPB_FWD5:
        rp_seek(g_tick + RP_JUMP);
        break;
    case RPB_SLOWER:
        if (g_rp_speed > 0)
            g_rp_speed--;
        break;
    case RPB_FASTER:
        if (g_rp_speed < 4)
            g_rp_speed++;
        break;
    }
}

/* Held: the jumps and the frame steps go on while it stays down. */
static int rp_repeats(int id, int held)
{
    if (id != RPB_BACK5 && id != RPB_FWD5 && id != RPB_FRAME_BACK
        && id != RPB_FRAME_FWD)
        return 0;
    return held > 20 && (held - 20) % (g_rp_paused ? 2 : 8) == 0;
}

/* The keys, as held this frame: play, back, on, faster, slower, start.
 * Back and on are a frame while paused, five seconds while playing. */
void rp_keys(int play, int back, int fwd, int faster, int slower, int start)
{
    static int was[6], held[6];
    int now[6], id[6], i;

    now[0] = play; now[1] = back; now[2] = fwd;
    now[3] = faster; now[4] = slower; now[5] = start;
    id[0] = RPB_PLAY;
    id[1] = g_rp_paused ? RPB_FRAME_BACK : RPB_BACK5;
    id[2] = g_rp_paused ? RPB_FRAME_FWD : RPB_FWD5;
    id[3] = RPB_FASTER; id[4] = RPB_SLOWER; id[5] = RPB_START;
    for (i = 0; i < 6; i++) {
        held[i] = now[i] ? held[i] + 1 : 0;
        if ((now[i] && !was[i]) || (now[i] && rp_repeats(id[i], held[i])))
            rp_press(id[i]);
        was[i] = now[i];
    }
}

/* The mouse on the panel: `id` the button under it (-1 none), `at` where
 * along the bar it is, 0..1. A press acts, a drag along the bar follows it.
 * Returns the button pressed this frame, for what the caller does itself
 * (RPB_SETTINGS), else -1. */
int rp_ui(int id, float at, int down)
{
    static int was = -1, held;
    static uint32_t last;
    uint32_t t;

    if (!rp_playing() || !down) {
        was = -1;
        held = 0;
        return -1;
    }
    if (id == RPB_BAR || was == RPB_BAR) {
        if (was != RPB_BAR && was != -1)
            return -1;                  /* pressed elsewhere, slid onto it */
        at = at < 0 ? 0 : at > 1 ? 1 : at;
        t = (uint32_t)(at * (float)g_total);
        if (was != RPB_BAR || (t > last ? t - last : last - t) > 30) {
            rp_seek(t);
            last = t;
        }
        was = RPB_BAR;
        return -1;
    }
    if (id != was) {
        int first = was == -1;

        was = id;
        held = 0;
        if (!first)
            return -1;                  /* slid onto it: not a press */
        if (id < 0) {                   /* a click beside the list closes it */
            g_rl_open = 0;
            return -1;
        }
        if (id >= RPB_ROW)
            return rl_pick(id - RPB_ROW) ? RPB_SWITCH : -1;
        rp_press(id);
        return id;
    }
    if (rp_repeats(id, ++held))
        rp_press(id);
    return -1;
}

/* What the panel shows: the time, and paused / speed (0..4, x1/4 .. x4) /
 * seeking / at the end / differs from the recording. Returns how far the
 * fight is, 0..1. */
float rp_info(char *time, int n, int *state)
{
    unsigned t = g_tick < g_total ? g_tick : g_total;

    snprintf(time, (size_t)n, "%u:%02u.%02u / %u:%02u.%02u",
             t / 3600, t / 60 % 60, t % 60 * 100 / 60,
             g_total / 3600, g_total / 60 % 60, g_total % 60 * 100 / 60);
    *state = g_rp_speed
             | (g_rp_paused ? RPS_PAUSED : 0)
             | (g_rp_target != RP_NONE ? RPS_SEEKING : 0)
             | (g_tick >= g_total ? RPS_END : 0)
             | (g_bad_ticks ? RPS_DIFFERS : 0);
    return g_total ? (float)t / (float)g_total : 0.0f;
}
