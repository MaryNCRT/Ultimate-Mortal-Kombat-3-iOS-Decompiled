/*
 * lime_glapi.h -- the calling convention of the GL entry points.
 *
 * The transcribed code declares the GL calls it makes itself, the way the
 * binary links them: as plain functions. On 32-bit Windows opengl32 exports
 * __stdcall entry points instead (`_glPopMatrix@0`).
 *
 * Two fixes for that were written in parallel. This header used to make the
 * decompiled declarations __stdcall on i686; runtime/platform/gl.h instead
 * declares every GL call cdecl on that target and
 * runtime/platform/win32_gl_cdecl.c forwards each one to the real entry point.
 * A file that includes both headers sees one function with two conventions,
 * which does not compile, so only one can stand. The forwarders win: they
 * keep the decompiled declarations exactly as the binary has them and put the
 * difference in one platform file.
 *
 * LIME_GLAPI is therefore empty everywhere. It is kept so the declarations
 * that carry it still compile, and so a future port with a real reason for a
 * convention has one place to say so.
 */
#ifndef LIME_GLAPI_H
#define LIME_GLAPI_H

#define LIME_GLAPI

#endif
