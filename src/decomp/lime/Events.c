/*
 * lime/common/Events.cpp -- the runtime event manager.
 *
 * Recovered from the armv6 slice. Addresses below are armv6.
 *
 * The `.events` file format is documented in docs/EVENTS-FORMAT.md; this is
 * the code that plays those tracks back. The two meet at FindEventOffsets,
 * which walks tracks at the 216-byte in-memory stride the format doc derived
 * from the loader.
 *
 * ---------------------------------------------------------------------------
 * The event pool
 *
 * A fixed array, not a list: **192 slots of 248 bytes each**, 47,616 bytes
 * total. Every function here that touches it walks from a global base in
 * 0xF8-byte steps and stops after 0xBA00 bytes, and 0xC0 * 0xF8 == 0xBA00
 * exactly.
 *
 * That the pool is fixed matters for a port: there is no allocation on the
 * event path at all, and GetFreeEvent returning -1 is the only failure mode.
 *
 * Fields identified so far, as offsets into a slot:
 *
 *      +0x00   int   state -- 0 is free, > 0 is live, NEGATIVE is dying
 *                    (a killed event starts at -2 and is counted UP to zero
 *                     by LIME_UpdateEvents; see that function)
 *      +0x14   SCENEEVENTTRACK *track
 *      +0x3c   long  group id
 *      +0xa4   float (set together with +0xe4 when an event is killed)
 *      +0xe4   float
 */

#include <math.h>
#include <string.h>
#include <stdio.h>
#include "lime.h"

/* EVENT_SLOTS and EVENT_STRIDE now live in lime.h */


/* --------------------------------------------------- LIME_InitEventsManager
 *
 * armv6 0x000e801c, 40 bytes.
 *
 * Marks every slot free by zeroing its state word. Nothing else is cleared --
 * stale data in the other 244 bytes is simply never read, because a free slot
 * is always fully written before it goes live.
 */
void LIME_InitEventsManager(void)
{
    int i;
    for (i = 0; i < EVENT_SLOTS; i++)
        SceneEvents[i].state = 0;
}


/* ------------------------------------------------------- LIME_KillAllEvents
 *
 * armv6 0x000e8048, 12 bytes.
 *
 * A single unconditional branch to LIME_InitEventsManager. The two names exist
 * for readability at the call site; the compiler collapsed the call into a
 * tail jump, so they are the same function in the binary.
 */
void LIME_KillAllEvents(void)
{
    LIME_InitEventsManager();
}


/* ---------------------------------------------------- LIME_CountActiveEvents
 *
 * armv6 0x000e804c, 32 bytes.
 *
 * Counts slots whose state is non-zero. Note that includes the dying states,
 * so this is "not free" rather than "running" -- an event killed on the
 * previous frame is still counted here for two more updates.
 */
int LIME_CountActiveEvents(void)
{
    int i, n = 0;
    for (i = 0; i < EVENT_SLOTS; i++)
        if (SceneEvents[i].state != 0)
            n++;
    return n;
}


/* ------------------------------------------------------------ GetFreeEvent
 *
 * armv6 0x000e7dc0, 36 bytes.
 *
 * First free slot, or **-1** when the pool is full. The -1 is produced as
 * `0xC0 - 0xC1` once the loop runs off the end, which is the compiler's way of
 * folding the sentinel into the counter rather than branching.
 */
int GetFreeEvent(void)
{
    int i;
    for (i = 0; i < EVENT_SLOTS; i++)
        if (SceneEvents[i].state == 0)
            return i;
    return -1;
}


/* ------------------------------------------------------ CountEventsMatching
 *
 * armv6 0x000e7df8, 56 bytes.
 *
 * How many live events belong to a given track. The test is `state > 0`, so
 * unlike LIME_CountActiveEvents this one excludes killed slots.
 *
 * The matrix argument is accepted and never read.
 */
int CountEventsMatching(SCENEEVENTTRACK *track, limeMATRIX44 *unused)
{
    int i, n = 0;
    (void)unused;

    for (i = 0; i < EVENT_SLOTS; i++)
        if (SceneEvents[i].state > 0 && SceneEvents[i].track == track)
            n++;
    return n;
}


/* --------------------------------------------------- KillAlleventsWithGroup
 *
 * armv6 0x000e7e3c, 60 bytes. The doubled 'l' in "Allevents" is the original
 * symbol's, not a transcription slip.
 *
 * Kills every live event carrying a group id, by setting state to **-2** and
 * writing one constant into two float fields. Killing is a state change in
 * place; the slot itself is reclaimed later.
 *
 * **-2 specifically, not just "negative".** LIME_UpdateEvents counts negative
 * states up toward zero, one per frame, and zero is what free means. So -2 buys
 * a **two-frame grace period** before the slot can be handed out again -- long
 * enough for anything still drawing from it this frame to finish. A port that
 * frees on kill will reuse a slot mid-frame.
 *
 * Groups are what let a fatality cancel its own particle swarm without knowing
 * which slots it used.
 */
void KillAlleventsWithGroup(long group)
{
    int i;
    for (i = 0; i < EVENT_SLOTS; i++) {
        if (SceneEvents[i].state > 0 && SceneEvents[i].group == group) {
            SceneEvents[i].state = -2;
            SceneEvents[i].world[15] = EVENT_KILL_VALUE;    /* +0xa4 */
            SceneEvents[i].local[15] = EVENT_KILL_VALUE;    /* +0xe4 */
        }
    }
}


/* --------------------------------------------------------- IsWhirlwindScene
 *
 * armv6 0x000e8ad8, 24 bytes.
 *
 * ```c
 * return strstr(name, "WHIRLWIND.scene") != NULL;
 * ```
 *
 * The effect is identified by a **substring of its filename**, not by a flag
 * in the data. Worth recording because it is the kind of coupling that breaks
 * silently if a port ever renames or repacks assets: nothing declares this
 * dependency except the string literal at 0x001c17d8.
 *
 * The mangled name types the argument as `SCENEINFO *`, and the register is
 * handed to strstr untouched -- so **the scene's name string sits at offset 0
 * of SCENEINFO**, with no dereference needed. That is a small structural fact
 * this function gives away for free.
 */
int IsWhirlwindScene(SCENEINFO *scene)
{
    return strstr((const char *)scene, "WHIRLWIND.scene") != NULL;
}


/* --------------------------------------------------------- FindEventOffsets
 *
 * armv6 0x000e8430, 56 bytes.
 *
 * Resolves each track's string id into an index, once, at load time, so the
 * per-frame path never does a lookup.
 *
 * The walk steps **0xD8 = 216 bytes** per track, which independently confirms
 * the in-memory SCENEEVENTTRACK size that docs/EVENTS-FORMAT.md derived from
 * the loader -- 268 bytes on disk, 216 in memory. It also places two fields:
 * the name at **+0x80** and the resolved id at **+0xc0**.
 */
void FindEventOffsets(SCENEEVENTS *events)
{
    long i;

    for (i = 0; i < events->numTracks; i++) {
        char *track = (char *)events->tracks + i * 216;
        *(int *)(track + 0xc0) = FindIdInMasterOffsets(track + 0x80);
    }
}


/* ------------------------------------------------- FindIdInMasterOffsets
 *
 * armv6 0x000e83b8, 76 bytes.  __Z21FindIdInMasterOffsetsPc
 *
 * Linear search of the master offsets table for a named entry, returning its
 * index. The stride is **0x50 = 80 bytes** per record, and the count comes from
 * a global rather than the table.
 *
 * This is the lookup `FindEventOffsets` calls once per track at load time so
 * the per-frame path never compares a string. A linear scan is fine precisely
 * because it happens once.
 */
/* armv7 0x000a46f8: a name not in the table is ADDED, and its new row
 * returned -- the earlier body returned -1, which then indexed the table. */
int FindIdInMasterOffsets(const char *name)
{
    const char *row = MasterEventOffsets;
    int i;

    for (i = 0; i < NumMasterEventOffsets; i++, row += 0x50)
        if (strcmp(row, name) == 0)
            return i;
    return AddNewID(name);
}


/* ---------------------------------------------------------------- AddNewID
 *
 * armv6 0x000e8134, 168 bytes.
 *
 * Appends an entry to the master offsets table and bumps the count.
 *
 * Its first act is `LIME_printf(0x1d, ...)` -- and that call is what settles
 * `LIME_printf`'s signature: the first argument is a **debug window index**,
 * not the format string. Since `LIME_printf` is compiled away the call does
 * nothing in the retail binary, but the argument order is still visible here.
 */
/* armv7 0x000a452c: grows the table by one 80-byte row -- a new block,
 * the old rows copied in, the old block freed -- then writes the name and
 * zeroes x, y, z. Returns the new row's index. */
int AddNewID(const char *name)
{
    char *old = MasterEventOffsets;
    char *row;

    LIME_printf(0x1d, "", name);
    MasterEventOffsets = (char *)limeMalloc("newID",
                                            (NumMasterEventOffsets + 1) * 0x50);
    if (old != NULL) {
        memcpy(MasterEventOffsets, old, NumMasterEventOffsets * 0x50);
        limeFree(old);
    }
    row = MasterEventOffsets + NumMasterEventOffsets * 0x50;
    memcpy(row, name, 0x40);
    *(int32_t *)(row + 0x40) = 0;
    *(int32_t *)(row + 0x44) = 0;
    *(int32_t *)(row + 0x48) = 0;
    NumMasterEventOffsets++;
    LIME_printf(0x1d, "");
    return NumMasterEventOffsets - 1;
}


/* ------------------------------------------------------------- IsOnWWFrame
 *
 * armv6 0x000e7e8c, 160 bytes.  __Z11IsOnWWFrameP8Mk3Obj_t
 *
 * Tests whether a fight object is on one of the whirlwind animation frames.
 *
 * The frame number is a **uint16 at Mk3Obj_t+0x08**, sign-extended before the
 * comparison, and it is checked against a run of **consecutive** values --
 * `base`, `base+1`, `base+2`, `base+3`, `base+4` -- unrolled rather than
 * ranged. So the whirlwind occupies a contiguous block of frames, which is
 * consistent with how docs/FRAMELISTS.md describes clips: consecutive entries
 * sharing a stem.
 *
 * `Mk3Obj_t` is a `gamecode` type, so this function is one of the few places
 * `lime/common` reaches up into the fight engine rather than the other way
 * round.
 */
/* armv7 0x000a4358 -- the body above described an armv6 reading with a
 * `g_whirlwindFirstFrame` global; the armv7 code has no such global. The
 * frames are constants:
 *
 *      0xeb3..0xeb7                        any object
 *      and when the signed byte at +0x0c is 9 (one character):
 *      0x149..0x14c, 0x14e, 0x150,
 *      0x15ec, 0x15ee, 0x15f0, 0x15f2, 0x15f4, 0x15f7, 0x15fa, 0x15fd
 */
int IsOnWWFrame(Mk3Obj_t *obj)
{
    unsigned raw = obj->frame;                      /* ldrh [r0, #8] */
    int f = (int16_t)raw;                           /* sxth */

    if (f >= 0xeb3 && f <= 0xeb7)
        return 1;
    if (((const int8_t *)obj)[0x0c] != 9)           /* ldrsb [r2, #0xc] */
        return 0;
    if ((uint16_t)(raw - 0x149) <= 3 || f == 0x14e || f == 0x150)
        return 1;
    return f == 0x15ec || f == 0x15ee || f == 0x15f0 || f == 0x15f2 ||
           f == 0x15f4 || f == 0x15f7 || f == 0x15fa || f == 0x15fd;
}


/* ------------------------------------------------------ KillIllegalWhirlwinds
 *
 * armv6 0x000e7f74, 120 bytes.
 *
 * Cancels whirlwind events that should not be running.
 *
 * It gates on two globals both being **10** before doing anything, then walks
 * the same fixed event pool the rest of this file uses -- base + 0xF8 stepping
 * to base + 0xBA00, which is the 192 slots of 248 bytes described at the top.
 *
 * The name is the interesting part. A function called *Kill Illegal* exists
 * because something could leave whirlwinds alive that should not be, and rather
 * than fix the cause the engine sweeps for them. That is worth knowing before
 * reproducing the behaviour: the sweep is load-bearing, not defensive.
 */
/* armv7 0x000a43f8. The gate is PLAYER1MODEL or PLAYER2MODEL being 10
 * (both read through their non-lazy slots, 0xf3668 -> 0x14e1b4 and
 * 0xf30a4 -> 0x14e1b8); the earlier body dereferenced two pointers nothing
 * ever set, and crashed on the first fight frame.
 *
 * For each of the 192 SceneEvents slots: a live one (state > 0) that follows
 * an object (+0xf0 nonzero) whose object (+0xf4) is no longer on a whirlwind
 * frame is killed -- state = -2 (`mvn r3, #1`) and the two floats at +0xa4
 * and +0xe4 set to 100.0f (literal 0x42c80000 at 0xa446c). */
extern long PLAYER1MODEL, PLAYER2MODEL;

void KillIllegalWhirlwinds(void)
{
    int i;

    if (PLAYER1MODEL != 10 && PLAYER2MODEL != 10)
        return;

    for (i = 0; i < EVENT_SLOTS; i++) {
        char *ev = (char *)&SceneEvents[i];
        Mk3Obj_t *obj;

        if (*(int32_t *)ev <= 0)                     /* ble: free or dying */
            continue;
        if (*(int32_t *)(ev + 0xf0) == 0)
            continue;
        obj = (Mk3Obj_t *)(uintptr_t)*(uint32_t *)(ev + 0xf4);
        if (IsOnWWFrame(obj))
            continue;
        *(int32_t *)ev = -2;
        *(float *)(ev + 0xa4) = 100.0f;
        *(float *)(ev + 0xe4) = 100.0f;
    }
}


/* ------------------------------------------------------------ LIME_FreeEvents
 *
 * armv6 0x000e8080, 104 bytes.
 *
 * Releases a scene's event tracks. The walk steps **0xD8 = 216 bytes** per
 * track -- the in-memory SCENEEVENTTRACK size, confirmed here for the second
 * time after FindEventOffsets.
 *
 * It also places a field the format doc did not have: **each track carries a
 * `SCENEINFO *` at +0x04**, and it is passed through `LIME_SceneExists` before
 * anything is done with it. So a track holds a live reference to the scene its
 * events spawn, and the freeing path assumes that reference may already be
 * stale -- which is exactly the situation reference counting creates.
 */
void LIME_FreeEvents(SCENEEVENTS *events)
{
    long i;

    if (events->numTracks == 0)
        return;

    for (i = 0; i < events->numTracks; i++) {
        char *track = (char *)events->tracks + i * 216;
        SCENEINFO *scene = *(SCENEINFO *P32 *)(track + 4);

        if (LIME_SceneExists(scene) != NULL)
            LIME_FreeScene(scene);
    }
}


/* ------------------------------------------------ LIME_TriggerEventsFromScene
 *
 * armv6 0x000e90e4, 216 bytes.  **Structurally complete.**
 *
 * Fires whatever a scene's events say should happen on a given frame.
 *
 * **The frame number is taken modulo the scene's track count** -- an
 * `___modsi3` call against `SCENEINFO+0x44`, which docs/SCENE-FORMAT.md
 * identifies as `count2`, the number of animation track records each object
 * carries. That is how the event track loops: nothing resets a counter, the
 * index just wraps.
 *
 * A negative result is clamped to zero rather than wrapped, with
 * `bic r0, r0, r0, asr #31` -- the sign bit smeared and used as a mask, which
 * is the branchless way to write `if (x < 0) x = 0`.
 *
 * The events themselves come from **`SCENEINFO+0x84`**, the pointer the scene
 * loader fills from the matching `.events` file. So the chain the format work
 * described from the file side -- every scene owns one `.events` -- is the same
 * chain the runtime walks.
 *
 * **It takes eight arguments and reads four.** This was written with four,
 * which was wrong: `AnimateBG` sets up `[sp]`, `[sp+4]`, `[sp+8]` and
 * `[sp+0xc]` before both of its calls, so four more arrive past the registers.
 * The body never touches `r7`, so it never loads them -- but they are part of
 * the interface and every caller pushes them. Two call sites agree, which is
 * what makes this the signature rather than one caller's mistake.
 */
/* armv7 0x000a4fc0. Every key of every track that sits on this frame
 * (the frame taken modulo the scene's length, clamped to [0, count2]) fires
 * one event, placed by the key's DS-format matrix. Each prints a line to
 * stdout -- `printf`, not the compiled-away LIME_printf. */
void LIME_TriggerEventsFromScene(SCENEINFO *scene, int frame,
                                 limeMATRIX44 *m, long a4,
                                 long offsetId2, long a7,
                                 TEXTURE *tex0, TEXTURE *tex1)
{
    const EVENTSINFO *events;
    long f, i, k;

    if (scene == NULL)
        return;
    f = frame % scene->count2;
    if (f < 0)
        f = 0;                          /* bic r0, r0, r0, asr #31 */
    if (f > scene->count2)
        f = scene->count2;

    events = (const EVENTSINFO *)scene->events;
    for (i = 0; i < events->count; i++) {
        SCENEEVENTTRACK *track = (SCENEEVENTTRACK *)
            ((char *)events->tracks + i * SCENEEVENTTRACK_STRIDE);
        const char *t = (const char *)track;
        const char *key = *(char *P32 const *)(t + 0xd4);

        for (k = 0; k < *(const int32_t *)t; k++, key += 0x44) {
            float mtx[16];
            float spd;

            if (*(const int32_t *)key != f)
                continue;
            memcpy(&spd, t + 0xc4, 4);
            printf("     - EVENT %s (fr %d) triggered event %s "
                   "(FromGround %d, spd %02.02f...\n ",
                   scene->name, (int)f,
                   track->scene ? (const char *)track->scene->name : "(null)",
                   *(const int32_t *)(t + 0x78), (double)spd);
            ConvertDSMatrixtoPCMatrix((const int32_t *)(key + 8), mtx);
            LIME_TriggerEvent(track, m, (limeMATRIX44 *)mtx, a4, offsetId2,
                              a7, tex0, tex1, 0);
        }
    }
}


/* ----------------------------------------------------------- LIME_UpdateEvents
 *
 * armv6 0x000e9238, 452 bytes.  **Rewritten -- the first version was wrong.**
 *
 * Ticks every slot in the event pool once per frame.
 *
 * ## What the first version got wrong, and how it was caught
 *
 * An earlier pass read `beq #0xe937c` on the repeat counter as "nothing to do,
 * go to the next slot". It is the opposite: that branch is where an event whose
 * counters have run out **kills itself**. The code there is
 *
 *      mvn  r3, #1             ; -2
 *      str  r3, [r4]           ; state = -2
 *      str  r3, [r4, #0xa4]    ; the same two float fields
 *      str  r3, [r4, #0xe4]    ; KillAlleventsWithGroup writes
 *
 * -- the identical kill signature. So the version written first left finished
 * effects alive forever, and read the whole function inside out.
 *
 * It was caught by `tests/test_events_diff.c` on its first run: a live event
 * with no repeats left stayed at state 3 in the clean C while the original took
 * it to -2. Nothing about that is visible by reading; it took running both.
 *
 * ## The real shape
 *
 * Per slot, in order:
 *
 *  - **state 0** -- free, skip.
 *  - **state < 0** -- dying. Count UP toward zero and skip. That part of the
 *    first version was right: -2 buys a two-frame grace period before the slot
 *    is handed out again.
 *  - **a delay at +0x38** -- decremented, clamped at zero, and while it is still
 *    positive the slot is skipped entirely. A start delay.
 *  - **the frame cursor at +0x04** is truncated to an integer and stored at
 *    +0x08. If it differs from the previous frame at +0x0c, the scene's event
 *    tracks fire through `LIME_TriggerEventsFromScene`, which is handed the
 *    scene at +0x10, the track block at +0x68, and four more fields.
 *    **Events fire on a frame CHANGE, not once per tick** -- so a slowed or
 *    paused cursor does not re-trigger, and a port that fires per tick will
 *    emit duplicates at low speed.
 *  - **+0x0c is then set to +0x08**, remembering the frame just handled.
 *
 * ## Two counters, then death
 *
 * When the frame reaches `scene->count2 - 1` the event has run its length, and
 * three things can happen:
 *
 * | condition | what happens |
 * |---|---|
 * | `+0x2c` non-zero | rewind: cursor and both frame fields reset to `count2 - 1`; the counter decrements unless it is **-1**, which loops forever |
 * | `+0x2c` zero, `+0x30` non-zero | a second pass: cursor steps back by the frame count and `+0x30` decrements while positive |
 * | both zero | **state = -2** and the two float fields are written -- the event kills itself |
 *
 * Two independent counters rather than one is not what the first reading
 * assumed, and it is the difference between an effect that ends and one that
 * never does.
 *
 * ## The pool geometry, unchanged
 *
 * The walk steps `#0xf8` and ends at a sentinel of `base + 0xb900 + 8`, which
 * is `0xBA00` minus one slot -- 192 slots of 248 bytes, confirmed a third time
 * from the consumer side.
 *
 * **Indexed, not stepped by 0xf8.** `sizeof(EVENT)` is 256 on a 64-bit host
 * because an ARM pointer is 4 bytes and ours is 8. Walking a real C array with
 * the original's byte stride runs off the end -- it did, with a segfault. A
 * hard-coded stride is correct only for memory the engine treats as raw bytes.
 *
 * ## What is still not written out
 *
 * The four extra arguments passed to LIME_TriggerEventsFromScene (+0x40, +0x60,
 * +0x48, +0xe8, +0xec) are named by offset here rather than given meanings, and
 * the exact float arithmetic on the rewind path -- which mixes a subtraction of
 * the frame count with an addition of the step at +0x28 -- is described above
 * rather than transcribed. Both are visible in the disassembly and neither was
 * traced to a confident conclusion.
 */
/* armv7 0x000a5098, transcribed whole. Per live slot, once a frame:
 *
 *   - dying (state < 0): count up toward 0, the slot frees itself;
 *   - delayed: count +0x38 down, start on the tick it reaches 0;
 *   - the frame is the integer part of the cursor; on a NEW frame the
 *     event's scene fires its own events (nested effects);
 *   - before the last frame: advance the cursor by +0x28;
 *   - on the last frame: if +0x2c (repeat) is set, hold on the last frame
 *     and count it down (-1 holds forever); else if +0x30 (repeat2) is set,
 *     wrap back one length and count it down (-1 loops forever); else kill
 *     the event (state -2, +0xa4 and +0xe4 to 100.0f). */
void LIME_UpdateEvents(void)
{
    int i;

    for (i = 0; i < EVENT_SLOTS; i++) {
        EVENT *ev = &SceneEvents[i];
        long n, f;

        if (ev->state == 0)
            continue;
        if (ev->state < 0) {
            ev->state++;
            continue;
        }
        if (ev->delay != 0) {
            ev->delay--;
            if (ev->delay < 0)
                ev->delay = 0;
            else if (ev->delay != 0)
                continue;
        }

        f = (long)ev->cursor;                       /* vcvt.s32.f32 */
        ev->frameA = (int)f;
        if (f != ev->frameB)
            LIME_TriggerEventsFromScene(ev->scene, (int)f,
                                        (limeMATRIX44 *)ev->world,
                                        ev->field40, ev->offsetId2,
                                        ev->field48, ev->flushTexture,
                                        (TEXTURE *)(uintptr_t)ev->fieldEC);
        ev->frameB = ev->frameA;
        n = ev->scene->count2;

        if (ev->frameA < n - 1) {
            ev->cursor += ev->step;
        } else if (ev->repeat != 0) {
            ev->cursor = (float)n - 1.0f;
            ev->frameA = (int)(n - 1);
            ev->frameB = (int)(n - 1);
            if (ev->repeat != -1)
                ev->repeat--;
        } else if (ev->repeat2 == 0) {
            ev->state = -2;
            ev->world[15] = EVENT_KILL_VALUE;       /* +0xa4 */
            ev->local[15] = EVENT_KILL_VALUE;       /* +0xe4 */
            ev->cursor += ev->step;
        } else {
            ev->cursor -= (float)n;
            ev->frameA = (int)(ev->frameA - n);
            if (ev->repeat2 > 0)
                ev->repeat2--;
            ev->cursor += ev->step;
        }
    }
}


/* ------------------------------------------------------------ LIME_PlayFBXAtPos
 *
 * armv6 0x000e8ddc, 188 bytes.  **Complete.**
 *
 * Fires a one-off effect at a position, without a scene to drive it.
 *
 * The whole function is **a synthetic SCENEEVENTTRACK built in a static
 * buffer**, then handed to LIME_TriggerEventFromSceneH. Nothing is allocated
 * and nothing is freed -- the same scratch track is overwritten on every call,
 * which means **this is not re-entrant and cannot be called from two places in
 * one frame** without the second clobbering the first. In a single-threaded
 * frame loop that is fine, and it is the kind of shortcut that stops being fine
 * the moment a port adds a worker thread.
 *
 * "FBX" is the engine's own word for an effect: the `.events` files are full of
 * names like `smokeparticle fbx` and `bomblets fbx`.
 *
 * ## What a default track looks like
 *
 * This is the most useful thing here -- it is a documented set of neutral
 * values for every field that matters:
 *
 * ```
 *   +0x08  +0x0c  +0x10  +0x14   1.0f   (four scale or colour terms)
 *   +0xc4                        1.0f
 *   +0x1c  +0x20  +0x24          0      (a vector, zeroed)
 *   +0x70  +0x78  +0x7c          0
 *   +0xd0                        -1     <- no instance limit
 *   matrix                       identity, via limeMatrixLoadIdentity
 * ```
 *
 * **`+0xd0` set to -1 is the interesting one.** LIME_TriggerEventFromSceneH
 * reads that same field, compares it against `CountEventsMatching`, and refuses
 * to spawn when the count has reached it -- so `+0xd0` is a **cap on how many
 * copies of a track may run at once**, and -1 disables the cap. A one-shot
 * effect fired by hand should never be throttled, so PlayFBXAtPos opts out.
 *
 * Two independent functions, one field, consistent meaning. That is the
 * standard this project holds field identifications to, and it is met here.
 */
/* **The forwarding call was wrong, and this is what fixed it.** The argument
 * list below was written shifted: `NULL` stood where the CALLER'S MATRIX goes,
 * everything after it moved up one, and `t->scene` stood where the caller's
 * scene goes -- a field this function never writes. The binary is plain about
 * all of it:
 *
 *      mov   fp, r2                ; the scene, kept across the body
 *      ...
 *      movs  r3, #1                ; a6 is 1, not 0
 *      mov   r0, fp                ; scene := the third argument
 *      mov   r1, r4                ; track := the scratch track
 *      mov   r2, r6                ; m1    := the scratch matrix
 *      ldr   r3, [sp, #0x1c]       ; m2    := the FIRST argument
 *      str.w r8, [sp]              ; a4    := the second
 *      str   sl, [sp, #0xc]        ; a7    := the fourth
 *
 * Nothing stores to `[r4, #4]` anywhere in the body, so `t->scene` was never
 * set and the old call passed whatever the previous caller left there.
 *
 * The parameters are typed for what the call sites pass -- a matrix and a
 * scene, in GameCode.c and Blood.c -- rather than as the four words the
 * registers happen to hold. `tools/protos.py` is what put the two side by
 * side.
 */
void LIME_PlayFBXAtPos(limeMATRIX44 *m, long arg1, SCENEINFO *scene, long arg3)
{
    SCENEEVENTTRACK *t = &g_fbxScratchTrack;    /* static, reused every call */

    t->maxInstances = -1;               /* +0xd0 -- no cap */

    t->f08 = 1.0f;                      /* +0x08 */
    t->f0c = 1.0f;                      /* +0x0c */
    t->f10 = 1.0f;                      /* +0x10 */
    t->f14 = 1.0f;                      /* +0x14 */
    t->fc4 = 1.0f;                      /* +0xc4 */

    t->flag7c = 0;                      /* +0x7c */
    t->v1c = 0; t->v20 = 0; t->v24 = 0; /* +0x1c..+0x24 */
    t->f70 = 0;                         /* +0x70 */
    t->f78 = 0;                         /* +0x78 */

    limeMatrixLoadIdentity(g_fbxScratchMatrix);   /* limeMATRIX44 is float[16] */

    LIME_TriggerEventFromSceneH(scene, t, &g_fbxScratchMatrix, m,
                                arg1, 0, 1, arg3, NULL, NULL, 0);
}


/* ------------------------------------------------- LIME_TriggerEventFromSceneH
 *
 * armv6 0x000e8afc, 736 bytes.  **Structurally complete.**
 *
 * The spawn path every other trigger in this file funnels into.
 *
 * ## Two gates before a slot is used
 *
 * ```
 *      bl    GetFreeEvent
 *      cmn   r0, #1
 *      beq   <give up>              ; pool full -> silently do nothing
 *
 *      ldr   r3, [r6, #0xd0]        ; the track's instance cap
 *      cmn   r3, #1
 *      beq   <skip the check>       ; -1 means unlimited
 *      bl    CountEventsMatching
 *      cmp   r0, r3
 *      bge   <give up>              ; already at the cap
 * ```
 *
 * So a track carries **its own limit on simultaneous copies**, checked by
 * counting live events that match it rather than by a per-track counter. That
 * is O(pool) per spawn -- 192 slots -- which is cheap enough at these numbers
 * and, more importantly, cannot drift out of sync the way a counter can when
 * events are freed by the deferred countdown in LIME_UpdateEvents.
 *
 * **Both failures are silent.** A full pool and an exceeded cap both just
 * return. Effects thin out under load instead of the game misbehaving, which is
 * the right call for a fighting game and a thing a port must not "fix" into an
 * error.
 *
 * `+0x7c` non-zero diverts to a separate path early on; it is the flag
 * LIME_PlayFBXAtPos explicitly clears.
 *
 * ## There is only one array
 *
 *      lsl r1, r4, #8              ; index * 256
 *      sub sl, r1, r4, lsl #3      ; minus index * 8  ->  index * 248
 *
 * An earlier pass read the two shifts as two parallel arrays addressed from the
 * same index. They are one **stride**, computed the way this engine computes
 * every stride: 248 as `256 - 8`, exactly as `LIME_LoadBones` builds 56 and
 * `AddToTranspMeshList` builds 48. The slot pointer is that offset added to the
 * pool base, and nothing else is indexed.
 *
 * ## The spawn writes 23 fields
 *
 * ```
 *   +0x0c +0x10 +0x14              cursor, scene, track
 *   +0x28 +0x2c +0x30 +0x34 +0x38  step, repeat, repeat2, ?, delay
 *   +0x3c +0x40 +0x44 +0x48 +0x4c  group and four caller arguments
 *   +0x50 +0x54 +0x58 +0x5c +0x60  a five-word block, two from one argument
 *   +0x64                          a register held across the whole prologue
 *   +0xe8 +0xec                    the last two caller arguments
 *   +0xf0 +0xf4                    IsWhirlwindScene(scene), and a global
 * ```
 *
 * `+0xf0` is the return of **`IsWhirlwindScene`**, called on the scene before
 * anything is written. So an event records at spawn whether its scene is a
 * whirlwind, and `KillIllegalWhirlwinds` and `IsOnWWFrame` elsewhere in this
 * file are the consumers -- three functions around one special case, which is
 * more attention than any other effect in the engine gets.
 *
 * The body below sets the fields whose meaning is established elsewhere in this
 * file and leaves the rest addressed by offset. Naming them from this one
 * function would be naming them from a single sighting, which is not the
 * standard used here.
 */
/* armv7 0x000a4be4, transcribed whole. The earlier body (from armv6) set
 * five fields and left the rest of the slot as the heap had it.
 *
 *      scene, track   the scene the event plays and its track
 *      m1             the matrix the event follows (kept at +0x64)
 *      m2             its local placement; +0x68 = m2 * m1, +0xa8 = m2
 *      a4             +0x40
 *      offsetId2      +0x60, a second MasterEventOffsets row, -1 for none
 *      zeroOffset     nonzero: no offset at all
 *      a7             +0x48
 *      tex0, tex1     +0xe8, +0xec
 *      a10            +0x44
 *
 * Returns what r0 last held, as the original does: the pool-full -1, the
 * count when the track is at its cap, the slot's address when the track is
 * a shadow and 0 otherwise. No caller in this tree reads it. */
int LIME_TriggerEventFromSceneH(SCENEINFO *scene, SCENEEVENTTRACK *track,
                                limeMATRIX44 *m1, limeMATRIX44 *m2,
                                long a4, long offsetId2, long zeroOffset,
                                long a7, TEXTURE *tex0, TEXTURE *tex1, long a10)
{
    const char *t = (const char *)track;
    float tmp[16];
    int slot, n;
    EVENT *ev;

    slot = GetFreeEvent();
    if (slot == -1)
        return -1;

    if (*(const int32_t *)(t + 0xd0) != -1) {        /* the instance cap */
        n = CountEventsMatching(track, m1);
        if (n >= *(const int32_t *)(t + 0xd0))
            return n;
    }
    if (*(const int32_t *)(t + 0x7c) != 0)           /* a grouped track */
        KillAlleventsWithGroup(*(const int32_t *)(t + 0x7c));

    ev = &SceneEvents[slot];
    ev->isWhirlwind  = IsWhirlwindScene(scene);
    ev->obj          = LastGObj;
    ev->flushTexture = tex0;
    ev->fieldEC      = (long)(uintptr_t)tex1;
    ev->field48      = (int)a7;
    ev->field40      = (int)a4;
    ev->field44      = (int)a10;
    ev->group        = *(const int32_t *)(t + 0x7c);
    ev->state        = 1;
    memcpy(ev->color, t + 0x08, 16);
    memcpy(&ev->repeat2, t + 0x1c, 4);
    memcpy(&ev->delay,   t + 0x6c, 4);
    memcpy(&ev->field34, t + 0x20, 4);
    ev->frameB  = -1;
    ev->frameA  = 0;
    ev->cursor  = 0.0f;                             /* str r10(=0), [r4, #4] */
    ev->scene   = scene;
    memcpy(&ev->field4c, t + 0x24, 4);
    memcpy(&ev->step,    t + 0xc4, 4);
    ev->track   = track;
    memcpy(&ev->repeat,  t + 0x70, 4);

    limeMatrixMult(*m2, *m1, tmp);
    memcpy(ev->world, tmp, 64);
    memcpy(ev->local, *m2, 64);
    ev->follow = *m1;

    ev->offsetId  = *(const int32_t *)(t + 0xc0);
    ev->offsetId2 = (int)offsetId2;

    if (zeroOffset != 0) {
        ev->offX = ev->offY = ev->offZ = 0.0f;
    } else if (offsetId2 == -1) {
        memcpy(&ev->offX, MasterEventOffsets + ev->offsetId * 0x50 + 0x40, 12);
    } else {
        const float *a = (const float *)(MasterEventOffsets
                                         + ev->offsetId * 0x50 + 0x40);
        const float *b = (const float *)(MasterEventOffsets
                                         + offsetId2 * 0x50 + 0x40);
        ev->offX = a[0] + b[0];
        ev->offY = a[1] + b[1];
        ev->offZ = a[2] + b[2];
    }

    /* +0x78: a shadow. It plays at ShadowOffset with a step of 1. */
    if (*(const int32_t *)(t + 0x78) == 0)
        return 0;
    ev->field4c = 0;
    memcpy((char *)ev + 0xa0, &ShadowOffset, 4);
    ev->step = 1.0f;
    return (int)(uintptr_t)ev;
}


/* ------------------------------------------------------------ LIME_LoadEvents
 *
 * armv6 0x000e8484, 1064 bytes.  **Structurally complete.**
 *
 * Reads a `.events` file into the SCENEEVENTTRACK array that
 * LIME_TriggerEventFromSceneH later spawns from.
 *
 * ## 216 bytes per track, spelled out in four instructions
 *
 * The allocation is the clearest confirmation of the record size this file has:
 *
 *      lsl  r1, r3, #5             ; count * 32
 *      sub  r1, r1, r3, lsl #3     ; minus count * 8   -> count * 24
 *      lsl  r3, r1, #3             ; that * 8          -> count * 192
 *      add  r1, r1, r3             ; 24 + 192          -> count * 216
 *
 * **216**, exactly the stride `FindEventOffsets` and `LIME_FreeEvents` step by.
 * Three functions, one number, arrived at three different ways -- the allocator
 * builds it out of shifts, and the two walkers use it as a literal.
 *
 * (Note this is *not* the 248-byte figure. 248 is the size of a live EVENT slot
 * in the runtime pool; 216 is the size of a SCENEEVENTTRACK loaded from disk.
 * They are different structures and the project has confused them before.)
 *
 * ## Names are uppercased at load, in a 64-byte field
 *
 *      ldrb   r2, [r4, r3]
 *      sub    r3, r2, #0x61        ; - 'a'
 *      uxtb   r3, r3
 *      cmp    r3, #0x19            ; <= 25, i.e. was it a-z ?
 *      subls  r3, r2, #0x20        ; then subtract 32
 *      strbls r3, [r4, r2]
 *      add    r4, r4, #1
 *      cmp    r4, #0x40            ; 64 bytes
 *
 * The classic branch-free `islower` -- subtract `'a'`, treat as unsigned, one
 * compare covers both ends of the range -- applied **in place** across a
 * **64-byte** name field.
 *
 * So track names are normalised to uppercase **once, at load time**, and every
 * later lookup is a plain case-sensitive compare against an uppercase name.
 * That is why the `.events` files can carry mixed-case artist names like
 * `smokeparticle fbx` and `Smoke_FLOAT fbx` while the code never calls a
 * case-insensitive compare anywhere.
 *
 * **A port must uppercase too.** Skip it and every lookup fails for any track
 * whose author used lowercase -- which, from the shipped data, is most of them.
 *
 * ## A second array at +0xd4
 *
 *      ldr  r6, [r6, #0x9c]        ; a per-track count
 *      lsl  r2, r6, #6             ; count * 64
 *      lsl  r1, r6, #2             ; count * 4
 *      add  r1, r1, r2             ; -> count * 68
 *      bl   limeMalloc
 *      str  r0, [r8, #0xd4]
 *
 * A **68-byte** record array, sized from a count at `+0x9c` and hung off
 * `+0xd4` -- immediately after `+0xd0`, the instance cap established by
 * LIME_TriggerEventFromSceneH and LIME_PlayFBXAtPos.
 *
 * ## Failure behaviour
 *
 * A missing file returns NULL. A zero track count frees the buffer and returns
 * NULL as well, so an empty `.events` file and an absent one are
 * indistinguishable to the caller -- which matters, because LIME_LoadScene
 * stores this result without checking it.
 *
 * ## 44 bytes on disk become 216 in memory
 *
 * The parse is a field-by-field copy with a **remap**, not a memcpy. The disk
 * record advances by `0x2c` -- 44 bytes, eleven words -- and its fields land
 * scattered across the 216-byte runtime record:
 *
 * ```
 *   disk        memory
 *   +0x00   ->  +0x08
 *   +0x00   ->  +0x0c        (read again through a second pointer)
 *   +0x04   ->  +0x10
 *   +0x08   ->  +0x14
 *   +0x0c   ->  +0x18
 *   +0x10   ->  +0x1c
 *   +0x14   ->  +0x20
 *   +0x18   ->  +0x6c
 *   +0x1c   ->  +0xc8
 *   +0x20   ->  +0xcc
 *   +0x24   ->  +0x68
 *   +0x28   ->  +0x24
 * ```
 *
 * Eleven source words, twelve destinations, and the destinations are nowhere
 * near contiguous -- `+0x18` on disk jumps to `+0x6c`, and the next two go to
 * `+0xc8` and `+0xcc`, past the middle of the record. **Any external tool that
 * reads a `.events` file needs this table**; treating the file as the runtime
 * struct produces a record that looks plausible and is wrong in every field
 * after the seventh.
 *
 * The gaps are where the runtime fields live that the file does not carry --
 * `+0xd0` is the instance cap, `+0xd4` the array allocated from `+0x9c`, and
 * the block from `+0x28` up is written by LIME_PlayFBXAtPos when it builds a
 * track by hand.
 *
 * The names are copied ahead of this and uppercased in place, as described
 * above.
 */
/* armv7 0x000a477c, transcribed whole. The earlier body (from armv6) read
 * the names and stopped: no colours, counters, keys or scenes, so every
 * track's key list at +0xd4 was heap garbage.
 *
 * The file: a track count, then per track a 0x10c-byte header and its keys.
 *
 *      +0x00  64 bytes  the scene name, without ".scene"
 *      +0x40  7 words   -> track +0x08..+0x20 (colour RGBA, then +0x18..+0x20)
 *      +0x5c  4 words   -> +0x6c, +0xc8, +0xcc, +0x68
 *      +0x6c            -> +0x24
 *      +0x70  64 bytes  -> +0x28
 *      +0xb0  6 words   -> +0xc4 (step), +0x70, +0xd0 (cap), +0x74, +0x78,
 *                          +0x7c (group)
 *      +0xc8  64 bytes  -> +0x80, the offset name FindEventOffsets looks up
 *      +0x108           the key count -> +0x00
 *      +0x10c           keys, 0x38 bytes each: frame, a word, then a 48-byte
 *                       DS matrix; 0x44 bytes each in memory
 *
 * The scene is loaded only if one of its keys falls on a frame the mask
 * (arg2) lets through -- or always, without a mask -- and the original
 * hangs (`b .` at 0xa4a1e) if it cannot be loaded. */
EVENTSINFO *LIME_LoadEvents(const char *filename, long arg1, long arg2)
{
    const signed char *mask = (const signed char *)(uintptr_t)(unsigned long)arg2;
    const uint8_t *data, *src;
    EVENTSINFO *info;
    int32_t n, i, k;

    LIME_printf(8, "Events file %s", filename);
    data = (const uint8_t *)limeLoadFile(filename);
    if (data == NULL)
        return NULL;
    info = (EVENTSINFO *)limeMalloc("sceneevents_container", 8);
    if (info == NULL)
        return NULL;

    n = *(const int32_t *)data;
    LIME_printf(8, " has %d event tracks.\n", n);
    info->count = n;
    /* An empty file is NOT the same as an absent one: 0xa47dc..0xa47ea frees
     * the file, stores the zero count as `tracks` and returns the info.
     * LIME_LoadScene hangs (`b .` at 0x5f200) on a NULL -- FIGHT.events is
     * such a file, four bytes of zero. */
    if (n == 0) {
        limeFree((void *)data);
        info->tracks = NULL;
        return info;
    }
    info->tracks = (SCENEEVENTTRACK *)limeMalloc("sceneevents_tracks",
                                                 (size_t)n * 216);
    if (info->tracks == NULL)
        return NULL;

    src = data + 4;
    for (i = 0; i < n; i++) {
        char *t = (char *)info->tracks + (size_t)i * 216;
        char name[0x40], sceneName[0x40];
        int32_t nkeys;
        char *keys;
        int want = 0;
        size_t len;

        memcpy(name, src, 0x40);
        len = strlen(name);
        strcpy(sceneName, name);
        for (k = 0; k < 0x40; k++) {
            uint8_t c = (uint8_t)sceneName[k];
            if ((uint8_t)(c - 'a') <= 25)
                sceneName[k] = (char)(c - 0x20);
        }
        memcpy(sceneName + len, ".scene", 7);

        memcpy(t + 0x08, src + 0x40, 28);           /* +0x08..+0x20 */
        memcpy(t + 0x6c, src + 0x5c, 4);
        memcpy(t + 0xc8, src + 0x60, 4);
        memcpy(t + 0xcc, src + 0x64, 4);
        memcpy(t + 0x68, src + 0x68, 4);
        memcpy(t + 0x24, src + 0x6c, 4);
        memcpy(t + 0x28, src + 0x70, 0x40);
        memcpy(t + 0xc4, src + 0xb0, 4);
        memcpy(t + 0x70, src + 0xb4, 4);
        memcpy(t + 0xd0, src + 0xb8, 4);
        memcpy(t + 0x74, src + 0xbc, 4);
        memcpy(t + 0x78, src + 0xc0, 4);
        memcpy(t + 0x7c, src + 0xc4, 4);
        memcpy(t + 0x80, src + 0xc8, 0x40);

        nkeys = *(const int32_t *)(src + 0x108);
        keys = (char *)limeMalloc("sceneevents_events", (size_t)nkeys * 0x44);
        *(char *P32 *)(t + 0xd4) = keys;
        *(int32_t *)t = nkeys;
        if (keys == NULL)
            return NULL;

        src += 0x10c;
        for (k = 0; k < nkeys; k++, src += 0x38) {
            char *key = keys + k * 0x44;
            int32_t frame = *(const int32_t *)src;

            *(int32_t *)key = frame;
            if (mask == NULL || mask[frame] != 0)
                want = 1;
            memcpy(key + 4, src + 4, 4);
            memcpy(key + 8, src + 8, 0x30);
            LIME_printf(9, "");
        }

        *(SCENEINFO *P32 *)(t + 4) = NULL;
        if (want) {
            SCENEINFO *sc = LIME_LoadScene(sceneName, 1,
                                           (const char *)(uintptr_t)arg1, 0);
            if (sc == NULL) {
                fprintf(stderr, "LIME_LoadEvents: no %s -- the original "
                        "hangs here\n", sceneName);
                for (;;) { }
            }
            *(SCENEINFO *P32 *)(t + 4) = sc;
            LIME_LoadMeshSetTextures(sc->meshset,
                                     (const char *)(uintptr_t)arg1);
        }
    }

    limeFree((void *)data);
    FindEventOffsets((SCENEEVENTS *)info);
    return info;
}


/* ---------------------------------------------------------- LIME_RenderEvents
 *
 * armv6 0x000e88ac, 556 bytes.  **Structurally complete.**
 *
 * Draws every live event once per frame.
 *
 * ## Two translations, and what they identify
 *
 * The interesting part is a pair of `glTranslatef` calls back to back:
 *
 *      ldr  r3, [r4, #0x10]        ; the event's scene
 *      ldr  r0, [r3, #0x54]
 *      ldr  r1, [r3, #0x58]
 *      ldr  r2, [r3, #0x5c]
 *      bl   _glTranslatef          ; ...to the scene's origin
 *
 *      ldr  r0, [r4, #0x50]
 *      ldr  r1, [r4, #0x54]
 *      ldr  r2, [r4, #0x58]
 *      bl   _glTranslatef          ; ...then by the event's own offset
 *
 * Two independent facts fall out.
 *
 * **`EVENT+0x10` is the scene pointer**, which is exactly where
 * LIME_UpdateEvents reads it from (`ldr ip, [r4, #0x10]`, then `[ip, #0x44]`
 * for count2). Two functions, one offset, same meaning.
 *
 * **`SCENEINFO+0x54`, `+0x58` and `+0x5c` are a position**, three consecutive
 * words fed straight to glTranslatef. LIME_LoadScene fills those three from a
 * sibling file and this is what they turn out to be for -- the loader showed
 * where they come from, this shows what they mean, and neither alone would
 * have been enough to name them.
 *
 * An effect is therefore placed **relative to its scene**: the scene's origin
 * first, the event's own offset second. A port that positions effects in world
 * space will have every one of them land in the right place only while the
 * scene sits at the origin.
 *
 * ## The rest
 *
 * Fields `+0x38`, `+0x40`, `+0x48` and `+0x4c` gate the draw -- `+0x4c` is
 * compared against 1 specifically, so it is a mode rather than a flag. `+0x64`
 * reaches a float at `[r1, #0x3c]`. `limeMatrixMult` composes the transform and
 * `RenderDebugCube` is called from here, which is the only caller recovered so
 * far for that half-stripped function.
 *
 * ## Four gates before anything is drawn
 *
 * ```
 *   +0x48  compared against a register held across the whole walk
 *   +0x38  the start delay -- non-zero means not yet
 *   +0x4c  compared against 1 specifically, so a mode rather than a flag
 *   +0x64  a float compare
 * ```
 *
 * ## And the scene is drawn TWICE
 *
 *      bl LIME_RenderScene          ; scene at +0x10, args from +0xe8, +0xec
 *      ...
 *      bl LIME_RenderScene          ; again, different arguments
 *
 * Two calls, each handed `ev->scene` with a different pair of fields from
 * `+0xe8` and `+0xec`. Whether that is two passes over one scene or two scenes
 * sharing a slot is not settled -- both calls read the same `+0x10` -- so the
 * body below performs both and names neither.
 *
 * ## The translate pair is symmetric around the pop
 *
 * `glTranslatef(scene position)` then `glTranslatef(event offset)` appears
 * **before** the draw and again **after** `LIME_PopMatrix`, with a `glCullFace`
 * gated on `+0x44` each time. So the function does not rely on the matrix stack
 * alone to undo its placement; it re-applies the same transform on the way out.
 *
 * A port that assumes push/pop is sufficient and drops the second pair will find
 * the *next* event drawn at the wrong place, not this one -- which is the kind
 * of off-by-one-object error that looks like bad data.
 */
/* **It takes an argument, and it is a filter.** This was written with none.
 * The binary keeps it in `fp` across the whole loop and tests every event
 * against it:
 *
 *      ldr r3, [r4]        ; ev->state
 *      cmp r3, #0
 *      beq next            ; skip when zero
 *      blt next            ; and when negative -- so the test is state > 0
 *      ldr r3, [r4, #0x48]
 *      cmp r3, fp          ; the argument
 *      bne next            ; only this group is drawn
 *
 * So a caller renders one group of events at a time rather than the whole
 * pool, and `+0x48` is which group an event belongs to. `tools/protos.py`
 * found the missing parameter; the filter came from reading what it is for.
 */
/* armv7 0x000a4a3c, transcribed whole. The body above it (from armv6)
 * multiplied an uninitialised local matrix by itself and drew with that, so
 * no event was ever placed where it belonged -- Shao Kahn's death scene
 * (SK_ENDING / SK_LOOP) never appeared. What armv7 does per live event of
 * the group, once its delay is out:
 *
 *      +0x4c == 1   the event follows a matrix (+0x64): if that matrix was
 *                   killed (its +0x3c is 100) the event is killed the same
 *                   way and skipped; otherwise +0x68 = +0xa8 * follow
 *      glMatrixMode(0xba6)        GL_MODELVIEW_MATRIX, not a mode: a no-op
 *                                 GL error on the device, kept as it is
 *      RenderDebugCube, LIME_PushMatrix
 *      +0x40 set    the translates negate x (a mirrored event)
 *      glCullFace   back when +0x44 is 0, front when not; +0x40 swaps them
 *      glMultMatrixf(+0x68)
 *      SceneTint = the event's colour (+0x18, 16 bytes), the scene drawn
 *      twice (opaque, then flush), SceneTint back to 1,1,1, LIME_PopMatrix(1)
 */
void LIME_RenderEvents(long group)
{
    int i;

    for (i = 0; i < EVENT_SLOTS; i++) {
        EVENT *ev = &SceneEvents[i];        /* the pool, stride 0xf8 */
        int mirror;

        if (ev->state <= 0)                 /* beq AND blt, so not just == 0 */
            continue;
        if (ev->field48 != group)           /* +0x48, this pass only */
            continue;
        if (ev->delay != 0)                 /* +0x38, still waiting */
            continue;

        if (ev->field4c == 1) {
            if (ev->follow[15] == EVENT_KILL_VALUE) {   /* 0xa4a86 */
                ev->state     = -2;
                ev->world[15] = EVENT_KILL_VALUE;
                ev->local[15] = EVENT_KILL_VALUE;
                continue;
            }
            limeMatrixMult(ev->local, ev->follow, ev->world);
        }

        glMatrixMode(0xba6);
        RenderDebugCube();
        LIME_PushMatrix();

        mirror = ev->field40 != 0;
        glTranslatef(mirror ? -ev->scene->posX : ev->scene->posX,
                     ev->scene->posY, ev->scene->posZ);
        glTranslatef(mirror ? -ev->offX : ev->offX, ev->offY, ev->offZ);
        if ((ev->field44 != 0) == mirror)
            glCullFace(0x405);              /* GL_BACK */
        else
            glCullFace(0x404);              /* GL_FRONT */

        glMultMatrixf(ev->world);

        memcpy(SceneTint, ev->color, 16);
        /* arg1 is the literal 26; the second pass (flush = 1) collects the
         * translucent meshes and flushes them. */
        LIME_RenderScene(26, ev->scene, ev->frameA, ev->frameA, 0.0f, 0, 0,
                         0, ev->flushTexture, ev->fieldEC, NULL);
        LIME_RenderScene(26, ev->scene, ev->frameA, ev->frameA, 0.0f, 0, 0,
                         1, ev->flushTexture, ev->fieldEC, NULL);
        SceneTint[0] = 1.0f;
        SceneTint[1] = 1.0f;
        SceneTint[2] = 1.0f;

        LIME_PopMatrix(1);
    }
}


/* -------------------------------------------------- LIME_LoadMasterEventOffsets
 *
 * armv6 0x000e8230, 392 bytes.  **Structurally complete.**
 *
 * Loads the table `FindIdInMasterOffsets` searches -- the global registry that
 * maps an effect name to an index.
 *
 * ## An 80-byte record
 *
 *      lsl r3, r1, #4              ; count * 16
 *      lsl r1, r1, #6              ; count * 64
 *      add r1, r3, r1              ; count * 80
 *
 * One multiply as two shifts and an add, the same trick LIME_LoadBones and
 * AddToTranspMeshList use. The file is `memcpy`d in wholesale rather than
 * parsed field by field, so **the on-disk and in-memory layouts are identical
 * for these 80 bytes** -- unlike `.bones`, where 25 bytes on disk become 56 in
 * memory.
 *
 * Within a record the code reads a float at `+0x40`, an int at `+0x44` and a
 * float at `+0x48`. The 64 bytes before them are the name -- which is what
 * FindIdInMasterOffsets compares against, and what makes the leading 64-byte
 * field consistent with the name buffers everywhere else in this engine.
 *
 * ## Diagnostics that are not there
 *
 * The function calls `LIME_printf` **five times**, more than any other in
 * lime/common -- on entry, after the load, on the count, on failure and at the
 * end. All of them compile to nothing in the retail build.
 *
 * That is worth seeing rather than skipping past: this loader was clearly
 * awkward enough to need tracing while it was being written, and every one of
 * those messages is gone. Reading the retail binary means reading code whose
 * author had more information than we do.
 *
 * The file buffer is freed on both the success and the failure path.
 *
 * The per-record field reads are left out of the body: the code touches a float
 * at +0x40, an int at +0x44 and a float at +0x48 of each record, but what it
 * does with them was not traced, and the table is already usable without it --
 * FindIdInMasterOffsets only needs the name and the index.
 */
/* armv7 0x000a45d4. The file is a count and then 76-byte rows (a 64-byte
 * name and three floats); in memory each row is 80 bytes. The earlier body
 * copied count * 80 bytes straight across, so every row after the first
 * was shifted and read past the end of the file. */
void LIME_LoadMasterEventOffsets(void)
{
    const uint8_t *data, *src;
    char *dst;
    int i;

    LIME_printf(0x1d, "");
    NumMasterEventOffsets = 0;

    data = (const uint8_t *)limeLoadFile(MasterOffsetsFilename);
    if (data == NULL) {
        LIME_printf(0x1d, "");
        return;
    }
    NumMasterEventOffsets = *(const int32_t *)data;
    LIME_printf(0x1d, "", NumMasterEventOffsets);
    if (NumMasterEventOffsets == 0) {
        limeFree((void *)data);
        return;
    }

    MasterEventOffsets = (char *)limeMalloc("mastereventoffsets",
                                            NumMasterEventOffsets * 0x50);
    dst = MasterEventOffsets;
    src = data + 4;
    for (i = 0; i < NumMasterEventOffsets; i++, src += 0x4c, dst += 0x50) {
        memcpy(dst, src, 0x40);
        memcpy(dst + 0x40, src + 0x40, 12);   /* x, y, z */
    }
    limeFree((void *)data);
    LIME_printf(0x1d, "");
}


/* ---------------------------------------------------------- LIME_TriggerEvent
 *
 * armv6 0x000e8e98, 104 bytes.  **Complete.**
 *
 * A thin forwarder to LIME_TriggerEventFromSceneH, and its whole job is one
 * dereference:
 *
 *      mov ip, r0              ; the track
 *      ldr r0, [r0, #4]        ; -> its scene becomes the first argument
 *      ...
 *      mov r1, ip              ; and the track follows as the second
 *
 * **`SCENEEVENTTRACK+0x04` is a SCENEINFO pointer.** `LIME_FreeEvents` already
 * read that offset as one when releasing a scene's tracks; this is the second
 * function to treat it the same way, which is the standard a field
 * identification has to meet here.
 *
 * So the two entry points differ only in what the caller has to hand. A caller
 * holding a track calls this and the scene is fetched for it; a caller that
 * already knows the scene calls LIME_TriggerEventFromSceneH directly. Same
 * spawn, same gates, same silent failures.
 *
 * One argument is **not** forwarded: the wrapper writes a literal zero into the
 * seventh stack slot (`mov r3, #0; str r3, [sp, #8]`) rather than passing
 * anything through. Whatever that parameter selects, this path always takes its
 * zero case.
 */
/* armv7 0x000a4e68: the track's own scene, and a zero for "zeroOffset". */
int LIME_TriggerEvent(SCENEEVENTTRACK *track, limeMATRIX44 *m1,
                      limeMATRIX44 *m2, long a4, long offsetId2, long a7,
                      TEXTURE *tex0, TEXTURE *tex1, long a10)
{
    return LIME_TriggerEventFromSceneH(track->scene, track, m1, m2,
                                       a4, offsetId2, 0, a7, tex0, tex1, a10);
}


/* ------------------------- LIME_TriggerEventsFromSceneOffsetIfFollowing
 *
 * armv6 0x000e8f00, 484 bytes.  **Structurally complete.**
 *
 * Fires a scene's event tracks, offset, and only for tracks that are
 * "following" -- the name is doing real work here.
 *
 * ## What it walks
 *
 * The scene's events come from `scene->[0x84]`, and within a track it reads
 * `+0xd4` -- the 68-byte record array `LIME_LoadEvents` allocates from the
 * count at `+0x9c`. So this is the only recovered consumer of that array, and
 * it confirms the loader was allocating something real rather than reserving
 * space.
 *
 * A track is tested with `[r5, #0x24]` against **1** specifically, so `+0x24`
 * is a mode with at least one distinguished value rather than a boolean.
 *
 * ## The DS matrix appears again
 *
 *      bl _ConvertDSMatrixtoPCMatrix
 *
 * The offset is stored in the **Nintendo DS 1.3.12 fixed-point format** and
 * converted here at runtime -- see LIMEDS_Misc.c, where that function's `1/4096`
 * scale is what identified the format. This is the second place the engine pays
 * for its handheld ancestry at frame rate rather than at build time.
 *
 * It produces a row-major matrix that needs transposing for GL, unlike the
 * basis-built matrices elsewhere. A port that feeds this result straight to
 * `glMultMatrixf` gets a transposed offset, which places every following effect
 * in the wrong spot in a way that looks like a bad export rather than a bug.
 *
 * ## Diagnostics that DID survive
 *
 * Unusually, it calls `printf` and `puts` -- the real ones, not `LIME_printf`.
 * Everything else in lime/common logs through the compiled-away wrapper, so
 * these two lines still reach stdout in the retail build. Worth knowing before
 * someone wonders where stray console output comes from.
 *
 * ## Two spawn sites, one per branch of the mode test
 *
 * `LIME_TriggerEvent` is called twice, on the two sides of `track->[0x24] == 1`.
 * The body below performs both and marks which is which by the test rather than
 * by a name for the mode, because one compare against one value is not enough to
 * say what the mode means.
 */
/* **It takes ELEVEN arguments**, and this was written with four. The frame is
 * `push {r4,r5,r6,r7,lr}` (20) + `push.w {r8,sl,fp}` (12) + `sub sp, #0x68`
 * (104) = 136 = 0x88, so `[sp, #0x88]` is the caller's first stacked argument
 * -- and the body reads that and every word up to `[sp, #0xa0]`, which is
 * seven more. GameCode.c had always declared eleven, with names.
 *
 * The body below still only models the first four. The rest are named so the
 * interface is right and marked unused so it is obvious which half is read.
 */
/* armv7 0x000a4ea0. The same walk for a fighter's scene, with the frame
 * matched exactly (no modulus). A track whose +0x24 is 1 follows the
 * fighter -- its events are placed against `mFollow` -- and every other
 * track against `m`. Returns how many events it fired.
 *
 * `printed` starts the "printed the header line" flag: the caller passes 0
 * on the first call of a frame and 1 after. Frame 0x116 also prints
 * "got ya" (`puts`, 0xa4fa6). */
long LIME_TriggerEventsFromSceneOffsetIfFollowing(long player, long printed,
                                                  SCENEINFO *scene, long frame,
                                                  limeMATRIX44 *m,
                                                  limeMATRIX44 *mFollow,
                                                  long a4, long a7,
                                                  TEXTURE *tex0,
                                                  TEXTURE *tex1, long a10)
{
    const EVENTSINFO *events;
    long fired = 0, i, k;

    if (scene == NULL)
        return 0;
    events = (const EVENTSINFO *)scene->events;
    for (i = 0; i < events->count; i++) {
        SCENEEVENTTRACK *track = (SCENEEVENTTRACK *)
            ((char *)events->tracks + i * SCENEEVENTTRACK_STRIDE);
        const char *t = (const char *)track;
        const char *key = *(char *P32 const *)(t + 0xd4);

        for (k = 0; k < *(const int32_t *)t; k++, key += 0x44) {
            float mtx[16];

            if (*(const int32_t *)key != frame)
                continue;
            ConvertDSMatrixtoPCMatrix((const int32_t *)(key + 8), mtx);
            fired++;
            if (!printed) {
                LIME_printf(5, "\n");
                printed = 1;
            }
            printf("     - player %d (fr %d-%s) triggered event %s...\n ",
                   (int)player, (int)frame, scene->name,
                   track->scene ? (const char *)track->scene->name : "(null)");
            if (frame == 0x116)
                puts("got ya");
            LIME_TriggerEvent(track,
                              *(const int32_t *)(t + 0x24) == 1 ? mFollow : m,
                              (limeMATRIX44 *)mtx, a4, -1, a7,
                              tex0, tex1, a10);
        }
    }
    return fired;
}
