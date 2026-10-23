/*
 * rgl_test.c -- the three renderers against the same fixed-function calls.
 *
 * Not the game: a hidden 256x256 window, the GL 1.1 calls the game makes
 * (glClear, glOrtho, quads with and without a texture, blending, the alpha
 * test, depth, culling, glReadPixels, and the framebuffer objects of the 3D
 * resolution with multisampling, resolved and scaled onto the window) and
 * the pixels read back, checked against what OpenGL 1.1 defines. Run with
 * the renderer as argument: opengl, vulkan or d3d12.
 *
 *     zig cc -o rgl_test.exe tests/rgl_test.c src/runtime/render/rgl_*.c
 *            -Isrc/runtime -Isrc/runtime/platform/vulkan-include
 *            -lopengl32 -lgdi32 -luser32
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "platform/gl.h"
#include "render/rgl.h"

#define W 256
#define H 256

static HWND g_wnd;

void plat_size(int *w, int *h) { *w = W; *h = H; }

static int g_fail;

static void expect(const char *what, int x, int y, int r, int g, int b)
{
    unsigned char p[4];
    glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, p);
    if (abs(p[0] - r) > 3 || abs(p[1] - g) > 3 || abs(p[2] - b) > 3) {
        printf("  FAIL %-34s at %3d,%3d: %3d %3d %3d, want %3d %3d %3d\n", what, x, y, p[0], p[1], p[2], r, g, b);
        g_fail++;
    } else
        printf("  ok   %-34s at %3d,%3d: %3d %3d %3d\n", what, x, y, p[0], p[1], p[2]);
}

static void quad(float x0, float y0, float x1, float y1, float z)
{
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(x0, y0);
    glTexCoord2f(1, 0); glVertex2f(x1, y0);
    glTexCoord2f(1, 1); glVertex2f(x1, y1);
    glTexCoord2f(0, 1); glVertex2f(x0, y1);
    glEnd();
    (void)z;
}

/* a quad at depth z, through arrays (the game's meshes come that way) */
static void quad_z(float x0, float y0, float x1, float y1, float z, int ccw)
{
    float v[12] = { x0, y0, z, x1, y0, z, x1, y1, z, x0, y1, z };
    unsigned short i_ccw[6] = { 0, 1, 2, 0, 2, 3 }, i_cw[6] = { 0, 2, 1, 0, 3, 2 };
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, v);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, ccw ? i_ccw : i_cw);
    glDisableClientState(GL_VERTEX_ARRAY);
}

typedef void (__stdcall *gen_t)(GLsizei, GLuint *);
typedef void (__stdcall *bind_t)(GLenum, GLuint);
typedef void (__stdcall *store_t)(GLenum, GLenum, GLsizei, GLsizei);
typedef void (__stdcall *storems_t)(GLenum, GLsizei, GLenum, GLsizei, GLsizei);
typedef void (__stdcall *att_t)(GLenum, GLenum, GLenum, GLuint);
typedef GLenum (__stdcall *stat_t)(GLenum);
typedef void (__stdcall *blit_t)(GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum);

static void *proc(const char *n)
{
    if (rgl_renderer != RGL_OPENGL)
        return rgl_proc(n);
    return (void *)wglGetProcAddress(n);
}

static void frame(void)
{
    if (rgl_renderer != RGL_OPENGL)
        rgl_present();
}

int main(int argc, char **argv)
{
    WNDCLASSA wc;
    HDC dc;
    GLuint tex;
    static const unsigned char texel[16] = { 255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255 };

    rgl_renderer = rgl_parse(argc > 1 ? argv[1] : "opengl");
    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = "rgltest";
    wc.style = CS_OWNDC;
    RegisterClassA(&wc);
    {
        RECT r = { 0, 0, W, H };
        AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
        g_wnd = CreateWindowA("rgltest", "rgl test", WS_OVERLAPPEDWINDOW, 0, 0, r.right - r.left,
                              r.bottom - r.top, NULL, NULL, wc.hInstance, NULL);
    }
    dc = GetDC(g_wnd);
    if (rgl_renderer == RGL_OPENGL || !rgl_open(g_wnd, W, H)) {
        PIXELFORMATDESCRIPTOR pfd;
        memset(&pfd, 0, sizeof pfd);
        pfd.nSize = sizeof pfd;
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.cColorBits = 32;
        pfd.cDepthBits = 24;
        SetPixelFormat(dc, ChoosePixelFormat(dc, &pfd), &pfd);
        wglMakeCurrent(dc, wglCreateContext(dc));
        rgl_renderer = RGL_OPENGL;
    }
    printf("%s: %s\n", rgl_label(rgl_renderer), (const char *)glGetString(GL_RENDERER));

    /* 1. clear */
    glViewport(0, 0, W, H);
    glClearColor(0.2f, 0.4f, 0.6f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    expect("clear", 128, 128, 51, 102, 153);

    /* 2. an untextured quad, left half, in a 0..W x 0..H space (y up) */
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, W, 0, H, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_TEXTURE_2D);
    glColor4f(1, 0, 0, 1);
    quad(0, 0, 128, 256, 0);
    expect("red quad, left", 32, 128, 255, 0, 0);
    expect("right still clear", 200, 128, 51, 102, 153);

    /* 3. a 2x2 texture over the right half: t=0 is the bottom */
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, texel);
    glEnable(GL_TEXTURE_2D);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, (float)GL_MODULATE);
    glColor4f(1, 1, 1, 1);
    quad(128, 0, 256, 256, 0);
    expect("texel 0 (red), bottom left", 150, 30, 255, 0, 0);
    expect("texel 1 (green), bottom right", 230, 30, 0, 255, 0);
    expect("texel 2 (blue), top left", 150, 230, 0, 0, 255);
    expect("texel 3 (white), top right", 230, 230, 255, 255, 255);
    glColor4f(1, 1, 0, 1);              /* MODULATE: white x yellow */
    quad(192, 128, 256, 256, 0);
    expect("modulate", 230, 230, 255, 255, 0);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, (float)GL_REPLACE);
    quad(192, 128, 256, 256, 0);
    expect("replace", 230, 230, 255, 255, 255);
    glDisable(GL_TEXTURE_2D);

    /* 4. blending */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0, 1, 0, 0.5f);
    quad(0, 0, 64, 64, 0);
    expect("half green over red", 32, 32, 128, 128, 0);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glColor4f(0, 0, 1, 1);
    quad(0, 64, 64, 128, 0);
    expect("additive blue on red", 32, 96, 255, 0, 255);
    glDisable(GL_BLEND);

    /* 5. alpha test: GREATER 0.5 drops alpha 0.4 */
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.5f);
    glColor4f(1, 1, 1, 0.4f);
    quad(64, 0, 128, 64, 0);
    expect("alpha 0.4 dropped", 96, 32, 255, 0, 0);
    glColor4f(1, 1, 1, 0.6f);
    quad(64, 0, 128, 64, 0);
    expect("alpha 0.6 kept", 96, 32, 255, 255, 255);
    glDisable(GL_ALPHA_TEST);

    /* 6. depth: near (z 0.5 -> -0.5 in the ortho's flip) then far */
    glEnable(GL_DEPTH_TEST);
    glColor4f(0, 1, 1, 1);
    quad_z(0, 128, 64, 192, 0.5f, 1);
    glColor4f(1, 0, 1, 1);
    quad_z(0, 128, 64, 192, -0.5f, 1);
    expect("depth: the nearer one", 32, 160, 0, 255, 255);
    glDisable(GL_DEPTH_TEST);

    /* 7. culling: back faces (clockwise) are dropped */
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glColor4f(1, 1, 0, 1);
    quad_z(64, 128, 128, 192, 0, 0);
    expect("clockwise culled", 96, 160, 255, 0, 0);
    quad_z(64, 128, 128, 192, 0, 1);
    expect("counter-clockwise drawn", 96, 160, 255, 255, 0);
    glCullFace(GL_FRONT);
    glColor4f(0, 0, 0, 1);
    quad_z(64, 128, 128, 192, 0, 1);
    expect("front culled", 96, 160, 255, 255, 0);
    glDisable(GL_CULL_FACE);

    /* 8. scissored clear */
    glEnable(GL_SCISSOR_TEST);
    glScissor(0, 192, 32, 64);
    glClearColor(1, 1, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);
    expect("scissor: inside", 16, 224, 255, 255, 255);
    expect("scissor: outside", 48, 224, 255, 0, 0);

    /* 9. the 3D resolution path: a 64x64 buffer, multisampled, resolved and
     * scaled onto the window's top-right quarter */
    {
        gen_t genfb = (gen_t)proc("glGenFramebuffers"), genrb = (gen_t)proc("glGenRenderbuffers");
        bind_t bindfb = (bind_t)proc("glBindFramebuffer"), bindrb = (bind_t)proc("glBindRenderbuffer");
        store_t store = (store_t)proc("glRenderbufferStorage");
        storems_t storems = (storems_t)proc("glRenderbufferStorageMultisample");
        att_t att = (att_t)proc("glFramebufferRenderbuffer");
        stat_t st = (stat_t)proc("glCheckFramebufferStatus");
        blit_t blit = (blit_t)proc("glBlitFramebuffer");
        GLuint fb[2], rb[3];

        if (!genfb || !blit) {
            printf("  FAIL no framebuffer objects\n");
            g_fail++;
        } else {
            genfb(2, fb);
            genrb(3, rb);
            bindfb(0x8D40, fb[0]);
            bindrb(0x8D41, rb[0]);
            store(0x8D41, 0x8058, 64, 64);
            att(0x8D40, 0x8CE0, 0x8D41, rb[0]);
            if (st(0x8D40) != 0x8CD5) { printf("  FAIL resolved buffer incomplete\n"); g_fail++; }
            bindfb(0x8D40, fb[1]);
            bindrb(0x8D41, rb[1]);
            storems(0x8D41, 4, 0x8058, 64, 64);
            att(0x8D40, 0x8CE0, 0x8D41, rb[1]);
            bindrb(0x8D41, rb[2]);
            storems(0x8D41, 4, 0x81A6, 64, 64);
            att(0x8D40, 0x8D00, 0x8D41, rb[2]);
            if (st(0x8D40) != 0x8CD5) { printf("  FAIL multisampled buffer incomplete\n"); g_fail++; }
            glViewport(0, 0, 64, 64);
            glClearColor(0, 0, 1, 1);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            glOrtho(0, 64, 0, 64, -1, 1);
            glColor4f(0, 1, 0, 1);
            quad(0, 0, 64, 32, 0);              /* green, bottom half */
            bindfb(0x8CA8, fb[1]);
            bindfb(0x8CA9, fb[0]);
            blit(0, 0, 64, 64, 0, 0, 64, 64, GL_COLOR_BUFFER_BIT, GL_NEAREST);
            bindfb(0x8D40, fb[0]);
            expect("resolved: bottom green", 32, 8, 0, 255, 0);
            expect("resolved: top blue", 32, 56, 0, 0, 255);
            bindfb(0x8CA8, fb[0]);
            bindfb(0x8CA9, 0);
            blit(0, 0, 64, 64, 128, 128, 256, 256, GL_COLOR_BUFFER_BIT, GL_LINEAR);
            bindfb(0x8D40, 0);
            glViewport(0, 0, W, H);
            expect("scaled: bottom green", 192, 140, 0, 255, 0);
            expect("scaled: top blue", 192, 240, 0, 0, 255);
            expect("outside the blit", 32, 32, 128, 128, 0);
        }
    }

    /* 10. a few frames, then again */
    {
        int i;
        for (i = 0; i < 3; i++) {
            glClearColor(i / 4.0f, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT);
            frame();
        }
        glClearColor(0, 0.5f, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        expect("after three presents", 10, 10, 0, 128, 0);
        frame();
    }

    printf("%s: %d failed\n", rgl_label(rgl_renderer), g_fail);
    if (rgl_renderer != RGL_OPENGL)
        rgl_close();
    return g_fail != 0;
}
