/*
 * The one place that knows where the GL headers live.
 *
 * Win32 needs windows.h pulled in first or GL/gl.h will not compile; Apple
 * moved the headers into a framework. Nothing else in the tree should include
 * a GL header directly.
 */
#ifndef LIME_GL_H
#define LIME_GL_H

#if defined(_WIN32)
/* Windows: every GL declaration is a plain C function -- no __stdcall, no
 * dllimport -- because render/rgl_front.c defines each one, and sends it to
 * opengl32.dll's entry point or to the Vulkan / Direct3D 12 fixed-function
 * layer (render/rgl.h). The decompiled files declare the calls they make
 * themselves, as plain C, the way the ARM binary called them; on 32-bit
 * Windows that is also the only way they agree with opengl32's __stdcall
 * exports, which rgl_front.c calls through pointers. */
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
#elif defined(__APPLE__)
#  include <OpenGL/gl.h>
#else
#  include <GL/gl.h>
#endif

#endif
