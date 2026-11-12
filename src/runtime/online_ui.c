/*
 * online_ui.c -- the players' badges in an online match. Port, not in the
 * binary.
 *
 * game_main.c calls online_after_frame() over every finished frame, as it
 * does for the main menu's badge (settings_menu.c, set_menu_badge). A badge
 * is a profile's icon and name (debug_menu.c, ui_badge_at):
 *
 *   ONLINE screen (FE task 51)     this player's, on the left
 *   multiplayer select (FE 0x2a)   this player's top left, the other's top
 *                                  right (names only: the code stays private)
 *   versus screen (FE 0x2b)        player 1's on the left, player 2's on the
 *                                  right; the host is player 1
 *
 * In the fight the names are drawn with the HUD instead, without the icon
 * (online_fight_names_hud, at the end of this file).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform/platform.h"
#include "debug_menu.h"
#include "mp_net.h"
#include "online_ui.h"
#include "profiles.h"

extern int  CurrentTask;
extern int  FE_CurrentTask;
extern long GameMode;

#define ICON_PX 96
#define PIC_PX  192                     /* the ONLINE screen's portraits */

struct look { int icon; unsigned color, color2; int dark, angle, alt; };

/* The icon for a slot, made again only when the look changes. */
static const unsigned char *slot_icon(int slot, const struct look *l)
{
    static unsigned char *px[8];
    static struct look have[8];
    static int made[8];

    if (!made[slot] || memcmp(&have[slot], l, sizeof *l) != 0) {
        free(px[slot]);
        px[slot] = prof_icon_rgba(l->icon, l->alt, l->color, l->dark, l->color2, l->angle, ICON_PX);
        have[slot] = *l;
        made[slot] = 1;
    }
    return px[slot];
}

static unsigned key_of(const char *name, const struct look *l)
{
    unsigned k = 2166136261u;
    const unsigned char *p = (const unsigned char *)name;
    const unsigned char *b = (const unsigned char *)l;
    size_t i;

    for (; *p; p++)
        k = (k ^ *p) * 16777619u;
    for (i = 0; i < sizeof *l; i++)
        k = (k ^ b[i]) * 16777619u;
    return k;
}

static void badge(int slot, const char *name, const struct look *l,
                  float x, float y, int align, float size)
{
    const unsigned char *icon = slot_icon(slot, l);

    ui_badge_at(slot, name, icon, ICON_PX, key_of(name, l), x, y, align, size);
}

static void my_look(struct look *l)
{
    const struct profile *p = prof_get();

    memset(l, 0, sizeof *l);
    if (p) {
        l->icon = p->icon;
        l->color = p->color;
        l->color2 = p->color2;
        l->dark = p->dark;
        l->angle = p->angle;
        l->alt = p->alt;
    } else
        l->angle = -1;
}

static void peer_look(const struct mpnet_peer *o, struct look *l)
{
    memset(l, 0, sizeof *l);
    l->icon = o->icon;
    l->color = o->color;
    l->color2 = o->color2;
    l->dark = o->dark;
    l->angle = o->angle;
    l->alt = o->alt;
}

void online_after_frame(void)
{
    const struct profile *me = prof_get();
    const struct mpnet_peer *o = mpnet_peer();
    struct look ml, ol;
    float w;

    if (!me)
        return;
    my_look(&ml);
    w = ui_space_width();

    /* the ONLINE screen, and the main menu's player banner (FrontEnd.c
     * PortMainBanner) */
    if (CurrentTask == 3 && (FE_CurrentTask == 51 || FE_CurrentTask == 0)) {
        /* FrontEnd.c lists the pictures it laid out this frame, in screen
         * pixels; into this space as ui_nav_frame takes them: 320 high.
         * Kind 1 is the fighter's VERSUS art alone, with its transparency,
         * standing out of a player banner; kind 0 the profile's icon (the
         * art on the profile's gradient, as the launcher draws it). */
        extern int limeScreenHeight;
        float k = limeScreenHeight > 0 ? 320.0f / (float)limeScreenHeight : 1.0f;
        extern int   PortOnlinePicN;
        extern char  PortOnlinePicCode[][16];   /* "" this player */
        extern float PortOnlinePicPx[][4];      /* x, y, size, opacity */
        extern int   PortOnlinePicKind[];
        extern int   PortOnlineDraftLook[6];    /* MY PROFILE's draft: code "*" */
        int port_online_friend_look(const char *code, int *icon, unsigned *color, int *dark,
                                    unsigned *color2, int *angle, int *alt);
        static unsigned char *px[UI_PICTURES];
        static unsigned have[UI_PICTURES];
        int i;

        for (i = 0; i < PortOnlinePicN && i < UI_PICTURES; i++) {
            struct look fl;
            unsigned key;
            const float *p = PortOnlinePicPx[i];
            int art = PortOnlinePicKind[i] == 1;

            if (!PortOnlinePicCode[i][0])
                fl = ml;
            else if (PortOnlinePicCode[i][0] == '*') {
                memset(&fl, 0, sizeof fl);
                fl.icon = PortOnlineDraftLook[0];
                fl.alt = PortOnlineDraftLook[1];
                fl.color = (unsigned)PortOnlineDraftLook[2];
                fl.dark = PortOnlineDraftLook[3];
                fl.color2 = (unsigned)PortOnlineDraftLook[4];
                fl.angle = PortOnlineDraftLook[5];
            } else if (PortOnlinePicCode[i][0] == '#') {    /* a fighter by number */
                memset(&fl, 0, sizeof fl);
                fl.icon = atoi(PortOnlinePicCode[i] + 1);
                fl.angle = -1;
            } else {
                memset(&fl, 0, sizeof fl);
                fl.angle = -1;
                port_online_friend_look(PortOnlinePicCode[i], &fl.icon, &fl.color, &fl.dark,
                                        &fl.color2, &fl.angle, &fl.alt);
            }
            key = (key_of(PortOnlinePicCode[i], &fl) ^ (art ? 0x9e3779b9u : 0u)) | 1u;
            if (key != have[i] || !px[i]) {
                free(px[i]);
                px[i] = art ? prof_art_rgba(fl.icon, fl.alt)
                            : prof_icon_rgba(fl.icon, fl.alt, fl.color, fl.dark, fl.color2,
                                             fl.angle, PIC_PX);
                have[i] = key;
            }
            if (px[i])
                ui_picture_at(i, px[i], art ? 256 : PIC_PX, art ? 256 : PIC_PX, key,
                              p[0] * k, p[1] * k, p[2] * k, p[2] * k, p[3]);
        }
        (void)w;
        return;
    }
    if (GameMode != 1 || !o || mpnet_connection_state() != 2)
        return;
    peer_look(o, &ol);

    if (CurrentTask == 3 && FE_CurrentTask == 0x2a) {
        badge(0, me->name, &ml, 4.0f, 6.0f, 0, 0.8f);
        badge(1, o->name, &ol, w - 4.0f, 6.0f, 2, 0.8f);
    } else if (CurrentTask == 3 && FE_CurrentTask == 0x2b) {
        /* the versus screen: player 1 (the host) left. The fight's names
         * are the HUD's (online_fight_names_hud, below). */
        int host = mpnet_am_server();

        badge(2, host ? me->name : o->name, host ? &ml : &ol, 4.0f, 6.0f, 0, 0.8f);
        badge(3, host ? o->name : me->name, host ? &ol : &ml, w - 4.0f, 6.0f, 2, 0.8f);
    }
}

/* ------------------------------------------------------- the fight's names
 *
 * In the fight the players' names stand alone (Mary, 2026-11-12: no profile
 * icon, which could be taken for the fighter's), in the type of the
 * fighters' names on the life bars -- the game's NameFont -- on a plate in
 * the dress of the ONLINE screen (FrontEnd.c OnPanel): dark stone, a beam
 * of gold on top that flares at the corners, a bronze foot, an oxblood line
 * under the beam. They sit in the strip above the life bars, player 1 (the
 * host) on the left: beside the moves (i) and pause buttons in the top
 * corners, or right against the screen's edges when those are hidden, and
 * short of the clock. DrawHUD (GameCode.c) calls online_fight_names_hud with
 * its scale, so the names are drawn with the HUD, under the pause menu. */

typedef struct TEXTURE TEXTURE;
TEXTURE *limeTextureFromRGBA(const char *key, const unsigned char *rgba, int w, int h);
void  limeDrawSprite(TEXTURE *tex, float x, float y, float w, float h,
                     float u0, float v0, float u1, float v1, const float *colour);
void  limeDrawFONT(void *font, const char *text, float x, float y,
                   long align, float scale, const float *colour);
float limeGetStringWidth(void *font, const char *text);
extern int limeScreenWidth;
extern int port_corner_buttons;         /* game_main.c: the (i) and pause */

#define NP_H     17.0f                  /* the plate, HUD points */
#define NP_Y      3.0f                  /* its top, below the screen's edge */
#define NP_EDGE   2.0f                  /* its margin for the beam's flare */

static float np_clamp(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }

/* coverage of pixel (x, y) by the box x0..x1, y0..y1, anti-aliased */
static float np_box(float x, float y, float x0, float y0, float x1, float y1)
{
    return np_clamp(x + 1 - x0) * np_clamp(x1 - x) * np_clamp(y + 1 - y0) * np_clamp(y1 - y);
}

/* gold as a metal bar lit from above: t 0 at its top, 1 at its foot */
static void np_gold(float t, float *c)
{
    static const float k[4][3] = {
        { 1.00f, 0.93f, 0.66f }, { 0.86f, 0.66f, 0.28f },
        { 0.55f, 0.37f, 0.12f }, { 0.78f, 0.58f, 0.24f } };
    float u = np_clamp(t) * 3;
    int i = u >= 3 ? 2 : (int)u, j;
    float f = u - (float)i;

    for (j = 0; j < 3; j++)
        c[j] = k[i][j] + (k[i + 1][j] - k[i][j]) * f;
}

/* premultiplied "over" */
static void np_over(float *d, float r, float g, float b, float a)
{
    d[0] = r * a + d[0] * (1 - a);
    d[1] = g * a + d[1] * (1 - a);
    d[2] = b * a + d[2] * (1 - a);
    d[3] = a + d[3] * (1 - a);
}

/* The plate, `w` x `h` pixels inside a margin of `m` all round (the beam's
 * flare, the drop shadow); `s` pixels a point. `mirror`: the name's end on
 * the right (player 2). Straight RGBA, (w + 2m) x (h + 2m), malloc'd. */
static unsigned char *np_make(int w, int h, int m, float s, int mirror)
{
    int W = w + 2 * m, H = h + 2 * m, x, y;
    float x0 = (float)m, y0 = (float)m, x1 = (float)(m + w), y1 = (float)(m + h);
    float beam = 2.6f * s, foot = 1.6f * s;
    float *px = (float *)calloc((size_t)W * H * 4, sizeof(float));
    unsigned char *out;

    if (!px)
        return NULL;
    for (y = 0; y < H; y++)
        for (x = 0; x < W; x++) {
            float *d = &px[(y * W + x) * 4];
            float fx = (float)x, fy = (float)y, c, t, u, g[3];

            /* u: 0 at the name's end, 1 at the inner end (the clock's) */
            u = np_clamp((fx + 0.5f - x0) / (x1 - x0));
            if (mirror)
                u = 1 - u;
            /* the drop shadow */
            c = np_box(fx, fy - 1.5f * s, x0 - s, y0, x1 + s, y1 + s);
            if (c > 0)
                np_over(d, 0, 0, 0, c * 0.45f);
            /* the stone: warm black, lit from above, deeper at the foot,
             * a little see-through towards the clock */
            c = np_box(fx, fy, x0, y0 + beam, x1, y1);
            if (c > 0) {
                unsigned hs = (unsigned)x * 73856093u ^ (unsigned)y * 19349663u;
                float n = (float)((hs >> 8) & 255) / 255.0f - 0.5f, l;
                t = (fy + 0.5f - y0) / (y1 - y0);
                l = 0.15f * (1 - t) + 0.05f + n * 0.025f;
                np_over(d, l * 1.05f, l * 0.92f, l * 0.74f,
                        c * (0.92f - 0.22f * (u > 0.7f ? (u - 0.7f) / 0.3f : 0)));
            }
            /* the fine gold line inside the frame */
            c = np_box(fx, fy, x0 + 1.4f * s, y0 + beam + 1.2f * s, x1 - 1.4f * s, y1 - foot - 0.6f * s)
              - np_box(fx, fy, x0 + 1.9f * s, y0 + beam + 1.7f * s, x1 - 1.9f * s, y1 - foot - 1.1f * s);
            if (c > 0)
                np_over(d, 0.72f, 0.56f, 0.28f, c * 0.35f);
            /* the oxblood line under the beam, fading towards the clock */
            c = np_box(fx, fy, x0 + 2 * s, y0 + beam + 0.4f * s, x1 - 2 * s, y0 + beam + 1.2f * s);
            if (c > 0)
                np_over(d, 0.62f, 0.12f, 0.07f, c * (1 - u) * 0.9f);
            /* the bronze foot */
            c = np_box(fx, fy, x0 - 0.6f * s, y1 - foot, x1 + 0.6f * s, y1);
            if (c > 0) {
                np_gold((fy + 0.5f - (y1 - foot)) / foot * 0.6f + 0.3f, g);
                np_over(d, g[0] * 0.62f, g[1] * 0.5f, g[2] * 0.42f, c);
            }
            /* the gold beam on top, flared at the ends */
            {
                float tb = (fy + 0.5f - y0) / beam, flare = 2 * s * (1 - np_clamp(tb));
                c = np_box(fx, fy, x0 - flare, y0, x1 + flare, y0 + beam + 0.3f * s);
                if (c > 0) {
                    np_gold(tb, g);
                    np_over(d, g[0], g[1], g[2], c);
                }
            }
            /* the uprights: dark bronze at each end */
            c = np_box(fx, fy, x0 - 0.4f * s, y0 + beam, x0 + 0.9f * s, y1)
              + np_box(fx, fy, x1 - 0.9f * s, y0 + beam, x1 + 0.4f * s, y1);
            if (c > 0)
                np_over(d, 0.42f, 0.29f, 0.12f, np_clamp(c));
        }
    out = (unsigned char *)malloc((size_t)W * H * 4);
    if (out)
        for (x = 0; x < W * H; x++) {
            float a = px[x * 4 + 3], k = a > 0 ? 1.0f / a : 0;
            int j;
            for (j = 0; j < 3; j++) {
                float v = px[x * 4 + j] * k;
                out[x * 4 + j] = (unsigned char)(np_clamp(v) * 255 + 0.5f);
            }
            out[x * 4 + 3] = (unsigned char)(np_clamp(a) * 255 + 0.5f);
        }
    free(px);
    return out;
}

/* A name as NameFont can draw it: upper case, the Latin-1 letters with
 * accents (in UTF-8 or as single bytes) as their plain ones, anything else
 * a '?' */
static void np_text(const char *s8, char *o, int n)
{
    static const char lat[] =           /* U+00C0..U+00FF */
        "AAAAAAACEEEEIIII" "DNOOOOO*OUUUUYTS" "AAAAAAACEEEEIIII" "DNOOOOO/OUUUUYTY";
    const unsigned char *p = (const unsigned char *)(s8 ? s8 : "");
    int i = 0;

    while (*p && i < n - 1) {
        unsigned c = *p;
        if (c < 0x80) {
            o[i++] = (char)(c >= 'a' && c <= 'z' ? c - 32 : c);
            p++;
            continue;
        }
        if ((c & 0xe0) == 0xc0 && (p[1] & 0xc0) == 0x80) {
            unsigned u = (c & 0x1f) << 6 | (p[1] & 0x3f);
            o[i++] = u >= 0xc0 && u <= 0xff ? lat[u - 0xc0] : '?';
            p += 2;
            continue;
        }
        if (c >= 0xc0) {                /* a Latin-1 byte (not UTF-8) */
            o[i++] = lat[c - 0xc0];
            p++;
            continue;
        }
        o[i++] = '?';
        for (p++; (*p & 0xc0) == 0x80; p++)
            ;
    }
    o[i] = 0;
    if (!i) {
        o[0] = '-';
        o[1] = 0;
    }
}

static void np_draw(void *font, float s, const char *name, int right)
{
    static const float white[4] = { 1, 1, 1, 1 };
    static const float gold[4] = { 1.0f, 0.86f, 0.52f, 1.0f };
    static TEXTURE *tex[2];
    static int have_w[2], have_h[2];
    char text[100], key[64];
    float edge = port_corner_buttons ? 33.0f : 3.0f;
    float room = (float)limeScreenWidth / s * 0.5f - 30.0f - edge;  /* to the clock */
    float pad = 11.0f, k = 0.78f, tw, pw, x;
    int wpx, hpx, mpx;
    unsigned char *px;
    TEXTURE *t;

    np_text(name, text, sizeof text);
    tw = limeGetStringWidth(font, text) * k;
    if (tw + 2 * pad > room && tw > 0) {        /* a long name, smaller */
        k *= (room - 2 * pad) / tw;
        tw = limeGetStringWidth(font, text) * k;
    }
    pw = tw + 2 * pad;
    if (pw < 72.0f)
        pw = 72.0f;
    /* the plate, made at the screen's pixels: one per size, kept */
    wpx = (int)(pw * s + 0.5f);
    hpx = (int)(NP_H * s + 0.5f);
    mpx = (int)(NP_EDGE * s + 1.5f);
    if (wpx != have_w[right] || hpx != have_h[right]) {
        /* the renderer keeps a texture by its key: one per size, which a
         * fight makes once or twice */
        snprintf(key, sizeof key, "port:nameplate-%dx%d-%d", wpx, hpx, right);
        tex[right] = NULL;
        px = np_make(wpx, hpx, mpx, s, right);
        if (px) {
            tex[right] = limeTextureFromRGBA(key, px, wpx + 2 * mpx, hpx + 2 * mpx);
            free(px);
        }
        have_w[right] = wpx;
        have_h[right] = hpx;
    }
    t = tex[right];
    x = right ? (float)limeScreenWidth - (edge + pw) * s : edge * s;
    if (t)
        limeDrawSprite(t, x - (float)mpx, NP_Y * s - (float)mpx,
                       (float)(wpx + 2 * mpx), (float)(hpx + 2 * mpx),
                       0.0f, 0.0f, 1.0f, 1.0f, white);
    /* the name, centred in the stone with room above and below, clear of
     * the frame (Mary) */
    limeDrawFONT(font, text, x + pw * 0.5f * s, (NP_Y + 5.0f) * s, 1, k * s, gold);
}

/* Called by DrawHUD (GameCode.c, port) while the fight is not paused, with
 * NameFont and the HUD's scale (pixels a point). UMK3_NAMES_PREVIEW=<name>
 * draws them in any fight, against a made-up player of that name, for a
 * screenshot. */
void online_fight_names_hud(void *font, float s)
{
    const struct profile *me = prof_get();
    const struct mpnet_peer *o = mpnet_peer();
    const char *e = getenv("UMK3_NAMES_PREVIEW"), *theirs;
    int host;

    if (!me || !font || s <= 0)
        return;
    if (e) {
        theirs = e[0] ? e : "RIVAL";
        host = 1;
    } else if (GameMode == 1 && o && mpnet_connection_state() == 2) {
        theirs = o->name;
        host = mpnet_am_server();
    } else
        return;
    np_draw(font, s, host ? me->name : theirs, 0);
    np_draw(font, s, host ? theirs : me->name, 1);
}
