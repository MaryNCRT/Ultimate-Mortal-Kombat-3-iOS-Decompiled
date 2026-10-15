#include "pvr.h"
#include "png.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PVR_HEADER  52
#define PVR_TAG     0x21525650u          /* "PVR!" little-endian */
#define BLOCK_BYTES 8

static uint32_t rd_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* The two endpoint colours of a block, by the PVRTC format (PowerVR's
 * reference decoder, PVRTDecompress): 5-bit red, green and blue and 4-bit
 * alpha, widened to 8 bits only once the pixel is mixed.
 *
 * Colour A is the LOW 16 bits of the colour word. Bit 0 is the block's
 * modulation flag, so A's blue has one bit fewer than B's: opaque 5-5-4,
 * translucent 3-4-4-3 (alpha-red-green-blue). Colour B is the HIGH 16 bits:
 * opaque 5-5-5, translucent 3-4-4-4. A translucent endpoint's three alpha
 * bits become aaa0, so it reaches at most 14 of 15. */
static void colour_a(uint32_t c, int *out)
{
    if (c & 0x8000) {
        out[0] = (int)((c & 0x7c00) >> 10);
        out[1] = (int)((c & 0x3e0) >> 5);
        out[2] = (int)((c & 0x1e) | ((c & 0x1e) >> 4));
        out[3] = 0xf;
    } else {
        out[0] = (int)(((c & 0xf00) >> 7) | ((c & 0xf00) >> 11));
        out[1] = (int)(((c & 0xf0) >> 3) | ((c & 0xf0) >> 7));
        out[2] = (int)(((c & 0xe) << 1) | ((c & 0xe) >> 2));
        out[3] = (int)((c & 0x7000) >> 11);
    }
}

static void colour_b(uint32_t c, int *out)
{
    if (c & 0x80000000u) {
        out[0] = (int)((c & 0x7c000000u) >> 26);
        out[1] = (int)((c & 0x3e00000u) >> 21);
        out[2] = (int)((c & 0x1f0000u) >> 16);
        out[3] = 0xf;
    } else {
        out[0] = (int)(((c & 0xf000000u) >> 23) | ((c & 0xf000000u) >> 27));
        out[1] = (int)(((c & 0xf00000u) >> 19) | ((c & 0xf00000u) >> 23));
        out[2] = (int)(((c & 0xf0000u) >> 15) | ((c & 0xf0000u) >> 19));
        out[3] = (int)((c & 0x70000000u) >> 27);
    }
}

/* Blocks are stored in Morton order: the bits of x and y interleaved, with
 * whichever axis is longer contributing its remaining high bits linearly. */
static int morton_index(int x, int y, int width, int height)
{
    int n = (width < height) ? width : height;
    int idx = 0, shift = 0;
    for (int k = n; k > 1; k >>= 1) {
        idx |= ((y & 1) << shift) | ((x & 1) << (shift + 1));
        x >>= 1; y >>= 1; shift += 2;
    }
    idx |= (width > height ? x : y) << shift;
    return idx;
}

static int wrapi(int v, int n)
{
    if (n <= 0) return 0;
    v %= n;
    return v < 0 ? v + n : v;
}

static int floor_div(float f)
{
    int i = (int)f;
    return (f < 0.0f && (float)i != f) ? i - 1 : i;
}

/* Modulation, per pixel: 0..8, the weight of colour B in eighths. PUNCH
 * marks a 4bpp punch-through (transparent) pixel; INTERP a 2bpp pixel whose
 * value comes from its neighbours, with the axis in its low two bits until
 * it is resolved. */
#define MOD_PUNCH  0x10
#define MOD_INTERP 0x20

static const uint8_t MOD_DIRECT[4] = { 0, 3, 5, 8 };
static const uint8_t MOD_PUNCHT[4] = { 0, 4, 4 | MOD_PUNCH, 8 };

static bool decode_pvrtc(const uint8_t *raw, long raw_len, int width, int height,
                         int two_bpp, LimeImage *out)
{
    const int bw = two_bpp ? 8 : 4, bh = 4;
    int bx = (width  + bw - 1) / bw; if (bx < 1) bx = 1;
    int by = (height + bh - 1) / bh; if (by < 1) by = 1;
    if (raw_len < (long)bx * by * BLOCK_BYTES) return false;

    const int nb = bx * by;
    const int gw = bx * bw, gh = by * bh;      /* the block grid, in pixels */
    int      *ca   = malloc((size_t)nb * 4 * sizeof(int));
    int      *cb   = malloc((size_t)nb * 4 * sizeof(int));
    uint8_t  *mod  = malloc((size_t)gw * (size_t)gh);
    uint8_t  *rgba = malloc((size_t)width * (size_t)height * 4);
    if (!ca || !cb || !mod || !rgba) {
        free(ca); free(cb); free(mod); free(rgba);
        return false;
    }

    /* 1. each block's endpoints, and each pixel's modulation */
    for (int j = 0; j < by; j++) {
        for (int i = 0; i < bx; i++) {
            long off = (long)morton_index(i, j, bx, by) * BLOCK_BYTES;
            uint32_t m = rd_u32(raw + off);
            uint32_t c = rd_u32(raw + off + 4);
            int k = j * bx + i;
            int flag = (int)(c & 1);
            int how = 0;

            colour_a(c, &ca[k * 4]);
            colour_b(c, &cb[k * 4]);

            if (two_bpp && flag) {
                /* Interpolated 2bpp. Bit 0 asks for one axis only and bit 20
                 * says which; then bit 1 stands in for bit 0, and bit 21 for
                 * bit 20, as data. */
                if (m & 1u) {
                    how = (m & (1u << 20)) ? 2 : 1;   /* 2 vertical, 1 horizontal */
                    if (m & (1u << 21)) m |= (1u << 20);
                    else                m &= ~(1u << 20);
                }
                if (m & 2u) m |= 1u; else m &= ~1u;
            }

            for (int y = 0; y < bh; y++) {
                uint8_t *row = mod + (size_t)(j * bh + y) * gw + (size_t)i * bw;
                for (int x = 0; x < bw; x++) {
                    uint8_t v;
                    if (!two_bpp) {
                        int sel = (int)((m >> ((y * 4 + x) * 2)) & 3u);
                        v = flag ? MOD_PUNCHT[sel] : MOD_DIRECT[sel];
                    } else if (!flag) {
                        v = ((m >> (y * 8 + x)) & 1u) ? 8 : 0;
                    } else if (((x ^ y) & 1) == 0) {
                        /* the stored half of the checkerboard, 2 bits each */
                        v = MOD_DIRECT[m & 3u];
                        m >>= 2;
                    } else {
                        v = (uint8_t)(MOD_INTERP | how);
                    }
                    row[x] = v;
                }
            }
        }
    }

    /* 2. an interpolated 2bpp pixel is the average of its neighbours -- the
     *    stored half of the checkerboard, or a direct block's pixels --
     *    wrapping across the texture as the block grid does */
    if (two_bpp) {
        for (int y = 0; y < gh; y++) {
            for (int x = 0; x < gw; x++) {
                uint8_t *p = mod + (size_t)y * gw + x;
                if (!(*p & MOD_INTERP))
                    continue;
                int l = mod[(size_t)y * gw + wrapi(x - 1, gw)] & 0xf;
                int r = mod[(size_t)y * gw + wrapi(x + 1, gw)] & 0xf;
                int u = mod[(size_t)wrapi(y - 1, gh) * gw + x] & 0xf;
                int d = mod[(size_t)wrapi(y + 1, gh) * gw + x] & 0xf;
                int how = *p & 3;
                *p = (uint8_t)(how == 1 ? (l + r + 1) / 2
                             : how == 2 ? (u + d + 1) / 2
                             : (l + r + u + d + 2) / 4);
            }
        }
    }

    /* 3. the endpoints of the four blocks whose centres surround the pixel
     *    (the grid sits half a block off), bilinearly, mixed by modulation */
    for (int py = 0; py < height; py++) {
        for (int px = 0; px < width; px++) {
            float fx = (float)(px - bw / 2) / (float)bw;
            float fy = (float)(py - bh / 2) / (float)bh;
            int i0 = floor_div(fx), j0 = floor_div(fy);
            float u = fx - (float)i0, v = fy - (float)j0;
            i0 = wrapi(i0, bx); j0 = wrapi(j0, by);
            int i1 = wrapi(i0 + 1, bx), j1 = wrapi(j0 + 1, by);

            int k00 = j0 * bx + i0, k10 = j0 * bx + i1;
            int k01 = j1 * bx + i0, k11 = j1 * bx + i1;
            float w00 = (1.0f - u) * (1.0f - v), w10 = u * (1.0f - v);
            float w01 = (1.0f - u) * v,          w11 = u * v;

            int mv = mod[(size_t)py * gw + px];
            float w = (float)(mv & 0xf) / 8.0f;

            uint8_t *o = rgba + ((size_t)py * (size_t)width + (size_t)px) * 4;
            for (int ch = 0; ch < 4; ch++) {
                float A = (float)ca[k00*4+ch]*w00 + (float)ca[k10*4+ch]*w10 +
                          (float)ca[k01*4+ch]*w01 + (float)ca[k11*4+ch]*w11;
                float B = (float)cb[k00*4+ch]*w00 + (float)cb[k10*4+ch]*w10 +
                          (float)cb[k01*4+ch]*w01 + (float)cb[k11*4+ch]*w11;
                float val = (A * (1.0f - w) + B * w)
                          * (ch < 3 ? 255.0f / 31.0f : 255.0f / 15.0f) + 0.5f;
                o[ch] = (uint8_t)(val < 0.0f ? 0.0f : (val > 255.0f ? 255.0f : val));
            }
            if (mv & MOD_PUNCH)
                o[3] = 0;
        }
    }

    free(ca); free(cb); free(mod);
    out->width = width;
    out->height = height;
    out->rgba = rgba;
    return true;
}

bool lime_pvr_load(const char *path, LimeImage *out)
{
    memset(out, 0, sizeof(*out));

    FILE *f = fopen(path, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < PVR_HEADER) { fclose(f); return false; }

    uint8_t *raw = malloc((size_t)size);
    if (!raw) { fclose(f); return false; }
    if (fread(raw, 1, (size_t)size, f) != (size_t)size) {
        free(raw); fclose(f); return false;
    }
    fclose(f);

    uint32_t hlen   = rd_u32(raw + 0);
    uint32_t height = rd_u32(raw + 4);
    uint32_t width  = rd_u32(raw + 8);
    uint32_t flags  = rd_u32(raw + 16);
    uint32_t tag    = rd_u32(raw + 44);

    if (tag != PVR_TAG || hlen != PVR_HEADER) { free(raw); return false; }

    uint32_t fmt = flags & 0xFF;
    bool ok = false;
    if (fmt == 0x19)                       /* PVRTC 4bpp */
        ok = decode_pvrtc(raw + PVR_HEADER, size - PVR_HEADER,
                          (int)width, (int)height, 0, out);
    else if (fmt == 0x18)                  /* PVRTC 2bpp */
        ok = decode_pvrtc(raw + PVR_HEADER, size - PVR_HEADER,
                          (int)width, (int)height, 1, out);

    free(raw);
    return ok;
}

void lime_image_free(LimeImage *img)
{
    if (img) { free(img->rgba); memset(img, 0, sizeof(*img)); }
}

bool lime_texture_load(const char *res_dir, const char *mesh_texture,
                       LimeImage *out)
{
    memset(out, 0, sizeof(*out));
    if (!mesh_texture || !*mesh_texture) return false;

    /* Strip the exporter placeholder extension. */
    char stem[128];
    size_t n = strlen(mesh_texture);
    if (n >= sizeof(stem)) n = sizeof(stem) - 1;
    memcpy(stem, mesh_texture, n);
    stem[n] = 0;
    char *dot = strrchr(stem, '.');
    if (dot) *dot = 0;

    static const char *exts[2] = { ".pvr", ".PVR" };
    char path[512];
    for (int e = 0; e < 2; e++) {
        snprintf(path, sizeof(path), "%s/Textures/%s%s", res_dir, stem, exts[e]);
        if (lime_pvr_load(path, out)) return true;
        snprintf(path, sizeof(path), "%s/%s%s", res_dir, stem, exts[e]);
        if (lime_pvr_load(path, out)) return true;
    }

    /* **221 of the shipped textures are PNG, not PVRTC.** This used to stop at
     * the loop above and return false, so every stage whose atlas is a PNG drew
     * untextured -- the single largest visual gap in the port. See png.c. */
    {
        static const char *png[2] = { ".PNG", ".png" };
        uint8_t *rgba;
        int w, h;
        for (int e = 0; e < 2; e++) {
            snprintf(path, sizeof(path), "%s/Textures/%s%s", res_dir, stem, png[e]);
            if (lime_png_load(path, &rgba, &w, &h)) {
                out->rgba = rgba; out->width = w; out->height = h;
                return true;
            }
            snprintf(path, sizeof(path), "%s/%s%s", res_dir, stem, png[e]);
            if (lime_png_load(path, &rgba, &w, &h)) {
                out->rgba = rgba; out->width = w; out->height = h;
                return true;
            }
        }
    }
    return false;
}
