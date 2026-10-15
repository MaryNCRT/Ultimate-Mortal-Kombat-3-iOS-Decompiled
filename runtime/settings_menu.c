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
 *     Every tab ends in RESET (on KEYBOARD and GAMEPAD), CLOSE and
 *     QUIT GAME.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform/platform.h"
#include "debug_menu.h"
#include "settings_menu.h"

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

enum { PAGE_GFX, PAGE_KEYS, PAGE_PAD, N_PAGES };

enum {
    R_DISPLAY, R_RES, R_APPLY, R_RRES, R_AA, R_TEXQ, R_ASPECT, R_UI, R_HIDE,
    R_PLAYER, R_LAYOUT, R_KEY, R_KRESET, R_DEVICE, R_PADB, R_PRESET,
    R_CLOSE, R_QUIT
};

#define MAX_ROWS 24
#define VIS_ROWS 13
static struct {
    int  kind, arg;
    char label[40], value[64];
    int  en;
} g_rows[MAX_ROWS];
static int g_n;

static int g_open, g_want, g_page, g_sel, g_first, g_player, g_dirty;
static struct port_gfx g_gfx;           /* the menu's */
static int g_disp_mode, g_disp_w, g_disp_h;   /* the window as applied */
static int g_capture;                   /* 1 a key, 2 a pad button */
static int g_armed;                     /* nothing held since it began */
static struct ui_geo g_geo;
static float g_mx, g_my;
static int   g_mdown, g_mouse_was;
static PLAT_PAD g_pad[2], g_pad_was[2];
static int   g_esc_was;

int set_menu_is_open(void) { return g_open; }

/* The keys as game_main.c reads them: a scripted test's (UMK3_KEYS) too. */
int (*set_menu_key)(int code) = plat_key;
#define plat_key(code) set_menu_key(code)

static void save_ini(void);

static void close_menu(void)
{
    g_open = 0;
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
    else
        g_want = 1;                     /* opened after the next frame */
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

void set_menu_pads(const PLAT_PAD *p1, const PLAT_PAD *p2)
{
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
    }
}

/* ----------------------------------------------------------------- pad */

#define B(x) (1u << PB_##x)
static const unsigned k_pad_def[16] = {
    B(WEST), B(SOUTH), B(R1), B(NORTH), B(EAST), B(L1) | B(L2), B(R2),
    B(WEST) | B(NORTH), 0, B(R1), B(SOUTH) | B(EAST), 0, B(L1) | B(L2), B(R2),
    B(START), B(SELECT),
};

static void reset_pad(int player)
{
    int i;

    for (i = 0; i < 16; i++)
        *port_pad_action(player, i) = k_pad_def[i];
}

static void mask_text(unsigned m, char *out, size_t n)
{
    size_t k = 0;
    int i;

    out[0] = 0;
    for (i = 0; i < PB_COUNT; i++)
        if (m & (1u << i))
            k += (size_t)snprintf(out + k, n > k ? n - k : 0, "%s%s",
                                  k ? "+" : "", k_pb[i]);
}

/* The pad rows: the action's index for port_pad_action, with the labels. */
static const struct kdef k_pad6[9] = {
    { 0, "PUNO ALTO", "HIGH PUNCH" }, { 1, "PUNO BAJO", "LOW PUNCH" },
    { 2, "BLOQUEO", "BLOCK" }, { 3, "PATADA ALTA", "HIGH KICK" },
    { 4, "PATADA BAJA", "LOW KICK" }, { 5, "CORRER", "RUN" },
    { 6, "ESPECIAL", "SPECIAL" }, { 14, "PAUSA", "PAUSE" },
    { 15, "LISTA DE MOVIMIENTOS", "MOVES LIST" },
};
static const struct kdef k_pad5[7] = {
    { 7, "PUNO", "PUNCH" }, { 9, "BLOQUEO", "BLOCK" },
    { 10, "PATADA", "KICK" }, { 12, "CORRER", "RUN" },
    { 6, "ESPECIAL", "SPECIAL" }, { 14, "PAUSA", "PAUSE" },
    { 15, "LISTA DE MOVIMIENTOS", "MOVES LIST" },
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
    char k[32];
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
        plat_key_label(code, k, sizeof k);
    if (d)
        add(R_KEY, code, tx(d->es, d->en), "%s", k);
    else if (code == PK_SPECIAL || code == PK_P2_SPECIAL)
        add(R_KEY, code, tx("ESPECIAL", "SPECIAL"), "%s", k);
    else if (code == PK_PAUSE)
        add(R_KEY, code, tx("PAUSA", "PAUSE"), "%s", k);
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

        add(R_DISPLAY, 0, tx("PANTALLA", "DISPLAY"), "< %s >",
            tx(mode_es[g_gfx.mode], mode_en[g_gfx.mode]));
        add(R_RES, 0, tx("RESOLUCION", "RESOLUTION"), "< %dx%d >", g_gfx.w, g_gfx.h);
        if (g_gfx.mode == 1)
            g_rows[g_n - 1].en = 0;     /* borderless is the monitor's size */
        add(R_APPLY, 0, tx("APLICAR PANTALLA", "APPLY DISPLAY"), "%s",
            g_gfx.mode != g_disp_mode
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
        add(R_TEXQ, 0, tx("TEXTURAS", "TEXTURES"), "< %s >",
            tx(tq_es[g_gfx.texq], tq_en[g_gfx.texq]));
        add(R_ASPECT, 0, tx("RELACION DE ASPECTO", "ASPECT RATIO"), "< %s >",
            g_gfx.aspect);
        add(R_UI, 0, tx("TAMANO DE LA INTERFAZ", "INTERFACE SIZE"), "< %s >",
            g_gfx.ui ? "IPAD" : "IPHONE");
        add(R_HIDE, 0, tx("CONTROLES EN PANTALLA", "ON-SCREEN CONTROLS"), "< %s >",
            g_gfx.hidectl ? tx("OCULTOS", "HIDDEN") : tx("VISIBLES", "SHOWN"));
        break;
    }
    case PAGE_KEYS: {
        int five = port_buttons() == 5;

        add(R_PLAYER, 0, tx("JUGADOR", "PLAYER"), "%s", player_text());
        add(R_LAYOUT, 0, tx("BOTONES", "LAYOUT"), "< %d %s >", five ? 5 : 6,
            tx("BOTONES", "BUTTONS"));
        for (i = 0; i < 4; i++)
            key_row(k_dirs[i].code);
        if (five)
            for (i = 0; i < 4; i++)
                key_row(k_five[i].code);
        else
            for (i = 0; i < 6; i++)
                key_row(k_six[i].code);
        key_row(PK_SPECIAL);
        if (!g_player) {
            key_row(PK_PAUSE);
            key_row(PK_MOVES);
        }
        add(R_KRESET, 0, tx("RESTABLECER TECLAS", "RESET KEYS"), "");
        break;
    }
    case PAGE_PAD: {
        const struct kdef *d = port_buttons() == 5 ? k_pad5 : k_pad6;
        int nd = port_buttons() == 5 ? 7 : 9;
        const char *dev = port_pad_device(g_player);
        const PLAT_PAD *p = &g_pad[g_player];

        add(R_PLAYER, 0, tx("JUGADOR", "PLAYER"), "%s", player_text());
        snprintf(v, sizeof v, "%s", dev);
        for (i = 0; v[i]; i++)
            if (v[i] >= 'a' && v[i] <= 'z')
                v[i] = (char)(v[i] - 32);
        if (strcmp(dev, "off") == 0)
            add(R_DEVICE, 0, tx("MANDO", "GAMEPAD"), "< %s >", tx("NINGUNO", "NONE"));
        else
            add(R_DEVICE, 0, tx("MANDO", "GAMEPAD"), "< %s > %.24s", v,
                p->connected ? p->name : tx("(NO CONECTADO)", "(NOT CONNECTED)"));
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
    }
    add(R_CLOSE, 0, tx("VOLVER AL JUEGO", "BACK TO THE GAME"), "");
    add(R_QUIT, 0, tx("SALIR DEL JUEGO", "QUIT THE GAME"), "");
}

static int has_value(int kind)
{
    switch (kind) {
    case R_DISPLAY: case R_RES: case R_RRES: case R_AA: case R_TEXQ:
    case R_ASPECT: case R_UI: case R_HIDE: case R_PLAYER: case R_LAYOUT:
    case R_DEVICE:
        return 1;
    }
    return 0;
}

static int wrap(int v, int n) { return (v % n + n) % n; }

static void change(int kind, int d)
{
    int i;

    switch (kind) {
    case R_DISPLAY:
        g_gfx.mode = wrap(g_gfx.mode + d, 3);
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

static void pick(int kind)
{
    switch (kind) {
    case R_APPLY:
        port_gfx_set(&g_gfx, 1);
        port_gfx_get(&g_gfx);
        g_disp_mode = g_gfx.mode;
        g_disp_w = g_gfx.w;
        g_disp_h = g_gfx.h;
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
    case R_CLOSE:
        close_menu();
        break;
    case R_QUIT:
        close_menu();
        plat_quit();
        break;
    default:
        if (has_value(kind))
            change(kind, 1);
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
    int row = g_rows[g_sel].kind == (g_capture == 1 ? R_KEY : R_PADB);
    int esc = plat_key(PK_SETTINGS);

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
    g_page = wrap(g_page + d, N_PAGES);
    g_sel = 0;
    g_first = 0;
}

void set_menu_tick(void)
{
    static int was[10];
    int up, down, left, right, ok, prev, next, esc, click, back;

    build();
    if (g_sel >= g_n)
        g_sel = g_n - 1;
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

    if (esc || back) {
        close_menu();
        return;
    }
    if (prev || next) {
        turn_page(next ? 1 : -1);
        build();
    }

    if (click && g_geo.rowh > 0 && g_mx >= g_geo.bx && g_mx < g_geo.bx + g_geo.bw) {
        float mid = g_geo.bx + g_geo.bw * 0.5f;
        if (g_my >= g_geo.title0 && g_my < g_geo.top) {
            int t = (int)((g_mx - g_geo.bx) * N_PAGES / g_geo.bw);
            if (t >= 0 && t < N_PAGES && t != g_page) {
                turn_page(t - g_page);
                build();
            }
        } else if (g_my >= g_geo.top) {
            int i = g_first + (int)((g_my - g_geo.top) / g_geo.rowh);
            if (i < g_n && i - g_first < VIS_ROWS && g_rows[i].en) {
                g_sel = i;
                if (has_value(g_rows[i].kind))
                    change(g_rows[i].kind, g_mx < mid ? -1 : 1);
                else
                    ok = 1;
            }
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
    char footer[128], k[24];

    if (g_sel < g_first)
        g_first = g_sel;
    if (g_sel >= g_first + VIS_ROWS)
        g_first = g_sel - VIS_ROWS + 1;
    if (g_first > g_n - VIS_ROWS)
        g_first = g_n - VIS_ROWS > 0 ? g_n - VIS_ROWS : 0;
    rows = g_n - g_first < VIS_ROWS ? g_n - g_first : VIS_ROWS;
    tabs[PAGE_GFX] = tx("GRAFICOS", "GRAPHICS");
    tabs[PAGE_KEYS] = tx("TECLADO", "KEYBOARD");
    tabs[PAGE_PAD] = tx("MANDO", "GAMEPAD");
    for (i = 0; i < rows; i++) {
        lp[i] = g_rows[g_first + i].label;
        vp[i] = g_rows[g_first + i].value;
        en[i] = g_rows[g_first + i].en;
    }
    plat_key_label(PK_SETTINGS, k, sizeof k);
    if (g_capture)
        snprintf(footer, sizeof footer, "%s",
                 tx("ESC CANCELA", "ESC CANCELS"));
    else if (g_n > VIS_ROWS)
        snprintf(footer, sizeof footer, "%d-%d / %d   %s %s",
                 g_first + 1, g_first + rows, g_n, k, tx("CIERRA", "CLOSES"));
    else
        snprintf(footer, sizeof footer, "%s %s   %s",
                 tx("FLECHAS O RATON", "ARROWS OR MOUSE"), k,
                 tx("CIERRA", "CLOSES"));
    ui_menu_show(tx("AJUSTES", "SETTINGS"), tabs, N_PAGES, g_page, lp, vp,
                 en, rows, g_sel - g_first, footer, &g_geo);
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
    static const char *const n6[9] = {
        "hp", "lp", "block", "hk", "lk", "run", "special", "pause", "moves"
    };
    static const int a6[9] = { 0, 1, 2, 3, 4, 5, 6, 14, 15 };
    static const char *const n5[4] = { "p", "b", "k", "r" };
    static const int a5[4] = { 7, 9, 10, 12 };
    char k[24], v[48];
    int i;

    snprintf(k, sizeof k, "%s", pre);
    put(k, "%s", port_pad_device(player));
    for (i = 0; i < 9; i++) {
        snprintf(k, sizeof k, "%s_%s", pre, n6[i]);
        mask_text(*port_pad_action(player, a6[i]), v, sizeof v);
        put(k, "%s", v);
    }
    for (i = 0; i < 4; i++) {
        snprintf(k, sizeof k, "%s5_%s", pre, n5[i]);
        mask_text(*port_pad_action(player, a5[i]), v, sizeof v);
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
