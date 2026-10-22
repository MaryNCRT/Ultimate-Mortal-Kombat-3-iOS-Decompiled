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
 *     ReadControls produces, 11 bits) is taken for the frame RB_DELAY ahead
 *     and sent with every input the other side has not acknowledged.
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
#endif

#include "savestate.h"
#include "mp_net.h"
#include "rollback.h"

/* the game's side */
long isParent(void);                    /* limeMPGameSpecific.c */
extern int CurrentTask;                 /* GameCode.c: 6 is the fight */
extern int Settings[10];                /* [4] the buttons, 4, 5 or 6 */
extern int32_t  rand_seed;              /* fight_runtime.c: limeRand's */
extern uint32_t umk3_rand_seed;         /* fight_runtime.c: the engine's */
uint16_t rb_game_local_input(void);     /* GameCode.c */
uint32_t rb_game_hash(void);            /* GameCode.c */
void     rb_game_buttons(int p1, int p2);   /* GameCode.c */

#define RB_VERSION      1
#define RB_DELAY        2               /* frames, UMK3_RB_DELAY 0..6 */
#define RB_WINDOW       8               /* frames ahead of the last input */
#define RB_SLOTS        (RB_WINDOW + 3) /* snapshots, the last for holds */
#define RB_RING         64              /* inputs and hashes, by frame */
#define RB_SEND_MAX     32              /* inputs in one packet */
#define RB_HASH_EVERY   60
#define RB_REPORT       600
#define RB_LINGER       180             /* frames of sending after a fight */
#define RB_FIGHT_TASK   6

#define NONE            0xffffffffu

static int g_on, g_started, g_me, g_delay = RB_DELAY;
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

/* hello: the other machine's buttons and its view of the fight's setup */
static int      g_hello, g_their_buttons;
static int32_t  g_setup[3], g_their_setup[3];

/* desync check */
static uint32_t g_hash[RB_RING], g_hash_frame[RB_RING];
static uint32_t g_next_hash = RB_HASH_EVERY;
static uint32_t g_their_hash_frame = NONE, g_their_hash;
static uint32_t g_my_hash_frame = NONE, g_my_hash;
static uint32_t g_desyncs;

/* numbers for the log */
static uint32_t g_rollbacks, g_resims, g_deepest, g_holds;
static double   g_save_ms, g_save_max;
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

    if (g_local_upto > RB_SEND_MAX && first < g_local_upto - RB_SEND_MAX)
        first = g_local_upto - RB_SEND_MAX;
    if (first > g_local_upto)
        first = g_local_upto;
    n = g_local_upto - first;
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
}

static void send_hello(void)
{
    unsigned char b[24];

    put32(b, RB_VERSION);
    put32(b + 4, (uint32_t)Settings[4]);
    put32(b + 8, (uint32_t)g_setup[0]);
    put32(b + 12, (uint32_t)g_setup[1]);
    put32(b + 16, (uint32_t)g_setup[2]);
    put32(b + 20, (uint32_t)g_me);
    mpnet_send(RB_PKT_HELLO, b, 24, 1);
}

static void report(void)
{
    printf("rollback: frame %u -- %u rollbacks, %u frames run again, deepest %u; "
           "%u holds; snapshot %.2f ms avg, %.2f max (%.1f MB); remote %d frames "
           "behind; desyncs %u\n",
           g_frame, g_rollbacks, g_resims, g_deepest, g_holds,
           g_saves ? g_save_ms / g_saves : 0.0, g_save_max,
           st_rb_size() / 1048576.0, (int)g_frame - g_remote_frame, g_desyncs);
    fflush(stdout);
    g_save_ms = g_save_max = 0;
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
static int run(void (*frame)(void), uint32_t f)
{
    g_cur[g_me] = g_in[g_me][f % RB_RING];
    g_cur[g_me ^ 1] = remote_input(f);
    g_used[f % RB_RING] = g_cur[g_me ^ 1];
    frame();
    g_hash[f % RB_RING] = rb_game_hash();
    g_hash_frame[f % RB_RING] = f;
    return CurrentTask == RB_FIGHT_TASK;
}

/* Back to the frame that ran on a wrong guess, and forward again. */
static void roll_back(void (*frame)(void))
{
    uint32_t f = g_rollback, depth = g_frame - g_rollback;

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
}

/* Draw the next frame without keeping it. */
static void hold(void (*frame)(void))
{
    g_holds++;
    save_slot(RB_SLOTS - 1);
    g_quiet = 1;
    run(frame, g_frame);
    g_quiet = 0;
    st_rb_load(RB_SLOTS - 1);
}

/* The loaded fighters: 60 MB a fight only reads, so no snapshot holds them
 * (gamecode_globals.c, 0x5f4 = PRELOAD_STRIDE). */
extern char PreloadedCharacters[41148][0x5f4];

static void start(void)
{
    int mine = Settings[4], theirs = g_their_buttons;

    g_started = 1;
    st_rb_skip(PreloadedCharacters, sizeof PreloadedCharacters);
    if (st_rb_begin(RB_SLOTS) != 0)
        printf("rollback: NO SNAPSHOTS -- save states are off in this build\n");
    rb_game_buttons(g_me ? theirs : mine, g_me ? mine : theirs);
    printf("rollback: the fight starts, this side is player %d, delay %d, "
           "buttons %d / %d\n", g_me + 1, g_delay,
           g_me ? theirs : mine, g_me ? mine : theirs);
    if (memcmp(g_setup, g_their_setup, sizeof g_setup))
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

    if (g_rollback != NONE && g_rollback < g_frame)
        roll_back(frame);
    if (CurrentTask != RB_FIGHT_TASK)
        return;

    /* the fight ended on a guess: hold until the input comes */
    if (g_waiting) {
        if (g_frame >= g_remote_upto) {
            send_inputs();
            hold(frame);
            return;
        }
        g_waiting = 0;
    }

    /* too far past the other side's inputs, or ahead of it on average */
    adv = (int32_t)g_frame - g_remote_frame;
    if ((int32_t)(g_frame - g_remote_upto) + 1 >= RB_WINDOW
        || (adv - g_remote_adv >= 3 && g_frame - g_last_hold > 10)) {
        g_last_hold = g_frame;
        send_inputs();
        hold(frame);
        return;
    }

    /* this frame's input, for RB_DELAY frames on */
    g_in[g_me][(g_frame + g_delay) % RB_RING] = rb_game_local_input();
    g_local_upto = g_frame + g_delay + 1;
    send_inputs();

    save_slot(slot_of(g_frame));
    if (!run(frame, g_frame) && g_frame >= g_remote_upto) {
        /* the fight ended on a guess: not yet */
        st_rb_load(slot_of(g_frame));
        g_waiting = 1;
        return;
    }
    g_frame++;
    send_hash();
    if (g_frame % RB_REPORT == 0)
        report();
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
    if (get32(b + 16) > g_acked)
        g_acked = get32(b + 16);

    for (i = 0; i < n; i++) {
        uint16_t v = (uint16_t)(b[20 + 2 * i] | b[21 + 2 * i] << 8);

        f = first + i;
        if (f != g_remote_upto)
            continue;                   /* had it, or a gap: wait for a resend */
        g_in[them][f % RB_RING] = v;
        if (f < g_frame && g_used[f % RB_RING] != v && f < g_rollback)
            g_rollback = f;
        g_remote_upto++;
    }
}

void rb_net(long type, const void *data, long len)
{
    const unsigned char *b = (const unsigned char *)data;

    if (type == RB_PKT_HELLO && len >= 24) {
        if (get32(b) != RB_VERSION)
            printf("rollback: the other game's netcode is version %u, this is %u\n",
                   get32(b), RB_VERSION);
        g_their_buttons = (int)get32(b + 4);
        g_their_setup[0] = (int32_t)get32(b + 8);
        g_their_setup[1] = (int32_t)get32(b + 12);
        g_their_setup[2] = (int32_t)get32(b + 16);
        g_hello = 1;
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
int rb_ready(void)  { return !g_on || g_hello; }

/* The guest's fight waits, before anything loads, for the host's hello and
 * takes the host's level: each side picked one of its own on the select
 * screen, and the host's word (sendLevelPacket) can come after the guest has
 * begun loading. 1 while waiting; gives up after ten seconds. */
int rb_host_level(long *level)
{
    static int waited;

    /* the classic netcode too: its guest drew a level it never loaded when
     * the host's arrived mid-fight (RenderLevelBG) */
    if (isParent())
        return 0;
    if (!g_hello && ++waited < 600)
        return 1;
    if (g_hello && *level != g_their_setup[2]) {
        printf("rollback: the host's level %d, not %ld\n",
               (int)g_their_setup[2], *level);
        *level = g_their_setup[2];
    } else if (!g_hello)
        printf("rollback: no hello from the host in ten seconds\n");
    waited = 0;
    return 0;
}

void rb_online_begin(int c1, int c2, int level)
{
    const char *e = getenv("UMK3_RB_DELAY");
    uint32_t seed;

    if (getenv("UMK3_NETCODE") && !strcmp(getenv("UMK3_NETCODE"), "classic")) {
        printf("rollback: off (UMK3_NETCODE=classic)\n");
        g_me = isParent() ? 0 : 1;      /* the hello still says the level */
        g_setup[0] = c1;
        g_setup[1] = c2;
        g_setup[2] = level;
        send_hello();
        return;
    }
    g_on = 1;
    g_started = 0;
    g_me = isParent() ? 0 : 1;
    g_delay = RB_DELAY;
    if (e && *e >= '0' && *e <= '6')
        g_delay = *e - '0';
    g_frame = 0;
    memset(g_in, 0, sizeof g_in);
    memset(g_used, 0, sizeof g_used);
    memset(g_hash_frame, 0xff, sizeof g_hash_frame);
    g_local_upto = g_remote_upto = (uint32_t)g_delay;   /* the first are 0 */
    g_acked = 0;
    g_rollback = NONE;
    g_remote_frame = 0;
    g_remote_adv = 0;
    g_last_hold = 0;
    g_waiting = 0;
    g_linger = 0;
    g_next_hash = RB_HASH_EVERY;
    g_my_hash_frame = g_their_hash_frame = NONE;
    g_desyncs = g_rollbacks = g_resims = g_deepest = g_holds = 0;

    /* both machines know the fighters and the level, so both seed the
     * engine's generators from them, before anything loads */
    g_setup[0] = c1;
    g_setup[1] = c2;
    g_setup[2] = level;
    seed = 0x9e3779b9u ^ (uint32_t)c1 * 0x85ebca6bu ^ (uint32_t)c2 * 0xc2b2ae35u
         ^ (uint32_t)level * 0x27d4eb2fu;
    umk3_rand_seed = seed;
    rand_seed = (int32_t)(seed >> 1);
    send_hello();
    printf("rollback: online fight, player %d, %d vs %d on %d\n", g_me + 1,
           c1, c2, level);
}

void rb_online_end(void)
{
    if (!g_on) {
        g_hello = 0;                    /* the classic netcode's */
        return;
    }
    if (g_started)
        report();
    st_rb_end();
    g_on = 0;
    g_started = 0;
    g_hello = 0;
    g_linger = RB_LINGER;
}

void rb_poll(void)
{
    if (g_linger > 0) {
        g_linger--;
        send_inputs();
    }
}
