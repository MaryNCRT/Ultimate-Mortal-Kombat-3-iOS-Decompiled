/* pad_nav.h -- the game's own menus with a pad; see pad_nav.c. Port, not in
 * the binary. */
#ifndef UMK3_PAD_NAV_H
#define UMK3_PAD_NAV_H

#include "platform/platform.h"

#define NAV_BUTTON 0
#define NAV_BACK   1                    /* the screen's BACK: the east button */
#define NAV_TAB    2                    /* the main menu beside a page */
#define NAV_AREA   3                    /* a touch area round a picture */
#define NAV_FRAME  4                    /* nothing on it lights: framed */
#define NAV_TABBACK 5                   /* a page's own tab: east closes it */
#define NAV_OK     6                    /* a screen's OK or PLAY: south's */

/* A button the screen tests the touch against, in the touch's units (the
 * surface's pixels, as limeTouchScreenX). FrontEnd.c calls it. */
void PortNavTarget(float x, float y, float w, float h, int kind);
/* This frame both players choose at once (the two-player select screen). */
void PortNavDual(void);

/* The keyboard's part: a direction held (1 up 2 down 3 left 4 right, 0
 * none), Enter, Backspace. */
struct nav_keys { int dir, ok, back; };

/* Around the frame: `on` when the pad drives this screen. pad_nav_end reads
 * the pads and the keys (k1 player one's, k2 player two's), moves the
 * highlights and returns 1 with a tap's place and its player (1, 2) when
 * one is due. */
void pad_nav_begin(int on);
int  pad_nav_end(const PLAT_PAD *p1, const PLAT_PAD *p2,
                 const struct nav_keys *k1, const struct nav_keys *k2,
                 float *tx, float *ty, int *who);
/* South / Enter or east / Backspace still held by player `who`: the finger
 * stays down until they are let go, and the button answers then, as it
 * does to a finger. */
int  pad_nav_held(int who);
/* Where the finger hovers: player one's highlighted button, held there
 * without a press or a release, so it lights the way a finger resting on
 * it does. 0 when nothing is highlighted. */
int  pad_nav_hover(float *x, float *y);
/* Player `who`'s highlighted button, to frame; 0 when none shows or it
 * lights by itself (its text goes yellow under the resting finger). */
int  pad_nav_focus(int who, float *x, float *y, float *w, float *h);
/* The screen's BACK (and its OK or PLAY), 0 when it has none. */
int  pad_nav_back(float *x, float *y, float *w, float *h);
int  pad_nav_ok(float *x, float *y, float *w, float *h);
/* This screen has buttons for the pad. */
int  pad_nav_on(void);
/* What player one used last: 1 a pad, 2 the keyboard. */
int  pad_nav_device(void);
/* A click: the highlight hides until the pad is used again. */
void pad_nav_hide(void);

#endif
