/*
 * Players.c — src/gamecode/Players.cpp
 *
 * 28 functions in the original. Verified against the oracle by
 * tests/test_gamecode2_diff.c.
 */

#include <stdint.h>
#include "lime_glapi.h"   /* the GL calls' calling convention */
#include <string.h>   /* memcpy, strcmp, strcpy */
#include <stdio.h>    /* sprintf */
#include <stdlib.h>   /* malloc, free: the bakes kept */
#include "../../runtime/smooth_models.h"   /* port: SMOOTH MODELS */
#include "../../runtime/skin_mats.h"       /* port: a modded skin's textures */
int rgl_shadow_active(void);            /* port: REAL-TIME SHADOWS, render/rgl.h */

typedef struct TEXTURE TEXTURE;
void limeDeleteTexture(TEXTURE *tex);

/* Only the one field this file touches is named. */
typedef struct PLAYER {
    uint8_t  _pad000[0x530];
    TEXTURE *P32 altCostume;         /* 0x530 */
} PLAYER;

void DumpAltCostume(PLAYER *p);


/* ----------------------------------------------------------- DumpAltCostume
 *
 * armv7 0x0005bef0, 24 bytes.
 *
 * Frees the alternate-costume texture and clears the pointer. The clear is
 * inside the guard, so a player who never had one is left alone rather than
 * written to -- and a second call after the first is a no-op rather than a
 * double free. Both of those fall out of `cbz` skipping the store as well as
 * the call, which is easy to lose when the guard is rewritten as an early
 * return.
 */
void DumpAltCostume(PLAYER *p)
{
    if (p->altCostume != NULL) {
        limeDeleteTexture(p->altCostume);
        p->altCostume = NULL;
    }
}


/* Declared here rather than pulled from a header gamecode does not have. */
extern long WantedFrames[];             /* 0x00295cd4, terminated by -1 */
int printf(const char *fmt, ...);


/* --------------------------------------------------------------------- Error
 *
 * armv7 0x0005c334, 20 bytes.  **Complete.**
 *
 *      mov r1, r0
 *      ldr r0, ="!!!!! FATAL ERROR: %s\n"
 *      blx _printf
 *      b   .                       <- 0x5c342 branches to ITSELF
 *
 * **It does not return, and it does not exit.** The last instruction is a
 * branch to its own address: a deliberate hang, with the message already
 * printed. On a 2011 iPhone that is a frozen screen and a watchdog kill, which
 * is a reasonable thing to do when there is no console to read.
 *
 * A port must not quietly turn this into a return. Anything calling Error()
 * has decided the process is finished, and callers are written on that
 * assumption -- there is no error path after the call because the original
 * never comes back from it.
 */
void Error(const char *msg)
{
    printf("!!!!! FATAL ERROR: %s\n", msg);
    for (;;)
        ;                           /* b 0x5c342 -- the branch is to itself */
}


/* ------------------------------------------------------------- WantThisFrame
 *
 * armv7 0x0005b9b4, 40 bytes.  **Complete.**
 *
 * Linear search of `_WantedFrames` for `frame`, with -1 ending the list.
 * Returns 1 on a hit and 0 otherwise.
 *
 * The disassembly reads oddly because the compiler peeled the first iteration:
 * the entry test loads [r2] and jumps straight into the comparison, and only
 * the loop body advances by 4 and re-checks for the terminator. Written as a
 * plain loop below, which is the same thing.
 */
int WantThisFrame(long frame)
{
    long i;

    for (i = 0; WantedFrames[i] != -1; i++)
        if (WantedFrames[i] == frame)
            return 1;
    return 0;
}


/* ----------------------------------------------------------- IsOstrichProblem
 *
 * armv7 0x0005bb1c, 40 bytes.  **Complete.**
 *
 *      ldr  r3, [r0]
 *      cmp  r3, #0xc               <- character 12 and no other
 *      bne  return 0
 *      ldr  r3, [r0, #0x51c]
 *      movw r2, #0x127             <- 295
 *      cmp  r3, r2
 *      beq  return 1
 *      ldr  r0, [r0, #0x520]
 *      cmp  r0, r2
 *      -> r0 = (equal)
 *
 * Character 12 with either of two adjacent fields equal to 295. The name is the
 * developers': something about that character's animation misbehaves and this
 * is the test that spots it. Which character 12 is, and what 295 indexes, are
 * not established here -- the shape of the test is, and inventing names for the
 * two fields would be inventing the finding.
 */
int IsOstrichProblem(PLAYER *p)
{
    const long *w = (const long *)p;

    if (w[0] != 12)
        return 0;
    if (w[0x51c / 4] == 295)
        return 1;
    return w[0x520 / 4] == 295;
}


/* `_FrameRemapTable` — 0x002003d4. Pairs of words, and the loop below gives its
 * length: it runs from the base to base + 0xe268 in steps of 8, so 7,245
 * entries of two words. */
#define FRAME_REMAP_ENTRIES  7245

extern long FrameRemapTable[FRAME_REMAP_ENTRIES][2];    /* 0x002003d4 */


/* ------------------------------------------------------- ClearAnimRemapTables
 *
 * armv7 0x0005b8dc, 40 bytes.  **Complete.**
 *
 *      movs r3, #0
 *      str  r3, [r2, #-0x8]        <- first word: 0
 *      subs r3, #1
 *      str  r3, [r2, #-0x4]        <- second word: -1
 *      adds r2, #8
 *
 * **The two words are cleared to different values**, and that is the point: 0
 * in the first and -1 in the second. `subs r3, #1` on a register holding zero
 * is how the compiler produced the -1 without a second literal, which is easy
 * to read past.
 *
 * A memset would put zero in both and quietly change what an empty entry means.
 */
void ClearAnimRemapTables(void)
{
    int i;

    for (i = 0; i < FRAME_REMAP_ENTRIES; i++) {
        FrameRemapTable[i][0] = 0;
        FrameRemapTable[i][1] = -1;
    }
}


typedef int EPLAYER;
typedef struct FRONTEND_CHARACTER FRONTEND_CHARACTER;
void Preload1Character(EPLAYER who, FRONTEND_CHARACTER *fe, long a, long b);


/* ------------------------------------------------------ PreloadGameCharacters
 *
 * armv7 0x0005ce74, 40 bytes.  **Complete.**
 *
 * Walks a -1 terminated list of player ids and preloads each one, passing NULL
 * for the front-end character and 0 for the third argument, with the caller's
 * argument going through unchanged as the fourth.
 *
 * `ldr r0, [r4, #4]!` is the advance: pre-indexed with writeback, so the
 * terminator check at the bottom sees the NEXT entry and the loop needs no
 * separate increment.
 */
void PreloadGameCharacters(const EPLAYER *list, long arg)
{
    long i;

    for (i = 0; list[i] != -1; i++)
        Preload1Character(list[i], 0, 0, arg);
}


/* **26, measured rather than assumed.** `_TheFECharacters` runs from 0x0020e634
 * to the next common symbol `_AllFramesTable` at 0x00218cc4 -- 0xa690 bytes,
 * which is 26 * 0x668 exactly. This said 25 until AnimateFECharacters was read:
 * its loop ends when the cursor reaches base + 0xa028, and the compare sits
 * AFTER the body, so slot 25 is processed. With 25 slots that would have been a
 * one-entry overrun in shipping code; with 26 it is simply the last entry. */
#define FE_CHARACTER_SLOTS  26
#define FE_CHARACTER_STRIDE 0x668

typedef struct ANIMATEDCHARACTER {
    long        meshCount;      /* 0x00 */
    void       *P32 meshes;         /* 0x04  meshCount records of 0x58 */
    long        id;             /* 0x08  the character, for the frame gates */
    long        field0c;        /* 0x0c  nothing reads it */
    void       *P32 scene;          /* 0x10 */
    void       *P32 diffuse;        /* 0x14 */
    void       *P32 diffuseIce;     /* 0x18 */
    void       *P32 jadeGreen;      /* 0x1c  JADE only */
    void       *P32 sindelHair;     /* 0x20  SINDEL only */
    void       *P32 diffuse2;       /* 0x24  the lite path's second sheet */
    void       *P32 babality;       /* 0x28  may stay NULL */
    void       *P32 frameList;      /* 0x2c */
    void       *P32 skin;           /* 0x30 */
    void       *P32 bones;          /* 0x34 */
    const long *P32 meshbase;       /* 0x38  kept so it can be freed */
    const long *P32 meshTable;      /* 0x3c */
    const long *P32 meshNames;      /* 0x40 */
} ANIMATEDCHARACTER;            /* 0x44 = 68 bytes, the size limeMalloc asks */

int  FreeAnimatedCharacter(ANIMATEDCHARACTER *c);
void LIME_FreeScene(void *scene);

extern char TheFECharacters[FE_CHARACTER_SLOTS][FE_CHARACTER_STRIDE];  /* 0x0020e634 */


/* ------------------------------------------------------ FreeFrontEndCharacters
 *
 * armv7 0x0005bff8, 60 bytes.  **Complete.**
 *
 * Twenty-five slots of 0x668 bytes -- the loop runs from the base to base +
 * 0xa028 in steps of 0x668, which is where both numbers come from.
 *
 * Per slot, the character pointer lives at +4:
 *
 *      if (!c) continue
 *      if (c->scene) { LIME_FreeScene(c->scene); c->scene = 0; }
 *      FreeAnimatedCharacter(c)
 *
 * **The pointer is re-read from the slot after the scene is freed** (`ldr r2,
 * [r4, #4]` and again `ldr r0, [r4, #4]`), rather than being kept in a
 * register across the call. Transcribed as written: LIME_FreeScene taking a
 * path that could touch the slot is not something this function assumes it
 * cannot.
 */
void FreeFrontEndCharacters(void)
{
    int i;

    for (i = 0; i < FE_CHARACTER_SLOTS; i++) {
        void *P32 *slot = (void *P32 *)(TheFECharacters[i] + 4);
        void *P32 *c = (void *P32 *)*slot;

        if (c == 0)
            continue;

        if (c[0x10 / 4] != 0) {
            LIME_FreeScene(c[0x10 / 4]);
            ((void *P32 *)*slot)[0x10 / 4] = 0;
        }
        FreeAnimatedCharacter((ANIMATEDCHARACTER *)*slot);
    }
}


typedef struct PLAYERDEF {
    long        id;             /* 0x00  0..25; the same as the index */
    float       scale;          /* 0x04  multiplied by PlayerSize */
    float       posOffsetX;     /* 0x08  added or subtracted by facing */
    float       height;         /* 0x0c  added to Z in ArcadePosTo3dPos */
    float       renderOffsetX;  /* 0x10  glTranslatef x, times +/-2.15 */
    float       renderOffsetZ;  /* 0x14  glTranslatef z, times 0.65 */
    const char *P32 lighting;       /* 0x18  "KANO" */
    const char *P32 frameList;      /* 0x1c  "kanoframes.txt" */
    const char *P32 bones;          /* 0x20  "KANO_STANDARD.bones" */
    const char *P32 skin;           /* 0x24  "KANO_STANDARD.skin" */
    const char *P32 skinAnim;       /* 0x28  "KANO_STANDARD.skinanim" */
    const char *P32 texBase;        /* 0x2c  "KANO" */
    const char *P32 scene;          /* 0x30  "KANO_STANDARD.scene" */
} PLAYERDEF;

/* 52 bytes an entry IN THE IMAGE, where a pointer is four bytes. Kept because
 * the compiler spells the multiply out as `(n*16 - n*4 + n) << 2` and that is
 * how the stride was read off in the first place. The host struct is wider and
 * nothing should use this number to index it. */
void Load1Character(PLAYER *p, EPLAYER who, FRONTEND_CHARACTER *fe, long a, long b);


/* ---------------------------------------------------------- LoadGameCharacter
 *
 * armv7 0x0005cd70, 48 bytes.  **Complete.**
 *
 *      Load1Character(p, def[0], fe, 0, arg)
 *      p->[0x5e8] = 0
 *      p->[0]     = def[0]
 *      p->[0x5ec] = -1
 *      p->[0x530] = 0
 *
 * **`def[0]` is re-read from memory after the call** rather than kept in a
 * register, so Load1Character is allowed to change it and the value stored into
 * the player is whatever it holds afterwards. Caching it would be a different
 * program.
 *
 * +0x5ec starts at -1, the same "nobody" sentinel GameCodeInit uses for the
 * camera. +0x530 is the alt-costume texture DumpAltCostume frees, cleared here
 * so a freshly loaded character never inherits the previous one's.
 */
void LoadGameCharacter(PLAYER *p, const PLAYERDEF *def,
                       FRONTEND_CHARACTER *fe,
                       long arg)
{
    long *pw = (long *)p;
    const long *dw = (const long *)def;

    Load1Character(p, (EPLAYER)dw[0], fe, 0, arg);

    pw[0x5e8 / 4] = 0;
    pw[0]         = dw[0];          /* re-read, not cached across the call */
    pw[0x5ec / 4] = -1;
    pw[0x530 / 4] = 0;              /* the alt costume DumpAltCostume frees */
}


#define PRELOAD_STRIDE 0x5f4

extern int  NumPreloadedCharacters;     /* 0x00171358 */
extern char PreloadedCharacters[][PRELOAD_STRIDE];  /* 0x0028bc10 */
int IsAFrameVisible(ANIMATEDCHARACTER *c, long frame);


/* -------------------------------------------------------------- IsFrameVisible
 *
 * armv7 0x0005ba54, 68 bytes.  **Complete.**
 *
 *      if (c->field8 == 12 && (a == 295 || b == 295))
 *          return 0
 *      return IsAFrameVisible(c, b) | IsAFrameVisible(c, a)
 *
 * **The same 12 and 295 that IsOstrichProblem tests**, in a different file and
 * for a different purpose: that one asks whether a player is in the broken
 * state, this one refuses to draw the frame. Two functions independently
 * special-casing the same character and the same id says 295 is a real frame
 * that character 12 must not show, not a coincidence of constants.
 *
 * Note the argument order in the tail call: `b` is tested first, then `a`. The
 * OR makes it irrelevant to the result and it is transcribed as written.
 */
int IsFrameVisible(ANIMATEDCHARACTER *c, long a, long b)
{
    if (((const long *)c)[2] == 12 && (a == 295 || b == 295))
        return 0;

    return IsAFrameVisible(c, b) | IsAFrameVisible(c, a);
}


/* ------------------------------------------------- FreePreloadedCharacters
 *
 * armv7 0x0005c038, 56 bytes.  **Complete.**
 *
 * Frees the character at +8 of each of `NumPreloadedCharacters` slots, stride
 * 0x5f4, then zeroes the count.
 *
 * **The count is re-read from memory every iteration** (`mov r3, r6; add r3,
 * pc; ldr r3, [r3]`) rather than held in a register, so FreeAnimatedCharacter
 * is allowed to change it and the loop would notice. Hoisting it is the obvious
 * optimisation and it is a different program.
 */
void FreePreloadedCharacters(void)
{
    int i;

    if (NumPreloadedCharacters == 0)
        return;

    for (i = 0; i != NumPreloadedCharacters; i++)
        FreeAnimatedCharacter(*(ANIMATEDCHARACTER *P32 *)
                              (PreloadedCharacters[i] + 8));

    NumPreloadedCharacters = 0;
}


/* --------------------------------------------------- HavePreloadedCharacter
 *
 * armv7 0x0005b90c, 84 bytes.  **Complete.**
 *
 * Linear search of the preloaded slots for one whose first word is `who`,
 * returning the ANIMATEDCHARACTER at +8 of that slot, or NULL.
 *
 * The stride is built out of shifts rather than a multiply: `i*12`, then
 * `(i*12) << 7` for `i*1536`, then a subtract for `i*1524` -- 0x5f4, the same
 * stride FreePreloadedCharacters walks.
 *
 * The compiler peeled the first comparison, which is why slot 0 is tested
 * before the loop and the loop body starts at slot 1.
 */
ANIMATEDCHARACTER *HavePreloadedCharacter(EPLAYER who)
{
    int i;

    for (i = 0; i < NumPreloadedCharacters; i++)
        if (*(const long *)PreloadedCharacters[i] == (long)who)
            return *(ANIMATEDCHARACTER *P32 *)(PreloadedCharacters[i] + 8);

    return 0;
}


#define PLAYER_STRIDE  0x5f0

extern ANIMATEDCHARACTER *OrigLoadedPlayers[2];  /* 0x00295ccc */
extern char Players[];                           /* 0x001fa4d4 */


/* --------------------------------------------------------- FreeLevelCharacters
 *
 * armv7 0x0005c080, 68 bytes.  **Complete.**
 *
 *      FreePreloadedCharacters()
 *      if (OrigLoadedPlayers[0]) FreeAnimatedCharacter(it)
 *      if (OrigLoadedPlayers[1]) FreeAnimatedCharacter(it)
 *      OrigLoadedPlayers[1] = 0
 *      OrigLoadedPlayers[0] = 0                <- cleared in that order
 *      if (Players[0xb20]) DumpAltCostume(&Players[0x5f0])
 *
 * **The last test cross-checks two numbers from elsewhere in the file.** It
 * reads `Players + 0xb20` and, if set, calls DumpAltCostume on `Players +
 * 0x5f0`. 0xb20 - 0x5f0 = 0x530, which is exactly the field LoadGameCharacter
 * clears and DumpAltCostume frees. So the PLAYER stride is 0x5f0 and the alt
 * costume lives at +0x530 -- three functions in agreement, none of which
 * mentions the other.
 *
 * Only player TWO is checked. Player one's alt costume is not dumped here.
 */
void FreeLevelCharacters(void)
{
    FreePreloadedCharacters();

    if (OrigLoadedPlayers[0])
        FreeAnimatedCharacter(OrigLoadedPlayers[0]);
    if (OrigLoadedPlayers[1])
        FreeAnimatedCharacter(OrigLoadedPlayers[1]);

    OrigLoadedPlayers[1] = 0;
    OrigLoadedPlayers[0] = 0;

    /* player two only: 0x5f0 + 0x530 = 0xb20 */
    if (*(const long *)(Players + PLAYER_STRIDE + 0x530) != 0)
        DumpAltCostume((PLAYER *)(Players + PLAYER_STRIDE));
}


extern int *DoIntroPtr;                 /* pointer slot -> 0x0014e1c0 */
extern int *CurrentTaskPtr;             /* pointer slot -> 0x00150590 */


/* ------------------------------------------------------------- IsLiaProblem
 *
 * armv7 0x0005ba98, 132 bytes.  **Complete.**
 *
 *      if (DoIntro)          return 0
 *      if (CurrentTask == 3) return 0
 *      if (p->field00 != 6)  return 0
 *      return  p->field51c is one of {0xf1, 0x101, 0x105, 0xf8, 0xf9}
 *           || p->field520 is one of the same five
 *
 * **The same shape as IsOstrichProblem, one character over.** That one tests
 * character 12 against a single id; this tests character 6 against five, in the
 * same two adjacent fields at +0x51c and +0x520, and IsFrameVisible tests the
 * pair again from a third file. Four functions, three files, one pattern: a
 * character id plus an animation id, checked in two neighbouring slots.
 *
 * The two extra guards are this one's own. It gives up during the intro and
 * during task 3, so whatever "the Lia problem" is, it is a fight-time artefact
 * and the code that asks about it runs in both places.
 *
 * The compiler built each set of comparisons as `ite` pairs OR-ed together
 * rather than a switch, which is why the disassembly reads as arithmetic.
 */
int IsLiaProblem(PLAYER *p)
{
    const long *w = (const long *)p;
    long a, b;

    if (*DoIntroPtr != 0)
        return 0;
    if (*CurrentTaskPtr == 3)
        return 0;
    if (w[0] != 6)
        return 0;

    a = w[0x51c / 4];
    if (a == 0xf1 || a == 0x105 || a == 0xf8 || a == 0xf9 || a == 0x101)
        return 1;

    b = w[0x520 / 4];
    return (b == 0xf1 || b == 0x105 || b == 0xf8 || b == 0xf9 || b == 0x101);
}


/* ---------------------------------------------------- CreateFramesWeWantFromFIDs
 *
 * armv7 0x0005b960, 84 bytes.  **Complete.**
 *
 *      n = 0
 *      while (fids[n] != -1) {
 *          v = 0
 *          if (FrameRemapTable[fids[n]][0] == who
 *              && FrameRemapTable[fids[n]][1] != -1)
 *              v = FrameRemapTable[fids[n]][1]
 *          out[n] = v
 *          n++
 *      }
 *      out[n] = -1
 *      return n
 *
 * Translates a -1 terminated list of frame IDs into the frames THIS character
 * actually has, writing 0 where it has none.
 *
 * **Both halves of the remap pair are checked, and 0 is the miss value while -1
 * is the terminator.** ClearAnimRemapTables fills the table with exactly that
 * pair -- 0 in the first word, -1 in the second -- so an entry nobody has
 * claimed fails the first test and an entry claimed but not resolved fails the
 * second. Two functions in two files agreeing on what an empty slot looks like.
 *
 * The output list is always terminated, including when the input was empty:
 * the -1 store is past the loop, not inside it.
 */
long CreateFramesWeWantFromFIDs(long *out, EPLAYER who, const long *fids)
{
    long n = 0;

    while (fids[n] != -1) {
        long fid = fids[n];
        long v = 0;

        if (FrameRemapTable[fid][0] == (long)who &&
            FrameRemapTable[fid][1] != -1)
            v = FrameRemapTable[fid][1];

        out[n] = v;
        n++;
    }
    out[n] = -1;                        /* always, even for an empty input */
    return n;
}


typedef struct limeVECTOR3 limeVECTOR3;


typedef struct limeVECTOR2 limeVECTOR2;
typedef struct SKININFO SKININFO;
typedef struct BONESINFO BONESINFO;

extern limeVECTOR2   *RenderUVs;        /* slot -> 0x003498c8 */
extern unsigned char *RenderRGBs;       /* slot -> 0x00366d88 */

void GenerateMatrices(char *dst, BONESINFO *bones, long a, long b, float t,
                      long arg);
/* Returns a byte count. It was declared `void` here until LoadAnimatedCharacter
 * was read and turned out to use the result -- and `mov r0, r4` does sit
 * immediately before the epilogue at 0x00060f58. The same trap as the seven
 * other wrong types in this project: a signature written from a call site that
 * happened to ignore what came back. */
long DrawSkinnedMesh2(SKININFO *skin, unsigned a, unsigned b, long c,
                      limeVECTOR3 *out, limeVECTOR2 *uv, unsigned char *rgb,
                      long d, long e);


/* ------------------------------------- GenerateFrameVertsBySkinning / ...SKIN2
 *
 * armv7 0x0005c118 and 0x0005c0d8, 84 and 64 bytes.  **Complete.**
 *
 * Two thin wrappers over DrawSkinnedMesh2, sharing the same three globals --
 * `_RenderUVs` and `_RenderRGBs` for the output arrays -- and differing in one
 * thing: **the plain version poses the skeleton first and SKIN2 does not.**
 *
 *      GenerateFrameVertsBySkinning:
 *          GenerateMatrices(c->[0x3c], c->[0x34], a, b, t, c->[0x40])
 *          DrawSkinnedMesh2(c->[0x30], 0, 0, 0, out, RenderUVs, RenderRGBs,
 *                           c->[4][0], 0)
 *
 *      GenerateFrameVertsBySkinningSKIN2:
 *          DrawSkinnedMesh2(*(c->[0x30]), 0, 0, 0, out, RenderUVs, RenderRGBs,
 *                           c->[4][2], 0)
 *
 * Two differences beyond that, and both are easy to read past:
 *
 *  - SKIN2 **dereferences** +0x30 one level further (`ldr r3, [r0,#0x30]` then
 *    `ldr r2, [r3]`) where the plain one passes it straight through. So +0x30
 *    holds a pointer to a pointer for the second skin and a pointer for the
 *    first, or the two skins live at different depths of the same field.
 *
 *  - the eighth argument comes from `c->[4][0]` in one and `c->[4][2]` in the
 *    other -- offsets 0 and 8 of the same sub-structure.
 *
 * A port that shares one body between them has to keep both differences, and
 * neither is visible from the names.
 */
void GenerateFrameVertsBySkinning(ANIMATEDCHARACTER *c, long a, long b,
                                  float t, limeVECTOR3 *out)
{
    char *P32 *w = (char *P32 *)c;
    const long *n = (const long *)c;

    GenerateMatrices(w[0x3c / 4], (BONESINFO *)w[0x34 / 4], a, b, t,
                     n[0x40 / 4]);

    DrawSkinnedMesh2((SKININFO *)w[0x30 / 4], 0, 0, 0, out,
                     RenderUVs, RenderRGBs,
                     ((const long *)w[4 / 4])[0], 0);
}

void GenerateFrameVertsBySkinningSKIN2(ANIMATEDCHARACTER *c, long a, long b,
                                       float t, limeVECTOR3 *out)
{
    char *P32 *w = (char *P32 *)c;

    (void)a;
    (void)b;
    (void)t;                            /* no pose: SKIN2 skips GenerateMatrices */

    DrawSkinnedMesh2(*(SKININFO *P32 *)w[0x30 / 4], 0, 0, 0, out,
                     RenderUVs, RenderRGBs,
                     ((const long *)w[4 / 4])[2], 0);
}


void limeFree(void *p);


#define ANIM_ENTRY_STRIDE 0x58

void  LIME_FreeSkin(void *skin);
void  LIME_FreeBones(void *bones);
void  LIME_FreeMeshSetTextures(void *set);
void  LIME_FreeScene(void *scene);
int   LIME_SceneExists(void *scene);


/* ------------------------------------------------------- FreeAnimatedCharacter
 *
 * armv7 0x0005bf08, 240 bytes.  **Complete.**
 *
 * Two loops' worth of frees, and three details make it worth reading rather
 * than skimming.
 *
 * **The per-entry loop runs n-1 times, not n.** `subs r3, #1; cmp r3, r6; bne`
 * -- so the LAST entry of the array is never processed, and when n == 1 the
 * loop is skipped entirely by the `cmp r3, #1; beq` at the top. Whatever the
 * final entry is, it is not owned by this character.
 *
 * **Four of the six per-entry frees happen only on entry ZERO.** +0x1c, +0x20,
 * +0x2c and +0x30 are guarded by `cbnz r6` on the loop index; +0x24 and +0x34
 * are freed for every entry. So two of the pointers are shared across entries
 * and only the first one owns them.
 *
 * **The entry base is re-read from `c->[4]` after every free.** Not cached in a
 * register across the calls, which is transcribed as written: limeFree is
 * allowed to move it.
 *
 * The scene is freed only if LIME_SceneExists agrees -- reference counting, the
 * same the loader does on the way in -- and its meshset textures go first.
 *
 * Returns 1 unconditionally. No caller in this tree checks it.
 */
int FreeAnimatedCharacter(ANIMATEDCHARACTER *c)
{
    char  *base = (char *)c;
    long   n    = *(const long *)base;
    long   i;

    for (i = 0; n != 1 && i < n - 1; i++) {
        char *e;

        e = *(char *P32 *)(base + 4) + i * ANIM_ENTRY_STRIDE;
        if (*(void *P32 *)(e + 0x24)) limeFree(*(void *P32 *)(e + 0x24));

        if (i == 0) {                   /* entry zero owns these two */
            e = *(char *P32 *)(base + 4) + i * ANIM_ENTRY_STRIDE;
            if (*(void *P32 *)(e + 0x1c)) limeFree(*(void *P32 *)(e + 0x1c));
            e = *(char *P32 *)(base + 4) + i * ANIM_ENTRY_STRIDE;
            if (*(void *P32 *)(e + 0x20)) limeFree(*(void *P32 *)(e + 0x20));
        }

        e = *(char *P32 *)(base + 4) + i * ANIM_ENTRY_STRIDE;
        if (*(void *P32 *)(e + 0x34)) limeFree(*(void *P32 *)(e + 0x34));

        if (i == 0) {                   /* and these two */
            e = *(char *P32 *)(base + 4) + i * ANIM_ENTRY_STRIDE;
            if (*(void *P32 *)(e + 0x2c)) limeFree(*(void *P32 *)(e + 0x2c));
            e = *(char *P32 *)(base + 4) + i * ANIM_ENTRY_STRIDE;
            if (*(void *P32 *)(e + 0x30)) limeFree(*(void *P32 *)(e + 0x30));
        }
    }

    if (*(void *P32 *)(base + 0x2c)) limeFree(*(void *P32 *)(base + 0x2c));
    LIME_FreeSkin(*(void *P32 *)(base + 0x30));
    LIME_FreeBones(*(void *P32 *)(base + 0x34));
    limeFree(*(void *P32 *)(base + 0x38));
    limeFree(*(void *P32 *)(base + 4));

    /* six textures, each only if present */
    if (*(TEXTURE *P32 *)(base + 0x14)) limeDeleteTexture(*(TEXTURE *P32 *)(base + 0x14));
    if (*(TEXTURE *P32 *)(base + 0x18)) limeDeleteTexture(*(TEXTURE *P32 *)(base + 0x18));
    if (*(TEXTURE *P32 *)(base + 0x1c)) limeDeleteTexture(*(TEXTURE *P32 *)(base + 0x1c));
    if (*(TEXTURE *P32 *)(base + 0x20)) limeDeleteTexture(*(TEXTURE *P32 *)(base + 0x20));
    if (*(TEXTURE *P32 *)(base + 0x24)) limeDeleteTexture(*(TEXTURE *P32 *)(base + 0x24));
    if (*(TEXTURE *P32 *)(base + 0x28)) limeDeleteTexture(*(TEXTURE *P32 *)(base + 0x28));

    if (LIME_SceneExists(*(void *P32 *)(base + 0x10))) {
        LIME_FreeMeshSetTextures(
            *(void *P32 *)(*(char *P32 *)(base + 0x10) + 0x80));
        LIME_FreeScene(*(void *P32 *)(base + 0x10));
    }

    limeFree(c);
    return 1;                           /* always; nothing checks it */
}


/* An ANIMATEDCHARACTER as far as IsAFrameVisible reaches. The frame stride is
 * 88 bytes and the compiler spells it out rather than multiplying:
 * `r1*12 - r1` shifted left three, which is 88. */
#define ANIMCHAR_FRAME_STRIDE  88

/* ----------------------------------------------------------- IsAFrameVisible
 *
 * armv7 0x0005b9dc, 120 bytes.  **Complete.**
 *
 * Whether one frame of one character may be drawn. Three things decide it, and
 * two of them are hardcoded per-character hacks:
 *
 *      character 3, frame 290   ->  NEVER visible
 *      character 6, frame 365   ->  ALWAYS visible
 *      otherwise                ->  read the frame own flags
 *
 * `c[2]` -- offset +8 -- is the character id. That was already established by
 * IsFrameVisible above, which special-cases character 12 and frame 295 for a
 * different purpose. **Three separate per-character frame exceptions across the
 * two functions**: 3/290, 6/365 and 12/295. None of them is derivable from
 * data; they are patches somebody made late and they have to be carried across.
 *
 * The general path clamps the frame id twice over: negatives to zero
 * (`bic r3, r1, r1, asr #31`), and anything past `c[0] - 2` down to that. Note
 * **minus two**, not minus one -- the last frame is not reachable here at all.
 *
 * Then, of the frame record at `c[1] + idx * 88`:
 *
 *      +0x18 == 1                          ->  visible
 *      else if *(void**)c[0x30/4] == NULL  ->  not visible
 *      else                                ->  visible if +0x28 == 1
 *
 * So the second flag is only consulted when the pointer at +0x30 has something
 * behind it -- a second table that may not be loaded.
 */
int IsAFrameVisible(ANIMATEDCHARACTER *c, long frame)
{
    const long *a = (const long *)c;
    long count = a[0];
    /* The fields at +4 and +0x30 are POINTERS in a 32-bit image, and `long` is
     * how the rest of this file reaches a record it has no struct for. The
     * uintptr_t hop is what keeps that honest on a 64-bit host; it is not a
     * reinterpretation of the data. */
    const char *frames = (const char *)(uintptr_t)(unsigned long)a[1];
    long id = a[2];
    long idx;
    const long *f;

    if (frame == 290 && id == 3)
        return 0;                       /* the hack, first */
    if (frame == 365 && id == 6)
        return 1;

    idx = (frame < 0) ? 0 : frame;      /* bic rN, rN, rN asr #31 */
    if (idx >= count - 2)
        idx = count - 2;

    f = (const long *)(frames + idx * ANIMCHAR_FRAME_STRIDE);

    if (f[0x18 / 4] == 1)
        return 1;

    if (*(void *P32 *)(uintptr_t)(unsigned long)a[0x30 / 4] == 0)
        return 0;

    return f[0x28 / 4] == 1;
}


/* `_PlayerDefs` -- 0x00170950, **52 bytes** an entry. Three functions now spell
 * that stride out the same way, `(n*16 - n*4 + n) << 2`, which is 13 << 2.
 *
 * Word 0 is the character id. The seven words from +0x18 to +0x30 are asset
 * name pointers; Load1Character passes all seven on and the ORDER it passes
 * them in is not the order they sit in. */
extern const PLAYERDEF PlayerDefs[];   /* 0x00170950, 26 entries */
#define PLAYERDEF_STRIDE  52

/* `_Players` -- 0x001fa4d4, and the second player is at +0x5f0. */
#define PLAYER_STRIDE  0x5f0

ANIMATEDCHARACTER *LoadAnimatedCharacter(char *a, char *b, char *c, char *d,
                                         char *e, char *f, char *g,
                                         EPLAYER who, FRONTEND_CHARACTER *fe,
                                         long p, long q, long r);


/* ------------------------------------------------------------ Load1Character
 *
 * armv7 0x0005ccec, 132 bytes.  **Complete.**
 *
 * Unpacks one PLAYERDEF into the twelve-argument LoadAnimatedCharacter call and
 * hangs the result off the player.
 *
 * **The seven asset names are not passed in memory order.** The def holds them
 * at +0x18 through +0x30 and they go out as
 *
 *      +0x30, +0x24, +0x20, +0x18, +0x28, +0x2c, +0x1c
 *
 * There is no pattern to recover and no way to guess it back; it is transcribed
 * from the register assignments and it has to stay exactly this.
 *
 * **The EPLAYER handed to LoadAnimatedCharacter is `def[0]`, not the `who` this
 * function was called with.** The caller argument is used only to index the
 * table and then to stamp the loaded character. Normally the two agree -- but
 * nothing here makes them, and the code goes out of its way to read the def.
 *
 * The tenth argument is a hardcoded 1; only the fifth argument of this function
 * reaches the twelfth.
 *
 * Afterwards:
 *
 *      p->anim   = result          (+0x04)
 *      result[8] = who             (+0x08)
 *
 * **That +0x08 store is where the character id in an ANIMATEDCHARACTER comes
 * from** -- the same field IsAFrameVisible and IsFrameVisible read to decide
 * their per-character frame exceptions. Written here, read there.
 */
void Load1Character(PLAYER *p, EPLAYER who, FRONTEND_CHARACTER *fe,
                    long a, long b)
{
    const PLAYERDEF *def = &PlayerDefs[who];
    ANIMATEDCHARACTER *c;

    c = LoadAnimatedCharacter(def->scene, def->skin, def->bones,
                              def->lighting, def->skinAnim, def->texBase,
                              def->frameList,
                              (EPLAYER)def->id, fe, a, 1, b);

    ((long *)p)[1] = (long)(uintptr_t)c;    /* p->anim, +0x04 */
    c->id = who;                   /* the character id, +0x08 */
}


void LoadGameCharacterCheckCache(PLAYER *p, const PLAYERDEF *def,
                                 FRONTEND_CHARACTER *fe);
int  puts(const char *s);
void LoadAllFramesTXT(void);


/* ------------------------------------------------ LoadGameCharacterCheckCache
 *
 * armv7 0x0005ce9c, 136 bytes.  **Complete.**
 *
 * LoadGameCharacter with two cache lookups in front of it:
 *
 *      printf("loading character %s...\n", def->name)
 *
 *      1. if OrigLoadedPlayers[1] is live and its id matches, reuse it
 *         OUTRIGHT -- p->anim points at the other player character and the
 *         function returns without touching anything else.
 *      2. otherwise ask HavePreloadedCharacter; on a hit, puts("Found in
 *         cache!") and take it.
 *      3. otherwise Load1Character, then **ask HavePreloadedCharacter again**
 *         and store THAT as p->anim.
 *
 * Case 1 returning early is the one that matters: it is the only path that does
 * not stamp `p->id`, `p->[0x5e8]` and `p->[0x5ec]`. A mirror match leaves those
 * three fields holding whatever the previous character left there.
 *
 * The second HavePreloadedCharacter in case 3 overwrites the pointer
 * Load1Character has already written into p->anim. The two agree in practice,
 * but the code asks rather than assuming, and that is transcribed.
 *
 * Note it checks `OrigLoadedPlayers[1]` only -- slot 0 is never consulted.
 */
void LoadGameCharacterCheckCache(PLAYER *p, const PLAYERDEF *def,
                                 FRONTEND_CHARACTER *fe)
{
    long *pw = (long *)p;
    const long *dw = (const long *)def;
    ANIMATEDCHARACTER *c;

    printf("loading character %s...\n", ((char *P32 const *)def)[0x18 / 4]);

    if (OrigLoadedPlayers[1] != 0 &&
        dw[0] == ((const long *)OrigLoadedPlayers[1])[2]) {
        pw[1] = (long)(uintptr_t)OrigLoadedPlayers[1];
        return;                         /* the three fields below are NOT set */
    }

    c = HavePreloadedCharacter((EPLAYER)dw[0]);
    if (c != 0) {
        puts("Found in cache!");
        pw[1] = (long)(uintptr_t)c;
    } else {
        Load1Character(p, (EPLAYER)dw[0], fe, 0, 1);
        c = HavePreloadedCharacter((EPLAYER)dw[0]);
        pw[1] = (long)(uintptr_t)c;     /* asked again, not reused */
    }

    pw[0x5e8 / 4] = 0;
    pw[0]         = dw[0];
    pw[0x5ec / 4] = -1;
}


/* ------------------------------------------------------- LoadLevelCharacters
 *
 * armv7 0x0005d078, 160 bytes.  **Complete.**
 *
 *      printf("Loading characters... (%s vs %s)...\n", nameA, nameB)
 *      ClearAnimRemapTables()
 *      LoadAllFramesTXT()
 *      LoadGameCharacter(&Players[0], &PlayerDefs[a], NULL, 0)
 *      LoadGameCharacter(&Players[1], &PlayerDefs[b], NULL, a == b)
 *      Players[0].altCostume = 0
 *      Players[1].altCostume = 0
 *      OrigLoadedPlayers[0] = Players[0].anim
 *      OrigLoadedPlayers[1] = Players[1].anim
 *
 * **The fourth argument to the second load is `a == b`** -- the mirror-match
 * flag, computed right there from the two ids. That is what tells the loader
 * the second fighter is the same character as the first, and it is the only
 * place the flag comes from.
 *
 * The two `altCostume` stores are redundant: LoadGameCharacter clears +0x530
 * for each player already, and these clear the same two words a second time
 * (0xb20 being 0x5f0 + 0x530, player two's copy of the field). Transcribed
 * because they are there, not because they do anything.
 *
 * The remap tables are cleared and the frame text reloaded BEFORE either
 * character is touched, so a level load always starts from a blank remap.
 */
extern int DbgPalette[2];              /* runtime/debug_menu.c, port only */
extern int DbgFightActive;             /* the same, while a debug fight runs */
int PortFightPalette(int side, long c1, long c2);  /* FrontEnd.c, port only */

void LoadLevelCharacters(EPLAYER a, EPLAYER b)
{
    PLAYER *p0 = (PLAYER *)Players;
    PLAYER *p1 = (PLAYER *)(Players + PLAYER_STRIDE);
    const PLAYERDEF *da = &PlayerDefs[a];
    const PLAYERDEF *db = &PlayerDefs[b];

    printf("Loading characters... (%s vs %s)...\n",
           da->lighting, db->lighting);

    ClearAnimRemapTables();
    LoadAllFramesTXT();

    /* Port only: in a debug fight the menu's PALETTE rows (DbgPalette,
     * 0 = the binary's own rule), otherwise the select screen's colour
     * picker (PortFightPalette, FrontEnd.c; in Arcade the palette the
     * ladder began with), pick each fighter's palette.
     * With nothing picked both give the binary's rule: 0, and 1 for
     * player two when a == b. */
    {
        int pa = DbgFightActive ? DbgPalette[0] : PortFightPalette(0, a, b);
        int pb = DbgFightActive ? DbgPalette[1] : PortFightPalette(1, a, b);

        LoadGameCharacter(p0, da, 0, pa ? pa == 2 : 0);
        LoadGameCharacter(p1, db, 0, pb ? pb == 2 : (a == b) ? 1 : 0);
    }

    p0->altCostume = 0;                 /* already cleared, twice over */
    p1->altCostume = 0;

    OrigLoadedPlayers[0] = (ANIMATEDCHARACTER *)(uintptr_t)((long *)p0)[1];
    OrigLoadedPlayers[1] = (ANIMATEDCHARACTER *)(uintptr_t)((long *)p1)[1];
}


/* `_AllFramesTable` -- 0x00218cc4, **65 bytes** an entry: one byte at +0 that
 * nothing here writes, then a 64-byte name. The compiler spells the stride as
 * `i*64 + i`. */
#define ALLFRAMES_STRIDE  65
#define ALLFRAMES_COUNT   0x1c4c        /* 7244 */

/* An ARRAY, not a pointer, and the symbol table settles it: `_AllFramesTable`
 * is a `__DATA,__common` object at 0x00218cc4 and the gap to the next symbol --
 * `_PreloadedCharacters` at 0x0028bc10 -- is 470,860 bytes, which is
 * ALLFRAMES_COUNT * ALLFRAMES_STRIDE to the byte. A pointer slot would live in
 * the 0x000f3xxx region and hold an address; this holds the table.
 *
 * Spelled `char *` it reads the table's first bytes as a pointer and
 * LoadAllFramesTXT's sscanf then writes through whatever that is. */
extern char AllFramesTable[ALLFRAMES_COUNT * ALLFRAMES_STRIDE]; /* 0x00218cc4 */

void *limeLoadFile(const char *name);
void  limeFree(void *p);
int   sscanf(const char *s, const char *fmt, ...);
size_t strlen(const char *s);


/* ---------------------------------------------------------- LoadAllFramesTXT
 *
 * armv7 0x0005cf24, 176 bytes.  **Complete.**
 *
 * Reads `framelists/allframes.txt` into `_AllFramesTable`, one whitespace-
 * delimited name per entry, with `sscanf(p, "%32s", entry + 1)`.
 *
 * **7244 entries, and that is the third place this number turns up.**
 * FrameID_GetBBox rejects a frame id above 7244 and ClearAnimRemapTables walks
 * 7245. So the table holds 7245 slots and this fills the first 7244 of them --
 * the numbers do not quite meet, and the difference is recorded rather than
 * smoothed: one slot is left as it was.
 *
 * The name field is 64 bytes but `%32s` caps a name at 32 characters plus its
 * terminator, so half the field is unreachable through this loader.
 *
 * After each successful token the cursor advances by `strlen` of what was
 * written -- not by what sscanf consumed -- and then any run of `\n` and
 * `\r` is skipped. A line that does not scan leaves the cursor where it
 * was, so the loop spends the rest of its 7244 iterations reading the same
 * bytes. It is bounded, so it terminates; it just stops making progress.
 *
 * ### The failure path only looks like it falls through
 *
 * If limeLoadFile returns NULL it calls
 *
 *      Error("Couldn't load master frames list!!\n")
 *
 * and then branches back into the parse loop with the null pointer still in
 * hand -- which reads like a crash waiting to happen. It is not: `Error` above
 * ends in a branch to its own address and never comes back. The loop head is
 * simply where the compiler pointed the unreachable edge.
 */
void LoadAllFramesTXT(void)
{
    char *buf = (char *)limeLoadFile("framelists/allframes.txt");
    char *p;
    long i;

    if (buf == 0)
        Error("Couldn't load master frames list!!\n");   /* never returns */

    p = buf;
    for (i = 0; i != ALLFRAMES_COUNT; i++) {
        char *dst = AllFramesTable + i * ALLFRAMES_STRIDE + 1;

        if (sscanf(p, "%32s", dst) == 1)
            p += strlen(dst);

        while (*p == 10 || *p == 13)    /* newline, carriage return */
            p++;
    }

    limeFree(buf);
}


/* `_IdlesPerPlayer` -- 0x00171288, eight bytes an entry.
 *
 * Spelled as a struct because the two words are not the same kind of thing and
 * an array of one type cannot hold both. The loader indexes it with
 * `lsls r3, r4, #3` -- who * 8, one entry -- then `ldr r2, [r3]` takes the
 * first word as a VALUE and `ldr r1, [r3, #4]` the second as an address it
 * walks. The first word is a float: the table reads 0x3f800000 and 0x3f000000,
 * which are 1.0f and 0.5f, and DrawFrontEndCharacter below loads the field it
 * lands in with `*(const float *)(fc + 0x660)`. Two independent readings, and
 * they agree.
 *
 * This was `long *IdlesPerPlayer[]` and every entry was one dereference of the
 * bits of 1.0f. */
typedef struct {
    float       speed;      /* idle playback rate: 1.0f, and 0.5f for a few */
    const long *frames;     /* frame numbers, terminated by -1 */
} IdleSet;

/* `_TheFECharacters` is already declared above as [25][0x668]; slot 23 is the
 * one this function loads as character 0. */
extern const IdleSet IdlesPerPlayer[];  /* 0x00171288 */


/* ----------------------------------------------------- LoadFrontEndCharacters
 *
 * armv7 0x0005cfd4, 164 bytes.  **Complete.**
 *
 * Sets up one front-end character slot and loads its model.
 *
 * **Character 0 gets a preamble nobody else gets**: ClearAnimRemapTables and
 * LoadAllFramesTXT run first, then it falls into the same body. So the frame
 * tables are rebuilt on the first character of a front-end load and not again.
 *
 * The body clears +4 and +0x65c, copies `IdlesPerPlayer[who].speed` into +0x660,
 * and then copies that entry second word -- a **-1-terminated list** -- into
 * the slot at +0x5f4, terminating it with its own -1.
 *
 * **Character 23 is loaded as character 0.** `(who == 0x17) ? 0 : who` is the
 * id handed to Load1Character, while the destination slot and the id stamped
 * into +0 afterwards stay 23. So slot 23 holds character 0 geometry under its
 * own name.
 *
 * The same pointer is passed as both the PLAYER and the FRONTEND_CHARACTER
 * argument -- one struct playing both parts.
 */
void LoadFrontEndCharacters(long who)
{
    char *fc;
    long *dst;
    const long *src;

    if (who == 0) {
        ClearAnimRemapTables();
        LoadAllFramesTXT();
    }

    fc = TheFECharacters[who];

    ((long *)fc)[1]            = 0;             /* +0x04 */
    *(long *)(fc + 0x650 + 0xc) = 0;            /* +0x65c */
    *(float *)(fc + 0x660)      = IdlesPerPlayer[who].speed;

    src = IdlesPerPlayer[who].frames;
    dst = (long *)(fc + 0x5f0 + 4);
    while (*src != -1)
        *dst++ = *src++;
    *dst = -1;

    Load1Character((PLAYER *)fc, (EPLAYER)((who == 0x17) ? 0 : who),
                   (FRONTEND_CHARACTER *)fc, 0, 0);

    ((long *)fc)[0] = who;              /* stamped AFTER the load */
}


extern long PLAYER1MODEL;               /* 0x0014e1b4 */

/* The preload array and its 0x5f4 stride are declared above. This function
 * builds that stride as `((n*16 - n*4) << 7) - (n*16 - n*4)` -- 12n * 127 --
 * which is 1524, and 1524 is 0x5f4. Two functions, two spellings, same number.
 * Note it is the PLAYER stride 0x5f0 plus four. */


/* --------------------------------------------------------- Preload1Character
 *
 * armv7 0x0005cda0, 212 bytes.  **Complete.**
 *
 * Load1Character's sibling for the preload cache: same seven asset names in the
 * same out-of-order order, same `def[0]` rather than the caller's `who` as the
 * EPLAYER argument. It returns immediately if the character is already
 * preloaded.
 *
 * **The two differ in the last three arguments.** Load1Character passes
 * `(a, 1, b)`; this passes `(a, b, who == PLAYER1MODEL)`. So the flag
 * Load1Character hardcodes to 1 is the caller `b` here, and the twelfth
 * argument -- constant in the other function -- is a live test against the
 * global `_PLAYER1MODEL`. Whatever that twelfth argument selects, it is on for
 * exactly one character per preload pass.
 *
 * `NumPreloadedCharacters` is read into a local BEFORE the load and the new
 * entry is written at that saved index afterwards, so a nested preload during
 * LoadAnimatedCharacter would be overwritten by this one. Transcribed as the
 * save-then-use it is.
 */
void Preload1Character(EPLAYER who, FRONTEND_CHARACTER *fe, long a, long b)
{
    const PLAYERDEF *def;
    long slot;
    ANIMATEDCHARACTER *c;

    if (HavePreloadedCharacter(who) != 0)
        return;

    slot = NumPreloadedCharacters;      /* read before the load */
    def = &PlayerDefs[who];

    c = LoadAnimatedCharacter(def->scene, def->skin, def->bones,
                              def->lighting, def->skinAnim, def->texBase,
                              def->frameList,
                              (EPLAYER)def->id, fe, a, b,
                              (who == PLAYER1MODEL) ? 1 : 0);

    *(long *)&PreloadedCharacters[slot][0] = who;
    *(long *)&PreloadedCharacters[slot][8] = (long)(uintptr_t)c;

    NumPreloadedCharacters++;
}


/* Both are ARRAYS in __data, 26 words each, not pointer slots. 0x0014e0d8 holds
 * 26 distinct addresses -- _KanoIdleFrames, _ReptileIdleFrames and the rest,
 * with 0 for character 23 -- and 0x0014e140 holds 26 byte counts (0xd4,
 * 0xcc, ...). Declared as slots they were given zeroed storage, every list was
 * NULL, AnimateFECharacters set every frame to -1 and RenderFECharacters drew
 * nobody. */
extern long *IdleLists[26];             /* 0x0014e0d8 */
extern long  SizeofIdleLists[26];       /* 0x0014e140 */
extern long   AnimSmoothWindowSize;     /* 0x00171368 */

void PlayerAutoSmoothAnims(PLAYER *p);


/* ------------------------------------------------------- AnimateFECharacters
 *
 * armv7 0x0005bdd4, 284 bytes.  **Complete.**
 *
 * Advances the idle animation of every front-end character slot, all 26 of
 * them, every frame.
 *
 * Per slot:
 *
 *      cursor += dt * speed * 0.5f                 (+0x65c += , speed at +0x660)
 *      if (IdleLists[who] == NULL) { frame = -1; }
 *      else {
 *          if (list[(long)cursor] == -1) cursor = 0.0f      <- end marker
 *          while ((float)(SizeofIdleLists[who] / 4) <= cursor)
 *              cursor -= SizeofIdleLists[who] / 4           <- wrap
 *          frame = list[(long)cursor]
 *      }
 *      p->[0x14] = frame
 *      p->[0x5d4] = 1.0f
 *
 * ### The half
 *
 * The step is `dt * speed * 0.5`, not `dt * speed`. The 0.5 is a `vmov.f32
 * s10, #0.5` immediate, separate from anything in the data, so it is a
 * hardcoded halving of every front-end idle -- the menus run their characters
 * at half the rate their own speed field asks for.
 *
 * ### The wrap is a loop
 *
 * When the cursor runs past the end of the list it is brought back by
 * subtracting the LIST LENGTH until it is in range: `vsub.f32 d6, d6, d7` at
 * 0x5be54, where s14 is the entry count just converted at 0x5be70. The idle
 * lists in the image carry no -1 end marker, so this is what loops them. An
 * earlier reading subtracted the step instead, which parked every fighter on
 * the last frame of its idle after one pass.
 *
 * The bound is `SizeofIdleLists[who] / 4` -- a byte size turned into an entry
 * count, with the signed-division rounding the compiler emits (`n + 3` when
 * negative, then `asr #2`) transcribed rather than simplified to `/ 4`.
 *
 * ### It borrows a global around the call
 *
 * `_AnimSmoothWindowSize` is saved, set to **0x28**, `PlayerAutoSmoothAnims` is
 * called, and the old value is put back -- the same save-write-restore shape
 * `drawPage2x2BigForSettings` uses on `Settings[3]`. So front-end characters
 * smooth over a 40-frame window whatever the game is using, and nothing else
 * ever sees the change.
 *
 * A slot with no idle list gets frame -1 and still goes through the smoothing.
 */
void AnimateFECharacters(float dt)
{
    long i;

    for (i = 0; i < FE_CHARACTER_SLOTS; i++) {
        char  *fc    = TheFECharacters[i];
        long   who   = ((long *)fc)[0];
        float *cursor = (float *)(fc + 0x65c);
        float  speed  = *(const float *)(fc + 0x660);
        float  step   = dt * speed * 0.5f;
        const long *list = IdleLists[who];
        long   saved;

        *cursor += step;

        if (list == 0) {
            *(long *)(fc + 0x664) = -1;
        } else {
            long n;

            if (list[(long)*cursor] == -1)
                *cursor = 0.0f;         /* the end marker */

            n = SizeofIdleLists[who];
            if (n < 0)
                n += 3;                 /* the signed /4 rounding, as emitted */
            n >>= 2;

            while ((float)n <= *cursor)
                *cursor -= (float)n;    /* wrap by the list length (0x5be54) */

            *(long *)(fc + 0x664) = list[(long)*cursor];
        }

        ((long *)fc)[0x14 / 4] = *(long *)(fc + 0x664);

        saved = AnimSmoothWindowSize;
        AnimSmoothWindowSize = 0x28;    /* borrowed for one call */
        PlayerAutoSmoothAnims((PLAYER *)fc);
        AnimSmoothWindowSize = saved;

        *(float *)(fc + 0x5d0 + 4) = 1.0f;
    }
}


extern float  ShadowOffset;             /* pointer slot */
extern float  ShadowHeightFromGround;   /* 0x00171364 */
extern limeVECTOR3 *RenderVerts;        /* pointer slot */

/* The two skinning generators are defined above. */
/* armv7 0x0005e358, five parameters: the frame record, the texture, the grey
 * level, the fade vector and which half of the record. The binary is
 * soft-float, so `grey` travels in r2 and the fifth argument is the first
 * stack word -- the "sixth parameter" an earlier note inferred came from
 * reading it as hard-float. The definition is in lime/RenderMesh.c. */
void LIME_RenderMeshSingleIndexed(void *frame, TEXTURE *tex, float grey,
                                  const limeVECTOR3 *fade, long second);
void LIME_printf(int window, const char *fmt, ...);
void LIME_GLAPI glColor4f(float r, float g, float b, float a);
void LIME_GLAPI glTranslatef(float x, float y, float z);
void LIME_GLAPI glScalef(float x, float y, float z);
void LIME_GLAPI glEnable(unsigned int cap);
void LIME_GLAPI glPushMatrix(void);
void LIME_GLAPI glPopMatrix(void);
#define GL_DEPTH_TEST 0x0B71


/* ---------------------------------------------------- RenderAnimatedCharacter
 *
 * armv7 0x0005c16c, 456 bytes.  **Complete.**
 *
 * Draws one fighter: the body, the optional second skin, and the shadow --
 * three matrix-bracketed passes over the same frame record.
 *
 * Both frame indices are clamped the same way `IsAFrameVisible` clamps its own:
 * negatives to zero via `bic rN, rN, rN asr #31`, then down to `count - 2`.
 * **Minus two again**, so the last frame is unreachable here too. The frame
 * record is at `frames + idx * 88`.
 *
 * ### The shadow is the mesh flattened, not a separate asset
 *
 *      glColor4f(0, 0, 0, 1)
 *      glTranslatef(0, (y - ShadowOffset) * -100 + ShadowHeightFromGround, 0)
 *      glScalef(1.0f, 0.0f, 1.0f)
 *      LIME_RenderMeshSingleIndexed(frame, NULL, ...)
 *
 * **`glScalef(1, 0, 1)` collapses Y to zero** -- the same geometry, squashed
 * onto the ground plane and painted black. There is no shadow mesh and no
 * shadow texture; a port that goes looking for one will not find it.
 *
 * The height is `(y - ShadowOffset) * -100 + ShadowHeightFromGround`. The
 * **-100** is a pool literal, not a scale anyone would guess, and it inverts
 * the sign: a fighter further from `ShadowOffset` puts its shadow further the
 * other way. `ShadowHeightFromGround` then lifts the whole thing off the floor
 * so it does not z-fight with it.
 *
 * The shadow pass reuses the vertices the body pass generated -- there is no
 * second skinning call -- so it is exactly the pose that was just drawn.
 *
 * ### The second skin, and the same per-character exception a third time
 *
 * The SKIN2 pass runs when `c[0x30]` points at something live and either the
 * frame's `+0x28` flag is set **or** `frame == 365 && character == 6`.
 *
 * That pair is the same exception `IsAFrameVisible` carries, and this is the
 * third function to hardcode it (`IsFrameVisible` has 12/295). It is not a
 * visibility quirk -- character 6's frame 365 genuinely needs the second skin,
 * and the flag in the data does not say so.
 *
 * SKIN2 draws with `c[0x20]` as its texture and passes 1 as the last argument
 * where the body pass passes 0.
 *
 * ### A debug line that shipped
 *
 *      LIME_printf(0x1b, "Player SKIN %s at fr %d is VISIBLE\n", name, frame)
 *
 * on every visible body, every frame. Level 0x1b presumably gates it at
 * runtime; the call is unconditional.
 *
 * The body colour is `(grey, grey, grey, 1)` from the caller -- one scalar for
 * all three channels, so a fighter can only be darkened, never tinted.
 */
void RenderAnimatedCharacter(char *name, ANIMATEDCHARACTER *c,
                             long frameA, long frameB, float t,
                             float y, float grey, limeVECTOR3 *pos,
                             void *tex, long visible)
{
    const long *a = (const long *)c;
    long count = a[0];
    long idxA = (frameA < 0) ? 0 : frameA;
    long idxB = (frameB < 0) ? 0 : frameB;
    void *frame;
    long vis = (visible != 0) ? 1 : 0;
    const long *f;

    if (idxA >= count - 2) idxA = count - 2;
    if (idxB >= count - 2) idxB = count - 2;

    frame = (char *)(uintptr_t)(unsigned long)a[1]
            + idxA * ANIMCHAR_FRAME_STRIDE;
    f = (const long *)frame;

    glColor4f(grey, grey, grey, 1.0f);
    glEnable(GL_DEPTH_TEST);

    /* ---- the body ---- */
    glPushMatrix();
    if (f[0x18 / 4] == 1 && vis) {
        GenerateFrameVertsBySkinning(c, idxA, idxB, t, RenderVerts);
        LIME_printf(0x1b, "Player SKIN %s at fr %d is VISIBLE\n",
                    name, (int)frameA);
        /* port: a modded skin's own textures, iced when frozen, not as a
         * baby (runtime/skin_mats.c) */
        port_skin_mats_use(tex && tex != ((void *P32 *)c)[0x1c / 4]
                           ? ((void *P32 *)c)[0x30 / 4] : NULL,
                           tex && tex == ((void *P32 *)c)[0x18 / 4]);
        /* port: SMOOTH MODELS draws the skin smoothed, or nothing */
        if (!port_smooth_draw(((void *P32 *)c)[0x30 / 4], frame, 0, tex,
                              grey, pos))
            LIME_RenderMeshSingleIndexed(frame, tex, grey, pos, 0);
        port_skin_mats_use(NULL, 0);
    } else {
        /* 0x5c2f4: a body not drawn is still posed. A baked frame (Mileena's
         * nail fatality, 310..334) draws scene meshes instead, and what they
         * hold -- her BIG CUP on bone 24 -- rides this skeleton through the
         * attach matrices RenderLevelPlayers copies after the draw. */
        GenerateMatrices(((char *P32 *)c)[0x3c / 4],
                         (BONESINFO *)((char *P32 *)c)[0x34 / 4],
                         idxA, idxB, t, a[0x40 / 4]);
        LIME_printf(0x1b, "Player SKIN %s at fr %d is INVISIBLE\n",
                    name, (int)frameA);
    }
    glPopMatrix();

    /* ---- the second skin ---- */
    glPushMatrix();
    if (*(void *P32 *)(uintptr_t)(unsigned long)a[0x30 / 4] != 0 && vis) {
        if (f[0x28 / 4] == 1 || (idxA == 365 && a[2] == 6)) {
            GenerateFrameVertsBySkinningSKIN2(c, idxA, idxB, t, RenderVerts);
            port_skin_mats_use(*(void *P32 *)(uintptr_t)(unsigned long)
                                   a[0x30 / 4], 0); /* port: skin_mats.c */
            if (!port_smooth_draw(*(void *P32 *)(uintptr_t)(unsigned long)
                                      a[0x30 / 4], frame, 1,
                                  (void *)(uintptr_t)(unsigned long)a[0x20 / 4],
                                  grey, pos))      /* port: SMOOTH MODELS */
                LIME_RenderMeshSingleIndexed(frame,
                        (void *)(uintptr_t)(unsigned long)a[0x20 / 4],
                        grey, pos, 1);
            port_skin_mats_use(NULL, 0);
        }
    }
    glPopMatrix();

    /* ---- the shadow: the same mesh, flattened ---- */
    glPushMatrix();
    glColor4f(0.0f, 0.0f, 0.0f, 1.0f);
    glTranslatef(0.0f,
                 (y - ShadowOffset) * -100.0f + ShadowHeightFromGround,
                 0.0f);
    glScalef(1.0f, 0.0f, 1.0f);         /* Y collapsed to the ground plane */

    /* port: SMOOTH MODELS leaves the shadow alone -- flattened onto the
     * floor the smoothing does not show, and it would cost a second draw of
     * the subdivided body (Mary: lag). REAL-TIME SHADOWS, when drawn,
     * replace it (render/rgl.h). */
    if (f[0x18 / 4] == 1 && vis && !rgl_shadow_active())
        LIME_RenderMeshSingleIndexed(frame, 0, grey, pos, 0);

    glPopMatrix();
}


/* ### The animation history rings, and they tile PLAYER exactly
 *
 * This function establishes a block of PLAYER that nothing else had named.
 * Three parallel 64-entry rings, indexed by `cursor & 0x3f`, laid end to end:
 *
 *      +0x018  long  frame[64]          64 * 4   = 256 bytes -> +0x118
 *      +0x118  float pos[64][3]         64 * 12  = 768 bytes -> +0x418
 *      +0x418  long  flag[64]           64 * 4   = 256 bytes -> +0x518
 *      +0x518  long  cursor             the write position
 *      +0x51c  long  frameA             the two frames being blended
 *      +0x520  long  frameB
 *      +0x524  float t                  and the blend factor
 *
 * Every boundary lands exactly on the next field, so the layout is not
 * inferred -- the three sizes have to be 64 entries for +0x518 to be where the
 * cursor is. `-1` in a frame slot means "not yet written".
 */
#define ANIMHIST_ENTRIES  64

/* IsLiaProblem and IsOstrichProblem are defined above. */
void LerpVector3(const float *a, const float *b, float t, float *out);


/* -------------------------------------------------------- PlayerAutoSmoothAnims
 *
 * armv7 0x0005bb44, 656 bytes.  **Complete.**
 *
 * Interpolates the fighter's pose between recorded history frames, so the
 * animation plays back smoothly at a rate the source frames do not have.
 * Called every frame from `AnimateFECharacters`,
 * `UpdateIntroCharacterPlayers` and `RenderAnimatedCharacter`'s callers, each
 * of which borrows `_AnimSmoothWindowSize` around the call.
 *
 * Each call first records the present: current frame, current position and the
 * flag at +0x540 go into the three rings at `cursor & 0x3f`.
 *
 * ### It plays back DELAYED, by exactly the smoothing window
 *
 * The sample point is `cursor - AnimSmoothWindowSize`, not `cursor`. So the
 * pose drawn is the one from `window` frames ago -- 40 frames back in the front
 * end, 20 in the intro. That latency is not a side effect of smoothing; it is
 * how the function can see both sides of a frame change before deciding how to
 * blend across it.
 *
 * ### Finding the run
 *
 * From the sample point it walks **backwards up to 31 entries** and **forwards
 * up to 63** while the recorded frame id stays the same, which brackets the run
 * of identical frames the sample sits inside. The midpoint of that run is
 *
 *      mid = back + (runLength + 1) * 0.5f
 *
 * and which half the sample falls in decides the direction of the blend:
 *
 *      first half:   t = 0.5f + 0.5f * (sample - back) / ((run + 1) * 0.5f)
 *                    LerpVector3(posBefore, posNow, t, p->pos)
 *
 *      second half:  t = 0.5f * (sample - mid) / (forward - mid)
 *                    LerpVector3(posNow, posAfter, t, p->pos)
 *
 * So the pose eases from the previous frame's position into the current one
 * across the first half of a held frame, and out towards the next across the
 * second. The two `t` ranges meet at 0.5 in the middle -- the first arm ends at
 * 1.0 and the second begins at 0.0, which is why the source vectors differ
 * between the arms rather than the factor being continuous.
 *
 * ### Four ways to refuse to smooth
 *
 * The blend is skipped and both frames set to the same value -- a hard cut --
 * when any of:
 *
 *      the run is longer than the window will reach (`0x3f - window < back`,
 *          or the forward run is shorter than the window)
 *      the character is 16 and its frame is between 35 and 41
 *      `IsLiaProblem(p)` or `IsOstrichProblem(p)`
 *      any of the 64 frame slots still holds -1 -- the ring is not full yet
 *
 * The last one is what stops a freshly loaded character being interpolated
 * against garbage, and the -1 scan runs over **all 64** entries every call, not
 * just the ones in the window.
 *
 * **The Lia and Ostrich checks run even on the smoothing path**, after the
 * blend has already been written, and overwrite it with a hard cut. So those
 * two functions have the last word.
 *
 * The 35..41 range is written as one unsigned compare (`frame - 0x23 <= 6`),
 * the same folding that appears in `RunGameEvents`.
 */
void PlayerAutoSmoothAnims(PLAYER *p)
{
    long *w = (long *)p;
    long  cursor = w[0x518 / 4];
    long  window = AnimSmoothWindowSize;
    long *frame  = &w[0x018 / 4];               /* the three rings */
    float *pos   = (float *)&w[0x118 / 4];
    long *flag   = &w[0x418 / 4];
    long  sample = cursor - window;
    long  ring   = sample & 0x3f;
    long  held, back, fwd, k;
    long  anyEmpty = 0;
    int   hardCut;
    float posBefore[3], posNow[3], posAfter[3];
    long  frameBefore, frameAfter;
    float mid, t;

    frame[cursor & 0x3f] = w[0x14 / 4];
    pos[(cursor & 0x3f) * 3 + 0] = ((float *)&w[8 / 4])[0];
    pos[(cursor & 0x3f) * 3 + 1] = ((float *)&w[8 / 4])[1];
    pos[(cursor & 0x3f) * 3 + 2] = ((float *)&w[8 / 4])[2];
    flag[cursor & 0x3f] = w[0x540 / 4];

    for (k = 0; k < ANIMHIST_ENTRIES; k++)      /* all 64, every call */
        if (frame[k] == -1)
            anyEmpty = 1;

    held = frame[ring];

    back = 0;
    for (k = sample - 1; frame[k & 0x3f] == held && back < 0x1f; k--)
        back++;
    frameBefore = frame[k & 0x3f];
    posBefore[0] = pos[(k & 0x3f) * 3 + 0];
    posBefore[1] = pos[(k & 0x3f) * 3 + 1];
    posBefore[2] = pos[(k & 0x3f) * 3 + 2];

    fwd = 0;
    {
        long j;
        for (j = sample + 1; frame[j & 0x3f] == held && fwd < 0x3f; j++)
            fwd++;
        frameAfter = frame[j & 0x3f];
        posAfter[0] = pos[(j & 0x3f) * 3 + 0];
        posAfter[1] = pos[(j & 0x3f) * 3 + 1];
        posAfter[2] = pos[(j & 0x3f) * 3 + 2];

        posNow[0] = ((float *)&w[8 / 4])[0];
        posNow[1] = ((float *)&w[8 / 4])[1];
        posNow[2] = ((float *)&w[8 / 4])[2];

        hardCut = (0x3f - window >= back) ? (fwd >= window) : 1;

        if (w[0] == 0x10 && (unsigned long)(w[0x14 / 4] - 0x23) <= 6) {
            w[0x524 / 4] = 0;
            w[0x51c / 4] = w[0x14 / 4];
            w[0x520 / 4] = w[0x14 / 4];
            hardCut = 1;
        } else if (!hardCut) {
            mid = (float)back + ((float)(back + 1 + fwd) + 1.0f) * 0.5f;

            if (mid > (float)sample) {          /* the first half */
                t = 0.5f + 0.5f * ((float)sample - (float)back)
                                / (mid - (float)back);
                w[0x51c / 4] = frameBefore;
                w[0x520 / 4] = held;
                ((float *)w)[0x524 / 4] = t;
                LerpVector3(posBefore, posNow, t, (float *)&w[8 / 4]);
            } else {                            /* and the second */
                t = 0.5f * ((float)sample - mid) / ((float)j - mid);
                w[0x520 / 4] = frameAfter;
                w[0x51c / 4] = held;
                ((float *)w)[0x524 / 4] = t;
                LerpVector3(posNow, posAfter, t, (float *)&w[8 / 4]);
            }
            cursor = w[0x518 / 4];
        }
    }

    w[0x518 / 4] = cursor + 1;

    /* These two get the last word, even after a blend has been written. */
    if (IsLiaProblem(p) || IsOstrichProblem(p)) {
        w[0x51c / 4] = w[0x14 / 4];
        w[0x520 / 4] = w[0x14 / 4];
        w[0x524 / 4] = 0;
        return;
    }

    if (hardCut || anyEmpty) {
        w[0x51c / 4] = held;
        w[0x520 / 4] = held;
        w[0x524 / 4] = 0;
    }
}


/* ----------------------------------------------------- LoadAnimatedCharacter
 *
 * armv7 0x0005c348, 2,468 bytes.  **Complete.**
 *
 * The character loader: allocates the `ANIMATEDCHARACTER`, pulls in the skin,
 * the bones, the scene, the frame list and the lighting, builds one 88-byte
 * record per mesh, and loads up to six textures. Everything a fighter needs to
 * exist comes through here.
 *
 * ### The frame list decides what gets loaded at all
 *
 * `framelists/<NAME>.bin` is a flat `int16` array of **7,244 entries** -- the
 * same 7,244 that bounds `FrameID_GetBBox`. The name is built by uppercasing
 * the seventh argument and cutting it at the first `.`, in place, one byte at a
 * time.
 *
 * Walking it fills `FrameRemapTable` in **pairs**: the owner at `[k]` and the
 * frame at `[k + 1]`. An entry of -1 is skipped, and **bit 0x8000 marks a
 * finisher frame**:
 *
 *      WantDoingFatalFrames == 0   the entry is thrown away -- the table gets
 *                                  -1, WantFrames[id & 0x7fff] is cleared, and
 *                                  the frame is never loaded
 *      otherwise                   the bit is stripped and the frame is kept
 *
 * So the fatality animations are not in memory unless something asked for them
 * first. That is the mechanism a port has to keep if it wants the same memory
 * ceiling; drop it and every character costs its finishers up front.
 *
 * `WantFrames` is 0x1c4c = 7,244 bytes, one flag a frame, and the function
 * sets all of them before it starts.
 *
 * ### `.lighting` is per-mesh colour blocks
 *
 * `DrawSkinnedMesh2` is called twice on the **first mesh only**, with the
 * shared `RenderVerts` / `RenderUVs` / `RenderRGBs` scratch buffers, and both
 * calls are used for their *return value* rather than for drawing -- they
 * measure the two colour-buffer sizes. Those sizes are then reused for every
 * mesh, and the file is sliced by them:
 *
 *      memcpy(mesh->cols,  lighting + i * (size1 + size2),         size1)
 *      memcpy(mesh->cols2, lighting + i * (size1 + size2) + size1, size2)
 *
 * so `STATICLIGHTING/<name>.lighting` is an array of `size1 + size2` byte
 * blocks, one a mesh, in mesh order. The file is freed as soon as the copy is
 * done. See [meshset-format.md](../../OUTPUT/meshset-format.md), which
 * documented the container without knowing what indexed it.
 *
 * ### A mesh below 0.5 loses its colour buffers
 *
 * After the copy, a mesh whose meshbase scalar is under 0.5 -- and which is not
 * mesh 0 and not mesh 0x16d -- has both colour buffers freed and every related
 * field zeroed. It keeps its geometry and gives up its per-vertex colour. That
 * is a memory decision baked into the data, not the code: change the scalar in
 * the meshbase file and the mesh keeps or loses its colours.
 *
 * ### The texture matrix
 *
 * The sixth argument is the texture base name and the fifth-to-last decides the
 * detail level. `UseLOWAssets` picks the low column; a frame list literally
 * named `dummyframes.txt` picks the dummy column.
 *
 *      field     normal                  low                    dummy
 *      +0x14     %s_DIFFUSE_LITE.PNG     %s_DIFFUSE_LOW.PNG     DUMMY_DIFFUSE_LITE.PNG
 *                %s_DIFFUSE.PNG          %s_DIFFUSE2_LOW.PNG    DUMMY_DIFFUSE.PNG
 *      +0x18     %s_DIFFUSE_ICE.PNG      --                     DUMMY_DIFFUSE_ICE.PNG
 *      +0x24     %s_DIFFUSE2_LITE.PNG    %s_DIFFUSE2.PNG        DUMMY_DIFFUSE_LITE.PNG
 *                %s_DIFFUSE_LITE2.PNG
 *      +0x28     %sBABY2.PVR, and if that misses, %sBABILITY2.PVR
 *
 * The babality texture is the only one with a **fallback name**, and the two
 * spellings are what a port's asset extractor has to try in that order.
 *
 * ### Two characters get a texture nobody else does
 *
 *      who == 0x10    JADE_DIFFUSE_GREEN.PNG   ->  +0x1c
 *      who == 6       SINDEL_HAIR_DIFF.PVR     ->  +0x20
 *
 * 16 is JADE and 6 is SINDEL in [ROSTER.md](../../docs/ROSTER.md), and these
 * are the asset names agreeing with a table that was built from switch
 * statements and symbol gaps. Jade's green is her invisibility tint and
 * Sindel's is her hair, which is the only hair in the game with its own sheet.
 *
 * ### The struct
 *
 * `limeMalloc("animatedcharacter", 0x44)`, so an `ANIMATEDCHARACTER` is
 * **68 bytes**, and the per-mesh record is **0x58 = 88**.
 *
 * ### All three failure paths print and then crash
 *
 * The struct allocation, the meshbase file and the mesh-record allocation each
 * have a check, and each one calls `Error` and then **branches back into the
 * normal flow with the null pointer still in hand**:
 *
 *      "LAC: outofmem1"            -> then writes the skin through the null
 *      "LAC: Filenotfound"         -> then reads the header out of the null
 *      "LAC: meshbase out of ram"  -> then fills records through the null
 *
 * None of them returns. So the messages are what you would see a fraction of a
 * second before the crash, not instead of it. A port that turns them into a
 * clean failure is changing behaviour, and should say so.
 *
 * ### The three trailing longs
 *
 * `p` gates the lite texture set, `q` gates loading the scene at all, and `r`
 * gates the babality sheet and doubles as the last two arguments to
 * `limeLoadTexture`. `Load1Character` passes `(a, 1, b)` and
 * `Preload1Character` passes `(a, b, who == PLAYER1MODEL)`, which is as far as
 * the call sites pin them down -- not far enough to name them, so they keep the
 * prototype's letters.
 */

#define ANIMCHAR_BYTES     0x44
#define MESHREC_BYTES      0x58
#define FRAMELIST_ENTRIES  7244         /* 0x3898 bytes of int16 */
#define WANTFRAMES_BYTES   0x1c4c       /* one flag a frame */
#define FATAL_FRAME_BIT    0x8000
#define JADE               0x10
#define SINDEL             6

extern signed char  WantFrames[];   /* 0x00295d24, WANTFRAMES_BYTES of them */
extern long        *WantDoingFatalFrames;            /* pointer slot -> 0x0017135c */
extern long  UseLOWAssets;                   /* 0x0010df10 */

/* Tag first, then size -- 0x0005c3d8 loads the string into r0 and 0x44
 * into r1. This file had the two the other way round, which made every
 * allocation here ask for a string's worth of bytes. */
void *limeMalloc(const char *tag, long size);
void *limeLoadTexture(const char *path, long a, long b);
void *LIME_LoadSkin(const char *path);
void *LIME_LoadBones(const char *path);
void *LIME_LoadScene(const char *path, long a, const char *tex, long b);
void  LIME_LoadMeshSetTextures(void *meshset, const char *tex);
void *LIME_SceneMeshSet(void *scene);

/* One 88-byte frame record (`animatedcharacter_meshbase`), two halves: the
 * body and the second skin. LIME_RenderMeshSingleIndexed (0x5e358) reads
 * +0x00/+0x04/+0x1c/+0x20/+0x24 for half 0 and +0x08/+0x0c/+0x2c/+0x30/+0x34
 * for half 1; IsAFrameVisible (0x5b9dc) reads the two flags. This is NOT a
 * MESHINFO, though both are 88 bytes: MESHINFO's +0x18 is a vertex pointer,
 * this one's is a flag. */
typedef struct MESHREC {
    long   colsSize;                    /* 0x00  vertices, half 0 */
    long   field04;                     /* 0x04  faces, half 0 (skin +8) */
    long   cols2Size;                   /* 0x08  vertices, half 1 */
    long   field0c;                     /* 0x0c  faces, half 1 (skin2 +8) */
    uint8_t _pad10[8];                  /* 0x10 */
    long   hasCols;                     /* 0x18  half 0 drawn */
    void  *P32 indexes;                     /* 0x1c  animatedcharacter_indexes */
    void  *P32 uvs;                         /* 0x20  animatedcharacter_UV */
    void  *P32 cols;                        /* 0x24  animatedcharacter_cols */
    long   hasCols2;                    /* 0x28  half 1 drawn */
    void  *P32 indexes2;                    /* 0x2c */
    void  *P32 uvs2;                        /* 0x30 */
    void  *P32 cols2;                       /* 0x34  animatedcharacter_cols2 */
    uint8_t _pad38[MESHREC_BYTES - 0x38];
} MESHREC;

extern uint16_t RenderIndexes[];        /* 0x0036a820, slot 0x000f3244 */
int WantThisFrame(long frame);

/* Port, not in the binary: the select screen's (and p's) fighters load the
 * _LITE skins, 256 square. From IPAD up the fight's own skin is taken
 * instead, 1024 square and on the same layout -- _DIFFUSE2_LITE and
 * _DIFFUSE_LITE2 become _DIFFUSE2, _DIFFUSE_LITE becomes _DIFFUSE -- and
 * the _LITE one is still the fallback. */
extern int PortTextureQuality;          /* runtime/draw_gl.c */

static TEXTURE *port_load_lite(const char *path)
{
    static const char *const from[3] = { "_DIFFUSE2_LITE.", "_DIFFUSE_LITE2.",
                                         "_DIFFUSE_LITE." };
    static const char *const to[3]   = { "_DIFFUSE2.", "_DIFFUSE2.",
                                         "_DIFFUSE." };
    char big[200];
    const char *hit;
    TEXTURE *t;
    int i;

    if (PortTextureQuality > 0) {
        for (i = 0; i < 3; i++) {
            hit = strstr(path, from[i]);
            if (hit != NULL && strlen(path) < sizeof(big) - 8) {
                snprintf(big, sizeof(big), "%.*s%s%s", (int)(hit - path), path,
                         to[i], hit + strlen(from[i]));
                t = limeLoadTexture(big, 0, 0);
                if (t != NULL)
                    return t;
                break;
            }
        }
    }
    return limeLoadTexture((char *)path, 0, 0);
}

/* Port, not in the binary. The `.lighting` file is baked for the game's own
 * skin: `need` bytes, a colour per unwelded vertex per frame. A skin brought
 * in with REPLACE MODEL has its own vertex count, and slicing the file by it
 * read past its end -- colours from the heap, the fighter drawn black. When
 * the file does not fit, every frame gets the file's average light (a mid
 * grey without one), flat but the model's own. */
long limeFileSize(const char *name);    /* the platform's, as text.c */

static void *port_fit_lighting(void *lighting, const char *path, size_t need)
{
    size_t have = lighting ? (size_t)limeFileSize(path) : 0, k;
    unsigned long sum = 0;
    unsigned char *out;

    if (lighting && have >= need)
        return lighting;
    for (k = 0; k < have; k++)
        sum += ((const unsigned char *)lighting)[k];
    out = (unsigned char *)limeMalloc("port_lighting", need ? need : 1);
    if (out == NULL)
        return lighting;
    memset(out, have ? (int)(sum / have) : 0xb4, need);
    printf("LAC: %s is %lu bytes, the skin needs %lu: flat light\n", path,
           (unsigned long)have, (unsigned long)need);
    if (lighting)
        limeFree(lighting);
    return out;
}

/* Port, not in the binary. The `.lighting` bake, made here for a skin a mod
 * brings (REPLACE MODEL), so it is shaded as the game's own fighters are
 * instead of flat. The shipped bakes are exactly what DrawSkinnedMesh2's lit
 * path gives (flags 1): each animation frame posed, each vertex's skinned
 * normal through LightVert with the lights the binary ships -- light 1 from
 * (0, -300, -50), power 2, exponent 3.5, light 0 at power 0 -- times 255,
 * one byte an unwelded vertex. Checked against the game's own files: Sonya,
 * Scorpion and Liu Kang come out byte for byte (scratch script over the
 * launcher's skin reader). Frame i is the meshbase's frame i, skin 1's
 * bytes then skin 2's, as LoadAnimatedCharacter slices them. The light
 * globals are set for the bake and put back. A bake is kept for the
 * session by the skin's file (BAKE_KEEP): the menu's fighter and every fight
 * with that model took it again, a second or so each time. */
extern float g_lightDir0[3], g_lightDir1[3];   /* lime/lime_globals.c */
double plat_time(void);
#define BAKE_KEEP 8
static struct { char key[160]; long frames; size_t per; unsigned char *px; } g_bake[BAKE_KEEP];
extern float g_lightPower0, g_lightPower1, g_lightExp0, g_lightExp1;
static void *port_bake_lighting(ANIMATEDCHARACTER *c, SKININFO *skin,
                                SKININFO *s2, long frames, long size1,
                                long size2, const char *key)
{
    char *P32 *w = (char *P32 *)c;
    const long *n = (const long *)c;
    size_t per = (size_t)(size1 + size2);
    unsigned char *out;
    float d0[3], d1[3], p0, p1, e0, e1;
    long f;
    int k;
    double t0 = plat_time();

    if (frames <= 0 || per == 0)
        return NULL;
    out = (unsigned char *)limeMalloc("port_lighting", (size_t)frames * per);
    if (out == NULL)
        return NULL;
    for (k = 0; key && k < BAKE_KEEP; k++)
        if (g_bake[k].px && g_bake[k].frames == frames && g_bake[k].per == per
            && strcmp(g_bake[k].key, key) == 0) {
            memcpy(out, g_bake[k].px, (size_t)frames * per);
            return out;
        }
    memcpy(d0, g_lightDir0, sizeof d0);
    memcpy(d1, g_lightDir1, sizeof d1);
    p0 = g_lightPower0; p1 = g_lightPower1;
    e0 = g_lightExp0;   e1 = g_lightExp1;
    g_lightDir0[0] = 68.0f; g_lightDir0[1] = 462.0f;  g_lightDir0[2] = 247.0f;
    g_lightDir1[0] = 0.0f;  g_lightDir1[1] = -300.0f; g_lightDir1[2] = -50.0f;
    g_lightPower0 = 0.0f;   g_lightExp0 = 0.8f;
    g_lightPower1 = 2.0f;   g_lightExp1 = 3.5f;
    for (f = 0; f < frames; f++) {
        GenerateMatrices(w[0x3c / 4], (BONESINFO *)w[0x34 / 4], f, f, 0.0f,
                         n[0x40 / 4]);
        DrawSkinnedMesh2(skin, 0, 0, 1, RenderVerts, RenderUVs, RenderRGBs,
                         0, 1);
        memcpy(out + (size_t)f * per, RenderRGBs, (size_t)size1);
        if (s2 != NULL && size2 > 0) {
            DrawSkinnedMesh2(s2, 0, 0, 1, RenderVerts, RenderUVs, RenderRGBs,
                             0, 1);
            memcpy(out + (size_t)f * per + size1, RenderRGBs, (size_t)size2);
        }
    }
    memcpy(g_lightDir0, d0, sizeof d0);
    memcpy(g_lightDir1, d1, sizeof d1);
    g_lightPower0 = p0; g_lightPower1 = p1;
    g_lightExp0 = e0;   g_lightExp1 = e1;
    printf("LAC: lighting baked for a mod's model, %ld frames of %lu bytes, %.0f ms\n",
           frames, (unsigned long)per, (plat_time() - t0) * 1000.0);
    if (key && strlen(key) < sizeof g_bake[0].key) {
        for (k = 0; k < BAKE_KEEP - 1 && g_bake[k].px; k++)
            ;
        free(g_bake[k].px);             /* full: the last one goes */
        g_bake[k].px = (unsigned char *)malloc((size_t)frames * per);
        if (g_bake[k].px) {
            memcpy(g_bake[k].px, out, (size_t)frames * per);
            strcpy(g_bake[k].key, key);
            g_bake[k].frames = frames;
            g_bake[k].per = per;
        }
    }
    return out;
}

ANIMATEDCHARACTER *LoadAnimatedCharacter(char *scene, char *skinFile,
                                         char *bonesFile, char *lightingName,
                                         char *meshbaseFile, char *texBase,
                                         char *frameListName,
                                         EPLAYER who, FRONTEND_CHARACTER *fe,
                                         long p, long q, long r)
{
    char  lightingPath[0x80];           /* sp+0x38 */
    char  name[0x80];                   /* sp+0xb8 */
    char  framePath[0x80];              /* sp+0x138 */
    char  texPath[0x80];                /* sp+0x1b8 */
    ANIMATEDCHARACTER *c;
    const short *list;
    void  *lighting;
    long   i, meshCount, size1 = 0, size2 = 0;

    /* the frame-list name: uppercased in place and cut at the first '.' */
    memcpy(name, frameListName, strlen(frameListName));
    {
        char *p = name;
        while (*p != '.') {
            if ((unsigned char)(*p - 'a') <= 25)
                *p = (char)(*p - 0x20);
            p++;
        }
        *p = '\0';
    }

    for (i = 0; i < WANTFRAMES_BYTES; i++)
        WantFrames[i] = 1;

    /* Four 128-byte stack buffers, at sp+0x38, +0xb8, +0x138 and +0x1b8. The
     * original bounds none of them: the memcpy above copies strlen(frameListName)
     * bytes into a 128-byte buffer, and these two sprintf a 128-byte name into
     * another 128-byte buffer with a prefix. Nothing but the asset names being
     * short keeps either inside. Kept as the original has it -- the warning gcc
     * raises here is about the shipped code, not about this transcription. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-overflow"
    sprintf(lightingPath, "STATICLIGHTING/%s.lighting", lightingName);
    sprintf(framePath, "framelists/%s.bin", name);
#pragma GCC diagnostic pop

    c = (ANIMATEDCHARACTER *)limeMalloc("animatedcharacter",
                                        sizeof(ANIMATEDCHARACTER));
    if (c == NULL)
        Error("LAC: outofmem1\n");      /* and then carries straight on */

    c->skin = LIME_LoadSkin(skinFile);
    c->bones = LIME_LoadBones(bonesFile);
    port_skin_bones_loaded(c->bones, skinFile); /* port: skin_mats.c */
    c->frameList = limeLoadFile(framePath);

    /* ---- the frame list, and the finisher-frame gate ---- */
    list = (const short *)c->frameList;
    for (i = 0; i != FRAMELIST_ENTRIES; i++) {
        if (list[i] == -1)
            continue;

        if ((unsigned short)list[i] & FATAL_FRAME_BIT) {
            if (!*WantDoingFatalFrames) {
                /* not wanted: drop the frame entirely */
                ((short *)list)[i] = -1;
                WantFrames[(unsigned short)list[i] & 0x7fff] = 0;
                FrameRemapTable[i][0] = -1;
                continue;
            }
            ((short *)list)[i] = (short)((unsigned short)list[i]
                                         & ~FATAL_FRAME_BIT);
        }
        FrameRemapTable[i][0] = who;
        FrameRemapTable[i][1] = list[i];
    }

    /* ---- scene and mesh textures ---- */
    if ((q != 0 && fe == NULL) || (who == 9 && fe != NULL)) {
        c->scene = LIME_LoadScene(scene, 1, texBase, 0);
        if (c->scene)
            /* `->meshset`, not word 32. lime.h states SCENEINFO, so the field
             * has a name and the compiler can find it; 0x80 is where it sits in
             * the IMAGE, and on the host the 64-byte name and the pointers
             * ahead of it do not add up to the same place. Spelled as an
             * offset it handed LIME_LoadMeshSetTextures the characters of
             * "KUNGLAO_" as an address. */
            LIME_LoadMeshSetTextures(LIME_SceneMeshSet(c->scene), texBase);
    } else {
        /* 0x5c9f8: `str r3, [r4, #0x10]` with r3 == 0. limeMalloc does not
         * clear, so without this the front end's fighters (no scene) handed
         * RenderPlayer's attach pass whatever the heap held. */
        c->scene = NULL;
    }

    /* ---- the meshbase file: a header then one block a mesh ---- */
    {
        const long *base = (const long *)limeLoadFile(meshbaseFile);
        const long *hdr;

        if (base == NULL)
            Error("LAC: Filenotfound\n");
        hdr = base + 1 + ((who == 6) ? 1 : 0);
        c->meshNames = (const long *)(uintptr_t)hdr[1];
        meshCount = hdr[0];
        c->meshTable = hdr + 2;
        c->meshCount = meshCount;

        if (fe)
            ((long *)fe)[0x5f0 / 4] =
                CreateFramesWeWantFromFIDs(WantedFrames, who,
                                           (const long *)((char *)fe + 0x5f4));

        c->meshes = limeMalloc("animatedcharacter_meshbase",
                               meshCount * sizeof(MESHREC));
        if (c->meshes == NULL)
            Error("LAC: meshbase out of ram\n");

        lighting = limeLoadFile(lightingPath);

        /* SKININFO is opaque here: +0x00 is the second skin, +0x08 the
         * face count (lime.h names it numVerts). */
#define SKIN_NEXT(s)  (*(SKININFO *P32 *)(void *)(s))
#define SKIN_FACES(s) (((const int *)(const void *)(s))[2])
        /* ---- one record a frame (armv7 0x5c530..0x5c78a) ----
         *
         * The loop runs to meshCount - 1, not meshCount, and not at all when
         * the count is 1 (0x5c530). Frame i's anim header is
         * meshTable + meshNames * i (the data and the stride GenerateMatrices
         * is given); its first float decides whether the frame is lit.
         *
         * A frame is built when it is wanted -- WantThisFrame(i) for a
         * front-end character, WantFrames[i] otherwise -- and, for the latter,
         * when it is frame 0 or its header float is above 0.5. Character 6's
         * frame 365 is always built. Frame 0 also measures the two vertex
         * counts with DrawSkinnedMesh2 and keeps the UVs and indexes it
         * produced; every later frame shares frame 0's.
         *
         * The version this replaces skipped every frame of a character with
         * no second skin, never kept the UVs and indexes, and so left the
         * flag at +0x18 zero on every frame: nothing was ever drawn. */
        if (meshCount != 1) {
            SKININFO *skin = (SKININFO *)c->skin;
            MESHREC  *m0   = &((MESHREC *)c->meshes)[0];
            long      six  = (who == 6);

            for (i = 0; i != meshCount - 1; i++) {
                MESHREC *m = &((MESHREC *)c->meshes)[i];
                SKININFO *s2;
                const float *mb =
                    (const float *)((const char *)c->meshTable
                                    + (long)(uintptr_t)c->meshNames * i);
                long first   = (i == 0);
                long special = six && i == 0x16d;
                long want;
                const char *src;

                m->field04 = SKIN_FACES(skin);
                s2 = SKIN_NEXT(skin);
                m->field0c = s2 ? SKIN_FACES(s2) : 0;

                if (special)
                    want = 1;
                else
                    want = first || mb[0] > 0.5f;

                if (fe != NULL && WantThisFrame(i)) {
                    m->cols  = NULL;
                    m->cols2 = NULL;
                } else {
                    if (fe != NULL || WantFrames[i] == 0)
                        want = 0;
                    m->cols  = NULL;
                    m->cols2 = NULL;
                    if (!(first || want)) {
                        m->hasCols  = 0;
                        m->uvs      = NULL;
                        m->indexes  = NULL;
                        m->cols     = NULL;
                        m->hasCols2 = 0;
                        m->uvs2     = NULL;
                        m->indexes2 = NULL;
                        m->cols2    = NULL;
                        goto dim;
                    }
                }

                /* flags = 1 and keepFloats = 1: `movs r3, #1` at 0x5c5f6 and
                 * 0x5c626 is stored as the fifth stack word AND left in r3. */
                if (first) {
                    s2 = SKIN_NEXT(skin);
                    size2 = s2 ? DrawSkinnedMesh2(s2, 0, 0, 1, RenderVerts,
                                                  RenderUVs, RenderRGBs, 0, 1)
                               : 0;
                    size1 = DrawSkinnedMesh2(skin, 0, 0, 1, RenderVerts,
                                             RenderUVs, RenderRGBs, 0, 1);
                    /* port: a mod's model gets its own bake, else the
                     * file (flat light when it does not fit) */
                    {
                        void *own = port_skin_custom(skin)
                            ? port_bake_lighting(c, skin, SKIN_NEXT(skin),
                                                 meshCount - 1, size1, size2, skinFile)
                            : NULL;
                        if (own != NULL) {
                            if (lighting)
                                limeFree(lighting);
                            lighting = own;
                        } else
                            lighting = port_fit_lighting(lighting, lightingPath,
                                                         (size_t)(meshCount - 1)
                                                         * (size_t)(size1 + size2));
                    }
                }

                m->cols = limeMalloc("animatedcharacter_cols", size1);
                src = (const char *)lighting + i * (size2 + size1);
                memcpy(m->cols, src, size1);

                if (first) {
                    m->uvs = limeMalloc("animatedcharacter_UV", size1 * 8);
                    memcpy(m->uvs, RenderUVs, size1 * 8);
                    m->indexes = limeMalloc("animatedcharacter_indexes",
                                            SKIN_FACES(skin) * 6);
                    memcpy(m->indexes, RenderIndexes, SKIN_FACES(skin) * 6);
                }

                s2 = SKIN_NEXT(skin);
                if (s2 == NULL) {
                    m->uvs2     = NULL;
                    m->indexes2 = NULL;
                    m->cols2    = NULL;
                } else {
                    if (first) {
                        /* measured again, so RenderUVs and RenderIndexes
                         * hold the second skin's (0x5c9de) */
                        size2 = DrawSkinnedMesh2(s2, 0, 0, 1, RenderVerts,
                                                 RenderUVs, RenderRGBs, 0, 1);
                        src = (const char *)lighting + i * (size2 + size1);
                    }
                    m->cols2 = limeMalloc("animatedcharacter_cols2", size2);
                    memcpy(m->cols2, src + size1, size2);
                    if (first) {
                        m->uvs2 = limeMalloc("animatedcharacter_UV", size2 * 8);
                        memcpy(m->uvs2, RenderUVs, size2 * 8);
                        m->indexes2 = limeMalloc("animatedcharacter_indexes",
                                                 SKIN_FACES(s2) * 6);
                        memcpy(m->indexes2, RenderIndexes, SKIN_FACES(s2) * 6);
                    }
                }

                m->hasCols   = 1;
                m->hasCols2  = 1;
                m->colsSize  = size1;
                m->cols2Size = size2;

                if (i == 0)
                    continue;

                m->indexes  = m0->indexes;
                m->uvs      = m0->uvs;
                m->indexes2 = m0->indexes2;
                m->uvs2     = m0->uvs2;

            dim:
                /* a dim frame gives up its colours (0x5c6f8) */
                if (mb[0] <= 0.5f && i != 0 && !special) {
                    m->hasCols = 0;
                    m->uvs     = NULL;
                    m->indexes = NULL;
                    if (m->cols)
                        limeFree(m->cols);
                    m->cols     = NULL;
                    m->hasCols2 = 0;
                    m->uvs2     = NULL;
                    m->indexes2 = NULL;
                    if (m->cols2)
                        limeFree(m->cols2);
                    m->cols2 = NULL;
                }
            }
        }
#undef SKIN_NEXT
#undef SKIN_FACES

        c->meshbase = base;   /* the meshbase file is kept */
        limeFree(lighting);                    /* the lighting file is not */
    }

    c->diffuse = NULL;
    c->diffuse2 = NULL;
    c->babality = NULL;

    /* ---- the textures ---- */
    if (fe != NULL || p != 0) {
        int dummy = (strcmp(frameListName, "dummyframes.txt") == 0);

        if (fe != NULL) {
            if (dummy)
                strcpy(texPath, "DUMMY_DIFFUSE_LITE.PNG");
            else
                sprintf(texPath, "%s_DIFFUSE2_LITE.PNG", texBase);
            c->diffuse2 = port_load_lite(texPath);

            if (c->diffuse2 == NULL) {
                if (dummy)
                    strcpy(texPath, "DUMMY_DIFFUSE_LITE.PNG");
                else
                    sprintf(texPath, "%s_DIFFUSE_LITE2.PNG", texBase);
                c->diffuse2 = port_load_lite(texPath);
            }
        }

        if (dummy)
            strcpy(texPath, "DUMMY_DIFFUSE_LITE.PNG");
        else
            sprintf(texPath, "%s_DIFFUSE_LITE.PNG", texBase);
        c->diffuse = port_load_lite(texPath);
    } else {
        int dummy = (strcmp(frameListName, "dummyframes.txt") == 0);

        if (dummy) {
            strcpy(texPath, "DUMMY_DIFFUSE.PNG");
        } else if (r != 0) {
            if (UseLOWAssets)
                sprintf(texPath, "%s_DIFFUSE2_LOW.PNG", texBase);
            else
                sprintf(texPath, "%s_DIFFUSE2.PNG", texBase);
        } else {
            if (UseLOWAssets)
                sprintf(texPath, "%s_DIFFUSE_LOW.PNG", texBase);
            else
                sprintf(texPath, "%s_DIFFUSE.PNG", texBase);
        }
        /* (0, 0), not (r, r): every arm reaches limeLoadTexture with r1 =
         * r2 = 0 (0x5cab8 r5 is 0 there, 0x5cb46 r is 0 there, 0x5c7ec). */
        c->diffuse = limeLoadTexture(texPath, 0, 0);
    }

    /* The ice sheet, for BOTH paths: the fight path's arms all branch back
     * to 0x5c7f8 after the diffuse (0x5cac8, 0x5cb58, 0x5cbee). Read as
     * front-end only, a fighter in a fight had no +0x18, so a frozen one
     * (LightPlayers' +0x534 -> anim[0x18]) and Sub-Zero's ice clone drew
     * with no texture: plain white. */
    if (strcmp(frameListName, "dummyframes.txt") == 0)
        strcpy(texPath, "DUMMY_DIFFUSE_ICE.PNG");
    else
        sprintf(texPath, "%s_DIFFUSE_ICE.PNG", texBase);
    c->diffuseIce = limeLoadTexture(texPath, 0, 0);

    /* the babality sheet is the only texture with a fallback spelling */
    if (r != 0) {
        if (strcmp(frameListName, "dummyframes.txt") == 0)
            strcpy(texPath, "DUMMY_DIFFUSE_ICE.PNG");
        else
            sprintf(texPath, "%sBABY2.PVR", texBase);
        c->babality = limeLoadTexture(texPath, 0, 0);

        if (c->babality == NULL) {
            sprintf(texPath, "%sBABILITY2.PVR", texBase);
            c->babality = limeLoadTexture(texPath, 0, 0);
        }
    } else {
        c->babality = NULL;
    }

    /* two characters, two textures nobody else has */
    c->jadeGreen = (who == JADE)
        ? limeLoadTexture("JADE_DIFFUSE_GREEN.PNG", 0, 0) : NULL;
    c->sindelHair = (who == SINDEL)
        ? limeLoadTexture("SINDEL_HAIR_DIFF.PVR", 0, 0) : NULL;

    return c;
}
