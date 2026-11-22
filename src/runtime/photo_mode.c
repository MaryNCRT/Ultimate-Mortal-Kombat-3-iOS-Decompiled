/*
 * photo_mode.c -- PHOTO MODE (phase 1). Port, not in the binary.
 *
 * What it is and why it is built this way: photo_mode.h and
 * docs/PHOTO-MODE.md. In short:
 *
 *   the freeze    GamePaused = 3. Task_GameMain_frame then skips
 *                 UpdateArcadeCode (the fight, AnimateBG, LIME_UpdateEvents)
 *                 and the particles stop their physics (Particles.c reads
 *                 GamePaused), while the scene is still drawn every tick.
 *                 Leaving puts back what GamePaused was.
 *   the camera    an orbit about a point: turn, tilt, distance, roll, the
 *                 point moved or put on a fighter. GameCode.c SetToUseCamera
 *                 hands the scene to it instead of the fight's camera; the
 *                 fight's own (LastEye / LastAt) is left as it was, so the
 *                 fight carries on from the same shot.
 *   the zoom      a scale of the projection's x and y, which for the
 *                 game's symmetric frustum is exactly a narrower or wider
 *                 field of view (LIMEDS_Set3dMode).
 *   the capture   the next frame drawn without the panel and the HUD, read
 *                 from the window once it is there (glReadPixels; Vulkan and
 *                 Direct3D 12 through rgl_ff.c), and written as a PNG by a
 *                 thread of its own into photos/ beside the game.
 *
 * There is no collision with the stage: the game is 2D underneath and its
 * stages carry no collision geometry. The camera keeps to limits instead --
 * a nearest distance, above the floor, a box about the fight.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "savestate.h"                  /* umk3_long */
#include "platform/platform.h"
#include "platform/gl.h"
#include "render/rgl.h"                 /* rgl_dir: the game's folder */
#include "rollback.h"
#include "photo_mode.h"

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#else
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ------------------------------------------------------------- the game */

extern umk3_long GamePaused, GameMode, DoIntro, Character1, Character2;
extern int   CurrentTask;
extern float FE_FadeAdd, FE_HeightScale;
extern int   limeScreenWidth, limeScreenHeight;
extern char  Language[10];
extern const char *CharacterNames[26];
extern float PortMouseX, PortMouseY;    /* game_main.c: the mouse in these units, -1 off */
extern int   PortFTSkipping;            /* game_main.c: the trainer's unseen run-up */
int  rp_playing(void);                  /* rollback_probe.c: a replay */
int  rp_switching(void);
int  rp_paused(void);
void rp_set_paused(int on);

/* GameCode.c, for this file */
void PortPhotoSceneCam(float eye[3], float at[3]);
int  PortPhotoFighter(int i, float out[3]);
void PortPhotoText(const char *s, float x, float y, int align, float scale,
                   const float rgb[3]);
void limeFillRect(float x, float y, float w, float h, float r, float g, float b, float a);
void limeFillRectGrad(float x, float y, float w, float h, const float *c);
void limeEnableAlphaBlending_Basic(void);
void limeSet2DDrawing(void);
void limeDisableDepthTest(void);

static int es(void) { return Language[0] == 'E' && Language[1] == 'S'; }
static const char *tx(const char *s_es, const char *s_en) { return es() ? s_es : s_en; }

/* ------------------------------------------------------------ the state */

static int   g_on, g_from_menu;
/* a replay (rollback_probe.c): it is held while the mode opens, and LIVE
 * plays it on under this camera (Mary: the free camera in the replays) */
static int   g_replay, g_rp_was_paused;
static int   g_free;                    /* a replay's FREE CAMERA is on */
static umk3_long g_prev_paused;

static float g_eye0[3], g_at0[3];       /* the shot it opened on */
static float g_d0, g_ground;            /* that shot's distance; the floor */
static float g_tgt[3];                  /* the point it turns about */
static float g_yaw, g_pitch, g_dist, g_roll, g_zoom = 1.0f;
static float g_yaw0, g_pitch0;
static int   g_focus;                   /* 0 free, 1 player one, 2 two, 3 both */

static int   g_cat, g_row;              /* the panel: category, row */
static int   g_panel_hidden;
static int   g_show_hud;                /* the HUD in the picture: off by default */
static int   g_grid;                    /* the thirds over the view, never captured */
static int   g_pad_last;                /* the hints: 1 the pad was used last */
static int   g_pad_kind;

static int   g_shot;                    /* 1 the next frame is the photo's */
static int   g_shot_drawn;              /* and it was drawn clean */
static char  g_toast[200];
static int   g_toast_t;
static int   g_shots_saved;
static long  g_ticks;                   /* photo_tick's calls, for the log */
static int   g_via_key;                 /* photo_open: from the shortcut */

enum { CAT_CAMERA, CAT_SCENE, CAT_CAPTURE, CAT_N };
static const int g_rows_base[CAT_N] = { 7, 2, 3 };
#define ROWS(c) (g_rows_base[c])

#define DEG(r) ((r) * (float)(180.0 / M_PI))
#define RAD(d) ((d) * (float)(M_PI / 180.0))

static float clampf(float v, float a, float b) { return v < a ? a : v > b ? b : v; }

/* ----------------------------------------------------------- the camera */

static void cam_basis(float eye[3], float up[3])
{
    float cp = cosf(g_pitch), sp = sinf(g_pitch);
    float d[3] = { cp * cosf(g_yaw), cp * sinf(g_yaw), sp };   /* target to eye */
    float f[3] = { -d[0], -d[1], -d[2] };                      /* where it looks */
    float r[3], u[3], len;
    int i;

    for (i = 0; i < 3; i++)
        eye[i] = g_tgt[i] + d[i] * g_dist;
    /* right = f x Z, up = right x f; then turned about f by the roll */
    r[0] = f[1];  r[1] = -f[0];  r[2] = 0.0f;
    len = sqrtf(r[0] * r[0] + r[1] * r[1]);
    if (len < 1e-6f) { r[0] = 1.0f; r[1] = 0.0f; len = 1.0f; }
    r[0] /= len; r[1] /= len;
    u[0] = r[1] * f[2] - r[2] * f[1];
    u[1] = r[2] * f[0] - r[0] * f[2];
    u[2] = r[0] * f[1] - r[1] * f[0];
    for (i = 0; i < 3; i++)
        up[i] = u[i] * cosf(g_roll) + r[i] * sinf(g_roll);
}

/* the limits: tilt, distance, the box about the shot it opened on, and the
 * eye above the floor */
static void cam_limit(void)
{
    float box = g_d0 * 1.5f, eye_z;

    g_pitch = clampf(g_pitch, RAD(-20.0f), RAD(80.0f));
    g_dist  = clampf(g_dist, g_d0 * 0.08f, g_d0 * 2.5f);
    g_zoom  = clampf(g_zoom, 0.5f, 4.0f);
    g_roll  = clampf(g_roll, RAD(-45.0f), RAD(45.0f));
    g_tgt[0] = clampf(g_tgt[0], g_at0[0] - box, g_at0[0] + box);
    g_tgt[1] = clampf(g_tgt[1], g_at0[1] - box, g_at0[1] + box);
    g_tgt[2] = clampf(g_tgt[2], g_ground, g_at0[2] + g_d0);
    eye_z = g_tgt[2] + sinf(g_pitch) * g_dist;
    if (eye_z < g_ground + g_d0 * 0.08f) {
        float s = (g_ground + g_d0 * 0.08f - g_tgt[2]) / g_dist;
        g_pitch = asinf(clampf(s, -1.0f, 1.0f));
    }
}

/* the point on a fighter, at the height the fight's camera looks at */
static void cam_focus(int which)
{
    float a[3], b[3];
    int ha = PortPhotoFighter(0, a), hb = PortPhotoFighter(1, b);

    g_focus = which;
    if (which == 1 && ha) {
        g_tgt[0] = a[0]; g_tgt[1] = a[1];
        g_dist = g_d0 * 0.45f;
    } else if (which == 2 && hb) {
        g_tgt[0] = b[0]; g_tgt[1] = b[1];
        g_dist = g_d0 * 0.45f;
    } else if (which == 3 && ha && hb) {
        g_tgt[0] = (a[0] + b[0]) * 0.5f; g_tgt[1] = (a[1] + b[1]) * 0.5f;
        g_dist = g_d0;
    } else
        return;
    g_tgt[2] = g_at0[2];
    cam_limit();
}

static void cam_reset(void)
{
    memcpy(g_tgt, g_at0, sizeof g_tgt);
    g_yaw = g_yaw0;
    g_pitch = g_pitch0;
    g_dist = g_d0;
    g_roll = 0.0f;
    g_zoom = 1.0f;
    g_focus = 0;
}

int photo_camera(float eye[3], float at[3], float up[3])
{
    if (!photo_on() && !photo_free_on())
        return 0;
    cam_basis(eye, up);
    memcpy(at, g_tgt, 3 * sizeof(float));
    return 1;
}

float photo_proj_scale(void)
{
    return photo_on() || photo_free_on() ? g_zoom : 1.0f;
}

int photo_hide_hud(void)
{
    return photo_on() && (!g_show_hud || g_shot);
}

/* --------------------------------------------------------- in and out */

int photo_on(void)
{
    return g_on && CurrentTask == 6 && GamePaused == PHOTO_PAUSED;
}

int photo_free_on(void)
{
    return g_free && CurrentTask == 6 && rp_playing();
}

int photo_allowed(void)
{
    return CurrentTask == 6 && (GameMode != 1 || rp_playing()) && !rb_active()
           && !rb_watching() && !(rp_playing() && rp_switching()) && !DoIntro
           && FE_FadeAdd == 0.0f && !PortFTSkipping;
}

/* the camera on the shot the fight's camera drew last */
static void cam_from_scene(void)
{
    float d[3];

    PortPhotoSceneCam(g_eye0, g_at0);
    d[0] = g_eye0[0] - g_at0[0];
    d[1] = g_eye0[1] - g_at0[1];
    d[2] = g_eye0[2] - g_at0[2];
    g_d0 = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    if (g_d0 < 1e-3f)
        g_d0 = 1.0f;
    g_yaw0 = atan2f(d[1], d[0]);
    g_pitch0 = asinf(clampf(d[2] / g_d0, -1.0f, 1.0f));
    /* the floor: under the fighters' feet, else well under the shot */
    g_ground = g_at0[2] - g_d0;
    {
        float p[3];
        int i, any = 0;
        for (i = 0; i < 2; i++)
            if (PortPhotoFighter(i, p)) {
                if (!any || p[2] < g_ground)
                    g_ground = p[2];
                any = 1;
            }
    }
    cam_reset();
}

void photo_open(int from_menu)
{
    if (g_on || !photo_allowed() || (GamePaused != 0 && GamePaused != 1))
        return;
    if (!photo_free_on())               /* from the FREE CAMERA: its shot kept */
        cam_from_scene();
    g_prev_paused = GamePaused;
    g_from_menu = from_menu;
    g_replay = rp_playing();
    if (g_replay) {                     /* a replay is held under the photo */
        g_rp_was_paused = rp_paused();
        rp_set_paused(1);
    }
    GamePaused = PHOTO_PAUSED;
    g_on = 1;
    g_shot = g_shot_drawn = 0;
    g_toast_t = 0;
    printf("photo: on at tick %ld (%s%s), eye %.1f %.1f %.1f at %.1f %.1f %.1f\n",
           g_ticks, g_via_key ? "shortcut" : "pause menu row",
           from_menu ? ", back to the pause menu after" : "",
           g_eye0[0], g_eye0[1], g_eye0[2], g_at0[0], g_at0[1], g_at0[2]);
}

/* a replay's FREE CAMERA (Mary): the fight goes on, seen from where the
 * player puts the camera, with the replay's own panel under it */
void photo_free_toggle(void)
{
    if (!rp_playing() || CurrentTask != 6 || g_on)
        return;
    g_free = !g_free;
    if (g_free)
        cam_from_scene();
    printf("photo: FREE CAMERA %s at tick %ld\n", g_free ? "on" : "off", g_ticks);
}

void photo_close(void)
{
    if (!g_on)
        return;
    g_on = 0;
    if (GamePaused == PHOTO_PAUSED)
        GamePaused = g_from_menu ? 1 : (g_prev_paused == 1 ? 1 : 0);
    if (g_replay) {
        rp_set_paused(g_rp_was_paused);
        GamePaused = 0;                 /* a replay has no pause menu */
    }
    g_replay = 0;
    printf("photo: off at tick %ld, back to %s\n", g_ticks,
           g_free ? "the FREE CAMERA" : GamePaused ? "the pause menu" : "the fight");
}

/* ------------------------------------------------------------ the panel */

static const char *cat_name(int c)
{
    static const char *const n_es[CAT_N] = { "CAMARA", "ESCENA", "CAPTURA" };
    static const char *const n_en[CAT_N] = { "CAMERA", "SCENE", "CAPTURE" };
    return es() ? n_es[c] : n_en[c];
}

/* a row's label and its value (empty for an action) */
static void row_text(int c, int r, char *label, char *value, int n)
{
    static const char *const fo_es[4] = { "LIBRE", "JUGADOR 1", "JUGADOR 2", "AMBOS" };
    static const char *const fo_en[4] = { "FREE", "PLAYER 1", "PLAYER 2", "BOTH" };
    value[0] = 0;
    switch (c * 16 + r) {
    case CAT_CAMERA * 16 + 0:
        snprintf(label, n, "%s", tx("OBJETIVO", "TARGET"));
        snprintf(value, n, "%s", es() ? fo_es[g_focus] : fo_en[g_focus]);
        break;
    case CAT_CAMERA * 16 + 1: {
        float y = DEG(g_yaw - g_yaw0);
        while (y > 180.0f) y -= 360.0f;
        while (y < -180.0f) y += 360.0f;
        snprintf(label, n, "%s", tx("GIRO", "ORBIT"));
        snprintf(value, n, "%+.0f", y);
        break;
    }
    case CAT_CAMERA * 16 + 2:
        snprintf(label, n, "%s", tx("INCLINACION", "TILT"));
        snprintf(value, n, "%+.0f", DEG(g_pitch));
        break;
    case CAT_CAMERA * 16 + 3:
        snprintf(label, n, "%s", tx("DISTANCIA", "DISTANCE"));
        snprintf(value, n, "%.0f%%", g_dist / g_d0 * 100.0f);
        break;
    case CAT_CAMERA * 16 + 4:
        snprintf(label, n, "%s", "ZOOM");
        snprintf(value, n, "%.1fX", g_zoom);
        break;
    case CAT_CAMERA * 16 + 5:
        snprintf(label, n, "%s", tx("ALABEO", "ROLL"));
        snprintf(value, n, "%+.0f", DEG(g_roll));
        break;
    case CAT_CAMERA * 16 + 6:
        snprintf(label, n, "%s", tx("RESTABLECER CAMARA", "RESET CAMERA"));
        break;
    case CAT_SCENE * 16 + 0:
        snprintf(label, n, "%s", "HUD");
        snprintf(value, n, "%s", g_show_hud ? tx("VISIBLE", "SHOWN") : tx("OCULTO", "HIDDEN"));
        break;
    case CAT_SCENE * 16 + 1:
        snprintf(label, n, "%s", tx("CUADRICULA", "GRID"));
        snprintf(value, n, "%s", g_grid ? tx("SI", "ON") : tx("NO", "OFF"));
        break;
    case CAT_CAPTURE * 16 + 0:
        snprintf(label, n, "%s", tx("HACER FOTO", "TAKE PHOTO"));
        break;
    case CAT_CAPTURE * 16 + 1:
        snprintf(label, n, "%s", tx("RESOLUCION", "RESOLUTION"));
        snprintf(value, n, "%s", tx("NATIVA", "NATIVE"));
        break;
    case CAT_CAPTURE * 16 + 2:
        snprintf(label, n, "%s", tx("ABRIR CARPETA", "OPEN FOLDER"));
        break;
    default:
        label[0] = 0;
    }
}

static void photo_folder(char *out, int n)
{
    snprintf(out, (size_t)n, "%sphotos", rgl_dir);
}

static void open_folder(void)
{
    char dir[1100];
    photo_folder(dir, sizeof dir);
#ifdef _WIN32
    {
        char cmd[1200];
        STARTUPINFOA si;
        PROCESS_INFORMATION pi;
        char *p;
        CreateDirectoryA(dir, NULL);
        for (p = dir; *p; p++)
            if (*p == '/')
                *p = '\\';
        snprintf(cmd, sizeof cmd, "explorer.exe \"%s\"", dir);
        memset(&si, 0, sizeof si);
        si.cb = sizeof si;
        if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
        }
    }
#else
    {
        char cmd[1200];
        mkdir(dir, 0755);
        snprintf(cmd, sizeof cmd, "xdg-open '%s' >/dev/null 2>&1 &", dir);
        if (system(cmd) != 0)
            printf("photo: could not open %s\n", dir);
    }
#endif
}

static void take_photo(void)
{
    if (!g_shot) {
        g_shot = 1;
        g_shot_drawn = 0;
    }
}

/* a row acted on: `d` -1 / +1 a step left or right, 0 the row's action */
static void row_act(int d)
{
    switch (g_cat * 16 + g_row) {
    case CAT_CAMERA * 16 + 0:
        if (d) {
            int f = (g_focus + (d > 0 ? 1 : 3)) & 3;
            if (f == 0)
                g_focus = 0;
            else
                cam_focus(f);
        }
        break;
    case CAT_CAMERA * 16 + 1: g_yaw += RAD(5.0f) * (float)d; break;
    case CAT_CAMERA * 16 + 2: g_pitch += RAD(5.0f) * (float)d; break;
    case CAT_CAMERA * 16 + 3: g_dist *= d > 0 ? 1.1f : d < 0 ? 1.0f / 1.1f : 1.0f; break;
    case CAT_CAMERA * 16 + 4: g_zoom += 0.1f * (float)d; break;
    case CAT_CAMERA * 16 + 5: g_roll += RAD(2.0f) * (float)d; break;
    case CAT_CAMERA * 16 + 6: if (!d) cam_reset(); break;
    case CAT_SCENE * 16 + 0:  g_show_hud ^= 1; break;
    case CAT_SCENE * 16 + 1:  g_grid ^= 1; break;
    case CAT_CAPTURE * 16 + 0: if (!d) take_photo(); break;
    case CAT_CAPTURE * 16 + 2: if (!d) open_folder(); break;
    default: break;
    }
    if (g_cat == CAT_CAMERA && g_row >= 1 && g_row <= 5 && d)
        g_focus = g_row == 3 || g_row == 4 ? g_focus : 0;
    cam_limit();
}

/* the panel's geometry, in the game's 2D units */
typedef struct { float x, y, w, tab_h, row_h, hdr; } PANEL;

static PANEL panel_geom(void)
{
    float s = FE_HeightScale;
    PANEL p;
    /* compact: under a quarter of the width (Mary: the first one covered
     * too much of the picture) */
    p.x = 6.0f * s;
    p.y = 6.0f * s;
    p.w = 112.0f * s;
    p.tab_h = 12.0f * s;
    p.hdr = 13.0f * s;
    p.row_h = 10.0f * s;
    return p;
}

static float panel_h(const PANEL *p)
{
    return p->tab_h + p->hdr + (float)ROWS(g_cat) * p->row_h + 5.0f * FE_HeightScale;
}

/* ------------------------------------------------------------- the input */

typedef struct { int down, was, rep; } KEY;

static int key_edge(KEY *k, int down)
{
    int hit = down && !k->was;
    k->was = down;
    return hit;
}

/* held: once, then again after a third of a second, every 4 ticks */
static int key_repeat(KEY *k, int down)
{
    int hit = 0;
    if (!down) {
        k->rep = 0;
    } else if (k->rep == 0) {
        hit = 1;
        k->rep = 1;
    } else if (++k->rep > 20 && (k->rep - 20) % 4 == 0) {
        hit = 1;
    }
    k->was = down;
    return hit;
}

static float dead(float v)
{
    return fabsf(v) < 0.2f ? 0.0f : (v - (v > 0 ? 0.2f : -0.2f)) / 0.8f;
}

void photo_tick(const PLAT_PAD *pad, unsigned pad_mask, int (*key)(int))
{
    static KEY k_live;
    static KEY k_open, k_pad_open, k_pause, k_tab, k_hide, k_shot, k_reset,
               k_up, k_dn, k_lt, k_rt, k_ok, k_l1, k_r1, k_a, k_b, k_x, k_y, k_r3, k_start;
    static int mouse_was, mx0, my0, drag;           /* drag: 1 turn, 2 move */
    const float dt = 1.0f / 60.0f;
    unsigned pb = pad && pad->connected ? pad->buttons : 0;
    int combo = pad_mask && (pb & pad_mask) == pad_mask;
    int open_hit = key_edge(&k_open, key(PK_PHOTO));
    int pad_hit = key_edge(&k_pad_open, combo);

    g_ticks++;
    if (g_toast_t > 0)
        g_toast_t--;
    if (g_on && !photo_on()) {          /* the fight left under it */
        g_on = 0;
        g_shot = 0;
    }
    if (!g_on) {
        if ((open_hit || pad_hit) && photo_allowed()
            && (GamePaused == 0 || GamePaused == 1))
        {
            g_via_key = 1;
            photo_open(GamePaused == 1);
            g_via_key = 0;
        }
        /* a replay: its FREE CAMERA on or off, F or the pad's Y */
        if (!rp_playing() || CurrentTask != 6)
            g_free = 0;
        if ((key_edge(&k_live, key(PK_PH_PLAY)) | key_edge(&k_y, (pb & (1u << PB_NORTH)) != 0))
            && rp_playing() && !g_on)
            photo_free_toggle();
        /* keep the edges current while it is shut */
        key_edge(&k_pause, key(PK_PAUSE));
        key_edge(&k_start, (pb & (1u << PB_START)) != 0);
        key_edge(&k_b, (pb & (1u << PB_EAST)) != 0);
        return;
    }

    /* ---- out ---- */
    if (open_hit || pad_hit
        || key_edge(&k_pause, key(PK_PAUSE))
        || key_edge(&k_start, (pb & (1u << PB_START)) != 0)
        || key_edge(&k_b, (pb & (1u << PB_EAST)) != 0)) {
        photo_close();
        return;
    }

    if (pad && pad->connected) {
        g_pad_kind = pad->kind;
        if (pb || fabsf(pad->ax[0]) > 0.3f || fabsf(pad->ax[1]) > 0.3f
            || fabsf(pad->ax[2]) > 0.3f || fabsf(pad->ax[3]) > 0.3f)
            g_pad_last = 1;
    }

    /* ---- the panel: categories, rows, values ---- */
    {
        int stick_free = !pad || (fabsf(pad->ax[0]) < 0.3f && fabsf(pad->ax[1]) < 0.3f);
        int pad_up = pad && pad->up && stick_free, pad_dn = pad && pad->down && stick_free;
        int pad_lt = pad && pad->left && stick_free, pad_rt = pad && pad->right && stick_free;
        int kb_any = key(PK_PH_NAV_UP) || key(PK_PH_NAV_DOWN)
                     || key(PK_PH_NAV_LEFT) || key(PK_PH_NAV_RIGHT)
                     || key(PK_PH_TAB) || key(PK_UP) || key(PK_DOWN)
                     || key(PK_LEFT) || key(PK_RIGHT);

        if (kb_any)
            g_pad_last = 0;
        if (key_edge(&k_tab, key(PK_PH_TAB)) || key_edge(&k_r1, (pb & (1u << PB_R1)) != 0)) {
            g_cat = (g_cat + 1) % CAT_N;
            g_row = 0;
        }
        if (key_edge(&k_l1, (pb & (1u << PB_L1)) != 0)) {
            g_cat = (g_cat + CAT_N - 1) % CAT_N;
            g_row = 0;
        }
        if (g_row >= ROWS(g_cat))
            g_row = 0;
        if (key_repeat(&k_up, key(PK_PH_NAV_UP) || pad_up))
            g_row = (g_row + ROWS(g_cat) - 1) % ROWS(g_cat);
        if (key_repeat(&k_dn, key(PK_PH_NAV_DOWN) || pad_dn))
            g_row = (g_row + 1) % ROWS(g_cat);
        if (key_repeat(&k_lt, key(PK_PH_NAV_LEFT) || pad_lt))
            row_act(-1);
        if (key_repeat(&k_rt, key(PK_PH_NAV_RIGHT) || pad_rt))
            row_act(1);
        if (key_edge(&k_ok, key(PK_OK)) || key_edge(&k_a, (pb & (1u << PB_SOUTH)) != 0))
            row_act(0);
    }
    key_edge(&k_live, key(PK_PH_PLAY));
    if (key_edge(&k_hide, key(PK_PH_HIDE)) || key_edge(&k_y, (pb & (1u << PB_NORTH)) != 0))
        g_panel_hidden ^= 1;
    if (key_edge(&k_shot, key(PK_PH_SHOT)) || key_edge(&k_x, (pb & (1u << PB_WEST)) != 0))
        take_photo();
    if (key_edge(&k_reset, key(PK_PH_RESET)) || key_edge(&k_r3, (pb & (1u << PB_R3)) != 0 && !combo))
        cam_reset();

    /* ---- the camera from the keys and the sticks ---- */
    {
        float fwd = 0.0f, side = 0.0f, rise = 0.0f, turn = 0.0f, tilt = 0.0f, dolly = 0.0f, roll = 0.0f;
        float speed = g_dist * 0.9f * dt;

        if (key(PK_UP))    fwd += 1.0f;
        if (key(PK_DOWN))  fwd -= 1.0f;
        if (key(PK_RIGHT)) side += 1.0f;
        if (key(PK_LEFT))  side -= 1.0f;
        if (key(PK_PH_RISE)) rise += 1.0f;
        if (key(PK_PH_SINK)) rise -= 1.0f;
        if (key(PK_PH_ROLL_L)) roll -= 1.0f;
        if (key(PK_PH_ROLL_R)) roll += 1.0f;
        if (pad && pad->connected) {
            fwd  += dead(pad->ax[1]);
            side += dead(pad->ax[0]);
            turn += dead(pad->ax[2]);
            tilt += dead(pad->ax[3]);
            if (pb & (1u << PB_R2)) dolly -= 1.0f;  /* in */
            if (pb & (1u << PB_L2)) dolly += 1.0f;  /* out */
        }
        if (fwd != 0.0f || side != 0.0f || rise != 0.0f) {
            /* along the ground, as the camera faces */
            float fx = -cosf(g_yaw), fy = -sinf(g_yaw);
            g_tgt[0] += (fx * fwd + fy * side) * speed;
            g_tgt[1] += (fy * fwd - fx * side) * speed;
            g_tgt[2] += rise * speed;
            g_focus = 0;
        }
        g_yaw   -= turn * RAD(100.0f) * dt;
        g_pitch -= tilt * RAD(70.0f) * dt;
        g_dist  *= powf(1.8f, dolly * dt);
        g_roll  += roll * RAD(30.0f) * dt;
    }

    /* ---- the mouse: the panel, then drags and the wheel over the view ---- */
    {
        int mx, my, btn = plat_mouse_buttons(), wheel = plat_mouse_wheel();
        int left = btn & 1, right = btn & 2, mid = btn & 4;
        PANEL p = panel_geom();
        float px = PortMouseX, py = PortMouseY;
        int over_panel = !g_panel_hidden && px >= p.x && px < p.x + p.w
                         && py >= p.y && py < p.y + panel_h(&p);

        plat_mouse(&mx, &my);
        if (btn || wheel)
            g_pad_last = 0;
        if (left && !(mouse_was & 1) && over_panel) {
            if (py < p.y + p.tab_h) {
                int c = (int)((px - p.x) / (p.w / (float)CAT_N));
                if (c >= 0 && c < CAT_N) { g_cat = c; g_row = 0; }
            } else if (py >= p.y + p.tab_h + p.hdr) {
                int r = (int)((py - (p.y + p.tab_h + p.hdr)) / p.row_h);
                if (r >= 0 && r < ROWS(g_cat)) {
                    char lb[64], va[64];
                    g_row = r;
                    row_text(g_cat, r, lb, va, sizeof lb);
                    if (!va[0])
                        row_act(0);
                    else if (px > p.x + p.w * 0.55f)
                        row_act(px > p.x + p.w * 0.78f ? 1 : -1);
                }
            }
        } else if (!(mouse_was & 7) && (btn & 7) && !over_panel) {
            drag = (right || left) ? 1 : mid ? 2 : 0;
            mx0 = mx;
            my0 = my;
        }
        if (!(btn & 7))
            drag = 0;
        if (drag && (mx != mx0 || my != my0)) {
            float dx = (float)(mx - mx0), dy = (float)(my - my0);
            if (drag == 1) {
                g_yaw   -= RAD(0.3f) * dx;
                g_pitch += RAD(0.3f) * dy;
            } else {
                float k = g_dist * 0.0015f;
                float rx = -sinf(g_yaw), ry = cosf(g_yaw);  /* the camera's right */
                g_tgt[0] -= rx * dx * k;
                g_tgt[1] -= ry * dx * k;
                g_tgt[2] += dy * k;
                g_focus = 0;
            }
            mx0 = mx;
            my0 = my;
        }
        if (wheel)
            g_dist *= powf(0.9f, (float)wheel);
        mouse_was = btn;
    }
    cam_limit();
}

/* A replay's FREE CAMERA, every tick while it is on (game_main.c, with the
 * replay's own keys): the arrows move it along the ground, Q / E up and
 * down, the right mouse button turns it, the middle one moves it, the wheel
 * brings it in or out, R puts it back; the pad's left stick moves it, the
 * right one turns it, LT / RT the distance, R3 back. The replay's panel and
 * keys go on as they were (A / D and the d-pad seek, Space / A play). */
void photo_free_tick(const PLAT_PAD *pad, int (*key)(int))
{
    static KEY k_reset, k_r3;
    static int mouse_was, mx0, my0, drag;
    const float dt = 1.0f / 60.0f;
    unsigned pb = pad && pad->connected ? pad->buttons : 0;
    float fwd = 0.0f, side = 0.0f, rise = 0.0f, turn = 0.0f, tilt = 0.0f, dolly = 0.0f;
    float speed;

    if (!photo_free_on())
        return;
    if (pad && pad->connected) {
        g_pad_kind = pad->kind;
        if (pb || fabsf(pad->ax[0]) > 0.3f || fabsf(pad->ax[1]) > 0.3f
            || fabsf(pad->ax[2]) > 0.3f || fabsf(pad->ax[3]) > 0.3f)
            g_pad_last = 1;
    }
    if (key_edge(&k_reset, key(PK_PH_RESET)) | key_edge(&k_r3, (pb & (1u << PB_R3)) != 0))
        cam_from_scene();
    speed = g_dist * 0.9f * dt;
    if (key(PK_PH_NAV_UP))    fwd += 1.0f;
    if (key(PK_PH_NAV_DOWN))  fwd -= 1.0f;
    if (key(PK_PH_NAV_RIGHT)) side += 1.0f;
    if (key(PK_PH_NAV_LEFT))  side -= 1.0f;
    if (key(PK_PH_RISE)) rise += 1.0f;
    if (key(PK_PH_SINK)) rise -= 1.0f;
    if (fwd != 0.0f || side != 0.0f || rise != 0.0f)
        g_pad_last = 0;
    if (pad && pad->connected) {
        fwd  += dead(pad->ax[1]);
        side += dead(pad->ax[0]);
        turn += dead(pad->ax[2]);
        tilt += dead(pad->ax[3]);
        if (pb & (1u << PB_R2)) dolly -= 1.0f;
        if (pb & (1u << PB_L2)) dolly += 1.0f;
    }
    if (fwd != 0.0f || side != 0.0f || rise != 0.0f) {
        float fx = -cosf(g_yaw), fy = -sinf(g_yaw);
        g_tgt[0] += (fx * fwd + fy * side) * speed;
        g_tgt[1] += (fy * fwd - fx * side) * speed;
        g_tgt[2] += rise * speed;
    }
    g_yaw   -= turn * RAD(100.0f) * dt;
    g_pitch -= tilt * RAD(70.0f) * dt;
    g_dist  *= powf(1.8f, dolly * dt);
    {
        int mx, my, btn = plat_mouse_buttons(), wheel = plat_mouse_wheel();
        plat_mouse(&mx, &my);
        if ((btn & 6) || wheel)
            g_pad_last = 0;
        if (!(mouse_was & 6) && (btn & 6)) {    /* the left button is the panel's */
            drag = (btn & 2) ? 1 : 2;
            mx0 = mx;
            my0 = my;
        }
        if (!(btn & 6))
            drag = 0;
        if (drag && (mx != mx0 || my != my0)) {
            float dx = (float)(mx - mx0), dy = (float)(my - my0);
            if (drag == 1) {
                g_yaw   -= RAD(0.3f) * dx;
                g_pitch += RAD(0.3f) * dy;
            } else {
                float k = g_dist * 0.0015f, rx = -sinf(g_yaw), ry = cosf(g_yaw);
                g_tgt[0] -= rx * dx * k;
                g_tgt[1] -= ry * dx * k;
                g_tgt[2] += dy * k;
            }
            mx0 = mx;
            my0 = my;
        }
        if (wheel)
            g_dist *= powf(0.9f, (float)wheel);
        mouse_was = btn;
    }
    cam_limit();
}

/* ------------------------------------------------------------ drawing */

static const float C_BONE[3]   = { 0.93f, 0.90f, 0.85f };
static const float C_ASH[3]    = { 0.66f, 0.62f, 0.57f };
static const float C_BRONZE[3] = { 0.85f, 0.60f, 0.29f };

static void text(const char *s, float x, float y, int align, float scale, const float *rgb)
{
    PortPhotoText(s, x, y, align, scale, rgb);
}

static void hint_line(float W, float H, float s)
{
    char line[400];
    int ps = g_pad_kind == 2 || g_pad_kind == 4;
    float bh = 11.0f * s, y = H - bh;
    float grad[16] = { 0.04f, 0.035f, 0.03f, 0.70f,  0.04f, 0.035f, 0.03f, 0.70f,
                       0.04f, 0.035f, 0.03f, 0.95f,  0.04f, 0.035f, 0.03f, 0.95f };

    if (g_pad_last)
        snprintf(line, sizeof line, "%s",
                 ps ? tx("STICK DER. GIRAR   STICK IZQ. MOVER   L2/R2 DISTANCIA   L1/R1 CATEGORIA   TRIANGULO PANEL   CUADRADO FOTO   CIRCULO SALIR",
                         "R STICK ORBIT   L STICK MOVE   L2/R2 DISTANCE   L1/R1 CATEGORY   TRIANGLE PANEL   SQUARE PHOTO   CIRCLE EXIT")
                    : tx("STICK DER. GIRAR   STICK IZQ. MOVER   LT/RT DISTANCIA   LB/RB CATEGORIA   Y PANEL   X FOTO   B SALIR",
                         "RS ORBIT   LS MOVE   LT/RT DISTANCE   LB/RB CATEGORY   Y PANEL   X PHOTO   B EXIT"));
    else {
        char k_photo[16], k_shot[16], k_hide[16], k_tab[16];
        plat_key_label(PK_PHOTO, k_photo, sizeof k_photo);
        plat_key_label(PK_PH_SHOT, k_shot, sizeof k_shot);
        plat_key_label(PK_PH_HIDE, k_hide, sizeof k_hide);
        plat_key_label(PK_PH_TAB, k_tab, sizeof k_tab);
        snprintf(line, sizeof line,
                 es() ? "RATON DER. GIRAR   CENTRAL MOVER   RUEDA DISTANCIA   WASD/Q/E MOVER   %s CATEGORIA   %s PANEL   %s FOTO   %s SALIR"
                      : "RIGHT MOUSE ORBIT   MIDDLE MOVE   WHEEL DISTANCE   WASD/Q/E MOVE   %s CATEGORY   %s PANEL   %s PHOTO   %s EXIT",
                 k_tab, k_hide, k_shot, k_photo);
    }
    limeFillRectGrad(0.0f, y, W, bh, grad);
    limeFillRect(0.0f, y, W, 1.0f * s, 0.23f, 0.20f, 0.16f, 1.0f);
    text(line, W * 0.5f, y + 3.0f * s, 1, 0.20f, C_BONE);
}

void photo_draw(void)
{
    float W = (float)limeScreenWidth, H = (float)limeScreenHeight, s = FE_HeightScale;
    PANEL p = panel_geom();
    int c, r;

    if (!photo_on()) {
        if (photo_free_on()) {          /* the FREE CAMERA's mark and keys, top centre */
            int ps = g_pad_kind == 2 || g_pad_kind == 4;
            const char *keys = g_pad_last
                ? (ps ? tx("STICK DER. GIRAR   STICK IZQ. MOVER   L2/R2 DISTANCIA   R3 CENTRAR   TRIANGULO SALIR",
                           "R STICK ORBIT   L STICK MOVE   L2/R2 DISTANCE   R3 RESET   TRIANGLE EXIT")
                      : tx("STICK DER. GIRAR   STICK IZQ. MOVER   LT/RT DISTANCIA   R3 CENTRAR   Y SALIR",
                           "RS ORBIT   LS MOVE   LT/RT DISTANCE   R3 RESET   Y EXIT"))
                : tx("RATON DER. GIRAR   CENTRAL MOVER   RUEDA DISTANCIA   FLECHAS / Q / E MOVER   R CENTRAR   F SALIR",
                     "RIGHT MOUSE ORBIT   MIDDLE MOVE   WHEEL DISTANCE   ARROWS / Q / E MOVE   R RESET   F EXIT");
            float bw = 260.0f * s, bx = W * 0.5f - bw * 0.5f, by = 58.0f * s;   /* under the bars and the clock */
            limeEnableAlphaBlending_Basic();
            limeSet2DDrawing();
            limeDisableDepthTest();
            limeFillRect(bx, by, bw, 20.0f * s, 0.055f, 0.05f, 0.045f, 0.72f);
            limeFillRect(bx, by, bw, 1.0f * s, 0.85f, 0.60f, 0.29f, 1.0f);
            limeFillRect(W * 0.5f - 34.0f * s, by + 3.5f * s, 4.0f * s, 4.0f * s, 0.35f, 0.80f, 0.40f, 1.0f);
            text(tx("CAMARA LIBRE", "FREE CAMERA"), W * 0.5f - 27.0f * s, by + 2.5f * s, 0, 0.24f, C_BRONZE);
            text(keys, W * 0.5f, by + 11.5f * s, 1, 0.17f, C_BONE);
        }
        return;
    }
    if (g_shot) {                       /* the photo's frame: nothing over it */
        g_shot_drawn = 1;
        return;
    }
    limeEnableAlphaBlending_Basic();
    limeSet2DDrawing();
    limeDisableDepthTest();

    if (g_grid) {
        for (c = 1; c < 3; c++) {
            limeFillRect(W * (float)c / 3.0f, 0.0f, 1.0f, H, 0.93f, 0.90f, 0.85f, 0.28f);
            limeFillRect(0.0f, H * (float)c / 3.0f, W, 1.0f, 0.93f, 0.90f, 0.85f, 0.28f);
        }
    }

    /* the frozen mark, top right */
    {
        const char *m = tx("ESCENA CONGELADA", "SCENE FROZEN");
        float bw = 70.0f * s, bx = W - bw - 6.0f * s, by = 6.0f * s;
        limeFillRect(bx, by, bw, 10.0f * s, 0.055f, 0.05f, 0.045f, 0.75f);
        limeFillRect(bx + 3.5f * s, by + 3.0f * s, 4.0f * s, 4.0f * s, 0.70f, 0.15f, 0.12f, 1.0f);
        text(m, bx + 10.0f * s, by + 2.5f * s, 0, 0.20f, C_BONE);
    }

    if (!g_panel_hidden) {
        float h = panel_h(&p), tw = p.w / (float)CAT_N, y;
        float plate[16] = { 0.13f, 0.12f, 0.10f, 0.94f,  0.13f, 0.12f, 0.10f, 0.94f,
                            0.08f, 0.07f, 0.06f, 0.94f,  0.08f, 0.07f, 0.06f, 0.94f };

        /* the plate: aged metal, a darker rim, rivets in the corners */
        limeFillRect(p.x - 1.0f * s, p.y - 1.0f * s, p.w + 2.0f * s, h + 2.0f * s, 0.29f, 0.25f, 0.21f, 1.0f);
        limeFillRectGrad(p.x, p.y, p.w, h, plate);
        limeFillRect(p.x + 2.0f * s, p.y + 2.0f * s, 2.0f * s, 2.0f * s, 0.50f, 0.44f, 0.36f, 1.0f);
        limeFillRect(p.x + p.w - 4.0f * s, p.y + 2.0f * s, 2.0f * s, 2.0f * s, 0.50f, 0.44f, 0.36f, 1.0f);
        limeFillRect(p.x + 2.0f * s, p.y + h - 4.0f * s, 2.0f * s, 2.0f * s, 0.50f, 0.44f, 0.36f, 1.0f);
        limeFillRect(p.x + p.w - 4.0f * s, p.y + h - 4.0f * s, 2.0f * s, 2.0f * s, 0.50f, 0.44f, 0.36f, 1.0f);

        /* the categories */
        for (c = 0; c < CAT_N; c++) {
            float x = p.x + tw * (float)c;
            if (c == g_cat) {
                limeFillRect(x, p.y, tw, p.tab_h, 0.85f, 0.60f, 0.29f, 0.16f);
                limeFillRect(x + 3.0f * s, p.y + p.tab_h - 1.5f * s, tw - 6.0f * s, 1.5f * s,
                             0.85f, 0.60f, 0.29f, 1.0f);
            }
            text(cat_name(c), x + tw * 0.5f, p.y + 3.5f * s, 1, 0.21f,
                 c == g_cat ? C_BRONZE : C_ASH);
        }
        limeFillRect(p.x, p.y + p.tab_h, p.w, 1.0f * s, 0.23f, 0.20f, 0.16f, 1.0f);

        /* the heading */
        y = p.y + p.tab_h;
        text(cat_name(g_cat), p.x + 6.0f * s, y + 3.5f * s, 0, 0.27f, C_BONE);
        y += p.hdr;

        /* the rows */
        for (r = 0; r < ROWS(g_cat); r++, y += p.row_h) {
            char lb[64], va[64];
            row_text(g_cat, r, lb, va, sizeof lb);
            if (r == g_row) {
                float sel[16] = { 0.85f, 0.60f, 0.29f, 0.30f,  0.85f, 0.60f, 0.29f, 0.0f,
                                  0.85f, 0.60f, 0.29f, 0.0f,   0.85f, 0.60f, 0.29f, 0.30f };
                limeFillRectGrad(p.x + 2.0f * s, y, p.w - 4.0f * s, p.row_h, sel);
                limeFillRect(p.x + 2.0f * s, y, 1.5f * s, p.row_h, 0.85f, 0.60f, 0.29f, 1.0f);
            }
            text(lb, p.x + 6.0f * s, y + 2.7f * s, 0, 0.20f, r == g_row ? C_BONE : C_ASH);
            if (va[0]) {
                char v[80];
                snprintf(v, sizeof v, r == g_row ? "< %s >" : "%s", va);
                text(v, p.x + p.w - 6.0f * s, y + 2.7f * s, 2, 0.20f, C_BRONZE);
            }
        }
    }

    if (g_toast_t > 0) {
        float a = g_toast_t > 30 ? 1.0f : (float)g_toast_t / 30.0f, tw = 220.0f * s;
        float col[3] = { C_BONE[0] * a, C_BONE[1] * a, C_BONE[2] * a };
        limeFillRect(W * 0.5f - tw * 0.5f, H - 26.0f * s, tw, 11.0f * s, 0.055f, 0.05f, 0.045f, 0.85f * a);
        text(g_toast, W * 0.5f, H - 23.5f * s, 1, 0.20f, col);
    }
    if (!g_panel_hidden)
        hint_line(W, H, s);
}

/* ------------------------------------------------------- the PNG writer
 *
 * The repository reads PNGs (lime/png.c) but never wrote one, so this is a
 * small writer of its own: RGB, eight bits, each row's filter chosen by the
 * least sum of its bytes (the usual heuristic), and a deflate stream of
 * fixed Huffman codes with an LZ77 match search over a hash of three bytes.
 * No dependency. */

typedef struct { unsigned char *b; size_t n, cap; unsigned bits; int nb; } BITS;

static int bits_grow(BITS *o, size_t more)
{
    if (o->n + more <= o->cap)
        return 1;
    {
        size_t cap = o->cap ? o->cap * 2 : 1 << 20;
        unsigned char *nb;
        while (cap < o->n + more)
            cap *= 2;
        nb = (unsigned char *)realloc(o->b, cap);
        if (!nb)
            return 0;
        o->b = nb;
        o->cap = cap;
    }
    return 1;
}

static void put_bits(BITS *o, unsigned v, int n)    /* least significant first */
{
    o->bits |= v << o->nb;
    o->nb += n;
    while (o->nb >= 8) {
        if (bits_grow(o, 1))
            o->b[o->n++] = (unsigned char)o->bits;
        o->bits >>= 8;
        o->nb -= 8;
    }
}

static void put_code(BITS *o, unsigned code, int len) /* a Huffman code, its top bit first */
{
    unsigned r = 0;
    int i;
    for (i = 0; i < len; i++)
        r |= ((code >> i) & 1u) << (len - 1 - i);
    put_bits(o, r, len);
}

static void put_lit(BITS *o, int v)
{
    if (v < 144)      put_code(o, 0x30u + (unsigned)v, 8);
    else if (v < 256) put_code(o, 0x190u + (unsigned)(v - 144), 9);
    else if (v < 280) put_code(o, (unsigned)(v - 256), 7);
    else              put_code(o, 0xc0u + (unsigned)(v - 280), 8);
}

static const unsigned short LBASE[29] = { 3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258 };
static const unsigned char  LEXT[29]  = { 0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0 };
static const unsigned short DBASE[30] = { 1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577 };
static const unsigned char  DEXT[30]  = { 0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13 };

static void put_match(BITS *o, int len, int dist)
{
    int i = 28, j = 29;
    while (LBASE[i] > len) i--;
    put_lit(o, 257 + i);
    put_bits(o, (unsigned)(len - LBASE[i]), LEXT[i]);
    while (DBASE[j] > dist) j--;
    put_code(o, (unsigned)j, 5);
    put_bits(o, (unsigned)(dist - DBASE[j]), DEXT[j]);
}

#define HBITS 15
#define WIN   32768
#define CHAIN 24

static int deflate_fixed(const unsigned char *in, size_t n, BITS *o)
{
    int *head = (int *)malloc(sizeof(int) << HBITS);
    int *prev = (int *)malloc(sizeof(int) * WIN);
    size_t i = 0;

    if (!head || !prev) {
        free(head);
        free(prev);
        return 0;
    }
    memset(head, 0xff, sizeof(int) << HBITS);
    put_bits(o, 1, 1);                  /* the last block */
    put_bits(o, 1, 2);                  /* fixed codes */
    while (i < n) {
        int best = 0, bdist = 0;
        if (i + 3 <= n) {
            unsigned h = ((in[i] << 10) ^ (in[i + 1] << 5) ^ in[i + 2]) & ((1u << HBITS) - 1);
            int cand = head[h], chain = CHAIN;
            size_t maxl = n - i < 258 ? n - i : 258;
            while (cand >= 0 && chain-- > 0 && i - (size_t)cand <= WIN - 1) {
                const unsigned char *a = in + cand, *b = in + i;
                size_t l = 0;
                while (l < maxl && a[l] == b[l])
                    l++;
                if ((int)l > best) {
                    best = (int)l;
                    bdist = (int)(i - (size_t)cand);
                    if (l == maxl)
                        break;
                }
                cand = prev[cand & (WIN - 1)];
            }
            prev[i & (WIN - 1)] = head[h];
            head[h] = (int)i;
        }
        if (best >= 3) {
            size_t k;
            put_match(o, best, bdist);
            for (k = 1; k < (size_t)best; k++) {   /* the matched bytes into the hash too */
                size_t q = i + k;
                if (q + 3 <= n) {
                    unsigned h = ((in[q] << 10) ^ (in[q + 1] << 5) ^ in[q + 2]) & ((1u << HBITS) - 1);
                    prev[q & (WIN - 1)] = head[h];
                    head[h] = (int)q;
                }
            }
            i += (size_t)best;
        } else {
            put_lit(o, in[i]);
            i++;
        }
    }
    put_lit(o, 256);
    if (o->nb)
        put_bits(o, 0, 8 - o->nb);
    free(head);
    free(prev);
    return 1;
}

static unsigned long g_crc[256];

static unsigned long crc(unsigned long c, const unsigned char *p, size_t n)
{
    size_t i;
    if (!g_crc[1]) {
        unsigned long k, v;
        int b;
        for (k = 0; k < 256; k++) {
            v = k;
            for (b = 0; b < 8; b++)
                v = v & 1 ? 0xedb88320ul ^ (v >> 1) : v >> 1;
            g_crc[k] = v;
        }
    }
    for (i = 0; i < n; i++)
        c = g_crc[(c ^ p[i]) & 0xff] ^ (c >> 8);
    return c;
}

static void be32(unsigned char *p, unsigned long v)
{
    p[0] = (unsigned char)(v >> 24); p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);  p[3] = (unsigned char)v;
}

static void chunk(FILE *f, const char *type, const unsigned char *d, size_t n)
{
    unsigned char h[8], t[4];
    unsigned long c;
    be32(h, (unsigned long)n);
    memcpy(h + 4, type, 4);
    fwrite(h, 1, 8, f);
    if (n)
        fwrite(d, 1, n, f);
    c = crc(0xfffffffful, (const unsigned char *)type, 4);
    c = crc(c, d, n) ^ 0xfffffffful;
    be32(t, c);
    fwrite(t, 1, 4, f);
}

static int paeth(int a, int b, int c)
{
    int p = a + b - c, pa = abs(p - a), pb = abs(p - b), pc = abs(p - c);
    return pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
}

/* rgb: w x h, three bytes a pixel, rows from the top */
static int png_write(const char *path, const unsigned char *rgb, int w, int h)
{
    size_t row = (size_t)w * 3, n = (size_t)h * (row + 1), y;
    unsigned char *raw = (unsigned char *)malloc(n), *try_ = (unsigned char *)malloc(row * 4);
    BITS z;
    unsigned long a = 1, b = 0;
    unsigned char hdr[13], tail[4];
    FILE *f;
    int ok = 0;

    memset(&z, 0, sizeof z);
    if (!raw || !try_)
        goto out;
    for (y = 0; y < (size_t)h; y++) {
        const unsigned char *cur = rgb + y * row, *up = y ? rgb + (y - 1) * row : NULL;
        long best = -1;
        int ft, bestf = 0;
        for (ft = 0; ft < 4; ft++) {         /* none, sub, up, paeth */
            unsigned char *d = try_ + (size_t)ft * row;
            long sum = 0;
            size_t i;
            for (i = 0; i < row; i++) {
                int L = i >= 3 ? cur[i - 3] : 0, U = up ? up[i] : 0, UL = up && i >= 3 ? up[i - 3] : 0;
                int v = ft == 0 ? cur[i] : ft == 1 ? cur[i] - L : ft == 2 ? cur[i] - U
                        : cur[i] - paeth(L, U, UL);
                d[i] = (unsigned char)v;
                sum += d[i] < 128 ? d[i] : 256 - d[i];
            }
            if (best < 0 || sum < best) {
                best = sum;
                bestf = ft;
            }
        }
        raw[y * (row + 1)] = (unsigned char)(bestf == 3 ? 4 : bestf);
        memcpy(raw + y * (row + 1) + 1, try_ + (size_t)bestf * row, row);
    }
    /* zlib: the header, the deflate stream, adler-32 */
    put_bits(&z, 0x78, 8);
    put_bits(&z, 0x01, 8);
    if (!deflate_fixed(raw, n, &z))
        goto out;
    for (y = 0; y < n; y++) {
        a = (a + raw[y]) % 65521ul;
        b = (b + a) % 65521ul;
    }
    be32(tail, (b << 16) | a);
    if (!bits_grow(&z, 4))
        goto out;
    memcpy(z.b + z.n, tail, 4);
    z.n += 4;

    f = fopen(path, "wb");
    if (!f)
        goto out;
    fwrite("\x89PNG\r\n\x1a\n", 1, 8, f);
    be32(hdr, (unsigned long)w);
    be32(hdr + 4, (unsigned long)h);
    hdr[8] = 8;                         /* bits */
    hdr[9] = 2;                         /* RGB */
    hdr[10] = hdr[11] = hdr[12] = 0;
    chunk(f, "IHDR", hdr, 13);
    chunk(f, "IDAT", z.b, z.n);
    chunk(f, "IEND", NULL, 0);
    ok = fclose(f) == 0;
out:
    free(raw);
    free(try_);
    free(z.b);
    return ok;
}

/* ------------------------------------------------------------ the capture */

typedef struct { char path[1200]; int w, h; unsigned char *rgb; } JOB;

static volatile int g_writing;

#ifdef _WIN32
static DWORD WINAPI write_job(LPVOID arg)
#else
static void *write_job(void *arg)
#endif
{
    JOB *j = (JOB *)arg;
    void st_heap_this_thread(void);
    st_heap_this_thread();              /* the pixels are no state (savestate.c) */
    if (png_write(j->path, j->rgb, j->w, j->h))
        printf("photo: wrote %s (%dx%d)\n", j->path, j->w, j->h);
    else
        printf("photo: could not write %s\n", j->path);
    free(j->rgb);
    free(j);
    g_writing--;
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

static void name_part(char *out, size_t n, umk3_long who)
{
    const char *s = who >= 0 && who < 26 && CharacterNames[who] ? CharacterNames[who] : "Fighter";
    size_t i = 0;
    for (; *s && i + 1 < n; s++)
        out[i++] = ((*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z') || (*s >= '0' && *s <= '9'))
                   ? *s : '-';
    out[i] = 0;
}

void photo_after_present(int vx, int vy, int vw, int vh)
{
    JOB *j;
    unsigned char *px;
    char dir[1100], a[40], b[40], file[160];
    time_t now;
    struct tm *t;
    int y;

    if (!g_shot || !g_shot_drawn)
        return;
    g_shot = g_shot_drawn = 0;
    if (vw <= 0 || vh <= 0)
        return;
    px = (unsigned char *)malloc((size_t)vw * vh * 4);
    j = (JOB *)calloc(1, sizeof *j);
    if (!px || !j) {
        free(px);
        free(j);
        return;
    }
    j->rgb = (unsigned char *)malloc((size_t)vw * vh * 3);
    if (!j->rgb) {
        free(px);
        free(j);
        return;
    }
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(vx, vy, vw, vh, GL_RGBA, GL_UNSIGNED_BYTE, px);
    for (y = 0; y < vh; y++) {          /* GL's rows from the bottom; RGB */
        const unsigned char *s = px + (size_t)(vh - 1 - y) * vw * 4;
        unsigned char *d = j->rgb + (size_t)y * vw * 3;
        int x;
        for (x = 0; x < vw; x++, s += 4, d += 3) {
            d[0] = s[0];
            d[1] = s[1];
            d[2] = s[2];
        }
    }
    free(px);
    j->w = vw;
    j->h = vh;

    photo_folder(dir, sizeof dir);
#ifdef _WIN32
    CreateDirectoryA(dir, NULL);
#else
    mkdir(dir, 0755);
#endif
    now = time(NULL);
    t = localtime(&now);
    name_part(a, sizeof a, Character1);
    name_part(b, sizeof b, Character2);
    snprintf(file, sizeof file, "UMK3_%04d-%02d-%02d_%02d-%02d-%02d_%s_vs_%s.png",
             t ? t->tm_year + 1900 : 0, t ? t->tm_mon + 1 : 0, t ? t->tm_mday : 0,
             t ? t->tm_hour : 0, t ? t->tm_min : 0, t ? t->tm_sec : 0, a, b);
    snprintf(j->path, sizeof j->path, "%s/%s", dir, file);
    {
        /* two in the same second: a number after it */
        FILE *f = fopen(j->path, "rb");
        if (f) {
            fclose(f);
            snprintf(j->path, sizeof j->path, "%s/%.*s_%d.png", dir,
                     (int)(strlen(file) - 4), file, ++g_shots_saved);
        }
    }
    snprintf(g_toast, sizeof g_toast, "%s  photos/%s",
             tx("FOTO GUARDADA", "PHOTO SAVED"), strrchr(j->path, '/') + 1);
    g_toast_t = 240;
    printf("photo: capture %dx%d -> %s\n", vw, vh, j->path);

    g_writing++;
#ifdef _WIN32
    {
        HANDLE th = CreateThread(NULL, 1 << 20, write_job, j, 0, NULL);
        if (th)
            CloseHandle(th);
        else
            write_job(j);
    }
#else
    {
        pthread_t th;
        if (pthread_create(&th, NULL, write_job, j) == 0)
            pthread_detach(th);
        else
            write_job(j);
    }
#endif
}
