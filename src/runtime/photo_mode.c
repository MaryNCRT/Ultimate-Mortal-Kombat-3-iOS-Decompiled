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

enum { CAT_CAMERA, CAT_IMAGE, CAT_EFFECTS, CAT_CHAR, CAT_SCENE, CAT_CAPTURE, CAT_N };
static const int g_rows_base[CAT_N] = { 7, 11, 9, 7, 6, 3 };

/* CHARACTER (phase 3): each fighter moved and turned for the picture, or
 * hidden -- drawn so, his state as it was (GameCode.c
 * RenderLevelPlayers_Pose); all of it put back when the mode leaves */
static struct { float dx, dy, dz, rot; int hidden; } g_fig[2];
static int g_fig_sel;                   /* the fighter the page is about */
/* SCENE: the stage, the effects hidden; the fight stepped a frame at a
 * time, the frame it froze on put back on the way out (a save state) */
static int g_hide_stage, g_hide_fx;
static int g_stepping, g_stepped, g_step_saved;

/* IMAGE and EFFECTS (phase 2): the settings of the pass over the picture
 * (render/rgl.h rgl_post_apply, the shaders' post_fx) */
typedef struct {
    float ev, bri, con, sat, temp, tint, gamma, sharp, hi, sh;  /* IMAGE */
    float vig, grain, ca, bloom, focus, aperture;               /* EFFECTS */
    int   filter, dof;
} FX;
static const FX k_fx0 = { 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
                          0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 2.0f, 0, 0 };
static FX  g_fx = { 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
                    0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 2.0f, 0, 0 };
static int g_fx_depth;                  /* the last pass had the scene's depth */
#define FILTERS 9
static int fx_dof_possible(void);
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
    return g_on && CurrentTask == 6 && (GamePaused == PHOTO_PAUSED || g_stepping);
}

/* GameCode.c's questions: a fighter's adjustment, a side hidden, the stage
 * and the effects hidden -- only while the mode (or the FREE CAMERA) is up */
int photo_fighter_adjust(int slot, float a[4])
{
    if (slot < 0 || slot > 1 || !(photo_on() || photo_free_on()))
        return 0;
    a[0] = g_fig[slot].dx;
    a[1] = g_fig[slot].dy;
    a[2] = g_fig[slot].dz;
    a[3] = g_fig[slot].rot;
    return a[0] != 0.0f || a[1] != 0.0f || a[2] != 0.0f || a[3] != 0.0f;
}

int photo_hide_side(long side)
{
    return (side == 0 || side == 1) && (photo_on() || photo_free_on()) && g_fig[side].hidden;
}

int photo_hide_stage(void)
{
    return (photo_on() || photo_free_on()) && g_hide_stage;
}

int photo_hide_effects(void)
{
    return (photo_on() || photo_free_on()) && g_hide_fx;
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
    if (g_stepped && g_step_saved) {    /* the frame it froze on, as it was */
        if (st_photo_load() == 0) {
            plat_audio_stop_all();
            printf("photo: the fight put back on the frame it was frozen on\n");
        }
    }
    st_photo_forget();
    g_stepping = g_stepped = g_step_saved = 0;
    memset(g_fig, 0, sizeof g_fig);
    g_hide_stage = g_hide_fx = 0;
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
    static const char *const n_es[CAT_N] = { "CAMARA", "IMAGEN", "EFECTOS", "PERSONAJE", "ESCENA", "CAPTURA" };
    static const char *const n_en[CAT_N] = { "CAMERA", "IMAGE", "EFFECTS", "CHARACTER", "SCENE", "CAPTURE" };
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
    case CAT_IMAGE * 16 + 0: snprintf(label, n, "%s", tx("EXPOSICION", "EXPOSURE"));
        snprintf(value, n, "%+.1f EV", g_fx.ev); break;
    case CAT_IMAGE * 16 + 1: snprintf(label, n, "%s", tx("BRILLO", "BRIGHTNESS"));
        snprintf(value, n, "%+.0f", g_fx.bri * 100.0f); break;
    case CAT_IMAGE * 16 + 2: snprintf(label, n, "%s", tx("CONTRASTE", "CONTRAST"));
        snprintf(value, n, "%.0f%%", g_fx.con * 100.0f); break;
    case CAT_IMAGE * 16 + 3: snprintf(label, n, "%s", tx("SATURACION", "SATURATION"));
        snprintf(value, n, "%.0f%%", g_fx.sat * 100.0f); break;
    case CAT_IMAGE * 16 + 4: snprintf(label, n, "%s", tx("TEMPERATURA", "TEMPERATURE"));
        snprintf(value, n, "%+.0f", g_fx.temp * 100.0f); break;
    case CAT_IMAGE * 16 + 5: snprintf(label, n, "%s", tx("TINTE", "TINT"));
        snprintf(value, n, "%+.0f", g_fx.tint * 100.0f); break;
    case CAT_IMAGE * 16 + 6: snprintf(label, n, "%s", "GAMMA");
        snprintf(value, n, "%.2f", g_fx.gamma); break;
    case CAT_IMAGE * 16 + 7: snprintf(label, n, "%s", tx("NITIDEZ", "SHARPNESS"));
        snprintf(value, n, "%.0f%%", g_fx.sharp * 50.0f); break;
    case CAT_IMAGE * 16 + 8: snprintf(label, n, "%s", tx("LUCES", "HIGHLIGHTS"));
        snprintf(value, n, "%+.0f", g_fx.hi * 100.0f); break;
    case CAT_IMAGE * 16 + 9: snprintf(label, n, "%s", tx("SOMBRAS", "SHADOWS"));
        snprintf(value, n, "%+.0f", g_fx.sh * 100.0f); break;
    case CAT_IMAGE * 16 + 10: snprintf(label, n, "%s", tx("RESTABLECER IMAGEN", "RESET IMAGE")); break;
    case CAT_EFFECTS * 16 + 0: snprintf(label, n, "%s", tx("PROFUNDIDAD DE CAMPO", "DEPTH OF FIELD"));
        if (!fx_dof_possible())
            snprintf(value, n, "%s", tx("NECESITA ILUM. MEJORADA", "NEEDS ENH. LIGHTING"));
        else if (g_fx.dof && !g_fx_depth)
            snprintf(value, n, "%s", tx("SIN PROFUNDIDAD", "NO DEPTH"));
        else
            snprintf(value, n, "%s", g_fx.dof ? tx("SI", "ON") : tx("NO", "OFF"));
        break;
    case CAT_EFFECTS * 16 + 1: snprintf(label, n, "%s", tx("DISTANCIA DE FOCO", "FOCUS DISTANCE"));
        snprintf(value, n, "%.0f%%", g_fx.focus * 100.0f); break;
    case CAT_EFFECTS * 16 + 2: snprintf(label, n, "%s", tx("APERTURA", "APERTURE"));
        snprintf(value, n, "F/%.1f", 5.6f / g_fx.aperture); break;
    case CAT_EFFECTS * 16 + 3: snprintf(label, n, "%s", "BLOOM");
        snprintf(value, n, "%.0f%%", g_fx.bloom * 100.0f); break;
    case CAT_EFFECTS * 16 + 4: snprintf(label, n, "%s", tx("VINETA", "VIGNETTE"));
        snprintf(value, n, "%.0f%%", g_fx.vig * 100.0f); break;
    case CAT_EFFECTS * 16 + 5: snprintf(label, n, "%s", tx("ABERRACION CROMATICA", "CHROMATIC ABERRATION"));
        snprintf(value, n, "%.0f%%", g_fx.ca * 100.0f); break;
    case CAT_EFFECTS * 16 + 6: snprintf(label, n, "%s", tx("GRANO", "FILM GRAIN"));
        snprintf(value, n, "%.0f%%", g_fx.grain * 100.0f); break;
    case CAT_EFFECTS * 16 + 7: {
        static const char *const f_es[FILTERS] = { "NINGUNO", "BLANCO Y NEGRO", "SEPIA", "FRIO", "CALIDO",
                                                   "SANGRE", "OUTWORLD", "NOIR", "ANTIGUO" };
        static const char *const f_en[FILTERS] = { "NONE", "BLACK AND WHITE", "SEPIA", "COLD", "WARM",
                                                   "BLOOD", "OUTWORLD", "NOIR", "VINTAGE" };
        snprintf(label, n, "%s", tx("FILTRO", "FILTER"));
        snprintf(value, n, "%s", es() ? f_es[g_fx.filter] : f_en[g_fx.filter]);
        break;
    }
    case CAT_EFFECTS * 16 + 8: snprintf(label, n, "%s", tx("RESTABLECER EFECTOS", "RESET EFFECTS")); break;
    case CAT_SCENE * 16 + 0:
        snprintf(label, n, "%s", "HUD");
        snprintf(value, n, "%s", g_show_hud ? tx("VISIBLE", "SHOWN") : tx("OCULTO", "HIDDEN"));
        break;
    case CAT_SCENE * 16 + 1:
        snprintf(label, n, "%s", tx("CUADRICULA", "GRID"));
        snprintf(value, n, "%s", g_grid ? tx("SI", "ON") : tx("NO", "OFF"));
        break;
    case CAT_SCENE * 16 + 2:
        snprintf(label, n, "%s", tx("ESCENARIO", "STAGE"));
        snprintf(value, n, "%s", g_hide_stage ? tx("OCULTO", "HIDDEN") : tx("VISIBLE", "SHOWN"));
        break;
    case CAT_SCENE * 16 + 3:
        snprintf(label, n, "%s", tx("EFECTOS Y SANGRE", "EFFECTS AND BLOOD"));
        snprintf(value, n, "%s", g_hide_fx ? tx("OCULTOS", "HIDDEN") : tx("VISIBLES", "SHOWN"));
        break;
    case CAT_SCENE * 16 + 4:
        snprintf(label, n, "%s", tx("AVANZAR UN CUADRO", "STEP ONE FRAME"));
        if (g_replay)
            snprintf(value, n, "%s", tx("USA EL PANEL", "USE THE PANEL"));
        break;
    case CAT_SCENE * 16 + 5:
        snprintf(label, n, "%s", tx("VOLVER AL CUADRO", "BACK TO THE FRAME"));
        if (!g_stepped)
            snprintf(value, n, "%s", "-");
        break;
    case CAT_CHAR * 16 + 0:
        snprintf(label, n, "%s", tx("LUCHADOR", "FIGHTER"));
        {
            umk3_long c = g_fig_sel ? Character2 : Character1;
            snprintf(value, n, "%s", c >= 0 && c < 26 && CharacterNames[c] ? CharacterNames[c] : "?");
        }
        break;
    case CAT_CHAR * 16 + 1:
        snprintf(label, n, "%s", tx("GIRAR", "TURN"));
        snprintf(value, n, "%+.0f", g_fig[g_fig_sel].rot);
        break;
    case CAT_CHAR * 16 + 2:
        snprintf(label, n, "%s", tx("MOVER A LOS LADOS", "MOVE SIDEWAYS"));
        snprintf(value, n, "%+.2f", g_fig[g_fig_sel].dx);
        break;
    case CAT_CHAR * 16 + 3:
        snprintf(label, n, "%s", tx("MOVER AL FONDO", "MOVE IN DEPTH"));
        snprintf(value, n, "%+.2f", g_fig[g_fig_sel].dy);
        break;
    case CAT_CHAR * 16 + 4:
        snprintf(label, n, "%s", tx("ALTURA", "HEIGHT"));
        snprintf(value, n, "%+.2f", g_fig[g_fig_sel].dz);
        break;
    case CAT_CHAR * 16 + 5:
        snprintf(label, n, "%s", tx("VISIBLE", "SHOWN"));
        snprintf(value, n, "%s", g_fig[g_fig_sel].hidden ? tx("NO", "NO") : tx("SI", "YES"));
        break;
    case CAT_CHAR * 16 + 6:
        snprintf(label, n, "%s", tx("RESTABLECER LUCHADOR", "RESET FIGHTER"));
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
#define STEP(v, st, lo, hi) (v) = clampf((v) + (st) * (float)d, (lo), (hi))
    case CAT_IMAGE * 16 + 0:  STEP(g_fx.ev, 0.1f, -2.0f, 2.0f); break;
    case CAT_IMAGE * 16 + 1:  STEP(g_fx.bri, 0.02f, -0.3f, 0.3f); break;
    case CAT_IMAGE * 16 + 2:  STEP(g_fx.con, 0.05f, 0.5f, 1.8f); break;
    case CAT_IMAGE * 16 + 3:  STEP(g_fx.sat, 0.05f, 0.0f, 2.0f); break;
    case CAT_IMAGE * 16 + 4:  STEP(g_fx.temp, 0.05f, -1.0f, 1.0f); break;
    case CAT_IMAGE * 16 + 5:  STEP(g_fx.tint, 0.05f, -1.0f, 1.0f); break;
    case CAT_IMAGE * 16 + 6:  STEP(g_fx.gamma, 0.05f, 0.6f, 1.6f); break;
    case CAT_IMAGE * 16 + 7:  STEP(g_fx.sharp, 0.1f, 0.0f, 2.0f); break;
    case CAT_IMAGE * 16 + 8:  STEP(g_fx.hi, 0.05f, -1.0f, 1.0f); break;
    case CAT_IMAGE * 16 + 9:  STEP(g_fx.sh, 0.05f, -1.0f, 1.0f); break;
    case CAT_IMAGE * 16 + 10:
        if (!d) {
            g_fx.ev = k_fx0.ev; g_fx.bri = k_fx0.bri; g_fx.con = k_fx0.con; g_fx.sat = k_fx0.sat;
            g_fx.temp = k_fx0.temp; g_fx.tint = k_fx0.tint; g_fx.gamma = k_fx0.gamma;
            g_fx.sharp = k_fx0.sharp; g_fx.hi = k_fx0.hi; g_fx.sh = k_fx0.sh;
        }
        break;
    case CAT_EFFECTS * 16 + 0: if (fx_dof_possible()) g_fx.dof ^= 1; break;
    case CAT_EFFECTS * 16 + 1: STEP(g_fx.focus, 0.05f, 0.25f, 3.0f); break;
    case CAT_EFFECTS * 16 + 2: STEP(g_fx.aperture, 0.25f, 0.5f, 6.0f); break;
    case CAT_EFFECTS * 16 + 3: STEP(g_fx.bloom, 0.05f, 0.0f, 1.0f); break;
    case CAT_EFFECTS * 16 + 4: STEP(g_fx.vig, 0.05f, 0.0f, 1.0f); break;
    case CAT_EFFECTS * 16 + 5: STEP(g_fx.ca, 0.05f, 0.0f, 1.0f); break;
    case CAT_EFFECTS * 16 + 6: STEP(g_fx.grain, 0.05f, 0.0f, 1.0f); break;
    case CAT_EFFECTS * 16 + 7: g_fx.filter = (g_fx.filter + (d < 0 ? FILTERS - 1 : 1)) % FILTERS; break;
    case CAT_EFFECTS * 16 + 8:
        if (!d) {
            int keep = g_fx.dof;
            g_fx.vig = k_fx0.vig; g_fx.grain = k_fx0.grain; g_fx.ca = k_fx0.ca; g_fx.bloom = k_fx0.bloom;
            g_fx.focus = k_fx0.focus; g_fx.aperture = k_fx0.aperture; g_fx.filter = 0;
            g_fx.dof = 0;
            (void)keep;
        }
        break;
#undef STEP
    case CAT_SCENE * 16 + 0:  g_show_hud ^= 1; break;
    case CAT_SCENE * 16 + 1:  g_grid ^= 1; break;
    case CAT_SCENE * 16 + 2:  g_hide_stage ^= 1; break;
    case CAT_SCENE * 16 + 3:  g_hide_fx ^= 1; break;
    case CAT_SCENE * 16 + 4:
        if (!d && !g_replay) {
            /* the first step keeps the frame it froze on (a save state),
             * to put it back on the way out */
            if (!g_stepped)
                g_step_saved = st_available() && st_photo_save() == 0;
            g_stepping = 1;             /* photo_tick lets one tick of the fight run */
        }
        break;
    case CAT_SCENE * 16 + 5:
        if (!d && g_stepped && g_step_saved && st_photo_load() == 0) {
            plat_audio_stop_all();
            GamePaused = PHOTO_PAUSED;
            g_stepped = 0;
            printf("photo: back to the frame it was frozen on\n");
        }
        break;
    case CAT_CHAR * 16 + 0:   g_fig_sel ^= 1; break;
    case CAT_CHAR * 16 + 1:   g_fig[g_fig_sel].rot = clampf(g_fig[g_fig_sel].rot + 15.0f * (float)d, -180.0f, 180.0f); break;
    case CAT_CHAR * 16 + 2:   g_fig[g_fig_sel].dx = clampf(g_fig[g_fig_sel].dx + 0.1f * (float)d, -3.0f, 3.0f); break;
    case CAT_CHAR * 16 + 3:   g_fig[g_fig_sel].dy = clampf(g_fig[g_fig_sel].dy + 0.1f * (float)d, -3.0f, 3.0f); break;
    case CAT_CHAR * 16 + 4:   g_fig[g_fig_sel].dz = clampf(g_fig[g_fig_sel].dz + 0.05f * (float)d, -1.0f, 2.0f); break;
    case CAT_CHAR * 16 + 5:   g_fig[g_fig_sel].hidden ^= 1; break;
    case CAT_CHAR * 16 + 6:   if (!d) memset(&g_fig[g_fig_sel], 0, sizeof g_fig[0]); break;
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
    p.w = 136.0f * s;                   /* Mary: the words a little larger */
    p.tab_h = 14.0f * s;
    p.hdr = 14.0f * s;
    p.row_h = 12.0f * s;
    return p;
}

#define HIDE_H 13.0f                    /* the HIDE MENU button under the rows */

static float panel_h(const PANEL *p)
{
    return p->tab_h + p->hdr + (float)ROWS(g_cat) * p->row_h + (HIDE_H + 9.0f) * FE_HeightScale;
}

/* the HIDE MENU button, and with the menu hidden the small MENU tab that
 * brings it back (Mary: a way to hide it with the mouse too) */
static void hide_btn(const PANEL *p, float *x, float *y, float *w, float *h)
{
    float s = FE_HeightScale;
    if (g_panel_hidden) {
        *x = p->x;
        *y = p->y;
        *w = 58.0f * s;
        *h = 13.0f * s;
    } else {
        *x = p->x + 4.0f * s;
        *y = p->y + panel_h(p) - (HIDE_H + 4.0f) * s;
        *w = p->w - 8.0f * s;
        *h = HIDE_H * s;
    }
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
    /* AVANZAR UN CUADRO: one tick of the fight runs (GamePaused 0 for it),
     * then it is frozen again */
    if (g_on && g_stepping == 2) {
        GamePaused = PHOTO_PAUSED;
        g_stepping = 0;
        g_stepped = 1;
    } else if (g_on && g_stepping == 1 && GamePaused == PHOTO_PAUSED) {
        GamePaused = 0;
        g_stepping = 2;
        return;
    }
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

        static float lpx, lpy;
        int hover_row = -1;

        plat_mouse(&mx, &my);
        if (btn || wheel)
            g_pad_last = 0;
        /* the row under the mouse is the chosen one once the mouse moves
         * over it (Mary: comfortable with the mouse) */
        if (over_panel && py >= p.y + p.tab_h + p.hdr) {
            int r = (int)((py - (p.y + p.tab_h + p.hdr)) / p.row_h);
            if (r >= 0 && r < ROWS(g_cat)) {
                hover_row = r;
                if (px != lpx || py != lpy)
                    g_row = r;
            }
        }
        lpx = px;
        lpy = py;
        if (over_panel && wheel && hover_row >= 0) {   /* the wheel turns the value */
            char lb[64], va[64];
            row_text(g_cat, hover_row, lb, va, sizeof lb);
            if (va[0])
                row_act(wheel > 0 ? 1 : -1);
            wheel = 0;
        }
        {
            float bx, by, bw, bh;
            hide_btn(&p, &bx, &by, &bw, &bh);
            if (left && !(mouse_was & 1) && px >= bx && px < bx + bw && py >= by && py < by + bh) {
                g_panel_hidden ^= 1;
                mouse_was = btn;
                return;
            }
        }
        if (left && !(mouse_was & 1) && over_panel) {
            if (py < p.y + p.tab_h) {   /* the strip: its left half back, its right half on */
                g_cat = (g_cat + (px < p.x + p.w * 0.5f ? CAT_N - 1 : 1)) % CAT_N;
                g_row = 0;
            } else if (hover_row >= 0) {
                char lb[64], va[64];
                g_row = hover_row;
                row_text(g_cat, hover_row, lb, va, sizeof lb);
                if (!va[0])
                    row_act(0);
                else if (px >= p.x + p.w * 0.50f && px < p.x + p.w * 0.64f)
                    row_act(-1);        /* its left arrow */
                else
                    row_act(1);         /* its right arrow, or the row */
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

/* --------------------------------------------------- IMAGE and EFFECTS */

/* the depth of field needs the scene's distance: OpenGL's depth buffer, or
 * Vulkan's and Direct3D 12's depth pass, which comes with ENHANCED LIGHTING */
static int fx_dof_possible(void)
{
    return rgl_post_available() && (rgl_renderer == RGL_OPENGL || rgl_shadows);
}

static int fx_any(void)
{
    FX a = g_fx, b = k_fx0;
    a.focus = b.focus;                  /* the focus and the aperture alone change nothing */
    a.aperture = b.aperture;
    return memcmp(&a, &b, sizeof a) != 0;
}

/* GameCode.c RenderGameView: the scene's projection, for OpenGL's depth */
void photo_note_scene(void)
{
    if ((photo_on() || photo_free_on()) && g_fx.dof)
        rgl_post_scene();
}

/* GameCode.c Task_GameMain_frame, before the panel: the pass, when a
 * setting is not at rest -- the photo's frame included */
void photo_post(void)
{
    float prm[RGL_POST_PARAMS];

    rgl_post_want_depth = 0;
    if (!(photo_on() || photo_free_on()) || !fx_any() || !rgl_post_available())
        return;
    rgl_post_want_depth = g_fx.dof && fx_dof_possible();
    memset(prm, 0, sizeof prm);
    prm[0] = g_fx.ev;    prm[1] = g_fx.bri;   prm[2] = g_fx.con;   prm[3] = g_fx.sat;
    prm[4] = g_fx.temp;  prm[5] = g_fx.tint;  prm[6] = g_fx.gamma; prm[7] = g_fx.sharp;
    prm[8] = g_fx.hi;    prm[9] = g_fx.sh;    prm[10] = g_fx.vig;  prm[11] = g_fx.grain;
    prm[12] = g_fx.ca;   prm[13] = g_fx.bloom; prm[14] = (float)g_fx.filter; prm[15] = 0.37f;
    prm[16] = rgl_post_want_depth ? 1.0f : 0.0f;
    prm[17] = g_fx.focus;
    prm[18] = g_fx.aperture;
    g_fx_depth = rgl_post_apply(prm);
}

/* ------------------------------------------------------------ drawing */

static const float C_BONE[3]   = { 0.93f, 0.90f, 0.85f };
static const float C_ASH[3]    = { 0.66f, 0.62f, 0.57f };
static const float C_BRONZE[3] = { 0.85f, 0.60f, 0.29f };

static void text(const char *s, float x, float y, int align, float scale, const float *rgb)
{
    PortPhotoText(s, x, y, align, scale, rgb);
}

/* The keys' and the pad's pictures in the hints (Mary: the buttons' icons),
 * as the menus' hints draw them: a line of "\001glyphs path\001" pictures
 * and words (FrontEnd.c PortNavDrawLineC). The mouse has no picture: it is
 * named. */
void PortNavDrawLineC(const char *s, float y, float scale);    /* FrontEnd.c */

static char *g_kb(int pk)               /* a key's picture, by its binding */
{
    static char b[16][96];
    static int k;
    char *o = b[k = (k + 1) & 15];
    plat_key_glyph(pk, o, 96);
    return o;
}

static char *g_pb(int pb)               /* a pad button's, for the pad in use */
{
    static char b[16][96];
    static int k;
    char *o = b[k = (k + 1) & 15];
    plat_pad_glyph(plat_pad_glyph_kind(0, g_pad_kind), pb, o, 96);
    return o;
}

/* a part of the pad with no PB_ of its own: "L_2D" "R_2D" (the sticks),
 * "Dpad_X" "Dpad_Y" (the cross across, up and down) */
static char *g_part(const char *part)
{
    static char b[4][96];
    static int k;
    char *o = b[k = (k + 1) & 3];
    int kind = plat_pad_glyph_kind(0, g_pad_kind);
    if (kind == 2)
        snprintf(o, 96, "\001ps/T_P5_%s_Retro\001", part);
    else if (kind == 4)
        snprintf(o, 96, "\001p4/T_P4_%s_Stylized\001", strcmp(part, "L_2D") == 0 ? "l_2D" : part);
    else if (kind == 3)
        snprintf(o, 96, "\001ns/T_S_%s\001", part);
    else
        snprintf(o, 96, "\001xb/T_X_%s_Retro\001", part);
    return o;
}

/* which: 0 PHOTO MODE, 1 a replay's FREE CAMERA, 2 the replay player */
static void hints_text(int which, char *line, int n)
{
    int e = es();

    if (g_pad_last && which == 0)
        snprintf(line, n, e ? "%s GIRAR   %s MOVER   %s%s DISTANCIA   %s%s CATEGORIA   %s PANEL   %s FOTO   %s SALIR"
                            : "%s ORBIT   %s MOVE   %s%s DISTANCE   %s%s CATEGORY   %s PANEL   %s PHOTO   %s EXIT",
                 g_part("R_2D"), g_part("L_2D"), g_pb(PB_L2), g_pb(PB_R2), g_pb(PB_L1), g_pb(PB_R1),
                 g_pb(PB_NORTH), g_pb(PB_WEST), g_pb(PB_EAST));
    else if (which == 0)
        snprintf(line, n, e ? "RATON DER. GIRAR   RUEDA DISTANCIA   %s%s%s%s MOVER   %s%s ALTURA   %s CATEGORIA   %s PANEL   %s FOTO   %s SALIR"
                            : "RIGHT MOUSE ORBIT   WHEEL DISTANCE   %s%s%s%s MOVE   %s%s HEIGHT   %s CATEGORY   %s PANEL   %s PHOTO   %s EXIT",
                 g_kb(PK_UP), g_kb(PK_LEFT), g_kb(PK_DOWN), g_kb(PK_RIGHT), g_kb(PK_PH_SINK), g_kb(PK_PH_RISE),
                 g_kb(PK_PH_TAB), g_kb(PK_PH_HIDE), g_kb(PK_PH_SHOT), g_kb(PK_PHOTO));
    else if (g_pad_last && which == 1)
        snprintf(line, n, e ? "%s GIRAR   %s MOVER   %s%s DISTANCIA   %s CENTRAR   %s SALIR"
                            : "%s ORBIT   %s MOVE   %s%s DISTANCE   %s RESET   %s EXIT",
                 g_part("R_2D"), g_part("L_2D"), g_pb(PB_L2), g_pb(PB_R2), g_pb(PB_R3), g_pb(PB_NORTH));
    else if (which == 1)
        snprintf(line, n, e ? "RATON DER. GIRAR   RUEDA DISTANCIA   %s%s%s%s MOVER   %s%s ALTURA   %s CENTRAR   %s SALIR"
                            : "RIGHT MOUSE ORBIT   WHEEL DISTANCE   %s%s%s%s MOVE   %s%s HEIGHT   %s RESET   %s EXIT",
                 g_kb(PK_PH_NAV_UP), g_kb(PK_PH_NAV_LEFT), g_kb(PK_PH_NAV_DOWN), g_kb(PK_PH_NAV_RIGHT),
                 g_kb(PK_PH_SINK), g_kb(PK_PH_RISE), g_kb(PK_PH_RESET), g_kb(PK_PH_PLAY));
    else if (g_pad_last)
        snprintf(line, n, e ? "%s PAUSA   %s BUSCAR   %s%s VELOCIDAD   %s CAMARA LIBRE   %s MOVIMIENTOS / CINE   %s%s FOTO   %s SALIR"
                            : "%s PAUSE   %s SEEK   %s%s SPEED   %s FREE CAMERA   %s MOVES / CINEMA   %s%s PHOTO   %s EXIT",
                 g_pb(PB_SOUTH), g_part("Dpad_X"), g_pb(PB_L1), g_pb(PB_R1), g_pb(PB_NORTH), g_part("Dpad_Y"),
                 g_pb(PB_L3), g_pb(PB_R3), g_pb(PB_EAST));
    else
        snprintf(line, n, e ? "%s PAUSA   %s%s BUSCAR   %s%s VELOCIDAD   %s CAMARA LIBRE   %s MOVIMIENTOS   %s CINE   %s FOTO"
                            : "%s PAUSE   %s%s SEEK   %s%s SPEED   %s FREE CAMERA   %s MOVES   %s CINEMA   %s PHOTO",
                 g_kb(PK_PH_SHOT), g_kb(PK_LEFT), g_kb(PK_RIGHT), g_kb(PK_DOWN), g_kb(PK_UP),
                 g_kb(PK_PH_PLAY), g_kb(PK_PH_TAB), g_kb(PK_PH_HIDE), g_kb(PK_PHOTO));
}

static void hint_line(float W, float H, float s)
{
    char line[1200];
    float bh = 15.0f * s, y = H - bh;
    float grad[16] = { 0.04f, 0.035f, 0.03f, 0.70f,  0.04f, 0.035f, 0.03f, 0.70f,
                       0.04f, 0.035f, 0.03f, 0.95f,  0.04f, 0.035f, 0.03f, 0.95f };

    hints_text(0, line, sizeof line);
    limeFillRectGrad(0.0f, y, W, bh, grad);
    limeFillRect(0.0f, y, W, 1.0f * s, 0.23f, 0.20f, 0.16f, 1.0f);
    PortNavDrawLineC(line, y + 2.5f * s, 0.5f);
}

/* game_main.c, over the replay player's panel: its keys with their
 * pictures, along its bottom (`device_pad` the pad used last) */
void photo_replay_hints(int device_pad, float y, float scale)
{
    char line[1200];
    int was = g_pad_last;
    g_pad_last = device_pad;
    hints_text(2, line, sizeof line);
    g_pad_last = was;
    limeEnableAlphaBlending_Basic();
    limeSet2DDrawing();
    PortNavDrawLineC(line, y, scale);
}

void photo_draw(void)
{
    float W = (float)limeScreenWidth, H = (float)limeScreenHeight, s = FE_HeightScale;
    PANEL p = panel_geom();
    int c, r;

    if (!photo_on()) {
        if (photo_free_on()) {          /* the FREE CAMERA's mark and keys, top centre */
            char keys[1200];
            float bw = 300.0f * s, bx = W * 0.5f - bw * 0.5f, by = 58.0f * s;   /* under the bars and the clock */
            hints_text(1, keys, sizeof keys);
            limeEnableAlphaBlending_Basic();
            limeSet2DDrawing();
            limeDisableDepthTest();
            limeFillRect(bx, by, bw, 24.0f * s, 0.055f, 0.05f, 0.045f, 0.72f);
            limeFillRect(bx, by, bw, 1.0f * s, 0.85f, 0.60f, 0.29f, 1.0f);
            limeFillRect(W * 0.5f - 34.0f * s, by + 3.5f * s, 4.0f * s, 4.0f * s, 0.35f, 0.80f, 0.40f, 1.0f);
            text(tx("CAMARA LIBRE", "FREE CAMERA"), W * 0.5f - 27.0f * s, by + 2.5f * s, 0, 0.24f, C_BRONZE);
            PortNavDrawLineC(keys, by + 11.0f * s, 0.55f);
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

        /* the categories: the one shown between two arrows, a pip each
         * (five no longer fit side by side in the compact panel) */
        (void)tw;
        limeFillRect(p.x, p.y, p.w, p.tab_h, 0.85f, 0.60f, 0.29f, 0.10f);
        /* the arrows are buttons: lit under the mouse */
        {
            float mx = PortMouseX, my = PortMouseY;
            int on_l = my >= p.y && my < p.y + p.tab_h && mx >= p.x && mx < p.x + 22.0f * s;
            int on_r = my >= p.y && my < p.y + p.tab_h && mx >= p.x + p.w - 22.0f * s && mx < p.x + p.w;
            if (on_l)
                limeFillRect(p.x, p.y, 22.0f * s, p.tab_h, 0.85f, 0.60f, 0.29f, 0.25f);
            if (on_r)
                limeFillRect(p.x + p.w - 22.0f * s, p.y, 22.0f * s, p.tab_h, 0.85f, 0.60f, 0.29f, 0.25f);
            text("<", p.x + 11.0f * s, p.y + 3.0f * s, 1, 0.30f, on_l ? C_BONE : C_BRONZE);
            text(">", p.x + p.w - 11.0f * s, p.y + 3.0f * s, 1, 0.30f, on_r ? C_BONE : C_BRONZE);
        }
        text(cat_name(g_cat), p.x + p.w * 0.5f, p.y + 2.5f * s, 1, 0.27f, C_BRONZE);
        for (c = 0; c < CAT_N; c++)
            limeFillRect(p.x + p.w * 0.5f + ((float)c - (float)(CAT_N - 1) * 0.5f) * 5.0f * s - 1.0f * s,
                         p.y + p.tab_h - 3.0f * s, 2.0f * s, 1.5f * s,
                         c == g_cat ? 0.85f : 0.40f, c == g_cat ? 0.60f : 0.36f,
                         c == g_cat ? 0.29f : 0.30f, 1.0f);
        limeFillRect(p.x, p.y + p.tab_h, p.w, 1.0f * s, 0.23f, 0.20f, 0.16f, 1.0f);

        /* the heading; IMAGE and EFFECTS say when what draws cannot */
        y = p.y + p.tab_h;
        text(cat_name(g_cat), p.x + 6.0f * s, y + 3.0f * s, 0, 0.32f, C_BONE);
        if ((g_cat == CAT_IMAGE || g_cat == CAT_EFFECTS) && !rgl_post_available())
            text(tx("NO DISPONIBLE CON ESTE RENDERIZADOR", "NOT AVAILABLE WITH THIS RENDERER"),
                 p.x + p.w - 6.0f * s, y + 4.5f * s, 2, 0.15f, C_ASH);
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
            text(lb, p.x + 6.0f * s, y + 3.0f * s, 0, 0.245f, r == g_row ? C_BONE : C_ASH);
            if (va[0]) {
                /* the value between two arrows the mouse presses: the left
                 * one less, the right one more (the row's own click more too) */
                text("<", p.x + p.w * 0.56f, y + 3.0f * s, 1, 0.26f, r == g_row ? C_BRONZE : C_ASH);
                text(">", p.x + p.w - 6.0f * s, y + 3.0f * s, 2, 0.26f, r == g_row ? C_BRONZE : C_ASH);
                text(va, p.x + p.w * 0.78f, y + 3.0f * s, 1, 0.245f, C_BRONZE);
            }
        }
    }
    {
        /* HIDE MENU, or the MENU tab while it is hidden; its key named */
        float bx, by, bw, bh, mx = PortMouseX, my = PortMouseY;
        int on;
        char lb[64], key[16];
        plat_key_label(PK_PH_HIDE, key, sizeof key);
        hide_btn(&p, &bx, &by, &bw, &bh);
        on = mx >= bx && mx < bx + bw && my >= by && my < by + bh;
        limeFillRect(bx, by, bw, bh, on ? 0.30f : 0.10f, on ? 0.20f : 0.09f, on ? 0.10f : 0.08f, 0.92f);
        limeFillRect(bx, by, bw, 1.0f * s, 0.85f, 0.60f, 0.29f, on ? 1.0f : 0.6f);
        snprintf(lb, sizeof lb, g_panel_hidden ? tx("MENU (%s)", "MENU (%s)")
                                               : tx("OCULTAR MENU (%s)", "HIDE MENU (%s)"), key);
        text(lb, bx + bw * 0.5f, by + 3.0f * s, 1, 0.24f, on ? C_BONE : C_BRONZE);
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
