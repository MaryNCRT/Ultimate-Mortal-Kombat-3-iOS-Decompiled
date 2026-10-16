/*
 * draw_gl.c -- the windowed half of the platform layer.
 *
 * `lime_platform.c` and `lime_menu.c` answer every draw call by counting it.
 * That is what lets `tests/test_menu_boot.c` exercise the front end with no
 * window, and it is the right default: it tests the transcription rather than
 * the renderer. This file is the other half. Build with `-DUMK3_REAL_GL` and
 * those blocks compile out, opengl32 supplies the `gl*` entry points, and the
 * lime draw calls land on the screen.
 *
 * ## What the front end asks for
 *
 * Two primitives and a mode:
 *
 *     limeSet2DDrawing()                     an orthographic 480x320, Y down
 *     limeDrawSprite(tex, x, y, w, h, u, v, du, dv, colour)
 *     limeFillRect(x, y, w, h, r, g, b, a)
 *
 * and text, which is not a third primitive: `limeDrawFONT` in
 * `decomp/lime/limeFont.c` walks the glyphs and draws each through
 * `limeDrawSprite`, so a sprite that works gives text for free.
 *
 * The UV pair after `u, v` is an EXTENT, not a second corner. `limeDrawFONT`
 * computes `du = glyphWidth / atlasWidth`, which is a width; and a negative
 * extent mirrors, which is how the front end flips a sprite without a second
 * texture.
 *
 * ## Coordinates
 *
 * The game draws in a 480x320 space with the origin at the top left -- the
 * iPhone's, in landscape. `limeScreenWidth` and `limeScreenHeight` say so, and
 * the front end scales its own layout against them through FE_X and FE_Y. So
 * the projection is that rectangle exactly, and the window stretches to it
 * through the viewport rather than the game knowing the window's size.
 *
 * ## Fixed-function GL, deliberately
 *
 * `glBegin`/`glEnd` and the fixed pipeline, because that is what the Windows
 * opengl32 exports without an extension loader and because the whole of this
 * file is quads. The 3D path -- the rotating character models on the select
 * screen -- goes through `glDrawElements` and the matrix stack, which come
 * from opengl32 unchanged.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform/gl.h"
#include "platform/platform.h"
#include "lime/png.h"
#include <stddef.h>
#include "lime/pvr.h"
#include "lime/par.h"

#include "../decomp/lime/lime.h"

const char *lime_platform_asset_root(void);

/* Multitexture is GL 1.3 and Windows' opengl32 stops at 1.1, so these two are
 * ours rather than the driver's. Nothing here uses a second unit: the menus
 * draw one textured quad at a time. */
void glClientActiveTexture(unsigned u) { (void)u; }
void glActiveTexture(unsigned u)       { (void)u; }


/* ----------------------------------------------------------------- state */

static long g_sprites, g_fills;
static int  g_screen_w = 480, g_screen_h = 320;

/* `lime_platform_draw_calls` and `..._draw_indices` stay in lime_platform.c:
 * they count glDrawElements, and with a real GL that call is the driver's, so
 * there is nothing here to count it from. The sprite and fill counters are
 * ours because the calls that raise them are. */
long lime_platform_sprite_count(void) { return g_sprites; }
long lime_gl_fill_count(void)         { return g_fills; }

void lime_gl_set_screen(int w, int h)
{
    g_screen_w = w;
    g_screen_h = h;
}


/* --------------------------------------------------------------- textures
 *
 * The bundle stores a sheet as .PNG or as .PVR and the game asks for it by the
 * name in the data, extension included. Both decoders are already here, so the
 * loader tries the name as given and then the other extension -- which is what
 * the device's own loader did, since the exporter wrote a literal ".???" into
 * meshes and the real extension was supplied at load time.
 *
 * Every distinct path is loaded once and kept. The menus re-request the same
 * sheets every frame and a texture upload per frame would be visible.
 */
typedef struct TexEntry {
    struct TexEntry *next;
    char            *path;
    TEXTURE          tex;       /* `name` is the GL object; lime.h states it */
    int              w, h;
} TexEntry;

static TexEntry *g_textures;

/* The launcher's texture quality (umk3.ini texture_quality=), port only:
 *
 *      0  IPHONE  the device's own: at 480 wide the game loads the _LOW
 *                 sheets (SetupFEScale's UseLOWAssets)
 *      1  IPAD    the full sheets a wider screen gets (FrontEnd.c)
 *      2  MAX     the full sheets, a sheet's lossless .png before its PVRTC
 *                 .pvr when the bundle has both, mipmaps and the card's
 *                 largest anisotropic filtering
 *
 * The .pvr data is compressed in the .ipa itself; nothing here can undo that,
 * MAX is the best those files give. */
int PortTextureQuality;

static TexEntry *tex_find(const char *path)
{
    TexEntry *e;
    for (e = g_textures; e; e = e->next)
        if (strcmp(e->path, path) == 0)
            return e;
    return NULL;
}

static int try_one(const char *full, LimeImage *out)
{
    uint8_t *px = NULL;
    int w = 0, h = 0;

    if (lime_pvr_load(full, out))
        return 1;
    if (lime_png_load(full, &px, &w, &h)) {
        out->rgba = px;
        out->width = w;
        out->height = h;
        return 1;
    }
    return 0;
}

static int fit_twin(LimeImage *img, int cw, int ch, int dw, int dh);

static int load_rgba(const char *path, LimeImage *out)
{
    /* Where the bundle keeps sheets, in the order that finds them soonest:
     * Textures/ holds 1,620 of them and res/ itself almost none. Nothing is
     * looked for outside res/. */
    static const char *const dirs[] = { "Textures/", "" };
    /* The name as asked, then the same stem under each real extension. The
     * data says ".PNG" for files that are .pvr on disk -- the exporter wrote
     * one extension and the device supplied another. */
    static const char *const exts[] = { ".pvr", ".PVR", NULL, ".png", ".PNG" };
    static const char *const exts_max[] = { ".png", ".PNG", NULL, ".pvr", ".PVR" };
    /* Port, not in the binary: from IPAD up, a sheet the bundle also ships
     * as uncompressed PNG with the SAME layout is taken from the PNG, not
     * the PVRTC: FE_MAINLOGO_*, LIGHTNING, VORTEX1/2 (mean difference under
     * 2/255 against the decoded PVR). The other twins are different art:
     * FE_METAL_BG's PNG is the 480x320 picture padded with white where the
     * PVR is stretched over the sheet, the *_VERSUS PNGs are laid out
     * differently, and MYBLOOD1/2 differ too -- the PNG drew a half-white
     * Lost Treasure screen and stats page. */
    static const char *const twins[] = {
        "FE_MAINLOGO_", "LIGHTNING", "VORTEX1", "VORTEX2", NULL };
    const char *const *ext = exts;

    const char *root = lime_platform_asset_root();
    char full[1200];
    char stem[256];
    const char *dot;
    size_t n;
    int d, e;

    dot = strrchr(path, '.');
    n = dot ? (size_t)(dot - path) : strlen(path);
    if (n >= sizeof(stem))
        return 0;
    memcpy(stem, path, n);
    stem[n] = 0;

    if (PortTextureQuality >= 1) {
        const char *base = strrchr(stem, '/') ? strrchr(stem, '/') + 1 : stem;
        int t;

        for (t = 0; twins[t]; t++)
            if (twins[t][strlen(twins[t]) - 1] == '_'
                    ? _strnicmp(base, twins[t], strlen(twins[t])) == 0
                    : _stricmp(base, twins[t]) == 0)
                ext = exts_max;
    }

    /* Port, not in the binary: from IPAD up, a sheet the bundle also has
     * at twice the size under an _IPAD name is taken from there -- the game
     * font (MKFONT_0/1 -> MKFONT_IPAD_0/1, 2048 square, the grid its
     * mkfont_ipad.ft2 is laid out on). The iPhone binary does not load it;
     * the iPad build's Task_LoadGeneralData does. BUTTONS_TPAGE_IPAD is not
     * swapped in here: the iPad build loads it for the fight only
     * (GameInit_LoadABit) and keeps BUTTONS_TPAGE in the menus
     * (FEInit_LoadABit), whose sample button draws the pressed cell --
     * a glow on the iPad sheet. See GameInit_LoadABit, case 26. */
    /* Port, not in the binary: FE_METAL_BG's PNG twin is the same picture
     * uncompressed but laid out differently -- 480x320 at the top left of
     * the sheet, white below and right -- where the PVR is that picture
     * stretched over all 512x512. From IPAD up the PNG is taken, cropped to
     * its 480x320 and stretched the same way, so the menus' coordinates land
     * where they do on the PVR. (The *_VERSUS PNG twins are not this: they
     * are other, smaller pictures -- 256 square, an orange backdrop -- and
     * the PVR's 512 art is kept.) */
    if (PortTextureQuality >= 1) {
        const char *base = strrchr(stem, '/') ? strrchr(stem, '/') + 1 : stem;

        if (_stricmp(base, "FE_METAL_BG") == 0) {
            for (e = 0; e < 2; e++) {
                snprintf(full, sizeof(full), "%s/Textures/%s%s", root, base,
                         e ? ".PNG" : ".png");
                if (try_one(full, out)) {
                    if (out->width >= 480 && out->height >= 320
                        && fit_twin(out, 480, 320, 512, 512))
                        return 1;
                    free(out->rgba);
                    memset(out, 0, sizeof(*out));
                }
            }
        }
    }

    if (PortTextureQuality >= 1 && strncmp(stem, "MKFONT_", 7) == 0) {
        char big[300];
        size_t k = strlen(stem);

        if (k >= 2 && stem[k - 2] == '_' && stem[k - 1] >= '0' && stem[k - 1] <= '9')
            snprintf(big, sizeof(big), "%.*s_IPAD%s", (int)(k - 2), stem, stem + k - 2);
        else
            snprintf(big, sizeof(big), "%s_IPAD", stem);
        for (e = 0; e < 2; e++) {
            snprintf(full, sizeof(full), "%s/Textures/%s%s", root, big,
                     e ? ".png" : ".PNG");
            if (try_one(full, out))
                return 1;
        }
    }

    for (e = 0; e < (int)(sizeof(exts) / sizeof(exts[0])); e++) {
        for (d = 0; d < (int)(sizeof(dirs) / sizeof(dirs[0])); d++) {
            if (ext[e] == NULL)
                snprintf(full, sizeof(full), "%s/%s%s", root, dirs[d], path);
            else
                snprintf(full, sizeof(full), "%s/%s%s%s",
                         root, dirs[d], stem, ext[e]);
            if (try_one(full, out))
                return 1;
        }
    }
    return 0;
}

/* Port, not in the binary: SIN LIMITE's upscale of the small sheets.
 *
 * The HUD is drawn from tiny sheets -- DANGER.PNG is 64x16, the timer's
 * COUNTDOWNFONT 128 square, NAMES_FONT and HUD_TPAGE (the bars, the round
 * medallion) 256 -- uncompressed, with nothing bigger in either bundle (the
 * iPad .ipa's textures are byte-for-byte the iPhone's). Magnified by the
 * card's bilinear filter they go soft and blocky. So a sheet of at most 256
 * on its longer side is enlarged four times here, once, at load: Catmull-Rom
 * bicubic on premultiplied colour, and where the alpha is mostly hard
 * (text, bars, icons -- not smoke or glows) the edge is tightened back to
 * the width of a source pixel. The coordinates are 0..1, so nothing else
 * changes. */
static float cubic_w(float t)
{
    t = t < 0.0f ? -t : t;
    if (t < 1.0f) return 1.5f * t * t * t - 2.5f * t * t + 1.0f;
    if (t < 2.0f) return -0.5f * t * t * t + 2.5f * t * t - 4.0f * t + 2.0f;
    return 0.0f;
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

/* The top-left cw x ch of img, resampled to dw x dh (Catmull-Rom, edges
 * clamped, straight colour: the sheets this serves are opaque). 0 on no
 * memory, img untouched. */
static int fit_twin(LimeImage *img, int cw, int ch, int dw, int dh)
{
    float *tmp = (float *)malloc((size_t)dw * ch * 4 * sizeof(float));
    uint8_t *out = (uint8_t *)malloc((size_t)dw * dh * 4);
    const int sw = img->width;
    int x, y, c, k;

    if (!tmp || !out) { free(tmp); free(out); return 0; }
    for (y = 0; y < ch; y++)
        for (x = 0; x < dw; x++) {
            float sx = (x + 0.5f) * cw / dw - 0.5f, acc[4] = { 0, 0, 0, 0 };
            int x0 = (int)(sx < 0 ? sx - 1 : sx);
            for (k = -1; k <= 2; k++) {
                float wt = cubic_w(sx - (float)(x0 + k));
                const uint8_t *q = img->rgba + ((size_t)y * sw + clampi(x0 + k, 0, cw - 1)) * 4;
                for (c = 0; c < 4; c++) acc[c] += q[c] * wt;
            }
            for (c = 0; c < 4; c++) tmp[((size_t)y * dw + x) * 4 + c] = acc[c];
        }
    for (y = 0; y < dh; y++) {
        float sy = (y + 0.5f) * ch / dh - 0.5f;
        int y0 = (int)(sy < 0 ? sy - 1 : sy);
        for (x = 0; x < dw; x++) {
            float acc[4] = { 0, 0, 0, 0 };
            for (k = -1; k <= 2; k++) {
                float wt = cubic_w(sy - (float)(y0 + k));
                const float *q = tmp + ((size_t)clampi(y0 + k, 0, ch - 1) * dw + x) * 4;
                for (c = 0; c < 4; c++) acc[c] += q[c] * wt;
            }
            for (c = 0; c < 4; c++)
                out[((size_t)y * dw + x) * 4 + c] = (uint8_t)(acc[c] < 0.0f ? 0.0f
                    : (acc[c] > 255.0f ? 255.0f : acc[c] + 0.5f));
        }
    }
    free(tmp);
    free(img->rgba);
    img->rgba = out;
    img->width = dw;
    img->height = dh;
    return 1;
}

/* The two passes work on rows, each writing only its own, so they go to
 * every core (runtime/lime/par.c) with the same result as one thread. */
typedef struct {
    const float *pm;
    float *tmp;
    uint8_t *out;
    int w, h, W, S, sharpen;
} Upscale;

static void upscale_across(void *vctx, int lo, int hi)
{
    const Upscale *U = (const Upscale *)vctx;
    int x, y, c, k;

    for (y = lo; y < hi; y++)
        for (x = 0; x < U->W; x++) {
            float sx = (x + 0.5f) / U->S - 0.5f, acc[4] = { 0, 0, 0, 0 };
            int x0 = (int)(sx < 0 ? sx - 1 : sx);
            for (k = -1; k <= 2; k++) {
                float wt = cubic_w(sx - (float)(x0 + k));
                const float *q = U->pm + ((size_t)y * U->w + clampi(x0 + k, 0, U->w - 1)) * 4;
                for (c = 0; c < 4; c++) acc[c] += q[c] * wt;
            }
            for (c = 0; c < 4; c++) U->tmp[((size_t)y * U->W + x) * 4 + c] = acc[c];
        }
}

static void upscale_down(void *vctx, int lo, int hi)
{
    const Upscale *U = (const Upscale *)vctx;
    int x, y, c, k;

    for (y = lo; y < hi; y++) {
        float sy = (y + 0.5f) / U->S - 0.5f;
        int y0 = (int)(sy < 0 ? sy - 1 : sy);
        for (x = 0; x < U->W; x++) {
            float acc[4] = { 0, 0, 0, 0 }, a;
            uint8_t *o = U->out + ((size_t)y * U->W + x) * 4;
            for (k = -1; k <= 2; k++) {
                float wt = cubic_w(sy - (float)(y0 + k));
                const float *q = U->tmp + ((size_t)clampi(y0 + k, 0, U->h - 1) * U->W + x) * 4;
                for (c = 0; c < 4; c++) acc[c] += q[c] * wt;
            }
            a = acc[3];
            if (U->sharpen) {            /* a source pixel's ramp -> a quarter of it */
                a = (a - 127.5f) * 2.5f + 127.5f;
            }
            a = a < 0.0f ? 0.0f : (a > 255.0f ? 255.0f : a);
            for (c = 0; c < 3; c++) {
                float v = acc[3] > 0.5f ? acc[c] / (acc[3] / 255.0f) : 0.0f;
                o[c] = (uint8_t)(v < 0.0f ? 0.0f : (v > 255.0f ? 255.0f : v + 0.5f));
            }
            o[3] = (uint8_t)(a + 0.5f);
        }
    }
}

static void upscale_small(LimeImage *img)
{
    const int S = 4, w = img->width, h = img->height;
    const int W = w * S, H = h * S;
    float *pm, *tmp;
    uint8_t *out;
    long hard = 0, n = (long)w * h;
    int c, k, sharpen;
    Upscale U;

    if (w <= 0 || h <= 0 || (w > 256 || h > 256) || (w < 8 && h < 8))
        return;
    for (k = 0; k < n; k++) {
        uint8_t a = img->rgba[k * 4 + 3];
        hard += (a < 24 || a > 231);
    }
    sharpen = hard * 10 >= n * 9;        /* 90% of the pixels hard */

    pm  = (float *)malloc((size_t)n * 4 * sizeof(float));
    tmp = (float *)malloc((size_t)W * h * 4 * sizeof(float));
    out = (uint8_t *)malloc((size_t)W * H * 4);
    if (!pm || !tmp || !out) { free(pm); free(tmp); free(out); return; }

    for (k = 0; k < n; k++) {
        float a = img->rgba[k * 4 + 3] / 255.0f;
        for (c = 0; c < 3; c++) pm[k * 4 + c] = img->rgba[k * 4 + c] * a;
        pm[k * 4 + 3] = img->rgba[k * 4 + 3];
    }
    /* across, then down; samples at pixel centres, edges clamped */
    U.pm = pm; U.tmp = tmp; U.out = out;
    U.w = w; U.h = h; U.W = W; U.S = S; U.sharpen = sharpen;
    lime_par_for(h, upscale_across, &U);
    lime_par_for(H, upscale_down, &U);
    free(pm); free(tmp); free(img->rgba);
    img->rgba = out;
    img->width = W;
    img->height = H;
}

/* Port, not in the binary: the loading screens' budget. The device ran one
 * step of FEInit_LoadABit / GameInit_LoadABit a frame (52 for a fight), so a
 * load took at least 52 frames however fast the steps were. The port runs
 * steps until 50 ms of the frame are spent -- the screen still moves, at
 * 20 frames a second or better -- and then draws. lime_port_load_more(1)
 * starts the frame's clock; (0) says whether another step fits. */
static double g_load_start;              /* 0 = no load running */

int lime_port_load_more(int first)
{
    static double t0;

    if (first) {
        t0 = plat_time();
        if (g_load_start == 0.0)
            g_load_start = t0;
        return 1;
    }
    return plat_time() - t0 < 0.050;
}

/* The end of a load: its time goes to the log ("loading: fight 1.23 s"). */
void lime_port_load_done(const char *what)
{
    if (g_load_start != 0.0)
        printf("loading: %s %.2f s\n", what, plat_time() - g_load_start);
    g_load_start = 0.0;
}

/* armv7 0x000670a0 keeps the texture's own name in its first 0x40 bytes:
 * sprintf("res/Textures/%s"), the last three characters replaced by "pvr"
 * (the PNG branch puts "pvr" back before the copy too), then
 *
 *      tex->field50 = IsTextureFullBrightPath(tex)        0x67226
 *
 * which is the flag LIME_RenderMeshSingle reads to skip the vertex lighting.
 * It used to stay 0, so every full-bright texture was lit. The binary's
 * strcpy has no bound; this one stops at the field. */
int IsTextureFullBrightPath(const char *path);       /* RenderMesh.c */

static void tex_set_name(TEXTURE *t, const char *path)
{
    char buf[0x80];
    size_t n;

    snprintf(buf, sizeof buf, "res/Textures/%s", path);
    n = strlen(buf);
    if (n >= 3)
        memcpy(buf + n - 3, "pvr", 3);
    snprintf((char *)t->_pad00, sizeof t->_pad00, "%s", buf);
    t->field50 = IsTextureFullBrightPath((const char *)t->_pad00);
}

#ifdef _WIN32
#define MIP_API __stdcall
#else
#define MIP_API
#endif
/* An image into GL object `name`, at the quality set now. */
static void tex_upload(GLuint name, LimeImage *img)
{
    static int mip_tried;
    static void (MIP_API *gen_mip)(GLenum);

    glBindTexture(GL_TEXTURE_2D, name);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    /* No wrap mode, on purpose. Every glTexParameteri in the binary (PVRTexture
     * createGLTexture, Texture2D initWithData, seven calls in all) sets the
     * filters or GENERATE_MIPMAP and none sets GL_TEXTURE_WRAP_S/T, so every
     * texture keeps the default GL_REPEAT -- and the font depends on it: a
     * glyph on the second atlas page has a V past 1.0 (J is at 2504 of 2048),
     * which REPEAT folds onto the second sheet. This used to set GL_CLAMP,
     * and J, Á and Ñ drew as nothing: "UGAR" for JUGAR. */
    if (PortTextureQuality >= 2) {
        /* MAX: mipmaps (GL_GENERATE_MIPMAP, GL 1.4) and the largest
         * anisotropy the card offers (EXT_texture_filter_anisotropic) */
        static float aniso = -1.0f;

        if (aniso < 0.0f) {
            const char *x = (const char *)glGetString(GL_EXTENSIONS);
            aniso = 0.0f;
            if (x && strstr(x, "GL_EXT_texture_filter_anisotropic"))
                glGetFloatv(0x84FF, &aniso);   /* MAX_TEXTURE_MAX_ANISOTROPY */
        }
        /* the card builds them (glGenerateMipmap, GL 3.0, after the
         * upload, below); without it the driver's GENERATE_MIPMAP, which
         * some drivers do on the CPU -- a second of every SIN LIMITE load */
        if (!mip_tried) {
            mip_tried = 1;
            gen_mip = (void (MIP_API *)(GLenum))plat_gl_proc("glGenerateMipmap");
        }
        if (!gen_mip)
            glTexParameteri(GL_TEXTURE_2D, 0x8191, GL_TRUE);  /* GENERATE_MIPMAP */
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                        GL_LINEAR_MIPMAP_LINEAR);
        if (aniso > 1.0f)
            glTexParameterf(GL_TEXTURE_2D, 0x84FE, aniso);  /* MAX_ANISOTROPY */
    } else {                            /* a texture MAX made, again lower */
        glTexParameteri(GL_TEXTURE_2D, 0x8191, GL_FALSE);
        glTexParameterf(GL_TEXTURE_2D, 0x84FE, 1.0f);
    }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img->width, img->height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, img->rgba);
    if (PortTextureQuality >= 2 && gen_mip)
        gen_mip(GL_TEXTURE_2D);
}

TEXTURE *limeLoadTexture(const char *path, int a, int b)
{
    TexEntry *e;
    LimeImage img;
    GLuint name = 0;

    (void)a; (void)b;
    if (path == NULL || *path == 0)
        return NULL;

    e = tex_find(path);
    if (e)
        return &e->tex;

    memset(&img, 0, sizeof(img));
    if (!load_rgba(path, &img)) {
        if (getenv("LIME_TRACE_TEX"))
            fprintf(stderr, "[tex] %s MISSING\n", path);
        return NULL;                    /* the failure branch the engine has */
    }
    if (PortTextureQuality >= 2)
        upscale_small(&img);
    if (getenv("LIME_TRACE_TEX"))
        fprintf(stderr, "[tex] %s %dx%d\n", path, img.width, img.height);

    glGenTextures(1, &name);
    tex_upload(name, &img);
    free(img.rgba);

    /* The TEXTURE handed back is stored in 32-bit words of the binary's
     * layout -- a fighter's skin at +0x528 -- so it must be below 4 GB, as
     * any i686 block is. From the C library's heap, not limeMalloc's arena:
     * a save state loaded must not take the cache back (savestate.h). Never
     * freed; see limeDeleteTexture. */
    e = (TexEntry *)malloc(sizeof(*e));
    if (e == NULL)
        return NULL;
    memset(e, 0, sizeof(*e));
    e->path = _strdup(path);
    e->tex.name = name;
    tex_set_name(&e->tex, path);
    e->w = img.width;
    e->h = img.height;
    e->next = g_textures;
    g_textures = e;
    return &e->tex;
}

/* Port, not in the binary: a soft shadow under a key or button picture
 * (glyphs/), so it does not melt into the background. In its own canvas:
 * the picture a little smaller and up, the shadow below and right of it,
 * blurred. The game's own textures are left alone. */
void lime_glyph_shadow(uint8_t *px, int w, int h)
{
    const float s = 0.86f;
    int cw = (int)(w * s), ch = (int)(h * s);
    int ox = (w - cw) / 2 - w / 50, oy = (h - ch) / 2 - h / 30;
    int dx = w * 3 / 100 + 1, dy = h * 5 / 100 + 1, r = w * 3 / 100 + 1;
    float *src = (float *)calloc((size_t)w * h * 4, sizeof(float));
    float *sa = (float *)calloc((size_t)w * h, sizeof(float));
    float *tb = (float *)calloc((size_t)w * h, sizeof(float));
    int x, y, i, pass;

    if (!src || !sa || !tb || cw <= 0 || ch <= 0) {
        free(src); free(sa); free(tb);
        return;
    }
    /* The picture scaled into place, premultiplied, bilinear. */
    for (y = 0; y < ch; y++)
        for (x = 0; x < cw; x++) {
            float fx = (x + 0.5f) * w / cw - 0.5f, fy = (y + 0.5f) * h / ch - 0.5f;
            int x0 = (int)fx, y0 = (int)fy, X = x + ox, Y = y + oy, c;
            float ax = fx - x0, ay = fy - y0, acc[4] = { 0, 0, 0, 0 };

            if (fx < 0) { x0 = 0; ax = 0; }
            if (fy < 0) { y0 = 0; ay = 0; }
            if (X < 0 || X >= w || Y < 0 || Y >= h)
                continue;
            for (i = 0; i < 4; i++) {
                int u = x0 + (i & 1), v = y0 + (i >> 1);
                float wt = ((i & 1) ? ax : 1 - ax) * ((i >> 1) ? ay : 1 - ay);
                const uint8_t *p;
                if (u >= w) u = w - 1;
                if (v >= h) v = h - 1;
                p = px + ((size_t)v * w + u) * 4;
                for (c = 0; c < 3; c++)
                    acc[c] += wt * p[c] * p[3] / (255.0f * 255.0f);
                acc[3] += wt * p[3] / 255.0f;
            }
            memcpy(src + ((size_t)Y * w + X) * 4, acc, sizeof acc);
        }
    /* Its alpha moved down and right, then a box blur three times. */
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            int X = x - dx, Y = y - dy;
            sa[y * w + x] = X >= 0 && Y >= 0 ? src[((size_t)Y * w + X) * 4 + 3] : 0;
        }
    for (pass = 0; pass < 3; pass++) {
        for (y = 0; y < h; y++)
            for (x = 0; x < w; x++) {
                float t = 0;
                for (i = -r; i <= r; i++)
                    t += (x + i >= 0 && x + i < w) ? sa[y * w + x + i] : 0;
                tb[y * w + x] = t / (2 * r + 1);
            }
        for (y = 0; y < h; y++)
            for (x = 0; x < w; x++) {
                float t = 0;
                for (i = -r; i <= r; i++)
                    t += (y + i >= 0 && y + i < h) ? tb[(y + i) * w + x] : 0;
                sa[y * w + x] = t / (2 * r + 1);
            }
    }
    /* The picture over a black shadow. */
    for (i = 0; i < w * h; i++) {
        const float *p = src + (size_t)i * 4;
        float sh = sa[i] * 0.8f * (1 - p[3]), a = p[3] + sh;
        uint8_t *o = px + (size_t)i * 4;
        o[0] = (uint8_t)(a > 0 ? p[0] / a * 255.0f + 0.5f : 0);
        o[1] = (uint8_t)(a > 0 ? p[1] / a * 255.0f + 0.5f : 0);
        o[2] = (uint8_t)(a > 0 ? p[2] / a * 255.0f + 0.5f : 0);
        o[3] = (uint8_t)(a * 255.0f + 0.5f);
    }
    free(src); free(sa); free(tb);
}

/* Port, not in the binary: a picture the player added for the wide screens
 * (FrontEnd.c, the continue screens), from "aditional textures" beside the
 * game's res folder. Loaded once and kept, as the game's own. NULL when it
 * is not there. */
TEXTURE *lime_load_extra_texture(const char *file)
{
    char key[300], full[1200];
    TexEntry *e;
    LimeImage img;
    GLuint name = 0;

    snprintf(key, sizeof key, "extra:%s", file);
    e = tex_find(key);
    if (e)
        return &e->tex;
    snprintf(full, sizeof full, "%s/../aditional textures/%s",
             lime_platform_asset_root(), file);
    memset(&img, 0, sizeof(img));
    if (!try_one(full, &img))
        return NULL;
    if (strncmp(file, "glyphs/", 7) == 0)
        lime_glyph_shadow(img.rgba, img.width, img.height);
    glGenTextures(1, &name);
    tex_upload(name, &img);
    free(img.rgba);
    e = (TexEntry *)malloc(sizeof(*e));
    if (e == NULL)
        return NULL;
    memset(e, 0, sizeof(*e));
    e->path = _strdup(key);
    e->tex.name = name;
    tex_set_name(&e->tex, file);
    e->w = img.width;
    e->h = img.height;
    e->next = g_textures;
    g_textures = e;
    printf("extra texture: %s (%dx%d)\n", full, img.width, img.height);
    return &e->tex;
}

/* A texture lime_load_extra_texture gave: its picture's size. */
void lime_extra_texture_size(const TEXTURE *t, int *w, int *h)
{
    const TexEntry *e = (const TexEntry *)((const char *)t - offsetof(TexEntry, tex));

    *w = t ? e->w : 0;
    *h = t ? e->h : 0;
}

/* Port, not in the binary: the settings menu's TEXTURES (settings_menu.c).
 * Every texture loaded so far is loaded again at the quality set now, into
 * the same GL object, so what the game holds stays valid. Where the game
 * picks a file by quality (UseLOWAssets' _LOW sheets, the fighters'
 * _DIFFUSE_LITE skins, Players.c port_load_lite), the other file is taken:
 * `use_low` is UseLOWAssets as SetupFEScale now sets it. A _LOW sheet
 * becomes its full one and back; a LITE skin becomes the full one (and stays
 * so until the next start, as at load time going down is not undone). */
void lime_textures_requality(int use_low)
{
    static const char *const from[3] = { "_DIFFUSE2_LITE.", "_DIFFUSE_LITE2.",
                                         "_DIFFUSE_LITE." };
    static const char *const to[3]   = { "_DIFFUSE2.", "_DIFFUSE2.",
                                         "_DIFFUSE." };
    TexEntry *e;
    int n = 0;

    for (e = g_textures; e; e = e->next) {
        char file[300], *low, *dot;
        LimeImage img;
        int i, got = 0;

        memset(&img, 0, sizeof(img));
        snprintf(file, sizeof file, "%s", e->path);
        low = strstr(file, "_LOW.");
        if (!use_low) {
            if (low)                        /* _LOW sheet -> the full one */
                memmove(low, low + 4, strlen(low + 4) + 1);
            for (i = 0; i < 3 && PortTextureQuality > 0; i++) {
                char *hit = strstr(file, from[i]);
                if (hit) {
                    char big[300];
                    snprintf(big, sizeof big, "%.*s%s%s", (int)(hit - file),
                             file, to[i], hit + strlen(from[i]));
                    got = load_rgba(big, &img);
                    break;
                }
            }
        } else if (!low && (dot = strrchr(file, '.')) != NULL
                   && strlen(file) + 5 < sizeof file) {
            char lo[300];                   /* the full sheet -> its _LOW */
            snprintf(lo, sizeof lo, "%.*s_LOW%s", (int)(dot - file), file, dot);
            got = load_rgba(lo, &img);
        }
        if (!got && !load_rgba(file, &img))
            continue;
        if (PortTextureQuality >= 2)
            upscale_small(&img);
        tex_upload(e->tex.name, &img);
        e->w = img.width;
        e->h = img.height;
        free(img.rgba);
        n++;
    }
    printf("textures: %d loaded again at quality %d\n", n, PortTextureQuality);
}

void limeDeleteTexture(TEXTURE *tex)
{
    /* Kept: the menus delete and reload the same sheets between screens, and
     * dropping the GL object each time would undo the cache above. The whole
     * set goes when the process does. */
    (void)tex;
}


/* ------------------------------------------------------------ blend state */

void limeEnableAlphaBlending_Additive(void)
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
}

void limeEnableAlphaBlending_Basic(void)
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void limeDisableAlphaBlending(void)
{
    glDisable(GL_BLEND);
}


/* ------------------------------------------------------------------- 2D */

void limeSet2DDrawing(void)
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    /* Y increases downward: the origin is the top-left corner, which is where
     * the front end puts (0,0) -- FE_Y(24) is 24 pixels below the top. */
    glOrtho(0.0, (double)g_screen_w, (double)g_screen_h, 0.0, -1.0, 1.0);

    /* **MODULATE, explicitly.** The front end colours its text by handing the
     * sprite call a row of `mmfontcol` and letting it multiply against a white
     * glyph -- that is how "More Games" comes out red while the other four are
     * white, and how a held entry turns yellow. Under GL_REPLACE the texture
     * wins, the vertex colour is discarded, and every string draws white,
     * which is exactly what this was doing.
     *
     * The spec's default is MODULATE, which is why this looked unnecessary. It
     * is not: nothing says what the last thing to touch the texture unit left
     * behind. The original does not rely on the default either -- Particles.c
     * sets it by hand, and so does runtime/main.c. This was the one draw path
     * that assumed. */
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    /* The rest of armv7 0x00066da0: cull off, modelview reset -- and nothing
     * else. The binary does NOT touch the depth test, texturing or blending
     * here; this used to disable the first and enable the other two. Each
     * sprite enables texturing for itself (below), and the callers set
     * blending and depth as they want them -- FE_Task_Main_Menu, for one,
     * calls limeEnableAlphaBlending_Basic just before this. */
    glDisable(GL_CULL_FACE);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

/* --------------------------------------------------------------------- 3D */

extern float limeSidewaysMat[16];

/* **The port's stand-in for holding the phone sideways.** Not in the binary.
 *
 * On the iPhone GL draws onto the PORTRAIT panel and the game turns itself onto
 * it: the binary's limeSet2DDrawing is `limeSidewaysMat * glOrthof(0, 480,
 * 320, 0)`, and LIMEDS_Set3dMode is `perspective * limeSidewaysMat`, with
 * limeSidewaysMat = RotMatrixZ(-pi/2) from limeBegin.
 *
 * This window is already landscape, so limeSet2DDrawing above draws the ortho
 * with no rotation at all -- which is the iPhone's image with the panel's
 * rotation undone. The 3D path has to undo the same rotation, or the 0.6
 * aspect lands on the wrong axis. LIMEDS_Set3dMode calls this right after its
 * projection glLoadIdentity, so the stack becomes
 *
 *      inverse(limeSidewaysMat) * perspective * limeSidewaysMat
 *
 * A rotation's inverse is its transpose. limeSidewaysMat is the identity when
 * the device is not sideways, and then this is a no-op. */
void limePortDisplayRotation(void)
{
    const float *s = limeSidewaysMat;
    float t[16] = {
        s[0], s[4], s[8],  0.0f,
        s[1], s[5], s[9],  0.0f,
        s[2], s[6], s[10], 0.0f,
        0.0f, 0.0f, 0.0f,  1.0f,
    };

    glMultMatrixf(t);
}

/* armv7 0x00065e54 / 0x00065e60 / 0x0006699c. */
void limeEnableDepthWrites(void)  { glDepthMask(GL_TRUE); }
void limeDisableDepthWrites(void) { glDepthMask(GL_FALSE); }
void limeGetCurrentModelMatrix(float *out)
{
    glGetFloatv(GL_MODELVIEW_MATRIX, out);
}

void limeEnableDepthTest(void)  { glEnable(GL_DEPTH_TEST); }
void limeDisableDepthTest(void) { glDisable(GL_DEPTH_TEST); }
void limeClearDepthBuffer(void) { glClear(GL_DEPTH_BUFFER_BIT); }

/* armv7 0x00066e68: glColorMask on the four arguments, each `uxtb`-narrowed.
 * The game calls it as (1, 1, 1, 0) -- colour on, ALPHA off -- around the
 * passes that must not disturb destination alpha. It used to take one int and
 * force alpha on, so that call wrote alpha. */
void limeSetColourMask(long r, long g, long b, long a)
{
    glColorMask((GLboolean)(unsigned char)r, (GLboolean)(unsigned char)g,
                (GLboolean)(unsigned char)b, (GLboolean)(unsigned char)a);
}


/* --------------------------------------------------------------- sprites */

/* Port, not in the binary: the edge clamp.
 *
 * Every texture keeps GL_REPEAT, as in the binary (see limeLoadTexture), and
 * with linear filtering a draw that reaches a texture's edge then blends in
 * the OPPOSITE edge. Art with a transparent or black margin -- a select-screen
 * portrait is u 0..1 with its right part empty -- shows that as a 1-pixel
 * light line along the edge at PC resolutions. A draw whose texture
 * coordinates all stay inside 0..1 never needs REPEAT, so it is drawn with
 * CLAMP_TO_EDGE and REPEAT is put back after; a draw that runs past 1.0 (the
 * font's second sheet, a tiled stage floor) keeps REPEAT untouched.
 *
 * `uv` is the first U, `stride` the distance between vertices in floats;
 * `idx` the indices drawn, or NULL for vertices 0..n-1. Returns whether it
 * clamped, for lime_port_edge_restore. */
#define EDGE_EPS 0.0001f

static int edge_clamp_if_inside(float u0, float u1, float v0, float v1)
{
    if (u0 < -EDGE_EPS || u1 > 1.0f + EDGE_EPS ||
        v0 < -EDGE_EPS || v1 > 1.0f + EDGE_EPS)
        return 0;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F);  /* CLAMP_TO_EDGE */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F);
    return 1;
}

int lime_port_edge_clamp(const float *uv, int stride,
                         const unsigned short *idx, int n)
{
    float u0 = 1e30f, u1 = -1e30f, v0 = 1e30f, v1 = -1e30f;
    int i;

    if (uv == NULL || n <= 0)
        return 0;
    for (i = 0; i < n; i++) {
        const float *t = uv + (size_t)(idx ? idx[i] : i) * (size_t)stride;
        if (t[0] < u0) u0 = t[0];
        if (t[0] > u1) u1 = t[0];
        if (t[1] < v0) v0 = t[1];
        if (t[1] > v1) v1 = t[1];
    }
    return edge_clamp_if_inside(u0, u1, v0, v1);
}

void lime_port_edge_restore(int clamped)
{
    if (!clamped)
        return;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

static void quad(TEXTURE *page, float x, float y, float w, float h,
                 float u, float v, float du, float dv,
                 const float *colour, float angle)
{
    const float r = colour ? colour[0] : 1.0f;
    const float g = colour ? colour[1] : 1.0f;
    const float b = colour ? colour[2] : 1.0f;
    const float a = colour ? colour[3] : 1.0f;

    if (page == NULL)
        return;

    /* armv7 0x00065e7c: depth writes off for the quad and back on after,
     * texturing switched on by the sprite itself. */
    glDepthMask(GL_FALSE);
    glEnable(GL_TEXTURE_2D);
    glColor4f(r, g, b, a);
    glBindTexture(GL_TEXTURE_2D, (GLuint)page->name);

    if (angle != 0.0f) {
        glPushMatrix();
        glTranslatef(x, y, 0.0f);
        glRotatef(angle, 0.0f, 0.0f, 1.0f);
        glTranslatef(-x, -y, 0.0f);
    }

    /* `du`/`dv` are extents, and a negative one mirrors -- the front end flips
     * a sprite that way rather than carrying a second texture. Adding the
     * extent to the origin handles both signs without a branch. */
    /* edge clamp, see lime_port_edge_clamp */
    const int clamp = edge_clamp_if_inside(du < 0.0f ? u + du : u,
                                           du < 0.0f ? u : u + du,
                                           dv < 0.0f ? v + dv : v,
                                           dv < 0.0f ? v : v + dv);
    glBegin(GL_QUADS);
        glTexCoord2f(u,      v);       glVertex2f(x,     y);
        glTexCoord2f(u + du, v);       glVertex2f(x + w, y);
        glTexCoord2f(u + du, v + dv);  glVertex2f(x + w, y + h);
        glTexCoord2f(u,      v + dv);  glVertex2f(x,     y + h);
    glEnd();

    lime_port_edge_restore(clamp);

    if (angle != 0.0f)
        glPopMatrix();

    glDepthMask(GL_TRUE);
    g_sprites++;
}

void limeDrawSprite(TEXTURE *page, float x, float y, float w, float h,
                    float u, float v, float du, float dv,
                    const float *colour)
{
    quad(page, x, y, w, h, u, v, du, dv, colour, 0.0f);
}

void limeDrawRotSpriteFromTopLeft(TEXTURE *page, float x, float y,
                                  float w, float h, float u, float v,
                                  float du, float dv, float angle,
                                  const float *colour)
{
    quad(page, x, y, w, h, u, v, du, dv, colour, angle);
}

void lime_platform_last_sprite(float *out8)
{
    /* The headless build records the last sprite so a test can inspect it.
     * Here the screen is the record. */
    memset(out8, 0, 8 * sizeof(float));
}

void limeFillRect(float x, float y, float w, float h,
                  float r, float g, float b, float a)
{
    /* armv7 0x000666ac: depth writes off, texturing off -- and LEFT off, the
     * next sprite turns it back on -- then depth writes back on. */
    glDepthMask(GL_FALSE);
    glColor4f(r, g, b, a);
    glDisable(GL_TEXTURE_2D);
    glBegin(GL_QUADS);
        glVertex2f(x,     y);
        glVertex2f(x + w, y);
        glVertex2f(x + w, y + h);
        glVertex2f(x,     y + h);
    glEnd();
    glDepthMask(GL_TRUE);
    g_fills++;
}


/* The character models on the select screen go through the array pointers and
 * `glDrawElements`, all of which opengl32 provides unchanged. Nothing to add
 * here. */


/* ------------------------------------------------------- face-me sprites
 *
 * Billboards for the fight: blood drops, blood pools and Scorpion's spear.
 * Transcribed from the binary (`python tools/cd.py limeDrawFaceMeSprite
 * limeDrawFaceUpSprite limeDrawFaceMeSpriteWH`); the arithmetic is 2-lane
 * NEON there and is written out per lane here.
 *
 * `m` is FaceMeMatrix, the camera's rotation, so an offset run through
 * RotVector(m, ...) lies in the screen plane: the quad faces the viewer.
 *
 *   FaceMe / FaceUp (tex, m, x, y, z, u0, v0, du, dv, size, r, g, b, a)
 *     h      = size / 2
 *     base   = (x, y + h, z) for all four corners -- the quad sits ON the
 *              point, not around it
 *     corner = base + RotVector(m, offset), offsets in strip order
 *              FaceMe: (-h,-h,0) (+h,-h,0) (-h,+h,0) (+h,+h,0)
 *              FaceUp: (-h,-h,0) (+h,0,-h) (-h,0,+h) (+h,0,+h)
 *              -- FaceUp's first corner really is FaceMe's: the binary
 *              differs from FaceMe only in the last three offsets. Kept.
 *     uv     = (u0, 1-v0) (u0+du, 1-v0) (u0, 1-(v0+dv)) (u0+du, 1-(v0+dv))
 *     colour = rgba * 255 as bytes, the same for every corner
 *
 *   FaceMeWH (tex, m, x, y, z, u0, v0, du, dv, w, h, r, g, b, a, -, -)
 *     corner = (x, y, z) + RotVector(m, (0,0,0) (w,0,0) (0,h,0) (w,h,0))
 *              -- anchored at a corner, not centred
 *     uv     as above; colour as four FLOATS
 *     The two trailing arguments are stored by the one caller
 *     (RenderExtras) and never read.
 *
 * All three bind tex->name, enable GL_TEXTURE_2D, draw a 4-vertex
 * GL_TRIANGLE_STRIP from client arrays and disable GL_TEXTURE_2D; WH also
 * disables GL_VERTEX_ARRAY. The other client arrays are left enabled, as
 * on device.
 */
static void face_corner(const float *m, const float *base, float ox, float oy,
                        float oz, float *out)
{
    limeVECTOR3 in, r;

    in.x = ox; in.y = oy; in.z = oz;
    RotVector(m, &in, &r);
    out[0] = base[0] + r.x;
    out[1] = base[1] + r.y;
    out[2] = base[2] + r.z;
}

static void face_uv(float *uv, float u0, float v0, float du, float dv)
{
    uv[0] = u0;      uv[1] = 1.0f - v0;
    uv[2] = u0 + du; uv[3] = 1.0f - v0;
    uv[4] = u0;      uv[5] = 1.0f - (v0 + dv);
    uv[6] = u0 + du; uv[7] = 1.0f - (v0 + dv);
}

static void face_draw(TEXTURE *tex, const float *xyz, const float *uv,
                      const void *colour, GLenum colour_type)
{
    glBindTexture(GL_TEXTURE_2D, (GLuint)tex->name);
    glEnable(GL_TEXTURE_2D);
    glColorPointer(4, colour_type, 0, colour);
    glEnableClientState(GL_COLOR_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, xyz);
    glEnableClientState(GL_VERTEX_ARRAY);
    glTexCoordPointer(2, GL_FLOAT, 0, uv);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    {
        const int clamp = lime_port_edge_clamp(uv, 2, NULL, 4);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        lime_port_edge_restore(clamp);
    }
    glDisable(GL_TEXTURE_2D);
}

static void face_sprite(int up, TEXTURE *tex, const float *m,
                        float x, float y, float z,
                        float u0, float v0, float du, float dv, float size,
                        float r, float g, float b, float a)
{
    float h = size * 0.5f, base[3], xyz[12], uv[8];
    unsigned char rgba[16];
    int i;

    base[0] = x; base[1] = y + h; base[2] = z;
    face_corner(m, base, -h, -h, 0.0f, xyz + 0);
    if (up) {
        face_corner(m, base,  h, 0.0f, -h, xyz + 3);
        face_corner(m, base, -h, 0.0f,  h, xyz + 6);
        face_corner(m, base,  h, 0.0f,  h, xyz + 9);
    } else {
        face_corner(m, base,  h, -h, 0.0f, xyz + 3);
        face_corner(m, base, -h,  h, 0.0f, xyz + 6);
        face_corner(m, base,  h,  h, 0.0f, xyz + 9);
    }
    face_uv(uv, u0, v0, du, dv);

    /* vcvt.u32.f32: truncation toward zero, saturating at 0 */
    for (i = 0; i < 4; i++) {
        float c[4];
        int k;
        c[0] = r * 255.0f; c[1] = g * 255.0f; c[2] = b * 255.0f; c[3] = a * 255.0f;
        for (k = 0; k < 4; k++)
            rgba[i * 4 + k] = (unsigned char)(c[k] > 0.0f ? (unsigned)c[k] : 0u);
    }
    face_draw(tex, xyz, uv, rgba, GL_UNSIGNED_BYTE);
}

void limeDrawFaceMeSprite(TEXTURE *tex, const float *m, float x, float y, float z,
                          float u0, float v0, float du, float dv, float size,
                          float r, float g, float b, float a)
{
    face_sprite(0, tex, m, x, y, z, u0, v0, du, dv, size, r, g, b, a);
}

void limeDrawFaceUpSprite(TEXTURE *tex, const float *m, float x, float y, float z,
                          float u0, float v0, float du, float dv, float size,
                          float r, float g, float b, float a)
{
    face_sprite(1, tex, m, x, y, z, u0, v0, du, dv, size, r, g, b, a);
}

void limeDrawFaceMeSpriteWH(TEXTURE *tex, const float *m, float x, float y, float z,
                            float u0, float v0, float du, float dv,
                            float w, float h, float r, float g, float b, float a,
                            float unused0, float unused1)
{
    float base[3], xyz[12], uv[8], rgba[16];
    int i;

    (void)unused0; (void)unused1;
    base[0] = x; base[1] = y; base[2] = z;
    xyz[0] = x; xyz[1] = y; xyz[2] = z;
    face_corner(m, base, w,    0.0f, 0.0f, xyz + 3);
    face_corner(m, base, 0.0f, h,    0.0f, xyz + 6);
    face_corner(m, base, w,    h,    0.0f, xyz + 9);
    face_uv(uv, u0, v0, du, dv);
    for (i = 0; i < 4; i++) {
        rgba[i * 4 + 0] = r; rgba[i * 4 + 1] = g;
        rgba[i * 4 + 2] = b; rgba[i * 4 + 3] = a;
    }
    face_draw(tex, xyz, uv, rgba, GL_FLOAT);
    glDisableClientState(GL_VERTEX_ARRAY);
}
