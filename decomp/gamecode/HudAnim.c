/*
 * HudAnim.c — src/gamecode/HudAnim.cpp
 *
 * The HUD's animated overlay: the "TOASTY!", "FLAWLESS VICTORY" and friends
 * that drop in over the fight. Hand-written from the disassembly of the armv7
 * slice.
 *
 * Everything here goes through one global, `_TheHud` at 0x00371458, and the two
 * functions below only ever touch its first three words. Their offsets are what
 * the code uses; the struct is spelled out rather than guessed at beyond that.
 */

#include <stdint.h>
#include "lime_glapi.h"   /* the GL calls' calling convention */

typedef struct TEXTURE TEXTURE;
typedef struct TEXTURETOLOAD {
    const char *P32 name;
    TEXTURE   *P32 *P32 dest;
} TEXTURETOLOAD;

void FreeSomeTextures(TEXTURETOLOAD *list);

/* HUDANIM is an enum passed by value in r0 — `7HUDANIM` in the mangled name is
 * a seven-character type, not a pointer, so it arrives as an int. */
typedef int HUDANIM;

/* `_TheHud` — 0x00371458. Only three words are reached from these two
 * functions, and they are named for what each is used as rather than for
 * anything the symbol table says, because it says nothing. */
typedef struct HUD {
    int     active;                 /* 0x00  zero means there is no HUD yet */
    HUDANIM anim;                   /* 0x04  the animation to play */
    float   timer;                  /* 0x08  a FLOAT -- see HUDANIM_Update */
} HUD;

extern HUD TheHud;                          /* 0x00371458 */
extern TEXTURETOLOAD HUDANIM_ttl[];         /* 0x00174e4c */


/* ------------------------------------------------------- HUDANIM_TriggerAnim
 *
 * armv7 0x0007dbe0, 20 bytes.  **Complete.**
 *
 *      ldr  r2, =_TheHud
 *      ldr  r3, [r2]
 *      cbz  r3, out            <- nothing happens without an active HUD
 *      movs r3, #0
 *      str  r0, [r2, #4]       <- anim
 *      str  r3, [r2, #8]       <- timer
 *
 * The guard is worth keeping rather than tidying away: a trigger arriving
 * before the HUD exists is DROPPED, not queued. Any port that queues it changes
 * behaviour at exactly the moments this is called — round start, first hit.
 */
void HUDANIM_TriggerAnim(HUDANIM anim)
{
    if (TheHud.active == 0)
        return;                     /* cbz r3 */

    TheHud.anim  = anim;
    TheHud.timer = 0.0f;
}


/* ----------------------------------------------------------- HUDANIM_Destroy
 *
 * armv7 0x0007dc3c, 36 bytes.  **Complete.**
 *
 *      ldr r4, =_TheHud
 *      ldr r3, [r4]
 *      cbz r3, out
 *      ldr r0, =_HUDANIM_ttl
 *      bl  _FreeSomeTextures
 *      movs r3, #0
 *      str r3, [r4, #4]        <- anim cleared BEFORE active
 *      str r3, [r4]
 *
 * Note it clears `+4` and then `+0`, in that order. Both are zeroed either way,
 * so the order does not matter here — but it is transcribed as written rather
 * than reordered, because the only reason to reorder it would be taste.
 */
void HUDANIM_Destroy(void)
{
    if (TheHud.active == 0)
        return;

    FreeSomeTextures(HUDANIM_ttl);
    TheHud.anim   = 0;
    TheHud.active = 0;
}


void LoadSomeTextures(TEXTURETOLOAD *list);


/* ------------------------------------------------------------- HUDANIM_Init
 *
 * armv7 0x0007dc60, 40 bytes.  **Complete.**
 *
 *      ldr  r3, [_TheHud]
 *      cbnz r3, skip               <- textures only when not already up
 *      bl   _LoadSomeTextures(_HUDANIM_ttl)
 *   skip:
 *      movs r2, #1
 *      str  r2, [_TheHud]          <- active
 *      subs r2, #1
 *      str  r2, [_TheHud, #4]      <- anim = 0
 *
 * The texture load is guarded but the two stores are not, so calling this twice
 * loads once and resets the animation both times. `subs r2, #1` producing the
 * zero is the compiler reusing the 1 it already had, not a second constant.
 */
void HUDANIM_Init(void)
{
    if (TheHud.active == 0)
        LoadSomeTextures(HUDANIM_ttl);

    TheHud.active = 1;
    TheHud.anim   = 0;
}


extern void *Scene_FIGHT;               /* 0x00183d60, one word, via a slot */


/* ------------------------------------------------------------ HUDANIM_Update
 *
 * armv7 0x0007dbf4, 68 bytes.  **Complete.**
 *
 *      if (!TheHud.active) return
 *      if (!TheHud.anim)   return
 *      TheHud.timer += 1.0f
 *      if (TheHud.timer >= (float)(Scene_FIGHT->count2 - 2))
 *          TheHud.anim = 0
 *
 * **The timer at +8 is a FLOAT.** `vldr s12, [r2, #8]`, `vadd.f32` with the
 * immediate 1.0, `vstr` back. This file had it as an int, which compiles, runs,
 * and is wrong: the comparison against the scene length is a float compare
 * (`vcmpe.f32`), and an integer timer would still terminate but by a different
 * rule.
 *
 * The length comes from the FIGHT scene's own `+0x44`, which
 * decomp/lime/lime.h names `count2` -- the modulus LIME_TriggerEventsFromScene
 * uses. **Minus two**, so the animation is cleared two frames before the scene
 * would wrap, not at the end.
 *
 * That -2 is the kind of constant a port drops as an off-by-one. It is not one.
 */
void HUDANIM_Update(void)
{
    float limit;

    if (TheHud.active == 0)
        return;
    if (TheHud.anim == 0)
        return;

    TheHud.timer += 1.0f;

    limit = (float)(((const long *)Scene_FIGHT)[0x44 / 4] - 2);
    if (TheHud.timer >= limit)
        TheHud.anim = 0;
}


extern void *MeshSet_FIGHT;             /* 0x00183d5c, one word, via a slot */
extern int   SceneRenderAlwaysTrans;    /* 0x00171760, defined in RenderScene.c */
extern float  finishsize;               /* 0x00175184 */

/* Mesh name -> the TEXTURE * variable to draw it with, ended by a NULL name
 * (LIME_SetSceneTextures 0x5f07c walks it 8 bytes a step). All three tables
 * name the sixteen meshes Fight01..Fight16 of FIGHT.meshset and point every
 * one at the same entry of HUDTextures: [0] FIGHT, [1] FINISH HER, [2]
 * FINISH HIM -- the order HUDANIM_ttl loads them in. */
typedef struct MESHANDTEXTURE {
    const char *P32 mesh;                   /* 0x00 */
    TEXTURE   *P32 *P32 texture;                /* 0x04 */
} MESHANDTEXTURE;

extern MESHANDTEXTURE Fight_MeshAndTexture[];      /* 0x00174eec */
extern MESHANDTEXTURE Finishher_MeshAndTexture[];  /* 0x00174ff4 */
extern MESHANDTEXTURE Finishim_MeshAndTexture[];   /* 0x001750fc */
/* 0x00371464 in __common: FIGHT, FINISH HER, FINISH HIM, filled by
 * LoadSomeTextures(HUDANIM_ttl). Only the tables name it. */
extern TEXTURE *P32 HUDTextures[];
extern TEXTURE *HSceneTextures[];       /* 0x006bc0a0, one per FIGHT mesh */

extern long GameMode;                   /* 0x0014faa4 */

void LIMEDS_SetCameraOrientation(float ex, float ey, float ez,
                                 float tx, float ty, float tz,
                                 float ux, float uy, float uz);
void LIME_SetSceneTextures(void *meshset, const MESHANDTEXTURE *table,
                           TEXTURE **out);
void LIME_RenderSceneOverrideTextures(void *scene, void *textures, long frame);
void limeDisableDepthTest(void);
void limeDisableDepthWrites(void);
void limeEnableDepthTest(void);
void limeEnableDepthWrites(void);
void LIME_GLAPI glPushMatrix(void);
void LIME_GLAPI glPopMatrix(void);
void LIME_GLAPI glRotatef(float a, float x, float y, float z);
void LIME_GLAPI glScalef(float x, float y, float z);


/* ------------------------------------------------------------ HUDANIM_Render
 *
 * armv7 0x0007dc90, 308 bytes.  **Complete.**
 *
 * Draws the "FIGHT" / "FINISH HIM" / "FINISH HER" overlay.
 *
 * ### The camera is three literal vectors
 *
 *      eye    (0, -5, 0)        __ZZ14HUDANIM_RendervE4C.11
 *      target (0,  0, 0)        __ZZ14HUDANIM_RendervE4C.10
 *      up     (0,  0, 1)        __ZZ14HUDANIM_RendervE4C.12
 *
 * Five units back along -Y, looking at the origin, Z up. That is a complete,
 * self-contained camera for the overlay and it does not depend on the game
 * camera at all -- **the orientation is set before the early-outs**, so even a
 * frame that draws nothing still moves the camera.
 *
 * ### It answers who writes SceneRenderAlwaysTrans
 *
 *      SceneRenderAlwaysTrans = 1
 *      LIME_RenderSceneOverrideTextures(Scene_FIGHT, HSceneTextures, timer)
 *      SceneRenderAlwaysTrans = 0
 *
 * That flag was an open question in decomp/lime/RenderScene.c -- the renderer
 * reads it and nothing found so far set it. **This is the writer**, and it is
 * the only kind of writer that matters: it raises the flag for one draw call
 * and lowers it immediately. So "always transparent" is not a mode the game
 * sits in; it is a property of the HUD overlay scene and nothing else.
 *
 * The animation timer is converted from its float to an int right at the call
 * (`vcvt.s32.f32`), which is the frame index into the FIGHT scene.
 *
 * ### An uninitialised register on an unknown animation
 *
 * The mesh is chosen by `TheHud.anim`: 1 FIGHT, 2 FINISH-HIM, 3 FINISH-HER.
 * Zero returns early. **Any other value falls through to the draw with the mesh
 * register never written** -- the compiler emitted no default. It cannot happen
 * as long as only HUDANIM_TriggerAnim writes the field, which is why it does
 * not crash; it is still worth a bounds check in a port rather than a faithful
 * copy of the omission.
 *
 * Depth test and depth writes are both off for the overlay and both restored.
 * The 90-degree X rotation stands the flat sign up to face the camera.
 */
void HUDANIM_Render(void)
{
    const MESHANDTEXTURE *mesh = 0;     /* the original leaves this unset */
    float s;

    LIMEDS_SetCameraOrientation(0.0f, -5.0f, 0.0f,   /* eye    */
                                0.0f,  0.0f, 0.0f,   /* target */
                                0.0f,  0.0f, 1.0f);  /* up     */

    if (GameMode == 2)
        return;
    if (TheHud.active == 0 || TheHud.anim == 0)
        return;

    if (TheHud.anim == 2)
        mesh = Finishim_MeshAndTexture;
    else if (TheHud.anim == 3)
        mesh = Finishher_MeshAndTexture;
    else if (TheHud.anim == 1)
        mesh = Fight_MeshAndTexture;
    /* no default -- see above */

    glPushMatrix();
    limeDisableDepthTest();
    limeDisableDepthWrites();

    glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
    s = finishsize;
    glScalef(s, s, s);

    LIME_SetSceneTextures(MeshSet_FIGHT, mesh, HSceneTextures);

    SceneRenderAlwaysTrans = 1;
    LIME_RenderSceneOverrideTextures(Scene_FIGHT, HSceneTextures,
                                     (long)TheHud.timer);
    SceneRenderAlwaysTrans = 0;

    glPopMatrix();
    limeEnableDepthTest();
    limeEnableDepthWrites();
}
