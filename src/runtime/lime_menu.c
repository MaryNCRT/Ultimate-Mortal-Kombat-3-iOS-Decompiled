/*
 * lime_menu.c — the rest of the engine's platform API, for the front end.
 *
 * `lime_platform.c` covers what the differential tests need: files, memory,
 * textures, blend state, and a `limeDrawSprite` that records rather than draws.
 * The front end needs more than that — it is 291 functions of menus that expect
 * a screen size, a touch position, a frame-rate scale, a 2D mode, a fill, and a
 * sound layer. This file is the rest of that boundary, and it is what takes
 * `decomp/gamecode` from "compiles" to "links".
 *
 * ## What is real here and what is not
 *
 * **Real:** the state the front end reads back and branches on. Screen size,
 * `limeFPSScaleFactor`, the touch globals, the matrix stack, the language, and
 * the save file. Every one of those changes what the decompiled code *does*, so
 * a stub that lies about them would produce a menu that behaves differently
 * from the original for reasons nobody could trace.
 *
 * **Sound is real** since 2026-10-02: sounds and tunes go to platform.h's
 * mixer, with the semantics read off the binary (see the sound section).
 * `limePlaySound` still counts, so a test can assert a click was requested
 * without a device to hear it.
 *
 * **Not real:** vibration, the modal dialogs and the loading spinner -- their
 * only effect is on the glass or the hand, and nothing reads them back.
 *
 * The line between the two is not "is it easy" -- it is **does the game read it
 * back**. `limeGetStringWidth` is in `decomp/lime/limeFont.c` because the front
 * end lays out text against the answer; nothing ever asks how a note sounded,
 * so sound needs no more than to be heard.
 *
 * ## limeFPSScaleFactor is 1.0 and that is a decision
 *
 * Every timer in the front end divides by it -- `0.01f / limeFPSScaleFactor` a
 * frame -- so it sets what a "frame" means. Holding it at 1.0 makes one call to
 * the task function one 60Hz frame, which is what the transcriptions were read
 * against. A host that runs faster should scale it rather than run the menus
 * faster, and `lime_menu_set_fps_scale` is how.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#include "../decomp/lime/lime.h"
#include "platform/platform.h"
#include "wav.h"


/* ------------------------------------------------------------------ screen */

/* The retail surface. The transcriptions were read against 480x320 and every
 * mixed-scale site in the front end is invisible at exactly this size, so it is
 * the honest default: anything else exercises bugs the original never hit.
 * See issue #22. */
int   limeScreenWidth  = 480;
int   limeScreenHeight = 320;
int  *limeScreenWidthP  = &limeScreenWidth;
int  *limeScreenHeightP = &limeScreenHeight;

int   limeDeviceSideways         = 1;
int   limeDeferredDeviceSideways = 1;
int   limeLastDeviceSideways     = 1;

/* The physical panel, portrait, as the binary's __data has it: 0x00171ae4 and
 * 0x00171ae8 hold 320 and 480. limeBegin derives limeScreenWidth/Height from
 * them every frame and `LIMEDS_Set3dMode` divides by the height. */
int   limeDeviceWidth  = 320;
int   limeDeviceHeight = 480;
int   limeDeviceOffsetX;
int   limeDeviceOffsetY;
int   limeDeviceCanVibrate = 1;         /* 0x00171c08 */

/* __common in the binary. `limeBegin` writes the sideways matrix every frame
 * and `LIMEDS_Set3dMode` the perspective one. */
float limePerspectiveMatrix[16];
float limeSidewaysMat[16];
float ratio = 1.666f;                   /* 0x0017146c; Set3dMode sets 0.6 */

/* The port's way to ask for a surface other than 480x320. limeBegin rebuilds
 * limeScreenWidth/Height from the panel every frame, as the binary does, so the
 * request goes in as the panel -- turned portrait, because the game is held
 * sideways -- and comes back out as (w, h). */
void lime_menu_set_screen(int w, int h)
{
    limeDeviceWidth  = h;
    limeDeviceHeight = w;
    limeScreenWidth  = w;
    limeScreenHeight = h;
}


/* -------------------------------------------------------------------- time */

float  limeFPS             = 60.0f;
float  limeFPSScaleFactor  = 1.0f;
float *limeFPSScaleFactorP = &limeFPSScaleFactor;
long   limeRenderedPolyCount;

/* `_currenttime` / `_lasttime`, doubles in __common. On the device limeBegin
 * fills the first from CFAbsoluteTimeGetCurrent. The port's hosts tick at a
 * fixed 60 Hz -- several ticks back to back after a slow frame -- so a wall
 * clock would hand limeBegin a dt near zero and a scale factor in the
 * thousands. The host advances this clock by the tick it is simulating
 * instead, which is what the device's clock reads at a steady 60 fps. */
double currenttime;
double lasttime;
static double g_clock;

void lime_menu_advance_clock(double seconds)
{
    g_clock += seconds;
}

void lime_menu_set_fps_scale(float f)
{
    /* Zero would divide by zero in every front-end timer at once, which reads
     * as "the menus froze" rather than as a bad argument. Overwritten by the
     * next limeBegin; this is for hosts that do not call it. */
    limeFPSScaleFactor = (f > 0.0f) ? f : 1.0f;
}


/* ------------------------------------------------------------------- touch */

/* Ten slots each, 0x28 bytes apart in the binary's __data (0x00171af4 on),
 * all -1 at boot. A host feeds them through the lime_touch_* calls below,
 * which are EAGLView's touch handlers; limeFinish copies the live pair into
 * the Last pair at the end of every frame. So for one finger:
 *
 *      frame      X[0]    Last[0]
 *      press      pos     -1
 *      held       pos     pos
 *      release    -1      pos
 *
 * which is the shape both spellings of the front end's hit tests read. */
float limeTouchScreenX[10]     = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
float limeTouchScreenY[10]     = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
float limeLastTouchScreenX[10] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
float limeLastTouchScreenY[10] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
float limeTouchScreenVelX[10];
float limeTouchScreenVelY[10];

/* 0x00171be8 and 0x00171bf8. Element 0 is the number of touches in the last
 * touchesBegan event; limeFinish copies all four into the Last copy. */
long limeDebugToggle[4]     = { 0, 1, 1, 0 };
long limeLastDebugToggle[4];

/* A single 64-bit key mask that Task_GameMain reads (bit 8 is BLOCK). Nothing
 * in the binary writes it, so nothing here does either. It used to be an int
 * that touches set to 1 -- half the size GameCode.c reads, and the wrong
 * meaning. */
long limePressed[2];

#define TOUCH_NEAR 24.0f                /* vmov.f32 s6, #24.0 in all three */

static int touch_near(int i, float x, float y)
{
    return fabsf(limeTouchScreenY[i] - y) < TOUCH_NEAR
        && fabsf(limeTouchScreenX[i] - x) < TOUCH_NEAR;
}

/* The handlers take game coordinates. The binary converts each UITouch with
 * `X = y * limeContextScale, Y = limeDeviceWidth - x * limeContextScale` when
 * sideways; that is a rotation, so the 24-unit tests below are the same in
 * either frame and a host that already has game coordinates skips it. */

/* -[EAGLView touchesBegan:withEvent:], 0x00062168. Each touch takes the first
 * free slot of ten; a touch with no free slot is dropped. */
void lime_touch_began(float x, float y)
{
    int i;

    limeDebugToggle[0] = 0;
    for (i = 0; i < 10; i++) {
        if (limeTouchScreenX[i] == -1.0f) {
            limeTouchScreenX[i] = x;
            limeTouchScreenY[i] = y;
            break;
        }
    }
    limeDebugToggle[0]++;
}

/* -[EAGLView touchesMoved:withEvent:], 0x00061edc. Every live slot within 24
 * units of where the touch is, or of where it was, moves to where it is. */
void lime_touch_moved(float x, float y, float prev_x, float prev_y)
{
    int i;

    for (i = 0; i < 10; i++) {
        if (limeTouchScreenX[i] == -1.0f)
            continue;
        if (touch_near(i, x, y) || touch_near(i, prev_x, prev_y)) {
            limeTouchScreenX[i] = x;
            limeTouchScreenY[i] = y;
        }
    }
}

/* -[EAGLView touchesEnded:withEvent:], 0x00061c3c, and touchesCancelled,
 * 0x000619b0, which is the same instruction for instruction except for the
 * fallback: when no slot was near the touch, Ended clears all ten and
 * Cancelled only the first two. */
static void touch_lift(float x, float y, float prev_x, float prev_y, int all)
{
    int i, hit = 0;

    for (i = 0; i < 10; i++) {
        if (limeTouchScreenX[i] == -1.0f)
            continue;
        if (touch_near(i, x, y) || touch_near(i, prev_x, prev_y)) {
            limeTouchScreenX[i] = -1.0f;
            limeTouchScreenY[i] = -1.0f;
            hit = 1;
        }
    }
    if (!hit) {
        for (i = 0; i < (all ? 10 : 2); i++) {
            limeTouchScreenX[i] = -1.0f;
            limeTouchScreenY[i] = -1.0f;
        }
    }
}

void lime_touch_ended(float x, float y, float prev_x, float prev_y)
{
    touch_lift(x, y, prev_x, prev_y, 1);
}

void lime_touch_cancelled(float x, float y, float prev_x, float prev_y)
{
    touch_lift(x, y, prev_x, prev_y, 0);
}

/* For tests: everything back to the boot state. */
void lime_menu_touch_idle(void)
{
    int i;

    for (i = 0; i < 10; i++) {
        limeTouchScreenX[i] = limeTouchScreenY[i] = -1.0f;
        limeLastTouchScreenX[i] = limeLastTouchScreenY[i] = -1.0f;
        limeTouchScreenVelX[i] = limeTouchScreenVelY[i] = 0.0f;
    }
}


/* ------------------------------------------------------------ frame + mode */

static int g_in_2d;
static int g_depth_test = 1;
static long g_colour_mask[4] = { 1, 1, 1, 1 };
static long g_fills;

int rb_fixed_step(void);               /* rollback_probe.c */
int rb_active(void);                    /* rollback.c */
int rb_quiet(void);

/* armv7 0x00066a74, transcribed whole. The one substitution is the clock:
 * CFAbsoluteTimeGetCurrent becomes the host-advanced clock above. */
/* Port: frames begun, so what runs once a frame can tell a second call in
 * the same frame (GameCode.c port_smooth_pose) */
unsigned long port_frame_no;

void limeBegin(void)
{
    double dt;
    int k;

    port_frame_no++;

    limeDeviceSideways = limeDeferredDeviceSideways;

    currenttime = g_clock;
    dt = currenttime - lasttime;
    if (dt > 0.0) {
        limeFPSScaleFactor = (float)((1.0 / 60.0) / dt);
        limeFPS = limeFPSScaleFactor * 60.0f;
        if (limeFPSScaleFactor < 0.25f)
            limeFPSScaleFactor = 0.25f;
    }
    /* Port, not in the binary: a recorded or replayed fight
     * (rollback_probe.c) runs a fixed step, whatever the clock says. */
    if (dt <= 0.0 || rb_fixed_step() || rb_active()) {
        limeFPSScaleFactor = 1.0f;
        limeFPS = 60.0f;
    }
    lasttime = currenttime;
    limeRenderedPolyCount = 0;

    /* A turn of the device drops both live touches and both last ones. */
    if (limeDeviceSideways != limeLastDeviceSideways) {
        for (k = 0; k < 2; k++) {
            limeTouchScreenX[k] = limeLastTouchScreenX[k] = -1.0f;
            limeTouchScreenY[k] = limeLastTouchScreenY[k] = -1.0f;
        }
    }
    limeLastDeviceSideways = limeDeviceSideways;

    if (limeDeviceSideways == 0) {
        limeScreenWidth  = limeDeviceWidth;
        limeScreenHeight = limeDeviceHeight;
        RotMatrixZ(limeSidewaysMat, 0.0f);
    } else {
        limeScreenWidth  = limeDeviceHeight;
        limeScreenHeight = limeDeviceWidth;
        RotMatrixZ(limeSidewaysMat, -1.57079637f);      /* 0xbfc90fdb */
    }
    limeDeviceOffsetX = (limeDeviceWidth  - limeScreenWidth)  / 2;
    limeDeviceOffsetY = (limeDeviceHeight - limeScreenHeight) / 2;

    /* Velocity of the first two touches, frame to frame -- zero unless both
     * this frame's and last frame's positions are real. */
    for (k = 0; k < 2; k++) {
        if (limeTouchScreenX[k] == -1.0f || limeTouchScreenY[k] == -1.0f
            || limeLastTouchScreenX[k] == -1.0f
            || limeLastTouchScreenY[k] == -1.0f) {
            limeTouchScreenVelX[k] = 0.0f;
            limeTouchScreenVelY[k] = 0.0f;
        } else {
            limeTouchScreenVelX[k] = limeTouchScreenX[k] - limeLastTouchScreenX[k];
            limeTouchScreenVelY[k] = limeTouchScreenY[k] - limeLastTouchScreenY[k];
        }
    }
}

/* armv7 0x00065df0: the debug toggles and the first two touch slots roll into
 * their Last copies, then depth writes go back on (glDepthMask(1), here through
 * the function that issues it). */
void limeFinish(void)
{
    memcpy(limeLastDebugToggle, limeDebugToggle, sizeof limeDebugToggle);
    limeLastTouchScreenX[0] = limeTouchScreenX[0];
    limeLastTouchScreenX[1] = limeTouchScreenX[1];
    limeLastTouchScreenY[0] = limeTouchScreenY[0];
    limeLastTouchScreenY[1] = limeTouchScreenY[1];
    limeEnableDepthWrites();
}

#ifndef UMK3_REAL_GL   /* headless; see runtime/draw_gl.c for the windowed half */
void limeSet2DDrawing(void)     { g_in_2d = 1; }
void limeEnableDepthTest(void)  { g_depth_test = 1; }
void limeDisableDepthTest(void) { g_depth_test = 0; }
void limeClearDepthBuffer(void) { }
void limePortDisplayRotation(void) { }
/* armv7 0x00066e68: four channels, each narrowed to a byte. */
void limeSetColourMask(long r, long g, long b, long a)
{
    g_colour_mask[0] = r;  g_colour_mask[1] = g;
    g_colour_mask[2] = b;  g_colour_mask[3] = a;
}

#endif  /* !UMK3_REAL_GL */
int  lime_menu_in_2d(void)      { return g_in_2d; }
int  lime_menu_depth_test(void) { return g_depth_test; }
long lime_menu_fill_count(void) { return g_fills; }

/* Counted, not drawn -- same contract as limeDrawSprite next door. The front
 * end fills rectangles for the button-editor's forbidden bands and for the
 * treasure screen's portrait box, and a test can assert the count without a
 * window. */
#ifndef UMK3_REAL_GL   /* headless; see runtime/draw_gl.c for the windowed half */
void limeFillRect(float x, float y, float w, float h,
                  float r, float g, float b, float a)
{
    (void)x; (void)y; (void)w; (void)h;
    (void)r; (void)g; (void)b; (void)a;
    g_fills++;
}
#endif  /* !UMK3_REAL_GL */

/* The three billboard entries -- blood and Scorpion's spear. The windowed
 * build draws them (runtime/draw_gl.c, transcribed from the binary); headless,
 * they only have to link, with the signatures the callers really use. */
#ifndef UMK3_REAL_GL
void limeDrawFaceMeSprite(TEXTURE *t, const float *m, float x, float y, float z,
                          float u0, float v0, float du, float dv, float size,
                          float r, float g, float b, float a)
{
    (void)t; (void)m; (void)x; (void)y; (void)z; (void)u0; (void)v0;
    (void)du; (void)dv; (void)size; (void)r; (void)g; (void)b; (void)a;
}

void limeDrawFaceUpSprite(TEXTURE *t, const float *m, float x, float y, float z,
                          float u0, float v0, float du, float dv, float size,
                          float r, float g, float b, float a)
{
    (void)t; (void)m; (void)x; (void)y; (void)z; (void)u0; (void)v0;
    (void)du; (void)dv; (void)size; (void)r; (void)g; (void)b; (void)a;
}

void limeDrawFaceMeSpriteWH(TEXTURE *t, const float *m, float x, float y, float z,
                            float u0, float v0, float du, float dv,
                            float w, float h, float r, float g, float b, float a,
                            float unused0, float unused1)
{
    (void)t; (void)m; (void)x; (void)y; (void)z; (void)u0; (void)v0;
    (void)du; (void)dv; (void)w; (void)h; (void)r; (void)g; (void)b; (void)a;
    (void)unused0; (void)unused1;
}
#endif  /* !UMK3_REAL_GL */


/* ---------------------------------------------------------------- matrices */

/* The current model matrix, as the engine's own calls leave it. Identity is the
 * right start: the front end reads it back through limeGetCurrentModelMatrix to
 * place 3D characters against 2D menu coordinates, and a garbage matrix there
 * puts fighters off-screen in a way that looks like a decompilation error. */
static float g_model[16] = {
    1, 0, 0, 0,
    0, 1, 0, 0,
    0, 0, 1, 0,
    0, 0, 0, 1,
};

/* armv7 0x0006699c is glGetFloatv(GL_MODELVIEW_MATRIX, out); the windowed
 * build does exactly that in draw_gl.c. */
#ifndef UMK3_REAL_GL
void limeGetCurrentModelMatrix(float *out)
{
    memcpy(out, g_model, sizeof g_model);
}
#endif


/* `lime_platform.c` covers the six GL entries the engine's mesh path needs;
 * this is the seventh, which only the front end reaches -- DrawTower3D spins
 * the vortex with it and drawCharacterSelection turns the fighters. A no-op for
 * the same reason as its neighbours: this boundary tracks state, it does not
 * render, and a port that draws replaces the whole of it at once. */
#ifndef UMK3_REAL_GL   /* headless; see runtime/draw_gl.c for the windowed half */
void glRotatef(float angle, float x, float y, float z)
{
    (void)angle; (void)x; (void)y; (void)z;
}
#endif  /* !UMK3_REAL_GL */


/* ------------------------------------------------------------------- sound
 *
 * The iOS layer is Finch (one-shot `Sound` objects) and `GBMusicTrack` (one
 * streamed tune). What each entry point does was read off the binary --
 * `python tools/cd.py limeLoadSound limePlaySound limePlayTune ...` -- and is
 * kept here, down to the quirks:
 *
 *   limeInitSound      calls limeInitAudio, which clears the 512-entry
 *                      sound table (0x171c38)
 *   limeLoadSound      the first EMPTY slot gets [Sound initWithFile:
 *                      "res/audio/<name>.wav"] and its index is returned --
 *                      from 0, -1 only when all 512 are taken. A file that
 *                      fails to load leaves the slot empty and still returns
 *                      the index, so the next load reuses it.
 *   limePlaySound      id == -1 does nothing; otherwise `play`, then
 *                      `setGain:` with the second argument. The third and
 *                      fourth arguments are never read.
 *   limePlayTune       "res/audio/<name>", repeat if `loop`, gain vol / 100.
 *   limeSetTuneVol     gain vol / 100.
 *   limeStopTune       close and release the tune.
 *
 * The SDL mixer streams MP3 tunes separately from the queued sound-effect
 * voices. Both paths are safe when the host has no audio output.
 */

void lime_platform_resolve(const char *rel, char *out, size_t n);
void limeLog(const char *fmt, ...);

#define LIME_SOUNDS 512

typedef struct {
    unsigned char *pcm;          /* NULL: the slot is empty */
    int            frames;
    int            rate;
} lime_sound;

static lime_sound g_sound[LIME_SOUNDS];
static long       g_sounds_played;

long lime_menu_sounds_played(void) { return g_sounds_played; }

/* [[Finch alloc] init], then the table cleared -- 0x200 words */
void limeInitAudio(void)
{
    int i;

    plat_audio_open(16000);             /* 496 of the 497 files are 16 kHz */
    for (i = 0; i < LIME_SOUNDS; i++) {
        free(g_sound[i].pcm);
        g_sound[i].pcm = NULL;
    }
}

void limeInitSound(void)
{
    limeInitAudio();                    /* all it does on device */
}

long limeLoadSound(const char *name)
{
    char rel[160], full[1200];
    long i;

    for (i = 0; i < LIME_SOUNDS; i++)
        if (!g_sound[i].pcm)
            break;
    if (i == LIME_SOUNDS) {
        /* On device too; said here because it is silent otherwise. */
        printf("limeLoadSound: all %d slots taken, %s not loaded\n",
               LIME_SOUNDS, name);
        return -1;
    }

    snprintf(rel, sizeof rel, "res/audio/%s.wav", name);
    lime_platform_resolve(rel, full, sizeof full);
    if (!wav_load_u8(full, &g_sound[i].pcm, &g_sound[i].frames, &g_sound[i].rate)) {
        g_sound[i].pcm = NULL;          /* as on device: the index, an empty slot */
        if (getenv("UMK3_LOG_SOUND"))
            printf("limeLoadSound: %s did not load (slot %ld)\n", name, i);
    }
    if (getenv("UMK3_LOG_SOUND"))
        printf("limeLoadSound: %s -> slot %ld\n", name, i);
    return i;
}

void limeDeleteSound(long h)
{
    if (h < 0 || h >= LIME_SOUNDS)
        return;
    plat_audio_stop_pcm(g_sound[h].pcm);   /* a voice may still be on it */
    free(g_sound[h].pcm);
    g_sound[h].pcm = NULL;
}

void limePlaySound(long h, float v, float p, long f)
{
    (void)p; (void)f;                   /* not read by the original either */
    if (rb_quiet())                     /* port: a frame run again (rollback.c) */
        return;
    if (h == -1) {
        if (getenv("UMK3_LOG_SOUND"))
            printf("limePlaySound: -1 (a sound never loaded)\n");
        return;
    }
    g_sounds_played++;
    if (h < 0 || h >= LIME_SOUNDS || !g_sound[h].pcm) {
        if (getenv("UMK3_LOG_SOUND"))
            printf("limePlaySound: %ld is empty\n", h);
        return;
    }
    if (getenv("UMK3_LOG_SOUND"))
        printf("limePlaySound: %ld (%d frames, gain %.2f)\n", h, g_sound[h].frames, v);
    plat_audio_play_at(g_sound[h].pcm, g_sound[h].frames, g_sound[h].rate, v);
}

/* limePlayTune keeps the path, the loop flag and the volume (lasttunevol,
 * lasttuneloop) for limeRestartPlayTune -- and sets the volume to -1 for a
 * tune that does not repeat, which is what makes the restart skip it. */
static char g_tune_path[1400];
static long g_tune_vol = -1, g_tune_loop;

void limePlayTune(const char *name, long vol, long loop)
{
    char rel[300];

    if (rb_quiet())                     /* port: a frame run again (rollback.c) */
        return;
    snprintf(rel, sizeof rel, "res/audio/%s", name);
    lime_platform_resolve(rel, g_tune_path, sizeof g_tune_path);
    g_tune_loop = loop;
    g_tune_vol  = loop ? vol : -1;    plat_music_volume((float)vol / 100.0f);
    plat_music_play(g_tune_path, loop != 0);
}

/* What the app delegate calls on returning to the foreground: the last
 * repeating tune again, from the start, at the last volume. */
void limeRestartPlayTune(void)
{
    if (g_tune_vol == -1)
        return;
    plat_music_volume((float)g_tune_vol / 100.0f);
    plat_music_play(g_tune_path, g_tune_loop != 0);
}

void limeStopTune(void)               { if (!rb_quiet()) plat_music_stop(); }
void limeSetTuneVol(long v)
{
    if (rb_quiet())                     /* port: a frame run again (rollback.c) */
        return;
    g_tune_vol = v;                     /* lasttunevol, as limeSetTuneVol stores it */
    plat_music_volume((float)v / 100.0f);
}
/* AudioSessionGetProperty('othr') on device -- "is another app's audio
 * playing?" -- as 1 or 0. ResetSettingsData turns the game's music OFF when it
 * says yes. This was `void`, so the caller read whatever was in the return
 * register, nearly always non-zero, and a first run started with music off.
 * No other app owns the speakers here: 0. */
int limeCheckForUserMusic(void)       { return 0; }


/* ------------------------------------------------------------------ system */

/* The language decides three different CJK carve-outs in the front end alone
 * (FE_Task_Bios at 0.95, FE_Task_About_About at 0.8, FE_Task_About_Help scaling
 * both up), so it is read back and it matters. */
static char g_language[8] = "EN";

/* Fills the caller's buffer; it does not return one. GameCode.c calls it as
 * `limeGetLanguage(Language, 10)` and then builds "LANGUAGE_TEXT_%s" from what
 * it wrote -- declared as returning a pointer, `Language` stayed empty and the
 * key came out as "LANGUAGE_TEXT_", which Info.plist has no entry for. */
/* armv7 0x00067478: [[[NSLocale preferredLanguages] objectAtIndex:0]
 * uppercaseString], strlcpy'd into the caller's buffer and then cut to two
 * characters with dst[2] = 0 -- "es-CO" arrives as "ES". The game ships
 * LANGUAGE_TEXT_ EN, FR, DE, IT, ES, KO, ZH and OTHER, so on a Spanish system
 * it comes up in Spanish, as the phone would.
 *
 * The windowed build asks the system (plat_language); UMK3_LANG=<code>
 * overrides it. The headless build keeps lime_menu_set_language's value, EN
 * unless a test says otherwise, so its counts do not depend on the machine. */
void limeGetLanguage(char *dst, int len)
{
    char code[16];
    int i;

    if (dst == NULL || len <= 0)
        return;

    snprintf(code, sizeof code, "%s", g_language);
#ifdef UMK3_REAL_GL
    {
        const char *e = getenv("UMK3_LANG");
        if (e && *e) {
            snprintf(code, sizeof code, "%s", e);
        } else {
            char sys[16];
            plat_language(sys, (int)sizeof sys);
            if (sys[0])
                snprintf(code, sizeof code, "%s", sys);
        }
    }
#endif
    for (i = 0; code[i]; i++)
        if (code[i] >= 'a' && code[i] <= 'z')
            code[i] = (char)(code[i] - 'a' + 'A');

    snprintf(dst, (size_t)len, "%s", code);
    if (len > 2)
        dst[2] = 0;
}

void lime_menu_set_language(const char *code)
{
    strncpy(g_language, code ? code : "EN", sizeof g_language - 1);
    g_language[sizeof g_language - 1] = 0;
}

/* Info.plist, read once and searched per key.
 *
 * The file is the XML plist form: pairs of
 *
 *     <key>LANGUAGE_TEXT_EN</key>
 *     <string>english_text.dat</string>
 *
 * so a match is "find the key's text, then take the next <string>". Reading it
 * rather than hardcoding the answers keeps the language mapping the bundle's
 * business, which is what the game expects -- it asks for LANGUAGE_TEXT_%s and
 * the %s comes from the device's locale.
 */
const char *lime_platform_asset_root(void);

static char *g_plist;

static void plist_load(void)
{
    static int tried;
    char path[1100];
    const char *root;
    FILE *f;
    long n;

    if (tried)
        return;
    tried = 1;

    /* On device Info.plist sits beside res/; the game folder keeps its copy
     * inside res/, so the port reads nothing outside its own folder. */
    root = lime_platform_asset_root();
    if (root == NULL || *root == 0)
        return;
    snprintf(path, sizeof(path), "%s/Info.plist", root);

    f = fopen(path, "rb");
    if (f == NULL)
        return;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n > 0 && n < (long)(4 * 1024 * 1024)) {
        g_plist = (char *)malloc((size_t)n + 1);
        if (g_plist && fread(g_plist, 1, (size_t)n, f) == (size_t)n)
            g_plist[n] = 0;
        else {
            free(g_plist);
            g_plist = NULL;
        }
    }
    fclose(f);
}

/* armv7 0x00065560: `[[[[NSBundle mainBundle] infoDictionary]
 * objectForKey:key] cString]`. A missing key is nil and `[nil cString]` is
 * NULL -- which is what -[UMK3AppDelegate startAppWithOptions:] tests to fall
 * back to English when the bundle has no TOS_URL_<language>. */
const char *limeGetPropertyString(const char *key)
{
    static char value[256];
    char needle[128];
    const char *at, *s, *e;

    if (key == NULL || *key == 0)
        return NULL;

    plist_load();
    if (g_plist) {
        snprintf(needle, sizeof(needle), "<key>%s</key>", key);
        at = strstr(g_plist, needle);
        if (at) {
            s = strstr(at + strlen(needle), "<string>");
            if (s) {
                s += 8;
                e = strstr(s, "</string>");
                if (e && (size_t)(e - s) < sizeof(value)) {
                    memcpy(value, s, (size_t)(e - s));
                    value[e - s] = 0;
                    return value;
                }
            }
        }
    }

    /* FE_Task_About_About prints this into "Version: %s". The binary this was
     * read from is 1.2.59, and it is the one answer worth keeping when the
     * plist is not where the assets are. */
    if (g_plist == NULL && strcmp(key, "CFBundleVersion") == 0)
        return "1.2.59";
    return NULL;
}

/* armv7 0x0006529c. Its own LCG, not the C library's: `_rand_seed` starts at
 * 0x00089c84 in __data, nothing reseeds it, and the result is the signed
 * `(seed / 65536) % 32768` the compiler's rounding sequence spells out -- the
 * same generator as PIrand in Particles.c, on its own seed. The arithmetic is
 * done unsigned and read back as int32 so it wraps the same on any host. */
extern int32_t rand_seed;               /* 0x00089c84 at start, fight_runtime.c:
                                         * in what a rollback puts back, which
                                         * this file is not (UMK3_NO_ROLLBACK) */

long limeRand(void)
{
    rand_seed = (int32_t)((uint32_t)rand_seed * 1103515245u + 12345u);
    return (rand_seed / 0x10000) % 0x8000;
}

void limeMemoryReport(const char *tag) { (void)tag; }

/* ------------------------------------------------------------- save files
 *
 * Read off the binary (`python tools/cd.py limeLoadSaveFile limeWriteFile`):
 *
 *   limeLoadSaveFile(name)   NSData dataWithContentsOfFile:
 *                            "<home>/Documents/<name>"; nil -> log "*** Load
 *                            failed: %s" and return NULL; else malloc(length),
 *                            getBytes:, log "*** Loaded %s, of size %d bytes"
 *                            and return the copy (the caller limeFree()s it).
 *   limeWriteFile(name, data, size, flags)
 *                            dataWithBytes:length: then writeToFile:atomically:
 *                            YES; returns that BOOL. No caller reads it.
 *
 * One argument to the loader, not two: all the call sites pass only the name.
 * NULL is the "no save yet" path the game handles -- Reset_SaveData runs and
 * the tower starts empty.
 *
 * `Documents/` becomes UMK3_SAVE_DIR if set, else `save/` beside the exe on
 * Windows and $XDG_DATA_HOME/umk3 (~/.local/share/umk3) elsewhere. The
 * directory is made on the first write.
 */
#ifdef _WIN32
#include <direct.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define lime_mkdir(p) _mkdir(p)

#else
#include <sys/stat.h>
#define lime_mkdir(p) mkdir((p), 0755)
#endif

void *limeMalloc(const char *tag, size_t bytes);

/* limeLog is EMPTY in the shipped binary (push the varargs, pop, return), so
 * nothing above was ever printed on device. UMK3_LOG=1 prints it here. */
void limeLog(const char *fmt, ...)
{
    va_list ap;
    const char *on = getenv("UMK3_LOG");

    if (!on || !*on)
        return;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
}

static void save_dir(char *out, size_t n);

/* save/<file> beside the exe (or UMK3_SAVE_DIR): for the port's own small
 * files, such as the colour picker's choice. */
void port_save_path(char *out, size_t n, const char *file)
{
    char dir[1024];

    save_dir(dir, sizeof dir);
    lime_mkdir(dir);
#ifdef _WIN32
    snprintf(out, n, "%s\\%s", dir, file);
#else
    snprintf(out, n, "%s/%s", dir, file);
#endif
}

static void save_dir(char *out, size_t n)
{
    const char *e = getenv("UMK3_SAVE_DIR");

    if (e && *e) {
        snprintf(out, n, "%s", e);
        return;
    }
#ifdef _WIN32
    /* Beside the exe, in `save/`: the game folder holds the build, its `res`
     * and its saves, and nothing lands in %APPDATA%. */
    {
        DWORD len = GetModuleFileNameA(NULL, out, (DWORD)n);
        char *slash = (len > 0 && len < n) ? strrchr(out, '\\') : NULL;

        if (slash == NULL)
            snprintf(out, n, "save");
        else
            snprintf(slash + 1, n - (size_t)(slash + 1 - out), "save");
    }
#else
    e = getenv("XDG_DATA_HOME");
    if (e && *e)
        snprintf(out, n, "%s/umk3", e);
    else {
        e = getenv("HOME");
        snprintf(out, n, "%s/.local/share/umk3", (e && *e) ? e : ".");
    }
#endif
}

/* Make the save directory and its parents; existing ones are fine. */
static void make_dirs(char *path)
{
    char *p;

    for (p = path + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            char c = *p;
            if (p[-1] == ':')           /* "C:/" is the drive, not a directory */
                continue;
            *p = 0;
            lime_mkdir(path);
            *p = c;
        }
    }
    lime_mkdir(path);
}

void *limeLoadSaveFile(const char *name)
{
    char dir[600], path[800];
    FILE *f;
    long n;
    void *buf;

    save_dir(dir, sizeof dir);
    snprintf(path, sizeof path, "%s/%s", dir, name);

    f = fopen(path, "rb");
    if (f == NULL) {
        limeLog("*** Load failed: %s\n", name);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = limeMalloc("savefile", n > 0 ? (size_t)n : 1);
    if (buf == NULL || (n > 0 && fread(buf, 1, (size_t)n, f) != (size_t)n)) {
        fclose(f);
        limeFree(buf);
        limeLog("*** Load failed: %s\n", name);
        return NULL;
    }
    fclose(f);
    limeLog("*** Loaded %s, of size %d bytes\n", name, (int)n);
    return buf;
}

int limeWriteFile(const char *name, const void *data, long size, long flags)
{
    char dir[600], path[800], tmp[820];
    FILE *f;
    int ok;

    (void)flags;
    save_dir(dir, sizeof dir);
    make_dirs(dir);
    snprintf(path, sizeof path, "%s/%s", dir, name);
    snprintf(tmp, sizeof tmp, "%s.tmp", path);

    /* atomically:YES -- a crash mid-write leaves the old file, not half of
     * the new one */
    f = fopen(tmp, "wb");
    if (f == NULL)
        return 0;
    ok = size <= 0 || fwrite(data, 1, (size_t)size, f) == (size_t)size;
    ok = (fclose(f) == 0) && ok;
    if (!ok) {
        remove(tmp);
        return 0;
    }
    remove(path);                       /* rename() does not replace on Windows */
    if (rename(tmp, path) != 0) {
        remove(tmp);
        return 0;
    }
    return 1;
}


/* --------------------------------------------------------- iOS-only chrome */

/* Modals, the spinner, vibration and the App Store link. All of them were UIKit
 * on the device and none has a host equivalent worth inventing. The two modals
 * answer "no", which is the branch that does not navigate anywhere. */

/* The two system dialogs. Both are blocking in the binary: they hand the
 * question to +[modalAlert askFull:textOK:textCANCEL:] / infoFull:textOK:
 * (gamecode/modalAlert.m), which spins a CFRunLoop until
 * -[modalAlertDelegate alertView:clickedButtonAtIndex:] records the button and
 * stops it; askFull/infoFull return 1 for button 0 and 0 otherwise. Neither
 * function reads an argument -- the texts are fixed GameText ids, joined
 * with the "%@\n%@" CFString at 0x0017e424:
 *
 *   limeModalAreYouSure  0x000655e4  GameText 0x3bc + 0x11b, OK 0xc, CANCEL 0x58
 *   limeModalNoInternet  0x00065744  GameText 0x3b5 + 0x3b6, OK 0xc
 *
 * GameText is UTF-16 (strLenUnicode counts two-byte units). A headless build
 * has no one to ask, and answers no. */
const char *GameTextNoHeader(long id);

#ifdef UMK3_REAL_GL
static long ask(long a, long b, long ok, long cancel)
{
    unsigned short msg[1024];
    const unsigned short *p;
    size_t n = 0;

    for (p = (const unsigned short *)GameTextNoHeader(a); p && *p && n < 1000; )
        msg[n++] = *p++;
    msg[n++] = '\n';
    for (p = (const unsigned short *)GameTextNoHeader(b); p && *p && n < 1022; )
        msg[n++] = *p++;
    msg[n] = 0;

    return plat_ask(msg, (const unsigned short *)GameTextNoHeader(ok),
                    cancel ? (const unsigned short *)GameTextNoHeader(cancel)
                           : NULL) == 0;
}

long limeModalAreYouSure(void) { return ask(0x3bc, 0x11b, 0xc, 0x58); }
void limeModalNoInternet(void) { (void)ask(0x3b5, 0x3b6, 0xc, 0); }
#else
long limeModalAreYouSure(void) { return 0; }
void limeModalNoInternet(void) { }
#endif
void limeStartLoadingAnim(void)            { }
void limeStopLoadingAnim(void)             { }
void limeSetVibrate(void)                   { }
void limeLoadURLInternal(const char *url)  { (void)url; }
/* armv7 0x000669ec: the PVR texture array, TextureDups cleared, and both
 * frame clocks set to now so the first limeBegin sees no elapsed time. The
 * texture bookkeeping lives in draw_gl.c / lime_platform.c. */
void limeInit(void)
{
    currenttime = lasttime = g_clock;
}
