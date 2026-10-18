/*
 * mkstat.c -- gamecode/logic/mkstat.c, decompiled.
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

#include <stdio.h>    /* fflush -- see t_stat_do_uppercut */

#include "mk3logic.h"

long t_retract_strike_act(struct MK3THREAD *thread);

/* t_retract_strike -- armv7 0x0004cb48, 56 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field20 = 0   (the register the guard proved)
 *      frame[frame].handler = t_retract_strike_act
 *      frame[frame+1].w0 = 0
 */

long t_retract_strike(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field20 = 0;   /* the guard proved this register */

    return mk3_push_handler(thread, (MK3THREADFUNC)t_retract_strike_act);
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

/* --------------------------------------------------------------------
 * Straight-line leaves, read by tools/leaffn.py: stores, calls and
 * a return, with every instruction accounted for. It refuses
 * anything that branches, any return value it cannot prove, and any
 * value read from a field the function also writes -- that is a
 * saved value being put back, not a re-read.
 * -------------------------------------------------------------------- */

long create_blood_proc(MK3OBJ *obj);

/* upcut_blood_me -- armv7 0x0004f05c, 16 bytes.  **Complete.**
 *
 *      obj->field1c = 0x1
 *      create_blood_proc(obj)
 */
void upcut_blood_me(MK3OBJ *obj)
{
    obj->field1c = 0x1;
    create_blood_proc(obj);
}


/* jade_normpal -- armv7 0x0004fd00, 12 bytes.  **Complete.**
 *
 *      ldr r0, [r0, #0x44]  ->  player_normpal(obj->a10)
 *
 * The palette is reset on the object in a10, not on obj itself (difftest).
 */
void jade_normpal(MK3OBJ *obj)
{
    player_normpal((MK3OBJ *)obj->a10);
}

/* --------------------------------------------------------------------
 * Added by a later sweep -- tools/sweep.py, running the same
 * readers again after one of them learned something. Each still
 * refuses anything it cannot account for instruction by
 * instruction; see tools/pushfn.py and tools/leaffn.py.
 * -------------------------------------------------------------------- */

void set_half_damage(struct MK3THREAD *thread);
long t_act_mframew(struct MK3THREAD *thread);
long t_axeup3(struct MK3THREAD *thread);
long t_local_reaction_exit(struct MK3THREAD *thread);
long t_victory_animation(struct MK3THREAD *thread);
void call_a0_for_him(MK3OBJ *obj);
void clear_inviso(MK3OBJ *obj);
void death_blow_complete(MK3OBJ *obj);
void get_char_ani(MK3OBJ *obj);
void stop_me_player(MK3OBJ *obj);

/* t_do_block_hi -- armv7 0x0004cea0, 88 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      stop_me_player(obj)
 *      obj->field40 = 0xc
 *      get_char_ani(obj)
 *      obj->field1c = 0x3
 *      obj->field20 = 0x700
 *      frame[frame].handler = t_act_mframew
 *      frame[frame+1].w0 = 0
 */

long t_do_block_hi(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    stop_me_player(obj);
    obj->field40 = 0xc;
    get_char_ani(obj);
    obj->field1c = 0x3;
    obj->field20 = 0x700;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_act_mframew);
}

/* tl_stat_do_fast_axe_up -- armv7 0x0004d078, 88 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = set_half_damage
 *      call_a0_for_him(obj)
 *      obj->field40 = 0x20002
 *      obj->field48 = 0x4
 *      frame[frame].handler = t_axeup3
 *      frame[frame+1].w0 = 0
 */

long tl_stat_do_fast_axe_up(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = (uint32_t)(uintptr_t)set_half_damage;
    call_a0_for_him(obj);
    obj->field40 = 0x20002;
    obj->field48 = 0x4;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_axeup3);
}

/* t_do_un_inviso -- armv7 0x0004f684, 68 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      clear_inviso(obj)
 *      frame[frame].handler = t_local_reaction_exit
 *      frame[frame+1].w0 = 0
 */

long t_do_un_inviso(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    clear_inviso(obj);

    return mk3_push_handler(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

/* tl_babality_complete -- armv7 0x0004fcb4, 76 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      death_blow_complete(obj)
 *      player_normpal(obj)
 *      frame[frame].handler = t_victory_animation
 *      frame[frame+1].w0 = 0
 */

long tl_babality_complete(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    death_blow_complete(obj);
    player_normpal(obj);

    return mk3_push_handler(thread, (MK3THREADFUNC)t_victory_animation);
}

/* --------------------------------------------------------------------
 * Added by a later sweep -- tools/sweep.py, running the same
 * readers again after one of them learned something. Each still
 * refuses anything it cannot account for instruction by
 * instruction; see tools/pushfn.py and tools/leaffn.py.
 * -------------------------------------------------------------------- */

long t_jump_up_land_jsrp(MK3THREAD *thread);
long t_ret9(MK3THREAD *thread);

/* t_retract_strike_act -- armv7 0x0004cadc, 108 bytes.  **Complete.**
 *
 *      token == 0:
 *          token := 0x737, then descend into t_act_mframew
 *      token == 0x737:
 *          frame[frame].handler = t_ret9
 *      otherwise:  return -3
 */
long t_retract_strike_act(MK3THREAD *thread)
{
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x737;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_act_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x737)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_ret9);
}

/* t_jump_up_land_jump -- armv7 0x0004cd30, 112 bytes.  **Complete.**
 *
 *      token == 0:
 *          token := 0x9ad, then descend into t_jump_up_land_jsrp
 *      token == 0x9ad:
 *          frame[frame].handler = t_local_reaction_exit
 *      otherwise:  return -3
 */
long t_jump_up_land_jump(MK3THREAD *thread)
{
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x9ad;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_jump_up_land_jsrp;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x9ad)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

/* --------------------------------------------------------------------
 * Added by a later sweep -- tools/sweep.py, running the same
 * readers again after one of them learned something. Each still
 * refuses anything it cannot account for instruction by
 * instruction; see tools/pushfn.py and tools/leaffn.py.
 * -------------------------------------------------------------------- */

long t_mframew(MK3THREAD *thread);
long tl_delete_proj_and_die(MK3THREAD *thread);
void get_char_ani2(MK3OBJ *obj);

/* t_scream_wave -- armv7 0x0004d8c0, 136 bytes.  **Complete.**
 *
 *      token == 0:
 *          obj->field40 = 0x1
 *          get_char_ani2(obj)
 *          obj->field1c = 0x5
 *          token := 0x643, then descend into t_mframew
 *      token == 0x643:
 *          frame[frame].handler = tl_delete_proj_and_die
 *      otherwise:  return -3
 */
long t_scream_wave(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field40 = 0x1;
        get_char_ani2(obj);
        obj->field1c = 0x5;
        *mk3_frame(thread, thread->frame + 1) = 0x643;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x643)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)tl_delete_proj_and_die);
}


/* --------------------------------------------------------------- update_block_fk
 *
 * armv7 0x0004d3d0, 28 bytes.  **Complete.**
 *
 *      obj->field1c = (uint32_t)(G + 0x420 + 4)
 *      update_tsl(obj)
 *
 * 0x1c carries an ADDRESS into update_tsl, not a number -- `G + 0x424`, built as
 * 0x420 then plus 4 because the compiler had 0x420 in hand. The same field-as-
 * argument convention `call_a0_for_him` uses, here with a data pointer instead of
 * a routine.
 */
void update_tsl(MK3OBJ *obj);

void update_block_fk(MK3OBJ *obj)
{
    obj->field1c = (uint32_t)(uintptr_t)(G_BYTES + 0x420 + 4);
    update_tsl(obj);
}

/* ------------------------------------------------------------- update_l_block_fk
 *
 * armv7 0x0004d060, 24 bytes.  **Complete.**
 *
 *      if (obj->field18 != 0) {
 *          obj->field1c = (uint32_t)update_block_fk
 *          call_a0_for_him(obj)
 *      }
 *
 * **The A0 mechanism at its smallest**: the whole routine is "if this fighter is
 * doing something, run update_block_fk on the OTHER one". The routine handed
 * across is the one directly above, so the pair is a local and a remote spelling
 * of the same update.
 */
void update_block_fk(MK3OBJ *obj);

void update_l_block_fk(MK3OBJ *obj)
{
    if (obj->field18 != 0) {
        obj->field1c = (uint32_t)(uintptr_t)update_block_fk;
        call_a0_for_him(obj);
    }
}

/* -------------------------------------------------------------- next_lao_anirate
 *
 * armv7 0x0004d89c, 36 bytes.  **Complete.**
 *
 *      next_anirate(obj)
 *      if (--obj->a10 == 0) {
 *          obj->field1c = 6
 *          ochar_sound(obj)
 *          obj->a10 = 0x1e
 *      }
 *
 * **A sound every thirty frames, on top of the ordinary rate step.** 0x44 counts
 * down and is reloaded with 0x1e when it runs out, so sound 6 plays once per
 * thirty calls -- the spin's repeating whoosh. The 6 is built as `adds r3, #6` on
 * a register the branch has just proved to be zero, which is why nothing loads it.
 */
long next_anirate(MK3OBJ *obj);
void ochar_sound(MK3OBJ *obj);

void next_lao_anirate(MK3OBJ *obj)
{
    next_anirate(obj);

    obj->a10 = obj->a10 - 1;
    if (obj->a10 == 0) {
        obj->field1c = 6;
        ochar_sound(obj);
        obj->a10 = 0x1e;
    }
}

/* --------------------------------------------------------------- air_combo_setup
 *
 * armv7 0x0004d0d0, 68 bytes.  **Complete.**
 *
 *      obj->field1c = obj->field00->field28
 *      if (that == 0) {
 *          obj->field1c = (uint32_t)set_no_block
 *          call_a0_for_him(obj)
 *      }
 *      ground_player(obj); match_me_with_him(obj); flip_multi(obj)
 *      obj->field1c = ~0x3f = -0x40
 *      obj->field20 = -0x40 + 0x40 = 0
 *      multi_adjust_xy(obj)
 *
 * **Blocking is taken off the other fighter only when the proc's 0x28 is
 * clear**, through the A0 mechanism again -- `set_no_block` off pointer slot
 * 0x000f3184. A non-zero 0x28 means somebody already did it and the call is
 * skipped.
 *
 * The adjust pair is -0x40 and 0, built from one `mvn` and one `adds`, so the
 * fighter is moved sixty-four units along the first axis and none along the
 * second.
 */
void set_no_block(MK3OBJ *obj);                  /* pointer slot 0x000f3184 */
void match_me_with_him(MK3OBJ *obj);
void flip_multi(MK3OBJ *obj);
void ground_player(MK3OBJ *obj);
void multi_adjust_xy(MK3OBJ *obj);

void air_combo_setup(MK3OBJ *obj)
{
    obj->field1c = obj->field00->field28;
    if (obj->field1c == 0) {
        obj->field1c = (uint32_t)(uintptr_t)set_no_block;
        call_a0_for_him(obj);
    }

    ground_player(obj);
    match_me_with_him(obj);
    flip_multi(obj);

    obj->field1c = (uint32_t)~0x3fu;
    obj->field20 = (uint32_t)(~0x3fu + 0x40u);
    multi_adjust_xy(obj);
}


/* ------------------------------------------------------------- tl_stat_do_axe_up
 *
 * armv7 0x0004ca98, 68 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field48 = 0xa
 *      obj->field40 = 0x00030002
 *      frame[frame].handler = t_axeup3
 *      frame[frame+1].w0 = 0
 *
 * Two constants and an install. **0x40 arrives packed again** -- 0x00030002,
 * a fifth site for that shape after 0x0005000d, 0x00040021, 0x00040047 and
 * 0x00030021, and the first to feed a routine other than t_animate_a9. The high
 * half is 3 and the low half 2, both small, which fits the two-independent-halves
 * reading and still does not name either.
 */
long t_axeup3(MK3THREAD *thread);

long tl_stat_do_axe_up(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field48 = 0xa;
    obj->field40 = 0x00030002;

    return mk3_install(thread, (MK3THREADFUNC)t_axeup3);
}

/* -------------------------------------------------------------- noogy_early_check
 *
 * armv7 0x0004e320, 72 bytes.  **Complete.**
 *
 *      if ((int16)obj->field00->field7c != 0)
 *          obj->field1c = (uint32_t)(G + 0x3a8)
 *      else
 *          obj->field1c = (uint32_t)(G + 0x3ac)
 *      get_tsl_px(obj, obj)
 *      if (obj->field20 > 7) {
 *          obj->field1c = 1
 *          obj->field00->field28 = 1
 *      }
 *
 * **Which of two per-player halfword sets is consulted depends on the
 * four-button gate.** proc+0x7c is that gate, read signed with `ldrsh` as it is
 * everywhere else; non-zero picks the set at G + 0x3a8 and zero the one at
 * G + 0x3ac. `read_lp_tick_state` reads the second of the two directly, and
 * `t_jax_slam_sleep` hands the first to the same `get_tsl_px`, so the four bytes
 * between them separate two button histories rather than two players.
 *
 * More than seven raises a flag in the proc's 0x28 -- the same field
 * air_combo_setup tests before taking blocking away -- so a long enough press is
 * what this routine is checking for and 0x28 is where it says so.
 */
void get_tsl_px(MK3OBJ *obj, MK3OBJ *ref);

void noogy_early_check(MK3OBJ *obj)
{
    if ((int16_t)obj->field00->field7c != 0)
        obj->field1c = (uint32_t)(uintptr_t)(G_BYTES + 0x3a8);
    else
        obj->field1c = (uint32_t)(uintptr_t)(G_BYTES + 0x3ac);

    get_tsl_px(obj, obj);

    if ((long)obj->field20 > 7) {
        obj->field1c = 1;
        obj->field00->field28 = 1;
    }
}

/* ------------------------------------------------------------------------ t_ret9
 *
 * armv7 0x0004d84c, 80 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      back_to_normal(obj)
 *      if (thread->frame > 0) thread->frame -= 1
 *      else frame[frame].handler = t_local_reaction_exit
 *
 * One call and a pop -- the plain "put the fighter back and hand this level
 * back" routine, with the same empty-stack fallback the whole logic module uses.
 */
void back_to_normal(MK3OBJ *obj);

long t_ret9(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    back_to_normal(obj);

    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
        return 0;
    }

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* --------------------------------------------------------------- t_do_stationary
 *
 * armv7 0x0004cda0, 104 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      h = stat_jumps[obj->field1c]
 *      obj->field1c = h
 *      frame[frame].handler = h ? h : t_local_reaction_exit
 *      frame[frame+1].w0 = 0
 *
 * **A jump table indexed by 0x1c, not by the character.** `t_do_body_slam` and
 * `t_do_air_slam` index their tables by `obj->field08->field24`; this one uses
 * whatever the caller left in 0x1c, so it is a dispatch on the requested move
 * rather than on who is performing it. A zero entry falls back to
 * t_local_reaction_exit, which is how the table encodes "no such move" without a
 * bound check.
 *
 * The fallback path writes the loaded zero into the token slot, so the same
 * register serves as both the missing handler and the zero the install needs.
 */
extern uint32_t stat_jumps[];                    /* 0x00167434 */

long t_do_stationary(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t h;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    h = stat_jumps[obj->field1c];
    obj->field1c = h;

    if (h != 0) {
        mk3_frame(thread, thread->frame)[1] = h;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

/* ------------------------------------------------------------- t_stat_do_hi_kick
 *
 * armv7 0x0004e660, 108 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      init_special(obj)
 *      obj->field1c = 0
 *      *(uint32_t *)((char *)obj->field00 + 0x58) = 0
 *      rsnd_func(obj, 0xf)
 *      obj->field1c = 0; group_sound(obj)
 *      obj->field1c = 1
 *      obj->field48 = 0
 *      obj->field20 = 1 + 0x102 = 0x103
 *      obj->field40 = 0x103 - 0xf2 = 0x11
 *      frame[frame].handler = t_kick2
 *
 * **This and t_stat_do_lo_kick are the same routine with one added to three
 * numbers.** High kick: action 0x103, animation 0x11, proc+0x58 and 0x48 both
 * zero. Low kick: action 0x104, animation 0x12, both of those one. So the pair
 * encodes height as a single increment in three places at once, and 0x48 and the
 * proc's 0x58 carry it as a flag while 0x20 and 0x40 carry it as an index.
 *
 * proc+0x58 is inside a pad in this header, so it is written by offset.
 *
 * The 0xf handed to rsnd_func is the same in both, and 0x1c is zeroed twice --
 * once for the proc store and once for group_sound.
 */
void init_special(MK3OBJ *obj);
void group_sound(MK3OBJ *obj);
long t_kick2(MK3THREAD *thread);

long t_stat_do_hi_kick(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    init_special(obj);

    obj->field1c = 0;
    *(uint32_t *)((char *)obj->field00 + 0x58) = 0;
    rsnd_func(obj, 0xf);

    obj->field1c = 0;
    group_sound(obj);

    obj->field1c = 1;
    obj->field48 = 0;
    obj->field20 = 1 + 0x102;
    obj->field40 = (1 + 0x102) - 0xf2;

    return mk3_install(thread, (MK3THREADFUNC)t_kick2);
}

/* ------------------------------------------------------------- t_stat_do_lo_kick
 *
 * armv7 0x0004e6cc, 104 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      init_special(obj)
 *      rsnd_func(obj, 0xf)
 *      obj->field1c = 0; group_sound(obj)
 *      obj->field40 = 0x12
 *      *(uint32_t *)((char *)obj->field00 + 0x58) = 0x12 - 0x11 = 1
 *      obj->field1c = 1
 *      obj->field48 = 1
 *      obj->field20 = 0x104
 *      frame[frame].handler = t_kick2
 *
 * t_stat_do_hi_kick with 1 where that has 0 and the next number where it has the
 * previous one. The order of the stores differs -- this one sets 0x40 first and
 * derives the flag from it, the other sets the flag first and derives 0x40 from
 * the action -- but the values that land are the pair described above.
 */
long t_stat_do_lo_kick(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    init_special(obj);
    rsnd_func(obj, 0xf);

    obj->field1c = 0;
    group_sound(obj);

    obj->field40 = 0x12;
    *(uint32_t *)((char *)obj->field00 + 0x58) = 0x12 - 0x11;
    obj->field1c = 1;
    obj->field48 = 1;
    obj->field20 = 0x104;

    return mk3_install(thread, (MK3THREADFUNC)t_kick2);
}


/* ----------------------------------------------------------- t_stat_do_duck_kickh
 *
 * armv7 0x0004e7a4, 112 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0; group_sound(obj)
 *      rsnd_func(obj, 0xe)
 *      init_special(obj)
 *      obj->field1c = 3
 *      obj->field20 = 0x106
 *      obj->field40 = 0x106 - 0xfd = 9
 *      obj->a10    = 9 - 4 = 5
 *      obj->field48 = 6
 *      *(uint32_t *)((char *)obj->field00 + 0x58) = 6
 *      frame[frame].handler = t_striker
 *
 * The crouching counterpart of t_stat_do_hi_kick, ending in t_striker rather
 * than t_kick2 -- so a duck kick is a strike and a standing kick is not, which is
 * the real difference between the two pairs.
 *
 * **This pair is NOT the clean increment the standing pair is.** High/low
 * standing differ by exactly one in four places; high/low ducking differ by one
 * in the action (0x106 against 0x107) and the animation (9 against 0xa), by three
 * in 0x44 (5 against 2) and by one the other way in 0x48 and proc+0x58 (6 against
 * 7). And the high variant has the LOWER action number. So the two pairs are not
 * built to one rule, and reading either one from the other would get three
 * numbers wrong.
 */
long t_striker(MK3THREAD *thread);               /* pointer slot 0x000f3880 */

long t_stat_do_duck_kickh(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0;
    group_sound(obj);
    rsnd_func(obj, 0xe);
    init_special(obj);

    obj->field1c = 3;
    obj->field20 = 0x106;
    obj->field40 = 0x106 - 0xfd;
    obj->a10     = (0x106 - 0xfd) - 4;
    obj->field48 = 6;
    *(uint32_t *)((char *)obj->field00 + 0x58) = 6;

    return mk3_install(thread, (MK3THREADFUNC)t_striker);
}

/* ----------------------------------------------------------- t_stat_do_duck_kickl
 *
 * armv7 0x0004e734, 112 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0; group_sound(obj)
 *      rsnd_func(obj, 0xe)
 *      init_special(obj)
 *      obj->field20 = 0x107
 *      obj->field40 = 0x107 - 0xfd = 0xa
 *      obj->field1c = 2
 *      obj->a10     = 2
 *      obj->field48 = 2 + 5 = 7
 *      *(uint32_t *)((char *)obj->field00 + 0x58) = 7
 *      frame[frame].handler = t_striker
 *
 * The low duck kick. Here 0x1c and 0x44 take the same value, 2, and 0x48 is
 * derived from it as `+5`; the high variant derives 0x44 from the animation
 * instead and writes 0x48 as a plain literal. Same four fields, different
 * arithmetic, so only the results are meaningful.
 */
long t_stat_do_duck_kickl(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0;
    group_sound(obj);
    rsnd_func(obj, 0xe);
    init_special(obj);

    obj->field20 = 0x107;
    obj->field40 = 0x107 - 0xfd;
    obj->field1c = 2;
    obj->a10     = 2;
    obj->field48 = 2 + 5;
    *(uint32_t *)((char *)obj->field00 + 0x58) = 2 + 5;

    return mk3_install(thread, (MK3THREADFUNC)t_striker);
}

/* ---------------------------------------------------------------- t_noog_lineup_1
 *
 * armv7 0x0004ea38, 108 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      match_him_with_me_f(obj)
 *      him = (MK3OBJ *)obj->field00->him
 *      obj->field1c = ochar_noogy_lineups[him->field24]
 *      adjust_him_a0(obj)
 *      if (thread->frame > 0) thread->frame -= 1
 *      else frame[frame].handler = t_local_reaction_exit
 *
 * **A per-character positional correction indexed by the OTHER fighter.** The
 * table is words, the index is `him->field24`, and the value goes to
 * adjust_him_a0 through 0x1c -- the same field-as-argument convention again, and
 * the same "indexed by who is on the receiving end" choice `t_slam_damage` makes
 * for its damage table.
 *
 * So lining a noogy up depends on how tall the victim is, not on who is giving
 * it.
 */
extern uint32_t ochar_noogy_lineups[];           /* 0x001673cc */
void match_him_with_me_f(MK3OBJ *obj);
void adjust_him_a0(MK3OBJ *obj);

long t_noog_lineup_1(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    MK3OBJ *him;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    match_him_with_me_f(obj);

    him = (MK3OBJ *)(void *)(uintptr_t)obj->field00->him;
    obj->field1c = ochar_noogy_lineups[him->field24];
    adjust_him_a0(obj);

    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
        return 0;
    }

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* --------------------------------------------------------------- t_do_jumpup_kick
 *
 * armv7 0x0004d54c, 116 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = obj->field00->field18 = 0x10b
 *      face_opponent(obj)
 *      obj->field1c = 0; group_sound(obj)
 *      rsnd_func(obj, 0xe)
 *      obj->field1c = 0xa
 *      *(uint32_t *)((char *)obj->field00 + 0x58) = 0xa
 *      obj->field20 = 0xa + 6  = 0x10
 *      obj->field24 = 0x10 - 0xd = 3
 *      obj->field40 = 3 + 0x14 = 0x17
 *      frame[frame].handler = t_jk6
 *
 * **A third pair, and this one shares a value the other two do not.** With
 * t_do_jumpup_punch below: action 0x10b against 0x10c, proc+0x58 0xa against 9,
 * 0x20 0x10 against 0xf, 0x40 0x17 against 0x18 -- **and 0x24 is 3 in both**,
 * reached by different arithmetic each time (0x10 minus 0xd here, 0xf minus 0xc
 * there). So the compiler chained from a different starting number in each and
 * both landed on 3 deliberately.
 *
 * The five constants come off one literal by `adds` and `subs`, as in the knee
 * and elbow; only the results mean anything.
 */
long t_jk6(MK3THREAD *thread);

long t_do_jumpup_kick(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x10b;
    obj->field00->field18 = 0x10b;
    face_opponent(obj);

    obj->field1c = 0;
    group_sound(obj);
    rsnd_func(obj, 0xe);

    obj->field1c = 0xa;
    *(uint32_t *)((char *)obj->field00 + 0x58) = 0xa;
    obj->field20 = 0xa + 6;
    obj->field24 = (0xa + 6) - 0xd;
    obj->field40 = ((0xa + 6) - 0xd) + 0x14;

    return mk3_install(thread, (MK3THREADFUNC)t_jk6);
}

/* -------------------------------------------------------------- t_do_jumpup_punch
 *
 * armv7 0x0004d5c0, 116 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = obj->field00->field18 = 0x10c
 *      face_opponent(obj)
 *      obj->field1c = 0; group_sound(obj)
 *      rsnd_func(obj, 0xe)
 *      obj->field1c = 9
 *      *(uint32_t *)((char *)obj->field00 + 0x58) = 9
 *      obj->field20 = 9 + 6 = 0xf
 *      obj->field24 = 0xf - 0xc = 3
 *      obj->field40 = 3 + 0x15 = 0x18
 *      frame[frame].handler = t_jk6
 *
 * t_do_jumpup_kick with the four numbers above and the same 3 in 0x24. Both
 * install t_jk6, so the jumping punch and the jumping kick share their whole
 * continuation and differ only in what they set up.
 */
long t_do_jumpup_punch(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x10c;
    obj->field00->field18 = 0x10c;
    face_opponent(obj);

    obj->field1c = 0;
    group_sound(obj);
    rsnd_func(obj, 0xe);

    obj->field1c = 9;
    *(uint32_t *)((char *)obj->field00 + 0x58) = 9;
    obj->field20 = 9 + 6;
    obj->field24 = (9 + 6) - 0xc;
    obj->field40 = ((9 + 6) - 0xc) + 0x15;

    return mk3_install(thread, (MK3THREADFUNC)t_jk6);
}


/* ------------------------------------------------------------- t_land_on_yer_feet
 *
 * armv7 0x0004cca4, 140 bytes.  **Complete.**
 *
 *      token == 0:      obj->field1c = 0
 *                       obj->field20 = 0
 *                       obj->field24 = 0x8000
 *                       obj->field28 = 0xfff
 *                       token := 0x9aa, descend into t_flight
 *
 *      token == 0x9aa:  frame[frame].handler = t_jump_up_land_jump
 *
 *      otherwise:       return -3
 *
 * **A seventh flight, and the first with BOTH components zero.** 0x24 is 0x8000
 * again -- seventh site, and here it is a plain literal rather than the end of a
 * chain, which is the clearest statement yet that 0x8000 is what a flight's 0x24
 * is meant to be. 0x28 is 0xfff, matching the drop-downs rather than the throws.
 *
 * With 0x1c and 0x20 both zero the fighter is given no horizontal or vertical
 * component at all, so the flight is whatever 0x24 and 0x28 alone produce -- which
 * is what landing on your feet after being thrown should look like.
 */
long t_jump_up_land_jump(MK3THREAD *thread);
long t_flight(MK3THREAD *thread);                /* pointer slot 0x000f3720 */

long t_land_on_yer_feet(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field1c = 0;
        obj->field20 = 0;
        obj->field24 = 0x8000;
        obj->field28 = 0xfff;

        *mk3_frame(thread, thread->frame + 1) = 0x9aa;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x9aa)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_jump_up_land_jump);
}

/* -------------------------------------------------------------- tl_do_jade_flash
 *
 * armv7 0x0004ff28, 128 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      init_special(obj)
 *      p = NewThreadProcPid(obj, t_jade_flash_proc, 0x11f)
 *      obj->field1c = (uint32_t)(G + 0x410 + 0xc)
 *      *(uint32_t *)((char *)p + 0x44) = (uint32_t)obj
 *      update_tsl(obj)
 *      if (thread->frame > 0) thread->frame -= 1
 *      else frame[frame].handler = t_local_reaction_exit
 *
 * **It spawns a second thread and gives that thread a pointer back to this
 * object.** NewThreadProcPid returns the new proc and its 0x44 -- the argument
 * slot, as everywhere else -- receives this object, so t_jade_flash_proc runs
 * independently and knows whose flash it is. That is how an effect outlives the
 * state that started it.
 *
 * 0x1c then takes `G + 0x41c` for update_tsl, built as 0x410 plus 0xc, which is
 * the same address-in-0x1c convention update_block_fk uses with G + 0x424.
 *
 * The routine pops its own level immediately afterwards, so the flash is entirely
 * the spawned thread's business from here.
 */
void *NewThreadProcPid(void *owner, MK3THREADFUNC func, uint32_t pid);
long t_jade_flash_proc(MK3THREAD *thread);

long tl_do_jade_flash(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    void   *p;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    init_special(obj);

    p = NewThreadProcPid(obj, (MK3THREADFUNC)t_jade_flash_proc, 0x11f);

    obj->field1c = (uint32_t)(uintptr_t)(G_BYTES + 0x410 + 0xc);
    *(uint32_t *)((char *)p + 0x44) = (uint32_t)(uintptr_t)obj;

    update_tsl(obj);

    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
        return 0;
    }

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* ------------------------------------------------------------- t_shake_suspended
 *
 * armv7 0x0004de98, 140 bytes.  **Complete.**
 *
 *      token == 0:      obj->field38 = 4
 *                       *(uint16_t *)(G + 0x456) = 4
 *                       token := 0x581, park 2
 *
 *      token == 0x581:  get_his_action(obj)
 *                       frame[frame].handler =
 *                           (obj->field20 == 0x111) ? t_shake_suspended
 *                                                   : t_local_reaction_exit
 *
 *      otherwise:       return -3
 *
 * **A poll that reinstalls ITSELF, which restarts it at state 0.** The install
 * zeroes the token, so a match on 0x111 does not resume at 0x581 -- it re-enters
 * from the top, writing 4 into 0x38 and into G + 0x456 again and parking another
 * two frames. So the loop body is state 0 and state 0x581 is only the test.
 *
 * That is a different loop shape from every other one in the module, where a
 * state re-arms by writing its own token back. Reading `frame[frame].handler =
 * t_shake_suspended` as "carry on where we were" would get the repeated writes
 * wrong.
 *
 * get_his_action answers in 0x20, and 0x111 is the action being waited for -- so
 * the shake lasts exactly as long as the other fighter stays in it.
 */
void get_his_action(MK3OBJ *obj);
long t_shake_suspended(MK3THREAD *thread);

long t_shake_suspended(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field38 = 4;
        *(uint16_t *)(G_BYTES + 0x456) = 4;

        *mk3_frame(thread, thread->frame + 1) = 0x581;
        thread->fieldfc = 2;
        return 2;
    }

    if (token != 0x581)
        return -3;

    get_his_action(obj);

    if (obj->field20 == 0x111)
        return mk3_install(thread, (MK3THREADFUNC)t_shake_suspended);

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

/* --------------------------------------------------------------- t_mid_air_pause
 *
 * armv7 0x0004ce08, 152 bytes.  **Complete.**
 *
 *      token == 0:      push obj->field1c
 *                       stop_me_player(obj)
 *                       obj->field1c = pop
 *                       token := 0x955, park obj->field1c
 *
 *      token == 0x955:  pop a level, or t_local_reaction_exit at the bottom
 *
 *      otherwise:       return -3
 *
 * The same save-across-one-call idiom as t_grab_animation: 0x1c goes onto the
 * thread's argument stack, `stop_me_player` runs, and the value comes straight
 * back to be used as the park duration. One push, one pop, one call between them.
 *
 * So the caller's 0x1c is how long the pause lasts, and the routine exists only to
 * stop the fighter without losing that number.
 */
long t_mid_air_pause(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    uint32_t cur;

    if (token == 0) {
        cur = thread->fieldf8;
        *mk3_arg(thread, cur) = obj->field1c;
        thread->fieldf8 = cur + 1;

        stop_me_player(obj);

        cur = thread->fieldf8 - 1;
        thread->fieldf8 = cur;
        obj->field1c = *mk3_arg(thread, cur);

        *mk3_frame(thread, thread->frame + 1) = 0x955;
        thread->fieldfc = obj->field1c;
        return (long)obj->field1c;
    }

    if (token != 0x955)
        return -3;

    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
        return 0;
    }

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* ------------------------------------------------------------- t_noogy_suspended
 *
 * armv7 0x0004df24, 160 bytes.  **Complete.**
 *
 *      token == 0:      obj->field38 = 4
 *                       *(uint16_t *)(G + 0x456) = 4
 *                       token := 0x498, park 2
 *
 *      token == 0x498:  get_his_action(obj)
 *                       if (obj->field20 == 0x112) {
 *                           frame[frame].handler = t_noogy_suspended
 *                           frame[frame+1].w0 = obj->field20 - 0x112
 *                       } else
 *                           frame[frame].handler = t_local_reaction_exit
 *
 *      otherwise:       return -3
 *
 * **t_shake_suspended's twin, waiting on action 0x112 instead of 0x111.** Same
 * two states, same reinstall-itself loop that restarts at state 0, same 4 into
 * 0x38 and G + 0x456 on every pass.
 *
 * The zero for the token slot is computed as `obj->field20 - 0x112` -- from the
 * value the branch has just proved equal to 0x112 -- rather than loaded. The same
 * trick t_jax_slam uses with its dereferenced zero, and t_shake_suspended does
 * not: that one loads a plain zero. Two spellings of the same store in twinned
 * routines.
 */
long t_noogy_suspended(MK3THREAD *thread);

long t_noogy_suspended(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field38 = 4;
        *(uint16_t *)(G_BYTES + 0x456) = 4;

        *mk3_frame(thread, thread->frame + 1) = 0x498;
        thread->fieldfc = 2;
        return 2;
    }

    if (token != 0x498)
        return -3;

    get_his_action(obj);

    if (obj->field20 == 0x112) {
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_noogy_suspended;
        *mk3_frame(thread, thread->frame + 1) = obj->field20 - 0x112;
        return 0;
    }

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

/* ------------------------------------------------------------ t_jade_flash_sleep
 *
 * armv7 0x0004fd0c, 136 bytes.  **Complete.**
 *
 *      token == 0:      token := 0xed, park 3
 *
 *      token == 0xed:   f = *(uint32_t *)((char *)obj->a10 + 0x10)
 *                       obj->field2c = f
 *                       if ((f & 4) != 0) {
 *                           jade_normpal(obj)
 *                           token := 0xf4, return 0x16462 (delete)
 *                       } else {
 *                           pop a level, or t_local_reaction_exit
 *                       }
 *
 *      otherwise:       return -3
 *
 * **It watches a flag on the object named in 0x44, not on itself.** That is the
 * object tl_do_jade_flash writes into the spawned proc's 0x44, so this routine
 * reads bit 2 of its 0x10 and acts when whoever owns the flash sets it.
 *
 * **0x16462 is the park-and-never-wake duration**, already recorded in this
 * project's notes, so the 0xf4 state is a terminal park -- the routine restores
 * the palette and then sleeps for good rather than exiting. The alternative is the
 * ordinary pop, and which of the two happens is decided entirely by that one bit.
 *
 * The `ands` leaves the masked value in the register the else path returns, so
 * the zero the token slot receives is the failed test itself.
 */
void jade_normpal(MK3OBJ *obj);

long t_jade_flash_sleep(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    uint32_t f;

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0xed;
        thread->fieldfc = 3;
        return 3;
    }

    if (token != 0xed)
        return -3;

    f = *(uint32_t *)((char *)(void *)(uintptr_t)obj->a10 + 0x10);
    obj->field2c = f;

    if ((f & 4u) != 0) {
        jade_normpal(obj);
        *mk3_frame(thread, thread->frame + 1) = 0xf4;
        thread->fieldfc = 0x16462;
        return 0x16462;
    }

    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
        return 0;
    }

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* ---------------------------------------------------------------- t_do_unblock_hi
 *
 * armv7 0x0004d7a0, 172 bytes.  **Complete.**
 *
 *      token == 0:      obj->field20 = obj->field00->field18 = 0
 *                       obj->field40 = 0xc
 *                       find_ani_last_frame(obj)
 *                       obj->field40 = obj->field40 - 4
 *                       obj->a10     = obj->field40 - 4
 *                       do_next_a9_frame(obj)
 *                       token := 0x772, park 4
 *
 *      token == 0x772:  obj->field40 = obj->a10
 *                       do_next_a9_frame(obj)
 *                       token := 0x775, park 4
 *
 *      token == 0x775:  pop a level, or t_local_reaction_exit at the bottom
 *
 *      otherwise:       return -3
 *
 * **Unblocking plays the block animation backwards, two frames of it.**
 * find_ani_last_frame leaves the last frame in 0x40; the routine steps back 4,
 * shows that, parks four frames, then steps back another 4 -- parked in 0x44
 * across the wait -- and shows that. So 0x44 is again the save slot and the two
 * `subs r3, #4` are two steps of one frame each, four bytes apart in whatever
 * table 0x40 indexes.
 *
 * The zero written into 0x20 and the proc's 0x18 is the token, still zero on
 * entry.
 */
void find_ani_last_frame(MK3OBJ *obj);
long do_next_a9_frame(MK3OBJ *obj);

long t_do_unblock_hi(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field20 = 0;
        obj->field00->field18 = 0;

        obj->field40 = 0xc;
        find_ani_last_frame(obj);
        obj->field40 = obj->field40 - 4;
        obj->a10     = obj->field40 - 4;
        do_next_a9_frame(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x772;
        thread->fieldfc = 4;
        return 4;
    }

    if (token == 0x772) {
        obj->field40 = obj->a10;
        do_next_a9_frame(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x775;
        thread->fieldfc = 4;
        return 4;
    }

    if (token != 0x775)
        return -3;

    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
        return 0;
    }

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

/* ---------------------------------------------------------------- tl_do_babality
 *
 * armv7 0x0004e994, 164 bytes.  **Complete.**
 *
 *      token == 0:      init_special(obj)
 *                       token := 0x1d0, descend into t_baby_start_pause
 *
 *      token == 0x1d0:  obj->field38 = t_turn_into_a_baby
 *                       takeover_him(obj)
 *                       token := 0x1d5, park 0x50
 *
 *      token == 0x1d5:  frame[frame].handler = tl_babality_complete
 *
 *      otherwise:       return -3
 *
 * **The victim does the work and the winner just waits eighty frames.** 0x38 is
 * loaded with t_turn_into_a_baby and takeover_him installs it on the other
 * fighter -- the same handover the slam family uses -- so the transformation runs
 * on the victim's thread while this one parks 0x50 and then finishes through
 * tl_babality_complete, which is already written above.
 *
 * Eighty frames is the longest single park in this file, which is about right for
 * a finisher animation.
 */
void takeover_him(MK3OBJ *obj);
long t_baby_start_pause(MK3THREAD *thread);
long t_turn_into_a_baby(MK3THREAD *thread);

long tl_do_babality(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        init_special(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x1d0;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_baby_start_pause;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x1d0) {
        obj->field38 = (uint32_t)(uintptr_t)t_turn_into_a_baby;
        takeover_him(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x1d5;
        thread->fieldfc = 0x50;
        return 0x50;
    }

    if (token != 0x1d5)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)tl_babality_complete);
}


/* ------------------------------------------------------------------------ t_kick2
 *
 * armv7 0x0004c9e4, 180 bytes.  **Complete.**
 *
 *      token == 0:      obj->a10 = 6
 *                       token := 0x31a, descend into t_striker
 *
 *      token == 0x31a:  if (obj->field5c == 0) -- into the 0x31e body --
 *                       token := 0x31e, park 0xc
 *
 *      token == 0x31e:  obj->field20 = 0x60b
 *                       obj->field1c = 0x60b - 0x608 = 3
 *                       frame[frame].handler = t_retract_strike_act
 *
 *      otherwise:       return -3
 *
 * **The same three-state shape as t_do_knee and t_do_elbow in mkcombo.c**, and it
 * retracts through the very routine t_do_elbow uses. Set up, descend into
 * t_striker, and when t_striker answers in 0x5c: a hit waits twelve frames before
 * retracting, a miss retracts at once. So the strike-and-retract idiom is shared
 * across the two files and only the constants differ.
 *
 * All four of t_stat_do_hi_kick, t_stat_do_lo_kick, t_do_jumpup_kick and
 * t_do_jumpup_punch install this, which is why those four carry only setup.
 */
long t_striker(MK3THREAD *thread);
long t_retract_strike_act(MK3THREAD *thread);

long t_kick2(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->a10 = 6;

        *mk3_frame(thread, thread->frame + 1) = 0x31a;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_striker;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x31a && obj->field5c != 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x31e;
        thread->fieldfc = 0xc;
        return 0xc;
    }

    if (token != 0x31a && token != 0x31e)
        return -3;

    obj->field20 = 0x60b;
    obj->field1c = 0x60b - 0x608;

    return mk3_install(thread, (MK3THREADFUNC)t_retract_strike_act);
}

/* ------------------------------------------------------------- t_baby_start_pause
 *
 * armv7 0x0004fd94, 172 bytes.  **Complete.**
 *
 *      token == 0:      obj->field20 = 5
 *                       token := 0x1c7, descend into t_init_death_blow
 *
 *      token == 0x1c7:  obj->field40 = 0
 *                       pose_a9_manual(obj)
 *                       token := 0x1ca, park 0x20
 *
 *      token == 0x1ca:  pop a level, or t_local_reaction_exit at the bottom
 *
 *      otherwise:       return -3
 *
 * **A babality goes through the death-blow machinery first.** State 0 sets 0x20
 * to 5 and descends into t_init_death_blow off pointer slot 0x000f3194 -- the same
 * routine an ordinary finisher would use -- and only afterwards does the routine
 * pose animation zero by hand and wait thirty-two frames.
 *
 * `pose_a9_manual` with 0x40 zeroed is the same pair t_smoke_slam uses to
 * reposition, so animation zero posed manually is how a fighter is put into a
 * neutral frame.
 */
long t_init_death_blow(MK3THREAD *thread);        /* pointer slot 0x000f3194 */
void pose_a9_manual(MK3OBJ *obj);

long t_baby_start_pause(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field20 = 5;

        *mk3_frame(thread, thread->frame + 1) = 0x1c7;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_init_death_blow;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x1c7) {
        obj->field40 = 0;
        pose_a9_manual(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x1ca;
        thread->fieldfc = 0x20;
        return 0x20;
    }

    if (token != 0x1ca)
        return -3;

    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
        return 0;
    }

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* ---------------------------------------------------------- t_stat_do_duck_punch
 *
 * armv7 0x0004e814, 176 bytes.  **Complete.**
 *
 *      token == 0:      obj->field1c = 0; group_sound(obj)
 *                       rsnd_func(obj, 0xe)
 *                       init_special(obj)
 *                       obj->field1c = 3
 *                       obj->field20 = 0x108
 *                       obj->field40 = 0x108 - 0x100 = 8
 *                       obj->a10     = 8 - 7 = 1
 *                       obj->field48 = 5
 *                       *(uint32_t *)((char *)obj->field00 + 0x58) = 5
 *                       token := 0x284, descend into t_striker
 *
 *      token == 0x284:  obj->field1c = 3
 *                       frame[frame].handler = t_retract_strike
 *
 *      otherwise:       return -3
 *
 * **The third crouching attack, and it differs from the two duck kicks in two
 * structural ways, not just in numbers.** They INSTALL t_striker and are finished;
 * this one DESCENDS into it and comes back to retract. And it retracts through
 * t_retract_strike where t_kick2 uses t_retract_strike_act -- the two variants
 * both live in this file.
 *
 * Its numbers continue the crouching set: action 0x108 after the kicks' 0x106 and
 * 0x107, animation 8 below their 9 and 0xa. So the three share one block of action
 * numbers and one of animations, running in opposite directions.
 */
long t_retract_strike(MK3THREAD *thread);

long t_stat_do_duck_punch(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field1c = 0;
        group_sound(obj);
        rsnd_func(obj, 0xe);
        init_special(obj);

        obj->field1c = 3;
        obj->field20 = 0x108;
        obj->field40 = 0x108 - 0x100;
        obj->a10     = (0x108 - 0x100) - 7;
        obj->field48 = 5;
        *(uint32_t *)((char *)obj->field00 + 0x58) = 5;

        *mk3_frame(thread, thread->frame + 1) = 0x284;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_striker;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x284)
        return -3;

    obj->field1c = 3;
    return mk3_install(thread, (MK3THREADFUNC)t_retract_strike);
}

/* ------------------------------------------------------------- t_jade_flash_proc
 *
 * armv7 0x0004ffa8, 188 bytes.  **Complete.**
 *
 *      token == 0:      obj->field40 = 0xa
 *                       -- into the swap body --
 *
 *      the swap body:   player_swpal((MK3OBJ *)obj->a10, 2)
 *                       token := 0xfe, descend into t_jade_flash_sleep
 *
 *      token == 0xfe:   jade_normpal(obj)
 *                       token := 0x100, descend into t_jade_flash_sleep
 *
 *      token == 0x100:  obj->field1c = 0xc; ochar_sound(obj)
 *                       if (--obj->field40 > 0) -- back to the swap body --
 *                       token := 0x108, return 0x16462 (delete)
 *
 *      otherwise:       return -3
 *
 * **Ten flashes, then a park that never ends.** 0x40 is loaded with 0xa and
 * counted down; each pass swaps the owner's palette to 2, sleeps, restores it,
 * sleeps, and makes a sound. When the count runs out the thread parks for 0x16462
 * -- the never-wake duration -- under token 0x108, and **that token is not in the
 * dispatch at all.** Reaching it would return -3, which is exactly why it is safe:
 * the park never expires, so the state is a terminator rather than a state.
 *
 * The palette swap is applied to the object named in 0x44, which is the fighter
 * tl_do_jade_flash recorded there when it spawned this thread. So the effect
 * thread never touches its own object except to count.
 */
void player_swpal(MK3OBJ *obj, uint32_t frozen);
long t_jade_flash_sleep(MK3THREAD *thread);

long t_jade_flash_proc(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field40 = 0xa;
        /* falls through to the swap body */

    } else if (token == 0xfe) {
        jade_normpal(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x100;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_jade_flash_sleep;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;

    } else if (token == 0x100) {
        obj->field1c = 0xc;
        ochar_sound(obj);

        obj->field40 = obj->field40 - 1;
        if ((long)obj->field40 <= 0) {
            *mk3_frame(thread, thread->frame + 1) = 0x108;
            thread->fieldfc = 0x16462;
            return 0x16462;
        }
        /* falls through to the swap body */

    } else {
        return -3;
    }

    player_swpal((MK3OBJ *)(void *)(uintptr_t)obj->a10, 2);

    *mk3_frame(thread, thread->frame + 1) = 0xfe;
    thread->frame = thread->frame + 1;              /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_jade_flash_sleep;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------------------ t_r_leg_slammed
 *
 * armv7 0x0004f9d0, 228 bytes.  **Complete.**
 *
 *      token == 0:      obj->field1c = obj->field00->field18 = 0x506
 *                       ground_player(obj)
 *                       obj->field40 = 0x1e
 *                       find_ani_part2(obj); do_next_a9_frame(obj)
 *                       obj->field1c = 2; group_sound(obj)
 *                       shake_n_sound(obj)
 *                       obj->a10 = 0x23
 *                       obj->field1c = obj->field00->p_hit
 *                       if (obj->field1c <= 3) {
 *                           damage_to_me(obj)
 *                           obj->field00->field54 += obj->a10
 *                       }
 *                       obj->field1c = 0x30000
 *                       obj->field20 = 0x30000 - 0x90000 = -0x60000
 *                       obj->field24 = that + 0x68000 = 0x8000
 *                       obj->field28 = 4
 *                       token := 0x38a, descend into t_flight
 *
 *      token == 0x38a:  frame[frame].handler = t_land_on_my_back
 *
 *      otherwise:       return -3
 *
 * **The damage is gated on the hit counter, and this is the first site to show
 * that.** proc+0x44 -- named `p_hit` in the header, on the authority of
 * zero_my_p_hit -- is compared against 3, and only at or below that do the damage
 * and the accumulator addition happen. So a slam late in a long combo costs
 * nothing, which is a scaling rule the earlier slam routines never revealed
 * because none of them checked.
 *
 * The five calls before it are ground_slammed_init's body inlined, as in
 * t_thrown_by_robo2 -- ground, animation 0x1e, find the part, advance a frame, a
 * sound. And the four flight numbers are t_common_slam's to the word, which makes
 * this the eighth site to land 0x24 on 0x8000.
 */
void find_ani_part2(MK3OBJ *obj);
void shake_n_sound(MK3OBJ *obj);
void damage_to_me(MK3OBJ *obj);
long t_land_on_my_back(MK3THREAD *thread);       /* pointer slot 0x000f3750 */

long t_r_leg_slammed(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field1c = 0x506;
        obj->field00->field18 = 0x506;

        ground_player(obj);
        obj->field40 = 0x1e;
        find_ani_part2(obj);
        do_next_a9_frame(obj);

        obj->field1c = 2;
        group_sound(obj);
        shake_n_sound(obj);

        /* Corrected: the damage is always dealt -- a combo past three hits
         * only drops it from 0x23 to 8 first (`bgt` over the `movs r3, #8`). */
        obj->a10 = 0x23;
        obj->field1c = obj->field00->p_hit;
        if ((long)obj->field1c > 3)
            obj->a10 = 8;
        damage_to_me(obj);
        obj->field00->field54 = obj->field00->field54 + obj->a10;

        obj->field1c = 0x30000;
        obj->field20 = (uint32_t)(0x30000 - 0x90000);
        obj->field24 = (uint32_t)(0x30000 - 0x90000 + 0x68000);
        obj->field28 = 4;

        *mk3_frame(thread, thread->frame + 1) = 0x38a;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x38a)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_land_on_my_back);
}

/* ----------------------------------------------------------------- tl_do_reptile_inv
 *
 * armv7 0x0004f5b8, 204 bytes.  **Complete.**
 *
 *      token == 0:      obj->field20 = 0x30c
 *                       init_special_act(obj)
 *                       obj->field40 = 0x00030001
 *                       token := 0xd3, descend into t_animate2_a9
 *
 *      token == 0xd3:   obj->field1c = 0x1e; create_fx(obj)
 *                       obj->field1c = 9;    ochar_sound(obj)
 *                       token := 0xda, park 0xa
 *
 *      token == 0xda:   set_inviso(obj)
 *                       pop a level, or t_local_reaction_exit at the bottom
 *
 *      otherwise:       return -3
 *
 * **The invisibility is set LAST, after the effect and the sound.** So the
 * animation plays, an effect is created, a sound is made, ten frames pass, and only
 * then does the fighter actually vanish -- which is why the disappearance reads as
 * the end of a move rather than the start of one.
 *
 * `init_special_act` rather than `init_special`, and 0x20 is filled with the action
 * 0x30c before the call rather than after, so the `_act` variant is the one that
 * takes its action from 0x20.
 *
 * 0x40 is 0x00030001 -- a sixth packed word for that field, and the second to feed
 * an animate routine other than t_animate_a9. Six sites now, all with a small high
 * half and a small low half: 0x0005000d, 0x00040021, 0x00040047, 0x00030021,
 * 0x00030002, 0x00030001.
 */
void init_special_act(MK3OBJ *obj);
void set_inviso(MK3OBJ *obj);
void create_fx(MK3OBJ *obj);
long t_animate2_a9(MK3THREAD *thread);           /* pointer slot 0x000f36c0 */

long tl_do_reptile_inv(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field20 = 0x30c;
        init_special_act(obj);
        obj->field40 = 0x00030001;

        *mk3_frame(thread, thread->frame + 1) = 0xd3;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_animate2_a9;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0xd3) {
        obj->field1c = 0x1e;
        create_fx(obj);
        obj->field1c = 9;
        ochar_sound(obj);

        *mk3_frame(thread, thread->frame + 1) = 0xda;
        thread->fieldfc = 0xa;
        return 0xa;
    }

    if (token != 0xda)
        return -3;

    set_inviso(obj);

    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
        return 0;
    }

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* ---------------------------------------------------------- t_stat_do_sweep_kick
 *
 * armv7 0x0004e8c4, 208 bytes.  **Complete.**
 *
 *      token == 0:      init_special(obj)
 *                       *(uint32_t *)((char *)obj->field00 + 0x58) = 4
 *                       obj->field1c = 3
 *                       obj->field20 = 3 + 0x10a = 0x10d
 *                       obj->field48 = 4
 *                       obj->field40 = 0x10d - 0xf9 = 0x14
 *                       obj->a10     = 0x14 - 0x13 = 1
 *                       token := 0x267, descend into t_behind_striker
 *
 *      token == 0x267:  obj->field1c = 5
 *                       if (obj->field5c == 0) -- into the retract, 0x1c left at 5 --
 *                       token := 0x26b, park 6
 *
 *      token == 0x26b:  obj->field1c = 6
 *                       obj->field20 = 0x600
 *                       frame[frame].handler = t_retract_strike_act
 *
 *      otherwise:       return -3
 *
 * **A hit and a miss retract with different values in 0x1c, and the difference is
 * a skipped store.** The 0x267 state writes 5 into 0x1c and, on a miss, branches
 * into the middle of the 0x26b body -- past the `obj->field1c = 6` and straight to
 * the 0x20 store. So a connected sweep retracts with 6 and a missed one with 5,
 * which is one instruction's worth of difference and easy to lose.
 *
 * **It strikes through t_behind_striker, not t_striker.** That is the only site in
 * this file to use it, and a sweep is the one attack that has to hit from behind
 * the legs, so the name and the choice agree.
 *
 * Action 0x10d continues the standing block after the kicks' 0x103/0x104 and the
 * crouching 0x106 to 0x108.
 */
long t_behind_striker(MK3THREAD *thread);         /* pointer slot 0x000f318c */

long t_stat_do_sweep_kick(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        init_special(obj);
        *(uint32_t *)((char *)obj->field00 + 0x58) = 4;

        obj->field1c = 3;
        obj->field20 = 3 + 0x10a;
        obj->field48 = 4;
        obj->field40 = (3 + 0x10a) - 0xf9;
        obj->a10     = ((3 + 0x10a) - 0xf9) - 0x13;

        *mk3_frame(thread, thread->frame + 1) = 0x267;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_behind_striker;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x267) {
        obj->field1c = 5;
        if (obj->field5c != 0) {
            *mk3_frame(thread, thread->frame + 1) = 0x26b;
            thread->fieldfc = 6;
            return 6;
        }
        /* a miss lands past the 0x1c store below, keeping 5 */
    } else if (token == 0x26b) {
        obj->field1c = 6;
    } else {
        return -3;
    }

    obj->field20 = 0x600;
    return mk3_install(thread, (MK3THREADFUNC)t_retract_strike_act);
}

/* ------------------------------------------------------------- t_turn_into_a_baby
 *
 * armv7 0x0004fe40, 232 bytes.  **Complete.**
 *
 *      token == 0:      tsound_func(obj, 0x8c)
 *                       obj->field1c = 0x1e; create_fx(obj)
 *                       token := 0x1a0, park 8
 *
 *      token == 0x1a0:  part = obj->field08
 *                       part->field2c = ochar_babies[part->field24]
 *                       mk3_getbbox(part->field2c,
 *                                   &part->field34, &part->field38,
 *                                   &part->field3c, &part->field40)
 *                       *(uint16_t *)((char *)part + 0x12) =
 *                           *(uint32_t *)(G + 0xac)
 *                           - (part->field40 - part->field38)
 *                       token := 0x1bc, park 0x30
 *
 *      token == 0x1bc:  tsound_func(obj, 0x8d)
 *                       frame[frame].handler = t_wait_forever
 *
 *      otherwise:       return -3
 *
 * **The baby is measured and then stood on the floor.** ochar_babies gives the
 * per-character baby by the victim's own number, mk3_getbbox fills four fields of
 * the part from it -- 0x34, 0x38, 0x3c, 0x40 as four out-parameters -- and the y
 * position is then `floor - (bottom - top)`, with the floor read from G + 0xac. So
 * the height comes out of the bounding box rather than a table, which is why the
 * bbox call has to happen before the placement.
 *
 * This is the first site in the logic module to use mk3_getbbox and the first to
 * read G + 0xac.
 *
 * Two sounds bracket the whole thing, 0x8c at the start and 0x8d at the end, and
 * the routine finishes in t_wait_forever -- the baby never does anything again.
 */
extern uint32_t ochar_babies[];                   /* 0x00167350 */
void mk3_getbbox(long ani, int *p1, int *p2, int *p3, int *p4);
void tsound_func(MK3OBJ *obj, uint32_t arg);
long t_wait_forever(MK3THREAD *thread);

long t_turn_into_a_baby(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    MK3OBJ  *part;

    if (token == 0) {
        tsound_func(obj, 0x8c);
        obj->field1c = 0x1e;
        create_fx(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x1a0;
        thread->fieldfc = 8;
        return 8;
    }

    if (token == 0x1a0) {
        part = obj->field08;
        part->field2c = ochar_babies[part->field24];

        part = obj->field08;
        mk3_getbbox(part->field2c,
                    (int *)&part->field34, (int *)&part->field38,
                    (int *)&part->field3c, (int *)&part->field40);

        part = obj->field08;
        MK3_SET_FIELD12(part,
                        *(uint32_t *)(G_BYTES + 0xac)
                        - (part->field40 - part->field38));

        *mk3_frame(thread, thread->frame + 1) = 0x1bc;
        thread->fieldfc = 0x30;
        return 0x30;
    }

    if (token != 0x1bc)
        return -3;

    tsound_func(obj, 0x8d);
    return mk3_install(thread, (MK3THREADFUNC)t_wait_forever);
}


/* ---------------------------------------------------------- t_stat_do_roundhouse
 *
 * armv7 0x0004eaa4, 220 bytes.  **Complete.**
 *
 *      token == 0:      init_special(obj)
 *                       tsound_func(obj, 2)
 *                       obj->field1c = 0; group_sound(obj)
 *                       obj->field1c = round_speeds[obj->field08->field24]
 *                       obj->field20 = 0x105
 *                       obj->field40 = 0x105 - 0xf0 = 0x15
 *                       obj->a10     = 0x15 - 0x12 = 3
 *                       obj->field48 = 3 + 0xa = 0xd
 *                       *(uint32_t *)((char *)obj->field00 + 0x58) = 0xd
 *                       token := 0x246, descend into t_striker
 *
 *      token == 0x246:  if (obj->field5c == 0) -- into the 0x249 body --
 *                       token := 0x249, park 0xa
 *
 *      token == 0x249:  obj->field1c = 4
 *                       frame[frame].handler = t_retract_strike
 *
 *      otherwise:       return -3
 *
 * **The only attack in this file whose rate comes from a table.** round_speeds is
 * bytes indexed by the character, and the value lands in 0x1c -- so how fast a
 * roundhouse swings is per-fighter where every other kick and punch here uses a
 * literal. That is the one thing the table adds; the four position and flag numbers
 * are constants like all the others.
 *
 * Action 0x105 sits between the standing kicks' 0x103/0x104 and the crouching
 * 0x106, so the whole set is one contiguous block.
 */
extern uint8_t round_speeds[];                    /* 0x001673b0 */

long t_stat_do_roundhouse(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        init_special(obj);
        tsound_func(obj, 2);

        obj->field1c = 0;
        group_sound(obj);

        obj->field1c = round_speeds[obj->field08->field24];
        obj->field20 = 0x105;
        obj->field40 = 0x105 - 0xf0;
        obj->a10     = (0x105 - 0xf0) - 0x12;
        obj->field48 = ((0x105 - 0xf0) - 0x12) + 0xa;
        *(uint32_t *)((char *)obj->field00 + 0x58) = obj->field48;

        *mk3_frame(thread, thread->frame + 1) = 0x246;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_striker;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x246 && obj->field5c != 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x249;
        thread->fieldfc = 0xa;
        return 0xa;
    }

    if (token != 0x246 && token != 0x249)
        return -3;

    obj->field1c = 4;
    return mk3_install(thread, (MK3THREADFUNC)t_retract_strike);
}

/* -------------------------------------------------------------------- tl_do_inviso
 *
 * armv7 0x0004f4c8, 240 bytes.  **Complete.**
 *
 *      token == 0:      init_special(obj)
 *                       obj->field40 = 0xb; get_char_ani2(obj)
 *                       obj->field1c = 2
 *                       token := 0x406, descend into t_mframew
 *
 *      token == 0x406:  obj->field1c = 0x1e; create_fx(obj)
 *                       token := 0x40a, park 6
 *
 *      token == 0x40a:  obj->field2c = obj->field08->field30
 *                       if ((obj->field2c & 0x20) != 0)
 *                           frame[frame].handler = t_do_un_inviso
 *                       else {
 *                           set_inviso(obj)
 *                           frame[frame].handler = t_local_reaction_exit
 *                       }
 *
 *      otherwise:       return -3
 *
 * **It is a toggle, and bit 5 of the part's 0x30 is the switch.** Set means the
 * fighter is already invisible and the routine hands over to t_do_un_inviso; clear
 * means it goes invisible now and exits. So one move covers both directions and
 * nothing outside has to know which way it will go.
 *
 * The animation and the effect happen before the test, so the flourish is the same
 * either way and only the last state differs. `tl_do_reptile_inv` above is the
 * one-way version, which sets inviso and never checks.
 *
 * The masked bit is left in the register the else path writes into the token slot,
 * so the zero the install needs is the failed test again.
 */
void get_char_ani2(MK3OBJ *obj);
long t_do_un_inviso(MK3THREAD *thread);

long tl_do_inviso(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        init_special(obj);

        obj->field40 = 0xb;
        get_char_ani2(obj);
        obj->field1c = 2;

        *mk3_frame(thread, thread->frame + 1) = 0x406;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x406) {
        obj->field1c = 0x1e;
        create_fx(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x40a;
        thread->fieldfc = 6;
        return 6;
    }

    if (token != 0x40a)
        return -3;

    obj->field2c = obj->field08->field30;

    if ((obj->field2c & 0x20u) != 0)
        return mk3_install(thread, (MK3THREADFUNC)t_do_un_inviso);

    set_inviso(obj);
    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* ------------------------------------------------------------------------- t_jk6
 *
 * armv7 0x0004cb80, 292 bytes.  **Complete.**
 *
 *      token == 0:      obj->field28 = 0x8000
 *                       obj->field2c = 4
 *                       token := 0x801, descend into t_air_strike
 *
 *      token == 0x801:  if (obj->field5c != 0) {
 *                           obj->field1c = 8
 *                           token := 0x806, descend into t_combo_air_pause
 *                       }
 *                       -- otherwise into the 0x813 body --
 *
 *      token == 0x806:  obj->field00->field18 = 0x609
 *                       obj->field1c = 3
 *                       obj->field40 += 4
 *                       obj->field20 = 0
 *                       *(uint32_t *)((char *)obj->field00 + 0x34) = 0
 *                       obj->field28 = 0xe000
 *                       obj->field08->field20 = 0xe000
 *                       token := 0x813, descend into t_flight_loop
 *
 *      token == 0x813:  frame[frame].handler = t_jump_up_land_jsrp
 *
 *      otherwise:       return -3
 *
 * **The continuation of both jumping attacks, and a hit costs two extra levels.**
 * A miss goes straight from the strike to the landing. A hit descends into
 * t_combo_air_pause, then into t_flight_loop with a fresh action and velocity, and
 * only then lands -- so connecting in the air is what turns a jump attack into
 * something that can carry a combo.
 *
 * **0x28 is written twice with different magnitudes**: 0x8000 before the strike
 * and 0xe000 after a hit, the second also copied into the part's 0x20. That is the
 * only field this routine sets on both sides of the strike.
 *
 * proc+0x34 is inside a pad in this header, so it is written by offset -- the fifth
 * distinct proc offset this file reaches that way, after 0x2c, 0x30, 0x38 and 0x58.
 *
 * **The dispatch destroys the object pointer.** `movw r2, #0x813` at 0x4cba4
 * overwrites the register holding `obj`, so only the arms whose comparison happens
 * before it -- 0 and 0x801 and 0x806 -- can touch the object at all. The 0x813 arm
 * uses nothing but the frame index it saved at entry, which is why it can afford
 * to lose it.
 */
long t_air_strike(MK3THREAD *thread);
long t_combo_air_pause(MK3THREAD *thread);
long t_flight_loop(MK3THREAD *thread);           /* pointer slot 0x000f317c */
long t_jump_up_land_jsrp(MK3THREAD *thread);     /* pointer slot 0x000f376c */

long t_jk6(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field28 = 0x8000;
        obj->field2c = 4;

        *mk3_frame(thread, thread->frame + 1) = 0x801;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_air_strike;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x801 && obj->field5c != 0) {
        obj->field1c = 8;

        *mk3_frame(thread, thread->frame + 1) = 0x806;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_combo_air_pause;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x806) {
        obj->field00->field18 = 0x609;
        obj->field1c = 3;
        obj->field40 = obj->field40 + 4;
        obj->field20 = 0;
        *(uint32_t *)((char *)obj->field00 + 0x34) = 0;
        obj->field28 = 0xe000;
        obj->field08->field20 = 0xe000;

        *mk3_frame(thread, thread->frame + 1) = 0x813;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_flight_loop;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x801 && token != 0x813)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_jump_up_land_jsrp);
}


/* ---------------------------------------------------------------------- t_axeup3
 *
 * armv7 0x0004da84, 312 bytes.  **Complete.**
 *
 *      token == 0:      obj->field20 = 0x115
 *                       init_special_act(obj)
 *                       obj->field1c = 5; ochar_sound(obj)
 *                       token := 0x6cf, descend into t_animate2_a9
 *
 *      token == 0x6cf:  obj->field00->field28 = 0
 *                       obj->field1c = 3; init_anirate(obj)
 *                       token := 0x6d8, park 1
 *
 *      token == 0x6d8:  obj->field1c = obj->field00->field28
 *                       if (that == 0) {
 *                           obj->field1c = 0x15
 *                           strike_check_a0(obj)
 *                           if (obj->field5c != 0)
 *                               obj->field1c = obj->field00->field28 = 1
 *                       }
 *                       next_anirate(obj)
 *                       obj->field1c = *(uint32_t *)obj->field40
 *                       if (that != 0) token := 0x6d8, park 1
 *                       else {
 *                           obj->field20 = obj->field00->field18 = 0x61c
 *                           obj->field1c = obj->field48
 *                           token := 0x61c + 0xd3 = 0x6ef
 *                           park obj->field1c
 *                       }
 *
 *      token == 0x6ef:  delete_slave(obj)
 *                       obj->field40 += 4
 *                       obj->field1c = 2
 *                       frame[frame].handler = t_mframew
 *
 *      otherwise:       return -3
 *
 * **0x40 is a CURSOR here, and this routine shows both halves of that at once.**
 * The 0x6d8 state dereferences it -- `ldr` then `ldr` again -- and loops while the
 * word it points at is non-zero; the 0x6ef state advances it by 4. So it walks an
 * array of words and stops on a zero terminator, which is a much better reading
 * than "sometimes a number, sometimes a pointer".
 *
 * That reading also covers the earlier sites: t_robo2_slam and t_jax_slam
 * dereference 0x40 without advancing it, t_jk6 advances it by 4, and
 * t_do_unblock_hi steps it back by 4 twice. **It does NOT cover the many routines
 * that hand 0x40 to get_char_ani as a small index** -- the field is genuinely
 * overloaded, and which meaning applies is decided by the routine, not by the
 * field.
 *
 * **The strike is checked at most once per swing.** proc+0x28 is the latch: cleared
 * on entry to the loop, and the strike check only runs while it is clear. A
 * connection sets it, so every later frame skips the check entirely.
 *
 * **The last token is built from the action number** -- 0x61c plus 0xd3 gives
 * 0x6ef -- which is the compiler reusing a register it already had loaded, not a
 * relationship between the two numbers.
 */
void delete_slave(MK3OBJ *obj);
long strike_check_a0(MK3OBJ *obj);
void init_anirate(MK3OBJ *obj);

long t_axeup3(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field20 = 0x115;
        init_special_act(obj);

        obj->field1c = 5;
        ochar_sound(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x6cf;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_animate2_a9;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x6cf) {
        obj->field00->field28 = 0;
        obj->field1c = 3;
        init_anirate(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x6d8;
        thread->fieldfc = 1;
        return 1;
    }

    if (token == 0x6d8) {
        obj->field1c = obj->field00->field28;
        if (obj->field1c == 0) {
            obj->field1c = 0x15;
            strike_check_a0(obj);
            if (obj->field5c != 0) {
                obj->field1c = 1;
                obj->field00->field28 = 1;
            }
        }

        next_anirate(obj);

        obj->field1c = *(uint32_t *)(void *)(uintptr_t)obj->field40;
        if (obj->field1c != 0) {
            *mk3_frame(thread, thread->frame + 1) = 0x6d8;
            thread->fieldfc = 1;
            return 1;
        }

        obj->field20 = 0x61c;
        obj->field00->field18 = 0x61c;
        obj->field1c = obj->field48;

        *mk3_frame(thread, thread->frame + 1) = 0x61c + 0xd3;
        thread->fieldfc = obj->field1c;
        return (long)obj->field1c;
    }

    if (token != 0x6ef)
        return -3;

    delete_slave(obj);
    obj->field40 = obj->field40 + 4;
    obj->field1c = 2;

    return mk3_install(thread, (MK3THREADFUNC)t_mframew);
}


/* ------------------------------------------------------------------ tl_do_reflect
 *
 * armv7 0x0004dbbc, 292 bytes.  **Complete.**
 *
 *      token == 0:      obj->field20 = 0x402
 *                       init_special_act(obj)
 *                       obj->field1c = 7; ochar_sound(obj)
 *                       obj->field40 = 3; get_char_ani2(obj)
 *                       obj->field1c = 5
 *                       token := 0x704, descend into t_mframew
 *
 *      token == 0x704:  obj->field1c = 5
 *                       token := 0x706, descend into t_mframew
 *
 *      token == 0x706:  obj->field20 = obj->field00->field18 = 0x60e
 *                       delete_slave(obj)
 *                       obj->field40 = 3
 *                       find_ani2_part2(obj); find_part2(obj)
 *                       obj->field1c = 4
 *                       token := 0x710, descend into t_mframew
 *
 *      token == 0x710:  frame[frame].handler = t_local_reaction_exit
 *
 *      otherwise:       return -3
 *
 * Three waits of 5, 5 and 4 through t_mframew with the work in the third state:
 * the slave is deleted and the animation parts are re-found for the same 0x40 = 3
 * the entry used, through both find_ani2_part2 and find_part2 in that order.
 *
 * **Here 0x40 is a small index, not a cursor** -- 3 handed to get_char_ani2 and
 * then to find_ani2_part2 -- which is the other half of the overload t_axeup3 shows
 * above. Same field, same file, two meanings, and only the surrounding calls say
 * which.
 *
 * The four states share one install site, entered with the frame index already
 * pushed on three of them and as it was on the fourth, which is the same
 * one-piece-of-code-for-both trick the slam family uses.
 */
void find_ani2_part2(MK3OBJ *obj);
void find_part2(MK3OBJ *obj);

long tl_do_reflect(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    uint32_t next;

    if (token == 0) {
        obj->field20 = 0x402;
        init_special_act(obj);

        obj->field1c = 7;
        ochar_sound(obj);

        obj->field40 = 3;
        get_char_ani2(obj);

        obj->field1c = 5;
        next = 0x704;

    } else if (token == 0x704) {
        obj->field1c = 5;
        next = 0x706;

    } else if (token == 0x706) {
        obj->field20 = 0x60e;
        obj->field00->field18 = 0x60e;

        delete_slave(obj);

        obj->field40 = 3;
        find_ani2_part2(obj);
        find_part2(obj);

        obj->field1c = 4;
        next = 0x710;

    } else if (token == 0x710) {
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

    } else {
        return -3;
    }

    *mk3_frame(thread, thread->frame + 1) = next;
    thread->frame = thread->frame + 1;              /* push a level */
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------------- tl_do_kano_swipe
 *
 * armv7 0x0004d948, 316 bytes.  **Complete.**
 *
 *      token == 0:      obj->field20 = 0x117
 *                       init_special_act(obj)
 *                       obj->field1c = 0; ochar_sound(obj)
 *                       obj->field1c = 0x12
 *                       *(uint32_t *)((char *)obj->field00 + 0x58) = 0x12
 *                       obj->field40 = 1; get_char_ani2(obj)
 *                       obj->field1c = 2
 *                       token := 0x357, descend into t_mframew
 *
 *      token == 0x357:  obj->field1c = 0x12
 *                       strike_check_a0(obj)
 *                       obj->field00->field18 = 0x618
 *                       obj->field1c = 4
 *                       token := 0x35f, descend into t_mframew
 *
 *      token == 0x35f:  token := 0x362, park 8
 *
 *      token == 0x362:  obj->field1c = 4
 *                       token := 0x364, descend into t_mframew
 *
 *      token == 0x364:  pop a level, or t_local_reaction_exit at the bottom
 *
 *      otherwise:       return -3
 *
 * **The strike is checked once, unconditionally, in the middle state.** No latch --
 * where t_axeup3 clears proc+0x28 and re-checks every frame while it stays clear,
 * this routine has exactly one frame on which the swipe can connect. So the two
 * spellings of a strike in this file are "poll until it lands" and "one chance",
 * and which one a move gets is a design choice rather than a shared helper.
 *
 * **0x12 does two jobs from one literal.** In state 0 it goes into the proc's 0x58
 * as a flag, and in the strike state it goes into 0x1c as strike_check_a0's
 * argument. Same number, two fields, two states apart.
 *
 * The sound in state 0 is number zero -- 0x1c still holds the token -- as in
 * t_robo1_slam and t_lia_slam.
 */
long strike_check_a0(MK3OBJ *obj);

long tl_do_kano_swipe(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    uint32_t next;

    if (token == 0) {
        obj->field20 = 0x117;
        init_special_act(obj);

        obj->field1c = 0;               /* the token, still zero */
        ochar_sound(obj);

        obj->field1c = 0x12;
        *(uint32_t *)((char *)obj->field00 + 0x58) = 0x12;

        obj->field40 = 1;
        get_char_ani2(obj);

        obj->field1c = 2;
        next = 0x357;

    } else if (token == 0x357) {
        obj->field1c = 0x12;
        strike_check_a0(obj);

        obj->field00->field18 = 0x618;
        obj->field1c = 4;
        next = 0x35f;

    } else if (token == 0x35f) {
        *mk3_frame(thread, thread->frame + 1) = 0x362;
        thread->fieldfc = 8;
        return 8;

    } else if (token == 0x362) {
        obj->field1c = 4;
        next = 0x364;

    } else if (token == 0x364) {
        if ((long)thread->frame > 0) {
            thread->frame = thread->frame - 1;
            return 0;
        }
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

    } else {
        return -3;
    }

    *mk3_frame(thread, thread->frame + 1) = next;
    thread->frame = thread->frame + 1;              /* push a level */
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ---------------------------------------------------------- t_edge_of_world_lineup
 *
 * armv7 0x0004f06c, 316 bytes.  **Complete.**
 *
 *      token == 0:      token := 0x479, descend into obj->a10
 *
 *      token == 0x479:  get_his_dfe(obj)
 *                       if (obj->field30 > 0x3f && obj->field34 > 0x40)
 *                           -- straight to the pop --
 *                       obj->field1c = 0x60000; away_x_vel(obj)
 *                       push obj->field48
 *                       obj->field48 = 8
 *                       token := 0x487, park 1
 *
 *      token == 0x487:  token := 0x488, descend into obj->a10
 *
 *      token == 0x488:  if (--obj->field48 > 0) token := 0x487, park 1
 *                       stop_me_player(obj)
 *                       token := 0x48d, descend into obj->a10
 *
 *      token == 0x48d:  obj->field48 = pop
 *                       pop a level, or t_local_reaction_exit at the bottom
 *
 *      otherwise:       return -3
 *
 * **0x44 holds a HANDLER here, and the routine descends into it three times.**
 * Every other site in this module treats 0x44 as an argument slot or a save slot;
 * this one loads it and stores it straight into a frame's handler word. So the
 * whole routine is a driver -- "run the caller's routine, then run it once a frame
 * for eight frames, then run it once more and leave" -- and the caller supplies the
 * body.
 *
 * **0x48 is both the counter and something borrowed.** It goes onto the thread's
 * argument stack before being loaded with 8, and it is restored from there in the
 * last state, so whatever the caller had in it survives the loop. That is the
 * push/pop idiom again, this time spanning four states rather than one call.
 *
 * The early exit is the only place `get_his_dfe` appears in this file: it answers
 * in 0x30 and 0x34, and the routine gives up before doing anything when both are
 * above their thresholds -- 0x3f and 0x40, one apart, which is worth noting only
 * because it makes a copy-paste error easy to mistake for intent either way.
 */
void get_his_dfe(MK3OBJ *obj);
void away_x_vel(MK3OBJ *obj);

long t_edge_of_world_lineup(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    uint32_t cur, next;

    if (token == 0) {
        next = 0x479;

    } else if (token == 0x479) {
        get_his_dfe(obj);

        if ((long)obj->field30 > 0x3f && (long)obj->field34 > 0x40)
            goto leave;

        obj->field1c = 0x60000;
        away_x_vel(obj);

        cur = thread->fieldf8;
        *mk3_arg(thread, cur) = obj->field48;
        thread->fieldf8 = cur + 1;

        obj->field48 = 8;

        *mk3_frame(thread, thread->frame + 1) = 0x487;
        thread->fieldfc = 1;
        return 1;

    } else if (token == 0x487) {
        next = 0x488;

    } else if (token == 0x488) {
        obj->field48 = obj->field48 - 1;
        if ((long)obj->field48 > 0) {
            *mk3_frame(thread, thread->frame + 1) = 0x487;
            thread->fieldfc = 1;
            return 1;
        }
        stop_me_player(obj);
        next = 0x48d;

    } else if (token == 0x48d) {
        cur = thread->fieldf8 - 1;
        thread->fieldf8 = cur;
        obj->field48 = *mk3_arg(thread, cur);
        goto leave;

    } else {
        return -3;
    }

    *mk3_frame(thread, thread->frame + 1) = next;
    thread->frame = thread->frame + 1;              /* push a level */
    mk3_frame(thread, thread->frame)[1] = obj->a10;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;

leave:
    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
        return 0;
    }

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* ----------------------------------------------------------------- tl_do_lao_spin
 *
 * armv7 0x0004e1e0, 320 bytes.  **Complete.**
 *
 *      token == 0:      obj->field20 = 0x119
 *                       init_special_act(obj)
 *                       obj->field40 = 2; get_char_ani2(obj)
 *                       obj->field1c = 1; init_anirate(obj)
 *                       obj->field1c = 6
 *                       obj->field48 = obj->a10 = 1
 *                       obj->field00->field28 = 6
 *                       token := 0x5ff, park 1
 *
 *      token == 0x5ff:  next_lao_anirate(obj)
 *                       obj->field1c = obj->field00->field28 - 1
 *                       if (that != 0) {
 *                           obj->field00->field28 = obj->field1c
 *                           token := 0x5ff, park 1
 *                       } else { token := 0x609, park 1 }
 *
 *      token == 0x609:  next_lao_anirate(obj)
 *                       obj->field1c = (uint32_t)(G + 0x3cc)
 *                       get_tsl_px(obj, obj)
 *                       if (obj->field20 <= 0xe) {
 *                           obj->field1c = 0x13
 *                           strike_check_a0(obj)
 *                           if (obj->field5c == 0) token := 0x609, park 1
 *                       }
 *                       obj->field48 = 0x20
 *                       if (obj->field18 != 0) {
 *                           set_no_block(obj)
 *                           obj->field48 = 0x30
 *                       }
 *                       obj->field1c = 3; init_anirate(obj)
 *                       token := 0x620, park 1
 *
 *      token == 0x620:  next_lao_anirate(obj)
 *                       if (--obj->field48 == 0)
 *                           frame[frame].handler = t_local_reaction_exit
 *                       else token := 0x620, park 1
 *
 *      otherwise:       return -3
 *
 * **The spin lasts as long as the button is held, and a connection also ends it.**
 * State 0x609 runs once a frame: it asks get_tsl_px about the halfwords at G + 0x3cc
 * and, while the answer in 0x20 is at or below 0xe, checks for a strike and stays.
 * Two things break the loop -- the answer going above 0xe, or the strike connecting
 * -- and **both land on the same recovery code**, which is why the hit case is a
 * branch forward into the middle of the release case rather than a state of its own.
 *
 * **The recovery is longer if 0x18 is set**: 0x20 frames normally, 0x30 with
 * blocking taken away first. So whatever 0x18 means here, it costs the fighter
 * sixteen extra frames of recovery and its ability to block.
 *
 * Three counters, all in different places: the wind-up counts down in the proc's
 * 0x28 from 6, the recovery counts down in 0x48 from 0x20 or 0x30, and
 * next_lao_anirate keeps its own thirty-frame sound counter in 0x44 -- which state 0
 * seeds with 1 so the first sound fires immediately.
 *
 * G + 0x3cc is a fourth per-player halfword set in this file, after 0x3a8, 0x3ac
 * and the 0x420/0x424 pair.
 */
void next_lao_anirate(MK3OBJ *obj);
void set_no_block(MK3OBJ *obj);

long tl_do_lao_spin(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field20 = 0x119;
        init_special_act(obj);

        obj->field40 = 2;
        get_char_ani2(obj);

        obj->field1c = 1;
        init_anirate(obj);

        obj->field1c = 6;
        obj->field48 = 1;
        obj->a10     = 1;
        obj->field00->field28 = obj->field1c;

        *mk3_frame(thread, thread->frame + 1) = 0x5ff;
        thread->fieldfc = 1;
        return 1;
    }

    if (token == 0x5ff) {
        next_lao_anirate(obj);

        obj->field1c = obj->field00->field28 - 1;
        if (obj->field1c != 0) {
            obj->field00->field28 = obj->field1c;
            *mk3_frame(thread, thread->frame + 1) = 0x5ff;
            thread->fieldfc = 1;
            return 1;
        }

        *mk3_frame(thread, thread->frame + 1) = 0x609;
        thread->fieldfc = 1;
        return 1;
    }

    if (token == 0x609) {
        next_lao_anirate(obj);

        obj->field1c = (uint32_t)(uintptr_t)(G_BYTES + 0x3cc);
        get_tsl_px(obj, obj);

        if ((long)obj->field20 <= 0xe) {
            obj->field1c = 0x13;
            strike_check_a0(obj);
            if (obj->field5c == 0) {
                *mk3_frame(thread, thread->frame + 1) = 0x609;
                thread->fieldfc = 1;
                return 1;
            }
        }

        obj->field48 = 0x20;
        if (obj->field18 != 0) {
            set_no_block(obj);
            obj->field48 = 0x30;
        }

        obj->field1c = 3;
        init_anirate(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x620;
        thread->fieldfc = 1;
        return 1;
    }

    if (token != 0x620)
        return -3;

    next_lao_anirate(obj);

    obj->field48 = obj->field48 - 1;
    if (obj->field48 == 0)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

    *mk3_frame(thread, thread->frame + 1) = 0x620;
    thread->fieldfc = 1;
    return 1;
}


/* -------------------------------------------------------------------- t_air_strike
 *
 * armv7 0x0004cef8, 360 bytes.  **Complete.**
 *
 *      token == 0:      obj->field08->field20 = obj->field28
 *                       obj->field00->field28 = obj->field20
 *                       *(uint32_t *)((char *)obj->field00 + 0x2c) = obj->field2c
 *                       obj->field48 = obj->field1c
 *                       obj->field1c = obj->field24
 *                       init_anirate(obj); get_char_ani(obj)
 *                       token := 0x90f, park 1
 *
 *      token == 0x90f:  next_anirate(obj)
 *                       obj->field1c = *(uint32_t *)obj->field40
 *                       if (that != 0) {
 *                           -- the height test --
 *                           obj->field1c = obj->field08->field1c
 *                           if (that < 0) token := 0x90f, park 1
 *                           obj->field24 = (int16)part[0x12]
 *                           obj->field1c = obj->field00->field40
 *                           if (that > obj->field24) token := 0x90f, park 1
 *                           stop_me_player(obj); ground_player(obj)
 *                           -- into the 0x943 body --
 *                       }
 *                       obj->field1c = obj->field48
 *                       strike_check_a0(obj)
 *                       if (obj->field5c != 0) {
 *                           obj->field5c = 1
 *                           pop a level, or t_local_reaction_exit
 *                       }
 *                       obj->field1c = --obj->field00->field28
 *                       if (that > 0) -- back to the height test --
 *                       obj->field40 += 4
 *                       obj->field00->field18 = 0x50a
 *                       obj->field1c = 0
 *                       *(uint32_t *)((char *)obj->field00 + 0x34) = 0
 *                       token := 0x943, descend into t_flight_loop
 *
 *      token == 0x943:  obj->field5c = 0
 *                       pop a level, or t_local_reaction_exit
 *
 *      otherwise:       return -3
 *
 * **The word at the 0x40 cursor decides what this frame does: non-zero means test
 * the height, zero means test for a strike.** That is a fourth site for 0x40 as a
 * cursor and the clearest one -- it is dereferenced at the top of every frame and
 * advanced by 4 only when the routine is finished with the current entry.
 *
 * **Three ways out, and all three answer in 0x5c.** Landing (the height test
 * passing) reaches the 0x943 body, which clears 0x5c and pops. Connecting sets 0x5c
 * to 1 and pops immediately. Running the proc's 0x28 down to zero advances the
 * cursor, sets an action, and descends into t_flight_loop with 0x5c untouched. So a
 * caller reading 0x5c afterwards gets hit, no-hit, or whatever the flight left --
 * and t_jk6, which is the caller, tests exactly that.
 *
 * The height test is two comparisons: the part's 0x1c must not be negative, and the
 * proc's 0x40 must not be above the part's signed 0x12. Both failing keep the loop
 * going, so the fighter stays airborne until it is both falling and low enough.
 *
 * State 0 shuffles five fields before doing anything -- 0x28 into the part's 0x20,
 * 0x20 into the proc's 0x28, 0x2c into the proc's 0x2c, 0x1c into 0x48 and 0x24
 * into 0x1c -- so the caller passes its arguments in one set of fields and this
 * routine redistributes them to where the loop and the strike check expect them.
 * That is why t_jk6 sets 0x28 and 0x2c and nothing else.
 */
long t_flight_loop(MK3THREAD *thread);           /* pointer slot 0x000f317c */

long t_air_strike(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    MK3OBJ  *part;

    if (token == 0) {
        obj->field08->field20 = obj->field28;
        obj->field00->field28 = obj->field20;
        *(uint32_t *)((char *)obj->field00 + 0x2c) = obj->field2c;

        obj->field48 = obj->field1c;
        obj->field1c = obj->field24;

        init_anirate(obj);
        get_char_ani(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x90f;
        thread->fieldfc = 1;
        return 1;
    }

    if (token == 0x90f) {
        next_anirate(obj);

        obj->field1c = *(uint32_t *)(void *)(uintptr_t)obj->field40;

        for (;;) {
            if (obj->field1c != 0) {
                /* the height test */
                part = obj->field08;
                obj->field1c = part->field1c;
                if ((long)obj->field1c < 0) {
                    *mk3_frame(thread, thread->frame + 1) = 0x90f;
                    thread->fieldfc = 1;
                    return 1;
                }

                obj->field24 = (uint32_t)(int32_t)MK3_FIELD12_S(part);
                obj->field1c = obj->field00->field40;
                if ((long)obj->field1c > (long)obj->field24) {
                    *mk3_frame(thread, thread->frame + 1) = 0x90f;
                    thread->fieldfc = 1;
                    return 1;
                }

                stop_me_player(obj);
                ground_player(obj);
                break;                      /* into the 0x943 body */
            }

            obj->field1c = obj->field48;
            strike_check_a0(obj);

            if (obj->field5c != 0) {
                obj->field5c = 1;
                if ((long)thread->frame > 0) {
                    thread->frame = thread->frame - 1;
                    return 0;
                }
                return mk3_install(thread,
                                   (MK3THREADFUNC)t_local_reaction_exit);
            }

            obj->field00->field28 = obj->field00->field28 - 1;
            obj->field1c = obj->field00->field28;
            if ((long)obj->field1c > 0)
                continue;                   /* back to the height test */

            obj->field40 = obj->field40 + 4;
            obj->field00->field18 = 0x50a;
            obj->field1c = 0;
            *(uint32_t *)((char *)obj->field00 + 0x34) = 0;

            *mk3_frame(thread, thread->frame + 1) = 0x943;
            thread->frame = thread->frame + 1;      /* push a level */
            mk3_frame(thread, thread->frame)[1] =
                (uint32_t)(uintptr_t)t_flight_loop;
            *mk3_frame(thread, thread->frame + 1) = 0;
            return 0;
        }
    } else if (token != 0x943) {
        return -3;
    }

    obj->field5c = 0;

    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
        return 0;
    }

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* ---------------------------------------------------------------- tl_do_ermac_slam
 *
 * armv7 0x0004f384, 324 bytes.  **Complete.**
 *
 *      token == 0:      obj->field20 = 0x11d
 *                       init_special_act(obj)
 *                       obj->field40 = 0x0002000c
 *                       token := 0x62, descend into t_animate2_a9
 *
 *      token == 0x62:   obj->field1c = 0xf; ochar_sound(obj)
 *                       obj->field48 = 0x00030008; shake_a11(obj)
 *                       obj->a10 = 0x10
 *                       token := 0x6b, park 1
 *
 *      token == 0x6b:   q_is_he_a_boss(obj)
 *                       if (obj->field5c == 0) {
 *                           obj->field1c = 0x1a
 *                           strike_check_a0(obj)
 *                           if (obj->field5c != 0) {
 *                               obj->field1c = (uint32_t)(G + 0x434)
 *                               update_tsl(obj)
 *                               if (obj->field18 == 0) {
 *                                   token := 0x86, park 8
 *                               }
 *                               -- otherwise the 0x62a path --
 *                           }
 *                       }
 *                       if (--obj->a10 != 0) token := 0x6b, park 1
 *                       obj->field1c = obj->field00->field18 = 0x62a
 *                       token := 0x7e, park 0x30
 *
 *      token == 0x7e:
 *      token == 0x86:   obj->field1c = 2
 *                       obj->field40 = 2 + 0xa = 0xc
 *                       frame[frame].handler = t_backwards_ani
 *
 *      otherwise:       return -3
 *
 * **A boss is not struck, it is only waited out.** q_is_he_a_boss answers in 0x5c,
 * and a set answer skips the strike check entirely and goes straight to the sixteen
 * frame countdown in 0x44. Only a non-boss gets strike_check_a0 -- so this move
 * cannot connect against a boss, which is a rule enforced here rather than in the
 * strike check.
 *
 * **Two tokens share one arm.** 0x7e and 0x86 both branch to the same three lines,
 * which is why the routine has five reachable states but only four bodies. The
 * arrival is all that differs: 0x7e after a thirty-frame hold, 0x86 after eight.
 *
 * **0x48 for shake_a11 is a pair of halfwords, and this site proves they are
 * independent.** 0x00030008 -- three and eight, not equal -- against the doubled
 * 0x00080008, 0x00050005 and 0x00090009 found elsewhere. Eight sites in the module
 * now write that field before a shake, and only this one is asymmetric, which is
 * what settles it as two numbers rather than one repeated.
 *
 * G + 0x434 is a fifth per-player set in this file, built as 0x430 plus 4 exactly as
 * update_block_fk builds G + 0x424 from 0x420.
 */
void q_is_he_a_boss(MK3OBJ *obj);
void shake_a11(MK3OBJ *obj);
long t_backwards_ani(MK3THREAD *thread);         /* pointer slot 0x000f37c4 */

long tl_do_ermac_slam(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field20 = 0x11d;
        init_special_act(obj);
        obj->field40 = 0x0002000c;

        *mk3_frame(thread, thread->frame + 1) = 0x62;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_animate2_a9;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x62) {
        obj->field1c = 0xf;
        ochar_sound(obj);

        obj->field48 = 0x00030008;
        shake_a11(obj);

        obj->a10 = 0x10;

        *mk3_frame(thread, thread->frame + 1) = 0x6b;
        thread->fieldfc = 1;
        return 1;
    }

    if (token == 0x6b) {
        q_is_he_a_boss(obj);

        if (obj->field5c == 0) {
            obj->field1c = 0x1a;
            strike_check_a0(obj);

            if (obj->field5c != 0) {
                obj->field1c = (uint32_t)(uintptr_t)(G_BYTES + 0x430 + 4);
                update_tsl(obj);

                if (obj->field18 == 0) {
                    *mk3_frame(thread, thread->frame + 1) = 0x86;
                    thread->fieldfc = 8;
                    return 8;
                }
                goto hold;
            }
        }

        obj->a10 = obj->a10 - 1;
        if (obj->a10 != 0) {
            *mk3_frame(thread, thread->frame + 1) = 0x6b;
            thread->fieldfc = 1;
            return 1;
        }

hold:
        obj->field1c = 0x62a;
        obj->field00->field18 = 0x62a;

        *mk3_frame(thread, thread->frame + 1) = 0x7e;
        thread->fieldfc = 0x30;
        return 0x30;
    }

    if (token != 0x7e && token != 0x86)
        return -3;

    obj->field1c = 2;
    obj->field40 = 2 + 0xa;

    return mk3_install(thread, (MK3THREADFUNC)t_backwards_ani);
}


/* ------------------------------------------------------------------ tl_do_swat_gun
 *
 * armv7 0x00050064, 328 bytes.  **Complete.**
 *
 *      token == 0:      obj->field20 = 0x11c
 *                       init_special_act(obj)
 *                       obj->field40 = 0x00020005
 *                       token := 0x94, descend into t_animate2_a9
 *
 *      token == 0x94:   obj->field1c = 2; init_anirate(obj)
 *                       obj->field48 = 0x20
 *                       token := 0x9c, park 1
 *
 *      token == 0x9c:   next_anirate(obj)
 *                       obj->field1c = 0x18
 *                       strike_check_a0(obj)
 *                       if (obj->field5c != 0) {
 *                           obj->field1c = obj->field00->field18 = 0x629
 *                           obj->field48 = 0x20
 *                           is_he_airborn(obj)
 *                           if (obj->field5c != 0) obj->field48 = 8
 *                           token := 0xc2, park 1
 *                       }
 *                       if (--obj->field48 > 0) token := 0x9c, park 1
 *                       -- into the cleanup --
 *
 *      token == 0xc2:   next_anirate(obj)
 *                       if (--obj->field48 > 0) token := 0xc2, park 1
 *                       -- into the cleanup --
 *
 *      the cleanup:     delete_slave(obj)
 *                       obj->field1c = (uint32_t)(G + 0x430)
 *                       update_tsl(obj)
 *                       obj->field40 = 5
 *                       obj->field54 = 5 - 2 = 3
 *                       find_ani2_part_a14(obj)
 *                       obj->field1c = 2
 *                       frame[frame].handler = t_mframew
 *
 *      otherwise:       return -3
 *
 * **Hitting an airborne target costs a quarter of the recovery.** On a connection
 * the routine loads 0x48 with 0x20, asks is_he_airborn, and overwrites it with 8 if
 * the answer is set. So catching someone in the air is rewarded with twenty-four
 * fewer frames of recovery -- a rule stated once, in one `if`, and nowhere else in
 * the module so far.
 *
 * **Two counters, one field, two states.** 0x48 is the firing window in 0x9c
 * (0x20 frames to connect) and the recovery in 0xc2 (0x20 or 8), and both states
 * count the same field down and fall into the same cleanup when it runs out. That
 * is why the two exhaustion paths converge rather than each having their own tail.
 *
 * G + 0x430 here against G + 0x434 in tl_do_ermac_slam -- two per-player sets four
 * bytes apart, the same pairing as 0x420/0x424 and 0x3a8/0x3ac. Three such pairs are
 * now known in this file.
 */
long is_he_airborn(MK3OBJ *obj);
void find_ani2_part_a14(MK3OBJ *obj);

long tl_do_swat_gun(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field20 = 0x11c;
        init_special_act(obj);
        obj->field40 = 0x00020005;

        *mk3_frame(thread, thread->frame + 1) = 0x94;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_animate2_a9;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x94) {
        obj->field1c = 2;
        init_anirate(obj);
        obj->field48 = 0x20;

        *mk3_frame(thread, thread->frame + 1) = 0x9c;
        thread->fieldfc = 1;
        return 1;
    }

    if (token == 0x9c) {
        next_anirate(obj);

        obj->field1c = 0x18;
        strike_check_a0(obj);

        if (obj->field5c != 0) {
            obj->field1c = 0x629;
            obj->field00->field18 = 0x629;

            obj->field48 = 0x20;
            is_he_airborn(obj);
            if (obj->field5c != 0)
                obj->field48 = 8;

            *mk3_frame(thread, thread->frame + 1) = 0xc2;
            thread->fieldfc = 1;
            return 1;
        }

        obj->field48 = obj->field48 - 1;
        if ((long)obj->field48 > 0) {
            *mk3_frame(thread, thread->frame + 1) = 0x9c;
            thread->fieldfc = 1;
            return 1;
        }

    } else if (token == 0xc2) {
        next_anirate(obj);

        obj->field48 = obj->field48 - 1;
        if ((long)obj->field48 > 0) {
            *mk3_frame(thread, thread->frame + 1) = 0xc2;
            thread->fieldfc = 1;
            return 1;
        }

    } else {
        return -3;
    }

    /* the cleanup, reached when either counter runs out */
    delete_slave(obj);

    obj->field1c = (uint32_t)(uintptr_t)(G_BYTES + 0x430);
    update_tsl(obj);

    obj->field40 = 5;
    obj->field54 = 5 - 2;
    find_ani2_part_a14(obj);

    obj->field1c = 2;
    return mk3_install(thread, (MK3THREADFUNC)t_mframew);
}


/* ----------------------------------------------------------------- t_do_flip_punch
 *
 * armv7 0x0004d3ec, 352 bytes.  **Complete.**
 *
 *      token == 0:      face_opponent(obj)
 *                       rsnd_func(obj, 0xe)
 *                       obj->field1c = 0; group_sound(obj)
 *                       *(uint32_t *)((char *)obj->field00 + 0x58) = 0xc
 *                       obj->field00->field18 = 0x201
 *                       obj->field20 = 0x10
 *                       obj->field24 = 0x10 - 0xd = 3
 *                       obj->field1c = 0xc
 *                       obj->field40 = 3 + 0x15 = 0x18
 *                       obj->field28 = 0x8000
 *                       obj->field2c = 4
 *                       token := 0x865, descend into t_air_strike
 *
 *      token == 0x865:  if (obj->field5c != 0) {
 *                           update_l_block_fk(obj)
 *                           obj->field1c = 8
 *                           token := 0x86f, descend into t_combo_air_pause
 *                       }
 *                       -- otherwise into the 0x879 body --
 *
 *      token == 0x86f:  obj->field00->field18 = 0x60c
 *                       *(uint32_t *)((char *)obj->field00 + 0x34) = 0
 *                       obj->field1c = 3
 *                       obj->field28 = 0xc000
 *                       obj->field20 = 0
 *                       obj->field08->field20 = 0xc000
 *                       token := 0x879, descend into t_flight_loop
 *
 *      token == 0x879:  obj->field1c = obj->field00->field18 = 0x60c
 *                       frame[frame].handler = t_angle_jump_land_jsrp
 *
 *      otherwise:       return -3
 *
 * **t_jk6's four states with two things changed**, and comparing the pair is what
 * makes both readable. Same shape: set up, strike in the air, and on a hit go
 * through t_combo_air_pause and then t_flight_loop before landing; on a miss go
 * straight to the landing. The differences are that this one calls
 * `update_l_block_fk` on the hit -- pushing a block update across to the other
 * fighter through the A0 mechanism -- and that it lands through
 * t_angle_jump_land_jsrp where t_jk6 uses t_jump_up_land_jsrp.
 *
 * So a flip punch and a jumping attack differ in their landing and in one extra
 * call, not in their structure.
 *
 * **0x28 is written twice with the same two magnitudes t_jk6 uses**, 0x8000 before
 * the strike and 0xc000 after -- against t_jk6's 0x8000 and 0xe000. The pair is the
 * same idea with a different second value, which is the clearest sign yet that
 * 0x8000 is the entry value and the second is per-move.
 *
 * This routine's whole setup differs from t_do_jumpup_punch's only in 0x28 and 0x2c
 * -- and those two are exactly the fields t_air_strike redistributes -- so all three
 * jumping attacks pass their arguments the same way.
 */
void update_l_block_fk(MK3OBJ *obj);
long t_angle_jump_land_jsrp(MK3THREAD *thread);  /* pointer slot 0x000f38a8 */

long t_do_flip_punch(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        face_opponent(obj);
        rsnd_func(obj, 0xe);

        obj->field1c = 0;
        group_sound(obj);

        *(uint32_t *)((char *)obj->field00 + 0x58) = 0xc;
        obj->field00->field18 = 0x201;

        obj->field20 = 0x10;
        obj->field24 = 0x10 - 0xd;
        obj->field1c = 0xc;
        obj->field40 = (0x10 - 0xd) + 0x15;
        obj->field28 = 0x8000;
        obj->field2c = 4;

        *mk3_frame(thread, thread->frame + 1) = 0x865;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_air_strike;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x865 && obj->field5c != 0) {
        update_l_block_fk(obj);
        obj->field1c = 8;

        *mk3_frame(thread, thread->frame + 1) = 0x86f;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_combo_air_pause;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x86f) {
        obj->field00->field18 = 0x60c;
        *(uint32_t *)((char *)obj->field00 + 0x34) = 0;

        obj->field1c = 3;
        obj->field28 = 0xc000;
        obj->field20 = 0;
        obj->field08->field20 = 0xc000;

        *mk3_frame(thread, thread->frame + 1) = 0x879;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_flight_loop;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x865 && token != 0x879)
        return -3;

    obj->field1c = 0x60c;
    obj->field00->field18 = 0x60c;

    return mk3_install(thread, (MK3THREADFUNC)t_angle_jump_land_jsrp);
}


/* ------------------------------------------------------------------ t_do_flip_kick
 *
 * armv7 0x0004d634, 364 bytes.  **Complete.**
 *
 *      token == 0:      face_opponent(obj)
 *                       rsnd_func(obj, 0xe)
 *                       obj->field1c = 0; group_sound(obj)
 *                       *(uint32_t *)((char *)obj->field00 + 0x58) = 0xb
 *                       obj->field00->field18 = 0x200
 *                       obj->field20 = 0x200 - 0x1f0 = 0x10
 *                       obj->field24 = 0x10 - 0xd = 3
 *                       obj->field1c = 0xb
 *                       obj->field40 = 3 + 0x16 = 0x19
 *                       obj->field28 = 0x8000
 *                       obj->field2c = 4
 *                       token := 0x834, descend into t_air_strike
 *
 *      token == 0x834:  if (obj->field5c != 0) {
 *                           update_l_block_fk(obj); me_in_front(obj)
 *                           obj->field00->field18 = 0x60c
 *                           obj->field1c = 8
 *                           token := 0x841, descend into t_mid_air_pause
 *                       }
 *                       -- otherwise into the 0x848 body --
 *
 *      token == 0x841:  obj->field1c = 3
 *                       obj->field20 = 0
 *                       *(uint32_t *)((char *)obj->field00 + 0x34) = 0
 *                       obj->field28 = 0xc000
 *                       obj->field08->field20 = 0xc000
 *                       token := 0x848, descend into t_flight_loop
 *
 *      token == 0x848:  frame[frame].handler = t_angle_jump_land_jsrp
 *
 *      otherwise:       return -3
 *
 * **The third of the three air attacks, and the set is now complete enough to say
 * what varies.** All three -- t_jk6, t_do_flip_punch, t_do_flip_kick -- have the
 * same four states, set 0x28 to 0x8000 before the strike and to a per-move value
 * after, and pass their arguments in the two fields t_air_strike redistributes.
 *
 * What differs, move by move:
 *
 *      pause on a hit   t_combo_air_pause  t_combo_air_pause  t_mid_air_pause
 *      landing          t_jump_up_land     t_angle_jump_land  t_angle_jump_land
 *      0x28 after       0xe000             0xc000             0xc000
 *      extra on a hit   --                 update_l_block_fk  + me_in_front
 *
 * So the flip kick is the only one that takes the cheaper pause -- t_mid_air_pause
 * is one state and a park, where t_combo_air_pause is 700 bytes -- and the only one
 * that turns the fighter to face front on connecting.
 *
 * Its 0x60c goes into the proc on the HIT path, where the flip punch writes it in
 * the state after. Same value, one state apart, which is the sort of difference that
 * disappears if the two are transcribed from each other rather than from the
 * instructions.
 */
void me_in_front(MK3OBJ *obj);
long t_mid_air_pause(MK3THREAD *thread);

long t_do_flip_kick(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        face_opponent(obj);
        rsnd_func(obj, 0xe);

        obj->field1c = 0;
        group_sound(obj);

        *(uint32_t *)((char *)obj->field00 + 0x58) = 0xb;
        obj->field00->field18 = 0x200;

        obj->field20 = 0x200 - 0x1f0;
        obj->field24 = (0x200 - 0x1f0) - 0xd;
        obj->field1c = 0xb;
        obj->field40 = ((0x200 - 0x1f0) - 0xd) + 0x16;
        obj->field28 = 0x8000;
        obj->field2c = 4;

        *mk3_frame(thread, thread->frame + 1) = 0x834;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_air_strike;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x834 && obj->field5c != 0) {
        update_l_block_fk(obj);
        me_in_front(obj);

        obj->field00->field18 = 0x60c;
        obj->field1c = 8;

        *mk3_frame(thread, thread->frame + 1) = 0x841;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mid_air_pause;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x841) {
        obj->field1c = 3;
        obj->field20 = 0;
        *(uint32_t *)((char *)obj->field00 + 0x34) = 0;
        obj->field28 = 0xc000;
        obj->field08->field20 = 0xc000;

        *mk3_frame(thread, thread->frame + 1) = 0x848;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_flight_loop;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x834 && token != 0x848)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_angle_jump_land_jsrp);
}


/* --------------------------------------------------------------- t_stat_do_uppercut
 *
 * armv7 0x0004fab4, 512 bytes.  **Complete.**
 *
 *      token == 0:      init_special(obj)
 *                       rsnd_func(obj, 0xf)
 *                       fflush(*uppercut_stream)
 *                       obj->field40 = 0xb; do_first_a9_frame(obj)
 *                       token := 0x2c5, park 1
 *
 *      token == 0x2c5:  fflush(*uppercut_stream)
 *                       do_next_a9_frame(obj)
 *                       token := 0x2c8, park 1
 *
 *      token == 0x2c8:  fflush(*uppercut_stream)
 *                       obj->field1c = 2
 *                       obj->field20 = 2 + 0x10c = 0x10e
 *                       obj->a10 = 1
 *                       obj->field48 = 1 + 7 = 8
 *                       *(uint32_t *)((char *)obj->field00 + 0x58) = 8
 *                       token := 0x2d0, descend into t_upcut_striker
 *
 *      token == 0x2d0:  fflush(*uppercut_stream)
 *                       obj->field00->field18 = 0x603
 *                       obj->field1c = 0xa
 *                       token := 0x2e5, park 0xa
 *
 *      token == 0x2e5:  obj->field1c = (int16)*(uint16_t *)(G + 0x45c)
 *                       if (that != 0) -- into the finish --
 *                       token := 0x2ea, park 1
 *
 *      token == 0x2ea:  get_his_action(obj)
 *                       if (obj->field20 != 0x610 && obj->field20 != 0x203) {
 *                           get_his_dog(obj)
 *                           if (obj->field1c > 0x50) token := 0x2ea, park 1
 *                       }
 *                       obj->field1c = 4
 *                       obj->field20 = 0x603
 *                       token := 0x2fb, descend into t_act_mframew
 *
 *      token == 0x2fb:  back_to_normal(obj)
 *                       pop a level, or t_local_reaction_exit at the bottom
 *
 *      otherwise:       return -3
 *
 * **The uppercut waits for the victim to come down, and there are three ways it
 * stops waiting.** State 0x2ea polls once a frame: the opponent's action being
 * 0x610 or 0x203 ends the wait, and so does `get_his_dog` answering 0x50 or less in
 * 0x1c. Anything else re-arms for another frame. So the routine holds the fighter in
 * its recovery pose until the other one has landed or come close enough.
 *
 * **State 0x2e5 can skip the wait entirely.** It reads the fatality-requirement
 * halfword at G + 0x45c -- the same one t_dizzy_wake and t_dizzy_dude gate on -- and
 * a non-zero value jumps straight to the finish without ever polling. So during a
 * finisher the uppercut does not wait for anything.
 *
 * **Four calls to `fflush`, one at the top of each of the first four states.** The
 * stub at 0x000dd794 resolves through `__la_symbol_ptr` 0x000f3b64 to `_fflush`, read
 * from the indirect symbol table rather than guessed -- and the stub spacing and the
 * alphabetical run of its neighbours (`_dlsym`, `_fflush`, `_floorf`, `_free`)
 * confirm the mapping. The argument is loaded from a pointer slot at 0x000f3180 and
 * dereferenced, and that slot is **zero in the file image**, so whatever stream it
 * names is set up at runtime.
 *
 * Four flushes at state boundaries inside a gameplay routine is where a debug trace
 * would sit, and this binary keeps plenty of that -- `__assert_rtn` calls, a whole
 * `DS_DebugWin.c`. But nothing here writes anything, so what is being flushed is not
 * visible from this routine. Recorded as found rather than named.
 */
extern void **uppercut_stream;                   /* pointer slot 0x000f3180 */
void do_first_a9_frame(MK3OBJ *obj);
void get_his_dog(MK3OBJ *obj);
long t_upcut_striker(MK3THREAD *thread);         /* pointer slot 0x000f3764 */
long t_act_mframew(MK3THREAD *thread);           /* pointer slot 0x000f37e8 */

long t_stat_do_uppercut(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        init_special(obj);
        rsnd_func(obj, 0xf);
        fflush((FILE *)*uppercut_stream);

        obj->field40 = 0xb;
        do_first_a9_frame(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x2c5;
        thread->fieldfc = 1;
        return 1;
    }

    if (token == 0x2c5) {
        fflush((FILE *)*uppercut_stream);
        do_next_a9_frame(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x2c8;
        thread->fieldfc = 1;
        return 1;
    }

    if (token == 0x2c8) {
        fflush((FILE *)*uppercut_stream);

        obj->field1c = 2;
        obj->field20 = 2 + 0x10c;
        obj->a10     = 1;
        obj->field48 = 1 + 7;
        *(uint32_t *)((char *)obj->field00 + 0x58) = obj->field48;

        *mk3_frame(thread, thread->frame + 1) = 0x2d0;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_upcut_striker;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x2d0) {
        fflush((FILE *)*uppercut_stream);

        obj->field00->field18 = 0x603;
        obj->field1c = 0xa;

        *mk3_frame(thread, thread->frame + 1) = 0x2e5;
        thread->fieldfc = obj->field1c;
        return (long)obj->field1c;
    }

    if (token == 0x2e5) {
        obj->field1c = (uint32_t)(int32_t)
            (int16_t)*(uint16_t *)(G_BYTES + 0x45c);

        if (*(uint16_t *)(G_BYTES + 0x45c) == 0) {
            *mk3_frame(thread, thread->frame + 1) = 0x2ea;
            thread->fieldfc = 1;
            return 1;
        }

    } else if (token == 0x2ea) {
        get_his_action(obj);

        if (obj->field20 != 0x610 && obj->field20 != 0x203) {
            get_his_dog(obj);
            if ((long)obj->field1c > 0x50) {
                *mk3_frame(thread, thread->frame + 1) = 0x2ea;
                thread->fieldfc = 1;
                return 1;
            }
        }

    } else if (token == 0x2fb) {
        back_to_normal(obj);

        if ((long)thread->frame > 0) {
            thread->frame = thread->frame - 1;
            return 0;
        }
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

    } else {
        return -3;
    }

    /* the finish, reached from 0x2e5 and 0x2ea */
    obj->field1c = 4;
    obj->field20 = 0x603;

    *mk3_frame(thread, thread->frame + 1) = 0x2fb;
    thread->frame = thread->frame + 1;              /* push a level */
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_act_mframew;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* -------------------------------------------------------------- tl_stat_do_fan_lift
 *
 * armv7 0x0004dce0, 440 bytes.  **Complete.**
 *
 *      token == 0:      obj->field20 = 0x11b
 *                       init_special_act(obj)
 *                       obj->field40 = 1; get_char_ani2(obj)
 *                       obj->field1c = 4
 *                       token := 0x124, descend into t_mframew
 *
 *      token == 0x124:  obj->field1c = 1; init_anirate(obj)
 *                       set_no_block(obj)
 *                       obj->field1c = 2; ochar_sound(obj)
 *                       obj->field48 = 1
 *                       obj->a10 = 0xc0
 *                       token := 0x132, park 1
 *
 *      token == 0x132:  obj->field1c = 0x12
 *                       strike_check_a0(obj)
 *                       if (obj->field5c == 0) {
 *                           if (--obj->field48 <= 0) obj->field48 = 0xc
 *                           next_anirate(obj)
 *                           if (--obj->a10 > 0) token := 0x132, park 1
 *                       }
 *                       obj->a10 = 0x10
 *                       if (obj->field18 != 0) obj->a10 = 0x50
 *                       token := 0x149, park 1
 *
 *      token == 0x149:  if (--obj->field48 <= 0) obj->field48 = 0xc
 *                       next_anirate(obj)
 *                       if (--obj->a10 > 0) token := 0x149, park 1
 *                       obj->field40 = 1; get_char_ani2(obj)
 *                       find_part2(obj); find_part2(obj)
 *                       obj->field1c = 4
 *                       token := 0x15c, descend into t_mframew
 *
 *      token == 0x15c:  back_to_normal(obj)
 *                       pop a level, or t_local_reaction_exit at the bottom
 *
 *      otherwise:       return -3
 *
 * **Two counters, and the small one is a repeating sub-count.** 0x44 is the main
 * budget -- 0xc0 frames for the lift, then 0x10 (or 0x50) for the recovery -- and
 * 0x48 counts 0xc down over and over, reloading itself every time it reaches zero.
 * Both the 0x132 and 0x149 states carry the same reload, so whatever 0x48 paces is
 * paced through both halves of the move at the same rate.
 *
 * **The strike jumps the fighter straight into recovery**, past both counters, to the
 * same 0x10-or-0x50 reload the lift's own expiry reaches. So connecting does not
 * shorten the recovery, it only ends the lift early.
 *
 * **The recovery is longer when 0x18 is set** -- 0x50 against 0x10 -- which is the
 * same shape tl_do_lao_spin uses (0x30 against 0x20) with a much wider gap. Two
 * routines, one rule, different penalties.
 *
 * **`find_part2` is called twice in a row**, at 0x4de22 and 0x4de28, with nothing
 * between them. Transcribed as written; the second call cannot see any input the
 * first did not.
 */
void find_part2(MK3OBJ *obj);

long tl_stat_do_fan_lift(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field20 = 0x11b;
        init_special_act(obj);

        obj->field40 = 1;
        get_char_ani2(obj);

        obj->field1c = 4;

        *mk3_frame(thread, thread->frame + 1) = 0x124;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x124) {
        obj->field1c = 1;
        init_anirate(obj);
        set_no_block(obj);

        obj->field1c = 2;
        ochar_sound(obj);

        obj->field48 = 1;
        obj->a10     = 0xc0;

        *mk3_frame(thread, thread->frame + 1) = 0x132;
        thread->fieldfc = 1;
        return 1;
    }

    if (token == 0x132) {
        obj->field1c = 0x12;
        strike_check_a0(obj);

        if (obj->field5c == 0) {
            obj->field48 = obj->field48 - 1;
            if ((long)obj->field48 <= 0)
                obj->field48 = 0xc;

            next_anirate(obj);

            obj->a10 = obj->a10 - 1;
            if ((long)obj->a10 > 0) {
                *mk3_frame(thread, thread->frame + 1) = 0x132;
                thread->fieldfc = 1;
                return 1;
            }
        }

        obj->a10 = 0x10;
        if (obj->field18 != 0)
            obj->a10 = 0x50;

        *mk3_frame(thread, thread->frame + 1) = 0x149;
        thread->fieldfc = 1;
        return 1;
    }

    if (token == 0x149) {
        obj->field48 = obj->field48 - 1;
        if ((long)obj->field48 <= 0)
            obj->field48 = 0xc;

        next_anirate(obj);

        obj->a10 = obj->a10 - 1;
        if ((long)obj->a10 > 0) {
            *mk3_frame(thread, thread->frame + 1) = 0x149;
            thread->fieldfc = 1;
            return 1;
        }

        obj->field40 = 1;
        get_char_ani2(obj);
        find_part2(obj);
        find_part2(obj);            /* twice, with nothing between */

        obj->field1c = 4;

        *mk3_frame(thread, thread->frame + 1) = 0x15c;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x15c)
        return -3;

    back_to_normal(obj);

    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
        return 0;
    }

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* ---------------------------------------------------------------- tl_stat_do_quake
 *
 * armv7 0x0004f1a8, 476 bytes.  **Complete.**
 *
 *      token == 0:      obj->field20 = 0x113
 *                       init_special_act(obj)
 *                       obj->field40 = 0; get_char_ani2(obj)
 *                       obj->field1c = 4
 *                       token := 0x41f, descend into t_mframew
 *
 *      token == 0x41f:  token := 0x420, park 6
 *
 *      token == 0x420:  tsound_func(obj, 2)
 *                       obj->field1c = 0; group_sound(obj)
 *                       obj->field1c = 3
 *                       token := 0x427, descend into t_mframew
 *
 *      token == 0x427:  push obj->a10
 *                       push obj->field48
 *                       is_he_airborn(obj)
 *                       if (obj->field5c == 0) {
 *                           q_is_he_a_boss(obj)
 *                           if (obj->field5c == 0) {
 *                               obj->field1c = 0x15
 *                               damage_to_him(obj)
 *                               obj->field38 = t_r_quake
 *                               takeover_him(obj)
 *                           }
 *                       }
 *                       obj->field48 = 0x0009000e; shake_a11(obj)
 *                       obj->field1c = 4; ochar_sound(obj)
 *                       obj->field48 = pop
 *                       obj->a10     = pop
 *                       obj->field20 = obj->field00->field18 = 0x604
 *                       obj->field1c = 3
 *                       token := 0x443, descend into t_mframew
 *
 *      token == 0x443:  delete_slave(obj)
 *                       frame[frame].handler = t_do_backup
 *
 *      otherwise:       return -3
 *
 * **Two kinds of opponent are exempt from the quake, and the order matters.**
 * is_he_airborn is asked first and a set answer skips everything; only a grounded
 * opponent is then checked against q_is_he_a_boss, and only a grounded non-boss takes
 * damage and is handed t_r_quake through takeover_him. So the shake and the sound
 * happen either way -- the quake always looks the same -- and what varies is whether
 * anyone is hurt by it.
 *
 * That is the second routine to exempt a boss, after tl_do_ermac_slam, and both put
 * the rule in the move rather than in the strike check.
 *
 * **0x44 and 0x48 are both saved across the whole state** and restored two lines
 * before the end, because 0x48 is borrowed in between as the shake magnitude. Two
 * pushes, two pops, LIFO -- so 0x48 comes back first and 0x44 second, which is the
 * reverse of the order they went on.
 *
 * The magnitude is 0x0009000e -- nine and fourteen, asymmetric like
 * tl_do_ermac_slam's 0x00030008 and unlike the doubled ones. Nine sites now write
 * that field before a shake and two of them are asymmetric, which is the settled
 * reading: two independent halfwords.
 */
void takeover_him(MK3OBJ *obj);
void damage_to_him(MK3OBJ *obj);
long t_r_quake(MK3THREAD *thread);               /* pointer slot 0x000f377c */
long t_do_backup(MK3THREAD *thread);             /* pointer slot 0x000f37e0 */

long tl_stat_do_quake(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    uint32_t cur, next;

    if (token == 0) {
        obj->field20 = 0x113;
        init_special_act(obj);

        obj->field40 = 0;
        get_char_ani2(obj);

        obj->field1c = 4;
        next = 0x41f;

    } else if (token == 0x41f) {
        *mk3_frame(thread, thread->frame + 1) = 0x420;
        thread->fieldfc = 6;
        return 6;

    } else if (token == 0x420) {
        tsound_func(obj, 2);

        obj->field1c = 0;
        group_sound(obj);

        obj->field1c = 3;
        next = 0x427;

    } else if (token == 0x427) {
        cur = thread->fieldf8;
        *mk3_arg(thread, cur) = obj->a10;
        thread->fieldf8 = cur + 1;

        cur = thread->fieldf8;
        *mk3_arg(thread, cur) = obj->field48;
        thread->fieldf8 = cur + 1;

        is_he_airborn(obj);
        if (obj->field5c == 0) {
            q_is_he_a_boss(obj);
            if (obj->field5c == 0) {
                obj->field1c = 0x15;
                damage_to_him(obj);

                obj->field38 = (uint32_t)(uintptr_t)t_r_quake;
                takeover_him(obj);
            }
        }

        obj->field48 = 0x0009000e;
        shake_a11(obj);

        obj->field1c = 4;
        ochar_sound(obj);

        cur = thread->fieldf8 - 1;
        thread->fieldf8 = cur;
        obj->field48 = *mk3_arg(thread, cur);

        cur = thread->fieldf8 - 1;
        thread->fieldf8 = cur;
        obj->a10 = *mk3_arg(thread, cur);

        obj->field20 = 0x604;
        obj->field00->field18 = 0x604;
        obj->field1c = 3;
        next = 0x443;

    } else if (token == 0x443) {
        delete_slave(obj);
        return mk3_install(thread, (MK3THREADFUNC)t_do_backup);

    } else {
        return -3;
    }

    *mk3_frame(thread, thread->frame + 1) = next;
    thread->frame = thread->frame + 1;              /* push a level */
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------------ tl_stat_do_lia_scream
 *
 * armv7 0x0004dfc4, 540 bytes.  **Complete.**
 *
 *      token == 0:      obj->field20 = 0x10f
 *                       init_special_act(obj)
 *                       obj->field40 = 0; get_char_ani2(obj)
 *                       obj->field1c = 3
 *                       token := 0x64f, descend into t_mframew
 *
 *      token == 0x64f:  get_his_action(obj)
 *                       if (obj->field20 == 0x615)
 *                           frame[frame].handler = t_r_scream
 *                       *(uint32_t *)((char *)proc + 0x2c) = 0
 *                       obj->field1c = 3; init_anirate(obj)
 *                       *(uint32_t *)((char *)proc + 0x30) = 1
 *                       obj->a10 = (uint32_t)obj->field00
 *                       obj->field1c = 6; create_fx(obj)
 *                       obj->field1c = 0x50
 *                       obj->field48 = 1
 *                       proc->field28 = 0x50
 *                       token := 0x665, park 1
 *
 *      token == 0x665:  next_anirate(obj)
 *                       if (--proc[0x30] == 0) {
 *                           obj->field1c = 1; ochar_sound(obj)
 *                           obj->field1c = 0x58
 *                       }
 *                       proc[0x30] = obj->field1c
 *                       if (proc[0x2c] == 0) {
 *                           obj->field1c = 0x11
 *                           strike_check_a0(obj)
 *                           if (obj->field5c != 0) {
 *                               if (obj->field18 != 0) {
 *                                   set_no_block(obj)
 *                                   obj->a10 = 0x28
 *                                   token := 0x6a2, park 1
 *                               }
 *                               proc[0x2c] = 1
 *                               obj->field1c = 0x20
 *                               proc->field28 = 0x20
 *                               dec_his_p_hit(obj)
 *                           }
 *                       }
 *                       if (--obj->field48 == 0) {
 *                           obj->field48 = 0x10
 *                           NewThread(obj, t_scream_wave)
 *                       }
 *                       if (--proc->field28 != 0) token := 0x665, park 1
 *                       -- into the finish --
 *
 *      token == 0x6a2:  if (--obj->field48 == 0) {
 *                           obj->field48 = 0x10
 *                           NewThread(obj, t_scream_wave)
 *                       }
 *                       next_anirate(obj)
 *                       if (--obj->a10 == 0) -- into the finish --
 *                       token := 0x6a2, park 1
 *
 *      the finish:      obj->field40 = 0; get_char_ani2(obj)
 *                       find_part2(obj); find_part2(obj)
 *                       delete_slave(obj)
 *                       obj->field1c = 3
 *                       frame[frame].handler = t_mframew
 *
 *      otherwise:       return -3
 *
 * **Four counters run at once, in four different places.** proc+0x30 paces the
 * sound and reloads to 0x58 when it fires; proc+0x28 is the 0x50-frame budget for
 * the whole scream; 0x48 counts 0x10 down and spawns a fresh t_scream_wave thread
 * every time it reaches zero; and 0x44 is the 0x28-frame budget for the alternate
 * 0x6a2 loop. So the scream is a wave emitter with its own lifetime, its own sound
 * cadence and two different ways to end.
 *
 * **NewThread is called with t_scream_wave, which lives in this file**, and it is
 * called from both loops -- so however the scream ends, waves keep being emitted
 * once every sixteen frames until it does.
 *
 * **The strike outcome depends on 0x18.** Connecting with 0x18 set takes blocking
 * away and switches to the 0x6a2 loop with a fresh 0x28-frame budget; connecting
 * without it latches proc+0x2c, shortens proc->field28 to 0x20, and takes a hit off
 * the opponent's counter through dec_his_p_hit -- the only site in this file to
 * call that. proc+0x2c being latched then stops any further strike check, so the
 * scream can only connect once.
 *
 * **`obj->a10 = (uint32_t)obj->field00` in the 0x64f state stores a POINTER into
 * the field the 0x6a2 loop counts down.** Nothing reads 0x44 between that store and
 * the only path into 0x6a2, which overwrites it with 0x28 first, so the pointer is
 * never decremented. Transcribed as written -- it is the sort of store that looks
 * like a bug and cannot be shown to be one from this routine alone.
 *
 * An action of 0x615 on the opponent aborts the whole move before any of this,
 * handing the thread to t_r_scream instead.
 */
void dec_his_p_hit(MK3OBJ *obj);
MK3THREAD *NewThread(void *owner, MK3THREADFUNC func);
long t_r_scream(MK3THREAD *thread);              /* pointer slot 0x000f3174 */

long tl_stat_do_lia_scream(MK3THREAD *thread)
{
    MK3OBJ      *obj   = (MK3OBJ *)thread->proc;
    MK3OBJPROC  *proc  = obj->field00;
    uint32_t     token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field20 = 0x10f;
        init_special_act(obj);

        obj->field40 = 0;
        get_char_ani2(obj);

        obj->field1c = 3;

        *mk3_frame(thread, thread->frame + 1) = 0x64f;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x64f) {
        get_his_action(obj);
        if (obj->field20 == 0x615)
            return mk3_install(thread, (MK3THREADFUNC)t_r_scream);

        *(uint32_t *)((char *)proc + 0x2c) = 0;

        obj->field1c = 3;
        init_anirate(obj);

        *(uint32_t *)((char *)proc + 0x30) = 1;
        obj->a10 = (uint32_t)(uintptr_t)obj->field00;   /* a pointer; see above */

        obj->field1c = 6;
        create_fx(obj);

        obj->field1c = 0x50;
        obj->field48 = 1;
        proc->field28 = obj->field1c;

        *mk3_frame(thread, thread->frame + 1) = 0x665;
        thread->fieldfc = 1;
        return 1;
    }

    if (token == 0x665) {
        next_anirate(obj);

        obj->field1c = *(uint32_t *)((char *)proc + 0x30) - 1;
        if (obj->field1c == 0) {
            obj->field1c = 1;
            ochar_sound(obj);
            obj->field1c = 0x58;
        }
        *(uint32_t *)((char *)proc + 0x30) = obj->field1c;

        obj->field1c = *(uint32_t *)((char *)proc + 0x2c);
        if (obj->field1c == 0) {
            obj->field1c = 0x11;
            strike_check_a0(obj);

            if (obj->field5c != 0) {
                if (obj->field18 != 0) {
                    set_no_block(obj);
                    obj->a10 = 0x28;

                    *mk3_frame(thread, thread->frame + 1) = 0x6a2;
                    thread->fieldfc = 1;
                    return 1;
                }

                *(uint32_t *)((char *)proc + 0x2c) = 1;
                obj->field1c = 0x20;
                proc->field28 = 0x20;
                dec_his_p_hit(obj);
            }
        }

        obj->field48 = obj->field48 - 1;
        if (obj->field48 == 0) {
            obj->field48 = 0x10;
            NewThread(obj, (MK3THREADFUNC)t_scream_wave);
        }

        obj->field1c = proc->field28 - 1;
        if (obj->field1c != 0) {
            proc->field28 = obj->field1c;
            *mk3_frame(thread, thread->frame + 1) = 0x665;
            thread->fieldfc = 1;
            return 1;
        }

    } else if (token == 0x6a2) {
        obj->field48 = obj->field48 - 1;
        if (obj->field48 == 0) {
            obj->field48 = 0x10;
            NewThread(obj, (MK3THREADFUNC)t_scream_wave);
        }

        next_anirate(obj);

        obj->a10 = obj->a10 - 1;
        if (obj->a10 != 0) {
            *mk3_frame(thread, thread->frame + 1) = 0x6a2;
            thread->fieldfc = 1;
            return 1;
        }

    } else {
        return -3;
    }

    /* the finish, reached when either budget runs out */
    obj->field40 = 0;
    get_char_ani2(obj);
    find_part2(obj);
    find_part2(obj);
    delete_slave(obj);

    obj->field1c = 3;
    return mk3_install(thread, (MK3THREADFUNC)t_mframew);
}


/* ---------------------------------------------------------------- t_combo_air_pause
 *
 * armv7 0x0004d114, 700 bytes.  **Complete.**
 *
 *      token == 0:      obj->field20 = obj->field00->field28 = obj->field18
 *                       push obj->field1c
 *                       stop_me_player(obj); clear_combo_butn(obj)
 *                       obj->a10 = obj->field00->field20 = pop
 *                       token := 0x8b3, park 4
 *
 *      token == 0x8b3:  is_he_airborn(obj)
 *                       if (obj->field5c != 0) {
 *                           obj->a10 -= 4
 *                           if (obj->a10 <= 0) obj->a10 = 1
 *                           obj->field1c = obj->a10
 *                           token := 0x8ea, park obj->field1c
 *                       }
 *                       am_i_joy(obj)
 *                       if (obj->field5c == 0) -- the elbow path --
 *                       obj->field28 = 0x10
 *                       obj->field48 = G + 0x3d0 + proc->field08 * 2
 *                       obj->a10     = G + 0x3dc + proc->field08 * 2
 *                       obj->field1c = proc->field20 - 4
 *                       if (that <= 0) obj->field1c = 1
 *                       proc->field20 = obj->field1c
 *                       token := 0x8cc, park 1
 *
 *      token == 0x8cc:  if (*(uint32_t *)obj->field48 != 0) -- the elbow path --
 *                       if (*(uint32_t *)obj->a10     != 0) -- the knee path --
 *                       obj->field1c = proc->field20 - 1
 *                       if (that != 0) {
 *                           proc->field20 = obj->field1c
 *                           token := 0x8cc, park 1
 *                       }
 *                       pop a level, or t_local_reaction_exit
 *
 *      the elbow path:  air_combo_setup(obj)
 *                       pop one level, inheriting the child's handler
 *                       token := 0x8da, descend into t_do_elbow
 *
 *      the knee path:   air_combo_setup(obj)
 *                       pop one level, inheriting the child's handler
 *                       token := 0x8e0, descend into t_do_knee
 *
 *      token == 0x8da:
 *      token == 0x8e0:  frame[frame].handler = t_local_reaction_exit
 *
 *      token == 0x8ea:  pop a level, or t_local_reaction_exit
 *
 *      otherwise:       return -3
 *
 * **This is the air-combo window, and it is where mkcombo.c's knee and elbow
 * starters are entered from.** State 0x8cc polls two words a frame apart: a non-zero
 * word at 0x48 launches t_do_elbow and a non-zero word at 0x44 launches t_do_knee,
 * each through pointer slots 0x000f3888 and 0x000f38b4.
 *
 * **The two addresses it polls are the button ring `clear_combo_butn` clears.**
 * 0x48 points at `G + 0x3d0 + proc->field08 * 2` -- the same base, the same
 * per-player offset of two -- and 0x44 at `G + 0x3dc`, twelve bytes further on, which
 * is one past the six halfwords that routine zeroes. So the two are the two halves
 * of one button history, and this routine calls `clear_combo_butn` itself in state 0
 * to start from empty.
 *
 * **A human and the machine take different routes.** am_i_joy answering clear goes
 * straight to the elbow path without polling anything; only a person gets the
 * window. An airborne opponent skips the whole thing and shortens a countdown by 4 a
 * pass instead, floored at 1.
 *
 * **Both combo paths pop a level and inherit the child's handler before
 * descending** -- the same six-line frame shuffle t_plwins and t_finish_him use --
 * so the combo starter replaces this level rather than stacking on top of it.
 *
 * 0x44 is a countdown in one state and a ring pointer in another, and 0x48 is a ring
 * pointer here where the rest of the file uses it as a counter. Both are decided by
 * the state, not by the field.
 */
long am_i_joy(MK3OBJ *obj);
void clear_combo_butn(MK3OBJ *obj);
void air_combo_setup(MK3OBJ *obj);
long t_do_elbow(MK3THREAD *thread);              /* pointer slot 0x000f3888 */
long t_do_knee(MK3THREAD *thread);               /* pointer slot 0x000f38b4 */

long t_combo_air_pause(MK3THREAD *thread)
{
    MK3OBJ      *obj   = (MK3OBJ *)thread->proc;
    MK3OBJPROC  *proc  = obj->field00;
    uint32_t     token = *mk3_frame(thread, thread->frame + 1);
    uint32_t     cur, f, n, handler;

    if (token == 0) {
        obj->field20   = obj->field18;
        proc->field28  = obj->field18;

        cur = thread->fieldf8;
        *mk3_arg(thread, cur) = obj->field1c;
        thread->fieldf8 = cur + 1;

        stop_me_player(obj);
        clear_combo_butn(obj);

        cur = thread->fieldf8 - 1;
        thread->fieldf8 = cur;
        obj->a10 = *mk3_arg(thread, cur);
        proc->field20 = obj->a10;

        *mk3_frame(thread, thread->frame + 1) = 0x8b3;
        thread->fieldfc = 4;
        return 4;
    }

    if (token == 0x8b3) {
        is_he_airborn(obj);

        if (obj->field5c != 0) {
            obj->a10 = obj->a10 - 4;
            if ((long)obj->a10 <= 0)
                obj->a10 = 1;

            obj->field1c = obj->a10;
            *mk3_frame(thread, thread->frame + 1) = 0x8ea;
            thread->fieldfc = obj->field1c;
            return (long)obj->field1c;
        }

        am_i_joy(obj);
        if (obj->field5c == 0)
            goto combo_elbow;                     /* the elbow path */

        obj->field28 = 0x10;
        obj->field48 = (uint32_t)(uintptr_t)
            (G_BYTES + 0x3d0 + proc->field08 * 2);
        obj->a10 = (uint32_t)(uintptr_t)
            (G_BYTES + 0x3dc + proc->field08 * 2);

        obj->field1c = proc->field20 - 4;
        if ((long)obj->field1c <= 0)
            obj->field1c = 1;
        proc->field20 = obj->field1c;

        *mk3_frame(thread, thread->frame + 1) = 0x8cc;
        thread->fieldfc = 1;
        return 1;
    }

    if (token == 0x8cc) {
        if (*(uint32_t *)(void *)(uintptr_t)obj->field48 != 0)
            goto combo_elbow;                     /* the elbow path */

        if (*(uint32_t *)(void *)(uintptr_t)obj->a10 != 0)
            goto combo_knee;

        obj->field1c = proc->field20 - 1;
        if (obj->field1c != 0) {
            proc->field20 = obj->field1c;
            *mk3_frame(thread, thread->frame + 1) = 0x8cc;
            thread->fieldfc = 1;
            return 1;
        }

        if ((long)thread->frame > 0) {
            thread->frame = thread->frame - 1;
            return 0;
        }
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }

    if (token == 0x8ea) {
        if ((long)thread->frame > 0) {
            thread->frame = thread->frame - 1;
            return 0;
        }
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }

    if (token != 0x8da && token != 0x8e0)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

    /* Two copies in the binary (0x4d240 elbow, 0x4d304 knee), each with its
     * own air_combo_setup, token and routine; written out the same way.
     * Corrected, 2026-09-26: at the bottom of the stack the handler install
     * also clears the slot above, which the first transcription left out. */
combo_elbow:
    air_combo_setup(obj);

    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
    } else {
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_local_reaction_exit;
        *mk3_frame(thread, thread->frame + 1) = 0;
    }

    f = thread->frame;
    n = f + 1;
    handler = mk3_frame(thread, n)[1];
    *mk3_frame(thread, n) = *mk3_frame(thread, n + 1);
    mk3_frame(thread, f)[1] = handler;

    *mk3_frame(thread, thread->frame + 1) = 0x8da;
    thread->frame = thread->frame + 1;              /* push a level */
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_do_elbow;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;

combo_knee:
    air_combo_setup(obj);

    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
    } else {
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_local_reaction_exit;
        *mk3_frame(thread, thread->frame + 1) = 0;
    }

    f = thread->frame;
    n = f + 1;
    handler = mk3_frame(thread, n)[1];
    *mk3_frame(thread, n) = *mk3_frame(thread, n + 1);
    mk3_frame(thread, f)[1] = handler;

    *mk3_frame(thread, thread->frame + 1) = 0x8e0;
    thread->frame = thread->frame + 1;              /* push a level */
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_do_knee;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------------------------ tl_do_shake
 *
 * armv7 0x0004e368, 760 bytes.  **Complete.**  Nine states.
 *
 *      token == 0:      obj->field20 = 0x111
 *                       init_special_act(obj)
 *                       obj->field1c = 0x10
 *                       *(uint32_t *)((char *)proc + 0x58) = 0x10
 *                       obj->field40 = 0x00030002
 *                       token := 0x58f, descend into t_animate2_a9
 *
 *      token == 0x58f:  obj->field48 = 6
 *                       -- into the search --
 *
 *      the search:      q_is_he_a_boss(obj)
 *                       if (obj->field5c == 0) {
 *                           obj->field1c = 0x10
 *                           strike_check_a0(obj)
 *                           if (obj->field5c != 0) {
 *                               lights_on_hit(obj)
 *                               if (obj->field18 != 0) token := 0x5a1, park 0xa
 *                               obj->field38 = t_shake_suspended
 *                               takeover_him(obj)
 *                               match_him_with_me_f(obj); me_in_front(obj)
 *                               obj->field48 = 0x40; get_his_a11_ani(obj)
 *                               obj->field1c = 8; his_group_sound(obj)
 *                               obj->field1c = 5
 *                               token := 0x5ba, descend into t_double_mframew
 *                           }
 *                       }
 *                       token := 0x59b, park 1
 *
 *      token == 0x59b:  if (--obj->field48 > 0) -- back into the search --
 *                       token := 0x5a1, park 0xa
 *
 *      token == 0x5a1:  obj->field40 = 2; get_char_ani2(obj)
 *                       obj->field1c = 5
 *                       frame[frame].handler = t_backwards_ani
 *
 *      token == 0x5ba:  obj->field1c = proc->field20 = 4
 *                       push obj->field40; push obj->field48
 *                       obj->field1c = 3
 *                       token := 0x5c1, descend into t_double_mframew
 *
 *      token == 0x5c1:  obj->field48 = pop; obj->field40 = pop
 *                       obj->field1c = 8; damage_to_him(obj)
 *                       if (--proc->field20 > 0) -- back to the 0x5ba body --
 *                       double_next_a9(obj)
 *                       token := 0x5cb, park 3
 *
 *      token == 0x5cb:  double_next_a9(obj)
 *                       token := 0x5cd, park 3
 *
 *      token == 0x5cd:  double_next_a9(obj)
 *                       obj->field38 = t_r_post_shake
 *                       takeover_him(obj)
 *                       token := 0x5d3, park 8
 *
 *      token == 0x5d3:  obj->field40 = 2; get_char_ani2(obj)
 *                       obj->field1c = 2
 *                       token := 0x5d7, descend into t_backwards_ani
 *
 *      token == 0x5d7:  frame[frame].handler = t_local_reaction_exit
 *
 *      otherwise:       return -3
 *
 * **Two loops with a handover between them.** The first, 0x58f and 0x59b, gives the
 * grab six frames to connect; the second, 0x5ba and 0x5c1, shakes four times and does
 * damage on every pass, counted down in proc->field20. Between them the victim is
 * taken over twice: `t_shake_suspended` when the grab lands, and `t_r_post_shake`
 * when the shaking is done.
 *
 * **t_shake_suspended is the routine written earlier in this file that reinstalls
 * itself and waits on action 0x111** -- which is exactly the action state 0 puts in
 * 0x20 here. So the two ends match: this routine sets the action, hands the victim a
 * poller that waits for it, and the victim stays held until the shaking ends.
 *
 * **0x40 and 0x48 are pushed and popped on every pass of the shake loop**, not once
 * around it, because the loop re-enters the 0x5ba body which does the pushing. Four
 * shakes therefore mean four push/pop pairs -- wasteful but harmless, and worth
 * noting because it looks at first like a leak of two stack slots.
 *
 * A boss is exempt from the strike check, as in tl_do_ermac_slam and
 * tl_stat_do_quake; and 0x18 being set on a connection skips the whole shake and goes
 * straight to the recovery, which is the third rule in this file keyed on that field.
 */
void lights_on_hit(MK3OBJ *obj);
void get_his_a11_ani(MK3OBJ *obj);
void his_group_sound(MK3OBJ *obj);
void match_him_with_me_f(MK3OBJ *obj);
long t_shake_suspended(MK3THREAD *thread);
long t_r_post_shake(MK3THREAD *thread);          /* pointer slot 0x000f3760 */
long t_double_mframew(MK3THREAD *thread);        /* pointer slot 0x000f36a8 */
long double_next_a9(MK3OBJ *obj);

long tl_do_shake(MK3THREAD *thread)
{
    MK3OBJ      *obj   = (MK3OBJ *)thread->proc;
    MK3OBJPROC  *proc  = obj->field00;
    uint32_t     token = *mk3_frame(thread, thread->frame + 1);
    uint32_t     cur;

    if (token == 0) {
        obj->field20 = 0x111;
        init_special_act(obj);

        obj->field1c = 0x10;
        *(uint32_t *)((char *)proc + 0x58) = 0x10;
        obj->field40 = 0x00030002;

        *mk3_frame(thread, thread->frame + 1) = 0x58f;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_animate2_a9;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x58f || token == 0x59b) {
        if (token == 0x58f) {
            obj->field48 = 6;
        } else {
            obj->field48 = obj->field48 - 1;
            if ((long)obj->field48 <= 0) {
                *mk3_frame(thread, thread->frame + 1) = 0x5a1;
                thread->fieldfc = 0xa;
                return 0xa;
            }
        }

        q_is_he_a_boss(obj);
        if (obj->field5c == 0) {
            obj->field1c = 0x10;
            strike_check_a0(obj);

            if (obj->field5c != 0) {
                lights_on_hit(obj);

                if (obj->field18 != 0) {
                    *mk3_frame(thread, thread->frame + 1) = 0x5a1;
                    thread->fieldfc = 0xa;
                    return 0xa;
                }

                obj->field38 = (uint32_t)(uintptr_t)t_shake_suspended;
                takeover_him(obj);
                match_him_with_me_f(obj);
                me_in_front(obj);

                obj->field48 = 0x40;
                get_his_a11_ani(obj);

                obj->field1c = 8;
                his_group_sound(obj);

                obj->field1c = 5;

                *mk3_frame(thread, thread->frame + 1) = 0x5ba;
                thread->frame = thread->frame + 1;  /* push a level */
                mk3_frame(thread, thread->frame)[1] =
                    (uint32_t)(uintptr_t)t_double_mframew;
                *mk3_frame(thread, thread->frame + 1) = 0;
                return 0;
            }
        }

        *mk3_frame(thread, thread->frame + 1) = 0x59b;
        thread->fieldfc = 1;
        return 1;
    }

    if (token == 0x5a1) {
        obj->field40 = 2;
        get_char_ani2(obj);
        obj->field1c = 5;
        return mk3_install(thread, (MK3THREADFUNC)t_backwards_ani);
    }

    if (token == 0x5ba || token == 0x5c1) {
        if (token == 0x5c1) {
            cur = thread->fieldf8 - 1;
            thread->fieldf8 = cur;
            obj->field48 = *mk3_arg(thread, cur);

            cur = thread->fieldf8 - 1;
            thread->fieldf8 = cur;
            obj->field40 = *mk3_arg(thread, cur);

            obj->field1c = 8;
            damage_to_him(obj);

            obj->field1c = proc->field20 - 1;
            if ((long)obj->field1c <= 0) {
                double_next_a9(obj);
                *mk3_frame(thread, thread->frame + 1) = 0x5cb;
                thread->fieldfc = 3;
                return 3;
            }
        } else {
            obj->field1c = 4;
        }

        proc->field20 = obj->field1c;

        cur = thread->fieldf8;
        *mk3_arg(thread, cur) = obj->field40;
        thread->fieldf8 = cur + 1;

        cur = thread->fieldf8;
        *mk3_arg(thread, cur) = obj->field48;
        thread->fieldf8 = cur + 1;

        obj->field1c = 3;

        *mk3_frame(thread, thread->frame + 1) = 0x5c1;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_double_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x5cb) {
        double_next_a9(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x5cd;
        thread->fieldfc = 3;
        return 3;
    }

    if (token == 0x5cd) {
        double_next_a9(obj);

        obj->field38 = (uint32_t)(uintptr_t)t_r_post_shake;
        takeover_him(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x5d3;
        thread->fieldfc = 8;
        return 8;
    }

    if (token == 0x5d3) {
        obj->field40 = 2;
        get_char_ani2(obj);
        obj->field1c = 2;

        *mk3_frame(thread, thread->frame + 1) = 0x5d7;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_backwards_ani;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x5d7)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* --------------------------------------------------------------------- tl_do_leg_throw
 *
 * armv7 0x0004f6c8, 776 bytes.  **Complete.**  Nine states.
 *
 *      token == 0:      obj->field20 = 0x110
 *                       init_special_act(obj)
 *                       rsnd_func(obj, 0xf)
 *                       obj->field1c = 0x10
 *                       *(uint32_t *)((char *)proc + 0x58) = 0x10
 *                       obj->field40 = 0; get_char_ani2(obj)
 *                       obj->field1c = 0x00020003
 *                       token := 0x399, descend into t_animate_a0_frames
 *
 *      token == 0x399:  obj->field1c = 1
 *                       token := 0x39b, descend into t_mframew
 *
 *      token == 0x39b:  q_is_he_a_boss(obj)
 *                       if (obj->field5c == 0) {
 *                           obj->field1c = 0x10
 *                           strike_check_a0(obj)
 *                           if (obj->field5c != 0) -- the grab, below --
 *                       }
 *                       obj->field1c = 0x10
 *                       obj->field20 = proc->field18 = 0x608
 *                       token := 0x3ea, park obj->field1c
 *
 *      the grab:        obj->field1c = proc->field18 = 0x608
 *                       obj->field1c = clear_inviso; call_a0_for_him(obj)
 *                       if (obj->field18 != 0) {
 *                           rsnd_func(obj, 5)
 *                           obj->field1c = 0x18
 *                           -- into the 0x3ea setup, keeping 0x18 --
 *                       }
 *                       obj->field1c = face_opponent; call_a0_for_him(obj)
 *                       call_for_him(obj, set_noedge)
 *                       rsnd_func(obj, 5)
 *                       obj->field1c = 2; his_group_sound(obj)
 *                       *(uint16_t *)(G + 0x456) = 0x60
 *                       obj->field1c = 0x110
 *                       xfer_him_to_flipped_pause(obj)
 *                       inc_his_p_hit(obj)
 *                       obj->field40 = 0x3e; get_his_char_ani(obj)
 *                       obj->field48 = obj->field40
 *                       obj->a10 = proc->him
 *                       obj->field40 = 0; get_char_ani2(obj)
 *                       find_part2(obj); double_next_a9(obj)
 *                       obj->field1c = 0x30000
 *                       obj->field20 = 3
 *                       obj->field24 = 3 + 2 = 5
 *                       token := 0x3d1, descend into t_shake_him_up
 *
 *      token == 0x3d1:  obj->field1c = 4
 *                       token := 0x3d4, descend into t_double_mframew
 *
 *      token == 0x3d4:  call_for_him(obj, clear_noedge)
 *                       obj->field38 = t_r_leg_slammed
 *                       takeover_him(obj)
 *                       obj->field1c = 5
 *                       token := 0x3da, descend into t_mframew
 *
 *      token == 0x3da:  frame[frame].handler = t_local_reaction_exit
 *
 *      token == 0x3ea:  obj->field40 = 0; get_char_ani2(obj)
 *                       find_part2(obj)
 *                       obj->field1c = 5
 *                       token := 0x3f1, descend into t_mframew
 *
 *      token == 0x3f1:  obj->field1c = 5
 *                       token := 0x3f3, descend into t_mframew
 *
 *      token == 0x3f3:  frame[frame].handler = t_local_reaction_exit
 *
 *      otherwise:       return -3
 *
 * **A grab with two whole branches: one where it lands and one where it does not.**
 * A miss, a boss, or 0x18 being set all end up in the 0x3ea chain -- three waits and
 * out. Only a clean connection on a non-boss with 0x18 clear reaches the grab, and
 * that path is by far the longest thing in this file.
 *
 * **`call_for_him` is a third way to run a routine on the other fighter**, alongside
 * `call_a0_for_him` and the 0x38-plus-`takeover_him` handover. This routine uses all
 * three: `call_a0_for_him` twice through 0x1c, `call_for_him` twice with the routine
 * in a register (`set_noedge` on the way in, `clear_noedge` on the way out), and
 * `takeover_him` at the end to install t_r_leg_slammed on the victim.
 *
 * **`set_noedge` and `clear_noedge` bracket the throw**, five states apart -- so the
 * victim is exempt from the screen edge for exactly as long as it is being carried.
 *
 * **0x1c takes a packed pair for once**, 0x00020003, handed to t_animate_a0_frames.
 * Every other packed pair in this module goes into 0x40, so this is the first site
 * where 0x1c carries one -- and it goes to a routine whose name says it walks frames.
 *
 * The victim handler is `t_r_leg_slammed`, written earlier in this same file, and
 * that routine is the one that gates its damage on the p_hit counter. So the two
 * halves of a leg throw are both here.
 */
void call_for_him(MK3OBJ *obj, void (*fn)(MK3OBJ *));
void clear_noedge(MK3OBJ *obj);                  /* pointer slot 0x000f3190 */
void set_noedge(MK3OBJ *obj);                    /* pointer slot 0x000f375c */
void inc_his_p_hit(MK3OBJ *obj);
void get_his_char_ani(MK3OBJ *obj);
void xfer_him_to_flipped_pause(MK3OBJ *obj);
long t_animate_a0_frames(MK3THREAD *thread);     /* pointer slot 0x000f36b8 */
long t_shake_him_up(MK3THREAD *thread);          /* pointer slot 0x000f36a4 */
long t_r_leg_slammed(MK3THREAD *thread);

long tl_do_leg_throw(MK3THREAD *thread)
{
    MK3OBJ      *obj   = (MK3OBJ *)thread->proc;
    MK3OBJPROC  *proc  = obj->field00;
    uint32_t     token = *mk3_frame(thread, thread->frame + 1);
    int          grabbed = 0;

    if (token == 0) {
        obj->field20 = 0x110;
        init_special_act(obj);
        rsnd_func(obj, 0xf);

        obj->field1c = 0x10;
        *(uint32_t *)((char *)proc + 0x58) = 0x10;

        obj->field40 = 0;
        get_char_ani2(obj);

        obj->field1c = 0x00020003;

        *mk3_frame(thread, thread->frame + 1) = 0x399;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_animate_a0_frames;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x399) {
        obj->field1c = 1;

        *mk3_frame(thread, thread->frame + 1) = 0x39b;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x39b) {
        q_is_he_a_boss(obj);

        if (obj->field5c == 0) {
            obj->field1c = 0x10;
            strike_check_a0(obj);
            if (obj->field5c != 0)
                grabbed = 1;
        }

        if (grabbed) {
            obj->field1c = 0x608;
            proc->field18 = 0x608;

            obj->field1c = (uint32_t)(uintptr_t)clear_inviso;
            call_a0_for_him(obj);

            if (obj->field18 != 0) {
                rsnd_func(obj, 5);
                obj->field1c = 0x18;
                goto miss;              /* keeps 0x18 in 0x1c */
            }

            obj->field1c = (uint32_t)(uintptr_t)face_opponent;
            call_a0_for_him(obj);
            call_for_him(obj, set_noedge);

            rsnd_func(obj, 5);
            obj->field1c = 2;
            his_group_sound(obj);

            *(uint16_t *)(G_BYTES + 0x456) = 0x60;

            obj->field1c = 0x110;
            xfer_him_to_flipped_pause(obj);
            inc_his_p_hit(obj);

            obj->field40 = 0x3e;
            get_his_char_ani(obj);
            obj->field48 = obj->field40;

            obj->a10 = proc->him;
            obj->field40 = 0;
            get_char_ani2(obj);
            find_part2(obj);
            double_next_a9(obj);

            obj->field1c = 0x30000;
            obj->field20 = 3;
            obj->field24 = 3 + 2;

            *mk3_frame(thread, thread->frame + 1) = 0x3d1;
            thread->frame = thread->frame + 1;      /* push a level */
            mk3_frame(thread, thread->frame)[1] =
                (uint32_t)(uintptr_t)t_shake_him_up;
            *mk3_frame(thread, thread->frame + 1) = 0;
            return 0;
        }

        obj->field1c = 0x10;

miss:
        obj->field20 = 0x608;
        proc->field18 = 0x608;

        *mk3_frame(thread, thread->frame + 1) = 0x3ea;
        thread->fieldfc = obj->field1c;
        return (long)obj->field1c;
    }

    if (token == 0x3d1) {
        obj->field1c = 4;

        *mk3_frame(thread, thread->frame + 1) = 0x3d4;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_double_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x3d4) {
        call_for_him(obj, clear_noedge);

        obj->field38 = (uint32_t)(uintptr_t)t_r_leg_slammed;
        takeover_him(obj);

        obj->field1c = 5;

        *mk3_frame(thread, thread->frame + 1) = 0x3da;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x3ea) {
        obj->field40 = 0;
        get_char_ani2(obj);
        find_part2(obj);

        obj->field1c = 5;

        *mk3_frame(thread, thread->frame + 1) = 0x3f1;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x3f1) {
        obj->field1c = 5;

        *mk3_frame(thread, thread->frame + 1) = 0x3f3;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x3da && token != 0x3f3)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* ---------------------------------------------------------------------- tl_do_noogy
 *
 * armv7 0x0004eb80, 1244 bytes.  **Complete.**  Seventeen states, the largest
 * routine in this file.
 *
 *      token == 0:      init_special(obj); me_in_back(obj)
 *                       *(uint32_t *)((char *)proc + 0x58) = 0x14
 *                       obj->field1c = 1; group_sound(obj)
 *                       obj->field40 = 1; get_char_ani2(obj)
 *                       obj->field1c = 3
 *                       obj->field20 = 0x112
 *                       token := 0x4c6, descend into t_act_mframew
 *
 *      token == 0x4c6:  obj->field48 = 6
 *      the strike:      obj->field1c = 0x14
 *                       strike_check_a0(obj)
 *                       if (obj->field5c != 0) -- the grab --
 *                       token := 0x4ce, park 1
 *
 *      token == 0x4ce:  if (--obj->field48 <= 0) -- the give-up --
 *                       -- back to the strike --
 *
 *      the give-up:     obj->field1c = proc->field18 = 0x61b
 *                       token := 0x4db, park 0xd
 *
 *      token == 0x4db:  obj->field40 = 1; get_char_ani2(obj)
 *                       obj->a10 = obj->field40
 *                       obj->field40 += 4
 *                       do_next_a9_frame(obj)
 *                       token := 0x4e2, park 6
 *
 *      token == 0x4e2:  obj->field40 = obj->a10
 *                       do_next_a9_frame(obj)
 *                       token := 0x4e5, park 6
 *
 *      token == 0x4e5:  pop a level, or t_local_reaction_exit
 *
 *      the grab:        lights_on_hit(obj)
 *                       if (obj->field18 != 0) -- the give-up --
 *                       obj->field1c = 6; ochar_sound(obj)
 *                       obj->field38 = t_noogy_suspended
 *                       takeover_him(obj)
 *                       push obj->field40
 *                       obj->field40 = 0x1c; get_his_char_ani(obj)
 *                       obj->field48 = obj->field40
 *                       obj->field40 += 4
 *                       do_his_next_a9_frame(obj)
 *                       obj->field40 = pop
 *                       obj->a10 = t_noog_lineup_1
 *                       token := 0x4fd, descend into t_edge_of_world_lineup
 *
 *      token == 0x4fd:  obj->a10 = proc->him
 *                       obj->field1c = 0x00030003
 *                       obj->field20 = obj->field24 = 3
 *                       token := 0x504, descend into t_shake_him_up
 *
 *      token == 0x504:  obj->field1c = 0x10; adjust_him_a0(obj)
 *      token == 0x50e:  obj->field1c = 0x1c (0x50e only)
 *                       do_next_a9_frame(obj)
 *                       token := 0x509 (from 0x504) / 0x513 (from 0x50e), park 4
 *
 *      token == 0x509:  obj->field1c = 0x00050010; adjust_him_a0(obj)
 *                       do_next_a9_frame(obj)
 *                       token := 0x50e, park 4
 *
 *      token == 0x513:  proc->field28 = 0
 *                       obj->field1c = 5
 *      the noogy step:  proc->field20 = obj->field1c
 *                       obj->field1c = 3
 *                       token := 0x521, descend into t_mframew
 *
 *      token == 0x521:  obj->field1c = obj->field54 = 7
 *                       add_combo_damage(obj); damage_to_him(obj)
 *                       inc_his_p_hit(obj)
 *                       obj->field1c = 2; his_group_sound(obj)
 *                       tsound_func(obj, 0x83)
 *                       obj->field1c = 0xc; ochar_sound(obj)
 *                       obj->field1c = upcut_blood_me; call_a0_for_him(obj)
 *                       if (proc->field20 == 1 || proc->field28 != 0) {
 *                           obj->field38 = t_r_last_noogy
 *                           xfer_otherguy(obj); dec_his_p_hit(obj)
 *                           token := 0x55a, park 0x10
 *                       }
 *                       push obj->field40; push obj->field48
 *                       obj->field40 = obj->field48
 *      the frame walk:  do_his_next_a9_frame(obj)
 *                       token := 0x544, park 2
 *
 *      token == 0x544:  obj->field1c = *(uint32_t *)obj->field40
 *                       if (that != 0) -- back to the frame walk --
 *                       obj->field48 = pop; obj->field40 = pop
 *                       noogy_early_check(obj)
 *                       token := 0x54d, park 2
 *
 *      token == 0x54d:  noogy_early_check(obj)
 *                       token := 0x54f, park 2
 *
 *      token == 0x54f:  noogy_early_check(obj)
 *                       obj->field1c = proc->field20 - 1
 *                       -- back to the noogy step --
 *
 *      token == 0x55a:  obj->field1c = 0x00040004
 *                       token := 0x55c, descend into t_animate_a0_frames
 *
 *      token == 0x55c:  pop a level, or t_local_reaction_exit
 *
 *      otherwise:       return -3
 *
 * **The noogy is a counted loop of hits, and proc+0x20 counts them.** 0x513 loads it
 * with 5; each pass through 0x521 does damage and each pass through 0x54f takes one
 * off, and when it reaches 1 -- or when proc+0x28 goes non-zero -- the victim is
 * handed to t_r_last_noogy instead and the move ends. So five noogies, or fewer if
 * something sets that flag.
 *
 * **`noogy_early_check` is called three times in a row, once per state**, at 0x544,
 * 0x54d and 0x54f with two-frame parks between them. That routine is the one written
 * earlier in this file that consults the four-button gate and raises proc+0x28 when a
 * press runs past seven -- so those three calls are the window in which the player can
 * end the noogy early, and proc+0x28 is how the answer comes back here.
 *
 * **Four of the routines it reaches are in this same file**: t_noogy_suspended for the
 * victim, t_noog_lineup_1 handed to t_edge_of_world_lineup through 0x44,
 * t_edge_of_world_lineup itself, and upcut_blood_me through call_a0_for_him. So the
 * whole move is local except for the shared waits and t_r_last_noogy.
 *
 * **0x44 carries a handler into t_edge_of_world_lineup.** That is the routine whose
 * only oddity was taking its body from 0x44 -- and this is the caller that puts one
 * there. The pair explains each other.
 *
 * Two more packed pairs in 0x1c, 0x00030003 and 0x00040004, plus 0x00050010 -- the
 * first two doubled and the third not, which matches what the shake magnitudes showed:
 * two independent halves that often happen to agree.
 */
void me_in_back(MK3OBJ *obj);
void add_combo_damage(MK3OBJ *obj);
void xfer_otherguy(MK3OBJ *obj);
long do_his_next_a9_frame(MK3OBJ *obj);
void adjust_him_a0(MK3OBJ *obj);
void noogy_early_check(MK3OBJ *obj);
void upcut_blood_me(MK3OBJ *obj);
long t_noogy_suspended(MK3THREAD *thread);
long t_noog_lineup_1(MK3THREAD *thread);
long t_edge_of_world_lineup(MK3THREAD *thread);
long t_r_last_noogy(MK3THREAD *thread);          /* pointer slot 0x000f3178 */

long tl_do_noogy(MK3THREAD *thread)
{
    MK3OBJ      *obj   = (MK3OBJ *)thread->proc;
    MK3OBJPROC  *proc  = obj->field00;
    uint32_t     token = *mk3_frame(thread, thread->frame + 1);
    uint32_t     cur;
    int          strike = 0, giveup = 0, grab = 0, last = 0, walk = 0;

    if (token == 0) {
        init_special(obj);
        me_in_back(obj);

        *(uint32_t *)((char *)proc + 0x58) = 0x14;

        obj->field1c = 1;
        group_sound(obj);

        obj->field40 = 1;
        get_char_ani2(obj);

        obj->field1c = 3;
        obj->field20 = 0x112;

        *mk3_frame(thread, thread->frame + 1) = 0x4c6;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_act_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x4c6) {
        obj->field48 = 6;
        strike = 1;

    } else if (token == 0x4ce) {
        obj->field48 = obj->field48 - 1;
        if ((long)obj->field48 <= 0)
            giveup = 1;
        else
            strike = 1;
    }

    if (strike) {
        obj->field1c = 0x14;
        strike_check_a0(obj);

        if (obj->field5c != 0) {
            lights_on_hit(obj);
            if (obj->field18 != 0)
                giveup = 1;
            else
                grab = 1;
        } else {
            *mk3_frame(thread, thread->frame + 1) = 0x4ce;
            thread->fieldfc = 1;
            return 1;
        }
    }

    if (giveup) {
        obj->field1c = 0x61b;
        proc->field18 = 0x61b;

        *mk3_frame(thread, thread->frame + 1) = 0x4db;
        thread->fieldfc = 0xd;
        return 0xd;
    }

    if (grab) {
        obj->field1c = 6;
        ochar_sound(obj);

        obj->field38 = (uint32_t)(uintptr_t)t_noogy_suspended;
        takeover_him(obj);

        cur = thread->fieldf8;
        *mk3_arg(thread, cur) = obj->field40;
        thread->fieldf8 = cur + 1;

        obj->field40 = 0x1c;
        get_his_char_ani(obj);
        obj->field48 = obj->field40;
        obj->field40 = obj->field40 + 4;
        do_his_next_a9_frame(obj);

        cur = thread->fieldf8 - 1;
        thread->fieldf8 = cur;
        obj->field40 = *mk3_arg(thread, cur);

        obj->a10 = (uint32_t)(uintptr_t)t_noog_lineup_1;

        *mk3_frame(thread, thread->frame + 1) = 0x4fd;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_edge_of_world_lineup;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x4db) {
        obj->field40 = 1;
        get_char_ani2(obj);
        obj->a10 = obj->field40;
        obj->field40 = obj->field40 + 4;
        do_next_a9_frame(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x4e2;
        thread->fieldfc = 6;
        return 6;
    }

    if (token == 0x4e2) {
        obj->field40 = obj->a10;
        do_next_a9_frame(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x4e5;
        thread->fieldfc = 6;
        return 6;
    }

    if (token == 0x4e5 || token == 0x55c) {
        if ((long)thread->frame > 0) {
            thread->frame = thread->frame - 1;
            return 0;
        }
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }

    if (token == 0x4fd) {
        obj->a10 = proc->him;
        obj->field1c = 0x00030003;
        obj->field20 = 0x00030003 - 0x30000;
        obj->field24 = obj->field20;

        *mk3_frame(thread, thread->frame + 1) = 0x504;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_shake_him_up;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x504 || token == 0x50e) {
        if (token == 0x504) {
            obj->field1c = 0x10;
            adjust_him_a0(obj);
        } else {
            obj->field1c = 0x1c;
        }

        do_next_a9_frame(obj);
        /* the token is r8, still the last constant the dispatch compared:
         * 0x509 when entered as 0x504, 0x513 when entered as 0x50e */
        *mk3_frame(thread, thread->frame + 1) = token == 0x504 ? 0x509 : 0x513;
        thread->fieldfc = 4;
        return 4;
    }

    if (token == 0x509) {
        obj->field1c = 0x00050010;
        adjust_him_a0(obj);
        do_next_a9_frame(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x50e;
        thread->fieldfc = 4;
        return 4;
    }

    if (token == 0x513 || token == 0x54f) {
        if (token == 0x513) {
            proc->field28 = 0;
            obj->field1c = 5;
        } else {
            noogy_early_check(obj);
            obj->field1c = proc->field20 - 1;
        }

        proc->field20 = obj->field1c;
        obj->field1c = 3;

        *mk3_frame(thread, thread->frame + 1) = 0x521;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x521) {
        obj->field1c = 7;
        obj->field54 = 7;
        add_combo_damage(obj);
        damage_to_him(obj);
        inc_his_p_hit(obj);

        obj->field1c = 2;
        his_group_sound(obj);
        tsound_func(obj, 0x83);

        obj->field1c = 0xc;
        ochar_sound(obj);

        obj->field1c = (uint32_t)(uintptr_t)upcut_blood_me;
        call_a0_for_him(obj);

        obj->field1c = proc->field20;
        if (obj->field1c == 1) {
            last = 1;
        } else {
            obj->field1c = proc->field28;
            if (obj->field1c != 0)
                last = 1;
        }

        if (last) {
            obj->field38 = (uint32_t)(uintptr_t)t_r_last_noogy;
            xfer_otherguy(obj);
            dec_his_p_hit(obj);

            *mk3_frame(thread, thread->frame + 1) = 0x55a;
            thread->fieldfc = 0x10;
            return 0x10;
        }

        cur = thread->fieldf8;
        *mk3_arg(thread, cur) = obj->field40;
        thread->fieldf8 = cur + 1;

        cur = thread->fieldf8;
        *mk3_arg(thread, cur) = obj->field48;
        thread->fieldf8 = cur + 1;

        obj->field40 = obj->field48;
        walk = 1;

    } else if (token == 0x544) {
        obj->field1c = *(uint32_t *)(void *)(uintptr_t)obj->field40;
        if (obj->field1c != 0) {
            walk = 1;
        } else {
            cur = thread->fieldf8 - 1;
            thread->fieldf8 = cur;
            obj->field48 = *mk3_arg(thread, cur);

            cur = thread->fieldf8 - 1;
            thread->fieldf8 = cur;
            obj->field40 = *mk3_arg(thread, cur);

            noogy_early_check(obj);

            *mk3_frame(thread, thread->frame + 1) = 0x54d;
            thread->fieldfc = 2;
            return 2;
        }

    } else if (token == 0x54d) {
        noogy_early_check(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x54f;
        thread->fieldfc = 2;
        return 2;

    } else if (token == 0x55a) {
        obj->field1c = 0x00040004;

        *mk3_frame(thread, thread->frame + 1) = 0x55c;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_animate_a0_frames;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;

    } else if (!walk) {
        return -3;
    }

    /* the frame walk, reached from 0x521 and from 0x544 */
    do_his_next_a9_frame(obj);
    *mk3_frame(thread, thread->frame + 1) = 0x544;
    thread->fieldfc = 2;
    return 2;
}
