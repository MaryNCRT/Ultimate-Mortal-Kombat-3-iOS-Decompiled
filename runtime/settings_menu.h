/* settings_menu.h -- the in-game settings menu (Esc); see settings_menu.c.
 * Port, not in the binary. Include platform/platform.h first. */
#ifndef UMK3_SETTINGS_MENU_H
#define UMK3_SETTINGS_MENU_H

int  set_menu_is_open(void);
void set_menu_toggle(void);
/* After GameCodeMain: a pending open grabs the frame just drawn. */
void set_menu_after_frame(int vx, int vy, int vw, int vh);
/* Each frame the menu is open, instead of the game's tick: the pointer as
 * fractions of the game view, both pads, then tick and draw. */
void set_menu_mouse(float x, float y, int down);
void set_menu_pads(const PLAT_PAD *p1, const PLAT_PAD *p2);
void set_menu_tick(void);
void set_menu_draw(void);
/* the player's badge (name and icon), bottom right of the main menu */
void set_menu_badge(void);
/* How the menu reads a key; game_main.c points it at its own reader so a
 * scripted test (UMK3_KEYS) reaches the menu. */
extern int (*set_menu_key)(int code);

/* What game_main.c gives the menu (game_main.c). */
struct port_gfx {
    int  mode;                  /* 0 window, 1 borderless, 2 fullscreen */
    int  w, h;                  /* width=, height= */
    int  rw, rh;                /* render_width=, render_height=; 0 none */
    int  aa;                    /* antialiasing=: 0, 2, 4, 8, 16 */
    int  texq;                  /* texture_quality=: 0 iphone 1 ipad 2 max */
    int  ui;                    /* ui_size=: 0 iphone, 1 ipad */
    int  hidectl;               /* hide_controls= */
    char aspect[8];             /* aspect=: 3:2 4:3 16:9 21:9 */
    int  monitor;               /* monitor=: 0 the window's, 1..n */
};
void port_gfx_get(struct port_gfx *g);
/* Applies it now; `display` also the window mode and size. */
void port_gfx_set(const struct port_gfx *g, int display);
/* Player 0 or 1's pad buttons for an action, a mask of 1 << PB_*:
 * 0..6 the six-button HP LP BL HK LK RN S, 7..13 the five-button P - B K -
 * R S, 14 pause, 15 the moves list. */
unsigned *port_pad_action(int player, int i);
/* pad= / p2pad=: auto, off, xinput or vvvv:pppp; set it with
 * port_pad_device_set. */
const char *port_pad_device(int player);
void port_pad_device_set(int player, const char *v);
/* buttons=: 5 or 6, the layout the game plays with */
int  port_buttons(void);
void port_buttons_set(int n);
/* umk3.ini's full path, "" when there is none */
const char *port_ini_path(void);

#endif
