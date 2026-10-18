/*
 * The one place that knows where the GL headers live.
 *
 * Win32 needs windows.h pulled in first or GL/gl.h will not compile; Apple
 * moved the headers into a framework. Nothing else in the tree should include
 * a GL header directly.
 */
#ifndef LIME_GL_H
#define LIME_GL_H

#if defined(_WIN32) && !defined(_WIN64)
/* 32-bit Windows: opengl32.dll exports __stdcall entry points, but the
 * decompiled files declare the GL calls they make themselves, as plain C
 * functions -- `void glDisable(unsigned cap);` -- the way the ARM binary
 * called them. On x86-64 and ARM there is one calling convention and the
 * two agree; here they do not, and the mismatch is a compile error at best
 * and a corrupted stack at worst. So on this target every GL declaration is
 * plain cdecl, and runtime/platform/win32_gl_cdecl.c forwards each one to
 * the real entry point. */
#  include <windows.h>
#  pragma push_macro("APIENTRY")
#  pragma push_macro("WINGDIAPI")
#  undef  APIENTRY
#  undef  WINGDIAPI
#  define APIENTRY
#  define WINGDIAPI
#  include <GL/gl.h>
#  pragma pop_macro("WINGDIAPI")
#  pragma pop_macro("APIENTRY")
#elif defined(_WIN32)
#  include <windows.h>
#  include <GL/gl.h>
#elif defined(__APPLE__)
#  include <OpenGL/gl.h>
#else
#  include <GL/gl.h>
#endif

#endif
