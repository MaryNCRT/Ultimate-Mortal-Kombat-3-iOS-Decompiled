/*
 * debug_menu.c -- the in-game debug menu (F2), a test aid, not part of the game.
 *
 * Only there when umk3.ini says debug_keys=1 (the launcher's "Debug mode"
 * box, in PLAY). F2 opens it over the game, which freezes underneath -- the
 * frame it was showing stays as a dimmed backdrop and GameCodeMain is not
 * called while the menu is up. It is laid out as the settings menu (Esc,
 * settings_menu.c): three categories down a side panel, sections under
 * headings, a list that scrolls, and a list of the values that opens on a
 * row that has them. Arrows or W/S (or the mouse's wheel) move, left/right
 * change a value, Enter picks or opens the list, Q / E change the category,
 * Esc goes back, F2 closes; every one of those debug keys can be rebound in
 * the launcher's CONTROLS (key_dbg_* in umk3.ini). A click on a category
 * opens it, on a row picks it; the pad works as in the settings menu.
 *
 *   FIGHT
 *     FIGHTERS   FIGHTER 1 / 2 (any of CharacterNames' 26, the bosses
 *                included), PALETTE 1 / 2 (AUTO: the game's rule, the
 *                alternate only for the second of two identical fighters;
 *                1 or 2, for the next debug fight, back to AUTO after),
 *                STAGE (a Level_Info row, 0..15), START FIGHT
 *     ROUND      WIN / LOSE ROUND, WIN MATCH (skips the fight), LOSE MATCH,
 *                only while a round is in play (not in the intro, the round
 *                summary, a finisher or the pause); FINISHER, only during
 *                FINISH HIM/HER: player 1 does a pit fatality, mercy,
 *                fatality 1 or 2, animality, babality or friendship
 *                (DoASpecial 0xd..0x13), picked from its list
 *     ARCADE     NEXT IS MOTARO / SHAO KAHN, Arcade only
 *     CHEATS     INFINITE HEALTH (player 1, 2 or both: the bar never goes
 *                down) and FREEZE CLOCK, the hooks training uses
 *                (logic/other.c); never in an online fight
 *   MENUS
 *     SCREENS    SCREEN (any of the front end's 51, picked from its list),
 *                MAIN MENU
 *   TOOLS
 *     OPTIONS    DIRECT KEYS (F3, F6..F12, on at start), INFO LINE (the
 *                task / screen line in the corner, also F3), FRAMES PER
 *                SECOND and ONLINE NUMBERS (the rollback's frame, delay,
 *                rollbacks, holds and desyncs) under it
 *     FIGHT      HITBOXES (the boxes the fight engine tests, drawn over the
 *                fight: each fighter's body, and every strike for a moment,
 *                red, white on a hit), FIGHT SPEED (10% to 400%, a bar)
 *     SAVE STATES  SLOT, SAVE STATE, LOAD STATE: four slots; a save is the
 *                whole game at that tick and a load goes back to it, in the
 *                same session (runtime/savestate.h)
 *
 * Everything is drawn here, with a 5x7 font of its own, in a space 320 high
 * and as wide as the view's shape, so it never stretches; nothing comes from the game's assets, so the menu works on a
 * screen whose own textures are broken. What a pick DOES is game_main.c's
 * business: this file only hands back a request.
 */

#include <math.h>
#include <stdarg.h>
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

extern char Language[10];
extern int PortDbgClock, PortDbgHealth;  /* logic/other.c, port */

static const char *tx(const char *es, const char *en)
{
    return Language[0] == 'E' && Language[1] == 'S' ? es : en;
}

/* Three categories down the side panel, as in the settings menu (Esc):
 * FIGHT (the fighters, the round, Arcade, cheats), MENUS (the screens) and
 * TOOLS (the options, the fight tools, save states); Q / E, L1 / R1 or a
 * click on one changes it. Each holds sections under their headings and
 * ends in BACK TO THE GAME. */
enum { PAGE_FIGHT, PAGE_MENUS, PAGE_TOOLS, N_PAGES };

enum {
    ROW_P1, ROW_PAL1, ROW_P2, ROW_PAL2, ROW_STAGE, ROW_FIGHT,
    ROW_SCREEN, ROW_MAIN,
    ROW_WIN_ROUND, ROW_LOSE_ROUND, ROW_WIN_MATCH, ROW_LOSE_MATCH,
    ROW_FINISHER,
    ROW_MOTARO, ROW_SK,
    ROW_HEALTH, ROW_CLOCK,
    ROW_KEYS, ROW_INFO, ROW_FPS, ROW_NET,
    ROW_HITBOX, ROW_SPEED, ROW_SLOT, ROW_SAVE, ROW_LOAD,
    ROW_CLOSE, ROW_HEAD
};

#define MAX_ROWS 40
#define VIS_ROWS 15
static struct {
    int  kind;
    char label[48], value[192];
    int  en;
} g_rows[MAX_ROWS];
static int g_n, g_first;

static int g_open, g_want_open, g_page, g_sel;
static int g_p1 = 15, g_p2 = 25, g_stage, g_screen;   /* Kitana vs Shao Kahn */
static int g_info, g_fps, g_net;
static int g_fin = 2;                   /* FATALITY 1 */
static const char *const k_fin_es[7] = {
    "FOSO / ESCENARIO", "MERCY", "FATALITY 1", "FATALITY 2", "ANIMALITY",
    "BABALITY", "FRIENDSHIP"
};
static const char *const k_fin_en[7] = {
    "PIT / STAGE", "MERCY", "FATALITY 1", "FATALITY 2", "ANIMALITY",
    "BABALITY", "FRIENDSHIP"
};
static int g_keys = 1;                 /* the direct keys (F3, F6..F12) */
static int g_hitbox;
static int g_health, g_clock;           /* INFINITE HEALTH (bits), FREEZE CLOCK */
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

static const char *pal_name(int i)
{
    return i == 0 ? "AUTO" : i == 1 ? "1" : tx("2 (ALTERNA)", "2 (ALTERNATE)");
}

static const char *health_name(int i)
{
    static const char *const es[4] = { "NO", "JUGADOR 1", "JUGADOR 2", "AMBOS" };
    static const char *const en[4] = { "OFF", "PLAYER 1", "PLAYER 2", "BOTH" };
    return tx(es[i & 3], en[i & 3]);
}

static const char *on_off(int on)
{
    return on ? tx("SI", "ON") : tx("NO", "OFF");
}

/* Where the box and an open list went, in the menu's space, for the mouse. */
static float g_bx, g_bw, g_top, g_rowh, g_title0;
static float g_sx, g_sw, g_stop, g_srow;    /* the side panel, if any */
static struct ui_geo g_geo, g_dgeo;
static int   g_mouse_was, g_esc_was;
static float g_mx, g_my;                /* the pointer, in the menu's space */
static int   g_mdown;

/* The list a row with values opens (Enter, or a click on it), as in the
 * settings menu: its row, the options, the current one, the one under the
 * cursor and the first one in sight. */
#define MAX_DROP 64
#define VIS_DROP 13
static int  g_drop, g_drop_kind, g_drop_n, g_drop_cur, g_drop_sel, g_drop_first;
static char g_drop_lab[MAX_DROP][48];
static char g_drop_title[48];

int dbg_menu_is_open(void) { return g_open; }
int dbg_info_on(void)      { return g_info; }
int dbg_fps_on(void)       { return g_fps; }
int dbg_net_on(void)       { return g_net; }
int dbg_keys_on(void)      { return g_keys; }
int dbg_hitboxes_on(void)  { return g_hitbox; }
int dbg_health_mask(void)  { return g_health; }
int dbg_clock_frozen(void) { return g_clock; }
int dbg_speed_percent(void) { return k_speed[g_speed]; }
void dbg_info_toggle(void) { g_info = !g_info; }

void dbg_menu_toggle(void)
{
    if (g_open) {
        g_open = 0;
        g_drop = 0;
        printf("debug menu: closed\n");
    } else
        g_want_open = 1;                /* opened after the next frame */
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
    if (row == ROW_HEAD)
        return 0;
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

/* ---------------------------------------------------------------- rows */

static void add(int kind, const char *label, const char *fmt, ...)
{
    va_list ap;

    if (g_n >= MAX_ROWS)
        return;
    g_rows[g_n].kind = kind;
    g_rows[g_n].en = row_enabled(kind);
    snprintf(g_rows[g_n].label, sizeof g_rows[0].label, "%s", label);
    va_start(ap, fmt);
    vsnprintf(g_rows[g_n].value, sizeof g_rows[0].value, fmt, ap);
    va_end(ap);
    g_n++;
}

/* A section's heading: shown, never chosen (plat_ui_menu draws "\002"). */
static void head(const char *name)
{
    add(ROW_HEAD, "", "");
    snprintf(g_rows[g_n - 1].label, sizeof g_rows[0].label, "\002%s", name);
}

static void build(void)
{
    char a[96], b[96];

    g_n = 0;
    switch (g_page) {
    case PAGE_FIGHT:
        head(tx("LUCHADORES", "FIGHTERS"));
        add(ROW_P1, tx("LUCHADOR 1", "FIGHTER 1"), "< %2d %s >", g_p1,
            CharacterNames[g_p1]);
        add(ROW_PAL1, tx("PALETA 1", "PALETTE 1"), "< %s >", pal_name(DbgPalette[0]));
        add(ROW_P2, tx("LUCHADOR 2", "FIGHTER 2"), "< %2d %s >", g_p2,
            CharacterNames[g_p2]);
        add(ROW_PAL2, tx("PALETA 2", "PALETTE 2"), "< %s >", pal_name(DbgPalette[1]));
        add(ROW_STAGE, tx("ESCENARIO", "STAGE"), "< %2d %s >", g_stage,
            stage_name(g_stage));
        add(ROW_FIGHT, tx("EMPEZAR PELEA", "START FIGHT"), "");
        head(tx("RONDA", "ROUND"));
        add(ROW_WIN_ROUND, tx("GANAR RONDA", "WIN ROUND"), "");
        add(ROW_LOSE_ROUND, tx("PERDER RONDA", "LOSE ROUND"), "");
        add(ROW_WIN_MATCH, tx("GANAR COMBATE (SALTA LA PELEA)",
                              "WIN MATCH (SKIP FIGHT)"), "");
        add(ROW_LOSE_MATCH, tx("PERDER COMBATE", "LOSE MATCH"), "");
        add(ROW_FINISHER, tx("REMATE (EN FINISH HIM)", "FINISHER (IN FINISH HIM)"),
            "< %s >", tx(k_fin_es[g_fin], k_fin_en[g_fin]));
        head("ARCADE");
        add(ROW_MOTARO, tx("SIGUIENTE: MOTARO", "NEXT IS MOTARO"), "");
        add(ROW_SK, tx("SIGUIENTE: SHAO KAHN", "NEXT IS SHAO KAHN"), "");
        head(tx("TRUCOS", "CHEATS"));
        add(ROW_HEALTH, tx("VIDA INFINITA", "INFINITE HEALTH"), "< %s >",
            health_name(g_health));
        add(ROW_CLOCK, tx("CONGELAR RELOJ", "FREEZE CLOCK"), "< %s >",
            on_off(g_clock));
        break;
    case PAGE_MENUS:
        head(tx("PANTALLAS", "SCREENS"));
        add(ROW_SCREEN, tx("PANTALLA", "SCREEN"), "< %2d %s >", g_screen,
            FETaskNames[g_screen] + 8);
        add(ROW_MAIN, tx("MENU PRINCIPAL", "MAIN MENU"), "");
        break;
    case PAGE_TOOLS:
        head("GENERAL");
        plat_key_glyph(PK_DBG_KO_P2, a, sizeof a);
        plat_key_glyph(PK_DBG_LOSE, b, sizeof b);
        add(ROW_KEYS, tx("TECLAS DIRECTAS", "DIRECT KEYS"), "< %s >  %s-%s",
            on_off(g_keys), a, b);
        plat_key_glyph(PK_BACK, a, sizeof a);
        add(ROW_INFO, tx("LINEA DE INFO", "INFO LINE"), "< %s >  %s",
            on_off(g_info), a);
        add(ROW_FPS, tx("FOTOGRAMAS POR SEGUNDO", "FRAMES PER SECOND"), "< %s >",
            on_off(g_fps));
        add(ROW_NET, tx("DATOS DEL ONLINE (ROLLBACK)", "ONLINE NUMBERS (ROLLBACK)"),
            "< %s >", on_off(g_net));
        head(tx("PELEA", "FIGHT"));
        add(ROW_HITBOX, tx("CAJAS DE GOLPE", "HITBOXES"), "< %s >", on_off(g_hitbox));
        {
            char bar[N_SPEEDS + 1];
            int i;
            for (i = 0; i < N_SPEEDS; i++)
                bar[i] = i <= g_speed ? '#' : '-';
            bar[N_SPEEDS] = 0;
            add(ROW_SPEED, tx("VELOCIDAD DE LA PELEA", "FIGHT SPEED"),
                "< %s %3d%% >", bar, k_speed[g_speed]);
        }
        head(tx("ESTADOS GUARDADOS", "SAVE STATES"));
        add(ROW_SLOT, tx("RANURA", "SLOT"), "< %d %s >", g_slot + 1,
            dbg_state_has(g_slot) ? tx("GUARDADO", "SAVED") : tx("VACIO", "EMPTY"));
        add(ROW_SAVE, tx("GUARDAR ESTADO", "SAVE STATE"), "");
        add(ROW_LOAD, tx("CARGAR ESTADO", "LOAD STATE"), "");
        break;
    }
    add(ROW_CLOSE, tx("VOLVER AL JUEGO", "BACK TO THE GAME"), "");
}

/* the rows that take Enter as a toggle, and those that open a list */
static int is_toggle(int row)
{
    return row == ROW_CLOCK || row == ROW_KEYS || row == ROW_INFO
        || row == ROW_FPS || row == ROW_NET || row == ROW_HITBOX;
}

/* The variable a list row sets and how many values it has. */
static int *list_var(int row, int *n)
{
    switch (row) {
    case ROW_P1:       *n = N_FIGHTERS;      return &g_p1;
    case ROW_P2:       *n = N_FIGHTERS;      return &g_p2;
    case ROW_PAL1:     *n = 3;               return &DbgPalette[0];
    case ROW_PAL2:     *n = 3;               return &DbgPalette[1];
    case ROW_STAGE:    *n = N_STAGES;        return &g_stage;
    case ROW_SCREEN:   *n = DBG_SCREENS;     return &g_screen;
    case ROW_FINISHER: *n = 7;               return &g_fin;
    case ROW_HEALTH:   *n = 4;               return &g_health;
    case ROW_SPEED:    *n = N_SPEEDS;        return &g_speed;
    case ROW_SLOT:     *n = DBG_STATE_SLOTS; return &g_slot;
    }
    *n = 0;
    return NULL;
}

static void change(int row, int d)
{
    int n, *v = list_var(row, &n);

    if (row == ROW_SPEED) {             /* a bar: it stops at both ends */
        g_speed += d;
        if (g_speed < 0) g_speed = 0;
        if (g_speed > N_SPEEDS - 1) g_speed = N_SPEEDS - 1;
    } else if (v)
        *v = wrap(*v + d, n);
    else switch (row) {
    case ROW_CLOCK:  g_clock = !g_clock; break;
    case ROW_INFO:   g_info = !g_info; break;
    case ROW_FPS:    g_fps = !g_fps; break;
    case ROW_NET:    g_net = !g_net; break;
    case ROW_KEYS:   g_keys = !g_keys; break;
    case ROW_HITBOX: g_hitbox = !g_hitbox; break;
    }
}

/* ------------------------------------------------------------ lists */

static void drop_open(int row)
{
    int n, i, *v = list_var(row, &n);

    if (!v)
        return;
    for (i = 0; i < n && i < MAX_DROP; i++) {
        char *L = g_drop_lab[i];
        switch (row) {
        case ROW_P1: case ROW_P2:
            snprintf(L, 48, "%2d %s", i, CharacterNames[i]); break;
        case ROW_PAL1: case ROW_PAL2:
            snprintf(L, 48, "%s", pal_name(i)); break;
        case ROW_STAGE:
            snprintf(L, 48, "%2d %s", i, stage_name(i)); break;
        case ROW_SCREEN:
            snprintf(L, 48, "%2d %s", i, FETaskNames[i] + 8); break;
        case ROW_FINISHER:
            snprintf(L, 48, "%s", tx(k_fin_es[i], k_fin_en[i])); break;
        case ROW_HEALTH:
            snprintf(L, 48, "%s", health_name(i)); break;
        case ROW_SPEED:
            snprintf(L, 48, "%d%%", k_speed[i]); break;
        case ROW_SLOT:
            snprintf(L, 48, "%d %s", i + 1, dbg_state_has(i)
                     ? tx("GUARDADO", "SAVED") : tx("VACIO", "EMPTY"));
            break;
        }
    }
    g_drop_n = i;
    for (i = 0; i < g_n && g_rows[i].kind != row; i++)
        ;
    snprintf(g_drop_title, sizeof g_drop_title, "%s", i < g_n ? g_rows[i].label : "");
    g_drop = 1;
    g_drop_kind = row;
    g_drop_cur = *v;
    g_drop_sel = *v;
    g_drop_first = 0;
}

static int drop_apply(int i)
{
    int n, *v = list_var(g_drop_kind, &n);

    g_drop = 0;
    if (!v || i < 0 || i >= n)
        return 0;
    *v = i;
    return 1;
}

/* The open list's turn: arrows and the wheel move, Enter or a click picks,
 * Esc, the pad's east button or a click outside goes back. 1 on a pick. */
static int drop_tick(int up, int down, int wheel, int ok, int back, int click)
{
    if (back) {
        g_drop = 0;
        return 0;
    }
    if (up)   g_drop_sel = wrap(g_drop_sel - 1, g_drop_n);
    if (down) g_drop_sel = wrap(g_drop_sel + 1, g_drop_n);
    if (wheel) {                        /* the wheel stops at both ends */
        g_drop_sel -= wheel;
        if (g_drop_sel < 0) g_drop_sel = 0;
        if (g_drop_sel > g_drop_n - 1) g_drop_sel = g_drop_n - 1;
    }
    if (click) {
        int i = -1;

        if (g_dgeo.rowh > 0 && g_mx >= g_dgeo.bx && g_mx < g_dgeo.bx + g_dgeo.bw
            && g_my >= g_dgeo.top) {
            i = g_drop_first + (int)((g_my - g_dgeo.top) / g_dgeo.rowh);
            if (i - g_drop_first >= VIS_DROP)
                i = -1;
        }
        if (i >= 0 && i < g_drop_n)
            return drop_apply(i);
        g_drop = 0;
        return 0;
    }
    return ok ? drop_apply(g_drop_sel) : 0;
}

/* ---------------------------------------------------------------- tick */

/* The first row that can be chosen at or after i (CLOSE, last, always can). */
static int first_on(int i)
{
    while (i < g_n - 1 && !g_rows[i].en)
        i++;
    return i;
}

static void turn_page(int d)
{
    g_page = wrap(g_page + d, N_PAGES);
    g_first = 0;
    g_drop = 0;
    build();
    g_sel = first_on(0);
}

static void select_row(int row)
{
    int i;

    for (i = 0; i < g_n; i++)
        if (g_rows[i].kind == row) {
            g_sel = i;
            return;
        }
}

void dbg_menu_after_frame(int vx, int vy, int vw, int vh, int task, int fe)
{
    if (!g_want_open)
        return;
    g_want_open = 0;
    grab(vx, vy, vw, vh);
    g_open = 1;
    g_drop = 0;
    g_task = task;
    g_mouse_was = 1;                    /* the click that opened it is not a pick */
    g_esc_was = 1;
    plat_mouse_wheel();                 /* turns made while it was shut */
    if (fe >= 0 && fe < DBG_SCREENS)
        g_screen = fe;
    build();
    if (g_sel >= g_n)
        g_sel = first_on(0);
    /* in a fight that can take a fight key or a finisher, start there */
    if (task == 6 && (row_enabled(ROW_WIN_ROUND) || row_enabled(ROW_FINISHER))) {
        g_page = PAGE_FIGHT;
        build();
        select_row(row_enabled(ROW_FINISHER) ? ROW_FINISHER : ROW_WIN_ROUND);
    }
    printf("debug menu: open (task %d, screen %d)\n", task, fe);
}

/* Key edges, read here so the menu never reaches the fight's input. */
static int edge(int k, int *was)
{
    int down = plat_key(k), hit = down && !*was;
    *was = down;
    return hit;
}

/* The mouse: game_main.c passes the pointer as fractions of the view each
 * frame the menu is open. A click on a category opens it; on a row with
 * values it opens the list, on an ON / OFF row it switches it, on any other
 * row it picks. The wheel moves the cursor. */

void dbg_menu_mouse(float x, float y, int down)
{
    g_mx = x * g_ow;
    g_my = y * 320.0f;
    g_mdown = down;
}

/* The pad, passed in by game_main.c each frame the menu is open: d-pad or
 * stick to move, south (cross / A) to pick, L1 / R1 to change the category,
 * east (circle / B) to go back. */
static PLAT_PAD g_pad, g_pad_was;

/* The footer shows the pictures of what the player is using: the pad or
 * the keyboard (and mouse), not both. */
static int g_using_pad;

void dbg_menu_pad(const PLAT_PAD *p)
{
    g_pad = *p;
    if (p->connected && (p->buttons || p->up || p->down || p->left || p->right))
        g_using_pad = 1;
    else if (!p->connected || plat_key_any())
        g_using_pad = 0;
}

#define PAD_EDGE(f)  (g_pad.f && !g_pad_was.f)
#define PAD_HIT(b)   ((g_pad.buttons & ~g_pad_was.buttons) & (1u << (b)))

int dbg_menu_tick(struct dbg_request *rq)
{
    static int was[11];
    int up, down, left, right, ok, prev, next, back, click, wheel, row;

    memset(rq, 0, sizeof *rq);
    up    = edge(PK_UP, &was[0])    | edge(PK_P2_UP, &was[1])    | PAD_EDGE(up);
    down  = edge(PK_DOWN, &was[2])  | edge(PK_P2_DOWN, &was[3])  | PAD_EDGE(down);
    left  = edge(PK_LEFT, &was[4])  | edge(PK_P2_LEFT, &was[5])  | PAD_EDGE(left);
    right = edge(PK_RIGHT, &was[6]) | edge(PK_P2_RIGHT, &was[7]) | PAD_EDGE(right);
    ok    = edge(PK_OK, &was[8]) | (PAD_HIT(PB_SOUTH) != 0);
    prev  = edge(PK_DBG_PAGE_PREV, &was[9]) | (PAD_HIT(PB_L1) != 0);
    next  = edge(PK_DBG_PAGE_NEXT, &was[10]) | (PAD_HIT(PB_R1) != 0);
    back  = edge(PK_SETTINGS, &g_esc_was) | (PAD_HIT(PB_EAST) != 0);
    g_pad_was = g_pad;
    click = g_mdown && !g_mouse_was;
    g_mouse_was = g_mdown;
    wheel = plat_mouse_wheel();

    build();
    if (g_sel >= g_n)
        g_sel = g_n - 1;
    if (!g_rows[g_sel].en)
        g_sel = first_on(g_sel);

    if (g_drop) {
        /* a screen or a finisher picked from its list is done at once */
        row = g_drop_kind;
        if (!drop_tick(up, down, wheel, ok, back, click)
            || (row != ROW_SCREEN && row != ROW_FINISHER))
            return 0;
        rq->what = row == ROW_SCREEN ? DBG_SCREEN : DBG_FINISHER;
        rq->a = row == ROW_SCREEN ? g_screen : g_fin;
        g_open = 0;
        return 1;
    }
    if (back) {
        dbg_menu_toggle();
        return 0;
    }
    if (prev || next)
        turn_page(next ? 1 : -1);

    if (click && g_geo.srow > 0 && g_mx >= g_geo.sx && g_mx < g_geo.sx + g_geo.sw
        && g_my >= g_geo.stop && g_my < g_geo.stop + N_PAGES * g_geo.srow) {
        int t = (int)((g_my - g_geo.stop) / g_geo.srow);   /* a category */
        if (t != g_page)
            turn_page(t - g_page);
    } else if (click && g_geo.rowh > 0 && g_mx >= g_geo.bx
               && g_mx < g_geo.bx + g_geo.bw && g_my >= g_geo.top) {
        int i = g_first + (int)((g_my - g_geo.top) / g_geo.rowh);
        if (i < g_n && i - g_first < VIS_ROWS && g_rows[i].en) {
            g_sel = i;
            ok = 1;
        }
    }

    if (up || down) {
        int i;
        for (i = 0; i < g_n; i++) {     /* skip the greyed rows */
            g_sel = wrap(g_sel + (up ? -1 : 1), g_n);
            if (g_rows[g_sel].en)
                break;
        }
    }
    for (; wheel; wheel += wheel > 0 ? -1 : 1) {    /* no wrap at the ends */
        int i = g_sel, d = wheel > 0 ? -1 : 1;
        for (i += d; i >= 0 && i < g_n && !g_rows[i].en; i += d)
            ;
        if (i >= 0 && i < g_n)
            g_sel = i;
    }
    row = g_rows[g_sel].kind;
    if (left || right)
        change(row, right ? 1 : -1);
    if (!ok || !row_enabled(row))
        return 0;

    if (is_toggle(row)) {
        change(row, 1);
        return 0;
    }
    {
        int n;
        if (list_var(row, &n)) {
            drop_open(row);
            return 0;
        }
    }
    switch (row) {
    case ROW_FIGHT:
        rq->what = DBG_FIGHT;
        rq->a = g_p1; rq->b = g_p2; rq->c = g_stage;
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
    case ROW_MOTARO: case ROW_SK:
        rq->what = DBG_BOSS;
        rq->a = row == ROW_MOTARO ? 24 : 25;
        break;
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

static void footer_text(char *f, size_t n, int rows)
{
    char p[96], q[96], c[96], o[96], pos[32];

    pos[0] = 0;
    if (g_n > VIS_ROWS)
        snprintf(pos, sizeof pos, "%d-%d / %d   ", g_first + 1, g_first + rows, g_n);
    if (g_using_pad) {                  /* only the pad's pictures */
        int k = plat_pad_glyph_kind(0, g_pad.kind);
        plat_pad_glyph(k, PB_L1, p, sizeof p);
        plat_pad_glyph(k, PB_R1, q, sizeof q);
        plat_pad_glyph(k, PB_SOUTH, o, sizeof o);
        plat_pad_glyph(k, PB_EAST, c, sizeof c);
    } else {                            /* only the keyboard's */
        plat_key_glyph(PK_DBG_PAGE_PREV, p, sizeof p);
        plat_key_glyph(PK_DBG_PAGE_NEXT, q, sizeof q);
        plat_key_glyph(PK_TEST, c, sizeof c);
        plat_key_glyph(PK_OK, o, sizeof o);
    }
    if (g_drop && !g_using_pad)         /* the list goes back with Esc */
        plat_key_glyph(PK_SETTINGS, c, sizeof c);
    if (g_drop)
        snprintf(f, n, "%s %s   %s %s", o, tx("ELIGE", "PICK"),
                 c, tx("VUELVE", "BACK"));
    else
        snprintf(f, n, "%s%s%s %s   %s %s   %s %s", pos, p, q,
                 tx("CATEGORIA", "CATEGORY"), o, tx("ELIGE", "PICK"), c,
                 tx("CIERRA", "CLOSES"));
}

/* Points the next boxes are moved right by, for a picture beside them
 * (the settings menu's profile icon); 0 centres them. */
float ui_menu_shift;
int ui_menu_layer;

/* the menu under an open list, dimmed */
static void dim_all(void)
{
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0, 0, 0, 0.78f);
    glBegin(GL_QUADS);
    glVertex2f(0, 0);
    glVertex2f(g_ow, 0);
    glVertex2f(g_ow, 320);
    glVertex2f(0, 320);
    glEnd();
}

/* Any menu box over the frozen frame: the debug menu's and the settings
 * menu's (settings_menu.c). In the iPhone OS 3 alert dress the in-game
 * alerts wear (plat_ui_menu, runtime/platform/win32_gl.c), drawn at the
 * game view's own pixel size so the text is sharp, and redrawn only when
 * something in it changes; the 5x7 font when the backend has no such
 * drawing. `geo` gets where the box went, in the menu's space, for the
 * mouse. */
/* The box's width in points: 360, or with the side panel as much as the
 * screen gives up to 464 (the 4:3 screen is 427 wide). */
static float ui_box_width(int ntabs)
{
    float w = g_ow - 12.0f;

    if (ntabs >= 0)
        return 360.0f;
    return w > 464.0f ? 464.0f : w;
}

static int draw_ios(const char *title, const char *const *tabs, int ntabs,
                    int page, const char *const *lp, const char *const *vp,
                    const int *en, int rows, int sel, const char *footer)
{
    /* one cache for the menu, one for a list over it (ui_menu_layer) */
    static GLuint texs[2];
    static char lasts[2][4096];
    static int tws[2], ths[2];
    static float tss[2];
    int L = ui_menu_layer ? 1 : 0;
    GLuint tex = texs[L];
    char *last = lasts[L];
    int tw = tws[L], th = ths[L];
    float ts = tss[L];
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
                                         en, rows, sel, footer, ui_box_width(ntabs),
                                         s, &w, &h);
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
        tws[L] = tw = w;
        ths[L] = th = h;
        tss[L] = ts = s;
        texs[L] = tex;
        strcpy(last, sig);
    }

    begin_2d();
    if (L)
        dim_all();
    else if (g_shot) {                  /* the frozen frame, dimmed */
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
        float x = (g_ow - w) * 0.5f + ui_menu_shift, y = (320 - h) * 0.5f;

        /* plat_ui_menu's layout, in points: a 6 margin, the title band
         * 22 tall, the tab strip 20 (inset 9 at the sides), a 4 gap, rows
         * 15 tall inside a box 360 wide */
        g_bx = x + 6 + 9;
        g_bw = 360 - 18;
        g_title0 = y + 6 + 22;
        g_top = y + 6 + 22 + (ntabs > 0 ? 20 + 4 : 4);
        g_rowh = 15;
        g_sx = g_sw = g_stop = g_srow = 0;
        if (ntabs < 0) {                /* the side panel's column */
            g_bx = x + 6 + UI_SIDE + 6;
            g_bw = ui_box_width(ntabs) - UI_SIDE - 6 - 9;
            g_sx = x + 6 + 6;
            g_sw = UI_SIDE - 10;
            g_stop = g_top + 4;
            g_srow = 24;
        }

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
        geo->sx = g_sx;
        geo->sw = g_sw;
        geo->stop = g_stop;
        geo->srow = g_srow;
    }
}

void ui_grab(int vx, int vy, int vw, int vh) { grab(vx, vy, vw, vh); }

void ui_nav_frame(float x, float y, float w, float h, float sh, int p2)
{
    static float t;
    float k = sh > 0 ? 320.0f / sh : 1.0f, pulse, g;
    int i;

    t += 1.0f / 60.0f;
    pulse = 0.65f + 0.35f * sinf(t * 6.0f);
    x *= k; y *= k; w *= k; h *= k;
    begin_2d();
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBegin(GL_QUADS);                  /* a faint fill */
    if (p2)                             /* player two's: green */
        glColor4f(0.3f, 1.0f, 0.45f, 0.12f * pulse);
    else
        glColor4f(0.3f, 0.55f, 1.0f, 0.12f * pulse);
    rect(x, y, w, h);
    glEnd();
    for (i = 4; i >= 0; i--) {          /* the glow, then the line */
        float lw = i ? 1.5f : 2.0f;
        g = (float)i * 1.5f;
        if (p2)
            glColor4f(i ? 0.3f : 0.7f, 1.0f, i ? 0.4f : 0.75f,
                      (i ? 0.22f : 1.0f) * pulse);
        else
            glColor4f(i ? 0.3f : 1.0f, i ? 0.6f : 1.0f, 1.0f,
                      (i ? 0.22f : 1.0f) * pulse);
        glBegin(GL_QUADS);
        rect(x - g, y - g, w + 2 * g, lw);
        rect(x - g, y + h + g - lw, w + 2 * g, lw);
        rect(x - g, y - g, lw, h + 2 * g);
        rect(x + w + g - lw, y - g, lw, h + 2 * g);
        glEnd();
    }
    end_2d();
}

void ui_grid_show(const char *title, const unsigned char *const *cells,
                  int n, int cols, int cpx, float cell, int sel,
                  const char *footer, unsigned key, struct ui_grid_geo *geo)
{
    static GLuint tex;
    static unsigned last;
    static int tw, th;
    static float ts;
    GLint view[4];
    float s, w, h, x, y;

    glGetIntegerv(GL_VIEWPORT, view);
    s = view[3] / 320.0f;
    if (s <= 0)
        return;
    key = (key ^ (unsigned)(s * 1000.0f)) * 16777619u ^ (unsigned)sel;
    if (!tex || key != last) {
        unsigned char *px = plat_ui_grid(title, cells, n, cols, cpx, cell, sel,
                                         footer, s, &tw, &th);
        if (!px)
            return;
        if (!tex)
            glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tw, th, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, px);
        free(px);
        ts = s;
        last = key;
    }
    w = tw / ts;
    h = th / ts;
    x = (g_ow - w) * 0.5f;
    y = (320 - h) * 0.5f;
    begin_2d();
    dim_all();
    glEnable(GL_TEXTURE_2D);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);        /* premultiplied */
    glBindTexture(GL_TEXTURE_2D, tex);
    glColor4f(1, 1, 1, 1);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(x, y);
    glTexCoord2f(1, 0); glVertex2f(x + w, y);
    glTexCoord2f(1, 1); glVertex2f(x + w, y + h);
    glTexCoord2f(0, 1); glVertex2f(x, y + h);
    glEnd();
    glDisable(GL_TEXTURE_2D);
    end_2d();
    if (geo) {                          /* plat_ui_grid's layout, in points */
        geo->x0 = x + 6 + 10;
        geo->y0 = y + 6 + 22 + 4;
        geo->step = cell + 4;
        geo->cell = cell;
        geo->cols = cols;
        geo->n = n;
    }
}

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

/* The player's badge in the top right corner of the menu space (the main
 * menu, game_main.c; the bottom one has the pad's hints): made again only when `key` changes. */
void ui_badge(const char *name, const unsigned char *icon, int icon_px,
              unsigned key)
{
    static GLuint tex;
    static unsigned last;
    static int tw, th;
    static float ts;
    GLint view[4];
    float s, w, h;

    glGetIntegerv(GL_VIEWPORT, view);
    s = view[3] / 320.0f;
    if (s <= 0)
        return;
    key ^= (unsigned)(s * 1000.0f);
    if (!tex || key != last) {
        unsigned char *px = plat_ui_badge(name, icon, icon_px, s, &tw, &th);
        if (!px)
            return;
        if (!tex)
            glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tw, th, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, px);
        free(px);
        ts = s;
        last = key;
    }
    begin_2d();
    w = tw / ts;
    h = th / ts;
    if (g_ow < 500.0f) {                /* 4:3 and 3:2: smaller, clear of
                                         * the logo */
        w *= 0.6f;
        h *= 0.6f;
    }
    glEnable(GL_TEXTURE_2D);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);        /* premultiplied */
    glBindTexture(GL_TEXTURE_2D, tex);
    glColor4f(1, 1, 1, 1);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(g_ow - w - 2, 2);
    glTexCoord2f(1, 0); glVertex2f(g_ow - 2, 2);
    glTexCoord2f(1, 1); glVertex2f(g_ow - 2, h + 2);
    glTexCoord2f(0, 1); glVertex2f(g_ow - w - 2, h + 2);
    glEnd();
    glDisable(GL_TEXTURE_2D);
    end_2d();
}

/* A badge as ui_badge draws it, anywhere (port: the ONLINE screens,
 * online_ui.c). Four slots, each with its own picture, made again when its
 * `key` changes. (x, y) in the 320-high menu space; `align` 0 puts the left
 * edge at x, 1 the centre, 2 the right edge; `size` scales it. */
void ui_badge_at(int slot, const char *name, const unsigned char *icon,
                 int icon_px, unsigned key, float x, float y, int align,
                 float size)
{
    static GLuint tex[4];
    static unsigned last[4];
    static int tw[4], th[4];
    static float ts[4];
    GLint view[4];
    float s, w, h;

    if (slot < 0 || slot > 3)
        return;
    glGetIntegerv(GL_VIEWPORT, view);
    s = view[3] / 320.0f * size;
    if (s <= 0)
        return;
    key ^= (unsigned)(s * 1000.0f);
    if (!tex[slot] || key != last[slot]) {
        unsigned char *px = plat_ui_badge(name, icon, icon_px, s,
                                          &tw[slot], &th[slot]);
        if (!px)
            return;
        if (!tex[slot])
            glGenTextures(1, &tex[slot]);
        glBindTexture(GL_TEXTURE_2D, tex[slot]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tw[slot], th[slot], 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, px);
        free(px);
        ts[slot] = s / size;
        last[slot] = key;
    }
    begin_2d();
    w = tw[slot] / ts[slot];
    h = th[slot] / ts[slot];
    if (align == 1)
        x -= w * 0.5f;
    else if (align == 2)
        x -= w;
    glEnable(GL_TEXTURE_2D);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);        /* premultiplied */
    glBindTexture(GL_TEXTURE_2D, tex[slot]);
    glColor4f(1, 1, 1, 1);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(x, y);
    glTexCoord2f(1, 0); glVertex2f(x + w, y);
    glTexCoord2f(1, 1); glVertex2f(x + w, y + h);
    glTexCoord2f(0, 1); glVertex2f(x, y + h);
    glEnd();
    glDisable(GL_TEXTURE_2D);
    end_2d();
}

/* A rectangle's outline, `t` thick, in the 320-high menu space (port: the
 * ONLINE screen's code box). */
void ui_rect_outline(float x, float y, float w, float h, float t,
                     float r, float g, float b, float a)
{
    begin_2d();
    glColor4f(r, g, b, a);
    glBegin(GL_QUADS);
    glVertex2f(x, y);         glVertex2f(x + w, y);
    glVertex2f(x + w, y + t); glVertex2f(x, y + t);
    glVertex2f(x, y + h - t); glVertex2f(x + w, y + h - t);
    glVertex2f(x + w, y + h); glVertex2f(x, y + h);
    glVertex2f(x, y);         glVertex2f(x + t, y);
    glVertex2f(x + t, y + h); glVertex2f(x, y + h);
    glVertex2f(x + w - t, y); glVertex2f(x + w, y);
    glVertex2f(x + w, y + h); glVertex2f(x + w - t, y + h);
    glEnd();
    end_2d();
}

/* The menu space's width (320 high, the view's shape). */
float ui_space_width(void)
{
    GLint view[4];

    glGetIntegerv(GL_VIEWPORT, view);
    return view[3] > 0 ? 320.0f * view[2] / view[3] : 480.0f;
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
    const char *tabs[N_PAGES], *lp[VIS_ROWS], *vp[VIS_ROWS];
    int en[VIS_ROWS], i, rows;
    char footer[512];

    build();
    if (g_sel < g_first)
        g_first = g_sel;
    if (g_sel >= g_first + VIS_ROWS)
        g_first = g_sel - VIS_ROWS + 1;
    if (g_first > g_n - VIS_ROWS)
        g_first = g_n - VIS_ROWS > 0 ? g_n - VIS_ROWS : 0;
    if (g_first > 0 && g_first == g_sel && g_rows[g_sel - 1].kind == ROW_HEAD)
        g_first--;                      /* a row's heading stays in sight */
    rows = g_n - g_first < VIS_ROWS ? g_n - g_first : VIS_ROWS;
    for (i = 0; i < rows; i++) {
        lp[i] = g_rows[g_first + i].label;
        vp[i] = g_rows[g_first + i].value;
        en[i] = g_rows[g_first + i].en;
    }
    tabs[PAGE_FIGHT] = tx("PELEA", "FIGHT");
    tabs[PAGE_MENUS] = tx("MENUS", "MENUS");
    tabs[PAGE_TOOLS] = tx("OPCIONES", "TOOLS");   /* fits the panel */
    footer_text(footer, sizeof footer, rows);
    /* the categories down the side panel, as the settings menu's */
    ui_menu_show(tx("MENU DEBUG", "DEBUG MENU"), tabs, -N_PAGES, g_page, lp, vp,
                 en, rows, g_sel - g_first, footer, &g_geo);
    if (g_drop) {                       /* the open list, over the menu */
        const char *dl[VIS_DROP], *dv[VIS_DROP];
        int de[VIS_DROP], dn;

        if (g_drop_sel < g_drop_first)
            g_drop_first = g_drop_sel;
        if (g_drop_sel >= g_drop_first + VIS_DROP)
            g_drop_first = g_drop_sel - VIS_DROP + 1;
        dn = g_drop_n - g_drop_first < VIS_DROP ? g_drop_n - g_drop_first : VIS_DROP;
        for (i = 0; i < dn; i++) {
            dl[i] = g_drop_lab[g_drop_first + i];
            dv[i] = g_drop_first + i == g_drop_cur ? tx("ACTUAL", "CURRENT") : "";
            de[i] = 1;
        }
        ui_menu_layer = 1;
        ui_menu_show(g_drop_title, NULL, 0, 0, dl, dv, de, dn,
                     g_drop_sel - g_drop_first, footer, &g_dgeo);
        ui_menu_layer = 0;
    }
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

void dbg_info_draw_at(const char *line, int row)
{
    float y = row * (CH_H + 2.0f);

    begin_2d();
    glBegin(GL_QUADS);
    glColor4f(0.0f, 0.0f, 0.0f, 0.6f);
    rect(0, y, (float)strlen(line) * CH_W + 4, CH_H + 2);
    glColor4f(0.4f, 1.0f, 0.4f, 1.0f);
    text(2, y + 2, 1.0f, line);
    glEnd();
    end_2d();
}

void dbg_info_draw(const char *line) { dbg_info_draw_at(line, 0); }
