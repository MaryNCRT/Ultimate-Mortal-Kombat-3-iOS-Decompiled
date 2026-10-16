/*
 * training.c — src/gamecode/training.cpp
 *
 * Training mode's on-screen prompts: the move to perform, and the "well done"
 * and "try again" messages that replace it for a moment afterwards.
 * Hand-written from the disassembly of the armv7 slice.
 */

#include <stdint.h>

/* `_TrainingData` -- 0x001782e8. The indexing in `TrainingMessages` gives the
 * whole shape away without any of it being written down anywhere:
 *
 *      byte offset = character * 288 + category * 96 + move * 24
 *
 * so an entry is **24 bytes**, there are **4 moves** per category (4 * 24 = 96),
 * **3 categories** per character (3 * 96 = 288), and the array is indexed by
 * `Character1` first.
 *
 * The same function then reads a second string at `base - 12 + Settings[4] * 4`
 * where `Settings[4]` is the button layout, 4, 5 or 6. That resolves to
 * `base + 4`, `base + 8` and `base + 12` -- so the first four words of an entry
 * are the move's name and its notation written three ways, one per layout, and
 * the remaining two words are not read here.
 *
 * A NULL name is the end marker: the move counter wraps to 0 when it lands on
 * one, which is how a character with fewer than four moves in a category still
 * cycles correctly. */
#define TRAINING_ENTRY_SIZE   24
#define TRAINING_MOVES        4
#define TRAINING_CATEGORIES   3

typedef struct TRAININGMOVE {
    const char *P32 name;                   /* 0x00  NULL ends the category */
    const char *P32 notation4;              /* 0x04  the 4-button layout */
    const char *P32 notation5;              /* 0x08  the 5-button layout */
    const char *P32 notation6;              /* 0x0c  the 6-button layout */
    long        pad10;                  /* 0x10 */
    /* 0x14 -- a pointer to the flag the move raises, or NULL. The table holds
     * 0x0014fb30 on the fatality row and 0x0014fb2c on the animality one, which
     * are `FatalityMessage` and `AnimalityMessage`; every other row is zero. */
    long       *P32 messageFlag;            /* 0x14 */
} TRAININGMOVE;

/* character-major, then category, then move */
extern TRAININGMOVE TrainingData[];     /* 0x001782e8 */

extern float TrainingGoodMessage;       /* 0x001780a0 */
extern float TrainingBadMessage;        /* 0x001780a4 */
extern long  TrainingMoveCount;         /* 0x001780a8 */
extern long  TrainingCatagory;          /* 0x0017809c, the binary's spelling */

extern int   Settings[10];              /* pointer slot -> 0x00100e34 */
extern long  Character1;                /* pointer slot */
extern long  PLAYER1MODEL;              /* pointer slot */
extern long PLAYER2MODEL;              /* 0x0014e1b8, a plain long */
extern int   limeScreenWidth;           /* pointer slot */
extern float  limeFPSScaleFactor;       /* pointer slot */
extern float  fontcol[];                  /* pointer slot -> 0x0014f9f0 */
extern char   GameFont[];               /* 0x001abb98, the font itself */

extern long  BabalityMessage;           /* pointer slot */
extern long  FriendshipMessage;         /* pointer slot */
extern long  AnimalityMessage;          /* pointer slot */
extern long  FatalityMessage;           /* pointer slot */

void limeDrawFONT(void *font, const char *text, float x, float y,
                  long align, float scale, const float *colour);
int  sprintf(char *dst, const char *fmt, ...);

/* The fight logic's entry points. `mk3_init`'s third argument is a FUNCTION
 * POINTER: the slot it comes from holds 0x0001c675, and 0x0001c674 is
 * `FrameID_GetBBox` (decomp/gamecode/GameCode.c) with the Thumb bit set. So
 * this is how `gamecode/logic` reaches frame bounding boxes -- it is handed
 * the lookup rather than calling it directly, which is worth knowing before
 * that module is decompiled. */
long mk3_init(long p1model, long p2model,
              void (*getBBox)(void), long flag);   /* returns 0; flag is unread */
void mk3_dizzy(void);
void LIME_KillAllEvents(void);

/* slot 0x000f33f8 is the non-lazy pointer to _FrameID_GetBBox (0x1c675):
 * the function's address, not a variable. */
void FrameID_GetBBox(long fid, long *x0, long *y0, long *x1, long *y1);
#define FrameID_GetBBoxPtr ((void (*)(void))FrameID_GetBBox)


/* ---------------------------------------------------------- TrainingMessages
 *
 * armv7 0x000a7930, 884 bytes.  **Complete.**
 *
 * Called once a frame while training is running. It is a three-way choice, and
 * the two message states take priority over the prompt:
 *
 *      TrainingGoodMessage > 0   draw "WELL DONE", count it down
 *      TrainingBadMessage  > 0   draw "TRY AGAIN", count it down
 *      otherwise                 draw the current move's name and notation
 *
 * Both counters tick by **-1.0 / limeFPSScaleFactor** in double precision and
 * are stored back as floats, so the messages last a fixed wall-clock time
 * rather than a fixed number of frames.
 *
 * ### The two messages are drawn with raw coordinates
 *
 *      limeDrawFONT(GameFont, text, limeScreenWidth / 2, 66.0f, 1, 1.0f, fontcol)
 *
 * `66.0f` and a scale of `1.0f`, with no `FE_Y` and no `FE_WidthScale`. This
 * is in-game text, not front-end text, and it does not participate in the
 * front end's scaling at all -- so on a screen where those differ the training
 * prompt sits somewhere the menus never would.
 *
 * The strings are **compiled-in English literals**, not `GameText` ids:
 * `"WELL DONE"`, `"TRY AGAIN"` and the `"%s : %s"` that joins the move name to
 * its notation. Training mode is not translated.
 *
 * ### What happens when a message finishes
 *
 * Both endings call
 *
 *      mk3_init(PLAYER1MODEL, PLAYER2MODEL, FrameID_GetBBox, flag)
 *      if (TrainingCatagory == 2) mk3_dizzy();
 *      LIME_KillAllEvents();
 *      BabalityMessage = FriendshipMessage = AnimalityMessage = FatalityMessage = 0;
 *
 * -- the fight is rebuilt from scratch between every training attempt, and
 * **category 2 additionally dizzies the opponent**, which is what makes the
 * finishing-move category practisable at all.
 *
 * Clearing the four finisher banners is why a failed babality attempt does not
 * leave its message on screen into the next attempt.
 *
 * ### Advancing the move, and the end marker
 *
 *      TrainingMoveCount = (TrainingMoveCount + 1) % 4;
 *      if (TrainingData[...].name == NULL) TrainingMoveCount = 0;
 *
 * The modulo is signed and written out with the usual sign correction. The
 * NULL check is the only thing that keeps a short category from showing blank
 * prompts, and it only ever wraps to 0 -- it does not search, so a category
 * with a hole in the middle would stop at the hole.
 *
 * **The success path passes the move's name pointer to `mk3_init` as its
 * fourth argument** where the failure path passes a literal 0. Whatever that
 * argument means to the fight logic, "the move you just did" and "nothing" are
 * the two values it gets from here.
 */
void TrainingMessages(void)
{
    char buf[128];                      /* sp+0xc */
    long layout = Settings[4];
    long cat    = TrainingCatagory;

    if (TrainingGoodMessage > 0.0f) {
        limeDrawFONT(GameFont, "WELL DONE",
                     (float)(limeScreenWidth / 2), 66.0f,
                     1, 1.0f, fontcol);

        TrainingGoodMessage =
            (float)((double)TrainingGoodMessage
                    + -1.0 / (double)limeFPSScaleFactor);

        if (TrainingGoodMessage <= 0.0f) {
            const TRAININGMOVE *m;
            long next;

            TrainingGoodMessage = 0.0f;

            next = (TrainingMoveCount + 1) % TRAINING_MOVES;
            TrainingMoveCount = next;

            m = &TrainingData[(Character1 * TRAINING_CATEGORIES
                               * TRAINING_MOVES)
                              + cat * TRAINING_MOVES + next];

            if (m->name == 0)
                TrainingMoveCount = 0;

            mk3_init(PLAYER1MODEL, PLAYER2MODEL, FrameID_GetBBoxPtr,
                     (long)(uintptr_t)m->name);

            if (cat == 2)
                mk3_dizzy();

            LIME_KillAllEvents();
            BabalityMessage   = 0;
            FriendshipMessage = 0;
            AnimalityMessage  = 0;
            FatalityMessage   = 0;
        }
        return;
    }

    if (TrainingBadMessage > 0.0f) {
        limeDrawFONT(GameFont, "TRY AGAIN",
                     (float)(limeScreenWidth / 2), 66.0f,
                     1, 1.0f, fontcol);

        TrainingBadMessage =
            (float)((double)TrainingBadMessage
                    + -1.0 / (double)limeFPSScaleFactor);

        if (TrainingBadMessage <= 0.0f) {
            TrainingBadMessage = 0.0f;

            mk3_init(PLAYER1MODEL, PLAYER2MODEL, FrameID_GetBBoxPtr, 0);

            if (cat == 2)
                mk3_dizzy();

            LIME_KillAllEvents();
            BabalityMessage   = 0;
            FriendshipMessage = 0;
            AnimalityMessage  = 0;
            FatalityMessage   = 0;
        }
        return;
    }

    /* The prompt: "<move name> : <notation for the current layout>". */
    {
        const TRAININGMOVE *m =
            &TrainingData[(Character1 * TRAINING_CATEGORIES * TRAINING_MOVES)
                          + cat * TRAINING_MOVES + TrainingMoveCount];
        const char *P32 const *notation = &m->name;   /* [1]..[3] by layout */

        sprintf(buf, "%s : %s", m->name, notation[layout - 3]);

        limeDrawFONT(GameFont, buf,
                     (float)(limeScreenWidth / 2), 66.0f,
                     1, 1.0f, fontcol);
    }
}
