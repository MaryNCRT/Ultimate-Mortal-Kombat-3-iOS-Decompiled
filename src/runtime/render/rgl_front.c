/*
 * rgl_front.c -- every GL call the game makes, as a plain C function. Port,
 * not in the binary.
 *
 * The decompiled code calls `glDisable(cap)` and the rest as plain C, the way
 * the ARM binary called them, and runtime/platform/gl.h declares them so on
 * every target (no dllimport, no __stdcall). Each one is defined here: with
 * OpenGL drawing it forwards to the driver's entry point, fetched by name the
 * first time (opengl32.dll on Windows, libGL.so.1 on Linux -- the game does
 * not link against either); with Vulkan or Direct3D 12 it goes to the
 * fixed-function layer, rgl_ff.c, as ff_<name>.
 *
 * This file does not include platform/gl.h: it spells out its own types, and
 * the driver's calling convention (__stdcall on 32-bit Windows) only appears
 * on the pointers it calls through. These are all the GL 1.1 functions the
 * tree calls; glActiveTexture, glClientActiveTexture are the runtime's own
 * (draw_gl.c), and the framebuffer objects come through plat_gl_proc.
 *
 * Replaces runtime/platform/win32_gl_cdecl.c, which did the forwarding for
 * 32-bit Windows alone.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rgl.h"

#ifdef _WIN32
#  include <windows.h>
#  define GLREAL WINAPI
#else
#  include <dlfcn.h>
#  define GLREAL
#endif

typedef unsigned int  GLenum, GLbitfield, GLuint;
typedef int           GLint, GLsizei;
typedef unsigned char GLboolean, GLubyte;
typedef float         GLfloat, GLclampf;
typedef double        GLdouble;
typedef void          GLvoid;

int rgl_renderer = RGL_OPENGL;

int rgl_parse(const char *name)
{
    if (!name)
        return RGL_OPENGL;
    if (strcmp(name, "vulkan") == 0 || strcmp(name, "VULKAN") == 0)
        return RGL_VULKAN;
#ifdef _WIN32
    if (strcmp(name, "d3d12") == 0 || strcmp(name, "D3D12") == 0
        || strcmp(name, "dx12") == 0 || strcmp(name, "directx12") == 0)
        return RGL_D3D12;
#endif
    return RGL_OPENGL;
}

const char *rgl_label(int r)
{
    return r == RGL_VULKAN ? "Vulkan" : r == RGL_D3D12 ? "Direct3D 12" : "OpenGL";
}

/* the driver's entry point by name; a missing one is fatal, as it was */
static void *gl_proc(const char *name)
{
    void *p = NULL;
#ifdef _WIN32
    static HMODULE dll;

    if (dll == NULL)
        dll = LoadLibraryA("opengl32.dll");
    p = dll ? (void *)GetProcAddress(dll, name) : NULL;
    if (p == NULL) {
        MessageBoxA(NULL, name, "opengl32.dll has no such entry point", MB_OK);
        ExitProcess(1);
    }
#else
    static void *lib;

    if (lib == NULL)
        lib = dlopen("libGL.so.1", RTLD_NOW | RTLD_GLOBAL);
    if (lib == NULL)
        lib = dlopen("libGL.so", RTLD_NOW | RTLD_GLOBAL);
    if (lib == NULL)
        lib = dlopen("libOpenGL.so.0", RTLD_NOW | RTLD_GLOBAL);
    p = lib ? dlsym(lib, name) : NULL;
    if (p == NULL) {
        fprintf(stderr, "OpenGL: no %s (libGL.so.1: %s)\n", name, dlerror());
        exit(1);
    }
#endif
    return p;
}

/* GLV(name, (typed params), (args)) for void, GLR(ret, ...) for the rest */
#define GLV(name, params, args)                                             \
    void ff_##name params;                                                  \
    void name params                                                        \
    {                                                                       \
        typedef void (GLREAL *fn_t) params;                                 \
        static fn_t fn;                                                     \
        if (rgl_renderer != RGL_OPENGL) {                                   \
            ff_##name args;                                                 \
            return;                                                         \
        }                                                                   \
        if (fn == NULL)                                                     \
            fn = (fn_t)gl_proc(#name);                                      \
        fn args;                                                            \
    }
#define GLR(ret, name, params, args)                                        \
    ret ff_##name params;                                                   \
    ret name params                                                         \
    {                                                                       \
        typedef ret (GLREAL *fn_t) params;                                  \
        static fn_t fn;                                                     \
        if (rgl_renderer != RGL_OPENGL)                                     \
            return ff_##name args;                                          \
        if (fn == NULL)                                                     \
            fn = (fn_t)gl_proc(#name);                                      \
        return fn args;                                                     \
    }

GLV(glAlphaFunc, (GLenum f, GLclampf r), (f, r))
GLV(glBegin, (GLenum m), (m))
GLV(glBindTexture, (GLenum t, GLuint x), (t, x))
GLV(glBlendFunc, (GLenum s, GLenum d), (s, d))
GLV(glClear, (GLbitfield m), (m))
GLV(glClearColor, (GLclampf r, GLclampf g, GLclampf b, GLclampf a), (r, g, b, a))
GLV(glColor3f, (GLfloat r, GLfloat g, GLfloat b), (r, g, b))
GLV(glColor4f, (GLfloat r, GLfloat g, GLfloat b, GLfloat a), (r, g, b, a))
GLV(glColorMask, (GLboolean r, GLboolean g, GLboolean b, GLboolean a), (r, g, b, a))
GLV(glColorPointer, (GLint n, GLenum t, GLsizei s, const GLvoid *p), (n, t, s, p))
GLV(glCullFace, (GLenum m), (m))
GLV(glDeleteTextures, (GLsizei n, const GLuint *t), (n, t))
GLV(glDepthMask, (GLboolean f), (f))
GLV(glDisable, (GLenum c), (c))
GLV(glDisableClientState, (GLenum a), (a))
GLV(glDrawArrays, (GLenum m, GLint f, GLsizei n), (m, f, n))
GLV(glDrawElements, (GLenum m, GLsizei n, GLenum t, const GLvoid *i), (m, n, t, i))
GLV(glEnable, (GLenum c), (c))
GLV(glEnableClientState, (GLenum a), (a))
GLV(glEnd, (void), ())
GLV(glFinish, (void), ())
GLV(glFogf, (GLenum p, GLfloat v), (p, v))
GLV(glFogfv, (GLenum p, const GLfloat *v), (p, v))
GLV(glFogi, (GLenum p, GLint v), (p, v))
GLV(glGenTextures, (GLsizei n, GLuint *t), (n, t))
GLR(GLenum, glGetError, (void), ())
GLV(glGetFloatv, (GLenum p, GLfloat *v), (p, v))
GLV(glGetIntegerv, (GLenum p, GLint *v), (p, v))
GLV(glHint, (GLenum t, GLenum m), (t, m))
GLV(glLineWidth, (GLfloat w), (w))
GLR(const GLubyte *, glGetString, (GLenum n), (n))
GLV(glTexParameterf, (GLenum t, GLenum p, GLfloat v), (t, p, v))
GLV(glLoadIdentity, (void), ())
GLV(glLoadMatrixf, (const GLfloat *m), (m))
GLV(glMatrixMode, (GLenum m), (m))
GLV(glMultMatrixf, (const GLfloat *m), (m))
GLV(glNormalPointer, (GLenum t, GLsizei s, const GLvoid *p), (t, s, p))
GLV(glOrtho, (GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f), (l, r, b, t, n, f))
GLV(glPixelStorei, (GLenum p, GLint v), (p, v))
GLV(glPolygonOffset, (GLfloat f, GLfloat u), (f, u))
GLV(glPopAttrib, (void), ())
GLV(glPopMatrix, (void), ())
GLV(glPushAttrib, (GLbitfield m), (m))
GLV(glPushMatrix, (void), ())
GLV(glReadPixels, (GLint x, GLint y, GLsizei w, GLsizei h, GLenum f, GLenum t, GLvoid *p), (x, y, w, h, f, t, p))
GLV(glRotatef, (GLfloat a, GLfloat x, GLfloat y, GLfloat z), (a, x, y, z))
GLV(glScalef, (GLfloat x, GLfloat y, GLfloat z), (x, y, z))
GLV(glScissor, (GLint x, GLint y, GLsizei w, GLsizei h), (x, y, w, h))
GLV(glShadeModel, (GLenum m), (m))
GLV(glTexCoord2f, (GLfloat s, GLfloat t), (s, t))
GLV(glTexCoordPointer, (GLint n, GLenum t, GLsizei s, const GLvoid *p), (n, t, s, p))
GLV(glTexEnvf, (GLenum t, GLenum p, GLfloat v), (t, p, v))
GLV(glTexEnvi, (GLenum t, GLenum p, GLint v), (t, p, v))
GLV(glTexImage2D, (GLenum t, GLint l, GLint i, GLsizei w, GLsizei h, GLint b, GLenum f, GLenum y, const GLvoid *p), (t, l, i, w, h, b, f, y, p))
GLV(glTexSubImage2D, (GLenum t, GLint l, GLint x, GLint y, GLsizei w, GLsizei h, GLenum f, GLenum ty, const GLvoid *p), (t, l, x, y, w, h, f, ty, p))
GLV(glTexParameteri, (GLenum t, GLenum p, GLint v), (t, p, v))
GLV(glTranslatef, (GLfloat x, GLfloat y, GLfloat z), (x, y, z))
GLV(glVertex2f, (GLfloat x, GLfloat y), (x, y))
GLV(glVertexPointer, (GLint n, GLenum t, GLsizei s, const GLvoid *p), (n, t, s, p))
GLV(glViewport, (GLint x, GLint y, GLsizei w, GLsizei h), (x, y, w, h))

/* ------------------------------------------------------------ the window */

void *rgl_ff_proc(const char *name);
static const rgl_backend *g_b;

int rgl_open(void *window, int width, int height)
{
    if (rgl_renderer == RGL_OPENGL)
        return 0;
#ifdef _WIN32
    g_b = rgl_renderer == RGL_D3D12 ? &rgl_d3d12 : &rgl_vk;
#else
    g_b = &rgl_vk;
#endif
    printf("renderer: %s\n", g_b->name);
    if (!g_b->open(window, width, height)) {
        printf("renderer: %s did not start, drawing with OpenGL\n", g_b->name);
        g_b->close();
        g_b = NULL;
        rgl_renderer = RGL_OPENGL;
        return 0;
    }
    rgl_ff_start(g_b, width, height);
    return 1;
}

void rgl_present(void)
{
    if (g_b) {
        g_b->present();
        rgl_ff_frame();
    }
}

void rgl_close(void)
{
    if (g_b) {
        rgl_ff_stop();
        g_b->close();
        g_b = NULL;
    }
}

void *rgl_proc(const char *name)
{
    return g_b ? rgl_ff_proc(name) : NULL;
}
