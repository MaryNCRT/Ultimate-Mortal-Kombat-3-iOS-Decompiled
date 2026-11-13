/* palettes.c -- colours 3 to 6 from a fighter's own sheets. Port, not in the
 * binary. See palettes.h.
 *
 * aditional textures/palettes.ini, one section a fighter by its sheets'
 * name (PlayerDefs texBase):
 *
 *     [KUNGLAO]
 *     base = 2            the sheet the colours start from: 1 or 2
 *     hue = 196           the clothes' hue in it, in degrees
 *     pick = 26           how far from that hue a pixel still changes
 *     sat_min = 0.30      below this saturation a pixel stays (skin, greys)
 *     3 = 118 1.00 1.30   colour 3: its hue, saturation x, value x
 *     6 = -1 0.00 1.05    -1 keeps the hue (a grey has none)
 *
 * A pixel's hue moves by (new hue - hue), as a turn of the hue wheel does
 * in a paint program's Hue / Saturation / Value, so the cloth's shading and
 * its small changes of hue stay; the pixels at the edge of the picked hues
 * and saturations change less, so there is no hard line around them. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "palettes.h"

const char *lime_platform_asset_root(void);

typedef struct {
    char  name[32];
    int   base;
    float hue, pick, sat_min;
    int   has[PAL_MAX + 1];
    float col[PAL_MAX + 1][3];          /* hue (-1 keep), sat x, value x */
} PalDef;

#define PAL_DEFS 64
static PalDef g_def[PAL_DEFS];
static int    g_ndef, g_read;

static char *trim(char *s)
{
    char *e;

    while (*s == ' ' || *s == '\t')
        s++;
    e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n'))
        *--e = 0;
    return s;
}

static void read_file(const char *path)
{
    FILE *f = fopen(path, "r");
    char line[256];
    PalDef *d = NULL;

    if (!f)
        return;
    while (fgets(line, sizeof line, f)) {
        char *s = trim(line), *eq, *c;

        if ((c = strchr(s, ';')) != NULL || (c = strchr(s, '#')) != NULL)
            *c = 0;
        s = trim(s);
        if (!*s)
            continue;
        if (*s == '[') {
            char *z = strchr(s, ']');
            int i;

            if (!z)
                continue;
            *z = 0;
            d = NULL;
            for (i = 0; i < g_ndef; i++)        /* a later file adds to it */
                if (_stricmp(g_def[i].name, s + 1) == 0)
                    d = &g_def[i];
            if (!d && g_ndef < PAL_DEFS) {
                d = &g_def[g_ndef++];
                memset(d, 0, sizeof *d);
                snprintf(d->name, sizeof d->name, "%s", s + 1);
                for (i = 0; d->name[i]; i++)
                    d->name[i] = (char)toupper((unsigned char)d->name[i]);
                d->base = 2;
                d->hue = -1.0f;
                d->pick = 25.0f;
                d->sat_min = 0.3f;
            }
            continue;
        }
        if (!d || (eq = strchr(s, '=')) == NULL)
            continue;
        *eq = 0;
        c = trim(s);
        s = trim(eq + 1);
        if (_stricmp(c, "base") == 0)
            d->base = atoi(s) == 1 ? 1 : 2;
        else if (_stricmp(c, "hue") == 0)
            d->hue = (float)atof(s);
        else if (_stricmp(c, "pick") == 0)
            d->pick = (float)atof(s);
        else if (_stricmp(c, "sat_min") == 0)
            d->sat_min = (float)atof(s);
        else if (c[0] >= '3' && c[0] <= '0' + PAL_MAX && c[1] == 0) {
            int n = c[0] - '0';
            float h = -1, sx = 1, vx = 1;

            if (sscanf(s, "%f %f %f", &h, &sx, &vx) >= 1) {
                d->has[n] = 1;
                d->col[n][0] = h;
                d->col[n][1] = sx;
                d->col[n][2] = vx;
            }
        }
    }
    fclose(f);
}

static void load(void)
{
    char path[1200];
    const char *root;

    if (g_read)
        return;
    g_read = 1;
    root = lime_platform_asset_root();
    if (!root)
        return;
    snprintf(path, sizeof path, "%s/../aditional textures/palettes.ini", root);
    read_file(path);
    /* the player's own, beside umk3.ini: over the shipped one */
    snprintf(path, sizeof path, "%s/../palettes.ini", root);
    read_file(path);
    if (g_ndef)
        printf("palettes: %d fighters with colours past 2\n", g_ndef);
}

/* the section for a sheet or a fighter: the longest name `name` starts with */
static const PalDef *find(const char *name)
{
    const PalDef *best = NULL;
    size_t bl = 0;
    int i;

    load();
    if (!name)
        return NULL;
    {
        const char *b = name + strlen(name);
        while (b > name && b[-1] != '/' && b[-1] != '\\')
            b--;
        name = b;
    }
    while (name[0] == '@') {            /* past any virtual marks */
        const char *z = strchr(name + 1, '@');
        if (!z)
            break;
        name = z + 1;
    }
    for (i = 0; i < g_ndef; i++) {
        size_t l = strlen(g_def[i].name);
        if (l > bl && _strnicmp(name, g_def[i].name, l) == 0) {
            best = &g_def[i];
            bl = l;
        }
    }
    return best;
}

int pal_count(const char *texBase)
{
    const PalDef *d = find(texBase);
    int n = 2;

    if (!d || d->hue < 0.0f)
        return 2;
    while (n < PAL_MAX && d->has[n + 1])
        n++;
    return n;
}

int pal_base(const char *texBase)
{
    const PalDef *d = find(texBase);
    return d ? d->base : 2;
}

int pal_strip(const char *name, const char **rest)
{
    int n;

    *rest = name;
    if (!name || name[0] != '@' || name[1] != 'P' || name[2] < '3'
        || name[2] > '0' + PAL_MAX || name[3] != '@')
        return 0;
    n = name[2] - '0';
    *rest = name + 4;
    return n;
}

void pal_prefix(int n, char *out, size_t size)
{
    snprintf(out, size, "@P%d@", n);
}

static void rgb_hsv(float r, float g, float b, float *h, float *s, float *v)
{
    float mx = r > g ? (r > b ? r : b) : (g > b ? g : b);
    float mn = r < g ? (r < b ? r : b) : (g < b ? g : b);
    float d = mx - mn;

    *v = mx;
    *s = mx > 0.0f ? d / mx : 0.0f;
    if (d <= 0.0f)
        *h = 0.0f;
    else if (mx == r)
        *h = 60.0f * fmodf((g - b) / d + 6.0f, 6.0f);
    else if (mx == g)
        *h = 60.0f * ((b - r) / d + 2.0f);
    else
        *h = 60.0f * ((r - g) / d + 4.0f);
}

static void hsv_rgb(float h, float s, float v, float *r, float *g, float *b)
{
    float c = v * s, hp = fmodf(h / 60.0f + 6.0f, 6.0f);
    float x = c * (1.0f - fabsf(fmodf(hp, 2.0f) - 1.0f)), m = v - c;
    float R = 0, G = 0, B = 0;

    if (hp < 1)      { R = c; G = x; }
    else if (hp < 2) { R = x; G = c; }
    else if (hp < 3) { G = c; B = x; }
    else if (hp < 4) { G = x; B = c; }
    else if (hp < 5) { R = x; B = c; }
    else             { R = c; B = x; }
    *r = R + m;
    *g = G + m;
    *b = B + m;
}

static float smooth(float e0, float e1, float x)
{
    float t = (x - e0) / (e1 - e0);

    t = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
    return t * t * (3.0f - 2.0f * t);
}

int pal_apply(const char *name, int n, uint8_t *rgba, int w, int h)
{
    const PalDef *d = find(name);
    float nh, sx, vx, feather = 10.0f;
    size_t i, count = (size_t)w * (size_t)h;

    if (!d || n < 3 || n > PAL_MAX || !d->has[n] || d->hue < 0.0f || !rgba)
        return 0;
    nh = d->col[n][0];
    sx = d->col[n][1];
    vx = d->col[n][2];
    for (i = 0; i < count; i++) {
        uint8_t *p = rgba + i * 4;
        float r = p[0] / 255.0f, g = p[1] / 255.0f, b = p[2] / 255.0f;
        float H, S, V, dh, wt, R, G, B;

        rgb_hsv(r, g, b, &H, &S, &V);
        dh = fabsf(fmodf(H - d->hue + 540.0f, 360.0f) - 180.0f);
        wt = (1.0f - smooth(d->pick, d->pick + feather, dh))
           * smooth(d->sat_min * 0.6f, d->sat_min, S);
        if (wt <= 0.0f)
            continue;
        if (nh >= 0.0f)
            H = fmodf(H + (nh - d->hue) + 720.0f, 360.0f);
        S = S * sx;
        V = V * vx;
        S = S > 1.0f ? 1.0f : S;
        V = V > 1.0f ? 1.0f : V;
        hsv_rgb(H, S, V, &R, &G, &B);
        p[0] = (uint8_t)((r + (R - r) * wt) * 255.0f + 0.5f);
        p[1] = (uint8_t)((g + (G - g) * wt) * 255.0f + 0.5f);
        p[2] = (uint8_t)((b + (B - b) * wt) * 255.0f + 0.5f);
    }
    return 1;
}
