/*
 * game_main.c -- the whole game, booted the way the device boots it.
 *
 *   umk3-game.exe <path to UMK3.app/res>
 *
 * runtime/menu_main.c drives the front end by hand: it calls the loader and
 * then Task_FEMain, and nothing past the menu can happen. This one runs the
 * game's own frame instead, so the menu, the loading screen and the fight
 * follow each other the way the binary sequences them. The order is read off
 * the binary:
 *
 *   -[UMK3AppDelegate startAppWithOptions:application:]   0x00064ae8
 *       limeGetLanguage(Language, 10)
 *       sprintf(buf, "TOS_URL_%s", Language)
 *       if (!limeGetPropertyString(buf)) Language = "EN"
 *       Load_SettingsData(); EASDK_Init(); ...
 *
 *   -[EAGLView createFramebuffer]                          0x0006190c
 *       GameCodeInit()     -- limeInit, the event manager, the debug window
 *
 *   -[EAGLView drawView], once per display-link tick       0x00061668
 *       glViewport; glClearColor(0, 0, 0, 1); glClear(colour | depth)
 *       GameCodeMain()     -- limeBegin, TaskFunctionList[CurrentTask](),
 *                             limeFinish, heartbeatUpdate
 *
 * CurrentTask starts at 0, Task_LoadSplashScreen, as it does in __common.
 *
 * The fight logic stores addresses in 32-bit words, so this program is built
 * for i686, with its data tables generated from the user's binary by
 * tools/logic_tables.py and verified by tools/check_logic_tables.py (see the
 * umk3-game target in CMakeLists.txt).
 *
 * Environment, for testing:
 *   UMK3_SHOT=<n>     tick n times, write umk3-game.ppm, quit
 *   UMK3_TAPS=<list>  scripted taps, "tick:x,y;tick:x,y;..." in game
 *                     coordinates (480x320), each a press held for 3 ticks;
 *                     the mouse is ignored while a script runs
 *   UMK3_SKIP_INTRO=1 no publisher logos (umk3.ini skip_intro=1)
 *   UMK3_BUTTONS=5|6  the button layout, over umk3.ini's buttons=
 *   UMK3_LOG_TASKS=1  print every change of CurrentTask and FE_CurrentTask
 *   UMK3_SCREEN=<n|name>  open that front-end screen once the menu is up
 *   UMK3_DBG_OPEN=<n> open the debug menu at tick n (with debug_keys)
 *   UMK3_DBG_KEY=<tick:k;...>  press F9+k at that tick (with debug_keys)
 *   UMK3_CRASH_TEST=<tick>     crash on purpose (the crash report)
 *   UMK3_LOG_P1=1              player one frame and buttons, every change
 *   UMK3_LOG_PACE=1            the frame pacing (input lag), once a second
 *   UMK3_LOG_PAD=1             the pad's buttons, every change (pad= in umk3.ini)
 *   UMK3_TEXQ=0|1|2          texture quality iphone / ipad / max (texture_quality=)
 *   UMK3_PAD=<auto|off|xinput|vvvv:pppp>  the pad, over umk3.ini's pad=
 *   UMK3_FRAME_DELAY=0         no late key read before a 60 Hz refresh
 *   UMK3_PACE_HZ=<hz>          pace as if the screen ran at <hz>
 *   UMK3_DBG_HITBOX=1          the hitboxes on (as the debug menu's TOOLS)
 *   UMK3_DBG_SPEED=<percent>   the fight's speed (TOOLS: FIGHT SPEED)
 *   UMK3_DBG_STATE="t:s1;t:l1" save / load a state slot by script
 *   UMK3_SHOTS=<t,t,...>       write umk3-shot-<t>.ppm at each tick, go on
 *   UMK3_ARCADE=<destiny,stage> with --fight: that rung of an Arcade ladder
 *   UMK3_SCREENS=<tick:n;...>  open front-end screen n at that tick, so one
 *                              session can walk the whole menu
 *
 * Keyboard, player 1, during a fight (runtime/platform's defaults):
 *   W A S D or the arrows   the joystick
 *   U  high punch   I  low punch   O  block
 *   J  high kick    K  low kick    L  run
 *   H  special (the S button of the five-button layout)
 *   P  pause menu   M  moves list   (Esc no longer quits)
 * umk3.ini key_up= ... key_moves= rebinds them (virtual-key codes).
 * The keys go straight into ReadControls' button and dial state (see
 * keyboard_buttons), so they do not need the on-screen controls.
 *
 * Debug, to skip the menus:
 *   umk3-game --fight <p1> <p2> [stage]     (or UMK3_FIGHT="p1,p2,stage")
 *   umk3-game --list-pads                   the pads attached (for the launcher)
 * goes straight from the main menu into a fight. A fighter is a name from
 * CharacterNames ("kitana", "kunglao", "sub-zero") or its number 0..25; the
 * stage is a Level_Info row, 0..15 (default 0). It sets what the select
 * screen would -- PLAYER1MODEL, PLAYER2MODEL, Character1/2, LevelSelect --
 * and hands the front end to Task_FEDestroy the way the tower does.
 *
 * A game started by double-click (stdout not redirected) writes everything it
 * prints -- task changes, loading steps, a crash's addresses -- to
 * logs/umk3-<date>-<time>.log beside the exe, one file per session. A log is
 * deleted once the error it shows has been found and fixed.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform/platform.h"
#include "platform/gl.h"
#include "debug_menu.h"

/* A scripted run (UMK3_SHOT) ignores the real keyboard, as it ignores the
 * mouse: a test window takes the focus when it opens, and whoever is typing
 * in another window would otherwise be pressing keys in the test. */
static int g_keys_off;
static long g_tick;                     /* the tick count, for UMK3_KEYS */

/* UMK3_KEYS="tick:keys:hold;..." -- player one's keys pressed by script, so a
 * test can type a move: keys joined with '+' from up down left right hp lp
 * bl hk lk run s (the special button), or p b k r for five buttons; held
 * for `hold` ticks (default 4).
 * Example, Scorpion's spear: "1000:left:4;1005:left:4;1010:lp:4". */
static int scripted_key(int code)
{
    static const struct { const char *n; int code; } k_names[] = {
        { "up", PK_UP }, { "down", PK_DOWN }, { "left", PK_LEFT },
        { "right", PK_RIGHT }, { "hp", PK_HP }, { "lp", PK_LP },
        { "bl", PK_BL }, { "hk", PK_HK }, { "lk", PK_LK },
        { "run", PK_RUN }, { "s", PK_SPECIAL },
        { "p", PK_5_P }, { "b", PK_5_B }, { "k", PK_5_K }, { "r", PK_5_R },
    };
    const char *q = getenv("UMK3_KEYS");

    while (q && *q) {
        long t = atol(q);
        const char *a = strchr(q, ':'), *b, *end = strchr(q, ';');
        long hold = 4;

        if (!a || (end && a > end))
            break;
        a++;
        b = strchr(a, ':');
        if (b && (!end || b < end))
            hold = atol(b + 1);
        else
            b = end ? end : a + strlen(a);
        if (g_tick >= t && g_tick < t + hold) {
            const char *p = a;
            while (p < b) {
                size_t i, n = strcspn(p, "+:;");
                for (i = 0; i < sizeof k_names / sizeof k_names[0]; i++)
                    if (strlen(k_names[i].n) == n
                        && strncmp(p, k_names[i].n, n) == 0
                        && k_names[i].code == code)
                        return 1;
                p += n;
                if (*p == '+')
                    p++;
                else
                    break;
            }
        }
        q = end ? end + 1 : NULL;
    }
    return 0;
}
#define plat_key(code) (g_keys_off ? scripted_key(code) \
                        : ((plat_key)(code) || scripted_key(code)))

#define VIRT_W 480
#define VIRT_H 320
#define SCALE    2

/* The surface the game lays itself out on. Port, not in the binary: the
 * launcher's UI size (umk3.ini ui_size=) picks it. IPHONE is the retail
 * 480x320. IPAD is 960x640, the same 3:2 at twice the size: SetupFEScale
 * then gives FE_WidthScale 2 and HUD_Scale 1.75 -- the binary's own path for
 * a big screen -- so the menus fill the window as before and the fight's
 * buttons and bars come out at 1.75/2 of their iPhone size. Touches,
 * scripted taps and the pad's HUD taps are given in 480x320 and scaled by
 * g_ui_k. */
static int   g_virt_w = VIRT_W, g_virt_h = VIRT_H;
static float g_ui_k = 1.0f;
/* aspect= (launcher): the surface's shape, w/h. 3:2 is the device's. */
static float g_aspect = 1.5f;

/* The front end lays itself out by its width and, on a surface wider than
 * 3:2, would crop its own top and bottom (SetupFEScale's FE_YOffset goes
 * negative). So from 16:9 up the menus keep 3:2 -- centred, the bars black
 * or the marco= picture -- and only the fight (CurrentTask 6) gets the wide
 * surface: its HUD is anchored to limeScreenWidth's edges at the menus'
 * FE and HUD scales, and LIMEDS_Set3dMode widens the view. 4:3 is one
 * surface for both, as the iPad build has it. Port, not in the binary. */
static int g_menu_w = VIRT_W, g_menu_h = VIRT_H;
static int g_fight_w = VIRT_W, g_fight_h = VIRT_H;
static int g_surface = -1;              /* 0 menus, 1 fight */

void lime_gl_set_screen(int w, int h);
void lime_menu_set_screen(int w, int h);

static void use_surface(int fight)
{
    if (fight == g_surface)
        return;
    g_surface = fight;
    g_virt_w = fight ? g_fight_w : g_menu_w;
    g_virt_h = fight ? g_fight_h : g_menu_h;
    lime_menu_set_screen(g_virt_w, g_virt_h);
    lime_gl_set_screen(g_virt_w, g_virt_h);
}

static float parse_aspect(const char *v)
{
    float a = 0.0f, b = 0.0f;

    if (sscanf(v, "%f:%f", &a, &b) == 2 && a > 0.0f && b > 0.0f && a / b >= 1.0f && a / b <= 4.0f)
        return a / b;
    return 1.5f;
}

void  lime_platform_set_asset_root(const char *path);
void  lime_gl_set_screen(int w, int h);
void  lime_menu_set_screen(int w, int h);

void  limeGetLanguage(char *dst, int len);
const char *limeGetPropertyString(const char *key);
void  Load_SettingsData(void);
void  GameCodeInit(void);
void  fight_runtime_init(void);
void  GameCodeMain(void);

void  lime_menu_advance_clock(double seconds);
void  lime_touch_began(float x, float y);
void  lime_touch_moved(float x, float y, float prev_x, float prev_y);
void  lime_touch_ended(float x, float y, float prev_x, float prev_y);
void  lime_app_resign_active(void);
void  lime_app_become_active(void);

extern char Language[10];
extern int  CurrentTask;
extern long SplashCount;                /* Task_LoadSplashScreen's frame */
extern float StaticMeshAmbient[3];      /* CreateFadedRGBS's offset */
extern int  FE_CurrentTask;
extern long PLAYER1MODEL, PLAYER2MODEL, Character1, Character2, LevelSelect,
            Character2Override;
extern const char *CharacterNames[26];

extern float limeTouchScreenX[10], limeTouchScreenY[10];
extern void *G;                         /* GAMESTATE *, the fight engine's state */
extern long  Player1NumButtons;        /* 5 or 6, Settings[4] */
extern int   Settings[10];

/* Keyboard -> player 1, straight into the engine's button state: no
 * synthetic touches, so it does not depend on where the on-screen controls
 * are, or on them being shown (hide_controls=1). ReadControls (GameCode.c)
 * ORs PortKeyButtons into ButtonStates after its touch scan and takes
 * PortKeyDial as JoystickState, every tick, so a held key is a held button
 * -- what the hold-and-release moves need. The mouse is still a finger.
 *
 * Button indices: six buttons HP LP BL HK LK RN = 0 1 2 3 4 5, five buttons
 * (DEFAULT_CustomButtonsPos5) P = 0, B = 2, K = 3, R = 5, and the special
 * button S = 6. Each layout has its own keys: key_hp .. key_run for six,
 * key5_p/b/k/r and key_special for five. */
extern int PortKeyButtons[7], PortKeyDial;

static PLAT_PAD g_pad;                  /* player one's pad, read each tick */

/* The pad combination that opens the debug menu: umk3.ini pad_dbg_menu=,
 * button names joined by "+" (SOUTH EAST WEST NORTH L1 R1 L2 R2 SELECT START
 * L3 R3 HOME). L3+R3 unless it says otherwise; empty = none. */
static unsigned g_pad_dbg_mask = (1u << PB_L3) | (1u << PB_R3);

static unsigned pad_combo(const char *v)
{
    static const char *const names[PB_COUNT] = {
        "SOUTH", "EAST", "WEST", "NORTH", "L1", "R1", "L2", "R2",
        "SELECT", "START", "L3", "R3", "HOME",
    };
    unsigned m = 0;
    int i;

    while (*v) {
        size_t n = strcspn(v, "+");
        for (i = 0; i < PB_COUNT; i++)
            if (strlen(names[i]) == n && _strnicmp(v, names[i], n) == 0)
                m |= 1u << i;
        v += n;
        if (*v)
            v++;
    }
    return m;
}

static void keyboard_buttons(void)
{
    static const int keys6[7][2] = {    /* button index 0..6, two keys each */
        { PK_HP, PK_P2_HP }, { PK_LP, PK_P2_LP }, { PK_BL, PK_P2_BL },
        { PK_HK, PK_P2_HK }, { PK_LK, PK_P2_LK }, { PK_RUN, PK_P2_RUN },
        { PK_SPECIAL, PK_P2_SPECIAL },
    };
    static const int keys5[7][2] = {    /* P - B K - R S; -1: no button */
        { PK_5_P, PK_P2_HP }, { -1, -1 }, { PK_5_B, PK_P2_BL },
        { PK_5_K, PK_P2_HK }, { -1, -1 }, { PK_5_R, PK_P2_RUN },
        { PK_SPECIAL, PK_P2_SPECIAL },
    };
    /* (dx, dy) -> CheckLeftDial + 1: 1 up, 2 up-right ... 8 up-left */
    static const int dial[3][3] = {     /* [dy + 1][dx + 1] */
        { 8, 1, 2 }, { 7, 0, 3 }, { 6, 5, 4 },
    };
    /* The pad (plat_pad_read, umk3.ini pad=), by position: the face
     * buttons as the arcade's -- punches on the left, kicks on the right,
     * high on top -- R1 block, L1 / L2 run, R2 the special button. With five
     * buttons both left ones punch and both right ones kick. */
    static const unsigned pad6[7] = {
        1u << PB_WEST, 1u << PB_SOUTH, 1u << PB_R1, 1u << PB_NORTH,
        1u << PB_EAST, (1u << PB_L1) | (1u << PB_L2), 1u << PB_R2,
    };
    static const unsigned pad5[7] = {
        (1u << PB_WEST) | (1u << PB_NORTH), 0, 1u << PB_R1,
        (1u << PB_SOUTH) | (1u << PB_EAST), 0,
        (1u << PB_L1) | (1u << PB_L2), 1u << PB_R2,
    };
    const int (*keys)[2] = (Player1NumButtons == 6) ? keys6 : keys5;
    const unsigned *padb = (Player1NumButtons == 6) ? pad6 : pad5;
    int dx, dy, b, live = CurrentTask == 6;

    dx = (plat_key(PK_RIGHT) || plat_key(PK_P2_RIGHT) || g_pad.right)
       - (plat_key(PK_LEFT) || plat_key(PK_P2_LEFT) || g_pad.left);
    dy = (plat_key(PK_DOWN) || plat_key(PK_P2_DOWN) || g_pad.down)
       - (plat_key(PK_UP) || plat_key(PK_P2_UP) || g_pad.up);
    PortKeyDial = live ? dial[dy + 1][dx + 1] : 0;

    for (b = 0; b < 7; b++)
        PortKeyButtons[b] = live && keys[b][0] >= 0
            && (plat_key(keys[b][0]) || plat_key(keys[b][1])
                || (g_pad.buttons & padb[b]));
}

/* --fight: -1 until parsed. */
static long g_fight_p1 = -1, g_fight_p2 = -1, g_fight_stage = 0;
static int  g_fight_override;           /* Character2Override is ours */
extern int  DbgPalette[2], DbgFightActive;      /* runtime/debug_menu.c */

/* A debug fight is a visit: what the player had chosen -- fighters, stage,
 * mode and the Arcade rung -- is saved when it starts and put back once the
 * game is in the front end again, so neither its fighters nor the ladder
 * step its win or loss took stay for any mode after it. A port feature, not
 * in the binary. The boss jump and UMK3_ARCADE climb a ladder on purpose
 * and keep what the fight did. */
static struct {
    int  on, keep, fought;
    long c1, c2, m1, m2, level, mode, destiny;
    int  stage;
} g_dbg_saved;

/* A fighter by number or by name, ignoring case, spaces and hyphens. */
static long fighter_id(const char *s)
{
    long i;
    char *end;

    i = strtol(s, &end, 10);
    if (*s && *end == 0)
        return (i >= 0 && i < 26) ? i : -1;
    for (i = 0; i < 26; i++) {
        const char *a = CharacterNames[i], *b = s;
        for (;;) {
            while (*a == ' ' || *a == '-') a++;
            while (*b == ' ' || *b == '-' || *b == '_') b++;
            if (!*a || !*b || (*a | 0x20) != (*b | 0x20))
                break;
            a++, b++;
        }
        if (!*a && !*b)
            return i;
    }
    return -1;
}

static void parse_fight(const char *p1, const char *p2, const char *stage)
{
    g_fight_p1 = fighter_id(p1);
    g_fight_p2 = fighter_id(p2);
    g_fight_stage = stage ? atol(stage) : 0;
    if (g_fight_p1 < 0 || g_fight_p2 < 0 || g_fight_stage < 0
        || g_fight_stage > 15) {
        fprintf(stderr, "--fight: unknown fighter or stage (%s %s %s)\n",
                p1, p2, stage ? stage : "0");
        g_fight_p1 = g_fight_p2 = -1;
    }
}

static void save_shot_as(const char *name, int w, int h);

static void save_shot(int w, int h)
{
    save_shot_as("umk3-game.ppm", w, h);
}

static void save_shot_as(const char *name, int w, int h)
{
    unsigned char *px = (unsigned char *)malloc((size_t)w * h * 3);
    FILE *f;
    int y;

    if (px == NULL)
        return;
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, px);
    f = fopen(name, "wb");
    if (f) {
        fprintf(f, "P6\n%d %d\n255\n", w, h);
        for (y = h - 1; y >= 0; y--)
            fwrite(px + (size_t)y * w * 3, 1, (size_t)w * 3, f);
        fclose(f);
        printf("wrote %s (%dx%d)\n", name, w, h);
    }
    free(px);
}

static int  g_cfg_buttons;            /* buttons=5|6: the launcher's layout */

/* -[UMK3AppDelegate startAppWithOptions:application:], the part that is the
 * game's: the language, with English when the bundle has no terms-of-service
 * URL for it, and the settings. The rest is UIKit and the EA SDK. */
static void start_app(void)
{
    char key[30];

    limeGetLanguage(Language, 10);
    snprintf(key, sizeof key, "TOS_URL_%s", Language);
    if (limeGetPropertyString(key) == NULL)
        memcpy(Language, "EN", 3);
    Load_SettingsData();
    /* port: the layout chosen in the launcher wins over the saved one; the
     * pause menu can still change it for the session */
    if (g_cfg_buttons == 5 || g_cfg_buttons == 6)
        Settings[4] = g_cfg_buttons;
}

#ifdef _WIN32
/* A crash prints where it happened, as offsets into the executable, so that
 *   i686-w64-mingw32-addr2line -f -e umk3-game.exe 0x<image base + offset>
 * names the function. Bringing up a fight engine of two thousand functions
 * crashes in places no log line was planned for; this is the log line. */
#include <windows.h>
#include <io.h>
#include <stdint.h>

/* The function an address falls in, from the exe's own COFF symbol table
 * (the build does not strip it): the nearest function symbol at or below
 * the address. Read from the file on disk, so it works after any crash
 * that leaves the file system usable. "?" when nothing matches. */
static void crash_symbol(const void *addr, char *out, size_t n)
{
    HMODULE self = GetModuleHandleA(NULL);
    unsigned long rva = (unsigned long)((uintptr_t)addr - (uintptr_t)self);
    unsigned long best = 0;
    char path[MAX_PATH], name[9];
    unsigned char h[24], *syms = NULL, *sec = NULL;
    unsigned long pe, symtab, nsym, nsec, opt, strsz = 0, i;
    char *strs = NULL;
    FILE *f;

    snprintf(out, n, "?");
    {
        /* outside the exe: name the DLL instead (opengl32, a driver...) */
        HMODULE m = NULL;
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                               | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               (LPCSTR)addr, &m) && m != self) {
            char *base;
            if (!GetModuleFileNameA(m, path, MAX_PATH))
                strcpy(path, "?");
            base = strrchr(path, '\\');
            snprintf(out, n, "%s+0x%lx", base ? base + 1 : path,
                     (unsigned long)((uintptr_t)addr - (uintptr_t)m));
            return;
        }
        if (!m) {
            snprintf(out, n, "0x%08lx (no module)",
                     (unsigned long)(uintptr_t)addr);
            return;
        }
    }
    if (!GetModuleFileNameA(NULL, path, MAX_PATH) || !(f = fopen(path, "rb")))
        return;
    if (fseek(f, 0x3c, SEEK_SET) || fread(h, 1, 4, f) != 4)
        goto done;
    pe = h[0] | h[1] << 8 | (unsigned long)h[2] << 16 | (unsigned long)h[3] << 24;
    if (fseek(f, (long)pe, SEEK_SET) || fread(h, 1, 24, f) != 24)
        goto done;
    nsec   = h[6] | h[7] << 8;
    symtab = h[12] | h[13] << 8 | (unsigned long)h[14] << 16 | (unsigned long)h[15] << 24;
    nsym   = h[16] | h[17] << 8 | (unsigned long)h[18] << 16 | (unsigned long)h[19] << 24;
    opt    = h[20] | h[21] << 8;
    if (!symtab || !nsym || nsym > 1000000 || !nsec || nsec > 96)
        goto done;
    sec  = malloc(nsec * 40);
    syms = malloc(nsym * 18);
    if (!sec || !syms || fseek(f, (long)(pe + 24 + opt), SEEK_SET)
        || fread(sec, 40, nsec, f) != nsec
        || fseek(f, (long)symtab, SEEK_SET) || fread(syms, 18, nsym, f) != nsym
        || fread(h, 1, 4, f) != 4)
        goto done;
    strsz = h[0] | h[1] << 8 | (unsigned long)h[2] << 16 | (unsigned long)h[3] << 24;
    if (strsz >= 4 && strsz < (64ul << 20) && (strs = malloc(strsz)) != NULL) {
        memset(strs, 0, 4);
        if (fread(strs + 4, 1, strsz - 4, f) != strsz - 4)
            strsz = 0;
    }
    for (i = 0; i < nsym; i++) {
        const unsigned char *s = syms + i * 18;
        unsigned secno = s[12] | s[13] << 8;
        unsigned long v = s[8] | s[9] << 8 | (unsigned long)s[10] << 16
                        | (unsigned long)s[11] << 24;
        unsigned type = s[14] | s[15] << 8;

        if (secno >= 1 && secno <= nsec && (type & 0x20)) {   /* a function */
            const unsigned char *sh = sec + (secno - 1) * 40;
            unsigned long at = v + (sh[12] | sh[13] << 8
                                    | (unsigned long)sh[14] << 16
                                    | (unsigned long)sh[15] << 24);
            if (at <= rva && at >= best) {
                best = at;
                if (s[0] | s[1] | s[2] | s[3]) {
                    memcpy(name, s, 8);
                    name[8] = 0;
                    snprintf(out, n, "%s+0x%lx", name, rva - at);
                } else {
                    unsigned long o = s[4] | s[5] << 8
                                    | (unsigned long)s[6] << 16
                                    | (unsigned long)s[7] << 24;
                    snprintf(out, n, "%s+0x%lx",
                             (strs && o < strsz) ? strs + o : "?", rva - at);
                }
            }
        }
        i += s[17];                     /* skip the aux records */
    }
done:
    free(sec);
    free(syms);
    free(strs);
    fclose(f);
}

static const char *crash_reason(unsigned long code)
{
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION:      return "memory access violation";
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "array index out of bounds";
    case EXCEPTION_DATATYPE_MISALIGNMENT: return "misaligned access";
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:    return "float division by zero";
    case EXCEPTION_INT_DIVIDE_BY_ZERO:    return "integer division by zero";
    case EXCEPTION_ILLEGAL_INSTRUCTION:   return "illegal instruction";
    case EXCEPTION_PRIV_INSTRUCTION:      return "privileged instruction";
    case EXCEPTION_STACK_OVERFLOW:        return "stack overflow";
    case EXCEPTION_IN_PAGE_ERROR:         return "page error (file read failed)";
    default:                              return "unknown exception";
    }
}

void plat_crash_restore(void);          /* win32_gl.c: desktop mode, no window */

/* A crash writes logs\crash-<date>-<time>.log beside the exe -- the reason,
 * the function, the call stack by name, what the game was doing -- copies it
 * to the session log, and shows the reason in a message box, so a player can
 * send the file and the next fix starts from a name instead of an address. */
static LONG WINAPI on_crash(EXCEPTION_POINTERS *ep)
{
    static char path[MAX_PATH + 64], msg[2048];
    HMODULE self = GetModuleHandleA(NULL);
    unsigned long code = (unsigned long)ep->ExceptionRecord->ExceptionCode;
    void *at = ep->ExceptionRecord->ExceptionAddress;
    char fn[160], detail[96] = "";
    FILE *f = NULL;
    SYSTEMTIME t;
    DWORD len;
    char *slash;
    int pos;

    crash_symbol(at, fn, sizeof fn);
    if (code == EXCEPTION_ACCESS_VIOLATION
        && ep->ExceptionRecord->NumberParameters >= 2)
        snprintf(detail, sizeof detail, " (%s address 0x%08lx)",
                 ep->ExceptionRecord->ExceptionInformation[0] == 1 ? "writing"
                 : ep->ExceptionRecord->ExceptionInformation[0] == 8 ? "running"
                 : "reading",
                 (unsigned long)ep->ExceptionRecord->ExceptionInformation[1]);

    pos = snprintf(msg, sizeof msg,
                   "Reason: %s%s\r\nException 0x%08lx in %s\r\n"
                   "Address %p (exe offset 0x%lx)\r\n"
                   "Tick %ld, task %d, screen %d, %s vs %s, stage %ld\r\n\r\n"
                   "Call stack:\r\n  %s\r\n",
                   crash_reason(code), detail, code, fn, at,
                   (unsigned long)((uintptr_t)at - (uintptr_t)self),
                   g_tick, CurrentTask, FE_CurrentTask,
                   CharacterNames[((unsigned long)PLAYER1MODEL) % 26],
                   CharacterNames[((unsigned long)PLAYER2MODEL) % 26],
                   LevelSelect, fn);
#if defined(_M_IX86) || defined(__i386__)
    if (code != EXCEPTION_STACK_OVERFLOW) {
        /* Walk the frame-pointer chain from the faulting context. */
        unsigned long *fp = (unsigned long *)ep->ContextRecord->Ebp;
        int i;
        for (i = 0; i < 24 && fp && !IsBadReadPtr(fp, 8)
                    && pos < (int)sizeof msg - 200; i++) {
            crash_symbol((void *)fp[1], fn, sizeof fn);
            pos += snprintf(msg + pos, sizeof msg - pos, "  %s\r\n", fn);
            fp = (unsigned long *)fp[0];
        }
    }
#endif

    fprintf(stderr, "\ncrash:\n%s", msg);
    fflush(stderr);

    len = GetModuleFileNameA(NULL, path, MAX_PATH);
    slash = (len > 0 && len < MAX_PATH) ? strrchr(path, '\\') : NULL;
    if (slash) {
        strcpy(slash + 1, "logs");
        CreateDirectoryA(path, NULL);
        GetLocalTime(&t);
        sprintf(slash + 1, "logs\\crash-%04d%02d%02d-%02d%02d%02d.log",
                t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
        f = fopen(path, "wb");
        if (f) {
            fputs("UMK3 crash report\r\n\r\n", f);
            fputs(msg, f);
            fclose(f);
        }
    }

    /* scripted runs (UMK3_SHOT) never wait on a dialog */
    if (!g_keys_off) {
        char box[2600];
        plat_crash_restore();
        snprintf(box, sizeof box,
                 "The game stopped because of an error.\r\n\r\n%s\r\n"
                 "Saved to:\r\n%s", msg, f ? path : "(could not write the file)");
        MessageBoxA(NULL, box, "UMK3 - crash",
                    MB_OK | MB_ICONERROR | MB_TOPMOST | MB_SETFOREGROUND);
    }
    return EXCEPTION_EXECUTE_HANDLER;
}

/* stdout and stderr into logs/umk3-<date>-<time>.log beside the exe, unless
 * they already go to a file or pipe (a scripted run's `> x.log`). Returns
 * whether it did, so task changes are logged in every session. */
static int open_session_log(void)
{
    static char path[MAX_PATH + 64];
    DWORD type = GetFileType(GetStdHandle(STD_OUTPUT_HANDLE));
    DWORD len;
    char *slash;
    SYSTEMTIME t;

    if (type == FILE_TYPE_DISK || type == FILE_TYPE_PIPE)
        return 0;
    len = GetModuleFileNameA(NULL, path, MAX_PATH);
    slash = (len > 0 && len < MAX_PATH) ? strrchr(path, '\\') : NULL;
    if (!slash)
        return 0;
    strcpy(slash + 1, "logs");
    CreateDirectoryA(path, NULL);
    GetLocalTime(&t);
    sprintf(slash + 1, "logs\\umk3-%04d%02d%02d-%02d%02d%02d.log",
            t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
    if (!freopen(path, "w", stdout))
        return 0;
    setvbuf(stdout, NULL, _IONBF, 0);
    if (_dup2(_fileno(stdout), _fileno(stderr)) == 0)
        setvbuf(stderr, NULL, _IONBF, 0);
    printf("session log %04d-%02d-%02d %02d:%02d:%02d\n",
           t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
    return 1;
}
#endif

/* UMK3_TAPS: "120:240,160;300:100,40" taps (240,160) at tick 120, and so on.
 * A fourth number holds the press that many ticks: "120:240,160,200". */
typedef struct { long tick; float x, y; long hold; } TAP;
static TAP  g_taps[64];
static int  g_ntaps;

static void parse_taps(const char *s)
{
    while (s && *s && g_ntaps < 64) {
        TAP t;
        t.hold = 3;
        if (sscanf(s, "%ld:%f,%f,%ld", &t.tick, &t.x, &t.y, &t.hold) < 3)
            break;
        g_taps[g_ntaps++] = t;
        s = strchr(s, ';');
        if (s)
            s++;
    }
}

void umk3_relocate_level_info(void);   /* build/level_info.c */
extern int PortHideControls;           /* GameCode.c: hide_controls=1 */

/* umk3.ini beside the exe, written by the launcher (tools/launcher):
 *
 *     width=1440
 *     height=960
 *     fullscreen=0
 *     language=ES
 *
 *     render_width=960
 *     render_height=640
 *     borderless=0
 *
 *     antialiasing=4
 *     aspect=3:2
 *
 * width/height are the window. render_width/render_height are the 3D
 * resolution: the game draws into a buffer that size and it is scaled to the
 * window (see render_init); missing or 0, the game's area on the window.
 * antialiasing= is the multisampling, 0 or 2..16. fullscreen=1 is exclusive
 * fullscreen at the window's size; borderless=1 covers the monitor without a
 * border at the desktop's mode. aspect= is the launcher's: 3:2 (the
 * iPhone's) is the only one so far, and what fit_view always uses.
 * Missing file or keys keep the defaults. */
static int  g_cfg_w = VIRT_W * SCALE, g_cfg_h = VIRT_H * SCALE, g_cfg_full;
static int  g_cfg_rw, g_cfg_rh, g_cfg_borderless, g_cfg_aa;
static int  g_cfg_debug_keys;          /* debug_keys=1: F9..F12, see debug_keys() */
static int  g_cfg_skip_intro;          /* skip_intro=1: no publisher logos */
static char g_cfg_lang[8];
static char g_cfg_frame[1024];        /* marco=: the picture behind the bars */

static void read_config(const char *dir)
{
    char path[1100], line[128];
    FILE *f;

    snprintf(path, sizeof path, "%s%s", dir, "umk3.ini");
    f = fopen(path, "r");
    if (!f)
        return;
    while (fgets(line, sizeof line, f)) {
        char *v = strchr(line, '='), *e;
        if (!v)
            continue;
        *v++ = 0;
        for (e = v + strlen(v); e > v && (unsigned char)e[-1] <= 32; )
            *--e = 0;
        if (strcmp(line, "width") == 0 && atoi(v) >= VIRT_W)
            g_cfg_w = atoi(v);
        else if (strcmp(line, "height") == 0 && atoi(v) >= VIRT_H)
            g_cfg_h = atoi(v);
        else if (strcmp(line, "fullscreen") == 0)
            g_cfg_full = atoi(v) != 0;
        else if (strcmp(line, "borderless") == 0)
            g_cfg_borderless = atoi(v) != 0;
        else if (strcmp(line, "antialiasing") == 0)
            g_cfg_aa = atoi(v) >= 2 ? (atoi(v) > 16 ? 16 : atoi(v)) : 0;
        else if (strcmp(line, "render_width") == 0)
            g_cfg_rw = atoi(v) >= VIRT_W ? atoi(v) : 0;
        else if (strcmp(line, "render_height") == 0)
            g_cfg_rh = atoi(v) >= VIRT_H ? atoi(v) : 0;
        else if (strcmp(line, "language") == 0)
            snprintf(g_cfg_lang, sizeof g_cfg_lang, "%s", v);
        else if (strcmp(line, "buttons") == 0)
            g_cfg_buttons = atoi(v);
        else if (strncmp(line, "key5_", 5) == 0) {
            static const char *const names5[] = { "p", "b", "k", "r" };
            int i;

            for (i = 0; i < 4; i++)
                if (strcmp(line + 5, names5[i]) == 0)
                    plat_bind_key(PK_5_P + i, atoi(v));
        }
        else if (strcmp(line, "marco") == 0)
            snprintf(g_cfg_frame, sizeof g_cfg_frame, "%s", v);
        else if (strcmp(line, "debug_keys") == 0)
            g_cfg_debug_keys = atoi(v) != 0;
        else if (strcmp(line, "skip_intro") == 0)
            g_cfg_skip_intro = atoi(v) != 0;
        else if (strcmp(line, "aspect") == 0)
            g_aspect = parse_aspect(v);
        else if (strcmp(line, "ui_size") == 0)        /* iphone or ipad */
            g_ui_k = _stricmp(v, "ipad") == 0 ? 2.0f : 1.0f;
        else if (strcmp(line, "texture_quality") == 0) {
            /* iphone, ipad or max: see draw_gl.c */
            extern int PortTextureQuality;
            PortTextureQuality = _stricmp(v, "max") == 0 ? 2
                               : _stricmp(v, "ipad") == 0 ? 1 : 0;
        }
        else if (strcmp(line, "pad_dbg_menu") == 0)
            g_pad_dbg_mask = pad_combo(v);
        else if (strcmp(line, "pad") == 0)    /* auto, off, xinput, vvvv:pppp */
            plat_pad_select(v);
        else if (strcmp(line, "hide_controls") == 0)
            PortHideControls = atoi(v) != 0;
        else if (strncmp(line, "key_", 4) == 0) {
            /* key_<name>=<virtual-key code>, from the launcher's key setup */
            static const char *const names[] = {
                "up", "down", "left", "right",
                "hp", "lp", "block", "hk", "lk", "run"
            };
            int i;

            for (i = 0; i < 10; i++)
                if (strcmp(line + 4, names[i]) == 0)
                    plat_bind_key(PK_UP + i, atoi(v));
            if (strcmp(line + 4, "pause") == 0)
                plat_bind_key(PK_PAUSE, atoi(v));
            else if (strcmp(line + 4, "moves") == 0)
                plat_bind_key(PK_MOVES, atoi(v));
            else if (strcmp(line + 4, "special") == 0)
                plat_bind_key(PK_SPECIAL, atoi(v));
            else {
                /* debug mode's keys, from the launcher's CONTROLS section */
                static const struct { const char *n; int code; } dbg[] = {
                    { "dbg_menu", PK_TEST }, { "dbg_info", PK_BACK },
                    { "dbg_page_prev", PK_DBG_PAGE_PREV },
                    { "dbg_page_next", PK_DBG_PAGE_NEXT },
                    { "dbg_ko_p2", PK_DBG_KO_P2 }, { "dbg_ko_p1", PK_DBG_KO_P1 },
                    { "dbg_win", PK_DBG_WIN }, { "dbg_lose", PK_DBG_LOSE },
                    { "dbg_scr_prev", PK_DBG_SCR_PREV },
                    { "dbg_scr_next", PK_DBG_SCR_NEXT },
                    { "dbg_scr_menu", PK_DBG_SCR_MENU },
                };
                for (i = 0; i < (int)(sizeof dbg / sizeof dbg[0]); i++)
                    if (strcmp(line + 4, dbg[i].n) == 0)
                        plat_bind_key(dbg[i].code, atoi(v));
            }
        }
    }
    fclose(f);
}

/* Fight debug keys, a test aid and not part of the game -- off unless
 * umk3.ini says debug_keys=1 (or UMK3_DEBUG_KEYS is set).
 *
 *     F9   player 2's health to 0: you win the round
 *     F10  player 1's health to 0: you lose the round
 *     F11  you win the match  (rounds to one short, then F9)
 *     F12  you lose the match (rounds to one short, then F10) -> Continue
 *
 * The health written is the ENGINE's (G + 0x368 / 0x36c, 166 = full) --
 * t_clock4 polls both for <= 0 every tick and goes to t_round_is_over, so the
 * round ends through the game's own path -- plus the HUD's Health[], which
 * DrawHUD's round-end test reads. */
extern long RoundWins[2], WinsNeeded;
extern char *H;                         /* the fight engine's state, 0x0038c674 */
extern int  Health[2];

/* P and M (umk3.ini can rebind them): the HUD's corner buttons, pressed the
 * way a finger presses them -- a three-tick tap on the spot TogglePauseMenu,
 * UpdateInGamePauseMenu and the moves list test, in 480x320 units.
 *
 *      P   not paused: top right (pause)   menu open: RESUME   list: CANCEL
 *      M   not paused: top left (list)     list open: CANCEL
 */
extern long GamePaused;

static float g_ktap_x, g_ktap_y;
static int   g_ktap_left;              /* ticks the synthetic finger stays down */

static void hud_keys(void)
{
    static int was[2];
    int k;

    if (g_ktap_left > 0 && --g_ktap_left == 0)
        lime_touch_ended(g_ktap_x, g_ktap_y, g_ktap_x, g_ktap_y);

    for (k = 0; k < 2; k++) {
        /* the pad: start is P, select (Back / Share / Create) is M */
        int down = plat_key(PK_PAUSE + k)
                   || (g_pad.buttons & (1u << (k ? PB_SELECT : PB_START)));
        int hit = down && !was[k];
        float x = -1.0f, y = 10.0f;

        was[k] = down;
        if (!hit || CurrentTask != 6 || g_ktap_left > 0)
            continue;
        if (k == 0) {
            if (GamePaused == 0 || GamePaused == 2) x = 470.0f;
            else if (GamePaused == 1) { x = 384.0f; y = 18.0f; }
        } else {
            if (GamePaused == 0) x = 10.0f;
            else if (GamePaused == 2) x = 470.0f;
        }
        if (x < 0.0f)
            continue;
        g_ktap_x = x * g_ui_k;
        g_ktap_y = y * g_ui_k;
        g_ktap_left = 3;
        lime_touch_began(g_ktap_x, g_ktap_y);
    }
}

/* A round is in play: past the intro, nobody down, not between rounds, no
 * finisher on, not paused. The debug keys only act then -- pressed during
 * a round's end they ended it a second time, with the other fighter, and
 * both got the round. */
extern long DoIntro, IsInFinishing, RoundSummary;
int dbg_round_live(void)
{
    /* RoundSummary is 1 from a round's end to the next round's fade-in and
     * 2 (RoundSummaryUpdate, 0x2abbc) while the next round plays -- not 0,
     * which only the first round starts from. */
    return CurrentTask == 6 && G != NULL && RoundSummary != 1
        && Health[0] > 0 && Health[1] > 0;
}

/* Arcade, with a ladder chosen: GameMode 0 and Destiny 0..3. */
extern long GameMode, Destiny;
extern int  Stage;
int dbg_in_arcade(void)
{
    return GameMode == 0 && Destiny >= 0 && Destiny <= 3;
}

static void fit_view(int ww, int wh, int *vx, int *vy, int *vw, int *vh);

/* The 3D resolution (render_width/render_height) and antialiasing
 * (antialiasing=2..16): the game's area is drawn into a framebuffer object
 * at that size -- multisampled, then resolved, when antialiasing is on --
 * and stretched onto its place in the window by render_present. Without a
 * 3D resolution the buffer follows the game's area on the window, so
 * antialiasing alone does not scale anything. GL 3.0 / ARB_framebuffer_object
 * entry points, fetched at run time; a driver without them, or a buffer that
 * will not complete, leaves g_rs_on 0 and the game draws into the window as
 * before. A port feature, not in the binary (the iPhone drew at the screen's
 * size, without antialiasing). */
#ifdef _WIN32
#define RS_API __stdcall
#else
#define RS_API
#endif
typedef void   (RS_API *rs_gen_t)(GLsizei, GLuint *);
typedef void   (RS_API *rs_del_t)(GLsizei, const GLuint *);
typedef void   (RS_API *rs_bind_t)(GLenum, GLuint);
typedef void   (RS_API *rs_storage_t)(GLenum, GLenum, GLsizei, GLsizei);
typedef void   (RS_API *rs_storage_ms_t)(GLenum, GLsizei, GLenum, GLsizei, GLsizei);
typedef void   (RS_API *rs_attach_t)(GLenum, GLenum, GLenum, GLuint);
typedef GLenum (RS_API *rs_status_t)(GLenum);
typedef void   (RS_API *rs_blit_t)(GLint, GLint, GLint, GLint, GLint, GLint,
                                   GLint, GLint, GLbitfield, GLenum);
static rs_gen_t        g_rs_gen_fb, g_rs_gen_rb;
static rs_del_t        g_rs_del_fb, g_rs_del_rb;
static rs_bind_t       g_rs_bind_fb, g_rs_bind_rb;
static rs_storage_t    g_rs_storage;
static rs_storage_ms_t g_rs_storage_ms;
static rs_attach_t     g_rs_attach;
static rs_status_t     g_rs_status;
static rs_blit_t       g_rs_blit;
static int       g_rs_on;                       /* buffers in use */
static int       g_rs_samples;                  /* 0, or 2..16 */
static GLuint    g_rs_fbo, g_rs_ms_fbo;         /* resolved, multisampled */
static GLuint    g_rs_rb[4];
static int       g_rs_w, g_rs_h;                /* the buffers' size */
static int       g_rs_vx, g_rs_vy, g_rs_vw, g_rs_vh;  /* where it goes */

#define RS_FRAMEBUFFER       0x8D40
#define RS_READ_FRAMEBUFFER  0x8CA8
#define RS_DRAW_FRAMEBUFFER  0x8CA9
#define RS_RENDERBUFFER      0x8D41
#define RS_COLOR_ATTACHMENT0 0x8CE0
#define RS_DEPTH_ATTACHMENT  0x8D00
#define RS_COMPLETE          0x8CD5
#define RS_RGBA8             0x8058
#define RS_DEPTH24           0x81A6
#define RS_MAX_SAMPLES       0x8D57

static void render_init(void)
{
    GLint max = 0;

    if (g_cfg_rw <= 0 && g_cfg_aa <= 0)
        return;
    g_rs_gen_fb     = (rs_gen_t)plat_gl_proc("glGenFramebuffers");
    g_rs_gen_rb     = (rs_gen_t)plat_gl_proc("glGenRenderbuffers");
    g_rs_del_fb     = (rs_del_t)plat_gl_proc("glDeleteFramebuffers");
    g_rs_del_rb     = (rs_del_t)plat_gl_proc("glDeleteRenderbuffers");
    g_rs_bind_fb    = (rs_bind_t)plat_gl_proc("glBindFramebuffer");
    g_rs_bind_rb    = (rs_bind_t)plat_gl_proc("glBindRenderbuffer");
    g_rs_storage    = (rs_storage_t)plat_gl_proc("glRenderbufferStorage");
    g_rs_storage_ms = (rs_storage_ms_t)plat_gl_proc("glRenderbufferStorageMultisample");
    g_rs_attach     = (rs_attach_t)plat_gl_proc("glFramebufferRenderbuffer");
    g_rs_status     = (rs_status_t)plat_gl_proc("glCheckFramebufferStatus");
    g_rs_blit       = (rs_blit_t)plat_gl_proc("glBlitFramebuffer");
    if (!g_rs_gen_fb || !g_rs_gen_rb || !g_rs_del_fb || !g_rs_del_rb
        || !g_rs_bind_fb || !g_rs_bind_rb || !g_rs_storage || !g_rs_attach
        || !g_rs_status || !g_rs_blit) {
        printf("render resolution: no framebuffer objects, drawing into the window\n");
        return;
    }
    if (g_cfg_aa > 0 && g_rs_storage_ms) {
        glGetIntegerv(RS_MAX_SAMPLES, &max);
        g_rs_samples = g_cfg_aa < max ? g_cfg_aa : max;
        if (g_rs_samples < 2)
            g_rs_samples = 0;
    }
    printf("antialiasing x%d (asked x%d, the card allows x%d)\n",
           g_rs_samples, g_cfg_aa, (int)max);
    g_rs_on = 1;
}

/* (Re)make the buffers at w x h. 0 if they will not complete -- the game
 * then draws into the window. */
static int render_alloc(int w, int h)
{
    int ms = g_rs_samples > 0;

    if (g_rs_fbo) {
        g_rs_del_fb(1, &g_rs_fbo);
        if (g_rs_ms_fbo)
            g_rs_del_fb(1, &g_rs_ms_fbo);
        g_rs_del_rb(4, g_rs_rb);
        g_rs_fbo = g_rs_ms_fbo = 0;
    }
    g_rs_w = w;
    g_rs_h = h;
    g_rs_gen_rb(4, g_rs_rb);

    /* the resolved buffer, the one that is shown: colour, and depth only
     * when the game draws into it directly */
    g_rs_gen_fb(1, &g_rs_fbo);
    g_rs_bind_fb(RS_FRAMEBUFFER, g_rs_fbo);
    g_rs_bind_rb(RS_RENDERBUFFER, g_rs_rb[0]);
    g_rs_storage(RS_RENDERBUFFER, RS_RGBA8, w, h);
    g_rs_attach(RS_FRAMEBUFFER, RS_COLOR_ATTACHMENT0, RS_RENDERBUFFER, g_rs_rb[0]);
    if (!ms) {
        g_rs_bind_rb(RS_RENDERBUFFER, g_rs_rb[1]);
        g_rs_storage(RS_RENDERBUFFER, RS_DEPTH24, w, h);
        g_rs_attach(RS_FRAMEBUFFER, RS_DEPTH_ATTACHMENT, RS_RENDERBUFFER, g_rs_rb[1]);
    }
    if (g_rs_status(RS_FRAMEBUFFER) != RS_COMPLETE)
        goto fail;

    if (ms) {
        g_rs_gen_fb(1, &g_rs_ms_fbo);
        g_rs_bind_fb(RS_FRAMEBUFFER, g_rs_ms_fbo);
        g_rs_bind_rb(RS_RENDERBUFFER, g_rs_rb[2]);
        g_rs_storage_ms(RS_RENDERBUFFER, g_rs_samples, RS_RGBA8, w, h);
        g_rs_attach(RS_FRAMEBUFFER, RS_COLOR_ATTACHMENT0, RS_RENDERBUFFER, g_rs_rb[2]);
        g_rs_bind_rb(RS_RENDERBUFFER, g_rs_rb[3]);
        g_rs_storage_ms(RS_RENDERBUFFER, g_rs_samples, RS_DEPTH24, w, h);
        g_rs_attach(RS_FRAMEBUFFER, RS_DEPTH_ATTACHMENT, RS_RENDERBUFFER, g_rs_rb[3]);
        if (g_rs_status(RS_FRAMEBUFFER) != RS_COMPLETE)
            goto fail;
    }
    g_rs_bind_fb(RS_FRAMEBUFFER, 0);
    printf("render buffer %dx%d, antialiasing x%d\n", w, h, g_rs_samples);
    return 1;

fail:
    printf("render resolution: framebuffer incomplete, drawing into the window\n");
    g_rs_bind_fb(RS_FRAMEBUFFER, 0);
    g_rs_on = 0;
    return 0;
}

/* After the game's frame: the multisampled buffer resolved into the shown
 * one, which is then bound -- the debug menu draws there and reads it back
 * (glReadPixels cannot read a multisampled buffer). */
static void render_resolve(void)
{
    if (!g_rs_on || !g_rs_ms_fbo)
        return;
    g_rs_bind_fb(RS_READ_FRAMEBUFFER, g_rs_ms_fbo);
    g_rs_bind_fb(RS_DRAW_FRAMEBUFFER, g_rs_fbo);
    g_rs_blit(0, 0, g_rs_w, g_rs_h, 0, 0, g_rs_w, g_rs_h,
              GL_COLOR_BUFFER_BIT, GL_NEAREST);
    g_rs_bind_fb(RS_FRAMEBUFFER, g_rs_fbo);
}

/* The game's area on the window, filtered (GL_LINEAR): a smaller buffer is
 * scaled up, a bigger one down (supersampling). */
static void render_present(void)
{
    if (!g_rs_on || !g_rs_fbo)
        return;
    g_rs_bind_fb(RS_READ_FRAMEBUFFER, g_rs_fbo);
    g_rs_bind_fb(RS_DRAW_FRAMEBUFFER, 0);
    g_rs_blit(0, 0, g_rs_w, g_rs_h,
              g_rs_vx, g_rs_vy, g_rs_vx + g_rs_vw, g_rs_vy + g_rs_vh,
              GL_COLOR_BUFFER_BIT, GL_LINEAR);
    g_rs_bind_fb(RS_FRAMEBUFFER, 0);
}

/* plat_ask draws its box on the window over the frame so far: put the
 * frame there first, and draw into the buffer again after. */
static void render_ask_hook(int begin)
{
    if (!g_rs_on || !g_rs_fbo)
        return;
    if (begin) {
        render_resolve();
        render_present();
    } else {
        g_rs_bind_fb(RS_FRAMEBUFFER, g_rs_ms_fbo ? g_rs_ms_fbo : g_rs_fbo);
        glViewport(0, 0, g_rs_w, g_rs_h);
    }
}

/* -[EAGLView drawView]: one tick is one clear and one frame. The clear
 * covers the whole window; in fullscreen (either kind) the frame picture,
 * if any, fills the bars, and the game's own area is cleared to black
 * again -- a stage leaves gaps (sky) it never draws, and the frame showed
 * through them. Leaves the viewport on the game's area, or on the render
 * buffer when there is one. */
static void clear_frame(int ww, int wh, int vx, int vy, int vw, int vh)
{
    if (g_rs_on && g_rs_fbo)
        g_rs_bind_fb(RS_FRAMEBUFFER, 0);
    glViewport(0, 0, ww, wh);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if ((g_cfg_full || g_cfg_borderless) && (vw != ww || vh != wh)) {
        dbg_frame_draw(ww, wh);
        glEnable(GL_SCISSOR_TEST);
        glScissor(vx, vy, vw, vh);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDisable(GL_SCISSOR_TEST);
    }
    if (g_rs_on) {
        int bw = vw, bh = vh, x, y;

        if (g_cfg_rw > 0 && g_cfg_rh > 0)
            fit_view(g_cfg_rw, g_cfg_rh, &x, &y, &bw, &bh);
        if ((bw != g_rs_w || bh != g_rs_h || !g_rs_fbo) && bw > 0 && bh > 0)
            render_alloc(bw, bh);
    }
    if (g_rs_on) {
        g_rs_vx = vx;
        g_rs_vy = vy;
        g_rs_vw = vw;
        g_rs_vh = vh;
        g_rs_bind_fb(RS_FRAMEBUFFER, g_rs_ms_fbo ? g_rs_ms_fbo : g_rs_fbo);
        glViewport(0, 0, g_rs_w, g_rs_h);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        return;
    }
    glViewport(vx, vy, vw, vh);
}

static void fight_key(int k)
{
    {
        if (!dbg_round_live())
            return;
        if (k >= 2 && WinsNeeded > 0) {                 /* F11 / F12 */
            RoundWins[k - 2] = WinsNeeded - 1;
            /* ...and the engine's own tally, H[0] / H[1], which
             * t_player_1_won / t_player_2_won count and t_results_retp
             * compares with 2 before it starts t_game_finished -- Shao
             * Kahn's death at the end of an Arcade ladder. Without it a
             * debug win never ended the ladder the way a real one does. */
            ((unsigned int *)H)[k - 2] = (unsigned int)(WinsNeeded - 1);
        }
        *(unsigned int *)((char *)G + ((k & 1) ? 0x368 : 0x36c)) = 0;
        /* ...and the HUD's copy, which only a hit's MKEvent_Add(3, 0, ..)
         * refreshes: RoundEndedAgainst tests Health[], so without this the
         * winner banner came up and the round never closed. */
        Health[(k & 1) ? 0 : 1] = 0;
        printf("debug key F%d: %s\n", 9 + k,
               k == 0 ? "KO player 2" : k == 1 ? "KO player 1"
             : k == 2 ? "win the match" : "lose the match");
    }
}

static void debug_keys(void)
{
    static int was[4];
    int k;

    for (k = 0; k < 4; k++) {
        int down = plat_key(PK_DBG_KO_P2 + k);
        if (down && !was[k])
            fight_key(k);
        was[k] = down;
    }
}

/* Debug mode (debug_keys=1, the launcher's "Debug mode" box):
 *
 *     F2   the debug menu, drawn over the game (runtime/debug_menu.c):
 *          any fight -- both fighters, Motaro and Shao Kahn included, and
 *          the stage -- any of the front end's 51 screens, the main menu,
 *          and the four fight keys above
 *     F3   the info line: task, front-end screen, fighters, rounds
 *     F6   previous screen     F7   next screen     F8   main menu
 *     F9..F12  as above (F11 wins the match: the whole fight skipped)
 * The menu's DIRECT KEYS row turns F3 and F6..F12 off and on; F2 stays.
 *
 * A screen is opened with the game's own PushFETaskDeferred -- the fade, the
 * push, then FE_Special_Inits -- so Back leaves it the normal way. A screen
 * that needs state its menu would have set first (the tower, the VS screen,
 * the summaries) may not draw right; that is the point of looking. From a
 * fight, a screen or a new fight goes through the fight's own exit, the way
 * QuitAsLose's mode 5 does it (0x269d0): FE_TaskStackPointer = 0, the screen
 * in FE_CurrentTask, a fade out, and Task_GameMain's fade end hands over to
 * Task_GameDestroy. --screen <n|name> (or UMK3_SCREEN) opens one screen once
 * at start. */
extern const char *FETaskNames[DBG_SCREENS];
extern int   PendingPush, FE_TaskStackPointer;
extern float FE_FadeAdd;
extern long  DontQuitAfterFade, RoundSummary;
extern float FE_Fade;
void PushFETaskDeferred(int task);

static int g_screen_jump = -1;          /* --screen: the screen to open */

static int parse_screen(const char *s)
{
    int i;

    if (s[0] >= '0' && s[0] <= '9')
        return atoi(s) < DBG_SCREENS ? atoi(s) : -1;
    for (i = 0; i < DBG_SCREENS; i++)   /* "treasure" -> FE_Task_Treasure */
        if (_stricmp(FETaskNames[i] + 8, s) == 0
            || _stricmp(FETaskNames[i], s) == 0)
            return i;
    return -1;
}

/* Out of the fight to front-end screen `task`. */
static void leave_fight(int task)
{
    FE_TaskStackPointer = 0;
    FE_CurrentTask = task;
    GamePaused = 0;
    DontQuitAfterFade = 0;
    FE_FadeAdd = -0.033333335f;
}

static void jump_screen(int task)
{
    printf("debug: screen %d %s\n", task, FETaskNames[task]);
    if (CurrentTask == 6)
        leave_fight(task);
    else if (CurrentTask == 3)
        PushFETaskDeferred(task);
    else
        g_screen_jump = task;           /* still loading: once the menu is up */
}

static void screen_keys(void)
{
    static int was[3];
    int k;

    for (k = 0; k < 3; k++) {
        int down = plat_key(PK_DBG_SCR_PREV + k);
        int hit = down && !was[k];
        int t = FE_CurrentTask;

        was[k] = down;
        if (!hit || CurrentTask != 3 || PendingPush != -1)
            continue;
        if (k == 0)
            t = (t + DBG_SCREENS - 1) % DBG_SCREENS;
        else if (k == 1)
            t = (t + 1) % DBG_SCREENS;
        else
            t = 0;
        jump_screen(t);
    }
}

extern char *Plyr;
long t_dizzy_sleep(void *thread);

/* Player two's thread sits in t_dizzy_sleep: mercy_xfer (moves.c, armv7
 * 0x54ac4), which every finisher goes through, starts nothing until the loser
 * is there -- q_is_he_dizzy's walk, Plyr[1] (stride 108) -> thread (+4) ->
 * the handler of its current frame (+0xa4 the index, 8 bytes a frame). */
static int opponent_dizzy(void)
{
    char *t = *(char **)(Plyr + 108 + 4);
    unsigned f;

    if (!t)
        return 0;
    f = *(unsigned *)(t + 0xa4);
    return *(uintptr_t *)(t + f * 8 + 4) == (uintptr_t)t_dizzy_sleep;
}

int dbg_finishing(void)
{
    return CurrentTask == 6 && IsInFinishing != 0 && opponent_dizzy();
}

/* DoASpecial (moves.c, armv7 0x51830) is where a typed finisher lands: which
 * 0xd..0x13 = pit, mercy, fatality 1, fatality 2, animality, babality,
 * friendship. Called for player one as the joystick code would, with the
 * finishing window open, so it takes the same gated path (the distance walk,
 * the mercy and friendship conditions). */
void DoASpecial(void *obj, unsigned int which);

/* ------------------------------------------------- debug tools (port only)
 *
 * Save states: runtime/savestate.c holds the copies; here is when they are
 * allowed and what has to follow a load -- the voices were reading sounds
 * the state put back, and the tune has to be the state's. In the menus and
 * in a fight, not while something loads. */
void limeRestartPlayTune(void);

int dbg_state_can_save(void)
{
    return st_available() && (CurrentTask == 3 || CurrentTask == 6);
}

int dbg_state_has(int slot)
{
    return st_has(slot);
}

static void state_save(int slot)
{
    const char *why;

    if (!dbg_state_can_save()) {
        printf("debug: state %d not saved (%s)\n", slot + 1,
               st_available() ? "only in a fight or the menus"
                              : "save states are off");
        return;
    }
    if (st_save(slot, &why) == 0)
        printf("debug: state %d saved, %.1f MB\n", slot + 1,
               st_size(slot) / 1048576.0);
    else
        printf("debug: state %d not saved (%s)\n", slot + 1, why);
}

static void state_load(int slot)
{
    const char *why;

    if (st_load(slot, &why) != 0) {
        printf("debug: state %d not loaded (%s)\n", slot + 1, why);
        return;
    }
    plat_audio_stop_all();
    plat_music_stop();
    limeRestartPlayTune();
    printf("debug: state %d loaded, task %d\n", slot + 1, CurrentTask);
}

/* Hitboxes: the boxes the fight engine itself tests, in its own arcade
 * coordinates (x right, y down, 64 to a world unit), drawn into the 3D
 * scene at the fighters' depth: x/64 plus or minus the character's
 * posOffsetX by facing (PlayerDefs, the character at +0x24 of the object),
 * as ArcadePosTo3dPos does, and up from the engine's floor, (floor - y)/64.
 *
 *   - Each fighter's body: the extents of its current frame (+0x34 left,
 *     +0x3c right, +0x38 top, +0x40 bottom, from x +0x0e and y +0x12,
 *     mirrored by bit 0x10 of +0x28), what leftmost/rightmost/highest/
 *     lowest_mpart_ob read for a strike. Player one blue, player two green.
 *   - Every strike strike_check_regs tests (PortStrikeHook): the attack's
 *     box red, and the target's box as squeezed for that test (cbox_squeeze)
 *     yellow; both white when they met. Shown for a third of a second. */
struct hb_box { int32_t l, t, r, b; };
struct hb_off { float dx, dz; };        /* arcade -> world, per character */
static struct {
    struct hb_box atk, tgt;
    struct hb_off atk_off, tgt_off;
    long tick;
    int hit;
} g_hb[24];
static int g_hb_next;

/* PlayerDefs' entry as the host lays it out (GameCode.c's PLAYERDEF). */
struct hb_def {
    long id;
    float scale, posOffsetX, height, renderOffsetX, renderOffsetZ;
    const char *names[7];
};
extern const struct hb_def PlayerDefs[26];

static struct hb_off hb_offset(const char *o)
{
    struct hb_off f = { 0.0f, 0.0f };
    uint32_t c;

    /* the engine's floor (G +0xac, RoundParam[2] + 0xf7 in mk3_update)
     * is the arena's ground, z 0: the models' own height would put the
     * boxes a body lower */
    f.dz = G ? *(const int32_t *)((const char *)G + 0xac) / 64.0f : 0.0f;
    if (!o)
        return f;
    c = *(const uint32_t *)(o + 0x24);
    if (c >= 26)
        return f;
    f.dx = (*(const uint32_t *)(o + 0x28) & 0x10) ? -PlayerDefs[c].posOffsetX
                                                 : PlayerDefs[c].posOffsetX;
    return f;
}

/* A strike's boxes: from the floor, without a character's posOffsetX (at
 * most 0.24 of a unit, 15 arcade pixels) -- the hook does not say whose. */
static void hb_strike(const void *attack, const void *target, long hit)
{
    g_hb[g_hb_next].atk = *(const struct hb_box *)attack;
    g_hb[g_hb_next].tgt = *(const struct hb_box *)target;
    g_hb[g_hb_next].atk_off = hb_offset(NULL);
    g_hb[g_hb_next].tgt_off = hb_offset(NULL);
    g_hb[g_hb_next].tick = g_tick;
    g_hb[g_hb_next].hit = hit != 0;
    g_hb_next = (g_hb_next + 1) % (int)(sizeof g_hb / sizeof g_hb[0]);
}

static void hb_draw_box(struct hb_box b, struct hb_off f, float depth,
                        float r, float g, float bl, int fill)
{
    float v[12];
    float x0 = b.l / 64.0f + f.dx, x1 = b.r / 64.0f + f.dx;
    float z0 = -b.t / 64.0f + f.dz, z1 = -b.b / 64.0f + f.dz;

    v[0] = x0; v[1] = depth; v[2]  = z0;
    v[3] = x1; v[4] = depth; v[5]  = z0;
    v[6] = x1; v[7] = depth; v[8]  = z1;
    v[9] = x0; v[10] = depth; v[11] = z1;
    glVertexPointer(3, GL_FLOAT, 0, v);
    if (fill) {
        glColor4f(r, g, bl, 0.22f);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    }
    glColor4f(r, g, bl, 1.0f);
    glDrawArrays(GL_LINE_LOOP, 0, 4);
}

static struct hb_box hb_body(const char *o)
{
    struct hb_box b;
    int32_t x = *(const int16_t *)(o + 0x0e), y = *(const int16_t *)(o + 0x12);
    int32_t l = *(const int32_t *)(o + 0x34), r = *(const int32_t *)(o + 0x3c);

    if (*(const uint32_t *)(o + 0x28) & 0x10) {     /* facing left */
        b.l = x - r;
        b.r = x - l;
    } else {
        b.l = x + l;
        b.r = x + r;
    }
    b.t = y + *(const int32_t *)(o + 0x38);
    b.b = y + *(const int32_t *)(o + 0x40);
    return b;
}

static void hb_draw(void)
{
    extern unsigned char PlayerZPos[4];
    static const GLenum arrays[3] = {
        GL_TEXTURE_COORD_ARRAY, GL_COLOR_ARRAY, GL_NORMAL_ARRAY
    };
    GLint was[3], vert;
    float depth;
    int i;

    if (CurrentTask != 6 || !G)
        return;
    memcpy(&depth, PlayerZPos, 4);
    depth -= 0.5f;                      /* in front of the fighters */

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
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glLineWidth(2.0f);

    for (i = 0; i < 2; i++) {
        const char *o = *(const char *const *)((const char *)G
                                               + (i ? 0x214 : 0xbc));
        if (o)
            hb_draw_box(hb_body(o), hb_offset(o), depth, i ? 0.2f : 0.25f,
                        i ? 1.0f : 0.55f, i ? 0.3f : 1.0f, 1);
    }
    for (i = 0; i < (int)(sizeof g_hb / sizeof g_hb[0]); i++) {
        if (g_hb[i].tick == 0 || g_tick - g_hb[i].tick > 20)
            continue;
        if (g_hb[i].hit) {
            hb_draw_box(g_hb[i].tgt, g_hb[i].tgt_off, depth - 0.1f,
                        1.0f, 1.0f, 1.0f, 0);
            hb_draw_box(g_hb[i].atk, g_hb[i].atk_off, depth - 0.2f,
                        1.0f, 1.0f, 1.0f, 1);
        } else {
            hb_draw_box(g_hb[i].tgt, g_hb[i].tgt_off, depth - 0.1f,
                        1.0f, 0.9f, 0.1f, 0);
            hb_draw_box(g_hb[i].atk, g_hb[i].atk_off, depth - 0.2f,
                        1.0f, 0.15f, 0.15f, 1);
        }
    }

    for (i = 0; i < 3; i++)
        if (was[i])
            glEnableClientState(arrays[i]);
    if (!vert)
        glDisableClientState(GL_VERTEX_ARRAY);
    glPopAttrib();
}

/* The select screen's colour picker (PortPalette, FrontEnd.c) survives a
 * restart, and so does the palette an Arcade ladder began with
 * (PortArcadePalette), for a ladder continued later: "p1 p2 arcade" in
 * save/palette.txt, read at start, written whenever one changes. A scripted
 * run neither reads nor writes it, unless UMK3_SAVE_DIR points it
 * elsewhere. Port only. */
extern int PortPalette[2], PortArcadePalette;
void port_save_path(char *out, size_t n, const char *file);

static int palette_scripted(void)
{
    return (getenv("UMK3_SHOT") || getenv("UMK3_SHOTS"))
           && !getenv("UMK3_SAVE_DIR");
}

static void palette_load(void)
{
    char path[1100];
    FILE *f;
    int a = 0, b = 0, c = 0;

    if (palette_scripted())
        return;
    port_save_path(path, sizeof path, "palette.txt");
    f = fopen(path, "r");
    if (!f)
        return;
    if (fscanf(f, "%d %d %d", &a, &b, &c) >= 2) {
        PortPalette[0] = (a == 1 || a == 2) ? a : 0;
        PortPalette[1] = (b == 1 || b == 2) ? b : 0;
        PortArcadePalette = (c == 1 || c == 2) ? c : 0;
    }
    fclose(f);
}

static void palette_sync(void)
{
    static int last[3], init;
    char path[1100];
    FILE *f;

    if (palette_scripted())
        return;
    if (init && last[0] == PortPalette[0] && last[1] == PortPalette[1]
        && last[2] == PortArcadePalette)
        return;
    init = 1;
    last[0] = PortPalette[0];
    last[1] = PortPalette[1];
    last[2] = PortArcadePalette;
    port_save_path(path, sizeof path, "palette.txt");
    f = fopen(path, "w");
    if (f) {
        fprintf(f, "%d %d %d\n", PortPalette[0], PortPalette[1],
                PortArcadePalette);
        fclose(f);
    }
}

/* On every tick: the hooks on while HITBOXES is (or UMK3_DBG_HITBOX=1). */

static void hb_update(void)
{
    extern void (*PortAfterScene)(void);
    extern void (*PortStrikeHook)(const void *, const void *, long);
    int on = (g_cfg_debug_keys && dbg_hitboxes_on())
             || getenv("UMK3_DBG_HITBOX");

    PortAfterScene = on ? hb_draw : NULL;
    PortStrikeHook = on ? hb_strike : NULL;
}

/* The fight's speed, from FIGHT SPEED (or UMK3_DBG_SPEED=<percent>): the
 * clock that feeds the ticks runs that much faster or slower. */
static double fight_speed(void)
{
    int pct = getenv("UMK3_DBG_SPEED") ? atoi(getenv("UMK3_DBG_SPEED"))
            : g_cfg_debug_keys ? dbg_speed_percent() : 100;

    if (CurrentTask != 6 || dbg_menu_is_open() || pct <= 0)
        return 1.0;
    return pct / 100.0;
}

static void debug_request(const struct dbg_request *rq)
{
    switch (rq->what) {
    case DBG_SAVE_STATE:
        state_save(rq->a);
        break;
    case DBG_LOAD_STATE:
        state_load(rq->a);
        break;
    case DBG_FINISHER:
        if (!dbg_finishing())
            break;
        printf("debug: finisher %d (DoASpecial 0x%x), G+0x45c %d G+0x450 %d\n",
               rq->a, 0xd + rq->a, *(short *)((char *)G + 0x45c),
               *(short *)((char *)G + 0x450));
        DoASpecial(Plyr, 0xd + (unsigned int)rq->a);
        break;
    case DBG_FIGHT:
        g_fight_p1 = rq->a;
        g_fight_p2 = rq->b;
        g_fight_stage = rq->c;
        printf("debug: fight %s vs %s, stage %d\n",
               CharacterNames[rq->a], CharacterNames[rq->b], rq->c);
        /* Started by the --fight code once the front end has been up for
         * half a second -- so a pick made while loading just waits. */
        if (CurrentTask == 6)
            leave_fight(0);
        break;
    case DBG_SCREEN:
        jump_screen(rq->a);
        break;
    case DBG_FIGHT_KEY:
        fight_key(rq->a);
        break;
    case DBG_BOSS:
        /* PopulateTower puts Motaro at rung Destiny + 6 and Shao Kahn at
         * Destiny + 7; QuitAsWin moves Stage up one on a win. In a fight:
         * the rung below the boss, then win, and the game climbs to it by
         * its own path. Elsewhere: that rung, and the tower again. */
        if (!dbg_in_arcade())
            break;
        g_dbg_saved.keep = 1;           /* the climb is the point */
        if (CurrentTask == 6) {
            Stage = Destiny + (rq->a == 24 ? 6 : 7) - 1;
            if (dbg_round_live())
                fight_key(2);
        } else {
            Stage = Destiny + (rq->a == 24 ? 6 : 7);
            if (CurrentTask == 3)
                jump_screen(28);
        }
        printf("debug: arcade, next fight %s (rung %d)\n",
               rq->a == 24 ? "Motaro" : "Shao Kahn", Stage);
        break;
    }
}

/* The largest 3:2 rectangle centred in the window: the game is drawn for a
 * 480x320 screen, so a fullscreen 16:9 monitor gets bars, not a stretch. */
static void fit_view(int ww, int wh, int *vx, int *vy, int *vw, int *vh)
{
    if ((long)ww * g_virt_h > (long)wh * g_virt_w) {
        *vh = wh;
        *vw = (int)((long)wh * g_virt_w / g_virt_h);
    } else {
        *vw = ww;
        *vh = (int)((long)ww * g_virt_h / g_virt_w);
    }
    *vx = (ww - *vw) / 2;
    *vy = (wh - *vh) / 2;
}

/* Input lag: a port's own, nothing of this is in the binary.
 *
 * With vsync the swap returns at the screen's refresh. Reading the keys
 * right then and drawing at once leaves the finished frame waiting most of
 * a refresh for the next one, and the keys it shows are that old. Instead
 * wait here until just before the refresh -- the screen's period less the
 * longest a frame has lately taken to tick and draw, less a margin -- and
 * only then read the keys, tick and draw. The press reaches the screen in
 * about the time the frame takes to make, not a whole refresh later.
 * Without vsync the swap does not wait, the period measured is about the
 * work itself, and this never sleeps. UMK3_FRAME_DELAY=0 turns it off. */
static double g_pace_vbl = -1.0, g_pace_gap, g_pace_work = 0.004;
static double g_pace_start;

/* Only on a screen of about 60 Hz. A faster one already shows a tick within
 * a few ms of making it: the loop spins, reading the keys, until the tick is
 * due, and the next refresh is close. */
static void pace_frame(void)
{
    static int off = -1, hz;
    double now = plat_time(), period;

    if (off < 0) {
        off = getenv("UMK3_FRAME_DELAY") && atoi(getenv("UMK3_FRAME_DELAY")) == 0;
        hz = getenv("UMK3_PACE_HZ") ? atoi(getenv("UMK3_PACE_HZ"))
                                    : plat_refresh_hz();   /* for tests */
    }
    period = (hz >= 50 && hz <= 65) ? 1.0 / hz : 0.0;
    if (g_pace_vbl >= 0.0) {
        double d = now - g_pace_vbl;
        g_pace_gap = g_pace_gap * 0.95 + d * 0.05;
        if (period > 0.0 && d > period * 1.5 && d < 0.1) {
            /* a refresh missed: the wait was too long, keep more margin */
            g_pace_work += 0.001;
            if (g_pace_work > 0.012)
                g_pace_work = 0.012;
        }
    }
    g_pace_vbl = now;
    if (getenv("UMK3_LOG_PACE")) {      /* UMK3_LOG_PACE=1: once a second */
        static int n;
        if (++n % 60 == 0)
            printf("pace: screen %d Hz, between swaps %.2f ms, frame work %.2f ms, wait %.2f ms\n",
                   hz, g_pace_gap * 1e3, g_pace_work * 1e3,
                   off || !period ? 0.0
                                  : (period - g_pace_work - 0.003) * 1e3);
    }
    /* the swap waited for the refresh only if it took a while: with vsync
     * off it returns at once and there is nothing to wait for */
    if (!off && period > 0.0 && g_pace_gap > period * 0.8) {
        double wake = now + period - g_pace_work - 0.003;
        if (wake > now)
            plat_wait_until(wake);
    }
}

/* The frame's work starts here, once a tick is due: not the spin before. */
static void pace_work_start(void)
{
    g_pace_start = plat_time();
}

static void pace_work_end(void)
{
    double w = plat_time() - g_pace_start;

    if (w > 0.012)          /* a load, not a frame: do not wait for long */
        w = 0.012;
    /* the longest lately: up at once, down over a second or so */
    g_pace_work = w > g_pace_work ? w : g_pace_work * 0.98 + w * 0.02;
}

int main(int argc, char **argv)
{
    const char *root = (argc > 1 && argv[1][0] != '-') ? argv[1] : "res";
    const char *shot = getenv("UMK3_SHOT");
    int    shot_at = shot ? atoi(shot) : 0;
    int    log_tasks = getenv("UMK3_LOG_TASKS") != NULL;
    long   ticks = 0;
    int    focused = 1, was_down = 0, last_task = -1, last_fe = -1;
    double last, acc = 0.0;

    setvbuf(stdout, NULL, _IONBF, 0);
    /* --list-pads: the pads attached, one "pad=value<TAB>name" a line, and
     * out -- the launcher's CONTROLS > GAMEPAD list comes from here */
    if (argc > 1 && strcmp(argv[1], "--list-pads") == 0) {
        plat_pad_list();
        return 0;
    }
#ifdef _WIN32
    /* No argument: `res` beside the exe, wherever it was started from, so a
     * double-click in the game folder just works. */
    static char exe_res[MAX_PATH + 8];
    if (argc <= 1 || argv[1][0] == '-') {
        DWORD len = GetModuleFileNameA(NULL, exe_res, MAX_PATH);
        char *slash = (len > 0 && len < MAX_PATH) ? strrchr(exe_res, '\\') : NULL;
        if (slash) {
            slash[1] = 0;
            read_config(exe_res);
            strcpy(slash + 1, "res");
            root = exe_res;
        }
    }
    if (g_cfg_lang[0] && !getenv("UMK3_LANG")) {
        static char env[24];
        snprintf(env, sizeof env, "UMK3_LANG=%s", g_cfg_lang);
        _putenv(env);
    }
    /* The game reads only this folder. Say what to do instead of opening a
     * black window when it is missing or was set up without Info.plist. */
    {
        char probe[MAX_PATH + 32];
        FILE *f;
        _snprintf(probe, sizeof probe, "%s\\Info.plist", root);
        probe[sizeof probe - 1] = 0;
        f = fopen(probe, "rb");
        if (!f) {
            MessageBoxA(NULL,
                "No se encontro la carpeta res completa junto a umk3-game.exe.\n\n"
                "Abre UMK3-Launcher.exe, elige tu UMK3 .ipa y pulsa Compilar.\n\n"
                "The res folder beside umk3-game.exe is missing or incomplete.\n"
                "Open UMK3-Launcher.exe, choose your UMK3 .ipa and press Compilar.",
                "Ultimate Mortal Kombat 3", MB_OK | MB_ICONERROR);
            return 1;
        }
        fclose(f);
    }
    SetUnhandledExceptionFilter(on_crash);
    if (open_session_log())
        log_tasks = 1;
#endif
    if (getenv("UMK3_DEBUG_KEYS"))
        g_cfg_debug_keys = 1;
    if (getenv("UMK3_SKIP_INTRO"))
        g_cfg_skip_intro = 1;
    if (getenv("UMK3_ASPECT"))          /* 4:3 16:9 21:9, over aspect= */
        g_aspect = parse_aspect(getenv("UMK3_ASPECT"));
    if (getenv("UMK3_UI"))              /* 0 1, over ui_size= */
        g_ui_k = atoi(getenv("UMK3_UI")) ? 2.0f : 1.0f;
    if (getenv("UMK3_TEXQ")) {          /* 0 1 2, over texture_quality= */
        extern int PortTextureQuality;
        PortTextureQuality = atoi(getenv("UMK3_TEXQ"));
    }
    if (getenv("UMK3_PAD"))             /* over umk3.ini's pad= */
        plat_pad_select(getenv("UMK3_PAD"));
    if (getenv("UMK3_BUTTONS"))         /* 5 or 6, over umk3.ini's buttons= */
        g_cfg_buttons = atoi(getenv("UMK3_BUTTONS"));
    g_keys_off = shot_at != 0;
    if (getenv("UMK3_SCREEN"))
        g_screen_jump = parse_screen(getenv("UMK3_SCREEN"));
    parse_taps(getenv("UMK3_TAPS"));
    palette_load();
    {
        int i;
        for (i = 1; i < argc; i++)
            if (strcmp(argv[i], "--fight") == 0 && i + 2 < argc) {
                parse_fight(argv[i + 1], argv[i + 2],
                            i + 3 < argc ? argv[i + 3] : NULL);
                break;
            }
        for (i = 1; i + 1 < argc; i++)
            if (strcmp(argv[i], "--screen") == 0)
                g_screen_jump = parse_screen(argv[i + 1]);
        if (g_fight_p1 < 0 && getenv("UMK3_FIGHT")) {
            char buf[128], *a, *b, *c;
            snprintf(buf, sizeof buf, "%s", getenv("UMK3_FIGHT"));
            a = buf;
            b = strchr(a, ',');
            if (b) {
                *b++ = 0;
                c = strchr(b, ',');
                if (c)
                    *c++ = 0;
                parse_fight(a, b, c);
            }
        }
    }

    plat_open_borderless = g_cfg_borderless && !g_cfg_full;
    if (!plat_open("Ultimate Mortal Kombat 3", g_cfg_w, g_cfg_h)) {
        fprintf(stderr, "could not open a window\n");
        return 1;
    }
    render_init();
    plat_ask_hook = render_ask_hook;
    if (g_cfg_full || g_cfg_borderless) {
        if (g_cfg_full)
            plat_fullscreen_exclusive();
        else
            plat_fullscreen();
        /* the picture for the bars: marco= in umk3.ini, else marco.png
         * beside the exe; none, and the bars stay black */
        if (!dbg_frame_load(g_cfg_frame[0] ? g_cfg_frame : NULL)) {
            char p[1100];
            snprintf(p, sizeof p, "%s", root);
            if (strlen(p) > 3 && (strrchr(p, '\\') || strrchr(p, '/'))) {
                char *sl = strrchr(p, '\\') ? strrchr(p, '\\') : strrchr(p, '/');
                snprintf(sl + 1, sizeof p - (size_t)(sl + 1 - p), "marco.png");
                dbg_frame_load(p);
            }
        }
    }
    lime_platform_set_asset_root(root);
    /* Narrower than 3:2 keeps the width and grows the height (4:3 at 480
     * wide is 480x360, the iPad's shape); wider keeps the height (16:9 at
     * 320 high is 568x320, the iPhone 5's). Even sizes. */
    if (g_aspect <= 1.5f) {
        g_fight_w = (int)(VIRT_W * g_ui_k);
        g_fight_h = ((int)(g_fight_w / g_aspect + 0.5f)) & ~1;
        g_menu_w = g_fight_w;
        g_menu_h = g_fight_h;
    } else {
        g_fight_h = (int)(VIRT_H * g_ui_k);
        g_fight_w = ((int)(g_fight_h * g_aspect + 0.5f)) & ~1;
        /* the menus wide too, their buttons out to the corners (FE_X) */
        extern int PortWideMenus;
        PortWideMenus = 1;
        g_menu_w = g_fight_w;
        g_menu_h = g_fight_h;
    }
    use_surface(0);

    start_app();
    /* Before GameCodeInit: the front end and the fight engine have to share
     * one G, H, MKEventQueue and RoundParam -- the storage the fight's tables
     * point into -- before anything is written through them. */
    fight_runtime_init();
    /* Level_Info's scene and layer names are iOS addresses until this runs
     * (tools/level_info.py); GameInit_LoadABit reads them at steps 27-29. */
    umk3_relocate_level_info();
    GameCodeInit();
    /* Port, not in the binary: in a fight PlayerAutoSmoothAnims draws each
     * fighter's pose AnimSmoothWindowSize ticks late (2 at 0x171368) while
     * positions, hits, blood and sound are immediate -- felt as input delay,
     * and seen as the victim reacting before the blow lands. 0 draws the
     * current frame (a hard cut every tick). The front end and the intro
     * borrow their own window (40, 20) around each call and put this back. */
    {
        extern long AnimSmoothWindowSize;
        AnimSmoothWindowSize = 0;
    }

    last = plat_time();
    while (plat_poll()) {
        int mx, my, down, ww, wh, vx, vy, vw, vh;

        /* The window's focus is the app's foreground; see runtime/lime_app.c. */
        {
            int f = shot_at ? 1 : plat_focused();
            if (f != focused) {
                focused = f;
                if (f) {
                    lime_app_become_active();
                    last = plat_time();
                    acc = 0.0;
                } else {
                    lime_app_resign_active();
                }
            }
            if (!f) {
                plat_swap();
                continue;
            }
        }

        {
            /* A frame that took one tick's time, give or take 2 ms, counts
             * as exactly one tick: on a 60 Hz screen the clock and the
             * refresh would otherwise drift apart and now and then run two
             * ticks in one frame and none in the next -- a press that
             * waits a whole frame to show. */
            double now = plat_time(), dt = now - last;
            if (dt > 1.0 / 60.0 - 0.002 && dt < 1.0 / 60.0 + 0.002)
                dt = 1.0 / 60.0;
            acc += dt * fight_speed();  /* the debug menu's FIGHT SPEED */
            last = now;
        }
        if (acc > 0.25)
            acc = 0.25;
        if (acc < 1.0 / 60.0)
            continue;
        pace_work_start();

        use_surface(CurrentTask == 6);
        plat_size(&ww, &wh);
        fit_view(ww, wh, &vx, &vy, &vw, &vh);

        /* The mouse as one finger, on its edges; see runtime/menu_main.c. */
        /* A scripted run is the script's alone: a click on the window
         * would be a second finger nobody asked for. */
        down = plat_mouse(&mx, &my) && g_ntaps == 0 && !dbg_menu_is_open();
        {
            static float prev_tx = -1.0f, prev_ty = -1.0f;
            float tx = (float)(mx - vx) * g_virt_w / (vw ? vw : 1);
            float ty = (float)(my - vy) * g_virt_h / (vh ? vh : 1);

            if (dbg_menu_is_open())     /* the debug menu takes the mouse, 480x320 */
                dbg_menu_mouse(tx / g_ui_k, ty / g_ui_k,
                               plat_mouse(&mx, &my) && g_ntaps == 0);

            if (down && !was_down)
                lime_touch_began(tx, ty);
            else if (down && (tx != prev_tx || ty != prev_ty))
                lime_touch_moved(tx, ty, prev_tx, prev_ty);
            else if (!down && was_down)
                lime_touch_ended(prev_tx, prev_ty, prev_tx, prev_ty);
            if (down) {
                prev_tx = tx;
                prev_ty = ty;
            }
        }
        was_down = down;

        while (acc >= 1.0 / 60.0) {
            int i;

            /* UMK3_CRASH_TEST=<tick>: crash on purpose, to check the
             * crash report and its message box */
            if (getenv("UMK3_CRASH_TEST")
                && ticks == atol(getenv("UMK3_CRASH_TEST")))
                *(volatile int *)(uintptr_t)16 = 1;
            if (g_cfg_debug_keys && getenv("UMK3_DBG_OPEN")
                && ticks == atol(getenv("UMK3_DBG_OPEN")))
                dbg_menu_toggle();      /* a test opens the menu by script */
            if (g_cfg_debug_keys && getenv("UMK3_DBG_BOSS")) {
                /* "tick:24|25" -- the menu's Arcade boss row, once, at the
                 * first round in play from that tick on */
                static int done;
                const char *q = getenv("UMK3_DBG_BOSS");
                if (!done && ticks >= atol(q) && strchr(q, ':')
                    && dbg_round_live()) {
                    struct dbg_request rq = { DBG_BOSS, 0, 0, 0 };
                    rq.a = atoi(strchr(q, ':') + 1);
                    debug_request(&rq);
                    done = 1;
                }
            }
            if (g_cfg_debug_keys && getenv("UMK3_DBG_SPECIAL")) {
                /* "tick:n" -- player one does special move n, the call the
                 * joystick code makes (DoASpecial; 0 is Scorpion's spear) */
                const char *q = getenv("UMK3_DBG_SPECIAL");
                if (CurrentTask == 6 && atol(q) == ticks && strchr(q, ':'))
                    DoASpecial(Plyr, (unsigned int)atoi(strchr(q, ':') + 1));
            }
            if (g_cfg_debug_keys && getenv("UMK3_DBG_FIN")) {
                /* "tick:n;tick:n" -- the menu's FINISHER row (0..6), each
                 * once, at the first FINISH HIM from its tick on (a mercy,
                 * then the animality it allows) */
                static int done;
                const char *q = getenv("UMK3_DBG_FIN");
                int i;

                for (i = 0; q && *q && i < 8; i++) {
                    if (!(done & (1 << i)) && ticks >= atol(q)
                        && strchr(q, ':') && dbg_finishing()
                        && (i == 0 || (done & (1 << (i - 1))))) {
                        struct dbg_request rq = { DBG_FINISHER, 0, 0, 0 };
                        rq.a = atoi(strchr(q, ':') + 1);
                        debug_request(&rq);
                        done |= 1 << i;
                        break;
                    }
                    q = strchr(q, ';');
                    if (q)
                        q++;
                }
            }
            if (g_cfg_debug_keys && getenv("UMK3_DBG_STATE")) {
                /* "tick:s1;tick:l1" -- save / load state slot 1..4 */
                const char *q = getenv("UMK3_DBG_STATE");
                while (q && *q) {
                    const char *c = strchr(q, ':');
                    if (atol(q) == ticks && c && (c[1] == 's' || c[1] == 'l')) {
                        if (c[1] == 's')
                            state_save(atoi(c + 2) - 1);
                        else
                            state_load(atoi(c + 2) - 1);
                    }
                    q = strchr(q, ';');
                    if (q)
                        q++;
                }
            }
            if (g_cfg_debug_keys && getenv("UMK3_DBG_KEY")) {
                /* "tick:k;tick:k" -- F9..F12 (k = 0..3) pressed by script */
                const char *q = getenv("UMK3_DBG_KEY");
                while (q && *q) {
                    if (atol(q) == ticks && strchr(q, ':'))
                        fight_key(atoi(strchr(q, ':') + 1));
                    q = strchr(q, ';');
                    if (q)
                        q++;
                }
            }
            /* a script ignores the pad too, unless UMK3_LOG_PAD asks to
             * watch it */
            if (!g_keys_off || getenv("UMK3_LOG_PAD")) {
                static int was_on = -1;
                static unsigned was_b;

                plat_pad_read(&g_pad);
                if (g_pad.connected != was_on)
                    printf(g_pad.connected ? "pad: %s\n" : "pad: none\n",
                           g_pad.name);
                if (getenv("UMK3_LOG_PAD") && g_pad.buttons != was_b)
                    printf("pad buttons %04x dirs %d%d%d%d\n", g_pad.buttons,
                           g_pad.up, g_pad.down, g_pad.left, g_pad.right);
                was_on = g_pad.connected;
                was_b = g_pad.buttons;
            }
            if (g_cfg_debug_keys) {
                static int was_f2, was_f3;
                /* the pad's combination (pad_dbg_menu=, L3+R3 unless the
                 * launcher set another) opens and closes the menu too */
                int f2 = plat_key(PK_TEST)
                         || (g_pad_dbg_mask
                             && (g_pad.buttons & g_pad_dbg_mask)
                                == g_pad_dbg_mask),
                    f3 = plat_key(PK_BACK);
                if (f2 && !was_f2)
                    dbg_menu_toggle();
                if (f3 && !was_f3 && dbg_keys_on())
                    dbg_info_toggle();
                was_f2 = f2;
                was_f3 = f3;
            }
            if (dbg_menu_is_open()) {
                struct dbg_request rq;

                /* glClear ignores the viewport: this cleared the whole
                 * window and the frame picture with it */
                clear_frame(ww, wh, vx, vy, vw, vh);
                dbg_menu_pad(&g_pad);
                if (dbg_menu_tick(&rq))
                    debug_request(&rq);
                dbg_menu_draw();
                /* drawn into the multisampled buffer: resolve it, or
                 * render_present shows the last frame without the menu */
                render_resolve();
                acc -= 1.0 / 60.0;
                ticks++;                /* the clock UMK3_SHOT counts */
                continue;               /* the game does not tick */
            }

            /* Scripted taps: press on the tick, release three later. */
            for (i = 0; i < g_ntaps; i++) {
                float x = g_taps[i].x * g_ui_k, y = g_taps[i].y * g_ui_k;

                if (g_taps[i].tick == ticks)
                    lime_touch_began(x, y);
                if (g_taps[i].tick + g_taps[i].hold == ticks)
                    lime_touch_ended(x, y, x, y);
            }

            clear_frame(ww, wh, vx, vy, vw, vh);
            hb_update();
            palette_sync();
            lime_menu_advance_clock(1.0 / 60.0);
            keyboard_buttons();
            hud_keys();
            if (g_cfg_debug_keys && dbg_keys_on()) {
                debug_keys();
                screen_keys();
            }
            /* The front end has no stage, so LightPlayers (MaintainLevelScenes,
             * Task_GameMain) never runs there and StaticMeshAmbient, its only
             * writer, stays 0 (__common) until the first fight. A fighter's
             * attachments take their colour from it (LIME_RenderMeshSingle ->
             * CreateFadedRGBS over baked lighting averaging ~15/255), so Kung
             * Lao's hat drew black on the select screen. Give it what
             * LightPlayers would derive from the light RenderFECharacters
             * already sets on the body (+0x5d8: 0.65 0.65 0.7) x 255. A port
             * fix, not in the binary. */
            if (CurrentTask == 3) {
                StaticMeshAmbient[0] = 0.65f * 255.0f;
                StaticMeshAmbient[1] = 0.65f * 255.0f;
                StaticMeshAmbient[2] = 0.7f * 255.0f;
            }
            GameCodeMain();
            /* skip_intro=1 (the launcher's box, or UMK3_SKIP_INTRO): once
             * Task_LoadSplashScreen's first frame has loaded both logos,
             * jump its counter to 491, the frame that deletes them and
             * hands over to the loading screen. The game's own path, only
             * sooner; a port option, not in the binary. */
            if (g_cfg_skip_intro && CurrentTask == 0 && SplashCount > 0
                && SplashCount < 491)
                SplashCount = 491;
            render_resolve();
            if (g_cfg_debug_keys) {
                if (dbg_info_on()) {
                    char line[96];
                    snprintf(line, sizeof line,
                             "TASK %d  SCREEN %d %s  P1 %s  P2 %s  ROUNDS %ld-%ld",
                             CurrentTask, FE_CurrentTask,
                             FE_CurrentTask >= 0 && FE_CurrentTask < DBG_SCREENS
                                 ? FETaskNames[FE_CurrentTask] + 8 : "?",
                             CharacterNames[PLAYER1MODEL % 26],
                             CharacterNames[PLAYER2MODEL % 26],
                             RoundWins[0], RoundWins[1]);
                    dbg_info_draw(line);
                }
                if (g_rs_on)        /* the menu draws in the game's buffer */
                    dbg_menu_after_frame(0, 0, g_rs_w, g_rs_h,
                                         CurrentTask, FE_CurrentTask);
                else
                    dbg_menu_after_frame(vx, vy, vw, vh, CurrentTask, FE_CurrentTask);
            }

            /* --fight: once the main menu is up, do what the select screen
             * and the tower would, and leave the front end. */
            {
                static long in_fe;
                in_fe = (CurrentTask == 3) ? in_fe + 1 : 0;
                if (g_fight_p1 >= 0 && in_fe < 30)
                    goto no_fight_yet;  /* the front end's first frames */
            }
            if (g_fight_p1 >= 0 && CurrentTask == 3) {
                if (!g_dbg_saved.on) {
                    g_dbg_saved.c1 = Character1;
                    g_dbg_saved.c2 = Character2;
                    g_dbg_saved.m1 = PLAYER1MODEL;
                    g_dbg_saved.m2 = PLAYER2MODEL;
                    g_dbg_saved.level = LevelSelect;
                    g_dbg_saved.mode = GameMode;
                    g_dbg_saved.destiny = Destiny;
                    g_dbg_saved.stage = Stage;
                    g_dbg_saved.on = 1;
                    g_dbg_saved.keep = getenv("UMK3_ARCADE") != NULL;
                }
                g_dbg_saved.fought = 0;
                DbgFightActive = 1;
                PLAYER1MODEL = Character1 = g_fight_p1;
                PLAYER2MODEL = Character2 = Character2Override = g_fight_p2;
                LevelSelect = g_fight_stage;
                printf("--fight: %s vs %s, stage %ld\n",
                       CharacterNames[g_fight_p1], CharacterNames[g_fight_p2],
                       g_fight_stage);
                /* UMK3_ARCADE="destiny,stage": the fight as that rung of
                 * an Arcade ladder, so a test reaches the last one */
                if (getenv("UMK3_ARCADE")) {
                    GameMode = 0;
                    Destiny = atol(getenv("UMK3_ARCADE"));
                    if (strchr(getenv("UMK3_ARCADE"), ','))
                        Stage = atoi(strchr(getenv("UMK3_ARCADE"), ',') + 1);
                }
                CurrentTask = 4;                /* Task_FEDestroy */
                g_fight_p1 = -1;
                g_fight_override = 1;
            }
            /* Character2Override is what makes Task_GameInit take our player
             * two; only the VS screen clears it (FrontEnd.c, -1 on its
             * entry), so a debug fight's opponent stayed for every Arcade
             * fight after it. Task_GameInit has copied it once the fight is
             * up: clear it then, so nothing of the debug fight remains. */
            if (g_fight_override && CurrentTask == 6) {
                Character2Override = -1;
                g_fight_override = 0;
            }
            if (g_dbg_saved.on && CurrentTask == 6)
                g_dbg_saved.fought = 1;
            if (g_dbg_saved.on && g_dbg_saved.fought && CurrentTask == 3
                && g_fight_p1 < 0) {
                if (!g_dbg_saved.keep) {
                    Character1 = g_dbg_saved.c1;
                    Character2 = g_dbg_saved.c2;
                    PLAYER1MODEL = g_dbg_saved.m1;
                    PLAYER2MODEL = g_dbg_saved.m2;
                    LevelSelect = g_dbg_saved.level;
                    GameMode = g_dbg_saved.mode;
                    Destiny = g_dbg_saved.destiny;
                    Stage = g_dbg_saved.stage;
                    printf("debug: fight over, %s vs %s and the mode put back\n",
                           CharacterNames[Character1 % 26],
                           CharacterNames[Character2 % 26]);
                }
                g_dbg_saved.on = 0;
                DbgFightActive = 0;
                DbgPalette[0] = DbgPalette[1] = 0;      /* back to AUTO */
            }
        no_fight_yet:
            if (getenv("UMK3_SCREENS")) {
                const char *q = getenv("UMK3_SCREENS");
                while (q && *q) {
                    if (atol(q) == ticks && strchr(q, ':'))
                        jump_screen(parse_screen(strchr(q, ':') + 1));
                    q = strchr(q, ';');
                    if (q)
                        q++;
                }
            }
            {
                static long in_menu;
                in_menu = (CurrentTask == 3) ? in_menu + 1 : 0;
                if (g_screen_jump >= 0 && in_menu > 30 && PendingPush == -1) {
                    jump_screen(g_screen_jump);
                    g_screen_jump = -1;
                }
            }

            /* UMK3_LOG_P1=1: in a fight, every change of player one's
             * frame (Players +0x14) or of the engine's button word
             * (G +0x1c, TranslateJoybits' output), for input tests. */
            if (getenv("UMK3_LOG_P1") && CurrentTask == 6 && G) {
                extern char Players[];
                static long last_fr = -1, last_bits = -1;
                long fr = *(long *)(Players + 0x14);
                long bits = *(long *)((char *)G + 0x1c);
                extern long ButtonStates[7], JoystickState;
                if (fr != last_fr || bits != last_bits)
                    printf("tick %ld: p1 frame %ld bits %06lx btn %ld%ld%ld%ld%ld%ld%ld dial %ld keys %ld\n",
                           ticks, fr, bits, ButtonStates[0], ButtonStates[1],
                           ButtonStates[2], ButtonStates[3], ButtonStates[4],
                           ButtonStates[5], ButtonStates[6], JoystickState,
                           Player1NumButtons);
                last_fr = fr;
                last_bits = bits;
            }
            {
                /* Task_LoadGeneralData (task 1, once at boot) loads the
                 * saved settings again, after start_app, and put the saved
                 * layout back over the launcher's: buttons=6 played with
                 * five. Apply it again the tick the loading screen after
                 * it (task 8, boot order 0, 1, 8, 2) hands over. */
                static int was8;
                if (was8 && CurrentTask != 8
                    && (g_cfg_buttons == 5 || g_cfg_buttons == 6))
                    Settings[4] = g_cfg_buttons;
                was8 = CurrentTask == 8;
            }
            /* UMK3_LOG_FADE=1: every start of a screen fade, for finding
             * who starts one. FE_FadeAdd < 0 fades out, > 0 back in. */
            if (getenv("UMK3_LOG_FADE")) {
                static float last_add;
                static long  last_dq = -1;
                if (FE_FadeAdd != last_add || DontQuitAfterFade != last_dq)
                    printf("tick %ld: fade add %+.4f fade %.3f dontquit %ld task %d round summary %ld\n",
                           ticks, FE_FadeAdd, FE_Fade, DontQuitAfterFade,
                           CurrentTask, RoundSummary);
                last_add = FE_FadeAdd;
                last_dq = DontQuitAfterFade;
            }
            if (log_tasks && (CurrentTask != last_task
                              || FE_CurrentTask != last_fe)) {
                printf("tick %ld: task %d, front-end task %d\n",
                       ticks, CurrentTask, FE_CurrentTask);
                last_task = CurrentTask;
                last_fe = FE_CurrentTask;
            }
            acc -= 1.0 / 60.0;
            ticks++;
            g_tick = ticks;
        }
        plat_input_used();      /* the taps this frame's ticks have seen */
        plat_audio_update();
        render_present();

        if (getenv("UMK3_SHOTS")) {
            static long done_upto = -1;
            const char *q = getenv("UMK3_SHOTS");
            while (q && *q) {
                long t = atol(q);
                if (t <= ticks && t > done_upto) {
                    char nm[40];
                    snprintf(nm, sizeof nm, "umk3-shot-%ld.ppm", t);
                    save_shot_as(nm, ww, wh);
                    done_upto = t;
                }
                q = strchr(q, ',');
                if (q)
                    q++;
            }
        }
        if (shot_at > 0 && ticks >= shot_at) {
            save_shot(ww, wh);
            break;
        }
        pace_work_end();
        if (!plat_swap())
            break;
        pace_frame();
    }

    plat_close();
    return 0;
}
