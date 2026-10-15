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

/* The debug menu drawn as an iPhone OS 3 alert box (title, rows of label and
 * value, the selected row in blue, greyed rows, a footer), at `s` pixels per
 * point of the 480x320 screen: RGBA8 premultiplied, *w x *h, malloc'd. NULL
 * where the backend cannot draw it; the menu then uses its own font. */
unsigned char *plat_ui_menu(const char *title, const char *const *label,
                            const char *const *value, const int *enabled,
                            int n, int sel, const char *footer, float s,
                            int *w, int *h);

/* The user's interface language as an ISO 639-1 code ("es", "en", ...) in
 * `out`, at least 3 bytes; "" when it cannot be told. The iPhone's
 * [NSLocale preferredLanguages][0]. */
void plat_language(char *out, int n);

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
    PK_COUNT
};

/* Is that key down right now? */
int plat_key(int code);

/* The key a code is bound to, as the platform names it ("F2", "Q"), in
 * `out` of `n` bytes, upper case. */
void plat_key_label(int code, char *out, int n);

/* Rebind a code to a platform key (a Windows virtual-key code on win32; the
 * SDL backend takes an SDL scancode). umk3.ini's key_* lines, written by the
 * launcher, come through here. Out-of-range codes are ignored. */
void plat_bind_key(int code, int key);

/* The first attached gamepad, as the same ten bits the engine wants, or -1
 * when there is none. Bit order is the engine's: 0..3 directions, then HP, LP,
 * BL, HK, LK, RUN. */
int plat_pad(int which);


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

/* Reclaim finished one-shot sound buffers. Call once a frame. */
void plat_audio_update(void);

/* Background music, straight from a file. SDL backends stream the game's MP3
 * tunes with SDL2_mixer; Win32 uses MCI. */
void plat_music_play(const char *path, int loop);
void plat_music_stop(void);

/* 0..1, applied to the track playing now and to the next one. */
void plat_music_volume(float gain);

#endif
