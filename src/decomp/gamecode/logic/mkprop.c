/*
 * mkprop.c -- gamecode/logic/mkprop.c, decompiled.
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

long t_zoom_blocked(struct MK3THREAD *thread);
long tl_do_slide(struct MK3THREAD *thread);

/* t_scorpion_tele_blocked -- armv7 0x0003c014, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = t_zoom_blocked
 *      frame[frame+1].w0 = 0
 */

long t_scorpion_tele_blocked(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_zoom_blocked);
}

/* tl_do_ninja_slide -- armv7 0x0003c048, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = tl_do_slide
 *      frame[frame+1].w0 = 0
 */

long tl_do_ninja_slide(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)tl_do_slide);
}

/* --------------------------------------------------------------------
 * Added by a later sweep -- tools/sweep.py, running the same
 * readers again after one of them learned something. Each still
 * refuses anything it cannot account for instruction by
 * instruction; see tools/pushfn.py and tools/leaffn.py.
 * -------------------------------------------------------------------- */

long am_i_airborn(MK3OBJ *obj);
void set_inviso(MK3OBJ *obj);
void inc_p_hit(MK3OBJ *obj);
void create_fx(MK3OBJ *obj);
long t_suspend_wait_action(MK3THREAD *thread);
long tl_do_scorp_tele(MK3THREAD *thread);
long t_local_reaction_exit(MK3THREAD *thread);
void match_me_with_him(MK3OBJ *obj);
void multi_adjust_xy(MK3OBJ *obj);
long is_he_left(MK3OBJ *obj);
long is_he_right(MK3OBJ *obj);
void face_opponent(MK3OBJ *obj);
long t_tele_scan(MK3THREAD *thread);
void air_init_special(MK3OBJ *obj);
void find_part2(MK3OBJ *obj);
void back_to_normal(MK3OBJ *obj);
void init_special_act(MK3OBJ *obj);
void ochar_sound(MK3OBJ *obj);
void find_ani_last_frame(MK3OBJ *obj);
long do_next_a9_frame(MK3OBJ *obj);
long t_main_hover_loop(MK3THREAD *thread);
long t_lfly5(MK3THREAD *thread);
void MKEvent_Add(long a, long b, long c, long d);
void match_him_with_me_f(MK3OBJ *obj);
void adjust_him_a0(MK3OBJ *obj);
long next_anirate(MK3OBJ *obj);
void advance_him(MK3OBJ *obj);
long is_he_airborn(MK3OBJ *obj);
void match_him_with_me(MK3OBJ *obj);
void adjust_him_xy(MK3OBJ *obj);
long strike_check_a0(MK3OBJ *obj);
long t_lao_angle_hit(MK3THREAD *thread);
long t_shake_ob_up(MK3THREAD *thread);
void find_ani2_part2(MK3OBJ *obj);
long t_air_sleep3(MK3THREAD *thread);
long t_land_on_yer_feet(MK3THREAD *thread);
long t_lao_angle_blocked(MK3THREAD *thread);
void sans_repell_3(MK3OBJ *obj);
void q_is_he_a_boss(MK3OBJ *obj);
long t_pounce_hit(MK3THREAD *thread);
long t_liz_fly_hit(MK3THREAD *thread);
long t_square3(MK3THREAD *thread);
void zero_turbo_bar(MK3OBJ *obj);
void lights_on_hit(MK3OBJ *obj);
long is_he_facing_me(MK3OBJ *obj);
void call_for_him(MK3OBJ *obj, void (*fn)(MK3OBJ *));
void adjust_him_x(MK3OBJ *obj);
void away_x_vel_him(MK3OBJ *obj);
long do_his_next_a9_frame(MK3OBJ *obj);
long t_r_bike_kicked_done(MK3THREAD *thread);
void call_a0_for_him(MK3OBJ *obj);
void find_part_a14(MK3OBJ *obj);
void get_his_a11_ani(MK3OBJ *obj);
long double_next_a9(MK3OBJ *obj);
void xfer_him_to_flipped_pause(MK3OBJ *obj);
void inc_his_p_hit(MK3OBJ *obj);
long t_r_post_bike(MK3THREAD *thread);
void pose_him_a9(MK3OBJ *obj);
void get_his_char_ani(MK3OBJ *obj);
void rsnd_func(MK3OBJ *unused, uint32_t which);
long t_double_mframew(MK3THREAD *thread);
long t_r_zoom_flipped(MK3THREAD *thread);
long tl_do_swat_zoom(MK3THREAD *thread);
long tl_do_super_kang(MK3THREAD *thread);
long t_do_backup(MK3THREAD *thread);
long tl_do_mileena_prop(MK3THREAD *thread);
long t_animate_a0_frames(MK3THREAD *thread);
long tl_jax_dash_punch(MK3THREAD *thread);
long tl_do_kano_upball(MK3THREAD *thread);
long tl_do_robo_tele(MK3THREAD *thread);
void clear_noflip(MK3OBJ *obj);
void set_no_block(MK3OBJ *obj);
void pose2_a9_manual(MK3OBJ *obj);
long t_wait_for_landing(MK3THREAD *thread);
void get_his_action(MK3OBJ *obj);
void edge_pick_a0(MK3OBJ *obj);
long t_r_pounce2(MK3THREAD *thread);
long am_i_joy(MK3OBJ *obj);
void mk_random(MK3OBJ *obj);
void stuff_buttons(MK3OBJ *obj, uint32_t buttons);
long t_do_jumpup_punch(MK3THREAD *thread);
long t_do_jumpup_kick(MK3THREAD *thread);
long tl_do_lao_tele(MK3THREAD *thread);
/* One of five button tables in __DATA, all exactly 0x28 apart -- bt_null,
 * bt_angle_jump, bt_duck, bt_stance, bt_jump. The size is the spacing between
 * consecutive symbols; the element type is not established, so it is declared
 * as bytes. */
extern uint8_t bt_jump[0x28];
void player_swpal(MK3OBJ *obj, uint32_t frozen);
void do_first_a9_frame(MK3OBJ *obj);
void find_last_frame(MK3OBJ *obj);
long tl_do_jade_prop(MK3THREAD *thread);
long t_animate_a9(MK3THREAD *thread);
long tl_do_mileena_tele(MK3THREAD *thread);
void damage_to_him(MK3OBJ *obj);
void takeover_him(MK3OBJ *obj);
long t_r_quake(MK3THREAD *thread);
long tl_do_sg_quake(MK3THREAD *thread);
void player_froze_pal(MK3OBJ *obj);
long is_he_joy(MK3OBJ *obj);
long t_decoy_proc(MK3THREAD *thread);
long tl_do_sz_decoy(MK3THREAD *thread);
long tl_do_stick_sweep(MK3THREAD *thread);
long is_stick_right(MK3OBJ *obj);
long is_stick_left(MK3OBJ *obj);
long is_stick_up(MK3OBJ *obj);
void init_special(MK3OBJ *obj);
void set_noflip(MK3OBJ *obj);
long tl_kano_cannon_ball(MK3THREAD *thread);
void check_block_bit(MK3OBJ *obj);
void clear_shadow_bit(MK3OBJ *obj);
long tl_ind_charge(MK3THREAD *thread);
long get_rough_hypotenuse(MK3OBJ *obj);
/* Read from the binary in joy.c: nothing is deliberately left in r0.
 * The `long` this used to be declared as came from a call site that
 * ignored the result. */
void get_x_dist(MK3OBJ *obj);
long tl_do_tusk_blur(MK3THREAD *thread);
long t_shake_and_collision(MK3THREAD *thread);
long tl_do_tele_explode(MK3THREAD *thread);
void set_ignore_y(MK3OBJ *obj);
long tl_do_lia_fly(MK3THREAD *thread);
long t_mileena_teleport_call(MK3THREAD *thread);
void find_ani_part_a14(MK3OBJ *obj);
long t_pounce_jsrp(MK3THREAD *thread);
void is_jade_protected(MK3OBJ *obj);   /* answers in 0x5c; it was declared
                                        * `long` here on the theory that q_yes
                                        * and q_no forward a value, and they do
                                        * not -- both are eight bytes that write
                                        * 0x5c and return. Corrected when the
                                        * definition was read in mkzap.c. */
void is_he_blocking(MK3OBJ *obj);
void disable_his_buttons(MK3OBJ *obj);
void xfer_otherguy(MK3OBJ *obj);
void highest_mpart_ob(MK3OBJ *out, MK3OBJ *src);
void lowest_mpart_ob(MK3OBJ *out, MK3OBJ *src);
void leftmost_mpart_ob(MK3OBJ *out, MK3OBJ *src);
void rightmost_mpart_ob(MK3OBJ *out, MK3OBJ *src);
long t_r_decoy_freeze(MK3THREAD *thread);
void decoy_collision_check(MK3OBJ *obj);
void distance_from_ground(MK3OBJ *obj);
long t_hover_sleep_1(MK3THREAD *thread);
long t_settle_down(MK3THREAD *thread);
void flip_multi(MK3OBJ *obj);
void update_tsl(MK3OBJ *obj);
void set_noedge(MK3OBJ *obj);
MK3OBJ *NewThreadProc(MK3OBJ *obj, MK3THREADFUNC fn);
void clear_inviso(MK3OBJ *obj);
void clear_noedge(MK3OBJ *obj);
long t_sctele_calla_2(MK3THREAD *thread);
long t_bike_call(MK3THREAD *thread);
void set_nocol(MK3OBJ *obj);
long t_animate2_a9(MK3THREAD *thread);
long tl_bike3(MK3THREAD *thread);
void reset_proc_stack(MK3THREAD *thread);
long t_pounce_fall(MK3THREAD *thread);
long t_reptile_dash_hit(MK3THREAD *thread);
void distance_off_ground(MK3OBJ *obj);
void towards_x_vel(MK3OBJ *obj);
void shake_a11(MK3OBJ *obj);
long t_mframew(MK3THREAD *thread);
void group_sound(MK3OBJ *obj);
long t_flight_call(MK3THREAD *thread);
void get_char_ani(MK3OBJ *obj);
long t_jump_up_land_jump(MK3THREAD *thread);
void ground_player(MK3OBJ *obj);
void clear_nocol(MK3OBJ *obj);

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

void away_x_vel(MK3OBJ *obj);
void pose_a9_manual(MK3OBJ *obj);
void stop_me_player(MK3OBJ *obj);
void set_x_vel_player(MK3OBJ *obj);
void stop_him(MK3OBJ *obj);
void ground_him(MK3OBJ *obj);
void get_char_ani2(MK3OBJ *obj);
void init_anirate(MK3OBJ *obj);

/* t_liz_fly_hit -- armv7 0x0003f280, 124 bytes.  **Complete.**
 *
 *      token == 0:
 *          stop_me_player(obj)
 *          obj->field40 = 0x20018
 *          pose_a9_manual(obj)
 *          park(token 0x6f5, duration 0xa)
 *      token == 0x6f5:
 *          obj->field1c = 0x20000
 *          away_x_vel(obj)
 *          frame[frame].handler = t_square3
 *      otherwise:  return -3
 */
long t_liz_fly_hit(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        stop_me_player(obj);
        obj->field40 = 0x20018;
        pose_a9_manual(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x6f5;
        thread->fieldfc = 0xa;
        return 0xa;
    }

    if (token != 0x6f5)
        return -3;

    obj->field1c = 0x20000;
    away_x_vel(obj);
    return mk3_install(thread, (MK3THREADFUNC)t_square3);
}

/* --------------------------------------------------------------------
 * Added by a later sweep -- tools/sweep.py, running the same
 * readers again after one of them learned something. Each still
 * refuses anything it cannot account for instruction by
 * instruction; see tools/pushfn.py and tools/leaffn.py.
 * -------------------------------------------------------------------- */

long t_flight(MK3THREAD *thread);
long t_jump_up_land_jsrp(MK3THREAD *thread);

/* t_air_grab_cancel -- armv7 0x0003d76c, 152 bytes.  **Complete.**
 *
 *      token == 0:
 *          obj->field40 = 0x19
 *          pose_a9_manual(obj)
 *          obj->field1c = 0xd
 *          obj->field20 = 0xd
 *          obj->field24 = 0x8000
 *          obj->field28 = 0xfff
 *          token := 0x83f, then descend into t_flight
 *      token == 0x83f:
 *          frame[frame].handler = t_jump_up_land_jsrp
 *      otherwise:  return -3
 */
long t_air_grab_cancel(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field40 = 0x19;
        pose_a9_manual(obj);
        obj->field1c = 0xd;
        obj->field20 = 0xd;
        obj->field24 = 0x8000;
        obj->field28 = 0xfff;
        *mk3_frame(thread, thread->frame + 1) = 0x83f;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x83f)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_jump_up_land_jsrp);
}


/* ------------------------------------------------------------ set_his_p_action
 *
 * armv7 0x0003c0dc, 16 bytes.  **Complete.**
 *
 *      obj->field00->field00->field00->field18 = 0x61a
 *
 * **Three dereferences to reach across to the other fighter.** The chain is
 * my PROC, then HIS object out of its 0x00, then HIS PROC out of that -- and
 * 0x18 on a PROC is the action `get_his_action` reads. So this reaches over
 * and writes what the opponent is doing, without either side holding a
 * pointer to the other's PROC directly.
 *
 * The name says it plainly, and the three `ldr r0, [r0]` in a row are the
 * shortest thing in this file.
 */
void set_his_p_action(MK3OBJ *obj)
{
    obj->field00->field00->field00->field18 = 0x61a;
}


/* ====================================== jade_prop_damping, zoom_damping
 *
 * armv7 0x0003c970 and 0x0003c958, 24 bytes each.  **Complete.**
 *
 *      v = obj->field08->field18
 *      obj->field20 = v >> N
 *      obj->field1c = v - (v >> N)
 *      set_x_vel_player(obj)
 *
 * One function with one number changed: **N is 3 for jade and 6 for zoom**, so
 * one keeps seven eighths of the speed and the other sixty-three sixty-fourths.
 * Both split the value the other object carries at 0x18 into a part that goes
 * to 0x20 and the remainder to 0x1c, then hand it to the velocity setter.
 *
 * The shift is `asrs`, arithmetic, so a negative speed damps toward zero from
 * below rather than wrapping. The remainder is `rsb r3, r3, r2` -- reverse
 * subtract, which computes `v - (v >> N)` in one instruction without needing
 * the operands the other way round.
 */
void jade_prop_damping(MK3OBJ *obj)
{
    int32_t v = (int32_t)obj->field08->field18;

    obj->field20 = (uint32_t)(v >> 3);          /* an eighth */
    obj->field1c = (uint32_t)(v - (v >> 3));    /* and the rest */
    set_x_vel_player(obj);
}

void zoom_damping(MK3OBJ *obj)
{
    int32_t v = (int32_t)obj->field08->field18;

    obj->field20 = (uint32_t)(v >> 6);          /* a sixty-fourth */
    obj->field1c = (uint32_t)(v - (v >> 6));    /* and the rest */
    set_x_vel_player(obj);
}


/* ---------------------------------------------------------- blur_blocked_setup
 *
 * armv7 0x0003fde4, 28 bytes.  **Complete.**
 *
 *      obj->field20         = 0x61f
 *      obj->field00->field18 = 0x61f
 *      set_no_block(obj)
 *      stop_me_player(obj)
 *
 * The same constant into two places: the object's own 0x20 and the action on
 * its PROC. Writing an action and a copy of it side by side is the pattern
 * this directory uses when something has to be visible both to the thread
 * that set it and to whatever reads the PROC.
 *
 * 0x61f is one past `set_his_p_action`'s 0x61a, which puts both in the same
 * run of action numbers.
 */
void blur_blocked_setup(MK3OBJ *obj)
{
    obj->field20          = 0x61f;
    obj->field00->field18 = 0x61f;

    set_no_block(obj);
    stop_me_player(obj);
}


/* ----------------------------------------------------------- pounce_ground_him
 *
 * armv7 0x0003f610, 32 bytes.  **Complete.**
 *
 *      stop_him(obj)
 *      him = obj->field00->him
 *      obj->field1c = him
 *      *(uint16_t *)((char *)him + 0x0e) =
 *          *(uint16_t *)((char *)obj->field08 + 0x0e)
 *      ground_him(obj)
 *
 * **It moves him to somewhere else's position.** The halfword at +0x0e is the
 * high half of 0x0c, the integer part of a coordinate, and it is copied out of
 * the object at 0x08 and into the opponent -- so whatever 0x08 is pointing at
 * when this runs is where he ends up. Then he is stopped and grounded.
 *
 * The opponent is loaded twice from PROC+0x04 rather than kept in a register,
 * which is the compiler not trusting `stop_him` to have left it alone.
 */
void pounce_ground_him(MK3OBJ *obj)
{
    MK3OBJ *him;

    stop_him(obj);

    him = (MK3OBJ *)(uintptr_t)obj->field00->him;
    obj->field1c = (uint32_t)(uintptr_t)him;

    *(uint16_t *)((char *)him + 0x0e) =
        *(uint16_t *)((char *)obj->field08 + 0x0e);

    ground_him(obj);
}


/* --------------------------------------------------------------- set_float_ani
 *
 * armv7 0x0003cef4, 48 bytes.  **Complete.**
 *
 *      n = obj->field1c                    ; the caller's index, kept
 *      *(uint32_t *)((char *)obj->field00 + 0x30) = n
 *      obj->field1c = n + 2
 *      obj->field40 = n + 2
 *      get_char_ani2(obj)
 *      obj->field1c = float_ani_speeds[n]  ; 0x00166f38
 *      init_anirate(obj)
 *
 * 0x1c is used three times for three different things in eight instructions:
 * it arrives as an index, becomes `index + 2` for the animation lookup, and
 * ends as the rate. The original index survives in r5 across all of it, which
 * is the only reason the table lookup at the end still has it.
 *
 * `_float_ani_speeds` reads 6, 3, 3, 3, 3 in its first five entries. **How
 * many entries it has is not established**: the words after those are six
 * figures long, which is either a different table beginning or a different
 * kind of value, and nothing here says which. Only the five are used by
 * anything read so far.
 */
extern uint32_t float_ani_speeds[];         /* 0x00166f38 */

void set_float_ani(MK3OBJ *obj)
{
    uint32_t n = obj->field1c;

    /* PROC+0x30 falls inside the unnamed run at 0x2c, so it is written by
     * offset rather than given a field the struct has not established. */
    *(uint32_t *)((char *)obj->field00 + 0x30) = n;

    obj->field1c = n + 2;
    obj->field40 = n + 2;
    get_char_ani2(obj);

    obj->field1c = float_ani_speeds[n];
    init_anirate(obj);
}


/* ---------------------------------------------------------------- accelerate_x
 *
 * armv7 0x0003c920, 56 bytes.  **Complete.**
 *
 *      step = obj->field48
 *      obj->field1c = step
 *      v = obj->field08->field18 + step
 *      obj->field20 = v
 *      obj->field24 = -0x80000
 *      if (v <= -0x80000)      obj->field20 = -0x80000
 *      else if (v >= 0x80000)  obj->field20 =  0x80000
 *      obj->field1c = obj->field20
 *      set_x_vel_player(obj)
 *
 * **The speed is clamped to plus or minus 0x80000**, and the two bounds are
 * reached differently: the low one is a pool literal (0xfff80000) that is also
 * parked in 0x24 on the way past, and the high one is an immediate built by
 * `movge.w`. Same magnitude, two spellings.
 *
 * 0x80000 is 8.0 in the 16.16 fixed point this directory uses for velocity, so
 * nothing accelerated through here goes faster than eight units a frame in
 * either direction.
 *
 * The clamp is written as an if/else so the high test only runs when the low
 * one did not fire -- with a symmetric range that is the same as clamping
 * both ways, but it is what the branches do.
 */
void accelerate_x(MK3OBJ *obj)
{
    int32_t step = (int32_t)obj->field48;
    int32_t v;

    obj->field1c = (uint32_t)step;
    v = (int32_t)obj->field08->field18 + step;

    obj->field20 = (uint32_t)v;
    obj->field24 = (uint32_t)(-0x80000);        /* parked on the way past */

    if (v <= -0x80000)
        obj->field20 = (uint32_t)(-0x80000);
    else if (v >= 0x80000)
        obj->field20 = 0x80000;

    obj->field1c = obj->field20;
    set_x_vel_player(obj);
}


/* --------------------------------------------------------------- t_biked_suspend
 *
 * armv7 0x0003fa2c, 80 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      inc_p_hit(obj)
 *      obj->field1c          = 0x20d
 *      *(uint32_t *)((char *)obj->field00 + 0x48) = 0x20d
 *      frame[frame].handler = t_suspend_wait_action
 *      frame[frame+1].w0 = 0
 *
 * A hit is counted, the same number goes to the object's 0x1c and to the
 * PROC's 0x48, and then the thread hands over to the routine that waits for
 * an action. Writing one value to a slot on each struct is how this directory
 * makes something visible both to the thread that set it and to whatever
 * reads the PROC.
 */
long t_biked_suspend(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    inc_p_hit(obj);

    obj->field1c          = 0x20d;
    /* PROC+0x48 falls in the unnamed run there; written by offset rather
     * than given a field one store is not enough to establish. */
    *(uint32_t *)((char *)obj->field00 + 0x48) = 0x20d;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_suspend_wait_action);
}


/* -------------------------------------------------------------- tl_do_ermac_tele
 *
 * armv7 0x00040a48, 88 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      am_i_airborn(obj)
 *      if (obj->field5c == 0) {        ; on the ground
 *          obj->field1c = 0x1e
 *          create_fx(obj)
 *      }
 *      set_inviso(obj)
 *      frame[frame].handler = tl_do_scorp_tele
 *      frame[frame+1].w0 = 0
 *
 * **Ermac's teleport ends by installing SCORPION's.** The two characters share
 * the rest of the move; what is different is only this preamble, and even that
 * is one conditional effect. Reached through a direct pc-relative address
 * rather than a pointer slot, so `tl_do_scorp_tele` is in this same file.
 *
 * The effect only spawns when the fighter is on the ground: `am_i_airborn`
 * answers into 0x5c and a zero there takes the branch. The 0x1e is derived
 * from that same zero with `adds r3, #0x1e` -- the register already held the
 * answer, so the constant costs no load.
 */
long tl_do_ermac_tele(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    am_i_airborn(obj);

    if (obj->field5c == 0) {            /* on the ground: leave a puff */
        obj->field1c = 0x1e;            /* adds r3, #0x1e on the zero */
        create_fx(obj);
    }

    set_inviso(obj);

    return mk3_push_handler(thread, (MK3THREADFUNC)tl_do_scorp_tele);
}


/* -------------------------------------------------------------- t_bike_scan_call
 *
 * armv7 0x0003c0ec, 92 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 3
 *      *(uint16_t *)(G + 0x456) = 3
 *      if (thread->frame > 0) { thread->frame -= 1; return 0 }
 *      frame[frame].handler = t_local_reaction_exit
 *      frame[frame+1].w0 = 0
 *
 * Three into the object's 0x1c and into the halfword at G+0x456 -- the same
 * global slot the `sans_repell` family writes, and the same constant into
 * both, held in `ip` across the pair.
 *
 * Then the return-up shape: the frame index is DECREMENTED to go back to the
 * caller, and the handler is only installed when there is nowhere to go back
 * to. `frame` is signed here -- `cmp #0` then `ble`, not `cbz`.
 */
long t_bike_scan_call(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 3;
    *(uint16_t *)(G_BYTES + 0x456) = 3;

    if ((long)thread->frame > 0) {      /* cmp #0 / ble: signed */
        thread->frame -= 1;             /* back up a level */
        return 0;
    }

    return mk3_push_handler(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* ------------------------------------------------------------- t_do_body_propell
 *
 * armv7 0x0003c148, 92 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      n = obj->field1c
 *      if (n <= 0x1c) { h = propell_table[n]; obj->field1c = h }
 *      else           { h = t_local_reaction_exit }
 *      frame[frame].handler = h
 *      frame[frame+1].w0 = 0
 *
 * **This is where every special move is dispatched from.** 0x1c arrives as a
 * move number, `_propell_table` at 0x00166f4c turns it into a handler, and the
 * thread hands over to it. Out of range falls back to the reaction exit, so a
 * bad number ends the move instead of jumping somewhere.
 *
 * The chosen handler is written back into 0x1c as well as installed -- the
 * same address in both places -- and only on the in-range path.
 *
 * The bound is `bhi` on 0x1c, so **twenty-nine entries, 0 through 0x1c**, and
 * every one of them is a named symbol:
 *
 *       0 tl_kano_cannon_ball     10 tl_do_tele_explode     20 tl_do_sg_quake
 *       1 tl_sonya_bike_kick      11 tl_do_lia_stay_afloat  21 tl_do_ninja_slide
 *       2 tl_ind_charge           12 tl_do_square_wave      22 tl_do_scorp_tele
 *       3 tl_jax_dash_punch       13 tl_do_lk_bike          23 tl_do_reptile_dash
 *       4 tl_do_sz_decoy          14 tl_do_super_kang       24 tl_do_jade_prop
 *       5 tl_do_lia_fly           15 tl_do_sg_pounce        25 tl_do_mileena_tele
 *       6 tl_do_lao_tele          16 tl_do_slide            26 tl_do_mileena_prop
 *       7 tl_do_lao_angle         17 tl_do_swat_zoom        27 tl_do_ermac_tele
 *       8 tl_do_robo_tele         18 tl_do_stick_sweep      28 tl_do_kano_upball
 *       9 tl_do_robo_air_grab     19 tl_do_tusk_blur
 *
 * That is the catalogue of the dashes, teleports and charges, and it is worth
 * reading next to `tl_do_ermac_tele` a few functions up: entry 27 ends by
 * installing entry 22, so Ermac's teleport is Scorpion's with a preamble.
 */
extern MK3THREADFUNC propell_table[0x1d];   /* 0x00166f4c */

long t_do_body_propell(MK3THREAD *thread)
{
    MK3OBJ       *obj = (MK3OBJ *)thread->proc;
    MK3THREADFUNC h;
    uint32_t      n;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    n = obj->field1c;
    if (n <= 0x1c) {
        h = propell_table[n];
        obj->field1c = (uint32_t)(uintptr_t)h;
    } else {
        h = (MK3THREADFUNC)t_local_reaction_exit;
    }

    return mk3_push_handler(thread, h);
}


/* ------------------------------------------------------------ t_upball_x_damping
 *
 * armv7 0x0003c988, 96 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      v = obj->field08->field18
 *      obj->field20 = v >> 3
 *      obj->field1c = v - (v >> 3)
 *      set_x_vel_player(obj)
 *      if (thread->frame > 0) { thread->frame -= 1; return 0 }
 *      frame[frame].handler = t_local_reaction_exit
 *
 * `jade_prop_damping` -- the same seven-eighths damping, same `asrs #3` and
 * same `rsb` for the remainder -- wrapped in the return-up shape, so it runs
 * once per frame while the thread stays where it is and only ends when there
 * is no caller left to return to.
 *
 * Three routines in this file now damp the same way with different shifts:
 * 3 here and in `jade_prop_damping`, 6 in `zoom_damping`.
 */
long t_upball_x_damping(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    int32_t v;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    v = (int32_t)obj->field08->field18;
    obj->field20 = (uint32_t)(v >> 3);
    obj->field1c = (uint32_t)(v - (v >> 3));
    set_x_vel_player(obj);

    if ((long)thread->frame > 0) {
        thread->frame -= 1;
        return 0;
    }

    return mk3_push_handler(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* --------------------------------------------------------------- teleport_next_to
 *
 * armv7 0x0003e534, 112 bytes.  **Complete.**
 *
 *      obj->field40 = 0x54
 *      him  = obj->field00->him
 *      obj->field1c = him
 *      hisx = (int16_t)him[+0x0e]
 *      obj->field28 = hisx
 *
 *      obj->field20 = |(*(long *)(G + 0xb4) + 0x18f) - hisx|
 *      if (obj->field20 > 0x80) {
 *          obj->field20 = |*(long *)(G + 0xb0) - hisx|
 *          if (obj->field20 > 0x80)
 *              obj->field40 = -obj->field40
 *      }
 *
 *      match_me_with_him(obj)
 *      obj->field20 = 0
 *      obj->field1c = obj->field40
 *      multi_adjust_xy(obj)
 *
 * **Which side of the opponent to land on.** 0x54 is the offset, positive by
 * default, and it is negated only when he is far from BOTH bounds in the
 * global state -- more than 0x80 from `G + 0xb4` plus 0x18f, and more than
 * 0x80 from `G + 0xb0`. Near either one the default side stands, which is what
 * keeps a teleport from putting the attacker through a wall.
 *
 * Both distances are made positive the same way, with `rsblt` in an IT block
 * rather than a branch, and both are compared against the same 0x80.
 *
 * The 0x18f is built as `+0x18c` then `+3`, two instructions for one constant,
 * because 0x18f is not a single Thumb immediate.
 */
void teleport_next_to(MK3OBJ *obj)
{
    MK3OBJ *him;
    int32_t hisx, d;

    obj->field40 = 0x54;

    him = (MK3OBJ *)(uintptr_t)obj->field00->him;
    obj->field1c = (uint32_t)(uintptr_t)him;

    hisx = *(int16_t *)((char *)him + 0x0e);
    obj->field28 = (uint32_t)hisx;

    d = (*(int32_t *)(G_BYTES + 0xb4) + 0x18f) - hisx;
    obj->field20 = (uint32_t)(d < 0 ? -d : d);

    if ((int32_t)obj->field20 > 0x80) {
        d = *(int32_t *)(G_BYTES + 0xb0) - hisx;
        obj->field20 = (uint32_t)(d < 0 ? -d : d);

        if ((int32_t)obj->field20 > 0x80)
            obj->field40 = (uint32_t)(-(int32_t)obj->field40);
    }

    match_me_with_him(obj);

    obj->field20 = 0;
    obj->field1c = obj->field40;
    multi_adjust_xy(obj);
}


/* ----------------------------------------------------------------- t_tele_scan2
 *
 * armv7 0x0003c07c, 96 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      v = obj->field08->field1c
 *      obj->field1c = v
 *      if (v < 0) {
 *          frame[frame].handler = t_tele_scan
 *      } else if (thread->frame > 0) {
 *          thread->frame -= 1
 *          return 0
 *      } else {
 *          frame[frame].handler = t_local_reaction_exit
 *      }
 *      frame[frame+1].w0 = 0
 *
 * **A sign test picks between going back and going round again.** A negative
 * value at the other object's 0x1c installs `t_tele_scan` -- the scan runs
 * another pass -- and anything else returns up a level, or ends if there is
 * no level to return to.
 *
 * `t_tele_scan` is reached as a direct pc-relative address rather than through
 * a pointer slot, so it is in this file; `t_local_reaction_exit` comes through
 * the slot at 0x000f3708, so it is not.
 */
long t_tele_scan2(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    int32_t v;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    v = (int32_t)obj->field08->field1c;
    obj->field1c = (uint32_t)v;

    if (v >= 0) {
        if ((long)thread->frame > 0) {
            thread->frame -= 1;         /* back up a level */
            return 0;
        }
        return mk3_push_handler(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }

    return mk3_push_handler(thread, (MK3THREADFUNC)t_tele_scan);
}


/* --------------------------------------------------------------- flight_move_ani
 *
 * armv7 0x0003cf24, 112 bytes.  **Complete.**
 *
 *      obj->field1c = |obj->field08->field18|
 *      if (obj->field1c < 0x60000) return
 *      m = obj->field00[+0x30]
 *      obj->field20 = m
 *      if (m == 1 || m == 2) return
 *      obj->field1c = 1
 *      obj->field34 = (obj->field48 < 0) ? is_he_left : is_he_right
 *      ((void (*)(MK3OBJ *))obj->field34)(obj)
 *      if (obj->field5c == 0) obj->field1c = 2
 *      saved = obj->field1c
 *      face_opponent(obj)
 *      obj->field1c = saved
 *      set_float_ani(obj)
 *
 * **A function pointer chosen by a sign, parked in 0x34 and called through.**
 * `is_he_left` and `is_he_right` are the same question asked two ways, and
 * which one runs is decided by the sign of 0x48 -- so the answer at 0x5c means
 * "is he on the side I am moving toward" rather than a fixed direction. Both
 * arrive through pointer slots, so neither is in this file.
 *
 * The two early exits are a speed floor and a mode filter: nothing happens
 * below 0x60000 -- six units in the 16.16 this directory uses -- and nothing
 * happens in modes 1 or 2, whatever PROC+0x30 counts.
 *
 * 0x1c is written four times with four meanings: a magnitude, then 1, then 2
 * on one branch, then restored around `face_opponent` because that routine
 * uses the slot itself. The save into r5 is the only reason the value survives.
 */
void flight_move_ani(MK3OBJ *obj)
{
    int32_t  v = (int32_t)obj->field08->field18;
    uint32_t m, saved;

    obj->field1c = (uint32_t)(v < 0 ? -v : v);
    if ((int32_t)obj->field1c < 0x60000)        /* a speed floor */
        return;

    m = *(uint32_t *)((char *)obj->field00 + 0x30);
    obj->field20 = m;
    if (m == 1 || m == 2)                       /* not in those modes */
        return;

    obj->field1c = 1;
    obj->field34 = (uint32_t)(uintptr_t)
                   (((int32_t)obj->field48 < 0) ? is_he_left : is_he_right);

    ((void (*)(MK3OBJ *))(uintptr_t)obj->field34)(obj);

    if (obj->field5c == 0)
        obj->field1c = 2;

    saved = obj->field1c;
    face_opponent(obj);                 /* which uses 0x1c itself */
    obj->field1c = saved;

    set_float_ani(obj);
}


/* ------------------------------------------------------------ tl_do_lia_stay_afloat
 *
 * armv7 0x0003df1c, 108 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      air_init_special(obj)
 *      obj->field40 = 2
 *      get_char_ani2(obj)
 *      find_part2(obj)
 *      obj->field40 += 0x18
 *      back_to_normal(obj)
 *      obj->field20          = 0x203
 *      obj->field00->field18 = 0x203
 *      frame[frame].handler = t_main_hover_loop
 *
 * Entry 11 of the propell table -- the one that keeps her in the air. The
 * animation is found in two steps: 2 into 0x40 and `get_char_ani2` resolves
 * it, then `find_part2` picks the part, then **0x18 is ADDED to whatever 0x40
 * holds afterwards**. So the second number is an offset from the resolved
 * animation, not an animation of its own.
 *
 * 0x203 goes to the object's 0x20 and to the PROC's 0x18, the action pair this
 * directory writes together so the value is visible from both structs.
 *
 * `t_main_hover_loop` is a direct pc-relative address, so it lives here.
 */
long tl_do_lia_stay_afloat(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    air_init_special(obj);

    obj->field40 = 2;
    get_char_ani2(obj);
    find_part2(obj);
    obj->field40 += 0x18;               /* an offset from what was resolved */

    back_to_normal(obj);

    obj->field20          = 0x203;
    obj->field00->field18 = 0x203;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_main_hover_loop);
}


/* --------------------------------------------------------------- tl_do_square_wave
 *
 * armv7 0x0003f3e4, 112 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field20 = 0x207
 *      init_special_act(obj)
 *      obj->field1c = 4
 *      ochar_sound(obj)
 *      obj->field40 = 0x16
 *      find_ani_last_frame(obj)
 *      do_next_a9_frame(obj)
 *      obj->field1c          = 0xfff40000
 *      obj->field08->field1c = 0xfff40000
 *      frame[frame].handler = t_lfly5
 *
 * Entry 12 of the propell table. **0xfff40000 is -12.0** in the 16.16 this
 * directory uses, written to the object's 0x1c and to the other object's, both
 * from one register -- so the launch speed is set on both sides in one go.
 *
 * The animation is started rather than chosen: 0x16 into 0x40,
 * `find_ani_last_frame` to place it, and `do_next_a9_frame` to run a frame
 * before the handler takes over.
 *
 * `t_lfly5` is a direct pc-relative address, so it is in this file.
 */
long tl_do_square_wave(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field20 = 0x207;
    init_special_act(obj);

    obj->field1c = 4;
    ochar_sound(obj);

    obj->field40 = 0x16;
    find_ani_last_frame(obj);
    do_next_a9_frame(obj);

    obj->field1c          = 0xfff40000u;    /* -12.0 in 16.16 */
    obj->field08->field1c = 0xfff40000u;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_lfly5);
}


/* ------------------------------------------------------------------ bike_hit_call
 *
 * armv7 0x0003d980, 124 bytes.  **Complete.**
 *
 *      obj->field1c = 3
 *      *(uint16_t *)(G + 0x456) = 3
 *      n = obj->field00[+0x2c] - 1
 *      obj->field1c = n
 *      if (n == 0) return
 *      obj->field00[+0x2c] = n
 *      match_him_with_me_f(obj)
 *      obj->field1c = 0xffe0ffd0
 *      adjust_him_a0(obj)
 *      ((MK3OBJ *)obj->a10)->field20 = obj->field08->field20
 *      ((MK3OBJ *)obj->a10)->field18 = obj->field08->field18
 *      ((MK3OBJ *)obj->a10)->field1c = obj->field08->field1c
 *      obj->field30 = obj->field40
 *      next_anirate(obj)
 *      if (obj->field40 != obj->field30) advance_him(obj)
 *
 * **A counter at PROC+0x2c that runs down and stops the call.** It is read,
 * decremented, and written back only if it did not reach zero -- so the last
 * hit leaves the counter at 1 rather than 0, and the routine simply returns
 * on the pass that would have taken it to zero.
 *
 * 0xffe0ffd0 is another **packed pair of signed halves**: -32 high and -48
 * low, the same shape `skinny_spawn` reads out of 0x48. Here it goes into 0x1c
 * for `adjust_him_a0` rather than being unpacked in place.
 *
 * Three fields are copied one at a time from the object at 0x08 to the one at
 * 0x44, each with its own pair of loads -- 0x20, 0x18 and 0x1c -- because the
 * compiler reloaded both pointers between every copy.
 *
 * The last test compares 0x40 against the 0x30 it was just copied into: if
 * `next_anirate` moved 0x40, the opponent is advanced too.
 */
void bike_hit_call(MK3OBJ *obj)
{
    uint32_t n;

    obj->field1c = 3;
    *(uint16_t *)(G_BYTES + 0x456) = 3;

    n = *(uint32_t *)((char *)obj->field00 + 0x2c) - 1;
    obj->field1c = n;
    if (n == 0) {
        /* Corrected, 2026-09-26: the pass that would zero the count does
         * not return -- it plays a sound and reloads the count to 7, then
         * carries on like every other pass. */
        rsnd_func(obj, 7);
        obj->field1c = 7;
    }

    *(uint32_t *)((char *)obj->field00 + 0x2c) = obj->field1c;

    match_him_with_me_f(obj);
    obj->field1c = 0xffe0ffd0u;         /* -32 high, -48 low */
    adjust_him_a0(obj);

    /* 0x44 is the argument slot, a word, and here it carries an object
     * address -- so the copies go through a cast rather than a field the
     * struct types as a pointer. */
    {
        MK3OBJ *dst = (MK3OBJ *)(uintptr_t)obj->a10;

        dst->field20 = obj->field08->field20;
        dst->field18 = obj->field08->field18;
        dst->field1c = obj->field08->field1c;
    }

    obj->field30 = obj->field40;
    next_anirate(obj);
    if (obj->field40 != obj->field30)
        advance_him(obj);
}


/* ----------------------------------------------------------------- t_s_t_scroller
 *
 * armv7 0x0003c3b8, 124 bytes.  **Complete.**
 *
 *      token == 0:       MKEvent_Add(1, 3, obj->field00->field08, 0)
 *                        (and on into the shared part)
 *      token == 0x297:   the shared part only
 *      otherwise:        return -3
 *
 *      shared:
 *          if (((MK3OBJ *)obj->field48)->field00->field18 == 0x215)
 *              park(token 0x297, duration 1)
 *          else {
 *              MKEvent_Add(1, 4, 0, 0)
 *              park(token 0x29c, duration 0x16462)
 *          }
 *
 * **A coroutine that re-parks with its own token until a state changes.**
 * 0x297 wakes it after one tick straight back into the shared part, so it
 * polls the field at +0x18 of whatever 0x48 points at, once a frame, for as
 * long as that reads 0x215. When it stops, the second event goes out and it
 * returns the 0x16462 delete sentinel with a token its own dispatch rejects -- the never-wake
 * ending this directory uses when a sequence must not unwind to its caller.
 *
 * The first event is only sent on the way in, not on the polling passes, which
 * is what the two entry points are for.
 */
long t_s_t_scroller(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token != 0 && token != 0x297)
        return -3;

    if (token == 0)
        MKEvent_Add(1, 3, (long)obj->field00->field08, 0);

    /* THREE loads in the binary, not two: `ldr r3, [r5, #0x48]` then
     * `ldr r3, [r3]` then `ldr r2, [r3, #0x18]`. So it is the PROC of
     * the object at 0x48, not that object's own 0x18 -- which is what
     * `tl_do_scorp_tele` writes 0x215 into when it starts this thread. */
    if (((MK3OBJ *)(uintptr_t)obj->field48)->field00->field18 == 0x215) {
        *mk3_frame(thread, thread->frame + 1) = 0x297;
        thread->fieldfc = 1;            /* look again next tick */
        return 1;
    }

    MKEvent_Add(1, 4, 0, 0);

    *mk3_frame(thread, thread->frame + 1) = 0x29c;
    thread->fieldfc = 0x16462;          /* and never wakes */
    return 0x16462;
}


/* ------------------------------------------------------------- t_pounce_adjust_him
 *
 * armv7 0x00040090, 136 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      is_he_airborn(obj)
 *      if (obj->field5c == 0) {
 *          pounce_ground_him(obj)          ; on the ground
 *      } else {
 *          stop_him(obj)                   ; in the air
 *          match_him_with_me(obj)
 *          obj->field1c = 0
 *          obj->field20 = 0x20
 *          adjust_him_xy(obj)
 *      }
 *      if (thread->frame > 0) { thread->frame -= 1; return 0 }
 *      frame[frame].handler = t_local_reaction_exit
 *
 * **Two ways to put the opponent where the pounce needs him**, chosen by
 * whether he is already off the ground. Grounded, the whole job is handed to
 * `pounce_ground_him`, which copies a position across and grounds him.
 * Airborne, he is stopped, matched, and moved by a fixed 0x20 through
 * `adjust_him_xy`.
 *
 * The zero into 0x1c is `str r6` -- the guard value the entry test already
 * proved is zero -- so the constant costs no instruction of its own. This
 * directory does that everywhere the guard's register is still live.
 *
 * Both branches then meet at the same ending, and the compiler emitted the
 * pointer-slot load for `t_local_reaction_exit` TWICE, once per path, rather
 * than sharing one. Same slot, 0x000f3708, both times.
 */
long t_pounce_adjust_him(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    is_he_airborn(obj);

    if (obj->field5c == 0) {
        pounce_ground_him(obj);         /* he is standing on it */
    } else {
        stop_him(obj);
        match_him_with_me(obj);
        obj->field1c = 0;               /* str r6: the guard's zero */
        obj->field20 = 0x20;
        adjust_him_xy(obj);
    }

    if ((long)thread->frame > 0) {
        thread->frame -= 1;             /* back up a level */
        return 0;
    }

    return mk3_push_handler(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* ---------------------------------------------------------------- t_lao_angle_scan
 *
 * armv7 0x0003c7e0, 136 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      w = *(uint32_t *)obj->field40       ; the animation's first word
 *      obj->field1c = w
 *      if (w == 0) {
 *          obj->field1c = 0x12
 *          strike_check_a0(obj)
 *          if (obj->field5c != 0) {
 *              frame[frame].handler = t_lao_angle_hit
 *              frame[frame+1].w0 = 0
 *              return 0
 *          }
 *      }
 *      if (thread->frame > 0) { thread->frame -= 1; return 0 }
 *      frame[frame].handler = t_local_reaction_exit
 *
 * **The scan only tests for a hit once the animation has run out.** 0x40 holds
 * a pointer here, not a number, and its first word being zero is what says the
 * frames are finished -- so the hat is checked against the opponent on the
 * frame the animation ends, and on every other frame the thread just returns
 * up a level.
 *
 * `strike_check_a0` answers into 0x5c, the same slot the `is_he_*` family uses.
 * A hit installs `t_lao_angle_hit`, reached as a direct pc-relative address so
 * it is in this file; a miss takes the same ending as an unfinished animation.
 *
 * The zero written to the slot above comes from r6, which still holds the word
 * the test proved was zero -- no constant is loaded for it.
 */
long t_lao_angle_scan(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t w;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    w = *(uint32_t *)(uintptr_t)obj->field40;
    obj->field1c = w;

    if (w == 0) {                       /* the animation has run out */
        obj->field1c = 0x12;
        strike_check_a0(obj);

        if (obj->field5c != 0)          /* and it connected */
            return mk3_push_handler(thread, (MK3THREADFUNC)t_lao_angle_hit);
    }

    if ((long)thread->frame > 0) {
        thread->frame -= 1;             /* back up a level */
        return 0;
    }

    return mk3_push_handler(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* ------------------------------------------------------------------------ t_blb8
 *
 * armv7 0x0003bf88, 140 bytes.  **Complete.**
 *
 *      token == 0:       obj->field1c = 4
 *                        obj->field24 = 4
 *                        obj->field20 = 3
 *                        token := 0x32c, then descend into t_shake_ob_up
 *      token == 0x32c:   frame[frame].handler = t_local_reaction_exit
 *      otherwise:        return -3
 *
 * The two-state shape: go down a level into the shake, and when the token
 * comes back, hand over to the exit.
 *
 * **The resume installs at the ORIGINAL frame index**, held in `ip` from the
 * first instruction of the function, rather than re-reading 0xa4. That is not
 * an optimisation -- it is the same index either way, because the descend
 * happened on a different entry and this one never touched it -- but it is
 * why the resume path has no second load.
 *
 * 4 goes to both 0x1c and 0x24 from one register, and 3 to 0x20; three stores,
 * two constants.
 */
long t_blb8(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token != 0) {
        if (token != 0x32c)
            return -3;
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }

    obj->field1c = 4;
    obj->field24 = 4;
    obj->field20 = 3;

    *mk3_frame(thread, thread->frame + 1) = 0x32c;
    thread->frame = thread->frame + 1;          /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_shake_ob_up;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------------- tl_do_robo_air_grab
 *
 * armv7 0x0003cf94, 152 bytes.  **Complete.**
 *
 *      token == 0:       obj->field20          = 0x209
 *                        obj->field00->field18 = 0x209
 *                        air_init_special(obj)
 *                        obj->field1c = 0x23
 *                        ochar_sound(obj)
 *                        obj->field40 = 6
 *                        find_ani2_part2(obj)
 *                        do_next_a9_frame(obj)
 *                        park(token 0x7ee, duration 3)
 *
 *      token == 0x7ee:   obj->field1c = 4
 *                        init_anirate(obj)
 *                        obj->a10     = obj->field00->him
 *                        obj->field48 = 6
 *                        frame[frame].handler = t_air_sleep3
 *
 *      otherwise:        return -3
 *
 * Entry 9 of the propell table. **It parks on one state and installs on the
 * other**, which is the mixed shape: three ticks of nothing after the grab
 * animation starts, then the opponent is written into the argument slot and
 * the thread hands over to the air sleep.
 *
 * 0x209 goes to the object's 0x20 and the PROC's 0x18 from one register, the
 * action pair this file writes together everywhere.
 *
 * The animation is found with `find_ani2_part2` -- the *2* variant, matching
 * the `get_char_ani2` family -- rather than the plain one used by the ground
 * moves.
 */
long tl_do_robo_air_grab(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token != 0) {
        if (token != 0x7ee)
            return -3;

        obj->field1c = 4;
        init_anirate(obj);

        obj->a10     = obj->field00->him;
        obj->field48 = 6;

        return mk3_install(thread, (MK3THREADFUNC)t_air_sleep3);
    }

    obj->field20          = 0x209;
    obj->field00->field18 = 0x209;
    air_init_special(obj);

    obj->field1c = 0x23;
    ochar_sound(obj);

    obj->field40 = 6;
    find_ani2_part2(obj);
    do_next_a9_frame(obj);

    *mk3_frame(thread, thread->frame + 1) = 0x7ee;
    thread->fieldfc = 3;
    return 3;
}


/* ----------------------------------------------------------------- t_lao_angle_hit
 *
 * armv7 0x0003cc30, 152 bytes.  **Complete.**
 *
 *      token == 0:       stop_me_player(obj)
 *                        if (obj->field18 != 0)
 *                            frame[frame].handler = t_lao_angle_blocked
 *                        else
 *                            park(token 0x8d1, duration 6)
 *
 *      token == 0x8d1:   obj->field24 = 0x8000
 *                        obj->field28 = 0xfff
 *                        obj->field1c = 0
 *                        obj->field20 = 0
 *                        frame[frame].handler = t_land_on_yer_feet
 *
 *      otherwise:        return -3
 *
 * The other half of `t_lao_angle_scan`, which installs this when the hat
 * connects. **0x18 decides whether it counted**: non-zero and the move goes
 * to the blocked routine straight away, zero and the thread waits six ticks
 * before setting up the landing.
 *
 * `t_lao_angle_blocked` is a direct pc-relative address, so it is in this
 * file; `t_land_on_yer_feet` comes through the slot at 0x000f3778, so it is
 * not.
 *
 * The two zeros on the resume path are `str r0` with r0 already zero from the
 * `movs r0, #0` that also feeds the slot clear -- one constant, three uses.
 */
long t_lao_angle_hit(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token != 0) {
        if (token != 0x8d1)
            return -3;

        obj->field24 = 0x8000;
        obj->field28 = 0xfff;
        obj->field1c = 0;
        obj->field20 = 0;

        return mk3_install(thread, (MK3THREADFUNC)t_land_on_yer_feet);
    }

    stop_me_player(obj);

    if (obj->field18 != 0)              /* he blocked it */
        return mk3_install(thread,
                                (MK3THREADFUNC)t_lao_angle_blocked);

    *mk3_frame(thread, thread->frame + 1) = 0x8d1;
    thread->fieldfc = 6;
    return 6;
}


/* ------------------------------------------------------------------- t_pounce_scan
 *
 * armv7 0x0003d224, 152 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      sans_repell_3(obj)
 *      obj->field1c = 0x12
 *      q_is_he_a_boss(obj)
 *      if (obj->field5c != 0)
 *          obj->field1c = 0x13         ; a boss gets a different box
 *      strike_check_a0(obj)
 *      if (obj->field5c != 0) {
 *          frame[frame].handler = t_pounce_hit
 *          return 0
 *      }
 *      if (thread->frame > 0) { thread->frame -= 1; return 0 }
 *      frame[frame].handler = t_local_reaction_exit
 *
 * **The strike box depends on who the opponent is.** 0x12 is written first,
 * `q_is_he_a_boss` answers into 0x5c, and a boss replaces it with 0x13 before
 * the check runs. So the pounce reaches a boss differently from anyone else,
 * and it is one number's difference.
 *
 * 0x5c carries two unrelated answers in six instructions -- first whether he
 * is a boss, then whether the strike connected -- because both routines write
 * their result there. Nothing saves the first one, and nothing needs to.
 *
 * `t_pounce_hit` is a direct pc-relative address, so it is in this file.
 */
long t_pounce_scan(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    sans_repell_3(obj);

    obj->field1c = 0x12;
    q_is_he_a_boss(obj);
    if (obj->field5c != 0) {
        obj->field1c = 0x13;            /* a boss gets its own box */
        strike_check_a0(obj);           /* one call per branch, as the binary has it */
    } else {
        strike_check_a0(obj);
    }
    if (obj->field5c != 0)
        return mk3_install(thread, (MK3THREADFUNC)t_pounce_hit);

    if ((long)thread->frame > 0) {
        thread->frame -= 1;             /* back up a level */
        return 0;
    }

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* ----------------------------------------------------------------------- t_lfly4
 *
 * armv7 0x0003d02c, 160 bytes.  **Complete.**
 *
 *      token == 0:       park(token 0x6dc, duration 1)
 *
 *      token == 0x6dc:   next_anirate(obj)
 *                        obj->field1c = 0x11
 *                        strike_check_a0(obj)
 *                        if (obj->field5c != 0) {
 *                            frame[frame].handler = t_liz_fly_hit
 *                            return 0
 *                        }
 *                        if (--obj->field48 > 0)
 *                            park(token 0x6dc, duration 1)
 *                        else
 *                            frame[frame].handler = t_square3
 *
 *      otherwise:        return -3
 *
 * **A one-tick poll with a countdown.** The flight checks for a strike once a
 * frame and re-parks with its own token, so 0x6dc is both what it waits on and
 * what it wakes into. 0x48 is the number of frames left; it is decremented and
 * written back every pass, and when it runs out the thread hands over to
 * `t_square3` instead.
 *
 * Entry and re-park share one piece of code -- the `beq` from the guard lands
 * on the same three instructions the countdown falls into -- which is why the
 * first tick and every later one cost the same.
 *
 * Both handlers are direct pc-relative addresses, so both are in this file.
 */
long t_lfly4(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token != 0) {
        if (token != 0x6dc)
            return -3;

        next_anirate(obj);

        obj->field1c = 0x11;
        strike_check_a0(obj);
        if (obj->field5c != 0)
            return mk3_install(thread, (MK3THREADFUNC)t_liz_fly_hit);

        obj->field48 -= 1;
        if ((int32_t)obj->field48 <= 0)         /* out of frames */
            return mk3_install(thread, (MK3THREADFUNC)t_square3);
    }

    *mk3_frame(thread, thread->frame + 1) = 0x6dc;
    thread->fieldfc = 1;                        /* look again next tick */
    return 1;
}


/* --------------------------------------------------------------- t_cyrax_implode
 *
 * armv7 0x0003d4f8, 164 bytes.  **Complete.**
 *
 *      token == 0:       obj->field08[+0x2c] = 0x422
 *                        ground_player(obj)
 *                        obj->field48 = obj->field1c
 *                        obj->field1c = 9
 *                        create_fx(obj)
 *                        park(token 0x722, duration 0xa)
 *
 *      token == 0x722:   clear_nocol(obj)
 *                        park(token 0x724, duration 9)
 *
 *      token == 0x724:   if (thread->frame > 0) { thread->frame -= 1; return 0 }
 *                        frame[frame].handler = t_local_reaction_exit
 *
 *      otherwise:        return -3
 *
 * **Three states, not two.** Every other coroutine read in this directory so
 * far has an entry and one resume; this one runs 0 -> 0x722 -> 0x724 and only
 * then returns up a level. `tools/parkfn.py` models two states and refuses
 * anything with a second token check, which is why routines like this stay
 * hand-read.
 *
 * The timing is ten ticks then nine: the effect is spawned and left alone for
 * ten, collision is cleared and left for nine more, and the move ends.
 *
 * 0x1c is saved into 0x48 before being overwritten with the effect number, so
 * whatever the caller left there survives the implosion.
 *
 * The frame index is read ONCE at the top into r1 and reused on every path,
 * including both parks -- so all three states address the same slot without
 * re-reading 0xa4, which the two-state routines do not bother to avoid.
 */
long t_cyrax_implode(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x722) {
        clear_nocol(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x724;
        thread->fieldfc = 9;
        return 9;
    }

    if (token == 0x724) {
        if ((long)thread->frame > 0) {
            thread->frame -= 1;         /* back up a level */
            return 0;
        }
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }

    if (token != 0)
        return -3;

    *(uint32_t *)((char *)obj->field08 + 0x2c) = 0x422;
    ground_player(obj);

    obj->field48 = obj->field1c;        /* kept across the effect */
    obj->field1c = 9;
    create_fx(obj);

    *mk3_frame(thread, thread->frame + 1) = 0x722;
    thread->fieldfc = 0xa;
    return 0xa;
}


/* --------------------------------------------------------------------- t_square3
 *
 * armv7 0x0003d804, 164 bytes.  **Complete.**
 *
 *      token == 0:       obj->field40 = 0x10016
 *                        pose_a9_manual(obj)
 *                        face_opponent(obj)
 *                        obj->field1c = 0
 *                        obj->field20 = 0x20000
 *                        obj->field24 = 0x8000
 *                        obj->field28 = 0xfff
 *                        token := 0x6ee, then descend into t_flight
 *
 *      token == 0x6ee:   frame[frame].handler = t_jump_up_land_jsrp
 *
 *      otherwise:        return -3
 *
 * The end of the square wave: pose, face the opponent, set a velocity and go
 * down a level into the shared flight routine; when that returns, land.
 *
 * **0x8000 is derived from 0x20000 by subtraction** -- `sub.w r3, r3, #0x18000`
 * on the value already in the register -- so 2.0 and 0.5 in the 16.16 this
 * directory uses for velocity cost one literal between them.
 *
 * 0x10016 into 0x40 is the packed shape 0x40 takes when it is a number rather
 * than a pointer: 1 in the high half and 0x16 in the low.
 *
 * `t_flight` and `t_jump_up_land_jsrp` both arrive through pointer slots, so
 * neither is in this file. `t_flight` is the routine in other.c that pushes a
 * level and keeps its place -- this is one of its callers.
 */
long t_square3(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token != 0) {
        if (token != 0x6ee)
            return -3;
        return mk3_install(thread,
                                (MK3THREADFUNC)t_jump_up_land_jsrp);
    }

    obj->field40 = 0x10016;             /* 1 high, 0x16 low */
    pose_a9_manual(obj);
    face_opponent(obj);

    obj->field1c = 0;
    obj->field20 = 0x20000;             /* 2.0 */
    obj->field24 = 0x8000;              /* 0.5, reached by subtraction */
    obj->field28 = 0xfff;

    *mk3_frame(thread, thread->frame + 1) = 0x6ee;
    thread->frame = thread->frame + 1;          /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_flight;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------------- t_lao_angle_blocked
 *
 * armv7 0x0003c1a4, 172 bytes.  **Complete.**
 *
 *      token == 0:       obj->field1c          = 0x60f
 *                        obj->field00->field18 = 0x60f
 *                        obj->field40 = 0x1a
 *                        get_char_ani(obj)
 *                        obj->field40 += 4
 *                        obj->field1c = 0x4000
 *                        obj->field20 = 0xfff90000
 *                        obj->field24 = 0x6000
 *                        obj->field28 = 3
 *                        token := 0x8c5, then descend into t_flight
 *
 *      token == 0x8c5:   frame[frame].handler = t_jump_up_land_jump
 *
 *      otherwise:        return -3
 *
 * What happens when the hat is blocked: the action pair goes to 0x60f, an
 * animation is resolved and then offset by 4, a velocity is set, and the
 * thread descends into the shared flight routine. When that comes back it
 * lands in a jump rather than on its feet -- `t_jump_up_land_jump`, where
 * `t_lao_angle_hit` used `t_land_on_yer_feet`. Blocked and connected end
 * differently.
 *
 * **The three velocity components are one literal and two arithmetic steps**:
 * 0x4000, then `sub #0x74000` reaches 0xfff90000, then `add #0x76000` on THAT
 * reaches 0x6000. In the 16.16 this directory uses that is 0.25, -7.0 and
 * 0.38 -- three numbers with nothing in common, produced by a chain because
 * each fits an immediate where the value itself would need a pool entry.
 *
 * 0x40 is resolved by `get_char_ani` and then has 4 ADDED to it, the same
 * offset-from-the-resolved-animation shape `tl_do_lia_stay_afloat` uses with
 * 0x18.
 */
long t_lao_angle_blocked(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token != 0) {
        if (token != 0x8c5)
            return -3;
        return mk3_install(thread,
                                (MK3THREADFUNC)t_jump_up_land_jump);
    }

    obj->field1c          = 0x60f;
    obj->field00->field18 = 0x60f;

    obj->field40 = 0x1a;
    get_char_ani(obj);
    obj->field40 += 4;                  /* an offset from what was resolved */

    obj->field1c = 0x4000;              /*  0.25 */
    obj->field20 = 0xfff90000u;         /* -7.00, by sub #0x74000 */
    obj->field24 = 0x6000;              /*  0.38, by add #0x76000 on that */
    obj->field28 = 3;

    *mk3_frame(thread, thread->frame + 1) = 0x8c5;
    thread->frame = thread->frame + 1;          /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_flight;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------------------- t_tele_scan
 *
 * armv7 0x0003c868, 184 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      n = obj->field00->field28
 *      obj->field1c = n
 *      if (n == 0) {
 *          obj->field00->field18 = 0x622
 *          obj->field1c = 0x13
 *          strike_check_a0(obj)
 *          if (obj->field5c != 0) {
 *              obj->field00[+0x2c]   = obj->field18
 *              obj->field1c          = 1
 *              obj->field00->field28 = 1
 *          }
 *      }
 *      if (thread->frame > 0) { thread->frame -= 1; return 0 }
 *      frame[frame].handler = t_local_reaction_exit
 *
 * **PROC+0x28 is a once-only flag here.** The scan strikes only while it reads
 * zero, and sets it to 1 on a hit -- so a teleport can connect once and then
 * keeps running without hitting again on later frames.
 *
 * Every path ends the same way: return up a level, or install the reaction
 * exit if there is no level left. The compiler emitted that ending THREE
 * times, once per path, each with its own load of the same pointer slot at
 * 0x000f3708. Written once here, because the three copies do the same thing.
 *
 * PROC+0x2c is written by byte offset: it falls inside the unnamed run at
 * 0x2c and this is the second store to reach it, which is still not enough to
 * name it.
 */
long t_tele_scan(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t n;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    n = obj->field00->field28;
    obj->field1c = n;

    if (n == 0) {                       /* it has not connected yet */
        obj->field00->field18 = 0x622;

        obj->field1c = 0x13;
        strike_check_a0(obj);

        if (obj->field5c != 0) {        /* and it does now */
            *(uint32_t *)((char *)obj->field00 + 0x2c) = obj->field18;
            obj->field1c          = 1;
            obj->field00->field28 = 1;  /* not again */
        }
    }

    if ((long)thread->frame > 0) {
        thread->frame -= 1;             /* back up a level */
        return 0;
    }

    return mk3_push_handler(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* -------------------------------------------------------------- tl_do_lao_angle
 *
 * armv7 0x0003e99c, 192 bytes.  **Complete.**
 *
 *      token == 0:       obj->field20          = 0x20c
 *                        obj->field00->field18 = 0x20c
 *                        air_init_special(obj)
 *                        obj->field1c = 0; group_sound(obj)
 *                        obj->field1c = 0; ochar_sound(obj)
 *                        obj->field40 = 0x19
 *                        obj->field1c = 0xfff80000
 *                        obj->field20 = 0x00030000
 *                        obj->field24 = 0x00004000
 *                        obj->field28 = 2
 *                        obj->field34 = t_lao_angle_scan
 *                        token := 0x8f8, then descend into t_flight_call
 *
 *      token == 0x8f8:   frame[frame].handler = t_jump_up_land_jsrp
 *
 *      otherwise:        return -3
 *
 * Entry 7 of the propell table, and the clearest example in this file of how a
 * special move is set up: the action pair, two sounds, an animation, three
 * velocity components, **a callback**, and then a descend.
 *
 * **0x34 is the callback slot.** `t_lao_angle_scan` is written there and
 * `t_flight_call` is what runs it -- so the flight routine is generic and the
 * per-move behaviour is the function it is handed. `flight_move_ani` writes
 * the same slot with `is_he_left` or `is_he_right` and calls it directly.
 *
 * The three velocity components are one literal and two steps again:
 * 0xfff80000, then `add #0xb0000` reaches 0x30000, then `sub #0x2c000` reaches
 * 0x4000 -- -8.0, 3.0 and 0.25 in 16.16.
 *
 * Both sounds are asked for with 0 in 0x1c, written from the register the
 * guard already proved was zero.
 */
long tl_do_lao_angle(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token != 0) {
        if (token != 0x8f8)
            return -3;
        return mk3_install(thread,
                                (MK3THREADFUNC)t_jump_up_land_jsrp);
    }

    obj->field20          = 0x20c;
    obj->field00->field18 = 0x20c;
    air_init_special(obj);

    obj->field1c = 0;
    group_sound(obj);
    obj->field1c = 0;
    ochar_sound(obj);

    obj->field40 = 0x19;

    obj->field1c = 0xfff80000u;         /* -8.00 */
    obj->field20 = 0x00030000;          /*  3.00, by add #0xb0000 */
    obj->field24 = 0x00004000;          /*  0.25, by sub #0x2c000 */
    obj->field28 = 2;

    obj->field34 = (uint32_t)(uintptr_t)t_lao_angle_scan;   /* the callback */

    *mk3_frame(thread, thread->frame + 1) = 0x8f8;
    thread->frame = thread->frame + 1;          /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_flight_call;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------------- t_super_kick_land
 *
 * armv7 0x0003ccc8, 196 bytes.  **Complete.**
 *
 *      token == 0:       obj->field20          = 0x612
 *                        obj->field00->field18 = 0x612
 *                        stop_me_player(obj)
 *                        park(token 0x5d7, duration 0x14)
 *
 *      token == 0x5d7:   obj->field1c = 0
 *                        obj->field24 = 0xa000
 *                        obj->field20 = 0
 *                        obj->field34 = 0
 *                        obj->field28 = 0xfff
 *                        token := 0x5de, then descend into t_flight
 *
 *      token == 0x5de:   obj->field40 = 3
 *                        find_ani2_part2(obj)
 *                        obj->field1c = 3
 *                        frame[frame].handler = t_mframew
 *
 *      otherwise:        return -3
 *
 * **Three states**, like `t_cyrax_implode`: stop and wait twenty ticks, then
 * fall through `t_flight`, then pick the landing animation. Two tokens and a
 * descend between them.
 *
 * **0x34 is CLEARED before the descend.** That is the callback slot -- the one
 * `tl_do_lao_angle` fills with `t_lao_angle_scan` so the flight has something
 * per-move to run. Zeroing it here says this flight has none, which is the
 * other half of what that slot means.
 *
 * 0xa000 is 0.625 in the 16.16 this directory uses. It is the only velocity
 * component set; 0x1c and 0x20 are zeroed from the same register beside it.
 *
 * The frame index is shifted by `lsls r3, r6` with r6 holding 3, rather than
 * by an immediate -- the register was already 3 for `obj->field40`, so the
 * shift borrows it.
 */
long t_super_kick_land(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x5d7) {
        obj->field1c = 0;
        obj->field24 = 0xa000;          /* 0.625 */
        obj->field20 = 0;
        obj->field34 = 0;               /* no callback for this flight */
        obj->field28 = 0xfff;

        *mk3_frame(thread, thread->frame + 1) = 0x5de;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x5de) {
        obj->field40 = 3;
        find_ani2_part2(obj);
        obj->field1c = 3;
        return mk3_install(thread, (MK3THREADFUNC)t_mframew);
    }

    if (token != 0)
        return -3;

    obj->field20          = 0x612;
    obj->field00->field18 = 0x612;
    stop_me_player(obj);

    *mk3_frame(thread, thread->frame + 1) = 0x5d7;
    thread->fieldfc = 0x14;
    return 0x14;
}


/* -------------------------------------------------------------- t_blur_catchup
 *
 * armv7 0x0003d6a4, 200 bytes.  **Complete.**
 *
 *      token == 0:       obj->field1c = 1
 *                        ochar_sound(obj)
 *                        obj->field48 = 0x00070004
 *                        shake_a11(obj)
 *                        obj->field38 = 0x12
 *                        *(uint16_t *)(G + 0x456) = 0x12
 *                        obj->field40 = 0
 *                        get_char_ani2(obj)
 *                        find_part2(obj)
 *                        obj->field1c = 3
 *                        token := 0x536, then descend into t_mframew
 *
 *      token == 0x536:   if (thread->frame > 0) { thread->frame -= 1; return 0 }
 *                        frame[frame].handler = t_local_reaction_exit
 *
 *      otherwise:        return -3
 *
 * 0x48 gets **another packed pair, 7 high and 4 low**, and `shake_a11` is what
 * reads it -- so the shake's two amplitudes come from one word, the same shape
 * `skinny_spawn` unpacks and `bike_hit_call` passes on.
 *
 * 0x12 goes to the object's 0x38 and to the halfword at G+0x456 from one
 * register, which is the third routine in this file to write that global slot
 * alongside a field of its own.
 *
 * The zero into 0x40 comes from the guard's register, so `get_char_ani2`
 * resolves animation zero without a constant being loaded for it.
 */
long t_blur_catchup(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token != 0) {
        if (token != 0x536)
            return -3;

        if ((long)thread->frame > 0) {
            thread->frame -= 1;         /* back up a level */
            return 0;
        }
        return mk3_install(thread,
                                (MK3THREADFUNC)t_local_reaction_exit);
    }

    obj->field1c = 1;
    ochar_sound(obj);

    obj->field48 = 0x00070004;          /* 7 high, 4 low */
    shake_a11(obj);

    obj->field38 = 0x12;
    *(uint16_t *)(G_BYTES + 0x456) = 0x12;

    obj->field40 = 0;                   /* the guard's zero */
    get_char_ani2(obj);
    find_part2(obj);

    obj->field1c = 3;

    *mk3_frame(thread, thread->frame + 1) = 0x536;
    thread->frame = thread->frame + 1;          /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_mframew;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ----------------------------------------------------------------------- t_lfly5
 *
 * armv7 0x0003ea5c, 200 bytes.  **Complete.**
 *
 *      token == 0:       park(token 0x6c4, duration 1)
 *
 *      token == 0x6c4:   distance_off_ground(obj)
 *                        if (obj->field1c <= 0x5f) {
 *                            frame[frame].handler = t_lfly5   ; itself
 *                        } else {
 *                            obj->field1c = 0; group_sound(obj)
 *                            obj->field1c          = 0
 *                            obj->field08->field1c = 0
 *                            obj->field40 = 3; get_char_ani2(obj)
 *                            obj->field1c = 3; init_anirate(obj)
 *                            obj->field1c = 0xa0000
 *                            towards_x_vel(obj)
 *                            face_opponent(obj)
 *                            obj->field48 = 0x1a
 *                            frame[frame].handler = t_lfly4
 *                        }
 *
 *      otherwise:        return -3
 *
 * **It waits for height by installing itself.** Below 0x5f off the ground the
 * handler it installs is `t_lfly5` -- the same function, at a direct
 * pc-relative 0x0003ea5d, which is this function's own address with the Thumb
 * bit. Installing also clears the slot above, so the token goes back to zero,
 * the next entry parks with 0x6c4 again, and the check repeats every two
 * ticks until the fighter is high enough.
 *
 * Once he is, the move is set up in full and handed to `t_lfly4`, which is the
 * one-tick strike poll with the countdown at 0x48 -- and 0x1a is the number of
 * frames written here for it to count down.
 *
 * 0xa0000 is 10.0 in the 16.16 this directory uses for velocity.
 */
long t_lfly5(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x6c4;
        thread->fieldfc = 1;
        return 1;
    }

    if (token != 0x6c4)
        return -3;

    distance_off_ground(obj);
    if ((long)obj->field1c <= 0x5f)     /* not high enough yet */
        return mk3_install(thread, (MK3THREADFUNC)t_lfly5);

    obj->field1c = 0;
    group_sound(obj);

    obj->field1c          = 0;
    obj->field08->field1c = 0;

    obj->field40 = 3;
    get_char_ani2(obj);
    obj->field1c = 3;
    init_anirate(obj);

    obj->field1c = 0xa0000;             /* 10.0 */
    towards_x_vel(obj);
    face_opponent(obj);

    obj->field48 = 0x1a;                /* the frames t_lfly4 counts down */

    return mk3_install(thread, (MK3THREADFUNC)t_lfly4);
}


/* ------------------------------------------------------------------ t_pounce_miss
 *
 * armv7 0x0003fd10, 212 bytes.  **Complete.**
 *
 *      token == 0:       obj->field1c          = 0x62f
 *                        obj->field00->field18 = 0x62f
 *                        set_no_block(obj)
 *                        park(token 0x50b, duration 0xc)
 *
 *      token == 0x50b:   do_next_a9_frame(obj)
 *                        park(token 0x50d, duration 4)
 *
 *      token == 0x50d:   q_is_he_a_boss(obj)
 *                        if (obj->field5c != 0)
 *                            park(token 0x512, duration 8)
 *                        else
 *                            frame[frame].handler = t_local_reaction_exit
 *
 *      token == 0x512:   frame[frame].handler = t_local_reaction_exit
 *
 *      otherwise:        return -3
 *
 * **Four states, and the fourth only exists against a boss.** The recovery
 * from a missed pounce is twelve ticks, then four, and then it ends -- unless
 * `q_is_he_a_boss` answers yes, in which case there are eight more before the
 * exit. Missing a boss costs longer than missing anyone else, and that is the
 * whole difference between the two endings.
 *
 * The dispatch is a **sorted comparison chain**, not a flat run of equality
 * tests: `cmp` against 0x50b, then `ble` for everything below it, then 0x50d,
 * then `adds r2, #5` to reach 0x512 from the constant already in the register.
 * With four tokens the compiler ordered them instead of testing each in turn.
 *
 * The last state addresses the frame through r1, the index read in the first
 * instruction, and `adds r3, r3, r0` with r0 still holding the thread -- that
 * path never touched either, so nothing had to be re-read.
 */
long t_pounce_miss(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x50b) {
        do_next_a9_frame(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x50d;
        thread->fieldfc = 4;
        return 4;
    }

    if (token == 0x50d) {
        q_is_he_a_boss(obj);
        if (obj->field5c != 0) {        /* a boss takes longer */
            *mk3_frame(thread, thread->frame + 1) = 0x512;
            thread->fieldfc = 8;
            return 8;
        }
        return mk3_install(thread,
                                (MK3THREADFUNC)t_local_reaction_exit);
    }

    if (token == 0x512)
        return mk3_install(thread,
                                (MK3THREADFUNC)t_local_reaction_exit);

    if (token != 0)
        return -3;

    obj->field1c          = 0x62f;
    obj->field00->field18 = 0x62f;
    set_no_block(obj);

    *mk3_frame(thread, thread->frame + 1) = 0x50b;
    thread->fieldfc = 0xc;
    return 0xc;
}


/* ------------------------------------------------------------- tl_do_reptile_dash
 *
 * armv7 0x0003ef90, 212 bytes.  **Complete.**
 *
 *      token == 0:       obj->field20 = 0x216
 *                        init_special_act(obj)
 *                        obj->field40 = 0x46
 *                        get_char_ani(obj)
 *                        obj->field1c = 0xd0000
 *                        towards_x_vel(obj)
 *                        obj->a10 = 0x10
 *                        park(token 0x236, duration 1)
 *
 *      token == 0x236:   do_next_a9_frame(obj)
 *                        sans_repell_3(obj)
 *                        is_he_airborn(obj)
 *                        if (obj->field5c == 0) {
 *                            obj->field1c = 0x15
 *                            strike_check_a0(obj)
 *                            if (obj->field5c != 0) {
 *                                frame[frame].handler = t_reptile_dash_hit
 *                                return 0
 *                            }
 *                        }
 *                        if (--obj->a10 > 0)
 *                            park(token 0x236, duration 1)
 *                        else
 *                            frame[frame].handler = t_local_reaction_exit
 *
 *      otherwise:        return -3
 *
 * Entry 23 of the propell table. A one-tick loop for sixteen frames: advance
 * the animation, cancel the repel, and look for a strike.
 *
 * **The dash only strikes an opponent who is on the ground.** `is_he_airborn`
 * answers into 0x5c and a non-zero there skips the check entirely -- so a
 * jumping opponent is passed under rather than hit, and the dash still runs
 * its full sixteen frames.
 *
 * **0x44 is a frame counter here, not the argument slot.** The struct calls it
 * `a10` because that is what it usually is; this routine sets it to 0x10 and
 * counts it down, which is worth knowing before anyone reads a value out of
 * it expecting an argument.
 *
 * The park is shared between the entry and the countdown -- the `bgt` jumps
 * back to the same three instructions state 0 falls into -- so the first tick
 * and every later one cost the same.
 *
 * 0xd0000 is 13.0 in the 16.16 this directory uses.
 */
long tl_do_reptile_dash(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token != 0) {
        if (token != 0x236)
            return -3;

        do_next_a9_frame(obj);
        sans_repell_3(obj);

        is_he_airborn(obj);
        if (obj->field5c == 0) {        /* only a grounded opponent is hit */
            obj->field1c = 0x15;
            strike_check_a0(obj);
            if (obj->field5c != 0)
                return mk3_install(thread,
                                        (MK3THREADFUNC)t_reptile_dash_hit);
        }

        obj->a10 -= 1;
        if ((int32_t)obj->a10 <= 0)     /* out of frames */
            return mk3_install(thread,
                                    (MK3THREADFUNC)t_local_reaction_exit);
    } else {
        obj->field20 = 0x216;
        init_special_act(obj);

        obj->field40 = 0x46;
        get_char_ani(obj);

        obj->field1c = 0xd0000;         /* 13.0 */
        towards_x_vel(obj);

        obj->a10 = 0x10;                /* sixteen frames, counted down */
    }

    *mk3_frame(thread, thread->frame + 1) = 0x236;
    thread->fieldfc = 1;
    return 1;
}


/* -------------------------------------------------------------------- t_pounce_hit
 *
 * armv7 0x0003e36c, 232 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      <collapse one level>            ; twice, inline
 *      <collapse one level>
 *      reset_proc_stack(thread)
 *      obj->field20 = 0xd
 *      obj->field24 = 0xd
 *      obj->field28 = 0xd
 *      obj->field1c = 0
 *      obj->field34 = t_pounce_adjust_him
 *      frame[frame].handler = t_pounce_fall
 *      frame[frame+1].w0 = 0
 *
 * **It unwinds TWO levels of the thread's frame stack**, and that is the whole
 * point of the function. `t_pounce_scan` is running nested inside whatever
 * launched the pounce; when the strike connects, the fall must not stay nested
 * under it, so two levels are collapsed before the fall handler is installed.
 *
 * One collapse is:
 *
 *      if (thread->frame > 0)
 *          thread->frame -= 1
 *      else
 *          frame[frame].handler = t_local_reaction_exit, slot cleared
 *
 *      n = thread->frame
 *      h = frame[n+1].handler          ; the level above
 *      frame[n+1].w0 = frame[n+2].w0   ; its token comes down
 *      frame[n].handler = h            ; and so does its handler
 *
 * so the level above is moved down onto this one. The binary has that block
 * written out TWICE, in full, one after the other; it is a static function
 * here because two identical copies read worse than one named thing used
 * twice, and nothing else in the file needs it.
 *
 * At the bottom of the stack the else-branch installs the reaction exit
 * instead of decrementing, so a pounce that connects with nothing left to
 * unwind still ends cleanly rather than running off the end.
 *
 * 0xd goes to three fields from one register, and 0x34 -- the callback slot --
 * gets `t_pounce_adjust_him`, which is the routine `t_pounce_fall` will run
 * each frame to keep the opponent under the attacker.
 */

long t_pounce_hit(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t n, h;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    /* Corrected, 2026-09-26: collapse one level, twice, written out as the
     * binary has it. The first transcription used a helper that installed
     * through mk3_push_handler -- which refuses when the slot above is
     * busy, a check the binary does not make -- and ended the same way. */
    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
    } else {
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_local_reaction_exit;
        *mk3_frame(thread, thread->frame + 1) = 0;
    }
    n = thread->frame;
    h = mk3_frame(thread, n + 1)[1];            /* the level above */
    *mk3_frame(thread, n + 1) = *mk3_frame(thread, n + 2);
    mk3_frame(thread, n)[1] = h;

    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
    } else {
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_local_reaction_exit;
        *mk3_frame(thread, thread->frame + 1) = 0;
    }
    n = thread->frame;
    h = mk3_frame(thread, n + 1)[1];            /* the level above */
    *mk3_frame(thread, n + 1) = *mk3_frame(thread, n + 2);
    mk3_frame(thread, n)[1] = h;

    reset_proc_stack(thread);

    obj->field20 = 0xd;
    obj->field24 = 0xd;
    obj->field28 = 0xd;
    obj->field1c = 0;

    obj->field34 = (uint32_t)(uintptr_t)t_pounce_adjust_him;    /* callback */

    return mk3_install(thread, (MK3THREADFUNC)t_pounce_fall);
}


/* ------------------------------------------------------------------ t_zoom_blocked
 *
 * armv7 0x0003e8bc, 224 bytes.  **Complete.**
 *
 *      token == 0:       obj->field1c          = 0x61e
 *                        obj->field00->field18 = 0x61e
 *                        match_me_with_him(obj)
 *                        f = obj->field08->field28 & ~0x10
 *                        obj->field2c          = f
 *                        obj->field08->field28 = f
 *                        obj->field1c = 0x10
 *                        v = obj->field08->field18
 *                        obj->field20 = 0
 *                        obj->field28 = v
 *                        if (v >= 0) obj->field1c = -0x10
 *                        multi_adjust_xy(obj)
 *                        face_opponent(obj)
 *                        obj->field40 = 0x1a
 *                        get_char_ani(obj)
 *                        obj->field40 += 4
 *                        obj->field1c = 0x00010000
 *                        obj->field20 = 0xfff80000
 *                        obj->field24 = 0x00006000
 *                        obj->field28 = 2
 *                        token := 0x3f6, then descend into t_flight
 *
 *      token == 0x3f6:   frame[frame].handler = t_jump_up_land_jump
 *
 *      otherwise:        return -3
 *
 * **It forces the flip bit OFF rather than toggling it.** `bic r3, r3, #0x10`
 * on the other object's 0x28 clears the same bit `flip_multi_ob` XORs, and the
 * result is written to both that field and this object's 0x2c. A blocked zoom
 * must end facing a known way, and a toggle could not guarantee that.
 *
 * The push-back distance is **signed by which side he is on**: 0x10 is written
 * first, and `mvnge r3, #0xf` replaces it with -0x10 when the other object's
 * 0x18 is not negative. One `mvn` for the negative constant, in an IT block
 * rather than a branch.
 *
 * The three velocity components are one literal and two steps once more:
 * 0x10000, then `sub #0x90000` reaches 0xfff80000, then `add #0x86000` reaches
 * 0x6000 -- 1.0, -8.0 and 0.375. That is the fourth routine in this file built
 * that way.
 *
 * It ends like `t_lao_angle_blocked`: descend into the shared flight and come
 * back to `t_jump_up_land_jump`. Blocked specials land in a jump.
 */
long t_zoom_blocked(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    uint32_t f;
    int32_t  v;

    if (token != 0) {
        if (token != 0x3f6)
            return -3;
        return mk3_install(thread,
                                (MK3THREADFUNC)t_jump_up_land_jump);
    }

    obj->field1c          = 0x61e;
    obj->field00->field18 = 0x61e;
    match_me_with_him(obj);

    f = obj->field08->field28 & ~0x10u;         /* cleared, not toggled */
    obj->field2c          = f;
    obj->field08->field28 = f;

    obj->field1c = 0x10;
    v = (int32_t)obj->field08->field18;
    obj->field20 = 0;
    obj->field28 = (uint32_t)v;
    if (v >= 0)
        obj->field1c = (uint32_t)(-0x10);       /* mvnge r3, #0xf */

    multi_adjust_xy(obj);
    face_opponent(obj);

    obj->field40 = 0x1a;
    get_char_ani(obj);
    obj->field40 += 4;                  /* an offset from what was resolved */

    obj->field1c = 0x00010000;          /*  1.000 */
    obj->field20 = 0xfff80000u;         /* -8.000, by sub #0x90000 */
    obj->field24 = 0x00006000;          /*  0.375, by add #0x86000 on that */
    obj->field28 = 2;

    *mk3_frame(thread, thread->frame + 1) = 0x3f6;
    thread->frame = thread->frame + 1;          /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_flight;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* -------------------------------------------------------------- t_tusk_blur_blocked
 *
 * armv7 0x0003fe00, 216 bytes.  **Complete.**
 *
 *      token == 0:       blur_blocked_setup(obj)
 *                        w = *(uint32_t *)obj->field40
 *                        obj->field1c = w
 *                        if (w != 0) {
 *                            obj->field1c = 1
 *                            token := 0x321, descend into t_mframew
 *                        }
 *                        ; and otherwise falls into the 0x321 body below
 *
 *      token == 0x321:   obj->field40 = 1
 *                        find_ani2_part2(obj)
 *                        obj->field1c = 1
 *                        token := 0x326, descend into t_mframew
 *
 *      token == 0x326:   frame[frame].handler = t_blb8
 *
 *      otherwise:        return -3
 *
 * Three states, and the first one **falls into the second's body** when the
 * animation at 0x40 has already run out. That is why the entry has two exits:
 * a first word still there means one pass through `t_mframew` under token
 * 0x321 before the second animation is chosen, and a zero means the second
 * animation is chosen straight away.
 *
 * The first word of an animation being zero is the same end-of-frames test
 * `t_lao_angle_scan` uses; there it decides whether to check for a hit, here
 * whether a wait is needed at all.
 *
 * `adds r3, r3, r6` reaches frame+1 through the register that already holds 1
 * for `obj->field40`, which is the same borrowing `t_super_kick_land` does
 * with its 3.
 */
long t_tusk_blur_blocked(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x326)
        return mk3_install(thread, (MK3THREADFUNC)t_blb8);

    if (token != 0 && token != 0x321)
        return -3;

    if (token == 0) {
        uint32_t w;

        blur_blocked_setup(obj);

        w = *(uint32_t *)(uintptr_t)obj->field40;
        obj->field1c = w;

        if (w != 0) {                   /* frames left: wait one pass */
            obj->field1c = 1;
            *mk3_frame(thread, thread->frame + 1) = 0x321;
            thread->frame = thread->frame + 1;      /* push a level */
            mk3_frame(thread, thread->frame)[1] =
                (uint32_t)(uintptr_t)t_mframew;
            *mk3_frame(thread, thread->frame + 1) = 0;
            return 0;
        }
        /* and otherwise straight on into the 0x321 body */
    }

    obj->field40 = 1;
    find_ani2_part2(obj);
    obj->field1c = 1;

    *mk3_frame(thread, thread->frame + 1) = 0x326;
    thread->frame = thread->frame + 1;              /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_mframew;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------------- tl_sonya_bike_kick
 *
 * armv7 0x0003d8a8, 216 bytes.  **Complete.**
 *
 *      token == 0:       obj->field20 = 0x204
 *                        init_special_act(obj)
 *                        obj->field1c = 3; ochar_sound(obj)
 *                        obj->field1c = 5; ochar_sound(obj)
 *                        obj->field40 = 0x00020001
 *                        token := 0xafc, then descend into t_animate2_a9
 *
 *      token == 0xafc:   obj->field1c = 4
 *                        init_anirate(obj)
 *                        obj->field1c = 0x40000
 *                        towards_x_vel(obj)
 *                        obj->field08->field1c = 0xfffe0000
 *                        obj->field1c          = 0xffff4000
 *                        obj->field08->field20 = 0xffff4000
 *                        set_nocol(obj)
 *                        frame[frame].handler = tl_bike3
 *
 *      otherwise:        return -3
 *
 * Entry 1 of the propell table. **Two sounds, 3 then 5**, both through
 * `ochar_sound` with the number in 0x1c -- the only routine in this file so far
 * to ask for two in a row from the same call.
 *
 * 0x40 gets 0x00020001, the packed shape it takes when it is a number: 2 in
 * the high half and 1 in the low.
 *
 * The velocities are three separate pool literals here rather than a chain --
 * 4.0 forward, then -2.0 into the other object's 0x1c and -0.75 into both this
 * object's 0x1c and the other's 0x20. Nothing is derived from anything, which
 * is the exception in this file.
 *
 * `set_nocol` turns collision off before the ride begins, and `tl_bike3` is a
 * direct pc-relative address so it is in this file.
 */
long tl_sonya_bike_kick(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token != 0) {
        if (token != 0xafc)
            return -3;

        obj->field1c = 4;
        init_anirate(obj);

        obj->field1c = 0x40000;                 /*  4.00 forward */
        towards_x_vel(obj);

        obj->field08->field1c = 0xfffe0000u;    /* -2.00 */
        obj->field1c          = 0xffff4000u;    /* -0.75 */
        obj->field08->field20 = 0xffff4000u;

        set_nocol(obj);                         /* no collision on the ride */

        return mk3_install(thread, (MK3THREADFUNC)tl_bike3);
    }

    obj->field20 = 0x204;
    init_special_act(obj);

    obj->field1c = 3;
    ochar_sound(obj);
    obj->field1c = 5;
    ochar_sound(obj);

    obj->field40 = 0x00020001;          /* 2 high, 1 low */

    *mk3_frame(thread, thread->frame + 1) = 0xafc;
    thread->frame = thread->frame + 1;          /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_animate2_a9;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ---------------------------------------------------------------- tl_do_sg_pounce
 *
 * armv7 0x0003e454, 224 bytes.  **Complete.**
 *
 *      token == 0:       obj->field20 = 0x20f
 *                        init_special_act(obj)
 *                        obj->field1c = 0; ochar_sound(obj)
 *                        obj->field40 = 0; get_char_ani2(obj)
 *                        do_next_a9_frame(obj)
 *                        obj->field48 = (int16_t)obj->field00->him[+0x0e]
 *                        obj->field08->field1c = 0xfff80000
 *                        obj->field1c          = obj->field08->field1c + 0x70000
 *                        obj->field08->field20 = that
 *                        park(token 0x4d9, duration 1)
 *
 *      token == 0x4d9:   distance_off_ground(obj)
 *                        if (obj->field1c <= 0xe7)
 *                            park(token 0x4d9, duration 1)
 *                        obj->field08[+0x0e] = (uint16_t)obj->field48
 *                        obj->field40 = 0; get_char_ani2(obj)
 *                        obj->field40 += 8
 *                        obj->field1c = 0
 *                        obj->field20 = 0x60000
 *                        obj->field24 = 0x08000
 *                        obj->field28 = 2
 *                        obj->field34 = t_pounce_scan
 *                        frame[frame].handler = t_pounce_fall
 *
 *      otherwise:        return -3
 *
 * Entry 15 of the propell table, and it **remembers where the opponent was**.
 * The halfword at his +0x0e -- the integer part of a coordinate -- is saved
 * into 0x48 on the way up and written back on the way down, so the pounce lands
 * where he stood when it started rather than where he has moved to.
 *
 * The rise is a one-tick poll like `t_lfly5`'s, waiting for 0xe7 off the
 * ground; the park is shared between the entry and the loop, so the height
 * test costs the same every tick.
 *
 * 0x34 gets `t_pounce_scan` -- the callback `t_pounce_fall` runs each frame,
 * and the routine that decides whether the landing connects. Both halves of
 * that pair were read separately and only join up here.
 *
 * Two derived constants again: the descent's -8.0 has 0x70000 added to reach
 * -1.0 for the horizontal pair, and 6.0 has 0x58000 subtracted to reach 0.5.
 */
long tl_do_sg_pounce(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token != 0) {
        if (token != 0x4d9)
            return -3;

        distance_off_ground(obj);
        if ((long)obj->field1c > 0xe7) {        /* high enough now */
            MK3OBJ *other = obj->field08;

            *(uint16_t *)((char *)other + 0x0e) = (uint16_t)obj->field48;

            obj->field40 = 0;
            get_char_ani2(obj);
            obj->field40 += 8;          /* an offset from what was resolved */

            obj->field1c = 0;
            obj->field20 = 0x60000;     /* 6.0 */
            obj->field24 = 0x08000;     /* 0.5, by sub #0x58000 */
            obj->field28 = 2;

            obj->field34 = (uint32_t)(uintptr_t)t_pounce_scan;  /* callback */

            return mk3_install(thread, (MK3THREADFUNC)t_pounce_fall);
        }
    } else {
        MK3OBJ *him;

        obj->field20 = 0x20f;
        init_special_act(obj);

        obj->field1c = 0;
        ochar_sound(obj);

        obj->field40 = 0;
        get_char_ani2(obj);
        do_next_a9_frame(obj);

        him = (MK3OBJ *)(uintptr_t)obj->field00->him;
        obj->field48 = (uint32_t)(int32_t)
                       *(int16_t *)((char *)him + 0x0e);   /* where he stood */

        obj->field08->field1c = 0xfff80000u;                /* -8.0 */
        obj->field1c          = obj->field08->field1c + 0x70000;
        obj->field08->field20 = obj->field1c;
    }

    *mk3_frame(thread, thread->frame + 1) = 0x4d9;
    thread->fieldfc = 1;                /* look again next tick */
    return 1;
}


/* ------------------------------------------------------------------ tl_do_lk_bike
 *
 * armv7 0x0003f2fc, 232 bytes.  **Complete.**
 *
 *      token == 0:       obj->field20 = 0x20d
 *                        init_special_act(obj)
 *                        obj->field1c = 4; ochar_sound(obj)
 *                        obj->field1c = 0xfff70000
 *                        away_x_vel(obj)
 *                        obj->field1c = 0xfff70000
 *                        obj->field28 = 4
 *                        obj->field40 = 0x00010002
 *                        obj->field20 = 0xfffc8000
 *                        obj->field24 = obj->field20 + 0x3b000
 *                        obj->field34 = t_bike_call
 *                        obj->field48 = 6
 *                        token := 0x6b1, then descend into t_flight_call
 *
 *      token == 0x6b1:   if (thread->frame > 0) { thread->frame -= 1; return 0 }
 *                        frame[frame].handler = t_local_reaction_exit
 *
 *      otherwise:        return -3
 *
 * Entry 13 of the propell table, and the second routine in this file to hand a
 * callback to `t_flight_call`: 0x34 gets `t_bike_call`, exactly as
 * `tl_do_lao_angle` gives it `t_lao_angle_scan`.
 *
 * -9.0 is used **twice from one register** -- once as the argument to
 * `away_x_vel` and once left in 0x1c afterwards -- which is why r8 holds it
 * across the call rather than the value being reloaded.
 *
 * -3.5 and its neighbour are the derived pair again: `add #0x3b000` on
 * 0xfffc8000 reaches 0x00003800, so 0.22 costs no literal of its own.
 *
 * 0x40 takes the packed number shape once more, 1 high and 2 low.
 */
long tl_do_lk_bike(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token != 0) {
        if (token != 0x6b1)
            return -3;

        if ((long)thread->frame > 0) {
            thread->frame -= 1;         /* back up a level */
            return 0;
        }
        return mk3_install(thread,
                                (MK3THREADFUNC)t_local_reaction_exit);
    }

    obj->field20 = 0x20d;
    init_special_act(obj);

    obj->field1c = 4;
    ochar_sound(obj);

    obj->field1c = 0xfff70000u;         /* -9.0, kept in r8 across the call */
    away_x_vel(obj);
    obj->field1c = 0xfff70000u;

    obj->field28 = 4;
    obj->field40 = 0x00010002;          /* 1 high, 2 low */

    obj->field20 = 0xfffc8000u;         /* -3.50 */
    obj->field24 = obj->field20 + 0x3b000;

    obj->field34 = (uint32_t)(uintptr_t)t_bike_call;    /* the callback */
    obj->field48 = 6;

    *mk3_frame(thread, thread->frame + 1) = 0x6b1;
    thread->frame = thread->frame + 1;          /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_flight_call;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ---------------------------------------------------------------- t_sctele_calla_1
 *
 * armv7 0x00040aa0, 212 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      sans_repell_3(obj)
 *      hisx = (int16_t)obj->field08[+0x0e]
 *      base = *(long *)(G + 0x468)
 *      obj->field1c = hisx
 *      obj->field20 = base
 *      obj->field28 = base + 0x18f
 *      d = obj->field08->field18
 *      obj->field24 = d
 *      if (d != 0) {
 *          if (d >= 0) {                       ; the three roles swap
 *              obj->field1c = base + 0x18f
 *              obj->field20 = hisx
 *              obj->field28 = base
 *          }
 *          if (obj->field20 >= obj->field1c) {
 *              obj->field08[+0x0e]   = (uint16_t)obj->field28
 *              obj->field34          = t_sctele_calla_2
 *              *(uint32_t *)((char *)obj->field00 + 0x34) = t_sctele_calla_2
 *              clear_inviso(obj)
 *              clear_noedge(obj)
 *          }
 *      }
 *      if (thread->frame > 0) { thread->frame -= 1; return 0 }
 *      frame[frame].handler = t_local_reaction_exit
 *
 * **Which side of the arena the teleport lands on.** Two bounds come out of
 * the global state -- `G + 0x468` and that plus 0x18f -- and the opponent's
 * own 0x18 decides which is the source and which the destination. When it is
 * positive the three slots are rewritten with the roles swapped, in three
 * conditional stores under two IT blocks rather than a branch.
 *
 * The move only happens when 0x20 has ended up at or past 0x1c. That test
 * reads the slots back rather than the registers, so it sees whichever
 * assignment the swap left behind.
 *
 * **The callback goes into TWO places**: this object's 0x34 and the PROC's.
 * Every other routine in this file writes only the object's, so whatever runs
 * the second stage looks for it on the PROC as well.
 *
 * 0x18f is built as `+0x18c` then `+3`, the same two-step `teleport_next_to`
 * uses for the same constant against a different global slot.
 */
long t_sctele_calla_1(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    int32_t  hisx, base, d;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    sans_repell_3(obj);

    hisx = *(int16_t *)((char *)obj->field08 + 0x0e);
    base = *(int32_t *)(G_BYTES + 0x468);

    obj->field1c = (uint32_t)hisx;
    obj->field20 = (uint32_t)base;
    obj->field28 = (uint32_t)(base + 0x18f);

    d = (int32_t)obj->field08->field18;
    obj->field24 = (uint32_t)d;

    if (d != 0) {
        if (d >= 0) {                   /* the three roles swap */
            obj->field1c = (uint32_t)(base + 0x18f);
            obj->field20 = (uint32_t)hisx;
            obj->field28 = (uint32_t)base;
        }

        if ((int32_t)obj->field20 >= (int32_t)obj->field1c) {
            *(uint16_t *)((char *)obj->field08 + 0x0e) =
                (uint16_t)obj->field28;

            /* The PROC's 0x34 falls inside an unnamed run, so it is
             * written by offset; the object's is a named field. */
            obj->field34 = (uint32_t)(uintptr_t)t_sctele_calla_2;
            *(uint32_t *)((char *)obj->field00 + 0x34) =
                (uint32_t)(uintptr_t)t_sctele_calla_2;

            clear_inviso(obj);
            clear_noedge(obj);
        }
    }

    if ((long)thread->frame > 0) {
        thread->frame -= 1;             /* back up a level */
        return 0;
    }

    return mk3_push_handler(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* ---------------------------------------------------------------- tl_do_scorp_tele
 *
 * armv7 0x0004093c, 268 bytes.  **Complete.**
 *
 *      token == 0:       obj->field20          = 0x215
 *                        obj->field00->field18 = 0x215
 *                        air_init_special(obj)
 *                        obj->field1c = G + 0x418
 *                        update_tsl(obj)
 *                        obj->field1c = 0xc; ochar_sound(obj)
 *                        t = NewThreadProc(obj, t_s_t_scroller)
 *                        t->field48 = obj
 *                        face_opponent(obj)
 *                        flip_multi(obj)
 *                        set_noedge(obj)
 *                        obj->field1c = 0x000a0000
 *                        obj->field20 = 0xfffd0000
 *                        obj->field24 = 0x00005000
 *                        obj->field28 = 3
 *                        obj->field40 = 0x00010008
 *                        obj->field34 = t_sctele_calla_1
 *                        token := 0x2ca, then descend into t_flight_call
 *
 *      token == 0x2ca:   clear_inviso(obj)
 *                        if (thread->frame > 0) { thread->frame -= 1; return 0 }
 *                        frame[frame].handler = t_local_reaction_exit
 *
 *      otherwise:        return -3
 *
 * Entry 22 of the propell table, the teleport Ermac's shares. **It starts a
 * second thread**: `NewThreadProc(obj, t_s_t_scroller)` and then the new
 * thread's 0x48 is pointed back at this object. That is the first thread
 * spawned by anything in this file.
 *
 * The two halves lock together. `t_s_t_scroller` polls
 * `((MK3OBJ *)its 0x48)->field00->field18` for 0x215 -- and 0x215 is exactly
 * what this routine writes into the PROC's 0x18 three instructions before
 * spawning it. So the scroller watches this teleport and stops when the action
 * changes.
 *
 * **Reading this pair is what found a transcription error** a few functions
 * above: `t_s_t_scroller` was written with one dereference where the binary
 * has two, which would have polled the wrong field forever. Corrected in the
 * same commit as this.
 *
 * `obj->field1c = G + 0x418` is an ADDRESS in that slot, not a value --
 * `update_tsl` takes it as a pointer into the global state.
 *
 * The velocity triple is derived again: 10.0, then `sub #0xd0000` for -3.0,
 * then `add #0x35000` for 0.3125.
 */
long tl_do_scorp_tele(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token != 0) {
        if (token != 0x2ca)
            return -3;

        clear_inviso(obj);

        if ((long)thread->frame > 0) {
            thread->frame -= 1;         /* back up a level */
            return 0;
        }
        return mk3_install(thread,
                                (MK3THREADFUNC)t_local_reaction_exit);
    }

    obj->field20          = 0x215;
    obj->field00->field18 = 0x215;      /* what the scroller watches for */
    air_init_special(obj);

    obj->field1c = (uint32_t)(uintptr_t)(G_BYTES + 0x418);  /* an address */
    update_tsl(obj);

    obj->field1c = 0xc;
    ochar_sound(obj);

    {
        MK3OBJ *t = NewThreadProc(obj, (MK3THREADFUNC)t_s_t_scroller);

        t->field48 = (uint32_t)(uintptr_t)obj;      /* pointed back at us */
    }

    face_opponent(obj);
    flip_multi(obj);
    set_noedge(obj);

    obj->field1c = 0x000a0000;          /* 10.000 */
    obj->field20 = 0xfffd0000u;         /* -3.000, by sub #0xd0000 */
    obj->field24 = 0x00005000;          /*  0.312, by add #0x35000 */
    obj->field28 = 3;

    obj->field40 = 0x00010008;          /* 1 high, 8 low */
    obj->field34 = (uint32_t)(uintptr_t)t_sctele_calla_1;   /* the callback */

    *mk3_frame(thread, thread->frame + 1) = 0x2ca;
    thread->frame = thread->frame + 1;          /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_flight_call;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------------------- t_settle_down
 *
 * armv7 0x0003d3f4, 260 bytes.  **Complete.**
 *
 *      token == 0:       token := 0x9b1, then descend into t_hover_sleep_1
 *
 *      token == 0x9b1:   v = obj->field08->field1c
 *                        obj->field30 = v
 *                        if (v < 0)
 *                            frame[frame].handler = t_settle_down   ; itself
 *                        else {
 *                            distance_from_ground(obj)
 *                            if (obj->field1c > 0xe0)
 *                                frame[frame].handler = t_settle_down
 *                            else {
 *                                obj->field34          = 0xffffa000
 *                                obj->field08->field20 = 0xffffa000
 *                                token := 0x9bd, descend into t_hover_sleep_1
 *                            }
 *                        }
 *
 *      token == 0x9bd:   v = obj->field08->field1c
 *                        obj->field30 = |v|
 *                        if (obj->field30 > 0x10000)
 *                            token := 0x9bd, descend into t_hover_sleep_1
 *                        else if (thread->frame > 0) { frame -= 1; return 0 }
 *                        else frame[frame].handler = t_local_reaction_exit
 *
 *      otherwise:        return -3
 *
 * **Two loops, and both are built out of installing something.** The first
 * reinstalls THIS function when the fall is still going the wrong way or the
 * fighter is still above 0xe0 -- installing clears the slot above, so the
 * token returns to zero, state 0 runs again, and it descends into the hover
 * once more. The second reinstalls the DESCEND itself, re-entering 0x9bd
 * until the speed at 0x1c falls to 1.0 or less.
 *
 * The two states measure the same field differently: 0x9b1 keeps the SIGN --
 * a negative speed means still rising, and that is a reason to wait -- while
 * 0x9bd takes the magnitude, because by then only how fast matters.
 *
 * -0.375 goes into 0x34 and the other object's 0x20 from one register. This is
 * the one place in the file where 0x34 takes a NUMBER rather than a callback
 * address, which is worth knowing before reading it as a function pointer.
 */
long t_settle_down(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    int32_t  v;

    if (token == 0x9b1) {
        v = (int32_t)obj->field08->field1c;
        obj->field30 = (uint32_t)v;

        if (v < 0)                      /* still rising: wait again */
            return mk3_install(thread, (MK3THREADFUNC)t_settle_down);

        distance_from_ground(obj);
        if ((long)obj->field1c > 0xe0)  /* still too high */
            return mk3_install(thread, (MK3THREADFUNC)t_settle_down);

        obj->field34          = 0xffffa000u;    /* -0.375, a number here */
        obj->field08->field20 = 0xffffa000u;

        *mk3_frame(thread, thread->frame + 1) = 0x9bd;
    } else if (token == 0x9bd) {
        v = (int32_t)obj->field08->field1c;
        obj->field30 = (uint32_t)(v < 0 ? -v : v);

        if ((int32_t)obj->field30 <= 0x10000) { /* settled */
            if ((long)thread->frame > 0) {
                thread->frame -= 1;
                return 0;
            }
            return mk3_install(thread,
                                    (MK3THREADFUNC)t_local_reaction_exit);
        }

        *mk3_frame(thread, thread->frame + 1) = 0x9bd;
    } else {
        if (token != 0)
            return -3;
        *mk3_frame(thread, thread->frame + 1) = 0x9b1;
    }

    thread->frame = thread->frame + 1;          /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_hover_sleep_1;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------- decoy_collision_check
 *
 * armv7 0x0003c434, 260 bytes.  **Complete.**
 *
 * Does the opponent touch the decoy? Smoke's decoy, and anything else that
 * puts a standing double on the screen: on contact the decoy freezes him.
 *
 * **The answer comes back in obj->field5c, not in r0.** The two tests at the
 * top call is_he_blocking and is_jade_protected for their SIDE EFFECT and then
 * read 0x5c -- is_he_blocking writes the field itself, is_jade_protected ends
 * in q_yes/q_no, which are two four-instruction routines that do nothing but
 * store 1 or 0 there. Both of those mean "no hit", so the field is cleared and
 * the routine leaves.
 *
 * The box is built twice into the SAME four fields:
 *
 *      highest_mpart_ob  -> obj->field1c      (part->field38 + part->field12)
 *      lowest_mpart_ob   -> obj->field20      (part->field40 + part->field12)
 *      leftmost_mpart_ob -> obj->field24      (flip-aware, part->field0e)
 *      rightmost_mpart_ob-> obj->field28
 *
 * First for obj->field08, the decoy's own part; then, after the shrink and the
 * copy, for obj->field00->him -- the opponent. So 0x1c..0x28 holds whoever was
 * measured last and 0x2c..0x38 holds the decoy, which is what the four
 * comparisons at the end read.
 *
 * **The decoy's box is shrunk to a quarter before it is saved.** Each axis
 * gives up (span>>3) + (span>>2) -- three eighths -- off BOTH ends, so a
 * quarter of the original span survives in each direction. That is a deliberately
 * mean hit box: you have to walk into the middle of the decoy, not brush it.
 *
 * The store of `span >> 3` into 0x38 partway through the first axis is real and
 * is dead: 0x38 is overwritten with the right edge before anything reads it,
 * and the second axis computes the same quantity into a register without
 * storing it anywhere. Transcribed as it stands rather than tidied away.
 *
 * On a hit the opponent loses his buttons, t_r_decoy_freeze goes into 0x38 --
 * the one field the box arithmetic has just finished with -- and xfer_otherguy
 * hands his thread over to run it. 0x38 is the handler slot for the transfer;
 * that it is also the box's right edge is reuse, not coincidence, and it is
 * why the pointer is written last.
 */
void decoy_collision_check(MK3OBJ *obj)
{
    int32_t left, right, top, bottom, span, adj;

    is_he_blocking(obj);
    if (obj->field5c != 0) {
        obj->field5c = 0;
        return;
    }

    is_jade_protected(obj);
    if (obj->field5c != 0) {
        obj->field5c = 0;
        return;
    }

    /* the decoy's own box */
    highest_mpart_ob(obj, obj->field08);
    lowest_mpart_ob(obj, obj->field08);
    leftmost_mpart_ob(obj, obj->field08);
    rightmost_mpart_ob(obj, obj->field08);

    left  = (int32_t)obj->field24;
    right = (int32_t)obj->field28;
    obj->field2c = (uint32_t)(left - right);
    if ((int32_t)obj->field2c < 0)
        obj->field2c = (uint32_t)(-(int32_t)obj->field2c);
    span = (int32_t)obj->field2c;
    obj->field38 = (uint32_t)(span >> 3);        /* dead: see the note above */
    adj   = (span >> 3) + (span >> 2);
    left  = left + adj;
    right = right - adj;
    obj->field24 = (uint32_t)left;

    top    = (int32_t)obj->field1c;
    bottom = (int32_t)obj->field20;
    obj->field2c = (uint32_t)(top - bottom);
    if ((int32_t)obj->field2c < 0)
        obj->field2c = (uint32_t)(-(int32_t)obj->field2c);
    span   = (int32_t)obj->field2c;
    adj    = (span >> 3) + (span >> 2);
    top    = top + adj;
    bottom = bottom - adj;

    obj->field20 = (uint32_t)bottom;
    obj->field30 = (uint32_t)bottom;            /* the saved copy starts here */
    obj->field1c = (uint32_t)top;
    obj->field2c = (uint32_t)top;
    obj->field28 = (uint32_t)right;
    obj->field34 = (uint32_t)left;
    obj->field38 = (uint32_t)right;

    /* and now the opponent's, over the top of the live four */
    highest_mpart_ob(obj, (MK3OBJ *)(uintptr_t)obj->field00->him);
    lowest_mpart_ob(obj, (MK3OBJ *)(uintptr_t)obj->field00->him);
    leftmost_mpart_ob(obj, (MK3OBJ *)(uintptr_t)obj->field00->him);
    rightmost_mpart_ob(obj, (MK3OBJ *)(uintptr_t)obj->field00->him);

    if ((int32_t)obj->field20 < (int32_t)obj->field2c ||
        (int32_t)obj->field1c > (int32_t)obj->field30 ||
        (int32_t)obj->field28 < (int32_t)obj->field34 ||
        (int32_t)obj->field24 > (int32_t)obj->field38) {
        obj->field5c = 0;                       /* no overlap */
        return;
    }

    disable_his_buttons(obj);
    obj->field38 = (uint32_t)(uintptr_t)t_r_decoy_freeze;
    xfer_otherguy(obj);
    obj->field5c = 1;
}


/* -------------------------------------------------------------- t_pounce_jsrp
 *
 * armv7 0x0003d59c, 264 bytes.  **Complete.**
 *
 * The pounce: a shake, a jump with real gravity, and a catch-up blur on the
 * way down. "jsrp" is jsr-with-parameters, and this is the routine that shows
 * what the thread's SECOND stack is for.
 *
 *      token == 0:
 *          arg[f8++] = obj->field1c            ; save
 *          arg[f8++] = obj->field48
 *          obj->field48 = 0x80008              ; (8, 8) packed, the shake
 *          shake_a11(obj)
 *          do_next_a9_frame(obj)
 *          obj->field48 = arg[--f8]            ; restore
 *          obj->field1c = arg[--f8]
 *          set_x_vel_player(obj)               ; reads the restored 0x1c
 *          obj->field1c = 0xd
 *          obj->field20 = -10.0
 *          obj->field24 = 0.75
 *          obj->field28 = 2
 *          token := 0x524, descend into t_flight
 *
 *      token == 0x524:
 *          obj->field38 = obj->field48
 *          if (obj->field48 != 0) xfer_otherguy(obj)
 *          frame[frame].handler = t_blur_catchup     ; install, NOT a descend
 *
 *      otherwise: return -3
 *
 * **The two words are saved because the shake needs 0x48 and set_x_vel_player
 * needs 0x1c, and they cannot both be live.** 0x48 carries the shake as a
 * packed pair of halves -- 0x80008 is (8, 8) -- and the routine has to give it
 * back afterwards, because on the way out of the flight the same field holds a
 * handler address. That is the whole reason for the stack: not recursion, but
 * two callees that want the same slot for different things.
 *
 * The restore of 0x1c matters for exactly one instruction. set_x_vel_player
 * reads it, and the very next store overwrites it with 13. Push, pop, use
 * once, discard.
 *
 * -10.0 and 0.75 come from ONE pool word: 0xfff60000 is stored, then
 * `add.w r3, r3, #0xac000` wraps it round to 0x0000c000. The wrap is the
 * point -- the two constants are a subtraction apart and the assembler only
 * had to spell one of them.
 *
 * The 0x524 state INSTALLS. It cannot use mk3_push_handler: the token is
 * 0x524 by construction and the refusal that helper carries would fire every
 * time. The binary branches straight into the store.
 */
long t_pounce_jsrp(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x524) {
        obj->field38 = obj->field48;
        if (obj->field48 != 0)
            xfer_otherguy(obj);
        return mk3_install(thread, (MK3THREADFUNC)t_blur_catchup);
    }

    if (token != 0)
        return -3;

    *mk3_arg(thread, thread->fieldf8) = obj->field1c;
    thread->fieldf8 += 1;
    *mk3_arg(thread, thread->fieldf8) = obj->field48;
    thread->fieldf8 += 1;

    obj->field48 = 0x80008u;                    /* (8, 8) */
    shake_a11(obj);
    do_next_a9_frame(obj);

    thread->fieldf8 -= 1;
    obj->field48 = *mk3_arg(thread, thread->fieldf8);
    thread->fieldf8 -= 1;
    obj->field1c = *mk3_arg(thread, thread->fieldf8);

    set_x_vel_player(obj);

    obj->field1c = 0xd;
    obj->field20 = 0xfff60000u;                 /* -10.0 */
    obj->field24 = 0xfff60000u + 0xac000u;      /* wraps to 0xc000 = 0.75 */
    obj->field28 = 2;

    *mk3_frame(thread, thread->frame + 1) = 0x524;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_flight;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------- t_mileena_teleport_call
 *
 * armv7 0x00040fbc, 292 bytes.  **Complete.**
 *
 * Mileena's teleport kick, from the caller's side: throw the strike, and if it
 * lands, launch her.
 *
 *      token == 0:
 *          obj->field1c = 0x15
 *          strike_check_a0(obj)                 ; answers in obj->field5c
 *          if (obj->field5c != 0) goto HIT
 *          if (thread->frame > 0) { frame -= 1; return 0 }   ; missed
 *          frame[frame].handler = t_local_reaction_exit
 *
 *      HIT:
 *          reset_proc_stack(thread)
 *          obj->field1c = obj->field00->field18 = 0x627       ; the action
 *          if (obj->field18 != 0) { field24 = 0.3125; field20 = -6.0 }
 *          else                   { field24 = 0.625;  field20 = -5.0 }
 *          arg[f8++] = obj->field20
 *          obj->field40 = 0x19
 *          obj->field54 = 3
 *          find_ani_part_a14(obj)
 *          obj->field20 = arg[--f8]             ; restore
 *          obj->field1c = 1.0
 *          obj->field28 = 6
 *          token := 0x13d, descend into t_flight
 *
 *      token == 0x13d:
 *          frame[frame].handler = t_jump_up_land_jump
 *
 *      otherwise: return -3
 *
 * **The launch is chosen by whether obj->field18 is zero**, and the two
 * choices are one constant apart in each direction: 0x5000 then minus 0x65000,
 * or 0xa000 then minus 0x5a000. A steeper, slower arc against a shallower,
 * faster one -- 0.3125 across and -6.0 up, or 0.625 across and -5.0 up. The
 * second pair is computed from `r3` while `r3` still holds the field that was
 * just tested for zero, so the add starts from a known nought.
 *
 * 0x20 goes on the argument stack across find_ani_part_a14 and comes straight
 * back: the same borrow-one-slot idiom as t_pounce_jsrp, for one callee.
 *
 * Both endings install rather than descend, and neither may carry the state-0
 * refusal: the 0x13d one because its token is 0x13d, and the miss one because
 * the binary's test at that point is `frame > 0`, not the slot. It reads the
 * frame index it stored at entry, before strike_check_a0 could move it.
 */
long t_mileena_teleport_call(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token != 0) {
        if (token != 0x13d)
            return -3;
        return mk3_install(thread, (MK3THREADFUNC)t_jump_up_land_jump);
    }

    obj->field1c = 0x15;
    strike_check_a0(obj);

    if (obj->field5c == 0) {                    /* the strike missed */
        if ((long)thread->frame > 0) {
            thread->frame -= 1;
            return 0;
        }
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }

    reset_proc_stack(thread);
    obj->field1c = 0x627;
    obj->field00->field18 = 0x627;              /* the action */

    obj->field24 = 0x5000u;                     /* 0.3125 across */
    obj->field20 = 0x5000u - 0x65000u;          /* -6.0 up */
    if (obj->field18 == 0) {
        obj->field24 = 0xa000u;                 /* 0.625 across */
        obj->field20 = 0xa000u - 0x5a000u;      /* -5.0 up */
    }

    *mk3_arg(thread, thread->fieldf8) = obj->field20;
    thread->fieldf8 += 1;

    obj->field40 = 0x19;
    obj->field54 = 3;
    find_ani_part_a14(obj);

    thread->fieldf8 -= 1;
    obj->field20 = *mk3_arg(thread, thread->fieldf8);
    obj->field1c = 0x10000u;                    /* 1.0 */
    obj->field28 = 6;

    *mk3_frame(thread, thread->frame + 1) = 0x13d;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_flight;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* -------------------------------------------------------------- tl_do_lia_fly
 *
 * armv7 0x0003e0e8, 300 bytes.  **Complete.**
 *
 * Liu Kang goes up, hovers until the ground is far enough away, then settles.
 * Three states, and the middle one is a loop built out of re-descending.
 *
 *      token == 0:      obj->field20 = 0x203
 *                       init_special_act(obj)
 *                       obj->field40 = 2
 *                       get_char_ani2(obj)
 *                       set_ignore_y(obj)
 *                       obj->field1c = 0 ; set_float_ani(obj)
 *                       obj->field1c = 5 ; ochar_sound(obj)
 *                       obj->field1c = obj->field08->field20 = -0.375
 *                       obj->a10     = 6.0
 *                       obj->field48 = 0xd0
 *                       -- falls into the descend --
 *
 *      token == 0xa02:  distance_from_ground(obj)
 *                       if (obj->field1c <= 0xbf) -- fall into the descend
 *                       else: back_to_normal(obj)
 *                             obj->field20 = obj->field00->field18 = 0x203
 *                             obj->field1c = 0 ; set_float_ani(obj)
 *                             find_part2(obj)
 *                             obj->field1c = obj->field08->field20 = 0.3125
 *                             token := 0xa11, descend into t_settle_down
 *
 *      token == 0xa11:  frame[frame].handler = t_main_hover_loop
 *
 *      the descend:     token := 0xa02, descend into t_hover_sleep_1
 *
 *      otherwise:       return -3
 *
 * **The two ways in share one exit.** Setting up and looping both end at the
 * same six instructions, so the entry state does not re-descend by jumping
 * anywhere -- it just stops writing fields and falls through. The C keeps that
 * shape rather than repeating the descend twice, because it is the shape:
 * `0x3e14e` has two predecessors and no others.
 *
 * `0xbf` here reads the OPPOSITE way to the `0xe0` in t_settle_down. There, a
 * distance above the threshold means keep waiting; here it means stop hovering
 * and start settling. Recorded, not reconciled -- the two are different phases
 * of the same move and nothing in either function says the field means the
 * same thing at both moments.
 *
 * 0x1c is written five times with five unrelated things: a zero for
 * set_float_ani, a 5 for ochar_sound, the -0.375 that is a velocity, whatever
 * distance_from_ground leaves, and finally 0.3125. It is the argument register
 * for everything here, not a field with a meaning.
 */
long tl_do_lia_fly(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0xa11)
        return mk3_install(thread, (MK3THREADFUNC)t_main_hover_loop);

    if (token == 0xa02) {
        distance_from_ground(obj);
        if ((long)obj->field1c > 0xbf) {        /* far enough: come down */
            back_to_normal(obj);
            obj->field20 = 0x203;
            obj->field00->field18 = 0x203;
            obj->field1c = 0;
            set_float_ani(obj);
            find_part2(obj);
            obj->field1c = 0x5000u;             /* 0.3125 */
            obj->field08->field20 = 0x5000u;

            *mk3_frame(thread, thread->frame + 1) = 0xa11;
            thread->frame = thread->frame + 1;
            mk3_frame(thread, thread->frame)[1] =
                (uint32_t)(uintptr_t)t_settle_down;
            *mk3_frame(thread, thread->frame + 1) = 0;
            return 0;
        }
    } else if (token != 0) {
        return -3;
    } else {
        obj->field20 = 0x203;
        init_special_act(obj);
        obj->field40 = 2;
        get_char_ani2(obj);
        set_ignore_y(obj);
        obj->field1c = 0;
        set_float_ani(obj);
        obj->field1c = 5;
        ochar_sound(obj);
        obj->field1c = 0xffffa000u;             /* -0.375 */
        obj->field08->field20 = 0xffffa000u;
        obj->a10 = 0x60000u;                    /* 6.0 */
        obj->field48 = 0xd0;
    }

    /* both ways in land here */
    *mk3_frame(thread, thread->frame + 1) = 0xa02;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_hover_sleep_1;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------- tl_do_tele_explode
 *
 * armv7 0x0003e790, 300 bytes.  **Complete.**
 *
 * Cyrax goes away in pieces and comes back next to you. Four states, three of
 * them parks, and the timing of the whole move is three numbers:
 *
 *      token == 0:      obj->field30 = obj->field08->field18   ; save the pair
 *                       obj->field34 = obj->field08->field1c
 *                       arg[f8++] = obj->field30
 *                       arg[f8++] = obj->field34
 *                       obj->field20 = obj->field00->field18 = 0x20a
 *                       air_init_special(obj)
 *                       obj->field34 = arg[--f8]               ; put it back
 *                       obj->field30 = arg[--f8]
 *                       obj->field08->field18 = obj->field30
 *                       obj->field08->field1c = obj->field34
 *                       obj->field08->field2c = 0x420
 *                       token := 0x70f, park 9
 *
 *      token == 0x70f:  obj->field1c = 8 ; create_fx(obj)
 *                       token := 0x712, park 1
 *
 *      token == 0x712:  stop_me_player(obj) ; set_nocol(obj)
 *                       token := 0x715, park 10
 *
 *      token == 0x715:  teleport_next_to(obj) ; face_opponent(obj)
 *                       frame[frame].handler = t_cyrax_implode
 *
 *      otherwise:       return -3
 *
 * **Nine frames, then one, then ten.** The effect is created on the tenth
 * frame and he is stopped and made non-colliding on the eleventh, which is one
 * frame after the flash rather than with it -- so the explosion is visible
 * before he stops being there. The teleport itself happens ten frames later
 * still, and by then nothing can touch him.
 *
 * The four words at 0x18 and 0x1c of the OTHER object are saved and restored
 * across air_init_special, which would otherwise overwrite them. They come
 * back through 0x30 and 0x34 -- the object's own scratch -- as well as the
 * argument stack, so the field pair is written twice and the stack pair once.
 * The second write is not redundant: air_init_special reads 0x30/0x34 while it
 * runs, so the copy has to be there and the restore has to come after.
 *
 * A park is a token plus `thread->fieldfc`, returned as the duration. All
 * three durations are returned in r0 as well as stored, which is how the
 * scheduler learns them.
 */
long tl_do_tele_explode(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x70f) {
        obj->field1c = 8;
        create_fx(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x712;
        thread->fieldfc = 1;
        return 1;
    }

    if (token == 0x712) {
        stop_me_player(obj);
        set_nocol(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x715;
        thread->fieldfc = 0xa;
        return 0xa;
    }

    if (token == 0x715) {
        teleport_next_to(obj);
        face_opponent(obj);
        return mk3_install(thread, (MK3THREADFUNC)t_cyrax_implode);
    }

    if (token != 0)
        return -3;

    obj->field30 = obj->field08->field18;
    obj->field34 = obj->field08->field1c;

    *mk3_arg(thread, thread->fieldf8) = obj->field30;
    thread->fieldf8 += 1;
    *mk3_arg(thread, thread->fieldf8) = obj->field34;
    thread->fieldf8 += 1;

    obj->field20 = 0x20a;
    obj->field00->field18 = 0x20a;
    air_init_special(obj);

    thread->fieldf8 -= 1;
    obj->field34 = *mk3_arg(thread, thread->fieldf8);
    thread->fieldf8 -= 1;
    obj->field30 = *mk3_arg(thread, thread->fieldf8);

    obj->field08->field18 = obj->field30;
    obj->field08->field1c = obj->field34;
    obj->field08->field2c = 0x420;

    *mk3_frame(thread, thread->frame + 1) = 0x70f;
    thread->fieldfc = 9;
    return 9;
}


/* ------------------------------------------------------------ t_sctele_calla_2
 *
 * armv7 0x0003ee5c, 308 bytes.  **Complete.**
 *
 * Scorpion's teleport punch from the caller's side. Throw the strike; on a hit
 * either the blocked routine takes over or he launches, and the launch is
 * timed differently depending on whether the opponent is already in the air.
 *
 *      token == 0:      sans_repell_3(obj)
 *                       obj->field1c = 0x17
 *                       strike_check_a0(obj)          ; answers in field5c
 *                       if (obj->field5c == 0) {      ; missed
 *                           if (thread->frame > 0) { frame -= 1; return 0 }
 *                           frame[frame].handler = t_local_reaction_exit
 *                       }
 *                       reset_proc_stack(thread)
 *                       if (obj->field18 != 0)
 *                           frame[frame].handler = t_scorpion_tele_blocked
 *                       else {
 *                           stop_me_player(obj)
 *                           obj->a10 = 6 ; obj->field48 = 0.5
 *                           is_he_airborn(obj)        ; answers in field5c
 *                           if (obj->field5c == 0) { obj->a10 = 4;
 *                                                    obj->field48 = 0.75 }
 *                           obj->field1c = obj->a10
 *                           token := 0x267, park obj->field1c
 *                       }
 *
 *      token == 0x267:  obj->field20 = 1.0
 *                       obj->field1c = 0
 *                       obj->field24 = obj->field48
 *                       obj->field28 = 0x1e
 *                       token := 0x26d, descend into t_flight
 *
 *      token == 0x26d:  frame[frame].handler = t_local_reaction_exit
 *
 *      otherwise:       return -3
 *
 * **The park duration is the launch parameter.** 0x44 is written with 6 or 4,
 * copied into 0x1c, and that number is BOTH the frames to wait and, through
 * 0x1c, what the state above reads. One field carrying two meanings at two
 * moments -- and 0x48 carries the matching speed, 0.5 with the six or 0.75
 * with the four. Longer wind-up, slower travel; shorter wind-up, faster.
 * A grounded opponent gets the quick version.
 *
 * `is_he_airborn` is one of the routines that answers through obj->field5c
 * rather than r0, and its result is read back the same way strike_check_a0's
 * is, two calls earlier in the same function. The zero case is the airborne
 * one -- the four goes in when field5c comes back CLEAR.
 *
 * Three of the four endings install and none may carry the state-0 refusal:
 * two of them run with a non-zero token, and the third is reached after the
 * binary has tested `frame > 0`, not the slot.
 */
long t_sctele_calla_2(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x267) {
        obj->field20 = 0x10000u;                /* 1.0 */
        obj->field1c = 0;
        obj->field24 = obj->field48;
        obj->field28 = 0x1e;

        *mk3_frame(thread, thread->frame + 1) = 0x26d;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x26d)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

    if (token != 0)
        return -3;

    sans_repell_3(obj);
    obj->field1c = 0x17;
    strike_check_a0(obj);

    if (obj->field5c == 0) {                    /* the strike missed */
        if ((long)thread->frame > 0) {
            thread->frame -= 1;
            return 0;
        }
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }

    reset_proc_stack(thread);

    if (obj->field18 != 0)
        return mk3_install(thread, (MK3THREADFUNC)t_scorpion_tele_blocked);

    stop_me_player(obj);
    obj->a10     = 6;
    obj->field48 = 0x8000u;                     /* 0.5 */
    is_he_airborn(obj);
    if (obj->field5c == 0) {                    /* he is on the ground */
        obj->a10     = 4;
        obj->field48 = 0xc000u;                 /* 0.75 */
    }

    obj->field1c = obj->a10;
    *mk3_frame(thread, thread->frame + 1) = 0x267;
    thread->fieldfc = obj->field1c;
    return (long)obj->field1c;
}


/* ------------------------------------------------------ t_shake_and_collision
 *
 * armv7 0x0003c6a8, 312 bytes.  **Complete.**
 *
 * A decoy standing there shuddering, and solid the whole time.
 *
 *      token == 0:      obj->field08's x-high += 3
 *                       obj->field48 = 3
 *                       token := 0xc9a, park 1
 *
 *      token == 0xc9a:  decoy_collision_check(obj)
 *                       if (obj->field5c != 0) -- touched, see below
 *                       if (--obj->field48 != 0) { token := 0xc9a, park 1 }
 *                       else { x-high -= 3 ; obj->field48 = 3
 *                              token := 0xca5, park 1 }
 *
 *      token == 0xca5:  decoy_collision_check(obj)
 *                       if (obj->field5c != 0) -- touched, see below
 *                       if (--obj->field48 != 0) { token := 0xca5, park 1 }
 *                       else if (--obj->field40 != 0)
 *                           frame[frame].handler = t_shake_and_collision
 *                       else -- finished, see below
 *
 *      touched, or finished:
 *                       if (thread->frame > 0) { frame -= 1; return 0 }
 *                       frame[frame].handler = t_local_reaction_exit
 *
 *      otherwise:       return -3
 *
 * **Three frames right, three frames left, N times.** 0x48 is the frame
 * counter within a half-swing and 0x40 counts the swings; the displacement is
 * a halfword add and subtract on the high half of 0x0c -- the integer part of
 * the horizontal position -- so the decoy jitters three pixels and comes back
 * exactly, with no drift, because the same 3 is added and taken away.
 *
 * The repeat is done by installing THIS function again, which clears the token
 * slot and puts it back in state 0 -- so the next swing re-adds the +3 through
 * the entry path rather than through a fourth state. Two states and a
 * reinstall do the work of four.
 *
 * **The collision test runs on every frame of the shake, before the counters.**
 * A decoy is not a decoration: walking into it during the shudder ends the
 * routine at once, and by the same exit the finished shake uses.
 *
 * The 0x0e access is a halfword, unsigned (`ldrh`/`strh`), on a struct with no
 * field there -- 0x0c is a word and this is its top half. Written as an offset
 * rather than invented as a field.
 */
long t_shake_and_collision(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    uint16_t *x_hi;

    if (token == 0xc9a) {
        decoy_collision_check(obj);
        if (obj->field5c != 0) {                /* somebody walked into it */
            if ((long)thread->frame > 0) {
                thread->frame -= 1;
                return 0;
            }
            return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
        }

        obj->field48 -= 1;
        if (obj->field48 == 0) {
            x_hi = (uint16_t *)((char *)obj->field08 + 0x0e);
            *x_hi = (uint16_t)(*x_hi - 3);      /* and back again */
            obj->field48 = 3;
            *mk3_frame(thread, thread->frame + 1) = 0xca5;
            thread->fieldfc = 1;
            return 1;
        }
        *mk3_frame(thread, thread->frame + 1) = 0xc9a;
        thread->fieldfc = 1;
        return 1;
    }

    if (token == 0xca5) {
        decoy_collision_check(obj);
        if (obj->field5c != 0) {
            if ((long)thread->frame > 0) {
                thread->frame -= 1;
                return 0;
            }
            return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
        }

        obj->field48 -= 1;
        if (obj->field48 != 0) {
            *mk3_frame(thread, thread->frame + 1) = 0xca5;
            thread->fieldfc = 1;
            return 1;
        }

        obj->field40 -= 1;
        if (obj->field40 != 0)                  /* one more swing */
            return mk3_install(thread,
                               (MK3THREADFUNC)t_shake_and_collision);

        if ((long)thread->frame > 0) {
            thread->frame -= 1;
            return 0;
        }
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }

    if (token != 0)
        return -3;

    x_hi = (uint16_t *)((char *)obj->field08 + 0x0e);
    *x_hi = (uint16_t)(*x_hi + 3);              /* three to the right */
    obj->field48 = 3;
    *mk3_frame(thread, thread->frame + 1) = 0xc9a;
    thread->fieldfc = 1;
    return 1;
}


/* ------------------------------------------------------------- tl_do_tusk_blur
 *
 * armv7 0x0003d2bc, 312 bytes.  **Complete.**
 *
 * The charge: twenty-six frames of running through the opponent, and the
 * strike only counts once.
 *
 *      token == 0:      obj->field20 = 0x213 ; init_special_act(obj)
 *                       obj->field40 = 1 ; get_char_ani2(obj)
 *                       obj->field1c = 13.0 ; towards_x_vel(obj)
 *                       obj->field1c = 1 ; init_anirate(obj)
 *                       obj->field1c = 2 ; ochar_sound(obj)
 *                       obj->field1c = obj->field00->field28 = 0
 *                       obj->a10 = 0x1a
 *                       token := 0x2f5, park 1
 *
 *      token == 0x2f5:  obj->field1c = obj->field00->field28
 *                       if (that was 0) {
 *                           obj->field1c = 0x12 ; strike_check_a0(obj)
 *                           if (obj->field5c != 0) {
 *                               if (obj->field18 != 0)
 *                                   frame[frame].handler = t_tusk_blur_blocked
 *                               obj->field1c = obj->field00->field28 = 1
 *                           }
 *                       }
 *                       sans_repell_3(obj) ; next_anirate(obj)
 *                       if (--obj->a10 > 0) { token := 0x2f5, park 1 }
 *                       obj->field40 = 1 ; find_ani2_part2(obj)
 *                       stop_me_player(obj) ; obj->field1c = 2
 *                       token := 0x30d, descend into t_mframew
 *
 *      token == 0x30d:  frame[frame].handler = t_local_reaction_exit
 *
 *      otherwise:       return -3
 *
 * **0x28 of the proc is a latch, not a counter.** It starts at zero, the
 * strike is only attempted while it is zero, and connecting sets it to one --
 * so a charge that passes through the opponent hits him once however many
 * frames the two of them overlap. The same field is cleared on the way in,
 * which is what makes the move repeatable.
 *
 * The countdown is in 0x44, the argument slot, used here as a plain frame
 * counter -- the same borrowing `tl_do_reptile_dash` does.
 *
 * **The install and the descend are the same six instructions.** 0x3d34c
 * loads a handler through a pointer slot and stores it at `frame[r2]`, and the
 * only difference between the two callers is whether 0xa4 was incremented
 * before jumping in: the 0x30d state arrives with r2 still holding the frame
 * it read at entry, and the descend arrives with r2 holding the frame it has
 * just written back. One tail, two meanings, told apart by an increment that
 * happened four instructions earlier.
 */
long tl_do_tusk_blur(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    uint32_t hit;

    if (token == 0x30d)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

    if (token == 0x2f5) {
        hit = obj->field00->field28;
        obj->field1c = hit;

        if (hit == 0) {
            obj->field1c = 0x12;
            strike_check_a0(obj);
            if (obj->field5c != 0) {
                if (obj->field18 != 0)          /* he blocked it */
                    return mk3_install(thread,
                                       (MK3THREADFUNC)t_tusk_blur_blocked);
                obj->field1c = 1;
                obj->field00->field28 = 1;      /* once is enough */
            }
        }

        sans_repell_3(obj);
        next_anirate(obj);

        obj->a10 -= 1;
        if ((long)obj->a10 > 0) {
            *mk3_frame(thread, thread->frame + 1) = 0x2f5;
            thread->fieldfc = 1;
            return 1;
        }

        obj->field40 = 1;
        find_ani2_part2(obj);
        stop_me_player(obj);
        obj->field1c = 2;

        *mk3_frame(thread, thread->frame + 1) = 0x30d;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0)
        return -3;

    obj->field20 = 0x213;
    init_special_act(obj);
    obj->field40 = 1;
    get_char_ani2(obj);
    obj->field1c = 0xd0000u;                    /* 13.0 */
    towards_x_vel(obj);
    obj->field1c = 1;
    init_anirate(obj);
    obj->field1c = 2;
    ochar_sound(obj);
    obj->field1c = 0;
    obj->field00->field28 = 0;
    obj->a10 = 0x1a;

    *mk3_frame(thread, thread->frame + 1) = 0x2f5;
    thread->fieldfc = 1;
    return 1;
}


/* ---------------------------------------------------------------- t_air_sleep3
 *
 * armv7 0x0003ed20, 316 bytes.  **Complete.**
 *
 * Fly at whatever 0x44 points at, one frame at a time, and give up when the
 * target lands or the patience runs out.
 *
 *      token == 0:      token := 0x7f6, park 1
 *
 *      token == 0x7f6:  next_anirate(obj)
 *                       dx = ((MK3OBJ *)obj->a10)->x_hi - obj->field08->x_hi
 *                       dy = ((MK3OBJ *)obj->a10)->y_hi - obj->field08->y_hi
 *                       obj->field20 = dx ; obj->field28 = dy
 *                       if (dx <= 1) obj->field20 = 2
 *                       obj->field20 <<= 16 ; obj->field38 = obj->field20
 *                       obj->field28 <<= 16 ; obj->field54 = obj->field28
 *                       get_rough_hypotenuse(obj)
 *                       obj->field54 >>= 16 ; if (0) obj->field54 = 1
 *                       obj->field20 = (obj->field20 / obj->field54) * 12
 *                       obj->field28 = (obj->field28 / obj->field54) * 12
 *                       obj->field08->field18 = obj->field20
 *                       obj->field08->field1c = obj->field28
 *                       obj->field1c = obj->field20
 *                       set_x_vel_player(obj)
 *                       is_he_airborn(obj)               ; answers in field5c
 *                       if (obj->field5c == 0)
 *                           frame[frame].handler = t_air_grab_cancel
 *                       get_x_dist(obj)
 *                       if (obj->field28 > 0x45)
 *                           frame[frame].handler = t_air_sleep3
 *                       else if (--obj->field48 > 0)
 *                           frame[frame].handler = t_air_sleep3
 *                       else
 *                           frame[frame].handler = t_air_grab_cancel
 *
 *      otherwise:       return -3
 *
 * **A normalised chase at a fixed speed of twelve.** The difference between
 * the two objects' integer positions is promoted to 16.16, its length comes
 * back from get_rough_hypotenuse through 0x54, and both components are divided
 * by that length and multiplied by twelve. The multiply is `(v << 4) - (v << 2)`
 * -- sixteen minus four -- and the divide is a call to `___divsi3`, because
 * this binary has no hardware integer divide.
 *
 * **Two guards, both against a division that cannot happen yet.** `dx <= 1`
 * forces the horizontal difference to 2 before anything is squared, and a
 * length that rounds to zero is forced to 1. Neither is a special case in the
 * flight: they are there so the two divides always have something to divide by.
 *
 * The loop is built out of reinstalling, so every frame re-enters through
 * state 0 and parks again -- three exits, two of which install this same
 * function and one of which hands over to t_air_grab_cancel. The patience is
 * 0x48, decremented only once the target is within 0x45 horizontally: he gets
 * unlimited time to close the distance and a fixed number of frames to finish.
 */
long t_air_sleep3(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    MK3OBJ  *target;
    int32_t  dx, dy, q;

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x7f6;
        thread->fieldfc = 1;
        return 1;
    }

    if (token != 0x7f6)
        return -3;

    next_anirate(obj);

    target = (MK3OBJ *)(uintptr_t)obj->a10;
    dx = *(int16_t *)((char *)target + 0x0e)
       - *(int16_t *)((char *)obj->field08 + 0x0e);
    dy = *(int16_t *)((char *)target + 0x12)
       - *(int16_t *)((char *)obj->field08 + 0x12);
    obj->field20 = (uint32_t)dx;
    obj->field28 = (uint32_t)dy;
    if (dx <= 1)
        obj->field20 = 2;                       /* never divide by nothing */

    obj->field20 = obj->field20 << 16;          /* integers -> 16.16 */
    obj->field38 = obj->field20;
    obj->field28 = obj->field28 << 16;
    obj->field54 = obj->field28;

    get_rough_hypotenuse(obj);

    obj->field54 = obj->field54 >> 16;
    if (obj->field54 == 0)
        obj->field54 = 1;

    q = mk3_sdiv((int32_t)obj->field20, (int32_t)obj->field54);   /* ___divsi3 */
    obj->field20 = (uint32_t)q;
    dy = mk3_sdiv((int32_t)obj->field28, (int32_t)obj->field54);   /* ___divsi3 */
    obj->field20 = (uint32_t)((q << 4) - (q << 2));      /* twelve */
    obj->field28 = (uint32_t)((dy << 4) - (dy << 2));

    obj->field08->field18 = obj->field20;
    obj->field08->field1c = obj->field28;
    obj->field1c = obj->field20;
    set_x_vel_player(obj);

    is_he_airborn(obj);
    if (obj->field5c == 0)                      /* he came down */
        return mk3_install(thread, (MK3THREADFUNC)t_air_grab_cancel);

    get_x_dist(obj);
    if ((long)obj->field28 > 0x45)              /* still closing */
        return mk3_install(thread, (MK3THREADFUNC)t_air_sleep3);

    obj->field48 -= 1;
    if ((long)obj->field48 > 0)
        return mk3_install(thread, (MK3THREADFUNC)t_air_sleep3);

    return mk3_install(thread, (MK3THREADFUNC)t_air_grab_cancel);
}


/* --------------------------------------------------------------- tl_ind_charge
 *
 * armv7 0x0003ddd8, 324 bytes.  **Complete.**
 *
 * The shoulder charge, in five states. Two frames of wind-up, two more, then
 * run at nine until either the strike lands or twenty frames go by.
 *
 *      token == 0:      obj->field20 = 0x214 ; init_special_act(obj)
 *                       obj->field40 = 4 ; get_char_ani2(obj)
 *                       obj->field1c = 8 ; ochar_sound(obj)
 *                       obj->field1c = 1 ; create_fx(obj)
 *                       clear_shadow_bit(obj)
 *                       obj->field1c = 1 ; towards_x_vel(obj)
 *                       do_next_a9_frame(obj)
 *                       token := 0xaa0, park 2
 *
 *      token == 0xaa0:  do_next_a9_frame(obj)
 *                       token := 0xaa2, park 2
 *
 *      token == 0xaa2:  obj->field1c = 9.0 ; towards_x_vel(obj)
 *                       do_next_a9_frame(obj)
 *                       obj->field48 = 0x14
 *                       token := 0xaaa, park 1
 *
 *      token == 0xaaa:  obj->field1c = 0x11 ; strike_check_a0(obj)
 *                       if (obj->field5c == 0 && --obj->field48 > 0)
 *                           token := 0xaaa, park 1
 *                       else {
 *                           stop_me_player(obj)
 *                           obj->field1c = obj->field00->field18 = 0x605
 *                           token := 0xab7, park 0x10
 *                       }
 *
 *      token == 0xab7:  obj->field40 = 4 ; find_ani2_part2(obj)
 *                       obj->field1c = 3
 *                       frame[frame].handler = t_mframew
 *
 *      otherwise:       return -3
 *
 * **Hitting and running out of time end identically.** The miss path falls
 * into the hit path's first instruction -- `b #0x3dee4` with nothing in
 * between -- so a charge that connects and a charge that expires both stop the
 * player, set action 0x605 and wait sixteen frames. Only the strike test
 * differs, and it differs by one branch.
 *
 * 0x1c is written 1 twice in the entry state, once before create_fx and again
 * before towards_x_vel, with clear_shadow_bit between them. The second store
 * is not redundant: the argument register has to be re-established because the
 * two calls in between are free to use it.
 *
 * The dispatch is a three-way compare rather than a chain: 0xaa2 first,
 * everything below it split into 0 and 0xaa0, everything above into 0xaaa and
 * 0xab7. Five states, three comparisons on the common path.
 */
long tl_ind_charge(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0xaa0) {
        do_next_a9_frame(obj);
        *mk3_frame(thread, thread->frame + 1) = 0xaa2;
        thread->fieldfc = 2;
        return 2;
    }

    if (token == 0xaa2) {
        obj->field1c = 0x90000u;                /* 9.0 */
        towards_x_vel(obj);
        do_next_a9_frame(obj);
        obj->field48 = 0x14;
        *mk3_frame(thread, thread->frame + 1) = 0xaaa;
        thread->fieldfc = 1;
        return 1;
    }

    if (token == 0xaaa) {
        obj->field1c = 0x11;
        strike_check_a0(obj);

        if (obj->field5c == 0) {                /* nothing there */
            obj->field48 -= 1;
            if ((long)obj->field48 > 0) {
                *mk3_frame(thread, thread->frame + 1) = 0xaaa;
                thread->fieldfc = 1;
                return 1;
            }
        }

        /* connected, or out of time -- the same ending either way */
        stop_me_player(obj);
        obj->field1c = 0x605;
        obj->field00->field18 = 0x605;
        *mk3_frame(thread, thread->frame + 1) = 0xab7;
        thread->fieldfc = 0x10;
        return 0x10;
    }

    if (token == 0xab7) {
        obj->field40 = 4;
        find_ani2_part2(obj);
        obj->field1c = 3;
        return mk3_install(thread, (MK3THREADFUNC)t_mframew);
    }

    if (token != 0)
        return -3;

    obj->field20 = 0x214;
    init_special_act(obj);
    obj->field40 = 4;
    get_char_ani2(obj);
    obj->field1c = 8;
    ochar_sound(obj);
    obj->field1c = 1;
    create_fx(obj);
    clear_shadow_bit(obj);
    obj->field1c = 1;
    towards_x_vel(obj);
    do_next_a9_frame(obj);

    *mk3_frame(thread, thread->frame + 1) = 0xaa0;
    thread->fieldfc = 2;
    return 2;
}


/* ------------------------------------------------------------- t_hover_sleep_1
 *
 * armv7 0x0003e214, 344 bytes.  **Complete.**
 *
 * The wait that t_settle_down and tl_do_lia_fly both descend into. It sleeps a
 * frame, then every frame asks whether the fighter is blocking, and the answer
 * decides whether the hover ends quietly or turns into a flight.
 *
 *      token == 0:      obj->field1c = 2
 *                       *(uint16_t *)(G + 0x456) = 2
 *                       token := 0x985, park 1
 *
 *      token == 0x985:  next_anirate(obj)
 *                       check_block_bit(obj)          ; answers in field5c
 *                       if (obj->field5c == 0) {
 *                           if (thread->frame > 0) { frame -= 1; return 0 }
 *                           frame[frame].handler = t_local_reaction_exit
 *                       }
 *                       if (thread->frame > 0) frame -= 1
 *                       else { frame[frame].handler = t_local_reaction_exit
 *                              frame[frame+1].w0 = 0 }
 *                       -- collapse one level, see below --
 *                       obj->field1c = 0 ; set_float_ani(obj)
 *                       face_opponent(obj)
 *                       obj->field1c = obj->field20 = 0xd
 *                       obj->field24 = 0.625 ; obj->field28 = 4
 *                       token := 0x996, descend into t_flight
 *
 *      token == 0x996:  reset_proc_stack(thread)
 *                       frame[frame].handler = t_jump_up_land_jump
 *
 *      otherwise:       return -3
 *
 * **The collapse is new here and appears nowhere else in the file:**
 *
 *      handler          = frame[frame+1].handler
 *      frame[frame+1].w0 = frame[frame+2].w0
 *      frame[frame].handler = handler
 *
 * It pulls the level above down into the current one -- the handler and the
 * token that goes with it -- so after `frame -= 1` the routine that WAS
 * running one level up is now running here. A return that keeps the callee
 * rather than the caller. It is the only place in mkprop.c that writes a frame
 * slot from another frame slot.
 *
 * **One oddity, transcribed rather than tidied.** When the fighter is blocking
 * and the frame index is already 0, the binary installs t_local_reaction_exit
 * at level 0 and then falls into the collapse, which immediately overwrites
 * that same handler with whatever level 1 holds. The store is dead on that
 * path. Both the install and the collapse are in the binary, in that order,
 * and nothing here explains why -- so both are here, in that order.
 *
 * 0x456 of G takes a 2 on the way in. The sans_repell family writes the same
 * halfword with other values; two is this move's.
 */
long t_hover_sleep_1(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    uint32_t below, handler;

    if (token == 0x996) {
        reset_proc_stack(thread);
        return mk3_install(thread, (MK3THREADFUNC)t_jump_up_land_jump);
    }

    if (token != 0x985) {
        if (token != 0)
            return -3;
        obj->field1c = 2;
        *(uint16_t *)(G_BYTES + 0x456) = 2;
        *mk3_frame(thread, thread->frame + 1) = 0x985;
        thread->fieldfc = 1;
        return 1;
    }

    next_anirate(obj);
    check_block_bit(obj);

    if (obj->field5c == 0) {                    /* not blocking: just leave */
        if ((long)thread->frame > 0) {
            thread->frame -= 1;
            return 0;
        }
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }

    if ((long)thread->frame > 0) {
        thread->frame -= 1;
    } else {
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_local_reaction_exit;
        *mk3_frame(thread, thread->frame + 1) = 0;
    }

    /* pull the level above down into this one, handler and token together */
    below   = thread->frame;
    handler = mk3_frame(thread, below + 1)[1];
    *mk3_frame(thread, below + 1) = *mk3_frame(thread, below + 2);
    mk3_frame(thread, below)[1] = handler;

    obj->field1c = 0;
    set_float_ani(obj);
    face_opponent(obj);
    obj->field1c = 0xd;
    obj->field20 = 0xd;
    obj->field24 = 0xa000u;                     /* 0.625 */
    obj->field28 = 4;

    *mk3_frame(thread, thread->frame + 1) = 0x996;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_flight;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------- tl_kano_cannon_ball
 *
 * armv7 0x0003d0cc, 344 bytes.  **Complete.**
 *
 * Kano rolls. Twenty-six frames, and the first six of them cannot hurt anyone.
 *
 *      token == 0:      init_special(obj)
 *                       obj->field1c = 4 ; ochar_sound(obj)
 *                       set_noflip(obj)
 *                       obj->field00->field18 = 0x20b
 *                       obj->field1c = 10.0 ; towards_x_vel(obj)
 *                       obj->field1c = 0 ; obj->field20 = 0x10
 *                       multi_adjust_xy(obj)
 *                       obj->field40 = 0x1a ; get_char_ani(obj)
 *                       obj->field40 += 4
 *                       obj->field1c = 3 ; init_anirate(obj)
 *                       obj->field48 = 0x1a
 *                       token := 0xbc0, park 1
 *
 *      token == 0xbc0:  if (obj->field48 <= 0x14) {
 *                           obj->field1c = 0x13 ; strike_check_a0(obj)
 *                           if (obj->field5c != 0) {
 *                               obj->field00->field18 = 0x602
 *                               obj->field1c = 5.0
 *                               obj->field20 = -8.0
 *                               if (obj->field18 != 0) obj->field1c = 1.0
 *                               obj->field24 = 0.375 ; obj->field28 = 2
 *                               token := 0xbe6, descend into t_flight
 *                           }
 *                       }
 *                       next_anirate(obj)
 *                       if (--obj->field48 > 0) { token := 0xbc0, park 1 }
 *                       stop_me_player(obj) ; ground_player(obj)
 *                       frame[frame].handler = t_local_reaction_exit
 *
 *      token == 0xbe6:  frame[frame].handler = t_jump_up_land_jsrp
 *
 *      otherwise:       return -3
 *
 * **The roll has a six-frame nose.** 0x48 counts down from 0x1a and the strike
 * is only attempted once it is 0x14 or less, so the first six frames are Kano
 * getting down and moving, and they pass through the opponent harmlessly. The
 * counter is one thing doing two jobs: the wind-up gate and the length of the
 * whole move.
 *
 * **The launch on a hit is graded by 0x18.** Both cases fly at -8.0 with the
 * same 0.375 and the same 2; only 0x1c differs, 5.0 against 1.0, and the small
 * one is for when 0x18 is set. Same arc, a fifth of the something -- one
 * constant is the whole difference between the two outcomes.
 *
 * `obj->field40` is written 0x1a, handed to get_char_ani, and then has 4 added
 * to whatever came back. The add is on the field, not on the literal: the call
 * is free to change it and the +4 applies to the result.
 */
long tl_kano_cannon_ball(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0xbe6)
        return mk3_install(thread, (MK3THREADFUNC)t_jump_up_land_jsrp);

    if (token == 0xbc0) {
        if ((long)obj->field48 <= 0x14) {       /* past the nose */
            obj->field1c = 0x13;
            strike_check_a0(obj);
            if (obj->field5c != 0) {
                obj->field00->field18 = 0x602;
                obj->field1c = 0x50000u;        /* 5.0 */
                obj->field20 = 0xfff80000u;     /* -8.0 */
                if (obj->field18 != 0)
                    obj->field1c = 0x10000u;    /* 1.0 */
                obj->field24 = 0x6000u;         /* 0.375 */
                obj->field28 = 2;

                *mk3_frame(thread, thread->frame + 1) = 0xbe6;
                thread->frame = thread->frame + 1;
                mk3_frame(thread, thread->frame)[1] =
                    (uint32_t)(uintptr_t)t_flight;
                *mk3_frame(thread, thread->frame + 1) = 0;
                return 0;
            }
        }

        next_anirate(obj);
        obj->field48 -= 1;
        if ((long)obj->field48 > 0) {
            *mk3_frame(thread, thread->frame + 1) = 0xbc0;
            thread->fieldfc = 1;
            return 1;
        }

        stop_me_player(obj);
        ground_player(obj);
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }

    if (token != 0)
        return -3;

    init_special(obj);
    obj->field1c = 4;
    ochar_sound(obj);
    set_noflip(obj);
    obj->field00->field18 = 0x20b;
    obj->field1c = 0xa0000u;                    /* 10.0 */
    towards_x_vel(obj);
    obj->field1c = 0;
    obj->field20 = 0x10;
    multi_adjust_xy(obj);
    obj->field40 = 0x1a;
    get_char_ani(obj);
    obj->field40 = obj->field40 + 4;
    obj->field1c = 3;
    init_anirate(obj);
    obj->field48 = 0x1a;

    *mk3_frame(thread, thread->frame + 1) = 0xbc0;
    thread->fieldfc = 1;
    return 1;
}


/* ----------------------------------------------------------- t_main_hover_loop
 *
 * armv7 0x0003df88, 352 bytes.  **Complete.**
 *
 * The hover itself: one frame of flying under stick control, then back to
 * sleep. It runs once per frame and reinstalls itself through 0xa79.
 *
 *      token == 0:      distance_from_ground(obj)
 *                       obj->field30 = obj->field1c        ; the height
 *                       is_stick_right(obj)
 *                       if (right)      obj->field48 = 0.5
 *                       else { is_stick_left(obj)
 *                              if (left) obj->field48 = -0.5 }
 *                       if (right || left) {
 *                           accelerate_x(obj) ; flight_move_ani(obj)
 *                       } else {
 *                           -- bleed the horizontal velocity, then the
 *                              action check, see below --
 *                       }
 *                       obj->field34 = 0.25
 *                       is_stick_up(obj)
 *                       -- the altitude bands, see below --
 *                       obj->field08->field20 = obj->field34
 *                       clamp obj->field08->field1c to +/-3.0
 *                       token := 0xa79, descend into t_hover_sleep_1
 *
 *      token == 0xa79:  frame[frame].handler = t_main_hover_loop
 *
 *      otherwise:       return -3
 *
 * **The altitude controller has a dead band two units wide.**
 *
 *      stick up      height > 0xdf    ->  +0.125     (already high: sink)
 *      stick up      otherwise        ->  -0.75      (climb hard)
 *      no stick      height <= 0xd5   ->  -0.1875    (drifted low: rise)
 *      no stick      0xd6 .. 0xd7     ->  nothing at all
 *      no stick      height > 0xd7    ->  +0.1875    (drifted high: fall)
 *
 * In the dead band the routine does not write 0x34 AND does not run the block
 * that copies it out or clamps the horizontal speed -- it branches straight to
 * the descend. So two units of height cost nothing to hold, which is what
 * makes a hover look still rather than hunting.
 *
 * **The horizontal bleed keeps only one value.** With no stick, the velocity
 * at obj->field08->field18 is shifted right four and put in 0x20; if that
 * shift did not come out -1 the WHOLE value is put there instead, and 0x1c
 * becomes the value minus 0x20 -- zero in every case except the one. So a
 * velocity in [-16,-1] decays by one a frame and everything else stops dead.
 * The asymmetry is in the binary (`asrs` then `cmp #-1`), not in the reading,
 * and nothing here says why it is there.
 *
 * The action at proc+0x30 selects an animation: 1 gives 0x1c = 3 and 2 gives
 * 0x1c = 4, both followed by set_float_ani; 0 and 3 do nothing; anything else
 * writes the 4 and makes no call. Proc has no named field at 0x30, so it is
 * reached as an offset.
 */
long t_main_hover_loop(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    int32_t  v, act;
    int      moved, settle;

    if (token != 0) {
        if (token != 0xa79)
            return -3;
        return mk3_install(thread, (MK3THREADFUNC)t_main_hover_loop);
    }

    distance_from_ground(obj);
    obj->field30 = obj->field1c;

    moved = 0;
    is_stick_right(obj);
    if (obj->field5c != 0) {
        obj->field48 = 0x8000u;                 /* 0.5 */
        moved = 1;
    } else {
        is_stick_left(obj);
        if (obj->field5c != 0) {
            obj->field48 = 0xffff8000u;         /* -0.5 */
            moved = 1;
        }
    }

    if (moved) {
        accelerate_x(obj);
        flight_move_ani(obj);
    } else {
        v = (int32_t)obj->field08->field18;
        obj->field1c = (uint32_t)v;
        if (v != 0) {
            obj->field20 = (uint32_t)(v >> 4);
            if ((int32_t)obj->field20 != -1)
                obj->field20 = (uint32_t)v;
            obj->field1c = obj->field1c - obj->field20;
            set_x_vel_player(obj);
        }

        act = *(int32_t *)((char *)obj->field00 + 0x30);
        obj->field24 = (uint32_t)act;
        if (act != 0 && act != 3) {
            obj->field1c = 3;
            if (act != 1)
                obj->field1c = 4;
            if (act == 1 || act == 2)
                set_float_ani(obj);
        }
    }

    obj->field34 = 0x4000u;                     /* 0.25 */
    settle = 1;
    is_stick_up(obj);
    if (obj->field5c != 0) {
        obj->field34 = 0xffff4000u;             /* -0.75 */
        if ((long)obj->field30 > 0xdf)
            obj->field34 = 0x2000u;             /* 0.125 */
    } else if ((long)obj->field30 <= 0xd5) {
        obj->field34 = 0xffffd000u;             /* -0.1875 */
    } else if ((long)obj->field30 <= 0xd7) {
        settle = 0;                             /* the dead band */
    } else {
        obj->field34 = 0x3000u;                 /* 0.1875 */
    }

    if (settle) {
        obj->field08->field20 = obj->field34;

        v = (int32_t)obj->field08->field1c;
        obj->field20 = 0x30000u;                /* 3.0 */
        obj->field1c = (uint32_t)v;
        if (v > 0x30000) {
            obj->field08->field1c = obj->field20;
        } else {
            obj->field20 = 0x30000u - 0x60000u; /* -3.0 */
            if (v < (int32_t)obj->field20)
                obj->field08->field1c = obj->field20;
        }
    }

    *mk3_frame(thread, thread->frame + 1) = 0xa79;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_hover_sleep_1;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ----------------------------------------------------------- tl_do_stick_sweep
 *
 * armv7 0x0003cd8c, 360 bytes.  **Complete.**
 *
 * A sweep with the stick, and the recovery depends on whether it landed.
 *
 *      token == 0:      obj->field1c = &G + 0x418      ; an ADDRESS
 *                       update_tsl(obj)
 *                       obj->field20 = 0x212 ; init_special_act(obj)
 *                       obj->field1c = 3 ; ochar_sound(obj)
 *                       obj->field1c = 6.0 ; towards_x_vel(obj)
 *                       obj->field40 = 2 ; get_char_ani2(obj)
 *                       obj->field1c = 4
 *                       token := 0x345, descend into t_mframew
 *
 *      token == 0x345:  stop_me_player(obj)
 *                       obj->field1c = 0x17 ; strike_check_a0(obj)
 *                       if (obj->field5c != 0 && obj->field18 == 0) {
 *                           token := 0x34f, park 6
 *                       } else {
 *                           obj->field1c = obj->field00->field18 = 0x614
 *                           do_next_a9_frame(obj)
 *                           token := 0x35a, park 0x20
 *                       }
 *
 *      token == 0x34f:  do_next_a9_frame(obj)
 *                       token := 0x352, park 0x10
 *
 *      token == 0x352:  obj->field1c = 4
 *                       frame[frame].handler = t_mframew
 *
 *      token == 0x35a:  obj->field1c = 4
 *                       frame[frame].handler = t_mframew
 *
 *      otherwise:       return -3
 *
 * **A clean hit recovers in 6 + 16 frames; a miss or a block costs 32.** The
 * two endings are the same two instructions and the binary shares the tail
 * between them, so 0x352 and 0x35a differ only in how long the fighter stood
 * there first. Landing the sweep is ten frames cheaper than not landing it.
 *
 * A block is treated exactly as a miss: `obj->field18 != 0` branches into the
 * miss path's first instruction, not past it.
 *
 * **0x1c holds an address here.** `&G + 0x418` goes into the argument slot and
 * update_tsl reads it from there -- the same field that carries a sound index
 * three instructions later and a 16.16 speed three after that. The slot has no
 * type; it has whatever the next call expects.
 */
long tl_do_stick_sweep(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x34f) {
        do_next_a9_frame(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x352;
        thread->fieldfc = 0x10;
        return 0x10;
    }

    if (token == 0x352 || token == 0x35a) {
        obj->field1c = 4;
        return mk3_install(thread, (MK3THREADFUNC)t_mframew);
    }

    if (token == 0x345) {
        stop_me_player(obj);
        obj->field1c = 0x17;
        strike_check_a0(obj);

        if (obj->field5c != 0 && obj->field18 == 0) {
            *mk3_frame(thread, thread->frame + 1) = 0x34f;
            thread->fieldfc = 6;
            return 6;
        }

        /* missed, or he blocked it -- the same ending */
        obj->field1c = 0x614;
        obj->field00->field18 = 0x614;
        do_next_a9_frame(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x35a;
        thread->fieldfc = 0x20;
        return 0x20;
    }

    if (token != 0)
        return -3;

    obj->field1c = (uint32_t)(uintptr_t)(G_BYTES + 0x418);
    update_tsl(obj);
    obj->field20 = 0x212;
    init_special_act(obj);
    obj->field1c = 3;
    ochar_sound(obj);
    obj->field1c = 0x60000u;                    /* 6.0 */
    towards_x_vel(obj);
    obj->field40 = 2;
    get_char_ani2(obj);
    obj->field1c = 4;

    *mk3_frame(thread, thread->frame + 1) = 0x345;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* -------------------------------------------------------------- tl_do_sz_decoy
 *
 * armv7 0x0003c250, 360 bytes.  **Complete.**
 *
 * Sub-Zero leaves a copy of himself standing there and flies off. This is the
 * side that makes the decoy; `t_decoy_proc` is what the decoy then runs, and
 * `decoy_collision_check` is what the decoy uses to notice being touched.
 *
 *      token == 0:      air_init_special(obj)
 *                       obj->field20 = obj->field00->field18 = 0x202
 *                       obj->field1c = &G + 0x3f8 ; update_tsl(obj)
 *                       decoy = NewThreadProc(obj, t_decoy_proc)
 *                       get_x_dist(obj)
 *                       decoy->field48       = obj->field28
 *                       decoy->field08->field2c = obj->field08->field2c
 *                       decoy->thread->pid   = obj->field00->field08 + 0x204
 *                       obj->field1c = 4 ; ochar_sound(obj)
 *                       obj->field40 = 0x1b ; get_char_ani(obj)
 *                       obj->field40 += 4
 *                       obj->field1c = 6.0
 *                       obj->field20 = -6.0
 *                       obj->field24 = 0.5
 *                       obj->field28 = 3
 *                       token := 0xd35, descend into t_flight
 *
 *      token == 0xd35:  obj->field20 = obj->field00->field18 = 0x304
 *                       face_opponent(obj)
 *                       obj->field40 = 0x1a ; get_char_ani(obj)
 *                       do_next_a9_frame(obj)
 *                       token := 0xd3f, park 3
 *
 *      token == 0xd3f:  MKEvent_Add(3, 0xd, 0, obj->field00->field08)
 *                       if (thread->frame > 0) { frame -= 1; return 0 }
 *                       frame[frame].handler = t_local_reaction_exit
 *
 *      otherwise:       return -3
 *
 * **Three things are handed to the decoy and nothing else is.** The horizontal
 * distance to the opponent, one word from the part at 0x2c, and a pid built as
 * `obj->field00->field08 + 0x204`. The decoy is otherwise a fresh object
 * running its own thread; everything it needs to shake in the right place and
 * be identified as belonging to this fighter goes across in those three
 * stores, immediately after NewThreadProc and before anything else can run.
 *
 * The pid is derived, not copied: the strength index at proc+0x08 plus 0x204.
 * So the decoy's pid moves with the fighter's strength, and two fighters can
 * never produce the same one.
 *
 * The launch is one literal and two steps -- 0x60000, minus 0xc0000, plus
 * 0x68000 -- giving 6.0 across, -6.0 up and 0.5 of whatever 0x24 is. The
 * assembler only ever spelled the six.
 */
long tl_do_sz_decoy(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    MK3OBJ  *decoy;

    if (token == 0xd35) {
        obj->field20 = 0x304;
        obj->field00->field18 = 0x304;
        face_opponent(obj);
        obj->field40 = 0x1a;
        get_char_ani(obj);
        do_next_a9_frame(obj);
        *mk3_frame(thread, thread->frame + 1) = 0xd3f;
        thread->fieldfc = 3;
        return 3;
    }

    if (token == 0xd3f) {
        MKEvent_Add(3, 0xd, 0, (long)obj->field00->field08);
        if ((long)thread->frame > 0) {
            thread->frame -= 1;
            return 0;
        }
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }

    if (token != 0)
        return -3;

    air_init_special(obj);
    obj->field20 = 0x202;
    obj->field00->field18 = 0x202;
    obj->field1c = (uint32_t)(uintptr_t)(G_BYTES + 0x3f8);
    update_tsl(obj);

    decoy = NewThreadProc(obj, (MK3THREADFUNC)t_decoy_proc);
    get_x_dist(obj);
    decoy->field48 = obj->field28;
    decoy->field08->field2c = obj->field08->field2c;
    decoy->thread->pid = obj->field00->field08 + 0x204;

    obj->field1c = 4;
    ochar_sound(obj);
    obj->field40 = 0x1b;
    get_char_ani(obj);
    obj->field40 = obj->field40 + 4;

    obj->field1c = 0x60000u;                    /* 6.0 */
    obj->field20 = 0x60000u - 0xc0000u;         /* -6.0 */
    obj->field24 = (0x60000u - 0xc0000u) + 0x68000u;    /* 0.5 */
    obj->field28 = 3;

    *mk3_frame(thread, thread->frame + 1) = 0xd35;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_flight;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------------- t_decoy_proc
 *
 * armv7 0x0003c538, 368 bytes.  **Complete.**
 *
 * What the decoy runs. `tl_do_sz_decoy` spawns it and hands it three things;
 * this is the whole life of the copy afterwards.
 *
 *      token == 0:      if (obj->field48 > 0x4f) {
 *                           player_froze_pal(obj)
 *                           obj->field1c = 3 ; create_fx(obj)
 *                           obj->a10 = 0xa
 *                           -- NUDGE --
 *                       } else -- FLASH AND STOP --
 *
 *      token == 0xcd8:  if (--obj->a10 != 0) -- NUDGE --
 *                       q_is_he_a_boss(obj)
 *                       if (obj->field5c != 0) -- GO AWAY --
 *                       obj->field48 = 0x2a
 *                       is_he_joy(obj)
 *                       if (obj->field5c == 0) obj->field48 = 0x20
 *                       -- WATCH --
 *
 *      token == 0xcea:  decoy_collision_check(obj)
 *                       if (obj->field5c != 0) -- GO AWAY --
 *                       if (--obj->field48 != 0) -- WATCH --
 *                       obj->field40 = 6
 *                       token := 0xcf3, descend into t_shake_and_collision
 *
 *      token == 0xcf3:  -- GO AWAY --
 *      token == 0xcfd:  token := 0xd03, return 0x16462 (delete)
 *      token == 0xd03:  -- FLASH AND STOP --
 *      otherwise:       return -3
 *
 *      NUDGE          obj->field1c = -4 ; obj->field20 = 0
 *                     multi_adjust_xy(obj) ; token := 0xcd8, park 1
 *      WATCH          token := 0xcea, park 1
 *      GO AWAY        obj->field1c = 4 ; ochar_sound(obj)
 *                     obj->field1c = 3 ; create_fx(obj)
 *                     token := 0xcfd, park 0xc
 *      FLASH AND STOP obj->field1c = 3 ; create_fx(obj)
 *                     token := 0xd09, return 0x16462 (delete)
 *
 * **How long the decoy stands there is decided by who the opponent is.**
 * 0x2a frames normally and 0x20 if `is_he_joy` comes back clear -- ten frames
 * shorter -- and a boss gets no decoy at all: it goes away the moment the
 * ten-frame nudge finishes.
 *
 * **0x16462 is park-and-never-wake**, and it appears twice with different
 * tokens. The 0xcfd state parks with 0xd03, which this dispatch DOES accept,
 * so waking would flash and park again; FLASH AND STOP parks with 0xd09, which
 * it does not, so waking would return -3. Ninety-one thousand frames is a
 * quarter of an hour -- neither ever happens inside a round, and the two
 * spellings amount to the same thing.
 *
 * The shake gets `obj->field40 = 6` written one instruction before the descend,
 * and t_shake_and_collision counts its swings down in exactly that field. The
 * decoy shudders six times and then this routine takes it away.
 *
 * The dispatch's low branch reuses the register still holding 0xcea, so the
 * value it tests is 0xcea - 0x12 = 0xcd8 -- NOT 0xcf3 - 0x12. Getting that
 * wrong invents a state the routine does not have.
 */
long t_decoy_proc(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    int      gone  = 0;

    if (token == 0xcea) {
        decoy_collision_check(obj);
        if (obj->field5c == 0) {
            obj->field48 -= 1;
            if (obj->field48 == 0) {
                obj->field40 = 6;               /* six swings */
                *mk3_frame(thread, thread->frame + 1) = 0xcf3;
                thread->frame = thread->frame + 1;
                mk3_frame(thread, thread->frame)[1] =
                    (uint32_t)(uintptr_t)t_shake_and_collision;
                *mk3_frame(thread, thread->frame + 1) = 0;
                return 0;
            }
            *mk3_frame(thread, thread->frame + 1) = 0xcea;      /* WATCH */
            thread->fieldfc = 1;
            return 1;
        }
        gone = 1;                               /* somebody touched it */
    } else if (token == 0xcd8) {
        obj->a10 -= 1;
        if (obj->a10 != 0)
            goto nudge;

        q_is_he_a_boss(obj);
        if (obj->field5c == 0) {
            obj->field48 = 0x2a;
            is_he_joy(obj);
            if (obj->field5c == 0)
                obj->field48 = 0x20;            /* ten frames shorter */
            *mk3_frame(thread, thread->frame + 1) = 0xcea;      /* WATCH */
            thread->fieldfc = 1;
            return 1;
        }
        gone = 1;                               /* a boss gets no decoy */
    }

    if (gone || token == 0xcf3) {               /* GO AWAY */
        obj->field1c = 4;
        ochar_sound(obj);
        obj->field1c = 3;
        create_fx(obj);
        *mk3_frame(thread, thread->frame + 1) = 0xcfd;
        thread->fieldfc = 0xc;
        return 0xc;
    }

    if (token == 0xcfd) {
        *mk3_frame(thread, thread->frame + 1) = 0xd03;
        thread->fieldfc = 0x16462;              /* never */
        return 0x16462;
    }

    if (token == 0xd03)
        goto flash_and_stop;

    if (token != 0 && token != 0xcea && token != 0xcd8)
        return -3;

    if ((long)obj->field48 > 0x4f) {
        player_froze_pal(obj);
        obj->field1c = 3;
        create_fx(obj);
        obj->a10 = 0xa;
        goto nudge;
    }

    /* Two tails the binary shares: NUDGE (state 0 and 0xcd8) and FLASH AND
     * STOP (state 0 and 0xd03); written once each, as there. */
flash_and_stop:
    obj->field1c = 3;
    create_fx(obj);
    *mk3_frame(thread, thread->frame + 1) = 0xd09;
    thread->fieldfc = 0x16462;
    return 0x16462;

nudge:
    obj->field1c = (uint32_t)-4;
    obj->field20 = 0;
    multi_adjust_xy(obj);
    *mk3_frame(thread, thread->frame + 1) = 0xcd8;
    thread->fieldfc = 1;
    return 1;
}


/* -------------------------------------------------------------- tl_do_sg_quake
 *
 * armv7 0x00040308, 400 bytes.  **Complete.**
 *
 * Jump up, come down hard, and shake everyone standing on the floor.
 *
 *      token == 0:      obj->field20 = 0x118 ; init_special_act(obj)
 *                       obj->field40 = 0 ; get_char_ani2(obj)
 *                       obj->field1c = 0
 *                       obj->field20 = -10.0
 *                       obj->field24 = 0.75
 *                       obj->field28 = 2
 *                       token := 0x483, descend into t_flight
 *
 *      token == 0x483:  obj->field1c = 1 ; ochar_sound(obj)
 *                       obj->field48 = 0x80008 ; shake_a11(obj)
 *                       obj->field48 = 0
 *                       is_he_airborn(obj)
 *                       if (!airborne) {
 *                           q_is_he_a_boss(obj)
 *                           if (!boss) {
 *                               obj->field1c = 0x19 ; damage_to_him(obj)
 *                               obj->field38 = t_r_quake
 *                               takeover_him(obj)
 *                               obj->field48 = 1
 *                           }
 *                       }
 *                       obj->field1c = obj->field00->field18 = 0x621
 *                       obj->field40 = 0 ; get_char_ani2(obj)
 *                       find_part2(obj) ; obj->field1c = 3
 *                       token := 0x4a4, descend into t_mframew
 *
 *      token == 0x4a4:  obj->field1c = (obj->field48 != 0) ? 0x10 : 0x1c
 *                       token := 0x4ac, park obj->field1c
 *
 *      token == 0x4ac:  do_next_a9_frame(obj)
 *                       token := 0x4ae, park 4
 *
 *      token == 0x4ae:  frame[frame].handler = t_local_reaction_exit
 *
 *      otherwise:       return -3
 *
 * **0x48 is the shake argument and then the receipt.** It carries 0x80008 --
 * the packed (8, 8) shake_a11 wants -- is cleared immediately after, and is
 * then set to 1 only if the quake actually took the opponent over. Two states
 * later that same field decides the recovery: 0x10 frames if it is set, 0x1c
 * if it is not. Connecting is twelve frames cheaper, and the field that says
 * so was an argument to a different function a moment earlier.
 *
 * **Being airborne and being a boss are the same defence.** Either one skips
 * damage, the takeover and the receipt, by branching to the same instruction.
 * There is no separate handling for a boss who happens to be in the air.
 *
 * The launch is the pool word 0xfff60000 with 0xac000 added, wrapping to
 * 0xc000: -10.0 up and 0.75 of gravity, exactly the pair t_pounce_jsrp uses.
 * The wrap is deliberate -- one constant spelled once.
 *
 * The dispatch's low branch tests against the register still holding 0x4a4,
 * so the value is 0x4a4 - 0x21 = 0x483, matching what state 0 parks with.
 */
long tl_do_sg_quake(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x4a4) {
        obj->field1c = 0x1c;
        if (obj->field48 != 0)
            obj->field1c = 0x10;                /* it landed: recover sooner */
        *mk3_frame(thread, thread->frame + 1) = 0x4ac;
        thread->fieldfc = obj->field1c;
        return (long)obj->field1c;
    }

    if (token == 0x4ac) {
        do_next_a9_frame(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x4ae;
        thread->fieldfc = 4;
        return 4;
    }

    if (token == 0x4ae)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

    if (token == 0x483) {
        obj->field1c = 1;
        ochar_sound(obj);
        obj->field48 = 0x80008u;                /* (8, 8) */
        shake_a11(obj);
        obj->field48 = 0;

        is_he_airborn(obj);
        if (obj->field5c == 0) {
            q_is_he_a_boss(obj);
            if (obj->field5c == 0) {            /* on the floor, and no boss */
                obj->field1c = 0x19;
                damage_to_him(obj);
                obj->field38 = (uint32_t)(uintptr_t)t_r_quake;
                takeover_him(obj);
                obj->field48 = 1;               /* the receipt */
            }
        }

        obj->field1c = 0x621;
        obj->field00->field18 = 0x621;
        obj->field40 = 0;
        get_char_ani2(obj);
        find_part2(obj);
        obj->field1c = 3;

        *mk3_frame(thread, thread->frame + 1) = 0x4a4;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0)
        return -3;

    obj->field20 = 0x118;
    init_special_act(obj);
    obj->field40 = 0;
    get_char_ani2(obj);
    obj->field1c = 0;
    obj->field20 = 0xfff60000u;                 /* -10.0 */
    obj->field24 = 0xfff60000u + 0xac000u;      /* wraps to 0.75 */
    obj->field28 = 2;

    *mk3_frame(thread, thread->frame + 1) = 0x483;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_flight;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ---------------------------------------------------------- t_reptile_dash_hit
 *
 * armv7 0x0003fed8, 440 bytes.  **Complete.**
 *
 * Reptile's dash, from the first frame to the recovery. Seven states, and the
 * frame counter at 0x44 is reused for two different lengths.
 *
 *      token == 0:      if (obj->field18 != 0) {          ; he blocked it
 *                           blur_blocked_setup(obj)
 *                           obj->field40 = 0x30046
 *                           pose_a9_manual(obj)
 *                           frame[frame].handler = t_blb8
 *                       }
 *                       obj->a10 = 0xe
 *                       token := 0x1fa, park 1
 *
 *      token == 0x1fa:  do_next_a9_frame(obj) ; sans_repell_3(obj)
 *                       if (--obj->a10 > 0) { token := 0x1fa, park 1 }
 *                       stop_me_player(obj)
 *                       token := 0x203, park 6
 *
 *      token == 0x203:  face_opponent(obj)
 *                       obj->field40 = 0xb ; get_char_ani2(obj)
 *                       obj->field1c = 2
 *                       token := 0x209, descend into t_mframew
 *
 *      token == 0x209:  obj->field1c = 13.0 ; towards_x_vel(obj)
 *                       obj->a10 = 0x14
 *                       token := 0x20f, park 1
 *
 *      token == 0x20f:  obj->field1c = 0x16 ; strike_check_a0(obj)
 *                       if (obj->field5c == 0 && --obj->a10 > 0)
 *                           { token := 0x20f, park 1 }
 *                       stop_me_player(obj)
 *                       token := 0x21a, park 0x14
 *
 *      token == 0x21a:  obj->field1c = 3
 *                       token := 0x21d, descend into t_mframew
 *
 *      token == 0x21d:  frame[frame].handler = t_local_reaction_exit
 *
 *      otherwise:       return -3
 *
 * **The block is decided before anything else happens.** State 0 reads 0x18
 * first, and a non-zero there ends the move on its first frame -- pose, hand
 * over to t_blb8, and the dash never starts. Everything below assumes it did.
 *
 * **0x44 counts two different things.** Fourteen frames of wind-up in the
 * first half and twenty frames of travel in the second, in the same field,
 * with two separate `park 1` loops around it. Nothing distinguishes them but
 * the token, which is what the token is for.
 *
 * **Hitting and running out of travel share an ending**: connecting jumps
 * straight to the stop-and-park, and so does the counter reaching zero. As in
 * tl_ind_charge, the fighter pays the same recovery either way.
 *
 * The dispatch reuses the register holding 0x209 on its low branch and
 * reloads it with 0x21a on the high one, so state 0x203 parks with 0x209 and
 * state 0x20f parks with 0x21a -- the same register, two values, decided by
 * which side of the first comparison the token fell.
 */
long t_reptile_dash_hit(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x1fa) {
        do_next_a9_frame(obj);
        sans_repell_3(obj);
        obj->a10 -= 1;
        if ((long)obj->a10 > 0) {
            *mk3_frame(thread, thread->frame + 1) = 0x1fa;
            thread->fieldfc = 1;
            return 1;
        }
        stop_me_player(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x203;
        thread->fieldfc = 6;
        return 6;
    }

    if (token == 0x203) {
        face_opponent(obj);
        obj->field40 = 0xb;
        get_char_ani2(obj);
        obj->field1c = 2;

        *mk3_frame(thread, thread->frame + 1) = 0x209;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x209) {
        obj->field1c = 0xd0000u;                /* 13.0 */
        towards_x_vel(obj);
        obj->a10 = 0x14;
        *mk3_frame(thread, thread->frame + 1) = 0x20f;
        thread->fieldfc = 1;
        return 1;
    }

    if (token == 0x20f) {
        obj->field1c = 0x16;
        strike_check_a0(obj);

        if (obj->field5c == 0) {                /* nothing yet */
            obj->a10 -= 1;
            if ((long)obj->a10 > 0) {
                *mk3_frame(thread, thread->frame + 1) = 0x20f;
                thread->fieldfc = 1;
                return 1;
            }
        }

        stop_me_player(obj);                    /* hit, or out of travel */
        *mk3_frame(thread, thread->frame + 1) = 0x21a;
        thread->fieldfc = 0x14;
        return 0x14;
    }

    if (token == 0x21a) {
        obj->field1c = 3;
        *mk3_frame(thread, thread->frame + 1) = 0x21d;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x21d)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

    if (token != 0)
        return -3;

    if (obj->field18 != 0) {                    /* blocked on the first frame */
        blur_blocked_setup(obj);
        obj->field40 = 0x30046;
        pose_a9_manual(obj);
        return mk3_install(thread, (MK3THREADFUNC)t_blb8);
    }

    obj->a10 = 0xe;
    *mk3_frame(thread, thread->frame + 1) = 0x1fa;
    thread->fieldfc = 1;
    return 1;
}


/* ---------------------------------------------------------- tl_do_mileena_tele
 *
 * armv7 0x0003f454, 444 bytes.  **Complete.**
 *
 * Mileena drops through the floor and comes up somewhere else.
 *
 *      token == 0:      obj->field20 = 0x218 ; init_special_act(obj)
 *                       obj->field1c = 7 ; ochar_sound(obj)
 *                       obj->field40 = 0x12 ; get_char_ani(obj)
 *                       do_next_a9_frame(obj)
 *                       obj->field1c = obj->field08->field1c = 5.0
 *                       token := 0x151, park 1
 *
 *      token == 0x151:  y = (int16)obj->field08->field12
 *                       obj->field1c = y
 *                       obj->field20 = *(G + 0xac)
 *                       if (*(G + 0xac) >= y) {          ; still falling
 *                           obj->field1c = obj->field08->field1c + 0.375
 *                           obj->field08->field1c = obj->field1c
 *                           token := 0x151, park 1
 *                       }
 *                       -- through the floor, see below --
 *
 *      token == 0x17b:  obj->field40 = 0x30004
 *                       token := 0x17e, descend into t_animate_a9
 *
 *      token == 0x17e:  token := 0x17f, park 8
 *
 *      token == 0x17f:  if (thread->frame > 0) { frame -= 1; return 0 }
 *                       frame[frame].handler = t_local_reaction_exit
 *
 *      otherwise:       return -3
 *
 * The fall is a loop of one state: each frame adds 0.375 to the downward speed
 * and parks, and the exit test compares the sprite's y against `*(G + 0xac)`.
 * **Gravity is applied by the routine itself, not by a flight** -- there is no
 * descend anywhere in that state, just an add and a park.
 *
 * Through the floor, all in one frame:
 *
 *      match_me_with_him(obj) ; flip_multi(obj)
 *      obj->field1c = -0x50 ; obj->field20 = 0 ; multi_adjust_xy(obj)
 *      obj->field20 = *(G + 0x464) - 0x20
 *      (int16)obj->field08->field12 = obj->field20     ; put her there
 *      obj->field40 = 0x19 ; find_ani_last_frame(obj)
 *      do_next_a9_frame(obj)
 *      obj->field1c = 4.0 ; towards_x_vel(obj)
 *      obj->field1c = obj->field20 = 0xd
 *      obj->field24 = 0.625 ; obj->field28 = 0xfff
 *      obj->field40 = 0x19
 *      obj->field34 = t_mileena_teleport_call
 *      token := 0x17b, descend into t_flight_call
 *
 * **0x34 is the callback slot and this is the proof.** The address written
 * there is `t_mileena_teleport_call`, and the routine descended into is
 * `t_flight_call` -- the one that runs whatever 0x34 holds. The pair is
 * spelled out in nine instructions: store the function, then call the caller.
 *
 * She is placed by writing a HALFWORD into the part's 0x12, taken from a
 * global twenty pixels up. The reappearance is a coordinate store, not a
 * movement -- nothing interpolates her there.
 *
 * 0x28 gets 0xfff, which is not a small count like the 2s and 3s the other
 * flights use. Recorded as it stands.
 */
long tl_do_mileena_tele(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    int32_t  y;

    if (token == 0x17b) {
        obj->field40 = 0x30004;
        *mk3_frame(thread, thread->frame + 1) = 0x17e;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_animate_a9;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x17e) {
        *mk3_frame(thread, thread->frame + 1) = 0x17f;
        thread->fieldfc = 8;
        return 8;
    }

    if (token == 0x17f) {
        if ((long)thread->frame > 0) {
            thread->frame -= 1;
            return 0;
        }
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }

    if (token == 0x151) {
        y = *(int16_t *)((char *)obj->field08 + 0x12);
        obj->field1c = (uint32_t)y;
        obj->field20 = *(uint32_t *)(G_BYTES + 0xac);

        if ((int32_t)obj->field20 >= y) {       /* not through the floor yet */
            obj->field1c = obj->field08->field1c + 0x6000u;     /* 0.375 */
            obj->field08->field1c = obj->field1c;
            *mk3_frame(thread, thread->frame + 1) = 0x151;
            thread->fieldfc = 1;
            return 1;
        }

        match_me_with_him(obj);
        flip_multi(obj);
        obj->field1c = (uint32_t)-0x50;
        obj->field20 = 0;
        multi_adjust_xy(obj);

        obj->field20 = *(uint32_t *)(G_BYTES + 0x464) - 0x20;
        *(int16_t *)((char *)obj->field08 + 0x12) = (int16_t)obj->field20;

        obj->field40 = 0x19;
        find_ani_last_frame(obj);
        do_next_a9_frame(obj);
        obj->field1c = 0x40000u;                /* 4.0 */
        towards_x_vel(obj);
        obj->field1c = 0xd;
        obj->field20 = 0xd;
        obj->field24 = 0xa000u;                 /* 0.625 */
        obj->field28 = 0xfff;
        obj->field40 = 0x19;
        obj->field34 = (uint32_t)(uintptr_t)t_mileena_teleport_call;

        *mk3_frame(thread, thread->frame + 1) = 0x17b;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_flight_call;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0)
        return -3;

    obj->field20 = 0x218;
    init_special_act(obj);
    obj->field1c = 7;
    ochar_sound(obj);
    obj->field40 = 0x12;
    get_char_ani(obj);
    do_next_a9_frame(obj);
    obj->field1c = 0x50000u;                    /* 5.0 */
    obj->field08->field1c = 0x50000u;

    *mk3_frame(thread, thread->frame + 1) = 0x151;
    thread->fieldfc = 1;
    return 1;
}


/* ------------------------------------------------------------- tl_do_jade_prop
 *
 * armv7 0x00040b74, 480 bytes.  **Complete.**
 *
 * Jade's staff thrust: five frames of wind-up, one frame that can hit, then
 * seven damped frames that can still hit, then eight more of follow-through.
 *
 *      token == 0:      obj->field20 = 0x217 ; init_special_act(obj)
 *                       player_swpal(obj, 4)
 *                       obj->field1c = 6 ; ochar_sound(obj)
 *                       obj->field1c = 0x30 ; create_fx(obj)
 *                       obj->field40 = 0x12 ; get_char_ani(obj)
 *                       obj->field1c = 1 ; init_anirate(obj)
 *                       obj->field1c = 8.0 ; towards_x_vel(obj)
 *                       obj->a10 = 5
 *                       token := 0x1a6, park 1
 *
 *      token == 0x1a6:  next_anirate(obj)
 *                       if (--obj->a10 > 0) { token := 0x1a6, park 1 }
 *                       token := 0x1ac, park 1
 *
 *      token == 0x1ac:  next_anirate(obj)
 *                       obj->field1c = 0x10 ; strike_check_a0(obj)
 *                       if (obj->field5c != 0) -- CONNECTED --
 *                       obj->a10 = 7
 *                       token := 0x1b5, park 1
 *
 *      token == 0x1b5:  jade_prop_damping(obj) ; next_anirate(obj)
 *                       obj->field1c = 0x10 ; strike_check_a0(obj)
 *                       if (obj->field5c != 0) -- CONNECTED --
 *                       if (--obj->a10 > 0) { token := 0x1b5, park 1 }
 *                       obj->field1c = obj->field00->field18 = 0x605
 *                       obj->a10 = 8
 *                       token := 0x1c6, park 1
 *
 *      token == 0x1c6:  jade_prop_damping(obj)
 *                       if (--obj->a10 > 0) { token := 0x1c6, park 1 }
 *                       stop_me_player(obj)
 *                       token := 0x1cd, park 0x20
 *
 *      token == 0x1cd:  frame[frame].handler = t_local_reaction_exit
 *      token == 0x1dc:  frame[frame].handler = t_local_reaction_exit
 *
 *      CONNECTED:       obj->field40 = 0x12 ; do_first_a9_frame(obj)
 *                       find_last_frame(obj) ; do_next_a9_frame(obj)
 *                       stop_me_player(obj)
 *                       obj->field1c = (obj->field18 != 0) ? 0x10 : 0x20
 *                       token := 0x1dc, park obj->field1c
 *
 *      otherwise:       return -3
 *
 * **The strike window is eight frames wide and only the last seven are
 * damped.** 0x1ac tests once with no damping and then hands over to 0x1b5,
 * which tests again on each of seven frames with jade_prop_damping in front of
 * it. The first frame of the thrust is the undamped one.
 *
 * **A blocked hit recovers in 16 frames and a clean one in 32** -- the
 * opposite of the usual arrangement, and it is the clean hit that costs more.
 * `obj->field18` set means blocked, and the `adds r3, #0x20` that doubles the
 * wait runs only when it is clear.
 *
 * Both 0x1cd and 0x1dc install the same exit, from the same shared six
 * instructions. They are separate tokens because they are reached from
 * different waits, not because they do different things.
 *
 * 0x44 counts three separate stretches here -- 5, then 7, then 8 -- one field
 * doing the whole shape of the move.
 */
long tl_do_jade_prop(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    int      connected = 0;

    if (token == 0x1cd || token == 0x1dc)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

    if (token == 0x1a6) {
        next_anirate(obj);
        obj->a10 -= 1;
        *mk3_frame(thread, thread->frame + 1) =
            ((long)obj->a10 > 0) ? 0x1a6 : 0x1ac;
        thread->fieldfc = 1;
        return 1;
    }

    if (token == 0x1ac) {
        next_anirate(obj);
        obj->field1c = 0x10;
        strike_check_a0(obj);
        if (obj->field5c != 0) {
            connected = 1;
        } else {
            obj->a10 = 7;
            *mk3_frame(thread, thread->frame + 1) = 0x1b5;
            thread->fieldfc = 1;
            return 1;
        }
    } else if (token == 0x1b5) {
        jade_prop_damping(obj);
        next_anirate(obj);
        obj->field1c = 0x10;
        strike_check_a0(obj);
        if (obj->field5c != 0) {
            connected = 1;
        } else {
            obj->a10 -= 1;
            if ((long)obj->a10 > 0) {
                *mk3_frame(thread, thread->frame + 1) = 0x1b5;
                thread->fieldfc = 1;
                return 1;
            }
            obj->field1c = 0x605;
            obj->field00->field18 = 0x605;
            obj->a10 = 8;
            *mk3_frame(thread, thread->frame + 1) = 0x1c6;
            thread->fieldfc = 1;
            return 1;
        }
    } else if (token == 0x1c6) {
        jade_prop_damping(obj);
        obj->a10 -= 1;
        if ((long)obj->a10 > 0) {
            *mk3_frame(thread, thread->frame + 1) = 0x1c6;
            thread->fieldfc = 1;
            return 1;
        }
        stop_me_player(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x1cd;
        thread->fieldfc = 0x20;
        return 0x20;
    }

    if (connected) {
        obj->field40 = 0x12;
        do_first_a9_frame(obj);
        find_last_frame(obj);
        do_next_a9_frame(obj);
        stop_me_player(obj);

        obj->field1c = 0x10;
        if (obj->field18 == 0)                  /* not blocked: hold longer */
            obj->field1c = 0x20;

        *mk3_frame(thread, thread->frame + 1) = 0x1dc;
        thread->fieldfc = obj->field1c;
        return (long)obj->field1c;
    }

    if (token != 0)
        return -3;

    obj->field20 = 0x217;
    init_special_act(obj);
    player_swpal(obj, 4);
    obj->field1c = 6;
    ochar_sound(obj);
    obj->field1c = 0x30;
    create_fx(obj);
    obj->field40 = 0x12;
    get_char_ani(obj);
    obj->field1c = 1;
    init_anirate(obj);
    obj->field1c = 0x80000u;                    /* 8.0 */
    towards_x_vel(obj);
    obj->a10 = 5;

    *mk3_frame(thread, thread->frame + 1) = 0x1a6;
    thread->fieldfc = 1;
    return 1;
}


/* -------------------------------------------------------------- tl_do_lao_tele
 *
 * armv7 0x0003e5a4, 492 bytes.  **Complete.**
 *
 * Kung Lao's teleport: rise off the ground, reappear above the opponent, fall,
 * and finish with whatever the fighter is -- a chosen attack for a person, a
 * random one for the machine.
 *
 *      token == 0:      obj->field20 = 0x206 ; init_special_act(obj)
 *                       obj->field1c = 4 ; ochar_sound(obj)
 *                       obj->field40 = 0 ; get_char_ani2(obj)
 *                       do_next_a9_frame(obj)
 *                       obj->field1c = 2 ; init_anirate(obj)
 *                       obj->field08->field1c = -8.0
 *                       obj->field1c = obj->field08->field20 = -1.0
 *                       token := 0x931, park 1
 *
 *      token == 0x931:  next_anirate(obj) ; distance_off_ground(obj)
 *                       if (obj->field1c <= 0xe7) { token := 0x931, park 1 }
 *                       teleport_next_to(obj) ; face_opponent(obj)
 *                       obj->field20 = *(G + 0xac) - 0x60
 *                       (int16)obj->field08->field12 = obj->field20
 *                       obj->field08->field20 = 0.5
 *                       obj->field1c = obj->field08->field1c = -10.0
 *                       token := 0x944, park 1
 *
 *      token == 0x944:  y = (int16)obj->field08->field12 - 0x10
 *                       obj->field1c = y
 *                       obj->field20 = obj->field00->field40
 *                       if (obj->field00->field40 < y) { token := 0x944, park 1 }
 *                       am_i_joy(obj)
 *                       if (obj->field5c != 0) -- A PERSON, below --
 *                       mk_random(obj)
 *                       if (obj->field1c & 0x1000)
 *                           frame[frame].handler = t_do_jumpup_punch
 *                       else
 *                           frame[frame].handler = t_do_jumpup_kick
 *
 *      A PERSON:        stuff_buttons(obj, bt_jump)
 *                       obj->field40 = 0 ; find_ani2_part2(obj)
 *                       obj->field1c = 0
 *                       obj->field20 = obj->field08->field1c
 *                       obj->field24 = obj->field08->field20
 *                       obj->field28 = 5
 *                       token := 0x957, descend into t_flight
 *
 *      token == 0x957:  if (thread->frame > 0) { frame -= 1; return 0 }
 *                       frame[frame].handler = t_local_reaction_exit
 *
 *      otherwise:       return -3
 *
 * **The machine picks its follow-up with one bit of one random number.**
 * mk_random leaves a value in 0x1c, bit 12 is tested, and the answer is a
 * punch or a kick -- nothing weights it and nothing remembers the last one.
 *
 * **A person gets the buttons back instead.** am_i_joy coming back set means
 * somebody is holding the stick, so the routine stuffs the jump table into the
 * object and hands over to an ordinary flight: whatever they press on the way
 * down is what happens. The same moment in the move is a decision for the AI
 * and a hand-back for the player.
 *
 * Both velocity pairs are one literal and one add: -8.0 then +0x70000 gives
 * -1.0, and 0.5 then -0xa8000 gives -10.0. The second wraps through zero and
 * the assembler still spelled only one of them.
 *
 * The fall is watched against obj->field00->field40 rather than a ground
 * constant, and 0x20 takes that field on every frame of the wait whether or
 * not the comparison ends it.
 */
long tl_do_lao_tele(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    int32_t  y;

    if (token == 0x931) {
        next_anirate(obj);
        distance_off_ground(obj);
        if ((long)obj->field1c <= 0xe7) {       /* not high enough yet */
            *mk3_frame(thread, thread->frame + 1) = 0x931;
            thread->fieldfc = 1;
            return 1;
        }

        teleport_next_to(obj);
        face_opponent(obj);
        obj->field20 = *(uint32_t *)(G_BYTES + 0xac) - 0x60;
        *(int16_t *)((char *)obj->field08 + 0x12) = (int16_t)obj->field20;
        obj->field08->field20 = 0x8000u;                /* 0.5 */
        obj->field1c = 0x8000u - 0xa8000u;              /* -10.0 */
        obj->field08->field1c = obj->field1c;

        *mk3_frame(thread, thread->frame + 1) = 0x944;
        thread->fieldfc = 1;
        return 1;
    }

    if (token == 0x944) {
        y = *(int16_t *)((char *)obj->field08 + 0x12) - 0x10;
        obj->field1c = (uint32_t)y;
        obj->field20 = *(uint32_t *)((char *)obj->field00 + 0x40);

        if (*(int32_t *)((char *)obj->field00 + 0x40) < y) {    /* still falling */
            *mk3_frame(thread, thread->frame + 1) = 0x944;
            thread->fieldfc = 1;
            return 1;
        }

        am_i_joy(obj);
        if (obj->field5c != 0) {                /* a person: give it back */
            stuff_buttons(obj, (uint32_t)(uintptr_t)bt_jump);
            obj->field40 = 0;
            find_ani2_part2(obj);
            obj->field1c = 0;
            obj->field20 = obj->field08->field1c;
            obj->field24 = obj->field08->field20;
            obj->field28 = 5;

            *mk3_frame(thread, thread->frame + 1) = 0x957;
            thread->frame = thread->frame + 1;
            mk3_frame(thread, thread->frame)[1] =
                (uint32_t)(uintptr_t)t_flight;
            *mk3_frame(thread, thread->frame + 1) = 0;
            return 0;
        }

        mk_random(obj);
        if ((obj->field1c & 0x1000u) != 0)
            return mk3_install(thread, (MK3THREADFUNC)t_do_jumpup_punch);
        return mk3_install(thread, (MK3THREADFUNC)t_do_jumpup_kick);
    }

    if (token == 0x957) {
        if ((long)thread->frame > 0) {
            thread->frame -= 1;
            return 0;
        }
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }

    if (token != 0)
        return -3;

    obj->field20 = 0x206;
    init_special_act(obj);
    obj->field1c = 4;
    ochar_sound(obj);
    obj->field40 = 0;
    get_char_ani2(obj);
    do_next_a9_frame(obj);
    obj->field1c = 2;
    init_anirate(obj);

    obj->field08->field1c = 0xfff80000u;        /* -8.0 */
    obj->field1c = 0xfff80000u + 0x70000u;      /* -1.0 */
    obj->field08->field20 = obj->field1c;

    *mk3_frame(thread, thread->frame + 1) = 0x931;
    thread->fieldfc = 1;
    return 1;
}


/* --------------------------------------------------------------- t_pounce_fall
 *
 * armv7 0x00040118, 496 bytes.  **Complete.**
 *
 * The pounce, all the way down and then a second time. Eight states, and two
 * of them are the same descend into t_pounce_jsrp with one field differing.
 *
 *      token == 0:      token := 0x4e8, descend into t_flight_call
 *
 *      token == 0x4e8:  token := 0x4e9, descend into t_blur_catchup
 *
 *      token == 0x4e9:  get_his_action(obj)
 *                       if (obj->field20 != 0x30b)
 *                           frame[frame].handler = t_pounce_miss
 *                       do_next_a9_frame(obj)
 *                       token := 0x4f0, park 4
 *
 *      token == 0x4f0:  set_ignore_y(obj) ; pounce_ground_him(obj)
 *                       *(uint16 *)(G + 0x456) = 0x1e
 *                       obj->field1c = 0
 *                       obj->field48 = t_r_pounce2
 *                       token := 0x4f8, descend into t_pounce_jsrp
 *
 *      token == 0x4f8:  do_next_a9_frame(obj)
 *                       token := 0x4fa, park 4
 *
 *      token == 0x4fa:  obj->field1c = 6.0 ; edge_pick_a0(obj)
 *                       obj->field48 = 0
 *                       *(uint16 *)(G + 0x456) = 0x1e
 *                       token := 0x500, descend into t_pounce_jsrp
 *
 *      token == 0x500:  do_next_a9_frame(obj)
 *                       token := 0x502, park 4
 *
 *      token == 0x502:  frame[frame].handler = t_local_reaction_exit
 *
 *      otherwise:       return -3
 *
 * **0x48 is a handler address on the first pounce and zero on the second, and
 * t_pounce_jsrp is what reads it.** That routine's 0x524 state copies 0x48
 * into 0x38 and calls xfer_otherguy only when it is not zero -- so the first
 * pounce hands the opponent's thread over to t_r_pounce2 and the second one
 * does not. The whole difference between the two bounces is one word written
 * one instruction before the descend. Both halves are now transcribed and they
 * agree.
 *
 * **The move is abandoned by asking what the opponent is doing.** State 0x4e9
 * calls get_his_action and compares 0x20 against 0x30b; anything else installs
 * t_pounce_miss and the pounce never lands. It is the only test in the routine
 * and it happens before the first ground contact.
 *
 * 0x1e goes into the halfword at G + 0x456 twice, once for each bounce -- the
 * same slot the sans_repell family and t_hover_sleep_1 write with their own
 * values.
 */
long t_pounce_fall(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x4e8) {
        *mk3_frame(thread, thread->frame + 1) = 0x4e9;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_blur_catchup;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x4e9) {
        get_his_action(obj);
        if (obj->field20 != 0x30b)              /* he is not there for it */
            return mk3_install(thread, (MK3THREADFUNC)t_pounce_miss);

        do_next_a9_frame(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x4f0;
        thread->fieldfc = 4;
        return 4;
    }

    if (token == 0x4f0) {
        set_ignore_y(obj);
        pounce_ground_him(obj);
        *(uint16_t *)(G_BYTES + 0x456) = 0x1e;
        obj->field1c = 0;
        obj->field48 = (uint32_t)(uintptr_t)t_r_pounce2;

        *mk3_frame(thread, thread->frame + 1) = 0x4f8;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_pounce_jsrp;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x4f8) {
        do_next_a9_frame(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x4fa;
        thread->fieldfc = 4;
        return 4;
    }

    if (token == 0x4fa) {
        obj->field1c = 0x60000u;                /* 6.0 */
        edge_pick_a0(obj);
        obj->field48 = 0;                       /* no hand-over this time */
        *(uint16_t *)(G_BYTES + 0x456) = 0x1e;

        *mk3_frame(thread, thread->frame + 1) = 0x500;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_pounce_jsrp;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x500) {
        do_next_a9_frame(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x502;
        thread->fieldfc = 4;
        return 4;
    }

    if (token == 0x502)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

    if (token != 0)
        return -3;

    *mk3_frame(thread, thread->frame + 1) = 0x4e8;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_flight_call;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ----------------------------------------------------------------- tl_do_slide
 *
 * armv7 0x00040498, 500 bytes.  **Complete.**
 *
 * The slide. Twenty-eight frames long, of which the first eight cannot hit.
 *
 *      token == 0:      obj->field20 = 0x210 ; init_special_act(obj)
 *                       obj->field1c = 8.0
 *                       obj->field54 = obj->field08->field24
 *                       if (obj->field54 == 0x15) obj->field1c = 11.0
 *                       towards_x_vel(obj)
 *                       obj->field40 = 2 ; get_char_ani2(obj)
 *                       obj->field1c = 4 ; init_anirate(obj)
 *                       obj->field1c = 6 ; ochar_sound(obj)
 *                       obj->a10 = 0x1c
 *                       token := 0x445, park 1
 *
 *      token == 0x445:  next_anirate(obj)
 *                       if (obj->a10 <= 0x14) {
 *                           obj->field1c = 0x10 ; strike_check_a0(obj)
 *                           if (obj->field5c != 0) {
 *                               if (obj->field18 != 0) -- FINISH --
 *                               obj->field40 = 0x20002
 *                               pose2_a9_manual(obj)
 *                               obj->field1c = obj->field08->field18 >> 1
 *                               set_x_vel_player(obj)
 *                               token := 0x45b, park 3
 *                           }
 *                       }
 *                       if (--obj->a10 > 0) { token := 0x445, park 1 }
 *                       -- FINISH --
 *
 *      FINISH:          stop_me_player(obj) ; clear_noflip(obj)
 *                       set_no_block(obj)
 *                       obj->field1c = obj->field00->field18 = 0x613
 *                       token := 0x467, park 0x16
 *
 *      token == 0x45b:  stop_me_player(obj)
 *                       token := 0x45d, descend into t_wait_for_landing
 *
 *      token == 0x45d:  obj->field40 = 0x10002 ; pose2_a9_manual(obj)
 *      token == 0x467:  the same, both branch to one instruction
 *                       token := 0x46c, park 4
 *
 *      token == 0x46c:  obj->field40 = 2 ; pose2_a9_manual(obj)
 *                       token := 0x46f, park 4
 *
 *      token == 0x46f:  obj->field1c = &G + 0x400 ; update_tsl(obj)
 *                       frame[frame].handler = t_local_reaction_exit
 *
 *      otherwise:       return -3
 *
 * **The speed is chosen by a number read out of the part.** 0x24 of the part
 * goes into 0x54 and is compared against 0x15; matching makes the slide 11.0
 * instead of 8.0. One character slides faster than the rest and the test for
 * which is a single equality on a field the routine also keeps a copy of.
 *
 * **The two ways the slide ends meet at the same instruction.** A blocked hit
 * and the counter running out both reach the stop-clear-unblock sequence, and
 * the connecting case is the only one that leaves early -- through 0x45b, a
 * landing wait, and two poses. So a slide that hits takes the long way home
 * and a slide that misses does not.
 *
 * **On a clean hit the opponent's own speed is halved into him.** 0x18 of the
 * part is arithmetic-shifted right one and handed to set_x_vel_player, so the
 * push depends on how fast he was already going rather than on a constant.
 *
 * 0x45d and 0x467 are two tokens with one body, which is why the dispatch
 * compares against 0x467 last and branches to the same address as 0x45d.
 */
long tl_do_slide(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    int      finish = 0;

    if (token == 0x45d || token == 0x467) {
        obj->field40 = 0x10002;
        pose2_a9_manual(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x46c;
        thread->fieldfc = 4;
        return 4;
    }

    if (token == 0x46c) {
        obj->field40 = 2;
        pose2_a9_manual(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x46f;
        thread->fieldfc = 4;
        return 4;
    }

    if (token == 0x46f) {
        obj->field1c = (uint32_t)(uintptr_t)(G_BYTES + 0x400);
        update_tsl(obj);
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }

    if (token == 0x45b) {
        stop_me_player(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x45d;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_wait_for_landing;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x445) {
        next_anirate(obj);

        if ((long)obj->a10 <= 0x14) {           /* past the eight-frame nose */
            obj->field1c = 0x10;
            strike_check_a0(obj);
            if (obj->field5c != 0) {
                if (obj->field18 != 0) {        /* blocked */
                    finish = 1;
                } else {
                    obj->field40 = 0x20002;
                    pose2_a9_manual(obj);
                    obj->field1c = (uint32_t)
                        ((int32_t)obj->field08->field18 >> 1);
                    set_x_vel_player(obj);
                    *mk3_frame(thread, thread->frame + 1) = 0x45b;
                    thread->fieldfc = 3;
                    return 3;
                }
            }
        }

        if (!finish) {
            obj->a10 -= 1;
            if ((long)obj->a10 > 0) {
                *mk3_frame(thread, thread->frame + 1) = 0x445;
                thread->fieldfc = 1;
                return 1;
            }
        }

        stop_me_player(obj);
        clear_noflip(obj);
        set_no_block(obj);
        obj->field1c = 0x613;
        obj->field00->field18 = 0x613;
        *mk3_frame(thread, thread->frame + 1) = 0x467;
        thread->fieldfc = 0x16;
        return 0x16;
    }

    if (token != 0)
        return -3;

    obj->field20 = 0x210;
    init_special_act(obj);
    obj->field1c = 0x80000u;                    /* 8.0 */
    obj->field54 = obj->field08->field24;
    if (obj->field54 == 0x15)
        obj->field1c = 0xb0000u;                /* 11.0 for one of them */
    towards_x_vel(obj);
    obj->field40 = 2;
    get_char_ani2(obj);
    obj->field1c = 4;
    init_anirate(obj);
    obj->field1c = 6;
    ochar_sound(obj);
    obj->a10 = 0x1c;

    *mk3_frame(thread, thread->frame + 1) = 0x445;
    thread->fieldfc = 1;
    return 1;
}


/* ------------------------------------------------------------- tl_do_robo_tele
 *
 * armv7 0x0003eb24, 508 bytes.  **Complete.**
 *
 * The robots' teleport: rise, drop through the floor, come up under the
 * opponent, and scan for him twice on the way.
 *
 *      token == 0:      obj->field20 = obj->field00->field18 = 0x208
 *                       air_init_special(obj)
 *                       obj->field1c = 7 ; ochar_sound(obj)
 *                       obj->field40 = 0xd ; get_char_ani(obj)
 *                       find_last_frame(obj) ; do_next_a9_frame(obj)
 *                       obj->field08->field1c = 7.0
 *                       obj->field1c = obj->field08->field20 = 0.6875
 *                       token := 0x878, park 1
 *
 *      token == 0x878:  distance_from_ground(obj)
 *                       if (obj->field1c > 0) { token := 0x878, park 1 }
 *                       -- THROUGH THE FLOOR, below --
 *
 *      token == 0x89b:  next_anirate(obj)
 *                       y = (int16)obj->field08->field12
 *                       obj->field1c = y
 *                       obj->field20 = *(proc + 0x40)
 *                       obj->field24 = y - obj->field20
 *                       if (obj->field24 > 0x30) -- the 0x8a3 body --
 *                       token := 0x8a3, descend into t_tele_scan
 *
 *      token == 0x8a3:  if (obj->field1c > obj->field20)
 *                           { token := 0x89b, park 1 }
 *                       obj->field1c = 0.9375
 *                       obj->field20 = *(proc + 0x2c)
 *                       if (obj->field20 != 0) obj->field1c = 0.6875
 *                       obj->field08->field20 = obj->field1c
 *                       obj->field20 = obj->field24 = 0xd
 *                       obj->field1c = 0
 *                       obj->field28 = obj->field00->field1c
 *                       obj->field34 = t_tele_scan2
 *                       token := 0x8b4, descend into t_flight_call
 *
 *      token == 0x8b4:  frame[frame].handler = t_jump_up_land_jsrp
 *
 *      THROUGH THE FLOOR:
 *                       match_me_with_him(obj)
 *                       obj->field20 = 0
 *                       h = ((MK3OBJ *)obj->field00->him)->field18
 *                       obj->field1c = ((h << 4) >> 16) - 0x10
 *                       multi_adjust_xy(obj) ; face_opponent(obj)
 *                       obj->field20 = *(G + 0xac)
 *                       (int16)obj->field08->field12 = obj->field20
 *                       obj->field08->field1c = -9.0
 *                       obj->field1c = obj->field08->field20 = -0.375
 *                       obj->field40 = 3 ; get_char_ani2(obj)
 *                       obj->field1c = 5 ; init_anirate(obj)
 *                       obj->field1c = obj->field00->field28 = 0
 *                       token := 0x89b, park 1
 *
 *      otherwise:       return -3
 *
 * **It scans twice, through two different routines.** 0x89b measures the gap
 * between the sprite and proc+0x40; a gap over 0x30 goes straight into the
 * 0x8a3 body, and anything smaller descends into `t_tele_scan` first. Then
 * 0x8a3 stores `t_tele_scan2` into 0x34 and descends into `t_flight_call`,
 * which runs it. Two scans, one before the rise and one during it, and the
 * second is a callback rather than a call.
 *
 * **0.9375 or 0.6875, decided by proc+0x2c.** The same two-branch shape as
 * every other move in this file, on a field the struct has no name for.
 *
 * The horizontal placement comes from the OPPONENT's 0x18 put through
 * `(h << 4) >> 16` -- a shift pair that truncates the top nibble before
 * sign-extending, not a plain divide -- and then twenty-two less. Transcribed
 * as the two shifts the binary uses.
 *
 * proc+0x40 is read here with `ldr`, a full word, though mk3logic.h declares a
 * halfword at that offset on other evidence. Reached as an offset so neither
 * reading is lost.
 */
long tl_do_robo_tele(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    int32_t  y, h;
    int      rise = 0;

    if (token == 0x8b4)
        return mk3_install(thread, (MK3THREADFUNC)t_jump_up_land_jsrp);

    if (token == 0x89b) {
        next_anirate(obj);
        y = *(int16_t *)((char *)obj->field08 + 0x12);
        obj->field1c = (uint32_t)y;
        obj->field20 = *(uint32_t *)((char *)obj->field00 + 0x40);
        obj->field24 = (uint32_t)(y - (int32_t)obj->field20);

        if ((int32_t)obj->field24 <= 0x30) {    /* close enough: scan first */
            *mk3_frame(thread, thread->frame + 1) = 0x8a3;
            thread->frame = thread->frame + 1;
            mk3_frame(thread, thread->frame)[1] =
                (uint32_t)(uintptr_t)t_tele_scan;
            *mk3_frame(thread, thread->frame + 1) = 0;
            return 0;
        }
        rise = 1;
    }

    if (rise || token == 0x8a3) {
        if ((int32_t)obj->field1c > (int32_t)obj->field20) {
            *mk3_frame(thread, thread->frame + 1) = 0x89b;
            thread->fieldfc = 1;
            return 1;
        }

        obj->field1c = 0xf000u;                 /* 0.9375 */
        obj->field20 = *(uint32_t *)((char *)obj->field00 + 0x2c);
        if (obj->field20 != 0)
            obj->field1c = 0xb000u;             /* 0.6875 */
        obj->field08->field20 = obj->field1c;

        obj->field20 = 0xd;
        obj->field24 = 0xd;
        obj->field1c = 0;
        obj->field28 = obj->field00->field1c;
        obj->field34 = (uint32_t)(uintptr_t)t_tele_scan2;

        *mk3_frame(thread, thread->frame + 1) = 0x8b4;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_flight_call;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0 && token != 0x878)
        return -3;

    if (token == 0x878) {
        distance_from_ground(obj);
        if ((long)obj->field1c > 0) {           /* not down yet */
            *mk3_frame(thread, thread->frame + 1) = 0x878;
            thread->fieldfc = 1;
            return 1;
        }

        match_me_with_him(obj);
        obj->field20 = 0;
        h = (int32_t)((MK3OBJ *)(uintptr_t)obj->field00->him)->field18;
        obj->field1c = (uint32_t)((((int32_t)(h << 4)) >> 16) - 0x10);
        multi_adjust_xy(obj);
        face_opponent(obj);

        obj->field20 = *(uint32_t *)(G_BYTES + 0xac);
        *(int16_t *)((char *)obj->field08 + 0x12) = (int16_t)obj->field20;
        obj->field08->field1c = 0xfff70000u;    /* -9.0 */
        obj->field1c = 0xffffa000u;             /* -0.375 */
        obj->field08->field20 = obj->field1c;

        obj->field40 = 3;
        get_char_ani2(obj);
        obj->field1c = 5;
        init_anirate(obj);
        obj->field1c = 0;
        obj->field00->field28 = 0;

        *mk3_frame(thread, thread->frame + 1) = 0x89b;
        thread->fieldfc = 1;
        return 1;
    }

    obj->field20 = 0x208;
    obj->field00->field18 = 0x208;
    air_init_special(obj);
    obj->field1c = 7;
    ochar_sound(obj);
    obj->field40 = 0xd;
    get_char_ani(obj);
    find_last_frame(obj);
    do_next_a9_frame(obj);
    obj->field08->field1c = 0x70000u;           /* 7.0 */
    obj->field1c = 0x70000u - 0x65000u;         /* 0.6875 */
    obj->field08->field20 = obj->field1c;

    *mk3_frame(thread, thread->frame + 1) = 0x878;
    thread->fieldfc = 1;
    return 1;
}


/* ----------------------------------------------------------- tl_do_kano_upball
 *
 * armv7 0x0003f064, 540 bytes.  **Complete.**
 *
 * Kano's upward cannonball. Six states; the rise is a flight, the fall is
 * watched by a damping routine, and the landing is a second flight with a
 * null callback.
 *
 *      token == 0:      obj->field20 = 0x21a ; init_special_act(obj)
 *                       obj->field1c = 4 ; ochar_sound(obj)
 *                       set_noflip(obj)
 *                       obj->field40 = 7.0
 *                       is_he_airborn(obj)
 *                       if (obj->field5c != 0) obj->field40 = 9.0
 *                       obj->field1c = obj->field40 ; towards_x_vel(obj)
 *                       obj->field08->field1c = -3.0
 *                       obj->field08->field20 = -1.0
 *                       obj->field1c = 0 ; obj->field20 = 0x18
 *                       multi_adjust_xy(obj)
 *                       obj->field40 = 0x1a ; get_char_ani(obj)
 *                       obj->field40 += 4
 *                       obj->field1c = 3 ; init_anirate(obj)
 *                       obj->field48 = 0xa
 *                       token := 0x6c, park 1
 *
 *      token == 0x6c:   if (obj->field48 <= 0x14) {
 *                           obj->field1c = 0x13 ; strike_check_a0(obj)
 *                           if (obj->field5c != 0) {
 *                               obj->field00->field18 = 0x602
 *                               obj->field1c = 3.0 ; obj->field20 = -8.0
 *                               if (obj->field18 != 0) {
 *                                   obj->field1c = 0 ; obj->field20 = -9.0
 *                               }
 *                               obj->field24 = 0.375 ; obj->field28 = 2
 *                               token := 0xa6, descend into t_flight
 *                           }
 *                       }
 *                       next_anirate(obj)
 *                       if (--obj->field48 > 0) { token := 0x6c, park 1 }
 *                       obj->field20 = obj->field00->field18 = 0x62d
 *                       obj->field1c = obj->field08->field20 = 1.5
 *                       token := 0x7f, park 1
 *
 *      token == 0x7f:   token := 0x80, descend into t_upball_x_damping
 *
 *      token == 0x80:   next_anirate(obj)
 *                       obj->field1c = obj->field08->field1c
 *                       if (obj->field1c < 0) { token := 0x7f, park 1 }
 *                       obj->field1c = 0 ; set_x_vel_player(obj)
 *                       obj->field08->field20 = 0.3125
 *                       obj->field34 = 0
 *                       obj->field1c = obj->field20 = obj->field24 = 0xd
 *                       obj->field28 = 2
 *                       token := 0x91, descend into t_flight_call
 *
 *      token == 0xa6:   frame[frame].handler = t_jump_up_land_jsrp
 *      token == 0x91:   frame[frame].handler = t_jump_up_land_jump
 *
 *      otherwise:       return -3
 *
 * **A guard that can never fire.** 0x48 is written 0xa and only ever
 * decrements, and the strike is skipped when it is above 0x14 -- which it
 * never is. The test is in the binary and it is dead for every value this
 * routine can produce. Transcribed rather than removed: the same shape gates a
 * real six-frame nose in tl_kano_cannon_ball, so this is the idiom with the
 * threshold left on the wrong side.
 *
 * **Being blocked replaces the whole launch, not part of it.** A clean hit
 * flies at 3.0 across and -8.0 up; blocked, it is 0 across and -9.0 up. The
 * second pair is written over the first by the same two instructions in
 * sequence, so the arithmetic for the clean case runs whether or not it is
 * used.
 *
 * **The landing descends into t_flight_call with 0x34 set to zero.** Every
 * other user of that routine puts a function there first; this one clears it,
 * so the callback is a null one. The flight still runs -- only the per-frame
 * call has nothing to reach.
 *
 * Both -3.0 and -1.0 come from one pool word plus 0x20000, and both -8.0 and
 * -9.0 from a subtraction off a number already in the register.
 */
long tl_do_kano_upball(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0xa6)
        return mk3_install(thread, (MK3THREADFUNC)t_jump_up_land_jsrp);

    if (token == 0x91)
        return mk3_install(thread, (MK3THREADFUNC)t_jump_up_land_jump);

    if (token == 0x7f) {
        *mk3_frame(thread, thread->frame + 1) = 0x80;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_upball_x_damping;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x80) {
        next_anirate(obj);
        obj->field1c = obj->field08->field1c;
        if ((int32_t)obj->field1c < 0) {        /* still going up */
            *mk3_frame(thread, thread->frame + 1) = 0x7f;
            thread->fieldfc = 1;
            return 1;
        }

        obj->field1c = 0;
        set_x_vel_player(obj);
        obj->field08->field20 = 0x5000u;        /* 0.3125 */
        obj->field34 = 0;                       /* no callback this time */
        obj->field1c = 0xd;
        obj->field20 = 0xd;
        obj->field24 = 0xd;
        obj->field28 = 2;

        *mk3_frame(thread, thread->frame + 1) = 0x91;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_flight_call;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x6c) {
        if ((long)obj->field48 <= 0x14) {       /* always true: 0x48 starts at 10 */
            obj->field1c = 0x13;
            strike_check_a0(obj);
            if (obj->field5c != 0) {
                obj->field00->field18 = 0x602;
                obj->field1c = 0x30000u;        /* 3.0 */
                obj->field20 = 0x30000u - 0xb0000u;     /* -8.0 */
                if (obj->field18 != 0) {        /* blocked: a different launch */
                    obj->field1c = 0;
                    obj->field20 = 0u - 0x90000u;       /* -9.0 */
                }
                obj->field24 = 0x6000u;         /* 0.375 */
                obj->field28 = 2;

                *mk3_frame(thread, thread->frame + 1) = 0xa6;
                thread->frame = thread->frame + 1;
                mk3_frame(thread, thread->frame)[1] =
                    (uint32_t)(uintptr_t)t_flight;
                *mk3_frame(thread, thread->frame + 1) = 0;
                return 0;
            }
        }

        next_anirate(obj);
        obj->field48 -= 1;
        if ((long)obj->field48 > 0) {
            *mk3_frame(thread, thread->frame + 1) = 0x6c;
            thread->fieldfc = 1;
            return 1;
        }

        obj->field20 = 0x62d;
        obj->field00->field18 = 0x62d;
        obj->field1c = 0x18000u;                /* 1.5 */
        obj->field08->field20 = obj->field1c;

        *mk3_frame(thread, thread->frame + 1) = 0x7f;
        thread->fieldfc = 1;
        return 1;
    }

    if (token != 0)
        return -3;

    obj->field20 = 0x21a;
    init_special_act(obj);
    obj->field1c = 4;
    ochar_sound(obj);
    set_noflip(obj);

    obj->field40 = 0x70000u;                    /* 7.0 */
    is_he_airborn(obj);
    if (obj->field5c != 0)
        obj->field40 = 0x90000u;                /* 9.0 if he is in the air */
    obj->field1c = obj->field40;
    towards_x_vel(obj);

    obj->field08->field1c = 0xfffd0000u;                /* -3.0 */
    obj->field08->field20 = 0xfffd0000u + 0x20000u;     /* -1.0 */

    obj->field1c = 0;
    obj->field20 = 0x18;
    multi_adjust_xy(obj);
    obj->field40 = 0x1a;
    get_char_ani(obj);
    obj->field40 = obj->field40 + 4;
    obj->field1c = 3;
    init_anirate(obj);
    obj->field48 = 0xa;

    *mk3_frame(thread, thread->frame + 1) = 0x6c;
    thread->fieldfc = 1;
    return 1;
}


/* ----------------------------------------------------------- tl_jax_dash_punch
 *
 * armv7 0x0003c9e8, 584 bytes.  **Complete.**
 *
 * Jax's dash punch: eight frames of closing, then sixteen of pushing through,
 * with the speed bleeding away a thirty-second at a time.
 *
 *      token == 0:      obj->field20 = 0x205 ; init_special_act(obj)
 *                       obj->field1c = 3 ; ochar_sound(obj)
 *                       obj->field40 = 2 ; get_char_ani2(obj)
 *                       obj->a10 = obj->field40
 *                       obj->field1c = 0x30002
 *                       token := 0xbf9, descend into t_animate_a0_frames
 *
 *      token == 0xbf9:  obj->field48 = obj->field1c = 10.0
 *                       towards_x_vel(obj) ; do_next_a9_frame(obj)
 *                       obj->a10 = 8
 *                       token := 0xc03, park 1
 *
 *      token == 0xc03:  obj->field1c = 0x10 ; strike_check_a0(obj)
 *                       if (obj->field5c != 0) -- CONNECTED --
 *                       if (--obj->a10 > 0) { token := 0xc03, park 1 }
 *                       obj->field00->field28 = obj->field08->field18
 *                       obj->a10 = 0x10
 *                       token := 0xc13, park 1
 *
 *      token == 0xc13:  v = obj->field08->field18
 *                       obj->field34 = v ; obj->field1c = v
 *                       obj->field34 = v >> 31          ; the sign
 *                       obj->field30 = obj->field00->field28 >> 31
 *                       if (obj->field2c != obj->field34) obj->field1c = 0
 *                       obj->field20 = obj->field1c >> 5
 *                       obj->field1c -= obj->field20
 *                       set_x_vel_player(obj)
 *                       if (obj->a10 > 4) {
 *                           obj->field1c = 0x10 ; strike_check_a0(obj)
 *                           if (obj->field5c != 0) -- CONNECTED --
 *                       } else
 *                           obj->field1c = obj->field00->field18 = 0x62c
 *                       if (--obj->a10 > 0) { token := 0xc13, park 1 }
 *                       -- FINISH --
 *
 *      CONNECTED:       if (obj->field18 != 0) {        ; blocked
 *                           stop_me_player(obj)
 *                           token := 0xc4f, park 4
 *                       }
 *                       obj->field1c = |obj->field08->field18|
 *                       if (obj->field1c >= 4.0) {
 *                           obj->field1c = 4.0 ; towards_x_vel(obj)
 *                       }
 *                       obj->field1c = 0x20
 *                       *(uint16 *)(G + 0x456) = 0x20
 *                       token := 0xc44, park 4
 *
 *      FINISH (0xc44, 0xc4f too):
 *                       obj->field40 = 2 ; find_ani2_part2(obj)
 *                       obj->field1c = 5
 *                       token := 0xc4a, descend into t_mframew
 *
 *      token == 0xc4a:  if (thread->frame > 0) { frame -= 1; return 0 }
 *                       frame[frame].handler = t_local_reaction_exit
 *
 *      otherwise:       return -3
 *
 * **The push is a thirty-second decay, not a constant.** Each frame of 0xc13
 * takes the current speed, shifts it right five, and subtracts -- so Jax slows
 * geometrically for sixteen frames rather than being stopped by a timer. The
 * shift is arithmetic, so it works on a leftward dash too.
 *
 * **The last four frames stop trying to hit.** `obj->a10 > 4` gates the strike
 * check; below that the routine writes action 0x62c instead and just finishes
 * the animation. The counter runs 0x10 down to 1 and the window closes at 4.
 *
 * **Two signs are computed and a third field is tested.** 0x34 gets the sign
 * of the part's 0x18 and 0x30 the sign of the proc's 0x28, and then the
 * comparison reads **0x2c**, which this routine never writes. Whatever is
 * there decides whether the speed is zeroed. Both stores and the mismatched
 * load are in the binary in that order; nothing here says what fills 0x2c, so
 * it is transcribed and left as a question.
 *
 * Being blocked and connecting cleanly reach the same two-token ending from
 * different waits -- 0xc4f after a stop, 0xc44 after the speed clamp -- and
 * both run the same three instructions.
 */
long tl_jax_dash_punch(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    int32_t  v;
    int      connected = 0, finish = 0;

    if (token == 0xc4a) {
        if ((long)thread->frame > 0) {
            thread->frame -= 1;
            return 0;
        }
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }

    if (token == 0xbf9) {
        obj->field48 = 0xa0000u;                /* 10.0 */
        obj->field1c = 0xa0000u;
        towards_x_vel(obj);
        do_next_a9_frame(obj);
        obj->a10 = 8;
        *mk3_frame(thread, thread->frame + 1) = 0xc03;
        thread->fieldfc = 1;
        return 1;
    }

    if (token == 0xc03) {
        obj->field1c = 0x10;
        strike_check_a0(obj);
        if (obj->field5c != 0) {
            connected = 1;
        } else {
            obj->a10 -= 1;
            if ((long)obj->a10 > 0) {
                *mk3_frame(thread, thread->frame + 1) = 0xc03;
                thread->fieldfc = 1;
                return 1;
            }
            obj->field00->field28 = obj->field08->field18;
            obj->a10 = 0x10;
            *mk3_frame(thread, thread->frame + 1) = 0xc13;
            thread->fieldfc = 1;
            return 1;
        }
    } else if (token == 0xc13) {
        v = (int32_t)obj->field08->field18;
        obj->field34 = (uint32_t)v;
        obj->field1c = (uint32_t)v;
        obj->field34 = (uint32_t)(v >> 31);             /* the sign */
        obj->field30 = (uint32_t)((int32_t)obj->field00->field28 >> 31);
        if (obj->field2c != obj->field34)               /* 0x2c, not 0x30 */
            obj->field1c = 0;

        obj->field20 = (uint32_t)((int32_t)obj->field1c >> 5);
        obj->field1c = obj->field1c - obj->field20;     /* a thirty-second off */
        set_x_vel_player(obj);

        if ((long)obj->a10 > 4) {
            obj->field1c = 0x10;
            strike_check_a0(obj);
            if (obj->field5c != 0)
                connected = 1;
        } else {
            obj->field1c = 0x62c;
            obj->field00->field18 = 0x62c;
        }

        if (!connected) {
            obj->a10 -= 1;
            if ((long)obj->a10 > 0) {
                *mk3_frame(thread, thread->frame + 1) = 0xc13;
                thread->fieldfc = 1;
                return 1;
            }
            finish = 1;
        }
    }

    if (connected) {
        if (obj->field18 != 0) {                /* he blocked it */
            stop_me_player(obj);
            *mk3_frame(thread, thread->frame + 1) = 0xc4f;
            thread->fieldfc = 4;
            return 4;
        }

        v = (int32_t)obj->field08->field18;
        obj->field1c = (uint32_t)v;
        if (v < 0)
            obj->field1c = (uint32_t)(-v);
        if ((long)obj->field1c >= 0x40000) {    /* clamp to 4.0 */
            obj->field1c = 0x40000u;
            towards_x_vel(obj);
        }
        obj->field1c = 0x20;
        *(uint16_t *)(G_BYTES + 0x456) = 0x20;

        *mk3_frame(thread, thread->frame + 1) = 0xc44;
        thread->fieldfc = 4;
        return 4;
    }

    if (finish || token == 0xc44 || token == 0xc4f) {
        obj->field40 = 2;
        find_ani2_part2(obj);
        obj->field1c = 5;

        *mk3_frame(thread, thread->frame + 1) = 0xc4a;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0)
        return -3;

    obj->field20 = 0x205;
    init_special_act(obj);
    obj->field1c = 3;
    ochar_sound(obj);
    obj->field40 = 2;
    get_char_ani2(obj);
    obj->a10 = obj->field40;
    obj->field1c = 0x30002;

    *mk3_frame(thread, thread->frame + 1) = 0xbf9;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_animate_a0_frames;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ---------------------------------------------------------- tl_do_mileena_prop
 *
 * armv7 0x00040d54, 616 bytes.  **Complete.**
 *
 * Mileena's spinning charge: five frames a revolution, twenty-four
 * revolutions, and three different endings depending on what it runs into.
 *
 *      token == 0:      obj->field1c = 0x219 ; init_special_act(obj)
 *                       obj->field1c = 0xa ; ochar_sound(obj)
 *                       obj->field40 = 4 ; do_first_a9_frame(obj)
 *                       token := 0xb6, park 3
 *
 *      token == 0xb6:   obj->field40 = 0x1a ; get_char_ani(obj)
 *                       obj->field40 += 4
 *                       obj->field1c = 0 ; obj->field20 = 0x4a
 *                       multi_adjust_xy(obj)
 *                       obj->field1c = 2 ; init_anirate(obj)
 *                       obj->field48 = 5 ; obj->a10 = 0x18
 *                       obj->field1c = 10.0 ; towards_x_vel(obj)
 *                       -- SPIN --
 *
 *      SPIN:            next_anirate(obj) ; sans_repell_3(obj)
 *                       obj->field1c = 0x16 ; strike_check_a0(obj)
 *                       if (obj->field5c != 0) -- CONNECTED --
 *                       token := 0xce, park 1
 *
 *      token == 0xce:   if (--obj->field48 > 0) -- SPIN --
 *                       obj->field48 = 1
 *                       if (--obj->a10 > 0) -- SPIN --
 *                       -- FINISH --
 *
 *      CONNECTED:       if (obj->field18 == 0) {
 *                           q_is_he_a_boss(obj)
 *                           if (obj->field5c == 0) {
 *                               obj->a10 = 0xb
 *                               next_anirate(obj)
 *                               token := 0xe1, park 1
 *                           }
 *                       }
 *                       -- BOUNCE OFF --
 *
 *      BOUNCE OFF:      obj->field1c = obj->field00->field18 = 0x61e
 *                       obj->field48 = (int16)obj->field08->field12
 *                       match_me_with_him(obj)
 *                       obj->field1c = 0x20 ; obj->field20 = 0
 *                       multi_adjust_xy(obj) ; face_opponent(obj)
 *                       (int16)obj->field08->field12 = (uint16)obj->field48
 *                       obj->field40 = 0x1a ; get_char_ani(obj)
 *                       obj->field40 += 4
 *                       obj->field08->field1c = -11.0
 *                       obj->field24 = obj->field08->field20 = 0.375
 *                       obj->field1c = 2 ; init_anirate(obj)
 *                       obj->field1c = 0.625 ; away_x_vel(obj)
 *                       token := 0x108, park 1
 *
 *      token == 0xe1:   if (--obj->a10 <= 0) -- FINISH --
 *                       next_anirate(obj)
 *                       token := 0xe1, park 1
 *
 *      token == 0x108:  next_anirate(obj)
 *                       y = (int16)obj->field08->field12
 *                       obj->field1c = y ; obj->field20 = *(proc + 0x40)
 *                       if (obj->field20 < y) { token := 0x108, park 1 }
 *                       obj->field1c = obj->field20 = obj->field24 = 0xd
 *                       obj->field28 = 2
 *                       token := 0x114, descend into t_flight
 *
 *      token == 0x114:  frame[frame].handler = t_jump_up_land_jump
 *
 *      FINISH:          stop_me_player(obj) ; ground_player(obj)
 *                       face_opponent(obj)
 *                       obj->field1c = &G + 0x428 ; update_tsl(obj)
 *                       frame[frame].handler = t_do_backup
 *
 *      otherwise:       return -3
 *
 * **Two counters, one loop.** 0x48 counts the five frames of a revolution and
 * 0x44 the twenty-four revolutions. When the inner one hits zero it is reset
 * to **1**, not to 5 -- so every revolution after the first is a single frame
 * long. The first turn is slow and the rest are not.
 *
 * **Hitting a boss is treated as being blocked.** Both take the same branch to
 * the bounce, and only a clean hit on somebody else goes to the eleven-frame
 * follow-through. There is no separate boss handling anywhere else in the
 * routine.
 *
 * **Her height is saved across the reposition.** 0x12 of the part is read
 * signed into 0x48, match_me_with_him and multi_adjust_xy move her, and then
 * the saved value is written back as a halfword -- so the bounce keeps her
 * vertical position and changes only where she is horizontally. The value goes
 * out sign-extended and comes back unsigned, which is what the two
 * instructions say.
 *
 * proc+0x40 is read as a word here, as in tl_do_lao_tele and tl_do_robo_tele.
 */
long tl_do_mileena_prop(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    int32_t  y;
    int      spin = 0, finish = 0, bounce = 0;

    if (token == 0x114)
        return mk3_install(thread, (MK3THREADFUNC)t_jump_up_land_jump);

    if (token == 0x108) {
        next_anirate(obj);
        y = *(int16_t *)((char *)obj->field08 + 0x12);
        obj->field1c = (uint32_t)y;
        obj->field20 = *(uint32_t *)((char *)obj->field00 + 0x40);

        if ((int32_t)obj->field20 < y) {        /* still coming down */
            *mk3_frame(thread, thread->frame + 1) = 0x108;
            thread->fieldfc = 1;
            return 1;
        }

        obj->field1c = 0xd;
        obj->field20 = 0xd;
        obj->field24 = 0xd;
        obj->field28 = 2;

        *mk3_frame(thread, thread->frame + 1) = 0x114;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0xe1) {
        obj->a10 -= 1;
        if ((long)obj->a10 > 0)
            goto hold_e1;                       /* one tail in the binary */
        finish = 1;
    } else if (token == 0xce) {
        obj->field48 -= 1;
        if ((long)obj->field48 > 0) {
            spin = 1;
        } else {
            obj->field48 = 1;                   /* one frame a turn from here */
            obj->a10 -= 1;
            if ((long)obj->a10 > 0)
                spin = 1;
            else
                finish = 1;
        }
    } else if (token == 0xb6) {
        obj->field40 = 0x1a;
        get_char_ani(obj);
        obj->field40 = obj->field40 + 4;
        obj->field1c = 0;
        obj->field20 = 0x4a;
        multi_adjust_xy(obj);
        obj->field1c = 2;
        init_anirate(obj);
        obj->field48 = 5;
        obj->a10 = 0x18;
        obj->field1c = 0xa0000u;                /* 10.0 */
        towards_x_vel(obj);
        spin = 1;
    } else if (token != 0) {
        return -3;
    }

    if (spin) {
        next_anirate(obj);
        sans_repell_3(obj);
        obj->field1c = 0x16;
        strike_check_a0(obj);

        if (obj->field5c == 0) {
            *mk3_frame(thread, thread->frame + 1) = 0xce;
            thread->fieldfc = 1;
            return 1;
        }

        bounce = 1;
        if (obj->field18 == 0) {
            q_is_he_a_boss(obj);
            if (obj->field5c == 0) {            /* a clean hit on a mortal */
                obj->a10 = 0xb;
hold_e1:
                next_anirate(obj);
                *mk3_frame(thread, thread->frame + 1) = 0xe1;
                thread->fieldfc = 1;
                return 1;
            }
        }
    }

    if (bounce) {
        obj->field1c = 0x61e;
        obj->field00->field18 = 0x61e;
        obj->field48 = (uint32_t)(int32_t)
            *(int16_t *)((char *)obj->field08 + 0x12);

        match_me_with_him(obj);
        obj->field1c = 0x20;
        obj->field20 = 0;
        multi_adjust_xy(obj);
        face_opponent(obj);
        *(int16_t *)((char *)obj->field08 + 0x12) = (int16_t)(uint16_t)
            obj->field48;                       /* put her back at that height */

        obj->field40 = 0x1a;
        get_char_ani(obj);
        obj->field40 = obj->field40 + 4;
        obj->field08->field1c = 0xfff50000u;    /* -11.0 */
        obj->field24 = 0x6000u;                 /* 0.375 */
        obj->field08->field20 = 0x6000u;
        obj->field1c = 2;
        init_anirate(obj);
        obj->field1c = 0xa000u;                 /* 0.625 */
        away_x_vel(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x108;
        thread->fieldfc = 1;
        return 1;
    }

    if (finish) {
        stop_me_player(obj);
        ground_player(obj);
        face_opponent(obj);
        obj->field1c = (uint32_t)(uintptr_t)(G_BYTES + 0x428);
        update_tsl(obj);
        return mk3_install(thread, (MK3THREADFUNC)t_do_backup);
    }

    obj->field1c = 0x219;
    init_special_act(obj);
    obj->field1c = 0xa;
    ochar_sound(obj);
    obj->field40 = 4;
    do_first_a9_frame(obj);

    *mk3_frame(thread, thread->frame + 1) = 0xb6;
    thread->fieldfc = 3;
    return 3;
}


/* ------------------------------------------------------------ tl_do_super_kang
 *
 * armv7 0x0003fa7c, 660 bytes.  **Complete.**
 *
 * Liu Kang's flying kick, and the only routine in this file that works out its
 * own speed from how far away the opponent is.
 *
 *      token == 0:      obj->field20 = 0x20e ; init_special_act(obj)
 *                       obj->field1c = 2 ; ochar_sound(obj)
 *                       obj->field1c = 5 ; ochar_sound(obj)
 *                       obj->field1c = 10.0 ; towards_x_vel(obj)
 *                       get_x_dist(obj)
 *                       d = obj->field28 - 0xa
 *                       if (d <= 0) d = 1
 *                       obj->field20 = 0xa
 *                       n = ((d << 16) / 10) >> 16
 *                       if (n <= 0) n = 1
 *                       obj->field48 = n
 *                       obj->field20 = 0x200000 / n
 *                       if (obj->field20 > 4.0) obj->field20 = 4.0
 *                       obj->field20 = -obj->field20
 *                       obj->field08->field1c = obj->field20
 *                       obj->field40 = 3 ; get_char_ani2(obj)
 *                       obj->field1c = 1 ; init_anirate(obj)
 *                       obj->a10 = 8
 *                       token := 0x598, park 1
 *
 *      token == 0x598:  if (--obj->a10 == 0) {
 *                           obj->a10 = 1
 *                           obj->field1c = 0x13 ; strike_check_a0(obj)
 *                           if (obj->field5c != 0) {
 *                               obj->field1c = obj->field00->field18 = 0x306
 *                               if (obj->field18 != 0) {          ; blocked
 *                                   obj->field20 = obj->field00->field18 = 0x612
 *                                   token := 0x5c8, descend into
 *                                                   t_super_kick_land
 *                               }
 *                               token := 0x5bf, descend into t_super_kick_land
 *                           }
 *                       }
 *                       next_anirate(obj)
 *                       if (--obj->field48 > 0) { token := 0x598, park 1 }
 *                       obj->field1c = obj->field20 = 0xd
 *                       obj->field34 = 0
 *                       obj->field24 = 0.625 ; obj->field28 = 0xfff
 *                       token := 0x5af, descend into t_flight_call
 *
 *      token == 0x5af:  obj->field40 = 3 ; find_ani2_part2(obj)
 *                       obj->field1c = 3
 *                       token := 0x5b4, descend into t_mframew
 *
 *      token == 0x5c8:  obj->field40 = 0 ; get_char_ani(obj)
 *                       do_next_a9_frame(obj) ; set_no_block(obj)
 *                       token := 0x5cd, park 0xa
 *
 *      token == 0x5b4:  frame[frame].handler = t_local_reaction_exit
 *      token == 0x5bf:  the same
 *      token == 0x5cd:  the same
 *
 *      otherwise:       return -3
 *
 * **The kick is divided into steps and the speed falls out of the arithmetic.**
 * The horizontal distance less ten is divided by ten to give a number of steps,
 * that number goes into 0x48 as the frame counter, and 32.0 divided by it
 * becomes the speed -- capped at 4.0 and then negated. So a distant opponent
 * gets more frames at a lower speed and a near one fewer frames at a higher
 * speed, and the product is roughly constant. The travel time varies; the
 * distance covered does not.
 *
 * Both the divide by ten and the divide by the step count are calls in the
 * binary: the first is the compiler's magic-number sequence (`0x66666667`,
 * `smull`, `asr #2`, minus the sign) and the second is `___divsi3`, because
 * this binary has no hardware integer divide.
 *
 * **Two clamps to one, in two different places.** A distance under ten and a
 * step count that rounds to nothing both write 1 into 0x28 and rejoin -- the
 * second one after the division has already been attempted. The move always
 * has at least one step.
 *
 * The strike is only tried on ONE frame: 0x44 counts eight down to zero, and
 * the check happens on the frame it reaches zero, after which 0x44 is set to 1
 * so it never fires again. Everything after that is the counter at 0x48
 * running the steps out.
 */
long tl_do_super_kang(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    int32_t  d, n;
    int      step = 0;

    if (token == 0x5b4 || token == 0x5bf || token == 0x5cd)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

    if (token == 0x5c8) {
        obj->field40 = 0;
        get_char_ani(obj);
        do_next_a9_frame(obj);
        set_no_block(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x5cd;
        thread->fieldfc = 0xa;
        return 0xa;
    }

    if (token == 0x5af) {
        obj->field40 = 3;
        find_ani2_part2(obj);
        obj->field1c = 3;

        *mk3_frame(thread, thread->frame + 1) = 0x5b4;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x598) {
        obj->a10 -= 1;
        if (obj->a10 == 0) {
            obj->a10 = 1;                       /* and never again */
            obj->field1c = 0x13;
            strike_check_a0(obj);
            if (obj->field5c != 0) {
                obj->field1c = 0x306;
                obj->field00->field18 = 0x306;

                if (obj->field18 != 0) {        /* he blocked it */
                    obj->field20 = 0x612;
                    obj->field00->field18 = 0x612;
                    *mk3_frame(thread, thread->frame + 1) = 0x5c8;
                } else {
                    *mk3_frame(thread, thread->frame + 1) = 0x5bf;
                }

                thread->frame = thread->frame + 1;
                mk3_frame(thread, thread->frame)[1] =
                    (uint32_t)(uintptr_t)t_super_kick_land;
                *mk3_frame(thread, thread->frame + 1) = 0;
                return 0;
            }
        }
        step = 1;
    } else if (token != 0) {
        return -3;
    }

    if (step) {
        next_anirate(obj);
        obj->field48 -= 1;
        if ((long)obj->field48 > 0) {
            *mk3_frame(thread, thread->frame + 1) = 0x598;
            thread->fieldfc = 1;
            return 1;
        }

        obj->field1c = 0xd;
        obj->field20 = 0xd;
        obj->field34 = 0;
        obj->field24 = 0xa000u;                 /* 0.625 */
        obj->field28 = 0xfff;

        *mk3_frame(thread, thread->frame + 1) = 0x5af;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_flight_call;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    obj->field20 = 0x20e;
    init_special_act(obj);
    obj->field1c = 2;
    ochar_sound(obj);
    obj->field1c = 5;
    ochar_sound(obj);
    obj->field1c = 0xa0000u;                    /* 10.0 */
    towards_x_vel(obj);

    get_x_dist(obj);
    d = (int32_t)obj->field28 - 0xa;
    obj->field28 = (uint32_t)d;
    if (d <= 0) {
        d = 1;                                  /* right on top of him */
        obj->field28 = 1;
    }

    obj->field20 = 0xa;
    n = ((d << 16) / 10) >> 16;                 /* 0x66666667, smull, asr */
    obj->field28 = (uint32_t)n;
    if (n <= 0) {
        n = 1;
        obj->field28 = 1;
    }

    obj->field48 = (uint32_t)n;
    obj->field20 = 0x200000u;
    obj->field20 = (uint32_t)mk3_sdiv(0x200000, (int32_t)n);    /* ___divsi3 */
    if ((long)obj->field20 > 0x40000)
        obj->field20 = 0x40000u;                /* 4.0 at most */
    obj->field20 = (uint32_t)(-(int32_t)obj->field20);
    obj->field08->field1c = obj->field20;

    obj->field40 = 3;
    get_char_ani2(obj);
    obj->field1c = 1;
    init_anirate(obj);
    obj->a10 = 8;

    *mk3_frame(thread, thread->frame + 1) = 0x598;
    thread->fieldfc = 1;
    return 1;
}


/* ------------------------------------------------------------- tl_do_swat_zoom
 *
 * armv7 0x0004068c, 688 bytes.  **Complete.**
 *
 * Stryker's low charge. It closes at twelve, catches the opponent, poses him,
 * and hands his thread away.
 *
 *      token == 0:      obj->field20 = 0x211 ; init_special_act(obj)
 *                       obj->field40 = 1 ; get_char_ani2(obj)
 *                       do_next_a9_frame(obj)
 *                       obj->field1c = 2 ; ochar_sound(obj)
 *                       obj->field1c = 12.0 ; towards_x_vel(obj)
 *                       obj->field08->field1c = -2.0
 *                       obj->field1c = obj->field08->field20 = 0x2400
 *                       sans_repell_3(obj)
 *                       obj->field1c = obj->field00->field28 = 0
 *                       token := 0x384, park 1
 *
 *      token == 0x384:  sans_repell_3(obj) ; zoom_damping(obj)
 *                       q_is_he_a_boss(obj)
 *                       if (!boss) {
 *                           obj->field1c = 0x15 ; strike_check_a0(obj)
 *                           if (obj->field5c != 0) {
 *                               if (obj->field18 != 0)
 *                                   frame[frame].handler = t_zoom_blocked
 *                               obj->field1c = 4 ; ochar_sound(obj)
 *                               stop_him(obj)
 *                               token := 0x3a7, park 1
 *                           }
 *                       }
 *                       obj->field20 = (int16)obj->field08->field12
 *                       obj->field1c = *(proc + 0x40)
 *                       if (obj->field1c > obj->field20)
 *                           { token := 0x384, park 1 }
 *                       stop_me_player(obj) ; ground_player(obj)
 *                       if (thread->frame > 0) { frame -= 1; return 0 }
 *                       frame[frame].handler = t_local_reaction_exit
 *
 *      token == 0x3a7:  sans_repell_3(obj) ; match_him_with_me_f(obj)
 *                       obj->field40 = 0x1001e ; pose_him_a9(obj)
 *                       obj->field20 = 0 ; obj->field1c = 0x40
 *                       adjust_him_xy(obj)
 *                       stop_me_player(obj) ; ground_player(obj)
 *                       stop_him(obj)
 *                       obj->field40 = 0x42 ; get_his_char_ani(obj)
 *                       obj->field48 = obj->field40
 *                       obj->a10 = obj->field00->him
 *                       obj->field40 = 1 ; get_char_ani2(obj)
 *                       find_part2(obj)
 *                       obj->field1c = 4 ; group_sound(obj)
 *                       rsnd_func(obj, 0xf)
 *                       obj->field1c = 3
 *                       token := 0x3ce, descend into t_double_mframew
 *
 *      token == 0x3ce:  obj->field38 = t_r_zoom_flipped
 *                       xfer_otherguy(obj)
 *                       obj->field00->field00->field40 = obj->field48
 *                       do_next_a9_frame(obj)
 *                       token := 0x3d3, park 0xa
 *
 *      token == 0x3d3:  obj->field1c = 6
 *                       token := 0x3d5, descend into t_mframew
 *
 *      token == 0x3d5:  if (thread->frame > 0) { frame -= 1; return 0 }
 *                       frame[frame].handler = t_local_reaction_exit
 *
 *      otherwise:       return -3
 *
 * **A boss is not hit and not missed -- he is skipped.** `q_is_he_a_boss`
 * coming back set branches past the strike check entirely, straight to the
 * height test, so the charge runs its course through him without ever asking
 * whether it connected.
 *
 * **The animation number is saved and given to the opponent.** 0x42 goes into
 * 0x40, `get_his_char_ani` turns it into a real animation, the result is kept
 * in 0x48, and two states later it is written into `proc->field00->field40` --
 * the other fighter's own slot -- immediately after `xfer_otherguy` hands his
 * thread to `t_r_zoom_flipped`. The pose and the thread that plays it are
 * transferred in that order, one state apart.
 *
 * **The dispatch's register is reloaded halfway.** 0x3a7 is in r8 for tokens
 * at or below it and 0x3d3 for tokens above, so the clean-hit path parks with
 * 0x3a7 and state 0x3ce parks with 0x3d3 -- the same instruction, two values,
 * decided by which side of the first comparison the token fell. The same trap
 * as t_reptile_dash_hit and t_decoy_proc.
 */
long tl_do_swat_zoom(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    int32_t  y;

    if (token == 0x3ce) {
        obj->field38 = (uint32_t)(uintptr_t)t_r_zoom_flipped;
        xfer_otherguy(obj);
        obj->field00->field00->field40 = obj->field48;
        do_next_a9_frame(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x3d3;
        thread->fieldfc = 0xa;
        return 0xa;
    }

    if (token == 0x3d3) {
        obj->field1c = 6;
        *mk3_frame(thread, thread->frame + 1) = 0x3d5;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x3d5) {
        if ((long)thread->frame > 0) {
            thread->frame -= 1;
            return 0;
        }
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }

    if (token == 0x3a7) {
        sans_repell_3(obj);
        match_him_with_me_f(obj);
        obj->field40 = 0x1001e;
        pose_him_a9(obj);
        obj->field20 = 0;
        obj->field1c = 0x40;
        adjust_him_xy(obj);
        stop_me_player(obj);
        ground_player(obj);
        stop_him(obj);

        obj->field40 = 0x42;
        get_his_char_ani(obj);
        obj->field48 = obj->field40;            /* keep it for two states */
        obj->a10 = obj->field00->him;

        obj->field40 = 1;
        get_char_ani2(obj);
        find_part2(obj);
        obj->field1c = 4;
        group_sound(obj);
        rsnd_func(obj, 0xf);
        obj->field1c = 3;

        *mk3_frame(thread, thread->frame + 1) = 0x3ce;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_double_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x384) {
        sans_repell_3(obj);
        zoom_damping(obj);

        q_is_he_a_boss(obj);
        if (obj->field5c == 0) {                /* a boss is simply skipped */
            obj->field1c = 0x15;
            strike_check_a0(obj);
            if (obj->field5c != 0) {
                if (obj->field18 != 0)          /* blocked */
                    return mk3_install(thread,
                                       (MK3THREADFUNC)t_zoom_blocked);
                obj->field1c = 4;
                ochar_sound(obj);
                stop_him(obj);
                *mk3_frame(thread, thread->frame + 1) = 0x3a7;
                thread->fieldfc = 1;
                return 1;
            }
        }

        y = *(int16_t *)((char *)obj->field08 + 0x12);
        obj->field20 = (uint32_t)y;
        obj->field1c = *(uint32_t *)((char *)obj->field00 + 0x40);

        if ((int32_t)obj->field1c > y) {         /* still above the floor */
            *mk3_frame(thread, thread->frame + 1) = 0x384;
            thread->fieldfc = 1;
            return 1;
        }

        stop_me_player(obj);
        ground_player(obj);
        if ((long)thread->frame > 0) {
            thread->frame -= 1;
            return 0;
        }
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }

    if (token != 0)
        return -3;

    obj->field20 = 0x211;
    init_special_act(obj);
    obj->field40 = 1;
    get_char_ani2(obj);
    do_next_a9_frame(obj);
    obj->field1c = 2;
    ochar_sound(obj);
    obj->field1c = 0xc0000u;                    /* 12.0 */
    towards_x_vel(obj);

    obj->field08->field1c = 0xfffe0000u;        /* -2.0 */
    obj->field1c = 0x2400u;
    obj->field08->field20 = 0x2400u;
    sans_repell_3(obj);
    obj->field1c = 0;
    obj->field00->field28 = 0;

    *mk3_frame(thread, thread->frame + 1) = 0x384;
    thread->fieldfc = 1;
    return 1;
}


/* ------------------------------------------------------------------- tl_bike3
 *
 * armv7 0x0003d9fc, 988 bytes.  **Complete.**
 *
 * The bike kick: a flying knee that carries the opponent along with it. Eight
 * states, three of them loops, and the counter for all three lives in the
 * PROC's 0x28 rather than in the object.
 *
 *      token == 0:      token := 0xb0b, park 1
 *
 *      token == 0xb0b:  sans_repell_3(obj) ; get_his_action(obj)
 *                       hit = (obj->field20 == 0x701 || obj->field20 == 0x302)
 *                       if (!hit) {
 *                           obj->field1c = 0x14 ; strike_check_a0(obj)
 *                           if (obj->field5c != 0) -- CONNECTED --
 *                       }
 *                       next_anirate(obj) ; distance_from_ground(obj)
 *                       if (obj->field1c > 0xbf) -- LET GO --
 *                       frame[frame].handler = tl_bike3
 *
 *      token == 0xb5a:  bike_hit_call(obj) ; distance_from_ground(obj)
 *                       if (obj->field1c > 0xe0) -- SETTLE --
 *                       if (--obj->field00->field28 <= 0) -- SETTLE --
 *                       obj->field00->field28 = obj->field1c
 *                       token := 0xb5a, park 1
 *
 *      token == 0xb6e:  bike_hit_call(obj)
 *                       if (--obj->field00->field28 <= 0) -- HAND OVER --
 *                       obj->field00->field28 = obj->field1c
 *                       token := 0xb6e, park 1
 *
 *      token == 0xb89:  obj->field40 = 0x10016 ; pose_a9_manual(obj)
 *                       face_opponent(obj)
 *                       obj->field20 = 2.0 ; obj->field1c = 0
 *                       obj->field24 = 0.5 ; obj->field28 = 0xfff
 *                       token := 0xb94, descend into t_flight
 *
 *      token == 0xb29:  if (thread->frame > 0) { frame -= 1; return 0 }
 *                       frame[frame].handler = t_local_reaction_exit
 *
 *      token == 0xb94:  frame[frame].handler = t_jump_up_land_jsrp
 *      token == 0xb9c:  the same
 *
 *      CONNECTED:       obj->field20 = obj->field00->field18 = 0x611
 *                       obj->field1c = clear_inviso ; call_a0_for_him(obj)
 *                       if (obj->field18 != 0) {          ; blocked
 *                           obj->field1c = 0
 *                           obj->field20 = -6.0 ; obj->field24 = 0.5
 *                           obj->field28 = 4
 *                           token := 0xb9c, descend into t_flight
 *                       }
 *                       obj->field48 = 0x30030 ; shake_a11(obj)
 *                       arg[f8++] = obj->field40
 *                       obj->field48 = 0x20 ; get_his_a11_ani(obj)
 *                       obj->field40 = obj->field48 ; find_part2(obj)
 *                       obj->field48 = obj->field40
 *                       obj->field40 = arg[--f8]
 *                       double_next_a9(obj)
 *                       xfer_him_to_flipped_pause(obj)
 *                       inc_his_p_hit(obj)
 *                       stop_me_player(obj) ; stop_him(obj)
 *                       obj->field1c = 4 ; init_anirate(obj)
 *                       obj->field00->field2c = 1
 *                       obj->field1c = 3.0 ; towards_x_vel(obj)
 *                       obj->field1c = obj->field08->field1c = -3.0
 *                       obj->a10->field1c = obj->field1c
 *                       obj->field00->field28 = 0x20
 *                       token := 0xb5a, park 1
 *
 *      SETTLE:          obj->field1c = 1.0 ; towards_x_vel(obj)
 *                       obj->field1c = obj->field08->field1c = 0.5
 *                       obj->a10->field1c = obj->field1c
 *                       obj->field00->field28 = 0x10
 *                       token := 0xb6e, park 1
 *
 *      LET GO:          obj->field1c = obj->field00->field18 = 0x611
 *                       clear_nocol(obj)
 *                       obj->field1c = obj->field20 = 0xd
 *                       obj->field24 = 0.625 ; obj->field28 = 4
 *                       obj->field34 = t_bike_scan_call
 *                       token := 0xb29, descend into t_flight_call
 *
 *      HAND OVER:       obj->field1c = &G + 0x444 ; update_tsl(obj)
 *                       obj->field38 = t_r_post_bike ; takeover_him(obj)
 *                       obj->field20 = 0x61a ; set_his_p_action(obj)
 *                       obj->field40 = 1 ; get_char_ani2(obj)
 *                       obj->field54 = 3 ; find_part_a14(obj)
 *                       obj->field1c = 4
 *                       token := 0xb89, descend into t_mframew
 *
 *      otherwise:       return -3
 *
 * **The ride is thirty-two frames and then sixteen.** 0x20 goes into the
 * proc's 0x28 when the knee connects and the 0xb5a loop counts it down; when
 * it runs out (or the pair gets too high) the speed drops to 0.5 and 0x10 goes
 * into the same field for the 0xb6e loop. Two phases of carrying him, fast
 * then slow, sharing one counter in the opponent's own proc.
 *
 * **Two actions count as already-hit.** 0x701 and 0x302 in his action skip the
 * strike check entirely -- the routine takes the boolean, ORs the two
 * comparisons together, and only tests for a strike when neither matched. So a
 * fighter already in the right reaction is carried without being hit again.
 *
 * **The other object is reached through 0x44 as a pointer.** Both the connect
 * and the settle write a speed into `obj->a10->field1c`, which is the argument
 * slot holding an object address rather than a number -- the same borrowing
 * `t_air_sleep3` does for its chase target.
 *
 * `clear_inviso` is put into 0x1c and handed to `call_a0_for_him`: an address
 * in the argument slot, for a routine whose job is to run it on the opponent.
 *
 * 0x40 is pushed on the thread's second stack across `get_his_a11_ani` and
 * `find_part2` and popped back afterwards, the same borrow-one-slot idiom as
 * t_pounce_jsrp -- two callees wanting the same field for different things.
 */
long tl_bike3(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    int      connected = 0, settle = 0, letgo = 0, handover = 0;
    long     act;

    if (token == 0xb94 || token == 0xb9c)
        return mk3_install(thread, (MK3THREADFUNC)t_jump_up_land_jsrp);

    if (token == 0xb29) {
        if ((long)thread->frame > 0) {
            thread->frame -= 1;
            return 0;
        }
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }

    if (token == 0xb89) {
        obj->field40 = 0x10016;
        pose_a9_manual(obj);
        face_opponent(obj);
        obj->field20 = 0x20000u;                /* 2.0 */
        obj->field1c = 0;
        obj->field24 = 0x20000u - 0x18000u;     /* 0.5 */
        obj->field28 = 0xfff;

        *mk3_frame(thread, thread->frame + 1) = 0xb94;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0xb5a) {
        bike_hit_call(obj);
        distance_from_ground(obj);
        if ((long)obj->field1c > 0xe0) {
            settle = 1;
        } else {
            obj->field1c = obj->field00->field28 - 1;
            if ((long)obj->field1c <= 0) {
                settle = 1;
            } else {
                obj->field00->field28 = obj->field1c;
                *mk3_frame(thread, thread->frame + 1) = 0xb5a;
                thread->fieldfc = 1;
                return 1;
            }
        }
    } else if (token == 0xb6e) {
        bike_hit_call(obj);
        obj->field1c = obj->field00->field28 - 1;
        if ((long)obj->field1c <= 0) {
            handover = 1;
        } else {
            obj->field00->field28 = obj->field1c;
            *mk3_frame(thread, thread->frame + 1) = 0xb6e;
            thread->fieldfc = 1;
            return 1;
        }
    } else if (token == 0xb0b) {
        sans_repell_3(obj);
        get_his_action(obj);
        act = (long)obj->field20;

        if (act != 0x701 && act != 0x302) {     /* not already reacting */
            obj->field1c = 0x14;
            strike_check_a0(obj);
            if (obj->field5c != 0)
                connected = 1;
        }

        if (!connected) {
            next_anirate(obj);
            distance_from_ground(obj);
            if ((long)obj->field1c > 0xbf)
                letgo = 1;
            else
                return mk3_install(thread, (MK3THREADFUNC)tl_bike3);
        }
    } else if (token != 0) {
        return -3;
    }

    if (connected) {
        obj->field20 = 0x611;
        obj->field00->field18 = 0x611;
        obj->field1c = (uint32_t)(uintptr_t)clear_inviso;
        call_a0_for_him(obj);

        if (obj->field18 != 0) {                /* he blocked it */
            obj->field1c = 0;
            obj->field20 = 0xfffa0000u;                 /* -6.0 */
            obj->field24 = 0xfffa0000u + 0x68000u;      /* 0.5 */
            obj->field28 = 4;

            *mk3_frame(thread, thread->frame + 1) = 0xb9c;
            thread->frame = thread->frame + 1;
            mk3_frame(thread, thread->frame)[1] =
                (uint32_t)(uintptr_t)t_flight;
            *mk3_frame(thread, thread->frame + 1) = 0;
            return 0;
        }

        obj->field48 = 0x30030;
        shake_a11(obj);

        *mk3_arg(thread, thread->fieldf8) = obj->field40;
        thread->fieldf8 += 1;

        obj->field48 = 0x20;
        get_his_a11_ani(obj);
        obj->field40 = obj->field48;
        find_part2(obj);
        obj->field48 = obj->field40;

        thread->fieldf8 -= 1;
        obj->field40 = *mk3_arg(thread, thread->fieldf8);

        double_next_a9(obj);
        xfer_him_to_flipped_pause(obj);
        inc_his_p_hit(obj);
        stop_me_player(obj);
        stop_him(obj);
        obj->field1c = 4;
        init_anirate(obj);
        *(uint32_t *)((char *)obj->field00 + 0x2c) = 1;
        obj->field1c = 0x30000u;                /* 3.0 */
        towards_x_vel(obj);
        obj->field1c = 0xfffd0000u;             /* -3.0 */
        obj->field08->field1c = obj->field1c;
        ((MK3OBJ *)(uintptr_t)obj->a10)->field1c = obj->field1c;
        obj->field1c = 0x20;

        obj->field00->field28 = obj->field1c;
        *mk3_frame(thread, thread->frame + 1) = 0xb5a;
        thread->fieldfc = 1;
        return 1;
    }

    if (settle) {
        obj->field1c = 0x10000u;                /* 1.0 */
        towards_x_vel(obj);
        obj->field1c = 0x8000u;                 /* 0.5 */
        obj->field08->field1c = obj->field1c;
        ((MK3OBJ *)(uintptr_t)obj->a10)->field1c = obj->field1c;
        obj->field1c = 0x10;

        obj->field00->field28 = obj->field1c;
        *mk3_frame(thread, thread->frame + 1) = 0xb6e;
        thread->fieldfc = 1;
        return 1;
    }

    if (letgo) {
        obj->field1c = 0x611;
        obj->field00->field18 = 0x611;
        clear_nocol(obj);
        obj->field1c = 0xd;
        obj->field20 = 0xd;
        obj->field24 = 0xa000u;                 /* 0.625 */
        obj->field28 = 4;
        obj->field34 = (uint32_t)(uintptr_t)t_bike_scan_call;

        *mk3_frame(thread, thread->frame + 1) = 0xb29;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_flight_call;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (handover) {
        obj->field1c = (uint32_t)(uintptr_t)(G_BYTES + 0x440 + 4);
        update_tsl(obj);
        obj->field38 = (uint32_t)(uintptr_t)t_r_post_bike;
        takeover_him(obj);
        obj->field20 = 0x61a;
        set_his_p_action(obj);
        obj->field40 = 1;
        get_char_ani2(obj);
        obj->field54 = 3;
        find_part_a14(obj);
        obj->field1c = 4;

        *mk3_frame(thread, thread->frame + 1) = 0xb89;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    *mk3_frame(thread, thread->frame + 1) = 0xb0b;
    thread->fieldfc = 1;
    return 1;
}


/* ---------------------------------------------------------------- t_bike_call
 *
 * armv7 0x0003f630, 1020 bytes.  **Complete.** The largest routine in the file.
 *
 * The bike kick from the caller's side: line the opponent up, take him over,
 * and then run the ride frame by frame until his counter runs out.
 *
 *      token == 0:      if (obj->field48 != 0) {
 *                           obj->field48 -= 1
 *                           -- UP A LEVEL --
 *                       }
 *                       obj->field1c = 0x12 ; strike_check_a0(obj)
 *                       if (obj->field5c == 0) -- UP A LEVEL --
 *                       -- CAUGHT HIM, below --
 *
 *      token == 0x640:  obj->a10 = obj->field00->him
 *                       obj->field40 = 2 ; get_char_ani2(obj)
 *                       obj->field1c = 3 ; init_anirate(obj)
 *                       obj->field00->field2c = 8
 *                       obj->field1c = 0x30
 *                       -- RIDE --
 *
 *      token == 0x673:  sans_repell_3(obj)
 *                       obj->field1c = obj->field00->field28 - 1
 *                       if (obj->field1c != 0) -- RIDE --
 *                       obj->field1c = &G + 0x444 ; update_tsl(obj)
 *                       obj->field38 = t_r_bike_kicked_done
 *                       takeover_him(obj)
 *                       obj->field1c = obj->field20 = 0
 *                       obj->field24 = 0.25 ; obj->field28 = 4
 *                       token := 0x686, descend into t_flight
 *
 *      token == 0x686:  zero_turbo_bar(obj)
 *                       frame[frame].handler = t_local_reaction_exit
 *
 *      token == 0x697:  frame[frame].handler = t_jump_up_land_jump
 *
 *      RIDE:            obj->field00->field28 = obj->field1c
 *                       next_anirate(obj)
 *                       obj->field1c = obj->field00->field2c - 1
 *                       if (obj->field1c == 0) {
 *                           rsnd_func(obj, 8) ; obj->field1c = 8
 *                       }
 *                       obj->field00->field2c = obj->field1c
 *                       obj->field1c = obj->field00->field20
 *                       if (obj->field1c == 3) -- SHAKE, below --
 *                       obj->field1c = obj->field00->him->field18
 *                       if (obj->field1c == 0) {
 *                           get_x_dist(obj)
 *                           if (obj->field28 <= 0x30) stop_me_player(obj)
 *                       }
 *                       token := 0x673, park 1
 *
 *      SHAKE:           arg[f8++] = obj->field48
 *                       obj->field48 = 0x30003 ; shake_a11(obj)
 *                       obj->field48 = arg[--f8]
 *                       arg[f8++] = obj->field40
 *                       obj->field40 = obj->field48
 *                       do_his_next_a9_frame(obj)
 *                       obj->field48 = obj->field40
 *                       obj->field40 = arg[--f8]
 *                       -- rejoin the RIDE at the him->field18 test --
 *
 *      CAUGHT HIM:      -- UP A LEVEL, twice, each followed by a COLLAPSE --
 *                       stop_me_player(obj) ; lights_on_hit(obj)
 *                       if (obj->field18 != 0) {              ; blocked
 *                           obj->field00->field18 = 0x61d
 *                           obj->field40 = 0x10002
 *                           obj->field1c = 1.0
 *                           obj->field20 = -5.0
 *                           obj->field24 = 0.25 ; obj->field28 = 4
 *                           token := 0x697, descend into t_flight
 *                       }
 *                       obj->field1c = 0x20d
 *                       obj->field38 = t_biked_suspend ; takeover_him(obj)
 *                       is_he_facing_me(obj)
 *                       if (!facing) call_for_him(obj, flip_multi)
 *                       is_he_airborn(obj)
 *                       if (!airborne) {
 *                           match_him_with_me_f(obj) ; ground_him(obj)
 *                           obj->field1c = -0x20 ; adjust_him_x(obj)
 *                       } else {
 *                           get_x_dist(obj)
 *                           if (obj->field28 <= 0x30) {
 *                               obj->field38 = (int16)him->field12
 *                               match_him_with_me_f(obj)
 *                               (int16)him->field12 = (uint16)obj->field38
 *                               obj->field1c = -0x20 ; adjust_him_x(obj)
 *                           }
 *                       }
 *                       obj->field40 = 0x20 ; get_his_char_ani(obj)
 *                       find_part2(obj) ; obj->field48 = obj->field40
 *                       obj->field1c = -5.0 ; away_x_vel(obj)
 *                       obj->field1c = 5.0 ; away_x_vel_him(obj)
 *                       token := 0x640, park 1
 *
 *      UP A LEVEL:      if (thread->frame > 0) frame -= 1
 *                       else frame[frame].handler = t_local_reaction_exit
 *
 *      COLLAPSE:        handler          = frame[frame+1].handler
 *                       frame[frame+1].w0 = frame[frame+2].w0
 *                       frame[frame].handler = handler
 *
 *      otherwise:       return -3
 *
 * **Catching him unwinds two levels, not one.** The hit path goes up a level,
 * collapses the level above down into it, and then does both again -- so the
 * two frames the call was made through are folded away before the ride starts.
 * It is the same collapse t_hover_sleep_1 uses, run twice in a row, and it is
 * the only place in the file where it appears more than once.
 *
 * **Three fields of the opponent's proc drive the whole ride.** 0x28 is the
 * frame counter, 0x2c is a sub-counter that fires a sound every eight frames
 * and resets to 8, and 0x20 is a mode: exactly 3 means shake him this frame.
 * None of them belongs to the fighter doing the kicking.
 *
 * **The shake borrows two fields in a nest.** 0x48 goes on the argument stack
 * across `shake_a11`, comes back, and is copied into 0x40 -- and 0x40's old
 * value goes on the same stack across `do_his_next_a9_frame` before being
 * restored. Two callees, two fields, one stack, and the pushes do not overlap.
 *
 * **Lining him up has three cases and they share their tail.** Not facing gets
 * a flip; on the ground gets matched, grounded and nudged; in the air gets
 * nudged only when he is within 0x30, and then only after his height is saved
 * and put back around the match. All three fall into the same six calls that
 * set the animation and the two velocities.
 *
 * `obj->field38` is a handler slot in one place and a saved sixteen-bit height
 * in another, four instructions apart in the same state.
 */
long t_bike_call(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    MK3OBJ  *him;
    uint32_t below, handler;
    int      ride = 0, caught = 0;

    if (token == 0x697)
        return mk3_install(thread, (MK3THREADFUNC)t_jump_up_land_jump);

    if (token == 0x686) {
        zero_turbo_bar(obj);
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }

    if (token == 0x673) {
        sans_repell_3(obj);
        obj->field1c = obj->field00->field28 - 1;
        if (obj->field1c != 0) {
            ride = 1;
        } else {
            obj->field1c = (uint32_t)(uintptr_t)(G_BYTES + 0x440 + 4);
            update_tsl(obj);
            obj->field38 = (uint32_t)(uintptr_t)t_r_bike_kicked_done;
            takeover_him(obj);
            obj->field1c = 0;
            obj->field20 = 0;
            obj->field24 = 0x4000u;             /* 0.25 */
            obj->field28 = 4;

            *mk3_frame(thread, thread->frame + 1) = 0x686;
            thread->frame = thread->frame + 1;
            mk3_frame(thread, thread->frame)[1] =
                (uint32_t)(uintptr_t)t_flight;
            *mk3_frame(thread, thread->frame + 1) = 0;
            return 0;
        }
    } else if (token == 0x640) {
        obj->a10 = obj->field00->him;
        obj->field40 = 2;
        get_char_ani2(obj);
        obj->field1c = 3;
        init_anirate(obj);
        *(uint32_t *)((char *)obj->field00 + 0x2c) = 8;
        obj->field1c = 0x30;
        ride = 1;
    } else if (token == 0) {
        if (obj->field48 != 0) {
            obj->field48 -= 1;
        } else {
            obj->field1c = 0x12;
            strike_check_a0(obj);
            if (obj->field5c != 0)
                caught = 1;
        }

        if (!caught) {                          /* UP A LEVEL */
            if ((long)thread->frame > 0) {
                thread->frame -= 1;
                return 0;
            }
            return mk3_install(thread,
                               (MK3THREADFUNC)t_local_reaction_exit);
        }
    } else {
        return -3;
    }

    if (ride) {
        obj->field00->field28 = obj->field1c;
        next_anirate(obj);

        obj->field1c = *(uint32_t *)((char *)obj->field00 + 0x2c) - 1;
        if (obj->field1c == 0) {
            rsnd_func(obj, 8);
            obj->field1c = 8;
        }
        *(uint32_t *)((char *)obj->field00 + 0x2c) = obj->field1c;

        obj->field1c = obj->field00->field20;
        if (obj->field1c == 3) {                /* shake him this frame */
            *mk3_arg(thread, thread->fieldf8) = obj->field48;
            thread->fieldf8 += 1;
            obj->field48 = 0x30003;
            shake_a11(obj);
            thread->fieldf8 -= 1;
            obj->field48 = *mk3_arg(thread, thread->fieldf8);

            *mk3_arg(thread, thread->fieldf8) = obj->field40;
            thread->fieldf8 += 1;
            obj->field40 = obj->field48;
            do_his_next_a9_frame(obj);
            obj->field48 = obj->field40;
            thread->fieldf8 -= 1;
            obj->field40 = *mk3_arg(thread, thread->fieldf8);
        }

        him = (MK3OBJ *)(uintptr_t)obj->field00->him;
        obj->field1c = him->field18;
        if (obj->field1c == 0) {
            get_x_dist(obj);
            if ((long)obj->field28 <= 0x30)
                stop_me_player(obj);
        }

        *mk3_frame(thread, thread->frame + 1) = 0x673;
        thread->fieldfc = 1;
        return 1;
    }

    /* caught him: up a level and collapse, twice */
    if ((long)thread->frame > 0) {
        thread->frame -= 1;
    } else {
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_local_reaction_exit;
        *mk3_frame(thread, thread->frame + 1) = 0;
    }
    below   = thread->frame;
    handler = mk3_frame(thread, below + 1)[1];
    *mk3_frame(thread, below + 1) = *mk3_frame(thread, below + 2);
    mk3_frame(thread, below)[1] = handler;

    if ((long)thread->frame > 0) {
        thread->frame -= 1;
    } else {
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_local_reaction_exit;
        *mk3_frame(thread, thread->frame + 1) = 0;
    }
    below   = thread->frame;
    handler = mk3_frame(thread, below + 1)[1];
    *mk3_frame(thread, below + 1) = *mk3_frame(thread, below + 2);
    mk3_frame(thread, below)[1] = handler;

    stop_me_player(obj);
    lights_on_hit(obj);

    if (obj->field18 != 0) {                    /* he blocked it */
        obj->field00->field18 = 0x61d;
        obj->field40 = 0x10002;
        obj->field1c = 0x10002u - 2;            /* 1.0 */
        obj->field20 = (0x10002u - 2) - 0x60000u;       /* -5.0 */
        obj->field24 = ((0x10002u - 2) - 0x60000u) + 0x54000u;  /* 0.25 */
        obj->field28 = 4;

        *mk3_frame(thread, thread->frame + 1) = 0x697;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    obj->field1c = 0x20d;
    obj->field38 = (uint32_t)(uintptr_t)t_biked_suspend;
    takeover_him(obj);

    is_he_facing_me(obj);
    if (obj->field5c == 0)
        call_for_him(obj, flip_multi);          /* turn him round first */

    is_he_airborn(obj);
    if (obj->field5c == 0) {                    /* on the ground */
        match_him_with_me_f(obj);
        ground_him(obj);
        goto place;                             /* one tail in the binary */
    } else {
        get_x_dist(obj);
        if ((long)obj->field28 <= 0x30) {       /* close enough to place */
            him = (MK3OBJ *)(uintptr_t)obj->field00->him;
            obj->field38 = (uint32_t)(int32_t)
                *(int16_t *)((char *)him + 0x12);
            match_him_with_me_f(obj);
            him = (MK3OBJ *)(uintptr_t)obj->field00->him;
            *(int16_t *)((char *)him + 0x12) =
                (int16_t)(uint16_t)obj->field38;
place:
            obj->field1c = (uint32_t)-0x20;
            adjust_him_x(obj);
        }
    }

    obj->field40 = 0x20;
    get_his_char_ani(obj);
    find_part2(obj);
    obj->field48 = obj->field40;
    obj->field1c = 0xfffb0000u;                 /* -5.0 */
    away_x_vel(obj);
    obj->field1c = 0x50000u;                    /* 5.0 */
    away_x_vel_him(obj);

    *mk3_frame(thread, thread->frame + 1) = 0x640;
    thread->fieldfc = 1;
    return 1;
}
