/*
 * lime_platform_trace.c -- the platform boundary for GL-stream tests.
 *
 * The clean engine calls a handful of symbols that live in `lime/iphone/lime.m`
 * -- the iOS layer this project rewrites rather than decompiles. For a test
 * that compares CALL STREAMS they cannot be no-ops: the oracle's copies issue
 * real GL calls, so a silent host stub shows up as a missing call and gets
 * blamed on whatever function was under test.
 *
 * These are implemented from what the ORACLE was measured to do, one function
 * at a time with `build/probe`:
 *
 *      limeEnableDepthWrites            -> glDepthMask(1)
 *      limeDisableDepthWrites           -> glDepthMask(0)
 *      limeDisableAlphaBlending         -> glDisable(0x0BE2)
 *      limeEnableAlphaBlending_Basic    -> glBlendFunc(0x0302, 0x0303), glEnable(0x0BE2)
 *      limeEnableAlphaBlending_Additive -> glBlendFunc(0x0302, 0x0001), glEnable(0x0BE2)
 *
 * 0x0BE2 is GL_BLEND, 0x0302 GL_SRC_ALPHA, 0x0303 GL_ONE_MINUS_SRC_ALPHA and
 * 0x0001 GL_ONE -- which confirms from behaviour what docs/ENCARGO.md had only
 * asserted: the additive path really is SRC_ALPHA/ONE and the basic path really
 * is the order-dependent one.
 *
 * Deliberately NOT `runtime/lime_platform.c`. That file's GL entries are
 * no-ops by design, which is right for tests comparing loaders and maths and
 * useless for one comparing a call stream. It also defines its own glXxx, so
 * linking both would collide with tests/gl_trace.c.
 */
#include "gl_trace.h"
#include "../src/decomp/lime/lime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GL_BLEND_                0x0BE2u
#define GL_SRC_ALPHA_            0x0302u
#define GL_ONE_MINUS_SRC_ALPHA_  0x0303u
#define GL_ONE_                  0x0001u

void limeEnableDepthWrites(void)    { glDepthMask(1); }
void limeDisableDepthWrites(void)   { glDepthMask(0); }
void limeDisableAlphaBlending(void) { glDisable(GL_BLEND_); }

void limeEnableAlphaBlending_Basic(void)
{
    glBlendFunc(GL_SRC_ALPHA_, GL_ONE_MINUS_SRC_ALPHA_);
    glEnable(GL_BLEND_);
}

void limeEnableAlphaBlending_Additive(void)
{
    glBlendFunc(GL_SRC_ALPHA_, GL_ONE_);
    glEnable(GL_BLEND_);
}

/* Host memory and files. Real but minimal -- the draw paths load nothing, and
 * these exist because other functions in the same translation units are linked
 * in and have to resolve. */
void  *limeMalloc(const char *tag, size_t n) { (void)tag; return calloc(1, n ? n : 1); }
void   limeFree(void *p) { free(p); }
size_t limeFileSize(const char *path) { (void)path; return 0; }
void  *limeLoadFile(const char *path) { (void)path; return NULL; }

/* None of these should be reached by a draw. Abort rather than return quietly:
 * a silent stub drops a call from one side only, and the divergence then gets
 * attributed to the wrong code. */
static void must_not_call(const char *who)
{
    printf("  FATAL: %s reached from a draw path\n", who);
    exit(3);
}

TEXTURE *limeLoadTexture(const char *p, int a, int b)
{ (void)p; (void)a; (void)b; must_not_call("limeLoadTexture"); return NULL; }

void limeDeleteTexture(TEXTURE *t) { (void)t; must_not_call("limeDeleteTexture"); }

void limeDrawSprite(TEXTURE *page, float x, float y, float w, float h,
                    float u0, float v0, float u1, float v1)
{ (void)page; (void)x; (void)y; (void)w; (void)h;
  (void)u0; (void)v0; (void)u1; (void)v1; must_not_call("limeDrawSprite"); }

void limeDrawRotSpriteFromTopLeft(TEXTURE *page, float x, float y,
                                  float w, float h, float u0, float v0,
                                  float u1, float v1, float angle)
{ (void)page; (void)x; (void)y; (void)w; (void)h;
  (void)u0; (void)v0; (void)u1; (void)v1; (void)angle;
  must_not_call("limeDrawRotSpriteFromTopLeft"); }
