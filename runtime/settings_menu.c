/*
 * settings_menu.c -- the in-game settings menu (Esc). Port, not in the
 * binary: the iPhone game has no keyboard, no window and no launcher.
 *
 * What the launcher's GRAPHICS and CONTROLS sections set, changed while the
 * game runs, without starting it again. Esc (umk3.ini can rebind it as
 * key_settings=) opens it over the game, which freezes underneath as under
 * the debug menu (debug_menu.c, whose box it shares); Esc again, the pad's
 * east button (circle / B) or CLOSE closes it. Arrows or W/S move,
 * left/right change a value, Enter picks, Q / E or L1 / R1 change the tab;
 * the mouse works as in the debug menu. Every change is written to umk3.ini
 * at once with the launcher's own names, so the launcher shows it (it reads
 * the ini again when it comes to the front) and the next start keeps it;
 * the ini's other lines are kept as they were.
 *
 *     GRAPHICS   DISPLAY (window, borderless, fullscreen) and RESOLUTION,
 *                both taken by APPLY DISPLAY; 3D RESOLUTION, ANTIALIASING,
 *                TEXTURES, ASPECT, INTERFACE and ON-SCREEN CONTROLS at
 *                once. TEXTURES loads every texture again at the new
 *                quality (draw_gl.c, lime_textures_requality).
 *     KEYBOARD   PLAYER 1 / 2, LAYOUT 6 / 5 buttons (buttons=), then each
 *                key: Enter on it and press the new key (Esc cancels).
 *     GAMEPAD    PLAYER 1 / 2, the pad (AUTO, OFF, XINPUT, or the one the
 *                ini named), then each action's button: Enter on it and
 *                press the button on that player's pad (Esc cancels).
 *     PROFILE    the player's profile (profiles.c, shared with the
 *                launcher): the name (Enter, then type; Enter keeps it, Esc
 *                cancels), the icon (a fighter's bio art) and its colour,
 *                the installation's id, and RESET DATA (twice: the id
 *                stays). The icon shows above the box.
 *     Every tab ends in RESET (on KEYBOARD and GAMEPAD), CLOSE and
 *     QUIT GAME.
 */

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform/platform.h"
#include "debug_menu.h"
#include "settings_menu.h"
#include "profiles.h"

extern char Language[10];

static const char *tx(const char *es, const char *en)
{
    return Language[0] == 'E' && Language[1] == 'S' ? es : en;
}

/* The launcher's lists (launcher/app/lib/ini.js RES, renderer.js AA). */
static const struct { int w, h; } k_res[] = {
    { 480, 320 }, { 960, 640 }, { 1440, 960 }, { 1920, 1280 },
    { 2400, 1600 }, { 2880, 1920 }, { 3840, 2560 },
    { 1280, 720 }, { 1600, 900 }, { 1920, 1080 }, { 2560, 1440 },
    { 3840, 2160 }, { 1024, 768 }, { 1600, 1200 }, { 2048, 1536 },
    { 2560, 1080 }, { 3440, 1440 },
};
#define N_RES (int)(sizeof k_res / sizeof k_res[0])
static const int k_aa[] = { 0, 2, 4, 8, 16 };
static const char *const k_aspect[] = { "3:2", "4:3", "16:9", "21:9" };
static const char *const k_pb[PB_COUNT] = {
    "SOUTH", "EAST", "WEST", "NORTH", "L1", "R1", "L2", "R2",
    "SELECT", "START", "L3", "R3", "HOME",
};

/* Three categories down the side panel; GRAPHICS holds VIDEO, TEXTURES and
 * INTERFACE, CONTROLS the KEYBOARD and the GAMEPAD, each under its heading */
enum { PAGE_GFX, PAGE_CTL, PAGE_PROF, N_PAGES };

enum {
    R_DISPLAY, R_MONITOR, R_RES, R_APPLY, R_RRES, R_AA, R_TEXQ, R_ASPECT, R_UI, R_HIDE,
    R_PLAYER, R_LAYOUT, R_KEY, R_KRESET, R_DEVICE, R_PICONS, R_PADB, R_PRESET,
    R_PNAME, R_PICON, R_PCOLOR, R_PSHADE, R_PCOLOR2, R_PID, R_PRESET2,
    R_CLOSE, R_QUIT, R_HEAD, R_CSUB
};

/* The profile icons' names, in CharacterNames order. */
static const char *const k_fighter_es[PROF_ICONS] = {
    "KANO", "SONYA", "JAX", "NIGHTWOLF", "SUB-ZERO (SIN MASCARA)", "STRYKER",
    "SINDEL", "SEKTOR", "CYRAX", "KUNG LAO", "KABAL", "SHEEVA", "SHANG TSUNG",
    "LIU KANG", "SMOKE", "KITANA", "JADE", "MILEENA", "SCORPION", "REPTILE",
    "ERMAC", "SUB-ZERO", "SMOKE (HUMANO)", "NOOB SAIBOT",
};
static const char *const k_fighter_en[PROF_ICONS] = {
    "KANO", "SONYA", "JAX", "NIGHTWOLF", "SUB-ZERO (UNMASKED)", "STRYKER",
    "SINDEL", "SEKTOR", "CYRAX", "KUNG LAO", "KABAL", "SHEEVA", "SHANG TSUNG",
    "LIU KANG", "SMOKE", "KITANA", "JADE", "MILEENA", "SCORPION", "REPTILE",
    "ERMAC", "SUB-ZERO", "SMOKE (HUMAN)", "NOOB SAIBOT",
};
/* The launcher's swatches (renderer.js PROF_COLORS), with their names. */
static const struct { unsigned rgb; const char *es, *en; } k_pcol[] = {
    { 0xE2423B, "ROJO", "RED" }, { 0xB0171F, "CARMESI", "CRIMSON" },
    { 0x6E0B12, "SANGRE", "BLOOD" }, { 0xF28A2E, "NARANJA", "ORANGE" },
    { 0xF2C230, "DORADO", "GOLD" }, { 0xFFE45C, "AMARILLO", "YELLOW" },
    { 0x8CCB2E, "LIMA", "LIME" }, { 0x3DBE5B, "VERDE", "GREEN" },
    { 0x1E7A3C, "BOSQUE", "FOREST" }, { 0x3AC7A8, "TURQUESA", "TEAL" },
    { 0x1FB5C9, "AGUAMARINA", "AQUA" }, { 0x3A8EE6, "AZUL CIELO", "SKY BLUE" },
    { 0x1D4FB8, "AZUL REY", "ROYAL BLUE" }, { 0x16245E, "MARINO", "NAVY" },
    { 0x5A2A9C, "INDIGO", "INDIGO" }, { 0x8A55D8, "MORADO", "PURPLE" },
    { 0xC34FE0, "VIOLETA", "VIOLET" }, { 0xE2509B, "ROSA", "PINK" },
    { 0xFF9DC0, "ROSA PALIDO", "ROSE" }, { 0x8B5A2B, "CAFE", "BROWN" },
    { 0xD9C39A, "ARENA", "SAND" }, { 0xFFFFFF, "BLANCO", "WHITE" },
    { 0x8A97A8, "PLATA", "SILVER" }, { 0x2A2F38, "GRAFITO", "GRAPHITE" },
    { 0x000000, "NEGRO", "BLACK" },
};
#define N_PCOL (int)(sizeof k_pcol / sizeof k_pcol[0])

static char g_edit[PROF_NAME_MAX * 4 + 1];     /* the name being typed */
static int  g_del_armed;
static int  g_icon_key = -1;                    /* icon and colour shown */

#define MAX_ROWS 48
#define VIS_ROWS 15
static struct {
    int  kind, arg;
    char label[40], value[192];
    int  en;
} g_rows[MAX_ROWS];
static int g_n;

static int g_open, g_want, g_page, g_sel, g_first, g_player, g_dirty;
/* Opened as the game's button setup (set_menu_open_controls): the CONTROLS
 * page alone, in the game's own panel (PortControlsPanel, FrontEnd.c). */
static int g_framed;
static int g_fsub;                      /* CONTROLS' page: 0 the keys, 1 the pad */
void PortControlsPanel(const char *title, const char *const *label,
                       const char *const *value, const int *enabled, int rows,
                       int sel, const char *footer, float *bx, float *top,
                       float *bw, float *rowh);
extern int limeScreenHeight;
static struct port_gfx g_gfx;           /* the menu's */
static int g_disp_mode, g_disp_w, g_disp_h;   /* the window as applied */
static int g_disp_mon;
static int g_capture;                   /* 1 a key, 2 a pad button */
static int g_armed;                     /* nothing held since it began */
static struct ui_geo g_geo;
static float g_mx, g_my;
static int   g_mdown, g_mouse_was;
static PLAT_PAD g_pad[2], g_pad_was[2];
static int   g_esc_was;

/* The list a row with values opens (Enter, or a click on it), as the
 * launcher's drop-downs: its row, the options, the current one and the one
 * under the cursor. The icons and colours open a grid of pictures. */
#define MAX_DROP 128
#define VIS_DROP 13
static int  g_drop, g_drop_kind, g_drop_n, g_drop_cur, g_drop_sel, g_drop_first;
static char g_drop_lab[MAX_DROP][48];
static char g_drop_title[40];
static unsigned char *g_drop_cell[MAX_DROP];   /* the grid's pictures */
static struct ui_geo g_dgeo;
static struct ui_grid_geo g_ggeo;

/* The colour grid, the launcher's picker in the game: 15 hues across, a
 * grey column, seven shades from light to dark -- any colour, not only the
 * named ones. */
#define PAL_COLS 16
#define PAL_ROWS 7
static unsigned g_pal[PAL_COLS * PAL_ROWS];

static unsigned hsl(float h, float s, float l)
{
    float c = (1 - fabsf(2 * l - 1)) * s, x = c * (1 - fabsf(fmodf(h / 60.0f, 2) - 1));
    float m = l - c / 2, r = 0, g = 0, b = 0;

    if (h < 60)       { r = c; g = x; }
    else if (h < 120) { r = x; g = c; }
    else if (h < 180) { g = c; b = x; }
    else if (h < 240) { g = x; b = c; }
    else if (h < 300) { r = x; b = c; }
    else              { r = c; b = x; }
    return (unsigned)((r + m) * 255 + 0.5f) << 16 | (unsigned)((g + m) * 255 + 0.5f) << 8
         | (unsigned)((b + m) * 255 + 0.5f);
}

static void pal_make(void)
{
    static const float L[PAL_ROWS] = { 0.88f, 0.76f, 0.64f, 0.52f, 0.40f, 0.28f, 0.16f };
    static const unsigned grey[PAL_ROWS] = { 0xFFFFFF, 0xD4D4D4, 0xAAAAAA, 0x808080,
                                             0x555555, 0x2B2B2B, 0x000000 };
    int r, c;

    for (r = 0; r < PAL_ROWS; r++) {
        for (c = 0; c < PAL_COLS - 1; c++)
            g_pal[r * PAL_COLS + c] = hsl(c * 24.0f, 0.85f, L[r]);
        g_pal[r * PAL_COLS + PAL_COLS - 1] = grey[r];
    }
}

int set_menu_is_open(void) { return g_open; }

/* The keys as game_main.c reads them: a scripted test's (UMK3_KEYS) too. */
int (*set_menu_key)(int code) = plat_key;
#define plat_key(code) set_menu_key(code)

static void save_ini(void);

static void close_menu(void)
{
    g_open = 0;
    g_framed = 0;
    g_capture = 0;
    if (g_dirty)
        save_ini();
    g_dirty = 0;
    printf("settings menu: closed\n");
}

void set_menu_toggle(void)
{
    if (g_open)
        close_menu();
    else {
        g_want = 1;                     /* opened after the next frame */
        g_framed = 0;
    }
}

void set_menu_open_controls(void)
{
    if (g_open || g_want)
        return;
    g_want = 1;
    g_framed = 1;
    g_page = PAGE_CTL;
    g_fsub = 0;
    g_sel = 0;
    g_first = 0;
}

void set_menu_after_frame(int vx, int vy, int vw, int vh)
{
    if (!g_want)
        return;
    g_want = 0;
    ui_grab(vx, vy, vw, vh);
    port_gfx_get(&g_gfx);
    g_disp_mode = g_gfx.mode;
    g_disp_w = g_gfx.w;
    g_disp_h = g_gfx.h;
    g_disp_mon = g_gfx.monitor;
    prof_load(port_ini_path(), tx("JUGADOR", "PLAYER"));    /* the launcher may have changed it */
    g_icon_key = -1;
    g_del_armed = 0;
    g_open = 1;
    g_mouse_was = 1;
    g_esc_was = 1;                      /* the press that opened it */
    memcpy(g_pad_was, g_pad, sizeof g_pad_was);
    printf("settings menu: open\n");
}

void set_menu_mouse(float x, float y, int down)
{
    g_mx = x * (g_geo.ow > 0 ? g_geo.ow : 480.0f);
    g_my = y * 320.0f;
    g_mdown = down;
}

/* The footer shows the pictures of what the player is using: 0 the
 * keyboard (and mouse), 1 player one's pad, 2 player two's. */
static int g_using;

static int pad_busy(const PLAT_PAD *p)
{
    return p->connected && (p->buttons || p->up || p->down || p->left || p->right);
}

void set_menu_pads(const PLAT_PAD *p1, const PLAT_PAD *p2)
{
    if (pad_busy(p1))
        g_using = 1;
    else if (pad_busy(p2))
        g_using = 2;
    else if (plat_key_any() || (g_using == 1 && !p1->connected)
             || (g_using == 2 && !p2->connected))
        g_using = 0;
    g_pad[0] = *p1;
    g_pad[1] = *p2;
}

/* ---------------------------------------------------------------- keys */

struct kdef { int code; const char *es, *en; };

static const struct kdef k_dirs[4] = {
    { PK_UP, "ARRIBA", "UP" }, { PK_DOWN, "ABAJO", "DOWN" },
    { PK_LEFT, "IZQUIERDA", "LEFT" }, { PK_RIGHT, "DERECHA", "RIGHT" },
};
static const struct kdef k_six[6] = {
    { PK_HP, "PUNO ALTO", "HIGH PUNCH" }, { PK_LP, "PUNO BAJO", "LOW PUNCH" },
    { PK_BL, "BLOQUEO", "BLOCK" }, { PK_HK, "PATADA ALTA", "HIGH KICK" },
    { PK_LK, "PATADA BAJA", "LOW KICK" }, { PK_RUN, "CORRER", "RUN" },
};
static const struct kdef k_five[4] = {
    { PK_5_P, "PUNO", "PUNCH" }, { PK_5_B, "BLOQUEO", "BLOCK" },
    { PK_5_K, "PATADA", "KICK" }, { PK_5_R, "CORRER", "RUN" },
};

/* Player one's code for a key, as player two's. */
static int p2_code(int code)
{
    if (code >= PK_UP && code <= PK_RUN)
        return code - PK_UP + PK_P2_UP;
    if (code >= PK_5_P && code <= PK_5_R)
        return code - PK_5_P + PK_P2_5_P;
    if (code == PK_SPECIAL)
        return PK_P2_SPECIAL;
    if (code == PK_PAUSE || code == PK_MOVES)
        return code - PK_PAUSE + PK_P2_PAUSE;
    if (code == PK_OK)
        return PK_P2_OK;
    return code;
}

/* The launcher's defaults (Windows virtual-key codes). */
static void reset_keys(int player)
{
    static const int p1[10] = { 'W', 'S', 'A', 'D', 'U', 'I', 'O', 'J', 'K', 'L' };
    static const int p2[10] = { 38, 40, 37, 39, 103, 104, 105, 100, 101, 102 };
    static const int p1_5[4] = { 'U', 'O', 'J', 'L' };
    static const int p2_5[4] = { 103, 105, 100, 102 };
    int i;

    for (i = 0; i < 10; i++)
        plat_bind_key((player ? PK_P2_UP : PK_UP) + i, player ? p2[i] : p1[i]);
    for (i = 0; i < 4; i++)
        plat_bind_key((player ? PK_P2_5_P : PK_5_P) + i,
                      player ? p2_5[i] : p1_5[i]);
    plat_bind_key(player ? PK_P2_SPECIAL : PK_SPECIAL, player ? 96 : 'H');
    if (!player) {
        plat_bind_key(PK_PAUSE, 'P');
        plat_bind_key(PK_MOVES, 'M');
    } else {
        plat_bind_key(PK_P2_PAUSE, 0x6f);   /* the keypad's / * 1 */
        plat_bind_key(PK_P2_MOVES, 0x6a);
        plat_bind_key(PK_P2_OK, 0x61);
    }
}

/* ----------------------------------------------------------------- pad */

#define B(x) (1u << PB_##x)
static const unsigned k_pad_def[17] = {
    B(WEST), B(SOUTH), B(R1), B(NORTH), B(EAST), B(L1) | B(L2), B(R2),
    B(WEST) | B(NORTH), 0, B(R1), B(SOUTH) | B(EAST), 0, B(L1) | B(L2), B(R2),
    B(START), B(SELECT), B(HOME),
};

static void reset_pad(int player)
{
    int i;

    for (i = 0; i < 17; i++)
        *port_pad_action(player, i) = k_pad_def[i];
}

/* Which pad's pictures: the one connected, else the one the ini names. */
static int pad_kind(int pl)
{
    const char *dev = port_pad_device(pl);

    if (plat_pad_glyph_style(pl, -1))
        return plat_pad_glyph_style(pl, -1);
    if (g_pad[pl].connected)
        return g_pad[pl].kind;
    if (strncmp(dev, "054c", 4) == 0 || strncmp(dev, "054C", 4) == 0)
        return plat_pad_sony_kind((unsigned)strtoul(dev + 5, NULL, 16));
    if (strncmp(dev, "057e", 4) == 0 || strncmp(dev, "057E", 4) == 0)
        return 3;
    return 1;
}

/* A combination as umk3.ini writes it: the buttons' names joined by "+". */
static void mask_names(unsigned m, char *out, size_t n)
{
    size_t k = 0;
    int i;

    out[0] = 0;
    for (i = 0; i < PB_COUNT; i++)
        if (m & (1u << i))
            k += (size_t)snprintf(out + k, n > k ? n - k : 0, "%s%s",
                                  k ? "+" : "", k_pb[i]);
}

/* The same as pictures, for the menu. */
static void mask_text(unsigned m, char *out, size_t n)
{
    size_t k = 0;
    char g[96];
    int i;

    out[0] = 0;
    for (i = 0; i < PB_COUNT; i++)
        if (m & (1u << i)) {
            plat_pad_glyph(pad_kind(g_player), i, g, sizeof g);
            k += (size_t)snprintf(out + k, n > k ? n - k : 0, "%s%s",
                                  k ? "+" : "", g);
        }
}

/* The pad rows: the action's index for port_pad_action, with the labels. */
static const struct kdef k_pad6[10] = {
    { 0, "PUNO ALTO", "HIGH PUNCH" }, { 1, "PUNO BAJO", "LOW PUNCH" },
    { 2, "BLOQUEO", "BLOCK" }, { 3, "PATADA ALTA", "HIGH KICK" },
    { 4, "PATADA BAJA", "LOW KICK" }, { 5, "CORRER", "RUN" },
    { 6, "ESPECIAL", "SPECIAL" }, { 14, "PAUSA", "PAUSE" },
    { 15, "LISTA DE MOVIMIENTOS", "MOVES LIST" },
    { 16, "ABRIR AJUSTES", "OPEN SETTINGS" },
};
static const struct kdef k_pad5[8] = {
    { 7, "PUNO", "PUNCH" }, { 9, "BLOQUEO", "BLOCK" },
    { 10, "PATADA", "KICK" }, { 12, "CORRER", "RUN" },
    { 6, "ESPECIAL", "SPECIAL" }, { 14, "PAUSA", "PAUSE" },
    { 15, "LISTA DE MOVIMIENTOS", "MOVES LIST" },
    { 16, "ABRIR AJUSTES", "OPEN SETTINGS" },
};

static const char *const k_dev[3] = { "auto", "off", "xinput" };

/* ---------------------------------------------------------------- rows */

static void add(int kind, int arg, const char *label, const char *fmt, ...)
{
    va_list ap;

    if (g_n >= MAX_ROWS)
        return;
    g_rows[g_n].kind = kind;
    g_rows[g_n].arg = arg;
    g_rows[g_n].en = 1;
    snprintf(g_rows[g_n].label, sizeof g_rows[g_n].label, "%s", label);
    va_start(ap, fmt);
    vsnprintf(g_rows[g_n].value, sizeof g_rows[g_n].value, fmt, ap);
    va_end(ap);
    g_n++;
}

/* A section's heading: shown, never chosen (plat_ui_menu draws "\002"). */
static void head(const char *name)
{
    add(R_HEAD, 0, "", "");
    snprintf(g_rows[g_n - 1].label, sizeof g_rows[0].label, "\002%s", name);
    g_rows[g_n - 1].en = 0;
}

/* A colour row: its name in the palette, or #RRGGBB for one the launcher's
 * picker chose. */
static void add_colour(int kind, const char *label, unsigned rgb)
{
    int i;

    for (i = 0; i < N_PCOL && k_pcol[i].rgb != rgb; i++)
        ;
    if (i < N_PCOL)
        add(kind, 0, label, "< %s >", tx(k_pcol[i].es, k_pcol[i].en));
    else
        add(kind, 0, label, "< #%06X >", rgb);
}

static int res_index(int w, int h)
{
    int i;

    for (i = 0; i < N_RES; i++)
        if (k_res[i].w == w && k_res[i].h == h)
            return i;
    return -1;
}

static const char *player_text(void)
{
    return g_player ? tx("< JUGADOR 2 (DUELO) >", "< PLAYER 2 (DUEL) >")
                    : tx("< JUGADOR 1 >", "< PLAYER 1 >");
}

static void key_row(int code)
{
    char k[96];
    const struct kdef *d = NULL;
    int i;

    for (i = 0; i < 4; i++) if (k_dirs[i].code == code) d = &k_dirs[i];
    for (i = 0; i < 6; i++) if (k_six[i].code == code) d = &k_six[i];
    for (i = 0; i < 4; i++) if (k_five[i].code == code) d = &k_five[i];
    if (g_player)
        code = p2_code(code);
    if (g_capture == 1 && g_n == g_sel)
        snprintf(k, sizeof k, "%s", tx("PULSA UNA TECLA...", "PRESS A KEY..."));
    else
        plat_key_glyph(code, k, sizeof k);
    if (d)
        add(R_KEY, code, tx(d->es, d->en), "%s", k);
    else if (code == PK_SPECIAL || code == PK_P2_SPECIAL)
        add(R_KEY, code, tx("ESPECIAL", "SPECIAL"), "%s", k);
    else if (code == PK_PAUSE || code == PK_P2_PAUSE)
        add(R_KEY, code, tx("PAUSA", "PAUSE"), "%s", k);
    else if (code == PK_P2_OK)
        add(R_KEY, code, tx("ACEPTAR", "ACCEPT"), "%s", k);
    else
        add(R_KEY, code, tx("LISTA DE MOVIMIENTOS", "MOVES LIST"), "%s", k);
}

static void build(void)
{
    char v[64];
    int i;

    g_n = 0;
    switch (g_page) {
    case PAGE_GFX: {
        static const char *mode_es[3] = { "VENTANA", "SIN BORDES", "PANTALLA COMPLETA" };
        static const char *mode_en[3] = { "WINDOW", "BORDERLESS", "FULLSCREEN" };
        static const char *tq_es[3] = { "IPHONE", "IPAD", "SIN LIMITE" };
        static const char *tq_en[3] = { "IPHONE", "IPAD", "NO LIMIT" };

        head("VIDEO");
        add(R_DISPLAY, 0, tx("PANTALLA", "DISPLAY"), "< %s >",
            tx(mode_es[g_gfx.mode], mode_en[g_gfx.mode]));
        if (g_gfx.monitor) {
            plat_monitor_name(g_gfx.monitor, v, sizeof v);
            add(R_MONITOR, 0, "MONITOR", "< %s >", v);
        } else
            add(R_MONITOR, 0, "MONITOR", "< %s >", tx("AUTOMATICO", "AUTOMATIC"));
        add(R_RES, 0, tx("RESOLUCION", "RESOLUTION"), "< %dx%d >", g_gfx.w, g_gfx.h);
        if (g_gfx.mode == 1)
            g_rows[g_n - 1].en = 0;     /* borderless is the monitor's size */
        add(R_APPLY, 0, tx("APLICAR PANTALLA", "APPLY DISPLAY"), "%s",
            g_gfx.mode != g_disp_mode || g_gfx.monitor != g_disp_mon
            || (g_gfx.mode != 1 && (g_gfx.w != g_disp_w || g_gfx.h != g_disp_h))
            ? tx("PENDIENTE", "PENDING") : "");
        g_rows[g_n - 1].en = g_rows[g_n - 1].value[0] != 0;
        if (g_gfx.rw > 0)
            add(R_RRES, 0, tx("RESOLUCION 3D", "3D RESOLUTION"), "< %dx%d >",
                g_gfx.rw, g_gfx.rh);
        else
            add(R_RRES, 0, tx("RESOLUCION 3D", "3D RESOLUTION"), "< %s >",
                tx("LA DE LA VENTANA", "THE WINDOW'S"));
        if (g_gfx.aa)
            add(R_AA, 0, "ANTIALIASING", "< X%d >", g_gfx.aa);
        else
            add(R_AA, 0, "ANTIALIASING", "< %s >", tx("DESACTIVADO", "OFF"));
        head(tx("TEXTURAS", "TEXTURES"));
        add(R_TEXQ, 0, tx("CALIDAD", "QUALITY"), "< %s >",
            tx(tq_es[g_gfx.texq], tq_en[g_gfx.texq]));
        head(tx("INTERFAZ", "INTERFACE"));
        add(R_ASPECT, 0, tx("RELACION DE ASPECTO", "ASPECT RATIO"), "< %s >",
            g_gfx.aspect);
        add(R_UI, 0, tx("TAMANO DE LA INTERFAZ", "INTERFACE SIZE"), "< %s >",
            g_gfx.ui ? "IPAD" : "IPHONE");
        add(R_HIDE, 0, tx("CONTROLES EN PANTALLA", "ON-SCREEN CONTROLS"), "< %s >",
            g_gfx.hidectl ? tx("OCULTOS", "HIDDEN") : tx("VISIBLES", "SHOWN"));
        break;
    }
    case PAGE_CTL: {
        int five = port_buttons() == 5;
        const struct kdef *d = five ? k_pad5 : k_pad6;
        int nd = five ? 8 : 10;
        const char *dev = port_pad_device(g_player);
        const PLAT_PAD *p = &g_pad[g_player];

        /* the keys and the pad on pages of their own */
        add(R_CSUB, 0, tx("SECCION", "SECTION"), "< %s >",
            g_fsub ? tx("MANDO", "GAMEPAD") : tx("TECLADO", "KEYBOARD"));
        add(R_PLAYER, 0, tx("JUGADOR", "PLAYER"), "%s", player_text());
        add(R_LAYOUT, 0, tx("BOTONES", "LAYOUT"), "< %d %s >", five ? 5 : 6,
            tx("BOTONES", "BUTTONS"));
        if (g_fsub)                     /* the pad's page */
            goto pad;
        head(tx("TECLADO", "KEYBOARD"));
        for (i = 0; i < 4; i++)
            key_row(k_dirs[i].code);
        if (five)
            for (i = 0; i < 4; i++)
                key_row(k_five[i].code);
        else
            for (i = 0; i < 6; i++)
                key_row(k_six[i].code);
        key_row(PK_SPECIAL);
        key_row(PK_PAUSE);              /* player two's too */
        key_row(PK_MOVES);
        if (g_player)
            key_row(PK_OK);             /* player one's is Enter */
        add(R_KRESET, 0, tx("RESTABLECER TECLAS", "RESET KEYS"), "");
        break;                          /* the keys' page ends here */
pad:
        head(tx("MANDO", "GAMEPAD"));
        snprintf(v, sizeof v, "%s", dev);
        for (i = 0; v[i]; i++)
            if (v[i] >= 'a' && v[i] <= 'z')
                v[i] = (char)(v[i] - 32);
        if (strcmp(dev, "off") == 0)
            add(R_DEVICE, 0, tx("MANDO", "GAMEPAD"), "< %s >", tx("NINGUNO", "NONE"));
        else
            add(R_DEVICE, 0, tx("MANDO", "GAMEPAD"), "< %s > %.24s", v,
                p->connected ? p->name : tx("(NO CONECTADO)", "(NOT CONNECTED)"));
        {
            static const char *const st_es[5] = { "AUTO (LAS DEL MANDO)", "XBOX", "PLAYSTATION 5", "NINTENDO", "PLAYSTATION 4" };
            static const char *const st_en[5] = { "AUTO (THE PAD'S OWN)", "XBOX", "PLAYSTATION 5", "NINTENDO", "PLAYSTATION 4" };
            int st = plat_pad_glyph_style(g_player, -1);
            add(R_PICONS, 0, tx("ICONOS DE BOTONES", "BUTTON ICONS"), "< %s >",
                tx(st_es[st], st_en[st]));
        }
        for (i = 0; i < nd; i++) {
            if (g_capture == 2 && g_sel == g_n)
                snprintf(v, sizeof v, "%s",
                         tx("PULSA UN BOTON...", "PRESS A BUTTON..."));
            else {
                mask_text(*port_pad_action(g_player, d[i].code), v, sizeof v);
                if (!v[0])
                    snprintf(v, sizeof v, "-");
            }
            add(R_PADB, d[i].code, tx(d[i].es, d[i].en), "%s", v);
        }
        add(R_PRESET, 0, tx("RESTABLECER MANDO", "RESET GAMEPAD"), "");
        break;
    }
    case PAGE_PROF: {
        struct profile *p = prof_get();

        if (p) {
            if (g_capture == 3)
                add(R_PNAME, 0, tx("NOMBRE", "NAME"), "%s_", g_edit);
            else
                add(R_PNAME, 0, tx("NOMBRE", "NAME"), "%s", p->name[0] ? p->name : "-");
            add(R_PICON, 0, tx("ICONO", "ICON"), "< %s >",
                tx(k_fighter_es[p->icon], k_fighter_en[p->icon]));
            add_colour(R_PCOLOR, tx("COLOR DE FONDO", "BACKGROUND COLOUR"), p->color);
            add(R_PSHADE, 0, tx("DEGRADADO", "GRADIENT"), "< %s >",
                p->dark == 2 ? tx("PERSONALIZADO", "CUSTOM")
                : p->dark ? tx("OSCURO", "DARK")
                          : tx("CLARO", "LIGHT"));
            add_colour(R_PCOLOR2, p->dark == 2 ? tx("HACIA EL COLOR", "INTO THE COLOUR")
                                  : p->dark ? tx("COLOR OSCURO", "DARK COLOUR")
                                            : tx("COLOR CLARO", "LIGHT COLOUR"), p->color2);
            add(R_PID, 0, tx("ID DE JUGADOR", "PLAYER ID"), "%s", p->id);
            g_rows[g_n - 1].en = 0;     /* shown, not chosen */
            add(R_PRESET2, 0, tx("REINICIAR DATOS", "RESET DATA"), "%s",
                g_del_armed ? tx("PULSA OTRA VEZ", "PRESS AGAIN") : "");
        }
        break;
    }
    }
    if (g_framed)                       /* the button setup: BACK leaves */
        return;
    add(R_CLOSE, 0, tx("VOLVER AL JUEGO", "BACK TO THE GAME"), "");
    add(R_QUIT, 0, tx("SALIR DEL JUEGO", "QUIT THE GAME"), "");
}

static int has_value(int kind)
{
    switch (kind) {
    case R_DISPLAY: case R_MONITOR: case R_RES: case R_RRES: case R_AA: case R_TEXQ:
    case R_ASPECT: case R_UI: case R_HIDE: case R_PLAYER: case R_LAYOUT:
    case R_DEVICE: case R_PICON: case R_PCOLOR: case R_PSHADE: case R_PCOLOR2:
    case R_PICONS:
        return 1;
    }
    return 0;
}

static int wrap(int v, int n) { return (v % n + n) % n; }

static void change(int kind, int d)
{
    int i;

    switch (kind) {
    case R_CSUB:
        g_fsub ^= 1;
        return;
    case R_DISPLAY:
        g_gfx.mode = wrap(g_gfx.mode + d, 3);
        return;
    case R_MONITOR:                     /* applied with APPLY DISPLAY */
        g_gfx.monitor = wrap(g_gfx.monitor + d, plat_monitor_count() + 1);
        return;
    case R_RES:
        i = res_index(g_gfx.w, g_gfx.h);
        i = i < 0 ? 0 : wrap(i + d, N_RES);
        g_gfx.w = k_res[i].w;
        g_gfx.h = k_res[i].h;
        return;
    case R_PLAYER:
        g_player = !g_player;
        return;
    case R_LAYOUT:
        port_buttons_set(port_buttons() == 5 ? 6 : 5);
        g_dirty = 1;
        return;
    case R_DEVICE: {
        const char *dev = port_pad_device(g_player);
        static char other[2][32];       /* a vvvv:pppp the ini named */
        int n = 3;

        for (i = 0; i < 3 && strcmp(dev, k_dev[i]) != 0; i++)
            ;
        if (i == 3) {
            snprintf(other[g_player], sizeof other[0], "%s", dev);
            n = 4;
        } else if (other[g_player][0])
            n = 4;
        i = wrap(i + d, n);
        port_pad_device_set(g_player, i < 3 ? k_dev[i] : other[g_player]);
        g_dirty = 1;
        return;
    }
    case R_PICONS:
        {
            /* in the order the row shows them: auto, Xbox, PS5, PS4, Nintendo */
            static const int order[5] = { 0, 1, 2, 4, 3 };
            int st = plat_pad_glyph_style(g_player, -1), at = 0;
            for (i = 0; i < 5; i++)
                if (order[i] == st)
                    at = i;
            plat_pad_glyph_style(g_player, order[wrap(at + d, 5)]);
        }
        g_dirty = 1;
        return;
    case R_PICON:
        if (prof_get()) {
            prof_get()->icon = wrap(prof_get()->icon + d, PROF_ICONS);
            prof_save();
        }
        return;
    case R_PSHADE:
        if (prof_get()) {
            struct profile *p = prof_get();

            p->dark = wrap(p->dark + d, 3);
            p->color2 = prof_shade_default(p->dark);
            prof_save();
        }
        return;
    case R_PCOLOR: case R_PCOLOR2: {
        struct profile *p = prof_get();
        unsigned *c;

        if (!p)
            return;
        c = kind == R_PCOLOR ? &p->color : &p->color2;
        for (i = 0; i < N_PCOL && k_pcol[i].rgb != *c; i++)
            ;
        i = i < N_PCOL ? wrap(i + d, N_PCOL) : (d > 0 ? 0 : N_PCOL - 1);
        *c = k_pcol[i].rgb;
        prof_save();
        return;
    }
    case R_RRES:
        i = g_gfx.rw > 0 ? res_index(g_gfx.rw, g_gfx.rh) + 1 : 0;
        i = wrap(i + d, N_RES + 1);
        g_gfx.rw = i ? k_res[i - 1].w : 0;
        g_gfx.rh = i ? k_res[i - 1].h : 0;
        break;
    case R_AA:
        for (i = 0; i < 5 && k_aa[i] != g_gfx.aa; i++)
            ;
        g_gfx.aa = k_aa[wrap((i < 5 ? i : 0) + d, 5)];
        break;
    case R_TEXQ:   g_gfx.texq = wrap(g_gfx.texq + d, 3); break;
    case R_UI:     g_gfx.ui = !g_gfx.ui; break;
    case R_HIDE:   g_gfx.hidectl = !g_gfx.hidectl; break;
    case R_ASPECT:
        for (i = 0; i < 4 && strcmp(k_aspect[i], g_gfx.aspect) != 0; i++)
            ;
        snprintf(g_gfx.aspect, sizeof g_gfx.aspect, "%s",
                 k_aspect[wrap((i < 4 ? i : 0) + d, 4)]);
        break;
    default:
        return;
    }
    port_gfx_set(&g_gfx, 0);            /* the ones that take at once */
    g_dirty = 1;
}

/* ------------------------------------------------------------ lists */

static int drop_grid(int kind)
{
    return kind == R_PICON || kind == R_PCOLOR || kind == R_PCOLOR2;
}

static void drop_free(void)
{
    int i;

    for (i = 0; i < MAX_DROP; i++) {
        free(g_drop_cell[i]);
        g_drop_cell[i] = NULL;
    }
}

/* A colour's swatch for the grid: a glossy disc with a white rim. */
static unsigned char *swatch(unsigned rgb, int S)
{
    unsigned char *o = (unsigned char *)malloc((size_t)S * S * 4);
    float c[3] = { (float)((rgb >> 16) & 255), (float)((rgb >> 8) & 255),
                   (float)(rgb & 255) }, R = S * 0.46f;
    int x, y, i;

    if (!o)
        return NULL;
    for (y = 0; y < S; y++)
        for (x = 0; x < S; x++) {
            float dx = x + 0.5f - S * 0.5f, dy = y + 0.5f - S * 0.5f;
            float d = sqrtf(dx * dx + dy * dy), a = R + 0.5f - d, rim, g;
            unsigned char *p = o + ((size_t)y * S + x) * 4;

            a = a < 0 ? 0 : a > 1 ? 1 : a;
            rim = d > R - S * 0.06f ? 1.0f : 0.0f;
            g = dy < 0 ? 0.4f * (-dy / R) : 0;          /* the gloss */
            for (i = 0; i < 3; i++) {
                float v = rim ? 235 : c[i] + (255 - c[i]) * g;
                p[i] = (unsigned char)(v > 255 ? 255 : v);
            }
            p[3] = (unsigned char)(a * 255);
        }
    return o;
}

/* The options of a row, the current one in g_drop_cur (-1: none of them). */
static int drop_fill(int kind)
{
    static const char *mode_es[3] = { "VENTANA", "SIN BORDES", "PANTALLA COMPLETA" };
    static const char *mode_en[3] = { "WINDOW", "BORDERLESS", "FULLSCREEN" };
    static const char *tq_es[3] = { "IPHONE", "IPAD", "SIN LIMITE" };
    static const char *tq_en[3] = { "IPHONE", "IPAD", "NO LIMIT" };
    static const char *ic_es[5] = { "AUTO (LAS DEL MANDO)", "XBOX", "PLAYSTATION 5", "PLAYSTATION 4", "NINTENDO" };
    static const char *ic_en[5] = { "AUTO (THE PAD'S OWN)", "XBOX", "PLAYSTATION 5", "PLAYSTATION 4", "NINTENDO" };
    static const int ic_order[5] = { 0, 1, 2, 4, 3 };
    struct profile *p = prof_get();
    char (*L)[48] = g_drop_lab;
    int n = 0, i;

    g_drop_cur = -1;
    switch (kind) {
    case R_DISPLAY:
        for (n = 0; n < 3; n++)
            snprintf(L[n], 48, "%s", tx(mode_es[n], mode_en[n]));
        g_drop_cur = g_gfx.mode;
        break;
    case R_MONITOR:
        snprintf(L[0], 48, "%s", tx("AUTOMATICO", "AUTOMATIC"));
        for (n = 1; n <= plat_monitor_count() && n < MAX_DROP; n++)
            plat_monitor_name(n, L[n], 48);
        g_drop_cur = g_gfx.monitor;
        break;
    case R_RES:
        for (n = 0; n < N_RES; n++)
            snprintf(L[n], 48, "%d x %d", k_res[n].w, k_res[n].h);
        g_drop_cur = res_index(g_gfx.w, g_gfx.h);
        break;
    case R_RRES:
        snprintf(L[0], 48, "%s", tx("LA DE LA VENTANA", "THE WINDOW'S"));
        for (n = 1; n <= N_RES; n++)
            snprintf(L[n], 48, "%d x %d", k_res[n - 1].w, k_res[n - 1].h);
        g_drop_cur = g_gfx.rw > 0 ? res_index(g_gfx.rw, g_gfx.rh) + 1 : 0;
        break;
    case R_AA:
        for (n = 0; n < 5; n++) {
            if (k_aa[n])
                snprintf(L[n], 48, "X%d", k_aa[n]);
            else
                snprintf(L[n], 48, "%s", tx("DESACTIVADO", "OFF"));
            if (k_aa[n] == g_gfx.aa)
                g_drop_cur = n;
        }
        break;
    case R_TEXQ:
        for (n = 0; n < 3; n++)
            snprintf(L[n], 48, "%s", tx(tq_es[n], tq_en[n]));
        g_drop_cur = g_gfx.texq;
        break;
    case R_ASPECT:
        for (n = 0; n < 4; n++) {
            snprintf(L[n], 48, "%s", k_aspect[n]);
            if (strcmp(k_aspect[n], g_gfx.aspect) == 0)
                g_drop_cur = n;
        }
        break;
    case R_UI:
        snprintf(L[0], 48, "IPHONE");
        snprintf(L[1], 48, "IPAD");
        n = 2;
        g_drop_cur = g_gfx.ui ? 1 : 0;
        break;
    case R_HIDE:
        snprintf(L[0], 48, "%s", tx("VISIBLES", "SHOWN"));
        snprintf(L[1], 48, "%s", tx("OCULTOS", "HIDDEN"));
        n = 2;
        g_drop_cur = g_gfx.hidectl ? 1 : 0;
        break;
    case R_PLAYER:
        snprintf(L[0], 48, "%s", tx("JUGADOR 1", "PLAYER 1"));
        snprintf(L[1], 48, "%s", tx("JUGADOR 2 (DUELO)", "PLAYER 2 (DUEL)"));
        n = 2;
        g_drop_cur = g_player;
        break;
    case R_LAYOUT:
        snprintf(L[0], 48, "6 %s", tx("BOTONES", "BUTTONS"));
        snprintf(L[1], 48, "5 %s", tx("BOTONES", "BUTTONS"));
        n = 2;
        g_drop_cur = port_buttons() == 5 ? 1 : 0;
        break;
    case R_DEVICE: {
        const char *dev = port_pad_device(g_player);

        snprintf(L[0], 48, "AUTO");
        snprintf(L[1], 48, "%s", tx("NINGUNO", "NONE"));
        snprintf(L[2], 48, "XINPUT (XBOX)");
        n = 3;
        for (i = 0; i < 3 && strcmp(dev, k_dev[i]) != 0; i++)
            ;
        if (i == 3) {                   /* the vvvv:pppp the ini names */
            snprintf(L[3], 48, "%s", g_pad[g_player].connected ? g_pad[g_player].name : dev);
            n = 4;
        }
        g_drop_cur = i;
        break;
    }
    case R_PICONS: {
        int st = plat_pad_glyph_style(g_player, -1);
        for (n = 0; n < 5; n++) {
            snprintf(L[n], 48, "%s", tx(ic_es[n], ic_en[n]));
            if (ic_order[n] == st)
                g_drop_cur = n;
        }
        break;
    }
    case R_PSHADE:
        snprintf(L[0], 48, "%s", tx("CLARO", "LIGHT"));
        snprintf(L[1], 48, "%s", tx("OSCURO", "DARK"));
        snprintf(L[2], 48, "%s", tx("PERSONALIZADO", "CUSTOM"));
        n = 3;
        g_drop_cur = p ? p->dark : 0;
        break;
    case R_PICON:
        if (!p)
            return 0;
        for (n = 0; n < PROF_ICONS; n++) {
            snprintf(L[n], 48, "%s", tx(k_fighter_es[n], k_fighter_en[n]));
            g_drop_cell[n] = prof_icon_rgba(n, p->color, p->dark, p->color2, 96);
        }
        g_drop_cur = p->icon;
        break;
    case R_PCOLOR: case R_PCOLOR2: {
        unsigned have;

        if (!p)
            return 0;
        have = kind == R_PCOLOR ? p->color : p->color2;
        pal_make();
        for (n = 0; n < PAL_COLS * PAL_ROWS; n++) {
            for (i = 0; i < N_PCOL && k_pcol[i].rgb != g_pal[n]; i++)
                ;
            if (i < N_PCOL)
                snprintf(L[n], 48, "%s", tx(k_pcol[i].es, k_pcol[i].en));
            else
                snprintf(L[n], 48, "#%06X", g_pal[n]);
            g_drop_cell[n] = swatch(g_pal[n], 64);
            if (g_pal[n] == have)
                g_drop_cur = n;
        }
        break;
    }
    }
    return n;
}

static void drop_open(int kind)
{
    int i;

    drop_free();
    g_drop_n = drop_fill(kind);
    if (g_drop_n <= 0)
        return;
    for (i = 0; i < g_n && g_rows[i].kind != kind; i++)
        ;
    snprintf(g_drop_title, sizeof g_drop_title, "%s", i < g_n ? g_rows[i].label : "");
    g_drop = 1;
    g_drop_kind = kind;
    g_drop_sel = g_drop_cur >= 0 ? g_drop_cur : 0;
    g_drop_first = 0;
}

static void drop_close(void)
{
    g_drop = 0;
    drop_free();
}

/* Choose option i of the open list: the row's own step does it, so
 * everything that follows a change (applied at once, umk3.ini) follows. */
static void drop_apply(int i)
{
    int kind = g_drop_kind, cur = g_drop_cur;
    struct profile *p = prof_get();

    if (kind == R_RES) {
        g_gfx.w = k_res[i].w;
        g_gfx.h = k_res[i].h;
    } else if (kind == R_PCOLOR || kind == R_PCOLOR2) {
        if (p) {
            *(kind == R_PCOLOR ? &p->color : &p->color2) = g_pal[i];
            prof_save();
        }
    } else if (kind == R_PICON) {
        if (p) {
            p->icon = i;
            prof_save();
        }
    } else if (i != cur)
        change(kind, i - (cur < 0 ? 0 : cur));
    drop_close();
}

/* The open list's turn: arrows (a grid's go across too), Enter picks, Esc
 * or the pad's east button goes back, a click picks or, outside, goes back. */
static void drop_tick(int up, int down, int left, int right, int ok, int back,
                      int click)
{
    int grid = drop_grid(g_drop_kind), cols = grid ? g_ggeo.cols : 1;

    if (back) {
        drop_close();
        return;
    }
    if (grid && cols > 0) {
        if (left)  g_drop_sel = wrap(g_drop_sel - 1, g_drop_n);
        if (right) g_drop_sel = wrap(g_drop_sel + 1, g_drop_n);
        if (up && g_drop_sel - cols >= 0) g_drop_sel -= cols;
        if (down && g_drop_sel + cols < g_drop_n) g_drop_sel += cols;
    } else {
        if (up)   g_drop_sel = wrap(g_drop_sel - 1, g_drop_n);
        if (down) g_drop_sel = wrap(g_drop_sel + 1, g_drop_n);
    }
    if (click) {
        int i = -1;

        if (grid && g_ggeo.step > 0) {
            float fx = (g_mx - g_ggeo.x0) / g_ggeo.step, fy = (g_my - g_ggeo.y0) / g_ggeo.step;
            int cx = (int)floorf(fx), cy = (int)floorf(fy);
            if (fx >= 0 && fy >= 0 && cx < g_ggeo.cols
                && fx - cx < g_ggeo.cell / g_ggeo.step && fy - cy < g_ggeo.cell / g_ggeo.step)
                i = cy * g_ggeo.cols + cx;
        } else if (g_dgeo.rowh > 0 && g_mx >= g_dgeo.bx && g_mx < g_dgeo.bx + g_dgeo.bw
                   && g_my >= g_dgeo.top) {
            i = g_drop_first + (int)((g_my - g_dgeo.top) / g_dgeo.rowh);
            if (i - g_drop_first >= VIS_DROP)
                i = -1;
        }
        if (i >= 0 && i < g_drop_n) {
            drop_apply(i);
            return;
        }
        drop_close();
        return;
    }
    if (ok)
        drop_apply(g_drop_sel);
}

static void pick(int kind)
{
    switch (kind) {
    case R_CSUB:
        g_fsub ^= 1;
        break;
    case R_APPLY:
        port_gfx_set(&g_gfx, 1);
        port_gfx_get(&g_gfx);
        g_disp_mode = g_gfx.mode;
        g_disp_w = g_gfx.w;
        g_disp_h = g_gfx.h;
        g_disp_mon = g_gfx.monitor;
        g_dirty = 1;
        break;
    case R_KEY:
        g_capture = 1;
        g_armed = 0;
        break;
    case R_PADB:
        g_capture = 2;
        g_armed = 0;
        break;
    case R_KRESET:
        reset_keys(g_player);
        g_dirty = 1;
        break;
    case R_PRESET:
        reset_pad(g_player);
        g_dirty = 1;
        break;
    case R_PNAME:
        if (prof_get()) {
            snprintf(g_edit, sizeof g_edit, "%s", prof_get()->name);
            plat_text_clear();
            g_capture = 3;
        }
        break;
    case R_PRESET2:
        if (!g_del_armed) {
            g_del_armed = 1;
            break;
        }
        g_del_armed = 0;
        prof_reset(tx("JUGADOR", "PLAYER"));
        break;
    case R_CLOSE:
        close_menu();
        break;
    case R_QUIT:
        close_menu();
        plat_quit();
        break;
    default:
        if (has_value(kind))
            drop_open(kind);
    }
}

/* ---------------------------------------------------------------- tick */

static int edge(int k, int *was)
{
    int down = plat_key(k), hit = down && !*was;
    *was = down;
    return hit;
}

#define PAD_EDGE(f)  ((g_pad[0].f && !g_pad_was[0].f) || (g_pad[1].f && !g_pad_was[1].f))
#define PAD_HIT(b)   ((((g_pad[0].buttons & ~g_pad_was[0].buttons) \
                       | (g_pad[1].buttons & ~g_pad_was[1].buttons)) & (1u << (b))) != 0)

/* Rebinding: wait for every key (or the player's pad buttons) to be let go,
 * then take the first one pressed. The settings key cancels. */
static void capture(void)
{
    int row = g_rows[g_sel].kind == (g_capture == 1 ? R_KEY
                                     : g_capture == 2 ? R_PADB : R_PNAME);
    int esc = plat_key(PK_SETTINGS);

    if (g_capture == 3) {                /* typing the profile's name */
        char ch[8];
        struct profile *p = prof_get();

        while (p && plat_text_take(ch, sizeof ch) > 0) {
            unsigned char c = (unsigned char)ch[0];
            size_t n = strlen(g_edit);

            if (c == 13) {              /* Enter: keep it (empty: the default) */
                snprintf(p->name, sizeof p->name, "%s",
                         strspn(g_edit, " ") == n ? tx("JUGADOR", "PLAYER") : g_edit);
                prof_save();
                g_capture = 0;
                g_esc_was = 1;
                return;
            }
            if (c == 27) {              /* Esc: as it was */
                g_capture = 0;
                g_esc_was = 1;
                return;
            }
            if (c == 8) {               /* Backspace: one character */
                while (n > 0 && ((unsigned char)g_edit[n - 1] & 0xc0) == 0x80)
                    n--;
                if (n > 0)
                    n--;
                g_edit[n] = 0;
            } else if (c >= 32 && c != '|' && c != 127
                       && prof_name_len(g_edit) < PROF_NAME_MAX
                       && n + strlen(ch) < sizeof g_edit)
                strcat(g_edit, ch);
        }
        if (!p)
            g_capture = 0;
        return;
    }
    if (!row) {
        g_capture = 0;
        return;
    }
    if (g_capture == 1) {
        int k = plat_key_any();
        if (!g_armed) {
            g_armed = k == 0;
            return;
        }
        if (!k)
            return;
        if (k != plat_key_bound(PK_SETTINGS)) {
            plat_bind_key(g_rows[g_sel].arg, k);
            g_dirty = 1;
        }
        g_capture = 0;
        g_esc_was = 1;                  /* that Esc does not close the menu */
        return;
    }
    {
        const PLAT_PAD *p = &g_pad[g_player];
        int i;

        if (esc) {
            g_capture = 0;
            g_esc_was = 1;
            return;
        }
        if (!g_armed) {
            g_armed = p->buttons == 0;
            return;
        }
        for (i = 0; i < PB_COUNT; i++)
            if (p->buttons & (1u << i)) {
                int a = g_rows[g_sel].arg;
                *port_pad_action(g_player, a) = 1u << i;
                if (a == 6)             /* one special button for both */
                    *port_pad_action(g_player, 13) = 1u << i;
                g_dirty = 1;
                g_capture = 0;
                break;
            }
    }
}

static void turn_page(int d)
{
    if (g_framed) {                     /* the button setup: keys, pad */
        g_fsub ^= 1;
        g_sel = 0;
        g_first = 0;
        build();
        return;
    }
    g_page = wrap(g_page + d, N_PAGES);
    g_sel = 0;
    g_first = 0;
    build();
    while (g_sel < g_n - 1 && !g_rows[g_sel].en)
        g_sel++;
}

void set_menu_tick(void)
{
    static int was[10];
    int up, down, left, right, ok, prev, next, esc, click, back;

    build();
    if (g_sel >= g_n)
        g_sel = g_n - 1;
    while (g_sel < g_n - 1 && g_rows[g_sel].kind == R_HEAD)
        g_sel++;                        /* a heading is never the one chosen */
    if (g_capture) {
        capture();
        memcpy(g_pad_was, g_pad, sizeof g_pad_was);
        g_mouse_was = g_mdown;
        {                               /* swallow the edges meanwhile */
            int i, k[] = { PK_UP, PK_P2_UP, PK_DOWN, PK_P2_DOWN, PK_LEFT,
                           PK_P2_LEFT, PK_RIGHT, PK_P2_RIGHT, PK_OK };
            for (i = 0; i < 9; i++)
                was[i] = plat_key(k[i]);
        }
        build();
        return;
    }
    up    = edge(PK_UP, &was[0])    | edge(PK_P2_UP, &was[1])    | PAD_EDGE(up);
    down  = edge(PK_DOWN, &was[2])  | edge(PK_P2_DOWN, &was[3])  | PAD_EDGE(down);
    left  = edge(PK_LEFT, &was[4])  | edge(PK_P2_LEFT, &was[5])  | PAD_EDGE(left);
    right = edge(PK_RIGHT, &was[6]) | edge(PK_P2_RIGHT, &was[7]) | PAD_EDGE(right);
    ok    = edge(PK_OK, &was[8]) | PAD_HIT(PB_SOUTH);
    prev  = edge(PK_DBG_PAGE_PREV, &was[9]) | PAD_HIT(PB_L1);
    next  = PAD_HIT(PB_R1);
    {
        static int was_next;
        next |= edge(PK_DBG_PAGE_NEXT, &was_next);
    }
    esc   = edge(PK_SETTINGS, &g_esc_was);
    back  = PAD_HIT(PB_EAST);
    memcpy(g_pad_was, g_pad, sizeof g_pad_was);
    click = g_mdown && !g_mouse_was;
    g_mouse_was = g_mdown;

    if (g_drop) {
        drop_tick(up, down, left, right, ok, esc || back, click);
        if (g_dirty) {
            save_ini();
            g_dirty = 0;
        }
        build();
        return;
    }
    if (esc || back) {
        close_menu();
        return;
    }
    if (prev || next) {
        turn_page(next ? 1 : -1);
        build();
    }

    if (click && g_geo.srow > 0 && g_mx >= g_geo.sx && g_mx < g_geo.sx + g_geo.sw
        && g_my >= g_geo.stop && g_my < g_geo.stop + N_PAGES * g_geo.srow) {
        int t = (int)((g_my - g_geo.stop) / g_geo.srow);   /* a category */
        if (t != g_page) {
            turn_page(t - g_page);
            build();
        }
    } else if (click && g_geo.rowh > 0 && g_mx >= g_geo.bx && g_mx < g_geo.bx + g_geo.bw) {
        float mid = g_geo.bx + g_geo.bw * 0.5f;
        if (g_my >= g_geo.top) {
            int i = g_first + (int)((g_my - g_geo.top) / g_geo.rowh);
            if (i < g_n && i - g_first < VIS_ROWS && g_rows[i].en) {
                g_sel = i;
                ok = 1;                 /* a row with values opens its list */
            }
            (void)mid;
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
    if (left || right)
        change(g_rows[g_sel].kind, right ? 1 : -1);
    if (ok && g_rows[g_sel].en)
        pick(g_rows[g_sel].kind);
    if (g_dirty) {                      /* umk3.ini at once: a window */
        save_ini();                     /* closed now loses nothing */
        g_dirty = 0;
    }
    if (g_open)
        build();
}

void set_menu_draw(void)
{
    const char *tabs[N_PAGES], *lp[VIS_ROWS], *vp[VIS_ROWS];
    int en[VIS_ROWS], i, rows;
    char footer[384], k[96], ok[96], pa[96], pb[96], ta[96], tb[96];

    if (g_sel < g_first)
        g_first = g_sel;
    if (g_sel >= g_first + VIS_ROWS)
        g_first = g_sel - VIS_ROWS + 1;
    if (g_first > g_n - VIS_ROWS)
        g_first = g_n - VIS_ROWS > 0 ? g_n - VIS_ROWS : 0;
    rows = g_n - g_first < VIS_ROWS ? g_n - g_first : VIS_ROWS;
    tabs[PAGE_GFX] = tx("GRAFICOS", "GRAPHICS");
    tabs[PAGE_CTL] = tx("CONTROLES", "CONTROLS");
    tabs[PAGE_PROF] = tx("PERFIL", "PROFILE");
    if (g_first > 0 && g_first == g_sel && g_rows[g_sel - 1].kind == R_HEAD)
        g_first--;                      /* a row's heading stays in sight */
    for (i = 0; i < rows; i++) {
        lp[i] = g_rows[g_first + i].label;
        vp[i] = g_rows[g_first + i].value;
        en[i] = g_rows[g_first + i].en;
    }
    if (g_using && g_capture != 3) {  /* only the pad's pictures */
        int kind = pad_kind(g_using - 1);
        plat_pad_glyph(kind, PB_SOUTH, ok, sizeof ok);
        plat_pad_glyph(kind, PB_EAST, k, sizeof k);
        plat_pad_glyph(kind, PB_L1, ta, sizeof ta);
        plat_pad_glyph(kind, PB_R1, tb, sizeof tb);
    } else {                            /* only the keyboard's */
        plat_key_glyph(PK_SETTINGS, k, sizeof k);
        plat_key_glyph(PK_OK, ok, sizeof ok);
        plat_key_glyph(PK_DBG_PAGE_PREV, ta, sizeof ta);
        plat_key_glyph(PK_DBG_PAGE_NEXT, tb, sizeof tb);
    }
    (void)pa;
    (void)pb;
    if (g_capture == 3)
        snprintf(footer, sizeof footer, "%s %s   %s %s",
                 ok, tx("GUARDA", "KEEPS IT"), k, tx("CANCELA", "CANCELS"));
    else if (g_capture)
        snprintf(footer, sizeof footer, "%s %s", k, tx("CANCELA", "CANCELS"));
    else if (g_n > VIS_ROWS)
        snprintf(footer, sizeof footer, "%d-%d / %d   %s%s %s   %s %s   %s %s",
                 g_first + 1, g_first + rows, g_n, ta, tb, tx("CATEGORIA", "CATEGORY"),
                 ok, tx("ELIGE", "PICK"), k, tx("CIERRA", "CLOSES"));
    else
        snprintf(footer, sizeof footer, "%s%s %s   %s %s   %s %s",
                 ta, tb, tx("CATEGORIA", "CATEGORY"), ok, tx("ELIGE", "PICK"),
                 k, tx("CIERRA", "CLOSES"));
    {
        if (g_framed) {                 /* the game's own panel */
            float bx, top, bw, rh, km = 320.0f / (float)limeScreenHeight;
            char ff[384];

            char ft[64];

            if (g_capture)
                snprintf(ff, sizeof ff, "%s", footer);
            else
                snprintf(ff, sizeof ff, "%s%s %s   %s %s   %s %s", ta, tb,
                         g_fsub ? tx("TECLADO", "KEYBOARD") : tx("MANDO", "GAMEPAD"),
                         ok, tx("ELIGE", "PICK"), k, tx("VUELVE", "BACK"));
            snprintf(ft, sizeof ft, "%s - %s", tx("CONFIG. BOTONES", "BUTTON SETUP"),
                     g_fsub ? tx("MANDO", "GAMEPAD") : tx("TECLADO", "KEYBOARD"));
            PortControlsPanel(ft, lp, vp, en,
                              rows, g_sel - g_first, ff, &bx, &top, &bw, &rh);
            g_geo.bx = bx * km;          /* the mouse's rows, in menu units */
            g_geo.bw = bw * km;
            g_geo.top = top * km;
            g_geo.rowh = rh * km;
            g_geo.srow = 0;
            goto drop;
        }
        /* the categories down the side panel, and under them the player's
         * icon, as wide as the panel */
        ui_menu_show(tx("AJUSTES", "SETTINGS"), tabs, -N_PAGES, g_page, lp, vp,
                     en, rows, g_sel - g_first, footer, &g_geo);
        if (prof_get() && g_geo.srow > 0) {
            const struct profile *p = prof_get();
            int key = (int)prof_look_key(p);
            float size = g_geo.sw - 6.0f;
            float x = g_geo.sx + (g_geo.sw - size) * 0.5f - 2.0f;
            float y = g_geo.stop + N_PAGES * g_geo.srow + 6.0f;

            if (key != g_icon_key) {
                unsigned char *px = prof_icon_rgba(p->icon, p->color, p->dark, p->color2, 256);
                ui_icon_set(px, 256, 256);
                free(px);
                g_icon_key = key;
            }
            ui_icon_draw(x, y, size);
        }
    }
drop:
    if (g_drop) {                       /* the open list, over the menu */
        char df[384];

        snprintf(df, sizeof df, "%s %s   %s %s", ok, tx("ELIGE", "PICK"),
                 k, tx("VUELVE", "BACK"));
        if (drop_grid(g_drop_kind)) {
            const unsigned char *cells[MAX_DROP];
            const struct profile *p = prof_get();
            unsigned key = (unsigned)g_drop_kind * 2654435761u ^ (p ? prof_look_key(p) : 0);
            char t[96];

            for (i = 0; i < g_drop_n; i++)
                cells[i] = g_drop_cell[i];
            /* the title names the one under the cursor */
            snprintf(t, sizeof t, "%s: %s", g_drop_title, g_drop_lab[g_drop_sel]);
            for (i = 0; t[i]; i++)
                key = (key ^ (unsigned char)t[i]) * 16777619u;
            if (g_drop_kind == R_PICON)
                ui_grid_show(t, cells, g_drop_n, 8, 96, 34, g_drop_sel, df, key, &g_ggeo);
            else
                ui_grid_show(t, cells, g_drop_n, PAL_COLS, 64, 22, g_drop_sel, df,
                             key, &g_ggeo);
        } else {
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
                         g_drop_sel - g_drop_first, df, &g_dgeo);
            ui_menu_layer = 0;
        }
    }
}

/* The player's badge on the main menu (game_main.c calls it after the
 * frame): the profile's icon and name, bottom right. */
void set_menu_badge(void)
{
    static unsigned char *icon;
    static int icon_key = -1;
    const struct profile *p = prof_get();
    unsigned key = 2166136261u;
    const char *c;
    int k;

    if (!p)
        return;
    k = (int)prof_look_key(p);
    if (k != icon_key || !icon) {
        free(icon);
        icon = prof_icon_rgba(p->icon, p->color, p->dark, p->color2, 96);
        icon_key = k;
    }
    for (c = p->name; *c; c++)
        key = (key ^ (unsigned char)*c) * 16777619u;
    ui_badge(p->name, icon, 96, key ^ (unsigned)k);
}

/* ---------------------------------------------------------------- save
 *
 * umk3.ini rewritten in place: the launcher's lines this menu sets get the
 * new value, the rest stay as they were, and those missing go at the end.
 * A value of NULL removes the line (render_width= when the 3D resolution
 * follows the window, as the launcher leaves it out). */

#define MAX_KV 96
static struct { char k[24]; char v[48]; int del, done; } g_kv[MAX_KV];
static int g_nkv;

static void put(const char *k, const char *fmt, ...)
{
    va_list ap;

    if (g_nkv >= MAX_KV)
        return;
    snprintf(g_kv[g_nkv].k, sizeof g_kv[0].k, "%s", k);
    g_kv[g_nkv].del = fmt == NULL;
    g_kv[g_nkv].done = 0;
    g_kv[g_nkv].v[0] = 0;
    if (fmt) {
        va_start(ap, fmt);
        vsnprintf(g_kv[g_nkv].v, sizeof g_kv[0].v, fmt, ap);
        va_end(ap);
    }
    g_nkv++;
}

static void put_pad(const char *pre, int player)
{
    static const char *const n6[10] = {
        "hp", "lp", "block", "hk", "lk", "run", "special", "pause", "moves",
        "settings"
    };
    static const int a6[10] = { 0, 1, 2, 3, 4, 5, 6, 14, 15, 16 };
    static const char *const n5[4] = { "p", "b", "k", "r" };
    static const int a5[4] = { 7, 9, 10, 12 };
    char k[24], v[48];
    int i;

    snprintf(k, sizeof k, "%s", pre);
    put(k, "%s", port_pad_device(player));
    for (i = 0; i < 10; i++) {
        snprintf(k, sizeof k, "%s_%s", pre, n6[i]);
        mask_names(*port_pad_action(player, a6[i]), v, sizeof v);
        put(k, "%s", v);
    }
    for (i = 0; i < 4; i++) {
        snprintf(k, sizeof k, "%s5_%s", pre, n5[i]);
        mask_names(*port_pad_action(player, a5[i]), v, sizeof v);
        put(k, "%s", v);
    }
}

static void save_ini(void)
{
    static const char *const kn[10] = {
        "up", "down", "left", "right", "hp", "lp", "block", "hk", "lk", "run"
    };
    static const char *const n5[4] = { "p", "b", "k", "r" };
    static const char *const tq[3] = { "iphone", "ipad", "max" };
    const char *path = port_ini_path();
    struct port_gfx g;
    char k[24], *buf = NULL, *line, *next;
    long size = 0;
    FILE *f;
    int i;

    if (!path || !path[0])
        return;
    port_gfx_get(&g);
    g_nkv = 0;
    put("width", "%d", g.w);
    put("height", "%d", g.h);
    put("fullscreen", "%d", g.mode == 2);
    put("borderless", "%d", g.mode == 1);
    put("monitor", "%d", g.monitor);
    put("antialiasing", "%d", g.aa);
    if (g.rw > 0) {
        put("render_width", "%d", g.rw);
        put("render_height", "%d", g.rh);
    } else {
        put("render_width", NULL);
        put("render_height", NULL);
    }
    put("aspect", "%s", g.aspect);
    put("ui_size", "%s", g.ui ? "ipad" : "iphone");
    put("texture_quality", "%s", tq[g.texq]);
    put("hide_controls", "%d", g.hidectl);
    put("buttons", "%d", port_buttons());
    {
        static const char *const st[5] = { "auto", "xbox", "ps5", "nintendo", "ps4" };
        put("pad_icons", "%s", st[plat_pad_glyph_style(0, -1)]);
        put("p2pad_icons", "%s", st[plat_pad_glyph_style(1, -1)]);
    }
    for (i = 0; i < 10; i++) {
        snprintf(k, sizeof k, "key_%s", kn[i]);
        put(k, "%d", plat_key_bound(PK_UP + i));
        snprintf(k, sizeof k, "p2key_%s", kn[i]);
        put(k, "%d", plat_key_bound(PK_P2_UP + i));
    }
    put("key_special", "%d", plat_key_bound(PK_SPECIAL));
    put("key_pause", "%d", plat_key_bound(PK_PAUSE));
    put("key_moves", "%d", plat_key_bound(PK_MOVES));
    put("p2key_special", "%d", plat_key_bound(PK_P2_SPECIAL));
    put("p2key_pause", "%d", plat_key_bound(PK_P2_PAUSE));
    put("p2key_moves", "%d", plat_key_bound(PK_P2_MOVES));
    put("p2key_ok", "%d", plat_key_bound(PK_P2_OK));
    for (i = 0; i < 4; i++) {
        snprintf(k, sizeof k, "key5_%s", n5[i]);
        put(k, "%d", plat_key_bound(PK_5_P + i));
        snprintf(k, sizeof k, "p2key5_%s", n5[i]);
        put(k, "%d", plat_key_bound(PK_P2_5_P + i));
    }
    put_pad("pad", 0);
    put_pad("p2pad", 1);

    f = fopen(path, "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        size = ftell(f);
        fseek(f, 0, SEEK_SET);
        buf = malloc((size_t)size + 1);
        if (buf && fread(buf, 1, (size_t)size, f) != (size_t)size)
            size = 0;
        fclose(f);
    }
    if (!buf)
        size = 0;
    f = fopen(path, "wb");
    if (!f) {
        printf("settings menu: cannot write %s\n", path);
        free(buf);
        return;
    }
    if (buf) {
        buf[size] = 0;
        for (line = buf; line && *line; line = next) {
            char *eq, *end;
            size_t n;

            next = strchr(line, '\n');
            if (next)
                *next++ = 0;
            end = line + strlen(line);
            if (end > line && end[-1] == '\r')
                *--end = 0;
            if (!*line)
                continue;
            eq = strchr(line, '=');
            n = eq ? (size_t)(eq - line) : 0;
            for (i = 0; i < g_nkv; i++)
                if (n && strlen(g_kv[i].k) == n && strncmp(line, g_kv[i].k, n) == 0)
                    break;
            if (i == g_nkv)
                fprintf(f, "%s\n", line);
            else if (!g_kv[i].del && !g_kv[i].done) {
                fprintf(f, "%s=%s\n", g_kv[i].k, g_kv[i].v);
                g_kv[i].done = 1;
            }
        }
    }
    for (i = 0; i < g_nkv; i++)
        if (!g_kv[i].del && !g_kv[i].done)
            fprintf(f, "%s=%s\n", g_kv[i].k, g_kv[i].v);
    fclose(f);
    free(buf);
    printf("settings menu: saved %s\n", path);
}
