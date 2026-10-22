/*
 * ui_alert.c -- the in-game alerts and the menus' boxes in the iPhone OS 3
 * dress, the same on every backend (win32_gl.c, sdl_gl.c), which give it a
 * font and the raw keys (ui_alert.h). Port, not in the binary: on the device
 * UIKit draws them. Also the pads' button pictures (plat_pad_glyph), read
 * from "aditional textures/glyphs/".
 */

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform.h"
#include "gl.h"
#include "ui_alert.h"
#include "lime/png.h"

#ifndef GL_CLAMP
#define GL_CLAMP 0x2900
#endif

/* ------------------------------------------------------------- pad glyphs */

static int g_glyph_style[2];

int plat_pad_sony_kind(unsigned product)
{
    /* the DualShock 4 (both models) and its USB adaptor */
    return product == 0x05c4 || product == 0x09cc || product == 0x0ba0 ? 4 : 2;
}

int plat_pad_glyph_style(int player, int style)
{
    player = player ? 1 : 0;
    if (style >= 0 && style <= 4)
        g_glyph_style[player] = style;
    return g_glyph_style[player];
}

int plat_pad_glyph_kind(int player, int kind)
{
    int st = g_glyph_style[player ? 1 : 0];
    return st ? st : kind;
}

/* Free Input Prompts' Retro pads (aditional textures/glyphs/xb, ps, ns). */
void plat_pad_glyph(int kind, int pb, char *out, int n)
{
    static const char *const xb[PB_COUNT] = {
        "T_X_A_Color_Retro", "T_X_B_Color_Retro", "T_X_X_Color_Retro",
        "T_X_Y_Color_Retro", "T_X_LB_Retro", "T_X_RB_Retro", "T_X_LT_Retro",
        "T_X_RT_Retro", "T_X_Share_Retro", "T_X_X_Retro",
        "T_X_Left_Stick_Click_Retro",
        "T_X_Right_Stick_Click_Retro", NULL,
    };
    static const char *const ps[PB_COUNT] = {
        "T_P5_Cross_Color_Retro", "T_P5_Circle_Color_Retro",
        "T_P5_Square_Color_Retro", "T_P5_Triangle_Color_Retro", "T_P5_L1_Retro",
        "T_P5_R1_Retro", "T_P5_L2_Retro", "T_P5_R2_Retro", "T_P5_Share_Retro",
        "T_P5_Options_Retro", "T_P5_L3_Retro", "T_P5_R3_Retro", NULL,
    };
    /* by position: the south button of a Nintendo pad is its B (the
     * Default set, so they are not taken for the Xbox letters) */
    static const char *const ns[PB_COUNT] = {
        "T_S_B", "T_S_A", "T_S_Y", "T_S_X", "T_S_LB", "T_S_RB", "T_S_LT",
        "T_S_RT", "T_S_Minus", "T_S_Plus", "T_S_L", "T_S_R", "T_S_Home",
    };
    static const char *const p4[PB_COUNT] = {
        "T_P4_Cross_Color_Stylized", "T_P4_Circle_Color_Stylized",
        "T_P4_Square_Color_Stylized", "T_P4_Triangle_Color_Stylized",
        "T_P4_L1_Stylized", "T_P4_R1_Stylized", "T_P4_L2_Stylized",
        "T_P4_R2_Stylized", "T_P4_Share_Stylized", "T_P4_Options_Stylized",
        "T_P4_L3_Stylized", "T_P4_R3_Stylized", NULL,
    };
    static const char *const *const sets[5] = { xb, xb, ps, ns, p4 };
    static const char *const dir[5] = { "xb", "xb", "ps", "ns", "p4" };
    static const char *const names[PB_COUNT] = {
        "SOUTH", "EAST", "WEST", "NORTH", "L1", "R1", "L2", "R2",
        "SELECT", "START", "L3", "R3", "HOME",
    };
    char name[64];
    const char *f;

    if (n <= 0)
        return;
    if (pb < 0 || pb >= PB_COUNT) {
        out[0] = 0;
        return;
    }
    if (kind < 0 || kind > 4)
        kind = 0;
    f = sets[kind][pb];
    if (f) {
        snprintf(name, sizeof name, "%s/%s", dir[kind], f);
        if (ui_glyph_have(name)) {
            snprintf(out, (size_t)n, "\001%s\001", name);
            return;
        }
    }
    snprintf(out, (size_t)n, "%s", (kind == 2 || kind == 4) && pb == PB_START ? "OPTIONS"
                                   : kind <= 1 && pb == PB_START ? "MENU" : names[pb]);
}

/* ------------------------------------------------------------------ alerts
 *
 * The PC's UIAlertView, drawn inside the game window. On the device
 * +[modalAlert askFull:textOK:textCANCEL:] / infoFull:textOK: (0xb5444 /
 * 0xb541c) build one UIAlertView -- initWithTitle: the whole text (both
 * GameText lines joined by "\n"), message: nil, the OK text as the
 * cancelButtonTitle (index 0, on the left) and CANCEL as the other button
 * (index 1, on the right) -- and spin a run loop until a button is pressed.
 * The game is frozen meanwhile, so it is here too: this loop owns the window
 * until the answer is in.
 *
 * The look is iPhone OS 3's: the screen dimmed, a translucent dark-blue box
 * with a light border and a gloss across the top, bold white centred text,
 * and glossy buttons. It is drawn in software into one texture (the shapes
 * anti-aliased by their distance to the edge, the text by the backend's font,
 * ui_font_line: GDI's Arial on Windows, a TrueType font through stb_truetype
 * on Linux, so the game's accented UTF-16 shows as it is), over a copy of the
 * frame the game had drawn when it asked. Mouse: press and release on a
 * button. Keyboard: Enter is the left button (OK), Esc the right one
 * (CANCEL), or OK when there is only one. The pad (port): left / right
 * light a button, the south button (cross / A) or START presses the lit one
 * (OK when none is), the east button (circle / B) is CANCEL; the arrow keys
 * light one too, and Enter then presses it. */

typedef struct { float r, g, b, a; } al_px;     /* premultiplied */

typedef struct {
    int    w, h;            /* the box texture, pixels (box plus shadow margin) */
    al_px *px;
    float  s;               /* pixels per iPhone point */
    float  bx0, by0, bx1, by1;          /* the box inside the texture */
    float  btn[2][4];       /* x0 y0 x1 y1 of each button, texture pixels */
    int    nbtn;
} al_box;

static void al_over(al_px *d, float r, float g, float b, float a)
{
    d->r = r * a + d->r * (1 - a);
    d->g = g * a + d->g * (1 - a);
    d->b = b * a + d->b * (1 - a);
    d->a = a + d->a * (1 - a);
}

/* coverage of pixel (x, y) by a rounded rectangle, anti-aliased */
static float al_rr(float x, float y, float x0, float y0, float x1, float y1,
                   float rad)
{
    float px = x + 0.5f, py = y + 0.5f;
    float cx = (x0 + x1) * 0.5f, cy = (y0 + y1) * 0.5f;
    float qx = fabsf(px - cx) - ((x1 - x0) * 0.5f - rad);
    float qy = fabsf(py - cy) - ((y1 - y0) * 0.5f - rad);
    float ox = qx > 0 ? qx : 0, oy = qy > 0 ? qy : 0;
    float in = (qx > qy ? qx : qy);
    float d = sqrtf(ox * ox + oy * oy) + (in < 0 ? in : 0) - rad;
    float c = 0.5f - d;
    return c < 0 ? 0 : c > 1 ? 1 : c;
}

static int u16len(const unsigned short *s)
{
    int n = 0;

    while (s && s[n])
        n++;
    return n;
}

/* a coverage mask of `text`, white on black, centred and word-wrapped in
 * `tw` pixels; returns the mask (w = tw, *th rows), malloc'd, or NULL with
 * no font. The wrapping is done here, at spaces and newlines only: GDI's
 * DrawText broke words at accented letters ("PÁ / GINA"). */
static unsigned char *al_text(const unsigned short *text, int tw, int fontpx,
                              int *th)
{
    unsigned char *mask;
    int h, i, n = 0, lh = ui_font_height(fontpx, 1), len = u16len(text);
    int ls[32], ll[32];

    *th = 1;
    if (lh <= 0 || tw <= 0)
        return NULL;
    for (i = 0; i <= len && n < 32; ) {
        int start = i, brk = -1, j = i;
        /* the longest run of words from `start` that fits */
        while (j < len && text[j] != '\n') {
            if (text[j] == ' ') {
                if (ui_font_width(text + start, j - start, fontpx, 1) > tw
                    && brk > start)
                    break;
                brk = j;
            }
            j++;
        }
        if (j >= len || text[j] == '\n') {
            if (ui_font_width(text + start, j - start, fontpx, 1) > tw
                && brk > start)
                j = brk;
        } else {
            j = brk;
        }
        ls[n] = start;
        ll[n] = j - start;
        n++;
        i = j + 1;                      /* past the space or the newline */
        if (j >= len)
            break;
    }
    h = n * lh > 1 ? n * lh : 1;
    mask = (unsigned char *)calloc((size_t)tw * h, 1);
    if (!mask)
        return NULL;
    for (i = 0; i < n; i++) {
        int lw, lhh, x, y, x0;
        unsigned char *m = ui_font_line(text + ls[i], ll[i], fontpx, 1, &lw, &lhh);

        if (!m)
            continue;
        x0 = (tw - lw) / 2;
        for (y = 0; y < lhh && i * lh + y < h; y++)
            for (x = 0; x < lw; x++) {
                int X = x0 + x;
                unsigned char c = m[y * lw + x];
                unsigned char *d = &mask[(i * lh + y) * tw + X];
                if (X >= 0 && X < tw && c > *d)
                    *d = c;
            }
        free(m);
    }
    *th = h;
    return mask;
}

/* white text with the iPhone's dark shadow one point above it */
static void al_put_text(al_box *b, const unsigned char *m, int tw, int th,
                        int x, int y)
{
    int i, j, sh = (int)(b->s + 0.5f);

    if (!m)
        return;
    for (j = 0; j < th; j++)
        for (i = 0; i < tw; i++) {
            float c = m[j * tw + i] / 255.0f;
            int X = x + i, Y = y + j - sh;
            if (c > 0 && X >= 0 && X < b->w && Y >= 0 && Y < b->h)
                al_over(&b->px[Y * b->w + X], 0, 0, 0, c * 0.55f);
        }
    for (j = 0; j < th; j++)
        for (i = 0; i < tw; i++) {
            float c = m[j * tw + i] / 255.0f;
            int X = x + i, Y = y + j;
            if (c > 0 && X >= 0 && X < b->w && Y >= 0 && Y < b->h)
                al_over(&b->px[Y * b->w + X], 1, 1, 1, c);
        }
}

static void al_button(al_box *b, int k, int pressed, int light,
                      const unsigned short *label)
{
    float x0 = b->btn[k][0], y0 = b->btn[k][1];
    float x1 = b->btn[k][2], y1 = b->btn[k][3];
    float rad = 6 * b->s, mid = (y0 + y1) * 0.5f;
    int x, y, tw, th;
    unsigned char *m;

    for (y = (int)y0 - 1; y <= (int)y1 + 1; y++)
        for (x = (int)x0 - 1; x <= (int)x1 + 1; x++) {
            float edge, fill, t, top, bot, r, g, bl;
            al_px *d;
            if (x < 0 || y < 0 || x >= b->w || y >= b->h)
                continue;
            d = &b->px[y * b->w + x];
            edge = al_rr((float)x, (float)y, x0, y0, x1, y1, rad);
            fill = al_rr((float)x, (float)y, x0 + b->s, y0 + b->s,
                         x1 - b->s, y1 - b->s, rad - b->s);
            if (edge <= 0)
                continue;
            al_over(d, 0.03f, 0.05f, 0.12f, (edge - fill > 0 ? edge - fill : 0) * 0.85f);   /* the dark rim */
            if (fill <= 0)
                continue;
            /* a lighter upper half with a gloss, a darker lower half */
            t = (y + 0.5f - y0) / (y1 - y0);
            top = light ? 0.56f : 0.44f;
            bot = light ? 0.30f : 0.22f;
            if (y + 0.5f < mid) {
                r = top + 0.10f * (1 - t * 2);
                g = r + 0.02f;
                bl = r + 0.08f;
            } else {
                r = bot - 0.06f * (t * 2 - 1);
                g = r + 0.02f;
                bl = r + 0.08f;
            }
            if (pressed) {                  /* the iPhone's blue press */
                r = r * 0.4f;
                g = g * 0.55f + 0.12f;
                bl = bl * 0.6f + 0.38f;
            }
            al_over(d, r, g, bl, fill * 0.82f);
            if (y < (int)(y0 + 2 * b->s))   /* the highlight along the top */
                al_over(d, 1, 1, 1, fill * 0.25f);
        }

    tw = (int)(x1 - x0);
    m = al_text(label, tw, (int)(17 * b->s), &th);
    al_put_text(b, m, tw, th, (int)x0, (int)(mid - th * 0.5f));
    free(m);
}

/* the box itself: drop shadow, light border ring, translucent dark blue,
 * the gloss across the top */
static void al_draw_bg(al_box *b)
{
    float s = b->s, rad = 10 * s, bw = 2 * s;
    float gcx = (b->bx0 + b->bx1) * 0.5f;
    float grx = (b->bx1 - b->bx0) * 0.75f, gry = 34 * s;
    int x, y;

    memset(b->px, 0, sizeof(al_px) * b->w * b->h);
    for (y = 0; y < b->h; y++)
        for (x = 0; x < b->w; x++) {
            al_px *d = &b->px[y * b->w + x];
            float sh = al_rr((float)x, (float)y - 3 * s, b->bx0, b->by0,
                             b->bx1, b->by1, rad + 2 * s);
            float out = al_rr((float)x, (float)y, b->bx0, b->by0, b->bx1,
                              b->by1, rad);
            float in = al_rr((float)x, (float)y, b->bx0 + bw, b->by0 + bw,
                             b->bx1 - bw, b->by1 - bw, rad - bw);
            float ex, ey;
            if (sh > 0)
                al_over(d, 0, 0, 0, sh * 0.25f);        /* the drop shadow */
            if (out <= 0)
                continue;
            al_over(d, 0.86f, 0.88f, 0.93f, (out - in > 0 ? out - in : 0) * 0.9f);   /* the border ring */
            if (in <= 0)
                continue;
            /* translucent dark blue, a touch lighter at the top */
            al_over(d, 0.03f, 0.09f, 0.24f,
                    in * (0.62f + 0.08f * (1 - (y - b->by0) / (b->by1 - b->by0))));
            /* the gloss: an ellipse whose lower edge crosses the top */
            ex = (x + 0.5f - gcx) / grx;
            ey = (y + 0.5f - (b->by0 + gry - gry * 1.6f)) / (gry * 1.6f);
            if (ex * ex + ey * ey < 1)
                al_over(d, 1, 1, 1, in * 0.16f);
        }
}

static void al_draw(al_box *b, const unsigned short *msg,
                    const unsigned short *ok, const unsigned short *cancel,
                    int pressed)
{
    float s = b->s;
    int tw, th;
    unsigned char *m;

    al_draw_bg(b);
    tw = (int)(b->bx1 - b->bx0 - 24 * s);
    m = al_text(msg, tw, (int)(18 * s), &th);
    al_put_text(b, m, tw, th, (int)(b->bx0 + 12 * s), (int)(b->by0 + 18 * s));
    free(m);

    al_button(b, 0, pressed == 0, 0, ok);
    if (b->nbtn == 2)
        al_button(b, 1, pressed == 1, 1, cancel);
}

/* one line of text, its exact width: a coverage mask *w x *h, malloc'd,
 * or NULL with no font. UTF-8 (a player's name, settings_menu.c); anything
 * that is not valid UTF-8 is read as Latin-1. */
static unsigned char *al_line(const char *s8, int fontpx, int bold,
                              int *w, int *h)
{
    unsigned short ws[256];
    const unsigned char *p = (const unsigned char *)s8;
    int n = 0, bad = 0;

    while (*p && n < 255) {
        unsigned c = *p, k = 0;
        if (c < 0x80) {
            ws[n++] = (unsigned short)c;
            p++;
            continue;
        }
        if ((c & 0xe0) == 0xc0)      { k = 1; c &= 0x1f; }
        else if ((c & 0xf0) == 0xe0) { k = 2; c &= 0x0f; }
        else { bad = 1; break; }
        for (p++; k; k--, p++) {
            if ((*p & 0xc0) != 0x80) {
                bad = 1;
                break;
            }
            c = c << 6 | (*p & 0x3f);
        }
        if (bad)
            break;
        ws[n++] = (unsigned short)c;
    }
    if (bad)
        for (n = 0; s8[n] && n < 255; n++)
            ws[n] = (unsigned char)s8[n];
    ws[n] = 0;
    return ui_font_line(ws, n, fontpx, bold, w, h);
}

/* text in a colour, with the dark shadow above it, alpha `a` */
static void al_put_line(al_box *b, const unsigned char *m, int tw, int th,
                        int x, int y, float r, float g, float bl, float a)
{
    int i, j, sh = (int)(b->s + 0.5f);

    if (!m)
        return;
    for (j = 0; j < th; j++)
        for (i = 0; i < tw; i++) {
            float c = m[j * tw + i] / 255.0f;
            int X = x + i, Y = y + j - sh;
            if (c > 0 && X >= 0 && X < b->w && Y >= 0 && Y < b->h)
                al_over(&b->px[Y * b->w + X], 0, 0, 0, c * 0.5f * a);
        }
    for (j = 0; j < th; j++)
        for (i = 0; i < tw; i++) {
            float c = m[j * tw + i] / 255.0f;
            int X = x + i, Y = y + j;
            if (c > 0 && X >= 0 && X < b->w && Y >= 0 && Y < b->h)
                al_over(&b->px[Y * b->w + X], r, g, bl, c * a);
        }
}

/* ------------------------------------------------------------ glyphs
 * Port, not in the binary: key and button pictures in the menus' text
 * (plat_key_glyph, plat_pad_glyph). Read once from "aditional textures/
 * glyphs/<name>.png" beside res -- Julio Cacko's Free Input Prompts, CC0 --
 * and kept. */
const char *lime_platform_asset_root(void);
void lime_glyph_shadow(uint8_t *px, int w, int h); /* draw_gl.c */

typedef struct { char name[64]; uint8_t *rgba; int w, h; } al_glyph;
static al_glyph g_glyphs[128];
static int g_nglyphs;

static const al_glyph *al_glyph_get(const char *name, int len)
{
    char full[1200];
    al_glyph *g;
    int i;

    if (len <= 0 || len >= (int)sizeof g_glyphs[0].name)
        return NULL;
    for (i = 0; i < g_nglyphs; i++)
        if ((int)strlen(g_glyphs[i].name) == len
            && memcmp(g_glyphs[i].name, name, (size_t)len) == 0)
            return g_glyphs[i].rgba ? &g_glyphs[i] : NULL;
    if (g_nglyphs == (int)(sizeof g_glyphs / sizeof g_glyphs[0]))
        return NULL;
    g = &g_glyphs[g_nglyphs++];
    memcpy(g->name, name, (size_t)len);
    g->name[len] = 0;
    snprintf(full, sizeof full, "%s/../aditional textures/glyphs/%s.png",
             lime_platform_asset_root(), g->name);
    if (!lime_png_load(full, &g->rgba, &g->w, &g->h) || g->w <= 0 || g->h <= 0)
        g->rgba = NULL;
    else
        lime_glyph_shadow(g->rgba, g->w, g->h);
    return g->rgba ? g : NULL;
}

int ui_glyph_have(const char *name)
{
    return al_glyph_get(name, (int)strlen(name)) != NULL;
}

/* A glyph hp pixels tall at (x, y), averaged down from its picture. */
static void al_put_glyph(al_box *b, const al_glyph *g, int x, int y, int hp,
                         float a)
{
    int w = g->w * hp / g->h, i, j;

    for (j = 0; j < hp; j++)
        for (i = 0; i < w; i++) {
            int sx0 = i * g->w / w, sx1 = (i + 1) * g->w / w;
            int sy0 = j * g->h / hp, sy1 = (j + 1) * g->h / hp, u, v, k = 0;
            float pr = 0, pg = 0, pb = 0, pa = 0;
            int X = x + i, Y = y + j;

            if (X < 0 || X >= b->w || Y < 0 || Y >= b->h)
                continue;
            if (sx1 <= sx0) sx1 = sx0 + 1;
            if (sy1 <= sy0) sy1 = sy0 + 1;
            for (v = sy0; v < sy1 && v < g->h; v++)
                for (u = sx0; u < sx1 && u < g->w; u++) {
                    const uint8_t *p = g->rgba + ((size_t)v * g->w + u) * 4;
                    float al = p[3] / 255.0f;
                    pr += p[0] / 255.0f * al;
                    pg += p[1] / 255.0f * al;
                    pb += p[2] / 255.0f * al;
                    pa += al;
                    k++;
                }
            if (k == 0 || pa <= 0)
                continue;
            al_over(&b->px[Y * b->w + X], pr / pa, pg / pa, pb / pa, pa / k * a);
        }
}

/* align: 0 left at x, 1 centred on x, 2 right-aligned to x. "\001name\001"
 * in s is a glyph, a little taller than the text. */
static void al_say(al_box *b, const char *s, int fontpx, int bold, float x,
                   float ymid, int align, float r, float g, float bl, float a)
{
    struct { unsigned char *m; const al_glyph *gl; int w, h; } pc[16];
    int np = 0, tot = 0, gh = (int)(fontpx * 1.4f + 0.5f), gap = fontpx / 5, i;
    const char *p = s;
    float cx;

    if (!s || !s[0])
        return;
    while (*p && np < 16) {
        const char *q;
        if (*p == 1) {
            const al_glyph *gl;
            q = strchr(p + 1, 1);
            if (!q)
                break;
            gl = al_glyph_get(p + 1, (int)(q - p - 1));
            if (gl) {
                pc[np].m = NULL;
                pc[np].gl = gl;
                pc[np].h = gh;
                pc[np].w = gl->w * gh / gl->h;
                np++;
            }
            p = q + 1;
        } else {
            char t[256];
            size_t len;
            q = strchr(p, 1);
            len = q ? (size_t)(q - p) : strlen(p);
            if (len > sizeof t - 1)
                len = sizeof t - 1;
            memcpy(t, p, len);
            t[len] = 0;
            p += len;
            pc[np].gl = NULL;
            pc[np].m = al_line(t, fontpx, bold, &pc[np].w, &pc[np].h);
            if (pc[np].m)
                np++;
        }
    }
    for (i = 0; i < np; i++)
        tot += pc[i].w + (i ? gap : 0);
    cx = align == 1 ? x - tot * 0.5f : align == 2 ? x - tot : x;
    for (i = 0; i < np; i++) {
        if (i)
            cx += gap;
        if (pc[i].gl)
            al_put_glyph(b, pc[i].gl, (int)cx, (int)(ymid - pc[i].h * 0.5f),
                         pc[i].h, a);
        else {
            al_put_line(b, pc[i].m, pc[i].w, pc[i].h, (int)cx,
                        (int)(ymid - pc[i].h * 0.5f), r, g, bl, a);
            free(pc[i].m);
        }
        cx += pc[i].w;
    }
}

/* The player's badge (settings_menu.c, port): the profile's icon and name
 * in a small box of the alerts' dress -- the border ring, the gloss, the
 * drop shadow -- but black instead of blue, darker at the bottom. `icon` is
 * icon_px square, straight RGBA; `s` pixels per point. Returns RGBA8,
 * premultiplied, *w x *h, malloc'd. */
unsigned char *plat_ui_badge(const char *name, const unsigned char *icon,
                             int icon_px, float s, int *ow, int *oh)
{
    al_box b;
    float m = 4 * s, h = 30 * s, isz = 24 * s, pad = 3 * s, rad = 8 * s,
          bw = 1.5f * s;
    int tw = 0, th = 0, x, y;
    unsigned char *tm = al_line(name && name[0] ? name : "-", (int)(12 * s), 1,
                                &tw, &th);
    unsigned char *out;

    if (!tm)
        return NULL;                    /* no font */
    memset(&b, 0, sizeof b);
    b.s = s;
    b.bx0 = m;
    b.by0 = m;
    b.bx1 = m + pad + isz + 6 * s + tw + 8 * s;
    b.by1 = m + h;
    b.w = (int)(b.bx1 + m + 1);
    b.h = (int)(b.by1 + m + 1);
    b.px = (al_px *)calloc((size_t)b.w * b.h, sizeof(al_px));
    if (!b.px) {
        free(tm);
        return NULL;
    }
    for (y = 0; y < b.h; y++)
        for (x = 0; x < b.w; x++) {
            al_px *d = &b.px[y * b.w + x];
            float sh = al_rr((float)x, (float)y - 2 * s, b.bx0, b.by0,
                             b.bx1, b.by1, rad + 2 * s);
            float out_ = al_rr((float)x, (float)y, b.bx0, b.by0, b.bx1,
                               b.by1, rad);
            float in = al_rr((float)x, (float)y, b.bx0 + bw, b.by0 + bw,
                             b.bx1 - bw, b.by1 - bw, rad - bw);
            float t = (y + 0.5f - b.by0) / (b.by1 - b.by0), ex, ey;

            if (sh > 0)
                al_over(d, 0, 0, 0, sh * 0.3f);         /* the drop shadow */
            if (out_ <= 0)
                continue;
            al_over(d, 0.86f, 0.88f, 0.93f, (out_ - in > 0 ? out_ - in : 0) * 0.85f);
            if (in <= 0)
                continue;
            /* black, a dark grey at the top */
            al_over(d, 0.16f * (1 - t), 0.16f * (1 - t), 0.18f * (1 - t),
                    in * (0.80f + 0.12f * t));
            ex = (x + 0.5f - (b.bx0 + b.bx1) * 0.5f) / ((b.bx1 - b.bx0) * 0.75f);
            ey = (y + 0.5f - (b.by0 - h * 0.35f)) / (h * 0.8f);
            if (ex * ex + ey * ey < 1)                  /* the gloss */
                al_over(d, 1, 1, 1, in * 0.18f);
        }
    if (icon && icon_px > 0) {                          /* the icon, scaled */
        int ix0 = (int)(b.bx0 + pad), iy0 = (int)(b.by0 + (h - isz) * 0.5f);
        int n = (int)isz;
        for (y = 0; y < n; y++)
            for (x = 0; x < n; x++) {
                int X = ix0 + x, Y = iy0 + y;
                const unsigned char *p = icon + ((size_t)(y * icon_px / n) * icon_px
                                                 + (size_t)(x * icon_px / n)) * 4;
                if (X >= 0 && X < b.w && Y >= 0 && Y < b.h && p[3])
                    al_over(&b.px[Y * b.w + X], p[0] / 255.0f, p[1] / 255.0f,
                            p[2] / 255.0f, p[3] / 255.0f);
            }
    }
    al_put_line(&b, tm, tw, th, (int)(b.bx0 + pad + isz + 6 * s),
                (int)((b.by0 + b.by1) * 0.5f - th * 0.5f), 1, 1, 1, 1);
    free(tm);

    out = (unsigned char *)malloc((size_t)b.w * b.h * 4);
    if (out)
        for (x = 0; x < b.w * b.h; x++) {
            out[x * 4 + 0] = (unsigned char)(b.px[x].r * 255 + 0.5f);
            out[x * 4 + 1] = (unsigned char)(b.px[x].g * 255 + 0.5f);
            out[x * 4 + 2] = (unsigned char)(b.px[x].b * 255 + 0.5f);
            out[x * 4 + 3] = (unsigned char)(b.px[x].a * 255 + 0.5f);
        }
    free(b.px);
    *ow = b.w;
    *oh = b.h;
    return out;
}

/* The debug menu (runtime/debug_menu.c) in the alerts' iPhone OS 3 dress:
 * the same translucent box, the title on top, one row per entry -- the
 * label on the left, its value on the right -- the selected row in the
 * iPhone's blue selection bar, rows that cannot act now in grey, and the
 * key help at the bottom. Under the title, one tab per page, the current
 * one in the selection blue. `s` is pixels per point of the menu's
 * 320-high space.
 * Returns RGBA8, premultiplied, *w x *h, malloc'd. */
unsigned char *plat_ui_menu(const char *title, const char *const *tabs,
                            int ntab, int tab, const char *const *label,
                            const char *const *value, const int *enabled,
                            int n, int sel, const char *footer, float width,
                            float s, int *ow, int *oh)
{
    al_box b;
    float m = 6 * s, top, rowh = 15 * s, x0, x1;
    int side = ntab < 0, nt = side ? -ntab : ntab;
    unsigned char *out;
    int i;

    if (!ui_font_ok())
        return NULL;                    /* the menu's own 5x7 font */
    memset(&b, 0, sizeof b);
    b.s = s;
    b.bx0 = m;
    b.by0 = m;
    b.bx1 = m + (width > 0 ? width : 360) * s;
    top = b.by0 + 22 * s + (ntab > 0 ? 20 * s : 0) + 4 * s;  /* under the title and tabs */
    b.by1 = top + n * rowh + 26 * s;
    if (side && b.by1 < top + (nt * 24 + 8 + UI_SIDE) * s + 26 * s)
        b.by1 = top + (nt * 24 + 8 + UI_SIDE) * s + 26 * s;   /* room for the icon */
    b.w = (int)(b.bx1 + m + 1);
    b.h = (int)(b.by1 + m + 1);
    b.px = (al_px *)malloc(sizeof(al_px) * b.w * b.h);
    al_draw_bg(&b);

    al_say(&b, title, (int)(13 * s), 1, (b.bx0 + b.bx1) * 0.5f,
           b.by0 + 12 * s, 1, 1, 1, 1, 1);

    x0 = b.bx0 + 9 * s;
    x1 = b.bx1 - 9 * s;
    if (side) {                             /* the side panel */
        float px0 = b.bx0 + 6 * s, px1 = b.bx0 + (UI_SIDE - 4) * s;
        float py0 = top, py1 = b.by1 - 24 * s;
        int x, y;

        for (y = (int)py0; y <= (int)py1; y++)
            for (x = (int)px0; x <= (int)px1; x++) {
                float c = al_rr((float)x, (float)y, px0, py0, px1, py1, 6 * s);
                if (c > 0 && x >= 0 && x < b.w && y >= 0 && y < b.h)
                    al_over(&b.px[y * b.w + x], 0, 0, 0.04f, c * 0.45f);
            }
        for (i = 0; i < nt; i++) {
            float ty0 = top + (4 + i * 24) * s, ty1 = ty0 + 20 * s;
            if (i == tab)
                for (y = (int)ty0; y <= (int)ty1; y++)
                    for (x = (int)px0 + 2; x <= (int)px1 - 2; x++) {
                        float c = al_rr((float)x, (float)y, px0 + 3 * s, ty0, px1 - 3 * s, ty1, 5 * s);
                        float t = (y + 0.5f - ty0) / (ty1 - ty0);
                        if (c > 0 && x >= 0 && x < b.w && y >= 0 && y < b.h)
                            al_over(&b.px[y * b.w + x], 0.02f + 0.03f * (1 - t),
                                    0.36f + 0.20f * (1 - t), 0.90f + 0.06f * (1 - t), c * 0.95f);
                    }
            al_say(&b, tabs[i], (int)(10.5f * s), 1, px0 + 10 * s, (ty0 + ty1) * 0.5f,
                   0, 1, 1, 1, i == tab ? 1.0f : 0.7f);
        }
        x0 = b.bx0 + (UI_SIDE + 6) * s;
        nt = 0;
    }
    for (i = 0; i < nt; i++) {              /* the tab strip */
        float tw = (x1 - x0) / ntab, tx0 = x0 + i * tw + 1 * s,
              tx1 = x0 + (i + 1) * tw - 1 * s, ty0 = b.by0 + 22 * s,
              ty1 = ty0 + 18 * s;
        int x, y;
        for (y = (int)ty0; y < (int)ty1 + 1; y++)
            for (x = (int)tx0 - 1; x <= (int)tx1 + 1; x++) {
                float c = al_rr((float)x, (float)y, tx0, ty0, tx1, ty1, 5 * s);
                float t = (y + 0.5f - ty0) / (ty1 - ty0);
                if (c <= 0 || x < 0 || x >= b.w || y < 0 || y >= b.h)
                    continue;
                if (i == tab)
                    al_over(&b.px[y * b.w + x], 0.02f + 0.03f * (1 - t),
                            0.36f + 0.20f * (1 - t), 0.90f + 0.06f * (1 - t),
                            c * 0.95f);
                else
                    al_over(&b.px[y * b.w + x], 1, 1, 1, c * 0.12f);
            }
        al_say(&b, tabs[i], (int)((ntab > 4 ? 8.5f : 10) * s), 1, (tx0 + tx1) * 0.5f,
               (ty0 + ty1) * 0.5f, 1, 1, 1, 1, i == tab ? 1.0f : 0.75f);
    }
    for (i = 0; i < n; i++) {
        float y0 = top + i * rowh, a = enabled[i] ? 1.0f : 0.4f;
        if (i == sel) {                     /* the blue selection bar */
            int x, y;
            for (y = (int)y0; y < (int)(y0 + rowh); y++)
                for (x = (int)x0 - 1; x <= (int)x1 + 1; x++) {
                    float c = al_rr((float)x, (float)y, x0, y0 + 0.5f * s,
                                    x1, y0 + rowh - 0.5f * s, 4 * s);
                    float t = (y + 0.5f - y0) / rowh;
                    if (c > 0 && x >= 0 && x < b.w)
                        al_over(&b.px[y * b.w + x],
                                0.02f + 0.03f * (1 - t), 0.36f + 0.20f * (1 - t),
                                0.90f + 0.06f * (1 - t), c * 0.92f);
                }
        }
        if (label[i][0] == 2) {             /* a section's heading */
            int x, y = (int)(y0 + rowh - 2 * s);
            al_say(&b, label[i] + 1, (int)(10 * s), 1, x0 + 4 * s, y0 + rowh * 0.55f,
                   0, 0.55f, 0.75f, 1.0f, 1);
            for (x = (int)(x0 + 4 * s); x < (int)(x1 - 4 * s) && y >= 0 && y < b.h; x++)
                al_over(&b.px[y * b.w + x], 0.55f, 0.75f, 1.0f,
                        0.45f * (1 - (x - x0) / (x1 - x0)));
            continue;
        }
        al_say(&b, label[i], (int)(11 * s), 1, x0 + 6 * s, y0 + rowh * 0.5f,
               0, 1, 1, 1, a);
        al_say(&b, value[i], (int)(11 * s), 0, x1 - 6 * s, y0 + rowh * 0.5f,
               2, 1, 1, 1, a);
    }
    al_say(&b, footer, (int)(9 * s), 0, (b.bx0 + b.bx1) * 0.5f,
           b.by1 - 13 * s, 1, 0.80f, 0.84f, 0.92f, 1);

    out = (unsigned char *)malloc((size_t)b.w * b.h * 4);
    for (i = 0; i < b.w * b.h; i++) {
        al_px *p = &b.px[i];
        out[i * 4 + 0] = (unsigned char)(p->r * 255 + 0.5f);
        out[i * 4 + 1] = (unsigned char)(p->g * 255 + 0.5f);
        out[i * 4 + 2] = (unsigned char)(p->b * 255 + 0.5f);
        out[i * 4 + 3] = (unsigned char)(p->a * 255 + 0.5f);
    }
    free(b.px);
    *ow = b.w;
    *oh = b.h;
    return out;
}

unsigned char *plat_ui_grid(const char *title,
                            const unsigned char *const *cells, int n,
                            int cols, int cpx, float cell, int sel,
                            const char *footer, float s, int *ow, int *oh)
{
    al_box b;
    float m = 6 * s, cs = cell * s, step = (cell + 4) * s, pad = 10 * s, top;
    int rows = cols > 0 ? (n + cols - 1) / cols : 0, i, x, y;
    unsigned char *out;

    if (n <= 0 || cols <= 0 || !ui_font_ok())
        return NULL;
    memset(&b, 0, sizeof b);
    b.s = s;
    b.bx0 = m;
    b.by0 = m;
    b.bx1 = m + 2 * pad + (cols - 1) * step + cs;
    top = b.by0 + 22 * s + 4 * s;
    b.by1 = top + (rows - 1) * step + cs + 26 * s;
    b.w = (int)(b.bx1 + m + 1);
    b.h = (int)(b.by1 + m + 1);
    b.px = (al_px *)malloc(sizeof(al_px) * b.w * b.h);
    if (!b.px)
        return NULL;
    al_draw_bg(&b);
    al_say(&b, title, (int)(13 * s), 1, (b.bx0 + b.bx1) * 0.5f,
           b.by0 + 12 * s, 1, 1, 1, 1, 1);
    for (i = 0; i < n; i++) {
        float x0 = b.bx0 + pad + (i % cols) * step, y0 = top + (i / cols) * step;
        al_glyph g;

        if (i == sel)                   /* the selection blue round it */
            for (y = (int)(y0 - 3 * s); y <= (int)(y0 + cs + 3 * s); y++)
                for (x = (int)(x0 - 3 * s); x <= (int)(x0 + cs + 3 * s); x++) {
                    float c = al_rr((float)x, (float)y, x0 - 2.5f * s, y0 - 2.5f * s,
                                    x0 + cs + 2.5f * s, y0 + cs + 2.5f * s, cs * 0.24f);
                    if (c > 0 && x >= 0 && x < b.w && y >= 0 && y < b.h)
                        al_over(&b.px[y * b.w + x], 0.15f, 0.55f, 1.0f, c * 0.95f);
                }
        if (!cells[i])
            continue;
        memset(&g, 0, sizeof g);
        g.rgba = (uint8_t *)cells[i];
        g.w = g.h = cpx;
        al_put_glyph(&b, &g, (int)x0, (int)y0, (int)cs, 1);
    }
    al_say(&b, footer, (int)(9 * s), 0, (b.bx0 + b.bx1) * 0.5f,
           top + (rows - 1) * step + cs + 13 * s, 1, 0.80f, 0.84f, 0.92f, 1);

    out = (unsigned char *)malloc((size_t)b.w * b.h * 4);
    for (i = 0; out && i < b.w * b.h; i++) {
        al_px *p = &b.px[i];
        out[i * 4 + 0] = (unsigned char)(p->r * 255 + 0.5f);
        out[i * 4 + 1] = (unsigned char)(p->g * 255 + 0.5f);
        out[i * 4 + 2] = (unsigned char)(p->b * 255 + 0.5f);
        out[i * 4 + 3] = (unsigned char)(p->a * 255 + 0.5f);
    }
    free(b.px);
    *ow = b.w;
    *oh = b.h;
    return out;
}

static int al_pot(int n)
{
    int p = 1;
    while (p < n)
        p <<= 1;
    return p;
}

/* upload w x h RGBA8 into a power-of-two texture (GL 1.1) */
static GLuint al_upload(const unsigned char *rgba, int w, int h)
{
    GLuint t;
    int pw = al_pot(w), ph = al_pot(h);

    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pw, ph, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, NULL);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE,
                    rgba);
    return t;
}

static GLuint al_box_tex(al_box *b)
{
    unsigned char *p = (unsigned char *)malloc((size_t)b->w * b->h * 4);
    GLuint t;
    int i;

    for (i = 0; i < b->w * b->h; i++) {
        al_px *s = &b->px[i];
        p[i * 4 + 0] = (unsigned char)(s->r * 255 + 0.5f);
        p[i * 4 + 1] = (unsigned char)(s->g * 255 + 0.5f);
        p[i * 4 + 2] = (unsigned char)(s->b * 255 + 0.5f);
        p[i * 4 + 3] = (unsigned char)(s->a * 255 + 0.5f);
    }
    t = al_upload(p, b->w, b->h);
    free(p);
    return t;
}

static void al_quad(GLuint t, float x0, float y0, float x1, float y1,
                    float u1, float v1, int flip)
{
    glBindTexture(GL_TEXTURE_2D, t);
    glBegin(GL_QUADS);
    glTexCoord2f(0, flip ? v1 : 0);  glVertex2f(x0, y0);
    glTexCoord2f(u1, flip ? v1 : 0); glVertex2f(x1, y0);
    glTexCoord2f(u1, flip ? 0 : v1); glVertex2f(x1, y1);
    glTexCoord2f(0, flip ? 0 : v1);  glVertex2f(x0, y1);
    glEnd();
}

static void al_layout(al_box *b, int ww, int wh, const unsigned short *msg)
{
    float s = (float)ww / 480.0f, sy = (float)wh / 320.0f;
    float m, bw, top, by;
    int th;
    unsigned char *mask;

    if (sy < s)
        s = sy;
    s *= 0.8f;                  /* a little smaller than the device's */
    b->s = s;
    m = 8 * s;
    bw = 284 * s;
    mask = al_text(msg, (int)(bw - 24 * s), (int)(18 * s), &th);
    free(mask);
    b->bx0 = m;
    b->by0 = m;
    b->bx1 = m + bw;
    top = 18 * s + th + 16 * s;             /* the text, then the buttons */
    by = b->by0 + top;
    b->by1 = by + 43 * s + 14 * s;
    b->w = (int)(bw + 2 * m + 1);
    b->h = (int)(b->by1 + m + 1);
    if (b->nbtn == 2) {
        float half = (bw - 22 * s - 8 * s) * 0.5f;
        b->btn[0][0] = b->bx0 + 11 * s;  b->btn[0][2] = b->btn[0][0] + half;
        b->btn[1][0] = b->btn[0][2] + 8 * s;  b->btn[1][2] = b->bx1 - 11 * s;
    } else {
        b->btn[0][0] = b->bx0 + 11 * s;  b->btn[0][2] = b->bx1 - 11 * s;
    }
    b->btn[0][1] = b->btn[1][1] = by;
    b->btn[0][3] = b->btn[1][3] = by + 43 * s;
    b->px = (al_px *)malloc(sizeof(al_px) * b->w * b->h);
}

int plat_ask(const unsigned short *msg16, const unsigned short *ok16,
             const unsigned short *cancel16)
{
    static const unsigned short ok_text[] = { 'O', 'K', 0 };
    const unsigned short *msg = msg16;
    const unsigned short *ok = ok16 ? ok16 : ok_text;
    const unsigned short *cancel = cancel16;
    int ww, wh, bgw, bgh, pressed = -1, drawn = -2, result = -1;
    int armed_mouse = 0, armed_keys = 0, press_on = -1, lit = -1;
    GLint vp[4];
    unsigned char *bg;
    GLuint bgt, boxt = 0;
    al_box b;

    if (!ui_font_ok())
        return ui_ask_native(msg, ok, cancel);
    plat_size(&ww, &wh);
    if (ww <= 0 || wh <= 0)
        return 0;
    if (plat_ask_hook)
        plat_ask_hook(1);
    bgw = ww;
    bgh = wh;

    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glGetIntegerv(GL_VIEWPORT, vp);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();

    /* the frame the game had drawn so far: the back buffer, bottom-up */
    bg = (unsigned char *)malloc((size_t)bgw * bgh * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, bgw, bgh, GL_RGBA, GL_UNSIGNED_BYTE, bg);
    {
        int i;
        for (i = 0; i < bgw * bgh; i++)
            bg[i * 4 + 3] = 255;
    }
    bgt = al_upload(bg, bgw, bgh);
    free(bg);

    memset(&b, 0, sizeof b);
    b.nbtn = cancel ? 2 : 1;
    al_layout(&b, ww, wh, msg);

    for (;;) {
        int mx, my, down, cw, ch, over = -1, k, show;
        int kok, kcancel, kleft, kright;
        PLAT_PAD p1, p2;
        unsigned pb;
        float ox, oy;

        if (!plat_poll()) {
            result = b.nbtn - 1;
            break;
        }
        plat_size(&cw, &ch);
        if (cw > 0 && ch > 0 && (cw != ww || ch != wh)) {
            ww = cw;
            wh = ch;
            free(b.px);
            {
                int n = b.nbtn;
                memset(&b, 0, sizeof b);
                b.nbtn = n;
            }
            al_layout(&b, ww, wh, msg);
            drawn = -2;
        }
        ox = (float)(int)((ww - b.w) * 0.5f);
        oy = (float)(int)((wh - b.h) * 0.5f);

        down = plat_mouse(&mx, &my) && ui_focused();
        for (k = 0; k < b.nbtn; k++)
            if (mx >= ox + b.btn[k][0] && mx < ox + b.btn[k][2] &&
                my >= oy + b.btn[k][1] && my < oy + b.btn[k][3])
                over = k;
        if (!down)
            armed_mouse = 1;    /* the click that opened it does not count */
        if (armed_mouse) {
            if (down && press_on < 0)
                press_on = over >= 0 ? over : 9;
            if (!down && press_on >= 0) {
                if (press_on == over) {
                    result = over;
                    break;
                }
                press_on = -1;
            }
        }
        pressed = (down && press_on >= 0 && press_on == over) ? over : -1;

        /* the keys and both players' pads: nothing counts until every one
         * is let go (the press that made the game ask) */
        plat_pad_read(&p1);
        plat_pad2_read(&p2);
        pb = p1.buttons | p2.buttons;
        kok = ui_key(UI_K_OK) || (pb & (1u << PB_SOUTH | 1u << PB_START));
        kcancel = ui_key(UI_K_CANCEL) || (pb & 1u << PB_EAST);
        kleft = ui_key(UI_K_LEFT) || p1.left || p2.left;
        kright = ui_key(UI_K_RIGHT) || p1.right || p2.right;
        if (!kok && !kcancel && !kleft && !kright)
            armed_keys = 1;
        else if (armed_keys) {
            armed_keys = 0;             /* one answer per press */
            if (kleft || kright)
                lit = b.nbtn == 2 && kright ? 1 : 0;
            else if (kok) {
                result = lit >= 0 ? lit : 0;
                break;
            } else {
                result = b.nbtn - 1;
                break;
            }
        }

        show = pressed >= 0 ? pressed : lit;    /* the lit one looks pressed */
        if (show != drawn) {
            if (boxt)
                glDeleteTextures(1, &boxt);
            al_draw(&b, msg, ok, cancel, show);
            boxt = al_box_tex(&b);
            drawn = show;
        }

        glViewport(0, 0, ww, wh);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, ww, wh, 0, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glDisable(GL_ALPHA_TEST);
        glDisable(GL_FOG);
        glDisable(GL_LIGHTING);
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_BLEND);
        glEnable(GL_TEXTURE_2D);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        glColor4f(1, 1, 1, 1);
        al_quad(bgt, 0, 0, (float)ww, (float)wh,
                (float)bgw / al_pot(bgw), (float)bgh / al_pot(bgh), 1);
        glEnable(GL_BLEND);
        glDisable(GL_TEXTURE_2D);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(0, 0, 0, 0.3f);           /* the screen dims behind it */
        glBegin(GL_QUADS);
        glVertex2f(0, 0); glVertex2f((float)ww, 0);
        glVertex2f((float)ww, (float)wh); glVertex2f(0, (float)wh);
        glEnd();
        glEnable(GL_TEXTURE_2D);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);   /* premultiplied */
        glColor4f(1, 1, 1, 1);
        al_quad(boxt, ox, oy, ox + b.w, oy + b.h,
                (float)b.w / al_pot(b.w), (float)b.h / al_pot(b.h), 0);
        /* UMK3_ALERT_SHOT=<file.ppm>: a scripted test saves the dialog and
         * answers OK, since nothing else runs while it is up */
        if (getenv("UMK3_ALERT_SHOT")) {
            FILE *f = fopen(getenv("UMK3_ALERT_SHOT"), "wb");
            unsigned char *p = (unsigned char *)malloc((size_t)ww * wh * 3);
            int r;
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glReadPixels(0, 0, ww, wh, GL_RGB, GL_UNSIGNED_BYTE, p);
            if (f && p) {
                fprintf(f, "P6\n%d %d\n255\n", ww, wh);
                for (r = wh - 1; r >= 0; r--)
                    fwrite(p + (size_t)r * ww * 3, 1, (size_t)ww * 3, f);
            }
            if (f)
                fclose(f);
            free(p);
            result = 0;
            break;
        }
        plat_swap();
        ui_sleep_ms(15);
    }

    /* Put the game's half-drawn frame back in the back buffer, so the rest
     * of this frame lands on it rather than on the dialog. */
    glViewport(0, 0, ww, wh);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, ww, wh, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
    glColor4f(1, 1, 1, 1);
    al_quad(bgt, 0, 0, (float)ww, (float)wh,
            (float)bgw / al_pot(bgw), (float)bgh / al_pot(bgh), 1);

    glDeleteTextures(1, &bgt);
    if (boxt)
        glDeleteTextures(1, &boxt);
    free(b.px);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glPopAttrib();
    glViewport(vp[0], vp[1], vp[2], vp[3]);
    if (plat_ask_hook)
        plat_ask_hook(0);
    return result < 0 ? 0 : result;
}

