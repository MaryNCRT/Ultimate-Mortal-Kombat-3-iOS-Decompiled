/*
 * rollback.c -- rollback netcode for ONLINE fights. A port feature, not in
 * the binary.
 *
 * The binary's network fight is host-authoritative: the host steps the
 * engine with both sticks and streams sprite lists, the guest only draws
 * them and feels the whole round trip on every press. Here both machines
 * run the fight -- the same frames from the same inputs -- and only the
 * inputs travel:
 *
 *   - Each frame, the local player's input (the stick and the seven buttons
 *     ReadControls produces, 11 bits) is taken for the frame g_delay ahead
 *     and sent with every input the other side has not acknowledged, put on
 *     the wire at once (mpnet_flush), not at the next frame's poll.
 *   - g_delay follows the round trip (adapt_delay): each side's delay is its
 *     own -- the other only receives inputs by frame -- so it moves one frame
 *     at a time, the inputs it skips filled, the one it overlaps dropped.
 *   - A frame whose remote input has not arrived uses the last one that did.
 *     When the real one arrives and differs, the state goes back to the
 *     snapshot taken before that frame and the frames since run again,
 *     silent and undrawn, with what is now known.
 *   - A side more than RB_WINDOW frames past the last input it has, or ahead
 *     of the other on average, holds: it draws the next frame and puts the
 *     state back.
 *   - The fight ends (CurrentTask leaves 6) only on a frame both inputs of
 *     which are real.
 *   - Once a second each side sends a hash of a frame both have confirmed
 *     (rb_game_hash: sprites, health, positions); a mismatch is a desync and
 *     goes to the log.
 *
 * The snapshots are savestate.c's st_rb_*: the game's sections minus what
 * belongs to the machine (UMK3_NO_ROLLBACK files) and the 60 MB of loaded
 * characters, plus the arena pages written during the fight. GameCode.c does
 * the game's side: the frame wrapper, the inputs, the hash. Windows only, as
 * the save states are.
 *
 * Not a state file: what it keeps must survive the loads it does.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <dbghelp.h>
#endif

#include "savestate.h"
#include "mp_net.h"
#include "rollback.h"
#include "mods.h"
#include "online_rules.h"

/* debug_menu.c: the host's STAGE SELECT, as the guest sees it */
int  stage_menu_watch_frame(void);
void stage_menu_watch_end(void);

/* the game's side */
long isParent(void);                    /* limeMPGameSpecific.c */
extern int CurrentTask;                 /* GameCode.c: 6 is the fight */
extern int Settings[10];                /* [4] the buttons, 4, 5 or 6 */
extern int32_t  rand_seed;              /* fight_runtime.c: limeRand's */
extern uint32_t umk3_rand_seed;         /* fight_runtime.c: the engine's */
uint16_t rb_game_local_input(void);     /* GameCode.c */
uint32_t rb_game_hash(void);            /* GameCode.c */
void     rb_game_buttons(int p1, int p2);   /* GameCode.c */
void     disableHeartbeat(void);        /* limeMPGameSpecific.c */
long     isMPConnected(void);           /* limeMPGameSpecific.c */
extern int mpLevelList[14];             /* gamecode_globals.c: the binary's online stages */
extern float FrameCompensation;         /* GameCode.c: the engine's 55.93 Hz step */
extern umk3_long  SpeedNormal;               /* GameCode.c: 0 slow motion */
void     disconnected(void);            /* limeMPGameSpecific.c */
void     endMP(void);                   /* limeMPGameSpecific.c */

#define RB_VERSION      3               /* 2: the hello carries the rules and a draw's number;
                                         * 3: and a lobby room's stage */
#define RB_DELAY        2               /* frames at the start; UMK3_RB_DELAY 0..6 fixes it */
#define RB_DELAY_MAX    6
#define RB_DEPTH        3               /* the rollback adapt_delay aims at */
#define RB_WINDOW       12              /* frames ahead of the last input */
#define RB_SLOTS        (RB_WINDOW + 3) /* snapshots, the last for holds */
#define RB_RING         64              /* inputs and hashes, by frame */
#define RB_SEND_MAX     32              /* inputs in one packet */
#define RB_HASH_EVERY   60
#define RB_REPORT       600
#define RB_LINGER       180             /* frames of sending after a fight */
#define RB_FIGHT_TASK   6
#define RB_SYNC_GAP     30              /* frames between two holds for being ahead */
#define RB_STALL_MS     15000.0         /* no new input from the other side that long:
                                         * the fight is over for it (it left, or the
                                         * two games parted): leave it here too */

#define NONE            0xffffffffu

static int g_on, g_started, g_me, g_delay = RB_DELAY, g_delay_fixed;
static int g_quiet, g_nodraw;
static uint32_t g_frame;                /* the next frame to run */
static uint16_t g_in[2][RB_RING];       /* inputs by player and frame */
static uint16_t g_used[RB_RING];        /* the remote input a frame ran with */
static uint32_t g_local_upto;           /* local inputs known below this */
static uint32_t g_remote_upto;          /* remote inputs received below this */
static uint32_t g_acked;                /* the other side has ours below this */
static uint32_t g_rollback = NONE;      /* the first frame that ran on a wrong guess */
static int32_t  g_remote_frame, g_remote_adv;
static uint32_t g_last_hold;
static int      g_waiting;              /* the fight ended on a guess: hold */
static uint16_t g_cur[2];               /* the frame being run */
static int      g_linger;
static uint32_t g_seed;                 /* the fight's, from the fighters and level */
static double   g_ahead;                /* frames ahead of the other side, averaged */
static double   g_last_input_ms;        /* when the other side's inputs last moved on */

/* the round trip: when each local input was first sent, until acknowledged */
static double   g_sent_ms[RB_RING];
static double   g_rtt;                  /* ms, smoothed; 0 before a sample */

/* hello: the other machine's buttons, rules and number for the draw;
 * setup: its view of the fight's fighters and level */
static int      g_hello, g_their_buttons, g_their_version, g_their_rollback;
static uint32_t g_my_draw, g_their_draw;
static int32_t  g_their_room = -1;      /* the other's room stage, -1 none */
static char     g_my_rules[ONLINE_RULES_TEXT], g_their_rules[ONLINE_RULES_TEXT];
static int      g_setup_in;
static int32_t  g_setup[3], g_their_setup[3];

/* the gate (rb_online_gate): 0 nothing said yet, 1 waiting for the other's
 * hello, 2 the guest waiting for the host's level, 3 through, 4 refusing */
static int      g_gate, g_gate_wait, g_refusal;
static int      g_swap;                 /* the draw put the host on the right */

/* desync check */
static uint32_t g_hash[RB_RING], g_hash_frame[RB_RING];
static uint32_t g_next_hash = RB_HASH_EVERY;
static uint32_t g_their_hash_frame = NONE, g_their_hash;
static uint32_t g_my_hash_frame = NONE, g_my_hash;
static uint32_t g_desyncs;

/* numbers for the log */
static uint32_t g_rollbacks, g_resims, g_deepest, g_holds;
static double   g_save_ms, g_save_max, g_resim_max;
static uint32_t g_saves;

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
    return 0.0;
#endif
}

static void put32(unsigned char *p, uint32_t v)
{
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16);
    p[3] = (unsigned char)(v >> 24);
}

static uint32_t get32(const unsigned char *p)
{
    return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16
         | (uint32_t)p[3] << 24;
}

static int slot_of(uint32_t frame)
{
    return (int)(frame % (RB_SLOTS - 1));
}

static int save_slot(int slot)
{
    double t0 = now_ms(), t;
    int r = st_rb_save(slot);

    t = now_ms() - t0;
    g_saves++;
    g_save_ms += t;
    if (t > g_save_max)
        g_save_max = t;
    return r;
}

/* The remote input frame `f` runs with: the real one, or the last real one. */
static uint16_t remote_input(uint32_t f)
{
    int them = g_me ^ 1;

    if (f < g_remote_upto)
        return g_in[them][f % RB_RING];
    return g_remote_upto ? g_in[them][(g_remote_upto - 1) % RB_RING] : 0;
}

static void send_inputs(void)
{
    unsigned char b[20 + 2 * RB_SEND_MAX];
    uint32_t first = g_acked, n, i;

    /* From the first the other side does not have, at most RB_SEND_MAX:
     * the other side takes inputs only in order (got_inputs), so sending the
     * NEWEST 32 once more than 32 were unacknowledged -- a burst of lost
     * packets, a stall on one side -- left a gap nothing ever filled, and
     * both games held for good (an online softlock). */
    if (first > g_local_upto)
        first = g_local_upto;
    n = g_local_upto - first;
    if (n > RB_SEND_MAX)
        n = RB_SEND_MAX;
    put32(b, first);
    put32(b + 4, n);
    put32(b + 8, g_frame);
    put32(b + 12, (uint32_t)((int32_t)g_frame - g_remote_frame));
    put32(b + 16, g_remote_upto);
    for (i = 0; i < n; i++) {
        uint16_t v = g_in[g_me][(first + i) % RB_RING];
        b[20 + 2 * i] = (unsigned char)v;
        b[21 + 2 * i] = (unsigned char)(v >> 8);
    }
    mpnet_send(RB_PKT_INPUT, b, (int)(20 + 2 * n), 0);
    mpnet_flush();
}

/* The hello: version, buttons, the netcode (1 rollback, 0 classic), the
 * number for the draw, the lobby room's stage (a level, -1 none: the
 * fight's own way), then the rules block as text (empty: no rules, a fight
 * not from a lobby). The room's stage rides here too: the one called to
 * host could be connected before its lobby told it the call, and opened
 * STAGE SELECT for a stage the room had chosen (Mary). */
#define RB_HELLO_LEN    (20 + ONLINE_RULES_TEXT)

static int classic(void)
{
    const char *e = getenv("UMK3_NETCODE");
    return e && !strcmp(e, "classic");
}

static void send_hello(void)
{
    unsigned char b[RB_HELLO_LEN];
    const online_rules *r = rules_online();
    const char *test = getenv("UMK3_HELLO_RULES");  /* a test aid: other rules */
    uint32_t t = (uint32_t)(now_ms() * 1000.0);

    memset(g_my_rules, 0, sizeof g_my_rules);
    if (test && *test)
        snprintf(g_my_rules, sizeof g_my_rules, "%s", test);
    else if (r)
        rules_encode(r, g_my_rules, sizeof g_my_rules);
    /* the number: the clock's microseconds, this process, the stack */
    g_my_draw = t * 0x9e3779b1u ^ (uint32_t)rand() << 16 ^ (uint32_t)(uintptr_t)&t;
#ifdef _WIN32
    g_my_draw ^= (uint32_t)GetCurrentProcessId() * 0x85ebca6bu;
#endif
    memset(b, 0, sizeof b);
    put32(b, RB_VERSION);
    put32(b + 4, (uint32_t)Settings[4]);
    put32(b + 8, classic() ? 0u : 1u);
    put32(b + 12, g_my_draw);
    put32(b + 16, (uint32_t)rules_room_level());
    memcpy(b + 20, g_my_rules, ONLINE_RULES_TEXT);
    mpnet_send(RB_PKT_HELLO, b, RB_HELLO_LEN, 1);
    mpnet_flush();
    printf("rollback: hello sent, rules \"%s\"\n", g_my_rules);
    fflush(stdout);
}

/* The setup, once each side knows its fighters and level: the guest takes
 * the host's level from it (HOST ADVANTAGE ON), and start() compares. */
static void send_setup(void)
{
    unsigned char b[12];

    put32(b, (uint32_t)g_setup[0]);
    put32(b + 4, (uint32_t)g_setup[1]);
    put32(b + 8, (uint32_t)g_setup[2]);
    mpnet_send(RB_PKT_SETUP, b, 12, 1);
    mpnet_flush();
}

static void report(void)
{
    printf("rollback: frame %u -- %u rollbacks, %u frames run again, deepest %u "
           "(longest %.1f ms); %u holds; snapshot %.2f ms avg, %.2f max (%.1f MB); "
           "remote %d frames behind; ping %.0f ms, delay %d; desyncs %u\n",
           g_frame, g_rollbacks, g_resims, g_deepest, g_resim_max, g_holds,
           g_saves ? g_save_ms / g_saves : 0.0, g_save_max,
           st_rb_size() / 1048576.0, (int)g_frame - g_remote_frame, g_rtt,
           g_delay, g_desyncs);
    fflush(stdout);
    g_save_ms = g_save_max = g_resim_max = 0;
    g_saves = 0;
}

/* -------------------------------------------------------------- desyncs */

static void compare_hashes(void)
{
    if (g_my_hash_frame == NONE || g_my_hash_frame != g_their_hash_frame)
        return;
    if (g_my_hash != g_their_hash) {
        g_desyncs++;
        printf("rollback: DESYNC at frame %u -- this side %08x, the other %08x\n",
               g_my_hash_frame, g_my_hash, g_their_hash);
        fflush(stdout);
    }
    g_my_hash_frame = NONE;
}

/* The hash of frame g_next_hash once both inputs of every frame up to it
 * are real and no rollback before it is pending: it will not change. */
static void send_hash(void)
{
    unsigned char b[8];
    uint32_t f = g_next_hash;

    if (f >= g_remote_upto || f >= g_frame || g_rollback <= f
        || g_hash_frame[f % RB_RING] != f)
        return;
    put32(b, f);
    put32(b + 4, g_hash[f % RB_RING]);
    mpnet_send(RB_PKT_HASH, b, 8, 1);
    g_my_hash_frame = f;
    g_my_hash = g_hash[f % RB_RING];
    g_next_hash += RB_HASH_EVERY;
    compare_hashes();
}

/* ------------------------------------------------------------ the frames */

/* One run of the game's frame `f`. Returns 1 if the fight carried on. */
/* The sounds each frame played (rb_sound_ok). Run again after a rollback, a
 * frame plays the ones that are new -- the other side's attack voice, when
 * its press came late, was only ever in frames run again, and was never
 * heard -- and not twice the ones it played. A hold's frame plays nothing:
 * it runs again for real. */
#define RB_SND 12
static long g_snd[RB_RING][RB_SND];
static uint32_t g_snd_frame[RB_RING];
static int g_snd_n[RB_RING], g_in_run, g_holding;
static uint32_t g_run_frame;

int rb_sound_ok(umk3_long h)
{
    uint32_t s = g_run_frame % RB_RING;
    int i;

    if (!g_in_run)
        return !g_quiet;
    if (g_holding)
        return 0;
    if (g_snd_frame[s] != g_run_frame) {
        g_snd_frame[s] = g_run_frame;
        g_snd_n[s] = 0;
    }
    for (i = 0; i < g_snd_n[s]; i++)
        if (g_snd[s][i] == h)
            return !g_quiet;            /* played the first time */
    if (g_snd_n[s] < RB_SND)
        g_snd[s][g_snd_n[s]++] = h;
    return 1;
}

static int run(void (*frame)(void), uint32_t f)
{
    g_cur[g_me] = g_in[g_me][f % RB_RING];
    g_cur[g_me ^ 1] = remote_input(f);
    g_used[f % RB_RING] = g_cur[g_me ^ 1];
    g_run_frame = f;
    g_in_run = 1;
    frame();
    g_in_run = 0;
    g_hash[f % RB_RING] = rb_game_hash();
    g_hash_frame[f % RB_RING] = f;
    return CurrentTask == RB_FIGHT_TASK;
}

/* ------------------------------------------------------------ sync test
 *
 * UMK3_RB_SYNCTEST=1 (a test aid, Windows): every frame of an online fight
 * runs twice from the same snapshot -- first as a rollback runs it again
 * (silent, undrawn), then for real -- and every 4-byte word of the game's
 * memory the two runs left different goes to the log by name (dbghelp and
 * the exe's .pdb). What a run again leaves different from the first run is
 * what parts two machines once one of them rolls back: state the snapshot
 * does not hold, or logic that hangs on drawing or sound. */
static int g_synctest = -1;
static unsigned g_st_frames, g_st_bad;
#define ST_NAMES 400
static struct { char name[64]; unsigned words, frames, last; int sect; uint64_t lo, hi; } g_st_name[ST_NAMES];
static int g_st_n;

#ifdef _WIN32
typedef BOOL (WINAPI *sym_init_fn)(HANDLE, PCSTR, BOOL);
typedef BOOL (WINAPI *sym_from_fn)(HANDLE, DWORD64, PDWORD64, PSYMBOL_INFO);
static sym_from_fn g_sym_from;

static uint64_t g_sym_off;

static void sym_name(const void *at, char *out, int n)
{
    static int tried;
    union { SYMBOL_INFO s; char b[sizeof(SYMBOL_INFO) + 256]; } u;
    DWORD64 off = 0;

    if (!tried) {
        HMODULE h = LoadLibraryA("dbghelp.dll");
        sym_init_fn init = h ? (sym_init_fn)(void *)GetProcAddress(h, "SymInitialize") : NULL;
        tried = 1;
        if (init && init(GetCurrentProcess(), NULL, TRUE))
            g_sym_from = (sym_from_fn)(void *)GetProcAddress(h, "SymFromAddr");
    }
    {
        /* the last symbol found, for the words that follow inside it */
        static uintptr_t c_lo, c_hi;
        static char c_name[64];
        uintptr_t a = (uintptr_t)at;

        if (c_hi && a >= c_lo && a < c_hi) {
            snprintf(out, (size_t)n, "%s", c_name);
            g_sym_off = a - c_lo;
            return;
        }
        memset(&u, 0, sizeof u);
        u.s.SizeOfStruct = sizeof(SYMBOL_INFO);
        u.s.MaxNameLen = 255;
        g_sym_off = 0;
        if (g_sym_from && g_sym_from(GetCurrentProcess(), (DWORD64)a, &off, &u.s)) {
            snprintf(out, (size_t)n, "%s", u.s.Name);
            snprintf(c_name, sizeof c_name, "%s", u.s.Name);
            g_sym_off = off;
            c_lo = a - (uintptr_t)off;
            c_hi = u.s.Size ? c_lo + u.s.Size : a + 4;
        } else
            snprintf(out, (size_t)n, "?");
    }
}
#else
static uint64_t g_sym_off;
static void sym_name(const void *at, char *out, int n) { (void)at; g_sym_off = 0; snprintf(out, (size_t)n, "?"); }
#endif

static int st_word(const void *at, uint32_t now, uint32_t was, int sect, void *ctx)
{
    char nm[64];
    int i;

    (void)ctx;
    {
        /* the loaded fighters' block, which the drawing writes into every
         * frame (its own skinning scratch) and the snapshot leaves out */
        extern char PreloadedCharacters[41148][0x5f4];
        if ((const char *)at >= &PreloadedCharacters[0][0]
            && (const char *)at < &PreloadedCharacters[0][0] + sizeof PreloadedCharacters)
            return 1;
    }
    if (sect == 4)
        snprintf(nm, sizeof nm, "arena+0x%x", (unsigned)(((uintptr_t)at >> 12) << 12) & 0xfffffff);
    else
        sym_name(at, nm, sizeof nm);
    for (i = 0; i < g_st_n; i++)
        if (!strcmp(g_st_name[i].name, nm))
            break;
    if (i == g_st_n) {
        if (g_st_n == ST_NAMES)
            return 1;
        snprintf(g_st_name[i].name, sizeof g_st_name[i].name, "%s", nm);
        g_st_name[i].sect = sect;
        g_st_name[i].lo = g_st_name[i].hi = g_sym_off;
        g_st_n++;
        printf("rollback sync test: frame %u, NEW %s (%s) %p: run again %08x, real %08x\n",
               g_frame, nm, sect == 0 ? ".umkstd" : sect == 1 ? ".umkstb" : sect == 2 ? ".umkntd"
               : sect == 3 ? ".umkntb" : "arena", at, was, now);
    }
    g_st_name[i].words++;
    if (g_sym_off < g_st_name[i].lo) g_st_name[i].lo = g_sym_off;
    if (g_sym_off > g_st_name[i].hi) g_st_name[i].hi = g_sym_off;
    if (g_st_name[i].last != g_frame + 1) {
        g_st_name[i].last = g_frame + 1;
        g_st_name[i].frames++;
    }
    return 1;
}

static void synctest_report(void)
{
    int i;

    if (g_synctest <= 0 || !g_st_frames)
        return;
    printf("rollback sync test: %u frames, %u with differences, %d names\n", g_st_frames,
           g_st_bad, g_st_n);
    for (i = 0; i < g_st_n; i++)
        printf("rollback sync test:   %-40s %-7s %u frames, %u words, +0x%llx..+0x%llx\n",
               g_st_name[i].name,
               g_st_name[i].sect == 4 ? "arena" : g_st_name[i].sect >= 2 ? "machine" : "game",
               g_st_name[i].frames, g_st_name[i].words,
               (unsigned long long)g_st_name[i].lo, (unsigned long long)g_st_name[i].hi);
    fflush(stdout);
}

/* UMK3_RB_DUMP=<frame> (a test aid, two games on one PC): both machines
 * stop at that frame until the other's input for it is in, run it, and write
 * the game's sections (.umkstd, .umkstb) to logs/rbdump-<player>.bin; the
 * one that writes second compares the two, word by word, and names in the
 * log what differs -- the same exe on one PC puts both at the same
 * addresses. */
static long g_dump_at = -2;

static void dump_compare(int me)
{
    uint8_t *p[2];
    uint32_t n[2], i, at = 0, a, b, count = 0;
    char path[64];
    FILE *f;
    int r, shown = 0;
    uint8_t *other;
    long len;

    snprintf(path, sizeof path, "logs/rbdump-%d.bin", me ^ 1);
    f = fopen(path, "rb");
    if (!f)
        return;
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);
    other = (uint8_t *)malloc((size_t)len);
    if (!other || fread(other, 1, (size_t)len, f) != (size_t)len) {
        fclose(f);
        free(other);
        return;
    }
    fclose(f);
    if (st_sections(p, n, 2) != 2 || (long)(n[0] + n[1]) > len) {
        printf("rollback dump: the other's dump is another size\n");
        free(other);
        return;
    }
    {
        /* the arena: the words that differ, by 4 KB page, the first ones
         * shown with where GameObjects (the fighters' sprite list) points */
        uint8_t *ap;
        uint32_t an, on, j, pages = 0, last = 0xffffffffu, shownw = 0, diffw = 0;
        uint64_t obase;
        extern void **GameObjects;
        const uint8_t *oa = other + n[0] + n[1];

        if ((long)(n[0] + n[1] + 12) <= len && st_arena(&ap, &an)) {
            memcpy(&obase, oa, 8);
            memcpy(&on, oa + 8, 4);
            oa += 12;
            printf("rollback dump: arena here %p, %u bytes; there %llx, %u bytes; "
                   "GameObjects at +0x%llx\n", (void *)ap, an, (unsigned long long)obase, on,
                   (unsigned long long)((uint8_t *)(void *)GameObjects - ap));
            for (j = 0; j + 4 <= an && j + 4 <= on && n[0] + n[1] + 12 + j + 4 <= (uint32_t)len;
                 j += 4) {
                uint32_t x, y;
                memcpy(&x, ap + j, 4);
                memcpy(&y, oa + j, 4);
                if (x != y) {
                    diffw++;
                    if (j / 4096 != last) {
                        last = j / 4096;
                        pages++;
                    }
                    if (shownw++ < 60)
                        printf("rollback dump: arena +0x%08x: here %08x, there %08x\n", j, x, y);
                }
            }
            printf("rollback dump: arena, %u words differ in %u pages\n", diffw, pages);
        }
    }
    g_st_n = 0;
    for (r = 0; r < 2; r++) {
        for (i = 0; i + 4 <= n[r]; i += 4) {
            memcpy(&a, p[r] + i, 4);
            memcpy(&b, other + at + i, 4);
            if (a != b) {
                char nm[64];
                int k;

                count++;
                sym_name(p[r] + i, nm, sizeof nm);
                for (k = 0; k < g_st_n; k++)
                    if (!strcmp(g_st_name[k].name, nm))
                        break;
                if (k == g_st_n && g_st_n < ST_NAMES) {
                    snprintf(g_st_name[k].name, sizeof g_st_name[k].name, "%s", nm);
                    g_st_name[k].words = 0;
                    g_st_name[k].lo = g_st_name[k].hi = g_sym_off;
                    g_st_n++;
                    if (shown++ < 80)
                        printf("rollback dump: frame %ld, %s+0x%llx: here %08x, there %08x\n",
                               g_dump_at, nm, (unsigned long long)g_sym_off, a, b);
                }
                if (k < g_st_n) {
                    g_st_name[k].words++;
                    if (g_sym_off > g_st_name[k].hi) g_st_name[k].hi = g_sym_off;
                }
            }
        }
        at += n[r];
    }
    printf("rollback dump: frame %ld, %u words differ in %d names\n", g_dump_at, count, g_st_n);
    for (r = 0; r < g_st_n; r++)
        printf("rollback dump:   %-40s %u words, +0x%llx..+0x%llx\n", g_st_name[r].name,
               g_st_name[r].words, (unsigned long long)g_st_name[r].lo,
               (unsigned long long)g_st_name[r].hi);
    fflush(stdout);
    g_st_n = 0;
    free(other);
}

static void dump_write(void)
{
    uint8_t *p[2];
    uint32_t n[2];
    char path[64];
    FILE *f;

    if (st_sections(p, n, 2) != 2)
        return;
    {
        int rb_game_hash_words(uint32_t *w);   /* GameCode.c */
        extern void **GameObjects;
        uint32_t w[32];
        int i;

        rb_game_hash_words(w);
        printf("rollback dump: frame %ld hash %08x, GameObjects %p, words", g_dump_at,
               g_hash[g_frame % RB_RING], (void *)GameObjects);
        for (i = 0; i < 32; i++)
            printf(" %08x", w[i]);
        printf("\n");
    }
    snprintf(path, sizeof path, "logs/rbdump-%d.bin", g_me);
    f = fopen(path, "wb");
    if (!f)
        return;
    fwrite(p[0], 1, n[0], f);
    fwrite(p[1], 1, n[1], f);
    {
        /* then the arena: its address, its size, its bytes */
        uint8_t *ap;
        uint32_t an;
        uint64_t base;

        if (st_arena(&ap, &an)) {
            base = (uint64_t)(uintptr_t)ap;
            fwrite(&base, 8, 1, f);
            fwrite(&an, 4, 1, f);
            fwrite(ap, 1, an, f);
        }
    }
    fclose(f);
    printf("rollback dump: frame %ld written (%u bytes)\n", g_dump_at, n[0] + n[1]);
    fflush(stdout);
    dump_compare(g_me);
}

/* Back to the frame that ran on a wrong guess, and forward again. */
static void roll_back(void (*frame)(void))
{
    uint32_t f = g_rollback, depth = g_frame - g_rollback;
    double t0 = now_ms();

    g_rollback = NONE;
    if (depth >= RB_SLOTS - 1 || st_rb_load(slot_of(f)) != 0) {
        printf("rollback: cannot go back %u frames, from %u\n", depth, g_frame);
        return;
    }
    g_rollbacks++;
    g_resims += depth;
    if (depth > g_deepest)
        g_deepest = depth;

    g_quiet = g_nodraw = 1;
    for (; f < g_frame; f++) {
        if (f != g_frame - depth)
            save_slot(slot_of(f));
        if (!run(frame, f)) {
            if (f < g_remote_upto) {
                g_frame = f + 1;        /* it really ended there */
            } else {
                st_rb_load(slot_of(f));
                g_frame = f;
                g_waiting = 1;
            }
            break;
        }
    }
    g_quiet = g_nodraw = 0;
    if (now_ms() - t0 > g_resim_max)
        g_resim_max = now_ms() - t0;
}

/* Once a second, a frame of delay toward the one that keeps rollbacks near
 * RB_DEPTH frames: the other side's input for a frame comes half the round
 * trip after it was taken, g_delay frames before that frame. Down only once
 * the round trip is half a frame clear of the step, so it does not flap. */
static void adapt_delay(void)
{
    double half = g_rtt / 2.0 / (1000.0 / 60.0);
    int up = (int)(half + 0.5) - RB_DEPTH, down = (int)(half + 1.0) - RB_DEPTH;

    if (g_delay_fixed || g_rtt <= 0 || g_frame % 60 != 0)
        return;
    if (up > g_delay && g_delay < RB_DELAY_MAX)
        g_delay++;
    else if (down < g_delay && g_delay > RB_DELAY)
        g_delay--;
}

/* This frame's input, for g_delay frames on. After the delay grew, the frames
 * between take it too; after it shrank, the frame already has its input and
 * this one is dropped (a press lasts longer than a frame). */
static void take_input(void)
{
    uint32_t want = g_frame + (uint32_t)g_delay;
    uint16_t v = rb_game_local_input();

    for (; g_local_upto <= want; g_local_upto++) {
        g_in[g_me][g_local_upto % RB_RING] = v;
        g_sent_ms[g_local_upto % RB_RING] = now_ms();
    }
}

/* Draw the next frame without keeping it. */
static void hold(void (*frame)(void))
{
    g_holds++;
    save_slot(RB_SLOTS - 1);
    g_quiet = g_holding = 1;
    run(frame, g_frame);
    g_quiet = g_holding = 0;
    st_rb_load(RB_SLOTS - 1);
}

/* ----------------------------------------------------------- spectators
 *
 * ONLINE stage 4c (docs/research/ONLINE-PHASE-PLAN.md): the others in a
 * lobby's room watch a fight live. The machine that hosts the fight (the
 * call's A) keeps every CONFIRMED frame -- both players' real inputs, a
 * frame below what each side has of the other's -- and sends them to the
 * members who ask. A spectator runs the same fight from them, as an online
 * fight (GameMode 1, the same seed, the same buttons and rules): never a
 * guess, so never a rollback; a frame whose inputs have not come is held
 * (drawn, not kept). It leaves to the room at the fight's last frame, or
 * when its player presses B / Esc (game_main.c).
 *
 * Packets (mp_net.c K_SPEC, over EOS, outside the fight's session), each a
 * kind byte then words:
 *   SP_ASK    spectator -> host   "let me watch"
 *   SP_SETUP  host -> spectator   fight id, c1, c2, level, p1 / p2 buttons,
 *                                 the rules text
 *   SP_NONE   host -> spectator   no fight to watch
 *   SP_IN     host -> spectator   fight id, first frame, count, the final
 *                                 frame count (NONE while it goes on), then
 *                                 the pairs (p1 | p2 << 16)
 *   SP_ACK    spectator -> host   fight id, frames received below
 *   SP_BYE    spectator -> host   stopped watching
 * Unreliable both ways: the host sends from what each watcher acknowledged,
 * the spectator asks again until a setup comes. */
enum { SP_ASK = 1, SP_SETUP, SP_NONE, SP_IN, SP_ACK, SP_BYE };
#define SP_FRAMES    (1u << 17)         /* 39 minutes at 55.93 Hz */
#define SP_WATCHERS  6
#define SP_PER_PKT   64
#define SP_PKTS      4                  /* a frame's burst to one watcher */
#define SP_RESEND_MS 120.0
#define SP_LINGER_MS 15000.0            /* after the fight: the last frames out */
#define SP_ASK_MS    500.0
#define SP_GIVEUP_MS 60000.0           /* asking: the fight may be at its select screen */
#define SP_CATCH_UP  90                 /* frames behind: run several a frame */

/* the host's side */
static uint32_t *g_sp_hist;             /* confirmed frames, by frame */
static uint32_t  g_sp_upto;             /* confirmed below this */
static uint32_t  g_sp_final = NONE;     /* the fight's frame count, once over */
static uint32_t  g_sp_id;               /* the fight's id: a watcher's check */
static double    g_sp_over_ms;
static int       g_sp_live;             /* a fight to watch: on, or lingering */
static int32_t   g_sp_btn[2];
static struct sp_watcher {
    char     who[40];
    uint32_t acked, sent;
    double   sent_ms;
} g_sp_w[SP_WATCHERS];

/* the spectator's side */
static int       g_watch;               /* 1 asking, 2 set up (the fight starts), 3 watching */
static char      g_watch_who[40];       /* the host */
static double    g_watch_ask_ms, g_watch_since_ms, g_watch_heard_ms;
static uint32_t  g_watch_id, g_watch_upto, g_watch_final = NONE;
static int32_t   g_watch_setup[5];      /* c1, c2, level, buttons 1 and 2 */
static char      g_watch_rules[ONLINE_RULES_TEXT];
static uint32_t *g_watch_hist;
static int       g_watch_none;          /* no fight came to watch */
static int       g_watch_none_said;

static void sp_put(unsigned char *p, uint32_t v) { put32(p, v); }

static void sp_send_setup(const char *who)
{
    unsigned char b[1 + 24 + ONLINE_RULES_TEXT];

    memset(b, 0, sizeof b);
    b[0] = SP_SETUP;
    sp_put(b + 1, g_sp_id);
    sp_put(b + 5, (uint32_t)g_setup[0]);
    sp_put(b + 9, (uint32_t)g_setup[1]);
    sp_put(b + 13, (uint32_t)g_setup[2]);
    sp_put(b + 17, (uint32_t)g_sp_btn[0]);
    sp_put(b + 21, (uint32_t)g_sp_btn[1]);
    memcpy(b + 25, g_my_rules, ONLINE_RULES_TEXT);
    mpnet_spec_send(who, b, (int)sizeof b);
}

/* The host, every frame of the fight and while it lingers: the frames
 * confirmed since, kept, and sent to each watcher from what it has. */
static void sp_host_tick(void)
{
    double t = now_ms();
    int i, k;

    if (g_on && g_started && g_sp_hist) {
        uint32_t top = g_local_upto < g_remote_upto ? g_local_upto : g_remote_upto;
        if (top > g_frame)
            top = g_frame;              /* run, not only known */
        for (; g_sp_upto < top && g_sp_upto < SP_FRAMES; g_sp_upto++)
            g_sp_hist[g_sp_upto] = (uint32_t)g_in[0][g_sp_upto % RB_RING]
                                 | (uint32_t)g_in[1][g_sp_upto % RB_RING] << 16;
    }
    if (!g_sp_live)
        return;
    if (g_sp_final != NONE && t - g_sp_over_ms > SP_LINGER_MS) {
        g_sp_live = 0;
        memset(g_sp_w, 0, sizeof g_sp_w);
        return;
    }
    for (i = 0; i < SP_WATCHERS; i++) {
        struct sp_watcher *w = &g_sp_w[i];
        if (!w->who[0])
            continue;
        if (w->sent > g_sp_upto || t - w->sent_ms > SP_RESEND_MS)
            w->sent = w->acked;         /* nothing heard back: from what it has */
        for (k = 0; k < SP_PKTS && (w->sent < g_sp_upto || (g_sp_final != NONE && k == 0)); k++) {
            unsigned char b[1 + 16 + 4 * SP_PER_PKT];
            uint32_t n = g_sp_upto - w->sent, j;
            if (n > SP_PER_PKT)
                n = SP_PER_PKT;
            b[0] = SP_IN;
            sp_put(b + 1, g_sp_id);
            sp_put(b + 5, w->sent);
            sp_put(b + 9, n);
            sp_put(b + 13, g_sp_final);
            for (j = 0; j < n; j++)
                sp_put(b + 17 + 4 * j, g_sp_hist[w->sent + j]);
            mpnet_spec_send(w->who, b, (int)(17 + 4 * n));
            w->sent += n;
            w->sent_ms = t;
            if (!n)
                break;
        }
    }
    mpnet_flush();
}

/* The host's fight begins (start()): a new id, the history from frame 0;
 * those who watched the last fight are told this one (Mary: watch the
 * room's fights one after another). */
static void sp_host_begin(int b1, int b2)
{
    int i;

    if (!g_sp_hist)
        g_sp_hist = (uint32_t *)calloc(SP_FRAMES, sizeof *g_sp_hist);
    g_sp_upto = 0;
    g_sp_final = NONE;
    g_sp_id = g_seed ^ (uint32_t)(now_ms() * 1000.0);
    g_sp_btn[0] = b1;
    g_sp_btn[1] = b2;
    g_sp_live = 1;
    for (i = 0; i < SP_WATCHERS; i++)
        if (g_sp_w[i].who[0]) {
            g_sp_w[i].acked = g_sp_w[i].sent = 0;
            sp_send_setup(g_sp_w[i].who);
        }
}

/* Task_GameDestroy (rb_online_end): the count of frames the fight ran. */
static void sp_host_end(void)
{
    if (!g_sp_live || g_sp_final != NONE)
        return;
    sp_host_tick();
    g_sp_final = g_sp_upto;
    g_sp_over_ms = now_ms();
    printf("spectators: the fight is over at frame %u\n", g_sp_final);
}

/* ---- the spectator ---- */

int rb_watching(void) { return g_watch >= 2; }
int rb_watch_state(void) { return g_watch_none ? -1 : g_watch; }

void rb_watch_begin(const char *host)
{
    if (!host || !host[0])
        return;
    snprintf(g_watch_who, sizeof g_watch_who, "%s", host);
    g_watch = 1;
    g_watch_none = 0;
    g_watch_ask_ms = 0;
    g_watch_since_ms = g_watch_heard_ms = now_ms();
    g_watch_upto = 0;
    g_watch_final = NONE;
    g_watch_none_said = 0;
    printf("spectators: asking %s to watch\n", host);
    fflush(stdout);
}

void rb_watch_stop(void)
{
    unsigned char b[1] = { SP_BYE };

    if (g_watch && g_watch_who[0])
        mpnet_spec_send(g_watch_who, b, 1);
    if (g_watch)
        printf("spectators: stopped watching at frame %u of %u\n", g_frame, g_watch_upto);
    g_watch = 0;
    g_watch_none = 0;
    g_watch_who[0] = 0;
}

/* game_main.c, every frame: the asking, and once set up the fight to start
 * (1 = start the fight now: *c1, *c2, *level filled). */
int rb_watch_poll(int *c1, int *c2, int *level)
{
    double t = now_ms();

    if (g_watch == 1) {
        if (t - g_watch_since_ms > SP_GIVEUP_MS) {
            printf("spectators: no answer from the host\n");
            g_watch = 0;
            g_watch_none = 1;
            return 0;
        }
        if (t - g_watch_ask_ms > SP_ASK_MS) {
            unsigned char b[1] = { SP_ASK };
            mpnet_spec_send(g_watch_who, b, 1);
            mpnet_flush();
            g_watch_ask_ms = t;
        }
        return 0;
    }
    if (g_watch == 2 && CurrentTask == 3) {
        *c1 = g_watch_setup[0];
        *c2 = g_watch_setup[1];
        *level = g_watch_setup[2];
        g_watch = 3;
        return 1;
    }
    if (g_watch == 3 && t - g_watch_heard_ms > RB_STALL_MS)
        return -1;                      /* the host is gone: away */
    return 0;
}

static void sp_ack(void)
{
    unsigned char b[9];
    b[0] = SP_ACK;
    sp_put(b + 1, g_watch_id);
    sp_put(b + 5, g_watch_upto);
    mpnet_spec_send(g_watch_who, b, 9);
}

/* mp_net.c: a K_SPEC packet from an EOS user */
void rb_spec_packet(const char *who, const unsigned char *b, int n)
{
    int i, free_i = -1;

    if (n < 1)
        return;
    switch (b[0]) {
    case SP_ASK:
        if (!g_sp_live) {
            unsigned char r[1] = { SP_NONE };
            mpnet_spec_send(who, r, 1);
            return;
        }
        for (i = 0; i < SP_WATCHERS; i++) {
            if (!strcmp(g_sp_w[i].who, who))
                break;
            if (!g_sp_w[i].who[0] && free_i < 0)
                free_i = i;
        }
        if (i == SP_WATCHERS) {
            if (free_i < 0)
                return;
            i = free_i;
            snprintf(g_sp_w[i].who, sizeof g_sp_w[i].who, "%s", who);
            g_sp_w[i].acked = g_sp_w[i].sent = 0;
            printf("spectators: %s watches (frame %u)\n", who, g_sp_upto);
        }
        sp_send_setup(who);
        mpnet_flush();
        return;
    case SP_ACK:
        if (n >= 9 && get32(b + 1) == g_sp_id)
            for (i = 0; i < SP_WATCHERS; i++)
                if (!strcmp(g_sp_w[i].who, who) && get32(b + 5) > g_sp_w[i].acked)
                    g_sp_w[i].acked = get32(b + 5);
        return;
    case SP_BYE:
        for (i = 0; i < SP_WATCHERS; i++)
            if (!strcmp(g_sp_w[i].who, who)) {
                printf("spectators: %s stopped watching\n", who);
                memset(&g_sp_w[i], 0, sizeof g_sp_w[i]);
            }
        return;
    case SP_NONE:
        /* not on yet (the select screen, the loading): asked again until it
         * is, or until SP_GIVEUP_MS (Mary: watch at any moment) */
        if (g_watch == 1 && !strcmp(who, g_watch_who) && !g_watch_none_said) {
            printf("spectators: the host's fight is not on yet\n");
            g_watch_none_said = 1;
        }
        return;
    case SP_SETUP:
        if (!g_watch || strcmp(who, g_watch_who) || n < 25 + ONLINE_RULES_TEXT)
            return;
        g_watch_heard_ms = now_ms();
        if (g_watch == 1 || (g_watch >= 2 && get32(b + 1) != g_watch_id && g_watch_final != NONE
                             && g_frame + 1 >= g_watch_final)) {
            g_watch_id = get32(b + 1);
            for (i = 0; i < 5; i++)
                g_watch_setup[i] = (int32_t)get32(b + 5 + 4 * i);
            memcpy(g_watch_rules, b + 25, ONLINE_RULES_TEXT);
            g_watch_rules[ONLINE_RULES_TEXT - 1] = 0;
            if (!g_watch_hist)
                g_watch_hist = (uint32_t *)calloc(SP_FRAMES, sizeof *g_watch_hist);
            g_watch_upto = 0;
            g_watch_final = NONE;
            g_watch = 2;
            printf("spectators: watching %d vs %d on %d, rules \"%s\"\n",
                   (int)g_watch_setup[0], (int)g_watch_setup[1], (int)g_watch_setup[2],
                   g_watch_rules);
            fflush(stdout);
        }
        return;
    case SP_IN: {
        uint32_t first, cnt, j;
        if (g_watch < 2 || strcmp(who, g_watch_who) || n < 17 || get32(b + 1) != g_watch_id
            || !g_watch_hist)
            return;
        g_watch_heard_ms = now_ms();
        first = get32(b + 5);
        cnt = get32(b + 9);
        if (get32(b + 13) != NONE)
            g_watch_final = get32(b + 13);
        if (cnt > SP_PER_PKT || n < (int)(17 + 4 * cnt))
            return;
        for (j = 0; j < cnt; j++)
            if (first + j == g_watch_upto && g_watch_upto < SP_FRAMES)
                g_watch_hist[g_watch_upto++] = get32(b + 17 + 4 * j);
        sp_ack();
        mpnet_flush();
        return;
    }
    }
}

/* The spectator's frame, in rb_frame's place: the next confirmed frame,
 * several when far behind, a hold when it has not come. 1 when the fight is
 * over for it (the host's last frame reached): game_main leaves. */
static int g_watch_over;
int rb_watch_over(void) { return g_watch_over; }

static void watch_run(void (*frame)(void), uint32_t f, int quiet)
{
    uint32_t v = g_watch_hist[f];

    g_cur[0] = (uint16_t)v;
    g_cur[1] = (uint16_t)(v >> 16);
    g_run_frame = f;
    g_in_run = 1;
    g_quiet = g_nodraw = g_holding = quiet;  /* catching up: unseen, unheard */
    frame();
    g_quiet = g_nodraw = g_holding = 0;
    g_in_run = 0;
}

static void watch_frame(void (*frame)(void))
{
    int k, runs = 1;

    if (g_watch_final != NONE && g_frame + 1 >= g_watch_final) {
        /* the fight's last frame is the host's leaving one: not run here
         * (it would go on into the online fight's end), the room instead */
        g_watch_over = 1;
        hold(frame);
        return;
    }
    if (g_frame >= g_watch_upto) {
        hold(frame);
        return;
    }
    if (g_watch_upto - g_frame > 600)
        runs = 32;                      /* far behind (joined late): catch up, unseen */
    else if (g_watch_upto - g_frame > SP_CATCH_UP)
        runs = 8;
    for (k = 0; k < runs && g_frame < g_watch_upto
                && !(g_watch_final != NONE && g_frame + 1 >= g_watch_final); k++) {
        watch_run(frame, g_frame, k + 1 < runs && g_frame + 1 < g_watch_upto);
        if (CurrentTask != RB_FIGHT_TASK)
            return;
        g_frame++;
    }
}

/* The loaded fighters: 60 MB a fight only reads, so no snapshot holds them
 * (gamecode_globals.c, 0x5f4 = PRELOAD_STRIDE). */
extern char PreloadedCharacters[41148][0x5f4];

static void start(void)
{
    int mine = Settings[4], theirs = g_their_buttons;

    g_started = 1;
    /* The first frame of the fight, the same moment on both machines: the
     * engine's step phase and its generators from the fight alone.
     * FrameCompensation (GameCode.c, 0.9322 of a step a tick) was never
     * reset between fights, so a second fight in one run began on each
     * machine with what its last fight had left: the engine stepped on
     * other ticks over there, every hash differed from the first second
     * and the fights parted -- one ended, the other waited (softlocks
     * after a fight or a round). The generators were seeded before the
     * loading, which runs a different number of frames on each side. */
    FrameCompensation = 0.0f;
    SpeedNormal = 1;
    umk3_rand_seed = g_seed;
    rand_seed = (int32_t)(g_seed >> 1);
    g_last_input_ms = now_ms();
    st_rb_skip(PreloadedCharacters, sizeof PreloadedCharacters);
    if (st_rb_begin(RB_SLOTS) != 0)
        printf("rollback: NO SNAPSHOTS -- save states are off in this build\n");
    if (g_watch >= 2)                   /* a spectator: the fighters' buttons */
        rb_game_buttons(g_watch_setup[3], g_watch_setup[4]);
    else {
        rb_game_buttons(g_me ? theirs : mine, g_me ? mine : theirs);
        sp_host_begin(g_me ? theirs : mine, g_me ? mine : theirs);
    }
    printf("rollback: the fight starts, this side is player %d, delay %d, "
           "buttons %d / %d\n", g_me + 1, g_delay,
           g_me ? theirs : mine, g_me ? mine : theirs);
    if (g_watch < 2 && memcmp(g_setup, g_their_setup, sizeof g_setup))
        printf("rollback: THE SETUPS DIFFER -- here %d %d %d, there %d %d %d\n",
               (int)g_setup[0], (int)g_setup[1], (int)g_setup[2],
               (int)g_their_setup[0], (int)g_their_setup[1],
               (int)g_their_setup[2]);
    fflush(stdout);
}

void rb_frame(void (*frame)(void))
{
    int32_t adv;

    if (!g_on) {
        frame();
        return;
    }
    if (!g_started)
        start();
    if (g_watch >= 2) {                 /* a spectator: see watch_frame */
        if (CurrentTask == RB_FIGHT_TASK)
            watch_frame(frame);
        return;
    }

    if (g_rollback != NONE && g_rollback < g_frame)
        roll_back(frame);
    if (CurrentTask != RB_FIGHT_TASK)
        return;

    /* nothing from the other side for RB_STALL_MS while this one waits on
     * it: its fight is over (it left, or the two parted). Out to the lobby
     * as a lost link, not frozen for good. */
    if (g_frame + 1 >= g_remote_upto + RB_WINDOW || g_waiting) {
        if (now_ms() - g_last_input_ms > RB_STALL_MS) {
            printf("rollback: nothing from the other side for %.0f s at frame %u "
                   "(inputs to %u) -- the fight is left\n",
                   RB_STALL_MS / 1000.0, g_frame, g_remote_upto);
            fflush(stdout);
            disconnected();
            endMP();
            return;
        }
    }

    /* the fight ended on a guess: hold until the input comes */
    if (g_waiting) {
        if (g_frame >= g_remote_upto) {
            send_inputs();
            hold(frame);
            return;
        }
        g_waiting = 0;
    }

    /* Ahead of the other side: adv is how far this side's frame is past the
     * last frame the other reported, g_remote_adv the same seen from there;
     * half their difference is how many frames this side runs ahead. Both
     * are a packet's worth of noise each, so it is averaged over about half
     * a second, and one frame is held at most every RB_SYNC_GAP frames, only
     * once the average is past one frame and a quarter. Held on the raw
     * numbers every 10 frames, as before, both sides took turns holding --
     * 75 held frames in 600 in a test on one PC: the stutter online. */
    adv = (int32_t)g_frame - g_remote_frame;
    g_ahead += ((double)(adv - g_remote_adv) / 2.0 - g_ahead) / 32.0;

    /* too far past the other side's inputs: the window is full */
    if ((int32_t)(g_frame - g_remote_upto) + 1 >= RB_WINDOW) {
        g_last_hold = g_frame;
        send_inputs();
        hold(frame);
        return;
    }
    if (g_ahead >= 1.25 && g_frame - g_last_hold >= RB_SYNC_GAP) {
        g_last_hold = g_frame;
        g_ahead -= 1.0;                 /* this hold's frame, before they hear of it */
        send_inputs();
        hold(frame);
        return;
    }

    adapt_delay();
    take_input();
    send_inputs();

    if (g_dump_at == -2) {
        const char *d = getenv("UMK3_RB_DUMP");
        g_dump_at = d ? atol(d) : -1;
    }
    if (g_dump_at >= 0 && g_frame == (uint32_t)g_dump_at && g_frame >= g_remote_upto) {
        send_inputs();                  /* the dump's frame: its real input first */
        hold(frame);
        return;
    }
    save_slot(slot_of(g_frame));
    if (g_synctest < 0)
        g_synctest = getenv("UMK3_RB_SYNCTEST") != NULL;
    if (g_synctest > 0) {
        /* the run a rollback makes, kept aside, then the snapshot back */
        g_quiet = g_nodraw = 1;
        run(frame, g_frame);
        g_quiet = g_nodraw = 0;
        st_probe_save(2);
        st_rb_load(slot_of(g_frame));
        if (CurrentTask != RB_FIGHT_TASK)
            CurrentTask = RB_FIGHT_TASK;
    }
    if (!run(frame, g_frame) && g_frame >= g_remote_upto) {
        /* the fight ended on a guess: not yet */
        st_rb_load(slot_of(g_frame));
        g_waiting = 1;
        return;
    }
    if (g_synctest > 0) {
        g_st_frames++;
        if (st_probe_words(2, st_word, NULL))
            g_st_bad++;
    }
    if (g_dump_at >= 0 && g_frame == (uint32_t)g_dump_at)
        dump_write();
    g_frame++;
    sp_host_tick();                     /* spectators: the frames confirmed */
    send_hash();
    if (g_frame % RB_REPORT == 0) {
        report();
        synctest_report();
    }
}

void rb_inputs(uint16_t *p1, uint16_t *p2)
{
    *p1 = g_cur[0];
    *p2 = g_cur[1];
}

/* -------------------------------------------------------------- packets */

static void got_inputs(const unsigned char *b, long len)
{
    uint32_t first, n, i, f;
    int them = g_me ^ 1;

    if (len < 20)
        return;
    first = get32(b);
    n = get32(b + 4);
    if (n > RB_SEND_MAX || len < (long)(20 + 2 * n))
        return;
    if ((int32_t)get32(b + 8) > g_remote_frame) {
        g_remote_frame = (int32_t)get32(b + 8);
        g_remote_adv = (int32_t)get32(b + 12);
    }
    if (get32(b + 16) > g_acked && get32(b + 16) <= g_local_upto) {
        double rtt = now_ms() - g_sent_ms[(get32(b + 16) - 1) % RB_RING];

        g_acked = get32(b + 16);
        if (rtt > 0 && rtt < 2000)
            g_rtt = g_rtt > 0 ? (g_rtt * 7 + rtt) / 8 : rtt;
    }

    for (i = 0; i < n; i++) {
        uint16_t v = (uint16_t)(b[20 + 2 * i] | b[21 + 2 * i] << 8);

        f = first + i;
        if (f != g_remote_upto)
            continue;                   /* had it, or a gap: wait for a resend */
        g_in[them][f % RB_RING] = v;
        if (f < g_frame && g_used[f % RB_RING] != v && f < g_rollback)
            g_rollback = f;
        g_remote_upto++;
        g_last_input_ms = now_ms();
    }
}

void rb_net(umk3_long type, const void *data, umk3_long len)
{
    const unsigned char *b = (const unsigned char *)data;

    if (type == RB_PKT_HELLO && len >= 4) {
        g_their_version = (int)get32(b);
        if (g_their_version != RB_VERSION || len < RB_HELLO_LEN) {
            printf("rollback: the other game's netcode is version %u, this is %u\n",
                   get32(b), RB_VERSION);
            g_their_version = -1;
        } else {
            g_their_buttons = (int)get32(b + 4);
            g_their_rollback = (int)get32(b + 8);
            g_their_draw = get32(b + 12);
            g_their_room = (int32_t)get32(b + 16);
            memcpy(g_their_rules, b + 20, ONLINE_RULES_TEXT);
            g_their_rules[ONLINE_RULES_TEXT - 1] = 0;
        }
        g_hello = 1;
        return;
    }
    if (type == RB_PKT_SETUP && len >= 12) {
        g_their_setup[0] = (int32_t)get32(b);
        g_their_setup[1] = (int32_t)get32(b + 4);
        g_their_setup[2] = (int32_t)get32(b + 8);
        g_setup_in = 1;
        return;
    }
    if (!g_on)
        return;
    if (type == RB_PKT_INPUT) {
        if (g_started)
            got_inputs(b, len);
    } else if (type == RB_PKT_HASH && len >= 8) {
        g_their_hash_frame = get32(b);
        g_their_hash = get32(b + 4);
        compare_hashes();
    }
}

/* ------------------------------------------------------------- the fight */

int rb_active(void) { return g_on; }
int rb_quiet(void)  { return g_quiet; }
int rb_nodraw(void) { return g_nodraw; }

/* The replay player's frames run again or skipped (rollback_probe.c): the
 * same silence as a rollback's. */
void rb_silent(int quiet, int nodraw)
{
    g_quiet = quiet;
    g_nodraw = nodraw;
}
int rb_ready(void)  { return !g_on || g_watch >= 2 || (g_hello && g_setup_in); }

/* One line of the numbers report() logs, for the debug menu's NET STATS
 * (game_main.c draws it); 0 when no rollback fight is on. */
int rb_stats(char *out, int n)
{
    if (!g_on)
        return 0;
    snprintf(out, (size_t)n, "ROLLBACK  FRAME %u  PING %.0f  DELAY %d  REMOTE %+d  "
             "ROLLBACKS %u  DEEPEST %u  HOLDS %u  DESYNCS %u",
             g_frame, g_rtt, g_delay, (int)g_frame - g_remote_frame, g_rollbacks,
             g_deepest, g_holds, g_desyncs);
    return 1;
}

/* ---------------------------------------------------------------- the gate
 *
 * Both sides say hello on entering the fight's loading, before STAGE SELECT,
 * and wait for the other's (ONLINE-PHASE-PLAN.md stage 4):
 *
 *   - the rules blocks (online_rules.c, the lobby's) must be the same text,
 *     and the netcode the same version, or the fight does not start: both
 *     sides see the same two hellos and refuse it together, and go back to
 *     the ONLINE screen (FrontEnd.c PortOnlineRefused) instead of loading a
 *     fight that would desync;
 *   - HOST ADVANTAGE OFF (both on rollback): the side and the level are
 *     drawn from both hellos' numbers, host's first, so both machines draw
 *     the same -- who is player one, and one of the binary's fourteen
 *     online stages (mpLevelList, getRandomLevel's list);
 *   - HOST ADVANTAGE ON: as before, the host is player one and its STAGE
 *     SELECT picks; the guest waits for the host's setup and takes its
 *     level (a fight's level picked on each side's select screen, and the
 *     host's word, sendLevelPacket, could come after the guest had begun
 *     loading).
 */
#define RB_GATE_WAIT    3600            /* a minute for the other's hello */
#define RB_REFUSE_SHOW  45              /* frames before leaving, for the packets */

static int seat(void) { return isParent() ? 0 : 1; }

static uint32_t mix(uint32_t h)
{
    h ^= h >> 16;
    h *= 0x7feb352du;
    h ^= h >> 15;
    h *= 0x846ca68bu;
    h ^= h >> 16;
    return h;
}

static int refuse(int why)
{
    static const char *const name[] = { "", "the rules differ", "another version",
                                        "no answer", "the link is gone" };

    g_refusal = why;
    g_gate = 4;
    g_gate_wait = 0;
    disableHeartbeat();                 /* the gate tells it, not disconnected() */
    printf("rollback: NO FIGHT -- %s (here \"%s\", there \"%s\")\n", name[why],
           g_my_rules, g_their_rules);
    fflush(stdout);
    return 1;
}

int rb_online_gate(long *c1, long *c2, long *level)
{
    if (g_watch >= 2) {                 /* a spectator: the host's fight as it is */
        online_rules r;
        *c1 = g_watch_setup[0];
        *c2 = g_watch_setup[1];
        *level = g_watch_setup[2];
        if (g_watch_rules[0] && rules_decode(g_watch_rules, &r))
            rules_set_active(&r);
        snprintf(g_my_rules, sizeof g_my_rules, "%s", g_watch_rules);
        rules_series_swap(0);
        g_swap = 0;
        g_gate = 3;
        return 0;
    }
    switch (g_gate) {
    case 0:
        g_swap = 0;
        g_refusal = 0;
        g_gate_wait = 0;
        send_hello();                   /* theirs may be in already */
        g_gate = 1;
        /* fall through */
    case 1:
        if (!g_hello) {
            if (!isMPConnected())
                return refuse(4);
            if (++g_gate_wait < RB_GATE_WAIT)
                return 1;
            return refuse(3);
        }
        if (g_their_version != RB_VERSION)
            return refuse(2);
        if (strcmp(g_my_rules, g_their_rules))
            return refuse(1);
        if (rules_room_level() < 0 && g_their_room >= 0) {
            /* the room's stage, from the other side's hello: this side's
             * lobby had not told it yet */
            rules_set_room_level(g_their_room);
            printf("rollback: the room's level %d, from the other side\n", (int)g_their_room);
        }
        {
            online_rules r;
            int drawn = g_my_rules[0] && rules_decode(g_my_rules, &r) && !r.host_adv;

            if (drawn && (classic() || !g_their_rollback)) {
                printf("rollback: HOST ADVANTAGE OFF needs rollback on both sides: "
                       "the host's side and stage\n");
                drawn = 0;
            }
            if (drawn) {
                uint32_t host = seat() == 0 ? g_my_draw : g_their_draw;
                uint32_t guest = seat() == 0 ? g_their_draw : g_my_draw;
                uint32_t h = mix(host ^ mix(guest + 0x9e3779b9u));

                g_swap = (int)(h >> 31);
                *level = rules_room_level() >= 0 ? rules_room_level()   /* the room's */
                                                 : mpLevelList[(h & 0xffffu) % 14];
                if (g_swap) {
                    long t = *c1;
                    *c1 = *c2;
                    *c2 = t;
                }
                rules_series_swap(g_swap);
                printf("rollback: HOST ADVANTAGE OFF -- drawn: the %s is player 1, "
                       "level %ld\n", g_swap ? "guest" : "host", *level);
                fflush(stdout);
                g_gate = 3;
                return 0;
            }
        }
        rules_series_swap(0);
        if (rules_room_level() >= 0) {  /* the room chose it: no STAGE SELECT */
            *level = rules_room_level();
            printf("rollback: the room's level %ld\n", *level);
        }
        g_gate = 2;
        g_gate_wait = 0;
        /* fall through */
    case 2:
        if (isParent()) {
            g_gate = 3;
            return 0;
        }
        /* the guest: the host's level, a minute, and on while the host's
         * STAGE SELECT is up (debug_menu.c, which the guest sees) */
        if (!g_setup_in) {
            if (!isMPConnected()) {
                stage_menu_watch_end();
                return refuse(4);
            }
            if (stage_menu_watch_frame() || ++g_gate_wait < RB_GATE_WAIT)
                return 1;
        }
        stage_menu_watch_end();
        if (g_setup_in && *level != g_their_setup[2]) {
            printf("rollback: the host's level %d, not %ld\n",
                   (int)g_their_setup[2], *level);
            *level = g_their_setup[2];
        } else if (!g_setup_in)
            printf("rollback: no setup from the host in a minute\n");
        g_gate = 3;
        return 0;
    case 4:
        /* a moment for the hello to reach the other side, then away */
        if (++g_gate_wait < RB_REFUSE_SHOW && isMPConnected())
            return 1;
        rb_gate_reset();
        return 2;
    default:
        return 0;
    }
}

int rb_refusal(void)   { return g_refusal; }
int rb_gate_busy(void) { return g_gate == 1 || g_gate == 2 || g_gate == 4; }

void rb_gate_reset(void)
{
    g_gate = 0;
    g_hello = 0;
    g_their_rules[0] = 0;
    g_setup_in = 0;
    g_swap = 0;
}

int rb_local_p1(void)
{
    return (isParent() != 0) ^ g_swap;
}

void rb_online_begin(int c1, int c2, int level)
{
    const char *e = getenv("UMK3_RB_DELAY");
    uint32_t seed;

    mods_online(1);                     /* the game's own fight (mods.c) */
    if (classic()) {
        printf("rollback: off (UMK3_NETCODE=classic)\n");
        g_me = seat();                  /* the setup still says the level */
        g_setup[0] = c1;
        g_setup[1] = c2;
        g_setup[2] = level;
        send_setup();
        return;
    }
    g_on = 1;
    g_started = 0;
    g_watch_over = 0;
    g_me = g_watch >= 2 ? 0 : seat() ^ g_swap;  /* the draw may put the host on the right */
    g_delay = RB_DELAY;
    g_delay_fixed = e && *e >= '0' && *e <= '6';
    if (g_delay_fixed)
        g_delay = *e - '0';
    g_frame = 0;
    memset(g_in, 0, sizeof g_in);
    memset(g_used, 0, sizeof g_used);
    memset(g_hash_frame, 0xff, sizeof g_hash_frame);
    g_local_upto = g_remote_upto = (uint32_t)g_delay;   /* the first are 0 */
    g_acked = 0;
    g_rtt = 0;
    memset(g_sent_ms, 0, sizeof g_sent_ms);
    g_rollback = NONE;
    g_remote_frame = 0;
    g_remote_adv = 0;
    g_last_hold = 0;
    g_waiting = 0;
    g_linger = 0;
    g_next_hash = RB_HASH_EVERY;
    g_my_hash_frame = g_their_hash_frame = NONE;
    g_desyncs = g_rollbacks = g_resims = g_deepest = g_holds = 0;
    g_ahead = 0;
    g_last_input_ms = now_ms();
    if (getenv("UMK3_RB_DUMP")) {       /* a test's dump: this side's old one away */
        char path[64];
        snprintf(path, sizeof path, "logs/rbdump-%d.bin", g_me);
        remove(path);
    }

    /* both machines know the fighters and the level, so both seed the
     * engine's generators from them, before anything loads */
    g_setup[0] = c1;
    g_setup[1] = c2;
    g_setup[2] = level;
    seed = 0x9e3779b9u ^ (uint32_t)c1 * 0x85ebca6bu ^ (uint32_t)c2 * 0xc2b2ae35u
         ^ (uint32_t)level * 0x27d4eb2fu;
    g_seed = seed;
    umk3_rand_seed = seed;
    rand_seed = (int32_t)(seed >> 1);
    if (g_watch >= 2) {
        printf("rollback: watching the fight, %d vs %d on %d\n", c1, c2, level);
        return;
    }
    send_setup();
    printf("rollback: online fight, player %d, %d vs %d on %d\n", g_me + 1,
           c1, c2, level);
}

void rb_online_end(void)
{
    mods_online(0);
    sp_host_end();                      /* spectators: the last frame's count */
    if (g_watch)
        rb_watch_stop();
    g_gate = 0;                         /* the next fight says hello again */
    g_setup_in = 0;
    g_swap = 0;
    if (!g_on) {
        g_hello = 0;                    /* the classic netcode's */
        g_their_rules[0] = 0;
        return;
    }
    if (g_started) {
        report();
        synctest_report();
    }
    g_st_n = 0;
    g_st_frames = g_st_bad = 0;
    st_rb_end();
    g_on = 0;
    g_started = 0;
    g_hello = 0;
    g_their_rules[0] = 0;
    g_linger = RB_LINGER;
}

void rb_poll(void)
{
    sp_host_tick();                     /* spectators: what is left to send */
    if (g_linger > 0) {
        g_linger--;
        send_inputs();
    }
}
