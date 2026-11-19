/*
 * adaptive_music.c -- port feature, not in the binary: dynamic stage music.
 *
 * Two things, both on with umk3.ini's dynamic_music=1 (the default), both
 * off with 0 -- then the music is exactly the iOS game's:
 *
 * 1. Sections (docs/research/DYNAMIC-STAGE-MUSIC.md, design A + D): the
 *    stage tune plays as on iOS; at FINISH HIM/HER it jumps to its
 *    "critical" section and repeats it; when the winner is named (a finish
 *    failed or timed out) it plays its "ending" section once and falls
 *    silent. A finisher's own tune (Fatal1/2, Animality, Friendship,
 *    Babality) still cuts in, as on iOS, and the ending section follows it.
 *    Mercy keeps iOS's behaviour (its banner restarts the stage tune). The
 *    next round starts the tune again from its beginning. The sections are
 *    the iOS tunes' own: measured from each file's end, as Mary's proposal
 *    for the Soul Chamber was (critical 13 s to 6 s before the end, ending
 *    the last 6 s), and changed per tune in music.ini beside the game.
 *
 * 2. Stage changes: in the stages with two scenes (level 4 the Subway under
 *    the Street, 10 the Soul Chamber under the Balcony, 11 Scorpion's Lair
 *    under Kahn's Kave) an uppercut through the ceiling moves the fight to
 *    the scene above (CurrentScene 1 -> 0, MaintainLevelScenes). The tune
 *    becomes the one that scene has as a stage of its own: the level whose
 *    first scene is the same file and that has no second one (the Street is
 *    level 5, Old.mp3). Found in LEVEL_INFO, not written here.
 *
 * Fed once a frame from AddNewGameEvents (Blood.c), before its early
 * return. It reads the fight's state (FinishHimHer, IsInFinishing,
 * WinnerMessage, CurrentScene), which rollback restores, and does nothing
 * while a frame is run again (rb_quiet), so an online fight hears the same
 * as the frames it keeps. Nothing here writes game state.
 */

#include "savestate.h"
#include "platform/platform.h"
#include "render/rgl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* game globals (gamecode_globals.c) */
extern umk3_long LevelSelect;
extern umk3_long CurrentScene;
extern umk3_long FinishHimHer;
extern umk3_long IsInFinishing;
extern umk3_long MercyMessage;
extern int CurrentTask;
extern int Settings[10];
extern float MusicVol[];
extern const char *LevelMusic[16];
extern unsigned char WinnerMessage[];
extern char Level_Info[];

#define LEVEL_INFO_STRIDE 0xf4
#define N_LEVELS 15                     /* LevelMusic[15] is the main menu's */

/* lime_menu.c */
void limePlayTune(const char *name, long vol, long loop);
const char *limeTuneName(void);
unsigned limeTuneSerial(void);
int rb_quiet(void);

int port_dynamic_music = 1;             /* umk3.ini dynamic_music= */

static int ci_cmp(const char *a, const char *b)
{
    for (; *a && *b; a++, b++) {
        int x = (*a >= 'a' && *a <= 'z') ? *a - 32 : *a;
        int y = (*b >= 'a' && *b <= 'z') ? *b - 32 : *b;
        if (x != y)
            return x - y;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

/* ----------------------------------------------------------- the sections */

typedef struct {
    char   tune[40];
    double crit_from, crit_to;          /* negative: back from the end */
    double end_from, end_to;            /* 0 as the end: the file's end */
} music_sec;

#define MAX_SEC 40
static music_sec g_sec[MAX_SEC];
static int       g_nsec, g_loaded;
static music_sec g_default = { "", -13.0, -6.0, -6.0, 0.0 };

static int read_pair(const char *v, double *a, double *b)
{
    char *e;
    double x = strtod(v, &e);
    if (e == v)
        return 0;
    while (*e == ' ' || *e == ',' || *e == '\t')
        e++;
    *a = x;
    *b = strtod(e, NULL);
    return 1;
}

/* music.ini beside the game: [default] or [<tune>.mp3], then
 * critical=<from>,<to> and ending=<from>,<to> in seconds (negative: back
 * from the file's end; an end of 0: the file's end). critical=off turns a
 * tune's sections off. */
static void load_ini(void)
{
    char path[1100], line[256];
    music_sec *cur = &g_default;
    FILE *f;

    g_loaded = 1;
    snprintf(path, sizeof path, "%smusic.ini", rgl_dir);
    f = fopen(path, "r");
    if (!f) {
        printf("music: no music.ini, sections %.0f..%.0f s and %.0f..end from each tune's end\n",
               g_default.crit_from, g_default.crit_to, g_default.end_from);
        return;
    }
    while (fgets(line, sizeof line, f)) {
        char *s = line, *e, *v;

        while (*s == ' ' || *s == '\t') s++;
        for (e = s + strlen(s); e > s && (unsigned char)e[-1] <= 32; )
            *--e = 0;
        if (*s == ';' || *s == '#' || !*s)
            continue;
        if (*s == '[') {
            if ((e = strchr(s, ']')) != NULL)
                *e = 0;
            s++;
            if (strcmp(s, "default") == 0)
                cur = &g_default;
            else if (g_nsec < MAX_SEC) {
                cur = &g_sec[g_nsec++];
                *cur = g_default;
                snprintf(cur->tune, sizeof cur->tune, "%s", s);
            }
            continue;
        }
        if ((v = strchr(s, '=')) == NULL)
            continue;
        *v++ = 0;
        for (e = v - 1; e > s && (unsigned char)e[-1] <= 32; ) *--e = 0;
        while (*v == ' ') v++;
        if (strcmp(s, "critical") == 0) {
            if (strcmp(v, "off") == 0)
                cur->crit_from = cur->crit_to = 1e9;
            else
                read_pair(v, &cur->crit_from, &cur->crit_to);
        } else if (strcmp(s, "ending") == 0)
            read_pair(v, &cur->end_from, &cur->end_to);
    }
    fclose(f);
    printf("music: music.ini, %d tunes of their own\n", g_nsec);
}

static const music_sec *sections(const char *tune)
{
    int i;
    if (!g_loaded)
        load_ini();
    for (i = 0; i < g_nsec; i++)
        if (ci_cmp(g_sec[i].tune, tune) == 0)
            return &g_sec[i];
    return &g_default;
}

/* ------------------------------------------------------- the scene's tune */

static const char *scene_name(int level, int which)
{
    if (level < 0 || level >= N_LEVELS)
        return NULL;
    return *(const char *P32 const *)(Level_Info + level * LEVEL_INFO_STRIDE
                                      + (which ? 0x50 : 0x1c));
}

/* The tune for where the fight is now: LevelMusic[LevelSelect], or in the
 * scene above a two-scene stage the tune of the level that is that scene. */
static const char *scene_tune(void)
{
    int lvl = (int)LevelSelect, j;
    const char *top, *low;

    if (lvl < 0 || lvl >= N_LEVELS)
        return NULL;
    top = scene_name(lvl, 0);
    low = scene_name(lvl, 1);
    if (!top || !low || CurrentScene != 0)
        return LevelMusic[lvl];
    for (j = 0; j < N_LEVELS; j++) {
        const char *t = scene_name(j, 0);
        if (j != lvl && t && !scene_name(j, 1) && ci_cmp(t, top) == 0)
            return LevelMusic[j];
    }
    return LevelMusic[lvl];
}

/* ---------------------------------------------------------- the controller */

enum { ST_NORMAL, ST_CRITICAL, ST_FINISHER, ST_ENDING, ST_DONE };
static const char *const k_state[] = { "normal", "critical", "finisher", "ending", "done" };

static int         g_state;
static unsigned    g_serial;            /* limeTuneSerial() after our own play */
static char        g_stage[40];         /* the stage tune we play */

static void set_state(int st, const char *why)
{
    if (st != g_state)
        printf("music: %s -> %s (%s)\n", k_state[g_state], k_state[st], why);
    g_state = st;
}

static int is_stage_tune(const char *name)
{
    int i;
    if (!name)
        return 0;
    for (i = 0; i < N_LEVELS; i++)
        if (LevelMusic[i] && ci_cmp(LevelMusic[i], name) == 0)
            return 1;
    return 0;
}

static void play_stage(const char *tune)
{
    limePlayTune(tune, (long)MusicVol[Settings[2]], 1);
    snprintf(g_stage, sizeof g_stage, "%s", tune);
    g_serial = limeTuneSerial();
}

static void play_section(int ending)
{
    const music_sec *s = sections(g_stage);
    double from = ending ? s->end_from : s->crit_from;
    double to   = ending ? s->end_to   : s->crit_to;

    printf("music: %s, %s section %.2f..%.2f s%s\n", g_stage,
           ending ? "ending" : "critical", from, to,
           from < 0 ? " (back from the end)" : "");
    plat_music_section(from, to, !ending);
}

void adaptive_music_tick(void)
{
    const char *want, *now;
    int winner, game_played;

    if (!port_dynamic_music || rb_quiet() || CurrentTask != 6)
        return;
    if (Settings[2] == 0)               /* the game's music is off */
        return;

    now  = limeTuneName();
    want = scene_tune();
    if (!want || !now)
        return;

    /* a tune the game played itself: a fight's start, Mercy's restart, a
     * finisher's tune */
    game_played = limeTuneSerial() != g_serial;
    if (game_played) {
        g_serial = limeTuneSerial();
        if (is_stage_tune(now)) {
            int lvl = (int)LevelSelect;
            snprintf(g_stage, sizeof g_stage, "%s", now);
            set_state(ST_NORMAL, "the game started the stage tune");
            if (lvl >= 0 && lvl < N_LEVELS && scene_name(lvl, 0) && scene_name(lvl, 1)) {
                umk3_long was = CurrentScene;
                CurrentScene = 0;       /* asked, then put back: no frame between */
                printf("music: level %d has a scene above (%s), its tune %s\n",
                       lvl, scene_name(lvl, 0), scene_tune());
                CurrentScene = was;
            }
        } else if (g_state == ST_CRITICAL || g_state == ST_NORMAL) {
            set_state(ST_FINISHER, now);
        }
    }

    winner = WinnerMessage[0] || WinnerMessage[1];

    switch (g_state) {
    case ST_NORMAL:
        if (!is_stage_tune(now))
            break;
        if (ci_cmp(want, g_stage) != 0) {
            printf("music: the fight moved to another scene, %s -> %s\n",
                   g_stage, want);
            play_stage(want);
        }
        if (FinishHimHer && IsInFinishing && !winner) {
            if (sections(g_stage)->crit_from >= 1e8)
                break;                  /* critical=off for this tune */
            play_section(0);
            set_state(ST_CRITICAL, FinishHimHer == 2 ? "FINISH HER" : "FINISH HIM");
        }
        break;

    case ST_CRITICAL:
        if (winner) {
            play_section(1);
            set_state(ST_ENDING, "the winner is named");
        }
        break;

    case ST_FINISHER:
        /* the finisher's tune is over; Mercy's banner restarts the stage
         * tune itself (GameCode.c), so not while it is up */
        if (!plat_music_playing() && !MercyMessage) {
            play_stage(g_stage[0] ? g_stage : want);
            play_section(1);
            set_state(ST_ENDING, "after the finisher's tune");
        }
        break;

    case ST_ENDING:
        if (!plat_music_playing())
            set_state(ST_DONE, "the ending section played out");
        /* fall through: a new round */
    case ST_DONE:
        if (!FinishHimHer && !winner) { /* ResetFightData: the next round */
            play_stage(want);
            set_state(ST_NORMAL, "a new round, the tune from its start");
        }
        break;
    }
}
