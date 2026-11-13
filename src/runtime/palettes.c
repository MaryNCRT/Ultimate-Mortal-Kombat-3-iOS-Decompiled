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
 * Clothes with no colour (Human Smoke's grey) cannot be turned: `grey =
 * 0.15` picks the pixels less saturated than that instead, and each
 * colour's saturation is then the one they get (a tint), not a factor.
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
int port_tex_exists(const char *stem);  /* draw_gl.c: a sheet by that name */

typedef struct {
    char  name[32];
    int   base;
    float hue, pick, sat_min;
    float grey;                         /* >0: the greys are what changes */
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
        else if (_stricmp(c, "grey") == 0)
            d->grey = (float)atof(s);
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

/* Mary: six colours a fighter once their pictures are there -- palettes.ini
 * makes them, and a fighter it does not name has as many as the files
 * <texBase>_DIFFUSE3.. the player put in (the CHARACTER VIEWER's COLOR
 * slots), in a mod or res/Textures. Looked for once a fighter. */
int pal_count(const char *texBase)
{
    static struct { char name[32]; int n; } memo[40];
    static int nmemo;
    const PalDef *d = find(texBase);
    int n = 2, i;

    if (!texBase)
        return 2;
    if (d && (d->hue >= 0.0f || d->grey > 0.0f))
        while (n < PAL_MAX && d->has[n + 1])
            n++;
    for (i = 0; i < nmemo; i++)
        if (_stricmp(memo[i].name, texBase) == 0)
            return memo[i].n > n ? memo[i].n : n;
    {
        int f = 2;
        char stem[64];

        while (f < PAL_MAX) {
            snprintf(stem, sizeof stem, "%s_DIFFUSE%d", texBase, f + 1);
            if (!port_tex_exists(stem))
                break;
            f++;
        }
        if (nmemo < 40) {
            snprintf(memo[nmemo].name, sizeof memo[nmemo].name, "%s", texBase);
            memo[nmemo++].n = f;
        }
        return f > n ? f : n;
    }
}

/* 1 when palettes.ini has a section of this very name */
int pal_known(const char *name)
{
    const PalDef *d = find(name);
    return d && name && _stricmp(d->name, name) == 0;
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

    if (!d || n < 3 || n > PAL_MAX || !d->has[n] || (d->hue < 0.0f && d->grey <= 0.0f) || !rgba)
        return 0;
    nh = d->col[n][0];
    sx = d->col[n][1];
    vx = d->col[n][2];
    for (i = 0; i < count; i++) {
        uint8_t *p = rgba + i * 4;
        float r = p[0] / 255.0f, g = p[1] / 255.0f, b = p[2] / 255.0f;
        float H, S, V, dh, wt, R, G, B;

        rgb_hsv(r, g, b, &H, &S, &V);
        if (d->grey > 0.0f) {           /* a tint of the greys */
            wt = (1.0f - smooth(d->grey, d->grey * 1.6f, S)) * smooth(0.03f, 0.08f, V);
            if (wt <= 0.0f)
                continue;
            H = nh >= 0.0f ? nh : 0.0f;
            S = nh >= 0.0f ? sx : 0.0f;
            V = V * vx;
            S = S > 1.0f ? 1.0f : S;
            V = V > 1.0f ? 1.0f : V;
            hsv_rgb(H, S, V, &R, &G, &B);
            p[0] = (uint8_t)((r + (R - r) * wt) * 255.0f + 0.5f);
            p[1] = (uint8_t)((g + (G - g) * wt) * 255.0f + 0.5f);
            p[2] = (uint8_t)((b + (B - b) * wt) * 255.0f + 0.5f);
            continue;
        }
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

/* ------------------------------------------------- the colours as files
 *
 * Mary: once the game is built, colours 3..6 are also written out as
 * pictures beside the sheets they come from (the game's res/Textures, a
 * texture mod's folder), so whoever wants can paint over them; a picture
 * there is taken as it is and nothing is recoloured (draw_gl.c). */

int pal_fighters(void)
{
    load();
    return g_ndef;
}

const char *pal_fighter(int i)
{
    load();
    return i >= 0 && i < g_ndef ? g_def[i].name : NULL;
}

/* "KUNGLAO_DIFFUSE2" as colour 3: "KUNGLAO_DIFFUSE3" -- the sheet's "2" is
 * the colour's number (DIFFUSE2_LOW -> DIFFUSE3_LOW, DIFFUSE_LITE2 ->
 * DIFFUSE_LITE3, VERSUS2 -> VERSUS3, KUNGLAOBABY2 -> KUNGLAOBABY3); a
 * colour-1 sheet gets the number after DIFFUSE / VERSUS / BABY. 0 when the
 * name is none of those. `stem` has no extension. */
int pal_file_name(const char *stem, int n, char *out, size_t size)
{
    static const char *const tok[] = { "DIFFUSE", "VERSUS", "BABILITY", "BABY" };
    char up[256];
    size_t L = strlen(stem), i;

    if (L + 2 >= sizeof up || L + 2 >= size || n < 3 || n > PAL_MAX)
        return 0;
    for (i = 0; i <= L; i++)
        up[i] = (char)toupper((unsigned char)stem[i]);
    snprintf(out, size, "%s", stem);
    if ((L >= 5 && strcmp(up + L - 5, "LITE2") == 0)
        || (L >= 4 && strcmp(up + L - 4, "LOW2") == 0)) {
        out[L - 1] = (char)('0' + n);
        return 1;
    }
    for (i = 0; i < 4; i++) {
        const char *p = strstr(up, tok[i]);
        size_t at;

        if (!p)
            continue;
        at = (size_t)(p - up) + strlen(tok[i]);
        if (up[at] == '2')
            out[at] = (char)('0' + n);
        else {
            memmove(out + at + 1, out + at, L - at + 1);
            out[at] = (char)('0' + n);
        }
        return 1;
    }
    return 0;
}

/* A small PNG writer: rows filtered by Sub, deflate with LZ77 and the fixed
 * Huffman codes, zlib's wrapper. Not as small as zlib's best, a fraction of
 * the raw size. */
typedef struct { uint8_t *p; size_t n, cap; uint32_t bits; int nb; } Out;

static int out_grow(Out *o, size_t more)
{
    if (o->n + more <= o->cap)
        return 1;
    {
        size_t c = o->cap ? o->cap * 2 : (size_t)1 << 16;
        uint8_t *q;
        while (c < o->n + more)
            c *= 2;
        q = (uint8_t *)realloc(o->p, c);
        if (!q)
            return 0;
        o->p = q;
        o->cap = c;
    }
    return 1;
}

static void put_bits(Out *o, uint32_t v, int n)    /* LSB first */
{
    o->bits |= v << o->nb;
    o->nb += n;
    while (o->nb >= 8) {
        if (out_grow(o, 1))
            o->p[o->n++] = (uint8_t)o->bits;
        o->bits >>= 8;
        o->nb -= 8;
    }
}

static void put_huff(Out *o, uint32_t code, int len) /* MSB first */
{
    uint32_t r = 0;
    int i;

    for (i = 0; i < len; i++)
        r |= ((code >> i) & 1u) << (len - 1 - i);
    put_bits(o, r, len);
}

static void put_lit(Out *o, int c)
{
    if (c < 144)      put_huff(o, (uint32_t)(0x30 + c), 8);
    else if (c < 256) put_huff(o, (uint32_t)(0x190 + c - 144), 9);
    else if (c < 280) put_huff(o, (uint32_t)(c - 256), 7);
    else              put_huff(o, (uint32_t)(0xc0 + c - 280), 8);
}

static void put_match(Out *o, int len, int dist)
{
    static const int lb[29] = { 3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,
                                67,83,99,115,131,163,195,227,258 };
    static const int le[29] = { 0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0 };
    static const int db[30] = { 1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,
                                1025,1537,2049,3073,4097,6145,8193,12289,16385,24577 };
    static const int de[30] = { 0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13 };
    int i = 28, j = 29;

    while (lb[i] > len)
        i--;
    put_lit(o, 257 + i);
    if (le[i])
        put_bits(o, (uint32_t)(len - lb[i]), le[i]);
    while (db[j] > dist)
        j--;
    put_huff(o, (uint32_t)j, 5);
    if (de[j])
        put_bits(o, (uint32_t)(dist - db[j]), de[j]);
}

#define WBITS 15
#define WSIZE (1 << WBITS)
#define HBITS 15
#define HASH3(p) ((((uint32_t)(p)[0] << 16 | (uint32_t)(p)[1] << 8 | (p)[2]) * 2654435761u) >> (32 - HBITS))

static int deflate_fixed(const uint8_t *in, size_t n, Out *o)
{
    int32_t *head = (int32_t *)malloc(sizeof(int32_t) << HBITS);
    int32_t *prev = (int32_t *)malloc(sizeof(int32_t) * WSIZE);
    size_t i = 0;

    if (!head || !prev) {
        free(head);
        free(prev);
        return 0;
    }
    memset(head, 0xff, sizeof(int32_t) << HBITS);
    put_bits(o, 1, 1);                  /* the last block */
    put_bits(o, 1, 2);                  /* fixed Huffman */
    while (i < n) {
        int best = 0, bd = 0;

        if (i + 3 <= n) {
            uint32_t h = HASH3(in + i);
            int32_t c = head[h];
            int chain = 48;

            while (c >= 0 && i - (size_t)c <= WSIZE - 1 && chain-- > 0) {
                size_t k = 0, lim = n - i < 258 ? n - i : 258;
                while (k < lim && in[(size_t)c + k] == in[i + k])
                    k++;
                if ((int)k > best) {
                    best = (int)k;
                    bd = (int)(i - (size_t)c);
                    if (k == lim)
                        break;
                }
                c = prev[c & (WSIZE - 1)];
            }
            prev[i & (WSIZE - 1)] = head[h];
            head[h] = (int32_t)i;
        }
        if (best >= 3) {
            size_t e = i + (size_t)best, k;
            put_match(o, best, bd);
            for (k = i + 1; k < e && k + 3 <= n; k++) {   /* the hash keeps up */
                uint32_t h = HASH3(in + k);
                prev[k & (WSIZE - 1)] = head[h];
                head[h] = (int32_t)k;
            }
            i = e;
        } else
            put_lit(o, in[i++]);
    }
    put_lit(o, 256);
    if (o->nb)
        put_bits(o, 0, 8 - o->nb);
    free(head);
    free(prev);
    return 1;
}

static uint32_t crc_of(const uint8_t *p, size_t n, uint32_t c)
{
    static uint32_t t[256];
    size_t i;

    if (!t[1]) {
        uint32_t k, v;
        int b;
        for (k = 0; k < 256; k++) {
            v = k;
            for (b = 0; b < 8; b++)
                v = (v & 1) ? 0xedb88320u ^ (v >> 1) : v >> 1;
            t[k] = v;
        }
    }
    c = ~c;
    for (i = 0; i < n; i++)
        c = t[(c ^ p[i]) & 255] ^ (c >> 8);
    return ~c;
}

static void be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);  p[3] = (uint8_t)v;
}

static int chunk(FILE *f, const char *type, const uint8_t *d, size_t n)
{
    uint8_t h[8], c[4];
    uint32_t crc;

    be32(h, (uint32_t)n);
    memcpy(h + 4, type, 4);
    crc = crc_of(h + 4, 4, 0);
    if (n)
        crc = crc_of(d, n, crc);
    be32(c, crc);
    return fwrite(h, 1, 8, f) == 8 && (n == 0 || fwrite(d, 1, n, f) == n)
           && fwrite(c, 1, 4, f) == 4;
}

int pal_png_write(const char *path, const uint8_t *rgba, int w, int h)
{
    static const uint8_t sig[8] = { 0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a };
    size_t row = (size_t)w * 4 + 1, n = row * (size_t)h, i;
    uint8_t *raw, ihdr[13];
    Out o;
    uint32_t a = 1, b = 0;
    FILE *f;
    int ok;

    if (w <= 0 || h <= 0 || (raw = (uint8_t *)malloc(n)) == NULL)
        return 0;
    for (i = 0; i < (size_t)h; i++) {   /* filter Sub */
        const uint8_t *s = rgba + i * (size_t)w * 4;
        uint8_t *d = raw + i * row;
        size_t x;
        d[0] = 1;
        for (x = 0; x < (size_t)w * 4; x++)
            d[1 + x] = (uint8_t)(s[x] - (x >= 4 ? s[x - 4] : 0));
    }
    memset(&o, 0, sizeof o);
    if (!out_grow(&o, 2)) {
        free(raw);
        return 0;
    }
    o.p[o.n++] = 0x78;
    o.p[o.n++] = 0x01;
    if (!deflate_fixed(raw, n, &o)) {
        free(raw);
        free(o.p);
        return 0;
    }
    for (i = 0; i < n; i++) {           /* adler-32 */
        a = (a + raw[i]) % 65521u;
        b = (b + a) % 65521u;
    }
    free(raw);
    if (!out_grow(&o, 4)) {
        free(o.p);
        return 0;
    }
    be32(o.p + o.n, (b << 16) | a);
    o.n += 4;
    be32(ihdr, (uint32_t)w);
    be32(ihdr + 4, (uint32_t)h);
    ihdr[8] = 8; ihdr[9] = 6; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
    f = fopen(path, "wb");
    if (!f) {
        free(o.p);
        return 0;
    }
    ok = fwrite(sig, 1, 8, f) == 8 && chunk(f, "IHDR", ihdr, 13)
         && chunk(f, "IDAT", o.p, o.n) && chunk(f, "IEND", NULL, 0);
    ok = (fclose(f) == 0) && ok;
    free(o.p);
    return ok;
}
