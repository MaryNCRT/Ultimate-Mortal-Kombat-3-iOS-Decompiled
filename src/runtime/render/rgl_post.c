/*
 * rgl_post.c -- PHOTO MODE's effects over the picture (render/rgl.h). Port,
 * not in the binary.
 *
 * With Vulkan or Direct3D 12 the pass is the backends' own shaders
 * (rgl_ff.c rgl_ff_post, shaders/ff.frag and ff.hlsl post_fx). With OpenGL,
 * which draws everything else with the fixed function, it is a GLSL 1.20
 * program of its own, made the first time it is asked for:
 *
 *   1  the game's area -- the current draw framebuffer's viewport -- blitted
 *      into a framebuffer of ours, its colour and its depth as textures
 *      (glBlitFramebuffer resolves a multisampled one; the depth copied too
 *      where the formats agree, else from the window by glCopyTexImage2D)
 *   2  one quad over that viewport with the program: the same effects, in
 *      the same order and with the same settings, as post_fx
 *
 * The depth is turned back into a distance with the scene's projection
 * (rgl_post_scene, taken in RenderGameView), for the depth of field. A
 * driver without GLSL or framebuffer objects (Windows' own 1.1 renderer)
 * has none of it: rgl_post_available is 0 and the panel says so.
 */
#include <stdio.h>
#include <string.h>

#include "../platform/gl.h"
#include "../platform/platform.h"
#include "rgl.h"

#ifdef _WIN32
#define PA __stdcall
#else
#define PA
#endif

#define P_FRAGMENT_SHADER   0x8B30
#define P_COMPILE_STATUS    0x8B81
#define P_LINK_STATUS       0x8B82
#define P_TEXTURE0          0x84C0
#define P_FRAMEBUFFER       0x8D40
#define P_READ_FRAMEBUFFER  0x8CA8
#define P_DRAW_FRAMEBUFFER  0x8CA9
#define P_DRAW_FB_BINDING   0x8CA6
#define P_READ_FB_BINDING   0x8CAA
#define P_COLOR_ATTACHMENT0 0x8CE0
#define P_DEPTH_ATTACHMENT  0x8D00
#define P_COMPLETE          0x8CD5
#define P_DEPTH_COMPONENT24 0x81A6
#define P_CLAMP_TO_EDGE     0x812F

typedef unsigned (PA *f_create_shader)(GLenum);
typedef void     (PA *f_shader_source)(unsigned, GLsizei, const char *const *, const GLint *);
typedef void     (PA *f_uint)(unsigned);
typedef void     (PA *f_get_iv)(unsigned, GLenum, GLint *);
typedef void     (PA *f_get_log)(unsigned, GLsizei, GLsizei *, char *);
typedef unsigned (PA *f_create_program)(void);
typedef void     (PA *f_attach)(unsigned, unsigned);
typedef GLint    (PA *f_uniform_loc)(unsigned, const char *);
typedef void     (PA *f_uniform1i)(GLint, GLint);
typedef void     (PA *f_uniform4fv)(GLint, GLsizei, const GLfloat *);
typedef void     (PA *f_active_tex)(GLenum);
typedef void     (PA *f_gen)(GLsizei, GLuint *);
typedef void     (PA *f_bind)(GLenum, GLuint);
typedef void     (PA *f_fb_tex)(GLenum, GLenum, GLenum, GLuint, GLint);
typedef GLenum   (PA *f_status)(GLenum);
typedef void     (PA *f_blit)(GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum);

static struct {
    int tried, ok;
    f_create_shader create_shader;
    f_shader_source shader_source;
    f_uint compile, link, use;
    f_get_iv shader_iv, program_iv;
    f_get_log shader_log, program_log;
    f_create_program create_program;
    f_attach attach;
    f_uniform_loc loc;
    f_uniform1i u1i;
    f_uniform4fv u4fv;
    f_active_tex active;
    f_gen gen_fb;
    f_bind bind_fb;
    f_fb_tex fb_tex;
    f_status status;
    f_blit blit;
    unsigned prog;
    GLint u_col, u_dep, u_p, u_vp, u_proj;
    GLuint fb, tex_c, tex_d;
    int w, h;
} G;

static float g_proj[4];                 /* m10, m14: the depth back to a distance */

/* post_fx of shaders/ff.frag, in GLSL 1.20: the same order and settings */
static const char *const k_fs =
    "#version 120\n"
    "uniform sampler2D col;\n"
    "uniform sampler2D dep;\n"
    "uniform vec4 p[8];\n"
    "uniform vec4 vp;\n"            /* the viewport, bottom-left */
    "uniform vec4 proj;\n"          /* m10, m14, the depth there, - */
    "vec3 C(vec2 uv) { return texture2D(col, clamp(uv, 0.0, 1.0)).rgb; }\n"
    "float D(vec2 uv) {\n"
    "    float z = texture2D(dep, clamp(uv, 0.0, 1.0)).r * 2.0 - 1.0;\n"
    "    return proj.y / (z + proj.x);\n"
    "}\n"
    "void main() {\n"
    "    vec4 P0 = p[0], P1 = p[1], P2 = p[2], P3 = p[3], P4 = p[4];\n"
    "    vec2 fc = gl_FragCoord.xy;\n"
    "    vec2 uv = (fc - vp.xy) / vp.zw;\n"
    "    vec2 px = 1.0 / vp.zw;\n"
    "    vec3 Y = vec3(0.299, 0.587, 0.114);\n"
    "    vec2 dc = (uv - 0.5) * P3.x * 0.012;\n"
    "    vec3 c = vec3(C(uv + dc).r, C(uv).g, C(uv - dc).b);\n"
    "    if (P4.x > 0.5 && proj.z > 0.5) {\n"
    "        float df = (D(vec2(0.5)) + D(vec2(0.47, 0.5)) + D(vec2(0.53, 0.5))\n"
    "                  + D(vec2(0.5, 0.46)) + D(vec2(0.5, 0.54))) * 0.2 * P4.y;\n"
    "        float coc = clamp(abs(D(uv) - df) / max(df, 1e-4) * P4.z, 0.0, 1.0) * 10.0;\n"
    "        if (coc > 0.5) {\n"
    "            vec3 acc = c;\n"
    "            for (int i = 0; i < 24; i++) {\n"
    "                float a = float(i) * 2.39996, r = sqrt((float(i) + 0.5) / 24.0) * coc;\n"
    "                acc += C(uv + vec2(cos(a), sin(a)) * r * px);\n"
    "            }\n"
    "            c = acc / 25.0;\n"
    "        }\n"
    "    }\n"
    "    if (P3.y > 0.0) {\n"
    "        vec3 b = vec3(0.0);\n"
    "        for (int i = 0; i < 16; i++) {\n"
    "            float a = float(i) * 2.39996, r = (4.0 + float(i)) * 1.2;\n"
    "            b += max(C(uv + vec2(cos(a), sin(a)) * r * px) - 0.6, 0.0);\n"
    "        }\n"
    "        c += b * (P3.y * 0.25);\n"
    "    }\n"
    "    if (P1.w > 0.0) {\n"
    "        vec3 n = C(uv + vec2(px.x, 0.0)) + C(uv - vec2(px.x, 0.0))\n"
    "               + C(uv + vec2(0.0, px.y)) + C(uv - vec2(0.0, px.y));\n"
    "        c += (c * 4.0 - n) * (P1.w * 0.25);\n"
    "    }\n"
    "    c *= exp2(P0.x);\n"
    "    c *= vec3(1.0 + 0.25 * P1.x, 1.0 - 0.15 * P1.y, 1.0 - 0.25 * P1.x);\n"
    "    float l = dot(c, Y);\n"
    "    c += P2.y * 0.35 * (1.0 - smoothstep(0.0, 0.5, l));\n"
    "    c += P2.x * 0.35 * smoothstep(0.5, 1.0, l);\n"
    "    c += P0.y;\n"
    "    c = (c - 0.5) * P0.z + 0.5;\n"
    "    l = dot(c, Y);\n"
    "    c = mix(vec3(l), c, P0.w);\n"
    "    int fl = int(P3.z + 0.5);\n"
    "    if (fl == 1) c = vec3(l);\n"
    "    else if (fl == 2) c = vec3(l) * vec3(1.07, 0.86, 0.62) + vec3(0.06, 0.03, 0.0);\n"
    "    else if (fl == 3) c *= vec3(0.85, 0.97, 1.15);\n"
    "    else if (fl == 4) c *= vec3(1.15, 1.0, 0.82);\n"
    "    else if (fl == 5) c = mix(c, vec3(c.r * 1.2, c.g * 0.55, c.b * 0.5), 0.85);\n"
    "    else if (fl == 6) c = mix(c, vec3(c.r * 0.9 + 0.05, c.g * 0.75, c.b * 1.2 + 0.05), 0.8);\n"
    "    else if (fl == 7) c = vec3(smoothstep(0.12, 0.88, l));\n"
    "    else if (fl == 8) c = mix(c, vec3(l), 0.35) * vec3(1.05, 1.0, 0.88) + 0.04;\n"
    "    c = pow(max(c, vec3(0.0)), vec3(1.0 / max(P1.z, 0.1)));\n"
    "    vec2 q = (uv - 0.5) * vec2(1.0, vp.w / vp.z) * 1.6;\n"
    "    c *= mix(1.0, smoothstep(1.05, 0.25, length(q)), P2.z);\n"
    "    c += (fract(sin(dot(fc + P3.w * 61.0, vec2(12.9898, 78.233))) * 43758.5453) - 0.5) * P2.w * 0.2;\n"
    "    gl_FragColor = vec4(clamp(c, 0.0, 1.0), 1.0);\n"
    "}\n";

static void *proc(const char *n)
{
    return plat_gl_proc(n);
}

static int gl_setup(void)
{
    unsigned fs;
    GLint ok = 0;

    if (G.tried)
        return G.ok;
    G.tried = 1;
    G.create_shader = (f_create_shader)proc("glCreateShader");
    G.shader_source = (f_shader_source)proc("glShaderSource");
    G.compile = (f_uint)proc("glCompileShader");
    G.shader_iv = (f_get_iv)proc("glGetShaderiv");
    G.shader_log = (f_get_log)proc("glGetShaderInfoLog");
    G.create_program = (f_create_program)proc("glCreateProgram");
    G.attach = (f_attach)proc("glAttachShader");
    G.link = (f_uint)proc("glLinkProgram");
    G.program_iv = (f_get_iv)proc("glGetProgramiv");
    G.program_log = (f_get_log)proc("glGetProgramInfoLog");
    G.use = (f_uint)proc("glUseProgram");
    G.loc = (f_uniform_loc)proc("glGetUniformLocation");
    G.u1i = (f_uniform1i)proc("glUniform1i");
    G.u4fv = (f_uniform4fv)proc("glUniform4fv");
    G.active = (f_active_tex)proc("glActiveTexture");
    G.gen_fb = (f_gen)proc("glGenFramebuffers");
    G.bind_fb = (f_bind)proc("glBindFramebuffer");
    G.fb_tex = (f_fb_tex)proc("glFramebufferTexture2D");
    G.status = (f_status)proc("glCheckFramebufferStatus");
    G.blit = (f_blit)proc("glBlitFramebuffer");
    if (!G.create_shader || !G.shader_source || !G.compile || !G.shader_iv || !G.create_program
        || !G.attach || !G.link || !G.program_iv || !G.use || !G.loc || !G.u1i || !G.u4fv
        || !G.active || !G.gen_fb || !G.bind_fb || !G.fb_tex || !G.status || !G.blit) {
        printf("photo effects: this OpenGL has no GLSL or framebuffer objects, no effects\n");
        return 0;
    }
    fs = G.create_shader(P_FRAGMENT_SHADER);
    G.shader_source(fs, 1, &k_fs, NULL);
    G.compile(fs);
    G.shader_iv(fs, P_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024] = "";
        if (G.shader_log)
            G.shader_log(fs, sizeof log, NULL, log);
        printf("photo effects: the shader did not compile: %s\n", log);
        return 0;
    }
    G.prog = G.create_program();
    G.attach(G.prog, fs);
    G.link(G.prog);
    G.program_iv(G.prog, P_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024] = "";
        if (G.program_log)
            G.program_log(G.prog, sizeof log, NULL, log);
        printf("photo effects: the program did not link: %s\n", log);
        return 0;
    }
    G.u_col = G.loc(G.prog, "col");
    G.u_dep = G.loc(G.prog, "dep");
    G.u_p = G.loc(G.prog, "p");
    G.u_vp = G.loc(G.prog, "vp");
    G.u_proj = G.loc(G.prog, "proj");
    printf("photo effects: OpenGL program ready\n");
    return G.ok = 1;
}

/* our framebuffer, the viewport's size: colour and depth as textures */
static int gl_targets(int w, int h)
{
    if (G.fb && G.w == w && G.h == h)
        return 1;
    if (!G.fb) {
        G.gen_fb(1, &G.fb);
        glGenTextures(1, &G.tex_c);
        glGenTextures(1, &G.tex_d);
    }
    glBindTexture(GL_TEXTURE_2D, G.tex_c);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, P_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, P_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, G.tex_d);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, P_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, P_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, P_DEPTH_COMPONENT24, w, h, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);
    G.bind_fb(P_FRAMEBUFFER, G.fb);
    G.fb_tex(P_FRAMEBUFFER, P_COLOR_ATTACHMENT0, GL_TEXTURE_2D, G.tex_c, 0);
    G.fb_tex(P_FRAMEBUFFER, P_DEPTH_ATTACHMENT, GL_TEXTURE_2D, G.tex_d, 0);
    if (G.status(P_FRAMEBUFFER) != P_COMPLETE) {
        printf("photo effects: the %dx%d framebuffer will not complete\n", w, h);
        G.w = G.h = 0;
        return 0;
    }
    G.w = w;
    G.h = h;
    return 1;
}

static int gl_post(const float *prm)
{
    GLint vp[4], draw_fb = 0, read_fb = 0, tex0 = 0, depth_ok = 0;
    float vpf[4], projv[4];

    if (!gl_setup())
        return 0;
    glGetIntegerv(GL_VIEWPORT, vp);
    if (vp[2] <= 0 || vp[3] <= 0)
        return 0;
    glGetIntegerv(P_DRAW_FB_BINDING, &draw_fb);
    glGetIntegerv(P_READ_FB_BINDING, &read_fb);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &tex0);
    while (glGetError() != GL_NO_ERROR)
        ;
    if (!gl_targets(vp[2], vp[3])) {
        G.bind_fb(P_FRAMEBUFFER, (GLuint)draw_fb);
        return 0;
    }
    /* 1 the picture, and its depth where the formats agree */
    G.bind_fb(P_READ_FRAMEBUFFER, (GLuint)draw_fb);
    G.bind_fb(P_DRAW_FRAMEBUFFER, G.fb);
    G.blit(vp[0], vp[1], vp[0] + vp[2], vp[1] + vp[3], 0, 0, vp[2], vp[3],
           GL_COLOR_BUFFER_BIT, GL_NEAREST);
    if (rgl_post_want_depth) {
        while (glGetError() != GL_NO_ERROR)
            ;
        G.blit(vp[0], vp[1], vp[0] + vp[2], vp[1] + vp[3], 0, 0, vp[2], vp[3],
               GL_DEPTH_BUFFER_BIT, GL_NEAREST);
        depth_ok = glGetError() == GL_NO_ERROR;
        if (!depth_ok && draw_fb == 0) {
            /* the window's depth (with stencil): copied, not blitted */
            G.bind_fb(P_READ_FRAMEBUFFER, 0);
            glBindTexture(GL_TEXTURE_2D, G.tex_d);
            glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, vp[0], vp[1], vp[2], vp[3]);
            depth_ok = glGetError() == GL_NO_ERROR;
            glBindTexture(GL_TEXTURE_2D, 0);
        }
    }
    /* 2 the quad over the viewport, back in the game's framebuffer */
    G.bind_fb(P_DRAW_FRAMEBUFFER, (GLuint)draw_fb);
    G.bind_fb(P_READ_FRAMEBUFFER, (GLuint)read_fb);
    glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glDepthMask(GL_FALSE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    G.active(P_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D, G.tex_d);
    G.active(P_TEXTURE0);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, G.tex_c);
    G.use(G.prog);
    G.u1i(G.u_col, 0);
    G.u1i(G.u_dep, 1);
    G.u4fv(G.u_p, 8, prm);
    vpf[0] = (float)vp[0]; vpf[1] = (float)vp[1]; vpf[2] = (float)vp[2]; vpf[3] = (float)vp[3];
    G.u4fv(G.u_vp, 1, vpf);
    projv[0] = g_proj[0];
    projv[1] = g_proj[1];
    projv[2] = depth_ok && g_proj[1] != 0.0f ? 1.0f : 0.0f;
    projv[3] = 0.0f;
    G.u4fv(G.u_proj, 1, projv);
    glBegin(GL_QUADS);
    glVertex2f(-1.0f, -1.0f);
    glVertex2f(1.0f, -1.0f);
    glVertex2f(1.0f, 1.0f);
    glVertex2f(-1.0f, 1.0f);
    glEnd();
    G.use(0);
    glBindTexture(GL_TEXTURE_2D, (GLuint)tex0);
    G.active(P_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D, 0);
    G.active(P_TEXTURE0);
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopAttrib();
    return projv[2] > 0.5f;
}

void rgl_post_scene(void)
{
    float m[16];

    if (rgl_renderer != RGL_OPENGL)
        return;
    glGetFloatv(GL_PROJECTION_MATRIX, m);
    g_proj[0] = m[10];
    g_proj[1] = m[14];
}

int rgl_post_available(void)
{
    if (rgl_renderer == RGL_OPENGL)
        return gl_setup();
    return 1;
}

int rgl_post_apply(const float *prm)
{
    if (rgl_renderer == RGL_OPENGL)
        return gl_post(prm);
    return rgl_ff_post(prm);
}
