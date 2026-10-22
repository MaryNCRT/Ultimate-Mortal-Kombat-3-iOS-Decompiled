#include <stdio.h>
/*
 * other.c — src/gamecode/logic/other.c (process scheduler)
 *
 * The fight engine runs on cooperative "threads": each player object owns a
 * PROC, and state changes are queued rather than applied directly. This file
 * is the queueing side of that machinery.
 *
 * Hand-written from the disassembly of the armv7 slice and verified against
 * the oracle: tests/test_switchqueue_diff.c.
 *
 * The entry points into this system, all confirmed present in the binary:
 *
 *   _StartThreadAt    0x00056cac
 *   _KillSThread      0x00056cbc
 *   _KillProc         0x00056d14
 *   _SwitchQueue      0x00055ed4   <- implemented here
 *   _DoSwitchJump     0x00055efc   <- dispatches through function pointers
 *   _QueueAndJump     0x000572b8
 *   _UnstackSwitches  0x000573e8
 *   _clear_queues     0x00058974
 *
 * Note _DoSwitchJump reaches its target through a table of function pointers,
 * so the static recompiler cannot follow it. It has to be decompiled by hand
 * and checked by observation against the game running in touchHLE.
 */

#include "mk3logic.h"


/* ------------------------------------------------------------------------
 * The player object, as far as isp2 establishes it.
 *
 * Two offsets only. Everything else about this struct is unmapped and stays
 * that way -- a placeholder array would imply a size nobody has measured.
 * ------------------------------------------------------------------------ */

/*
 * Push a value onto a process's switch queue.
 *
 * Each entry packs two 16-bit halves:
 *   low  16 bits — a counter read from the global state struct at +0xa8
 *   high 16 bits — the caller's value
 *
 * so a consumer can tell which scheduling generation an entry belongs to.
 *
 * The wrap is deliberately checked *after* the store, which means the last
 * slot is written and then `head` resets. Reordering it would drop an entry.
 */
/* It RETURNS the packed word. That value is built in r0 and never overwritten
 * before the return, so it falls out the way `strike_check_ptr`'s answer does,
 * and nothing had read it while this was the only routine known to call it.
 * `QueueAndJump` reads it -- and stores only the low half, the counter. */
uint32_t SwitchQueue(uint16_t value, SWITCHQUEUE *q)
{
    uint32_t packed = (uint32_t)G_SWITCH_COUNTER(G)
                    | ((uint32_t)value << 16);

    *q->head = packed;
    q->head++;

    if (q->head >= &q->slots[SWITCH_QUEUE_SLOTS]) {
        q->head = &q->slots[0];
    }
    return packed;
}


/* ------------------------------------------------------------------ isp2
 *
 * armv7 0x00058798, 48 bytes.  `_isp2`
 *
 * The first function decompiled out of `gamecode`, and it is mostly a
 * sequencer: five calls in order, then two stores, then a sixth call.
 *
 *      face_opponent(obj)
 *      set_no_block(obj)
 *      me_in_front(obj)
 *      player_normpal(obj)
 *      flags = obj->field00->field10 | 0x10
 *      obj->field2c = flags
 *      obj->field00->field10 = flags
 *      disable_all_buttons(obj)
 *
 * ## What the two stores are, and what they are not
 *
 *      ldr r2, [r4]           ; obj->field00 -- a POINTER
 *      ldr r3, [r2, #0x10]
 *      orr r3, r3, #0x10
 *      str r3, [r4, #0x2c]    ; into the OBJECT
 *      str r3, [r2, #0x10]    ; and back into whatever field00 points at
 *
 * The same value lands in two places, which is worth noticing rather than
 * tidying: `obj->field2c` is not a copy that drifts, it is set from the
 * combined value at the moment the bit goes in.
 *
 * ## Names not given
 *
 * `field00` is a pointer to something carrying a flags word at `+0x10`. The
 * header of this file says each player object owns a PROC, and a process
 * pointer at offset zero would fit — but "would fit" is not the standard this
 * project holds itself to, and one function reading an offset is a hypothesis.
 * So both fields are named by offset and the bit is left as `0x10`.
 *
 * Nor is the function's own name interpreted. `isp2` sits beside `gup2` in the
 * two-player code and it is tempting to read it as something about player two;
 * nothing here supports that, and the five calls it makes are all
 * player-agnostic.
 *
 * Verified against the oracle by tests/test_isp2_diff.c: the call ORDER and the
 * two stores, over a sweep of starting flag values.
 */
void isp2(MK3OBJ *obj)
{
    uint32_t flags;

    face_opponent(obj);
    set_no_block(obj);
    me_in_front(obj);
    player_normpal(obj);

    flags = obj->field00->field10 | 0x10u;
    obj->field2c = flags;
    obj->field00->field10 = flags;

    disable_all_buttons(obj);
}


/* ------------------------------------------------------------------------
 * The A10 loaders.
 *
 * armv7 0x00057594 and 0x0005759c, four bytes each.  **Complete.**
 *
 *      ldr r3, [r0]        ; the object's PROC
 *      ldr r3, [r3, #4]    ; or #0x68
 *      str r3, [r0, #0x44]
 *
 * Two instructions and a store, and the only difference between them is which
 * field of the PROC is read. The names say what those fields are -- `him` and
 * `slave` -- and that is the whole of what is established: 0x04 of a PROC is
 * the opponent and 0x68 is the slave object.
 * ------------------------------------------------------------------------ */
void f_set_a10_to_him(MK3OBJ *obj)
{
    obj->a10 = obj->field00->him;
}

void f_set_a10_to_slave(MK3OBJ *obj)
{
    obj->a10 = obj->field00->slave;
}


/* ---------------------------------------------------------------- player_swpal
 *
 * armv7 0x00057480, six bytes.  **Complete.**
 *
 *      ldr r3, [r0, #8]
 *      str r1, [r3, #0x44]
 *
 * Writes the caller's second argument into the OTHER object's A10 slot -- not
 * its own. "swpal" is swap palette, and the value goes where an argument goes,
 * so this sets up the call rather than performing it.
 *
 * The argument is a flag and not a palette. Its only two callers are
 * `player_normpal`, which passes 0, and `player_froze_pal`, which passes 1.
 */
void player_swpal(MK3OBJ *obj, uint32_t frozen)
{
    obj->field08->a10 = frozen;
}


/* -------------------------------------------------------------------- stop_a8
 *
 * armv7 0x00055a60, six bytes.  **Complete.**
 *
 * Clears 0x18 and 0x1c together. The name gives them as the A8 pair; what the
 * pair MEANS is not in these three instructions and is not guessed at here.
 */
void stop_a8(MK3OBJ *obj)
{
    obj->field18 = 0;
    obj->field1c = 0;
}


/* ------------------------------------------------------------- zero_my_p_hit
 *
 * armv7 0x00054ec8, six bytes.  **Complete.**
 *
 *      ldr r0, [r0]        ; note r0 is REASSIGNED
 *      str r3, [r0, #0x44]
 *
 * The store goes through the PROC, so this clears the PROC's 0x44 and not the
 * object's. Same offset, different struct -- worth stating, because the three
 * routines above write the object's.
 */
void zero_my_p_hit(MK3OBJ *obj)
{
    obj->field00->p_hit = 0;
}


/* --------------------------------------------- GetThreadFunc and GetProcFunc
 *
 * armv7 0x000575dc and 0x000575cc, ten and sixteen bytes.  **Complete.**
 *
 *      ldr.w r3, [r0, #0xa4]   ; GetProcFunc first does r0 = obj->thread
 *      lsls  r3, r3, #3        ; times eight
 *      adds  r3, r3, r0
 *      ldr   r0, [r3, #4]
 *
 * An array of eight-byte entries laid out at the THREAD's own address, indexed
 * by 0xa4, with the function in the second word. The pair is what says whose
 * array it is: GetProcFunc loads `obj->thread` and then does exactly this on
 * it, so the index and the array belong to the thread and GetThreadFunc's
 * argument is one.
 *
 * The array overlaps the thread's own leading fields -- entry 0 is its first
 * eight bytes -- so the index is never small. `t_self_terminate` reads entry
 * `index + 1`, which is the frame above the running one.
 */
void *GetThreadFunc(MK3THREAD *thread)
{
    const uint32_t *entry =
        (const uint32_t *)((const char *)thread + thread->frame * 8);
    return (void *)(uintptr_t)entry[1];
}

void *GetProcFunc(MK3OBJ *obj)
{
    MK3THREAD *t = obj->thread;

    return (void *)(uintptr_t)((const uint32_t *)((const char *)t + t->frame * 8))[1];
}


/* -------------------------------------------------- KillProc and StartProcAt
 *
 * armv7 0x00056d14 and 0x00056cd0, twelve bytes each.  **Complete.**
 *
 * The object-level spelling of the two thread calls. Each takes the thread out
 * of the object and forwards it.
 *
 *      ldr r0, [r0, #4]
 *      bl  StartThreadAt
 *
 * r1 is not touched, so StartProcAt's SECOND argument reaches StartThreadAt
 * unchanged -- it is the function to start at, and it flows through. Written
 * with one parameter this would start every thread at whatever the register
 * happened to hold.
 */
void KillProc(MK3OBJ *obj)
{
    KillSThread(obj->thread);
}

void StartProcAt(MK3OBJ *obj, MK3THREADFUNC func)
{
    StartThreadAt(obj->thread, func);
}


/* ------------------------------------------------------- the object wrappers
 *
 * armv7 0x00055070, 0x000551f0, 0x00058508 and 0x00059750, ten bytes each.
 * **Complete.**
 *
 * Four of the same shape:
 *
 *      mov r1, r0          ; or: ldr r1, [r0, #8]
 *      bl  <the real one>
 *
 * r0 is not touched, so the callee receives the object twice -- as the subject
 * and as the target. The suffixed names are the ones that take the target
 * explicitly and these are the convenience spellings that supply it, which is
 * why `am_i_airborn` and `aborn4` are not the same function.
 *
 * `ani2` is the exception: it passes the object at 0x08 rather than itself, so
 * `ani2_ob` acts on the other one.
 */
long aborn4(MK3OBJ *obj, MK3OBJ *target);
long am_i_facing_him_px(MK3OBJ *obj, MK3OBJ *target);
void ani2_ob(MK3OBJ *obj, MK3OBJ *target);
void back_to_normal_px(MK3OBJ *obj, MK3OBJ *target);

long am_i_airborn(MK3OBJ *obj)
{
    return aborn4(obj, obj);
}

long am_i_facing_him(MK3OBJ *obj)
{
    return am_i_facing_him_px(obj, obj);
}

void ani2(MK3OBJ *obj)
{
    ani2_ob(obj, obj->field08);
}

void back_to_normal(MK3OBJ *obj)
{
    back_to_normal_px(obj, obj);
}


/* ------------------------------------------------------------- the flip_multi trio
 *
 * armv7 0x00055358, 0x00055364 and 0x00055394.  **Complete.**
 *
 *      _ob:  obj->field28  ^= 0x10
 *      _px:  target->field08->field28 ^= 0x10
 *      bare: flip_multi_px(obj, obj)
 *
 * The same bit 4 at 0x28 in all three, reached through a different number of
 * pointers. `isp2` above ORs bit 4 into 0x10 of the PROC; this is a different
 * field of a different struct and the shared bit number is a coincidence worth
 * naming as one.
 */
void flip_multi_ob(MK3OBJ *obj)
{
    obj->field28 ^= 0x10u;
}

void flip_multi_px(MK3OBJ *obj, MK3OBJ *target)
{
    (void)obj;                          /* r0 is not read */
    target->field08->field28 ^= 0x10u;
}

void flip_multi(MK3OBJ *obj)
{
    flip_multi_px(obj, obj);
}


/* -------------------------------------------------------------- face_opponent
 *
 * armv7 0x00055388, ten bytes.  **Complete.**
 *
 * Declared at the top of this file because `isp2` calls it; here is the body.
 * It is the wrapper, and `face_opponent_px` does the work.
 */
void face_opponent_px(MK3OBJ *obj, MK3OBJ *target);

void face_opponent(MK3OBJ *obj)
{
    face_opponent_px(obj, obj);
}


/* --------------------------------------------------------- gmo_proc_insobja8
 *
 * armv7 0x00059c54, eight bytes.  **Complete.**
 *
 * A tail call to `getobjectinsert` with the arguments untouched -- not even a
 * register move. The wrapper exists for the name.
 */
MK3OBJ *getobjectinsert(MK3OBJ *obj);

void *gmo_proc_insobja8(MK3OBJ *obj)
{
    return getobjectinsert(obj);
}


/* ------------------------------------------------------------- ground_player
 *
 * armv7 0x0005533c, ten bytes.  **Complete.**
 *
 *      ldr   r3, [r0, #8]
 *      ldr   r0, [r0]
 *      ldrh  r0, [r0, #0x40]
 *      strh  r0, [r3, #0x12]
 *
 * Copies a halfword from the PROC's 0x40 to the other object's 0x12. Both are
 * 16-bit; the load is unsigned and the store keeps the low half, so no sign
 * question arises here -- though `highest_mpart_ob` below reads the same 0x12
 * as SIGNED, which is worth knowing before anyone widens it.
 */
void ground_player(MK3OBJ *obj)
{
    MK3_SET_FIELD12(obj->field08, obj->field00->field40);
}


/* ------------------------------------------- highest_mpart_ob, lowest_mpart_ob
 *
 * armv7 0x0005579c and 0x00055820, ten bytes each.  **Complete.**
 *
 *      highest:  out->field1c = src->field38 + (int16_t)MK3_FIELD12(src)
 *      lowest:   out->field20 = src->field40 + (int16_t)MK3_FIELD12(src)
 *
 * `ldrsh`, so 0x12 is signed here. Two bounds, one offset added to both, and
 * the results land in adjacent words -- which is what a pair of bounds looks
 * like. Which axis is not established.
 */
void highest_mpart_ob(MK3OBJ *out, MK3OBJ *src)
{
    out->field1c = (uint32_t)(src->field38 + (int32_t)(int16_t)MK3_FIELD12(src));
}

void lowest_mpart_ob(MK3OBJ *out, MK3OBJ *src)
{
    out->field20 = (uint32_t)(src->field40 + (int32_t)(int16_t)MK3_FIELD12(src));
}


/* --------------------------------------------------- isa5 and joystick_in_a0
 *
 * armv7 0x00055dc4 and 0x00055d94, ten bytes each.  **Complete.**
 *
 * Two more of the wrapper shape.
 */
long isa5_px(MK3OBJ *obj, MK3OBJ *target);
long joystick_in_a0_px(MK3OBJ *obj, MK3OBJ *target);

long isa5(MK3OBJ *obj)
{
    return isa5_px(obj, obj);
}

long joystick_in_a0(MK3OBJ *obj)
{
    return joystick_in_a0_px(obj, obj);
}


/* ----------------------------------------------- player_normpal, player_froze_pal
 *
 * armv7 0x00057488 and 0x000575a4, ten bytes each.  **Complete.**
 *
 * The two callers that name player_swpal's second argument. `player_normpal`
 * is also declared at the top of this file, because `isp2` calls it.
 */
void player_normpal(MK3OBJ *obj)
{
    player_swpal(obj, 0);
}

void player_froze_pal(MK3OBJ *obj)
{
    player_swpal(obj, 1);
}


/* --------------------------------------- strike_check_a0, strike_check_a0_test
 *
 * armv7 0x000594c8 and 0x000595d8, ten bytes each.  **Complete.**
 *
 * The same pair shape: one core, called with 0 and with 1, and the `_test`
 * name on the 1. So the core's second argument asks it to report rather than
 * to act.
 */
long strike_check_a0_core(MK3OBJ *obj, long test_only);

long strike_check_a0(MK3OBJ *obj)
{
    return strike_check_a0_core(obj, 0);
}

long strike_check_a0_test(MK3OBJ *obj)
{
    return strike_check_a0_core(obj, 1);
}


/* ========================================================================
 * The thread machinery.
 *
 * The fight engine is cooperative: each object owns a thread, a thread owns a
 * stack of eight-byte frames, and nothing is pre-empted. These four are the
 * whole of the start-and-stop side of it.
 * ======================================================================== */



/* ------------------------------------------------------------- StartThreadAt
 *
 * armv7 0x00056cac, sixteen bytes.  **Complete.**
 *
 * Four stores and no call: the frame index back to zero, the function, and two
 * words cleared. Starting a thread is entirely a matter of writing to it -- the
 * scheduler picks it up on its own next pass.
 */
void StartThreadAt(MK3THREAD *thread, MK3THREADFUNC func)
{
    thread->frame = 0;
    thread->func = func;
    thread->field08 = 0;
    thread->fieldfc = 0;
}


/* -------------------------------------------------------------- KillSThread
 *
 * armv7 0x00056cbc, twenty bytes.  **Complete.**
 *
 * It does not kill anything. It starts the thread at `t_self_terminate`, which
 * ends it on its own next turn -- the only way to stop a thread in a scheduler
 * that never interrupts one.
 */
long t_self_terminate(MK3THREAD *thread);

void KillSThread(MK3THREAD *thread)
{
    /* The cast is where the shape changes: the scheduler installs a
     * `void (*)(void)` and calls it with the thread in r0, which is what
     * `t_self_terminate` reads. The name carries no suffix because the symbol
     * carries none. */
    StartThreadAt(thread, (MK3THREADFUNC)(void *)t_self_terminate);
}


/* --------------------------------------------------------- t_self_terminate
 *
 * armv7 0x00056c84, forty bytes.  **Complete.**
 *
 *      r2 = thread->frame + 1
 *      if (*(thread + r2 * 8) != 0) return -3
 *      *(thread + r2 * 8) = 0x12ff
 *      thread->fieldfc = 0x00016462
 *      return 0x00016462
 *
 * The frame ABOVE the running one. A non-zero entry there means something is
 * still stacked on this thread and the terminate refuses with -3; otherwise it
 * writes a marker into that slot and reports the same literal it stored.
 *
 * That refusal is the cooperative contract in one branch: a thread cannot be
 * taken down while a frame sits above it, so `KillSThread` is a request and not
 * an order.
 *
 * The two constants are left as constants. 0x12ff is a marker and 0x00016462
 * is both the stored value and the return; neither is an address in any
 * section, and nothing else in this file reads either back.
 *
 * Spelled with the thread as an argument although the scheduler installs it as
 * a `void (*)(void)`: it is called with the thread in r0, the way a thread
 * function receives its own. The cast is at the installation in `KillSThread`.
 */
long t_self_terminate(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    *mk3_frame(thread, thread->frame + 1) = 0x12ff;
    thread->fieldfc = 0x00016462u;
    return 0x00016462;
}


/* ------------------------------------------- FindThreadProc and NewThreadProc
 *
 * armv7 0x00057664 and 0x00058b54, sixteen bytes each.  **Complete.**
 *
 *      bl   FindThread          ; or NewThread
 *      cbz  r0, out
 *      ldr  r0, [r0, #0x108]
 *
 * Each calls the thread-level function with its arguments untouched and turns
 * the thread into the proc at 0x108, passing a null through.
 *
 * `FindThread`'s argument is a pid -- see its body below, which walks a list
 * comparing 0x104.
 *
 * `NewThread` takes an owner and a thread entry point. Neither wrapper reads
 * its arguments, so neither says so, and the signature was written as `void`
 * until `t_fx_friendship` called `NewThread` directly with a proc and a
 * function -- and `NewThread`'s own body stores its second argument at the
 * thread's 0x04, which is the handler slot. The wrappers were forwarding them
 * all along.
 */
MK3THREAD *FindThread(uint32_t pid);
MK3THREAD *NewThread(void *owner, MK3THREADFUNC func);

void *FindThreadProc(uint32_t pid)
{
    MK3THREAD *t = FindThread(pid);
    return t ? t->proc : NULL;
}

void *NewThreadProc(void *owner, MK3THREADFUNC func)
{
    MK3THREAD *t = NewThread(owner, func);
    return t ? t->proc : NULL;
}


/* ========================================================================
 * The flag clearers.
 *
 * Three of one shape, and the shape is worth naming once:
 *
 *      r2 = obj->field08          ; the other object
 *      r3 = r2->field30           ; its flag word
 *      r3 &= ~BIT
 *      obj->field54 = r3          ; a copy on the subject
 *      r2->field30  = r3
 *
 * The copy into 0x54 is not a return value -- none of the three returns
 * anything. It is the same habit `isp2` shows with 0x2c and `am_i_joy` with
 * 0x2c and 0x5c: the arcade kept the word it had just computed in a slot on
 * the object, because on the TMS34010 that is where a register spilled to.
 * ======================================================================== */

void clear_inviso(MK3OBJ *obj)
{
    uint32_t flags = obj->field08->field30 & ~0x20u;
    obj->field54 = flags;
    obj->field08->field30 = flags;
}

void clear_nocol(MK3OBJ *obj)
{
    uint32_t flags = obj->field08->field30 & ~0x100u;
    obj->field54 = flags;
    obj->field08->field30 = flags;
}

void clear_noedge(MK3OBJ *obj)
{
    uint32_t flags = obj->field08->field30 & ~0x400u;
    obj->field54 = flags;
    obj->field08->field30 = flags;
}


/* ------------------------------------------------------------------ am_i_joy
 *
 * armv7 0x00054ce0, sixteen bytes.  **Complete.**
 *
 * Bit 0 of the PROC's flag word, kept in two places on the way out: the whole
 * word into 0x2c and the isolated bit into 0x5c, and the bit is also returned.
 * `isp2` writes the same 0x2c, so that slot is where this file parks a flag
 * word it has just read.
 *
 * The name says the bit distinguishes a joystick from something else; which
 * else -- an AI, a replay, a network peer -- is not in these six instructions.
 */
long am_i_joy(MK3OBJ *obj)
{
    uint32_t flags = obj->field00->field10;

    obj->field2c = flags;
    obj->field5c = flags & 1u;
    return (long)(flags & 1u);
}


/* ---------------------------------------------------------- add_combo_damage
 *
 * armv7 0x00057494, sixteen bytes.  **Complete.**
 *
 *      r3 = obj->field00         ; the PROC
 *      r3 = r3->field00          ; an object
 *      r2 = r3->field00          ; and ITS proc
 *      r2->field54 += obj->field54
 *
 * Three loads, so the total lands on a PROC and not on an object -- 0x54
 * exists on both. The subject's 0x54 is added into that one's. 0x54 is the same slot the
 * flag clearers above write, which is worth stating and not resolving: two
 * routines using one offset for different things is exactly what a spill slot
 * looks like.
 */
void add_combo_damage(MK3OBJ *obj)
{
    MK3OBJPROC *target = obj->field00->field00->field00;

    target->field54 += obj->field54;
}


/* ------------------------------------------------------------- adjust_xy_a5
 *
 * armv7 0x000570bc, sixteen bytes.  **Complete.**
 *
 * Reads three fields and hands them to the worker in order.
 */
void multi_adjust_xy_ob(MK3OBJ *obj, uint32_t a, uint32_t b, uint32_t c);

void adjust_xy_a5(MK3OBJ *obj)
{
    multi_adjust_xy_ob(obj, obj->field30, obj->field1c, obj->field20);
}


/* -------------------------------------------------------- center_around_him
 *
 * armv7 0x00057b84, sixteen bytes.  **Complete.**
 *
 * Two dereferences and then the "me" version, so "him" is `obj->field00->
 * field00`. The same double step `add_combo_damage` takes.
 */
void center_around_me(MK3OBJ *obj);

void center_around_him(MK3OBJ *obj)
{
    center_around_me(obj->field00->field00);
}


/* -------------------------------------------------------------- center_obj_x
 *
 * armv7 0x00058458, sixteen bytes.  **Complete.**
 *
 * Writes 199 into 0x1c and then centres. 0x1c is the high bound
 * `highest_mpart_ob` computes into, so this is setting the bound before the
 * call rather than passing it -- the A-register habit again.
 */
void center_about_x(MK3OBJ *obj);

void center_obj_x(MK3OBJ *obj)
{
    obj->field1c = 0xc7;                /* 199 */
    center_about_x(obj);
}


/* --------------------------------------- take3, takeover_him, takeover_him_sr
 *
 * armv7 0x00058930, 0x00058954 and 0x00058948.  **Complete.**
 *
 * `take3` stops the opponent, disables his buttons and transfers control. The
 * two takeover names are BYTE-FOR-BYTE identical wrappers around it -- same
 * three instructions, no argument set up, twelve bytes each at two addresses.
 * The compiler did not merge them and neither does this: two names that the
 * game calls separately are two functions, whatever they contain.
 */
void stop_him(MK3OBJ *obj);
void disable_his_buttons(MK3OBJ *obj);
void xfer_otherguy(MK3OBJ *obj);

void take3(MK3OBJ *obj)
{
    stop_him(obj);
    disable_his_buttons(obj);
    xfer_otherguy(obj);
}

void takeover_him(MK3OBJ *obj)
{
    take3(obj);
}

void takeover_him_sr(MK3OBJ *obj)
{
    take3(obj);
}


/* ------------------------------------------------- two more flag clearers
 *
 * armv7 0x00054f00 and 0x00054f60, sixteen bytes each.  **Complete.**
 *
 * The same five instructions as the three above, masking bit 3 and bit 4 out
 * of the other object's 0x30. With these the family reads:
 *
 *      0x008  noflip        0x020  inviso
 *      0x010  shadow        0x100  nocol        0x400  noedge
 */
void clear_noflip(MK3OBJ *obj)
{
    uint32_t flags = obj->field08->field30 & ~0x8u;
    obj->field54 = flags;
    obj->field08->field30 = flags;
}

void clear_shadow_bit(MK3OBJ *obj)
{
    uint32_t flags = obj->field08->field30 & ~0x10u;
    obj->field54 = flags;
    obj->field08->field30 = flags;
}


/* --------------------------------- do_next_a9_frame, do_his_next_a9_frame
 *
 * armv7 0x00059e24 and 0x00059e14, sixteen bytes each.  **Complete.**
 *
 *      mine: pxob(obj, obj,               obj->field08)
 *      his:  pxob(obj, proc->field00,     proc->field04)
 *
 * The two differ only in which pair they hand over, and the "his" one takes
 * both out of the PROC -- the opponent object and the field beside it. Whether
 * those two are the same pairing as (obj, obj->field08) is not established
 * here; they occupy the same argument positions and that is all.
 */
/* Returns something: `next_anirate` branches past its own `movs r0, #0`
 * after calling through here, so the callee's r0 is the result. What the
 * value MEANS is not established by any caller written so far. */
long do_next_a9_frame_pxob(MK3OBJ *obj, MK3OBJ *a, MK3OBJ *b);

long do_next_a9_frame(MK3OBJ *obj)
{
    return do_next_a9_frame_pxob(obj, obj, obj->field08);
}

long do_his_next_a9_frame(MK3OBJ *obj)
{
    MK3OBJPROC *proc = obj->field00;

    return do_next_a9_frame_pxob(obj, proc->field00,
                                 (MK3OBJ *)(uintptr_t)proc->him);
}


/* ------------------------------------------------------------- find_part2
 *
 * armv7 0x00055450, sixteen bytes.  **Complete.**
 *
 *      r2 = obj->field40
 *      do { r3 = *r2++; obj->field1c = r3; } while (r3);
 *      obj->field40 = r2
 *
 * The only loop in this batch, and a short one: walk a word list to its zero
 * and leave the cursor past it. "Part 2" is what follows the first
 * NUL-terminated run, and the cursor at 0x40 is how the caller gets there.
 *
 * `field1c` is written every time round and ends up holding the terminator, so
 * it is the spill slot rather than a result -- the same 0x1c `highest_mpart_ob`
 * uses for a bound, which is what a spill slot looks like.
 */
void find_part2(MK3OBJ *obj)
{
    const uint32_t *p = (const uint32_t *)(uintptr_t)obj->field40;
    uint32_t word;

    do {
        word = *p++;
        obj->field1c = word;
    } while (word != 0);

    obj->field40 = (uint32_t)(uintptr_t)p;
}


/* ---------------------------------------------------------- get_his_action
 *
 * armv7 0x00054e38, sixteen bytes.  **Complete.**
 *
 *      obj->field1c = proc->field00                 ; the opponent object
 *      obj->field20 = proc->field00->field00->field18
 *
 * Two spills: the opponent, and something at 0x18 of the opponent's own PROC.
 * The name calls that second one his action.
 */
void get_his_action(MK3OBJ *obj)
{
    MK3OBJPROC *proc = obj->field00;

    obj->field1c = (uint32_t)(uintptr_t)proc->field00;
    obj->field20 = proc->field00->field00->field18;
}


/* -------------------------------------------------------------- ground_him
 *
 * armv7 0x00055348, sixteen bytes.  **Complete.**
 */
void ground_him(MK3OBJ *obj)
{
    ground_player(obj->field00->field00);
}


/* -------------------------------------------------------- init_special_act
 *
 * armv7 0x000587e0, sixteen bytes.  **Complete.**
 *
 *      proc->field18 = obj->field20
 *      init_special(obj)
 *
 * Moves 0x20 into the PROC's 0x18 and then initialises. 0x18 of a PROC is what
 * `get_his_action` reads as an action, so this is setting the action before
 * the call -- the argument-in-a-field habit again, and the reason so many of
 * these are two instructions and a branch.
 */
void init_special(MK3OBJ *obj);

void init_special_act(MK3OBJ *obj)
{
    obj->field00->field18 = obj->field20;
    init_special(obj);
}


/* --------------------------------------------- is_he_airborn, is_he_facing_me
 *
 * armv7 0x00055060 and 0x000551fc, sixteen bytes each.  **Complete.**
 *
 * The "him" spellings of the two questions written last batch. `is_he_airborn`
 * keeps the subject and passes the opponent as the target; `is_he_facing_me`
 * asks the opponent the "am I" question, which is the same question from the
 * other side and is why it needs no second argument.
 */
long is_he_airborn(MK3OBJ *obj)
{
    return aborn4(obj, obj->field00->field00);
}

long is_he_facing_me(MK3OBJ *obj)
{
    return am_i_facing_him(obj->field00->field00);
}


/* ----------------------------------------------------------- is_stick_down
 *
 * armv7 0x00055e1c, sixteen bytes.  **Complete.**
 *
 *      obj->field38 = 2
 *      return isa5(obj)
 *
 * A direction written into 0x38 and then the general test. 2 is down, by the
 * name; the rest of the family will fill in the other three and say whether
 * this is a bit mask or an enumeration.
 */
long is_stick_down(MK3OBJ *obj)
{
    obj->field38 = 2;
    return isa5(obj);
}



void set_noscroll(MK3OBJ *obj)
{
    uint32_t flags = obj->field08->field30 | MK3F_NOSCROLL;
    obj->field54 = flags;
    obj->field08->field30 = flags;
}

void set_no_block(MK3OBJ *obj)
{
    uint32_t flags = obj->field08->field30 | MK3F_NO_BLOCK;
    obj->field54 = flags;
    obj->field08->field30 = flags;
}

void set_noflip(MK3OBJ *obj)
{
    uint32_t flags = obj->field08->field30 | MK3F_NOFLIP;
    obj->field54 = flags;
    obj->field08->field30 = flags;
}

void set_ignore_y(MK3OBJ *obj)
{
    uint32_t flags = obj->field08->field30 | MK3F_IGNORE_Y;
    obj->field54 = flags;
    obj->field08->field30 = flags;
}

void set_nocol(MK3OBJ *obj)
{
    uint32_t flags = obj->field08->field30 | MK3F_NOCOL;
    obj->field54 = flags;
    obj->field08->field30 = flags;
}

void set_noedge(MK3OBJ *obj)
{
    uint32_t flags = obj->field08->field30 | MK3F_NOEDGE;
    obj->field54 = flags;
    obj->field08->field30 = flags;
}

void set_half_damage(MK3OBJ *obj)
{
    uint32_t flags = obj->field08->field30 | MK3F_HALF_DAMAGE;
    obj->field54 = flags;
    obj->field08->field30 = flags;
}

void set_quarter_damage(MK3OBJ *obj)
{
    uint32_t flags = obj->field08->field30 | MK3F_QUARTER_DAMAGE;
    obj->field54 = flags;
    obj->field08->field30 = flags;
}



long is_stick_up(MK3OBJ *obj)
{
    obj->field38 = MK3_STICK_UP;
    return isa5(obj);
}

long is_stick_left(MK3OBJ *obj)
{
    obj->field38 = MK3_STICK_LEFT;
    return isa5(obj);
}

long is_stick_right(MK3OBJ *obj)
{
    obj->field38 = MK3_STICK_RIGHT;
    return isa5(obj);
}


/* ------------------------------------------------------- lineup_a0_onto_a1
 *
 * armv7 0x00057214, sixteen bytes.  **Complete.**
 *
 *      match_ani_points_ob_ob(obj->field20, obj->field1c)
 *
 * Both arguments come out of spill slots and the object itself is not passed,
 * so by the time this is called the two things to line up are already sitting
 * in 0x1c and 0x20. The A-register names in the symbol say the same: a0 and a1
 * were where the arcade put them.
 */
void match_ani_points_ob_ob(uint32_t a, uint32_t b);

void lineup_a0_onto_a1(MK3OBJ *obj)
{
    match_ani_points_ob_ob(obj->field20, obj->field1c);
}


/* --------------------------------------------------------------- mk_random
 *
 * armv7 0x000586cc, sixteen bytes.  **Complete.**
 *
 * Calls the generator and parks the result in 0x1c. It returns nothing: the
 * caller reads the slot, which is why so much of this file writes 0x1c before
 * a call and reads it after.
 */
uint32_t random32(void);

void mk_random(MK3OBJ *obj)
{
    obj->field1c = random32();
}


/* -------------------------------------------------------- multi_adjust_xy
 *
 * armv7 0x000570ac, sixteen bytes.  **Complete.**
 *
 * The same worker `adjust_xy_a5` calls, with `obj->field08` in the second
 * position where that one passes `obj->field30`. Two different things in one
 * argument slot, so the parameter is written as a word rather than typed.
 */
void multi_adjust_xy(MK3OBJ *obj)
{
    multi_adjust_xy_ob(obj, (uint32_t)(uintptr_t)obj->field08,
                       obj->field1c, obj->field20);
}


/* ------------------------------------------------------------ strike_check
 *
 * armv7 0x000594a0, sixteen bytes.  **Complete.**
 *
 * The third argument is a literal zero and the second comes out of 0x1c.
 */
long strike_check_ptr(MK3OBJ *obj, uint32_t what, long flag);

long strike_check(MK3OBJ *obj)
{
    return strike_check_ptr(obj, obj->field1c, 0);
}


/* --------------------------------------------------- Endurance_ClearStruct
 *
 * armv7 0x00058300, twenty bytes.  **Complete.**
 *
 *      rsb r0, r0, r1      ; r0 = to - from
 *      subs r2, r2, r0     ; size -= that
 *      memset(to, 0, size)
 *
 * The `blx` target is 0x000ddbb4, which is not in the function table because
 * it is an import thunk; the symbol table has `_memset` as UNDF, which is what
 * identifies it.
 *
 * The arithmetic is a total size with a leading run taken off it, so this
 * clears the tail of a struct from a field onwards rather than the whole of
 * it. The three parameters are named for that reading and it is a reading:
 * only the subtraction is certain.
 */
void *memset(void *dst, int c, size_t n);

/* Its three call sites are all in `Endurance_ClearPlayer`, further down: GrObj
 * passes `from == base` and is cleared whole, Plyr and Pp pass `base + 12` and
 * keep twelve bytes. Read separately and arrived at the same body. */
void Endurance_ClearStruct(char *base, char *from, size_t total)
{
    memset(from, 0, total - (size_t)(from - base));
}


/* ------------------------------------------- adjust_him_x and adjust_him_xy
 *
 * armv7 0x0005706c and 0x00057080, twenty bytes each.  **Complete.**
 *
 * The same worker as `multi_adjust_xy`, with the opponent from the PROC's 0x04
 * in the second position. The pair differ in the last argument only: the `_x`
 * one passes a literal zero where `_xy` passes 0x20, so the fourth parameter
 * is the Y and leaving it zero is what makes the first horizontal.
 */
void adjust_him_x(MK3OBJ *obj)
{
    multi_adjust_xy_ob(obj, obj->field00->him, obj->field1c, 0);
}

void adjust_him_xy(MK3OBJ *obj)
{
    multi_adjust_xy_ob(obj, obj->field00->him, obj->field1c, obj->field20);
}


/* -------------------------------------------------------- air_init_special
 *
 * armv7 0x0005891c, twenty bytes.  **Complete.**
 */
void stop_me_player(MK3OBJ *obj);

void air_init_special(MK3OBJ *obj)
{
    stop_me_player(obj);
    isp2(obj);
}


/* ------------------------------------------------------------- allow_moves
 *
 * armv7 0x000597e4, twenty bytes.  **Complete.**
 *
 * The other end of `isp2`, which disables the buttons and forces the state:
 * this enables them again and returns the fighter to normal.
 */
void enable_all_buttons(MK3OBJ *obj);

void allow_moves(MK3OBJ *obj)
{
    enable_all_buttons(obj);
    back_to_normal(obj);
}


/* --------------------------------------------------------------- create_fx
 *
 * armv7 0x00058d70, twenty bytes.  **Complete.**
 *
 *      r1 = (MK3_FIELD0E(other) << 16) | MK3_FIELD12(other)
 *
 * Two halfwords packed into one word and passed as a single parameter -- the
 * high half from 0x0e and the low from 0x12. 0x12 is the same field
 * `highest_mpart_ob` reads as a signed offset; being the low half of a packed
 * pair here is a use of it and not a contradiction.
 */
void create_fx_param(MK3OBJ *obj, uint32_t packed);

void create_fx(MK3OBJ *obj)
{
    MK3OBJ *other = obj->field08;

    create_fx_param(obj, ((uint32_t)MK3_FIELD0E(other) << 16) | MK3_FIELD12(other));
}


/* ------------------------------------------------------------ dec_my_p_hit
 *
 * armv7 0x00054dec, twenty bytes.  **Complete.**
 *
 *      obj->field1c = obj            ; the object pointer itself
 *      proc->p_hit -= 1
 *      obj->field20 = proc->p_hit    ; re-read after the store
 *
 * Two spills of different kinds through one habit. 0x1c takes a pointer here
 * and a count in `find_part2` and a bound in `highest_mpart_ob`, which is why
 * it is a word and not a type.
 *
 * The re-read is in the original: it loads the PROC again rather than reusing
 * the value it just stored. Kept, because a transcription that tidies a
 * redundant load is one that will tidy a necessary one later.
 */
void dec_my_p_hit(MK3OBJ *obj)
{
    MK3OBJPROC *proc = obj->field00;

    obj->field1c = (uint32_t)(uintptr_t)obj;
    proc->p_hit = proc->p_hit - 1;
    obj->field20 = obj->field00->p_hit;
}


/* ================================================================= the pairs
 *
 * Four of one shape: fetch the character's animation, then do the thing.
 * `get_char_ani` and `get_char_ani2` leave what they find in a spill slot, so
 * the second call takes no argument beyond the object.
 * ========================================================================= */

void get_char_ani(MK3OBJ *obj);
void get_char_ani2(MK3OBJ *obj);
void find_last_frame(MK3OBJ *obj);

void do_first_a9_frame(MK3OBJ *obj)
{
    get_char_ani(obj);
    do_next_a9_frame(obj);
}

void find_ani_last_frame(MK3OBJ *obj)
{
    get_char_ani(obj);
    find_last_frame(obj);
}

void find_ani_part2(MK3OBJ *obj)
{
    get_char_ani(obj);
    find_part2(obj);
}

void find_ani2_part2(MK3OBJ *obj)
{
    get_char_ani2(obj);
    find_part2(obj);
}


/* -------------------------------------------------------- find_last_frame
 *
 * armv7 0x00055428, twenty bytes.  **Complete.**
 *
 *      cursor = obj->field40
 *      do { obj->field40 = cursor + 4;
 *           obj->field1c = cursor[1]; } while (cursor[1] && (cursor += 4));
 *      obj->field40 = cursor
 *
 * It looks one entry AHEAD and stops on the last non-zero, which is the
 * difference from `find_part2` next door: that one steps past the terminator,
 * this one stops before it. The cursor is left on the last frame and the
 * terminator is never consumed.
 */
void find_last_frame(MK3OBJ *obj)
{
    const uint32_t *cursor = (const uint32_t *)(uintptr_t)obj->field40;

    for (;;) {
        obj->field40 = (uint32_t)(uintptr_t)(cursor + 1);
        obj->field1c = cursor[1];
        if (cursor[1] == 0)
            break;
        cursor++;
    }
    obj->field40 = (uint32_t)(uintptr_t)cursor;
}


/* ---------------------------------------- get_his_dog and his_group_sound
 *
 * armv7 0x00057828 and 0x00057148, twenty bytes each.  **Complete.**
 *
 * Both are `call_for_him` with a function pointer and nothing else --
 * `distance_off_ground` and `group_sound`. The trampoline swaps the object's
 * 0x00 and 0x08 for the opponent's and calls through, so this is the file's
 * third way of not writing a routine twice.
 */
void call_for_him(MK3OBJ *obj, void (*fn)(MK3OBJ *));
void distance_off_ground(MK3OBJ *obj);
void group_sound(MK3OBJ *obj);

void get_his_dog(MK3OBJ *obj)
{
    call_for_him(obj, distance_off_ground);
}

void his_group_sound(MK3OBJ *obj)
{
    call_for_him(obj, group_sound);
}


/* ------------------------------------------------------------- get_my_dfe
 *
 * armv7 0x00054dc0, twenty bytes.  **Complete.**
 *
 * Sign-extends the other object's 0x0e into 0x28 and calls the worker. 0x0e is
 * the halfword `create_fx` packs into the high half of its parameter; read
 * here as SIGNED, which `create_fx` does not settle either way.
 */
void gdfe4(MK3OBJ *obj);

void get_my_dfe(MK3OBJ *obj)
{
    obj->field28 = (uint32_t)(int32_t)(int16_t)MK3_FIELD0E(obj->field08);
    gdfe4(obj);
}


/* -------------------------------------------- get_strength, get_my_strength
 *
 * armv7 0x0005509c and 0x000550b0, twenty bytes each.  **Complete.**
 *
 *      get_strength(i) = (*table)[i] read at +0x368
 *
 * The pointer slot at 0x000f357c is `G` -- see its declaration above -- so the
 * strengths begin 0x368 into the global state. `get_my_strength` supplies the
 * index from the PROC's 0x08 and parks the answer in 0x1c.
 */
uint32_t get_strength(uint32_t index)
{
    return *(const uint32_t *)(G_BYTES + index * 4 + 0x368);
}

void get_my_strength(MK3OBJ *obj)
{
    obj->field1c = get_strength(obj->field00->field08);
}


/* -------------------------------------------------- gso_dmawnz_insobja8
 *
 * armv7 0x00059c60, twenty bytes.  **Complete.**
 *
 * Fetches the insert object and, if there is one, copies 0x30 across into its
 * other object's 0x2c. The null check is the whole of the error handling.
 */
void gso_dmawnz_insobja8(MK3OBJ *obj)
{
    MK3OBJ *ins = (MK3OBJ *)getobjectinsert(obj);

    if (ins != NULL)
        ins->field08->field2c = obj->field30;
}


/* --------------------------------------- lights_on_hit and lights_on_slam
 *
 * armv7 0x00057a10 and 0x000579fc, twenty bytes each.  **Complete.**
 *
 *      MKEvent_Add(3, 0xb, 0x18, proc->field08)      ; hit
 *      MKEvent_Add(3, 0xb, 0x38, proc->field08)      ; slam
 *
 * One constant apart. Three of the four arguments are shared, so the third
 * chooses which lighting event and the first two are the event system's own
 * vocabulary rather than anything this file establishes.
 */
void MKEvent_Add(long a, long b, long c, long d);

void lights_on_hit(MK3OBJ *obj)
{
    MKEvent_Add(3, 0xb, 0x18, obj->field00->field08);
}

void lights_on_slam(MK3OBJ *obj)
{
    MKEvent_Add(3, 0xb, 0x38, obj->field00->field08);
}


/* --------------------------------------------------------------- init_1_q
 *
 * armv7 0x00058960, twenty bytes.  **Complete.**
 *
 *      memset(q, 0, 0x54)
 *      q->head = q + 4
 *
 * The constructor for the ring buffer this file's header describes: clear the
 * whole of it and point the head at the array, which starts one word in. The
 * header says the wrap check is against `q + 0x54`, and 0x54 is the size
 * cleared here -- the two agree, which is the only check available on a
 * function this small.
 */
void init_1_q(char *q)
{
    memset(q, 0, 0x54);
    *(uint32_t *)q = (uint32_t)(uintptr_t)(q + 4);
}


/* -------------------------------------- match_him_with_me, match_me_with_him
 *
 * armv7 0x000570cc and 0x000570f8, twenty bytes each.  **Complete.**
 *
 * The same worker with the two arguments the other way round. `_him_with_me`
 * passes (mine, his) and `_me_with_him` passes (his, mine), so the first
 * argument is the one that moves.
 */
void match_him_with_me(MK3OBJ *obj)
{
    match_ani_points_ob_ob((uint32_t)(uintptr_t)obj->field08,
                           obj->field00->him);
}

void match_me_with_him(MK3OBJ *obj)
{
    match_ani_points_ob_ob(obj->field00->him,
                           (uint32_t)(uintptr_t)obj->field08);
}


/* ------------------------------------------------------------------- mpyu
 *
 * armv7 0x00056150, twenty bytes.  **Complete.**
 *
 *      umull lo, hi, b, a
 *      if (lo_out) *lo_out = lo
 *      if (hi_out) *hi_out = hi
 *
 * A 64-bit unsigned multiply with both halves optional -- each pointer is
 * tested before it is written, so a caller that wants only the high word
 * passes null for the other. The name is the TMS34010 instruction it replaces.
 */
void mpyu(uint32_t a, uint32_t b, uint32_t *hi_out, uint32_t *lo_out)
{
    uint64_t product = (uint64_t)b * (uint64_t)a;

    if (lo_out != NULL)
        *lo_out = (uint32_t)product;
    if (hi_out != NULL)
        *hi_out = (uint32_t)(product >> 32);
}


/* -------------------------------------------------------------- rsnd_func
 *
 * armv7 0x00057dbc, twenty bytes.  **Complete.**
 *
 *      MKEvent_Add(2, 2, arg, 0)
 *
 * The caller's second argument becomes the event's third and everything else
 * is a literal. The object reaches r0 at every call site and is then ignored,
 * so this is a sound request that belongs to nobody -- the parameter is typed
 * for what the callers pass, not for what this function does with it.
 */
void rsnd_func(MK3OBJ *unused, uint32_t which)
{
    (void)unused;
    MKEvent_Add(2, 2, (long)which, 0);
}


/* ------------------------------------------ sans_repell and sans_repell_3
 *
 * armv7 0x00054d64 and 0x00054d78, twenty bytes each.  **Complete.**
 *
 *      obj->field38 = N
 *      *(uint16_t *)(G + 0x456) = N        ; the same N
 *
 * 0x40 and 3. Each writes its constant to two places, one on the object and
 * one in the global state, and the object's 0x38 is the same slot the stick
 * routines write their direction mask into. Two unrelated things through one
 * offset, again.
 */
void sans_repell(MK3OBJ *obj)
{
    obj->field38 = 0x40;
    *(uint16_t *)(G_BYTES + 0x456) = 0x40;
}

void sans_repell_3(MK3OBJ *obj)
{
    obj->field38 = 3;
    *(uint16_t *)(G_BYTES + 0x456) = 3;
}


/* ================================================== four events, four shapes
 *
 * `MKEvent_Add(kind, a, b, c)` again. Each of these fills it from a different
 * place and everything else is a literal, so what they establish is which
 * argument carries what -- not what the event system does with it.
 * ======================================================================== */

void send_code_a3(MK3OBJ *obj)
{
    MKEvent_Add(2, 4, (long)obj->field28, 0);
}

void shake_a11(MK3OBJ *obj)
{
    MKEvent_Add(1, 0, (long)obj->field48, 0);
}

/* r0 is ignored, and every call site has the object in it -- so the parameter
 * is typed for what the callers pass, as with `rsnd_func` above. */
void tsound_func(MK3OBJ *unused, uint32_t which)
{
    (void)unused;
    MKEvent_Add(2, 0, (long)which, 0);
}


/* ---------------------------------------------------------- set_his_noedge
 *
 * armv7 0x0005715c, twenty bytes.  **Complete.**
 *
 * `call_for_him` with `set_noedge`. The third caller of the trampoline, and
 * the first that passes a routine this file also defines -- which is what the
 * mechanism is for: one flag setter, reachable for either fighter.
 */
void set_his_noedge(MK3OBJ *obj)
{
    call_for_him(obj, set_noedge);
}


/* -------------------------------------------------------- walk_flip_reverse
 *
 * armv7 0x000552c8, twenty bytes.  **Complete.**
 *
 *      flags = obj->field08->field28
 *      obj->field2c = flags
 *      if (flags & 0x10) obj->field20 = -obj->field20
 *
 * Bit 4 of 0x28 is the bit `flip_multi` toggles, so that bit is a facing and
 * this negates the walk when it is set. Two functions written a batch apart
 * meeting on one bit is the first time this file has explained itself.
 *
 * The `ittt ne` block is three predicated instructions, which is a load, a
 * negate and a store -- an if with no else.
 */
void walk_flip_reverse(MK3OBJ *obj)
{
    uint32_t flags = obj->field08->field28;

    obj->field2c = flags;
    if (flags & 0x10u)
        obj->field20 = (uint32_t)(-(int32_t)obj->field20);
}


/* ------------------------------------------------------------ xfer_otherguy
 *
 * armv7 0x00055130, twenty bytes.  **Complete.**
 *
 * Parks the opponent in 0x1c and hands his THREAD to the fast transfer. The
 * PROC is loaded twice, once for each -- kept as written.
 */
void fastxfer_thread(MK3OBJ *obj, MK3THREAD *thread);

void xfer_otherguy(MK3OBJ *obj)
{
    MK3OBJPROC *proc = obj->field00;

    obj->field1c = (uint32_t)(uintptr_t)proc->field00;
    fastxfer_thread(obj, proc->field00->thread);
}


/* ---------------------------------------------------- ReallyKillProjectile
 *
 * armv7 0x0005764c, twenty-four bytes.  **Complete.**
 *
 *      t = FindThread(proc->field08 + 0x700)
 *      if (t) KillSThread(t)
 *
 * The 0x700 is added to the PROC's 0x08 before the search, so the projectile's
 * thread is found at a fixed distance from whatever that field addresses.
 * `KillSThread` is the restart-at-terminate call, so "really kill" is still a
 * request the thread has to honour on its next turn.
 */
void ReallyKillProjectile(MK3OBJ *obj)
{
    MK3THREAD *t = FindThread(obj->field00->field08 + 0x700);

    if (t != NULL)
        KillSThread(t);
}


/* ------------------------------------------------------------ adjust_him_a0
 *
 * armv7 0x00057094, twenty-four bytes.  **Complete.**
 *
 *      obj->field20 = (int32_t)obj->field1c >> 16          ; arithmetic
 *      obj->field1c = (int16_t)obj->field1c                ; sign-extended
 *      adjust_him_xy(obj)
 *
 * A packed coordinate pair split into its two halves before the call, both
 * signed. `create_fx` packs a pair the same way and this is the other end of
 * it: one word travels, two numbers arrive.
 */
void adjust_him_a0(MK3OBJ *obj)
{
    uint32_t packed = obj->field1c;

    obj->field20 = (uint32_t)((int32_t)packed >> 16);
    obj->field1c = (uint32_t)(int32_t)(int16_t)packed;
    adjust_him_xy(obj);
}


/* -------------------------------------------------------------- air_dragon
 *
 * armv7 0x00057530, twenty-four bytes.  **Complete.**
 *
 * Two literals -- 40 and -40 -- into the spill slots and into the call, with
 * the PROC's slave as the object to move. The slots are written as well as
 * passed, which is the habit and not a second use.
 */
void air_dragon(MK3OBJ *obj)
{
    obj->field1c = 0x28;                        /* 40 */
    obj->field20 = (uint32_t)-40;
    multi_adjust_xy_ob(obj, obj->field00->slave, 0x28, (uint32_t)-40);
}


/* -------------------------------------------------------------- am_i_shang
 *
 * armv7 0x0005790c, twenty-four bytes.  **Complete.**
 *
 * Bit 9 of the PROC's flag word, and the second bit of it this file names --
 * `am_i_joy` reads bit 0. The whole word goes to 0x2c and the answer to 0x5c,
 * and unlike `am_i_joy` this one does not return it: the caller reads the slot.
 */
void am_i_shang(MK3OBJ *obj)
{
    uint32_t flags = obj->field00->field10;

    obj->field5c = 0;
    obj->field2c = flags;
    if (flags & 0x200u)
        obj->field5c = 1;
}


/* --------------------------------------------------------------- am_i_short
 *
 * armv7 0x00055808, twenty-four bytes.  **Complete.**
 *
 *      get_my_height(obj)
 *      obj->field5c = (obj->field20 <= 103)
 *      return that
 *
 * The height arrives in 0x20 and the comparison is `> 0x67` inverted, so 103
 * is the threshold and short is at-or-below it. The answer goes to 0x5c, the
 * same slot `am_i_joy` and `am_i_shang` use.
 *
 * **It is also returned.** The `ite gt` writes the flag into r0 and the store
 * to 0x5c reads r0 back, so the value is still in r0 at the `pop` -- this file
 * previously said it was not returned, which was read from the store alone.
 * `t_dizzy_dude` in mkcanned.c branches on the returned value with `cbz r0`
 * and never looks at 0x5c, so both halves are live and a caller may use
 * either. `tall_or_short_ani`, the other caller, uses 0x5c.
 */
void get_my_height(MK3OBJ *obj);

long am_i_short(MK3OBJ *obj)
{
    get_my_height(obj);
    obj->field5c = ((int32_t)obj->field20 > 0x67) ? 0u : 1u;
    return (long)obj->field5c;
}


/* -------------------------------------------------------- boomerang_adjuster
 *
 * armv7 0x0005757c, twenty-four bytes.  **Complete.**
 *
 * Three slots filled and then the general adjuster: 72 and 32 as the pair, and
 * the PROC's slave as the object. The 32 is computed as `72 - 40` in one
 * instruction rather than loaded, which is the compiler and not the source.
 */
void boomerang_adjuster(MK3OBJ *obj)
{
    obj->field1c = 0x48;                        /* 72 */
    obj->field20 = 0x48 - 0x28;                 /* 32, as the code forms it */
    obj->field30 = obj->field00->slave;
    adjust_xy_a5(obj);
}


/* ------------------------------------------------------- borrow_ochar_sound
 *
 * armv7 0x00057c00, twenty-four bytes.  **Complete.**
 *
 *      saved = other->field24
 *      other->field24 = obj->field20
 *      ochar_sound(obj)
 *      other->field24 = saved
 *
 * The first function in this file that puts a slot back. Everything else that
 * sets up a call leaves the value where it landed, which is why "borrow" is in
 * the name -- it is the exception being marked.
 *
 * The object is re-read from 0x08 after the call rather than kept in a
 * register. Transcribed as written: whether `ochar_sound` can move it is not
 * established here, and assuming it cannot is how a port acquires a bug that
 * only shows on one character.
 */
void ochar_sound(MK3OBJ *obj);

void borrow_ochar_sound(MK3OBJ *obj)
{
    MK3OBJ  *other = obj->field08;
    uint32_t saved = other->field24;

    other->field24 = obj->field20;
    ochar_sound(obj);
    obj->field08->field24 = saved;
}


/* --------------------------------------------------------- center_around_me
 *
 * armv7 0x00057b6c, twenty-four bytes.  **Complete.**
 *
 *      MKEvent_Add(1, 1, (int16_t)MK3_FIELD0E(other), proc->field08)
 *
 * The same event kind as `shake_a11` with a different second argument. 0x0e is
 * read signed here and signed in `get_my_dfe`; `create_fx` takes it as the
 * high half of a packed word, which does not contradict either.
 */
void center_around_me(MK3OBJ *obj)
{
    MKEvent_Add(1, 1, (int32_t)(int16_t)MK3_FIELD0E(obj->field08),
                obj->field00->field08);
}


/* ------------------------------------------------------- decode_walk_table
 *
 * armv7 0x000552dc, twenty-four bytes.  **Complete.**
 *
 *      base  = obj->field1c
 *      entry = base + other->field24 * 8
 *      obj->field1c = entry[0]
 *      obj->field20 = entry[1] << 4
 *
 * A table of eight-byte entries whose base arrives in a slot and whose index
 * is the other object's 0x24 -- the same field `borrow_ochar_sound` lends. The
 * second word is scaled by sixteen on the way out, so it is a fixed-point
 * quantity and the first is not.
 */
void decode_walk_table(MK3OBJ *obj)
{
    const uint32_t *entry =
        (const uint32_t *)(uintptr_t)(obj->field1c + obj->field08->field24 * 8);

    obj->field1c = entry[0];
    obj->field20 = entry[1] << 4;
}


/* ----------------------------------------------------- distance_off_ground
 *
 * armv7 0x00055164, twenty-four bytes.  **Complete.**
 *
 *      ground = proc->field40
 *      obj->field1c = ground                       ; then overwritten
 *      obj->field24 =(int16_t)MK3_FIELD12(other)
 *      obj->field1c = ground - that
 *
 * 0x1c is written twice, the first time with a value nothing reads. Kept:
 * the store is in the instruction stream and a transcription that removes it
 * is asserting that nothing between the two can observe it, which nothing
 * here establishes.
 *
 * `ground_player` copies the same 0x40 into the same 0x12 that this subtracts,
 * so the two are a matched pair: one sets the height, the other measures it.
 */
void distance_off_ground(MK3OBJ *obj)
{
    uint32_t ground = obj->field00->field40;
    int32_t  height;

    obj->field1c = ground;
    height =(int32_t)(int16_t)MK3_FIELD12(obj->field08);
    obj->field24 = (uint32_t)height;
    obj->field1c = (uint32_t)((int32_t)ground - height);
}


/* ------------------------------------------------------ end_of_match_chores
 *
 * armv7 0x00056ae4, twenty-four bytes.  **Complete.**
 *
 *      obj->field1c = 0
 *      H[0x00] = H[0x04] = H[0x10] = H[0x14] = 0
 *
 * Four words of the second global cleared, in two adjacent pairs with a gap
 * between them. The gap is 0x08 and 0x0c, which this leaves alone -- worth
 * noting, because clearing a struct usually means clearing all of it.
 */
void end_of_match_chores(MK3OBJ *obj)
{
    obj->field1c = 0;
    *(uint32_t *)(H + 0x00) = 0;
    *(uint32_t *)(H + 0x04) = 0;
    *(uint32_t *)(H + 0x10) = 0;
    *(uint32_t *)(H + 0x14) = 0;
}


/* ------------------------------------------------------- face_opponent_px
 *
 * armv7 0x00055370, twenty-four bytes.  **Complete.**
 *
 *      if (!am_i_facing_him_px(obj, target)) flip_multi_px(obj, target);
 *
 * The chain `isp2` opens, closed. Both halves were written a batch apart --
 * the question and the flip -- and this is the one that joins them.
 */
void face_opponent_px(MK3OBJ *obj, MK3OBJ *target)
{
    if (!am_i_facing_him_px(obj, target))
        flip_multi_px(obj, target);
}


/* -------------------------------------------------------- fastxfer_thread
 *
 * armv7 0x00055118, twenty-four bytes.  **Complete.**
 *
 * `StartThreadAt` with one more field cleared -- 0xf8 as well as 0xfc, 0xa4 and
 * 0x08 -- and the function taken out of the caller's 0x38 rather than passed.
 * So 0x38 holds a thread entry point here, a direction mask in the stick
 * routines and a constant in `sans_repell`: three unrelated things through one
 * offset, which is now the clearest evidence in this file that these slots are
 * spill space and not fields.
 */
void fastxfer_thread(MK3OBJ *obj, MK3THREAD *thread)
{
    thread->fieldfc = 0;
    thread->frame = 0;
    thread->fieldf8 = 0;
    thread->field08 = 0;
    thread->func = (MK3THREADFUNC)(uintptr_t)obj->field38;
}


/* ------------------------------- find_ani_part_a14, find_ani2_part_a14
 *
 * armv7 0x000554a8 and 0x000554c0, twenty-four bytes each.  **Complete.**
 *
 *      saved = obj->field54
 *      get_char_ani(obj)            ; or get_char_ani2
 *      obj->field54 = saved
 *      find_part_a14(obj)
 *
 * The second and third restores in this file. What they defend against is NOT
 * the fetch: `get_char_ani` is decompiled further down and it writes 0x40 and
 * nothing else. The note here previously said the opposite, inferred from the
 * restore before the fetch had been read, and that inference was wrong.
 *
 * So 0x54 is preserved across a call that does not touch it, and
 * `find_part_a14` below says why it is worth preserving: 0x54 is the repeat
 * count that function consumes. The save defends a real quantity across a call
 * that happens not to threaten it -- which is redundant as the code stands and
 * is not the same as defending nothing.
 */
void find_part_a14(MK3OBJ *obj);

void find_ani_part_a14(MK3OBJ *obj)
{
    uint32_t saved = obj->field54;

    get_char_ani(obj);
    obj->field54 = saved;
    find_part_a14(obj);
}

void find_ani2_part_a14(MK3OBJ *obj)
{
    uint32_t saved = obj->field54;

    get_char_ani2(obj);
    obj->field54 = saved;
    find_part_a14(obj);
}


/* ------------------------------------------------------------- get_his_dfe
 *
 * armv7 0x00054dd4, twenty-four bytes.  **Complete.**
 *
 * The "him" spelling of `get_my_dfe`: the opponent into 0x1c, his 0x0e
 * sign-extended into 0x28, then the same worker. The PROC's 0x04 is loaded
 * twice, once for each.
 */
void get_his_dfe(MK3OBJ *obj)
{
    MK3OBJPROC *proc = obj->field00;

    obj->field1c = proc->him;
    obj->field28 = (uint32_t)(int32_t)(int16_t)
                   MK3_FIELD0E((const MK3OBJ *)(uintptr_t)proc->him);
    gdfe4(obj);
}


/* ---------------------------------------------------------- get_my_matchw
 *
 * armv7 0x000550e4, twenty-four bytes.  **Complete.**
 *
 *      obj->field1c = ((uint32_t *)H)[proc->field08]
 *
 * H indexed by the PROC's 0x08 -- the same number `get_strength` uses to index
 * G. So the two globals are addressed by one per-fighter index, and H is at
 * least an array of words.
 */
void get_my_matchw(MK3OBJ *obj)
{
    obj->field1c = ((const uint32_t *)H)[obj->field00->field08];
}


/* ------------------------------------------------------------ init_special
 *
 * armv7 0x000587c8, twenty-four bytes.  **Complete.**
 *
 * `air_init_special` without the grounding: this stops the player, grounds
 * him, then runs isp2, where the air version skips the middle step. The pair
 * is what makes the name of the other one mean something.
 */
void init_special(MK3OBJ *obj)
{
    stop_me_player(obj);
    ground_player(obj);
    isp2(obj);
}


/* --------------------------------------------- is_he_right and is_he_left
 *
 * armv7 0x0005517c and 0x00055194, twenty-four bytes each.  **Complete.**
 *
 *      right: (him->field0c > other->field0c)
 *      left:  r = 1 - is_he_right(obj);  if (r borrowed) r = 0
 *
 * A comparison of 0x0c on two objects, so 0x0c is a horizontal position.
 *
 * The left one is not a negation. `rsbs` sets the carry and `it lo` clamps
 * anything that borrowed to zero, so a return above 1 would come out as 0
 * rather than negative. On the two values `is_he_right` actually returns the
 * two spellings agree; the clamp is transcribed because it is there, not
 * because it is reachable.
 */
long is_he_right(MK3OBJ *obj)
{
    MK3OBJ *him = (MK3OBJ *)(uintptr_t)obj->field00->him;
    long    r = ((int32_t)him->field0c > (int32_t)obj->field08->field0c) ? 1 : 0;  /* `ite le`: signed */

    obj->field5c = (uint32_t)r;
    return r;
}

long is_he_left(MK3OBJ *obj)
{
    uint32_t r = 1u - (uint32_t)is_he_right(obj);

    if (r > 1u)                         /* the borrow the `it lo` tests */
        r = 0;
    obj->field5c = r;
    return (long)r;
}


/* ------------------------------------------------------------- is_he_short
 *
 * armv7 0x000557cc, twenty-four bytes.  **Complete.**
 *
 * The "him" spelling of `am_i_short`, with the same threshold of 103 and the
 * same slot for the answer.
 */
void get_his_height(MK3OBJ *obj);

void is_he_short(MK3OBJ *obj)
{
    get_his_height(obj);
    obj->field5c = ((int32_t)obj->field20 > 0x67) ? 0u : 1u;
}


/* ------------------------------------------------------------ ladderorder_a1
 *
 * armv7 0x00057674, twenty-four bytes.  **Complete.**
 *
 *      v = RoundParam[0x14]
 *      obj->field20 = v
 *      if (v > 3) obj->field20 = 1
 *
 * A clamp to 1, not to 3. Anything past the last order folds back to the
 * first, which is what the name asks for and what a saturating clamp would
 * not do.
 *
 * `_RoundParam` is the third global reached through a pointer slot in this
 * file, after G and H; the slot is 0x000f3534 and the word it holds is
 * 0x0038ed04.
 */
extern uint32_t *RoundParam;            /* 0x0038ed04 */

void ladderorder_a1(MK3OBJ *obj)
{
    uint32_t order = ((const uint32_t *)RoundParam)[0x14 / 4];

    obj->field20 = order;
    if ((int32_t)order > 3)
        obj->field20 = 1;
}


/* -------------------------------------------------------------- lower_dragon
 *
 * armv7 0x00057548, twenty-four bytes.  **Complete.**
 *
 * `air_dragon` with different numbers: 29 and 48 rather than 40 and -40, and
 * the same slave. Both fill the slots as well as passing the values, which is
 * the habit and not a second use.
 */
void lower_dragon(MK3OBJ *obj)
{
    obj->field1c = 0x1d;                        /* 29 */
    obj->field20 = 0x30;                        /* 48 */
    multi_adjust_xy_ob(obj, obj->field00->slave, 0x1d, 0x30);
}


/* ------------------------------------------------------- match_him_with_me_f
 *
 * armv7 0x000570e0, twenty-four bytes.  **Complete.**
 *
 * The match, then a flip. The `_f` is the flip, and the object flipped is the
 * opponent -- `proc->field00`, the same double step the "his" functions take.
 */
void match_him_with_me_f(MK3OBJ *obj)
{
    match_him_with_me(obj);
    flip_multi_px(obj, obj->field00->field00);
}


/* ------------------------------------------------------------ ochar_sound_c
 *
 * armv7 0x00057bb0, twenty-four bytes.  **Complete.**
 *
 *      MKEvent_Add(2, 3, (c << 8) + obj->field1c, 0)
 *
 * The caller's second argument goes in the high byte and the slot in the low,
 * so the two are packed into one event parameter. Adding rather than ORing is
 * what the code does; with a slot below 256 the two are the same and nothing
 * here bounds it.
 */
void ochar_sound_c(MK3OBJ *obj, uint32_t c)
{
    MKEvent_Add(2, 3, (long)((c << 8) + obj->field1c), 0);
}


/* --------------------------------------------------------- previous_q_entry
 *
 * armv7 0x00056138, twenty-four bytes.  **Complete.**
 *
 *      head = obj->field38
 *      back = head - 4
 *      if (back < obj->field34 + 4) back = head + 0x4c
 *      obj->field38 = back
 *      obj->field1c = *back
 *
 * The ring buffer this file's header describes, walked backwards. The wrap
 * adds 0x4c -- nineteen words -- which with the entry itself is the twenty
 * slots the header names. The two descriptions were written from opposite ends
 * of the same array and they agree, which is the only check either gets.
 *
 * The comparison is unsigned (`it lo`), so the guard is against the pointer
 * going below the base and not against a signed underflow.
 */
void previous_q_entry(MK3OBJ *obj)
{
    uint32_t head = obj->field38;
    uint32_t back = head - 4;

    if (back < obj->field34 + 4)
        back = head + 0x4c;

    obj->field38 = back;
    obj->field1c = *(const uint32_t *)(uintptr_t)back;
}


/* ------------------------------------------------------------ randu_minimum
 *
 * armv7 0x00058764, twenty-four bytes.  **Complete.**
 *
 *      randu(obj)
 *      obj->field1c = obj->field1c - 1 + obj->field20
 *
 * The generator leaves its number in 0x1c and this shifts it by a minimum held
 * in 0x20, less one. So 0x20 is the low bound and the range the generator
 * produced is inclusive at its top.
 */
void randu(MK3OBJ *obj);

void randu_minimum(MK3OBJ *obj)
{
    randu(obj);
    obj->field1c = obj->field1c - 1 + obj->field20;
}


/* ------------------------------------------------------------ restore_power
 *
 * armv7 0x00057810, twenty-four bytes.  **Complete.**
 *
 *      slot = obj->field30
 *      obj->field1c = *slot
 *      if (*slot != 0) return
 *      *slot = 5
 *      obj->field1c = 1
 *      *(obj->field34) = 1
 *
 * The first early return in this file: a non-zero value means there is nothing
 * to restore. The 5 and the 1 are formed as `+5` then `-4` from the zero just
 * tested, which is a register being reused rather than two constants.
 */
void restore_power(MK3OBJ *obj)
{
    uint32_t *slot = (uint32_t *)(uintptr_t)obj->field30;

    obj->field1c = *slot;
    if (*slot != 0)
        return;

    *slot = 5;
    obj->field1c = 1;
    *(uint32_t *)(uintptr_t)obj->field34 = 1;
}


/* --------------------------------------------------------------- set_inviso
 *
 * armv7 0x00054f70, twenty-four bytes.  **Complete.**
 *
 *      clear_shadow_bit(obj)
 *      flags = obj->field08->field30 | 0x20
 *      obj->field54 = flags
 *      obj->field08->field30 = flags
 *
 * Not the mirror of `clear_inviso`. Going invisible clears the shadow bit as
 * well, and coming back does not restore it -- the asymmetry is in the code
 * and is left there.
 */
void set_inviso(MK3OBJ *obj)
{
    uint32_t flags;

    clear_shadow_bit(obj);
    flags = obj->field08->field30 | MK3F_INVISO;
    obj->field54 = flags;
    obj->field08->field30 = flags;
}


/* ------------------------------------------------------------------ stop_him
 *
 * armv7 0x00055c20, twenty-four bytes.  **Complete.**
 *
 * `stop_me_player` applied to the opponent, with his 0x1c preserved across it.
 * The fourth restore in this file, and like the others it reloads the object
 * afterwards rather than keeping it in a register.
 */
void stop_him(MK3OBJ *obj)
{
    MK3OBJ  *him = obj->field00->field00;
    uint32_t saved = him->field1c;

    stop_me_player(him);
    obj->field00->field00->field1c = saved;
}


/* -------------------------------------------------------- strike_check_a0_core
 *
 * armv7 0x000594b0, twenty-four bytes.  **Complete.**
 *
 * Fetches the strike table into 0x1c and passes it on with the caller's flag
 * untouched. The two wrappers written earlier -- `strike_check_a0` and
 * `strike_check_a0_test` -- differ only in that flag, and this is where it
 * goes.
 */
void get_char_stk(MK3OBJ *obj);

long strike_check_a0_core(MK3OBJ *obj, long test_only)
{
    get_char_stk(obj);
    return strike_check_ptr(obj, obj->field1c, test_only);
}


/* ------------------------------------------------------------- sweep_sounds
 *
 * armv7 0x000580d0, twenty-four bytes.  **Complete.**
 *
 * A sound request with 15, then 0x1c cleared, then the group sound. The clear
 * sits between the two calls, so it is the second one's argument being set up
 * and not the first one's being torn down.
 */
void sweep_sounds(MK3OBJ *obj)
{
    rsnd_func(obj, 0xf);
    obj->field1c = 0;
    group_sound(obj);
}


/* --------------------------------------------- GetFrameWidth, GetFrameHeight
 *
 * armv7 0x0005841c and 0x00058468, twenty-eight bytes each.  **Complete.**
 *
 *      int a, b, c, d;
 *      mk3_getbbox(ani, &d, &c, &b, &a)
 *      return b;                       ; width
 *      return a;                       ; height
 *
 * The same call with four output pointers and a different one read back. Two
 * functions to get two numbers out of one bounding box, which is how the
 * arcade asked for them and is kept.
 *
 * The four locals are named by position because that is all the pair
 * establishes: which slot is which, not what the other two are.
 *
 * The first argument is an ANIMATION, not an object. Neither of these two says
 * so -- both forward r0 untouched -- and it was written as `MK3OBJ *` because
 * everything else in this file takes one. `slave_ani`, read later, passes
 * `script[0] & 0x3fff` into the same call, which settles it, and the
 * correction propagates back here.
 */
/* The id is SIGNED: mk3_update passes -1 to mean "the camera". */
void mk3_getbbox(long ani, int *p1, int *p2, int *p3, int *p4);

int GetFrameWidth(uint32_t ani)
{
    int s04 = 0, s08 = 0, s0c = 0, s10 = 0;

    mk3_getbbox(ani, &s10, &s0c, &s08, &s04);
    return s08;
}

int GetFrameHeight(uint32_t ani)
{
    int s04 = 0, s08 = 0, s0c = 0, s10 = 0;

    mk3_getbbox(ani, &s10, &s0c, &s08, &s04);
    return s04;
}


/* --------------------------------------------------------- NewThreadProcPid
 *
 * armv7 0x00058b38, twenty-eight bytes.  **Complete.**
 *
 * `NewThreadProc` with one more store: the caller's third argument goes into
 * the thread's 0x104 before the proc at 0x108 is returned. So 0x104 is the pid
 * and this is the only routine here that sets one.
 */
void *NewThreadProcPid(void *owner, MK3THREADFUNC func, uint32_t pid)
{
    MK3THREAD *t = NewThread(owner, func);

    if (t == NULL)
        return NULL;

    t->pid = pid;
    return t->proc;
}


/* ------------------------------------------------------------- call_for_him
 *
 * armv7 0x0005712c, twenty-eight bytes.  **Complete.**
 *
 *      save   obj->field00, obj->field08
 *      him  = obj->field00->field00
 *      obj->field00 = him->field00
 *      obj->field08 = him->field08
 *      fn(obj)
 *      restore
 *
 * The trampoline the wrappers use. The callee gets the same object with the
 * opponent's pair hung off it, which is why `set_his_noedge` is `set_noedge`
 * with nothing else -- one flag setter serves both fighters.
 *
 * The only function here that restores two slots, and the only one that calls
 * through a pointer.
 */
void call_for_him(MK3OBJ *obj, void (*fn)(MK3OBJ *))
{
    MK3OBJPROC *saved_proc  = obj->field00;
    MK3OBJ     *saved_other = obj->field08;
    MK3OBJ     *him         = saved_proc->field00;

    obj->field00 = him->field00;
    obj->field08 = him->field08;

    fn(obj);

    obj->field00 = saved_proc;
    obj->field08 = saved_other;
}


/* -------------------------------------------------- ReallyKillHisProjectile
 *
 * armv7 0x00057630, twenty-eight bytes.  **Complete.**
 *
 *      key = 0x700 - proc->field08 + 1
 *      t = FindThread(key); if (t) KillSThread(t)
 *
 * NOT the "him" spelling of `ReallyKillProjectile`, which adds 0x700 to the
 * same field -- it reflects about it instead.
 *
 * `delete_slave_notproj` below says why both work: pids 0x700 and 0x701 are
 * the two projectiles, one per fighter. For indices 0 and 1, `index + 0x700`
 * gives 0x700 and 0x701 while `0x700 - index + 1` gives 0x701 and 0x700 -- the
 * same pair with the fighters swapped, which is what "his" asks for.
 */
void ReallyKillHisProjectile(MK3OBJ *obj)
{
    MK3THREAD *t = FindThread(0x700 - obj->field00->field08 + 1);

    if (t != NULL)
        KillSThread(t);
}


/* -------------------------------------------------------------- away_x_vel
 *
 * armv7 0x00055ab0, twenty-eight bytes.  **Complete.**
 *
 *      if (is_he_right(obj)) obj->field1c = -obj->field1c
 *      set_x_vel_player(obj)
 *
 * Away from him: negate the velocity when he is to the right, then apply it.
 * `walk_flip_reverse` negates the same kind of quantity off a facing bit; this
 * one asks the question instead.
 */
void set_x_vel_player(MK3OBJ *obj);

void away_x_vel(MK3OBJ *obj)
{
    if (is_he_right(obj))
        obj->field1c = (uint32_t)(-(int32_t)obj->field1c);
    set_x_vel_player(obj);
}


/* -------------------------------------------------------- create_blood_proc
 *
 * armv7 0x0005877c, twenty-eight bytes.  **Complete.**
 *
 *      n = obj->field1c
 *      if (n <= 12) { mk3_bloodevent(proc->field08, n); n = obj->field1c; }
 *      return n
 *
 * The comparison is unsigned (`bhi`), so a large value skips the call rather
 * than wrapping into it. 0x1c is re-read after the event and returned, so the
 * event can change it -- which is why the reload is kept.
 */
/* No return value: it tail-calls MKEvent_Add, which is void. */
void mk3_bloodevent(long a, long n);

long create_blood_proc(MK3OBJ *obj)
{
    uint32_t n = obj->field1c;

    if (n <= 12u) {
        mk3_bloodevent(obj->field00->field08, n);
        n = obj->field1c;
    }
    return (long)n;
}


/* ----------------------------------------------------------- dec_his_p_hit
 *
 * armv7 0x00054e00, twenty-eight bytes.  **Complete.**
 *
 * `dec_my_p_hit` one step further out: the opponent goes into 0x1c, HIS PROC's
 * 0x44 is decremented, and the new value comes back into 0x20 -- re-read
 * through all three pointers rather than kept, exactly as the "my" version
 * re-reads through one.
 */
void dec_his_p_hit(MK3OBJ *obj)
{
    MK3OBJPROC *proc = obj->field00;
    MK3OBJPROC *his  = proc->field00->field00;

    obj->field1c = (uint32_t)(uintptr_t)proc->field00;
    his->p_hit = his->p_hit - 1;
    obj->field20 = obj->field00->field00->field00->p_hit;
}


/* ------------------------------------------------------------- delete_slave
 *
 * armv7 0x00056d20, twenty-eight bytes.  **Complete.**
 *
 *      if (proc->field64) {
 *          KillProc(that)
 *          proc->field64 = 0
 *          proc->slave   = 0
 *      }
 *
 * Two fields cleared, with the PROC reloaded between them. 0x68 is the slave
 * `f_set_a10_to_slave` reads, so 0x64 holds whatever `KillProc` was given --
 * the slave's object, since KillProc takes one and reads its thread.
 */
void delete_slave(MK3OBJ *obj)
{
    MK3OBJ *slave = (MK3OBJ *)(uintptr_t)obj->field00->field64;

    if (slave == NULL)
        return;

    KillProc(slave);
    obj->field00->field64 = 0;
    obj->field00->slave = 0;
}


/* ------------------------------------------------------------- edge_pick_a0
 *
 * armv7 0x00057170, twenty-eight bytes.  **Complete.**
 *
 *      saved = obj->field1c
 *      get_my_dfe(obj)
 *      obj->field1c = saved
 *      if (obj->field34 <= obj->field30) obj->field1c = -saved
 *
 * A restore and then a conditional negate of the restored value, comparing two
 * slots the fetch filled. Signed (`itt le`), and the negate is of the SAVED
 * value rather than of whatever is in the slot -- which is the same thing
 * here, and is written the way the code writes it.
 */
void edge_pick_a0(MK3OBJ *obj)
{
    uint32_t saved = obj->field1c;

    get_my_dfe(obj);
    obj->field1c = saved;
    if ((int32_t)obj->field34 <= (int32_t)obj->field30)
        obj->field1c = (uint32_t)(-(int32_t)saved);
}


/* -------------------------------------------------------- four_button_switch
 *
 * armv7 0x00057274, twenty-eight bytes.  **Complete.**
 *
 *      if ((int16_t)proc->field7c == 0) return button;
 *      if (button == 1) return 0;
 *      if (button == 4) return 3;
 *      return button;
 *
 * The six-button layout folded onto four. Only two buttons move -- 1 becomes 0
 * and 4 becomes 3 -- and everything else passes through, including 0 and 5.
 * The gate at 0x7c is read as a SIGNED halfword and tested against zero, so it
 * is a mode flag and not a count.
 *
 * `mk3_set_four_button` in the front end is the other half of this switch; the
 * stub in runtime/gamecode_stubs.c is where it currently stops.
 */
long four_button_switch(MK3OBJ *obj, long button)
{
    if ((int16_t)obj->field00->field7c == 0)
        return button;
    if (button == 1)
        return 0;
    if (button == 4)
        return 3;
    return button;
}


/* ---------------------------------------------------------------- frame_a9
 *
 * armv7 0x0005a664, twenty-eight bytes.  **Complete.**
 *
 *      do_next_a9_frame(obj)
 *      r = 1 - *(uint32_t *)obj->field40
 *      if (r borrowed) r = 0
 *      obj->field5c = r
 *
 * The same clamped negation `is_he_left` uses, on the first word of whatever
 * 0x40 now points at. So this reports "the frame list is exhausted": one means
 * the word was zero.
 */
void frame_a9(MK3OBJ *obj)
{
    uint32_t r;

    do_next_a9_frame(obj);
    r = 1u - *(const uint32_t *)(uintptr_t)obj->field40;
    if (r > 1u)
        r = 0;
    obj->field5c = r;
}


/* ============================================ the character animation tables
 *
 * Two named tables of pointers, indexed by a character number and then by a
 * frame:
 *
 *      obj->field40 = character_anitabs1[other->field24][obj->field40]
 *
 * 0x40 is both the index in and the result out, which is what makes these
 * "fetch" routines rather than getters -- the slot is advanced in place.
 *
 * The first index is 0x24, the same field `decode_walk_table` uses as a table
 * index and `borrow_ochar_sound` lends: a character number.
 * ======================================================================== */

extern uint32_t *P32 character_anitabs1[];  /* 0x0016ee34 */
extern uint32_t *P32 character_anitabs2[];  /* 0x0016ee9c */

void get_char_ani(MK3OBJ *obj)
{
    obj->field40 = character_anitabs1[obj->field08->field24][obj->field40];
}

void get_char_ani2(MK3OBJ *obj)
{
    obj->field40 = character_anitabs2[obj->field08->field24][obj->field40];
}

void get_his_char_ani(MK3OBJ *obj)
{
    const MK3OBJ *him = (const MK3OBJ *)(uintptr_t)obj->field00->him;

    obj->field40 = character_anitabs1[him->field24][obj->field40];
}


/* ------------------------------------------------------- get_his_char_ani2
 *
 * armv7 0x00055260, twenty-eight bytes.  **Complete.**
 *
 * The fourth of the fetch family: the second table, indexed by the opponent's
 * character number rather than the other object's.
 */
void get_his_char_ani2(MK3OBJ *obj)
{
    const MK3OBJ *him = (const MK3OBJ *)(uintptr_t)obj->field00->him;

    obj->field40 = character_anitabs2[him->field24][obj->field40];
}


/* ---------------------------------------------------------- get_his_matchw
 *
 * armv7 0x000550fc, twenty-eight bytes.  **Complete.**
 *
 *      obj->field1c = ((uint32_t *)H)[him_proc->field08]
 *
 * `get_my_matchw` with three dereferences instead of one, to reach the
 * opponent's PROC and take his index. Same table, same indexing.
 */
void get_his_matchw(MK3OBJ *obj)
{
    const MK3OBJPROC *his = obj->field00->field00->field00;

    obj->field1c = ((const uint32_t *)H)[his->field08];
}


/* --------------------------------------------------------- his_ochar_sound
 *
 * armv7 0x00057b94, twenty-eight bytes.  **Complete.**
 *
 *      MKEvent_Add(2, 3, (him->field24 << 8) + obj->field1c, 0)
 *
 * `ochar_sound_c` with the character number supplied from the opponent rather
 * than by the caller. The same packing: character in the high byte, slot in
 * the low, added rather than ORed.
 */
void his_ochar_sound(MK3OBJ *obj)
{
    const MK3OBJ *him = (const MK3OBJ *)(uintptr_t)obj->field00->him;

    MKEvent_Add(2, 3, (long)((him->field24 << 8) + obj->field1c), 0);
}


/* --------------------------------------------------------- hob_ochar_sound
 *
 * armv7 0x00057c18, twenty-eight bytes.  **Complete.**
 *
 *      if (obj->field18) obj->field1c = (uint16_t)obj->field1c;
 *      else              obj->field1c = obj->field1c >> 16;
 *      ochar_sound(obj)
 *
 * Half of a packed word, chosen by a flag: the low half when 0x18 is set and
 * the high half when it is not. Both unsigned -- a zero-extending `ldrh` in one
 * branch and an `lsrs` in the other -- which is worth stating beside
 * `adjust_him_a0`, which splits the same kind of word with arithmetic shifts.
 * The two are not interchangeable.
 */
void hob_ochar_sound(MK3OBJ *obj)
{
    if (obj->field18 != 0)
        obj->field1c = (uint16_t)obj->field1c;
    else
        obj->field1c = obj->field1c >> 16;
    ochar_sound(obj);
}


/* ----------------------------------------------------------- inc_his_p_hit
 *
 * armv7 0x00054e1c, twenty-eight bytes.  **Complete.**
 *
 * `dec_his_p_hit` with an add, and the same re-read through all three pointers
 * afterwards.
 */
void inc_his_p_hit(MK3OBJ *obj)
{
    MK3OBJPROC *proc = obj->field00;
    MK3OBJPROC *his  = proc->field00->field00;

    obj->field1c = (uint32_t)(uintptr_t)proc->field00;
    his->p_hit = his->p_hit + 1;
    obj->field20 = obj->field00->field00->field00->p_hit;
}


/* ------------------------------------------------------------ is_stick_away
 *
 * armv7 0x00055df0, twenty-eight bytes.  **Complete.**
 *
 *      obj->field38 = LEFT
 *      if (!is_he_right(obj)) obj->field38 = RIGHT
 *      return isa5(obj)
 *
 * The first of the stick routines that chooses its direction instead of
 * naming one. "Away" is not a fifth direction: it is left or right decided at
 * the time of asking, which is what the four constant ones were written for.
 */
long is_stick_away(MK3OBJ *obj)
{
    obj->field38 = MK3_STICK_LEFT;
    if (!is_he_right(obj))
        obj->field38 = MK3_STICK_RIGHT;
    return isa5(obj);
}


/* ------------------------------------------------------- joystick_in_a0_px
 *
 * armv7 0x00055d78, twenty-eight bytes.  **Complete.**
 *
 *      obj->field1c = ((uint32_t *)G)[target->field00->field08] & 0xf
 *
 * The stick state lives in G, one word per fighter, indexed the same way
 * `get_strength` indexes it. The low nibble is four bits, and four bits is
 * exactly what `is_stick_up`, `_down`, `_left` and `_right` write into 0x38 as
 * 1, 2, 4 and 8.
 *
 * Three readings meeting: the constants, the mask, and the shared index. That
 * is what makes it safe to say the stick is here rather than that a nibble is
 * read here.
 */
long joystick_in_a0_px(MK3OBJ *obj, MK3OBJ *target)
{
    uint32_t state = ((const uint32_t *)G)[target->field00->field08] & 0xfu;

    obj->field1c = state;
    return (long)state;
}


/* ------------------------------------------------------------ lineup_second
 *
 * armv7 0x00057560, twenty-eight bytes.  **Complete.**
 *
 * `air_dragon` and `lower_dragon` again with a third pair: -26 and -37, formed
 * as `~0x19` and then `-0xb` from it. Both negative, and the slave as the
 * object.
 */
void lineup_second(MK3OBJ *obj)
{
    obj->field1c = (uint32_t)-26;
    obj->field20 = (uint32_t)-37;
    obj->field30 = obj->field00->slave;
    adjust_xy_a5(obj);
}


/* ------------------------------------------------------ no_edge_both_players
 *
 * armv7 0x000575b0, twenty-eight bytes.  **Complete.**
 *
 *      GrObj[0x30] |= 0x400
 *      GrObj[0x7c] |= 0x400
 *
 * No argument. It sets the noedge bit at two offsets in one global, which is
 * what "both players" means when neither is passed in -- whatever GrObj holds,
 * it holds two of them 0x4c apart.
 */
void no_edge_both_players(void)
{
    *(uint32_t *)(GrObj + 0x30) |= MK3F_NOEDGE;
    *(uint32_t *)(GrObj + 0x7c) |= MK3F_NOEDGE;
}


/* --------------------------------------------- ochar_sound and ochar_sound_n
 *
 * armv7 0x00057be4 and 0x00057bc8, twenty-eight bytes each.  **Complete.**
 *
 *      MKEvent_Add(2, 3, (other->field24 << 8) + n, 0)
 *
 * The character number in the high byte and a sound number in the low. The two
 * differ only in where the low part comes from: `ochar_sound` takes it out of
 * 0x1c and `ochar_sound_n` from the caller. `his_ochar_sound` written last
 * batch is the third, taking the character from the opponent instead.
 */
void ochar_sound(MK3OBJ *obj)
{
    MKEvent_Add(2, 3, (long)((obj->field08->field24 << 8) + obj->field1c), 0);
}

void ochar_sound_n(MK3OBJ *obj, uint32_t n)
{
    MKEvent_Add(2, 3, (long)(n + (obj->field08->field24 << 8)), 0);
}


/* ---------------------------------------------------------------- FindThread
 *
 * armv7 0x00057610, thirty-two bytes.  **Complete.**
 *
 *      for (t = *head; t; t = t->next)
 *          if (t->pid == pid) return t;
 *      return NULL;
 *
 * A singly linked list from a pointer slot at 0x000f3220, with the link at
 * offset 0x00 and the key at 0x104 -- the pid `NewThreadProcPid` writes. The
 * loop is rotated so the test comes after the load, which is why the entry
 * jumps into the middle.
 *
 * The list head is not named here: one read of a slot is not a global worth
 * declaring, and nothing else in this file touches it.
 */
extern MK3THREAD *P32 *ThreadListHead;      /* pointer slot -> 0x000f3220 */

MK3THREAD *FindThread(uint32_t pid)
{
    MK3THREAD *t = *ThreadListHead;

    while (t != NULL) {
        if (t->pid == pid)
            return t;
        t = t->next;
    }
    return NULL;
}


/* ----------------------------------------------------------------- random32
 *
 * armv7 0x000586b0, twenty-eight bytes.  **Complete.**
 *
 *      hi = rand() << 17
 *      lo = rand() & 0x7fff
 *      return hi | (lo << 2)
 *
 * Two library calls -- the `blx` goes to an import thunk and `_rand` is UNDF
 * in the symbol table, the same identification as `memset` earlier.
 *
 * **Bits 0 and 1 are never set.** Fifteen bits land at 2..16 and fifteen at
 * 17..31, and nothing fills the bottom two. Every value is a multiple of four,
 * and `randu_minimum` builds its ranges on top of that. It is in the shifts,
 * not in this transcription.
 */
int rand(void);

uint32_t random32(void)
{
    uint32_t hi = (uint32_t)rand() << 17;
    uint32_t lo = (uint32_t)rand() & 0x7fffu;

    return hi | (lo << 2);
}


/* ------------------------------------------------------------ stop_me_player
 *
 * armv7 0x00055c04, twenty-eight bytes.  **Complete.**
 *
 *      stop_a8(obj->field08)
 *      obj->field1c = 0
 *      obj->field08->field20 = 0
 *      set_x_vel_player(obj)
 *
 * Three things cleared and then a velocity applied -- which is zero, because
 * 0x1c was just cleared and that is where `set_x_vel_player` reads from.
 */
void stop_me_player(MK3OBJ *obj)
{
    stop_a8(obj->field08);
    obj->field1c = 0;
    obj->field08->field20 = 0;
    set_x_vel_player(obj);
}


/* ------------------------------------------------------------ towards_x_vel
 *
 * armv7 0x00055a94, twenty-eight bytes.  **Complete.**
 *
 * `away_x_vel` with the test the other way round: negate when he is NOT to the
 * right. The pair is the same eight instructions with `cbz` against `cbnz`.
 */
void towards_x_vel(MK3OBJ *obj)
{
    if (!is_he_right(obj))
        obj->field1c = (uint32_t)(-(int32_t)obj->field1c);
    set_x_vel_player(obj);
}


/* ----------------------------------------------------------- away_x_vel_him
 *
 * armv7 0x00055acc, thirty-two bytes.  **Complete.**
 *
 *      save obj->field00, obj->field08
 *      obj->field08 = proc->him                 ; the PROC's 0x04
 *      obj->field00 = proc->field00->field00    ; the opponent's PROC
 *      away_x_vel(obj)
 *      restore
 *
 * The same idea as `call_for_him` and NOT the same substitution: that one sets
 * 0x08 from the opponent's own 0x08, this one from the PROC's 0x04. One field
 * differs, so they are two things and are written as two.
 */
void away_x_vel_him(MK3OBJ *obj)
{
    MK3OBJPROC *saved_proc  = obj->field00;
    MK3OBJ     *saved_other = obj->field08;

    obj->field08 = (MK3OBJ *)(uintptr_t)saved_proc->him;
    obj->field00 = saved_proc->field00->field00;

    away_x_vel(obj);

    obj->field08 = saved_other;
    obj->field00 = saved_proc;
}


/* ---------------------------------------------------------- call_a0_for_him
 *
 * armv7 0x0005710c, thirty-two bytes.  **Complete.**
 *
 * `call_for_him` with the function taken from 0x1c rather than passed. The a0
 * slot holding a code pointer, after holding a count, a bound, an object and a
 * table base elsewhere in this file.
 */
void call_a0_for_him(MK3OBJ *obj)
{
    MK3OBJPROC *saved_proc  = obj->field00;
    MK3OBJ     *saved_other = obj->field08;
    MK3OBJ     *him         = saved_proc->field00;
    void      (*fn)(MK3OBJ *);

    obj->field00 = him->field00;
    obj->field08 = him->field08;

    fn = (void (*)(MK3OBJ *))(uintptr_t)obj->field1c;
    fn(obj);

    obj->field00 = saved_proc;
    obj->field08 = saved_other;
}


/* ------------------------------------------------------------ center_about_x
 *
 * armv7 0x00058438, thirty-two bytes.  **Complete.**
 *
 *      half = GetFrameWidth(other->field2c) >> 1      ; arithmetic
 *      obj->field20 = half
 *      obj->field1c -= half
 *      other->field0e = (uint16_t)obj->field1c
 *
 * Half a frame width taken off a position and the result stored back as a
 * HALFWORD into 0x0e -- the high half of the word at 0x0c. So the x this
 * centres is the one `is_he_right` compares, reached through its top half.
 *
 * `GetFrameWidth` is given `other->field2c`, not the object, so 0x2c holds
 * the animation the bounding box is asked about -- which is what later made
 * `mk3_getbbox`'s first parameter an animation everywhere.
 */
void center_about_x(MK3OBJ *obj)
{
    int32_t half = (int32_t)GetFrameWidth(obj->field08->field2c) >> 1;

    obj->field20 = (uint32_t)half;
    obj->field1c = (uint32_t)((int32_t)obj->field1c - half);
    MK3_SET_FIELD0E(obj->field08, obj->field1c);
}


/* ------------------------------------------------------------- damage_to_me
 *
 * armv7 0x00059650, thirty-two bytes.  **Complete.**
 *
 *      if (proc->field08 == 0) { obj->field34 = 0; obj->field20 = 1; }
 *      else                    { obj->field34 = 1; obj->field20 = 0; }
 *      bar_reducer(obj)
 *
 * The two slots take opposite values, and which way round depends on the
 * fighter index being zero. So this is "which bar" -- player one against
 * player two -- and the pair is a selector rather than two flags.
 *
 * Both branches form the second value from the first with a `+1` or a `-1`,
 * which is the compiler reusing a register.
 */
void bar_reducer(MK3OBJ *obj);

void damage_to_me(MK3OBJ *obj)
{
    if (obj->field00->field08 == 0) {
        obj->field34 = 0;
        obj->field20 = 1;
    } else {
        obj->field34 = 1;
        obj->field20 = 0;
    }
    bar_reducer(obj);
}


/* ------------------------------------------------------ distance_from_ground
 *
 * armv7 0x00055144, thirty-two bytes.  **Complete.**
 *
 * `distance_off_ground` with the ground taken from G at 0xac instead of from
 * the PROC's 0x40. Same subtraction, same two slots, same double write of
 * 0x1c. One measures against the fighter's own ground and the other against
 * the stage's.
 */
void distance_from_ground(MK3OBJ *obj)
{
    uint32_t ground = *(const uint32_t *)(G_BYTES + 0xac);
    int32_t  height;

    obj->field1c = ground;
    height =(int32_t)(int16_t)MK3_FIELD12(obj->field08);
    obj->field24 = (uint32_t)height;
    obj->field1c = (uint32_t)((int32_t)ground - height);
}


/* ------------------------------------------------------------ find_part_a14
 *
 * armv7 0x00055488, thirty-two bytes.  **Complete.**
 *
 *      if (--obj->field54 == 0) return;
 *      do { find_part2(obj); } while (--obj->field54 != 0);
 *
 * 0x54 is a repeat count, decremented once before the loop and once per turn.
 * This is what the two `find_ani_*_part_a14` wrappers are protecting when they
 * save it across the fetch.
 *
 * The decrement before the test means a count of 1 does nothing and a count of
 * 0 wraps to 0xffffffff and runs four billion times. Nothing here bounds it.
 */
void find_part_a14(MK3OBJ *obj)
{
    obj->field54 = obj->field54 - 1;
    if (obj->field54 == 0)
        return;

    do {
        find_part2(obj);
        obj->field54 = obj->field54 - 1;
    } while (obj->field54 != 0);
}


/* ------------------------------------------------------------- get_char_stk
 *
 * armv7 0x00055f88, thirty-two bytes.  **Complete.**
 *
 *      chr = other->field24
 *      obj->field20 = chr                  ; overwritten two lines down
 *      row = strike_tables[chr]
 *      obj->field20 = row
 *      obj->field1c = row[obj->field1c]
 *
 * The same two-level shape as the animation fetches, on a named table. 0x20 is
 * written twice and the first value is not read in between -- kept, because
 * the store is in the instruction stream.
 */
extern uint32_t *P32 strike_tables[];       /* 0x00169fbc */

void get_char_stk(MK3OBJ *obj)
{
    uint32_t  chr = obj->field08->field24;
    uint32_t *row;

    obj->field20 = chr;
    row = strike_tables[chr];
    obj->field20 = (uint32_t)(uintptr_t)row;
    obj->field1c = row[obj->field1c];
}


/* ---------------------------------------------------------- get_his_a11_ani
 *
 * armv7 0x000554d8, thirty-two bytes.  **Complete.**
 *
 *      saved = obj->field40
 *      obj->field40 = obj->field48
 *      get_his_char_ani(obj)               ; reads and writes 0x40
 *      obj->field48 = obj->field40
 *      obj->field40 = saved
 *      obj->a10 = proc->him                ; 0x44, the argument slot
 *
 * The fetch works on 0x40 and this needs it done to 0x48, so the value is
 * moved in, fetched, and moved out. The same borrow-and-restore as
 * `borrow_ochar_sound`, applied to a slot rather than to another object's
 * field.
 */
void get_his_a11_ani(MK3OBJ *obj)
{
    uint32_t saved = obj->field40;

    obj->field40 = obj->field48;
    get_his_char_ani(obj);
    obj->field48 = obj->field40;
    obj->field40 = saved;
    obj->a10 = obj->field00->him;
}


/* --------------------------------------------------------- get_his_strength
 *
 * armv7 0x000550c4, thirty-two bytes.  **Complete.**
 *
 * `get_my_strength` through three pointers instead of one: the opponent's
 * PROC's index, into the same table at G + 0x368.
 */
void get_his_strength(MK3OBJ *obj)
{
    const MK3OBJPROC *his = obj->field00->field00->field00;

    obj->field1c = *(const uint32_t *)(G_BYTES + his->field08 * 4 + 0x368);
}


/* ---------------------------------------------------------- get_walk_info_f
 *
 * armv7 0x0005531c, thirty-two bytes.  **Complete.**
 *
 *      obj->field24 = 1
 *      obj->field1c = walk_forward_info
 *      decode_walk_table(obj)
 *      walk_flip_reverse(obj)
 *
 * The table base into the slot `decode_walk_table` reads it from, then the
 * decode, then the facing flip. The 1 into 0x24 is the SUBJECT's 0x24, while
 * the decode indexes with the OTHER object's -- two different fields at one
 * offset, which is worth stating because the two lines sit three apart.
 *
 * `_walk_forward_info` is a named symbol, so this is the fourth table this
 * file addresses by name.
 */
extern uint32_t walk_forward_info[];    /* 0x0016ef6c */

void get_walk_info_f(MK3OBJ *obj)
{
    obj->field24 = 1;
    obj->field1c = (uint32_t)(uintptr_t)walk_forward_info;
    decode_walk_table(obj);
    walk_flip_reverse(obj);
}


/* ---------------------------------------------------------------- is_he_joy
 *
 * armv7 0x0005507c, thirty-two bytes.  **Complete.**
 *
 * `am_i_joy` through the opponent, and it RETURNS the answer where `am_i_shang`
 * only stores it. The opponent also goes into 0x1c on the way, which `am_i_joy`
 * does not do.
 *
 * The `it eq` writes zero and falls through; the other path writes one. Both
 * branches store to 0x5c and the return is a re-read of that slot rather than
 * of the register just written.
 */
long is_he_joy(MK3OBJ *obj)
{
    MK3OBJ  *him = obj->field00->field00;
    uint32_t flags;

    obj->field1c = (uint32_t)(uintptr_t)him;
    flags = him->field00->field10;
    obj->field2c = flags;
    obj->field5c = (flags & 1u) ? 1u : 0u;
    return (long)obj->field5c;
}


/* ------------------------------------------------------------- pose_him_a9
 *
 * armv7 0x0005a050, thirty-two bytes.  **Complete.**
 *
 * The same substitution `away_x_vel_him` performs -- 0x08 from the PROC's 0x04
 * and 0x00 from the opponent's PROC -- around `pose_a9_manual`. Two functions
 * with one trampoline written out twice rather than shared, which is how the
 * original has it.
 */
void pose_a9_manual(MK3OBJ *obj);

void pose_him_a9(MK3OBJ *obj)
{
    MK3OBJPROC *saved_proc  = obj->field00;
    MK3OBJ     *saved_other = obj->field08;

    obj->field08 = (MK3OBJ *)(uintptr_t)saved_proc->him;
    obj->field00 = saved_proc->field00->field00;

    pose_a9_manual(obj);

    obj->field08 = saved_other;
    obj->field00 = saved_proc;
}


/* ------------------------------------------------------------------ aborn4
 *
 * armv7 0x0005503c, thirty-six bytes.  **Complete.**
 *
 *      ground = target->field00->field40
 *      obj->field20 = ground
 *      obj->field28 = target->field08->field1c
 *      result = obj->field28
 *      if (result == 0) {
 *          obj->field24 =(int16_t)MK3_FIELD12(target->field08)
 *          if (obj->field24 == ground) result = 0; else result = 1;
 *      } else result = 1;
 *      obj->field5c = result
 *
 * Where `am_i_airborn`, `is_he_airborn` and their two `_px` spellings all end
 * up. Airborne unless a slot is clear AND the height equals the ground -- an
 * equality, not a comparison, so a fighter one unit below the floor reads as
 * airborne.
 *
 * `distance_off_ground` subtracts these same two numbers. This asks whether
 * they are the same.
 */
long aborn4(MK3OBJ *obj, MK3OBJ *target)
{
    uint32_t ground = target->field00->field40;
    uint32_t result;

    obj->field20 = ground;
    result = target->field08->field1c;
    obj->field28 = result;

    if (result == 0) {
        int32_t height =(int32_t)(int16_t)MK3_FIELD12(target->field08);

        obj->field24 = (uint32_t)height;
        if ((uint32_t)height != ground)
            result = 1;
    } else {
        result = 1;
    }

    obj->field5c = result;
    return (long)result;
}


/* --------------------------------------------------------------- create_fx_xy
 *
 * armv7 0x00057d98, thirty-six bytes.  **Complete.**
 *
 *      MKEvent_Add(4, obj->field1c, (y & 0xffff) | ((x & 0xffff) << 16),
 *                  proc->field08)
 *
 * Two coordinates packed into one word, high half from the first argument. The
 * third packing in this file after `create_fx` and `ochar_sound_c`, and the
 * only one that masks both halves first -- the others rely on the values
 * already fitting.
 */
void create_fx_xy(MK3OBJ *obj, uint32_t x, uint32_t y)
{
    uint32_t packed = (y & 0xffffu) | ((x & 0xffffu) << 16);

    MKEvent_Add(4, (long)obj->field1c, (long)packed, obj->field00->field08);
}


/* ------------------------------------------------------------ double_next_a9
 *
 * armv7 0x0005a178, thirty-six bytes.  **Complete.**
 *
 *      do_next_a9_frame(obj)
 *      saved = obj->field40
 *      obj->field40 = obj->field48
 *      do_next_a9_frame_pxob(obj, proc->field00, obj->field44)
 *      obj->field48 = obj->field40
 *      obj->field40 = saved
 *
 * The advance run twice: once on 0x40 and once on 0x48, the second through the
 * borrow-and-restore `get_his_a11_ani` uses. Both cursors move, which is what
 * "double" names.
 *
 * The second call takes its pair from the PROC and from 0x44 -- the same slot
 * `get_his_a11_ani` leaves the opponent in, which is why the two are usually
 * called together.
 *
 * It RETURNS `do_next_a9_frame_pxob`'s answer: nothing touches r0 after that
 * call, so the value falls through, the same way it does out of
 * `strike_check_ptr`. That went unnoticed while nothing read it.
 * `t_double_mframew` does -- it loops while the value is non-zero -- which
 * makes this "is there another frame", answered by the second cursor.
 */
long double_next_a9(MK3OBJ *obj)
{
    uint32_t saved;
    long r;

    do_next_a9_frame(obj);

    saved = obj->field40;
    obj->field40 = obj->field48;
    r = do_next_a9_frame_pxob(obj, obj->field00->field00,
                              (MK3OBJ *)(uintptr_t)obj->a10);
    obj->field48 = obj->field40;
    obj->field40 = saved;
    return r;
}


/* ------------------------------------------------------------ get_his_height
 *
 * armv7 0x000557a8, thirty-six bytes.  **Complete.**
 *
 *      highest_mpart_ob(obj, proc->him)      ; leaves the top in 0x1c
 *      obj->field20 = G[0xac] - obj->field1c
 *
 * The stage's ground from G at 0xac -- the same word `distance_from_ground`
 * reads -- minus the top of the opponent's bounding part. So height here is
 * measured downwards from the ceiling of that number, not upwards from zero.
 */
void get_his_height(MK3OBJ *obj)
{
    highest_mpart_ob(obj, (MK3OBJ *)(uintptr_t)obj->field00->him);
    obj->field20 = *(const uint32_t *)(G_BYTES + 0xac) - obj->field1c;
}


/* ------------------------------------------------------------ get_my_height
 *
 * armv7 0x000557e4, thirty-six bytes.  **Complete.**
 *
 * `get_his_height` with the other object instead of the opponent. Same two
 * steps: the top of the bounding part, subtracted from G's 0xac.
 */
void get_my_height(MK3OBJ *obj)
{
    highest_mpart_ob(obj, obj->field08);
    obj->field20 = *(const uint32_t *)(G_BYTES + 0xac) - obj->field1c;
}


/* ---------------------------------------------------------- ground_ochar_ob
 *
 * armv7 0x0005527c, thirty-six bytes.  **Complete.**
 *
 *      MK3_SET_FIELD12(obj, (G[0xac] - ochar_ground_offsets[obj->field24])
 *
 * The stage ground less a per-character offset, stored as a halfword into
 * 0x12. That is the field `ground_player` copies into and
 * `distance_off_ground` subtracts from -- so this is where the number they
 * pass around is computed, and the character index is 0x24 again.
 *
 * `_ochar_ground_offsets` is the fifth named table this file reaches.
 */
extern uint32_t ochar_ground_offsets[];  /* 0x0016ef04 */

void ground_ochar_ob(MK3OBJ *obj)
{
    uint32_t ground = *(const uint32_t *)(G_BYTES + 0xac);

    MK3_SET_FIELD12(obj, (ground - ochar_ground_offsets[obj->field24]));
}


/* ------------------------------------------------------------- init_anirate
 *
 * armv7 0x000553a0, thirty-six bytes.  **Complete.**
 *
 *      proc->field1c = obj->field1c
 *      proc->field20 = (obj->field1c == 0xfff) ? obj->field1c : 1
 *
 * Both branches write the rate; they differ only in the second store. 0xfff is
 * a sentinel that puts itself in 0x20 where every other value puts 1, so the
 * pair is "rate, and a counter that is normally one".
 *
 * The sentinel path re-reads 0x1c rather than reusing the register, which is
 * why the two branches are four instructions each instead of one and a
 * conditional move.
 */
void init_anirate(MK3OBJ *obj)
{
    uint32_t rate = obj->field1c;

    obj->field00->field1c = rate;
    if (rate == 0xfff)
        obj->field00->field20 = obj->field1c;
    else
        obj->field00->field20 = 1;
}


/* ----------------------------------------------------------------- isa5_px
 *
 * armv7 0x00055da0, thirty-six bytes.  **Complete.**
 *
 *      joystick_in_a0_px(obj, target)      ; 0x1c = G[index] & 0xf
 *      obj->field1c &= obj->field38        ; the direction mask
 *      obj->field5c = (obj->field1c != 0)
 *
 * The end of the stick chain, and every link of it was read on its own: the
 * constants 1/2/4/8 written into 0x38, the nibble taken out of G, and the AND
 * here. A test that any requested direction is held -- so asking for two at
 * once answers yes to either, which is what a mask means and what an
 * enumeration would not have allowed.
 */
long isa5_px(MK3OBJ *obj, MK3OBJ *target)
{
    joystick_in_a0_px(obj, target);
    obj->field1c &= obj->field38;
    obj->field5c = (obj->field1c != 0) ? 1u : 0u;
    return (long)obj->field5c;
}


/* -------------------------------------------------------- multi_adjust_xy_ob
 *
 * armv7 0x000554f8, thirty-six bytes.  **Complete.**
 *
 *      flags = target->field28
 *      obj->field2c = flags
 *      if (flags & 0x10) dx = -dx
 *      target->x0e += dx
 *      target y  += dy                    ; the halfword at 0x12
 *
 * What every `adjust_*`, `lineup_*` and `dragon` routine in this file has been
 * feeding. The facing bit negates the horizontal step -- the same bit
 * `flip_multi` toggles and `walk_flip_reverse` tests -- so a step is given in
 * the fighter's own terms and turned into the world's here.
 *
 * Both coordinates are halfwords and both are read and written unsigned, so
 * they wrap at 65536 rather than saturating.
 */
void multi_adjust_xy_ob(MK3OBJ *obj, uint32_t target_w, uint32_t dx, uint32_t dy)
{
    MK3OBJ  *target = (MK3OBJ *)(uintptr_t)target_w;
    uint32_t flags  = target->field28;

    obj->field2c = flags;
    if (flags & 0x10u)
        dx = (uint32_t)(-(int32_t)dx);

    MK3_SET_FIELD0E(target, (uint16_t)(MK3_FIELD0E(target) + dx));
    MK3_SET_FIELD12(target, (uint16_t)(MK3_FIELD12(target) + dy));
}


/* --------------------------------- leftmost_mpart_ob, rightmost_mpart_ob
 *
 * armv7 0x0005582c and 0x00055850, thirty-six bytes each.  **Complete.**
 *
 *      leftmost:   out->field24 = flipped ? x - src->field3c : x + src->field34
 *      rightmost:  out->field28 = flipped ? x - src->field34 : x + src->field3c
 *
 * A mirrored pair. The facing bit chooses BOTH which of 0x34 and 0x3c is the
 * bound and whether it is added or subtracted, and the two functions swap both
 * choices -- which is what makes them a left and a right rather than two
 * bounds.
 *
 * `x` is the signed halfword at 0x0e, and it is loaded identically in both
 * arms of each `itete`; the compiler predicated a common load rather than
 * hoisting it.
 *
 * They write different slots -- 0x24 and 0x28 -- so a caller can have both.
 */
void leftmost_mpart_ob(MK3OBJ *out, MK3OBJ *src)
{
    int32_t x = (int32_t)(int16_t)MK3_FIELD0E(src);

    if (src->field28 & 0x10u)
        out->field24 = (uint32_t)(x - (int32_t)src->field3c);
    else
        out->field24 = (uint32_t)(x + (int32_t)src->field34);
}

void rightmost_mpart_ob(MK3OBJ *out, MK3OBJ *src)
{
    int32_t x = (int32_t)(int16_t)MK3_FIELD0E(src);

    if (src->field28 & 0x10u)
        out->field28 = (uint32_t)(x - (int32_t)src->field34);
    else
        out->field28 = (uint32_t)(x + (int32_t)src->field3c);
}


/* ------------------------------------------------------------- update_tsl
 *
 * armv7 0x0005742c, thirty-six bytes.  **Complete.**
 *
 *      i = proc->field08
 *      obj->field20 = i                    ; overwritten below
 *      obj->field1c += i * 2
 *      obj->field20 = G[0xa8]
 *      *(uint16_t *)(base + i * 2) = G[0xa8]
 *
 * A halfword table indexed by the fighter number, taking its value from G at
 * 0xa8 -- four bytes below the 0xac that holds the stage ground. 0x1c is
 * advanced past the entry as well as being used as the base, so the caller
 * gets a cursor back.
 *
 * 0x20 is written twice and the first value is not read between them.
 */
void update_tsl(MK3OBJ *obj)
{
    uint32_t  i    = obj->field00->field08;
    uint32_t  base = obj->field1c;
    uint32_t  v;

    obj->field20 = i;
    obj->field1c = base + i * 2;

    v = *(const uint32_t *)(G_BYTES + 0xa8);
    obj->field20 = v;
    *(uint16_t *)(uintptr_t)(base + i * 2) = (uint16_t)v;
}


/* -------------------------------------------------------------- CountThreads
 *
 * armv7 0x000575e8, forty bytes.  **Complete.**
 *
 *      n = 0
 *      for (t = *head; t; t = t->next) if (t->pid == pid) n++;
 *      return n
 *
 * `FindThread` counting instead of stopping. The empty-list case returns
 * through a separate exit that moves the null into the counter, which is why
 * the function has two returns for one value.
 */
long CountThreads(uint32_t pid)
{
    const MK3THREAD *t = *ThreadListHead;
    long n = 0;

    while (t != NULL) {
        if (t->pid == pid)
            n++;
        t = t->next;
    }
    return n;
}


/* ------------------------ get_bcq_next_pointer_idx, get_jcq_next_pointer_idx
 *
 * armv7 0x000560e8 and 0x00056110, forty bytes each.  **Complete.**
 *
 *      base = G + (which ? 0x218 : 0x0c0)      ; bcq
 *      base = G + (which ? 0x26c : 0x114)      ; jcq
 *      obj->field34 = base
 *      obj->field38 = *base
 *
 * Four ring buffers inside G, two per name, with the argument choosing between
 * them -- one for each fighter, on the same reading that makes the strengths
 * at 0x368 a per-fighter array.
 *
 * The pair written out is the base and the head, which is exactly what
 * `previous_q_entry` walks. Three functions and one representation of a queue.
 */
void get_bcq_next_pointer_idx(MK3OBJ *obj, long which)
{
    char *base = G_BYTES + (which ? 0x218 : 0x0c0);

    obj->field34 = (uint32_t)(uintptr_t)base;
    obj->field38 = *(const uint32_t *)base;
}

void get_jcq_next_pointer_idx(MK3OBJ *obj, long which)
{
    char *base = G_BYTES + (which ? 0x26c : 0x114);

    obj->field34 = (uint32_t)(uintptr_t)base;
    obj->field38 = *(const uint32_t *)base;
}


/* ----------------------------------------------------------- get_walk_info_b
 *
 * armv7 0x000552f4, forty bytes.  **Complete.**
 *
 *      obj->field24 = 2
 *      obj->field1c = walk_backward_info
 *      decode_walk_table(obj)
 *      obj->field20 = -obj->field20
 *      walk_flip_reverse(obj)
 *
 * `get_walk_info_f` with a sign. The negate happens BEFORE the facing flip, so
 * the two compose: walking backwards while facing left is forwards in world
 * terms. Putting the negate after the flip would cancel one with the other,
 * which is the mistake this ordering is evidence against.
 */
extern uint32_t walk_backward_info[];   /* 0x0016f03c */

void get_walk_info_b(MK3OBJ *obj)
{
    obj->field24 = 2;
    obj->field1c = (uint32_t)(uintptr_t)walk_backward_info;
    decode_walk_table(obj);
    obj->field20 = (uint32_t)(-(int32_t)obj->field20);
    walk_flip_reverse(obj);
}


/* ------------------------------------------------------------- ground_ochar
 *
 * armv7 0x000552a0, forty bytes.  **Complete.**
 *
 *      off = ochar_ground_offsets[other->field24]
 *      obj->field1c = off
 *      obj->field20 = G[0xac] - off
 *      MK3_SET_FIELD12(other, obj->field20
 *
 * `ground_ochar_ob` with both intermediate values kept. That one computes the
 * same halfword and stores only it); this leaves the offset in 0x1c and the
 * result in 0x20 as well, so a caller can see either.
 */
void ground_ochar(MK3OBJ *obj)
{
    uint32_t off = ochar_ground_offsets[obj->field08->field24];

    obj->field1c = off;
    obj->field20 = *(const uint32_t *)(G_BYTES + 0xac) - off;
    MK3_SET_FIELD12(obj->field08, obj->field20);
}


/* ------------------------------------------------------------------ my_func
 *
 * armv7 0x00057290, forty bytes.  **Complete.**
 *
 *      t = &mytc[proc->field08]            ; stride 268
 *      return GetThreadFunc(t)
 *
 * The stride is what makes this worth writing out: `i*4 + i*64 - i`, scaled by
 * four, is 268 -- 0x10c -- and the thread's last known field is the proc at
 * 0x108. So the array stride is the struct size, which is the first size
 * established for MK3THREAD rather than the first offset.
 *
 * The rest is `GetThreadFunc` spelled out again -- index by 0xa4, scale by
 * eight, second word -- and the duplication is the original's, not this
 * transcription's.
 */
extern MK3THREAD *mytc;  /* slot 0x0016566c -> 0x0038ef3c; the host variable holds the slot's contents, as in mk3.c */

void *my_func(MK3OBJ *obj)
{
    MK3THREAD *t = (MK3THREAD *)((char *)mytc
                                 + obj->field00->field08 * MK3THREAD_STRIDE);

    return (void *)(uintptr_t)((const uint32_t *)((const char *)t + t->frame * 8))[1];
}


/* --------------------------------------------------------------- next_anirate
 *
 * armv7 0x0005a680, forty bytes.  **Complete.**
 *
 *      if (--proc->field20 > 0) return 0
 *      proc->field20 = proc->field1c
 *      if (*(uint32_t *)obj->field40 == 0) return 0
 *      return do_next_a9_frame(obj)
 *
 * The animation clock. `init_anirate` writes the rate into 0x1c and a counter
 * into 0x20; this decrements the counter, and when it reaches zero reloads it
 * from the rate and advances a frame -- unless the frame list has run out,
 * which is the same zero-terminator test `frame_a9` makes.
 *
 * The comparison is signed and `<= 0`, so a counter that starts at zero
 * reloads immediately rather than running four billion times. That is the
 * opposite of `find_part_a14`, which decrements before testing for equality
 * and does wrap.
 */
long next_anirate(MK3OBJ *obj)
{
    MK3OBJPROC *proc = obj->field00;

    proc->field20 = proc->field20 - 1;
    if ((int32_t)proc->field20 > 0)
        return 0;

    obj->field00->field20 = obj->field00->field1c;
    if (*(const uint32_t *)(uintptr_t)obj->field40 == 0)
        return 0;

    /* The original branches PAST its own `movs r0, #0` here, so this path
     * returns the callee's value and the two above return zero. */
    return do_next_a9_frame(obj);
}


/* ------------------------------------ pose_a9_manual and pose2_a9_manual
 *
 * armv7 0x0005a028 and 0x0005a000, forty bytes each.  **Complete.**
 *
 *      packed = obj->field40
 *      step   = ((int32_t)packed >> 16) * 4
 *      obj->field1c = step
 *      obj->field40 = (uint16_t)packed          ; the index alone
 *      get_char_ani(obj)                        ; or get_char_ani2
 *      obj->field1c = step                      ; again
 *      obj->field40 += step
 *      do_next_a9_frame(obj)
 *
 * 0x40 arrives packed: a frame index in the low half and a signed offset in
 * the high, scaled by four. The fetch runs on the index alone and the offset
 * is added to whatever it returns, so the pair is "which animation, and how
 * far into it".
 *
 * 0x1c is written twice with the same value, once on each side of the fetch.
 * `get_char_ani` does not touch it, so the second store is redundant as the
 * code stands -- the same shape as the 0x54 saves, and left alone for the same
 * reason.
 */
void pose_a9_manual(MK3OBJ *obj)
{
    uint32_t packed = obj->field40;
    uint32_t step   = (uint32_t)(((int32_t)packed >> 16) * 4);

    obj->field1c = step;
    obj->field40 = (uint16_t)packed;
    get_char_ani(obj);
    obj->field1c = step;
    obj->field40 = obj->field40 + step;
    do_next_a9_frame(obj);
}

void pose2_a9_manual(MK3OBJ *obj)
{
    uint32_t packed = obj->field40;
    uint32_t step   = (uint32_t)(((int32_t)packed >> 16) * 4);

    obj->field1c = step;
    obj->field40 = (uint16_t)packed;
    get_char_ani2(obj);
    obj->field1c = step;
    obj->field40 = obj->field40 + step;
    do_next_a9_frame(obj);
}


/* -------------------------------------------------------------------- randu
 *
 * armv7 0x00058714, forty bytes.  **Complete.**
 *
 *      saved = obj->field20
 *      obj->field20 = obj->field1c          ; the bound
 *      mk_random(obj)                       ; 0x1c = random32()
 *      mpyu(obj->field1c, obj->field20, &obj->field1c, NULL)
 *      obj->field20 = saved
 *      obj->field1c += 1
 *
 * A bound applied by multiply-high, not by modulo: the product's high word of
 * `random * bound` is uniform over 0..bound-1 without a division. `mpyu`'s
 * optional outputs are why one call does it -- the low half is discarded by
 * passing null, which is the only thing that test in `mpyu` is for.
 *
 * The result is 1..bound. It inherits `random32`'s missing low two bits: the
 * high word is fed by a value that is always a multiple of four, so the
 * distribution is not what a uniform generator would give. That is in the
 * generator, not here.
 *
 * 0x20 is borrowed for the bound and restored, the fifth restore in this file.
 */
void randu(MK3OBJ *obj)
{
    uint32_t saved = obj->field20;

    obj->field20 = obj->field1c;
    mk_random(obj);
    mpyu(obj->field1c, obj->field20, &obj->field1c, NULL);
    obj->field20 = saved;
    obj->field1c = obj->field1c + 1;
}


/* -------------------------------------------------------- rsnd_ochar_sound
 *
 * armv7 0x0005873c, forty bytes.  **Complete.**
 *
 *      packed = obj->field1c
 *      obj->field1c = (int16_t)packed              ; the range
 *      obj->field20 = (int32_t)packed >> 16        ; the minimum
 *      randu(obj)                                  ; 1..range
 *      obj->field1c += obj->field20 - 1
 *      ochar_sound(obj)
 *
 * `randu_minimum` inlined: it does the same `-1 + minimum` rather than calling
 * the routine that exists for it. Both halves are sign-extended, so a negative
 * minimum is expressible.
 */
void rsnd_ochar_sound(MK3OBJ *obj)
{
    uint32_t packed = obj->field1c;

    obj->field1c = (uint32_t)(int32_t)(int16_t)packed;
    obj->field20 = (uint32_t)((int32_t)packed >> 16);
    randu(obj);
    obj->field1c = obj->field1c - 1 + obj->field20;
    ochar_sound(obj);
}


/* ----------------------------------------------------- delete_slave_notproj
 *
 * armv7 0x00056d3c, forty-four bytes.  **Complete.**
 *
 *      slave = proc->field64
 *      if (!slave) return
 *      if ((slave->thread->pid - 0x700) <= 1) return        ; unsigned
 *      KillProc(slave)
 *      proc->field64 = 0
 *      proc->slave   = 0
 *
 * `delete_slave` with one test in front of it, and that test is what 0x700
 * means: pids 0x700 and 0x701 are the projectiles, one per fighter, and this
 * refuses to delete a slave that is one.
 *
 * It also settles the pair noted earlier. `ReallyKillProjectile` computes
 * `index + 0x700` and `ReallyKillHisProjectile` computes `0x700 - index + 1`;
 * for indices 0 and 1 those give 0x700/0x701 and 0x701/0x700, so the second is
 * the first with the fighters swapped. The two arithmetics were transcribed as
 * unrelated because nothing then said they were a pair. This does.
 */
void delete_slave_notproj(MK3OBJ *obj)
{
    MK3OBJ *slave = (MK3OBJ *)(uintptr_t)obj->field00->field64;

    if (slave == NULL)
        return;

    /* Unsigned, so a pid below 0x700 wraps large and passes. */
    if (slave->thread->pid - 0x700u <= 1u)
        return;

    KillProc(slave);
    obj->field00->field64 = 0;
    obj->field00->slave = 0;
}


/* ------------------------------------ get_rough_hypotenuse and _hypotenuse_of
 *
 * armv7 0x000571bc and 0x000571e8, forty-four bytes each.  **Complete.**
 *
 *      a = |dx|;  b = |dy|
 *      obj->field54 = max(a, b) + min(a, b) / 2
 *
 * The octagonal distance approximation: no multiply and no square root, about
 * eight per cent high on the diagonal. For deciding whether two fighters are
 * close enough that is the right trade, and it is why the name says rough.
 *
 * The `+ (x >>> 31)` the compiler emits before each shift is the rounding
 * fixup for a SIGNED divide by two. Both operands are already positive by then
 * so it adds nothing -- and it is the reason this is written as `/ 2` rather
 * than `>> 1`: the fixup is what says the source divided.
 *
 * The two differ in where the operands come from. `get_rough_hypotenuse` takes
 * them from 0x38 and 0x54 and writes the absolute values BACK into those slots;
 * `_of` takes them as arguments and makes them positive branchlessly with
 * `eor`/`sub`, touching nothing but the result.
 */
long get_rough_hypotenuse(MK3OBJ *obj)
{
    int32_t a = (int32_t)obj->field38;
    int32_t b = (int32_t)obj->field54;

    if (a < 0) { a = -a; obj->field38 = (uint32_t)a; }
    if (b < 0) { b = -b; obj->field54 = (uint32_t)b; }

    obj->field54 = (uint32_t)((a < b) ? (b + a / 2) : (a + b / 2));
    return (long)obj->field54;
}

long get_rough_hypotenuse_of(MK3OBJ *obj, int32_t dx, int32_t dy)
{
    int32_t a = (dx < 0) ? -dx : dx;
    int32_t b = (dy < 0) ? -dy : dy;

    obj->field54 = (uint32_t)((a >= b) ? (a + b / 2) : (b + a / 2));
    return (long)obj->field54;
}


/* ------------------------------------------------------------- group_sound
 *
 * armv7 0x000580a4, forty-four bytes.  **Complete.**
 *
 *      g = ochar_voice_groups[other->field24]
 *      if (g >= 0)
 *          MKEvent_Add(2, 1, (g << 4) + obj->field1c, 0)
 *
 * A per-character voice group, and a NEGATIVE entry means the character has
 * none -- the test is signed and the whole call is skipped. So the table is
 * sparse by sign rather than by a sentinel value.
 *
 * The group is shifted by four and added to 0x1c, the same
 * pack-two-things-in-a-word shape as `ochar_sound`. The consumer,
 * AddNewGameEvents (0x7368a-0x73696), unpacks it as
 * `get_gsound(arg & 0xf, arg >> 4, limeRand())`: the low nibble (0x1c) picks
 * the kind of sound -- the `group_table` row, attack/jump/grab/... -- and the
 * voice group picks this character's block of takes inside that row.
 *
 * `_ochar_voice_groups` is the sixth named table this file reaches.
 */
extern int32_t ochar_voice_groups[];    /* 0x0016f55c */

void group_sound(MK3OBJ *obj)
{
    int32_t group = ochar_voice_groups[obj->field08->field24];

    if (group < 0)
        return;

    MKEvent_Add(2, 1, (long)(((uint32_t)group << 4) + obj->field1c), 0);
}



/* ------------------------------------------------------------------ intersect
 *
 * armv7 0x00055fa8, forty-four bytes.  **Complete.**
 *
 *      !(a->left >= b->right || a->top    >= b->bottom ||
 *        b->left >= a->right || b->top    >= a->bottom)
 *
 * The standard four-comparison overlap test, and it is what establishes the
 * box: four words at 0x00, 0x04, 0x08 and 0x0c, left top right bottom, because
 * each is compared against the opposite field of the other box.
 *
 * Every comparison is `>=`, so two boxes that merely TOUCH do not intersect.
 * On the frame two hitboxes meet exactly that is the difference between a hit
 * and a miss, so it is stated rather than reproduced quietly.
 */
long intersect(const MK3BOX *a, const MK3BOX *b)
{
    if (a->left >= b->right)
        return 0;
    if (a->top >= b->bottom)
        return 0;
    if (b->left >= a->right)
        return 0;
    if (b->top >= a->bottom)
        return 0;
    return 1;
}


/* -------------------------------------------------------- set_x_vel_player
 *
 * armv7 0x00055a68, forty-four bytes.  **Complete.**
 *
 *      if (obj->field08 == G[0xbc])  G[0xb8]  = obj->field1c
 *      else if (obj->field08 == G[0x214]) G[0x210] = obj->field1c
 *
 * The velocity does not go on the object. It goes into a global slot chosen by
 * matching the object against the two that G already holds -- so a third
 * object silently stores nothing, which is the whole of the error handling.
 *
 * `away_x_vel`, `towards_x_vel` and `stop_me_player` all end here.
 */
void set_x_vel_player(MK3OBJ *obj)
{
    uint32_t who = (uint32_t)(uintptr_t)obj->field08;

    if (*(const uint32_t *)(G_BYTES + 0xbc) == who)
        *(uint32_t *)(G_BYTES + 0xb8) = obj->field1c;
    else if (*(const uint32_t *)(G_BYTES + 0x214) == who)
        *(uint32_t *)(G_BYTES + 0x210) = obj->field1c;
}


/* ------------------------------------------------------- set_winner_status
 *
 * armv7 0x00056afc, forty-four bytes.  **Complete.**
 *
 *      obj->field20 = (G[0x368] == 0) ? 2 : 1
 *      G[0x45c] = (uint16_t)obj->field20
 *
 * 0x368 is where `get_strength` reads from -- entry zero of the strengths --
 * and here it is tested against zero to pick between two and one. So the
 * winner status is 2 when the first strength is clear and 1 otherwise, and it
 * is published as a halfword at 0x45c, four bytes past the repell value
 * `sans_repell` writes.
 *
 * The 2 is formed as `0 + 2` from the zero just tested, which is the compiler
 * and not two constants.
 */
void set_winner_status(MK3OBJ *obj)
{
    obj->field20 = (*(const uint32_t *)(G_BYTES + 0x368) == 0) ? 2u : 1u;
    *(uint16_t *)(G_BYTES + 0x45c) = (uint16_t)obj->field20;
}


/* ------------------------------------------------------ am_i_close_to_edge
 *
 * armv7 0x0005718c, forty-eight bytes.  **Complete.**
 *
 *      get_my_dfe(obj)
 *      is_he_right(obj)
 *      if (!obj->field5c) obj->field30 = obj->field34
 *      obj->field1c = 0x40000
 *      obj->field5c = (obj->field30 <= 103)
 *
 * Two distances are computed and the facing picks which one is tested: the
 * fetch leaves one in 0x30 and `is_he_right` decides whether 0x34 replaces it.
 * So "close to the edge" means close to the edge BEHIND you, and which edge
 * that is depends on where the opponent stands.
 *
 * The threshold is 103, the same number `am_i_short` compares a height
 * against. Two unrelated quantities sharing a constant is worth noting and not
 * worth unifying.
 *
 * 0x1c takes 0x40000 -- 262144 -- and nothing here reads it back.
 */
void am_i_close_to_edge(MK3OBJ *obj)
{
    get_my_dfe(obj);
    is_he_right(obj);

    if (obj->field5c == 0)
        obj->field30 = obj->field34;

    obj->field1c = 0x40000;
    obj->field5c = ((int32_t)obj->field30 > 0x67) ? 0u : 1u;
}


/* ------------------------------------------------------- match_ani_points_ob_ob
 *
 * armv7 0x000555f0, forty-eight bytes.  **Complete.**
 *
 *      if (a == NULL || b == NULL) return
 *      b->field0c = a->field0c                     ; x, whole word
 *      b->field10 = a->field10                     ; y, whole word
 *      b->field28 = (b->field28 & ~0x10) | (a->field28 & 0x10)
 *
 * Both coordinates copied as WORDS, so the fractions come with them -- this is
 * the only routine here that moves a fighter without losing the sub-unit part.
 * And the facing bit is masked out of the destination and back in from the
 * source, which is what makes this "be where he is, facing the way he faces"
 * rather than a position copy.
 *
 * The null test is written as two comparisons and an `and`, which is `a && b`
 * compiled without short-circuiting: both pointers are tested whatever the
 * first says.
 */
void match_ani_points_ob_ob(uint32_t a_w, uint32_t b_w)
{
    MK3OBJ *a = (MK3OBJ *)(uintptr_t)a_w;
    MK3OBJ *b = (MK3OBJ *)(uintptr_t)b_w;

    if (a == NULL || b == NULL)
        return;

    b->field0c = a->field0c;
    b->field10 = a->field10;
    b->field28 = (b->field28 & ~0x10u) | (a->field28 & 0x10u);
}


/* ------------------------------------------------------------- ground_multi
 *
 * armv7 0x00057450, forty-eight bytes.  **Complete.**
 *
 *      lowest_mpart_ob(obj, obj->field08)          ; the bottom into 0x20
 *      obj->field1c = G[0xac] - obj->field20       ; how far above the ground
 *      obj->field20 = (int16_t)other_y + obj->field1c
 *      other_y = (uint16_t)obj->field20
 *
 * Drops the fighter until his lowest part sits on the stage ground: the gap is
 * measured once and added to the current y. `ground_player` does the same job
 * by copying a precomputed number; this one computes it from the bounding box,
 * which is why it needs the fetch first.
 *
 * The y is read signed and written as a halfword, so the fraction underneath
 * is left untouched -- a whole-unit move, like every other adjust here.
 */
void ground_multi(MK3OBJ *obj)
{
    lowest_mpart_ob(obj, obj->field08);

    obj->field1c = *(const uint32_t *)(G_BYTES + 0xac) - obj->field20;
    obj->field20 = (uint32_t)((int32_t)(int16_t)MK3_FIELD12(obj->field08)
                              + (int32_t)obj->field1c);
    MK3_SET_FIELD12(obj->field08, (uint16_t)obj->field20);
}


/* ------------------------------------------------------------- t_round_loop
 *
 * armv7 0x00056c54, forty-eight bytes.  **Complete.**
 *
 *      above = thread frame index + 1
 *      if (*above != 0 && *above != 0x12f8) return -3
 *      *above = 0x12f8
 *      thread->fieldfc = 1
 *      return 1
 *
 * `t_self_terminate` with one difference that matters: it accepts a frame
 * already marked 0x12f8 as well as an empty one, so calling it twice is
 * harmless where terminating twice is not. Its marker is 0x12f8 against the
 * terminator's 0x12ff, and it reports 1 rather than the literal 0x00016462.
 *
 * Two functions writing two different markers into the same slot is what makes
 * that slot a state and not a flag.
 */
long t_round_loop(MK3THREAD *thread)
{
    uint32_t seen = *mk3_frame(thread, thread->frame + 1);

    if (seen != 0 && seen != 0x12f8u)
        return -3;

    *mk3_frame(thread, thread->frame + 1) = 0x12f8u;
    thread->fieldfc = 1;
    return 1;
}


/* --------------------------------------------------------------- ani2_ob
 *
 * armv7 0x000584d4, fifty-two bytes.  **Complete.**
 *
 *      obj->field1c &= 0x3fff
 *      target->field2c = obj->field1c
 *      mk3_getbbox(obj->field1c,
 *                  &target->field34, &target->field38,
 *                  &target->field3c, &target->field40)
 *
 * The four out-pointers are the target's box. `GetFrameWidth` and
 * `GetFrameHeight` call the same function with four locals and read one each;
 * this one keeps all four, in the object, where the `*_mpart_ob` routines
 * expect them.
 *
 * The mask to fourteen bits happens before the call and is stored back, so
 * whatever 0x1c held is truncated for good rather than for the call.
 */
void ani2_ob(MK3OBJ *obj, MK3OBJ *target)
{
    obj->field1c &= 0x3fffu;
    target->field2c = obj->field1c;

    mk3_getbbox(obj->field1c,
                (int *)&target->field34, (int *)&target->field38,
                (int *)&target->field3c, (int *)&target->field40);
}


/* ------------------------------------------------------------------- gdfe4
 *
 * armv7 0x00054d8c, fifty-two bytes.  **Complete.**
 *
 *      d = obj->field28
 *      obj->field30 = |G[0xb0] - d|
 *      obj->field34 = |G[0xb4] + 0x18f - d|
 *
 * Distance from each edge of the arena: 0xb0 is one boundary and 0xb4 plus 399
 * the other, and both differences are made positive. So "dfe" is distance from
 * edge and the caller gets both, with `am_i_close_to_edge` picking between
 * them by the facing.
 *
 * The 0x18f is formed as `+0x18c` then `+3`, which is the compiler splitting an
 * immediate that does not fit one encoding rather than two constants.
 *
 * 0xb0 and 0xb4 sit just below the per-fighter block at 0xb8, so the arena
 * bounds are shared and the velocities are not.
 */
void gdfe4(MK3OBJ *obj)
{
    int32_t d = (int32_t)obj->field28;
    int32_t a = (int32_t)*(const uint32_t *)(G_BYTES + 0xb0) - d;
    int32_t b = (int32_t)*(const uint32_t *)(G_BYTES + 0xb4) + 0x18f - d;

    obj->field30 = (uint32_t)((a < 0) ? -a : a);
    obj->field34 = (uint32_t)((b < 0) ? -b : b);
}


/* ============================================== the frame-push family
 *
 * armv7 0x000560b4, 0x00056234, 0x000555bc and 0x00054e6c, fifty-two bytes
 * each.  **Complete.**
 *
 * Four functions, one body. Every instruction matches except the literal each
 * loads, and what they do is
 *
 *      if (frame[current + 1] != 0) return -3;
 *      frame[current].func = handler;
 *      frame[current + 1]  = 0;
 *      return 0;
 *
 * -- install a handler in the RUNNING frame and clear the one above it, so the
 * thread resumes as that handler on its next turn. Nothing runs here, which is
 * what "wake" means in two of the names.
 *
 * The guard is `t_self_terminate`'s: refuse if something is stacked on this
 * thread. Where terminate marks the frame above with 0x12ff and `t_round_loop`
 * with 0x12f8, these clear it -- three routines writing three things into one
 * slot, which is what makes it a state.
 *
 * Written out four times because the game calls four names. Folding them into
 * one helper with an argument would be a tidier file and a worse record of
 * what is in the binary.
 * ================================================================== */

long t_attk2(struct MK3THREAD *thread);
long t_master_mercy_entry(struct MK3THREAD *thread);
long t_multi_dummy_proc(struct MK3THREAD *thread);
long t_wait_forever(struct MK3THREAD *thread);



long t_attk5(MK3THREAD *thread)
{
    return mk3_push_handler(thread, (MK3THREADFUNC)t_attk2);
}

long t_continue_fighting(MK3THREAD *thread)
{
    return mk3_push_handler(thread, (MK3THREADFUNC)t_master_mercy_entry);
}

long t_multi_dummy_wake(MK3THREAD *thread)
{
    return mk3_push_handler(thread, (MK3THREADFUNC)t_multi_dummy_proc);
}

long t_wait_forever_wake(MK3THREAD *thread)
{
    return mk3_push_handler(thread, (MK3THREADFUNC)t_wait_forever);
}


/* ------------------------------------------------------------ StartGrObjAt
 *
 * armv7 0x00056cdc, fifty-six bytes.  **Complete.**
 *
 *      i = (grobj - GrObj) / 76
 *      StartProcAt(&Plyr[i], func)
 *
 * A pointer into one global array turned into the matching entry of another.
 * Both strides come out of the arithmetic and neither is stated anywhere:
 *
 *   - the divide is `>> 2` then a multiply by 0x286bca1b keeping the low 32
 *     bits, which is the modular-inverse trick for an exact division by an odd
 *     number. The inverse of that constant mod 2^32 is 19, so the divisor is
 *     4 * 19 = 76.
 *
 *   - the multiply back is `i*16 - i*4` and then `+ that*8`, which is 108.
 *
 * So GrObj entries are 76 bytes, Plyr entries are 108, and the arrays are
 * parallel. That difference is the reason this function exists.
 *
 * The second argument is not touched, so it reaches `StartProcAt` and then
 * `StartThreadAt` -- the function to start at, flowing through two wrappers.
 */
void StartGrObjAt(char *grobj, MK3THREADFUNC func)
{
    size_t i = (size_t)(grobj - GrObj) / GROBJ_STRIDE;

    StartProcAt((MK3OBJ *)(Plyr + i * PLYR_STRIDE), func);
}


/* ------------------------------------------------------------- advance_him
 *
 * armv7 0x00059e78, fifty-six bytes.  **Complete.**
 *
 *      save   obj->field00, obj->field08, obj->field40
 *      obj->field00 = proc->field00->field00      ; his PROC
 *      obj->field40 = obj->field48                ; his frame cursor
 *      obj->field08 = obj->a10                    ; and his object
 *      do_next_a9_frame(obj)
 *      result = obj->field40
 *      restore all three
 *      obj->field48 = result
 *
 * The trampoline pattern with three slots instead of two, and a result carried
 * back out. `get_his_a11_ani` and `double_next_a9` borrow 0x40 the same way;
 * this one borrows the whole triple so the advance happens entirely in the
 * opponent's terms.
 *
 * 0x44 supplies the object here, which is the slot `get_his_a11_ani` leaves
 * the opponent in -- the two are meant to be called in that order.
 */
void advance_him(MK3OBJ *obj)
{
    MK3OBJPROC *saved_proc  = obj->field00;
    MK3OBJ     *saved_other = obj->field08;
    uint32_t    saved_frame = obj->field40;
    uint32_t    result;

    obj->field00 = saved_proc->field00->field00;
    obj->field40 = obj->field48;
    obj->field08 = (MK3OBJ *)(uintptr_t)obj->a10;

    do_next_a9_frame(obj);

    result = obj->field40;
    obj->field08 = saved_other;
    obj->field00 = saved_proc;
    obj->field40 = saved_frame;
    obj->field48 = result;
}


/* ------------------------------------------------------------ damage_to_him
 *
 * armv7 0x00059670, fifty-six bytes.  **Complete.**
 *
 *      save   obj->field00, obj->field08, obj->a10
 *      obj->field08 = proc->him
 *      obj->field00 = proc->field00->field00       ; his PROC
 *      obj->a10     = obj->field1c
 *      obj->field10 = 0
 *      damage_to_me(obj)
 *      restore
 *
 * The trampoline with an argument passed through 0x44 -- the A10 slot doing
 * what its name says for once, carrying the amount into the call.
 *
 * The zero into 0x10 is on the SUBJECT, not the target. On a fighter 0x10 is
 * the y coordinate; on this object it is a slot like every other, which is
 * what these carrier objects are for. It is not restored.
 */
void damage_to_him(MK3OBJ *obj)
{
    MK3OBJPROC *saved_proc  = obj->field00;
    MK3OBJ     *saved_other = obj->field08;
    uint32_t    saved_a10   = obj->a10;

    obj->field08 = (MK3OBJ *)(uintptr_t)saved_proc->him;
    obj->field00 = saved_proc->field00->field00;
    obj->a10 = obj->field1c;
    obj->field10 = 0;

    damage_to_me(obj);

    obj->field08 = saved_other;
    obj->field00 = saved_proc;
    obj->a10 = saved_a10;
}


/* ----------------------------------------------------------------- randper
 *
 * armv7 0x000586dc, fifty-six bytes.  **Complete.**
 *
 *      chance = obj->field1c
 *      mk_random(obj)                                  ; 0x1c = random32()
 *      mpyu(1000, obj->field1c, &obj->field1c, NULL)   ; scale to 0..999
 *      obj->field5c = (chance > obj->field1c)
 *
 * The constant is 0x3e8 -- ONE THOUSAND. So what the name calls a percentage
 * is a per-mille chance, and a table entry of 50 is five per cent and not
 * fifty. A port that reads the name instead of the constant gets an AI that
 * almost never does anything, and nothing in the tables says which it is.
 *
 * Both 0x20 and 0x24 are borrowed for the working values and restored, so the
 * only lasting effects are 0x1c and the answer in 0x5c.
 *
 * It inherits `random32`'s missing low two bits through the same multiply-high
 * `randu` uses.
 *
 * **It also RETURNS the answer**, which this was first written without.
 * `bossrandper_org` is nothing but `return randper(obj)`, and the tail says
 * why that works:
 *
 *      movle r0, #0
 *      movgt r0, #1
 *      str   r0, [r4, #0x5c]           ; into the field
 *      pop   {r4, r5, r6, r7, pc}      ; and r0 still holds it
 *
 * The store is not the only output. Callers use both, and reading it as void
 * silently dropped the one the boss AI depends on.
 */
long randper(MK3OBJ *obj)
{
    uint32_t saved20 = obj->field20;
    uint32_t saved24 = obj->field24;
    uint32_t chance;
    long answer;

    obj->field24 = obj->field1c;
    mk_random(obj);

    obj->field20 = 1000;
    mpyu(1000, obj->field1c, &obj->field1c, NULL);

    chance = obj->field24;
    obj->field20 = saved20;
    obj->field24 = saved24;

    answer = ((int32_t)chance > (int32_t)obj->field1c) ? 1 : 0;
    obj->field5c = (uint32_t)answer;
    return answer;
}


/* -------------------------------------------------------------- t_fatal_no
 *
 * armv7 0x00056610, fifty-six bytes.  **Complete.**
 *
 * The frame-push family with one extra store: the thread's proc gets a zero at
 * 0x48 before the handler goes in. The handler is `t_fatal_yes`, so the pair
 * is a question already answered -- "no" installs the routine that runs when
 * the answer is yes, and the zero at 0x48 is what makes it a no.
 *
 * The zero comes from the frame-above test, which had to be zero to get here.
 * The compiler reuses that register rather than loading a constant, which is
 * why the store looks like it writes an arbitrary value.
 */
long t_fatal_yes(struct MK3THREAD *thread);

long t_fatal_no(MK3THREAD *thread)
{
    uint32_t current = thread->frame;
    char    *proc    = (char *)thread->proc;

    if (*mk3_frame(thread, current + 1) != 0)
        return -3;

    *(uint32_t *)(proc + 0x48) = 0;

    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_fatal_yes;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------- t_p1_won and t_round_tied
 *
 * armv7 0x0005643c and 0x000564c0, fifty-six and sixty bytes.  **Complete.**
 *
 * The frame-push family with a result written first:
 *
 *      t_p1_won:      proc->field48 = 0
 *      t_round_tied:  proc->field48 = 2
 *
 * and both install `t_prend`. So 0x48 is the round's outcome and t_prend is
 * what runs afterwards regardless -- the two functions differ only in what
 * they record.
 *
 * Two values in use and a gap at 1, which is very likely the other player
 * winning. Written down as a gap: nothing here shows it, and a third function
 * would.
 *
 * (`t_results_of_round`, further down, is that function. It dispatches on this
 * slot and sends 1 to `t_player_2_won`, so the guess was right and the gap is
 * closed. The note is kept as it was because the reasoning is the point.)
 *
 * `t_p1_won` takes its zero from the frame-above test, which had to be zero to
 * get past the guard; `t_round_tied` loads a 2. That is why one is fifty-six
 * bytes and the other sixty.
 */
long t_prend(struct MK3THREAD *thread);

long t_p1_won(MK3THREAD *thread)
{
    char *proc = (char *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    *(uint32_t *)(proc + 0x48) = 0;

    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_prend;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}

long t_round_tied(MK3THREAD *thread)
{
    char *proc = (char *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    *(uint32_t *)(proc + 0x48) = 2;

    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_prend;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ----------------------------------------------------------- react_xfer_him
 *
 * armv7 0x000589d4, sixty bytes.  **Complete.**
 *
 *      clear_queues(his_proc->field08)
 *      obj->field38 = reaction_table[obj->byte_at_0x49]
 *      xfer_otherguy(obj)
 *      his_proc->field10 |= 4
 *
 * A reaction chosen from a named table, installed in 0x38 -- the slot
 * `fastxfer_thread` reads a thread entry point out of -- and then the transfer
 * that uses it. So this is "make him react like this", and the three steps are
 * clear his queues, pick the routine, hand it over.
 *
 * The index is a BYTE at 0x49, inside the word at 0x48 that `t_p1_won` and
 * `t_round_tied` write the round result into. One offset, two widths, two
 * meanings -- the third time in this file.
 *
 * The final bit 2 into his 0x10 is the flag word `am_i_joy` reads bit 0 of and
 * `am_i_shang` bit 9. A third bit of it, and still no name for the word.
 *
 * `_reaction_table` is the seventh named table this file reaches.
 */
extern uint32_t reaction_table[];       /* 0x00166fc0 */
void clear_queues(uint32_t which);

void react_xfer_him(MK3OBJ *obj)
{
    MK3OBJPROC *his = obj->field00->field00->field00;

    clear_queues(his->field08);

    obj->field38 = reaction_table[((const uint8_t *)obj)[0x49]];
    xfer_otherguy(obj);

    obj->field00->field00->field00->field10 |= 4u;
}


/* ---------------------------------------------------------- RaiseTurboBars
 *
 * armv7 0x00057a90, sixty-four bytes.  **Complete.**
 *
 *      for (i = 0; i < 2; i++) {
 *          if (G[0x388 + i*4] != 0) { G[0x388 + i*4]--; continue; }
 *          if (G[0x378 + i*4] > 47)  continue;
 *          G[0x378 + i*4]++;
 *          MKEvent_Add(3, 5, the new value, i);
 *      }
 *
 * Two of each, walked with a stride of FOUR. Everything per-fighter found so
 * far in G has been 0x158 apart -- the velocities, the objects, the two ring
 * buffers -- so this is a second layout for the same idea and the first sight
 * of it.
 *
 * The countdown at 0x388 gates the raise: while it is non-zero it ticks down
 * and the bar does not move, so it is the delay between increments rather than
 * a timer on the bar itself. The bar stops at 48, and the event carries the new
 * value and which fighter it belongs to.
 *
 * No argument, like `no_edge_both_players`: both fighters are in the global.
 */
void RaiseTurboBars(void)
{
    int i;

    for (i = 0; i < 2; i++) {
        uint32_t *delay = (uint32_t *)(G_BYTES + 0x388 + i * 4);
        uint32_t *bar   = (uint32_t *)(G_BYTES + 0x378 + i * 4);

        if (*delay != 0) {
            *delay = *delay - 1;
            continue;
        }
        if ((int32_t)*bar > 0x2f)
            continue;

        *bar = *bar + 1;
        MKEvent_Add(3, 5, (long)*bar, (uint32_t)i);
    }
}


/* ------------------------------------------------------------- t_ship_proc
 *
 * armv7 0x00056164, sixty-four bytes.  **Complete.**
 *
 * The frame-push family with a table installed first: the PROC's 0x40 takes
 * `_a_ship` before the handler `t_fani3` goes into the frame.
 *
 * The 0x40 written here is an MK3OBJ's, not an MK3OBJPROC's -- `thread->proc`
 * points at the object. `t_attk3` and `pose_him_a0` read the same slot and
 * resolve it through `get_char_ani` when it is small, so 0x40 is the animation
 * and a table address is simply the large case. Nothing anomalous: an earlier
 * note here called it the ground, which is the PROC's 0x40 and a different
 * struct.
 *
 * `_a_ship` is the eighth named table this file reaches.
 */
extern uint32_t a_ship[];               /* 0x0016f61c */
long t_fani3(struct MK3THREAD *thread);

long t_ship_proc(MK3THREAD *thread)
{
    char *proc = (char *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    *(uint32_t *)(proc + 0x40) = (uint32_t)(uintptr_t)a_ship;

    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_fani3;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ---------------------------------------------------------- UnstackSwitches
 *
 * armv7 0x000573e8, sixty-eight bytes.  **Complete.**
 *
 *      while (G[0xa4] <= 31) {
 *          i  = G[0xa4]
 *          id = G[0x24 + i*4]
 *          G[0xa4] = i + 1
 *          entry = &swtab[id * 4]
 *          if ((int32_t)entry[0] < 0) continue
 *          QueueAndJump(entry, id)
 *      }
 *
 * One of the eight entry points this file's header lists, and the last of them
 * to be written. A stack of at most thirty-two queued switches in G at 0x24
 * with its cursor at 0xa4, and `_swtab` giving sixteen bytes for each.
 *
 * A negative first word in the table entry means "not this one": the cursor
 * still advances and the loop continues, so the skip is per entry and not a
 * break.
 *
 * The cursor is advanced BEFORE the jump. That is what lets `QueueAndJump`
 * push more switches without this loop re-running the one it is standing on --
 * the whole of the re-entrancy story, in one instruction, and the sort of
 * ordering a rewrite loses by tidying the increment to the end.
 *
 * The bound is unsigned (`bhi`, `bls`), so a cursor above 31 -- including one
 * that has gone negative -- ends the loop rather than indexing past the array.
 *
 * `_swtab` is the ninth named table this file reaches.
 */
extern uint32_t swtab[];                /* 0x0016f10c */
void QueueAndJump(const uint32_t *entry, uint32_t id);

void UnstackSwitches(void)
{
    while (*(const uint32_t *)(G_BYTES + 0xa4) <= 31u) {
        uint32_t  i  = *(const uint32_t *)(G_BYTES + 0xa4);
        uint32_t  id = *(const uint32_t *)(G_BYTES + 0x24 + i * 4);
        const uint32_t *entry = &swtab[id * 4];

        *(uint32_t *)(G_BYTES + 0xa4) = i + 1;

        if ((int32_t)entry[0] < 0)
            continue;

        QueueAndJump(entry, id);
    }
}


/* ------------------------------------------------------ am_i_facing_him_px
 *
 * armv7 0x000551ac, sixty-eight bytes.  **Complete.**
 *
 *      right = is_he_right(him)
 *      mine  = (him->field08->field28 & 0x10) != 0
 *      me->field5c = right ^ mine
 *
 * The compiler spells the exclusive-or as two branches -- `right && !mine` on
 * one side and `!right && mine` on the other -- which is why sixty-eight bytes
 * buy a single bit.
 *
 * The bit is worth the reading. `him->field08` is exactly the object
 * `is_he_right` treats as "me", so what this reads is MY 0x28 and not his. He
 * is to the right and my bit is clear: facing. He is to the left and my bit is
 * set: facing. So **bit 4 of 0x28 set means facing LEFT.**
 *
 * The flip_multi trio toggles that bit and `set_facing_from_0e` writes the word
 * it lives in; until now it was "the bit those three touch". It is a direction.
 *
 * `is_he_right` also writes 0x5c, so on the usual call where both arguments are
 * the same carrier the answer here lands on top of its answer. Two parameters
 * because the code reads two registers, not because the call sites need them
 * distinct.
 */
long am_i_facing_him_px(MK3OBJ *me, MK3OBJ *him)
{
    long     right = is_he_right(him);
    uint32_t mine  = him->field08->field28 & 0x10u;

    me->field5c = ((right != 0) != (mine != 0)) ? 1u : 0u;
    return (long)me->field5c;
}


/* ------------------------------------------------------------- pose_him_a0
 *
 * armv7 0x00059e34, sixty-eight bytes.  **Complete.**
 *
 *      save   obj->field00, obj->field08, obj->field40
 *      obj->field08 = proc->him
 *      obj->field00 = proc->field00->field00        ; his PROC
 *      obj->field40 = obj->field1c
 *      if (obj->field1c <= 255) get_char_ani(obj)
 *      do_next_a9_frame(obj)
 *      result = obj->field40
 *      restore the three
 *      proc->field00->field40 = result
 *
 * The trampoline again, but the compare in the middle is the find. A value at
 * or below 255 in 0x1c is a character-relative animation NUMBER, which
 * `get_char_ani` resolves into a pointer at 0x40; anything larger is already
 * the pointer and is used as it stands.
 *
 * That is a tagged union with no tag -- the range IS the tag -- and it is one
 * `cmp #0xff` deciding it. A port that always calls the resolver, or never
 * does, is wrong in one direction or the other and looks like a character whose
 * animations are somebody else's.
 *
 * The answer does not come back on the carrier: it goes into 0x40 of
 * `proc->field00`, the opponent's own object. So this poses HIM and leaves the
 * result where he keeps his frame, which is what separates it from
 * `advance_him` -- that one carries the result back to 0x48 of the carrier.
 */
void pose_him_a0(MK3OBJ *obj)
{
    MK3OBJPROC *saved_proc  = obj->field00;
    MK3OBJ     *saved_other = obj->field08;
    uint32_t    saved_40    = obj->field40;
    uint32_t    result;

    obj->field08 = (MK3OBJ *)(uintptr_t)saved_proc->him;
    obj->field00 = saved_proc->field00->field00;

    obj->field40 = obj->field1c;
    if ((int32_t)obj->field1c <= 0xff)          /* a number, not a pointer */
        get_char_ani(obj);

    do_next_a9_frame(obj);

    result = obj->field40;
    obj->field00 = saved_proc;
    obj->field08 = saved_other;
    saved_proc->field00->field40 = result;
    obj->field40 = saved_40;
}


/* ------------------------------------------------------------------ swscan
 *
 * armv7 0x00055e90, sixty-eight bytes.  **Complete.**
 *
 *      now  = G[0x1c]
 *      diff = G[0x20] ^ now
 *      G[0x20] = now
 *      if (diff == 0) return
 *      if (diff & now)  stack_switch_bits(now, diff & now,  1)
 *      if (diff & ~now) stack_switch_bits(now, diff & ~now, 0)
 *
 * The switch edge detector, and the other end of `UnstackSwitches`: this is
 * what fills the stack at G+0x24 that the unstacker walks.
 *
 * G+0x1c is the live switch word and G+0x20 the previous frame's copy, so the
 * pair is the standard "held" and "was held". `diff & now` are the bits that
 * just went on, `diff & ~now` the ones that just went off -- the compiler gets
 * the second with `bics`, which is why the two look asymmetric.
 *
 * **Presses are stacked before releases**, and both within the same frame. That
 * ordering is not incidental: a switch tapped and let go inside one frame
 * arrives as press-then-release rather than in whatever order the bits fall,
 * and anything reading the stack sees a coherent sequence.
 *
 * The copy at 0x20 is updated BEFORE either call, so a handler that reads the
 * switches sees the new state and not the one being reported.
 */
void stack_switch_bits(uint32_t now, uint32_t changed, uint32_t pressed);

void swscan(void)
{
    uint32_t now  = *(const uint32_t *)(G_BYTES + 0x1c);
    uint32_t diff = *(const uint32_t *)(G_BYTES + 0x20) ^ now;

    *(uint32_t *)(G_BYTES + 0x20) = now;

    if (diff == 0)
        return;

    if ((diff & now) != 0)
        stack_switch_bits(now, diff & now, 1);

    if ((diff & ~now) != 0)
        stack_switch_bits(now, diff & ~now, 0);
}




/* ------------------- t_prend, t_fatal_yes, t_finish_him_exit, t_its_a_tie
 *
 * armv7 0x000564fc, 0x00056648, 0x00056740 and 0x000568b0, sixty-eight bytes
 * each.  **Complete** -- and the same sixty-eight bytes four times.
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      if (frame > 0) { frame--; return 0; }
 *      frame[0].handler = t_local_reaction_exit
 *      frame[1].w0 = 0
 *      return 0
 *
 * A shape the frame family has not shown before: a POP. Every other member
 * installs a handler at the current level and leaves the index alone; this one
 * drops a level if there is one to drop, and only at the bottom does it install
 * anything. So these are returns, and `t_local_reaction_exit` is where a thread
 * that has run out of frames goes.
 *
 * The handler comes through the pointer slot at 0x000f3708 rather than as a
 * link-time constant -- the same slot `mkreact.c` reads.
 *
 * Four names for one body is not the compiler failing to fold them. A thread
 * is identified by the ADDRESS of its handler, so "is this the tie routine"
 * is answered by comparing a pointer; folding them would make four different
 * questions have the same answer. They must be four functions to be four
 * things, and they are written out four times here for the same reason.
 *
 * (`t_p1_won` and `t_round_tied` install `t_prend` after recording the result
 * at 0x48, so the outcome is read by whatever `t_local_reaction_exit` reaches
 * and not by this.)
 */
long t_local_reaction_exit(MK3THREAD *thread);

/* Drop one level, or at the bottom hand over to the local reaction exit.
 * Four routines end this way: the pop family, `t_white_flash`, `t_flight` and
 * `t_print_round_number`. The last three reach it after recognising their own
 * token rather than after finding the slot empty, which is the only difference
 * between them and why the guard is the caller's. */
static long mk3_unwind(MK3THREAD *thread)
{
    uint32_t current = thread->frame;

    if ((int32_t)current > 0) {
        thread->frame = current - 1;
        return 0;
    }

    mk3_frame(thread, current)[1] =
        (uint32_t)(uintptr_t)t_local_reaction_exit;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}

static long mk3_pop_or_exit(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_unwind(thread);
}

long t_prend(MK3THREAD *thread)          { return mk3_pop_or_exit(thread); }
long t_fatal_yes(MK3THREAD *thread)      { return mk3_pop_or_exit(thread); }
long t_finish_him_exit(MK3THREAD *thread){ return mk3_pop_or_exit(thread); }
long t_its_a_tie(MK3THREAD *thread)      { return mk3_pop_or_exit(thread); }


/* -------------------------------------------- t_striker and t_behind_striker
 *
 * armv7 0x000587f0 and 0x00058854, one hundred bytes each.  **Complete** --
 * and again one body at two addresses.
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      args[argc]     = proc->field1c
 *      args[argc + 1] = proc->field20
 *      argc += 2
 *      init_special(proc)
 *      frame[frame].handler = t_attk3
 *      frame[frame+1].w0 = 0
 *
 * The first sight of the thread's second stack being written, and what fixes
 * the layout above.
 *
 * The two words are the PROC's 0x1c and 0x20 -- the animation rate and its
 * counter -- handed to `t_attk3` as arguments rather than left where it would
 * have to know to look. So 0xa8 is an argument stack and 0xf8 counts it.
 *
 * `init_special` is `stop_me_player`, `ground_player`, `isp2`: stop, drop to
 * the floor, initialise. It runs BEFORE the handler is installed, so t_attk3
 * begins on a fighter already still and on the ground.
 *
 * The cursor is stored twice -- once after each push -- because the compiler
 * kept no running total. Not observable, transcribed because it is there.
 */
long t_attk3(MK3THREAD *thread);

static long mk3_striker_entry(MK3THREAD *thread, MK3THREADFUNC next)
{
    MK3OBJ  *proc = (MK3OBJ *)thread->proc;
    uint32_t argc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    argc = thread->fieldf8;
    *mk3_arg(thread, argc) = proc->field1c;
    thread->fieldf8 = argc + 1;
    *mk3_arg(thread, argc + 1) = proc->field20;
    thread->fieldf8 = argc + 2;

    init_special(proc);

    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)next;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}

/* Three entries, one body, two destinations. `t_upcut_striker` is the same
 * hundred bytes with `t_attk5` where the other two have `t_attk3` -- so the
 * handler is the only thing that varies and the helper takes it. */
long t_striker(MK3THREAD *thread)
{
    return mk3_striker_entry(thread, (MK3THREADFUNC)t_attk3);
}

long t_behind_striker(MK3THREAD *thread)
{
    return mk3_striker_entry(thread, (MK3THREADFUNC)t_attk3);
}

long t_upcut_striker(MK3THREAD *thread)
{
    return mk3_striker_entry(thread, (MK3THREADFUNC)t_attk5);
}


/* ------------------------------------------------------------------ t_attk3
 *
 * armv7 0x0005606c, seventy-two bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      if ((int32_t)obj->field40 <= 0x47) get_char_ani(obj)
 *      frame[frame].handler = t_attk5
 *      frame[frame+1].w0 = 0
 *
 * `pose_him_a0`'s tagged union again, with a different threshold: 0x47 here on
 * the object's 0x40, 0xff there on its 0x1c. Two slots hold "an animation,
 * either as a number or as a pointer" and each has its own boundary, so the
 * boundary belongs to the slot and is not a global constant. Seventy-one
 * animations reachable by number in this one.
 *
 * Except that `t_backwards_ani` reads the SAME slot and resolves at or below
 * **0x48**. One apart, on one field, in one file. Either 0x48 is a number
 * there and a pointer here, or one of the two is off by one; nothing in either
 * function says which, so both are transcribed as written and the
 * disagreement is left standing rather than smoothed over.
 *
 * The frame index is re-read after `get_char_ani`, so the call is allowed to
 * move it. Transcribed as the re-read rather than as a saved local.
 *
 * The chain runs t_striker -> t_attk3 -> t_attk5 -> t_attk2, one handler
 * installed per entry, which is how this scheduler spells a sequence.
 */
long t_attk3(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    if ((int32_t)obj->field40 <= 0x47)          /* a number, not a pointer */
        get_char_ani(obj);

    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_attk5;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------------------- t_fhs3
 *
 * armv7 0x0005668c, seventy-two bytes.  **Complete**, and it rewrites what the
 * rest of this family's first line means.
 *
 *      token = frame[frame+1].w0
 *      if (token == 0) {
 *          frame[frame+1].w0 = 0x1180
 *          thread->fieldfc = 1
 *          return 1
 *      }
 *      if (token != 0x1180) return -3
 *      frame[frame].handler = t_wait_fatality_finish
 *      frame[frame+1].w0 = 0
 *      return 0
 *
 * The slot above the current frame is a **resume token**. Until now every
 * member of the family had only tested it against zero, which reads as a busy
 * flag; this one stamps 0x1180 into it, sets 0xfc, and returns **1** -- a value
 * no other member returns.
 *
 * Read the family again with that in hand:
 *
 *   - zero means nobody is waiting there, so the caller may install;
 *   - a token means somebody is, and -3 is "not mine, I am not the one to
 *     run" rather than a failure;
 *   - returning 1 is "I have parked myself; come back".
 *
 * On the second entry the routine recognises its own stamp, clears it and
 * moves on. So a thread yields by writing a value only it will recognise, and
 * the scheduler needs to know nothing about who is waiting for what.
 *
 * 0x1180 is a tag as far as this function is concerned -- it is only ever
 * compared with itself. Whether the number also means something to whatever
 * reads 0xfc is not visible from here and is left open.
 *
 * 0xfc is the slot `NewThread` clears and `t_self_terminate` sets. Set here
 * too, on a park, which fits "this thread is not runnable" better than the
 * "terminated" the earlier note guessed at.
 */
long t_wait_fatality_finish(MK3THREAD *thread);

/* The park, shared by every member of the shape.
 *
 *      if (the slot above is empty)  stamp `token`, set 0xfc to `mask`,
 *                                    and return the mask
 *      if (it holds somebody else's) return -3
 *      otherwise clear it and install `next`
 *
 * Two numbers, and they are not the same kind of thing:
 *
 *   - `token` identifies WHERE the routine is parked -- a resume point, not
 *     just an owner. `t_fx_babality` parks twice in one function with 0xf28
 *     and then 0xf2a, and each entry recognises exactly one of them, so the
 *     token is that function's program counter and these routines are
 *     coroutines. The ones with a single park have a single resume point,
 *     which is why one value looked like an identity.
 *
 *   - `mask` says HOW LONG. It is stored at 0xfc and also returned, so the
 *     scheduler is told twice, in the thread and in the return.
 *
 *     It was called a mask because the first three values seen were single
 *     bits. `t_double_mframew` settles it: that routine writes `obj->field1c`
 *     there -- the animation rate, a runtime value -- and every other park
 *     writes a literal. A field that takes an arbitrary number from an
 *     animation is a duration, not a set of flags, and the constants
 *     elsewhere are durations too.
 *
 * That is the whole of this cooperative scheduler's blocking: no queue, no
 * wait list. A thread writes down what it wants and a value only it will
 * recognise, and is asked again later. */
static long mk3_park(MK3THREAD *thread, uint32_t token, uint32_t mask,
                     MK3THREADFUNC next)
{
    uint32_t current = thread->frame;
    uint32_t seen    = *mk3_frame(thread, current + 1);

    if (seen == 0) {
        *mk3_frame(thread, current + 1) = token;
        thread->fieldfc = mask;
        return (long)mask;              /* parked -- come back */
    }

    if (seen != token)
        return -3;                      /* somebody else is waiting here */

    mk3_frame(thread, current)[1] = (uint32_t)(uintptr_t)next;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}

long t_fhs3(MK3THREAD *thread)
{
    uint32_t seen = *mk3_frame(thread, thread->frame + 1);

    if (seen == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x1180;
        thread->fieldfc = 0x0001;
        return 0x0001;
    }
    if (seen != 0x1180)
        return -3;

    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_wait_fatality_finish;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* -------------------------------------------------- is_finish_him_allowed
 *
 * armv7 0x000578c0, seventy-six bytes.  **Complete.**
 *
 *      obj->field1c = GrObj[1].field24        ; read at 0x70
 *      obj->field20 = GrObj[0].field24        ; read at 0x24
 *      if either is 0x17, 0x18 or 0x19 -> 0
 *      else if (int16)G[0x45e] or (int16)G[0x460] -> 1
 *      else 0
 *
 * The two reads are the SAME FIELD of two GrObj entries: the offsets are 0x24
 * and 0x70, and 0x70 - 0x24 is 76 -- `GROBJ_STRIDE` exactly, arrived at from
 * the other direction. `StartGrObjAt` got that 76 out of a modular inverse the
 * compiler used to avoid a divide; this function reaches it by writing two
 * offsets down. Two independent derivations agreeing is what makes the stride
 * safe to build on.
 *
 * So a GrObj entry plus 0x24 is the character, and three consecutive values --
 * 0x17, 0x18, 0x19 -- forbid the finisher for whichever side holds one. Three
 * in a row at the end of a range is the shape of a roster's non-selectable
 * tail; which characters they are is not in this function and is not guessed
 * here.
 *
 * The two SIGNED halfwords at G+0x45e and G+0x460 are the **clock**: `t_clock3`
 * writes the low digit into the first and the high into the second, packs them
 * a nibble apart for the display, and calls `t_round_timeout` when both reach
 * zero. So "either non-zero" is "there is time left", and the finisher is
 * offered only while the clock is still running.
 *
 * That is why `t_round_timeout` clears them: not as a side effect but because
 * clearing them IS the clock reaching zero. Three functions read at different
 * times, and the meaning only appears with all three.
 *
 * The two character numbers are left behind in 0x1c and 0x20, written before
 * the test rather than after, so a caller can read them whatever the answer.
 */
long is_finish_him_allowed(MK3OBJ *obj)
{
    uint32_t a = *(const uint32_t *)(GrObj + 0x24 + GROBJ_STRIDE);
    uint32_t b = *(const uint32_t *)(GrObj + 0x24);
    uint32_t allowed;

    obj->field1c = a;
    obj->field20 = b;

    if (a == 0x18 || b == 0x18 || a == 0x19 || b == 0x19 ||
        a == 0x17 || b == 0x17)
        allowed = 0;
    else if (*(const int16_t *)(G_BYTES + 0x45e) != 0 ||
             *(const int16_t *)(G_BYTES + 0x460) != 0)
        allowed = 1;
    else
        allowed = 0;

    obj->field5c = allowed;
    return (long)allowed;
}


/* ------------------------------- t_multi_dummy_proc and t_wait_forever
 *
 * armv7 0x00055570 and 0x00054e48, seventy-six bytes each.  **Complete.**
 *
 * Two more parks, and having three of them is what separated the token from
 * the mask:
 *
 *      t_fhs3              token 0x1180   mask 0x0001
 *      t_multi_dummy_proc  token 0x05e6   mask 0x1000
 *      t_wait_forever      token 0x011d   mask 0x0040
 *
 * Each continues into its own wake routine, and both wake routines install the
 * parker again. `t_wait_forever` and `t_wait_forever_wake` are a two-function
 * loop with a park in the middle -- which is exactly what the name promises,
 * and it is the whole implementation. Nothing counts down; the thread simply
 * keeps asking and keeps being told to wait until something outside clears the
 * mask at 0xfc.
 *
 * `t_multi_dummy_proc` is the same loop with a different bit, which is what a
 * thread that exists only to hold a slot open would look like.
 *
 * These three are single bits, but 0xfc is not a bitmask: `t_fx_babality`
 * writes 0x60 and then 0x16462, and `t_double_mframew` writes the animation
 * rate straight out of the object. It is a duration.
 */

long t_multi_dummy_proc(MK3THREAD *thread)
{
    uint32_t seen = *mk3_frame(thread, thread->frame + 1);

    if (seen == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x05e6;
        thread->fieldfc = 0x1000;
        return 0x1000;
    }
    if (seen != 0x05e6)
        return -3;

    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_multi_dummy_wake;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}

long t_wait_forever(MK3THREAD *thread)
{
    uint32_t seen = *mk3_frame(thread, thread->frame + 1);

    if (seen == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x011d;
        thread->fieldfc = 0x0040;
        return 0x0040;
    }
    if (seen != 0x011d)
        return -3;

    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_wait_forever_wake;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------------ t_round_timeout
 *
 * armv7 0x00056474, seventy-six bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      G[0x460] = 0        ; halfwords
 *      G[0x45e] = 0
 *      obj->field48 = 3
 *      frame[frame].handler = t_prend
 *      frame[frame+1].w0 = 0
 *
 * A fourth value for the round result at 0x48, and the one that finally makes
 * the enumeration legible:
 *
 *      0  t_p1_won
 *      1  (no routine writes it -- still a gap)
 *      2  t_round_tied
 *      3  t_round_timeout
 *
 * and all four continue into `t_prend`, so the result is recorded here and
 * acted on further along.
 *
 * The two halfwords it zeroes are the pair `is_finish_him_allowed` requires to
 * be non-zero. So a round that runs out of time offers no finisher -- and the
 * two functions agree about that having been read independently, neither one
 * for the sake of the other. That is the check on both readings.
 *
 * The zero it writes comes from the frame-above test, which had to be zero to
 * get here; the compiler reuses the register rather than loading a constant,
 * which is why three different stores share one value.
 */
long t_round_timeout(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    *(int16_t *)(G_BYTES + 0x460) = 0;
    *(int16_t *)(G_BYTES + 0x45e) = 0;
    obj->field48 = 3;

    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_prend;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------------ adjust_damage
 *
 * armv7 0x00054c90, eighty bytes.  **Complete.**
 *
 *      f = (float)damage
 *      s = adjust_diff[RoundParam[0x0c]] * adjust_ladder[RoundParam[0x14]]
 *      r = (int32_t)(f * s)            ; vcvt truncates toward zero
 *      return r < 1 ? 1 : r
 *
 * Two scale tables, and the symbol table gives both extents exactly:
 *
 *      _adjust_diff    0x0016edfc .. 0x0016ee24   ten floats
 *      _adjust_ladder  0x0016ee24 .. 0x0016ee34   four floats
 *
 * and the values in the image are:
 *
 *      adjust_diff    1.0 ten times
 *      adjust_ladder  0.8, 1.0, 1.0, 1.0
 *
 * So **difficulty does not scale damage in this build** -- ten entries, all
 * 1.0, a table left in place with its effect removed. And the first ladder
 * step deals 80 per cent, the rest full.
 *
 * Four ladder entries is the second time that number has appeared:
 * `ladderorder_a1` reads the same RoundParam+0x14 and folds anything above 3
 * back to 1. That was read from a clamp and this from a symbol's extent, and
 * they agree.
 *
 * The multiply order is the compiler's: the two scales are multiplied together
 * FIRST and the damage applied to the product. Float multiplication is not
 * associative, so the grouping is transcribed rather than tidied.
 *
 * The clamp is a floor of 1, applied after the truncation. A hit that scales
 * to nothing still takes a point, which is what stops a weak attack against a
 * generous table from becoming free.
 */
extern float adjust_diff[10];           /* 0x0016edfc, ten floats, all 1.0 */
extern float adjust_ladder[4];          /* 0x0016ee24, 0.8 then three 1.0 */

long adjust_damage(long damage)
{
    const uint32_t *rp = (const uint32_t *)RoundParam;
    float scale = adjust_diff[rp[0x0c / 4]] * adjust_ladder[rp[0x14 / 4]];
    long  r = (long)((float)damage * scale);

    return r < 1 ? 1 : r;
}


/* --------------------------------------------------------- four_button_bits
 *
 * armv7 0x00057224, eighty bytes.  **Complete.**
 *
 *      if ((int16_t)proc->field7c == 0) return bits
 *      for each pair (high, low):
 *          if (bits & high) bits = (bits & ~(high | low)) | low
 *
 *      0x00010000 -> 0x0010
 *      0x00100000 -> 0x1000
 *      0x00020000 -> 0x0040
 *      0x00200000 -> 0x4000
 *
 * The four-button control scheme, and the gate at 0x7c is what turns it on --
 * the field this struct has called "the four-button gate" since the flag was
 * first seen being written, now with the routine that reads it.
 *
 * Each pair is cleared whole before the low bit is set, so the high bit does
 * not survive the translation and a consumer downstream sees only the ordinary
 * button. The masks are three literals in the pool -- 0xfffeffef, 0xffefefff,
 * 0xfffdffbf -- and the fourth pair is done with an immediate `bic` of
 * 0x00204000 instead, which is the same operation the assembler could spell
 * without a constant.
 *
 * This is the hook a gamepad wants. The port's controller work does not need
 * to invent a mapping: the game already has one, and it is four bits wide.
 *
 * The gate is read as a SIGNED halfword and compared with zero, so any
 * non-zero value enables it.
 */
uint32_t four_button_bits(MK3OBJ *obj, uint32_t bits)
{
    static const uint32_t pair[4][2] = {
        { 0x00010000u, 0x0010u },
        { 0x00100000u, 0x1000u },
        { 0x00020000u, 0x0040u },
        { 0x00200000u, 0x4000u },
    };
    int i;

    if ((int16_t)obj->field00->field7c == 0)
        return bits;

    for (i = 0; i < 4; i++)
        if ((bits & pair[i][0]) != 0)
            bits = (bits & ~(pair[i][0] | pair[i][1])) | pair[i][1];

    return bits;
}


/* --------------------------------------------------------------- slave_ani
 *
 * armv7 0x00058484, eighty bytes.  **Complete.**
 *
 *      slave = obj->field00->slave
 *      if (slave != NULL) {
 *          ani = script[0] & 0x3fff
 *          match_ani_points_ob_ob(obj->field08, slave)
 *          slave->field2c = ani
 *          mk3_getbbox(ani, &slave->box.left, &slave->box.top,
 *                           &slave->box.right, &slave->box.bottom)
 *          obj->a10 = slave
 *      }
 *      return script + 1
 *
 * The first function in this file that is a **script step**: it reads one word
 * from a cursor and returns the cursor advanced by four, whether or not it did
 * anything. So there is a stream of these somewhere and this is one opcode's
 * worth of work.
 *
 * The word is masked to fourteen bits for the animation, which leaves the top
 * eighteen for something this opcode does not look at. Not guessed here; a
 * second reader of the same word will say what they are.
 *
 * `mk3_getbbox` fills the object's box at 0x34..0x40 -- four out-parameters,
 * the fourth passed on the stack because ARM ran out of registers. That box is
 * the one `MK3BOX` describes, and this is the first sight of what writes it.
 *
 * 0x68 of the PROC is dereferenced as an object here for the first time, which
 * is what makes the name `slave` a pointer rather than a number.
 */
const uint32_t *slave_ani(MK3OBJ *obj, const uint32_t *script)
{
    MK3OBJ *slave = (MK3OBJ *)(uintptr_t)obj->field00->slave;

    if (slave != NULL) {
        uint32_t ani = script[0] & 0x3fffu;

        match_ani_points_ob_ob((uint32_t)(uintptr_t)obj->field08,
                               (uint32_t)(uintptr_t)slave);
        slave->field2c = ani;
        /* The box fields are unsigned here and the out-parameters signed;
         * GetFrameWidth's locals are what fixed the signedness and this
         * struct's what fixed the width. Cast rather than pick a side. */
        mk3_getbbox(ani, (int *)&slave->field34, (int *)&slave->field38,
                    (int *)&slave->field3c, (int *)&slave->field40);
        obj->a10 = (uint32_t)(uintptr_t)slave;
    }

    return script + 1;
}


/* ----------------------------------------------------------------- t_shake2
 *
 * armv7 0x00056df8, eighty bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field48 = obj->field24
 *      obj->field40 = obj->field1c & 0xffff
 *      obj->a10     = (obj->field1c >> 16) & 0xffff
 *      frame[frame].handler = t_shake3
 *      frame[frame+1].w0 = 0
 *
 * A packed word unpacked: 0x1c carries two halves and they go to different
 * slots -- the low one to 0x40, which is the animation, and the high one to
 * 0x44, which is the A10 register this port kept from the arcade.
 *
 * The high half is taken with an ARITHMETIC shift and then masked back to
 * sixteen bits, so the sign is shifted in and immediately thrown away. The
 * result is the unsigned high half either way; transcribed as the mask.
 *
 * The shake family is the arcade's screen shake, and `shake_a11` was already
 * seen passing 0x48 out as an event.
 */
long t_shake3(MK3THREAD *thread);

long t_shake2(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field48 = obj->field24;
    obj->field40 = obj->field1c & 0xffffu;
    obj->a10 = ((int32_t)obj->field1c >> 16) & 0xffffu;

    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_shake3;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ---------------------------------------------------------- do_ani_offset_xy
 *
 * armv7 0x0005551c, eighty-four bytes.  **Complete.**
 *
 *      dx = script[0]
 *      obj->field1c = dx
 *      obj->field20 = target->field28
 *      if (target->field28 & 0x10) obj->field1c = -dx      ; facing left
 *      x = him->field0e + obj->field1c
 *      obj->field28 = x;  target->field0e = (uint16_t)x
 *      dy = script[1]
 *      obj->field1c = dy
 *      y = him->field12 + dy
 *      obj->field28 = y;  target->field12 = (uint16_t)y
 *      return script + 2
 *
 * The second script step in this file, and the first that consumes more than
 * one word. It places the target at an offset from the opponent -- `him` being
 * `ref->field00->him`, the PROC's 0x04 -- one word per axis.
 *
 * **Only the horizontal offset flips.** Bit 4 of 0x28 is the facing that
 * `am_i_facing_him_px` gave a direction to, and it negates dx and never dy.
 * That asymmetry is why this is one routine with two unrolled halves rather
 * than a loop, and it is exactly what a rewrite loses: a mirrored character
 * whose limb lands in the right place vertically and the wrong one across.
 *
 * The two coordinates are written as HALFWORDS into 0x0e and 0x12 -- the high
 * halves of the words at 0x0c and 0x10, which is where this file's accessors
 * put x and y. The full sum is also left in 0x28 of the working object, so a
 * caller can see the untruncated value.
 *
 * The bases are read SIGNED and the sums stored back unsigned, so an object
 * offset far enough to the left of the opponent wraps rather than going
 * negative. Transcribed, not corrected.
 */
const uint32_t *do_ani_offset_xy(MK3OBJ *obj, MK3OBJ *ref, MK3OBJ *target,
                                 const uint32_t *script)
{
    MK3OBJ  *him = (MK3OBJ *)(uintptr_t)ref->field00->him;
    uint32_t dx = script[0];
    uint32_t dy;
    uint32_t v;

    obj->field1c = dx;
    obj->field20 = target->field28;
    if ((target->field28 & 0x10u) != 0)         /* facing left */
        obj->field1c = (uint32_t)(-(int32_t)dx);

    v = (uint32_t)((int32_t)MK3_FIELD0E(him) + (int32_t)obj->field1c);
    obj->field28 = v;
    MK3_SET_FIELD0E(target, v);

    dy = script[1];
    obj->field1c = dy;
    v = (uint32_t)((int32_t)MK3_FIELD12_S(him) + (int32_t)dy);
    obj->field28 = v;
    MK3_SET_FIELD12(target, v);

    return script + 2;
}


/* ----------------------------------------------------- t_master_mercy_entry
 *
 * armv7 0x00056268, eighty-eight bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 1
 *      G[0x44e] = (int16_t)1
 *      obj->field40 = 11
 *      obj->field48 = 9
 *      obj->a10     = 9
 *      frame[frame].handler = t_clock3
 *      frame[frame+1].w0 = 0
 *
 * Mercy: a halfword flag into G at 0x44e -- near, but not in, the pair at
 * 0x45e/0x460 that a timeout clears -- and three constants on the object.
 *
 * 11 goes to the animation slot at 0x40, which is well under the 0x47 that
 * `t_attk3` treats as "a number, resolve it", so it is an animation index and
 * not a pointer.
 *
 * The two nines are one register: the compiler loads 11, stores it, subtracts
 * 2 and stores the result twice. Three constants, two instructions -- and the
 * reason the disassembly looks as though 9 were derived from 11 rather than
 * written.
 *
 * `t_continue_fighting` installs this, and this installs `t_clock3`, so mercy
 * hands the round back to the clock.
 */
long t_clock3(MK3THREAD *thread);

long t_master_mercy_entry(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 1;
    *(int16_t *)(G_BYTES + 0x44e) = 1;
    obj->field40 = 11;
    obj->field48 = 9;
    obj->a10 = 9;

    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_clock3;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------------- clear_queues
 *
 * armv7 0x00058974, ninety-six bytes.  **Complete.**
 *
 *      base = which ? 0x218 : 0xc0
 *      init_1_q(G + base + 0x00)
 *      init_1_q(G + base + 0x54)
 *      init_1_q(G + base + 0xa8)
 *      init_1_q(G + base + 0xfc)
 *
 * There are **four** queues per fighter, not two. The pair already known --
 * bcq at 0xc0 and jcq at 0x114 -- are the first two of four, spaced 0x54
 * apart, so a queue is eighty-four bytes and the block runs 0xc0 to 0x210.
 *
 * The two bases are 0xc0 and 0x218, which differ by 0x158: `G_FIGHTER_STRIDE`
 * for the fifth time and from a fifth independent place. The block ending at
 * 0x210 also butts exactly against the second fighter's velocity at 0x210,
 * which is the shape a per-fighter record should have.
 *
 * The argument selects the fighter and nothing else; the two halves are
 * separate straight-line code rather than a loop over a computed base, so the
 * compiler was given two constants and not an index.
 */
void clear_queues(uint32_t which)
{
    char *base;

    if (which == 0) {
        base = G_BYTES;
        init_1_q(base + 0xc0);
        init_1_q(base + 0x114);
        init_1_q(base + 0x168);
        init_1_q(base + 0x1bc);
    } else {
        base = G_BYTES;
        init_1_q(base + 0x218);
        init_1_q(base + 0x26c);
        init_1_q(base + 0x2c0);
        init_1_q(base + 0x314);
    }
}


/* ---------------------------------------------------------- getobjectinsert
 *
 * armv7 0x00059bf0, one hundred bytes.  **Complete.**
 *
 *      z = getprc_z(obj)
 *      if (z == NULL) return NULL
 *      z->field08->field2c = -1
 *      z->thread->func    = t_multi_dummy_proc
 *      z->thread->field08 = 0
 *      match_ani_points_ob_ob(obj->field08, z->field08)
 *      obj->field3c = z->field08
 *      p = Plyr + z->thread->player * 108
 *      p->field3c = z->field08
 *      p->field40 = obj->field40
 *      p->field48 = obj->field48
 *      p->field44 = obj->a10
 *      return z
 *
 * The multiply is written as `n*16 - n*4` and then `+ that*8`, which is
 * n * 108: `PLYR_STRIDE`, reached for the third time and from a third
 * direction. `StartGrObjAt` got it from a shift-and-add, `is_finish_him_allowed`
 * got the GrObj one from two offsets, and this one arrives at 108 again.
 *
 * It also names the thread's 0x100: it is the index into Plyr, so a thread
 * knows which player entry is its own. That fills the last pad in MK3THREAD
 * before the pid.
 *
 * The animation set to -1 is on `z->field08`, not on z, and the handler
 * installed is `t_multi_dummy_proc` -- the park that waits on bit 0x1000. So
 * the inserted object is created parked, with no animation, and something else
 * wakes it.
 */
MK3OBJ *getprc_z(MK3OBJ *obj);

MK3OBJ *getobjectinsert(MK3OBJ *obj)
{
    MK3OBJ *z = getprc_z(obj);
    MK3THREAD *th;
    MK3OBJ *p;

    if (z == NULL)
        return NULL;

    th = z->thread;

    z->field08->field2c = 0xffffffffu;
    th->func = (MK3THREADFUNC)t_multi_dummy_proc;
    th->field08 = 0;

    match_ani_points_ob_ob((uint32_t)(uintptr_t)obj->field08,
                           (uint32_t)(uintptr_t)z->field08);

    obj->field3c = (uint32_t)(uintptr_t)z->field08;

    p = (MK3OBJ *)(Plyr + th->player * PLYR_STRIDE);
    p->field3c = (uint32_t)(uintptr_t)z->field08;
    p->field40 = obj->field40;
    p->field48 = obj->field48;
    p->a10     = obj->a10;

    return z;
}


/* -------------------------------------------------------- stack_switch_bits
 *
 * armv7 0x00055e2c, one hundred bytes.  **Complete**, and with `swscan` and
 * `UnstackSwitches` it accounts for the whole switch mechanism.
 *
 *      cursor = G[0xa4]
 *      for (i = 0; i < 32; i++)
 *          if (changed & (1u << i)) {
 *              id = pressed ? i : i + 32
 *              if ((int32_t)cursor > 0) G[0x24 + (--cursor)*4] = id
 *          }
 *      G[0xa4] = cursor
 *
 * The stack is filled **downward** from the cursor and consumed **upward** to
 * 31. That is why `UnstackSwitches` loops while the cursor is at or below 31
 * and increments: the two meet in the middle of a thirty-two entry array, and
 * the cursor is the boundary rather than a count.
 *
 * A press is id `i` and a release is `i + 32`, so `_swtab` has sixty-four
 * entries -- one per switch per edge -- of sixteen bytes each.
 *
 * When the cursor reaches zero the stack is full and further entries are
 * **dropped silently**: the loop keeps running and the store is simply
 * skipped. No error, no overwrite. Transcribed as it stands.
 *
 * The first parameter is DEAD. `swscan` passes the live switch word and r0 is
 * zeroed on the first instruction to become the bit counter. It is kept in the
 * signature because the call site passes it and the ABI is part of what is
 * being recorded.
 */
void stack_switch_bits(uint32_t now, uint32_t changed, uint32_t pressed)
{
    uint32_t cursor = *(const uint32_t *)(G_BYTES + 0xa4);
    uint32_t mask = 1u;
    uint32_t i;

    (void)now;                          /* zeroed on entry; never read */

    for (i = 0; i < 32; i++, mask <<= 1) {
        uint32_t id;

        if ((changed & mask) == 0)
            continue;

        id = pressed ? i : i + 32;

        if ((int32_t)cursor > 0)        /* full: dropped, not overwritten */
            *(uint32_t *)(G_BYTES + 0x24 + (--cursor) * 4) = id;
    }

    *(uint32_t *)(G_BYTES + 0xa4) = cursor;
}


/* ------------------------------------------------------------- stance_setup
 *
 * armv7 0x000553c4, one hundred bytes.  **Complete.**
 *
 *      obj->field00->field18 = 0x303       ; the action
 *      obj->field30 = obj->field40         ; save where the animation was
 *      obj->field40 = 0
 *      get_char_ani(obj)                   ; animation 0 is the stance
 *      obj->field38 = obj->field40
 *      obj->field1c = 6
 *      init_anirate(obj)
 *
 *      p = obj->field38                    ; follow the jumps
 *      while (p[0] == 8) p = (uint32_t *)p[2]
 *      p += 0x18 / 4                       ; past the header
 *      do { p++; } while (*p != 1)         ; to the terminator
 *
 *      saved = obj->field30
 *      if (saved <= (uint32_t)p && saved >= obj->field40)
 *          obj->field40 = saved
 *
 * Two things come out of this that nothing else has shown.
 *
 * **The animation script has a format.** Opcode 8 at word 0 is a jump whose
 * target is at word 2 -- followed in a loop, so a chain of them resolves. Past
 * that, an 0x18-byte header, and then a scan in words for the value 1, which
 * ends it. Each word read is left in 0x1c on the way past, which is how the
 * caller sees the last one.
 *
 * **The stance resumes.** The animation position is saved before the stance is
 * loaded and put back afterwards, but only if it falls between the script's
 * base and its terminator -- that is, only if it was already a position inside
 * this same stance. A fighter returning to stance from a stance carries on;
 * one arriving from anywhere else starts at the beginning. Both comparisons
 * are needed and each alone would be wrong.
 *
 * The comparison mixes an animation slot with an address, which is only
 * coherent because a resolved animation IS an address -- the large case of the
 * tagged union `t_attk3` tests with 0x47.
 *
 * 0x303 into the PROC's action is written before anything else and never
 * conditionally, so the stance is committed to before the script is read.
 */

void stance_setup(MK3OBJ *obj)
{
    const uint32_t *p;
    uint32_t saved;

    obj->field00->field18 = 0x303;

    obj->field30 = obj->field40;        /* where the animation stood */
    obj->field40 = 0;
    get_char_ani(obj);                  /* animation 0 is the stance */

    obj->field38 = obj->field40;
    obj->field1c = 6;
    init_anirate(obj);

    p = (const uint32_t *)(uintptr_t)obj->field38;
    while ((obj->field1c = p[0]) == 8) {        /* a jump: follow it */
        p = (const uint32_t *)(uintptr_t)p[2];
        obj->field38 = (uint32_t)(uintptr_t)p;
    }

    p = (const uint32_t *)((const char *)p + 0x18);      /* past the header */
    obj->field38 = (uint32_t)(uintptr_t)p;
    do {
        p++;
        obj->field38 = (uint32_t)(uintptr_t)p;
        obj->field1c = *p;
    } while (*p != 1);                          /* to the terminator */

    saved = obj->field30;
    if (saved <= (uint32_t)(uintptr_t)p && saved >= obj->field40)
        obj->field40 = saved;                   /* inside: carry on */
}


/* ----------------------------------------------------------- t_fx_babality
 *
 * armv7 0x00058040, one hundred bytes.  **Complete**, and it is a coroutine.
 *
 *      token == 0:      proc->field28 = 0x43
 *                       send_code_a3(proc)
 *                       park(token 0xf28, mask 0x60)
 *      token == 0xf28:  tsound_func(proc, 0x65)
 *                       park(token 0xf2a, mask 0x16462)
 *      otherwise:       return -3
 *
 * The first routine here that parks **twice in one function**, and it never
 * installs a handler at all. Each entry looks at the token, recognises exactly
 * one value, does that step's work and parks with the next.
 *
 * So the token is a resume point -- a program counter for a routine that is
 * written as a state machine because the language had no coroutines. 0xf28 and
 * 0xf2a are two apart, consecutive labels in whatever generated them.
 *
 * The two durations are 0x60 and 0x16462, and the second is how this routine
 * ENDS. The token it parks with, 0xf2a, is not one this function accepts -- it
 * would answer -3 to itself -- so the last step waits ninety-one thousand
 * ticks for a wake-up that cannot come, and something kills the thread first.
 *
 * `t_friendship_speech` and `t_round_intro_fx` finish the same way, on the same
 * duration and each with a token of its own that it rejects. That is how a
 * sequence ends when it must not unwind to its caller.
 *
 * 0x43 into the object's 0x28 is a code and 0x65 the sound; both are constants
 * this routine does not compute.
 */
long t_fx_babality(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field28 = 0x43;
        send_code_a3(obj);
        *mk3_frame(thread, thread->frame + 1) = 0xf28;
        thread->fieldfc = 0x60;
        return 0x60;
    }

    if (token != 0xf28)
        return -3;

    tsound_func(obj, 0x65);
    *mk3_frame(thread, thread->frame + 1) = 0xf2a;
    thread->fieldfc = 0x16462;
    return 0x16462;
}


/* ---------------------------------------------------------- t_fx_friendship
 *
 * armv7 0x00059950, one hundred bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      NewThread(proc, t_friendship_speech)
 *      NewThread(proc, t_ship_proc)
 *      proc->field40 = a_friend
 *      frame[frame].handler = t_fani3
 *      frame[frame+1].w0 = 0
 *
 * Two threads started on the same proc and then the caller continues as a
 * third -- the speech and the ship run alongside the animation rather than in
 * sequence. This is the first place in the file where one routine fans out.
 *
 * `proc->field40 = a_friend` is written AFTER `t_ship_proc` is created and
 * before it can have run: a new thread is queued, not entered. `t_ship_proc`
 * writes `a_ship` into the same slot when it does run, so the two do not
 * collide -- but the order is only safe because creation is not a call, and it
 * is transcribed in the order the code has it.
 *
 * `_a_friend` is the tenth named table this file reaches, and the second of
 * the animation tables that go into 0x40 whole.
 *
 * This settled `NewThread`'s signature: it is called here with a proc and a
 * function, and the wrappers that had made it look argumentless were only
 * forwarding.
 */
extern uint32_t a_friend[];             /* 0x0016f5c4 */
long t_friendship_speech(MK3THREAD *thread);

long t_fx_friendship(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    NewThread(obj, (MK3THREADFUNC)t_friendship_speech);
    NewThread(obj, (MK3THREADFUNC)t_ship_proc);

    obj->field40 = (uint32_t)(uintptr_t)a_friend;

    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_fani3;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------- t_player_1_won and t_player_2_won
 *
 * armv7 0x000567e8 and 0x0005684c, one hundred bytes each.  **Complete.**
 *
 *      obj->field20 = n                    ; 1 or 2
 *      G[0x45c] = (int16_t)n
 *      H[n - 1] += 1
 *      H[n + 1] += 1
 *      frame[frame].handler = t_results_retp
 *      frame[frame+1].w0 = 0
 *
 * The pair differ only in the number, and the number is 1 for the first player
 * and 2 for the second -- a one-based winner, not a zero-based index, which is
 * worth saying because the round RESULT two functions away is zero-based.
 *
 * Each bumps two counters in H, and they are the four words a match's end
 * clears: H[0] and H[2] for player 1, H[1] and H[3] for player 2. Two parallel
 * tallies per player, kept in one array interleaved by player rather than
 * grouped by tally.
 *
 * G+0x45c is a signed halfword, immediately below the pair at 0x45e and 0x460
 * that `is_finish_him_allowed` reads and `t_round_timeout` clears. Three
 * halfwords in a row, and this is the one that says who won.
 *
 * The compiler keeps the winner in a register and reuses it as the +1 for both
 * increments, which is why player 1's additions look like `add ip` and player
 * 2's like `adds #1`.
 */
long t_results_retp(MK3THREAD *thread);

static long mk3_player_won(MK3THREAD *thread, uint32_t who)
{
    MK3OBJ   *obj = (MK3OBJ *)thread->proc;
    uint32_t *h   = (uint32_t *)H;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field20 = who;
    *(int16_t *)(G_BYTES + 0x45c) = (int16_t)who;

    h[who - 1] += 1;
    h[who + 1] += 1;

    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_results_retp;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}

long t_player_1_won(MK3THREAD *thread) { return mk3_player_won(thread, 1); }
long t_player_2_won(MK3THREAD *thread) { return mk3_player_won(thread, 2); }


/* -------------------------------------------------------- t_results_of_round
 *
 * armv7 0x00056784, one hundred bytes.  **Complete**, and it closes the
 * enumeration this file has been assembling one writer at a time.
 *
 *      switch (obj->field48) {
 *      case 0:  next = t_player_1_won;  break;
 *      case 1:  next = t_player_2_won;  break;
 *      case 2:  next = t_its_a_tie;     break;
 *      default: next = t_clock_ran_out; break;
 *      }
 *      frame[frame].handler = next
 *      frame[frame+1].w0 = 0
 *
 * The reader of 0x48. The writers were found first and separately --
 * `t_p1_won` and `t_fatal_no` write 0, `t_round_tied` writes 2,
 * `t_round_timeout` writes 3 -- and the value 1 was left as a gap with a guess
 * beside it. This dispatcher sends 1 to `t_player_2_won`, which is the guess,
 * and it sends everything above 2 to `t_clock_ran_out`, which is why 3 works
 * without being named.
 *
 * So the enumeration is: 0 player one, 1 player two, 2 a tie, anything else
 * the clock. Four writers and one reader, read in that order, and the reader
 * is what makes the set closed rather than merely observed.
 *
 * The default arm is a range and not a value, so a fifth outcome would land on
 * the clock rather than crash -- and there may be no fifth.
 */
long t_clock_ran_out(MK3THREAD *thread);

long t_results_of_round(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    MK3THREADFUNC next;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    switch (obj->field48) {
    case 0:  next = (MK3THREADFUNC)t_player_1_won;  break;
    case 1:  next = (MK3THREADFUNC)t_player_2_won;  break;
    case 2:  next = (MK3THREADFUNC)t_its_a_tie;     break;
    default: next = (MK3THREADFUNC)t_clock_ran_out; break;
    }

    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)next;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------------ recharge_bars
 *
 * armv7 0x000581f4, one hundred and four bytes.  **Complete**, and it lays out
 * G's bar block.
 *
 *      G[0x378] = G[0x37c] = G[0x380] = G[0x384] = 48
 *      G[0x368] = G[0x36c] = G[0x370] = G[0x374] = 166
 *      MKEvent_Add(3, 0, G[0x368], 0)
 *      MKEvent_Add(3, 0, G[0x36c], 1)
 *      MKEvent_Add(3, 5, G[0x378], 0)
 *      MKEvent_Add(3, 5, G[0x37c], 1)
 *
 * Eight words set and four announced, which is what identifies them.
 * `RaiseTurboBars` caps its bar at 48 and sends event (3, 5) with the fighter
 * index -- exactly the last two calls here -- so:
 *
 *      0x368  0x36c   health, and 166 is full
 *      0x370  0x374   a second pair, set with them and never announced
 *      0x378  0x37c   turbo, and 48 is full
 *      0x380  0x384   a second pair, likewise
 *      0x388  0x38c   the turbo raise delay, from RaiseTurboBars
 *
 * Five pairs, four bytes apart within a pair and eight between them: the
 * per-fighter layout that is NOT the 0x158 block, laid out in full for the
 * first time.
 *
 * 166 is the arcade's full health. Nothing here depends on that and it is not
 * how the number was arrived at -- the events did that -- but it is the number.
 *
 * The two unannounced pairs are written from the same registers in the same
 * order and never read here. A displayed value against a real one is the usual
 * reason for such a pair; this function does not say so and it is left open.
 *
 * The event arguments are re-read from G rather than reused from the register
 * that was just stored, which is why the disassembly loads 0x36c and 0x378
 * back after writing them.
 */
void recharge_bars(void)
{
    uint32_t *g = (uint32_t *)G_BYTES;

    g[0x380 / 4] = 0x30;
    g[0x378 / 4] = 0x30;
    g[0x384 / 4] = 0x30;
    g[0x37c / 4] = 0x30;
    g[0x36c / 4] = 0xa6;
    g[0x368 / 4] = 0xa6;
    g[0x374 / 4] = 0xa6;
    g[0x370 / 4] = 0xa6;

    MKEvent_Add(3, 0, (long)g[0x368 / 4], 0);
    MKEvent_Add(3, 0, (long)g[0x36c / 4], 1);
    MKEvent_Add(3, 5, (long)g[0x378 / 4], 0);
    MKEvent_Add(3, 5, (long)g[0x37c / 4], 1);
}


/* ------------------------------------------------------ Endurance_ClearPlayer
 *
 * armv7 0x00058314, one hundred and four bytes.  **Complete.**
 *
 *      Endurance_ClearStruct(&GrObj[i], &GrObj[i],        76)
 *      Endurance_ClearStruct(&Plyr[i],  &Plyr[i]  + 0xc, 108)
 *      Endurance_ClearStruct(&Pp[i],    &Pp[i]    + 0xc, 140)
 *
 * One player's entry in three parallel arrays, and the three strides are the
 * whole content of the function. None is written as a multiply:
 *
 *      i*4 + i*16, minus i, times 4    ->  i * 76      GrObj
 *      i*16 - i*4, plus that*8         ->  i * 108     Plyr
 *      (i*4 + i*16)*8 minus that       ->  i * 140     Pp
 *
 * The first two agree with `StartGrObjAt` and `getobjectinsert`, which reached
 * 76 and 108 from a modular inverse and a shift-and-add in other functions
 * entirely. The third is new: `_Pp` at 0x0038dc9c, 140 bytes an entry.
 *
 * GrObj is cleared whole and the other two keep their first twelve bytes.
 * Whatever lives in those twelve survives a player being reset for endurance,
 * which is presumably an identity or a link the entry must not lose.
 */
void Endurance_ClearPlayer(uint32_t i)
{
    Endurance_ClearStruct(GrObj + i * GROBJ_STRIDE,
                          GrObj + i * GROBJ_STRIDE, GROBJ_STRIDE);

    Endurance_ClearStruct(Plyr + i * PLYR_STRIDE,
                          Plyr + i * PLYR_STRIDE + 0xc, PLYR_STRIDE);

    Endurance_ClearStruct(Pp + i * PP_STRIDE,
                          Pp + i * PP_STRIDE + 0xc, PP_STRIDE);
}


/* -------------------------------------------------------- finish_him_or_her
 *
 * armv7 0x00058eec, one hundred and eight bytes.  **Complete.**
 *
 *      loser = (G[0x368] != 0) ? &GrObj[1] : &GrObj[0]
 *      obj->field28 = loser
 *      c = loser->field24                      ; the character
 *      obj->field28 = c
 *      obj->field1c = is_female(c) ? 0x12 : 0x11
 *      create_fx(obj)
 *
 *      is_female(c):  c == 1 || c == 6 || c == 0xb ||
 *                     c == 0xf || c == 0x10 || c == 0x11
 *
 * The loser is chosen by health: G+0x368 is player one's bar -- named by
 * `recharge_bars`, which sets it to 166 and announces it -- so a zero there
 * means player one lost and their own entry is read. Otherwise the entry 0x4c
 * further on, which is `GROBJ_STRIDE`, so it is player two's.
 *
 * **Six characters** take the second effect, and the function's name says what
 * separates them. Six is how many women the roster has. That is corroboration
 * and not the derivation: which id is which character is not in this function
 * and is not guessed here.
 *
 * The two effect numbers differ by one, 0x11 and 0x12, so whatever consumes
 * 0x1c has them adjacent -- two entries of one table rather than two unrelated
 * constants.
 *
 * 0x28 is written twice: first the entry pointer, then the character read out
 * of it. The first store is dead. Transcribed because the second load reads
 * 0x28 back rather than using the register, which is what makes the first
 * store necessary to the compiler even though nothing else sees it.
 *
 * The comparisons are laid out as two (6 and 1, folded into one flag) and then
 * four in a chain, which is the compiler's ordering and not a grouping of the
 * characters.
 */

void finish_him_or_her(MK3OBJ *obj)
{
    const char *loser = GrObj;
    uint32_t c;

    if (*(const uint32_t *)(G_BYTES + 0x368) != 0)
        loser = GrObj + GROBJ_STRIDE;

    obj->field28 = (uint32_t)(uintptr_t)loser;      /* dead, but written */
    c = *(const uint32_t *)(loser + 0x24);
    obj->field28 = c;

    obj->field1c = (c == 6 || c == 1 || c == 0xb ||
                    c == 0xf || c == 0x10 || c == 0x11) ? 0x12 : 0x11;

    create_fx(obj);
}


/* -------------------------------------------------------- strike_check_box
 *
 * armv7 0x000595e4, one hundred and eight bytes.  **Complete.**
 *
 *      obj->field14 = obj->field18 = 0
 *      saved20 = obj->field20
 *      get_char_stk(obj)
 *      obj->field20 = saved20                      ; put it straight back
 *      obj->field54 = him->field30
 *      if (him->field30 & MK3F_NOCOL) { obj->field5c = 0; return; }
 *
 *      packed = obj->field24
 *      obj->field24 = (int32_t)saved20 >> 16       ; high half
 *      obj->field20 = (int16_t)saved20             ; low half
 *      obj->field2c = (int32_t)packed >> 16
 *      obj->field28 = (int16_t)packed
 *      obj->field1c += 0x10
 *      strike_check_regs(obj)
 *      restore obj->field1c, obj->a10, obj->field48
 *
 * Two packed words unpacked into four coordinates. Each half is taken by
 * shifting -- right sixteen for the high one, left sixteen and back for the
 * low -- so both are SIGN-EXTENDED and a box may sit at negative coordinates.
 * A mask would have been wrong.
 *
 * `get_char_stk` is called for its other effects and its clobbering of 0x20 is
 * undone on the next instruction, which is why 0x20 is saved before a call
 * that appears not to need it.
 *
 * The opponent's flag word is copied to 0x54 whether or not it matters, and
 * then bit 0x100 -- `MK3F_NOCOL` -- ends the routine with a clear answer. So
 * "no collision" is checked on the TARGET and not on the striker.
 *
 * 0x1c is advanced by sixteen for the call and put back afterwards, which
 * selects a second structure sixteen bytes on from the first. Three slots are
 * saved and restored around the call and 0x14, 0x18, 0x54 and the four
 * coordinates are left changed.
 */
long strike_check_regs(MK3OBJ *obj, const uint32_t *p);

void strike_check_box(MK3OBJ *obj)
{
    uint32_t saved20 = obj->field20;
    uint32_t saved1c, saved_a10, saved48, packed, flags;

    obj->field14 = 0;
    obj->field18 = 0;

    get_char_stk(obj);

    saved1c   = obj->field1c;
    obj->field20 = saved20;
    saved_a10 = obj->a10;
    saved48   = obj->field48;

    flags = ((MK3OBJ *)(uintptr_t)obj->field00->him)->field30;
    obj->field54 = flags;
    if ((flags & MK3F_NOCOL) != 0) {
        obj->field5c = 0;
        return;
    }

    packed = obj->field24;
    obj->field24 = (uint32_t)((int32_t)saved20 >> 16);
    obj->field20 = (uint32_t)(int32_t)(int16_t)saved20;
    obj->field2c = (uint32_t)((int32_t)packed >> 16);
    obj->field28 = (uint32_t)(int32_t)(int16_t)packed;

    obj->field1c = saved1c + 0x10;
    strike_check_regs(obj, (const uint32_t *)(uintptr_t)(saved1c + 0x10));

    obj->field1c = saved1c;
    obj->a10     = saved_a10;
    obj->field48 = saved48;
}


/* --------------------------------------------------- t_wait_fatality_finish
 *
 * armv7 0x000566d4, one hundred and eight bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = (int16_t)G[0x450]
 *      if (obj->field1c == -1)          next = t_finish_him_exit
 *      else if (--obj->field48 <= 0)    next = t_finish_him_exit
 *      else                             next = t_fhs3
 *      frame[frame].handler = next
 *      frame[frame+1].w0 = 0
 *
 * The tick that pairs with `t_fhs3`'s park. Together they are the wait: park,
 * come back here, decrement, park again -- until the counter at 0x48 runs out
 * or the signed halfword at G+0x450 goes to -1.
 *
 * Two conditions and one destination between them. The -1 is an abort from
 * outside; the counter is the ordinary end. Both reach `t_finish_him_exit` and
 * the code has two separate loads of the same address for them, which is how
 * the disassembly ends up with the branch target twice.
 *
 * The counter is written back before it is tested, so a zero left in 0x48 goes
 * to -1 and still leaves. `ble` and not `beq`, which is what makes that safe.
 *
 * 0x48 is the round result in the routines around this one. Here it is a
 * countdown on the same object -- another slot doing two jobs, and the third
 * such in this file.
 */
long t_wait_fatality_finish(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    MK3THREADFUNC next;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = (uint32_t)(int32_t)*(const int16_t *)(G_BYTES + 0x450);

    if ((int32_t)obj->field1c == -1) {
        next = (MK3THREADFUNC)t_finish_him_exit;
    } else {
        obj->field48 = obj->field48 - 1;
        next = ((int32_t)obj->field48 <= 0)
                   ? (MK3THREADFUNC)t_finish_him_exit
                   : (MK3THREADFUNC)t_fhs3;
    }

    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)next;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------------ t_white_flash
 *
 * armv7 0x00057a24, one hundred and eight bytes.  **Complete.**
 *
 *      token == 0:      MKEvent_Add(3, 6, 0, 0)
 *                       park(token 0x1549, mask 6)
 *      token == 0x1549: if (frame > 0) { frame--; return 0; }
 *                       frame[0].handler = t_local_reaction_exit
 *                       frame[1].w0 = 0
 *      otherwise:       return -3
 *
 * The first routine to put both halves together: it parks like `t_fhs3` and
 * then returns like `t_prend`, so one function is the whole of a step --
 * announce, wait, unwind.
 *
 * Event (3, 6) with three zeroes. `recharge_bars` sends (3, 0) for health and
 * (3, 5) for turbo, both with a value and a player; this one carries nothing,
 * so 6 is an event that needs no argument.
 *
 * The mask is 6, which is 2 | 4 and the first park in this file to write more
 * than one bit -- and it is still not what 0xfc holds after `t_fx_babality`.
 */
long t_white_flash(MK3THREAD *thread)
{
    uint32_t current = thread->frame;
    uint32_t token   = *mk3_frame(thread, current + 1);

    if (token == 0) {
        MKEvent_Add(3, 6, 0, 0);
        *mk3_frame(thread, current + 1) = 0x1549;
        thread->fieldfc = 6;
        return 6;
    }

    if (token != 0x1549)
        return -3;

    return mk3_unwind(thread);
}


/* -------------------------------------------------------------- get_tsl_px
 *
 * armv7 0x00054cf0, one hundred and sixteen bytes.  **Complete.**
 *
 *      table = obj->field1c
 *      if ((int16_t)proc->field7c != 0) {
 *          if (table == G + 0x3b8) table = G + 0x3b4
 *          if (table == G + 0x3ac) table = G + 0x3a8
 *      }
 *      v = ((const uint16_t *)table)[proc->field08]
 *      obj->field1c = v
 *      if (v == 0) { obj->field20 = 0x780; return; }
 *      if (v & 0x8000) obj->field1c = v | 0xffff0000       ; sign-extend
 *      if (obj->field20 & 0x8000) obj->field20 |= 0xffff0000   ; dead
 *      obj->field20 = G[0xa8] - obj->field1c
 *
 * **The second four-button hook.** `four_button_bits` folds the button bits;
 * this swaps the table they index, and both are gated on the PROC's 0x7c. So
 * there are four tables in G at 0x3a8, 0x3ac, 0x3b4 and 0x3b8 -- two pairs,
 * each pair four bytes apart -- and the four-button scheme takes the lower of
 * each pair.
 *
 * The comparison is against the ADDRESS, not an index: the caller has already
 * chosen a table and this substitutes one for another. A port that reorders
 * those four words in G breaks this even though nothing indexes them.
 *
 * The lookup is by halfword, indexed by the PROC's 0x08 -- the strength or
 * fighter index -- and the result is signed. `ldrh` cannot sign-extend, so the
 * compiler tests bit 15 and ORs in 0xffff0000, which is the same value both
 * times because it is the same literal in the pool.
 *
 * It does that twice, and **the second is dead**: 0x20 is sign-extended and
 * then immediately overwritten by `G[0xa8] - field1c` on both paths. An
 * artefact of the same expansion appearing twice; transcribed with the store
 * that makes it visible.
 *
 * A zero entry means something else entirely -- 0x20 gets 0x780 and the
 * subtraction never happens -- so zero is not a value in this table, it is a
 * hole.
 */
void get_tsl_px(MK3OBJ *obj, MK3OBJ *ref)
{
    MK3OBJPROC     *proc  = ref->field00;
    const uint16_t *table = (const uint16_t *)(uintptr_t)obj->field1c;
    uint32_t v;

    if ((int16_t)proc->field7c != 0) {          /* the four-button gate */
        if ((const char *)table == G_BYTES + 0x3b8)
            table = (const uint16_t *)(G_BYTES + 0x3b4);
        if ((const char *)table == G_BYTES + 0x3ac)
            table = (const uint16_t *)(G_BYTES + 0x3a8);
    }

    v = table[proc->field08];
    obj->field1c = v;

    if (v == 0) {                               /* a hole, not a value */
        obj->field20 = 0x780;
        return;
    }

    if ((v & 0x8000u) != 0)
        obj->field1c = v | 0xffff0000u;

    if ((obj->field20 & 0x8000u) != 0)          /* dead: overwritten below */
        obj->field20 |= 0xffff0000u;

    obj->field20 = *(const uint32_t *)(G_BYTES + 0xa8) - obj->field1c;
}


/* ------------------------------------- t_animate_a9 and t_animate2_a9
 *
 * armv7 0x000556ac and 0x00055724, one hundred and twenty bytes each.
 * **Complete**, and one body with one call swapped.
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      packed = obj->field40
 *      obj->field1c = (int32_t)packed >> 16        ; high half, signed
 *      args[argc++] = obj->field1c
 *      obj->field40 = (uint16_t)packed             ; low half, unsigned
 *      get_char_ani(obj)           ; get_char_ani2 in the other
 *      obj->field1c = args[--argc]
 *      frame[frame].handler = t_mframew
 *      frame[frame+1].w0 = 0
 *
 * 0x40 arrives packed: an animation in the low half and a number in the high
 * one. The halves are taken with different signedness -- an arithmetic shift
 * for the high, `ldrh` for the low -- so the high one may be negative and the
 * low one is a plain animation index, which is what `get_char_ani` wants.
 *
 * The push and the pop straddle the resolver, which writes 0x1c. So the
 * argument stack at 0xa8 is doing duty as a save area for a single word: push,
 * call, pop. That is what a stack is for and why it has its own cursor rather
 * than being a fixed set of slots.
 *
 * The two functions differ only in the resolver, so the pairing is
 * `get_char_ani` against `get_char_ani2` and nothing else -- whatever the 2
 * means, it does not change how the caller is set up or where it goes next.
 *
 * Both continue into `t_mframew`.
 */
long t_mframew(MK3THREAD *thread);

static long mk3_animate_a9(MK3THREAD *thread, void (*resolve)(MK3OBJ *))
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t packed, argc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    packed = obj->field40;
    obj->field1c = (uint32_t)((int32_t)packed >> 16);

    argc = thread->fieldf8;
    *mk3_arg(thread, argc) = obj->field1c;
    thread->fieldf8 = argc + 1;

    obj->field40 = (uint16_t)packed;
    resolve(obj);

    argc = thread->fieldf8 - 1;
    thread->fieldf8 = argc;
    obj->field1c = *mk3_arg(thread, argc);

    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}

long t_animate_a9(MK3THREAD *thread)
{
    return mk3_animate_a9(thread, get_char_ani);
}

long t_animate2_a9(MK3THREAD *thread)
{
    return mk3_animate_a9(thread, get_char_ani2);
}


/* --------------------------------------------------------- strike_check_ptr
 *
 * armv7 0x00059424, one hundred and twenty-four bytes.  **Complete.**
 *
 *      obj->field14 = arg
 *      obj->field18 = 0
 *      save 0x1c, 0x20, 0x24, 0x28, 0x2c, 0x44, 0x48
 *      if (him->field30 & MK3F_NOCOL) { obj->field5c = 0; return; }
 *      obj->field20 = p[0]
 *      obj->field24 = p[1]
 *      obj->field28 = p[2]
 *      obj->field2c = p[3]
 *      strike_check_regs(obj)
 *      restore all seven
 *
 * `strike_check_box`'s sibling. Same four coordinate slots, same
 * `strike_check_regs`, same short-circuit on the TARGET's NOCOL bit -- the
 * difference is entirely where the numbers come from: four consecutive words
 * behind a pointer here, two packed words unpacked there.
 *
 * So the four slots at 0x20..0x2c are the box `strike_check_regs` reads, and
 * two callers fill them two ways. That is what makes them a box rather than
 * four numbers that happen to be adjacent.
 *
 * Seven slots are saved and put back, against three in the box version -- 0x44
 * and 0x48 as well, because this one is called with a live A10 the caller wants
 * afterwards. 0x14 and 0x18 are written and NOT restored, as in the sibling.
 *
 * The compiler reads the four words with a post-incrementing load and then
 * three offsets from two different registers, which makes the disassembly look
 * like it walks the pointer. It does not: the four are p[0] to p[3].
 *
 * The `adds r1, #0xc` afterwards is NOT dead -- an earlier note here said it
 * was, before `strike_check_regs` had been read. It leaves `p + 4` in r1, and
 * that is the second argument: this routine reads the first four words and
 * hands the next three on.
 *
 * The prototype was already here: two callers written earlier do
 * `return strike_check_ptr(obj, obj->field1c, flag)`, which fixed the shape
 * before the body was read and turns out to fit -- the second argument is the
 * pointer, arriving as a word.
 *
 * What it returns is whatever is left in r0. On the ordinary path that is
 * `strike_check_regs`' result, untouched by the restores that follow; on the
 * NOCOL path r0 still holds the argument. Both are written out rather than
 * picked between, because the function does not choose either.
 */
long strike_check_ptr(MK3OBJ *obj, uint32_t what, long arg)
{
    const uint32_t *p = (const uint32_t *)(uintptr_t)what;
    uint32_t s1c = obj->field1c, s20 = obj->field20, s24 = obj->field24;
    uint32_t s28 = obj->field28, s2c = obj->field2c;
    uint32_t s44 = obj->a10,     s48 = obj->field48;
    uint32_t flags;
    long r;

    obj->field14 = (uint32_t)arg;
    obj->field18 = 0;

    flags = ((MK3OBJ *)(uintptr_t)obj->field00->him)->field30;
    if ((flags & MK3F_NOCOL) != 0) {
        obj->field5c = 0;
        return (long)what;              /* r0 still holds the argument */
    }

    obj->field20 = p[0];
    obj->field24 = p[1];
    obj->field28 = p[2];
    obj->field2c = p[3];

    r = strike_check_regs(obj, p + 4);

    obj->field20 = s20;
    obj->field24 = s24;
    obj->field28 = s28;
    obj->field2c = s2c;
    obj->field48 = s48;
    obj->field1c = s1c;
    obj->a10     = s44;
    return r;
}


/* ------------------------------------------------------ t_friendship_speech
 *
 * armv7 0x00057fc0, one hundred and twenty-eight bytes.  **Complete**, and the
 * longest coroutine here so far: three resume points.
 *
 *      token 0      tsound_func(proc, 0x68)   park(0xf69, 0x0040)
 *      token 0xf69  tsound_func(proc, 0x69)   park(0xf70, 0x0040)
 *      token 0xf70  tsound_func(proc, 0x6a)   park(0xf78, 0x16462)
 *      otherwise    return -3
 *
 * Three sounds played in order with a wait between each, written as one
 * function that is entered four times. `t_fx_babality` had two steps; this has
 * three, and the shape is the same -- the token says where, the mask says what
 * to wait for.
 *
 * The sounds are 0x68, 0x69, 0x6a: consecutive, so they are three entries of
 * one table and almost certainly three lines of one speech.
 *
 * The first two waits are 0x40 ticks, the same as `t_wait_forever`. The last is
 * 0x16462 with the token 0xf78, which this function does not accept -- it
 * would answer -3 to itself. So the third step never wakes: it waits ninety-one
 * thousand ticks and is killed in the meantime, which is how a sequence ends
 * without unwinding. `t_fx_babality` and `t_round_intro_fx` end identically.
 *
 * `t_fx_friendship` starts this alongside `t_ship_proc` and then becomes
 * `t_fani3` itself, so the speech runs beside the animation rather than in it.
 */
long t_friendship_speech(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0xf69) {
        tsound_func(obj, 0x69);
        *mk3_frame(thread, thread->frame + 1) = 0xf70;
        thread->fieldfc = 0x40;
        return 0x40;
    }

    if (token == 0xf70) {
        tsound_func(obj, 0x6a);
        *mk3_frame(thread, thread->frame + 1) = 0xf78;
        thread->fieldfc = 0x16462;
        return 0x16462;
    }

    if (token != 0)
        return -3;

    tsound_func(obj, 0x68);
    *mk3_frame(thread, thread->frame + 1) = 0xf69;
    thread->fieldfc = 0x40;
    return 0x40;
}


/* ----------------------------------------------------------------- t_flight
 *
 * armv7 0x00054fb8, one hundred and thirty-two bytes.  **Complete**, and it
 * settles what the slot above the current frame is.
 *
 *      token == 0:
 *          obj->field34 = 0
 *          frame[frame+1].w0 = 0x205
 *          frame = frame + 1                   ; <- a PUSH
 *          frame[frame].handler = t_flight_call
 *          frame[frame+1].w0 = 0
 *          return 0
 *      token == 0x205:  unwind
 *      otherwise:       return -3
 *
 * The first routine in this file to **increment** the frame index. Everything
 * else either leaves it alone or drops it by one.
 *
 * Look at the order: the token is written into the slot the child is about to
 * occupy, and then the child is pushed on top of it. So when the child unwinds
 * and this routine runs again, it finds its own token exactly one level up --
 * which is how it knows the call it made is the thing that just finished.
 *
 * With that, every use of that slot is one idea:
 *
 *      zero          nothing above me; I may install
 *      my token      what I started is above me, or has just left
 *      anything else somebody else's, so -3 and let them run
 *
 * And it separates two shapes that had looked alike. `mk3_push_handler`
 * replaces the handler at the CURRENT level -- a tail call, the routine gives
 * up its place. `t_flight` pushes a new level -- a call, the routine keeps its
 * place and expects to come back. The port needs both and they differ by one
 * increment.
 *
 * The zero into the PROC's 0x34 comes from the token test, which had to be
 * zero to get here.
 */
long t_flight_call(MK3THREAD *thread);

long t_flight(MK3THREAD *thread)
{
    MK3OBJPROC *proc  = (MK3OBJPROC *)thread->proc;
    uint32_t    token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x205)
        return mk3_unwind(thread);

    if (token != 0)
        return -3;

    *(uint32_t *)((char *)proc + 0x34) = 0;

    *mk3_frame(thread, thread->frame + 1) = 0x205;
    thread->frame = thread->frame + 1;                  /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_flight_call;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ---------------------------------------------------- t_print_round_number
 *
 * armv7 0x00057e6c, one hundred and thirty-two bytes.  **Complete.**
 *
 *      token == 0:
 *          round = RoundParam[0x10]
 *          obj->field1c = round
 *          if (round <= 4) {
 *              obj->field1c = round + 0x10
 *              tsound_func(obj, round + 0x10)
 *          }
 *          park(0x103c, 0x30)
 *      token == 0x103c:  unwind
 *      otherwise:        return -3
 *
 * The round number is spoken for rounds one to four and **not at all from five
 * on** -- the wait still happens, so the pause is there and the voice is not.
 * Four recordings, 0x11 to 0x14, and a fifth round that passes in silence.
 *
 * RoundParam+0x10 is the round; +0x14 is the ladder order `ladderorder_a1`
 * reads and +0x0c the difficulty `adjust_damage` indexes with. Three fields of
 * that struct now, all small integers about the current match.
 *
 * 0x11 and 0x12 are also the two `finish_him_or_her` chooses between, but that
 * pair goes to `create_fx` through 0x1c and these go to `tsound_func`. Two
 * tables, and the overlap is a coincidence of small numbers.
 */
long t_print_round_number(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    uint32_t round;

    if (token == 0x103c)
        return mk3_unwind(thread);

    if (token != 0)
        return -3;

    round = ((const uint32_t *)RoundParam)[0x10 / 4];
    obj->field1c = round;

    if ((int32_t)round <= 4) {                  /* five and up are silent */
        obj->field1c = round + 0x10;
        tsound_func(obj, round + 0x10);
    }

    *mk3_frame(thread, thread->frame + 1) = 0x103c;
    thread->fieldfc = 0x30;
    return 0x30;
}


/* ------------------------------------------------------- t_wait_for_his_dog
 *
 * armv7 0x0005783c, one hundred and thirty-two bytes.  **Complete.**
 *
 *      token == 0:       park(0x17f6, 1)
 *      token == 0x17f6:  get_his_dog(obj)
 *                        if ((int32_t)obj->field1c > (int32_t)obj->a10)
 *                            frame[frame].handler = t_wait_for_his_dog
 *                        else
 *                            unwind
 *      otherwise:        return -3
 *
 * The first **polling loop** here: on the second entry it re-installs ITSELF
 * at the current level, so the next tick runs it again with the slot above
 * cleared -- back to the park, back to here, until the condition fails.
 *
 * Installing yourself is not the same as staying: the token above is cleared
 * on the way, so the routine re-parks each time round rather than spinning.
 * One tick per poll, which is the only rate a cooperative scheduler has.
 *
 * The condition compares two of the object's own slots, both signed, after
 * `get_his_dog` has filled them. Nothing else in this file compares 0x1c
 * against 0x44.
 */

long t_wait_for_his_dog(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x17f6;
        thread->fieldfc = 1;
        return 1;
    }

    if (token != 0x17f6)
        return -3;

    get_his_dog(obj);

    if ((int32_t)obj->field1c <= (int32_t)obj->a10)
        return mk3_unwind(thread);

    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_wait_for_his_dog;    /* poll again */
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ----------------------------------------------------------------- t_clock3
 *
 * armv7 0x00057de4, one hundred and thirty-six bytes.  **Complete**, and it is
 * the round clock.
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      G[0x45e] = (uint16_t)obj->a10           ; the low digit
 *      G[0x460] = (uint16_t)obj->field48       ; the high one
 *      MKEvent_Add(3, 1, (obj->field48 << 4) + obj->a10, 0)
 *      if ((int32_t)obj->field48 <= 1) tsound_func(obj, 0x17)
 *      obj->field1c = obj->field48 + obj->a10
 *      if (obj->field1c == 0) next = t_round_timeout
 *      else { obj->field3c = obj->field40; next = t_clock4; }
 *
 * Two digits, kept in two slots of the object and mirrored into two halfwords
 * of G. The event packs them a nibble apart -- `high * 16 + low` -- which is
 * how a two-digit display takes them in one word.
 *
 * The warning sound fires while the high digit is 0 or 1, so it starts at ten
 * remaining and keeps sounding, once per tick of this routine, rather than
 * firing once.
 *
 * Zero on BOTH digits ends the round, and it is tested as a sum rather than
 * two comparisons -- which works only because neither digit is ever negative.
 *
 * This is what `is_finish_him_allowed` was reading: the pair it needs non-zero
 * is the time left. And it is what `t_round_timeout` clears -- not a side
 * effect, but the clock reaching zero written down. Three functions read at
 * three different times, and none of them alone says any of this.
 *
 * On the ordinary path 0x40 is copied to 0x3c before `t_clock4`, which is the
 * animation slot being handed on.
 */
long t_clock4(MK3THREAD *thread);

long t_clock3(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    MK3THREADFUNC next;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    *(int16_t *)(G_BYTES + 0x45e) = (int16_t)obj->a10;
    *(int16_t *)(G_BYTES + 0x460) = (int16_t)obj->field48;

    MKEvent_Add(3, 1, (long)((obj->field48 << 4) + obj->a10), 0);

    if ((int32_t)obj->field48 <= 1)             /* ten seconds and under */
        tsound_func(obj, 0x17);

    obj->field1c = obj->field48 + obj->a10;

    if (obj->field1c == 0) {
        next = (MK3THREADFUNC)t_round_timeout;
    } else {
        obj->field3c = obj->field40;
        next = (MK3THREADFUNC)t_clock4;
    }

    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)next;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------------ t_mercy_start
 *
 * armv7 0x0005975c, one hundred and thirty-six bytes.  **Complete.**
 *
 *      token == 0:
 *          back_to_normal(obj)
 *          face_opponent(obj)
 *          disable_all_buttons(obj)
 *          frame[frame+1].w0 = 0x17ec
 *          frame = frame + 1                   ; a call, not a tail call
 *          frame[frame].handler = t_wait_for_start
 *          frame[frame+1].w0 = 0
 *      token == 0x17ec:  install t_local_reaction_exit (slot 0x000f3708)
 *      otherwise:        return -3
 *
 * The second routine to push a level, and the shape reads as a call now that
 * `t_flight` has established it: stamp the token where the child will sit,
 * push, and recognise it coming back.
 *
 * Three calls before the wait, and the order is the whole of what mercy does
 * to the fighter -- return to normal, turn to face the opponent, take the
 * controls away. The controls come back somewhere past `t_wait_for_start`;
 * nothing here gives them back, which is what makes this a call rather than a
 * sequence.
 *
 * `disable_all_buttons` is in another translation unit -- 0x0002ec68, well
 * below this file -- so the button state is not private to the fight logic.
 */
long t_wait_for_start(MK3THREAD *thread);

long t_mercy_start(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    /* not an unwind: the binary installs the handler in pointer slot
     * 0x000f3708 (t_local_reaction_exit) at this level (difftest) */
    if (token == 0x17ec)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

    if (token != 0)
        return -3;

    back_to_normal(obj);
    face_opponent(obj);
    disable_all_buttons(obj);

    *mk3_frame(thread, thread->frame + 1) = 0x17ec;
    thread->frame = thread->frame + 1;                  /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_wait_for_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------ t_print_timeout_msg
 *
 * armv7 0x00057ae4, one hundred and thirty-six bytes.  **Complete.**
 *
 *      token == 0:
 *          obj->field28 = 0;      send_code_a3(obj)
 *          obj->field28 = 0x21b;  send_code_a3(obj)
 *          MKEvent_Add(3, 0xc, 0, 0)
 *          park(0x186b, 0x50)
 *      token == 0x186b:  unwind
 *      otherwise:        return -3
 *
 * Two codes sent through the same slot, one after the other: a zero and then
 * 0x21b. The zero is not a clear -- it is a code in its own right, sent the
 * same way, because `send_code_a3` reads 0x28 each time it is called.
 *
 * Event (3, 0xc) carries nothing, like the (3, 6) `t_white_flash` sends. So
 * the event numbers with arguments and those without are mixed in one
 * enumeration rather than split.
 *
 * The mask is 0x50, which is 0x10 | 0x40 -- and 0x40 on its own is what
 * `t_wait_forever` and the friendship speech park on.
 */
long t_print_timeout_msg(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x186b)
        return mk3_unwind(thread);

    if (token != 0)
        return -3;

    obj->field28 = 0;
    send_code_a3(obj);
    obj->field28 = 0x21b;
    send_code_a3(obj);

    MKEvent_Add(3, 0xc, 0, 0);

    *mk3_frame(thread, thread->frame + 1) = 0x186b;
    thread->fieldfc = 0x50;
    return 0x50;
}


/* ------------------------------------------------------------ DoSwitchJump
 *
 * armv7 0x00055efc, one hundred and forty bytes.  **Complete.**
 *
 *      table = flag ? *(0x000f3204) : *(0x000f3210)
 *      row = table[index]
 *      if (row == NULL) return
 *      frame.pp    = &Pp[player]            ; stride 140
 *      frame.grobj = &GrObj[player]         ; stride 76
 *      c = Plyr[player].field08->field24    ; stride 108, then the character
 *      fn = row[c]
 *      if (fn == NULL) return
 *      fn(&frame)
 *
 * A two-level dispatch: an index picks a row and the CHARACTER picks the entry
 * in it, so this is how a switch does something different per fighter without
 * every switch knowing the roster. Both levels are checked for null and both
 * failures are silent.
 *
 * The first argument chooses between two tables entirely -- 0x000f3204 and
 * 0x000f3210 -- so there are two complete sets of per-character handlers.
 *
 * The three multiplies are 140, 76 and 108: `Pp`, `GrObj` and `Plyr`, all
 * three strides in one function for the second time after
 * `Endurance_ClearPlayer`. The character is reached the long way here, through
 * Plyr's 0x08 rather than GrObj's own 0x24, which is a second path to the same
 * number.
 *
 * The callee is handed a pointer to a 0x6c-byte stack frame of which exactly
 * two words are filled -- 0 and 8. **The word at 4 is never written**, so it
 * is whatever the stack held. Transcribed as an uninitialised field rather
 * than zeroed, because zeroing it would be inventing a value the callee may
 * be relying on not to exist.
 */
typedef struct SWITCHFRAME {
    char    *P32 pp;                /* 0x00  &Pp[player] */
    uint32_t uninitialised;     /* 0x04  never written -- see above */
    char    *P32 grobj;             /* 0x08  &GrObj[player] */
    uint8_t  rest[0x6c - 12];   /* the frame is 0x6c bytes; nothing fills it */
} SWITCHFRAME;

extern void *P32 *SwitchTableA;             /* slot 0x000f3204 */
extern void *P32 *SwitchTableB;             /* slot 0x000f3210 */

void DoSwitchJump(long flag, uint32_t player, uint32_t index)
{
    void *P32 *table = flag ? SwitchTableA : SwitchTableB;
    void *P32 *row = (void *P32 *)table[index];
    SWITCHFRAME frame;
    uint32_t c;
    void (*fn)(SWITCHFRAME *, char *);

    if (row == NULL)
        return;

    frame.pp    = Pp + player * PP_STRIDE;
    frame.grobj = GrObj + player * GROBJ_STRIDE;

    c = *(const uint32_t *)
        (*(char *P32 *)(Plyr + player * PLYR_STRIDE + 8) + 0x24);

    fn = (void (*)(SWITCHFRAME *, char *))row[c];
    if (fn == NULL)
        return;

    /* TWO arguments: at the `blx r2` (0x55f60) r1 still holds
     * &Plyr[player], computed for the character index two instructions
     * earlier, and the handlers read it -- secret_move_search hands it to
     * stick_look_lr as the "other" whose proc names the opponent. Called
     * with the frame alone, the second argument was whatever the register
     * held, and the first close-range button of a fight crashed. */
    fn(&frame, Plyr + player * PLYR_STRIDE);
}


/* ---------------------------------------------------------- create_fx_param
 *
 * armv7 0x00058b64, one hundred and forty bytes.  **Complete**, and it names
 * four finishers.
 *
 *      switch (obj->field1c) {          ; a tbb over [0x16, 0x2b]
 *      case 0x16: tsound_func(obj, 0x62);                    break;
 *      case 0x18: tsound_func(obj, 0); tsound_func(obj, 1);  break;
 *      case 0x1c: NewThread(obj, t_fx_animality);            break;
 *      case 0x1d: NewThread(obj, t_fx_mercy);                break;
 *      case 0x2a: NewThread(obj, t_fx_friendship);           break;
 *      case 0x2b: NewThread(obj, t_fx_babality);             break;
 *      default:                                              break;
 *      }
 *      MKEvent_Add(4, obj->field1c, param, obj->field00->field08)
 *
 * So the effect numbers are: **0x1c animality, 0x1d mercy, 0x2a friendship,
 * 0x2b babality**. Four of the game's finishers, identified not by guessing at
 * constants but by the names of the threads they start.
 *
 * Sixteen of the twenty-two cases do nothing but the event, and the two that
 * are not threads are sounds. So one table carries "make a noise", "start a
 * sequence" and "just tell the front end", and the event at the end runs
 * whatever the case did.
 *
 * The event is (4, effect, param, strength) -- the first this file has seen
 * with a 4 in front, and the only one that carries the PROC's 0x08.
 *
 * The jump table is a `tbb`: twenty-two bytes at 0x00058b7a, each the
 * half-offset from that address. Read out of the image rather than from the
 * disassembly, which renders them as instructions.
 *
 * `finish_him_or_her` sets 0x11 or 0x12, which are below this range, so those
 * go to `create_fx` and not here. Two entry points, two ranges.
 */
long t_fx_animality(MK3THREAD *thread);
long t_fx_mercy(MK3THREAD *thread);

void create_fx_param(MK3OBJ *obj, uint32_t param)
{
    /* Read ONCE, on entry (0x58b68 `ldr r5, [r0, #0x1c]`), and r5 is what
     * MKEvent_Add gets. NewThread rewrites obj->field1c, so reading it again
     * after the switch sent the four threaded effects' events with a stray
     * code: no BABALITY / ANIMALITY / MERCY / FRIENDSHIP banner or tune. */
    uint32_t code = obj->field1c;

    switch (code) {
    case 0x16:
        tsound_func(obj, 0x62);
        break;
    case 0x18:
        tsound_func(obj, 0);
        tsound_func(obj, 1);
        break;
    case 0x1c:
        NewThread(obj, (MK3THREADFUNC)t_fx_animality);
        break;
    case 0x1d:
        NewThread(obj, (MK3THREADFUNC)t_fx_mercy);
        break;
    case 0x2a:
        NewThread(obj, (MK3THREADFUNC)t_fx_friendship);
        break;
    case 0x2b:
        NewThread(obj, (MK3THREADFUNC)t_fx_babality);
        break;
    default:
        break;
    }

    MKEvent_Add(4, (long)code, (long)param, obj->field00->field08);
}


/* ------------------------------------------------------------ t_act_mframew
 *
 * armv7 0x00055620, one hundred and forty bytes.  **Complete.**
 *
 *      token == 0:
 *          obj->field00->field18 = obj->field20        ; set the action
 *          frame[frame+1].w0 = 0x6b4
 *          frame = frame + 1                           ; push
 *          frame[frame].handler = t_mframew
 *          frame[frame+1].w0 = 0
 *      token == 0x6b4:  unwind
 *      otherwise:       return -3
 *
 * The call shape with one store first, and the name says what it is: act, then
 * mframew. The action comes from the object's 0x20 rather than a constant, so
 * the caller chooses it and this only commits it.
 *
 * `stance_setup` writes 0x303 into the same field directly. So an action is
 * set either way -- with a constant in place, or from a slot through this --
 * and both write the PROC's 0x18.
 */
long t_act_mframew(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x6b4)
        return mk3_unwind(thread);

    if (token != 0)
        return -3;

    obj->field00->field18 = obj->field20;

    *mk3_frame(thread, thread->frame + 1) = 0x6b4;
    thread->frame = thread->frame + 1;                  /* push a level */
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------- t_wait_for_landing
 *
 * armv7 0x000574a4, one hundred and forty bytes.  **Complete.**
 *
 *      token == 0:       park(0x15c3, 1)
 *      token == 0x15c3:
 *          get_his_action(obj)
 *          if (obj->field20 == 0x610) unwind
 *          is_he_airborn(obj)
 *          if (obj->field5c == 0)     unwind
 *          frame[frame].handler = t_wait_for_landing       ; poll again
 *      otherwise:        return -3
 *
 * The second polling loop, and the first with two ways out. It ends when he
 * lands -- `is_he_airborn` leaves its answer in 0x5c -- or when his action
 * becomes 0x610, whichever happens first.
 *
 * The action test comes FIRST and short-circuits, so `is_he_airborn` is not
 * called at all in that case. An escape hatch rather than a second condition:
 * whatever 0x610 is, it means this wait is over regardless of where he is.
 *
 * Both exits go to the same unwind, so the caller cannot tell which happened.
 * If it needs to know, it has to look at the action itself -- and 0x20 still
 * holds it, because `get_his_action` left it there.
 */

long t_wait_for_landing(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x15c3;
        thread->fieldfc = 1;
        return 1;
    }

    if (token != 0x15c3)
        return -3;

    get_his_action(obj);
    if (obj->field20 == 0x610)          /* the escape hatch */
        return mk3_unwind(thread);

    is_he_airborn(obj);
    if (obj->field5c == 0)              /* he has landed */
        return mk3_unwind(thread);

    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_wait_for_landing;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------- t_wait_for_start
 *
 * armv7 0x0005a990, one hundred and forty bytes.  **Complete.**
 *
 *      token == 0:
 *          stance_setup(obj)
 *          park(0x105d, 1)
 *      token == 0x105d:
 *          next_anirate(obj)
 *          obj->field1c = (int16_t)G[0x44e]
 *          if ((uint16_t)G[0x44e] == 0) park(0x105d, 1)    ; still waiting
 *          else unwind
 *      otherwise:  return -3
 *
 * The cleanest poll here. It re-parks with **the same token**, which is
 * already in the slot, so the store changes nothing and the next entry lands
 * on the same branch. No handler is written at all.
 *
 * Compare `t_wait_for_his_dog`, which re-installs itself and clears the slot
 * above so the park happens again from the top. Both poll once per tick; this
 * one does it with a store that is a no-op and a return value.
 *
 * The flag is G+0x44e, which `t_master_mercy_entry` sets to 1. So this is what
 * mercy waits on, and `t_mercy_start` is what pushed it -- after taking the
 * controls away, which is why nothing here reads a button.
 *
 * The halfword is read twice: unsigned to test, signed into 0x1c. So a
 * negative value would be non-zero and would end the wait, arriving at the
 * caller as a negative number.
 */
long t_wait_for_start(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        stance_setup(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x105d;
        thread->fieldfc = 1;
        return 1;
    }

    if (token != 0x105d)
        return -3;

    next_anirate(obj);
    obj->field1c = (uint32_t)(int32_t)*(const int16_t *)(G_BYTES + 0x44e);

    if (*(const uint16_t *)(G_BYTES + 0x44e) == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x105d;   /* unchanged */
        thread->fieldfc = 1;
        return 1;
    }

    return mk3_unwind(thread);
}


/* ------------------------------------------------------------------ t_fani3
 *
 * armv7 0x000561a4, one hundred and forty-four bytes.  **Complete.**
 *
 *      token == 0:
 *          set_noscroll(obj)
 *          obj->field08->field0e = 0xc7        ; a fixed position
 *          obj->field08->field12 = 0x50
 *          obj->field1c = 4
 *          frame[frame+1].w0 = 0xf8d
 *          frame = frame + 1                   ; push
 *          frame[frame].handler = t_mframew
 *          frame[frame+1].w0 = 0
 *      token == 0xf8d:
 *          frame[frame].handler = t_wait_forever
 *          frame[frame+1].w0 = 0
 *      otherwise:  return -3
 *
 * The finisher animations' frame. It stops the camera, plants the object at
 * (0xc7, 0x50) -- 199 and 80, constants and not a computation -- runs
 * `t_mframew` as a call, and then becomes `t_wait_forever`.
 *
 * The second half is a tail call and NOT an unwind, which is the difference
 * that matters: the thread does not return to whoever pushed it. It stops
 * where it is and waits to be killed. That is what the end of a finisher looks
 * like from inside.
 *
 * `t_ship_proc` and `t_fx_friendship` both install this, so ship and
 * friendship share the frame and differ only in the table they put in 0x40
 * beforehand.
 *
 * The two coordinates are written as halfwords into 0x0e and 0x12 of the
 * OTHER object at 0x08, not this one.
 */

long t_fani3(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0xf8d) {
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_wait_forever;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0)
        return -3;

    set_noscroll(obj);
    MK3_SET_FIELD0E(obj->field08, 0xc7);
    MK3_SET_FIELD12(obj->field08, 0x50);
    obj->field1c = 4;

    *mk3_frame(thread, thread->frame + 1) = 0xf8d;
    thread->frame = thread->frame + 1;                  /* push a level */
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------- t_shake_him_up and t_shake_ob_up
 *
 * armv7 0x00056fdc and 0x00056d68, one hundred and forty-four bytes each.
 * **Complete**, and one body with one store swapped.
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      args[argc++] = obj->field40
 *      args[argc++] = obj->a10
 *      args[argc++] = obj->field48
 *      args[argc++] = obj->field20
 *      proc->field28 = <him>                   ; the only difference
 *      frame[frame].handler = t_shake2
 *      frame[frame+1].w0 = 0
 *
 *      t_shake_him_up:  proc->field28 = proc->him
 *      t_shake_ob_up:   proc->field28 = obj->field08
 *
 * Two notions of "the other guy" -- the PROC's `him` at 0x04 and the object's
 * 0x08 -- and a pair of functions because they are not always the same object.
 * The names say which is which, and that is what gives 0x28 its meaning: who
 * this shake is about.
 *
 * Four words pushed, the largest push here: the animation at 0x40, the A10 at
 * 0x44, 0x48 and 0x20. `t_striker` pushes two and `t_animate_a9` one, so four
 * of the twenty slots is still well inside the array.
 *
 * The cursor is stored after every push rather than once at the end, as in
 * `t_striker`. Not observable and transcribed as written.
 */

static long mk3_shake_up(MK3THREAD *thread, uint32_t about)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t argc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    argc = thread->fieldf8;
    *mk3_arg(thread, argc) = obj->field40;
    thread->fieldf8 = ++argc;
    *mk3_arg(thread, argc) = obj->a10;
    thread->fieldf8 = ++argc;
    *mk3_arg(thread, argc) = obj->field48;
    thread->fieldf8 = ++argc;
    *mk3_arg(thread, argc) = obj->field20;
    thread->fieldf8 = ++argc;

    obj->field00->field28 = about;

    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_shake2;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}

long t_shake_him_up(MK3THREAD *thread)
{
    return mk3_shake_up(thread, ((MK3OBJ *)thread->proc)->field00->him);
}

long t_shake_ob_up(MK3THREAD *thread)
{
    return mk3_shake_up(thread, (uint32_t)(uintptr_t)((MK3OBJ *)thread->proc)->field08);
}


/* ---------------------------------------------------- t_angle_jump_land_jsrp
 *
 * armv7 0x00059eb0, one hundred and forty-eight bytes.  **Complete.**
 *
 *      token == 0:
 *          obj->field20 = 0x304
 *          obj->field00->field18 = 0x304       ; the same value, twice
 *          face_opponent(obj)
 *          tsound_func(obj, 0x18)
 *          obj->field40 = 0x1a
 *          get_char_ani(obj)
 *          allow_moves(obj)
 *          do_next_a9_frame(obj)
 *          park(0x85e, 3)
 *      token == 0x85e:  unwind
 *      otherwise:       return -3
 *
 * Landing from an angled jump, and the longest run of set-up before a park so
 * far. The order is the content: commit the action, turn to face, make the
 * noise, load the animation, re-enable moves, show one frame, then wait.
 *
 * 0x304 goes to two places from one register -- the object's 0x20 and the
 * PROC's action at 0x18. `t_act_mframew` copies 0x20 into that same field, so
 * this is the same pairing done inline.
 *
 * `allow_moves` before the frame rather than after: the fighter is controllable
 * from the first frame of the landing, not once it finishes.
 *
 * The animation number is 0x1a, well under the 0x47 that makes it a number
 * rather than a pointer.
 */

long t_angle_jump_land_jsrp(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x85e)
        return mk3_unwind(thread);

    if (token != 0)
        return -3;

    obj->field20 = 0x304;
    obj->field00->field18 = 0x304;

    face_opponent(obj);
    tsound_func(obj, 0x18);

    obj->field40 = 0x1a;
    get_char_ani(obj);

    allow_moves(obj);
    do_next_a9_frame(obj);

    *mk3_frame(thread, thread->frame + 1) = 0x85e;
    thread->fieldfc = 3;
    return 3;
}


/* -------------------------------------------------------- t_duck_turnaround
 *
 * armv7 0x000559cc, one hundred and forty-eight bytes.  **Complete.**
 *
 *      token == 0:
 *          obj->field40 = 5
 *          get_char_ani(obj)
 *          obj->field1c = 2
 *          frame[frame+1].w0 = 0x89d
 *          frame = frame + 1                   ; push
 *          frame[frame].handler = t_mframew
 *          frame[frame+1].w0 = 0
 *      token == 0x89d:  unwind
 *      otherwise:       return -3
 *
 * Animation 5 at rate 2, run as a call. Three of the four routines that push
 * `t_mframew` -- this, `t_act_mframew` and `t_fani3` -- set something up and
 * hand it the frame, so `t_mframew` is the shared "play this and come back".
 *
 * The rate goes into 0x1c AFTER the resolver, which writes 0x40 and not 0x1c,
 * so the order is free and this is the order the code has.
 */
long t_duck_turnaround(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x89d)
        return mk3_unwind(thread);

    if (token != 0)
        return -3;

    obj->field40 = 5;
    get_char_ani(obj);
    obj->field1c = 2;

    *mk3_frame(thread, thread->frame + 1) = 0x89d;
    thread->frame = thread->frame + 1;                  /* push a level */
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------- t_round_is_over
 *
 * armv7 0x000563a8, one hundred and forty-eight bytes.  **Complete**, and it
 * is the writer the round-result enumeration was missing.
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field20 = G[0x368]                 ; player one's health
 *      obj->field24 = G[0x36c]                 ; player two's
 *      if (equal)          next = t_round_tied
 *      else {
 *          obj->field34 = 1
 *          if (h1 > h2)    next = t_p1_won
 *          else {
 *              obj->field48 = 1
 *              next = t_prend
 *          }
 *      }
 *
 * The health bars are the ones `recharge_bars` fills to 166 and announces, so
 * the comparison is exactly what it looks like.
 *
 * The third arm writes **1** into 0x48 -- the round result -- which is the
 * value `t_results_of_round` sends to `t_player_2_won` and the one nothing had
 * been seen to write. There is no `t_p2_won` to match `t_p1_won`: this routine
 * writes the value itself and goes straight to `t_prend`, which is all
 * `t_p1_won` does after writing 0. One of the two got a function and the other
 * did not.
 *
 * Every value of that enumeration now has a writer and a reader.
 *
 * The tie arm does NOT set 0x34, because the store sits after the equality
 * branch. Two outcomes set it and one does not, so 0x34 is "somebody won"
 * rather than "the round ended".
 */
long t_round_is_over(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t h1, h2;
    MK3THREADFUNC next;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    h1 = *(const uint32_t *)(G_BYTES + 0x368);
    obj->field20 = h1;
    h2 = *(const uint32_t *)(G_BYTES + 0x36c);
    obj->field24 = h2;

    if (h1 == h2) {
        next = (MK3THREADFUNC)t_round_tied;
    } else {
        obj->field34 = 1;
        if ((int32_t)h1 > (int32_t)h2) {
            next = (MK3THREADFUNC)t_p1_won;
        } else {
            obj->field48 = 1;               /* player two won */
            next = (MK3THREADFUNC)t_prend;
        }
    }

    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)next;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------------ cbox_squeeze
 *
 * armv7 0x00055fd4, one hundred and fifty-two bytes.  **Complete.**
 *
 *      if (!(flags & 1)) return
 *      obj->field54 = other->field24                   ; the character
 *      if (other->field24 == 0x18) {
 *          d = obj->field20 - obj->field1c
 *          obj->field30 = d < 0 ? -d : d               ; the span, unsigned
 *          d = obj->field30 >> 3
 *          obj->field1c += d                           ; pull both ends in
 *          obj->field20 -= d
 *          shift = (other->field28 & 0x10) ? -0x25 : 0x25
 *          obj->field54 = other->field28
 *          obj->field24 += shift                       ; and slide the box
 *          obj->field28 += shift
 *      }
 *      w = obj->field2c
 *      q = w >> 2
 *      if (other->a10 != 1 && obj->field00->field18 != 0x20f)
 *          q += w >> 3                                 ; a quarter, or 3/8
 *      obj->field2c = q
 *      obj->field24 += q
 *      obj->field28 -= q
 *
 * A collision box narrowed from both sides by a fraction of its own width: a
 * quarter normally, three eighths otherwise. The two conditions that keep it
 * at a quarter are the other object's A10 being 1 and this one's action being
 * 0x20f -- either alone is enough.
 *
 * **One character, 0x18, is squeezed differently.** Before the shared part it
 * pulls the 0x1c..0x20 span in by an eighth at each end and slides the
 * horizontal pair by 0x25 -- negated when bit 4 of the other's 0x28 says facing
 * left, which is the direction `am_i_facing_him_px` established.
 *
 * 0x18 is also one of the three characters `is_finish_him_allowed` refuses. So
 * that id is unusual in two unrelated ways, which is what a boss looks like
 * from inside the code. Which character it is is still not stated anywhere
 * here.
 *
 * 0x38 is borrowed for the eighth and put back, and the special path jumps
 * into the middle of the shared one so the two loads it has already done are
 * not repeated. Written out as straight-line code, which costs those two loads
 * and says the same thing.
 *
 * All the shifts are arithmetic, so a negative width shrinks toward zero
 * rather than wrapping.
 */
void cbox_squeeze(MK3OBJ *obj, uint32_t flags, MK3OBJ *other)
{
    uint32_t saved38;
    int32_t w, q;

    if ((flags & 1) == 0)
        return;

    obj->field54 = other->field24;

    if (other->field24 == 0x18) {           /* this one character only */
        int32_t d = (int32_t)obj->field20 - (int32_t)obj->field1c;
        int32_t shift;

        obj->field30 = (uint32_t)d;
        if (d < 0)
            obj->field30 = (uint32_t)(-d);

        d = (int32_t)obj->field30 >> 3;
        obj->field1c = (uint32_t)((int32_t)obj->field1c + d);
        obj->field20 = (uint32_t)((int32_t)obj->field20 - d);

        shift = 0x25;
        obj->field30 = 0x25;
        if ((other->field28 & 0x10u) != 0) {        /* facing left */
            shift = 0x25 - 0x4a;
            obj->field30 = (uint32_t)shift;
        }
        obj->field54 = other->field28;

        obj->field24 = (uint32_t)((int32_t)obj->field24 + shift);
        obj->field28 = (uint32_t)((int32_t)obj->field28 + shift);
    }

    saved38 = obj->field38;
    w = (int32_t)obj->field2c;
    q = w >> 2;
    obj->field38 = (uint32_t)(w >> 3);      /* borrowed for the eighth */
    obj->field2c = (uint32_t)q;

    if (other->a10 != 1 && obj->field00->field18 != 0x20f) {
        q += w >> 3;                        /* three eighths instead */
        obj->field2c = (uint32_t)q;
    }

    obj->field24 = (uint32_t)((int32_t)obj->field24 + q);
    obj->field38 = saved38;
    obj->field28 = (uint32_t)((int32_t)obj->field28 - q);
}


/* ------------------------------------------------------------- t_do_backup
 *
 * armv7 0x00055ce0, one hundred and fifty-two bytes.  **Complete.**
 *
 *      token == 0:
 *          obj->field20 = 0x301
 *          obj->field00->field18 = 0x301       ; the action, from one register
 *          obj->field40 = 4                    ; the animation
 *          obj->field1c = 2                    ; formed as 4 - 2
 *          frame[frame+1].w0 = 0x8e7
 *          frame = frame + 1                   ; push
 *          frame[frame].handler = t_backwards_ani
 *          frame[frame+1].w0 = 0
 *      token == 0x8e7:  unwind
 *      otherwise:       return -3
 *
 * Walking backwards. The same shape as `t_duck_turnaround` with an action set
 * as well, and the same trick as `t_master_mercy_entry`: the rate is loaded as
 * 4 and then reduced by 2 rather than loaded twice.
 *
 * 0x301 goes to both the object's 0x20 and the PROC's action, which is the
 * pairing `t_angle_jump_land_jsrp` does with 0x304 -- so 0x30N is a family of
 * ordinary movement actions.
 */
long t_backwards_ani(MK3THREAD *thread);

long t_do_backup(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x8e7)
        return mk3_unwind(thread);

    if (token != 0)
        return -3;

    obj->field20 = 0x301;
    obj->field00->field18 = 0x301;
    obj->field40 = 4;
    obj->field1c = 2;

    *mk3_frame(thread, thread->frame + 1) = 0x8e7;
    thread->frame = thread->frame + 1;                  /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_backwards_ani;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ---------------------------------------------------------- is_he_blocking
 *
 * armv7 0x0005837c, one hundred and sixty bytes.  **Complete**, and it answers
 * one question two ways.
 *
 *      obj->field14 = 0
 *      obj->field1c = (int16_t)H[0x1c]
 *      if (H[0x1c] != 0) { obj->field5c = 0; return; }     ; nobody blocks
 *
 *      is_he_joy(obj)
 *      if (!obj->field5c) {                                ; the AI
 *          act = obj->field1c->field00->field18
 *          obj->field20 = act
 *          if (act == 0x700)      { obj->field5c = 1; return; }
 *          if (act != 0x701)      { obj->field5c = 0; return; }
 *          obj->field14 = 1;        obj->field5c = 1; return;
 *      }
 *                                                          ; a human
 *      is_he_airborn(obj)
 *      if (obj->field5c) { obj->field5c = 0; return; }
 *      proc = obj->field00
 *      obj->field1c = him->field30
 *      if (him->field30 & MK3F_NO_BLOCK) { obj->field5c = 0; return; }
 *      obj->field00 = proc->field00->field00               ; borrow
 *      check_block_bit(obj)
 *      obj->field00 = proc
 *      if (!obj->field5c) return
 *      i = proc->field00->field00->field08
 *      obj->field1c = G[i]
 *      if (G[i] & 2)          { obj->field14 = 1; obj->field5c = 1; return; }
 *      if (obj->field30 & 2)  { obj->field5c = 0; return; }
 *      obj->field5c = 1
 *
 * **Two definitions of blocking.** For a person it is a button, reached
 * through `check_block_bit`, plus three conditions -- not airborne, the
 * NO_BLOCK flag clear, and a bit in G. For the computer it is nothing but the
 * action: 0x700 or 0x701 and no other evidence is consulted.
 *
 * That asymmetry is the whole of what makes an AI opponent in a fighting game.
 * A port that unifies the two -- giving the computer a button, or the player an
 * action test -- would be simpler and would not be this game.
 *
 * 0x701 sets 0x14 as well as 0x5c, and so does the human path when bit 1 of
 * G[index] is set. So 0x14 is the low block, told apart from the high one, and
 * the two paths reach the same output from unrelated evidence.
 *
 * The halfword at H+0x1c short-circuits everything: while it is non-zero
 * nobody is blocking at all, whichever kind of player they are.
 *
 * The PROC is swapped for the opponent's around `check_block_bit` and put
 * back -- the borrow-and-restore this file does everywhere, here across a call
 * in another translation unit.
 *
 * G is indexed by word from its base, so the bit tested lives in the first two
 * words of the whole structure. Everything else in this file reaches G at
 * three-digit offsets.
 *
 * **0x14 and 0x5c each have exactly one physical store in the binary,** not
 * one per branch that sets them. `0x701` and the human `G[i] & 2` case both
 * jump into the SAME `str r3,[r4,#0x14]` at 0x583b8, which falls into the
 * same `str r3,[r4,#0x5c]` at 0x583bc that `0x700` and the final pass-through
 * also reach directly. Written here as `low_block:`/`set_true:` labels so the
 * call-site count matches; the four inline returns this had before were
 * behaviourally identical but landfn's fact count catches the duplication.
 */
void check_block_bit(MK3OBJ *obj);

void is_he_blocking(MK3OBJ *obj)
{
    MK3OBJPROC *proc;
    uint32_t flags, i;

    obj->field14 = 0;
    obj->field1c = (uint32_t)(int32_t)*(const int16_t *)(H + 0x1c);
    if (*(const uint16_t *)(H + 0x1c) != 0) {
        obj->field5c = 0;
        return;
    }

    is_he_joy(obj);

    if (obj->field5c == 0) {                    /* the computer */
        uint32_t act = ((MK3OBJ *)(uintptr_t)obj->field1c)->field00->field18;

        obj->field20 = act;
        if (act == 0x700)
            goto set_true;
        if (act != 0x701) {
            obj->field5c = 0;
            return;
        }
        goto low_block;
    }

    is_he_airborn(obj);                         /* a person */
    if (obj->field5c != 0) {
        obj->field5c = 0;
        return;
    }

    proc = obj->field00;
    flags = ((MK3OBJ *)(uintptr_t)proc->him)->field30;
    obj->field1c = flags;
    if ((flags & MK3F_NO_BLOCK) != 0) {
        obj->field5c = 0;
        return;
    }

    obj->field00 = proc->field00->field00;      /* borrow his PROC */
    check_block_bit(obj);
    obj->field00 = proc;

    if (obj->field5c == 0)
        return;

    i = proc->field00->field00->field08;
    obj->field1c = ((const uint32_t *)G_BYTES)[i];

    if ((obj->field1c & 2) != 0)
        goto low_block;

    if ((obj->field30 & 2) != 0) {
        obj->field5c = 0;
        return;
    }

    goto set_true;

low_block:
    obj->field14 = 1;                           /* the low block */
set_true:
    obj->field5c = 1;
}


/* ---------------------------------------------------------- t_turn_around
 *
 * armv7 0x0005825c, one hundred and sixty-four bytes.  **Complete.**
 *
 *      token == 0:
 *          obj->field40 = 3                    ; the animation
 *          stop_me_player(obj)
 *          disable_all_buttons(obj)
 *          get_char_ani(obj)
 *          obj->field1c = 2                    ; rate, and
 *          obj->a10     = 2                    ; A10, from one register
 *          frame[frame+1].w0 = 0x7ff
 *          frame = frame + 1                   ; push
 *          frame[frame].handler = t_mframew
 *          frame[frame+1].w0 = 0
 *      token == 0x7ff:  unwind
 *      otherwise:       return -3
 *
 * Turning to face the other way: stop, take the controls, play animation 3.
 * The controls are not given back here, exactly as in `t_mercy_start` -- the
 * routine that regains them is somewhere past `t_mframew`.
 *
 * The animation number is set BEFORE `stop_me_player`, which is the only order
 * that works if that call reads 0x40. Transcribed in the order written rather
 * than grouped with the resolver it belongs to.
 */
long t_turn_around(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x7ff)
        return mk3_unwind(thread);

    if (token != 0)
        return -3;

    obj->field40 = 3;
    stop_me_player(obj);
    disable_all_buttons(obj);
    get_char_ani(obj);
    obj->field1c = 2;
    obj->a10     = 2;

    *mk3_frame(thread, thread->frame + 1) = 0x7ff;
    thread->frame = thread->frame + 1;                  /* push a level */
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------- back_to_normal_px
 *
 * armv7 0x000596a8, one hundred and sixty-eight bytes.  **Complete**, and it
 * checks six of this file's flag names in one instruction.
 *
 *      hits = obj->field00->p_hit
 *      obj->field30 = hits
 *      if (hits > 1) {
 *          obj->field1c = 0xd
 *          create_fx_param(obj,
 *              ((obj->field00->field54 * 100 / 166) << 8) + hits)
 *      }
 *      his = other->field00
 *      obj->field1c = 0
 *      his->p_hit = his->field4c = his->field50 = his->field54 =
 *          his->field18 = his->field58 = 0
 *      player_normpal(other)
 *      delete_slave(other)
 *      f = other->field08->field30
 *      f &= ~(NO_BLOCK|NOFLIP|NOCOL|NOEDGE|HALF_DAMAGE|QUARTER_DAMAGE)
 *      if (!(other->field08->field30 & MK3F_INVISO)) f |= MK3F_SHADOW
 *      other->field08->field30 = f
 *      other->field08->field28 &= ~0x20
 *      other->field00->field10 &= ~0x1e
 *
 * **The mask is the find.** The literal is 0xffffe2f3 and its complement is
 * 0x1d0c, which is exactly
 *
 *      MK3F_NO_BLOCK | MK3F_NOFLIP | MK3F_NOCOL | MK3F_NOEDGE |
 *      MK3F_HALF_DAMAGE | MK3F_QUARTER_DAMAGE
 *
 * -- six of the ten names this file assigned one routine at a time, each from
 * its own single-bit clearer. A function called "back to normal" clearing
 * precisely those six and nothing else is a check on all six readings at once,
 * and it passes. The four it leaves alone are NOSCROLL, SHADOW, INVISO and
 * IGNORE_Y, which are not damage or collision states.
 *
 * SHADOW is then put back unless INVISO is set. Those are the two the mask
 * skips and they are handled together, which fits: a fighter who is invisible
 * should not regain a shadow.
 *
 * The effect only fires when the hit count is above 1 -- a combo, not a single
 * blow. The number it reports is `field54 * 100 / 166` in the high bits with
 * the hit count in the low byte, and 166 is the full health `recharge_bars`
 * writes, so it is a **percentage of a life bar**.
 *
 * The divide is a modular-inverse multiply by 827945503 with a shift of 37,
 * which is ceil(2^37 / 166) exactly and not ceil(2^37 / 165). That is how the
 * divisor is known to be the health value rather than something near it.
 *
 * Effect 0xd is below `create_fx_param`'s jump-table range, so it does nothing
 * but send the event -- which is the point: this is a report, not an action.
 *
 * The six zeroes into the opponent's PROC come from one register and include
 * 0x18, the action. So "back to normal" ends whatever he was doing as well as
 * clearing the damage bookkeeping.
 */

void back_to_normal_px(MK3OBJ *obj, MK3OBJ *other)
{
    MK3OBJPROC *mine = obj->field00;
    MK3OBJPROC *his;
    uint32_t hits = mine->p_hit;
    uint32_t f;

    obj->field30 = hits;

    if ((int32_t)hits > 1) {                    /* a combo, not one blow */
        uint32_t percent = (mine->field54 * 100u) / 166u;

        obj->field1c = 0xd;
        create_fx_param(obj, (percent << 8) + hits);
    }

    his = other->field00;
    obj->field1c = 0;
    his->p_hit    = 0;
    *(uint32_t *)((char *)his + 0x4c) = 0;
    *(uint32_t *)((char *)his + 0x50) = 0;
    his->field54  = 0;
    his->field18  = 0;                          /* the action, too */
    *(uint32_t *)((char *)his + 0x58) = 0;

    player_normpal(other);
    delete_slave(other);

    f = other->field08->field30;
    obj->field2c = f & ~(MK3F_NO_BLOCK | MK3F_NOFLIP | MK3F_NOCOL |
                         MK3F_NOEDGE | MK3F_HALF_DAMAGE |
                         MK3F_QUARTER_DAMAGE);
    if ((f & MK3F_INVISO) == 0)                 /* no shadow if invisible */
        obj->field2c |= MK3F_SHADOW;
    other->field08->field30 = obj->field2c;

    other->field08->field28 &= ~0x20u;

    obj->field2c = other->field00->field10 & ~0x1eu;
    other->field00->field10 = obj->field2c;
}


/* --------------------------------------------------------------- t_do_duck
 *
 * armv7 0x00055c38, one hundred and sixty-eight bytes.  **Complete.**
 *
 *      token == 0:
 *          stop_me_player(obj)
 *          face_opponent(obj)
 *          obj->field40 = 4
 *          get_char_ani(obj)
 *          obj->field1c = 2                    ; the rate
 *          obj->field20 = 0x302                ; formed as 2 + 0x300
 *          frame[frame+1].w0 = 0x8d7
 *          frame = frame + 1                   ; push
 *          frame[frame].handler = t_act_mframew
 *          frame[frame+1].w0 = 0
 *      token == 0x8d7:  unwind
 *      otherwise:       return -3
 *
 * Ducking, and the third member of the movement-action family: 0x301 backing
 * up, 0x302 ducking, 0x304 landing from an angled jump. The action is left in
 * 0x20 and `t_act_mframew` -- pushed here -- is what copies it to the PROC.
 *
 * The action is built from the rate: 2 is stored, then 0x300 added to the same
 * register. Two constants for the price of one, and the reason the disassembly
 * makes 0x302 look derived.
 *
 * Animation 4 is the same number `t_do_backup` uses, so the two share an
 * animation and differ in the action and in what they push.
 */
long t_do_duck(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x8d7)
        return mk3_unwind(thread);

    if (token != 0)
        return -3;

    stop_me_player(obj);
    face_opponent(obj);

    obj->field40 = 4;
    get_char_ani(obj);

    obj->field1c = 2;
    obj->field20 = 0x302;

    *mk3_frame(thread, thread->frame + 1) = 0x8d7;
    thread->frame = thread->frame + 1;                  /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_act_mframew;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------- t_backwards_ani
 *
 * armv7 0x00055874, one hundred and seventy-two bytes.  **Complete.**
 *
 *      token == 0:
 *          args[argc++] = obj->field1c
 *          if ((int32_t)obj->field40 <= 0x48) get_char_ani(obj)
 *          frame[frame+1].w0 = 0x7d3
 *          frame = frame + 1                   ; push
 *          frame[frame].handler = t_bani2
 *          frame[frame+1].w0 = 0
 *      token == 0x7d3:  unwind
 *      otherwise:       return -3
 *
 * The rate is pushed onto the argument stack and left there -- no pop, unlike
 * `t_animate_a9`, which pushes and pops around its resolver. So `t_bani2`,
 * running one level up, is expected to find it.
 *
 * That is the first use of the argument stack as a real argument rather than a
 * save area: the pusher and the reader are different functions at different
 * levels.
 *
 * The threshold is **0x48**, and `t_attk3` uses 0x47 on the same slot. See the
 * note there; the two are left disagreeing because neither says which is
 * right.
 *
 * The frame index is re-read after the resolver, as in `t_attk3`, so the call
 * is allowed to move it.
 */
long t_bani2(MK3THREAD *thread);

/* `t_backwards_ani2` at 0x00055920 is the same one hundred and seventy-two
 * bytes with `get_char_ani2` in place of `get_char_ani` and 0x7e1 for its
 * token. The same pairing `t_animate_a9` and `t_animate2_a9` have, so the two
 * share a body here and differ in a resolver and a number. */
static long mk3_backwards_ani(MK3THREAD *thread, uint32_t token,
                              void (*resolve)(MK3OBJ *))
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t seen = *mk3_frame(thread, thread->frame + 1);
    uint32_t argc;

    if (seen == token)
        return mk3_unwind(thread);

    if (seen != 0)
        return -3;

    argc = thread->fieldf8;
    *mk3_arg(thread, argc) = obj->field1c;      /* left for t_bani2 */
    thread->fieldf8 = argc + 1;

    if ((int32_t)obj->field40 <= 0x48)          /* a number, not a pointer */
        resolve(obj);

    *mk3_frame(thread, thread->frame + 1) = token;
    thread->frame = thread->frame + 1;                  /* push a level */
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_bani2;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}

long t_backwards_ani(MK3THREAD *thread)
{
    return mk3_backwards_ani(thread, 0x7d3, get_char_ani);
}

long t_backwards_ani2(MK3THREAD *thread)
{
    return mk3_backwards_ani(thread, 0x7e1, get_char_ani2);
}


/* ---------------------------------------------------- t_is_endurance_possible
 *
 * armv7 0x0005768c, one hundred and eighty bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field5c = 0
 *      if ((int32_t)obj->field48 <= 1
 *          && (Pp[obj->field48].field10 & 1)
 *          && (int8_t)RoundParam[0x18] >= 0)
 *          obj->field5c = 1
 *      unwind
 *
 * Three conditions and one answer, and every failing path falls through to the
 * same unwind with 0x5c already zero -- so the routine is written as "assume
 * no" and never has to write a negative.
 *
 * The middle one indexes `Pp` by the round result at 0x48, which is 0 or 1
 * here: player one or player two. So the bit asked about is the WINNER's, and
 * `Pp` is per-player rather than per-fighter-slot.
 *
 * The stride is 140 again, formed as `n*20` then `that*8 - that`, which is a
 * third spelling of `PP_STRIDE` after `Endurance_ClearPlayer`'s and
 * `DoSwitchJump`'s.
 *
 * The last condition is a SIGNED BYTE at RoundParam+0x18 tested for being zero
 * or positive. The compiler spells it `mvn` then a logical shift right by 31 --
 * the inverted sign bit -- which is why the disassembly has no comparison in
 * it at all.
 *
 * That is a fourth field of RoundParam: 0x0c the difficulty `adjust_damage`
 * indexes with, 0x10 the round `t_print_round_number` speaks, 0x14 the ladder
 * order, and now 0x18. A byte among three words, and negative means no.
 *
 * `t_spawn_endurance_guy` says what it is: the head of a QUEUE of endurance
 * opponents, ten signed bytes from 0x18, which that routine pops and shifts
 * down. So this test is "is there another opponent", answered by the queue not
 * having run out. The two were read separately and each explains the other.
 */
long t_is_endurance_possible(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t who;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field5c = 0;

    who = obj->field48;
    if ((int32_t)who <= 1) {
        const char *entry = Pp + who * PP_STRIDE;

        if ((*(const uint32_t *)(entry + 0x10) & 1) != 0 &&
            *(const int8_t *)((const char *)RoundParam + 0x18) >= 0)
            obj->field5c = 1;
    }

    return mk3_unwind(thread);
}


/* ------------------------------------------------------ t_jump_up_land_jsrp
 *
 * armv7 0x00059f44, one hundred and eighty-eight bytes.  **Complete.**
 *
 *      token == 0:
 *          obj->field20 = 0x304
 *          obj->field00->field18 = 0x304
 *          tsound_func(obj, 0x18)
 *          obj->field40 = 0x16
 *          get_char_ani(obj)
 *          allow_moves(obj)
 *          obj->a10 = obj->field40         ; keep the resolved pointer
 *          obj->field40 += 4               ; and step it one word on
 *          do_next_a9_frame(obj)
 *          park(0x824, 3)
 *      token == 0x824:
 *          obj->field40 = obj->a10         ; put it back
 *          do_next_a9_frame(obj)
 *          park(0x827, 3)
 *      token == 0x827:  unwind
 *      otherwise:       return -3
 *
 * Landing from a straight jump, and a three-step coroutine that shows **two
 * frames of one animation out of order**: the second entry in the script
 * first, then the first.
 *
 * The four added to 0x40 is one word of the animation script, which
 * `stance_setup` walks in the same units. So this is a deliberate hop -- show
 * the next frame, then come back for the one it skipped.
 *
 * The same action 0x304 and the same sound 0x18 as `t_angle_jump_land_jsrp`,
 * and the same `allow_moves` before the first frame. The two landings differ
 * in the animation -- 0x16 against 0x1a -- and in this hop.
 *
 * The A10 slot carries the saved pointer across the park, which is a value
 * surviving in the object rather than on the argument stack. Both are used in
 * this file and this is the cheaper one for a single word.
 */
long t_jump_up_land_jsrp(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x824) {
        obj->field40 = obj->a10;                /* back to the skipped one */
        do_next_a9_frame(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x827;
        thread->fieldfc = 3;
        return 3;
    }

    if (token == 0x827)
        return mk3_unwind(thread);

    if (token != 0)
        return -3;

    obj->field20 = 0x304;
    obj->field00->field18 = 0x304;
    tsound_func(obj, 0x18);

    obj->field40 = 0x16;
    get_char_ani(obj);
    allow_moves(obj);

    obj->a10 = obj->field40;
    obj->field40 = obj->field40 + 4;            /* one word on */
    do_next_a9_frame(obj);

    *mk3_frame(thread, thread->frame + 1) = 0x824;
    thread->fieldfc = 3;
    return 3;
}


/* -------------------------------------------------------- t_double_mframew
 *
 * armv7 0x0005a19c, one hundred and ninety-two bytes.  **Complete**, and it
 * says what 0xfc is.
 *
 *      if (token != 0 && token != 0x6e8) return -3
 *      if (token == 0) args[argc++] = obj->field1c
 *      if (double_next_a9(obj) == 0) {
 *          obj->field1c = args[--argc]         ; pop
 *          unwind
 *      }
 *      v = args[--argc]                        ; peek, written as pop...
 *      obj->field1c = v
 *      args[argc++] = v                        ; ...and push back
 *      frame[frame+1].w0 = 0x6e8
 *      thread->fieldfc = obj->field1c
 *      return obj->field1c
 *
 * **The value written into 0xfc is `obj->field1c`** -- the animation rate,
 * read at run time. Every other park in this file writes a literal, and the
 * three earliest happened to be single bits, which is why that field was
 * called a mask. A field that takes an arbitrary number out of an animation is
 * a duration, and so the literals are durations.
 *
 * The loop is the animation: `double_next_a9` returns non-zero while there are
 * frames left, and each time round the thread parks for as long as the current
 * frame lasts. When it returns zero the rate is popped and the routine unwinds.
 *
 * The middle is a peek written as a pop and a push: the cursor comes down, the
 * word is read, the same word is written back to the same slot, and the cursor
 * goes up. Nothing changes. The compiler did not see through it and neither
 * does this; it is transcribed as the four operations it is.
 *
 * Both entries share everything after the initial push, which the second one
 * skips -- so the rate is stacked once and read on every tick.
 */
/* `t_mframew` at 0x0005a25c is the same one hundred and ninety-two bytes with
 * `do_next_a9_frame` in place of `double_next_a9` and 0x6a6 for its token, so
 * the two share this body.
 *
 * That one is the routine four others push as a call -- `t_act_mframew`,
 * `t_duck_turnaround`, `t_fani3` and `t_turn_around` -- and this is all it
 * does: play the animation, one frame per tick, come back when it ends. The
 * whole of "run an animation to completion" in this engine is a loop that
 * parks for the current frame's own rate. */
static long mk3_mframew(MK3THREAD *thread, uint32_t token,
                        long (*advance)(MK3OBJ *))
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t seen = *mk3_frame(thread, thread->frame + 1);
    uint32_t argc;

    if (seen != 0 && seen != token)
        return -3;

    if (seen == 0) {
        argc = thread->fieldf8;
        *mk3_arg(thread, argc) = obj->field1c;
        thread->fieldf8 = argc + 1;
    }

    if (advance(obj) == 0) {                    /* the animation is done */
        argc = thread->fieldf8 - 1;
        thread->fieldf8 = argc;
        obj->field1c = *mk3_arg(thread, argc);
        return mk3_unwind(thread);
    }

    argc = thread->fieldf8 - 1;                 /* a peek, spelled long-hand */
    thread->fieldf8 = argc;
    obj->field1c = *mk3_arg(thread, argc);
    *mk3_arg(thread, argc) = obj->field1c;
    thread->fieldf8 = argc + 1;

    *mk3_frame(thread, thread->frame + 1) = token;
    thread->fieldfc = obj->field1c;             /* wait this animation's rate */
    return (long)obj->field1c;
}

long t_double_mframew(MK3THREAD *thread)
{
    return mk3_mframew(thread, 0x6e8, double_next_a9);
}

long t_mframew(MK3THREAD *thread)
{
    return mk3_mframew(thread, 0x6a6, do_next_a9_frame);
}


/* --------------------------------------------------------- t_round_intro_fx
 *
 * armv7 0x00059094, one hundred and ninety-two bytes.  **Complete**, and the
 * longest coroutine here: four resume points, one of which is a call.
 *
 *      token 0:       obj->field1c = 0x13; create_fx(obj)
 *                     frame[frame+1].w0 = 0x1047
 *                     frame = frame + 1              ; a CALL, not a park
 *                     frame[frame].handler = t_print_round_number
 *      token 0x1047:  obj->field1c = 0xb; create_fx(obj)
 *                     park(0x104b, 0x10)
 *      token 0x104b:  tsound_func(obj, 0x10)
 *                     park(0x104d, 0x20)
 *      token 0x104d:  park(0x104e, 0x16462)          ; and never wake
 *      otherwise:     return -3
 *
 * The round introduction: an effect, then the spoken round number as a
 * subroutine, then a second effect, then a sound, then a wait that does not
 * end.
 *
 * The first step is the only one that pushes. So this mixes a call and three
 * parks in one function, and the token does double duty -- it is the resume
 * point for the parks AND the marker `t_print_round_number` leaves behind when
 * it unwinds. Both meanings work because both are "what is one level up".
 *
 * **The last step never wakes.** 0x104e is not a token this function accepts;
 * a fifth entry would fall through to `return -3`. Together with the duration
 * of 0x16462 that makes it a deliberate dead end, and `t_fx_babality` and
 * `t_friendship_speech` end exactly the same way. Three routines, three
 * unaccepted tokens, one duration.
 *
 * The dispatch is a binary comparison -- equal, then less-than, then two more
 * equals -- rather than a chain, which is why 0x104d is formed as 0x104b + 2.
 */
long t_round_intro_fx(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x1047) {
        obj->field1c = 0xb;
        create_fx(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x104b;
        thread->fieldfc = 0x10;
        return 0x10;
    }

    if (token == 0x104b) {
        tsound_func(obj, 0x10);
        *mk3_frame(thread, thread->frame + 1) = 0x104d;
        thread->fieldfc = 0x20;
        return 0x20;
    }

    if (token == 0x104d) {
        *mk3_frame(thread, thread->frame + 1) = 0x104e;
        thread->fieldfc = 0x16462;      /* nothing accepts 0x104e */
        return 0x16462;
    }

    if (token != 0)
        return -3;

    obj->field1c = 0x13;
    create_fx(obj);

    *mk3_frame(thread, thread->frame + 1) = 0x1047;
    thread->frame = thread->frame + 1;                  /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_print_round_number;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ----------------------------------------------------------- t_play_1_round
 *
 * armv7 0x000585e8, two hundred bytes.  **Complete.**
 *
 *      token 0:
 *          th = TList_Get()
 *          if (th != NULL) {
 *              th->fieldf8 = th->frame = th->fieldfc = th->field08 = 0
 *              th->proc = Plyr + th->player * 108
 *              th->func = t_round_intro_fx
 *              GrObj[th->player].field2c = -1
 *          }
 *          park(0x10ac, 0x20)
 *      token 0x10ac:  park(0x10ad, 0x40)
 *      token 0x10ad:  frame[frame].handler = t_continue_fighting
 *      otherwise:     return -3
 *
 * **NewThread inlined.** The first step does not call it; it does the same
 * work in place -- TList_Get, four zeroed fields, the proc from Plyr[player],
 * the entry point at 0x04. Field for field it matches `NewThread`'s body,
 * which makes this a check on that reading rather than a repetition of it. The
 * two were read from different addresses and agree.
 *
 * A failed `TList_Get` is not treated as an error: the park happens either
 * way, and a round with no threads left simply gets no intro.
 *
 * The animation of the GrObj entry is set to -1, which `getobjectinsert` also
 * does to the object it creates. So -1 in that slot is "nothing yet" rather
 * than a real animation.
 *
 * Two parks in a row with nothing between them: 0x20 ticks, then 0x40. Written
 * as two because the durations differ; a single 0x60 would not be the same if
 * anything can happen at the boundary.
 *
 * Both strides appear again -- 108 for Plyr, 76 for GrObj -- from the same two
 * shift-and-subtract sequences the rest of the file uses.
 */
MK3THREAD *TList_Get(void);

long t_play_1_round(MK3THREAD *thread)
{
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x10ac) {
        *mk3_frame(thread, thread->frame + 1) = 0x10ad;
        thread->fieldfc = 0x40;
        return 0x40;
    }

    if (token == 0x10ad) {
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_continue_fighting;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0)
        return -3;

    {
        MK3THREAD *th = TList_Get();

        if (th != NULL) {                       /* NewThread, written out */
            uint32_t i = th->player;

            th->fieldf8 = 0;
            th->frame   = 0;
            th->fieldfc = 0;
            th->field08 = 0;
            th->proc    = Plyr + i * PLYR_STRIDE;
            mk3_frame(th, 0)[1] = (uint32_t)(uintptr_t)t_round_intro_fx;

            *(uint32_t *)(GrObj + i * GROBJ_STRIDE + 0x2c) = 0xffffffffu;
        }
    }

    *mk3_frame(thread, thread->frame + 1) = 0x10ac;
    thread->fieldfc = 0x20;
    return 0x20;
}


/* ---------------------------------------------------------- t_fatality_wait
 *
 * armv7 0x00056540, two hundred and eight bytes.  **Complete.**
 *
 *      token 0:       park(0x113c, 1)
 *      token 0x113c:
 *          a = (uint16_t)G[0x450]
 *          obj->field1c = (int16_t)a
 *          if (a != 0)  { install t_fatal_yes; return 0; }
 *          b = (uint16_t)G[0x452]
 *          obj->field1c = (int16_t)b
 *          if (b != 0)  { install t_fatal_no;  return 0; }
 *          if (--obj->field48 > 0) install t_fatality_wait      ; poll again
 *          else                    install t_fatal_no
 *      otherwise:     return -3
 *
 * The fatality window. A countdown at 0x48 polled once a tick, and two
 * halfwords in G that can end it early: 0x450 means yes, 0x452 means no.
 *
 * **Running out and being refused are the same outcome.** Both install
 * `t_fatal_no`, from two different places with two different constants loaded
 * into the same register. The code does not distinguish them and neither does
 * this.
 *
 * The poll re-installs the routine at the current level and clears the slot
 * above, so the next tick re-enters at token 0 and parks again -- the shape
 * `t_wait_for_his_dog` uses, not the one `t_wait_for_start` uses.
 *
 * Each halfword is read UNSIGNED to test and SIGNED into 0x1c, so a negative
 * value counts as set and reaches the caller as a negative number. That
 * matters: `t_wait_fatality_finish` reads the same G+0x450 and tests it
 * against **-1**, so the halfword has at least three states -- zero, -1, and
 * anything else -- and two routines look at two different ones.
 *
 * The zeroes written into the slot above come from whichever halfword was just
 * found to be zero, which is why the disassembly stores three different
 * registers into the same place.
 */

long t_fatality_wait(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    MK3THREADFUNC next;
    uint16_t a, b;

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x113c;
        thread->fieldfc = 1;
        return 1;
    }

    if (token != 0x113c)
        return -3;

    a = *(const uint16_t *)(G_BYTES + 0x450);
    obj->field1c = (uint32_t)(int32_t)(int16_t)a;
    if (a != 0) {
        next = (MK3THREADFUNC)t_fatal_yes;              /* yes */
    } else {
        b = *(const uint16_t *)(G_BYTES + 0x452);
        obj->field1c = (uint32_t)(int32_t)(int16_t)b;
        if (b != 0) {
            next = (MK3THREADFUNC)t_fatal_no;           /* refused */
        } else {
            obj->field48 = obj->field48 - 1;
            next = ((int32_t)obj->field48 > 0)
                       ? (MK3THREADFUNC)t_fatality_wait /* keep waiting */
                       : (MK3THREADFUNC)t_fatal_no;     /* out of time */
        }
    }

    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)next;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ----------------------------------------------------------- t_fx_animality
 *
 * armv7 0x00057ef0, two hundred and eight bytes.  **Complete.**
 *
 *      token 0:
 *          tsound_func(obj, 0x95)
 *          obj->field08->field2c = 0xf40           ; an animation id
 *          obj->field08->field0e = 0x50            ; and a fixed position
 *          obj->field08->field12 = 0x50
 *          obj->field40 = a_animality
 *          set_noscroll(obj)
 *          obj->field1c = 4
 *          frame[frame+1].w0 = 0xfae
 *          frame = frame + 1                       ; push
 *          frame[frame].handler = t_mframew
 *      token 0xfae:  park(0xfb4, 0x2a)
 *      token 0xfb4:
 *          tsound_func(obj, 0x64)
 *          frame[frame].handler = t_wait_forever
 *      otherwise:    return -3
 *
 * `t_fani3`'s frame with two sounds and a pause added: stop the camera, plant
 * the object, run the animation as a call, wait 0x2a, make the second noise,
 * and stop. The ending is the same tail call to `t_wait_forever` -- the thread
 * does not return to whoever pushed it.
 *
 * The position is (0x50, 0x50) where `t_fani3` uses (0xc7, 0x50), so the
 * vertical is shared and the horizontal is not.
 *
 * 0xf40 goes into 0x2c, which `ani2_ob` masks to fourteen bits. It fits, so
 * this is an animation id written directly rather than resolved -- the third
 * way an animation reaches an object here, after a small number through
 * `get_char_ani` and a table address into 0x40.
 *
 * `_a_animality` is the eleventh named table this file reaches, and the third
 * of the finisher tables after `_a_ship` and `_a_friend`.
 */
extern uint32_t a_animality[];          /* 0x0016f674 */

long t_fx_animality(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0xfae) {
        *mk3_frame(thread, thread->frame + 1) = 0xfb4;
        thread->fieldfc = 0x2a;
        return 0x2a;
    }

    if (token == 0xfb4) {
        tsound_func(obj, 0x64);
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_wait_forever;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0)
        return -3;

    tsound_func(obj, 0x95);

    obj->field08->field2c = 0xf40;
    MK3_SET_FIELD0E(obj->field08, 0x50);
    MK3_SET_FIELD12(obj->field08, 0x50);

    obj->field40 = (uint32_t)(uintptr_t)a_animality;
    set_noscroll(obj);
    obj->field1c = 4;

    *mk3_frame(thread, thread->frame + 1) = 0xfae;
    thread->frame = thread->frame + 1;                  /* push a level */
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ----------------------------------------------------- t_spawn_endurance_guy
 *
 * armv7 0x00057740, two hundred and eight bytes.  **Complete.**
 *
 *      token 0:
 *          head = RoundParam[0x18]                     ; a signed byte
 *          for (k = 0; k < 9; k++)
 *              RoundParam[0x18 + k] = RoundParam[0x19 + k]
 *          i = 1 - obj->field48                        ; the other player
 *          StartProcAt(&Plyr[i], t_spawn_wingman)
 *          Plyr[i].field30 = (int8_t)head
 *          frame[frame+1].w0 = 0x1784
 *          frame = frame + 1                           ; push
 *          frame[frame].handler = t_continue_fighting
 *      token 0x1784:  unwind
 *      otherwise:     return -3
 *
 * **RoundParam+0x18 is a queue.** Ten signed bytes at least -- nine are shifted
 * and one is read -- holding the endurance opponents in order. This pops the
 * head and moves the rest down a place, which is the whole of "next fighter".
 *
 * That settles `t_is_endurance_possible`, which reads the same byte and asks
 * whether it is zero or positive. It is asking whether the queue still has
 * somebody in it, with a negative value as the end marker. Neither function
 * says so alone.
 *
 * `1 - obj->field48` turns the round result into the other player: 0 becomes 1
 * and 1 becomes 0. The winner is at 0x48 and the new fighter takes the loser's
 * slot, which is what an endurance match is.
 *
 * The shift is written as a byte loop over nine iterations with a pointer and
 * a limit, not a memmove, so the queue is short enough that a loop was
 * cheaper -- and it moves upward through overlapping bytes, which is only safe
 * in this direction.
 *
 * The character is sign-extended into `Plyr[i].field30` after the proc starts,
 * so `t_spawn_wingman` runs later and finds it there rather than being handed
 * it.
 */
long t_spawn_wingman(MK3THREAD *thread);

long t_spawn_endurance_guy(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    char    *rp    = (char *)RoundParam;
    int8_t   head;
    uint32_t i;
    int      k;

    if (token == 0x1784)
        return mk3_unwind(thread);

    if (token != 0)
        return -3;

    head = *(const int8_t *)(rp + 0x18);
    for (k = 0; k < 9; k++)                     /* pop the head */
        rp[0x18 + k] = rp[0x19 + k];

    i = 1u - obj->field48;                      /* the loser's slot */
    StartProcAt((MK3OBJ *)(Plyr + i * PLYR_STRIDE),
                (MK3THREADFUNC)t_spawn_wingman);
    *(uint32_t *)(Plyr + i * PLYR_STRIDE + 0x30) = (uint32_t)(int32_t)head;

    *mk3_frame(thread, thread->frame + 1) = 0x1784;
    thread->frame = thread->frame + 1;                  /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_continue_fighting;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------------------ t_bani2
 *
 * armv7 0x00058514, two hundred and twelve bytes.  **Complete.**
 *
 *      token 0:
 *          obj->a10 = obj->field40             ; remember the base
 *          p = obj->field40
 *          do { obj->field40 = p + 1;
 *               obj->field1c = p[1];
 *               p++; } while (p[0] != 0)       ; walk to the terminator
 *          fall through
 *      step back:
 *          obj->field40 = --p
 *          obj->field1c = p[0]
 *          ani2(obj)
 *          obj->field1c = args[argc-1]         ; peek the saved rate
 *          park(0x7c1, obj->field1c)
 *      token 0x7c1:
 *          if (obj->field40 != obj->a10) step back
 *          obj->field1c = args[--argc]         ; pop
 *          unwind
 *      otherwise:  return -3
 *
 * The animation run in reverse, and **there is no length**. The first entry
 * walks forward from the base until it finds the zero that ends the script --
 * the same terminator `stance_setup` scans for -- and then plays entries
 * backwards, one per tick, until the cursor is back where it started.
 *
 * The comparison happens before the step, so the entry AT the base is never
 * played. Written out as it stands; whether that is intended is not something
 * this function says.
 *
 * The rate comes off the argument stack, where `t_backwards_ani` left it
 * before pushing this -- the only place in the file where a value crosses
 * between frames on that stack rather than being saved and restored in one
 * routine. It is peeked on every tick with the pop-and-push-back the mframew
 * family uses, and popped for real on the way out.
 *
 * The park duration is that saved rate and not whatever `ani2` leaves in 0x1c,
 * because the peek overwrites 0x1c between the two.
 *
 * A10 holds the base for the life of the animation, which is the same use
 * `t_jump_up_land_jsrp` makes of it -- a pointer surviving across a park in
 * the object rather than on the stack.
 */

long t_bani2(MK3THREAD *thread)
{
    MK3OBJ   *obj   = (MK3OBJ *)thread->proc;
    uint32_t  token = *mk3_frame(thread, thread->frame + 1);
    uint32_t *p;
    uint32_t  argc;

    if (token != 0 && token != 0x7c1)
        return -3;

    if (token == 0x7c1) {
        if (obj->field40 == obj->a10) {         /* back at the start */
            argc = thread->fieldf8 - 1;
            thread->fieldf8 = argc;
            obj->field1c = *mk3_arg(thread, argc);
            return mk3_unwind(thread);
        }
    } else {
        obj->a10 = obj->field40;                /* remember the base */
        p = (uint32_t *)(uintptr_t)obj->field40;
        do {                                    /* walk to the terminator */
            obj->field40 = (uint32_t)(uintptr_t)(p + 1);
            obj->field1c = p[1];
            p++;
        } while (p[0] != 0);
    }

    p = (uint32_t *)(uintptr_t)obj->field40;
    p--;
    obj->field40 = (uint32_t)(uintptr_t)p;
    obj->field1c = p[0];
    ani2(obj);

    argc = thread->fieldf8 - 1;                 /* peek the saved rate */
    thread->fieldf8 = argc;
    obj->field1c = *mk3_arg(thread, argc);
    *mk3_arg(thread, argc) = obj->field1c;
    thread->fieldf8 = argc + 1;

    *mk3_frame(thread, thread->frame + 1) = 0x7c1;
    thread->fieldfc = obj->field1c;
    return (long)obj->field1c;
}


/* ---------------------------------------------------- t_back_to_shang_check
 *
 * armv7 0x00057924, two hundred and sixteen bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      am_i_shang(obj)
 *      if (obj->field5c == 0)                      unwind
 *      obj->field1c = obj->field08->field24
 *      if (obj->field1c == 0xc)                    unwind
 *      obj->field1c = G + 0x3fc
 *      get_tsl_px(obj, obj)
 *      if ((int32_t)obj->field20 < 0x200)          unwind
 *      if (frame > 0) frame = frame - 1
 *      frame[frame].handler = t_back_to_shang_form
 *      frame[frame+1].w0 = 0
 *
 * Three tests before it will change anything: he must be Shang, his current
 * form must not be character 0xc, and a value looked up in a table must be at
 * least 0x200. Only then does the morph happen.
 *
 * **The last arm is not an unwind.** It drops a level exactly as one would and
 * then installs a DIFFERENT handler there, so the caller is both returned to
 * and replaced. Nothing else in this file does that; every other pop leaves
 * the parent's handler alone.
 *
 * The table is G+0x3fc, which is not one of the four `get_tsl_px` swaps for
 * the four-button scheme -- a fifth table reached the same way and left alone
 * by that gate.
 *
 * The lookup goes through `get_tsl_px` with the object as both arguments, so
 * the PROC it reads the gate and the index from is this fighter's own.
 *
 * Between the decrement and the install the compiler copies level+1 down to
 * level -- its handler AND its first word -- and then overwrites both.
 *
 * That copy is an idiom, not noise: `t_clock_ran_out` emits the same six
 * instructions and does NOT overwrite the handler half, so it means "drop a
 * level and take my place in it". Here the handler is replaced immediately
 * afterwards by a different one, which is what makes this routine a return
 * that replaces its caller rather than one that carries itself down. The
 * stores are dead in this instance and the shape is not.
 *
 * The frame <= 0 case reaches the same install after writing
 * `t_local_reaction_exit` at the same slot, which is likewise overwritten. So
 * at the bottom of the stack this routine still becomes the morph rather than
 * exiting.
 */
long t_back_to_shang_form(MK3THREAD *thread);

long t_back_to_shang_check(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    am_i_shang(obj);
    if (obj->field5c == 0)
        return mk3_unwind(thread);

    obj->field1c = obj->field08->field24;
    if (obj->field1c == 0xc)                    /* already that form */
        return mk3_unwind(thread);

    obj->field1c = (uint32_t)(uintptr_t)(G_BYTES + 0x3fc);
    get_tsl_px(obj, obj);
    if ((int32_t)obj->field20 < 0x200)
        return mk3_unwind(thread);

    if ((int32_t)thread->frame > 0)
        thread->frame = thread->frame - 1;
    else {
        /* Written and then overwritten below -- see the note. */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_local_reaction_exit;
        *mk3_frame(thread, thread->frame + 1) = 0;
    }

    /* Dead: both halves are overwritten by the two stores that follow. */
    *mk3_frame(thread, thread->frame + 1) =
        *mk3_frame(thread, thread->frame + 2);
    mk3_frame(thread, thread->frame)[1] =
        mk3_frame(thread, thread->frame + 1)[1];

    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_back_to_shang_form;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ----------------------------------------------------- t_back_to_shang_form
 *
 * armv7 0x0005a330, two hundred and thirty-two bytes.  **Complete**, and the
 * first function here that names a character.
 *
 *      token 0:
 *          obj->field20 = 0x401
 *          init_special_act(obj)
 *          clear_inviso(obj)
 *          obj->field20 = 0xc
 *          obj->field1c = 4
 *          borrow_ochar_sound(obj)
 *          mine  = ochar_ground_offsets[obj->field08->field24]
 *          shang = ochar_ground_offsets[12]
 *          obj->field20 = shang
 *          obj->field1c = mine - shang
 *          obj->field24 = (int16_t)obj->field08->field12 + (mine - shang)
 *          obj->field08->field12 = obj->field24
 *          park(0x18ae, 4)
 *      token 0x18ae:
 *          obj->field1c = 0xc
 *          obj->field08->field24 = 0xc             ; he is Shang again
 *          player_normpal(obj)
 *          obj->field40 = 0
 *          do_first_a9_frame(obj)
 *          ground_ochar(obj)
 *          obj->field00->field40 = (int16_t)obj->field08->field12
 *          park(0x18c1, 4)
 *      token 0x18c1:  install t_local_reaction_exit
 *      otherwise:     return -3
 *
 * **Character 0xc is Shang Tsung.** The routine is called
 * `t_back_to_shang_form`, it writes 0xc into the character field, and
 * `t_back_to_shang_check` refuses when the character is already 0xc. That is
 * three uses of one constant across two functions whose SYMBOL NAMES say what
 * it is, so the identification comes off the symbol table and not off a guess
 * at the roster. It is the first character id this file can name.
 *
 * The vertical adjustment is what makes the morph work. `_ochar_ground_offsets`
 * has one entry per character, indexed by the character number and read as
 * words; entry 12 is Shang's own. The difference between the current form's
 * offset and his is added to y BEFORE the character changes, so the feet stay
 * where they were. Without it a morph would leave him hanging or sunk.
 *
 * The order matters and is preserved: the position is fixed while he is still
 * the other character, and only then does the character number change.
 *
 * The PROC's 0x40 -- the ground, which `ground_player` reads -- is refreshed
 * from the new y at the end, after `ground_ochar` has had its say.
 *
 * `_ochar_ground_offsets` at 0x0016ef04 is the twelfth named table this file
 * reaches, and the second whose entries are per character rather than per
 * animation.
 */
extern uint32_t ochar_ground_offsets[];  /* 0x0016ef04, one word a character */


#define MK3_CHAR_SHANG  0xc

long t_back_to_shang_form(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x18ae) {
        obj->field1c = MK3_CHAR_SHANG;
        obj->field08->field24 = MK3_CHAR_SHANG;

        player_normpal(obj);
        obj->field40 = 0;
        do_first_a9_frame(obj);
        ground_ochar(obj);

        obj->field00->field40 = (uint32_t)(int32_t)MK3_FIELD12_S(obj->field08);   /* ldrsh */

        *mk3_frame(thread, thread->frame + 1) = 0x18c1;
        thread->fieldfc = 4;
        return 4;
    }

    /* not an unwind: installs the handler in pointer slot 0x000f3708
     * (t_local_reaction_exit) at this level (difftest) */
    if (token == 0x18c1)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

    if (token != 0)
        return -3;

    obj->field20 = 0x401;
    init_special_act(obj);
    clear_inviso(obj);

    obj->field20 = MK3_CHAR_SHANG;
    obj->field1c = 4;
    borrow_ochar_sound(obj);

    {
        uint32_t mine  = ochar_ground_offsets[obj->field08->field24];
        uint32_t shang = ochar_ground_offsets[MK3_CHAR_SHANG];
        uint32_t d     = mine - shang;

        obj->field1c = mine;
        obj->field20 = shang;
        obj->field1c = d;

        /* Move him before he changes, so the feet stay put. */
        obj->field24 = (uint32_t)((int32_t)(int16_t)MK3_FIELD12(obj->field08)
                                  + (int32_t)d);
        MK3_SET_FIELD12(obj->field08, obj->field24);
    }

    *mk3_frame(thread, thread->frame + 1) = 0x18ae;
    thread->fieldfc = 4;
    return 4;
}


/* ----------------------------------------------------------------- t_clock4
 *
 * armv7 0x000562c0, two hundred and thirty-two bytes.  **Complete**, and it is
 * the countdown that `t_clock3` displays.
 *
 *      token 0:       park(0x10e2, 3)
 *      token 0x10e2:
 *          obj->field1c = 0
 *          if ((int32_t)G[0x368] <= 0)  next = t_round_is_over
 *          else if ((int32_t)G[0x36c] <= 0) next = t_round_is_over
 *          else if (RoundParam[0x2c] & 1)   next = t_clock4     ; frozen
 *          else if (--obj->field3c > 0)     next = t_clock4
 *          else if (--obj->a10 >= 0)        next = t_clock3
 *          else { obj->a10 = 9; obj->field48 -= 1; next = t_clock3 }
 *      otherwise:     return -3
 *
 * A two-digit decimal countdown written out in full. 0x3c counts ticks down to
 * the next second; `a10` is the units digit and rolls from 0 back to 9;
 * `field48` is the tens. `t_clock3` reads those two and packs them a nibble
 * apart for the display, so the two functions agree about which digit is which
 * from opposite ends.
 *
 * Either health reaching zero ends the round before the clock is touched, so a
 * knockout on the same tick as a second boundary is a knockout.
 *
 * **Bit 0 of RoundParam+0x2c freezes the clock**: the routine reinstalls
 * itself and the digits never move. That is a fifth field of that struct --
 * 0x0c difficulty, 0x10 the round, 0x14 the ladder order, 0x18 the endurance
 * queue, and now 0x2c -- and it is what a "no time limit" option would set.
 *
 * The self-install and the two-digit paths write different registers into the
 * slot above, all of them zero, which is why the disassembly stores three
 * different names into one place.
 *
 * The tick reload is not visible here: 0x3c is decremented and, when it
 * reaches zero, the digit moves -- but nothing in this function puts a fresh
 * count back into 0x3c. Whatever does is elsewhere, and it is left as a gap
 * rather than assumed.
 */
extern int PortPractice;                /* FrontEnd.c, port: training */

/* Port, not in the binary: the debug menu's FREEZE CLOCK and INFINITE HEALTH
 * (runtime/debug_menu.c sets them; bit 0 player one, bit 1 player two). */
int PortDbgClock;
int PortDbgHealth;

long t_clock4(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    MK3THREADFUNC next;

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x10e2;
        thread->fieldfc = 3;
        return 3;
    }

    if (token != 0x10e2)
        return -3;

    obj->field1c = 0;

    if ((int32_t)*(const uint32_t *)(G_BYTES + 0x368) <= 0 ||
        (int32_t)*(const uint32_t *)(G_BYTES + 0x36c) <= 0) {
        next = (MK3THREADFUNC)t_round_is_over;
    } else if ((((const uint32_t *)RoundParam)[0x2c / 4] & 1) != 0
               || PortPractice          /* port: training stops it too */
               || PortDbgClock) {       /* port: and the debug menu */
        next = (MK3THREADFUNC)t_clock4;         /* the clock is frozen */
    } else {
        obj->field3c = obj->field3c - 1;
        if ((int32_t)obj->field3c > 0) {
            next = (MK3THREADFUNC)t_clock4;     /* not a second yet */
        } else {
            obj->a10 = obj->a10 - 1;            /* the units digit */
            if ((int32_t)obj->a10 < 0) {
                obj->a10 = 9;                   /* roll, and carry */
                obj->field48 = obj->field48 - 1;
            }
            next = (MK3THREADFUNC)t_clock3;
        }
    }

    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)next;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------------ t_results_retp
 *
 * armv7 0x000569f0, two hundred and forty-four bytes.  **Complete.**
 *
 *      token 0:
 *          w = (int16_t)G[0x45c]                   ; the winner, 1 or 2
 *          obj->field1c = w
 *          if (RoundParam[0x3c] != 0
 *              && (uint16_t)(w - 1) <= 1
 *              && (Pp[w - 1].field10 & 1)
 *              && H[w - 1] == 2) {
 *              frame[frame+1].w0 = 0x121f
 *              frame = frame + 1                   ; push
 *              frame[frame].handler = t_game_finished
 *          } else unwind
 *      token 0x121f:  unwind
 *      otherwise:     return -3
 *
 * What runs after a round is won -- `t_player_1_won` and `t_player_2_won` both
 * install it -- and what it decides is whether the MATCH is over.
 *
 * **Two round wins ends it.** `H[0]` and `H[1]` are the tallies those two
 * routines increment, and this compares one of them against 2. That is
 * best-of-three, and it is the only place the number appears.
 *
 * The winner in G+0x45c is one-based, so it is turned into an index by
 * subtracting one -- and the bounds check is done on the UNSIGNED difference:
 * `(uint16_t)(w - 1) <= 1` accepts 1 and 2 and rejects 0 in a single
 * comparison, because 0 - 1 wraps to 0xffff. A signed test would have needed
 * two.
 *
 * `Pp[w-1].field10 & 1` is the same bit `t_is_endurance_possible` reads on the
 * same struct, so that flag gates both the endurance queue and the end of the
 * match. What it means is not in either function.
 *
 * RoundParam+0x3c gates the whole thing -- a sixth field of that struct, after
 * 0x0c, 0x10, 0x14, 0x18 and 0x2c. With it clear the match never ends here.
 */
long t_game_finished(MK3THREAD *thread);

long t_results_retp(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    int32_t  w;

    if (token == 0x121f)
        return mk3_unwind(thread);

    if (token != 0)
        return -3;

    w = *(const int16_t *)(G_BYTES + 0x45c);
    obj->field1c = (uint32_t)w;

    if (((const uint32_t *)RoundParam)[0x3c / 4] != 0 &&
        (uint16_t)(w - 1) <= 1) {
        uint32_t i = (uint32_t)(w - 1);

        if ((*(const uint32_t *)(Pp + i * PP_STRIDE + 0x10) & 1) != 0 &&
            ((const uint32_t *)H)[i] == 2) {            /* best of three */
            *mk3_frame(thread, thread->frame + 1) = 0x121f;
            thread->frame = thread->frame + 1;          /* push a level */
            mk3_frame(thread, thread->frame)[1] =
                (uint32_t)(uintptr_t)t_game_finished;
            *mk3_frame(thread, thread->frame + 1) = 0;
            return 0;
        }
    }

    return mk3_unwind(thread);
}


/* ------------------------------------------------------------- t_do_jump_up
 *
 * armv7 0x000580e8, two hundred and forty-eight bytes.  **Complete.**
 *
 *      token 0:
 *          obj->field1c = 1
 *          group_sound(obj)
 *          obj->field1c = 0x30a
 *          obj->field00->field18 = 0x30a
 *          obj->field40 = 0x16
 *          get_char_ani(obj)
 *          obj->field1c = 0
 *          obj->field40 = obj->field40 + 4         ; skip the first entry
 *          obj->field34 = obj->field48
 *          obj->field20 = 0xfff60000               ; -10.0 in 16.16
 *          obj->field24 = 0x00008000               ;  +0.5 in 16.16
 *          obj->field28 = 4
 *          frame[frame+1].w0 = 0x840
 *          frame = frame + 1                       ; push
 *          frame[frame].handler = t_flight_call
 *      token 0x840:
 *          frame[frame+1].w0 = 0x842
 *          frame = frame + 1                       ; push again
 *          frame[frame].handler = t_jump_up_land_jsrp
 *      token 0x842:  unwind
 *      otherwise:    return -3
 *
 * A straight jump, start to finish: make the noise, commit the action, load
 * the animation, set three numbers, fly, land.
 *
 * **The two words are 16.16 fixed point.** 0xfff60000 is -10.0 and 0x00008000
 * is 0.5 -- the same format `is_he_right` and the 0x0e/0x12 accessors use for
 * coordinates. The compiler forms them from ONE pool entry: it loads
 * 0xfff60000 and adds 0xa8000, which wraps to 0x8000. Two constants, one
 * literal, and the second looks derived when it is not.
 *
 * What they are is `t_flight_call`'s business; this routine only sets them.
 * Given a jump, an upward ten and a downward half read as a velocity and a
 * gravity, but neither is named here and neither is assumed.
 *
 * The animation cursor is advanced past its first entry before the flight
 * starts, as in `t_jump_up_land_jsrp` -- the same four bytes, and the same
 * reason: the first frame belongs to the take-off, not the arc.
 *
 * Action 0x30a joins the movement family: 0x301 backing up, 0x302 ducking,
 * 0x304 landing, 0x30a jumping.
 *
 * The two pushes never pop between them, so the flight and the landing run at
 * the same depth one after the other rather than nested.
 */

long t_do_jump_up(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x840) {
        *mk3_frame(thread, thread->frame + 1) = 0x842;
        thread->frame = thread->frame + 1;              /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_jump_up_land_jsrp;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x842)
        return mk3_unwind(thread);

    if (token != 0)
        return -3;

    obj->field1c = 1;
    group_sound(obj);

    obj->field1c = 0x30a;
    obj->field00->field18 = 0x30a;

    obj->field40 = 0x16;
    get_char_ani(obj);
    obj->field1c = 0;
    obj->field40 = obj->field40 + 4;            /* past the take-off frame */

    obj->field34 = obj->field48;
    obj->field20 = 0xfff60000u;                 /* -10.0 in 16.16 */
    obj->field24 = 0xfff60000u + 0xa8000u;      /*  +0.5, from one literal */
    obj->field28 = 4;

    *mk3_frame(thread, thread->frame + 1) = 0x840;
    thread->frame = thread->frame + 1;                  /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_flight_call;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ---------------------------------------------------------- t_clock_ran_out
 *
 * armv7 0x000568f4, two hundred and fifty-two bytes.  **Complete.**
 *
 *      token 0:
 *          if (frame > 0) frame = frame - 1
 *          else { frame[frame].handler = t_local_reaction_exit;
 *                 frame[frame+1].w0 = 0; }
 *          frame[frame+1].w0     = frame[frame+2].w0
 *          frame[frame].handler  = frame[frame+1].handler   ; carry me down
 *          obj->field1c = 1
 *          G[0x452] = (uint16_t)obj->field20
 *          frame[frame+1].w0 = 0x11ee
 *          frame = frame + 1                                ; push
 *          frame[frame].handler = t_print_timeout_msg
 *      token 0x11ee:
 *          frame[frame].handler = t_play3
 *          frame[frame+1].w0 = 0
 *          frame = frame + 1                                ; push
 *          frame[frame].handler = t_round_is_over
 *      otherwise:  return -3
 *
 * What `t_results_of_round` reaches when the outcome is anything but 0, 1 or 2.
 *
 * The first step **pops a level and carries this handler down into it**. That
 * is the same six-instruction shuffle `t_back_to_shang_check` emits, and here
 * the handler half survives -- nothing writes over it -- so the idiom has a
 * meaning: drop a level and take the parent's place. In the other routine a
 * different handler is stored immediately afterwards, which is what makes that
 * one a return that REPLACES its caller. One shape, two uses.
 *
 * `G[0x452]` is the halfword `t_fatality_wait` reads as "no". A round that ran
 * out of time refuses the fatality by writing it here, from 0x20 rather than
 * from a constant.
 *
 * The second step is the clearest call-with-continuation in the file: install
 * `t_play3` at the current level and then push `t_round_is_over` above it. When
 * that unwinds, `t_play3` is what it comes back to -- so "do X, then continue
 * as Y" is two stores and an increment.
 */
long t_play3(MK3THREAD *thread);

long t_clock_ran_out(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x11ee) {
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_play3;
        *mk3_frame(thread, thread->frame + 1) = 0;

        thread->frame = thread->frame + 1;              /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_round_is_over;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0)
        return -3;

    if ((int32_t)thread->frame > 0) {
        thread->frame = thread->frame - 1;
    } else {
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_local_reaction_exit;
        *mk3_frame(thread, thread->frame + 1) = 0;
    }

    /* Drop a level and take its place: the handler above comes down. */
    *mk3_frame(thread, thread->frame + 1) =
        *mk3_frame(thread, thread->frame + 2);
    mk3_frame(thread, thread->frame)[1] =
        mk3_frame(thread, thread->frame + 1)[1];

    obj->field1c = 1;
    *(uint16_t *)(G_BYTES + 0x452) = (uint16_t)obj->field20;

    *mk3_frame(thread, thread->frame + 1) = 0x11ee;
    thread->frame = thread->frame + 1;                  /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_print_timeout_msg;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------------ t_flight_loop
 *
 * armv7 0x0005aa1c, two hundred and fifty-two bytes.  **Complete**, and the
 * first routine here to install a handler that is not a link-time constant.
 *
 *      token 0:       park(0x1b5, 1)
 *      token 0x1b5:
 *          obj->field34 = obj->field00->field34
 *          if (obj->field34 != 0) {
 *              args[argc++] = obj->a10
 *              frame[frame+1].w0 = 0x1bb
 *              frame = frame + 1                   ; push
 *              frame[frame].handler = obj->field34 ; from DATA
 *              frame[frame+1].w0 = 0
 *              return 0
 *          }
 *          fall through
 *      token 0x1bb:
 *          obj->a10 = args[--argc]
 *          fall through
 *      common:
 *          next_anirate(obj)
 *          y = (int16_t)obj->field08->field12
 *          obj->field20 = y
 *          obj->field1c = obj->field00->field40            ; the ground
 *          if (obj->field00->field40 > y) park(0x1b5, 1)   ; still airborne
 *          obj->field1c = 0
 *          obj->field08->field20 = 0
 *          stop_me_player(obj)
 *          ground_player(obj)
 *          unwind
 *      otherwise:  return -3
 *
 * The body of a jump. One tick per pass: advance the animation rate, look at
 * y, and either park again or land.
 *
 * **The PROC's 0x34 is a callback.** Everywhere else a handler is a constant
 * the linker fixed; here one is read out of the object and pushed as a frame,
 * so a flight can carry a routine of its own -- and it is optional, because
 * zero there skips the push and falls straight into the common part.
 * `t_flight` clears that slot before starting, so a flight with no callback is
 * the default.
 *
 * The A10 is stacked around that call and popped when the callback unwinds,
 * which is why the two tokens have to be different: 0x1b5 means "I am waiting"
 * and 0x1bb means "my callback just finished, there is something to pop".
 *
 * Landing is `ground > y`, on the PROC's 0x40 -- the same ground `ground_player`
 * copies out and `distance_off_ground` measures against. So y grows downward
 * and the test is "the floor is still below me".
 *
 * The zero into `obj->field08->field20` is on the other object, not this one,
 * and is not restored.
 */
long t_flight_loop(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    uint32_t argc;
    int32_t  y, ground;

    if (token != 0 && token != 0x1b5 && token != 0x1bb)
        return -3;

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x1b5;
        thread->fieldfc = 1;
        return 1;
    }

    if (token == 0x1b5) {
        obj->field34 = *(const uint32_t *)((char *)obj->field00 + 0x34);

        if (obj->field34 != 0) {                /* an optional callback */
            argc = thread->fieldf8;
            *mk3_arg(thread, argc) = obj->a10;
            thread->fieldf8 = argc + 1;

            *mk3_frame(thread, thread->frame + 1) = 0x1bb;
            thread->frame = thread->frame + 1;          /* push a level */
            mk3_frame(thread, thread->frame)[1] = obj->field34;
            *mk3_frame(thread, thread->frame + 1) = 0;
            return 0;
        }
    } else {                                    /* 0x1bb: it just finished */
        argc = thread->fieldf8 - 1;
        thread->fieldf8 = argc;
        obj->a10 = *mk3_arg(thread, argc);
    }

    next_anirate(obj);

    y = (int32_t)(int16_t)MK3_FIELD12(obj->field08);
    obj->field20 = (uint32_t)y;
    ground = (int32_t)obj->field00->field40;
    obj->field1c = (uint32_t)ground;

    if (ground > y) {                           /* the floor is still below */
        *mk3_frame(thread, thread->frame + 1) = 0x1b5;
        thread->fieldfc = 1;
        return 1;
    }

    obj->field1c = 0;
    obj->field08->field20 = 0;
    stop_me_player(obj);
    ground_player(obj);
    return mk3_unwind(thread);
}


/* ------------------------------------------------------------------ t_attk2
 *
 * armv7 0x000594d4, two hundred and sixty bytes.  **Complete**, and it is the
 * far end of the striker chain's arguments.
 *
 *      token 0:
 *          obj->field20 = args[--argc]
 *          obj->field1c = args[--argc]
 *          frame[frame+1].w0 = 0xd35
 *          frame = frame + 1                       ; push
 *          frame[frame].handler = t_act_mframew
 *      token 0xd40:
 *          obj->a10 = obj->a10 - 1
 *          if (obj->a10 <= 0) { obj->field5c = 0; unwind }
 *          fall through
 *      token 0xd35:
 *          obj->field1c = obj->field48
 *          strike_check_a0(obj)
 *          if (obj->field5c == 0) park(0xd40, 1)
 *          obj->field5c = 1
 *          unwind
 *      otherwise:  return -3
 *
 * **The two words come from `t_striker`.** That routine pushed
 * `proc->field1c` and `proc->field20` and then installed `t_attk3`; `t_attk3`
 * installed `t_attk5` and `t_attk5` installed `t_attk2`. All three are tail
 * calls at the same level, so nothing touches the argument stack in between,
 * and the two words are still sitting there when this pops them -- in the
 * right order, last in first out.
 *
 * That is the chain's entire argument-passing convention, and neither end says
 * so alone: the pusher does not know who will read them and the reader does not
 * name who pushed.
 *
 * After the animation runs as a call, the routine becomes a per-tick strike
 * test with a countdown on the A10. It ends two ways and says which in 0x5c:
 * 1 when the strike connects, 0 when the count runs out. Both unwind, so the
 * caller reads the answer rather than the destination.
 *
 * The 0xd40 arm falls INTO the 0xd35 body rather than duplicating it, which is
 * why one token decrements and the other does not and both reach the same
 * strike check.
 */
long t_attk2(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    uint32_t argc;

    if (token != 0 && token != 0xd35 && token != 0xd40)
        return -3;

    if (token == 0) {
        argc = thread->fieldf8 - 1;             /* what t_striker pushed */
        thread->fieldf8 = argc;
        obj->field20 = *mk3_arg(thread, argc);
        argc = thread->fieldf8 - 1;
        thread->fieldf8 = argc;
        obj->field1c = *mk3_arg(thread, argc);

        *mk3_frame(thread, thread->frame + 1) = 0xd35;
        thread->frame = thread->frame + 1;              /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_act_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0xd40) {
        obj->a10 = obj->a10 - 1;
        if ((int32_t)obj->a10 <= 0) {           /* out of time */
            obj->field5c = 0;
            return mk3_unwind(thread);
        }
    }

    obj->field1c = obj->field48;
    strike_check_a0(obj);

    if (obj->field5c == 0) {                    /* nothing yet */
        *mk3_frame(thread, thread->frame + 1) = 0xd40;
        thread->fieldfc = 1;
        return 1;
    }

    obj->field5c = 1;                           /* it connected */
    return mk3_unwind(thread);
}


/* ------------------------------------------------------- t_animate_a0_frames
 *
 * armv7 0x0005a070, two hundred and sixty-four bytes.  **Complete.**
 *
 *      token 0:
 *          args[argc++] = obj->a10
 *          obj->field20 = obj->field1c >> 16       ; the rate
 *          args[argc++] = obj->field20
 *          obj->a10 = (uint16_t)obj->field1c       ; the frame count
 *          if (do_next_a9_frame(obj) == 0) goto done
 *          goto tick
 *      token 0x730:
 *          obj->a10 = obj->a10 - 1
 *          if ((int32_t)obj->a10 < 0) goto done
 *          if (do_next_a9_frame(obj) == 0) goto done
 *          goto tick
 *      tick:
 *          obj->field1c = args[argc-1]             ; peek the rate
 *          park(0x730, obj->field1c)
 *      done:
 *          obj->field1c = args[--argc]
 *          obj->a10     = args[--argc]
 *          unwind
 *      otherwise:  return -3
 *
 * The mframew family with a count and a caller's rate. 0x1c arrives **packed**:
 * the low half is how many frames to play and the high half is how long each
 * one lasts. `t_animate_a9` packs 0x40 the same way, so a halfword pair in one
 * slot is this file's habit rather than a one-off.
 *
 * `t_mframew` parks for whatever rate the animation itself carries; this parks
 * for the rate the caller supplied and stops after the count -- or when the
 * animation ends, whichever comes first. Two ways out and the same unwind, so
 * a caller cannot tell which happened.
 *
 * The count lives in A10 for the duration, which is only safe because the old
 * A10 is the first thing pushed. Both words stay on the argument stack across
 * every tick and come off in the right order at the end.
 *
 * The rate is peeked with the pop-and-push-back the family uses everywhere,
 * and it is the value that goes into 0xfc -- a duration read out of a caller's
 * word, which is the second runtime value that field takes.
 *
 * On the way out 0x1c is left holding the rate, not the packed word it
 * arrived as. Transcribed; nothing here puts the pair back together.
 */
long t_animate_a0_frames(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    uint32_t argc;
    int      done = 0;

    if (token != 0 && token != 0x730)
        return -3;

    if (token == 0) {
        argc = thread->fieldf8;
        *mk3_arg(thread, argc) = obj->a10;
        thread->fieldf8 = argc + 1;

        obj->field20 = obj->field1c >> 16;              /* the rate */
        argc = thread->fieldf8;
        *mk3_arg(thread, argc) = obj->field20;
        thread->fieldf8 = argc + 1;

        obj->a10 = (uint16_t)obj->field1c;              /* the count */

        if (do_next_a9_frame(obj) == 0)
            done = 1;
    } else {
        obj->a10 = obj->a10 - 1;
        if ((int32_t)obj->a10 < 0 || do_next_a9_frame(obj) == 0)
            done = 1;
    }

    if (done) {
        argc = thread->fieldf8 - 1;
        thread->fieldf8 = argc;
        obj->field1c = *mk3_arg(thread, argc);
        argc = thread->fieldf8 - 1;
        thread->fieldf8 = argc;
        obj->a10 = *mk3_arg(thread, argc);
        return mk3_unwind(thread);
    }

    argc = thread->fieldf8 - 1;                 /* peek the rate */
    thread->fieldf8 = argc;
    obj->field1c = *mk3_arg(thread, argc);
    *mk3_arg(thread, argc) = obj->field1c;
    thread->fieldf8 = argc + 1;

    *mk3_frame(thread, thread->frame + 1) = 0x730;
    thread->fieldfc = obj->field1c;
    return (long)obj->field1c;
}


/* ------------------------------------------------------------ t_flight_call
 *
 * armv7 0x00055aec, two hundred and eighty bytes.  **Complete**, and it sets a
 * flight up.
 *
 *      token 0:
 *          args[argc++] = obj->field28
 *          if (obj->field20 != 0xd) obj->field08->field1c = obj->field20
 *          if (obj->field24 != 0xd) obj->field08->field20 = obj->field24
 *          if (obj->field1c != 0xd) away_x_vel(obj)
 *          a = obj->field40
 *          if ((int32_t)a <= 0x48) get_char_ani(obj)
 *          else if ((uint16_t)a <= 0x47 && (int32_t)a >> 16 == 1) {
 *              obj->field40 = (uint16_t)a
 *              get_char_ani2(obj)
 *          }
 *          obj->field1c = args[--argc]
 *          if (obj->field1c != 0xd) init_anirate(obj)
 *          obj->field00->field34 = obj->field34        ; the callback
 *          y = (int16_t)obj->field08->field12
 *          if (obj->field00->field40 < y)
 *              obj->field08->field12 = obj->field00->field40
 *          frame[frame+1].w0 = 0x1f9
 *          frame = frame + 1                           ; push
 *          frame[frame].handler = t_flight_loop
 *      token 0x1f9:  unwind
 *      otherwise:    return -3
 *
 * **0xd is a sentinel.** Four slots are compared against it and skipped when
 * they match: the two velocities the caller may or may not want set, the rate,
 * and the horizontal push. So 13 is not a value in those fields, it is "leave
 * this alone", and a caller fills the ones it cares about and puts 0xd in the
 * rest. `t_do_jump_up` sets all three of its numbers, so none of them is 0xd
 * there -- the sentinel only shows when some other caller wants a partial
 * flight.
 *
 * It installs the callback `t_flight_loop` reads: the PROC's 0x34 takes the
 * object's 0x34, which `t_do_jump_up` loaded out of 0x48 before starting. That
 * is the whole path by which a jump gets a routine of its own, and it crosses
 * three functions.
 *
 * The animation resolution is a THIRD form of the tagged union: at or below
 * 0x48 it is a plain number for `get_char_ani`; above that, if the low half is
 * at or below 0x47 AND the high half is exactly 1, the low half is a number
 * for `get_char_ani2`. Two halves and two resolvers, chosen by a value in the
 * high half rather than by a range.
 *
 * The last step clamps y up to the ground when it has gone past -- `strh` under
 * `lt`, so it only ever moves the fighter back onto the floor and never off it.
 *
 * 0x28 goes onto the argument stack at the start and comes off at the end,
 * across the calls in between; it is a real pop, not the peek the mframew
 * family uses.
 */

long t_flight_call(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    uint32_t argc, a;
    int32_t  y, ground;

    if (token == 0x1f9)
        return mk3_unwind(thread);

    if (token != 0)
        return -3;

    argc = thread->fieldf8;
    *mk3_arg(thread, argc) = obj->field28;
    thread->fieldf8 = argc + 1;

    if (obj->field20 != 0xd)                    /* 0xd: leave it alone */
        obj->field08->field1c = obj->field20;
    if (obj->field24 != 0xd)
        obj->field08->field20 = obj->field24;
    if (obj->field1c != 0xd)
        away_x_vel(obj);

    a = obj->field40;
    if ((int32_t)a <= 0x48) {
        get_char_ani(obj);
    } else if ((uint16_t)a <= 0x47 && ((int32_t)a >> 16) == 1) {
        obj->field40 = (uint16_t)a;
        get_char_ani2(obj);
    }

    argc = thread->fieldf8 - 1;
    thread->fieldf8 = argc;
    obj->field1c = *mk3_arg(thread, argc);
    if (obj->field1c != 0xd)
        init_anirate(obj);

    /* What t_flight_loop will run once a tick, or nothing if it is zero. */
    *(uint32_t *)((char *)obj->field00 + 0x34) = obj->field34;

    y = (int32_t)(int16_t)MK3_FIELD12(obj->field08);
    obj->field20 = (uint32_t)y;
    ground = (int32_t)obj->field00->field40;
    obj->field1c = (uint32_t)ground;
    if (ground < y)                             /* never off the floor */
        MK3_SET_FIELD12(obj->field08, (uint32_t)ground);

    *mk3_frame(thread, thread->frame + 1) = 0x1f9;
    thread->frame = thread->frame + 1;                  /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_flight_loop;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------------ t_gravity_ani
 *
 * armv7 0x0005a870, two hundred and eighty-eight bytes.  **Complete.**
 *
 *      token 0:
 *          obj->field00->field34 = obj->field34        ; the callback
 *          if (obj->field20 != 0) obj->field08->field1c = obj->field20
 *          init_anirate(obj)
 *          park(0x1684, 1)
 *      token 0x1684:
 *          obj->field34 = obj->field00->field34
 *          if (obj->field34 != 0) {
 *              args[argc++] = obj->a10
 *              frame[frame+1].w0 = 0x168a
 *              frame = frame + 1                       ; push
 *              frame[frame].handler = obj->field34
 *          } else fall through
 *      token 0x168a:
 *          obj->a10 = args[--argc]
 *          fall through
 *      common:
 *          next_anirate(obj)
 *          obj->field1c = obj->field08->field1c + obj->a10
 *          obj->field08->field1c = obj->field1c
 *          if ((int32_t)obj->field1c < 0)   park(0x1684, 1)
 *          if ((int16_t)obj->field08->field12 < obj->field00->field40)
 *                                           park(0x1684, 1)
 *          stop_me_player(obj)
 *          ground_player(obj)
 *          obj->field1c = 0
 *          obj->field00->p_hit = 0
 *          unwind
 *      otherwise:  return -3
 *
 * `t_flight_loop`'s shape with the arithmetic actually in it. That routine
 * only waits for y to reach the ground; this one **integrates**: every tick it
 * adds A10 to 0x1c of the other object, so A10 is an acceleration and that
 * slot is a velocity.
 *
 * There are two reasons to keep going and they park on the same token: the
 * velocity is still negative -- he is on the way up -- or y has not reached the
 * floor. Either one alone would be wrong, because a rising fighter is above
 * the floor and a falling one may have negative velocity for a while yet.
 *
 * The optional callback is the same one `t_flight_loop` runs, read from the
 * PROC's 0x34, with the same A10-across-the-call and the same pair of tokens
 * six apart. Two loops share the mechanism and differ in the physics.
 *
 * On the first entry 0x20 seeds the velocity, and it is skipped when zero --
 * so a zero there means "keep whatever velocity is already set" rather than
 * "start from rest". `t_flight_call` uses 0xd for that idea in a different
 * slot; this one uses 0.
 *
 * The PROC's 0x44 is cleared on landing -- the hit count `back_to_normal_px`
 * reads to decide whether a combo happened. So landing ends a combo.
 */
long t_gravity_ani(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    uint32_t argc;

    if (token != 0 && token != 0x1684 && token != 0x168a)
        return -3;

    if (token == 0) {
        *(uint32_t *)((char *)obj->field00 + 0x34) = obj->field34;

        if (obj->field20 != 0)                  /* 0: keep what is set */
            obj->field08->field1c = obj->field20;

        init_anirate(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x1684;
        thread->fieldfc = 1;
        return 1;
    }

    if (token == 0x1684) {
        obj->field34 = *(const uint32_t *)((char *)obj->field00 + 0x34);

        if (obj->field34 != 0) {                /* the optional callback */
            argc = thread->fieldf8;
            *mk3_arg(thread, argc) = obj->a10;
            thread->fieldf8 = argc + 1;

            *mk3_frame(thread, thread->frame + 1) = 0x168a;
            thread->frame = thread->frame + 1;          /* push a level */
            mk3_frame(thread, thread->frame)[1] = obj->field34;
            *mk3_frame(thread, thread->frame + 1) = 0;
            return 0;
        }
    } else {                                    /* 0x168a: it just finished */
        argc = thread->fieldf8 - 1;
        thread->fieldf8 = argc;
        obj->a10 = *mk3_arg(thread, argc);
    }

    next_anirate(obj);

    obj->field1c = obj->field08->field1c + obj->a10;     /* v += g */
    obj->field08->field1c = obj->field1c;

    if ((int32_t)obj->field1c < 0 ||            /* still rising */
        (int32_t)(int16_t)MK3_FIELD12(obj->field08)
            < (int32_t)obj->field00->field40) { /* still above the floor */
        *mk3_frame(thread, thread->frame + 1) = 0x1684;
        thread->fieldfc = 1;
        return 1;
    }

    stop_me_player(obj);
    ground_player(obj);
    obj->field1c = 0;
    obj->field00->p_hit = 0;                    /* landing ends the combo */
    return mk3_unwind(thread);
}


/* --------------------------------------------------------------- bar_reducer
 *
 * armv7 0x00059154, three hundred bytes.  **Complete**, and it is where damage
 * actually lands.
 *
 *      if (G[0x368] == 0 || G[0x36c] == 0) { obj->field5c = 1; return; }
 *      dmg = obj->a10
 *      if (dmg == 0x28) dmg = MotaroPunchDamage()
 *      i = obj->field34
 *      if ((Pp[i].field10 & 1) && !(Pp[1 - i].field10 & 1))
 *          dmg = adjust_damage(dmg)
 *      obj->field38 = &G[0x368 + i*4]
 *      obj->field24 = GrObj[i].field24
 *      f = GrObj[i].field30
 *      if ((f & 0x1000) || (f & 0x800)) {
 *          dmg >>= 2
 *          if (dmg <= 0) dmg = 1
 *      }
 *      h = *obj->field38 - dmg
 *      if (h < 0) h = 0
 *      *obj->field38 = h
 *      MKEvent_Add(3, 0, h, i)
 *      if (h <= 0) { obj->field5c = 1; return; }
 *      if (h <= 6) {
 *          obj->field30 = i + 1
 *          if (((int16_t)G[0x454] & (i + 1)) == 0) {
 *              G[0x454] = i + 1
 *              obj->field1c = 0x27
 *              create_fx_param(obj, i)
 *          }
 *      }
 *      obj->field5c = 0
 *
 * The health bar is G+0x368 indexed by the fighter, exactly as `recharge_bars`
 * fills it and `t_clock4` tests it, and the event is the same (3, 0) with the
 * new value and the player.
 *
 * **A damage of 40 is a sentinel.** It is replaced by whatever
 * `MotaroPunchDamage` returns -- a function in another translation unit --
 * rather than being used. So one attack's damage is computed elsewhere and 40
 * is how this routine is told to go and ask.
 *
 * The difficulty and ladder scaling is applied only when **this** entry has
 * bit 0 of Pp's 0x10 and the other does not. The same bit gates the endurance
 * queue and the end of the match; what it means is still not stated anywhere,
 * but the asymmetry here says the two fighters are not alike and only one
 * side's damage is scaled.
 *
 * The two flag bits `back_to_normal_px` clears as HALF_DAMAGE and
 * QUARTER_DAMAGE **both branch to the same `asrs #2`**. One behaviour, two
 * names, and the names come from the symbol table. Written down as observed
 * rather than reconciled: either the half case is unimplemented or the naming
 * is off, and nothing here says which.
 *
 * The floor of 1 is applied to the reduced damage, so a quartered hit still
 * takes a point -- the same floor `adjust_damage` puts on its own result.
 *
 * At six health or less the routine fires effect 0x27 once per player per
 * round, remembering it as a bit in the signed halfword at G+0x454. The bit is
 * `i + 1`, so player 0 uses bit 0 and player 1 uses bit 1 -- and the store is a
 * plain assignment, not an OR, so setting one clears the other.
 */
long MotaroPunchDamage(void);
extern int PortPractice;                /* FrontEnd.c, port: training */

void bar_reducer(MK3OBJ *obj)
{
    uint32_t i;
    int32_t dmg, h;
    uint32_t f;

    if (*(const uint32_t *)(G_BYTES + 0x368) == 0 ||
        *(const uint32_t *)(G_BYTES + 0x36c) == 0) {
        obj->field5c = 1;                       /* somebody is already down */
        return;
    }

    dmg = (int32_t)obj->a10;
    if (dmg == 0x28) {                          /* ask somebody else */
        dmg = MotaroPunchDamage();
        obj->a10 = (uint32_t)dmg;
    }

    i = obj->field34;

    if ((*(const uint32_t *)(Pp + i * PP_STRIDE + 0x10) & 1) != 0 &&
        (*(const uint32_t *)(Pp + (1 - i) * PP_STRIDE + 0x10) & 1) == 0) {
        dmg = adjust_damage(dmg);
        obj->a10 = (uint32_t)dmg;
    }

    obj->field38 = (uint32_t)(uintptr_t)(G_BYTES + 0x368 + i * 4);
    obj->field20 = (uint32_t)(uintptr_t)(GrObj + i * GROBJ_STRIDE);
    obj->field24 = *(const uint32_t *)(GrObj + i * GROBJ_STRIDE + 0x24);

    f = *(const uint32_t *)(GrObj + i * GROBJ_STRIDE + 0x30);
    obj->field20 = f;

    if ((f & MK3F_QUARTER_DAMAGE) != 0 || (f & MK3F_HALF_DAMAGE) != 0) {
        dmg = dmg >> 2;                         /* both bits, one shift */
        if (dmg <= 0)
            dmg = 1;
        obj->a10 = (uint32_t)dmg;
    }

    h = (int32_t)*(const uint32_t *)(uintptr_t)obj->field38 - dmg;
    if (h < 0)
        h = 0;
    if (PortPractice && h <= 0)
        h = 0xa6;                       /* port: training, full again */
    if (PortDbgHealth & (1 << i))
        h = 0xa6;                       /* port: debug, never takes any */
    obj->field20 = (uint32_t)h;
    *(uint32_t *)(uintptr_t)obj->field38 = (uint32_t)h;

    MKEvent_Add(3, 0, (long)*(const uint32_t *)(G_BYTES + 0x368 + i * 4),
                (uint32_t)i);

    if (h <= 0) {
        obj->field5c = 1;                       /* that was the last of it */
        return;
    }

    if (h <= 6) {                               /* the low-health warning */
        uint32_t bit = i + 1;

        obj->field30 = bit;
        /* `ands r3, r2` is stored to 0x20 before the test (difftest) */
        obj->field20 = (uint32_t)(int32_t)*(const int16_t *)(G_BYTES + 0x454) & bit;
        if (obj->field20 == 0) {
            obj->field20 = bit;
            *(int16_t *)(G_BYTES + 0x454) = (int16_t)bit;
            obj->field1c = 0x27;
            create_fx_param(obj, obj->field34);
        }
    }

    obj->field5c = 0;
}


/* -------------------------------------------------------------- t_one_on_one
 *
 * armv7 0x00056b28, three hundred bytes.  **Complete**, and it is the match
 * loop.
 *
 *      token 0:
 *          frame[frame+1].w0 = 0x129b
 *          push t_play_1_round
 *      token 0x129b or 0x12b1:
 *          frame[frame+1].w0 = 0x12a1
 *          push t_is_endurance_possible
 *      token 0x12a1:
 *          if (obj->field5c == 0) {
 *              frame[frame].handler = t_play3          ; the loop ends
 *              frame[frame+1].w0 = 0
 *          } else if ((Pp[0].field10 & 0x200) != 0
 *                     && GrObj[0].field24 != 0xc) {
 *              G[0x3fc] = (uint16_t)0                  ; order the morph
 *              park(0x12ac, 1)
 *          } else {
 *              frame[frame+1].w0 = 0x12b1
 *              push t_spawn_endurance_guy
 *          }
 *      token 0x12ac:
 *          if (GrObj[0].field24 != 0xc) park(0x12ac, 1)
 *          else { frame[frame+1].w0 = 0x12b1; push t_spawn_endurance_guy; }
 *      otherwise:  return -3
 *
 * Play a round, ask whether endurance continues, spawn the next opponent, ask
 * again -- and 0x12b1 rejoins at the same place as 0x129b, which is what makes
 * it a loop rather than a sequence. When `t_is_endurance_possible` says no, the
 * routine becomes `t_play3` at the current level instead.
 *
 * **The wait is the interesting part.** If the fighter is not currently
 * character 0xc, this zeroes the halfword at G+0x3fc and then polls until it
 * IS. That address is exactly the table `t_back_to_shang_check` looks up
 * through `get_tsl_px`, and `get_tsl_px` treats a zero entry as a hole and
 * answers 0x780 -- comfortably above the 0x200 that check demands. So writing
 * the zero is how this routine **orders** the morph back to Shang, and the
 * poll is it waiting for the morph to finish before the next opponent walks
 * on.
 *
 * Three functions had to be read to see that: the writer here, the reader in
 * `t_back_to_shang_check`, and the lookup in `get_tsl_px`. None of them says
 * it alone.
 *
 * Bit 0x200 of Pp's 0x10 gates the whole morph branch -- a second bit of that
 * word, after the 1 that gates endurance and the end of the match.
 *
 * The dispatch is a binary search: equal, less-than, then two more equals, with
 * 0x129b formed as 0x12a1 - 6 and 0x12b1 as 0x12ac + 5. That is why the
 * disassembly reaches its constants by arithmetic.
 */
long t_one_on_one(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token != 0 && token != 0x129b && token != 0x12a1 &&
        token != 0x12ac && token != 0x12b1)
        return -3;

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x129b;
        thread->frame = thread->frame + 1;              /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_play_1_round;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x129b || token == 0x12b1) {           /* round the loop */
        *mk3_frame(thread, thread->frame + 1) = 0x12a1;
        thread->frame = thread->frame + 1;              /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_is_endurance_possible;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x12a1) {
        if (obj->field5c == 0) {                        /* no more opponents */
            mk3_frame(thread, thread->frame)[1] =
                (uint32_t)(uintptr_t)t_play3;
            *mk3_frame(thread, thread->frame + 1) = 0;
            return 0;
        }

        if ((*(const uint32_t *)(Pp + 0x10) & 0x200) != 0 &&
            *(const uint32_t *)(GrObj + 0x24) != MK3_CHAR_SHANG) {
            /* Zeroing that entry makes t_back_to_shang_check's lookup pass.
             * Only here: the polling arm below does not repeat it. */
            *(uint16_t *)(G_BYTES + 0x3fc) = 0;
            *mk3_frame(thread, thread->frame + 1) = 0x12ac;
            thread->fieldfc = 1;
            return 1;
        }
    } else if (*(const uint32_t *)(GrObj + 0x24) != MK3_CHAR_SHANG) {
        *mk3_frame(thread, thread->frame + 1) = 0x12ac; /* still waiting */
        thread->fieldfc = 1;
        return 1;
    }

    *mk3_frame(thread, thread->frame + 1) = 0x12b1;
    thread->frame = thread->frame + 1;                  /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_spawn_endurance_guy;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------------- QueueAndJump
 *
 * armv7 0x000572b8, three hundred and four bytes.  **Complete**, and it is
 * where `UnstackSwitches` lands.
 *
 * A `_swtab` entry is four words: `[0]` the switch id, `[1]` an argument,
 * `[2]` the player, `[3]` a table for `DoSwitchJump`.
 *
 *      r = SwitchQueue(e[0], e[1])
 *      if (e[3] != 0) {
 *          h = Plyr[who].field60
 *          if (h != NULL && h[id] != NULL
 *              && (int16_t)Pp[who].field7e == 0) {
 *              mytc[who].frame   = 0
 *              mytc[who].func    = h[id]
 *              mytc[who].field08 = 0
 *          }
 *          last_switch_ram[id * 2    ][who] = r    ; halfwords, presses only
 *          last_switch_ram[id * 2 + 1][who] = r
 *      }
 *      if ((int16_t)Plyr[who].field00->field7c != 0) {
 *          if (id == 0)      DoSwitchJump(e[3], who, 1)
 *          else if (id == 3) DoSwitchJump(e[3], who, 4)
 *          f = my_func(&Plyr[who])
 *          if (f == t_do_shake || f == t_do_fatality_1 || f == t_do_fatality_2)
 *              return
 *      }
 *      DoSwitchJump(e[3], who, id)
 *
 * **Three routines are not to be interrupted.** The function currently
 * installed on the player's thread is compared against `t_do_shake`,
 * `t_do_fatality_1` and `t_do_fatality_2` -- all three named in the symbol
 * table, the fatalities in `mkfatal.c` and the shake in `moves.c` -- and if it
 * is any of them the switch is queued and recorded but never dispatched. A
 * button pressed during a fatality goes nowhere.
 *
 * That comparison is by ADDRESS, through three pointer slots, which is exactly
 * why the pop family has four identical bodies at four addresses: identity is
 * how this engine asks what a thread is doing.
 *
 * **A third four-button hook.** With the gate at 0x7c set, switches 0 and 3
 * fire an extra `DoSwitchJump` with ids 1 and 4 before the ordinary one --
 * after `four_button_bits` folding the bits and `get_tsl_px` swapping the
 * tables, this dispatches twice. All three read the same halfword.
 *
 * The guard test is only reached under that gate, so on the four-button scheme
 * a fatality is protected and on the six-button one it is not. Transcribed as
 * written; nothing here says whether that is intended.
 *
 * `_last_switch_ram` is a table of POINTERS, indexed twice: entry `id*2` and
 * entry `id*2+1`, each a halfword array indexed by player. The same value goes
 * to both, so the pair is a current-and-previous kept side by side.
 *
 * The per-switch handler at `Plyr[who].field60` is installed straight into
 * `mytc[who]` -- the thread array `my_func` indexes with a stride of 268 -- and
 * only when `Pp[who].field7e` is zero. That is a fifth place a handler comes
 * out of data.
 */
extern uint16_t *P32 last_switch_ram[];     /* 0x0016f50c */
long t_do_shake(MK3THREAD *thread);
long t_do_fatality_1(MK3THREAD *thread);
long t_do_fatality_2(MK3THREAD *thread);

void QueueAndJump(const uint32_t *e, uint32_t unused)
{
    uint32_t id  = e[0];
    uint32_t who = e[2];
    char *plyr   = Plyr + who * PLYR_STRIDE;
    uint16_t r;

    (void)unused;

    /* The low half of what SwitchQueue builds: the counter at G+0xa8. */
    r = (uint16_t)SwitchQueue((uint16_t)id, (SWITCHQUEUE *)(uintptr_t)e[1]);

    if (e[3] != 0) {
        MK3THREADFUNC *h = *(MK3THREADFUNC *P32 *)(plyr + 0x60);


        if (h != NULL && h[id] != NULL &&
            *(const int16_t *)(Pp + who * PP_STRIDE + 0x7e) == 0) {
            MK3THREAD *th =
                (MK3THREAD *)((char *)mytc + who * MK3THREAD_STRIDE);

            th->frame   = 0;
            th->func    = h[id];
            th->field08 = 0;
        }

        /* Inside the e[3] test: 0x572d4 `cbz` on e[3] goes to 0x57394,
         * past these two stores (0x57322, 0x57326). A release entry has
         * e[3] == 0, so only a press is timestamped. Written outside it,
         * the release overwrote its own press and every hold-and-release
         * move (Mileena's sai, Liu Kang's bicycle kick) read "pressed 0
         * ticks ago" and never fired. */
        last_switch_ram[id * 2][who]     = r;
        last_switch_ram[id * 2 + 1][who] = r;
    }

    if (*(const int16_t *)((char *)(*(MK3OBJPROC *P32 *)plyr) + 0x7c) != 0) {
        if (id == 0)                            /* the four-button scheme */
            DoSwitchJump((long)e[3], who, 1);
        else if (id == 3)
            DoSwitchJump((long)e[3], who, 4);

        {
            const void *f = my_func((MK3OBJ *)plyr);

            if (f == (const void *)t_do_shake ||
                f == (const void *)t_do_fatality_1 ||
                f == (const void *)t_do_fatality_2)
                return;                         /* do not interrupt these */
        }
    }

    DoSwitchJump((long)e[3], who, id);
}


/* ------------------------------- NewThread, getprc_z and getprc_x
 *
 * armv7 0x00058a10, 0x00059ae8 and 0x000599b4 -- two hundred and ninety-six,
 * two hundred and sixty-four and three hundred and eight bytes.
 * **All three complete**, and all three are one body.
 *
 *      th = the caller's, or TList_Get()
 *      if (th == NULL) return NULL
 *      i = th->player
 *      th->fieldf8 = th->frame = th->fieldfc = th->field08 = 0
 *      th->func = <the handler>
 *      th->proc = Plyr + i * 108
 *      memcpy(Pp    + i * 140, owner->field00, 140)
 *      memcpy(GrObj + i *  76, owner->field08,  76)
 *      GrObj[i].field44 = 0
 *      [owner->field1c = Pp + i * 140]
 *      Plyr[i].field3c = owner->field3c
 *      Plyr[i].field40 = owner->field40
 *      Plyr[i].field44 = owner->a10
 *      Plyr[i].field48 = owner->field48
 *      [Plyr[i].field08->field2c = -1]
 *      return <the thread, or &Plyr[i]>
 *
 * A fighter cloned into a spare thread. The three differ in four things and
 * nothing else:
 *
 *      NewThread  handler from an argument   returns the thread
 *                 tells the owner            clears the animation
 *      getprc_z   handler from owner->0x38   returns &Plyr[i]
 *      getprc_x   handler from owner->0x38   returns &Plyr[i]
 *                 tells the owner            takes a thread, or gets one
 *
 * and `t_play_1_round` writes the first six stores inline rather than calling
 * any of them. Four readings of one routine from four addresses, agreeing
 * field for field -- which is what makes each a check on the others rather
 * than a repetition.
 *
 * **The two memcpy lengths are the two strides**, 140 and 76. Every earlier
 * derivation -- the modular inverse in `StartGrObjAt`, the shift-and-add in
 * `getobjectinsert`, the two offsets in `is_finish_him_allowed`, the three
 * multiplies in `Endurance_ClearPlayer` and in `DoSwitchJump` -- gave the
 * distance from one entry to the next. These give the number of bytes actually
 * copied, which is the first evidence that the entries are that SIZE and not
 * merely that far apart.
 *
 * "Tells the owner" is `owner->field1c = &Pp[i]`, a second return value handed
 * back through the argument. `getprc_x` also clears that slot on entry, so a
 * caller that fails to get a thread finds a zero there rather than the last
 * clone's address.
 *
 * The handler out of `owner->field38` is the slot `fastxfer_thread` takes a
 * thread function from and `react_xfer_him` writes a reaction into -- "the
 * routine this object is about to become".
 *
 * The index is recomputed from `th->player` for every store, eight or nine
 * times over, so each disassembly repeats the same five instructions again and
 * again. Written once here.
 */
#define MK3_CLONE_TELL_OWNER   1u
#define MK3_CLONE_CLEAR_ANI    2u

static MK3THREAD *mk3_clone(MK3OBJ *owner, MK3THREAD *th,
                            MK3THREADFUNC func, uint32_t how)
{
    char *plyr;
    uint32_t i;

    if (th == NULL)
        th = TList_Get();
    if (th == NULL)
        return NULL;

    i = th->player;

    th->fieldf8 = 0;
    th->frame   = 0;
    th->func    = func;
    th->fieldfc = 0;
    th->field08 = 0;

    plyr = Plyr + i * PLYR_STRIDE;
    th->proc = plyr;

    memcpy(Pp    + i * PP_STRIDE,    owner->field00, PP_STRIDE);
    memcpy(GrObj + i * GROBJ_STRIDE, owner->field08, GROBJ_STRIDE);

    *(uint32_t *)(GrObj + i * GROBJ_STRIDE + 0x44) = 0;

    if (how & MK3_CLONE_TELL_OWNER)             /* the second return value */
        owner->field1c = (uint32_t)(uintptr_t)(Pp + i * PP_STRIDE);

    *(uint32_t *)(plyr + 0x3c) = owner->field3c;
    *(uint32_t *)(plyr + 0x40) = owner->field40;
    *(uint32_t *)(plyr + 0x44) = owner->a10;
    *(uint32_t *)(plyr + 0x48) = owner->field48;

    if (how & MK3_CLONE_CLEAR_ANI)
        (*(MK3OBJ *P32 *)(plyr + 8))->field2c = 0xffffffffu;    /* nothing yet */

    return th;
}

MK3THREAD *NewThread(void *owner_p, MK3THREADFUNC func)
{
    return mk3_clone((MK3OBJ *)owner_p, NULL, func,
                     MK3_CLONE_TELL_OWNER | MK3_CLONE_CLEAR_ANI);
}

MK3OBJ *getprc_z(MK3OBJ *obj)
{
    MK3THREAD *th = mk3_clone(obj, NULL,
                              (MK3THREADFUNC)(uintptr_t)obj->field38, 0);

    return th ? (MK3OBJ *)th->proc : NULL;
}

MK3OBJ *getprc_x(MK3OBJ *obj, MK3THREAD *th)
{
    obj->field1c = 0;                           /* cleared before the attempt */
    th = mk3_clone(obj, th, (MK3THREADFUNC)(uintptr_t)obj->field38,
                   MK3_CLONE_TELL_OWNER);

    return th ? (MK3OBJ *)th->proc : NULL;
}


/* --------------------------------------------------- t_finish_him_sequence
 *
 * armv7 0x00058f58, three hundred and sixteen bytes.  **Complete.**
 *
 *      token 0:
 *          is_finish_him_allowed(obj)
 *          if (obj->field5c == 0) { install t_finish_him_exit; return 0; }
 *          obj->field1c = 3
 *          G[0x45c] = (int16_t)3
 *          finish_him_or_her(obj)
 *          obj->field20 = 0x28
 *          frame[frame+1].w0 = 0x1163
 *          frame = frame + 1                       ; push
 *          frame[frame].handler = t_fatality_wait
 *      token 0x1163:
 *          v = (uint16_t)G[0x452]
 *          obj->field1c = (int16_t)v
 *          if (v != 0) { install t_finish_him_exit; return 0; }
 *          obj->field48 = 0x120
 *          frame[frame+1].w0 = 0x1170
 *          frame = frame + 1                       ; push
 *          frame[frame].handler = t_fatality_wait
 *      token 0x1170:
 *          if (obj->field48 != 0) {
 *              obj->field48 = 0x140
 *              install t_fhs3
 *          } else install t_finish_him_exit
 *      otherwise:  return -3
 *
 * Two waits with a check between them. `t_fatality_wait` counts 0x48 down and
 * ends early when one of two halfwords in G is set, so **a non-zero 0x48 on
 * return means it stopped early and a zero means it ran out** -- the countdown
 * doubles as the verdict and no separate flag is needed. That is why the third
 * arm can decide with one comparison.
 *
 * G+0x45c is the winner halfword: `t_player_1_won` writes 1 and
 * `t_player_2_won` writes 2. Writing **3** here says neither, for as long as
 * the finisher lasts, which is a fifth value for a field that had two and a
 * gap.
 *
 * The refusal in the middle reads G+0x452 directly rather than trusting the
 * wait's answer -- "no fatality" arriving between the two windows ends the
 * sequence at once.
 *
 * Three of the four exits install `t_finish_him_exit` at the current level,
 * from three different places with three different registers holding the zero.
 * One body, three constants.
 */
long t_finish_him_sequence(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    MK3THREADFUNC next;

    if (token != 0 && token != 0x1163 && token != 0x1170)
        return -3;

    if (token == 0) {
        is_finish_him_allowed(obj);
        if (obj->field5c == 0) {
            next = (MK3THREADFUNC)t_finish_him_exit;
        } else {
            obj->field1c = 3;
            *(int16_t *)(G_BYTES + 0x45c) = 3;      /* nobody yet */
            finish_him_or_her(obj);
            obj->field20 = 0x28;

            *mk3_frame(thread, thread->frame + 1) = 0x1163;
            thread->frame = thread->frame + 1;      /* push a level */
            mk3_frame(thread, thread->frame)[1] =
                (uint32_t)(uintptr_t)t_fatality_wait;
            *mk3_frame(thread, thread->frame + 1) = 0;
            return 0;
        }
    } else if (token == 0x1163) {
        uint16_t v = *(const uint16_t *)(G_BYTES + 0x452);

        obj->field1c = (uint32_t)(int32_t)(int16_t)v;
        if (v != 0) {
            next = (MK3THREADFUNC)t_finish_him_exit;    /* refused */
        } else {
            obj->field48 = 0x120;

            *mk3_frame(thread, thread->frame + 1) = 0x1170;
            thread->frame = thread->frame + 1;      /* push a level */
            mk3_frame(thread, thread->frame)[1] =
                (uint32_t)(uintptr_t)t_fatality_wait;
            *mk3_frame(thread, thread->frame + 1) = 0;
            return 0;
        }
    } else {                                        /* 0x1170 */
        if (obj->field48 != 0) {                    /* it ended early */
            obj->field48 = 0x140;
            next = (MK3THREADFUNC)t_fhs3;
        } else {
            next = (MK3THREADFUNC)t_finish_him_exit;
        }
    }

    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)next;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ---------------------------------------------------------------- t_fx_mercy
 *
 * armv7 0x000597f8, three hundred and forty-four bytes.  **Complete**, and the
 * longest coroutine in this file: five resume points.
 *
 *      token 0:
 *          NewThread(obj, t_make_db_tone)
 *          MKEvent_Add(3, 0xf, 0, 0)
 *          park(0xfde, 0x30)
 *      token 0xfde:
 *          tsound_func(obj, 0x94)
 *          obj->field08->field2c = 0x104
 *          center_obj_x(obj)
 *          set_noscroll(obj)
 *          obj->field40 = a_mercy
 *          obj->field1c = 4
 *          push t_mframew with token 0xff0
 *      token 0xff0:  obj->field1c = 4; push t_mframew with token 0xff3
 *      token 0xff3:  obj->field1c = 4; push t_mframew with token 0xff6
 *      token 0xff6:
 *          obj->field1c = obj->a10 + 0x99
 *          tsound_func(obj, obj->field1c)
 *          park(0xffd, 8)
 *      token 0xffd:  park(0x1004, 0x16462)         ; and never wake
 *      otherwise:    return -3
 *
 * Mercy from the inside: start a tone thread, tell the front end, wait, make
 * the noise, plant the object and run `_a_mercy` three times at rate 4, then a
 * spoken line, then stop.
 *
 * **The sound is `A10 + 0x99`.** A base plus whatever the caller left in A10,
 * so the line is per character and 0x99 is where that block of recordings
 * starts.
 *
 * The three animation runs are three separate pushes with three tokens, not a
 * loop -- and the middle one takes its next token from a register the dispatch
 * left behind, which is why the disassembly stores `r1` without loading it.
 *
 * The last park is 0x16462 on token 0x1004, which this function does not
 * accept. That is the fourth routine here to end that way, after
 * `t_fx_babality`, `t_friendship_speech` and `t_round_intro_fx`: wait for a
 * wake-up that cannot come and be killed in the meantime.
 *
 * `t_make_db_tone` comes through a pointer slot rather than as a link-time
 * constant, and it lives in another translation unit.
 *
 * `_a_mercy` at 0x0016f6a8 is the thirteenth named table this file reaches and
 * the fourth of the finisher tables, after `_a_ship`, `_a_friend` and
 * `_a_animality`.
 */
extern uint32_t a_mercy[];              /* 0x0016f6a8 */
long t_make_db_tone(MK3THREAD *thread);  /* pointer slot 0x000f3200 -> 0x00033c31 */

long t_fx_mercy(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    uint32_t next_token;

    if (token == 0) {
        NewThread(obj, (MK3THREADFUNC)t_make_db_tone);
        MKEvent_Add(3, 0xf, 0, 0);
        *mk3_frame(thread, thread->frame + 1) = 0xfde;
        thread->fieldfc = 0x30;
        return 0x30;
    }

    if (token == 0xff6) {
        obj->field1c = obj->a10 + 0x99;         /* the per-character line */
        tsound_func(obj, obj->field1c);
        *mk3_frame(thread, thread->frame + 1) = 0xffd;
        thread->fieldfc = 8;
        return 8;
    }

    if (token == 0xffd) {
        *mk3_frame(thread, thread->frame + 1) = 0x1004;
        thread->fieldfc = 0x16462;              /* nothing accepts 0x1004 */
        return 0x16462;
    }

    if (token == 0xfde) {
        tsound_func(obj, 0x94);
        obj->field08->field2c = 0x104;
        center_obj_x(obj);
        set_noscroll(obj);
        obj->field40 = (uint32_t)(uintptr_t)a_mercy;
        next_token = 0xff0;
    } else if (token == 0xff0) {
        next_token = 0xff3;
    } else if (token == 0xff3) {
        next_token = 0xff6;
    } else {
        return -3;
    }

    obj->field1c = 4;
    *mk3_frame(thread, thread->frame + 1) = next_token;
    thread->frame = thread->frame + 1;                  /* push a level */
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------------------ t_play3
 *
 * armv7 0x00057c34, three hundred and fifty-six bytes.  **Complete**, and it
 * is the end of a round.
 *
 *      token 0:
 *          frame[frame+1].w0 = 0x12bc
 *          push t_results_of_round
 *      token 0x12bc:
 *          obj->field1c = H[0]
 *          obj->field20 = H[1]
 *          if (H[0] == 2 || H[1] == 2) {
 *              a = (int16_t)G[0x45e]
 *              b = (int16_t)G[0x460]
 *              obj->field1c = a
 *              obj->field20 = a + b
 *              if (a + b != 0) {
 *                  frame[frame+1].w0 = 0x12ca
 *                  push t_finish_him_sequence
 *              }
 *          }
 *          fall to the tail
 *      token 0x12ca:
 *          set_winner_status(obj)
 *          fall to the tail
 *      tail:
 *          obj->field1c = 1
 *          G[0x452] = (int16_t)1
 *          frame[frame+1].w0 = 0x12d0
 *          push t_bonus_count
 *      token 0x12d0:
 *          MKEvent_Add(3, 4, 0, 0)
 *          obj->field1c = H[0]; obj->field20 = H[1]
 *          if (H[0] == 2 || H[1] == 2) end_of_match_chores(obj)
 *          frame[frame].handler = t_round_loop
 *      otherwise:  return -3
 *
 * **Two conditions gate the finisher and both are here.** Somebody must have
 * two round wins -- `H[0]` and `H[1]` are the tallies the winner routines
 * increment, and 2 is the same number `t_results_retp` ends the match on -- and
 * the clock must not have run out, tested as the sum of its two digits. So
 * "Finish him" is offered on the last round only and only with time left,
 * which is what `is_finish_him_allowed` was checking from the other side.
 *
 * Adding the two clock digits to test for zero is the same trick `t_clock3`
 * uses, and it works for the same reason: neither digit is ever negative.
 *
 * The tail writes 1 into G+0x452 whatever happened, so by the time the bonus
 * count runs no fatality is pending. `t_fatality_wait` reads that halfword as
 * "no".
 *
 * `t_bonus_count` comes through a pointer slot rather than as a link-time
 * constant, like `t_make_db_tone` in `t_fx_mercy`.
 *
 * Both exits install `t_round_loop`; the match-over one runs
 * `end_of_match_chores` first. So the loop is left the same way whether or not
 * the match ended, and the difference is one call.
 */
long t_bonus_count(MK3THREAD *thread);   /* pointer slot 0x000f3214 -> 0x0007ccb9 */

long t_play3(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    const uint32_t *h = (const uint32_t *)H;

    if (token != 0 && token != 0x12bc && token != 0x12ca && token != 0x12d0)
        return -3;

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x12bc;
        thread->frame = thread->frame + 1;              /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_results_of_round;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x12d0) {
        MKEvent_Add(3, 4, 0, 0);
        obj->field1c = h[0];
        obj->field20 = h[1];
        if (h[0] == 2 || h[1] == 2)
            end_of_match_chores(obj);

        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_round_loop;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x12bc) {
        obj->field1c = h[0];
        obj->field20 = h[1];

        if (h[0] == 2 || h[1] == 2) {           /* the match is over */
            int32_t a = *(const int16_t *)(G_BYTES + 0x45e);
            int32_t b = *(const int16_t *)(G_BYTES + 0x460);

            obj->field1c = (uint32_t)a;
            obj->field20 = (uint32_t)(a + b);

            if (a + b != 0) {                   /* and there is time left */
                *mk3_frame(thread, thread->frame + 1) = 0x12ca;
                thread->frame = thread->frame + 1;      /* push a level */
                mk3_frame(thread, thread->frame)[1] =
                    (uint32_t)(uintptr_t)t_finish_him_sequence;
                *mk3_frame(thread, thread->frame + 1) = 0;
                return 0;
            }
        }
    } else {                                    /* 0x12ca */
        set_winner_status(obj);
    }

    obj->field1c = 1;
    *(int16_t *)(G_BYTES + 0x452) = 1;          /* no fatality pending */

    *mk3_frame(thread, thread->frame + 1) = 0x12d0;
    thread->frame = thread->frame + 1;                  /* push a level */
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_bonus_count;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------------ t_game_finished
 *
 * armv7 0x00058d84, three hundred and sixty bytes.  **Complete.**
 *
 *      token 0:
 *          for (n = 1; n <= 30; n++) {
 *              if (n - 1 <= 1) {
 *                  StartProcAt(&Plyr[n-1], t_wait_forever)
 *                  GrObj[n-1].field20 = 0
 *                  GrObj[n-1].field1c = 0
 *                  GrObj[n-1].field18 = 0
 *              } else if (n - 1 != 2) {
 *                  KillProc(&Plyr[n-1])
 *              }
 *          }
 *          G[0x210] = 0
 *          G[0x0b8] = 0
 *          obj->field1c = 0x42;  create_fx(obj)
 *          obj->a10 = 10
 *          obj->field1c = 0x43;  create_fx(obj)
 *          park(0x11aa, 6)
 *      token 0x11aa:
 *          obj->field1c = 0x44;  create_fx(obj)
 *          park(0x11ad, 6)
 *      token 0x11ad:
 *          if (--obj->a10 != 0) {
 *              obj->field1c = 0x43;  create_fx(obj)
 *              park(0x11aa, 6)
 *          } else {
 *              GrObj[0].field2c = -1
 *              GrObj[0].field78 = -1
 *              obj->field1c = 0x41;  create_fx(obj)
 *              park(0x11b8, 0x1c7)
 *          }
 *      token 0x11b8:  unwind
 *      otherwise:     return -3
 *
 * **The arrays are thirty entries long.** The sweep walks `Plyr` by 108 and
 * `GrObj` by 76 in step, thirty times, which is the first statement of how many
 * there are -- every other routine indexes them by a player number and never
 * says where they stop.
 *
 * Three kinds of entry:
 *
 *   - 0 and 1, the two fighters: parked on `t_wait_forever` rather than
 *     killed, and three of their GrObj words cleared. They stay alive because
 *     the ending still has to draw them.
 *   - 2: skipped entirely, neither started nor killed. What lives there is not
 *     said here.
 *   - 3 to 29: killed.
 *
 * The two zeroes into G are 0xb8 and 0x210, which are the same field one
 * `G_FIGHTER_STRIDE` apart -- the x velocity of each fighter. Everything stops
 * moving.
 *
 * Then ten alternations of effects 0x43 and 0x44 six ticks apart, and a final
 * 0x41 held for 455. The count lives in A10 and the alternation is two resume
 * points, so the loop is the coroutine rather than anything counted inside one
 * entry.
 *
 * The -1 pair before the last effect goes into GrObj[0] at 0x2c and 0x78 --
 * the animation, and a second slot seventy-six bytes into the same entry, which
 * is its last word.
 *
 * The 0x41 is formed as -1 + 0x42, reusing the register the two stores just
 * used, which is why the disassembly makes it look derived.
 */

long t_game_finished(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x11aa) {
        obj->field1c = 0x44;
        create_fx(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x11ad;
        thread->fieldfc = 6;
        return 6;
    }

    if (token == 0x11ad) {
        obj->a10 = obj->a10 - 1;
        if (obj->a10 != 0)
            goto blink;

        *(uint32_t *)(GrObj + 0x2c) = 0xffffffffu;
        *(uint32_t *)(GrObj + 0x78) = 0xffffffffu;
        obj->field1c = 0x41;
        create_fx(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x11b8;
        thread->fieldfc = 0x1c7;
        return 0x1c7;
    }

    if (token == 0x11b8)
        return mk3_unwind(thread);

    if (token != 0)
        return -3;

    {
        char *plyr = Plyr;
        char *grobj = GrObj;
        uint32_t n;

        for (n = 1; n <= 30; n++) {
            if (n - 1 <= 1) {                   /* the two fighters */
                StartProcAt((MK3OBJ *)plyr, (MK3THREADFUNC)t_wait_forever);
                *(uint32_t *)(grobj + 0x20) = 0;
                *(uint32_t *)(grobj + 0x1c) = 0;
                *(uint32_t *)(grobj + 0x18) = 0;
            } else if (n - 1 != 2) {            /* entry 2 is spared */
                KillProc((MK3OBJ *)plyr);
            }
            plyr  += PLYR_STRIDE;
            grobj += GROBJ_STRIDE;
        }
    }

    *(uint32_t *)(G_BYTES + 0x210) = 0;         /* both x velocities */
    *(uint32_t *)(G_BYTES + 0xb8) = 0;

    obj->field1c = 0x42;
    create_fx(obj);

    obj->a10 = 10;                              /* ten alternations */

blink:
    obj->field1c = 0x43;
    create_fx(obj);

    *mk3_frame(thread, thread->frame + 1) = 0x11aa;
    thread->fieldfc = 6;
    return 6;
}


/* -------------------------------------------------------- t_master_proc_mercy
 *
 * armv7 0x00058bf0, three hundred and eighty-four bytes.  **Complete.**
 *
 *      token 0:
 *          obj->field1c = 0
 *          G[0x44e] = (uint16_t)0
 *          G[0x452] = (uint16_t)obj->field1c
 *          obj->a10 = (G[0x368] != 0) ? GrObj[0].field24 : GrObj[1].field24
 *          obj->field24 = G[0x368]
 *          obj->field30 = G + 0x368; obj->field34 = H + 4
 *          restore_power(obj)
 *          obj->field30 = G + 0x36c; obj->field34 = H
 *          restore_power(obj)
 *          MKEvent_Add(3, 0, G[0x368], 0)
 *          MKEvent_Add(3, 0, G[0x36c], 1)
 *          obj->field1c = (int16_t)G[0x460]
 *          if (obj->field1c <= 3) obj->field1c = 4
 *          G[0x460] = (uint16_t)obj->field1c
 *          obj->field1c = 0x1d
 *          create_fx_param(obj, obj->a10)
 *          park(0x17d0, 0x40)
 *      token 0x17d0:
 *          StartProcAt(&Plyr[0], t_mercy_start)
 *          StartProcAt(&Plyr[1], t_mercy_start)
 *          park(0x17de, 0x40)
 *      token 0x17de:
 *          obj->field1c = 0
 *          G[0x45c] = (uint16_t)0
 *          reset_proc_stack(thread)
 *          frame[frame].handler = t_play3
 *          frame[frame+1].w0 = 0
 *          frame = frame + 1                   ; push
 *          frame[frame].handler = t_master_mercy_entry
 *      otherwise:  return -3
 *
 * Mercy, granted from above: clear the two fatality halfwords, put both health
 * bars back, make sure the clock has at least four on it, announce effect 0x1d
 * -- which `create_fx_param` maps to `t_fx_mercy` -- and start both fighters on
 * `t_mercy_start`.
 *
 * The character handed to the effect is whoever still has health: player one's
 * if their bar is non-zero and player two's otherwise. The one granting mercy.
 *
 * **Each bar is restored against the OTHER player's tally.** 0x368 is paired
 * with `H + 4` and 0x36c with `H`, and H[0] and H[1] are player one's and
 * player two's round wins. The crossing is written out in two adjacent calls
 * with two adjacent constants, so it is not a slip of one offset -- but what
 * `restore_power` does with the pair is in another function and nothing here
 * says why they cross.
 *
 * The clock floor of 4 is applied to the high digit alone, so mercy always
 * leaves at least forty on the timer.
 *
 * The last arm is the call-with-continuation shape: install `t_play3` at this
 * level, then push `t_master_mercy_entry` on top of it. Mercy runs, and when it
 * unwinds the round ends normally.
 */
void reset_proc_stack(MK3THREAD *thread);   /* r0 is the thread, not the object */

long t_master_proc_mercy(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x17d0) {
        StartProcAt((MK3OBJ *)Plyr, (MK3THREADFUNC)t_mercy_start);
        StartProcAt((MK3OBJ *)(Plyr + PLYR_STRIDE),
                    (MK3THREADFUNC)t_mercy_start);
        *mk3_frame(thread, thread->frame + 1) = 0x17de;
        thread->fieldfc = 0x40;
        return 0x40;
    }

    if (token == 0x17de) {
        obj->field1c = 0;
        *(uint16_t *)(G_BYTES + 0x45c) = 0;     /* nobody has won */
        reset_proc_stack(thread);

        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_play3;
        *mk3_frame(thread, thread->frame + 1) = 0;

        thread->frame = thread->frame + 1;              /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_master_mercy_entry;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0)
        return -3;

    obj->field1c = 0;
    *(uint16_t *)(G_BYTES + 0x44e) = 0;
    *(uint16_t *)(G_BYTES + 0x452) = (uint16_t)obj->field1c;

    obj->field24 = *(const uint32_t *)(G_BYTES + 0x368);
    obj->a10 = obj->field24 != 0
                 ? *(const uint32_t *)(GrObj + 0x24)            /* player one */
                 : *(const uint32_t *)(GrObj + GROBJ_STRIDE + 0x24);

    obj->field30 = (uint32_t)(uintptr_t)(G_BYTES + 0x368);
    obj->field34 = (uint32_t)(uintptr_t)(H + 4);        /* the other tally */
    restore_power(obj);

    obj->field30 = (uint32_t)(uintptr_t)(G_BYTES + 0x36c);
    obj->field34 = (uint32_t)(uintptr_t)H;
    restore_power(obj);

    MKEvent_Add(3, 0, (long)*(const uint32_t *)(G_BYTES + 0x368), 0);
    MKEvent_Add(3, 0, (long)*(const uint32_t *)(G_BYTES + 0x36c), 1);

    obj->field1c = (uint32_t)(int32_t)*(const int16_t *)(G_BYTES + 0x460);
    if ((int32_t)obj->field1c <= 3)
        obj->field1c = 4;                       /* at least forty seconds */
    *(uint16_t *)(G_BYTES + 0x460) = (uint16_t)obj->field1c;

    obj->field1c = 0x1d;                        /* -> t_fx_mercy */
    create_fx_param(obj, obj->a10);

    *mk3_frame(thread, thread->frame + 1) = 0x17d0;
    thread->fieldfc = 0x40;
    return 0x40;
}


/* ----------------------------------------------------------------- t_shake3
 *
 * armv7 0x00056e48, four hundred and four bytes.  **Complete**, and it is the
 * shake.
 *
 *      token 0:
 *          o = obj->field00->field28
 *          obj->field28 = o
 *          o->field0e = (int16_t)o->field0e + obj->field40
 *          o->field12 = (int16_t)o->field12 + obj->a10
 *          obj->field1c = args[argc-1]             ; peek the half-period
 *          park(0x1378, obj->field1c)
 *      token 0x1378:
 *          o = obj->field00->field28
 *          obj->field28 = o
 *          o->field0e = (int16_t)o->field0e - obj->field40
 *          o->field12 = (int16_t)o->field12 - obj->a10
 *          obj->field1c = args[argc-1]
 *          park(0x1382, obj->field1c)
 *      token 0x1382:
 *          obj->field48 = obj->field48 - 1
 *          if (obj->field48 > 0) { install t_shake3; return 0; }
 *          obj->field20 = args[--argc]
 *          obj->field48 = args[--argc]
 *          obj->a10     = args[--argc]
 *          obj->field40 = args[--argc]
 *          unwind
 *      otherwise:  return -3
 *
 * Displace, wait, displace back, wait, and round again: a square wave on both
 * axes at once. The amplitudes are 0x40 and A10, the count is 0x48, and the
 * half-period is peeked off the argument stack on every pass with the
 * pop-and-push-back the mframew family uses.
 *
 * **The four pops are exactly the four pushes of `t_shake_him_up` and
 * `t_shake_ob_up`**, in reverse: 0x20, 0x48, 0x44, 0x40 coming back off what
 * went on as 0x40, 0x44, 0x48, 0x20. Those two pushed and installed `t_shake2`,
 * `t_shake2` unpacked a word and installed this, and this is where the
 * arguments land -- four values across three handlers, the deepest argument
 * passing in the file.
 *
 * What moves is `proc->field28`, the slot the shake pair set to either the
 * PROC's own `him` or the object's 0x08. So the two entry points chose whose
 * screen shakes and this does not have to know.
 *
 * The displacement is signed on the way in and stored back as a halfword, so a
 * shake that runs past the coordinate's range wraps rather than clamping.
 *
 * The repeat re-installs the routine at the current level and clears the slot
 * above, so each cycle re-enters at token 0 -- `t_wait_for_his_dog`'s shape.
 */
long t_shake3(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    MK3OBJ  *o;
    uint32_t argc;

    if (token != 0 && token != 0x1378 && token != 0x1382)
        return -3;

    if (token == 0x1382) {
        obj->field48 = obj->field48 - 1;

        if ((int32_t)obj->field48 > 0) {        /* one more cycle */
            mk3_frame(thread, thread->frame)[1] =
                (uint32_t)(uintptr_t)t_shake3;
            *mk3_frame(thread, thread->frame + 1) = 0;
            return 0;
        }

        argc = thread->fieldf8 - 1;             /* what the pair pushed */
        thread->fieldf8 = argc;
        obj->field20 = *mk3_arg(thread, argc);
        argc = thread->fieldf8 - 1;
        thread->fieldf8 = argc;
        obj->field48 = *mk3_arg(thread, argc);
        argc = thread->fieldf8 - 1;
        thread->fieldf8 = argc;
        obj->a10 = *mk3_arg(thread, argc);
        argc = thread->fieldf8 - 1;
        thread->fieldf8 = argc;
        obj->field40 = *mk3_arg(thread, argc);

        return mk3_unwind(thread);
    }

    o = (MK3OBJ *)(uintptr_t)obj->field00->field28;
    obj->field28 = (uint32_t)(uintptr_t)o;

    if (token == 0) {                           /* out */
        obj->field1c = (uint32_t)((int32_t)(int16_t)MK3_FIELD0E(o)
                                  + (int32_t)obj->field40);
        MK3_SET_FIELD0E(o, obj->field1c);
        o = (MK3OBJ *)(uintptr_t)obj->field28;
        obj->field1c = (uint32_t)((int32_t)(int16_t)MK3_FIELD12(o)
                                  + (int32_t)obj->a10);
        MK3_SET_FIELD12(o, obj->field1c);
    } else {                                    /* and back */
        obj->field1c = (uint32_t)((int32_t)(int16_t)MK3_FIELD0E(o)
                                  - (int32_t)obj->field40);
        MK3_SET_FIELD0E(o, obj->field1c);
        o = (MK3OBJ *)(uintptr_t)obj->field28;
        obj->field1c = (uint32_t)((int32_t)(int16_t)MK3_FIELD12(o)
                                  - (int32_t)obj->a10);
        MK3_SET_FIELD12(o, obj->field1c);
    }

    argc = thread->fieldf8 - 1;                 /* peek the half-period */
    thread->fieldf8 = argc;
    obj->field1c = *mk3_arg(thread, argc);
    *mk3_arg(thread, argc) = obj->field1c;
    thread->fieldf8 = argc + 1;

    *mk3_frame(thread, thread->frame + 1) = (token == 0) ? 0x1378 : 0x1382;
    thread->fieldfc = obj->field1c;
    return (long)obj->field1c;
}


/* ---------------------------------------------------- do_next_a9_frame_pxob
 *
 * armv7 0x00059c74, four hundred and sixteen bytes.  **Complete**, and it is
 * the **animation script interpreter**.
 *
 * The cursor is `obj->field40`. Each pass fetches one word, leaves it in 0x1c,
 * and dispatches through a table of nineteen `b.w` instructions the code jumps
 * into with `mov pc, r2` -- so opcodes 0 to 0x12 are a jump table and anything
 * above falls to a default. The loop runs until an opcode returns.
 *
 *       0  store the cursor, return 0                   the end
 *       1  cursor = *cursor                             an unconditional jump
 *       2  other->field28 ^= 0x10                       flip the facing
 *       3  multi_adjust_xy_ob(obj, other, w, 0)         one word
 *       4  multi_adjust_xy_ob(obj, other, w0, w1)       two words
 *       5  obj->field1c = w; ani2_ob(obj, other)
 *          store the cursor, return 1
 *       6  obj->field1c = w                             a word, and nothing
 *       7  the same as 6
 *       8  if (w0 == other->field24) cursor = w1
 *          else cursor += 2                             per character
 *       9  nothing at all
 *      10  cursor = do_ani_offset_xy(obj, ref, other, cursor)
 *      11  z = getprc_z(obj); if (z) link it in as the slave; cursor++
 *      12  cursor = slave_ani(ref, cursor)
 *      13  obj->field1c = w; ani2_ob(obj, other)
 *      14  cursor = slave_ani(ref, cursor)
 *          store the cursor, return 2
 *      15  obj->field1c = w; ochar_sound(ref)
 *      16  multi_adjust_xy_ob(obj, other, w, 0); cursor++   two words, one used
 *      17  (*(void (**)(MK3OBJ *))cursor++)(obj)        call a function
 *      18  obj->field1c = w; rsnd_func(obj, w)
 *     >18  ani2_ob(obj, other); store the cursor, return 2
 *
 * **Three return values**, and they are what the mframew family loops on: 0
 * ends the animation, 1 says a frame was drawn, 2 says one was drawn by a
 * slave or by the default arm. `t_mframew` keeps going while the answer is
 * non-zero, which is why 0 is the only opcode that finishes anything.
 *
 * Opcode 0 is what `stance_setup` scans forward for -- and opcode 8 is the one
 * it follows. Note the difference: **here 8 is CONDITIONAL**, comparing its
 * first word against the other object's character and taking `cursor[1]` only
 * on a match. `stance_setup` follows that target unconditionally. Both are
 * transcribed as written; a walker that only wants the end of the script can
 * take either arm, and nothing says the two were meant to agree.
 *
 * **Opcode 17 is a function pointer in the script.** The interpreter calls it
 * with the object and carries on. That is the sixth place in this file where a
 * routine comes out of data rather than the linker, and the only one where the
 * data is an animation.
 *
 * Opcodes 6 and 7 share an arm and do nothing but consume a word into 0x1c;
 * opcode 9 consumes nothing at all. Three opcodes that are, as written, no-ops
 * -- transcribed rather than folded away, because a script that uses them
 * still has to step over the right number of words.
 *
 * Opcode 16 reads one word, uses it, and skips a second. Opcode 4 reads two
 * and uses both. So the same call takes one, two or two-with-one-ignored
 * depending on which opcode reached it.
 *
 * Opcode 11 is the only one that creates anything: it clones the fighter with
 * `getprc_z`, clears the clone's animation, hangs it off the reference
 * object's 0x64 and 0x68 -- the slave pair -- starts its thread on
 * `t_multi_dummy_proc`, and matches the two up. The slave the `slave_ani`
 * opcodes then drive.
 */

long do_next_a9_frame_pxob(MK3OBJ *obj, MK3OBJ *ref, MK3OBJ *other)
{
    const uint32_t *cursor = (const uint32_t *)(uintptr_t)obj->field40;

    for (;;) {
        uint32_t op = *cursor++;

        obj->field1c = op;

        if (op > 0x12) {                        /* the default arm */
            ani2_ob(obj, other);
            obj->field40 = (uint32_t)(uintptr_t)cursor;
            return 2;
        }

        switch (op) {
        case 0:
            obj->field40 = (uint32_t)(uintptr_t)cursor;
            return 0;                           /* the end */

        case 1:
            cursor = (const uint32_t *)(uintptr_t)cursor[0];
            break;

        case 2:
            other->field28 ^= 0x10u;            /* flip the facing */
            break;

        case 3:
            multi_adjust_xy_ob(obj, (uint32_t)(uintptr_t)other,
                               *cursor++, 0);
            break;

        case 4: {
            uint32_t a = *cursor++;
            uint32_t b = cursor[0];

            cursor++;
            multi_adjust_xy_ob(obj, (uint32_t)(uintptr_t)other, a, b);
            break;
        }

        case 5:
            obj->field1c = *cursor++;
            ani2_ob(obj, other);
            obj->field40 = (uint32_t)(uintptr_t)cursor;
            return 1;

        case 6:
        case 7:
            obj->field1c = *cursor++;           /* read, and nothing else */
            break;

        case 8:                                 /* branch on the character */
            obj->field1c = cursor[0];           /* the compare value is stored too */
            if (cursor[0] == obj->field08->field24)
                cursor = (const uint32_t *)(uintptr_t)cursor[1];
            else
                cursor += 2;
            break;

        case 9:
            break;                              /* nothing at all */

        case 10:
            cursor = do_ani_offset_xy(obj, ref, other, cursor);
            break;

        case 11: {
            MK3OBJ *z = getprc_z(obj);

            if (z != NULL) {
                MK3THREAD *th = z->thread;

                z->field08->field2c = 0xffffffffu;
                ref->field00->field64 = (uint32_t)(uintptr_t)z;
                ref->field00->slave   = (uint32_t)(uintptr_t)z->field08;
                th->func = (MK3THREADFUNC)t_multi_dummy_proc;
                th->field08 = 0;
                match_ani_points_ob_ob((uint32_t)(uintptr_t)ref->field08,
                                       (uint32_t)(uintptr_t)z->field08);
            }
            cursor++;
            break;
        }

        case 12:
            cursor = slave_ani(ref, cursor);
            break;

        case 13:
            obj->field1c = *cursor++;
            ani2_ob(obj, other);
            break;

        case 14:
            cursor = slave_ani(ref, cursor);
            obj->field40 = (uint32_t)(uintptr_t)cursor;
            return 2;

        case 15:
            obj->field1c = *cursor++;
            ochar_sound(ref);
            break;

        case 16:
            multi_adjust_xy_ob(obj, (uint32_t)(uintptr_t)other,
                               *cursor++, 0);
            cursor++;                           /* a second word, unused */
            break;

        case 17: {                              /* a function IN the script */
            void (*fn)(MK3OBJ *) =
                (void (*)(MK3OBJ *))(uintptr_t)*cursor++;

            fn(obj);
            break;
        }

        case 18:
            obj->field1c = *cursor++;
            rsnd_func(obj, obj->field1c);
            break;
        }
    }
}


/* -------------------------------------------------------- strike_check_regs
 *
 * armv7 0x00059280, four hundred and twenty bytes.  **Complete**, and it is
 * the hit test.
 *
 *      save obj->field48, obj->field38, obj->field30
 *      obj->field48 = p[0]
 *      obj->field38 = p[1]
 *      obj->field30 = p[2]
 *      if (obj->field20 & 0x8000) obj->field20 |= 0xffff0000
 *      if (obj->field24 & 0x8000) obj->field24 |= 0xffff0000
 *      obj->field20 = (him->field28 & 0x10) ? -obj->field20
 *                                           : obj->field20 - obj->field28
 *      box_b.left = (int16_t)him->field0e + obj->field20
 *      box_b.top = (int16_t)him->field12 + obj->field24
 *      box_b.right = obj->field28 + box_b.left
 *      box_b.bottom = obj->field2c + box_b.top
 *      highest_mpart_ob(obj, him);  lowest_mpart_ob(obj, him)
 *      leftmost_mpart_ob(obj, him); rightmost_mpart_ob(obj, him)
 *      obj->field2c = |obj->field28 - obj->field24|
 *      cbox_squeeze(obj, obj->field30, him)
 *      box_a.left = obj->field24;  box_a.top = obj->field1c
 *      box_a.right = obj->field28;  box_a.bottom = obj->field20
 *      if (!intersect(box_b, box_a)) { obj->field5c = 0; restore; return; }
 *      if (obj->field14 == 0) <resolve the hit>
 *      obj->field5c = 1
 *      restore the three
 *
 * The three words behind the pointer go into 0x48, 0x38 and 0x30, and all
 * three are put back on the way out -- so the caller's copies survive and this
 * borrows them for the length of the test. `strike_check_box` and
 * `strike_check_ptr` save the same slots around their own calls, which is why
 * they nest.
 *
 * Two coordinates are SIGN-EXTENDED by ORing 0xffff0000 when bit 15 is set,
 * the same expansion `get_tsl_px` uses, so the box may sit at negative
 * coordinates.
 *
 * The horizontal offset is negated or subtracted depending on bit 4 of the
 * opponent's 0x28 -- the facing `am_i_facing_him_px` gave a direction to. A
 * strike box is mirrored with the fighter.
 *
 * **`obj->field38` packs two damages.** Byte 0 is what the hit does through a
 * block and byte 1 what it does through none. And a blocked hit whose chip
 * damage is NOT less than the opponent's remaining health falls through to the
 * full-damage arm: the comparison is `<`, and the else side is the unblocked
 * path. Transcribed as written.
 *
 * The block reaction comes from `_block_xfers` indexed by the low byte of
 * 0x48, and the ordinary one from `react_xfer_him`, which picks from
 * `_reaction_table` by the byte at 0x49. Two tables, two paths, one word.
 *
 * `obj->field14` is the test-only flag `strike_check_ptr` sets from its third
 * argument: non-zero means answer whether they touch and do nothing about it.
 *
 * The four `*_mpart_ob` calls run before the box is assembled, so what they
 * write is what `intersect` compares.
 */
extern uint32_t block_xfers[];          /* through the slot at 0x000f3208 */

/* Port, not in the binary: the debug menu's hitboxes see every strike test,
 * the attack's box and the target's, and whether they met. NULL otherwise. */
void (*PortStrikeHook)(const MK3BOX *attack, const MK3BOX *target, long hit);

long strike_check_regs(MK3OBJ *obj, const uint32_t *p)
{
    uint32_t saved48 = obj->field48;
    uint32_t saved38 = obj->field38;
    uint32_t saved30 = obj->field30;
    uint32_t saved1c;
    MK3BOX box_a, box_b;
    MK3OBJ  *him;
    int32_t  dx;

    obj->field48 = p[0];
    obj->field38 = p[1];
    obj->field30 = p[2];

    if ((obj->field20 & 0x8000u) != 0)          /* sign-extend both */
        obj->field20 |= 0xffff0000u;
    if ((obj->field24 & 0x8000u) != 0)
        obj->field24 |= 0xffff0000u;

    saved1c = obj->field1c;

    if ((obj->field08->field28 & 0x10u) != 0)   /* facing left */
        dx = -(int32_t)obj->field20;
    else
        dx = (int32_t)obj->field20 - (int32_t)obj->field28;
    obj->field20 = (uint32_t)dx;

    box_b.left = (uint32_t)((int32_t)(int16_t)MK3_FIELD0E(obj->field08) + dx);
    box_b.top = (uint32_t)((int32_t)(int16_t)MK3_FIELD12(obj->field08)
                          + (int32_t)obj->field24);
    box_b.right = obj->field28 + box_b.left;
    box_b.bottom = obj->field2c + box_b.top;

    him = (MK3OBJ *)(uintptr_t)obj->field00->him;
    highest_mpart_ob(obj, him);
    lowest_mpart_ob(obj, him);
    leftmost_mpart_ob(obj, him);
    rightmost_mpart_ob(obj, him);

    obj->field2c = (uint32_t)((int32_t)obj->field28 - (int32_t)obj->field24);
    if ((int32_t)obj->field2c < 0)
        obj->field2c = (uint32_t)(-(int32_t)obj->field2c);

    cbox_squeeze(obj, obj->field30,
                 (MK3OBJ *)(uintptr_t)obj->field00->him);

    box_a.left = obj->field24;
    box_a.top = obj->field1c;
    obj->field1c = saved1c;
    box_a.right = obj->field28;
    box_a.bottom = obj->field20;

    if (PortStrikeHook != NULL)
        PortStrikeHook(&box_b, &box_a, intersect(&box_b, &box_a));

    if (intersect(&box_b, &box_a) == 0) {
        obj->field5c = 0;
    } else if (obj->field14 != 0) {             /* asked, not told */
        obj->field5c = 1;
    } else {
        MK3OBJPROC *mine = obj->field00;
        uint32_t packed;
        uint32_t i;

        disable_his_buttons(obj);
        /* 0x48 of his PROC; the struct has a pad there. */
        *(uint32_t *)((char *)mine->field00->field00 + 0x48) = mine->field18;

        i = mine->field00->field00->field08;
        obj->field34 = i;

        is_he_blocking(obj);
        packed = obj->field38;

        if (obj->field5c != 0 &&
            (packed & 0xffu) <
                *(const uint32_t *)(G_BYTES + 0x368 + i * 4)) {
            obj->a10 = packed & 0xffu;          /* the chip damage */
            obj->field18 = 1;
            obj->field48 = (uint8_t)obj->field48;
            obj->field38 = block_xfers[obj->field48];
            xfer_otherguy(obj);
            bar_reducer(obj);
        } else {
            obj->a10 = (packed >> 8) & 0xffu;   /* the full damage */
            bar_reducer(obj);
            obj->field54 = obj->a10;
            add_combo_damage(obj);
            react_xfer_him(obj);
        }
        obj->field5c = 1;
    }

    obj->field30 = saved30;
    obj->field38 = saved38;
    obj->field48 = saved48;
    return (long)obj->field5c;
}


/* ------------------------------------------------------------ t_spawn_wingman
 *
 * armv7 0x0005a6a8, four hundred and fifty-six bytes.  **Complete**, and it is
 * the next endurance opponent walking on.
 *
 *      token 0:
 *          i = obj->field00->field08
 *          c = obj->field30
 *          Endurance_ClearPlayer(i)
 *          set_noedge(obj)
 *          G[0x380 + i*4] = G[0x378 + i*4] = 0x30
 *          G[0x370 + i*4] = G[0x368 + i*4] = 0xa6
 *          MKEvent_Add(3, 0,   0xa6, i)
 *          MKEvent_Add(3, 5,   G[0x378 + i*4], i)
 *          MKEvent_Add(3, 0xa, c, i)
 *          mid = (RoundParam[0] + RoundParam[1]) / 2
 *          if ((int16_t)him->field0e <= mid) {
 *              x = G[0x470] + 0x3c;  facing = 0x10;  vel = 0xfffd8000
 *          } else {
 *              x = G[0x468] - 0x3c;  facing = 0;     vel = 0x00028000
 *          }
 *          obj->field1c = vel
 *          set_x_vel_player(obj)
 *          obj->field08->field0e = x
 *          obj->field08->field28 = facing
 *          obj->field08->field24 = c
 *          ground_ochar(obj)
 *          tsound_func(obj, c + 0x28)
 *          obj->field00->field40 = (int16_t)obj->field08->field12
 *          obj->field40 = 0x1a;  get_char_ani(obj)
 *          obj->field1c = 3
 *          obj->field34 = 0
 *          obj->field20 = 0xfff60000
 *          obj->a10     = 0xfff60000 + 0xa8000
 *          push t_gravity_ani with token 0x171a
 *      token 0x171a:
 *          obj->a10 = 0x80
 *          stance_setup(obj)
 *          park(0x171f, 1)
 *      token 0x171f:
 *          next_anirate(obj)
 *          if (--obj->a10 != 0) park(0x171f, 1)
 *          else { reset_proc_stack(thread); install t_local_reaction_exit }
 *      otherwise:  return -3
 *
 * The whole arrival. His slot is cleared, both his bars filled to the same
 * values `recharge_bars` uses -- 0xa6 health and 0x30 turbo, in the same five
 * pairs -- and three events sent, the last of them (3, 0xa) carrying the
 * character.
 *
 * **He comes in from the far side.** The opponent's x is compared against the
 * midpoint of `RoundParam[0]` and `RoundParam[1]`, and he is placed 0x3c inside
 * whichever edge -- G+0x470 or G+0x468 -- is on the other half, facing inward,
 * with the velocity signed to match. Two more fields of RoundParam, and they
 * are the arena's bounds.
 *
 * The velocities are 16.16 again: -2.5 or +2.5 walking in, then -10.0 and +0.5
 * for the jump. That last pair is the same as `t_do_jump_up`'s and reached the
 * same way -- one literal and `+ 0xa8000` for the second.
 *
 * `tsound_func(obj, c + 0x28)` is his name announced: a base plus the
 * character, like `t_fx_mercy`'s `A10 + 0x99`.
 *
 * The count of 0x80 in the last two arms is a wait measured in calls to
 * `next_anirate` rather than in ticks -- the routine parks for one each time
 * and decrements, so the stance plays for 128 frames before the thread lets go.
 */

long t_spawn_wingman(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    uint32_t *g = (uint32_t *)G_BYTES;
    const uint32_t *rp = (const uint32_t *)RoundParam;
    uint32_t i, c, facing, vel;
    int32_t mid, x;

    if (token == 0x171a) {
        obj->a10 = 0x80;
        stance_setup(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x171f;
        thread->fieldfc = 1;
        return 1;
    }

    if (token == 0x171f) {
        next_anirate(obj);
        obj->a10 = obj->a10 - 1;
        if (obj->a10 != 0) {
            *mk3_frame(thread, thread->frame + 1) = 0x171f;
            thread->fieldfc = 1;
            return 1;
        }

        reset_proc_stack(thread);
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_local_reaction_exit;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0)
        return -3;

    i = obj->field00->field08;
    c = obj->field30;

    Endurance_ClearPlayer(i);
    set_noedge(obj);

    g[(0x380 + i * 4) / 4] = 0x30;              /* turbo, full */
    g[(0x378 + i * 4) / 4] = 0x30;
    g[(0x370 + i * 4) / 4] = 0xa6;              /* health, full */
    g[(0x368 + i * 4) / 4] = 0xa6;

    MKEvent_Add(3, 0, 0xa6, i);
    MKEvent_Add(3, 5, (long)g[(0x378 + i * 4) / 4], i);
    MKEvent_Add(3, 0xa, (long)c, i);

    mid = (int32_t)(rp[0] + rp[1]);
    mid = (mid + (int32_t)((uint32_t)mid >> 31)) >> 1;

    if ((int32_t)(int16_t)MK3_FIELD0E(
            (MK3OBJ *)(uintptr_t)obj->field00->him) <= mid) {
        x      = (int32_t)g[0x470 / 4] + 0x3c;  /* in from the left */
        facing = 0x10;
        vel    = 0xfffd8000u;                   /* -2.5 */
    } else {
        x      = (int32_t)g[0x468 / 4] - 0x3c;  /* in from the right */
        facing = 0;
        vel    = 0x00028000u;                   /* +2.5 */
    }

    obj->field1c = vel;
    set_x_vel_player(obj);

    MK3_SET_FIELD0E(obj->field08, (uint32_t)x);
    obj->field08->field28 = facing;
    obj->field08->field24 = c;

    ground_ochar(obj);
    tsound_func(obj, c + 0x28);    /* his name */

    obj->field00->field40 = (uint32_t)(int32_t)MK3_FIELD12_S(obj->field08);   /* ldrsh */

    obj->field40 = 0x1a;
    get_char_ani(obj);

    obj->field1c = 3;
    obj->field34 = 0;
    obj->field20 = 0xfff60000u;                 /* -10.0 */
    obj->a10     = 0xfff60000u + 0xa8000u;      /*  +0.5, from one literal */

    *mk3_frame(thread, thread->frame + 1) = 0x171a;
    thread->frame = thread->frame + 1;                  /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_gravity_ani;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* -------------------------------------------------------------- init_players
 *
 * armv7 0x0005a418, five hundred and eighty-eight bytes.  **Complete**, and it
 * is where a match begins.
 *
 *      G[0xac]  = RoundParam[2] + 0xf7
 *      G[0xb0]  = RoundParam[0]
 *      G[0xb4]  = RoundParam[1] - 0x18c - 3
 *      G[0x44c] = (uint16_t)RoundParam[3]
 *      init_1_q(G + 0xc0);   init_1_q(G + 0x114)
 *      init_1_q(G + 0x168);  init_1_q(G + 0x1bc)
 *      init_1_q(G + 0x218);  init_1_q(G + 0x26c)
 *      init_1_q(G + 0x2c0);  init_1_q(G + 0x314)
 *      G[0xb8] = 0;  G[0x210] = 0
 *      GrObj[0].field24 = a
 *      GrObj[1].field24 = b
 *      for (n = 0; n < 2; n++) {
 *          Plyr[n].field08 = &GrObj[n]
 *          Plyr[n].field00 = &Pp[n]
 *          Pp[n].field08   = n
 *          Plyr[n].field3c = Plyr[n].field08
 *          Pp[n].him       = &GrObj[1 - n]
 *          Pp[n].field00   = &Plyr[1 - n]
 *          GrObj[n].field0e = n ? 0x12f : 0x60
 *          ground_ochar_ob(&GrObj[n])
 *          Plyr[n].field00->field40 = (int16_t)GrObj[n].field12
 *          Plyr[n].field40 = 0
 *          do_first_a9_frame(&Plyr[n])
 *      }
 *      GrObj[0].field74 ^= 0x10
 *      G[0x214] = &GrObj[1]
 *      G[0xbc]  = &GrObj[0]
 *      G[0xa4]  = 0x20
 *      recharge_bars()
 *
 * The two arguments are the two characters, and everything else is wiring: each
 * player entry points at its GrObj and its Pp, each Pp points at the OTHER
 * fighter's object and player entry, and each Pp's 0x08 is its own index. That
 * is the whole of "who is who" in this engine, set once.
 *
 * **Two offsets are confirmed here.** G+0xbc and G+0x214 are one
 * `G_FIGHTER_STRIDE` apart and take the two objects -- the pair those offsets
 * were named from, now seen being written. And **G+0xa4 starts at 0x20**: the
 * switch stack's cursor at the top of a thirty-two entry array, which is
 * "empty" for a stack `stack_switch_bits` fills downward and `UnstackSwitches`
 * consumes upward to 31. Both ends of that mechanism were read months of
 * functions ago and this is the initialiser they implied.
 *
 * All eight queues are cleared, which is `clear_queues` for both fighters at
 * once -- four each, 0x54 apart, the second set 0x158 on.
 *
 * The starting positions are 0x60 and 0x12f, a fixed pair rather than anything
 * computed from the arena bounds this same routine writes into G.
 *
 * The facing bit is flipped on GrObj[0] only, so the two fighters start facing
 * each other from one constant and one exclusive-or.
 *
 * `RoundParam[1] - 0x18f` is written as a subtraction of 0x18c and then 3,
 * which is the compiler running out of room in one immediate.
 *
 * The `str [sp]` before each `ground_ochar_ob` is the loop counter spilled
 * across the call and read back afterwards, not an argument -- that routine
 * takes one.
 *
 * The original computes the six pointers from `n` on the first pass and then
 * ADVANCES them by their strides for the second, rather than recomputing. The
 * loop here recomputes; the addresses are the same and the arithmetic is the
 * evidence for the strides either way.
 */
void init_players(uint32_t a, uint32_t b)
{
    uint32_t *g  = (uint32_t *)G_BYTES;
    const uint32_t *rp = (const uint32_t *)RoundParam;
    uint32_t n;

    g[0xac / 4] = rp[2] + 0xf7;
    g[0xb0 / 4] = rp[0];
    g[0xb4 / 4] = rp[1] - 0x18c - 3;
    *(uint16_t *)(G_BYTES + 0x44c) = (uint16_t)rp[3];

    init_1_q(G_BYTES + 0x0c0);          /* both fighters' four queues */
    init_1_q(G_BYTES + 0x114);
    init_1_q(G_BYTES + 0x168);
    init_1_q(G_BYTES + 0x1bc);
    init_1_q(G_BYTES + 0x218);
    init_1_q(G_BYTES + 0x26c);
    init_1_q(G_BYTES + 0x2c0);
    init_1_q(G_BYTES + 0x314);

    g[0x0b8 / 4] = 0;                   /* both x velocities */
    g[0x210 / 4] = 0;

    *(uint32_t *)(GrObj + 0x24) = a;
    *(uint32_t *)(GrObj + GROBJ_STRIDE + 0x24) = b;

    for (n = 0; n < 2; n++) {
        char *grobj  = GrObj + n * GROBJ_STRIDE;
        char *other  = GrObj + (1 - n) * GROBJ_STRIDE;
        char *plyr   = Plyr + n * PLYR_STRIDE;
        char *plyr_o = Plyr + (1 - n) * PLYR_STRIDE;
        char *pp     = Pp + n * PP_STRIDE;

        *(char *P32 *)(plyr + 8)    = grobj;
        *(char *P32 *)(plyr)        = pp;
        *(uint32_t *)(pp + 8)   = n;
        *(char *P32 *)(plyr + 0x3c) = *(char *P32 *)(plyr + 8);
        *(char *P32 *)(pp + 4)      = other;        /* the other fighter */
        *(char *P32 *)(pp)          = plyr_o;

        if (n == 0) {
            MK3_SET_FIELD0E((MK3OBJ *)grobj, 0x60);
            ground_ochar_ob((MK3OBJ *)grobj);

            *(uint32_t *)(*(char *P32 *)plyr + 0x40) =
                (uint16_t)MK3_FIELD12((MK3OBJ *)grobj);
            *(uint32_t *)(plyr + 0x40) = 0;

            do_first_a9_frame((MK3OBJ *)plyr);
        } else {
            MK3_SET_FIELD0E((MK3OBJ *)grobj, 0x12f);
            ground_ochar_ob((MK3OBJ *)grobj);

            *(uint32_t *)(*(char *P32 *)plyr + 0x40) =
                (uint16_t)MK3_FIELD12((MK3OBJ *)grobj);
            *(uint32_t *)(plyr + 0x40) = 0;

            do_first_a9_frame((MK3OBJ *)plyr);
        }
    }

    *(uint32_t *)(GrObj + 0x74) ^= 0x10u;       /* face each other */

    g[0x214 / 4] = (uint32_t)(uintptr_t)(GrObj + GROBJ_STRIDE);
    g[0x0bc / 4] = (uint32_t)(uintptr_t)GrObj;
    g[0x0a4 / 4] = 0x20;                        /* the switch stack, empty */

    recharge_bars();
}
