/*
 * debug_menu.c -- the in-game debug menu (F2), a test aid, not part of the game.
 *
 * Only there when umk3.ini says debug_keys=1 (the launcher's "Debug mode"
 * box, in PLAY). F2 opens it over the game, which freezes underneath -- the
 * frame it was showing stays as a dimmed backdrop and GameCodeMain is not
 * called while the menu is up. Arrows or W/S move, left/right change a
 * value, Enter picks, Q / E change the tab, F2 closes; every one of those
 * debug keys can be rebound in the launcher's CONTROLS (key_dbg_* in
 * umk3.ini). The mouse works too: a click on a tab opens it, on a
 * value row its left half lowers the value and its right half raises it,
 * on any other row it picks.
 *
 * Five tabs along the top: FIGHTERS (the two fighters, their palettes, the
 * stage, START FIGHT), FIGHT (round and match keys, FINISHER, the Arcade
 * bosses), MENUS (any screen, the main menu), OPTIONS (direct keys, info),
 * TOOLS (hitboxes, fight speed, save states).
 *
 *     FIGHTER 1 / FIGHTER 2   any of CharacterNames' 26, the bosses included
 *     PALETTE 1 / PALETTE 2   AUTO (the game's rule: the alternate only for
 *                             the second of two identical fighters), 1 or 2,
 *                             for the next debug fight (back to AUTO after)
 *     STAGE                   a Level_Info row, 0..15
 *     START FIGHT             from the menus or from inside a fight
 *     SCREEN                  any of the front end's 51 screens
 *     MAIN MENU
 *     WIN / LOSE ROUND, WIN MATCH (skips the fight), LOSE MATCH   only while
 *                             a round is in play (not in the intro, the
 *                             round summary, a finisher or the pause)
 *     FINISHER                only during FINISH HIM/HER: player 1 does a pit
 *                             fatality, mercy, fatality 1 or 2, animality,
 *                             babality or friendship (DoASpecial 0xd..0x13),
 *                             whatever was typed
 *     ARCADE: MOTARO / SHAO KAHN  Arcade only: the next fight is that boss
 *     DIRECT KEYS             F3, F6..F12 on or off (on at start)
 *     INFO                    the task / screen line in the corner (also F3)
 *     HITBOXES                the boxes the fight engine tests, drawn over
 *                             the fight: each fighter's body, and every
 *                             strike for a moment (red, white on a hit)
 *     FIGHT SPEED             10% to 400%, a bar; only the fight runs at it
 *     STATE SLOT, SAVE STATE, LOAD STATE   four slots; a save is the whole
 *                             game at that tick and a load goes back to it,
 *                             in the same session (runtime/savestate.h)
 *
 * Everything is drawn here, with a 5x7 font of its own, in a space 320 high
 * and as wide as the view's shape, so it never stretches; nothing comes from the game's assets, so the menu works on a
 * screen whose own textures are broken. What a pick DOES is game_main.c's
 * business: this file only hands back a request.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform/platform.h"
#include "platform/gl.h"
#include "debug_menu.h"

#ifndef GL_CLAMP
#define GL_CLAMP 0x2900
#endif

extern const char *CharacterNames[26];
extern const char *FETaskNames[DBG_SCREENS];
extern char Level_Info[];

#define N_FIGHTERS 26
#define N_STAGES   16
#define LEVEL_INFO_STRIDE 0xf4

/* ------------------------------------------------------------------ font */

/* 5x7, one byte per row, bit 4 the leftmost pixel; ' ' to '_'. */
static const unsigned char k_font[64][7] = {
    {0,0,0,0,0,0,0},                                    /* ' ' */
    {0x04,0x04,0x04,0x04,0x04,0x00,0x04},               /* ! */
    {0x0A,0x0A,0x00,0x00,0x00,0x00,0x00},               /* " */
    {0x0A,0x0A,0x1F,0x0A,0x1F,0x0A,0x0A},               /* # */
    {0x04,0x0F,0x14,0x0E,0x05,0x1E,0x04},               /* $ */
    {0x18,0x19,0x02,0x04,0x08,0x13,0x03},               /* % */
    {0x0C,0x12,0x14,0x08,0x15,0x12,0x0D},               /* & */
    {0x0C,0x04,0x08,0x00,0x00,0x00,0x00},               /* ' */
    {0x02,0x04,0x08,0x08,0x08,0x04,0x02},               /* ( */
    {0x08,0x04,0x02,0x02,0x02,0x04,0x08},               /* ) */
    {0x00,0x04,0x15,0x0E,0x15,0x04,0x00},               /* * */
    {0x00,0x04,0x04,0x1F,0x04,0x04,0x00},               /* + */
    {0x00,0x00,0x00,0x00,0x0C,0x04,0x08},               /* , */
    {0x00,0x00,0x00,0x1F,0x00,0x00,0x00},               /* - */
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C},               /* . */
    {0x00,0x01,0x02,0x04,0x08,0x10,0x00},               /* / */
    {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E},               /* 0 */
    {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E},               /* 1 */
    {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F},               /* 2 */
    {0x1F,0x02,0x04,0x02,0x01,0x11,0x0E},               /* 3 */
    {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02},               /* 4 */
    {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E},               /* 5 */
    {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E},               /* 6 */
    {0x1F,0x01,0x02,0x04,0x08,0x08,0x08},               /* 7 */
    {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E},               /* 8 */
    {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C},               /* 9 */
    {0x00,0x0C,0x0C,0x00,0x0C,0x0C,0x00},               /* : */
    {0x00,0x0C,0x0C,0x00,0x0C,0x04,0x08},               /* ; */
    {0x02,0x04,0x08,0x10,0x08,0x04,0x02},               /* < */
    {0x00,0x00,0x1F,0x00,0x1F,0x00,0x00},               /* = */
    {0x08,0x04,0x02,0x01,0x02,0x04,0x08},               /* > */
    {0x0E,0x11,0x01,0x02,0x04,0x00,0x04},               /* ? */
    {0x0E,0x11,0x17,0x15,0x17,0x10,0x0F},               /* @ */
    {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11},               /* A */
    {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E},               /* B */
    {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E},               /* C */
    {0x1C,0x12,0x11,0x11,0x11,0x12,0x1C},               /* D */
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F},               /* E */
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10},               /* F */
    {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F},               /* G */
    {0x11,0x11,0x11,0x1F,0x11,0x11,0x11},               /* H */
    {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E},               /* I */
    {0x07,0x02,0x02,0x02,0x02,0x12,0x0C},               /* J */
    {0x11,0x12,0x14,0x18,0x14,0x12,0x11},               /* K */
    {0x10,0x10,0x10,0x10,0x10,0x10,0x1F},               /* L */
    {0x11,0x1B,0x15,0x15,0x11,0x11,0x11},               /* M */
    {0x11,0x11,0x19,0x15,0x13,0x11,0x11},               /* N */
    {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E},               /* O */
    {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10},               /* P */
    {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D},               /* Q */
    {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11},               /* R */
    {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E},               /* S */
    {0x1F,0x04,0x04,0x04,0x04,0x04,0x04},               /* T */
    {0x11,0x11,0x11,0x11,0x11,0x11,0x0E},               /* U */
    {0x11,0x11,0x11,0x11,0x11,0x0A,0x04},               /* V */
    {0x11,0x11,0x11,0x15,0x15,0x15,0x0A},               /* W */
    {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11},               /* X */
    {0x11,0x11,0x11,0x0A,0x04,0x04,0x04},               /* Y */
    {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F},               /* Z */
    {0x0E,0x08,0x08,0x08,0x08,0x08,0x0E},               /* [ */
    {0x00,0x10,0x08,0x04,0x02,0x01,0x00},               /* \ */
    {0x0E,0x02,0x02,0x02,0x02,0x02,0x0E},               /* ] */
    {0x04,0x0A,0x11,0x00,0x00,0x00,0x00},               /* ^ */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x1F},               /* _ */
};

#define CH_W 6                          /* 5 pixels and a gap */
#define CH_H 9

static void rect(float x, float y, float w, float h)
{
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
}

/* One pixel of the font is `px` units of the 480x320 screen. Lower case is
 * drawn as upper case. Inside glBegin(GL_QUADS). */
static void text(float x, float y, float px, const char *s)
{
    for (; *s; s++, x += CH_W * px) {
        int c = (unsigned char)*s, row, col;
        if (c >= 'a' && c <= 'z')
            c -= 32;
        if (c < 32 || c > 95)
            c = '?';
        for (row = 0; row < 7; row++)
            for (col = 0; col < 5; col++)
                if (k_font[c - 32][row] & (0x10 >> col))
                    rect(x + col * px, y + row * px, px, px);
    }
}

/* -------------------------------------------------------------- GL state */

static const GLenum k_caps[] = {
    GL_TEXTURE_2D, GL_DEPTH_TEST, GL_CULL_FACE, GL_FOG, GL_ALPHA_TEST,
    GL_LIGHTING, GL_BLEND
};
#define N_CAPS (int)(sizeof k_caps / sizeof k_caps[0])

static struct {
    GLint caps[N_CAPS], src, dst, tex, mode;
    GLfloat colour[4];
} g_saved;

/* The menu's space: 320 high and as wide as the view's shape, so on 4:3,
 * 16:9 or 21:9 the box keeps its proportions instead of stretching with a
 * 480-wide space. Set by begin_2d. */
static float g_ow = 480.0f;

/* The game sets most of its state once and expects to find it again, so the
 * menu puts back everything it touches. `w` is the space's width, 0 for the
 * view's shape. */
static void begin_2d_w(float w)
{
    int i;

    if (w <= 0) {
        GLint view[4];
        glGetIntegerv(GL_VIEWPORT, view);
        w = view[3] > 0 ? 320.0f * view[2] / view[3] : 480.0f;
    }
    g_ow = w;

    for (i = 0; i < N_CAPS; i++)
        glGetIntegerv(k_caps[i], &g_saved.caps[i]);
    glGetIntegerv(GL_BLEND_SRC, &g_saved.src);
    glGetIntegerv(GL_BLEND_DST, &g_saved.dst);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &g_saved.tex);
    glGetIntegerv(GL_MATRIX_MODE, &g_saved.mode);
    glGetFloatv(GL_CURRENT_COLOR, g_saved.colour);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, w, 320.0, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    for (i = 0; i < N_CAPS; i++)
        glDisable(k_caps[i]);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

static void begin_2d(void) { begin_2d_w(0); }

static void end_2d(void)
{
    int i;

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode((GLenum)g_saved.mode);
    for (i = 0; i < N_CAPS; i++)
        (g_saved.caps[i] ? glEnable : glDisable)(k_caps[i]);
    glBlendFunc((GLenum)g_saved.src, (GLenum)g_saved.dst);
    glBindTexture(GL_TEXTURE_2D, (GLuint)g_saved.tex);
    glColor4f(g_saved.colour[0], g_saved.colour[1], g_saved.colour[2],
              g_saved.colour[3]);
}

/* ------------------------------------------------------------- backdrop */

static GLuint g_shot;                   /* the frozen frame, or 0 */

/* The frame the game has just drawn, still in the back buffer. */
static void grab(int vx, int vy, int vw, int vh)
{
    unsigned char *px = malloc((size_t)vw * vh * 4);
    GLint old;

    if (!px)
        return;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &old);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(vx, vy, vw, vh, GL_RGBA, GL_UNSIGNED_BYTE, px);
    if (!g_shot)
        glGenTextures(1, &g_shot);
    glBindTexture(GL_TEXTURE_2D, g_shot);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, vw, vh, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, px);
    glBindTexture(GL_TEXTURE_2D, (GLuint)old);
    free(px);
}

/* ----------------------------------------------------------------- menu */

enum {
    ROW_P1, ROW_PAL1, ROW_P2, ROW_PAL2, ROW_STAGE, ROW_FIGHT,
    ROW_SCREEN, ROW_MAIN,
    ROW_WIN_ROUND, ROW_LOSE_ROUND, ROW_WIN_MATCH, ROW_LOSE_MATCH,
    ROW_FINISHER,
    ROW_MOTARO, ROW_SK,
    ROW_KEYS, ROW_INFO,
    ROW_HITBOX, ROW_SPEED, ROW_SLOT, ROW_SAVE, ROW_LOAD,
    ROW_CLOSE, N_ROWS
};

/* One page per kind of thing, shown as tabs along the top: Q / E (the
 * launcher can rebind them), L1 / R1 or a click on a tab to change; CLOSE
 * ends every page. */
#define MAX_PAGE_ROWS 10
static const struct {
    const char *name;
    int n;
    int rows[MAX_PAGE_ROWS];
} k_pages[] = {
    { "FIGHTERS", 7,
      { ROW_P1, ROW_PAL1, ROW_P2, ROW_PAL2, ROW_STAGE, ROW_FIGHT, ROW_CLOSE } },
    { "FIGHT", 8,
      { ROW_WIN_ROUND, ROW_LOSE_ROUND, ROW_WIN_MATCH, ROW_LOSE_MATCH,
        ROW_FINISHER, ROW_MOTARO, ROW_SK, ROW_CLOSE } },
    { "MENUS", 3, { ROW_SCREEN, ROW_MAIN, ROW_CLOSE } },
    { "OPTIONS", 3, { ROW_KEYS, ROW_INFO, ROW_CLOSE } },
    { "TOOLS", 6,
      { ROW_HITBOX, ROW_SPEED, ROW_SLOT, ROW_SAVE, ROW_LOAD, ROW_CLOSE } },
};
#define N_PAGES (int)(sizeof k_pages / sizeof k_pages[0])
enum { PAGE_FIGHTERS, PAGE_FIGHT, PAGE_MENUS, PAGE_OPTIONS, PAGE_TOOLS };

static const char *const k_label[N_ROWS] = {
    "FIGHTER 1", "PALETTE 1", "FIGHTER 2", "PALETTE 2", "STAGE",
    "START FIGHT", "SCREEN", "MAIN MENU", "WIN ROUND", "LOSE ROUND",
    "WIN MATCH (SKIP FIGHT)", "LOSE MATCH", "FINISHER (IN FINISH HIM)",
    "ARCADE: NEXT IS MOTARO", "ARCADE: NEXT IS SHAO KAHN",
    "DIRECT KEYS", "INFO LINE",
    "HITBOXES", "FIGHT SPEED", "STATE SLOT", "SAVE STATE", "LOAD STATE",
    "CLOSE"
};

static int g_open, g_want_open, g_page, g_sel;
static int g_p1 = 15, g_p2 = 25, g_stage, g_screen;   /* Kitana vs Shao Kahn */
static int g_info;
static int g_fin = 2;                   /* FATALITY 1 */
static const char *const k_fin[7] = {
    "PIT / STAGE", "MERCY", "FATALITY 1", "FATALITY 2", "ANIMALITY",
    "BABALITY", "FRIENDSHIP"
};
static int g_keys = 1;                 /* the direct keys (F3, F6..F12) */
static int g_hitbox;                    /* the TOOLS page */
static const int k_speed[] = { 10, 25, 50, 75, 100, 125, 150, 200, 300, 400 };
#define N_SPEEDS (int)(sizeof k_speed / sizeof k_speed[0])
static int g_speed = 4;                 /* 100% */
static int g_slot;
static int g_task;                      /* CurrentTask while open */

/* Each fighter's colours for the next debug fight: 0 the game's own rule
 * (palette 2 only for the second of two identical fighters), 1 the first
 * palette, 2 the alternate. LoadLevelCharacters reads it only while
 * DbgFightActive (game_main.c sets it from the start of a debug fight to
 * its end, then puts both back to AUTO). Port only. */
int DbgPalette[2];
int DbgFightActive;
static const char *const k_pal[3] = { "AUTO", "1", "2 (ALTERNATE)" };

/* Where the box was last drawn, in the menu's space, for the mouse: the
 * tab strip from g_title0 to g_top, N_PAGES tabs from g_bx, g_bw wide. */
static float g_bx, g_by, g_bw, g_top, g_rowh, g_title0;
static int   g_mouse_was;

int dbg_menu_is_open(void) { return g_open; }
int dbg_info_on(void)      { return g_info; }
int dbg_keys_on(void)      { return g_keys; }
int dbg_hitboxes_on(void)  { return g_hitbox; }
int dbg_speed_percent(void) { return k_speed[g_speed]; }
void dbg_info_toggle(void) { g_info = !g_info; }

void dbg_menu_toggle(void)
{
    if (g_open) {
        g_open = 0;
        printf("debug menu: closed\n");
    } else
        g_want_open = 1;                /* opened after the next frame */
}

static int row_enabled(int row);

void dbg_menu_after_frame(int vx, int vy, int vw, int vh, int task, int fe)
{
    if (!g_want_open)
        return;
    g_want_open = 0;
    grab(vx, vy, vw, vh);
    g_open = 1;
    g_task = task;
    g_mouse_was = 1;                    /* the click that opened it is not a pick */
    if (fe >= 0 && fe < DBG_SCREENS)
        g_screen = fe;
    /* in a fight that can take a fight key or a finisher, start there */
    if (task == 6 && (row_enabled(ROW_WIN_ROUND) || row_enabled(ROW_FINISHER))) {
        g_page = PAGE_FIGHT;
        g_sel = row_enabled(ROW_FINISHER) ? 4 : 0;
    }
    printf("debug menu: open (task %d, screen %d)\n", task, fe);
}

static const char *stage_name(int i)
{
    static char buf[40];
    const char *s = *(const char *const *)(Level_Info + i * LEVEL_INFO_STRIDE + 0x1c);
    char *u;

    snprintf(buf, sizeof buf, "%s", s ? s : "?");
    if ((u = strstr(buf, "_LEVEL")) != NULL || (u = strchr(buf, '.')) != NULL)
        *u = 0;
    return buf;
}

/* A fight or a screen picked while the game is still loading waits for the
 * main menu (game_main.c queues it), so only the fight keys ever grey out. */
static int row_enabled(int row)
{
    if (row >= ROW_WIN_ROUND && row <= ROW_LOSE_MATCH)
        return g_task == 6 && dbg_round_live();
    if (row == ROW_FINISHER)
        return g_task == 6 && dbg_finishing();
    if (row == ROW_MOTARO || row == ROW_SK)
        return dbg_in_arcade();
    if (row == ROW_SAVE)
        return dbg_state_can_save();
    if (row == ROW_LOAD)
        return dbg_state_has(g_slot);
    return 1;
}

static int wrap(int v, int n) { return (v % n + n) % n; }

static int has_value(int row)
{
    switch (row) {
    case ROW_P1: case ROW_PAL1: case ROW_P2: case ROW_PAL2: case ROW_STAGE:
    case ROW_SCREEN: case ROW_FINISHER: case ROW_KEYS: case ROW_INFO:
    case ROW_HITBOX: case ROW_SPEED: case ROW_SLOT:
        return 1;
    }
    return 0;
}

static void change(int row, int d)
{
    switch (row) {
    case ROW_P1:       g_p1 = wrap(g_p1 + d, N_FIGHTERS); break;
    case ROW_P2:       g_p2 = wrap(g_p2 + d, N_FIGHTERS); break;
    case ROW_PAL1:     DbgPalette[0] = wrap(DbgPalette[0] + d, 3); break;
    case ROW_PAL2:     DbgPalette[1] = wrap(DbgPalette[1] + d, 3); break;
    case ROW_STAGE:    g_stage = wrap(g_stage + d, N_STAGES); break;
    case ROW_SCREEN:   g_screen = wrap(g_screen + d, DBG_SCREENS); break;
    case ROW_FINISHER: g_fin = wrap(g_fin + d, 7); break;
    case ROW_INFO:     g_info = !g_info; break;
    case ROW_KEYS:     g_keys = !g_keys; break;
    case ROW_HITBOX:   g_hitbox = !g_hitbox; break;
    case ROW_SPEED:                     /* a bar: it stops at both ends */
        g_speed += d;
        if (g_speed < 0) g_speed = 0;
        if (g_speed > N_SPEEDS - 1) g_speed = N_SPEEDS - 1;
        break;
    case ROW_SLOT:     g_slot = wrap(g_slot + d, DBG_STATE_SLOTS); break;
    }
}

static void turn_page(int d)
{
    g_page = wrap(g_page + d, N_PAGES);
    g_sel = 0;
    while (g_sel < k_pages[g_page].n - 1
           && !row_enabled(k_pages[g_page].rows[g_sel]))
        g_sel++;                        /* CLOSE, last, is always on */
}

/* Key edges, read here so the menu never reaches the fight's input. */
static int edge(int k, int *was)
{
    int down = plat_key(k), hit = down && !*was;
    *was = down;
    return hit;
}

/* The mouse: game_main.c passes the pointer as fractions of the view each
 * frame the menu is open. A click on a tab opens that page; on a row with a
 * value, the left half lowers it and the right half raises it; on any other
 * row it picks. */
static float g_mx, g_my;
static int   g_mdown;

void dbg_menu_mouse(float x, float y, int down)
{
    g_mx = x * g_ow;
    g_my = y * 320.0f;
    g_mdown = down;
}

/* The pad, passed in by game_main.c each frame the menu is open: d-pad or
 * stick to move, south (cross / A) to pick, L1 / R1 to turn the page, east
 * (circle / B) to close. */
static PLAT_PAD g_pad, g_pad_was;

void dbg_menu_pad(const PLAT_PAD *p)
{
    g_pad = *p;
}

#define PAD_EDGE(f)  (g_pad.f && !g_pad_was.f)
#define PAD_HIT(b)   ((g_pad.buttons & ~g_pad_was.buttons) & (1u << (b)))

int dbg_menu_tick(struct dbg_request *rq)
{
    static int was[11];
    int up, down, left, right, ok, prev, next, row, click;

    up    = edge(PK_UP, &was[0])    | edge(PK_P2_UP, &was[1])    | PAD_EDGE(up);
    down  = edge(PK_DOWN, &was[2])  | edge(PK_P2_DOWN, &was[3])  | PAD_EDGE(down);
    left  = edge(PK_LEFT, &was[4])  | edge(PK_P2_LEFT, &was[5])  | PAD_EDGE(left);
    right = edge(PK_RIGHT, &was[6]) | edge(PK_P2_RIGHT, &was[7]) | PAD_EDGE(right);
    ok    = edge(PK_OK, &was[8]) | (PAD_HIT(PB_SOUTH) != 0);
    prev  = edge(PK_DBG_PAGE_PREV, &was[9]) | (PAD_HIT(PB_L1) != 0);
    next  = edge(PK_DBG_PAGE_NEXT, &was[10]) | (PAD_HIT(PB_R1) != 0);
    if (PAD_HIT(PB_EAST)) {
        g_pad_was = g_pad;
        dbg_menu_toggle();
        memset(rq, 0, sizeof *rq);
        return 0;
    }
    g_pad_was = g_pad;
    click = g_mdown && !g_mouse_was;
    g_mouse_was = g_mdown;

    memset(rq, 0, sizeof *rq);
    if (prev || next)
        turn_page(next ? 1 : -1);

    if (click && g_rowh > 0 && g_mx >= g_bx && g_mx < g_bx + g_bw) {
        float mid = g_bx + g_bw * 0.5f;
        if (g_my >= g_title0 && g_my < g_top) {
            int t = (int)((g_mx - g_bx) * N_PAGES / g_bw);
            if (t >= 0 && t < N_PAGES && t != g_page)
                turn_page(t - g_page);
        } else if (g_my >= g_top) {
            int i = (int)((g_my - g_top) / g_rowh);
            if (i < k_pages[g_page].n
                && row_enabled(k_pages[g_page].rows[i])) {
                g_sel = i;
                if (has_value(k_pages[g_page].rows[i]))
                    change(k_pages[g_page].rows[i], g_mx < mid ? -1 : 1);
                else
                    ok = 1;
            }
        }
    }

    if (up || down) {
        int i, n = k_pages[g_page].n;
        for (i = 0; i < n; i++) {               /* skip the greyed rows */
            g_sel = wrap(g_sel + (up ? -1 : 1), n);
            if (row_enabled(k_pages[g_page].rows[g_sel]))
                break;
        }
    }
    row = k_pages[g_page].rows[g_sel];
    if (left || right)
        change(row, right ? 1 : -1);
    if (!ok || !row_enabled(row))
        return 0;

    switch (row) {
    case ROW_P1: case ROW_PAL1: case ROW_P2: case ROW_PAL2: case ROW_STAGE:
        g_sel = 5;                              /* Enter on a value: go on */
        return 0;
    case ROW_FIGHT:
        rq->what = DBG_FIGHT;
        rq->a = g_p1; rq->b = g_p2; rq->c = g_stage;
        break;
    case ROW_SCREEN:
        rq->what = DBG_SCREEN;
        rq->a = g_screen;
        break;
    case ROW_MAIN:
        rq->what = DBG_SCREEN;
        rq->a = 0;
        break;
    case ROW_WIN_ROUND: case ROW_LOSE_ROUND:
    case ROW_WIN_MATCH: case ROW_LOSE_MATCH:
        rq->what = DBG_FIGHT_KEY;               /* 0..3, as F9..F12 */
        rq->a = row - ROW_WIN_ROUND;
        break;
    case ROW_FINISHER:
        rq->what = DBG_FINISHER;
        rq->a = g_fin;
        break;
    case ROW_MOTARO: case ROW_SK:
        rq->what = DBG_BOSS;
        rq->a = row == ROW_MOTARO ? 24 : 25;
        break;
    case ROW_INFO: case ROW_KEYS: case ROW_HITBOX:
        change(row, 1);
        return 0;
    case ROW_SPEED: case ROW_SLOT:
        return 0;
    case ROW_SAVE: case ROW_LOAD:
        rq->what = row == ROW_SAVE ? DBG_SAVE_STATE : DBG_LOAD_STATE;
        rq->a = g_slot;
        break;
    case ROW_CLOSE:
        break;
    }
    g_open = 0;
    return rq->what != DBG_NONE;
}

/* A row's value, "" for none. */
static void row_value(int i, char *v, size_t n)
{
    char a[24], b[24];

    v[0] = 0;
    switch (i) {
    case ROW_P1: snprintf(v, n, "< %2d %s >", g_p1, CharacterNames[g_p1]); break;
    case ROW_P2: snprintf(v, n, "< %2d %s >", g_p2, CharacterNames[g_p2]); break;
    case ROW_PAL1: snprintf(v, n, "< %s >", k_pal[DbgPalette[0]]); break;
    case ROW_PAL2: snprintf(v, n, "< %s >", k_pal[DbgPalette[1]]); break;
    case ROW_STAGE: snprintf(v, n, "< %2d %s >", g_stage, stage_name(g_stage)); break;
    case ROW_SCREEN:
        snprintf(v, n, "< %2d %s >", g_screen, FETaskNames[g_screen] + 8);
        break;
    case ROW_KEYS:
        plat_key_label(PK_DBG_KO_P2, a, sizeof a);
        plat_key_label(PK_DBG_LOSE, b, sizeof b);
        snprintf(v, n, "< %s >  %s-%s", g_keys ? "ON" : "OFF", a, b);
        break;
    case ROW_INFO:
        plat_key_label(PK_BACK, a, sizeof a);
        snprintf(v, n, "< %s >  %s", g_info ? "ON" : "OFF", a);
        break;
    case ROW_FINISHER: snprintf(v, n, "< %s >", k_fin[g_fin]); break;
    case ROW_HITBOX: snprintf(v, n, "< %s >", g_hitbox ? "ON" : "OFF"); break;
    case ROW_SPEED: {
        char bar[N_SPEEDS + 1];
        int i;
        for (i = 0; i < N_SPEEDS; i++)
            bar[i] = i <= g_speed ? '#' : '-';
        bar[N_SPEEDS] = 0;
        snprintf(v, n, "< %s %3d%% >", bar, k_speed[g_speed]);
        break;
    }
    case ROW_SLOT:
        snprintf(v, n, "< %d %s >", g_slot + 1,
                 dbg_state_has(g_slot) ? "SAVED" : "EMPTY");
        break;
    }
}

static void title_text(char *t, size_t n)
{
    snprintf(t, n, "DEBUG MENU");
}

static void footer_text(char *f, size_t n)
{
    char p[24], q[24], c[24];

    plat_key_label(PK_DBG_PAGE_PREV, p, sizeof p);
    plat_key_label(PK_DBG_PAGE_NEXT, q, sizeof q);
    plat_key_label(PK_TEST, c, sizeof c);
    snprintf(f, n, "%s/%s TAB   ARROWS OR MOUSE   ENTER PICK   %s CLOSE",
             p, q, c);
}

/* Any menu box over the frozen frame: the debug menu's and the settings
 * menu's (settings_menu.c). In the iPhone OS 3 alert dress the in-game
 * alerts wear (plat_ui_menu, runtime/platform/win32_gl.c), drawn at the
 * game view's own pixel size so the text is sharp, and redrawn only when
 * something in it changes; the 5x7 font when the backend has no such
 * drawing. `geo` gets where the box went, in the menu's space, for the
 * mouse. */
static int draw_ios(const char *title, const char *const *tabs, int ntabs,
                    int page, const char *const *lp, const char *const *vp,
                    const int *en, int rows, int sel, const char *footer)
{
    static GLuint tex;
    static char last[4096];
    static int tw, th;
    static float ts;
    char sig[4096];
    int i, n = 0;
    GLint view[4];
    float s;

    glGetIntegerv(GL_VIEWPORT, view);
    s = view[3] / 320.0f;               /* by height: the shape is kept */
    if (s <= 0)
        return 0;
    n = snprintf(sig, sizeof sig, "%s|%s|%d|", title, footer, ntabs);
    for (i = 0; i < ntabs && n < (int)sizeof sig - 200; i++)
        n += snprintf(sig + n, sizeof sig - n, "%s|", tabs[i]);
    for (i = 0; i < rows && n < (int)sizeof sig - 200; i++)
        n += snprintf(sig + n, sizeof sig - n, "%s=%s%d|", lp[i], vp[i], en[i]);
    snprintf(sig + n, sizeof sig - n, "%d %d %.3f", sel, page, s);

    if (!tex || strcmp(sig, last) != 0) {
        int w, h;
        unsigned char *px = plat_ui_menu(title, tabs, ntabs, page, lp, vp,
                                         en, rows, sel, footer, s, &w, &h);
        if (!px)
            return 0;
        if (!tex)
            glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, px);
        free(px);
        tw = w;
        th = h;
        ts = s;
        strcpy(last, sig);
    }

    begin_2d();
    if (g_shot) {                       /* the frozen frame, dimmed */
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, g_shot);
        glColor4f(0.6f, 0.6f, 0.6f, 1.0f);
        glBegin(GL_QUADS);                      /* glReadPixels is bottom-up */
        glTexCoord2f(0, 1); glVertex2f(0, 0);
        glTexCoord2f(1, 1); glVertex2f(g_ow, 0);
        glTexCoord2f(1, 0); glVertex2f(g_ow, 320);
        glTexCoord2f(0, 0); glVertex2f(0, 320);
        glEnd();
    }
    {
        float w = tw / ts, h = th / ts;
        float x = (g_ow - w) * 0.5f, y = (320 - h) * 0.5f;

        /* plat_ui_menu's layout, in points: a 6 margin, the title band
         * 22 tall, the tab strip 20 (inset 9 at the sides), a 4 gap, rows
         * 15 tall inside a box 360 wide */
        g_bx = x + 6 + 9;
        g_bw = 360 - 18;
        g_title0 = y + 6 + 22;
        g_top = y + 6 + 22 + (ntabs > 0 ? 20 + 4 : 4);
        g_rowh = 15;

        glEnable(GL_TEXTURE_2D);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);    /* premultiplied */
        glBindTexture(GL_TEXTURE_2D, tex);
        glColor4f(1, 1, 1, 1);
        glBegin(GL_QUADS);
        glTexCoord2f(0, 0); glVertex2f(x, y);
        glTexCoord2f(1, 0); glVertex2f(x + w, y);
        glTexCoord2f(1, 1); glVertex2f(x + w, y + h);
        glTexCoord2f(0, 1); glVertex2f(x, y + h);
        glEnd();
        glDisable(GL_TEXTURE_2D);
    }
    end_2d();
    return 1;
}

void ui_menu_show(const char *title, const char *const *tabs, int ntabs,
                  int page, const char *const *lp, const char *const *vp,
                  const int *en, int rows, int sel, const char *footer,
                  struct ui_geo *geo)
{
    const float px = 1.0f, lh = 15.0f, y0 = 46.0f;
    float x0, bw;
    int i;

    if (!draw_ios(title, tabs, ntabs, page, lp, vp, en, rows, sel, footer)) {
        begin_2d();
        x0 = (g_ow - 288.0f) * 0.5f;
        bw = g_ow - 2 * (x0 - 12);
        g_bx = x0 - 12;
        g_bw = bw;
        g_title0 = y0 - 22;
        g_top = y0 - 4;
        g_rowh = lh;
        if (g_shot) {
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, g_shot);
            glColor4f(0.45f, 0.45f, 0.45f, 1.0f);
            glBegin(GL_QUADS);                  /* glReadPixels is bottom-up */
            glTexCoord2f(0, 1); glVertex2f(0, 0);
            glTexCoord2f(1, 1); glVertex2f(g_ow, 0);
            glTexCoord2f(1, 0); glVertex2f(g_ow, 320);
            glTexCoord2f(0, 0); glVertex2f(0, 320);
            glEnd();
            glDisable(GL_TEXTURE_2D);
        }

        glBegin(GL_QUADS);
        glColor4f(0.0f, 0.0f, 0.0f, 0.75f);
        rect(x0 - 12, y0 - 40, bw, rows * lh + 66);
        for (i = 0; i < ntabs; i++) {           /* the tabs */
            float tw = bw / ntabs, tx = x0 - 12 + i * tw;
            if (i == page)
                glColor4f(0.8f, 0.1f, 0.1f, 0.9f);
            else
                glColor4f(0.25f, 0.25f, 0.25f, 0.9f);
            rect(tx + 1, y0 - 22, tw - 2, 14);
            glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
            text(tx + (tw - (float)strlen(tabs[i]) * CH_W) * 0.5f,
                 y0 - 19, px, tabs[i]);
        }
        glColor4f(0.8f, 0.1f, 0.1f, 0.9f);
        rect(x0 - 8, y0 + sel * lh - 4, bw - 8, lh - 1);

        glColor4f(1.0f, 0.85f, 0.2f, 1.0f);
        text(x0, y0 - 36, px, title);
        for (i = 0; i < rows; i++) {
            float y = y0 + i * lh;

            if (en[i])
                glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
            else
                glColor4f(0.45f, 0.45f, 0.45f, 1.0f);
            text(x0, y, px, lp[i]);
            if (vp[i][0])
                text(x0 + 100, y, px, vp[i]);
        }
        glColor4f(0.7f, 0.7f, 0.7f, 1.0f);
        text(x0, y0 + rows * lh + 6, px, footer);
        glEnd();
        end_2d();
    }
    if (geo) {
        geo->bx = g_bx;
        geo->bw = g_bw;
        geo->title0 = g_title0;
        geo->top = g_top;
        geo->rowh = g_rowh;
        geo->ow = g_ow;
    }
}

void ui_grab(int vx, int vy, int vw, int vh) { grab(vx, vy, vw, vh); }

/* A picture beside the box (the settings menu's profile icon): uploaded
 * once by ui_icon_set, drawn each frame by ui_icon_draw in the menu's
 * space. Straight alpha. */
static GLuint g_icon;

void ui_icon_set(const unsigned char *rgba, int w, int h)
{
    GLint old;

    if (!rgba)
        return;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &old);
    if (!g_icon)
        glGenTextures(1, &g_icon);
    glBindTexture(GL_TEXTURE_2D, g_icon);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, rgba);
    glBindTexture(GL_TEXTURE_2D, (GLuint)old);
}

void ui_icon_draw(float x, float y, float size)
{
    if (!g_icon)
        return;
    begin_2d();
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, g_icon);
    glColor4f(1, 1, 1, 1);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(x, y);
    glTexCoord2f(1, 0); glVertex2f(x + size, y);
    glTexCoord2f(1, 1); glVertex2f(x + size, y + size);
    glTexCoord2f(0, 1); glVertex2f(x, y + size);
    glEnd();
    glDisable(GL_TEXTURE_2D);
    end_2d();
}

void dbg_menu_draw(void)
{
    char vals[MAX_PAGE_ROWS][64], title[96], footer[128];
    const char *vp[MAX_PAGE_ROWS], *lp[MAX_PAGE_ROWS], *tabs[N_PAGES];
    int en[MAX_PAGE_ROWS], i, rows = k_pages[g_page].n;

    for (i = 0; i < N_PAGES; i++)
        tabs[i] = k_pages[i].name;
    title_text(title, sizeof title);
    footer_text(footer, sizeof footer);
    for (i = 0; i < rows; i++) {
        int r = k_pages[g_page].rows[i];
        row_value(r, vals[i], sizeof vals[i]);
        vp[i] = vals[i];
        lp[i] = k_label[r];
        en[i] = row_enabled(r);
    }
    ui_menu_show(title, tabs, N_PAGES, g_page, lp, vp, en, rows, g_sel,
                 footer, NULL);
}

/* ---------------------------------------------------------------- frame
 *
 * Port only, not part of the game: in fullscreen the 3:2 game leaves bars at
 * the sides of a wider screen; a picture of the player's choosing fills them
 * (umk3.ini marco=<file>, or marco.png beside the exe). It is drawn over the
 * whole window first and the game draws its 3:2 rectangle on top. "Cover"
 * scaling: the picture fills the window and is cropped, never stretched. */
#include "lime/png.h"

static GLuint g_frame_tex;
static int    g_frame_w, g_frame_h;

int dbg_frame_load(const char *path)
{
    uint8_t *px = NULL;
    int w = 0, h = 0;

    if (!path || !lime_png_load(path, &px, &w, &h))
        return 0;
    glGenTextures(1, &g_frame_tex);
    glBindTexture(GL_TEXTURE_2D, g_frame_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, px);
    free(px);
    g_frame_w = w;
    g_frame_h = h;
    printf("frame picture: %s (%dx%d)\n", path, w, h);
    return 1;
}

/* The window is ww x wh pixels; the caller has set a full-window viewport. */
void dbg_frame_draw(int ww, int wh)
{
    float u0 = 0, v0 = 0, u1 = 1, v1 = 1;

    if (!g_frame_tex || ww <= 0 || wh <= 0)
        return;
    if ((float)g_frame_w / g_frame_h > (float)ww / wh) {
        float keep = ((float)ww / wh) / ((float)g_frame_w / g_frame_h);
        u0 = (1 - keep) / 2;
        u1 = u0 + keep;
    } else {
        float keep = ((float)g_frame_w / g_frame_h) / ((float)ww / wh);
        v0 = (1 - keep) / 2;
        v1 = v0 + keep;
    }
    begin_2d_w(480.0f);                 /* the whole window, cropped above */
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, g_frame_tex);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glBegin(GL_QUADS);                  /* the 480x320 ortho of begin_2d */
    glTexCoord2f(u0, v0); glVertex2f(0, 0);
    glTexCoord2f(u1, v0); glVertex2f(480, 0);
    glTexCoord2f(u1, v1); glVertex2f(480, 320);
    glTexCoord2f(u0, v1); glVertex2f(0, 320);
    glEnd();
    end_2d();
}

void dbg_info_draw(const char *line)
{
    begin_2d();
    glBegin(GL_QUADS);
    glColor4f(0.0f, 0.0f, 0.0f, 0.6f);
    rect(0, 0, (float)strlen(line) * CH_W + 4, CH_H + 2);
    glColor4f(0.4f, 1.0f, 0.4f, 1.0f);
    text(2, 2, 1.0f, line);
    glEnd();
    end_2d();
}
