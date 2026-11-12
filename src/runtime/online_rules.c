/* online_rules.c -- the rules block of an online fight. Port, not in the
 * binary; see online_rules.h. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "online_rules.h"

int PortRoundTime;                      /* 0: the binary's 99 */

static online_rules g_active;           /* the online fight's (the lobby's) */
static int g_on;
static online_rules g_local;            /* the local versus's (its rules window) */
static int g_has_local;
static int g_use;                       /* 0 not chosen, 1 online, 6 local, -1 none */

void rules_default(online_rules *r)
{
    r->version = ONLINE_RULES_VERSION;
    r->best_of = 3;                     /* the binary's WinsNeeded 2 */
    r->series = 1;                      /* one fight */
    r->time = 99;
    r->kode = -1;
    r->kombo_slide = 1;                 /* the original's rule */
    r->host_adv = 1;                    /* as today */
}

static int valid(const online_rules *r)
{
    return r->version == ONLINE_RULES_VERSION
        && (r->best_of == 1 || r->best_of == 3 || r->best_of == 5)
        && (r->series == 1 || r->series == 3 || r->series == 5)
        && (r->time == 0 || r->time == 30 || r->time == 60 || r->time == 99)
        && r->kode >= -1 && r->kode <= 19
        && (r->kombo_slide == 0 || r->kombo_slide == 1)
        && (r->host_adv == 0 || r->host_adv == 1);
}

int rules_encode(const online_rules *r, char *out, int n)
{
    return snprintf(out, (size_t)n, "r%d;b%d;m%d;t%d;k%d;s%d;h%d", r->version,
                    r->best_of, r->series, r->time, r->kode, r->kombo_slide, r->host_adv);
}

/* "r1;b3;m1;t99;k-1;s1;h1": fields in any order, all seven needed. */
int rules_decode(const char *s, online_rules *r)
{
    online_rules t;
    unsigned seen = 0;
    char buf[ONLINE_RULES_TEXT * 2], *p, *next;

    if (!s || strlen(s) >= sizeof buf)
        return 0;
    strcpy(buf, s);
    memset(&t, 0, sizeof t);
    for (p = buf; p; p = next) {
        char *end;
        long v;
        int bit;

        next = strchr(p, ';');
        if (next)
            *next++ = 0;
        if (!p[0] || !p[1])
            return 0;
        v = strtol(p + 1, &end, 10);
        if (*end)
            return 0;
        switch (p[0]) {
        case 'r': t.version = (int)v;     bit = 0; break;
        case 'b': t.best_of = (int)v;     bit = 1; break;
        case 'm': t.series = (int)v;      bit = 6; break;
        case 't': t.time = (int)v;        bit = 2; break;
        case 'k': t.kode = (int)v;        bit = 3; break;
        case 's': t.kombo_slide = (int)v; bit = 4; break;
        case 'h': t.host_adv = (int)v;    bit = 5; break;
        default:  return 0;
        }
        seen |= 1u << bit;
    }
    if (seen != 0x7f || !valid(&t))
        return 0;
    *r = t;
    return 1;
}

int rules_equal(const online_rules *a, const online_rules *b)
{
    return a->version == b->version && a->best_of == b->best_of
        && a->series == b->series
        && a->time == b->time && a->kode == b->kode
        && a->kombo_slide == b->kombo_slide && a->host_adv == b->host_adv;
}

void rules_describe(const online_rules *r, char *out, int n)
{
    char t[16], k[16];

    if (r->time == 0)
        snprintf(t, sizeof t, "INFINITE");
    else
        snprintf(t, sizeof t, "%d", r->time);
    if (r->kode < 0)
        snprintf(k, sizeof k, "NO KODE");
    else
        snprintf(k, sizeof k, "KODE %d", r->kode);
    snprintf(out, (size_t)n, "BEST OF %d ROUNDS, %s, TIME %s, %s, KOMBO SLIDE %s, HOST ADVANTAGE %s",
             r->best_of, r->series > 1 ? (r->series == 3 ? "BEST OF 3 FIGHTS" : "BEST OF 5 FIGHTS") : "ONE FIGHT",
             t, k, r->kombo_slide ? "ON" : "OFF", r->host_adv ? "ON" : "OFF");
}

void rules_set_active(const online_rules *r)
{
    char d[160];

    g_active = *r;
    g_on = 1;
    rules_describe(r, d, sizeof d);
    printf("online rules: %s\n", d);
}

void rules_clear_active(void)
{
    g_on = 0;
    PortRoundTime = 0;
    rules_series_reset();
}

/* ---- the series of fights ---- */

static int g_series[2];

int rules_series_needed(void)
{
    const online_rules *r = rules_active();
    return r && r->series > 1 ? (r->series + 1) / 2 : 0;
}

void rules_series_reset(void)
{
    g_series[0] = g_series[1] = 0;
}

int rules_series_score(int *p1, int *p2)
{
    int need = rules_series_needed();

    if (p1) *p1 = g_series[0];
    if (p2) *p2 = g_series[1];
    if (!need)
        return -1;
    return g_series[0] >= need ? 0 : g_series[1] >= need ? 1 : -1;
}

void rules_series_won(int side)
{
    int need = rules_series_needed();

    if (!need || side < 0 || side > 1)
        return;
    if (g_series[0] >= need || g_series[1] >= need)
        rules_series_reset();           /* the last one was over: a new series */
    g_series[side]++;
    printf("online rules: series %d - %d (first to %d)\n", g_series[0], g_series[1], need);
}

const online_rules *rules_active(void)
{
    static int env_read;

    /* UMK3_RULES=<text>: a test's block, for every versus and online fight */
    if (!env_read) {
        const char *e = getenv("UMK3_RULES");
        online_rules r;

        env_read = 1;
        if (e && *e) {
            if (rules_decode(e, &r)) {
                rules_set_active(&r);
                g_local = r;            /* the local versus's too */
                g_has_local = 1;
            } else
                printf("online rules: UMK3_RULES=%s is not a rules block\n", e);
        }
    }
    if (g_use == 6)
        return g_has_local ? &g_local : NULL;
    if (g_use < 0)
        return NULL;
    return g_on ? &g_active : NULL;
}

void rules_use_mode(int game_mode)
{
    int use = game_mode == 6 ? 6 : game_mode == 1 ? 1 : -1;

    if (use != g_use)
        rules_series_reset();           /* another kind of fight: a new series */
    g_use = use;
}

void rules_set_local(const online_rules *r)
{
    char d[160];

    rules_series_reset();               /* new rules: a new series */
    if (!r) {
        g_has_local = 0;
        printf("versus rules: the game's own\n");
        return;
    }
    g_local = *r;
    g_has_local = 1;
    rules_describe(r, d, sizeof d);
    printf("versus rules: %s\n", d);
}

const online_rules *rules_local(void)
{
    rules_active();                     /* UMK3_RULES read */
    return g_has_local ? &g_local : NULL;
}

int rules_wins_needed(int binary_value)
{
    const online_rules *r = rules_active();
    return r ? (r->best_of + 1) / 2 : binary_value;
}

int rules_kode(void)
{
    const online_rules *r = rules_active();
    return r ? r->kode : -1;
}

int rules_kombo_slide(int otherwise)
{
    const online_rules *r = rules_active();
    return r ? r->kombo_slide : otherwise;
}

void rules_fight_begin(void)
{
    const online_rules *r = rules_active();

    PortRoundTime = !r || r->time == 99 ? 0 : r->time == 0 ? -1 : r->time;
}
