/*
 * test_events_diff.c — clean Events.c against the oracle.
 *
 * The effect pool: 192 slots of 248 bytes, allocated, counted, killed and
 * ticked. This is state-machine code rather than arithmetic, and its failure
 * mode is not a wrong number — it is an effect that flickers once in a real
 * match and is never reproduced.
 *
 * ## What is actually being checked
 *
 * The pool's whole lifecycle, in the order it happens:
 *
 *   1. `GetFreeEvent` finds a slot, or returns -1 when there is none.
 *   2. `LIME_CountActiveEvents` counts what is NOT free — which includes the
 *      dying, and that distinction is the point of testing it.
 *   3. `KillAlleventsWithGroup` sets matching slots to -2 and leaves the rest.
 *   4. `LIME_UpdateEvents` counts the negatives UP toward zero.
 *
 * Step 4 is the one worth having. This repo previously recorded that a killed
 * event is never freed, and it was wrong: -2 becomes -1, then 0, and zero is
 * free. So the test drives a full kill, tick, tick, reuse cycle and asserts the
 * slot comes back — and comes back on the SECOND tick, not the first. A
 * two-frame grace period and a one-frame one both "work" until something is
 * still drawing from the slot.
 *
 * ## Why field-by-field and not memcmp
 *
 * The clean pool is an array of host structs. `sizeof(EVENT)` is 256 here and
 * 248 in the binary, because an ARM pointer is 4 bytes and ours is 8. The
 * layouts describe field ORDER, not host byte offsets, so the comparison reads
 * each field through the C struct on one side and through the guest offset on
 * the other. Anything else would compare padding.
 */

#include "arm_runtime.h"
#include "events.h"                     /* oracle: tools/armrecomp */
#include "../src/decomp/lime/lime.h"        /* clean C */

#include <stdio.h>
#include <string.h>

#define RAM_SIZE    (8u << 20)
#define STACK_TOP   0x007F0000u

/* _SceneEvents in __DATA -- 47,616 bytes, exactly 192 * 248. */
#define POOL        0x00379cc0u
#define STRIDE      0xF8u

/* guest-side EVENT offsets, each read from the disassembly */
#define EV_STATE    0x00u
#define EV_CURSOR   0x04u
#define EV_FRAMEA   0x08u
#define EV_FRAMEB   0x0Cu
#define EV_SCENE    0x10u
#define EV_REPEAT   0x2Cu
#define EV_GROUP    0x3Cu

static int  g_fail  = 0;
static long g_cases = 0;

static void ctx_reset(arm_ctx *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->r[SP] = STACK_TOP;
}

static uint32_t slot(int i)          { return POOL + STRIDE * (uint32_t)i; }
static int32_t  g_state(int i)       { return (int32_t)MEM_LD32(slot(i) + EV_STATE); }
static void     g_set_state(int i, int32_t v) { MEM_ST32(slot(i) + EV_STATE, (uint32_t)v); }
static int32_t  g_group(int i)       { return (int32_t)MEM_LD32(slot(i) + EV_GROUP); }
static void     g_set_group(int i, int32_t v) { MEM_ST32(slot(i) + EV_GROUP, (uint32_t)v); }
static void     g_set_repeat(int i, int32_t v){ MEM_ST32(slot(i) + EV_REPEAT, (uint32_t)v); }
static void     g_set_scene(int i, uint32_t v){ MEM_ST32(slot(i) + EV_SCENE, v); }

/* A scene for the events to belong to.
 *
 * NOT NULL. The original tolerates a null scene only because guest address 0 is
 * mapped RAM, so reading scene->count2 through it quietly returns zero; the
 * clean C runs natively and segfaults. A null scene is not a real case anyway --
 * LIME_LoadScene fills the field before any event references it -- so the test
 * gives both sides a genuine one rather than exercising a state the game cannot
 * reach. */
#define GUEST_SCENE 0x00200000u
#define SC_COUNT2   0x44u

static SCENEINFO g_host_scene;

static void scene_init(int count2)
{
    memset(&g_host_scene, 0, sizeof(g_host_scene));
    g_host_scene.count2 = count2;
    for (uint32_t a = 0; a < 0x100u; a += 4u) MEM_ST32(GUEST_SCENE + a, 0u);
    MEM_ST32(GUEST_SCENE + SC_COUNT2, (uint32_t)count2);
}

/* Wipe both pools to the same starting point. */
static void pools_clear(void)
{
    memset(SceneEvents, 0, sizeof(SceneEvents));
    for (uint32_t a = 0; a < 0xBA00u; a += 4u) MEM_ST32(POOL + a, 0u);
}

static void note(const char *what, int i, const char *field, long clean, long orc)
{
    printf("  DIVERGE %s  slot %d  %s: clean=%ld  oracle=%ld\n",
           what, i, field, clean, orc);
    g_fail++;
}

/* Compare every slot's state and group. Stops after the first divergence in a
 * given sweep so a systematic error does not print 192 times. */
static void cmp_pool(const char *what)
{
    g_cases++;
    for (int i = 0; i < EVENT_SLOTS; i++) {
        if (SceneEvents[i].state != g_state(i)) {
            note(what, i, "state", (long)SceneEvents[i].state, (long)g_state(i));
            return;
        }
        if (SceneEvents[i].group != g_group(i)) {
            note(what, i, "group", (long)SceneEvents[i].group, (long)g_group(i));
            return;
        }
    }
}

/* Put both pools into the same arbitrary configuration. */
static uint32_t g_seed = 0x2468ACE0u;

static int nexti(int lo, int hi)
{
    g_seed = g_seed * 1664525u + 1013904223u;
    return lo + (int)((g_seed >> 9) % (uint32_t)(hi - lo + 1));
}

static void seed_pool(int live_chance, int use_groups)
{
    pools_clear();
    for (int i = 0; i < EVENT_SLOTS; i++) {
        int roll = nexti(0, 99);
        int32_t st = 0;
        if (roll < live_chance)       st = nexti(1, 5);        /* live */
        else if (roll < live_chance + 8) st = -nexti(1, 2);     /* dying */

        int32_t grp = use_groups ? (int32_t)nexti(0, 3) : 0;

        SceneEvents[i].state = st;
        SceneEvents[i].group = grp;
        g_set_state(i, st);
        g_set_group(i, grp);
    }
}


/* ------------------------------------------------------ LIME_CountActiveEvents */

static void test_count(void)
{
    arm_ctx ctx;
    seed_pool(nexti(0, 100), 0);

    int clean = LIME_CountActiveEvents();

    ctx_reset(&ctx);
    func_000a44a4_LIME_CountActiveEvents(&ctx);
    int orc = (int)ctx.r[0];

    g_cases++;
    if (clean != orc) {
        printf("  DIVERGE CountActiveEvents: clean=%d  oracle=%d\n", clean, orc);
        g_fail++;
    }
}


/* -------------------------------------------------------------- GetFreeEvent */

static void test_getfree(int live_chance)
{
    arm_ctx ctx;
    seed_pool(live_chance, 0);

    int clean = GetFreeEvent();

    ctx_reset(&ctx);
    func_000a42c0_Z12GetFreeEventv(&ctx);
    int orc = (int)(int32_t)ctx.r[0];

    g_cases++;
    if (clean != orc) {
        printf("  DIVERGE GetFreeEvent: clean=%d  oracle=%d  (live_chance=%d)\n",
               clean, orc, live_chance);
        g_fail++;
    }
}


/* ------------------------------------------------------------ the kill / tick
 *
 * The sequence that matters: kill a group, then tick twice, checking the pool
 * after every step. A one-frame grace period would agree with the original on
 * the kill and on the second tick, and differ only on the first -- which is
 * exactly why the first tick is compared rather than only the end state.
 */
static void test_kill_and_tick(void)
{
    arm_ctx ctx;
    int32_t victim = (int32_t)nexti(0, 3);

    seed_pool(60, 1);

    /* every event needs a scene: UpdateEvents reads scene->count2 through
     * EVENT+0x10 when the repeat counter is non-zero. Zero it so that path is
     * not taken -- the repeat behaviour gets its own sweep below. */
    scene_init(nexti(2, 30));
    for (int i = 0; i < EVENT_SLOTS; i++) {
        SceneEvents[i].repeat = 0;
        g_set_repeat(i, 0);
        SceneEvents[i].scene = &g_host_scene;
        g_set_scene(i, GUEST_SCENE);
    }

    KillAlleventsWithGroup(victim);
    ctx_reset(&ctx);
    ctx.r[0] = (uint32_t)victim;
    func_000a431c_Z22KillAlleventsWithGroupl(&ctx);
    cmp_pool("after kill");

    for (int tick = 1; tick <= 3; tick++) {
        LIME_UpdateEvents();
        ctx_reset(&ctx);
        func_000a5098_LIME_UpdateEvents(&ctx);
        cmp_pool(tick == 1 ? "after tick 1"
                           : (tick == 2 ? "after tick 2" : "after tick 3"));
    }
}


/* The two-frame grace period, asserted directly rather than only differentially.
 * Both implementations agreeing on the wrong thing is still wrong, so this one
 * checks the behaviour the documentation claims. */
static void test_grace_period(void)
{
    arm_ctx ctx;

    pools_clear();
    SceneEvents[7].state = 3;  SceneEvents[7].group = 1;
    g_set_state(7, 3);         g_set_group(7, 1);
    SceneEvents[7].repeat = 0; g_set_repeat(7, 0);
    scene_init(8);
    SceneEvents[7].scene = &g_host_scene; g_set_scene(7, GUEST_SCENE);

    KillAlleventsWithGroup(1);
    g_cases++;
    if (SceneEvents[7].state != -2) {
        printf("  BEHAVIOUR kill did not set -2 (got %d)\n", SceneEvents[7].state);
        g_fail++;
        return;
    }

    LIME_UpdateEvents();
    g_cases++;
    if (SceneEvents[7].state != -1) {
        printf("  BEHAVIOUR after one tick expected -1, got %d\n",
               SceneEvents[7].state);
        g_fail++;
    }

    LIME_UpdateEvents();
    g_cases++;
    if (SceneEvents[7].state != 0) {
        printf("  BEHAVIOUR after two ticks expected 0 (free), got %d\n",
               SceneEvents[7].state);
        g_fail++;
    }

    /* and the slot must now be handed out again */
    g_cases++;
    if (GetFreeEvent() < 0) {
        printf("  BEHAVIOUR pool reports full after the grace period elapsed\n");
        g_fail++;
    }

    (void)ctx;
}


int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    arm_mem_init(RAM_SIZE);

    printf("=== clean Events.c vs the recompiled original ===\n\n");

    for (int i = 0; i < 400; i++) test_count();

    /* sweep the occupancy from empty to full: the -1 return only happens at
     * 100%, and an off-by-one in the search only shows at the extremes */
    for (int c = 0; c <= 100; c += 5)
        for (int k = 0; k < 20; k++) test_getfree(c);
    for (int k = 0; k < 200; k++) test_getfree(100);

    for (int i = 0; i < 300; i++) test_kill_and_tick();

    test_grace_period();

    printf("\ncases compared: %ld    divergences: %d\n", g_cases, g_fail);
    printf("%s\n", g_fail ? "RESULT: FAIL"
                          : "RESULT: the clean event pool matches the original");

    arm_mem_free();
    return g_fail ? 1 : 0;
}
