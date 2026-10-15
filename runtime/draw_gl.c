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
#include "lime/png.h"
#include "lime/pvr.h"

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

    for (e = 0; e < (int)(sizeof(exts) / sizeof(exts[0])); e++) {
        for (d = 0; d < (int)(sizeof(dirs) / sizeof(dirs[0])); d++) {
            if (exts[e] == NULL)
                snprintf(full, sizeof(full), "%s/%s%s", root, dirs[d], path);
            else
                snprintf(full, sizeof(full), "%s/%s%s%s",
                         root, dirs[d], stem, exts[e]);
            if (try_one(full, out))
                return 1;
        }
    }
    return 0;
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
    if (getenv("LIME_TRACE_TEX"))
        fprintf(stderr, "[tex] %s %dx%d\n", path, img.width, img.height);

    glGenTextures(1, &name);
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
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img.width, img.height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, img.rgba);
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
    glBegin(GL_QUADS);
        glTexCoord2f(u,      v);       glVertex2f(x,     y);
        glTexCoord2f(u + du, v);       glVertex2f(x + w, y);
        glTexCoord2f(u + du, v + dv);  glVertex2f(x + w, y + h);
        glTexCoord2f(u,      v + dv);  glVertex2f(x,     y + h);
    glEnd();

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
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
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
