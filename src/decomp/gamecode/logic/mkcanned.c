/*
 * mkcanned.c -- gamecode/logic/mkcanned.c, decompiled.
 *
 * Part of the fight engine. *
 * This first pass was read by two programs rather than by eye, because most of
 * what is here is one function written many times.
 *
 * `tools/pushfn.py` executes a body symbolically -- every register tracked as
 * a constant, a pc-relative address, a load or nothing at all -- and accepts
 * it only when it accounted for EVERY instruction and the effects come out as
 * the frame-push shape: some stores into the object, then the handler and the
 * cleared slot above. One instruction it cannot model and the function is
 * refused rather than guessed at.
 *
 * `tools/microfn.py` matches whole bodies against fixed templates for the
 * smaller shapes -- a tail call, a constant into 0x5c, a table handed to a
 * search routine -- and refuses anything with an instruction out of place.
 *
 * Both refuse loudly. What they could not prove is not in this file; it is
 * read one function at a time.
 */

#include "mk3logic.h"

long t_plwins(struct MK3THREAD *thread);
long t_vic7(struct MK3THREAD *thread);

/* t_player_2_wins -- armv7 0x0007cea0, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field24 = 0x1
 *      frame[frame].handler = t_plwins
 *      frame[frame+1].w0 = 0
 */

long t_player_2_wins(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field24 = 0x1;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_plwins);
}

/* t_kano_victory -- armv7 0x0007cfa0, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->a10 = 0x4
 *      frame[frame].handler = t_vic7
 *      frame[frame+1].w0 = 0
 */

long t_kano_victory(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->a10 = 0x4;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_vic7);
}

/* t_generic_victory -- armv7 0x0007cfdc, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->a10 = 0x5
 *      frame[frame].handler = t_vic7
 *      frame[frame+1].w0 = 0
 */

long t_generic_victory(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->a10 = 0x5;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_vic7);
}





/* --------------------------------------------------------------------
 * What the readers could prove. See tools/pushfn.py, which executes
 * a body symbolically, and tools/microfn.py, which matches whole
 * bodies against templates. Both refuse anything they cannot account
 * for instruction by instruction.
 * -------------------------------------------------------------------- */






/* --------------------------------------------------------------------
 * What the readers could prove. See tools/pushfn.py, which executes
 * a body symbolically, and tools/microfn.py, which matches whole
 * bodies against templates. Both refuse anything they cannot account
 * for instruction by instruction.
 * -------------------------------------------------------------------- */


/* t_player_1_wins -- armv7 0x0007cedc, 56 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field24 = 0   (the register the guard proved)
 *      frame[frame].handler = t_plwins
 *      frame[frame+1].w0 = 0
 */

long t_player_1_wins(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field24 = 0;   /* the guard proved this register */

    return mk3_push_handler(thread, (MK3THREADFUNC)t_plwins);
}

/* --------------------------------------------------------------------
 * Added by a later sweep -- tools/sweep.py, running the same
 * readers again after one of them learned something. Each still
 * refuses anything it cannot account for instruction by
 * instruction; see tools/pushfn.py and tools/leaffn.py.
 * -------------------------------------------------------------------- */

long t_vicjump(struct MK3THREAD *thread);
void am_i_shang(MK3OBJ *obj);
void clear_inviso(MK3OBJ *obj);
void init_special(MK3OBJ *obj);

/* t_victory_animation -- armv7 0x0007d3a4, 116 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      clear_inviso(obj)
 *      init_special(obj)
 *      am_i_shang(obj)
 *      frame[frame].handler = t_vicjump
 *      frame[frame+1].w0 = 0
 */

long t_st_victory(MK3THREAD *thread);

long t_victory_animation(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    clear_inviso(obj);
    init_special(obj);
    am_i_shang(obj);

    /* Corrected: this was an unconditional t_vicjump. The binary branches on
     * am_i_shang's answer -- Shang Tsung gets his own victory, t_st_victory. */
    if (obj->field5c != 0)
        return mk3_install(thread, (MK3THREADFUNC)t_st_victory);

    return mk3_install(thread, (MK3THREADFUNC)t_vicjump);
}

/* --------------------------------------------------------------------
 * Added by a later sweep -- tools/sweep.py, running the same
 * readers again after one of them learned something. Each still
 * refuses anything it cannot account for instruction by
 * instruction; see tools/pushfn.py and tools/leaffn.py.
 * -------------------------------------------------------------------- */

long t_dizzy_wake(MK3THREAD *thread);
long t_mframew(MK3THREAD *thread);
long t_wait_forever(MK3THREAD *thread);
void get_char_ani(MK3OBJ *obj);

/* t_dizzy_sleep -- armv7 0x0007ce58, 72 bytes.  **Complete.**
 *
 *      token == 0:
 *          park(token 0x154, duration 0x1)
 *      token == 0x154:
 *          frame[frame].handler = t_dizzy_wake
 *      otherwise:  return -3
 */
long t_dizzy_sleep(MK3THREAD *thread)
{
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x154;
        thread->fieldfc = 0x1;
        return 0x1;
    }

    if (token != 0x154)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_dizzy_wake);
}

/* t_vic7 -- armv7 0x0007d418, 136 bytes.  **Complete.**
 *
 *      token == 0:
 *          obj->field40 = 0xd
 *          get_char_ani(obj)
 *          obj->field1c = obj->a10
 *          token := 0x276, then descend into t_mframew
 *      token == 0x276:
 *          frame[frame].handler = t_wait_forever
 *      otherwise:  return -3
 */
long t_vic7(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field40 = 0xd;
        get_char_ani(obj);
        obj->field1c = obj->a10;
        *mk3_frame(thread, thread->frame + 1) = 0x276;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x276)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_wait_forever);
}


/* -------------------------------------------------------------- t_st_victory
 *
 * armv7 0x0007cf14, 140 bytes.  **Complete.**
 *
 *      token == 0:      obj->field1c = obj->field08->field24
 *                       if (that == 0xc) -- into the 0x1e7 body --
 *                       token := 0x1e7, descend into t_back_to_shang_form
 *
 *      token == 0x1e7:  frame[frame].handler = t_generic_victory
 *
 *      otherwise:       return -3
 *
 * **Shang Tsung wins as whoever he currently is, and then changes back.** The
 * character number is checked against 0xc and anything else is sent down into
 * t_back_to_shang_form first; 0xc -- which must be Shang's own number, since it
 * is the one that needs no reverting -- skips straight to the generic pose.
 *
 * The two paths share the install, which is why the character test lands in the
 * middle of the second state rather than before it. A leaf frame: no push, no
 * registers saved. */
long t_back_to_shang_form(MK3THREAD *thread);   /* pointer slot 0x000f33dc */

long t_st_victory(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field1c = obj->field08->field24;
        if (obj->field1c != 0xc) {
            *mk3_frame(thread, thread->frame + 1) = 0x1e7;
            thread->frame = thread->frame + 1;      /* push a level */
            mk3_frame(thread, thread->frame)[1] =
                (uint32_t)(uintptr_t)t_back_to_shang_form;
            *mk3_frame(thread, thread->frame + 1) = 0;
            return 0;
        }
    } else if (token != 0x1e7) {
        return -3;
    }

    return mk3_install(thread, (MK3THREADFUNC)t_generic_victory);
}

/* ----------------------------------------------------------------- t_vicjump
 *
 * armv7 0x0007d018, 144 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      switch (obj->field08->field24) {
 *          case 0:  h = t_kano_victory
 *          case 1:  h = t_sonya_victory
 *          case 2:  h = t_jax_victory
 *          case 3:  h = t_indian_victory
 *          case 5:  h = t_swat_victory
 *          case 6:  h = t_lia_victory
 *          default: h = t_generic_victory
 *      }
 *      frame[frame].handler = h
 *      frame[frame+1].w0 = 0
 *
 * **The whole routine is one table from character to victory pose**, and the
 * six that have one are the six the file writes out. Everyone else gets
 * t_generic_victory, and the numbers agree with the ones DoASpecial dispatches
 * on -- 0 Kano, 1 Sonya, 2 Jax, 3 the Indian, 5 the swat, 6 Liu Kang.
 *
 * Character 4 falls to the default even though it sits inside the run, so the
 * gap is deliberate and not the end of the table. */
long t_sonya_victory(MK3THREAD *thread);
long t_jax_victory(MK3THREAD *thread);
long t_indian_victory(MK3THREAD *thread);
long t_swat_victory(MK3THREAD *thread);
long t_lia_victory(MK3THREAD *thread);

long t_vicjump(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    MK3THREADFUNC h;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    switch (obj->field08->field24) {
    case 0:  h = (MK3THREADFUNC)t_kano_victory;    break;
    case 1:  h = (MK3THREADFUNC)t_sonya_victory;   break;
    case 2:  h = (MK3THREADFUNC)t_jax_victory;     break;
    case 3:  h = (MK3THREADFUNC)t_indian_victory;  break;
    case 5:  h = (MK3THREADFUNC)t_swat_victory;    break;
    case 6:  h = (MK3THREADFUNC)t_lia_victory;     break;
    default: h = (MK3THREADFUNC)t_generic_victory; break;
    }

    return mk3_install(thread, h);
}

/* -------------------------------------------------------------- t_mframew_2x
 *
 * armv7 0x0007d0a8, 240 bytes.  **Complete.**
 *
 *      token == 0:      push obj->field1c, push obj->field20
 *                       token := 0x2d5, descend into t_mframew
 *
 *      token == 0x2d5:  obj->field20 = pop
 *                       obj->a10     = pop
 *                       obj->field1c = obj->field20
 *                       token := 0x2da, park obj->field1c
 *
 *      token == 0x2da:  obj->field1c = obj->a10
 *                       frame[frame].handler = t_mframew
 *
 *      otherwise:       return -3
 *
 * **Two waits out of one call, which is what the name says.** 0x1c and 0x20 go
 * onto the thread's argument stack -- the array at 0xa8 with its cursor at 0xf8,
 * the one mk3_arg indexes -- and the routine waits the first count in t_mframew.
 * Coming back it pops both, and the crossing is the point: the top of the stack
 * was 0x20 and goes back to 0x20, but the one under it was the ORIGINAL 0x1c and
 * lands in **0x44** instead. 0x1c is then loaded from 0x20, so the second wait
 * runs on the second count. State 0x2da puts the saved first count back into
 * 0x1c and goes into t_mframew a second time.
 *
 * So a caller sets 0x1c and 0x20 to two durations and gets both, in that order,
 * with 0x44 used as the parking space across the first one. */
long t_mframew_2x(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    uint32_t cur;

    if (token == 0) {
        cur = thread->fieldf8;
        *mk3_arg(thread, cur) = obj->field1c;
        thread->fieldf8 = cur + 1;
        *mk3_arg(thread, cur + 1) = obj->field20;
        thread->fieldf8 = cur + 2;

        *mk3_frame(thread, thread->frame + 1) = 0x2d5;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x2d5) {
        cur = thread->fieldf8 - 1;
        thread->fieldf8 = cur;
        obj->field20 = *mk3_arg(thread, cur);

        cur = cur - 1;
        thread->fieldf8 = cur;
        obj->a10 = *mk3_arg(thread, cur);

        obj->field1c = obj->field20;
        *mk3_frame(thread, thread->frame + 1) = 0x2da;
        thread->fieldfc = obj->field1c;
        return (long)obj->field1c;
    }

    if (token != 0x2da)
        return -3;

    obj->field1c = obj->a10;
    return mk3_install(thread, (MK3THREADFUNC)t_mframew);
}

/* ---------------------------------------------------------- t_indian_victory
 *
 * armv7 0x0007d198, 252 bytes.  **Complete.**
 *
 *      token == 0:      obj->field40 = 0x0005000d
 *                       token := 0x1f5, descend into t_animate_a9
 *
 *      token == 0x1f5:  token := 0x1f6, park 8
 *      token == 0x1f6:  do_next_a9_frame(obj)
 *                       token := 0x1fc, park 9
 *      token == 0x1fc:  obj->field1c = 6
 *                       token := 0x1ff, descend into t_animate_a9
 *      token == 0x1ff:  frame[frame].handler = t_wait_forever
 *
 *      otherwise:       return -3
 *
 * **0x40 arrives at t_animate_a9 packed, and this is one of three sites that
 * says how.** The word is 0x0005000d here, 0x00040021 in t_dizzy_dude and
 * 0x00040047 in t_collapse_on_ground -- in every case a small number in the
 * high halfword and an animation-sized number in the low one. Three sites is
 * enough to say the field is two halves and not enough to name them, so the
 * constants are written whole.
 *
 * The pose is four steps: animate, wait eight, advance one frame by hand, wait
 * nine, animate again, then stop forever. `do_next_a9_frame` between the two
 * waits is the only thing that moves the animation on that pass. */
long t_animate_a9(MK3THREAD *thread);           /* pointer slot 0x000f36d0 */
long do_next_a9_frame(MK3OBJ *obj);

long t_indian_victory(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x1fc) {
        obj->field1c = 6;
        *mk3_frame(thread, thread->frame + 1) = 0x1ff;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_mframew);
    }

    if (token == 0) {
        obj->field40 = 0x0005000d;
        *mk3_frame(thread, thread->frame + 1) = 0x1f5;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_animate_a9);
    }

    if (token == 0x1f5) {
        *mk3_frame(thread, thread->frame + 1) = 0x1f6;
        thread->fieldfc = 8;
        return 8;
    }

    if (token == 0x1f6) {
        do_next_a9_frame(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x1fc;
        thread->fieldfc = 9;
        return 9;
    }

    if (token != 0x1ff)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_wait_forever);
}

/* ------------------------------------------------------ t_collapse_on_ground
 *
 * armv7 0x0007d294, 272 bytes.  **Complete.**
 *
 *      token == 0:      player_normpal(obj); set_noedge(obj);
 *                       stop_me_player(obj)
 *                       *(uint32_t *)((char *)obj + 0x64) = 0x12
 *                       token := 0x30c, descend into t_boss_branch
 *
 *      token == 0x30c:  obj->field40 = 0x00040047
 *                       token := 0x30f, descend into t_animate_a9
 *
 *      token == 0x30f:  obj->field40 = 0x1e
 *                       find_ani_part2(obj); find_last_frame(obj)
 *                       do_next_a9_frame(obj)
 *                       obj->field20 = 0
 *                       obj->field1c =
 *                           ochar_dead_adjusts[obj->field08->field24]
 *                       multi_adjust_xy(obj); shake_n_sound(obj)
 *                       frame[frame].handler = t_wait_forever
 *
 *      otherwise:       return -3
 *
 * **The body has to be put where the corpse belongs, per character.** The last
 * state jumps the animation to its final frame -- find_last_frame then
 * do_next_a9_frame -- and then reads a per-character word out of
 * ochar_dead_adjusts into 0x1c with 0x20 zeroed, which is the pair
 * multi_adjust_xy consumes. So the table is a positional correction: characters
 * whose dead frame is drawn off-centre are nudged back by a fixed amount, and
 * the shake and its sound follow.
 *
 * 0x64 is past the end of MK3OBJ as this header declares it, so the store is
 * written by offset rather than through a field invented from one site.
 *
 * t_boss_branch before the animation is the hook that lets a boss collapse
 * differently; everything after it is the ordinary path. */
long t_boss_branch(MK3THREAD *thread);          /* pointer slot 0x000f33e0 */
void set_noedge(MK3OBJ *obj);
void stop_me_player(MK3OBJ *obj);
void find_ani_part2(MK3OBJ *obj);
void find_last_frame(MK3OBJ *obj);
void multi_adjust_xy(MK3OBJ *obj);
void shake_n_sound(MK3OBJ *obj);
extern uint32_t ochar_dead_adjusts[];           /* 0x00174dbc */

long t_collapse_on_ground(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        player_normpal(obj);
        set_noedge(obj);
        stop_me_player(obj);
        *(uint32_t *)((char *)obj + 0x64) = 0x12;

        *mk3_frame(thread, thread->frame + 1) = 0x30c;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_boss_branch;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x30c) {
        obj->field40 = 0x00040047;

        *mk3_frame(thread, thread->frame + 1) = 0x30f;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_animate_a9;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x30f)
        return -3;

    obj->field40 = 0x1e;
    find_ani_part2(obj);
    find_last_frame(obj);
    do_next_a9_frame(obj);

    obj->field20 = 0;
    obj->field1c = ochar_dead_adjusts[obj->field08->field24];
    multi_adjust_xy(obj);
    shake_n_sound(obj);

    return mk3_install(thread, (MK3THREADFUNC)t_wait_forever);
}

/* ------------------------------------------------------------ t_swat_victory
 *
 * armv7 0x0007d4a0, 280 bytes.  **Complete.**
 *
 *      token == 0:      obj->field40 = 0xd; get_char_ani(obj)
 *                       obj->field1c = 6
 *                       token := 0x249, descend into t_mframew
 *
 *      token == 0x249:  obj->field48 = 3
 *                       token := 0x24f, park 6
 *
 *      token == 0x24f:  obj->field40 = 0xd; find_ani_part2(obj)
 *                       obj->field1c = 2
 *                       token := 0x253, descend into t_mframew
 *
 *      token == 0x253:  if (--obj->field48 > 0) { token := 0x24f, park 6 }
 *                       else { obj->field1c = 6
 *                              token := 0x25b, descend into t_mframew }
 *
 *      token == 0x25b:  frame[frame].handler = t_wait_forever
 *
 *      otherwise:       return -3
 *
 * **The pose repeats three times and 0x48 is the counter.** State 0x249 loads
 * it with 3 and 0x253 takes one off and goes back to 0x24f while any are left,
 * so the two-frame animation plus the six-frame park runs three times before
 * the routine settles.
 *
 * Both animation loads use 0x40 = 0xd, but the first goes through get_char_ani
 * and the repeat through find_ani_part2 -- so the entry picks the character's
 * animation and the repeat re-finds a part of the same one, which is why the
 * durations differ, six then two. */
long t_swat_victory(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    uint32_t next;

    if (token == 0) {
        obj->field40 = 0xd;
        get_char_ani(obj);
        obj->field1c = 6;
        next = 0x249;

    } else if (token == 0x249) {
        obj->field48 = 3;
        *mk3_frame(thread, thread->frame + 1) = 0x24f;
        thread->fieldfc = 6;
        return 6;

    } else if (token == 0x24f) {
        obj->field40 = 0xd;
        find_ani_part2(obj);
        obj->field1c = 2;
        next = 0x253;

    } else if (token == 0x253) {
        obj->field48 = obj->field48 - 1;
        if ((long)obj->field48 > 0) {
            *mk3_frame(thread, thread->frame + 1) = 0x24f;
            thread->fieldfc = 6;
            return 6;
        }
        obj->field1c = 6;
        next = 0x25b;

    } else if (token == 0x25b) {
        return mk3_install(thread, (MK3THREADFUNC)t_wait_forever);

    } else {
        return -3;
    }

    *mk3_frame(thread, thread->frame + 1) = next;
    thread->frame = thread->frame + 1;              /* push a level */
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}

/* ----------------------------------------------------------- t_sonya_victory
 *
 * armv7 0x0007d5b8, 208 bytes.  **Complete.**
 *
 *      token == 0:      obj->field40 = 0xd; get_char_ani(obj)
 *                       obj->field1c = 5
 *                       token := 0x210, descend into t_mframew
 *
 *      token == 0x210:  token := 0x211, park 0xc
 *
 *      token == 0x211:  obj->field1c = 4
 *                       token := 0x213, descend into t_mframew
 *
 *      token == 0x213:  frame[frame].handler = t_wait_forever
 *
 *      otherwise:       return -3
 *
 * The plainest of the six: animate five frames, hold twelve, animate four more,
 * stop. The last two states share their install site, one arriving with the
 * frame index already pushed and the other with it as it was -- which is how one
 * piece of code serves both a descend and an install. */
long t_sonya_victory(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    uint32_t next;

    if (token == 0) {
        obj->field40 = 0xd;
        get_char_ani(obj);
        obj->field1c = 5;
        next = 0x210;

    } else if (token == 0x210) {
        *mk3_frame(thread, thread->frame + 1) = 0x211;
        thread->fieldfc = 0xc;
        return 0xc;

    } else if (token == 0x211) {
        obj->field1c = 4;
        next = 0x213;

    } else if (token == 0x213) {
        return mk3_install(thread, (MK3THREADFUNC)t_wait_forever);

    } else {
        return -3;
    }

    *mk3_frame(thread, thread->frame + 1) = next;
    thread->frame = thread->frame + 1;              /* push a level */
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}

/* ------------------------------------------------------------- t_jax_victory
 *
 * armv7 0x0007d688, 136 bytes.  **Complete.**
 *
 *      token == 0:      obj->field40 = 0xd; get_char_ani(obj)
 *                       obj->field1c = 6
 *                       obj->field20 = 6 + 6 = 0xc
 *                       token := 0x208, descend into t_mframew_2x
 *
 *      token == 0x208:  frame[frame].handler = t_wait_forever
 *
 *      otherwise:       return -3
 *
 * **The only caller of t_mframew_2x, and it fills both slots the way that
 * routine expects**: 0x1c is the first count and 0x20 the second, six then
 * twelve. The second is built as `adds r3, r3, r3` off the first, so the pair is
 * a duration and its double rather than two independent numbers. */
long t_mframew_2x(MK3THREAD *thread);

long t_jax_victory(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field40 = 0xd;
        get_char_ani(obj);
        obj->field1c = 6;
        obj->field20 = 6 + 6;

        *mk3_frame(thread, thread->frame + 1) = 0x208;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew_2x;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x208)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_wait_forever);
}

/* ------------------------------------------------------------- t_lia_victory
 *
 * armv7 0x0007d710, 316 bytes.  **Complete.**
 *
 *      token == 0:      set_ignore_y(obj)
 *                       obj->field40 = 0xd; get_char_ani(obj)
 *                       obj->field1c = 4
 *                       token := 0x227, descend into t_mframew
 *
 *      token == 0x227:  obj->field1c = part->field20 = 0xffffc000
 *                       token := 0x22b, park 0xc
 *
 *      token == 0x22b:  obj->a10 = part->field20 = 0x2000
 *      token == 0x240:  token := 0x232, park 1
 *
 *      token == 0x232:  obj->field1c = part->field1c
 *                       if (that < 0)  token := 0x232, park 1
 *                       else           token := 0x236, park 0xd
 *
 *      token == 0x236:  obj->a10 = part->field20 = -obj->a10
 *      (falls through)  token := 0x23c, park 1
 *
 *      token == 0x23c:  obj->field1c = part->field1c
 *                       if (that >= 0) token := 0x23c, park 1
 *                       else           token := 0x240, park 0xd
 *
 *      otherwise:       return -3
 *
 * **It never ends.** 0x232 waits for the part's 0x1c to come non-negative,
 * 0x236 flips the sign of the value in 0x2000, 0x23c waits for it to go
 * negative again, and 0x240 goes back to the state that reloads 0x2000 -- so
 * the four states are a closed cycle with no exit. A victory pose runs until
 * something outside the thread takes the fighter away, which is why it has no
 * t_wait_forever like the other five.
 *
 * The opening kick is a big negative into the part's 0x20 -- 0xffffc000, minus
 * sixteen thousand -- and the loop then rocks a value a quarter that size back
 * and forth. set_ignore_y at the entry is what lets the object leave the
 * ground for it. */
void set_ignore_y(MK3OBJ *obj);

long t_lia_victory(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    MK3OBJ  *part  = obj->field08;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        set_ignore_y(obj);
        obj->field40 = 0xd;
        get_char_ani(obj);
        obj->field1c = 4;

        *mk3_frame(thread, thread->frame + 1) = 0x227;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x227) {
        obj->field1c    = 0xffffc000u;
        part->field20   = 0xffffc000u;
        *mk3_frame(thread, thread->frame + 1) = 0x22b;
        thread->fieldfc = 0xc;
        return 0xc;
    }

    if (token == 0x22b || token == 0x240) {
        obj->a10      = 0x2000;
        part->field20 = 0x2000;
        *mk3_frame(thread, thread->frame + 1) = 0x232;
        thread->fieldfc = 1;
        return 1;
    }

    if (token == 0x232) {
        obj->field1c = part->field1c;
        if ((int32_t)obj->field1c < 0) {
            *mk3_frame(thread, thread->frame + 1) = 0x232;
            thread->fieldfc = 1;
            return 1;
        }
        *mk3_frame(thread, thread->frame + 1) = 0x236;
        thread->fieldfc = 0xd;
        return 0xd;
    }

    if (token == 0x236) {
        obj->a10      = (uint32_t)(-(int32_t)obj->a10);
        part->field20 = obj->a10;
        *mk3_frame(thread, thread->frame + 1) = 0x23c;
        thread->fieldfc = 1;
        return 1;
    }

    if (token != 0x23c)
        return -3;

    obj->field1c = part->field1c;
    if ((int32_t)obj->field1c >= 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x23c;
        thread->fieldfc = 1;
        return 1;
    }

    *mk3_frame(thread, thread->frame + 1) = 0x240;
    thread->fieldfc = 0xd;
    return 0xd;
}

/* ----------------------------------------------------------------- t_plwins
 *
 * armv7 0x0007d84c, 236 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = obj->field00->field08
 *      if (obj->field1c != obj->field24) {
 *          pop one level
 *          disable_all_buttons(obj)
 *          frame[frame].handler = t_collapse_on_ground
 *      } else {
 *          pop one level
 *          frame[frame].handler = t_victory_animation
 *      }
 *      frame[frame+1].w0 = 0
 *
 * **The test is which player this thread belongs to.** The proc's 0x08 is the
 * strength index -- the player number -- and 0x24 holds the number this routine
 * was started for, so the two are equal for the winner and different for the
 * loser. The winner gets the victory animation and the loser collapses with its
 * buttons taken away.
 *
 * **Both arms pop a level first, and the pop is written out twice because the
 * compiler emitted it twice.** It is not a plain decrement: the handler and the
 * token one level down are moved up into the level being left, so the parent
 * inherits the child's continuation, and only then is the new handler stored
 * over it. When the frame index is already at the bottom there is nothing to
 * inherit, so a fallback handler -- t_local_reaction_exit -- is written in
 * first and the same move then runs against it.
 *
 * Two exits, four copies of the same six lines. Kept as they are rather than
 * factored, because one shared helper here would be an invention from one
 * routine. */
long t_collapse_on_ground(MK3THREAD *thread);
long t_local_reaction_exit(MK3THREAD *thread);  /* pointer slot 0x000f3708 */
long t_victory_animation(MK3THREAD *thread);

long t_plwins(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t f, n;
    uint32_t handler;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = obj->field00->field08;

    if (obj->field1c != obj->field24) {
        if ((long)thread->frame > 0)
            thread->frame = thread->frame - 1;
        else
            mk3_frame(thread, thread->frame)[1] =
                (uint32_t)(uintptr_t)t_local_reaction_exit;

        f = thread->frame;
        n = f + 1;
        handler = mk3_frame(thread, n)[1];
        *mk3_frame(thread, n) = *mk3_frame(thread, n + 1);
        mk3_frame(thread, f)[1] = handler;

        disable_all_buttons(obj);
        return mk3_install(thread, (MK3THREADFUNC)t_collapse_on_ground);
    }

    if ((long)thread->frame > 0)
        thread->frame = thread->frame - 1;
    else
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_local_reaction_exit;

    f = thread->frame;
    n = f + 1;
    handler = mk3_frame(thread, n)[1];
    *mk3_frame(thread, n) = *mk3_frame(thread, n + 1);
    mk3_frame(thread, f)[1] = handler;

    return mk3_install(thread, (MK3THREADFUNC)t_victory_animation);
}

/* -------------------------------------------------------------- t_dizzy_wake
 *
 * armv7 0x0007d938, 140 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      next_anirate(obj)
 *      obj->field1c = (int16)*(G + 0x45c)
 *      if (that == 3) { frame[frame].handler = t_dizzy_sleep }
 *      else {
 *          w = *(uint16_t *)(G + 0x450)
 *          obj->field1c = (int16)w
 *          frame[frame].handler = w ? t_dizzy_sleep : t_collapse_on_ground
 *      }
 *      frame[frame+1].w0 = 0
 *
 * **G + 0x45c is the fatality requirement**, the third of the three consecutive
 * halfwords at 0x456, 0x45a and 0x45c, and a 3 there sends the dizzied fighter
 * back to sleep unconditionally. Anything else consults the halfword at
 * G + 0x450, and only a zero there lets the fighter fall.
 *
 * Both halfwords are widened signed into 0x1c even though only the second is
 * tested for zero, so 0x1c carries whichever one the routine looked at last --
 * a diagnostic value, not one anybody downstream needs. */
long t_collapse_on_ground(MK3THREAD *thread);
long next_anirate(MK3OBJ *obj);

long t_dizzy_wake(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint16_t w;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    next_anirate(obj);

    obj->field1c = (uint32_t)(int32_t)*(int16_t *)(G_BYTES + 0x45c);
    if (obj->field1c == 3)
        return mk3_install(thread, (MK3THREADFUNC)t_dizzy_sleep);

    w = *(uint16_t *)(G_BYTES + 0x450);
    obj->field1c = (uint32_t)(int32_t)(int16_t)w;
    if (w == 0)
        return mk3_install(thread, (MK3THREADFUNC)t_collapse_on_ground);

    return mk3_install(thread, (MK3THREADFUNC)t_dizzy_sleep);
}

/* -------------------------------------------------------------- t_dizzy_dude
 *
 * armv7 0x0007d9c4, 240 bytes.  **Complete.**
 *
 *      token == 0:      disable_all_buttons(obj); face_opponent(obj)
 *                       zero_my_p_hit(obj)
 *                       obj->field1c = (int16)*(G + 0x45c)
 *                       if (that == 3) {
 *                           obj->field2c = part->field30 &= ~8
 *                           obj->field2c = proc->field10 |= 8
 *                           set_no_block(obj)
 *                       }
 *                       if (am_i_short(obj)) {
 *                           obj->field40 = 0x00040021
 *                           token := 0x145, descend into t_animate_a9
 *                       }
 *                       -- otherwise fall into the 0x145 body --
 *
 *      token == 0x145:  obj->field20 = obj->field00->field18 = 0x400
 *                       obj->field1c = 6
 *                       init_anirate(obj)
 *                       obj->field40 = 0x25
 *                       get_char_ani(obj)
 *                       frame[frame].handler = t_dizzy_sleep
 *
 *      otherwise:       return -3
 *
 * **A short character gets its own dizzy animation and everyone else shares
 * one.** am_i_short answers in r0 as well as in 0x5c, and this routine reads
 * the returned value -- which is what settled that other.c's `void am_i_short`
 * was understating the function. The short path hands the packed 0x00040021 to
 * t_animate_a9 and comes back at 0x145; the tall path drops straight into the
 * same body.
 *
 * **The fatality-requirement 3 turns two flag bits the opposite way.** Bit 3 is
 * cleared in the part's 0x30 and set in the proc's 0x10, and 0x2c takes a copy
 * of each in turn, so it ends holding the second. Then blocking is taken away.
 * The same halfword at G + 0x45c gates t_dizzy_wake's exit, so the two ends of
 * the dizzy agree on which condition is special.
 *
 * `set_no_block` is called with the argument register left over from
 * `zero_my_p_hit` rather than reloaded, which is the compiler knowing the object
 * survives that call. */
long am_i_short(MK3OBJ *obj);
void zero_my_p_hit(MK3OBJ *obj);
void init_anirate(MK3OBJ *obj);

long t_dizzy_dude(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    MK3OBJ  *part;

    if (token == 0) {
        disable_all_buttons(obj);
        face_opponent(obj);
        zero_my_p_hit(obj);

        obj->field1c = (uint32_t)(int32_t)*(int16_t *)(G_BYTES + 0x45c);
        if (obj->field1c == 3) {
            part = obj->field08;
            obj->field2c = part->field30 & ~8u;
            part->field30 = obj->field2c;

            obj->field2c = obj->field00->field10 | 8u;
            obj->field00->field10 = obj->field2c;

            set_no_block(obj);
        }

        if (am_i_short(obj) != 0) {
            obj->field40 = 0x00040021;

            *mk3_frame(thread, thread->frame + 1) = 0x145;
            thread->frame = thread->frame + 1;      /* push a level */
            mk3_frame(thread, thread->frame)[1] =
                (uint32_t)(uintptr_t)t_animate_a9;
            *mk3_frame(thread, thread->frame + 1) = 0;
            return 0;
        }

    } else if (token != 0x145) {
        return -3;
    }

    obj->field20 = 0x400;
    obj->field00->field18 = 0x400;
    obj->field1c = 6;
    init_anirate(obj);

    obj->field40 = 0x25;
    get_char_ani(obj);

    return mk3_install(thread, (MK3THREADFUNC)t_dizzy_sleep);
}

/* --------------------------------------------------------------- t_finish_him
 *
 * armv7 0x0007dab4, 300 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      proc = obj->field00
 *      obj->field2c = proc->field10
 *      if (obj->field2c & 2) { pop a level; return }
 *      if (*(long *)(H + proc->field08 * 4)
 *          < *(long *)(H + (1 - proc->field08) * 4)) {
 *          pop one level, inheriting the child's handler
 *          frame[frame].handler = t_dizzy_dude
 *      } else {
 *          proc->field10 |= 2
 *          if (am_i_joy(obj)) { pop a level; return }
 *          obj->field1c = 0
 *          frame[frame].handler = t_d_finish_him
 *      }
 *      frame[frame+1].w0 = 0
 *
 * **Bit 1 of the proc's 0x10 is "this one has already been asked", and the
 * routine both reads and sets it.** Finding it set costs a level of frame and
 * nothing else; the branch that goes on to the drone version sets it before
 * calling, so the question is asked once per fighter.
 *
 * **H, indexed by the player number and by one minus it, is what decides who
 * finishes whom.** The proc's 0x08 is the player index, so the two loads are
 * this fighter's counter and the other's, and the fighter with the lower value
 * is the one that gets dizzied -- t_dizzy_dude -- while the other is offered the
 * finisher. `1 - index` is the whole of the two-player assumption, written as
 * `rsb r3, r2, #1`.
 *
 * am_i_joy separates a person from the machine at the last step: a person is
 * given the prompt by popping back out, and the machine goes to t_d_finish_him,
 * the drone's own version.
 *
 * **Every one of the three pops has a fallback for an empty frame stack**, and
 * all three write t_local_reaction_exit -- the same handler t_plwins falls back
 * to. Only the middle one goes on to inherit the child's handler afterwards;
 * the outer two just leave. */
long t_d_finish_him(MK3THREAD *thread);         /* pointer slot 0x000f33e4 */
long t_dizzy_dude(MK3THREAD *thread);
long am_i_joy(MK3OBJ *obj);
extern char *H;                                 /* 0x0038c674 */

long t_finish_him(MK3THREAD *thread)
{
    MK3OBJ      *obj  = (MK3OBJ *)thread->proc;
    MK3OBJPROC  *proc = obj->field00;
    uint32_t     me, f, n, handler;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field2c = proc->field10;
    if ((obj->field2c & 2u) != 0) {
        if ((long)thread->frame > 0)
            thread->frame = thread->frame - 1;
        else
            return mk3_install(thread,
                               (MK3THREADFUNC)t_local_reaction_exit);
        return 0;
    }

    me = proc->field08;
    if (*(long *)(H + me * 4) < *(long *)(H + (1 - me) * 4)) {
        if ((long)thread->frame > 0)
            thread->frame = thread->frame - 1;
        else
            mk3_frame(thread, thread->frame)[1] =
                (uint32_t)(uintptr_t)t_local_reaction_exit;

        f = thread->frame;
        n = f + 1;
        handler = mk3_frame(thread, n)[1];
        *mk3_frame(thread, n) = *mk3_frame(thread, n + 1);
        mk3_frame(thread, f)[1] = handler;

        return mk3_install(thread, (MK3THREADFUNC)t_dizzy_dude);
    }

    proc->field10 = proc->field10 | 2u;

    if (am_i_joy(obj) != 0) {
        if ((long)thread->frame > 0)
            thread->frame = thread->frame - 1;
        else
            return mk3_install(thread,
                               (MK3THREADFUNC)t_local_reaction_exit);
        return 0;
    }

    obj->field1c = 0;
    return mk3_install(thread, (MK3THREADFUNC)t_d_finish_him);
}
