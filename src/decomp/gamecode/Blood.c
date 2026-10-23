/*
 * Blood.c — src/gamecode/Blood.cpp
 *
 * Blood and the game-event pool that drives it. Hand-written from the
 * disassembly of the armv7 slice.
 */

#include <stdint.h>
#include <stdio.h>     /* port: the csound diagnostic */

/* `_GameEvents` — 0x00370e18.
 *
 * The size comes out of InitGameEvents itself rather than from a header: the
 * loop steps the byte cursor by 0x64 and stops at 0x640, which is sixteen
 * entries of a hundred bytes. Only the first word of each is cleared, so the
 * rest of an entry is left as it was and the first word is what marks a slot
 * free. What the other 96 bytes hold is not established here. */
#define GAME_EVENT_SLOTS   16
#define GAME_EVENT_STRIDE  0x64

typedef struct GAMEEVENT {
    int32_t active;                     /* 0x00  the only field this file touches */
    uint8_t rest[GAME_EVENT_STRIDE - 4];
} GAMEEVENT;

extern GAMEEVENT GameEvents[GAME_EVENT_SLOTS];      /* 0x00370e18 */


/* ------------------------------------------------------------ InitGameEvents
 *
 * armv7 0x00072f30, 32 bytes.  **Complete.**
 *
 *      str  r2, [r3]               <- slot 0, before the loop
 *      loop:
 *      str  r2, [r1, r3]           <- r1 = 0x64, 0xc8, ... base-relative
 *      adds r1, #0x64
 *      cmp  r1, #0x640
 *      bne  loop
 *
 * Slot zero is written outside the loop and the loop starts at 0x64, so all
 * sixteen are cleared exactly once. The compiler peeled the first iteration
 * because the cursor is used as the loop counter and starting it at zero would
 * have cost a separate compare.
 *
 * **It clears one word per entry, not the entry.** A port that memsets the
 * whole array is doing more than the original and would wipe fields the game
 * expects to survive an init.
 */
void InitGameEvents(void)
{
    int i;

    for (i = 0; i < GAME_EVENT_SLOTS; i++)
        GameEvents[i].active = 0;
}


void AddParticles(long type, float x, float y, float z, float speed, long arg);


/* ------------------------------------------------- DoBlood / Green / Black
 *
 * armv7 0x00073078, 0x0007302c and 0x00072fe0, 76 bytes each.  **Complete.**
 *
 * Three identical functions that differ in ONE constant: the particle type,
 * 2 for red, 4 for green and 6 for black. Everything else -- the 60.0f speed,
 * the pass-through of x, y, z and the caller's count, and the loop -- is the
 * same in all three, and the compiler emitted three copies rather than sharing
 * a body.
 *
 * **Each of them calls AddParticles EIGHTY times.** One call before the loop,
 * then the loop runs its counter from 1 up to 0x50 emitting one per pass, and
 * every call gets the same arguments. So `count` is not how many particles come
 * out; it is passed to each of eighty bursts.
 *
 * That is worth stating because it is the kind of thing a port "optimises" into
 * a single call with a multiplied count, and the two are not the same: whatever
 * AddParticles does with randomness happens eighty times here.
 */
static void DoBloodOfType(long type, float x, float y, float z, long count)
{
    long i;

    AddParticles(type, x, y, z, 60.0f, count);
    for (i = 1; i != 0x50; i++)
        AddParticles(type, x, y, z, 60.0f, count);
}

void DoBlood(float x, float y, float z, long count)
{
    DoBloodOfType(2, x, y, z, count);
}

void DoGreenBlood(float x, float y, float z, long count)
{
    DoBloodOfType(4, x, y, z, count);
}

void DoBlackBlood(float x, float y, float z, long count)
{
    DoBloodOfType(6, x, y, z, count);
}


/* ---------------------------------------------------------------- GetNewEvent
 *
 * armv7 0x00072eec, 68 bytes.  **Complete.**
 *
 * Finds the first free slot -- first word zero -- in the same sixteen-entry
 * pool InitGameEvents clears, and returns NULL when they are all taken. The
 * walk ends at base + 0x5dc, which is entry 15 at stride 0x64.
 *
 * On a hit it writes the type at +4, marks the slot live with 1 at +0, and
 * clears **+0x24 through +0x60** -- sixteen words -- leaving +8 through +0x20
 * untouched. So a reused slot inherits whatever those held.
 *
 * That asymmetry is transcribed as written. Clearing the whole entry would be
 * tidier and would erase fields the game may be relying on surviving.
 */
GAMEEVENT *GetNewEvent(long type)
{
    int i, k;

    for (i = 0; i < GAME_EVENT_SLOTS; i++) {
        long *e = (long *)&GameEvents[i];

        if (e[0] != 0)
            continue;

        e[1] = type;                    /* +0x04 */
        e[0] = 1;                       /* +0x00, live */
        for (k = 0x24; k <= 0x60; k += 4)
            e[k / 4] = 0;
        return &GameEvents[i];
    }
    return 0;
}


extern int Settings[10];                /* 0x00100e34 */


extern long  MercyMessage;              /* pointer slot -> 0x0014fb40 */
extern float MusicVol[];                /* 0x000ff830 -- an ARRAY, same correction:
                                         * `add r1,pc` puts the array address in r1
                                         * and the volume is `[r1 + idx*4]`. */
long limeRand(void);
void limeStopTune(void);
void limePlayTune(const char *file, long vol, long arg);


/* --------------------------------------------------------- PlayFatalityVoice
 *
 * armv7 0x00072f58, 116 bytes.  **Complete.**
 *
 *      m = MercyMessage
 *      if (m == 0 && Settings[2] == 0) return       <- nothing to do
 *      limeStopTune()
 *      if (limeRand() & 1) limePlayTune("Fatal1.mp3", vol, m)
 *      else                limePlayTune("Fatal2.mp3", vol, m)
 *
 *      vol = (long)MusicVol[Settings[2]]
 *
 * **The volume is a float TABLE indexed by the music setting**, converted to an
 * int with `vcvt.s32.f32` at the call. Settings[2] is the same slot
 * ResetSettingsData writes twice -- 3 by default, 0 when the phone already had
 * music playing -- so index 0 of MusicVol is the muted level and the guard
 * above means the voice is skipped entirely in that case unless a mercy message
 * is pending.
 *
 * The coin flip is `limeRand() & 1`, one bit, so the two clips are equally
 * likely. `_MercyMessage` is passed through to limePlayTune untouched.
 */
void PlayFatalityVoice(void)
{
    long mercy = MercyMessage;
    long vol;

    if (mercy == 0 && Settings[2] == 0)
        return;

    limeStopTune();
    vol = (long)MusicVol[Settings[2]];

    if (limeRand() & 1)
        limePlayTune("Fatal1.mp3", vol, mercy);
    else
        limePlayTune("Fatal2.mp3", vol, mercy);
}


extern int   Settings[10];              /* 0x00100e34 */
extern float MusicVol[];                /* 0x000ff830 -- an ARRAY, same correction:
                                         * `add r1,pc` puts the array address in r1
                                         * and the volume is `[r1 + idx*4]`. */
extern long   PLAYER1MODEL;             /* pointer slot -> 0x0014e1b4 */
extern long  PLAYER2MODEL;              /* 0x0014e1b8, a plain long */

typedef struct Mk3Obj_t Mk3Obj_t;

void ArcadePosTo3dPos(Mk3Obj_t *obj, float *out, const signed char *who);
void CalcShakeOffset(long magnitude);
long get_tsound(long id);
void limePlaySound(long id, float vol, float pan, long flags);
void limeSetVibrate(void);


/* ------------------------------------------------------------- RunGameEvents
 *
 * armv7 0x000730c4, 484 bytes.  **Complete.**
 *
 * Runs the sixteen-slot event pool once a frame. A slot with `active == 0` or a
 * negative type is skipped; a type above 12 is skipped unless it is exactly 13.
 *
 * ### Types 0..12: blood, and which colour depends on the MODEL
 *
 * The position comes from `ArcadePosTo3dPos(obj, &event[0x0c], NULL)` and the
 * direction from `obj->flags & 0x10` -- **+1 or -1**, the same flag bit that
 * chooses the sign of the X offset in that conversion.
 *
 * The colour is chosen from the fighter's model id, read through
 * `PLAYER1MODEL` or `PLAYER2MODEL` depending on a signed byte at `obj + 0xd`:
 *
 *      model 7, 8 or 14   ->  DoBlackBlood
 *      model 11 or 19     ->  DoGreenBlood
 *      anything else      ->  DoBlood
 *
 * Those five ids are the robots and the reptilian characters, and they are
 * hardcoded here rather than carried in the player definition. The `7` and `8`
 * arrive as a range test (`(unsigned)(model - 7) <= 1`), which is the compiler
 * folding two comparisons, not a range in the data.
 *
 * All three are called with `z + 1.5f` -- the spray starts above the hit.
 *
 * **`Settings[0]` gates the blood entirely.** With it clear the event still
 * ages and still expires; nothing is drawn. So a port must not skip the
 * decrement along with the drawing.
 *
 * The hit sound is `get_tsound(0x22)` at `MusicVol[Settings[3]] / 100.0f`, and
 * only when `Settings[3]` is non-zero -- the same volume-index-into-a-float-
 * table that `PlayFatalityVoice` uses.
 *
 * ### Type 7 nudges the position first
 *
 *      x -= 0.765625f
 *      z -= 1.59375f
 *
 * then falls into the ordinary blood path. Two odd literals, both exact in
 * binary (49/64 and 51/32), so they were typed as decimals that happen to be
 * representable rather than tuned by ear.
 *
 * ### Type 13 is the screen shake
 *
 * A three-frame oscillator:
 *
 *      if (++event[0x2c] == 3) {
 *          event[0x2c] = 0;
 *          event[0x28]--;              <- shakes remaining
 *          event[0x24] = -event[0x24]; <- flip the offset
 *      }
 *      if (shakes remaining == 0) { CalcShakeOffset(0); event->active = 0; }
 *      else { CalcShakeOffset(event[0x24]); if (Settings[1]) limeSetVibrate(); }
 *
 * **The magnitude is negated, not recomputed**, so the shake alternates around
 * zero at exactly one third of the frame rate and ends by calling
 * `CalcShakeOffset(0)` -- it always leaves the camera centred rather than
 * wherever the last flip put it.
 *
 * `Settings[1]` is the vibration toggle, and it is checked per shake frame
 * rather than once.
 *
 * ### Expiry
 *
 * Every non-shake type decrements `event[0x24]` and frees the slot when it goes
 * **below** zero -- so a lifetime of 0 still runs one more frame.
 */
void RunGameEvents(void)
{
    GAMEEVENT *e = GameEvents;
    long *w;

    for (;;) {
        w = (long *)e;

        if (w[0] == 0)
            goto next;

        if (w[1] < 0)
            goto next;

        if (w[1] > 12) {
            if (w[1] != 13)
                goto next;

            /* the screen shake */
            w[0x2c / 4]++;
            if (w[0x2c / 4] == 3) {
                w[0x2c / 4] = 0;
                w[0x28 / 4]--;
                w[0x24 / 4] = -w[0x24 / 4];
            }

            if (w[0x28 / 4] == 0) {
                CalcShakeOffset(0);
                w[0] = 0;               /* always ends centred */
            } else {
                CalcShakeOffset(w[0x24 / 4]);
                if (Settings[1] != 0)
                    limeSetVibrate();
            }
            goto next;
        }

        {
            Mk3Obj_t *obj = (Mk3Obj_t *)(uintptr_t)(unsigned long)w[2];
            float *pos = (float *)&w[0x0c / 4];
            long dir = (((const unsigned short *)obj)[5] & 0x10) ? 1 : -1;
            long model;

            ArcadePosTo3dPos(obj, pos, 0);

            if (w[1] == 7) {
                pos[0] -= 0.765625f;    /* 49/64, exact */
                pos[2] -= 1.59375f;     /* 51/32, exact */
            }

            if (Settings[0] == 0)
                goto expire;            /* the event still ages */

            model = (((const signed char *)obj)[0xd] == 0)
                    ? PLAYER1MODEL : PLAYER2MODEL;

            if (model == 14 || (unsigned long)(model - 7) <= 1)
                DoBlackBlood(pos[0], pos[1], pos[2] + 1.5f, dir);
            else if (model == 19 || model == 11)
                DoGreenBlood(pos[0], pos[1], pos[2] + 1.5f, dir);
            else
                DoBlood(pos[0], pos[1], pos[2] + 1.5f, dir);

            if (Settings[3] != 0) {
                long s = get_tsound(0x22);
                if (s != -1)
                    limePlaySound(s, MusicVol[Settings[3]] / 100.0f, 1.0f, 0);
            }
        }

    expire:
        w[0x24 / 4]--;
        if (w[0x24 / 4] < 0)
            w[0] = 0;                   /* below zero, not at it */

    next:
        if (e == &GameEvents[GAME_EVENT_SLOTS - 1])
            return;
        e++;
    }
}


/* ------------------------------------------------------------ AddNewGameEvents
 *
 * armv7 0x000732a8, 6,644 bytes.  **Complete.**
 *
 * **The seam between the ported arcade logic and the iOS presentation layer.**
 * The MK3 fight engine does not draw, play sound, or know what a fatality is.
 * It appends eight-byte records to `MKEventQueue`, and once a frame this
 * function drains that queue and turns each record into blood, an FBX effect, a
 * sound, a banner, an achievement, a stat, or a change of round state.
 *
 * A port that wants to change how the game *looks* changes this function. A
 * port that wants to change how it *plays* does not touch it.
 *
 * Note there are two unrelated event queues in this file. `GameEvents` (stride
 * 0x64, above) is this file's own particle and screen-shake pool, run by
 * `RunGameEvents`. `MKEventQueue` is the engine's, and is what this function
 * reads -- though type 0 and the shake are how the one feeds the other.
 *
 * ### The record
 *
 *      +0  int8   type       0..4, the class
 *      +1  int8   player     which side the event is about
 *      +2  int8   subtype    the event proper
 *      +3  int8   (unused)
 *      +4  int32  param
 *
 * `MKEventQueue[0]` is the count and the records start at +4. The loop walks
 * exactly `count` of them and never looks at a used-flag, so the engine hands
 * over a fresh, packed array each frame.
 *
 * Eighty-eight switch arms across four tables; **thirty-nine do nothing**. The
 * 69-entry table for type 4 is an inline `.word` array the compiler jumps into
 * with `mov pc, r0`, so the dead subtypes are visible as explicit jumps back to
 * the loop tail rather than as gaps.
 *
 * ### Type 0 and the shake write GAMEEVENTs, which names three of its fields
 *
 * Type 0 calls `GetNewEvent`, stores the fighter's object at **+8** and drops
 * the 3D position at **+0xc**; `RunGameEvents` reads exactly those two. The
 * shake (type 1's default arm) takes event id 13 and writes **+0x24**, **+0x28**
 * and **+0x2c** -- magnitude, count and tick -- which is again exactly what
 * `RunGameEvents`'s `w[1] == 13` arm reads. Two functions written from opposite
 * ends of the same struct, agreeing field for field.
 *
 * ### The finishers, named by the binary
 *
 * Four of the five log an EA analytics event whose last argument is a plain
 * ASCII literal, so these names are **read, not inferred**:
 *
 *      22  Fatality     "Fatality"      (no music change)
 *      28  Animality    "Animality"     Animality.mp3
 *      29  Mercy        not logged      Mercy.mp3
 *      42  Friendship   "Friendship"    Friendship.mp3
 *      43  Babality     "Babality"      Babality.mp3
 *
 * Every logging call is `EASDK_LogEventEnumEnumString(0x3f7, 15,
 * DestinyNames[Destiny], 15, <name>)` -- the difficulty and the finisher. The
 * log fires only when the loser is actually at zero health; in `GameMode == 0`
 * it also unlocks an achievement and bumps a stat, which is why those two only
 * ever count arcade play; and in `GameMode == 1` the stat is bumped for
 * whichever side the local machine is, decided by `isParent()`.
 *
 * ### Stats and achievements this function owns
 *
 *      Stats[5]    fatalities        achievement 0xc on the first
 *      Stats[7]    friendships       achievement 0xb on the first
 *      Stats[8]    animalities       achievement 0xa on the first
 *      --          babalities        achievement 9 on the first
 *      --          the blast         achievement 4, outside modes 1 and 6
 *      --          a mercy then a finisher   achievement 5
 *
 * ### Two character indices appear as bare numbers
 *
 * `0x19` (Shao Kahn) gates the whole Shao-Kahn death sequence, and `0x15`
 * (classic Sub-Zero) **suppresses the friendship** on either side. Both match
 * [ROSTER.md](../../docs/ROSTER.md).
 *
 * ### `groundoffsets`
 *
 * Subtype 54 computes `blast_player_height = 0xf7 - groundoffsets[model]` -- a
 * per-character ground offset table at 0x001725bc subtracted from a fixed 247,
 * reading player 1's entry or player 2's depending on which side blasted. First
 * use of that table found, and what names it.
 *
 * ### The stage deaths pick their scene from the stage
 *
 * Subtype 58 is the stage death and it branches on `LevelSelect`:
 *
 *      4   Subway            TrainScene, then TrainDie1Scene or TrainDie2Scene
 *      11  Scorpion's Lair   SLDie1Scene or SLDie2Scene
 *
 * and nothing else. Those are exactly the two stages
 * [ROSTER.md](../../docs/ROSTER.md) found per-character deaths for, reached
 * from a different direction.
 *
 * ### Health arrives as a bar length
 *
 *      Health[p] = 100 * (param + 1) / 166
 *
 * the exact inverse of the `* 166 / 100` `GameCode.c` uses to draw the bar. The
 * run meter is `100 * param / 48`. Neither is a percentage on the wire.
 *
 * ### Two shared tails
 *
 * The sound tail (0x0007417e) is `limePlaySound(id, MusicVol[Settings[3]] /
 * 100.0f, 1.0f, 0)` and is skipped whole when `Settings[3]` is zero. The effect
 * spawn is *not* shared as tidily as it looks: every arm builds the matrix the
 * same way -- identity, `RotMatrixX(m, pi/2)`, `limeScaleMatrix(m, 1 /
 * WorldScaleAdjust)` -- and then they differ in what they put at `m[0x30]` and
 * `m[0x38]`, in whether a second scale follows, and in the 2.2 and 2.8 offsets
 * some of them add. Those differences are written out per arm below rather than
 * folded, because folding them is where a reading of this function would go
 * wrong.
 *
 * ### One dead store, and one uninitialised read that does not matter
 *
 * Subtype 13 stores `ev->player` back over itself, predicated on the combo
 * damage exceeding 100. It is a genuine no-op in the shipped code.
 *
 * Type 1 subtype 1 calls `SetCameraOverridePosFrom2d` with two stack words the
 * function never writes. That is safe only because the callee ignores both --
 * which `SetCameraOverridePosFrom2d` was already decompiled as doing, before
 * this call site was read.
 */

#define BLAST_GROUND     0xf7           /* what groundoffsets is subtracted from */
#define SHAO_KAHN        0x19
#define CLASSIC_SUBZERO  0x15
#define HALF_PI          1.5707964f

/* One record of the engine's queue. */
typedef struct MKEVENT {
    signed char type;
    signed char player;
    signed char subtype;
    signed char pad;
    int32_t     param;
} MKEVENT;

typedef struct MKEVENTQUEUE {
    int32_t count;
    MKEVENT event[1];
} MKEVENTQUEUE;

extern MKEVENTQUEUE *MKEventQueue;      /* pointer slot -> 0x0038cf80 */
extern float  m[];                      /* 0x00370dd8, the scratch matrix */
extern float  WorldScaleAdjust;         /* pointer slot -> 0x0014df9c */
extern float  ShadowOffset;             /* pointer slot -> 0x0014dfc8 */
extern float  Camera[];                 /* pointer slot -> 0x0014fa74 */
extern float  Player1Pos[];             /* pointer slot -> 0x00150564 */
extern float  Player2Pos[];             /* pointer slot -> 0x00150570 */
extern void **GameObjects;             /* 0x0014dfec, one per object */
extern long   FrameRemapTable[];          /* pointer slot -> 0x002003d4 */
extern char   Players[];                /* 0x001fa4d4 */
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
extern const PLAYERDEF PlayerDefs[];   /* 0x00170950, 26 entries */
extern long   groundoffsets[];          /* 0x001725bc */
extern long   LevelSelect;              /* pointer slot -> 0x000ff7f8 */
extern long   LockCamera;               /* pointer slot -> 0x00150e8c */
extern long   CamTrackToPlayer;         /* pointer slot -> 0x0014dfc4 */
extern long  *RoundParam;               /* pointer slot -> 0x0038ed04 */
extern long   RoundMessage;             /* pointer slot -> 0x0014e250 */
extern long   RoundHasEnded;            /* pointer slot -> 0x0014e248 */
extern long   FinishHimHer;             /* pointer slot -> 0x0014e24c */
extern long   IsInFinishing;            /* pointer slot -> 0x00150cbc */
extern long   DoingStageFatal;          /* pointer slot -> 0x0010dee0 */
extern long   DoingSKDeath;             /* pointer slot -> 0x0010deb8 */
extern long   opponentPerformedMercy;   /* pointer slot -> 0x0010dea4 */
extern long   GameMode;                 /* pointer slot -> 0x0014faa4 */
extern long   Character2;               /* pointer slot -> 0x000ff98c */
extern long   Destiny;                  /* pointer slot -> 0x0014e20c */
extern const char *DestinyNames[];        /* 0x00176760, 108 bytes */
extern const char *CharacterNames[];     /* pointer slot -> 0x0014fe54 */
extern long   Health[];                   /* pointer slot -> 0x0014fa64 */
extern long   RunBar[];                   /* pointer slot -> 0x0014fa6c */
extern long   SurvivalHealth;           /* pointer slot -> 0x000ff994 */
extern long   ClockTens;                /* pointer slot -> 0x0014fa50 */
extern long   ClockSingles;             /* pointer slot -> 0x0014fa54 */
extern long   DangerMessage[];            /* pointer slot -> 0x0014e23c */
extern long   FightMessage;             /* pointer slot -> 0x0014e258 */
/* Flags, not slots. Both are plain words in `__DATA,__data`, and a slot lives
 * in the 0x000f3xxx region and holds an address -- these are at 0x0014fb2c and
 * 0x0014fb30 and hold zero. What settles it is a read from somewhere else
 * entirely: `TrainingData` stores 0x0014fb30 as a VALUE in its last field, so
 * the training table points AT this variable, which it could not do if the
 * variable were itself the pointer. Spelled `long *`, `*FatalityMessage = 1`
 * wrote through a null pointer. GameCode.c reached the same two words under
 * invented names -- `FatalityMessageG`, `AnimalityMessageG` -- and got the
 * shape right there only because `= 0` looks the same either way. */
extern long  FatalityMessage;           /* 0x0014fb30 */
extern long  AnimalityMessage;          /* 0x0014fb2c */
extern long   BabalityMessage;          /* pointer slot -> 0x0014fb28 */
extern long   FriendshipMessage;        /* pointer slot -> 0x0014fb34 */
extern char   WinnerMessage[];            /* pointer slot -> 0x0014faa8 */
extern long   ComboNumber[];              /* pointer slot -> 0x0014e27c */
extern long   ComboDamage[];              /* pointer slot -> 0x0014e274 */
extern float  ComboSlider1[];             /* pointer slot -> 0x0014e28c */
extern float  ComboSlider2[];             /* pointer slot -> 0x0014e294 */
extern float  ComboTimer[];               /* pointer slot -> 0x0014e284 */
extern float  ScorpionFade;             /* pointer slot -> 0x0010df04 */
extern float  ScorpionFadeAdd;          /* pointer slot -> 0x0010df08 */
extern float  ScorpionFlash;            /* pointer slot -> 0x0010df0c */
extern float  lightsOn;                 /* pointer slot -> 0x0010decc */
extern float  SKDeathMessageOffset;     /* pointer slot -> 0x0010debc */
extern long   blast_state;              /* pointer slot -> 0x0014df94 */
extern long   blast_player_height;      /* pointer slot -> 0x0014df98 */
extern long   EnduranceChange;          /* pointer slot -> 0x0010df14 */
extern long   EnduranceCharacters[];      /* pointer slot -> 0x001ab014 */
extern long   Stats[];                    /* pointer slot -> 0x00183c84 */
extern void *BloodScene;               /* pointer slot -> 0x001aba58 */
extern void *PitDeathScene;            /* pointer slot -> 0x001aba50 */
extern void *SZEffectScene;            /* pointer slot -> 0x001aba48 */
extern void *SwatEffectScene;          /* pointer slot -> 0x001aba4c */
extern void *CyraxSelfDestructScene;   /* pointer slot -> 0x001aba54 */
extern void *SKEffectScene;            /* pointer slot -> 0x001aba5c */
extern void *XeroxScene;               /* pointer slot -> 0x001aba60 */
extern void *RocksScene;               /* pointer slot -> 0x001aba64 */
extern void *TrainScene;               /* pointer slot -> 0x001aba68 */
extern void *TrainDie1Scene;           /* pointer slot -> 0x001aba6c */
extern void *TrainDie2Scene;           /* pointer slot -> 0x001aba70 */
extern void *SLDie1Scene;              /* pointer slot -> 0x001aba78 */
extern void *SLDie2Scene;              /* pointer slot -> 0x001aba7c */

#define PLAYER_STRIDE     0x5f0
#define PLAYER_MATRIX     0x548         /* the fighter's world matrix */
#define M_X               (0x30 / 4)
#define M_Y               (0x34 / 4)
#define M_Z               (0x38 / 4)

GAMEEVENT *GetNewEvent(long type);
uint32_t  get_csound(long a, long b);
long  get_rsound(long id, long seed);
long  get_gsound(long group, long variant, long seed);
int  achievementsUnlock(int id);
void  achievementsIncreaseSubzeroXerox(void);
void  EASDK_LogEventEnumEnumString(long id, long a, const char *s1,
                                   long b, const char *s2);
void  SaveUnclaimedTreasure(long treasure);
void  DumpAltCostume(char *player);
void  LoadGameCharacterCheckCache(char *player, const PLAYERDEF *def,
                                  long *stats);
/* HUDANIM is `typedef int` in HudAnim.c, so int is that same type rather
 * than a second copy of the typedef here. */
void  HUDANIM_TriggerAnim(int anim);
void  StopCameraTracking(void);
void  SetCameraOverridePosFrom2d(float x, float y, float z);
void  limeMatrixLoadIdentity(float *mtx);
void  limeMatrixCopy(const float *src, float *dst);
void  limeScaleMatrix(float *mtx, float s);
void  RotMatrixX(float *mtx, float angle);
void  LIME_PlayFBXAtPos(float *mtx, long a, void *scene, long b);
void  LIME_TriggerEventsFromScene(void *scene, int frame, const float *mtx,
                                  long flags, long a, long b, long c, long d);
long  isParent(void);
long  isParentBasedOnSpeed(void);
const char *GameTextNoHeader(long id);
const char *UC(const char *s);
long  usprintf(char *dst, const char *fmt, ...);

/* The matrix every effect arm starts from. What goes in m[X] and m[Z] after
 * this is the arm's own business -- see the header. */
static void BeginEffectMatrix(void)
{
    limeMatrixLoadIdentity(m);
    RotMatrixX(m, HALF_PI);
    limeScaleMatrix(m, 1.0f / WorldScaleAdjust);
}

/* The sound tail. Every lookup in the function converges here, and every one is
 * gated on the sound-effects setting before the lookup even happens. */
static void PlaySoundId(long id)
{
    if (id != -1)
        limePlaySound(id, MusicVol[Settings[3]] / 100.0f, 1.0f, 0);
}

/* Four of the five finishers log the difficulty and their own name. */
static void LogFinisher(const char *name)
{
    EASDK_LogEventEnumEnumString(0x3f7, 15, DestinyNames[Destiny], 15, name);
}

/* ...and three of them swap the music for their own track. */
static void PlayFinisherTune(const char *file)
{
    if (Settings[2] != 0) {
        limeStopTune();
        limePlayTune(file, (long)MusicVol[Settings[2]], 0);
    }
}

/* The finisher stat, which counts the local player's side in a network game and
 * player 1's everywhere else. */
static void CountFinisherStat(long index)
{
    if (GameMode == 1) {
        if (isParent()  && Health[1] == 0) Stats[index]++;
        if (!isParent() && Health[0] == 0) Stats[index]++;
    }
}

void AddNewGameEvents(void)
{
    MKEVENTQUEUE *q = MKEventQueue;
    long i;
    long sceneSide = -1;                /* carried across the loop; only 27 reads it */

    if (q->count == 0)
        return;

    for (i = 0; i != q->count; i++) {
        MKEVENT *ev  = &q->event[i];
        long     p   = ev->player;
        long     arg = ev->param;
        float   *pos = p ? Player2Pos : Player1Pos;

        switch (ev->type) {

        /* ---- 0: hand a fighter's position to the particle pool ---------- */
        case 0:
            if ((unsigned long)ev->subtype > 12)
                break;
            {
                GAMEEVENT *ge  = GetNewEvent(ev->subtype);
                Mk3Obj_t  *obj = (Mk3Obj_t *)((char *)GameObjects + p * 16);

                *(Mk3Obj_t *P32 *)((char *)ge + 8)  = obj;
                ArcadePosTo3dPos(obj, (float *)((char *)ge + 0xc), 0);
                *(long *)((char *)ge + 0x24) = 0;
            }
            break;

        /* ---- 1: camera, and the screen shake ---------------------------- */
        case 1:
            switch (ev->subtype) {
            case 1:
                /* the second and third arguments are stack words this function
                 * never writes; the callee ignores both */
                SetCameraOverridePosFrom2d((float)arg, 0.0f, 0.0f);
                break;
            case 2:
                StopCameraTracking();
                break;
            case 3:
                LockCamera = 1;
                break;
            case 4:
                LockCamera = 0;
                break;
            default: {
                /* event 13 is the shake, and RunGameEvents reads these three */
                GAMEEVENT *ge = GetNewEvent(13);
                *(long *)((char *)ge + 0x24) = arg >> 16;           /* magnitude */
                *(long *)((char *)ge + 0x28) = (unsigned short)arg; /* count */
                *(long *)((char *)ge + 0x2c) = 0;                   /* tick */
                break;
            }
            }
            break;

        /* ---- 2: sound --------------------------------------------------- */
        case 2:
            switch (ev->subtype) {
            case 0:
                /* four of the ids double as the round announcement */
                if ((unsigned long)(arg - 0x11) <= 3)
                    RoundMessage = arg - 0x10;
                if (Settings[3])
                    PlaySoundId(get_tsound(arg));
                break;
            case 1:
                /* 0x7368a-0x73696: r0 = arg & 0xf (the sound kind, which
                 * group_table row), r1 = arg >> 4 (the character's voice
                 * group, which block of the row), r2 = limeRand() (which take
                 * in the block). Passed as (kind, rand, voice) the random
                 * number became the block index and read far past the row:
                 * voices went missing and stray sounds played instead. */
                if (Settings[3])
                    PlaySoundId(get_gsound(arg & 0xf, arg >> 4, limeRand()));
                break;
            case 2:
                if (Settings[3])
                    PlaySoundId(get_rsound(arg, limeRand()));
                break;
            case 3:
                /* Port, not in the binary: ochar_sound_tables has 26 rows,
                 * and two projectiles meeting (SCORPION vs KABAL, Mary)
                 * queued a character number past them -- the read crashed.
                 * Logged and skipped until the sender is found. */
                if ((unsigned long)(arg >> 8) >= 26) {
                    printf("csound: character %ld out of range "
                           "(event param 0x%lx), skipped\n",
                           (long)(arg >> 8), (unsigned long)arg);
                    break;
                }
                if (Settings[3])
                    PlaySoundId(get_csound(arg & 0xff, arg >> 8));
                break;
            case 4:
                if (arg == 0x36)
                    PlayFatalityVoice();
                break;
            default:
                break;
            }
            break;

        /* ---- 3: round and fighter state --------------------------------- */
        case 3:
            switch (ev->subtype) {
            case 0:
                Health[p] = (arg == 0) ? 0 : 100 * (arg + 1) / 166;
                if (GameMode == 4 && p == 0)
                    SurvivalHealth = Health[0];
                break;

            case 1:
                ClockTens    = arg >> 4;
                ClockSingles = arg & 0xf;
                break;

            case 3:
                if (arg == -1)
                    usprintf(WinnerMessage, UC("%s"), GameTextNoHeader(0x3ba));
                else
                    usprintf(WinnerMessage, UC("%s %s"),
                             UC(CharacterNames[arg]), GameTextNoHeader(0x39b));
                if (DoingSKDeath)
                    ScorpionFlash = 1.0f;
                break;

            case 4:
                RoundHasEnded = 1;
                break;

            case 5:
                RunBar[p] = 100 * arg / 48;
                break;

            case 6:
                ScorpionFlash = 1.0f;
                break;

            case 9:
                ScorpionFade    = 0.0f;
                ScorpionFadeAdd = 1.0f / 30.0f;
                break;

            case 10:
                /* the endurance swap: bleed the old opponent out, load the next */
                limeMatrixCopy((const float *)(Players + PLAYER_STRIDE
                                               + PLAYER_MATRIX), m);
                LIME_PlayFBXAtPos(m, 0, BloodScene, 1);

                DoBlood(Player2Pos[0], Player2Pos[1], Player2Pos[2], -1);
                DoBlood(Player2Pos[0], Player2Pos[1], Player2Pos[2], -1);
                DoBlood(Player2Pos[0], Player2Pos[1], Player2Pos[2], -1);
                DoBlood(Player2Pos[0], Player2Pos[1], Player2Pos[2], -1);
                DoBlood(Player2Pos[0], Player2Pos[1], Player2Pos[2], -1);
                DoBlood(Player2Pos[0], Player2Pos[1], Player2Pos[2], -1);

                PLAYER2MODEL = EnduranceCharacters[EnduranceChange];
                DumpAltCostume(Players + PLAYER_STRIDE);
                LoadGameCharacterCheckCache(Players + PLAYER_STRIDE,
                                            &PlayerDefs[PLAYER2MODEL], 0);
                (EnduranceChange)++;
                DangerMessage[1] = 0;
                break;

            case 11:
                lightsOn = (float)arg;
                break;

            case 13:
                if (GameMode == 0 && p == 0)
                    achievementsIncreaseSubzeroXerox();
                break;

            default:
                break;
            }
            break;

        /* ---- 4: effects, finishers, round flow -------------------------- */
        case 4:
            switch (ev->subtype) {

            case 1:
                CamTrackToPlayer = p;
                break;

            case 3:
                BeginEffectMatrix();
                limeScaleMatrix(m, 0.25f);          /* this one is scaled twice */
                m[M_Y] = 0.0f;
                m[M_X] = (float)(arg >> 16) / WorldScaleAdjust;
                m[M_Z] = -((float)(unsigned short)arg / WorldScaleAdjust) + 2.8f;
                LIME_PlayFBXAtPos(m, 0, SZEffectScene, 1);
                break;

            case 5: case 10: case 16:
                BeginEffectMatrix();
                m[M_Y] = 0.0f;
                m[M_X] = (float)(arg >> 16) / WorldScaleAdjust - 2.2f;
                m[M_Z] = -((float)(unsigned short)arg / WorldScaleAdjust) + 2.8f;
                LIME_PlayFBXAtPos(m, 0, SwatEffectScene, 1);
                break;

            case 6: case 67:
                ScorpionFlash = 1.0f;
                break;

            case 11:
                FightMessage = 1;
                HUDANIM_TriggerAnim(1);
                break;

            case 13:
                /* the combo belongs to whoever did NOT take the hit */
                ComboNumber[p ^ 1] = (unsigned char)arg;
                ComboDamage[p ^ 1] = arg >> 8;
                if (ComboDamage[p ^ 1] > 100)
                    ev->player = ev->player;        /* a genuine no-op; see header */
                ComboSlider1[p ^ 1] = 192.0f;
                ComboSlider2[p ^ 1] = 256.0f;
                ComboTimer[p ^ 1]   = 300.0f;
                break;

            case 17: case 18:
                FinishHimHer = (ev->subtype == 17) ? 1 : 2;
                if (GameMode != 1 && opponentPerformedMercy && Health[1] == 0)
                    achievementsUnlock(5);          /* mercy, then a finisher */
                if (Settings[3])
                    PlaySoundId(get_tsound(ev->subtype == 17 ? 0x15 : 0x16));
                /* Two copies in the binary: FinishHimHer = 1, tsound 0x15,
                 * TriggerAnim(2) -- FINISH HIM (0x73eec, 0x7488e, 0x73f2a) --
                 * and FinishHimHer = 2, tsound 0x16, TriggerAnim(3) -- FINISH
                 * HER (0x73cee, 0x7483e, 0x73d28). Merged into one with a
                 * constant 3, every finisher said HIM and showed HER. */
                HUDANIM_TriggerAnim(ev->subtype == 17 ? 2 : 3);
                IsInFinishing = 1;
                RoundParam[14] = 1;
                RoundParam[13] = 0;
                break;

            case 21:
                BeginEffectMatrix();
                m[M_Z] = ShadowOffset;
                m[M_Y] = 0.0f;
                m[M_X] = (float)(arg >> 16) / WorldScaleAdjust;
                LIME_PlayFBXAtPos(m, 0, CyraxSelfDestructScene, 1);
                break;

            case 22:
                if (Health[1] == 0) {
                    if (GameMode == 0) {
                        achievementsUnlock(0xc);
                        Stats[5]++;
                    }
                    LogFinisher("Fatality");
                }
                CountFinisherStat(5);
                FatalityMessage = 1;
                break;

            case 24:
                BeginEffectMatrix();
                m[M_Z] = ShadowOffset;
                m[M_Y] = 0.0f;
                m[M_X] = (float)(arg >> 16) / WorldScaleAdjust;
                LIME_PlayFBXAtPos(m, 0, PitDeathScene, 1);
                break;

            case 27:
                /* whichever fighter is in state 1 supplies the scene */
                if (*(long *)Players == 1)
                    sceneSide = 0;
                if (*(long *)(Players + PLAYER_STRIDE) == 1)
                    sceneSide = 1;
                else if (sceneSide == -1)
                    break;
                if (FrameRemapTable[0xd5a0 / 4] != 1)
                    break;
                {
                    char *pl = Players + sceneSide * PLAYER_STRIDE;
                    LIME_TriggerEventsFromScene(
                        *(void *P32 *)(*(char *P32 *)(pl + 4) + 0x10),
                        FrameRemapTable[0xd5a4 / 4],
                        (const float *)(pl + PLAYER_MATRIX),
                        *(long *)(pl + 0x540),
                        -1, 1, 0, 0);
                }
                break;

            case 28:
                if (Health[1] == 0) {
                    if (GameMode == 0) {
                        achievementsUnlock(0xa);
                        Stats[8]++;
                    }
                    LogFinisher("Animality");
                }
                CountFinisherStat(8);
                AnimalityMessage = 1;
                PlayFinisherTune("Animality.mp3");
                break;

            case 29:
                MercyMessage  = 1;
                IsInFinishing = 0;
                PlayFinisherTune("Mercy.mp3");
                break;

            case 30:
                BeginEffectMatrix();
                m[M_Y] = 0.0f;
                m[M_X] = (float)(arg >> 16) / WorldScaleAdjust;
                m[M_Z] = -((float)(unsigned short)arg / WorldScaleAdjust) + 2.8f;
                LIME_PlayFBXAtPos(m, 0, XeroxScene, 1);
                break;

            case 32:
                /* six sprays at three heights, each height twice */
                DoBlood(pos[0], pos[1], pos[2] + 1.5f, -1);
                DoBlood(pos[0], pos[1], pos[2] + 1.0f, -1);
                DoBlood(pos[0], pos[1], pos[2] + 0.5f, -1);
                DoBlood(pos[0], pos[1], pos[2] + 1.5f, -1);
                DoBlood(pos[0], pos[1], pos[2] + 1.0f, -1);
                DoBlood(pos[0], pos[1], pos[2] + 0.5f, -1);
                break;

            case 34:
                BeginEffectMatrix();
                m[M_Z] = ShadowOffset;
                m[M_Y] = 0.0f;
                m[M_X] = (float)(arg >> 16) / WorldScaleAdjust;
                LIME_PlayFBXAtPos(m, 0, CyraxSelfDestructScene, 1);
                PlaySoundId(get_tsound(1));
                break;

            case 35:
                PlaySoundId(get_tsound(0x25));
                break;

            case 39:
                DangerMessage[arg] = 1;
                if (Settings[3])
                    PlaySoundId(get_tsound(0xb0));
                break;

            case 42:
                /* classic Sub-Zero has no friendship, on either side */
                if (Health[1] == 0 && PLAYER1MODEL == CLASSIC_SUBZERO)
                    break;
                if (Health[0] == 0 && PLAYER2MODEL == CLASSIC_SUBZERO)
                    break;
                if (Health[1] == 0) {
                    if (GameMode == 0) {
                        achievementsUnlock(0xb);
                        Stats[7]++;
                    }
                    LogFinisher("Friendship");
                }
                CountFinisherStat(7);
                FriendshipMessage = 1;
                PlayFinisherTune("Friendship.mp3");
                break;

            case 43:
                if (Health[1] == 0) {
                    if (GameMode == 0)
                        achievementsUnlock(9);
                    LogFinisher("Babality");
                }
                BabalityMessage = 1;
                PlayFinisherTune("Babality.mp3");
                break;

            case 54:
                blast_state = p + 1;
                if (blast_state == 1) {
                    blast_player_height = BLAST_GROUND
                                           - groundoffsets[PLAYER1MODEL];
                } else {
                    blast_player_height = BLAST_GROUND
                                           - groundoffsets[PLAYER2MODEL];
                    if (GameMode != 1 && GameMode != 6)
                        achievementsUnlock(4);
                }
                break;

            case 56:
                blast_state = 3;
                break;

            case 57:
                BeginEffectMatrix();
                m[M_X] = pos[0];
                m[M_Y] = 0.0f;
                m[M_Z] = ShadowOffset;
                LIME_PlayFBXAtPos(m, 0, RocksScene, 1);
                break;

            case 58:
                /* the stage death, and only two stages have one */
                if (LevelSelect == 4) {
                    limeMatrixLoadIdentity(m);
                    RotMatrixX(m, HALF_PI);
                    limeScaleMatrix(m, (float)(2.2 / (double)WorldScaleAdjust));
                    m[M_X] = pos[0];
                    m[M_Z] = ShadowOffset;
                    m[M_Y] = 0.0f;
                    LIME_PlayFBXAtPos(m, 0, TrainScene, 0);
                    if (Settings[3])
                        PlaySoundId(get_tsound(8));
                    LIME_PlayFBXAtPos(m, 0,
                                      p ? TrainDie2Scene : TrainDie1Scene, 0);
                }
                if (LevelSelect == 11) {
                    BeginEffectMatrix();
                    m[M_X] = pos[0];
                    m[M_Y] = 0.0f;
                    m[M_Z] = ShadowOffset;
                    LIME_PlayFBXAtPos(m, 0,
                                      p ? SLDie2Scene : SLDie1Scene, 1);
                }
                break;

            case 59:
                DoingStageFatal = p + 1;
                if (GameMode == 1 && !isParentBasedOnSpeed())
                    DoingStageFatal = (DoingStageFatal == 2) ? 1 : 2;
                RoundParam[2] = 0x12c;
                break;

            case 60:
                limeMatrixCopy((const float *)(Players
                                   + (DoingStageFatal == 2 ? PLAYER_STRIDE : 0)
                                   + PLAYER_MATRIX), m);
                LIME_PlayFBXAtPos(m, 0, PitDeathScene, 1);
                LIME_PlayFBXAtPos(m, 1, PitDeathScene, 1);
                break;

            case 61:
                DoingStageFatal = p + 1;
                RoundParam[2]    = 0x190;
                if (GameMode == 1 && !isParentBasedOnSpeed())
                    DoingStageFatal = (DoingStageFatal == 2) ? 1 : 2;
                break;

            case 63:
                limeMatrixCopy((const float *)(Players
                                   + (DoingStageFatal == 2 ? PLAYER_STRIDE : 0)
                                   + PLAYER_MATRIX), m);
                m[M_Z] += 0.5f;
                LIME_PlayFBXAtPos(m, 0, BloodScene, 1);
                LIME_PlayFBXAtPos(m, 1, BloodScene, 1);
                break;

            case 65:
                if (Character2 == SHAO_KAHN) {
                    SaveUnclaimedTreasure(1);
                    DoingSKDeath         = 1;
                    SKDeathMessageOffset = 1.0f;
                    BeginEffectMatrix();
                    m[M_Z] = ShadowOffset;
                    m[M_X] = Camera[0];
                    m[M_Y] = 0.0f;
                    LIME_PlayFBXAtPos(m, 0, SKEffectScene, 1);
                    if (Settings[3])
                        PlaySoundId(get_tsound(0x88));
                }
                break;

            case 69:
                if (GameMode != 1)
                    opponentPerformedMercy = 1;
                break;

            default:
                break;
            }
            break;

        default:
            break;
        }
    }
}
