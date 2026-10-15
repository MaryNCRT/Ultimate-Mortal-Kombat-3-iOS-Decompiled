/*
 * achievements.c — src/gamecode/achievements.cpp
 *
 * Twelve functions in the original. Verified against the oracle by
 * tests/test_gamecode3_diff.c.
 */

#include <stdint.h>

/* **Twenty-four entries, bounded by the symbol table.**
 *
 * This said twenty, honestly labelled as "as far as this function reaches":
 * areAchievementsViewing's loop runs while the byte cursor is not 0x50, which
 * is 20 words, and that is a lower bound rather than a size.
 *
 * achievementsIncreaseSubzeroXerox reaches `[r2, #0x5c]` -- index 23 -- and the
 * compiler caught the contradiction at -Wextra as soon as it was written. The
 * real bound is in the symbol table: `_achievementTracker` sits at 0x00379c60
 * and the next symbol, `_SceneEvents`, at 0x00379cc0, so the block is 96 bytes,
 * 24 words, and index 23 is its last slot exactly.
 *
 * Nothing here says what an entry means, only that 1 is the value being counted
 * and that slot 23 counts Sub-Zero's clones. */
#define ACHIEVEMENT_SLOTS 24

extern int achievementTracker[ACHIEVEMENT_SLOTS];   /* 0x00379c60 */

int areAchievementsViewing(void);


/* -------------------------------------------------- areAchievementsViewing
 *
 * armv7 0x000a02ac, 32 bytes.
 *
 *      movs r0, #0 ; mov r2, r0
 *  L:  mov  r3, r1 ; add r3, pc ; ldr r3, [r2, r3]
 *      cmp  r3, #1 ; it eq ; addeq r0, #1
 *      adds r2, #4 ; cmp r2, #0x50 ; bne L
 *
 * **It returns a COUNT, not a boolean**, despite the name. Every slot equal to
 * exactly 1 adds one to the total, so a caller writing
 * `if (areAchievementsViewing())` gets the right answer by accident and a
 * caller comparing it against 1 does not.
 *
 * Equal to 1, not non-zero. Whatever other values a slot can hold are not
 * counted, which is the difference between "how many are in this state" and
 * "how many are set".
 *
 * The base address is recomputed inside the loop from a register the compiler
 * loaded once -- `mov r3, r1 ; add r3, pc`. That is the one shape
 * `tools/annotate.py` documents as its gap, so 0x00379c60 was resolved by
 * hand: 0x002d99a8 + 0x000a02b8.
 *
 * Another `it eq`, so another check that the IT-block flag fix holds.
 */
int areAchievementsViewing(void)
{
    int count = 0;
    int i;

    /* TWENTY slots, `cmp r2, #0x50` -- the twenty achievementsDraw walks --
     * not the 24 the tracker holds. Counting 24, a slot past 19 left at 1
     * (a save had slot 21 at 1) was "on screen" forever: nothing draws or
     * retires slots 20-23, and every screen fade, which runs ten times slower
     * while a banner shows (Task_GameMain), took five seconds instead of half
     * a second. */
    for (i = 0; i < 0x50 / 4; i++) {
        if (achievementTracker[i] == 1)
            count++;
    }
    return count;
}


/* Returns non-zero when the caller should pop a banner -- see the definition
 * below. This was declared `void` before that function was read. */
int achievementsUnlock(int id);


/* ------------------------------------------ achievementsIncreaseSubzeroXerox
 *
 * armv7 0x000a08dc, 32 bytes.  **Complete.**
 *
 *      ldr  r3, [r2, #0x5c]
 *      adds r3, #1
 *      cmp  r3, #0x63              <- 99
 *      str  r3, [r2, #0x5c]        <- stored BEFORE the branch
 *      ble  out
 *      movs r0, #0x12              <- achievement 18
 *      bl   _achievementsUnlock
 *
 * The counter is written back whether or not the threshold is crossed, and the
 * comparison is `> 99`, so the unlock fires on the hundredth and on every one
 * after it. That is not a bug to tidy: achievementsUnlock is idempotent for an
 * already-unlocked id everywhere else in this file, and a port that fires it
 * only once would differ on a reset save.
 *
 * "Xerox" is Sub-Zero's clone move. Field 0x5c of the tracker counts them.
 */
void achievementsIncreaseSubzeroXerox(void)
{
    achievementTracker[0x5c / 4]++;
    if (achievementTracker[0x5c / 4] > 99)
        achievementsUnlock(0x12);
}


/* --------------------------------------------------------- achievementsReset
 *
 * armv7 0x000a0280, 40 bytes.  **Complete.**
 *
 * Zeroes the whole tracker: `[+0]`, then a loop from `+4` to `+0x4c`, then four
 * unrolled stores at `+0x50`, `+0x54`, `+0x58` and `+0x5c`.
 *
 * **That last store is an independent confirmation of the array's size.** The
 * bound came from the symbol table -- `_achievementTracker` at 0x00379c60 and
 * `_SceneEvents` at 0x00379cc0, so 96 bytes -- and this function clears exactly
 * 0x00 through 0x5c and stops. Twenty-four words, from a completely different
 * direction, agreeing to the word.
 */
void achievementsReset(void)
{
    int i;

    for (i = 0; i < ACHIEVEMENT_SLOTS; i++)
        achievementTracker[i] = 0;
}


/* ------------------------------------------- achievementsIncreaseMatchesWon
 *
 * armv7 0x000a08fc, 44 bytes.  **Complete.**
 *
 *      ldr  r3, [r2, #0x50]
 *      adds r3, #1
 *      cmp  r3, #0xa
 *      str  r3, [r2, #0x50]
 *      beq  unlock 7
 *      cmp  r3, #0x64
 *      beq  unlock 8
 *
 * **Exact equality, where achievementsIncreaseSubzeroXerox uses `>`.** These
 * two fire once each, on the tenth win and the hundredth; the Xerox one fires
 * on the hundredth and on every clone after it. Both are transcribed as
 * written -- the difference is in the original and a port that normalises them
 * to one style changes one of the two.
 *
 * Ten wins is achievement 7, a hundred is 8. Slot 0x50 counts matches won.
 */
void achievementsIncreaseMatchesWon(void)
{
    int n = ++achievementTracker[0x50 / 4];

    if (n == 10)
        achievementsUnlock(7);
    else if (n == 100)
        achievementsUnlock(8);
}


/* --------------------------------------------------------------- getLayoutName
 *
 * armv7 0x000a0234, 48 bytes.  **Complete.**
 *
 * Six strings from two arguments: the button count picks 4, 5 or anything else
 * (which means 6), and the flag picks CUSTOM or PRESET.
 *
 * The default is SIX, not an error -- `cmp 4 / cmp 5 / fall through` -- so a
 * count of 7 returns the six-button name. Transcribed as written; validating
 * the count here would reject inputs the original accepts.
 */
const char *getLayoutName(int buttons, int custom)
{
    if (buttons == 4)
        return custom ? "CUSTOM_LAYOUT_4_BUTTONS" : "PRESET_LAYOUT_4_BUTTONS";
    if (buttons == 5)
        return custom ? "CUSTOM_LAYOUT_5_BUTTONS" : "PRESET_LAYOUT_5_BUTTONS";
    return custom ? "CUSTOM_LAYOUT_6_BUTTONS" : "PRESET_LAYOUT_6_BUTTONS";
}


extern short *H;                        /* pointer slot -> 0x0038c674 */
extern int   *theKode;                  /* pointer slot -> 0x0010ded0 */


/* ------------------------------------------------------ preprocessPostloadKode
 *
 * armv7 0x000a02cc, 56 bytes.  **Complete.**
 *
 * Clears three HALFWORDS in `H` -- +0x18, +0x1a and +0x1c -- and then sets
 * exactly one of them from the active kode:
 *
 *      kode == 2     -> +0x18
 *      kode == 0x12  -> +0x1a
 *      kode == 0     -> +0x1c
 *      anything else -> none of them
 *
 * **Zero is a kode, not the absence of one.** The `cbnz r3` at the end returns
 * without setting anything for any value other than 0, 2 and 0x12, so kode 0
 * has its own flag rather than meaning "no kode selected". A port that treats 0
 * as empty loses whatever +0x1c enables.
 *
 * The clear order is +0x18, +0x1c, +0x1a -- not sequential. It makes no
 * difference and it is transcribed as written.
 */
void preprocessPostloadKode(void)
{
    int kode;

    H[0x18 / 2] = 0;
    H[0x1c / 2] = 0;
    H[0x1a / 2] = 0;

    kode = *theKode;
    if (kode == 2)
        H[0x18 / 2] = 1;
    else if (kode == 0x12)
        H[0x1a / 2] = 1;
    else if (kode == 0)
        H[0x1c / 2] = 1;
}


int puts(const char *s);


extern const char *DestinationNovice[];      /* 0x001769f0 */
extern const char *DestinationWarrior[];     /* 0x00176a80 */
extern const char *DestinationMaster[];      /* 0x00176b20 */
extern const char *DestinationGrandMaster[]; /* 0x00176bd8 */


/* --------------------------------------------------------------- getStageName
 *
 * armv7 0x000a0440, 92 bytes.  **Complete.**
 *
 * Four tables, one per tier, selected by a `tbb` jump table:
 *
 *      0  DestinationNovice
 *      1  DestinationWarrior
 *      2  DestinationMaster
 *      3  DestinationGrandMaster
 *
 * **The default is not an error path even though it prints like one.** A tier
 * above 3 puts "DEFAULT @getStageName!!!" and then reads DestinationNovice --
 * the same table case 0 uses, reached by different code. So an out-of-range
 * tier returns a valid name, loudly.
 *
 * The message ships. Three exclamation marks, like PushFETaskDeferred's: the
 * house style for "this should not happen but here is a sane answer".
 */
const char *getStageName(int tier, int index)
{
    switch (tier) {
        case 0: return DestinationNovice[index];
        case 1: return DestinationWarrior[index];
        case 2: return DestinationMaster[index];
        case 3: return DestinationGrandMaster[index];
        default: break;
    }
    puts("DEFAULT @getStageName!!!");
    return DestinationNovice[index];    /* the same table as case 0 */
}


extern int Settings[10];                /* 0x00100e34 */

/* `_achievementsDescr` -- 0x0017684c, sixteen bytes an entry. Only +0xc is
 * touched here and it is cleared on every unlock; what it holds is not
 * established by this function.
 *
 * **+0x04 is settled elsewhere.** `FE_Task_Achievements`
 * (decomp/gamecode/FrontEnd.c) draws two lines per entry and hands +0x04 to
 * `GameText` exactly as it does +0x00 -- so an entry carries two text ids, the
 * name and the description, and this file simply never reads the second. */
typedef struct ACHIEVEMENTDESCR {
    long id;                            /* 0x00, the GameText id of the label */
    long descr;                         /* 0x04, the GameText id of the
                                         *       description line */
    long pad08;                         /* 0x08 */
    /* +0x0c is a FLOAT -- achievementsDraw runs sin() on it. achievementsUnlock
     * clears it with an integer zero store, which is the same bit pattern, so
     * the union keeps both honest. */
    union { float f; long w; } timer;   /* 0x0c */
} ACHIEVEMENTDESCR;

extern ACHIEVEMENTDESCR achievementsDescr[];    /* 0x0017684c */


/* -------------------------------------------------------- achievementsUnlock
 *
 * armv7 0x000a0854, 136 bytes.  **Complete.**
 *
 * Marks achievement `id` unlocked. The value written into the tracker is
 * **not a boolean -- it is one of three**, and which one depends on
 * `Settings[7]` and on whether the achievements screen is open right now:
 *
 *      Settings[7] == 0                  ->  tracker = 2, return 0
 *      Settings[7] != 0, screen open     ->  tracker = 4, return 1
 *      Settings[7] != 0, screen closed   ->  tracker = 1, return 1
 *
 * The return value is what the caller uses to decide whether to pop a banner,
 * and it is zero in the first case -- so `Settings[7]` is the notifications
 * toggle, unlocking silently when it is off.
 *
 * **1 is the value areAchievementsViewing counts.** That function scans the
 * tracker for entries equal to 1, so an achievement unlocked while the screen
 * is already open gets 4 instead, deliberately keeping it out of that count.
 * The two functions only make sense read together.
 *
 * Every path clears `achievementsDescr[id].timer`, and an id above 0x13 (19) is
 * rejected -- four short of the 24 slots the tracker really has, which is the
 * gap slot 23 Sub-Zero clone counter lives in. So the tail of the tracker is
 * not reachable through this function.
 *
 * An already-unlocked achievement returns 0 and changes nothing.
 */
int achievementsUnlock(int id)
{
    if (Settings[7] == 0) {
        if (id > 0x13 || achievementTracker[id] != 0)
            return 0;

        achievementTracker[id]      = 2;
        achievementsDescr[id].timer.w = 0;
        return 0;
    }

    if (id > 0x13 || achievementTracker[id] != 0)
        return 0;

    achievementTracker[id]      = areAchievementsViewing() ? 4 : 1;
    achievementsDescr[id].timer.w = 0;
    return 1;
}


/* `_kodes` -- 0x00176c04, 32 bytes an entry, terminated by -1 in word 0.
 * Six words compared, word 6 the text drawKodeTip shows, word 7 the payload. */
typedef struct KODE {
    long seq[6];                        /* 0x00 .. 0x14 */
    long text;                          /* 0x18, GameText id: drawKodeTip */
    long value;                         /* 0x1c */
} KODE;

extern KODE  kodes[];                   /* 0x00176c04 */
extern int   KodeSelector[10];          /* 0x000ff8f8 */
/* `theKode` is already declared above as the pointer-slot idiom this file uses:
 * the slot holds 0x0010ded0 and the value lives behind it. */

int  puts(const char *s);
int  printf(const char *fmt, ...);


/* --------------------------------------------------------------- checkIfKode
 *
 * armv7 0x000a04a8, 136 bytes.  **Complete.**
 *
 * Walks `_kodes` comparing each entry six words against the player dial
 * positions, and on a match publishes that entry payload in `_theKode`.
 *
 * **The six words it reads are KodeSelector[0..2] and KodeSelector[7..9]** --
 * indices 0, 1, 2, 7, 8, 9, skipping four in the middle. Those are exactly the
 * six words `resetKodeSelector` clears, and that function comment had the gap
 * recorded as an open question. It is answered: the six that are reset are the
 * six that are compared, and the four in between are not part of a kode.
 *
 * Three functions now agree on this array. InitKodeScreen clears ten words,
 * resetKodeSelector clears six of them, and this one reads those same six.
 *
 * **It does not stop at the first match.** The loop runs to the terminator
 * either way, so with two entries sharing a sequence the LAST one wins. It also
 * prints on every hit:
 *
 *      CHECKING KODE
 *      KODE OK:%d
 *
 * Both survive in the retail binary.
 */
void checkIfKode(void)
{
    KODE *k = kodes;

    puts("CHECKING KODE");

    while (k->seq[0] != -1) {
        if (k->seq[0] == KodeSelector[0] &&
            k->seq[1] == KodeSelector[1] &&
            k->seq[2] == KodeSelector[2] &&
            k->seq[3] == KodeSelector[7] &&
            k->seq[4] == KodeSelector[8] &&
            k->seq[5] == KodeSelector[9]) {
            *theKode = k->value;
            printf("KODE OK:%d\n", (int)*theKode);
        }
        k++;                            /* ldr r3, [r4, #0x20]! */
    }
}


extern long  LevelSelect;               /* pointer slot -> 0x000ff7f8 */
extern long *requestedLevel;            /* pointer slot -> 0x0010deb0 */

extern long GameMode;                   /* 0x0014faa4 */

void sendLevelPacket(void);
int  isParent(void);


/* ---------------------------------------------------- preprocessPreloadKode
 *
 * armv7 0x000a030c, 308 bytes.  **Complete.**
 *
 * Turns an entered kode into a level choice. A `tbb` on `theKode - 3` covers
 * ten values and each one writes a different level index into `_LevelSelect`,
 * then sends it to the other player:
 *
 *      theKode   3   4   5   6   7   8   9  10  11  12
 *      level     1  10   0   4   8   9   2   3   5  11
 *
 * **The mapping is not the identity and not monotonic**, and there is no
 * formula behind it -- it is a hand-written table and the only way to have it
 * is to copy it. Kodes 0, 1, 2 and anything above 12 fall through and select
 * nothing.
 *
 * The tbb table itself is `{13, 21, 29, 37, 45, 53, 61, 69, 77, 85}`, evenly
 * spaced by 8 halfwords, so the ten cases are sixteen bytes apart and in
 * address order here -- unlike `initArguments`, where reading the table as
 * ordered was wrong.
 *
 * **Every case calls `sendLevelPacket()`**, including in single player where
 * there is nobody to send to.
 *
 * ### The multiplayer default
 *
 * Afterwards, if `GameMode == 1` and `isParent()`, the parent overwrites
 * whatever the kode chose with `_requestedLevel` and sends that instead,
 * printing
 *
 *      MP:KODE NOT ENTERED
 *
 * So in a two-player match the host has the last word on the arena regardless
 * of what the guest typed, and the message says exactly that -- it is not an
 * error, it is the host asserting its own choice. Anything replacing the
 * transport has to keep that ordering: kode first, host override second.
 */
void preprocessPreloadKode(void)
{
    switch (*theKode) {
    case  3: LevelSelect =  1; sendLevelPacket(); break;
    case  4: LevelSelect = 10; sendLevelPacket(); break;
    case  5: LevelSelect =  0; sendLevelPacket(); break;
    case  6: LevelSelect =  4; sendLevelPacket(); break;
    case  7: LevelSelect =  8; sendLevelPacket(); break;
    case  8: LevelSelect =  9; sendLevelPacket(); break;
    case  9: LevelSelect =  2; sendLevelPacket(); break;
    case 10: LevelSelect =  3; sendLevelPacket(); break;
    case 11: LevelSelect =  5; sendLevelPacket(); break;
    case 12: LevelSelect = 11; sendLevelPacket(); break;
    default: break;                     /* no level selected */
    }

    if (GameMode != 1)
        return;

    if (!isParent())
        return;

    LevelSelect = *requestedLevel;     /* the host has the last word */
    sendLevelPacket();
    puts("MP:KODE NOT ENTERED");
}


extern float  FE_HeightScale;           /* pointer slot -> 0x000ff9bc */
extern float  FE_WidthScale;            /* pointer slot -> 0x000ff9b8 */
extern void **GameFontSlot;             /* pointer slot -> 0x001abb98 */
extern long  *GamePausedPtr;            /* pointer slot */
extern int   *limeScreenWidthP;         /* pointer slot */
extern int   *limeScreenHeightP;        /* pointer slot */

const char *GameText(long id);
void limeFillRect(float x, float y, float w, float h,
                  float r, float g, float b, float a);
void limeDrawFONT(void *font, const char *text, float x, float y,
                  long align, float scale, const float *colour);
double sin(double x);
double cos(double x);
double fabs(double x);


/* ---------------------------------------------------------- achievementsDraw
 *
 * armv7 0x000a0928, 684 bytes.  **Complete.**
 *
 * Draws the unlock banner for any achievement in state 1, and retires it when
 * it has finished sliding.
 *
 * ### The banner slides on a sine and eases at the wrong end
 *
 *      y = limeScreenHeight + sin(timer) * -32 * FE_HeightScale
 *
 * At `timer == 0` that is exactly the bottom of the screen -- off it. At PI/2
 * the banner is 32 scaled units up, fully visible. At PI it is back off the
 * bottom. One half-period of sine is the whole animation.
 *
 * The timer advances by
 *
 *      timer += fabs(cos(timer) * 0.05) + 0.01
 *
 * and `|cos|` is **largest at the ends and smallest in the middle**, so the
 * banner moves fast on the way in, **lingers at the top**, and moves fast on
 * the way out. That is the opposite of the ease-in-out a port would reach for
 * by default, and it is exactly what a notification wants. The `+ 0.01` floor
 * is what stops it stalling completely at PI/2, where `cos` is zero.
 *
 * Both the sine and the step are computed in **double**; only the final Y is
 * narrowed.
 *
 * **The timer does not advance while `GamePaused` is set** -- the banner
 * freezes mid-slide rather than running out behind the pause menu.
 *
 * ### The three tracker states, from the reading side
 *
 * `achievementsUnlock` writes 1, 2 or 4. Here is what they mean:
 *
 *      1   sliding -- draw the banner and advance the timer
 *      2   done -- counted, never drawn again
 *      4   unlocked while the achievements screen was open; on the first frame
 *          the screen is NOT open it becomes 1 and starts sliding
 *
 * So 4 is a deferred 1, and this function is what defers it. Reaching PI sets
 * the entry to 2 and zeroes its timer.
 *
 * ### Achievement 17 is awarded for the other nineteen
 *
 *      if (finished == 19 && achievementTracker[17] == 0)
 *          achievementsUnlock(17);
 *
 * `finished` counts entries in state 2 across the twenty slots this loop walks.
 * So slot 17 is the meta-achievement, and it is granted from inside the draw
 * loop rather than from wherever the other nineteen are earned.
 *
 * Two lines of text per banner: `GameText(0x65)` -- a fixed heading -- in grey
 * at 0.9 scale, and `GameText(descr[i].id)` in white at 0.65, both offset from
 * the sliding Y.
 */
void achievementsDraw(void)
{
    float white[4], grey[4];
    long finished = 0;
    long i;

    white[0] = white[1] = white[2] = white[3] = 1.0f;           /* C.13 */
    grey[0]  = grey[1]  = grey[2]  = grey[3]  = 0.9f;           /* C.14 */

    for (i = 0; i < 20; i++) {
        long v = achievementTracker[i];
        float y;

        if (v == 4) {
            if (areAchievementsViewing()) {
                v = achievementTracker[i];      /* re-read, still 4 */
                if (v != 1)
                    goto next;
            } else {
                achievementTracker[i] = 1;      /* the deferred start */
                achievementsDescr[i].timer.w = 0;
                v = 1;
            }
        }

        if (v != 1) {
            if (v == 2)
                finished++;
            goto next;
        }

        y = (float)((double)*limeScreenHeightP
                    + sin((double)achievementsDescr[i].timer.f)
                      * -32.0 * (double)FE_HeightScale);

        limeFillRect(0.0f, y, (float)*limeScreenWidthP,
                     32.0f * FE_HeightScale, 0.0f, 0.0f, 0.0f, 0.5f);

        /* Both lines at x = 20 * FE_WidthScale, the heading at y + 4 and the
         * name at y + 14 (0xa0a18-0xa0a8c: 4.0 and 14.0 scale the HEIGHT,
         * 20.0 the width). The constants were swapped -- x 4 and 14, both
         * lines at y + 20 -- and the two lines drew on top of each other. */
        limeDrawFONT(GameFontSlot, GameText(0x65),
                     20.0f * FE_WidthScale, y + 4.0f * FE_HeightScale,
                     0, 0.65f * FE_WidthScale, grey);

        limeDrawFONT(GameFontSlot, GameText(achievementsDescr[i].id),
                     20.0f * FE_WidthScale, y + 14.0f * FE_HeightScale,
                     0, 0.9f * FE_WidthScale, white);

        if (*GamePausedPtr == 0) {
            double t = achievementsDescr[i].timer.f;
            achievementsDescr[i].timer.f =
                (float)(t + fabs(cos(t) * 0.05) + 0.01);
        }

        if (achievementsDescr[i].timer.f >= 3.1415927f) {
            achievementTracker[i] = 2;
            achievementsDescr[i].timer.w = 0;
            finished++;
        }

    next:
        ;
    }

    if (finished == 19 && achievementTracker[17] == 0)
        achievementsUnlock(17);         /* the meta-achievement */
}


extern void *KodesTexture;             /* pointer slot -> 0x00183f1c */
/* The font itself: _GameFont is 98,396 bytes of storage at 0x001abb98,
 * reached through a slot whose content is its address. Declared as a
 * pointer, its first word was passed as the font. */
extern char   GameFont[];               /* 0x001abb98 */
extern float *fontcolP;                 /* pointer slot -> 0x0014f9f0 */
extern float FE_WidthScale;             /* 0x000ff9b8 */
extern float FE_HeightScale;            /* 0x000ff9bc */
typedef struct TEXTURE TEXTURE;

void *limeLoadTexture(const char *name, long a, long b);
void  limeDeleteTexture(TEXTURE *tex);
void  limeDrawSprite(TEXTURE *tex, float x, float y, float w, float h,
                     float u0, float v0, float u1, float v1, long *colour);


/* ---------------------------------------------------------------- drawKodeTip
 *
 * armv7 0x000a0530, 804 bytes.  **Complete.**
 *
 * Shows one kode's button sequence: two lines of text and **six glyphs**, drawn
 * from `KOMBAT_KODES_TPAGE.PNG`.
 *
 * The six values come straight out of `_kodes[index]` -- words 0 through 5 of
 * its 32-byte entry, copied into two stack arrays of three. **Those are exactly
 * the six words `checkIfKode` compares against `KodeSelector[0..2]` and
 * `[7..9]`**, so this function is the display side of the same six-symbol
 * sequence: three per row, two rows.
 *
 * ### The glyph atlas is 5 wide with 48-pixel cells
 *
 *      column = value % 5
 *      row    = value / 5
 *      u0 = column * 48 * (1/256)      u1 = 0.1875   (48/256)
 *      v0 = row    * 48 * (1/128)      v1 = 0.375    (48/128)
 *
 * -- a 256x128 texture in a 5x2 grid of 48x48 cells with padding. The `% 5` is
 * a reciprocal multiply with magic `0x66666667` and a shift of 1, worked out
 * numerically rather than assumed, and the `* 48` is spelled `(n << 6) -
 * (n << 4)` both times.
 *
 * So a kode symbol is a **number 0..9** and the atlas holds ten glyphs. That is
 * the alphabet of the kode screen.
 *
 * The glyphs are **one row of six**, 32 wide and 32 apart, at y = 160: the
 * loop runs x = 0x86, 0xa6, 0xc6 and draws two glyphs each pass, seq[i] at x
 * and seq[i + 3] at x + 0x74 (`add.w r3, r4, #0x74` feeds the X register of
 * the second limeDrawSprite; both calls take the same 160 * FE_HeightScale
 * as Y). So player one's three sit at 134..230 and player two's at 250..346,
 * a 20-pixel gap between them, centred on 240 like the text. The earlier
 * transcription put the +0x74 on Y, and the second three drew a row lower,
 * over the loading screen's own text.
 *
 * ### Three lines of text
 *
 *      GameText(0x363)            at (240, 116)
 *      GameText(0x364)            at (240, 132)
 *      GameText(kodes[i].text)    at (240, 202)   after the glyphs
 *
 * all centred (align 1), scaled by FE_WidthScale / FE_HeightScale. The third
 * is word 6 of the kode's entry -- what the kode does -- and was missing.
 *
 * ### The texture is loaded and deleted every call
 *
 *      KodesTexture = limeLoadTexture("KOMBAT_KODES_TPAGE.PNG", 0, 0);
 *      ...
 *      limeDeleteTexture(KodesTexture);        ; the last call
 *
 * So nothing leaks; the port's limeDeleteTexture keeps its cache anyway.
 */
void drawKodeTip(long index)
{
    float white[4];
    long seq[6];
    long i;

    white[0] = white[1] = white[2] = white[3] = 1.0f;   /* C.25 */

    seq[0] = kodes[index].seq[0];
    seq[1] = kodes[index].seq[1];
    seq[2] = kodes[index].seq[2];
    seq[3] = kodes[index].seq[3];
    seq[4] = kodes[index].seq[4];
    seq[5] = kodes[index].seq[5];

    limeDrawFONT(GameFont, GameText(0x363),
                 240.0f * FE_WidthScale, 116.0f * FE_HeightScale,
                 1, FE_WidthScale, fontcolP);

    limeDrawFONT(GameFont, GameText(0x364),
                 240.0f * FE_WidthScale, 132.0f * FE_HeightScale,
                 1, FE_WidthScale, fontcolP);

    /* loaded here and deleted at the end, every call -- see above */
    KodesTexture = limeLoadTexture("KOMBAT_KODES_TPAGE.PNG", 0, 0);

    for (i = 0; i < 6; i++) {
        long v   = seq[i];
        long col = v % 5;               /* magic 0x66666667, shift 1 */
        long row = v / 5;
        float x  = (float)(0x86 + (i % 3) * 0x20 + (i < 3 ? 0 : 0x74));

        limeDrawSprite((TEXTURE *)KodesTexture,
                       x * FE_WidthScale, 160.0f * FE_HeightScale,
                       32.0f * FE_WidthScale, 32.0f * FE_HeightScale,
                       (float)((double)(col * 48) * 0.00390625),   /* 1/256 */
                       (float)((double)(row * 48) * 0.0078125),    /* 1/128 */
                       0.1875f, 0.375f, (long *)white);
    }

    limeDrawFONT(GameFont, GameText(kodes[index].text),
                 240.0f * FE_WidthScale, 202.0f * FE_HeightScale,
                 1, FE_WidthScale, fontcolP);

    limeDeleteTexture((TEXTURE *)KodesTexture);
}
