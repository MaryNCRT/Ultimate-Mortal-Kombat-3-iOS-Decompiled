/*
 * mk3.c -- gamecode/logic/mk3.c, decompiled.
 *
 * Nineteen functions, and they are the ones that turn everything else in this
 * directory into a running fight: the per-frame tick, the thread list every
 * object lives on, the input translation, gravity and the screen bounds, the
 * event queue, and the two init routines.
 *
 * It was left until last because `tools/progress.py` could not see it -- the
 * per-file table skipped any source with nothing written, so this file and six
 * others were invisible for weeks. Fixed in the session this file was opened.
 *
 * ## THIRTY. The number this file settles.
 *
 * Six globals, all reached through pointer slots, and the gaps between their
 * symbols measure every one of them:
 *
 *      _GrObj         0x0038c698   2280 bytes = 30 x  76   GROBJ_STRIDE
 *      _MKEventQueue  0x0038cf80     84 bytes = 4 + 10 x 8
 *      _Plyr          0x0038cff4   3240 bytes = 30 x 108   PLYR_STRIDE
 *      _Pp            0x0038dc9c   4200 bytes = 30 x 140   PP_STRIDE
 *      _TList         0x0038ed48      the live thread list
 *      _TList_Free    0x0038ed4c      the free list they come from
 *      _mytc          0x0038ef3c   8040 bytes = 30 x 268   MK3THREAD_STRIDE
 *
 * **Four parallel arrays of exactly thirty, and the strides are the four this
 * project had been assembling one function at a time.** Every one of them now
 * divides its array exactly, which is as close to proof as a symbol table gets.
 *
 * So the engine's hard limit is **thirty live things at once** -- two fighters
 * and twenty-eight everything-else -- and a thread, a fighter, a proc and a
 * GrObj with the same index are the same entity seen four ways. `MK3THREAD`'s
 * 0x100 is the index that ties them together, and `TList_Init` below writes it
 * once and never again.
 *
 * **The event queue is 4 + 10 x 8 bytes.** `MKEvent_Add` refuses an eleventh
 * with a `cmp #9`, and the symbol gap says the array behind it really is ten
 * long -- so the refusal is a bound, not a policy, and raising it in a port
 * needs the array grown too.
 */

#include <stdio.h>
#include "mk3logic.h"

/* Blood.c types the queue; GameCode.c declares the same global as `long *`.
 * The typed form is the right one and is repeated here rather than shared,
 * because there is no header for it yet and inventing one would move code this
 * file did not decompile. This file sides with Blood.c, and the layout below is
 * confirmed twice over: by `MKEvent_Add`'s four stores and by the 84-byte
 * symbol gap. */
typedef struct MKEVENT {
    signed char type;            /* 0x00 */
    signed char player;          /* 0x01 */
    signed char subtype;         /* 0x02 */
    signed char pad;             /* 0x03 */
    int32_t     param;           /* 0x04 */
} MKEVENT;

typedef struct MKEVENTQUEUE {
    int32_t count;               /* 0x00 */
    MKEVENT event[10];           /* 0x04  ten, from the symbol gap */
} MKEVENTQUEUE;

#define MKEVENT_MAX  10

extern MKEVENTQUEUE *MKEventQueue;         /* slot 0x00165664 -> 0x0038cf80 */
extern MK3THREAD    *P32 TList;                /* slot 0x00165684 -> 0x0038ed48 */
extern MK3THREAD    *P32 TList_Free;           /* slot 0x00165674 -> 0x0038ed4c */
extern MK3THREAD    *mytc;                 /* slot 0x0016566c -> 0x0038ef3c */

/* `Plyr` and `Pp` are declared in mk3logic.h as `char *` with their strides as
 * macros, because that is how the other twenty files in this directory reach
 * them and changing it here would rewrite them all. The two accesses below keep
 * that spelling. */


/* ------------------------------------------------------------- MKEvent_Clear
 *
 * armv7 0x00031a18, 16 bytes.  **Complete.**
 *
 *      MKEventQueue->count = 0
 *
 * The records are not cleared, only the count -- so whatever is past the new
 * end stays there until it is overwritten. Anything that reads the queue must
 * stop at the count and must not scan for a terminator; there is none, and the
 * stale records look exactly like live ones.
 */
void MKEvent_Clear(void)
{
    MKEventQueue->count = 0;
}


/* --------------------------------------------------------------- MKEvent_Add
 *
 * armv7 0x00031a28, 60 bytes.  **Complete.**
 *
 *      if (MKEventQueue->count > 9) return          ; and that is all it does
 *      e = &MKEventQueue->event[count]
 *      e->subtype = subtype                         ; strb [ip, #6]
 *      e->type    = (signed char)type               ; strb [ip, #4]
 *      e->param   = param                           ; str  [sb, r1, lsl #3]
 *      e->player  = player                          ; strb [ip, #5]
 *      MKEventQueue->count = count + 1
 *
 * **The queue holds ten and drops the eleventh in silence.** There is no return
 * value and no error path -- `cmp #9` then `bgt` straight to the `pop {pc}`. A
 * frame that generates more than ten events loses the rest and nothing anywhere
 * finds out. The symbol gap says the array is ten long, so this is the real
 * bound and not a throttle someone can just raise.
 *
 * That is worth knowing before a port "improves" it: code elsewhere may depend
 * on events being dropped under load rather than queued up and delivered a
 * frame or two late, which is a different bug and a much harder one to see.
 *
 * **`param` is stored through the ADVANCED index.** `r1` is `count + 1`, needed
 * for the count store at the end, and the compiler reuses it: `[sb, r1, lsl #3]`
 * is `queue + (count+1)*8`, which is `queue + 4 + count*8 + 4` -- the `param`
 * field of record `count`. One register doing two jobs, and the same
 * shared-literal habit this directory is full of, applied to an index.
 *
 * **The first argument is sign-extended on the way in** (`sxtb r0`) and the
 * field is a signed char, so a type above 127 arrives negative. The callers
 * measured so far pass 1 and 3.
 *
 * The argument order is not the field order -- `type`, `subtype`, `param`,
 * `player` going in, landing at bytes 0, 2, word 4, byte 1 -- but the callers
 * read naturally with it: mkfatal.c's `MKEvent_Add(3, 9, 0, 0)` is type 3
 * subtype 9, and `MKEvent_Add(1, 1, x, strength)` puts a coordinate in `param`
 * and a strength index in `player`, which is what that field is for.
 */
void MKEvent_Add(long type, long subtype, long param, long player)
{
    MKEVENTQUEUE *q = MKEventQueue;
    int32_t       n = q->count;

    if (n > MKEVENT_MAX - 1)                 /* the eleventh is dropped */
        return;

    q->event[n].subtype = (signed char)subtype;
    q->event[n].type    = (signed char)type;
    q->event[n].param   = (int32_t)param;
    q->event[n].player  = (signed char)player;

    q->count = n + 1;
}


/* ---------------------------------------------------------- mk3_who_in_front
 *
 * armv7 0x00031eb4, 16 bytes.  **Complete.**
 *
 *      return *(long *)(G + 0x474)
 *
 * A load and nothing else. `G + 0x474` is a new offset in the global block, and
 * it sits immediately after the two already known there: 0x468 and 0x470 are
 * the camera's left and right edges, so the three are one group -- where the
 * camera is, and which fighter it is favouring.
 */
long mk3_who_in_front(void)
{
    return *(long *)(G_BYTES + 0x474);
}


/* ---------------------------------------------------------------- no_ai_hack
 *
 * armv7 0x00031d48, 32 bytes.  **Complete.**
 *
 *      ((MK3OBJ *)Plyr)->field00->field10                |= 1
 *      ((MK3OBJ *)(Plyr + PLYR_STRIDE))->field00->field10 |= 1
 *
 * **Both fighters get bit 0 of their proc's 0x10, and the name says what that
 * means: no AI.** mk3logic.h already records `isp2` OR-ing bit 4 into the same
 * word, so 0x10 is a flag set and bit 0 is "this one is driven from outside".
 *
 * For anything that wants a fight with no computer opponent -- a test, a
 * training mode, a character moving on its own against a dummy -- **this is the
 * switch, and it is thirty-two bytes.** It is the one function in this file
 * whose name was written by someone who knew they were cutting a corner.
 *
 * `Plyr[1]` is reached as `[r1, #0x6c]`. 0x6c is 108, so the second fighter's
 * offset is `PLYR_STRIDE` stated outright as a constant rather than computed --
 * a fourth independent confirmation of that stride, and the cheapest one.
 */
void no_ai_hack(void)
{
    ((MK3OBJ *)Plyr)->field00->field10 |= 1;
    ((MK3OBJ *)(Plyr + PLYR_STRIDE))->field00->field10 |= 1;
}


/* ------------------------------------------------------- mk3_set_four_button
 *
 * armv7 0x00031ec4, 32 bytes.  **Complete.**
 *
 *      *(uint16_t *)(Pp + player * PP_STRIDE + 0x7c) = value
 *
 * **The multiply is written out in shifts, and it is where `Pp`'s stride comes
 * from**: `(p<<2) + (p<<4)` is `p*20`, then `(x<<3) - x` is `x*7`, so the whole
 * thing is `p*140`. 140 is 0x8c, `MK3OBJPROC`'s last known field is the word at
 * 0x88, and 0x88 + 4 is 0x8c -- so **the struct is exactly 140 bytes with
 * nothing after 0x88**, and the 4200-byte symbol gap divides by it thirty times.
 * A size this project had been building up one field at a time is now measured
 * three ways.
 *
 * 0x7c is the four-button gate and the header already had it as a signed
 * halfword; this stores a halfword (`strh`), so a value above 0xffff is
 * truncated here rather than at the reader.
 */
void mk3_set_four_button(long player, long value)
{
    *(uint16_t *)(void *)(Pp + player * PP_STRIDE + 0x7c) = (uint16_t)value;
}


/* ============================== TList_Get, TList_GetFront, TList_Release
 *
 * The allocator. Three routines, one free list, and **which end of `TList` a
 * thread goes on is a behavioural decision, not a style one.**
 *
 * Whatever walks `TList` runs the threads in list order, so:
 *
 *      TList_Get       appends at the TAIL -- acts AFTER everything that exists
 *      TList_GetFront  links at the HEAD  -- acts BEFORE everything that exists
 *
 * A port that keeps one list and ignores which end things go on will run the
 * fight in a subtly different order every frame and will not be able to say
 * why. That is the whole reason two functions exist for one job.
 *
 * Both **return NULL when the free list is empty, with no complaint.** With
 * twenty-eight free threads that is reachable, and `is_jade_protected` in
 * mkzap.c walks `TList` looking for pid 0x11f -- so a failed allocation there is
 * indistinguishable from a thread that was never started. Nothing checks.
 *
 * Both clear `pid` (0x104) and **neither clears 0x100**, the index `TList_Init`
 * stamped in. That is deliberate: 0x100 is what makes `Plyr`, `Pp`, `GrObj` and
 * `mytc` parallel, and it belongs to the slot, not to whatever is using it.
 */
MK3THREAD *TList_Get(void)
{
    MK3THREAD  *t = TList_Free;
    MK3THREAD *P32 *tail;

    if (t == NULL)
        return NULL;

    TList_Free = t->next;

    tail = &TList;
    while (*tail != NULL)                    /* walk to the end */
        tail = &(*tail)->next;

    t->next = NULL;
    t->pid  = 0;
    *tail   = t;

    return t;
}

MK3THREAD *TList_GetFront(void)
{
    MK3THREAD *t = TList_Free;

    if (t == NULL)
        return NULL;

    TList_Free = t->next;

    t->next = TList;                         /* at the head, not the end */
    TList   = t;

    t->pid = 0;

    return t;
}


/* -------------------------------------------------------------- TList_Release
 *
 * armv7 0x00031c78, 64 bytes.  **Complete.**
 *
 *      if (t == NULL) return
 *      for every node in TList equal to t:
 *          unlink it
 *          push it on the front of TList_Free
 *
 * **It removes EVERY occurrence, not the first.** After unlinking one it
 * re-reads the predecessor's link (`ldr r3, [r1]`) and, if that is non-zero,
 * branches back into the scan. So a thread that somehow got onto `TList` twice
 * is taken off twice and pushed onto the free list twice -- which would corrupt
 * the free list rather than fix it. This is defensive code against a state that
 * should be impossible, written in a way that makes the impossible state worse.
 *
 * Recorded rather than tidied: a port that "simplifies" this to a single unlink
 * changes behaviour only in a case the game may never reach, and the honest
 * thing is to keep the loop and note that nothing is known to exercise it.
 *
 * **The freed thread keeps its pid.** Only the allocators clear it, so between
 * a release and the next `TList_Get` a thread sitting on the free list still
 * answers to its old pid -- and `FindThread`-style walks over `TList` cannot see
 * it, but anything walking the free list would.
 *
 * It does not clear 0x100 either, for the same reason the allocators do not.
 */
void TList_Release(MK3THREAD *t)
{
    MK3THREAD *P32 *prev;
    MK3THREAD  *n;

    if (t == NULL)
        return;

    prev = &TList;
    n    = TList;

    for (;;) {
        while (n != t) {
            if (n == NULL)
                return;
            prev = &n->next;
            n    = n->next;
        }

        *prev      = n->next;                /* unlink */
        n->next    = TList_Free;             /* and onto the free list */
        TList_Free = n;

        n = *prev;                           /* and keep looking */
        if (n == NULL)
            return;
    }
}


/* ----------------------------------------------------------------- TList_Init
 *
 * armv7 0x00031bb4, 100 bytes.  **Complete.**
 *
 *      TList      = &mytc[0]
 *      mytc[n].next = &mytc[n+1]   for n = 0 .. 28
 *      mytc[1].next = NULL                      ; written AFTER the loop
 *      mytc[29].next = NULL
 *      TList_Free = &mytc[2]
 *      mytc[n].field100 = n        for n = 0 .. 29
 *
 * **This is where the thirty comes from, and where the two fighters are born.**
 * The loop chains all thirty threads into one list, and then two stores cut it
 * in two:
 *
 *      TList       = mytc[0] -> mytc[1] -> NULL           two threads
 *      TList_Free  = mytc[2] -> ... -> mytc[29] -> NULL   twenty-eight
 *
 * So **`TList` starts out holding exactly the two fighters and nothing else**,
 * and every projectile, decoy, striker and effect in the game comes out of the
 * twenty-eight behind them. `mytc[1].next = NULL` is written after the loop that
 * had just linked it to `mytc[2]`, which is the cut: one store, and the array
 * becomes two lists.
 *
 * The loop bound is a literal: `cmp r2, #0x1e5c`, and 0x1e5c is 29 * 0x10c. The
 * index loop's bound is `cmp r3, #0x1e` -- thirty. Two different spellings of
 * the same count, in one function, from two different registers.
 *
 * **0x100 is the slot index and it is written once, here, for good.** Neither
 * allocator clears it and neither does `TList_Release`, so `mytc[n]` answers
 * `n` for the life of the process. mk3logic.h calls the field `player` because
 * `getobjectinsert` multiplies it by `PLYR_STRIDE` to index `Plyr` -- and the
 * four 30-element arrays say why that is right rather than a bug: **a thread,
 * a fighter, a proc and a GrObj with the same index are one entity.** `player`
 * is a narrow name for it; the field is the entity's identity.
 *
 * There is no `mytc[0].field100 = 0` in a loop -- it is stored separately with
 * the zero register that was already live, and the loop starts at `mytc[1]`
 * with 1. Another shared register, another one-instruction saving.
 */
void TList_Init(void)
{
    long n;

    TList = &mytc[0];

    for (n = 0; n <= 28; n++)
        mytc[n].next = &mytc[n + 1];

    mytc[1].next  = NULL;                    /* the cut: TList is two long */
    mytc[29].next = NULL;                    /* and the free list ends */

    TList_Free = &mytc[2];

    mytc[0].player = 0;
    for (n = 1; n < 30; n++)
        mytc[n].player = (uint32_t)n;
}


/* ----------------------------------------------------------- TranslateJoybits
 *
 * armv7 0x00031a64, 224 bytes.  **Complete.**  Called from `mk3_update`, once,
 * and from nowhere else.
 *
 * **This is the input layer, and it is the whole of it.** Two raw words in, one
 * engine word out. Everything the fight reads about buttons comes from here.
 *
 *      G[0x10] = G[0x08]                        ; last frame's raw P1
 *      G[0x14] = G[0x0c]                        ; last frame's raw P2
 *
 *      p1 = joy[0]
 *      if (p1 & 0x400)  G[0x08] = p1 & 0x3ff    ; RAW, untranslated
 *      else             spread p1 into `out`
 *
 *      p2 = joy[1]
 *      if (p2 & 0x400)  G[0x0c] = p2 & 0x3ff    ; RAW, untranslated
 *      else             spread p2 into `out`
 *
 *      G[0x1c] =  out
 *      G[0x18] = ~out
 *
 * ## The bit layout, and the cross-check that proves it
 *
 * Nine bits come in per player -- four directions and six buttons -- and they
 * are spread into one word, one player in the low half and one in the high:
 *
 *      in bit   0..3   ->  out  0..3     P1 directions
 *      in bit   4      ->  out  4        P1 button, low group
 *      in bit   6      ->  out  5        P1 button, low group
 *      in bit   7      ->  out  6        P1 button, low group
 *      in bit   5      ->  out 16        P1 button, high group
 *      in bit   8      ->  out 17        P1 button, high group
 *      in bit   9      ->  out 18        P1 button, high group
 *
 *      in bit   0..3   ->  out  8..11    P2 directions
 *      in bit   4      ->  out 12        P2 button, low group
 *      in bit   6      ->  out 13        P2 button, low group
 *      in bit   7      ->  out 14        P2 button, low group
 *      in bit   5      ->  out 20        P2 button, high group
 *      in bit   8      ->  out 21        P2 button, high group
 *      in bit   9      ->  out 22        P2 button, high group
 *
 * **The six buttons are split into two groups of three, and the split is not
 * arbitrary: it is input bits {4,6,7} against {5,8,9}.** Read down the list and
 * the two groups land as 0x00000070 / 0x00070000 for P1 and 0x00007000 /
 * 0x00700000 for P2.
 *
 * Or, in one number each: **0x00070070 and 0x00707000.** Those are, to the bit,
 * the two masks `buttons_in_a2` in moves.c picks between on the strength index:
 *
 *      obj->field28 = 0x00070070;  if (strength) obj->field28 = 0x00707000;
 *      obj->field24 = G[0x1c] & obj->field28;
 *
 * **That file was decompiled long before this one, from a different function,
 * and the two agree exactly.** It is the strongest confirmation this project
 * has produced for any layout: the writer and the reader were read
 * independently and the bits line up.
 *
 * It also says what the masks mean. `buttons_in_a2` masks off the DIRECTIONS
 * and keeps the six buttons -- so a move lookup sees buttons only, and the
 * directions are read by something else.
 *
 * The buttons are deliberately NOT named here. Which of the six is high punch
 * and which is block is not decidable from this routine, and the arcade layout
 * is a question for MAME and the ROM, not for a guess written into a header.
 *
 * ## Bit 0x400: the escape hatch
 *
 * Bit 10 of an incoming word means **do not translate me**. The low ten bits
 * are then dropped, unchanged, into `G[0x08]` (P1) or `G[0x0c]` (P2) -- a
 * DIFFERENT pair of slots from the translated output, and the two are shadowed
 * into `G[0x10]` / `G[0x14]` at the top of every call, so something wants both
 * this frame's and last frame's copy.
 *
 * **The raw path contributes nothing to `G[0x1c]`.** A player whose word has bit
 * 10 set is, as far as `buttons_in_a2` and every other consumer of 0x1c is
 * concerned, pressing nothing at all.
 *
 * **What bit 10 means is settled by `mk3_update`, the only caller**, and the
 * answer arrived a few hours after this note first guessed at it. The guess was
 * demo playback, because `_Playback` sits near `_Plyr` in the symbol table. It
 * was wrong, and it is recorded here as wrong because a tested prediction is
 * worth more in the file than a quiet correction.
 *
 * **Bit 10 marks a SPECIAL-MOVE REQUEST, and the low ten bits name the move.**
 * `mk3_update` consumes it twice, before and after this routine:
 *
 *   - Before, when `RoundParam[0x34]` is set: `seq_lookup` turns the ten bits
 *     into a recorded stick sequence and `Playback` then synthesises an
 *     ordinary joystick word every frame. The special move is replayed as if
 *     someone had done the motion, so by the time this routine runs the word is
 *     a normal one and bit 10 is gone.
 *   - After, for any word that still carries it: `DoSpecial` is called on that
 *     fighter and the word is zeroed.
 *
 * So this routine's raw path is reached only when neither of those handled it,
 * and 0x08 / 0x0c are where the unconsumed request is parked for `DoSpecial`.
 * `Playback` was the right global for the wrong reason.
 *
 * Still open: **nothing decompiled reads 0x10 or 0x14**, the previous-frame
 * shadows. They are written unconditionally at the top of every call, so
 * something wants edge detection on the raw words.
 *
 * ## For the port
 *
 * A keyboard or a gamepad has to produce exactly two words here, nine bits
 * each, bit 10 clear. That is the entire contract. There is no touch anywhere
 * in this routine and never was -- the touch layer sits above it, in the iOS
 * code that fills `joy[]`, and replacing that layer replaces the input method
 * without touching a line of the fight.
 */
void TranslateJoybits(const long *joy)
{
    char    *g  = G_BYTES;
    uint32_t p1 = (uint32_t)joy[0];
    uint32_t p2;
    uint32_t out = 0;

    *(uint32_t *)(g + 0x10) = *(uint32_t *)(g + 0x08);
    *(uint32_t *)(g + 0x14) = *(uint32_t *)(g + 0x0c);

    if ((p1 & 0x400u) != 0) {
        *(uint32_t *)(g + 0x08) = p1 & 0x3ffu;   /* raw, and out stays 0 */
    } else {
        out = p1 & 0xfu;                         /* directions pass through */
        if (p1 & 0x010u) out |= 0x00000010u;
        if (p1 & 0x020u) out |= 0x00010000u;
        if (p1 & 0x040u) out |= 0x00000020u;
        if (p1 & 0x080u) out |= 0x00000040u;
        if (p1 & 0x100u) out |= 0x00020000u;
        if (p1 & 0x200u) out |= 0x00040000u;
    }

    p2 = (uint32_t)joy[1];

    if ((p2 & 0x400u) != 0) {
        *(uint32_t *)(g + 0x0c) = p2 & 0x3ffu;
    } else {
        out |= (p2 & 0xfu) << 8;                 /* and P2's into bits 8..11 */
        if (p2 & 0x010u) out |= 0x00001000u;
        if (p2 & 0x020u) out |= 0x00100000u;
        if (p2 & 0x040u) out |= 0x00002000u;
        if (p2 & 0x080u) out |= 0x00004000u;
        if (p2 & 0x100u) out |= 0x00200000u;
        if (p2 & 0x200u) out |= 0x00400000u;
    }

    *(uint32_t *)(g + 0x1c) = out;
    *(uint32_t *)(g + 0x18) = ~out;
}


/* ----------------------------------------------------------- gravity_n_bounds
 *
 * armv7 0x00031b44, 112 bytes.  **Complete.**  Called from `DisplayUpdate`,
 * once, and from nowhere else.
 *
 *      part  = obj->field08
 *      left  = G[0xb0] + 0x3a
 *      right = G[0xb4] + 0x15c + 3
 *      x     = (int16_t)part->0x0e
 *
 *      if (x <= left  && !(part->0x30 & 0x400)) {
 *          if ((int32_t)part->0x18 < 0) part->0x18 = 0
 *          x = left
 *      }
 *      if (x >= right && !(part->0x30 & 0x400)) {
 *          if ((int32_t)part->0x18 > 0) part->0x18 = 0
 *          x = right
 *      }
 *      part->0x0e = (int16_t)x
 *
 *      if (part->0x20 != 0) part->0x1c += part->0x20
 *
 * **Two unrelated jobs in one routine, and the name admits it.** The bounds are
 * horizontal and the gravity is vertical, and they share a function only
 * because `DisplayUpdate` wanted one call per object.
 *
 * ## The bounds
 *
 * The arena is `[G[0xb0] + 0x3a, G[0xb4] + 0x15f]`, and **GameCode.c already
 * knew what those two are.** `SetupLevelLimits` there writes:
 *
 *      RoundParam[0] = Level_Info[lvl].leftPlayerLimit
 *      RoundParam[1] = Level_Info[lvl].rightPlayerLimit
 *      G[0xb0]       = RoundParam[0]
 *      G[0xb4]       = RoundParam[1] - 399
 *
 * Substitute, and the walls are exactly
 *
 *      left  = leftPlayerLimit  + 58
 *      right = rightPlayerLimit - 48          ; -399 + 351
 *
 * So the two hard-coded insets in this routine are **58 pixels in from the
 * level's own left limit and 48 in from its right**, and a port that reads the
 * level data has everything it needs. The asymmetry is real: the two insets are
 * not the same number.
 *
 * They are NOT the camera pair at 0x468/0x470 that four other routines use --
 * `SetupLevelLimits` derives `CamLeftLimit` and `CamRightLimit` from the same
 * two limits with two DIFFERENT margins. **The walls and the camera are two
 * different insets off one pair of limits**, which is why a fighter can be
 * against a wall with the camera still moving.
 *
 * 0x15f is assembled as `0x15c` then `+3`, in two instructions, because it is
 * not a Thumb-2 modified immediate. Not a shared literal -- just the encoding.
 *
 * **Hitting a wall zeroes the velocity INTO it and leaves the other direction
 * alone**: `< 0` on the left, `> 0` on the right. So a fighter pinned against
 * the left wall can still be pushed right on the same frame, and 0x18 is the
 * horizontal velocity.
 *
 * ## The override
 *
 * **Bit 10 of `part->0x30` turns the walls off entirely** -- both of them, each
 * tested separately with the same mask. That is how anything leaves the arena:
 * a pit fall, a body flying off the bell tower, a projectile that should not
 * stop at the edge. A port that clamps unconditionally traps all of them at the
 * wall, and the symptom will look like a physics bug rather than a missing flag.
 *
 * Bit 10 is 0x400, the same numeric value as `TranslateJoybits`' escape hatch
 * above, on a different word in a different struct. Coincidence, noted here so
 * nobody connects them.
 *
 * ## The gravity
 *
 * `part->0x1c += part->0x20`, guarded by `0x20 != 0`. So **0x20 is the
 * acceleration and 0x1c is the vertical velocity**, and the guard means an
 * object with no gravity set costs a compare instead of a read-modify-write.
 * Nothing here integrates velocity into position -- that happens elsewhere, and
 * this routine only feeds it.
 *
 * The pairing is what makes the reading safe: 0x18 is the horizontal velocity
 * (zeroed at a wall), 0x1c the vertical one (accumulated into), 0x20 the
 * constant added to it. Three adjacent words, one meaning each.
 *
 * **This is jumping.** A jump is a negative 0x1c and a positive 0x20; the arc
 * comes out of this one add.
 */
void gravity_n_bounds(MK3OBJ *obj)
{
    char *part  = (char *)(void *)obj->field08;
    long  left  = *(long *)(G_BYTES + 0xb0) + 0x3a;
    long  right = *(long *)(G_BYTES + 0xb4) + 0x15c + 3;
    long  x     = *(int16_t *)(void *)(part + 0x0e);

    if (left >= x && (*(uint32_t *)(void *)(part + 0x30) & 0x400u) == 0) {
        if (*(int32_t *)(void *)(part + 0x18) < 0)
            *(int32_t *)(void *)(part + 0x18) = 0;
        x = left;
    }

    if (x >= right && (*(uint32_t *)(void *)(part + 0x30) & 0x400u) == 0) {
        if (*(int32_t *)(void *)(part + 0x18) > 0)
            *(int32_t *)(void *)(part + 0x18) = 0;
        x = right;
    }

    *(int16_t *)(void *)(part + 0x0e) = (int16_t)x;

    part = (char *)(void *)obj->field08;         /* reloaded, as the binary does */
    if (*(int32_t *)(void *)(part + 0x20) != 0)
        *(int32_t *)(void *)(part + 0x1c) +=
            *(int32_t *)(void *)(part + 0x20);
}


/* ------------------------------------------------------------- mk3_bloodevent
 *
 * armv7 0x00031d0c, 60 bytes.  **Complete.**
 *
 *      g     = GrObj + who * 76
 *      param = (g->0x0e << 16) | g->0x12        ; two halfwords, packed
 *      blood[who] = kind
 *      MKEvent_Add(0, kind, param, who)
 *
 * **The stride is 76 and the compiler spells it out**: `(w<<2) + (w<<4)` is
 * `w*20`, minus `w` is `w*19`, then `<<2` is `w*76`. That is `GROBJ_STRIDE`,
 * measured a second way and agreeing with the 2280-byte symbol gap.
 *
 * **A position is passed through the event queue as one packed word**: x in the
 * high half, y in the low, taken from the GrObj's 0x0e and 0x12 -- the same two
 * halfword coordinates `MK3_FIELD0E` and `MK3_FIELD12` name on an MK3OBJ. So
 * `MKEVENT.param` is not always a scalar, and a reader has to know the event
 * type before it can know what `param` means. Type 0 means "unpack me".
 *
 * `_blood` is 8 bytes in the symbol table -- **two longs, one per fighter** --
 * and the kind is written there as well as sent. So there are two consumers: a
 * queue reader that gets it once, and anything that polls `blood[who]` and gets
 * it until the next call. Blood.c is where the first of those lives.
 *
 * The event type is a literal 0, and it is the only type-0 producer found so
 * far. mkfatal.c sends 1 and 3.
 */
extern long blood[];                       /* slot 0x0016565c -> 0x0038ed50 */
extern char *GrObj;                        /* slot 0x00165668 -> 0x0038c698 */

void mk3_bloodevent(long who, long kind)
{
    const char *g = GrObj + who * GROBJ_STRIDE;
    long param = (long)(((uint32_t)*(const uint16_t *)(const void *)(g + 0x0e) << 16)
                        | (uint32_t)*(const uint16_t *)(const void *)(g + 0x12));

    blood[who] = kind;

    MKEvent_Add(0, kind, param, who);
}


/* ------------------------------------------------------------------ mk3_dizzy
 *
 * armv7 0x00031ee4, 76 bytes.  **Complete.**
 *
 *      *(uint16_t *)(G + 0x45c) = 3
 *      StartThreadAt(&mytc[1], t_dizzy_dude)
 *      Pp[1].field10 |= 1
 *
 * **This settles how the four parallel arrays are wired to each other.**
 * `no_ai_hack` above writes `((MK3OBJ *)Plyr)->field00->field10 |= 1` through a
 * pointer; this writes `Pp` + 0x9c directly, and 0x9c is `PP_STRIDE + 0x10` --
 * **`Pp[1].field10`.** Two routines, two spellings, one word. So
 * **`Plyr[n].field00` is `&Pp[n]`**: the fighter's `field00` is not some
 * arbitrary proc, it is the proc with the same index, and the arrays really are
 * parallel rather than merely the same length.
 *
 * That also makes this function's effect precise: it **takes the AI off player
 * two only**, where `no_ai_hack` takes it off both.
 *
 * `mytc[1]` is player two's thread -- the second of the two `TList_Init` leaves
 * on `TList` -- and it is restarted at `t_dizzy_dude` outright. Not a handover,
 * not a descent: `StartThreadAt` replaces whatever that fighter was doing.
 *
 * `G + 0x45c` is a halfword and a new offset; 3 is written as a full word
 * register and stored with `strh`, so the high half is discarded here.
 *
 * The name and the shape together say what it is: a debug switch that stuns
 * player two so a tester can hit them.
 */
long t_dizzy_dude(MK3THREAD *thread);      /* slot 0x000f3864 -> 0x0007d9c4 */

void mk3_dizzy(void)
{
    *(uint16_t *)(void *)(G_BYTES + 0x45c) = 3;

    StartThreadAt(&mytc[1], (MK3THREADFUNC)t_dizzy_dude);

    *(uint32_t *)(void *)(Pp + PP_STRIDE + 0x10) |= 1;
}


/* ---------------------------------------------------------------- mk3_getbbox
 *
 * armv7 0x00031cb8, 84 bytes.  **Complete.**
 *
 *      mk3_getbbox_cb(ani, left, top, right, bottom)     ; indirect
 *      if (ani == 0x12be) {
 *          *left = -0x20; *top = 0x44; *right = 0x18; *bottom = 0x8c;
 *      }
 *
 * **It is a wrapper around a function pointer, with one animation hard-coded
 * past it.** `_mk3_getbbox_cb` is a global slot, so the real measurement is
 * installed at run time by whatever owns the animation data -- and this routine
 * does not care what it computed, it just overwrites the answer for 0x12be.
 *
 * **0x12be is a ninth port-critical constant**, and a different kind from the
 * eight character numbers already catalogued: those pick a fighter, this one
 * picks an animation whose measured box is wrong and patches it. A port that
 * re-derives boxes from the asset files and drops this line gets one animation
 * subtly mis-sized, in a way that will look like an asset bug.
 *
 * The four values are one register walked along: `mvn #0x1f` is -0x20, then
 * `+0x64` is 0x44, `-0x2c` is 0x18, `+0x74` is 0x8c. The shared-literal habit
 * this directory is full of, four deep -- and it means **the four numbers must
 * be transcribed as the chain, not as four independent constants**, or a later
 * edit to one silently breaks the next.
 *
 * In the caller's order -- `mkanimal.c` stores them into 0x34, 0x38, 0x3c, 0x40
 * -- that is left -32, top 68, right 24, bottom 140: **56 wide and 72 tall.**
 *
 * The fifth argument arrives on the stack and is passed straight through on the
 * stack; the other four are in registers. Five arguments is the widest call in
 * this directory so far.
 */
extern void (*P32 mk3_getbbox_cb)(long ani, int *left, int *top,
                              int *right, int *bottom);

#define MK3_BBOX_PATCHED_ANI  0x12be

void mk3_getbbox(long ani, int *left, int *top, int *right, int *bottom)
{
    /* through a plain pointer: clang cannot call a 32-bit one (P32) */
    ((void (*)(long, int *, int *, int *, int *))mk3_getbbox_cb)
        (ani, left, top, right, bottom);

    if (ani == MK3_BBOX_PATCHED_ANI) {
        int v = ~0x1f;                     /* -0x20, and the chain starts here */
        *left   = v;
        v      += 0x64;                    /*  0x44 */
        *top    = v;
        v      -= 0x2c;                    /*  0x18 */
        *right  = v;
        v      += 0x74;                    /*  0x8c */
        *bottom = v;
    }
}


/* -------------------------------------------------------------- DisplayUpdate
 *
 * armv7 0x00032220, 160 bytes.  **Complete.**
 *
 *      repell_func()
 *      gravity_n_bounds(&Plyr[0])
 *      gravity_n_bounds(&Plyr[1])
 *
 *      GrObj[0].0x0c += GrObj[0].0x18       ; x += vx
 *      GrObj[0].0x10 += GrObj[0].0x1c       ; y += vy
 *      GrObj[1].0x0c += GrObj[1].0x18
 *      GrObj[1].0x10 += GrObj[1].0x1c
 *
 *      for (t = mytc[1].next; t; t = t->next) {
 *          g = &GrObj[t->player];
 *          g->0x0c += g->0x18;
 *          g->0x10 += g->0x1c;
 *      }
 *
 * ## This routine plus `gravity_n_bounds` is the entire physics of the game
 *
 * Between them, four words in a `GrObj` do everything:
 *
 *      0x0c   x, 16.16 fixed point -- its high half is the halfword at 0x0e
 *      0x10   y, 16.16 fixed point -- its high half is the halfword at 0x12
 *      0x18   x velocity, added to 0x0c once a frame, HERE
 *      0x1c   y velocity, added to 0x10 once a frame, HERE
 *      0x20   gravity, added to 0x1c once a frame, in `gravity_n_bounds`
 *      0x30   flags; bit 10 turns the arena walls off
 *
 * **`MK3OBJ.field08` is a pointer into `GrObj`.** That is what ties this
 * function to the last one: `gravity_n_bounds` takes an `MK3OBJ *`, works on
 * `obj->field08`, and touches 0x0e, 0x18, 0x1c, 0x20 and 0x30 -- every one of
 * them a `GrObj` field, and `mk3_bloodevent` reads 0x0e and 0x12 off a `GrObj`
 * by index to get a position. The "part" this directory has been calling
 * `field08` for twenty files is a GrObj record.
 *
 * And the 16.16 split is the one mk3logic.h already documents for `MK3OBJ`'s
 * own 0x0c and 0x10 -- "x and y as 16.16 fixed point, with the integer part on
 * top and a fraction underneath that only a copy preserves". **The same layout,
 * at the same offsets, in the other struct.** So the fraction is not decorative:
 * it is where sub-pixel movement accumulates between frames, and a port that
 * keeps positions as integers gets visibly different motion.
 *
 * **A jump is one negative 0x1c and one positive 0x20**, and the arc is these
 * two adds. Nothing else is involved.
 *
 * ## The walk starts at mytc[1].next, not at TList
 *
 * The two fighters are `mytc[0]` and `mytc[1]`, handled above by name, and the
 * loop picks up whatever is linked after them. `TList_Get` appends at the tail,
 * so **everything it ever allocates lands on this chain** -- which is why the
 * loop can start there and skip exactly the two objects already done.
 *
 * But `TList_GetFront` links at the HEAD, in front of `mytc[0]`. **A thread
 * taken from the front is never integrated by this loop.** It gets no velocity
 * applied at all. That is a real difference between the two allocators, on top
 * of the execution-order one, and it is invisible from either allocator alone.
 * `mk3_init` is the only caller of `TList_GetFront` found so far, which is
 * consistent -- whatever it takes from the front is not a moving object.
 *
 * `t->player` is the slot index `TList_Init` stamped in and nobody clears, used
 * here to index `GrObj`. Fourth confirmation that the four 30-element arrays
 * are one entity seen four ways.
 *
 * `repell_func` comes first and is in mkrepell.c, which is a one-function file
 * nobody has opened. It runs before gravity, so whatever it does to velocities
 * is what gravity and the walls then correct.
 */
void repell_func(void);

void DisplayUpdate(void)
{
    MK3THREAD *t;
    char      *g;

    repell_func();

    gravity_n_bounds((MK3OBJ *)(void *)Plyr);
    gravity_n_bounds((MK3OBJ *)(void *)(Plyr + PLYR_STRIDE));

    /* The two fighters, unrolled. The binary reaches the second one with
     * absolute displacements -- 0x58 is 0x4c + 0x0c and 0x64 is 0x4c + 0x18 --
     * rather than by adding the stride, which is the same thing written
     * shorter. */
    g = GrObj;
    *(int32_t *)(void *)(g + 0x0c) += *(int32_t *)(void *)(g + 0x18);
    *(int32_t *)(void *)(g + 0x10) += *(int32_t *)(void *)(g + 0x1c);

    g = GrObj + GROBJ_STRIDE;
    *(int32_t *)(void *)(g + 0x0c) += *(int32_t *)(void *)(g + 0x18);
    *(int32_t *)(void *)(g + 0x10) += *(int32_t *)(void *)(g + 0x1c);

    /* and everything TList_Get has appended behind them */
    for (t = mytc[1].next; t != NULL; t = t->next) {
        g = GrObj + t->player * GROBJ_STRIDE;
        *(int32_t *)(void *)(g + 0x0c) += *(int32_t *)(void *)(g + 0x18);
        *(int32_t *)(void *)(g + 0x10) += *(int32_t *)(void *)(g + 0x1c);
    }
}


/* ----------------------------------------------------------------- mk3_remap
 *
 * armv7 0x00031d68, 332 bytes.  **Complete, and verified exhaustively.**
 * Called from `mk3_update`, once, and from nowhere else.
 *
 *      sixty-nine values map to six constants; everything else is returned
 *      unchanged.
 *
 * ## How this one was read
 *
 * It is a compiled `switch` -- a balanced binary search with subtract-and-
 * compare range tests, three levels deep, spread over 332 bytes with no jump
 * table. Transcribing that by eye gets a case list wrong sooner or later and
 * nothing would ever notice, because a wrong entry here just means one
 * animation is not substituted.
 *
 * So it was not transcribed by eye. **The routine touches no memory -- it is
 * compares, branches, and `mov` of literals -- so it can be emulated exactly**,
 * and it was: every input from 0 to 0x20000 plus a handful of negatives, run
 * through an interpreter over the instruction listing, and the resulting table
 * is what the `switch` below spells out. The C was then compiled and run
 * against that same table, input by input, and agrees on all 131,072.
 *
 * That is the first function in this directory proved rather than argued, and
 * it is worth saying why it was possible: **no loads, no stores, no calls.**
 * Any function with that shape can get the same treatment.
 *
 * ## What it does
 *
 * Six outputs, 0x1aaf through 0x1ab5, with **0x1ab4 conspicuously absent** --
 * so the destination set is seven consecutive numbers with one unused, which
 * reads like a table someone removed an entry from.
 *
 * The inputs cluster, and the clusters are the interesting part:
 *
 *      -> 0x1aaf   0x3f5..0x404 (7), 0x119f..0x11a9 (9), 0x17a3/0x17a5
 *      -> 0x1ab0   0x0a94..0x0a9b (7)
 *      -> 0x1ab1   0x1331..0x1336 (6), 0x14d5..0x14d9 (4)
 *      -> 0x1ab2   0x09d2..0x09da (9), 0x0c23/0x0c24, 0x11df..0x11ed (10),
 *                  0x11fd..0x1201 (5)
 *      -> 0x1ab3   0x12a4                        one value, alone
 *      -> 0x1ab5   0x00e6..0x00f5 (7)
 *
 * **Runs of consecutive numbers with gaps in them.** 0x11a0 and 0x11a2 are not
 * in the 0x1aaf list but 0x119f, 0x11a1 and 0x11a3 are; 0x0a9a is missing from
 * the middle of 0x1ab0's run. Those are animation numbers, and the gaps are
 * frames that did not need substituting -- which is what makes this a remap
 * table and not a range check.
 *
 * **What the numbers mean is not decided here and must not be guessed.** They
 * are the same order of magnitude as the animation ids `mk3_getbbox` and
 * `get_char_ani2` handle, and 0x12a4 sits near `mk3_getbbox`'s hard-coded
 * 0x12be -- but "near" is not evidence. `mk3_update` is the only caller, so
 * reading that will say what is being remapped and when. Until then this is a
 * verified table with an unknown subject.
 *
 * **All sixty-nine numbers are port-critical** in the same way the eight
 * character numbers are: they are data welded into code, and no asset file
 * carries them.
 */
long mk3_remap(long ani)
{
    switch (ani) {
    case 0x03f5: case 0x03f8: case 0x03fc: case 0x03fe: case 0x0400:
    case 0x0402: case 0x0404: case 0x119f: case 0x11a1: case 0x11a3:
    case 0x11a4: case 0x11a5: case 0x11a6: case 0x11a7: case 0x11a8:
    case 0x11a9: case 0x17a3: case 0x17a5:
        return 0x1aaf;

    case 0x0a94: case 0x0a95: case 0x0a96: case 0x0a97: case 0x0a98:
    case 0x0a99: case 0x0a9b:
        return 0x1ab0;

    case 0x1331: case 0x1332: case 0x1333: case 0x1334: case 0x1335:
    case 0x1336: case 0x14d5: case 0x14d7: case 0x14d8: case 0x14d9:
        return 0x1ab1;

    case 0x09d2: case 0x09d3: case 0x09d4: case 0x09d5: case 0x09d6:
    case 0x09d7: case 0x09d8: case 0x09d9: case 0x09da: case 0x0c23:
    case 0x0c24: case 0x11df: case 0x11e0: case 0x11e1: case 0x11e3:
    case 0x11e5: case 0x11e7: case 0x11e8: case 0x11e9: case 0x11eb:
    case 0x11ed: case 0x11fd: case 0x11fe: case 0x11ff: case 0x1200:
    case 0x1201:
        return 0x1ab2;

    case 0x12a4:
        return 0x1ab3;

    case 0x00e6: case 0x00e8: case 0x00eb: case 0x00ef: case 0x00f1:
    case 0x00f3: case 0x00f5:
        return 0x1ab5;
    default:
        return ani;
    }
}


/* -------------------------------------------------------------- mk3_init_game
 *
 * armv7 0x00031f30, 108 bytes.  **Complete.**  Called from `GameInit_LoadABit`.
 *
 *      H[0] .. H[8] = 0                         ; nine words, all of H
 *
 *      RoundParam[0x00] = -550                  ; left player limit
 *      RoundParam[0x04] =  950                  ; right player limit
 *      RoundParam[0x08] =  0
 *      RoundParam[0x0c] =  5
 *      RoundParam[0x10] =  1
 *      RoundParam[0x14] =  0
 *      RoundParam[0x18] = -1                    ; a BYTE
 *      RoundParam[0x24] =  0
 *      RoundParam[0x28] =  0
 *      RoundParam[0x2c] =  0
 *      RoundParam[0x30] =  0                    ; a BYTE
 *      RoundParam[0x34] =  0
 *      RoundParam[0x38] =  0
 *      RoundParam[0x3c] =  0
 *      RoundParam[0x40] =  0
 *
 * **Both blocks are cleared exactly to their symbol-table size.** `_H` is 36
 * bytes and nine words are written; `_RoundParam` is 68 bytes and the last
 * store lands at 0x40. Neither is a prefix of a bigger struct -- this routine
 * initialises the whole of both, and anything found at a higher offset in
 * either is a misreading.
 *
 * **0x1c and 0x20 are the exception and they are skipped on purpose.** Every
 * other word from 0 to 0x40 is written; those two are left as they were. The
 * three bytes at 0x18, 0x19 and 0x1a are a group GameCode.c already documents,
 * and only the first of them is set here.
 *
 * ## -550 and 950 are the default arena
 *
 * `RoundParam[0]` and `RoundParam[1]` are the level's left and right player
 * limits -- GameCode.c's `SetupLevelLimits` overwrites both from
 * `Level_Info[lvl]` and derives `G[0xb0]`, `G[0xb4]` and the two camera limits
 * from them. So these two numbers are **the arena before any level has been
 * chosen**, and through `gravity_n_bounds` above they put the walls at -492 and
 * 902 until a level says otherwise.
 *
 * 1500 wide. A port that boots straight into a fight without loading level data
 * gets a working arena out of this, which is exactly what a test scene needs.
 *
 * ## The literal chain
 *
 * 5, then 1, then -1: `movs #5`, `subs #4`, `subs #2`, one register walked
 * along, and the zero for every other field is a second register held across
 * the whole routine. The shared-literal habit again -- so the three values must
 * be transcribed as the chain and not as three independent constants.
 */
/* `H` is already declared in mk3logic.h as `char *`, with its own note. Nine
 * words is its whole 36-byte extent, so this writes through that spelling. */
extern long *RoundParam;                   /* slot 0x00165670 -> 0x0038ed04 */

void mk3_init_game(void)
{
    char *rp = (char *)(void *)RoundParam;
    long  v;

    *(long *)(void *)(H + 0) = 0;
    *(long *)(void *)(H + 4) = 0;
    *(long *)(void *)(H + 8) = 0;
    *(long *)(void *)(H + 12) = 0;
    *(long *)(void *)(H + 16) = 0;
    *(long *)(void *)(H + 20) = 0;
    *(long *)(void *)(H + 24) = 0;
    *(long *)(void *)(H + 28) = 0;
    *(long *)(void *)(H + 32) = 0;

    *(long *)(void *)(rp + 0x00) = -550;   /* default leftPlayerLimit  */
    *(long *)(void *)(rp + 0x04) =  0x3b6; /* default rightPlayerLimit */
    *(long *)(void *)(rp + 0x08) =  0;

    v = 5;
    *(long *)(void *)(rp + 0x0c) = v;
    v -= 4;                                /* 1 */
    *(long *)(void *)(rp + 0x10) = v;
    *(long *)(void *)(rp + 0x14) = 0;
    v -= 2;                                /* -1 */
    *(long *)(void *)(rp + 0x24) = 0;
    *(signed char *)(void *)(rp + 0x18) = (signed char)v;

    *(long *)(void *)(rp + 0x28) = 0;
    *(long *)(void *)(rp + 0x2c) = 0;
    *(signed char *)(void *)(rp + 0x30) = 0;
    *(long *)(void *)(rp + 0x34) = 0;
    *(long *)(void *)(rp + 0x38) = 0;
    *(long *)(void *)(rp + 0x3c) = 0;
    *(long *)(void *)(rp + 0x40) = 0;

    /* 0x1c and 0x20 are deliberately untouched. */
}


/* ------------------------------------------------------------------- mk3_init
 *
 * armv7 0x00031f9c, 644 bytes.  **Complete.**  Called from `GameInit_LoadABit`,
 * `TrainingMessages` and `DrawHUD`.
 *
 *      memset(G,     0, 0x478)                  ; 1144 -- ALL of GAMESTATE
 *      memset(Plyr,  0, 0x0ca8)                 ; 30 x 108
 *      memset(GrObj, 0, 0x08e8)                 ; 30 x  76
 *      memset(Pp,    0, 0x1068)                 ; 30 x 140
 *      memset(mo,    0, 0x01e0)                 ; 480 -- all of it
 *      memset(mytc,  0, 0x1f68)                 ; 30 x 268
 *
 *      MKEvent_Clear()
 *      G[0x474] = (random32() >> 9) & 1
 *      TList_Init()
 *
 *      for (n = 0; n < 30; n++) {               ; the wiring loop
 *          Plyr[n].field00 = &Pp[n]
 *          Plyr[n].thread  = &mytc[n]
 *          Plyr[n].field08 = &GrObj[n]
 *      }
 *
 *      mk3_getbbox_cb = cb
 *      init_players(p1 & 0x7f, p2 & 0x7f)
 *
 *      start mytc[0] at (p1 & 0x80) ? t_drone_begin : plyrthread, proc &Plyr[0]
 *      start mytc[1] at (p2 & 0x80) ? t_drone_begin : plyrthread, proc &Plyr[1]
 *
 *      Plyr[0].thread = &mytc[0]                ; again, AFTER init_players
 *      Plyr[1].thread = &mytc[1]
 *
 *      Playback_Init(&Playback[0])
 *      Playback_Init((char *)Playback + 0x10)
 *      G[0xa8] = 0
 *      mo[0x10] = 0;  mo[0] = mo + 0x10
 *
 *      t = TList_GetFront()                     ; a THIRD thread, at the front
 *      if (t) {
 *          t->frame = t->fieldf8 = t->fieldfc = t->field08 = 0
 *          t->proc = &Plyr[t->player]
 *          t->func = t_one_on_one
 *          GrObj[t->player].0x2c = -1
 *      }
 *      return 0
 *
 * ## The wiring loop is the answer to what the four arrays are
 *
 * Thirty iterations, three stores each, and it says outright what `mk3_dizzy`
 * and `DisplayUpdate` only implied:
 *
 *      Plyr[n].field00 -> Pp[n]          the proc
 *      Plyr[n].thread  -> mytc[n]        the thread
 *      Plyr[n].field08 -> GrObj[n]       the position and velocity
 *
 * **`MK3OBJ.field08` -- "another object", as mk3logic.h has called it for
 * twenty files -- is the GrObj with the same index.** Every `obj->field08->0x0e`
 * in this directory is a position, every `->0x18` a velocity, and the "part"
 * this project has been reading around is one of thirty physics records.
 *
 * The four strides appear one more time in the loop increments -- 0x6c, 0x8c,
 * 0x4c, 0x10c -- all four in six instructions.
 *
 * ## GAMESTATE is 1144 bytes, and that is its size
 *
 * `memset(G, 0, 0x478)`. The highest offset anything is known to read is
 * `G + 0x474`, which is the last word: 0x474 + 4 = 0x478 exactly. So **G has no
 * fields past 0x474** and any future reading that claims one is wrong.
 *
 * ## Who starts in front is a coin flip
 *
 * `G[0x474] = (random32() >> 9) & 1` -- one bit out of the generator, bit 9.
 * That word is what `mk3_who_in_front` returns, and `RenderLevelPlayers` is its
 * only caller. So **the draw order of the two fighters is randomised once per
 * init and never changes during the fight.**
 *
 * Bit 9 specifically, not bit 0. Worth keeping: a port with a different
 * generator that takes the low bit will get a different sequence of matches, and
 * a recorded demo will desynchronise on frame one.
 *
 * ## Bit 0x80 of a character number is the CPU flag
 *
 * Each fighter's thread is started at one of two routines on the high bit of its
 * character argument:
 *
 *      p & 0x80 set    -> t_drone_begin     mkdrone.c, the AI
 *      p & 0x80 clear  -> plyrthread        joy.c, a human
 *
 * and `init_players` gets `p & 0x7f`, so **the character number is seven bits
 * and the eighth says who is driving.** That is the switch a playable scene
 * needs: `mk3_init(char, char, cb)` with both high bits clear is two
 * human-controlled fighters, and it does not need `no_ai_hack` or any other
 * patch to get there.
 *
 * Six fields are set on each thread by hand rather than through
 * `StartThreadAt`: 0x04 (the function), and 0x08, 0xa4, 0xf8, 0xfc and 0x108
 * cleared or pointed at the fighter. That is `StartThreadAt`'s job written out
 * inline, which is why this routine does not call it.
 *
 * ## `init_players` clobbers the thread pointers
 *
 * `Plyr[0].thread` and `Plyr[1].thread` are written by the wiring loop and then
 * **written again, with the same values, after `init_players` returns.** A
 * compiler does not emit a redundant store to a global it cannot prove
 * unchanged -- and `init_players` is a call, so it cannot. The re-store is
 * therefore deliberate: **`init_players` damages those two pointers and this
 * repairs them.** Recorded here because it is the kind of thing a port
 * "cleans up" and then spends a day debugging.
 *
 * ## The third thread, and why it is taken from the FRONT
 *
 * `TList_GetFront` -- the only call to it anywhere -- puts a thread at the head
 * of `TList`, in front of both fighters, and starts it at `t_one_on_one`. So the
 * match referee runs BEFORE either fighter every frame.
 *
 * **And this confirms the prediction made while reading `DisplayUpdate`.** That
 * loop walks `mytc[1].next`, so a thread linked at the head is never reached and
 * never has its velocity integrated. `t_one_on_one` is exactly that: a thread
 * with a `Plyr` slot and a `GrObj` slot that does not move. The two allocators
 * differ in execution order AND in whether the object gets physics, and one
 * caller uses each.
 *
 * `GrObj[t->player].0x2c = -1` is the one field it does set, through the same
 * 76-byte multiply spelled out again.
 *
 * If the free list were empty the referee would silently not exist -- and
 * `TList_GetFront` returns NULL without complaint. It cannot happen here,
 * because `TList_Init` has just run and there are twenty-eight free.
 *
 * The return value is a literal 0 and no caller is known to read it.
 *
 * ## A fourth argument that is never read
 *
 * `r3` is not touched anywhere in the 644 bytes -- but **both callers pass
 * one.** GameCode.c's step 52 and training.c both call
 * `mk3_init(P1, P2, FrameID_GetBBox, 1)`, so the prototype is four wide and the
 * last parameter is dead. It is declared here rather than dropped, because a
 * definition that disagrees with its callers on argument count is exactly what
 * `tools/protos.py` exists to catch, and dropping it would just move the lie.
 *
 * ## GameCode.c found the CPU flag independently, and agrees
 *
 * That same note in GameCode.c was written from the CALL side, long before this
 * function was read:
 *
 *      AIOn == 0   mk3_init(P1,        P2,        cb, 1)
 *      AIOn == 1   mk3_init(P1,        P2 | 0x80, cb, 1)
 *      AIOn == 2   mk3_init(P1 | 0x80, P2 | 0x80, cb, 1)
 *
 * -- "Human vs Human", "Human vs CPU", "CPU vs CPU". **Two independent readings
 * of bit 7, from opposite ends of the same call, saying the same thing.**
 */
void  *memset(void *s, int c, size_t n);
uint32_t random32(void);
void  init_players(uint32_t a, uint32_t b);
void  Playback_Init(void *p);
long  plyrthread(MK3THREAD *thread);       /* slot 0x000f3870, joy.c */
long  t_drone_begin(MK3THREAD *thread);    /* slot 0x000f3860, mkdrone.c */
long  t_one_on_one(MK3THREAD *thread);     /* slot 0x000f386c, other.c */

extern char *mo;                           /* slot 0x00165680 -> 0x0038ed5c */
extern char *Playback;                     /* slot 0x00165660 -> 0x0038cfd4 */

#define MK3_SLOTS      30
#define MK3_CPU_FLAG   0x80               /* bit 7 of a character number */

long mk3_init(long p1, long p2, void (*bbox_cb)(void), long unused)
{
    (void)unused;                          /* r3 is never read; see the note */
    MK3THREAD *t;
    long       n;

    memset(G_BYTES, 0, 0x478);             /* all of GAMESTATE */
    memset(Plyr,    0, (size_t)MK3_SLOTS * PLYR_STRIDE);
    memset(GrObj,   0, (size_t)MK3_SLOTS * GROBJ_STRIDE);
    memset(Pp,      0, (size_t)MK3_SLOTS * PP_STRIDE);
    memset(mo,      0, 0x1e0);
    memset(mytc,    0, (size_t)MK3_SLOTS * MK3THREAD_STRIDE);

    MKEvent_Clear();

    /* bit 9 of the generator, and nothing else, decides the draw order */
    *(uint32_t *)(void *)(G_BYTES + 0x474) = (random32() >> 9) & 1u;

    TList_Init();

    /* the four parallel arrays, wired together thirty times */
    for (n = 0; n < MK3_SLOTS; n++) {
        char *plyr = Plyr + n * PLYR_STRIDE;
        *(void *P32 *)(void *)(plyr + 0x00) = Pp    + n * PP_STRIDE;
        *(void *P32 *)(void *)(plyr + 0x04) = &mytc[n];
        *(void *P32 *)(void *)(plyr + 0x08) = GrObj + n * GROBJ_STRIDE;
    }

    mk3_getbbox_cb = (void (*)(long, int *, int *, int *, int *))
                     (uintptr_t)bbox_cb;

    init_players((uint32_t)(p1 & 0x7f), (uint32_t)(p2 & 0x7f));

    for (n = 0; n < 2; n++) {
        long who = (n == 0) ? p1 : p2;
        MK3THREADFUNC f = (who & MK3_CPU_FLAG)
                            ? (MK3THREADFUNC)t_drone_begin
                            : (MK3THREADFUNC)plyrthread;

        mytc[n].field08 = 0;
        mytc[n].frame   = 0;
        mytc[n].fieldf8 = 0;
        mytc[n].fieldfc = 0;
        mytc[n].proc    = Plyr + n * PLYR_STRIDE;
        mytc[n].func    = f;
    }

    /* again, and on purpose: init_players damages these two. */
    *(void *P32 *)(void *)(Plyr + 0x04)               = &mytc[0];
    *(void *P32 *)(void *)(Plyr + PLYR_STRIDE + 0x04) = &mytc[1];

    Playback_Init(Playback);
    Playback_Init(Playback + 0x10);

    *(uint32_t *)(void *)(G_BYTES + 0xa8) = 0;

    *(uint32_t *)(void *)(mo + 0x10) = 0;
    *(void *P32 *)(void *)mo             = mo + 0x10;

    /* the referee: at the FRONT, so it runs first and gets no physics */
    t = TList_GetFront();
    if (t != NULL) {
        long who = (long)t->player;

        t->field08 = 0;
        t->frame   = 0;
        t->fieldf8 = 0;
        t->fieldfc = 0;
        t->proc    = Plyr + who * PLYR_STRIDE;
        t->func    = (MK3THREADFUNC)t_one_on_one;

        *(int32_t *)(void *)(GrObj + who * GROBJ_STRIDE + 0x2c) = -1;
    }

    return 0;
}


/* ----------------------------------------------------------------- mk3_update
 *
 * armv7 0x000322c0, 988 bytes.  **Complete.**  Called from
 * `UpdateArcadeCode(int *, int *)`, once, and from nowhere else.
 *
 * **This is the frame.** Everything in this directory happens because this
 * function calls it, in this order, once per tick.
 *
 *      jb[0] = joy[0];  jb[1] = joy[1]          ; a local copy, rewritten below
 *
 *      if (RoundParam[0x34]) for (i = 0; i < 2; i++) {
 *          if (Playback[i].w0) {                ; a sequence is running
 *              Pp[i].0x7e = 1
 *              Playback_Update(&Playback[i])
 *              jb[i] = (int16_t)Playback[i].0x0c
 *          } else if (jb[i] & 0x400) {          ; a sequence is requested
 *              seq = seq_lookup(GrObj[i].0x24, jb[i] & 0x3ff, Pp[i].0x7c)
 *              jb[i] = 0
 *              Playback_Begin(&Playback[i], seq, him_x < my_x)
 *              Pp[i].0x7e = 1
 *              Playback_Update(&Playback[i])
 *              jb[i] = (int16_t)Playback[i].0x0c
 *          } else Pp[i].0x7e = 0
 *      }
 *
 *      G[0xa8]++                                ; the switch counter
 *      G[0xac] = RoundParam[2] + 0xf7           ; THE FLOOR
 *      mk3_getbbox(-1, &G[0x468], &G[0x464], &G[0x470], &G[0x46c])
 *      blood[0] = blood[1] = -1
 *      MKEvent_Clear()
 *      RaiseTurboBars()
 *      TranslateJoybits(jb)
 *      swscan()
 *      UnstackSwitches()
 *
 *      for (i = 0; i < 2; i++)
 *          if (jb[i] & 0x400) { DoSpecial(&Plyr[i]); jb[i] = 0; }
 *
 *      G[0x00] = jb[0];  G[0x04] = jb[1]
 *
 *      for (t = TList; t; t = t->next) {        ; THE THREAD LOOP
 *          if (t->fieldfc > 0) t->fieldfc--
 *          if (t->fieldfc != 0) continue        ; still asleep
 *          for (;;) {
 *              r = mk3_frame(t, t->frame)[1](t)
 *              if (r <  0) return 1             ; abort the whole frame
 *              if (r == 0) continue             ; run this thread again NOW
 *              break
 *          }
 *          if (r == 0x16462) {                  ; the thread is finished
 *              if (Pp[0].0x64 == t->proc) { Pp[0].0x64 = 0; Pp[0].0x68 = 0; }
 *              else if (Pp[1].0x64 == t->proc) { Pp[1].0x64 = 0; Pp[1].0x68 = 0; }
 *              TList_Release(t)
 *          }
 *      }
 *
 *      DisplayUpdate()
 *
 *      n = 0
 *      for (t = TList; t; t = t->next) {        ; THE DISPLAY LIST
 *          g = &GrObj[t->player]
 *          if ((int32_t)g->0x2c < 0) continue   ; invisible
 *          if (n) mo[n-1].link = &mo[n]
 *          mo[n].link = NULL
 *          mo[n].0x0e = (g->0x30 & 0x20) ? 0x4e20 : g->0x2c
 *          mo[n].0x08 = mk3_remap(mo[n].0x0e)   ; through the scratch slot
 *          mo[n].0x0e = g->0x2c                 ; and overwritten
 *          mo[n].0x04 = g->0x0e                 ; x
 *          mo[n].0x06 = g->0x12                 ; y
 *          mo[n].0x0a = (g->0x28 & 0x10)
 *                     | ((g->0x30 & 0x80) ? 0x20 : 0)
 *                     |  g->0x44
 *                     | ((g->0x30 & 0x01) ? 0x40 : 0)
 *                     | ((g->0x48)        ? 0x100 : 0)
 *                     | ((Pp[t->player].field08) ? 0x80 : 0)
 *          mo[n].0x0c = (int8_t)g->0x24
 *          mo[n].0x0d = t->player * 2 + (uint8_t)Pp[t->player].field08
 *          n++
 *      }
 *      *out = mo
 *      return 0
 *
 * ## 0x16462 is not a duration. It is the termination sentinel.
 *
 * This project has recorded 0x16462 at eleven sites as "the park-and-never-wake
 * duration", because eleven handlers end with
 *
 *      thread->fieldfc = 0x16462;  return 0x16462;
 *
 * and the only thing known about it was that `fieldfc` counts down. **That
 * reading was incomplete, and this function completes it.** The comparison here
 * is against the RETURN VALUE, and a handler that returns 0x16462 has its
 * thread taken off `TList` and pushed back on the free list before `fieldfc` is
 * ever looked at again.
 *
 * So those eleven lines mean **"I am done, delete me"**, and the `fieldfc`
 * store is belt-and-braces that never matters. The number is a sentinel chosen
 * to be far outside any real sleep count, not a duration at all.
 *
 * The notes at those eleven sites are wrong in the same way and have been
 * corrected in place rather than deleted -- an incomplete reading that a later
 * function finishes is worth leaving visible.
 *
 * ## Releasing a thread cleans up the slave trio
 *
 * Before `TList_Release`, both fighters' procs are checked: if `Pp[i].0x64` is
 * this thread's proc, **0x64 and 0x68 are both cleared.** Those are the two
 * fields the slave trio uses -- `proc->field64` is the object and
 * `proc->slave` (0x68) is its part -- so `delete_slave`'s bookkeeping has a
 * central safety net here. A projectile that dies without tidying up does not
 * leave its owner pointing at a freed thread.
 *
 * Only the two fighters are checked. A slave owned by anything else is not
 * cleaned up, and nothing looks for one.
 *
 * ## A handler that returns 0 runs again immediately
 *
 * `if (r == 0) continue` loops back to the SAME thread, re-reading
 * `t->frame` and calling whatever handler is there now. That is how the descend
 * idiom advances within one frame: a state that pushes a level returns 0, and
 * the handler it pushed runs before the tick moves on. **Zero means "I changed
 * what I am; ask me again", not "I am done for this frame".**
 *
 * A handler that returns 0 without changing anything hangs the game in this
 * loop with no diagnostic.
 *
 * ## A negative return aborts the entire frame
 *
 * `return 1` -- immediately, skipping `DisplayUpdate`, the display list, and
 * `*out`. So the caller gets a stale `*out` and a 1. **-3 is the refusal every
 * dispatcher in this directory returns for an unknown token**, which means an
 * unhandled token does not misbehave quietly: it stops the frame dead.
 *
 * That is worth knowing for a port. `tools/instck.py` exists because 116
 * handlers were once written to refuse where the binary installs, and this is
 * what that would have looked like at run time.
 *
 * The normal return is `(uint32_t)fp >> 31` on a value that can only be zero or
 * positive by then, so **the normal path always returns 0** and the shift is a
 * compiler artefact of merging the two exits.
 *
 * ## The floor and the camera box
 *
 *      G[0xac]  = RoundParam[2] + 0xf7
 *
 * `G + 0xac` is the floor, read by ten routines across this directory, and this
 * is the one place it is written. `RoundParam[2]` is the level's ground plane --
 * Blood.c sets it to 0x12c or 0x190 -- so **the floor is the level's ground
 * plus 247**, recomputed every single frame.
 *
 *      mk3_getbbox(-1, &G[0x468], &G[0x464], &G[0x470], &G[0x46c])
 *
 * **Animation -1 means "the camera".** `G + 0x468` and `G + 0x470` are the
 * camera's left and right edges, already known from four other sites, and they
 * arrive here as the first and third outputs -- confirming the left/right
 * assignment and adding 0x464 as the top and 0x46c as the bottom. The camera
 * box is a bounding-box query with a reserved id, through the same callback
 * that measures animations.
 *
 * ## Bit 0x400 of an input word is a special-move request
 *
 * It is consumed twice, and `TranslateJoybits`' note above has been corrected
 * from this:
 *
 *   - **Here, before translation**, when `RoundParam[0x34]` is set: the low ten
 *     bits name a sequence, `seq_lookup` turns them into one, and `Playback`
 *     then synthesises an ordinary stick word every frame. **The special move
 *     is played back as if a human had done the motion.** The facing argument
 *     is `opponent_x < my_x` -- one signed compare on the two 0x0e coordinates.
 *   - **After translation**, for any word that still has the bit: `DoSpecial`
 *     is called on that fighter and the word is zeroed.
 *
 * So it is not a demo-playback flag, which is what the hypothesis in
 * `TranslateJoybits` guessed before this function was read. **The guess was
 * wrong and has been replaced there.** The `Playback` global it pointed at is
 * real and is used -- for this, not for demos.
 *
 * ## The display list
 *
 * `_mo` is 480 bytes and entries are indexed `n << 4`: **thirty records of
 * sixteen bytes**, one per slot, exactly like the other four arrays.
 *
 * Each entry is a linked node -- `mo[n-1].link = &mo[n]`, head at `mo[0]` --
 * built fresh every frame from whatever is on `TList` and visible. **A negative
 * `GrObj.0x2c` means invisible** and skips the object entirely, which is why
 * `mk3_init` sets exactly that field to -1 on the referee thread: the match
 * logic has a slot and is never drawn.
 *
 * `mo[n].0x0e` is used as a **scratch slot**: the animation number (or 0x4e20
 * when `GrObj.0x30` bit 5 is set) is parked there, read back, passed through
 * `mk3_remap`, and the result stored at 0x08 -- then 0x0e is overwritten with
 * the real value. So **`mk3_remap`'s subject is an animation number**, which
 * the note there had to leave open, and 0x4e20 is the substitute for a whole
 * class of object.
 *
 * `mo[n].0x0a` accumulates six flags out of four different `GrObj` fields, each
 * ORed in and stored back separately -- so a debugger watching that halfword
 * sees it built up in six steps. 0x80 comes from `Pp[slot].field08`, the
 * strength index, which is the same field `buttons_in_a2` uses to choose
 * between the two button masks. **`field08` is which side you are on**, and it
 * reaches the renderer as a flag and again as the low bit of `mo[n].0x0d`.
 *
 * ## For a playable scene
 *
 * `UpdateArcadeCode` passes a two-word input array and takes back a display
 * list. That is the whole interface between the fight and everything else:
 * **two words in, a linked list of sixteen-byte draw records out.** A port that
 * can fill `joy[2]` from a keyboard and draw from `mo` has a game, and needs to
 * understand nothing else in this directory to get there.
 */
void  Playback_Update(void *p);
void  Playback_Begin(void *p, long seq, long flip);
long  seq_lookup(long a, long b, long c);
void  RaiseTurboBars(void);
void  swscan(void);
void  UnstackSwitches(void);
void  DoSpecial(MK3OBJ *obj);

/* MK3_THREAD_DONE is in mk3logic.h, with the note that corrects the eleven
 * sites which called it a duration. */

long mk3_update(const long *joy, void **out)
{
    long       jb[2];
    MK3THREAD *t;
    long       i, n, r;

    jb[0] = joy[0];
    jb[1] = joy[1];

    if (RoundParam[0x34 / 4] != 0) {
        for (i = 0; i < 2; i++) {
            char *pb   = Playback + i * 0x10;
            char *pp   = Pp       + i * PP_STRIDE;
            char *mine = GrObj    + i * GROBJ_STRIDE;
            char *his  = GrObj    + (1 - i) * GROBJ_STRIDE;

            if (*(long *)(void *)pb == 0) {
                if ((jb[i] & 0x400) == 0) {
                    *(uint16_t *)(void *)(pp + 0x7e) = 0;
                    continue;
                }
                {
                    long seq = seq_lookup(*(long *)(void *)(mine + 0x24),
                                          jb[i] & 0x3ff,
                                          *(int16_t *)(void *)(pp + 0x7c));
                    long my_x  = *(int16_t *)(void *)(mine + 0x0e);
                    long his_x = *(int16_t *)(void *)(his  + 0x0e);

                    jb[i] = 0;             /* the request word is consumed */
                    Playback_Begin(pb, seq, (his_x >= my_x) ? 0 : 1);

                    /* the binary has this update twice: one physical call
                     * here and one for the already-running case below */
                    *(uint16_t *)(void *)(pp + 0x7e) = 1;
                    Playback_Update(pb);
                    jb[i] = *(int16_t *)(void *)(pb + 0x0c);
                    continue;
                }
            }

            *(uint16_t *)(void *)(pp + 0x7e) = 1;
            Playback_Update(pb);
            jb[i] = *(int16_t *)(void *)(pb + 0x0c);
        }
    }

    *(uint32_t *)(void *)(G_BYTES + 0xa8) += 1;
    *(long *)(void *)(G_BYTES + 0xac) = RoundParam[2] + 0xf7;   /* the floor */

    /* animation -1 is the camera; 0x468/0x470 are its left and right */
    mk3_getbbox(-1,
                (int *)(void *)(G_BYTES + 0x468),
                (int *)(void *)(G_BYTES + 0x464),
                (int *)(void *)(G_BYTES + 0x470),
                (int *)(void *)(G_BYTES + 0x46c));

    blood[1] = -1;
    blood[0] = -1;

    MKEvent_Clear();
    RaiseTurboBars();
    TranslateJoybits(jb);
    swscan();
    UnstackSwitches();

    for (i = 0; i < 2; i++) {
        if ((jb[i] & 0x400) != 0) {
            DoSpecial((MK3OBJ *)(void *)(Plyr + i * PLYR_STRIDE));
            jb[i] = 0;
        }
    }

    *(long *)(void *)(G_BYTES + 0x00) = jb[0];
    *(long *)(void *)(G_BYTES + 0x04) = jb[1];

    /* ------------------------------------------------------ the thread loop */
    for (t = TList; t != NULL; ) {
        MK3THREAD *next;

        if (t->fieldfc > 0)
            t->fieldfc = t->fieldfc - 1;

        if (t->fieldfc != 0) {             /* still asleep */
            t = t->next;
            continue;
        }

        do {
            MK3THREADLONGFUNC f =
                (MK3THREADLONGFUNC)(uintptr_t)mk3_frame(t, t->frame)[1];
            r = f(t);
            if (r < 0) {
                /* Port diagnostic, not in the binary: which handler refused
                 * (an unknown token) and stopped the tick. */
                printf("mk3: tick abandoned, handler %p refused token 0x%x "
                       "(level %u)\n", (void *)(uintptr_t)f,
                       (unsigned)*mk3_frame(t, t->frame + 1),
                       (unsigned)t->frame);
                return 1;                  /* the whole frame is abandoned */
            }
        } while (r == 0);                  /* 0 means "ask me again, now" */

        next = t->next;

        if (r == MK3_THREAD_DONE) {
            char *p0 = *(char *P32 *)(void *)(Plyr + 0x00);
            char *p1 = *(char *P32 *)(void *)(Plyr + PLYR_STRIDE + 0x00);
            long  k;

            if (*(void *P32 *)(void *)(p0 + 0x64) == t->proc)
                k = 0;
            else if (*(void *P32 *)(void *)(p1 + 0x64) == t->proc)
                k = 1;
            else
                k = -1;

            if (k >= 0) {                       /* one store pair, indexed */
                char *pk = k ? p1 : p0;

                *(uint32_t *)(void *)(pk + 0x64) = 0;
                pk = *(char *P32 *)(void *)(Plyr + k * PLYR_STRIDE + 0x00);
                *(uint32_t *)(void *)(pk + 0x68) = 0;
            }
            TList_Release(t);
        }

        t = next;
    }

    DisplayUpdate();

    /* ------------------------------------------------------ the display list */
    n = 0;
    for (t = TList; t != NULL; t = t->next) {
        char *g = GrObj + t->player * GROBJ_STRIDE;
        char *e;
        long  flags;
        long  ani;

        if (*(int32_t *)(void *)(g + 0x2c) < 0)      /* invisible */
            continue;

        e = mo + n * 0x10;
        if (n > 0)
            *(void *P32 *)(void *)(e - 0x10) = e;        /* link the previous one */
        *(void *P32 *)(void *)e = NULL;

        /* 0x0e is a scratch slot: the number goes in, comes back out, and is
         * replaced by the real value four instructions later. */
        *(uint16_t *)(void *)(e + 0x0e) =
            (*(uint32_t *)(void *)(g + 0x30) & 0x20u)
                ? (uint16_t)0x4e20
                : (uint16_t)*(uint16_t *)(void *)(g + 0x2c);

        ani = mk3_remap(*(int16_t *)(void *)(e + 0x0e));
        *(uint16_t *)(void *)(e + 0x08) = (uint16_t)ani;

        *(uint16_t *)(void *)(e + 0x0e) = *(uint16_t *)(void *)(g + 0x2c);
        *(uint16_t *)(void *)(e + 0x04) = *(uint16_t *)(void *)(g + 0x0e);
        *(uint16_t *)(void *)(e + 0x06) = *(uint16_t *)(void *)(g + 0x12);

        flags = *(uint16_t *)(void *)(g + 0x28) & 0x10;
        *(uint16_t *)(void *)(e + 0x0a) = (uint16_t)flags;

        if (*(uint32_t *)(void *)(g + 0x30) & 0x80u)
            flags |= 0x20;
        *(uint16_t *)(void *)(e + 0x0a) = (uint16_t)flags;

        flags |= *(uint16_t *)(void *)(g + 0x44);
        *(uint16_t *)(void *)(e + 0x0a) = (uint16_t)flags;

        if (*(uint32_t *)(void *)(g + 0x30) & 1u)
            flags |= 0x40;
        *(uint16_t *)(void *)(e + 0x0a) = (uint16_t)flags;

        if (*(uint32_t *)(void *)(g + 0x48) != 0)
            flags |= 0x100;
        *(uint16_t *)(void *)(e + 0x0a) = (uint16_t)flags;

        if (*(uint32_t *)(void *)(Pp + t->player * PP_STRIDE + 0x08) != 0)
            flags |= 0x80;
        *(uint16_t *)(void *)(e + 0x0a) = (uint16_t)flags;

        *(int8_t *)(void *)(e + 0x0c) =
            (int8_t)*(int32_t *)(void *)(g + 0x24);

        /* through Plyr[n].field00 -- which mk3_init proved is &Pp[n] */
        {
            char *proc = *(char *P32 *)(void *)(Plyr + t->player * PLYR_STRIDE);
            *(uint8_t *)(void *)(e + 0x0d) =
                (uint8_t)(t->player * 2 + *(uint8_t *)(void *)(proc + 0x08));
        }

        n++;
    }

    *out = mo;
    return 0;
}
