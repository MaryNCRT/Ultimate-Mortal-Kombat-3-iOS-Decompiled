/* pad_nav.c -- the game's own menus with a pad. Port, not in the binary.
 *
 * The front end is made for a finger: every screen draws its buttons and
 * tests the touch against them in the same pass. Here those tests also say
 * where their buttons are (PortNavTarget, called from TouchAreaWH,
 * DrawButtonNew, BasicMenuMod, DrawOptionAsButton, the select screen's
 * portraits and the colour boxes in FrontEnd.c), and with that list:
 *
 *   - the d-pad or the left stick moves a highlight to the nearest button
 *     that way (held, it repeats);
 *   - the button under the highlight lights as it does under a resting
 *     finger (the touch is held there, never pressed nor lifted); what
 *     does not light so (the select screen's portraits) gets a frame;
 *   - the south button (A / cross) puts the finger down in its middle and
 *     lifts it when let go, so the button answers on the release, as it
 *     does to a finger;
 *   - the east button (B / circle) taps the screen's BACK, if it has one,
 *     or a page's own main menu entry, which closes it.
 *
 * A page that opens beside the main menu keeps the highlight on its own
 * buttons: the main menu's entries are its tabs, reached with BACK.
 *
 * The keyboard does the same: the arrows (or the movement keys) move,
 * Enter taps, Backspace is BACK.
 *
 * On the two-player select screen each player has a highlight: player one
 * the first pad and the keyboard, player two the second pad and player
 * two's keys (a punch accepts, block is BACK), and both choose at once.
 *
 * Nothing is moved like a mouse and the screens are not touched otherwise.
 * The highlight shows only while the pad is the last thing used; a click
 * hides it. On the front end and in the pause menu only -- in a fight the
 * pad is the fighter's. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "platform/platform.h"
#include "pad_nav.h"

#define MAX_T 128

typedef struct { float x, y, w, h; int kind; } NavT;

/* One player's highlight. */
typedef struct {
    float fx, fy;                       /* its middle, -1 for none yet */
    float hx, hy, hw, hh;               /* its box, last frame */
    int   kind;
    int   show;                         /* the pad or keys were used last */
    int   dev;                          /* 1 a pad, 2 the keyboard */
    unsigned was;
    int   kok_was, kback_was;
    int   dir_was, hold;
    int   held;                         /* south / east / Enter / Backspace down */
} NavC;

static NavT  g_t[MAX_T];                /* this frame's buttons */
static int   g_n;
static int   g_on;                      /* a screen the pad drives */
static int   g_dual;                    /* both players choose at once */
static NavC  g_c[2] = { { -1.0f, -1.0f }, { -1.0f, -1.0f } };
static NavT  g_tb;                     /* the page's own tab: its BACK */
static int   g_has_tb;

void PortNavTarget(float x, float y, float w, float h, int kind)
{
    int i;

    if (!g_on || w < 2.0f || h < 2.0f || g_n >= MAX_T)
        return;
    if (kind == NAV_TABBACK) {          /* not a stop, only east's */
        g_tb.x = x;
        g_tb.y = y;
        g_tb.w = w;
        g_tb.h = h;
        g_has_tb = 1;
        return;
    }
    for (i = 0; i < g_n; i++)           /* drawn twice in a frame: once */
        if (fabsf(g_t[i].x - x) < 1.0f && fabsf(g_t[i].y - y) < 1.0f
            && fabsf(g_t[i].w - w) < 1.0f && fabsf(g_t[i].h - h) < 1.0f)
            return;
    g_t[g_n].x = x;
    g_t[g_n].y = y;
    g_t[g_n].w = w;
    g_t[g_n].h = h;
    g_t[g_n].kind = kind;
    g_n++;
}

void PortNavDual(void)
{
    g_dual = 1;
}

void pad_nav_begin(int on)
{
    g_on = on;
    g_n = 0;
    g_has_tb = 0;
    g_dual = 0;
}

void pad_nav_hide(void)
{
    g_c[0].show = g_c[1].show = 0;
}

/* A page's tabs go when it has buttons of its own. */
static void drop_tabs(void)
{
    int i, j, own = 0;

    for (i = 0; i < g_n; i++)
        if (g_t[i].kind != NAV_TAB)
            own = 1;
    if (!own)
        return;
    for (i = j = 0; i < g_n; i++)
        if (g_t[i].kind != NAV_TAB)
            g_t[j++] = g_t[i];
    g_n = j;
}

/* The button the highlight was on: the one of its size nearest where it
 * was (a sliding panel moves it a little); -1 when the screen has changed
 * under it. */
static int same(const NavC *c)
{
    int i, best = -1;
    float bd = 1e30f;

    if (c->fx < 0.0f)
        return -1;
    for (i = 0; i < g_n; i++) {
        float cx = g_t[i].x + g_t[i].w * 0.5f, cy = g_t[i].y + g_t[i].h * 0.5f;
        float d = fabsf(cx - c->fx) + fabsf(cy - c->fy);

        if (fabsf(g_t[i].w - c->hw) > c->hw * 0.25f + 2.0f
            || fabsf(g_t[i].h - c->hh) > c->hh * 0.25f + 2.0f)
            continue;
        if (d < bd) {
            bd = d;
            best = i;
        }
    }
    return bd < 24.0f ? best : -1;
}

/* The nearest button from `from` in direction (dx, dy): along that way
 * counts once, sideways three times. -1 when there is none. */
static int step(int from, int dx, int dy)
{
    float fx = g_t[from].x + g_t[from].w * 0.5f, fy = g_t[from].y + g_t[from].h * 0.5f;
    float bd = 1e30f;
    int i, best = -1;

    for (i = 0; i < g_n; i++) {
        float cx = g_t[i].x + g_t[i].w * 0.5f, cy = g_t[i].y + g_t[i].h * 0.5f;
        float along = (cx - fx) * dx + (cy - fy) * dy;
        float side = fabsf((cx - fx) * dy) + fabsf((cy - fy) * dx);
        float d;

        if (i == from || along <= 1.0f)
            continue;
        d = along + side * 3.0f;
        if (d < bd) {
            bd = d;
            best = i;
        }
    }
    return best;
}

/* The first button a screen offers: the top-left one that is not BACK --
 * the top-right one for player two. */
static int first(int who)
{
    int i, best = -1;
    float bd = 1e30f;

    for (i = 0; i < g_n; i++) {
        float d = g_t[i].y * 4.0f + (who ? -g_t[i].x : g_t[i].x);

        if (g_t[i].kind == NAV_BACK)
            continue;
        if (d < bd) {
            bd = d;
            best = i;
        }
    }
    return best >= 0 ? best : (g_n ? 0 : -1);
}

/* One player's frame: moves the highlight, 1 with a tap's place when one is
 * due. */
static int run(NavC *c, int who, const PLAT_PAD *pa, const PLAT_PAD *pb,
               const struct nav_keys *k, float *tx, float *ty)
{
    unsigned now = (pa && pa->connected ? pa->buttons : 0)
                 | (pb && pb->connected ? pb->buttons : 0);
    unsigned hit = now & ~c->was;
    int up = (pa && pa->up) || (pb && pb->up);
    int down = (pa && pa->down) || (pb && pb->down);
    int left = (pa && pa->left) || (pb && pb->left);
    int right = (pa && pa->right) || (pb && pb->right);
    int dir = up ? 1 : down ? 2 : left ? 3 : right ? 4 : 0;
    int cur, i;
    int kok = k && k->ok && !c->kok_was, kback = k && k->back && !c->kback_was;

    if (dir || hit)
        c->dev = 1;
    c->kok_was = k && k->ok;
    c->kback_was = k && k->back;
    c->held = (now & ((1u << PB_SOUTH) | (1u << PB_EAST))) || (k && (k->ok || k->back));
    if (!dir && k && k->dir) {
        dir = k->dir;
        c->dev = 2;
    }
    if (kok || kback)
        c->dev = 2;
    if (kok)
        hit |= 1u << PB_SOUTH;          /* Enter is the south button */
    if (kback)
        hit |= 1u << PB_EAST;           /* Backspace the east */
    c->was = now;
    if (!g_on || g_n == 0) {
        c->dir_was = dir;
        return 0;
    }
    if (hit || dir)
        c->show = 1;

    cur = same(c);
    if (cur < 0)
        cur = first(who);

    /* a direction moves once, then again every 8 frames while held, after
     * 24 */
    if (dir && (dir != c->dir_was || (++c->hold >= 24 && c->hold % 8 == 0))) {
        int n;

        if (dir != c->dir_was)
            c->hold = 0;
        n = step(cur, dir == 3 ? -1 : dir == 4 ? 1 : 0, dir == 1 ? -1 : dir == 2 ? 1 : 0);
        if (n >= 0)
            cur = n;
    }
    if (!dir)
        c->hold = 0;
    c->dir_was = dir;

    c->fx = g_t[cur].x + g_t[cur].w * 0.5f;
    c->fy = g_t[cur].y + g_t[cur].h * 0.5f;
    c->hx = g_t[cur].x;
    c->hy = g_t[cur].y;
    c->hw = g_t[cur].w;
    c->hh = g_t[cur].h;
    c->kind = g_t[cur].kind;

    if (getenv("UMK3_LOG_NAV") && (hit || dir))
        printf("nav %d: %d targets, focus %.0f,%.0f %.0fx%.0f show %d\n", who,
               g_n, c->hx, c->hy, c->hw, c->hh, c->show);
    if (hit & (1u << PB_SOUTH)) {       /* the highlighted one */
        *tx = c->fx;
        *ty = c->fy;
        return 1;
    }
    if (hit & (1u << PB_EAST)) {        /* the screen's BACK */
        for (i = 0; i < g_n; i++)
            if (g_t[i].kind == NAV_BACK) {
                *tx = g_t[i].x + g_t[i].w * 0.5f;
                *ty = g_t[i].y + g_t[i].h * 0.5f;
                return 1;
            }
        if (g_has_tb) {                 /* or the page's own tab again */
            *tx = g_tb.x + g_tb.w * 0.5f;
            *ty = g_tb.y + g_tb.h * 0.5f;
            c->fx = *tx;                /* and the highlight lands on it */
            c->fy = *ty;
            c->hw = g_tb.w;
            c->hh = g_tb.h;
            return 1;
        }
    }
    return 0;
}

int pad_nav_end(const PLAT_PAD *p1, const PLAT_PAD *p2,
                const struct nav_keys *k1, const struct nav_keys *k2,
                float *tx, float *ty, int *who)
{
    float x2, y2;
    int t1, t2;

    drop_tabs();
    if (!g_dual) {                      /* one highlight, every input */
        struct nav_keys k = *k1;

        if (!k.dir)
            k.dir = k2->dir;
        k.ok |= k2->ok;
        k.back |= k2->back;
        g_c[1].show = 0;
        *who = 1;
        return run(&g_c[0], 0, p1, p2, &k, tx, ty);
    }
    t1 = run(&g_c[0], 0, p1, NULL, k1, tx, ty);
    t2 = run(&g_c[1], 1, p2, NULL, k2, &x2, &y2);
    *who = 1;
    if (!t1 && t2) {
        *tx = x2;
        *ty = y2;
        *who = 2;
    }
    return t1 || t2;
}

int pad_nav_held(int who)
{
    return g_c[who == 2 ? 1 : 0].held;
}

int pad_nav_hover(float *x, float *y)
{
    if (!g_on || !g_c[0].show || g_c[0].fx < 0.0f)
        return 0;
    *x = g_c[0].fx;
    *y = g_c[0].fy;
    return 1;
}

int pad_nav_focus(int who, float *x, float *y, float *w, float *h)
{
    const NavC *c = &g_c[who == 2 ? 1 : 0];

    if (!g_on || !c->show || g_n == 0 || c->fx < 0.0f || (who == 2 && !g_dual))
        return 0;
    if (c->kind != NAV_FRAME)
        return 0;
    *x = c->hx;
    *y = c->hy;
    *w = c->hw;
    *h = c->hh;
    return 1;
}

int pad_nav_back(float *x, float *y, float *w, float *h)
{
    int i;

    if (!g_on)
        return 0;
    for (i = 0; i < g_n; i++)
        if (g_t[i].kind == NAV_BACK) {
            *x = g_t[i].x;
            *y = g_t[i].y;
            *w = g_t[i].w;
            *h = g_t[i].h;
            return 1;
        }
    return 0;
}

int pad_nav_ok(float *x, float *y, float *w, float *h)
{
    int i;

    if (!g_on)
        return 0;
    for (i = 0; i < g_n; i++)
        if (g_t[i].kind == NAV_OK) {
            *x = g_t[i].x;
            *y = g_t[i].y;
            *w = g_t[i].w;
            *h = g_t[i].h;
            return 1;
        }
    return 0;
}

int pad_nav_on(void)
{
    return g_on && g_n > 0;
}

int pad_nav_device(void)
{
    return g_c[0].dev ? g_c[0].dev : 2;
}
