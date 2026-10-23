/*
 * rgl.h -- what draws the game: OpenGL, Vulkan or Direct3D 12. Port, not in
 * the binary.
 *
 * The decompiled code draws with OpenGL ES 1.1 calls, as the iPhone did, and
 * the port's own screens (debug menu, settings, boxes) with the same fixed
 * function calls. Every one of them is a plain `glX` function defined in
 * rgl_front.c. With OpenGL chosen they go straight to the driver's entry
 * points, as before. With Vulkan or Direct3D 12 they go to rgl_ff.c, which
 * keeps the fixed-function state (matrices, blending, depth, the bound
 * texture, the client arrays, framebuffer objects) and turns each draw into
 * transformed vertices and a small state block for a backend:
 *
 *     rgl_vk.c      Vulkan 1.0 (Windows and Linux)
 *     rgl_d3d12.c   Direct3D 12 (Windows)
 *
 * The shaders that stand in for the fixed function are in render/shaders/;
 * their compiled forms are rgl_shaders_vk.h and rgl_shaders_d3d12.h.
 */
#ifndef UMK3_RGL_H
#define UMK3_RGL_H

#include <stddef.h>

enum { RGL_OPENGL, RGL_VULKAN, RGL_D3D12 };

/* What draws: umk3.ini renderer=opengl|vulkan|d3d12, set before the window
 * opens; a backend that cannot start falls back to OpenGL. */
extern int rgl_renderer;
int  rgl_parse(const char *name);          /* RGL_* for an ini value */
const char *rgl_label(int renderer);       /* "OpenGL", "Vulkan", "Direct3D 12" */

/* The window side, called by the platform layer (win32_gl.c, sdl_gl.c).
 * `window` is the HWND or the SDL_Window. 1 when rgl_renderer's backend is
 * up; 0 and rgl_renderer = RGL_OPENGL when it is not. */
int  rgl_open(void *window, int width, int height);
void rgl_present(void);                    /* the frame to the screen */
void rgl_close(void);
void *rgl_proc(const char *name);          /* the FBO entry points */

/* ----------------------------------------------------- backend interface */

/* A vertex as the backends take it: clip-space position (OpenGL's
 * convention, z from -w to w), colour as RGBA bytes, texture coordinates. */
typedef struct {
    float x, y, z, w;
    unsigned char rgba[4];
    float u, v;
} rgl_vtx;

enum { RGL_TRIS, RGL_LINES };
enum { RGL_BF_ZERO, RGL_BF_ONE, RGL_BF_SRC_ALPHA, RGL_BF_ONE_MINUS_SRC_ALPHA,
       RGL_BF_DST_ALPHA, RGL_BF_ONE_MINUS_DST_ALPHA, RGL_BF_SRC_COLOR,
       RGL_BF_ONE_MINUS_SRC_COLOR, RGL_BF_DST_COLOR, RGL_BF_ONE_MINUS_DST_COLOR };
enum { RGL_CULL_NONE, RGL_CULL_FRONT, RGL_CULL_BACK };
enum { RGL_TEX_NONE, RGL_TEX_MODULATE, RGL_TEX_REPLACE };

/* Everything a pipeline is made of; equal keys share one. */
typedef struct {
    unsigned char topo;                    /* RGL_TRIS / RGL_LINES */
    unsigned char blend, src, dst;         /* blend on, RGL_BF_* */
    unsigned char depth_test, depth_write;
    unsigned char cull;                    /* RGL_CULL_* (front = CCW) */
    unsigned char mask;                    /* colour mask, bit 0 red .. 3 alpha */
} rgl_pipe;

/* What the fragment shader is told per draw. */
typedef struct {
    int   tex_mode;                        /* RGL_TEX_* */
    int   alpha_func;                      /* 0 none, else the GL compare enum */
    float alpha_ref;
    int   pad;
} rgl_frag;

/* A texture's sampling, packed: filter and wrap as GL has them. */
typedef struct {
    unsigned char mag_linear, min_linear, mip; /* mip: 0 none, 1 nearest, 2 linear */
    unsigned char wrap_s_clamp, wrap_t_clamp;
    unsigned char aniso;                   /* 1..16 */
} rgl_samp;

/* GL rectangles, lower-left origin, on the current target. */
typedef struct { int x, y, w, h; } rgl_rect;

typedef struct rgl_backend {
    const char *name;                      /* "Vulkan", "Direct3D 12" */
    int  (*open)(void *window, int width, int height);
    void (*close)(void);
    const char *(*device)(void);           /* the adapter's name */
    int  max_texture, max_samples;

    /* textures: handles from 1; RGBA bytes, rows from the first; `levels`
     * mip levels, each half the last (rgl_ff.c makes them) */
    int  (*tex_new)(void);
    void (*tex_image)(int tex, int w, int h, int levels, const unsigned char *const *rgba);
    void (*tex_sub)(int tex, int x, int y, int w, int h, const void *rgba);
    void (*tex_free)(int tex);

    /* render targets: 0 is the window; others colour + optional depth */
    int  (*target_new)(int w, int h, int samples, int depth);
    void (*target_free)(int target);
    void (*target_use)(int target);        /* what draws go to */
    void (*target_size)(int target, int *w, int *h);

    void (*clear)(const rgl_rect *scissor, int color, const float rgba[4],
                  int depth, float z);
    void (*draw)(const rgl_pipe *p, const rgl_frag *f, int tex, const rgl_samp *s,
                 const rgl_rect *viewport, const rgl_rect *scissor,
                 const rgl_vtx *v, int n);
    /* colour from one target's rectangle to another's, scaled; a
     * multisampled source of the same size is resolved */
    void (*blit)(int src, const rgl_rect *from, int dst, const rgl_rect *to, int linear);
    /* a target's pixels, RGBA, rows from the bottom as glReadPixels */
    void (*read)(int target, const rgl_rect *r, unsigned char *rgba);
    void (*present)(void);
    void (*finish)(void);
} rgl_backend;

extern rgl_backend rgl_vk;
#ifdef _WIN32
extern rgl_backend rgl_d3d12;
#endif

/* Linux: the window's Vulkan surface, made by SDL (sdl_gl.c sets these
 * before rgl_open); Windows makes its own from the HWND. */
extern const char *const *(*rgl_vk_instance_exts)(unsigned *count);
extern int (*rgl_vk_make_surface)(void *window, void *instance, void *surface);

/* rgl_ff.c */
void rgl_ff_start(const rgl_backend *b, int width, int height);
void rgl_ff_stop(void);
void rgl_ff_frame(void);                   /* after a present */

#endif
