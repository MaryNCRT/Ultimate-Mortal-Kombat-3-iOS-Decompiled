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
/* Refreshes each present stays on the screen: 1, or 2/3/4 on a 120/180/240 Hz
 * screen, so every 60 Hz frame shows for the same time (game_main.c). */
extern int rgl_swap_interval;

/* COMPILE SHADERS (the launcher's PLAY): every pipeline a backend has made is
 * a line in a file in the game's folder, made again when the backend opens --
 * from the driver's cache on disk, so it is quick -- and never compiled in
 * the middle of a fight. `seed` is a backend's own list for a first run,
 * lines of the same form. rgl_dir is the folder, with its slash at the end. */
extern char rgl_dir[1024];
void rgl_path(char *out, int n, const char *name);
void rgl_pipes_note(const char *name, const void *key, int keysize, int a, int b, int c);
void rgl_pipes_each(const char *name, const char *seed, int keysize,
                    void (*fn)(const void *key, int a, int b, int c));
void rgl_close(void);
void *rgl_proc(const char *name);          /* the FBO entry points */

/* ----------------------------------------------------- backend interface */

/* A vertex as the backends take it: clip-space position (OpenGL's
 * convention, z from -w to w), colour as RGBA bytes, texture coordinates,
 * and where it is in the shadow map's light (clip space too; REAL-TIME
 * SHADOWS, rgl_frag.shadow 1). */
typedef struct {
    float x, y, z, w;
    unsigned char rgba[4];
    float u, v;
    float s[4];
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
enum { RGL_SHADOW_NONE, RGL_SHADOW_RECEIVE, RGL_SHADOW_DEPTH };
typedef struct {
    int   tex_mode;                        /* RGL_TEX_* */
    int   alpha_func;                      /* 0 none, else the GL compare enum */
    float alpha_ref;
    int   shadow;                          /* RGL_SHADOW_* in the low four
                                            * bits: darkened where the map has
                                            * something nearer the light (the
                                            * bits above: the receiver's own
                                            * caster, skipped), or the map's
                                            * texel itself */
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
    /* the target draws sample as the shadow map from now on (its colour, a
     * depth packed in RGBA8), made readable; 0 none */
    void (*shadow_map)(int target);
} rgl_backend;

extern rgl_backend rgl_vk;
#ifdef _WIN32
extern rgl_backend rgl_d3d12;
#endif

/* Linux: the window's Vulkan surface, made by SDL (sdl_gl.c sets these
 * before rgl_open); Windows makes its own from the HWND. */
extern const char *const *(*rgl_vk_instance_exts)(unsigned *count);
extern int (*rgl_vk_make_surface)(void *window, void *instance, void *surface);

/* REAL-TIME SHADOWS (umk3.ini rt_shadows=1, experimental; rgl_ff.c): the
 * fighters and the stage about them seen from a light over it, into a
 * shadow map, and every 3D draw of the scene darkened where it is behind
 * something. Vulkan and
 * Direct3D 12 only; with OpenGL these do nothing and rgl_shadow_active is 0.
 *
 *   rgl_shadow_camera(1)  the view the scene is drawn with was just set
 *                         (LIMEDS_SetCameraOrientation); 0 when another
 *                         camera (the HUD's) takes over
 *   rgl_shadow_caster(1)  what is drawn now is a fighter, with an id of
 *                         its own each call in a frame, until 0
 *   rgl_shadow_proxy(1)   what is drawn now only casts (a fighter's base
 *                         mesh); 2: what is drawn now does not cast (its
 *                         smoothed mesh); 0 back to normal
 *   rgl_shadow_active()   1 while shadows are on and drawn: the flattened
 *                         shadow under a fighter is left out */
extern int rgl_shadows;
extern double rgl_shadow_ms;           /* this frame's map and held draws, for the log */
extern int rgl_shadow_tris;            /* the triangles the last map was drawn from */
double rgl_now_ms(void);
void rgl_shadow_camera(int on);
void rgl_shadow_caster(int on);
void rgl_shadow_proxy(int mode);
int  rgl_shadow_active(void);

/* rgl_ff.c */
void rgl_ff_start(const rgl_backend *b, int width, int height);
void rgl_ff_stop(void);
void rgl_ff_frame(void);                   /* after a present */

#endif
