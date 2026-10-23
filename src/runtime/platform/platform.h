/*
 * The platform layer's entire interface.
 *
 * Everything iOS-specific in the original lives behind this. The point of
 * keeping it this small is that porting to another OS means writing one file,
 * not auditing the engine.
 *
 * Backends:
 *   win32_gl.c   Win32 + WGL. No dependencies, builds with the toolchain that
 *                is already here. This is what the vertical slice uses.
 *   sdl_gl.c     SDL2. The portable path, and the reason the interface is
 *                shaped this way rather than exposing HWND. Default off
 *                Windows; -DUMK3_BACKEND=sdl2 selects it on Windows too.
 */
#ifndef LIME_PLATFORM_H
#define LIME_PLATFORM_H

#include <stdbool.h>

/* Open a window with a GL context current on it. Returns false on failure. */
bool plat_open(const char *title, int width, int height);

/* Cover the whole monitor the window is on, without a border, at the
 * desktop's own mode: the launcher's "ventana sin bordes" (borderless=1). */
void plat_fullscreen(void);

/* Set before plat_open: the window is created borderless over the whole
 * monitor instead of as a window that plat_fullscreen restyles later. */
extern int plat_open_borderless;

/* Exclusive fullscreen: the monitor switched to the window's size. Falls
 * back to plat_fullscreen when the monitor has no such mode. The launcher's
 * "pantalla completa" (fullscreen=1). */
void plat_fullscreen_exclusive(void);

/* The settings menu (port): the window as it is now, 0 a window whose
 * inside is w x h centred on the monitor, 1 borderless over the monitor,
 * 2 exclusive fullscreen at w x h. And leaving the game. */
void plat_display(int mode, int w, int h);

/* The monitor the game goes to (umk3.ini monitor=, port): 0 the one the
 * window is on (the main one at the start), 1..n one of them in Windows'
 * order. Set before plat_open, or before plat_display to move there. */
void plat_monitor_select(int i);
int  plat_monitor(void);
/* How many monitors, and one's name for the menus ("2: 2560x1440"), with
 * " *" after the main one. */
int  plat_monitor_count(void);
void plat_monitor_name(int i, char *out, int n);
/* Print them for the launcher, `umk3-game --list-monitors`: one
 * "index<TAB>name" a line. */
void plat_monitor_list(void);
void plat_quit(void);

/* Typed text, for a player's name (settings_menu.c, port): the next
 * character typed as UTF-8 in `out` (n >= 5 bytes), its length, 0 for none;
 * Backspace, Enter and Esc come as 8, 13 and 27. plat_text_clear drops
 * what was typed before the field opened. */
int  plat_text_take(char *out, int n);
void plat_text_clear(void);
/* The clipboard's text as UTF-8 in `out` (port: Ctrl+V on the ONLINE
 * screen pastes a code); its length, 0 for none. */
int  plat_clipboard_text(char *out, int n);
/* `utf8` onto the clipboard (port: COPY beside YOUR CODE); 1 when done. */
int  plat_clipboard_set(const char *utf8);

/* A GL entry point beyond 1.1 (the framebuffer objects the render
 * resolution draws into), or NULL. Same calling convention as GL's own. */
void *plat_gl_proc(const char *name);

/* Called with 1 as plat_ask starts and 0 as it returns, so the game can put
 * its frame on the window first and go back to where it draws after. */
extern void (*plat_ask_hook)(int begin);

/* Pump the OS event queue. Returns false once the user has asked to quit. */
bool plat_poll(void);

/* Present the back buffer. */
bool plat_swap(void);

/* The game has ticked with the keys as they are: forget the taps it saw
 * (a key down and up again between two ticks still counts as one press). */
void plat_input_used(void);

/* The screen's refresh rate in Hz, 0 if unknown. */
int plat_refresh_hz(void);

/* Sleep until plat_time() reaches t. */
void plat_wait_until(double t);

void plat_close(void);

/* Current drawable size in pixels -- not the same as the requested size once
 * the user resizes or the OS applies scaling. */
void plat_size(int *width, int *height);

/* Pointer position in client pixels, and whether a button is down. The engine
 * has no mouse -- it reads a touch position -- so the caller maps one onto the
 * other. */
int plat_mouse(int *x, int *y);

/* The wheel's notches since the last call: up positive, down negative
 * (port: the in-game menus scroll with it). */
int plat_mouse_wheel(void);

/* Seconds since plat_open, monotonic. */
double plat_time(void);

/* Whether the window has the keyboard focus -- the PC's "app is active". The
 * iOS delegate's WillResignActive / DidBecomeActive pair is driven from the
 * edges of this (runtime/lime_app.c). */
bool plat_focused(void);

/* A blocking question, the PC's UIAlertView. `msg`, `ok` and `cancel` are the
 * game's own UTF-16 strings (GameTextNoHeader); `cancel` NULL means a single
 * button. Returns the index of the button pressed: 0 for `ok`, 1 for
 * `cancel` -- the index -[modalAlertDelegate alertView:clickedButtonAtIndex:]
 * records. */
int plat_ask(const unsigned short *msg, const unsigned short *ok,
             const unsigned short *cancel);

/* The debug menu drawn as an iPhone OS 3 alert box (title, a strip of
 * `ntab` tabs with `tab` current, rows of label and value, the selected row
 * in blue, greyed rows, a footer), at `s` pixels per point of the menu's
 * 320-high space: RGBA8 premultiplied, *w x *h, malloc'd. NULL where the
 * backend cannot draw it; the menu then uses its own font. */
unsigned char *plat_ui_menu(const char *title, const char *const *tabs,
                            int ntab, int tab, const char *const *label,
                            const char *const *value, const int *enabled,
                            int n, int sel, const char *footer, float width,
                            int split, float s, int *w, int *h);
/* plat_ui_menu with ntab < 0 (the settings menu, port): the -ntab tabs go
 * down a side panel UI_SIDE points wide on the left instead of across the
 * top, the rows beside it; `width` is the whole box's, in points (360 with
 * the tabs on top). A label starting "\002" is a section's heading.
 * Two columns side by side when 0 < split < n: rows 0..split-1 down the
 * left, the rest down the right; a row too long for its column is drawn
 * smaller. */
#define UI_SIDE 96

/* A grid of pictures in the same dress (the settings menu's icons and
 * colours, port): the title, `n` cells of cpx x cpx straight RGBA, `cols`
 * across, the chosen one on the selection blue, the footer. In points: a 6
 * margin, the title band 22 and 4 more, 10 inside at the sides, cells
 * `cell` big every cell + 4, 26 for the footer. NULL where the backend has
 * no such drawing. */
unsigned char *plat_ui_grid(const char *title,
                            const unsigned char *const *cells, int n,
                            int cols, int cpx, float cell, int sel,
                            const char *footer, float s, int *w, int *h);

/* The user's interface language as an ISO 639-1 code ("es", "en", ...) in
 * `out`, at least 3 bytes; "" when it cannot be told. The iPhone's
 * [NSLocale preferredLanguages][0]. */
void plat_language(char *out, int n);

/* The player's badge (port): an icon (icon_px square, straight RGBA) and
 * a name in a small black box of the same dress; NULL where there is no
 * such drawing. */
unsigned char *plat_ui_badge(const char *name, const unsigned char *icon,
                             int icon_px, float s, int *w, int *h);

/* PROFILE's gradient picker as the launcher draws it (port; ui_alert.c,
 * plat_ui_gradient): the colours, the angle (0 up, clockwise), `empty` for
 * no background, the presets, which one is the profile's (`active`, -1
 * none), the control lit for the keys (`focus`: 0 the dial, 1 FROM, 2 TO,
 * 3 the presets at `cursor`, 4 COPY, 5 CLEAR, -1 none) and the words. `k` is
 * texture pixels per CSS pixel of the launcher's component. `hit` gets the
 * controls' boxes in texture pixels. */
#define PLAT_GRAD_PRESETS 18
struct plat_grad {
    unsigned from, to;
    int angle, empty, active, focus, cursor, npre;
    unsigned pfrom[PLAT_GRAD_PRESETS], pto[PLAT_GRAD_PRESETS];
    int pangle[PLAT_GRAD_PRESETS];
    const char *label, *none, *lfrom, *lto, *copy, *clear, *css;
};
struct plat_grad_hit {
    float panel[4], dial[3], field[2][4], swatch[PLAT_GRAD_PRESETS][4], button[2][4];
};
unsigned char *plat_ui_gradient(const struct plat_grad *g, float k,
                                struct plat_grad_hit *hit, int *w, int *h);

/* ------------------------------------------------------------------ input
 *
 * The fight engine takes ONE TEN-BIT WORD PER PLAYER and nothing else -- see
 * "THE INPUT CONTRACT" at the top of decomp/gamecode/logic/joy.c, where the
 * six buttons are named from three independent measurements. These two calls
 * are how a keyboard and a gamepad produce that word.
 *
 * `plat_key` takes a platform-independent code from the list below. The engine
 * never sees these; the caller maps them onto the ten bits.
 */
enum {
    PK_UP, PK_DOWN, PK_LEFT, PK_RIGHT,
    PK_HP, PK_LP, PK_BL, PK_HK, PK_LK, PK_RUN,
    PK_P2_UP, PK_P2_DOWN, PK_P2_LEFT, PK_P2_RIGHT,
    PK_P2_HP, PK_P2_LP, PK_P2_BL, PK_P2_HK, PK_P2_LK, PK_P2_RUN,
    PK_RESET,
    /* the debug selector's own keys, kept apart from the fight's */
    PK_MENU, PK_OK, PK_NEXT, PK_PREV, PK_BACK, PK_TEST,
    /* fight debug keys (umk3.ini debug_keys=1): end the round or the match */
    PK_DBG_KO_P2, PK_DBG_KO_P1, PK_DBG_WIN, PK_DBG_LOSE,
    /* the HUD's two corner buttons: the pause menu and the moves list */
    PK_PAUSE, PK_MOVES,
    /* debug mode, front end: previous / next screen, back to the main menu */
    PK_DBG_SCR_PREV, PK_DBG_SCR_NEXT, PK_DBG_SCR_MENU,
    /* the special button (index 6): the S of the five-button layout */
    PK_SPECIAL, PK_P2_SPECIAL,
    /* player one's keys for the five-button layout: P B K R (S is
     * PK_SPECIAL); the six-button layout uses PK_HP .. PK_RUN */
    PK_5_P, PK_5_B, PK_5_K, PK_5_R,
    /* debug menu: previous / next page */
    PK_DBG_PAGE_PREV, PK_DBG_PAGE_NEXT,
    /* player two's keys for the five-button layout: P B K R (S is
     * PK_P2_SPECIAL) */
    PK_P2_5_P, PK_P2_5_B, PK_P2_5_K, PK_P2_5_R,
    /* the settings menu (Esc; settings_menu.c, port) */
    PK_SETTINGS,
    /* player two's pause menu, moves list and ACCEPT on the menus, as
     * player one's P, M and Enter (port) */
    PK_P2_PAUSE, PK_P2_MOVES, PK_P2_OK,
    PK_COUNT
};

/* Is that key down right now? */
int plat_key(int code);

/* The key a code is bound to, as the platform names it ("F2", "Q"), in
 * `out` of `n` bytes, upper case. */
void plat_key_label(int code, char *out, int n);

/* The same, as a picture of the key when there is one: "\001kb/<file>\001",
 * which plat_ui_menu draws from "aditional textures/glyphs/" beside res
 * (Julio Cacko's Free Input Prompts, CC0); else plat_key_label's name.
 * Port only. */
void plat_key_glyph(int code, char *out, int n);

/* Rebind a code to a platform key (a Windows virtual-key code on win32; the
 * SDL backend takes an SDL scancode). umk3.ini's key_* lines, written by the
 * launcher, come through here. Out-of-range codes are ignored. */
void plat_bind_key(int code, int key);

/* The platform key a code is bound to now (what plat_bind_key takes), and
 * the first platform key held down right now, 0 for none: the settings
 * menu's rebinding (settings_menu.c, port). */
int plat_key_bound(int code);
int plat_key_any(void);
/* Backspace held: BACK on the game's menus from the keyboard (pad_nav.c,
 * port). */
int plat_key_backspace(void);

/* The first attached gamepad, as the same ten bits the engine wants, or -1
 * when there is none. Bit order is the engine's: 0..3 directions, then HP, LP,
 * BL, HK, LK, RUN. */
int plat_pad(int which);

/* The pad player one plays with, in one layout whatever the make: the face
 * buttons by POSITION (south = Xbox A / PlayStation cross / Switch B), the
 * shoulders, the triggers, select (Back / Share / Create / minus), start
 * (Start / Options / plus), the stick clicks and the home button. The d-pad
 * and the left stick both give the four directions. Port only. */
enum {
    PB_SOUTH, PB_EAST, PB_WEST, PB_NORTH,
    PB_L1, PB_R1, PB_L2, PB_R2,
    PB_SELECT, PB_START, PB_L3, PB_R3, PB_HOME,
    PB_COUNT
};

typedef struct {
    int      connected;
    int      up, down, left, right;
    unsigned buttons;                   /* 1 << PB_* */
    int      rx;                        /* the right stick across: -1 0 1 */
    char     name[64];                  /* as the system names the pad */
    int      kind;                      /* 1 Xbox (XInput), 2 PlayStation 5,
                                           3 Nintendo, 4 PlayStation 4,
                                           0 other */
} PLAT_PAD;

/* A pad button (PB_*) as a picture for plat_ui_menu: the Xbox, PlayStation
 * or Nintendo one by `kind` (PLAT_PAD's; 0 draws the Xbox ones), or its
 * name when there is none. Port only. */
void plat_pad_glyph(int kind, int pb, char *out, int n);

/* Which pictures a player's pad shows whatever the pad: 0 the pad's own
 * (auto), 1 Xbox, 2 PlayStation 5, 3 Nintendo, 4 PlayStation 4 -- umk3.ini
 * pad_icons= and p2pad_icons= (auto xbox ps5 nintendo ps4). A negative `style` only asks. Returns the style in force.
 * Port only. */
int plat_pad_glyph_style(int player, int style);

/* The kind plat_pad_glyph should draw for a player whose pad is `kind`. */
int plat_pad_glyph_kind(int player, int kind);

/* A Sony pad's kind by its USB product: 4 the PlayStation 4's, else 2. */
int plat_pad_sony_kind(unsigned product);

/* Which pad: "auto" (the first one found, XInput first), "off", "xinput"
 * (an Xbox pad or anything shown as one), or "vvvv:pppp", the USB vendor
 * and product in hex, for one particular pad (PlayStation, Switch, any
 * DirectInput pad). umk3.ini's pad= line, written by the launcher. */
void plat_pad_select(const char *which);

/* Player one's pad right now; 0 (and out->connected 0) when there is none. */
int plat_pad_read(PLAT_PAD *out);

/* Player two's pad, the same way (umk3.ini p2pad=). Never the pad player
 * one is using. Port only; the SDL backend has none. */
void plat_pad2_select(const char *which);
int plat_pad2_read(PLAT_PAD *out);

/* Print every pad attached now, one line each: the pad= value, a tab, the
 * name the system gives it. For `umk3-game --list-pads`, which the launcher
 * runs to fill its list. */
void plat_pad_list(void);


/* ------------------------------------------------------------------ audio
 *
 * The game's sounds are plain RIFF/WAVE: PCM, mono, 8-bit unsigned, 16 kHz.
 * 497 of them sit in `res/audio`, and which ones belong together is the sound
 * group table `tools/sounds.py` recovers from the binary.
 *
 * The mixer is deliberately small: one output stream, a fixed number of
 * one-shot voices, no streaming and no 3D. A fighting game plays short
 * overlapping samples and nothing else, and anything more would be a
 * capability this port has no use for yet.
 *
 * Every call is safe to make when the audio device failed to open -- it
 * silently does nothing. A machine with no sound card still plays the game.
 */

/* Open audio output; `rate` is the default source rate for sound effects. */
int  plat_audio_open(int rate);
void plat_audio_close(void);

/* Start one sound. `pcm` is unsigned 8-bit mono at the rate `plat_audio_open`
 * was given, and the CALLER OWNS IT -- it must stay alive until the sound has
 * finished, which for a loaded .wav held for the life of the program it does.
 *
 * Returns 0 when every voice is busy. That is not an error: dropping the
 * quietest new sound is what a fixed voice count means, and the engine's own
 * event queue drops its eleventh event the same way. */
int  plat_audio_play(const unsigned char *pcm, int frames, float gain);

/* The same, for a source whose rate is NOT the device's. The game's assets are
 * not uniform -- 496 of its 497 .wav files are 16 kHz and one is 44,100 -- so
 * a caller that loaded a file should pass what the file actually said rather
 * than assume. */
int  plat_audio_play_at(const unsigned char *pcm, int frames, int rate,
                        float gain);

/* Stop every voice still playing from `pcm`, before the caller frees it. */
void plat_audio_stop_pcm(const unsigned char *pcm);
/* Every voice off (a save state was loaded). */
void plat_audio_stop_all(void);

/* Reclaim finished one-shot sound buffers. Call once a frame. */
void plat_audio_update(void);

/* Background music, straight from a file. SDL backends stream the game's MP3
 * tunes with SDL2_mixer; Win32 uses MCI. */
void plat_music_play(const char *path, int loop);
void plat_music_stop(void);

/* 0..1, applied to the track playing now and to the next one. */
void plat_music_volume(float gain);

#endif
