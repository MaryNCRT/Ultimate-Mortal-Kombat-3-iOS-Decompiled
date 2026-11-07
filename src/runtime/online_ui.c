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

    if (CurrentTask == 3 && FE_CurrentTask == 51) {
        /* FrontEnd.c says where, in screen pixels; into this space as
         * ui_nav_frame takes them: 320 high */
        extern int limeScreenHeight;
        float k = limeScreenHeight > 0 ? 320.0f / (float)limeScreenHeight : 1.0f;
        extern float PortOnlineMePx[3];     /* HOME's PROFILE: x, y, size (0 none) */
        extern int   PortOnlineFrShown;     /* FRIENDS' and INVITATIONS' rows */
        extern char  PortOnlineFrCode[][16];
        extern char  PortOnlineFrName[][32];
        extern float PortOnlineFrPx[][2];
        int port_online_friend_look(const char *code, int *icon, unsigned *color, int *dark,
                                    unsigned *color2, int *angle, int *alt);
        int i;

        if (PortOnlineMePx[2] > 0.0f)
            badge(0, me->name, &ml, PortOnlineMePx[0] * k, PortOnlineMePx[1] * k, 0,
                  PortOnlineMePx[2]);
        /* each friend's name and icon, left of INVITE / JOIN */
        for (i = 0; i < PortOnlineFrShown && i < 6; i++) {
            struct look fl;
            memset(&fl, 0, sizeof fl);
            fl.angle = -1;
            port_online_friend_look(PortOnlineFrCode[i], &fl.icon, &fl.color, &fl.dark,
                                    &fl.color2, &fl.angle, &fl.alt);
            badge(1 + i, PortOnlineFrName[i], &fl, PortOnlineFrPx[i][0] * k,
                  PortOnlineFrPx[i][1] * k, 0, 0.75f);
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
    } else if ((CurrentTask == 3 && FE_CurrentTask == 0x2b)
               || CurrentTask == 6) {
        /* the versus screen and the fight: player 1 (the host) left. In
         * the fight, above the life bars, beside the moves (i) and pause
         * buttons in the top corners, so they cover nothing. */
        int host = mpnet_am_server();
        int fight = CurrentTask == 6;
        float y = fight ? 2.0f : 6.0f;
        float k = fight ? 0.55f : 0.8f;
        float x = fight ? 26.0f : 4.0f;

        badge(2, host ? me->name : o->name, host ? &ml : &ol, x, y, 0, k);
        badge(3, host ? o->name : me->name, host ? &ol : &ml, w - x, y, 2, k);
    }
}
