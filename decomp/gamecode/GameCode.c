/*
 * GameCode.c — src/gamecode/GameCode.cpp
 *
 * Top-level game state: camera, intro, and the texture sets that come and go
 * with it. Seventy-nine functions in the original; these are the small
 * self-contained ones, hand-written from the disassembly of the armv7 slice and
 * verified against the oracle by tests/test_gamecode_diff.c.
 */

#include <stdint.h>
#include "lime_glapi.h"   /* the GL calls' calling convention */
#include <string.h>   /* memset, for clearSpriteListsAndEvents */

extern int PortTextureQuality;          /* runtime/draw_gl.c, port only */
int  lime_port_load_more(int first);    /* runtime/draw_gl.c, port only */
void lime_port_load_done(const char *what);

/* TEXTURETOLOAD is the engine's {name, destination} pair — see decomp/lime,
 * where LIME_SetSceneTextures walks the same shape. Declared rather than
 * included: gamecode has no header of its own yet and one file should not
 * invent the project's include graph. */
/* Completed from decomp/lime/lime.h, which settles the two fields gamecode
 * looks at. +0x40 is the GL texture name, and it is how the loading screen
 * asks "has this reached the GPU yet". */
typedef struct TEXTURE {
    uint8_t  _pad00[0x40];
    unsigned name;               /* 0x40  the GL texture name */
    uint8_t  _pad44[0x0c];
    int      field50;            /* 0x50 */
} TEXTURE;
typedef struct TEXTURETOLOAD {
    const char *name;            /* 0x00  NULL ends the list */
    TEXTURE   **dest;            /* 0x04  where the handle goes */
} TEXTURETOLOAD;

/* The engine's font object, laid out to match FONT in decomp/lime/lime.h.
 * GameCode.cpp reaches these by ADDRESS -- `add r0, pc` and straight into the
 * call, with no load in between -- so the C has to pass `&GameFont`, not
 * `GameFont`. An earlier `extern void *NameFont;` here dereferenced once too
 * many and would have drawn every debug line with the first word of the struct
 * as its font pointer. Only the fields gamecode touches are named; lime.h has
 * the rest. (`GameFontP` further down is a different thing: the pointer SLOT
 * other translation units use to find the same object.) */
/* The same fields as lime.h's `FONT`, and it has to stay that way:
 * `limeCreateFONT` writes through a `FONT *` and this reads the result.
 * Copied rather than included because GameCode.c declares a dozen lime
 * types and functions of its own and the header collides with them --
 * unifying those is the real fix and a larger one.
 *
 * It was previously written as padding plus three image offsets, which
 * put `codesW` at 0x4c. Six pointers sit before it, four bytes each in
 * the image and eight here, so 0x4c is not where it lands. */
typedef struct GAMEFONT {
    uint8_t   _pad00[4];
    int       simple;            /* 0x04  stored INVERTED from the file flag */
    int       glyphHeight;       /* 0x08  header byte 2 -- ONE height for every
                                  *       glyph, which is why it is in the
                                  *       header and not in a per-glyph array */
    int       spacing;           /* 0x0c  added once per character by both
                                  *       width routines -- inter-character
                                  *       spacing, set from limeCreateFONT */
    int       fallbackAdvance;   /* 0x10  the constant 8 */
    /* A FLOAT. Both width routines read it with `vldr s15, [r4, #0x14]`, which
     * reinterprets the bits -- it does not convert an integer. An earlier
     * version of this header typed it `int` and the clean C cast it, which
     * gives the right answer only when the caller happened to pass a small
     * whole number. */
    float     field14;           /* 0x14  the scale applied at measure time */
    int16_t   numGlyphs;         /* 0x18 */
    uint8_t   _pad1a[2];
    /* The atlas rectangle of each glyph. limeDrawFONT settles all three by what
     * it divides them BY on the way to limeDrawSprite: atlasU and glyphWidth are
     * both normalised by +0x34, atlasV by +0x38. Two share a divisor, so two
     * share an axis. */
    int16_t  *atlasU;            /* 0x1c  x position in the atlas */
    int16_t  *atlasV;            /* 0x20  y position in the atlas */
    int16_t  *glyphWidth;        /* 0x24  per-glyph width, and the advance
                                  *       limeGetStringWidth accumulates */
    /* Optional per-glyph kerning, one SIGNED byte each, added to the width when
     * the pointer is non-null. limeGetStringWidth is the only reader:
     *     ldr     r3, [r4, #0x28]
     *     cmp     r3, #0
     *     ldrsbne r3, [r3, r0]
     *     addne   r8, r8, r3
     * Nothing in limeCreateFONT was seen to allocate it, so a font that does not
     * carry kerning leaves it null and every glyph keeps its plain width. */
    int8_t   *kerning;           /* 0x28 */
    int       defaultAdvance;    /* 0x2c  simple fonts only */
    uint8_t   _pad30[4];
    /* The atlas dimensions, and they are the OPPOSITE way round from an earlier
     * naming here. +0x34 divides the horizontal metrics and +0x38 the vertical,
     * which is what fixes them: the divisor names the axis. */
    float     atlasWidth;        /* 0x34 */
    float     atlasHeight;       /* 0x38 */
    /* Added to the width of a character that is NOT in the table, on top of
     * the fallback advance. Only the not-found branch reads it:
     *     ldr r3, [r4, #0x10]   ; the fallback
     *     ldr r2, [r4, #0x3c]   ; and this
     *     add r3, r3, r2
     * so a font can make unknown characters wider than the plain fallback
     * without touching the fallback itself. */
    int       extraUnknown;      /* 0x3c */
    /* limeDrawFONT (armv7 0x7e5b8): at the +0x44-th character not in the
     * table, +0x40 more is added. Both are zeroed by every call that is not
     * alignment 3. The earlier 12-byte pad here put `codes` at 0x4c. */
    int       extraAtCount;      /* 0x40 */
    int       countForExtra;     /* 0x44 */
    uint8_t  *codes;             /* 0x48  one byte per glyph */
    int16_t  *codesW;            /* 0x4c  the same codes widened to 16 bits */
    TEXTURE  *texture0;          /* 0x50 */
    TEXTURE  *texture1;          /* 0x54 */
} GAMEFONT;

void LoadSomeTextures(TEXTURETOLOAD *list);
void FreeSomeTextures(TEXTURETOLOAD *list);

/* Real globals, named in the symbol table and reached PC-relative. */
extern int LockCamera;                  /* 0x00150e8c */
extern int AxeTrailDisallowed;          /* 0x00150eac */
extern TEXTURETOLOAD BloodTexturesToLoad[];  /* 0x00150e1c */
extern int RenderSettings[2];           /* 0x001ab678, two words */

/* The game state. `getTransferableFlags` reaches it through a pointer slot at
 * 0x000f357c holding 0x0038c1fc, which is exactly where other.c independently
 * places `G`. Only the one field this file reads is named. */
typedef struct GAMESTATE {
    uint8_t _pad000[0x368];
    /* The two health BARS, and they are the health scaled by 1.66 -- see
     * ResetFightData, which writes 166/83/41 beside health 100/50/25 and
     * computes the survival one as `Health[0] * 166 / 100`. */
    long    healthBar1;                 /* 0x368 */
    long    healthBar2;                 /* 0x36c */
    uint8_t _pad370[0x44e - 0x370];
    int16_t field44e;                   /* 0x44e  ldrsh, normalised to 0 or 1 */
} GAMESTATE;
extern GAMESTATE *G;                    /* 0x0038c1fc */

/* Only the two fields JadeStomachShaker reads are named. */
typedef struct PLAYER {
    uint32_t field00;                   /* 0x00  must be 0x10 */
    uint8_t  _pad04[0x10];
    uint32_t field14;                   /* 0x14  must land in 0x23..0x29 */
} PLAYER;

extern char OpponentTowerList[];         /* 0x0014fcb4 */

int  limeWriteFile(const char *path, const void *data, long size, long flags);
void Write_Tower(void);

int  JadeStomachShaker(PLAYER *p);
void DeviceRenderSettings(void);
int  getTransferableFlags(void);

void AllowCameraTracking(void);
void StopCameraTracking(void);
void EndIntro(void);
void LoadBloodTextures(void);
void FreeBloodTextures(void);


/* ------------------------------------------------- camera tracking, the pair
 *
 * armv7 0x0001c41c and 0x0001c40c, 16 bytes each.
 *
 *      ldr r3, [pc, #8] ; movs r2, #N ; add r3, pc ; str r2, [r3] ; bx lr
 *
 * Both PC-relative computations land on **the same address**, 0x00150e8c,
 * which the symbol table calls `_LockCamera`. So this is one flag with two
 * named setters rather than two flags.
 *
 * The polarity reads backwards for a moment and is right: *allowing* tracking
 * clears the lock, *stopping* it sets the lock. The function names describe
 * the camera, the variable describes the constraint.
 */
void AllowCameraTracking(void)
{
    LockCamera = 0;
}

void StopCameraTracking(void)
{
    LockCamera = 1;
}


/* ------------------------------------------------------------------ EndIntro
 *
 * armv7 0x0001c664, 16 bytes.  `__Z8EndIntrov`
 *
 * The same shape, writing **10** to 0x00150eac — an exact symbol-table match
 * for `_AxeTrailDisallowed`.
 *
 * Ten is not a boolean, and a name ending in "Disallowed" taking the value ten
 * does not read as sensible. The address matches a named symbol exactly, so
 * this is not a nearest-neighbour guess; something else about the variable is
 * not what its name suggests. Written as the machine writes it, with the
 * oddity left standing rather than smoothed over by picking a nicer story.
 *
 * Whoever finds the reader of this variable will settle it in one step.
 */
void EndIntro(void)
{
    AxeTrailDisallowed = 10;
}


/* ------------------------------------------------------- the blood textures
 *
 * armv7 0x00023250 and 0x00022c60, 20 bytes each.
 *
 *      push {r7, lr} ; ldr r0, [pc, #8] ; add r0, pc ; bl ... ; pop {r7, pc}
 *
 * Both resolve to 0x00150e1c, `_BloodTexturesToLoad`, and hand it to the
 * engine's generic pair. The list itself is data in the slice; these two are
 * only the load and free sides of it, which is why they are twenty bytes each
 * and why they are worth having early: everything about blood rendering hangs
 * off a list this project can now name.
 */
void LoadBloodTextures(void)
{
    LoadSomeTextures(BloodTexturesToLoad);
}

void FreeBloodTextures(void)
{
    FreeSomeTextures(BloodTexturesToLoad);
}


/* -------------------------------------------------- DeviceRenderSettings
 *
 * armv7 0x0001b590, 16 bytes.  `__Z20DeviceRenderSettingsv`
 *
 *      ldr r3, [pc, #8] ; movs r2, #0 ; add r3, pc
 *      str r2, [r3] ; str r2, [r3, #4] ; bx lr
 *
 * Two consecutive words zeroed at 0x001ab678, `_RenderSettings`. Two words and
 * not one -- a body that cleared only the first would agree with the original
 * on every test that never read the second, which is why the test checks the
 * SPAN that moved rather than the value at one address.
 *
 * What the two words mean is not established here. Nothing in this function
 * reads them back and nothing else decompiled so far touches them.
 */
void DeviceRenderSettings(void)
{
    RenderSettings[0] = 0;
    RenderSettings[1] = 0;
}


/* -------------------------------------------------- getTransferableFlags
 *
 * armv7 0x0001c77c, 24 bytes.
 *
 *      ldr    r3, [pc, #0x10] ; add r3, pc ; ldr r3, [r3]
 *      ldrsh.w r0, [r3, #0x44e]
 *      subs   r0, #0 ; it ne ; movne r0, #1
 *      bx     lr
 *
 * The PC-relative computation lands on a POINTER SLOT at 0x000f357c, and what
 * the slot holds is 0x0038c1fc -- `_G`, the game state. That is the same
 * global `other.c` reaches for `SwitchQueue`, arrived at from a different
 * function by a different route, which is the standard this project asks for
 * before a name is treated as established.
 *
 * The field is a SIGNED halfword at +0x44e and the return is normalised to
 * 0 or 1, so the caller gets a boolean and the storage is not one. Both halves
 * matter: `ldrsh` sign-extends, and a value like 0xFFFF is non-zero either way
 * but reads as -1 rather than 65535 if anything ever compares it.
 *
 * Note the `it ne`. Instructions inside a Thumb IT block do not update the
 * flags, and recomp.py used to emit one that did -- see docs/RENDERMESH-DRAW.md.
 * This function is a small direct check that the fix holds.
 */
int getTransferableFlags(void)
{
    return (G->field44e != 0) ? 1 : 0;
}


/* --------------------------------------------------- JadeStomachShaker
 *
 * armv7 0x0001c64c, 24 bytes.  `__Z17JadeStomachShakerP6PLAYER`
 *
 *      ldr  r3, [r0]        ; cmp r3, #0x10 ; bne -> return 0
 *      ldr  r0, [r0, #0x14]
 *      subs r0, #0x23
 *      cmp  r0, #6
 *      ite  hi ; movhi r0, #0 ; movls r0, #1
 *
 * Two gates: the field at +0x00 must be exactly 0x10, and then +0x14 must fall
 * in **0x23..0x29 inclusive**.
 *
 * The range test is the subtract-and-compare-unsigned idiom, and the bound is
 * inclusive because the predicate is `ls` and not `lo`. Writing `< 6` instead
 * of `<= 6` loses the last value in the range -- one character state out of
 * seven, which is exactly the kind of thing that ships.
 *
 * Unsigned also matters: a value below 0x23 wraps to something enormous rather
 * than going negative, so the single comparison catches both ends. A signed
 * version would let everything under 0x23 through.
 *
 * Another `ite`, so another small check that the IT-block flag fix holds.
 */
int JadeStomachShaker(PLAYER *p)
{
    unsigned d;

    if (p->field00 != 0x10)
        return 0;

    d = (unsigned)p->field14 - 0x23u;
    return (d <= 6u) ? 1 : 0;
}


/* ---------------------------------------------------------------- Write_Tower
 *
 * armv7 0x00023818, 32 bytes.
 *
 *      ldr r1, [pc, #0x10] ; add r1, pc     ; -> _OpponentTowerList
 *      ldr r0, [pc, #0x14] ; add r0, pc     ; -> "towerdata"
 *      movs r2, #0xb0 ; movs r3, #0
 *      bl  _limeWriteFile
 *
 * Saves the tower ladder: 0xb0 bytes of `_OpponentTowerList` to a file called
 * "towerdata". The name is a string in the slice, not a constructed path, so
 * there is no directory involved -- whatever `limeWriteFile` prefixes is the
 * whole of the location policy.
 *
 * 0xb0 is 176 bytes. Nothing here says how many entries that is.
 */
void Write_Tower(void)
{
    limeWriteFile("towerdata", OpponentTowerList, 0xb0, 0);
}


extern TEXTURE *LoadingTexture;         /* 0x001abb94 */
void limeDeleteTexture(TEXTURE *tex);


/* ----------------------------------------------------------- GameCodeDestroy
 *
 * armv7 0x0001c778, 4 bytes: `bx lr`.
 *
 * Empty. The teardown counterpart to GameCodeInit, and the shipped game never
 * tears down -- iOS 3 apps were killed, not closed. Written out so the file's
 * count matches the binary rather than silently skipped.
 */
void GameCodeDestroy(void)
{
}


/* ---------------------------------------------------------- RemapKicksPunches
 *
 * armv7 0x0001b7a0, 4 bytes: `bx lr`.
 *
 * Empty, and it takes an argument it never reads. The name and the signature
 * are all that survive of a button-remapping hook; whatever it did lives in the
 * settings code now.
 *
 * **It returns its argument, and something depends on that.** `bx lr` leaves r0
 * exactly as it arrived, and `GetArcadeJoyBits` below tail-calls this and
 * returns immediately -- so the value that reaches its caller is the bits it
 * passed in. Declared as returning `long` for that reason: writing it `void`
 * compiles and hides the only thing this stub still does.
 */
long RemapKicksPunches(long arg)
{
    return arg;                         /* bx lr -- r0 is untouched */
}


/* -------------------------------------------------- DeleteLoadingScreenTexture
 *
 * armv7 0x0001c800, 36 bytes.  **Complete.**
 *
 *      ldr  r0, =_LoadingTexture
 *      ldr  r0, [r0]
 *      cbz  r0, skip
 *      bl   _limeDeleteTexture
 *   skip:
 *      ldr  r3, =_LoadingTexture
 *      movs r2, #0
 *      str  r2, [r3]
 *
 * The pointer is cleared on BOTH paths -- the store is past the cbz target, not
 * inside the branch. So calling this twice is safe, and calling it on a texture
 * that was never loaded is safe, which is what lets the loading screen be torn
 * down from more than one place.
 */
void DeleteLoadingScreenTexture(void)
{
    if (LoadingTexture != 0)
        limeDeleteTexture(LoadingTexture);
    LoadingTexture = 0;
}


extern float ShakeOffset[3];            /* 0x001f44bc */


/* ---------------------------------------------------------- CalcShakeOffset
 *
 * armv7 0x0001c620, 36 bytes.  **Complete.**
 *
 *      rsb.w r0, r0, #0            <- NEGATED first
 *      vcvt.f32.s32
 *      vdiv.f32 by 100.0
 *      str 0 -> [+0]
 *      str 0 -> [+4]
 *      str   -> [+8]
 *
 * The screen shake is **one axis only**: x and y are zeroed and the value goes
 * to z. And the input is negated before the divide, so a positive argument
 * pushes the offset negative.
 *
 * Both details are easy to lose. A port that writes all three components, or
 * that drops the `rsb`, produces a shake that looks plausible and is wrong in a
 * direction nobody notices until it is compared side by side.
 */
void CalcShakeOffset(long amount)
{
    ShakeOffset[0] = 0.0f;
    ShakeOffset[1] = 0.0f;
    ShakeOffset[2] = (float)(-amount) / 100.0f;
}


extern char mpSpriteList[0x140];        /* 0x001ab680 */
extern char mpEventQueue[0x1b0];        /* 0x001ab7c0 */


/* --------------------------------------------------- clearSpriteListsAndEvents
 *
 * armv7 0x00021ebc, 36 bytes.  **Complete.**
 *
 * Two memsets, and the sizes are literals: 0x140 and 0x1b0. Both buffers belong
 * to the multiplayer path -- they are the lists packSpriteList and
 * setNextSpritesAndEvents fill -- and this is called from Task_GameInit and
 * GameInit_LoadABit, so they are cleared on every load whether or not a
 * multiplayer session exists.
 */
void clearSpriteListsAndEvents(void)
{
    memset(mpSpriteList, 0, 0x140);
    memset(mpEventQueue, 0, 0x1b0);
}


extern float WorldScaleAdjust;          /* 0x0014df9c = 64.0 */
extern float CamOverridePos[3];         /* 0x001ab000 */
extern int   OverrideCamera;            /* 0x0010dea8 */


/* ------------------------------------------------------ setTransferableFlags
 *
 * armv7 0x0001c794, 36 bytes.  **Complete.**
 *
 * Masks the argument to one bit and stores it as a HALFWORD at `G + 0x44e`.
 * Both branches store the same masked value -- the compiler split them only
 * because it had the constant 1 in a register on one side -- so this is a plain
 * `G->field44e = flags & 1`.
 *
 * getTransferableFlags, already in this file, reads the same halfword and
 * normalises it to 0 or 1 on the way out -- so the pair round-trips, and the
 * mask here is what makes that true rather than the read doing the work.
 */
void setTransferableFlags(long flags)
{
    G->field44e = (int16_t)(flags & 1);
}


/* ----------------------------------------------- SetCameraOverridePosFrom2d
 *
 * armv7 0x0001c42c, 44 bytes.  **Complete, and it uses one of its three
 * arguments.**
 *
 *      stm.w sp, {r0, r1, r2}          <- three words spilled
 *      vldr  s14, [sp]                 <- only the FIRST is read
 *      vdiv.f32 s14, s14, _WorldScaleAdjust     ; 64.0
 *      vstr  s14, [_CamOverridePos]
 *      movs  r2, #1
 *      str   r2, [_OverrideCamera]
 *
 * The spill of r0, r1 and r2 into three consecutive words is what a struct
 * passed by value looks like under this ABI, so the parameter is almost
 * certainly a `limeVECTOR3` rather than three floats -- but only x is read, y
 * and z are spilled and abandoned, and `_CamOverridePos` keeps whatever its
 * other two components already held.
 *
 * That is transcribed rather than tidied. A port that helpfully writes all
 * three components is not doing what the original does, and the name says the
 * input is a 2d position, so dropping components may well be deliberate.
 *
 * The divisor is `_WorldScaleAdjust`, 64.0 -- a second world-scale constant
 * alongside `_SceneScale` and `_PlayerSize`. What relates them is not
 * established.
 */
void SetCameraOverridePosFrom2d(float x, float y, float z)
{
    (void)y;
    (void)z;                            /* spilled to the stack, never read */

    CamOverridePos[0] = x / WorldScaleAdjust;
    OverrideCamera = 1;
}


extern int CamTrackToPlayer;            /* 0x0014dfc4 */
extern int IsInFinishing;               /* 0x00150cbc */
extern int *RoundParam;                 /* pointer slot -> 0x0038ed04 */
void limeInit(void);
void LIME_InitEventsManager(void);
void LIME_InitDebugWindow(void);
void LIME_LoadMasterEventOffsets(void);


/* -------------------------------------------------------------- GameCodeInit
 *
 * armv7 0x0001c7c0, 56 bytes.  **Complete.**
 *
 * The whole of the game's startup, and it is short: three engine inits, three
 * globals, and the master event offsets last.
 *
 * **`_CamTrackToPlayer` starts at -1, not 0.** Zero is a valid player index, so
 * the sentinel for "track nobody" has to be something else, and a port that
 * zero-initialises this instead has the camera following player 0 from the
 * first frame.
 *
 * `RoundParam` is reached through a pointer slot and only two of its fields are
 * touched here: +0x34 set to 1 and +0x38 cleared. They are not named because
 * nothing in this function says what they are.
 */
void GameCodeInit(void)
{
    limeInit();
    LIME_InitEventsManager();
    LIME_InitDebugWindow();

    CamTrackToPlayer = -1;              /* mov.w r2, #-1 */
    IsInFinishing    = 0;

    RoundParam[0x38 / 4] = 0;
    RoundParam[0x34 / 4] = 1;

    LIME_LoadMasterEventOffsets();
}


extern int JaxGrowCounter;              /* 0x0010ded8 */
extern void **GameObjects;              /* 0x0014dfec */
/* GameObjects' VALUE is the base of the fighters' 16-byte object records:
 * every reader loads the word at 0x14dfec once and indexes from it
 * (RunJaxGrowCounters 0x1c3d0 `ldr r2,[r3]; ldrh r3,[r2,#8]`,
 * GetReal6ButtonJoyBits 0x1e478/0x1e48a). It is never `GameObjects[0]`:
 * reading it twice faulted on the first fight frame after Task_GameInit
 * zeroed it. Declared `void **` only because the generated globals are. */


/* -------------------------------------------------------- RunJaxGrowCounters
 *
 * armv7 0x0001c3cc, 56 bytes.  **Complete.**
 *
 * **Both tests are the same test, written two different ways by the compiler.**
 *
 *      ldrh  r3, [r2, #8]
 *      add.w r3, r3, #0xe800
 *      adds  r3, #0x84
 *      uxth  r3, r3
 *      cmp   r3, #1
 *      bls   grow
 *
 * Adding 0xe884 and truncating to sixteen bits maps 0x177c to 0 and 0x177d to
 * 1, so `<= 1` after that is exactly `x == 0x177c || x == 0x177d`. The second
 * test, on the signed halfword at +0x18, spells the same pair out as two
 * compares because the value is signed and the trick does not apply.
 *
 * So: the counter advances when EITHER of two fields holds one of two adjacent
 * ids. 0x177c is 6012 -- an animation or frame number for Jax's grow, which the
 * function name says and nothing here confirms.
 *
 * A port that reads the range check as a range has invented a rule the game
 * does not have.
 */
void RunJaxGrowCounters(void)
{
    const unsigned short *o = (const unsigned short *)(void *)GameObjects;

    if (o == 0)
        return;

    if (o[4] == 0x177c || o[4] == 0x177d) {         /* the halfword at +8 */
        JaxGrowCounter++;
        return;
    }
    {
        short s = ((const short *)o)[0x18 / 2];
        if (s == 0x177c || s == 0x177d)
            JaxGrowCounter++;
    }
}


extern int GameMode;                    /* 0x0014faa4 */
extern int Health[2];                   /* 0x0014fa64 */
int  isParentBasedOnSpeed(void);
void achievementsIncreaseMatchesWon(void);


/* ------------------------------------------------------------- updateMPWins
 *
 * armv7 0x000224d4, 48 bytes.  **Complete.**
 *
 *      if (GameMode != 1) return
 *      if (isParentBasedOnSpeed())  { if (Health[1]) return; }
 *      else                         { if (Health[0]) return; }
 *      achievementsIncreaseMatchesWon()
 *
 * **Each side watches the OTHER player's health, and which index that is
 * depends on the role.** The peer that `isParentBasedOnSpeed` calls the parent
 * reads Health[1]; the other reads Health[0]. Both then credit the same win.
 *
 * That asymmetry is the whole function. A port that picks one index for both
 * sides gives one player every multiplayer achievement and the other none, and
 * it would look correct in single-player testing because GameMode gates it.
 */
void updateMPWins(void)
{
    if (GameMode != 1)
        return;

    if (isParentBasedOnSpeed()) {
        if (Health[1] != 0)
            return;
    } else {
        if (Health[0] != 0)
            return;
    }
    achievementsIncreaseMatchesWon();
}


extern int    LoadedBGExtents;          /* 0x0015108c */
extern int    CurrentTask;              /* 0x00150590 */
extern void (*TaskFunctionList[])(void);/* 0x0017d940 */
void LoadBGExtents(void);
void limeBegin(void);
void limeFinish(void);
void limeStartLoadingAnim(void);
void limeStopLoadingAnim(void);
void heartbeatUpdate(void);
void LIME_GLAPI glDisable(unsigned cap);


/* -------------------------------------------------------- LIME_KillAllLights
 *
 * armv7 0x00023c24, 88 bytes.  **Complete.**
 *
 * Ten glDisable calls and nothing else: GL_LIGHTING (0xb50), all eight lights
 * GL_LIGHT0..GL_LIGHT7 (0x4000..0x4007), and GL_COLOR_MATERIAL (0xb57).
 *
 * **All eight lights are disabled explicitly**, not in a loop, though the enum
 * values are consecutive and a loop would have been equivalent. Worth noting
 * for a GL ES 2 port: none of this exists there, and the whole function becomes
 * a no-op rather than something to translate.
 */
void LIME_KillAllLights(void)
{
    glDisable(0x0b50);                  /* GL_LIGHTING */
    glDisable(0x4000);                  /* GL_LIGHT0 */
    glDisable(0x4001);
    glDisable(0x4002);
    glDisable(0x4003);
    glDisable(0x4004);
    glDisable(0x4005);
    glDisable(0x4006);
    glDisable(0x4007);                  /* GL_LIGHT7 */
    glDisable(0x0b57);                  /* GL_COLOR_MATERIAL */
}


/* -------------------------------------------------------------- GameCodeMain
 *
 * armv7 0x000232bc, 88 bytes.  **Complete. This is the frame.**
 *
 *      if (!LoadedBGExtents) { LoadBGExtents(); LoadedBGExtents = 1; }
 *      limeBegin()
 *      if (CurrentTask == 8) limeStartLoadingAnim()
 *      else                  limeStopLoadingAnim()
 *      TaskFunctionList[CurrentTask]()
 *      limeFinish()
 *      heartbeatUpdate()
 *
 * The whole game is one indirect call through a task table, and **task 8 is the
 * loading screen** -- the only index that gets the spinner started rather than
 * stopped, and the stop runs on every other frame whether or not one is
 * showing.
 *
 * The lazy LoadBGExtents happens INSIDE the frame rather than at init, and the
 * flag is set after the call, so a LoadBGExtents that failed would retry every
 * frame.
 */
void GameCodeMain(void)
{
    if (LoadedBGExtents == 0) {
        LoadBGExtents();
        LoadedBGExtents = 1;
    }

    limeBegin();

    if (CurrentTask == 8)
        limeStartLoadingAnim();         /* task 8 is the loading screen */
    else
        limeStopLoadingAnim();

    TaskFunctionList[CurrentTask]();

    limeFinish();
    heartbeatUpdate();
}



extern int PLAYER1MODEL;                /* 0x0014e1b4 */
extern int LastDestiny;                 /* 0x0014e210 */


/* ------------------------------------------------------- SaveUnclaimedTreasure
 *
 * armv7 0x00023838, 78 bytes.  **Complete.**
 *
 * Three separate four-byte files rather than one record:
 *
 *      "unclaimedtreasure"              <- the argument
 *      "unclaimedtreasure_playedas"     <- _PLAYER1MODEL
 *      "unclaimedtreasure_lastdestiny"  <- _LastDestiny
 *
 * Each write spills its value to the same stack slot with a pre-indexed
 * `str r3, [r1, #-0x4]!` and hands limeWriteFile the address, so the four bytes
 * on disk are the raw int in the device's byte order.
 *
 * **Three files means three chances to be left half-written.** A port that
 * merges them into one record changes the recovery behaviour of a save that is
 * interrupted, which is exactly the kind of thing this data exists to survive.
 */
void SaveUnclaimedTreasure(long treasure)
{
    long v;

    v = treasure;
    limeWriteFile("unclaimedtreasure", &v, 4, 0);

    v = PLAYER1MODEL;
    limeWriteFile("unclaimedtreasure_playedas", &v, 4, 0);

    v = LastDestiny;
    limeWriteFile("unclaimedtreasure_lastdestiny", &v, 4, 0);
}


#define LEVEL_INFO_STRIDE  0xf4
#define LEVEL_INFO_SLOTS   16
#define TOWER_TIERS        4
#define TOWER_PER_TIER     11

extern char  Level_Info[];              /* 0x0014e8d4 */
void  limeFree(void *p);
void *limeLoadFile(const char *name);
void *limeLoadSaveFile(const char *name);


/* ------------------------------------------------------------- LoadBGExtents
 *
 * armv7 0x00023264, 76 bytes.  **Complete.**
 *
 * Reads BGEXTENTS.BIN and scatters eight words per level into `_Level_Info`,
 * from file offsets +4..+0x20 to entry offsets 0x24, 0x28, 0x58, 0x5c, 0x2c,
 * 0x30, 0x60 and 0x64 -- **not in order**, so the file's layout and the
 * struct's are different and the mapping is the whole content of this function.
 *
 * **It independently confirms two numbers from GetNextLevel.** The entry stride
 * is 0xf4 -- 244, which GetNextLevel builds out of shifts -- and the walk stops
 * at Level_Info + 0xf40, which is 16 entries. Two functions in different files,
 * neither citing the other.
 *
 * The source cursor advances with a pre-indexed `ldr r2, [r3, #0x20]!`, so the
 * file record is 0x20 bytes and its first word is never read. The cursor is
 * r3, a copy (`mov r3, r0`); the buffer freed at the end is r0, the address
 * limeLoadFile returned. Freeing the advanced cursor instead hands limeFree a
 * pointer 512 bytes into its own block.
 *
 * A missing file is silent: `cbz r0` returns without touching Level_Info, so
 * whatever it already held stays.
 */
void LoadBGExtents(void)
{
    const long *file = (const long *)limeLoadFile("BGEXTENTS.BIN");
    const long *src = file;             /* mov r3, r0: the cursor is a copy */
    int i;

    if (src == 0)
        return;                         /* silent: Level_Info keeps its values */

    for (i = 0; i < LEVEL_INFO_SLOTS; i++) {
        long *e = (long *)(Level_Info + (long)i * LEVEL_INFO_STRIDE);

        e[0x24 / 4] = src[1];
        e[0x28 / 4] = src[2];
        e[0x58 / 4] = src[3];
        e[0x5c / 4] = src[4];
        e[0x2c / 4] = src[5];
        e[0x30 / 4] = src[6];
        e[0x60 / 4] = src[7];
        e[0x64 / 4] = src[8];
        src += 0x20 / 4;                /* the record, first word unread */
    }
    limeFree((void *)file);             /* r0, untouched by the loop */
}


/* ----------------------------------------------------------------- Load_Tower
 *
 * armv7 0x00023314, 92 bytes.  **Complete.**
 *
 * Four tiers of eleven entries, read from "towerdata" and **clamped on the way
 * in**: anything below 0 or above 0x19 becomes 1, not 0 and not a rejection of
 * the file.
 *
 *      if (v < 0 || v > 25) v = 1
 *
 * So a corrupt save produces a playable tower rather than an empty one, and the
 * clamp is per entry -- one bad value does not discard the rest.
 *
 * The destination stride is 44 bytes per tier, built as `i*12 - i` shifted, and
 * the source advances 0x2c per tier, so the file and the table have the same
 * shape. A missing file leaves the table untouched.
 *
 * The destination is `_OpponentTowerList` itself (literal 0x12c972 + pc
 * 0x23342 = 0x14fcb4), the table Write_Tower saves and DrawTower3D reads.
 * It used to fill a separate `TowerData` that nothing read, so a saved
 * ladder never reached the tower and every rung kept the image default,
 * character 16, on every cell.
 */
void Load_Tower(void)
{
    /* The loop walks a copy (`mov lr, r0`); r0, the pointer the loader
     * returned, is never touched and is what limeFree gets. Freeing the
     * advanced cursor instead handed the allocator an address 0xb0 into the
     * block, and with a saved tower on disk the game died loading its data. */
    const long *file = (const long *)limeLoadSaveFile("towerdata");
    const long *src = file;
    int tier, i;

    if (file == 0)
        return;

    for (tier = 0; tier < TOWER_TIERS; tier++) {
        for (i = 0; i < TOWER_PER_TIER; i++) {
            long v = src[i];

            if (v < 0 || v > 0x19)
                v = 1;                  /* clamped, not rejected */
            ((long *)OpponentTowerList)[tier * TOWER_PER_TIER + i] = v;
        }
        src += 0x2c / 4;
    }
    limeFree((void *)file);             /* r0, untouched by the loop */
}


extern char Language[10];               /* 0x001ab980 */
int strcmp(const char *a, const char *b);


/* ------------------------------------------------------------ compareLanguages
 *
 * armv7 0x0002389c, 92 bytes.  **Complete.**
 *
 *      p = limeLoadSaveFile("previouslanguage")
 *      if (!p) {
 *          if (!write) return 1
 *          limeWriteFile("previouslanguage", Language, 10, 0)
 *          return 1
 *      }
 *      if (write) limeWriteFile("previouslanguage", Language, 10, 0)
 *      r = strcmp(p, Language)
 *      limeFree(p)
 *      return r != 0
 *
 * **No previous file means "changed", not "unknown".** Both no-file paths
 * return 1, so a first run behaves as if the language had just been switched --
 * which is what makes whatever depends on this rebuild its text on install.
 *
 * The write is 10 bytes, a literal, so the language code is a fixed-width field
 * and not a C string of whatever length. `strcmp` still reads it as one.
 *
 * The argument only controls whether the current language is SAVED; the
 * comparison happens either way.
 */
int compareLanguages(int write)
{
    char *prev = (char *)limeLoadSaveFile("previouslanguage");
    int r;

    if (prev == 0) {
        if (write)
            limeWriteFile("previouslanguage", Language, 10, 0);
        return 1;                       /* no file: treat as changed */
    }

    if (write)
        limeWriteFile("previouslanguage", Language, 10, 0);

    r = strcmp(prev, Language);
    limeFree(prev);
    return r != 0;
}


/* ---------------------------------------------------- CheckForUnclaimedTreasure
 *
 * armv7 0x0002337c, 104 bytes.  **Complete.**
 *
 *      p = load("unclaimedtreasure")
 *      if (!p) return 0
 *      v = *p
 *      if (v == 0) { free(p); return 0 }
 *      free(p)
 *      p = load("unclaimedtreasure_playedas")
 *      if (p) { PLAYER1MODEL = *p; free(p) }
 *      p = load("unclaimedtreasure_lastdestiny")
 *      if (p) { LastDestiny = *p; free(p) }
 *      return 1
 *
 * The reader for the three files SaveUnclaimedTreasure writes, and it treats
 * them as three independent facts rather than one record: **a missing companion
 * file is not a failure**, and the two branches that hit one write the loaded
 * pointer's own value into LastDestiny before returning -- which on the
 * not-found path is NULL, so LastDestiny becomes 0 and the function still
 * returns 1.
 *
 * That is transcribed as written. Storing a null pointer as a destiny index
 * looks like a bug and behaves as "destiny 0"; the alternative reading, that
 * the register happened to be zero and the store is deliberate, is the same
 * thing at runtime. Either way a port must not "fix" it into leaving
 * LastDestiny alone, because 0 is what the original leaves behind.
 *
 * A treasure value of zero is "nothing to claim" and returns 0 without reading
 * the other two files at all.
 */
int CheckForUnclaimedTreasure(void)
{
    long *p = (long *)limeLoadSaveFile("unclaimedtreasure");
    long treasure;

    if (p == 0)
        return 0;

    treasure = *p;
    if (treasure == 0) {
        limeFree(p);
        return 0;                       /* zero means nothing pending */
    }
    limeFree(p);

    p = (long *)limeLoadSaveFile("unclaimedtreasure_playedas");
    if (p) {
        PLAYER1MODEL = (int)*p;
        limeFree(p);
    } else {
        LastDestiny = 0;                /* the not-found path stores the NULL */
        return 1;
    }

    p = (long *)limeLoadSaveFile("unclaimedtreasure_lastdestiny");
    if (p) {
        LastDestiny = (int)*p;
        limeFree(p);
    } else {
        LastDestiny = 0;
    }
    return 1;
}


extern float Camera[3];                 /* 0x0014fa74 = (0.0, -600.0, 146.0) */
extern int   ClearedZBuffer;            /* 0x001f44c8 */
extern int  *LevelSelectPtr;            /* pointer slot -> 0x000ff7f8 */
void LIMEDS_Set3dMode(void);
void DoSmokesSmoke(long id1, long id2);
void SetToUseCamera(const float *eye);
void RenderLevelBG(void);
void RenderLevelPlayers(void);
void LIME_RenderEvents(long pass);
void MaintainParticles(void);
void KillIllegalWhirlwinds(void);
void RenderExtras(void);
void limeClearDepthBuffer(void);


/* ------------------------------------------------------------- RenderGameView
 *
 * armv7 0x000260d4, 140 bytes.  **Complete. This is the frame's 3D half.**
 *
 *      LIMEDS_Set3dMode()
 *      eye = Camera + ShakeOffset            <- componentwise, all three
 *      SetToUseCamera(eye)
 *      RenderLevelBG()
 *      LIME_RenderEvents(0)                  <- pass 0
 *      ClearedZBuffer = 0
 *      RenderLevelPlayers()
 *      if (ClearedZBuffer && LevelSelect == 2) limeClearDepthBuffer()
 *      LIME_RenderEvents(1)                  <- pass 1
 *      MaintainParticles()
 *      KillIllegalWhirlwinds()
 *      RenderExtras()
 *
 * **The screen shake is added to the camera here and nowhere else.**
 * CalcShakeOffset writes only the z component and zeroes x and y, and this adds
 * all three, so the shake moves the eye along z -- toward and away from the
 * fight -- rather than jittering it sideways.
 *
 * `_Camera` is (0.0, -600.0, 146.0) and `_CameraLookAt` is the origin, which is
 * what this port measured independently from a capture of the game running:
 * an eye height of 136 to 143 against the 146 the binary carries.
 *
 * **LIME_RenderEvents runs twice, with 0 and then 1** -- the same two-pass
 * argument decomp/lime/Events.c documents, opaque then translucent, and the
 * players are drawn BETWEEN them.
 *
 * `_ClearedZBuffer` is cleared before the players and tested after, so
 * RenderLevelPlayers sets it. Combined with LevelSelect == 2 it triggers a
 * depth clear before the second event pass -- a character drawn on top of the
 * arena regardless of depth, which is what a level-select preview needs.
 *
 * `PortAfterScene` is the port's, not the binary's: the debug menu's hitboxes,
 * drawn while the camera is still set. NULL unless they are on.
 */
void (*PortAfterScene)(void);

void RenderGameView(void)
{
    float eye[3];

    LIMEDS_Set3dMode();

    eye[0] = Camera[0] + ShakeOffset[0];
    eye[1] = Camera[1] + ShakeOffset[1];
    eye[2] = Camera[2] + ShakeOffset[2];    /* the shake is z-only */
    SetToUseCamera(eye);

    RenderLevelBG();
    LIME_RenderEvents(0);                   /* opaque pass */

    ClearedZBuffer = 0;
    RenderLevelPlayers();                   /* which may set it */

    if (ClearedZBuffer != 0 && *LevelSelectPtr == 2)
        limeClearDepthBuffer();

    LIME_RenderEvents(1);                   /* translucent pass */
    MaintainParticles();
    KillIllegalWhirlwinds();
    RenderExtras();

    if (PortAfterScene != NULL)             /* port: the debug hitboxes */
        PortAfterScene();
}


extern int Destiny;                     /* 0x0014e20c */
extern int GameStarted;                 /* 0x0014e208 */
extern int Stage;                       /* 0x0014e214 */


extern int *Character1Ptr;              /* slot -> 0x000ff988 */
extern int *ClassicSubZeroUnlockedPtr;  /* slot -> 0x000ff970 */
extern int *ErmacUnlockedPtr;           /* slot -> 0x000ff974 */
extern int *MileenaUnlockedPtr;         /* slot -> 0x000ff978 */
extern int *JadeUnlockedPtr;            /* slot -> 0x000ff97c */
extern int *SurvivalStagePtr;           /* slot -> 0x000ff980 */
extern int  TreasureGained[10];        /* 0x00101164, the array itself (as FrontEnd.c) */
extern long  EndingsGained[23];         /* 0x00101088, the array itself */
extern int  winStreak;                  /* 0x0014e1a8 */
void Write_SaveData(void);


/* -------------------------------------------------------------- Reset_SaveData
 *
 * armv7 0x0002359c, 124 bytes.  **Complete.**
 *
 * Zeroes everything a save holds and writes it straight back out:
 *
 *      Destiny, GameStarted, Stage, Character1, winStreak, SurvivalStage
 *      ClassicSubZeroUnlocked, ErmacUnlocked, MileenaUnlocked, JadeUnlocked
 *      TreasureGained[0..9]        (loop to 0x28)
 *      EndingsGained[0..22]        (loop to 0x5c)
 *      Write_SaveData()
 *
 * **The four unlock flags are cleared too**, so this is a full wipe and not a
 * "start a new run" -- Classic Sub-Zero, Ermac, Mileena and Jade all go back to
 * locked. Anything that wants to reset progress without taking the roster away
 * needs a different function; this one does not have a partial mode.
 *
 * `Destiny` is set to 0 here, where FE_Task_Game_Over sets it to -1. Two
 * different "no destiny" values in the same codebase, and both are transcribed
 * as written -- FE_Task_Game_Over is ending a run, this is erasing one.
 *
 * The two arrays are 10 and 23 words, from the loop bounds 0x28 and 0x5c.
 */
void Reset_SaveData(void)
{
    int i;

    Destiny     = 0;                    /* 0 here, -1 in FE_Task_Game_Over */
    GameStarted = 0;
    Stage       = 0;

    *Character1Ptr = 0;
    *ClassicSubZeroUnlockedPtr = 0;     /* the unlocks go too */
    *ErmacUnlockedPtr          = 0;
    *MileenaUnlockedPtr        = 0;
    *JadeUnlockedPtr           = 0;

    winStreak         = 0;
    *SurvivalStagePtr = 0;

    for (i = 0; i < 0x28 / 4; i++)
        TreasureGained[i] = 0;
    for (i = 0; i < 0x5c / 4; i++)
        EndingsGained[i] = 0;

    Write_SaveData();
}


void Task_LoadingScreen_DRAWSCREEN(long a, long percent);


extern int   randomKode;                /* 0x00151084 */
extern int   DrawRandomKode;            /* 0x001f44cc */
extern int   TipToDisplay;              /* 0x001f44d0 */
extern int   NextTask;                  /* 0x0015058c */
void *limeLoadTexture(const char *name, long a, long b);
long  limeRand(void);
int   printf(const char *fmt, ...);


/* ---------------------------------------------------------- Task_LoadingScreen
 *
 * armv7 0x0001d4c4, 168 bytes.  **Complete.**
 *
 *      LoadingTexture = limeLoadTexture("FE_TITLE_LOADING.PNG", 0, 0)
 *      randomKode     = |limeRand()| % 20
 *      printf("RANDOM KODE IS %d\n", randomKode)
 *      DrawRandomKode = limeRand() & 1
 *      TipToDisplay   = |limeRand()| % 14
 *      Task_LoadingScreen_DRAWSCREEN(NextTask == 5 ? 1 : 0, 0)
 *      CurrentTask = NextTask
 *
 * **Three separate limeRand calls, and each is masked differently**: modulo 20
 * for the kode, one bit for whether to show it at all, modulo 14 for the tip.
 * Both moduli are reciprocal multiplies -- the %14 uses the same 0x92492493
 * getRandomLevel does, which is how you can tell they are the same idiom rather
 * than two different intents.
 *
 * The negation before each modulus is the same guard get_rsound and get_gsound
 * carry: a negative remainder would index backwards.
 *
 * **The loading screen's own art is a PNG.** `FE_TITLE_LOADING.PNG` -- one of
 * the 183 textures that exist only in that format, which is why this port needed
 * a PNG decoder before any of it could be seen.
 *
 * NextTask == 5 selects a different draw variant and is the only thing the
 * first argument carries.
 */
void Task_LoadingScreen(void)
{
    long r;

    LoadingTexture = limeLoadTexture("FE_TITLE_LOADING.PNG", 0, 0);

    r = limeRand();
    if (r < 0) r = -r;
    randomKode = (int)(r % 20);
    printf("RANDOM KODE IS %d\n", randomKode);

    DrawRandomKode = (int)(limeRand() & 1);

    r = limeRand();
    if (r < 0) r = -r;
    TipToDisplay = (int)(r % 14);       /* the same 0x92492493 as getRandomLevel */

    Task_LoadingScreen_DRAWSCREEN(NextTask == 5 ? 1 : 0, 0);
    CurrentTask = NextTask;
}


#define FRAME_ID_MAX  0x1c4c            /* 7244 */

extern long FrameInfo2[][4];            /* 0x00129f1c, {x, y, w, h} */
extern long FrameInfo[][4];             /* 0x0010df1c, the fallback */


/* ------------------------------------------------------------ FrameID_GetBBox
 *
 * armv7 0x0001c674, 260 bytes.  **Complete, and it has three paths.**
 *
 * Normal: FrameInfo2[fid] is {x, y, w, h} at a 16-byte stride, and the outputs
 * are x, y, x+w and y+h -- each written only if its pointer is non-NULL, so a
 * caller can ask for any subset.
 *
 * **Fallback: if FrameInfo2[fid].h is zero it reads `_FrameInfo` instead**, a
 * second table at 0x0010df1c with the same shape. So there are two frame tables
 * and the second is consulted when the first has no height. That is not
 * mentioned anywhere else in this tree.
 *
 * **Out of range is not an error**: fid above 7244 zeroes all four outputs and
 * carries on. 7244 is one less than the 7,245 entries ClearAnimRemapTables
 * walks -- the two tables are the same length, established from opposite ends.
 *
 * **fid == -1 means "the camera", not a frame.** It builds a box from
 * `_Camera` and `_WorldScaleAdjust` (64.0) with three literals -- 200, 196 and
 * 100:
 *
 *      cx = Camera[0] * 64
 *      *x0 = cx - 200         *x1 = cx + 200
 *      cy = -(Camera[2] * 64)
 *      *y0 = cy - 100 + 196   *y1 = cy + 100 + 196
 *
 * Note the vertical uses Camera[2], negated, where the horizontal uses
 * Camera[0] -- the Z-up world reaching the 2D box -- and that the 196 is added
 * to BOTH edges while the 100 is the half-height. So the box is 400 by 200,
 * centred horizontally on the camera and offset 196 vertically.
 *
 * The -1 path writes all four unconditionally, unlike the normal one.
 */
void FrameID_GetBBox(long fid, long *x0, long *y0, long *x1, long *y1)
{
    long x = 0, y = 0, w = 0, h = 0;

    if (fid <= FRAME_ID_MAX) {
        if (FrameInfo2[fid][3] != 0) {
            x = FrameInfo2[fid][0];
            y = FrameInfo2[fid][1];
            w = FrameInfo2[fid][2];
            h = FrameInfo2[fid][3];
        } else {
            x = FrameInfo[fid][0];      /* the second table */
            y = FrameInfo[fid][1];
            w = FrameInfo[fid][2];
            h = FrameInfo[fid][3];
        }
    }

    if (x0) *x0 = x;
    if (y0) *y0 = y;
    if (x1) *x1 = x + w;
    if (y1) *y1 = y + h;

    if (fid != -1)
        return;

    {   /* -1 is the camera, and every output is written */
        float cx = Camera[0] * WorldScaleAdjust;
        float cy = -(Camera[2] * WorldScaleAdjust);

        *x0 = (long)(cx - 200.0f);
        *x1 = (long)(cx + 200.0f);
        *y0 = (long)(cy - 100.0f + 196.0f);
        *y1 = (long)(cy + 100.0f + 196.0f);
    }
}


int  puts(const char *s);
int  putchar(int c);


/* -------------------------------------------------------------------- dumpMem
 *
 * armv7 0x0002b8dc, 100 bytes.  **Complete.**
 *
 * A debug hex dump, banner above and below:
 *
 *      ################################################
 *      %04x %04x %04x ...
 *      \n################################################
 *
 * **The bytes are read SIGNED** -- `ldrsb`, then passed to a `%04x`. So a byte
 * above 0x7f prints as `ffffffb2`, eight characters wide, not `00b2`. The
 * columns stop lining up exactly where the data stops being ASCII, which for a
 * dump of raw memory is most of it.
 *
 * That is transcribed rather than corrected. It is a debug function that
 * shipped in the retail binary, the misalignment is what its author saw, and
 * anyone reading a dump produced by a "fixed" version against one in an old bug
 * report would be comparing two different things.
 *
 * The trailing newline is only emitted when the last byte fills a row, so a
 * dump whose length is not a multiple of `perline` runs its closing banner onto
 * the same line as the last row.
 */
void dumpMem(char *p, int len, int perline)
{
    int i, col;

    puts("################################################");

    for (i = 0, col = 0; i < len; ) {
        printf("%04x ", (int)*(signed char *)p);
        p++;
        i++;
        col++;
        if (col >= perline) {
            putchar(10);                /* \n */
            col = 0;
        }
    }

    puts("\n################################################");
}


extern float FE_Fade;                   /* 0x00100898 */
extern int   limeScreenWidth;           /* 0x00171aec */
extern int   limeScreenHeight;          /* 0x00171af0 */

void limeEnableAlphaBlending_Basic(void);
void limeSet2DDrawing(void);
void limeFillRect(float x, float y, float w, float h,
                  float r, float g, float b, float a);


/* --------------------------------------------------------- IntroRender2dBits
 *
 * armv7 0x0001d7cc, 120 bytes.  **Complete.**
 *
 * The intro fade overlay, and that is all it is: a full-screen black quad at
 * alpha `1 - FE_Fade`, drawn in 2D with basic alpha blending.
 *
 *      if (FE_Fade == 1.0f) return         <- fully faded in, nothing to darken
 *      limeFillRect(0, 0, w, h, 0, 0, 0, 1 - FE_Fade)
 *
 * The early-out and the subtraction agree with each other: at FE_Fade == 1 the
 * alpha would be zero, so the test is not an approximation of "close enough",
 * it is the exact point where the quad stops mattering.
 *
 * `_limeScreenWidth` and `_limeScreenHeight` are ints and are converted with
 * `vcvt.f32.s32` at the call. The name "2dBits" suggests more than one overlay;
 * there is only this one.
 */
void IntroRender2dBits(void)
{
    limeEnableAlphaBlending_Basic();
    limeSet2DDrawing();

    if (FE_Fade == 1.0f)
        return;

    limeFillRect(0.0f, 0.0f,
                 (float)limeScreenWidth, (float)limeScreenHeight,
                 0.0f, 0.0f, 0.0f, 1.0f - FE_Fade);
}


/* `_Mk3Obj_t` -- the arcade object record. Only the four fields this function
 * reads are established here, all of them 16-bit:
 *
 *      +0x04  int16   x in arcade units
 *      +0x06  int16   y in arcade units, sign INVERTED on the way out
 *      +0x08  int16   frame id, signed
 *      +0x0a  uint16  flags, bit 4 tested
 */
typedef struct Mk3Obj_t Mk3Obj_t;

/* `_FrameRemapTable` 0x002003d4, eight bytes an entry, first word used.
 * `_PlayerDefs` 0x00170950, **52 bytes** an entry -- the compiler spells the
 * multiply out as `(n*16 - n*4 + n) << 2`, which is 13 << 2. */
extern long   FrameRemapTable[];          /* pointer slot -> 0x002003d4 */
typedef struct PLAYERDEF {
    long        id;             /* 0x00  0..25; the same as the index */
    float       scale;          /* 0x04  multiplied by PlayerSize */
    float       posOffsetX;     /* 0x08  added or subtracted by facing */
    float       height;         /* 0x0c  added to Z in ArcadePosTo3dPos */
    float       renderOffsetX;  /* 0x10  glTranslatef x, times +/-2.15 */
    float       renderOffsetZ;  /* 0x14  glTranslatef z, times 0.65 */
    const char *lighting;       /* 0x18  "KANO" */
    const char *frameList;      /* 0x1c  "kanoframes.txt" */
    const char *bones;          /* 0x20  "KANO_STANDARD.bones" */
    const char *skin;           /* 0x24  "KANO_STANDARD.skin" */
    const char *skinAnim;       /* 0x28  "KANO_STANDARD.skinanim" */
    const char *texBase;        /* 0x2c  "KANO" */
    const char *scene;          /* 0x30  "KANO_STANDARD.scene" */
} PLAYERDEF;

/* 52 bytes an entry IN THE IMAGE, where a pointer is four bytes. Kept because
 * the compiler spells the multiply out as `(n*16 - n*4 + n) << 2` and that is
 * how the stride was read off in the first place. The host struct is wider and
 * nothing should use this number to index it. */
extern const PLAYERDEF PlayerDefs[];   /* 0x00170950, 26 entries */
/* Moved with `ldr`/`str`, never through the FPU, so it is declared as the word
 * it is copied as. See the note in the function below. */
extern union { float f; long w; } PlayerZPos;   /* 0x00150e88 */

#define PLAYERDEF_STRIDE  52


/* ------------------------------------------------- ArcadePosTo3dPosNO_OFFSETS
 *
 * armv7 0x0001c594, 140 bytes.  **Complete.**
 *
 * Arcade coordinates to a world position, without the per-object offsets the
 * 304-byte `ArcadePosTo3dPos` applies:
 *
 *      out->x = (float)obj->x / WorldScaleAdjust
 *      out->y = PlayerZPos                          <- copied as a raw word
 *      out->z = -(float)obj->y / WorldScaleAdjust
 *               + PlayerDefs[remap].height
 *
 * **The arcade Y is negated and lands in Z.** The arcade runs Y down the
 * screen; the engine runs Z up the world. That sign is the whole conversion and
 * it is the easiest thing here to lose.
 *
 * `PlayerZPos` is moved with `ldr`/`str`, not through the FPU -- the bit
 * pattern is copied, so a signalling NaN would survive it intact. Transcribed
 * as a word copy for that reason.
 *
 * The player-def index comes from `FrameRemapTable[obj->frame]`, and the frame
 * id is read SIGNED (`ldrsh`), so a negative frame indexes backwards off the
 * front of the table. Nothing guards it.
 *
 * ### The branch that does not branch
 *
 * `obj->flags & 0x10` selects between two literal-pool entries -- and **both
 * resolve to `_WorldScaleAdjust` at 0x0014df9c**, checked by hand rather than
 * trusted from the tool. The neighbours are `_blast_player_height` at
 * 0x0014df98 and `_psp_scale` at 0x0014dfa0, so there is no second symbol
 * hiding at that address.
 *
 * So the test is vestigial: whichever way it goes, the divisor is the same.
 * It is transcribed as the single divisor it really is, with the flag noted
 * here, because writing an `if` whose arms are identical would be inventing a
 * distinction the machine does not make.
 *
 * **Why it is there is now known.** `ArcadePosTo3dPos` below is the same
 * function with the player offsets kept, and there the very same
 * `flags & 0x10` chooses the SIGN of the X offset -- subtract when set, add
 * when clear. Drop the offset term and the test has nothing left to select.
 * The dead branch here is the scar of that function, not a compiler artefact.
 */
void ArcadePosTo3dPosNO_OFFSETS(Mk3Obj_t *obj, float *out)
{
    const short *o = (const short *)obj;
    long remap = FrameRemapTable[o[4] * 2];         /* [frame], stride 8 */
    const PLAYERDEF *def;

    /* o[2] is +4, o[3] is +6, o[4] is +8 -- all int16. */
    out[0] = (float)o[2] / WorldScaleAdjust;
    ((long *)out)[1] = PlayerZPos.w;    /* word copy, not a float load */

    def = &PlayerDefs[remap];
    out[2] = (float)(-o[3]) / WorldScaleAdjust + def->height;
}


extern int  limeScreenWidthI;           /* alias comment only -- see below */
/* Four floats, all 1.0f -- white. FrontEnd.c spells it `float col[]` and
 * the bits at 0x0014fa00 are 0x3f800000 four times, so `long` here made
 * one symbol into two incompatible types. */
extern float col[];                     /* 0x0014fa00, RGBA */

/* The colour is four floats, as FrontEnd.c and lime.h both have it. Spelled
 * `long *` here only because `col` was spelled `long []` a few lines up. */
void limeDrawSprite(TEXTURE *tex, float x, float y, float w, float h,
                    float u0, float v0, float u1, float v1,
                    const float *colour);


/* ------------------------------------------------------ drawLoadingBackground
 *
 * armv7 0x0001c824, 152 bytes.  **Complete.**
 *
 * The loading screen backdrop, drawn full-screen from `_LoadingTexture`:
 *
 *      limeDrawSprite(tex, 0, 0, screenW, screenH, 0, 0, 1.0f, 0.75f, col)
 *
 * **`v1` is 0.75, not 1.0.** The texture is sampled to three quarters of its
 * height, which is what a 1024x768 image sitting in a 1024x1024 texture needs.
 * A port that stretches the whole texture gets the picture squashed and a
 * quarter of blank space at the bottom, and it will look almost right.
 *
 * ### It reloads its own texture
 *
 * If `_LoadingTexture` is NULL it prints `reloading texture...` and calls
 * `limeLoadTexture("FE_TITLE_LOADING.PNG", 0, 0)` itself, stores the result,
 * and falls back into the draw. Task_LoadingScreen above loads the same file --
 * so this is a second, defensive load for the case where the texture was freed
 * underneath the loading screen. If that load fails too it draws nothing and
 * returns; it does not loop.
 *
 * The guard before the draw is `tex[0x40] != 0`, a field of TEXTURE this tree
 * has not otherwise named -- a loaded texture with that word clear is skipped.
 */
void drawLoadingBackground(void)
{
    TEXTURE *tex = LoadingTexture;

    if (tex == 0) {
        puts("reloading texture...");
        tex = (TEXTURE *)limeLoadTexture("FE_TITLE_LOADING.PNG", 0, 0);
        LoadingTexture = tex;
        if (tex == 0)
            return;
    }

    if (((const long *)tex)[0x40 / 4] == 0)
        return;

    limeDrawSprite(tex, 0.0f, 0.0f,
                   (float)limeScreenWidth, (float)limeScreenHeight,
                   0.0f, 0.0f, 1.0f, 0.75f, col);
}


/* `_ButtonsTPage` -- 0x001f40d8, one word: the button atlas texture,
 * reached through a slot. FrontEnd.c assigns it from limeLoadTexture.
 * Eight buttons across: the U extent below is exactly 1/8. */
extern TEXTURE *ButtonsTPage;

/* These return FLOATS -- `vmul.f32` into s14 and `vmov r0, s14` on the way out,
 * see FrontEnd.c. They were declared `int` here, which made every `(float)`
 * cast at the call sites convert a float's bit pattern as if it were an
 * integer. Corrected while writing UpdateInGamePauseMenu, which passes their
 * results straight to limeDrawSprite. */
float FE_X(float x);
float FE_Y(float y);
float FE_W(float w);
float FE_H(float h);


/* --------------------------------------------------------- drawSingleButton
 *
 * armv7 0x0001e09c, 152 bytes.  **Complete.**
 *
 * One 64x64 button from the atlas, positioned through the front-end scalers:
 *
 *      limeDrawSprite(ButtonsTPage,
 *                     FE_X(x), FE_Y(y), FE_W(64), FE_H(64),
 *                     0.0f, 0.5f, 0.125f, 0.25f, colour)
 *
 * **The V pair runs backwards: v0 is 0.5 and v1 is 0.25.** That is not a
 * transcription slip -- 0.5 is loaded into the sp+8 slot and 0.25 into sp+0x10,
 * the same slots that hold 0 and 0.75 in drawLoadingBackground. The sprite is
 * sampled bottom-to-top, so a port that normalises the pair draws every button
 * upside down.
 *
 * U runs 0 to 0.125, one eighth, which is where the eight-across atlas comes
 * from.
 *
 * The colour is a five-word local: four 1.0f copied from the function own
 * static constant, then the caller's word written over the fifth. So the caller
 * controls only that last word and the RGBA above it is always white.
 *
 * That word is the **alpha**, and it is a float. It arrives in `r2` and is
 * stored with `str` rather than `vstr`, which is why it was typed `long` here;
 * `FE_Task_Button_Config` is the first call site to be written and it passes
 * 0.5, 0.75 or 1.0 straight from `Settings[6]`. Typed float now, and the union
 * keeps the "moved as a word" reading visible.
 */
void drawSingleButton(int x, int y, float alpha)
{
    union { float f; long w; } colour[5];

    colour[0].f = 1.0f;                 /* __ZZ16drawSingleButtonE5C.105 */
    colour[1].f = 1.0f;
    colour[2].f = 1.0f;
    colour[3].f = 1.0f;
    colour[4].f = alpha;

    /* Port, not in the binary: on the iPad sheet (texture quality IPAD and
     * up, FEInit_LoadABit) the even cell is a pressed glow; the odd one is
     * the button as the iPhone sheet's even cell shows it. */
    limeDrawSprite(ButtonsTPage,
                   FE_X((float)x), FE_Y((float)y),
                   FE_W(64.0f),    FE_H(64.0f),
                   PortTextureQuality >= 1 ? 0.125f : 0.0f,
                   0.5f, 0.125f, 0.25f, (const float *)&colour[0]);
}


extern float TestScale;                 /* 0x0014e268 */
extern float RenderMeshAlphaOverRide;   /* 0x0014e238 */

typedef struct limeVECTOR3  limeVECTOR3;
typedef struct limeMATRIX44 limeMATRIX44;
typedef struct MESHSETINFO  MESHSETINFO;

void LIMEDS_SetObjectOrientation(limeMATRIX44 *m, limeVECTOR3 *pos);
void LIME_RenderMesh(MESHSETINFO *set, int index, TEXTURE *tex0, TEXTURE *tex1, long flags);
void LIME_GLAPI glPushMatrix(void);
void LIME_GLAPI glPopMatrix(void);
void LIME_GLAPI glScalef(float x, float y, float z);
void LIME_GLAPI glEnable(unsigned int cap);
void LIME_GLAPI glCullFace(unsigned int mode);
void LIME_GLAPI glColor4f(float r, float g, float b, float a);

#define GL_FRONT      0x0404
#define GL_BACK       0x0405
#define GL_CULL_FACE  0x0B44


/* ----------------------------------------------------------------- RenderAMesh
 *
 * armv7 0x000250c4, 164 bytes.  **Complete.**
 *
 * The generic one-mesh draw, and the two things in it worth carrying across are
 * both about mirroring and both easy to get wrong:
 *
 *      flip == 0:  glScalef( TestScale, TestScale, TestScale)
 *                  glCullFace(GL_BACK)
 *
 *      flip != 0:  glScalef(-TestScale, TestScale, TestScale)
 *                  glCullFace(GL_FRONT)
 *
 * **A negative X scale reverses triangle winding, so the cull face has to flip
 * with it.** The game does exactly that, and it does it by culling the FRONT
 * face rather than by reordering anything. A port that mirrors the matrix and
 * leaves the cull alone loses every front-facing triangle of the mirrored
 * fighter and keeps the back ones -- which looks like the model turning inside
 * out, not like a missing flag.
 *
 * The negation is `eor r0, r1, #0x80000000`, the sign bit flipped in the
 * integer domain, so it is exact for every value including zero.
 *
 * **`glCullFace(GL_BACK)` is issued again on the way out, unconditionally.** So
 * the flipped state never leaks to the next draw, and nothing else has to
 * remember to restore it. Both `glPushMatrix` calls are matched by pops.
 *
 * The colour is white with alpha from `_RenderMeshAlphaOverRide` -- a global,
 * not an argument, so every mesh drawn through here shares one fade value.
 *
 * The two int arguments in r0 and r1 are never read.
 */
void RenderAMesh(int unused0, int unused1, limeVECTOR3 *pos, limeMATRIX44 *m,
                 int flip, TEXTURE *t0, TEXTURE *t1, MESHSETINFO *ms, long n)
{
    (void)unused0;
    (void)unused1;

    glPushMatrix();

    if (flip) {
        glScalef(-TestScale, TestScale, TestScale);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);           /* the winding is reversed */
    } else {
        glScalef(TestScale, TestScale, TestScale);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
    }

    LIMEDS_SetObjectOrientation(m, pos);
    glColor4f(1.0f, 1.0f, 1.0f, RenderMeshAlphaOverRide);

    glPushMatrix();
    LIME_RenderMesh(ms, n, t0, t1, 0);
    glPopMatrix();

    glPopMatrix();
    glCullFace(GL_BACK);                /* always restored */
}


/* The joystick bit alphabet, as far as this function establishes it:
 *
 *      0x0004  left            0x0800  "toward the opponent"
 *      0x0008  right           0x1000  "away from the opponent"
 *      0x0080  a button, tested against the PREVIOUS frame
 *      0x2000  set while IsInFinishing and the caller flag is set
 *
 * 0x800 and 0x1000 are FACING-RELATIVE and 4 and 8 are absolute. Translating
 * between the two is what the tail of this function does. */
#define JOY_LEFT     0x0004
#define JOY_RIGHT    0x0008
#define JOY_TOWARD   0x0800
#define JOY_AWAY     0x1000
#define JOY_FINISH   0x2000

typedef struct MKMOVE {
    long unused;                        /* 0x00 */
    long mask;                          /* 0x04  the bit pattern to match */
    long index;                         /* 0x08  into _VeteranMoves */
} MKMOVE;

#define MKMOVE_SLOTS  0x4c              /* 76 */

extern MKMOVE VeteranMoves[];           /* 0x0015059c, also 12 bytes an entry */
extern long   LastJoyBitsIn;            /* 0x00150cc0 */
/* `IsInFinishing` and `RemapKicksPunches` are declared above. */


/* --------------------------------------------------------- GetArcadeJoyBits
 *
 * armv7 0x0001b7a4, 272 bytes.  **Complete.**
 *
 * Matches the current joystick state against a 76-entry move table and returns
 * the arcade bit pattern for whatever it found.
 *
 * ### Facing-relative directions are converted on the way OUT, not in
 *
 * The table stores `JOY_TOWARD` / `JOY_AWAY`; the game wants `JOY_LEFT` /
 * `JOY_RIGHT`. The tail does the swap and it is a genuine mirror:
 *
 *      facing != 0:   AWAY -> RIGHT,  TOWARD -> LEFT
 *      facing == 0:   AWAY -> LEFT,   TOWARD -> RIGHT
 *
 * then clears both relative bits. **Both arms then re-test `0x800` on the
 * value they are building, not on the original** -- so a pattern holding both
 * TOWARD and AWAY comes out with both LEFT and RIGHT set rather than one.
 * Transcribed as written; it is reachable only from a table entry that sets
 * both, and whether any does is not established here.
 *
 * ### The 0x80 button is edge-detected against the previous frame
 *
 * `_LastJoyBitsIn` is read and immediately overwritten with this frame's bits,
 * and the match then uses `~previous & current & 0x80` -- the rising edge, not
 * the level. Only entries whose mask has a relative direction bit take that
 * path; the rest compare the raw bits with `0x1800` masked off.
 *
 * That edge is why `_LastJoyBitsIn` must be updated **once per call and before
 * anything else**: a port that refreshes it after the scan, or twice, changes
 * which frame a button counts on.
 *
 * ### An assertion that hangs
 *
 * On a match it looks the move up in `_VeteranMoves` and checks the entry
 * agrees with its own index:
 *
 *      if (VeteranMoves[idx].index != idx) for (;;) ;
 *
 * A branch to its own address, the same shape `Error()` uses. It shipped in
 * retail, so it is a table-consistency assertion nobody expected to fire. A
 * port should make it loud rather than silent -- but it must not be dropped,
 * because reaching it means the move tables are wrong.
 *
 * `0xcf` in a mask is a sentinel: it ends the scan with a tail call to
 * `RemapKicksPunches` on the accumulated bits instead of a table lookup.
 * Falling off the end of the 76 entries returns 0.
 */
long GetArcadeJoyBits(long bits, MKMOVE *moves, long facing, long finishFlag)
{
    long prev = LastJoyBitsIn;
    long notPrev;
    long inFinish;
    long cur, probe, out, idx, mask;
    long i;

    LastJoyBitsIn = bits;               /* before anything else */
    notPrev = ~prev;
    inFinish = IsInFinishing ? ((finishFlag != 0) ? 1 : 0) : 0;

    cur = bits;

    for (i = 0; i < MKMOVE_SLOTS; i++, moves++) {
        out = inFinish ? (cur | JOY_FINISH) : cur;
        mask = moves->mask;

        if ((mask & (JOY_TOWARD | JOY_AWAY)) == 0) {
            probe = cur & ~(JOY_TOWARD | JOY_AWAY);
        } else {
            probe = cur & ~(JOY_LEFT | JOY_RIGHT);
            if (cur & JOY_TOWARD) {
                long edge = notPrev & 0x80 & bits;   /* the rising edge */
                probe = (cur & ~0x8c) | edge;
                cur   = (cur & ~0x80) | edge;
            }
        }

        if (probe == mask || cur == mask)
            break;                      /* matched */

        if (mask == 0xcf)
            return RemapKicksPunches(out);

        /* 0x1b830 `mov r3, r0`: the next entry is compared against the
         * word WITH the finishing bit (r0 is bits | 0x2000 while
         * IsInFinishing). Resetting to the bare bits meant no finisher
         * entry -- every one carries 0x2000 -- could ever match, so no
         * typed fatality, babality, friendship or animality fired. */
        cur = out;
    }

    if (i == MKMOVE_SLOTS)
        return 0;

    idx = moves->index;
    if (VeteranMoves[idx].index != idx)
        for (;;)                        /* b to its own address */
            ;

    mask = VeteranMoves[idx].mask;
    if ((mask & (JOY_TOWARD | JOY_AWAY)) == 0)
        return mask;

    probe = mask & ~(JOY_LEFT | JOY_RIGHT);

    if (facing != 0) {
        if (mask  & JOY_AWAY)   probe |= JOY_RIGHT;
        if (probe & JOY_TOWARD) probe |= JOY_LEFT;
    } else {
        if (mask  & JOY_AWAY)   probe |= JOY_LEFT;
        if (probe & JOY_TOWARD) probe |= JOY_RIGHT;
    }

    return probe & ~(JOY_TOWARD | JOY_AWAY);
}


#define ARCADE_POS_RAW  0x4e20          /* 20000 -- the "no offsets at all" frame */


/* ---------------------------------------------------------- ArcadePosTo3dPos
 *
 * armv7 0x0001c464, 304 bytes.  **Complete.**
 *
 * The full version of the conversion `ArcadePosTo3dPosNO_OFFSETS` does without
 * the player offsets. Same core arithmetic:
 *
 *      out->x = obj->x / WorldScaleAdjust  -/+ def->xOffset
 *      out->y = PlayerZPos
 *      out->z = -obj->y / WorldScaleAdjust  +  def->height
 *
 * ### The flag bit that was vestigial there is load-bearing here
 *
 * `obj->flags & 0x10` chooses the SIGN of the X offset -- subtract when set,
 * add when clear -- and nothing else differs between the two arms. In
 * `NO_OFFSETS` the same test survives with both arms resolving to the same
 * literal-pool entry, which read as a branch that does not branch.
 *
 * Together they explain each other: the two functions share a shape, the
 * offset term was dropped from the NO_OFFSETS variant, and the test that
 * selected its sign was left behind with nothing to select. So the dead branch
 * there is not a compiler artefact -- it is the scar of this function.
 *
 * ### Three ways to reach a PLAYERDEF, and a sentinel
 *
 *      frame == 20000        no def at all: raw divide, no offsets, no height
 *      third argument NULL   def = PlayerDefs[FrameRemapTable[frame]]
 *      otherwise             def = PlayerDefs[*(signed char *)arg]
 *
 * The third argument is a pointer to a single SIGNED byte holding a character
 * index -- not the index itself. A negative byte indexes backwards off the
 * front of the table, and nothing guards it.
 *
 * **A frame above 7243 returns without writing anything**, leaving the caller
 * vector holding whatever it did before. Note 7243, one below the 7244 that
 * bounds `LoadAllFramesTXT` and one below the `> 7244` that `FrameID_GetBBox`
 * rejects on -- three related limits, three different numbers. They are
 * recorded as they are rather than reconciled.
 *
 * `PlayerZPos` is copied as a word here too, never through the FPU.
 */
void ArcadePosTo3dPos(Mk3Obj_t *obj, float *out, const signed char *who)
{
    const short *o = (const short *)obj;
    long frame = o[4];                          /* +8, signed */
    float s = WorldScaleAdjust;
    const PLAYERDEF *def;
    long idx;

    if (frame == ARCADE_POS_RAW) {              /* the sentinel: no offsets */
        out[0] = (float)o[2] / s;
        ((long *)out)[1] = PlayerZPos.w;
        out[2] = (float)(-o[3]) / s;
        return;
    }

    if (frame > 0x1c4b)                         /* 7243 -- nothing is written */
        return;

    if (who == 0)
        idx = FrameRemapTable[frame * 2];       /* stride 8, first word */
    else
        idx = *who;                             /* a signed byte, not an index arg */

    def = &PlayerDefs[idx];

    if (((const unsigned short *)obj)[5] & 0x10)        /* +0x0a */
        out[0] = (float)o[2] / s - def->posOffsetX;
    else
        out[0] = (float)o[2] / s + def->posOffsetX;

    ((long *)out)[1] = PlayerZPos.w;
    out[2] = (float)(-o[3]) / s + def->height;
}


extern long **IntroLists;               /* 0x0014e008 */
extern long  *SizeofIntroLists;         /* 0x0014e070 */
extern float  IntroCount;               /* 0x0014e1c4 */
extern float  IntroCount2;              /* 0x0014e1c8 */
extern long   IntroFrame1;              /* 0x0014f908 */
extern long   IntroFrame2;              /* 0x0014f90c */
extern long   AnimSmoothWindowSize;     /* 0x00171368 */
extern long   Character1;               /* 0x000ff988 */
extern long   Character2;               /* 0x000ff98c */
extern char   Players[];                /* 0x001fa4d4 */

void PlayerAutoSmoothAnims(void *p);


/* ------------------------------------------------ UpdateIntroCharacterPlayers
 *
 * armv7 0x00021970, 316 bytes.  **Complete.**
 *
 * Steps both fighters through their intro animation, one player at a time and
 * each from its own counter and its own list:
 *
 *      player 1:  IntroCount  -> IntroLists[Character1] -> IntroFrame1
 *      player 2:  IntroCount2 -> IntroLists[Character2] -> IntroFrame2
 *
 * Each half is skipped entirely if that player has no loaded character
 * (`+0x04` of the PLAYER, and `+0x5f4` for the second -- 0x5f0 plus the same
 * four, confirming the player stride from a third function).
 *
 * ### The frame is only overwritten when the list agrees
 *
 *      if (count < entries && list[count] != -1)
 *          IntroFrameN = list[count];
 *      player->frame = IntroFrameN;
 *
 * A count past the end of the list, or a -1 in it, leaves `IntroFrameN`
 * **holding its previous value** and the player is set to that. So the
 * animation freezes on its last good frame rather than snapping to zero or
 * reading past the array -- and the sticky global is what makes that work.
 * Clamping the index instead would look equivalent and is not: it would replay
 * the last entry rather than hold whatever was last accepted.
 *
 * The counters are floats converted with `vcvt.s32.f32`, so they advance
 * fractionally somewhere else and are truncated here.
 *
 * The entry count is `SizeofIntroLists[who] / 4` with the signed-division
 * rounding transcribed as emitted, the same shape `AnimateFECharacters` uses.
 *
 * ### The smoothing window is borrowed again, at a different value
 *
 * `_AnimSmoothWindowSize` is saved, set to **0x14** and restored -- twenty,
 * where `AnimateFECharacters` uses forty for the same global. So intros smooth
 * over half the window the menus do, and both leave the game's own value
 * untouched.
 */
void UpdateIntroCharacterPlayers(void)
{
    long saved = AnimSmoothWindowSize;
    long *p1 = (long *)Players;
    long *p2 = (long *)(Players + 0x5f0);

    AnimSmoothWindowSize = 0x14;        /* 20, not the 40 the front end uses */

    if (p1[1] != 0) {                   /* +0x04, a loaded character */
        long i = (long)IntroCount;
        const long *list = IntroLists[Character1];
        long n = SizeofIntroLists[Character1];

        if (n < 0)
            n += 3;
        n >>= 2;

        if (i < n && list[i] != -1)
            IntroFrame1 = list[i];      /* otherwise it keeps its old value */

        p1[0x14 / 4] = IntroFrame1;
        PlayerAutoSmoothAnims(p1);
    }

    if (p2[1] != 0) {                   /* +0x5f4 */
        long i = (long)IntroCount2;
        const long *list = IntroLists[Character2];
        long n = SizeofIntroLists[Character2];

        if (n < 0)
            n += 3;
        n >>= 2;

        if (i < n && list[i] != -1)
            IntroFrame2 = list[i];

        p2[0x14 / 4] = IntroFrame2;     /* Players + 0x604 */
        PlayerAutoSmoothAnims(p2);
    }

    AnimSmoothWindowSize = saved;
}


extern float IntroCountTimer;           /* 0x0014e1cc */
extern long  IntroCamCount;             /* 0x0014f940 */


/* ------------------------------------------ AnimateIntroCharacterPlayers1Frame
 *
 * armv7 0x00021aac, 368 bytes.  **Complete.**
 *
 * Steps one intro frame for each fighter, then calls
 * `UpdateIntroCharacterPlayers` to push the result into the players.
 *
 * ### Which fighter advances is decided by IntroCamCount
 *
 *      IntroCamCount == 0   ->  player 2 always;  player 1 only if `force`
 *      IntroCamCount != 0   ->  player 1 always;  player 2 only if `force`
 *
 * The two tests are built from the same `force != 0` boolean with an `orr #1`
 * on opposite arms, so exactly one fighter is unconditional at any moment and
 * the argument overrides that. The intro alternates whose animation runs by
 * flipping one counter.
 *
 * ### The counter is clamped UP, not down: only the last 120 entries play
 *
 * Per fighter:
 *
 *      count += 1.0f
 *      n = SizeofIntroLists[character]
 *      if (n == 0) { IntroCountTimer = 1.0f; }
 *      else {
 *          limit = n / 4 - 120
 *          if (count < limit) { count = limit; if (limit < 0) count = 0; }
 *          if (!force) IntroCountTimer += 1/120
 *      }
 *
 * `count < limit` sets `count = limit` -- forward, not back. So an intro whose
 * list is longer than 120 entries **never plays anything before its last 120**;
 * the counter is snapped past them on the first frame and increments normally
 * after that. A list shorter than 120 gives a negative limit and the counter is
 * reset to 0 instead.
 *
 * A clamp written the other way round -- the obvious reading -- makes long
 * intros play from the beginning and run 120 frames too long. This is the
 * detail in the function.
 *
 * ### The timer step is 1/120
 *
 * `0.008333333767950535`, the nearest float to 1/120, and only when `force` is
 * zero. So the timer tracks unforced frames only, and at 30 fps it reaches 1.0
 * after four seconds of them.
 *
 * The `n == 0` path sets the timer to a flat 1.0 and skips the counter work
 * entirely -- a character with no intro list is treated as already finished.
 */
void AnimateIntroCharacterPlayers1Frame(long force)
{
    long f = (force != 0) ? 1 : 0;
    long n;
    float limit;

    /* player 2 -- unconditional while IntroCamCount is zero */
    if ((IntroCamCount != 0) ? f : (f | 1)) {
        IntroCount2 += 1.0f;
        n = SizeofIntroLists[Character2];

        if (n == 0) {
            IntroCountTimer = 1.0f;
        } else {
            if (n < 0)
                n += 3;
            limit = (float)((n >> 2) - 0x78);       /* 120 */

            if (IntroCount2 < limit) {
                IntroCount2 = limit;                /* forward, not back */
                if (limit < 0.0f)
                    IntroCount2 = 0.0f;
            }
            if (!force)
                IntroCountTimer += 0.008333333767950535f;   /* 1/120 */
        }
    }

    /* player 1 -- unconditional while IntroCamCount is non-zero */
    if ((IntroCamCount == 0) ? f : (f | 1)) {
        IntroCount += 1.0f;
        n = SizeofIntroLists[Character1];

        if (n == 0) {
            IntroCountTimer = 1.0f;
        } else {
            if (n < 0)
                n += 3;
            limit = (float)((n >> 2) - 0x78);

            if (IntroCount < limit) {
                IntroCount = limit;
                if (limit < 0.0f)
                    IntroCount = 0.0f;
            }
            if (!force)
                IntroCountTimer += 0.008333333767950535f;
        }
    }

    UpdateIntroCharacterPlayers();
}


extern float JOUTERDIAL;                /* 0x00150598 */
extern float JINNERDIAL;                /* 0x00150594, read only here */
extern long  P2Controls;                /* 0x0014fec8 */
extern long  JoystickStatePosX;         /* 0x0014fec0 */
extern long  JoystickStatePosY;         /* 0x0014fec4 */
extern long  JoystickStatePosXP2;       /* 0x0014fed0 */
extern long  JoystickStatePosYP2;       /* 0x0014fed4 */

/* Ten touch slots each; -1.0f means the slot is empty. */
#define TOUCH_SLOTS  10
extern float  limeTouchScreenX[];        /* 0x00171af4, four slots */
extern float  limeTouchScreenY[];         /* pointer slot */

double acos(double x);
float  sqrtf(float x);


/* ------------------------------------------------------------- CheckLeftDial
 *
 * armv7 0x00026170, 384 bytes.  **Complete.**
 *
 * Reads the on-screen direction dial. Returns **0..7** for the eight-way
 * direction under the finger, or **-1** when no touch is on the dial.
 *
 * A touch counts only inside the ANNULUS between `JINNERDIAL` and
 * `JOUTERDIAL` -- squared distances are compared, so no square root is taken
 * until a slot has already passed both tests.
 *
 * ### The outer radius stops scaling under P2 controls
 *
 *      JOUTERDIAL = FE_W(80.0f);
 *      if (P2Controls) JOUTERDIAL = 96.0f;
 *
 * The default goes through `FE_W` and follows the screen; the P2 override is a
 * **raw 96 with no scaler anywhere near it**. On any screen where FE_W(80) is
 * not 96 the two players get different dial sizes. Transcribed as written --
 * it reads like a hardcoded pixel value someone dropped in, and reproducing it
 * is the only way a port behaves the same.
 *
 * It also writes the global every call rather than computing a local, so
 * anything else reading `JOUTERDIAL` sees whichever player was checked last.
 *
 * ### The angle
 *
 *      len = sqrt(dx*dx + dy*dy)
 *      t   = acos(dy / len) / PI          <- in [0, 1]
 *      if (dx / len < 0) t = -t           <- signed by the x half
 *      t   = (t + 1) * 0.5                <- back to [0, 1]
 *      if (t < 0) t += 1
 *      dir = (((int)((1 - t) * 256) + 16) & 0xFF) >> 5
 *
 * The `acos` is done in **double** precision -- `vcvt.f64.f32` in, the divide
 * by PI in `.f64`, and only then narrowed back. PI is the literal
 * 3.1415927410125732, the float value widened, not the double constant.
 *
 * The last line is the whole quantiser: scale to 256, add half a sector (16),
 * wrap to a byte, and take the top three bits. The `& 0xFF` is what makes the
 * wrap free, so sector 7 and sector 0 meet without a special case.
 *
 * `dx` and `dy` are both `touch - centre` (see the note in the body).
 */
long CheckLeftDial(int player)
{
    float cx, cy;
    float outer2, inner2;
    long i;

    JOUTERDIAL = FE_W(80.0f);
    if (P2Controls != 0)
        JOUTERDIAL = 96.0f;             /* unscaled, unlike the default */

    if (player == 0) {
        cx = (float)JoystickStatePosX;
        cy = (float)JoystickStatePosY;
    } else {
        cx = (float)JoystickStatePosXP2;
        cy = (float)JoystickStatePosYP2;
    }

    outer2 = JOUTERDIAL * JOUTERDIAL;
    inner2 = JINNERDIAL * JINNERDIAL;

    for (i = 0; i < TOUCH_SLOTS; i++) {
        float dx, dy, d2, len, t;
        long dir;

        if (limeTouchScreenX[i] == -1.0f)
            continue;                   /* empty slot */

        dx = limeTouchScreenX[i] - cx;
        /* touch minus centre on BOTH axes: 0x261e6 `vsub d8, d6, d7` and
         * 0x261f4 `vsub d5, d7, d6` with s14 = the touch's y. An earlier
         * reading had dy = centre - touch, which swapped up and down -- the
         * diagonal attacks came out mirrored. With screen y growing down, a
         * touch straight up gives acos(-1), t = 1, sector 0: JoystickState 1,
         * which GetReal6ButtonJoyBits reads as up. */
        dy = limeTouchScreenY[i] - cy;
        d2 = dx * dx + dy * dy;

        if (d2 > outer2)
            continue;
        if (d2 < inner2)
            continue;

        len = sqrtf(d2);
        t = (float)(acos((double)(dy / len)) / 3.1415927410125732);

        if (dx / len < 0.0f)
            t = -t;

        t = (t + 1.0f) * 0.5f;
        if (t < 0.0f)
            t += 1.0f;

        dir = (long)((1.0f - t) * 256.0f);
        return ((dir + 0x10) & 0xFF) >> 5;
    }

    return -1;
}


/* `_SaveData` -- 0x001f439c, **172 bytes** (0xac), the last word being a
 * checksum. Every field below is written by this function and nothing else in
 * it is touched:
 *
 *      +0x00  GameStarted
 *      +0x04  Destiny                 which arcade ladder
 *      +0x08  Stage                   how far up it
 *      +0x0c  Character1
 *      +0x10  ClassicSubZeroUnlocked
 *      +0x14  ErmacUnlocked
 *      +0x18  MileenaUnlocked
 *      +0x1c  JadeUnlocked
 *      +0x20  TreasureGained[10]
 *      +0x48  winStreak
 *      +0x4c  EndingsGained[23]
 *      +0xa8  checksum -- the sum of every word above
 */
#define SAVEDATA_SIZE  0xac

/* `TreasureGained` (10 words) and `EndingsGained` (23 words) are already
 * declared above with those same counts, derived there independently. */
extern long  SaveData[];                /* 0x001f439c */
/* The variables themselves, each one word in the image (FrontEnd.c and
 * Blood.c declare the same six the same way). These were declared as
 * pointers and dereferenced, which read each flag's VALUE as an address --
 * the first thing confirming a new arcade game did was fault on it. */
extern long ClassicSubZeroUnlocked;     /* 0x000ff970 */
extern long ErmacUnlocked;              /* 0x000ff974 */
extern long MileenaUnlocked;            /* 0x000ff978 */
extern long JadeUnlocked;               /* 0x000ff97c */
extern long SurvivalStage;              /* 0x000ff980 */
extern long SurvivalHealth;             /* 0x000ff994 */


/* ------------------------------------------------------------- Write_SaveData
 *
 * armv7 0x00023404, 408 bytes.  **Complete.**
 *
 * Serialises the save into `_SaveData` and writes it to `savedata`.
 *
 * ### The four unlock flags are named here
 *
 *      ClassicSubZeroUnlocked, ErmacUnlocked, MileenaUnlocked, JadeUnlocked
 *
 * -- the four classic MK3 hidden fighters, each its own word in the save at
 * +0x10 through +0x1c. So the hidden roster is not a derived state or a kode
 * side effect: it is **persisted, one flag per character**, and whatever sets
 * them is what a port has to find. Nothing in this function writes them; it
 * only copies them out.
 *
 * ### It does nothing at all unless GameMode is 0
 *
 * The whole body is skipped for any other mode, and the function then falls
 * straight to the survival check. So a save written during an arcade ladder is
 * written by somebody else, or not at all.
 *
 * ### The checksum is a plain sum
 *
 * Every word stored is added into a running total and the total goes in the
 * last word, +0xa8. Not a CRC, not weighted -- an unsigned sum, so any two
 * fields that swap values leave it unchanged. The size passed to
 * `limeWriteFile` is 0xac, exactly the checksum offset plus four.
 *
 * ### Survival is a second, separate file
 *
 * `GameMode == 4` additionally writes **twelve bytes** to `survival` from a
 * stack buffer: `SurvivalStage`, `Character1`, `SurvivalHealth`. That file has
 * no checksum and shares no layout with the main one.
 */
void Write_SaveData(void)
{
    long sum = 0;
    long i;

    if (GameMode == 0) {
        SaveData[0x00 / 4] = GameStarted;
        SaveData[0x04 / 4] = Destiny;
        SaveData[0x08 / 4] = Stage;
        SaveData[0x0c / 4] = Character1;
        SaveData[0x10 / 4] = ClassicSubZeroUnlocked;
        SaveData[0x14 / 4] = ErmacUnlocked;
        SaveData[0x18 / 4] = MileenaUnlocked;
        SaveData[0x1c / 4] = JadeUnlocked;
        SaveData[0x48 / 4] = winStreak;

        sum = SaveData[0x00 / 4] + SaveData[0x04 / 4] + SaveData[0x08 / 4]
            + SaveData[0x0c / 4] + SaveData[0x10 / 4] + SaveData[0x14 / 4]
            + SaveData[0x18 / 4] + SaveData[0x1c / 4] + SaveData[0x48 / 4];

        for (i = 0; i < 10; i++) {              /* +0x20 .. +0x44 */
            SaveData[0x20 / 4 + i] = TreasureGained[i];
            sum += TreasureGained[i];
        }

        for (i = 0; i < 23; i++) {              /* +0x4c .. +0xa4 */
            SaveData[0x4c / 4 + i] = EndingsGained[i];
            sum += EndingsGained[i];
        }

        SaveData[0xa8 / 4] = sum;               /* a plain sum, not a CRC */
        limeWriteFile("savedata", SaveData, SAVEDATA_SIZE, 0);
    }

    if (GameMode == 4) {
        long survival[3];

        survival[0] = SurvivalStage;
        survival[1] = Character1;
        survival[2] = SurvivalHealth;
        limeWriteFile("survival", survival, 0xc, 0);
    }
}


extern long  SurvivalCharacter1;        /* pointer slot -> 0x000ff9?? */

void *limeLoadSaveFile(const char *name);
void  Reset_SaveData(void);


/* -------------------------------------------------------------- Load_SaveData
 *
 * armv7 0x00023648, 464 bytes.  **Complete.**
 *
 * Reads `savedata` back into the globals `Write_SaveData` copied out of, then
 * validates every one of them.
 *
 * ### The checksum is verified, and failing it wipes the save
 *
 * The same plain sum is recomputed while reading and compared against +0xa8;
 * a mismatch calls **`Reset_SaveData()`** and the fields are then re-validated
 * on top of the reset values. So a corrupt save is not rejected -- it is
 * silently replaced, and the game continues.
 *
 * ### The clamps encode the ladder lengths
 *
 *      Destiny     > 3 -> 0,  < 0 -> 0
 *      Character1  outside [0, 0x17] -> 1
 *      Stage       clamped to at most Destiny + 8
 *
 * **`Destiny + 8` is exactly the ladder length.** `PopulateTower` fills the
 * four ladders with 6, 7, 8 and 9 random opponents plus the two fixed bosses --
 * 8, 9, 10 and 11 fights for Destiny 0 through 3. Two functions written from
 * opposite ends of the save agree on the number without either stating it.
 *
 * Character 0x17 is 23, the slot `LoadFrontEndCharacters` loads as character 0.
 * The out-of-range fallback is **1**, not 0.
 *
 * ### The survival file is read asymmetrically
 *
 * `Write_SaveData` writes `{SurvivalStage, Character1, SurvivalHealth}`; this
 * reads `{SurvivalStage, SurvivalCharacter1, SurvivalHealth}`. The middle word
 * goes out of one global and comes back into a different one. Whether that is
 * deliberate is not established here -- it is transcribed because a port that
 * "fixes" it changes which character a resumed survival run uses.
 *
 * Health is clamped to (0, 100]: zero or negative and anything above 100 both
 * become 100, so a corrupt file resumes at full health rather than dead.
 *
 * Both files are freed after use. A missing `savedata` skips straight past
 * everything, including the survival load.
 */
void Load_SaveData(void)
{
    long *save = (long *)limeLoadSaveFile("savedata");
    long *surv;
    long sum, i, v;

    if (save == 0)
        return;

    GameStarted = save[0x00 / 4];
    Destiny     = save[0x04 / 4];
    Stage       = save[0x08 / 4];
    Character1  = save[0x0c / 4];
    ClassicSubZeroUnlocked = save[0x10 / 4];
    ErmacUnlocked          = save[0x14 / 4];
    MileenaUnlocked        = save[0x18 / 4];
    JadeUnlocked           = save[0x1c / 4];
    winStreak   = save[0x48 / 4];

    sum = save[0x00 / 4] + save[0x04 / 4] + save[0x08 / 4] + save[0x0c / 4]
        + save[0x10 / 4] + save[0x14 / 4] + save[0x18 / 4] + save[0x1c / 4]
        + save[0x48 / 4];

    for (i = 0; i < 10; i++) {
        TreasureGained[i] = save[0x20 / 4 + i];
        sum += save[0x20 / 4 + i];
    }
    for (i = 0; i < 23; i++) {
        EndingsGained[i] = save[0x4c / 4 + i];
        sum += save[0x4c / 4 + i];
    }

    if (sum != save[0xa8 / 4])
        Reset_SaveData();               /* silently replaced, not rejected */

    if (Destiny > 3)
        Destiny = 0;
    else if (Destiny < 0)
        Destiny = 0;

    if (Character1 < 0 || Character1 > 0x17)
        Character1 = 1;                 /* the fallback is 1, not 0 */

    if (Destiny + 8 <= Stage)
        Stage = Destiny + 8;            /* the ladder length */

    limeFree(save);

    surv = (long *)limeLoadSaveFile("survival");
    if (surv == 0)
        return;

    SurvivalStage = surv[0];

    v = surv[1];
    SurvivalCharacter1 = v;            /* written from Character1, read here */
    if (v < 0 || v > 0x17)
        SurvivalCharacter1 = 1;

    v = surv[2];
    SurvivalHealth = v;
    if (v <= 0 || v > 100)
        SurvivalHealth = 100;

    limeFree(surv);
}


extern long  EnduranceChange;           /* 0x0010df14 */
extern long  EnduranceCharacters[];     /* 0x001ab014 */
extern long  LastEnduranceCharacter;    /* 0x001ab674 */
extern long  EnduranceMatchTable2[];    /* 0x00145f1c, two words a row */
extern long  EnduranceMatchTreasure7[]; /* 0x00145f3c, four words a row */
extern long  EnduranceMatchTreasure8[]; /* 0x00145f7c, four words a row */
extern long  EnduranceTowerList[];      /* 0x0014fb50, eleven words a row */
/* `RoundParam` is declared above as `int *`; the three fields this function
 * writes are BYTES, so they go through a char view of the same pointer. */
#define ROUNDPARAM_B  ((char *)RoundParam)
extern long  TreasurePlayed;            /* pointer slot */
extern long  girlrand, girlrand2;       /* 0x0010de74, 0x0010de78 */
extern long  boyrand,  boyrand2;        /* 0x0010de6c, 0x0010de70 */
extern long PLAYER2MODEL;              /* 0x0014e1b8, a plain long like PLAYER1MODEL */


/* --------------------------------------------------------- InitEnduranceMatch
 *
 * armv7 0x0001b5a0, 512 bytes.  **Complete.**
 *
 * Sets up an endurance fight -- one player against a queue of opponents. The
 * queue lives in `_EnduranceCharacters`, terminated by -1, and is mirrored into
 * three bytes of `_RoundParam` at +0x18, +0x19 and +0x1a.
 *
 * The default comes from `EnduranceMatchTable2[Destiny]`, two words a row: the
 * first becomes `Character2` and `PLAYER2MODEL`, the second the single queued
 * opponent. The queue is then closed with -1.
 *
 * ### `EnduranceTowerList` is eleven words a row -- again
 *
 * The index is `Stage + 11 * Destiny`, the same shape `FE_Task_VS_Screen_Init`
 * uses on `OpponentTowerList` and the same 44-byte row `PopulateTower` writes.
 * **A value of 2 in that list means a two-opponent endurance match**, and the
 * setup is derived from the 2 itself rather than looked up: `2 + 16` is the
 * fighter shown, and the queue becomes `{5, 15, -1}` with 15 being that
 * fighter minus three. Three constants from one table entry.
 *
 * ### GameMode 5 overrides everything with the treasure matches
 *
 *      TreasurePlayed == 7   three from EnduranceMatchTreasure7, by girlrand
 *      TreasurePlayed == 8   three from EnduranceMatchTreasure8, by boyrand
 *      TreasurePlayed == 9   a fixed queue: fighter 11 against 24 then 25
 *
 * The two random ones read **three consecutive entries of a four-wide row,
 * wrapping**: row `girlrand`, starting at `girlrand2 & 3`, then `+1` and `+2`
 * masked the same way. So the same three opponents always appear together and
 * only their order rotates -- it is a rotation, not a draw, exactly like the
 * ladder in `PopulateTower`.
 *
 * 24 and 25 in the third case are the same two fixed bosses `PopulateTower`
 * puts at the end of every ladder.
 *
 * Every path writes `-1` into the last queue slot and into `RoundParam + 0x1a`,
 * so the queue is always terminated even when it is shorter than three.
 */
void InitEnduranceMatch(void)
{
    long d = Destiny;
    long v;

    EnduranceChange = 0;

    Character2    = EnduranceMatchTable2[d * 2];
    PLAYER2MODEL = EnduranceMatchTable2[d * 2];
    EnduranceCharacters[0]  = EnduranceMatchTable2[d * 2 + 1];
    LastEnduranceCharacter  = EnduranceMatchTable2[d * 2 + 1];
    ROUNDPARAM_B[0x18] = (char)EnduranceMatchTable2[d * 2 + 1];
    ROUNDPARAM_B[0x19] = (char)-1;
    EnduranceCharacters[1]  = -1;

    v = EnduranceTowerList[Stage + 11 * d];
    if (v == 2) {
        Character2    = v + 0x10;       /* 18, derived from the 2 */
        PLAYER2MODEL = v + 0x10;
        ROUNDPARAM_B[0x18] = 5;
        ROUNDPARAM_B[0x19] = (char)(v + 0x10 - 3);        /* 15 */
        ROUNDPARAM_B[0x1a] = (char)-1;
        EnduranceCharacters[0] = 5;
        EnduranceCharacters[1] = v + 0x10 - 3;
        EnduranceCharacters[2] = -1;
        LastEnduranceCharacter = v + 0x10 - 3;
    }

    if (GameMode != 5)
        return;

    if (TreasurePlayed == 7) {
        long r = girlrand * 4;
        long a = EnduranceMatchTreasure7[r + ((girlrand2 + 0) & 3)];
        long b = EnduranceMatchTreasure7[r + ((girlrand2 + 1) & 3)];
        long c = EnduranceMatchTreasure7[r + ((girlrand2 + 2) & 3)];

        Character2    = a;
        PLAYER2MODEL = a;
        ROUNDPARAM_B[0x18] = (char)b;
        ROUNDPARAM_B[0x19] = (char)c;
        ROUNDPARAM_B[0x1a] = (char)-1;
        EnduranceCharacters[0] = b;
        EnduranceCharacters[1] = c;
        EnduranceCharacters[2] = -1;
        LastEnduranceCharacter = c;
    } else if (TreasurePlayed == 8) {
        long r = boyrand * 4;
        long a = EnduranceMatchTreasure8[r + ((boyrand2 + 0) & 3)];
        long b = EnduranceMatchTreasure8[r + ((boyrand2 + 1) & 3)];
        long c = EnduranceMatchTreasure8[r + ((boyrand2 + 2) & 3)];

        Character2    = a;
        PLAYER2MODEL = a;
        ROUNDPARAM_B[0x18] = (char)b;
        ROUNDPARAM_B[0x19] = (char)c;
        ROUNDPARAM_B[0x1a] = (char)-1;
        EnduranceCharacters[0] = b;
        EnduranceCharacters[1] = c;
        EnduranceCharacters[2] = -1;
        LastEnduranceCharacter = c;
    } else if (TreasurePlayed == 9) {
        Character2    = 11;
        PLAYER2MODEL = 11;
        ROUNDPARAM_B[0x18] = 24;          /* the two ladder bosses */
        ROUNDPARAM_B[0x19] = 25;
        ROUNDPARAM_B[0x1a] = (char)-1;
        EnduranceCharacters[0] = 24;
        EnduranceCharacters[1] = 25;
        EnduranceCharacters[2] = -1;
        LastEnduranceCharacter = 25;
    }
}


extern void *SplashTexture1;            /* 0x001abb8c */
extern void *SplashTexture2;            /* 0x001abb90 */
extern long  SplashCount;               /* 0x00150cc8 */
extern int limeDeferredDeviceSideways;/* pointer slot */
extern int limeDeviceSideways;        /* pointer slot */
extern float  FE_YOffset;               /* pointer slot */
extern float FE_WidthScale;             /* 0x000ff9b8 */
extern float FE_HeightScale;            /* 0x000ff9bc */

void SetupFEScale(void);
void limeSetColourMask(long r, long g, long b, long a);
void Task_LoadingScreen(void);


/* ------------------------------------------------------- Task_LoadSplashScreen
 *
 * armv7 0x0001d594, 568 bytes.  **Complete.**
 *
 * The two publisher splash screens, driven entirely by one frame counter,
 * `_SplashCount`, which this function increments on **every** path including
 * the ones that draw nothing.
 *
 * The whole timeline, in frames:
 *
 *        0          load SPLASH1.PNG and SPLASH2.PNG, draw 1 at full alpha
 *        1 .. 120   SPLASH1, alpha 1
 *      121 .. 179   SPLASH1, alpha (180 - n) / 60      fading out
 *      180 .. 190   nothing drawn -- a deliberate gap
 *      191 .. 249   SPLASH2, alpha (n - 190) / 60      fading in
 *      250 .. 430   SPLASH2, alpha 1
 *      431 .. 489   SPLASH2, alpha (490 - n) / 60      fading out
 *      491 ..       delete both textures, CurrentTask = NextTask = 1,
 *                   Task_LoadingScreen(), delete LoadingTexture if present
 *
 * **The eleven-frame gap between the two logos is real**, not an artefact of
 * the fades meeting: 180..190 falls outside every drawing range. At 30 fps that
 * is about a third of a second of black, and it is what makes the two logos
 * read as separate rather than as a crossfade.
 *
 * All three fades divide by **60**, so each takes two seconds at 30 fps
 * regardless of how long the plateau either side of it is.
 *
 * ### Both orientation flags are forced sideways
 *
 *      limeDeferredDeviceSideways = 1
 *      limeDeviceSideways         = 1
 *
 * every frame, not once. So the splash cannot be rotated out from under itself
 * by anything else changing them.
 *
 * ### The sprite is a window into the texture, not the whole thing
 *
 *      limeDrawSprite(tex, 0, FE_YOffset,
 *                     480 * FE_WidthScale, 320 * FE_HeightScale,
 *                     0.03125f, 0.1875f, 0.9375f, 0.625f, colour)
 *
 * The four UVs are 1/32, 6/32, 30/32 and 20/32 -- an inset window, so the
 * shipped PNG is larger than the part that is shown and the margins are never
 * sampled. A port that draws 0..1 gets the logo surrounded by whatever the
 * padding contains.
 *
 * 480 x 320 is the device resolution written as literals and scaled at draw
 * time, which is how this survives a different screen.
 *
 * `limeSetColourMask(1, 1, 1, 0)` leaves the alpha channel unwritten -- the
 * fade is in the vertex colour, not in the framebuffer alpha.
 *
 * The colour block is written **twice**: copied from the function's own static
 * `{1, 1, 1, 1}` and then filled with four more explicit 1.0 stores before the
 * alpha is overwritten. Transcribed once; the duplication does nothing.
 */
extern float PortFEExtraX;             /* FrontEnd.c, port only */

void Task_LoadSplashScreen(void)
{
    float colour[4];
    long n;
    void *tex;

    colour[0] = colour[1] = colour[2] = colour[3] = 1.0f;   /* C.243 */

    SetupFEScale();
    limeDeferredDeviceSideways = 1;
    limeDeviceSideways         = 1;

    limeEnableAlphaBlending_Basic();
    limeSetColourMask(1, 1, 1, 0);
    limeSet2DDrawing();

    n = SplashCount;

    if (n == 0) {
        SplashTexture1 = limeLoadTexture("SPLASH1.PNG", 1, 0);
        SplashTexture2 = limeLoadTexture("SPLASH2.PNG", 1, 0);
        tex = SplashTexture1;
    } else if (n > 179) {
        if ((unsigned long)(n - 191) <= 298) {          /* 191 .. 489 */
            if (n <= 249)
                colour[3] = (float)(n - 190) / 60.0f;   /* fading in */
            else if (n > 430)
                colour[3] = (float)(490 - n) / 60.0f;   /* fading out */
            tex = SplashTexture2;
        } else if (n <= 490) {
            SplashCount++;                              /* the gap: nothing */
            return;
        } else {
            limeDeleteTexture(SplashTexture1);
            limeDeleteTexture(SplashTexture2);
            CurrentTask = 1;
            NextTask    = 1;
            Task_LoadingScreen();
            if (LoadingTexture != 0)
                limeDeleteTexture(LoadingTexture);
            SplashCount++;
            return;
        }
    } else if (n > 120) {
        colour[3] = (float)(180 - n) / 60.0f;           /* fading out */
        tex = SplashTexture1;
    } else {
        tex = SplashTexture1;
    }

    /* Port: centred on a surface wider than 3:2 (FrontEnd.c FE_X);
     * PortFEExtraX is 0 otherwise, so x stays 0 as in the binary. */
    limeDrawSprite(tex, PortFEExtraX * 0.5f, FE_YOffset,
                   480.0f * FE_WidthScale,
                   320.0f * FE_HeightScale,
                   0.03125f, 0.1875f, 0.9375f, 0.625f,
                   colour);

    SplashCount++;
}


/* `GameObjects` and `limeScreenWidth` are declared above. */
extern long  AverageFPSCount;           /* 0x0014e1d0 */
extern float AverageFPS;                /* 0x0014e1d4 */
extern float DisplayAverageFPS;         /* 0x0014e1d8 */
extern long  ToggleDebug;               /* 0x00150588 */
extern long  AIOn;                      /* 0x0014e1f4 */
extern long  SpeedNormal;               /* 0x0014e1f8 */
extern long  JoystickState;             /* 0x0014febc */
extern float limeFPS;                  /* pointer slot */
extern long limeRenderedPolyCount;    /* pointer slot */
extern GAMEFONT NameFont;               /* 0x001c3bf4 */
extern float  fontcol[];                /* 0x0014f9f0 */
extern char   str[];                    /* 0x001f3cac, the shared scratch buffer */

int  sprintf(char *dst, const char *fmt, ...);
void limeDrawFONT(void *font, const char *text, float x, float y,
                  long align, float scale, const float *colour);


/* -------------------------------------------------------------- ShowDebugInfo
 *
 * armv7 0x0001db84, 572 bytes.  **Complete.**
 *
 * The developer overlay, and it shipped: poly count, joystick bits, AI mode and
 * game speed drawn over the fight in `_NameFont`.
 *
 * ### The FPS average runs even when the overlay is off
 *
 *      AverageFPS += limeFPS
 *      if (++AverageFPSCount == 10) {
 *          DisplayAverageFPS = AverageFPS / 10
 *          AverageFPSCount = 0
 *          AverageFPS = 0
 *      }
 *      if (!ToggleDebug) return
 *
 * The accumulation happens **before** the `ToggleDebug` check, so
 * `_DisplayAverageFPS` is correct the moment the overlay is switched on rather
 * than needing ten frames to settle. That ordering is the only reason the
 * number is not garbage on the first frame.
 *
 * The whole function returns immediately if `_GameObjects` is null -- so the
 * average does not accumulate outside a fight either.
 *
 * **Ten frames, hardcoded**, and the divisor is the `vmov.f32 s12, #10.0`
 * immediate rather than a constant anywhere in the data.
 *
 * ### What it draws
 *
 *      "Poly Count %d  Joy: %d"    at FE_Y(96),  left,  size 8
 *      "AI: Human vs Human"        at FE_Y(112)          AIOn == 0
 *      "AI: Human vs CPU"                                AIOn == 1
 *      "AI: CPU vs CPU"                                  AIOn == 2
 *      "Speed: Normal"             at FE_Y(96),  right   SpeedNormal != 0
 *      "Speed: Slow-mo"                                  SpeedNormal == 0
 *
 * The two speed lines are right-aligned (`align` 2) at
 * `limeScreenWidth - 8`, and **-8 is a raw pixel inset with no scaler on it** --
 * the same shape as the leaderboard's unscaled left margin and the dial's
 * unscaled P2 radius. Three separate places where one dimension of a pair is
 * scaled and the other is not.
 *
 * `_str` at 0x001f3cac is a shared scratch buffer and `sprintf` is unbounded,
 * which is normal for this tree.
 */
void ShowDebugInfo(void)
{
    if (GameObjects == 0)
        return;

    AverageFPSCount++;
    AverageFPS += limeFPS;

    if (AverageFPSCount == 10) {
        DisplayAverageFPS = AverageFPS / 10.0f;
        AverageFPSCount   = 0;
        AverageFPS        = 0.0f;
    }

    if (ToggleDebug == 0)
        return;                         /* the average has already been kept */

    sprintf(str, "Poly Count %d  Joy: %d",
            (int)limeRenderedPolyCount, (int)JoystickState);
    limeDrawFONT(&NameFont, str, 8.0f, (float)FE_Y(96.0f), 0, 1.0f, fontcol);

    if (AIOn == 2)
        limeDrawFONT(&NameFont, "AI: CPU vs CPU", 8.0f, (float)FE_Y(112.0f),
                     0, 1.0f, fontcol);
    else if (AIOn == 1)
        limeDrawFONT(&NameFont, "AI: Human vs CPU", 8.0f, (float)FE_Y(112.0f),
                     0, 1.0f, fontcol);
    else
        limeDrawFONT(&NameFont, "AI: Human vs Human", 8.0f, (float)FE_Y(112.0f),
                     0, 1.0f, fontcol);

    if (SpeedNormal != 0)
        limeDrawFONT(&NameFont, "Speed: Normal",
                     (float)(limeScreenWidth - 8), (float)FE_Y(96.0f),
                     2, 1.0f, fontcol);
    else
        limeDrawFONT(&NameFont, "Speed: Slow-mo",
                     (float)(limeScreenWidth - 8), (float)FE_Y(96.0f),
                     2, 1.0f, fontcol);
}


extern long  LastSpecialButton[2];      /* 0x00150e98 */
extern long  Player1NumButtons;         /* 0x0010de64 */
extern long  Player2NumButtons;         /* 0x0010de68 */
extern MKMOVE FourButtonMoves[];        /* 0x0015092c */

int isParentBasedOnSpeed(void);


/* ------------------------------------------------------ GetReal6ButtonJoyBits
 *
 * armv7 0x0001e3cc, 576 bytes.  **Complete.**
 *
 * Builds the raw six-button input word from the dial direction and the button
 * array, then converts the directions to facing-relative form.
 *
 * ### The dial is eight-way and diagonals set two bits
 *
 *      dir in {8, 1, 2}   |= 0x01   up
 *      dir in {4, 5, 6}   |= 0x02   down
 *      dir in {6, 7, 8}   |= 0x04   left
 *      dir in {2, 3, 4}   |= 0x08   right
 *
 * Each range is three wide and they overlap at the corners, so `dir == 2` sets
 * up **and** right. Direction 0 sets nothing -- neutral. Every test is written
 * as an unsigned range check (`subs` then `cmp ... #2`), which is how one
 * comparison covers three values.
 *
 * The six buttons are `buttons[0..5]` into bits 0x10 through 0x200 -- that is
 * the "6Button" in the name.
 *
 * ### This is the exact inverse of GetArcadeJoyBits' tail
 *
 *      if (LEFT)  set (facing & 0x10) ? 0x0800 : 0x1000
 *      if (RIGHT) set (facing & 0x10) ? 0x1000 : 0x0800
 *
 * 0x800 is TOWARD and 0x1000 is AWAY -- the same two bits `GetArcadeJoyBits`
 * converts back into LEFT and RIGHT on the way out, with the same facing flag.
 * **One function turns absolute into relative on input and the other turns
 * relative into absolute on output**, and neither makes sense without the
 * other. Both are needed, in that order, or the fighter mirrors his own inputs.
 *
 * The facing flag lives at `GameObjects[player * 16 + 0x0a]`, so the two
 * players' object records are sixteen bytes apart.
 *
 * ### 0x400 is edge-triggered
 *
 *      if (buttons[6] && LastSpecialButton[p] == 0) bits |= 0x400;
 *      LastSpecialButton[p] = buttons[6];
 *
 * The bit is set only on the rising edge, and the store happens on **every**
 * path including when the button is released -- so a port that only updates
 * `LastSpecialButton` when the button is down latches the bit forever.
 *
 * ### Fewer than six buttons goes back through GetArcadeJoyBits
 *
 * With `PlayerNNumButtons` at 4 or 5 the assembled word is run through
 * `GetArcadeJoyBits(bits, FourButtonMoves, facing & 0x10, 1)` to synthesise the
 * buttons the player does not have. At 6 nothing extra happens.
 *
 * The last argument is **1 in all four cases**, but spelled two ways: the
 * four-button arms compute it as `NumButtons - 3` and the five-button arms use
 * a literal 1. `NumButtons - 3` would be 2 at five buttons -- that expression is
 * simply never reached with a 5. Transcribed as the constant it always is.
 *
 * The four-button arms then fall through into the five-button test, which
 * cannot also be true. Dead, harmless, and left as written.
 *
 * ### Which player
 *
 *      isParentBasedOnSpeed()  ->  p = (the fifth argument != 0)
 *      otherwise               ->  p = (GameMode == 1)
 *
 * so in single player the player index comes from the game mode rather than
 * from the caller.
 */
void GetReal6ButtonJoyBits(int dir, const int *buttons, Mk3Obj_t *unused,
                           long *out, int which)
{
    long bits = *out;
    long p;
    const unsigned short *obj;

    (void)unused;

    if (dir == 8 || (unsigned)(dir - 1) <= 1) bits |= 0x01;     /* up    */
    if ((unsigned)(dir - 4) <= 2)             bits |= 0x02;     /* down  */
    if ((unsigned)(dir - 6) <= 2)             bits |= 0x04;     /* left  */
    if ((unsigned)(dir - 2) <= 2)             bits |= 0x08;     /* right */

    if (buttons[0]) bits |= 0x010;
    if (buttons[1]) bits |= 0x020;
    if (buttons[2]) bits |= 0x040;
    if (buttons[3]) bits |= 0x080;
    if (buttons[4]) bits |= 0x100;
    if (buttons[5]) bits |= 0x200;

    *out = bits;

    p = isParentBasedOnSpeed() ? (which != 0) : (GameMode == 1);

    obj = (const unsigned short *)(void *)GameObjects;
    if (obj != 0) {
        long facing = obj[(p * 16 + 0x0a) / 2] & 0x10;

        if (bits & 0x04)                        /* left  */
            bits |= facing ? 0x0800 : 0x1000;
        if (bits & 0x08)                        /* right */
            bits |= facing ? 0x1000 : 0x0800;

        *out = bits;
    }

    if (buttons[6] != 0 && LastSpecialButton[p] == 0)
        *out |= 0x400;                          /* the rising edge only */
    LastSpecialButton[p] = buttons[6];           /* stored either way */

    obj = (const unsigned short *)(void *)GameObjects;
    if (obj == 0)
        return;

    if (p) {
        if (Player2NumButtons == 4 || Player2NumButtons == 5)
            *out = GetArcadeJoyBits(*out, FourButtonMoves,
                                    obj[0x1a / 2] & 0x10, 1);
    } else {
        if (Player1NumButtons == 4 || Player1NumButtons == 5)
            *out = GetArcadeJoyBits(*out, FourButtonMoves,
                                    obj[0x0a / 2] & 0x10, 1);
    }
}


/* ### More of PLAYER, from the reading side
 *
 * `PlayerAutoSmoothAnims` writes +0x51c, +0x520 and +0x524; this function is
 * what reads them, and it hands all three straight to
 * `RenderAnimatedCharacter` as its two frames and blend factor. The chain is
 * closed: the smoother decides, the renderer interpolates.
 *
 *      +0x51c  long   frameA          written by PlayerAutoSmoothAnims
 *      +0x520  long   frameB
 *      +0x524  float  t
 *      +0x528  TEXTURE *skin
 *      +0x52c  long            (passed to LIME_RenderScene)
 *      +0x540  long   mirrored        also read by DoSmokesSmoke
 *      +0x548  float  matrix[16]      multiplied straight onto MODELVIEW
 *      +0x5d0  float  shadowY
 *      +0x5d4  float  grey            AnimateFECharacters sets this to 1.0f
 *      +0x5d8  float  pos[3]
 */
typedef struct ANIMATEDCHARACTER {
    long        meshCount;      /* 0x00 */
    void       *meshes;         /* 0x04  meshCount records of 0x58 */
    long        id;             /* 0x08  the character, for the frame gates */
    long        field0c;        /* 0x0c  nothing reads it */
    void       *scene;          /* 0x10 */
    void       *diffuse;        /* 0x14 */
    void       *diffuseIce;     /* 0x18 */
    void       *jadeGreen;      /* 0x1c  JADE only */
    void       *sindelHair;     /* 0x20  SINDEL only */
    void       *diffuse2;       /* 0x24  the lite path's second sheet */
    void       *babality;       /* 0x28  may stay NULL */
    void       *frameList;      /* 0x2c */
    void       *skin;           /* 0x30 */
    void       *bones;          /* 0x34 */
    const long *meshbase;       /* 0x38  kept so it can be freed */
    const long *meshTable;      /* 0x3c */
    const long *meshNames;      /* 0x40 */
} ANIMATEDCHARACTER;            /* 0x44 = 68 bytes, the size limeMalloc asks */

typedef struct PLAYER PLAYER;

int IsFrameVisible(ANIMATEDCHARACTER *c, long a, long b);
void RenderAnimatedCharacter(char *name, ANIMATEDCHARACTER *c,
                             long frameA, long frameB, float t,
                             float y, float grey, limeVECTOR3 *pos,
                             void *tex, long visible);
extern float AttachTransforms[];        /* 0x0018ee00 */
/* `_MatrixPalette2` 0x002c3f48, 150 matrices of 48 bytes. RenderPlayer's
 * memcpy source is one load of the slot at 0x000f34ac, whose contents are
 * the table's own address (0x00023d72): the table, not a pointer in it.
 * Defined in lime/lime_globals.c, where DrawSkinnedMesh2 reads it. */
extern char  MatrixPalette2[];

void LIME_PushMatrix(void);
void LIME_PopMatrix(int count);
void LIME_KillAllLights(void);
void LIME_RenderScene(long a, void *scene, long frameA, long frameB, float t,
                      long b, long c, long d, void *tex, long e, float *att);
void limeDisableAlphaBlending(void);
void limeEnableDepthWrites(void);
void LIME_GLAPI glMatrixMode(unsigned int m);
void LIME_GLAPI glMultMatrixf(const float *m);
#define GL_MODELVIEW 0x1700


/* --------------------------------------------------------------- RenderPlayer
 *
 * armv7 0x00023c7c, 620 bytes.  **Complete.**
 *
 * Draws one fighter and, optionally, whatever is attached to him.
 *
 *      LIME_PushMatrix()
 *      glMatrixMode(GL_MODELVIEW)
 *      glMultMatrixf(p + 0x548)            <- the player's own 4x4
 *      glCullFace(p->mirrored ? GL_FRONT : GL_BACK)
 *      limeDisableAlphaBlending()
 *      limeEnableDepthWrites()
 *      ... RenderAnimatedCharacter(...)
 *      glCullFace(GL_BACK)
 *      LIME_KillAllLights()
 *      LIME_PopMatrix(1)
 *
 * **The mirrored fighter flips the cull face here too**, from the same +0x540
 * flag, and restores GL_BACK unconditionally on the way out -- the identical
 * discipline `RenderAMesh` uses. Two independent draw paths, one rule.
 *
 * ### Four call sites, one difference
 *
 * `RenderAnimatedCharacter` is called from four places in this function and
 * every argument but the last is the same. The last is its `visible` flag:
 *
 *      IsFrameVisible said yes, `flag` set      ->  0
 *      IsFrameVisible said yes, `flag` clear    ->  1
 *      character 12 on frame 295                ->  1, and a DIFFERENT name
 *      IsFrameVisible said no                   ->  whatever it returned
 *
 * The third is the per-character exception again -- **12 and 295, the same pair
 * `IsFrameVisible` special-cases** -- and here it does something concrete: the
 * name passed is `PlayerDefs[0x288]` rather than the fighter's own
 * `PlayerDefs[id].name`. A fixed entry, not indexed by character. So that
 * combination draws with somebody else's asset name.
 *
 * A `frameA` of -1 skips the draw entirely and goes straight to the cleanup.
 *
 * ### The attachment pass copies the whole palette first
 *
 *      memcpy(AttachTransforms, MatrixPalette2, 0x1c20)
 *      LIME_RenderScene(6, anim->[0x10], frameA, frameB, t, 0, 0, 0,
 *                       skin, +0x52c, AttachTransforms)
 *
 * 0x1c20 is 7,200 bytes -- 150 matrices of 48, the 3x4 skin matrix this tree
 * already knows. So the attachment is posed against a **snapshot** of the
 * palette taken after the fighter was drawn, not against the live one; anything
 * that moves the palette between the two draws does not affect the attachment.
 *
 * Returns `p->anim->[0x34][1]`, read after everything else is done.
 */
long RenderPlayer(PLAYER *p, long attach, long flag)
{
    long *w = (long *)p;
    ANIMATEDCHARACTER *c = (ANIMATEDCHARACTER *)(uintptr_t)
                           (unsigned long)w[1];
    float grey = ((float *)w)[0x5d4 / 4];
    const PLAYERDEF *def;
    long vis;

    LIME_PushMatrix();
    glMatrixMode(GL_MODELVIEW);
    glMultMatrixf((const float *)&w[0x548 / 4]);

    glEnable(GL_CULL_FACE);
    glCullFace(w[0x540 / 4] != 0 ? GL_FRONT : GL_BACK);

    limeDisableAlphaBlending();
    limeEnableDepthWrites();

    vis = IsFrameVisible(c, w[0x51c / 4], w[0x520 / 4]);
    def = &PlayerDefs[w[0]];

    if (vis == 0) {
        RenderAnimatedCharacter(def->lighting, c,
                                w[0x51c / 4], w[0x520 / 4],
                                ((float *)w)[0x524 / 4],
                                ((float *)w)[0x5d0 / 4], grey,
                                (limeVECTOR3 *)&w[0x5d8 / 4],
                                (void *)(uintptr_t)(unsigned long)w[0x528 / 4],
                                vis);
    } else if (w[0x51c / 4] != -1) {
        if (w[0x14 / 4] == 0x127 && w[0] == 12) {
            /* the 12/295 exception: somebody else's asset name */
            RenderAnimatedCharacter(
                    PlayerDefs[12].lighting, c,
                    w[0x51c / 4], w[0x520 / 4],
                    ((float *)w)[0x524 / 4],
                    ((float *)w)[0x5d0 / 4], grey,
                    (limeVECTOR3 *)&w[0x5d8 / 4],
                    (void *)(uintptr_t)(unsigned long)w[0x528 / 4], 1);
        } else {
            RenderAnimatedCharacter(def->lighting, c,
                                    w[0x51c / 4], w[0x520 / 4],
                                    ((float *)w)[0x524 / 4],
                                    ((float *)w)[0x5d0 / 4], grey,
                                    (limeVECTOR3 *)&w[0x5d8 / 4],
                                    (void *)(uintptr_t)(unsigned long)
                                        w[0x528 / 4],
                                    flag ? 0 : 1);
        }

        if (attach != 0) {
            memcpy(AttachTransforms, MatrixPalette2, 0x1c20);
            LIME_RenderScene(6,
                             c->scene,
                             w[0x51c / 4], w[0x520 / 4],
                             ((float *)w)[0x524 / 4], 0, 0, 0,
                             (void *)(uintptr_t)(unsigned long) w[0x528 / 4],
                             w[0x52c / 4], AttachTransforms);
        }
    }

    glCullFace(GL_BACK);
    LIME_KillAllLights();
    LIME_PopMatrix(1);

    return ((const long *)c->bones)[1];
}


/* ### The second background layer has its own everything
 *
 * This function is the first place in the tree where `_BGSceneHandle2` is
 * driven rather than merely mentioned, and it settles the shape of the pair:
 *
 *      layer 0   BGSceneHandle,  SceneScale,  SceneX,  SceneY,  SceneZ
 *                -> RealBGSceneMatrix + 0x00
 *      layer 1   BGSceneHandle2, SceneScale2, SceneX2, SceneY2, SceneZ2
 *                -> RealBGSceneMatrix + 0x40
 *
 * so `_RealBGSceneMatrix` is TWO 4x4 matrices, and the second layer is
 * positioned and scaled entirely independently of the first. See issue #17. */
extern void  *BGSceneHandle;            /* 0x001aba40 */
extern void  *BGSceneHandle2;           /* 0x001aba44 */
extern long   BGSceneLoops[2];          /* 0x001aba80 */
extern float  BGSceneFrame[2];          /* 0x001abb20 */
extern long   BGSceneController[];      /* 0x001aba88, twelve bytes an entry */
extern float  RealBGSceneMatrix[32];    /* 0x001abaa0, two 4x4 */
extern float  M_Rot90[16];              /* 0x0018edc0 */
extern long   ExtraEffects;             /* 0x0014e1e8 */
extern float  SceneScale,  SceneX,  SceneY,  SceneZ;    /* 0x0014dfd8 .. */
extern float  SceneScale2, SceneX2, SceneY2, SceneZ2;   /* 0x0014dfe8 .. */

void RotMatrixX(float *m, float angle);
void limeMatrixLoadIdentity(float *m);
void limeMatrixMult(const float *a, const float *b, float *out);
void limeScaleMatrix(float *m, float s);
void LIME_TriggerEventsFromScene(void *scene, int frame, const float *m,
                                 long flags, long a4, long a5, long a6, long a7);


/* ------------------------------------------------------------------ AnimateBG
 *
 * armv7 0x00021c1c, 672 bytes.  **Complete.**
 *
 * Advances the background scene animation, **for both layers**, and fires their
 * event tracks. Returns immediately when `_ExtraEffects` is clear or the first
 * scene handle is null.
 *
 * ### Building the two matrices
 *
 *      RotMatrixX(M_Rot90, 1.5707964f)         <- PI/2, from the pool
 *      identity -> multiply by M_Rot90 -> scale by SceneScale
 *      translate by (SceneX, SceneY, SceneZ)   -> RealBGSceneMatrix[0]
 *      identity -> scale by SceneScale2
 *      translate by (SceneX2, SceneY2, SceneZ2) -> RealBGSceneMatrix[0x40]
 *
 * **Only the first layer gets the X rotation.** The second is built from a
 * fresh identity and never sees `M_Rot90`, so the two layers do not share an
 * orientation -- which is the sort of thing a port reproduces by accident only
 * if it copies the sequence rather than the intent.
 *
 * The translation is written **directly into elements 12, 13 and 14** of the
 * identity matrix before the multiply, not applied through a call -- the stores
 * are at `sp+0x40` with the matrix at `sp+0x10`, so they land on the
 * translation row of the 4x4. Reading those offsets as if they were relative to
 * the matrix pointer puts them past its end; gcc's `-Warray-bounds` caught
 * exactly that mistake here before it was committed.
 *
 * ### The per-layer loop
 *
 * For each layer, if its frame counter has crossed into a new integer it calls
 *
 *      LIME_TriggerEventsFromScene(handle, (long)frame, matrix,
 *                                  0, -1, LevelSelect == 2, 0, 0)
 *
 * -- and the second layer passes `0` where the first passes `0` too, but from a
 * different literal (`r3 = -1` then `adds r3, #1`), so the two call sites are
 * genuinely separate code rather than a shared tail.
 *
 * **`LevelSelect == 2` is passed as a flag to both.** Whatever stage 2 is, it
 * changes how the background events are triggered.
 *
 * Then the counter advances by exactly `1.0f` a call -- **not scaled by frame
 * time**, unlike `MaintainFESlide`. The background animation is therefore tied
 * to the frame rate, and a 60 fps port runs it at double speed unless this is
 * changed deliberately.
 *
 * ### Non-looping scenes stop on the last frame
 *
 * When `BGSceneLoops[layer]` is zero the counter is clamped to
 * `scene->count - 1` (`+0x44` of the scene, the same field `HUDANIM_Update`
 * reads) and the layer stops advancing. Looping scenes are never clamped here
 * at all -- the wrap happens inside `LIME_TriggerEventsFromScene`.
 *
 * The loop ends after layer 1, and layer 1 is skipped entirely when
 * `_BGSceneHandle2` is null -- which is the normal case for most stages and is
 * why the second layer went unnoticed for so long.
 */
void AnimateBG(void)
{
    long stage2;
    long i;

    if (ExtraEffects == 0)
        return;

    stage2 = (*LevelSelectPtr == 2);

    if (BGSceneHandle == 0)
        return;

    {
        float rot[16], tmp[16], acc[16];

        RotMatrixX(M_Rot90, 1.5707964f);        /* 0x3fc90e55 */

        limeMatrixLoadIdentity(tmp);
        limeMatrixMult(M_Rot90, tmp, rot);
        limeScaleMatrix(rot, SceneScale);

        limeMatrixLoadIdentity(acc);
        acc[12] = SceneX;   /* sp+0x40, the matrix being at sp+0x10 */
        acc[13] = SceneY;
        acc[14] = SceneZ;
        limeMatrixMult(acc, rot, RealBGSceneMatrix);

        limeMatrixLoadIdentity(acc);            /* layer 2: no rotation */
        acc[12] = SceneX2;
        acc[13] = SceneY2;
        acc[14] = SceneZ2;
        limeScaleMatrix(acc, SceneScale2);
        limeMatrixMult(acc, RealBGSceneMatrix, &RealBGSceneMatrix[0x40 / 4]);
    }

    for (i = 0; i < 2; i++) {
        void *handle = (i == 0) ? BGSceneHandle : BGSceneHandle2;
        long *ctl = &BGSceneController[i * 3];
        float *m  = (i == 0) ? RealBGSceneMatrix
                             : &RealBGSceneMatrix[0x40 / 4];

        if (i == 1 && BGSceneHandle2 == 0)
            return;
        if (handle == 0)
            continue;

        if (BGSceneLoops[i] == 0) {
            float last = (float)(((const long *)handle)[0x44 / 4] - 1);
            if (BGSceneFrame[i] >= last) {
                BGSceneFrame[i] = last;         /* stop on the last frame */
                continue;
            }
        }

        if ((long)BGSceneFrame[i] != ctl[2]) {
            LIME_TriggerEventsFromScene(handle, (long)BGSceneFrame[i], m,
                                        0, -1, stage2, 0, 0);
            ctl[2] = (long)BGSceneFrame[i];
        }
        ctl[1] = (long)BGSceneFrame[i];

        BGSceneFrame[i] += 1.0f;                /* not frame-rate scaled */
    }
}


extern long   DrawSpear[2];             /* 0x0010deec */
extern long   SpearWhichTexture[2];     /* 0x0010def4 */
extern float  SpearStartPos[2][3];      /* 0x001ab63c */
extern float  SpearEndPos[2][3];        /* 0x001ab624 */
extern void  *SpearTexture[];           /* 0x001ab654 */
extern float  FaceMeMatrix[];             /* pointer slot */
extern float  ShadowOffset;             /* 0x0014dfc8 */

/* (tex, m, x, y, z, u0, v0, du, dv, w, h, r, g, b, a) plus two words the call
 * stores and the callee never reads -- every one a float, which matters off
 * ARM: on x86-64 a float and a long travel in different registers. Read off
 * limeDrawFaceMeSpriteWH (0x0006649c) and its call at 0x000210cc. */
void limeDrawFaceMeSpriteWH(void *tex, const float *m, float x, float y,
                            float z, float u0, float v0, float du, float dv,
                            float w, float h, float r, float g, float b,
                            float a, float unused0, float unused1);


/* ------------------------------------------------------------- RenderExtras
 *
 * armv7 0x00020fa8, 636 bytes.  **Complete.**
 *
 * Draws Scorpion's spear -- the only thing in "extras" -- for both players, as
 * two face-me sprites: a stretched shaft and a fixed-size tip.
 *
 * Per player, and it is skipped entirely unless `_DrawSpear[player]` is set:
 *
 * ### The direction test is inverted by the mirror flag
 *
 *      mirrored (+0x540 set):  draw only while start.x >= end.x  (0x20ffc bpl)
 *      not mirrored:           draw only while start.x <= end.x  (0x21044 bgt)
 *
 * The mirrored test was written inverted, and player one is the mirrored
 * one, so Scorpion's spear was never drawn.
 *
 * The same +0x540 that flips the cull face in `RenderPlayer` and picks the
 * smoke offset in `DoSmokesSmoke`. Here it decides which way the spear is
 * allowed to point, so a spear travelling the wrong way for the fighter's
 * facing is simply not drawn.
 *
 * ### The shaft
 *
 * The texture is `SpearTexture[SpearWhichTexture[player] % 3]` -- **three
 * shaft textures cycled**, and the modulo is a reciprocal multiply with magic
 * `0x55555556` and no shift, which is 3 worked out numerically rather than
 * assumed.
 *
 * The sprite is stretched: its width argument is `start.x - end.x`, so it spans
 * whatever the spear currently covers, and its Y is
 * `ShadowOffset + 1.35` computed in **double** and narrowed at the call. The
 * 1.35 is a double literal in the pool, not a float widened.
 *
 * ### The tip
 *
 * A second sprite, drawn only when `SpearWhichTexture[player] % 3 <= 1` -- so
 * one of the three shaft phases has no tip at all. It uses
 * `SpearTexture[3]` or `SpearTexture[4]` and a width of **+0.25 or -0.25**,
 * chosen by comparing the same two X positions again and by whether
 * `SpearWhichTexture` is above 2. The sign is what points the tip.
 *
 * So `_SpearTexture` holds at least five entries: three shaft phases and two
 * tip orientations.
 *
 * Everything else in the long argument list is 1.0f or 0, written slot by slot;
 * the parameter names of `limeDrawFaceMeSpriteWH` are not established, so the
 * call below is transcribed positionally.
 */
void RenderExtras(void)
{
    long i;
    char *p = Players;

    limeEnableAlphaBlending_Basic();

    for (i = 0; i < 2; i++, p += 0x5f0) {
        float y;
        long phase;
        void *tex;

        if (DrawSpear[i] == 0)
            continue;

        if (((const long *)p)[0x540 / 4] != 0) {
            if (SpearStartPos[i][0] < SpearEndPos[i][0])
                continue;
        } else {
            if (SpearStartPos[i][0] > SpearEndPos[i][0])
                continue;
        }

        phase = SpearWhichTexture[i] % 3;       /* magic 0x55555556 */
        tex   = SpearTexture[phase];
        y     = (float)((double)ShadowOffset + 1.35);

        /* the shaft: width is however far the spear reaches */
        limeDrawFaceMeSpriteWH(tex, FaceMeMatrix, SpearEndPos[i][0], 0.0f,
                               y, 0.0f, 0.0f, 1.0f, 1.0f,
                               SpearStartPos[i][0] - SpearEndPos[i][0],
                               0.25f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.25f);

        if (phase > 1)
            continue;                           /* one phase has no tip */

        if (SpearWhichTexture[i] > 2) {
            if (SpearStartPos[i][0] < SpearEndPos[i][0]) {
                tex = SpearTexture[4];
                limeDrawFaceMeSpriteWH(tex, FaceMeMatrix, SpearEndPos[i][0], 0.0f,
                                       y, 0.0f, 0.0f, 1.0f, 1.0f, 0.25f,
                                       0.25f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.25f);
            } else {
                tex = SpearTexture[3];
                limeDrawFaceMeSpriteWH(tex, FaceMeMatrix, SpearEndPos[i][0], 0.0f,
                                       y, 0.0f, 0.0f, 1.0f, 1.0f, 0.25f,
                                       0.25f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.25f);
            }
        } else {
            tex = SpearTexture[3];
            if (SpearStartPos[i][0] < SpearEndPos[i][0])
                limeDrawFaceMeSpriteWH(tex, FaceMeMatrix, SpearEndPos[i][0], 0.0f,
                                       y, 0.0f, 0.0f, 1.0f, 1.0f, 0.25f,
                                       0.25f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.25f);
            else
                limeDrawFaceMeSpriteWH(tex, FaceMeMatrix, SpearEndPos[i][0], 0.0f,
                                       y, 0.0f, 0.0f, 1.0f, 1.0f, -0.25f,
                                       0.25f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.25f);
        }
    }
}


extern long   GamePaused;               /* 0x0014e1fc */
extern long   PauseMenuAreYouSure;      /* 0x0014e260 */
extern long   DidIntroThisFrame;        /* 0x0010dec4 */
extern float  HUD_Scale;                /* pointer slot */
extern int    PortIPad;                 /* FrontEnd.c: 4:3 is the iPad build's */

/* The pause and moves-list corners: 80 wide on the iPhone, 40 in the iPad
 * build (TogglePauseMenu 0x1e1fc / 0x1e2ae there, both vldr of 40.0f). */
#define PAUSE_CORNER  (PortIPad ? 40.0f : 80.0f)
extern float *FE_FadeAddP;              /* pointer slot -> 0x0010089c */
extern float  limeLastTouchScreenX[];     /* pointer slot */
extern float  limeLastTouchScreenY[];     /* pointer slot */
extern long  SFXHandle[];               /* 0x001ab99c -- an ARRAY of sound handles.
                                         * Every site reaches it as `ldr r3,[slot]`
                                         * then `ldr r0,[r3,#0x68]`: the slot holds
                                         * the ADDRESS of the array, so declaring it
                                         * `long *` dereferenced once too many. */
extern int    Settings[10];             /* 0x00100e34 */
extern float MusicVol[];                /* 0x000ff830 -- an ARRAY, same correction:
                                         * `add r1,pc` puts the array address in r1
                                         * and the volume is `[r1 + idx*4]`. */

void limePlaySound(long id, float vol, float pan, long flags);

void sendPause(long state);
void EASDK_LogEventEnumEnumString(long id, long a, const char *s1,
                                  long b, const char *s2);


/* ----------------------------------------------------------- TogglePauseMenu
 *
 * armv7 0x0001e134, 664 bytes.  **Complete.**
 *
 * Two hot corners at the top of the screen, tested on **touch release**:
 *
 *      top RIGHT   x > limeScreenWidth - HUD_Scale * 80,  y < HUD_Scale * 64
 *                  -> GamePaused = 1, PauseMenuAreYouSure = 0
 *                  -> logs "INGAME MENU" / "PAUSE BUTTON"
 *
 *      top LEFT    x < HUD_Scale * 80,                    y < HUD_Scale * 64
 *                  -> GamePaused = 2
 *                  -> logs "INGAME MENU" / "MOVES INFO"
 *
 * So **the pause menu and the moves list are two different values of the same
 * flag**, 1 and 2, reached from opposite corners of the same 80x64 shape. Both
 * corners are scaled by `HUD_Scale`, both dimensions -- unlike the three
 * unscaled insets recorded in issue #22.
 *
 * ### The event id is 50016, and that matches the captured log
 *
 *      EASDK_LogEventEnumEnumString(0xc360, 15, "INGAME MENU", 15, "MOVES INFO")
 *
 * 0xc360 is 50016. The log lines quoted in issue #5 -- `LOGGING (50016):
 * INGAME MENU, MOVES INFO` -- are emitted from exactly here, which pins the
 * capture to this function and confirms the two strings are literals rather
 * than anything computed.
 *
 * ### Three guards before either corner counts
 *
 *      FE_FadeAdd != 0                 no input during a screen fade
 *      limeTouchScreenX[0] != -1       only a RELEASE counts, never a hold
 *      DidIntroThisFrame               swallowed on the intro frame
 *
 * The fade guard is the same one `drawPage2x1Wide` and
 * `drawPage2x2BigForSettings` use, and it is re-tested **after** the pause
 * branch as well -- so opening the pause menu cannot also trigger the moves
 * list in the same frame.
 *
 * The click sound is `SFXHandle[0x1a]` at `MusicVol[Settings[3]] / 100`, the
 * same one `TouchAreaWH` plays on release.
 *
 * ### sendPause is called three times
 *
 * In `GameMode == 1` the state goes out **three times in a row** -- and the
 * first call's argument is whatever was already in r0, not `GamePaused`; only
 * the second and third read the global. Transcribed as written. Three sends for
 * one state change is what an unreliable link gets instead of an ack, and the
 * stale first argument suggests it was written that way rather than looped.
 */
void TogglePauseMenu(void)
{
    float lx, ly;

    if (*FE_FadeAddP != 0.0f)
        return;
    if (limeTouchScreenX[0] != -1.0f)
        return;

    lx = limeLastTouchScreenX[0];

    if (lx != -1.0f
        && lx > (float)limeScreenWidth - HUD_Scale * PAUSE_CORNER
        && (ly = limeLastTouchScreenY[0]) < HUD_Scale * 64.0f
        && DidIntroThisFrame == 0) {

        if (Settings[3] != 0)
            limePlaySound(SFXHandle[0x68 / 4],
                          MusicVol[Settings[3]] / 100.0f, 1.0f, 0);

        GamePaused          = 1;
        PauseMenuAreYouSure = 0;

        if (GameMode == 1) {
            sendPause(0);               /* r0 is stale here */
            sendPause(GamePaused);
            sendPause(GamePaused);
        }

        EASDK_LogEventEnumEnumString(0xc360, 15, "INGAME MENU",
                                     15, "PAUSE BUTTON");

        if (*FE_FadeAddP != 0.0f)
            return;
        if (limeTouchScreenX[0] != -1.0f)
            return;
    }

    lx = limeLastTouchScreenX[0];
    if (lx == -1.0f)
        return;
    if (lx >= HUD_Scale * PAUSE_CORNER)
        return;
    if (limeLastTouchScreenY[0] >= HUD_Scale * 64.0f)
        return;
    if (DidIntroThisFrame != 0)
        return;

    if (Settings[3] != 0)
        limePlaySound(SFXHandle[0x68 / 4],
                      MusicVol[Settings[3]] / 100.0f, 1.0f, 0);

    GamePaused = 2;                     /* the moves list, not the menu */

    if (GameMode == 1) {
        sendPause(0);
        sendPause(GamePaused);
        sendPause(GamePaused);
    }

    EASDK_LogEventEnumEnumString(0xc360, 15, "INGAME MENU", 15, "MOVES INFO");
}


extern float *startTime;                /* pointer slot */
extern float  FrameCount;               /* 0x0014fa60 */
extern long  *readyToSync;              /* pointer slot */
extern const char *LevelMusic[];         /* 0x0014f8c8: names inside Level_Info's tail */
extern void  *GameFontP;                /* pointer slot -> 0x001abb98 */
extern float *limeFPSScaleFactorP;      /* pointer slot */

char *limeUC(const char *s);
long  usprintf(char *dst, const char *fmt, ...);

const char *UC(const char *s);
const char *GameTextNoHeader(long id);
long  syncGame(long frame);
void  enableHeartbeat(long n);
void  limePlayTune(const char *file, long vol, long arg);


/* -------------------------------------------------------- Task_MultiplayerSync
 *
 * armv7 0x00022514, 504 bytes.  **Complete.**
 *
 * The "waiting for the other player" screen. Draws the loading backdrop, one
 * centred line of text, and calls `syncGame(FrameCount)` every frame until it
 * returns 1.
 *
 * ### The text blinks on a 128-frame cycle
 *
 *      (FrameCount % 128) > 64  ?  <one string>  :  <the other>
 *
 * The modulo is signed and spelled out the way the compiler emits it
 * (`asr #31`, `lsr #25`, `add`, `and #0x7f`, `sub`) rather than as `% 128`.
 * Which pair of strings is used depends on `*startTime`:
 *
 *      startTime != 0    GameTextNoHeader(0x3b7)  alternating with `" "`
 *      startTime == 0    GameTextNoHeader(0x0e)   alternating with `" "`
 *
 * So it is a two-state blink between a message and a single space -- the
 * message flashes rather than animating, and the "%s" format is only there to
 * pass one or the other through `usprintf`.
 *
 * The result goes through `usprintf` into a stack buffer and then `limeUC` for
 * the BOM, which is three of the four functions in the UTF-16 chain used in one
 * line.
 *
 * ### FrameCount is frame-rate scaled, unlike BGSceneFrame
 *
 *      FrameCount += 1.0f / limeFPSScaleFactor
 *
 * -- the same `limeFPSScaleFactor` division `MaintainFESlide` uses, and the
 * opposite of `AnimateBG`, which adds a flat 1.0f. Two frame counters in the
 * same binary with different frame-rate behaviour; a port has to keep them
 * apart.
 *
 * ### On a successful sync
 *
 *      enableHeartbeat(10)
 *      CurrentTask = 6
 *      free the loading texture if it is still there
 *      if (Settings[2]) limePlayTune(LevelMusic[*LevelSelect],
 *                                    MusicVol[Settings[2]], 1)
 *
 * `_LevelMusic` is indexed by the selected stage, so each arena has its own
 * tune, and `Settings[2]` is the music volume index -- the same slot
 * `PlayFatalityVoice` reads. The volume is converted from the float table to an
 * int at the call.
 *
 * `FrameCount` still advances on the frame the sync succeeds.
 */
void Task_MultiplayerSync(void)
{
    char buf[0x100];
    const char *msg;
    long f;

    limeSet2DDrawing();
    limeEnableAlphaBlending_Basic();
    drawLoadingBackground();

    f = (long)FrameCount;

    if (*startTime != 0.0f)
        msg = (f % 128 > 64) ? GameTextNoHeader(0x3b7) : UC(" ");
    else
        msg = (f % 128 > 64) ? UC(" ") : GameTextNoHeader(0x0e);

    usprintf(buf, UC("%s"), msg);

    limeDrawFONT(GameFontP, limeUC(buf),
                 (float)(limeScreenWidth / 2),
                 (float)(limeScreenHeight / 2),
                 1, FE_WidthScale, fontcol);

    *readyToSync = 1;

    if (syncGame((long)FrameCount) == 1) {
        enableHeartbeat(10);
        CurrentTask = 6;

        if (LoadingTexture != 0)
            limeDeleteTexture(LoadingTexture);

        if (Settings[2] != 0)
            limePlayTune(LevelMusic[*LevelSelectPtr],
                         (long)MusicVol[Settings[2]], 1);
    }

    FrameCount += 1.0f / *limeFPSScaleFactorP;
}


/* ### The sprite-animation frame record
 *
 * `DrawAnimAsSprite` indexes an animation table, takes the frame number out of
 * it, and reads a record at a **64-byte stride**. Eleven of its words are used:
 *
 *      [0]  x extent          [6]  texture index into the caller's array
 *      [1]  y extent          [7]  texture width   (the UV divisor)
 *      [2]  x extent 2        [8]  texture height  (the UV divisor)
 *      [3]  y extent 2        [9]  x anchor
 *      [4]  x origin          [10] y anchor
 *      [5]  y origin
 *
 * The names are what the arithmetic makes them; nothing in the symbol table
 * describes this record.
 */
#define ANIMSPRITE_STRIDE  64

long __modsi3(long a, long b);


/* ---------------------------------------------------------- DrawAnimAsSprite
 *
 * armv7 0x0001c8bc, 392 bytes.  **Complete**, and unusually opaque per byte --
 * fourteen arguments, four of which are only pinned down by what the arithmetic
 * does with them.
 *
 * ### Picking the frame
 *
 *      wrap != 0:   idx = first + ((last - first + 1) % abs(modulus))
 *      wrap == 0:   idx = first + min(last - first, modulus)
 *
 * The absolute value is `eor`/`sub` on the sign bit, and the modulo is a real
 * `__modsi3` call rather than a reciprocal multiply -- so the divisor is not a
 * constant here. **The two arms differ in kind, not just in clamping**: one
 * wraps and one saturates, and the caller's flag chooses between a looping
 * animation and one that holds its last frame.
 *
 * The frame number is `table[idx + 1]`, not `table[idx]`.
 *
 * ### The UVs are ratios against the record's own texture size
 *
 *      u0 =  record[0] / record[7]        v0 =  record[1] / record[8]
 *      u1 = -record[2] / record[7]        v1 =  record[3] / record[8]
 *
 * with **the mirrored path swapping which extent feeds u0 and u1**, and only
 * that. Everything else -- position, colour, the vertical pair -- is identical
 * between the two arms, so the whole mirror is one exchange of two divisions.
 *
 * The negation on `u1` is a `vneg.f32` on the unmirrored path only.
 *
 * ### The position
 *
 * The draw origin is built from the record's origin minus its anchor, with two
 * of the caller's scalars subtracted in:
 *
 *      x = record[4] - (record[9]  - arg4)
 *      y = record[5] - (record[10] - arg5)
 *
 * so the caller supplies a per-call offset that cancels part of the record's
 * own anchoring. Both are then scaled by the float in r2 and added to the
 * caller's x and y.
 *
 * Returns the frame index it used.
 */
long DrawAnimAsSprite(long x, long y, float scale, long ax,
                      long ay, long textures,
                      const char *frames, const long *table,
                      long mirror, long modulus,
                      long first, long last, long wrap,
                      const float *colour)
{
    long n = last - first;
    long idx;
    const long *r;
    float ox, oy, tw, th;
    void *tex;

    if (wrap != 0) {
        long m = (modulus ^ (modulus >> 31)) - (modulus >> 31);   /* abs */
        /* abs(counter) % frames: r0 = abs(modulus), r1 = n + 1 at
         * 0x1c8e6-0x1c8f6. The two were swapped, which gave (n + 1) %
         * counter -- an index past the end of the table once the counter
         * passed the frame count. */
        idx = first + __modsi3(m, n + 1);
    } else {
        idx = first + ((n >= modulus) ? modulus : n);
    }

    r = (const long *)(frames + table[idx + 1] * ANIMSPRITE_STRIDE);

    ox = (float)(r[4] - (r[9]  - ax));
    oy = (float)(r[5] - (r[10] - ay));
    tw = (float)r[7];
    th = (float)r[8];
    /* The sixth argument IS the texture array (sp+0x54, loaded at 0x1c900
     * and indexed by record[6] at 0x1c956). This read `frames[record[6]]`,
     * a word of the sprite definition, and handed it to limeDrawSprite as a
     * texture -- the crash in every screen that draws the spotlights. */
    tex = ((void *const *)(uintptr_t)textures)[r[6]];

    /* record[0..1] is the cell's corner in the atlas and record[2..3] its
     * size; limeDrawSprite takes a corner and an EXTENT in UV. Read from
     * 0x1ca00 (plain) and 0x1c97c (mirrored): the mirrored cell starts at its
     * right edge and runs back, -record[2]. This used to draw record[0..1]
     * (8 x 8) as the size, which is what made the spotlights flicker. */
    limeDrawSprite((TEXTURE *)tex,
                   (float)x + ox * scale,
                   (float)y + oy * scale,
                   (float)r[2] * scale,
                   (float)r[3] * scale,
                   mirror ? (float)(r[0] + r[2]) / tw : (float)r[0] / tw,
                   (float)r[1] / th,
                   mirror ? -(float)r[2] / tw : (float)r[2] / tw,
                   (float)r[3] / th,
                   colour);

    return idx;
}


/* `_Level_Info` -- 0x0014e8d4, **244 bytes** a level. The compiler builds that
 * stride as `(n*64 - n*4 + n) << 2`, which is 61 << 2.
 *
 * Eight of its fields are established here, and they come in **two sets of
 * four** -- one per value of `_CurrentScene`:
 *
 *      scene 0            other scenes        what it is
 *      +0x24              +0x58               left player limit   ("lx2")
 *      +0x28              +0x5c               right player limit  ("rx2")
 *      +0x2c              +0x60               left camera margin
 *      +0x30              +0x64               right camera margin
 *
 * All eight are floats, and the debug sliders below name them. */
#define LEVELINFO_STRIDE  244

/* `Level_Info` is declared above as char[]; the eight fields here are floats
 * inside it. `G` is declared above as GAMESTATE *. */
extern long   CurrentScene;             /* 0x0014e29c */
extern long   blast_state;              /* 0x0014df94 */
extern float  CamLeftLimit;             /* 0x001f44a8 */
extern float  CamRightLimit;            /* 0x001f44ac */
extern long  *UpperLowerTxt;            /* 0x0015107c */
extern float  ShadowHeightFromGround;   /* 0x00171364 */

void ClearDebugWindow(int index);   /* lime.h names the argument; this file
                                     * had it as void until RenderLevelBG
                                     * called it with 1 */
void LIME_printf(int window, const char *fmt, ...);
void LIME_Slider(int window, float *value, const char *label, float lo, float hi, long step, long e);
void LightPlayers(void);


/* ------------------------------------------------------- MaintainLevelScenes
 *
 * armv7 0x0001d844, 832 bytes.  **Complete.**
 *
 * Applies the current level's camera and player limits, then puts a row of
 * debug sliders on screen for them.
 *
 * ### Every arena has two halves, and `_CurrentScene` picks one
 *
 * The limits are read from **two separate sets of four fields** in
 * `_Level_Info`, chosen by whether `_CurrentScene` is zero. The debug labels
 * for the second set are literally the first set's with a `2` appended --
 * `"lx2"`, `"rx2"`, `"left cam margin2"`, `"right cam margin2"`.
 *
 * And the function prints `_UpperLowerTxt[CurrentScene]`, which names the two
 * states outright. **So a stage is not one space: it is an upper and a lower,
 * with independent camera bounds**, and `_CurrentScene` is which one the fight
 * is in. That is the Pit, the Subway and the Bell Tower -- the arenas with a
 * drop -- and it is why a port that treats a stage as a single camera volume
 * will get the limits wrong on exactly those.
 *
 * ### How the limits are derived
 *
 *      RoundParam[0] = (long)Level_Info[lvl].leftPlayerLimit
 *      RoundParam[1] = (long)Level_Info[lvl].rightPlayerLimit
 *      CamLeftLimit  =  (float)RoundParam[0] + leftCamMargin
 *      CamRightLimit =  (float)RoundParam[1] - rightCamMargin
 *      G[0xb0]       = RoundParam[0]
 *      G[0xb4]       = RoundParam[1] - 399
 *
 * The player limits are **truncated to int and then widened back to float**
 * before the margins are added, so the camera bound is quantised to whole units
 * even though the source field is a float. That rounding is visible only at the
 * edges of a stage and is easy to lose.
 *
 * The **399** is `0x18c + 3` -- written as two subtractions, not one constant.
 *
 * ### `blast_state` gates the whole thing
 *
 *      1 or 2   reset: CurrentScene = 0, RoundParam[9] = 0,
 *               ShadowOffset = 0, and fall through
 *      3        blast_state++ and fall through
 *      other    straight to the limits
 *
 * So the "blast" -- whatever knocks a fighter between the two halves -- resets
 * the scene and the shadow origin on its way through, and state 3 self-advances
 * every frame it is in.
 *
 * ### The sliders shipped
 *
 * Five `LIME_Slider` calls with their ranges, all live in the retail binary:
 *
 *      left playerlimit           -1280 .. 1280, step 6
 *      right playerlimit          -1024 .. 1280
 *      left cam margin             -300 .. 300
 *      right cam margin            -300 .. 300
 *      Shadow HeightFrom Ground      -4 .. 300
 *
 * The last one is `_ShadowHeightFromGround`, the same global
 * `RenderAnimatedCharacter` adds to the flattened shadow's height -- so the
 * developers tuned it live, and its slider range says they expected it small
 * and possibly negative.
 *
 * Ends with `LightPlayers()`.
 */
void MaintainLevelScenes(void)
{
    long lvl;
    float *info;
    long *rp;
    long base;

    if (blast_state == 1 || blast_state == 2) {
        CurrentScene = 0;
        ((long *)RoundParam)[0x24 / 4] = 0;
        ShadowOffset = 0.0f;    /* an integer zero store; same bit pattern */
    } else if (blast_state == 3) {
        blast_state++;
    }

    lvl  = *LevelSelectPtr;
    info = (float *)&Level_Info[lvl * LEVELINFO_STRIDE];
    rp   = (long *)RoundParam;
    base = (CurrentScene == 0) ? 0x24 : 0x58;

    rp[0] = (long)info[base / 4];
    rp[1] = (long)info[(base + 4) / 4];

    CamLeftLimit  = (float)rp[0] + info[(base + 8) / 4];
    CamRightLimit = (float)rp[1] - info[(base + 12) / 4];

    ((long *)G)[0xb0 / 4] = rp[0];
    ((long *)G)[0xb4 / 4] = rp[1] - 0x18c - 3;   /* 399, as two subtractions */

    ClearDebugWindow(0);                /* window 0 -- r0 is 0 at the call */
    LIME_printf(0, "");
    LIME_printf(0, "", UpperLowerTxt[CurrentScene]);
    LIME_printf(0, "");

    if (CurrentScene == 0) {
        LIME_Slider(0, &info[0x24 / 4], "left playerlimit",
                    -1280.0f, 1280.0f, 6, 0);
        LIME_Slider(0, &info[0x28 / 4], "right playerlimit",
                    -1024.0f, 1280.0f, 6, 0);
        LIME_Slider(0, &info[0x2c / 4], "left cam margin",
                    -300.0f, 300.0f, 6, 0);
        LIME_Slider(0, &info[0x30 / 4], "right cam margin",
                    -300.0f, 300.0f, 6, 0);
    } else {
        LIME_Slider(0, &info[0x58 / 4], "lx2",  -1024.0f, 1280.0f, 6, 0);
        LIME_Slider(0, &info[0x5c / 4], "rx2",  -1024.0f, 1280.0f, 6, 0);
        LIME_Slider(0, &info[0x60 / 4], "left cam margin2",
                    -300.0f, 300.0f, 6, 0);
        LIME_Slider(0, &info[0x64 / 4], "right cam margin2",
                    -300.0f, 300.0f, 6, 0);
    }

    LIME_Slider(0, &ShadowHeightFromGround, "Shadow HeightFrom Ground",
                -4.0f, 300.0f, 6, 0);

    LightPlayers();
}


extern float SceneGroundOffset;         /* 0x0014df8c */
extern long  InitGroundOffset;          /* 0x0014df90 */
extern long  blast_player_height;       /* 0x0014df98 */
extern long  DoneSmashEffect;           /* 0x0010ded4 */
extern void *SmashThruScene;            /* 0x001aba74 */
extern float LastEye[3];                /* 0x001f4454 */
extern float LastAt[3];                 /* 0x001f4448 */
extern long  NewCam;                    /* 0x0014e1bc */
extern long  DoIntroFlag;               /* 0x0014e1c0 */
extern float GameTime;                  /* 0x0014fa58 */
extern float  m[];                        /* pointer slot */
extern float *CameraLookAt;             /* pointer slot -> 0x0014fa80 */

long get_tsound(long id);

void limeMatrixCopy(const float *src, float *dst);
void LIME_PlayFBXAtPos(float *m, long a, void *scene, long b);
void LIMEDS_SetCameraOrientation(float ex, float ey, float ez,
                                 float tx, float ty, float tz,
                                 float ux, float uy, float uz);


/* ------------------------------------------------------------ SetToUseCamera
 *
 * armv7 0x00025d50, 900 bytes.  **Complete.**
 *
 * Places the camera. Takes an eye position and produces the eye, target and up
 * vectors for `LIMEDS_SetCameraOrientation` -- with the up vector a static
 * `(0, 0, 1)`, the same Z-up `HUDANIM_Render` uses.
 *
 * ### The camera lags by exactly one seventh
 *
 *      eye = (eye + LastEye * 6) / 7
 *      at  = (at  + LastAt  * 6) / 7
 *
 * Each frame it moves **one seventh of the way** to where it was asked to go.
 * That is the whole camera damping, and 6 and 7 are `vmov.f32` immediates, not
 * data -- there is no tuning global for it.
 *
 * It is not always on. The smoothing runs only when **all five** of these hold:
 *
 *      CurrentTask == 6            in the fight
 *      GamePaused == 0
 *      DoIntro == 0
 *      GameTime < 98.0
 *      blast_state is 0 or 4
 *
 * so the camera snaps rather than glides during the intro, while paused, in the
 * last stretch of the round, and through a stage transition. A port that damps
 * unconditionally gets a camera that drifts during the intro and refuses to cut
 * on a blast.
 *
 * `_LastEye` and `_LastAt` are written on **every** path, smoothed or not, so
 * the history is correct the moment smoothing turns back on.
 *
 * ### Two camera modes
 *
 *      NewCam == 0   at  = arg + (0, 0, SceneGroundOffset)
 *                    eye = at with Y reduced by exactly 1.0
 *
 *      NewCam != 0   at  = CameraLookAt + (0, 0, SceneGroundOffset)
 *                    eye = arg         + (0, 0, SceneGroundOffset)
 *
 * The old mode ignores `_CameraLookAt` entirely and pins the eye one unit
 * behind the target on Y; the new one takes both from globals. Both add the
 * same ground offset to Z only.
 *
 * ### SceneGroundOffset, and the smash
 *
 *      blast_state == 0    if RoundParam[0x30] != 0,
 *                          offset = -InitGroundOffset / WorldScaleAdjust
 *      blast_state 1 or 2  offset = (blast_player_height - player Y)
 *                                   / WorldScaleAdjust
 *      CurrentTask == 3    offset = 0, unconditionally
 *
 * The player Y is an `int16` read out of `_GameObjects` at `blast_state * 16 -
 * 0x0a`, so state 1 reads player one and state 2 reads player two -- the state
 * doubles as the player index.
 *
 * **The smash effect fires from inside the camera code.** Once the offset
 * reaches -1.5 or above and `_DoneSmashEffect` is clear, it copies the falling
 * player's matrix, zeroes its +0x38, plays `_SmashThruScene` through
 * `LIME_PlayFBXAtPos`, plays `get_tsound(0)` at the usual
 * `MusicVol[Settings[3]] / 100`, and sets the done flag. So the sound and the
 * particle burst for falling through a floor are triggered by the camera
 * noticing how far down the fighter is, not by the collision.
 *
 * Player two's matrix is at `Players + 0xb20 + 0x18` and player one's at
 * `Players + 0x548` -- 0xb20 being 0x548 + 0x5d8, which is neither the player
 * stride nor a round number. Transcribed as the two literals they are.
 */
void SetToUseCamera(const float *arg)
{
    float up[3];
    float eye[3], at[3];

    up[0] = 0.0f;                       /* C.217 */
    up[1] = 0.0f;
    up[2] = 1.0f;

    if (GameObjects != 0) {
        long st = blast_state;

        if (st == 0) {
            if (((const signed char *)RoundParam)[0x30] != 0)
                SceneGroundOffset = (float)(-InitGroundOffset)
                                    / WorldScaleAdjust;
        } else if (st == 1 || st == 2) {
            const short *o = (const short *)GameObjects;
            long h = blast_player_height - o[st * 8 - 5];

            SceneGroundOffset = (float)h / WorldScaleAdjust;

            if (SceneGroundOffset >= -1.5f && DoneSmashEffect == 0) {
                const char *pm = (st == 2) ? (Players + 0xb20 + 0x18)
                                           : (Players + 0x548);
                limeMatrixCopy((const float *)pm, m);
                m[0x38 / 4] = 0.0f;
                LIME_PlayFBXAtPos(m, 0, SmashThruScene, 1);

                if (Settings[3] != 0) {
                    long s = get_tsound(0);
                    if (s != -1)
                        limePlaySound(s, MusicVol[Settings[3]] / 100.0f,
                                      1.0f, 0);
                }
                DoneSmashEffect = 1;
            }
        }
    }

    if (CurrentTask == 3)
        SceneGroundOffset = 0.0f;

    if (NewCam == 0) {
        at[0]  = arg[0];
        at[1]  = arg[1];
        at[2]  = arg[2] + SceneGroundOffset;
        eye[0] = arg[0];
        eye[1] = arg[1] - 1.0f;         /* one unit behind on Y */
        eye[2] = at[2];
    } else {
        at[0]  = CameraLookAt[0];
        at[1]  = CameraLookAt[1];
        at[2]  = CameraLookAt[2] + SceneGroundOffset;
        eye[0] = arg[0];
        eye[1] = arg[1];
        eye[2] = arg[2] + SceneGroundOffset;

        if (CurrentTask == 6 && GamePaused == 0 && DoIntroFlag == 0
            && GameTime < 98.0f
            && (blast_state == 4 || blast_state == 0)) {
            at[0]  = (at[0]  + LastAt[0]  * 6.0f) / 7.0f;
            at[1]  = (at[1]  + LastAt[1]  * 6.0f) / 7.0f;
            at[2]  = (at[2]  + LastAt[2]  * 6.0f) / 7.0f;
            eye[0] = (eye[0] + LastEye[0] * 6.0f) / 7.0f;
            eye[1] = (eye[1] + LastEye[1] * 6.0f) / 7.0f;
            eye[2] = (eye[2] + LastEye[2] * 6.0f) / 7.0f;
        }
    }

    LastEye[0] = eye[0];  LastEye[1] = eye[1];  LastEye[2] = eye[2];
    LastAt[0]  = at[0];   LastAt[1]  = at[1];   LastAt[2]  = at[2];

    LIMEDS_SetCameraOrientation(eye[0], eye[1], eye[2],
                                at[0],  at[1],  at[2],
                                up[0],  up[1],  up[2]);
}


extern float StaticMeshAmbient[3];      /* 0x002bfe74, defined in lime_globals.c */
extern void  *WhiteTexture;             /* 0x001ab998 */

float fabsf(float x);

/* Fifteen slots, not two: this loop walks `_Players` fifteen times at the
 * 0x5f0 stride. The compiler spells that stride as `(i*128 - i*32 - i) << 4`,
 * which is 95 << 4 = 1520 = 0x5f0 -- a third independent confirmation. */
#define PLAYER_SLOTS  15


/* ------------------------------------------------------------- LightPlayers
 *
 * armv7 0x0001bfe4, 1000 bytes.  **Complete.**
 *
 * Sets each player slot's light position and brightness, and publishes the
 * ambient for static meshes. Called at the end of `MaintainLevelScenes`.
 *
 * ### The light position comes from the level, per scene
 *
 *      CurrentScene == 0   Level_Info[lvl] + 0x00, 0x04, 0x08
 *      otherwise           Level_Info[lvl] + 0x34, 0x38, 0x3c
 *
 * -- a **third** pair of per-scene fields in that table, on top of the two
 * camera sets `MaintainLevelScenes` established. So the upper and lower halves
 * of an arena have independent lighting as well as independent camera bounds.
 *
 * The three components are copied into the player at +0x5d8, +0x5dc and +0x5e0,
 * and the same three multiplied by **255.0** become `_StaticMeshAmbient`.
 *
 * ### Four levels get distance-based brightness, and no two the same way
 *
 * Brightness starts at 1.0 in `p[0x5d4]` and four stages override it from the
 * fighter's Z (`p[8]`), each with its own reference height, falloff and step
 * count. All four floor at **0.7** and clamp at 1.0:
 *
 *      level 1   b = 1 + |z - 183/scale| / -10       one sample
 *      level 8   b = 1 + |z - 120/scale| / -11       one sample
 *      level 5   four samples from -630/scale, stepping 480/scale,
 *                b = 1 + |z - h| * -0.5, taking the BRIGHTEST
 *      level 4   scene 1: five samples from -550/scale, stepping 330/scale,
 *                         b = 1 + |z - h| / -3.5, brightest wins
 *                scene 0: falls into level 5's four-sample form
 *
 * The multi-sample cases are **lamps in a row**: each iteration measures the
 * distance to one light and keeps the largest brightness, which is why the
 * comparison is `if (candidate > current)` and not an accumulation. Level 4's
 * upper and lower halves have different numbers of lamps -- five and four.
 *
 * All the falloff arithmetic is done in **double** and narrowed only when
 * stored, including the `-0.5`, `-3.5`, `-10` and `-11` which are `vmov.f64`
 * immediates rather than pool constants.
 *
 * ### Per-slot texture selection
 *
 * Four flags on the player pick which texture goes into `p[0x528]`:
 *
 *      always            +0x530 if non-null, else anim[0x14]; p[0x52c] = anim[0x28]
 *      +0x538 set        anim[0x1c], and all four corners set to 1.0
 *      +0x53c set        `_WhiteTexture`, corners to 1.0
 *      +0x534 set        anim[0x18], corners to 1.0
 *
 * They are tested in that order and each can overwrite the last, so the final
 * texture is whichever of the four flags is set **latest in the sequence**, not
 * the first one found.
 *
 * ### One slot is skipped by identity
 *
 * When `CurrentScene` is non-zero and the slot is index 0, it compares a signed
 * byte at `GameObjects + 0x0c` against the same byte at `GameObjects + 0x1c`;
 * equal means the two fighters are the same object and the lighting for that
 * slot is derived from `Players[0].anim[8]` instead. A mirror-match guard, in
 * the lighting.
 */
void LightPlayers(void)
{
    long lvl = *LevelSelectPtr;
    long scene = CurrentScene;
    float ref1 = 183.0f / WorldScaleAdjust;
    float ref8 = 120.0f / WorldScaleAdjust;
    char *p = Players;
    long i;

    for (i = 0; i < PLAYER_SLOTS; i++, p += 0x5f0) {
        float *bright = (float *)(p + 0x5d4);
        float *lp     = (float *)(p + 0x5d8);
        const float *src;
        float z = ((const float *)p)[2];
        long k;

        *bright = 1.0f;

        if (lvl == 1) {
            double b = 1.0 + (double)fabsf(z - ref1) / -10.0;
            *bright = 0.7f;
            if (b > 0.7)
                *bright = (b > 1.0) ? 1.0f : (float)b;
        } else if (lvl == 8) {
            double b = 1.0 + (double)fabsf(z - ref8) / -11.0;
            *bright = 0.7f;
            if (b > 0.7)
                *bright = (b > 1.0) ? 1.0f : (float)b;
        } else if (lvl == 4 && scene == 1) {
            float h = -550.0f / WorldScaleAdjust;
            *bright = 0.7f;
            for (k = 0; k < 5; k++, h += 330.0f / WorldScaleAdjust) {
                double b = 1.0 + (double)fabsf(z - h) / -3.5;
                if ((double)*bright < b)
                    *bright = (float)b;
            }
            if (*bright > 1.0f)
                *bright = 1.0f;
        } else if (lvl == 5 || (lvl == 4 && scene == 0)) {
            float h = -630.0f / WorldScaleAdjust;
            *bright = 0.7f;
            for (k = 0; k < 4; k++, h += 480.0f / WorldScaleAdjust) {
                double b = 1.0 + (double)fabsf(z - h) * -0.5;
                if ((double)*bright < b)
                    *bright = (float)b;
            }
            if (*bright > 1.0f)
                *bright = 1.0f;
        }

        src = (const float *)&Level_Info[lvl * LEVELINFO_STRIDE
                                         + (scene != 0 ? 0x34 : 0x00)];
        lp[0] = src[0];
        lp[1] = src[1];
        lp[2] = src[2];

        StaticMeshAmbient[0] = lp[0] * 255.0f;
        StaticMeshAmbient[1] = lp[1] * 255.0f;
        StaticMeshAmbient[2] = lp[2] * 255.0f;

        /* the four texture flags, in order, each able to override the last */
        {
            long *pw = (long *)p;
            const long *anim = (const long *)(uintptr_t)
                               (unsigned long)pw[1];

            /* 0x1c0d8-0x1c100 and 0x1c2d8: with a character loaded the
             * texture is ALWAYS written -- the alternate costume at +0x530
             * when there is one, else anim[0x14], the character's own sheet.
             * Read as "only when +0x530 is set", +0x528 kept whatever the
             * intro left (nothing, for player one when the intro never
             * reached IntroCamCount 2), and LIME_RenderMeshSingleIndexed
             * drew the fighter with no texture: solid black. */
            if (anim != 0) {
                pw[0x528 / 4] = (pw[0x530 / 4] != 0) ? pw[0x530 / 4]
                                                     : anim[0x14 / 4];
                pw[0x52c / 4] = anim[0x28 / 4];
            }
            if (pw[0x538 / 4] != 0) {
                *bright = 1.0f;
                if (anim != 0)
                    pw[0x528 / 4] = anim[0x1c / 4];
            }
            if (pw[0x53c / 4] != 0) {
                *bright = 1.0f;
                if (anim != 0)
                    pw[0x528 / 4] = (long)(uintptr_t)WhiteTexture;
            }
            if (pw[0x534 / 4] != 0) {
                *bright = 1.0f;
                if (anim != 0)
                    pw[0x528 / 4] = anim[0x18 / 4];
            }
        }
    }
}


/* `_ButtonsPos` 0x001f4144 and `_ButtonsPosP2` 0x001f41bc, **20 bytes** an
 * entry, six entries each:
 *
 *      +0x00  long  x
 *      +0x04  long  y
 *      +0x08, +0x0c  not read here
 *      +0x10  long  button index, or -1 for an unused slot
 */
#define BUTTONPOS_STRIDE  20
#define BUTTON_SLOTS      6
#define TOUCH_SLOTS_RC    10

extern long ButtonsPos[];               /* 0x001f4144 */
extern long ButtonsPosP2[];             /* 0x001f41bc */
extern long ButtonStates[7];            /* 0x0014fef4 */
extern long LastButtonStates[7];        /* 0x0014fed8 */
extern long ButtonStatesP2[7];          /* 0x0014ff2c */
extern long LastButtonStatesP2[7];      /* 0x0014ff10 */
extern long JoystickStateP2;            /* 0x0014fecc */

/* Port, not in the binary: player one's keys, written by the runtime every
 * tick and ORed in at the end of ReadControls' player-one scan. The dial is
 * 0 (no key) or 1..8 as CheckLeftDial + 1 gives it: 1 up, then clockwise. */
int PortKeyButtons[7];
int PortKeyDial;
/* Port, not in the binary: player two's keys in GameMode 6 (two players on
 * one device, the iPad's DUELO), applied as the P2 touches are. */
int PortKeyButtonsP2[7];
int PortKeyDialP2;


/* -------------------------------------------------------------- ReadControls
 *
 * armv7 0x000262f0, 740 bytes.  **Complete.**
 *
 * Turns the raw touch slots into dial and button state, once per frame, for one
 * player or two.
 *
 *      P2Controls = 0
 *      if (GameMode == 6) P2Controls = GameMode - 5      <- 1, written as 6-5
 *
 * so two-player controls are on exactly in mode 6, and the 1 is computed from
 * the mode rather than assigned.
 *
 * ### Each touch claims the NEAREST button, not every button it overlaps
 *
 * For each of the ten touch slots it measures the distance to all six button
 * positions, keeps the closest one inside a radius, and sets that button's
 * state. A slot whose X is -1 is skipped, and a button whose index is -1 is
 * skipped -- so an unused layout slot costs nothing.
 *
 * The search starts from **999.0f**, which is the "no button yet" sentinel; a
 * touch further than that from everything would be missed, and nothing on a
 * 480-wide screen can be.
 *
 * Because it is nearest-wins rather than any-overlap, **two adjacent buttons
 * can never both fire from one finger** -- but two fingers can each claim the
 * same button, and nothing here prevents that.
 *
 * ### The two players get different hit radii, and one is unscaled
 *
 *      player 1, single-player   (float)(long)FE_W(40.0f)   truncated to int
 *      player 1, two-player      FE_W(40.0f)                not truncated
 *      player 2                  52.0f                      no scaler at all
 *
 * Three behaviours from one function. The truncation is a real `vcvt.s32.f32`
 * followed by `vcvt.f32.s32` on the P1 path and it happens **only** when
 * `P2Controls` is clear, so turning on two-player controls silently changes
 * player one's button radius by up to a pixel.
 *
 * The flat 52 for player two is the fourth place in this tree where one side of
 * a pair is scaled and the other is not -- see issue #22 -- and the second in
 * the input path, after `CheckLeftDial`'s unscaled P2 dial radius. On the
 * original 480x320 screen `FE_W(40)` is 40 and 52 is simply a bigger target;
 * anywhere else they diverge.
 *
 * ### The state rings
 *
 * Before the scan, each of the seven button slots is copied into
 * `LastButtonStates` and then cleared, so a button not touched this frame is
 * released rather than sticky, and the previous frame is preserved for edge
 * detection. Slot 0 is done outside the loop, which then runs from 4 to 0x1c --
 * the same peeled first iteration `InitGameEvents` and `InitKodeScreen` use.
 *
 * `JoystickState` is `CheckLeftDial(player) + 1`, so the eight-way result 0..7
 * becomes 1..8 -- exactly the range `GetReal6ButtonJoyBits` tests, with 0 left
 * free for "no touch on the dial".
 */
void ReadControls(void)
{
    long t, b, i;

    P2Controls = 0;
    if (GameMode == 6)
        P2Controls = GameMode - 5;

    JoystickState = CheckLeftDial(0) + 1;

    LastButtonStates[0] = ButtonStates[0];
    ButtonStates[0] = 0;
    for (i = 1; i < 7; i++) {
        LastButtonStates[i] = ButtonStates[i];
        ButtonStates[i] = 0;
    }

    for (t = 0; t < TOUCH_SLOTS_RC; t++) {
        float best = 999.0f;
        long nearest = -1;

        for (b = 0; b < BUTTON_SLOTS; b++) {
            const long *e = &ButtonsPos[b * (BUTTONPOS_STRIDE / 4)];
            float dx, dy, d, radius;

            if (limeTouchScreenX[t] == -1.0f)
                continue;
            if (e[4] == -1)
                continue;

            dx = (float)e[0] - limeTouchScreenX[t];
            dy = limeTouchScreenY[t] - (float)e[1];
            d  = sqrtf(dx * dx + dy * dy);

            radius = (float)FE_W(40.0f);
            if (P2Controls == 0)
                radius = (float)(long)radius;   /* truncated, only here */

            if (d >= radius)
                continue;
            if (best > d) {
                best = d;
                nearest = b;
            }
        }

        if (nearest != -1)
            ButtonStates[ButtonsPos[nearest * (BUTTONPOS_STRIDE / 4) + 4]] = 1;
    }

    /* Port, not in the binary: the keyboard and the gamepad go straight
     * into the state the touches produce, so they work with the on-screen
     * controls hidden and a held key is a held button. See
     * runtime/game_main.c, keyboard_buttons. */
    for (i = 0; i < 7; i++)
        if (PortKeyButtons[i])
            ButtonStates[i] = 1;
    if (PortKeyDial != 0)
        JoystickState = PortKeyDial;

    if (P2Controls == 0)
        return;

    JoystickStateP2 = CheckLeftDial(1) + 1;

    LastButtonStatesP2[0] = ButtonStatesP2[0];
    ButtonStatesP2[0] = 0;
    for (i = 1; i < 7; i++) {
        LastButtonStatesP2[i] = ButtonStatesP2[i];
        ButtonStatesP2[i] = 0;
    }

    for (t = 0; t < TOUCH_SLOTS_RC; t++) {
        float best = 999.0f;
        long nearest = -1;

        for (b = 0; b < BUTTON_SLOTS; b++) {
            const long *e = &ButtonsPosP2[b * (BUTTONPOS_STRIDE / 4)];
            float dx, dy, d;

            if (limeTouchScreenX[t] == -1.0f)
                continue;
            if (e[4] == -1)
                continue;

            dx = (float)e[0] - limeTouchScreenX[t];
            dy = limeTouchScreenY[t] - (float)e[1];
            d  = sqrtf(dx * dx + dy * dy);

            if (d >= 52.0f)             /* a flat 52, never scaled */
                continue;
            if (d < best) {
                best = d;
                nearest = b;
            }
        }

        if (nearest != -1)
            ButtonStatesP2[ButtonsPosP2[nearest * (BUTTONPOS_STRIDE / 4) + 4]]
                = 1;
    }

    /* Port, not in the binary: player two's keys, as player one's above */
    for (i = 0; i < 7; i++)
        if (PortKeyButtonsP2[i])
            ButtonStatesP2[i] = 1;
    if (PortKeyDialP2 != 0)
        JoystickStateP2 = PortKeyDialP2;
}


extern long ButtonsPos4[];              /* 0x0015012c */
extern long ButtonsPos5[];              /* 0x001501a4 */
extern long ButtonsPos6[];              /* 0x0015021c */
extern long JoystickStatePosXv;         /* 0x0014fec0 */
extern long JoystickStatePosYv;         /* 0x0014fec4 */
extern float JSIZE;                     /* 0x0014e1dc = 64.0 */


/* -------------------------------------------------------- DrawControlsPreview
 *
 * armv7 0x0001ddc0, 732 bytes.  **Complete.**
 *
 * Draws the on-screen controls at half size, offset by the caller's origin.
 *
 * ### It rebuilds the live layout from the scheme every frame
 *
 *      Settings[4] == 4  ->  copy ButtonsPos4 into ButtonsPos
 *                  == 5  ->  ButtonsPos5
 *                  == 6  ->  ButtonsPos6
 *
 * all 6 entries x 5 words, and the copy is written as a **transposed** double
 * loop -- outer over the five words, inner over the six entries -- so it walks
 * the destination in 0x14 steps five separate times rather than once in
 * sequence. Same bytes, five passes.
 *
 * ### The dial position is derived, not stored
 *
 *      JoystickStatePosX = (long)JSIZE
 *      JoystickStatePosY = (long)(limeScreenHeight - JSIZE)
 *
 * so the dial sits `JSIZE` in from the left and `JSIZE` up from the bottom --
 * one constant placing it in the corner, and `_JSIZE` is 64.0 in the shipped
 * data. `CheckLeftDial` then hit-tests against this position.
 *
 * ### Opacity comes from Settings[6]
 *
 *      0 -> 0.5      1 -> 0.75      2 -> 1.0
 *
 * Three steps, written as two `it eq` blocks with 0.5 as the fall-through, so
 * any other value also gives 0.5.
 *
 * ### The atlas: pressed is the EVEN column
 *
 *      u0 = ((glyph & 3) * 2 + (pressed ? 0 : 1)) * 0.125
 *      v0 = (glyph / 4) * 0.25
 *      u1 = 0.125      v1 = 0.25
 *
 * Eight columns by four rows, and each button occupies **two adjacent cells** --
 * pressed on the even column, released on the odd one. The `+ 1` is the only
 * difference between the two draw paths, which the compiler emitted as two
 * near-identical copies of the whole call.
 *
 * The glyph id is `+0x0c` of the entry; the button index used for
 * `ButtonStates` is `+0x10`. They are separate fields, so the same artwork can
 * serve two logical buttons.
 *
 * ### Everything is halved
 *
 *      x = FE_X(originX) + (entry.x - entry.size / 2) * 0.5
 *      y = FE_Y(originY) + (entry.y - entry.size / 2) * 0.5
 *      w = h = entry.size * 0.5
 *
 * The `size / 2` centring uses the signed `(n + (n >>> 31)) >> 1` shape, and the
 * outer 0.5 is what makes this a preview rather than the live control.
 */
float PortPreviewCX = -1.0f, PortPreviewCY = -1.0f;   /* port, see below */

void DrawControlsPreview(long originX, long originY)
{
    float colour[4];
    float alpha;
    long i, k;
    long pminx, pminy, pmaxx, pmaxy;
    const long *src;

    colour[0] = colour[1] = colour[2] = colour[3] = 1.0f;   /* C.126 */

    src = (Settings[4] == 4) ? ButtonsPos4
        : (Settings[4] == 5) ? ButtonsPos5
        : (Settings[4] == 6) ? ButtonsPos6 : 0;

    if (src != 0) {
        for (k = 0; k < 5; k++)                 /* transposed on purpose */
            for (i = 0; i < 6; i++)
                ButtonsPos[i * 5 + k] = src[i * 5 + k];
    }

    alpha = (Settings[6] == 1) ? 0.75f
          : (Settings[6] == 2) ? 1.0f : 0.5f;

    JoystickStatePosXv = (long)JSIZE;
    JoystickStatePosYv = (long)((float)limeScreenHeight - JSIZE);

    pminx = pminy = 1 << 30;            /* port: the group's extents */
    pmaxx = pmaxy = -(1 << 30);
    for (i = 0; i < 6; i++) {
        const long *e = &ButtonsPos[i * 5];

        if (e[4] == -1)
            continue;
        if (e[0] - e[2] / 2 < pminx) pminx = e[0] - e[2] / 2;
        if (e[0] + e[2] / 2 > pmaxx) pmaxx = e[0] + e[2] / 2;
        if (e[1] - e[2] / 2 < pminy) pminy = e[1] - e[2] / 2;
        if (e[1] + e[2] / 2 > pmaxy) pmaxy = e[1] + e[2] / 2;
    }

    for (i = 0; i < 6; i++) {
        const long *e = &ButtonsPos[i * 5];
        long idx = e[4];
        long glyph, col;
        float x, y, wh;

        if (idx == -1)
            continue;

        glyph = e[3];
        col   = (glyph & 3) * 2;
        if (ButtonStates[idx] == 0)
            col += 1;                           /* released is the odd cell */

        x  = (float)FE_X((float)originX)
             + (float)(e[0] - ((e[2] + ((unsigned long)e[2] >> 31)) >> 1))
               * 0.5f;

        y  = (float)FE_Y((float)originY)
             + (float)(e[1] - ((e[2] + ((unsigned long)e[2] >> 31)) >> 1))
               * 0.5f;
        wh = (float)e[2] * 0.5f;
        /* Port: on a wide surface (PortPreviewCX set by FE_Task_Button_Config)
         * the half-size buttons are centred in the menu box: the group's
         * extents, centred on the box's centre. Not taken on 3:2 and 4:3. */
        if (PortPreviewCX >= 0.0f) {
            x = PortPreviewCX - (pmaxx - pminx) * 0.25f
              + (float)(e[0] - e[2] / 2 - pminx) * 0.5f;
            y = PortPreviewCY - (pmaxy - pminy) * 0.25f
              + (float)(e[1] - e[2] / 2 - pminy) * 0.5f;
        }

        colour[3] = alpha;

        limeDrawSprite((TEXTURE *)ButtonsTPage, x, y, wh, wh,
                       (float)((double)col * 0.125),
                       (float)((double)(glyph / 4) * 0.25),
                       0.125f, 0.25f, colour);
    }
}


extern long   JustWon;                  /* pointer slot */
extern long   winningStryk;             /* 0x0014dffc */
extern long   feedPosted;               /* pointer slot */
extern long   defeatedBySK;             /* 0x0010deb4 */
extern long   lastWinStreak;            /* 0x0014e1ac */
extern long   points;                   /* 0x0014e1b0 */
extern float  timeInGame;               /* 0x0014e1e0 */
extern float  KontinueTime;             /* pointer slot -> 0x000ff960 -- a FLOAT.
                                         * The store is a raw word from the pool
                                         * (0x419ffdf4), which says nothing on its
                                         * own; the countdown that reads it is what
                                         * types it. */
extern float  exitTimeout;              /* pointer slot -> 0x00182c80 -- also a
                                         * FLOAT: the value stored is 0x44160000,
                                         * which is 600.0f, not the integer 600.
                                         * This header always said 600.0f; the C
                                         * under it stored 600 and was wrong. */
extern long   WaitForOpponent;          /* 0x0010df18 */
extern long   FadeMusicOut;             /* 0x0010dee8 */
/* DestinyNames is the array of C strings itself (108 bytes at 0x00176760,
 * as Blood.c and FrontEnd.c declare it); DestinyNamesLoss is reached through
 * a slot. */
extern const char *DestinyNames[];
extern const char **DestinyNamesLoss;
extern long  *DisplaySurvivalStageP, *SurvivalStageP;   /* slots: &DisplaySurvivalStage, &SurvivalStage */
/* Two words, not two pointers. FrontEnd.c declares both as plain `int` at
 * 0x001008ac and 0x001008bc, and the symbol table agrees -- four bytes each
 * in __DATA,__data. Written as one line with two declarators, this escaped
 * tools/slotcheck.py, whose pattern reads a single name per line. */
extern int  FE_TaskStackPointer;        /* 0x001008ac */
extern int  FE_CurrentTask;             /* 0x001008bc */

extern long Player1Wins;                /* 0x0014e204 */

/* TWO arguments: `getStageName(tier, index)`. Every call site loads r0 with
 * `Destiny` and r1 with `Stage` before the branch, and achievements.c
 * decompiles the callee as `getStageName(int tier, int index)`. It was
 * declared with one argument here, which silently dropped the second at both
 * call sites below; corrected while writing UpdateInGamePauseMenu, which makes
 * the same call a third time. */
const char *getStageName(long tier, long index);
void PushFETask(int task);
void UpdateStats(void);
int  achievementsUnlock(int id);
void sendQuit(void);
void Write_AchievementsData(void);
/* The last argument is a LONG, not a float. The call site stores it with
 * `vstr s14, [sp]` after a `vcvt.s32.f32`, so the word on the stack is an
 * integer that happens to have been computed in a VFP register; the callee
 * loads it with a plain `ldr` and hands it to `numberWithInt:`. Typed float,
 * the clean C converted the integer back to a float and logged nonsense. */
void EASDK_LogEventEnumEnumStringNum(long id, long a, const char *s,
                                     long b, long n);


/* ---------------------------------------------------------------- QuitAsLose
 *
 * armv7 0x000269d0, 692 bytes.  **Complete.**
 *
 * The lose path out of a fight. Clears `JustWon`, logs the arcade case, then
 * does something different for every game mode.
 *
 * ### Three losses to character 25 unlock achievement 15
 *
 *      if (PLAYER2MODEL == 25) {
 *          if (++defeatedBySK > 2) achievementsUnlock(15);
 *      } else {
 *          defeatedBySK = 0;
 *      }
 *
 * The counter is **reset by any loss to anyone else**, so it is three
 * consecutive losses, not three in total. Character 25 is one of the two fixed
 * bosses `PopulateTower` puts at the end of every ladder, and `defeatedBySK`
 * names it: Shao Kahn.
 *
 * ### Per-mode exits
 *
 *      mode 0 (arcade)  winStreak -> lastWinStreak and points, then 0
 *                       KontinueTime = 19.999001f
 *                       PushFETask(0x1d), Player1Wins = 0, UpdateStats()
 *      mode 1 (versus)  PushFETask(0x28), WaitForOpponent = 0,
 *                       Player1Wins = 0, sendQuit(); UpdateStats() only if
 *                       the fade had not already started
 *      mode 2           nothing but the fade
 *      mode 3           PushFETask(0x27), exitTimeout = 600.0f
 *      mode 4 (survival) streak saved as above, PushFETask(0x26),
 *                       DisplaySurvivalStage = SurvivalStage, SurvivalStage = 0,
 *                       Write_SaveData(), exitTimeout = 600.0f
 *      mode 5           FE_TaskStackPointer = 0, FE_CurrentTask = 0,
 *                       Player1Wins = 0, UpdateStats()
 *      mode 6           nothing but the fade
 *
 * Every one of them sets `FE_FadeAdd` to **-0.03333333507180214**, the same
 * literal the whole front end uses to start a fade out.
 *
 * **`KontinueTime` gets 19.999001f -- bit for bit the same odd literal
 * `InitKodeScreen` writes into `_KodeTime`.** Two unrelated countdowns sharing
 * one value that is neither 20 nor anything derived; whatever produced it was
 * used twice.
 *
 * Mode 5 is the only one that empties the front-end task stack rather than
 * pushing onto it.
 *
 * ### The arcade logging happens before the switch
 *
 *      EASDK_LogEventEnumEnumString(0x754e, 15, DestinyNamesLoss[Destiny],
 *                                   15, getStageName(Destiny, Stage))
 *      EASDK_LogEventEnumEnumStringNum(0x754f, 15, DestinyNames[Destiny],
 *                                      7, (long)timeInGame)
 *      printf(..., (double)timeInGame)
 *
 * 0x754e and 0x754f are 30030 and 30031. `timeInGame` is truncated to an int
 * for the event and passed as a **double** to the printf on the very next line,
 * so the log and the analytics disagree about its precision.
 *
 * Ends with `Write_AchievementsData()` and, if `Settings[2]` is set,
 * `FadeMusicOut = 1`.
 */
void QuitAsLose(void)
{
    JustWon = 0;

    if (GameMode == 0) {
        EASDK_LogEventEnumEnumString(0x754e, 15,
                DestinyNamesLoss[Destiny],
                15, getStageName(Destiny, Stage));
        EASDK_LogEventEnumEnumStringNum(0x754f, 15,
                DestinyNames[Destiny],
                7, (long)timeInGame);
        printf("%f", (double)timeInGame);
    }

    winningStryk = 0;
    feedPosted  = 0;

    switch (GameMode) {
    case 1:
        PushFETask(0x28);
        WaitForOpponent = 0;
        Player1Wins = 0;
        sendQuit();
        if (*FE_FadeAddP == 0.0f)
            UpdateStats();
        *FE_FadeAddP = -0.033333335f;
        break;

    case 2:
    case 6:
        *FE_FadeAddP = -0.033333335f;
        break;

    case 3:
        *FE_FadeAddP = -0.033333335f;
        PushFETask(0x27);
        exitTimeout = 600.0f;
        break;

    case 4:
        lastWinStreak = winStreak;
        winStreak     = 0;
        points        = lastWinStreak;
        *FE_FadeAddP    = -0.033333335f;
        PushFETask(0x26);
        *DisplaySurvivalStageP = *SurvivalStageP;
        *SurvivalStageP       = 0;
        Write_SaveData();
        exitTimeout = 600.0f;
        break;

    case 5:
        *FE_FadeAddP = -0.033333335f;
        FE_TaskStackPointer = 0;
        FE_CurrentTask      = 0;
        Player1Wins = 0;
        UpdateStats();
        break;

    default:                            /* arcade */
        if (PLAYER2MODEL == 25) {
            defeatedBySK++;
            if (defeatedBySK > 2)
                achievementsUnlock(15);
        } else {
            defeatedBySK = 0;
        }
        lastWinStreak = winStreak;
        winStreak     = 0;
        points        = lastWinStreak;
        *FE_FadeAddP    = -0.033333335f;
        KontinueTime = 19.999001f;     /* the same odd literal InitKodeScreen
                                         * writes into KodeTime */
        PushFETask(0x1d);
        Player1Wins = 0;
        UpdateStats();
        break;
    }

    Write_AchievementsData();
    if (Settings[2] != 0)
        FadeMusicOut = 1;
}


extern float IntroPlayer1PosX;          /* 0x0014f930 */
extern float IntroPlayer1PosZ;          /* 0x0014f934 */
extern float PlayerSize;                /* 0x00150cc4 */

void LIME_GLAPI glTranslatef(float x, float y, float z);
void LIME_GLAPI glRotatef(float a, float x, float y, float z);


/* --------------------------------------------------- RenderIntroCharacterPlayer
 *
 * armv7 0x00021638, 824 bytes.  **Complete.**
 *
 * Draws the two fighters during the intro, each with its attached objects.
 *
 *      glTranslatef(IntroPlayer1PosX, IntroPlayer1PosZ, SceneGroundOffset)
 *      glRotatef(90, 1, 0, 0)
 *      glScalef(PlayerSize, PlayerSize, PlayerSize)
 *
 * `_PlayerSize` is the same 0.01015625 global `runtime/demo.c` uses for the
 * fighter-to-stage ratio, and the 90-degree X rotation is the same one
 * `HUDANIM_Render` applies -- the Z-up world meeting a Y-up mesh.
 *
 * ### Character 24 is drawn mirrored -- for player one
 *
 * (Player two is the reverse: mirrored unless it is 24. See the body.)
 *
 *      if (anim->characterId == 24) {
 *          glScalef(-scale, scale, scale);
 *          glCullFace(GL_FRONT);
 *      } else {
 *          glScalef(scale, scale, scale);
 *          glCullFace(GL_BACK);
 *      }
 *
 * The negation is `eor r0, r1, #0x80000000` and the cull flip goes with it --
 * the **fourth** place in this tree with that exact pairing, after
 * `RenderAMesh`, `RenderPlayer` and `RenderExtras`. Character 24 is one of the
 * two fixed bosses `PopulateTower` appends to every ladder, and it is the only
 * character with a hardcoded mirror in the intro.
 *
 * The per-character scale comes from `PlayerDefs[id] + 0x04`, so it multiplies
 * the global `PlayerSize` already on the matrix.
 *
 * ### Which fighters appear depends on IntroCamCount
 *
 *      player 1 drawn when  IntroCamCount > 1
 *      player 2 drawn when  IntroCamCount > 2 || IntroCamCount == 0
 *
 * so at 0 only player two, at 1 neither, at 2 only player one, and above that
 * both. `AnimateIntroCharacterPlayers1Frame` uses the same global to choose
 * which fighter's animation advances -- the two halves of one intro
 * choreography.
 *
 * ### Player two's translate is divided by the scale
 *
 *      glTranslatef(-IntroPlayer1PosX / PlayerSize,
 *                   -IntroPlayer1PosZ / PlayerSize, ...)
 *
 * because `glScalef(PlayerSize, ...)` is already on the matrix stack, so the
 * offset has to be pre-divided to land in the same units. Both components are
 * negated first: player two is placed by undoing player one's offset and going
 * the other way.
 *
 * Each fighter is followed by the same attachment pass `RenderPlayer` uses --
 * `memcpy(AttachTransforms, MatrixPalette2, 0x1c20)` then
 * `LIME_RenderScene(6, ...)` -- so the attachment is posed against a snapshot
 * taken after the body was drawn.
 *
 * `p[0x534]` is cleared before each draw, and `p[0x528]` is set from
 * `anim[0x14]`, overriding whatever `LightPlayers` chose.
 */
extern float IntroPlayer2PosX, IntroPlayer2PosZ;  /* 0x0014f938, 0x0014f93c */

void RenderIntroCharacterPlayer(void)
{
    long *p0 = (long *)Players;
    long *p1 = (long *)(Players + 0x5f0);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();

    glTranslatef(IntroPlayer1PosX, IntroPlayer1PosZ, SceneGroundOffset);
    glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
    glScalef(PlayerSize, PlayerSize, PlayerSize);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    if (IntroCamCount > 1 && p0[1] != 0) {
        const long *anim = (const long *)(uintptr_t)(unsigned long) p0[1];
        const PLAYERDEF *def = &PlayerDefs[p0[0]];
        float s = def->scale;

        glEnable(GL_CULL_FACE);

        if (anim[2] == 24) {            /* the hardcoded mirror */
            glScalef(-s, s, s);
            glCullFace(GL_FRONT);
        } else {
            glScalef(s, s, s);
            glCullFace(GL_BACK);
        }

        p0[0x534 / 4] = 0;
        p0[0x528 / 4] = anim[0x14 / 4];

        RenderAnimatedCharacter(0, (ANIMATEDCHARACTER *)anim,
                                p0[0x51c / 4], p0[0x520 / 4],
                                ((float *)p0)[0x524 / 4],
                                0.0f, 1.0f,
                                (limeVECTOR3 *)&p0[0x5d8 / 4],
                                (void *)(uintptr_t)(unsigned long)
                                    p0[0x528 / 4], 1);

        memcpy(AttachTransforms, MatrixPalette2, 0x1c20);
        LIME_RenderScene(6,
                         (void *)(uintptr_t)(unsigned long) anim[0x10 / 4],
                         p0[0x51c / 4], p0[0x520 / 4],
                         ((float *)p0)[0x524 / 4], 0, 0, 0,
                         (void *)(uintptr_t)(unsigned long) p0[0x528 / 4],
                         p0[0x52c / 4], AttachTransforms);
    }

    if ((IntroCamCount > 2 || IntroCamCount == 0) && p1[1] != 0) {
        const long *anim = (const long *)(uintptr_t)(unsigned long) p1[1];
        const PLAYERDEF *def = &PlayerDefs[p1[0]];
        float s = def->scale;

        /* pre-divided: PlayerSize is already on the matrix. Two translates,
         * not one: undo player one's offset (0x217ce), then apply player
         * two's own (0x217d2..0x217f8, IntroPlayer2PosX/Z over PlayerSize).
         * With only the first, player two stood at the origin -- out of
         * the shot the intro camera frames for him. */
        glTranslatef(-IntroPlayer1PosX / PlayerSize,
                     -IntroPlayer1PosZ / PlayerSize, 0.0f);
        glTranslatef(IntroPlayer2PosX / PlayerSize,
                     IntroPlayer2PosZ / PlayerSize, 0.0f);

        /* Player two is the MIRROR of player one (0x21822..0x218f4): every
         * character but 24 is drawn with x negated and front faces culled,
         * so the two fighters face each other; 24, already mirrored in its
         * data, is the one drawn straight. glEnable(GL_CULL_FACE) comes
         * first, at 0x21800. */
        glEnable(GL_CULL_FACE);
        if (anim[2] == 24) {
            glCullFace(GL_BACK);
            glScalef(s, s, s);
        } else {
            glCullFace(GL_FRONT);
            glScalef(-s, s, s);
        }

        /* The alternate costume (+0x530) when one is loaded, else the
         * character's own sheet (0x21846..0x2190e). Player one has no such
         * test. */
        p1[0x534 / 4] = 0;
        p1[0x528 / 4] = (p1[0x530 / 4] != 0) ? p1[0x530 / 4] : anim[0x14 / 4];

        RenderAnimatedCharacter(0, (ANIMATEDCHARACTER *)anim,
                                p1[0x51c / 4], p1[0x520 / 4],
                                ((float *)p1)[0x524 / 4],
                                0.0f, 1.0f,
                                (limeVECTOR3 *)&p1[0x5d8 / 4],
                                (void *)(uintptr_t)(unsigned long)
                                    p1[0x528 / 4], 1);

        memcpy(AttachTransforms, MatrixPalette2, 0x1c20);
        LIME_RenderScene(6,
                         (void *)(uintptr_t)(unsigned long) anim[0x10 / 4],
                         p1[0x51c / 4], p1[0x520 / 4],
                         ((float *)p1)[0x524 / 4], 0, 0, 0,
                         (void *)(uintptr_t)(unsigned long) p1[0x528 / 4],
                         p1[0x52c / 4], AttachTransforms);
    }

    glPopMatrix();
}

/* ------------------------------------------------- Task_LoadGeneralData
 *
 * armv7 0x00023910, 788 bytes.  **Complete.**
 *
 * The boot loader for everything that is not a level: language, save data,
 * sound, the three fonts, the translated text, and the first task to run. It
 * is one straight line with two early-out branches parked at the end of the
 * function, so the ORDER below is the game's real start-up order and can be
 * read off directly.
 *
 * ## The language is chosen by whether EA shipped a terms-of-service URL
 *
 * `limeGetLanguage` fills `Language` from the device, and the very next thing
 * the function does is ask for the property `TOS_URL_<lang>`. If that property
 * is missing the code does `strcpy(Language, "EN")` and carries on. So the
 * list of languages the game supports is not a list anywhere in the code --
 * it is whichever `TOS_URL_*` keys exist in the property file. A device set to
 * a language EA never shipped legal text for falls back to English, and it
 * falls back BEFORE anything else reads `Language`, so the fallback also
 * chooses the font tables and the text file.
 *
 * `compareLanguages(1)` then saves the code and reports whether it differs
 * from the previous run. Only the changed path logs to EA's tracking; the
 * unchanged path just prints. Both converge on `InitAllGameData`.
 *
 * ## The game mutes its own music if the player is already playing some
 *
 *      if (limeCheckForUserMusic()) Settings[2] = 0;
 *
 * `Settings[2]` is the music volume index -- `FrontEnd.c` plays MainMenu.mp3
 * at `MusicVol[Settings[2]]`. Index 0 is the muted level. So a player who
 * started their own music before launching gets the game's soundtrack turned
 * off for them, once, at boot, and it is written into the settings rather
 * than applied as a temporary override: it survives into the next launch.
 *
 * ## The three fonts, and the two things done to each after loading
 *
 *      GameFont       MKFONT_0.PNG + MKFONT_1.PNG   mkfont_ipad.ft2  2048^2
 *      NameFont       NAMES_FONT.PNG                namefont.ft2      256^2
 *      CountDownFont  COUNTDOWNFONT.PNG             countdownfont.ft2 128^2
 *
 * Only the game font has a second atlas -- it is the one that has to carry a
 * translated character set. The other two pass NULL and limeCreateFONT skips
 * the second load.
 *
 * Each font then gets `spacing` and `fallbackAdvance` written by hand,
 * OVERWRITING what limeCreateFONT just put there (it sets spacing from its
 * own argument, which is 0 in all three calls, and fallbackAdvance to the
 * constant 8):
 *
 *      GameFont       spacing -4   fallbackAdvance 17
 *      NameFont       spacing -2   fallbackAdvance  6
 *      CountDownFont  spacing  0   fallbackAdvance 10
 *
 * Negative spacing is a kern-in: the game font pulls every character four
 * units back towards the previous one.
 *
 * ## mkunicode.txt REPLACES the game font's code table
 *
 * `limeCreateFONT` builds `codes`/`codesW` from the .ft2 file. This function
 * then loads `mkunicode.txt` and overwrites both, one glyph at a time, from a
 * file that is plain UTF-16LE: the loop starts at byte 2, which skips the BOM,
 * and reads two bytes per glyph. The wide table gets the whole code unit and
 * the byte table gets its low half.
 *
 * That is what makes a translated build possible without touching the .ft2:
 * the atlas keeps its glyph rectangles in file order and mkunicode.txt says
 * which Unicode character each rectangle is. Swap the text file and the same
 * atlas serves a different language.
 *
 * The name font goes the other way -- it has no unicode file, so its wide
 * table is built by widening its byte table, and the byte table is a constant
 * compiled into the binary: the digits, A-Z, a-z, and then
 * `-%:;@` a pound sign, `.,!?'&[]+/\"*()#<>~`.
 *
 * 87 characters and a terminator, memcpy'd as a fixed 0x58 bytes. See
 * docs/GAME-BUGS.md: the length is a literal, not the font's glyph count.
 *
 * ## The float that proves limeCreateFONT's signature
 *
 * The last argument is `0x3ea66666` for the game font and `0x3f800000` for the
 * other two -- 0.325f and 1.0f. Those are float literals, which is what fixed
 * `limeCreateFONT`'s last parameter from `int` to `float` in decomp/lime.
 *
 * ## Where the game goes next
 *
 *      NextTask        = 2
 *      FE_CurrentTask  = 0, or 0x24 when CheckForUnclaimedTreasure() is true
 *      CurrentTask     = 8
 *
 * so a player owed a treasure from a previous run is routed to front-end task
 * 0x24 instead of the main menu, before the menu has drawn once.
 *
 * ## The on-screen joystick is placed from the button size
 *
 *      JSIZE             = ButtonSize
 *      JoystickStatePosX = (int)ButtonSize
 *      JoystickStatePosY = (int)(limeScreenHeight - ButtonSize)
 *
 * One button-size in from the left and one up from the bottom -- the joystick
 * is anchored to the bottom-left corner with a margin equal to its own size,
 * and both coordinates are truncated to whole pixels. `ButtonSize` is a float
 * and the two positions are ints; the conversions are `vcvt.s32.f32`, so they
 * truncate rather than round.
 */

extern GAMEFONT GameFont;               /* 0x001abb98 */
extern GAMEFONT CountDownFont;          /* 0x001dbc50 */

extern float ButtonSize;                /* 0x0014ff48 */

void limeCreateFONT(const char *tex0, const char *tex1, const char *metrics,
                    GAMEFONT *font, int spacing, int height, int width,
                    int wide, float scale);
void limeGetLanguage(char *dst, int len);
const char *limeGetPropertyString(const char *key);
void limeInitSound(void);
int  limeCheckForUserMusic(void);
long limeLoadSound(const char *name);
void InitAllGameData(void);
void Load_Tower(void);
void Load_SettingsData(void);
void Load_PresetButtonData(void);
void Load_AchievementsData(void);
void Load_Stats(void);
void LoadTextData(const char *file);
void CheckAllUnicodeCharsUsed(void);

/* Five fixed arguments -- the callee reads r0..r3 and one stack word. Part of
 * the EA tracking family (LogEvent / LogEventEnumEnum / ...EnumEnumString),
 * all of which are stub territory for the port. The last argument is a
 * STRING: this call site passes 0 for it and says nothing, but
 * FE_Task_Main_Menu passes "OPTIONS", "PLAY" and friends. */
void EASDK_LogEvent(long id, long a, const char *s1, long b, const char *s2);

/* The name font's character set, 87 characters and a NUL. The 0xa3 is a pound
 * sign in Latin-1 -- the only non-ASCII code in the table, and the reason the
 * set is a byte array and not a plain C identifier-safe string. */
static const char NameFontCodes[] =
    "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "-%:;@\xa3.,!?'&[]+/\\\"*()#<>~";

void Task_LoadGeneralData(void)
{
    char langbuf[30];                   /* sp+0x14 */
    char tosbuf[30];                    /* sp+0x32 */
    const uint8_t *unicode;
    int i;

    limeDeferredDeviceSideways = 1;
    limeDeviceSideways = 1;

    Task_LoadingScreen();
    if (LoadingTexture)
        limeDeleteTexture(LoadingTexture);

    limeGetLanguage(Language, 10);
    sprintf(tosbuf, "TOS_URL_%s", Language);
    if (limeGetPropertyString(tosbuf) == 0)
        strcpy(Language, "EN");         /* no legal text shipped: fall back */

    if (compareLanguages(1)) {
        printf("####################\n"
               "LANGUAGE CHANGED TO:%s\n"
               "####################", Language);
        EASDK_LogEvent(0xc35c, 15, Language, 0, 0);
    } else {
        puts("####################\n"
             "LANGUAGE NOT CHANGED\n"
             "####################");
    }

    InitAllGameData();
    Load_Tower();
    Load_SaveData();
    Load_SettingsData();
    Load_PresetButtonData();
    Load_AchievementsData();
    Load_Stats();

    limeInitSound();
    if (limeCheckForUserMusic())
        Settings[2] = 0;                /* their music wins over ours */

    /* ---- game font: two atlases, and a code table from a separate file */
    limeCreateFONT("MKFONT_0.PNG", "MKFONT_1.PNG", "mkfont_ipad.ft2",
                   &GameFont, 0, 0x800, 0x800, 1, 0.32499999f);
    GameFont.spacing = -4;
    GameFont.fallbackAdvance = 17;

    unicode = (const uint8_t *)limeLoadFile("mkunicode.txt");
    for (i = 0; i < GameFont.numGlyphs; i++) {
        const uint8_t *p = unicode + 2 + i * 2;   /* +2 skips the BOM */
        GameFont.codesW[i] = (int16_t)(p[0] | (p[1] << 8));
        GameFont.codes[i]  = p[0];
    }
    limeFree((void *)unicode);

    /* ---- name font: a fixed character set, widened in place */
    limeCreateFONT("NAMES_FONT.PNG", 0, "namefont.ft2",
                   &NameFont, 0, 0x100, 0x100, 1, 1.0f);
    memcpy(NameFont.codes, NameFontCodes, 0x58);
    for (i = 0; i < NameFont.numGlyphs; i++)
        NameFont.codesW[i] = NameFont.codes[i];
    NameFont.spacing = -2;
    NameFont.fallbackAdvance = 6;

    SFXHandle[0x68 / 4] = limeLoadSound("Gstart");

    /* ---- countdown font */
    limeCreateFONT("COUNTDOWNFONT.PNG", 0, "countdownfont.ft2",
                   &CountDownFont, 0, 0x80, 0x80, 1, 1.0f);
    CountDownFont.spacing = 0;
    CountDownFont.fallbackAdvance = 10;

    sprintf(langbuf, "LANGUAGE_TEXT_%s", Language);
    LoadTextData(limeGetPropertyString(langbuf));
    CheckAllUnicodeCharsUsed();

    NextTask = 2;
    FE_CurrentTask = 0;
    if (CheckForUnclaimedTreasure())
        FE_CurrentTask = 0x24;
    CurrentTask = 8;

    JSIZE = ButtonSize;
    JoystickStatePosX = (long)ButtonSize;
    JoystickStatePosY = (long)((float)limeScreenHeight - ButtonSize);
}

/* ----------------------------------------------------------------- QuitAsWin
 *
 * armv7 0x000265d4, 1020 bytes.  **Complete.**
 *
 * The mirror of `QuitAsLose` above, and the two are worth reading together:
 * they share a shape, the same fade constant, and the same per-mode exits, but
 * the win path carries the whole arcade ladder inside it.
 *
 * ### The ladder ends when the stage passes Destiny + 8
 *
 *      Stage++
 *      if (Stage >= Destiny + 8) { ...tower cleared... }
 *
 * `Destiny` is which of the four towers is being played, and it is also the
 * OFFSET that makes each tower longer than the last: tower 0 ends after 8
 * fights, tower 3 after 11. One field doing two jobs, which is why `Destiny`
 * is reset to **-1** and not to 0 when the tower is cleared -- 0 is a real
 * tower and -1 is the "no tower" value the front end tests for.
 *
 * Clearing a tower does five things in order: unlocks achievement 0 if the
 * player never lost a round on tower 3, sets this tower's bit in
 * `achievementTracker[0x54/4]` and unlocks achievement 1 once all four bits
 * are set, arms an unclaimed treasure, pushes front-end task 0x24 (the same
 * task `Task_LoadGeneralData` routes to at boot when a treasure is owed), and
 * clears `GameStarted`, `Destiny` and `Stage`.
 *
 * `TreasureSelected` is set to **-1**, not to a treasure: the value is chosen
 * later, on the screen that awards it. `SaveUnclaimedTreasure(1)` is what
 * makes it survive a quit.
 *
 * ### Playing as character 5 five times in a row unlocks achievement 13
 *
 *      if (PLAYER1MODEL == 5 && ++winningStryk == 5) achievementsUnlock(13);
 *
 * The test is `== 5`, not `>= 5`, so the achievement fires on exactly the
 * fifth win and never again in that run -- and `QuitAsLose` zeroes
 * `winningStryk`, so it is five consecutive wins as that character. This is
 * checked on two separate paths (the arcade tail and the survival tail),
 * written out twice in the original.
 *
 * ### Survival
 *
 *      survivalWinStreak++, Player1Wins = 1, (*SurvivalStageP)++
 *      if (survivalWinStreak > 19) { achievementsUnlock(19); Write_AchievementsData(); }
 *      Write_SaveData()
 *      Character2 = TowerRand[abs(limeRand()) % 22]
 *
 * Twenty survival wins unlock achievement 19. The next opponent is drawn from
 * `TowerRand`, the same 22-entry shuffle table `PopulateTower` uses, so
 * survival can repeat an opponent -- there is no exclusion, just a fresh draw.
 *
 * **`abs()` before the modulo, not after.** `limeRand` returns a signed value
 * and the code negates it when negative *before* dividing, so the index is
 * always in 0..21. Taking `% 22` of a negative first would have given a
 * negative index and read off the front of the table.
 *
 * ### Survival writes the save file twice
 *
 * The survival branch calls `Write_SaveData()`, and the common tail below it
 * calls `Write_SaveData()` again because the mode is not 1. Harmless, but it
 * is two full writes of the save file on every survival win, and the port
 * should not copy the second one into a build that writes to slower storage.
 *
 * ### The arcade logging, and what it says about the analytics
 *
 *      EASDK_LogEventEnumEnumString(0x7554, 15, DestinyNames[Destiny],
 *                                   15, getLayoutName(Settings[4], Settings[5]))
 *      EASDK_LogEventEnumEnumString(0x754e, 15, DestinyNamesWin[Destiny],
 *                                   15, getStageName(Destiny, Stage))
 *      EASDK_LogEventEnumEnumStringNum(0x754f, 15, DestinyNames[Destiny],
 *                                      7, (long)timeInGame)
 *      printf("time spent:%f\n", (double)timeInGame)
 *
 * `0x754e` is the SAME event id the loss path uses -- the two are told apart
 * by the string, `DestinyNamesWin[Destiny]` here against `DestinyNamesLoss`
 * there, not by the id. EA also logs the button layout on a win and not on a
 * loss, so the layout data set is biased towards players who were winning.
 */
extern const char **DestinyNamesWin;    /* pointer slot -> 0x00176838 */
extern long   TreasureSelected;         /* pointer slot */
extern long   playerLostRound;          /* pointer slot -> 0x000ff8b8 */
extern long   survivalWinStreak;        /* 0x0014e1ec */
extern long   TowerRand[];              /* pointer slot -> 0x001014d0 */
extern int    achievementTracker[24];   /* 0x00379c60, see achievements.c */

const char *getLayoutName(int buttons, int custom);

void QuitAsWin(void)
{
    JustWon = 1;

    if (GameMode == 0) {
        EASDK_LogEventEnumEnumString(0x7554, 15,
                DestinyNames[Destiny],
                15, getLayoutName(Settings[4], Settings[5]));
        EASDK_LogEventEnumEnumString(0x754e, 15,
                DestinyNamesWin[Destiny],
                15, getStageName(Destiny, Stage));
        EASDK_LogEventEnumEnumStringNum(0x754f, 15,
                DestinyNames[Destiny],
                7, (long)timeInGame);
        printf("time spent:%f\n", (double)timeInGame);
    }

    switch (GameMode) {
    case 1:
        PushFETask(0x28);
        WaitForOpponent = 1;
        if (isParentBasedOnSpeed())
            sendQuit();
        if (*FE_FadeAddP == 0.0f) {
            *FE_FadeAddP = -0.033333335f;
            UpdateStats();
        }
        break;

    case 2:
        *FE_FadeAddP = -0.033333335f;
        break;

    case 3:
        *FE_FadeAddP = -0.033333335f;
        exitTimeout = 600.0f;
        PushFETask(0x27);
        break;

    case 4:
        if (*FE_FadeAddP == 0.0f) {
            long r;

            survivalWinStreak++;
            Player1Wins = 1;
            (*SurvivalStageP)++;

            if (survivalWinStreak > 19) {
                achievementsUnlock(0x13);
                Write_AchievementsData();
            }

            Write_SaveData();
            r = limeRand();
            if (r < 0)
                r = -r;
            *FE_FadeAddP = -0.033333335f;
            Character2 = TowerRand[r % 22];

            if (PLAYER1MODEL == 5 && ++winningStryk == 5)
                achievementsUnlock(0xd);
        }
        break;

    case 5:
        *FE_FadeAddP = -0.033333335f;
        FE_TaskStackPointer = 0;
        FE_CurrentTask = 0;
        Player1Wins = 1;
        UpdateStats();
        break;

    default:
        /* Arcade (0) and mode 6 share this tail; only arcade counts the win
         * towards the streak. */
        if (GameMode != 6)
            winStreak++;

        if (PLAYER1MODEL == 5 && ++winningStryk == 5)
            achievementsUnlock(0xd);

        if (*FE_FadeAddP == 0.0f) {
            Player1Wins = 1;

            if (GameMode != 6) {
                Stage++;
                if (Stage >= Destiny + 8) {
                    /* Tower cleared. */
                    if (Destiny == 3 && playerLostRound == 0)
                        achievementsUnlock(0);

                    achievementTracker[0x54 / 4] |= 1 << Destiny;
                    if (achievementTracker[0x54 / 4] == 0xf)
                        achievementsUnlock(1);

                    TreasureSelected = -1;
                    SaveUnclaimedTreasure(1);
                    PushFETask(0x24);
                    GameStarted = 0;
                    Destiny = -1;
                    Stage = 0;
                }
            }

            *FE_FadeAddP = -0.033333335f;
            UpdateStats();
        }
        break;
    }

    if (GameMode != 1)
        Write_SaveData();
    Write_AchievementsData();
    if (Settings[2])
        FadeMusicOut = 1;
}

/* ------------------------------------------- Task_LoadingScreen_DRAWSCREEN
 *
 * armv7 0x0001d178, 844 bytes.  **Complete.**
 *
 * One frame of the loading screen: the background, one piece of advice, the
 * word "LOADING" and the percentage. `Task_LoadingScreen` calls it; the
 * prototype above this file's `Task_LoadingScreen` has been there since that
 * function landed.
 *
 * ### The background is drawn only once the texture has reached the GPU
 *
 *      if (LoadingTexture && LoadingTexture->name)
 *          limeDrawSprite(LoadingTexture, 0, 0, w, h, 0, 0, 1.0f, 0.75f, col);
 *
 * `TEXTURE+0x40` is the GL texture name, so the second half of that test is
 * "and it has actually been uploaded". A texture object that exists but has
 * not been given to GL draws nothing rather than drawing garbage -- which
 * matters here specifically, because this is the one screen that runs while
 * other textures are still being uploaded.
 *
 * **The V range stops at 0.75.** The image is 3/4 of its texture's height:
 * a 4:3 background in a square power-of-two atlas. A widescreen port that
 * replaces this art has to keep the same 0.75, or change it here as well.
 *
 * ### Which advice is shown
 *
 *      if (DrawRandomKode == 0 && Stage > 1)   drawKodeTip(randomKode)
 *      else if (Stage == 0)                    GameText(0x369)
 *      else if (Stage == 1)                    GameText(0x365)
 *      else                                    GameText(TipToDisplay + 0x365)
 *
 * **The flag reads inverted**: `DrawRandomKode` being SET is what suppresses
 * the kode tip, not what asks for it. Zero plus a stage past the first two is
 * what draws it. Whatever the name meant originally, the test in the binary
 * is the one above.
 *
 * Stages 0 and 1 get fixed strings -- the first two fights of a ladder, where
 * the player is most likely new -- and everything after that gets one of a
 * rotating set indexed by `TipToDisplay`, which is why the tips only start
 * varying once the player is past the opening.
 *
 * The chosen string is wrapped to `limeScreenWidth - 32` by
 * `CreateWrappedTextArrays` into `HelpSpiltText`, 256 bytes a line, and drawn
 * centred (`align 1`) at y = (line*16 + 132) * FE_HeightScale.
 *
 * ### Everything else is anchored to the far corner
 *
 *      "LOADING"   x = 8   * FE_WidthScale     align 0 (left)
 *      "%d%%"      x = w - 16 * FE_WidthScale  align 2 (right)
 *      both        y = h - 24 * FE_HeightScale
 *
 * The two share a baseline and sit at opposite ends of it. Note the y is
 * `limeScreenHeight - 24 * FE_HeightScale` and NOT `(h - 24) * scale`: the
 * screen height is used raw and only the inset is scaled, so the text stays
 * the same distance off the bottom edge on any screen. Getting that grouping
 * wrong is the classic way a port ends up with text off the bottom of the
 * display.
 *
 * ### The percentage is clamped, and the font is checked twice
 *
 *      sprintf(buf, "%d%%", percent >= 100 ? 100 : percent)
 *
 * so a caller that overshoots still shows 100%%, never 103%%.
 *
 * Both text draws are guarded by `GameFont.numGlyphs != 0` -- separately, with
 * the guard re-read between them. This function runs before
 * `Task_LoadGeneralData` has built the font, so on the very first loading
 * screen the guard is false and only the background is drawn. That is the
 * whole reason the check exists.
 */
extern char   HelpSpiltText[];          /* 0x00182c84, 256 bytes a line */
extern float  FE_WidthScale;            /* 0x000ff9b8 */
extern float  FE_HeightScale;           /* 0x000ff9bc */

const char *GameText(long id);
void drawKodeTip(long index);
long CreateWrappedTextArrays(const char *text, char *out, long *lines,
                             long maxWidth, void *font, float scale);

void Task_LoadingScreen_DRAWSCREEN(long a, long percent)
{
    /* The original's is EIGHT bytes -- sp+0x20, with `lines` immediately
     * after it at sp+0x28. That is exactly enough for "100%" and it is only
     * enough because the clamp below guards the top end. A NEGATIVE percent
     * is not clamped and formats up to 12 characters, straight over `lines`.
     * Sixteen here: identical for every value the game passes, and not a
     * stack overwrite for the ones it does not. See docs/GAME-BUGS.md. */
    char buf[16];                       /* sp+0x20, widened deliberately */
    long lines;                         /* sp+0x28 */
    long i;

    SetupFEScale();
    limeSet2DDrawing();
    limeEnableAlphaBlending_Basic();

    if (LoadingTexture != 0 && LoadingTexture->name != 0)
        limeDrawSprite(LoadingTexture, 0.0f, 0.0f,
                       (float)limeScreenWidth, (float)limeScreenHeight,
                       0.0f, 0.0f, 1.0f, 0.75f, col);

    if (a != 0) {
        const char *text;

        if (DrawRandomKode == 0 && Stage > 1) {
            drawKodeTip(randomKode);
        } else {
            if (Stage == 0)
                text = GameText(0x369);
            else if (Stage == 1)
                text = GameText(0x365);
            else
                text = GameText(TipToDisplay + 0x365);

            CreateWrappedTextArrays(text, HelpSpiltText, &lines,
                                    limeScreenWidth - 0x20,
                                    &GameFont, FE_WidthScale);

            for (i = 0; i < lines; i++)
                limeDrawFONT(&GameFont, limeUC(&HelpSpiltText[i * 256]),
                             (float)(limeScreenWidth / 2),
                             (float)(i * 16 + 0x84) * FE_HeightScale,
                             1, FE_WidthScale, fontcol);
        }
    }

    if (GameFont.numGlyphs != 0)
        limeDrawFONT(&GameFont, GameText(0x13),
                     8.0f * FE_WidthScale,
                     (float)limeScreenHeight + -24.0f * FE_HeightScale,
                     0, FE_WidthScale, fontcol);

    sprintf(buf, "%d%%", (int)(percent >= 100 ? 100 : percent));

    if (GameFont.numGlyphs != 0)
        limeDrawFONT(&GameFont, buf,
                     (float)limeScreenWidth + -16.0f * FE_WidthScale,
                     (float)limeScreenHeight + -24.0f * FE_HeightScale,
                     2, FE_WidthScale, fontcol);
}


/* ----------------------------------------------------------- InitAllGameData
 *
 * armv7 0x00021224, 1044 bytes.  **Complete.**
 *
 * Despite the name this function does exactly one thing: it lays out every
 * on-screen touch button, for every control scheme, from the screen size.
 * `Task_LoadGeneralData` calls it once at boot and reads `ButtonSize` straight
 * afterwards to place the joystick, so the two have to be read together.
 *
 * ### Everything comes from one ratio
 *
 *      k = limeScreenWidth / 640.0f      the design width
 *      B = k * ButtonSize                the one-player button
 *      C = B * 0.66666698f               the two-player button, 2/3 as big
 *
 * 640 is the layout the art was authored for; `k` is what carries it to any
 * other width. **Only the width feeds the scale** -- the height is used for
 * positions but never for scaling, so a taller screen gets the same button
 * size and more empty space, and a wider one gets bigger buttons.
 *
 * ### ButtonSize is rewritten in place, so this must run exactly once
 *
 *      ButtonSize = C / 0.66666698f      ( == B, up to one rounding )
 *
 * The global it read is the global it overwrites. Called twice, every button
 * would be scaled twice. Nothing else calls it -- `Task_LoadGeneralData` is
 * the only caller -- but a port that adds a "resolution changed" path cannot
 * simply call this again: it has to restore the design-time `ButtonSize`
 * first. Note also the round trip through 2/3 and back rather than just
 * keeping `B`; the two agree to within a float rounding, and the code stores
 * the round-tripped one.
 *
 * ### The layouts
 *
 * Every array is 120 bytes: six records of five words, of which this function
 * writes three -- x, y and size -- and leaves the last two alone. The
 * one-player arrays step in units of B from the bottom-right corner:
 *
 *      x: SW-0.5B, SW-1.5B, SW-2.5B      y: SH-0.5B, SH-1.5B
 *
 *      ButtonsPos4   4 buttons   a 2x2 block in the corner
 *      ButtonsPos5   5 buttons   the block plus one to its left
 *      ButtonsPos6   6 buttons   a 3x2 block
 *
 * `CustomButtonsPos4/5/6` get exactly the same values -- they are the player's
 * editable copy -- and are then copied into `DEFAULT_CustomButtonsPos4/5/6`,
 * which is what "reset controls" restores. **All three copies are 120 bytes**,
 * including the four- and five-button ones, so the unused records travel too.
 *
 * The two-player arrays split the screen. Player 2 (`_2`) works leftwards from
 * the right edge exactly as above but in units of C. Player 1 (`_1`) works
 * leftwards from
 *
 *      M = (limeScreenWidth / 2) - 0.5f * C
 *
 * and then subtracts a **flat 48 pixels** before stepping. That 48 is the only
 * number here that is not scaled by anything -- on a 640-wide screen it is
 * 48px of dead space either side of the centre line, and on a 2048-wide one it
 * is still 48px. For a widescreen port it is the first constant to revisit.
 *
 * ### The same value computed twice
 *
 * `M - C - 48 - C` and `(M - 2C) - 48` both appear, for what is arithmetically
 * one number, and both are truncated to int separately. They can differ by one
 * unit when the float rounding falls badly. Both are kept below rather than
 * folded into one, because folding them would be a change and not a
 * transcription.
 */

/* The records are the same 20-byte BUTTONPOS_STRIDE entries the touch code
 * above walks: word 0 x, word 1 y, word 2 size, word 4 the button id. This
 * function writes the first three and never touches the id -- that comes from
 * the preset data. Every array is 120 bytes, six records, however many the
 * layout actually uses. */
extern long ButtonsPosP2_1[];           /* 0x0014ff4c  2P mode, player 1 */
extern long ButtonsPosP2_2[];           /* 0x0014ffc4  2P mode, player 2 */
extern long ButtonsPos6P2_1[];          /* 0x0015003c  2P six-button, player 1 */
extern long ButtonsPos6P2_2[];          /* 0x001500b4  2P six-button, player 2 */
extern long CustomButtonsPos4[];        /* 0x00150294 */
extern long CustomButtonsPos5[];        /* 0x0015030c */
extern long CustomButtonsPos6[];        /* 0x00150384 */
extern long DEFAULT_CustomButtonsPos4[];        /* 0x001503fc */
extern long DEFAULT_CustomButtonsPos5[];        /* 0x00150474 */
extern long DEFAULT_CustomButtonsPos6[];        /* 0x001504ec */

static void SetButton(long *a, int n, long x, long y, long size)
{
    a[n * (BUTTONPOS_STRIDE / 4) + 0] = x;
    a[n * (BUTTONPOS_STRIDE / 4) + 1] = y;
    a[n * (BUTTONPOS_STRIDE / 4) + 2] = size;
}

void InitAllGameData(void)
{
    float SW = (float)limeScreenWidth;
    float SH = (float)limeScreenHeight;
    float halfW = (float)(limeScreenWidth / 2);
    /* Port, not in the binary: the device scaled the buttons by the WIDTH,
     * which on a surface wider than 3:2 (launcher aspect= 16:9 / 21:9) makes
     * them bigger than on the iPhone. There the width of the 3:2 screen of the
     * same height is used; the buttons still sit against the right edge. */
    float BW = (SW > SH * 1.5f) ? SH * 1.5f : SW;
    float B = (BW / 640.0f) * ButtonSize;
    float C = B * 0.66666698f;
    float M = halfW + C * -0.5f;

    /* Two-player geometry, in units of C. */
    long px1 = (long)((M - 48.0f) - C);
    long px2 = (long)(((M - C) - 48.0f) - C);
    long px2b = (long)((M + C * -2.0f) - 48.0f);   /* the same number, again */
    long px3 = (long)(((M + C * -2.0f) - 48.0f) - C);
    long py1 = (long)(SH + C * -0.5f);
    long py2 = (long)((SH + C * -0.5f) - C);
    long py3 = (long)((SH + C * -0.5f) + C * -2.0f);
    long qx1 = (long)(SW + C * -0.5f);
    long qx2 = (long)((SW + C * -0.5f) - C);
    long qx3 = (long)(C * -2.0f + (SW + C * -0.5f));
    long sz2 = (long)C;

    long bx1, bx2, bx3, by1, by2, sz1;
    int i;

    SetButton(ButtonsPosP2_1, 0, px1, py1, sz2);
    SetButton(ButtonsPosP2_1, 1, px2, py1, sz2);
    SetButton(ButtonsPosP2_1, 2, px2, py2, sz2);
    SetButton(ButtonsPosP2_1, 3, px1, py2, sz2);
    SetButton(ButtonsPosP2_1, 4, px3, py1, sz2);

    SetButton(ButtonsPosP2_2, 0, qx1, py2, sz2);
    SetButton(ButtonsPosP2_2, 1, qx2, py2, sz2);
    SetButton(ButtonsPosP2_2, 2, qx2, py3, sz2);
    SetButton(ButtonsPosP2_2, 3, qx1, py3, sz2);
    SetButton(ButtonsPosP2_2, 4, qx3, py3, sz2);

    SetButton(ButtonsPos6P2_1, 0, px1,  py1, sz2);
    SetButton(ButtonsPos6P2_1, 1, px2,  py1, sz2);
    SetButton(ButtonsPos6P2_1, 2, px1,  py2, sz2);
    SetButton(ButtonsPos6P2_1, 3, px2,  py2, sz2);
    SetButton(ButtonsPos6P2_1, 4, px3,  py1, sz2);
    SetButton(ButtonsPos6P2_1, 5, px2b, py3, sz2);

    SetButton(ButtonsPos6P2_2, 0, qx1, py2, sz2);
    SetButton(ButtonsPos6P2_2, 1, qx2, py2, sz2);
    SetButton(ButtonsPos6P2_2, 2, qx1, py3, sz2);
    SetButton(ButtonsPos6P2_2, 3, qx2, py3, sz2);
    SetButton(ButtonsPos6P2_2, 4, qx1, py1, sz2);
    SetButton(ButtonsPos6P2_2, 5, qx3, py3, sz2);

    /* One-player geometry, in units of B -- and the write-back. */
    bx1 = (long)(SW + B * -0.5f);
    by1 = (long)(SH + B * -0.5f);
    bx2 = (long)((SW + B * -0.5f) - B);
    by2 = (long)((SH + B * -0.5f) - B);
    bx3 = (long)((SW + B * -0.5f) + B * -2.0f);
    sz1 = (long)B;

    ButtonSize = C / 0.66666698f;

    /* The 2x2 corner block is identical in all three one-player layouts. */
    for (i = 0; i < 3; i++) {
        long *fixed  = (i == 0) ? ButtonsPos4
                     : (i == 1) ? ButtonsPos5 : ButtonsPos6;
        long *custom = (i == 0) ? CustomButtonsPos4
                     : (i == 1) ? CustomButtonsPos5 : CustomButtonsPos6;

        SetButton(fixed,  0, bx1, by1, sz1);
        SetButton(fixed,  1, bx2, by1, sz1);
        SetButton(fixed,  2, bx2, by2, sz1);
        SetButton(fixed,  3, bx1, by2, sz1);
        SetButton(custom, 0, bx1, by1, sz1);
        SetButton(custom, 1, bx2, by1, sz1);
        SetButton(custom, 2, bx2, by2, sz1);
        SetButton(custom, 3, bx1, by2, sz1);
    }

    SetButton(ButtonsPos5, 4, bx3, by1, sz1);
    SetButton(CustomButtonsPos5, 4, bx3, by1, sz1);

    SetButton(ButtonsPos6, 4, bx3, by1, sz1);
    SetButton(CustomButtonsPos6, 4, bx3, by1, sz1);
    SetButton(ButtonsPos6, 5, bx3, by2, sz1);
    SetButton(CustomButtonsPos6, 5, bx3, by2, sz1);

    /* Every copy is 120 bytes -- six records -- whatever the layout uses. */
    memcpy(DEFAULT_CustomButtonsPos4, CustomButtonsPos4, 0x78);
    memcpy(DEFAULT_CustomButtonsPos5, CustomButtonsPos5, 0x78);
    memcpy(DEFAULT_CustomButtonsPos6, CustomButtonsPos6, 0x78);
}


/* ---------------------------------------------------------------- Task_GameInit
 *
 * armv7 0x0002e6f4, 1232 bytes.  **Complete.**
 *
 * The task that builds a fight. It is a three-state machine driven by
 * `GameInitState`, and it runs once a frame like every other task -- the
 * loading is spread across frames so the loading screen keeps animating.
 *
 *      state 0   first frame: clear the players, reset the event system,
 *                GameInitState = 1
 *      state 1   one slice of loading per frame, then GameInitState = 2
 *      state 2   finish the loading screen, GameInitState = 0, and fall
 *                through into the fight setup
 *
 * **The fight setup is only reached from state 2** (or from an out-of-range
 * state), so states 0 and 1 return early and the long tail below runs exactly
 * once per fight.
 *
 * ### The loading bar is 52 steps
 *
 *      Task_LoadingScreen_DRAWSCREEN(1, GI_LoadCount * 100 / 52)
 *      done = GameInit_LoadABit(GI_LoadCount)
 *      GI_LoadCount++
 *
 * The division is the usual reciprocal multiply -- magic `0x4ec4ec4f` with a
 * shift of 36, which is exactly `1/52`. So `GameInit_LoadABit` is expected to
 * take **52 calls**, and the percentage is derived from that expectation
 * rather than from the function's own progress: if it finishes early the bar
 * never reaches 100 on its own, which is why state 2 redraws it at a literal
 * 100 before deleting the texture.
 *
 * ### Sixteen player slots, one word each
 *
 *      for (i = 0; i < 16; i++) ((long *)(Players + i * 0x5f0))[1] = 0;
 *
 * 0x5f00 / 0x5f0 = 16, which is the fourth independent confirmation of the
 * 0x5f0 PLAYER stride. Only `+0x04` is cleared -- the rest of each slot is
 * left as it was.
 *
 * ### Wins needed, by mode
 *
 *      mode 2 (training)   0, and no intro at all
 *      mode 4 (survival)   1
 *      mode 3 (karnage)    0, and KarnageScore = 0
 *      everything else     2
 *
 * Training is the only mode that clears `DoIntro`; every other mode sets it
 * and then runs **twenty-five frames of `AnimateIntroCharacterPlayers1Frame`
 * back to back** before returning, to settle the intro animation before the
 * first drawn frame. That is a visible cost at load time and a place a port
 * could stop early.
 *
 * ### Kode 0x13 in versus starts the match one round from the end
 *
 *      if (GameMode == 1 && theKode == 0x13) {
 *          RoundWins[0] = RoundWins[1] = WinsNeeded - 1;
 *          H[0] = H[1] = 1;
 *      }
 *
 * Both players at match point and both at one health: a one-hit-kill round.
 *
 * ### A treasure played in mode 5 rewrites the kode
 *
 *      if (GameMode == 5 && TreasurePlayed == 10) {
 *          theKode = TreasurePlayed - 9;      // 1
 *          puts("TREASURE PLAY IN DARK KOBAT MODE!");
 *      }
 *
 * `TreasurePlayed - 9` for a value the branch has already established is 10 --
 * so it is the constant 1 written as arithmetic, which suggests the branch was
 * once a range.
 *
 * ### Two identical debug prints
 *
 * `"ACTIVE EVENTS:%ld"` is printed before `LIME_KillAllEvents` and again after
 * `LIME_InitEventsManager`, from two separate copies of the same string, and
 * `dumpMem` walks `mpSpriteList` and `mpEventQueue` on every entry into state
 * 0. All of it is live in the shipped build.
 */
extern long  *InGameLevelSelect;        /* pointer slot -> 0x000ff7f4 */
extern long  *LevelSelectP;             /* pointer slot -> 0x000ff7f8 */
extern long   MoveListPage;             /* 0x00150eb4 */
extern long   SnapCam;                  /* 0x00150e90 */
extern long   GameInitState;            /* 0x00150e84 */
extern long   GI_LoadCount;             /* 0x00151088 */
extern long  *MKEventQueue;             /* pointer slot */
extern long  *lastTimestamp;            /* pointer slot */
extern float  ScorpionFade;             /* 0x0010df04 */
extern float  ScorpionFadeAdd;          /* 0x0010df08 */
extern float  ScorpionFlash;            /* 0x0010df0c */
extern long   ClockTens;                /* 0x0014fa50 */
extern long   ClockSingles;             /* 0x0014fa54 */
extern float  RoundSummaryTime;         /* 0x0014e220 */
extern long   RoundSummary;             /* 0x0014e224 */
extern long   Round;                    /* 0x0014e228 */
extern long   RoundWins[2];             /* 0x0014e22c */
extern long   FightMessage;             /* 0x0014e258 */
extern float  FightMessageTimer;        /* 0x0014e25c */
extern long   DontQuitAfterFade;        /* 0x0014e254 */
extern float  DoSmokesEarthFatal;       /* 0x0010defc -- a TICK COUNTER, not a
                                         * flag: Task_GameMain reads it with
                                         * vldr and thresholds it at 120, 240,
                                         * 300 and 330. */
extern long   DoSmokeEarthFatalSFX;     /* 0x0010df00 */
extern long   DoIntro;                  /* 0x0014e1c0 */
extern long   WinsNeeded;               /* 0x0014e234 */
extern long   KarnageScore;             /* 0x0014df88 */
extern long  *theKode;                  /* pointer slot -> 0x0010ded0 */
extern long  *H;                        /* pointer slot */
extern long  *TrainingCatagoryP;        /* pointer slot -> 0x0017809c */

void mk3_dizzy(void);

void limeMemoryReport(const char *label);
void InitVarEdit(void);
int LIME_CountActiveEvents(void);
void LIME_KillAllEvents(void);
void ResetFightData(void);
void heartbeatSetIncoming(long n);
void preprocessPostloadKode(void);
long GameInit_LoadABit(long step);

void Task_GameInit(void)
{
    long i;

    *InGameLevelSelect = *LevelSelectP;
    MoveListPage   = 0;
    LockCamera     = 0;
    OverrideCamera = 0;
    SnapCam        = 1;
    AllowCameraTracking();
    DidIntroThisFrame = 0;

    if (GameInitState == 1) {
        long done;

        Task_LoadingScreen_DRAWSCREEN(1, GI_LoadCount * 100 / 52);
        /* Port, not in the binary: as many steps as fit in the frame
         * (lime_port_load_more, draw_gl.c); the device ran one. */
        lime_port_load_more(1);
        do {
            done = GameInit_LoadABit(GI_LoadCount);
            GI_LoadCount++;
        } while (!done && lime_port_load_more(0));
        if (done) {
            lime_port_load_done("fight");
            GameInitState = 2;
        }
        return;
    }

    if (GameInitState == 2) {
        Task_LoadingScreen_DRAWSCREEN(1, 100);
        DeleteLoadingScreenTexture();
        GameInitState = 0;
        /* falls through to the fight setup */
    } else if (GameInitState == 0) {
        for (i = 0; i < 16; i++)
            ((long *)(Players + i * 0x5f0))[1] = 0;

        limeMemoryReport("Start OF Game Init Memory report");
        InitVarEdit();
        GI_LoadCount = 0;
        Task_LoadingScreen_DRAWSCREEN(1, 0);
        GameInitState = 1;
        GameObjects = 0;
        clearSpriteListsAndEvents();
        *MKEventQueue = 0;
        IsInFinishing = 0;

        printf("#########################\nACTIVE EVENTS:%ld\n"
               "###########################\n", LIME_CountActiveEvents());
        LIME_KillAllEvents();
        LIME_InitEventsManager();
        printf("#########################\nACTIVE EVENTS:%ld\n"
               "###########################\n", LIME_CountActiveEvents());

        dumpMem(mpSpriteList, 0x140, 0x20);
        dumpMem(mpEventQueue, 0x1b0, 0x20);
        return;
    }

    /* ---- the fight setup, reached from state 2 (or an out-of-range state) */
    CurrentTask    = 6;
    FrameCount     = 0.0f;
    *lastTimestamp = 0;

    if (GameMode == 1) {
        CurrentTask = 6 + 8;            /* 14 */
        syncGame(1);
    } else if (Settings[2] != 0) {
        limePlayTune(LevelMusic[*LevelSelectP],
                     (long)MusicVol[Settings[2]], 1);
    }

    FE_Fade      = 0.0f;
    *FE_FadeAddP = 0.033333335f;        /* fade IN, note the sign */
    ScorpionFade    = 0;
    ScorpionFadeAdd = 0;
    ScorpionFlash   = 0;

    if (GameMode == 2 && *TrainingCatagoryP == 2)
        mk3_dizzy();

    CalcShakeOffset(0);

    GameTime     = 99.0f;
    ClockTens    = 9;
    ClockSingles = 9;

    RoundSummaryTime = 0;
    RoundSummary     = 2;
    Round            = 0;
    RoundWins[0]     = 0;
    RoundWins[1]     = 0;

    FightMessage      = 0;
    FightMessageTimer = 0;
    DontQuitAfterFade = 0;

    DoSmokesEarthFatal   = 0.0f;
    DoSmokeEarthFatalSFX = 0;

    if (GameMode == 4)
        Health[0] = SurvivalHealth;

    ResetFightData();

    if (GameMode == 2) {
        DoIntro         = 0;
        IntroCount      = 0.0f;
        IntroCount2     = 0.0f;
        IntroCountTimer = 0.0f;
        IntroCamCount   = 0;
        WinsNeeded      = GameMode;     /* 2, from the register the test used */
    } else {
        DoIntro         = 1;
        IntroCount      = 0.0f;
        IntroCount2     = 0.0f;
        IntroCountTimer = 0.0f;
        IntroCamCount   = 0;

        if (GameMode == 4) {
            WinsNeeded = 1;
        } else {
            WinsNeeded = 2;
            if (GameMode == 3) {
                WinsNeeded   = 0;
                KarnageScore = 0;
            }
        }
    }

    timeInGame = 0.0f;
    DeleteLoadingScreenTexture();
    limeMemoryReport("End of GAME Init Memory report");

    heartbeatSetIncoming(3);
    heartbeatUpdate();

    /* Twenty-five frames of intro animation, run back to back. */
    AnimateIntroCharacterPlayers1Frame(1);
    for (i = 1; i < 25; i++)
        AnimateIntroCharacterPlayers1Frame(1);

    preprocessPostloadKode();
    puts("########## TASK_GAME_INIT: 10");

    if (GameMode == 1 && *theKode == 0x13) {
        RoundWins[0] = WinsNeeded - 1;
        RoundWins[1] = WinsNeeded - 1;
        H[0] = 1;
        H[1] = 1;
    }

    printf("GameMode = %d, TreasurePlayed = %d\n",
           (int)GameMode, (int)TreasurePlayed);

    if (GameMode == 5 && TreasurePlayed == 10) {
        *theKode = TreasurePlayed - 9;
        puts("\n#############################\n"
             "TREASURE PLAY IN DARK KOBAT MODE!");
    }
}


/* ------------------------------------------------------------- ResetFightData
 *
 * armv7 0x0002270c, 1364 bytes.  **Complete.**
 *
 * Puts every per-fight global back to its starting value. Most of it is a flat
 * list of zeroes; the interesting parts are the two places where it is not.
 *
 * ### G+0x368 and G+0x36c are health x 1.66
 *
 * Every kode branch writes a pair of numbers into the game state alongside the
 * health it sets:
 *
 *      health 100 -> 0xa6 = 166
 *      health  50 -> 0x53 =  83
 *      health  25 -> 0x29 =  41
 *
 * and the survival branch computes its one directly:
 *
 *      G->healthBar1 = Health[0] * 166 / 100
 *
 * 166, 83 and 41.5-truncated-to-41. **The two words are the health scaled by
 * 1.66**, which is the bar length -- 100 health drawn across 166 units. Five
 * independently written constants and one open-coded multiply all agreeing is
 * what settles it; from any one branch alone they are just numbers.
 *
 * ### Survival carries health between fights, and rescales it
 *
 *      SurvivalHealth = Health[0]          // whatever the last fight left
 *      if (SurvivalStage > 0 || Round != 0)
 *          G->healthBar1 = Health[0] * 166 / 100;
 *      else
 *          SurvivalHealth = Health[0] = 100;
 *
 * `Health[0]` is read **before** it is set, so it is the carry-over. Only the
 * very first round of the very first stage resets it. Note the bar is rescaled
 * from the carried health but `Health[0]` itself is left alone -- the fight
 * starts with the bar already short.
 *
 * ### Four handicap kodes, which are really two
 *
 *      0x13   both players to 1 health          (one hit kills)
 *      0x0d   100 against 50
 *      0x0e   100 against 25
 *      0x0f   100 against 50   -- identical to 0x0d in every branch
 *      0x10   100 against 25   -- 0x0e with the two sides SWAPPED
 *
 * Which player gets the short bar is decided by two network predicates:
 *
 *      0x0d, 0x0e, 0x0f    player 1 gets the big bar when
 *                          isParent() != isParentBasedOnSpeed()
 *      0x10                player 1 gets the big bar when
 *                          isParent() == isParentBasedOnSpeed()
 *
 * So `0x0d` and `0x0f` are byte-for-byte the same handicap under two different
 * kodes, and `0x10` is `0x0e` with the sides reversed. Whether the reversal is
 * the point of the kode or a slip is not established -- what is established is
 * that it is the only one of the four whose side rule differs.
 *
 * Using two predicates rather than one is how both machines in a network game
 * reach the same answer without exchanging anything: `isParent` is the session
 * role and `isParentBasedOnSpeed` is derived from the measured link, and their
 * agreement or disagreement is a value both ends compute identically.
 *
 * ### The babality bounce table
 *
 *      BabalityVel[0..7]    = 0
 *      BabalityHeight[0..7] = { 288, 256, 272, 256, 288, 272, 256, 288 }
 *
 * Eight entries each, written out one store at a time and deliberately out of
 * order in the original. Three distinct heights in a repeating pattern -- the
 * bounce is authored, not simulated.
 */
extern long  RunBar[2];                 /* 0x0014fa6c */
extern float FlawlessCounter;           /* 0x0014fb48 */
extern long  FlawlessMessage;           /* 0x0014fb4c */
extern long  DangerMessage[2];          /* 0x0014e23c */
extern long  RoundMessage;              /* 0x0014e250 */
extern float lightsOn;                  /* 0x0010decc */
extern uint8_t WinnerMessage[0x80];     /* 0x0014faa8..0x0014fb28, the next symbol (_BabalityMessage) */
extern long  RoundHasEnded;             /* 0x0014e248 */
extern long  FinishHimHer;              /* 0x0014e24c */
extern long  RoundHasEndedStatsUpdated; /* 0x0014e244 */
extern long  DoneSmashEffect;           /* 0x0010ded4 */
extern float BabalityVel[8];            /* 0x001f4104 */
extern float BabalityHeight[8];         /* 0x001f4124 */
extern long  MercyMessage;              /* 0x0014fb40 */
extern float MercyMessageCounter;       /* 0x0014fb44 */
/* The symbol table calls these four `_BabalityMessage`, `_AnimalityMessage`,
 * `_FatalityMessage` and `_FriendshipMessage`. The trailing G was invented
 * here, and it made each of them look like a different variable from the one
 * Blood.c writes at the same address. */
extern long  BabalityMessage;           /* 0x0014fb28 */
extern long  AnimalityMessage;          /* 0x0014fb2c */
extern long  FatalityMessage;           /* 0x0014fb30 */
extern long  FriendshipMessage;         /* 0x0014fb34 */
extern float AnimalityMessageCounter;   /* 0x0014fb38 -- float: DrawHUD's vldr */
extern float FatalityMessageCounter;    /* 0x0014fb3c -- float: DrawHUD's vldr */
extern long  DoingStageFatal;           /* 0x0010dee0 */
extern float DoingStageFatalBringForward;   /* 0x0010dee4 -- a float; Task_GameMain
                                             * walks it down to -1.2 in double */
extern long  sindelFlying;              /* 0x0014dff8 */
extern long  DoingSKDeath;              /* 0x0010deb8 */
extern float SKDeathMessageOffset;      /* 0x0010debc */
extern long  opponentPerformedMercy;    /* 0x0010dea4 */
extern long *SurvivalStageP2;           /* pointer slot -> 0x000ff980 */

void checkIfKode(void);
int  isParent(void);
int  isParentBasedOnSpeed(void);

/* The kode handicaps. `p1Big` says which side gets the long bar; the bar
 * lengths are the health scaled by 1.66, which is why they arrive as a pair. */
static void SetKodeHandicap(long bigHp, long bigBar,
                            long smallHp, long smallBar, int p1Big)
{
    if (p1Big) {
        G->healthBar1 = bigBar;
        Health[0]     = (int)bigHp;
        G->healthBar2 = smallBar;
        Health[1]     = (int)smallHp;
    } else {
        G->healthBar1 = smallBar;
        Health[0]     = (int)smallHp;
        G->healthBar2 = bigBar;
        Health[1]     = (int)bigHp;
    }
}

void ResetFightData(void)
{
    long i;

    if (GameMode == 4) {
        /* Survival: Health[0] is read BEFORE it is set -- the carry-over. */
        SurvivalHealth = Health[0];

        if (*SurvivalStageP2 > 0 || Round != 0)
            G->healthBar1 = Health[0] * 166 / 100;
        else
            SurvivalHealth = Health[0] = 100;
    } else {
        Health[0] = 100;
    }

    RunBar[0] = 100;
    RunBar[1] = 100;
    Health[1] = 100;
    BGSceneFrame[0] = 0.0f;
    BGSceneFrame[1] = 0.0f;

    checkIfKode();

    if (*theKode == 0x13) {
        Health[0] = 1;
        Health[1] = 1;
        G->healthBar1 = 1;
        G->healthBar2 = 1;
    } else if (*theKode == 0x0d) {
        SetKodeHandicap(100, 0xa6, 50, 0x53,
                        isParent() != isParentBasedOnSpeed());
    } else if (*theKode == 0x0e) {
        SetKodeHandicap(100, 0xa6, 25, 0x29,
                        isParent() != isParentBasedOnSpeed());
    } else if (*theKode == 0x0f) {
        SetKodeHandicap(100, 0xa6, 50, 0x53,
                        isParent() != isParentBasedOnSpeed());
    } else if (*theKode == 0x10) {
        /* The one whose side rule is inverted -- see the header. */
        SetKodeHandicap(100, 0xa6, 25, 0x29,
                        isParent() == isParentBasedOnSpeed());
    }

    FlawlessCounter  = 0;
    FlawlessMessage  = 0;
    DangerMessage[0] = 0;
    DangerMessage[1] = 0;
    RoundMessage     = 0;
    lightsOn         = 0;

    GameTime     = 99.0f;
    ClockTens    = 9;
    ClockSingles = 9;

    RoundSummaryTime = 0;

    WinnerMessage[0] = 0;
    WinnerMessage[1] = 0;

    RoundHasEnded             = 0;
    FinishHimHer              = 0;
    RoundHasEndedStatsUpdated = 0;
    DoneSmashEffect           = 0;

    for (i = 0; i < 8; i++)
        BabalityVel[i] = 0.0f;

    BabalityHeight[0] = 288.0f;
    BabalityHeight[1] = 256.0f;
    BabalityHeight[2] = 272.0f;
    BabalityHeight[3] = 256.0f;
    BabalityHeight[4] = 288.0f;
    BabalityHeight[5] = 272.0f;
    BabalityHeight[6] = 256.0f;
    BabalityHeight[7] = 288.0f;

    MercyMessage            = 0;
    MercyMessageCounter     = 0;
    BabalityMessage        = 0;
    AnimalityMessage       = 0;
    FatalityMessage        = 0;
    FriendshipMessage      = 0;
    AnimalityMessageCounter = 0;
    FatalityMessageCounter  = 0;

    DoingStageFatal             = 0;
    DoingStageFatalBringForward = 0.0f;
    sindelFlying                = 0;
    DoingSKDeath                = 0;
    SKDeathMessageOffset        = 0;
    opponentPerformedMercy      = 0;
}


/* -------------------------------------------------------------- RenderLevelBG
 *
 * armv7 0x00025168, 1564 bytes.  **Complete.**
 *
 * The whole stage background: two scene handles, up to eight extra mesh
 * layers, and one special case for level 1. This answers
 * [issue #17](../../issues/17) -- the port draws the first scene only.
 *
 * ### The second scene is real, and it is drawn with its own transform
 *
 *      scene 1   glRotatef(90, 1,0,0); glScalef(SceneScale);
 *                glTranslatef(SceneX, SceneY, SceneZ)
 *      scene 2   the same matrix, then
 *                glTranslatef(SceneX2, SceneY2, SceneZ2);
 *                glScalef(SceneScale2)
 *
 * Scene 2's transform is applied **inside** scene 1's push, so its offsets are
 * relative: `SceneY2 = -978.0` puts it 978 units below the stage, in the
 * already-rotated and already-scaled space. The port has to reproduce the
 * nesting, not just the numbers.
 *
 * ### Both scenes are drawn TWICE, and the only difference is one argument
 *
 *      LIME_RenderScene(-1, handle, frame, frame, 0, 0, 0, **0**, 0, 0, 0)
 *      LIME_RenderScene(-1, handle, frame, frame, 0, 0, 0, **1**, 0, 0, 0)
 *
 * Argument `d` is 0 then 1 -- the opaque pass and the transparent pass. Four
 * `LIME_RenderScene` calls per frame when both handles are set. A port that
 * draws each scene once will lose either the solid geometry or the glass,
 * depending which pass it happens to reproduce.
 *
 * ### With no second scene, the first is still drawn twice
 *
 * `BGSceneHandle2 == 0` takes an early path that renders scene 1's two passes
 * and then rechecks both handles -- and since the check that got it there was
 * `handle2 == 0`, the recheck always sends it to the tail. So the shape is:
 *
 *      no handle       draw nothing
 *      one handle      draw scene 1, two passes
 *      two handles     draw scene 1 and scene 2, two passes each
 *
 * ### Each scene is gated on `blast_state` and `CurrentScene`
 *
 *      draw scene 1 if (blast_state in 1..2) || CurrentScene == 0
 *      draw scene 2 if (blast_state in 1..2) || CurrentScene == 1
 *
 * `CurrentScene` picks between them when `blast_state` is 0 or above 2, and
 * both are drawn during a blast. So the two scenes are **alternatives** most of
 * the time and simultaneous during the transition -- which is why a port that
 * only ever draws scene 1 looks correct on the stages where `CurrentScene` is 0
 * and empty on the others.
 *
 * ### The second placement matrix is computed and then thrown away
 *
 *      limeMatrixMult(RealBGSceneMatrix, translate(Scene*2 * SceneScale2),
 *                     RealBGSceneMatrix + 16);
 *      memcpy(RealBGSceneMatrix + 16, RealBGSceneMatrix, 64);
 *
 * The multiply writes the second matrix and the copy immediately overwrites it
 * with a copy of the first, so **on leaving this function the two halves of
 * `RealBGSceneMatrix` are identical** and the `Scene*2` offsets have reached
 * only the GL matrix stack.
 *
 * `AnimateBG` (above) computes the same second matrix, correctly, every frame.
 * Which of the two a reader sees therefore depends on the order the two
 * functions run in -- and `AnimateBG` is the one that means it. Transcribed as
 * written; that this copy exists at all is the finding.
 *
 * ### All four draws use `BGSceneFrame[0]`
 *
 * `AnimateBG` maintains **two** frame counters, one per layer, and advances
 * them independently with their own loop flags. Every `LIME_RenderScene` here
 * passes `BGSceneFrame[0]`. So the second layer's counter is kept up to date
 * and never used: scene 2 is drawn at scene 1's frame. A port that wires
 * `BGSceneFrame[1]` to the second layer is fixing something, not transcribing
 * it.
 *
 * ### Five debug sliders, still live
 *
 *      Scene2X, Scene2Y, Scene2Z   -1500 .. 500
 *      Scene2Scale                   0.1 .. 2.0
 *      ShadowOffset                    0 .. 20
 *
 * plus a `LIME_printf` of a divider line, every frame. The ranges are the
 * useful part: they say what the author considered plausible, and `-1500` is
 * what makes `SceneY2 = -978` an ordinary value rather than an outlier.
 *
 * ### Eight extra mesh layers per stage, from LEVEL_INFO
 *
 *      for (i = 0; i < 8; i++)
 *          if (*(long *)(rec + 0xd4 + i*4) && ((char **)(rec + 0x74))[i][0])
 *              RenderAMesh(0, 0, &LevelBGPos, IdentityMatrix, 0,
 *                          LevelBGTexture[((long *)(rec + 0xb4))[i]],
 *                          0, *MeshSetLayers[i], 0);
 *
 * That settles three arrays inside the 244-byte `LEVEL_INFO` record: **+0x74 is
 * eight `char *` names, +0xb4 eight texture indices, +0xd4 eight enable flags**
 * -- and `0xd4 + 8*4 = 0xf4`, exactly the stride, so the flags are the last
 * field in the record. A layer is drawn only when its flag is set **and** its
 * name is a non-empty string; either alone is not enough.
 *
 * ### Level 1 has a hand-written glass effect
 *
 * When `LevelSelect == 1`, layer 0 is absent and `ExtraEffects` is set, the
 * loop is interrupted by 32 additive passes of `MeshSet_LEVEL_01`:
 *
 *      alpha = (1.0 + i * -0.03125) / 10.0        i = 0 .. 31
 *      GlassWindowPos.y -= 40.0 each pass
 *      ...then GlassWindowPos.y += 1280.0         (32 * 40, restored exactly)
 *
 * Thirty-two stacked translucent copies of one mesh, each 40 units lower and
 * one thirty-second fainter, drawn additively with depth test and depth writes
 * off. That is a volumetric shaft built out of slices -- and `-1/32` with 32
 * iterations means the last slice has alpha `(1 - 31/32) / 10`, just above
 * zero. `TestScale` is 0.0095 during it and 0.01 after, and the position is
 * restored exactly, so nothing leaks into the next frame.
 *
 * This is the single most expensive thing in the background: 32 draw calls for
 * one effect on one stage.
 */
extern void **MeshSetLayers[8];         /* 0x0014f910 -- each entry points AT
                                         * a meshset handle, so the draw needs
                                         * two dereferences */
/* The eight handles MeshSetLayers points at, 0x001aba20..0x001aba3c in
 * __common. Declared so tools/mkglobals.py can spell MeshSetLayers'
 * initialiser: undeclared, it stopped at the first word and left the table
 * NULL, and stage 1 (the only one with a layer, TEMPLEWINDOW.meshset) wrote
 * through MeshSetLayers[1] at GameInit_LoadABit step 30 and crashed. */
extern void  *MeshSet_LEVEL_00;         /* 0x001aba20 */
extern void  *MeshSet_LEVEL_02;         /* 0x001aba28 */
extern void  *MeshSet_LEVEL_03;         /* 0x001aba2c */
extern void  *MeshSet_LEVEL_04;         /* 0x001aba30 */
extern void  *MeshSet_LEVEL_05;         /* 0x001aba34 */
extern void  *MeshSet_LEVEL_06;         /* 0x001aba38 */
extern void  *MeshSet_LEVEL_07;         /* 0x001aba3c */
extern void  *LevelBGTexture[];         /* 0x001abb28 */
extern float  LevelBGPos[3];            /* 0x0015057c */
extern float  GlassWindowPos[3];        /* 0x0014f9e4 */
extern void  *MeshSet_LEVEL_01;         /* 0x001aba24 */
extern float  IdentityMatrix[16];       /* 0x0014f9a4 */

void limeEnableDepthTest(void);
void limeDisableDepthTest(void);
void limeEnableAlphaBlending_Additive(void);
void limeDisableDepthWrites(void);

/* One extra background layer, drawn only when both its flag and its name say
 * so. See the header for the three arrays inside LEVEL_INFO. */
static void RenderLevelExtra(const char *rec, long i)
{
    RenderAMesh(0, 0, (limeVECTOR3 *)LevelBGPos,
                (limeMATRIX44 *)IdentityMatrix, 0,
                (TEXTURE *)LevelBGTexture[((const long *)(rec + 0xb4))[i]],
                0,
                (MESHSETINFO *)*MeshSetLayers[i],
                0);
}

void RenderLevelBG(void)
{
    float m1[16], m2[16], m3[16];
    long  i;

    limeEnableDepthWrites();
    limeEnableDepthTest();
    limeEnableAlphaBlending_Basic();

    /* The stage's placement matrix: rotate Z-up to Y-up, scale, translate. */
    RotMatrixX(M_Rot90, 1.5707964f);
    limeMatrixLoadIdentity(m1);
    limeMatrixMult(M_Rot90, m1, m2);
    limeScaleMatrix(m2, SceneScale);

    limeMatrixLoadIdentity(m3);
    m3[12] = SceneX;
    m3[13] = SceneY;
    m3[14] = SceneZ;
    limeMatrixMult(m3, m2, RealBGSceneMatrix);

    /* Computed, and overwritten on the next line. See the header. */
    limeMatrixLoadIdentity(m3);
    m3[12] = SceneX2 * SceneScale2;
    m3[13] = SceneY2 * SceneScale2;
    m3[14] = SceneZ2 * SceneScale2;
    limeMatrixMult(RealBGSceneMatrix, m3, RealBGSceneMatrix + 16);

    memcpy(RealBGSceneMatrix + 16, RealBGSceneMatrix, 0x40);

    LIME_PushMatrix();
    glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
    glScalef(SceneScale, SceneScale, SceneScale);
    glTranslatef(SceneX, SceneY, SceneZ);

    ClearDebugWindow(1);
    glCullFace(0x405);                  /* GL_BACK */

    if (BGSceneHandle != 0) {
        if (BGSceneHandle2 == 0) {
            /* One scene only: draw its two passes and fall to the tail. */
            LIME_PushMatrix();
            LIME_RenderScene(-1, BGSceneHandle, (long)BGSceneFrame[0],
                             (long)BGSceneFrame[0], 0.0f, 0, 0, 0, 0, 0, 0);
            LIME_RenderScene(-1, BGSceneHandle, (long)BGSceneFrame[0],
                             (long)BGSceneFrame[0], 0.0f, 0, 0, 1, 0, 0, 0);
            LIME_PopMatrix(1);
        } else {
            int blasting = (blast_state != 0 && blast_state <= 2);

            if (blasting || CurrentScene == 0) {
                LIME_PushMatrix();
                LIME_RenderScene(-1, BGSceneHandle, (long)BGSceneFrame[0],
                                 (long)BGSceneFrame[0], 0.0f, 0, 0, 0, 0, 0, 0);
                LIME_RenderScene(-1, BGSceneHandle, (long)BGSceneFrame[0],
                                 (long)BGSceneFrame[0], 0.0f, 0, 0, 1, 0, 0, 0);
                LIME_PopMatrix(1);
            }

            LIME_printf(1, "--------------------------------------\n");
            LIME_Slider(1, &SceneX2, "Scene2X", -1500.0f, 500.0f, 0, 0);
            LIME_Slider(1, &SceneY2, "Scene2Y", -1500.0f, 500.0f, 0, 0);
            LIME_Slider(1, &SceneZ2, "Scene2Z", -1500.0f, 500.0f, 0, 0);
            LIME_Slider(1, &SceneScale2, "Scene2Scale", 0.1f, 2.0f, 0, 0);
            LIME_Slider(1, &ShadowOffset, "ShadowOffset", 0.0f, 20.0f, 0, 0);

            if (blasting || CurrentScene == 1) {
                LIME_PushMatrix();
                glTranslatef(SceneX2, SceneY2, SceneZ2);
                glScalef(SceneScale2, SceneScale2, SceneScale2);
                LIME_RenderScene(-1, BGSceneHandle2, (long)BGSceneFrame[0],
                                 (long)BGSceneFrame[0], 0.0f, 0, 0, 0, 0, 0, 0);
                LIME_RenderScene(-1, BGSceneHandle2, (long)BGSceneFrame[0],
                                 (long)BGSceneFrame[0], 0.0f, 0, 0, 1, 0, 0, 0);
                LIME_PopMatrix(1);
            }
        }
    }

    limeDisableAlphaBlending();
    limeEnableDepthWrites();
    LIME_PopMatrix(1);

    /* ---- the eight extra layers, and level 1's glass */
    for (i = 0; i <= 7; i++) {
        const char *rec = Level_Info + *LevelSelectP * LEVEL_INFO_STRIDE;

        if (*(const long *)(rec + 0xd4 + i * 4) != 0
            && ((const char *const *)(rec + 0x74))[i][0] != 0)
            RenderLevelExtra(rec, i);

        /* Slot 0 on level 1 is followed by the glass, whether or not the slot
         * itself drew anything. */
        if (i != 0 || *LevelSelectP != 1 || ExtraEffects == 0)
            continue;

        TestScale = 0.0095f;
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        limeEnableAlphaBlending_Additive();
        limeDisableDepthTest();
        limeDisableDepthWrites();

        {
            long slice;
            /* the iPad build (0x2561e there): 12 slices 55 apart, the same
             * alpha step; the iPhone's 32 slices 40 apart */
            long  slices = PortIPad ? 12 : 32;
            float pitch  = PortIPad ? 55.0f : 40.0f;

            for (slice = 0; slice < slices; slice++) {
                RenderMeshAlphaOverRide =
                    (float)((1.0 + (double)slice * -0.03125) / 10.0);

                RenderAMesh(0, 0, (limeVECTOR3 *)GlassWindowPos,
                            (limeMATRIX44 *)IdentityMatrix, 0,
                            (TEXTURE *)LevelBGTexture[1], 0,
                            (MESHSETINFO *)MeshSet_LEVEL_01, 0);

                GlassWindowPos[1] = GlassWindowPos[1] - pitch;
            }
        }

        TestScale = 0.01f;
        /* 32 * 40 = 1280 on the iPhone, 12 * 55 = 660 on the iPad, exact */
        GlassWindowPos[1] = GlassWindowPos[1] + (PortIPad ? 660.0f : 1280.0f);
        RenderMeshAlphaOverRide = 1.0f;

        limeEnableDepthTest();
        limeEnableDepthWrites();
        limeEnableAlphaBlending_Basic();
    }

    limeEnableAlphaBlending_Basic();
}


/* ---------------------------------------------------------- DrawMoveListIcons
 *
 * armv7 0x0001e60c, 1856 bytes.  **Complete.**
 *
 * Draws one move's input sequence as a row of button icons. This is the
 * function behind [issue #5](../../issues/5), and it settles the half of that
 * issue that needed settling: **the icon alphabet**.
 *
 * ### The printf in issue #5 is the first line of this function
 *
 *      printf("%d\n", seq[0]);
 *
 * Unconditional, every frame, first thing. That is the source of the 10,208
 * bare-number lines in the touchHLE log -- and it prints **only `seq[0]`**, not
 * the whole sequence. The periodicity in the log is the screen being redrawn,
 * not the sequence being walked, so a run of period 6 is a six-frame animation
 * cycle in the caller, not a six-input move.
 *
 * That changes what the capture in issue #5 is worth: it gives the FIRST input
 * of each move, repeated, not the input list. The list is `seq`, and the way to
 * read it is the table below.
 *
 * ### The icon alphabet: a 4x4 atlas, all sixteen cells used
 *
 * Values 0 to 15 index `MoveIconsTexture` by writing a `(u, v)` pair, each from
 * {0, 0.25, 0.5, 0.75}, and drawing a quarter-by-quarter cell. Sixteen values,
 * sixteen cells, one each:
 *
 *          u=0     u=0.25   u=0.5   u=0.75
 *      v=0     0       1        2       3
 *      v=.25   4       5        9       8
 *      v=.5   13       7        6      14
 *      v=.75  12      11       10      15
 *
 * Value 0 reaches cell (0,0) the same way an out-of-range value does -- the
 * range check is `(unsigned)(value - 1) > 21`, so 0 wraps into the default --
 * but 0 is a real value that appears throughout the move tables, so cell (0,0)
 * is a real glyph and not a blank. The first row is 0, 1, 2, 3 in order and the
 * rest is not, which says the atlas was laid out by hand.
 *
 * Each icon is drawn `16 x 16` design units scaled by `FE_WidthScale` and
 * `FE_HeightScale`, two units above the row's baseline, and the cursor advances
 * **17** units -- one unit of gap.
 *
 * ### Values 16 to 22 are text, not icons
 *
 *      16   GameTextNoHeader(0x3a8)
 *      17   GameTextNoHeader(0x3a9)
 *      18   "("
 *      19   ") "
 *      20   "/"
 *      21   GameTextNoHeader(0x3aa)
 *      22   GameTextNoHeader(0x3f9)
 *
 * So a move's notation can contain brackets and a slash -- "(x / y)" style
 * alternatives -- and four translated words. Those seven values will never
 * appear as button inputs in the fight engine; they are punctuation for the
 * display. Anyone decoding a captured sequence has to split the alphabet at 15.
 *
 * Text advances by `limeGetStringWidth * 0.75 * FE_WidthScale` and is drawn at
 * scale `0.75 * FE_WidthScale`, so the punctuation is three-quarter size next
 * to the icons.
 *
 * ### The two directions are the same dispatch written twice
 *
 *      rightAlign == 0   caption first, then icons left to right from FE_X(48)
 *      rightAlign != 0   icons first, then the caption, anchored to
 *                        limeScreenWidth - FE_X(48) and walked BACKWARDS
 *
 * The sequence reads left to right on screen either way -- only the anchor and
 * the order of the caption differ. The 22-way dispatch, the atlas coordinates
 * and the advance are duplicated in full for the two cases; roughly two thirds
 * of this function's 1856 bytes are that duplication.
 *
 * The caption is wrapped as `"(%s)  "` and skipped when `limeFontStrLen`
 * returns 0.
 */
extern void *MoveIconsTexture;          /* 0x001abb70 */

int limeFontStrLen(const char *s);
float limeGetStringWidth(void *font, const char *s);   /* a FLOAT -- see
                                         * decomp/lime/limeFont.c, and the
                                         * unconverted `vmov s10, r0` in
                                         * FE_Task_VS_Screen */

/* Values 1..15 -> the atlas cell, as (v, u). Written out because the original
 * is a 22-way jump table with one `mov` pair per case; the table is the fact,
 * the dispatch is not. */
static const float MoveIconUV[16][2] = {
    { 0.00f, 0.00f },                   /* 0 and out-of-range: the blank cell */
    { 0.00f, 0.25f },                   /* 1  */
    { 0.00f, 0.50f },                   /* 2  */
    { 0.00f, 0.75f },                   /* 3  */
    { 0.25f, 0.00f },                   /* 4  */
    { 0.25f, 0.25f },                   /* 5  */
    { 0.50f, 0.50f },                   /* 6  */
    { 0.50f, 0.25f },                   /* 7  */
    { 0.25f, 0.75f },                   /* 8  */
    { 0.25f, 0.50f },                   /* 9  */
    { 0.75f, 0.50f },                   /* 10 */
    { 0.75f, 0.25f },                   /* 11 */
    { 0.75f, 0.00f },                   /* 12 */
    { 0.50f, 0.00f },                   /* 13 */
    { 0.50f, 0.75f },                   /* 14 */
    { 0.75f, 0.75f }                    /* 15 */
};

/* Port, not in the binary: with the on-screen controls hidden the buttons
 * are drawn as the player's own controls (PortMoveGlyph, game_main.c):
 * one picture, or player one's and player two's with a "/" between. 0 when
 * the letters stay. */
int  PortMovePlayers(void);
int  PortMoveGlyph(long value, int p, char *out, int n);
TEXTURE *lime_load_extra_texture(const char *file);
void lime_extra_texture_size(const TEXTURE *t, int *w, int *h);

static long PortMoveListGlyphs(long value, long cursor, int right, float y)
{
    int who = PortMovePlayers(), p, k = 0, i;
    TEXTURE *t[2] = { 0, 0 };
    float w[2] = { 0, 0 }, total = 0, x0;
    char file[160];

    if (!who || value < 6 || value > 14)
        return -1;
    for (p = 0; p < 2; p++) {
        int tw, th;
        if (!(who & (1 << p)) || !PortMoveGlyph(value, p, file, sizeof file))
            continue;
        t[k] = lime_load_extra_texture(file);
        if (!t[k])
            continue;
        lime_extra_texture_size(t[k], &tw, &th);
        w[k] = th > 0 ? FE_WidthScale * 16.0f * (float)tw / (float)th : FE_WidthScale * 16.0f;
        k++;
    }
    if (k == 0)
        return -1;
    total = w[0] + (k > 1 ? FE_WidthScale * 8.0f + w[1] : 0.0f);
    x0 = right ? (float)(limeScreenWidth - cursor) - total : (float)cursor;
    for (i = 0; i < k; i++) {
        limeDrawSprite(t[i], x0, y + FE_HeightScale * -2.0f, w[i],
                       FE_HeightScale * 16.0f, 0.0f, 0.0f, 1.0f, 1.0f, col);
        x0 += w[i];
        if (i == 0 && k > 1) {
            limeDrawFONT(&GameFont, limeUC("/"), x0 + FE_WidthScale * 1.0f, y,
                         0, 0.75f * FE_WidthScale, fontcol);
            x0 += FE_WidthScale * 8.0f;
        }
    }
    return (long)((float)cursor + total + FE_WidthScale * 1.0f);
}

/* Draw one entry and return the new cursor. The two directions differ only in
 * where the entry lands and how text is aligned, so they share this. */
static long DrawMoveListEntry(long value, long cursor, int right, float y,
                              char *buf)
{
    const char *text = 0;
    float       x = right ? (float)(limeScreenWidth - cursor) : (float)cursor;

    {
        long next = PortMoveListGlyphs(value, cursor, right, y);
        if (next >= 0)
            return next;
    }

    if (value >= 16 && value <= 22) {
        switch (value) {
        case 16: text = GameTextNoHeader(0x3a8); break;
        case 17: text = GameTextNoHeader(0x3a9); break;
        case 21: text = GameTextNoHeader(0x3aa); break;
        case 22: text = GameTextNoHeader(0x3f9); break;
        case 18: usprintf(buf, UC("(")); break;
        case 19: usprintf(buf, UC(") ")); break;
        default: usprintf(buf, UC("/")); break;      /* 20 */
        }

        if (text != 0)
            usprintf(buf, UC("%s "), text);

        limeDrawFONT(&GameFont, limeUC(buf), x, y,
                     right ? 2 : 0, 0.75f * FE_WidthScale, fontcol);

        return cursor + (long)((double)limeGetStringWidth(&GameFont, limeUC(buf))
                               * 0.75 * (double)FE_WidthScale);
    }

    {
        long cell = (value >= 1 && value <= 15) ? value : 0;

        /* Right-anchored icons are shifted back by their own width so the
         * right edge sits on the cursor. */
        limeDrawSprite((TEXTURE *)MoveIconsTexture,
                       right ? x + FE_WidthScale * -16.0f : x,
                       y + FE_HeightScale * -2.0f,
                       FE_WidthScale * 16.0f,
                       FE_HeightScale * 16.0f,
                       MoveIconUV[cell][1], MoveIconUV[cell][0],
                       0.25f, 0.25f, col);
    }

    return (long)((float)cursor + FE_WidthScale * 17.0f);
}

void DrawMoveListIcons(int y, const int *seq, const char *caption,
                       int rightAlign)
{
    char buf[128];                      /* sp+0x18 */
    long count = 0;
    long cursor;
    long i;

    /* Every frame, first thing, and only seq[0]. See the header. */
    printf("%d\n", seq[0]);

    if (seq[0] != -1) {
        do {
            count++;
        } while (seq[count] != -1);
    }

    cursor = (long)FE_X(48.0f);

    if (rightAlign == 0) {
        if (limeFontStrLen(caption) != 0) {
            usprintf(buf, UC("(%s)  "), caption);
            limeDrawFONT(&GameFont, limeUC(buf), (float)cursor, (float)y,
                         0, 0.75f * FE_WidthScale, fontcol);
            cursor += (long)((double)limeGetStringWidth(&GameFont, limeUC(buf))
                             * 0.75 * (double)FE_WidthScale);
        }

        for (i = 0; i < count; i++)
            cursor = DrawMoveListEntry(seq[i], cursor, 0, (float)y, buf);
        return;
    }

    /* Right-anchored: the sequence is walked from the end so that it still
     * reads left to right on screen. */
    for (i = count - 1; i >= 0; i--)
        cursor = DrawMoveListEntry(seq[i], cursor, 1, (float)y, buf);

    if (limeFontStrLen(caption) == 0)
        return;

    usprintf(buf, UC("(%s)  "), caption);
    limeDrawFONT(&GameFont, limeUC(buf),
                 (float)(limeScreenWidth - cursor), (float)y,
                 2, 0.75f * FE_WidthScale, fontcol);
}


/* ------------------------------------------------------------ GameInit_LoadABit
 *
 * armv7 0x0002b940, **11,700 bytes** -- the largest function in `gamecode`.
 * **Complete.**
 *
 * One slice of fight loading per call. `Task_GameInit` calls it with
 * `GI_LoadCount` and stops when it returns non-zero; the body is a 53-way
 * `tbh` on that counter, `0 .. 0x34`. **Only step 52 returns 1.** Every other
 * step returns 0, including the ones that find nothing to do -- so a skipped
 * step costs one frame and nothing else.
 *
 * It is worth reading as three things at once: the **asset manifest for a
 * fight**, the **map of the LEVEL_INFO record**, and the **character index
 * table**.
 *
 * ## The roster, settled six ways
 *
 * Four steps are 24-way switches picking a per-character stage-death scene, and
 * all four order the characters identically. `_CharacterNames` (0x0014fe54, an
 * array of `char *`) then names them, and it runs to 26 -- exactly the
 * `FE_CHARACTER_SLOTS` that `Players.c` measured from a symbol gap and
 * `Task_FEDestroy` measured from a texture array:
 *
 *       0 KANO         7 SEKTOR      14 SMOKE        21 SUB-ZERO (classic)
 *       1 SONYA        8 CYRAX       15 KITANA       22 SMOKE (classic)
 *       2 JAX          9 KUNG LAO    16 JADE         23 NOOB SAIBOT
 *       3 NIGHTWOLF   10 KABAL       17 MILEENA      24 MOTARO
 *       4 SUB-ZERO    11 SHEEVA      18 SCORPION     25 SHAO KAHN
 *       5 STRYKER     12 SHANG TSUNG 19 REPTILE
 *       6 SINDEL      13 LIU KANG    20 ERMAC
 *
 * That closes a set of loose ends across the whole tree at once:
 *
 *   - `defeatedBySK` in `QuitAsLose` counts losses to `PLAYER2MODEL == 25`.
 *     **25 is Shao Kahn.** The variable was named for him and it checks out.
 *   - `QuitAsWin` and `FE_Task_Main_Menu` count a streak in `winningStryk`,
 *     gated on `PLAYER1MODEL == 5`. **5 is Stryker** -- the odd variable name
 *     is the character's.
 *   - `FE_Task_Character_Select` sets `Character2 = 0x19` for karnage and for
 *     mode 5. That is Shao Kahn as the fixed opponent.
 *   - the death switches stop at 23, so **Motaro and Shao Kahn have no stage
 *     death** -- they are bosses, not selectable fighters.
 *   - `Stats[PLAYER1MODEL + 15]++` below is the per-character play counter, and
 *     `FE_Task_Stats` scans **23** of them starting at word 15. Characters 23,
 *     24 and 25 are counted and never scanned, so Noob Saibot can never be your
 *     "most played".
 *
 * ## The LEVEL_INFO record, mostly mapped
 *
 * Steps 27 to 30 and 51 read most of the 244-byte record. With the three arrays
 * `RenderLevelBG` settles, the map is now:
 *
 *      +0x0c .. +0x18   SceneX, SceneY, SceneZ, SceneScale   (floats)
 *      +0x1c            scene 1 filename        (char *)
 *      +0x20            scene 1 loop flag
 *      +0x24, +0x28     ground offsets, float -> int
 *      +0x40 .. +0x4c   SceneX2, SceneY2, SceneZ2, SceneScale2
 *      +0x50            scene 2 filename
 *      +0x54            scene 2 loop flag
 *      +0x58, +0x5c     ground offsets used when BOTH scenes exist
 *      +0x74 + i*4      eight extra-layer mesh names   (char *, "" = absent)
 *      +0x94 + i*4      eight extra-layer texture names
 *      +0xb4 + i*4      eight texture indices
 *      +0xd4 + i*4      eight enable flags               (0xd4 + 32 = 0xf4)
 *
 * **`CurrentScene` is set to 1 exactly when both `+0x1c` and `+0x50` are
 * non-null**, and that same branch is the only thing that writes
 * `SceneX2..SceneScale2`. So the second background layer `RenderLevelBG` draws
 * is enabled by the stage data carrying two scene filenames and by nothing
 * else -- which answers the last open question in
 * [issue #17](../../issues/17).
 *
 * ## Two stages have per-character deaths
 *
 *      LevelSelect == 4    SUBWAY_<CHARACTER>.scene
 *      LevelSelect == 11   LAIR_<CHARACTER>.scene
 *
 * Step 46 loads player 1's into `TrainDie1Scene` and step 47 player 2's into
 * `TrainDie2Scene`; steps 44 and 45 do the same pair for the lair into
 * `SLDie1Scene` and `SLDie2Scene`. In endurance the second slot comes from
 * `LastEnduranceCharacter` rather than `Character2`, so the death matches
 * whoever is actually on screen.
 *
 * Forty-eight scene files for two stages, which is why they are the two most
 * expensive stages to enter.
 *
 * ## The manifest, in load order
 *
 *      2      FE_BUTTONS_01.PNG
 *      3-12   FATAL_HUDGFX_0..9.PNG      ten frames, ONE PER STEP
 *      13     MOVES_ICONS.PNG            the atlas DrawMoveListIcons indexes
 *      14     DANGER.PNG
 *      15     LOGO_COIN_32.PNG
 *      16     LoadBloodTextures()
 *      17     BLOODSPLAT1 / GREENBLOODSPLAT1 / BLACKBLOODSPLAT1 / SMOKEPARTICLE2
 *      18     InitParticles, InitGameEvents, STARFIELD / EARTH_128 / EXPLOSION_0
 *      19-20  SPEAR1..4, TRIDENT4
 *      21-26  WHITE, PAUSE, INFO, X, HUD_TPAGE, PAUSEBG, BUTTONS_TPAGE
 *      27-28  the stage's two scenes, from LEVEL_INFO
 *      29-30  the eight extra layers' textures and meshes
 *      31-32  FIGHT.meshset, FIGHT.scene
 *      33-40  SHOCKWAVE2, JAXZAP8, GENERICBLOODEXPLODE, XEROX,
 *             GENERICBODYEXPLODE, ROCKFALL, SUBWAY_TRAIN, ROCKBREAKTHROUGH
 *      41     SK_ENDING.scene, outside endurance only
 *      48     LoadLevelCharacters(PLAYER1MODEL, PLAYER2MODEL)
 *      49-50  PreloadGameCharacters for endurance, and for Stryker
 *      52     LoadAllSounds, mk3_init, and the return of 1
 *
 * **Ten separate steps to load ten HUD frames** is the clearest statement of
 * what the counter is for: the work is chopped so no single frame stalls, and
 * the 52 in `Task_GameInit`'s progress bar is a budget, not a measurement.
 *
 * ## Stryker drags three other characters in
 *
 * Step 50 fires when either fighter is character 5 and fills
 * `StrykerCharacters` with `{ 2, 3, 13, -1 }` before preloading them -- Jax,
 * Nightwolf and Liu Kang. They are loaded whether or not they are in the fight,
 * so Stryker costs three extra character loads.
 *
 * ## Cyrax's self-destruct is loaded only when it can happen
 *
 *      if (either fighter is 0, 8 or 11)
 *          CyraxSelfDestructScene = "PURPLEHAZEDEATH.scene"
 *      else
 *          CyraxSelfDestructScene = NULL
 *
 * 0, 8 and 11 are Kano, Cyrax and Sheeva. The scene is Cyrax's, so the other
 * two are on the list as the ones it can be used against.
 *
 * ## The AI flag is bit 7 of the model index
 *
 * Step 52 hands the fight engine its fighters:
 *
 *      AIOn == 0   mk3_init(P1,        P2,        FrameID_GetBBox, 1)
 *      AIOn == 1   mk3_init(P1,        P2 | 0x80, FrameID_GetBBox, 1)
 *      AIOn == 2   mk3_init(P1 | 0x80, P2 | 0x80, FrameID_GetBBox, 1)
 *
 * which is exactly the three states `ShowDebugInfo` prints as "Human vs Human",
 * "Human vs CPU" and "CPU vs CPU". **The engine is told a fighter is CPU by
 * setting bit 7 of its character index**, so a model index is seven bits wide
 * and the roster's 26 entries have plenty of room.
 *
 * Then `mk3_set_four_button(side, buttons != 6)` for each side, from
 * `Settings[4]`: the fight engine only distinguishes six-button from
 * everything-else.
 *
 * ## One number worth checking
 *
 *      Camera[0] = TestScale * 0.0;
 *      Camera[1] = TestScale * -1200.0f;
 *      Camera[2] = TestScale * 146.0f;
 *
 * The declaration of `Camera` further up this file records an observed
 * `(0.0, -600.0, 146.0)`. That would need `TestScale` to be 0.5 for the Y and
 * 1.0 for the Z, and it cannot be both. This is the write that runs at fight
 * start; whichever of the two is wrong, they disagree.
 */
#define GILAB_CHARACTERS 24

/* The two per-character stage-death tables. The original writes each of these
 * out as a 24-case switch with one `LIME_LoadSceneWithTextures` per case, twice
 * over for the two player slots -- roughly 6 KB of the function's 11.7 KB. The
 * table is the fact; the switch is how it was typed. */
static const char *const SubwayDeathScene[GILAB_CHARACTERS] = {
    "SUBWAY_KANO.scene",       "SUBWAY_SONYA.scene",   "SUBWAY_JAX.scene",
    "SUBWAY_NIGHTWOLF.scene",  "SUBWAY_SUBZERO.scene", "SUBWAY_STRYKER.scene",
    "SUBWAY_SINDEL.scene",     "SUBWAY_SEKTOR.scene",  "SUBWAY_CYRAX.scene",
    "SUBWAY_KUNGLAO.scene",    "SUBWAY_KABAL.scene",   "SUBWAY_SHEEVA.scene",
    "SUBWAY_SHANGTSUNG.scene", "SUBWAY_LIUKANG.scene", "SUBWAY_SMOKE.scene",
    "SUBWAY_KITANA.scene",     "SUBWAY_JADE.scene",    "SUBWAY_MILEENA.scene",
    "SUBWAY_SCORPION.scene",   "SUBWAY_REPTILE.scene", "SUBWAY_ERMAC.scene",
    "SUBWAY_OLDSUBZERO.scene", "SUBWAY_OLDSMOKE.scene","SUBWAY_NOOBSAIBOT.scene"
};

static const char *const LairDeathScene[GILAB_CHARACTERS] = {
    "LAIR_KANO.scene",       "LAIR_SONYA.scene",   "LAIR_JAX.scene",
    "LAIR_NIGHTWOLF.scene",  "LAIR_SUBZERO.scene", "LAIR_STRYKER.scene",
    "LAIR_SINDEL.scene",     "LAIR_SEKTOR.scene",  "LAIR_CYRAX.scene",
    "LAIR_KUNGLAO.scene",    "LAIR_KABAL.scene",   "LAIR_SHEEVA.scene",
    "LAIR_SHANGTSUNG.scene", "LAIR_LIUKANG.scene", "LAIR_SMOKE.scene",
    "LAIR_KITANA.scene",     "LAIR_JADE.scene",    "LAIR_MILEENA.scene",
    "LAIR_SCORPION.scene",   "LAIR_REPTILE.scene", "LAIR_ERMAC.scene",
    "LAIR_OLDSUBZERO.scene", "LAIR_OLDSMOKE.scene","LAIR_NOOBSAIBOT.scene"
};

/* Ten fatality HUD frames, one per step. */
static const char *const FatalHudFrame[10] = {
    "FATAL_HUDGFX_0.PNG", "FATAL_HUDGFX_1.PNG", "FATAL_HUDGFX_2.PNG",
    "FATAL_HUDGFX_3.PNG", "FATAL_HUDGFX_4.PNG", "FATAL_HUDGFX_5.PNG",
    "FATAL_HUDGFX_6.PNG", "FATAL_HUDGFX_7.PNG", "FATAL_HUDGFX_8.PNG",
    "FATAL_HUDGFX_9.PNG"
};

extern void  *HUDFatalsTexture[10];
extern void  *DangerTPage;
extern void  *CoinTPage;
extern void  *NewBloodTexture;          /* 0x001f4490 */
extern void  *NewGreenBloodTexture;     /* 0x001f4494 */
extern void  *NewBlackBloodTexture;
extern void  *SmokeStarFieldTexture;    /* 0x001ab668 */
extern void  *SmokeEarthTexture;        /* 0x001ab66c */
extern void  *SmokeExplosionTexture;    /* 0x001ab670 */
extern void  *PauseTexture;
extern void  *InfoTexture;
extern void  *CancelTexture;
extern void  *HUDTPage;
extern void  *PauseBGTexture;
extern void  *SZEffectScene;
extern void  *SwatEffectScene;
extern void  *BloodScene;
extern void  *XeroxScene;
extern void  *PitDeathScene;            /* 0x001aba50 */
extern void  *RocksScene;
extern void  *TrainScene;
extern void  *SKEffectScene;
extern void  *TrainDie1Scene;           /* 0x001aba6c */
extern void  *TrainDie2Scene;           /* 0x001aba70 */
extern void  *SLDie1Scene;
extern void  *SLDie2Scene;
extern void  *CyraxSelfDestructScene;   /* 0x001aba54 */
extern void  *MeshSet_FIGHT;            /* pointer slot -> 0x000f3824 */
extern void  *Scene_FIGHT;              /* pointer slot -> 0x000f381c */
extern long   JaxBeingSquashed;         /* 0x0010dedc */
extern long   IsEndurance;              /* 0x0014fcb0 */
extern long   StrykerCharacters[4];     /* 0x001ab004 */
extern long   DifficultyList[];         /* 0x0014fc00, eleven words a row */
extern float  Player1Pos[3];            /* 0x00150564 */
extern float  Player2Pos[3];            /* 0x00150570 */
extern long   towerFinishedAndLogged;   /* 0x0010de60 */
extern long   Character2Override;       /* 0x00101798 */
extern char   Stats_[];                 /* 0x00183c84, see FrontEnd.c */
extern const char **kodeNames;          /* pointer slot -> 0x001770b0 */
extern void **FEBits1;                  /* pointer slot */
extern void **SmokeTexture;             /* pointer slot */

void  preprocessPreloadKode(void);

/* The slot at 0x000f33f8 holds 0x0001c675 -- FrameID_GetBBox with the Thumb
 * bit set. mk3_init is handed the bounding-box lookup rather than calling it;
 * see decomp/gamecode/training.c, which found the same slot. */
/* The binary loads FrameID_GetBBox's address through the non-lazy pointer
 * 0x000f33f8 (__nl_symbol_ptr, contents 0x0001c675 = _FrameID_GetBBox|1).
 * That slot is the compiler's way to take a function's address, not a
 * variable: modelled as a global it was generated NULL and mk3_init installed
 * a null bbox callback, which ani2_ob (0x584d4) then called. */
#define FrameID_GetBBoxPtr ((void (*)(void))FrameID_GetBBox)

void *LIME_LoadScene(const char *name, long a, long b, long c);
void *LIME_LoadSceneWithTextures(const char *name, long a);
void *LIME_LoadMeshSet(const char *name, long a);
/* The second argument is a STRING, not a word: the body does
 * `sprintf(buf, "%s_DIFF", it)`. Both call sites here pass 0, which the
 * body checks for with `cbz` before using it, so a null suffix is the
 * normal case and the type was never exercised. */
void  LIME_LoadMeshSetTextures(void *ms, const char *suffix);
void  InitParticles(void);
void  InitGameEvents(void);
/* EPLAYER is `typedef int` in Players.c; same reasoning. */
void  LoadLevelCharacters(int p1, int p2);
void  PreloadGameCharacters(long *list, long which);
void  HUDANIM_Init(void);
void  mk3_init_game(void);
void  LoadAllSounds(void);
void  mk3_set_four_button(long side, long four);
/* Returns 0 always; no caller reads it. `flag` is passed as 1 by both callers
 * and is never read inside -- see decomp/gamecode/logic/mk3.c. */
long  mk3_init(long p1model, long p2model, void (*getBBox)(void), long flag);

/* Return 0 to keep loading, non-zero when the last step is done. */
long GameInit_LoadABit(long step)
{
    const char *rec = Level_Info + *LevelSelectP * LEVEL_INFO_STRIDE;
    long i;

    /* Steps 3..12 are ten copies of the same two lines. */
    if (step >= 3 && step <= 12) {
        HUDFatalsTexture[step - 3] =
            limeLoadTexture(FatalHudFrame[step - 3], 0, 0);
        return 0;
    }

    switch (step) {
    case 0:
        if (GameMode != 1)
            return 0;
        checkIfKode();
        if (*theKode > 0)
            EASDK_LogEventEnumEnumString(0x3f6, 15, kodeNames[*theKode], 15, 0);
        preprocessPreloadKode();
        return 0;

    case 1:
        /* In a network game only one end runs the engine's own init. */
        if (GameMode == 1 && !isParentBasedOnSpeed())
            return 0;
        mk3_init_game();
        return 0;

    case 2:
        puts("########## TASK_GAME_INIT: 1");
        RoundParam[0x24 / 4] = 0;
        RoundParam[0x34 / 4] = 1;

        /* Four stages map to a RoundParam value; the rest leave it at 0. The
         * original computes each from the level number it just compared
         * against (`4 - 1`, `8 - 6`, `11 - 7`), which is why they look like
         * arithmetic rather than constants. */
        if (*LevelSelectP == 3)
            RoundParam[0x24 / 4] = 1;
        else if (*LevelSelectP == 4)
            RoundParam[0x24 / 4] = 3;
        else if (*LevelSelectP == 8)
            RoundParam[0x24 / 4] = 2;
        else if (*LevelSelectP == 11)
            RoundParam[0x24 / 4] = 4;

        if (GameMode == 5 && TreasurePlayed == 2)
            *LevelSelectP = 0;

        *FEBits1 = limeLoadTexture("FE_BUTTONS_01.PNG", 0, 0);
        return 0;

    case 13: MoveIconsTexture = limeLoadTexture("MOVES_ICONS.PNG", 0, 0);   return 0;
    case 14: DangerTPage      = limeLoadTexture("DANGER.PNG", 0, 0);        return 0;
    case 15: CoinTPage        = limeLoadTexture("LOGO_COIN_32.PNG", 0, 0);  return 0;

    case 16:
        LoadBloodTextures();
        return 0;

    case 17:
        NewBloodTexture      = limeLoadTexture("BLOODSPLAT1.PNG", 0, 1);
        NewGreenBloodTexture = limeLoadTexture("GREENBLOODSPLAT1.PNG", 0, 1);
        NewBlackBloodTexture = limeLoadTexture("BLACKBLOODSPLAT1.PNG", 0, 1);
        *SmokeTexture        = limeLoadTexture("SMOKEPARTICLE2.PNG", 0, 1);
        return 0;

    case 18:
        InitParticles();
        InitGameEvents();
        JaxGrowCounter   = 0;
        JaxBeingSquashed = 0;
        puts("########## TASK_GAME_INIT: 2");
        SmokeStarFieldTexture = limeLoadTexture("STARFIELD.PNG", 0, 0);
        SmokeEarthTexture     = limeLoadTexture("EARTH_128.PNG", 0, 0);
        SmokeExplosionTexture = limeLoadTexture("EXPLOSION_0.PNG", 0, 0);
        return 0;

    case 19:
        SpearTexture[0] = limeLoadTexture("SPEAR1.PNG", 0, 0);
        SpearTexture[1] = limeLoadTexture("SPEAR2.PNG", 0, 0);
        return 0;

    case 20:
        SpearTexture[2] = limeLoadTexture("SPEAR3.PNG", 0, 0);
        SpearTexture[3] = limeLoadTexture("SPEAR4.PNG", 0, 0);
        SpearTexture[4] = limeLoadTexture("TRIDENT4.PNG", 0, 0);
        return 0;

    case 21: WhiteTexture  = limeLoadTexture("WHITE.PNG", 0, 0);      return 0;
    case 22: PauseTexture  = limeLoadTexture("PAUSE.PNG", 0, 0);      return 0;
    case 23: InfoTexture   = limeLoadTexture("INFO.PNG", 0, 0);       return 0;
    case 24: CancelTexture = limeLoadTexture("X.PNG", 0, 0);          return 0;
    case 25: HUDTPage      = limeLoadTexture("HUD_TPAGE.PNG", 0, 0);  return 0;

    case 26:
        PauseBGTexture = limeLoadTexture("PAUSEBG.PNG", 0, 0);
        /* Port, not in the binary: from texture quality IPAD up, the iPad
         * build's sheet, which its own GameInit_LoadABit loads here (the
         * menus keep BUTTONS_TPAGE, as in that build's FEInit_LoadABit). */
        ButtonsTPage   = (TEXTURE *)limeLoadTexture(PortTextureQuality >= 1
                                                    ? "BUTTONS_TPAGE_IPAD.PNG"
                                                    : "BUTTONS_TPAGE.PNG", 0, 0);
        return 0;

    case 27: {
        const char *name;

        BGSceneHandle2       = 0;
        BGSceneHandle        = 0;
        BGSceneFrame[0]      = 0.0f;
        BGSceneFrame[1]      = 0.0f;
        BGSceneController[2] = -1;
        BGSceneController[5] = -1;

        name = *(const char *const *)(rec + 0x1c);
        if (name == 0)
            return 0;

        BGSceneHandle = LIME_LoadScene(name, 0, 0, 0);
        if (BGSceneHandle != 0)
            LIME_LoadMeshSetTextures(((void **)BGSceneHandle)[0x80 / 4], NULL);
        BGSceneLoops[0] = *(const long *)(rec + 0x20);
        return 0;
    }

    case 28: {
        const char *name = *(const char *const *)(rec + 0x50);

        if (name == 0)
            return 0;

        BGSceneHandle2 = LIME_LoadScene(name, 0, 0, 0);
        if (BGSceneHandle2 != 0)
            LIME_LoadMeshSetTextures(((void **)BGSceneHandle2)[0x80 / 4], NULL);
        BGSceneLoops[1] = *(const long *)(rec + 0x54);
        return 0;
    }

    case 29:
        /* All eight layer textures in one step, empty names skipped. */
        for (i = 0; i < 8; i++) {
            const char *name = ((const char *const *)(rec + 0x94))[i];

            if (name[0] != 0)
                LevelBGTexture[i] = limeLoadTexture(name, 0, 0);
        }
        return 0;

    case 30:
        for (i = 0; i < 8; i++) {
            const char *name = ((const char *const *)(rec + 0x74))[i];

            if (name[0] != 0)
                *MeshSetLayers[i] = LIME_LoadMeshSet(name, 0);
        }
        return 0;

    case 31: MeshSet_FIGHT = LIME_LoadMeshSet("FIGHT.meshset", 0);    return 0;
    case 32: Scene_FIGHT   = LIME_LoadScene("FIGHT.scene", 0, 0, 0);  return 0;

    case 33:
        puts("########## TASK_GAME_INIT: 3");
        SZEffectScene = LIME_LoadSceneWithTextures("SHOCKWAVE2.scene", 1);
        return 0;

    case 34: SwatEffectScene = LIME_LoadSceneWithTextures("JAXZAP8.scene", 1);             return 0;
    case 35: BloodScene      = LIME_LoadSceneWithTextures("GENERICBLOODEXPLODE.scene", 1); return 0;
    case 36: XeroxScene      = LIME_LoadSceneWithTextures("XEROX.scene", 1);               return 0;
    case 37: PitDeathScene   = LIME_LoadSceneWithTextures("GENERICBODYEXPLODE.scene", 1);  return 0;
    case 38: RocksScene      = LIME_LoadSceneWithTextures("ROCKFALL.scene", 1);            return 0;
    case 39: TrainScene      = LIME_LoadSceneWithTextures("SUBWAY_TRAIN.scene", 1);        return 0;
    case 40: SmashThruScene  = LIME_LoadSceneWithTextures("ROCKBREAKTHROUGH.scene", 1);    return 0;

    case 41:
        if (IsEndurance != 0)
            return 0;
        SKEffectScene = LIME_LoadSceneWithTextures("SK_ENDING.scene", 0);
        return 0;

    case 42:
        heartbeatSetIncoming(3);
        heartbeatUpdate();

        if (Destiny == -1)
            Destiny = Destiny + 1;      /* -1 becomes 0, written as an add */

        LastDestiny  = Destiny;
        PLAYER1MODEL = Character1;

        /* The per-character play counter -- and in a network game only the
         * host counts it, so the two ends do not both credit the same fight. */
        if (GameMode != 1 || isParent())
            ((long *)Stats_)[PLAYER1MODEL + 15]++;

        if (GameMode == 0) {
            long who = ((const long *)OpponentTowerList)[Destiny * 11 + Stage];

            Character2   = who;
            PLAYER2MODEL = who;
        }

        if (Character2Override != -1) {
            Character2   = Character2Override;
            PLAYER2MODEL = Character2Override;
        }
        PLAYER2MODEL = Character2;

        HUDANIM_Init();

        if (PLAYER1MODEL == 8  || PLAYER2MODEL == 8
            || PLAYER1MODEL == 0  || PLAYER2MODEL == 0
            || PLAYER1MODEL == 11 || PLAYER2MODEL == 11)
            CyraxSelfDestructScene =
                LIME_LoadSceneWithTextures("PURPLEHAZEDEATH.scene", 0);
        else
            CyraxSelfDestructScene = 0;
        return 0;

    case 43:
        puts("########## TASK_GAME_INIT: 4");
        P2Controls = 0;
        AIOn       = 1;

        if (GameMode == 6) {                    /* two players, one device */
            P2Controls  = 1;
            AIOn        = 0;
            IsEndurance = 0;
        } else if (GameMode == 2) {             /* training */
            AIOn        = 0;
            IsEndurance = 0;
        } else {
            IsEndurance = 0;
            if (GameMode == 0) {
                IsEndurance = EnduranceTowerList[Destiny * 11 + Stage];
                if (IsEndurance != 0) {
                    /* Four independent draws, each masked to 0..3: the two
                     * endurance opponents and their alternates. */
                    girlrand  = limeRand() & 3;
                    girlrand2 = limeRand() & 3;
                    boyrand   = limeRand() & 3;
                    boyrand2  = limeRand() & 3;
                    InitEnduranceMatch();
                    return 0;
                }
            }
        }

        ((char *)RoundParam)[0x18] = (char)-1;  /* a BYTE store */
        RoundParam[0x3c / 4] = 0;

        /* The last stage of a tower sets the flag and still returns 0 -- the
         * last STAGE is not the last STEP. */
        if (GameMode == 0 && Destiny + 7 <= Stage)
            RoundParam[0x3c / 4] = 1;
        return 0;

    case 44:                                    /* the lair, player 1 */
        puts("########## TASK_GAME_INIT: 5");
        if (*LevelSelectP != 11)
            return 0;
        SLDie1Scene =
            LIME_LoadSceneWithTextures(LairDeathScene[Character1], 0);
        return 0;

    case 45:                                    /* the lair, player 2 */
        if (*LevelSelectP != 11)
            return 0;
        SLDie2Scene = LIME_LoadSceneWithTextures(
            LairDeathScene[IsEndurance ? LastEnduranceCharacter : Character2],
            0);
        return 0;

    case 46:                                    /* the subway, player 1 */
        if (*LevelSelectP != 4)
            return 0;
        TrainDie1Scene =
            LIME_LoadSceneWithTextures(SubwayDeathScene[Character1], 0);
        return 0;

    case 47:                                    /* the subway, player 2 */
        puts("########## TASK_GAME_INIT: 6");
        if (*LevelSelectP != 4)
            return 0;
        TrainDie2Scene = LIME_LoadSceneWithTextures(
            SubwayDeathScene[IsEndurance ? LastEnduranceCharacter : Character2],
            0);
        return 0;

    case 48:
        puts("########## TASK_GAME_INIT: 7");
        LoadLevelCharacters(PLAYER1MODEL, PLAYER2MODEL);
        return 0;

    case 49:
        puts("########## TASK_GAME_INIT: 7.05");
        if (IsEndurance == 0)
            return 0;
        PreloadGameCharacters(EnduranceCharacters, 1);
        return 0;

    case 50:
        puts("########## TASK_GAME_INIT: 7.1");
        if (PLAYER1MODEL != 5 && PLAYER2MODEL != 5)
            return 0;
        /* Stryker's moves reference Jax, Nightwolf and Liu Kang. */
        StrykerCharacters[0] = 2;
        StrykerCharacters[1] = 3;
        StrykerCharacters[2] = 13;
        StrykerCharacters[3] = -1;
        PreloadGameCharacters(StrykerCharacters, 0);
        return 0;

    case 51:
        puts("########## TASK_GAME_INIT: 7.2");
        CurrentScene = 0;

        RoundParam[0] = (long)*(const float *)(rec + 0x24);
        RoundParam[1] = (long)*(const float *)(rec + 0x28);
        SceneX     = *(const float *)(rec + 0x0c);
        SceneY     = *(const float *)(rec + 0x10);
        SceneZ     = *(const float *)(rec + 0x14);
        SceneScale = *(const float *)(rec + 0x18);

        InitGroundOffset  = 0;
        SceneGroundOffset = 0.0f;
        ShadowOffset      = 0.0f;

        puts("########## TASK_GAME_INIT: 7.3");

        /* Two scene filenames is the whole switch for the second layer. */
        if (*(const long *)(rec + 0x1c) != 0
            && *(const long *)(rec + 0x50) != 0) {
            blast_state = 0;
            SceneX2     = *(const float *)(rec + 0x40);
            SceneY2     = *(const float *)(rec + 0x44);
            SceneZ2     = *(const float *)(rec + 0x48);
            SceneScale2 = *(const float *)(rec + 0x4c);

            CurrentScene = 1;
            ((char *)RoundParam)[0x30] = 1;     /* a BYTE store */
            RoundParam[0x08 / 4] = 0x320;
            InitGroundOffset     = 0x320;       /* the same 800, as an int */

            SceneGroundOffset = -800.0f / WorldScaleAdjust;
            ShadowOffset      = -800.0f / WorldScaleAdjust;

            RoundParam[0] = (long)*(const float *)(rec + 0x58);
            RoundParam[1] = (long)*(const float *)(rec + 0x5c);
        }

        puts("########## TASK_GAME_INIT: 8");

        RoundParam[0x10 / 4] = 1;
        RoundParam[0x14 / 4] = Destiny;
        RoundParam[0x0c / 4] = DifficultyList[Destiny * 11 + Stage];

        if (GameMode == 4) {                    /* survival ramps its own */
            long d = survivalWinStreak / 4;

            RoundParam[0x0c / 4] = d > 9 ? 9 : d;
            RoundParam[0x14 / 4] = 0;
        } else if (GameMode == 3) {             /* karnage */
            /* 0x2db1c `subs r3, #3` on the register still holding GameMode
             * (just compared with 3): both are 0. This read Destiny - 3,
             * which is -4 outside a ladder -- a negative difficulty and
             * tower index for the whole Karnage fight. */
            RoundParam[0x0c / 4] = GameMode - 3;
            RoundParam[0x14 / 4] = GameMode - 3;
        }

        Player1Pos[0] = 1.36f;
        Player1Pos[1] = -0.44f;
        Player1Pos[2] = -0.00062f;
        Player2Pos[0] = 4.714375f;
        Player2Pos[1] = -0.44f;
        Player2Pos[2] = -0.0125f;

        IsInFinishing    = 0;
        CamTrackToPlayer = -1;
        RoundParam[0x38 / 4] = 0;
        RoundParam[0x34 / 4] = 1;
        return 0;

    case 52:
        LoadAllSounds();

        if (GameMode == 1 && !isParentBasedOnSpeed()) {
            puts("INITIALIZING MP SPRITELIST AND EVENT QUEUE");
            clearSpriteListsAndEvents();
            dumpMem(mpSpriteList, 0x140, 0x20);
            dumpMem(mpEventQueue, 0x1b0, 0x20);
        }

        if (isParentBasedOnSpeed())
            AIOn = 0;

        CamTrackToPlayer = -1;
        IsInFinishing    = 0;
        RoundParam[0x34 / 4] = 1;
        RoundParam[0x38 / 4] = 0;

        /* Bit 7 of a model index is "this side is the CPU". */
        if (AIOn == 2)
            mk3_init(PLAYER1MODEL | 0x80, PLAYER2MODEL | 0x80,
                     FrameID_GetBBoxPtr, 1);
        else if (AIOn == 1)
            mk3_init(PLAYER1MODEL, PLAYER2MODEL | 0x80,
                     FrameID_GetBBoxPtr, 1);
        else
            mk3_init(PLAYER1MODEL, PLAYER2MODEL, FrameID_GetBBoxPtr, 1);

        Player1NumButtons = Settings[4];
        mk3_set_four_button(0, Player1NumButtons != 6);
        mk3_set_four_button(1, Player2NumButtons != 6);

        puts("########## TASK_GAME_INIT: 9");

        Camera[0] = TestScale * 0.0f;
        Camera[1] = TestScale * -1200.0f;
        Camera[2] = TestScale * 146.0f;

        LIME_KillAllEvents();
        towerFinishedAndLogged = 0;
        return 1;                       /* the only non-zero return */

    default:
        return 0;
    }
}


/* ------------------------------------------------------------------- MovesList
 *
 * armv7 0x0001ed4c, 8,796 bytes.  **Complete**, with one qualification recorded
 * at the end of this comment.
 *
 * The in-game Moves Info screen: ten pages of "notation on the left, move name
 * on the right", with a button at the bottom that advances the page.
 *
 * The tables it displays are static data, documented in
 * [MOVES-TABLES.md](../../docs/MOVES-TABLES.md) and readable with
 * `tools/moves.py`. This function is how they are sliced and drawn.
 *
 * ### Five pages, or ten in two-player
 *
 *      if (DrawButtonNew(&BUTTON_BACK, 0x1a7, 0x130, 1)) {
 *          MoveListPage++;
 *          MoveListPage %= (GameMode == 6) ? 10 : 5;
 *      }
 *
 * Pages 0-4 are player 1's list and 5-9 player 2's, so the second half is only
 * reachable in mode 6 -- two players on one device. Both moduli are reciprocal
 * multiplies in the original.
 *
 * The button asset is `BUTTON_BACK` and it moves **forward**. There is no way
 * back except all the way round.
 *
 * ### What each page shows
 *
 *      0, 5   Generic moves, rows 0-5      names GameTextNoHeader(row + 0x124)
 *      1, 6   Generic moves, rows 6-11     the same base
 *      2, 7   Generic moves, rows 12-16    the same base
 *      3, 8   the character's section 1    from MovesListTab
 *      4, 9   the character's section 2    from MovesListTab
 *
 * The shared moves are three pages of six -- `Generic_Moves` is seventeen rows,
 * which is why the third is short -- and the character's own are two.
 *
 * **`MovesListTab`'s section 3 is never displayed.** Fourteen of the twenty-
 * three characters have one (Sonya's is seven rows) and none of the ten pages
 * reads words 10, 11 or 12 of the entry. Whatever section 3 is for, it is not
 * this menu, and `tools/moves.py` prints it because the data is there.
 *
 * ### The two id ranges are the name and the qualifier
 *
 * Each `MovesListTab` section carries two `GameText` bases, and this function
 * settles what they are:
 *
 *      GameTextNoHeader(idA + row)   drawn as text -- the move's NAME
 *      GameTextNoHeader(idB + row)   handed to DrawMoveListIcons as its caption
 *
 * and `DrawMoveListIcons` wraps a caption as `"(%s)  "`. So idB is the
 * parenthesised qualifier that follows a move name. One range per column.
 *
 * ### The row is drawn as a stripe, and the anchor alternates
 *
 *      limeFillRect(0, (y + 0x2f) * FE_HeightScale,
 *                   limeScreenWidth, 32.0f * FE_HeightScale,
 *                   shade, shade, shade, 0.7f)
 *
 * with `shade` **0.0 on rows 0, 2, 4 and 0.15 on rows 1, 3, 5** -- both bars are drawn, at
 * the same height and the same 0.7 alpha, and the alternation is the only
 * difference. That is the banding behind the list.
 *
 * **The whole row alternates sides.** `(row + 1) & 1` chooses between two
 * copies of the row: rows 0, 2, 4 draw the name right-aligned at `FE_X(432)`
 * and anchor the notation to the right edge; rows 1, 3, 5 draw the name
 * left-aligned at `FE_X(48)` and the notation from the left. Name and icons
 * always share a side -- the list zig-zags.
 *
 * ### The y counter doubles as the row offset
 *
 *      seq = table + (y << 1)
 *
 * `y` steps by 0x20 a row, so `y << 1` steps by 0x40 -- exactly the 64-byte row
 * stride. One counter, two uses, no multiply. Worth knowing before anyone
 * tidies the loop.
 *
 * ### The guest shows the other player's moves
 *
 *      if (GameMode == 1 && !isParentBasedOnSpeed())
 *          character = PLAYER2MODEL;
 *
 * On the machine that is not the speed-elected parent the local player *is*
 * player 2, so that is the list it shows.
 *
 * ### Ten unrolled copies, and what was read
 *
 * The ten pages are ten separate blocks of 690 to 830 bytes, differing only in
 * the starting row, the section and which player. That duplication is most of
 * the 8,796 bytes.
 *
 * **Pages 0 and 3 were read instruction by instruction**; the other eight were
 * established from their distinguishing constants -- the starting row, the
 * table symbol, the id base and the header id -- which is what differs. The
 * shared row-drawing below is page 0's and page 3's, verified twice.
 */
#define MOVESLIST_ROWS_PER_PAGE 6
#define MOVESLIST_GENERIC_ROWS  17
#define MOVESLIST_GENERIC_NAMES 0x124

extern const char *CharacterNames[];    /* pointer slot -> 0x0014fe54 */
extern char *strBuf;                    /* pointer slot -> _str, 0x001f3cac */

/* The BUTTONNEW layout is established in FrontEnd.c, at DrawButtonNew. */
typedef struct BUTTONNEW BUTTONNEW;
extern BUTTONNEW BUTTON_BACK;           /* 0x001007bc */
long DrawButtonNew(BUTTONNEW *b, int x, int y, int interactive);
void PortFERightEdgeSet(int on);

extern long  MoveListPage;              /* 0x00150eb4 */
extern long  Generic_Moves5[];          /* 0x001030d8 */
extern long  Generic_Moves6[];          /* 0x00103518 */

/* One of a character's three sections: how many rows, the two GameText bases,
 * and the table for each button layout. Section 3 has no id bases, which is why
 * the entry is 13 words and not 15. */
typedef struct {
    long        rows;
    long        nameId;                 /* GameText base A */
    long        captionId;              /* GameText base B */
    const long *moves5;
    const long *moves6;
} MOVESSECTION;

typedef struct {
    MOVESSECTION section[2];
    long         rows3;                 /* section 3 -- never displayed */
    const long  *moves5_3;
    const long  *moves6_3;
} MOVESLISTENTRY;

extern MOVESLISTENTRY MovesListTab[];   /* 0x0010d918, thirteen words each */

int asciiToUnicode(const char *src, char *dst, long len);
void DrawMoveListIcons(int y, const int *seq, const char *caption,
                       int rightAlign);

/* One row: a banded background, the move's name right-aligned, and its notation
 * drawn by DrawMoveListIcons. `y` is the row's top in design units, stepping by
 * 0x20; `row` indexes the id ranges. `seq` is the row's notation and `withCaption`
 * says whether the row has a caption at all -- the generic pages pass NULL
 * (0x1efb4: `movs r2, #0`), the character pages their own string. */
static void DrawMovesRow(long y, long row, const int *seq,
                         long nameId, long captionId, int withCaption)
{
    char  caption[128];                 /* sp+0x70 */
    /* fp = (row + 1) & 1 (0x1efe6) picks the whole row's side. fp != 0
     * (0x1eed2): stripe 0.0, name right-aligned at FE_X(432) (0x43d80000),
     * notation right-anchored. fp == 0 (0x1efee): stripe 0.15 (0x3e19999a),
     * name LEFT-aligned at FE_X(48) (0x42400000, alignment = fp), notation
     * from the left. The name used to stay right-aligned on every row and
     * the stripe shades were swapped, so every other row's icons sat at the
     * far side of the screen from their move. */
    long  right = (row + 1) & 1;
    float shade = right ? 0.0f : 0.15f;

    limeFillRect(0.0f, (float)(y + 0x2f) * FE_HeightScale,
                 (float)limeScreenWidth, 32.0f * FE_HeightScale,
                 shade, shade, shade, 0.7f);

    limeDrawFONT(&GameFont, limeUC(GameTextNoHeader(nameId + row)),
                 (float)FE_X(right ? 432.0f : 48.0f),
                 (float)(y + 0x30) * FE_HeightScale,
                 right ? 2 : 0, 0.75f * FE_WidthScale, fontcol);

    if (withCaption)
        usprintf(caption, UC("%s"), GameTextNoHeader(captionId + row));

    DrawMoveListIcons((int)((float)(y + 0x40) * FE_HeightScale), seq,
                      withCaption ? caption : 0, (int)right);
}

/* One page of the shared list; `first` is the row it starts at. */
static void DrawGenericPage(long first)
{
    const long *table = (Player1NumButtons == 5) ? Generic_Moves5
                                                 : Generic_Moves6;
    long row;

    for (row = 0; row < MOVESLIST_ROWS_PER_PAGE; row++) {
        if (first + row >= MOVESLIST_GENERIC_ROWS)
            break;
        /* the absolute row, `lsl r1, sl, #6` -- 64 bytes a row (0x1efb0) */
        DrawMovesRow(row * 0x20, first + row,
                     (const int *)((const char *)table + (first + row) * 0x40),
                     MOVESLIST_GENERIC_NAMES, MOVESLIST_GENERIC_NAMES, 0);
    }
}

/* One page of a character's own list. `section` is 0 or 1; section 2 exists in
 * the table and is never shown here. */
static void DrawCharacterPage(long character, long section)
{
    const MOVESSECTION *s = &MovesListTab[character].section[section];
    const long *table = (Player1NumButtons == 5) ? s->moves5 : s->moves6;
    long row;

    for (row = 0; row < s->rows; row++)
        /* `y << 1` is the 64-byte row stride (0x1f36e) */
        DrawMovesRow(row * 0x20, row,
                     (const int *)((const char *)table + ((row * 0x20) << 1)),
                     s->nameId, s->captionId, 1);
}

void MovesList(void)
{
    char title[128];                    /* sp+0x170 */
    long character = PLAYER1MODEL;
    long page;
    long header;

    /* On the guest of a network game the local player is player 2. */
    if (GameMode == 1 && !isParentBasedOnSpeed())
        character = PLAYER2MODEL;

    page   = MoveListPage;
    header = (page == 3 || page == 4 || page == 8 || page == 9) ? 0xef : 0xee;

    asciiToUnicode(CharacterNames[character], title, 0x80);
    usprintf(strBuf, UC("%s - %s"), limeUC(title), GameTextNoHeader(header));
    limeDrawFONT(&GameFont, strBuf, (float)(limeScreenWidth / 2),
                 8.0f * FE_HeightScale, 1, FE_WidthScale, fontcol);

    switch (page) {
    case 0: case 5: DrawGenericPage(0);              break;
    case 1: case 6: DrawGenericPage(6);              break;
    case 2: case 7: DrawGenericPage(12);             break;
    case 3: case 8: DrawCharacterPage(character, 0); break;
    case 4: case 9: DrawCharacterPage(character, 1); break;
    default: break;
    }

    /* Port, not in the binary: on 16:9 and 21:9 NEXT and its label sit at
     * the screen's right edge, as on the front end's screens */
    PortFERightEdgeSet(1);
    if (DrawButtonNew(&BUTTON_BACK, 0x1a7, 0x130, 1)) {
        MoveListPage = MoveListPage + 1;
        MoveListPage %= (GameMode == 6) ? 10 : 5;
    }

    limeDrawFONT(&GameFont, GameText(8), (float)FE_X(423.0f),
                 (float)FE_Y(296.0f), 1, FE_WidthScale, fontcol);
    PortFERightEdgeSet(0);
}


/* `C.175` in DrawHUD, 0x000de07c: white, alpha replaced per frame. */
static const float DrawHUD_PulseColour[4] = { 1.0f, 1.0f, 1.0f, 0.0f };

/* --------------------------------------------------------------------- DrawHUD
 *
 * armv7 0x000282dc, 11,536 bytes.  **Complete.**
 *
 * The largest function in `gamecode`, and its name is a lie: `DrawHUD` runs the
 * **round and match state machine**. Deciding who won the round, awarding the
 * flawless, starting the next round, loading the next endurance opponent and
 * ending the match all happen here, in among the health bars and the combo
 * counter. `QuitAsWin`, `QuitAsLose`, `ResetFightData`, `mk3_init`,
 * `InitEnduranceMatch` and `LoadGameCharacterCheckCache` are all called from
 * inside it.
 *
 * Anything porting the fight loop has to know that. This is not a call you can
 * reorder, skip a frame, or move onto a render thread.
 *
 * The original is one flat function: the two player halves, the two combo
 * counters and the two round-end halves are each written out twice in full,
 * which is most of the 11,536 bytes. They are folded into helpers here. Every
 * constant is the original's, and where the two copies differ the difference is
 * called out.
 *
 * The block layout is heavily reordered -- the `return` sits at 0x00028f12 with
 * a dozen cold blocks after it that jump back into the body -- so the control
 * flow below comes from tracing branches, not from reading the range in order.
 *
 * ### The game modes, finally pinned down
 *
 * Six modes disambiguate each other across this one function:
 *
 *      0   Arcade / tower            `winStreak`, the tower completion logging
 *      1   Network                   `isParentBasedOnSpeed`, `updateMPWins`
 *      2   Training                  the only caller of `TrainingMessages`
 *      3   Karnage                   draws `KarnageScore`, and is the one mode
 *                                    that does NOT draw player 2's plate
 *      4   Survival                  `survivalWinStreak`
 *      6   Two players, one device   logged as the literal "2 Players on 1 iPad"
 *
 * Mode 5 never appears here.
 *
 * ### The bars are the depleted part, drawn with a negative width
 *
 * `limeDrawSprite`'s last four floats are UV **extents**, not corners: the
 * plate is 192x28 at 0.75 x 0.109375, and 192/256 and 28/256 are exactly those.
 * The atlas is 256x256. Player 1, all times `HUD_Scale`:
 *
 *      plate    x 18   y 24   w  192   h 28   uv (0, 0)         ext (.75, .109375)
 *      health   x 209  y 24   w -190*  h 20   uv (1/256, .125)  ext (1/256, 20/256)
 *      run bar  x 81   y 44   w  -62*  h  7   uv (.125, .125)   ext (1/256, 7/256)
 *
 *      * scaled by (100 - Health) / 100 and (100 - RunBar) / 100
 *
 * The two bar widths are **negative**, so each rectangle runs leftward from its
 * x. They do not draw the bar -- the full bar is painted into the plate -- they
 * draw the *depleted* part over it, growing leftward from the right end. 209 is
 * the plate's right edge (18 + 192 = 210), 190 is very nearly the bar's full
 * width, and the source is one pixel of UV width stretched across the
 * rectangle. Health sits in the plate's top 20 rows and the run meter in the
 * 7 below it, which is the whole 28.
 *
 * Player 2 is the mirror: every x becomes `limeScreenWidth - x`, and the widths
 * come out **positive** so they grow rightward instead. The name flips from
 * left- to right-aligned. The coins keep a positive width and move their left
 * edge, so 18 and 50 become -50 and -82 -- the same 32-pixel pitch measured
 * from the other side.
 *
 * ### The combo counter slides in, and retracts in two stages
 *
 *      x = (128 - ComboSlider1[p]) * HUD_Scale                     player 1
 *      x = limeScreenWidth - (128 - ComboSlider1[p]) * HUD_Scale    player 2
 *      y = 108 * HUD_Scale   hit count,  GameTextNoHeader(0xb5)
 *      y = 124 * HUD_Scale   damage,     GameTextNoHeader(0x11c)
 *
 * `ComboTimer`, `ComboNumber`, `ComboDamage`, `ComboSlider1` and `ComboSlider2`
 * are all **two-element arrays**, one per player -- the second element is only
 * ever reached as `[r3, #4]`, which is why nothing found them before. Above a
 * timer of 60 both sliders run on together at 8 a frame; below it they retract,
 * and `ComboSlider2` does not start until `ComboSlider1` is past 64. That
 * stagger is the damage line trailing the hit line off the screen.
 *
 * ### Round end
 *
 * Two symmetric halves, each guarded by the same `RoundSummary == 0`, so only
 * the first to fire in a frame counts:
 *
 *      Health[0] == 0  ->  player 2 takes the round
 *      Health[1] == 0  ->  player 1 takes the round
 *
 * and a third path when the clock runs out, where the higher health wins and an
 * exact tie gives the round to nobody. A timeout win at 100 health keeps the
 * flawless streak; any other win below 100 clears it.
 *
 * Reaching `WinsNeeded` calls `updateMPWins()` and stops there. The match end
 * itself is driven later, from the summary timer.
 *
 * ### Three kodes are implemented here
 *
 * - **`theKode == 1` is Blackout.** `lightsOn` counts down a frame at a time and
 *   at zero the screen is filled solid black -- *unless* `IsInFinishing`, so the
 *   lights come back up for the fatality. That exception is the whole point of
 *   the kode.
 * - **`theKode == 0x13` ends the match in one round.** In a network game the
 *   loser's `RoundWins` is set to `WinsNeeded` and the winner's to
 *   `WinsNeeded - 1`, so the very next test ends the match. Both halves do it,
 *   and the coin display reads the other player's counter to match.
 * - **`theKode == 0x11`** suppresses both plates: the bars, both names and the
 *   clock. The coins still draw.
 *
 * ### Achievements, and what earns them
 *
 *      2     a flawless round                    modes 0 and 4
 *      3     five flawless victories             modes 0 and 4
 *      6     a combo above four hits             modes 0 and 4
 *      0xe   beating Motaro (PLAYER2MODEL 24)
 *      0x10  beating Shao Kahn (PLAYER2MODEL 25)
 *      0x13  a survival streak above 19
 *
 * The two boss indices land exactly where [ROSTER.md](../../docs/ROSTER.md) put
 * them, reached from a direction that table did not use.
 *
 * ### Four stats counters, named
 *
 *      Stats[6]    flawless victories
 *      Stats[12]   accumulated fight time, as 99 - GameTime a round
 *      Stats[13]   accumulated health kept
 *      Stats[14]   rounds fought
 *
 * In a network game `Stats[13]` takes `Health[isParent() ? 0 : 1]`, because the
 * local player is player 2 on the guest. Everywhere else it is `Health[0]`.
 *
 * ### The round banners are hardcoded English
 *
 * `Round` 0 to 3 select the ASCII literals `"ROUND 1"` through `"ROUND 4"` at
 * 0x00102064, drawn straight through `limeDrawFONT` with no `GameText` lookup.
 * Every other string in this function is translated; these four are not. A
 * localisation gap in the original, and one a port can fix.
 *
 * ### `mk3_init`'s fourth argument is `AIOn`
 *
 * All three call sites here leave `AIOn` in r3 across the switch that chose
 * between them, and reading `GameInit_LoadABit` again shows the identical shape
 * -- r3 is loaded from `AIOn` at 0x0002d974 and never reassigned before any of
 * its three calls. [ROSTER.md](../../docs/ROSTER.md) recorded that argument as
 * a constant `1`; it is not.
 *
 * ### Types corrected here
 *
 * `ScorpionFade`, `ScorpionFadeAdd`, `ScorpionFlash`, `lightsOn`,
 * `RoundSummaryTime` and `FightMessageTimer` were all declared `long` in this
 * file. Every one is loaded with `vldr` and compared against a float literal.
 * They were declared by functions that only ever assign them zero, which
 * assigns the same bits either way -- the same trap that had seven other
 * declarations wrong before a call site settled them.
 */

extern float  limeFPSScaleFactor;       /* pointer slot -> 0x00171acc */
extern long   flawlessVictories;        /* 0x0014e000 */
extern long   Stats[];                    /* pointer slot -> 0x00183c84 */
extern long   RoundHasEnded;            /* 0x0014e248 */
extern long   RoundHasEndedStatsUpdated;/* 0x0014e244 */
extern long   DoingSKDeath;             /* 0x0010deb8 */
extern float  InfoScale;                /* 0x0010de80 */
extern float  InfoScaleAdd;             /* 0x0010de7c */
extern char   fatal_HUDgfx_SpriteDef[]; /* pointer slot -> 0x0017b9dc */
extern long   fatal_HUDgfx_Anim[];      /* pointer slot -> 0x0017c81c; the slot
                                         * holds the ARRAY's address, so this is
                                         * long[], not long* */
extern float  ComboTimer[2];            /* 0x0014e284 */
extern long   ComboNumber[2];           /* 0x0014e27c */
extern long   ComboDamage[2];           /* 0x0014e274 */
extern float  ComboSlider1[2];          /* 0x0014e28c */
extern float  ComboSlider2[2];          /* 0x0014e294 */
extern float  limeLastTouchScreenX[];     /* 0x00171b44, four slots */
extern float  limeTouchScreenY[];         /* 0x00171b1c, four slots */
extern float  FE_FadeAdd;               /* pointer slot -> 0x0010089c */

void  HUDANIM_Render(void);
void  achievementsDraw(void);
void  no_ai_hack(void);
void  updateMPWins(void);
void  InitEnduranceMatch(void);
void  LIME_InitEventsManager(void);
void  DumpAltCostume(char *player);
void  LoadGameCharacterCheckCache(char *player, const PLAYERDEF *def,
                                  long *stats);
void  mk3_set_four_button(long player, long fourButton);
void  DrawControls(void);
void  TrainingMessages(void);
int   sprintf(char *dst, const char *fmt, ...);

#define HUD_ATLAS           256.0f      /* the sheet the UV extents divide by */
#define HUD_PLATE_W         192.0f
#define HUD_PLATE_H          28.0f
#define HUD_COIN             32.0f
#define COMBO_SLIDE         128.0f      /* the counters start this far off-screen */
#define COMBO_HOLD           60.0f      /* above this the counter is still coming on */
#define ROUND_SUMMARY_HOLD  300.0f
#define MERCY_FRAMES         13
#define MERCY_FIRST_FRAME  0x2c

static void RoundSummaryUpdate(void);

/* One player's plate, bars and name. `p` is 0 or 1: player 2 measures every x
 * from `limeScreenWidth` and comes out with positive bar widths, which is the
 * only difference apart from the text alignment. */
static void DrawPlayerPlate(long p)
{
    float s    = HUD_Scale;
    float edge = p ? (float)limeScreenWidth : 0.0f;
    float dir  = p ? -1.0f : 1.0f;
    long  model = p ? PLAYER2MODEL : PLAYER1MODEL;

    limeDrawSprite(HUDTPage, edge + dir * 18.0f * s, 24.0f * s,
                   dir * HUD_PLATE_W * s, HUD_PLATE_H * s,
                   0.0f, 0.0f,
                   HUD_PLATE_W / HUD_ATLAS, HUD_PLATE_H / HUD_ATLAS, col);

    /* Both bars are the DEPLETED part drawn over the plate: a one-pixel UV
     * column stretched across a width that runs back toward the centre. */
    limeDrawSprite(HUDTPage, edge + dir * 209.0f * s, 24.0f * s,
                   dir * (float)(-190 * (100 - Health[p])) / 100.0f * s,
                   20.0f * s,
                   1.0f / HUD_ATLAS, 0.125f,
                   1.0f / HUD_ATLAS, 20.0f / HUD_ATLAS, col);

    limeDrawSprite(HUDTPage, edge + dir * 81.0f * s, 44.0f * s,
                   dir * (float)(-62 * (100 - RunBar[p])) / 100.0f * s,
                   7.0f * s,
                   0.125f, 0.125f,
                   1.0f / HUD_ATLAS, 7.0f / HUD_ATLAS, col);

    limeDrawFONT(&NameFont, CharacterNames[model],
                 edge + dir * 24.0f * s, 27.0f * s,
                 p ? 2 : 0, s, fontcol);
}

/* One coin a round won, 32 apart. The one-round kode reads the other player's
 * counter, because it has just been set to WinsNeeded on the loser. */
static void DrawRoundWinCoins(long p)
{
    float s    = HUD_Scale;
    float edge = p ? (float)limeScreenWidth : 0.0f;
    float dir  = p ? -1.0f : 1.0f;
    long  wins = (GameMode == 1 && *theKode == 0x13) ? RoundWins[p ^ 1]
                                                     : RoundWins[p];

    if (wins > 0)
        limeDrawSprite(CoinTPage, edge + dir * (p ? 50.0f : 18.0f) * s,
                       54.0f * s, HUD_COIN * s, HUD_COIN * s,
                       0.0f, 0.0f, 1.0f, 1.0f, col);
    if (wins > 1)
        limeDrawSprite(CoinTPage, edge + dir * (p ? 82.0f : 50.0f) * s,
                       54.0f * s, HUD_COIN * s, HUD_COIN * s,
                       0.0f, 0.0f, 1.0f, 1.0f, col);
}

/* The hit count and damage lines for one player, and the slider that carries
 * them on and off the screen. Karnage draws no text but still runs the timer. */
static void DrawComboCounter(long p)
{
    float s = HUD_Scale;

    if (ComboTimer[p] <= 0.0f)
        return;

    if (ComboNumber[p] > 4 && (GameMode == 4 || GameMode == 0))
        achievementsUnlock(6);

    if (GameMode != 3) {
        float x1 = (COMBO_SLIDE - ComboSlider1[p]) * s;
        float x2 = (COMBO_SLIDE - ComboSlider2[p]) * s;

        if (p) {
            x1 = (float)limeScreenWidth - x1;
            x2 = (float)limeScreenWidth - x2;
        }

        usprintf(strBuf, UC("%d %s"), ComboNumber[p], GameTextNoHeader(0xb5));
        limeDrawFONT(&GameFont, limeUC(strBuf), x1, 108.0f * s,
                     p ? 0 : 2, s, fontcol);

        usprintf(strBuf, UC("%d %s"), ComboDamage[p], GameTextNoHeader(0x11c));
        limeDrawFONT(&GameFont, limeUC(strBuf), x2, 124.0f * s,
                     p ? 0 : 2, s, fontcol);
    }

    ComboTimer[p] -= 1.0f / limeFPSScaleFactor;

    if (ComboTimer[p] > COMBO_HOLD) {
        ComboSlider1[p] -= 8.0f / limeFPSScaleFactor;
        ComboSlider2[p] -= 8.0f / limeFPSScaleFactor;
        if (ComboSlider1[p] < 0.0f) ComboSlider1[p] = 0.0f;
        if (ComboSlider2[p] < 0.0f) ComboSlider2[p] = 0.0f;
    } else {
        if (ComboTimer[p] <= 0.0f)
            ComboTimer[p] = 0.0f;
        /* the damage line does not follow until the hit line is half out */
        ComboSlider1[p] += 8.0f / limeFPSScaleFactor;
        if (ComboSlider1[p] >= 64.0f)
            ComboSlider2[p] += 8.0f / limeFPSScaleFactor;
    }
}

/* The finisher banners, drawn under the winner's name (0x29d4a..0x2ab26).
 * Missing until now: the four flags were set by Blood.c's events 22, 28, 42
 * and 43 and nothing drew them. One at a time, in the binary's order --
 * babality, animality, fatality, friendship -- each a DrawAnimAsSprite of
 * the HUD finisher sheet at the centre of the screen, half scale:
 *
 *      animality   frame (int)counter, +0.2 a tick, held at 11   (0x2a4fc)
 *      fatality    frame 0x19 + (int)counter, +0.2 a tick, back to 7 once
 *                  it reaches 14 -- the last seven frames loop      (0x2a3d8)
 *      friendship  two sprites at centre -/+ 128, frames 0x27 / 0x14 plus
 *                  C.195[(int)(GameCounter * 0.25) & 7], C.195 being
 *                  0 1 2 3 4 3 2 1                                   (0x2a9c0)
 *
 *      babality    eight sprites, frames 12..19, x = centre - 140 + 40 i,
 *                  each falling and bouncing on its own: BabalityVel[i]
 *                  -= 0.1 a tick (in double), BabalityHeight[i] += vel,
 *                  both over limeFPSScaleFactor; below 0 the height is
 *                  set to 0 and the velocity divided by -1.5. y is the
 *                  centre minus the height (0x29d56..0x29eba). The start
 *                  heights are set where the flag is cleared (288 256 ...). */
extern float GameCounter;               /* 0x0014fa5c */
static const long DrawHUD_C195[8] = { 0, 1, 2, 3, 4, 3, 2, 1 };  /* 0xde05c */

static void FinisherSpriteAt(long x, long y, long frame)
{
    DrawAnimAsSprite(x, y, FE_WidthScale * 0.5f,
                     0x100, 0x100, (long)(uintptr_t)HUDFatalsTexture,
                     fatal_HUDgfx_SpriteDef, fatal_HUDgfx_Anim, 0, frame,
                     0, fatal_HUDgfx_Anim[0] - 1, 1, col);
}

static void FinisherSprite(long x, long frame)
{
    FinisherSpriteAt(x, limeScreenHeight / 2, frame);
}

static void DrawFinisherBanner(void)
{
    if (BabalityMessage) {
        long i;

        for (i = 0; i < 8; i++) {
            float vel, h, x;

            vel = (float)((double)BabalityVel[i]
                          + -0.1 / (double)limeFPSScaleFactor);
            BabalityVel[i] = vel;
            h = BabalityHeight[i] + vel / limeFPSScaleFactor;
            BabalityHeight[i] = h;
            if (h < 0.0f) {             /* the bounce */
                h = 0.0f;
                BabalityHeight[i] = 0.0f;
                BabalityVel[i] = vel / -1.5f;
            }
            x = (float)(limeScreenWidth / 2) + FE_WidthScale * -140.0f;
            x = x + FE_WidthScale * (float)(i * 40);
            FinisherSpriteAt((long)x,
                             (long)((float)(limeScreenHeight / 2)
                                    - h * FE_HeightScale),
                             12 + i);
        }
        return;
    }

    if (AnimalityMessage) {
        AnimalityMessageCounter = (float)((double)AnimalityMessageCounter
                                  + 0.2 / (double)limeFPSScaleFactor);
        if (AnimalityMessageCounter > 11.0f)
            AnimalityMessageCounter = 11.0f;
        FinisherSprite(limeScreenWidth / 2, (long)AnimalityMessageCounter);
        return;
    }

    if (FatalityMessage) {
        FatalityMessageCounter = (float)((double)FatalityMessageCounter
                                 + 0.2 / (double)limeFPSScaleFactor);
        if (FatalityMessageCounter >= 14.0f)
            FatalityMessageCounter = 7.0f;
        FinisherSprite(limeScreenWidth / 2,
                       (long)FatalityMessageCounter + 0x19);
        return;
    }

    if (FriendshipMessage) {
        long k = DrawHUD_C195[(long)(GameCounter * 0.25f) & 7];

        FinisherSprite((long)((float)(limeScreenWidth / 2)
                              + FE_WidthScale * -128.0f), k + 0x27);
        FinisherSprite((long)((float)(limeScreenWidth / 2)
                              + FE_WidthScale * 128.0f), k + 0x14);
    }
}

/* One half of the round-end test. `loser` is the player whose health hit zero,
 * so the round goes to the other one. */
static void RoundEndedAgainst(long loser)
{
    long winner = loser ^ 1;

    if (Health[loser] != 0 || RoundSummary != 0)
        return;

    RoundSummary = 1;
    RoundWins[winner]++;
    Round++;

    if (RoundWins[0] == WinsNeeded || RoundWins[1] == WinsNeeded) {
        updateMPWins();
        return;
    }

    flawlessVictories = 0;
    playerLostRound  = 1;

    /* The one-round kode hands the match over immediately. */
    if (GameMode == 1 && *theKode == 0x13) {
        RoundWins[loser]  = WinsNeeded;
        RoundWins[winner] = WinsNeeded - 1;
        return;
    }
    if (GameMode == 3)
        return;

    if (Health[winner] == 100) {
        FlawlessMessage = 1;
        if (GameMode == 4 || GameMode == 0) {
            Stats[6]++;
            flawlessVictories++;
            achievementsUnlock(2);
            if (flawlessVictories == 5)
                achievementsUnlock(3);
        }
    } else if (GameMode == 1 && isParentBasedOnSpeed()
               && Health[1] == 0 && Health[0] == 100) {
        /* the network half counts the host's flawless without the message */
        Stats[6]++;
    }
}

void DrawHUD(void)
{
    float s;

    HUDANIM_Render();
    limeEnableAlphaBlending_Basic();
    limeSet2DDrawing();

    /* Two full-screen overlays, both from Scorpion's lair transition. */
    ScorpionFade += ScorpionFadeAdd / limeFPSScaleFactor;
    if (ScorpionFade > 1.0f)
        ScorpionFade = 1.0f;
    if (ScorpionFade != 0.0f)
        limeFillRect(0.0f, 0.0f, (float)limeScreenWidth, (float)limeScreenHeight,
                     0.0f, 0.0f, 0.0f, ScorpionFade);

    if (ScorpionFlash != 0.0f) {
        ScorpionFlash += (-1.0f / 6.0f) / limeFPSScaleFactor;
        if (ScorpionFlash <= 0.0f)
            ScorpionFlash = 0.0f;
        limeFillRect(0.0f, 0.0f, (float)limeScreenWidth, (float)limeScreenHeight,
                     1.0f, 1.0f, 1.0f, 1.0f);
    }

    limeEnableAlphaBlending_Basic();
    limeSet2DDrawing();
    limeDisableDepthTest();
    limeDisableDepthWrites();

    /* Blackout. IsInFinishing suspends it, which is what makes the kode
     * playable at all -- the fatality stays lit. */
    if (*theKode == 1) {
        if (lightsOn == 0.0f && !IsInFinishing) {
            limeFillRect(0.0f, 0.0f, (float)limeScreenWidth,
                         (float)limeScreenHeight, 0.0f, 0.0f, 0.0f, 1.0f);
            goto tail;
        }
        lightsOn -= 1.0f / limeFPSScaleFactor;
        if (lightsOn < 0.0f)
            lightsOn = 0.0f;
    }

    if (!GamePaused)
        DrawControls();

    if (GameMode == 3) {
        usprintf(strBuf, UC("%s: %d"), GameTextNoHeader(0x121), KarnageScore);
        limeDrawFONT(&GameFont, limeUC(strBuf), FE_WidthScale * 8.0f,
                     FE_HeightScale * 32.0f, 0, FE_WidthScale, fontcol);
    }

    /* The mercy banner cycles thirteen frames of the finisher sheet. */
    if (MercyMessage) {
        if (MercyMessageCounter < 13.0f)
            DrawAnimAsSprite(limeScreenWidth / 2, limeScreenHeight / 2,
                             FE_WidthScale * 0.5f, 0x100, 0x100,
                             (long)(uintptr_t)HUDFatalsTexture,
                             fatal_HUDgfx_SpriteDef, fatal_HUDgfx_Anim,
                             0,
                             MERCY_FIRST_FRAME
                                 + (long)MercyMessageCounter % MERCY_FRAMES,
                             0, fatal_HUDgfx_Anim[0] - 1, 1, col);

        /* 0.2 a frame (0x2844e: the double at 0x286f4), not 13: at 13 the
         * banner was gone in three frames and the stage music came back
         * over Mercy.mp3 at once. */
        MercyMessageCounter = (float)((double)MercyMessageCounter
                              + 0.2 / (double)limeFPSScaleFactor);
        if (MercyMessageCounter >= 30.0f) {
            MercyMessageCounter = 0.0f;
            MercyMessage        = 0;
            /* the banner ducked the music; put it back */
            if (Settings[2] != 0)
                limePlayTune(LevelMusic[*LevelSelectPtr],
                             (long)MusicVol[Settings[2]], 1);
        }
    }

    if (FlawlessMessage) {
        if (!DoingSKDeath)
            limeDrawFONT(&GameFont, GameText(0x3a0),
                         (float)(limeScreenWidth / 2), FE_HeightScale * 112.0f,
                         1, FE_WidthScale, fontcol);
        FlawlessCounter += 1.0f / limeFPSScaleFactor;
        if (FlawlessCounter > 180.0f) {
            FlawlessCounter = 0.0f;
            FlawlessMessage = 0;
        }
    }

    if (GamePaused)
        goto tail;

    s = HUD_Scale;

    /* ---- the plates ---- */
    /* Karnage skips player one's plate too (0x284ec: GameMode 3 jumps past
     * the whole plate block to the streak line); drawn here, it covered
     * the score at (8, 32). */
    if (GameMode != 3) {
        if (*theKode != 0x11)
            DrawPlayerPlate(0);
        DrawRoundWinCoins(0);
    }

    if (GameMode == 0 && winStreak > 1) {
        usprintf(strBuf, UC("%s: %d"), GameTextNoHeader(0xb4), winStreak);
        limeDrawFONT(&GameFont, limeUC(strBuf), s * 34.0f, s * 6.0f,
                     0, s, fontcol);
    } else if (GameMode == 4 && survivalWinStreak > 1) {
        usprintf(strBuf, UC("%s: %d"), GameTextNoHeader(0xb4), survivalWinStreak);
        limeDrawFONT(&GameFont, limeUC(strBuf), s * 34.0f, s * 6.0f,
                     0, s, fontcol);
    }

    if (DangerMessage[0])
        limeDrawSprite(DangerTPage, s * 18.0f, s * 86.0f, s * 64.0f, s * 16.0f,
                       0.0f, 0.0f, 1.0f, 1.0f, col);

    /* Karnage is the one mode that never shows player 2's plate. */
    if (*theKode != 0x11 && GameMode != 3) {
        DrawPlayerPlate(1);
        DrawRoundWinCoins(1);
    }

    if (DangerMessage[1])
        limeDrawSprite(DangerTPage,
                       (float)limeScreenWidth - s * 82.0f, s * 86.0f,
                       s * 64.0f, s * 16.0f, 0.0f, 0.0f, 1.0f, 1.0f, col);

    if (GameMode == 2)
        TrainingMessages();

    /* ---- the clock ---- */
    if (!GamePaused)
        GameTime = (float)(ClockTens * 10 + ClockSingles);

    if (GameTime <= 0.0f) {
        /* Time out: the higher health takes the round, an exact tie neither. */
        GameTime = 0.0f;
        if (Health[1] > Health[0]) {
            RoundEndedAgainst(0);
        } else if (Health[1] < Health[0] && RoundSummary == 0) {
            RoundSummary = 1;
            RoundWins[0]++;
            Round++;
            if (Health[0] != 100)
                flawlessVictories = 0;
        }
    } else if (!GamePaused) {
        /* Every mode draws the two corner buttons while not paused (0x28910,
         * 0x2a170). Modes 0 and 1 also count timeInGame, outside the intro. */
        if (GameMode <= 1 && !DoIntro)
            timeInGame += (1.0f / 60.0f) / limeFPSScaleFactor;

        limeEnableAlphaBlending_Additive();

        /* INFO, top left: the moves list */
        limeDrawSprite(InfoTexture, s * -4.0f, s * -7.0f,
                       s * 36.0f, s * 36.0f, 0.0f, 0.0f, 1.0f, 1.0f, col);

        /* ...and its pulse: a second INFO that grows by 32 and fades out
         * (0x29810, 0x2ab2a), every 4 units -- 0.75 in the first five
         * seconds of the round. */
        InfoScaleAdd += (1.0f / 60.0f) / limeFPSScaleFactor;
        if ((GameTime > 95.0f ? 0.75f : 4.0f) < InfoScaleAdd) {
            InfoScale += 0.05f / limeFPSScaleFactor;
            if (InfoScale > 1.0f) {
                InfoScale    = 0.0f;
                InfoScaleAdd = 0.0f;
            } else {
                float fade[4];

                memcpy(fade, DrawHUD_PulseColour, sizeof fade);
                fade[3] = 1.0f - InfoScale;
                limeDrawSprite(InfoTexture,
                               s * (InfoScale * -16.0f - 4.0f),
                               s * (InfoScale * -16.0f - 7.0f),
                               s * (InfoScale * 32.0f + 36.0f),
                               s * (InfoScale * 32.0f + 36.0f),
                               0.0f, 0.0f, 1.0f, 1.0f, fade);
            }
        }

        /* PAUSE, top right, every frame (0x289c6) */
        limeDrawSprite(PauseTexture,
                       (float)limeScreenWidth - s * 32.0f, s * -7.0f,
                       s * 36.0f, s * 36.0f, 0.0f, 0.0f, 1.0f, 1.0f, col);
        limeEnableAlphaBlending_Basic();
    }

    sprintf(strBuf, "%d", (int)GameTime);
    limeDrawFONT(&CountDownFont, strBuf, (float)(limeScreenWidth / 2),
                 s * 20.0f, 1, s, fontcol);

    /* ---- who won ---- */
    if (WinnerMessage[0] || WinnerMessage[1]) {
        RoundEndedAgainst(0);
        RoundEndedAgainst(1);

        if (GameMode != 3)
            limeDrawFONT(&GameFont, limeUC((const char *)WinnerMessage),
                         (float)(limeScreenWidth / 2),
                         FE_HeightScale * 80.0f
                             + FE_HeightScale * 320.0f * SKDeathMessageOffset,
                         1, FE_WidthScale, fontcol);

        /* Shao Kahn's death slides the banner up into place; once it is
         * there (0x29d46, `bls`, so a NaN counts too) the three lines of the
         * ending come up under it every frame (0x2a5e0):
         *      0x39c  SHAO KAHN IS NO MORE     y 112
         *      0x39d  YOU ARE THE              y 144
         *      0x39e  ULTIMATE MK3 CHAMPION    y 176
         * They were missing here. */
        if (DoingSKDeath) {
            SKDeathMessageOffset += -0.016666668f / limeFPSScaleFactor;
            if (!(SKDeathMessageOffset > 0.0f)) {
                SKDeathMessageOffset = 0.0f;
                limeDrawFONT(&GameFont, GameText(0x39c),
                             (float)(limeScreenWidth / 2),
                             FE_HeightScale * 112.0f, 1, FE_WidthScale, fontcol);
                limeDrawFONT(&GameFont, GameText(0x39d),
                             (float)(limeScreenWidth / 2),
                             FE_HeightScale * 144.0f, 1, FE_WidthScale, fontcol);
                limeDrawFONT(&GameFont, GameText(0x39e),
                             (float)(limeScreenWidth / 2),
                             FE_HeightScale * 176.0f, 1, FE_WidthScale, fontcol);
            }
        }

        DrawFinisherBanner();
    }

    /* The last two of the tower. The indices are Motaro and Shao Kahn. */
    if (GameMode == 0 && RoundWins[0] == WinsNeeded) {
        if (PLAYER2MODEL == 0x18) {
            achievementsUnlock(0xe);
        } else if (PLAYER2MODEL == 0x19) {
            achievementsUnlock(0x10);
            if (!towerFinishedAndLogged) {
                towerFinishedAndLogged = 1;
                EASDK_LogEvent(0x3f8, 15, DestinyNames[Destiny],
                               15, CharacterNames[PLAYER1MODEL]);
            }
        }
    }

    if (RoundSummary == 1) {
        RoundSummaryUpdate();
    } else if (RoundSummary == 2) {
        if (FightMessage) {
            FightMessageTimer += 1.0f / limeFPSScaleFactor;
            if (FightMessageTimer > 180.0f) {
                /* 0x29f00..0x29f08: THREE stores. RoundSummary goes back to 0
                 * with the other two; without it the banner below came back
                 * the moment "FIGHT" ended, and "ROUND 1" stayed up for the
                 * whole round. */
                RoundSummary      = 0;
                FightMessage      = 0;
                FightMessageTimer = 0.0f;
            }
        } else {
            /* the round banner, and the only untranslated strings in the file */
            const char *banner = (Round == 0) ? "ROUND 1"
                               : (Round == 1) ? "ROUND 2"
                               : (Round == 2) ? "ROUND 3"
                               : (Round == 3) ? "ROUND 4" : (const char *)0;
            if (banner)
                limeDrawFONT(&GameFont, banner, (float)(limeScreenWidth / 2),
                             FE_HeightScale * 80.0f, 1, FE_WidthScale, fontcol);
        }
    }

    DrawComboCounter(0);
    DrawComboCounter(1);

tail:
    achievementsDraw();
    limeEnableDepthTest();
    limeEnableDepthWrites();
}

/* The summary timer and everything hanging off it: the tap that skips the wait,
 * the end-of-round stats, the two ways a match can finish, and the setup for
 * the next round. This is `DrawHUD`'s `RoundSummary == 1` arm. */
static void RoundSummaryUpdate(void)
{
    /* Only the host runs the AI. */
    if (GameMode != 1 || isParentBasedOnSpeed())
        no_ai_hack();

    if (GameMode == 4 && RoundWins[0] == WinsNeeded && survivalWinStreak > 0x13)
        achievementsUnlock(0x13);

    /* No early return on WinnerMessage or IsInFinishing. With the winner
     * banner up the binary goes 0x29a64 -> 0x29c9c (draw it, then the
     * fatality/babality banners) -> back to 0x29a7c, and with IsInFinishing
     * set 0x29a86 -> 0x2a2d6 adds 0.7 instead (the double at 0x2a32c) and
     * rejoins at 0x29aba. The timer always runs; IsInFinishing only slows it
     * and blocks the tap below. Returning here froze the summary forever:
     * WinnerMessage is cleared only by ResetFightData (0x227ca), which runs
     * at the end of this function -- the round-1 softlock of 0.0.1. Adding
     * 1.25 during the finish too ended the fight before a fatality and its
     * announcer were over. */
    RoundSummaryTime = (float)((double)RoundSummaryTime
                       + (IsInFinishing ? 0.7 : 1.25) / (double)limeFPSScaleFactor);

    /* A tap above the bottom band skips the rest of the wait. */
    if (GameMode != 4 && !IsInFinishing
        && limeLastTouchScreenX[0] == -1.0f
        && limeTouchScreenY[0] != -1.0f
        && limeTouchScreenY[0] < (float)limeScreenHeight + FE_HeightScale * -64.0f
        && RoundSummaryTime < ROUND_SUMMARY_HOLD)
        RoundSummaryTime = ROUND_SUMMARY_HOLD;

    if (RoundSummaryTime < ROUND_SUMMARY_HOLD || !RoundHasEnded)
        return;

    if (!RoundHasEndedStatsUpdated) {
        RoundHasEndedStatsUpdated = 1;
        if (GameMode == 1) {
            Stats[14]++;
            Stats[13] += Health[isParent() ? 0 : 1];
            Stats[12]  = (long)((float)Stats[12] + (99.0f - GameTime));
        } else if (GameMode == 6 || GameMode == 0) {
            Stats[14]++;
            Stats[13] += Health[0];
            Stats[12]  = (long)((float)Stats[12] + (99.0f - GameTime));
        }
    }

    if (RoundWins[0] == WinsNeeded) {
        QuitAsWin();
        RoundSummaryTime = 0.0f;
        if (GameMode == 6) {
            EASDK_LogEvent(0x7559, 15, "2 Players on 1 iPad", 0, (const char *)0);
            EASDK_LogEventEnumEnumStringNum(0x755a, 15, "2 Players on 1 iPad",
                                            7, (long)timeInGame);
        }
        return;
    }
    if (WinsNeeded == RoundWins[1]) {
        QuitAsLose();
        RoundSummaryTime = 0.0f;
        if (GameMode == 6) {
            EASDK_LogEvent(0x7559, 15, "2 Players on 1 iPad", 0, (const char *)0);
            EASDK_LogEventEnumEnumStringNum(0x755a, 15, "2 Players on 1 iPad",
                                            7, (long)timeInGame);
        }
        return;
    }

    /* Nobody has the match yet. Fade out first, unless the fade already ran, in
     * which case start the next round now. */
    if (FE_Fade != 0.0f) {
        FE_FadeAdd       = -1.0f / 30.0f;
        DontQuitAfterFade = 1;
        return;
    }

    /* 0x2abaa-0x2abb6: the 1/30 goes through the slot 0xf3540, which holds
     * 0x10089c, _FE_FadeAdd -- the fade back in. Written to InfoScaleAdd,
     * FE_Fade stayed at 0 and round 2 played on a black screen. */
    FE_FadeAdd        = 1.0f / 30.0f;
    DontQuitAfterFade = 0;
    RoundSummary      = 2;

    LIME_InitEventsManager();
    CamTrackToPlayer = -1;
    IsInFinishing    = 0;
    RoundParam[13]   = 1;
    RoundParam[14]   = 0;
    RoundParam[4]    = Round + 1;

    if (IsEndurance) {
        InitEnduranceMatch();
        if (PLAYER2MODEL != PLAYER1MODEL)
            DumpAltCostume(Players + 0x5f0);
        LoadGameCharacterCheckCache(Players + 0x5f0,
                                    &PlayerDefs[PLAYER2MODEL],
                                    Stats);
    } else {
        ((signed char *)RoundParam)[0x18] = -1;
    }

    /* The fourth argument is AIOn, not 1 -- see the header. */
    switch (AIOn) {
    case 2:
        mk3_init(PLAYER1MODEL | 0x80, PLAYER2MODEL | 0x80,
                 (void (*)(void))FrameID_GetBBox, AIOn);
        break;
    case 1:
        mk3_init(PLAYER1MODEL, PLAYER2MODEL | 0x80, (void (*)(void))FrameID_GetBBox, AIOn);
        break;
    default:
        mk3_init(PLAYER1MODEL, PLAYER2MODEL, (void (*)(void))FrameID_GetBBox, AIOn);
        break;
    }

    Player1NumButtons = Settings[4];
    mk3_set_four_button(0, Settings[4] != 6);
    mk3_set_four_button(1, Player2NumButtons != 6);
    ResetFightData();
}


/* ----------------------------------------------------------------- DrawControls
 *
 * armv7 0x0001ca44, 1,844 bytes.  **Complete.**
 *
 * The on-screen touch controls: a joystick and six buttons for player one, and
 * the same again for player two when the screen is shared. `DrawHUD` calls it
 * once a frame while the game is not paused.
 *
 * It does two jobs, and the first is not drawing. **It rebuilds `ButtonsPos`
 * and `ButtonsPosP2` from the settings every frame** before drawing anything,
 * copying six five-word records out of whichever layout table the current
 * settings select. `HandleTouches` reads those same two arrays to decide what
 * the player pressed, so the layout the player sees and the layout the game
 * tests against cannot drift apart -- they are rebuilt together, from the same
 * source, in the same frame.
 *
 * ### Which layout table
 *
 * `Settings[4]` is the button count and `Settings[5]` picks stock or custom:
 *
 *      Settings[5] == 0        Settings[5] != 0
 *      4  ButtonsPos4          4  CustomButtonsPos4
 *      5  ButtonsPos5          5  CustomButtonsPos5
 *      6  ButtonsPos6          6  CustomButtonsPos6
 *
 * and when `P2Controls` is set, both halves are overwritten again from the
 * split-screen tables -- `ButtonsPosP2_1` / `ButtonsPos6P2_1` for player one and
 * `ButtonsPosP2_2` / `ButtonsPos6P2_2` for player two, chosen by each player's
 * own button count. The copy is written as five passes over six records, one
 * word per pass, which is why the loop looks inside out.
 *
 * ### Three alpha levels, one of them a setting
 *
 * The function starts by copying three RGBA constants onto its own stack:
 *
 *      {1, 1, 1, 0.6}   the buttons
 *      {1, 1, 1, 0.3}   the joystick base
 *      {1, 1, 1, 1.0}   the joystick knob
 *
 * and then **overwrites the first two alphas**. `Settings[6]` selects 0.5, 0.75
 * or 1.0 for the buttons, and the joystick base gets exactly half of whatever
 * that came out as. The knob is always opaque. So the controls-opacity setting
 * moves two of the three and the knob stays put, which is what makes the stick
 * readable at the lowest setting.
 *
 * ### The joystick
 *
 * Single player:
 *
 *      JoystickStatePosX = (long)ButtonSize
 *      JoystickStatePosY = (long)(limeScreenHeight - ButtonSize)
 *      JSIZE             = ButtonSize
 *
 * Split screen shrinks the stick to two thirds and lifts it, and puts player
 * two's on the right half:
 *
 *      JSIZE  = ButtonSize * 2/3
 *      P1     = ( ButtonSize*2/3,  limeScreenHeight - ButtonSize*4/3 )
 *      P2     = ( ButtonSize*4/3 + limeScreenWidth/2 + 48,
 *                 limeScreenHeight - ButtonSize*2/3 )
 *
 * The arithmetic runs in **double** in the original -- 2/3 is a `double`
 * literal and the sums go through `vcvt.f64.f32` -- so a port that does it in
 * float will land a pixel off at some resolutions.
 *
 * The base is drawn at `2 * JSIZE` square centred on that point, and the knob
 * is offset by `JoyOffset[JoystickState]`:
 *
 *      0  (  0,   0)   centre       5  (  0,  56)   down
 *      1  (  0, -56)   up           6  (-39,  39)   down-left
 *      2  ( 39, -39)   up-right     7  (-56,   0)   left
 *      3  ( 56,   0)   right        8  (-39, -39)   up-left
 *      4  ( 39,  39)   down-right
 *
 * Nine entries: the centre and eight compass points on a circle of radius 56,
 * with 39 standing in for 56/sqrt(2). The knob offsets are **not** scaled by
 * `JSIZE` -- they are pixels, so a port that changes the stick size has to
 * scale this table itself.
 *
 * ### The buttons
 *
 * Six records of five words each -- x, y, size, atlas cell, button id -- and a
 * record whose id is -1 is skipped. The sprite is drawn centred:
 *
 *      x - size/2,  y - size/2,  size x size
 *
 * and the atlas cell picks the UV:
 *
 *      u = ((cell & 3) * 2 + (ButtonStates[id] ? 0 : 1)) * 0.125
 *      v = (cell / 4) * 0.25
 *
 * so the sheet is **eight columns by four rows**, and a button's pressed and
 * unpressed art are the two halves of one pair -- **pressed is the even
 * column**. That pairing is why `cell & 3` indexes in twos.
 *
 * Player two's half is the same loop over `ButtonsPosP2` and `ButtonStatesP2`,
 * and it only runs when `P2Controls` is set.
 */

#define DRAWCONTROLS_BUTTONS   6
#define BUTTON_ATLAS_COLS      8            /* 0.125 of the sheet a column */
#define BUTTON_ATLAS_ROWS      4            /* 0.25 a row */
#define JOY_DIRECTIONS         9

extern long JoyOffset[JOY_DIRECTIONS][2];   /* 0x000de09c, pixels, not scaled */

/* Copy one layout table into the live one. The original writes this as five
 * passes over six records, one word each, so a partially-written table is never
 * left behind if a later pass picks a different source. */
static void CopyButtonLayout(long *dst, const long *src)
{
    long i;

    for (i = 0; i < DRAWCONTROLS_BUTTONS * (BUTTONPOS_STRIDE / 4); i++)
        dst[i] = src[i];
}

/* One player's six buttons. `pos` is the live layout and `states` the pressed
 * flags that go with it. */
static void DrawButtonRow(const long *pos, const long *states,
                          float *colour)
{
    long b;

    for (b = 0; b < DRAWCONTROLS_BUTTONS; b++) {
        const long *e    = &pos[b * (BUTTONPOS_STRIDE / 4)];
        long        id   = e[4];
        long        size, cell, u, v;

        if (id == -1)
            continue;

        size = e[2];
        cell = e[3];

        /* pressed is the even column of the pair */
        u = (cell & 3) * 2 + (states[id] ? 0 : 1);
        v = cell / BUTTON_ATLAS_ROWS;

        limeDrawSprite(ButtonsTPage,
                       (float)(e[0] - size / 2), (float)(e[1] - size / 2),
                       (float)size, (float)size,
                       (float)u / (float)BUTTON_ATLAS_COLS,
                       (float)v / (float)BUTTON_ATLAS_ROWS,
                       1.0f / (float)BUTTON_ATLAS_COLS,
                       1.0f / (float)BUTTON_ATLAS_ROWS,
                       colour);
    }
}

/* The stick: a base at 2*JSIZE square, and a knob offset by the direction. */
static void DrawJoystick(long x, long y, long state,
                         float *base, float *knob)
{
    limeDrawSprite(ButtonsTPage,
                   (float)x - JSIZE, (float)y - JSIZE,
                   JSIZE + JSIZE, JSIZE + JSIZE,
                   0.375f, 0.5f, 0.25f, 0.5f, base);

    limeDrawSprite(ButtonsTPage,
                   (float)(x + JoyOffset[state][0]) - JSIZE * 0.5f,
                   (float)(y + JoyOffset[state][1]) - JSIZE * 0.5f,
                   JSIZE, JSIZE,
                   0.25f, 0.5f, 0.125f, 0.25f, knob);
}

int PortHideControls;                   /* port: see the end of DrawControls */

void DrawControls(void)
{
    float buttonCol[4];                 /* sp+0x44 */
    float joyBaseCol[4];                /* sp+0x34 */
    float joyKnobCol[4];                /* sp+0x24 */
    const long *src;

    buttonCol[0]  = 1.0f; buttonCol[1]  = 1.0f; buttonCol[2]  = 1.0f;
    buttonCol[3]  = 0.6f;
    joyBaseCol[0] = 1.0f; joyBaseCol[1] = 1.0f; joyBaseCol[2] = 1.0f;
    joyBaseCol[3] = 0.3f;
    joyKnobCol[0] = 1.0f; joyKnobCol[1] = 1.0f; joyKnobCol[2] = 1.0f;
    joyKnobCol[3] = 1.0f;

    Player1NumButtons = Settings[4];

    /* ---- rebuild the live layouts from the settings ---- */
    if (Settings[5] == 0) {
        src = (Settings[4] == 4) ? ButtonsPos4
            : (Settings[4] == 5) ? ButtonsPos5
            : (Settings[4] == 6) ? ButtonsPos6 : (const long *)0;
    } else {
        src = (Settings[4] == 4) ? CustomButtonsPos4
            : (Settings[4] == 5) ? CustomButtonsPos5
            : (Settings[4] == 6) ? CustomButtonsPos6 : (const long *)0;
    }
    if (src)
        CopyButtonLayout(ButtonsPos, src);

    if (P2Controls) {
        CopyButtonLayout(ButtonsPos,
                         (Settings[4] == 5) ? ButtonsPosP2_1 : ButtonsPos6P2_1);
        CopyButtonLayout(ButtonsPosP2,
                         (Player2NumButtons == 5) ? ButtonsPosP2_2
                                                  : ButtonsPos6P2_2);
    }

    /* ---- the opacity setting moves two of the three alphas ---- */
    buttonCol[3]  = (Settings[6] == 1) ? 0.75f
                  : (Settings[6] == 2) ? 1.0f : 0.5f;
    joyBaseCol[3] = buttonCol[3] * 0.5f;

    /* ---- where the stick goes ---- */
    JoystickStatePosX = (long)ButtonSize;
    JSIZE             = ButtonSize;
    JoystickStatePosY = (long)((float)limeScreenHeight - ButtonSize);

    if (P2Controls) {
        /* the doubles are the original's; a float port lands a pixel off */
        double two_thirds = 0.666667;
        float  small      = (float)((double)ButtonSize * two_thirds);

        JSIZE             = small;
        JoystickStatePosX = (long)small;
        JoystickStatePosY = (long)((double)((float)limeScreenHeight - small)
                                   + (double)ButtonSize * -two_thirds);

        JoystickStatePosXP2 =
            (long)((double)ButtonSize * two_thirds
                   + (double)(small + (float)(limeScreenWidth / 2) + 48.0f));
        JoystickStatePosYP2 = (long)((float)limeScreenHeight - small);
    }

    /* A port option (hide_controls=1, the launcher's box): the layout above
     * is still worked out -- the touch code hit-tests against it -- but the
     * stick and the buttons are not drawn. Not in the binary. */
    if (PortHideControls)
        return;

    DrawJoystick(JoystickStatePosX, JoystickStatePosY, JoystickState,
                 joyBaseCol, joyKnobCol);

    if (P2Controls)
        DrawJoystick(JoystickStatePosXP2, JoystickStatePosYP2, JoystickStateP2,
                     joyBaseCol, joyKnobCol);

    DrawButtonRow(ButtonsPos, ButtonStates, buttonCol);

    if (P2Controls)
        DrawButtonRow(ButtonsPosP2, ButtonStatesP2, buttonCol);

}


/* ------------------------------------------------------------ Task_GameDestroy
 *
 * armv7 0x00022c74, 1,500 bytes.  **Complete.**
 *
 * Tears the fight down and decides what the game does next. Most of it is a
 * flat list -- every texture deleted, every scene freed -- and the last forty
 * lines are the only part that thinks.
 *
 * ### The kode does not survive the fight
 *
 * `*theKode = -1` is the second thing it does, before a single texture is
 * freed. Whatever kode was entered on the VS screen is gone the moment the
 * fight ends; nothing carries it into the next one.
 *
 * ### The music is stopped only if the music is on
 *
 * `if (Settings[2]) limeStopTune();` — the volume setting doubles as an
 * enable, so with music off the call is skipped rather than made against a
 * silent mixer. `FadeMusicOut` is cleared either way.
 *
 * ### The layer free is gated by the level's own table
 *
 *      Level_Info[LevelSelect * 0xf4] + 0x74 + i    a char * a layer, i in 0..7
 *
 * and a layer is freed only when the first byte of its name is non-zero. So a
 * level with four background layers frees four, and the other four slots are
 * left alone rather than freed blind. That 0xf4 stride is **244**, which is
 * `LEVEL_INFO_STRIDE` — the compiler builds it here as `61 * 4` out of shifts,
 * a third independent sighting after `GetNextLevel` and the BGEXTENTS loader.
 *
 * ### Sixteen scenes, one shape
 *
 * Every scene handle is freed the same way and then zeroed:
 *
 *      if (h) { LIME_FreeMeshSetTextures(h->0x80); LIME_FreeScene(h); h = 0; }
 *
 * `BGSceneHandle`, `BGSceneHandle2`, `SZEffectScene`, `SwatEffectScene`,
 * `SmashThruScene`, `SKEffectScene`, `CyraxSelfDestructScene`, `XeroxScene`,
 * `BloodScene`, `PitDeathScene`, `RocksScene`, `TrainScene`, `TrainDie1Scene`,
 * `TrainDie2Scene`, `SLDie1Scene`, `SLDie2Scene` — the same list
 * [GAME-EVENTS.md](../../docs/GAME-EVENTS.md) named from the other end, where
 * they are played rather than freed.
 *
 * `MeshSet_FIGHT` and `Scene_FIGHT` are freed last and are the only two that
 * are not zeroed afterwards.
 *
 * ### Where the game goes next
 *
 *      GameMode == 1                NextTask = 2, and disableHeartbeat()
 *      GameMode == 4 and JustWon    NextTask = 5, and LevelSelect advances
 *                                   through GetNextLevel
 *      otherwise                    NextTask = 2
 *
 * and `CurrentTask = 8` on every path. Then, outside a network game, a level
 * chosen from the pause menu overrides all of it:
 *
 *      if (InGameLevelSelect != LevelSelect) {
 *          LevelSelect = InGameLevelSelect;
 *          NextTask    = 5;
 *      }
 *
 * so **the in-game level select wins over the survival ladder**. Picking a
 * stage mid-fight and then finishing the round sends you to that stage, not to
 * the next rung.
 *
 * `opponentCharacter = -1` is the last thing written, which is what makes the
 * next fight pick a fresh opponent rather than reuse this one's.
 */

extern void  *InfoTexture;              /* 0x001ab990 */
extern void  *PauseTexture;             /* 0x001ab98c */
extern void  *SmokeEarthTexture;        /* 0x001ab66c */
extern void  *SmokeExplosionTexture;    /* 0x001ab670 */
extern void  *NewGreenBloodTexture;     /* 0x001f4494 */
extern void  *NewBlackBloodTexture;     /* 0x001f4498 */
extern void  *CoinTPage;                /* 0x001f40d4 */
extern void  *DangerTPage;              /* 0x001f40d0 */
extern void  *TPages[6];                /* 0x001f40ac */
extern void  *SZEffectScene;            /* 0x001aba48 */
extern void  *SwatEffectScene;          /* 0x001aba4c */
extern void  *PitDeathScene;            /* 0x001aba50 */
extern void  *CyraxSelfDestructScene;   /* 0x001aba54 */
extern void  *BloodScene;               /* 0x001aba58 */
extern void  *SKEffectScene;            /* 0x001aba5c */
extern void  *XeroxScene;               /* 0x001aba60 */
extern void  *RocksScene;               /* 0x001aba64 */
extern void  *TrainScene;               /* 0x001aba68 */
extern void  *TrainDie1Scene;           /* 0x001aba6c */
extern void  *TrainDie2Scene;           /* 0x001aba70 */
extern void  *SLDie1Scene;              /* 0x001aba78 */
extern void  *SLDie2Scene;              /* 0x001aba7c */
extern long   otherPlayerPaused;        /* 0x0014e200 */
extern long  *opponentCharacterP;       /* pointer slot -> 0x000ff998 */

void UnLoadAllSounds(void);
void limeStopTune(void);
void HUDANIM_Destroy(void);
void FreeLevelCharacters(void);
void LIME_FreeMeshSet(void *meshset);
void LIME_FreeMeshSetTextures(void *meshset);
void LIME_FreeScene(void *scene);
int GetNextLevel(int cur);
void disableHeartbeat(void);

#define GAMEDESTROY_LAYERS   8
#define GAMEDESTROY_BGTEX    8
#define LEVELINFO_LAYERNAMES 0x74       /* eight char * a level, at this offset */

/* Every scene is freed the same way, and every one but the last two is then
 * zeroed. */
static void FreeSceneHandle(void **h)
{
    if (*h) {
        LIME_FreeMeshSetTextures(((void **)*h)[0x80 / 4]);
        LIME_FreeScene(*h);
    }
    *h = NULL;
}

void Task_GameDestroy(void)
{
    long i;

    FadeMusicOut = 0;
    *theKode     = -1;                  /* the kode does not outlive the fight */

    UnLoadAllSounds();
    LIME_KillAllEvents();

    if (Settings[2])
        limeStopTune();

    /* ---- every texture ---- */
    limeDeleteTexture(*FEBits1);
    limeDeleteTexture(PauseBGTexture);
    limeDeleteTexture(MoveIconsTexture);
    limeDeleteTexture(CoinTPage);
    limeDeleteTexture(DangerTPage);

    for (i = 0; i < 10; i++)
        limeDeleteTexture(HUDFatalsTexture[i]);

    FreeBloodTextures();

    limeDeleteTexture(*SmokeTexture);
    limeDeleteTexture(NewBloodTexture);
    limeDeleteTexture(NewGreenBloodTexture);
    limeDeleteTexture(NewBlackBloodTexture);
    limeDeleteTexture(InfoTexture);
    limeDeleteTexture(CancelTexture);
    limeDeleteTexture(PauseTexture);
    limeDeleteTexture(WhiteTexture);
    limeDeleteTexture(SmokeStarFieldTexture);
    limeDeleteTexture(SmokeEarthTexture);
    limeDeleteTexture(SmokeExplosionTexture);

    for (i = 0; i < 5; i++)
        limeDeleteTexture(SpearTexture[i]);
    for (i = 0; i < 6; i++)
        limeDeleteTexture(TPages[i]);

    limeDeleteTexture(HUDTPage);
    limeDeleteTexture(ButtonsTPage);

    for (i = 0; i < GAMEDESTROY_BGTEX; i++)
        limeDeleteTexture(LevelBGTexture[i]);

    HUDANIM_Destroy();
    FreeLevelCharacters();

    /* ---- the background layers this level actually has ---- */
    for (i = 0; i < GAMEDESTROY_LAYERS; i++) {
        const char *const *names =
            (const char *const *)(Level_Info
                                  + *LevelSelectPtr * LEVEL_INFO_STRIDE
                                  + LEVELINFO_LAYERNAMES);
        if (names[i] == NULL || names[i][0] == '\0')
            continue;                   /* the slot was never loaded */
        LIME_FreeMeshSet(*MeshSetLayers[i]);
    }

    /* ---- every scene ---- */
    FreeSceneHandle(&BGSceneHandle);
    FreeSceneHandle(&BGSceneHandle2);
    FreeSceneHandle(&SZEffectScene);
    FreeSceneHandle(&SwatEffectScene);
    FreeSceneHandle(&SmashThruScene);
    FreeSceneHandle(&SKEffectScene);
    FreeSceneHandle(&CyraxSelfDestructScene);
    FreeSceneHandle(&XeroxScene);
    FreeSceneHandle(&BloodScene);
    FreeSceneHandle(&PitDeathScene);
    FreeSceneHandle(&RocksScene);
    FreeSceneHandle(&TrainScene);
    FreeSceneHandle(&TrainDie1Scene);
    FreeSceneHandle(&TrainDie2Scene);
    FreeSceneHandle(&SLDie1Scene);
    FreeSceneHandle(&SLDie2Scene);

    /* these two are freed but not zeroed */
    LIME_FreeMeshSet(MeshSet_FIGHT);       /* the value, as stored at step 31 */
    LIME_FreeScene(Scene_FIGHT);           /* and at step 32 */

    /* ---- where next ---- */
    if (GameMode == 1) {
        NextTask    = 2;
        CurrentTask = 8;
        disableHeartbeat();
    } else if (GameMode == 4 && JustWon) {
        NextTask         = 5;
        CurrentTask      = 8;
        *LevelSelectPtr  = GetNextLevel(*LevelSelectPtr);
        *InGameLevelSelect = *LevelSelectPtr;
    } else {
        NextTask    = 2;
        CurrentTask = 8;
    }

    GamePaused        = 0;
    otherPlayerPaused = 0;

    /* a stage picked from the pause menu beats the survival ladder */
    if (GameMode != 1 && *InGameLevelSelect != *LevelSelectPtr) {
        *LevelSelectPtr = *InGameLevelSelect;
        NextTask        = 5;
    }

    *opponentCharacterP = -1;
}


/* ----------------------------------------------------------- UpdateArcadeCode
 *
 * armv7 0x00021ee8, 1,516 bytes.  **Complete.**
 *
 * The fight tick. Not the kode entry the name suggests -- "arcade code" is the
 * ported arcade *engine*, and this is what steps it. It calls `mk3_update`, and
 * then `AddNewGameEvents` to drain what the step produced.
 *
 * ### The arcade runs at 55.93 Hz, and this is where that lives
 *
 *      FrameCompensation += 0.9322 / limeFPSScaleFactor;
 *      while (FrameCompensation >= 1.0f) {
 *          ... one arcade tick ...
 *          FrameCompensation -= 1.0f;
 *      }
 *
 * **0.9322 arcade ticks per display frame** -- 55.93 a second at sixty, which
 * is the original cabinet's rate. The loop runs zero, one or two ticks in a
 * frame, so the engine is already decoupled from the display and a port does
 * not have to invent that. `SpeedNormal` changes only the constant:
 * **0.4661 when clear, exactly half**, which is the slow-motion mode. The
 * compiler duplicated the whole accumulate for the two constants, so the two
 * arms look like different code and are not.
 *
 * ### `ToggleDebug` freezes the engine and nothing else
 *
 *      if (!ToggleDebug) mk3_update(joy, &GameObjects);
 *
 * Everything after it -- the events, the HUD animation, the positions, the
 * camera -- runs either way. So the debug freeze holds the fighters still while
 * the presentation layer keeps ticking, which is what makes it useful and what
 * a port has to reproduce if it keeps the toggle.
 *
 * ### One tick
 *
 *      the joystick pair            from the caller, or from the wire
 *      mk3_update                   unless ToggleDebug
 *      the parent's send            sendSpriteListPacket, FrameCount++
 *      AnimateBG
 *      AddNewGameEvents             drain what the step queued
 *      RunGameEvents
 *      LIME_UpdateEvents
 *      HUDANIM_Update
 *      ArcadePosTo3dPos x8          every object into its player slot
 *      Player1Pos / Player2Pos      out of Players +8 and +0x5f8
 *      RunJaxGrowCounters, and Jax's squash
 *      PlayerAutoSmoothAnims x2
 *      TrackCam
 *
 * The eight objects sit at `GameObjects[0] + i * 16` -- **a 16-byte stride**,
 * the same one `RenderLevelPlayers` walks -- and each lands at
 * `Players + i * 0x5f0 + 8`.
 *
 * ### The camera flattens during a finisher
 *
 * With `IsInFinishing` set and `DoingStageFatal` clear, both positions are
 * copied to the stack and **both get the lower of the two Z values** before
 * `TrackCam` sees them, so the camera does not tilt up when the winner is
 * airborne over a corpse. Any other time `TrackCam` gets them unmodified --
 * and on the network guest it gets them **swapped**, player two first, because
 * the guest is player two.
 *
 * ### Jax's squash
 *
 * Frames 0x1b12 to 0x1b2c on either object trigger it: the whole 0x5f0-byte
 * PLAYER is copied into `JaxSquashedPlayer`, `JaxGrowCounter` is zeroed,
 * `JaxBeingSquashed` is set, and the live player's mirror flag is **toggled
 * between 0 and 0x10** with the old value saved in `JaxSquashFlip`. The counter
 * then increments every tick, and `RenderLevelPlayers` stops drawing the
 * squashed copy once it passes 0x108.
 *
 * ### The network split
 *
 * `isParentBasedOnSpeed` decides which half a machine runs.
 *
 * **The parent** builds the joystick pair from its own stick and
 * `mpOpponentJoystickInput` -- two words an entry, the second being
 * `opponentFPS2` -- steps the engine, and sends the sprite list every tick.
 * The joystick packet goes out **only when the bits change**, compared against
 * `lastJoybits`, so an idle stick costs nothing on the wire.
 *
 * **The guest never steps the engine.** It drains every sprite list that has
 * arrived, in a loop:
 *
 *      while (setNextSpritesAndEvents()) {
 *          GameObjects[0] = &mpSpriteList[i * 160];
 *          Player1Pos, Player2Pos memcpy'd out of the packet
 *          MKEventQueue  memcpy'd out of the packet, 0x54 bytes
 *          the packet's event block memset to zero
 *          AnimateBG / AddNewGameEvents / RunGameEvents / LIME_UpdateEvents
 *      }
 *      if (!SpritelistReceived) GameObjects[0] = dummyObjects;
 *
 * so a guest that fell behind catches up by running every queued list in one
 * display frame rather than one a frame. And a guest that has never received
 * anything points at `dummyObjects` instead of at nothing.
 *
 * **0x54 is 84 bytes: a four-byte count and ten eight-byte records.** The wire
 * carries at most ten of the events [GAME-EVENTS.md](../../docs/GAME-EVENTS.md)
 * describes, per tick. That is the netcode's event budget and it is a hard cap.
 * `mpEventQueue`'s stride is 216 bytes and `mpSpriteList`'s is 160.
 *
 * ### A debug watermark left in the shipped build
 *
 * The parent still prints, on every new high:
 *
 *      #########################
 *      ## MAX EVENT QUEUE NUM = %d
 *      #########################
 *
 * a high-water trace for exactly the ten-event budget above. It ships.
 */

#define ARCADE_TICKS_PER_FRAME   0.9322     /* 55.93 Hz at sixty */
#define ARCADE_TICKS_SLOWMO      0.4661     /* exactly half */
#define ARCADE_OBJECTS           8
#define ARCADE_PLAYER_STRIDE     0x5f0
#define ARCADE_OBJECT_STRIDE     16
#define JAX_SQUASH_FRAME_LO      0x1b12
#define JAX_SQUASH_FRAME_HI      0x1b2c
#define MP_SPRITELIST_STRIDE     160
#define MP_EVENTQUEUE_STRIDE     216
#define MP_EVENT_BYTES           0x54       /* a count and ten records */

extern long   gameCnt;                  /* 0x00150ea0 */
extern long   SpeedNormal;              /* 0x0014e1f8 */
extern float  FrameCompensation;        /* 0x0014e1f0 */
extern float  FrameCount;               /* 0x0014fa60 */
extern long   ToggleDebug;              /* 0x00150588 */
extern long   currentMPJoystickData;    /* 0x0014e26c */
extern long   currentMPSpriteList;      /* 0x0014e270 */
extern long   lastSpritelistIndex;      /* 0x0010dec8 */
extern long   lastJoybits;              /* 0x00150ea4 */
extern long   highestQueueNum;          /* 0x00150ea8 */
extern long   opponentFPS2;             /* 0x0014e1e4 */
extern long   SpritelistReceived;       /* 0x0010deac */
extern long   JaxSquashFlip;            /* 0x001ab030 */
extern char   JaxSquashedPlayer[];      /* 0x001ab034 */
extern long   mpOpponentJoystickInput[];/* 0x001ab970, two words an entry */
extern char   mpSpriteList[];           /* 0x001ab680 */
extern char   mpEventQueue[];           /* 0x001ab7c0 */
extern char   dummyObjects[];           /* 0x0010de84 */

void AnimateBG(void);
void AddNewGameEvents(void);
void RunGameEvents(void);
void LIME_UpdateEvents(void);
void HUDANIM_Update(void);
void RunJaxGrowCounters(void);
void TrackCam(const float *a, const float *b, long flag);
/* Returns 1 when a thread handler returned negative and the frame was
 * abandoned, 0 otherwise. Declared void here until mk3.c was read. */
long mk3_update(const long *joy, void **objects);
long setNextSpritesAndEvents(void);
void sendSpriteListPacket(void *objects, long a, long b);
void sendJoystickInputPacket(long a, long bits);

/* Frames 0x1b12..0x1b2c mean Jax has just flattened someone. `mirror` is the
 * victim's mirror-flag offset -- 0x540 for player one, 0xb30 for player two. */
static void CatchJaxSquash(long mirror)
{
    long *flag;

    if (JaxBeingSquashed)
        return;

    memcpy(JaxSquashedPlayer, (char *)Players + (mirror - 0x540),
           ARCADE_PLAYER_STRIDE);
    JaxGrowCounter   = 0;
    JaxBeingSquashed = 1;

    flag  = (long *)((char *)Players + mirror);
    *flag = *flag ? 0 : 0x10;
    JaxSquashFlip = *flag;
}

void UpdateArcadeCode(int *joy1, int *joy2)
{
    long i;

    (void)joy2;                         /* the second pointer is never read */

    gameCnt++;

    FrameCompensation = (float)((double)FrameCompensation
                                + (SpeedNormal ? ARCADE_TICKS_PER_FRAME
                                               : ARCADE_TICKS_SLOWMO)
                                  / (double)limeFPSScaleFactor);

    while (FrameCompensation >= 1.0f) {
        long joy[2];
        int  stepped = 1;

        if (GameMode == 1 && !isParentBasedOnSpeed()) {
            /* ---- the guest: drain every list that arrived ---- */
            while (setNextSpritesAndEvents()) {
                long k = currentMPSpriteList;
                char *pkt = mpEventQueue + k * MP_EVENTQUEUE_STRIDE;

                GameObjects[0] = &mpSpriteList[k * MP_SPRITELIST_STRIDE];
                lastSpritelistIndex = k;

                memcpy(Player1Pos, pkt,        12);
                memcpy(Player2Pos, pkt + 0xc,  12);
                memcpy(MKEventQueue, pkt + 0x18, MP_EVENT_BYTES);
                memset(pkt + 0x18, 0, MP_EVENT_BYTES);

                AnimateBG();
                AddNewGameEvents();
                RunGameEvents();
                LIME_UpdateEvents();
            }
            if (!SpritelistReceived)
                GameObjects[0] = dummyObjects;   /* nothing has ever arrived */
            stepped = 0;
        } else if (GameMode != 1 && !isParentBasedOnSpeed()) {
            /* the caller's pair goes straight through */
            /* `&GameObjects`: 0x222a2 loads the ADDRESS 0x14dfec into r1, and
             * mk3_update stores the frame's object list through it. This is
             * the only writer GameObjects has besides Task_GameInit's zero. */
            mk3_update((const long *)joy1, (void **)&GameObjects);
        } else {
            long k = currentMPJoystickData;

            joy[0] = *joy1;
            joy[1] = mpOpponentJoystickInput[k * 2];
            opponentFPS2 = mpOpponentJoystickInput[k * 2 + 1];

            if (!ToggleDebug)
                mk3_update(joy, (void **)&GameObjects);   /* 0x2238c, the same */
        }

        if (stepped) {
            if (GameMode == 1 && isParentBasedOnSpeed()) {
                if (MKEventQueue[0] > highestQueueNum) {
                    highestQueueNum = MKEventQueue[0];
                    puts("#########################");
                    printf("## MAX EVENT QUEUE NUM = %d\n", (int)highestQueueNum);
                    puts("#########################");
                }
                sendSpriteListPacket(GameObjects[0], 0, 0);
                FrameCount = FrameCount + 1.0f;
            }

            AnimateBG();
            AddNewGameEvents();
            RunGameEvents();
            LIME_UpdateEvents();
        }

        HUDANIM_Update();

        /* ---- every object's world position, into its player slot ---- */
        /* 0x21fde-0x22026: the object is `GameObjects + 16*i` -- the
         * VALUE loaded from 0x14dfec, both for the first call (fp) and the
         * rest (literal -> 0x14dfec, plus (i+1)*16). Written as
         * GameObjects[0] it read a pointer out of object 0's own bytes, so
         * only one fighter's 3D position was right and the camera followed
         * one player. */
        if (GameObjects) {
            for (i = 0; i < ARCADE_OBJECTS; i++)
                ArcadePosTo3dPos((Mk3Obj_t *)((char *)GameObjects
                                              + i * ARCADE_OBJECT_STRIDE),
                                 (float *)((char *)Players
                                           + i * ARCADE_PLAYER_STRIDE + 8), 0);
        }

        memcpy(Player1Pos, (char *)Players + 8, 12);
        memcpy(Player2Pos, (char *)Players + 0x5f8, 12);
        RunJaxGrowCounters();

        {
            const unsigned short *o = (const unsigned short *)(void *)GameObjects;

            if (o[8 / 2] >= JAX_SQUASH_FRAME_LO
                && o[8 / 2] <= JAX_SQUASH_FRAME_HI)
                CatchJaxSquash(0x540);
            if (o[0x18 / 2] >= JAX_SQUASH_FRAME_LO
                && o[0x18 / 2] <= JAX_SQUASH_FRAME_HI)
                CatchJaxSquash(0xb30);
        }
        JaxGrowCounter++;

        PlayerAutoSmoothAnims((PLAYER *)Players);
        PlayerAutoSmoothAnims((PLAYER *)((char *)Players + ARCADE_PLAYER_STRIDE));

        /* ---- the camera ---- */
        if (IsInFinishing && !DoingStageFatal) {
            float a[3], b[3], z;

            memcpy(a, Player1Pos, 12);
            memcpy(b, Player2Pos, 12);
            z = (Player1Pos[2] <= Player2Pos[2]) ? Player1Pos[2]
                                                 : Player2Pos[2];
            a[2] = z;                   /* both flattened to the lower Z */
            b[2] = z;
            TrackCam(a, b, 1);
        } else if (GameMode == 1 && !isParentBasedOnSpeed()) {
            TrackCam(Player2Pos, Player1Pos, 1);   /* the guest is player two */
        } else {
            TrackCam(Player1Pos, Player2Pos, 1);
        }

        FrameCompensation -= 1.0f;
    }

    /* ---- and the stick goes out, but only when it changed ---- */
    if (GameMode == 1 && !isParentBasedOnSpeed() && *joy1 != lastJoybits) {
        sendJoystickInputPacket(*joy1, *joy1);
        lastJoybits = *joy1;
    }
}


/* ------------------------------------------------------------------ TrackCam
 *
 * armv7 0x0001b8b4, 1,840 bytes.  **Complete.**
 *
 * The fight camera. A leaf function -- **not one call in 1,840 bytes**, just
 * float arithmetic over named tuning globals -- called once a tick from
 * `UpdateArcadeCode` with the two fighters' positions.
 *
 * There are **two cameras in here** and `NewCam` picks between them. The old
 * one is the PSP camera and it is dead code in the shipped build unless
 * `NewCam` is cleared; it is decompiled below because it is still reachable and
 * because it is the simpler of the two to read against.
 *
 * ### The screen edges are converted once, at the top
 *
 *      CamLeftLimit3d  = CamLeftLimit  / WorldScaleAdjust
 *      CamRightLimit3d = CamRightLimit / WorldScaleAdjust
 *
 * so the two limits are authored in arcade units and cached in world units
 * every frame. Both cameras clamp against the `3d` pair, never the originals.
 *
 * ### The old camera: a spring with a speed limit
 *
 *      t      = (|a.x - b.x| - 1) / psp_maxdist,  clamped to [0, 1]
 *      target = (t*a.x + midpoint*(1 - t)) * psp_scale + psp_off
 *      CamVelX = (target - Camera.x) * 0.125,  clamped to +-mxvelx
 *      Camera.x += CamVelX
 *
 * Far apart it follows **player one**; close together it slides to the
 * midpoint. The height is a straight lerp on distance:
 *
 *      u = (clamp(dist, distzoomedin, distzoomedout) - distzoomedin)
 *          / (distzoomedout - distzoomedin)
 *      Camera.y = -((1 - u)*camzoomedin + u*camzoomedout)
 *      Camera.z = camheight
 *
 * ### The new camera: distance sets the pull-back
 *
 *      dist = clamp(|(a.x - b.x) * 1.5| + 5.0, 6.0, 8.0)
 *
 * and the camera sits exactly that far behind the look-at in Y --
 * `NewCamera.y = a.y - dist`. So **the pull-back is a clamped linear function
 * of how far apart the fighters are**, and 6 to 8 is its whole range.
 *
 * The look-at tracks the midpoint but is leashed to player one:
 *
 *      DistInX = clamp(midpoint - a.x, -MaxDistInX, MaxDistInX)
 *      NewCameraLookAt.x = a.x + DistInX
 *
 * so a fighter who runs away drags the camera only `MaxDistInX` before the
 * other one starts leaving the frame. `swivelscale` is set to 1 and
 * `zoomedoutweight` to 0 on entry and neither is read again here.
 *
 * ### Z is optional, and clamped to two different windows
 *
 * The third argument gates the Z work entirely. With it set:
 *
 *      NewCameraLookAt.z = midZ + 1.25 - SceneGroundOffset,  clamped [1.25, 2.25]
 *      NewCamera.z       = midZ + 1.5  - SceneGroundOffset,  clamped [1.5,  2.5]
 *
 * Two windows a quarter apart, so the camera always looks slightly down.
 *
 * ### A stage fatality follows the falling player
 *
 * `DoingStageFatal` is `player + 1`, and its two values pick **which fighter's
 * Z the camera follows** -- 1 takes `a.z`, anything else takes `b.z`, and both
 * recompute the two Z values above from that one player rather than the
 * midpoint. That is the camera riding someone down the pit.
 *
 * ### Jax's grow drifts the look-at
 *
 *      NewCameraLookAt.x += min(JaxGrowCounter, 0xaf) * 0.0075 * 1.2
 *
 * negated when `JaxSquashFlip` is set, so the drift goes the way the squashed
 * fighter was facing. The counter is capped at 0xaf here while
 * `RenderLevelPlayers` stops drawing the squashed copy at 0x108 -- **the camera
 * stops moving before the sprite disappears**, which is presumably the point.
 *
 * ### Three ways the result is committed
 *
 *      LockCamera        nothing is written at all
 *      IsInFinishing     Camera and CameraLookAt EASE toward their targets at
 *                        0.125 a tick -- unless SnapCam, which jumps
 *      otherwise         both are assigned outright
 *
 * and `OverrideCamera` fires only inside the finisher path, where
 * `CamOverridePos.x` overwrites the X of both the camera and the look-at.
 * `SnapCam` is cleared on every exit, so it is a one-shot.
 *
 * The direct commit writes the look-at as `x + (y - x)` -- **an ease with the
 * weight gone**, which the compiler could not fold because it is float -- and
 * it is kept that way below, because in float it is not the same as `y`.
 */

#define TRACKCAM_DIST_MIN     6.0f
#define TRACKCAM_DIST_MAX     8.0f
#define TRACKCAM_LOOK_Z_MIN   1.25f
#define TRACKCAM_LOOK_Z_MAX   2.25f
#define TRACKCAM_CAM_Z_MIN    1.5f
#define TRACKCAM_CAM_Z_MAX    2.5f
#define TRACKCAM_EASE         0.125f
#define JAXGROW_CAM_CAP       0xaf

extern float  CamLeftLimit;             /* 0x001f44a8, arcade units */
extern float  CamRightLimit;            /* 0x001f44ac */
extern float  CamLeftLimit3d;           /* 0x001f44b0, world units */
extern float  CamRightLimit3d;          /* 0x001f44b4 */
extern long   NewCam;                   /* 0x0014e1bc */
extern float  psp_maxdist;              /* 0x0014dfa8 */
extern float  psp_scale;                /* 0x0014dfa0 */
extern float  psp_off;                  /* 0x0014dfa4 */
extern float  CamVelX;                  /* 0x0014dfac */
extern float  mxvelx;                   /* 0x0014dfb0 */
extern float  distzoomedin;             /* 0x0014dfbc */
extern float  distzoomedout;            /* 0x0014dfc0 */
extern float  camzoomedin;              /* 0x0014dfb4 */
extern float  camzoomedout;             /* 0x0014dfb8 */
extern float  camheight;                /* 0x0014dff0 -- the compiler
                                         * copies it with a plain word move */
extern float  swivelscale;              /* 0x001f44a0 */
extern float  zoomedoutweight;          /* 0x001f44a4 */
extern float  MaxDistInX;               /* 0x00150e94 */
extern float  DistInX;                  /* 0x001f44b8 */
extern float  SceneGroundOffset;        /* 0x0014df8c */
extern float  Camera[3];                /* 0x0014fa74 */
extern float  NewCamera[3];             /* 0x0014fa8c */
extern float  NewCameraLookAt[3];       /* 0x0014fa98 */
extern float  CamOverridePos[3];        /* 0x001ab000 */
extern long   SnapCam;                  /* 0x00150e90 */

/* Ease one axis toward its target, or jump straight to it when SnapCam is set.
 * The weight is the same 0.125 the old camera's spring uses. */
static float EaseTo(float cur, float target, long snap)
{
    float d = target - cur;

    if (!snap)
        d = d * TRACKCAM_EASE;
    return cur + d;
}

void TrackCam(const float *a, const float *b, long withZ)
{
    float dx = a[0] - b[0];

    CamLeftLimit3d  = CamLeftLimit  / WorldScaleAdjust;
    CamRightLimit3d = CamRightLimit / WorldScaleAdjust;

    /* ---------------------------------------------------------- old camera */
    if (!NewCam) {
        float dist = (dx < 0.0f) ? -dx : dx;
        float t, w, target, u;

        t = (float)(((double)dist - 1.0) / (double)psp_maxdist);
        w = 1.0f;
        if (t > 1.0f) {
            t = 1.0f;
            w = 0.0f;
        } else if (t < 0.0f) {
            t = 0.0f;                   /* w stays 1: follow the midpoint */
        } else {
            w = w - t;
        }

        target  = (t * a[0] + (a[0] + b[0]) * 0.5f * w) * psp_scale + psp_off;
        CamVelX = (target - Camera[0]) * TRACKCAM_EASE;
        if (CamVelX > mxvelx)
            CamVelX = mxvelx;
        if (-mxvelx > CamVelX)
            CamVelX = -mxvelx;
        Camera[0] = Camera[0] + CamVelX;

        if (dist > distzoomedout) dist = distzoomedout;
        if (dist < distzoomedin)  dist = distzoomedin;
        u = (dist - distzoomedin) / (distzoomedout - distzoomedin);

        Camera[1] = -((1.0f - u) * camzoomedin + u * camzoomedout);
        Camera[2] = camheight;
        SnapCam = 0;
        return;
    }

    /* ---------------------------------------------------------- new camera */
    {
        float pull, mid;

        swivelscale     = 1.0f;
        zoomedoutweight = 0.0f;

        /* how far back the camera sits, straight off the fighters' spread */
        pull = dx * 1.5f;
        if (pull < 0.0f)
            pull = -pull;
        pull += 5.0f;
        if (pull < TRACKCAM_DIST_MIN) pull = TRACKCAM_DIST_MIN;
        if (pull > TRACKCAM_DIST_MAX) pull = TRACKCAM_DIST_MAX;

        /* the look-at chases the midpoint, leashed to player one */
        mid     = (a[0] + b[0]) * 0.5f;
        DistInX = mid - a[0];
        if (DistInX < -MaxDistInX) DistInX = -MaxDistInX;
        if (MaxDistInX < DistInX)  DistInX = MaxDistInX;

        NewCameraLookAt[0] = a[0] + DistInX;
        NewCameraLookAt[1] = a[1];

        if (withZ) {
            float midz = (a[2] + b[2]) * 0.5f;

            NewCameraLookAt[2] = (float)((double)midz + 1.25
                                         - (double)SceneGroundOffset);
        }
        if (NewCameraLookAt[2] < TRACKCAM_LOOK_Z_MIN)
            NewCameraLookAt[2] = TRACKCAM_LOOK_Z_MIN;
        else if (NewCameraLookAt[2] > TRACKCAM_LOOK_Z_MAX)
            NewCameraLookAt[2] = TRACKCAM_LOOK_Z_MAX;

        /* Jax's grow drifts the look-at, the way the squashed one faced */
        if (JaxBeingSquashed) {
            long n = JaxGrowCounter;

            if (n >= JAXGROW_CAM_CAP)
                n = JAXGROW_CAM_CAP;
            if (JaxSquashFlip)
                n = -n;
            NewCameraLookAt[0] += (float)n * 0.0075f * 1.2f;
        }

        NewCamera[0] = NewCameraLookAt[0];
        NewCamera[1] = a[1] - pull;

        if (withZ) {
            float midz = (a[2] + b[2]) * 0.5f;

            NewCamera[2] = (float)((double)midz + 1.5
                                   - (double)SceneGroundOffset);
        }
        if (NewCamera[2] < TRACKCAM_CAM_Z_MIN)
            NewCamera[2] = TRACKCAM_CAM_Z_MIN;
        else if (NewCamera[2] > TRACKCAM_CAM_Z_MAX)
            NewCamera[2] = TRACKCAM_CAM_Z_MAX;

        /* a stage fatality rides ONE player down instead of the midpoint */
        if (DoingStageFatal) {
            float z = (DoingStageFatal == 1) ? a[2] : b[2];

            NewCameraLookAt[2] = (float)((double)z + 1.25
                                         - (double)SceneGroundOffset);
            NewCamera[2]       = (float)((double)z + 1.5
                                         - (double)SceneGroundOffset);
        }

        /* both ends clamped to the screen edges, in world units */
        if (CamLeftLimit3d  > NewCamera[0])       NewCamera[0]       = CamLeftLimit3d;
        if (CamRightLimit3d < NewCamera[0])       NewCamera[0]       = CamRightLimit3d;
        if (CamLeftLimit3d  > NewCameraLookAt[0]) NewCameraLookAt[0] = CamLeftLimit3d;
        if (CamRightLimit3d < NewCameraLookAt[0]) NewCameraLookAt[0] = CamRightLimit3d;
    }

    /* ------------------------------------------------------------- commit */
    if (IsInFinishing) {
        long snap = SnapCam;

        if (OverrideCamera) {
            NewCamera[0]       = CamOverridePos[0];
            NewCameraLookAt[0] = CamOverridePos[0];
        }

        Camera[0] = EaseTo(Camera[0], NewCamera[0], snap);
        Camera[1] = EaseTo(Camera[1], NewCamera[1], snap);
        Camera[2] = EaseTo(Camera[2], NewCamera[2], snap);

        CameraLookAt[0] = EaseTo(CameraLookAt[0], NewCameraLookAt[0], snap);
        CameraLookAt[1] = EaseTo(CameraLookAt[1], NewCameraLookAt[1], snap);
        CameraLookAt[2] = EaseTo(CameraLookAt[2], NewCameraLookAt[2], snap);
    } else if (!LockCamera) {
        Camera[0] = NewCamera[0];
        Camera[1] = NewCamera[1];
        Camera[2] = NewCamera[2];

        /* x + (y - x), as the original computes it -- an ease with no
         * weight, and NOT the same float as y: the difftest against the
         * oracle (work/, 2026-10-08) showed 1-ulp differences when this was
         * written as a plain assignment. */
        CameraLookAt[0] = CameraLookAt[0] + (NewCameraLookAt[0] - CameraLookAt[0]);
        CameraLookAt[1] = CameraLookAt[1] + (NewCameraLookAt[1] - CameraLookAt[1]);
        CameraLookAt[2] = CameraLookAt[2] + (NewCameraLookAt[2] - CameraLookAt[2]);
    }

    SnapCam = 0;                        /* a one-shot, cleared on every exit */
}


/* ---------------------------------------------------------------- IntroRender
 *
 * armv7 0x00025784, 1,484 bytes.  **Complete.**
 *
 * The pre-fight intro: a two-shot camera move over the fighters, then a
 * cross-fade into the gameplay camera. Runs once a frame while `DoIntro` is
 * set, and takes itself down when it is done.
 *
 * ### Four keyframes, built from where the fighters actually stand
 *
 * The eye path and the look-at path are four `vec3` each, and their **X and Y
 * are rewritten every frame** from the two players:
 *
 *      IntroPos[0].x = P2.x - 1.25      IntroLook[0] = (P2.x, P2.y)
 *      IntroPos[1].x = P2.x + 0.25      IntroLook[1] = (P2.x, P2.y)
 *      IntroPos[2].x = P1.x + 1.25      IntroLook[2] = (P1.x, P1.y)
 *      IntroPos[3].x = P1.x - 0.25      IntroLook[3] = (P1.x, P1.y)
 *
 * so it is two shots -- 0 to 1 across player two, then 2 to 3 across player one
 * -- and each sweeps 1.5 units past its subject. Z is never written and stays
 * at its initialiser, 1.6 for every eye keyframe.
 *
 * `IntroCamCount` steps by **two**, so it is the shot index rather than the
 * keyframe index, and at 4 the camera work is over.
 *
 * ### The eye's height lerps from a value in the next array
 *
 * The lerp reads the *from* keyframe's Z at a **constant** offset, `IntroPos +
 * 0x50`. `IntroPos` is 0x30 bytes -- four `vec3` -- so 0x50 is thirty-two bytes
 * past its end, and lands inside `IntroLook` on keyframe 2's Z.
 *
 * It is not a harmless overrun. `IntroLook[2].z` is **1.35** in the data and
 * this function never writes any look-at Z, while every `IntroPos` Z is
 * **1.6**. So the eye rises from 1.35 to 1.6 across each shot instead of
 * holding 1.6. The drift is visible, it is what shipped, and a port that
 * "fixes" the index changes how the intro looks. Written below the way the
 * binary reads it.
 *
 * ### Three phases
 *
 *      IntroCamCount <= 2    animate the fighters a frame at a time, lerp the
 *                            camera between the shot's two keyframes
 *      IntroCamCount  > 2    TrackCam takes over and the intro camera
 *                            CROSS-FADES into it on the same timer
 *      IntroCountTimer >= 1  in phase three: pin the timer at 60, clear
 *                            DoIntro, call EndIntro
 *
 * The animation catch-up is the same accumulate the fight tick uses --
 * `IntroFrameComp += 1 / limeFPSScaleFactor` and a frame of animation per whole
 * unit -- so the intro plays at a fixed rate whatever the display does.
 *
 * **The timer is only advanced in phase three.** Phases one and two read it and
 * reset it to zero when it reaches 1.0, but nothing here increments it; that
 * happens inside `AnimateIntroCharacterPlayers1Frame`.
 *
 * ### Skipping it
 *
 * Outside a network game, a touch ends the intro immediately -- but the test is
 * inverted from what you would expect:
 *
 *      limeLastTouchScreenX == -1 && limeTouchScreenX != -1
 *
 * that is, **nothing was touched last frame and something is touched now**: the
 * leading edge of a tap, not the tap itself. `DoIntro` is cleared and `EndIntro`
 * runs. In a network game the intro cannot be skipped at all.
 *
 * ### The fade, and what viewing achievements does to it
 *
 * While `FE_FadeAdd` is non-zero the fade advances and the intro does nothing
 * else. The step normally divides by `limeFPSScaleFactor`; while the
 * achievements overlay is up it **multiplies** by it instead, and by a tenth:
 *
 *      normal          FE_Fade += FE_FadeAdd / limeFPSScaleFactor
 *      achievements    FE_Fade += FE_FadeAdd / 10 * limeFPSScaleFactor
 *
 * Two different senses of the same scale factor in one function. At either end
 * the fade clamps and clears `FE_FadeAdd`, so it is self-terminating.
 */

#define INTRO_SHOTS          2          /* IntroCamCount steps by two */
#define INTRO_TIMER_STEP     (1.0 / 15.0)
#define INTRO_TIMER_PARKED   60.0f

extern float  IntroPlayer1PosX;         /* 0x0014f930 */
extern float  IntroPlayer1PosZ;         /* 0x0014f934 */
extern float  IntroPlayer2PosX;         /* 0x0014f938 */
extern float  IntroPlayer2PosZ;         /* 0x0014f93c */
extern long   IntroCamCount;            /* 0x0014f940 */
extern float  IntroPos[4][3];           /* 0x0014f944, 0x30 bytes exactly */
extern float  IntroLook[4][3];          /* 0x0014f974 */
extern float  IntroEye[3];              /* 0x001abb80 */
extern float  IntroAt[3];               /* 0x001abb74 */
extern float  IntroCountTimer;          /* 0x0014e1cc */
extern float  IntroFrameComp;           /* 0x00151068 */
extern float  FE_Fade;                  /* pointer slot -> 0x00100898 */
extern float  limeTouchScreenX[];        /* 0x00171af4, four slots */

void AnimateIntroCharacterPlayers1Frame(long a);
void RenderIntroCharacterPlayer(void);
void MaintainLevelScenes(void);
void RenderLevelBG(void);
void EndIntro(void);
int areAchievementsViewing(void);
void LIMEDS_Set3dMode(void);

void IntroRender(void)
{
    float up[3];                        /* sp+0x2c, from a const (0, 0, 1) */
    float t, w;
    long  i, j;

    up[0] = 0.0f; up[1] = 0.0f; up[2] = 1.0f;

    /* the shot geometry follows the fighters every frame */
    IntroPlayer1PosX = Player1Pos[0];
    IntroPlayer1PosZ = Player1Pos[1];
    IntroPlayer2PosX = Player2Pos[0];
    IntroPlayer2PosZ = Player2Pos[1];

    IntroPos[0][0] = Player2Pos[0] - 1.25f;
    IntroPos[1][0] = Player2Pos[0] + 0.25f;
    IntroPos[2][0] = Player1Pos[0] + 1.25f;
    IntroPos[3][0] = Player1Pos[0] - 0.25f;

    IntroLook[0][0] = Player2Pos[0]; IntroLook[0][1] = Player2Pos[1];
    IntroLook[1][0] = Player2Pos[0]; IntroLook[1][1] = Player2Pos[1];
    IntroLook[2][0] = Player1Pos[0]; IntroLook[2][1] = Player1Pos[1];
    IntroLook[3][0] = Player1Pos[0]; IntroLook[3][1] = Player1Pos[1];

    LIMEDS_Set3dMode();
    limeEnableDepthTest();
    limeEnableDepthWrites();

    /* ---- the fighters animate at a fixed rate, whatever the display does ---- */
    if (IntroCamCount <= INTRO_SHOTS) {
        IntroFrameComp += 1.0f / limeFPSScaleFactor;
        while (IntroFrameComp > 0.0f) {
            IntroFrameComp -= 1.0f;
            AnimateIntroCharacterPlayers1Frame(0);
        }
    }

    if (IntroCountTimer >= 1.0f) {
        IntroCamCount  += 2;            /* the shot index, not the keyframe */
        IntroCountTimer = 0.0f;
    }

    t = IntroCountTimer;
    w = 1.0f - t;

    if (IntroCamCount <= INTRO_SHOTS) {
        /* ---- phase one and two: lerp across the shot's two keyframes ---- */
        i = IntroCamCount;
        j = i + 1;

        IntroEye[0] = w * IntroPos[i][0] + t * IntroPos[j][0];
        IntroEye[1] = w * IntroPos[i][1] + t * IntroPos[j][1];
        /* The binary computes this address as `IntroPos + 0x50`, thirty-two
         * bytes past the end of IntroPos, and it lands here -- on keyframe
         * two's look-at Z, which nothing ever writes. Spelled as where it
         * lands rather than as how it is computed; see the header. */
        IntroEye[2] = w * IntroLook[2][2] + t * IntroPos[j][2];

        IntroAt[0] = w * IntroLook[i][0] + t * IntroLook[j][0];
        IntroAt[1] = w * IntroLook[i][1] + t * IntroLook[j][1];
        IntroAt[2] = w * IntroLook[i][2] + t * IntroLook[j][2];

        IntroEye[2] += SceneGroundOffset;
        IntroAt[2]  += SceneGroundOffset;

        LIMEDS_SetCameraOrientation(IntroEye[0], IntroEye[1], IntroEye[2],
                                    IntroAt[0],  IntroAt[1],  IntroAt[2],
                                    up[0], up[1], up[2]);
    } else {
        /* ---- phase three: hand over to the gameplay camera ---- */
        float eye[3], at[3];

        TrackCam(Player1Pos, Player2Pos, 1);

        t = IntroCountTimer;
        w = 1.0f - t;

        eye[0] = w * IntroEye[0] + t * Camera[0];
        eye[1] = w * IntroEye[1] + t * Camera[1];
        eye[2] = w * (IntroEye[2] + SceneGroundOffset) + t * Camera[2];
        at[0]  = w * IntroAt[0]  + t * CameraLookAt[0];
        at[1]  = w * IntroAt[1]  + t * CameraLookAt[1];
        at[2]  = w * IntroAt[2]  + t * CameraLookAt[2];

        LIMEDS_SetCameraOrientation(eye[0], eye[1], eye[2],
                                    at[0],  at[1],  at[2],
                                    up[0], up[1], up[2]);

        IntroCountTimer = (float)((double)IntroCountTimer
                                  + INTRO_TIMER_STEP / (double)limeFPSScaleFactor);
        if (IntroCountTimer >= 1.0f) {
            IntroCountTimer = INTRO_TIMER_PARKED;
            DoIntro = 0;
            EndIntro();
        }
    }

    /* ---- draw, while the intro is still up ---- */
    if (DoIntro) {
        MaintainLevelScenes();
        RenderLevelBG();
        RenderIntroCharacterPlayer();
        DoSmokesSmoke(PLAYER1MODEL, PLAYER2MODEL);
        MaintainParticles();
    }

    /* ---- the fade owns the frame while it is running ---- */
    if (FE_FadeAdd != 0.0f) {
        if (areAchievementsViewing())
            FE_Fade += FE_FadeAdd / 10.0f * limeFPSScaleFactor;
        else
            FE_Fade += FE_FadeAdd / limeFPSScaleFactor;

        if (FE_Fade <= 0.0f) {
            if (FE_FadeAdd < 0.0f) {
                FE_Fade     = 0.0f;
                FE_FadeAdd = 0.0f;
            }
        } else if (FE_Fade >= 1.0f && FE_FadeAdd > 0.0f) {
            FE_Fade     = 1.0f;
            FE_FadeAdd = 0.0f;
        }
        return;
    }

    /* ---- the leading edge of a tap skips it, except on the network ---- */
    if (GameMode != 1
        && limeLastTouchScreenX[0] == -1.0f
        && limeTouchScreenX[0] != -1.0f) {
        DoIntro = 0;
        EndIntro();
    }
}


/* --------------------------------------------------------------- Task_GameMain
 *
 * armv7 0x0002afec, 2,288 bytes.  **Complete.**
 *
 * The fight's per-frame task. Everything else in this file is reached from
 * here: input, the arcade tick, the 3D pass, the HUD, the pause menu, the
 * stage-fatality cutaway and the fade that ends the round.
 *
 * ### The frame, in order
 *
 *      InGame = 1
 *      a stage picked from the pause menu starts a fade-out
 *      one hardware key can press BLOCK for both sides
 *      FrameCount and GameCounter step by 1 / limeFPSScaleFactor
 *      2D state: basic blend, colour mask RGB only, 2D, basic blend again
 *      the intro, if one is running -- and NOTHING ELSE this frame
 *      MaintainLevelScenes
 *      the round summary swallows input, or a transfer takes it instead
 *      GetReal6ButtonJoyBits for player one, and player two on a shared pad
 *      UpdateArcadeCode                          <- the fight itself
 *      DeviceRenderSettings, LightPlayers, RenderGameView
 *      the stage-fatality cutaway, if one is running
 *      DrawHUD
 *      karnage scoring
 *      the stage-fatal camera pull
 *      the fade, and the music fade riding on it
 *      the black overlay, ShowDebugInfo, UpdateInGamePauseMenu
 *
 * ### limePressed is one 64-bit mask, and bit 8 presses BLOCK for both sides
 *
 * The test the compiler emitted is
 *
 *      (limePressed[0] & 0x100) | (limePressed[1] & 0)
 *
 * and the second half is ANDed with zero, so it can never contribute. That is
 * exactly what a 64-bit `limePressed & 0x100` compiles to on a 32-bit target --
 * the high word's mask is all zeroes and the AND is emitted anyway. So
 * `limePressed` is a **single 64-bit key bitmask**, not two independent words.
 *
 * When the bit is set both joy words are pre-seeded with 0x400 *before*
 * `GetReal6ButtonJoyBits` runs, and that function ORs into whatever it finds
 * there. 0x400 is the bit it otherwise only sets on the **rising edge** of
 * `buttons[6]`, the special/block button -- so this key holds block down for
 * **both players at once** and skips the edge gate. No touch control reaches
 * it; it is a key path inherited from the Java ME build.
 *
 * ### The intro owns the whole frame
 *
 * While `DoIntro` is set the task runs `AnimateBG`, `LIME_UpdateEvents`,
 * `IntroRender`, both `LIME_RenderEvents` passes and `IntroRender2dBits`, and
 * then **returns** -- no input, no arcade tick, no HUD. It only falls through
 * on the frame `IntroRender` clears `DoIntro` itself. In a network game the
 * intro is cleared before any of that, so `GameMode == 1` never sees one.
 *
 * ### Three ways the frame gets its input
 *
 *      RoundSummary == 1     JoystickState = 0, and every button is moved to
 *                            LastButtonStates and cleared -- so the summary
 *                            sees releases and the fight sees nothing
 *      G->field44e != 0      ReadControls() instead, the transfer path that
 *                            `getTransferableFlags` reads from the other end
 *      otherwise             nothing extra
 *
 * and `GetReal6ButtonJoyBits` runs afterwards in **all three** cases. Player
 * two is only read when `P2Controls` is set, and then with `which = 1` and the
 * object pointer advanced by one 16-byte record.
 *
 * `UpdateArcadeCode` is skipped entirely while `GamePaused` or `LIME_Paused` is
 * set -- the render still runs, so a paused fight is a live picture of a frozen
 * world. With no second pad it is called with a **null** second joy word rather
 * than a pointer to the unused one.
 *
 * ### The stage fatality: a 330-tick cutaway on one float
 *
 * `DoSmokesEarthFatal` is a float counter, not the flag its declaration in this
 * file suggested, and every stage of the cutaway is a threshold on it:
 *
 *      <= 120     nothing drawn, the counter just runs
 *       > 120     the starfield tiles the screen, 512x512 at a time
 *       > 120     the earth, 128x128, centred
 *       > 240     ...and the earth shakes +-8 px, alternating on the tick's
 *                 parity, so it is a 1-tick square wave rather than a wobble
 *      >= 300     the earth is gone; the explosion animation and its sound
 *      300..330   a white flash over everything, 1.0 fading to 0 across 30
 *
 * The explosion is driven by the same counter at **two different rates**: the
 * frame index steps every 5 ticks and the scale grows every 7, from 2.0 by a
 * quarter each step, and drawing stops once the 7-tick index passes 15. The
 * frame index is passed unclamped -- `DrawAnimAsSprite` clamps it to the last
 * frame itself, which is why nothing bounds it here.
 *
 * The sound fires once, on the first frame past 300, and only if `Settings[3]`
 * is set -- the same "the volume setting doubles as an enable" shape
 * `Task_GameDestroy` uses for music. Its volume is `MusicVol[Settings[3]]`,
 * a table indexed by the setting rather than the setting scaled.
 *
 * ### Karnage refills the health bar every frame and banks the difference
 *
 *      KarnageScore += (100 - Health[1]) * 100
 *      Health[0] = Health[1] = 100, and the two bar widths with them
 *
 * so mode 3 is not a fight that can be lost: whatever damage landed this frame
 * becomes score at a hundred points a point, and both fighters are topped up
 * before the next one. The multiply is built out of shifts as `n * 20 * 5`.
 *
 * ### Where the fade sends the game
 *
 * The fade itself is the same self-terminating shape `IntroRender` uses, down
 * to the achievements overlay dividing by ten and multiplying by the scale
 * factor instead of dividing. What is new here is the end of it:
 *
 *      FE_Fade hits 0 going down, and DontQuitAfterFade is clear
 *          -> CurrentTask = 7, InGame = 0, GamePaused = 0, otherPlayerPaused = 0
 *
 * Task 7 is the fight's teardown. So **the fade is what ends the round** --
 * nothing else in this function leaves the task. `DontQuitAfterFade` is the
 * opt-out for fades that are only meant to dim the screen.
 *
 * Riding on the same fade, and only while `Settings[2]` and `FadeMusicOut` are
 * both set, `limeSetTuneVol(FE_Fade * 100)` walks the music down with the
 * picture, and `FadeMusicOut` clears itself when it lands on zero.
 */

#define GAMEMAIN_TASK_DESTROY   7           /* CurrentTask the fade hands off to */
#define GAMEMAIN_FADE_OUT_STEP  (-1.0f / 30.0f)
#define GAMEMAIN_KEY_BLOCK      0x100       /* bit 8 of the 64-bit limePressed */
#define GAMEMAIN_JOY_BLOCK      0x400       /* the bit it forces into both joys */
#define GAMEMAIN_BUTTONS        7           /* ButtonStates / LastButtonStates */
#define MK3OBJ_STRIDE           0x10

#define SMOKEEARTH_SHOW         120.0f      /* starfield and earth appear */
#define SMOKEEARTH_SHAKE        240.0f      /* the earth starts shaking */
#define SMOKEEARTH_BOOM         300.0f      /* explosion, sound, white flash */
#define SMOKEEARTH_FLASH_END    330.0f
#define SMOKEEARTH_TILE         512         /* the starfield tile, in pixels */
#define SMOKEEARTH_SIZE         128.0f      /* the earth sprite */
#define SMOKEEARTH_HALF         64          /* ...and half of it, as an offset */
#define SMOKEEARTH_SHAKE_PX     8
#define SMOKEEARTH_LAST_FRAME   15          /* of the 7-tick index */

extern long   InGame;                   /* 0x0010dec0 */
extern long   limePressed[2];           /* pointer slot -> 0x00391fcc, ONE u64 */
extern float  GameCounter;              /* 0x0014fa5c */
extern long   LIME_Paused;              /* 0x0014e004 */
extern char   explosion_SpriteDef[];    /* pointer slot -> 0x0017cac8 */
extern long   explosion_Anim[];         /* pointer slot -> 0x0017cec8 */

void limeSetTuneVol(long vol);
void UpdateInGamePauseMenu(void);

/* The stage fatality's cutaway. Split out for legibility; the binary has it
 * inline, jumping back to DrawHUD from every arm. */
static void SmokeEarthFatalCutaway(void)
{
    long  x, y;
    long  shake;
    float t;

    DoSmokesEarthFatal += 1.0f / limeFPSScaleFactor;
    if (DoSmokesEarthFatal <= SMOKEEARTH_SHOW)
        return;

    /* ---- the starfield, tiled over the whole screen ---- */
    limeEnableAlphaBlending_Basic();
    limeSet2DDrawing();
    for (y = 0; y < limeScreenHeight; y += SMOKEEARTH_TILE)
        for (x = 0; x < limeScreenWidth; x += SMOKEEARTH_TILE)
            limeDrawSprite((TEXTURE *)SmokeStarFieldTexture,
                           (float)x, (float)y,
                           (float)SMOKEEARTH_TILE, (float)SMOKEEARTH_TILE,
                           0.0f, 0.0f, 1.0f, 1.0f, col);

    if (DoSmokesEarthFatal < SMOKEEARTH_BOOM) {
        /* ---- the earth, centred, shaking once it is past 240 ---- */
        if (DoSmokesEarthFatal > SMOKEEARTH_SHAKE)
            shake = ((long)DoSmokesEarthFatal & 1) ? SMOKEEARTH_SHAKE_PX
                                                   : -SMOKEEARTH_SHAKE_PX;
        else
            shake = 0;

        limeDrawSprite((TEXTURE *)SmokeEarthTexture,
                       (float)(limeScreenWidth  / 2 - SMOKEEARTH_HALF + shake),
                       (float)(limeScreenHeight / 2 - SMOKEEARTH_HALF),
                       SMOKEEARTH_SIZE, SMOKEEARTH_SIZE,
                       0.0f, 0.0f, 1.0f, 1.0f, col);
    } else {
        /* ---- the explosion, and the one-shot bang ---- */
        if (DoSmokeEarthFatalSFX == 0 && Settings[3] != 0) {
            long snd;

            DoSmokeEarthFatalSFX = 1;
            snd = get_tsound(1);
            if (snd != -1)
                limePlaySound(snd, MusicVol[Settings[3]] / 100.0f, 1.0f, 0);
        }

        limeEnableAlphaBlending_Additive();

        t = DoSmokesEarthFatal - SMOKEEARTH_BOOM;
        if ((long)(t / 7.0f) <= SMOKEEARTH_LAST_FRAME)
            DrawAnimAsSprite(limeScreenWidth / 2, limeScreenHeight / 2,
                             (float)(2.0 + (double)(t / 7.0f) * 0.25),
                             0x80, 0x80,
                             (long)(uintptr_t)&SmokeExplosionTexture,
                             explosion_SpriteDef, explosion_Anim,
                             0, (long)(t / 5.0f),
                             0, explosion_Anim[0] - 1, 0, col);
    }

    /* ---- the white flash, 300 to 330 ---- */
    if (DoSmokesEarthFatal >= SMOKEEARTH_BOOM
        && DoSmokesEarthFatal < SMOKEEARTH_FLASH_END) {
        limeEnableAlphaBlending_Basic();
        limeFillRect(0.0f, 0.0f,
                     (float)limeScreenWidth, (float)limeScreenHeight,
                     1.0f, 1.0f, 1.0f,
                     (SMOKEEARTH_FLASH_END - DoSmokesEarthFatal) / 30.0f);
    }
}

void Task_GameMain(void)
{
    long joy[2];
    long i;

    joy[0] = 0;
    joy[1] = 0;

    InGame = 1;

    /* a stage picked from the pause menu starts the fade that ends the round */
    if (GamePaused == 0
        && *InGameLevelSelect != *LevelSelectPtr
        && FE_FadeAdd == 0.0f)
        FE_FadeAdd = GAMEMAIN_FADE_OUT_STEP;

    /* one key holds block for both sides -- see the header */
    if (limePressed[0] & GAMEMAIN_KEY_BLOCK) {
        joy[0] = GAMEMAIN_JOY_BLOCK;
        joy[1] = GAMEMAIN_JOY_BLOCK;
    }

    FrameCount  += 1.0f / limeFPSScaleFactor;
    GameCounter += 1.0f / limeFPSScaleFactor;

    limeEnableAlphaBlending_Basic();
    limeSetColourMask(1, 1, 1, 0);
    limeSet2DDrawing();
    limeEnableAlphaBlending_Basic();

    /* ---- the intro, which owns the frame while it runs ---- */
    if (GameMode == 1) {                     /* no intro on the network */
        DoIntro           = 0;
        DidIntroThisFrame = 0;
    } else {
        DidIntroThisFrame = 0;
        if (DoIntro) {
            DidIntroThisFrame = 1;
            AnimateBG();
            LIME_UpdateEvents();
            IntroRender();
            LIME_RenderEvents(0);
            LIME_RenderEvents(1);
            IntroRender2dBits();
            if (DoIntro)
                return;
        }
    }

    MaintainLevelScenes();

    /* ---- input ---- */
    if (RoundSummary == 1) {
        /* the summary eats the frame's input and leaves it as releases */
        JoystickState = 0;
        for (i = 0; i < GAMEMAIN_BUTTONS; i++) {
            LastButtonStates[i] = ButtonStates[i];
            ButtonStates[i]     = 0;
        }
    } else if (G->field44e != 0) {
        ReadControls();
    }

    GetReal6ButtonJoyBits((int)JoystickState, (const int *)ButtonStates,
                          (Mk3Obj_t *)GameObjects, &joy[0], 0);

    if (P2Controls != 0)
        GetReal6ButtonJoyBits((int)JoystickStateP2,
                              (const int *)ButtonStatesP2,
                              (Mk3Obj_t *)((char *)GameObjects + MK3OBJ_STRIDE),
                              &joy[1], 1);

    if (GamePaused == 0 && LIME_Paused == 0) {
        if (P2Controls != 0)
            UpdateArcadeCode((int *)&joy[0], (int *)&joy[1]);
        else
            UpdateArcadeCode((int *)&joy[0], NULL);
    }

    /* ---- the world ---- */
    DeviceRenderSettings();
    LightPlayers();
    RenderGameView();

    if (DoSmokesEarthFatal != 0.0f)
        SmokeEarthFatalCutaway();

    DrawHUD();

    /* ---- karnage: top the bars up and bank what was taken off them ---- */
    if (GameMode == 3) {
        if (100 - Health[1] > 0)
            KarnageScore += (100 - Health[1]) * 100;
        Health[0]     = 100;
        Health[1]     = 100;
        G->healthBar1 = 100;
        G->healthBar2 = 100;
    }

    /* ---- the stage-fatal camera pull, computed in double ---- */
    if (DoingStageFatal) {
        DoingStageFatalBringForward =
            (float)((double)DoingStageFatalBringForward
                    + -0.05 / (double)limeFPSScaleFactor);
        if (DoingStageFatalBringForward < -1.2f)
            DoingStageFatalBringForward = -1.2f;
    }

    /* ---- the fade, and what it ends ---- */
    if (FE_FadeAdd != 0.0f) {
        if (areAchievementsViewing())
            FE_Fade += FE_FadeAdd / 10.0f * limeFPSScaleFactor;
        else
            FE_Fade += FE_FadeAdd / limeFPSScaleFactor;

        if (FE_Fade <= 0.0f) {
            if (FE_FadeAdd < 0.0f) {
                FE_Fade     = 0.0f;
                FE_FadeAdd = 0.0f;
                if (DontQuitAfterFade == 0) {
                    CurrentTask       = GAMEMAIN_TASK_DESTROY;
                    InGame            = 0;
                    GamePaused        = 0;
                    otherPlayerPaused = 0;
                }
            }
        } else if (FE_Fade >= 1.0f && FE_FadeAdd > 0.0f) {
            FE_Fade     = 1.0f;
            FE_FadeAdd = 0.0f;
        }

        /* the music rides the same fade, if it is on and asked to */
        if (Settings[2] && FadeMusicOut) {
            limeSetTuneVol((long)(FE_Fade * 100.0f));
            if (FE_Fade == 0.0f)
                FadeMusicOut = 0;
        }
    }

    if (FE_Fade != 1.0f)
        limeFillRect(0.0f, 0.0f,
                     (float)limeScreenWidth, (float)limeScreenHeight,
                     0.0f, 0.0f, 0.0f, 1.0f - FE_Fade);

    ShowDebugInfo();
    UpdateInGamePauseMenu();
}


/* ----------------------------------------------------------- RenderLevelPlayers
 *
 * armv7 0x00023ee8, 4,572 bytes.  **Complete.**
 *
 * Where the game stops being arcade logic and becomes OpenGL. It walks the
 * engine's object list **twice** and, for each object, resolves an animation
 * frame, builds a model matrix and hands it to `RenderPlayer` or
 * `LIME_RenderScene`. [RENDER-PLAYERS.md](../../docs/RENDER-PLAYERS.md) is the
 * map that was made of it before it was written; this is the transcription.
 *
 * ### The two passes, and the walk that is not a plain walk
 *
 * `mk3_who_in_front()` decides the order. Its complement is `startOneIn`, and
 * when that is set the walk visits the list as **1, 0, 2, 3, ...**: it starts
 * at `GameObjects->next`, comes back to the head after the first object, and
 * then steps **twice** to skip the one it already drew. When it is clear the
 * walk is the plain 0, 1, 2, ... one.
 *
 * `limeClearDepthBuffer()` runs on pass 0 **after the first object only**, and
 * sets `ClearedZBuffer`. That is what lets the far fighter be drawn, the depth
 * buffer wiped, and the near fighter drawn over it without sorting.
 *
 * ### The reset covers nine slots, the sweep only eight
 *
 * Before the walk, `p->f584 = 100.0f` for **nine** PLAYER records. After both
 * passes, `if (p->f584 == 100.0f) p->f5ec = -1` for **eight**. The ninth slot
 * is marked and never swept, so its `f5ec` keeps whatever the previous frame
 * left there. Both counts are built from the same 0x5f0 stride in the binary
 * (0x3570 is nine of them, 0x2f80 is eight), so this is the original's
 * off-by-one and not a reading of it.
 *
 * ### One object, in order
 *
 *      LastGObj = obj                        the walk's cursor, published
 *      side = obj->f0d & 1                   which fighter owns it
 *      slot = (int8)obj->f0d >> 1            which PLAYER record it draws into
 *      chr  = (int8)obj->f0c                 which character it looks like
 *
 * Slots 0 and 1 are the fighters; 2 and up are projectiles, and the debug
 * `printf` calls in here name them exactly that ("projectile@%d").
 *
 * ### Same character on both sides: the owner is swapped, not the model
 *
 * When both fighters picked the same character one of them has to draw from
 * the *other* record, or they would share one animation state. The test is
 *
 *      GameObjects[1].chr == GameObjects[0].chr
 *      && GameObjects[1].chr != Players[0].anim->f08
 *
 * and it fires for slot 0 and for every projectile owned by side 0. The
 * override is applied around the draw and **taken back off afterwards** --
 * `P->anim` is saved, replaced with `Players[1].anim`, drawn, restored. The
 * owner pointer is repointed too, at `GameObjects[1]`'s side rather than this
 * object's.
 *
 * ### The frame id decides almost everything
 *
 *      0x4e20            no model: skip the transform, park f584 at 100
 *      0x1b36, 0x1b37    clear the mirror bit
 *      0x1b4e .. 0x1b67  TOGGLE the mirror bit
 *      0x10aa            set SkipFrame86 for the duration of one RenderScene
 *      0x1a7e .. 0x1a80  a spear, and 0x129c .. 0x129e another three
 *      0x758             starts the stage fatality: DoSmokesEarthFatal = 0.01
 *
 * That last one is the other end of `Task_GameMain`'s cutaway: one animation
 * frame kicks the 330-tick counter off its zero, and every threshold in that
 * function follows from here.
 *
 * The spear ids write `SpearWhichTexture[which] = 1..5` in id order and take
 * `which` from **flags bit 7**, not from the slot. `SpearStartPos[which]` is
 * copied from `Players[which]`'s 3D position and `SpearEndPos[which]` from the
 * object's -- so the spear is a line from a fighter to the object.
 * `DrawSpear[which] = 1` follows every arm (0x24962): the ids only pick the
 * texture.
 *
 * ### Character 24's mirror is inverted
 *
 *      P->f540 = obj->flags & 0x10
 *      if (P->chr == 24) { if (P->f540) P->f540 = 1; P->f540 ^= 1; }
 *
 * Whatever bit the object carries, character 24 draws the other way round.
 * `RenderIntroCharacterPlayer` already recorded that this character is the only
 * one with a hardcoded mirror; this is the same fact from the gameplay side.
 *
 * ### The transform, twice
 *
 * The same six GL calls are emitted in two places -- once for the pass-0 model
 * matrix and once for every projectile -- and the only difference is where the
 * result is read back to, `f588` the first time and `f548` the second:
 *
 *      glMatrixMode(GL_MODELVIEW); glLoadIdentity();
 *      glTranslatef(P->x, P->y, P->z);
 *      glScalef(s, s, s);                  s = PlayerDefs[chr].f04 * PlayerSize
 *      glTranslatef(PlayerDefs[chr].f10 * ±2.15, 0, PlayerDefs[chr].f14 * 0.65);
 *      glRotatef(90, 1, 0, 0);
 *      if (mirrored) glScalef(-1, 1, 1);
 *
 * `+2.15` mirrored, `-2.15` not. The `glScalef(-1,1,1)` is the mirror proper,
 * and the cull face is flipped to match further down -- `GL_FRONT` when
 * mirrored, `GL_BACK` otherwise, the fifth sighting of that pairing in this
 * tree. Alongside the GL matrix a `limeMATRIX` is built for the same object:
 * `limeScaleMatrixXYZ(m, ±s, s, s)` with the translation written straight into
 * the fourth row, then `limeMatrixMult(M_Rot90, m, &P->f548)`. `M_Rot90` is set
 * up once at the top with **1.57075**, which is not pi/2.
 *
 * ### Slot 3 gets a debug print, every frame, forever
 *
 *      if (slot == 3) LIME_printf(4, "**ph arcadeXY=%f,%f,%f, otype %d\n", ...)
 *
 * No flag guards it. `LIME_printf` is an eight-byte no-op in this build, so it
 * costs three float-to-double conversions and a call -- but it is a live debug
 * print that shipped.
 *
 * ### The debug arm swallows the fighter
 *
 * `axes` is set whenever `LIME_Paused` is on, and whenever the frame lookup
 * fails (`P->f14 == -1`) -- the four `LIME_printf` variants in here are that
 * failure being reported, two for a projectile and two for a fighter. When it
 * is set the object goes down a different path: `ArcadePosTo3dPosNO_OFFSETS`
 * and `RenderAxesLines`, then `RenderPlayer(P, 0, 0)` instead of
 * `RenderPlayer(P, 0, 1)`; and when the lookup failed as well, `RenderPlayer`
 * is not called at all.
 *
 * `RenderAxesLines` is an empty function in the retail binary and
 * `RenderDebugCube` only loads a scene behind a debug flag, so on a shipped
 * device this arm draws **nothing** and the object silently vanishes. It is
 * still the arm a paused game takes.
 *
 * ### Events fire on pass 1 only, and only when the frame changed
 *
 *      P->f5e8 = P->f14 (or f51c for a fighter)
 *      if (P->f5e8 != P->f5ec) { trigger; P->f5ec = P->f5e8; followMode = 1; }
 *
 * so a scene's events fire once per frame *change*, not once per rendered
 * frame. Projectiles go through `LIME_TriggerEventsFromScene`; fighters go
 * through `LIME_TriggerEventsFromSceneOffsetIfFollowing` with **eleven**
 * arguments and three variants, chosen by the frame id:
 *
 *      0x1b12 .. 0x1b2c   the scene is P->anim's and the skin comes from the
 *                         OTHER fighter, Players[slot ^ 1].f528
 *      an override live    the scene is the override's
 *      otherwise           the scene is P->anim's
 *
 * and its last argument is 1 exactly for frames 0x1b4e..0x1b67 -- the same
 * range that toggles the mirror bit, tested a second time here.
 *
 * `AxeTrailDisallowed` is a countdown that suppresses the trigger entirely for
 * character 3, and decrements once per frame while it does.
 *
 * ### f584 is borrowed as a scratch flag
 *
 * `f584` starts at 100.0 for every slot. A fighter whose events are about to
 * fire has it set to 1.0 for the duration of the trigger and put back to 100.0
 * afterwards; an object with frame 0x4e20 parks it at 100.0 and never moves it.
 * So the sweep at the end reads three different things through one float: a
 * sentinel, a re-entrancy guard, and "did this slot draw".
 */

#define RLP_RESET_SLOTS      9          /* 0x3570 / 0x5f0 */
#define RLP_SWEEP_SLOTS      8          /* 0x2f80 / 0x5f0 -- see the header */
#define RLP_UNSET            100.0f
#define RLP_ROT90            1.57075f   /* not pi/2, and it is what shipped */
#define RLP_JAX_SQUASH_MAX   0x108

#define GOBJ_FRAME           0x08       /* int16  */
#define GOBJ_FLAGS           0x0a       /* uint16 */
#define GOBJ_CHR             0x0c       /* int8   */
#define GOBJ_WHO             0x0d       /* int8: bit 0 side, >> 1 slot */
#define GOBJ_FRAME2          0x0e       /* int16  */
#define GOBJ_STRIDE          0x10
#define GOBJ_MIRROR          0x0010     /* flags bit 4 */
#define GOBJ_NOOWNER         0x0100     /* flags bit 8 */

#define FRAME_NO_MODEL       0x4e20
#define FRAME_SKIP86         0x10aa
#define FRAME_STAGE_FATAL    0x0758
#define FRAME_UNMIRROR_LO    0x1b36     /* .. 0x1b37, clears the mirror bit */
#define FRAME_TOGGLE_LO      0x1b4e     /* .. 0x1b67, toggles it */
#define FRAME_TOGGLE_SPAN    0x19
#define FRAME_FOLLOW_LO      0x1b12     /* .. 0x1b2c, the other fighter's skin */
#define FRAME_FOLLOW_SPAN    0x1a
#define FRAME_SPEAR_LO       0x1a7e     /* .. 0x1a80 */
#define FRAME_SPEAR_SPAN     2

#define RLP_MIRROR_CHAR      24         /* the one character drawn the other way */
#define RLP_AXETRAIL_CHAR    3
#define RLP_DEBUG_SLOT       3          /* the slot with the unguarded printf */
#define RLP_STAGEFATAL_SEED  0.01f

#define ATTACH_STRIDE        0x1c20     /* 150 matrices of 48 bytes */
#define ALLFRAMES_STRIDE     0x41       /* 65 bytes, one leading byte skipped */
#define RLP_SCENE_BASE       6          /* LIME_RenderScene's first argument */

extern void  *LastGObj;                 /* 0x00150eb0 */
extern int   SkipFrame86;               /* 0x00171774, defined in RenderScene.c */
/* Not a pointer slot -- 0x00218cc4 IS the array, a 470,860-byte
 * `__DATA,__common` object. The extent and the evidence are on the declaration
 * in Players.c, which spelled the same symbol `char *` and crashed on it. */
extern char   AllFramesTable[];         /* 0x00218cc4 */

long  mk3_who_in_front(void);
long *HavePreloadedCharacter(long who);
void  RenderDebugCube(void);
void  RenderAxesLines(float x, float y, float z);
void LIME_GLAPI glLoadIdentity(void);
void  limeGetCurrentModelMatrix(float *out);
void  limeScaleMatrixXYZ(float *m, float sx, float sy, float sz);
void  LIME_TriggerEventsFromSceneOffsetIfFollowing(long slot, long follow,
                                                   void *scene, long frame,
                                                   const float *m,
                                                   const float *m2,
                                                   long mirror, long one,
                                                   void *skin, long e,
                                                   long inToggleRange);

#define RLP_NEXT(o)     (*(Mk3Obj_t **)(o))
#define RLP_PLAYER(i)   ((long *)(Players + (long)(i) * ARCADE_PLAYER_STRIDE))
#define RLP_PTR(x)      ((long *)(uintptr_t)(unsigned long)(x))
#define RLP_DEF(c)      (&PlayerDefs[c])
#define RLP_DEFNAME(c)  (PlayerDefs[c].lighting)

/* The six GL calls the transform is made of, emitted in two places. `out` is
 * where the model matrix is read back to -- f588 the first time, f548 the
 * second -- and that is the only difference between them. The caller pops. */
static void RenderLevelPlayers_Pose(const long *w, long chr, float s,
                                    int withOffset, float *out)
{
    const float *pf  = (const float *)w;
    const PLAYERDEF *def = RLP_DEF(chr);

    LIME_PushMatrix();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(pf[0x5c8 / 4], pf[0x5cc / 4], pf[0x5d0 / 4]);
    glScalef(s, s, s);

    if (withOffset)
        glTranslatef(def->renderOffsetX * (w[0x540 / 4] ? 2.15f : -2.15f),
                     0.0f,
                     def->renderOffsetZ * 0.65f);

    glRotatef(90.0f, 1.0f, 0.0f, 0.0f);

    if (w[0x540 / 4])
        glScalef(-1.0f, 1.0f, 1.0f);        /* the mirror proper */

    limeGetCurrentModelMatrix(out);
}

void RenderLevelPlayers(void)
{
    float     mtx[16];                  /* sp+0x7c */
    Mk3Obj_t *obj;
    long      pass, n, startOneIn, followMode;
    long      i;

    if (DoingSKDeath)
        return;
    if (GameObjects == NULL)
        return;

    DrawSpear[0] = 0;
    DrawSpear[1] = 0;

    if (JaxBeingSquashed) {
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        if (JaxGrowCounter <= RLP_JAX_SQUASH_MAX)
            RenderPlayer((PLAYER *)JaxSquashedPlayer, 0, 0);
    }

    RotMatrixX(M_Rot90, RLP_ROT90);

    if (JadeStomachShaker((PLAYER *)RLP_PLAYER(0))
        || JadeStomachShaker((PLAYER *)RLP_PLAYER(1)))
        limeClearDepthBuffer();

    for (i = 0; i < RLP_RESET_SLOTS; i++)
        ((float *)RLP_PLAYER(i))[0x584 / 4] = RLP_UNSET;

    startOneIn = mk3_who_in_front() ^ 1;
    followMode = 0;

    for (pass = 0; pass < 2; pass++) {
        obj = (Mk3Obj_t *)GameObjects;
        if (startOneIn)
            obj = RLP_NEXT(obj);
        n = 0;

        while (obj != NULL) {
            const unsigned char  *ob = (const unsigned char *)obj;
            const signed char    *os = (const signed char *)obj;
            unsigned short       *ou = (unsigned short *)obj;
            const short          *oi = (const short *)obj;
            const signed char    *g0 = (const signed char *)GameObjects;
            const unsigned char  *g0u = (const unsigned char *)GameObjects;

            long   slot, side, chr, frame;
            long   axes, ovrActive, flags100;
            long  *ovr, *owner, *w;
            float *att, *pf;
            float  s16;

            LastGObj = obj;

            side = ob[GOBJ_WHO] & 1;
            slot = (long)os[GOBJ_WHO] >> 1;
            chr  = os[GOBJ_CHR];

            w     = RLP_PLAYER(slot);
            pf    = (float *)w;
            att   = AttachTransforms + slot * (ATTACH_STRIDE / 4);
            owner = RLP_PLAYER(side);

            /* ---- both fighters picked the same character ---- */
            ovr       = NULL;
            ovrActive = 0;
            if ((slot == 0 || ((unsigned long)slot > 1 && side == 0))
                && g0[GOBJ_STRIDE + GOBJ_CHR] == g0[GOBJ_CHR]
                && g0[GOBJ_STRIDE + GOBJ_CHR]
                   != RLP_PTR(RLP_PLAYER(0)[4 / 4])[8 / 4]) {
                ovr       = RLP_PTR(RLP_PLAYER(1)[4 / 4]);
                ovrActive = 1;
                owner     = RLP_PLAYER(g0u[GOBJ_STRIDE + GOBJ_WHO] & 1);
            }

            ArcadePosTo3dPos(obj, &pf[0x5c8 / 4], &os[GOBJ_CHR]);

            flags100 = ou[GOBJ_FLAGS / 2] & GOBJ_NOOWNER;

            if (slot > 1) {
                if (flags100) {
                    /* the binary writes obj->flags straight back here -- a
                     * dead store; the effect is skipping the line below */
                    ou[GOBJ_FLAGS / 2] = ou[GOBJ_FLAGS / 2];
                } else {
                    w[4 / 4] = owner[4 / 4];
                }
            }

            /* ---- the tint triple, from the flags' low nibble ---- */
            w[0x534 / 4] = 0;
            w[0x538 / 4] = 0;
            w[0x53c / 4] = 0;
            switch (ou[GOBJ_FLAGS / 2] & 0xf) {
            case 1:  w[0x534 / 4] = 1; break;
            case 2:  w[0x53c / 4] = 1; break;
            case 4:  w[0x538 / 4] = 1; break;
            default: break;
            }

            glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

            frame = ou[GOBJ_FRAME / 2];

            if (frame == FRAME_NO_MODEL) {
                /* no model: the frame lookup and P->chr are skipped entirely */
                axes = 0;
            } else {
                if ((unsigned short)(frame - FRAME_UNMIRROR_LO) <= 1)
                    ou[GOBJ_FLAGS / 2] =
                        (unsigned short)(ou[GOBJ_FLAGS / 2] & ~GOBJ_MIRROR);
                if ((unsigned short)(frame - FRAME_TOGGLE_LO) <= FRAME_TOGGLE_SPAN)
                    ou[GOBJ_FLAGS / 2] =
                        (unsigned short)(ou[GOBJ_FLAGS / 2] ^ GOBJ_MIRROR);

                /* the frame goes through the object's own animation record, or
                 * the owner's when the override is live */
                if (w[4 / 4] != 0 && !ovrActive)
                    w[0x14 / 4] = ((const short *)RLP_PTR(RLP_PTR(w[4 / 4])
                                   [0x2c / 4]))[oi[GOBJ_FRAME / 2]];
                else
                    w[0x14 / 4] = ((const short *)RLP_PTR(RLP_PTR(owner[4 / 4])
                                   [0x2c / 4]))[oi[GOBJ_FRAME / 2]];

                w[0] = chr;

                if (LIME_Paused) {
                    axes = 1;
                } else {
                    const char *name =
                        AllFramesTable + frame * ALLFRAMES_STRIDE + 1;

                    axes = 1;
                    if (w[0x14 / 4] == -1) {
                        if (slot > 1)
                            LIME_printf(5, "   --projectile@%d: %s (!NOTFOUND!)"
                                           "owned by p%d,chartype=%s\n",
                                        slot, name, side, RLP_DEFNAME(chr));
                        else
                            LIME_printf(5, "p%d: %s (!NOTFOUND!)char=%s",
                                        slot, name, RLP_DEFNAME(chr));
                    } else if (slot > 1) {
                        LIME_printf(5, "   --projectile@%d: %s (fr%d)"
                                       "owned by p%d,chartype=%s\n",
                                    slot, name, w[0x14 / 4], side,
                                    RLP_DEFNAME(chr));
                    } else {
                        if (slot == 1)
                            LIME_printf(5, " ");
                        LIME_printf(5, "p%d: %s (fr%d)char=%s(p%d)",
                                    slot, name, w[0x14 / 4],
                                    RLP_DEFNAME(chr), side);
                    }
                }
            }

            /* ---- spears, and the stage fatality's starting gun ---- */
            if (pass == 0) {
                long f2 = oi[GOBJ_FRAME2 / 2];

                if ((unsigned short)(f2 - FRAME_SPEAR_LO) <= FRAME_SPEAR_SPAN
                    || f2 == 0x129c || f2 == 0x129d || f2 == 0x129e) {
                    long   which = (ou[GOBJ_FLAGS / 2] >> 7) & 1;
                    float *sp    = (float *)RLP_PLAYER(which);

                    ArcadePosTo3dPosNO_OFFSETS(obj, SpearEndPos[which]);
                    SpearStartPos[which][0] = sp[0x5c8 / 4];
                    SpearStartPos[which][1] = sp[0x5cc / 4];
                    SpearStartPos[which][2] = sp[0x5d0 / 4];
                    SpearWhichTexture[which] = 0;

                    if (f2 == 0x1a7f)      SpearWhichTexture[which] = 1;
                    else if (f2 == 0x1a80) SpearWhichTexture[which] = 2;
                    else if (f2 == 0x129c) SpearWhichTexture[which] = 3;
                    else if (f2 == 0x129d) SpearWhichTexture[which] = 4;
                    else if (f2 == 0x129e) SpearWhichTexture[which] = 5;
                    /* every arm comes back to 0x24962: the spear is drawn on
                     * all five ids, not only when none matched */
                    DrawSpear[which] = 1;
                }

                if (f2 == FRAME_STAGE_FATAL && DoSmokesEarthFatal == 0.0f)
                    DoSmokesEarthFatal = RLP_STAGEFATAL_SEED;
            }

            /* ---- the mirror bit, and the one character that inverts it ---- */
            w[0x540 / 4] = ou[GOBJ_FLAGS / 2] & GOBJ_MIRROR;
            if (w[0] == RLP_MIRROR_CHAR) {
                if (w[0x540 / 4])
                    w[0x540 / 4] = 1;
                w[0x540 / 4] ^= 1;
            }

            s16 = RLP_DEF(chr)->scale;

            /* ---- pass 0 builds the lime matrix and the GL model matrix ---- */
            if (pass == 0 && w[0x14 / 4] != -1) {
                float s = s16 * PlayerSize;

                limeMatrixLoadIdentity(mtx);
                mtx[12] = pf[0x5c8 / 4];
                mtx[13] = pf[0x5cc / 4];
                mtx[14] = pf[0x5d0 / 4];

                limeScaleMatrixXYZ(mtx, w[0x540 / 4] ? -s : s, s, s);
                limeMatrixMult(M_Rot90, mtx, &pf[0x548 / 4]);

                RenderLevelPlayers_Pose(w, chr, s,
                                        frame != FRAME_NO_MODEL,
                                        &pf[0x588 / 4]);
                LIME_PopMatrix(1);
            }

            /* ---- a projectile takes its animation state from its owner ---- */
            if (slot > 1) {
                long *pre;

                w[4 / 4] = owner[4 / 4];
                if (w[0x534 / 4] == 0 && w[0x538 / 4] == 0 && w[0x53c / 4] == 0)
                    w[0x528 / 4] = owner[0x528 / 4];
                w[0x530 / 4] = owner[0x530 / 4];

                pre = HavePreloadedCharacter(w[0]);
                if (pre != NULL) {
                    w[4 / 4]     = (long)(uintptr_t)pre;
                    w[0x528 / 4] = pre[0x14 / 4];
                    w[0x530 / 4] = pre[0x14 / 4];
                    w[0x14 / 4]  = ((const short *)RLP_PTR(pre[0x2c / 4]))
                                   [oi[GOBJ_FRAME / 2]];
                }

                w[0x51c / 4] = w[0x14 / 4];
                w[0x520 / 4] = w[0x14 / 4];
                w[0x524 / 4] = 0;
            }

            /* ---- draw it ---- */
            if (axes == 0) {
                if (pass == 0) {
                    RenderPlayer((PLAYER *)w, 0, 1);
                    memcpy(att, MatrixPalette2, ATTACH_STRIDE);
                }
            } else if (w[0x14 / 4] == -1) {
                /* both of these are empty in the retail binary, so nothing at
                 * all is drawn for this object */
                ArcadePosTo3dPosNO_OFFSETS(obj, mtx);
                RenderAxesLines(mtx[0], mtx[1], mtx[2]);
                axes = 0;
            } else {
                long saved;

                ArcadePosTo3dPosNO_OFFSETS(obj, mtx);
                RenderAxesLines(mtx[0], mtx[1], mtx[2]);

                if (DoingStageFatal != 0 && DoingStageFatal - 1 == slot)
                    glTranslatef(0.0f, DoingStageFatalBringForward, 0.0f);

                saved = w[4 / 4];
                if (ovrActive)
                    w[4 / 4] = (long)(uintptr_t)ovr;
                if (pass == 0) {
                    RenderPlayer((PLAYER *)w, 0, 0);
                    memcpy(att, MatrixPalette2, ATTACH_STRIDE);
                }
                if (ovrActive)
                    w[4 / 4] = saved;
            }

            /* ---- a projectile gets its own model matrix ---- */
            if (slot > 1) {
                if (frame == FRAME_NO_MODEL) {
                    pf[0x584 / 4] = RLP_UNSET;
                } else {
                    RenderLevelPlayers_Pose(w, chr, s16 * PlayerSize, 1,
                                            &pf[0x548 / 4]);
                    if (slot == RLP_DEBUG_SLOT)
                        LIME_printf(4, "**ph arcadeXY=%f,%f,%f, otype %d\n",
                                    (double)pf[0x578 / 4],
                                    (double)pf[0x57c / 4],
                                    (double)pf[0x580 / 4], chr);
                    LIME_PopMatrix(1);
                }
            }

            /* ---- the debug arm: cube, matrix, scene ---- */
            if (axes != 0) {
                RenderDebugCube();          /* the call site passes &P->f548,
                                             * which the callee never reads */
                LIME_PushMatrix();
                glMultMatrixf(&pf[0x548 / 4]);
                glCullFace(w[0x540 / 4] ? GL_FRONT : GL_BACK);

                if (slot <= 1) {
                    SkipFrame86 = 0;
                    if (oi[GOBJ_FRAME / 2] == FRAME_SKIP86)
                        SkipFrame86 = 1;
                    LIME_RenderScene(slot + RLP_SCENE_BASE,
                                     RLP_PTR(RLP_PTR(owner[4 / 4])[0x10 / 4]),
                                     w[0x51c / 4], w[0x520 / 4], pf[0x524 / 4],
                                     0, 0, pass,
                                     RLP_PTR(w[0x528 / 4]), w[0x52c / 4], att);
                    SkipFrame86 = 0;
                } else if (flags100 == 0) {
                    LIME_RenderScene(slot + RLP_SCENE_BASE,
                                     RLP_PTR(RLP_PTR(owner[4 / 4])[0x10 / 4]),
                                     w[0x14 / 4], w[0x14 / 4], 0.0f,
                                     flags100, flags100, pass,
                                     RLP_PTR(w[0x528 / 4]), w[0x52c / 4], att);
                }
                LIME_PopMatrix(1);
            }

            /* ---- pass 1 fires the scene's events, once per frame CHANGE ---- */
            if (pass == 1) {
                w[0x5e8 / 4] = (slot > 1) ? w[0x14 / 4] : w[0x51c / 4];

                if (w[0x5e8 / 4] != w[0x5ec / 4]) {
                    if (slot > 1) {
                        LIME_TriggerEventsFromScene(
                            RLP_PTR(RLP_PTR(owner[4 / 4])[0x10 / 4]),
                            w[0x14 / 4], &pf[0x548 / 4], w[0x540 / 4],
                            -1, 1, w[0x528 / 4], w[0x52c / 4]);
                    } else {
                        long mirror  = w[0x540 / 4];
                        long fr      = ou[GOBJ_FRAME / 2];
                        long hadFull = 0;
                        long toggled;
                        int  suppressed = 0;

                        if (pf[0x584 / 4] == RLP_UNSET) {
                            pf[0x584 / 4] = 1.0f;
                            hadFull = 1;
                        }

                        toggled = ((unsigned short)(fr - FRAME_TOGGLE_LO)
                                   <= FRAME_TOGGLE_SPAN) ? 1 : 0;

                        if (w[0] == RLP_AXETRAIL_CHAR && AxeTrailDisallowed != 0) {
                            AxeTrailDisallowed--;   /* the trail is held off */
                            suppressed = 1;
                        }

                        if (!suppressed) {
                            long *scene;
                            long  skin;

                            if ((unsigned short)(fr - FRAME_FOLLOW_LO)
                                <= FRAME_FOLLOW_SPAN) {
                                scene = RLP_PTR(RLP_PTR(w[4 / 4])[0x10 / 4]);
                                skin  = RLP_PLAYER(slot ^ 1)[0x528 / 4];
                            } else if (ovr != NULL) {
                                scene = RLP_PTR(ovr[0x10 / 4]);
                                skin  = w[0x528 / 4];
                            } else {
                                scene = RLP_PTR(RLP_PTR(w[4 / 4])[0x10 / 4]);
                                skin  = w[0x528 / 4];
                            }

                            LIME_TriggerEventsFromSceneOffsetIfFollowing(
                                slot, followMode, scene, w[0x51c / 4],
                                &pf[0x548 / 4], &pf[0x588 / 4],
                                mirror, 1, RLP_PTR(skin), w[0x52c / 4],
                                toggled);
                        }

                        if (hadFull)
                            pf[0x584 / 4] = RLP_UNSET;
                    }

                    w[0x5ec / 4] = w[0x5e8 / 4];
                    followMode   = 1;
                }

                if (axes == 0)
                    w[0x5ec / 4] = -1;
            }

            glCullFace(GL_BACK);

            if (slot == 1) {
                if (!LIME_Paused)
                    LIME_printf(5, "\n");
                followMode = slot;
            }

            /* ---- pick the next object ---- */
            if (!startOneIn) {
                obj = RLP_NEXT(obj);
            } else if (n == 0) {
                obj = (Mk3Obj_t *)GameObjects;      /* back to the head */
            } else if (n == 1) {
                obj = RLP_NEXT(obj);                /* the one already drawn */
                if (obj != NULL)
                    obj = RLP_NEXT(obj);
            } else {
                obj = RLP_NEXT(obj);
            }

            if (pass == 0 && n == 0) {
                limeClearDepthBuffer();
                ClearedZBuffer = 1;
            }
            if (obj == NULL)
                break;
            n++;
        }
    }

    /* the reset above covered nine slots; this sweep covers eight */
    for (i = 0; i < RLP_SWEEP_SLOTS; i++)
        if (((const float *)RLP_PLAYER(i))[0x584 / 4] == RLP_UNSET)
            RLP_PLAYER(i)[0x5ec / 4] = -1;

    DoSmokesSmoke(PLAYER1MODEL, PLAYER2MODEL);
}


/* ------------------------------------------------------ UpdateInGamePauseMenu
 *
 * armv7 0x00026c84, 5,720 bytes.  **Complete.**
 *
 * The pause menu, drawn and driven from inside `Task_GameMain`. `GamePaused`
 * is a three-state variable, not a flag, and it selects which of three screens
 * this function is:
 *
 *      0   not paused          -> TogglePauseMenu() and nothing else
 *      1   the menu            -> six touch options
 *      2   the moves list      -> MovesList() under a CANCEL button
 *
 * ### Six options, all at the same x, 56 apart
 *
 *      y=6    GameText(0xf9)   the heading, and option 1 -- RESUME
 *      y=62   GameText(9)      EXIT (asks first)
 *      y=118  "%s : %s"        control layout, from Settings[4]
 *      y=174  "%s : %s"        the second layout line, from Settings[5]
 *      y=230  "%s : %s"        MUSIC, from Settings[2]
 *      y=286  "%s : %s"        SFX,   from Settings[3]
 *
 * all at x=384, scale 1.25, wrapped at `FE_W(186)`. **The heading is a live
 * button** -- `GameText(0xf9)` is drawn with `DrawOptionAsButton` like the
 * rest, so tapping the title resumes the game.
 *
 * ### Every option fires on RELEASE, and only on a real release
 *
 *      LastTouch_PauseN = Touch_PauseN;
 *      Touch_PauseN     = DrawOptionAsButton(...);
 *      if (LastTouch_PauseN && !Touch_PauseN && limeTouchScreenX[0] == -1.0f)
 *
 * -- the finger has to have been inside last frame, be outside now, **and** be
 * off the screen entirely. Sliding off an option does not activate it. The
 * highlight colour is `mmfontcol[(LastTouch + 1) * 4]`, so it lags the touch by
 * a frame.
 *
 * ### The selection is carried in one variable that starts as GameMode
 *
 * `sl` holds `GameMode` while the network check runs, is then overwritten with
 * 0 and set to 1..5 by whichever option was released. Option 6 does not set it
 * -- it branches straight into its own handler -- which leaves the `sl == 6`
 * test in the dispatcher **unreachable**. Transcribed as written.
 *
 * ### The click is played before the action, and only for options 1 to 5
 *
 *      if (Settings[3]) limePlaySound(SFXHandle[0x68/4],
 *                                     MusicVol[Settings[3]] / 100, 1.0f, 0);
 *
 * `Settings[3]` is the SFX setting and doubles as the enable, the same shape
 * `Task_GameDestroy` uses for music. Option 6 -- the SFX option itself --
 * bypasses this and plays its click *after* changing the setting, so turning
 * SFX on clicks and turning it off does not.
 *
 * ### What each option does
 *
 *      1  RESUME     GamePaused = 0, and three sendPause(0) on the network
 *      2  EXIT       clears four flags, QuitAsLose(), then FIVE analytics
 *                    events -- one for the menu and one per game mode
 *      3  BUTTONS    Settings[4]++, wrapping 7 back to 5, then
 *                    mk3_set_four_button for both sides
 *      4  LAYOUT     GameMode 6: Player2NumButtons++, wrapping 7 to 5
 *                    otherwise:  Settings[5] ^= 1
 *      5  MUSIC      a four-state cycle; see below
 *      6  SFX        Settings[3] = (Settings[3] + 1) & 3
 *
 * Options 3, 4 and 6 call `Write_SettingsData()`; option 5 calls it only on the
 * branch that does not find user music. **Turning the music off because the
 * device is playing its own does not persist** -- `limeCheckForUserMusic()`
 * sets `Settings[2] = 0` and returns without saving.
 *
 * ### The music option changes the volume for the state it is leaving
 *
 *      Settings[2] == 3   limeStopTune()
 *      Settings[2] == 0   limePlayTune(LevelMusic[LevelSelect], MusicVol[1], 1)
 *      Settings[2] == 1   limeSetTuneVol(MusicVol[2])
 *      Settings[2] == 2   limeSetTuneVol(MusicVol[3])
 *      then Settings[2] = (Settings[2] + 1) & 3
 *
 * so the volume applied is `MusicVol[Settings[2] + 1]` -- the level it is about
 * to move to, applied before the move. The compiler wrote this as a fall-
 * through cascade that re-reads `Settings[2]` after each arm; since none of the
 * calls touch it, exactly one arm runs.
 *
 * ### The moves list is dismissed by a corner, not a button
 *
 * With `GamePaused == 2` the CANCEL sprite is drawn at the top right, scaled by
 * `HUD_Scale`, and the dismiss test is a raw rectangle rather than a hit-tested
 * option:
 *
 *      last touch x > limeScreenWidth - HUD_Scale * 80
 *      last touch y < HUD_Scale * 80
 *
 * on the frame the finger comes off. The sprite is 36 units square and the hit
 * box is 80 -- more than twice the drawn size in each direction.
 *
 * ### The network game has two extra screens
 *
 * When `GameMode == 1` and `otherPlayerPaused` is set the local player gets a
 * cut-down panel: `GameText(0x3bb)` as a label and `GameText(9)` as the only
 * option, which arms `PauseMenuAreYouSure`. The confirm itself --
 * `GameText(0x11b)` over `GameText(0xeb)`/`GameText(0xec)` -- appears in
 * **four** places in the binary, once per (paused state x network) combination,
 * each with its own copy of the "INGAME PAUSE MENU"/"EXIT" strings. They are
 * written out here the same way, because the four are not quite identical: the
 * two network copies call `QuitAsLose()` directly on confirm, and the two local
 * ones set the selection and let the dispatcher do it.
 */

#define PAUSE_X              384.0f
#define PAUSE_SCALE          1.25f
#define PAUSE_WIDTH          186.0f
#define PAUSE_ROW0           6.0f
#define PAUSE_ROW_PITCH      56.0f
#define PAUSE_CONFIRM_LABEL  48.0f
#define PAUSE_CONFIRM_YES    176.0f
#define PAUSE_CONFIRM_NO     240.0f
#define PAUSE_WAIT_LABEL     112.0f

#define PAUSE_PANEL_X        272.0f
#define PAUSE_PANEL_Y        (-32.0f)
#define PAUSE_PANEL_W        256.0f
#define PAUSE_PANEL_H        384.0f

#define CANCEL_SIZE          36.0f     /* HUD_Scale units */
#define CANCEL_X_OFF         (-32.0f)
#define CANCEL_Y_OFF         (-6.0f)
/* the hit box, more than twice the art; 40 in the iPad build (0x27276) */
#define CANCEL_HIT           (PortIPad ? 40.0f : 80.0f)

#define PAUSE_SFX_CLICK      (0x68 / 4)
#define PAUSE_BUTTONS_WRAP   7         /* 7 wraps back to 5 */
#define PAUSE_BUTTONS_LOW    5

#define TXT_PAUSE_TITLE      0xf9
#define TXT_EXIT             9
#define TXT_LAYOUT_A         0xe4
#define TXT_LAYOUT_B         0xe5
#define TXT_MUSIC            0xdc
#define TXT_SFX              0xdd
#define TXT_VOL_LOW          0xde
#define TXT_VOL_MED          0xdf
#define TXT_VOL_HIGH         0xe0
#define TXT_VOL_OFF          0xe2
#define TXT_ARE_YOU_SURE     0x11b
#define TXT_YES              0xeb
#define TXT_NO               0xec
#define TXT_OTHER_PAUSED     0x3bb
#define TXT_BUTTONS_P1       0x3f3
#define TXT_BUTTONS_P2       0x3f4
#define TXT_LAYOUT_NAME      0x111
#define TXT_LAYOUT_CUSTOM    0x122

#define EV_PAUSE_MENU        0xc360
#define EV_PAUSE_SETTING     0xc35f
#define EV_ARCADE_QUIT       0x754d
#define EV_TRAINING_QUIT     0x7551
#define EV_VERSUS_QUIT       0x7557

extern float  mmfontcol[];              /* pointer slot -> 0x000ff854 */
extern char   Menu_GamePaused_Options[];/* 0x00150eb8 */
extern long   Touch_Pause1, Touch_Pause2, Touch_Pause3;   /* 0x00151038.. */
extern long   Touch_Pause4, Touch_Pause5, Touch_Pause6;
extern long   LastTouch_Pause1, LastTouch_Pause2, LastTouch_Pause3;
extern long   LastTouch_Pause4, LastTouch_Pause5, LastTouch_Pause6;
extern const char *TrainingNames[];     /* pointer slot -> 0x00176718 */
extern long   TrainingCatagory;         /* pointer slot -> 0x0017809c */

void  Write_SettingsData(void);
long  DrawOptionAsText(const char *text, float x, float y, float scale,
                       const float *colour, float maxWidth);
long  DrawOptionAsButton(const char *text, float x, float y, float scale,
                         const float *colour, float maxWidth);

#define PAUSE_COL(last)   (&mmfontcol[((last) + 1) * 4])
#define PAUSE_RELEASED(last, now) \
    ((last) != 0 && (now) == 0 && limeTouchScreenX[0] == -1.0f)

/* The panel behind every one of these screens: the right half of the 512-square
 * page, so u is 0.5 and the extents are 0.5 by 0.75. */
static void DrawPausePanel(void)
{
    limeDrawSprite((TEXTURE *)PauseBGTexture,
                   FE_X(PAUSE_PANEL_X), FE_Y(PAUSE_PANEL_Y),
                   FE_W(PAUSE_PANEL_W), FE_H(PAUSE_PANEL_H),
                   0.5f, 0.0f, 0.5f, 0.75f, col);
}

/* The click every option but the SFX one plays before it acts. */
static void PauseClick(void)
{
    if (Settings[3])
        limePlaySound(SFXHandle[PAUSE_SFX_CLICK],
                      MusicVol[Settings[3]] / 100.0f, 1.0f, 0);
}

void UpdateInGamePauseMenu(void)
{
    char text[256];                     /* sp+0x18, to the top of the frame */
    long sel;                           /* the option released this frame */

    /* ---- not paused ---- */
    if (GamePaused == 0) {
        TogglePauseMenu();
        return;
    }

    /* ---- the moves list ---- */
    if (GamePaused == 2) {
        sel = GameMode;

        if (GameMode == 1 && otherPlayerPaused != 0) {
            if (PauseMenuAreYouSure != 0) {
                /* the network confirm, drawn over the moves list */
                DrawPausePanel();

                DrawOptionAsText(GameText(TXT_ARE_YOU_SURE),
                                 PAUSE_X, PAUSE_CONFIRM_LABEL,
                                 PAUSE_SCALE, &mmfontcol[4],
                                 FE_W(PAUSE_WIDTH));

                LastTouch_Pause2 = Touch_Pause2;
                Touch_Pause2 = DrawOptionAsButton(GameText(TXT_YES),
                                                  PAUSE_X, PAUSE_CONFIRM_YES,
                                                  PAUSE_SCALE,
                                                  PAUSE_COL(LastTouch_Pause2),
                                                  FE_W(PAUSE_WIDTH));
                if (PAUSE_RELEASED(LastTouch_Pause2, Touch_Pause2))
                    QuitAsLose();

                LastTouch_Pause3 = Touch_Pause3;
                Touch_Pause3 = DrawOptionAsButton(GameText(TXT_NO),
                                                  PAUSE_X, PAUSE_CONFIRM_NO,
                                                  PAUSE_SCALE,
                                                  PAUSE_COL(LastTouch_Pause3),
                                                  FE_W(PAUSE_WIDTH));
                if (PAUSE_RELEASED(LastTouch_Pause3, Touch_Pause3)) {
                    PauseMenuAreYouSure = 0;
                    PauseClick();
                    EASDK_LogEventEnumEnumString(EV_PAUSE_MENU, 15,
                                                 "INGAME PAUSE MENU",
                                                 15, "EXIT");
                }
                return;
            }

            /* waiting on the other player */
            DrawPausePanel();

            LastTouch_Pause2 = Touch_Pause2;
            Touch_Pause2 = DrawOptionAsText(GameText(TXT_OTHER_PAUSED),
                                            PAUSE_X, PAUSE_WAIT_LABEL,
                                            PAUSE_SCALE,
                                            PAUSE_COL(LastTouch_Pause2),
                                            FE_W(PAUSE_WIDTH));

            LastTouch_Pause4 = Touch_Pause4;
            Touch_Pause4 = DrawOptionAsButton(GameText(TXT_EXIT),
                                              PAUSE_X, PAUSE_CONFIRM_NO,
                                              PAUSE_SCALE,
                                              PAUSE_COL(LastTouch_Pause4),
                                              FE_W(PAUSE_WIDTH));
            if (PAUSE_RELEASED(LastTouch_Pause4, Touch_Pause4))
                PauseMenuAreYouSure = sel;      /* GameMode, i.e. 1 */
            return;
        }

        /* ---- CANCEL, then the list itself ---- */
        limeEnableAlphaBlending_Additive();
        limeDrawSprite((TEXTURE *)CancelTexture,
                       (float)limeScreenWidth + HUD_Scale * CANCEL_X_OFF,
                       HUD_Scale * CANCEL_Y_OFF,
                       HUD_Scale * CANCEL_SIZE, HUD_Scale * CANCEL_SIZE,
                       0.0f, 0.0f, 1.0f, 1.0f, col);
        limeEnableAlphaBlending_Basic();
        MovesList();

        /* the corner is dismissed on the frame the finger comes off */
        if (limeLastTouchScreenX[0] == -1.0f)
            return;
        if (limeTouchScreenX[0] != -1.0f)
            return;
        if (limeLastTouchScreenX[0]
            <= (float)limeScreenWidth - HUD_Scale * CANCEL_HIT)
            return;
        if (HUD_Scale * CANCEL_HIT <= limeLastTouchScreenY[0])
            return;

        PauseClick();
        GamePaused = 0;
        if (GameMode == 1) {
            sendPause(0);
            sendPause(0);
            sendPause(0);
        }
        return;
    }

    /* ---- GamePaused == 1: the menu ---- */
    sel = GameMode;

    if (GameMode == 1 && otherPlayerPaused != 0) {
        if (PauseMenuAreYouSure != 0) {
            DrawPausePanel();

            DrawOptionAsText(GameText(TXT_ARE_YOU_SURE),
                             PAUSE_X, PAUSE_CONFIRM_LABEL,
                             PAUSE_SCALE, &mmfontcol[4],
                             FE_W(PAUSE_WIDTH));

            LastTouch_Pause2 = Touch_Pause2;
            Touch_Pause2 = DrawOptionAsButton(GameText(TXT_YES),
                                              PAUSE_X, PAUSE_CONFIRM_YES,
                                              PAUSE_SCALE,
                                              PAUSE_COL(LastTouch_Pause2),
                                              FE_W(PAUSE_WIDTH));
            if (PAUSE_RELEASED(LastTouch_Pause2, Touch_Pause2))
                QuitAsLose();

            LastTouch_Pause3 = Touch_Pause3;
            Touch_Pause3 = DrawOptionAsButton(GameText(TXT_NO),
                                              PAUSE_X, PAUSE_CONFIRM_NO,
                                              PAUSE_SCALE,
                                              PAUSE_COL(LastTouch_Pause3),
                                              FE_W(PAUSE_WIDTH));
            if (PAUSE_RELEASED(LastTouch_Pause3, Touch_Pause3)) {
                PauseMenuAreYouSure = 0;
                PauseClick();
                EASDK_LogEventEnumEnumString(EV_PAUSE_MENU, 15,
                                             "INGAME PAUSE MENU", 15, "EXIT");
            }
            return;
        }

        DrawPausePanel();

        LastTouch_Pause2 = Touch_Pause2;
        Touch_Pause2 = DrawOptionAsText(GameText(TXT_OTHER_PAUSED),
                                        PAUSE_X, PAUSE_WAIT_LABEL,
                                        PAUSE_SCALE,
                                        PAUSE_COL(LastTouch_Pause2),
                                        FE_W(PAUSE_WIDTH));

        LastTouch_Pause4 = Touch_Pause4;
        Touch_Pause4 = DrawOptionAsButton(GameText(TXT_EXIT),
                                          PAUSE_X, PAUSE_CONFIRM_NO,
                                          PAUSE_SCALE,
                                          PAUSE_COL(LastTouch_Pause4),
                                          FE_W(PAUSE_WIDTH));
        if (PAUSE_RELEASED(LastTouch_Pause4, Touch_Pause4))
            PauseMenuAreYouSure = sel;
        return;
    }

    /* ---- the two option strings the rows share ---- */
    usprintf(Menu_GamePaused_Options + 0xc0, UC("%d"), Settings[4]);

    if (GameMode == 6)
        usprintf(Menu_GamePaused_Options + 0x100, UC("%d"), Player2NumButtons);
    else if (Settings[5] == 0)
        usprintf(Menu_GamePaused_Options + 0x100, UC("%s"),
                 GameTextNoHeader(TXT_LAYOUT_CUSTOM));
    else
        usprintf(Menu_GamePaused_Options + 0x100, UC("%s"),
                 GameTextNoHeader(TXT_LAYOUT_NAME));

    DrawPausePanel();

    /* ---- the local confirm ---- */
    if (PauseMenuAreYouSure != 0) {
        DrawOptionAsText(GameText(TXT_ARE_YOU_SURE),
                         PAUSE_X, PAUSE_CONFIRM_LABEL,
                         PAUSE_SCALE, &mmfontcol[4], FE_W(PAUSE_WIDTH));

        LastTouch_Pause2 = Touch_Pause2;
        Touch_Pause2 = DrawOptionAsButton(GameText(TXT_YES),
                                          PAUSE_X, PAUSE_CONFIRM_YES,
                                          PAUSE_SCALE,
                                          PAUSE_COL(LastTouch_Pause2),
                                          FE_W(PAUSE_WIDTH));
        sel = PAUSE_RELEASED(LastTouch_Pause2, Touch_Pause2) ? 2 : 0;

        LastTouch_Pause3 = Touch_Pause3;
        Touch_Pause3 = DrawOptionAsButton(GameText(TXT_NO),
                                          PAUSE_X, PAUSE_CONFIRM_NO,
                                          PAUSE_SCALE,
                                          PAUSE_COL(LastTouch_Pause3),
                                          FE_W(PAUSE_WIDTH));
        if (PAUSE_RELEASED(LastTouch_Pause3, Touch_Pause3)) {
            PauseMenuAreYouSure = 0;
            PauseClick();
        }
    } else {
        /* ---- row 1: the heading, and it is a button ---- */
        LastTouch_Pause1 = Touch_Pause1;
        Touch_Pause1 = DrawOptionAsButton(GameText(TXT_PAUSE_TITLE),
                                          PAUSE_X, PAUSE_ROW0, PAUSE_SCALE,
                                          PAUSE_COL(LastTouch_Pause1),
                                          FE_W(PAUSE_WIDTH));
        sel = PAUSE_RELEASED(LastTouch_Pause1, Touch_Pause1) ? 1 : 0;

        /* ---- row 2: EXIT, which arms the confirm ---- */
        LastTouch_Pause2 = Touch_Pause2;
        Touch_Pause2 = DrawOptionAsButton(GameText(TXT_EXIT),
                                          PAUSE_X,
                                          PAUSE_ROW0 + PAUSE_ROW_PITCH,
                                          PAUSE_SCALE,
                                          PAUSE_COL(LastTouch_Pause2),
                                          FE_W(PAUSE_WIDTH));
        if (PAUSE_RELEASED(LastTouch_Pause2, Touch_Pause2)) {
            PauseClick();
            PauseMenuAreYouSure = 1;
        }

        /* ---- row 3: the first layout line ---- */
        LastTouch_Pause3 = Touch_Pause3;
        if (GameMode == 6)
            usprintf(text, UC("%s - %s : %s"),
                     GameTextNoHeader(TXT_BUTTONS_P1),
                     GameTextNoHeader(TXT_LAYOUT_A),
                     Menu_GamePaused_Options + 0xc0);
        else
            usprintf(text, UC("%s : %s"),
                     GameTextNoHeader(TXT_LAYOUT_A),
                     Menu_GamePaused_Options + 0xc0);
        Touch_Pause3 = DrawOptionAsButton(limeUC(text), PAUSE_X,
                                          PAUSE_ROW0 + 2 * PAUSE_ROW_PITCH,
                                          PAUSE_SCALE,
                                          PAUSE_COL(LastTouch_Pause3),
                                          FE_W(PAUSE_WIDTH));
        if (PAUSE_RELEASED(LastTouch_Pause3, Touch_Pause3))
            sel = 3;

        /* ---- row 4: the second layout line ---- */
        LastTouch_Pause4 = Touch_Pause4;
        if (GameMode == 6)
            usprintf(text, UC("%s - %s : %s"),
                     GameTextNoHeader(TXT_BUTTONS_P2),
                     GameTextNoHeader(TXT_LAYOUT_A),
                     Menu_GamePaused_Options + 0x100);
        else
            usprintf(text, UC("%s : %s"),
                     GameTextNoHeader(TXT_LAYOUT_B),
                     Menu_GamePaused_Options + 0x100);
        Touch_Pause4 = DrawOptionAsButton(limeUC(text), PAUSE_X,
                                          PAUSE_ROW0 + 3 * PAUSE_ROW_PITCH,
                                          PAUSE_SCALE,
                                          PAUSE_COL(LastTouch_Pause4),
                                          FE_W(PAUSE_WIDTH));
        if (PAUSE_RELEASED(LastTouch_Pause4, Touch_Pause4))
            sel = 4;

        /* ---- row 5: MUSIC.  The fourth argument is one too many for the
         * format and is never consumed -- it is in the binary all four ways. */
        LastTouch_Pause5 = Touch_Pause5;
        switch (Settings[2]) {
        case 1:  usprintf(text, UC("%s : %s"), GameText(TXT_MUSIC),
                          GameText(TXT_VOL_LOW),
                          Menu_GamePaused_Options + 0x100); break;
        case 2:  usprintf(text, UC("%s : %s"), GameText(TXT_MUSIC),
                          GameText(TXT_VOL_MED),
                          Menu_GamePaused_Options + 0x100); break;
        case 3:  usprintf(text, UC("%s : %s"), GameText(TXT_MUSIC),
                          GameText(TXT_VOL_HIGH),
                          Menu_GamePaused_Options + 0x100); break;
        default: usprintf(text, UC("%s : %s"), GameText(TXT_MUSIC),
                          GameText(TXT_VOL_OFF),
                          Menu_GamePaused_Options + 0x100); break;
        }
        Touch_Pause5 = DrawOptionAsButton(limeUC(text), PAUSE_X,
                                          PAUSE_ROW0 + 4 * PAUSE_ROW_PITCH,
                                          PAUSE_SCALE,
                                          PAUSE_COL(LastTouch_Pause5),
                                          FE_W(PAUSE_WIDTH));
        if (PAUSE_RELEASED(LastTouch_Pause5, Touch_Pause5))
            sel = 5;

        /* ---- row 6: SFX, which jumps straight to its own handler ---- */
        LastTouch_Pause6 = Touch_Pause6;
        switch (Settings[3]) {
        case 1:  usprintf(text, UC("%s : %s"), GameText(TXT_SFX),
                          GameText(TXT_VOL_LOW),
                          Menu_GamePaused_Options + 0x100); break;
        case 2:  usprintf(text, UC("%s : %s"), GameText(TXT_SFX),
                          GameText(TXT_VOL_MED),
                          Menu_GamePaused_Options + 0x100); break;
        case 3:  usprintf(text, UC("%s : %s"), GameText(TXT_SFX),
                          GameText(TXT_VOL_HIGH),
                          Menu_GamePaused_Options + 0x100); break;
        default: usprintf(text, UC("%s : %s"), GameText(TXT_SFX),
                          GameText(TXT_VOL_OFF),
                          Menu_GamePaused_Options + 0x100); break;
        }
        Touch_Pause6 = DrawOptionAsButton(limeUC(text), PAUSE_X,
                                          PAUSE_ROW0 + 5 * PAUSE_ROW_PITCH,
                                          PAUSE_SCALE,
                                          PAUSE_COL(LastTouch_Pause6),
                                          FE_W(PAUSE_WIDTH));
        if (PAUSE_RELEASED(LastTouch_Pause6, Touch_Pause6)) {
            /* the SFX option acts here rather than through `sel`, so it plays
             * its click AFTER the change instead of before it */
            Settings[3] = (Settings[3] + 1) & 3;
            if (Settings[3] != 0)
                limePlaySound(SFXHandle[PAUSE_SFX_CLICK],
                              MusicVol[Settings[3]] / 100.0f, 1.0f, 0);
            Write_SettingsData();
            EASDK_LogEventEnumEnumString(EV_PAUSE_SETTING, 15,
                                         "INGAME PAUSE MENU", 15, "SFX");
            return;
        }
    }

    /* ---- act on the selection ---- */
    if (sel == 0)
        return;

    PauseClick();

    if (sel == 1) {                             /* RESUME */
        GamePaused = 0;
        if (GameMode == 1) {
            sendPause(0);
            sendPause(0);
            sendPause(0);
        }
        EASDK_LogEventEnumEnumString(EV_PAUSE_MENU, 15,
                                     "INGAME PAUSE MENU", 15, "RESUME");
        return;
    }

    if (sel == 2) {                             /* EXIT */
        GamePaused          = 0;
        PauseMenuAreYouSure = 0;
        RoundHasEnded       = 0;
        DontQuitAfterFade   = 0;
        QuitAsLose();

        EASDK_LogEvent(EV_PAUSE_MENU, 15, "INGAME PAUSE MENU", 15, "EXIT");

        if (GameMode == 0)
            EASDK_LogEvent(EV_ARCADE_QUIT, 15, DestinyNames[Destiny],
                           15, getStageName(Destiny, Stage));
        else if (GameMode == 1)
            EASDK_LogEvent(EV_VERSUS_QUIT, 0, NULL, 0, NULL);
        else if (GameMode == 2)
            EASDK_LogEvent(EV_TRAINING_QUIT, 15,
                           TrainingNames[TrainingCatagory], 0, NULL);
        else if (GameMode == 6)
            EASDK_LogEvent(EV_VERSUS_QUIT, 15, "2 Players on 1 iPad", 0, NULL);
        return;
    }

    if (sel == 3) {                             /* NUMBER OF BUTTONS */
        Settings[4]++;
        if (Settings[4] == PAUSE_BUTTONS_WRAP)
            Settings[4] = PAUSE_BUTTONS_LOW;

        EASDK_LogEventEnumEnumString(EV_PAUSE_SETTING, 15,
                                     "INGAME PAUSE MENU",
                                     15, "NUMBER OF BUTTONS");

        Player1NumButtons = Settings[4];
        Write_SettingsData();
        mk3_set_four_button(0, Player1NumButtons != 6);
        mk3_set_four_button(1, Player2NumButtons != 6);
        return;
    }

    if (sel == 4) {                             /* LAYOUT */
        if (GameMode == 6) {
            Player2NumButtons++;
            if (Player2NumButtons == PAUSE_BUTTONS_WRAP)
                Player2NumButtons = PAUSE_BUTTONS_LOW;
            mk3_set_four_button(0, Player1NumButtons != 6);
            mk3_set_four_button(1, Player2NumButtons != 6);
        } else {
            Settings[5] ^= 1;
            Write_SettingsData();
        }
        EASDK_LogEventEnumEnumString(EV_PAUSE_SETTING, 15,
                                     "INGAME PAUSE MENU", 15, "LAYOUT");
        return;
    }

    if (sel == 5) {                             /* MUSIC */
        if (limeCheckForUserMusic()) {
            /* the device is playing its own music -- and this is NOT saved */
            Settings[2] = 0;
        } else {
            switch (Settings[2]) {
            case 3: limeStopTune(); break;
            case 0: limePlayTune(LevelMusic[*LevelSelectPtr],
                                 (long)MusicVol[1], 1); break;
            case 1: limeSetTuneVol((long)MusicVol[2]); break;
            case 2: limeSetTuneVol((long)MusicVol[3]); break;
            default: break;
            }
            Settings[2] = (Settings[2] + 1) & 3;
            Write_SettingsData();
        }
        EASDK_LogEventEnumEnumString(EV_PAUSE_SETTING, 15,
                                     "INGAME PAUSE MENU", 15, "MUSIC");
        return;
    }
}
