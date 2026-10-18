/*
 * profiles.c -- the player profiles. Port, not in the binary: kept for the
 * leaderboards to come.
 *
 * profiles.ini, beside umk3.ini, is shared with the launcher (its
 * lib/profiles.js reads and writes the same line):
 *
 *     profile=7KQ2-9XMD-4TPA|MARY|18|3A8EE6|L
 *
 * One profile per installation: the id, the name (UTF-8, anything but "|"
 * and line breaks, up to 24 characters), the icon (a fighter 0..23) and the
 * colour (RRGGBB), and the gradient behind the fighter: L from white to
 * the colour, D from black to it (absent: L). The id is given once, the first time there is no
 * profile -- 60 random bits from the system's cryptographic generator
 * (rand_s) as twelve Crockford base-32 digits -- and never changes: RESET
 * puts the name, icon and colour back and keeps it.
 *
 * The icon is built from the game's own art, nothing imported: the fighter's
 * bio and endings picture (<NAME>_VERSUS.PNG, Versus_Names) over a glossy
 * Frutiger Aero gradient of the colour. prof_icon_rgba draws it with the
 * numbers the launcher's drawIcon uses (renderer.js), so both show the same.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "profiles.h"
#include "lime/png.h"

const char *lime_platform_asset_root(void);
#ifdef _WIN32
int __cdecl rand_s(unsigned int *v);    /* the UCRT's, from RtlGenRandom */
#else
#include <time.h>
static int rand_s(unsigned int *v)
{
    FILE *f = fopen("/dev/urandom", "rb");
    if (f) {
        size_t n = fread(v, sizeof(*v), 1, f);
        fclose(f);
        if (n == 1)
            return 0;
    }
    *v = (unsigned int)time(NULL) ^ (unsigned int)rand();
    return 0;
}
#endif

#define DEF_ICON  18                    /* Scorpion */
#define DEF_COLOR 0x3A8EE6

static struct profile g_prof;
static int  g_have;
static char g_path[1100];

/* The bio art, in CharacterNames order (Versus_Names, 0x000ffbf4). */
static const char *const k_art[PROF_ICONS] = {
    "KANO", "SONYA", "JAXX", "NIGHTWOLF", "SUBZERO_HUMAN", "STRYKER", "SINDEL",
    "SEKTOR", "CYRAX", "KUNGLAO", "KABAL", "SHEEVA", "SHANGTSUNG", "LUIKANG",
    "SMOKE", "KITANA", "JADE", "MILEENA", "SCORPION", "REPTILE", "ERMAC",
    "SUBZERO", "SMOKE_SECRET", "NOOBSAIBOT",
};

struct profile *prof_get(void) { return g_have ? &g_prof : NULL; }

int prof_name_len(const char *s)
{
    int n = 0;

    for (; *s; s++)
        if (((unsigned char)*s & 0xc0) != 0x80)
            n++;
    return n;
}

/* "|" and line breaks out, at most PROF_NAME_MAX characters. */
static void clean_name(char *d, size_t dn, const char *s)
{
    size_t k = 0;
    int chars = 0;

    for (; *s && k + 1 < dn; s++) {
        unsigned char c = (unsigned char)*s;
        if ((c & 0xc0) != 0x80 && ++chars > PROF_NAME_MAX)
            break;
        d[k++] = (c == '|' || c == '\r' || c == '\n' || c == '\t') ? ' ' : (char)c;
    }
    d[k] = 0;                           /* 24 characters fit: no half one */
}

static void new_id(char *out)
{
    static const char alpha[] = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";
    unsigned a = 0, b = 0;
    unsigned long long v;
    char s[13];
    int i;

    rand_s(&a);
    rand_s(&b);
    v = ((unsigned long long)a << 28) ^ b;      /* 60 bits used */
    for (i = 11; i >= 0; i--, v >>= 5)
        s[i] = alpha[v & 31];
    snprintf(out, 16, "%.4s-%.4s-%.4s", s, s + 4, s + 8);
}

void prof_save(void)
{
    FILE *f;

    if (!g_path[0] || !g_have)
        return;
    f = fopen(g_path, "wb");
    if (!f) {
        printf("profiles: cannot write %s\n", g_path);
        return;
    }
    fprintf(f, "; UMK3 player profile: the launcher and the game both edit this;\n");
    fprintf(f, "; the id belongs to this installation and never changes\n");
    fprintf(f, "profile=%s|%s|%d|%06X|%c|%06X\n", g_prof.id, g_prof.name, g_prof.icon,
            g_prof.color & 0xffffff, "LDC"[g_prof.dark >= 0 && g_prof.dark <= 2 ? g_prof.dark : 0],
            g_prof.color2 & 0xffffff);
    fclose(f);
}

void prof_load(const char *ini_path, const char *default_name)
{
    char line[512], *sl;
    FILE *f;

    snprintf(g_path, sizeof g_path, "%s", ini_path ? ini_path : "");
    sl = strrchr(g_path, '\\');
    if (!sl || (strrchr(g_path, '/') && strrchr(g_path, '/') > sl))
        sl = strrchr(g_path, '/');
    {
        /* UMK3_PROFILES: another file name in the same folder, so a second
         * game on this computer is another player (testing ONLINE) */
        const char *name = getenv("UMK3_PROFILES");
        if (!name || !*name || strchr(name, '/') || strchr(name, '\\'))
            name = "profiles.ini";
        if (sl)
            snprintf(sl + 1, sizeof g_path - (size_t)(sl + 1 - g_path), "%s", name);
        else
            snprintf(g_path, sizeof g_path, "%s", name);
    }

    g_have = 0;
    f = fopen(g_path, "rb");
    while (f && !g_have && fgets(line, sizeof line, f)) {
        char *e = line + strlen(line), *v, *fld[4];
        int k;

        while (e > line && (e[-1] == '\n' || e[-1] == '\r'))
            *--e = 0;
        if (strncmp(line, "profile=", 8) != 0)
            continue;
        v = line + 8;
        for (k = 0; k < 4; k++) {
            fld[k] = v;
            v = strchr(v, '|');
            if (!v && k < 3)
                break;
            if (v)
                *v++ = 0;
        }
        if (k < 4 || strlen(fld[0]) != 14)
            continue;
        snprintf(g_prof.id, sizeof g_prof.id, "%s", fld[0]);
        clean_name(g_prof.name, sizeof g_prof.name, fld[1]);
        g_prof.icon = atoi(fld[2]);
        if (g_prof.icon < 0 || g_prof.icon >= PROF_ICONS)
            g_prof.icon = DEF_ICON;
        g_prof.color = (unsigned)strtoul(fld[3], NULL, 16) & 0xffffff;
        /* the fifth field, if any: the loop left v on it */
        /* L, D or C, then |RRGGBB, the other end; an older file has only
         * L or D, or the custom one's RRGGBB alone */
        g_prof.dark = 0;
        if (v && strspn(v, "0123456789ABCDEFabcdef") >= 6) {
            g_prof.dark = 2;
            g_prof.color2 = (unsigned)strtoul(v, NULL, 16) & 0xffffff;
        } else {
            if (v && (*v == 'D' || *v == 'd'))
                g_prof.dark = 1;
            else if (v && (*v == 'C' || *v == 'c'))
                g_prof.dark = 2;
            g_prof.color2 = prof_shade_default(g_prof.dark);
            if (v && (v = strchr(v, '|')) != NULL && strspn(v + 1, "0123456789ABCDEFabcdef") >= 6)
                g_prof.color2 = (unsigned)strtoul(v + 1, NULL, 16) & 0xffffff;
        }
        g_have = 1;
    }
    if (f)
        fclose(f);
    if (!g_have) {                      /* the installation's first: its id */
        memset(&g_prof, 0, sizeof g_prof);
        new_id(g_prof.id);
        clean_name(g_prof.name, sizeof g_prof.name, default_name);
        g_prof.icon = DEF_ICON;
        g_prof.color = DEF_COLOR;
        g_prof.dark = 0;
        g_prof.color2 = prof_shade_default(0);
        g_have = 1;
        prof_save();
        printf("profiles: new profile %s\n", g_prof.id);
    }
}

void prof_reset(const char *default_name)
{
    if (!g_have)
        return;
    clean_name(g_prof.name, sizeof g_prof.name, default_name);
    g_prof.icon = DEF_ICON;
    g_prof.color = DEF_COLOR;
    g_prof.dark = 0;
    g_prof.color2 = prof_shade_default(0);
    prof_save();
}

unsigned prof_shade_default(int dark)
{
    return dark == 1 ? 0x000000 : dark == 2 ? 0x6E0B12 : 0xFFFFFF;
}

unsigned prof_look_key(const struct profile *p)
{
    unsigned k = 2166136261u;

    k = (k ^ (unsigned)p->icon) * 16777619u;
    k = (k ^ p->color) * 16777619u;
    k = (k ^ (unsigned)p->dark) * 16777619u;
    k = (k ^ p->color2) * 16777619u;
    return k;
}

/* ------------------------------------------------------------------ icon */

static float clampf(float v, float a, float b) { return v < a ? a : v > b ? b : v; }

/* Signed distance to a rounded square of half size h and corner r, centred
 * on (c, c). */
static float sd_round(float x, float y, float c, float h, float r)
{
    float qx = fabsf(x - c) - (h - r), qy = fabsf(y - c) - (h - r);
    float ox = qx > 0 ? qx : 0, oy = qy > 0 ? qy : 0;
    float in = qx > qy ? qx : qy;

    return sqrtf(ox * ox + oy * oy) + (in < 0 ? in : 0) - r;
}

unsigned char *prof_icon_rgba(int icon, unsigned color, int dark,
                              unsigned color2, int S)
{
    uint8_t *art = NULL;
    int aw = 0, ah = 0, x, y;
    unsigned char *out;
    float c[3], c2[3], top[3], mid[3], bot[3], glow[3], ga, g0, g1;
    char path[1200];
    const char *root = lime_platform_asset_root();

    if (icon < 0 || icon >= PROF_ICONS || S <= 0)
        return NULL;
    snprintf(path, sizeof path, "%s/Textures/%s_VERSUS.PNG", root ? root : "res",
             k_art[icon]);
    if (!lime_png_load(path, &art, &aw, &ah))
        art = NULL;                     /* the gradient alone */
    out = malloc((size_t)S * S * 4);
    if (!out) {
        free(art);
        return NULL;
    }
    c[0] = (float)((color >> 16) & 255);
    c[1] = (float)((color >> 8) & 255);
    c[2] = (float)(color & 255);
    c2[0] = (float)((color2 >> 16) & 255);
    c2[1] = (float)((color2 >> 8) & 255);
    c2[2] = (float)(color2 & 255);
    for (x = 0; x < 3; x++) {
        /* light: white into the colour, deeper at the bottom; dark: the
         * colour at the top going down to black (red into black); custom:
         * the colour at the top going down to the second one */
        if (dark == 2) {
            top[x]  = c[x] + (255 - c[x]) * 0.2f;
            mid[x]  = c[x] + (c2[x] - c[x]) * 0.5f;
            bot[x]  = c2[x];
            glow[x] = c2[x] + (255 - c2[x]) * 0.3f;
        } else if (dark) {
            /* the colour into color2, black by default */
            top[x]  = c[x] + (255 - c[x]) * 0.15f;
            mid[x]  = c[x] + (c2[x] - c[x]) * 0.45f;
            bot[x]  = c[x] + (c2[x] - c[x]) * 0.92f;
            glow[x] = c[x];
        } else {
            /* color2, white by default, into the colour */
            top[x]  = c[x] + (c2[x] - c[x]) * 0.55f;
            mid[x]  = c[x];
            bot[x]  = c[x] * 0.7f;
            glow[x] = c[x] + (c2[x] - c[x]) * 0.6f;
        }
    }
    ga = dark == 2 ? 0.45f : dark ? 0.35f : 0.75f;     /* glow */
    g0 = dark == 2 ? 0.45f : dark ? 0.35f : 0.55f;     /* gloss, top */
    g1 = dark == 2 ? 0.04f : dark ? 0.03f : 0.04f;     /* gloss, middle */

    for (y = 0; y < S; y++)
        for (x = 0; x < S; x++) {
            float px = x + 0.5f, py = y + 0.5f, t = py / S, col[3], a, k, e;
            float cov = clampf(0.5f - sd_round(px, py, S * 0.5f, S * 0.5f, S * 0.18f), 0, 1);
            float rim, ra, outa;
            int i;

            for (i = 0; i < 3; i++)             /* the sky */
                col[i] = t < 0.5f ? top[i] + (mid[i] - top[i]) * (t / 0.5f)
                                  : mid[i] + (bot[i] - mid[i]) * ((t - 0.5f) / 0.5f);
            k = sqrtf((px - S * 0.5f) * (px - S * 0.5f)
                      + (py - S * 1.05f) * (py - S * 1.05f));
            a = ga * clampf(1.0f - k / (S * 0.6f), 0, 1);   /* the glow */
            for (i = 0; i < 3; i++)
                col[i] += (glow[i] - col[i]) * a;
            if (art) {                          /* the fighter, bilinear */
                float sx = px * aw / S - 0.5f, sy = py * ah / S - 0.5f;
                int x0 = (int)floorf(sx), y0 = (int)floorf(sy), dx, dy;
                float fx = sx - x0, fy = sy - y0, acc[4] = { 0, 0, 0, 0 };

                for (dy = 0; dy < 2; dy++)
                    for (dx = 0; dx < 2; dx++) {
                        int ix = x0 + dx, iy = y0 + dy;
                        float w = (dx ? fx : 1 - fx) * (dy ? fy : 1 - fy), al;
                        const uint8_t *p;

                        if (ix < 0) ix = 0;
                        if (iy < 0) iy = 0;
                        if (ix >= aw) ix = aw - 1;
                        if (iy >= ah) iy = ah - 1;
                        p = art + ((size_t)iy * aw + ix) * 4;
                        al = p[3] / 255.0f * w;
                        acc[0] += p[0] * al;
                        acc[1] += p[1] * al;
                        acc[2] += p[2] * al;
                        acc[3] += al;
                    }
                for (i = 0; i < 3; i++)
                    col[i] = col[i] * (1 - acc[3]) + acc[i];
            }
            e = ((px - S * 0.5f) / (S * 0.72f)) * ((px - S * 0.5f) / (S * 0.72f))
              + (py / (S * 0.5f)) * (py / (S * 0.5f));
            if (e < 1.0f) {                     /* the gloss */
                float edge = clampf((1.0f - sqrtf(e)) * S * 0.5f, 0, 1);
                a = (g0 + (g1 - g0) * clampf(py / (S * 0.5f), 0, 1)) * edge;
                for (i = 0; i < 3; i++)
                    col[i] += (255 - col[i]) * a;
            }
            rim = fabsf(sd_round(px, py, S * 0.5f, S * 0.4875f, S * 0.17f));
            ra = 0.7f * clampf(S * 0.0125f + 0.5f - rim, 0, 1);     /* the rim */
            outa = ra + cov * (1 - ra);
            for (i = 0; i < 3; i++) {
                float v = outa > 0 ? (255 * ra + col[i] * cov * (1 - ra)) / outa : 0;
                out[((size_t)y * S + x) * 4 + i] = (unsigned char)clampf(v + 0.5f, 0, 255);
            }
            out[((size_t)y * S + x) * 4 + 3] = (unsigned char)clampf(outa * 255 + 0.5f, 0, 255);
        }
    free(art);
    return out;
}
