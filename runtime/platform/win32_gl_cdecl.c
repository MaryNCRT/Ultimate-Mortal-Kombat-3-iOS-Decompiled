/*
 * win32_gl_cdecl.c -- plain-C GL entry points for 32-bit Windows.
 *
 * See runtime/platform/gl.h. On x86-32, opengl32.dll exports __stdcall
 * functions (`_glDisable@4`), while the decompiled code -- and, on this target,
 * every file that includes platform/gl.h -- calls plain cdecl ones
 * (`_glDisable`). Each function below is the cdecl name, forwarding to the
 * DLL's entry point. The two decorated names differ, so they coexist.
 *
 * This file deliberately does NOT include platform/gl.h: it is the one place
 * that has to see both conventions, and it spells out its own types.
 *
 * Only OpenGL 1.1 is here, because that is all opengl32.dll exports and all
 * the tree calls. `glActiveTexture`, `glClientActiveTexture` and `glOrthof` are
 * defined by the runtime itself.
 */
#if defined(_WIN32) && !defined(_WIN64)

#include <windows.h>

typedef unsigned int  GLenum, GLbitfield, GLuint;
typedef int           GLint, GLsizei;
typedef unsigned char GLboolean;
typedef float         GLfloat, GLclampf;
typedef double        GLdouble;
typedef void          GLvoid;

static FARPROC gl_proc(const char *name)
{
    static HMODULE dll;
    FARPROC p;

    if (dll == NULL)
        dll = LoadLibraryA("opengl32.dll");
    p = dll ? GetProcAddress(dll, name) : NULL;
    if (p == NULL) {
        MessageBoxA(NULL, name, "opengl32.dll has no such entry point", MB_OK);
        ExitProcess(1);
    }
    return p;
}

/* GL(ret, name, (typed params), (args)) */
#define GL(ret, name, params, args)                                         \
    ret name params                                                         \
    {                                                                       \
        typedef ret (WINAPI *fn_t) params;                                  \
        static fn_t fn;                                                     \
        if (fn == NULL)                                                     \
            fn = (fn_t)(void *)gl_proc(#name);                              \
        return fn args;                                                     \
    }

GL(void, glAlphaFunc, (GLenum f, GLclampf r), (f, r))
GL(void, glBegin, (GLenum m), (m))
GL(void, glBindTexture, (GLenum t, GLuint x), (t, x))
GL(void, glBlendFunc, (GLenum s, GLenum d), (s, d))
GL(void, glClear, (GLbitfield m), (m))
GL(void, glClearColor, (GLclampf r, GLclampf g, GLclampf b, GLclampf a), (r, g, b, a))
GL(void, glColor3f, (GLfloat r, GLfloat g, GLfloat b), (r, g, b))
GL(void, glColor4f, (GLfloat r, GLfloat g, GLfloat b, GLfloat a), (r, g, b, a))
GL(void, glColorMask, (GLboolean r, GLboolean g, GLboolean b, GLboolean a), (r, g, b, a))
GL(void, glColorPointer, (GLint n, GLenum t, GLsizei s, const GLvoid *p), (n, t, s, p))
GL(void, glCullFace, (GLenum m), (m))
GL(void, glDeleteTextures, (GLsizei n, const GLuint *t), (n, t))
GL(void, glDepthMask, (GLboolean f), (f))
GL(void, glDisable, (GLenum c), (c))
GL(void, glDisableClientState, (GLenum a), (a))
GL(void, glDrawArrays, (GLenum m, GLint f, GLsizei n), (m, f, n))
GL(void, glDrawElements, (GLenum m, GLsizei n, GLenum t, const GLvoid *i), (m, n, t, i))
GL(void, glEnable, (GLenum c), (c))
GL(void, glEnableClientState, (GLenum a), (a))
GL(void, glEnd, (void), ())
GL(void, glFinish, (void), ())
GL(void, glFogf, (GLenum p, GLfloat v), (p, v))
GL(void, glFogfv, (GLenum p, const GLfloat *v), (p, v))
GL(void, glFogi, (GLenum p, GLint v), (p, v))
GL(void, glGenTextures, (GLsizei n, GLuint *t), (n, t))
GL(GLenum, glGetError, (void), ())
GL(void, glGetFloatv, (GLenum p, GLfloat *v), (p, v))
GL(void, glGetIntegerv, (GLenum p, GLint *v), (p, v))
GL(void, glHint, (GLenum t, GLenum m), (t, m))
GL(void, glLineWidth, (GLfloat w), (w))
GL(void, glLoadIdentity, (void), ())
GL(void, glLoadMatrixf, (const GLfloat *m), (m))
GL(void, glMatrixMode, (GLenum m), (m))
GL(void, glMultMatrixf, (const GLfloat *m), (m))
GL(void, glNormalPointer, (GLenum t, GLsizei s, const GLvoid *p), (t, s, p))
GL(void, glOrtho, (GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f), (l, r, b, t, n, f))
GL(void, glPixelStorei, (GLenum p, GLint v), (p, v))
GL(void, glPolygonOffset, (GLfloat f, GLfloat u), (f, u))
GL(void, glPopAttrib, (void), ())
GL(void, glPopMatrix, (void), ())
GL(void, glPushAttrib, (GLbitfield m), (m))
GL(void, glPushMatrix, (void), ())
GL(void, glReadPixels, (GLint x, GLint y, GLsizei w, GLsizei h, GLenum f, GLenum t, GLvoid *p), (x, y, w, h, f, t, p))
GL(void, glRotatef, (GLfloat a, GLfloat x, GLfloat y, GLfloat z), (a, x, y, z))
GL(void, glScalef, (GLfloat x, GLfloat y, GLfloat z), (x, y, z))
GL(void, glScissor, (GLint x, GLint y, GLsizei w, GLsizei h), (x, y, w, h))
GL(void, glShadeModel, (GLenum m), (m))
GL(void, glTexCoord2f, (GLfloat s, GLfloat t), (s, t))
GL(void, glTexCoordPointer, (GLint n, GLenum t, GLsizei s, const GLvoid *p), (n, t, s, p))
GL(void, glTexEnvf, (GLenum t, GLenum p, GLfloat v), (t, p, v))
GL(void, glTexEnvi, (GLenum t, GLenum p, GLint v), (t, p, v))
GL(void, glTexImage2D, (GLenum t, GLint l, GLint i, GLsizei w, GLsizei h, GLint b, GLenum f, GLenum y, const GLvoid *p), (t, l, i, w, h, b, f, y, p))
GL(void, glTexSubImage2D, (GLenum t, GLint l, GLint x, GLint y, GLsizei w, GLsizei h, GLenum f, GLenum ty, const GLvoid *p), (t, l, x, y, w, h, f, ty, p))
GL(void, glTexParameteri, (GLenum t, GLenum p, GLint v), (t, p, v))
GL(void, glTranslatef, (GLfloat x, GLfloat y, GLfloat z), (x, y, z))
GL(void, glVertex2f, (GLfloat x, GLfloat y), (x, y))
GL(void, glVertexPointer, (GLint n, GLenum t, GLsizei s, const GLvoid *p), (n, t, s, p))
GL(void, glViewport, (GLint x, GLint y, GLsizei w, GLsizei h), (x, y, w, h))

#endif
