/*
 * lime_unused_stubs.c -- what test_skinning_armv7_diff links against and
 * never calls: the skin and bone loaders in RenderSkinned.c reach the file and
 * texture layer, and the oracle reaches the same two texture calls. Anything
 * here that runs is a test bug, so each one says so and stops.
 */
#include "arm_runtime.h"
#include "../src/decomp/lime/lime.h"

#include <stdio.h>
#include <stdlib.h>

static void never(const char *what)
{
    fprintf(stderr, "lime_unused_stubs: %s called\n", what);
    abort();
}

void *limeMalloc(const char *tag, size_t bytes) { (void)tag; (void)bytes; never("limeMalloc"); return NULL; }
void  limeFree(void *p) { (void)p; never("limeFree"); }
void *limeLoadFile(const char *path) { (void)path; never("limeLoadFile"); return NULL; }
TEXTURE *limeLoadTexture(const char *path, int a, int b) { (void)path; (void)a; (void)b; never("limeLoadTexture"); return NULL; }
void  limeDeleteTexture(TEXTURE *t) { (void)t; never("limeDeleteTexture"); }

void func_000670a0_limeLoadTexture(arm_ctx *ctx) { (void)ctx; never("oracle limeLoadTexture"); }
void func_000673cc_limeDeleteTexture(arm_ctx *ctx) { (void)ctx; never("oracle limeDeleteTexture"); }

/* RenderMesh.c's loaders and debug cube, linked by the draw test. */
size_t limeFileSize(const char *path) { (void)path; never("limeFileSize"); return 0; }
SCENEINFO *LIME_LoadScene(const char *path, int a, const char *tex, int b) { (void)path; (void)a; (void)tex; (void)b; never("LIME_LoadScene"); return NULL; }
void func_0005f0ac_LIME_LoadScene(arm_ctx *ctx) { (void)ctx; never("oracle LIME_LoadScene"); }
