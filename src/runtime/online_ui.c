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
 *   versus screen (FE 0x2b) and    player 1's on the left, player 2's on the
 *   the fight (CurrentTask 6)      right; the host is player 1
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

/* A player's banner, `right` measured from its right end */
static void banner(int slot, const char *name, const struct look *l, float x, float y,
                   float w, float h, int right)
{
    int look[6];

    look[0] = l->icon;
    look[1] = l->alt;
    look[2] = (int)l->color;
    look[3] = l->dark;
    look[4] = (int)l->color2;
    look[5] = l->angle;
    ui_player_banner(slot, name, NULL, look, right ? x - w : x, y, w, h);
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

    /* Mary (2026-11-12): the players in the banner's design, the ONLINE
     * screen's (FrontEnd.c PortSkinBandOf), no longer the old badge */
    if (CurrentTask == 3 && FE_CurrentTask == 0x2a) {
        /* above the roster's first row (y 30), the fighter inside the screen */
        banner(0, me->name, &ml, 10.0f, 9.0f, 116.0f, 19.0f, 0);
        banner(1, o->name, &ol, w - 10.0f, 9.0f, 116.0f, 19.0f, 1);
    } else if (CurrentTask == 3 && FE_CurrentTask == 0x2b) {
        /* the versus screen: player 1 (the host) left. The fight's names
         * are its own, gold plates from DrawHUD (PR #209) */
        int host = mpnet_am_server();

        banner(0, host ? me->name : o->name, host ? &ml : &ol, 10.0f, 9.0f, 116.0f, 19.0f, 0);
        banner(1, host ? o->name : me->name, host ? &ol : &ml, w - 10.0f, 9.0f, 116.0f, 19.0f, 1);
    }
    (void)badge;
}
