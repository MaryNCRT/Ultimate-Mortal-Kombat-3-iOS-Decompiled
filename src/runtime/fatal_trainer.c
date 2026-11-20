/*
 * fatal_trainer.c -- the FATALITY TRAINER (EXTRAS > FATALITY TRAINER), port
 * only. Mary's brief, after Legacy Kollection's trainer and the floor marker
 * of MKX and MK11.
 *
 * What it knows comes from the fight's own code and tables, nothing typed in:
 *
 *   - Which finishers a fighter has. A button reaches a per-character routine
 *     (the ochar_*_close / *_open tables, made from the .ipa into
 *     build/logic_tables.c); each routine hands a secret-move table (sm_*)
 *     to secret_move_search (logic/moves.c). fatal_trainer_map.h, written by
 *     tools/gen_finisher_map.py from the decompiled code, says which table
 *     each routine reaches. An entry of a table is 18 words: two button
 *     masks, the transfer, a predicate, the handler, the stick pattern; the
 *     handler says what it is (t_do_fatality_1, t_do_animality...).
 *
 *   - Where the fighter has to stand. The predicate is what decides: most
 *     call q_fatal_dist or q_animal_dist (moves.c, armv7 0x0005322c and
 *     0x00053178), which take |x(him) - x(me)| (get_x_dist, the halfword at
 *     0x0e of each fighter's object) and answer yes when
 *     min <= d <= max, the pair the predicate put in 0x30 / 0x34 -- and then
 *     only in the finishing window (q_fatality_req: G+0x45c == 3). Others
 *     test one bound, or a mercy first, or the stage. So the range is not
 *     read out of constants: the predicate is ASKED, on a copy of the
 *     fighter moved to every distance 0..FT_DMAX, and the distances it says
 *     yes to are the range. What is shown is exactly what the game accepts.
 *
 *   - Whether the game would take it now: the same predicate asked about
 *     the fighter where they stand. The stick and the buttons are the one
 *     thing left to the player.
 *
 * A copy is what DoASpecial (moves.c 0x51830) itself hands a predicate: an
 * object whose 0x00 and 0x08 are the real fighter's. The predicates write
 * only into it (0x1c..0x34, 0x5c).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "platform/gl.h"
#include "fatal_trainer.h"
#include "fatal_trainer_map.h"

extern void *G;                         /* the fight engine's state */
extern char *Plyr;                      /* the two fighters, 108 bytes apart */
extern int  CurrentTask;

/* the per-character button routines (build/logic_tables.c), with how many
 * characters each table has */
extern uint32_t ochar_hp_close[], ochar_lp_close[], ochar_block_close[],
                ochar_hk_close[], ochar_lk_close[], ochar_run_close[],
                ochar_up[], ochar_down[], ochar_left[], ochar_right[],
                ochar_hi_punch_open[], ochar_lo_punch_open[], ochar_block_open[],
                ochar_lo_kick_open[], ochar_hi_kick_open[], ochar_run_open[];
static const struct { const uint32_t *t; int n; } ft_buttons[] = {
    { ochar_hp_close, 24 }, { ochar_lp_close, 24 }, { ochar_block_close, 24 },
    { ochar_hk_close, 25 }, { ochar_lk_close, 24 }, { ochar_run_close, 25 },
    { ochar_up, 24 }, { ochar_down, 26 }, { ochar_left, 25 }, { ochar_right, 25 },
    { ochar_hi_punch_open, 25 }, { ochar_lo_punch_open, 25 }, { ochar_block_open, 25 },
    { ochar_lo_kick_open, 25 }, { ochar_hi_kick_open, 25 }, { ochar_run_open, 25 },
};

/* the finishers' handlers (the window-shut column of DoASpecial) */
long t_do_pit_fatality(void *), t_do_mercy(void *), t_do_fatality_1(void *),
     t_do_fatality_2(void *), t_do_animality(void *), t_do_baby(void *),
     t_do_friendship(void *);

extern uint32_t xfer_types_table[];     /* the transfers, (copy, fighter) */

/* the options (the trainer's screen, FrontEnd.c) */
int PortFT;                             /* a trainer fight is on, or asked for */
int PortFTP1 = 0x12, PortFTP2 = 0, PortFTStage = 0, PortFTMove = 0;
int PortFTUnlimited = 1, PortFTDistance = 1, PortFTOneButton = 0, PortFTInput = 1;

static FT_LIVE g_live;

static int kind_of(uint32_t h)
{
    if (h == (uint32_t)(uintptr_t)t_do_fatality_1)  return FT_FATALITY1;
    if (h == (uint32_t)(uintptr_t)t_do_fatality_2)  return FT_FATALITY2;
    if (h == (uint32_t)(uintptr_t)t_do_pit_fatality) return FT_STAGE;
    if (h == (uint32_t)(uintptr_t)t_do_animality)   return FT_ANIMALITY;
    if (h == (uint32_t)(uintptr_t)t_do_baby)        return FT_BABALITY;
    if (h == (uint32_t)(uintptr_t)t_do_friendship)  return FT_FRIENDSHIP;
    if (h == (uint32_t)(uintptr_t)t_do_mercy)       return FT_MERCY;
    return -1;
}

static const uint32_t *map_table(uint32_t fn, int k)
{
    size_t i;

    for (i = 0; i < sizeof ft_map / sizeof ft_map[0]; i++)
        if ((uint32_t)(uintptr_t)ft_map[i].fn == fn)
            return k < FT_MAP_TABLES ? ft_map[i].tab[k] : NULL;
    return NULL;
}

int ft_scan(int ch, FT_MOVE *out, int max)
{
    int n = 0, b, k;

    if (ch < 0 || ch >= 26)
        return 0;
    for (b = 0; b < (int)(sizeof ft_buttons / sizeof ft_buttons[0]); b++) {
        uint32_t fn;

        if (ch >= ft_buttons[b].n || !(fn = ft_buttons[b].t[ch]))
            continue;
        for (k = 0; k < FT_MAP_TABLES; k++) {
            const uint32_t *t = map_table(fn, k), *e;

            if (!t)
                break;
            for (e = t; e[0] != 0; e += 18) {
                int kind = kind_of(e[4]);
                int j, dup = 0;

                if (kind < 0)
                    continue;
                for (j = 0; j < n; j++)
                    if (out[j].entry == e)
                        dup = 1;
                if (dup || n >= max)
                    continue;
                out[n].kind = kind;
                out[n].button = b;
                out[n].entry = e;
                n++;
            }
        }
    }
    /* in the order the moves list writes them */
    {
        int i, j;
        for (i = 1; i < n; i++)
            for (j = i; j > 0 && out[j].kind < out[j - 1].kind; j--) {
                FT_MOVE t = out[j];
                out[j] = out[j - 1];
                out[j - 1] = t;
            }
    }
    return n;
}

/* ------------------------------------------------------ asking the game */

#define OBJ_SIZE 0x6c                   /* MK3OBJ, a fighter's slot in Plyr */
static uint8_t g_copy[OBJ_SIZE], g_part[0x80];

static uint32_t rd32(const void *p, int off) { return *(const uint32_t *)((const char *)p + off); }

/* the predicate of `e` asked about the fighter standing `d` from him on the
 * side `side` (-1 left of him, 1 right), or where they are (d < 0): 1 yes.
 * *lo / *hi: what it put in 0x30 / 0x34 (left alone: -1). */
static int ask(const uint32_t *e, int d, int side, int *lo, int *hi)
{
    char *me = Plyr;
    char *part = (char *)(uintptr_t)rd32(me, 0x08);
    void (*pred)(void *) = (void (*)(void *))(uintptr_t)e[3];

    if (!pred)
        return 1;                       /* no condition of its own */
    memset(g_copy, 0, sizeof g_copy);
    *(uint32_t *)(g_copy + 0x00) = rd32(me, 0x00);
    if (d >= 0) {
        int hx = (int16_t)(rd32((const char *)(uintptr_t)rd32((const char *)(uintptr_t)rd32(me, 0), 4), 0x0c) >> 16);
        int x = hx + side * d;
        memcpy(g_part, part, sizeof g_part);
        *(uint32_t *)(g_part + 0x0c) = (rd32(part, 0x0c) & 0xffffu) | ((uint32_t)(uint16_t)x << 16);
        *(uint32_t *)(g_copy + 0x08) = (uint32_t)(uintptr_t)g_part;
    } else
        *(uint32_t *)(g_copy + 0x08) = (uint32_t)(uintptr_t)part;
    *(uint32_t *)(g_copy + 0x30) = 0xffffffffu;
    *(uint32_t *)(g_copy + 0x34) = 0xffffffffu;
    pred(g_copy);
    if (lo)
        *lo = (int)rd32(g_copy, 0x30);
    if (hi)
        *hi = (int)rd32(g_copy, 0x34);
    return rd32(g_copy, 0x5c) != 0;
}

static int fighters(char **me_part, char **him_part)
{
    if (!G || !Plyr || CurrentTask != 6)
        return 0;
    *me_part = (char *)(uintptr_t)rd32(Plyr, 0x08);
    *him_part = (char *)(uintptr_t)rd32((const char *)(uintptr_t)rd32(Plyr, 0), 4);
    return *me_part && *him_part;
}

void ft_tick(const FT_MOVE *m)
{
    char *mp, *hp;
    int mx, hx, d, lo, hi, side, inrun = 0;

    memset(&g_live, 0, sizeof g_live);
    if (!m || !fighters(&mp, &hp))
        return;
    mx = (int16_t)(rd32(mp, 0x0c) >> 16);
    hx = (int16_t)(rd32(hp, 0x0c) >> 16);
    g_live.on = 1;
    g_live.me_x = mx;
    g_live.him_x = hx;
    g_live.d = mx > hx ? mx - hx : hx - mx;
    side = mx < hx ? -1 : 1;
    g_live.side = side;
    g_live.lo = g_live.hi = -1;
    g_live.bound_lo = g_live.bound_hi = -1;
    /* every distance, the runs of yes */
    for (d = 0; d <= FT_DMAX; d++) {
        int y = ask(m->entry, d, side, &lo, &hi);
        if (lo != -1 && hi != -1 && g_live.bound_lo < 0) {
            g_live.bound_lo = lo;       /* the band it measures against */
            g_live.bound_hi = hi;
        }
        if (y && !inrun) {
            inrun = 1;
            if (g_live.lo < 0)
                g_live.lo = d;
        }
        if (!y && inrun) {
            inrun = 0;
            if (g_live.hi < 0)
                g_live.hi = d - 1;
        }
    }
    if (inrun && g_live.hi < 0)
        g_live.hi = FT_DMAX;            /* no upper bound */
    g_live.any = g_live.lo == 0 && g_live.hi == FT_DMAX;
    g_live.now = ask(m->entry, -1, side, NULL, NULL);
    g_live.in = g_live.lo >= 0 && g_live.d >= g_live.lo && g_live.d <= g_live.hi;
    g_live.window = *(int16_t *)((char *)G + 0x45c) == 3;
    g_live.mercy = *(uint16_t *)((char *)G + 0x45a) != 0;
}

const FT_LIVE *ft_live(void) { return &g_live; }

/* ONE-BUTTON FATALITY: the entry's own path, as secret_move_search takes it
 * on a match (moves.c) -- its predicate, then the handler into the copy's
 * 0x38 and its transfer with (copy, fighter), the convention DoASpecial
 * uses -- with only the stick pattern skipped. A refusal does nothing. */
int ft_fire(const FT_MOVE *m)
{
    char *me = Plyr;
    void (*xfer)(void *, void *);

    if (!m || !G || !Plyr || CurrentTask != 6)
        return 0;
    if (!ask(m->entry, -1, 1, NULL, NULL))
        return 0;
    *(uint32_t *)(g_copy + 0x38) = m->entry[4];
    xfer = (void (*)(void *, void *))(uintptr_t)xfer_types_table[m->entry[2]];
    if (!xfer)
        return 0;
    xfer(g_copy, me);
    return 1;
}

/* --------------------------------------------------- the floor's marker */

/* PlayerDefs' entry as GameCode.c lays it out (game_main.c hb_def) */
struct ft_def {
    long id;
    float scale, posOffsetX, height, renderOffsetX, renderOffsetZ;
    uint32_t names[7];
};
extern const struct ft_def PlayerDefs[26];

/* The range as a frame on the floor, the stage's 3D: where the fighter has
 * to stand, from him, on the side they are on -- in the arcade's units, 64
 * to a world unit, the model's own offset added as the fight draws it
 * (ArcadePosTo3dPos; game_main.c hb_offset). Green inside, red outside,
 * white while the game would not take it (the window shut, a condition
 * unmet). Called from the scene's end (PortAfterScene). */
void ft_draw3d(void)
{
    extern unsigned char PlayerZPos[4];
    static const GLenum arrays[3] = { GL_TEXTURE_COORD_ARRAY, GL_COLOR_ARRAY, GL_NORMAL_ARRAY };
    GLint was[3], vert;
    float depth, off = 0.0f, x0, x1, z = 0.02f, r, g, b, v[12];
    char *mp, *hp;
    uint32_t c;
    int i, a, z1;

    if (!PortFT || !PortFTDistance || !g_live.on || g_live.lo < 0 || !fighters(&mp, &hp))
        return;
    memcpy(&depth, PlayerZPos, 4);
    c = rd32(mp, 0x24);
    if (c < 26)
        off = (rd32(mp, 0x28) & 0x10) ? -PlayerDefs[c].posOffsetX : PlayerDefs[c].posOffsetX;
    a = g_live.him_x + g_live.side * g_live.lo;
    z1 = g_live.him_x + g_live.side * (g_live.hi > 400 ? 400 : g_live.hi);
    x0 = (float)(a < z1 ? a : z1) / 64.0f + off;
    x1 = (float)(a < z1 ? z1 : a) / 64.0f + off;
    if (x1 - x0 < 0.08f) {              /* one distance only: still a frame */
        x0 -= 0.04f;
        x1 += 0.04f;
    }
    if (!g_live.now && !g_live.in) { r = 1.0f; g = 0.18f; b = 0.12f; }
    else if (!g_live.now)          { r = 0.9f; g = 0.9f;  b = 0.85f; }
    else                           { r = 0.25f; g = 1.0f; b = 0.3f; }

    glPushAttrib(GL_ALL_ATTRIB_BITS);
    for (i = 0; i < 3; i++) {
        glGetIntegerv(arrays[i], &was[i]);
        glDisableClientState(arrays[i]);
    }
    glGetIntegerv(GL_VERTEX_ARRAY, &vert);
    glEnableClientState(GL_VERTEX_ARRAY);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glDisable(GL_FOG);
    glDisable(GL_CULL_FACE);
    glDisable(GL_ALPHA_TEST);
    glEnable(GL_DEPTH_TEST);            /* on the floor, under the feet */
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);  /* a glow, as the consoles' marker */

    /* the frame: x across, depth into the screen around the fighters' line,
     * on the floor (z 0, a hair over it) */
    {
        float d0 = depth - 0.55f, d1 = depth + 0.55f;
        v[0] = x0; v[1] = d0; v[2] = z;
        v[3] = x1; v[4] = d0; v[5] = z;
        v[6] = x1; v[7] = d1; v[8] = z;
        v[9] = x0; v[10] = d1; v[11] = z;
        glVertexPointer(3, GL_FLOAT, 0, v);
        glColor4f(r, g, b, 0.10f);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        for (i = 0; i < 3; i++) {       /* a thick bright line, three strokes */
            float k = 0.012f * (float)i;
            v[0] = x0 - k; v[1] = d0 - k;
            v[3] = x1 + k; v[4] = d0 - k;
            v[6] = x1 + k; v[7] = d1 + k;
            v[9] = x0 - k; v[10] = d1 + k;
            glLineWidth(3.0f);
            glColor4f(r, g, b, i ? 0.35f : 0.95f);
            glDrawArrays(GL_LINE_LOOP, 0, 4);
        }
    }

    for (i = 0; i < 3; i++)
        if (was[i])
            glEnableClientState(arrays[i]);
    if (!vert)
        glDisableClientState(GL_VERTEX_ARRAY);
    glDepthMask(GL_TRUE);
    glPopAttrib();
}

/* ------------------------------------------- the fighter's finishers, kept */

extern umk3_long PLAYER1MODEL;
int PortFTGo;                           /* FIGHT! pressed: game_main starts it */

static FT_MOVE g_moves[FT_MOVES];
static int g_nmoves, g_moves_of = -1;

const FT_MOVE *ft_moves_of(int ch, int *n)
{
    if (ch != g_moves_of) {
        g_nmoves = ft_scan(ch, g_moves, FT_MOVES);
        g_moves_of = ch;
    }
    *n = g_nmoves;
    return g_moves;
}

/* the move chosen on the trainer's screen, for the fighter in the fight */
const FT_MOVE *ft_selected(void)
{
    int n;
    const FT_MOVE *m = ft_moves_of((int)PLAYER1MODEL, &n);

    if (n <= 0)
        return NULL;
    if (PortFTMove < 0 || PortFTMove >= n)
        PortFTMove = 0;
    return &m[PortFTMove];
}
