/*
 * ui_alert.h -- what ui_alert.c (the in-game alerts and menu boxes in the
 * iPhone OS 3 dress, shared by every backend) needs from the backend: a font
 * and a few raw keys. Port, not in the binary. Only the backends and
 * ui_alert.c include it.
 */
#ifndef UMK3_UI_ALERT_H
#define UMK3_UI_ALERT_H

/* Whether the backend has a font at all; without one plat_ui_* return NULL
 * (the menus' own 5x7 font) and plat_ask asks with ui_ask_native. */
int ui_font_ok(void);
/* The font's line height at `px` pixels to the em. */
int ui_font_height(int px, int bold);
/* The width of n UTF-16 characters in pixels. */
int ui_font_width(const unsigned short *s, int n, int px, int bold);
/* n UTF-16 characters as an 8-bit coverage mask, white on black, *w (at
 * least 1) x *h (the line height), malloc'd; NULL without a font. */
unsigned char *ui_font_line(const unsigned short *s, int n, int px, int bold,
                            int *w, int *h);

/* The keys an alert answers to, held now: Enter or Space, Esc, the
 * arrows. */
enum { UI_K_OK, UI_K_CANCEL, UI_K_LEFT, UI_K_RIGHT };
int  ui_key(int k);
int  ui_focused(void);                 /* the window has the focus */
void ui_sleep_ms(int ms);
/* The system's own dialog, for a backend with no font. */
int  ui_ask_native(const unsigned short *msg, const unsigned short *ok,
                   const unsigned short *cancel);

/* Whether "aditional textures/glyphs/<name>.png" is there (plat_key_glyph). */
int  ui_glyph_have(const char *name);

#endif
