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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#include "platform/platform.h"
#include "platform/gl.h"
#include "debug_menu.h"
#include "profiles.h"
extern int rgl_ao_debug;                /* render/rgl.h: AMBIENT OCCLUSION's debug views */
static const char *const k_aodbg_es[6];   /* the debug views' names, below */
static const char *const k_aodbg_en[6];
#include "rollback.h"
#include "mp_net.h"

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
    ROW_P1, ROW_PAL1, ROW_P2, ROW_PAL2, ROW_STAGE, ROW_MODE, ROW_DIFF, ROW_FIGHT,
    ROW_SCREEN, ROW_MAIN,
    ROW_WIN_ROUND, ROW_LOSE_ROUND, ROW_WIN_MATCH, ROW_LOSE_MATCH,
    ROW_FINISHER,
    ROW_MOTARO, ROW_SK,
    ROW_HEALTH, ROW_CLOCK,
    ROW_KEYS, ROW_INFO, ROW_FPS, ROW_NET,
    ROW_HITBOX, ROW_AODBG, ROW_SPEED, ROW_SLOT, ROW_SAVE, ROW_LOAD,
    ROW_UNLOCK,
    ROW_CLOSE, ROW_HEAD
};

/* UNLOCK EVERYTHING (0.4.4, Mary: to test the Lost Treasures): the four
 * hidden fighters, the ten treasures and the 23 endings, as the save keeps
 * them (GameCode.c Write_SaveData: 1 = won), then written to `savedata`.
 * Write_SaveData only writes in GameMode 0, so the mode is lent for it. */
extern long ClassicSubZeroUnlocked, ErmacUnlocked, MileenaUnlocked, JadeUnlocked;
extern int  TreasureGained[10];
extern long EndingsGained[23];
extern long GameMode;
void Write_SaveData(void);
static int g_unlocked;                  /* done this session: the row says so */

static int all_unlocked(void)
{
    int i;
    if (!ClassicSubZeroUnlocked || !ErmacUnlocked || !MileenaUnlocked || !JadeUnlocked)
        return 0;
    for (i = 0; i < 10; i++)
        if (TreasureGained[i] != 1)
            return 0;
    for (i = 0; i < 23; i++)
        if (EndingsGained[i] != 1)
            return 0;
    return 1;
}

static void unlock_all(void)
{
    long mode = GameMode;
    int i;

    ClassicSubZeroUnlocked = ErmacUnlocked = MileenaUnlocked = JadeUnlocked = 1;
    for (i = 0; i < 10; i++)
        TreasureGained[i] = 1;
    for (i = 0; i < 23; i++)
        EndingsGained[i] = 1;
    GameMode = 0;
    Write_SaveData();
    GameMode = mode;
    g_unlocked = 1;
    printf("debug menu: everything unlocked (fighters, treasures, endings), savedata written\n");
}

#define MAX_ROWS 40
#define VIS_ROWS 30                     /* two columns of 15 */
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
/* START FIGHT's mode (0 the menu's own, 1 ARCADE, 2 TRAINING: player two
 * stands still) and the CPU's difficulty (0 the ladder's, else 1 + 0..9,
 * RoundParam[3]: Task_GameInit) -- for Mary and for the scripted runs */
static int g_mode, g_diff;
static const char *mode_name(int i)
{
    static const char *const es[3] = { "EL DEL MENU", "ARCADE", "ENTRENAMIENTO" };
    static const char *const en[3] = { "THE MENU'S", "ARCADE", "TRAINING" };
    return tx(es[i % 3], en[i % 3]);
}
static const char *diff_name(int i)
{
    static char b[8];
    if (i <= 0)
        return tx("LA DEL ARCADE", "THE LADDER'S");
    snprintf(b, sizeof b, "%d", i - 1);
    return b;
}
int dbg_fight_mode(void)     { return g_mode; }
int dbg_cpu_difficulty(void) { return g_diff - 1; }
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
static float g_bx, g_bw, g_top, g_rowh, g_title0, g_cw, g_gap;
static int   g_split, g_tall;         /* the left column's rows, the taller's */
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
    /* a 32-bit word, as the engine stores it: read as an 8-byte pointer
     * it crashed the 64-bit build as soon as the menu opened */
    const char *s = *(const char *P32 const *)(Level_Info + i * LEVEL_INFO_STRIDE + 0x1c);
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
        add(ROW_MODE, tx("MODO", "MODE"), "< %s >", mode_name(g_mode));
        add(ROW_DIFF, tx("DIFICULTAD CPU (0-9)", "CPU DIFFICULTY (0-9)"), "< %s >",
            diff_name(g_diff));
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
        head(tx("DESBLOQUEOS", "UNLOCKS"));
        add(ROW_UNLOCK, tx("DESBLOQUEAR TODO", "UNLOCK EVERYTHING"), "%s",
            g_unlocked || all_unlocked() ? tx("HECHO", "DONE")
                                         : tx("LUCHADORES, TESOROS, FINALES", "FIGHTERS, TREASURES, ENDINGS"));
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
        {   /* AMBIENT OCCLUSION's debug views (render/rgl.h rgl_ao_debug) */
            int v = rgl_ao_debug < 0 || rgl_ao_debug > 5 ? 0 : rgl_ao_debug;
            add(ROW_AODBG, tx("VISTA DE OCLUSION AMBIENTAL", "AMBIENT OCCLUSION VIEW"), "< %s >",
                tx(k_aodbg_es[v], k_aodbg_en[v]));
        }
        {
            char bar[24], pc[12];
            snprintf(pc, sizeof pc, "%d%%", k_speed[g_speed]);
            add(ROW_SPEED, tx("VELOCIDAD DE LA PELEA", "FIGHT SPEED"), "%s",
                ui_bar_value(bar, sizeof bar, (float)g_speed / (N_SPEEDS - 1), pc));
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
    case ROW_MODE:     *n = 3;               return &g_mode;
    case ROW_DIFF:     *n = 11;              return &g_diff;
    case ROW_HEALTH:   *n = 4;               return &g_health;
    case ROW_SPEED:    *n = N_SPEEDS;        return &g_speed;
    case ROW_SLOT:     *n = DBG_STATE_SLOTS; return &g_slot;
    case ROW_AODBG:    *n = 6;               return &rgl_ao_debug;
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

/* AMBIENT OCCLUSION's debug views, for the row and its list */
static const char *const k_aodbg_es[6] = { "NO", "AO (TRAS EL DESENFOQUE)", "AO (SIN DESENFOQUE)",
                                           "PROFUNDIDAD", "NORMALES", "CONTRIBUCION FINAL" };
static const char *const k_aodbg_en[6] = { "OFF", "AO (AFTER THE BLUR)", "AO (BEFORE THE BLUR)",
                                           "DEPTH", "NORMALS", "FINAL CONTRIBUTION" };

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
        case ROW_MODE:
            snprintf(L, 48, "%s", mode_name(i)); break;
        case ROW_DIFF:
            snprintf(L, 48, "%s", diff_name(i)); break;
        case ROW_SPEED:
            snprintf(L, 48, "%d%%", k_speed[i]); break;
        case ROW_SLOT:
            snprintf(L, 48, "%d %s", i + 1, dbg_state_has(i)
                     ? tx("GUARDADO", "SAVED") : tx("VACIO", "EMPTY"));
            break;
        case ROW_AODBG:                 /* the list had no names (Mary) */
            snprintf(L, 48, "%s", tx(k_aodbg_es[i], k_aodbg_en[i]));
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

    /* FIGHT SPEED's bar: clicked or dragged along */
    if (g_mdown && ui_geo_bar(&g_geo, g_mx, g_my, VIS_ROWS) >= 0
        && g_first + ui_geo_row(&g_geo, g_mx, g_my, VIS_ROWS) < g_n
        && g_rows[g_first + ui_geo_row(&g_geo, g_mx, g_my, VIS_ROWS)].kind == ROW_SPEED) {
        g_sel = g_first + ui_geo_row(&g_geo, g_mx, g_my, VIS_ROWS);
        g_speed = (int)(ui_geo_bar(&g_geo, g_mx, g_my, VIS_ROWS) * (N_SPEEDS - 1) + 0.5f);
        click = 0;
    }
    if (click && g_geo.srow > 0 && g_mx >= g_geo.sx && g_mx < g_geo.sx + g_geo.sw
        && g_my >= g_geo.stop && g_my < g_geo.stop + N_PAGES * g_geo.srow) {
        int t = (int)((g_my - g_geo.stop) / g_geo.srow);   /* a category */
        if (t != g_page)
            turn_page(t - g_page);
    } else if (click && ui_geo_row(&g_geo, g_mx, g_my, VIS_ROWS) >= 0) {
        int i = g_first + ui_geo_row(&g_geo, g_mx, g_my, VIS_ROWS);
        if (i < g_n && g_rows[i].en) {
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
    if (row == ROW_UNLOCK) {            /* done here; the menu stays open */
        unlock_all();
        return 0;
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
int ui_menu_cols = 1;

int ui_geo_row(const struct ui_geo *g, float x, float y, int rows)
{
    int c, r, split = g->split > 0 && g->split < rows ? g->split : rows;
    float cw = g->cw > 0 ? g->cw : g->bw;

    if (g->rowh <= 0 || x < g->bx || x >= g->bx + g->bw || y < g->top)
        return -1;
    c = split < rows ? (int)((x - g->bx) / (cw + g->gap)) : 0;
    r = (int)((y - g->top) / g->rowh);
    if (c > 1 || x - g->bx - c * (cw + g->gap) >= cw)
        return -1;                      /* the gap */
    if (c == 0)
        return r < split ? r : -1;
    return split + r < rows ? split + r : -1;
}

float ui_geo_bar(const struct ui_geo *g, float x, float y, int rows)
{
    int r = ui_geo_row(g, x, y, rows), split = g->split > 0 && g->split < rows ? g->split : rows;
    float cw = g->cw > 0 ? g->cw : g->bw, c0, x0, x1, f;

    if (r < 0)
        return -1.0f;
    c0 = g->bx + (r >= split ? cw + g->gap : 0.0f);
    x0 = c0 + cw * UI_BAR_X0;
    x1 = c0 + cw * UI_BAR_X1;
    if (x < x0 - cw * 0.05f || x > x1 + cw * 0.05f)
        return -1.0f;
    /* the well is the row's lower half, under its label (ui_alert.c) */
    if (y < g->top + (float)(r >= split ? r - split : r) * g->rowh + g->rowh * 0.5f)
        return -1.0f;
    f = (x - x0) / (x1 - x0);
    return f < 0 ? 0 : f > 1 ? 1 : f;
}

const char *ui_bar_value(char *buf, int n, float share, const char *text)
{
    int p = (int)(share * 100.0f + 0.5f);

    snprintf(buf, (size_t)n, "\003%03d%s", p < 0 ? 0 : p > 100 ? 100 : p, text);
    return buf;
}

/* Where the right column starts: at a section's heading ("\002"), so no
 * section is cut in two, the one that leaves the columns most even; half
 * way when there is none or a column would not fit (16 rows). */
int ui_menu_split(const char *const *label, int rows)
{
    int i, best = 0, tall = rows;

    for (i = 1; i < rows; i++)
        if (label[i][0] == 2) {
            int t = i > rows - i ? i : rows - i;
            if (t < tall) {
                tall = t;
                best = i;
            }
        }
    if (!best && rows <= 16)
        return rows;                    /* one section: one column */
    if (!best || tall > 16)
        return (rows + 1) / 2;
    return best;
}

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
/* the width of the menu's space that is seen: the picture's shape, inside
 * the window's (a 4:3 picture in a 16:9 window shows its middle 427) */
static float g_vis = 480.0f;
float port_aspect(void);                /* game_main.c */

static float ui_box_width(int ntabs)
{
    float w = g_vis - 12.0f;

    if (ntabs >= 0 && ui_menu_cols < 2)
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
    float s, fit;

    glGetIntegerv(GL_VIEWPORT, view);
    s = view[3] / 320.0f;               /* by height: the shape is kept */
    if (s <= 0)
        return 0;
    /* the space's width from this viewport before the box is sized from it:
     * g_ow was still the last 2D pass's (the whole 16:9 window around a 4:3
     * picture), and the box came out 464 wide on a 427 screen */
    g_ow = view[3] > 0 ? 320.0f * view[2] / view[3] : 480.0f;
    /* the menu is drawn over the whole window, but a 4:3 picture shows
     * only its middle: the box is sized from what is seen */
    g_vis = 320.0f * port_aspect();
    if (g_vis > g_ow || g_vis <= 0)
        g_vis = g_ow;
    /* and should the box still be wider, all of it drawn smaller to fit,
     * its pictures as sharp; on a picture narrower than 3:2 a little
     * smaller still, so it does not run to the edges */
    fit = g_vis * (g_vis < 470.0f ? 0.9f : 1.0f) / (ui_box_width(ntabs) + 12.0f);
    if (fit > 1.0f)
        fit = 1.0f;
    s *= fit;
    g_split = ui_menu_cols > 1 ? ui_menu_split(lp, rows) : rows;
    g_tall = g_split > rows - g_split ? g_split : rows - g_split;
    n = snprintf(sig, sizeof sig, "%s|%s|%d|%d|", title, footer, ntabs,
                 g_split);
    for (i = 0; i < ntabs && n < (int)sizeof sig - 200; i++)
        n += snprintf(sig + n, sizeof sig - n, "%s|", tabs[i]);
    for (i = 0; i < rows && n < (int)sizeof sig - 200; i++)
        n += snprintf(sig + n, sizeof sig - n, "%s=%s%d|", lp[i], vp[i], en[i]);
    snprintf(sig + n, sizeof sig - n, "%d %d %.3f", sel, page, s);

    if (!tex || strcmp(sig, last) != 0) {
        int w, h;
        unsigned char *px = plat_ui_menu(title, tabs, ntabs, page, lp, vp,
                                         en, rows, sel, footer, ui_box_width(ntabs),
                                         g_split, s, &w, &h);
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
        float w = tw * fit / ts, h = th * fit / ts, f = fit;
        float x = (g_ow - w) * 0.5f + ui_menu_shift, y = (320 - h) * 0.5f;

        /* plat_ui_menu's layout, in points: a 6 margin, the title band
         * 22 tall, the tab strip 20 (inset 9 at the sides), a 4 gap, rows
         * 15 tall inside a box 360 wide */
        g_bx = x + (6 + 9) * f;
        g_bw = (ui_box_width(ntabs) - 18) * f;
        g_title0 = y + (6 + 22) * f;
        g_top = y + (6 + 22 + (ntabs > 0 ? 20 + 4 : 4)) * f;
        g_rowh = 15 * f;
        g_sx = g_sw = g_stop = g_srow = 0;
        if (ntabs < 0) {                /* the side panel's column */
            g_bx = x + (6 + UI_SIDE + 6) * f;
            g_bw = (ui_box_width(ntabs) - UI_SIDE - 6 - 9) * f;
            g_sx = x + (6 + 6) * f;
            g_sw = (UI_SIDE - 10) * f;
            g_stop = g_top + 4 * f;
            g_srow = UI_SIDE_STEP(-ntabs) * f;
        }
        g_gap = 6 * f;                  /* the columns, 6 apart */
        g_cw = g_split < rows ? (g_bw - g_gap) * 0.5f : g_bw;

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
        g_gap = 0;
        g_split = ui_menu_cols > 1 ? ui_menu_split(lp, rows) : rows;
        g_tall = g_split > rows - g_split ? g_split : rows - g_split;
        g_cw = g_split < rows ? bw * 0.5f : bw;
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
        rect(x0 - 12, y0 - 40, bw, g_tall * lh + 66);
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
        if (sel >= 0 && sel < rows)
            rect(x0 - 8 + (sel >= g_split) * g_cw,
                 y0 + (sel - (sel >= g_split) * g_split) * lh - 4, g_cw - 8, lh - 1);

        glColor4f(1.0f, 0.85f, 0.2f, 1.0f);
        text(x0, y0 - 36, px, title);
        for (i = 0; i < rows; i++) {
            int c = i >= g_split;
            float y = y0 + (i - c * g_split) * lh, cx = x0 + c * g_cw;

            if (en[i])
                glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
            else
                glColor4f(0.45f, 0.45f, 0.45f, 1.0f);
            text(cx, y, px, lp[i]);
            if (vp[i][0])
                text(cx + (g_split < rows ? 70 : 100), y, px,
                     vp[i][0] == 3 && vp[i][1] && vp[i][2] && vp[i][3] ? vp[i] + 4 : vp[i]);
        }
        glColor4f(0.7f, 0.7f, 0.7f, 1.0f);
        text(x0, y0 + g_tall * lh + 6, px, footer);
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
        geo->cw = g_cw;
        geo->gap = g_gap;
        geo->split = g_split;
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

/* PROFILE's gradient picker over the menu, dimmed, as the launcher draws it
 * (plat_ui_gradient): sized so its 440 CSS pixels fit the menu's space,
 * made again only when `key` changes. `hit` gets the controls' boxes in the
 * menu's points (the dial: centre and radius), for the mouse. */
void ui_gradient_show(const struct plat_grad *g, unsigned key, struct plat_grad_hit *hit)
{
    static GLuint tex;
    static unsigned last;
    static int tw, th;
    static float ts, tk;
    static struct plat_grad_hit th_hit;
    GLint view[4];
    float s, k, w, h, x, y, p;
    int i, j;

    glGetIntegerv(GL_VIEWPORT, view);
    s = view[3] / 320.0f;
    if (s <= 0)
        return;
    begin_2d();
    end_2d();                           /* g_ow, the menu space's width */
    /* points per CSS pixel: the panel (440 x 307) within 92% of the space */
    p = 0.92f * 320.0f / 307.0f;
    if (440.0f * p > 0.92f * g_ow)
        p = 0.92f * g_ow / 440.0f;
    k = p * s;
    key = (key ^ (unsigned)(k * 1000.0f)) * 16777619u;
    if (!tex || key != last) {
        unsigned char *px = plat_ui_gradient(g, k, &th_hit, &tw, &th);
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
        tk = k;
        last = key;
    }
    (void)tk;
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
    if (hit) {                          /* texture pixels -> the menu's points */
#define GP_BOX(b) do { (b)[0] = x + (b)[0] / ts; (b)[1] = y + (b)[1] / ts;                        (b)[2] = x + (b)[2] / ts; (b)[3] = y + (b)[3] / ts; } while (0)
        *hit = th_hit;
        GP_BOX(hit->panel);
        for (i = 0; i < 2; i++) {
            GP_BOX(hit->field[i]);
            GP_BOX(hit->button[i]);
        }
        for (j = 0; j < PLAT_GRAD_PRESETS; j++)
            GP_BOX(hit->swatch[j]);
#undef GP_BOX
        hit->dial[0] = x + hit->dial[0] / ts;
        hit->dial[1] = y + hit->dial[1] / ts;
        hit->dial[2] = hit->dial[2] / ts;
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
    static GLuint tex[8];
    static unsigned last[8];
    static int tw[8], th[8];
    static float ts[8];
    GLint view[4];
    float s, w, h;

    if (slot < 0 || slot > 7)
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

/* A picture (straight RGBA, pw x ph) at (x, y), w x h points, in
 * the 320-high menu space; uploaded again only when `key` changes. `a` its
 * opacity. Port: the ONLINE screen's portraits (online_ui.c). */
void ui_picture_at(int slot, const unsigned char *rgba, int pw, int ph, unsigned key,
                   float x, float y, float w, float h, float a)
{
    static GLuint tex[UI_PICTURES];
    static unsigned last[UI_PICTURES];

    if (slot < 0 || slot >= UI_PICTURES || w <= 0 || h <= 0 || a <= 0)
        return;
    if (!tex[slot] || key != last[slot]) {
        if (!rgba)
            return;
        if (!tex[slot])
            glGenTextures(1, &tex[slot]);
        glBindTexture(GL_TEXTURE_2D, tex[slot]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pw, ph, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, rgba);
        last[slot] = key;
    }
    begin_2d();
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindTexture(GL_TEXTURE_2D, tex[slot]);
    glColor4f(1, 1, 1, a);
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
    ui_menu_cols = 2;
    ui_menu_show(tx("MENU DEBUG", "DEBUG MENU"), tabs, -N_PAGES, g_page, lp, vp,
                 en, rows, g_sel - g_first, footer, &g_geo);
    ui_menu_cols = 1;
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

/* The ONLINE screen's skin (FrontEnd.c, PortSkin*): STAGE SELECT and the
 * replay player are drawn
 * in its materials and font, in this space's units (Mary, 0.4.2). */
void PortSkinBegin(void);
void PortSkinEnd(void);
float PortSkinTime(void);
void PortSkinPanel(float x, float y, float w, float h, float a, int red);
void PortSkinHeading(const char *title, float x, float y, float w, float a);
void PortSkinButton(const char *text, float x, float y, float w, float h,
                    int main, int st, float lit, float a);
void PortSkinText(const char *s, float x, float y, int align, float size,
                  float maxw, int kind, float a);
void PortSkinFrame(float x, float y, float w, float h, float t, int gold, float a);
void PortSkinShadow(float x, float y, float w, float h, float a);
void PortSkinGlow(float x, float y, float w, float h, float a);
void PortSkinRule(float x, float y, float w, float a);
void PortSkinChevron(float x, float y, float w, float h, int right, float a);

/* back from the skin's pieces to this file's flat quads: they leave
 * texturing on */
static void skin_end(void)
{
    PortSkinEnd();
    glDisable(GL_TEXTURE_2D);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
}

/* ------------------------------------------------------ the replay player */

/* The player's panel along the bottom (rollback_probe.c plays the fight),
 * drawn as the launcher draws its pages: a deep violet glass with faded
 * light rules, buttons with a sheen and a light rim, the one that matters
 * (play) lit. In the 2D space, 320 high: the status line, the bar, then the
 * buttons -- speed on the left, the transport in the middle, the settings
 * on the right. */

#define RPP_TOP    248.0f               /* the panel */
#define RPP_TEXT   254.0f               /* the status line */
#define RPP_BAR_Y  268.0f
#define RPP_BAR_H  6.0f
#define RPP_BTN_Y  284.0f
#define RPP_BTN_H  26.0f
#define RPP_BTN_W  32.0f
#define RPP_PLAY_W 44.0f
#define RPP_SMALL  26.0f                /* slower, faster */
#define RPP_SPEED  40.0f                /* the speed between them */
#define RPP_GAP    5.0f

static float g_rp_w = 480.0f;           /* the space's width, last drawn */
static float g_rp_h = 320.0f;           /* and its height: 320 at 100% */

/* SIZE: the player drawn at 75 to 150% (umk3.ini replay_scale); the space
 * is 320 / scale high, the panel on its bottom, the corners on its top */
int dbg_replay_pct = 100;

void dbg_replay_size(void)
{
    static const int k[4] = { 75, 100, 125, 150 };
    int i;

    for (i = 0; i < 4 && k[i] <= dbg_replay_pct; i++)
        ;
    dbg_replay_pct = k[i % 4];
}

static float rp_scale(void)
{
    int p = dbg_replay_pct < 50 ? 50 : dbg_replay_pct > 200 ? 200 : dbg_replay_pct;

    return (float)p / 100.0f;
}

static const char *const k_rp_tip[RPB_COUNT] = {
    "TO THE START", "BACK 5 SECONDS", "BACK ONE FRAME", "PLAY / PAUSE",
    "ON ONE FRAME", "ON 5 SECONDS", "SLOWER", "FASTER", "SETTINGS (ESC)"
};
static const char *const k_rp_speed[5] = { "X1/4", "X1/2", "X1", "X2", "X4" };

/* Where each button is, in a space `w` wide. */
static void rp_layout(float w, float x[RPB_COUNT], float bw[RPB_COUNT])
{
    float x0 = w * 0.04f, at;
    int i;

    for (i = 0; i < RPB_COUNT; i++)
        bw[i] = RPP_BTN_W;
    bw[RPB_PLAY] = RPP_PLAY_W;
    bw[RPB_SLOWER] = bw[RPB_FASTER] = RPP_SMALL;

    at = w / 2 - (5 * RPP_BTN_W + RPP_PLAY_W + 5 * RPP_GAP) / 2;
    for (i = RPB_START; i <= RPB_FWD5; i++) {
        x[i] = at;
        at += bw[i] + RPP_GAP;
    }
    x[RPB_SLOWER] = x0;
    x[RPB_FASTER] = x0 + RPP_SMALL + RPP_SPEED;
    x[RPB_SETTINGS] = w * 0.96f - RPP_BTN_W;
}

/* The top corners: the REPLAYS button on the left, the X on the right;
 * the list hangs under the button. */
#define RPT_BTN_Y  4.0f
#define RPT_BTN_H  20.0f
#define RPT_LIST_W 84.0f
#define RPT_X_W    24.0f
#define RPT_HIDE_W 24.0f
#define RPT_SIZE_W 44.0f
#define RPT_GAP    4.0f
#define RL_TOP     34.0f
#define RL_W       230.0f
#define RL_ROW     14.0f
#define RL_HEAD    13.0f
#define RL_MAX     16

/* Where the list's rows are: their tops in `y`, the two headings' in `h`
 * (h[2]: the box's bottom); an empty part has one "NONE" row, not counted. */
static void rl_layout(int n, int n_recent, float *y, float h[3])
{
    float at = RL_TOP + 5;
    int i;

    h[0] = at;
    at += RL_HEAD;
    for (i = 0; i < n_recent; i++, at += RL_ROW)
        y[i] = at;
    if (!n_recent)
        at += RL_ROW;
    at += 4;
    h[1] = at;
    at += RL_HEAD;
    for (; i < n; i++, at += RL_ROW)
        y[i] = at;
    if (n == n_recent)
        at += RL_ROW;
    h[2] = at + 4;
}

/* The button under (fx, fy), fractions of the view, -1 for none, RPB_BAR on
 * the bar, RPB_ROW + i on the open list's row i (`list_n` rows, the first
 * `n_recent` the recent ones; 0 when it is closed); `at` is where along the
 * bar, 0..1, wherever the mouse is. */
int dbg_replay_hit(float fx, float fy, float *at, int list_n, int n_recent)
{
    float x[RPB_COUNT], bw[RPB_COUNT], w = g_rp_w;
    float px = fx * w, py = fy * g_rp_h, b0 = w * 0.04f, b1 = w * 0.96f;
    float ry[RL_MAX], rh[3], hx = b1 - RPT_X_W - RPT_GAP - RPT_HIDE_W;
    int i;

    *at = (px - b0) / (b1 - b0);
    if (py >= RPT_BTN_Y && py <= RPT_BTN_Y + RPT_BTN_H) {
        if (px >= b0 && px <= b0 + RPT_LIST_W)
            return RPB_LIST;
        if (px >= b1 - RPT_X_W && px <= b1)
            return RPB_CLOSE;
        if (px >= hx && px <= hx + RPT_HIDE_W)
            return RPB_HIDE;
        if (px >= hx - RPT_GAP - RPT_SIZE_W && px <= hx - RPT_GAP)
            return RPB_SIZE;
    }
    if (list_n > 0) {
        if (list_n > RL_MAX)
            list_n = RL_MAX;
        rl_layout(list_n, n_recent, ry, rh);
        if (px >= b0 && px <= b0 + RL_W)
            for (i = 0; i < list_n; i++)
                if (py >= ry[i] && py < ry[i] + RL_ROW)
                    return RPB_ROW + i;
    }
    py -= g_rp_h - 320.0f;              /* the panel: on the bottom */
    if (py >= RPP_BAR_Y - 7 && py <= RPP_BAR_Y + RPP_BAR_H + 7
        && px >= b0 - 6 && px <= b1 + 6)
        return RPB_BAR;
    rp_layout(w, x, bw);
    if (py >= RPP_BTN_Y && py <= RPP_BTN_Y + RPP_BTN_H)
        for (i = 0; i < RPB_COUNT; i++)
            if (px >= x[i] && px <= x[i] + bw[i])
                return i;
    return -1;
}

/* A box, its top colour fading to its bottom one. */
static void vgrad(float x, float y, float w, float h, const float *a, const float *b)
{
    glColor4f(a[0], a[1], a[2], a[3]);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glColor4f(b[0], b[1], b[2], b[3]);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
}

/* A light rule fading in at its left and out at its right, as --rim-fade. */
static void rule(float x, float y, float w, float r, float g, float b, float a)
{
    float s[5] = { x, x + w * 0.2f, x + w * 0.8f, x + w };
    int i;

    for (i = 0; i < 3; i++) {
        float a0 = i == 0 ? 0 : a, a1 = i == 2 ? 0 : a;
        glColor4f(r, g, b, a0);
        glVertex2f(s[i], y);
        glColor4f(r, g, b, a1);
        glVertex2f(s[i + 1], y);
        glVertex2f(s[i + 1], y + 0.75f);
        glColor4f(r, g, b, a0);
        glVertex2f(s[i], y + 0.75f);
    }
}

static void frame_rect(float x, float y, float w, float h, float t)
{
    rect(x, y, w, t);
    rect(x, y + h - t, w, t);
    rect(x, y, t, h);
    rect(x + w - t, y, t, h);
}

/* A triangle pointing right (dir 1) or left (-1) in the box, as a quad with
 * two corners on its point. */
static void tri(float x0, float y0, float x1, float y1, int dir)
{
    float ym = (y0 + y1) / 2;

    if (dir > 0) {
        glVertex2f(x0, y0);
        glVertex2f(x1, ym);
        glVertex2f(x1, ym);
        glVertex2f(x0, y1);
    } else {
        glVertex2f(x1, y0);
        glVertex2f(x1, y1);
        glVertex2f(x0, ym);
        glVertex2f(x0, ym);
    }
}

static void rp_icon(int id, float cx, float cy, int paused)
{
    float t = cy - 5, b = cy + 5;

    switch (id) {
    case RPB_START:
        rect(cx - 6, t, 2, 10);
        tri(cx - 4, t, cx + 1, b, -1);
        tri(cx + 1, t, cx + 6, b, -1);
        break;
    case RPB_BACK5:
        tri(cx - 7, t, cx, b, -1);
        tri(cx, t, cx + 7, b, -1);
        break;
    case RPB_FRAME_BACK:
        tri(cx - 5, t, cx + 2, b, -1);
        rect(cx + 3, t, 2, 10);
        break;
    case RPB_PLAY:
        if (paused)
            tri(cx - 4, cy - 6, cx + 6, cy + 6, 1);
        else {
            rect(cx - 5, cy - 6, 3.5f, 12);
            rect(cx + 1.5f, cy - 6, 3.5f, 12);
        }
        break;
    case RPB_FRAME_FWD:
        rect(cx - 5, t, 2, 10);
        tri(cx - 2, t, cx + 5, b, 1);
        break;
    case RPB_FWD5:
        tri(cx - 7, t, cx, b, 1);
        tri(cx, t, cx + 7, b, 1);
        break;
    case RPB_SLOWER:
        rect(cx - 5, cy - 1, 10, 2);
        break;
    case RPB_FASTER:
        rect(cx - 5, cy - 1, 10, 2);
        rect(cx - 1, cy - 5, 2, 10);
        break;
    case RPB_LIST:                      /* a list: dots and lines */
        for (t = cy - 5; t < cy + 6; t += 4) {
            rect(cx - 6, t, 2, 2);
            rect(cx - 2, t, 8, 2);
        }
        break;
    case RPB_CLOSE:                     /* an X: two bars across */
        glVertex2f(cx - 6, cy - 5);
        glVertex2f(cx - 4.5f, cy - 6.5f);
        glVertex2f(cx + 6, cy + 5);
        glVertex2f(cx + 4.5f, cy + 6.5f);
        glVertex2f(cx + 4.5f, cy - 6.5f);
        glVertex2f(cx + 6, cy - 5);
        glVertex2f(cx - 4.5f, cy + 6.5f);
        glVertex2f(cx - 6, cy + 5);
        break;
    case RPB_HIDE:                      /* away: a bar at the foot */
        rect(cx - 6, cy + 3, 12, 2);
        break;
    case RPB_SETTINGS:                  /* the menu's three lines */
        rect(cx - 6, cy - 5, 12, 2);
        rect(cx - 6, cy - 1, 12, 2);
        rect(cx - 6, cy + 3, 12, 2);
        break;
    }
}

/* `at` how far the fight is, 0..1; `time` its clock; `state` rp_info's;
 * `hover` the button under the mouse, -1 none; `title` the replay's name;
 * the list's `list_n` labels (-1 while it is closed), the first `n_recent`
 * the last fights, `current` the one playing. */
void dbg_replay_draw(float at, const char *time, int state, int hover,
                     const char *title, const char *const *labels, int list_n,
                     int n_recent, int current)
{
    /* Mary (0.4.2): the ONLINE screen's materials -- a stone panel under a
     * gold bar, the steel plates (PLAY the red one) lit under the mouse, a
     * well with the fight played in blood and a gold knob, the game's font.
     * Where everything is (rp_layout, RPT_*) is as it was: the mouse's
     * boxes are the same. */
    static const float pl0[4] = { 0.95f, 0.30f, 0.16f, 1.0f }, pl1[4] = { 0.45f, 0.05f, 0.02f, 1.0f };
    static float lit[RPB_COUNT + 4];
    float x[RPB_COUNT], bw[RPB_COUNT], w, b0, b1, f;
    const char *what;
    char sp[8];
    int i, paused = (state & (RPS_PAUSED | RPS_END)) != 0;

    begin_2d();
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    w = g_rp_w = g_ow / rp_scale();
    g_rp_h = 320.0f / rp_scale();
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, w, g_rp_h, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glTranslatef(0.0f, g_rp_h - 320.0f, 0.0f);     /* the panel on the bottom */
    b0 = w * 0.04f;
    b1 = w * 0.96f;
    f = at < 0 ? 0 : at > 1 ? 1 : at;
    rp_layout(w, x, bw);
    for (i = 0; i < RPB_COUNT; i++)
        lit[i] += ((hover == i ? 1.0f : 0.0f) - lit[i]) * 0.3f;

    PortSkinBegin();
    PortSkinPanel(3, RPP_TOP + 2, w - 6, 320.0f - RPP_TOP - 4, 1.0f, 1);

    /* the status line: the replay's name and the clock on the left, what it
     * is doing on the right, a warning before it when it differs */
    if (!title || !*title)
        title = "REPLAY";
    PortSkinText(title, b0, RPP_TEXT - 3, 0, 0.42f, (b1 - b0) * 0.45f, 0, 1.0f);
    PortSkinText(time, b0 + (b1 - b0) * 0.47f, RPP_TEXT - 3, 2, 0.42f, 80, 1, 1.0f);
    what = state & RPS_SEEKING ? "SEEKING" : state & RPS_END ? "END"
         : state & RPS_PAUSED ? "PAUSED" : "PLAYING";
    PortSkinText(what, b1, RPP_TEXT - 3, 2, 0.42f, 100, 2, 1.0f);
    if (state & RPS_DIFFERS)
        PortSkinText("DIFFERS - SEE THE LOG", b1 - 70, RPP_TEXT - 3, 2, 0.40f, 140, 3, 1.0f);

    /* the buttons: the ONLINE plates; their pictures in bone white */
    for (i = 0; i < RPB_COUNT; i++)
        PortSkinButton("", x[i], RPP_BTN_Y, bw[i], RPP_BTN_H, i == RPB_PLAY,
                       hover == i ? 1 : 0, lit[i], 1.0f);
    snprintf(sp, sizeof sp, "%s", k_rp_speed[(state & RPS_SPEED) % 5]);
    PortSkinText(sp, x[RPB_SLOWER] + RPP_SMALL + RPP_SPEED / 2, RPP_BTN_Y + RPP_BTN_H / 2 - 6,
                 1, 0.48f, RPP_SPEED - 4, 1, 1.0f);
    skin_end();

    glBegin(GL_QUADS);
    /* the bar: a sunk well, the fight played in blood, a gold knob */
    glColor4f(0.0f, 0.0f, 0.0f, 0.85f);
    rect(b0 - 1, RPP_BAR_Y - 1, b1 - b0 + 2, RPP_BAR_H + 2);
    vgrad(b0, RPP_BAR_Y, (b1 - b0) * f, RPP_BAR_H, pl0, pl1);
    glColor4f(1.0f, 0.9f, 0.7f, 0.25f);
    rect(b0, RPP_BAR_Y, (b1 - b0) * f, 1);
    glColor4f(0.0f, 0.0f, 0.0f, 0.8f);
    rect(b0 + (b1 - b0) * f - 4, RPP_BAR_Y - 4, 8, RPP_BAR_H + 8);
    glColor4f(1.0f, 0.78f, 0.38f, 1.0f);
    rect(b0 + (b1 - b0) * f - 3, RPP_BAR_Y - 3, 6, RPP_BAR_H + 6);
    glColor4f(1.0f, 0.95f, 0.8f, 1.0f);
    rect(b0 + (b1 - b0) * f - 3, RPP_BAR_Y - 3, 6, 1.5f);
    for (i = 0; i < RPB_COUNT; i++) {
        glColor4f(0.0f, 0.0f, 0.0f, 0.7f);              /* the pictures, cut in */
        glEnd();
        glPushMatrix();
        glTranslatef(0.6f, 0.8f, 0.0f);
        glBegin(GL_QUADS);
        rp_icon(i, x[i] + bw[i] / 2, RPP_BTN_Y + RPP_BTN_H / 2, paused);
        glEnd();
        glPopMatrix();
        glBegin(GL_QUADS);
        if (hover == i)
            glColor4f(1.0f, 0.88f, 0.62f, 1.0f);
        else
            glColor4f(0.94f, 0.91f, 0.83f, 1.0f);
        rp_icon(i, x[i] + bw[i] / 2, RPP_BTN_Y + RPP_BTN_H / 2, paused);
    }
    glEnd();

    /* the hovered button's name, on a plate above it */
    if (hover >= 0 && hover < RPB_COUNT) {
        const char *tip = k_rp_tip[hover];
        float tw = (float)strlen(tip) * 6.4f + 16, tx = x[hover] + bw[hover] / 2 - tw / 2;

        if (tx < 2) tx = 2;
        if (tx + tw > w - 2) tx = w - 2 - tw;
        PortSkinBegin();
        PortSkinButton(tip, tx, RPP_TOP - 21, tw, 15, 0, 0, 0.0f, 1.0f);
        skin_end();
    }

    glLoadIdentity();                   /* the top: where it is */

    /* the top corners: REPLAYS (the list) on the left; SIZE, HIDE and the X
     * on the right, with nothing between, so the health bars stay in sight */
    for (i = 0; i < 4; i++) {
        static const int ids[4] = { RPB_LIST, RPB_CLOSE, RPB_HIDE, RPB_SIZE };
        int id = ids[i], on = hover == id || (!i && list_n >= 0);
        float hx = b1 - RPT_X_W - RPT_GAP - RPT_HIDE_W;
        float bx = i == 0 ? b0 : i == 1 ? b1 - RPT_X_W : i == 2 ? hx : hx - RPT_GAP - RPT_SIZE_W;
        float bwid = i == 0 ? RPT_LIST_W : i == 1 ? RPT_X_W : i == 2 ? RPT_HIDE_W : RPT_SIZE_W;
        float *l = &lit[RPB_COUNT + i];

        *l += ((hover == id ? 1.0f : 0.0f) - *l) * 0.3f;
        PortSkinBegin();
        /* REPLAYS red while its list is open; the X red under the mouse */
        PortSkinButton("", bx, RPT_BTN_Y, bwid, RPT_BTN_H, (!i && list_n >= 0) || (i == 1 && hover == id),
                       hover == id ? 1 : 0, *l, 1.0f);
        if (i == 3) {                   /* SIZE: the scale, as text */
            char pc[8];

            snprintf(pc, sizeof pc, "%d%%", dbg_replay_pct);
            PortSkinText(pc, bx + bwid / 2, RPT_BTN_Y + RPT_BTN_H / 2 - 5.5f, 1, 0.42f, bwid - 4, on ? 1 : 0, 1.0f);
        } else if (!i)
            PortSkinText("REPLAYS", bx + 24, RPT_BTN_Y + RPT_BTN_H / 2 - 5.5f, 0, 0.42f, bwid - 28, on ? 1 : 0, 1.0f);
        skin_end();
        if (i == 3)
            continue;
        glBegin(GL_QUADS);
        glColor4f(0.94f, 0.91f, 0.83f, 1.0f);
        rp_icon(id, i ? bx + bwid / 2 : bx + 12, RPT_BTN_Y + RPT_BTN_H / 2, 0);
        glEnd();
    }
    /* the list, under its button: the last fights, then the kept ones */
    if (list_n >= 0) {
        float ry[RL_MAX], rh[3];
        static const float r0[4] = { 0.66f, 0.13f, 0.08f, 0.95f }, r1[4] = { 0.30f, 0.04f, 0.02f, 0.95f };
        static const float s0[4] = { 0.20f, 0.19f, 0.18f, 0.9f }, s1[4] = { 0.07f, 0.07f, 0.07f, 0.9f };

        if (list_n > RL_MAX)
            list_n = RL_MAX;
        rl_layout(list_n, n_recent, ry, rh);
        PortSkinBegin();
        PortSkinPanel(b0, RL_TOP, RL_W, rh[2] - RL_TOP, 1.0f, 1);
        PortSkinText("LAST FIGHTS", b0 + 6, rh[0] + 1, 0, 0.38f, RL_W - 12, 1, 1.0f);
        PortSkinText("KEPT", b0 + 6, rh[1] + 1, 0, 0.38f, RL_W - 12, 1, 1.0f);
        if (!n_recent)
            PortSkinText("NONE", b0 + 12, rh[0] + RL_HEAD + 2, 0, 0.36f, 60, 2, 1.0f);
        if (list_n == n_recent)
            PortSkinText("NONE", b0 + 12, rh[1] + RL_HEAD + 2, 0, 0.36f, 60, 2, 1.0f);
        skin_end();
        glBegin(GL_QUADS);
        for (i = 0; i < list_n; i++) {
            int on = hover == RPB_ROW + i, cur = i == current;

            if (on || cur)
                vgrad(b0 + 4, ry[i], RL_W - 8, RL_ROW - 1, on ? r0 : s0, on ? r1 : s1);
            if (cur) {
                glColor4f(1.0f, 0.78f, 0.38f, 1.0f);
                tri(b0 + 6, ry[i] + 3.5f, b0 + 10, ry[i] + 10.5f, 1);
            }
        }
        glEnd();
        PortSkinBegin();
        for (i = 0; i < list_n; i++) {
            int on = hover == RPB_ROW + i, cur = i == current;
            PortSkinText(labels[i], b0 + 14, ry[i] + 1.5f, 0, 0.36f, RL_W - 20,
                         on ? 0 : cur ? 1 : 2, 1.0f);
        }
        skin_end();
    }
    end_2d();
}

/* HIDE: the player away, the fight alone; a line on the bottom for a while
 * says how to have it back. `alpha` 0..1, fading. */
void dbg_replay_hidden(float alpha)
{
    const char *what = "CLICK OR ESC: THE PLAYER";
    float tw = (float)strlen(what) * CH_W;

    if (alpha <= 0)
        return;
    begin_2d();
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glBegin(GL_QUADS);
    glColor4f(0.0f, 0.0f, 0.0f, 0.65f * alpha);
    rect((g_ow - tw * 1.3f) * 0.5f - 8, 300.0f, tw * 1.3f + 16, 16.0f);
    glColor4f(0.95f, 0.72f, 0.36f, 0.8f * alpha);
    rect((g_ow - tw * 1.3f) * 0.5f - 8, 300.0f, tw * 1.3f + 16, 1.0f);
    glEnd();
    PortSkinBegin();
    PortSkinText(what, g_ow * 0.5f, 302.0f, 1, 0.42f, g_ow, 0, alpha);
    skin_end();
    end_2d();
}

/* Between one replay and the next: the screen dark, the next one's name. */
void dbg_replay_loading(const char *title)
{
    static const float veil0[4] = { 0.06f, 0.055f, 0.05f, 1.0f };
    static const float veil1[4] = { 0.01f, 0.01f, 0.01f, 1.0f };
    const char *what = "LOADING";
    float y = 160.0f - CH_H;

    begin_2d();
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glBegin(GL_QUADS);
    vgrad(0, 0, g_ow, 320.0f, veil0, veil1);
    glEnd();
    PortSkinBegin();
    PortSkinPanel(g_ow * 0.5f - 130, y - 22, 260, 64, 1.0f, 1);
    PortSkinText(what, g_ow * 0.5f, y - 12, 1, 0.70f, 240, 1, 1.0f);
    if (title && *title)
        PortSkinText(title, g_ow * 0.5f, y + 14, 1, 0.42f, 240, 0, 1.0f);
    skin_end();
    end_2d();
}


/* ------------------------------------------------------------ STAGE SELECT
 *
 * Port, not in the binary. After the fighters are chosen -- VS (GameMode 6),
 * training (2) and online (1, the host; the guest takes the host's level,
 * rollback.c rb_host_level) -- Task_GameInit asks stage_menu_gate before it
 * loads anything, and a window comes up over FE_SELECTFIGHTER: the stage's
 * picture large with arrows, or a list of all of them with their pictures
 * (the TAB key, the pad's Y or the LIST button; the mouse's wheel moves
 * through it), CANCEL (back to the select screen; online, where the guest
 * waits, the stage the game would have taken), RANDOM and SELECT. The pictures are the game's CS_BG sheets
 * (960x640 on a 1024 sheet), or STAGE_<SCENE>.PNG when one is added; a
 * stage with neither gets one drawn (st_thumb). A debug
 * fight and a replay go straight on, with their own stage. */
extern long GameMode;
long isParent(void);
struct TEXTURE;
struct TEXTURE *limeLoadTexture(const char *path, int a, int b);
struct TEXTURE *lime_load_extra_texture(const char *file);
extern char Language[10];

#define ST_MAX 16
#define ST_BTN_H 15.0f                  /* the buttons' height */

static int  g_st_open, g_st_done, g_st_chosen = -1, g_st_back;
static int  g_st_n, g_st_ids[ST_MAX], g_st_cur, g_st_list, g_st_first;
static int  g_st_hover = -1;
static int  g_st_fresh;              /* opened: what is held does not count */
/* what the last draw put where, for the mouse: 0 the picture's left arrow,
 * 1 its right one, 2 CANCEL, 3 LIST / LARGE, 4 SELECT, 5 RANDOM, 8.. a
 * list tile */
static struct { float x, y, w, h; } g_st_hit[8 + ST_MAX];
static int g_st_nhit;

static const char *st_tx(const char *es, const char *en)
{
    return Language[0] == 'E' && Language[1] == 'S' ? es : en;
}

static const struct { const char *scene, *name, *pic; } k_stages[] = {
    { "SUBWAY",         "SUBWAY",           "CS_BG.PNG"  },
    { "BELLTOWER",      "BELL TOWER",       "CS_BG1.PNG" },
    { "SOULCHAMBER",    "SOUL CHAMBER",     "CS_BG2.PNG" },
    { "GRAVEYARD",      "THE GRAVEYARD",    "CS_BG3.PNG" },
    { "TEMPLE",         "TEMPLE",           "CS_BG4.PNG" },
    { "STREET",         "THE STREET",       NULL },
    { "BALCONY",        "THE BALCONY",      NULL },
    { "BRIDGE",         "THE BRIDGE",       NULL },
    { "CAVE",           "KAHN'S KAVE",      NULL },
    { "JADESDESERT",    "JADE'S DESERT",    NULL },
    { "LAIR",           "SCORPION'S LAIR",  NULL },
    { "NOOBSDORFEN",    "NOOB'S DORFEN",    NULL },
    { "PIT",            "THE PIT III",      NULL },
    { "ROOFTOP",        "ROOFTOP",          NULL },
    { "SCISLACBUSOREZ", "SCISLAC BUSOREZ",  NULL },
    { "WATERFRONT",     "WATERFRONT",       NULL },
};
#define N_KSTAGES (int)(sizeof k_stages / sizeof k_stages[0])

/* The scene a level's fight starts in: its second scene when it has one
 * (Task_GameInit step 51 sets CurrentScene to 1 when both LEVEL_INFO names
 * are there, +0x1c and +0x50), so level 4 starts in the Subway under the
 * Street, 10 in the Soul Chamber under the Balcony, 11 in the Lair under
 * the Kave -- else its first. Named by it, the Street's two levels are the
 * Subway and the Street, not the Street twice. */
static const char *st_scene(int level)
{
    const char *rec = Level_Info + level * LEVEL_INFO_STRIDE;
    const char *s2 = *(const char *P32 const *)(rec + 0x50);
    static char buf[40];
    char *u;

    if (!s2 || !*(const char *P32 const *)(rec + 0x1c))
        return stage_name(level);
    snprintf(buf, sizeof buf, "%s", s2);
    if ((u = strstr(buf, "_LEVEL")) != NULL || (u = strchr(buf, '.')) != NULL)
        *u = 0;
    return buf;
}

static int st_entry(int level)
{
    const char *s = st_scene(level);
    int i;
    for (i = 0; i < N_KSTAGES; i++)
        if (strcmp(s, k_stages[i].scene) == 0)
            return i;
    return -1;
}

static const char *st_name(int level)
{
    int e = st_entry(level);
    return e >= 0 ? k_stages[e].name : st_scene(level);
}

/* a texture's GL name, loaded once (0: none) */
static unsigned st_tex(const char *file)
{
    static struct { char file[48]; unsigned name; } cache[40];
    int i;
    if (!file)
        return 0;
    for (i = 0; i < 40 && cache[i].file[0]; i++)
        if (strcmp(cache[i].file, file) == 0)
            return cache[i].name;
    if (i == 40)
        return 0;
    snprintf(cache[i].file, sizeof cache[i].file, "%s", file);
    {
        struct TEXTURE *t = limeLoadTexture(file, 0, 0);
        cache[i].name = t ? *(const unsigned *)((const char *)t + 0x40) : 0;
    }
    return cache[i].name;
}

static void st_pic(unsigned name, float x, float y, float w, float h,
                   float u1, float v1)
{
    if (!name)
        return;
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, name);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex2f(x, y);
    glTexCoord2f(u1, 0.0f);   glVertex2f(x + w, y);
    glTexCoord2f(u1, v1);     glVertex2f(x + w, y + h);
    glTexCoord2f(0.0f, v1);   glVertex2f(x, y + h);
    glEnd();
    glDisable(GL_TEXTURE_2D);
}

/* A stage's picture: STAGE_<SCENE>.PNG when there is one (a picture added
 * later, by a mod or in res), else its CS_BG sheet, else the one the
 * launcher renders ("aditional textures/stages/<SCENE>.png"), else one drawn
 * here --
 * a gradient in a colour of its own with the name across it. `big` the
 * name's size. */
static void st_thumb(int level, float x, float y, float w, float h, float big)
{
    static unsigned char miss[64];      /* no rendered picture */
    char file[48];
    int e = st_entry(level);
    unsigned t;
    const char *n = st_name(level);
    snprintf(file, sizeof file, "STAGE_%s.PNG", st_scene(level));
    if ((t = st_tex(file)) != 0) {
        st_pic(t, x, y, w, h, 1.0f, 1.0f);
        return;
    }
    if (e >= 0 && k_stages[e].pic && (t = st_tex(k_stages[e].pic)) != 0) {
        st_pic(t, x, y, w, h, 0.9375f, 0.625f);
        return;
    }
    /* the stage rendered by the launcher (mods/stagepics.js) */
    snprintf(file, sizeof file, "stages/%s.png", st_scene(level));
    if (level >= 0 && level < 64 && !miss[level]) {     /* looked for once */
        struct TEXTURE *r = lime_load_extra_texture(file);
        if (r && (t = *(const unsigned *)((const char *)r + 0x40)) != 0) {
            st_pic(t, x, y, w, h, 1.0f, 1.0f);
            return;
        }
        miss[level] = 1;
    }
    {
        unsigned hsh = 2166136261u;
        const char *c;
        float c0[4], c1[4], hue, sc;
        int k;
        for (c = n; *c; c++)
            hsh = (hsh ^ (unsigned char)*c) * 16777619u;
        hue = (float)(hsh % 360u) / 60.0f;
        for (k = 0; k < 3; k++) {       /* the hue as rgb, dimmed */
            float d = hue - (float)(k == 0 ? 0 : k == 1 ? 2 : 4);
            float v;
            if (d < -3.0f) d += 6.0f;
            if (d > 3.0f) d -= 6.0f;
            v = d < 0 ? -d : d;
            v = v < 1.0f ? 1.0f : v < 2.0f ? 2.0f - v : 0.0f;
            c0[k] = 0.10f + 0.40f * v;
            c1[k] = 0.03f + 0.10f * v;
        }
        c0[3] = c1[3] = 1.0f;
        glBegin(GL_QUADS);
        vgrad(x, y, w, h, c0, c1);
        /* a floor line, as a stage would have */
        glColor4f(1.0f, 1.0f, 1.0f, 0.12f);
        rect(x, y + h * 0.72f, w, 1.0f);
        sc = (float)strlen(n) * CH_W * big > w - 8 ? (w - 8) / ((float)strlen(n) * CH_W) : big;
        glColor4f(1.0f, 1.0f, 1.0f, 0.85f);
        text(x + (w - (float)strlen(n) * CH_W * sc) / 2, y + h * 0.45f - 3.5f * sc, sc, n);
        glEnd();
    }
}

static int eligible(void)
{
    if (DbgFightActive || rp_playing())
        return 0;
    return GameMode == 6 || GameMode == 2 || (GameMode == 1 && isParent());
}

static void st_music(int on);
static int  g_st_tune_check;        /* back: is the tune still on? */

/* CANCEL: Task_FEDestroy has run (the front end's pictures and models are
 * gone), so the front end is loaded again, as after a fight -- Task_FEInit,
 * with the loading picture Task_LoadingScreen made for the fight -- and
 * Task_FEMain carries on at the top of its stack, the select screen. The
 * selection screen pops on its way out (training queues the whole stack's
 * pop): that is undone, so it is the select screen it comes back to. The
 * tune is the select screen's, already playing (st_music), and the front
 * end's load latches FETuneSelection to it, so it plays on. */
extern int CurrentTask, FEInitState, PendingPop, PendingPopAll;

static void back_to_select(void)
{
    PendingPop = PendingPopAll = -1;
    FEInitState = 0;
    CurrentTask = 2;
    g_st_tune_check = 1;
    printf("stage select: back to the select screen\n");
}

/* the stages the window offers: the levels with a scene and no lock */
static void st_build(void)
{
    int i;

    g_st_n = 0;
    for (i = 0; i < ST_MAX; i++) {
        const char *s = *(const char *P32 const *)(Level_Info + i * LEVEL_INFO_STRIDE + 0x1c);
        if (s && *(const int32_t *)(Level_Info + i * LEVEL_INFO_STRIDE + 0x70) == 0)
            g_st_ids[g_st_n++] = i;
    }
}

/* Online, the guest waited on a black screen while the host picked (Mary).
 * The host sends the stage it shows a few times a second while its window is
 * up; the guest draws the same window, without buttons, over its frame
 * (game_main.c) until the host's hello brings the level (rollback.c
 * rb_host_level, which waits on as long as these keep coming). */
static int g_st_watch, g_st_heard;      /* the guest's window; frames since the last packet */

static void st_tell_guest(void)
{
    static int every;
    int32_t w[2];

    if (GameMode != 1 || !isParent() || ++every % 6)
        return;
    w[0] = g_st_ids[g_st_cur];
    w[1] = g_st_list;
    mpnet_send(PORT_PKT_STAGE, w, sizeof w, 0);
    mpnet_flush();
}

void stage_menu_remote(int level, int list)
{
    int i;

    if (GameMode != 1 || isParent())
        return;
    if (!g_st_watch) {
        st_build();
        g_st_first = 0;
        printf("stage select: the host is choosing\n");
    }
    g_st_watch = 1;
    g_st_heard = 0;
    g_st_list = list != 0;
    for (i = 0; i < g_st_n; i++)
        if (g_st_ids[i] == level)
            g_st_cur = i;
}

int stage_menu_watch_frame(void)
{
    if (g_st_watch && ++g_st_heard > 120)       /* two seconds without a word */
        g_st_watch = 0;
    return g_st_watch;
}

void stage_menu_watch_end(void) { g_st_watch = 0; }

void stage_menu_watch_draw(void)
{
    if (g_st_watch && g_st_n)
        stage_menu_draw();
}

int stage_menu_gate(long *level)
{
    int i;

    if (g_st_done && g_st_back) {       /* CANCEL: the select screen again */
        g_st_done = g_st_back = 0;
        back_to_select();
        return 1;
    }
    if (g_st_done) {                    /* picked: this fight's stage */
        g_st_done = 0;
        if (g_st_chosen >= 0) {
            printf("stage select: %d %s\n", g_st_chosen, st_name(g_st_chosen));
            *level = g_st_chosen;
        }
        return 0;
    }
    if (g_st_open)
        return 1;
    if (!eligible())
        return 0;
    st_build();
    if (g_st_n < 2)
        return 0;
    g_st_cur = 0;
    for (i = 0; i < g_st_n; i++)
        if (g_st_ids[i] == *level)
            g_st_cur = i;
    g_st_open = 1;
    g_st_fresh = 1;
    st_music(1);
    plat_mouse_wheel();                 /* turns made while it was shut */
    g_st_hover = -1;
    printf("stage select: open (%d stages)\n", g_st_n);
    return 1;
}

int stage_menu_is_open(void) { return g_st_open; }

/* UMK3_STAGE_PREVIEW=<tick>[:list] (game_main.c): STAGE SELECT opened on
 * that tick over whatever is on screen, for a script's screenshot of its
 * look; nothing is chosen from it. */
void stage_menu_preview(int list)
{
    st_build();
    if (g_st_n < 1)
        return;
    g_st_cur = g_st_n > 3 ? 3 : 0;
    g_st_list = list;
    g_st_open = 1;
    g_st_hover = -1;
}

/* The music: the select screen's tune plays on, unbroken, from the select
 * screen into this window -- its fade to the fight and Task_FEDestroy's
 * stop are skipped when this window comes next (stage_menu_keeps_tune,
 * FrontEnd.c) -- and back to the select screen again. Picked, it stops for
 * the loading, as Task_FEDestroy had it; the fight starts its own. Should
 * the tune not be on (none, or another screen's), the select screen's is
 * started, as FEMain_SwitchTune would. */
void limePlayTune(const char *file, long vol, long loop);
void limeStopTune(void);
void limeSetTuneVol(long vol);
int  plat_music_playing(void);
extern int Settings[10];
extern float MusicVol[];
extern long FETuneSelection;

static void st_tune_on(const char *why)
{
    if (!Settings[2])
        return;
    if (plat_music_playing()) {
        limeSetTuneVol((long)MusicVol[Settings[2]]);
        printf("stage select: the tune plays on (%s)\n", why);
        return;
    }
    limePlayTune("CharacterSelect.mp3", (long)MusicVol[Settings[2]], 1);
    FETuneSelection = 1;
    printf("stage select: the tune was off, started again (%s)\n", why);
}

static void st_music(int on)
{
    if (!Settings[2])
        return;
    if (on)
        st_tune_on("open");
    else
        limeStopTune();
}

/* Task_FEDestroy and the front end's fade ask: is STAGE SELECT next? */
int stage_menu_keeps_tune(void)
{
    return eligible();
}

/* Task_FEMain, every frame: the first after CANCEL makes sure it plays */
void stage_menu_fe_frame(void)
{
    if (!g_st_tune_check)
        return;
    g_st_tune_check = 0;
    st_tune_on("back");
}

/* pick: 1 the stage shown, 0 the game's own, -1 back to the select screen
 * (online, the guest waits on the host's level: the game's own) */
static void st_close(int pick)
{
    if (pick < 0 && GameMode == 1)
        pick = 0;
    g_st_chosen = pick > 0 ? g_st_ids[g_st_cur] : -1;
    g_st_back = pick < 0;
    g_st_open = 0;
    g_st_done = 1;
    if (!g_st_back)
        st_music(0);
}

static void st_move(int d)
{
    g_st_cur = (g_st_cur + d + g_st_n) % g_st_n;
}

/* RANDOM: any of them, and the fight goes on. Its own numbers, from the
 * clock: the C library's rand() is left to whatever else draws on it. */
static void st_random(void)
{
    static unsigned long seed;
    if (!seed)
        seed = (unsigned long)time(NULL) | 1;
    seed = seed * 1103515245UL + 12345UL;
    g_st_cur = (int)((seed >> 16) % (unsigned long)g_st_n);
    st_close(1);
}


/* the animations: the window rising in, the big picture sliding in from
 * the side it was turned to, each button's and picture's light */
static float g_st_in;                   /* 0..1, the window coming in */
static int   g_st_was = -1, g_st_dir;   /* the stage last drawn, the way it turned */
static float g_st_slide = 1.0f;         /* 0..1, the new picture coming in */
static float g_st_lit[8 + 16];          /* hover light, buttons and pictures */

static float st_ease(float t) { t = t < 0 ? 0 : t > 1 ? 1 : t; return 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t); }

static void st_light(int i)
{
    float want = g_st_hover == i ? 1.0f : 0.0f;
    g_st_lit[i] += (want - g_st_lit[i]) * 0.3f;
}

/* black over a box, `a` strong (raw GL, this space) */
static void st_shade(float x, float y, float w, float h, float a)
{
    glBegin(GL_QUADS);
    glColor4f(0.0f, 0.0f, 0.0f, a);
    rect(x, y, w, h);
    glEnd();
}

static void st_hit(int i, float x, float y, float w, float h)
{
    g_st_hit[i].x = x; g_st_hit[i].y = y; g_st_hit[i].w = w; g_st_hit[i].h = h;
    if (i + 1 > g_st_nhit)
        g_st_nhit = i + 1;
}

void stage_menu_draw(void)
{
    /* Mary (0.4.2): the ONLINE screen's look and life -- the chosen stage
     * itself large behind everything, dimmed; a stone panel in gold and
     * bronze rising in; the heading's steel plate; the picture in a gold
     * frame with gold chevrons, sliding in from the side it was turned to;
     * the list's pictures lit under the mouse, the chosen one pulsing red;
     * the buttons the ONLINE screen's plates, SELECT the red one */
    float pw, px, py = 14, ph = 292, bx, by, bw, a, k, t;
    const char *title = g_st_watch ? st_tx("EL ANFITRION ELIGE ESCENARIO", "THE HOST PICKS THE STAGE")
                                   : st_tx("ELIGE ESCENARIO", "SELECT STAGE");
    int i, cur;

    if (!g_st_open && !g_st_watch) {
        g_st_in = 0.0f;
        g_st_was = -1;
        return;
    }
    begin_2d();
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    g_st_nhit = 0;
    memset(g_st_hit, 0, sizeof g_st_hit);
    cur = g_st_ids[g_st_cur];
    if (g_st_was != g_st_cur) {         /* a new stage: it slides in */
        if (g_st_was >= 0) {
            g_st_dir = (g_st_cur > g_st_was) ? 1 : -1;
            if (g_st_was == g_st_n - 1 && g_st_cur == 0) g_st_dir = 1;
            if (g_st_was == 0 && g_st_cur == g_st_n - 1) g_st_dir = -1;
            g_st_slide = 0.0f;
        }
        g_st_was = g_st_cur;
    }
    g_st_in += (1.0f - g_st_in) * 0.16f + 0.004f;
    if (g_st_in > 1.0f) g_st_in = 1.0f;
    g_st_slide += (1.0f - g_st_slide) * 0.22f + 0.004f;
    if (g_st_slide > 1.0f) g_st_slide = 1.0f;
    a = st_ease(g_st_in);
    k = st_ease(g_st_slide);
    for (i = 0; i < 8 + 16; i++)
        st_light(i);

    /* behind: the chosen stage, the whole screen, dimmed, darker at the
     * edges; dark for a moment when it changes */
    st_thumb(cur, 0, 0, g_ow, 320, 0.0f);
    st_shade(0, 0, g_ow, 320, 0.58f + 0.3f * (1.0f - k));
    {
        static const float e0[4] = { 0, 0, 0, 0.75f }, e1[4] = { 0, 0, 0, 0 };
        glBegin(GL_QUADS);
        vgrad(0, 0, g_ow, 60, e0, e1);
        vgrad(0, 260, g_ow, 60, e1, e0);
        glEnd();
    }

    pw = g_ow - 24 < 460 ? g_ow - 24 : 460;
    px = (g_ow - pw) / 2;
    py = 14 + (1.0f - a) * 18.0f;       /* it rises into place */
    PortSkinBegin();
    PortSkinPanel(px, py, pw, ph, a, 1);
    PortSkinHeading(title, px + 10, py + 7, pw - 20, a);
    skin_end();

    t = PortSkinTime();
    if (!g_st_list) {                   /* one stage, large */
        float aw = 18, iw = pw - 2 * aw - 40, ih = iw * 2 / 3, ix, iy = py + 34, off;
        if (ih > 180) {
            ih = 180;
            iw = ih * 3 / 2;
        }
        ix = (g_ow - iw) / 2;
        off = (1.0f - k) * 26.0f * (float)g_st_dir;
        PortSkinBegin();
        PortSkinShadow(ix - 2, iy - 2, iw + 4, ih + 4, a);
        skin_end();
        st_thumb(cur, ix + off, iy, iw, ih, 3.0f);
        st_shade(ix + off, iy, iw, ih, 0.7f * (1.0f - k));
        PortSkinBegin();
        PortSkinFrame(ix - 2, iy - 2, iw + 4, ih + 4, 2.0f, 1, a);
        /* the chevrons, out from the picture; they swell under the mouse */
        {
            float l0 = g_st_lit[0], l1 = g_st_lit[1];
            float ch0 = 26 + 6 * l0, ch1 = 26 + 6 * l1, nudge = 2.0f * sinf(t * 4.0f);
            PortSkinChevron(ix - 14 - ch0 * 0.62f - nudge, iy + ih / 2 - ch0 / 2, ch0 * 0.62f, ch0, 1, (0.75f + 0.25f * l0) * a);
            PortSkinChevron(ix + iw + 14 + nudge, iy + ih / 2 - ch1 / 2, ch1 * 0.62f, ch1, 0, (0.75f + 0.25f * l1) * a);
        }
        PortSkinText(st_name(cur), g_ow / 2, iy + ih + 8, 1, 0.80f, iw, 1, a * (0.4f + 0.6f * k));
        {
            char cnt[16];
            snprintf(cnt, sizeof cnt, "%d / %d", g_st_cur + 1, g_st_n);
            PortSkinText(cnt, g_ow / 2, iy + ih + 30, 1, 0.40f, 80, 2, a);
        }
        skin_end();
        st_hit(0, ix - aw - 26, iy, aw + 26, ih);
        st_hit(1, ix + iw, iy, aw + 26, ih);
    } else {                            /* the list: every stage, its picture */
        /* Mary: five rows, so every stage is in sight and nothing scrolls;
         * as many columns as that takes (4 for the game's 16), the names on
         * a dark band across each picture's foot */
        int rows = 5, cols = (g_st_n + rows - 1) / rows < 4 ? 4 : (g_st_n + rows - 1) / rows, vis;
        float gx = px + 14, gy = py + 34, gw = pw - 28;
        float tw, th;
        if (cols > 7) cols = 7;
        vis = cols * rows;
        {   /* sized by the rows filled, up to five: the panel's room used */
            int used = (g_st_n + cols - 1) / cols;
            if (used > rows) used = rows;
            if (used < 1) used = 1;
            th = (ph - 34 - ST_BTN_H - 30 - (used - 1) * 5) / used;
        }
        tw = th * 3 / 2;
        if (cols * tw + (cols - 1) * 8 > gw) {
            tw = (gw - (cols - 1) * 8) / cols;
            th = tw * 2 / 3;
        }
        gx = (g_ow - (cols * tw + (cols - 1) * 8)) / 2;
        if (g_st_cur < g_st_first)
            g_st_first = g_st_cur / cols * cols;
        if (g_st_cur >= g_st_first + vis)
            g_st_first = (g_st_cur / cols - rows + 1) * cols;
        for (i = 0; i < vis && g_st_first + i < g_st_n; i++) {
            int kk = g_st_first + i, lv = g_st_ids[kk], on = kk == g_st_cur;
            float l = g_st_lit[8 + i], lift = 1.5f * l;
            float x = gx + (i % cols) * (tw + 8), y = gy + (i / cols) * (th + 5) - lift;
            PortSkinBegin();
            if (on)
                PortSkinGlow(x, y, tw, th, a * (0.7f + 0.3f * sinf(t * 5.0f)));
            PortSkinShadow(x, y, tw, th, a);
            skin_end();
            st_thumb(lv, x, y, tw, th, 1.0f);
            if (!on)
                st_shade(x, y, tw, th, 0.35f * (1.0f - l));
            PortSkinBegin();
            skin_end();
            st_shade(x, y + th - 10, tw, 10, 0.62f);
            PortSkinBegin();
            PortSkinFrame(x - 1, y - 1, tw + 2, th + 2, on ? 1.4f : 0.9f, on || l > 0.5f, a);
            PortSkinText(st_name(lv), x + tw / 2, y + th - 9.5f, 1, 0.30f, tw - 4, on ? 1 : 0, a * (on ? 1.0f : 0.8f + 0.2f * l));
            skin_end();
            st_hit(8 + i, x, y, tw, th);
        }
        if (g_st_n > vis) {             /* a scroll bar: where the rows shown are */
            int lines = (g_st_n + cols - 1) / cols;
            float sx = px + pw - 9, h = rows * (th + 5) - 5;
            float y0 = gy + h * (float)(g_st_first / cols) / (float)lines;
            float y1 = gy + h * (float)(g_st_first / cols + rows) / (float)lines;
            glBegin(GL_QUADS);
            glColor4f(0.0f, 0.0f, 0.0f, 0.6f * a);
            rect(sx, gy, 3, h);
            glColor4f(0.95f, 0.72f, 0.36f, a);
            rect(sx, y0, 3, (y1 < gy + h ? y1 : gy + h) - y0);
            glEnd();
        }
    }

    if (g_st_watch) {                   /* the guest's: no buttons */
        end_2d();
        return;
    }
    /* CANCEL, LIST / LARGE, RANDOM, SELECT: the ONLINE screen's plates,
     * SELECT the red one, lit under the mouse */
    bw = 92;
    by = py + ph - ST_BTN_H - 12;
    bx = (g_ow - 4 * bw - 3 * 8) / 2;
    PortSkinBegin();
    PortSkinRule(px + 14, by - 9, pw - 28, a);
    PortSkinButton(st_tx("CANCELAR", "CANCEL"), bx, by, bw, ST_BTN_H + 2, 0,
                   g_st_hover == 2, g_st_lit[2], a);
    PortSkinButton(g_st_list ? st_tx("GRANDE", "LARGE") : st_tx("LISTA", "LIST"),
                   bx + bw + 8, by, bw, ST_BTN_H + 2, 0, g_st_hover == 3, g_st_lit[3], a);
    PortSkinButton(st_tx("ALEATORIO", "RANDOM"), bx + 2 * (bw + 8), by, bw, ST_BTN_H + 2, 0,
                   g_st_hover == 5, g_st_lit[5], a);
    PortSkinButton(st_tx("ELEGIR", "SELECT"), bx + 3 * (bw + 8), by, bw, ST_BTN_H + 2, 1,
                   g_st_hover == 4 ? 1 : 2, g_st_lit[4], a);
    skin_end();
    st_hit(2, bx, by, bw, ST_BTN_H + 2);
    st_hit(3, bx + bw + 8, by, bw, ST_BTN_H + 2);
    st_hit(5, bx + 2 * (bw + 8), by, bw, ST_BTN_H + 2);
    st_hit(4, bx + 3 * (bw + 8), by, bw, ST_BTN_H + 2);
    end_2d();
}

void stage_menu_mouse(float fx, float fy, int down)
{
    static int was;
    float x = fx * g_ow, y = fy * 320.0f;
    int i, hit = -1;

    if (!g_st_open) {
        was = down;
        return;
    }
    {   /* the wheel: a row of the list a notch (a stage in the large view),
         * up when it turns up, as in the debug menu; it stops at the ends */
        int w = plat_mouse_wheel(), k;
        if (w) {
            k = g_st_cur - w * (g_st_list ? 4 : 1);
            g_st_cur = k < 0 ? 0 : k >= g_st_n ? g_st_n - 1 : k;
        }
    }
    for (i = 0; i < g_st_nhit; i++)
        if (g_st_hit[i].w > 0 && x >= g_st_hit[i].x && x < g_st_hit[i].x + g_st_hit[i].w
            && y >= g_st_hit[i].y && y < g_st_hit[i].y + g_st_hit[i].h)
            hit = i;
    g_st_hover = hit;
    if (was && !down && hit >= 0) {     /* a click, on release */
        if (hit == 0)
            st_move(-1);
        else if (hit == 1)
            st_move(1);
        else if (hit == 2)
            st_close(-1);
        else if (hit == 3)
            g_st_list = !g_st_list;
        else if (hit == 4)
            st_close(1);
        else if (hit == 5)
            st_random();
        else if (hit >= 8) {
            int k = g_st_first + hit - 8;
            if (k == g_st_cur)          /* a second click on it: that one */
                st_close(1);
            else if (k < g_st_n)
                g_st_cur = k;
        }
    }
    was = down;
}

void stage_menu_keys(int left, int right, int up, int down, int ok, int back,
                     int tab)
{
    static int was[7];
    int now[7], i, e[7];

    now[0] = left; now[1] = right; now[2] = up; now[3] = down;
    now[4] = ok; now[5] = back; now[6] = tab;
    for (i = 0; i < 7; i++) {
        e[i] = now[i] && !was[i] && !g_st_fresh;
        was[i] = now[i];
    }
    g_st_fresh = 0;                     /* START / ENTER from PLAY still down */
    st_tell_guest();
    if (!g_st_open)
        return;
    if (e[0]) st_move(-1);
    if (e[1]) st_move(1);
    if (g_st_list && e[2]) st_move(-4);
    if (g_st_list && e[3]) st_move(4);
    if (e[6]) g_st_list = !g_st_list;
    if (e[4]) st_close(1);
    else if (e[5]) st_close(-1);
}

/* ------------------------------------------------------------ VS RULES ----
 *
 * Port, not in the binary (Mary, 2026-11-12): on the local versus's select
 * screen (FE_Task_Character_Select in GameMode 6) the RULES button in the
 * top left corner, the RULES key or the pad's VERSUS RULES button (SELECT)
 * opens the fight's rules in the ONLINE screen's look:
 * ROUNDS a fight (best of 1/3/5), FIGHTS in a series (one, best of 3/5),
 * TIME (30/60/99/infinite), KOMBO SLIDE and the VS kodes (the binary's,
 * runtime/online_rules.h), as many as wanted, ticked in a checklist that
 * KODES opens. ORIGINAL puts the game's own rules back. Kept
 * in umk3.ini as vs_rules= (empty: the game's own). The stage kodes are
 * left out -- STAGE SELECT picks the stage -- and so is the one-round kode
 * (0x13), which the binary only plays online. */
#include "online_rules.h"

void set_menu_put_key(const char *k, const char *v);   /* settings_menu.c */
const char *GameTextNoHeader(long id);                  /* the game's texts, UTF-16 */

enum { VR_ROUNDS, VR_FIGHTS, VR_TIME, VR_SLIDE, VR_KODE, VR_HOST, VR_ROWS,
       VR_ORIGINAL = VR_ROWS, VR_OK, VR_ITEMS };
/* the kodes the checklist offers: blocking off, dark kombat, infinite run,
 * the four handicaps, the bars hidden, throwing off */
static const int k_vr_kodes[] = { 0, 1, 2, 13, 14, 15, 16, 17, 18 };
#define VR_NKODES ((int)(sizeof k_vr_kodes / sizeof k_vr_kodes[0]))
#define VK_DONE   VR_NKODES             /* the checklist's button */

static int g_vr_open, g_vr_row, g_vr_hover = -1, g_vr_fresh, g_vr_on;
static float g_vr_in, g_vr_lit[VR_ITEMS];
static online_rules g_vr;               /* being edited */
/* a lobby's rules (the ONLINE screen's LOBBIES): HOST ADVANTAGE too, and
 * ACCEPT hands them back instead of keeping them as the local versus's */
static int g_vr_lobby;
static void (*g_vr_done)(const online_rules *);
static struct { float x, y, w, h; int item, dir; } g_vr_hit[3 * VR_ROWS + 2];
static int g_vr_nhit;
/* the KODES checklist, a window over the rules' */
static int g_vk_open, g_vk_row, g_vk_hover = -1;
static float g_vk_in, g_vk_lit[VR_NKODES + 1];
static struct { float x, y, w, h; } g_vk_hit[VR_NKODES + 1];

int vs_rules_is_open(void) { return g_vr_open; }

void vs_rules_open(void)
{
    const online_rules *r = rules_local();

    if (r) {
        g_vr = *r;
        g_vr_on = 1;
    } else {
        rules_default(&g_vr);
        g_vr_on = 0;
    }
    g_vr_lobby = 0;
    g_vr_done = NULL;
    g_vr_open = 1;
    g_vk_open = 0;
    g_vr_row = 0;
    g_vr_in = 0.0f;
    g_vr_fresh = 1;                     /* SELECT is still down */
    printf("versus rules: opened\n");
}

/* The same window for a lobby's rules (port, ONLINE-PHASE-PLAN.md stage 3):
 * done gets them on ACCEPT (or back). */
void lobby_rules_open(const online_rules *r, void (*done)(const online_rules *))
{
    if (r)
        g_vr = *r;
    else
        rules_default(&g_vr);
    g_vr_on = 1;
    g_vr_lobby = 1;
    g_vr_done = done;
    g_vr_open = 1;
    g_vk_open = 0;
    g_vr_row = 0;
    g_vr_in = 0.0f;
    g_vr_fresh = 1;                     /* the click or key that opened it */
    printf("lobby rules: opened\n");
}

static void vr_close(void)
{
    char t[ONLINE_RULES_TEXT] = "";

    if (g_vr_lobby) {
        g_vr_open = 0;
        g_vk_open = 0;
        if (g_vr_done)
            g_vr_done(&g_vr);
        return;
    }
    if (g_vr_on)
        rules_encode(&g_vr, t, sizeof t);
    rules_set_local(g_vr_on ? &g_vr : NULL);
    set_menu_put_key("vs_rules", t);
    g_vr_open = 0;
    g_vk_open = 0;
}

static void vk_open(void)
{
    g_vk_open = 1;
    g_vk_row = 0;
    g_vk_in = 0.0f;
}

/* a kode on or off; a player's two handicaps exclude each other */
static void vk_toggle(int i)
{
    unsigned bit = 1u << k_vr_kodes[i];

    g_vr_on = 1;
    g_vr.kodes ^= bit;
    if (g_vr.kodes & bit) {
        if (bit & (1u << 13)) g_vr.kodes &= ~(1u << 14);
        if (bit & (1u << 14)) g_vr.kodes &= ~(1u << 13);
        if (bit & (1u << 15)) g_vr.kodes &= ~(1u << 16);
        if (bit & (1u << 16)) g_vr.kodes &= ~(1u << 15);
    }
}

/* a value one step on (d 1) or back (-1), round */
static void vr_step(int row, int d)
{
    static const int bo[3] = { 1, 3, 5 }, tm[4] = { 30, 60, 99, 0 };
    int i;

    if (row == VR_KODE) {               /* the checklist */
        vk_open();
        return;
    }
    g_vr_on = 1;                        /* a change: these rules, not the game's */
    switch (row) {
    case VR_ROUNDS: case VR_FIGHTS: {
        int *v = row == VR_ROUNDS ? &g_vr.best_of : &g_vr.series;
        for (i = 0; i < 3 && bo[i] != *v; i++)
            ;
        *v = bo[((i < 3 ? i : 1) + d + 3) % 3];
        break;
    }
    case VR_TIME:
        for (i = 0; i < 4 && tm[i] != g_vr.time; i++)
            ;
        g_vr.time = tm[((i < 4 ? i : 2) + d + 4) % 4];
        break;
    case VR_SLIDE:
        g_vr.kombo_slide = !g_vr.kombo_slide;
        break;
    case VR_HOST:
        g_vr.host_adv = !g_vr.host_adv;
        break;
    }
}

/* the next row up or down; HOST ADVANTAGE is a lobby's alone */
static int vr_next(int row, int d)
{
    do
        row = (row + d + VR_ITEMS) % VR_ITEMS;
    while (row == VR_HOST && !g_vr_lobby);
    return row;
}

static void vr_original(void)
{
    rules_default(&g_vr);
    g_vr_on = g_vr_lobby;               /* a lobby has rules either way */
}

/* the binary's own name for a VS kode (english_text.dat and the others,
 * GameText 148.. -- 166 has no kode), upper case, in the font's range */
static void vr_kode_name(int kode, char *out, int n)
{
    long id = kode <= 17 ? 148 + kode : 149 + kode;
    const unsigned short *u = (const unsigned short *)GameTextNoHeader(id);
    int i = 0;

    if (kode < 0 || !u) {
        snprintf(out, (size_t)n, "%s", kode < 0 ? st_tx("NINGUNO", "NONE") : "KODE");
        return;
    }
    for (; *u && i < n - 1; u++) {
        unsigned c = *u;
        if (c >= 'a' && c <= 'z')
            c -= 32;
        else if (c >= 0xe0 && c <= 0xfe && c != 0xf7)
            c -= 32;                    /* latin-1 lower to upper */
        out[i++] = c < 256 ? (char)c : '?';
    }
    out[i] = 0;
}

static void vr_value(int row, char *out, int n)
{
    switch (row) {
    case VR_ROUNDS:
        if (g_vr.best_of == 1)
            snprintf(out, (size_t)n, "%s", st_tx("UNA RONDA", "ONE ROUND"));
        else
            snprintf(out, (size_t)n, st_tx("MEJOR DE %d", "BEST OF %d"), g_vr.best_of);
        break;
    case VR_FIGHTS:
        if (g_vr.series == 1)
            snprintf(out, (size_t)n, "%s", st_tx("UNA PELEA", "ONE FIGHT"));
        else
            snprintf(out, (size_t)n, st_tx("MEJOR DE %d PELEAS", "BEST OF %d FIGHTS"), g_vr.series);
        break;
    case VR_TIME:
        if (g_vr.time == 0)
            snprintf(out, (size_t)n, "%s", st_tx("INFINITO", "INFINITE"));
        else
            snprintf(out, (size_t)n, "%d", g_vr.time);
        break;
    case VR_SLIDE:
        snprintf(out, (size_t)n, "%s", g_vr.kombo_slide ? st_tx("SI (ORIGINAL)", "ON (ORIGINAL)")
                                                        : st_tx("NO", "OFF"));
        break;
    case VR_HOST:
        snprintf(out, (size_t)n, "%s", g_vr.host_adv ? st_tx("SI: EL ANFITRION ES EL 1", "ON: THE HOST IS PLAYER 1")
                                                     : st_tx("NO: LADO Y ESCENARIO AL AZAR", "OFF: SIDE AND STAGE DRAWN"));
        break;
    default: {                          /* none, the one's name, or how many */
        int i, c = 0, one = -1;
        for (i = 0; i < 20; i++)
            if (g_vr.kodes & (1u << i)) {
                c++;
                one = i;
            }
        if (c <= 1)
            vr_kode_name(one, out, n);
        else
            snprintf(out, (size_t)n, "%d KODES", c);
        break;
    }
    }
}

static void vr_hit(float x, float y, float w, float h, int item, int dir)
{
    if (g_vr_nhit < (int)(sizeof g_vr_hit / sizeof g_vr_hit[0])) {
        g_vr_hit[g_vr_nhit].x = x; g_vr_hit[g_vr_nhit].y = y;
        g_vr_hit[g_vr_nhit].w = w; g_vr_hit[g_vr_nhit].h = h;
        g_vr_hit[g_vr_nhit].item = item; g_vr_hit[g_vr_nhit].dir = dir;
        g_vr_nhit++;
    }
}

/* a flat quad in the skin's space, texturing off */
static void vr_fill(float x, float y, float w, float h, float r, float g, float b, float a)
{
    glDisable(GL_TEXTURE_2D);
    glBegin(GL_QUADS);
    glColor4f(r, g, b, a);
    rect(x, y, w, h);
    glEnd();
}

/* The KODES checklist: a box a kode, the game's own names, DONE back. */
static void vk_draw(void)
{
    float rh = 22, pw, ph = 48 + VR_NKODES * rh + 40, px, py, a, ry;
    char name[64];
    int i;

    g_vk_in += (1.0f - g_vk_in) * 0.2f + 0.004f;
    if (g_vk_in > 1.0f) g_vk_in = 1.0f;
    a = st_ease(g_vk_in);
    for (i = 0; i <= VR_NKODES; i++) {
        float want = (i == g_vk_hover || i == g_vk_row) ? 1.0f : 0.0f;
        g_vk_lit[i] += (want - g_vk_lit[i]) * 0.25f;
    }
    st_shade(0, 0, g_ow, 320, 0.5f * a);
    pw = g_ow - 40 < 360 ? g_ow - 40 : 360;
    px = (g_ow - pw) / 2;
    py = (320 - ph) / 2 + (1.0f - a) * 14.0f;

    PortSkinPanel(px, py, pw, ph, a, 0);
    PortSkinHeading("KODES", px + 10, py + 7, pw - 20, a);
    PortSkinText(st_tx("MARCA LOS QUE QUIERAS", "TICK AS MANY AS YOU LIKE"),
                 g_ow / 2, py + 34, 1, 0.38f, pw - 30, 2, a);
    ry = py + 48;
    for (i = 0; i < VR_NKODES; i++, ry += rh) {
        int on = (g_vr.kodes >> k_vr_kodes[i]) & 1;
        float bx = px + 20, by = ry + 5, bs = rh - 10, l = g_vk_lit[i];

        if (i == g_vk_row)
            PortSkinGlow(px + 10, ry + 1, pw - 20, rh - 2, 0.5f * a);
        vr_fill(bx, by, bs, bs, 0.0f, 0.0f, 0.0f, 0.7f * a);
        if (on)
            vr_fill(bx + 3, by + 3, bs - 6, bs - 6, 1.0f, 0.78f, 0.25f, a);
        PortSkinFrame(bx, by, bs, bs, 1.2f, on || l > 0.5f, a);
        vr_kode_name(k_vr_kodes[i], name, sizeof name);
        PortSkinText(name, bx + bs + 10, ry + 6, 0, 0.44f, pw - bs - 50,
                     on ? 1 : i == g_vk_row ? 0 : 2, a);
        g_vk_hit[i].x = px + 10; g_vk_hit[i].y = ry; g_vk_hit[i].w = pw - 20; g_vk_hit[i].h = rh;
    }
    {
        float bw = pw * 0.42f, bh = 24, bx = (g_ow - bw) / 2, by = py + ph - bh - 10;
        int st = g_vk_hover == VK_DONE ? 1 : g_vk_row == VK_DONE ? 2 : 0;
        PortSkinButton(st_tx("LISTO", "DONE"), bx, by, bw, bh, 1, st, g_vk_lit[VK_DONE], a);
        g_vk_hit[VK_DONE].x = bx; g_vk_hit[VK_DONE].y = by;
        g_vk_hit[VK_DONE].w = bw; g_vk_hit[VK_DONE].h = bh;
    }
}

void vs_rules_draw(void)
{
    static const char *const es[VR_ROWS] = { "RONDAS POR PELEA", "PELEAS", "TIEMPO", "KOMBO SLIDE", "KODES",
                                             "VENTAJA DEL ANFITRION" };
    static const char *const en[VR_ROWS] = { "ROUNDS A FIGHT", "FIGHTS", "TIME", "KOMBO SLIDE", "KODES",
                                             "HOST ADVANTAGE" };
    float pw, ph = g_vr_lobby ? 292 : 262, px, py, a, rh = 30, ry, t;
    char v[64];
    int i;

    if (!g_vr_open)
        return;
    begin_2d();
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    g_vr_nhit = 0;
    g_vr_in += (1.0f - g_vr_in) * 0.16f + 0.004f;
    if (g_vr_in > 1.0f) g_vr_in = 1.0f;
    a = st_ease(g_vr_in);
    for (i = 0; i < VR_ITEMS; i++) {
        float want = !g_vk_open && (i == g_vr_hover || i == g_vr_row) ? 1.0f : 0.0f;
        g_vr_lit[i] += (want - g_vr_lit[i]) * 0.25f;
    }

    st_shade(0, 0, g_ow, 320, 0.62f * a);   /* the select screen, dimmed */
    pw = g_ow - 24 < 420 ? g_ow - 24 : 420;
    px = (g_ow - pw) / 2;
    py = (320 - ph) / 2 + (1.0f - a) * 18.0f;
    t = PortSkinTime();

    PortSkinBegin();
    PortSkinPanel(px, py, pw, ph, a, 1);
    PortSkinHeading(g_vr_lobby ? st_tx("REGLAS DE LA SALA", "LOBBY RULES")
                               : st_tx("REGLAS DE VERSUS", "VERSUS RULES"), px + 10, py + 7, pw - 20, a);
    if (g_vr_lobby)
        PortSkinText(st_tx("TODOS EN LA SALA PELEAN CON ESTAS REGLAS",
                           "EVERYONE IN THE LOBBY FIGHTS BY THESE"),
                     g_ow / 2, py + 36, 1, 0.42f, pw - 30, 2, a);
    else
    PortSkinText(g_vr_on ? st_tx("ESTAS REGLAS SE USAN EN CADA VERSUS",
                                 "THESE RULES PLAY IN EVERY VERSUS")
                         : st_tx("REGLAS DEL JUEGO ORIGINAL", "THE ORIGINAL GAME'S RULES"),
                 g_ow / 2, py + 36, 1, 0.42f, pw - 30, g_vr_on ? 2 : 1, a);

    ry = py + 54;
    for (i = 0; i < VR_ROWS; i++, ry += rh) {
        if (i == VR_HOST && !g_vr_lobby) {
            ry -= rh;
            continue;
        }
        {
        float lx = px + 22, vx = px + pw * 0.45f, vw = pw * 0.55f - 22, cw = 12, l = g_vr_lit[i];
        float nudge = (i == g_vr_row && !g_vk_open) ? 1.5f * sinf(t * 5.0f) : 0.0f;

        if (i == g_vr_row)
            PortSkinGlow(px + 12, ry + 2, pw - 24, rh - 4, 0.55f * a);
        PortSkinText(st_tx(es[i], en[i]), lx, ry + 9, 0, 0.50f, vx - lx - 8,
                     i == g_vr_row ? 1 : 0, a);
        vr_value(i, v, sizeof v);
        if (i == VR_KODE) {             /* the checklist: one chevron, it opens */
            PortSkinText(v, vx + (vw - cw) / 2, ry + 9, 1, 0.50f, vw - cw - 12,
                         g_vr_on ? 0 : 2, a);
            PortSkinChevron(vx + vw - cw * 0.62f + nudge, ry + 6, cw * 0.62f, rh - 12, 1,
                            (0.55f + 0.45f * l) * a);
            vr_hit(px + 12, ry, pw - 24, rh, i, 0);
        } else {
            PortSkinText(v, vx + vw / 2, ry + 9, 1, 0.50f, vw - 2 * cw - 12,
                         g_vr_on ? 0 : 2, a);
            PortSkinChevron(vx - nudge, ry + 6, cw * 0.62f, rh - 12, 0, (0.55f + 0.45f * l) * a);
            PortSkinChevron(vx + vw - cw * 0.62f + nudge, ry + 6, cw * 0.62f, rh - 12, 1,
                            (0.55f + 0.45f * l) * a);
            vr_hit(vx - 4, ry, cw + 8, rh, i, -1);
            vr_hit(vx + vw - cw - 4, ry, cw + 8, rh, i, 1);
            vr_hit(px + 12, ry, pw - 24, rh, i, 0);
        }
        PortSkinRule(px + 16, ry + rh - 1, pw - 32, 0.35f * a);
        }
    }

    {
        float bw = (pw - 60) / 2, bh = 24, by = py + ph - bh - 14, bx0 = px + 20, bx1 = px + pw - 20 - bw;
        int s0 = g_vr_hover == VR_ORIGINAL ? 1 : g_vr_row == VR_ORIGINAL ? 2 : 0;
        int s1 = g_vr_hover == VR_OK ? 1 : g_vr_row == VR_OK ? 2 : 0;

        PortSkinButton("ORIGINAL", bx0, by, bw, bh, 0, s0, g_vr_lit[VR_ORIGINAL], a);
        PortSkinButton(st_tx("ACEPTAR", "ACCEPT"), bx1, by, bw, bh, 1, s1, g_vr_lit[VR_OK], a);
        vr_hit(bx0, by, bw, bh, VR_ORIGINAL, 0);
        vr_hit(bx1, by, bw, bh, VR_OK, 0);
    }
    if (g_vk_open)
        vk_draw();
    skin_end();
    end_2d();                           /* the game's matrices and state back */
}

void vs_rules_mouse(float fx, float fy, int down)
{
    static int was;
    float x = fx * g_ow, y = fy * 320.0f;
    int i, hit = -1;

    if (!g_vr_open) {
        was = down;
        return;
    }
    if (g_vk_open) {                    /* the checklist has the mouse */
        for (i = 0; i <= VR_NKODES; i++)
            if (x >= g_vk_hit[i].x && x < g_vk_hit[i].x + g_vk_hit[i].w
                && y >= g_vk_hit[i].y && y < g_vk_hit[i].y + g_vk_hit[i].h) {
                hit = i;
                break;
            }
        g_vk_hover = hit;
        if (was && !down && hit >= 0) {
            g_vk_row = hit;
            if (hit == VK_DONE)
                g_vk_open = 0;
            else
                vk_toggle(hit);
        }
        was = down;
        return;
    }
    for (i = 0; i < g_vr_nhit; i++)     /* the chevrons come before their row */
        if (x >= g_vr_hit[i].x && x < g_vr_hit[i].x + g_vr_hit[i].w
            && y >= g_vr_hit[i].y && y < g_vr_hit[i].y + g_vr_hit[i].h) {
            hit = i;
            break;
        }
    g_vr_hover = hit >= 0 ? g_vr_hit[hit].item : -1;
    if (was && !down && hit >= 0) {     /* a click, on release */
        int item = g_vr_hit[hit].item;
        if (item == VR_OK)
            vr_close();
        else if (item == VR_ORIGINAL)
            vr_original();
        else {
            g_vr_row = item;
            vr_step(item, g_vr_hit[hit].dir ? g_vr_hit[hit].dir : 1);
        }
    }
    was = down;
}

void vs_rules_keys(int left, int right, int up, int down, int ok, int back)
{
    static int was[6];
    int now[6], e[6], i;

    now[0] = left; now[1] = right; now[2] = up; now[3] = down; now[4] = ok; now[5] = back;
    for (i = 0; i < 6; i++) {
        e[i] = now[i] && !was[i] && !g_vr_fresh;
        was[i] = now[i];
    }
    g_vr_fresh = 0;
    if (!g_vr_open)
        return;
    if (g_vk_open) {                    /* the checklist */
        if (e[2]) g_vk_row = (g_vk_row + VR_NKODES) % (VR_NKODES + 1);
        if (e[3]) g_vk_row = (g_vk_row + 1) % (VR_NKODES + 1);
        if (e[4] || ((e[0] || e[1]) && g_vk_row < VK_DONE)) {
            if (g_vk_row == VK_DONE)
                g_vk_open = 0;
            else
                vk_toggle(g_vk_row);
        }
        if (e[5])                       /* back: to the rules */
            g_vk_open = 0;
        return;
    }
    if (e[2]) g_vr_row = vr_next(g_vr_row, -1);
    if (e[3]) g_vr_row = vr_next(g_vr_row, 1);
    if (g_vr_row < VR_ROWS) {
        if (e[0] && g_vr_row != VR_KODE) vr_step(g_vr_row, -1);
        if (e[1] || e[4]) vr_step(g_vr_row, 1);
    } else {
        if (e[0] || e[1])
            g_vr_row = g_vr_row == VR_OK ? VR_ORIGINAL : VR_OK;
        if (e[4]) {
            if (g_vr_row == VR_OK)
                vr_close();
            else
                vr_original();
        }
    }
    if (e[5])                           /* back, SELECT again: kept, as ACCEPT */
        vr_close();
}

/* On the local versus's select screen, a line over player one's COLOR box:
 * SELECT opens the rules, and what they are now (the game's own, or a
 * summary). */
/* The RULES button, the select screen's top left corner (Mary): a click
 * opens the window, as the RULES key (PK_RULES, CONTROLS) and the pad's
 * VERSUS RULES button do; under it what the rules are. */
#define VB_X 8.0f
#define VB_Y 6.0f
#define VB_W 118.0f
#define VB_H 22.0f
static int g_vb_hover, g_vb_press;
static float g_vb_lit;

int vs_rules_button_mouse(float fx, float fy, int down)
{
    static int was;
    float x = fx * g_ow, y = fy * 320.0f;
    int over = x >= VB_X && x < VB_X + VB_W && y >= VB_Y && y < VB_Y + VB_H;
    int mine;

    if (g_vr_open) {
        g_vb_press = 0;
        was = down;
        return 0;
    }
    g_vb_hover = over;
    if (down && !was && over)
        g_vb_press = 1;                 /* a press that began on it */
    mine = over || g_vb_press;
    if (!down && was && g_vb_press && over)
        vs_rules_open();                /* a click, on release */
    if (!down)
        g_vb_press = 0;
    was = down;
    return mine;                        /* the select screen does not get it */
}

void vs_rules_hint(void)
{
    const online_rules *r = rules_local();
    char line[96], what[48], key[32];

    if (g_vr_open)
        return;
    if (r) {
        char b[24];
        if (r->series > 1)
            snprintf(b, sizeof b, st_tx("%d PELEAS", "%d FIGHTS"), r->series);
        else
            snprintf(b, sizeof b, "%s", st_tx("1 PELEA", "1 FIGHT"));
        snprintf(what, sizeof what, st_tx("MEJOR DE %d, %s", "BEST OF %d, %s"), r->best_of, b);
    } else
        snprintf(what, sizeof what, "%s", st_tx("ORIGINALES", "ORIGINAL"));
    plat_key_label(PK_RULES, key, sizeof key);
    snprintf(line, sizeof line, "%s: %s", key, what);
    g_vb_lit += ((g_vb_hover ? 1.0f : 0.0f) - g_vb_lit) * 0.25f;
    begin_2d();
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    PortSkinBegin();
    PortSkinButton(st_tx("REGLAS", "RULES"), VB_X, VB_Y, VB_W, VB_H, 0,
                   g_vb_press && g_vb_hover ? 3 : g_vb_hover ? 1 : 0, g_vb_lit, 1.0f);
    PortSkinText(line, VB_X + 2, VB_Y + VB_H + 4, 0, 0.34f, g_ow * 0.34f, r ? 1 : 0, 0.9f);
    skin_end();
    end_2d();                           /* the game's matrices and state back */
}

/* The player's banner, as the game shows it now (FrontEnd.c PortSkinBand,
 * the ONLINE screen's): the gradient on steel, the name and id, the fighter
 * standing out of its left end. Esc's settings menu draws it under its
 * categories (settings_menu.c), in this space. */
void PortSkinBegin(void);
void PortSkinEnd(void);
void PortSkinBand(float x, float y, float w, float h, float art, float a);

void ui_profile_banner(float x, float y, float w, float h)
{
    static unsigned char *art;
    static unsigned have;
    const struct profile *me = prof_get();
    float sz = h * 1.45f;
    unsigned key;

    if (!me)
        return;
    begin_2d();
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    PortSkinBegin();
    PortSkinBand(x, y, w, h, sz, 1.0f);
    skin_end();
    end_2d();
    key = ((unsigned)me->icon * 2654435761u) ^ (me->alt ? 0x5bd1e995u : 0u) ^ 0x7f4a7c15u;
    if (key != have || !art) {
        free(art);
        art = prof_art_rgba(me->icon, me->alt);
        have = key;
    }
    if (art)
        ui_picture_at(UI_PICTURES - 1, art, 256, 256, key, x - sz * 0.08f, y + h - sz, sz, sz, 1.0f);
}

/* Any player's banner (port, Mary 2026-11-12): the online select screen's
 * and the fight's players, in place of the old badge (ui_badge_at). `look`
 * icon, alt, colour, dark, colour2, angle; `slot` 0 or 1 (each its own
 * fighter picture). */
void PortSkinBandOf(float x, float y, float w, float h, float art, float a, const char *name,
                    const char *sub, const int look[6]);

void ui_player_banner(int slot, const char *name, const char *sub, const int look[6],
                      float x, float y, float w, float h)
{
    static unsigned char *art[2];
    static unsigned have[2];
    float sz = h * 1.45f;
    unsigned key;

    if (slot < 0 || slot > 1)
        return;
    begin_2d();
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    PortSkinBegin();
    PortSkinBandOf(x, y, w, h, sz, 1.0f, name, sub, look);
    skin_end();
    end_2d();
    key = ((unsigned)look[0] * 2654435761u) ^ (look[1] ? 0x5bd1e995u : 0u) ^ 0x3c6ef372u
          ^ (unsigned)(slot + 1);
    if (key != have[slot] || !art[slot]) {
        free(art[slot]);
        art[slot] = prof_art_rgba(look[0], look[1]);
        have[slot] = key;
    }
    if (art[slot])
        ui_picture_at(UI_PICTURES - 2 - slot, art[slot], 256, 256, key, x - sz * 0.08f, y + h - sz,
                      sz, sz, 1.0f);
}
