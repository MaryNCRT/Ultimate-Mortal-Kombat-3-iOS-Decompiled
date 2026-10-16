/*
 * gamecode_stubs.c — the three boundaries the front end calls across.
 *
 * `decomp/gamecode` is complete, but a menu that runs still has to resolve
 * everything it calls *out* to. Three groups, and they are stubbed for three
 * different reasons:
 *
 *   EA_SDK / EASOC   ~27 calls. Store, social, tracking, Facebook, leaderboards.
 *                    These went to EA's servers, some of which no longer exist.
 *                    The project's plan was always to delete this layer rather
 *                    than decompile it, and these are that deletion.
 *
 *   multiplayer      was ~30 calls here; now decomp/gamecode/limeMPGameSpecific.c
 *                    (the game's packets) over runtime/mp_net.c (the session).
 *
 *   mk3_*            7 calls into gamecode/logic, the fight engine, which is
 *                    3 of 2,172 written. The front end only reaches these on
 *                    the way *out* of the menus, so a menu build can link
 *                    without them and a fight build cannot.
 *
 * ## Every answer here is a state the original could produce
 *
 * That is the rule this file follows and it is worth stating. The store and
 * the leaderboards answer as a device with no network does, a state the front
 * end has real code for, so the menus behave like the retail game offline,
 * not like a game running against a broken library.
 *
 * Where an answer would be a lie, the call does nothing and returns nothing
 * instead. `EASOC_FBGetFriendName` returns an empty string rather than a name.
 */

#include <stddef.h>

#include "gamecode_globals.h"


/* ---- counters, so a test can assert a call happened without a server ---- */

static long g_ea_events;

long gamecode_stub_ea_events(void)  { return g_ea_events; }


/* ------------------------------------------------------------- EA_SDK ---- */

void EASDK_LogEvent(long id, long a, const char *s1, long b, const char *s2)
{ (void)id; (void)a; (void)s1; (void)b; (void)s2; g_ea_events++; }

void EASDK_LogEventEnumEnum(long id, long a, long b, long c, long d)
{ (void)id; (void)a; (void)b; (void)c; (void)d; g_ea_events++; }

void EASDK_LogEventEnumEnumString(long id, long a, const char *s1,
                                  long b, const char *s2)
{ (void)id; (void)a; (void)s1; (void)b; (void)s2; g_ea_events++; }

void EASDK_LogEventEnumEnumStringNum(long id, long a, const char *s,
                                     long b, long n)
{ (void)id; (void)a; (void)s; (void)b; (void)n; g_ea_events++; }

void EASDK_SetLoggingDisable(long off) { (void)off; }
void EASDK_ShowMessage(void)           { }
void EASDK_GetMoreGames(const char *language, long a)
{ (void)language; (void)a; }

/* No network: the ticker is empty and nothing is connected. DrawTicker already
 * handles an empty list -- it is what the game shows offline. */
long        EASDK_ConnectedToNetwork(void)      { return 0; }
void       *EASDK_GetLoadedTicker(long i)       { (void)i; return NULL; }
long        EASDK_GetTickerId(void *t)          { (void)t; return 0; }
const char *EASDK_GetTickerMsg(void)            { return ""; }
const char *EASDK_GetTickerUrl(void *t)         { (void)t; return ""; }


/* -------------------------------------------------- EA social / Mayhem ---- */

void  EASOC_debugFunc(void)                     { }
void  EASOC_FBInit(void)                        { }
int   EASOC_FBGetFriendsNum(void)               { return 0; }
char *EASOC_FBGetFriendName(int index)          { (void)index; return ""; }

/* Mayhem is EA's leaderboard service. "Ready, nothing pending, no name needed"
 * is the quiet state: FE_Task_Karnage_Summary's four-state username handshake
 * then completes instead of hanging on a reply that never comes. */
int  EASOC_MayhemIsReady(void)                  { return 1; }
int  EASOC_MayhemIsPending(void)                { return 0; }
int  EASOC_MayhemNeedsUserName(void)            { return 0; }
void EASOC_MayhemReset(void)                    { }
void EASOC_MayhemTest(long a)                   { (void)a; }
void EASOC_MayhemSetUserName(const char *name)  { (void)name; }
void EASOC_MayhemGetUserStat(const char *stat)  { (void)stat; }

void EASOC_MayhemPostStatWithData(const char *key, long value,
                                  const long *data, long size)
{ (void)key; (void)value; (void)data; (void)size; }

void EASOC_MayhemInitLeaderBoard(long board, const char *filter, long period,
                                 long count, const char *stat)
{ (void)board; (void)filter; (void)period; (void)count; (void)stat; }

void EASOC_MayhemReloadLeaderBoard(long board, long period, long page,
                                   long count, const char *stat)
{ (void)board; (void)period; (void)page; (void)count; (void)stat; }

long EASOC_MayhemGetLeaderBoard(long page, long count, long period, void *cb)
{ (void)page; (void)count; (void)period; (void)cb; return 0; }


/* -------------------------------------------------------- multiplayer ---- */

/* Not stubbed any more: decomp/gamecode/limeMPGameSpecific.c is the game's
 * own multiplayer code and runtime/mp_net.c the session under it. */


/* ------------------------------------------- gamecode/logic, the fight ---- */

/* The menus call into these only on the way out, so a MENU-ONLY build links
 * against these and any build that carries the real `mk3.c` must not.
 *
 * `mk3.c` is now decompiled -- all nineteen of its functions -- so
 * `UMK3_HAVE_MK3` is what a target sets to say "the real ones are in this
 * link". `umk3-test` sets it; `umk3-menu` does not, because it links no
 * gamecode/logic at all and would otherwise fail to resolve them.
 *
 * The stubs stay rather than being deleted: the menu build is still the thing
 * that proves the front end works on its own, and it must keep building. */
#ifndef UMK3_HAVE_MK3

void mk3_init(long p1model, long p2model, void (*getBBox)(void), long flag)
{ (void)p1model; (void)p2model; (void)getBBox; (void)flag; }

void mk3_init_game(void)                       { }
void mk3_update(const long *joy, void **objects) { (void)joy; (void)objects; }
void mk3_dizzy(void)                           { }
void mk3_set_four_button(long side, long four) { (void)side; (void)four; }
long mk3_who_in_front(void)                    { return 0; }
void no_ai_hack(void)                          { }

#endif /* UMK3_HAVE_MK3 */


/* ------------------------------------------------- compiler support ---- */

/* The ARM EABI's signed-modulo helper. It is in the transcription because the
 * compiler emitted a real call rather than a reciprocal multiply -- which it
 * only does when the divisor is not a constant -- so the call is evidence about
 * the source and was kept. On a host it is just the operator.
 *
 * `long` and not `int`: the transcription declares it that way and on 32-bit
 * ARM the two are the same width. */
long __modsi3(long a, long b)
{
    return a % b;
}

/* The compiler-generated thunk that runs `LocaleManager`'s static destructor.
 * `__GLOBAL__D` registers it with `__cxa_atexit` at startup. There is no
 * LocaleManager here to destroy, so it does nothing, and it is stubbed rather
 * than removed because the registration call is part of the transcription. */
void __tcf_0(void *p)
{
    (void)p;
}


/* --------------------------------------------------------- the one C++ ---- */

/* `LocaleManager` is the only C++ class the front end touches, and the
 * transcription carries its symbols mangled because that is how they appear in
 * the binary: `_ZN13LocaleManagerC1Ev` is the constructor and
 * `_ZN13LocaleManager10s_instanceE` its static instance, both reached from the
 * translation unit's `__GLOBAL__I` static-initialisation thunk.
 *
 * Defining them here in C works because a mangled name is just a name to the
 * linker. The class body is not in the decomp, so the instance gets placeholder
 * storage rather than an invented layout -- nothing in the menus reads it, and
 * the constructor is what would fill it.
 */
struct LocaleManager_placeholder { long words[16]; };
struct LocaleManager_placeholder _ZN13LocaleManager10s_instanceE;

void _ZN13LocaleManagerC1Ev(void *self)
{
    (void)self;
}


/* -------------------------------------------------- an odd declaration ---- */

/* `PlayerZPos` used to be defined here, as a zeroed anonymous union. The image
 * holds 0xbee147ae there -- -0.44 -- and tools/mkglobals.py now defines it with
 * those bytes, because the only declaration that names it in full is the slot
 * FrontEnd.c reads it through. */

/* `TheHud` is a HUD, and HUD embeds a `HUDANIM anim` by value whose layout is
 * not established anywhere in the decomp. Its size is unknown, so the generator
 * skips it and this gives it the smallest storage that lets the tree link,
 * clearly marked. The HUD is a fight-screen structure; nothing in the menus
 * reads it. Whoever decompiles HUDANIM should delete this. */
struct HUD_placeholder { long words[16]; };
struct HUD_placeholder TheHud;
