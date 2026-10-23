/*
 * mkreact.c — src/gamecode/logic/mkreact.c (reaction sequences)
 *
 * Reactions are the things that happen TO a fighter: being knocked down,
 * getting up, staggering. They are written as coroutines on the switch-stack
 * machinery that `other.c` describes — each one runs a little, records where to
 * resume, and returns to the scheduler.
 *
 * Hand-written from the disassembly of the armv7 slice and verified against the
 * oracle: tests/test_gup2_diff.c.
 */

#include "mk3logic.h"

#define MK3_CHAR_SHAO_KAHN  0x19

void group_sound(MK3OBJ *obj);
long am_i_short(MK3OBJ *obj);
void shake_a11(MK3OBJ *obj);
void pose_a9_manual(MK3OBJ *obj);
void *FindThreadProc(uint32_t pid);
void away_x_vel(MK3OBJ *obj);
void match_ani_points_ob_ob(uint32_t a, uint32_t b);

/* gup2 was read before other.c existed, and it worked the thread struct out
 * on its own: a frame array at the head, the index at 0xa4, a word at 0xfc
 * that mirrors the return, and the object at 0x108. That is MK3THREAD field
 * for field, arrived at twice from two functions in two files without either
 * reading knowing about the other. Its local `PROC` and `SWITCHFRAME` are
 * dropped here in favour of the shared ones; the agreement is the point and
 * is recorded rather than quietly tidied away.
 *
 * Its local MK3OBJ named 0x1c, 0x40, 0x44, 0x48 and 0x5c, and the shared one
 * names all five. Only the spelling changes -- 0x44 is `a10` there. */

/* The reaction threads gup2 suspends into. Three of these are reached through
 * pointer slots in __DATA rather than directly — see the note on gup2. */
/* Declared as an array so the NAME is the address, without claiming anything
 * about what is stored there. `extern void *x` would say this is a variable
 * holding a pointer; it is code, defined for real in joy.c -- this file only
 * ever takes its address. */
extern char t_check_winner_status[];
long t_check_stay_down(struct MK3THREAD *thread);
long t_mframew(struct MK3THREAD *thread);
long t_local_reaction_exit(MK3THREAD *thread);
extern char t_d_getup[];
long t_getup_stay_ducked(MK3THREAD *thread);
long t_joy_getup_abort(MK3THREAD *thread);

long t_slip_sleep(MK3THREAD *thread);

long t_slammed_zoom_up(MK3THREAD *thread);
void damage_to_me(MK3OBJ *obj);

long t_death_slam_pause(MK3THREAD *thread);
void set_nocol(MK3OBJ *obj);
long t_wait_forever(MK3THREAD *thread);         /* pointer slot 0x000f3724 */

/* A data table, not a resume target. */
extern char getup_speeds[];

/* Decompiled elsewhere; this file owns the sequencing, not the steps. */
void back_to_normal(MK3OBJ *obj);
long  am_i_joy(MK3OBJ *obj);
long is_stick_down(MK3OBJ *obj);
void get_char_ani(MK3OBJ *obj);
void init_anirate(MK3OBJ *obj);
long next_anirate(MK3OBJ *obj);
long joystick_in_a0(MK3OBJ *obj);

long gup2(MK3THREAD *thread);


/* ------------------------------------------------------------------ gup2
 *
 * armv7 0x00044254, 368 bytes.  `_gup2` -- src/gamecode/logic/mkreact.c
 *
 * **A coroutine, not a function.** This is the get-up sequence, and it is
 * resumable: it runs a little, records where to continue, and returns. The
 * switch stack at the head of PROC is how it remembers.
 *
 *      ldr.w r2, [r0, #0xa4]        ; the stack pointer
 *      ldr.w r5, [r0, #0x108]       ; the object
 *      adds  r3, r2, #1
 *      ldr.w r0, [r0, r3, lsl #3]   ; stack[sp + 1].code
 *
 * Entries are EIGHT bytes at `proc + index * 8` -- a code and a resume
 * address -- and the stack pointer lives at `+0xa4`, which is index 20.5. So
 * the array is twenty frames at the head of PROC with the pointer just past
 * it. The same machinery `other.c` describes.
 *
 * ## The five entry codes
 *
 * `stack[sp + 1].code` selects where to resume. Anything else returns -3:
 *
 *      0        first entry
 *      0x14d9   after t_check_stay_down
 *      0x14da   after t_check_winner_status
 *      0x14fc   after the animation step
 *      0x14ff   after t_local_reaction_exit
 *
 * ## Every resume target resolves to a named thread function
 *
 * They are reached two different ways and both had to be followed:
 *
 *      ldr r2, [pc, #N] ; add r2, pc            -> the function, Thumb bit set
 *      ldr r3, [pc, #N] ; add r3, pc ; ldr r1, [r3]   -> a POINTER SLOT
 *
 * The second kind is why three of them first looked like offsets into a UTF-16
 * string blob: the PC-relative arithmetic lands on a slot in __DATA and the
 * function address is what the slot HOLDS.
 *
 *      0x000f37a8 -> _t_check_winner_status
 *      0x000f3708 -> _t_local_reaction_exit
 *      0x000f37a4 -> _t_d_getup
 *      direct     -> _t_check_stay_down, _t_getup_stay_ducked, _t_joy_getup_abort
 *
 * `_getup_speeds` is not a resume target -- it is a data table assigned to the
 * object at `+0x48` on first entry.
 *
 * ## The shared epilogue
 *
 * Three paths jump into 0x442a2 and two into 0x442e0, which are the same two
 * lines: write a resume address into one frame, then zero the code of the frame
 * above it. That zero is what makes the NEXT call take the first-entry branch,
 * so it is the sequence terminating itself rather than housekeeping.
 *
 * ## Return values
 *
 *      -3   the code in the frame is none of the five
 *       0   suspended, resume address recorded
 *       1   the animation wants another frame        (also stored at proc+0xfc)
 *       2   the sequence is finished                 (also stored at proc+0xfc)
 *
 * ## What is NOT established
 *
 * The object offsets. `+0x40` is a pointer that is dereferenced for a word,
 * `+0x44` receives a copy of it and is later compared against 0x17, `+0x1c`
 * takes an animation rate, `+0x5c` is what `is_stick_down` sets, `+0x48` takes
 * the speed table. Every one of those is a single sighting in a single
 * function, which is a hypothesis and not a field. They are named by offset.
 *
 * Verified by tests/test_gup2_diff.c: the call sequence, the frames written,
 * the stack pointer, and the return value, over every entry code and both
 * branches of each gate. The six thread functions and the seven callees are
 * stubbed identically on both sides -- their addresses are compared by
 * identity, since a host build cannot hold the binary's own code pointers.
 */
long gup2(MK3THREAD *thread)
{
    uint32_t sp   = thread->frame;              /* +0xa4 */
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;     /* +0x108 */
    uint32_t code = *mk3_frame(thread, sp + 1);
    uint32_t next;

    switch (code) {
    default:
        return -3;                              /* mvn r0, #2 */

    /* ---------------------------------------------------- first entry */
    case 0:
        obj->field48 = (uint32_t)(uintptr_t)getup_speeds;
        obj->a10 = obj->field40;

        *mk3_frame(thread, sp + 1) = 0x14d9u;
        thread->frame = sp + 1;
        mk3_frame(thread, sp + 1)[1] = (uint32_t)(uintptr_t)t_check_stay_down;
        *mk3_frame(thread, sp + 2) = 0u;
        return 0;

    /* ------------------------------------- resumed after check_stay_down */
    case 0x14d9u:
        *mk3_frame(thread, sp + 1) = 0x14dau;
        thread->frame = sp + 1;
        mk3_frame(thread, sp + 1)[1] = (uint32_t)(uintptr_t)t_check_winner_status;
        *mk3_frame(thread, sp + 2) = 0u;
        return 0;

    /* ---------------------------------- resumed after local_reaction_exit
     *
     * Joins the epilogue above at 0x442a2, but with r2 still holding the
     * stack pointer from ENTRY rather than the incremented one -- so this
     * path writes into frame `sp` and does not advance. Reading the shared
     * tail without tracking which value of r2 reached it would put the resume
     * address one frame too high. */
    case 0x14ffu:
        mk3_frame(thread, sp)[1] = (uint32_t)(uintptr_t)t_local_reaction_exit;
        *mk3_frame(thread, sp + 1) = 0u;
        return 0;

    /* ------------------------------------ resumed after check_winner_status */
    case 0x14dau:
        back_to_normal(obj);
        if (am_i_joy(obj) == 0) {
            mk3_frame(thread, sp)[1] = (uint32_t)(uintptr_t)t_d_getup;
            *mk3_frame(thread, sp + 1) = 0u;
            return 0;
        }
        goto joystick_path;

    /* ------------------------------------------ resumed mid-animation */
    case 0x14fcu:
        goto call_next_anirate;
    }

joystick_path:
    is_stick_down(obj);
    if (obj->field5c != 0) {
        mk3_frame(thread, sp)[1] = (uint32_t)(uintptr_t)t_getup_stay_ducked;
        *mk3_frame(thread, sp + 1) = 0u;
        return 0;
    }

    get_char_ani(obj);
    obj->field1c = 4;
    init_anirate(obj);

/* Binary has ONE static call to next_anirate, at 0x44314 -- reached either
 * by falling through here (fresh animation setup) or by a direct branch
 * from token 0x14fc above (resumed mid-animation). Spelling the call out
 * twice, once per path, made the C report one more next_anirate call than
 * the binary has -- same class of bug as t_r_scream's duplicated shared
 * tail. */
call_next_anirate:
    next_anirate(obj);

    /* 0x40 holds the animation cursor; the shared struct types it as a
     * word, so the dereference is spelled out. */
    next = *(const uint32_t *)(uintptr_t)obj->field40;
    obj->field1c = next;

    if (next == 0u) {
        /* the animation is done */
        *mk3_frame(thread, sp + 1) = 0x14ffu;
        thread->fieldfc            = 2;
        return 2;
    }

    if ((uintptr_t)obj->a10 != 0x17u) {
        is_stick_down(obj);
        if (obj->field5c != 0) {
            mk3_frame(thread, sp)[1] = (uint32_t)(uintptr_t)t_joy_getup_abort;
            *mk3_frame(thread, sp + 1) = 0u;
            return 0;
        }
        joystick_in_a0(obj);
    }

    *mk3_frame(thread, sp + 1) = 0x14fcu;
    thread->fieldfc            = 1;
    return 1;
}


/* --------------------------------------------------------------------
 * What the readers could prove. See tools/pushfn.py, which executes
 * a body symbolically, and tools/microfn.py, which matches whole
 * bodies against templates. Both refuse anything they cannot account
 * for instruction by instruction.
 * -------------------------------------------------------------------- */

long t_avoid_corner_trap(struct MK3THREAD *thread);
long t_avoid_corner_trap_b(struct MK3THREAD *thread);
long t_b_hard(struct MK3THREAD *thread);
long t_block2(struct MK3THREAD *thread);
long t_block_shake_n_exit(struct MK3THREAD *thread);
long t_r_boss_hit1(struct MK3THREAD *thread);
long t_r_kano_swipe(struct MK3THREAD *thread);
long t_r_last_noogy(struct MK3THREAD *thread);
long t_r_lia_zap(struct MK3THREAD *thread);
long t_r_rocket(struct MK3THREAD *thread);
long t_r_motaro_kick(struct MK3THREAD *thread);
long t_r_sw_zap(struct MK3THREAD *thread);
long t_spear0(struct MK3THREAD *thread);
long t_stumble_back_vel(struct MK3THREAD *thread);
long t_sweep3(struct MK3THREAD *thread);
long t_zap_stumble(struct MK3THREAD *thread);

/* t_r_smoke_spear -- armv7 0x000411c4, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x1f
 *      frame[frame].handler = t_spear0
 *      frame[frame+1].w0 = 0
 */

long t_r_smoke_spear(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x1f;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_spear0);
}

/* t_r_scorpion_spear -- armv7 0x00041200, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x11
 *      frame[frame].handler = t_spear0
 *      frame[frame+1].w0 = 0
 */

long t_r_scorpion_spear(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x11;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_spear0);
}

/* t_r_sk_punch -- armv7 0x000413dc, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = t_r_boss_hit1
 *      frame[frame+1].w0 = 0
 */

long t_r_sk_punch(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_r_boss_hit1);
}

/* t_r_angle_kick -- armv7 0x0004154c, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = t_r_last_noogy
 *      frame[frame+1].w0 = 0
 */

long t_r_angle_kick(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_r_last_noogy);
}

/* t_r_axe_horz -- armv7 0x00041590, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = t_r_kano_swipe
 *      frame[frame+1].w0 = 0
 */

long t_r_axe_horz(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_r_kano_swipe);
}

/* t_cc_block_avoid_corner -- armv7 0x00041644, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field20 = 0x1
 *      frame[frame].handler = t_avoid_corner_trap_b
 *      frame[frame+1].w0 = 0
 */

long t_cc_block_avoid_corner(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field20 = 0x1;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_avoid_corner_trap_b);
}

/* t_cc_ken_masters -- armv7 0x00041680, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field20 = 0x1
 *      frame[frame].handler = t_avoid_corner_trap
 *      frame[frame+1].w0 = 0
 */

long t_cc_ken_masters(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field20 = 0x1;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_avoid_corner_trap);
}

/* t_cc_block_upcut -- armv7 0x000416bc, 56 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field20 = 0   (the register the guard proved)
 *      frame[frame].handler = t_avoid_corner_trap
 *      frame[frame+1].w0 = 0
 */

long t_cc_block_upcut(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field20 = 0;   /* the guard proved this register */

    return mk3_push_handler(thread, (MK3THREADFUNC)t_avoid_corner_trap);
}

/* t_r_skull -- armv7 0x00041800, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = t_zap_stumble
 *      frame[frame+1].w0 = 0
 */

long t_r_skull(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_zap_stumble);
}

/* t_r_ermac_zap -- armv7 0x00041834, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = t_zap_stumble
 *      frame[frame+1].w0 = 0
 */

long t_r_ermac_zap(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_zap_stumble);
}

/* t_r_swat_bomb -- armv7 0x00041868, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = t_r_rocket
 *      frame[frame+1].w0 = 0
 */

long t_r_swat_bomb(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_r_rocket);
}

/* t_r_ind_zap -- armv7 0x0004189c, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = t_r_lia_zap
 *      frame[frame+1].w0 = 0
 */

long t_r_ind_zap(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_r_lia_zap);
}

/* t_r_lia_zap -- armv7 0x000418d0, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = t_r_sw_zap
 *      frame[frame+1].w0 = 0
 */

long t_r_lia_zap(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_r_sw_zap);
}

/* t_stumble_back -- armv7 0x00041a78, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x30000
 *      frame[frame].handler = t_stumble_back_vel
 *      frame[frame+1].w0 = 0
 */

long t_stumble_back(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x30000;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_stumble_back_vel);
}

/* t_b_scream -- armv7 0x00041b0c, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = t_b_hard
 *      frame[frame+1].w0 = 0
 */

long t_b_scream(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_b_hard);
}

/* t_b_hard_silent -- armv7 0x00041b40, 56 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field38 = 0   (the register the guard proved)
 *      frame[frame].handler = t_block2
 *      frame[frame+1].w0 = 0
 */

long t_b_hard_silent(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field38 = 0;   /* the guard proved this register */

    return mk3_push_handler(thread, (MK3THREADFUNC)t_block2);
}

/* t_weak3 -- armv7 0x00041dc4, 64 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field48 = 0x2
 *      obj->a10 = 0x3
 *      frame[frame].handler = t_block_shake_n_exit
 *      frame[frame+1].w0 = 0
 */

long t_weak3(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field48 = 0x2;
    obj->a10 = 0x3;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_block_shake_n_exit);
}

/* t_getup_reaction_exit -- armv7 0x00041f8c, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field40 = 0x21
 *      frame[frame].handler = gup2
 *      frame[frame+1].w0 = 0
 */

long t_getup_reaction_exit(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field40 = 0x21;

    return mk3_push_handler(thread, (MK3THREADFUNC)gup2);
}

/* t_sweepup_local_reaction_exit -- armv7 0x00041fc8, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field40 = 0x22
 *      frame[frame].handler = gup2
 *      frame[frame+1].w0 = 0
 */

long t_sweepup_local_reaction_exit(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field40 = 0x22;

    return mk3_push_handler(thread, (MK3THREADFUNC)gup2);
}


/* ---------------------------------------------------------------- inc_p_hit
 *
 * armv7 0x00041ab4, twelve bytes.  **Complete.**
 *
 *      n = obj->field00->p_hit + 1
 *      obj->field1c = n
 *      obj->field00->p_hit = n
 *
 * The hit counter `back_to_normal_px` reads to decide whether a combo
 * happened, and `t_gravity_ani` clears on landing. Both stores come from one
 * register, so the caller sees the new value in 0x1c without a second load.
 */
void inc_p_hit(MK3OBJ *obj)
{
    MK3OBJPROC *proc = obj->field00;
    uint32_t n = proc->p_hit + 1;

    obj->field1c = n;
    proc->p_hit = n;
}


/* -------------------------------------------------------------- inc_p_block
 *
 * armv7 0x00041580, sixteen bytes.  **Complete.**
 *
 *      obj->field00->field4c += 1
 *      obj->field1c = obj->field00->field4c
 *
 * `inc_p_hit`'s twin for blocks, at 0x4c of the PROC -- one of the six words
 * `back_to_normal_px` clears when a fighter goes back to normal. So a block
 * count and a hit count sit side by side and are reset together.
 *
 * Unlike its twin this one RE-READS the field for the copy rather than reusing
 * the register, which is four bytes more and the same answer.
 */
void inc_p_block(MK3OBJ *obj)
{
    uint32_t *n = (uint32_t *)((char *)obj->field00 + 0x4c);

    *n = *n + 1;
    obj->field1c = *(const uint32_t *)((char *)obj->field00 + 0x4c);
}


/* -------------------------------------------------------- if_shao_then_pass
 *
 * armv7 0x00041354, sixteen bytes.  **Complete**, and it names a character.
 *
 *      c = obj->field08->field24
 *      obj->field1c = c
 *      if (c == 0x19) obj->field34 = 0
 *
 * **Character 0x19 is Shao Kahn.** The function's name says so and its body
 * tests that one number -- the same way `t_back_to_shang_form` established
 * 0xc for Shang Tsung. Read off the symbol table rather than guessed from the
 * roster, which is the only way this project will name a character.
 *
 * 0x19 is also one of the three ids `is_finish_him_allowed` refuses, so the
 * two readings agree: the character with no finisher is the boss.
 *
 * The zero is formed as `c - 0x19`, which is zero exactly because the branch
 * that reaches it tested for equality. One register, no constant.
 */
void if_shao_then_pass(MK3OBJ *obj)
{
    uint32_t c = obj->field08->field24;

    obj->field1c = c;
    if (c == MK3_CHAR_SHAO_KAHN)
        obj->field34 = 0;               /* formed as c - 0x19 */
}


/* -------------------------------------------------------- rsnd_react_voice
 *
 * armv7 0x000420e4, sixteen bytes.  **Complete.**
 *
 *      obj->field1c = 6
 *      group_sound(obj)
 *
 * Group six is the reaction voice. `t_do_jump_up` uses group 1 the same way,
 * so 0x1c is which group and `group_sound` picks from it.
 */
void rsnd_react_voice(MK3OBJ *obj)
{
    obj->field1c = 6;
    group_sound(obj);
}


/* -------------------------------------------------------- tall_or_short_ani
 *
 * armv7 0x00044820, twenty bytes.  **Complete.**
 *
 *      am_i_short(obj)
 *      if (obj->field5c != 0) obj->field40 = obj->field30
 *
 * Two animations for one reaction, chosen by height: a short character takes
 * the one parked in 0x30 and everybody else keeps whatever 0x40 already held.
 * So the caller sets both and this picks, which is why the tall case has no
 * store at all.
 */
void tall_or_short_ani(MK3OBJ *obj)
{
    am_i_short(obj);
    if (obj->field5c != 0)
        obj->field40 = obj->field30;
}


/* --------------------------------------------------- at_least_ground_level
 *
 * armv7 0x000410e0, twenty-four bytes.  **Complete.**
 *
 *      y = (int16_t)obj->field08->field12
 *      obj->field1c = y
 *      g = obj->field00->field40
 *      obj->field20 = g
 *      if (g < y) obj->field08->field12 = g
 *
 * The clamp `t_flight_call` ends on, on its own: if the ground is above y the
 * fighter has sunk through the floor and is put back on it. `strh` under `lt`,
 * so it only ever moves upward and never off the floor.
 *
 * Both numbers are left behind, in 0x1c and 0x20, whether or not the clamp
 * fired -- so a caller can see how far under he was.
 */
void at_least_ground_level(MK3OBJ *obj)
{
    int32_t y = (int32_t)(int16_t)MK3_FIELD12(obj->field08);
    int32_t g = (int32_t)obj->field00->field40;

    obj->field1c = (uint32_t)y;
    obj->field20 = (uint32_t)g;

    if (g < y)
        MK3_SET_FIELD12(obj->field08, (uint32_t)g);
}


/* ------------------------------------------------------------ is_he_flipped
 *
 * armv7 0x000410f8, twenty-four bytes.  **Complete.**
 *
 *      f = him->field28
 *      obj->field2c = f
 *      obj->field5c = ((f >> 4) ^ 1) & 1
 *
 * Bit 4 of 0x28 is the facing `am_i_facing_him_px` gave a direction to: set
 * means facing left. So "flipped" is its complement, and this answers 1 when
 * he faces right.
 *
 * The whole flag word is left in 0x2c on the way past, which is how a caller
 * gets at the other bits without a second load.
 */
void is_he_flipped(MK3OBJ *obj)
{
    uint32_t f = ((MK3OBJ *)(uintptr_t)obj->field00->him)->field28;

    obj->field2c = f;
    obj->field5c = ((f >> 4) ^ 1u) & 1u;
}


/* ----------------------------------------------------------- move_slave_too
 *
 * armv7 0x0004761c, twenty-four bytes.  **Complete.**
 *
 *      s = obj->field00->slave
 *      obj->field1c = s
 *      if (s != 0) match_ani_points_ob_ob(obj->field08, s)
 *
 * Whatever just moved the fighter, the slave follows. The PROC's 0x68 is the
 * slave object `slave_ani` drives and the opcode-11 arm of the animation
 * interpreter creates; a zero there means there is none and the call is
 * skipped.
 */
void move_slave_too(MK3OBJ *obj)
{
    uint32_t s = obj->field00->slave;

    obj->field1c = s;
    if (s != 0)
        match_ani_points_ob_ob((uint32_t)(uintptr_t)obj->field08, s);
}


/* --------------------------------------------------------------- combo_setup
 *
 * armv7 0x000424e0, twenty-eight bytes.  **Complete.**
 *
 *      obj->field1c = 2
 *      group_sound(obj)
 *      obj->field48 = 0x60006
 *      shake_a11(obj)
 *
 * **The shake's two amplitudes are one word.** 0x60006 is 6 in each half, and
 * `t_shake2` splits exactly that slot -- the low half into the animation at
 * 0x40 and the high half into the A10 -- which `t_shake3` then uses as the
 * horizontal and vertical displacement. One `mov.w` and the screen shakes six
 * each way.
 *
 * Sound group 2 first, so the noise starts on the same tick as the shake.
 */
void combo_setup(MK3OBJ *obj)
{
    obj->field1c = 2;
    group_sound(obj);

    obj->field48 = 0x60006;             /* six each way, packed */
    shake_a11(obj);
}


/* -------------------------------------------------- pose_stumble_frame_1
 *
 * armv7 0x00047ad8, twenty-eight bytes.  **Complete.**
 *
 *      obj->field40 = 0x20
 *      pose_a9_manual(obj)
 *      obj->field1c = 2
 *      group_sound(obj)
 *
 * Animation 0x20 posed by hand rather than run, then the same sound group 2
 * `combo_setup` uses. The pose comes first, so the frame is on screen before
 * the noise.
 */
void pose_stumble_frame_1(MK3OBJ *obj)
{
    obj->field40 = 0x20;
    pose_a9_manual(obj);

    obj->field1c = 2;
    group_sound(obj);
}


/* -------------------------------------------------------------- shake_n_sound
 *
 * armv7 0x000424fc, twenty-eight bytes.  **Complete.**
 *
 *      obj->field48 = 0x60006
 *      shake_a11(obj)
 *      rsnd_func(obj, 0xd)
 *
 * `combo_setup` without the group sound and with a single effect instead: the
 * same 0x60006 -- six each way, packed as two halves of one word -- and then
 * sound 0xd directly rather than through a group.
 */
void shake_n_sound(MK3OBJ *obj)
{
    obj->field48 = 0x60006;             /* six each way, packed */
    shake_a11(obj);
    rsnd_func(obj, 0xd);
}


/* ---------------------------------------------------------- get_his_floor_ice
 *
 * armv7 0x000495cc, thirty-two bytes.  **Complete.**
 *
 *      i = obj->field00->field00->field00->field08
 *      obj->field1c = i + 0x707
 *      obj->field1c = FindThreadProc(i + 0x707)
 *
 * **A pid is a base plus the fighter's index.** `FindThreadProc` walks the
 * thread list comparing 0x104, and what it is given here is 0x707 plus the
 * opponent's index -- so a thread of this kind is registered under a
 * predictable number and found by arithmetic rather than by being remembered.
 *
 * The pid is left in 0x1c and then overwritten by the answer, so a caller sees
 * the proc and not the number it was found by.
 */
void get_his_floor_ice(MK3OBJ *obj)
{
    uint32_t pid = obj->field00->field00->field00->field08 + 0x707;

    obj->field1c = pid;
    obj->field1c = (uint32_t)(uintptr_t)FindThreadProc(pid);
}


/* ------------------------------------------------------------- get_my_hitq
 *
 * armv7 0x00041ac0, thirty-two bytes.  **Complete.**
 *
 *      obj->field1c = G + 0x390 + obj->field00->field08 * 12
 *
 * A per-fighter array in G with a stride of TWELVE -- a sixth layout in that
 * struct, after the 0x158 blocks, the four-byte bar pairs, the three clock
 * halfwords, the four queues at 0xc0 and the four tables at 0x3a8.
 *
 * `uhq_entry` says what twelve bytes are for: six halfwords. The stride is the
 * size, so each fighter's queue is its own array rather than a window on a
 * longer one.
 *
 * The multiply is `i*16 - i*4`, which is the compiler's way of reaching twelve
 * without a multiply instruction.
 */
void get_my_hitq(MK3OBJ *obj)
{
    uint32_t i = obj->field00->field08;

    obj->field1c = (uint32_t)(uintptr_t)(G_BYTES + 0x390 + i * 12);
}


/* --------------------------------------------------------------- uhq_entry
 *
 * armv7 0x00041ae0, forty-four bytes.  **Complete.**
 *
 *      obj->field38 = obj->field00->field48
 *      get_my_hitq(obj)
 *      q = (uint16_t *)obj->field1c
 *      q[5] = q[4];  q[4] = q[3];  q[3] = q[2]
 *      q[2] = q[1];  q[1] = q[0]
 *      q[0] = (uint16_t)obj->field38
 *
 * The hit queue's push, written out: five moves down and one write at the
 * front, unrolled rather than looped. Six halfwords, oldest at the end, and
 * the sixth falls off.
 *
 * What goes in is the PROC's 0x48. `back_to_normal_px` clears that slot along
 * with the hit and block counters, so it is part of the same bookkeeping -- a
 * record of what has been landing, one entry per hit.
 *
 * The moves run high to low, which is the only order that does not overwrite
 * an entry before it has been copied.
 */
void uhq_entry(MK3OBJ *obj)
{
    uint16_t *q;

    obj->field38 = *(const uint32_t *)((char *)obj->field00 + 0x48);

    get_my_hitq(obj);
    q = (uint16_t *)(uintptr_t)obj->field1c;

    q[5] = q[4];                        /* high to low, or it eats itself */
    q[4] = q[3];
    q[3] = q[2];
    q[2] = q[1];
    q[1] = q[0];
    q[0] = (uint16_t)obj->field38;
}

/*
 * **`G + 0x390` has no reader anywhere in this binary, checked exhaustively
 * across all 7,521 symbols, not just gamecode/logic.** `get_my_hitq` is the
 * only function in the whole game that computes this address: a Thumb
 * disassembly scan walked every symbol looking for the same three-level
 * GOT-indirect pattern this function's own body uses -- `ldr rX,[pc,#n];
 * add rX,pc` to the `_G` slot (0x000f357c), one `ldr rX,[rX]` through it to
 * `_G`'s base (0x0038c1fc), then an `add`/`sub` of #0x390 or #0x39c (the
 * second fighter's entry) off that -- and it found exactly one match: this
 * function itself. Nothing anywhere else in the LIME engine, EA_SDK,
 * front end or gamecode reads it.
 *
 * So this is write-only bookkeeping in the retail game as shipped. A record
 * of the last six hits lands here every time a reaction starts, and as far
 * as this binary is concerned nothing ever looks at it again -- not a
 * combo counter, not an announcer, not an AI heuristic. Whatever it was
 * for, the feature reading it either shipped in a version this binary
 * isn't, or was cut before this one. A port has nothing to reproduce here:
 * there is no observable behavior difference between keeping this queue and
 * not keeping it.
 */


/* --------------------------------------------------------------------
 * What the readers could prove. See tools/pushfn.py, which executes
 * a body symbolically, and tools/microfn.py, which matches whole
 * bodies against templates. Both refuse anything they cannot account
 * for instruction by instruction.
 * -------------------------------------------------------------------- */

long t_wait_forever(struct MK3THREAD *thread);

/* t_r_dummy -- armv7 0x00041110, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = t_wait_forever
 *      frame[frame+1].w0 = 0
 */

long t_r_dummy(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_wait_forever);
}


/* ------------------------------------------------------ get_block_ani_offset
 *
 * armv7 0x00044834, forty-eight bytes.  **Complete.**
 *
 *      obj->field40 = 0xc
 *      obj->field30 = 6
 *      c = obj->field08->field24
 *      obj->field1c = c
 *      if (c == 0xb) {
 *          a = obj->field08->field2c
 *          obj->field1c = a
 *          if ((uint32_t)(a - 0x1e3) <= 2) obj->field40 = 6
 *      }
 *      tall_or_short_ani(obj)
 *
 * Which animation a block uses. Everyone gets 0xc, with 6 parked in 0x30 as
 * the short-character alternative -- `tall_or_short_ani` is what chooses
 * between them, and it runs on every path.
 *
 * **One character is different.** When the character is 0xb and its current
 * animation is one of three consecutive numbers -- 0x1e3, 0x1e4, 0x1e5 --
 * the tall animation becomes 6 as well, so both branches of the height test
 * lead to the same frame.
 *
 * The range is tested as `(a - 0x1e3) <= 2` UNSIGNED, which rejects anything
 * below 0x1e3 by wrapping. One comparison for three values, and a signed test
 * would have needed two.
 *
 * The two constants are loaded once and 6 stays in its register across the
 * whole function, which is why the second store to 0x40 has no `movs` in
 * front of it.
 */
void get_block_ani_offset(MK3OBJ *obj)
{
    uint32_t c;

    obj->field40 = 0xc;
    obj->field30 = 6;                   /* the short-character alternative */

    c = obj->field08->field24;
    obj->field1c = c;

    if (c == 0xb) {
        uint32_t a = obj->field08->field2c;

        obj->field1c = a;
        if ((uint32_t)(a - 0x1e3) <= 2)         /* 0x1e3, 0x1e4 or 0x1e5 */
            obj->field40 = 6;
    }

    tall_or_short_ani(obj);
}


/* --------------------------------------------------------- repell_one_of_us
 *
 * armv7 0x000432b8, sixty-four bytes.  **Complete.**
 *
 *      am_i_close_to_edge(obj)
 *      if (obj->field5c == 0) { away_x_vel(obj); return; }
 *      if (obj->field38 != 0) {
 *          s1 = obj->field1c; s2 = obj->field20
 *          takeover_him(obj)
 *          obj->field1c = s1; obj->field20 = s2
 *      }
 *      obj->field1c = obj->field20
 *      call_for_him(obj, away_x_vel)
 *
 * Two fighters cannot occupy the same ground, and one of them has to give.
 * Away from the edge, this one moves itself. Against it, HE moves instead --
 * `call_for_him` runs `away_x_vel` on the other fighter, which is the same
 * routine pointed the other way.
 *
 * The name says "one of us" and the body says which: whoever is not cornered.
 *
 * `takeover_him` runs first when 0x38 is set, with 0x1c and 0x20 saved and put
 * back around it -- the borrow-and-restore this directory does everywhere, here
 * protecting two slots the callee is known to use.
 *
 * `away_x_vel` arrives through a pointer slot even though the other branch
 * calls it directly four instructions earlier. One function, one routine, two
 * ways of naming it -- because a direct call is a `bl` and a pointer handed to
 * `call_for_him` has to be an address.
 */
void am_i_close_to_edge(MK3OBJ *obj);
void takeover_him(MK3OBJ *obj);
void call_for_him(MK3OBJ *obj, void (*what)(MK3OBJ *));

void repell_one_of_us(MK3OBJ *obj)
{
    am_i_close_to_edge(obj);

    if (obj->field5c == 0) {            /* room to move: move myself */
        away_x_vel(obj);
        return;
    }

    if (obj->field38 != 0) {
        uint32_t s1 = obj->field1c;
        uint32_t s2 = obj->field20;

        takeover_him(obj);
        obj->field1c = s1;
        obj->field20 = s2;
    }

    obj->field1c = obj->field20;
    call_for_him(obj, away_x_vel);      /* cornered: he moves */
}


/* --------------------------------------------------------------------
 * What the readers could prove. See tools/pushfn.py, which executes
 * a body symbolically, and tools/microfn.py, which matches whole
 * bodies against templates. Both refuse anything they cannot account
 * for instruction by instruction.
 * -------------------------------------------------------------------- */

long t_ccp3(struct MK3THREAD *thread);

/* t_cc_hi_punch -- armv7 0x000415c4, 64 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field30 = 0x6
 *      obj->field34 = 0x4
 *      frame[frame].handler = t_ccp3
 *      frame[frame+1].w0 = 0
 */

long t_cc_hi_punch(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field30 = 0x6;
    obj->field34 = 0x4;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_ccp3);
}

/* t_cc_lo_punch -- armv7 0x00041604, 64 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field30 = 0x5
 *      obj->field34 = 0x3
 *      frame[frame].handler = t_ccp3
 *      frame[frame+1].w0 = 0
 */

long t_cc_lo_punch(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field30 = 0x5;
    obj->field34 = 0x3;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_ccp3);
}





/* --------------------------------------------------------------------
 * What the readers could prove. See tools/pushfn.py, which executes
 * a body symbolically, and tools/microfn.py, which matches whole
 * bodies against templates. Both refuse anything they cannot account
 * for instruction by instruction.
 * -------------------------------------------------------------------- */

long t_airborn_hit_no_sound(struct MK3THREAD *thread);
long t_b_combo(struct MK3THREAD *thread);
long t_b_weak_silent(struct MK3THREAD *thread);
long t_combo43(struct MK3THREAD *thread);
long t_fall_on_my_back(struct MK3THREAD *thread);
long t_joy_down(struct MK3THREAD *thread);
long t_joy_getup_entry(struct MK3THREAD *thread);
long t_ken_masters_xfer(struct MK3THREAD *thread);
long t_r_freeze(struct MK3THREAD *thread);
long t_rek3(struct MK3THREAD *thread);
long t_rup3(struct MK3THREAD *thread);
long am_i_airborn(MK3OBJ *obj);
void stance_setup(MK3OBJ *obj);
long create_blood_proc(MK3OBJ *obj);
void create_fx(MK3OBJ *obj);
void find_last_frame(MK3OBJ *obj);
long do_next_a9_frame(MK3OBJ *obj);
void find_ani_last_frame(MK3OBJ *obj);
void get_his_action(MK3OBJ *obj);
void his_ochar_sound(MK3OBJ *obj);
void ochar_sound_c(MK3OBJ *obj, uint32_t arg);
void set_half_damage(MK3OBJ *obj);

/* t_r_lk_zap -- armv7 0x00042190, 68 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x2
 *      group_sound(obj)
 *      frame[frame].handler = t_zap_stumble
 *      frame[frame+1].w0 = 0
 */

long t_r_lk_zap(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x2;
    group_sound(obj);

    return mk3_push_handler(thread, (MK3THREADFUNC)t_zap_stumble);
}

/* t_pit_abort -- armv7 0x000421d4, 68 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x2
 *      group_sound(obj)
 *      frame[frame].handler = t_rup3
 *      frame[frame+1].w0 = 0
 */

long t_pit_abort(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x2;
    group_sound(obj);

    return mk3_push_handler(thread, (MK3THREADFUNC)t_rup3);
}

/* t_b_knee_elbow -- armv7 0x00042310, 72 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field48 = 0x40004
 *      shake_a11(obj)
 *      frame[frame].handler = t_b_combo
 *      frame[frame+1].w0 = 0
 */

long t_b_knee_elbow(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field48 = 0x40004;
    shake_a11(obj);

    return mk3_push_handler(thread, (MK3THREADFUNC)t_b_combo);
}

/* t_r_rocket -- armv7 0x00042400, 72 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field48 = 0x60006
 *      shake_a11(obj)
 *      frame[frame].handler = t_zap_stumble
 *      frame[frame+1].w0 = 0
 */

long t_r_rocket(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field48 = 0x60006;
    shake_a11(obj);

    return mk3_push_handler(thread, (MK3THREADFUNC)t_zap_stumble);
}

/* t_b_weak -- armv7 0x00042778, 68 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      rsnd_func(obj, 0x6)
 *      frame[frame].handler = t_b_weak_silent
 *      frame[frame+1].w0 = 0
 */

long t_b_weak(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    rsnd_func(obj, 0x6);

    return mk3_push_handler(thread, (MK3THREADFUNC)t_b_weak_silent);
}

/* t_b_hard_ken_masters -- armv7 0x000427bc, 80 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      rsnd_func(obj, 0x5)
 *      obj->field38 = t_cc_block_avoid_corner
 *      frame[frame].handler = t_block2
 *      frame[frame+1].w0 = 0
 */

long t_b_hard_ken_masters(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    rsnd_func(obj, 0x5);
    obj->field38 = (uint32_t)(uintptr_t)t_cc_block_avoid_corner;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_block2);
}

/* t_b_hard -- armv7 0x0004280c, 68 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      rsnd_func(obj, 0x5)
 *      frame[frame].handler = t_b_hard_silent
 *      frame[frame+1].w0 = 0
 */

long t_b_hard(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    rsnd_func(obj, 0x5);

    return mk3_push_handler(thread, (MK3THREADFUNC)t_b_hard_silent);
}

/* t_generic_airborn_hit -- armv7 0x00042850, 76 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      rsnd_func(obj, 0x8)
 *      rsnd_react_voice(obj)
 *      frame[frame].handler = t_airborn_hit_no_sound
 *      frame[frame+1].w0 = 0
 */

long t_generic_airborn_hit(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    rsnd_func(obj, 0x8);
    rsnd_react_voice(obj);

    return mk3_push_handler(thread, (MK3THREADFUNC)t_airborn_hit_no_sound);
}

/* t_r_elbow_knee -- armv7 0x00042ed4, 68 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      rsnd_func(obj, 0x8)
 *      frame[frame].handler = t_rek3
 *      frame[frame+1].w0 = 0
 */

long t_r_elbow_knee(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    rsnd_func(obj, 0x8);

    return mk3_push_handler(thread, (MK3THREADFUNC)t_rek3);
}

/* t_separate_us -- armv7 0x00043250, 104 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      am_i_close_to_edge(obj)
 *      obj->field1c = 0x30000
 *      if (obj->field5c == 0) {
 *          frame[frame].handler = t_stumble_back_vel
 *      } else {                        -- at the edge: drag him along
 *          obj->field38 = t_ken_masters_xfer
 *          takeover_him(obj)
 *          frame[frame].handler = t_block_exit
 *      }
 *      frame[frame+1].w0 = 0
 *
 * The cbnz on 0x5c at 0x43274 was missing: the takeover only happens at the
 * edge, and that path installs t_block_exit (0x448dd), not t_stumble_back_vel
 * (difftest).
 */

long t_block_exit(MK3THREAD *thread);

long t_separate_us(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    am_i_close_to_edge(obj);
    obj->field1c = 0x30000;
    if (obj->field5c == 0)
        return mk3_push_handler(thread, (MK3THREADFUNC)t_stumble_back_vel);

    obj->field38 = (uint32_t)(uintptr_t)t_ken_masters_xfer;
    takeover_him(obj);

    return mk3_push_handler(thread, (MK3THREADFUNC)t_block_exit);
}

/* t_block3 -- armv7 0x00043698, 92 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field48 = 0x40004
 *      shake_a11(obj)
 *      obj->field1c = 0x20000
 *      away_x_vel(obj)
 *      obj->field48 = 0x2
 *      obj->a10 = 0x3
 *      frame[frame].handler = t_block_shake_n_exit
 *      frame[frame+1].w0 = 0
 */

long t_block3(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field48 = 0x40004;
    shake_a11(obj);
    obj->field1c = 0x20000;
    away_x_vel(obj);
    obj->field48 = 0x2;
    obj->a10 = 0x3;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_block_shake_n_exit);
}

/* t_joy_getup_abort -- armv7 0x000443c4, 68 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      player_normpal(obj)
 *      frame[frame].handler = t_joy_down
 *      frame[frame+1].w0 = 0
 */

long t_joy_getup_abort(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    player_normpal(obj);

    return mk3_push_handler(thread, (MK3THREADFUNC)t_joy_down);
}

/* t_getup_stay_ducked -- armv7 0x00044408, 84 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      back_to_normal(obj)
 *      obj->field40 = 0x4
 *      find_ani_last_frame(obj)
 *      do_next_a9_frame(obj)
 *      frame[frame].handler = t_joy_getup_entry
 *      frame[frame+1].w0 = 0
 */

long t_getup_stay_ducked(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    back_to_normal(obj);
    obj->field40 = 0x4;
    find_ani_last_frame(obj);
    do_next_a9_frame(obj);

    return mk3_push_handler(thread, (MK3THREADFUNC)t_joy_getup_entry);
}

/* t_r_boss_hit1 -- armv7 0x00045230, 80 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x4
 *      create_blood_proc(obj)
 *      obj->field1c = 0x1
 *      create_blood_proc(obj)
 *      frame[frame].handler = t_combo43
 *      frame[frame+1].w0 = 0
 */

long t_r_boss_hit1(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x4;
    create_blood_proc(obj);
    obj->field1c = 0x1;
    create_blood_proc(obj);

    return mk3_push_handler(thread, (MK3THREADFUNC)t_combo43);
}

/* t_r_bike_kicked_done -- armv7 0x000461ac, 92 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      am_i_airborn(obj)
 *      obj->field1c = 0   (the register the guard proved)
 *      obj->field24 = 0x6000
 *      obj->field20 = 0   (the register the guard proved)
 *      obj->field28 = 0x5
 *      frame[frame].handler = t_fall_on_my_back
 *      frame[frame+1].w0 = 0
 */

long t_r_bike_kicked_done(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    am_i_airborn(obj);
    obj->field1c = 0;   /* the guard proved this register */
    obj->field24 = 0x6000;
    obj->field20 = 0;   /* the guard proved this register */
    obj->field28 = 0x5;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_fall_on_my_back);
}

/* t_r_decoy_freeze -- armv7 0x00046208, 80 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x3
 *      create_fx(obj)
 *      obj->field1c = 0x5
 *      his_ochar_sound(obj)
 *      frame[frame].handler = t_r_freeze
 *      frame[frame+1].w0 = 0
 */

long t_r_decoy_freeze(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x3;
    create_fx(obj);
    obj->field1c = 0x5;
    his_ochar_sound(obj);

    return mk3_push_handler(thread, (MK3THREADFUNC)t_r_freeze);
}

/* t_r_sonya_zap -- armv7 0x00046258, 68 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x1
 *      his_ochar_sound(obj)
 *      frame[frame].handler = t_r_ermac_zap
 *      frame[frame+1].w0 = 0
 */

long t_r_sonya_zap(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x1;
    his_ochar_sound(obj);

    return mk3_push_handler(thread, (MK3THREADFUNC)t_r_ermac_zap);
}

/* t_r_sg_zap -- armv7 0x0004629c, 68 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x2
 *      his_ochar_sound(obj)
 *      frame[frame].handler = t_zap_stumble
 *      frame[frame+1].w0 = 0
 */

long t_r_sg_zap(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x2;
    his_ochar_sound(obj);

    return mk3_push_handler(thread, (MK3THREADFUNC)t_zap_stumble);
}

/* t_r_kano_zap -- armv7 0x000462e0, 68 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x2
 *      his_ochar_sound(obj)
 *      frame[frame].handler = t_zap_stumble
 *      frame[frame+1].w0 = 0
 */

long t_r_kano_zap(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x2;
    his_ochar_sound(obj);

    return mk3_push_handler(thread, (MK3THREADFUNC)t_zap_stumble);
}

/* t_r_mileena_zap -- armv7 0x00046710, 80 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x9
 *      ochar_sound_c(obj, 0x11)
 *      set_half_damage(obj)
 *      frame[frame].handler = t_zap_stumble
 *      frame[frame+1].w0 = 0
 */

long t_r_mileena_zap(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x9;
    ochar_sound_c(obj, 0x11);
    set_half_damage(obj);

    return mk3_push_handler(thread, (MK3THREADFUNC)t_zap_stumble);
}

/* t_r_combo4 -- armv7 0x00046808, 64 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      set_half_damage(obj)
 *      frame[frame].handler = t_combo43
 *      frame[frame+1].w0 = 0
 */

long t_r_combo4(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    set_half_damage(obj);

    return mk3_push_handler(thread, (MK3THREADFUNC)t_combo43);
}





/* --------------------------------------------------------------------
 * What the readers could prove. See tools/pushfn.py, which executes
 * a body symbolically, and tools/microfn.py, which matches whole
 * bodies against templates. Both refuse anything they cannot account
 * for instruction by instruction.
 * -------------------------------------------------------------------- */

/* --------------------------------------------------------------------
 * Added by a later sweep -- tools/sweep.py, running the same
 * readers again after one of them learned something. Each still
 * refuses anything it cannot account for instruction by
 * instruction; see tools/pushfn.py and tools/leaffn.py.
 * -------------------------------------------------------------------- */

/* --------------------------------------------------------------------
 * Added by a later sweep -- tools/sweep.py, running the same
 * readers again after one of them learned something. Each still
 * refuses anything it cannot account for instruction by
 * instruction; see tools/pushfn.py and tools/leaffn.py.
 * -------------------------------------------------------------------- */

long t_local_reaction_exit(MK3THREAD *thread);
long t_net_struggle(MK3THREAD *thread);
long t_rhat_wake(MK3THREAD *thread);
long t_suspend_wait_action_jsrp(MK3THREAD *thread);
long t_suspend_wait_wake(MK3THREAD *thread);

/* t_r_bike_kicked -- armv7 0x000412bc, 76 bytes.  **Complete.**
 *
 *      token == 0:
 *          park(token 0x4be, duration 0x80)
 *      token == 0x4be:
 *          frame[frame].handler = t_r_bike_kicked_done
 *      otherwise:  return -3
 */
long t_r_bike_kicked(MK3THREAD *thread)
{
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x4be;
        thread->fieldfc = 0x80;
        return 0x80;
    }

    if (token != 0x4be)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_r_bike_kicked_done);
}

/* t_net_sleep -- armv7 0x00041308, 76 bytes.  **Complete.**
 *
 *      token == 0:
 *          park(token 0x4cd, duration 0x1)
 *      token == 0x4cd:
 *          frame[frame].handler = t_net_struggle
 *      otherwise:  return -3
 */
long t_net_sleep(MK3THREAD *thread)
{
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x4cd;
        thread->fieldfc = 0x1;
        return 0x1;
    }

    if (token != 0x4cd)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_net_struggle);
}

/* t_r_airborn_duck_kick -- armv7 0x000416f4, 120 bytes.  **Complete.**
 *
 *      token == 0:
 *          obj->field20 = 0x1
 *          token := 0x1067, then descend into t_avoid_corner_trap
 *      token == 0x1067:
 *          frame[frame].handler = t_generic_airborn_hit
 *      otherwise:  return -3
 */
long t_r_airborn_duck_kick(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field20 = 0x1;
        *mk3_frame(thread, thread->frame + 1) = 0x1067;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_avoid_corner_trap;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x1067)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_generic_airborn_hit);
}

/* t_rhat_sleep -- armv7 0x00041a2c, 76 bytes.  **Complete.**
 *
 *      token == 0:
 *          park(token 0x11ea, duration 0x1)
 *      token == 0x11ea:
 *          frame[frame].handler = t_rhat_wake
 *      otherwise:  return -3
 */
long t_rhat_sleep(MK3THREAD *thread)
{
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x11ea;
        thread->fieldfc = 0x1;
        return 0x1;
    }

    if (token != 0x11ea)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_rhat_wake);
}

/* t_susp3 -- armv7 0x00041e48, 76 bytes.  **Complete.**
 *
 *      token == 0:
 *          park(token 0x1453, duration 0x2)
 *      token == 0x1453:
 *          frame[frame].handler = t_suspend_wait_wake
 *      otherwise:  return -3
 */
long t_susp3(MK3THREAD *thread)
{
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x1453;
        thread->fieldfc = 0x2;
        return 0x2;
    }

    if (token != 0x1453)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_suspend_wait_wake);
}

/* t_suspend_wait_action -- armv7 0x00041e94, 104 bytes.  **Complete.**
 *
 *      token == 0:
 *          token := 0x145c, then descend into t_suspend_wait_action_jsrp
 *      token == 0x145c:
 *          frame[frame].handler = t_local_reaction_exit
 *      otherwise:  return -3
 */
long t_suspend_wait_action(MK3THREAD *thread)
{
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x145c;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_suspend_wait_action_jsrp;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x145c)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

/* --------------------------------------------------------------------
 * Added by a later sweep -- tools/sweep.py, running the same
 * readers again after one of them learned something. Each still
 * refuses anything it cannot account for instruction by
 * instruction; see tools/pushfn.py and tools/leaffn.py.
 * -------------------------------------------------------------------- */

long t_block_exit(MK3THREAD *thread);
long t_block_shake(MK3THREAD *thread);

/* t_block_shake_n_exit -- armv7 0x00044864, 120 bytes.  **Complete.**
 *
 *      token == 0:
 *          get_block_ani_offset(obj)
 *          token := 0x13eb, then descend into t_block_shake
 *      token == 0x13eb:
 *          frame[frame].handler = t_block_exit
 *      otherwise:  return -3
 */
long t_block_shake_n_exit(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        get_block_ani_offset(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x13eb;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_block_shake;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x13eb)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_block_exit);
}

/* --------------------------------------------------------------------
 * Added by a later sweep -- tools/sweep.py, running the same
 * readers again after one of them learned something. Each still
 * refuses anything it cannot account for instruction by
 * instruction; see tools/pushfn.py and tools/leaffn.py.
 * -------------------------------------------------------------------- */

long t_flight(MK3THREAD *thread);
long t_land_on_my_back(MK3THREAD *thread);
long t_reaction_land(MK3THREAD *thread);

/* t_airborn_hit_no_sound -- armv7 0x0004176c, 148 bytes.  **Complete.**
 *
 *      token == 0:
 *          obj->field1c = 0x38000
 *          obj->field20 = 0xfffa0000
 *          obj->field24 = 0x8000
 *          obj->field28 = 0x5
 *          obj->field40 = 0x1e
 *          token := 0x1073, then descend into t_flight
 *      token == 0x1073:
 *          frame[frame].handler = t_land_on_my_back
 *      otherwise:  return -3
 */
long t_airborn_hit_no_sound(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field1c = 0x38000;
        obj->field20 = 0xfffa0000;
        obj->field24 = 0x8000;
        obj->field28 = 0x5;
        obj->field40 = 0x1e;
        *mk3_frame(thread, thread->frame + 1) = 0x1073;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x1073)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_land_on_my_back);
}

/* t_r_post_shake -- armv7 0x00041904, 148 bytes.  **Complete.**
 *
 *      token == 0:
 *          obj->field1c = 0x40000
 *          obj->field20 = 0xfffc0000
 *          obj->field24 = 0x8000
 *          obj->field28 = 0x5
 *          obj->field40 = 0x1e
 *          token := 0x113b, then descend into t_flight
 *      token == 0x113b:
 *          frame[frame].handler = t_reaction_land
 *      otherwise:  return -3
 */
long t_r_post_shake(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field1c = 0x40000;
        obj->field20 = 0xfffc0000;
        obj->field24 = 0x8000;
        obj->field28 = 0x5;
        obj->field40 = 0x1e;
        *mk3_frame(thread, thread->frame + 1) = 0x113b;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x113b)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_reaction_land);
}

/* t_r_post_bike -- armv7 0x00041998, 148 bytes.  **Complete.**
 *
 *      token == 0:
 *          obj->field1c = 0x40000
 *          obj->field20 = 0xfffa0000
 *          obj->field24 = 0xa000
 *          obj->field28 = 0x5
 *          obj->field40 = 0x1e
 *          token := 0x1145, then descend into t_flight
 *      token == 0x1145:
 *          frame[frame].handler = t_reaction_land
 *      otherwise:  return -3
 */
long t_r_post_bike(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field1c = 0x40000;
        obj->field20 = 0xfffa0000;
        obj->field24 = 0xa000;
        obj->field28 = 0x5;
        obj->field40 = 0x1e;
        *mk3_frame(thread, thread->frame + 1) = 0x1145;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x1145)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_reaction_land);
}

/* t_combo_airborn_hit -- armv7 0x0004289c, 176 bytes.  **Complete.**
 *
 *      token == 0:
 *          obj->field1c = 0x2
 *          group_sound(obj)
 *          rsnd_func(obj, 0xa)
 *          obj->field48 = 0x60006
 *          shake_a11(obj)
 *          obj->field1c = 0x50000
 *          obj->field20 = 0xfffd0000
 *          obj->field24 = 0x8000
 *          obj->field28 = 0x5
 *          obj->field40 = 0x1e
 *          token := 0x1061, then descend into t_flight
 *      token == 0x1061:
 *          frame[frame].handler = t_land_on_my_back
 *      otherwise:  return -3
 */
long t_combo_airborn_hit(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field1c = 0x2;
        group_sound(obj);
        rsnd_func(obj, 0xa);
        obj->field48 = 0x60006;
        shake_a11(obj);
        obj->field1c = 0x50000;
        obj->field20 = 0xfffd0000;
        obj->field24 = 0x8000;
        obj->field28 = 0x5;
        obj->field40 = 0x1e;
        *mk3_frame(thread, thread->frame + 1) = 0x1061;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x1061)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_land_on_my_back);
}

/* t_combo2 -- armv7 0x00046944, 172 bytes.  **Complete.**
 *
 *      token == 0:
 *          obj->field1c = 0x4
 *          create_blood_proc(obj)
 *          set_half_damage(obj)
 *          obj->field1c = 0xe
 *          create_fx(obj)
 *          obj->field1c = 0x40000
 *          obj->field20 = 0xfff80000
 *          obj->field24 = 0x8000
 *          obj->field28 = 0x5
 *          obj->field40 = 0x1e
 *          token := 0xca9, then descend into t_flight
 *      token == 0xca9:
 *          frame[frame].handler = t_land_on_my_back
 *      otherwise:  return -3
 */
long t_combo2(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field1c = 0x4;
        create_blood_proc(obj);
        set_half_damage(obj);
        obj->field1c = 0xe;
        create_fx(obj);
        obj->field1c = 0x40000;
        obj->field20 = 0xfff80000;
        obj->field24 = 0x8000;
        obj->field28 = 0x5;
        obj->field40 = 0x1e;
        *mk3_frame(thread, thread->frame + 1) = 0xca9;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0xca9)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_land_on_my_back);
}


/* ======================================================================
 * The reactions. What happens to the fighter who is HIT, which is the half
 * of a fight this port has been missing.
 *
 * Every one below was read from the annotated disassembly and checked with
 * tools/factdiff.py against tools/facts_asm.py's reading of the same
 * function, so a store, a call, a handler or a token that differs between
 * the two is a difference the tool names rather than something a person has
 * to notice.
 * ====================================================================== */

void delete_slave_notproj(MK3OBJ *obj);
void player_normpal(MK3OBJ *obj);
void face_opponent(MK3OBJ *obj);
void flip_multi(MK3OBJ *obj);
void stop_me_player(MK3OBJ *obj);
long t_rst5(struct MK3THREAD *thread);
extern const uint32_t motaro_branches[];  /* 0x0017b8d0 */
extern const uint32_t sk_branches[];      /* 0x0017b884 */


/* ----------------------------------------------------- reaction_start_chores
 *
 * armv7 0x00044b0c, a hundred and twenty bytes.
 *
 *      inc_p_hit(obj)
 *      obj->field38 = 0 ; obj->field08->field20 = 0
 *      if (obj->field00->field10 & 8) {
 *          obj->field1c = 1 ; G[0x452] = 1        ; the halfword, as a SHORT
 *      }
 *      obj->field2c = obj->field00->field10 | 4
 *      obj->field00->field10 = obj->field2c
 *      delete_slave_notproj(obj)
 *      obj->field2c = (obj->field08->field30 & ~0x1a4) | 0x10
 *      obj->field08->field30 = obj->field2c
 *      obj->field1c = 0
 *      obj->field00->field50 = 0 ; obj->field00->field58 = 0
 *      player_normpal ; face_opponent ; stop_me_player ; uhq_entry
 *
 * **Everything that has to be true before a reaction can play**, in one
 * routine that eight of them call. Read as a list of what a hit costs you:
 * your slave object, your palette, your facing, your velocity, and four
 * separate flag words.
 *
 * `G[0x452]` is the halfword the punch swings test before their strike check
 * -- `t_jhp4` and its three siblings retract instead of striking when it is
 * set. This is where the 1 comes from, and it is written only when bit 3 of
 * the header's 0x10 is already up, so not every reaction raises it.
 *
 * The mask `~0x1a4` clears bits 2, 5, 7 and 8 of the other object's 0x30 and
 * then sets bit 4. Five bits touched in one expression, which the compiler
 * folded into a `bic` and an `orr`; both are kept rather than collapsed into
 * a single constant, because the pair is what the original says.
 *
 * `stop_me_player` here is the same full stop `t_air_strike` uses to land a
 * fighter: velocity cleared and then applied, so it is zero.
 */
void reaction_start_chores(MK3OBJ *obj)
{
    uint32_t flags;

    inc_p_hit(obj);

    obj->field38 = 0;
    obj->field08->field20 = 0;

    flags = obj->field00->field10;
    obj->field2c = flags;
    if (flags & 8) {
        obj->field1c = 1;
        *(uint16_t *)(void *)(G_BYTES + 0x452) = 1;
    }

    obj->field2c = obj->field2c | 4;
    obj->field00->field10 = obj->field2c;

    delete_slave_notproj(obj);

    obj->field2c = (obj->field08->field30 & ~0x1a4u) | 0x10u;
    obj->field08->field30 = obj->field2c;

    obj->field1c = 0;
    obj->field00->field50 = 0;
    obj->field00->field58 = obj->field1c;

    player_normpal(obj);
    face_opponent(obj);
    stop_me_player(obj);
    uhq_entry(obj);
}


/* --------------------------------------------------------- t_reaction_start
 *
 * armv7 0x00044b84, a hundred and eight bytes.
 *
 *      state 0 only
 *          obj->field1c = 0x503 ; part->field18 = 0x503
 *          save obj->field30, obj->field34, obj->field38
 *          reaction_start_chores(obj)
 *          restore the three
 *          install t_rst5
 *
 * **The save and restore is the whole point.** `reaction_start_chores` zeroes
 * field38 on its second line and writes field2c all over, and this routine
 * pulls 0x30, 0x34 and 0x38 into three registers BEFORE the call and puts
 * them back after it. So whatever the hit deposited in those three survives
 * the housekeeping, and the housekeeping does not have to know about them.
 *
 * The compiler keeps them in fp, sl and r8 -- three callee-saved registers
 * pushed in the prologue for exactly this -- which is how you can tell the
 * saving is deliberate and not an artefact.
 *
 * 0x503 is the action tag for a reaction. It goes in both places the tag
 * lives: the object's own 0x1c and the header's 0x18, which is the pair
 * `is_he_blocking` and the strike tests read.
 */
long t_reaction_start(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t saved30, saved34, saved38;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x503;
    obj->field00->field18 = 0x503;

    saved30 = obj->field30;
    saved34 = obj->field34;
    saved38 = obj->field38;

    reaction_start_chores(obj);

    obj->field30 = saved30;
    obj->field34 = saved34;
    obj->field38 = saved38;

    return mk3_install(thread, (MK3THREADFUNC)t_rst5);
}


/* ======================================================================
 * The one-shot reaction steps.
 *
 * Three of the same shape, and it is the commonest shape in this file: do
 * one thing, then POP. No token of their own, no state to resume at -- the
 * caller pushed them, they act once, and the frame index comes back down.
 * The `t_local_reaction_exit` at the bottom of each is the floor: a routine
 * pushed at the bottom of the stack has nothing to pop to, and that is where
 * it goes instead.
 * ====================================================================== */

void repell_one_of_us(MK3OBJ *obj);


/* ------------------------------------------------------- t_block_shake_wake
 *
 * armv7 0x00041e04, sixty-eight bytes.
 *
 *      state 0 only:  pop
 *
 * **It does nothing.** No call, no store, not one field touched -- the whole
 * body is the guard and the pop. It exists to be a NAME in the frame: the
 * block shake pushes it, sleeps, and the wake-up is this routine returning
 * control to whatever was underneath.
 *
 * Worth writing out rather than folding into its caller, because a handler
 * is identified by its address and the shake needs something to point at.
 * Several of these appear in this file and none of them is dead code.
 */
long t_block_shake_wake(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
        return 0;
    }
    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* ----------------------------------------------------------- t_net_struggle
 *
 * armv7 0x0004416c, eighty bytes.
 *
 *      state 0 only:  next_anirate(obj) ; pop
 *
 * One frame of animation and out. `next_anirate` is the clock -- it counts
 * the rate down and advances a frame only when it reaches zero -- so a
 * caller that pushes this on a loop plays the struggle at whatever rate is
 * already installed, without knowing what that rate is.
 *
 * The name says a net; nothing here does. What it is a struggle AGAINST is
 * decided entirely by whichever animation the caller set up.
 */
long t_net_struggle(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    next_anirate(obj);

    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
        return 0;
    }
    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* --------------------------------------------------------- t_cc_block_sweep
 *
 * armv7 0x000432f8, ninety-two bytes.
 *
 *      state 0 only
 *          obj->field38 = 0
 *          obj->field1c = 0x30000 ; obj->field20 = 0x30000
 *          repell_one_of_us(obj)
 *          pop
 *
 * **0x30000 is 3.0.** The same 16.16 the jump's -10.0 and +0.5 are written
 * in, and it goes into BOTH velocity slots from one register -- `mov.w r3,
 * #0x30000` and two stores. So this is three units a frame, in whichever
 * pair 0x1c and 0x20 are, applied just before the two fighters are pushed
 * apart.
 *
 * `repell_one_of_us` is the separation. `_repell` in the port's own tick
 * carries the reading of the leash at 304 units; this is the half of it that
 * moves ONE fighter, which is what a sweep needs -- the man on the floor
 * stays put and the man who swept is the one who gives ground.
 */
long t_cc_block_sweep(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field38 = 0;
    obj->field1c = 0x30000;         /* 3.0 in 16.16 */
    obj->field20 = 0x30000;
    repell_one_of_us(obj);

    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
        return 0;
    }
    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


void pose_stumble_frame_1(MK3OBJ *obj);
long t_susp3(struct MK3THREAD *thread);
long t_shake_ob_up(struct MK3THREAD *thread);
long t_rek3(struct MK3THREAD *thread);


/* ------------------------------------------- t_suspend_wait_action_jsrp
 *
 * armv7 0x00044674, seventy-two bytes.
 *
 *      state 0 only
 *          get_his_action(obj)
 *          obj->field48 = obj->field20
 *          install t_susp3
 *
 * `get_his_action` leaves the other fighter's action tag in field20, and
 * this copies it straight into field48 before handing on. So whatever
 * `t_susp3` does about a suspended fighter, it does it against **the tag the
 * opponent had at this instant**, frozen here rather than read later.
 *
 * That is worth naming because the tag moves: a fighter changes action
 * several times a second, and a routine that asked again two frames on
 * would get a different answer.
 */
long t_suspend_wait_action_jsrp(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    get_his_action(obj);
    obj->field48 = obj->field20;

    return mk3_install(thread, (MK3THREADFUNC)t_susp3);
}


/* ------------------------------------------------------- t_slammed_shake_up
 *
 * armv7 0x00047af4, ninety-two bytes.
 *
 *      state 0 only
 *          pose_stumble_frame_1(obj)
 *          obj->field08->field1c = 0xfffe0000      ; -2.0
 *          obj->field1c = 0x30003
 *          obj->field20 = 2 ; obj->field24 = 4
 *          install t_shake_ob_up
 *
 * **-2.0 upward, and the number is written as one literal.** The other
 * object's 0x1c takes -0x20000, which in the 16.16 this engine writes its
 * positions in is minus two -- a small hop, an eighth of the jump's -10.0.
 * So being slammed lifts you a little rather than launching you.
 *
 * 0x30003 is not a coordinate. It is two halves, 3 and 3, in the slot the
 * animation rate lives in -- the same packed-pair habit `t_do_flip` uses for
 * its animation numbers. Which half is which is not established here.
 *
 * The 4 in field24 is `adds r3, r3, r3` on the 2 that just went into
 * field20: the compiler doubled the register rather than loading a second
 * constant, and the pair is kept as a pair for that reason.
 */
long t_slammed_shake_up(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    pose_stumble_frame_1(obj);

    obj->field08->field1c = 0xfffe0000u;    /* -2.0 in 16.16 */
    obj->field1c = 0x30003;
    obj->field20 = 2;
    obj->field24 = 2 + 2;

    return mk3_install(thread, (MK3THREADFUNC)t_shake_ob_up);
}


/* -------------------------------------------------------- t_block_shake_ani
 *
 * armv7 0x0004445c, a hundred bytes.
 *
 *      state 0
 *          do_next_a9_frame(obj)
 *          obj->field1c = obj->field48
 *          token = 0x1436 ; thread->fieldfc = obj->field1c
 *          return obj->field1c
 *      state 0x1436
 *          install t_block_shake_wake
 *
 * **The sleep length comes out of field48, not out of a constant.** One
 * frame of animation, then park for however many frames the caller left
 * there -- and the same value is both the sleep counter and the return, the
 * way every parking state in this directory does it.
 *
 * So the shake's rhythm is set by whoever pushed it, and this routine is the
 * one beat. `t_block_shake_wake` underneath does nothing at all, which makes
 * the pair a pure timer: play a frame, wait, hand back.
 */
long t_block_shake_ani(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        do_next_a9_frame(obj);
        obj->field1c = obj->field48;

        *mk3_frame(thread, thread->frame + 1) = 0x1436;
        thread->fieldfc = obj->field1c;
        return (long)obj->field1c;
    }

    if (token != 0x1436)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_block_shake_wake);
}


/* ----------------------------------------------------------- t_r_tusk_elbow
 *
 * armv7 0x00044e90, a hundred bytes.
 *
 *      state 0 only
 *          obj->field1c = 4 ; create_blood_proc(obj)
 *          obj->field1c = 4 ; create_blood_proc(obj)
 *          rsnd_func(obj, 3)
 *          install t_rek3
 *
 * **Two of the same blood event, back to back.** field1c is reloaded with 4
 * between the calls because `create_blood_proc` re-reads it after firing the
 * event and can leave something else there -- so the second 4 is not a
 * redundant store, it is the reload that makes the second spray happen.
 *
 * `create_blood_proc` refuses anything above 12, so 4 is one of thirteen
 * kinds and this reaction asks for the same kind twice rather than for a
 * bigger one. Sound 3 goes with it.
 *
 * The compiler keeps the 4 in r8 across both calls, which is why the
 * prologue saves that register in a function with nothing else to spill.
 */
long t_r_tusk_elbow(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 4;
    create_blood_proc(obj);
    obj->field1c = 4;
    create_blood_proc(obj);

    rsnd_func(obj, 3);

    return mk3_install(thread, (MK3THREADFUNC)t_rek3);
}


void tsound_func(MK3OBJ *obj, long which);
long t_rhat_sleep(struct MK3THREAD *thread);


/* ------------------------------------------------------- t_suspend_wait_wake
 *
 * armv7 0x0004460c, a hundred and four bytes.
 *
 *      state 0 only
 *          get_his_action(obj)
 *          obj->field20 == obj->field48 ? install t_susp3
 *                                       : pop
 *
 * The other half of `t_suspend_wait_action_jsrp`, which froze the opponent's
 * tag in field48. This asks for it again and compares: **the same tag means
 * he has not moved on**, and only then does the suspension continue into
 * `t_susp3`. Any change at all and this pops, which ends the wait.
 *
 * So the pair is a "hold while he is still doing that" and the equality is
 * exact -- not a mask, not a range. One tag, tested whole.
 */
long t_suspend_wait_wake(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    get_his_action(obj);
    if (obj->field20 == obj->field48)
        return mk3_install(thread, (MK3THREADFUNC)t_susp3);

    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
        return 0;
    }
    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* ------------------------------------ t_bone_grind_sound and t_machine_sound
 *
 * armv7 0x0004791c and 0x00047984, a hundred and four bytes each.
 *
 *      state 0        obj->a10 = N            ; 3 grinds, 6 machine
 *                     -> play
 *      state TOK      if (--obj->a10 != 0) -> play
 *                     token = DONETOK ; thread->fieldfc = MK3_THREAD_DONE
 *                     return MK3_THREAD_DONE
 *      play:          tsound_func(obj, S)     ; 0x23 grind, 0x22 machine
 *                     token = TOK ; thread->fieldfc = 0x10 ; return 0x10
 *
 * **A sound played N times, sixteen frames apart, and then the thread
 * deletes itself.** The counter lives in a10 -- the argument slot -- rather
 * than in a state, and the loop is a branch back into the play block from
 * the resume, which is why both share one tail.
 *
 * The 0x16462 at the end is `MK3_THREAD_DONE`: the header establishes that
 * a handler returning it is unlinked and freed on the spot, so these do not
 * pop to a caller. They are their own thread and they end it.
 *
 * The two differ in exactly three constants -- the count, the sound and the
 * pair of tokens -- and in nothing else, so they are written together.
 */
long t_bone_grind_sound(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->a10 = 3;
    } else {
        if (token != 0x9b7)
            return -3;
        obj->a10 = obj->a10 - 1;
        if (obj->a10 == 0) {
            *mk3_frame(thread, thread->frame + 1) = 0x9bb;
            thread->fieldfc = MK3_THREAD_DONE;
            return MK3_THREAD_DONE;
        }
    }

    tsound_func(obj, 0x23);
    *mk3_frame(thread, thread->frame + 1) = 0x9b7;
    thread->fieldfc = 0x10;
    return 0x10;
}


long t_machine_sound(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->a10 = 6;
    } else {
        if (token != 0x9ac)
            return -3;
        obj->a10 = obj->a10 - 1;
        if (obj->a10 == 0) {
            *mk3_frame(thread, thread->frame + 1) = 0x9b0;
            thread->fieldfc = MK3_THREAD_DONE;
            return MK3_THREAD_DONE;
        }
    }

    tsound_func(obj, 0x22);
    *mk3_frame(thread, thread->frame + 1) = 0x9ac;
    thread->fieldfc = 0x10;
    return 0x10;
}


/* -------------------------------------------------------------- t_rhat_wake
 *
 * armv7 0x00044bf0, a hundred and twelve bytes.
 *
 *      state 0 only
 *          obj->field1c = obj->field00->field20
 *          if (obj->field1c == 1) { obj->field1c = 5 ; create_blood_proc }
 *          next_anirate(obj)
 *          --obj->a10 > 0 ? install t_rhat_sleep
 *                         : install t_local_reaction_exit
 *
 * **Blood on one frame of the animation and no other.** The proc's 0x20 is
 * the animation's own counter -- it runs down from the rate and reloads --
 * so `== 1` is true on exactly the tick before a frame advances. One spray
 * per animation frame, and the routine does not have to know the rate.
 *
 * The 5 is `adds r3, #4` on the 1 that was just compared, which is the same
 * register trick `t_slammed_shake_up` uses for its 2 and 4. It is kept as an
 * addition rather than written as 5 because that is what the original says.
 *
 * Unlike the pop-shaped routines above, both exits here are INSTALLS: this
 * one does not come back, it hands on. A countdown in a10 decides which.
 */
long t_rhat_wake(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = obj->field00->field20;
    if (obj->field1c == 1) {
        obj->field1c = 1 + 4;
        create_blood_proc(obj);
    }

    next_anirate(obj);

    obj->a10 = obj->a10 - 1;
    if ((long)obj->a10 > 0)
        return mk3_install(thread, (MK3THREADFUNC)t_rhat_sleep);

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


void am_i_close_to_edge(MK3OBJ *obj);
void xfer_otherguy(MK3OBJ *obj);
void zero_my_p_hit(MK3OBJ *obj);
void inc_p_block(MK3OBJ *obj);
long t_block3(struct MK3THREAD *thread);
long t_blocked_start(struct MK3THREAD *thread);
long t_ken_masters_xfer(struct MK3THREAD *thread);
long t_pit_abort(struct MK3THREAD *thread);
long t_fall_down_pit(struct MK3THREAD *thread);
long t_fall_down_bell_tower(struct MK3THREAD *thread);
long t_fall_on_trax(struct MK3THREAD *thread);
long t_fall_in_lava(struct MK3THREAD *thread);


/* ---------------------------------------------------------------- t_block2
 *
 * armv7 0x00041b78, a hundred and sixteen bytes.
 *
 *      state 0       part->field30 = 0 ; part->field34 = 0
 *                    push t_blocked_start             (0x1322)
 *      state 0x1322  install t_block3
 *
 * `part->field34` is the DIRECTION MASK -- `mask_joystick` ANDs the stick's
 * four bits with it -- so clearing it takes every direction away for the
 * duration of the block. 0x30 goes with it, and this is the only writer of
 * the pair in the file.
 *
 * So a blocked hit costs you the stick before anything else happens, and
 * gives it back through whatever restores 0x34 later.
 */
long t_block2(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field30 = 0;
        obj->field34 = 0;

        *mk3_frame(thread, thread->frame + 1) = 0x1322;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_blocked_start;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x1322)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_block3);
}


/* ------------------------------------------------------- t_blocked_start
 *
 * armv7 0x000475a0, a hundred and twenty-four bytes.
 *
 *      state 0 only
 *          obj->field1c = 0x503 ; part->field18 = 0x503
 *          save 0x30, 0x34, 0x38 ; reaction_start_chores ; restore
 *          zero_my_p_hit(obj)
 *          inc_p_block(obj)
 *          obj->field30 = 0
 *          install t_rst5
 *
 * **`t_reaction_start` with two lines added, and those two lines are the
 * whole difference between being hit and blocking a hit.**
 *
 * Everything up to the restore is the same function, register for register,
 * including the same save of the three fields the chores would destroy. Then
 * this one CLEARS the hit counter and BUMPS the block counter. The tag it
 * writes is the same 0x503 -- so as far as every other routine is concerned
 * a blocked hit is a reaction like any other, and only the two counters know
 * the difference.
 *
 * That is worth stating because it means the block has no tag of its own to
 * test for. Anything wanting to know reads p_hit and p_block.
 */
long t_blocked_start(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t saved30, saved34, saved38;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x503;
    obj->field00->field18 = 0x503;

    saved30 = obj->field30;
    saved34 = obj->field34;
    saved38 = obj->field38;

    reaction_start_chores(obj);

    obj->field30 = saved30;
    obj->field34 = saved34;
    obj->field38 = saved38;

    zero_my_p_hit(obj);
    inc_p_block(obj);
    obj->field30 = 0;

    return mk3_install(thread, (MK3THREADFUNC)t_rst5);
}


/* --------------------------------------------------- t_avoid_corner_trap
 *
 * armv7 0x00047b50, a hundred and sixteen bytes.
 *
 *      state 0 only
 *          am_i_close_to_edge(obj)
 *          if (obj->field5c) {
 *              obj->field1c = obj->field00->p_hit
 *              if (obj->field1c >= obj->field20) {
 *                  obj->field38 = t_ken_masters_xfer
 *                  xfer_otherguy(obj)
 *              }
 *          }
 *          pop
 *
 * **A mercy rule, and it has two conditions.** Near a wall AND having taken
 * at least `field20` hits, the other fighter is handed to
 * `t_ken_masters_xfer` -- which is to say the game moves HIM rather than
 * the man in the corner.
 *
 * The threshold is not a constant: 0x20 is whatever the caller left there,
 * so how many hits count as a trap is decided outside. `p_hit` at 0x44 of
 * the header is the counter `zero_my_p_hit` clears and `inc_p_hit` bumps,
 * which is the same one `t_blocked_start` resets on a block -- so blocking
 * genuinely buys you out of the corner rule.
 *
 * The handler goes into field38 and then `xfer_otherguy` acts on it. Storing
 * a routine in a field for another function to pick up is the same handover
 * `obj->field34` does for the jump's per-frame callback.
 */
long t_avoid_corner_trap(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    am_i_close_to_edge(obj);
    if (obj->field5c != 0) {
        obj->field1c = obj->field00->p_hit;
        if ((long)obj->field1c >= (long)obj->field20) {
            obj->field38 = (uint32_t)(uintptr_t)t_ken_masters_xfer;
            xfer_otherguy(obj);
        }
    }

    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
        return 0;
    }
    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* ------------------------------------------------------- t_background_death
 *
 * armv7 0x00041364, a hundred and twenty bytes.
 *
 *      state 0 only
 *          switch (G[0x24] - 1) {
 *              case 0: install t_fall_down_pit
 *              case 1: install t_fall_down_bell_tower
 *              case 2: install t_fall_on_trax
 *              case 3: install t_fall_in_lava
 *              default: install t_pit_abort
 *          }
 *
 * **The stage hazards, as one table.** `G[0x24]` is which stage is being
 * played; subtract one and four of them have a death of their own. Every
 * other stage gets `t_pit_abort`, which is to say nothing happens.
 *
 * The compiler emitted it as a `tbb` -- a byte table of branch offsets
 * immediately after the instruction, indexed by the register -- so the four
 * cases really are a switch in the original and not a chain of compares.
 * The bound is `cmp r3, #3 ; bhi`, unsigned, so a stage number of zero wraps
 * to a huge value and takes the default rather than falling off the table.
 *
 * Four hazards for a game with more stages than that: the Pit, the Bell
 * Tower, the Subway and the Lava. Which stage number is which is not
 * established here.
 *
 * **CHECKED BY HAND, not by tools/factdiff.py.** The four cases branch into
 * one shared store, so the asm reader sees a single handler with a value it
 * cannot resolve and reports all four of these as invented. The four targets
 * were resolved individually off the `tbb` table at 0x00041388 -- offsets
 * 0x11, 0x14, 0x18, 0x1b from it, landing on 0x413aa, 0x413b0, 0x413b8 and
 * 0x413be -- and each pc-relative load there names a real symbol. That is
 * the evidence; the tool is not able to repeat it yet and its gap list says
 * so.
 */
extern long *RoundParam;                /* pointer slot -> 0x0038ed04 */
long t_background_death(MK3THREAD *thread)
{
    uint32_t stage;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    stage = (uint32_t)RoundParam[9] - 1;   /* slot 0xf3534 is RoundParam, not G (difftest) */

    if (stage > 3)
        return mk3_install(thread, (MK3THREADFUNC)t_pit_abort);

    switch (stage) {
    case 0:
        return mk3_install(thread, (MK3THREADFUNC)t_fall_down_pit);
    case 1:
        return mk3_install(thread, (MK3THREADFUNC)t_fall_down_bell_tower);
    case 2:
        return mk3_install(thread, (MK3THREADFUNC)t_fall_on_trax);
    default:
        return mk3_install(thread, (MK3THREADFUNC)t_fall_in_lava);
    }
}


long t_cc_ken_masters(struct MK3THREAD *thread);


/* ------------------------------------------------------- t_r_null_speared
 *
 * armv7 0x00041144, a hundred and thirty-two bytes.
 *
 *      state 0
 *          part->field30 = 0 ; part->field34 = 0 ; part->field38 = 0
 *          push t_reaction_start                  (0x3f4)
 *      state 0x3f4
 *          obj->field1c = 0x40000                 ; 4.0
 *          pop
 *
 * A reaction with a number left over for whoever pops back to. 0x40000 is
 * 4.0 in the engine's 16.16, the same shape as the sweep's 3.0 and the
 * flip's 4.0 -- so this is a speed handed up rather than a state of its own.
 * The name says a speared reaction with nothing to react TO; the number is
 * what a caller further up does with that.
 */
long t_r_null_speared(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field30 = 0;
        obj->field34 = 0;
        obj->field38 = 0;

        *mk3_frame(thread, thread->frame + 1) = 0x3f4;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_reaction_start;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x3f4)
        return -3;

    obj->field1c = 0x40000;         /* 4.0 in 16.16 */

    /* Corrected: the binary installs t_stumble_back_vel -- no pop. */
    return mk3_install(thread, (MK3THREADFUNC)t_stumble_back_vel);
}


/* -------------------------------------------------------------- t_r_zoom
 *
 * armv7 0x0004123c, a hundred and twenty-eight bytes.
 *
 *      state 0
 *          part->field30 = 0 ; part->field34 = 1 ; part->field38 = 0
 *          push t_reaction_start                  (0x43a)
 *      state 0x43a
 *          pop
 *
 * The same shape as `t_r_null_speared` with one bit changed: field34 is set
 * to 1 rather than cleared, and there is no number left behind on the way
 * out. Field34 is the direction mask `mask_joystick` reads, so this reaction
 * leaves exactly ONE direction bit standing rather than none.
 */
long t_r_zoom(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field30 = 0;
        obj->field34 = 1;
        obj->field38 = 0;

        *mk3_frame(thread, thread->frame + 1) = 0x43a;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_reaction_start;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x43a)
        return -3;

    /* corrected: installs t_suspend_wait_action -- no pop */
    return mk3_install(thread, (MK3THREADFUNC)t_suspend_wait_action);
}


/* --------------------------------------------------------- t_b_weak_silent
 *
 * armv7 0x00041d40, a hundred and thirty-two bytes.
 *
 *      state 0
 *          part->field30 = 0 ; part->field34 = 0
 *          part->field38 = t_cc_ken_masters
 *          push t_blocked_start                   (0x13bd)
 *      state 0x13bd
 *          pop
 *
 * A BLOCK, not a hit -- it pushes `t_blocked_start`, the routine that clears
 * p_hit and bumps p_block, where the two above push `t_reaction_start`. And
 * it parks `t_cc_ken_masters` in field38, the same handover slot
 * `t_avoid_corner_trap` uses for its own transfer, so whatever reads field38
 * next hands this block off to that routine specifically.
 *
 * "Silent" and "no masters" together suggest this is a blocked hit from an
 * opponent the corner rule does not apply to; nothing in this function
 * itself says why, and that is left unguessed.
 */
long t_b_weak_silent(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field30 = 0;
        obj->field34 = 0;
        obj->field38 = (uint32_t)(uintptr_t)t_cc_ken_masters;

        *mk3_frame(thread, thread->frame + 1) = 0x13bd;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_blocked_start;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x13bd)
        return -3;

    /* Corrected: the binary installs t_weak3 here, unconditionally -- there
     * is no "pop a level" on this path, and no t_local_reaction_exit. */
    return mk3_install(thread, (MK3THREADFUNC)t_weak3);
}


/* ---------------------------------------------------- t_b_weak_no_masters
 *
 * armv7 0x000426f4, a hundred and thirty-two bytes.
 *
 *      state 0
 *          rsnd_func(obj, 6)
 *          part->field30 = 0 ; part->field34 = 0 ; part->field38 = 0
 *          push t_blocked_start                   (0x13ca)
 *      state 0x13ca
 *          pop
 *
 * The plain block: a sound, the three fields cleared with no handover parked
 * in field38, and `t_blocked_start`. Same family as `t_b_weak_silent`, minus
 * whatever "silent" was withholding -- here the sound plays and nothing is
 * queued in field38 for a later routine to pick up.
 */
long t_b_weak_no_masters(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        rsnd_func(obj, 6);

        obj->field30 = 0;
        obj->field34 = 0;
        obj->field38 = 0;

        *mk3_frame(thread, thread->frame + 1) = 0x13ca;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_blocked_start;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x13ca)
        return -3;

    /* Corrected: the binary installs t_weak3 here, unconditionally -- there
     * is no "pop a level" on this path, and no t_local_reaction_exit. */
    return mk3_install(thread, (MK3THREADFUNC)t_weak3);
}


long t_generic_airborn_hit(struct MK3THREAD *thread);
long t_joy_block_loop(struct MK3THREAD *thread);
long t_joy_duck_block_loop(struct MK3THREAD *thread);
long t_flight(struct MK3THREAD *thread);
long t_d_post_block(struct MK3THREAD *thread);


/* ----------------------------------------------------------------- t_cc_punch
 *
 * armv7 0x00041bec, a hundred and thirty-six bytes.
 *
 *      state 0        obj->field20 = 3
 *                     push t_avoid_corner_trap_b        (resume 0x137c)
 *      state 0x137c   pop a level, or install t_local_reaction_exit
 *
 * **Corrected, 2026-09-25.** This was first read as a three-turn sleep that
 * counts field20 down on field08's part. The binary has no countdown and no
 * sleep: it writes 3 into the object's own field20 ([thread->proc, #0x20])
 * and pushes t_avoid_corner_trap_b -- field20 is the count that routine
 * works with -- then pops when it returns.
 */
long t_cc_punch(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field20 = 3;

        *mk3_frame(thread, thread->frame + 1) = 0x137c;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_avoid_corner_trap_b;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x137c)
        return -3;

    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
        return 0;
    }
    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* -------------------------------------------------------------- t_r_last_noogy
 *
 * armv7 0x00042db0, a hundred and forty bytes.
 *
 *      state 0
 *          rsnd_func(obj, 8)
 *          part->field38 = 0
 *          part->field30 = t_generic_airborn_hit
 *          part->field34 = 1
 *          push t_reaction_start                  (0xe35)
 *      state 0xe35
 *          pop
 *
 * Parks `t_generic_airborn_hit` in field30 -- the WALK ROUTINE slot
 * `plyrthread` calls indirectly -- rather than in field38 the way the block
 * handovers do. So this reaction is not queuing a routine for a later step
 * to pick up; it is substituting what runs on the object's normal update in
 * place of walking, for as long as field30 holds it.
 */
long t_onback3(MK3THREAD *thread);

long t_r_last_noogy(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        rsnd_func(obj, 8);

        obj->field38 = 0;
        obj->field30 = (uint32_t)(uintptr_t)t_generic_airborn_hit;
        obj->field34 = 1;

        *mk3_frame(thread, thread->frame + 1) = 0xe35;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_reaction_start;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0xe35)
        return -3;

    /* corrected: installs t_onback3 -- no pop; and the three stores
     * above go to the object itself ([thread->proc, #off]), not field08 */
    return mk3_install(thread, (MK3THREADFUNC)t_onback3);
}


/* ------------------------------------------------------------ t_fall_on_my_back
 *
 * armv7 0x00041efc, a hundred and forty-four bytes.
 *
 *      state 0
 *          part->field24 = 0x8000                 ; 0.5
 *          part->field1c = 0 ; part->field20 = 0
 *          part->field28 = 5
 *          part->field40 = 5 + 0x19               ; 30, SCKNOCKDOWN
 *          push t_flight                          (0x1474)
 *      state 0x1474
 *          install t_reaction_land
 *
 * The same launch shape `t_do_flip` uses -- a velocity, a gravity, an
 * animation number -- built here for a fall onto your back rather than a
 * jump. `part->field40 = 30` is animation 30, which `docs`/other work has
 * already pinned as SCKNOCKDOWN. So this is what happens after a hit strong
 * enough to put you flat: a short flight, then `t_reaction_land`, which
 * this file's own comment on `FALL_TAIL` in the Godot port already reads --
 * it plays the two extra knockdown frames and holds the last one.
 *
 * 0x8000 is 0.5 in 16.16, the same magnitude the jump's gravity uses; only
 * the velocity in field1c/field20 differs, and both are zero here, which
 * means this fall's motion is gravity alone with no initial push.
 */
long t_fall_on_my_back(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field24 = 0x8000;         /* 0.5 in 16.16 */
        obj->field1c = 0;
        obj->field20 = 0;
        obj->field28 = 5;
        obj->field40 = 5 + 0x19;        /* 30, SCKNOCKDOWN */

        *mk3_frame(thread, thread->frame + 1) = 0x1474;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x1474)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_reaction_land);
}


/* -------------------------------------------------------------- t_block_exit
 *
 * armv7 0x000448dc, a hundred and forty-four bytes.
 *
 *      state 0
 *          obj->field1c = 0
 *          obj->field00->p_hit = 0 ; obj->field00->field4c = obj->field1c
 *          back_to_normal(obj)
 *          am_i_joy(obj)
 *          if (!obj->field5c)
 *              install t_d_post_block
 *          am_i_short(obj)
 *          if (obj->field5c)
 *              install t_joy_duck_block_loop
 *          install t_joy_block_loop
 *
 * **Three ways out of a block, in order.** Against the machine, always
 * `t_d_post_block` -- a player only reaches the other two. A human is then
 * asked if he is short -- ducking -- and goes back into the duck-block loop
 * or the standing one accordingly.
 *
 * `am_i_short` is asked with `am_i_joy` already known true, so it is not
 * "am I a small character" but "is the stick held down right now": the
 * routine is choosing which loop to re-enter based on the CURRENT stance,
 * not resuming whichever one was interrupted.
 *
 * `back_to_normal` runs unconditionally before any of the three branches,
 * so whatever it undoes is undone regardless of which loop comes next.
 *
 * **`p_hit` is cleared here too, and field4c is a mirror of the same 0.**
 * Blocking ends with the hit counter at zero exactly the way
 * `t_blocked_start` clears it going in; leaving the block does not carry a
 * count into whatever comes next.
 */
long t_block_exit(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0;
    obj->field00->p_hit = 0;
    obj->field00->field4c = obj->field1c;

    back_to_normal(obj);

    am_i_joy(obj);
    if (obj->field5c == 0)
        return mk3_install(thread, (MK3THREADFUNC)t_d_post_block);

    am_i_short(obj);
    if (obj->field5c != 0)
        return mk3_install(thread, (MK3THREADFUNC)t_joy_duck_block_loop);

    return mk3_install(thread, (MK3THREADFUNC)t_joy_block_loop);
}


long t_pit_fall_scan(struct MK3THREAD *thread);
long t_flight_loop(struct MK3THREAD *thread);
void clear_shadow_bit(MK3OBJ *obj);
void distance_off_ground(MK3OBJ *obj);
void call_a0_for_him(MK3OBJ *obj);
void zero_turbo_bar(MK3OBJ *obj);
void lights_on_hit(MK3OBJ *obj);
long is_he_airborn(MK3OBJ *obj);
void towards_x_vel(MK3OBJ *obj);
void get_x_dist(MK3OBJ *obj);
void clear_noflip(MK3OBJ *obj);
void get_char_ani2(MK3OBJ *obj);
void q_am_i_a_boss(MK3OBJ *obj);
void center_around_me(MK3OBJ *obj);
void ground_player(MK3OBJ *obj);
void MKEvent_Add(long type, long subtype, long param, long player);


/* -------------------------------------------------------------------- t_b_sweep
 *
 * armv7 0x00042660, a hundred and forty-eight bytes.
 *
 *      state 0
 *          rsnd_func(obj, 6)
 *          part->field30 = 0 ; part->field34 = 0
 *          part->field38 = t_cc_block_sweep
 *          push t_blocked_start                   (0x13d4)
 *      state 0x13d4
 *          part->field48 = 2 ; part->field44 = 2
 *          pop
 *
 * A block, like `t_b_weak_silent`, and it hands off to `t_cc_block_sweep`
 * through field38 the same way -- which is the routine that writes 3.0 into
 * both velocity slots and calls `repell_one_of_us`, so blocking a sweep
 * still pushes you back rather than standing you in place.
 *
 * The two 2s after the reaction starts are p_hit's neighbours -- 0x44 is
 * p_hit itself and 0x48 the field beside it -- both written from the same
 * register, which is the compiler reusing a value rather than two separate
 * constants happening to match.
 */
long t_b_sweep(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        rsnd_func(obj, 6);

        obj->field30 = 0;
        obj->field34 = 0;
        obj->field38 = (uint32_t)(uintptr_t)t_cc_block_sweep;

        *mk3_frame(thread, thread->frame + 1) = 0x13d4;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_blocked_start;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x13d4)
        return -3;

    obj->field48 = 2;
    obj->a10 = 2;

    /* Corrected: the binary installs t_block_shake_n_exit -- no pop. */
    return mk3_install(thread, (MK3THREADFUNC)t_block_shake_n_exit);
}


/* ------------------------------------------------------------------- t_pounce4
 *
 * armv7 0x000446bc, a hundred and forty-eight bytes.
 *
 *      state 0
 *          get_his_action(obj)
 *          obj->field20 == 0x20f ? go to the wait branch
 *                                : install t_getup_reaction_exit
 *      wait branch
 *          token = 0x4a0 ; push t_suspend_wait_action_jsrp
 *      state 0x4a0
 *          install t_getup_reaction_exit
 *
 * **A one-shot poll, not a loop.** This asks the opponent's action exactly
 * once. If he is already in 0x20f it defers to
 * `t_suspend_wait_action_jsrp` -- which itself freezes the tag and hands to
 * `t_susp3` -- for one extra step; any other action goes straight to
 * `t_getup_reaction_exit`. So the pounce only waits when it catches him in
 * that specific state, and only for the single step the suspend routine
 * takes.
 */
long t_pounce4(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        get_his_action(obj);
        if (obj->field20 != 0x20f)
            return mk3_install(thread, (MK3THREADFUNC)t_getup_reaction_exit);

        *mk3_frame(thread, thread->frame + 1) = 0x4a0;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_suspend_wait_action_jsrp;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x4a0)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_getup_reaction_exit);
}


/* ------------------------------------------------------------ t_slammed_slam_down
 *
 * armv7 0x000470dc, a hundred and forty-eight bytes.
 *
 *      state 0
 *          obj->field20 = obj->field08->field20 = 0x80000   ; 8.0
 *          token = 0x1cf ; thread->fieldfc = 1 ; return 1
 *      state 0x1cf
 *          obj->field1c = (int16_t)MK3_FIELD12(obj->field08)
 *          obj->field20 = obj->field00->field40     (both corrected by difftest)
 *          if (obj->field20 > obj->field1c) sleep again
 *          stop_me_player(obj) ; ground_player(obj)
 *          pop
 *
 * **The floor test, written out a second time.** `field20 > field1c` is the
 * same "am I still above the floor" comparison `t_air_strike` makes against
 * `MK3_FIELD12`, one frame at a time, and it ends the same way: a full stop
 * and a snap to the ground. Gravity itself is 8.0 here rather than the
 * jump's 0.5 -- a much harder fall, which fits a slam.
 */
long t_slammed_slam_down(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field20 = 0x80000;                 /* 8.0 in 16.16 */
        obj->field08->field20 = 0x80000;

        *mk3_frame(thread, thread->frame + 1) = 0x1cf;
        thread->fieldfc = 1;
        return 1;
    }

    if (token != 0x1cf)
        return -3;

    obj->field1c = (uint32_t)(int32_t)(int16_t)MK3_FIELD12(obj->field08);
    obj->field20 = obj->field00->field40;       /* ldr r3,[r4]; ldr r3,[r3,#0x40] */
    if ((long)obj->field20 > (long)obj->field1c) {
        thread->fieldfc = 1;
        return 1;
    }

    stop_me_player(obj);
    ground_player(obj);

    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
        return 0;
    }
    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* ------------------------------------------------------- t_fall_down_bell_tower
 *
 * armv7 0x0004858c, a hundred and thirty-two bytes.
 *
 *      state 0 only
 *          MKEvent_Add(4, 0x3d, 0, obj->field00->field08)
 *          obj->field1c = 9 ; group_sound(obj)
 *          clear_shadow_bit(obj) ; center_around_me(obj)
 *          obj->field48 = 0x6000a ; shake_a11(obj)
 *          obj->field1c = 0 ; obj->field20 = 0xfff40000  ; -12.0
 *          obj->field24 = 0xfff40000 + 0xc6000            ; -0.406
 *          obj->field28 = 5 ; obj->field40 = 5 + 0x19  ; 30, SCKNOCKDOWN
 *          obj->field34 = t_pit_fall_scan
 *          install t_pit_fall_scan
 *
 * **The stage's own death, chosen by `t_background_death`'s table.** The
 * event, 4/0x3d, carries the strength index at proc->0x08 as its player
 * argument and 0 (the token, still zero here) as its param; presumably the
 * fall's own trigger for whatever the front end does with a background
 * death, and nothing here says more about it.
 *
 * **The fall is UPWARD first, not down.** -12.0 in 16.16 is a negative
 * velocity, the same sign the jump's -10.0 has, so the bell-tower death
 * launches him before gravity takes over -- and the gravity it adds each
 * frame, -0.406, is also negative: this pair keeps accelerating him
 * upward rather than pulling him back down, unlike every other flight in
 * this file. `shake_a11` runs with `field48` carrying an event id the way
 * the shake pair's own field28 does with `him` or the object's 0x08, so
 * 0x6000a names whatever camera or screen event a bell-tower fall
 * triggers.
 */
long t_brp1(MK3THREAD *thread);

long t_fall_down_bell_tower(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    MKEvent_Add(4, 0x3d, 0, (long)obj->field00->field08);

    obj->field1c = 9;
    group_sound(obj);

    clear_shadow_bit(obj);
    center_around_me(obj);

    obj->field48 = 0x6000a;
    shake_a11(obj);

    obj->field1c = 0;
    obj->field20 = 0xfff40000u;      /* -12.0 in 16.16 */
    obj->field24 = 0xfff40000u + 0xc6000u;
    obj->field28 = 5;
    obj->field40 = 5 + 0x19;         /* 30, SCKNOCKDOWN */
    obj->field34 = (uint32_t)(uintptr_t)t_pit_fall_scan;

    /* corrected: installs t_brp1; t_pit_fall_scan only goes in field34 */
    return mk3_install(thread, (MK3THREADFUNC)t_brp1);
}


long t_stumble_back_vel(struct MK3THREAD *thread);
long t_cc_block_avoid_corner(struct MK3THREAD *thread);
long t_block_shake_n_exit(struct MK3THREAD *thread);


/* ------------------------------------------------------- t_avoid_corner_trap_b
 *
 * armv7 0x00041c74, two hundred and four bytes.
 *
 *      state 0
 *          obj->field1c = obj->field00->p_hit
 *          thread->args[fieldf8++] = obj->field1c    ; save p_hit
 *          obj->field00->field4c = obj->field1c
 *          obj->field00->p_hit = obj->field1c
 *          push t_avoid_corner_trap                  (0x13ad)
 *      state 0x13ad
 *          --thread->fieldf8
 *          obj->field00->p_hit = thread->args[fieldf8]  ; restore
 *          pop
 *
 * **The B-suffix is a SAVE/RESTORE wrapper around its own A version.**
 * `t_avoid_corner_trap` reads and can change p_hit through the corner
 * check; this pushes it, lets the check run, and puts the original value
 * back afterwards -- so whatever the corner rule does to the hit count is
 * undone once this caller resumes. The two self-writes before the push
 * (field4c and p_hit both set to the same value just read) leave the count
 * unchanged; they exist because the save happens through field1c as a
 * relay rather than copying p_hit to p_hit directly.
 */
long t_avoid_corner_trap_b(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    uint32_t cur;

    if (token == 0) {
        obj->field1c = obj->field00->p_hit;

        cur = thread->fieldf8;
        *mk3_arg(thread, cur) = obj->field1c;
        thread->fieldf8 = cur + 1;

        obj->field00->field4c = obj->field1c;
        obj->field00->p_hit = obj->field1c;

        *mk3_frame(thread, thread->frame + 1) = 0x13ad;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_avoid_corner_trap;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x13ad)
        return -3;

    thread->fieldf8 = thread->fieldf8 - 1;
    obj->field1c = *mk3_arg(thread, thread->fieldf8);   /* str r1,[r2,#0x1c] */
    obj->field00->p_hit = obj->field1c;

    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
        return 0;
    }
    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* -------------------------------------------------------------- t_b_boss_hit1
 *
 * armv7 0x000436f4, two hundred and twelve bytes.
 *
 *      state 0        obj->field38/30/34 = 0
 *                     push t_blocked_start              (resume 0x1305)
 *      0x1305         rsnd_func(obj, 5)
 *                     obj->field1c = 0x60000 ; away_x_vel(obj)
 *                     obj->field48 = 0x60006 ; shake_a11(obj)
 *                     obj->field48 = 2 ; obj->a10 = 3 ; obj->field40 = 0xc
 *                     push t_block_shake                (resume 0x1311)
 *      0x1311         obj->field1c = 0x40000
 *                     install t_stumble_back_vel
 *
 * **Corrected, 2026-09-24.** The first transcription had the two resume
 * states the wrong way round, pushed t_stumble_back_vel where the binary
 * pushes t_block_shake, ended 0x1305 on a "pop, or t_local_reaction_exit"
 * the binary does not have, and wrote every field through `field00` where
 * the binary writes the object itself (`[thread->proc, #off]`). The
 * oracle's shared slack let all of it verify.
 *
 * A boss hit is a block followed by a shove: after the block, a knockback
 * sound, a hard push away (6.0) and a shake, with a short countdown in
 * field48/a10; after the shake, a slower 4.0 push into t_stumble_back_vel.
 */
long t_b_boss_hit1(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x1311) {
        obj->field1c = 0x40000;        /* 4.0 in 16.16 */
        return mk3_install(thread, (MK3THREADFUNC)t_stumble_back_vel);
    }

    if (token == 0x1305) {
        rsnd_func(obj, 5);

        obj->field1c = 0x60000;        /* 6.0 in 16.16 */
        away_x_vel(obj);

        obj->field48 = 0x60006;
        shake_a11(obj);

        obj->field48 = 2;
        obj->a10     = 2 + 1;
        obj->field40 = 2 + 1 + 9;

        *mk3_frame(thread, thread->frame + 1) = 0x1311;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_block_shake;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0)
        return -3;

    obj->field38 = 0;
    obj->field30 = 0;
    obj->field34 = 0;

    *mk3_frame(thread, thread->frame + 1) = 0x1305;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_blocked_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------------------- t_b_combo
 *
 * armv7 0x000435f8, a hundred and forty bytes.
 *
 *      state 0
 *          part->field30 = 0 ; part->field34 = 0
 *          part->field38 = t_cc_block_avoid_corner
 *          push t_blocked_start                       (0x133f)
 *      state 0x133f
 *          rsnd_func(obj, 5)
 *          part->field1c = 0x40000 ; away_x_vel(obj)
 *          part->field48 = 2 ; obj->p_hit = 3
 *          install t_block_shake_n_exit
 *
 * Blocks, hands the corner check to `t_cc_block_avoid_corner` through the
 * usual field38 slot, and on return pushes the victim away at 4.0 with a
 * short shake countdown before handing to the exit routine -- softer than
 * `t_b_boss_hit1`'s 6.0, which fits a combo hit rather than a boss hit.
 */
long t_b_combo(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field30 = 0;
        obj->field34 = 0;
        obj->field38 =
            (uint32_t)(uintptr_t)t_cc_block_avoid_corner;

        *mk3_frame(thread, thread->frame + 1) = 0x133f;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_blocked_start;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x133f)
        return -3;

    rsnd_func(obj, 5);

    obj->field1c = 0x40000;        /* 4.0 in 16.16 */
    away_x_vel(obj);

    obj->field48 = 2;
    obj->a10 = 2 + 1;

    return mk3_install(thread, (MK3THREADFUNC)t_block_shake_n_exit);
}


/* -------------------------------------------------------------- t_b_combo_hard
 *
 * armv7 0x00043548, a hundred and sixty bytes.
 *
 *      state 0
 *          part->field48 = 0x40004 ; shake_a11(obj)
 *          part->field30 = 0 ; part->field34 = 0
 *          part->field38 = t_cc_block_avoid_corner
 *          push t_blocked_start                       (0x1350)
 *      state 0x1350
 *          rsnd_func(obj, 5)
 *          part->field1c = 0x50000 ; away_x_vel(obj)
 *          obj->p_hit = 4 ; part->field48 = 3
 *          install t_block_shake_n_exit
 *
 * The same shape as `t_b_combo` with three numbers changed: a shake BEFORE
 * the block starts (0x40004, its own event id), a push of 5.0 rather than
 * 4.0, and a longer countdown (p_hit/field48 = 4/3 rather than 3/2). "Hard"
 * is exactly these three increments over the plain combo block.
 */
long t_b_combo_hard(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field48 = 0x40004;
        shake_a11(obj);

        obj->field30 = 0;
        obj->field34 = 0;
        obj->field38 =
            (uint32_t)(uintptr_t)t_cc_block_avoid_corner;

        *mk3_frame(thread, thread->frame + 1) = 0x1350;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_blocked_start;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x1350)
        return -3;

    rsnd_func(obj, 5);

    obj->field1c = 0x50000;        /* 5.0 in 16.16 */
    away_x_vel(obj);

    obj->a10 = 4;
    obj->field48 = 4 - 1;

    return mk3_install(thread, (MK3THREADFUNC)t_block_shake_n_exit);
}


/* --------------------------------------------------------- t_b_duck_hit_hard
 *
 * armv7 0x000433f8, a hundred and sixty-eight bytes.
 *
 *      state 0
 *          part->field34 = 0
 *          part->field30 = t_generic_airborn_hit
 *          part->field38 = t_cc_block_avoid_corner
 *          push t_blocked_start                       (0x136f)
 *      state 0x136f
 *          rsnd_func(obj, 5)
 *          part->field1c = 0x50000 ; away_x_vel(obj)
 *          part->field48 = 2 ; obj->p_hit = 3
 *          install t_block_shake_n_exit
 *
 * A duck-block whose walk-routine slot (field30) is set to
 * `t_generic_airborn_hit` rather than left at 0 the way the standing combo
 * blocks leave it. That parks a routine for the object's own update to run
 * instead of walking, the same handover `t_r_last_noogy` uses -- so a hard
 * hit while ducking substitutes airborne-hit handling underneath the block,
 * even though the fighter never leaves the ground here.
 */
long t_b_duck_hit_hard(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field34 = 0;
        obj->field30 = (uint32_t)(uintptr_t)t_generic_airborn_hit;
        obj->field38 =
            (uint32_t)(uintptr_t)t_cc_block_avoid_corner;

        *mk3_frame(thread, thread->frame + 1) = 0x136f;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_blocked_start;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x136f)
        return -3;

    rsnd_func(obj, 5);

    obj->field1c = 0x50000;        /* 5.0 in 16.16 */
    away_x_vel(obj);

    obj->field48 = 2;
    obj->a10 = 2 + 1;

    return mk3_install(thread, (MK3THREADFUNC)t_block_shake_n_exit);
}


/* --------------------------------------------------------- t_b_duck_hit_soft
 *
 * armv7 0x000434a0, a hundred and sixty-eight bytes.
 *
 *      state 0
 *          part->field34 = 0
 *          part->field30 = t_generic_airborn_hit
 *          part->field38 = t_cc_block_avoid_corner
 *          push t_blocked_start                       (0x135f)
 *      state 0x135f
 *          rsnd_func(obj, 6)
 *          part->field1c = 0x20000 ; away_x_vel(obj)
 *          part->field48 = 2 ; obj->p_hit = 3
 *          install t_block_shake_n_exit
 *
 * `t_b_duck_hit_hard` with the sound and the push changed -- 6 instead of 5,
 * 2.0 instead of 5.0 -- and nothing else. Same substitution of
 * `t_generic_airborn_hit` into the walk slot.
 */
long t_b_duck_hit_soft(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field34 = 0;
        obj->field30 = (uint32_t)(uintptr_t)t_generic_airborn_hit;
        obj->field38 =
            (uint32_t)(uintptr_t)t_cc_block_avoid_corner;

        *mk3_frame(thread, thread->frame + 1) = 0x135f;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_blocked_start;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x135f)
        return -3;

    rsnd_func(obj, 6);

    obj->field1c = 0x20000;        /* 2.0 in 16.16 */
    away_x_vel(obj);

    obj->field48 = 2;
    obj->a10 = 2 + 1;

    return mk3_install(thread, (MK3THREADFUNC)t_block_shake_n_exit);
}


/* -------------------------------------------------------------- t_b_lo_punch
 *
 * armv7 0x0004496c, a hundred and eighty-four bytes.
 *
 *      state 0
 *          rsnd_func(obj, 6)
 *          part->field30 = 0 ; part->field34 = 0
 *          part->field38 = t_cc_punch
 *          push t_blocked_start                       (0x1386)
 *      state 0x1386
 *          obj->field54 = 0x50000                     ; 5.0
 *          am_i_short(obj)
 *          if (obj->field5c == 0)
 *              obj->field54 = 0 + 0x30000             ; 3.0 standing
 *          obj->field1c = obj->field54 ; away_x_vel(obj)
 *          obj->field48 = 2 ; obj->p_hit = 3
 *          install t_block_shake_n_exit
 *
 * **The push is 5.0 crouched, 3.0 standing.** The add at 0x44a10 is
 * `add r3, r3, #0x30000` with r3 still holding field5c, which is zero on that
 * path -- so it does not add to the 5.0, it replaces it with 3.0. An earlier
 * reading took it as `field54 += 3.0` (8.0 standing); the differential test
 * showed the binary leaves 3.0.
 *
 * This time the corner handover in field38 is `t_cc_punch` rather than
 * `t_cc_block_avoid_corner` -- the pause-and-pop routine, not the corner
 * check -- so a blocked low punch does not get the avoid-corner treatment
 * at all.
 */
long t_b_lo_punch(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        rsnd_func(obj, 6);

        obj->field30 = 0;
        obj->field34 = 0;
        obj->field38 = (uint32_t)(uintptr_t)t_cc_punch;

        *mk3_frame(thread, thread->frame + 1) = 0x1386;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_blocked_start;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x1386)
        return -3;

    obj->field54 = 0x50000;        /* 5.0 in 16.16 */
    am_i_short(obj);
    if (obj->field5c == 0)
        /* add r3,r3,#0x30000 with r3 = field5c, which is 0 here: the speed
         * becomes 3.0, it is not 5.0 + 3.0 (difftest) */
        obj->field54 = obj->field5c + 0x30000;

    obj->field1c = obj->field54;
    away_x_vel(obj);

    obj->field48 = 2;
    obj->a10 = 2 + 1;

    return mk3_install(thread, (MK3THREADFUNC)t_block_shake_n_exit);
}


/* ----------------------------------------------------------------- t_b_punch
 *
 * armv7 0x00043354, a hundred and sixty-four bytes.
 *
 *      state 0
 *          rsnd_func(obj, 6)
 *          part->field30 = 0 ; part->field34 = 0
 *          part->field38 = t_cc_block_avoid_corner
 *          push t_blocked_start                       (0x139c)
 *      state 0x139c
 *          part->field1c = 0x20000 ; away_x_vel(obj)
 *          part->field48 = 2 ; obj->p_hit = 3
 *          install t_block_shake_n_exit
 *
 * The plain high-punch block: a fixed 2.0 push, no stance question, and the
 * ordinary corner handover. This is the baseline every other block above
 * varies from -- `t_b_combo` doubles the push to 4.0, `t_b_boss_hit1`
 * triples it to 6.0, `t_b_lo_punch` conditions it on stance.
 */
long t_b_punch(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        rsnd_func(obj, 6);

        obj->field30 = 0;
        obj->field34 = 0;
        obj->field38 =
            (uint32_t)(uintptr_t)t_cc_punch;   /* corrected: was t_cc_block_avoid_corner */

        *mk3_frame(thread, thread->frame + 1) = 0x139c;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_blocked_start;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x139c)
        return -3;

    obj->field1c = 0x20000;        /* 2.0 in 16.16 */
    away_x_vel(obj);

    obj->field48 = 2;
    obj->a10 = 2 + 1;

    return mk3_install(thread, (MK3THREADFUNC)t_block_shake_n_exit);
}


long t_cc_block_upcut(struct MK3THREAD *thread);
long t_jump_up_land_jsrp(struct MK3THREAD *thread);
void ground_ochar(MK3OBJ *obj);
long is_stick_away(MK3OBJ *obj);


/* -------------------------------------------------------------------- t_b_uppercut
 *
 * armv7 0x000437c8, a hundred and seventy-six bytes.
 *
 *      state 0
 *          rsnd_func(obj, 5)
 *          part->field48 = 0x40004 ; shake_a11(obj)
 *          part->field30 = 0 ; part->field34 = 0
 *          part->field38 = t_cc_block_upcut
 *          push t_blocked_start                       (0x12f6)
 *      state 0x12f6
 *          part->field1c = 0x40000 ; away_x_vel(obj)
 *          part->field48 = 2 ; obj->p_hit = 2 + 2
 *          install t_block_shake_n_exit
 *
 * Blocking an uppercut shakes first, like `t_b_combo_hard`, and hands its
 * corner check to `t_cc_block_upcut` -- a third variant of the handover
 * `t_cc_block_avoid_corner` and `t_cc_punch` already cover, specific to
 * this move. The push is 4.0 and the countdown 4, in between the plain
 * punch block's 2.0/3 and the boss hit's 6.0.
 */
long t_b_uppercut(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        rsnd_func(obj, 5);

        obj->field48 = 0x40004;
        shake_a11(obj);

        obj->field30 = 0;
        obj->field34 = 0;
        obj->field38 = (uint32_t)(uintptr_t)t_cc_block_upcut;

        *mk3_frame(thread, thread->frame + 1) = 0x12f6;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_blocked_start;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x12f6)
        return -3;

    obj->field1c = 0x40000;        /* 4.0 in 16.16 */
    away_x_vel(obj);

    obj->field48 = 2;
    obj->a10 = 2 + 2;

    return mk3_install(thread, (MK3THREADFUNC)t_block_shake_n_exit);
}


/* --------------------------------------------------------------- t_block_shake
 *
 * armv7 0x00044750, two hundred and eight bytes.
 *
 *      state 0
 *          get_char_ani(obj) ; obj->field40 += 4
 *          token = 0x1440 ; push t_block_shake_ani
 *      state 0x1440
 *          token = 0x1441 ; push t_block_shake_ani
 *      state 0x1441
 *          obj->field40 -= 8
 *          if (--obj->a10 != 0)
 *              token = 0x1440 ; push t_block_shake_ani     ; loop
 *          else
 *              stop_me_player(obj) ; pop
 *      otherwise refuse
 *
 * **The animation runs BACKWARDS, two words at a time.** `get_char_ani` and
 * one advance of 4 set up the clip's tail; every pass after that pulls the
 * cursor back 8, twice the step it moved forward, so the net motion per
 * loop is backward. `t_block_shake_ani` -- see its own banner -- plays one
 * frame and parks for whatever `obj->a10` holds, and THIS routine is what
 * decrements a10 and decides whether to loop or stop.
 *
 * So the shake is: play a frame going backward through the clip, wait,
 * repeat, for `a10` frames, then a full stop. State 0 and state 0x1440 push
 * the same child for the same reason -- 0 does the one-time setup first and
 * 0x1440 is the bare re-entry the loop uses afterward.
 */
long t_block_shake(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        get_char_ani(obj);
        obj->field40 = obj->field40 + 4;

        *mk3_frame(thread, thread->frame + 1) = 0x1440;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_block_shake_ani;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x1440) {
        *mk3_frame(thread, thread->frame + 1) = 0x1441;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_block_shake_ani;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x1441)
        return -3;

    obj->field40 = obj->field40 - 8;
    obj->a10 = obj->a10 - 1;
    if (obj->a10 != 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x1440;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_block_shake_ani;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    stop_me_player(obj);

    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
        return 0;
    }
    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* --------------------------------------------------------- t_back_to_the_fight
 *
 * armv7 0x00047bc4, two hundred and ninety-six bytes.
 *
 *      state 0
 *          MK3_SET_FIELD12(obj->field08, G[0xac] + 0x32)
 *          obj->field40 = 0x16 ; get_char_ani(obj)
 *          obj->field40 += 4 ; do_next_a9_frame(obj)
 *          face_opponent(obj)
 *          obj->field08->field20 = 0
 *          obj->field1c = obj->field08->field1c = 0xfff40000     ; -12.0
 *          token = 0xbb0 ; thread->fieldfc = 1 ; return 1
 *      state 0xbb0
 *          if ((int16_t)MK3_FIELD12(obj->field08) > obj->field00->field40)
 *              sleep again at 0xbb0
 *          obj->field1c = 0
 *          obj->field20 = obj->field08->field1c    ; -12.0, carried over
 *          obj->field24 = 0x10000 ; obj->field28 = 0xfff
 *          push t_flight                            (0xbb8)
 *      state 0xbb8
 *          ground_ochar(obj)
 *          MKEvent_Add(4, 0x38, 0, obj->field00->field08)
 *          install t_local_reaction_exit
 *          push t_jump_up_land_jsrp
 *
 * **The reverse of `t_fall_down_bell_tower`.** That launches upward at
 * -12.0; this waits for the SAME fighter to come back down -- the height
 * test compares `MK3_FIELD12` against the floor exactly the way
 * `t_air_strike` and `t_slammed_slam_down` do -- and once he is down it
 * grounds him, fires a different event (4/0x38, "back to the fight" rather
 * than the fall's 4/0x3d), replaces itself with `t_local_reaction_exit`,
 * and pushes `t_jump_up_land_jsrp` on top of that. So the routine that
 * finishes the landing outlives this one: when it eventually pops, it
 * returns into the exit rather than back here.
 *
 * `G[0xac] + 0x32` written into the OTHER object's field12 is a countdown
 * or a timestamp of some kind that this file does not otherwise explain;
 * it is transcribed as read and not guessed at further.
 */
long t_back_to_the_fight(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0xbb0) {
        if ((int32_t)(int16_t)MK3_FIELD12(obj->field08)
            > (long)obj->field00->field40) {
            *mk3_frame(thread, thread->frame + 1) = 0xbb0;
            thread->fieldfc = 1;
            return 1;
        }

        obj->field1c = 0;
        obj->field20 = obj->field08->field1c;
        obj->field24 = 0x10000;         /* 1.0 in 16.16 */
        obj->field28 = 0xfff;

        *mk3_frame(thread, thread->frame + 1) = 0xbb8;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0xbb8) {
        ground_ochar(obj);
        MKEvent_Add(4, 0x38, 0, (long)obj->field00->field08);

        mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_jump_up_land_jsrp;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0)
        return -3;

    MK3_SET_FIELD12(obj->field08,
                    (uint32_t)(*(const uint32_t *)(const void *)(G_BYTES
                                                                 + 0xac)
                              + 0x32));

    obj->field40 = 0x16;
    get_char_ani(obj);
    obj->field40 = obj->field40 + 4;
    do_next_a9_frame(obj);

    face_opponent(obj);

    obj->field08->field20 = 0;

    obj->field1c = 0xfff40000u;              /* -12.0 in 16.16 */
    obj->field08->field1c = obj->field1c;

    *mk3_frame(thread, thread->frame + 1) = 0xbb0;
    thread->fieldfc = 1;
    return 1;
}


long t_flight_call(struct MK3THREAD *thread);
void find_ani_part2(MK3OBJ *obj);
void death_scream(MK3OBJ *obj);
void death_blow_complete(MK3OBJ *obj);
extern long *RoundParam;                /* pointer slot -> 0x0038ed04 */


/* ------------------------------------------------------------------- t_brp1
 *
 * armv7 0x000483a0, two hundred and sixteen bytes.
 *
 *      state 0
 *          token = 0x7e0 ; push t_flight_call
 *      state 0x7e0
 *          tsound_func(obj, 0xb) ; stop_me_player(obj)
 *          MKEvent_Add(4, 0x3f, 0, obj->field00->field08)
 *          rsnd_func(obj, 3) ; death_scream(obj)
 *          obj->field40 = 0x1e ; find_ani_part2(obj) ; do_next_a9_frame(obj)
 *          obj->field1c = 0xa ; create_blood_proc(obj)
 *          death_blow_complete(obj)
 *          token = 0x7ed ; thread->fieldfc = 0xa ; return 0xa
 *      state 0x7ed
 *          install t_local_reaction_exit
 *
 * A death that flies before it finishes: `t_flight_call` is let run first,
 * and only once it pops back does this play the scream, the blood and
 * `death_blow_complete` -- all of it timed to the SAME animation 30
 * (SCKNOCKDOWN) every hard fall in this file lands on. Ten frames after that
 * it ends for good through `t_local_reaction_exit`, which this installs
 * directly rather than popping to, so there is nothing left above it.
 */
long t_brp1(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x7e0;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_flight_call;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x7ed)   /* corrected: t_wait_forever (slot 0x000f3724) */
        return mk3_install(thread, (MK3THREADFUNC)t_wait_forever);

    if (token != 0x7e0)
        return -3;

    tsound_func(obj, 0xb);
    stop_me_player(obj);

    MKEvent_Add(4, 0x3f, 0, (long)obj->field00->field08);

    rsnd_func(obj, 3);
    death_scream(obj);

    obj->field40 = 0x1e;             /* 30, SCKNOCKDOWN */
    find_ani_part2(obj);
    do_next_a9_frame(obj);

    obj->field1c = 0xa;
    create_blood_proc(obj);

    death_blow_complete(obj);

    *mk3_frame(thread, thread->frame + 1) = 0x7ed;
    thread->fieldfc = 0xa;
    return 0xa;
}


/* --------------------------------------------------------- t_blast_through_anything
 *
 * armv7 0x00047cec, five hundred and four bytes -- the largest function in
 * this file so far.
 *
 *      state 0
 *          MKEvent_Add(4, 0x36, 0, obj->field00->field08)
 *          obj->field38 = t_ken_masters_xfer ; xfer_otherguy(obj)
 *          obj->field1c = 0x10000 ; away_x_vel(obj)             ; 1.0
 *          obj->field24 = 0x5000                                ; 0.3125
 *          obj->field20 = 0xffe70000                            ; -25.0
 *          obj->field08->field1c = obj->field20
 *          obj->field08->field20 = obj->field24
 *          obj->field40 = 0x1e ; get_char_ani(obj)               ; SCKNOCKDOWN
 *          obj->field1c = 0xf ; init_anirate(obj)
 *          token = 0xb6b ; thread->fieldfc = 1 ; return 1
 *      state 0xb6b
 *          next_anirate(obj)
 *          obj->field1c = obj->field08->field1c
 *          if (obj->field1c < 0) sleep again at 0xb6b
 *          RoundParam[2] and the other fighter's proc.field40 both lose
 *              RoundParam[2] worth of value, RoundParam[2] itself cleared:
 *                  obj->field00->field40 -= RoundParam[2]
 *                  otherObj->field00->field40 -= RoundParam[2]
 *                  RoundParam->0x30 (byte) = 0
 *                  G->field0xac -= RoundParam[2]   ; before clearing it
 *                  RoundParam[2] = 0
 *          MKEvent_Add(4, 0x37, 0, obj->field00->field08)
 *          obj->field1c = 0xd ; obj->field20 = 0xd  ; sentinels, leave alone
 *          obj->field24 = 0x8000 ; obj->field28 = 5              ; 0.5, count 5
 *          token = 0xb82 ; push t_flight
 *      state 0xb82
 *          install t_getup_reaction_exit
 *      state 0xb90
 *          install t_getup_reaction_exit
 *      state 0
 *          (also reached from `beq 0` inside the low branch, but the
 *           low-branch chooses between token 0 and 0xb6b by an explicit
 *           `cmp #0` first)
 *      state 0xb8e
 *          ground_ochar(obj)
 *          obj->field40 = (int16_t)MK3_FIELD12(obj->field08)
 *          obj->field38 = t_back_to_the_fight ; xfer_otherguy(obj)
 *          shake_n_sound(obj)
 *          obj->field40 = 0x1e ; find_ani_part2(obj)
 *          obj->field1c = 4
 *          token = 0xb90 ; thread->fieldfc = 0x20 ; return 0x20
 *
 * **A blast that both fighters pay for.** The launch is symmetric -- both
 * fighters get the same -25.0/0.3125 velocity and gravity written into
 * field1c/field20, one directly and one through obj->field08 -- and the
 * RoundParam subtraction removes the SAME stored value from both sides'
 * proc.field40 before clearing it, which reads as undoing whatever
 * RoundParam[2] had been contributing to each of them individually.
 *
 * The five states are two flights back to back: `t_flight_call` for the
 * initial launch (0xb6b's own frame-by-frame descent, not pushed -- it
 * calls `next_anirate` itself each pass), then once RoundParam's value is
 * cleared, a SECOND flight through `t_flight` at 0.5 gravity for five
 * ticks (0xb82). Landing installs `t_getup_reaction_exit` twice over --
 * once directly from 0xb82 and again from 0xb90, the second of which is
 * reached from 0xb8e's own separate wait rather than from 0xb82's push,
 * so the two are alternate endings rather than one calling the other.
 *
 * `RoundParam[2]` is `RoundParam + 0x08`, and `RoundParam->0x30` a byte
 * inside the same block -- both already established as a `long*` reached
 * through a pointer slot; nothing further about what index 2 or byte 0x30
 * mean is claimed here.
 */
long t_blast_through_anything(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    /* Corrected, 2026-09-26: 0xb82 (the landing) does the grounding and
     * pushes t_mframew; 0xb8e only waits. The first transcription put the
     * grounding under 0xb8e, skipped the t_mframew run, and wrote the first
     * field40 to the object where the binary writes the header's. */
    if (token == 0xb82) {
        ground_ochar(obj);

        obj->field00->field40 =
            (uint32_t)(int32_t)(int16_t)MK3_FIELD12(obj->field08);   /* ldrsh: sign-extended (difftest) */
        obj->field38 = (uint32_t)(uintptr_t)t_back_to_the_fight;
        xfer_otherguy(obj);

        shake_n_sound(obj);

        obj->field40 = 0x1e;         /* 30, SCKNOCKDOWN */
        find_ani_part2(obj);

        obj->field1c = 4;

        *mk3_frame(thread, thread->frame + 1) = 0xb8e;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0xb8e) {
        *mk3_frame(thread, thread->frame + 1) = 0xb90;
        thread->fieldfc = 0x20;
        return 0x20;
    }

    if (token == 0xb90)
        return mk3_install(thread, (MK3THREADFUNC)t_getup_reaction_exit);

    if (token != 0 && token != 0xb6b)
        return -3;

    if (token == 0xb6b) {
        next_anirate(obj);

        obj->field1c = obj->field08->field1c;
        if ((long)obj->field1c < 0) {
            *mk3_frame(thread, thread->frame + 1) = 0xb6b;
            thread->fieldfc = 1;
            return 1;
        }

        obj->field00->field40 = obj->field00->field40 - RoundParam[2];
        obj->field00->field00->field00->field40 =
            obj->field00->field00->field00->field40 - RoundParam[2];
        *(uint8_t *)((char *)RoundParam + 0x30) = 0;
        *(uint32_t *)(void *)(G_BYTES + 0xac) =
            *(const uint32_t *)(const void *)(G_BYTES + 0xac) - RoundParam[2];
        RoundParam[2] = 0;

        MKEvent_Add(4, 0x37, 0, (long)obj->field00->field08);

        obj->field1c = 0xd;               /* sentinel: leave alone */
        obj->field20 = 0xd;               /* sentinel: leave alone */
        obj->field24 = 0x8000;            /* 0.5 in 16.16 */
        obj->field28 = 5;

        *mk3_frame(thread, thread->frame + 1) = 0xb82;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    MKEvent_Add(4, 0x36, 0, (long)obj->field00->field08);

    /* pointer slot 0xf3724 holds t_wait_forever (read from the binary;
     * difftest) */
    obj->field38 = (uint32_t)(uintptr_t)t_wait_forever;
    xfer_otherguy(obj);

    obj->field1c = 0x10000;           /* 1.0 in 16.16 */
    away_x_vel(obj);

    obj->field24 = 0x5000;            /* 0.3125 in 16.16 */
    obj->field20 = 0xffe70000u;       /* -25.0 in 16.16 */
    obj->field08->field1c = obj->field20;
    obj->field08->field20 = obj->field24;

    obj->field40 = 0x1e;              /* 30, SCKNOCKDOWN */
    get_char_ani(obj);

    obj->field1c = 0xf;
    init_anirate(obj);

    *mk3_frame(thread, thread->frame + 1) = 0xb6b;
    thread->fieldfc = 1;
    return 1;
}


long t_wait_forever(struct MK3THREAD *thread);
long t_separate_us(struct MK3THREAD *thread);


/* -------------------------------------------------------------------- t_ccp3
 *
 * armv7 0x000477bc, three hundred and fifty-two bytes.
 *
 *      state 0 only
 *          am_i_joy(obj)
 *          if (obj->field5c) {
 *              is_stick_away(obj)
 *              if (!obj->field5c) obj->field34 = obj->field30
 *          }
 *          obj->field1c = obj->field00->field4c
 *          if (obj->field1c < obj->field34)
 *              pop one level, plain
 *          else
 *              pop-erase three times over, each restoring the level below
 *              before the next, and land on `t_separate_us`
 *
 * **The corner check reads differently for a human and the machine.** A
 * human's answer comes from `is_stick_away` -- if he is not holding back,
 * `field34` is overwritten from `field30` before the comparison runs at
 * all. The AI skips straight to the comparison with whatever `field34`
 * already holds. Either way the same test decides the outcome:
 * `p_hit`'s mirror at field4c against field34, the identical shape
 * `t_avoid_corner_trap` uses for its own hit-count threshold.
 *
 * **Below the threshold, it is one ordinary pop.** At or past it, the
 * routine erases itself from the call stack THREE LEVELS DEEP -- the same
 * below/carried shuffle `t_knee_check` and `t_elbow_check` use once each,
 * chained three times in a row -- and only the last of the three lands on
 * a real destination, `t_separate_us`; the first two erasures land on
 * `t_local_reaction_exit` only if the stack runs out early, which is the
 * same safety floor every one-shot pop in this file falls back to.
 */
long t_ccp3(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t below, carried;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    am_i_joy(obj);
    if (obj->field5c != 0) {
        is_stick_away(obj);
        if (obj->field5c == 0)
            obj->field34 = obj->field30;
    }

    obj->field1c = obj->field00->field4c;
    if ((long)obj->field1c < (long)obj->field34) {
        if ((long)thread->frame > 0) {
            thread->frame = thread->frame - 1;
            return 0;
        }
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }

    /* First erasure. */
    /* at the bottom the exit goes in first and the slide still runs (difftest) */
    if ((long)thread->frame <= 0) {
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_local_reaction_exit;
        *mk3_frame(thread, thread->frame + 1) = 0;
    } else {
        thread->frame = thread->frame - 1;
    }
    below   = mk3_frame(thread, thread->frame + 1)[1];
    carried = *mk3_frame(thread, thread->frame + 2);
    *mk3_frame(thread, thread->frame + 1) = carried;
    mk3_frame(thread, thread->frame)[1] = below;

    /* Second erasure. */
    /* at the bottom the exit goes in first and the slide still runs (difftest) */
    if ((long)thread->frame <= 0) {
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_local_reaction_exit;
        *mk3_frame(thread, thread->frame + 1) = 0;
    } else {
        thread->frame = thread->frame - 1;
    }
    below   = mk3_frame(thread, thread->frame + 1)[1];
    carried = *mk3_frame(thread, thread->frame + 2);
    *mk3_frame(thread, thread->frame + 1) = carried;
    mk3_frame(thread, thread->frame)[1] = below;

    /* Third erasure, landing on t_separate_us. */
    /* at the bottom the exit goes in first and the slide still runs (difftest) */
    if ((long)thread->frame <= 0) {
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_local_reaction_exit;
        *mk3_frame(thread, thread->frame + 1) = 0;
    } else {
        thread->frame = thread->frame - 1;
    }
    below   = mk3_frame(thread, thread->frame + 1)[1];
    carried = *mk3_frame(thread, thread->frame + 2);
    *mk3_frame(thread, thread->frame + 1) = carried;

    return mk3_install(thread, (MK3THREADFUNC)t_separate_us);
}


/* -------------------------------------------------------------- t_check_stay_down
 *
 * armv7 0x00042004, two hundred and twenty-four bytes.
 *
 *      state 0 only
 *          obj->field2c = obj->field00->field10
 *          if (obj->field2c & 0x40) {
 *              pop-erase once, install t_wait_forever
 *          } else {
 *              obj->field1c = (int16_t)G[0x45c]
 *              if (raw == 0 || obj->field1c == 3) goto plain-pop
 *              obj->field20 = obj->field00->field08 + 1
 *              if (obj->field20 == obj->field1c) goto plain-pop
 *              -- otherwise, treat it as the 0x40-bit-set case: pop-erase
 *                 once, install t_wait_forever
 *          }
 *      plain-pop: one ordinary pop
 *
 * **The stay-down bit overrides the round status.** If bit 6 of the
 * header's field10 is already up, this erases itself and installs
 * `t_wait_forever` without even looking at the round -- that bit alone is
 * enough to keep a fighter down. Otherwise it reads `G[0x45c]`, the same
 * status word `t_check_winner_status` reads, and only when the round is
 * live (nonzero) AND not finished (not 3) AND the strength index plus one
 * matches does it fall through to a plain pop; every other combination --
 * including the round having just ended -- routes into the SAME
 * erase-and-wait as the explicit stay-down bit.
 */
long t_check_stay_down(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint16_t raw;
    uint32_t below, carried;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field2c = obj->field00->field10;

    if ((obj->field2c & 0x40) == 0) {
        raw = *(const uint16_t *)(const void *)(G_BYTES + 0x45c);
        obj->field1c = (uint32_t)(int32_t)(int16_t)raw;

        if (raw != 0 && (int32_t)obj->field1c != 3) {
            obj->field20 = obj->field00->field08 + 1;
            if (obj->field20 == obj->field1c)
                goto plain_pop;
        } else {
            goto plain_pop;
        }
    }

    /* at the bottom the exit goes in first and the slide still runs (difftest) */
    if ((long)thread->frame <= 0) {
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_local_reaction_exit;
        *mk3_frame(thread, thread->frame + 1) = 0;
    } else {
        thread->frame = thread->frame - 1;
    }
    below   = mk3_frame(thread, thread->frame + 1)[1];
    carried = *mk3_frame(thread, thread->frame + 2);
    *mk3_frame(thread, thread->frame + 1) = carried;
    mk3_frame(thread, thread->frame)[1] = below;

    return mk3_install(thread, (MK3THREADFUNC)t_wait_forever);

plain_pop:
    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
        return 0;
    }
    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* ----------------------------------------------------------------- t_combo1
 *
 * armv7 0x00045280, two hundred bytes.
 *
 *      state 0
 *          obj->field1c = 4 ; create_blood_proc(obj)
 *          obj->field1c = 0x20000 ; away_x_vel(obj)              ; 2.0
 *          obj->field40 = 0x1c ; get_char_ani(obj)
 *          obj->field44 = obj->field40 ; obj->field40 += 0xc
 *          do_next_a9_frame(obj)
 *          token = 0xc77 ; thread->fieldfc = 3 ; return 3
 *      state 0xc77
 *          obj->field40 = obj->field44 ; obj->field1c = 4
 *          push t_mframew                             (0xc7a)
 *      state 0xc7a
 *          install t_local_reaction_exit
 *
 * `a10` -- the argument slot, not p_hit; this function works on `obj`
 * directly rather than through `obj->field00` -- parks `field40`'s value
 * across `do_next_a9_frame`'s three-tick wait and restores it afterward.
 * The same slot other functions in this file use for a countdown is used
 * here to carry a single animation-cursor value instead.
 */
long t_combo1(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field1c = 4;
        create_blood_proc(obj);

        obj->field1c = 0x20000;         /* 2.0 in 16.16 */
        away_x_vel(obj);

        obj->field40 = 0x1c;
        get_char_ani(obj);

        obj->a10 = obj->field40;
        obj->field40 = obj->field40 + 0xc;
        do_next_a9_frame(obj);

        *mk3_frame(thread, thread->frame + 1) = 0xc77;
        thread->fieldfc = 3;
        return 3;
    }

    if (token == 0xc77) {
        obj->field40 = obj->a10;
        obj->field1c = 4;

        *mk3_frame(thread, thread->frame + 1) = 0xc7a;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0xc7a)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


long t_land_on_my_back(struct MK3THREAD *thread);
void set_no_block(MK3OBJ *obj);
void create_fx(MK3OBJ *obj);


/* ---------------------------------------------------------------- t_combo43
 *
 * armv7 0x00045ddc, two hundred and forty-eight bytes.
 *
 *      state 0
 *          obj->field30 = 0 ; obj->field34 = 0 ; obj->field38 = 9
 *          push t_reaction_start                      (0xce8)
 *      state 0xce8
 *          set_no_block(obj)
 *          obj->field1c = 4 ; create_blood_proc(obj)
 *          obj->field1c = 2 ; group_sound(obj)
 *          rsnd_func(obj, 0xa)
 *          obj->field1c = 0xe ; create_fx(obj)
 *          obj->field48 = 0xa000a ; shake_a11(obj)
 *          obj->field1c = 0xa0000                       ; 10.0
 *          obj->field20 = obj->field1c - 0x120000        ; -8.0
 *          obj->field24 = obj->field20 + 0x88000         ; 0.53125
 *          obj->field28 = 5 ; obj->field40 = 5 + 0x19    ; 30, SCKNOCKDOWN
 *          push t_flight                                (0xcfb)
 *      state 0xcfb
 *          install t_land_on_my_back
 *
 * Sets `field38 = 9` before the reaction even starts -- a plain word, not
 * a handler, unlike every other reaction that parks a routine there. This
 * combo finisher then blocks blocking outright (`set_no_block`), sprays
 * blood and an effect, shakes, and launches on the same 10.0/-8.0 pair
 * (net -8.0 velocity against a small positive `field24`) that lands on
 * animation 30 -- the knockdown every hard fall in this file shares --
 * before handing the landing to `t_land_on_my_back` once the flight ends.
 */
long t_combo43(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field30 = 0;
        obj->field38 = 0;
        obj->field34 = 9;               /* corrected: 9 goes to +0x34, not +0x38 */

        *mk3_frame(thread, thread->frame + 1) = 0xce8;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_reaction_start;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0xcfb)
        return mk3_install(thread, (MK3THREADFUNC)t_land_on_my_back);

    if (token != 0xce8)
        return -3;

    set_no_block(obj);

    obj->field1c = 4;
    create_blood_proc(obj);

    obj->field1c = 2;
    group_sound(obj);

    rsnd_func(obj, 0xa);

    obj->field1c = 0xe;
    create_fx(obj);

    obj->field48 = 0xa000a;
    shake_a11(obj);

    obj->field1c = 0xa0000;                     /* 10.0 in 16.16 */
    obj->field20 = obj->field1c - 0x120000;     /* -8.0 */
    obj->field24 = obj->field20 + 0x88000;      /* 0.53125 */
    obj->field28 = 5;
    obj->field40 = 5 + 0x19;                    /* 30, SCKNOCKDOWN */

    *mk3_frame(thread, thread->frame + 1) = 0xcfb;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_flight;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------- t_death_slam_pause
 *
 * armv7 0x00049a64, two hundred and four bytes.
 *
 *      state 0
 *          death_scream(obj)
 *          obj->field40 = 0x1e ; find_ani_part2(obj) ; find_last_frame(obj)
 *          do_next_a9_frame(obj)
 *          save obj->field48 on the arg ring
 *          obj->field48 = 0x80005 ; shake_a11(obj)
 *          restore obj->field48 from the ring
 *          tsound_func(obj, 0x81)
 *          token = 0x1e4 ; thread->fieldfc = 3 ; return 3
 *      state 0x1e4
 *          pose_stumble_frame_1(obj) ; pop
 *
 * **field48 is borrowed and given back in the same breath.** It is saved
 * on `thread->args` before `shake_a11` needs a fresh event id there and
 * restored the instruction after the call returns -- the shortest save
 * this file makes, one call wide, rather than spanning a push the way
 * `t_reaction_start` spans its child.
 */
long t_death_slam_pause(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    uint32_t cur, saved;

    if (token == 0) {
        death_scream(obj);

        obj->field40 = 0x1e;
        find_ani_part2(obj);
        find_last_frame(obj);
        do_next_a9_frame(obj);

        cur = thread->fieldf8;
        saved = obj->field48;
        *mk3_arg(thread, cur) = saved;
        thread->fieldf8 = cur + 1;

        obj->field48 = 0x80005;
        shake_a11(obj);

        thread->fieldf8 = thread->fieldf8 - 1;
        obj->field48 = *mk3_arg(thread, thread->fieldf8);

        tsound_func(obj, 0x81);

        *mk3_frame(thread, thread->frame + 1) = 0x1e4;
        thread->fieldfc = 3;
        return 3;
    }

    if (token != 0x1e4)
        return -3;

    pose_stumble_frame_1(obj);

    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
        return 0;
    }
    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


long t_animate_a0_frames(struct MK3THREAD *thread);
long t_d_beware(struct MK3THREAD *thread);
long t_d_beware_mframew(struct MK3THREAD *thread);


/* ------------------------------------------------------------ t_dizzy_by_boss
 *
 * armv7 0x00043f18, a hundred and fifty-two bytes.
 *
 *      state 0
 *          part->field18 = 0x620 ; obj->field1c = 0x620
 *          obj->field40 = 0x25 ; get_char_ani(obj)
 *          obj->field1c = 0x25 (reloaded via a literal after the call)
 *          push t_animate_a0_frames                   (0xd8d)
 *      state 0xd8d
 *          install t_local_reaction_exit
 */
long t_dizzy_by_boss(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field00->field18 = 0x620;
        obj->field1c = 0x620;

        obj->field40 = 0x25;
        get_char_ani(obj);

        obj->field1c = 0x50010;   /* corrected: a literal-pool load, not the 0x25 still in r3 */

        *mk3_frame(thread, thread->frame + 1) = 0xd8d;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_animate_a0_frames;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0xd8d)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* ------------------------------------------------------- t_drone_flipk_getup
 *
 * armv7 0x00041410, three hundred and eight bytes -- seven states, one for
 * each numbered stop between 0xe10 and 0xe17.
 *
 *      state 0
 *          obj->field1c = 3
 *          push t_d_beware_mframew                    (0xe10)
 *      state 0xe10
 *          push t_d_beware                             (0xe12)
 *      state 0xe12
 *          thread->fieldfc = 1 ; return 1              ; continues as 0xe13
 *      state 0xe13
 *          push t_d_beware                             (0xe14)
 *      state 0xe14
 *          thread->fieldfc = 1 ; return 1              ; continues as 0xe15
 *      state 0xe15
 *          push t_d_beware                             (0xe16)
 *      state 0xe16
 *          thread->fieldfc = 1 ; return 1              ; continues as 0xe17
 *      state 0xe17
 *          install t_getup_reaction_exit
 *
 * **The wait states are the same eleven bytes twice over, and the register
 * that makes it work is a coincidence of the dispatch order, not a shared
 * routine.** In the binary, states 0xe12 and 0xe14 branch into ONE block
 * that stores whatever the CPU's `ip` register happens to hold and sleeps.
 * `ip` gets there by falling out of a failed comparison one step earlier in
 * the dispatch chain -- checking "is this 0xe13" leaves 0xe13 sitting in a
 * register that state 0xe12 then stores as its own next token, and the same
 * happens with 0xe15 for state 0xe14. It works because the numbering is
 * sequential and the compiler's own dispatch order matches it; nothing
 * about the source needed to know that. The C here writes each state's
 * real continuation directly rather than reproducing the register reuse,
 * which is what `landfn.sh` checks: the STORED VALUES match, however they
 * got into the register that wrote them.
 *
 * `t_d_beware` is pushed three times running, once per odd-to-even step,
 * and `t_d_beware_mframew` once at the very start -- a getup with three
 * separate glances before the fighter is finally handed to
 * `t_getup_reaction_exit`.
 */
long t_drone_flipk_getup(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0xe13) {
        *mk3_frame(thread, thread->frame + 1) = 0xe14;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_d_beware;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0xe12) {
        *mk3_frame(thread, thread->frame + 1) = 0xe13;
        thread->fieldfc = 1;
        return 1;
    }

    if (token == 0xe15) {
        *mk3_frame(thread, thread->frame + 1) = 0xe16;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_d_beware;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0xe16) {
        *mk3_frame(thread, thread->frame + 1) = 0xe17;
        thread->fieldfc = 1;
        return 1;
    }

    if (token == 0xe17)
        return mk3_install(thread, (MK3THREADFUNC)t_getup_reaction_exit);

    if (token == 0xe10) {
        *mk3_frame(thread, thread->frame + 1) = 0xe12;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_d_beware;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0xe14) {
        *mk3_frame(thread, thread->frame + 1) = 0xe15;
        thread->fieldfc = 1;
        return 1;
    }

    if (token != 0)
        return -3;

    obj->field1c = 3;

    *mk3_frame(thread, thread->frame + 1) = 0xe10;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_d_beware_mframew;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


long t_up_2_ceiling(struct MK3THREAD *thread);
long t_wait_forever(struct MK3THREAD *thread);
long t_pit_fall_scan(struct MK3THREAD *thread);
long t_shake_ob_up(struct MK3THREAD *thread);
void set_inviso(MK3OBJ *obj);
MK3THREAD *NewThread(void *owner_p, MK3THREADFUNC func);


/* -------------------------------------------------------------------- t_fall_in_lava
 *
 * armv7 0x0004819c, a hundred and fifty-two bytes.
 *
 *      state 0
 *          token = 0xa46 ; push t_up_2_ceiling
 *      state 0xa46
 *          obj->field08->field2c = obj->field08->field24 + 0x1bc6
 *          token = 0xa48 ; thread->fieldfc = 0xb4 ; return 0xb4
 *      state 0xa48
 *          death_blow_complete(obj)
 *          install t_wait_forever
 *
 * Sends the fighter up before the lava does anything else --
 * `t_up_2_ceiling` runs first -- and only once that pops does the wait
 * begin: 0x1bc6 added to the other object's own field24 and parked in its
 * field2c, then a hundred and eighty frames of nothing before
 * `death_blow_complete` and `t_wait_forever` end it for good.
 */
long t_fall_in_lava(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0xa46) {
        obj->field08->field2c = obj->field08->field24 + 0x1bc6;

        *mk3_frame(thread, thread->frame + 1) = 0xa48;
        thread->fieldfc = 0xb4;
        return 0xb4;
    }

    if (token == 0xa48) {
        death_blow_complete(obj);
        return mk3_install(thread, (MK3THREADFUNC)t_wait_forever);
    }

    if (token != 0)
        return -3;

    *mk3_frame(thread, thread->frame + 1) = 0xa46;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_up_2_ceiling;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* -------------------------------------------------------------------- t_fall_down_pit
 *
 * armv7 0x0004823c, three hundred and thirty-two bytes -- the OTHER stage
 * hazard `t_background_death`'s table can choose, alongside
 * `t_fall_in_lava` above.
 *
 *      state 0
 *          MKEvent_Add(4, 0x3b, 0, obj->field00->field08)
 *          obj->field1c = 9 ; group_sound(obj) ; obj->field1c = 0
 *          obj->field20 = 0xfff40000                    ; -12.0
 *          obj->field24 = 0x6000                        ; 0.375
 *          obj->field28 = 5 ; obj->field40 = 5 + 0x19    ; 30, SCKNOCKDOWN
 *          obj->field34 = t_pit_fall_scan                ; parked, not run
 *          push t_flight_call                            (0x9e2)
 *      state 0x9e2
 *          obj->field1c = 0x1a ; create_fx(obj)
 *          NewThread(obj, t_machine_sound)
 *          NewThread(obj, t_bone_grind_sound)
 *          obj->field1c = 5 ; obj->field20 = 3 ; obj->field24 = 3
 *          push t_shake_ob_up                            (0x9ed)
 *      state 0x9ed
 *          set_inviso(obj)
 *          MKEvent_Add(4, 0x3c, 0, obj->field00->field08)
 *          token = 0x9f0 ; thread->fieldfc = 0x60 ; return 0x60
 *      state 0x9f0
 *          death_blow_complete(obj)
 *          install t_wait_forever
 *
 * **Two independent sound threads, spawned and left running.** Neither
 * `NewThread` call is saved anywhere -- this routine never touches them
 * again -- so `t_machine_sound` and `t_bone_grind_sound` each finish on
 * their own schedule (three and six plays, per their own banners) and
 * delete themselves through `MK3_THREAD_DONE`, independent of whichever
 * state this function is in by the time they do.
 *
 * `field34` is parked with `t_pit_fall_scan` during the fall but nothing
 * in this function ever installs or calls it -- the same handover shape
 * used elsewhere in this file for a routine some OTHER caller picks up,
 * here left for whatever reads the object's own field34 during the flight
 * that follows.
 */
long t_fall_down_pit(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x9e2) {
        obj->field1c = 0x1a;
        create_fx(obj);

        NewThread(obj, (MK3THREADFUNC)t_machine_sound);
        NewThread(obj, (MK3THREADFUNC)t_bone_grind_sound);

        obj->field1c = 5;
        obj->field20 = 3;
        obj->field24 = 3;

        *mk3_frame(thread, thread->frame + 1) = 0x9ed;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_shake_ob_up;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x9ed) {
        set_inviso(obj);
        MKEvent_Add(4, 0x3c, 0, (long)obj->field00->field08);

        *mk3_frame(thread, thread->frame + 1) = 0x9f0;
        thread->fieldfc = 0x60;
        return 0x60;
    }

    if (token == 0x9f0) {
        death_blow_complete(obj);
        return mk3_install(thread, (MK3THREADFUNC)t_wait_forever);
    }

    if (token != 0)
        return -3;

    MKEvent_Add(4, 0x3b, 0, (long)obj->field00->field08);

    obj->field1c = 9;
    group_sound(obj);
    obj->field1c = 0;

    obj->field20 = 0xfff40000u;      /* -12.0 in 16.16 */
    obj->field24 = 0x6000;           /* 0.375 in 16.16 */
    obj->field28 = 5;
    obj->field40 = 5 + 0x19;         /* 30, SCKNOCKDOWN */
    obj->field34 = (uint32_t)(uintptr_t)t_pit_fall_scan;

    *mk3_frame(thread, thread->frame + 1) = 0x9e2;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_flight_call;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


long t_shake_on_my_back(struct MK3THREAD *thread);
extern uint32_t ochar_ground_offsets[];  /* 0x0016ef04 */


/* -------------------------------------------------------------------- t_pit_fall_scan
 *
 * armv7 0x00047ee4, two hundred and sixty bytes -- a one-shot check, not a
 * routine with a state of its own: every exit here is a real pop, and
 * whoever parks this in field34 is expected to call it again next frame.
 * `t_fall_down_pit`'s own banner already names it as exactly that handover.
 *
 *      state 0 only
 *          obj->field00->field40 = G[0xac] - ochar_ground_offsets[obj->field08->field24]
 *          other = obj->field08
 *          if (other->field1c > 0x130000) {
 *              other->field20 = 0 ; other->field1c = 0x130000
 *          }
 *          if (obj->field00->field40 > (other->field10 + other->field1c*2) >> 16)
 *              pop                                    ; not there yet
 *          if (RoundParam[10] == 0)
 *              pop                                    ; nothing left to spend
 *          RoundParam[10] -= 1
 *          MKEvent_Add(4, 0x3e, 0, obj->field00->field08)
 *          other->field1c = (int32_t)other->field1c / 3
 *          obj->field48 = 0x50005 ; shake_a11(obj)
 *          tsound_func(obj, 1)
 *          obj->field1c = 2 ; group_sound(obj)
 *          pop
 *
 * **The height test compares against a THIRD position, not the floor.**
 * `field10 + field1c*2`, taken as its high half, is neither
 * `MK3_FIELD12` nor `ochar_ground_offsets` -- a target the caller set up
 * before parking this scan, read here without this function needing to
 * know what it is. Passing it clamps the falling velocity to 0x130000 and
 * consumes one of a limited pool (`RoundParam[10]`) to fire the landing
 * effects exactly once; running out of that pool silently stops the
 * effects without stopping the fall.
 */
long t_pit_fall_scan(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    MK3OBJ *other = obj->field08;
    int32_t threshold;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field00->field40 =
        *(const uint32_t *)(const void *)(G_BYTES + 0xac)
        - ochar_ground_offsets[other->field24];

    if ((long)other->field1c > 0x130000) {
        other->field20 = 0;
        other->field1c = 0x130000;
    }

    threshold = (int32_t)(other->field10 + other->field1c * 2) >> 16;
    if ((long)obj->field00->field40 > threshold) {
        if ((long)thread->frame > 0) {
            thread->frame = thread->frame - 1;
            return 0;
        }
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }

    if (RoundParam[10] == 0) {
        if ((long)thread->frame > 0) {
            thread->frame = thread->frame - 1;
            return 0;
        }
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }

    RoundParam[10] = RoundParam[10] - 1;
    MKEvent_Add(4, 0x3e, 0, (long)obj->field00->field08);

    /* `smull` by 0x55555556 is /3, then `rsb r3, r3, r2`: what is kept is
     * the other two thirds (difftest) */
    other->field1c = (uint32_t)((int32_t)other->field1c
                                - (int32_t)other->field1c / 3);

    obj->field48 = 0x50005;
    shake_a11(obj);

    tsound_func(obj, 1);

    obj->field1c = 2;
    group_sound(obj);

    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
        return 0;
    }
    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* -------------------------------------------------------------------- t_fall_on_trax
 *
 * armv7 0x000480e0, a hundred and eighty-eight bytes.
 *
 *      state 0
 *          token = 0xa99 ; push t_up_2_ceiling
 *      state 0xa99
 *          obj->field08->field2c = obj->field08->field24 + 0x1ba8
 *          token = 0xa9b ; thread->fieldfc = 0xb4 ; return 0xb4
 *      state 0xa9b
 *          death_blow_complete(obj)
 *          token = 0xae5 ; thread->fieldfc = 0x20 ; return 0x20
 *      state 0xae5
 *          install t_wait_forever
 *
 * The same three-hazard shape as `t_fall_in_lava` -- send him up first,
 * park an offset on the other object, wait -- with one more sleep spliced
 * in after `death_blow_complete` before this one finally lets go.
 */
long t_fall_on_trax(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0xa99) {
        obj->field08->field2c = obj->field08->field24 + 0x1ba8;

        *mk3_frame(thread, thread->frame + 1) = 0xa9b;
        thread->fieldfc = 0xb4;
        return 0xb4;
    }

    if (token == 0xa9b) {
        death_blow_complete(obj);

        *mk3_frame(thread, thread->frame + 1) = 0xae5;
        thread->fieldfc = 0x20;
        return 0x20;
    }

    if (token == 0xae5)
        return mk3_install(thread, (MK3THREADFUNC)t_wait_forever);

    if (token != 0)
        return -3;

    *mk3_frame(thread, thread->frame + 1) = 0xa99;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_up_2_ceiling;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* -------------------------------------------------------------------- t_land_on_my_back
 *
 * armv7 0x00042518, three hundred and twenty-eight bytes.
 *
 *      state 0
 *          shake_n_sound(obj)
 *          obj->field1c = 3 ; obj->a10 = 1
 *          push t_shake_on_my_back                    (0x1485)
 *      state 0x1485
 *          token = 0x1486 ; thread->fieldfc = 4 ; return 4
 *      state 0x1486
 *          install t_getup_reaction_exit
 *
 * `a10 = obj->field1c - 2` -- the argument slot loaded from a value just
 * set two lines above, kept as a subtraction because that is how the
 * compiler wrote it, not because 1 is meant to look derived.
 */
long t_land_on_my_back(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x1485) {
        *mk3_frame(thread, thread->frame + 1) = 0x1486;
        thread->fieldfc = 4;
        return 4;
    }

    if (token == 0x1486)
        return mk3_install(thread, (MK3THREADFUNC)t_getup_reaction_exit);

    if (token != 0)
        return -3;

    shake_n_sound(obj);

    obj->field1c = 3;
    obj->a10 = obj->field1c - 2;

    *mk3_frame(thread, thread->frame + 1) = 0x1485;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_shake_on_my_back;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* -------------------------------------------------------------------- t_onback3
 *
 * armv7 0x00044d54, three hundred and sixteen bytes.
 *
 *      state 0
 *          rsnd_react_voice(obj)
 *          obj->field1c = 2 ; create_blood_proc(obj)
 *          obj->field1c = 0x30000                       ; 3.0
 *          obj->field20 = obj->field1c - 0xb0000         ; -8.0
 *          obj->field24 = obj->field20 + 0x88000         ; 0.53125
 *          obj->field28 = 5 ; obj->field40 = 5 + 0x19    ; 30, SCKNOCKDOWN
 *          push t_flight                                 (0xe42)
 *      state 0xe42
 *          shake_n_sound(obj)
 *          obj->field40 = 0x1e ; find_ani_part2(obj)
 *          am_i_joy(obj)
 *          if (!obj->field5c)
 *              install t_drone_flipk_getup
 *          else {
 *              obj->field1c = 5
 *              push t_mframew                             (0xe4d)
 *          }
 *      state 0xe4d
 *          token = 0xe4e ; thread->fieldfc = 4 ; return 4
 *      state 0xe4e
 *          install t_getup_reaction_exit
 *
 * **The AI and a human get different getups from the same landing.** Both
 * play the same flight and the same shake, but only a human waits through
 * `t_mframew` into `t_getup_reaction_exit`; the machine is handed straight
 * to `t_drone_flipk_getup` -- the seven-state getup this file already
 * covers -- the moment `am_i_joy` comes back false.
 */
long t_onback3(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0xe42) {
        shake_n_sound(obj);

        obj->field40 = 0x1e;
        find_ani_part2(obj);

        am_i_joy(obj);
        if (obj->field5c == 0)
            return mk3_install(thread, (MK3THREADFUNC)t_drone_flipk_getup);

        obj->field1c = 5;

        *mk3_frame(thread, thread->frame + 1) = 0xe4d;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0xe4d) {
        *mk3_frame(thread, thread->frame + 1) = 0xe4e;
        thread->fieldfc = 4;
        return 4;
    }

    if (token == 0xe4e)
        return mk3_install(thread, (MK3THREADFUNC)t_getup_reaction_exit);

    if (token != 0)
        return -3;

    rsnd_react_voice(obj);

    obj->field1c = 2;
    create_blood_proc(obj);

    obj->field1c = 0x30000;                     /* 3.0 in 16.16 */
    obj->field20 = obj->field1c - 0xb0000;      /* -8.0 */
    obj->field24 = obj->field20 + 0x88000;      /* 0.53125 */
    obj->field28 = 5;
    obj->field40 = 5 + 0x19;                    /* 30, SCKNOCKDOWN */

    *mk3_frame(thread, thread->frame + 1) = 0xe42;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_flight;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


long t_wait_for_his_dog(struct MK3THREAD *thread);


/* ----------------------------------------------------------- t_ken_masters_xfer
 *
 * armv7 0x00047634, three hundred and ninety-two bytes -- the binary's own
 * symbol name, kept as found. Whatever EA's joke was, it is not this
 * project's to solve; both callers in this file (`t_avoid_corner_trap` and
 * `t_separate_us`) park it at `obj->field38` right after `xfer_otherguy` /
 * `takeover_him`, which is the "hand this fighter to the other side" pair.
 *
 *      state 0
 *          obj->field1c = 7.0
 *          save obj->field1c on the arg ring ; am_i_airborn(obj) ; restore
 *          if (obj->field5c) {
 *              obj->field1c = 3.5
 *              obj->field20 = obj->field1c - 9.5     ; -6.0
 *              obj->field24 = obj->field20 + 6.5      ; 0.5
 *              obj->field28 = 5 ; obj->field40 = 5 + 0x19   ; 30, SCKNOCKDOWN
 *              push t_flight                          (0xfd7)
 *          } else {
 *              away_x_vel(obj)
 *              obj->a10 = 10
 *              goto the 0xfde tail below, primed
 *          }
 *      state 0xfd7
 *          install t_land_on_my_back
 *      state 0xfde
 *          move_slave_too(obj)
 *          if (--obj->a10 != 0)
 *              token = 0xfde ; thread->fieldfc = 1 ; return 1     ; loop
 *          token = 0xfe7 ; thread->fieldfc = 1 ; return 1
 *      state 0xfe7
 *          move_slave_too(obj)
 *          other = obj->field08
 *          obj->field1c = abs(other->field18 >> 1)
 *          if (obj->field1c < 0x1000) {
 *              move_slave_too(obj)
 *              obj->a10 = 0x50
 *              push t_wait_for_his_dog                (0xff5)
 *          } else {
 *              away_x_vel(obj)
 *              token = 0xfe7 ; thread->fieldfc = 1 ; return 1     ; loop
 *          }
 *      state 0xff5
 *          install t_local_reaction_exit
 *
 * **This was mislabeled human-vs-AI. It is airborne-vs-grounded.**
 * `obj->field5c` is not a controller flag: it is exactly what `am_i_airborn`
 * (via `aborn4`) writes as its answer, and nothing else touches `field5c`
 * between that call and the branch that reads it. An airborne attacker gets
 * launched on a real arc (3.5/-6.0/0.5, the same launch shape as every other
 * knockdown here, and 0.5 is exactly `FALL_G`) straight into
 * `t_land_on_my_back`. A grounded one never leaves the ground: `away_x_vel`
 * once, then ten frames of `move_slave_too` while `a10` counts down -- a
 * slave object kept in step, one tick at a time, rather than a fighter in
 * flight. So the mercy rule ejects an airborne attacker with a knockdown and
 * a grounded one with a shove, which needs no controller distinction at all.
 *
 * **Only the RETURN VALUE is thrown away, not the answer.** `obj->field1c`
 * is saved and restored around the call -- the same one-call-wide borrow
 * `t_death_slam_pause` makes of `field48` -- so this routine's own 7.0
 * survives unread by anything; nothing captures `am_i_airborn`'s `r0`
 * either. But its SIDE EFFECT on `field5c` is exactly what the very next
 * line branches on, which is the whole point of calling it.
 *
 * **State 0xfe7 is a closing loop, not a single check.** `other->field18`
 * halved and made positive is compared against 0x1000 (0.0625) every pass:
 * still far, `away_x_vel` again and sleep; close enough, one more
 * `move_slave_too`, `a10 = 0x50` and a push into `t_wait_for_his_dog`. So
 * the grounded path is two waits back to back -- ten frames walked in step,
 * then however many it takes to close that gap -- before it ever reaches
 * the same `t_local_reaction_exit` the airborne path's flight eventually
 * lands at through `t_land_on_my_back`.
 */
long t_ken_masters_xfer(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    uint32_t cur, saved;

    if (token == 0xfd7)
        return mk3_install(thread, (MK3THREADFUNC)t_land_on_my_back);

    if (token == 0xfde) {
        move_slave_too(obj);
        obj->field48 = obj->field48 - 1;   /* corrected: the countdown is field48, signed */
        if ((int32_t)obj->field48 > 0) {
            *mk3_frame(thread, thread->frame + 1) = 0xfde;
            thread->fieldfc = 1;
            return 1;
        }

        *mk3_frame(thread, thread->frame + 1) = 0xfe7;
        thread->fieldfc = 1;
        return 1;
    }

    if (token == 0xfe7) {
        MK3OBJ *other;

        move_slave_too(obj);

        other = obj->field08;
        obj->field1c = (uint32_t)((int32_t)other->field18 >> 1);   /* asrs (difftest) */
        if ((int32_t)obj->field1c < 0)
            obj->field1c = (uint32_t)(-(int32_t)obj->field1c);

        if ((int32_t)obj->field1c < 0x1000) {
            move_slave_too(obj);
            obj->a10 = 0x50;

            *mk3_frame(thread, thread->frame + 1) = 0xff5;
            thread->frame = thread->frame + 1;
            mk3_frame(thread, thread->frame)[1] =
                (uint32_t)(uintptr_t)t_wait_for_his_dog;
            *mk3_frame(thread, thread->frame + 1) = 0;
            return 0;
        }

        away_x_vel(obj);
        *mk3_frame(thread, thread->frame + 1) = 0xfe7;
        thread->fieldfc = 1;
        return 1;
    }

    if (token == 0xff5)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

    if (token != 0)
        return -3;

    obj->field1c = 0x70000;                      /* 7.0 in 16.16 */

    cur = thread->fieldf8;
    saved = obj->field1c;
    *mk3_arg(thread, cur) = saved;
    thread->fieldf8 = cur + 1;

    am_i_airborn(obj);

    thread->fieldf8 = thread->fieldf8 - 1;
    obj->field1c = *mk3_arg(thread, thread->fieldf8);

    if (obj->field5c != 0) {
        obj->field1c = 0x38000;                  /* 3.5 in 16.16 */
        obj->field20 = obj->field1c - 0x98000;   /* -6.0 */
        obj->field24 = obj->field20 + 0x68000;   /* 0.5 */
        obj->field28 = 5;
        obj->field40 = 5 + 0x19;                 /* 30, SCKNOCKDOWN */

        *mk3_frame(thread, thread->frame + 1) = 0xfd7;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    away_x_vel(obj);
    obj->field48 = 10;                    /* corrected: field48, not a10 */

    *mk3_frame(thread, thread->frame + 1) = 0xfde;
    thread->fieldfc = 1;
    return 1;
}


/* ------------------------------------------------- the five combo reactions
 *
 * `t_r_combo1`, `t_r_combo2`, `t_r_combo1_stab`, `t_r_combo2_stab` and
 * `t_r_combo_klang` -- five reaction ids, one shape, three numbers apart:
 *
 *      state 0
 *          combo_setup(obj) ; obj->field38 = 0
 *          obj->field30 = t_combo_airborn_hit ; obj->field34 = N
 *          push t_reaction_start               (continuation token C)
 *      state C
 *          set_no_block(obj)
 *          rsnd_func(obj, S)
 *          install <t_combo1 or t_combo2>
 *
 * | function | armv7 | bytes | C | N | S | installs |
 * |---|---|---:|---|---:|---:|---|
 * | `t_r_combo1` | 0x00045ab0 | 156 | 0xc66 | 5 | 10 | `t_combo1` |
 * | `t_r_combo2` | 0x00045978 | 156 | 0xc96 | 9 | 10 | `t_combo2` |
 * | `t_r_combo1_stab` | 0x00045be8 | 156 | 0xc4d | 5 | 3 | `t_combo1` |
 * | `t_r_combo2_stab` | 0x00045a14 | 156 | 0xc88 | 9 | 3 | `t_combo2` |
 * | `t_r_combo_klang` | 0x00045b4c | 156 | 0xc5a | 5 | 9 | `t_combo1` |
 *
 * **`field30` names which airborne-hit routine this combo answers to, and
 * `field34` is not read here at all** -- it is left for whatever runs after
 * `t_combo1`/`t_combo2` install, the way `field38` in `t_combo43` sets up a
 * value this function never reads back either.
 *
 * **The three sound arguments are the one real difference in the group**:
 * 10 for the two ordinary combos, 3 for the two stab variants, 9 for klang.
 * Nothing else about a stab or a klang landing differs from an ordinary
 * combo hit at this level -- the difference is entirely in which sound
 * `rsnd_func` plays and, through `field34`, in code downstream of
 * `t_combo1`/`t_combo2` that this function only sets up for.
 *
 * **Which of `t_combo1`/`t_combo2` a reaction installs is fixed by the
 * strike, not derived here** -- `t_r_combo1`, `t_r_combo1_stab` and
 * `t_r_combo_klang` all install `t_combo1`; only `t_r_combo2` and
 * `t_r_combo2_stab` install `t_combo2`. Three reactions collapsing onto one
 * installer is not a coincidence worth resolving further than stating it.
 */
long t_r_combo1(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0xc66) {
        set_no_block(obj);
        rsnd_func(obj, 0xa);
        return mk3_install(thread, (MK3THREADFUNC)t_combo1);
    }

    if (token != 0)
        return -3;

    combo_setup(obj);
    obj->field38 = 0;
    obj->field30 = (uint32_t)(uintptr_t)t_combo_airborn_hit;
    obj->field34 = 5;

    *mk3_frame(thread, thread->frame + 1) = 0xc66;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


long t_r_combo2(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0xc96) {
        set_no_block(obj);
        rsnd_func(obj, 0xa);
        return mk3_install(thread, (MK3THREADFUNC)t_combo2);
    }

    if (token != 0)
        return -3;

    combo_setup(obj);
    obj->field38 = 0;
    obj->field30 = (uint32_t)(uintptr_t)t_combo_airborn_hit;
    obj->field34 = 9;

    *mk3_frame(thread, thread->frame + 1) = 0xc96;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


long t_r_combo1_stab(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0xc4d) {
        set_no_block(obj);
        rsnd_func(obj, 3);
        return mk3_install(thread, (MK3THREADFUNC)t_combo1);
    }

    if (token != 0)
        return -3;

    combo_setup(obj);
    obj->field38 = 0;
    obj->field30 = (uint32_t)(uintptr_t)t_combo_airborn_hit;
    obj->field34 = 5;

    *mk3_frame(thread, thread->frame + 1) = 0xc4d;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


long t_r_combo2_stab(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0xc88) {
        set_no_block(obj);
        rsnd_func(obj, 3);
        return mk3_install(thread, (MK3THREADFUNC)t_combo2);
    }

    if (token != 0)
        return -3;

    combo_setup(obj);
    obj->field38 = 0;
    obj->field30 = (uint32_t)(uintptr_t)t_combo_airborn_hit;
    obj->field34 = 9;

    *mk3_frame(thread, thread->frame + 1) = 0xc88;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


long t_r_combo_klang(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0xc5a) {
        set_no_block(obj);
        rsnd_func(obj, 9);
        return mk3_install(thread, (MK3THREADFUNC)t_combo1);
    }

    if (token != 0)
        return -3;

    combo_setup(obj);
    obj->field38 = 0;
    obj->field30 = (uint32_t)(uintptr_t)t_combo_airborn_hit;
    obj->field34 = 5;

    *mk3_frame(thread, thread->frame + 1) = 0xc5a;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------------- t_r_boomerang
 *
 * armv7 0x000465b0, a hundred and eighty-eight bytes.
 *
 *      state 0
 *          obj->field48 = 0x40006 ; shake_a11(obj)
 *          obj->field1c = 2 ; group_sound(obj)
 *          obj->field1c = 5 ; his_ochar_sound(obj)
 *          obj->field1c = 0xb ; create_blood_proc(obj)
 *          obj->field34 = 0 ; obj->field38 = 0
 *          obj->field30 = t_airborn_hit_no_sound
 *          push t_reaction_start                        (0x36a)
 *      state 0x36a
 *          obj->field1c = 4.0
 *          install t_stumble_back_vel
 *
 * **Read the field48 literal before trusting it.** The pc-relative `add`
 * that resolves a pointer prints its target in the disassembly, but a plain
 * `ldr rX, [pc, #N]` does not -- and this one is not a pointer. Read
 * straight out of the binary it is `0x40006`, the same {amp, duration} pair
 * `shake_a11` takes everywhere else in this file, not the address stored two
 * lines later into `field30`. The two literals sit close together and are
 * easy to conflate; they are not the same kind of value.
 */
long t_r_boomerang(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x36a) {
        obj->field1c = 0x40000;                  /* 4.0 in 16.16 */
        return mk3_install(thread, (MK3THREADFUNC)t_stumble_back_vel);
    }

    if (token != 0)
        return -3;

    obj->field48 = 0x40006;
    shake_a11(obj);

    obj->field1c = 2;
    group_sound(obj);

    obj->field1c = 5;
    his_ochar_sound(obj);

    obj->field1c = 0xb;
    create_blood_proc(obj);

    obj->field34 = 0;
    obj->field38 = 0;
    obj->field30 = (uint32_t)(uintptr_t)t_airborn_hit_no_sound;

    *mk3_frame(thread, thread->frame + 1) = 0x36a;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* -------------------------------------------------------------------- t_r_fan
 *
 * armv7 0x00046ce8, a hundred and ninety-six bytes.
 *
 *      state 0
 *          obj->field34 = 1 ; if_shao_then_pass(obj)
 *          obj->field30 = 0 ; obj->field38 = 0
 *          push t_reaction_start                        (0x3d4)
 *      state 0x3d4
 *          set_half_damage(obj)
 *          obj->field1c = 2 ; group_sound(obj)
 *          obj->field1c = 1 ; his_ochar_sound(obj)
 *          obj->field40 = 0x20 ; find_ani_part2(obj)
 *          obj->field1c = 3.0 ; away_x_vel(obj)
 *          obj->field1c = 6 ; init_anirate(obj)
 *          obj->a10 = 0x24
 *          install t_rhat_wake
 *
 * **`field30` is left null on purpose, unlike every combo reaction above.**
 * Fan does not name a hit-response routine the way `t_r_combo1` names
 * `t_combo_airborn_hit` or `t_r_boomerang` names `t_airborn_hit_no_sound` --
 * whatever field30 gates, a fan hit does not go through it.
 *
 * **`set_half_damage` runs before anything else in the continuation.** No
 * other reaction here calls it; a fan connecting costs the victim only half
 * of whatever the strike table says.
 */
long t_r_fan(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x3d4) {
        set_half_damage(obj);

        obj->field1c = 2;
        group_sound(obj);

        obj->field1c = 1;
        his_ochar_sound(obj);

        obj->field40 = 0x20;
        find_ani_part2(obj);

        obj->field1c = 0x30000;                  /* 3.0 in 16.16 */
        away_x_vel(obj);

        obj->field1c = 6;
        init_anirate(obj);

        obj->a10 = 0x24;

        return mk3_install(thread, (MK3THREADFUNC)t_rhat_wake);
    }

    if (token != 0)
        return -3;

    obj->field34 = 1;
    if_shao_then_pass(obj);

    obj->field30 = 0;
    obj->field38 = 0;

    *mk3_frame(thread, thread->frame + 1) = 0x3d4;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------- t_r_ind_charge
 *
 * armv7 0x00042b14, two hundred and twenty-four bytes.
 *
 *      state 0
 *          obj->field30 = 0 ; obj->field38 = 0 ; obj->field34 = 3
 *          push t_reaction_start                        (0xee4)
 *      state 0xee4
 *          obj->field48 = 0x60006 ; shake_a11(obj)
 *          obj->field1c = 2 ; group_sound(obj)
 *          rsnd_func(obj, 0xa)
 *          obj->field1c = 4.0 ; field20 = 4.0 - 7.0 = -3.0
 *          field24 = -3.0 + 3.375 = 0.375
 *          field28 = 5 ; field40 = 5 + 0x19 = 30
 *          push t_flight                                (0xef1)
 *      state 0xef1
 *          install t_land_on_my_back
 *
 * The same three-state launch shape `t_ken_masters_xfer`'s human branch
 * uses -- a setup push, a launch push, a landing install -- with its own
 * numbers: -3.0/0.375 here against -6.0/0.5 there.
 */
long t_r_ind_charge(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0xee4) {
        obj->field48 = 0x60006;
        shake_a11(obj);

        obj->field1c = 2;
        group_sound(obj);

        rsnd_func(obj, 0xa);

        obj->field1c = 0x40000;                  /* 4.0 in 16.16 */
        obj->field20 = obj->field1c - 0x70000;   /* -3.0 */
        obj->field24 = obj->field20 + 0x36000;   /* 0.375 */
        obj->field28 = 5;
        obj->field40 = 5 + 0x19;                 /* 30, SCKNOCKDOWN */

        *mk3_frame(thread, thread->frame + 1) = 0xef1;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0xef1)
        return mk3_install(thread, (MK3THREADFUNC)t_land_on_my_back);

    if (token != 0)
        return -3;

    obj->field30 = 0;
    obj->field38 = 0;
    obj->field34 = 3;

    *mk3_frame(thread, thread->frame + 1) = 0xee4;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ----------------------------------------------------------- t_r_jade_prop
 *
 * armv7 0x00042bf4, two hundred and twenty-four bytes -- `t_r_ind_charge`
 * register for register, three constants different.
 *
 *      state 0
 *          obj->field30 = 0 ; obj->field38 = 0 ; obj->field34 = 3
 *          push t_reaction_start                        (0xece)
 *      state 0xece
 *          obj->field48 = 0x60006 ; shake_a11(obj)
 *          obj->field1c = 2 ; group_sound(obj)
 *          rsnd_func(obj, 0xa)
 *          obj->field1c = 6.0 ; field20 = 6.0 - 10.0 = -4.0
 *          field24 = -4.0 + 4.375 = 0.375
 *          field28 = 5 ; field40 = 30
 *          push t_flight                                (0xedb)
 *      state 0xedb
 *          install t_land_on_my_back
 */
long t_r_jade_prop(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0xece) {
        obj->field48 = 0x60006;
        shake_a11(obj);

        obj->field1c = 2;
        group_sound(obj);

        rsnd_func(obj, 0xa);

        obj->field1c = 0x60000;                  /* 6.0 in 16.16 */
        obj->field20 = obj->field1c - 0xa0000;   /* -4.0 */
        obj->field24 = obj->field20 + 0x46000;   /* 0.375 */
        obj->field28 = 5;
        obj->field40 = 5 + 0x19;                 /* 30, SCKNOCKDOWN */

        *mk3_frame(thread, thread->frame + 1) = 0xedb;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0xedb)
        return mk3_install(thread, (MK3THREADFUNC)t_land_on_my_back);

    if (token != 0)
        return -3;

    obj->field30 = 0;
    obj->field38 = 0;
    obj->field34 = 3;

    *mk3_frame(thread, thread->frame + 1) = 0xece;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------------- t_r_airpunch
 *
 * armv7 0x00042cd4, two hundred and twenty bytes.
 *
 *      state 0
 *          rsnd_func(obj, 8)
 *          rsnd_react_voice(obj)
 *          obj->field00->field18 = 0x619 ; obj->field1c = 0x619
 *          obj->field20 = 2
 *          push t_avoid_corner_trap                     (0xea1)
 *      state 0xea1
 *          obj->field1c = 3.0 ; field20 = 3.0 - 9.0 = -6.0
 *          field24 = -6.0 + 6.4375 = 0.4375
 *          field28 = 5 ; field40 = 30
 *          push t_flight                                (0xeb0)
 *      state 0xeb0
 *          install t_land_on_my_back
 *
 * **The setup pushes `t_avoid_corner_trap`, not `t_reaction_start`.** Every
 * other reaction in this batch opens through `t_reaction_start`; this one
 * folds the mercy-rule check straight into its own opening move instead,
 * ahead of the launch. `0x619` written to both `obj->field1c` and, mirrored,
 * `obj->field00->field18` is transcribed as found -- its meaning is not
 * pinned down.
 */
long t_r_airpunch(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0xea1) {
        obj->field1c = 0x30000;                  /* 3.0 in 16.16 */
        obj->field20 = obj->field1c - 0x90000;   /* -6.0 */
        obj->field24 = obj->field20 + 0x67000;   /* 0.4375 */
        obj->field28 = 5;
        obj->field40 = 5 + 0x19;                 /* 30, SCKNOCKDOWN */

        *mk3_frame(thread, thread->frame + 1) = 0xeb0;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0xeb0)
        return mk3_install(thread, (MK3THREADFUNC)t_land_on_my_back);

    if (token != 0)
        return -3;

    rsnd_func(obj, 8);
    rsnd_react_voice(obj);

    obj->field00->field18 = 0x619;
    obj->field1c = 0x619;
    obj->field20 = 2;

    *mk3_frame(thread, thread->frame + 1) = 0xea1;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_avoid_corner_trap;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


long t_animate_a9(struct MK3THREAD *thread);
void dec_my_p_hit(MK3OBJ *obj);


/* ------------------------------------------------------------- t_r_hi_punch
 *
 * armv7 0x00045784, two hundred and forty-four bytes.
 *
 *      state 0
 *          inc_p_block(obj)
 *          obj->field30 = t_r_airpunch ; obj->field34 = 1
 *          obj->field38 = t_cc_hi_punch
 *          push t_reaction_start                        (0xe66)
 *      state 0xe66
 *          rsnd_react_voice(obj)
 *          rsnd_func(obj, 7)
 *          dec_my_p_hit(obj)
 *          obj->field1c = 3 ; create_blood_proc(obj)
 *          obj->field00->field18 = 0x509 ; obj->field1c = 0x509
 *          obj->field40 = 0x3001c
 *          push t_animate_a9                            (0xe72)
 *      state 0xe72
 *          install t_local_reaction_exit
 *
 * **`field30` names the reaction itself, not a generic hit-response
 * routine.** Every other reaction in this batch names a shared responder --
 * `t_r_combo1` names `t_combo_airborn_hit`, `t_r_boomerang` names
 * `t_airborn_hit_no_sound` -- this one names `t_r_airpunch`. Whatever reads
 * `field30` back treats a hi-punch landing as if it could re-enter its own
 * reaction, which `t_r_airpunch` being a launch-into-`t_flight` reaction in
 * its own right (see its own banner above) makes plausible rather than odd.
 *
 * **`field40 = 0x3001c` is transcribed whole, not split, and the high
 * halfword's meaning is settled: it is the animation rate.** The low
 * halfword, 0x1c, is animation 28 -- SCHIHIT, the ordinary hit clip -- and
 * matches `REACT_ANI`'s own reading of this reaction in the Godot port.
 * `t_animate_a9` (0x000556ac) is what unpacks the pair -- an arithmetic
 * shift of the high half into `field1c`, which is what `t_mframew` reads as
 * the rate -- so 3 is the rate here, the same idiom `t_r_hi_kick` uses
 * through a bare `field1c = 4` instead. Every other reaction that looked
 * like it "inherits" (`t_r_lo_punch`, `t_r_duck_punch`, `t_r_duck_kickh/l`,
 * `t_r_flip_punch`, `t_r_lo_kick`, and `t_r_elbow_knee`/`t_r_tusk_elbow`
 * through `t_rek3`) sets the same pair through the same function and reads
 * 3 or 4 the same way -- only `t_r_scorpion_spear` (its own `field1c` is a
 * sound index for `his_ochar_sound`, not a rate, and nothing downstream of
 * it sets one either) genuinely has none. It is a single 32-bit literal
 * load in the binary (`ldr r3, [pc, #0x30]`), not two separate stores, so
 * it is one fact here too -- transcribed whole for that reason, not because
 * the halves were unreadable.
 *
 * `dec_my_p_hit` is the mirror of `inc_p_block`: `t_blocked_start` bumps a
 * block counter, this decrements the hit one -- a landed punch working the
 * opposite side of the same pair of counters `t_avoid_corner_trap`'s mercy
 * rule already reads.
 */
long t_r_hi_punch(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0xe66) {
        rsnd_react_voice(obj);
        rsnd_func(obj, 7);
        dec_my_p_hit(obj);

        obj->field1c = 3;
        create_blood_proc(obj);

        obj->field00->field18 = 0x509;
        obj->field1c = 0x509;

        obj->field40 = 0x3001c;

        *mk3_frame(thread, thread->frame + 1) = 0xe72;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_animate_a9;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0xe72)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

    if (token != 0)
        return -3;

    inc_p_block(obj);

    obj->field30 = (uint32_t)(uintptr_t)t_r_airpunch;
    obj->field34 = 1;
    obj->field38 = (uint32_t)(uintptr_t)t_cc_hi_punch;

    *mk3_frame(thread, thread->frame + 1) = 0xe66;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* -------------------------------------------------------------- t_r_hi_kick
 *
 * armv7 0x00045410, four hundred and twelve bytes -- six states, and the
 * one reaction in this batch confirmed to set its own rate for the ordinary
 * hit clip (see the walk-rate note in the Godot port, `REACT_RATE`).
 *
 *      state 0
 *          obj->field1c = 2 ; create_blood_proc(obj)
 *          obj->field1c = 2 ; group_sound(obj)
 *          rsnd_func(obj, 0xa)
 *          obj->field38 = 0 ; obj->field30 = t_generic_airborn_hit
 *          obj->field34 = 1
 *          push t_reaction_start                        (0x5f8)
 *      state 0x5f8
 *          obj->field1c = 4.5 ; away_x_vel(obj)
 *          obj->field40 = 0x1c ; get_char_ani(obj)
 *          do_next_a9_frame(obj)
 *          token = 0x600 ; thread->fieldfc = 2 ; return 2
 *      state 0x600
 *          do_next_a9_frame(obj)
 *          token = 0x602 ; thread->fieldfc = 6 ; return 6
 *      state 0x602
 *          obj->a10 = 0xa
 *          -- falls into state 0x609's own sleep, primed, skipping its
 *             first pass through the distance check
 *      state 0x609
 *          other = obj->field08
 *          obj->field1c = abs(other->field18)
 *          obj->field20 = obj->field1c >> 6
 *          obj->field1c = obj->field1c - obj->field20
 *          away_x_vel(obj)
 *          if (--obj->a10 > 0)
 *              token = 0x609 ; thread->fieldfc = 1 ; return 1     ; loop
 *      (falls through when a10 reaches 0)
 *          stop_me_player(obj)
 *          obj->field40 = 0x1c ; get_char_ani(obj) ; obj->field40 += 4
 *          obj->field1c = 4
 *          push t_mframew                               (0x61b)
 *      state 0x61b
 *          install t_local_reaction_exit
 *
 * **The same closing-distance loop `t_ken_masters_xfer`'s AI branch runs**,
 * register for register -- `other->field18` scaled and subtracted from
 * itself, `away_x_vel` every pass, a counter primed to 10 either by falling
 * out of `state 0x600`'s own six-frame sleep or by `state 0x602` seating it
 * directly. Two different ways into the same wait, the way this file's
 * launch reactions share one landing.
 *
 * **`field1c = 4` before `t_mframew` is the rate this reaction plays SCHIHIT
 * at, read straight out of the pool as its own literal** -- not inherited,
 * the way `t_r_hi_punch` above and every other non-knockdown reaction in
 * this file leaves it. `t_mframew` (other.c, already decompiled) takes
 * `field1c` back out as the sleep length itself, so this is a real,
 * verified rate rather than a guess: a high kick landing plays its
 * recipient's ordinary hit clip at 4, whatever they were doing when it
 * connected.
 */
long t_r_hi_kick(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x600) {
        do_next_a9_frame(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x602;
        thread->fieldfc = 6;
        return 6;
    }

    if (token == 0x609 || token == 0x602) {
        MK3OBJ *other;

        if (token == 0x602) {
            /* 0x4555e: a10 = 10, then b 0x45548 -- park on 0x609 and return
             * 1; the countdown starts next frame (difftest) */
            obj->a10 = 0xa;
            *mk3_frame(thread, thread->frame + 1) = 0x609;
            thread->fieldfc = 1;
            return 1;
        } else {
            other = obj->field08;
            obj->field1c = other->field18;
            if ((int32_t)obj->field1c < 0)
                obj->field1c = (uint32_t)(-(int32_t)obj->field1c);

            obj->field20 = (uint32_t)((int32_t)obj->field1c >> 6);
            obj->field1c = obj->field1c - obj->field20;

            away_x_vel(obj);

            obj->a10 = obj->a10 - 1;
            if ((int32_t)obj->a10 > 0) {
                *mk3_frame(thread, thread->frame + 1) = 0x609;
                thread->fieldfc = 1;
                return 1;
            }
        }

        stop_me_player(obj);

        obj->field40 = 0x1c;
        get_char_ani(obj);
        obj->field40 = obj->field40 + 4;

        obj->field1c = 4;

        *mk3_frame(thread, thread->frame + 1) = 0x61b;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x61b)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

    if (token == 0x5f8) {
        obj->field1c = 0x48000;                  /* 4.5 in 16.16 */
        away_x_vel(obj);

        obj->field40 = 0x1c;
        get_char_ani(obj);

        do_next_a9_frame(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x600;
        thread->fieldfc = 2;
        return 2;
    }

    if (token != 0)
        return -3;

    obj->field1c = 2;
    create_blood_proc(obj);

    obj->field1c = 2;
    group_sound(obj);

    rsnd_func(obj, 0xa);

    obj->field38 = 0;
    obj->field30 = (uint32_t)(uintptr_t)t_generic_airborn_hit;
    obj->field34 = 1;

    *mk3_frame(thread, thread->frame + 1) = 0x5f8;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


long t_drone_post_duck_hit(struct MK3THREAD *thread);   /* mkdrone.c, not yet written */
void set_quarter_damage(MK3OBJ *obj);
void get_my_height(MK3OBJ *obj);


/* --------------------------------------------------------- t_r_duck_kickh
 *
 * armv7 0x00048ae8, two hundred and ninety-six bytes.
 *
 *      state 0
 *          obj->field30 = t_generic_airborn_hit ; obj->field34 = 1
 *          obj->field38 = t_cc_ken_masters
 *          push t_reaction_start                        (0x673)
 *      state 0x673
 *          rsnd_func(obj, 7) ; rsnd_react_voice(obj)
 *          obj->field1c = 4.5 ; away_x_vel(obj)
 *          obj->field40 = 0x3001d
 *          get_my_height(obj)
 *          if (obj->field20 <= 0x80) obj->field40 = 0x30007
 *          push t_animate_a9                            (0x682)
 *      state 0x682
 *          stop_me_player(obj)
 *          am_i_joy(obj)
 *          if (!obj->field5c) install t_drone_post_duck_hit
 *          else                install t_local_reaction_exit
 *
 * **Human and machine get different endings, same as `t_onback3`.**
 * `am_i_joy` here reads back through the same `field5c` every other
 * am-I-human check in this file uses; a human is simply handed to
 * `t_local_reaction_exit`, while the AI gets routed into `mkdrone.c`'s own
 * follow-up (not yet decompiled, forward-declared here).
 *
 * **The animation the victim plays depends on their own height, not on a
 * fixed clip.** `get_my_height` fills `field20`, and only then does the
 * choice between `0x3001d` and `0x30007` get made -- a crouching victim
 * (height at or under 0x80) plays a different clip than a standing one.
 * Both literals share `t_r_hi_punch`'s `0x3000x` shape; the high halfword's
 * meaning is carried over from there, unread.
 */
long t_r_duck_kickh(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x682) {
        stop_me_player(obj);

        am_i_joy(obj);
        if (obj->field5c == 0)
            return mk3_install(thread, (MK3THREADFUNC)t_drone_post_duck_hit);

        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }

    if (token == 0x673) {
        rsnd_func(obj, 7);
        rsnd_react_voice(obj);

        obj->field1c = 0x48000;                  /* 4.5 in 16.16 */
        away_x_vel(obj);

        obj->field40 = 0x3001d;
        get_my_height(obj);
        if ((int32_t)obj->field20 <= 0x80)
            obj->field40 = 0x30007;

        *mk3_frame(thread, thread->frame + 1) = 0x682;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_animate_a9;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0)
        return -3;

    obj->field30 = (uint32_t)(uintptr_t)t_generic_airborn_hit;
    obj->field34 = 1;
    obj->field38 = (uint32_t)(uintptr_t)t_cc_ken_masters;

    *mk3_frame(thread, thread->frame + 1) = 0x673;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------- t_r_duck_kickl
 *
 * armv7 0x000489c0, two hundred and ninety-six bytes -- `t_r_duck_kickh`
 * register for register, three fields different.
 *
 *      state 0
 *          obj->field30 = t_r_airborn_duck_kick ; obj->field34 = 1
 *          obj->field38 = t_cc_ken_masters
 *          push t_reaction_start                        (0x692)
 *      state 0x692
 *          rsnd_func(obj, 7) ; rsnd_react_voice(obj)
 *          obj->field1c = 3.0 ; away_x_vel(obj)
 *          obj->field40 = 0x3001d
 *          get_my_height(obj)
 *          if (obj->field20 <= 0x80) obj->field40 = 0x30007
 *          push t_animate_a9                            (0x69f)
 *      state 0x69f
 *          stop_me_player(obj)
 *          am_i_joy(obj)
 *          if (!obj->field5c) install t_drone_post_duck_hit
 *          else                install t_local_reaction_exit
 *
 * **The two duck-kick reactions land on the same victim clips.** Both the
 * `0x3001d`/`0x30007` pair and the height gate that picks between them are
 * identical to `t_r_duck_kickh` -- a low kick and a high kick to a
 * crouching fighter knock him into the same two poses. What differs is the
 * push-back (3.0 here against 4.5 there) and which reaction the victim's
 * own `field30` is left pointing at for whatever reads it back:
 * `t_r_airborn_duck_kick` here, `t_generic_airborn_hit` there.
 */
long t_r_duck_kickl(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x69f) {
        stop_me_player(obj);

        am_i_joy(obj);
        if (obj->field5c == 0)
            return mk3_install(thread, (MK3THREADFUNC)t_drone_post_duck_hit);

        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }

    if (token == 0x692) {
        rsnd_func(obj, 7);
        rsnd_react_voice(obj);

        obj->field1c = 0x30000;                  /* 3.0 in 16.16 */
        away_x_vel(obj);

        obj->field40 = 0x3001d;
        get_my_height(obj);
        if ((int32_t)obj->field20 <= 0x80)
            obj->field40 = 0x30007;

        *mk3_frame(thread, thread->frame + 1) = 0x69f;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_animate_a9;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0)
        return -3;

    obj->field30 = (uint32_t)(uintptr_t)t_r_airborn_duck_kick;
    obj->field34 = 1;
    obj->field38 = (uint32_t)(uintptr_t)t_cc_ken_masters;

    *mk3_frame(thread, thread->frame + 1) = 0x692;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


long t_r_duck_airpunch(struct MK3THREAD *thread);


/* --------------------------------------------------------- t_r_duck_punch
 *
 * armv7 0x00048c10, two hundred and ninety-six bytes -- the third of the
 * three duck reactions, same shape as `t_r_duck_kickh`/`t_r_duck_kickl`.
 *
 *      state 0
 *          obj->field30 = t_r_duck_airpunch ; obj->field34 = 1
 *          obj->field38 = t_cc_ken_masters
 *          push t_reaction_start                        (0x643)
 *      state 0x643
 *          rsnd_func(obj, 7) ; rsnd_react_voice(obj)
 *          obj->field1c = 4.0 ; away_x_vel(obj)
 *          obj->field40 = 0x3001d
 *          get_my_height(obj)
 *          if (obj->field20 <= 0x80) obj->field40 = 0x30007
 *          push t_animate_a9                            (0x651)
 *      state 0x651
 *          stop_me_player(obj)
 *          am_i_joy(obj)
 *          if (!obj->field5c) install t_drone_post_duck_hit
 *          else                install t_local_reaction_exit
 *
 * The third of three -- all three duck reactions play the same victim
 * pair (`0x3001d`/`0x30007`, height-gated), route the AI through the same
 * `t_drone_post_duck_hit`, and differ only in their own push-back speed
 * (4.5, 3.0, 4.0) and which of their own kind `field30` names back:
 * `t_generic_airborn_hit`, `t_r_airborn_duck_kick`, `t_r_duck_airpunch`.
 */
long t_r_duck_punch(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x651) {
        stop_me_player(obj);

        am_i_joy(obj);
        if (obj->field5c == 0)
            return mk3_install(thread, (MK3THREADFUNC)t_drone_post_duck_hit);

        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }

    if (token == 0x643) {
        rsnd_func(obj, 7);
        rsnd_react_voice(obj);

        obj->field1c = 0x40000;                  /* 4.0 in 16.16 */
        away_x_vel(obj);

        obj->field40 = 0x3001d;
        get_my_height(obj);
        if ((int32_t)obj->field20 <= 0x80)
            obj->field40 = 0x30007;

        *mk3_frame(thread, thread->frame + 1) = 0x651;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_animate_a9;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0)
        return -3;

    obj->field30 = (uint32_t)(uintptr_t)t_r_duck_airpunch;
    obj->field34 = 1;
    obj->field38 = (uint32_t)(uintptr_t)t_cc_ken_masters;

    *mk3_frame(thread, thread->frame + 1) = 0x643;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ----------------------------------------------------------- t_r_duck_airpunch
 *
 * armv7 0x00042f18, two hundred and four bytes -- the same three-state
 * launch shape `t_r_ind_charge`/`t_r_jade_prop`/`t_r_airpunch` already use,
 * with `t_avoid_corner_trap` as the opening move like `t_r_airpunch`.
 *
 *      state 0
 *          rsnd_func(obj, 8) ; rsnd_react_voice(obj)
 *          obj->field20 = 3
 *          push t_avoid_corner_trap                     (0x660)
 *      state 0x660
 *          obj->field1c = 3.5 ; field20 = 3.5 - 9.5 = -6.0
 *          field24 = -6.0 + 6.5 = 0.5
 *          field28 = 5 ; field40 = 5 + 0x19 = 30
 *          push t_flight                                (0x667)
 *      state 0x667
 *          install t_land_on_my_back
 */
long t_r_duck_airpunch(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x660) {
        obj->field1c = 0x38000;                  /* 3.5 in 16.16 */
        obj->field20 = obj->field1c - 0x98000;   /* -6.0 */
        obj->field24 = obj->field20 + 0x68000;   /* 0.5 */
        obj->field28 = 5;
        obj->field40 = 5 + 0x19;                 /* 30, SCKNOCKDOWN */

        *mk3_frame(thread, thread->frame + 1) = 0x667;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x667)
        return mk3_install(thread, (MK3THREADFUNC)t_land_on_my_back);

    if (token != 0)
        return -3;

    rsnd_func(obj, 8);
    rsnd_react_voice(obj);

    obj->field20 = 3;

    *mk3_frame(thread, thread->frame + 1) = 0x660;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_avoid_corner_trap;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------------- t_r_flip_punch
 *
 * armv7 0x00043a38, two hundred and twenty-eight bytes.
 *
 *      state 0
 *          rsnd_react_voice(obj)
 *          obj->field48 = 0x40004 ; shake_a11(obj)
 *          obj->field38 = 0
 *          obj->field30 = t_generic_airborn_hit ; obj->field34 = 1
 *          push t_reaction_start                        (0xdc3)
 *      state 0xdc3
 *          rsnd_func(obj, 8)
 *          obj->field1c = 1.0 ; away_x_vel(obj)
 *          obj->field40 = 0x4001c
 *          push t_animate_a9                            (0xdc9)
 *      state 0xdc9
 *          install t_local_reaction_exit
 */
long t_r_flip_punch(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0xdc3) {
        rsnd_func(obj, 8);

        obj->field1c = 0x10000;                  /* 1.0 in 16.16 */
        away_x_vel(obj);

        obj->field40 = 0x4001c;

        *mk3_frame(thread, thread->frame + 1) = 0xdc9;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_animate_a9;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0xdc9)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

    if (token != 0)
        return -3;

    rsnd_react_voice(obj);

    obj->field48 = 0x40004;
    shake_a11(obj);

    obj->field38 = 0;
    obj->field30 = (uint32_t)(uintptr_t)t_generic_airborn_hit;
    obj->field34 = 1;

    *mk3_frame(thread, thread->frame + 1) = 0xdc3;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------------------- t_r_hat
 *
 * armv7 0x000440b8, a hundred and eighty bytes -- `t_r_fan`'s shape again,
 * register for register, with its own animation offset and installer.
 *
 *      state 0
 *          obj->field34 = 1 ; if_shao_then_pass(obj)
 *          obj->field30 = 0 ; obj->field38 = 0
 *          push t_reaction_start                        (0x11dc)
 *      state 0x11dc
 *          obj->field1c = 2 ; group_sound(obj)
 *          obj->field40 = 0x20 ; find_ani_part2(obj)
 *          obj->field1c = 3.0 ; away_x_vel(obj)
 *          obj->field1c = 6 ; init_anirate(obj)
 *          obj->a10 = 0x24
 *          install t_rhat_sleep
 */
long t_r_hat(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x11dc) {
        obj->field1c = 2;
        group_sound(obj);

        obj->field40 = 0x20;
        find_ani_part2(obj);

        obj->field1c = 0x30000;                  /* 3.0 in 16.16 */
        away_x_vel(obj);

        obj->field1c = 6;
        init_anirate(obj);

        obj->a10 = 0x24;

        return mk3_install(thread, (MK3THREADFUNC)t_rhat_sleep);
    }

    if (token != 0)
        return -3;

    obj->field34 = 1;
    if_shao_then_pass(obj);

    obj->field30 = 0;
    obj->field38 = 0;

    *mk3_frame(thread, thread->frame + 1) = 0x11dc;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* -------------------------------------------------------------- t_reaction_land
 *
 * armv7 0x000425b8, a hundred and sixty bytes -- the routine `FALL_TAIL`'s
 * own comment in the Godot port already named without having read it.
 *
 *      state 0
 *          shake_n_sound(obj)
 *          obj->field40 = 0x1e ; find_ani_part2(obj)
 *          obj->field1c = 4
 *          push t_mframew                               (0x147b)
 *      state 0x147b
 *          token = 0x147c ; thread->fieldfc = 3 ; return 3
 *      state 0x147c
 *          install t_getup_reaction_exit
 *
 * Confirms the port's own reading exactly: rate 4 through `t_mframew`
 * (`SCKNOCKDOWN`, animation 30, plays its tail at 4), then a plain
 * three-frame sleep before the getup -- `RATE_LAND := 4` and
 * `LAND_WAIT := 3` in `umk3_fight.gd`.
 */
long t_reaction_land(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x147b) {
        *mk3_frame(thread, thread->frame + 1) = 0x147c;
        thread->fieldfc = 3;
        return 3;
    }

    if (token == 0x147c)
        return mk3_install(thread, (MK3THREADFUNC)t_getup_reaction_exit);

    if (token != 0)
        return -3;

    shake_n_sound(obj);

    obj->field40 = 0x1e;
    find_ani_part2(obj);

    obj->field1c = 4;

    *mk3_frame(thread, thread->frame + 1) = 0x147b;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------------------ t_r_axe_up
 *
 * armv7 0x00046bec, two hundred and fifty-two bytes.
 *
 *      state 0
 *          obj->field1c = 1 ; create_blood_proc(obj)
 *          obj->field1c = 9 ; his_ochar_sound(obj)
 *          set_half_damage(obj)
 *          obj->field30 = 0 ; obj->field34 = 1
 *          obj->field38 = t_cc_ken_masters
 *          push t_reaction_start                        (0x5be)
 *      state 0x5be
 *          obj->field1c = 2 ; group_sound(obj)
 *          obj->field1c = 1.0 ; field20 = 1.0 - 9.0 = -8.0
 *          field24 = -8.0 + 8.375 = 0.375
 *          field28 = 5 ; field40 = 5 + 0x19 = 30
 *          push t_flight                                (0x5c8)
 *      state 0x5c8
 *          install t_reaction_land
 */
long t_r_axe_up(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x5be) {
        obj->field1c = 2;
        group_sound(obj);

        obj->field1c = 0x10000;                  /* 1.0 in 16.16 */
        obj->field20 = obj->field1c - 0x90000;   /* -8.0 */
        obj->field24 = obj->field20 + 0x86000;   /* 0.375 */
        obj->field28 = 5;
        obj->field40 = 5 + 0x19;                 /* 30, SCKNOCKDOWN */

        *mk3_frame(thread, thread->frame + 1) = 0x5c8;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x5c8)
        return mk3_install(thread, (MK3THREADFUNC)t_reaction_land);

    if (token != 0)
        return -3;

    obj->field1c = 1;
    create_blood_proc(obj);

    obj->field1c = 9;
    his_ochar_sound(obj);

    set_half_damage(obj);

    obj->field30 = 0;
    obj->field34 = 1;
    obj->field38 = (uint32_t)(uintptr_t)t_cc_ken_masters;

    *mk3_frame(thread, thread->frame + 1) = 0x5be;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ---------------------------------------------------------------- t_r_combo3
 *
 * armv7 0x00046848, two hundred and fifty-two bytes.
 *
 *      state 0
 *          combo_setup(obj) ; obj->field38 = 0
 *          obj->field30 = t_combo_airborn_hit ; obj->field34 = 9
 *          push t_reaction_start                        (0xcb8)
 *      (corrected: field30 was first read as t_generic_airborn_hit)
 *      state 0xcb8
 *          set_no_block(obj)
 *          rsnd_func(obj, 0xa)
 *          set_half_damage(obj)
 *          obj->field1c = 4 ; create_blood_proc(obj)
 *          obj->field1c = 0xe ; create_fx(obj)
 *          obj->field1c = 2.0 ; field20 = 2.0 - 10.0 = -8.0
 *          field24 = -8.0 + 8.53125 = 0.53125
 *          field28 = 5 ; field40 = 5 + 0x19 = 30
 *          push t_flight                                (0xcca)
 *      state 0xcca
 *          install t_land_on_my_back
 */
long t_r_combo3(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0xcb8) {
        set_no_block(obj);
        rsnd_func(obj, 0xa);
        set_half_damage(obj);

        obj->field1c = 4;
        create_blood_proc(obj);

        obj->field1c = 0xe;
        create_fx(obj);

        obj->field1c = 0x20000;                  /* 2.0 in 16.16 */
        obj->field20 = obj->field1c - 0xa0000;   /* -8.0 */
        obj->field24 = obj->field20 + 0x88000;   /* 0.53125 */
        obj->field28 = 5;
        obj->field40 = 5 + 0x19;                 /* 30, SCKNOCKDOWN */

        *mk3_frame(thread, thread->frame + 1) = 0xcca;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0xcca)
        return mk3_install(thread, (MK3THREADFUNC)t_land_on_my_back);

    if (token != 0)
        return -3;

    combo_setup(obj);
    obj->field38 = 0;
    obj->field30 = (uint32_t)(uintptr_t)t_combo_airborn_hit;   /* corrected: was t_generic_airborn_hit */
    obj->field34 = 9;

    *mk3_frame(thread, thread->frame + 1) = 0xcb8;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ---------------------------------------------------------------- t_r_combo6
 *
 * armv7 0x00046ae4, two hundred and sixty-four bytes.
 *
 *      state 0
 *          set_half_damage(obj)
 *          obj->field34 = 4 ; obj->field30 = 0
 *          obj->field38 = t_cc_ken_masters
 *          push t_reaction_start                        (0x6e6)
 *      state 0x6e6
 *          obj->field1c = 1 ; create_blood_proc(obj)
 *          obj->field48 = 0x60006 ; shake_a11(obj)
 *          rsnd_func(obj, 0xa)
 *          obj->field1c = 2 ; group_sound(obj)
 *          obj->field1c = 0xe ; create_fx(obj)
 *          obj->field1c = 1.0 ; field20 = 1.0 - 13.0 = -12.0
 *          field24 = -12.0 + 12.375 = 0.375
 *          field28 = 5 ; field40 = 5 + 0x19 = 30
 *          push t_flight                                (0x6fa)
 *      state 0x6fa
 *          install t_reaction_land
 */
long t_r_combo6(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x6e6) {
        obj->field1c = 1;
        create_blood_proc(obj);

        obj->field48 = 0x60006;
        shake_a11(obj);

        rsnd_func(obj, 0xa);

        obj->field1c = 2;
        group_sound(obj);

        obj->field1c = 0xe;
        create_fx(obj);

        obj->field1c = 0x10000;                  /* 1.0 in 16.16 */
        obj->field20 = obj->field1c - 0xd0000;   /* -12.0 */
        obj->field24 = obj->field20 + 0xc6000;   /* 0.375 */
        obj->field28 = 5;
        obj->field40 = 5 + 0x19;                 /* 30, SCKNOCKDOWN */

        *mk3_frame(thread, thread->frame + 1) = 0x6fa;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x6fa)
        return mk3_install(thread, (MK3THREADFUNC)t_reaction_land);

    if (token != 0)
        return -3;

    set_half_damage(obj);

    obj->field34 = 4;
    obj->field30 = 0;
    obj->field38 = (uint32_t)(uintptr_t)t_cc_ken_masters;

    *mk3_frame(thread, thread->frame + 1) = 0x6e6;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------------------ t_rup3
 *
 * armv7 0x00045ed4, two hundred and sixteen bytes -- the uppercut's real
 * launch, already partly read from `UPCUT_VY`/`UPCUT_G`'s own citation in
 * the Godot port; this is the full state machine behind that citation.
 *
 *      state 0
 *          obj->field1c = 0xe ; create_fx(obj)
 *          obj->field1c = 2.0
 *          ptr = *(void **)0xf3534           -- an unnamed global pointer
 *          if (ptr->field08 != 0)
 *              install t_blast_through_anything
 *          else if (ptr->field30 (signed byte) == 0)
 *              obj->field20 = -12.0 ; obj->field24 = field20 + 12.375
 *              obj->field28 = 5 ; obj->field40 = 30
 *              push t_flight                            (0x786)
 *          else
 *              obj->field20 = -18.0 ; obj->field24 = 0x5800 (0.34375)
 *              obj->field28 = 5 ; obj->field40 = 30
 *              push t_flight                            (0x786)
 *      state 0x786
 *          install t_reaction_land
 *
 * **`UPCUT_VY`/`UPCUT_G` are the THIRD branch, not the only one.** The
 * ordinary uppercut launch -- -18.0/0.34375, exactly what the Godot port
 * already carries -- only fires when the unnamed global's signed byte at
 * `+0x30` is non-zero; a zero there launches at a flatter -12.0/0.375
 * instead, and a non-zero word at the SAME pointer's `+0x08` skips the
 * launch entirely and hands straight to `t_blast_through_anything`. What
 * that pointer is, and what its two fields mean, is not established --
 * the address (0x000f3534) resolves to `0x0038ed04` in `__DATA`, distinct
 * from `_G` (`0x0038c1fc`), and carries no symbol of its own. Read as two
 * raw offsets rather than guessed at.
 */
long t_rup3(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x786)
        return mk3_install(thread, (MK3THREADFUNC)t_reaction_land);

    if (token != 0)
        return -3;

    obj->field1c = 0xe;
    create_fx(obj);

    obj->field1c = 0x20000;                      /* 2.0 in 16.16 */

    {
        /* Slot 0x000f3534 holds &_RoundParam (0x0038ed04). Read as the raw
         * iOS address it was, the first uppercut of a fight crashed. */
        unsigned char *ptr = (unsigned char *)RoundParam;
        uint32_t field08 = *(uint32_t *)(ptr + 8);
        int8_t field30 = *(int8_t *)(ptr + 0x30);

        if (field08 != 0)
            return mk3_install(thread, (MK3THREADFUNC)t_blast_through_anything);

        if (field30 == 0) {
            obj->field20 = 0xfff40000;           /* -12.0 in 16.16 */
            obj->field24 = obj->field20 + 0xc6000; /* 0.375 */
        } else {
            obj->field20 = 0xffee0000;           /* -18.0 in 16.16 */
            obj->field24 = 0x5800;               /* 0.34375, UPCUT_G */
        }
    }

    obj->field28 = 5;
    obj->field40 = 5 + 0x19;                     /* 30, SCKNOCKDOWN */

    *mk3_frame(thread, thread->frame + 1) = 0x786;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_flight;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ---------------------------------------------------------------- t_r_combo5
 *
 * armv7 0x00048620, two hundred and four bytes.
 *
 *      state 0
 *          if (obj->field00->field04->field24 == 0xd)
 *              set_quarter_damage(obj)
 *          else
 *              set_half_damage(obj)
 *          obj->field34 = 4 ; obj->field30 = 0
 *          obj->field38 = t_cc_ken_masters
 *          push t_reaction_start                        (0x712)
 *      state 0x712
 *          obj->field1c = 1 ; create_blood_proc(obj)
 *          obj->field48 = 0x60006 ; shake_a11(obj)
 *          rsnd_func(obj, 0xa)
 *          obj->field1c = 2 ; group_sound(obj)
 *          install t_rup3
 *
 * **The damage cut depends on a triple dereference, read out as a plain
 * comparison rather than named.** `obj->field00->field04->field24`
 * (a struct two hops from the object) equal to `0xd` selects
 * `set_quarter_damage` over the usual `set_half_damage` every other combo
 * finisher in this file calls; the field's own meaning is not established.
 *
 * **This one installs `t_rup3` directly, no `t_flight` push of its own.**
 * `t_rup3` (see its own banner above) does the actual launch -- the same
 * shape `t_r_uppercut`'s own chain reaches it through
 * (`t_reaction_start -> t_rst5 -> t_cc_ken_masters -> t_avoid_corner_trap
 * -> t_pit_abort -> t_rup3`), reused here as a finisher's landing.
 */
long t_r_combo5(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x712) {
        obj->field1c = 1;
        create_blood_proc(obj);

        obj->field48 = 0x60006;
        shake_a11(obj);

        rsnd_func(obj, 0xa);

        obj->field1c = 2;
        group_sound(obj);

        return mk3_install(thread, (MK3THREADFUNC)t_rup3);
    }

    if (token != 0)
        return -3;

    {
        /* obj->field00->field04->field24 -- field04 is not a struct field
         * anywhere else in this file, so read raw rather than invent one. */
        uint8_t *inner = *(uint8_t *P32 *)((uint8_t *)obj->field00 + 4);
        obj->field1c = *(uint32_t *)(inner + 0x24);  /* str r3,[r4,#0x1c] (difftest) */
        if (obj->field1c == 0xd)
            set_quarter_damage(obj);
        else
            set_half_damage(obj);
    }

    obj->field34 = 4;
    obj->field30 = 0;
    obj->field38 = (uint32_t)(uintptr_t)t_cc_ken_masters;

    *mk3_frame(thread, thread->frame + 1) = 0x712;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ---------------------------------------------------------------- t_r_combo0
 *
 * armv7 0x00045c84, three hundred and forty-four bytes.
 *
 *      state 0
 *          obj->field1c = 4 ; create_blood_proc(obj)
 *          combo_setup(obj) ; obj->field38 = 0
 *          obj->field30 = t_combo_airborn_hit ; obj->field34 = 5
 *          push t_reaction_start                        (0xc27)
 *      state 0xc27
 *          if (obj->field00->p_hit > 5) {
 *              if (obj->field00->field04->field24 == 4) {
 *                  other = obj->field08 ; other->field30 &= ~4
 *                  if ((obj->field00->field10 & 1) == 0)
 *                      install t_separate_us
 *                  -- else falls into the ordinary swing below
 *              }
 *          }
 *          -- ordinary swing:
 *          set_no_block(obj)
 *          rsnd_func(obj, 0xa)
 *          obj->field40 = 0x1c ; get_char_ani(obj)
 *          obj->field00->p_hit = obj->field40 ; obj->field40 += 0xc
 *          do_next_a9_frame(obj)
 *          token = 0xc3a ; thread->fieldfc = 3 ; return 3
 *      state 0xc3a
 *          obj->field40 = obj->field00->p_hit ; obj->field1c = 4
 *          push t_mframew                               (0xc3d)
 *      state 0xc3d
 *          install t_local_reaction_exit
 *
 * **`p_hit` (offset 0x44 on `MK3OBJPROC`) is doing double duty here**,
 * reused as a plain save slot for the animation offset across the sleep --
 * nothing to do with the hit counter every other reader of `p_hit` in this
 * file uses it for. The compiler reused the field because it was free, not
 * because the value means "hits taken" in this context.
 *
 * **The `t_separate_us` branch only fires once, and clears its own
 * trigger.** `other->field30 &= ~4` turns the condition off on the object
 * that carries it, so a combo that ends past six swings with that bit set
 * and the low bit of `field10` clear breaks the pair apart exactly once
 * rather than every frame the state is re-entered.
 */
long t_r_combo0(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0xc3a) {
        obj->field40 = obj->a10;          /* corrected: the object's own +0x44, not p_hit */
        obj->field1c = 4;

        *mk3_frame(thread, thread->frame + 1) = 0xc3d;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0xc3d)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

    if (token == 0xc27) {
        if ((int32_t)obj->field00->p_hit > 5) {
            uint8_t *inner = *(uint8_t *P32 *)((uint8_t *)obj->field00 + 4);
            if (*(uint32_t *)(inner + 0x24) == 4) {
                MK3OBJ *other = obj->field08;
                other->field30 = other->field30 & ~4u;

                if ((obj->field00->field10 & 1) == 0)
                    return mk3_install(thread, (MK3THREADFUNC)t_separate_us);
            }
        }

        set_no_block(obj);
        rsnd_func(obj, 0xa);

        obj->field40 = 0x1c;
        get_char_ani(obj);

        obj->a10 = obj->field40;          /* corrected: [thread->proc, #0x44] */
        obj->field40 = obj->field40 + 0xc;

        do_next_a9_frame(obj);

        *mk3_frame(thread, thread->frame + 1) = 0xc3a;
        thread->fieldfc = 3;
        return 3;
    }

    if (token != 0)
        return -3;

    obj->field1c = 4;
    create_blood_proc(obj);

    combo_setup(obj);
    obj->field38 = 0;
    obj->field30 = (uint32_t)(uintptr_t)t_combo_airborn_hit;
    obj->field34 = 5;

    *mk3_frame(thread, thread->frame + 1) = 0xc27;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


void get_my_strength(MK3OBJ *obj);
void lights_on_slam(MK3OBJ *obj);
void player_froze_pal(MK3OBJ *obj);
void pose2_a9_manual(MK3OBJ *obj);
void me_in_back(MK3OBJ *obj);


/* ------------------------------------------------------------------- t_r_freeze
 *
 * armv7 0x00045fac, five hundred and twelve bytes -- six states, the most
 * of any reaction in this batch, and one real fall-through between two of
 * them (state 0's `field18 == 0x610` case runs straight into state
 * `0x11bd`'s own body without re-entering through the token dispatch --
 * modelled here with a `goto` to keep that one true rather than
 * duplicating or re-shaping it).
 *
 *      state 0
 *          lights_on_slam(obj)
 *          obj->field20 = 6 ; obj->field00->field48 = 6
 *          reaction_start_chores(obj)
 *          dec_my_p_hit(obj)
 *          obj->field1c = obj->field00->field18
 *          if (obj->field1c == 0x610) {
 *              obj->field38 = t_r_freeze
 *              takeover_him(obj)
 *              -- falls into state 0x11bd's own body
 *          } else {
 *              stop_me_player(obj) ; set_no_block(obj)
 *              me_in_back(obj) ; get_my_strength(obj)
 *              if (obj->field1c <= 8) {
 *                  obj->field1c = 0x17 ; create_fx(obj)
 *                  -- falls into the ">8" block below
 *              }
 *              obj->field1c = 0x610 ; obj->field00->field18 = 0x610
 *              other = obj->field08
 *              obj->field2c = other->field30 & ~8 ; other->field30 = obj->field2c
 *              player_froze_pal(obj)
 *              other = obj->field08 ; obj->field1c = other->field24
 *              if (other->field24 == 0xa) {
 *                  obj->field1c = other->field2c
 *                  if ((unsigned)(obj->field1c - 0x811) <= 3) {
 *                      obj->field40 = 1 ; pose2_a9_manual(obj)
 *                  }
 *              }
 *              obj->field1c = 4 ; obj->field20 = 3 ; obj->field24 = 2   (both paths)
 *              push t_shake_ob_up                       (0x11b6)
 *          }
 *      state 0x11b6
 *          obj->field1c = 4 ; obj->field20 = 3 ; obj->field24 = 8
 *          push t_shake_ob_up                           (0x11bb: a second shake, then the wait)
 *      state 0x11bb
 *          token = 0x11bd ; thread->fieldfc = 0x60 ; return 0x60
 *      state 0x11bd  (also reached by fall-through, see above)
 *          player_normpal(obj)
 *          am_i_airborn(obj)
 *          if (obj->field5c) {
 *              obj->field20 = 1.0 ; obj->field24 = 0.5
 *              obj->field28 = 5 ; obj->field1c = 0 ; obj->field40 = 30
 *              push t_flight                            (0x11cb)
 *          } else
 *              install t_local_reaction_exit
 *      state 0x11cb
 *          install t_land_on_my_back
 *
 * **`state 0x11b6` re-pushes itself with the same numbers every time --
 * `t_shake_ob_up` is the shiver loop, not a one-shot.** Whatever ends it
 * (a timeout inside `t_shake_ob_up` itself, not read here) is what finally
 * lets the frozen fighter fall through to `state 0x11bd`'s landing check.
 */
long t_r_freeze(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    MK3OBJ *other;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x11bb) {
        *mk3_frame(thread, thread->frame + 1) = 0x11bd;
        thread->fieldfc = 0x60;
        return 0x60;
    }

    if (token == 0x11b6) {
        obj->field1c = 4;
        obj->field20 = 3;
        obj->field24 = 3 + 5;   /* corrected: `adds r3, #5`, not the decrement of the other path */

        /* str r0 with r0 = 0x11bb, the dispatch's first compare (difftest):
         * the second shake resumes into the 0x60-frame wait */
        *mk3_frame(thread, thread->frame + 1) = 0x11bb;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_shake_ob_up;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x11cb)
        return mk3_install(thread, (MK3THREADFUNC)t_land_on_my_back);

    if (token == 0x11bd) {
state_11bd:
        player_normpal(obj);

        am_i_airborn(obj);
        if (obj->field5c != 0) {
            obj->field20 = 0x10000;              /* 1.0 in 16.16 */
            obj->field24 = 0x8000;               /* 0.5 */
            obj->field28 = 5;
            obj->field1c = 0;
            obj->field40 = 5 + 0x19;              /* 30, SCKNOCKDOWN */

            *mk3_frame(thread, thread->frame + 1) = 0x11cb;
            thread->frame = thread->frame + 1;
            mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_flight;
            *mk3_frame(thread, thread->frame + 1) = 0;
            return 0;
        }

        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }

    if (token != 0)
        return -3;

    lights_on_slam(obj);

    obj->field20 = 6;
    obj->field00->field48 = 6;

    reaction_start_chores(obj);
    dec_my_p_hit(obj);

    obj->field1c = obj->field00->field18;

    if (obj->field1c == 0x610) {
        obj->field38 = (uint32_t)(uintptr_t)t_r_freeze;
        takeover_him(obj);
        goto state_11bd;
    }

    stop_me_player(obj);
    set_no_block(obj);
    me_in_back(obj);
    get_my_strength(obj);

    if ((int32_t)obj->field1c <= 8) {
        obj->field1c = 0x17;
        create_fx(obj);
    }

    obj->field1c = 0x610;
    obj->field00->field18 = 0x610;

    other = obj->field08;
    obj->field2c = other->field30 & ~8u;        /* str r3,[r4,#0x2c]: obj's (difftest) */
    other->field30 = obj->field2c;

    player_froze_pal(obj);

    other = obj->field08;
    obj->field1c = other->field24;

    if (other->field24 == 0xa) {
        obj->field1c = other->field2c;
        if ((uint32_t)(obj->field1c - 0x811) <= 3) {
            obj->field40 = 1;
            pose2_a9_manual(obj);
        }
    }
    /* both paths reach 0x46054 (difftest) */
    obj->field1c = 4;
    obj->field20 = 3;
    obj->field24 = 2;

    *mk3_frame(thread, thread->frame + 1) = 0x11b6;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_shake_ob_up;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


void distance_from_ground(MK3OBJ *obj);
void do_first_a9_frame(MK3OBJ *obj);
void set_x_vel_player(MK3OBJ *obj);
void multi_adjust_xy(MK3OBJ *obj);


/* ---------------------------------------------------------------- t_r_fan_lift
 *
 * armv7 0x00049338, four hundred and twenty-eight bytes -- four states by
 * the dispatcher's own count, but the real control flow crosses between
 * them more than once. `state 0x39b` ends by falling into `state 0x3b3`'s
 * own shared tail rather than sleeping on its own account, and inside
 * `state 0x3b3` itself the "still counting down" and "just reset" paths
 * both land on that SAME tail -- one block in the binary, written once
 * here too, not duplicated into two copies that happen to read alike.
 *
 *      state 0
 *          obj->field30 = 0 ; obj->field34 = 0 ; obj->field38 = 0
 *          push t_reaction_start                        (0x382)
 *      state 0x382
 *          obj->field40 = 0x20 ; do_first_a9_frame(obj)
 *          obj->field20 = 0x624 ; obj->field00->field18 = 0x624
 *          set_half_damage(obj)
 *          obj->field34 = 0 ; distance_from_ground(obj)
 *          other = obj->field08
 *          if (obj->field1c <= 0xa8) obj->field34 = -1.0
 *          other->field1c = obj->field34
 *          obj->field1c = 4 ; obj->field20 = 3 ; obj->field24 = 8
 *          push t_shake_ob_up                           (0x39b)
 *      state 0x39b
 *          obj->field34 = 0 ; distance_from_ground(obj)
 *          other = obj->field08
 *          if (obj->field1c <= 0xf0) obj->field34 = -1.0
 *          other->field1c = obj->field34
 *          obj->field1c = 2.0
 *          is_he_flipped(obj)
 *          if (!obj->field5c) obj->field1c = -obj->field1c
 *          -- both paths converge here:
 *          set_x_vel_player(obj)
 *          obj->a10 = 3 ; obj->field40 = 4 ; obj->field48 = 0x40   (one register)
 *          -- falls into the sleep below
 *      state 0x3b3
 *          token = 0x3b3 ; thread->fieldfc = 1 ; return 1       ; the sleep
 *          -- on resume:
 *          if (--obj->a10 > 0) {
 *              distance_from_ground(obj)
 *              if (obj->field1c > 0xf0) {
 *                  other = obj->field08
 *                  obj->field1c = 0 ; other->field1c = 0
 *              }
 *              if (--obj->field48 > 0)
 *                  -- back to the sleep above
 *              else
 *                  install t_local_reaction_exit
 *          } else {
 *              obj->a10 = 3 ; obj->field40 = -obj->field40
 *              obj->field1c = obj->field40 ; obj->field20 = obj->field40
 *              multi_adjust_xy(obj)
 *              -- back to `distance_from_ground` above, a10 freshly reset
 *          }
 */
long t_r_fan_lift(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    MK3OBJ *other;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x382) {
        obj->field40 = 0x20;
        do_first_a9_frame(obj);

        obj->field20 = 0x624;
        obj->field00->field18 = 0x624;

        set_half_damage(obj);

        obj->field34 = 0;
        distance_from_ground(obj);

        other = obj->field08;
        if ((int32_t)obj->field1c <= 0xa8)
            obj->field34 = 0xffff0000;           /* -1.0 in 16.16 */
        other->field1c = obj->field34;

        obj->field1c = 4;
        obj->field20 = 3;
        obj->field24 = 8;

        *mk3_frame(thread, thread->frame + 1) = 0x39b;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_shake_ob_up;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x39b) {
        obj->field34 = 0;
        distance_from_ground(obj);

        other = obj->field08;
        if ((int32_t)obj->field1c <= 0xf0)
            obj->field34 = 0xffff0000;           /* -1.0 in 16.16 */
        other->field1c = obj->field34;

        obj->field1c = 0x20000;                  /* 2.0 in 16.16 */

        is_he_flipped(obj);
        if (obj->field5c == 0)
            obj->field1c = (uint32_t)(-(int32_t)obj->field1c);

        goto fan_lift_launch;
    }

    if (token == 0x3b3) {
        obj->a10 = obj->a10 - 1;
        if ((int32_t)obj->a10 <= 0) {
            obj->a10 = 3;
            obj->field40 = (uint32_t)(-(int32_t)obj->field40);
            obj->field1c = obj->field40;
            obj->field20 = obj->field40;
            multi_adjust_xy(obj);
        }

        /* Both the "still shaking" and "just reset" paths land on this
         * same block in the binary -- one shared tail, not two copies. */
        distance_from_ground(obj);
        if ((int32_t)obj->field1c > 0xf0) {
            other = obj->field08;
            obj->field1c = 0;
            other->field1c = 0;
        }

        obj->field48 = obj->field48 - 1;
        if ((int32_t)obj->field48 > 0) {
            *mk3_frame(thread, thread->frame + 1) = 0x3b3;
            thread->fieldfc = 1;
            return 1;
        }

        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }

    if (token != 0)
        return -3;

    obj->field30 = 0;
    obj->field34 = 0;
    obj->field38 = 0;

    *mk3_frame(thread, thread->frame + 1) = 0x382;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;

fan_lift_launch:
    set_x_vel_player(obj);

    /* one register: movs r3,#3 / adds r3,#1 / adds r3,#0x3c -- constants
     * 3, 4 and 0x40, not field40 + 1 (difftest) */
    obj->a10 = 3;
    obj->field40 = 3 + 1;
    obj->field48 = 3 + 1 + 0x3c;

    *mk3_frame(thread, thread->frame + 1) = 0x3b3;
    thread->fieldfc = 1;
    return 1;
}


/* ------------------------------------------------------------ t_r_floor_ice
 *
 * armv7 0x000495ec, four hundred and ninety-two bytes.
 *
 * The floor-ice wobble: once set up, the victim's `a10` and `field48` are two
 * X positions, and this coroutine slides them back and forth between the two,
 * flipping `field1c` between +1.0 and -1.0 (16.16) each time the current
 * target is reached. States 0x2c3 and 0x2b8 are that back-and-forth; state
 * 0x282 is the one-shot setup that decides which anchor to head for first.
 *
 * `t_slip_sleep` is who actually drives the wobble frame to frame -- this
 * function only PUSHES it, with token 0x2c3 or 0x2b8, and never calls it. It
 * is not decompiled here (see the extern near the top of the file); the two
 * cases this function itself handles for those tokens are reached some other
 * way -- most likely `t_slip_sleep` reinstalling `t_r_floor_ice` under a
 * condition it alone knows -- which is outside what 0x495ec's own bytes say.
 *
 *      state 0x282  (setup)
 *          am_i_joy(obj)
 *          if obj->field5c != 0                    -- a stick is held
 *              goto shared setup block
 *          get_his_floor_ice(obj)                  -- obj->field1c becomes
 *                                                      the opponent's own
 *                                                      floor-ice MK3THREAD*
 *          if obj->field1c == 0
 *              install t_local_reaction_exit
 *          val = mk3_frame((MK3THREAD *)obj->field1c, 9)[0]   -- its frame 9
 *                                                                 token
 *          obj->field20 = val
 *          if val <= 0x20
 *              install t_local_reaction_exit
 *          -- else fall into the same shared setup block
 *
 *      shared setup block
 *          obj->field20 = 0x26 ; obj->field00->field48 = 0x26
 *          set_no_block(obj) ; ground_player(obj)
 *          obj->field1c = 0x628 ; obj->field00->field18 = 0x628
 *          obj->field40 = 0x20
 *          find_ani_part2(obj)
 *          obj->field54 = 0x50
 *          am_i_joy(obj) ; if obj->field5c == 0, obj->field54 = 0x38
 *          obj->field00->field3c = obj->field54
 *          obj->field1c = 8 ; group_sound(obj)
 *          obj->field1c = 4 ; init_anirate(obj)
 *          other = obj->field08
 *          d_a10 = MK3_FIELD0E_S(other) - obj->a10
 *          d_f48 = MK3_FIELD0E_S(other) - obj->field48
 *          obj->field28 = d_f48 ; obj->field24 = d_a10  -- then both abs()'d
 *          if |d_a10| <= |d_f48|
 *              goto "near a10"       -- vel = +1.0 ; push 0x2b8
 *          else
 *              goto "far from a10"   -- vel = -1.0 ; push 0x2c3
 *
 *      state 0x2c3  (heading for a10)
 *          d = MK3_FIELD0E_S(obj->field08) - obj->a10, abs'd, into field24
 *          if d <= 5
 *              goto "near a10"       -- vel = +1.0 ; push 0x2b8
 *          else
 *              push 0x2c3 again, unchanged vel                 (keep going)
 *
 *      state 0x2b8  (heading for field48)
 *          d = MK3_FIELD0E_S(obj->field08) - obj->field48, abs'd, into field24
 *          if d <= 5
 *              goto "far from a10"   -- vel = -1.0 ; push 0x2c3   (turn back)
 *          else
 *              push 0x2b8 again, unchanged vel                  (keep going)
 *
 * Every push in this function targets `t_slip_sleep`, never itself -- this
 * function is only ever the entry and the redirector, not the loop.
 */
long t_r_floor_ice(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    MK3OBJ *other;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    int32_t d;

    if (token == 0x282) {
        am_i_joy(obj);
        if (obj->field5c != 0)
            goto floor_ice_setup;

        get_his_floor_ice(obj);
        if (obj->field1c == 0)
            return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

        obj->field20 = mk3_frame((MK3THREAD *)(uintptr_t)obj->field1c, 9)[0];
        if ((int32_t)obj->field20 <= 0x20)
            return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

floor_ice_setup:
        obj->field20 = 0x26;
        obj->field00->field48 = 0x26;
        set_no_block(obj);
        ground_player(obj);

        obj->field1c = 0x628;
        obj->field00->field18 = 0x628;
        obj->field40 = 0x20;
        find_ani_part2(obj);

        obj->field54 = 0x50;
        am_i_joy(obj);
        if (obj->field5c == 0)
            obj->field54 = 0x38;
        obj->field00->field3c = obj->field54;

        obj->field1c = 8;
        group_sound(obj);
        obj->field1c = 4;
        init_anirate(obj);

        other = obj->field08;
        obj->field28 = MK3_FIELD0E_S(other) - obj->field48;
        obj->field24 = MK3_FIELD0E_S(other) - obj->a10;
        if ((int32_t)obj->field24 < 0)
            obj->field24 = (uint32_t)(-(int32_t)obj->field24);
        if ((int32_t)obj->field28 < 0)
            obj->field28 = (uint32_t)(-(int32_t)obj->field28);

        if ((int32_t)obj->field24 <= (int32_t)obj->field28)
            goto floor_ice_near_a10;
        goto floor_ice_far_a10;
    }

    if (token == 0x2c3) {
        other = obj->field08;
        d = MK3_FIELD0E_S(other) - (int32_t)obj->a10;
        obj->field24 = (uint32_t)(d < 0 ? -d : d);

        if ((int32_t)obj->field24 <= 5)
            goto floor_ice_near_a10;

        *mk3_frame(thread, thread->frame + 1) = 0x2c3;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_slip_sleep;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x2b8) {
        other = obj->field08;
        d = MK3_FIELD0E_S(other) - (int32_t)obj->field48;
        obj->field24 = (uint32_t)(d < 0 ? -d : d);

        if ((int32_t)obj->field24 <= 5)
            goto floor_ice_far_a10;

        *mk3_frame(thread, thread->frame + 1) = 0x2b8;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_slip_sleep;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0)
        return -3;

    obj->field30 = 0;
    obj->field34 = 0;
    obj->field38 = 0;

    *mk3_frame(thread, thread->frame + 1) = 0x282;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;

floor_ice_near_a10:
    obj->field1c = 0x10000;                  /* +1.0 in 16.16 */
    set_x_vel_player(obj);

    *mk3_frame(thread, thread->frame + 1) = 0x2b8;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_slip_sleep;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;

floor_ice_far_a10:
    obj->field1c = 0xffff0000;               /* -1.0 in 16.16 */
    set_x_vel_player(obj);

    *mk3_frame(thread, thread->frame + 1) = 0x2c3;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_slip_sleep;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ----------------------------------------------------------- t_r_ermac_slam
 *
 * armv7 0x00049180, four hundred and forty bytes.
 *
 * A six-state chain, each state pushing the next by name and self-terminating
 * after the last: t_reaction_start -> t_slammed_shake_up -> t_slammed_zoom_up
 * (not decompiled; declared extern near the top of the file) -> t_slammed_slam_down
 * -> a self-resume at token 0x226 after an eight-frame sleep -> t_flight (with
 * the launch numbers for the slam) -> stop_me_player + t_land_on_my_back. Every
 * step just names the next handler; none of the five thread functions this one
 * chains into are re-entered by number the way floor_ice's wobble is.
 *
 *      state 0
 *          obj->field30/34/38 = 0
 *          push t_reaction_start                     (0x211)
 *      state 0x211
 *          push t_slammed_shake_up                    (0x213)
 *      state 0x213
 *          obj->a10 = 8
 *          push t_slammed_zoom_up                     (0x215)
 *      state 0x215
 *          push t_slammed_slam_down                   (0x217)
 *      state 0x217
 *          obj->a10 = 9 ; damage_to_me(obj)
 *          obj->field40 = 0x1e ; find_ani_part2(obj)
 *          obj->field40 += 4 ; do_next_a9_frame(obj)
 *          obj->field48 = 0x0008000c
 *          shake_a11(obj) ; dec_my_p_hit(obj) ; rsnd_func(obj, 0xd)
 *          obj->field1c = 2 ; group_sound(obj)
 *          resume self at token 0x226 after 8 frames (no handler change)
 *      state 0x226
 *          obj->field1c = 0xfffc8000                  (-3.5 in 16.16)
 *          obj->field20 = obj->field1c - 0x48000       (-8.0)
 *          obj->field24 = obj->field20 + 0x86000        (0.375)
 *          obj->field28 = 4 ; obj->field40 += 0x3f
 *          push t_flight                              (0x22e)
 *      state 0x22e
 *          stop_me_player(obj)
 *          install t_land_on_my_back
 */
long t_r_ermac_slam(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x215) {
        *mk3_frame(thread, thread->frame + 1) = 0x217;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_slammed_slam_down;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x226) {
        obj->field1c = 0xfffc8000;               /* -3.5 in 16.16 */
        obj->field34 = 0;
        obj->field20 = obj->field1c - 0x48000;   /* -8.0 */
        obj->field24 = obj->field20 + 0x86000;   /* 0.375 */
        obj->field28 = 4;
        obj->field40 = obj->field28 + 0x3f;

        *mk3_frame(thread, thread->frame + 1) = 0x22e;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x22e) {
        stop_me_player(obj);
        return mk3_install(thread, (MK3THREADFUNC)t_land_on_my_back);
    }

    if (token == 0x217) {
        obj->a10 = 9;
        damage_to_me(obj);

        obj->field40 = 0x1e;
        find_ani_part2(obj);
        obj->field40 = obj->field40 + 4;
        do_next_a9_frame(obj);

        obj->field48 = 0x0008000c;
        shake_a11(obj);
        dec_my_p_hit(obj);
        rsnd_func(obj, 0xd);

        obj->field1c = 2;
        group_sound(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x226;
        thread->fieldfc = 8;
        return 8;
    }

    if (token == 0x211) {
        *mk3_frame(thread, thread->frame + 1) = 0x213;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_slammed_shake_up;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x213) {
        obj->a10 = 8;

        *mk3_frame(thread, thread->frame + 1) = 0x215;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_slammed_zoom_up;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0)
        return -3;

    obj->field30 = 0;
    obj->field34 = 0;
    obj->field38 = 0;

    *mk3_frame(thread, thread->frame + 1) = 0x211;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ----------------------------------------------------------- t_r_ermac_fatal_slam
 *
 * armv7 0x000497d8, six hundred and fifty-two bytes.
 *
 * The fatal-finish twin of `t_r_ermac_slam`: the same three handoff targets
 * -- t_slammed_shake_up, t_slammed_zoom_up (not decompiled, extern near the
 * top of the file), t_slammed_slam_down -- cycled FOUR times instead of
 * once, with `t_death_slam_pause` (decompiled above, in this same file)
 * inserted as a fourth link after every `t_slammed_slam_down`, and
 * `obj->a10 = 8` reset at the start of each of the four cycles. Then a
 * visual beat (inviso, an fx, no collision) and a hard stop into
 * `t_wait_forever`, reached through the pointer slot at 0x000f3724 the same
 * way mkfatal.c's fatalities reach it.
 *
 *      state 0                          center_around_me(obj)
 *          push t_slammed_shake_up                        (0x1ee)
 *      state 0x1ee     obj->a10 = 8
 *          push t_slammed_zoom_up                          (0x1f0)
 *      state 0x1f0
 *          push t_slammed_slam_down                        (0x1f1)
 *      state 0x1f1
 *          push t_death_slam_pause                         (0x1f2)
 *      state 0x1f2     obj->a10 = 8
 *          push t_slammed_zoom_up                          (0x1f5)
 *      state 0x1f5
 *          push t_slammed_slam_down                        (0x1f6)
 *      state 0x1f6
 *          push t_death_slam_pause                         (0x1f7)
 *      state 0x1f7     obj->a10 = 8
 *          push t_slammed_zoom_up                          (0x1fa)
 *      state 0x1fa
 *          push t_slammed_slam_down                        (0x1fb)
 *      state 0x1fb
 *          push t_death_slam_pause                         (0x1fc)
 *      state 0x1fc     obj->a10 = 8
 *          push t_slammed_zoom_up                          (0x1ff)
 *      state 0x1ff
 *          push t_slammed_slam_down                        (0x200)
 *      state 0x200
 *          push t_death_slam_pause                         (0x201)
 *      state 0x201     set_inviso(obj) ; obj->field1c = 0x18 ; create_fx(obj)
 *                      set_nocol(obj)
 *          resume self at token 0x207 after 3 frames
 *      state 0x207     ground_player(obj)
 *          install t_wait_forever                (slot 0x000f3724)
 *
 * The four-fold repeat was checked by hand each time rather than assumed:
 * every "which register still holds the value" question -- r1, r2 or r3
 * surviving untouched from three branches up -- was answered from the raw
 * disassembly, not from the shape of the previous cycle.
 */
long t_r_ermac_fatal_slam(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x1ee) {
        obj->a10 = 8;
        *mk3_frame(thread, thread->frame + 1) = 0x1f0;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_slammed_zoom_up;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x1f0) {
        *mk3_frame(thread, thread->frame + 1) = 0x1f1;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_slammed_slam_down;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x1f1) {
        *mk3_frame(thread, thread->frame + 1) = 0x1f2;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_death_slam_pause;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x1f2) {
        obj->a10 = 8;
        *mk3_frame(thread, thread->frame + 1) = 0x1f5;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_slammed_zoom_up;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x1f5) {
        *mk3_frame(thread, thread->frame + 1) = 0x1f6;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_slammed_slam_down;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x1f6) {
        *mk3_frame(thread, thread->frame + 1) = 0x1f7;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_death_slam_pause;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x1f7) {
        obj->a10 = 8;
        *mk3_frame(thread, thread->frame + 1) = 0x1fa;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_slammed_zoom_up;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x1fa) {
        *mk3_frame(thread, thread->frame + 1) = 0x1fb;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_slammed_slam_down;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x1fb) {
        *mk3_frame(thread, thread->frame + 1) = 0x1fc;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_death_slam_pause;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x1fc) {
        obj->a10 = 8;
        *mk3_frame(thread, thread->frame + 1) = 0x1ff;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_slammed_zoom_up;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x1ff) {
        *mk3_frame(thread, thread->frame + 1) = 0x200;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_slammed_slam_down;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x200) {
        *mk3_frame(thread, thread->frame + 1) = 0x201;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_death_slam_pause;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x201) {
        set_inviso(obj);
        obj->field1c = 0x18;
        create_fx(obj);
        set_nocol(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x207;
        thread->fieldfc = 3;
        return 3;
    }

    if (token == 0x207) {
        ground_player(obj);
        return mk3_install(thread, (MK3THREADFUNC)t_wait_forever);
    }

    if (token != 0)
        return -3;

    center_around_me(obj);

    *mk3_frame(thread, thread->frame + 1) = 0x1ee;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_slammed_shake_up;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------------- t_slip_sleep
 *
 * armv7 0x000441bc, a hundred and fifty-two bytes.
 *
 * The wobble's clock: `t_r_floor_ice` pushes this, and this is what actually
 * counts frames while a fighter slides. State 0 arms it (token := 0x273,
 * sleep one frame); every call after that spends one tick of
 * `obj->field00->field3c` -- a countdown this function only reads and
 * decrements, never sets -- and, as long as it is still running, does not
 * re-push or install anything at all. It just pops: `thread->frame -= 1`,
 * the same "act once, frame comes back down" idiom the one-shot reaction
 * steps use, except spread across as many calls as the countdown has left in
 * it rather than a single call.
 *
 * Two different ways to run out both land on the same place,
 * `t_local_reaction_exit`, reached through its pointer slot at 0x000f3708:
 * the countdown hitting zero, and the stack frame itself being empty (frame
 * <= 0) when there is nothing left to pop into.
 *
 *      state 0
 *          token := 0x273 ; thread->fieldfc = 1 ; return 1
 *      state 0x273
 *          next_anirate(obj)
 *          obj->field1c = obj->field00->field3c - 1
 *          if obj->field1c == 0
 *              install t_local_reaction_exit
 *          obj->field00->field3c = obj->field1c
 *          if thread->frame <= 0
 *              install t_local_reaction_exit
 *          thread->frame -= 1 ; return 0
 */
long t_slip_sleep(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x273;
        thread->fieldfc = 1;
        return 1;
    }

    if (token != 0x273)
        return -3;

    next_anirate(obj);

    obj->field1c = obj->field00->field3c - 1;
    if (obj->field1c == 0)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

    obj->field00->field3c = obj->field1c;

    if ((int32_t)thread->frame <= 0)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

    thread->frame = thread->frame - 1;
    return 0;
}


/* ----------------------------------------------------------- t_slammed_zoom_up
 *
 * armv7 0x00044574, a hundred and fifty-two bytes.
 *
 * State 0 sets up the camera-style zoom the slam chain's name promises --
 * both fighters get a fresh animation rate, the opponent's steeper than
 * obj's own (-4.0 against -3.0, 16.16) -- and then does something this file
 * hasn't done before: it borrows `obj->field1c`, one of its own real fields,
 * as the scratch slot to carry `obj->a10` from the assignment straight into
 * the self-resume's sleep count, so the number of frames slept is whatever
 * `a10` happened to be when this state started (`t_slammed_shake_up` and
 * `t_slammed_slam_down` set it earlier in the same chain).
 *
 * State 0x1c3 does the actual animation step and then just pops -- same
 * "act once, frame comes back down" idiom as `t_slip_sleep` -- unless the
 * stack is already empty, in which case it installs `t_local_reaction_exit`
 * through the pointer slot at 0x000f3708 instead.
 *
 *      state 0
 *          other = obj->field08
 *          obj->field20 = -3.0 ; other->field1c = -4.0 ; other->field20 = -3.0
 *          obj->field1c = obj->a10
 *          token := 0x1c3 ; thread->fieldfc = obj->field1c ; return obj->field1c
 *      state 0x1c3
 *          obj->field40 = 0x1e ; find_ani_part2(obj) ; do_next_a9_frame(obj)
 *          if thread->frame <= 0
 *              install t_local_reaction_exit
 *          thread->frame -= 1 ; return 0
 */
long t_slammed_zoom_up(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    MK3OBJ *other;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x1c3) {
        obj->field40 = 0x1e;
        find_ani_part2(obj);
        do_next_a9_frame(obj);

        if ((int32_t)thread->frame <= 0)
            return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

        thread->frame = thread->frame - 1;
        return 0;
    }

    if (token != 0)
        return -3;

    other = obj->field08;
    obj->field20 = 0xfffd0000;               /* -3.0 in 16.16 */
    other->field1c = 0xfffc0000;             /* -4.0 in 16.16 */
    other->field20 = obj->field20;           /* -3.0 */

    obj->field1c = obj->a10;

    *mk3_frame(thread, thread->frame + 1) = 0x1c3;
    thread->fieldfc = obj->field1c;
    return obj->field1c;
}


/* --------------------------------------------------------------- t_zap_stumble
 *
 * armv7 0x00042448, a hundred and fifty-two bytes.
 *
 * State 0 parks `t_generic_airborn_hit` in `field30` -- the walk-routine
 * slot every airborne-hit reaction in this file sets the same way -- and
 * pushes `t_reaction_start`. State 0x10fd, reached once that returns, is a
 * two-line shake and a plain `mk3_install(t_stumble_back_vel)`.
 *
 *      state 0
 *          obj->field38 = 0 ; obj->field30 = t_generic_airborn_hit
 *          obj->field34 = 6
 *          push t_reaction_start                       (0x10fd)
 *      state 0x10fd
 *          obj->field48 = 0x60006 ; shake_a11(obj)
 *          obj->field1c = 0x40000                       (4.0 in 16.16)
 *          install t_stumble_back_vel
 */
long t_zap_stumble(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x10fd) {
        obj->field48 = 0x60006;
        shake_a11(obj);

        obj->field1c = 0x40000;                  /* 4.0 in 16.16 */
        return mk3_install(thread, (MK3THREADFUNC)t_stumble_back_vel);
    }

    if (token != 0)
        return -3;

    obj->field38 = 0;
    obj->field30 = (uint32_t)(uintptr_t)t_generic_airborn_hit;
    obj->field34 = 6;

    *mk3_frame(thread, thread->frame + 1) = 0x10fd;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------------ t_stumble_back_vel
 *
 * armv7 0x00043df4, two hundred and ninety-two bytes.
 *
 * `t_zap_stumble` installs this directly, and it forks on a stick check the
 * moment it starts: a fighter holding the stick gets pushed toward
 * `t_animate_a9` (pointer slot 0x000f36d0), anyone else toward
 * `t_animate_a0_frames` (0x000f36b8) by way of an intermediate state, 0x1211,
 * that itself pushes `t_d_beware_mframew` (0x000f37ac) next. Both of the
 * chain's tail states, 0x120a and 0x1214, are a bare `mk3_install
 * (t_local_reaction_exit)` (0x000f3708) -- the stumble just runs out.
 *
 * The four pointer slots here were read straight from `__DATA` rather than
 * matched against a prior sighting -- `tools/macho.py`'s `section_for_addr`
 * plus a four-byte little-endian read is enough, and it is faster and less
 * error-prone than grepping for a comment that may not exist yet.
 *
 *      state 0
 *          away_x_vel(obj)
 *          if am_i_joy(obj) != 0
 *              obj->field40 = 0x40020
 *              push t_animate_a9                          (0x120a)
 *          else
 *              obj->field40 = 0x20 ; get_char_ani(obj)
 *              obj->field1c = 0x40003
 *              push t_animate_a0_frames                    (0x1211)
 *      state 0x1211
 *          obj->field1c = 4
 *          push t_d_beware_mframew                          (0x1214)
 *      state 0x120a
 *          install t_local_reaction_exit
 *      state 0x1214
 *          install t_local_reaction_exit
 */
long t_stumble_back_vel(struct MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x120a)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

    if (token == 0x1211) {
        obj->field1c = 4;

        *mk3_frame(thread, thread->frame + 1) = 0x1214;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_d_beware_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x1214)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

    if (token != 0)
        return -3;

    away_x_vel(obj);

    if (am_i_joy(obj) != 0) {
        obj->field40 = 0x40020;

        *mk3_frame(thread, thread->frame + 1) = 0x120a;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_animate_a9;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    obj->field40 = 0x20;
    get_char_ani(obj);
    obj->field1c = 0x40003;

    *mk3_frame(thread, thread->frame + 1) = 0x1211;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_animate_a0_frames;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------------- t_r_sweep
 *
 * armv7 0x00042e3c, a hundred and fifty-two bytes.
 *
 * The same two-state shape as `t_zap_stumble` -- park a walk-routine
 * override in `field30` and push `t_reaction_start`, then a sound and a
 * `group_sound` in the second state -- except this one hands off to
 * `t_sweep3` at the end instead of installing directly.
 *
 *      state 0
 *          obj->field38 = 0 ; obj->field30 = t_generic_airborn_hit
 *          obj->field34 = 1
 *          push t_reaction_start                        (0xde2)
 *      state 0xde2
 *          rsnd_func(obj, 0xc)
 *          obj->field1c = 5 ; group_sound(obj)
 *          push t_sweep3                                  (0)
 */
long t_r_sweep(struct MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0xde2) {
        rsnd_func(obj, 0xc);

        obj->field1c = 5;
        group_sound(obj);

        /* installed in place, not pushed (difftest) */
        return mk3_install(thread, (MK3THREADFUNC)t_sweep3);
    }

    if (token != 0)
        return -3;

    obj->field38 = 0;
    obj->field30 = (uint32_t)(uintptr_t)t_generic_airborn_hit;
    obj->field34 = 1;

    *mk3_frame(thread, thread->frame + 1) = 0xde2;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* -------------------------------------------------------------------- t_sweep3
 *
 * armv7 0x00043c08, four hundred and ninety-two bytes.
 *
 * `t_r_sweep` pushes this. It runs `t_animate_a9` once, then -- depending on
 * whether the stick was held right after `shake_n_sound` -- either jumps
 * straight to `t_mframew`, or spends three separate pushes of `t_d_beware`
 * chained by self-resumes in between (0xdf2 -> 0xdf6 -> 0xdf7 -> 0xdf8 ->
 * 0xdf9 -> 0xdfa -> 0xdfb). 0xdfa resumes at 0xdfb, not 0xdf8: the token is
 * r1, still the dispatch's compare constant 0xdfb -- an earlier reading had
 * it loop back, the differential test showed it does not. 0xdfb and 0xe01
 * both land on the same next step, `t_check_stay_down`.
 *
 * `MK3OBJPROC.field5c` is named here for the first time: `t_sweep3` clears
 * it (state 0xdea's stick-held branch) right after `shake_n_sound`, a single
 * sighting, distinct from `MK3OBJ.field5c` -- the stick flag `am_i_joy`
 * itself sets -- which this same state also reads two lines earlier.
 *
 *      state 0
 *          obj->field40 = 0x5001f
 *          push t_animate_a9                              (0xdea)
 *      state 0xdea
 *          shake_n_sound(obj) ; am_i_joy(obj)
 *          if obj->field5c != 0
 *              obj->field1c = 4 ; push t_mframew            (0xe01)
 *          else
 *              obj->field1c = 4 ; push t_d_beware            (0xdf2)
 *      state 0xdf2
 *          obj->field1c = 0 ; obj->field00->field5c = 0
 *          push t_d_beware                                   (0xdf6)
 *      state 0xdf6
 *          resume self at 0xdf7 after 1 frame
 *      state 0xdf7
 *          push t_d_beware                                    (0xdf8)
 *      state 0xdf8
 *          resume self at 0xdf9 after 1 frame
 *      state 0xdf9
 *          push t_d_beware                                    (0xdfa)
 *      state 0xdfa
 *          resume self at 0xdfb after 1 frame
 *      state 0xdfb, 0xe01
 *          push t_check_stay_down                              (0xe05)
 *      state 0xe05
 *          install t_sweepup_local_reaction_exit
 */
long t_sweep3(struct MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0xdf8) {
        *mk3_frame(thread, thread->frame + 1) = 0xdf9;
        thread->fieldfc = 1;
        return 1;
    }

    if (token == 0xdfb || token == 0xe01) {
        *mk3_frame(thread, thread->frame + 1) = 0xe05;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_check_stay_down;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0xdf9) {
        *mk3_frame(thread, thread->frame + 1) = 0xdfa;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_d_beware;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0xdfa) {
        /* str r1 with r1 = 0xdfb, the dispatch's compare constant (difftest) */
        *mk3_frame(thread, thread->frame + 1) = 0xdfb;
        thread->fieldfc = 1;
        return 1;
    }

    if (token == 0xe05)
        return mk3_install(thread, (MK3THREADFUNC)t_sweepup_local_reaction_exit);

    if (token == 0xdf2) {
        obj->field1c = 0;
        obj->field00->field5c = 0;

        *mk3_frame(thread, thread->frame + 1) = 0xdf6;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_d_beware;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0xdf6) {
        *mk3_frame(thread, thread->frame + 1) = 0xdf7;
        thread->fieldfc = 1;
        return 1;
    }

    if (token == 0xdf7) {
        *mk3_frame(thread, thread->frame + 1) = 0xdf8;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_d_beware;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0xdea) {
        shake_n_sound(obj);
        am_i_joy(obj);

        if (obj->field5c != 0) {
            obj->field1c = 4;

            *mk3_frame(thread, thread->frame + 1) = 0xe01;
            thread->frame = thread->frame + 1;
            mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
            *mk3_frame(thread, thread->frame + 1) = 0;
            return 0;
        }

        obj->field1c = 4;

        *mk3_frame(thread, thread->frame + 1) = 0xdf2;
        thread->frame = thread->frame + 1;
        /* slot 0xf37ac: the mframew flavour of beware (difftest) */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_d_beware_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0)
        return -3;

    obj->field40 = 0x5001f;

    *mk3_frame(thread, thread->frame + 1) = 0xdea;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_animate_a9;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------------- t_r_tusk_zap
 *
 * armv7 0x00046324, a hundred and fifty-two bytes.
 *
 * The same shake-and-park shape as `t_zap_stumble` and `t_r_sweep` -- park
 * `t_generic_airborn_hit` in `field30`, push `t_reaction_start` -- with a
 * `shake_a11` up front this time, and a plain `mk3_install
 * (t_stumble_back_vel)` to close it instead of a push.
 *
 *      state 0
 *          obj->field48 = 0x60006 ; shake_a11(obj)
 *          obj->field38 = 0 ; obj->field30 = t_generic_airborn_hit
 *          obj->field34 = 1
 *          push t_reaction_start                        (0x10cd)
 *      state 0x10cd
 *          obj->field1c = 0 ; his_ochar_sound(obj)
 *          obj->field1c = 0x40000                          (4.0 in 16.16)
 *          install t_stumble_back_vel
 */
long t_r_tusk_zap(struct MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x10cd) {
        /* 0x46396: the sound number (0) goes in BEFORE the call. After it,
         * the call read whatever 0x1c held -- uhq_entry's hit-queue pointer
         * from reaction_start_chores -- as the sound, and the character
         * number came out past ochar_sound_tables: the crash when a
         * fighter took Kabal's zap, two projectiles in the air (Mary). */
        obj->field1c = 0;
        his_ochar_sound(obj);

        obj->field1c = 0x40000;                  /* 4.0 in 16.16 */
        return mk3_install(thread, (MK3THREADFUNC)t_stumble_back_vel);
    }

    if (token != 0)
        return -3;

    obj->field48 = 0x60006;
    shake_a11(obj);

    obj->field38 = 0;
    obj->field30 = (uint32_t)(uintptr_t)t_generic_airborn_hit;
    obj->field34 = 1;

    *mk3_frame(thread, thread->frame + 1) = 0x10cd;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ---------------------------------------------------------- t_r_stick_sweep
 *
 * armv7 0x00047044, a hundred and fifty-two bytes.
 *
 * The odd one out among this shake-and-park family: `field30` is cleared
 * rather than parked with a callback, so there is no walk-routine override
 * here. State 0 pushes `t_reaction_start`; state 0xdd5 plays the sweep sound
 * and grounds the fighter, then installs `t_sweep3` directly -- no
 * intermediate `t_stumble_back_vel` step the way the punch/zap variants take.
 *
 *      state 0
 *          obj->field30 = 0 ; obj->field38 = 0 ; obj->field34 = 1
 *          push t_reaction_start                        (0xdd5)
 *      state 0xdd5
 *          rsnd_func(obj, 0xc)
 *          obj->field1c = 6 ; group_sound(obj)
 *          ground_player(obj)
 *          install t_sweep3
 */
long t_r_stick_sweep(struct MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0xdd5) {
        rsnd_func(obj, 0xc);

        obj->field1c = 6;
        group_sound(obj);

        ground_player(obj);
        return mk3_install(thread, (MK3THREADFUNC)t_sweep3);
    }

    if (token != 0)
        return -3;

    obj->field30 = 0;
    obj->field38 = 0;
    obj->field34 = 1;

    *mk3_frame(thread, thread->frame + 1) = 0xdd5;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------------- t_r_sw_zap
 *
 * armv7 0x000420f4, a hundred and fifty-six bytes.
 *
 * Another of the shake-and-park family, same trio as `t_r_tusk_zap`:
 * park `t_generic_airborn_hit`, push `t_reaction_start`, close with
 * `mk3_install(t_stumble_back_vel)`. No shake_a11 up front this time --
 * just a group_sound at rate 2.
 *
 *      state 0
 *          obj->field1c = 2 ; group_sound(obj)
 *          obj->field38 = 0 ; obj->field30 = t_generic_airborn_hit
 *          obj->field34 = 1
 *          push t_reaction_start                        (0x1116)
 *      state 0x1116
 *          obj->field1c = 0x40000                          (4.0 in 16.16)
 *          install t_stumble_back_vel
 */
long t_r_sw_zap(struct MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x1116) {
        obj->field1c = 0x40000;                  /* 4.0 in 16.16 */
        return mk3_install(thread, (MK3THREADFUNC)t_stumble_back_vel);
    }

    if (token != 0)
        return -3;

    obj->field1c = 2;
    group_sound(obj);

    obj->field38 = 0;
    obj->field30 = (uint32_t)(uintptr_t)t_generic_airborn_hit;
    obj->field34 = 1;

    *mk3_frame(thread, thread->frame + 1) = 0x1116;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ---------------------------------------------------------------- t_r_spit
 *
 * armv7 0x0004666c, a hundred and sixty-four bytes.
 *
 * Same shake-and-park family, one more variant: `obj->field00->field48` gets
 * an action tag (0x626) alongside the usual `field30` park, and the closing
 * install goes to `t_stumble_back` -- a different, shorter stumble than the
 * `t_stumble_back_vel` the zap/punch variants use.
 *
 *      state 0
 *          obj->field00->field48 = 0x626
 *          obj->field1c = 0x10 ; ochar_sound_c(obj, 0x13)
 *          obj->field34 = 0 ; obj->field38 = 0
 *          obj->field30 = t_generic_airborn_hit
 *          push t_reaction_start                        (0x334)
 *      state 0x334
 *          obj->field1c = 2 ; group_sound(obj)
 *          install t_stumble_back
 */
long t_r_spit(struct MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x334) {
        obj->field1c = 2;
        group_sound(obj);
        return mk3_install(thread, (MK3THREADFUNC)t_stumble_back);
    }

    if (token != 0)
        return -3;

    obj->field00->field48 = 0x626;
    obj->field1c = 0x10;
    ochar_sound_c(obj, 0x13);

    obj->field34 = 0;
    obj->field38 = 0;
    obj->field30 = (uint32_t)(uintptr_t)t_generic_airborn_hit;

    *mk3_frame(thread, thread->frame + 1) = 0x334;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* -------------------------------------------------------------- t_r_jax_zap
 *
 * armv7 0x00042358, a hundred and sixty-eight bytes.
 *
 * Shake-and-park again, same closing trio as `t_r_tusk_zap`
 * (t_reaction_start / t_stumble_back_vel) but the walk-routine override
 * parked in `field30` this time is `t_airborn_hit_no_sound`, not
 * `t_generic_airborn_hit` -- the zap already played its own sound via
 * `group_sound` up front, so the airborne-hit follow-up does not need to
 * play another.
 *
 *      state 0
 *          obj->field1c = 2 ; group_sound(obj)
 *          obj->field48 = 0x60006 ; shake_a11(obj)
 *          obj->field38 = 0 ; obj->field30 = t_airborn_hit_no_sound
 *          obj->field34 = 1
 *          push t_reaction_start                        (0x116b)
 *      state 0x116b
 *          obj->field1c = 0x40000                          (4.0 in 16.16)
 *          install t_stumble_back_vel
 */
long t_r_jax_zap(struct MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x116b) {
        obj->field1c = 0x40000;                  /* 4.0 in 16.16 */
        return mk3_install(thread, (MK3THREADFUNC)t_stumble_back_vel);
    }

    if (token != 0)
        return -3;

    obj->field1c = 2;
    group_sound(obj);

    obj->field48 = 0x60006;
    shake_a11(obj);

    obj->field38 = 0;
    obj->field30 = (uint32_t)(uintptr_t)t_airborn_hit_no_sound;
    obj->field34 = 1;

    *mk3_frame(thread, thread->frame + 1) = 0x116b;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ----------------------------------------------------------- t_r_flip_kick
 *
 * armv7 0x00046760, a hundred and sixty-eight bytes.
 *
 * A launch-style reaction, not another shake-and-park: state 0 plays a
 * sound, parks `t_generic_airborn_hit`, and calls `if_shao_then_pass`
 * (Shao Kahn ignores the field30 park the way he ignores several other
 * reaction setups in this file) before pushing straight to state 0xe27,
 * which halves the damage and installs `t_onback3` -- no `t_reaction_start`
 * step in between the way the shake-and-park family takes.
 *
 *      state 0
 *          rsnd_func(obj, 8)
 *          obj->field30 = t_generic_airborn_hit ; obj->field34 = 8
 *          if_shao_then_pass(obj)
 *          obj->field38 = 0
 *          push t_reaction_start                        (0xe27)
 *      state 0xe27
 *          set_half_damage(obj)
 *          obj->field1c = 0x507 ; obj->field00->field18 = 0x507
 *          install t_onback3
 */
long t_r_flip_kick(struct MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0xe27) {
        set_half_damage(obj);

        obj->field1c = 0x507;
        obj->field00->field18 = 0x507;
        return mk3_install(thread, (MK3THREADFUNC)t_onback3);
    }

    if (token != 0)
        return -3;

    rsnd_func(obj, 8);

    obj->field30 = (uint32_t)(uintptr_t)t_generic_airborn_hit;
    obj->field34 = 8;
    if_shao_then_pass(obj);

    obj->field38 = 0;

    *mk3_frame(thread, thread->frame + 1) = 0xe27;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------- t_r_mileena_tele
 *
 * armv7 0x000431a4, a hundred and seventy-two bytes.
 *
 * Shake-and-park again, closing into `t_stumble_back` this time (the
 * shorter stumble `t_r_spit` also uses) rather than `t_stumble_back_vel`.
 *
 *      state 0
 *          obj->field1c = 0x218 ; obj->field00->field48 = 0x218
 *          rsnd_func(obj, 0xa)
 *          obj->field34 = 0 ; obj->field38 = 0
 *          obj->field30 = t_generic_airborn_hit
 *          push t_reaction_start                        (0x2f3)
 *      state 0x2f3
 *          rsnd_react_voice(obj)
 *          obj->field48 = 0x40004 ; shake_a11(obj)
 *          install t_stumble_back
 */
long t_r_mileena_tele(struct MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x2f3) {
        rsnd_react_voice(obj);

        obj->field48 = 0x40004;
        shake_a11(obj);
        return mk3_install(thread, (MK3THREADFUNC)t_stumble_back);
    }

    if (token != 0)
        return -3;

    obj->field1c = 0x218;
    obj->field00->field48 = 0x218;
    rsnd_func(obj, 0xa);

    obj->field34 = 0;
    obj->field38 = 0;
    obj->field30 = (uint32_t)(uintptr_t)t_generic_airborn_hit;

    *mk3_frame(thread, thread->frame + 1) = 0x2f3;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------- t_r_scorp_tele
 *
 * armv7 0x00046ea0, a hundred and seventy-two bytes.
 *
 * Shake-and-park, same trio as `t_r_mileena_tele` (t_generic_airborn_hit /
 * t_reaction_start / t_stumble_back), with `set_half_damage` up front
 * instead of an action-tag write.
 *
 *      state 0
 *          rsnd_func(obj, 0xa) ; set_half_damage(obj)
 *          obj->field34 = 0 ; obj->field38 = 0
 *          obj->field30 = t_generic_airborn_hit
 *          push t_reaction_start                        (0x31e)
 *      state 0x31e
 *          obj->field48 = 0x30003 ; shake_a11(obj)
 *          obj->field1c = 2 ; group_sound(obj)
 *          install t_stumble_back
 */
long t_r_scorp_tele(struct MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x31e) {
        obj->field48 = 0x30003;
        shake_a11(obj);

        obj->field1c = 2;
        group_sound(obj);
        return mk3_install(thread, (MK3THREADFUNC)t_stumble_back);
    }

    if (token != 0)
        return -3;

    rsnd_func(obj, 0xa);
    set_half_damage(obj);

    obj->field34 = 0;
    obj->field38 = 0;
    obj->field30 = (uint32_t)(uintptr_t)t_generic_airborn_hit;

    *mk3_frame(thread, thread->frame + 1) = 0x31e;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------------- t_r_pounce2
 *
 * armv7 0x000444c0, a hundred and eighty bytes.
 *
 * A self-resuming animation loop rather than a shake-and-park: state 0 sets
 * up the animation window (obj->a10 = the frame past it, obj->field40 =
 * eight frames before it) and a repeat count in obj->field48, then falls
 * straight into the shared "advance one frame, sleep three" block that
 * states 0x47b and 0x47f also enter. 0x47b re-enters that same block every
 * time; 0x47f is what decrements the repeat count and either loops back
 * into it (still counting down) or plays the final frame and installs
 * `t_pounce4` (count exhausted).
 *
 *      state 0
 *          obj->field1c = 2 ; group_sound(obj)
 *          obj->field48 = 3
 *          (falls into the shared block below)
 *      shared block (also entered from 0x47f when still counting down)
 *          obj->field40 = 0x1e ; find_ani_part2(obj)
 *          obj->field44 = obj->field40 + 4
 *          obj->field40 = obj->field40 - 8
 *          do_next_a9_frame(obj)
 *          resume self at 0x47b after 3 frames
 *      state 0x47b
 *          obj->field40 = obj->a10 ; do_next_a9_frame(obj)
 *          resume self at 0x47f after 3 frames
 *      state 0x47f
 *          obj->field48 -= 1
 *          if obj->field48 > 0
 *              (back to the shared block)
 *          else
 *              install t_pounce4
 */
long t_r_pounce2(struct MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x47b) {
        obj->field40 = obj->a10;
        do_next_a9_frame(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x47f;
        thread->fieldfc = 3;
        return 3;
    }

    if (token == 0x47f) {
        obj->field48 = obj->field48 - 1;
        if ((int32_t)obj->field48 > 0)
            goto pounce2_animate;

        return mk3_install(thread, (MK3THREADFUNC)t_pounce4);
    }

    if (token != 0)
        return -3;

    obj->field1c = 2;
    group_sound(obj);
    obj->field48 = 3;

pounce2_animate:
    obj->field40 = 0x1e;
    find_ani_part2(obj);

    obj->a10 = obj->field40 + 4;
    obj->field40 = obj->field40 - 8;
    do_next_a9_frame(obj);

    *mk3_frame(thread, thread->frame + 1) = 0x47b;
    thread->fieldfc = 3;
    return 3;
}


/* ---------------------------------------------------------- t_r_zoom_flipped
 *
 * armv7 0x000490c8, a hundred and eighty-four bytes.
 *
 * A launch into `t_flight`, same as the combo/uppercut family: state 0 sets
 * the launch numbers and pushes `t_flight` with token 0x443. State 0x443 is
 * the landing -- it always copies `obj->field00->p_hit` into `obj->field1c`,
 * and only calls `damage_to_me` (accumulating the hit into
 * `obj->field00->field54`, `add_combo_damage`'s own field) when `p_hit` is
 * still 3 or under; past that, `obj->a10` is forced to 8 first so the
 * landing plays through unarmed either way before installing
 * `t_land_on_my_back`.
 *
 *      state 0
 *          obj->field1c = -8.0 ; obj->field20 = -6.0
 *          obj->field24 = 0.40625 ; obj->field28 = 4
 *          push t_flight                                (0x443)
 *      state 0x443
 *          obj->a10 = 0x20
 *          obj->field1c = obj->field00->p_hit
 *          if obj->field1c > 3
 *              obj->a10 = 8
 *          damage_to_me(obj)
 *          obj->field20 = obj->a10 + obj->field00->field54
 *          obj->field00->field54 = obj->field20
 *          install t_land_on_my_back
 */
long t_r_zoom_flipped(struct MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x443) {
        obj->a10 = 0x20;

        obj->field1c = obj->field00->p_hit;
        if ((int32_t)obj->field1c > 3)
            obj->a10 = 8;

        damage_to_me(obj);

        obj->field20 = obj->a10 + obj->field00->field54;
        obj->field00->field54 = obj->field20;
        return mk3_install(thread, (MK3THREADFUNC)t_land_on_my_back);
    }

    if (token != 0)
        return -3;

    obj->field1c = 0xfff80000;               /* -8.0 in 16.16 */
    obj->field20 = obj->field1c + 0x20000;   /* -6.0 */
    obj->field24 = obj->field20 + 0x68000;   /* 0.40625 */
    obj->field28 = 4;

    *mk3_frame(thread, thread->frame + 1) = 0x443;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_flight;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------------ t_r_uppercut
 *
 * armv7 0x00045348, two hundred bytes.
 *
 * State 0 clears `obj->field30` and parks `t_cc_ken_masters` in `obj->own`
 * `field38` -- the MK3OBJ field named for `highest_mpart_ob`, not the
 * MK3OBJPROC handover slot the comment on `t_cc_ken_masters` describes --
 * unless `obj->a10` already holds the sentinel `0xedb00`, in which case the
 * park is skipped and `field38` stays 0. State 0x748 plays the hit and then
 * installs `t_pit_abort` or `t_background_death` depending on that same
 * sentinel check on `obj->a10`, repeated a second time.
 *
 *      state 0
 *          obj->field34 = 4 ; obj->field30 = 0
 *          obj->field38 = t_cc_ken_masters
 *          if obj->a10 == 0xedb00
 *              obj->field38 = 0
 *          push t_reaction_start                        (0x748)
 *      state 0x748
 *          obj->field1c = 1 ; create_blood_proc(obj)
 *          obj->field48 = 0x60006 ; shake_a11(obj)
 *          rsnd_func(obj, 0xa)
 *          if obj->a10 == 0xedb00
 *              install t_background_death
 *          else
 *              install t_pit_abort
 */
long t_r_uppercut(struct MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x748) {
        obj->field1c = 1;
        create_blood_proc(obj);

        obj->field48 = 0x60006;
        shake_a11(obj);

        rsnd_func(obj, 0xa);

        if (obj->a10 == 0xedb00)
            return mk3_install(thread, (MK3THREADFUNC)t_background_death);

        return mk3_install(thread, (MK3THREADFUNC)t_pit_abort);
    }

    if (token != 0)
        return -3;

    obj->field34 = 4;
    obj->field30 = 0;
    obj->field38 = (uint32_t)(uintptr_t)t_cc_ken_masters;

    if (obj->a10 == 0xedb00)
        obj->field38 = 0;

    *mk3_frame(thread, thread->frame + 1) = 0x748;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------------------ t_spear0
 *
 * armv7 0x000464d8, two hundred and sixteen bytes.
 *
 * State 0 sets the opponent's own p_hit-style action tag on both objects
 * (obj->field1c and obj->field00->field48, the same pairing `t_b_sweep`
 * writes together) and clears field30/34/38, then pushes `t_reaction_start`.
 * State 0x404 plays the hit sound and blood, then falls into a shared
 * self-resume tail. State 0x40c reads the OPPONENT'S opponent's action tag
 * -- `obj->field00->field00->field00->field18`, one hop further than
 * `get_his_action` goes, since that function stops at storing it and this
 * reads it inline instead -- and if it isn't 0x11a, replays the sound/blood
 * step once more before installing `t_local_reaction_exit`; if it IS 0x11a,
 * it rejoins the same shared tail state 0x404 uses instead.
 *
 *      state 0
 *          his_ochar_sound(obj)
 *          obj->field1c = 0x1d ; obj->field00->field48 = 0x1d
 *          obj->field30 = 0 ; obj->field34 = 0 ; obj->field38 = 0
 *          push t_reaction_start                        (0x404)
 *      state 0x404
 *          obj->field1c = 2 ; group_sound(obj)
 *          obj->field1c = 0xc ; create_blood_proc(obj)
 *          (falls into the shared tail below)
 *      shared tail (also reached from 0x40c when the tag is 0x11a)
 *          resume self at 0x40c after 2 frames
 *      state 0x40c
 *          obj->field1c = obj->field00->field00->field00->field18
 *          if obj->field1c == 0x11a
 *              (join the shared tail above)
 *          else
 *              install t_local_reaction_exit
 */
long t_spear0(struct MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x40c) {
        obj->field1c = obj->field00->field00->field00->field18;
        if (obj->field1c == 0x11a)
            goto spear0_resume;

        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }

    if (token == 0x404) {
        obj->field1c = 2;
        group_sound(obj);

        obj->field1c = 0xc;
        create_blood_proc(obj);

spear0_resume:
        *mk3_frame(thread, thread->frame + 1) = 0x40c;
        thread->fieldfc = 2;
        return 2;
    }

    if (token != 0)
        return -3;

    his_ochar_sound(obj);

    obj->field1c = 0x1d;
    obj->field00->field48 = 0x1d;

    obj->field30 = 0;
    obj->field34 = 0;
    obj->field38 = 0;

    *mk3_frame(thread, thread->frame + 1) = 0x404;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* -------------------------------------------------------------------- t_r_quake
 *
 * armv7 0x00043960, two hundred and sixteen bytes.
 *
 * Shake-and-park (t_generic_airborn_hit / t_reaction_start), but the state
 * after t_reaction_start pushes t_animate_a9 itself rather than installing
 * a stumble, and only after THAT returns does it install
 * t_local_reaction_exit -- one more link in the chain than the zap variants
 * take.
 *
 *      state 0
 *          obj->field1c = 0x113 ; obj->field00->field48 = 0x113
 *          obj->field38 = 0 ; obj->field30 = t_generic_airborn_hit
 *          obj->field34 = 6
 *          push t_reaction_start                        (0x1125)
 *      state 0x1125
 *          obj->field1c = 2 ; group_sound(obj)
 *          obj->field1c = 0x28000                          (2.5 in 16.16)
 *          away_x_vel(obj)
 *          obj->a10 = 0x14 ; obj->field40 = 0x40020
 *          push t_animate_a9                              (0x112e)
 *      state 0x112e
 *          install t_local_reaction_exit
 */
long t_r_quake(struct MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x112e)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

    if (token == 0x1125) {
        obj->field1c = 2;
        group_sound(obj);

        obj->field1c = 0x28000;                  /* 2.5 in 16.16 */
        away_x_vel(obj);

        obj->a10 = 0x14;
        obj->field40 = 0x40020;

        *mk3_frame(thread, thread->frame + 1) = 0x112e;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_animate_a9;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0)
        return -3;

    obj->field1c = 0x113;
    obj->field00->field48 = 0x113;

    obj->field38 = 0;
    obj->field30 = (uint32_t)(uintptr_t)t_generic_airborn_hit;
    obj->field34 = 6;

    *mk3_frame(thread, thread->frame + 1) = 0x1125;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------------- t_r_tusk_saw
 *
 * armv7 0x000455ac, two hundred and twenty bytes.
 *
 * State 0 clears all three park fields (no walk-routine override here) and
 * pushes `t_reaction_start`. State 0x41f is the hit itself -- blood, sound,
 * a shake and an animation rate -- then pushes `t_animate_a0_frames`
 * (0x000f36b8); state 0x42e installs `t_local_reaction_exit` once that's
 * done, the same "one more link" shape `t_r_quake` takes.
 *
 *      state 0
 *          obj->field30 = 0 ; obj->field34 = 0 ; obj->field38 = 0
 *          push t_reaction_start                        (0x41f)
 *      state 0x41f
 *          obj->field1c = 9 ; create_blood_proc(obj)
 *          obj->field1c = 2 ; group_sound(obj)
 *          obj->field40 = 0x20 ; find_ani_part2(obj)
 *          obj->field48 = 0x3000c ; shake_a11(obj)
 *          obj->field1c = 0x4000d
 *          push t_animate_a0_frames                      (0x42e)
 *      state 0x42e
 *          install t_local_reaction_exit
 */
long t_r_tusk_saw(struct MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x42e)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

    if (token == 0x41f) {
        obj->field1c = 9;
        create_blood_proc(obj);

        obj->field1c = 2;
        group_sound(obj);

        obj->field40 = 0x20;
        find_ani_part2(obj);

        obj->field48 = 0x3000c;
        shake_a11(obj);

        obj->field1c = 0x4000d;

        *mk3_frame(thread, thread->frame + 1) = 0x42e;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_animate_a0_frames;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0)
        return -3;

    obj->field30 = 0;
    obj->field34 = 0;
    obj->field38 = 0;

    *mk3_frame(thread, thread->frame + 1) = 0x41f;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* -------------------------------------------------------------------- t_r_summon
 *
 * armv7 0x000487d4, two hundred and twenty bytes.
 *
 * A launch-into-t_flight reaction with the flip pair `face_opponent` /
 * `flip_multi` up front -- the same "turn to face them, then flip the
 * multi-part object" combination other reactions in this family use before
 * a knockdown. State 0x5de is where t_flight hands back, installing
 * `t_reaction_land`.
 *
 *      state 0
 *          obj->field30 = 0 ; obj->field38 = 0 ; obj->field34 = 1
 *          push t_reaction_start                        (0x5d2)
 *      state 0x5d2
 *          obj->field1c = 2 ; group_sound(obj)
 *          face_opponent(obj) ; flip_multi(obj)
 *          obj->field1c = -3.0 ; obj->field20 = -9.0
 *          obj->field24 = 0.34375 ; obj->field28 = 5
 *          obj->field40 += 0x19
 *          push t_flight                                (0x5de)
 *      state 0x5de
 *          install t_reaction_land
 */
long t_r_summon(struct MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x5de)
        return mk3_install(thread, (MK3THREADFUNC)t_reaction_land);

    if (token == 0x5d2) {
        obj->field1c = 2;
        group_sound(obj);

        face_opponent(obj);
        flip_multi(obj);

        obj->field1c = 0xfffd0000;               /* -3.0 in 16.16 */
        obj->field20 = obj->field1c - 0x60000;   /* -9.0 */
        obj->field24 = obj->field20 + 0x96000;   /* 0.34375 */
        obj->field28 = 5;
        obj->field40 = obj->field28 + 0x19;

        *mk3_frame(thread, thread->frame + 1) = 0x5de;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0)
        return -3;

    obj->field30 = 0;
    obj->field38 = 0;
    obj->field34 = 1;

    *mk3_frame(thread, thread->frame + 1) = 0x5d2;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------------ t_r_lo_punch
 *
 * armv7 0x00045688, two hundred and fifty-two bytes.
 *
 * State 0 parks the same pair `t_r_hi_punch` parks -- `t_r_airpunch` in
 * field30, but here `t_cc_lo_punch` in field38 instead of `t_cc_hi_punch` --
 * and pushes `t_reaction_start`. State 0xe84 does the actual hit (react
 * voice, sound, dec_my_p_hit, an action tag, a height-gated animation the
 * same way the duck-kick reactions gate theirs) and pushes `t_animate_a9`;
 * state 0xe90 installs `t_local_reaction_exit` once that's done.
 *
 *      state 0
 *          inc_p_block(obj)
 *          obj->field30 = t_r_airpunch ; obj->field34 = 1
 *          obj->field38 = t_cc_lo_punch
 *          push t_reaction_start                        (0xe84)
 *      state 0xe84
 *          rsnd_react_voice(obj) ; rsnd_func(obj, 0xc)
 *          dec_my_p_hit(obj)
 *          obj->field1c = 0x509 ; obj->field00->field18 = 0x509
 *          obj->field40 = 0x3001d
 *          if am_i_short(obj) != 0
 *              obj->field40 = 0x30007
 *          push t_animate_a9                              (0xe90)
 *      state 0xe90
 *          install t_local_reaction_exit
 */
long t_r_lo_punch(struct MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0xe90)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

    if (token == 0xe84) {
        rsnd_react_voice(obj);
        rsnd_func(obj, 0xc);

        dec_my_p_hit(obj);

        obj->field1c = 0x509;
        obj->field00->field18 = 0x509;
        obj->field40 = 0x3001d;

        if (am_i_short(obj) != 0)
            obj->field40 = 0x30007;

        *mk3_frame(thread, thread->frame + 1) = 0xe90;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_animate_a9;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0)
        return -3;

    inc_p_block(obj);

    obj->field30 = (uint32_t)(uintptr_t)t_r_airpunch;
    obj->field34 = 1;
    obj->field38 = (uint32_t)(uintptr_t)t_cc_lo_punch;

    *mk3_frame(thread, thread->frame + 1) = 0xe84;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------------- t_r_kano_roll
 *
 * armv7 0x00042a34, two hundred and twenty-four bytes.
 *
 * State 0 clears both park fields (no walk-routine override) and pushes
 * t_reaction_start. State 0xef9 is the hit -- shake, sound, the same
 * launch-number shape t_r_summon uses -- then pushes t_flight; state 0xf06
 * installs t_land_on_my_back once t_flight hands back.
 *
 *      state 0
 *          obj->field30 = 0 ; obj->field38 = 0 ; obj->field34 = 3
 *          push t_reaction_start                        (0xef9)
 *      state 0xef9
 *          obj->field48 = 0x60006 ; shake_a11(obj)
 *          obj->field1c = 2 ; group_sound(obj)
 *          rsnd_func(obj, 0xa)
 *          obj->field1c = 4.0 ; obj->field20 = -4.0
 *          obj->field24 = 0.265625 ; obj->field28 = 5
 *          obj->field40 += 0x19
 *          push t_flight                                (0xf06)
 *      state 0xf06
 *          install t_land_on_my_back
 */
long t_r_kano_roll(struct MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0xf06)
        return mk3_install(thread, (MK3THREADFUNC)t_land_on_my_back);

    if (token == 0xef9) {
        obj->field48 = 0x60006;
        shake_a11(obj);

        obj->field1c = 2;
        group_sound(obj);

        rsnd_func(obj, 0xa);

        obj->field1c = 0x40000;                  /* 4.0 in 16.16 */
        obj->field20 = obj->field1c - 0x80000;   /* -4.0 */
        obj->field24 = obj->field20 + 0x44000;   /* 0.265625 */
        obj->field28 = 5;
        obj->field40 = obj->field28 + 0x19;

        *mk3_frame(thread, thread->frame + 1) = 0xf06;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0)
        return -3;

    obj->field30 = 0;
    obj->field38 = 0;
    obj->field34 = 3;

    *mk3_frame(thread, thread->frame + 1) = 0xef9;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* -------------------------------------------------------------- t_r_jax_dash
 *
 * armv7 0x00042fe4, two hundred and twenty-four bytes.
 *
 * Same shape as `t_r_kano_roll` -- clear both park fields, push
 * `t_reaction_start`, land in a launch-into-`t_flight` state -- with its
 * own launch numbers and closing into `t_reaction_land` instead of
 * `t_land_on_my_back`.
 *
 *      state 0
 *          obj->field30 = 0 ; obj->field38 = 0 ; obj->field34 = 2
 *          push t_reaction_start                        (0x59f)
 *      state 0x59f
 *          obj->field1c = 2 ; group_sound(obj)
 *          rsnd_func(obj, 0xa)
 *          obj->field48 = 0x60006 ; shake_a11(obj)
 *          obj->field1c = 8.0 ; obj->field20 = -6.0
 *          obj->field24 = 0.3984375 ; obj->field28 = 5
 *          obj->field40 += 0x19
 *          push t_flight                                (0x5ae)
 *      state 0x5ae
 *          install t_reaction_land
 */
long t_r_jax_dash(struct MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x5ae)
        return mk3_install(thread, (MK3THREADFUNC)t_reaction_land);

    if (token == 0x59f) {
        obj->field1c = 2;
        group_sound(obj);

        rsnd_func(obj, 0xa);

        obj->field48 = 0x60006;
        shake_a11(obj);

        obj->field1c = 0x80000;                  /* 8.0 in 16.16 */
        obj->field20 = obj->field1c - 0xe0000;   /* -6.0 */
        obj->field24 = obj->field20 + 0x66000;   /* 0.3984375 */
        obj->field28 = 5;
        obj->field40 = obj->field28 + 0x19;

        *mk3_frame(thread, thread->frame + 1) = 0x5ae;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0)
        return -3;

    obj->field30 = 0;
    obj->field38 = 0;
    obj->field34 = 2;

    *mk3_frame(thread, thread->frame + 1) = 0x59f;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* -------------------------------------------------------------- t_r_superkang
 *
 * armv7 0x000430c4, two hundred and twenty-four bytes.
 *
 * The same shake-and-launch shape as `t_r_jax_dash` and `t_r_kano_roll` --
 * clear both park fields, push `t_reaction_start`, launch into `t_flight`
 * with this move's own numbers -- closing into `t_land_on_my_back`.
 *
 *      state 0
 *          obj->field30 = 0 ; obj->field38 = 0 ; obj->field34 = 1
 *          push t_reaction_start                        (0x4aa)
 *      state 0x4aa
 *          rsnd_func(obj, 8)
 *          obj->field1c = 2 ; group_sound(obj)
 *          obj->field48 = 0x50005 ; shake_a11(obj)
 *          obj->field1c = 6.0 ; obj->field20 = -3.0
 *          obj->field24 = 0.203125 ; obj->field28 = 5
 *          obj->field40 += 0x19
 *          push t_flight                                (0x4b7)
 *      state 0x4b7
 *          install t_land_on_my_back
 */
long t_r_superkang(struct MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x4b7)
        return mk3_install(thread, (MK3THREADFUNC)t_land_on_my_back);

    if (token == 0x4aa) {
        rsnd_func(obj, 8);

        obj->field1c = 2;
        group_sound(obj);

        obj->field48 = 0x50005;
        shake_a11(obj);

        obj->field1c = 0x60000;                  /* 6.0 in 16.16 */
        obj->field20 = obj->field1c - 0x90000;   /* -3.0 */
        obj->field24 = obj->field20 + 0x34000;   /* 0.203125 */
        obj->field28 = 5;
        obj->field40 = obj->field28 + 0x19;

        *mk3_frame(thread, thread->frame + 1) = 0x4b7;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0)
        return -3;

    obj->field30 = 0;
    obj->field38 = 0;
    obj->field34 = 1;

    *mk3_frame(thread, thread->frame + 1) = 0x4aa;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------------------ t_r_square
 *
 * armv7 0x0004294c, two hundred and thirty-two bytes.
 *
 * Same shake-and-launch family, one more variant: the hit and the park
 * happen together in state 0 (sound, shake, THEN clear field30/38 and set
 * field34) before pushing t_reaction_start; the launch numbers and
 * t_flight push live in the state after that.
 *
 *      state 0
 *          obj->field1c = 2 ; group_sound(obj)
 *          rsnd_func(obj, 0xa)
 *          obj->field48 = 0x80008 ; shake_a11(obj)
 *          obj->field30 = 0 ; obj->field38 = 0 ; obj->field34 = 1
 *          push t_reaction_start                        (0xf36)
 *      state 0xf36
 *          obj->field1c = 5.0 ; obj->field20 = -5.0
 *          obj->field24 = 0.34375 ; obj->field28 = 5
 *          obj->field40 += 0x19
 *          push t_flight                                (0xf3e)
 *      state 0xf3e
 *          install t_land_on_my_back
 */
long t_r_square(struct MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0xf3e)
        return mk3_install(thread, (MK3THREADFUNC)t_land_on_my_back);

    if (token == 0xf36) {
        obj->field1c = 0x50000;                  /* 5.0 in 16.16 */
        obj->field20 = obj->field1c - 0xa0000;   /* -5.0 */
        obj->field24 = obj->field20 + 0x58000;   /* 0.34375 */
        obj->field28 = 5;
        obj->field40 = obj->field28 + 0x19;

        *mk3_frame(thread, thread->frame + 1) = 0xf3e;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0)
        return -3;

    obj->field1c = 2;
    group_sound(obj);

    rsnd_func(obj, 0xa);

    obj->field48 = 0x80008;
    shake_a11(obj);

    obj->field30 = 0;
    obj->field38 = 0;
    obj->field34 = 1;

    *mk3_frame(thread, thread->frame + 1) = 0xf36;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* -------------------------------------------------------------- t_r_sk_charge
 *
 * armv7 0x00043878, two hundred and thirty-two bytes.
 *
 * The zap/jax_zap shake-and-park trio (t_airborn_hit_no_sound /
 * t_reaction_start), but with one more animation step: state 0x1158 sets
 * an X velocity and pushes t_animate_a9 before state 0x115d installs
 * t_local_reaction_exit -- same "one more link" shape as t_r_quake.
 *
 *      state 0
 *          rsnd_func(obj, 0xa)
 *          obj->field1c = 2 ; group_sound(obj)
 *          obj->field48 = 0x60006 ; shake_a11(obj)
 *          obj->field38 = 0 ; obj->field30 = t_airborn_hit_no_sound
 *          obj->field34 = 6
 *          push t_reaction_start                        (0x1158)
 *      state 0x1158
 *          obj->field1c = 5.0 ; away_x_vel(obj)
 *          obj->field40 = 0x30020
 *          push t_animate_a9                              (0x115d)
 *      state 0x115d
 *          install t_local_reaction_exit
 */
long t_r_sk_charge(struct MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x115d)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

    if (token == 0x1158) {
        obj->field1c = 0x50000;                  /* 5.0 in 16.16 */
        away_x_vel(obj);

        obj->field40 = 0x30020;

        *mk3_frame(thread, thread->frame + 1) = 0x115d;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_animate_a9;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0)
        return -3;

    rsnd_func(obj, 0xa);

    obj->field1c = 2;
    group_sound(obj);

    obj->field48 = 0x60006;
    shake_a11(obj);

    obj->field38 = 0;
    obj->field30 = (uint32_t)(uintptr_t)t_airborn_hit_no_sound;
    obj->field34 = 6;

    *mk3_frame(thread, thread->frame + 1) = 0x1158;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------------- t_r_lo_kick
 *
 * armv7 0x00044a24, two hundred and thirty-two bytes.
 *
 * The generic-airborn-hit shake-and-park chain, one more link: push
 * t_reaction_start, then in the next state set an X velocity and a
 * height-gated animation (the same am_i_short gate t_r_hi_kick and
 * t_r_lo_punch use) before pushing t_animate_a9; the state after that
 * installs t_local_reaction_exit.
 *
 *      state 0
 *          rsnd_func(obj, 0xc) ; rsnd_react_voice(obj)
 *          obj->field38 = 0 ; obj->field30 = t_generic_airborn_hit
 *          obj->field34 = 1
 *          push t_reaction_start                        (0x62d)
 *      state 0x62d
 *          obj->field1c = 4.0 ; away_x_vel(obj)
 *          obj->field40 = 0x4001d
 *          if am_i_short(obj) != 0
 *              obj->field40 = 0x40007
 *          push t_animate_a9                              (0x637)
 *      state 0x637
 *          install t_local_reaction_exit
 */
long t_r_lo_kick(struct MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x637)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

    if (token == 0x62d) {
        obj->field1c = 0x40000;                  /* 4.0 in 16.16 */
        away_x_vel(obj);

        obj->field40 = 0x4001d;
        if (am_i_short(obj) != 0)
            obj->field40 = 0x40007;

        *mk3_frame(thread, thread->frame + 1) = 0x637;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_animate_a9;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0)
        return -3;

    rsnd_func(obj, 0xc);
    rsnd_react_voice(obj);

    obj->field38 = 0;
    obj->field30 = (uint32_t)(uintptr_t)t_generic_airborn_hit;
    obj->field34 = 1;

    *mk3_frame(thread, thread->frame + 1) = 0x62d;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ----------------------------------------------------------- t_r_robo_bomb
 *
 * armv7 0x000486ec, two hundred and thirty-two bytes.
 *
 * Shake-and-launch with the same face_opponent / flip_multi pair
 * t_r_summon uses, closing into t_reaction_land.
 *
 *      state 0
 *          obj->field30 = 0 ; obj->field38 = 0 ; obj->field34 = 1
 *          push t_reaction_start                        (0x6cc)
 *      state 0x6cc
 *          obj->field1c = 2 ; group_sound(obj)
 *          obj->field48 = 0x6006 ; shake_a11(obj)
 *          face_opponent(obj) ; flip_multi(obj)
 *          obj->field1c = -3.0 ; obj->field20 = -6.0
 *          obj->field24 = 0.71875 ; obj->field28 = 5
 *          obj->field40 += 0x19
 *          push t_flight                                (0x6dc)
 *      state 0x6dc
 *          install t_reaction_land
 */
long t_r_robo_bomb(struct MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x6dc)
        return mk3_install(thread, (MK3THREADFUNC)t_reaction_land);

    if (token == 0x6cc) {
        obj->field1c = 2;
        group_sound(obj);

        obj->field48 = 0x6006;
        shake_a11(obj);

        face_opponent(obj);
        flip_multi(obj);

        obj->field1c = 0xfffd0000;               /* -3.0 in 16.16 */
        obj->field20 = obj->field1c - 0x90000;   /* -6.0 */
        obj->field24 = obj->field20 + 0xc6000;   /* 0.71875 */
        obj->field28 = 5;
        obj->field40 = obj->field28 + 0x19;

        *mk3_frame(thread, thread->frame + 1) = 0x6dc;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0)
        return -3;

    obj->field30 = 0;
    obj->field38 = 0;
    obj->field34 = 1;

    *mk3_frame(thread, thread->frame + 1) = 0x6cc;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* -------------------------------------------------------- t_r_reptile_dash
 *
 * armv7 0x000494e4, two hundred and thirty-two bytes.
 *
 * A stance-and-countdown reaction rather than a park/launch: state 0 forks
 * on `am_i_airborn` -- airborne installs `t_local_reaction_exit` straight
 * away, grounded just clears the park fields and pushes `t_reaction_start`.
 * State 0x306, reached once that returns, is where the stance actually
 * gets set up: `stance_setup` plus a five-tick countdown in `obj->a10`,
 * then it falls into the same self-resume tail state 0x30d also reaches.
 * State 0x30d spends one tick of that countdown per call via `next_anirate`
 * and either resumes itself again or, once the countdown goes negative,
 * installs `t_local_reaction_exit`.
 *
 *      state 0
 *          if am_i_airborn(obj) != 0
 *              install t_local_reaction_exit
 *          obj->field30 = 0 ; obj->field34 = 0 ; obj->field38 = 0
 *          push t_reaction_start                        (0x306)
 *      state 0x306
 *          stance_setup(obj) ; obj->a10 = 0x14
 *          (falls into the shared resume below)
 *      shared resume (also reached from 0x30d when a10 is still >= 0)
 *          resume self at 0x30d after 1 frame
 *      state 0x30d
 *          next_anirate(obj) ; obj->a10 -= 1
 *          if obj->a10 < 0
 *              install t_local_reaction_exit
 *          else
 *              (back to the shared resume above)
 */
long t_r_reptile_dash(struct MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x306) {
        stance_setup(obj);
        obj->a10 = 0x14;
        goto reptile_dash_resume;
    }

    if (token == 0x30d) {
        next_anirate(obj);
        obj->a10 = obj->a10 - 1;
        if ((int32_t)obj->a10 < 0)
            return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

reptile_dash_resume:
        *mk3_frame(thread, thread->frame + 1) = 0x30d;
        thread->fieldfc = 1;
        return 1;
    }

    if (token != 0)
        return -3;

    if (am_i_airborn(obj) != 0)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

    obj->field30 = 0;
    obj->field34 = 0;
    obj->field38 = 0;

    *mk3_frame(thread, thread->frame + 1) = 0x306;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------------------- t_r_slide
 *
 * armv7 0x00043b1c, two hundred and thirty-six bytes.
 *
 * Shake-and-launch, this move's own numbers -- state 0 clears both park
 * fields and pushes t_reaction_start; state 0x45a plays the sound and sets
 * the launch velocities before pushing t_flight; state 0x466 installs
 * t_land_on_my_back once t_flight hands back.
 *
 *      state 0
 *          obj->field30 = 0 ; obj->field38 = 0 ; obj->field34 = 7
 *          push t_reaction_start                        (0x45a)
 *      state 0x45a
 *          obj->field1c = 5 ; group_sound(obj)
 *          obj->field1c = 3.0 ; away_x_vel(obj)
 *          obj->field1c = 3.5 ; obj->field20 = -6.0
 *          obj->field24 = 0.375 ; obj->field28 = 4 ; obj->field34 = 0
 *          obj->field40 += 0x1a
 *          push t_flight                                (0x466)
 *      state 0x466
 *          install t_land_on_my_back
 */
long t_r_slide(struct MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x466)
        return mk3_install(thread, (MK3THREADFUNC)t_land_on_my_back);

    if (token == 0x45a) {
        obj->field1c = 5;
        group_sound(obj);

        obj->field1c = 0x30000;                  /* 3.0 in 16.16 */
        away_x_vel(obj);

        obj->field1c = 0x38000;                  /* 3.5 in 16.16 */
        obj->field20 = obj->field1c - 0x98000;   /* -6.0 */
        obj->field24 = obj->field20 + 0x66000;   /* 0.375 */
        obj->field28 = 4;
        obj->field34 = 0;
        obj->field40 = obj->field28 + 0x1a;

        *mk3_frame(thread, thread->frame + 1) = 0x466;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0)
        return -3;

    obj->field30 = 0;
    obj->field38 = 0;
    obj->field34 = 7;

    *mk3_frame(thread, thread->frame + 1) = 0x45a;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------------- t_r_kano_swipe
 *
 * armv7 0x000479ec, two hundred and thirty-six bytes.
 *
 * The t_airborn_hit_no_sound / t_reaction_start shake-and-park chain, with
 * an extra self-resume in the middle: state 0xf50 poses the fighter and
 * sets an X velocity, then resumes itself at 0xf56 after 8 frames rather
 * than pushing a new handler; 0xf56 sets one more animation rate and
 * installs t_stumble_back_vel.
 *
 *      state 0
 *          obj->field1c = 2 ; group_sound(obj)
 *          tsound_func(obj, 0xf)
 *          obj->field1c = 1 ; create_blood_proc(obj)
 *          obj->field34 = 1 ; obj->field38 = 0
 *          obj->field30 = t_airborn_hit_no_sound
 *          push t_reaction_start                        (0xf50)
 *      state 0xf50
 *          obj->field40 = 0x20 ; pose_a9_manual(obj)
 *          obj->field1c = 6.0 ; away_x_vel(obj)
 *          resume self at 0xf56 after 8 frames
 *      state 0xf56
 *          obj->field1c = 4.0
 *          install t_stumble_back_vel
 */
long t_r_kano_swipe(struct MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0xf50) {
        obj->field40 = 0x20;
        pose_a9_manual(obj);

        obj->field1c = 0x60000;                  /* 6.0 in 16.16 */
        away_x_vel(obj);

        *mk3_frame(thread, thread->frame + 1) = 0xf56;
        thread->fieldfc = 8;
        return 8;
    }

    if (token == 0xf56) {
        obj->field1c = 0x40000;                  /* 4.0 in 16.16 */
        return mk3_install(thread, (MK3THREADFUNC)t_stumble_back_vel);
    }

    if (token != 0)
        return -3;

    obj->field1c = 2;
    group_sound(obj);

    tsound_func(obj, 0xf);

    obj->field1c = 1;
    create_blood_proc(obj);

    obj->field34 = 1;
    obj->field38 = 0;
    obj->field30 = (uint32_t)(uintptr_t)t_airborn_hit_no_sound;

    *mk3_frame(thread, thread->frame + 1) = 0xf50;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------------ t_r_roundhouse
 *
 * armv7 0x00044c60, two hundred and forty-four bytes.
 *
 * A hit-and-launch reaction: state 0 plays the hit (blood, sound, shake)
 * and parks t_cc_ken_masters in field38 -- the same handover slot
 * t_cc_ken_masters's own banner describes, since here it's obj->field00
 * being written, not obj's own field38 the way t_r_uppercut wrote it --
 * before pushing t_reaction_start; the state after that is the launch into
 * t_flight, closing into t_land_on_my_back.
 *
 *      state 0
 *          obj->field1c = 0 ; create_blood_proc(obj)
 *          obj->field1c = 2 ; group_sound(obj)
 *          rsnd_func(obj, 0xa)
 *          obj->field48 = 0x60006 ; shake_a11(obj)
 *          obj->field34 = 1 ; obj->field30 = 0
 *          obj->field00->field38 = t_cc_ken_masters
 *          push t_reaction_start                        (0xf1c)
 *      state 0xf1c
 *          obj->field1c = 6.0 ; obj->field20 = -8.0
 *          obj->field24 = 0.53125 ; obj->field28 = 5
 *          obj->field40 += 0x19
 *          push t_flight                                (0xf23)
 *      state 0xf23
 *          install t_land_on_my_back
 */
long t_r_roundhouse(struct MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0xf23)
        return mk3_install(thread, (MK3THREADFUNC)t_land_on_my_back);

    if (token == 0xf1c) {
        obj->field1c = 0x60000;                  /* 6.0 in 16.16 */
        obj->field20 = obj->field1c - 0xe0000;   /* -8.0 */
        obj->field24 = obj->field20 + 0x88000;   /* 0.53125 */
        obj->field28 = 5;
        obj->field40 = obj->field28 + 0x19;

        *mk3_frame(thread, thread->frame + 1) = 0xf23;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0)
        return -3;

    obj->field1c = 0;
    create_blood_proc(obj);

    obj->field1c = 2;
    group_sound(obj);

    rsnd_func(obj, 0xa);

    obj->field48 = 0x60006;
    shake_a11(obj);

    obj->field34 = 1;
    obj->field30 = 0;
    obj->field38 = (uint32_t)(uintptr_t)t_cc_ken_masters;   /* corrected: the object, not field00 */

    *mk3_frame(thread, thread->frame + 1) = 0xf1c;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------------- t_r_lao_spin
 *
 * armv7 0x000469f0, two hundred and forty-four bytes.
 *
 * State 0 clears both park fields and pushes t_reaction_start. State 0x725
 * is the hit -- half damage, an action tag, blood, shake, two sounds --
 * then sets its own launch numbers and pushes t_flight; state 0x738
 * installs t_reaction_land.
 *
 *      state 0
 *          obj->field30 = 0 ; obj->field38 = 0 ; obj->field34 = 4
 *          push t_reaction_start                        (0x725)
 *      state 0x725
 *          set_half_damage(obj) ; obj->field00->field18 = 0x62b
 *          obj->field1c = 1 ; create_blood_proc(obj)
 *          obj->field48 = 0x60006 ; shake_a11(obj)
 *          rsnd_func(obj, 0xa)
 *          obj->field1c = 2 ; group_sound(obj)
 *          obj->field1c = 2.0 ; obj->field20 = -12.0
 *          obj->field24 = 0.375 ; obj->field28 = 5
 *          obj->field40 += 0x19
 *          push t_flight                                (0x738)
 *      state 0x738
 *          install t_reaction_land
 */
long t_r_lao_spin(struct MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x738)
        return mk3_install(thread, (MK3THREADFUNC)t_reaction_land);

    if (token == 0x725) {
        set_half_damage(obj);
        obj->field00->field18 = 0x62b;

        obj->field1c = 1;
        create_blood_proc(obj);

        obj->field48 = 0x60006;
        shake_a11(obj);

        rsnd_func(obj, 0xa);

        obj->field1c = 2;
        group_sound(obj);

        obj->field1c = 0x20000;                  /* 2.0 in 16.16 */
        obj->field20 = obj->field1c - 0xe0000;   /* -12.0 */
        obj->field24 = obj->field20 + 0xc6000;   /* 0.375 */
        obj->field28 = 5;
        obj->field40 = obj->field28 + 0x19;

        *mk3_frame(thread, thread->frame + 1) = 0x738;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0)
        return -3;

    obj->field30 = 0;
    obj->field38 = 0;
    obj->field34 = 4;

    *mk3_frame(thread, thread->frame + 1) = 0x725;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* -------------------------------------------------------------------- t_r_orb
 *
 * armv7 0x00046dac, two hundred and forty-four bytes.
 *
 * State 0 sets an action tag on both objects and clears all three park
 * fields, then pushes t_reaction_start. State 0x345 is the launch (voice,
 * half damage, its own numbers) into t_flight; state 0x351 stops the
 * fighter and installs t_land_on_my_back.
 *
 *      state 0
 *          obj->field20 = 0x625 ; obj->field00->field48 = 0x625
 *          obj->field30 = 0 ; obj->field34 = 0 ; obj->field38 = 0
 *          push t_reaction_start                        (0x345)
 *      state 0x345
 *          rsnd_react_voice(obj) ; set_half_damage(obj)
 *          obj->field34 = 0
 *          obj->field1c = -3.5 ; obj->field20 = -8.0
 *          obj->field24 = 0.375 ; obj->field28 = 4
 *          obj->field40 += 0x3f
 *          push t_flight                                (0x351)
 *      state 0x351
 *          stop_me_player(obj)
 *          install t_land_on_my_back
 */
long t_r_orb(struct MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x351) {
        stop_me_player(obj);
        return mk3_install(thread, (MK3THREADFUNC)t_land_on_my_back);
    }

    if (token == 0x345) {
        rsnd_react_voice(obj);
        set_half_damage(obj);

        obj->field34 = 0;
        obj->field1c = 0xfffc8000;                /* -3.5 in 16.16 */
        obj->field20 = obj->field1c - 0x48000;   /* -8.0 */
        obj->field24 = obj->field20 + 0x86000;   /* 0.375 */
        obj->field28 = 4;
        obj->field40 = obj->field28 + 0x3f;

        *mk3_frame(thread, thread->frame + 1) = 0x351;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0)
        return -3;

    obj->field20 = 0x625;
    obj->field00->field48 = 0x625;

    obj->field30 = 0;
    obj->field34 = 0;
    obj->field38 = 0;

    *mk3_frame(thread, thread->frame + 1) = 0x345;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ---------------------------------------------------------- t_shake_on_my_back
 *
 * armv7 0x00042218, two hundred and forty-eight bytes.
 *
 * A one-shot animation loop that borrows the thread's SECOND stack
 * (`args`/`fieldf8`, `mk3_arg`) to save and restore `obj->field1c` around
 * it, rather than using an object field the way most of this file's
 * countdowns do. State 0 pushes the current field1c, sets up an animation
 * window, and falls into the shared loop body; state 0x158a re-enters it
 * every time, decrementing `obj->a10` (the loop counter) until it reaches
 * zero, then restores field1c from the arg stack and either pops (frame
 * still has a caller) or installs `t_local_reaction_exit` (frame empty).
 *
 * The loop body itself pops field1c's saved value off the arg stack, uses
 * it, and immediately pushes the SAME value back before re-pushing itself
 * -- net no-op on the stack's depth or contents, just how the compiler
 * expressed "peek without disturbing" through the push/pop primitives.
 *
 *      state 0
 *          *mk3_arg(thread, thread->fieldf8++) = obj->field1c
 *          obj->field40 = 0x1e ; find_ani_part2(obj)
 *          obj->field48 = obj->field40
 *          (falls into the shared loop body below)
 *      shared loop body (also entered from 0x158a when a10 is still > 0)
 *          obj->field40 = obj->field48
 *          v = *mk3_arg(thread, --thread->fieldf8)
 *          obj->field1c = v
 *          *mk3_arg(thread, thread->fieldf8++) = v
 *          push t_mframew                                (0x158a)
 *      state 0x158a
 *          obj->a10 -= 1
 *          if obj->a10 > 0
 *              (back to the shared loop body above)
 *          else
 *              obj->field1c = *mk3_arg(thread, --thread->fieldf8)
 *              if thread->frame > 0
 *                  thread->frame -= 1 ; return 0
 *              else
 *                  install t_local_reaction_exit
 */
long t_shake_on_my_back(struct MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    uint32_t v;

    if (token == 0x158a) {
        obj->a10 = obj->a10 - 1;
        if ((int32_t)obj->a10 > 0)
            goto shake_on_my_back_loop;

        thread->fieldf8 = thread->fieldf8 - 1;
        obj->field1c = *mk3_arg(thread, thread->fieldf8);

        if ((int32_t)thread->frame > 0) {
            thread->frame = thread->frame - 1;
            return 0;
        }

        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }

    if (token != 0)
        return -3;

    *mk3_arg(thread, thread->fieldf8) = obj->field1c;
    thread->fieldf8 = thread->fieldf8 + 1;

    obj->field40 = 0x1e;
    find_ani_part2(obj);
    obj->field48 = obj->field40;

shake_on_my_back_loop:
    obj->field40 = obj->field48;

    thread->fieldf8 = thread->fieldf8 - 1;
    v = *mk3_arg(thread, thread->fieldf8);
    obj->field1c = v;
    *mk3_arg(thread, thread->fieldf8) = v;
    thread->fieldf8 = thread->fieldf8 + 1;

    *mk3_frame(thread, thread->frame + 1) = 0x158a;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------------ t_r_mileena_roll
 *
 * armv7 0x00046f4c, two hundred and forty-eight bytes.
 *
 * The same shape as t_r_orb -- action tag on both objects, clear the park
 * fields, push t_reaction_start; then a launch (voice, half damage, sound,
 * its own numbers) into t_flight; installs t_land_on_my_back.
 *
 *      state 0
 *          obj->field20 = 0x219 ; obj->field00->field48 = 0x219
 *          obj->field30 = 0 ; obj->field38 = 0 ; obj->field34 = 1
 *          push t_reaction_start                        (0x2d6)
 *      state 0x2d6
 *          rsnd_react_voice(obj) ; set_half_damage(obj)
 *          rsnd_func(obj, 0xa)
 *          obj->field34 = 0
 *          obj->field1c = -3.5 ; obj->field20 = -8.0
 *          obj->field24 = 0.375 ; obj->field28 = 4
 *          obj->field40 += 0x3f
 *          push t_flight                                (0x2e2)
 *      state 0x2e2
 *          install t_land_on_my_back
 */
long t_r_mileena_roll(struct MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x2e2)
        return mk3_install(thread, (MK3THREADFUNC)t_land_on_my_back);

    if (token == 0x2d6) {
        rsnd_react_voice(obj);
        set_half_damage(obj);

        rsnd_func(obj, 0xa);

        obj->field34 = 0;
        obj->field1c = 0xfffc8000;                /* -3.5 in 16.16 */
        obj->field20 = obj->field1c - 0x48000;   /* -8.0 */
        obj->field24 = obj->field20 + 0x86000;   /* 0.375 */
        obj->field28 = 4;
        obj->field40 = obj->field28 + 0x3f;

        *mk3_frame(thread, thread->frame + 1) = 0x2e2;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0)
        return -3;

    obj->field20 = 0x219;
    obj->field00->field48 = 0x219;

    obj->field30 = 0;
    obj->field38 = 0;
    obj->field34 = 1;

    *mk3_frame(thread, thread->frame + 1) = 0x2d6;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------------ t_up_2_ceiling
 *
 * armv7 0x00047fe8, two hundred and forty-eight bytes.
 *
 * A stick-to-the-ceiling loop: state 0 fires the launch event and gives
 * both objects the same -16.0 rise rate, then falls into a self-resume
 * loop (state 0xafc) that repeatedly checks `distance_off_ground` until it
 * exceeds 0xff -- once it does, the fighter is stopped against the ceiling
 * and given an 80-frame hold before state 0xb07 fires the landing event and
 * either pops (frame still has a caller) or installs `t_local_reaction_exit`.
 *
 *      state 0
 *          MKEvent_Add(4, 0x39, 0, obj->field00->field08)
 *          obj->field1c = 9 ; group_sound(obj)
 *          obj->field1c = -16.0 ; obj->field08->field1c = -16.0
 *          obj->field40 = 0x1e ; get_char_ani(obj)
 *          obj->field1c = 6 ; init_anirate(obj)
 *          (falls into the shared resume below)
 *      shared resume (also reached from 0xafc while still climbing)
 *          resume self at 0xafc after 1 frame
 *      state 0xafc
 *          next_anirate(obj) ; distance_off_ground(obj)
 *          if obj->field1c <= 0xff
 *              (back to the shared resume above)
 *          else
 *              stop_me_player(obj) ; clear_shadow_bit(obj)
 *              obj->field1c = 0x29 ; create_fx(obj)
 *              resume self at 0xb07 after 0x50 frames
 *      state 0xb07
 *          player_normpal(obj)
 *          MKEvent_Add(4, 0x3a, 0, obj->field00->field08)
 *          if thread->frame > 0
 *              thread->frame -= 1 ; return 0
 *          else
 *              install t_local_reaction_exit
 */
long t_up_2_ceiling(struct MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0xafc) {
        next_anirate(obj);
        distance_off_ground(obj);

        if ((int32_t)obj->field1c <= 0xff)
            goto ceiling_resume;

        stop_me_player(obj);
        clear_shadow_bit(obj);

        obj->field1c = 0x29;
        create_fx(obj);

        *mk3_frame(thread, thread->frame + 1) = 0xb07;
        thread->fieldfc = 0x50;
        return 0x50;
    }

    if (token == 0xb07) {
        player_normpal(obj);
        MKEvent_Add(4, 0x3a, 0, obj->field00->field08);

        if ((int32_t)thread->frame > 0) {
            thread->frame = thread->frame - 1;
            return 0;
        }

        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }

    if (token != 0)
        return -3;

    MKEvent_Add(4, 0x39, 0, obj->field00->field08);

    obj->field1c = 9;
    group_sound(obj);

    obj->field1c = 0xfff00000;               /* -16.0 in 16.16 */
    obj->field08->field1c = obj->field1c;

    obj->field40 = 0x1e;
    get_char_ani(obj);

    obj->field1c = 6;
    init_anirate(obj);

ceiling_resume:
    *mk3_frame(thread, thread->frame + 1) = 0xafc;
    thread->fieldfc = 1;
    return 1;
}


/* -------------------------------------------------------------------- t_r_pounce
 *
 * armv7 0x00043fb0, two hundred and sixty-four bytes.
 *
 * A four-state chain that pushes `t_mframew` twice running -- once with
 * token 0x495, once with 0x497 -- before installing `t_pounce4`. State 0
 * calls `if_shao_then_pass` the way `t_r_flip_kick` does.
 *
 *      state 0
 *          obj->field34 = 1 ; if_shao_then_pass(obj)
 *          obj->field30 = 0 ; obj->field38 = 0
 *          push t_reaction_start                        (0x48a)
 *      state 0x48a
 *          obj->field00->field18 = 0x30b
 *          obj->field1c = 2 ; group_sound(obj)
 *          obj->field40 = 0x1e ; get_char_ani(obj)
 *          obj->field1c = 2
 *          push t_mframew                                (0x495)
 *      state 0x495
 *          obj->field1c = 2
 *          push t_mframew                                (0x497)
 *      state 0x497
 *          install t_pounce4
 */
long t_r_pounce(struct MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x497)
        return mk3_install(thread, (MK3THREADFUNC)t_pounce4);

    if (token == 0x495) {
        obj->field1c = 2;

        *mk3_frame(thread, thread->frame + 1) = 0x497;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x48a) {
        obj->field00->field18 = 0x30b;

        obj->field1c = 2;
        group_sound(obj);

        obj->field40 = 0x1e;
        get_char_ani(obj);

        obj->field1c = 2;

        *mk3_frame(thread, thread->frame + 1) = 0x495;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0)
        return -3;

    obj->field34 = 1;
    if_shao_then_pass(obj);

    obj->field30 = 0;
    obj->field38 = 0;

    *mk3_frame(thread, thread->frame + 1) = 0x48a;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* -------------------------------------------------------------- t_r_robo_tele
 *
 * armv7 0x000488b0, two hundred and seventy-two bytes.
 *
 * A three-state chain, shake-and-park shaped but the hit and the launch
 * setup happen together in state 0. It oddly writes `zero_turbo_bar`'s own
 * address into
 * `obj->field1c` (a pointer slot read straight, not a value computed from
 * it -- the binary really does put a code address into what is everywhere
 * else in this file a fixed-point rate) before pushing straight to the
 * launch state.
 *
 *      state 0
 *          obj->field1c = 2 ; group_sound(obj)
 *          rsnd_func(obj, 0xa)
 *          obj->field48 = 0x60006 ; shake_a11(obj)
 *          obj->field1c = (uintptr_t)zero_turbo_bar
 *          call_a0_for_him(obj)
 *          obj->field20 = 0x208 ; obj->field00->field48 = 0x208
 *          obj->field30 = 0 ; obj->field38 = 0 ; obj->field34 = 4
 *          push t_reaction_start                        (0x6bb)
 *      state 0x6bb
 *          obj->field00->field18 = 0x616
 *          obj->field1c = 1.0 ; obj->field20 = -11.0
 *          obj->field24 = 0.34375 ; obj->field28 = 5
 *          obj->field40 += 0x19
 *          push t_flight                                (0x6c4)
 *      state 0x6c4
 *          install t_reaction_land
 */
long t_r_robo_tele(struct MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x6c4)
        return mk3_install(thread, (MK3THREADFUNC)t_reaction_land);

    if (token == 0x6bb) {
        obj->field00->field18 = 0x616;

        obj->field1c = 0x10000;                  /* 1.0 in 16.16 */
        obj->field20 = obj->field1c - 0xc0000;   /* -11.0 */
        obj->field24 = obj->field20 + 0xb6000;   /* 0.34375 */
        obj->field28 = 5;
        obj->field40 = obj->field28 + 0x19;

        *mk3_frame(thread, thread->frame + 1) = 0x6c4;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0)
        return -3;

    obj->field1c = 2;
    group_sound(obj);

    rsnd_func(obj, 0xa);

    obj->field48 = 0x60006;
    shake_a11(obj);

    obj->field1c = (uint32_t)(uintptr_t)zero_turbo_bar;
    call_a0_for_him(obj);

    obj->field20 = 0x208;
    obj->field00->field48 = 0x208;

    obj->field30 = 0;
    obj->field38 = 0;
    obj->field34 = 4;

    *mk3_frame(thread, thread->frame + 1) = 0x6bb;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* -------------------------------------------------------------- t_r_swat_gun
 *
 * armv7 0x00048478, two hundred and seventy-six bytes.
 *
 * State 0 clears the park fields and pushes t_reaction_start. State 0x242
 * does the hit and sets two independent countdowns -- obj->field48 (a
 * per-tick counter) and obj->a10 (how many times to let it run dry before
 * playing blood) -- then falls straight into the self-resume tail. State
 * 0x255 spends one a10 tick per call: while it still has ticks left it
 * runs the per-frame countdown (next_anirate, obj->field48), and once
 * field48 itself runs out it installs t_local_reaction_exit; if a10 itself
 * runs dry first, blood and a sound play and both counters reset before
 * falling into the same per-frame tick.
 *
 *      state 0
 *          obj->field30 = 0 ; obj->field34 = 0 ; obj->field38 = 0
 *          push t_reaction_start                        (0x242)
 *      state 0x242
 *          at_least_ground_level(obj) ; set_half_damage(obj)
 *          obj->field48 = 0x5000a ; shake_a11(obj)
 *          obj->field1c = 3.0 ; away_x_vel(obj)
 *          death_scream(obj)
 *          obj->field40 = 0x20 ; find_ani_part2(obj)
 *          obj->field1c = 3 ; init_anirate(obj)
 *          obj->field48 = 0x30 ; obj->a10 = 6
 *          (falls into the shared resume below)
 *      shared resume (also reached from the tick below)
 *          resume self at 0x255 after 1 frame
 *      state 0x255
 *          obj->a10 -= 1
 *          if obj->a10 <= 0
 *              obj->field1c = 5 ; create_blood_proc(obj)
 *              rsnd_func(obj, 3)
 *              obj->a10 = 6
 *          (falls into the shared tick below)
 *      shared tick (also reached from 0x255 when a10 was still > 0)
 *          next_anirate(obj)
 *          obj->field48 -= 1
 *          if obj->field48 <= 0
 *              install t_local_reaction_exit
 *          else
 *              (back to the shared resume above)
 */
long t_r_swat_gun(struct MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x255) {
        obj->a10 = obj->a10 - 1;
        if ((int32_t)obj->a10 <= 0) {
            obj->field1c = 5;
            create_blood_proc(obj);

            rsnd_func(obj, 3);
            obj->a10 = 6;
        }

        next_anirate(obj);
        obj->field48 = obj->field48 - 1;
        if ((int32_t)obj->field48 <= 0)
            return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

        goto swat_gun_resume;
    }

    if (token == 0x242) {
        at_least_ground_level(obj);
        set_half_damage(obj);

        obj->field48 = 0x5000a;
        shake_a11(obj);

        obj->field1c = 0x30000;                  /* 3.0 in 16.16 */
        away_x_vel(obj);

        death_scream(obj);

        obj->field40 = 0x20;
        find_ani_part2(obj);

        obj->field1c = 3;
        init_anirate(obj);

        obj->field48 = 0x30;
        obj->a10 = 6;

swat_gun_resume:
        *mk3_frame(thread, thread->frame + 1) = 0x255;
        thread->fieldfc = 1;
        return 1;
    }

    if (token != 0)
        return -3;

    obj->field30 = 0;
    obj->field34 = 0;
    obj->field38 = 0;

    *mk3_frame(thread, thread->frame + 1) = 0x242;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* -------------------------------------------------------------------- t_rek3
 *
 * armv7 0x00045878, two hundred and fifty-six bytes.
 *
 * Shake-and-park (t_generic_airborn_hit / t_reaction_start) with blood up
 * front, then a chain: state 0xda5 pushes t_animate_a9 with token 0xdab;
 * state 0xdab pushes a token (0xdac) for a handler read through its own
 * pointer slot -- that next handler is what actually reacts to 0xdac, not
 * t_rek3 itself.
 *
 *      state 0
 *          obj->field1c = 4 ; create_blood_proc(obj)
 *          rsnd_react_voice(obj)
 *          obj->field48 = 0x40004 ; shake_a11(obj)
 *          obj->field38 = 0 ; obj->field30 = t_generic_airborn_hit
 *          obj->field34 = 1
 *          push t_reaction_start                        (0xda5)
 *      state 0xda5
 *          set_no_block(obj)
 *          obj->field1c = 1.0 ; away_x_vel(obj)
 *          obj->field40 = 0x4001c
 *          push t_animate_a9                              (0xdab)
 *      state 0xdab
 *          push (slot 0x000add6e)                          (0xdac)
 */
long t_rek3(struct MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0xdab) {
        *mk3_frame(thread, thread->frame + 1) = 0xdac;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_local_reaction_exit;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0xda5) {
        set_no_block(obj);

        obj->field1c = 0x10000;                  /* 1.0 in 16.16 */
        away_x_vel(obj);

        obj->field40 = 0x4001c;

        *mk3_frame(thread, thread->frame + 1) = 0xdab;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_animate_a9;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0)
        return -3;

    obj->field1c = 4;
    create_blood_proc(obj);

    rsnd_react_voice(obj);

    obj->field48 = 0x40004;
    shake_a11(obj);

    obj->field38 = 0;
    obj->field30 = (uint32_t)(uintptr_t)t_generic_airborn_hit;
    obj->field34 = 1;

    *mk3_frame(thread, thread->frame + 1) = 0xda5;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* -------------------------------------------------------------------- t_rst5
 *
 * armv7 0x000473d0, four hundred and sixty-four bytes. `t_reaction_start`
 * installs this, per its own banner, and this is the busiest dispatcher in
 * the file: it decides between three DIFFERENT reaction chains parked in
 * obj's own fields, rather than pushing one fixed next step.
 *
 * State 0 (and the field34-dispatch it shares with the 0x1038 resume)
 * checks, in order: `obj->field30` -- a handler another routine parked
 * there (airborne hit reactions store `t_r_airpunch`-style targets here) --
 * gated behind `am_i_airborn`; then `obj->field38`, a handler another
 * routine parked there directly (`t_cc_ken_masters`-style); then
 * `obj->field34` as a dispatch index into whichever of two data tables
 * (0x0017b8d0 or 0x0017b884) the opponent's own `field24` selects, neither
 * traced further -- this is the same kind of `tbb`-style jump table
 * `factdiff.py`'s known-gaps section already flags for `t_background_death`.
 * Every dead end lands on `t_local_reaction_exit`.
 *
 * The "shift" seen twice -- copying `mk3_frame(thread, frame+2)`'s token
 * down into `frame+1` and `frame+1`'s handler down into `frame` -- happens
 * right before an `mk3_install` that immediately re-zeroes the very token
 * slot the shift just wrote, so the token half of the shift has no lasting
 * effect; kept faithful to the exact sequence rather than trimmed.
 *
 *      state 0
 *          lights_on_hit(obj)
 *          if obj->field30 != 0 and am_i_airborn(obj) != 0
 *              if thread->frame > 0: thread->frame -= 1
 *              shift ; install obj->field30
 *          (falls into the field38 check below when field30 == 0, or when
 *           field30 != 0 but not airborne)
 *      field38 check
 *          if obj->field38 != 0
 *              *mk3_arg(thread, thread->fieldf8++) = obj->field34
 *              push obj->field38                             (0x1038)
 *          else if obj->field34 == 0
 *              if thread->frame > 0: thread->frame -= 1 ; return 0
 *              else: install t_local_reaction_exit
 *          else
 *              obj->field20 = 0x0017b8d0
 *              obj->field38 = obj->field08->field24
 *              if obj->field38 != 0x18
 *                  obj->field20 = 0x0017b884           (unconditional)
 *              if obj->field38 == 0x18 or obj->field38 == 0x19
 *                  if thread->frame > 0: thread->frame -= 1
 *                  else: install t_local_reaction_exit (result discarded,
 *                        immediately overwritten by the shift+lookup below)
 *                  shift ; obj->field34 = table[obj->field34]
 *                  install obj->field34
 *              else
 *                  if thread->frame > 0: thread->frame -= 1 ; return 0
 *                  else: install t_local_reaction_exit
 *      state 0x1038
 *          obj->field34 = *mk3_arg(thread, --thread->fieldf8)
 *          (rejoin at the field34 test, PAST the field38 check: b 0x4745e.
 *           Re-testing field38 would push its handler again -- difftest)
 */
long t_rst5(struct MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    uint32_t *table;

    if (token == 0x1038) {
        thread->fieldf8 = thread->fieldf8 - 1;
        obj->field34 = *mk3_arg(thread, thread->fieldf8);
        goto rst5_field34_check;        /* b 0x4745e: past the field38 test (difftest) */
    }

    if (token != 0)
        return -3;

    lights_on_hit(obj);

    if (obj->field30 != 0 && am_i_airborn(obj) != 0) {
        if ((int32_t)thread->frame > 0)
            thread->frame = thread->frame - 1;

        *mk3_frame(thread, thread->frame + 1) = *mk3_frame(thread, thread->frame + 2);
        mk3_frame(thread, thread->frame)[1] = mk3_frame(thread, thread->frame + 1)[1];
        return mk3_install(thread, (MK3THREADFUNC)(uintptr_t)obj->field30);
    }

rst5_field38_check:
    if (obj->field38 != 0) {
        *mk3_arg(thread, thread->fieldf8) = obj->field34;
        thread->fieldf8 = thread->fieldf8 + 1;

        *mk3_frame(thread, thread->frame + 1) = 0x1038;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = obj->field38;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

rst5_field34_check:
    if (obj->field34 == 0) {
        if ((int32_t)thread->frame > 0) {
            thread->frame = thread->frame - 1;
            return 0;
        }
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }

    /* The literals are _motaro_branches (0x0017b8d0) and _sk_branches
     * (0x0017b884), the bosses' reaction tables -- the same two mkdrone.c
     * reads, generated with host addresses by tools/logic_tables.py. Written
     * as raw iOS addresses they were read as a table at 0x17b884 of the PC's
     * address space: the first hit reaction against Motaro or Shao Kahn
     * crashed (or, when that page happened to be mapped, ran garbage). */
    obj->field20 = (uint32_t)(uintptr_t)motaro_branches;
    obj->field38 = obj->field08->field24;

    if (obj->field38 != 0x18)
        obj->field20 = (uint32_t)(uintptr_t)sk_branches;

    if (obj->field38 == 0x18 || obj->field38 == 0x19) {
        if ((int32_t)thread->frame > 0) {
            thread->frame = thread->frame - 1;
        } else {
            mk3_frame(thread, thread->frame)[1] =
                (uint32_t)(uintptr_t)t_local_reaction_exit;
            *mk3_frame(thread, thread->frame + 1) = 0;
        }

        *mk3_frame(thread, thread->frame + 1) = *mk3_frame(thread, thread->frame + 2);
        mk3_frame(thread, thread->frame)[1] = mk3_frame(thread, thread->frame + 1)[1];

        table = (uint32_t *)(uintptr_t)obj->field20;
        obj->field34 = table[obj->field34];
        return mk3_install(thread, (MK3THREADFUNC)(uintptr_t)obj->field34);
    }

    if ((int32_t)thread->frame > 0) {
        thread->frame = thread->frame - 1;
        return 0;
    }
    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* ------------------------------------------------------------- t_r_sk_hammer
 *
 * armv7 0x000463bc, two hundred and eighty-four bytes.
 *
 * State 0 parks `t_r_motaro_kick`'s own address in `field30` (not called,
 * just parked the way `t_generic_airborn_hit` is elsewhere) and pushes
 * `t_reaction_start`. State 0xd20 is the actual hit -- two separate
 * `create_blood_proc` calls with different `field1c` values -- then pushes
 * `t_animate_a9`; state 0xd32 stops the fighter and installs
 * `t_dizzy_by_boss`.
 *
 *      state 0
 *          obj->field34 = 1 ; obj->field38 = 0
 *          obj->field30 = t_r_motaro_kick
 *          push t_reaction_start                        (0xd20)
 *      state 0xd20
 *          obj->field1c = 2 ; his_ochar_sound(obj)
 *          obj->field1c = 9 ; group_sound(obj)
 *          obj->field48 = 0xa000a ; shake_a11(obj)
 *          obj->field1c = 4 ; create_blood_proc(obj)
 *          obj->field1c = 1 ; create_blood_proc(obj)
 *          obj->field1c = 3.0 ; away_x_vel(obj)
 *          obj->field40 = 0x40020
 *          push t_animate_a9                              (0xd32)
 *      state 0xd32
 *          stop_me_player(obj)
 *          obj->field1c = 0x620 ; obj->field00->field18 = 0x620
 *          install t_dizzy_by_boss
 */
long t_r_sk_hammer(struct MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0xd32) {
        stop_me_player(obj);

        obj->field1c = 0x620;
        obj->field00->field18 = 0x620;
        return mk3_install(thread, (MK3THREADFUNC)t_dizzy_by_boss);
    }

    if (token == 0xd20) {
        obj->field1c = 2;
        his_ochar_sound(obj);

        obj->field1c = 9;
        group_sound(obj);

        obj->field48 = 0xa000a;
        shake_a11(obj);

        obj->field1c = 4;
        create_blood_proc(obj);

        obj->field1c = 1;
        create_blood_proc(obj);

        obj->field1c = 0x30000;                  /* 3.0 in 16.16 */
        away_x_vel(obj);

        obj->field40 = 0x40020;

        *mk3_frame(thread, thread->frame + 1) = 0xd32;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_animate_a9;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0)
        return -3;

    obj->field34 = 1;
    obj->field38 = 0;
    obj->field30 = (uint32_t)(uintptr_t)t_r_motaro_kick;

    *mk3_frame(thread, thread->frame + 1) = 0xd20;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------------ t_r_motaro_kick
 *
 * armv7 0x00044ef4, four hundred and seventy-six bytes. `t_r_sk_hammer`
 * parks this function's own address in `field30`, but that's a callback
 * park, not a call -- this thread is reached the usual way, through its
 * own push chain.
 *
 * A six-state relay, each state pushing the next by name and never
 * revisiting a token: t_reaction_start -> t_shake_on_my_back ->
 * t_flight -> t_check_winner_status -> t_check_stay_down -> a
 * self-resume -> t_animate_a9 -> install t_local_reaction_exit. Two of
 * the six handoffs (t_shake_on_my_back, t_check_stay_down) are direct
 * pc-relative addresses; the rest go through pointer slots.
 *
 *      state 0
 *          obj->field1c = 4 ; create_blood_proc(obj)
 *          obj->field1c = 1 ; create_blood_proc(obj)
 *          obj->field34 = 1 ; obj->field30 = 0 ; obj->field38 = 0
 *          push t_reaction_start                        (0xd68)
 *      state 0xd68
 *          obj->field1c = 2 ; group_sound(obj)
 *          rsnd_func(obj, 0xa)
 *          obj->field48 = 0xa000a ; shake_a11(obj)
 *          obj->field1c = 8.0 ; obj->field20 = -8.0
 *          obj->field24 = 0.53125 ; obj->field28 = 5
 *          obj->field40 += 0x19
 *          push t_flight                                (0xd75)
 *      state 0xd75
 *          shake_n_sound(obj)
 *          obj->field1c = 3 ; obj->a10 = 1
 *          push t_shake_on_my_back                       (0xd7a)
 *      state 0xd7a
 *          resume self at 0xd7c after 4 frames
 *      state 0xd7c
 *          push t_check_stay_down                         (0xd7e)
 *      state 0xd7e
 *          push t_check_winner_status                      (0xd7f)
 *      state 0xd7f
 *          obj->field40 = 0x40021
 *          push t_animate_a9                                (0xd81)
 *      state 0xd81
 *          install t_local_reaction_exit
 */
long t_r_motaro_kick(struct MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0xd81)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

    if (token == 0xd7f) {
        obj->field40 = 0x40021;

        *mk3_frame(thread, thread->frame + 1) = 0xd81;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_animate_a9;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0xd7e) {
        *mk3_frame(thread, thread->frame + 1) = 0xd7f;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_check_winner_status;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0xd7c) {
        *mk3_frame(thread, thread->frame + 1) = 0xd7e;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_check_stay_down;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0xd7a) {
        *mk3_frame(thread, thread->frame + 1) = 0xd7c;
        thread->fieldfc = 4;
        return 4;
    }

    if (token == 0xd75) {
        shake_n_sound(obj);

        obj->field1c = 3;
        obj->a10 = 1;

        *mk3_frame(thread, thread->frame + 1) = 0xd7a;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_shake_on_my_back;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0xd68) {
        obj->field1c = 2;
        group_sound(obj);

        rsnd_func(obj, 0xa);

        obj->field48 = 0xa000a;
        shake_a11(obj);

        obj->field1c = 0x80000;                  /* 8.0 in 16.16 */
        obj->field20 = obj->field1c - 0x100000;  /* -8.0 */
        obj->field24 = obj->field20 + 0x88000;   /* 0.53125 */
        obj->field28 = 5;
        obj->field40 = obj->field28 + 0x19;

        *mk3_frame(thread, thread->frame + 1) = 0xd75;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0)
        return -3;

    obj->field1c = 4;
    create_blood_proc(obj);

    obj->field1c = 1;
    create_blood_proc(obj);

    obj->field34 = 1;
    obj->field30 = 0;
    obj->field38 = 0;

    *mk3_frame(thread, thread->frame + 1) = 0xd68;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* -------------------------------------------------------------- t_r_sk_air_charge
 *
 * armv7 0x000450d0, three hundred and fifty-two bytes.
 *
 * A five-state relay: t_reaction_start -> the hit (blood x2, sound, shake,
 * launch numbers) -> t_flight -> shake_n_sound + an animation step ->
 * t_mframew -> a self-resume -> install t_getup_reaction_exit.
 *
 *      state 0
 *          obj->field30 = 0 ; obj->field38 = 0 ; obj->field34 = 1
 *          push t_reaction_start                        (0xd3f)
 *      state 0xd3f
 *          obj->field1c = 4 ; create_blood_proc(obj)
 *          obj->field1c = 1 ; create_blood_proc(obj)
 *          obj->field1c = 2 ; group_sound(obj)
 *          rsnd_func(obj, 0xa)
 *          obj->field48 = 0xa000a ; shake_a11(obj)
 *          obj->field1c = 6.0 ; obj->field20 = -6.0
 *          obj->field24 = 0.40625 ; obj->field28 = 5
 *          obj->field40 += 0x19
 *          push t_flight                                (0xd51)
 *      state 0xd51
 *          shake_n_sound(obj)
 *          obj->field40 = 0x1e ; find_ani_part2(obj)
 *          obj->field1c = 6
 *          push t_mframew                                (0xd57)
 *      state 0xd57
 *          resume self at 0xd58 after 4 frames
 *      state 0xd58
 *          install t_getup_reaction_exit
 */
long t_r_sk_air_charge(struct MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0xd58)
        return mk3_install(thread, (MK3THREADFUNC)t_getup_reaction_exit);

    if (token == 0xd57) {
        *mk3_frame(thread, thread->frame + 1) = 0xd58;
        thread->fieldfc = 4;
        return 4;
    }

    if (token == 0xd51) {
        shake_n_sound(obj);

        obj->field40 = 0x1e;
        find_ani_part2(obj);

        obj->field1c = 6;

        *mk3_frame(thread, thread->frame + 1) = 0xd57;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0xd3f) {
        obj->field1c = 4;
        create_blood_proc(obj);

        obj->field1c = 1;
        create_blood_proc(obj);

        obj->field1c = 2;
        group_sound(obj);

        rsnd_func(obj, 0xa);

        obj->field48 = 0xa000a;
        shake_a11(obj);

        obj->field1c = 0x60000;                  /* 6.0 in 16.16 */
        obj->field20 = obj->field1c - 0xc0000;   /* -6.0 */
        obj->field24 = obj->field20 + 0x68000;   /* 0.40625 */
        obj->field28 = 5;
        obj->field40 = obj->field28 + 0x19;

        *mk3_frame(thread, thread->frame + 1) = 0xd51;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0)
        return -3;

    obj->field30 = 0;
    obj->field38 = 0;
    obj->field34 = 1;

    *mk3_frame(thread, thread->frame + 1) = 0xd3f;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------------------- t_r_scream
 *
 * armv7 0x00048d38, four hundred bytes.
 *
 * State 0 clears the park fields and pushes t_reaction_start. State 0x531
 * grounds and poses the fighter, then forks on whether a stick is held --
 * airborne, it also gives the opponent the same 1.0 field1c -- before
 * setting an a10 countdown (either 4 or 0x2a) and self-resuming into
 * state 0x565. That state is the actual scream: unless the opponent is
 * airborne or already at action 0x615 (both dead ends into
 * t_local_reaction_exit), it swaps a fixed-point offset onto the
 * opponent's own field0e/field12 pair, negates obj->field48, and compares
 * heights -- if obj is still below, it grounds again and adjusts x
 * velocity depending on distance and a proc flag, then falls into the a10
 * countdown: each call spends one a10 tick, looping back into the same
 * state until it reaches zero, at which point it installs
 * t_local_reaction_exit.
 *
 *      state 0
 *          obj->field34 = 0 ; obj->field30 = 0 ; obj->field38 = 0
 *          push t_reaction_start                        (0x531)
 *      state 0x531
 *          set_no_block(obj)
 *          obj->field1c = 0x615 ; obj->field00->field18 = 0x615
 *          dec_my_p_hit(obj)
 *          obj->field1c = 0 ; obj->field00->field28 = 0
 *          stop_me_player(obj)
 *          obj->field40 = 0x20 ; pose_a9_manual(obj)
 *          am_i_airborn(obj)
 *          if obj->field5c != 0
 *              obj->field1c = 1.0 ; obj->field08->field1c = 1.0
 *          obj->field00->field48 = 4 ; obj->a10 = 0x2a
 *          resume self at 0x565 after 3 frames
 *      state 0x565
 *          is_he_airborn(obj)
 *          if obj->field5c != 0
 *              stop_me_player(obj) ; ground_player(obj)
 *              install t_local_reaction_exit
 *          get_his_action(obj)
 *          if obj->field20 == 0x615
 *              stop_me_player(obj) ; ground_player(obj)
 *              install t_local_reaction_exit
 *          other = obj->field08
 *          MK3_SET_FIELD0E(other, MK3_FIELD0E_S(other) + obj->field48)
 *          obj->field1c = MK3_FIELD0E_S(other)
 *          obj->field48 = -obj->field48
 *          obj->field1c = MK3_FIELD12_S(other)
 *          obj->field20 = obj->field00->field40
 *          if obj->field20 > obj->field1c
 *              obj->field1c = other->field5c(saved) ; other->field1c = same
 *              ground_player(obj) ; get_x_dist(obj)
 *              if obj->field28 > 0x50
 *                  obj->field1c = obj->field00->field28
 *                  if obj->field1c == 0
 *                      obj->field1c += 2.0 ; towards_x_vel(obj)
 *              else
 *                  obj->field1c = 1 ; obj->field00->field28 = 1
 *                  stop_me_player(obj)
 *          (falls into the a10 countdown below)
 *      a10 countdown (also reached from the field20<=field1c branch above)
 *          obj->a10 -= 1
 *          if obj->a10 != 0
 *              (back to state 0x565's shared merge, resumed at 0x565
 *               after 3 frames)
 *          else
 *              install t_local_reaction_exit
 */
long t_r_scream(struct MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    MK3OBJ *other;
    int32_t stick;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x565) {
        is_he_airborn(obj);
        if (obj->field5c != 0)
            goto scream_dead_end;

        get_his_action(obj);
        if (obj->field20 == 0x615)
            goto scream_dead_end;

        other = obj->field08;
        MK3_SET_FIELD0E(other, MK3_FIELD0E_S(other) + obj->field48);
        obj->field1c = MK3_FIELD0E_S(other);

        obj->field48 = (uint32_t)(-(int32_t)obj->field48);

        obj->field1c = MK3_FIELD12_S(other);
        obj->field20 = obj->field00->field40;

        if ((int32_t)obj->field20 > (int32_t)obj->field1c) {
            stick = obj->field5c;
            obj->field1c = stick;
            other->field1c = stick;

            ground_player(obj);
            get_x_dist(obj);

            if ((int32_t)obj->field28 > 0x50) {
                obj->field1c = obj->field00->field28;
                if (obj->field1c == 0) {
                    obj->field1c = obj->field1c + 0x20000;   /* 2.0 in 16.16 */
                    towards_x_vel(obj);
                }
            } else {
                obj->field1c = 1;
                obj->field00->field28 = 1;
                stop_me_player(obj);
            }
        }

        goto scream_countdown;
    }

    if (token == 0x531) {
        set_no_block(obj);

        obj->field1c = 0x615;
        obj->field00->field18 = 0x615;

        dec_my_p_hit(obj);

        obj->field1c = 0;
        obj->field00->field28 = 0;

        stop_me_player(obj);

        obj->field40 = 0x20;
        pose_a9_manual(obj);

        am_i_airborn(obj);
        if (obj->field5c != 0) {
            obj->field1c = 0x10000;                  /* 1.0 in 16.16 */
            obj->field08->field1c = 0x10000;
        }

        obj->field00->field48 = 4;
        obj->a10 = 4 + 0x26;

        *mk3_frame(thread, thread->frame + 1) = 0x565;
        thread->fieldfc = 3;
        return 3;
    }

    if (token != 0)
        return -3;

    obj->field34 = 0;
    obj->field30 = 0;
    obj->field38 = 0;

    *mk3_frame(thread, thread->frame + 1) = 0x531;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;

scream_dead_end:
    stop_me_player(obj);
    ground_player(obj);
    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

scream_countdown:
    obj->a10 = obj->a10 - 1;
    if (obj->a10 != 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x565;
        thread->fieldfc = 3;
        return 3;
    }
    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* ----------------------------------------------------------------------- t_r_net
 *
 * armv7 0x00048ec8, five hundred and twelve bytes.
 *
 * A tug-of-war: state 0x4db sets up a decelerating pull (obj->field48
 * loses obj->a10, here holding 0.125 rather than its usual argument-slot
 * role -- every cycle) and pushes `t_net_sleep` at token 0x4f4; state
 * 0x4f4 checks `obj->field00->field28`, a counter another routine
 * maintains, either walking it down one tick per cycle (looping back
 * through the same decelerating pull) or, once it drops to 0x28 or below,
 * switching to `get_x_dist` and comparing against 0x45 to decide whether
 * to keep counting down or stop outright. Once the countdown reaches zero
 * the fighter is stopped and `obj->a10` is repurposed a second time, now
 * for a genuine 0x20-tick countdown at token 0x50a, ending in a stick
 * check that either installs `t_local_reaction_exit` or launches into
 * `t_flight` / `t_reaction_land` (token 0x518).
 *
 *      state 0
 *          obj->field20 = 0xa ; obj->field00->field48 = 0xa
 *          obj->field34 = 1 ; if_shao_then_pass(obj)
 *          obj->field30 = 0 ; obj->field38 = 0
 *          push t_reaction_start                        (0x4db)
 *      state 0x4db
 *          dec_my_p_hit(obj)
 *          obj->field1c = 8 ; group_sound(obj)
 *          clear_noflip(obj) ; set_no_block(obj)
 *          obj->field20 = 0x607 ; obj->field00->field18 = 0x607
 *          obj->field40 = 0x20 ; find_ani_part2(obj)
 *          obj->field1c = 4 ; init_anirate(obj)
 *          obj->field48 = 3.0 ; obj->a10 = 0.125
 *          obj->field1c = 0x50
 *          (falls into the pull loop below)
 *      pull loop (also reached from the decrement below)
 *          obj->field00->field28 = obj->field1c
 *          obj->field48 -= obj->a10 ; obj->field1c = obj->field48
 *          away_x_vel(obj)
 *          push t_net_sleep                              (0x4f4)
 *      state 0x4f4
 *          obj->field1c = obj->field00->field28
 *          if obj->field1c <= 0x28
 *              get_x_dist(obj)
 *              if obj->field28 > 0x45
 *                  (join the decrement below)
 *              (else falls into the stop-and-countdown below)
 *          else
 *              (join the decrement below)
 *      decrement (also reached from state 0x4f4's <=0x28 branch above)
 *          obj->field1c = obj->field00->field28 - 1
 *          obj->field00->field28 = obj->field1c
 *          if obj->field1c != 0
 *              (back to the pull loop above)
 *          (falls into the stop-and-countdown below)
 *      stop and 0x20-tick countdown
 *          stop_me_player(obj) ; obj->a10 = 0x20
 *          push t_net_sleep                              (0x50a)
 *      state 0x50a
 *          obj->a10 -= 1
 *          if obj->a10 != 0
 *              (back to the same push above)
 *          else
 *              am_i_airborn(obj)
 *              if obj->field5c == 0
 *                  install t_local_reaction_exit
 *              obj->field24 = 0x8000 ; obj->field1c = 0 ; obj->field28 = 5
 *              obj->field20 = 0 ; obj->field40 += 0x19
 *              push t_flight                                (0x518)
 *      state 0x518
 *          install t_reaction_land
 */
long t_r_net(struct MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x4f4) {
        obj->field1c = obj->field00->field28;
        if ((int32_t)obj->field1c <= 0x28) {
            get_x_dist(obj);
            if ((int32_t)obj->field28 > 0x45)
                goto net_decrement;
            goto net_stop;
        }
        goto net_decrement;
    }

    if (token == 0x50a) {
        obj->a10 = obj->a10 - 1;
        if (obj->a10 != 0)
            goto net_push_50a;

        am_i_airborn(obj);
        if (obj->field5c == 0)
            return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

        obj->field24 = 0x8000;
        obj->field1c = obj->a10;
        obj->field28 = 5;
        obj->field20 = obj->a10;
        obj->field40 = obj->field28 + 0x19;

        *mk3_frame(thread, thread->frame + 1) = 0x518;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x518)
        return mk3_install(thread, (MK3THREADFUNC)t_reaction_land);

    if (token == 0x4db) {
        dec_my_p_hit(obj);

        obj->field1c = 8;
        group_sound(obj);

        clear_noflip(obj);
        set_no_block(obj);

        obj->field20 = 0x607;
        obj->field00->field18 = 0x607;

        obj->field40 = 0x20;
        find_ani_part2(obj);

        obj->field1c = 4;
        init_anirate(obj);

        obj->field48 = 0x30000;                  /* 3.0 in 16.16 */
        obj->a10 = obj->field48 - 0x2e000;       /* 0.125 */

        obj->field1c = 0x50;
        goto net_pull_loop;
    }

    if (token != 0)
        return -3;

    obj->field20 = 0xa;
    obj->field00->field48 = 0xa;
    obj->field34 = 1;

    if_shao_then_pass(obj);

    obj->field30 = 0;
    obj->field38 = 0;

    *mk3_frame(thread, thread->frame + 1) = 0x4db;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;

net_decrement:
    obj->field1c = obj->field00->field28 - 1;
    obj->field00->field28 = obj->field1c;
    if (obj->field1c != 0)
        goto net_pull_loop;

net_stop:
    stop_me_player(obj);
    obj->a10 = 0x20;

net_push_50a:
    *mk3_frame(thread, thread->frame + 1) = 0x50a;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_net_sleep;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;

net_pull_loop:
    obj->field00->field28 = obj->field1c;

    obj->field48 = obj->field48 - obj->a10;
    obj->field1c = obj->field48;
    away_x_vel(obj);

    *mk3_frame(thread, thread->frame + 1) = 0x4f4;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_net_sleep;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* -------------------------------------------------------------- t_r_tusk_blur
 *
 * armv7 0x00047170, six hundred and eight bytes -- the largest function in
 * this file.
 *
 * State 0 pushes `t_reaction_start` (after the usual `if_shao_then_pass`).
 * State 0x107f sets up the animation, borrowing the thread's SECOND stack
 * (`mk3_arg`) once to save and restore `obj->field00->field24` around a
 * write of 6 into it -- the same save/restore-through-the-arg-stack idiom
 * `t_shake_on_my_back` uses -- then self-resumes into state 0x1095. That
 * state counts down `obj->field00->field28` and, separately,
 * `obj->a10` (0x30, decremented once every other call), looping
 * through the self-resume until both run out. Once they do, `p_hit` and
 * `field54` get the same save-through-the-arg-stack-then-zero-then-restore
 * treatment (two pushes this time, popped in reverse) around a call to
 * `back_to_normal`, and the reaction forks three ways on whether the
 * fighter is airborne, is a boss, or neither -- ending respectively in
 * `t_fall_on_my_back`, `t_local_reaction_exit`, or a genuine push into
 * `t_animate_a0_frames` (token 0x10be), which itself just installs
 * `t_local_reaction_exit`.
 *
 *      state 0
 *          obj->field34 = 1 ; if_shao_then_pass(obj)
 *          obj->field30 = 0 ; obj->field38 = 0
 *          push t_reaction_start                        (0x107f)
 *      state 0x107f
 *          stop_me_player(obj) ; set_no_block(obj)
 *          obj->field1c = 0x617 ; obj->field00->field18 = 0x617
 *          obj->field1c = obj->field08->field24
 *          *mk3_arg(thread, thread->fieldf8++) = obj->field1c
 *          obj->field1c = 6 ; obj->field08->field24 = 6
 *          obj->field40 = 8 ; get_char_ani2(obj)
 *          obj->field1c = *mk3_arg(thread, --thread->fieldf8)
 *          obj->field08->field24 = obj->field1c
 *          do_next_a9_frame(obj)
 *          obj->field1c = 4 ; obj->field00->field28 = 4
 *          obj->a10 = 0x30
 *          (falls into the self-resume below)
 *      self-resume (also reached from state 0x1095's own countdown)
 *          resume self at 0x1095 after 2 frames
 *      state 0x1095
 *          do_next_a9_frame(obj)
 *          obj->field1c = obj->field00->field28 - 1
 *          if obj->field1c == 0
 *              rsnd_func(obj, 0xe) ; obj->field1c = 4
 *              (join the countdown-continue block below)
 *          (falls into the countdown-continue block below)
 *      countdown-continue (also reached from the field28==0 branch above)
 *          obj->field00->field28 = obj->field1c
 *          obj->a10 -= 1
 *          if obj->a10 != 0
 *              (back to the self-resume above)
 *          obj->field1c = obj->field00->p_hit
 *          obj->field20 = obj->field00->field54
 *          *mk3_arg(thread, thread->fieldf8++) = obj->field1c
 *          *mk3_arg(thread, thread->fieldf8++) = obj->field20
 *          obj->field1c = obj->a10 ; obj->field00->p_hit = obj->a10
 *          obj->field1c = obj->a10 ; obj->field00->field54 = obj->a10
 *          back_to_normal(obj)
 *          obj->field20 = *mk3_arg(thread, --thread->fieldf8)
 *          obj->field1c = *mk3_arg(thread, --thread->fieldf8)
 *          obj->field00->p_hit = obj->field1c
 *          obj->field00->field54 = obj->field20
 *          obj->field1c = 0x617 ; obj->field00->field18 = 0x617
 *          am_i_airborn(obj)
 *          if obj->field5c != 0
 *              install t_fall_on_my_back
 *          ground_player(obj) ; q_am_i_a_boss(obj)
 *          if obj->field5c != 0
 *              install t_local_reaction_exit
 *          obj->field40 = 0x25 ; get_char_ani(obj)
 *          obj->field1c = 0x5000c
 *          push t_animate_a0_frames                        (0x10be)
 *      state 0x10be
 *          install t_local_reaction_exit
 */
long t_r_tusk_blur(struct MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x10be)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

    if (token == 0x1095) {
        do_next_a9_frame(obj);

        obj->field1c = obj->field00->field28 - 1;
        if (obj->field1c == 0) {
            rsnd_func(obj, 0xe);
            obj->field1c = 4;
        }

        goto tusk_blur_countdown;
    }

    if (token == 0x107f) {
        stop_me_player(obj);
        set_no_block(obj);

        obj->field1c = 0x617;
        obj->field00->field18 = 0x617;

        obj->field1c = obj->field08->field24;
        *mk3_arg(thread, thread->fieldf8) = obj->field1c;
        thread->fieldf8 = thread->fieldf8 + 1;

        obj->field1c = 6;
        obj->field08->field24 = 6;

        obj->field40 = 8;
        get_char_ani2(obj);

        thread->fieldf8 = thread->fieldf8 - 1;
        obj->field1c = *mk3_arg(thread, thread->fieldf8);
        obj->field08->field24 = obj->field1c;

        do_next_a9_frame(obj);

        obj->field1c = 4;
        obj->field00->field28 = 4;
        obj->a10 = 0x30;

        goto tusk_blur_resume;
    }

    if (token != 0)
        return -3;

    obj->field34 = 1;
    if_shao_then_pass(obj);

    obj->field30 = 0;
    obj->field38 = 0;

    *mk3_frame(thread, thread->frame + 1) = 0x107f;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_reaction_start;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;

tusk_blur_countdown:
    obj->field00->field28 = obj->field1c;

    obj->a10 = obj->a10 - 1;
    if (obj->a10 != 0)
        goto tusk_blur_resume;

    obj->field1c = obj->field00->p_hit;
    obj->field20 = obj->field00->field54;

    *mk3_arg(thread, thread->fieldf8) = obj->field1c;
    thread->fieldf8 = thread->fieldf8 + 1;
    *mk3_arg(thread, thread->fieldf8) = obj->field20;
    thread->fieldf8 = thread->fieldf8 + 1;

    obj->field1c = obj->a10;
    obj->field00->p_hit = obj->a10;
    obj->field1c = obj->a10;
    obj->field00->field54 = obj->a10;

    back_to_normal(obj);

    thread->fieldf8 = thread->fieldf8 - 1;
    obj->field20 = *mk3_arg(thread, thread->fieldf8);
    thread->fieldf8 = thread->fieldf8 - 1;
    obj->field1c = *mk3_arg(thread, thread->fieldf8);

    obj->field00->p_hit = obj->field1c;
    obj->field00->field54 = obj->field20;

    obj->field1c = 0x617;
    obj->field00->field18 = 0x617;

    am_i_airborn(obj);
    if (obj->field5c != 0)
        return mk3_install(thread, (MK3THREADFUNC)t_fall_on_my_back);

    ground_player(obj);
    q_am_i_a_boss(obj);
    if (obj->field5c != 0)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

    obj->field40 = 0x25;
    get_char_ani(obj);

    obj->field1c = 0x5000c;

    *mk3_frame(thread, thread->frame + 1) = 0x10be;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_animate_a0_frames;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;

tusk_blur_resume:
    *mk3_frame(thread, thread->frame + 1) = 0x1095;
    thread->fieldfc = 2;
    return 2;
}
