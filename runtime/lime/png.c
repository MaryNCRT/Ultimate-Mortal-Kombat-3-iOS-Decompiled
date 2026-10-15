/* png.c -- see png.h. */
#include "png.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ inflate
 *
 * Port, a loader only (the device used its own PNG reader). Table driven:
 * a 64-bit bit buffer refilled a byte at a time, Huffman codes of up to 9
 * bits resolved in one look-up, longer ones walked canonically. The first
 * version read one bit per call and grew the output one byte at a time;
 * MKFONT_0 took 193 ms, a second of every fight load. */

typedef struct {
    const uint8_t *d;
    size_t         n, pos;
    uint64_t       bb;                  /* bits not yet used, LSB first */
    int            bc;                  /* how many */
    int            pad;                 /* zero bytes fed past the end */
    int            err;
} BitIn;

#define FAST_BITS 9

typedef struct {
    uint32_t fast[1 << FAST_BITS];      /* sym | len << 16, 0 = longer code */
    short    counts[16];
    short    symbols[288];
} Huff;

static void refill(BitIn *s)
{
    while (s->bc <= 56) {
        uint64_t b = 0;

        if (s->pos < s->n)
            b = s->d[s->pos++];
        else if (++s->pad > 8)
            s->err = 1;
        s->bb |= b << s->bc;
        s->bc += 8;
    }
}

static int getbits(BitIn *s, int need)
{
    int v;

    if (need == 0) return 0;
    if (s->bc < need) refill(s);
    v = (int)(s->bb & ((1u << need) - 1u));
    s->bb >>= need;
    s->bc -= need;
    return v;
}

static void huff_build(Huff *h, const uint8_t *lengths, int n)
{
    int i, sum = 0, offs[16], code = 0, next[16];

    for (i = 0; i < 16; i++) h->counts[i] = 0;
    for (i = 0; i < n; i++) h->counts[lengths[i]]++;
    h->counts[0] = 0;
    for (i = 0; i < 16; i++) { offs[i] = sum; sum += h->counts[i]; }
    for (i = 0; i < n; i++)
        if (lengths[i]) h->symbols[offs[lengths[i]]++] = (short)i;

    /* canonical codes, MSB first, then bit-reversed into the fast table */
    memset(h->fast, 0, sizeof(h->fast));
    for (i = 1; i < 16; i++) {
        code = (code + h->counts[i - 1]) << 1;
        next[i] = code;
    }
    for (i = 0; i < n; i++) {
        int len = lengths[i], c, r = 0, k;

        if (len == 0) continue;
        c = next[len]++;
        if (len > FAST_BITS) continue;
        for (k = 0; k < len; k++)
            r |= ((c >> k) & 1) << (len - 1 - k);
        for (k = r; k < (1 << FAST_BITS); k += 1 << len)
            h->fast[k] = (uint32_t)i | ((uint32_t)len << 16);
    }
}

static int huff_decode(BitIn *s, const Huff *h)
{
    int len, code = 0, first = 0, index = 0, count;
    uint32_t e;

    if (s->bc < 16) refill(s);
    e = h->fast[s->bb & ((1u << FAST_BITS) - 1u)];
    if (e) {
        int l = (int)(e >> 16);
        s->bb >>= l;
        s->bc -= l;
        return (int)(e & 0xffffu);
    }
    /* longer than FAST_BITS: the canonical walk, the puff formulation */
    for (len = 1; len <= 15; len++) {
        code |= getbits(s, 1);
        count = h->counts[len];
        if (code - count < first) return h->symbols[index + (code - first)];
        index += count;
        first += count;
        first <<= 1;
        code  <<= 1;
    }
    s->err = 1;
    return -1;
}

static const short LBASE[29] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59,
    67, 83, 99, 115, 131, 163, 195, 227, 258 };
static const short LEXT[29] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3,
    4, 4, 4, 4, 5, 5, 5, 5, 0 };
static const short DBASE[30] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513,
    769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577 };
static const short DEXT[30] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8,
    9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };

typedef struct { uint8_t *d; size_t n, cap; } Out;

/* Room for `more` bytes. The caller sizes the buffer from the image, so this
 * almost never grows. */
static int out_room(Out *o, size_t more)
{
    if (o->n + more > o->cap) {
        size_t cap = o->cap ? o->cap : 65536;
        uint8_t *p;

        while (cap < o->n + more) cap *= 2;
        p = (uint8_t *)realloc(o->d, cap);
        if (!p) return 0;
        o->d = p;
        o->cap = cap;
    }
    return 1;
}

static int inflate_block(BitIn *s, Out *o, const Huff *lc, const Huff *dc)
{
    for (;;) {
        int sym = huff_decode(s, lc);

        if (s->err) return 0;
        if (sym < 256) {
            if (o->n >= o->cap && !out_room(o, 1)) return 0;
            o->d[o->n++] = (uint8_t)sym;
        } else if (sym == 256) {
            return 1;
        } else {
            int len, dist, dsym;
            uint8_t *dst;
            const uint8_t *src;

            sym -= 257;
            if (sym >= 29) return 0;
            len = LBASE[sym] + getbits(s, LEXT[sym]);
            dsym = huff_decode(s, dc);
            if (dsym < 0 || dsym >= 30) return 0;
            dist = DBASE[dsym] + getbits(s, DEXT[dsym]);
            if ((size_t)dist > o->n) return 0;
            if (!out_room(o, (size_t)len)) return 0;
            dst = o->d + o->n;
            src = dst - dist;
            o->n += (size_t)len;
            if (dist >= len) {
                memcpy(dst, src, (size_t)len);
            } else {
                while (len-- > 0)       /* overlapping: byte by byte */
                    *dst++ = *src++;
            }
        }
    }
}

static uint8_t *zinflate(const uint8_t *src, size_t n, size_t *out_len,
                         size_t hint)
{
    static const uint8_t ORDER[19] = {
        16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };
    BitIn s;
    Out   o;
    Huff *fixed = NULL, *dyn = NULL;      /* [0] lit/len, [1] distance, [2] code lengths */
    uint8_t lengths[320];
    int i;

    memset(&s, 0, sizeof(s));
    memset(&o, 0, sizeof(o));
    s.d = src;
    s.n = n;

    /* zlib wrapper: two header bytes, and a trailing adler32 we do not check. */
    if (n < 2) return NULL;
    s.pos = 2;
    if (hint && !out_room(&o, hint)) return NULL;

#define FAIL() do { free(o.d); free(fixed); free(dyn); return NULL; } while (0)
    for (;;) {
        int final = getbits(&s, 1);
        int type  = getbits(&s, 2);

        if (s.err) FAIL();

        if (type == 0) {                        /* stored */
            unsigned len;

            /* back to a byte boundary, and give back the whole bytes the
             * bit buffer had read ahead */
            getbits(&s, s.bc & 7);
            s.pos -= (size_t)(s.bc >> 3) - (size_t)(s.pad > 0 ? s.pad : 0);
            s.bb = 0; s.bc = 0; s.pad = 0;
            if (s.pos + 4 > s.n) FAIL();
            len = (unsigned)s.d[s.pos] | ((unsigned)s.d[s.pos + 1] << 8);
            s.pos += 4;
            if (s.pos + len > s.n) FAIL();
            if (!out_room(&o, len)) FAIL();
            memcpy(o.d + o.n, s.d + s.pos, len);
            o.n += len;
            s.pos += len;
        } else if (type == 1) {                 /* fixed Huffman */
            if (!fixed) {
                fixed = (Huff *)malloc(2 * sizeof(Huff));
                if (!fixed) FAIL();
                for (i = 0;   i < 144; i++) lengths[i] = 8;
                for (i = 144; i < 256; i++) lengths[i] = 9;
                for (i = 256; i < 280; i++) lengths[i] = 7;
                for (i = 280; i < 288; i++) lengths[i] = 8;
                huff_build(&fixed[0], lengths, 288);
                for (i = 0; i < 30; i++) lengths[i] = 5;
                huff_build(&fixed[1], lengths, 30);
            }
            if (!inflate_block(&s, &o, &fixed[0], &fixed[1])) FAIL();
        } else if (type == 2) {                 /* dynamic Huffman */
            uint8_t cl_len[19];
            int hlit  = getbits(&s, 5) + 257;
            int hdist = getbits(&s, 5) + 1;
            int hclen = getbits(&s, 4) + 4;
            int k = 0;

            if (!dyn && !(dyn = (Huff *)malloc(3 * sizeof(Huff)))) FAIL();
            if (hlit > 286 || hdist > 30) FAIL();
            memset(cl_len, 0, sizeof(cl_len));
            for (i = 0; i < hclen; i++) cl_len[ORDER[i]] = (uint8_t)getbits(&s, 3);
            huff_build(&dyn[2], cl_len, 19);

            memset(lengths, 0, sizeof(lengths));
            while (k < hlit + hdist) {
                int sym = huff_decode(&s, &dyn[2]);

                if (sym < 0 || s.err) FAIL();
                if (sym < 16) {
                    lengths[k++] = (uint8_t)sym;
                } else if (sym == 16) {
                    int rep = 3 + getbits(&s, 2);
                    uint8_t prev = k ? lengths[k - 1] : 0;
                    while (rep-- > 0 && k < hlit + hdist) lengths[k++] = prev;
                } else if (sym == 17) {
                    int rep = 3 + getbits(&s, 3);
                    while (rep-- > 0 && k < hlit + hdist) lengths[k++] = 0;
                } else {
                    int rep = 11 + getbits(&s, 7);
                    while (rep-- > 0 && k < hlit + hdist) lengths[k++] = 0;
                }
            }
            huff_build(&dyn[0], lengths, hlit);
            huff_build(&dyn[1], lengths + hlit, hdist);
            if (!inflate_block(&s, &o, &dyn[0], &dyn[1])) FAIL();
        } else {
            FAIL();
        }

        if (final) break;
        if (s.err) FAIL();
    }
#undef FAIL

    free(fixed);
    free(dyn);
    *out_len = o.n;
    return o.d;
}

/* ---------------------------------------------------------------------- PNG */

static uint32_t rd_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static int iabs(int v) { return v < 0 ? -v : v; }

static int paeth(int a, int b, int c)
{
    int p = a + b - c, pa = iabs(p - a), pb = iabs(p - b), pc = iabs(p - c);

    if (pa <= pb && pa <= pc) return a;
    return (pb <= pc) ? b : c;
}

bool lime_png_load(const char *path, uint8_t **out, int *width, int *height)
{
    static const uint8_t SIG[8] = {
        0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };
    FILE *f;
    uint8_t *file = NULL, *idat = NULL, *raw = NULL, *rgba = NULL;
    uint8_t  pal[256 * 3], alpha[256];
    long     size;
    size_t   idat_len = 0, idat_cap = 0, raw_len = 0, off;
    int      w = 0, h = 0, bitdepth = 0, colour = 0, interlace = 0;
    int      channels = 0, stride, y, x, npal = 0, i;

    *out = NULL;
    memset(pal, 0, sizeof(pal));
    for (i = 0; i < 256; i++) alpha[i] = 255;

    f = fopen(path, "rb");
    if (!f) return false;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return false; }
    size = ftell(f);
    rewind(f);
    if (size < 8) { fclose(f); return false; }
    file = (uint8_t *)malloc((size_t)size);
    if (!file) { fclose(f); return false; }
    if (fread(file, 1, (size_t)size, f) != (size_t)size) {
        free(file); fclose(f); return false;
    }
    fclose(f);

    if (memcmp(file, SIG, 8) != 0) { free(file); return false; }

    off = 8;
    while (off + 8 <= (size_t)size) {
        uint32_t len = rd_be32(file + off);
        const char *tag = (const char *)file + off + 4;
        const uint8_t *data = file + off + 8;

        if (off + 12 + (size_t)len > (size_t)size) break;

        if (!memcmp(tag, "IHDR", 4) && len >= 13) {
            w = (int)rd_be32(data);
            h = (int)rd_be32(data + 4);
            bitdepth  = data[8];
            colour    = data[9];
            interlace = data[12];
        } else if (!memcmp(tag, "PLTE", 4)) {
            npal = (int)(len / 3);
            if (npal > 256) npal = 256;
            memcpy(pal, data, (size_t)npal * 3);
        } else if (!memcmp(tag, "tRNS", 4)) {
            size_t n = len > 256 ? 256 : (size_t)len;
            memcpy(alpha, data, n);
        } else if (!memcmp(tag, "IDAT", 4)) {
            if (idat_len + (size_t)len > idat_cap) {
                size_t cap = idat_cap ? idat_cap : (size_t)len + 65536;
                uint8_t *p;

                while (cap < idat_len + (size_t)len) cap *= 2;
                p = (uint8_t *)realloc(idat, cap);
                if (!p) { free(idat); free(file); return false; }
                idat = p;
                idat_cap = cap;
            }
            memcpy(idat + idat_len, data, len);
            idat_len += len;
        } else if (!memcmp(tag, "IEND", 4)) {
            break;
        }
        off += 12 + (size_t)len;
    }
    free(file);

    if (!idat || w <= 0 || h <= 0 || bitdepth != 8 || interlace != 0) {
        free(idat);
        return false;
    }
    switch (colour) {
        case 0: channels = 1; break;            /* grey */
        case 2: channels = 3; break;            /* RGB */
        case 3: channels = 1; break;            /* palette */
        case 4: channels = 2; break;            /* grey + alpha */
        case 6: channels = 4; break;            /* RGBA */
        default: free(idat); return false;
    }
    if (colour == 3 && npal == 0) { free(idat); return false; }

    raw = zinflate(idat, idat_len, &raw_len,
                   (size_t)h * (size_t)(w * channels + 1));
    free(idat);
    if (!raw) return false;

    stride = w * channels;
    if (raw_len < (size_t)h * (size_t)(stride + 1)) { free(raw); return false; }

    /* Unfilter in place. Each scanline is one filter byte then stride bytes,
     * and every filter refers to the row above AFTER that row was unfiltered,
     * which is why this runs top to bottom and writes back as it goes. */
    for (y = 0; y < h; y++) {
        uint8_t *row  = raw + (size_t)y * (size_t)(stride + 1) + 1;
        const uint8_t *prev = (y > 0)
                      ? raw + (size_t)(y - 1) * (size_t)(stride + 1) + 1
                      : NULL;
        int ft = raw[(size_t)y * (size_t)(stride + 1)];
        int c = channels;

        /* one loop per filter, not a switch per byte */
        switch (ft) {
        case 0:
            break;
        case 1:
            for (x = c; x < stride; x++) row[x] = (uint8_t)(row[x] + row[x - c]);
            break;
        case 2:
            if (prev)
                for (x = 0; x < stride; x++) row[x] = (uint8_t)(row[x] + prev[x]);
            break;
        case 3:
            if (prev) {
                for (x = 0; x < c; x++) row[x] = (uint8_t)(row[x] + prev[x] / 2);
                for (; x < stride; x++)
                    row[x] = (uint8_t)(row[x] + ((int)row[x - c] + (int)prev[x]) / 2);
            } else {
                for (x = c; x < stride; x++) row[x] = (uint8_t)(row[x] + row[x - c] / 2);
            }
            break;
        case 4:
            if (prev) {
                for (x = 0; x < c; x++) row[x] = (uint8_t)(row[x] + prev[x]);
                for (; x < stride; x++)
                    row[x] = (uint8_t)(row[x] + paeth(row[x - c], prev[x], prev[x - c]));
            } else {
                for (x = c; x < stride; x++) row[x] = (uint8_t)(row[x] + row[x - c]);
            }
            break;
        default:
            free(raw);
            return false;
        }
    }

    rgba = (uint8_t *)malloc((size_t)w * (size_t)h * 4);
    if (!rgba) { free(raw); return false; }

    for (y = 0; y < h; y++) {
        const uint8_t *row = raw + (size_t)y * (size_t)(stride + 1) + 1;
        uint8_t *dst = rgba + (size_t)y * (size_t)w * 4;

        for (x = 0; x < w; x++) {
            const uint8_t *s = row + (size_t)x * (size_t)channels;
            uint8_t *d = dst + (size_t)x * 4;

            switch (colour) {
                case 0:
                    d[0] = d[1] = d[2] = s[0];
                    d[3] = 255;
                    break;
                case 2:
                    d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = 255;
                    break;
                case 3: {
                    int idx = (s[0] < npal) ? s[0] : 0;
                    d[0] = pal[idx * 3 + 0];
                    d[1] = pal[idx * 3 + 1];
                    d[2] = pal[idx * 3 + 2];
                    d[3] = alpha[idx];
                    break;
                }
                case 4:
                    d[0] = d[1] = d[2] = s[0];
                    d[3] = s[1];
                    break;
                default:
                    d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = s[3];
                    break;
            }
        }
    }
    free(raw);

    *out = rgba;
    *width = w;
    *height = h;
    return true;
}
