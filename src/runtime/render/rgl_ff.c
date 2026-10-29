/*
 * rgl_ff.c -- OpenGL's fixed function, for the Vulkan and Direct3D 12
 * backends. Port, not in the binary.
 *
 * What the game calls (rgl_front.c sends it here as ff_glX when OpenGL is
 * not the one drawing) is kept as OpenGL 1.1 keeps it: three matrix stacks,
 * the enables, blending, the alpha test, depth, culling, the colour mask,
 * the viewport and scissor, the current colour and texture coordinate, the
 * texture environment, the bound texture and its parameters, the client
 * arrays and glBegin/glEnd, the pixel store, glPushAttrib, and the
 * framebuffer objects game_main.c draws its 3D resolution and antialiasing
 * through (fetched with plat_gl_proc, which rgl_proc answers).
 *
 * Each draw is assembled here: its vertices fetched from the arrays (or the
 * glBegin list), transformed by projection x modelview into clip space and
 * by the texture matrix, its primitives made triangles or lines (quads,
 * strips and fans split as OpenGL splits them, so the winding stays), and
 * handed to the backend with the state it needs as a pipeline key
 * (rgl_pipe) and the fragment shader's few numbers (rgl_frag). The shaders
 * do the rest: the texture environment (MODULATE / REPLACE) and the alpha
 * test, which OpenGL ES 2 and everything after it left to them.
 *
 * Only what the tree uses is kept: one texture unit (the game's
 * glActiveTexture is a no-op), no lighting or fog (the game only turns them
 * off), RGBA textures (others are converted), and lines drawn one pixel wide.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rgl.h"

#if defined(_WIN32) && !defined(_WIN64)
#  define FBAPI __stdcall          /* what game_main.c and draw_gl.c call through */
#else
#  define FBAPI
#endif

typedef unsigned int  GLenum, GLbitfield, GLuint;
typedef int           GLint, GLsizei;
typedef unsigned char GLboolean, GLubyte;
typedef float         GLfloat, GLclampf;
typedef double        GLdouble;
typedef void          GLvoid;

/* the enums, as numbers (no GL header here) */
#define E_POINTS 0x0000
#define E_LINES 0x0001
#define E_LINE_LOOP 0x0002
#define E_LINE_STRIP 0x0003
#define E_TRIANGLES 0x0004
#define E_TRIANGLE_STRIP 0x0005
#define E_TRIANGLE_FAN 0x0006
#define E_QUADS 0x0007
#define E_QUAD_STRIP 0x0008
#define E_POLYGON 0x0009
#define E_BYTE 0x1400
#define E_UNSIGNED_BYTE 0x1401
#define E_SHORT 0x1402
#define E_UNSIGNED_SHORT 0x1403
#define E_INT 0x1404
#define E_UNSIGNED_INT 0x1405
#define E_FLOAT 0x1406
#define E_DOUBLE 0x140A
#define E_FIXED 0x140C
#define E_MODELVIEW 0x1700
#define E_PROJECTION 0x1701
#define E_TEXTURE 0x1702
#define E_FRONT 0x0404
#define E_BACK 0x0405
#define E_FRONT_AND_BACK 0x0408
#define E_CULL_FACE 0x0B44
#define E_CULL_FACE_MODE 0x0B45
#define E_DEPTH_TEST 0x0B71
#define E_DEPTH_WRITEMASK 0x0B72
#define E_ALPHA_TEST 0x0BC0
#define E_BLEND 0x0BE2
#define E_BLEND_DST 0x0BE0
#define E_BLEND_SRC 0x0BE1
#define E_SCISSOR_TEST 0x0C11
#define E_SCISSOR_BOX 0x0C10
#define E_TEXTURE_2D 0x0DE1
#define E_VIEWPORT 0x0BA2
#define E_MATRIX_MODE 0x0BA0
#define E_MODELVIEW_MATRIX 0x0BA6
#define E_PROJECTION_MATRIX 0x0BA7
#define E_TEXTURE_MATRIX 0x0BA8
#define E_CURRENT_COLOR 0x0B00
#define E_COLOR_CLEAR_VALUE 0x0C22
#define E_COLOR_WRITEMASK 0x0C23
#define E_TEXTURE_BINDING_2D 0x8069
#define E_MAX_TEXTURE_SIZE 0x0D33
#define E_MAX_SAMPLES 0x8D57
#define E_SAMPLES 0x80A9
#define E_VERTEX_ARRAY 0x8074
#define E_NORMAL_ARRAY 0x8075
#define E_COLOR_ARRAY 0x8076
#define E_TEXTURE_COORD_ARRAY 0x8078
#define E_PACK_ALIGNMENT 0x0D05
#define E_UNPACK_ALIGNMENT 0x0CF5
#define E_ALPHA 0x1906
#define E_RGB 0x1907
#define E_RGBA 0x1908
#define E_BGRA 0x80E1
#define E_LUMINANCE 0x1909
#define E_LUMINANCE_ALPHA 0x190A
#define E_UNSIGNED_SHORT_4_4_4_4 0x8033
#define E_UNSIGNED_SHORT_5_5_5_1 0x8034
#define E_UNSIGNED_SHORT_5_6_5 0x8363
#define E_TEXTURE_MAG_FILTER 0x2800
#define E_TEXTURE_MIN_FILTER 0x2801
#define E_TEXTURE_WRAP_S 0x2802
#define E_TEXTURE_WRAP_T 0x2803
#define E_NEAREST 0x2600
#define E_LINEAR 0x2601
#define E_NEAREST_MIPMAP_NEAREST 0x2700
#define E_LINEAR_MIPMAP_NEAREST 0x2701
#define E_NEAREST_MIPMAP_LINEAR 0x2702
#define E_LINEAR_MIPMAP_LINEAR 0x2703
#define E_REPEAT 0x2901
#define E_CLAMP 0x2900
#define E_CLAMP_TO_EDGE 0x812F
#define E_GENERATE_MIPMAP 0x8191
#define E_TEXTURE_MAX_ANISOTROPY 0x84FE
#define E_MAX_TEXTURE_MAX_ANISOTROPY 0x84FF
#define E_TEXTURE_ENV 0x2300
#define E_TEXTURE_ENV_MODE 0x2200
#define E_MODULATE 0x2100
#define E_REPLACE 0x1E01
#define E_DECAL 0x2101
#define E_ZERO 0
#define E_ONE 1
#define E_SRC_COLOR 0x0300
#define E_ONE_MINUS_SRC_COLOR 0x0301
#define E_SRC_ALPHA 0x0302
#define E_ONE_MINUS_SRC_ALPHA 0x0303
#define E_DST_ALPHA 0x0304
#define E_ONE_MINUS_DST_ALPHA 0x0305
#define E_DST_COLOR 0x0306
#define E_ONE_MINUS_DST_COLOR 0x0307
#define E_NEVER 0x0200
#define E_ALWAYS 0x0207
#define E_COLOR_BUFFER_BIT 0x4000
#define E_DEPTH_BUFFER_BIT 0x0100
#define E_VENDOR 0x1F00
#define E_RENDERER 0x1F01
#define E_VERSION 0x1F02
#define E_EXTENSIONS 0x1F03
#define E_FRAMEBUFFER 0x8D40
#define E_READ_FRAMEBUFFER 0x8CA8
#define E_DRAW_FRAMEBUFFER 0x8CA9
#define E_RENDERBUFFER 0x8D41
#define E_COLOR_ATTACHMENT0 0x8CE0
#define E_DEPTH_ATTACHMENT 0x8D00
#define E_DEPTH_STENCIL_ATTACHMENT 0x821A
#define E_FRAMEBUFFER_COMPLETE 0x8CD5
#define E_FRAMEBUFFER_UNSUPPORTED 0x8CDD

static const rgl_backend *B;

/* ------------------------------------------------------------- matrices */

typedef struct { float m[16]; } mat4;     /* column-major, as GL */

static const mat4 k_ident = { { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 } };

static mat4 mul(const mat4 *a, const mat4 *b)
{
    mat4 r;
    int i, j;

    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
            r.m[j * 4 + i] = a->m[0 * 4 + i] * b->m[j * 4 + 0] + a->m[1 * 4 + i] * b->m[j * 4 + 1]
                           + a->m[2 * 4 + i] * b->m[j * 4 + 2] + a->m[3 * 4 + i] * b->m[j * 4 + 3];
    return r;
}

#define STACK 32
static struct { mat4 s[STACK]; int top; } g_mv, g_pr, g_tx;

/* ---------------------------------------------------------------- state */

typedef struct {
    unsigned char blend, depth_test, cull, alpha_test, texture, scissor;
    GLenum src, dst, alpha_func, cull_mode, matrix_mode, env, shade;
    float alpha_ref;
    unsigned char depth_mask, mask[4];
    float clear[4], color[4], tc[2], line;
    GLint viewport[4], box[4];
    GLuint bound;
} ff_state;

static ff_state S;
static ff_state g_attr[8];
static int g_nattr;
static int g_pack = 4, g_unpack = 4;
static int g_mvp_dirty = 1;
static mat4 g_mvp;

typedef struct { int on, size; GLenum type; int stride; const unsigned char *p; } carray;
static carray g_va, g_ca, g_ta;

/* textures by GL name */
typedef struct {
    int used, handle, w, h, levels;
    GLenum min, mag, ws, wt;
    int gen_mip;
    float aniso;
} ff_tex;
static ff_tex *g_tex;
static int g_ntex, g_tex_next = 1;

/* framebuffer objects */
typedef struct { int used; GLenum fmt; int samples, w, h; } ff_rb;
typedef struct { int used; GLuint color, depth; int target, made; } ff_fb;
static ff_rb *g_rb;
static ff_fb *g_fb;
static int g_nrb, g_nfb, g_rb_next = 1, g_fb_next = 1;
static GLuint g_draw_fb, g_read_fb, g_bound_rb;

/* the vertices of a glBegin */
typedef struct { float x, y, z, w; float c[4]; float s, t; } ff_imm;
static ff_imm *g_imm;
static int g_nimm, g_cimm;
static GLenum g_begin = (GLenum)-1;

/* output scratch */
static rgl_vtx *g_out;
static int g_cout;

static void reset_state(void)
{
    memset(&S, 0, sizeof S);
    S.src = E_ONE;
    S.dst = E_ZERO;
    S.alpha_func = E_ALWAYS;
    S.cull_mode = E_BACK;
    S.matrix_mode = E_MODELVIEW;
    S.env = E_MODULATE;
    S.depth_mask = 1;
    S.mask[0] = S.mask[1] = S.mask[2] = S.mask[3] = 1;
    S.color[0] = S.color[1] = S.color[2] = S.color[3] = 1.0f;
    S.line = 1.0f;
    g_mv.top = g_pr.top = g_tx.top = 0;
    g_mv.s[0] = g_pr.s[0] = g_tx.s[0] = k_ident;
    g_mvp_dirty = 1;
}

void rgl_ff_start(const rgl_backend *b, int width, int height)
{
    B = b;
    reset_state();
    S.viewport[2] = S.box[2] = width;
    S.viewport[3] = S.box[3] = height;
    g_draw_fb = g_read_fb = 0;
}

static void scene_flush(void);
static void SH_end_frame(void);
static int hold_clear(const rgl_rect *sc, int color, const float rgba[4], int depth);
static void shadow_forget(void);

void rgl_ff_stop(void)
{
    scene_flush();
    B = NULL;
    shadow_forget();                    /* its target went with the backend */
}

/* the end of a frame, before it is presented: what the scene held back
 * drawn, and the next frame's scene starts with no caster */
void rgl_ff_frame(void)
{
    scene_flush();
    SH_end_frame();
}

/* ------------------------------------------------------------- matrices */

static mat4 *cur(void)
{
    if (S.matrix_mode == E_PROJECTION) return &g_pr.s[g_pr.top];
    if (S.matrix_mode == E_TEXTURE) return &g_tx.s[g_tx.top];
    return &g_mv.s[g_mv.top];
}

static void mult(const mat4 *m)
{
    mat4 *c = cur();
    *c = mul(c, m);
    g_mvp_dirty = 1;
}

void ff_glMatrixMode(GLenum m) { S.matrix_mode = m; }
void ff_glLoadIdentity(void) { *cur() = k_ident; g_mvp_dirty = 1; }
void ff_glLoadMatrixf(const GLfloat *m) { memcpy(cur()->m, m, sizeof(mat4)); g_mvp_dirty = 1; }
void ff_glMultMatrixf(const GLfloat *m) { mat4 t; memcpy(t.m, m, sizeof t); mult(&t); }

void ff_glPushMatrix(void)
{
    if (S.matrix_mode == E_PROJECTION) { if (g_pr.top < STACK - 1) { g_pr.s[g_pr.top + 1] = g_pr.s[g_pr.top]; g_pr.top++; } }
    else if (S.matrix_mode == E_TEXTURE) { if (g_tx.top < STACK - 1) { g_tx.s[g_tx.top + 1] = g_tx.s[g_tx.top]; g_tx.top++; } }
    else if (g_mv.top < STACK - 1) { g_mv.s[g_mv.top + 1] = g_mv.s[g_mv.top]; g_mv.top++; }
}

void ff_glPopMatrix(void)
{
    if (S.matrix_mode == E_PROJECTION) { if (g_pr.top > 0) g_pr.top--; }
    else if (S.matrix_mode == E_TEXTURE) { if (g_tx.top > 0) g_tx.top--; }
    else if (g_mv.top > 0) g_mv.top--;
    g_mvp_dirty = 1;
}

void ff_glTranslatef(GLfloat x, GLfloat y, GLfloat z)
{
    mat4 t = k_ident;
    t.m[12] = x; t.m[13] = y; t.m[14] = z;
    mult(&t);
}

void ff_glScalef(GLfloat x, GLfloat y, GLfloat z)
{
    mat4 t = k_ident;
    t.m[0] = x; t.m[5] = y; t.m[10] = z;
    mult(&t);
}

void ff_glRotatef(GLfloat a, GLfloat x, GLfloat y, GLfloat z)
{
    mat4 t = k_ident;
    float l = sqrtf(x * x + y * y + z * z), c, s, k;

    if (l <= 0.0f)
        return;
    x /= l; y /= l; z /= l;
    c = cosf(a * 3.14159265358979f / 180.0f);
    s = sinf(a * 3.14159265358979f / 180.0f);
    k = 1.0f - c;
    t.m[0] = x * x * k + c;     t.m[4] = x * y * k - z * s; t.m[8]  = x * z * k + y * s;
    t.m[1] = y * x * k + z * s; t.m[5] = y * y * k + c;     t.m[9]  = y * z * k - x * s;
    t.m[2] = x * z * k - y * s; t.m[6] = y * z * k + x * s; t.m[10] = z * z * k + c;
    mult(&t);
}

void ff_glOrtho(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f)
{
    mat4 o = k_ident;
    o.m[0] = (float)(2.0 / (r - l));
    o.m[5] = (float)(2.0 / (t - b));
    o.m[10] = (float)(-2.0 / (f - n));
    o.m[12] = (float)(-(r + l) / (r - l));
    o.m[13] = (float)(-(t + b) / (t - b));
    o.m[14] = (float)(-(f + n) / (f - n));
    mult(&o);
}

/* --------------------------------------------------------------- simple */

static unsigned char *cap(GLenum c)
{
    switch (c) {
    case E_BLEND:        return &S.blend;
    case E_DEPTH_TEST:   return &S.depth_test;
    case E_CULL_FACE:    return &S.cull;
    case E_ALPHA_TEST:   return &S.alpha_test;
    case E_TEXTURE_2D:   return &S.texture;
    case E_SCISSOR_TEST: return &S.scissor;
    }
    return NULL;                        /* fog, lighting, ...: off anyway */
}

void ff_glEnable(GLenum c) { unsigned char *p = cap(c); if (p) *p = 1; }
void ff_glDisable(GLenum c) { unsigned char *p = cap(c); if (p) *p = 0; }

static carray *arr(GLenum a)
{
    return a == E_VERTEX_ARRAY ? &g_va : a == E_COLOR_ARRAY ? &g_ca
         : a == E_TEXTURE_COORD_ARRAY ? &g_ta : NULL;
}
void ff_glEnableClientState(GLenum a) { carray *p = arr(a); if (p) p->on = 1; }
void ff_glDisableClientState(GLenum a) { carray *p = arr(a); if (p) p->on = 0; }

static void set_array(carray *a, GLint n, GLenum t, GLsizei s, const GLvoid *p)
{
    a->size = n; a->type = t; a->stride = s; a->p = (const unsigned char *)p;
}
void ff_glVertexPointer(GLint n, GLenum t, GLsizei s, const GLvoid *p) { set_array(&g_va, n, t, s, p); }
void ff_glColorPointer(GLint n, GLenum t, GLsizei s, const GLvoid *p) { set_array(&g_ca, n, t, s, p); }
void ff_glTexCoordPointer(GLint n, GLenum t, GLsizei s, const GLvoid *p) { set_array(&g_ta, n, t, s, p); }
void ff_glNormalPointer(GLenum t, GLsizei s, const GLvoid *p) { (void)t; (void)s; (void)p; }

void ff_glAlphaFunc(GLenum f, GLclampf r) { S.alpha_func = f; S.alpha_ref = r; }
void ff_glBlendFunc(GLenum s, GLenum d) { S.src = s; S.dst = d; }
void ff_glCullFace(GLenum m) { S.cull_mode = m; }
void ff_glDepthMask(GLboolean f) { S.depth_mask = f != 0; }
void ff_glColorMask(GLboolean r, GLboolean g, GLboolean b, GLboolean a)
{
    S.mask[0] = r != 0; S.mask[1] = g != 0; S.mask[2] = b != 0; S.mask[3] = a != 0;
}
void ff_glClearColor(GLclampf r, GLclampf g, GLclampf b, GLclampf a)
{
    S.clear[0] = r; S.clear[1] = g; S.clear[2] = b; S.clear[3] = a;
}
void ff_glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
    S.color[0] = r; S.color[1] = g; S.color[2] = b; S.color[3] = a;
}
void ff_glColor3f(GLfloat r, GLfloat g, GLfloat b) { ff_glColor4f(r, g, b, 1.0f); }
void ff_glTexCoord2f(GLfloat s, GLfloat t) { S.tc[0] = s; S.tc[1] = t; }
void ff_glShadeModel(GLenum m) { S.shade = m; }
void ff_glLineWidth(GLfloat w) { S.line = w; }
void ff_glHint(GLenum t, GLenum m) { (void)t; (void)m; }
void ff_glFogf(GLenum p, GLfloat v) { (void)p; (void)v; }
void ff_glFogfv(GLenum p, const GLfloat *v) { (void)p; (void)v; }
void ff_glFogi(GLenum p, GLint v) { (void)p; (void)v; }
void ff_glPolygonOffset(GLfloat f, GLfloat u) { (void)f; (void)u; }
GLenum ff_glGetError(void) { return 0; }
void ff_glViewport(GLint x, GLint y, GLsizei w, GLsizei h)
{
    S.viewport[0] = x; S.viewport[1] = y; S.viewport[2] = w; S.viewport[3] = h;
}
void ff_glScissor(GLint x, GLint y, GLsizei w, GLsizei h)
{
    S.box[0] = x; S.box[1] = y; S.box[2] = w; S.box[3] = h;
}
void ff_glPixelStorei(GLenum p, GLint v)
{
    if (p == E_PACK_ALIGNMENT) g_pack = v;
    else if (p == E_UNPACK_ALIGNMENT) g_unpack = v;
}
void ff_glTexEnvf(GLenum t, GLenum p, GLfloat v)
{
    if (t == E_TEXTURE_ENV && p == E_TEXTURE_ENV_MODE)
        S.env = (GLenum)v;
}
void ff_glTexEnvi(GLenum t, GLenum p, GLint v) { ff_glTexEnvf(t, p, (GLfloat)v); }

/* GL_ALL_ATTRIB_BITS or not, all of it: the debug menu is the one caller */
void ff_glPushAttrib(GLbitfield m)
{
    (void)m;
    if (g_nattr < 8)
        g_attr[g_nattr++] = S;
}
void ff_glPopAttrib(void)
{
    if (g_nattr > 0) {
        S = g_attr[--g_nattr];
        g_mvp_dirty = 1;
    }
}

void ff_glFinish(void)
{
    scene_flush();
    if (B && B->finish)
        B->finish();
}

/* ------------------------------------------------------------- textures */

static ff_tex *tex(GLuint name, int make)
{
    if (name == 0)
        return NULL;
    if ((int)name >= g_ntex) {
        int n;
        if (!make)
            return NULL;
        n = g_ntex ? g_ntex : 256;
        while (n <= (int)name)
            n *= 2;
        g_tex = (ff_tex *)realloc(g_tex, (size_t)n * sizeof *g_tex);
        memset(g_tex + g_ntex, 0, (size_t)(n - g_ntex) * sizeof *g_tex);
        g_ntex = n;
    }
    if (!g_tex[name].used) {
        if (!make)
            return NULL;
        memset(&g_tex[name], 0, sizeof g_tex[name]);
        g_tex[name].used = 1;
        g_tex[name].min = E_NEAREST_MIPMAP_LINEAR;   /* GL's defaults */
        g_tex[name].mag = E_LINEAR;
        g_tex[name].ws = g_tex[name].wt = E_REPEAT;
        g_tex[name].aniso = 1.0f;
    }
    return &g_tex[name];
}

void ff_glGenTextures(GLsizei n, GLuint *t)
{
    int i;
    for (i = 0; i < n; i++) {
        while (tex((GLuint)g_tex_next, 0))
            g_tex_next++;
        t[i] = (GLuint)g_tex_next;
        tex(t[i], 1);
        g_tex_next++;
    }
}

void ff_glDeleteTextures(GLsizei n, const GLuint *t)
{
    int i;
    for (i = 0; i < n; i++) {
        ff_tex *x = tex(t[i], 0);
        if (!x)
            continue;
        if (x->handle && B) {
            scene_flush();              /* a held draw may use it */
            B->tex_free(x->handle);
        }
        x->used = 0;
        x->handle = 0;
        if (S.bound == t[i])
            S.bound = 0;
    }
}

void ff_glBindTexture(GLenum target, GLuint name)
{
    (void)target;
    S.bound = name;
    tex(name, 1);
}

void ff_glTexParameteri(GLenum t, GLenum p, GLint v)
{
    ff_tex *x = tex(S.bound, 1);
    (void)t;
    if (!x)
        return;
    switch (p) {
    case E_TEXTURE_MIN_FILTER: x->min = (GLenum)v; break;
    case E_TEXTURE_MAG_FILTER: x->mag = (GLenum)v; break;
    case E_TEXTURE_WRAP_S: x->ws = (GLenum)v; break;
    case E_TEXTURE_WRAP_T: x->wt = (GLenum)v; break;
    case E_GENERATE_MIPMAP: x->gen_mip = v != 0; break;
    case E_TEXTURE_MAX_ANISOTROPY: x->aniso = (float)v; break;
    }
}

void ff_glTexParameterf(GLenum t, GLenum p, GLfloat v)
{
    if (p == E_TEXTURE_MAX_ANISOTROPY) {
        ff_tex *x = tex(S.bound, 1);
        if (x)
            x->aniso = v;
        return;
    }
    ff_glTexParameteri(t, p, (GLint)v);
}

/* the pixels as RGBA bytes, whatever they came as */
static unsigned char *to_rgba(int w, int h, GLenum f, GLenum type, const void *p, int align)
{
    unsigned char *o = (unsigned char *)calloc((size_t)w * h, 4);
    const unsigned char *s = (const unsigned char *)p;
    int x, y, bpp, row;

    if (!o || !p)
        return o;
    if (type == E_UNSIGNED_BYTE)
        bpp = f == E_RGBA || f == E_BGRA ? 4 : f == E_RGB ? 3 : f == E_LUMINANCE_ALPHA ? 2 : 1;
    else
        bpp = 2;
    row = w * bpp;
    if (align > 1)
        row = (row + align - 1) / align * align;
    for (y = 0; y < h; y++) {
        const unsigned char *r = s + (size_t)y * row;
        unsigned char *d = o + (size_t)y * w * 4;
        for (x = 0; x < w; x++, d += 4) {
            if (type == E_UNSIGNED_BYTE) {
                const unsigned char *q = r + x * bpp;
                switch (f) {
                case E_RGBA: d[0] = q[0]; d[1] = q[1]; d[2] = q[2]; d[3] = q[3]; break;
                case E_BGRA: d[0] = q[2]; d[1] = q[1]; d[2] = q[0]; d[3] = q[3]; break;
                case E_RGB: d[0] = q[0]; d[1] = q[1]; d[2] = q[2]; d[3] = 255; break;
                case E_LUMINANCE_ALPHA: d[0] = d[1] = d[2] = q[0]; d[3] = q[1]; break;
                case E_ALPHA: d[0] = d[1] = d[2] = 255; d[3] = q[0]; break;
                default: d[0] = d[1] = d[2] = q[0]; d[3] = 255; break;
                }
            } else {
                unsigned v = r[x * 2] | (r[x * 2 + 1] << 8);
                if (type == E_UNSIGNED_SHORT_5_6_5) {
                    d[0] = (unsigned char)((v >> 11) * 255 / 31); d[1] = (unsigned char)(((v >> 5) & 63) * 255 / 63);
                    d[2] = (unsigned char)((v & 31) * 255 / 31); d[3] = 255;
                } else if (type == E_UNSIGNED_SHORT_4_4_4_4) {
                    d[0] = (unsigned char)((v >> 12) * 17); d[1] = (unsigned char)(((v >> 8) & 15) * 17);
                    d[2] = (unsigned char)(((v >> 4) & 15) * 17); d[3] = (unsigned char)((v & 15) * 17);
                } else {
                    d[0] = (unsigned char)((v >> 11) * 255 / 31); d[1] = (unsigned char)(((v >> 6) & 31) * 255 / 31);
                    d[2] = (unsigned char)(((v >> 1) & 31) * 255 / 31); d[3] = (v & 1) ? 255 : 0;
                }
            }
        }
    }
    return o;
}

static int wants_mips(const ff_tex *x)
{
    return x->gen_mip || (x->min != E_NEAREST && x->min != E_LINEAR);
}

#define MAX_LEVELS 16

void ff_glTexImage2D(GLenum t, GLint level, GLint internal, GLsizei w, GLsizei h,
                     GLint border, GLenum f, GLenum type, const GLvoid *p)
{
    ff_tex *x = tex(S.bound, 1);
    unsigned char *lv[MAX_LEVELS];
    int n = 1, i;

    (void)t; (void)internal; (void)border;
    if (!x || !B || level != 0 || w <= 0 || h <= 0)
        return;
    lv[0] = to_rgba(w, h, f, type, p, g_unpack);
    if (!lv[0])
        return;
    if (wants_mips(x)) {
        /* each level a 2x2 box of the one above; odd sizes keep the edge */
        int cw = w, ch = h;
        while ((cw > 1 || ch > 1) && n < MAX_LEVELS) {
            int nw = cw > 1 ? cw / 2 : 1, nh = ch > 1 ? ch / 2 : 1, xx, yy, k;
            const unsigned char *s = lv[n - 1];
            unsigned char *d = (unsigned char *)malloc((size_t)nw * nh * 4);
            if (!d)
                break;
            for (yy = 0; yy < nh; yy++)
                for (xx = 0; xx < nw; xx++) {
                    int x0 = xx * 2 < cw ? xx * 2 : cw - 1, x1 = xx * 2 + 1 < cw ? xx * 2 + 1 : cw - 1;
                    int y0 = yy * 2 < ch ? yy * 2 : ch - 1, y1 = yy * 2 + 1 < ch ? yy * 2 + 1 : ch - 1;
                    for (k = 0; k < 4; k++)
                        d[(yy * nw + xx) * 4 + k] = (unsigned char)((
                            s[(y0 * cw + x0) * 4 + k] + s[(y0 * cw + x1) * 4 + k]
                          + s[(y1 * cw + x0) * 4 + k] + s[(y1 * cw + x1) * 4 + k] + 2) / 4);
                }
            lv[n++] = d;
            cw = nw;
            ch = nh;
        }
    }
    scene_flush();                      /* held draws see the texture as it was */
    if (!x->handle)
        x->handle = B->tex_new();
    B->tex_image(x->handle, w, h, n, (const unsigned char *const *)lv);
    x->w = w;
    x->h = h;
    x->levels = n;
    for (i = 0; i < n; i++)
        free(lv[i]);
}

void ff_glTexSubImage2D(GLenum t, GLint level, GLint x0, GLint y0, GLsizei w, GLsizei h,
                        GLenum f, GLenum type, const GLvoid *p)
{
    ff_tex *x = tex(S.bound, 0);
    unsigned char *px;

    (void)t;
    if (!x || !x->handle || !B || level != 0 || w <= 0 || h <= 0)
        return;
    px = to_rgba(w, h, f, type, p, g_unpack);
    scene_flush();
    if (px)
        B->tex_sub(x->handle, x0, y0, w, h, px);
    free(px);
}

/* glGenerateMipmap: the levels were made with the upload already */
static void FBAPI ff_glGenerateMipmap(GLenum t) { (void)t; }

/* ------------------------------------------------------ framebuffers */

static int cur_target(GLuint fb)
{
    return fb && (int)fb < g_nfb && g_fb[fb].used ? g_fb[fb].target : 0;
}

static void grow(void **a, int *n, int need, size_t sz)
{
    if (need < *n)
        return;
    {
        int m = *n ? *n : 16;
        while (m <= need)
            m *= 2;
        *a = realloc(*a, (size_t)m * sz);
        memset((char *)*a + (size_t)*n * sz, 0, (size_t)(m - *n) * sz);
        *n = m;
    }
}

static void FBAPI ff_glGenFramebuffers(GLsizei n, GLuint *o)
{
    int i;
    for (i = 0; i < n; i++) {
        grow((void **)&g_fb, &g_nfb, g_fb_next, sizeof *g_fb);
        memset(&g_fb[g_fb_next], 0, sizeof *g_fb);
        g_fb[g_fb_next].used = 1;
        o[i] = (GLuint)g_fb_next++;
    }
}

static void FBAPI ff_glGenRenderbuffers(GLsizei n, GLuint *o)
{
    int i;
    for (i = 0; i < n; i++) {
        grow((void **)&g_rb, &g_nrb, g_rb_next, sizeof *g_rb);
        memset(&g_rb[g_rb_next], 0, sizeof *g_rb);
        g_rb[g_rb_next].used = 1;
        o[i] = (GLuint)g_rb_next++;
    }
}

static void FBAPI ff_glDeleteFramebuffers(GLsizei n, const GLuint *o)
{
    int i;
    scene_flush();
    for (i = 0; i < n; i++)
        if (o[i] && (int)o[i] < g_nfb && g_fb[o[i]].used) {
            if (g_fb[o[i]].target && B)
                B->target_free(g_fb[o[i]].target);
            g_fb[o[i]].used = 0;
            if (g_draw_fb == o[i]) { g_draw_fb = 0; if (B) B->target_use(0); }
            if (g_read_fb == o[i]) g_read_fb = 0;
        }
}

static void FBAPI ff_glDeleteRenderbuffers(GLsizei n, const GLuint *o)
{
    int i;
    for (i = 0; i < n; i++)
        if (o[i] && (int)o[i] < g_nrb)
            g_rb[o[i]].used = 0;
}

/* the backend's target for a framebuffer, made once it has its colour */
static int make_target(GLuint fb)
{
    ff_fb *f;
    ff_rb *c, *d;

    if (!fb || (int)fb >= g_nfb || !g_fb[fb].used)
        return 0;
    f = &g_fb[fb];
    if (f->made)
        return f->target;
    c = f->color && (int)f->color < g_nrb ? &g_rb[f->color] : NULL;
    d = f->depth && (int)f->depth < g_nrb ? &g_rb[f->depth] : NULL;
    if (!c || c->w <= 0 || c->h <= 0 || !B)
        return 0;
    f->target = B->target_new(c->w, c->h, c->samples, d != NULL);
    f->made = 1;
    return f->target;
}

static void FBAPI ff_glBindFramebuffer(GLenum t, GLuint fb)
{
    if (t == E_FRAMEBUFFER || t == E_DRAW_FRAMEBUFFER) {
        if (fb != g_draw_fb)
            scene_flush();              /* held draws go where they were made */
        g_draw_fb = fb;
        if (B)
            B->target_use(fb ? make_target(fb) : 0);
    }
    if (t == E_FRAMEBUFFER || t == E_READ_FRAMEBUFFER)
        g_read_fb = fb;
}

static void FBAPI ff_glBindRenderbuffer(GLenum t, GLuint rb) { (void)t; g_bound_rb = rb; }

static void FBAPI ff_glRenderbufferStorageMultisample(GLenum t, GLsizei samples, GLenum fmt,
                                                      GLsizei w, GLsizei h)
{
    (void)t;
    if (!g_bound_rb || (int)g_bound_rb >= g_nrb)
        return;
    g_rb[g_bound_rb].fmt = fmt;
    g_rb[g_bound_rb].samples = samples;
    g_rb[g_bound_rb].w = w;
    g_rb[g_bound_rb].h = h;
}

static void FBAPI ff_glRenderbufferStorage(GLenum t, GLenum fmt, GLsizei w, GLsizei h)
{
    ff_glRenderbufferStorageMultisample(t, 0, fmt, w, h);
}

static void FBAPI ff_glFramebufferRenderbuffer(GLenum t, GLenum att, GLenum rt, GLuint rb)
{
    GLuint fb = t == E_READ_FRAMEBUFFER ? g_read_fb : g_draw_fb;
    (void)rt;
    if (!fb || (int)fb >= g_nfb)
        return;
    if (att == E_COLOR_ATTACHMENT0)
        g_fb[fb].color = rb;
    else if (att == E_DEPTH_ATTACHMENT || att == E_DEPTH_STENCIL_ATTACHMENT)
        g_fb[fb].depth = rb;
    if (g_fb[fb].made && g_fb[fb].target && B) {
        scene_flush();
        B->target_free(g_fb[fb].target);
    }
    g_fb[fb].made = 0;
    g_fb[fb].target = 0;
}

static GLenum FBAPI ff_glCheckFramebufferStatus(GLenum t)
{
    GLuint fb = t == E_READ_FRAMEBUFFER ? g_read_fb : g_draw_fb;
    int tg;

    if (!fb)
        return E_FRAMEBUFFER_COMPLETE;
    tg = make_target(fb);
    if (!tg)
        return E_FRAMEBUFFER_UNSUPPORTED;
    if (fb == g_draw_fb && B)
        B->target_use(tg);
    return E_FRAMEBUFFER_COMPLETE;
}

static void FBAPI ff_glBlitFramebuffer(GLint sx0, GLint sy0, GLint sx1, GLint sy1,
                                       GLint dx0, GLint dy0, GLint dx1, GLint dy1,
                                       GLbitfield mask, GLenum filter)
{
    rgl_rect a, b;

    if (!B || !(mask & E_COLOR_BUFFER_BIT))
        return;
    a.x = sx0; a.y = sy0; a.w = sx1 - sx0; a.h = sy1 - sy0;
    b.x = dx0; b.y = dy0; b.w = dx1 - dx0; b.h = dy1 - dy0;
    scene_flush();
    B->blit(cur_target(g_read_fb), &a, cur_target(g_draw_fb), &b, filter == E_LINEAR);
}

void *rgl_ff_proc(const char *name)
{
    static const struct { const char *n; void *p; } k[] = {
        { "glGenFramebuffers", (void *)ff_glGenFramebuffers },
        { "glGenRenderbuffers", (void *)ff_glGenRenderbuffers },
        { "glDeleteFramebuffers", (void *)ff_glDeleteFramebuffers },
        { "glDeleteRenderbuffers", (void *)ff_glDeleteRenderbuffers },
        { "glBindFramebuffer", (void *)ff_glBindFramebuffer },
        { "glBindRenderbuffer", (void *)ff_glBindRenderbuffer },
        { "glRenderbufferStorage", (void *)ff_glRenderbufferStorage },
        { "glRenderbufferStorageMultisample", (void *)ff_glRenderbufferStorageMultisample },
        { "glFramebufferRenderbuffer", (void *)ff_glFramebufferRenderbuffer },
        { "glCheckFramebufferStatus", (void *)ff_glCheckFramebufferStatus },
        { "glBlitFramebuffer", (void *)ff_glBlitFramebuffer },
        { "glGenerateMipmap", (void *)ff_glGenerateMipmap },
    };
    size_t i;

    for (i = 0; i < sizeof k / sizeof k[0]; i++)
        if (strcmp(k[i].n, name) == 0)
            return k[i].p;
    return NULL;
}

/* ---------------------------------------------------------------- reads */

static void target_size(int target, int *w, int *h)
{
    *w = *h = 0;
    if (B)
        B->target_size(target, w, h);
}

void ff_glReadPixels(GLint x, GLint y, GLsizei w, GLsizei h, GLenum f, GLenum type, GLvoid *p)
{
    unsigned char *px, *o = (unsigned char *)p;
    rgl_rect r;
    int row, bpp, i, j;

    if (!B || !p || w <= 0 || h <= 0 || type != E_UNSIGNED_BYTE)
        return;
    px = (unsigned char *)malloc((size_t)w * h * 4);
    if (!px)
        return;
    r.x = x; r.y = y; r.w = w; r.h = h;
    scene_flush();
    B->read(cur_target(g_read_fb), &r, px);
    bpp = f == E_RGB ? 3 : 4;
    row = w * bpp;
    if (g_pack > 1)
        row = (row + g_pack - 1) / g_pack * g_pack;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++) {
            const unsigned char *s = px + ((size_t)j * w + i) * 4;
            unsigned char *d = o + (size_t)j * row + i * bpp;
            if (f == E_BGRA) { d[0] = s[2]; d[1] = s[1]; d[2] = s[0]; d[3] = s[3]; }
            else { d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; if (bpp == 4) d[3] = s[3]; }
        }
    free(px);
}

static int cap_query(GLenum p, GLint *v)
{
    unsigned char *c = cap(p);
    carray *a = arr(p);

    if (c) { *v = *c; return 1; }
    if (a) { *v = a->on; return 1; }
    return 0;
}

static void mat_out(const mat4 *m, GLfloat *v) { memcpy(v, m->m, sizeof(mat4)); }

void ff_glGetFloatv(GLenum p, GLfloat *v)
{
    int i;
    switch (p) {
    case E_MODELVIEW_MATRIX: mat_out(&g_mv.s[g_mv.top], v); return;
    case E_PROJECTION_MATRIX: mat_out(&g_pr.s[g_pr.top], v); return;
    case E_TEXTURE_MATRIX: mat_out(&g_tx.s[g_tx.top], v); return;
    case E_CURRENT_COLOR: for (i = 0; i < 4; i++) v[i] = S.color[i]; return;
    case E_COLOR_CLEAR_VALUE: for (i = 0; i < 4; i++) v[i] = S.clear[i]; return;
    case E_MAX_TEXTURE_MAX_ANISOTROPY: v[0] = 16.0f; return;
    }
    {
        GLint t[4] = { 0, 0, 0, 0 };
        extern void ff_glGetIntegerv(GLenum, GLint *);
        ff_glGetIntegerv(p, t);
        v[0] = (GLfloat)t[0];
    }
}

void ff_glGetIntegerv(GLenum p, GLint *v)
{
    int i;

    if (cap_query(p, v))
        return;
    switch (p) {
    case E_VIEWPORT: for (i = 0; i < 4; i++) v[i] = S.viewport[i]; return;
    case E_SCISSOR_BOX: for (i = 0; i < 4; i++) v[i] = S.box[i]; return;
    case E_BLEND_SRC: v[0] = (GLint)S.src; return;
    case E_BLEND_DST: v[0] = (GLint)S.dst; return;
    case E_TEXTURE_BINDING_2D: v[0] = (GLint)S.bound; return;
    case E_MATRIX_MODE: v[0] = (GLint)S.matrix_mode; return;
    case E_CULL_FACE_MODE: v[0] = (GLint)S.cull_mode; return;
    case E_DEPTH_WRITEMASK: v[0] = S.depth_mask; return;
    case E_COLOR_WRITEMASK: for (i = 0; i < 4; i++) v[i] = S.mask[i]; return;
    case E_MAX_TEXTURE_SIZE: v[0] = B ? B->max_texture : 4096; return;
    case E_MAX_SAMPLES: v[0] = B ? B->max_samples : 1; return;
    case E_SAMPLES: v[0] = 0; return;
    case E_PACK_ALIGNMENT: v[0] = g_pack; return;
    case E_UNPACK_ALIGNMENT: v[0] = g_unpack; return;
    }
    v[0] = 0;
}

const GLubyte *ff_glGetString(GLenum n)
{
    static char renderer[256], version[64];

    switch (n) {
    case E_VENDOR: return (const GLubyte *)"UMK3 PC port";
    case E_RENDERER:
        snprintf(renderer, sizeof renderer, "%s (%s)", B && B->device ? B->device() : "?",
                 B ? B->name : "?");
        return (const GLubyte *)renderer;
    case E_VERSION:
        snprintf(version, sizeof version, "1.1 fixed function on %s", B ? B->name : "?");
        return (const GLubyte *)version;
    case E_EXTENSIONS: return (const GLubyte *)"GL_EXT_texture_filter_anisotropic";
    }
    return (const GLubyte *)"";
}

/* --------------------------------------------------------------- clears */

static int draw_rect(rgl_rect *r)
{
    if (!S.scissor)
        return 0;
    r->x = S.box[0]; r->y = S.box[1]; r->w = S.box[2]; r->h = S.box[3];
    return 1;
}

void ff_glClear(GLbitfield m)
{
    rgl_rect r;
    int sc;

    if (!B)
        return;
    sc = draw_rect(&r);
    if (hold_clear(sc ? &r : NULL, (m & E_COLOR_BUFFER_BIT) && (S.mask[0] | S.mask[1] | S.mask[2] | S.mask[3]),
                   S.clear, (m & E_DEPTH_BUFFER_BIT) && S.depth_mask))
        return;
    scene_flush();
    B->clear(sc ? &r : NULL, (m & E_COLOR_BUFFER_BIT) && (S.mask[0] | S.mask[1] | S.mask[2] | S.mask[3]),
             S.clear, (m & E_DEPTH_BUFFER_BIT) && S.depth_mask, 1.0f);
}

/* ---------------------------------------------------------------- draws */

static void need_out(int n)
{
    if (n <= g_cout)
        return;
    g_cout = n < 1024 ? 1024 : n * 2;
    g_out = (rgl_vtx *)realloc(g_out, (size_t)g_cout * sizeof *g_out);
}

static float comp(const unsigned char *p, GLenum t, int i)
{
    switch (t) {
    case E_FLOAT: return ((const float *)p)[i];
    case E_SHORT: return (float)((const short *)p)[i];
    case E_UNSIGNED_SHORT: return (float)((const unsigned short *)p)[i];
    case E_BYTE: return (float)((const signed char *)p)[i];
    case E_UNSIGNED_BYTE: return (float)p[i];
    case E_INT: return (float)((const int *)p)[i];
    case E_FIXED: return (float)((const int *)p)[i] / 65536.0f;
    case E_DOUBLE: return (float)((const double *)p)[i];
    }
    return 0.0f;
}

static int tsize(GLenum t)
{
    switch (t) {
    case E_BYTE: case E_UNSIGNED_BYTE: return 1;
    case E_SHORT: case E_UNSIGNED_SHORT: return 2;
    case E_DOUBLE: return 8;
    }
    return 4;
}

static const unsigned char *elem(const carray *a, int i)
{
    int st = a->stride ? a->stride : a->size * tsize(a->type);
    return a->p + (size_t)i * st;
}

static unsigned char ub(float f)
{
    if (f <= 0.0f) return 0;
    if (f >= 1.0f) return 255;
    return (unsigned char)(f * 255.0f + 0.5f);
}

static void update_mvp(void)
{
    if (!g_mvp_dirty)
        return;
    g_mvp = mul(&g_pr.s[g_pr.top], &g_mv.s[g_mv.top]);
    g_mvp_dirty = 0;
}

/* ------------------------------------------------------ REAL-TIME SHADOWS
 *
 * umk3.ini rt_shadows=1 (rgl.h). A shadow map, as CopperCube's: the scene
 * seen from a light, keeping the depth nearest it, and each point drawn
 * darkened when the map has something nearer the light than it is.
 *
 *   the light     a direction, from above and in front of the stage, a
 *                 little from the left (world Z is up, the camera looks
 *                 along +Y): an orthographic view, fitted around the
 *                 fighters with room about them (SHADOW_ROOM), its size
 *                 in steps and its centre on the map's texels, so the
 *                 shadows do not crawl as the fighters move
 *   the casters   the fighters (rgl_shadow_caster, RenderPlayer), each
 *                 with an id of its own, and the stage: every opaque
 *                 triangle of the scene that writes depth, the ones the
 *                 light saw the last time. Their triangles are kept in
 *                 world space as they are drawn, and drawn from the light
 *                 into the map, a SHADOW_SIZE target whose colour holds
 *                 the depth in RGB and the caster's id in A
 *   receivers     every 3D draw of the scene (the camera set by
 *                 LIMEDS_SetCameraOrientation, depth tested, a
 *                 perspective projection; not the HUD): each vertex also
 *                 carries where it is in the map's light, and the shader
 *                 compares (render/shaders: nine texels for a soft edge,
 *                 each against the receiver's own plane so a surface does
 *                 not shadow itself, and faded out at the map's edge)
 *
 * As a shadow map is drawn (LearnOpenGL's "Shadow Mapping"): the light's
 * depth first, then the scene with it, the same frame. The scene's draws
 * are held back (hold_draw) while its camera is on, with their vertices in
 * world space; when something else needs the backend (the HUD's draws, a
 * blit, a texture changing, the frame's end: scene_flush) the map is drawn
 * from the casters held, and then the draws, each vertex put in the map's
 * light. A fighter is not shadowed by its own texels (SMOOTH MODELS casts
 * the base mesh and draws the smoothed one; they differ); by the other
 * fighter's and the stage's it is. World space is the eye space of the
 * scene's camera taken back through the view matrix, captured when the
 * camera is set (rgl_shadow_camera). */
int rgl_shadows;
double rgl_shadow_ms;
int rgl_shadow_tris;

#define SHADOW_SIZE 4096                /* 64 MB: the fighters' texels as fine as at 2048 */
#define SHADOW_ROOM 3.2f                /* the map's half width, the fighters' radius times */
#define SHADOW_MIN 10.0f                /* and at least this: the stage they stand in */

static struct {
    int scene, caster, ids;             /* the scene's camera; the fighter drawing
                                         * (its id, 0 none), and the ids given */
    int valid, target;                  /* the map holds the held casters; its target */
    int lit;                            /* light is one drawn with, to cull the stage by */
    mat4 inv_view;                      /* world from eye */
    mat4 light;                         /* light clip from world, the map's */
    float *w;                           /* the casters' triangles, world xyz and id */
    int nw, cw;
} SH;
static int g_sh_on;                     /* this draw: 1 receives, 2 casts too */

/* a scene's draw or clear held back until the map is drawn */
typedef struct {
    rgl_pipe p;
    rgl_frag f;
    rgl_samp sm;
    rgl_rect vp, sc;
    int tex, has_sc, first, n;
    int clear, color, depth;            /* a clear: what it clears */
    float rgba[4];
} held;
static held *g_held;
static int g_nheld, g_cheld;
static rgl_vtx *g_hv;                   /* their vertices, s in world space */
static int g_nhv, g_chv;
static int g_sh_proxy;                  /* rgl_shadow_proxy: 1 casts only, 2 never casts */
static mat4 g_shm;                      /* this draw: rgl_vtx.s from the vertex */
static rgl_vtx *g_shv;                  /* the map's vertices */
static int g_cshv;

/* the inverse of a 4x4, 0 when it has none (Cramer's rule, as MESA's
 * gluInvertMatrix) */
static int invert(const mat4 *a, mat4 *out)
{
    const float *m = a->m;
    float v[16], d;
    int i;

    v[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15]
         + m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
    v[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15]
         - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
    v[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15]
         + m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
    v[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14]
          - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
    v[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15]
         - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
    v[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15]
         + m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
    v[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15]
         - m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
    v[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14]
          + m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
    v[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15]
         + m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
    v[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15]
         - m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
    v[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15]
          + m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
    v[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14]
          - m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
    v[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11]
         - m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
    v[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11]
         + m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
    v[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11]
          - m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
    v[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10]
          + m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];
    d = m[0] * v[0] + m[1] * v[4] + m[2] * v[8] + m[3] * v[12];
    if (d == 0.0f)
        return 0;
    for (i = 0; i < 16; i++)
        out->m[i] = v[i] / d;
    return 1;
}

void rgl_shadow_camera(int on)
{
    mat4 inv;

    if (on && B && rgl_shadows && B->shadow_map && invert(&g_mv.s[g_mv.top], &inv)) {
        /* another camera (the character select draws each fighter with
         * its own): what the last one held is a scene of its own, with a
         * map of its own -- its world is not this one's */
        if (SH.scene && g_nheld && memcmp(&inv, &SH.inv_view, sizeof inv) != 0) {
            scene_flush();
            SH.lit = 0;
        }
        SH.inv_view = inv;
        SH.scene = 1;
    } else
        SH.scene = 0;
}

void rgl_shadow_caster(int on)
{
    SH.caster = !on ? 0 : SH.ids < 254 ? ++SH.ids : 254;
}

/* a mesh that stands in for another: drawn into the map only (1), the one
 * it stands for drawn but not cast (2), back to normal (0) -- SMOOTH MODELS
 * casts the base mesh, ~2,500 triangles, not the ~60,000 it draws */
void rgl_shadow_proxy(int mode)
{
    g_sh_proxy = mode;
}

int rgl_shadow_active(void)
{
    return B && rgl_shadows && B->shadow_map && SH.scene;
}

/* what this draw does with shadows: casts (a fighter's triangles, or the
 * stage's opaque ones) and receives, receives, or neither; its vertices
 * carry their world position */
static void shadow_prep(int tris)
{
    mat4 w;

    g_sh_on = 0;
    if (!SH.scene || !S.depth_test || g_pr.s[g_pr.top].m[11] == 0.0f)
        return;
    w = mul(&SH.inv_view, &g_mv.s[g_mv.top]);
    g_shm = w;                          /* into the light when the draw is let go */
    /* the stage casts what is solid: not blended, and not cut out by its
     * alpha (a fence's or a chain's texels: LearnOpenGL's "Blending"), which
     * the map, drawn untextured, would cast as a whole quad */
    g_sh_on = tris && g_sh_proxy != 2
              && (SH.caster || (!S.blend && S.depth_mask && !(S.alpha_test && S.texture))) ? 2 : 1;
}

/* a caster's triangles kept. The stage's are kept only where the light
 * drawn with last saw them (its map's square with a margin, the fighters
 * move; between its near and far), all of them before there is one */
static void shadow_cast(const rgl_vtx *v, int n, int id)
{
    const float *m = SH.light.m;
    int i, k;

    if (SH.nw + n * 4 > SH.cw) {
        int c = (SH.nw + n * 4) * 2;
        float *p = (float *)realloc(SH.w, (size_t)c * sizeof *p);
        if (!p)
            return;
        SH.w = p;
        SH.cw = c;
    }
    for (i = 0; i + 2 < n; i += 3) {
        float w[3][3];
        unsigned out = 0x3f;

        for (k = 0; k < 3; k++) {
            const rgl_vtx *o = &v[i + k];
            const float x = o->s[0], y = o->s[1], z = o->s[2];
            float lx, ly, lz;
            unsigned side = 0;

            w[k][0] = x; w[k][1] = y; w[k][2] = z;
            if (id || !SH.lit)
                continue;
            lx = m[0] * x + m[4] * y + m[8] * z + m[12];
            ly = m[1] * x + m[5] * y + m[9] * z + m[13];
            lz = m[2] * x + m[6] * y + m[10] * z + m[14];
            side |= lx < -1.3f ? 1u : 0u;
            side |= lx > 1.3f ? 2u : 0u;
            side |= ly < -1.3f ? 4u : 0u;
            side |= ly > 1.3f ? 8u : 0u;
            side |= lz < -1.0f ? 16u : 0u;
            side |= lz > 1.0f ? 32u : 0u;
            out &= side;
        }
        if (!id && SH.lit && out)
            continue;
        for (k = 0; k < 3; k++) {
            SH.w[SH.nw++] = w[k][0];
            SH.w[SH.nw++] = w[k][1];
            SH.w[SH.nw++] = w[k][2];
            SH.w[SH.nw++] = (float)id;
        }
    }
}

static void shadow_drop(void)
{
    if (SH.target && B) {
        B->shadow_map(0);
        B->target_free(SH.target);
    }
    SH.target = 0;
    SH.valid = SH.lit = 0;
}

static void shadow_forget(void)
{
    SH.target = 0;
    SH.valid = SH.lit = SH.scene = SH.caster = SH.ids = 0;
    SH.nw = 0;
    g_nheld = g_nhv = 0;
}

static void SH_end_frame(void)
{
    SH.scene = SH.caster = SH.ids = 0;
    SH.nw = 0;
    if (!rgl_shadows && SH.target)
        shadow_drop();                  /* turned off: the map's memory back */
}

/* the held casters into the map, from a light fitted around the fighters,
 * for the held draws to sample: SH.valid when it was drawn */
static void shadow_build(void)
{
    static const float dir[3] = { 0.2316f, 0.4169f, -0.8789f };   /* (0.25, 0.45, -0.95) */
    float lo[3] = { 1e30f, 1e30f, 1e30f }, hi[3] = { -1e30f, -1e30f, -1e30f };
    float c[3], r = 0.0f, eye[3], side[3], up[3], l;
    mat4 view = k_ident, ortho = k_ident;
    rgl_vtx *v;
    rgl_pipe p;
    rgl_frag f;
    rgl_samp sm;
    rgl_rect vp;
    static const float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    int i, k, f_n = 0, n = SH.nw / 4;
    float texel, cs, cu;

    SH.caster = SH.ids = 0;
    SH.valid = 0;
    if (!B || !B->shadow_map)
        return;
    if (!rgl_shadows) {
        shadow_drop();
        SH.nw = 0;
        return;
    }
    SH.nw = 0;
    if (n < 3) {
        SH.valid = 0;
        return;
    }
    for (i = 0; i < n; i++) {
        if (SH.w[i * 4 + 3] == 0.0f)
            continue;                   /* the stage: the map follows the fighters */
        f_n++;
        for (k = 0; k < 3; k++) {
            if (SH.w[i * 4 + k] < lo[k]) lo[k] = SH.w[i * 4 + k];
            if (SH.w[i * 4 + k] > hi[k]) hi[k] = SH.w[i * 4 + k];
        }
    }
    for (k = 0; k < 3; k++)
        c[k] = (lo[k] + hi[k]) * 0.5f;
    for (k = 0; k < 3; k++)
        r += (hi[k] - c[k]) * (hi[k] - c[k]);
    r = sqrtf(r) * SHADOW_ROOM;
    if (r < SHADOW_MIN)
        r = SHADOW_MIN;                 /* the stage about them too */
    if (f_n < 3 || !(r > 0.0f) || r > 1e6f) {
        SH.valid = 0;
        return;
    }
    /* steady: the size in eighths of its power of two, so it changes only
     * now and then, and the centre on a texel (below), so the texels do not
     * slide over the stage as the fighters move */
    {
        const float q = exp2f(ceilf(log2f(r)) - 3.0f);
        r = ceilf(r / q) * q;
    }

    /* the light's view: from 3r back along its direction, looking at the
     * casters' centre (gluLookAt, with +Y as up -- never the direction) */
    side[0] = dir[1] * 0.0f - dir[2] * 1.0f;    /* dir x (0, 1, 0) */
    side[1] = dir[2] * 0.0f - dir[0] * 0.0f;
    side[2] = dir[0] * 1.0f - dir[1] * 0.0f;
    l = sqrtf(side[0] * side[0] + side[1] * side[1] + side[2] * side[2]);
    for (k = 0; k < 3; k++)
        side[k] /= l;
    up[0] = side[1] * dir[2] - side[2] * dir[1];
    up[1] = side[2] * dir[0] - side[0] * dir[2];
    up[2] = side[0] * dir[1] - side[1] * dir[0];
    texel = 2.0f * r / SHADOW_SIZE;
    cs = side[0] * c[0] + side[1] * c[1] + side[2] * c[2];
    cu = up[0] * c[0] + up[1] * c[1] + up[2] * c[2];
    cs = floorf(cs / texel + 0.5f) * texel - cs;
    cu = floorf(cu / texel + 0.5f) * texel - cu;
    for (k = 0; k < 3; k++) {
        c[k] += side[k] * cs + up[k] * cu;
        eye[k] = c[k] - dir[k] * 3.0f * r;
    }
    for (k = 0; k < 3; k++) {
        view.m[k * 4 + 0] = side[k];
        view.m[k * 4 + 1] = up[k];
        view.m[k * 4 + 2] = -dir[k];
    }
    view.m[12] = -(side[0] * eye[0] + side[1] * eye[1] + side[2] * eye[2]);
    view.m[13] = -(up[0] * eye[0] + up[1] * eye[1] + up[2] * eye[2]);
    view.m[14] = dir[0] * eye[0] + dir[1] * eye[1] + dir[2] * eye[2];
    /* glOrtho(-r, r, -r, r, 0, 6r): the room about the fighters, and the
     * stage from as far above them as below */
    ortho.m[0] = 1.0f / r;
    ortho.m[5] = 1.0f / r;
    ortho.m[10] = -2.0f / (6.0f * r);
    ortho.m[14] = -1.0f;
    SH.light = mul(&ortho, &view);

    if (!SH.target) {
        SH.target = B->target_new(SHADOW_SIZE, SHADOW_SIZE, 1, 1);
        if (!SH.target) {
            printf("real-time shadows: no %dx%d target, off\n", SHADOW_SIZE, SHADOW_SIZE);
            rgl_shadows = 0;
            return;
        }
        printf("real-time shadows: on, a %dx%d shadow map\n", SHADOW_SIZE, SHADOW_SIZE);
    }
    /* a buffer of its own: the draw that let the scene go is in g_out */
    if (n > g_cshv) {
        rgl_vtx *a = (rgl_vtx *)realloc(g_shv, (size_t)n * 2 * sizeof *a);
        if (!a)
            return;
        g_shv = a;
        g_cshv = n * 2;
    }
    v = g_shv;
    memset(v, 0, (size_t)n * sizeof *v);
    for (i = 0; i < n; i++) {
        const float x = SH.w[i * 4], y = SH.w[i * 4 + 1], z = SH.w[i * 4 + 2];
        const float *m = SH.light.m;
        v[i].x = m[0] * x + m[4] * y + m[8] * z + m[12];
        v[i].y = m[1] * x + m[5] * y + m[9] * z + m[13];
        v[i].z = m[2] * x + m[6] * y + m[10] * z + m[14];
        v[i].w = m[3] * x + m[7] * y + m[11] * z + m[15];
        v[i].rgba[0] = v[i].rgba[1] = v[i].rgba[2] = v[i].rgba[3] = 255;
        v[i].s[0] = SH.w[i * 4 + 3];   /* the caster's id, into the texel's A */
    }
    B->target_use(SH.target);
    B->clear(NULL, 1, white, 1, 1.0f);
    memset(&p, 0, sizeof p);
    p.topo = RGL_TRIS;
    p.depth_test = p.depth_write = 1;
    p.cull = RGL_CULL_NONE;
    p.mask = 15;
    memset(&f, 0, sizeof f);
    f.tex_mode = RGL_TEX_NONE;
    f.shadow = RGL_SHADOW_DEPTH;
    memset(&sm, 0, sizeof sm);
    sm.aniso = 1;
    vp.x = vp.y = 0;
    vp.w = vp.h = SHADOW_SIZE;
    B->draw(&p, &f, 0, &sm, &vp, NULL, v, n);
    B->shadow_map(SH.target);
    B->target_use(cur_target(g_draw_fb));
    SH.valid = SH.lit = 1;
}

/* the scene's draws held back while its camera is on (1), or not (0) */
static int hold_draw(const rgl_pipe *p, const rgl_frag *f, int tex, const rgl_samp *sm,
                     const rgl_rect *vp, const rgl_rect *sc, const rgl_vtx *v, int n)
{
    held *h;

    if (!SH.scene)
        return 0;
    if (g_nheld >= g_cheld) {
        int c = g_cheld ? g_cheld * 2 : 256;
        held *a = (held *)realloc(g_held, (size_t)c * sizeof *a);
        if (!a)
            return 0;
        g_held = a;
        g_cheld = c;
    }
    if (g_nhv + n > g_chv) {
        int c = (g_nhv + n) * 2;
        rgl_vtx *a = (rgl_vtx *)realloc(g_hv, (size_t)c * sizeof *a);
        if (!a)
            return 0;
        g_hv = a;
        g_chv = c;
    }
    h = &g_held[g_nheld++];
    memset(h, 0, sizeof *h);
    h->p = *p;
    h->f = *f;
    h->sm = *sm;
    h->vp = *vp;
    if (sc) {
        h->sc = *sc;
        h->has_sc = 1;
    }
    h->tex = tex;
    h->first = g_nhv;
    h->n = n;
    memcpy(g_hv + g_nhv, v, (size_t)n * sizeof *v);
    g_nhv += n;
    return 1;
}

static int hold_clear(const rgl_rect *sc, int color, const float rgba[4], int depth)
{
    held *h;

    if (!SH.scene || !g_nheld)
        return 0;                       /* nothing held to keep the order of */
    if (g_nheld >= g_cheld) {
        int c = g_cheld * 2;
        held *a = (held *)realloc(g_held, (size_t)c * sizeof *a);
        if (!a)
            return 0;
        g_held = a;
        g_cheld = c;
    }
    h = &g_held[g_nheld++];
    memset(h, 0, sizeof *h);
    h->clear = 1;
    h->color = color;
    h->depth = depth;
    memcpy(h->rgba, rgba, sizeof h->rgba);
    if (sc) {
        h->sc = *sc;
        h->has_sc = 1;
    }
    return 1;
}

/* the map from the casters held, then the held draws with it */
static void scene_flush(void)
{
    int i, k;
    double t0;

    if (!B || !g_nheld) {
        SH.nw = 0;
        SH.caster = SH.ids = 0;
        return;
    }
    t0 = rgl_now_ms();
    rgl_shadow_tris = SH.nw / 12;
    if (rgl_shadows)
        shadow_build();
    else {
        shadow_drop();
        SH.nw = 0;
    }
    SH.nw = 0;
    for (i = 0; i < g_nheld; i++) {
        held *h = &g_held[i];
        rgl_vtx *v = g_hv + h->first;

        if (h->clear) {
            B->clear(h->has_sc ? &h->sc : NULL, h->color, h->rgba, h->depth, 1.0f);
            continue;
        }
        if ((h->f.shadow & 15) == RGL_SHADOW_RECEIVE) {
            if (SH.valid) {
                const float *m = SH.light.m;
                for (k = 0; k < h->n; k++) {
                    const float x = v[k].s[0], y = v[k].s[1], z = v[k].s[2], w = v[k].s[3];
                    v[k].s[0] = m[0] * x + m[4] * y + m[8] * z + m[12] * w;
                    v[k].s[1] = m[1] * x + m[5] * y + m[9] * z + m[13] * w;
                    v[k].s[2] = m[2] * x + m[6] * y + m[10] * z + m[14] * w;
                    v[k].s[3] = m[3] * x + m[7] * y + m[11] * z + m[15] * w;
                }
            } else
                h->f.shadow = RGL_SHADOW_NONE;
        }
        B->draw(&h->p, &h->f, h->tex, &h->sm, &h->vp, h->has_sc ? &h->sc : NULL, v, h->n);
    }
    g_nheld = g_nhv = 0;
    rgl_shadow_ms += rgl_now_ms() - t0;
}

/* one vertex out: position transformed, colour and texture coordinate */
static void put(rgl_vtx *o, const float p[4], const float c[4], float s, float t)
{
    const float *m = g_mvp.m;
    const float *x = g_tx.s[g_tx.top].m;

    o->x = m[0] * p[0] + m[4] * p[1] + m[8] * p[2] + m[12] * p[3];
    o->y = m[1] * p[0] + m[5] * p[1] + m[9] * p[2] + m[13] * p[3];
    o->z = m[2] * p[0] + m[6] * p[1] + m[10] * p[2] + m[14] * p[3];
    o->w = m[3] * p[0] + m[7] * p[1] + m[11] * p[2] + m[15] * p[3];
    o->rgba[0] = ub(c[0]); o->rgba[1] = ub(c[1]); o->rgba[2] = ub(c[2]); o->rgba[3] = ub(c[3]);
    o->u = x[0] * s + x[4] * t + x[12];
    o->v = x[1] * s + x[5] * t + x[13];
    if (g_sh_on) {
        const float *h = g_shm.m;
        o->s[0] = h[0] * p[0] + h[4] * p[1] + h[8] * p[2] + h[12] * p[3];
        o->s[1] = h[1] * p[0] + h[5] * p[1] + h[9] * p[2] + h[13] * p[3];
        o->s[2] = h[2] * p[0] + h[6] * p[1] + h[10] * p[2] + h[14] * p[3];
        o->s[3] = h[3] * p[0] + h[7] * p[1] + h[11] * p[2] + h[15] * p[3];
    } else
        o->s[0] = o->s[1] = o->s[2] = o->s[3] = 0.0f;
}

static void from_arrays(rgl_vtx *o, int i)
{
    float p[4] = { 0, 0, 0, 1 }, c[4], s = S.tc[0], t = S.tc[1];
    int k;

    if (g_va.on && g_va.p) {
        const unsigned char *e = elem(&g_va, i);
        for (k = 0; k < g_va.size && k < 4; k++)
            p[k] = comp(e, g_va.type, k);
    }
    if (g_ca.on && g_ca.p) {
        const unsigned char *e = elem(&g_ca, i);
        float sc = g_ca.type == E_UNSIGNED_BYTE ? 1.0f / 255.0f : 1.0f;
        c[3] = 1.0f;
        for (k = 0; k < g_ca.size && k < 4; k++)
            c[k] = comp(e, g_ca.type, k) * sc;
    } else
        memcpy(c, S.color, sizeof c);
    if (g_ta.on && g_ta.p) {
        const unsigned char *e = elem(&g_ta, i);
        s = comp(e, g_ta.type, 0);
        t = g_ta.size > 1 ? comp(e, g_ta.type, 1) : 0.0f;
    }
    put(o, p, c, s, t);
}

static unsigned char bf(GLenum f)
{
    switch (f) {
    case E_ZERO: return RGL_BF_ZERO;
    case E_ONE: return RGL_BF_ONE;
    case E_SRC_ALPHA: return RGL_BF_SRC_ALPHA;
    case E_ONE_MINUS_SRC_ALPHA: return RGL_BF_ONE_MINUS_SRC_ALPHA;
    case E_DST_ALPHA: return RGL_BF_DST_ALPHA;
    case E_ONE_MINUS_DST_ALPHA: return RGL_BF_ONE_MINUS_DST_ALPHA;
    case E_SRC_COLOR: return RGL_BF_SRC_COLOR;
    case E_ONE_MINUS_SRC_COLOR: return RGL_BF_ONE_MINUS_SRC_COLOR;
    case E_DST_COLOR: return RGL_BF_DST_COLOR;
    case E_ONE_MINUS_DST_COLOR: return RGL_BF_ONE_MINUS_DST_COLOR;
    }
    return RGL_BF_ONE;
}

/* the assembled primitives to the backend */
static void submit(int topo, rgl_vtx *v, int n)
{
    rgl_pipe p;
    rgl_frag f;
    rgl_samp sm;
    rgl_rect vp, sc;
    ff_tex *t = S.texture ? tex(S.bound, 0) : NULL;
    int handle = t && t->handle ? t->handle : 0;

    if (!B || n <= 0)
        return;
    if (S.cull && S.cull_mode == E_FRONT_AND_BACK && topo == RGL_TRIS)
        return;
    memset(&p, 0, sizeof p);
    p.topo = (unsigned char)topo;
    p.blend = S.blend;
    p.src = bf(S.src);
    p.dst = bf(S.dst);
    p.depth_test = S.depth_test;
    p.depth_write = S.depth_test && S.depth_mask;
    p.cull = !S.cull || topo != RGL_TRIS ? RGL_CULL_NONE
           : S.cull_mode == E_FRONT ? RGL_CULL_FRONT : RGL_CULL_BACK;
    p.mask = (unsigned char)(S.mask[0] | S.mask[1] << 1 | S.mask[2] << 2 | S.mask[3] << 3);

    memset(&f, 0, sizeof f);
    f.tex_mode = !handle ? RGL_TEX_NONE : S.env == E_REPLACE ? RGL_TEX_REPLACE : RGL_TEX_MODULATE;
    f.alpha_func = S.alpha_test ? (int)S.alpha_func : 0;
    f.alpha_ref = S.alpha_ref;
    if (g_sh_on == 2 && topo == RGL_TRIS)
        shadow_cast(v, n, SH.caster);
    if (g_sh_proxy == 1)
        return;                         /* a stand-in, cast and not drawn */
    /* a receiver's own caster id above the mode: its texels are skipped */
    f.shadow = g_sh_on ? RGL_SHADOW_RECEIVE | SH.caster << 4 : RGL_SHADOW_NONE;

    memset(&sm, 0, sizeof sm);
    if (t) {
        sm.mag_linear = t->mag != E_NEAREST;
        sm.min_linear = t->min == E_LINEAR || t->min == E_LINEAR_MIPMAP_NEAREST
                     || t->min == E_LINEAR_MIPMAP_LINEAR;
        sm.mip = t->levels <= 1 || t->min == E_NEAREST || t->min == E_LINEAR ? 0
               : t->min == E_NEAREST_MIPMAP_NEAREST || t->min == E_LINEAR_MIPMAP_NEAREST ? 1 : 2;
        sm.wrap_s_clamp = t->ws != E_REPEAT;
        sm.wrap_t_clamp = t->wt != E_REPEAT;
        sm.aniso = (unsigned char)(t->aniso < 1.0f ? 1 : t->aniso > 16.0f ? 16 : t->aniso);
    }
    vp.x = S.viewport[0]; vp.y = S.viewport[1]; vp.w = S.viewport[2]; vp.h = S.viewport[3];
    if (hold_draw(&p, &f, handle, &sm, &vp, draw_rect(&sc) ? &sc : NULL, v, n))
        return;
    scene_flush();
    B->draw(&p, &f, handle, &sm, &vp, draw_rect(&sc) ? &sc : NULL, v, n);
}

/* `get(i)` for vertex i of the call, primitives made of them */
static void assemble(GLenum mode, int count, void (*get)(rgl_vtx *, int, const void *), const void *ctx)
{
    rgl_vtx *src;
    int i, n = 0, topo = RGL_TRIS;

    if (count <= 0)
        return;
    update_mvp();
    shadow_prep(mode == E_TRIANGLES || mode == E_TRIANGLE_STRIP || mode == E_TRIANGLE_FAN);
    need_out(count * 3 + 16);
    src = (rgl_vtx *)malloc((size_t)count * sizeof *src);
    if (!src)
        return;
    for (i = 0; i < count; i++)
        get(&src[i], i, ctx);

    switch (mode) {
    case E_TRIANGLES:
        for (i = 0; i + 2 < count; i += 3) {
            g_out[n++] = src[i]; g_out[n++] = src[i + 1]; g_out[n++] = src[i + 2];
        }
        break;
    case E_TRIANGLE_STRIP:              /* odd ones swapped: GL's own winding */
        for (i = 0; i + 2 < count; i++) {
            if (i & 1) { g_out[n++] = src[i + 1]; g_out[n++] = src[i]; g_out[n++] = src[i + 2]; }
            else { g_out[n++] = src[i]; g_out[n++] = src[i + 1]; g_out[n++] = src[i + 2]; }
        }
        break;
    case E_TRIANGLE_FAN: case E_POLYGON:
        for (i = 1; i + 1 < count; i++) {
            g_out[n++] = src[0]; g_out[n++] = src[i]; g_out[n++] = src[i + 1];
        }
        break;
    case E_QUADS:
        for (i = 0; i + 3 < count; i += 4) {
            g_out[n++] = src[i]; g_out[n++] = src[i + 1]; g_out[n++] = src[i + 2];
            g_out[n++] = src[i]; g_out[n++] = src[i + 2]; g_out[n++] = src[i + 3];
        }
        break;
    case E_QUAD_STRIP:
        for (i = 0; i + 3 < count; i += 2) {
            g_out[n++] = src[i]; g_out[n++] = src[i + 1]; g_out[n++] = src[i + 3];
            g_out[n++] = src[i]; g_out[n++] = src[i + 3]; g_out[n++] = src[i + 2];
        }
        break;
    case E_LINES:
        topo = RGL_LINES;
        for (i = 0; i + 1 < count; i += 2) { g_out[n++] = src[i]; g_out[n++] = src[i + 1]; }
        break;
    case E_LINE_STRIP: case E_LINE_LOOP:
        topo = RGL_LINES;
        for (i = 0; i + 1 < count; i++) { g_out[n++] = src[i]; g_out[n++] = src[i + 1]; }
        if (mode == E_LINE_LOOP && count > 2) { g_out[n++] = src[count - 1]; g_out[n++] = src[0]; }
        break;
    default:                            /* points: nothing in the tree */
        break;
    }
    free(src);
    submit(topo, g_out, n);
}

static void get_array(rgl_vtx *o, int i, const void *ctx)
{
    from_arrays(o, *(const int *)ctx + i);
}

void ff_glDrawArrays(GLenum m, GLint first, GLsizei n)
{
    assemble(m, n, get_array, &first);
}

typedef struct { GLenum type; const void *idx; } elems;

static void get_elem(rgl_vtx *o, int i, const void *ctx)
{
    const elems *e = (const elems *)ctx;
    int k = e->type == E_UNSIGNED_BYTE ? ((const unsigned char *)e->idx)[i]
          : e->type == E_UNSIGNED_INT ? (int)((const unsigned int *)e->idx)[i]
          : ((const unsigned short *)e->idx)[i];
    from_arrays(o, k);
}

static int elem_index(GLenum t, const void *idx, int i)
{
    return t == E_UNSIGNED_BYTE ? ((const unsigned char *)idx)[i]
         : t == E_UNSIGNED_INT ? (int)((const unsigned int *)idx)[i]
         : ((const unsigned short *)idx)[i];
}

/* Indexed triangles: each vertex the indices reach is transformed once and
 * then gathered by index, instead of once per index -- a subdivided fighter
 * (SMOOTH MODELS) is ~180,000 indices over ~32,000 vertices. The same
 * vertices come out, so the picture is the same.
 *
 * Only the vertices these indices name are transformed, not 0..max: a
 * REPLACE MODEL skin is drawn a material at a time (skin_mats.c), each part
 * a run of indices into the whole model's vertices, and transforming all of
 * them for every part cost the model's vertex count times its parts, each
 * frame (Mary: custom models dropped frames). A stamp a vertex says which
 * are done this draw. */
static int tris_indexed(int count, GLenum t, const void *idx)
{
    static rgl_vtx *cache;
    static unsigned *stamp;
    static int *used;
    static int ccap, ucap;
    static unsigned gen;
    int i, max = 0, n = 0, nu = 0;

    for (i = 0; i < count; i++) {
        int k = elem_index(t, idx, i);
        if (k > max)
            max = k;
    }
    if (max + 1 > ccap) {
        int cap = (max + 1) * 2;
        rgl_vtx *c = (rgl_vtx *)realloc(cache, (size_t)cap * sizeof *cache);
        unsigned *st;
        if (!c)
            return 0;
        cache = c;
        st = (unsigned *)realloc(stamp, (size_t)cap * sizeof *stamp);
        if (!st)
            return 0;
        memset(st + ccap, 0, (size_t)(cap - ccap) * sizeof *st);
        stamp = st;
        ccap = cap;
    }
    if (count > ucap) {
        int *u = (int *)realloc(used, (size_t)count * 2 * sizeof *used);
        if (!u)
            return 0;
        used = u;
        ucap = count * 2;
    }
    if (++gen == 0) {                   /* wrapped: every stamp is stale */
        memset(stamp, 0, (size_t)ccap * sizeof *stamp);
        gen = 1;
    }
    for (i = 0; i < count; i++) {
        int k = elem_index(t, idx, i);
        if (stamp[k] != gen) {
            stamp[k] = gen;
            used[nu++] = k;
        }
    }
    update_mvp();
    shadow_prep(1);
    /* every core on a big mesh (thirdparty/blender/omp_lite.c): each vertex
     * and each index is its own work */
#pragma omp parallel for if (nu > 4096)
    for (i = 0; i < nu; i++)
        from_arrays(&cache[used[i]], used[i]);
    need_out(count + 16);
    n = count / 3 * 3;
#pragma omp parallel for if (n > 12288)
    for (i = 0; i < n; i++)
        g_out[i] = cache[elem_index(t, idx, i)];
    submit(RGL_TRIS, g_out, n);
    return 1;
}

void ff_glDrawElements(GLenum m, GLsizei n, GLenum t, const GLvoid *i)
{
    elems e;
    e.type = t;
    e.idx = i;
    if (!i)
        return;
    if (m == E_TRIANGLES && n >= 3 && tris_indexed(n, t, i))
        return;
    assemble(m, n, get_elem, &e);
}

/* glBegin / glVertex2f / glEnd */
void ff_glBegin(GLenum m)
{
    g_begin = m;
    g_nimm = 0;
}

void ff_glVertex2f(GLfloat x, GLfloat y)
{
    ff_imm *v;

    if (g_nimm >= g_cimm) {
        g_cimm = g_cimm ? g_cimm * 2 : 256;
        g_imm = (ff_imm *)realloc(g_imm, (size_t)g_cimm * sizeof *g_imm);
    }
    v = &g_imm[g_nimm++];
    v->x = x; v->y = y; v->z = 0.0f; v->w = 1.0f;
    memcpy(v->c, S.color, sizeof v->c);
    v->s = S.tc[0];
    v->t = S.tc[1];
}

static void get_imm(rgl_vtx *o, int i, const void *ctx)
{
    const ff_imm *v = &g_imm[i];
    (void)ctx;
    put(o, &v->x, v->c, v->s, v->t);
}

void ff_glEnd(void)
{
    GLenum m = g_begin;
    g_begin = (GLenum)-1;
    if (m != (GLenum)-1)
        assemble(m, g_nimm, get_imm, NULL);
    g_nimm = 0;
}
