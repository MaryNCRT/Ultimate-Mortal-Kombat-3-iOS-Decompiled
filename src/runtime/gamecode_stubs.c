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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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

/* Connected means an account: the launcher wrote leaderboards.txt (below).
 * A GUEST is the device with no network, which is what keeps the guest out
 * of the leaderboards -- the front end then shows its own "no internet". The
 * ticker stays empty either way; DrawTicker handles that. */
static int mayhem_load(void);
long        EASDK_ConnectedToNetwork(void)      { return mayhem_load(); }
void       *EASDK_GetLoadedTicker(long i)       { (void)i; return NULL; }
long        EASDK_GetTickerId(void *t)          { (void)t; return 0; }
const char *EASDK_GetTickerMsg(void)            { return ""; }
const char *EASDK_GetTickerUrl(void *t)         { (void)t; return ""; }


/* -------------------------------------------------- EA social / Mayhem ---- */

void  EASOC_debugFunc(void)                     { }
void  EASOC_FBInit(void)                        { }
int   EASOC_FBGetFriendsNum(void)               { return 0; }
char *EASOC_FBGetFriendName(int index)          { (void)index; return ""; }

/* Mayhem was EA's leaderboard service; these are the account server's
 * (Port, not in the binary). The game has no HTTPS of its own, so the
 * launcher carries the boards both ways through two files beside umk3.ini:
 *
 *   leaderboards.txt   written by the launcher before the game starts, only
 *                      for an account (a guest has none):
 *                          user=Mary
 *                          board=survival_easy
 *                          Mary|41
 *                          ...rows, best first
 *   scores.txt         appended here, one "<board> <score>" a line; the
 *                      launcher posts them while the game runs (every few
 *                      seconds) and empties it. The server keeps each
 *                      account's best.
 *
 * The launcher writes leaderboards.txt again while the game runs, so the
 * other players' new scores come in: the file is read again whenever it
 * changed. A run that had the debug menu's help (game_main.c,
 * PortDbgTainted) is not posted.
 *
 * "Ready, nothing pending, no name needed" is still the answer: the boards
 * are already on disk, and the name is the account's. A score posted here is
 * also put into the board in memory, so the player sees it at once. */

#define MAYHEM_BOARDS 4
#define MAYHEM_ROWS   100

typedef struct { char name[24]; long score; } MayhemRow;
typedef struct { char key[33]; long n; MayhemRow row[MAYHEM_ROWS]; } MayhemBoard;

static char        g_mh_dir[512];       /* umk3.ini's folder, with the slash */
static char        g_mh_user[24];
static MayhemBoard g_mh_board[MAYHEM_BOARDS];
static long        g_mh_boards;
static int         g_mh_loaded = -1;    /* -1 not read yet, 0 no account, 1 */
static struct stat g_mh_st;             /* leaderboards.txt as last read */
static const char *g_mh_stat;           /* the board the screen asked for */

void mayhem_set_dir(const char *ini_path)
{
    const char *a = ini_path ? strrchr(ini_path, '/') : NULL;
    const char *b = ini_path ? strrchr(ini_path, '\\') : NULL;
    const char *sl = a > b ? a : b;
    size_t n = sl ? (size_t)(sl + 1 - ini_path) : 0;

    if (n >= sizeof g_mh_dir)
        n = 0;
    memcpy(g_mh_dir, ini_path ? ini_path : "", n);
    g_mh_dir[n] = 0;
    g_mh_loaded = -1;
}

static void mayhem_path(char *out, size_t size, const char *name)
{
    snprintf(out, size, "%s%s", g_mh_dir, name);
}

static MayhemBoard *mayhem_board(const char *key, int make)
{
    long i;

    for (i = 0; i < g_mh_boards; i++)
        if (strcmp(g_mh_board[i].key, key) == 0)
            return &g_mh_board[i];
    if (!make || g_mh_boards == MAYHEM_BOARDS)
        return NULL;
    memset(&g_mh_board[g_mh_boards], 0, sizeof g_mh_board[0]);
    snprintf(g_mh_board[g_mh_boards].key, sizeof g_mh_board[0].key, "%s", key);
    return &g_mh_board[g_mh_boards++];
}

/* the font draws ASCII; anything else in a name becomes '?' */
static void mayhem_name(char *out, size_t size, const char *in)
{
    size_t k = 0;

    for (; *in && k + 1 < size; in++)
        out[k++] = (*in >= 0x20 && *in < 0x7f && *in != '|') ? *in : '?';
    out[k] = 0;
}

static int mayhem_load(void)
{
    char path[600], line[256];
    MayhemBoard *bd = NULL;
    FILE *f;

    struct stat st;

    mayhem_path(path, sizeof path, "leaderboards.txt");
    if (stat(path, &st) != 0)
        memset(&st, 0, sizeof st);
    if (g_mh_loaded >= 0 && st.st_mtime == g_mh_st.st_mtime && st.st_size == g_mh_st.st_size)
        return g_mh_loaded;
    g_mh_st = st;
    g_mh_boards = 0;
    g_mh_user[0] = 0;
    f = fopen(path, "rb");
    g_mh_loaded = 0;
    if (!f)
        return 0;
    while (fgets(line, sizeof line, f)) {
        char *e = line + strlen(line), *bar;

        while (e > line && (e[-1] == '\n' || e[-1] == '\r'))
            *--e = 0;
        if (strncmp(line, "user=", 5) == 0) {
            mayhem_name(g_mh_user, sizeof g_mh_user, line + 5);
        } else if (strncmp(line, "board=", 6) == 0) {
            bd = mayhem_board(line + 6, 1);
        } else if (bd && (bar = strrchr(line, '|')) != NULL && bd->n < MAYHEM_ROWS) {
            *bar = 0;
            mayhem_name(bd->row[bd->n].name, sizeof bd->row[0].name, line);
            bd->row[bd->n].score = strtol(bar + 1, NULL, 10);
            bd->n++;
        }
    }
    fclose(f);
    g_mh_loaded = g_mh_user[0] != 0;
    return g_mh_loaded;
}

/* the player's own row, moved up if the new score beats it */
static void mayhem_place(MayhemBoard *bd, long score)
{
    long i, at = -1;
    MayhemRow me;

    for (i = 0; i < bd->n; i++)
        if (strcmp(bd->row[i].name, g_mh_user) == 0)
            at = i;
    if (at >= 0 && bd->row[at].score >= score)
        return;
    if (at < 0) {
        if (bd->n < MAYHEM_ROWS)
            at = bd->n++;
        else if (bd->row[MAYHEM_ROWS - 1].score < score)
            at = MAYHEM_ROWS - 1;
        else
            return;
    }
    snprintf(me.name, sizeof me.name, "%s", g_mh_user);
    me.score = score;
    while (at > 0 && bd->row[at - 1].score < score) {
        bd->row[at] = bd->row[at - 1];
        at--;
    }
    bd->row[at] = me;
}

int  EASOC_MayhemIsReady(void)                  { return 1; }
int  EASOC_MayhemIsPending(void)                { return 0; }
int  EASOC_MayhemNeedsUserName(void)            { return 0; }
void EASOC_MayhemReset(void)                    { }
void EASOC_MayhemTest(long a)                   { (void)a; }
void EASOC_MayhemSetUserName(const char *name)  { (void)name; }
void EASOC_MayhemGetUserStat(const char *stat)  { (void)stat; }

void EASOC_MayhemPostStatWithData(const char *key, long value,
                                  const long *data, long size)
{
    char path[600];
    MayhemBoard *bd;
    FILE *f;

    extern int PortDbgTainted;

    (void)data; (void)size;
    if (!key || value < 0 || !mayhem_load())
        return;
    if (PortDbgTainted) {
        printf("leaderboards: %s %ld not posted, the run had the debug menu's help\n", key, value);
        PortDbgTainted = 0;
        return;
    }
    mayhem_path(path, sizeof path, "scores.txt");
    f = fopen(path, "ab");
    if (f) {
        fprintf(f, "%s %ld\n", key, value);
        fclose(f);
    }
    bd = mayhem_board(key, 1);
    if (bd)
        mayhem_place(bd, value);
}

void EASOC_MayhemInitLeaderBoard(long board, const char *filter, long period,
                                 long count, const char *stat)
{ (void)board; (void)filter; (void)period; (void)count; g_mh_stat = stat; }

void EASOC_MayhemReloadLeaderBoard(long board, long period, long page,
                                   long count, const char *stat)
{ (void)board; (void)period; (void)page; (void)count; g_mh_stat = stat; }

/* One page: the callback once a row, ranks 1..count on the page, and the
 * page's row count back. The screen enables Next for a full page (any period
 * but 5), so a full LAST page answers count - 1 -- the rows are all drawn
 * already, it only keeps Next from opening an empty page. */
long EASOC_MayhemGetLeaderBoard(long page, long count, long period, void *cb)
{
    void (*draw)(int, int, const char *, int, int, int) =
        (void (*)(int, int, const char *, int, int, int))cb;
    MayhemBoard *bd;
    long i, first = page * count, rows;

    (void)period;
    if (!mayhem_load() || !g_mh_stat || page < 0 || count <= 0)
        return 0;
    bd = mayhem_board(g_mh_stat, 0);
    if (!bd || first >= bd->n)
        return 0;
    rows = bd->n - first < count ? bd->n - first : count;
    for (i = 0; i < rows && draw; i++)
        draw((int)(i + 1), (int)first, bd->row[first + i].name, 1, 0,
             (int)bd->row[first + i].score);
    return (rows == count && first + rows >= bd->n) ? count - 1 : rows;
}


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
