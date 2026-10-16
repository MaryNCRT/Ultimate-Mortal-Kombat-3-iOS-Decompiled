/*
 * moves.c -- gamecode/logic/moves.c, decompiled.
 *
 * armv7 0x000501ac .. 0x00054e48, 357 functions in nineteen kilobytes. The
 * median is thirty-six bytes and three hundred of them are under sixty-four:
 * this file is a directory of names, not of code. A typical entry is a
 * character, a button and a range -- `ermac_block_close`, `sz_lk_close` -- and
 * its whole body is which table to hand to `secret_move_search`.
 *
 * The small ones in this first pass were matched by `tools/microfn.py`, which
 * checks a whole body against a template and refuses anything with one
 * instruction out of place. That is a stricter reading than doing three
 * hundred by eye, not a looser one: a body with an extra store does not get
 * emitted with the store dropped, it lands on the unrecognised list and gets
 * read by hand.
 *
 * The shapes it recognises here:
 *
 *   125  a table and a tail call to `secret_move_search`
 *    83  the frame-push family, a constant into 0x1c and a handler out of a
 *        pointer slot
 *    17  two constants and a tail call
 *     2  a bare tail call
 *     2  `obj->field5c = <constant>`
 *     1  `bx lr`, and nothing else
 *
 * Eighty-six more are larger or one-of-a-kind and are read individually.
 */

#include "mk3logic.h"

/* The callees these reach, declared from what the call sites
 * pass. One written later with a different signature will
 * conflict here, which is what the check is for. */
void q_animal_dist(MK3OBJ *obj);
void q_fatal_dist(MK3OBJ *obj);

/* The eleven handlers DoASpecial reaches that nothing else in this file names.
 * Every one of them arrives through a pointer slot except t_do_mercy and
 * t_do_baby, which are pc-relative -- so these nine live in other translation
 * units and those two live here. */
long tl_do_square_wave(MK3THREAD *thread);    /* pointer slot 0x000f3138 */
long t_do_pit_fatality(MK3THREAD *thread);    /* pointer slot 0x000f3130 */
long t_do_animality(MK3THREAD *thread);       /* pointer slot 0x000f31ac */
long t_do_friendship(MK3THREAD *thread);      /* pointer slot 0x000f3160 */
long t_drone_mercy(MK3THREAD *thread);        /* pointer slot 0x000f3144 */
long t_drone_do_fatality1(MK3THREAD *thread); /* pointer slot 0x000f3148 */
long t_drone_do_fatality2(MK3THREAD *thread); /* pointer slot 0x000f319c */
long t_drone_animality(MK3THREAD *thread);    /* pointer slot 0x000f315c */
long t_drone_babality(MK3THREAD *thread);     /* pointer slot 0x000f3168 */
long t_drone_friendship(MK3THREAD *thread);   /* pointer slot 0x000f31b8 */
long t_d_background_fatal(MK3THREAD *thread); /* pointer slot 0x000f31b0 */
extern long *RoundParam;                      /* pointer slot -> 0x0038ed04 */
extern uint32_t scom_bomb_mid[];           /* 0x0016a2e4 */
extern uint32_t scom_bomb_mid_four[];      /* 0x0016a318 */
void ground_ochar(MK3OBJ *obj);
void player_normpal(MK3OBJ *obj);
void tsound_func(MK3OBJ *obj, uint32_t sound);
void init_special_act(MK3OBJ *obj);
void stop_me_player(MK3OBJ *obj);
void ochar_sound(MK3OBJ *obj);
void clear_shadow_bit(MK3OBJ *obj);
void get_char_ani(MK3OBJ *obj);
long do_next_a9_frame(MK3OBJ *obj);
void update_tsl(MK3OBJ *obj);
void StartThreadAt(MK3THREAD *thread, MK3THREADFUNC fn);
long t_victory_animation(MK3THREAD *thread);
long t_master_proc_mercy(MK3THREAD *thread);
long t_mercy_start(MK3THREAD *thread);
extern uint32_t scom_robo2_tele[];         /* 0x0016a380 */
extern uint32_t scom_robo_air_grab[];      /* 0x0016a3b4 */
extern uint32_t sm_smoke_bc[];             /* 0x0016b9bc */
extern uint32_t scom_sky_zap_on_4but[];    /* 0x0016a4b8 */
extern uint32_t scom_sky_zap_on[];         /* 0x0016a4ec */
extern uint32_t scom_sky_zap_behind[];     /* 0x0016a520 */
extern uint32_t scom_sky_zap_front[];      /* 0x0016a554 */
long t_do_fatality_1(MK3THREAD *thread);
long t_do_fatality_2(MK3THREAD *thread);
long is_he_right(MK3OBJ *obj);
void get_bcq_next_pointer_idx(MK3OBJ *obj, long which);
void get_jcq_next_pointer_idx(MK3OBJ *obj, long which);
long four_button_switch(MK3OBJ *obj, long button);
void previous_q_entry(MK3OBJ *obj);
extern uint32_t scom_bike[];              /* 0x0016a450 */
long t_do_back_breaker(MK3THREAD *thread);
extern uint32_t scom_sonya_zap[];        /* 0x0016a484 */
long t_local_reaction_exit(MK3THREAD *thread);
uint32_t get_strength(uint32_t index);
void fastxfer_thread(MK3OBJ *obj, MK3THREAD *thread);
extern uint32_t scom_lia_anglez[];       /* 0x0016a41c */
void get_his_dfe(MK3OBJ *obj);
long is_he_airborn(MK3OBJ *obj);
long t_dizzy_sleep(MK3THREAD *thread);
extern uint32_t scom_robo_tele[];        /* 0x0016a34c */
void get_y_dist(MK3OBJ *obj);
long t_air_sleep3(MK3THREAD *thread);
long t_do_air_slam(MK3THREAD *thread);
extern uint32_t scom_fly[];               /* 0x0016a3e8 */
extern uint32_t scom_lao_zap[];           /* 0x0016a5f0 */
void distance_from_ground(MK3OBJ *obj);
long CountThreads(uint32_t pid);
long is_he_facing_me(MK3OBJ *obj);
extern uint32_t scom_lao_angle_kick[];   /* 0x0016a5bc */
long t_shang_morph(MK3THREAD *thread);
extern uint32_t scom_lao_teleport[];     /* 0x0016a588 */
void q_am_i_cornered(MK3OBJ *obj);
/* Eight function pointers, called as (obj, other). The extent is
 * what check_tsl allows -- an index above 7 is reset to 0 -- not a
 * measured array size. */
extern void (*P32 xfer_types_table[])(MK3OBJ *obj, MK3OBJ *other);
/* A call goes through a plain function pointer: clang cannot call through a
 * 32-bit one (P32) in the 64-bit build. The same pointer elsewhere. */
typedef void (*XFER_FN)(MK3OBJ *obj, MK3OBJ *other);
extern uint32_t scom_robo_zap1[];        /* 0x0016a278 */
extern uint32_t scom_robo_zap2[];        /* 0x0016a2ac */
extern MK3THREAD *mytc;  /* slot 0x0016566c -> 0x0038ef3c; the host variable holds the slot's contents, as in mk3.c */
void *GetThreadFunc(MK3THREAD *thread);
long t_fatality_wait(MK3THREAD *thread);
extern long *RoundParam;                 /* pointer slot -> 0x0038ed04 */
void get_tsl_px(MK3OBJ *obj, MK3OBJ *ref);
uint32_t four_button_bits(MK3OBJ *obj, uint32_t bits);
void DoASpecial(MK3OBJ *obj, uint32_t which);
void q_is_he_a_boss(MK3OBJ *obj);
void check_sonya_legs(MK3OBJ *obj);
void q_is_he_cornered(MK3OBJ *obj);
extern uint32_t sm_sz_lpc[];              /* 0x0016c640 */
long am_i_airborn(MK3OBJ *obj);
void get_his_action(MK3OBJ *obj);
long is_stick_away(MK3OBJ *obj);
long is_stick_down(MK3OBJ *obj);
void q_mercy(MK3OBJ *obj);
void q_fatality_req(MK3OBJ *obj);
void free_xfer(MK3OBJ *obj, MK3OBJ *other);
/* Read from the binary in joy.c: nothing is deliberately left in r0.
 * The `long` this used to be declared as came from a call site that
 * ignored the result. */
void get_x_dist(MK3OBJ *obj);
long stick_look_lr(MK3OBJ *obj, uint32_t a, uint32_t b,
                   uint32_t *pair);
void button_bit_check(MK3OBJ *obj);
/* Its two paths disagree about producing a value -- one falls through
 * after q_mercy, the other tail-calls fatality_xfer -- so it computes
 * none. Declared long at first; the compiler caught it. */
void animality_xfer(MK3OBJ *obj, MK3OBJ *other);
void q_friend(MK3OBJ *obj);
void q_jade_flash(MK3OBJ *obj);
void q_scorp_tele(MK3OBJ *obj);
long is_he_joy(MK3OBJ *obj);
long mercy_xfer(MK3OBJ *obj, MK3OBJ *other);
long secret_move_search(MK3OBJ *obj, uint32_t arg, uint32_t *table);
void slide_check(MK3OBJ *obj, MK3OBJ *other);

/* q_yes -- armv7 0x000501ac, 8 bytes.  **Complete.**
 *
 *      obj->field5c = 1
 */
static void q_yes(MK3OBJ *obj)
{
    obj->field5c = 1;
}

/* q_no -- armv7 0x000501b4, 8 bytes.  **Complete.**
 *
 *      obj->field5c = 0
 */
static void q_no(MK3OBJ *obj)
{
    obj->field5c = 0;
}

/* t_do_ermac_slam -- armv7 0x0005037c, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x1c
 *      frame[frame].handler = t_do_stationary
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f3154 rather than as a
 * link-time constant, so it lives in another translation unit. */
long t_do_stationary(struct MK3THREAD *thread);

long t_do_ermac_slam(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x1c;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_stationary);
}

/* t_do_reptile_inv -- armv7 0x000503b8, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x1b
 *      frame[frame].handler = t_do_stationary
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f3154 rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_reptile_inv(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x1b;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_stationary);
}

/* t_do_jade_flash -- armv7 0x000503f4, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x1a
 *      frame[frame].handler = t_do_stationary
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f3154 rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_jade_flash(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x1a;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_stationary);
}

/* t_do_fan_lift -- armv7 0x00050430, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x19
 *      frame[frame].handler = t_do_stationary
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f3154 rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_fan_lift(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x19;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_stationary);
}

/* t_do_baby -- armv7 0x0005046c, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x18
 *      frame[frame].handler = t_do_stationary
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f3154 rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_baby(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x18;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_stationary);
}

/* t_do_swat_gun -- armv7 0x000504a8, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0xb
 *      frame[frame].handler = t_do_stationary
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f3154 rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_swat_gun(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0xb;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_stationary);
}

/* t_do_kano_swipe -- armv7 0x000504e4, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0xa
 *      frame[frame].handler = t_do_stationary
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f3154 rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_kano_swipe(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0xa;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_stationary);
}

/* t_do_leg_throw -- armv7 0x00050520, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x9
 *      frame[frame].handler = t_do_stationary
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f3154 rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_leg_throw(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x9;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_stationary);
}

/* t_do_inviso -- armv7 0x0005055c, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x8
 *      frame[frame].handler = t_do_stationary
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f3154 rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_inviso(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x8;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_stationary);
}

/* t_do_quake -- armv7 0x00050598, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x7
 *      frame[frame].handler = t_do_stationary
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f3154 rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_quake(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x7;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_stationary);
}

/* t_do_noogy -- armv7 0x000505d4, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x6
 *      frame[frame].handler = t_do_stationary
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f3154 rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_noogy(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x6;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_stationary);
}

/* t_do_shake -- armv7 0x00050610, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x5
 *      frame[frame].handler = t_do_stationary
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f3154 rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_shake(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x5;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_stationary);
}

/* t_do_reflect -- armv7 0x0005064c, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x4
 *      frame[frame].handler = t_do_stationary
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f3154 rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_reflect(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x4;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_stationary);
}

/* t_do_axe_up -- armv7 0x00050688, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x2
 *      frame[frame].handler = t_do_stationary
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f3154 rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_axe_up(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x2;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_stationary);
}

/* t_do_lia_scream -- armv7 0x000506c4, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x1
 *      frame[frame].handler = t_do_stationary
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f3154 rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_lia_scream(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x1;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_stationary);
}

/* t_do_bike -- armv7 0x00050778, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x1
 *      frame[frame].handler = t_do_body_propell
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f31a0 rather than as a
 * link-time constant, so it lives in another translation unit. */
long t_do_body_propell(struct MK3THREAD *thread);

long t_do_bike(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x1;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_body_propell);
}

/* t_do_ind_charge -- armv7 0x000507b4, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x2
 *      frame[frame].handler = t_do_body_propell
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f31a0 rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_ind_charge(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x2;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_body_propell);
}

/* t_jax_dash_punch -- armv7 0x000507f0, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x3
 *      frame[frame].handler = t_do_body_propell
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f31a0 rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_jax_dash_punch(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x3;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_body_propell);
}

/* t_do_sz_decoy -- armv7 0x0005082c, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x4
 *      frame[frame].handler = t_do_body_propell
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f31a0 rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_sz_decoy(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x4;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_body_propell);
}

/* t_do_lia_fly -- armv7 0x00050868, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x5
 *      frame[frame].handler = t_do_body_propell
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f31a0 rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_lia_fly(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x5;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_body_propell);
}

/* t_do_lao_tele -- armv7 0x000508a4, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x6
 *      frame[frame].handler = t_do_body_propell
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f31a0 rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_lao_tele(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x6;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_body_propell);
}

/* t_do_lao_angle_kick -- armv7 0x000508e0, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x7
 *      frame[frame].handler = t_do_body_propell
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f31a0 rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_lao_angle_kick(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x7;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_body_propell);
}

/* t_do_robo_tele -- armv7 0x00050950, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x8
 *      frame[frame].handler = t_do_body_propell
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f31a0 rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_robo_tele(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x8;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_body_propell);
}

/* t_do_robo_air_grab -- armv7 0x0005098c, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x9
 *      frame[frame].handler = t_do_body_propell
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f31a0 rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_robo_air_grab(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x9;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_body_propell);
}

/* t_do_tele_explode -- armv7 0x000509c8, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0xa
 *      frame[frame].handler = t_do_body_propell
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f31a0 rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_tele_explode(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0xa;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_body_propell);
}

/* t_do_square_wave -- armv7 0x00050a04, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0xc
 *      frame[frame].handler = t_do_body_propell
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f31a0 rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_square_wave(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0xc;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_body_propell);
}

/* t_lk_bike_kick -- armv7 0x00050a40, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0xd
 *      frame[frame].handler = t_do_body_propell
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f31a0 rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_lk_bike_kick(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0xd;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_body_propell);
}

/* t_do_super_kang -- armv7 0x00050a7c, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0xe
 *      frame[frame].handler = t_do_body_propell
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f31a0 rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_super_kang(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0xe;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_body_propell);
}

/* t_do_sg_pounce -- armv7 0x00050ab8, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0xf
 *      frame[frame].handler = t_do_body_propell
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f31a0 rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_sg_pounce(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0xf;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_body_propell);
}

/* t_do_slide -- armv7 0x00050af4, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x10
 *      frame[frame].handler = t_do_body_propell
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f31a0 rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_slide(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x10;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_body_propell);
}

/* t_do_swat_zoom -- armv7 0x00050b30, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x11
 *      frame[frame].handler = t_do_body_propell
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f31a0 rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_swat_zoom(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x11;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_body_propell);
}

/* t_do_stick_sweep -- armv7 0x00050b6c, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x12
 *      frame[frame].handler = t_do_body_propell
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f31a0 rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_stick_sweep(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x12;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_body_propell);
}

/* t_do_tusk_blur -- armv7 0x00050ba8, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x13
 *      frame[frame].handler = t_do_body_propell
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f31a0 rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_tusk_blur(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x13;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_body_propell);
}

/* t_do_sg_quake -- armv7 0x00050be4, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x14
 *      frame[frame].handler = t_do_body_propell
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f31a0 rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_sg_quake(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x14;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_body_propell);
}

/* t_do_ninja_slide -- armv7 0x00050c20, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x15
 *      frame[frame].handler = t_do_body_propell
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f31a0 rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_ninja_slide(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x15;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_body_propell);
}

/* t_do_scorp_tele -- armv7 0x00050c5c, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x16
 *      frame[frame].handler = t_do_body_propell
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f31a0 rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_scorp_tele(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x16;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_body_propell);
}

/* t_do_reptile_dash -- armv7 0x00050c98, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x17
 *      frame[frame].handler = t_do_body_propell
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f31a0 rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_reptile_dash(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x17;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_body_propell);
}

/* t_do_jade_prop -- armv7 0x00050cd4, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x18
 *      frame[frame].handler = t_do_body_propell
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f31a0 rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_jade_prop(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x18;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_body_propell);
}

/* t_do_mileena_tele -- armv7 0x00050d10, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x19
 *      frame[frame].handler = t_do_body_propell
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f31a0 rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_mileena_tele(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x19;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_body_propell);
}

/* t_do_mileena_roll -- armv7 0x00050d4c, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x1a
 *      frame[frame].handler = t_do_body_propell
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f31a0 rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_mileena_roll(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x1a;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_body_propell);
}

/* t_do_ermac_tele -- armv7 0x00050d88, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x1b
 *      frame[frame].handler = t_do_body_propell
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f31a0 rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_ermac_tele(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x1b;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_body_propell);
}

/* t_do_kano_upball -- armv7 0x00050dc4, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x1c
 *      frame[frame].handler = t_do_body_propell
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f31a0 rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_kano_upball(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x1c;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_body_propell);
}

/* t_do_sonya_zap -- armv7 0x00050e3c, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x1
 *      frame[frame].handler = t_do_zap
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f314c rather than as a
 * link-time constant, so it lives in another translation unit. */
long t_do_zap(struct MK3THREAD *thread);

long t_do_sonya_zap(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x1;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_zap);
}

/* t_do_jax_zap1 -- armv7 0x00050e78, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x2
 *      frame[frame].handler = t_do_zap
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f314c rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_jax_zap1(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x2;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_zap);
}

/* t_do_jax_zap2 -- armv7 0x00050eb4, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x3
 *      frame[frame].handler = t_do_zap
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f314c rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_jax_zap2(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x3;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_zap);
}

/* t_do_ind_zap -- armv7 0x00050ef0, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x4
 *      frame[frame].handler = t_do_zap
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f314c rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_ind_zap(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x4;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_zap);
}

/* t_do_sky_ice_on -- armv7 0x00050f2c, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x5
 *      frame[frame].handler = t_do_zap
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f314c rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_sky_ice_on(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x5;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_zap);
}

/* t_do_sky_ice_behind -- armv7 0x00050f68, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x6
 *      frame[frame].handler = t_do_zap
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f314c rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_sky_ice_behind(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x6;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_zap);
}

/* t_do_sky_ice_front -- armv7 0x00050fa4, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x7
 *      frame[frame].handler = t_do_zap
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f314c rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_sky_ice_front(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x7;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_zap);
}

/* t_do_robo_zap -- armv7 0x00050fe0, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x9
 *      frame[frame].handler = t_do_zap
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f314c rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_robo_zap(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x9;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_zap);
}

/* t_do_robo_zap2 -- armv7 0x0005101c, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0xa
 *      frame[frame].handler = t_do_zap
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f314c rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_robo_zap2(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0xa;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_zap);
}

/* t_do_robo_net -- armv7 0x00051058, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0xb
 *      frame[frame].handler = t_do_zap
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f314c rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_robo_net(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0xb;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_zap);
}

/* t_do_sz_forward_zap -- armv7 0x00051094, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0xc
 *      frame[frame].handler = t_do_zap
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f314c rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_sz_forward_zap(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0xc;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_zap);
}

/* t_do_lia_anglez -- armv7 0x000510d0, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0xd
 *      frame[frame].handler = t_do_zap
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f314c rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_lia_anglez(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0xd;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_zap);
}

/* t_do_lao_zap -- armv7 0x0005110c, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0xe
 *      frame[frame].handler = t_do_zap
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f314c rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_lao_zap(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0xe;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_zap);
}

/* t_do_bomb_full -- armv7 0x00051148, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0xf
 *      frame[frame].handler = t_do_zap
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f314c rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_bomb_full(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0xf;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_zap);
}

/* t_do_bomb_mid -- armv7 0x00051184, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x10
 *      frame[frame].handler = t_do_zap
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f314c rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_bomb_mid(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x10;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_zap);
}

/* t_do_tusk_zap -- armv7 0x000511c0, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x11
 *      frame[frame].handler = t_do_zap
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f314c rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_tusk_zap(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x11;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_zap);
}

/* t_do_summon -- armv7 0x000511fc, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x12
 *      frame[frame].handler = t_do_zap
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f314c rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_summon(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x12;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_zap);
}

/* t_do_st_zap1 -- armv7 0x00051238, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x13
 *      frame[frame].handler = t_do_zap
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f314c rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_st_zap1(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x13;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_zap);
}

/* t_do_st_zap2 -- armv7 0x00051274, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x14
 *      frame[frame].handler = t_do_zap
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f314c rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_st_zap2(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x14;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_zap);
}

/* t_do_st_zap3 -- armv7 0x000512b0, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x15
 *      frame[frame].handler = t_do_zap
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f314c rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_st_zap3(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x15;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_zap);
}

/* t_lk_zap_hi -- armv7 0x000512ec, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x16
 *      frame[frame].handler = t_do_zap
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f314c rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_lk_zap_hi(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x16;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_zap);
}

/* t_lk_zap_lo -- armv7 0x00051328, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x17
 *      frame[frame].handler = t_do_zap
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f314c rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_lk_zap_lo(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x17;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_zap);
}

/* t_do_sg_zap -- armv7 0x00051364, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x18
 *      frame[frame].handler = t_do_zap
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f314c rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_sg_zap(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x18;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_zap);
}

/* t_do_swat_bomb_hi -- armv7 0x000513a0, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x19
 *      frame[frame].handler = t_do_zap
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f314c rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_swat_bomb_hi(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x19;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_zap);
}

/* t_do_swat_bomb_lo -- armv7 0x000513dc, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x1a
 *      frame[frame].handler = t_do_zap
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f314c rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_swat_bomb_lo(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x1a;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_zap);
}

/* t_do_lia_forward -- armv7 0x00051418, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x1b
 *      frame[frame].handler = t_do_zap
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f314c rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_lia_forward(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x1b;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_zap);
}

/* t_do_floor_blade -- armv7 0x00051454, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x1c
 *      frame[frame].handler = t_do_zap
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f314c rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_floor_blade(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x1c;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_zap);
}

/* t_do_smoke_spear -- armv7 0x00051490, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x1e
 *      frame[frame].handler = t_do_zap
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f314c rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_smoke_spear(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x1e;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_zap);
}

/* t_do_kitana_zap -- armv7 0x000514cc, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x20
 *      frame[frame].handler = t_do_zap
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f314c rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_kitana_zap(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x20;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_zap);
}

/* t_do_jade_zap_med -- armv7 0x00051508, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x21
 *      frame[frame].handler = t_do_zap
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f314c rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_jade_zap_med(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x21;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_zap);
}

/* t_do_reptile_orb -- armv7 0x00051544, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x22
 *      frame[frame].handler = t_do_zap
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f314c rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_reptile_orb(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x22;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_zap);
}

/* t_do_spit -- armv7 0x00051580, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x23
 *      frame[frame].handler = t_do_zap
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f314c rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_spit(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x23;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_zap);
}

/* t_do_scorpion_spear -- armv7 0x000515bc, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x24
 *      frame[frame].handler = t_do_zap
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f314c rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_scorpion_spear(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x24;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_zap);
}

/* t_do_jade_zap_hi -- armv7 0x000515f8, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x25
 *      frame[frame].handler = t_do_zap
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f314c rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_jade_zap_hi(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x25;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_zap);
}

/* t_do_jade_zap_lo -- armv7 0x00051634, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x26
 *      frame[frame].handler = t_do_zap
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f314c rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_jade_zap_lo(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x26;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_zap);
}

/* t_do_jade_zap_ret -- armv7 0x00051670, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x27
 *      frame[frame].handler = t_do_zap
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f314c rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_jade_zap_ret(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x27;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_zap);
}

/* t_do_reptile_orb_fast -- armv7 0x000516ac, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x28
 *      frame[frame].handler = t_do_zap
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f314c rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_reptile_orb_fast(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x28;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_zap);
}

/* t_do_mileena_zap -- armv7 0x000516e8, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x29
 *      frame[frame].handler = t_do_zap
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f314c rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_mileena_zap(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x29;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_zap);
}

/* t_do_osz_zap -- armv7 0x00051724, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x2a
 *      frame[frame].handler = t_do_zap
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f314c rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_osz_zap(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x2a;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_zap);
}

/* t_do_floor_ice -- armv7 0x00051760, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x2b
 *      frame[frame].handler = t_do_zap
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f314c rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_floor_ice(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x2b;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_zap);
}

/* t_do_ermac_zap -- armv7 0x0005179c, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x2c
 *      frame[frame].handler = t_do_zap
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f314c rather than as a
 * link-time constant, so it lives in another translation unit. */

long t_do_ermac_zap(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x2c;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_zap);
}

/* tusk_lp_close -- armv7 0x00052520, 4 bytes.  **Complete.**
 *
 * `bx lr`, and nothing else. The function exists so that a table can name it;
 * whatever it is asked, the answer is whatever the caller already had. */
void tusk_lp_close(MK3OBJ *obj)
{
    (void)obj;
}

/* q_smoke_animal -- armv7 0x000531dc, 20 bytes.  **Complete.**
 *
 *      obj->field30 = 160
 *      obj->field34 = 336
 *      q_animal_dist(obj)
 *
 * The second constant is formed by adding to the first, which is how the
 * compiler gets two numbers out of one `movs`. */
void q_smoke_animal(MK3OBJ *obj)
{
    obj->field30 = 160;
    obj->field34 = 336;
    q_animal_dist(obj);
}

/* q_lk_animal -- armv7 0x000531f0, 20 bytes.  **Complete.**
 *
 *      obj->field30 = 88
 *      obj->field34 = 144
 *      q_animal_dist(obj)
 *
 * The second constant is formed by adding to the first, which is how the
 * compiler gets two numbers out of one `movs`. */
void q_lk_animal(MK3OBJ *obj)
{
    obj->field30 = 88;
    obj->field34 = 144;
    q_animal_dist(obj);
}

/* q_swat_animal -- armv7 0x00053204, 20 bytes.  **Complete.**
 *
 *      obj->field30 = 88
 *      obj->field34 = 120
 *      q_animal_dist(obj)
 *
 * The second constant is formed by adding to the first, which is how the
 * compiler gets two numbers out of one `movs`. */
void q_swat_animal(MK3OBJ *obj)
{
    obj->field30 = 88;
    obj->field34 = 120;
    q_animal_dist(obj);
}

/* q_kit_animal -- armv7 0x00053218, 20 bytes.  **Complete.**
 *
 *      obj->field30 = 72
 *      obj->field34 = 112
 *      q_animal_dist(obj)
 *
 * The second constant is formed by adding to the first, which is how the
 * compiler gets two numbers out of one `movs`. */
void q_kit_animal(MK3OBJ *obj)
{
    obj->field30 = 72;
    obj->field34 = 112;
    q_animal_dist(obj);
}

/* q_half_screen_fatal -- armv7 0x000532c8, 20 bytes.  **Complete.**
 *
 *      obj->field30 = 128
 *      obj->field34 = 176
 *      q_fatal_dist(obj)
 *
 * The second constant is formed by adding to the first, which is how the
 * compiler gets two numbers out of one `movs`. */
void q_half_screen_fatal(MK3OBJ *obj)
{
    obj->field30 = 128;
    obj->field34 = 176;
    q_fatal_dist(obj);
}

/* q_close_fatal_pit -- armv7 0x000532dc, 20 bytes.  **Complete.**
 *
 *      obj->field30 = 32
 *      obj->field34 = 78
 *      q_fatal_dist(obj)
 *
 * The second constant is formed by adding to the first, which is how the
 * compiler gets two numbers out of one `movs`. */
void q_close_fatal_pit(MK3OBJ *obj)
{
    obj->field30 = 32;
    obj->field34 = 78;
    q_fatal_dist(obj);
}

/* q_close_fatal -- armv7 0x000532f0, 20 bytes.  **Complete.**
 *
 *      obj->field30 = 32
 *      obj->field34 = 80
 *      q_fatal_dist(obj)
 *
 * The second constant is formed by adding to the first, which is how the
 * compiler gets two numbers out of one `movs`. */
void q_close_fatal(MK3OBJ *obj)
{
    obj->field30 = 32;
    obj->field34 = 80;
    q_fatal_dist(obj);
}

/* q_far_fatal -- armv7 0x000533f0, 20 bytes.  **Complete.**
 *
 *      obj->field30 = 240
 *      obj->field34 = 336
 *      q_fatal_dist(obj)
 *
 * The second constant is formed by adding to the first, which is how the
 * compiler gets two numbers out of one `movs`. */
void q_far_fatal(MK3OBJ *obj)
{
    obj->field30 = 240;
    obj->field34 = 336;
    q_fatal_dist(obj);
}

/* q_skull_fatal -- armv7 0x00053404, 20 bytes.  **Complete.**
 *
 *      obj->field30 = 160
 *      obj->field34 = 224
 *      q_fatal_dist(obj)
 *
 * The second constant is formed by adding to the first, which is how the
 * compiler gets two numbers out of one `movs`. */
void q_skull_fatal(MK3OBJ *obj)
{
    obj->field30 = 160;
    obj->field34 = 224;
    q_fatal_dist(obj);
}

/* q_vomit_fatal -- armv7 0x00053418, 20 bytes.  **Complete.**
 *
 *      obj->field30 = 96
 *      obj->field34 = 128
 *      q_fatal_dist(obj)
 *
 * The second constant is formed by adding to the first, which is how the
 * compiler gets two numbers out of one `movs`. */
void q_vomit_fatal(MK3OBJ *obj)
{
    obj->field30 = 96;
    obj->field34 = 128;
    q_fatal_dist(obj);
}

/* q_grow_fatal -- armv7 0x0005342c, 20 bytes.  **Complete.**
 *
 *      obj->field30 = 208
 *      obj->field34 = 320
 *      q_fatal_dist(obj)
 *
 * The second constant is formed by adding to the first, which is how the
 * compiler gets two numbers out of one `movs`. */
void q_grow_fatal(MK3OBJ *obj)
{
    obj->field30 = 208;
    obj->field34 = 320;
    q_fatal_dist(obj);
}

/* q_lia_hair_fatal -- armv7 0x00053440, 20 bytes.  **Complete.**
 *
 *      obj->field30 = 80
 *      obj->field34 = 144
 *      q_fatal_dist(obj)
 *
 * The second constant is formed by adding to the first, which is how the
 * compiler gets two numbers out of one `movs`. */
void q_lia_hair_fatal(MK3OBJ *obj)
{
    obj->field30 = 80;
    obj->field34 = 144;
    q_fatal_dist(obj);
}

/* q_lao_hat_fatal -- armv7 0x00053454, 20 bytes.  **Complete.**
 *
 *      obj->field30 = 32
 *      obj->field34 = 112
 *      q_fatal_dist(obj)
 *
 * The second constant is formed by adding to the first, which is how the
 * compiler gets two numbers out of one `movs`. */
void q_lao_hat_fatal(MK3OBJ *obj)
{
    obj->field30 = 32;
    obj->field34 = 112;
    q_fatal_dist(obj);
}

/* q_earth_fatal -- armv7 0x00053468, 20 bytes.  **Complete.**
 *
 *      obj->field30 = 208
 *      obj->field34 = 320
 *      q_fatal_dist(obj)
 *
 * The second constant is formed by adding to the first, which is how the
 * compiler gets two numbers out of one `movs`. */
void q_earth_fatal(MK3OBJ *obj)
{
    obj->field30 = 208;
    obj->field34 = 320;
    q_fatal_dist(obj);
}

/* q_ermac_decap -- armv7 0x000534c0, 20 bytes.  **Complete.**
 *
 *      obj->field30 = 48
 *      obj->field34 = 80
 *      q_fatal_dist(obj)
 *
 * The second constant is formed by adding to the first, which is how the
 * compiler gets two numbers out of one `movs`. */
void q_ermac_decap(MK3OBJ *obj)
{
    obj->field30 = 48;
    obj->field34 = 80;
    q_fatal_dist(obj);
}

/* q_robo_flame_fatal -- armv7 0x000534e8, 20 bytes.  **Complete.**
 *
 *      obj->field30 = 192
 *      obj->field34 = 256
 *      q_fatal_dist(obj)
 *
 * The second constant is formed by adding to the first, which is how the
 * compiler gets two numbers out of one `movs`. */
void q_robo_flame_fatal(MK3OBJ *obj)
{
    obj->field30 = 192;
    obj->field34 = 256;
    q_fatal_dist(obj);
}

/* q_robo_crush_fatal -- armv7 0x000534fc, 20 bytes.  **Complete.**
 *
 *      obj->field30 = 96
 *      obj->field34 = 144
 *      q_fatal_dist(obj)
 *
 * The second constant is formed by adding to the first, which is how the
 * compiler gets two numbers out of one `movs`. */
void q_robo_crush_fatal(MK3OBJ *obj)
{
    obj->field30 = 96;
    obj->field34 = 144;
    q_fatal_dist(obj);
}

/* robo2_lk_close -- armv7 0x0005380c, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_robo2_lkc` (0x0016bf4c) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_robo2_lkc[];            /* 0x0016bf4c */

long robo2_lk_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_robo2_lkc);
}

/* robo1_up -- armv7 0x00053820, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_robo1_uc` (0x0016bd40) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_robo1_uc[];            /* 0x0016bd40 */

long robo1_up(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_robo1_uc);
}

/* robo1_down_close -- armv7 0x00053834, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_robo1_dc` (0x0016bcac) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_robo1_dc[];            /* 0x0016bcac */

long robo1_down_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_robo1_dc);
}

/* robo2_hp_close -- armv7 0x00053848, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_robo2_hpc` (0x0016be6c) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_robo2_hpc[];            /* 0x0016be6c */

long robo2_hp_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_robo2_hpc);
}

/* robo2_run_close -- armv7 0x0005385c, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_robo2_rc` (0x0016bf98) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_robo2_rc[];            /* 0x0016bf98 */

long robo2_run_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_robo2_rc);
}

/* robo_block_close -- armv7 0x00053870, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_robo_bc` (0x0016bdd8) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_robo_bc[];            /* 0x0016bdd8 */

long robo_block_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_robo_bc);
}

/* robo_hk_close -- armv7 0x00053884, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_robo_hkc` (0x0016bd8c) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_robo_hkc[];            /* 0x0016bd8c */

long robo_hk_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_robo_hkc);
}

/* robo2_down_close -- armv7 0x00053898, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_robo2_dc` (0x0016bf00) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_robo2_dc[];            /* 0x0016bf00 */

long robo2_down_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_robo2_dc);
}

/* robo2_up_close -- armv7 0x000538ac, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_robo2_uc` (0x0016bc60) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_robo2_uc[];            /* 0x0016bc60 */

long robo2_up_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_robo2_uc);
}

/* sw_hp_close -- armv7 0x000538c0, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_swat_hpc` (0x0016d198) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_swat_hpc[];            /* 0x0016d198 */

long sw_hp_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_swat_hpc);
}

/* sw_lp_close -- armv7 0x000538d4, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_swat_lpc` (0x0016d074) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_swat_lpc[];            /* 0x0016d074 */

long sw_lp_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_swat_lpc);
}

/* sw_hk_close -- armv7 0x000538e8, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_swat_hkc` (0x0016cfe0) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_swat_hkc[];            /* 0x0016cfe0 */

long sw_hk_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_swat_hkc);
}

/* sw_lk_close -- armv7 0x000538fc, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_sw_lkc` (0x0016cf48) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_sw_lkc[];            /* 0x0016cf48 */

long sw_lk_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_sw_lkc);
}

/* sw_block_close -- armv7 0x00053910, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_sw_bc` (0x0016ceb4) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_sw_bc[];            /* 0x0016ceb4 */

long sw_block_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_sw_bc);
}

/* sz_hk_close -- armv7 0x00053924, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_sz_hkc` (0x0016c5ac) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_sz_hkc[];            /* 0x0016c5ac */

long sz_hk_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_sz_hkc);
}

/* sz_up -- armv7 0x00053938, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_sz_uc` (0x0016c518) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_sz_uc[];            /* 0x0016c518 */

long sz_up(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_sz_uc);
}

/* sz_run_close -- armv7 0x0005394c, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_sz_rc` (0x0016c484) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_sz_rc[];            /* 0x0016c484 */

long sz_run_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_sz_rc);
}

/* osz_block_close -- armv7 0x00053960, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_osz_bc` (0x0016c438) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_osz_bc[];            /* 0x0016c438 */

long osz_block_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_osz_bc);
}

/* osz_lk_close -- armv7 0x00053974, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_osz_lkc` (0x0016c3a4) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_osz_lkc[];            /* 0x0016c3a4 */

long osz_lk_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_osz_lkc);
}

/* osz_lp_close -- armv7 0x00053988, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_osz_lpc` (0x0016c310) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_osz_lpc[];            /* 0x0016c310 */

long osz_lp_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_osz_lpc);
}

/* osz_hp_close -- armv7 0x0005399c, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_osz_hpc` (0x0016c27c) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_osz_hpc[];            /* 0x0016c27c */

long osz_hp_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_osz_hpc);
}

/* osm_lp_close -- armv7 0x000539c4, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_scorp_lpc` (0x0016a6b8) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_scorp_lpc[];            /* 0x0016a6b8 */

long osm_lp_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_scorp_lpc);
}

/* ind_hk_close -- armv7 0x000539d8, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_ind_hkc` (0x0016c974) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_ind_hkc[];            /* 0x0016c974 */

long ind_hk_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_ind_hkc);
}

/* ind_lk_close -- armv7 0x000539ec, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_ind_lkc` (0x0016c6d4) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_ind_lkc[];            /* 0x0016c6d4 */

long ind_lk_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_ind_lkc);
}

/* ind_lp_close -- armv7 0x00053a00, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_ind_lpc` (0x0016c720) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_ind_lpc[];            /* 0x0016c720 */

long ind_lp_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_ind_lpc);
}

/* ind_hp_open -- armv7 0x00053a14, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_ind_hpo` (0x0016c8dc) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_ind_hpo[];            /* 0x0016c8dc */

long ind_hp_open(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_ind_hpo);
}

/* ind_hp_close -- armv7 0x00053a28, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_ind_hpc` (0x0016c928) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_ind_hpc[];            /* 0x0016c928 */

long ind_hp_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_ind_hpc);
}

/* ind_down_close -- armv7 0x00053a3c, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_ind_dc` (0x0016c848) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_ind_dc[];            /* 0x0016c848 */

long ind_down_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_ind_dc);
}

/* ind_block_close -- armv7 0x00053a50, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_ind_bc` (0x0016c7b4) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_ind_bc[];            /* 0x0016c7b4 */

long ind_block_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_ind_bc);
}

/* kano_hp_open -- armv7 0x00053a64, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_kano_hpo` (0x0016bc14) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_kano_hpo[];            /* 0x0016bc14 */

long kano_hp_open(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_kano_hpo);
}

/* kano_lk_close -- armv7 0x00053a78, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_kano_lkc` (0x0016bb80) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_kano_lkc[];            /* 0x0016bb80 */

long kano_lk_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_kano_lkc);
}

/* kano_hp_close -- armv7 0x00053a8c, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_kano_hpc` (0x0016baa0) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_kano_hpc[];            /* 0x0016baa0 */

long kano_hp_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_kano_hpc);
}

/* jax_hk_close -- armv7 0x00053aa0, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_jax_hkc` (0x0016cc60) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_jax_hkc[];            /* 0x0016cc60 */

long jax_hk_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_jax_hkc);
}

/* jax_lp_close -- armv7 0x00053ab4, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_jax_lpc` (0x0016ccac) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_jax_lpc[];            /* 0x0016ccac */

long jax_lp_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_jax_lpc);
}

/* jax_hp_close -- armv7 0x00053ac8, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_jax_hpc` (0x0016cbcc) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_jax_hpc[];            /* 0x0016cbcc */

long jax_hp_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_jax_hpc);
}

/* jax_lp_open -- armv7 0x00053adc, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_jax_lpo` (0x0016ce68) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_jax_lpo[];            /* 0x0016ce68 */

long jax_lp_open(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_jax_lpo);
}

/* jax_block_open -- armv7 0x00053af0, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_jax_bo` (0x0016ce1c) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_jax_bo[];            /* 0x0016ce1c */

long jax_block_open(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_jax_bo);
}

/* jax_lk_close -- armv7 0x00053b04, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_jax_lkc` (0x0016cd40) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_jax_lkc[];            /* 0x0016cd40 */

long jax_lk_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_jax_lkc);
}

/* all_run_open -- armv7 0x00053b18, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_all_ro` (0x0016e6c0) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_all_ro[];            /* 0x0016e6c0 */

long all_run_open(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_all_ro);
}

/* lao_run_close -- armv7 0x00053b2c, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_lao_rc` (0x0016d6d4) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_lao_rc[];            /* 0x0016d6d4 */

long lao_run_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_lao_rc);
}

/* lao_hp_close -- armv7 0x00053b40, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_lao_hpc` (0x0016d5f4) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_lao_hpc[];            /* 0x0016d5f4 */

long lao_hp_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_lao_hpc);
}

/* lao_down_close -- armv7 0x00053b54, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_lao_dc` (0x0016d688) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_lao_dc[];            /* 0x0016d688 */

long lao_down_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_lao_dc);
}

/* lao_block_close -- armv7 0x00053b68, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_lao_bc` (0x0016d5a8) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_lao_bc[];            /* 0x0016d5a8 */

long lao_block_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_lao_bc);
}

/* lao_lk_close -- armv7 0x00053b7c, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_lao_lkc` (0x0016d514) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_lao_lkc[];            /* 0x0016d514 */

long lao_lk_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_lao_lkc);
}

/* lia_hp_close -- armv7 0x00053b90, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_lia_hpc` (0x0016c0c0) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_lia_hpc[];            /* 0x0016c0c0 */

long lia_hp_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_lia_hpc);
}

/* lia_lp_close -- armv7 0x00053ba4, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_lia_lpc` (0x0016c1e8) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_lia_lpc[];            /* 0x0016c1e8 */

long lia_lp_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_lia_lpc);
}

/* lia_block_close -- armv7 0x00053bb8, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_lia_bc` (0x0016c154) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_lia_bc[];            /* 0x0016c154 */

long lia_block_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_lia_bc);
}

/* lia_up_close -- armv7 0x00053bcc, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_lia_uc` (0x0016c02c) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_lia_uc[];            /* 0x0016c02c */

long lia_up_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_lia_uc);
}

/* sonya_hp_close -- armv7 0x00053be0, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_sonya_hpc` (0x0016c9c0) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_sonya_hpc[];            /* 0x0016c9c0 */

long sonya_hp_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_sonya_hpc);
}

/* sonya_lk_close -- armv7 0x00053bf4, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_sonya_lkc` (0x0016cb80) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_sonya_lkc[];            /* 0x0016cb80 */

long sonya_lk_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_sonya_lkc);
}

/* sonya_lp_open -- armv7 0x00053c08, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_sonya_lpo` (0x0016cb34) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_sonya_lpo[];            /* 0x0016cb34 */

long sonya_lp_open(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_sonya_lpo);
}

/* sonya_run_close -- armv7 0x00053c1c, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_sonya_rc` (0x0016caa0) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_sonya_rc[];            /* 0x0016caa0 */

long sonya_run_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_sonya_rc);
}

/* sonya_down_close -- armv7 0x00053c30, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_sonya_dc` (0x0016ca54) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_sonya_dc[];            /* 0x0016ca54 */

long sonya_down_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_sonya_dc);
}

/* kano_lp_open -- armv7 0x00053c44, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_kano_lpo` (0x0016bb34) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_kano_lpo[];            /* 0x0016bb34 */

long kano_lp_open(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_kano_lpo);
}

/* kano_hk_close -- armv7 0x00053c58, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_kano_hkc` (0x0016e550) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_kano_hkc[];            /* 0x0016e550 */

long kano_hk_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_kano_hkc);
}

/* kano_lp_close -- armv7 0x00053c6c, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_kano_lpc` (0x0016e62c) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_kano_lpc[];            /* 0x0016e62c */

long kano_lp_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_kano_lpc);
}

/* smoke_lp_close -- armv7 0x00053c80, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_smoke_lpc` (0x0016ba08) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_smoke_lpc[];            /* 0x0016ba08 */

long smoke_lp_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_smoke_lpc);
}

/* smoke_run_close -- armv7 0x00053c94, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_smoke_rc` (0x0016ba54) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_smoke_rc[];            /* 0x0016ba54 */

long smoke_run_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_smoke_rc);
}

/* smoke_hk_close -- armv7 0x00053ca8, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_smoke_hkc` (0x0016b928) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_smoke_hkc[];            /* 0x0016b928 */

long smoke_hk_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_smoke_hkc);
}

/* smoke_down_close -- armv7 0x00053cbc, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_smoke_dc` (0x0016b8dc) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_smoke_dc[];            /* 0x0016b8dc */

long smoke_down_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_smoke_dc);
}

/* smoke_up_close -- armv7 0x00053cd0, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_smoke_uc` (0x0016b890) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_smoke_uc[];            /* 0x0016b890 */

long smoke_up_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_smoke_uc);
}

/* smoke_lk_close -- armv7 0x00053ce4, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_smoke_lkc` (0x0016b7fc) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_smoke_lkc[];            /* 0x0016b7fc */

long smoke_lk_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_smoke_lkc);
}

/* tusk_hp_open -- armv7 0x00053cf8, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_tusk_hpo` (0x0016e2ac) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_tusk_hpo[];            /* 0x0016e2ac */

long tusk_hp_open(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_tusk_hpo);
}

/* tusk_hp_close -- armv7 0x00053d0c, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_tusk_zap` (0x0016e504) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_tusk_zap[];            /* 0x0016e504 */

long tusk_hp_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_tusk_zap);
}

/* tusk_lk_close -- armv7 0x00053d20, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_tusk_lkc` (0x0016e3d8) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_tusk_lkc[];            /* 0x0016e3d8 */

long tusk_lk_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_tusk_lkc);
}

/* tusk_run_close -- armv7 0x00053d34, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_tusk_rc` (0x0016e4b8) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_tusk_rc[];            /* 0x0016e4b8 */

long tusk_run_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_tusk_rc);
}

/* tusk_block_close -- armv7 0x00053d48, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_tusk_bc` (0x0016e46c) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_tusk_bc[];            /* 0x0016e46c */

long tusk_block_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_tusk_bc);
}

/* tusk_hk_close -- armv7 0x00053d5c, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_tusk_hkc` (0x0016e344) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_tusk_hkc[];            /* 0x0016e344 */

long tusk_hk_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_tusk_hkc);
}

/* tusk_up -- armv7 0x00053d70, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_tusk_uc` (0x0016e2f8) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_tusk_uc[];            /* 0x0016e2f8 */

long tusk_up(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_tusk_uc);
}

/* st_hp_close -- armv7 0x00053d84, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_st_hpc` (0x0016e0ac) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_st_hpc[];            /* 0x0016e0ac */

long st_hp_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_st_hpc);
}

/* st_lp_close -- armv7 0x00053d98, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_st_lpc` (0x0016dfd0) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_st_lpc[];            /* 0x0016dfd0 */

long st_lp_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_st_lpc);
}

/* st_block_close -- armv7 0x00053dac, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_st_bc` (0x0016def4) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_st_bc[];            /* 0x0016def4 */

long st_block_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_st_bc);
}

/* st_down_close -- armv7 0x00053dc0, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_st_dc` (0x0016db38) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_st_dc[];            /* 0x0016db38 */

long st_down_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_st_dc);
}

/* st_up_close -- armv7 0x00053dd4, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_st_uc` (0x0016de60) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_st_uc[];            /* 0x0016de60 */

long st_up_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_st_uc);
}

/* st_lk_close -- armv7 0x00053de8, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_st_lkc` (0x0016e1d0) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_st_lkc[];            /* 0x0016e1d0 */

long st_lk_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_st_lkc);
}

/* st_hk_close -- armv7 0x00053dfc, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_st_hkc` (0x0016dd3c) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_st_hkc[];            /* 0x0016dd3c */

long st_hk_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_st_hkc);
}

/* st_run_close -- armv7 0x00053e10, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_st_rc` (0x0016dc18) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_st_rc[];            /* 0x0016dc18 */

long st_run_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_st_rc);
}

/* st_lk_open -- armv7 0x00053e24, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_st_lko` (0x0016dbcc) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_st_lko[];            /* 0x0016dbcc */

long st_lk_open(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_st_lko);
}

/* st_lp_open -- armv7 0x00053e38, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_st_lpo` (0x0016daa4) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_st_lpo[];            /* 0x0016daa4 */

long st_lp_open(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_st_lpo);
}

/* st_hp_open -- armv7 0x00053e4c, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_st_hpo` (0x0016da58) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_st_hpo[];            /* 0x0016da58 */

long st_hp_open(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_st_hpo);
}

/* lk_lk_open -- armv7 0x00053e60, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_lk_lko` (0x0016d978) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_lk_lko[];            /* 0x0016d978 */

long lk_lk_open(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_lk_lko);
}

/* lk_hk_close -- armv7 0x00053e74, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_lk_hkc` (0x0016d898) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_lk_hkc[];            /* 0x0016d898 */

long lk_hk_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_lk_hkc);
}

/* lk_lp_close -- armv7 0x00053e88, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_lk_lpc` (0x0016d9c4) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_lk_lpc[];            /* 0x0016d9c4 */

long lk_lp_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_lk_lpc);
}

/* lk_hp_close -- armv7 0x00053e9c, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_lk_hpc` (0x0016d92c) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_lk_hpc[];            /* 0x0016d92c */

long lk_hp_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_lk_hpc);
}

/* lk_lk_close -- armv7 0x00053eb0, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_lk_lkc` (0x0016d804) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_lk_lkc[];            /* 0x0016d804 */

long lk_lk_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_lk_lkc);
}

/* lk_block_close -- armv7 0x00053ec4, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_lk_bc` (0x0016d7b8) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_lk_bc[];            /* 0x0016d7b8 */

long lk_block_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_lk_bc);
}

/* lk_up_close -- armv7 0x00053ed8, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_lk_uc` (0x0016d76c) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_lk_uc[];            /* 0x0016d76c */

long lk_up_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_lk_uc);
}

/* lk_down_close -- armv7 0x00053eec, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_lk_dc` (0x0016d720) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_lk_dc[];            /* 0x0016d720 */

long lk_down_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_lk_dc);
}

/* sg_hp_close -- armv7 0x00053f00, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_sg_hpc` (0x0016d434) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_sg_hpc[];            /* 0x0016d434 */

long sg_hp_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_sg_hpc);
}

/* sg_up -- armv7 0x00053f14, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_sg_uc` (0x0016d4c8) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_sg_uc[];            /* 0x0016d4c8 */

long sg_up(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_sg_uc);
}

/* sg_hk_close -- armv7 0x00053f28, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_sg_hkc` (0x0016d3a0) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_sg_hkc[];            /* 0x0016d3a0 */

long sg_hk_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_sg_hkc);
}

/* sg_lp_close -- armv7 0x00053f3c, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_sg_lpc` (0x0016d30c) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_sg_lpc[];            /* 0x0016d30c */

long sg_lp_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_sg_lpc);
}

/* sg_hk_open -- armv7 0x00053f50, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_sg_hko` (0x0016d2c0) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_sg_hko[];            /* 0x0016d2c0 */

long sg_hk_open(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_sg_hko);
}

/* sg_block_close -- armv7 0x00053f64, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_sg_bc` (0x0016d274) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_sg_bc[];            /* 0x0016d274 */

long sg_block_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_sg_bc);
}

/* jad_hk_close -- armv7 0x00053f78, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_jad_hkc` (0x0016b3ac) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_jad_hkc[];            /* 0x0016b3ac */

long jad_hk_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_jad_hkc);
}

/* jad_lk_close -- armv7 0x00053f8c, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_jad_lkc` (0x0016b288) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_jad_lkc[];            /* 0x0016b288 */

long jad_lk_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_jad_lkc);
}

/* jad_run_close -- armv7 0x00053fa0, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_jad_rc` (0x0016b1f4) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_jad_rc[];            /* 0x0016b1f4 */

long jad_run_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_jad_rc);
}

/* jad_lp_close -- armv7 0x00053fb4, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_jad_lpc` (0x0016b118) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_jad_lpc[];            /* 0x0016b118 */

long jad_lp_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_jad_lpc);
}

/* jad_hp_close -- armv7 0x00053fc8, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_jad_hpc` (0x0016b03c) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_jad_hpc[];            /* 0x0016b03c */

long jad_hp_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_jad_hpc);
}

/* mil_hp_open -- armv7 0x00053fdc, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_mil_hpo` (0x0016aff0) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_mil_hpo[];            /* 0x0016aff0 */

long mil_hp_open(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_mil_hpo);
}

/* mil_lp_close -- armv7 0x00053ff0, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_mil_lpc` (0x0016ae34) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_mil_lpc[];            /* 0x0016ae34 */

long mil_lp_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_mil_lpc);
}

/* mil_hp_close -- armv7 0x00054004, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_mil_hpc` (0x0016ada0) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_mil_hpc[];            /* 0x0016ada0 */

long mil_hp_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_mil_hpc);
}

/* mil_hk_close -- armv7 0x00054018, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_mil_hkc` (0x0016af5c) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_mil_hkc[];            /* 0x0016af5c */

long mil_hk_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_mil_hkc);
}

/* mil_lk_close -- armv7 0x0005402c, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_mil_lkc` (0x0016aec8) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_mil_lkc[];            /* 0x0016aec8 */

long mil_lk_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_mil_lkc);
}

/* kit_run_close -- armv7 0x00054040, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_kt_rc` (0x0016b7b0) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_kt_rc[];            /* 0x0016b7b0 */

long kit_run_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_kt_rc);
}

/* kit_lk_close -- armv7 0x00054054, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_kt_lkc` (0x0016b71c) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_kt_lkc[];            /* 0x0016b71c */

long kit_lk_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_kt_lkc);
}

/* kit_hk_close -- armv7 0x00054068, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_kt_hkc` (0x0016b688) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_kt_hkc[];            /* 0x0016b688 */

long kit_hk_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_kt_hkc);
}

/* kit_hp_close -- armv7 0x0005407c, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_kt_hpc` (0x0016b5ac) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_kt_hpc[];            /* 0x0016b5ac */

long kit_hp_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_kt_hpc);
}

/* kit_lp_close -- armv7 0x00054090, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_kt_lpc` (0x0016b4d0) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_kt_lpc[];            /* 0x0016b4d0 */

long kit_lp_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_kt_lpc);
}

/* rep_hk_close -- armv7 0x000540a4, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_rep_hkc` (0x0016aba0) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_rep_hkc[];            /* 0x0016aba0 */

long rep_hk_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_rep_hkc);
}

/* rep_lk_close -- armv7 0x000540b8, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_rep_lkc` (0x0016acc4) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_rep_lkc[];            /* 0x0016acc4 */

long rep_lk_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_rep_lkc);
}

/* rep_block_close -- armv7 0x000540cc, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_rep_bc` (0x0016aac4) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_rep_bc[];            /* 0x0016aac4 */

long rep_block_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_rep_bc);
}

/* rep_lp_close -- armv7 0x000540e0, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_rep_lpc` (0x0016a9e8) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_rep_lpc[];            /* 0x0016a9e8 */

long rep_lp_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_rep_lpc);
}

/* rep_hp_close -- armv7 0x000540f4, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_rep_hpc` (0x0016a8c4) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_rep_hpc[];            /* 0x0016a8c4 */

long rep_hp_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_rep_hpc);
}

/* scorp_run_close -- armv7 0x00054108, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_scorp_rc` (0x0016a878) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_scorp_rc[];            /* 0x0016a878 */

long scorp_run_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_scorp_rc);
}

/* scorp_lk_close -- armv7 0x0005411c, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_scorp_lkc` (0x0016a82c) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_scorp_lkc[];            /* 0x0016a82c */

long scorp_lk_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_scorp_lkc);
}

/* scorp_hk_close -- armv7 0x00054130, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_scorp_hkc` (0x0016a798) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_scorp_hkc[];            /* 0x0016a798 */

long scorp_hk_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_scorp_hkc);
}

/* scorp_block_close -- armv7 0x00054144, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_scorp_bc` (0x0016a74c) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_scorp_bc[];            /* 0x0016a74c */

long scorp_block_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_scorp_bc);
}

/* scorp_lp_close -- armv7 0x00054158, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_scorp_lpc` (0x0016a6b8) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_scorp_lpc[];            /* 0x0016a6b8 */

long scorp_lp_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_scorp_lpc);
}

/* scorp_hp_close -- armv7 0x0005416c, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_scorp_hpc` (0x0016a624) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_scorp_hpc[];            /* 0x0016a624 */

long scorp_hp_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_scorp_hpc);
}

/* ermac_block_close -- armv7 0x00054180, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_ermac_bc` (0x0016a024) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_ermac_bc[];            /* 0x0016a024 */

long ermac_block_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_ermac_bc);
}

/* ermac_lp_close -- armv7 0x00054194, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_ermac_lpc` (0x0016a150) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_ermac_lpc[];            /* 0x0016a150 */

long ermac_lp_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_ermac_lpc);
}

/* ermac_hp_close -- armv7 0x000541a8, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_ermac_hpc` (0x0016a19c) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_ermac_hpc[];            /* 0x0016a19c */

long ermac_hp_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_ermac_hpc);
}

/* ermac_lk_close -- armv7 0x000541bc, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_ermac_lkc` (0x0016a104) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_ermac_lkc[];            /* 0x0016a104 */

long ermac_lk_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_ermac_lkc);
}

/* ermac_hk_close -- armv7 0x000541d0, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_ermac_hkc` (0x0016a070) in r2. r0 and r1 are
 * untouched, so the second argument is the caller's. */
extern uint32_t sm_ermac_hkc[];            /* 0x0016a070 */

long ermac_hk_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg, sm_ermac_hkc);
}

/* sz_block_close -- armv7 0x0005482c, 12 bytes.  **Complete.**
 *
 * A tail call to `slide_check` with the arguments untouched. What the caller
 * put in r1 is slide_check's second parameter -- the object it hands to
 * restricted_xfer -- which is why this takes two and returns nothing. */
void sz_block_close(MK3OBJ *obj, MK3OBJ *other)
{
    slide_check(obj, other);
}

/* sz_lk_close -- armv7 0x00054838, 12 bytes.  **Complete.**
 *
 * A tail call to `slide_check` with the arguments untouched. What the caller
 * put in r1 is slide_check's second parameter -- the object it hands to
 * restricted_xfer -- which is why this takes two and returns nothing. */
void sz_lk_close(MK3OBJ *obj, MK3OBJ *other)
{
    slide_check(obj, other);
}



/* --------------------------------------------------------------------
 * What the readers could prove. See tools/pushfn.py, which executes
 * a body symbolically, and tools/microfn.py, which matches whole
 * bodies against templates. Both refuse anything they cannot account
 * for instruction by instruction.
 * -------------------------------------------------------------------- */


/* t_do_smoke_tele -- armv7 0x0005091c, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = t_do_robo_tele
 *      frame[frame+1].w0 = 0
 */

long t_do_smoke_tele(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_robo_tele);
}





/* --------------------------------------------------------------------
 * What the readers could prove. See tools/pushfn.py, which executes
 * a body symbolically, and tools/microfn.py, which matches whole
 * bodies against templates. Both refuse anything they cannot account
 * for instruction by instruction.
 * -------------------------------------------------------------------- */


/* t_do_lao_spin -- armv7 0x00050700, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0   (the register the guard proved)
 *      frame[frame].handler = t_do_stationary
 *      frame[frame+1].w0 = 0
 */

long t_do_lao_spin(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0;   /* the guard proved this register */

    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_stationary);
}

/* t_do_kano_roll -- armv7 0x0005073c, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0   (the register the guard proved)
 *      frame[frame].handler = t_do_body_propell
 *      frame[frame+1].w0 = 0
 */

long t_do_kano_roll(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0;   /* the guard proved this register */

    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_body_propell);
}

/* t_do_kano_zap -- armv7 0x00050e00, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0   (the register the guard proved)
 *      frame[frame].handler = t_do_zap
 *      frame[frame+1].w0 = 0
 */

long t_do_kano_zap(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0;   /* the guard proved this register */

    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_zap);
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

/* q_taser_fatal -- armv7 0x000534d4, 20 bytes.  **Complete.**
 *
 *      obj->field30 = 0x100
 *      obj->field34 = 0x130
 *      q_fatal_dist(obj)
 */
void q_taser_fatal(MK3OBJ *obj)
{
    obj->field30 = 0x100;
    obj->field34 = 0x130;
    q_fatal_dist(obj);
}


/* get_his_p_hit -- armv7 0x0005031c, 12 bytes.  **Complete.**
 *
 *      obj->field1c = obj->field00->field00->field00->p_hit
 *
 * Four loads to reach one field: this object's proc, the object that proc
 * points back at, THAT object's proc, and its 0x44. The chain goes out to the
 * opponent and back down, which is why a routine called "his" starts from the
 * object it was handed. */
void get_his_p_hit(MK3OBJ *obj)
{
    obj->field1c = obj->field00->field00->field00->p_hit;
}

/* q_four_button -- armv7 0x000502c0, 12 bytes.  **Complete.**
 *
 *      obj->field5c = (int16)obj->field00->field7c
 *
 * Read with `ldrsh`, so the gate at 0x7c is signed even though the field is
 * declared as a halfword. Answers through 0x5c like the rest of the q_ family. */
void q_four_button(MK3OBJ *obj)
{
    obj->field5c = (uint32_t)(int32_t)(int16_t)obj->field00->field7c;
}

/* q_jade_zap_ret -- armv7 0x00054c84, 12 bytes.  **Complete.**
 *
 * The whole body is a call to `is_he_joy` whose value is returned. It also
 * answers through 0x5c, because that is what is_he_joy writes -- the return
 * value and the field carry the same answer. */
long q_jade_zap_ret(MK3OBJ *obj)
{
    return is_he_joy(obj);
}

/* fatality_xfer -- armv7 0x00054b24, 20 bytes.  **Complete.**
 *
 *      *(uint16_t *)((char *)other->field00 + 0x80) = 0
 *      return mercy_xfer(obj, other)
 *
 * Clears a halfword in the OTHER fighter's proc and then hands over. 0x80 has
 * no name in the struct, so it is reached as an offset. */
long fatality_xfer(MK3OBJ *obj, MK3OBJ *other)
{
    *(uint16_t *)((char *)other->field00 + 0x80) = 0;
    return mercy_xfer(obj, other);
}

/* osm_hk_close -- armv7 0x000539b0, 20 bytes.  **Complete.**
 *
 * `secret_move_search` with `_sm_ermac_hkc` (0x0016a070) in r2 -- **plus
 * 0x48**. Every other member of this family passes the table's own address;
 * this one starts eighteen words in, and the `adds r2, #0x48` that does it is
 * the only instruction telling them apart. r0 and r1 are untouched, so the
 * second argument is the caller's. */
long osm_hk_close(MK3OBJ *obj, uint32_t arg)
{
    return secret_move_search(obj, arg,
                              (uint32_t *)((char *)sm_ermac_hkc + 0x48));
}

/* q_ermac_fatal -- armv7 0x000534ac, 20 bytes.  **Complete.**
 *
 *      obj->field30 = 0x60
 *      obj->field34 = 0xc0
 *      return q_fatal_dist(obj)
 *
 * The second constant is the first doubled in place (`adds r3, r3, r3`), so
 * the near and far bounds of the range are one instruction apart and cannot
 * drift. */
void q_ermac_fatal(MK3OBJ *obj)
{
    obj->field30 = 0x60;
    obj->field34 = 0x60 + 0x60;
    q_fatal_dist(obj);
}


/* q_six_button -- armv7 0x000502cc, 20 bytes.  **Complete.**
 *
 *      obj->field5c = ((int16)obj->field00->field7c == 0)
 *
 * `rsbs r3, r3, #1` then `movlo r3, #0` is the compiler's `x == 0`: one minus
 * the value, and zero whenever the subtraction borrowed -- which it does for
 * everything except 0 and 1. So this is the NEGATION of q_four_button, and
 * the two together say what the suffixes mean. Four wants the gate at 0x7c
 * set; six wants it clear. */
void q_six_button(MK3OBJ *obj)
{
    obj->field5c = ((int16_t)obj->field00->field7c == 0) ? 1u : 0u;
}

/* q_friend_four -- armv7 0x000502a8, 24 bytes.  **Complete.**
 *
 *      if ((int16)obj->field00->field7c == 0) q_no(obj); else q_friend(obj);
 *
 * The four-button gate in front of another question. Both arms end in a call
 * whose value is not used, so this answers only through 0x5c. */
void q_friend_four(MK3OBJ *obj)
{
    if ((int16_t)obj->field00->field7c == 0)
        q_no(obj);
    else
        q_friend(obj);
}

/* q_jade_flash_four -- armv7 0x00052bb0, 24 bytes.  **Complete.**
 *
 * The same gate in front of `q_jade_flash`. */
void q_jade_flash_four(MK3OBJ *obj)
{
    if ((int16_t)obj->field00->field7c == 0)
        q_no(obj);
    else
        q_jade_flash(obj);
}

/* q_jade_flash_six -- armv7 0x00052b98, 24 bytes.  **Complete.**
 *
 * **The same twenty-four bytes as q_jade_flash_four with `cbz` where it has
 * `cbnz`.** One instruction is the whole difference between the four-button
 * and six-button forms of a move, and it agrees with q_six_button being the
 * negation of q_four_button. */
void q_jade_flash_six(MK3OBJ *obj)
{
    if ((int16_t)obj->field00->field7c != 0)
        q_no(obj);
    else
        q_jade_flash(obj);
}

/* q_scorp_tele_four -- armv7 0x00052a5c, 24 bytes.  **Complete.**
 *
 * The same gate in front of `q_scorp_tele`. */
void q_scorp_tele_four(MK3OBJ *obj)
{
    if ((int16_t)obj->field00->field7c == 0)
        q_no(obj);
    else
        q_scorp_tele(obj);
}

/* DbgTableDump -- armv7 0x000517d8, 24 bytes.  **Complete.**
 *
 *      if ((uint32_t)(table[0] - 1) > 8) return
 *      i = 0
 *      do { i += 1 } while ((uint32_t)(table[i] - 1) <= 8)
 *
 * **A loop with nothing in it.** The routine walks the table while each word
 * is between 1 and 9 and does nothing with what it reads -- no store, no call,
 * no return value. Whatever it printed was behind a debug macro that compiled
 * to nothing, and the scan is all that survived the build. Transcribed as it
 * stands, because the walk is real code and its emptiness is the finding.
 *
 * r0 is indexed as an array of words, not dereferenced as an object, so the
 * parameter is a table pointer. */
void DbgTableDump(uint32_t *table)
{
    uint32_t i;

    if ((uint32_t)(table[0] - 1) > 8)
        return;

    i = 0;
    do {
        i += 1;
    } while ((uint32_t)(table[i] - 1) <= 8);
}


/* q_scorp_tele_six -- armv7 0x00052a44, 24 bytes.  **Complete.**
 *
 * `q_scorp_tele_four` with `cbz` where that has `cbnz` -- the second pair in
 * this file to differ by exactly one instruction. */
void q_scorp_tele_six(MK3OBJ *obj)
{
    if ((int16_t)obj->field00->field7c != 0)
        q_no(obj);
    else
        q_scorp_tele(obj);
}

/* stick_look_lr2 -- armv7 0x000541e4, 24 bytes.  **Complete.**
 *
 *      pair[0] = c        ; r3, the fourth argument
 *      pair[1] = d        ; [sp,#0x10], the fifth
 *      return stick_look_lr(obj, a, b, pair)
 *
 * **A shim that turns two arguments into an array.** `stick_look_lr` takes its
 * last argument by pointer, so this builds the two words on the stack and
 * passes their address. The fifth argument is read from `[sp, #0x10]` --
 * eight bytes of pushed registers plus the eight this routine subtracts --
 * which is where the caller's first stacked argument lands. */
long stick_look_lr2(MK3OBJ *obj, uint32_t a, uint32_t b,
                    uint32_t c, uint32_t d)
{
    uint32_t pair[2];

    pair[0] = c;
    pair[1] = d;
    return stick_look_lr(obj, a, b, pair);
}

/* close_animality_xfer -- armv7 0x00054b5c, 28 bytes.  **Complete.**
 *
 *      get_x_dist(obj)
 *      if (obj->field28 <= 0x50) animality_xfer(obj, other)
 *
 * Both arguments are kept in callee-saved registers across `get_x_dist` and
 * handed on unchanged. Too far apart and nothing happens at all -- the
 * routine has no other exit. */
void close_animality_xfer(MK3OBJ *obj, MK3OBJ *other)
{
    get_x_dist(obj);
    if ((long)obj->field28 <= 0x50)
        animality_xfer(obj, other);
}

/* q_both_punches -- armv7 0x00052858, 28 bytes.  **Complete.**
 *
 *      obj->field1c = 0x00010010
 *      obj->field20 = 0x00101000
 *      return button_bit_check(obj)
 *
 * Two packed words in the argument slots and one call. Each is a pair of
 * halves -- (0x0001, 0x0010) and (0x0010, 0x1000) -- and what the halves
 * select is button_bit_check's business, not this routine's. */
void q_both_punches(MK3OBJ *obj)
{
    obj->field1c = 0x00010010u;
    obj->field20 = 0x00101000u;
    button_bit_check(obj);
}

/* q_lp_block_lk -- armv7 0x0005283c, 28 bytes.  **Complete.**
 *
 * The same twenty-eight bytes as q_both_punches with two different constants:
 * (0x0003, 0x0020) and (0x0030, 0x2000). */
void q_lp_block_lk(MK3OBJ *obj)
{
    obj->field1c = 0x00030020u;
    obj->field20 = 0x00302000u;
    button_bit_check(obj);
}


/* qorb3 -- armv7 0x00052874, 28 bytes.  **Complete.**
 *
 *      obj->field1c = ((MK3OBJ *)obj->field00->him)->field24
 *      if (obj->field1c == 0x19) q_no(obj); else q_both_punches(obj);
 *
 * The character number is read out of the opponent and compared against 0x19;
 * that one character cannot be answered and everything else falls through to
 * the button test. The value is stored into 0x1c before the branch, so it is
 * live for whichever call follows -- and q_both_punches overwrites it with its
 * own masks straight away, which is what makes the store a scratch use rather
 * than an argument.
 *
 * Both arms end in a call whose value this routine does not compute, so it
 * answers only through 0x5c. */
void qorb3(MK3OBJ *obj)
{
    obj->field1c = ((MK3OBJ *)(uintptr_t)obj->field00->him)->field24;
    if (obj->field1c == 0x19)
        q_no(obj);
    else
        q_both_punches(obj);
}

/* q_reptile_orb_slow -- armv7 0x00052890, 28 bytes.  **Complete.**
 *
 *      obj->field1c = &G + 0x438
 *      qorb3(obj)
 *
 * **The address it stores is dead.** `qorb3` overwrites 0x1c with the
 * opponent's character number on its first three instructions, before anything
 * reads it. The store is in the binary and nothing between the two touches the
 * field, so it is transcribed as it stands. */
void q_reptile_orb_slow(MK3OBJ *obj)
{
    obj->field1c = (uint32_t)(uintptr_t)(G_BYTES + 0x438);
    qorb3(obj);
}

/* airborn_xfer -- armv7 0x0005442c, 32 bytes.  **Complete.**
 *
 *      *(uint16_t *)((char *)other->field00 + 0x80) = 0
 *      if ((other->field00->field10 & 0x1c) == 0) free_xfer(obj, other)
 *
 * The same halfword `fatality_xfer` clears, cleared here too, and then three
 * bits of the proc's 0x10 decide whether the transfer happens at all. The
 * clear is unconditional; only the hand-over is gated. */
void airborn_xfer(MK3OBJ *obj, MK3OBJ *other)
{
    *(uint16_t *)((char *)other->field00 + 0x80) = 0;
    if ((other->field00->field10 & 0x1cu) == 0)
        free_xfer(obj, other);
}

/* q_animal_req -- armv7 0x00050230, 32 bytes.  **Complete.**
 *
 *      q_mercy(obj)
 *      if (obj->field5c == 0) q_no(obj); else q_fatality_req(obj);
 *
 * A question asked in front of another question: mercy first, and only if that
 * answered yes is the fatality requirement asked. Both answer through 0x5c, so
 * the second overwrites the first. */
void q_animal_req(MK3OBJ *obj)
{
    q_mercy(obj);
    if (obj->field5c == 0)
        q_no(obj);
    else
        q_fatality_req(obj);
}

/* q_close_animal -- armv7 0x0005339c, 32 bytes.  **Complete.**
 *
 * The same thirty-two bytes as q_animal_req with `q_close_fatal` in place of
 * `q_fatality_req`. */
void q_close_animal(MK3OBJ *obj)
{
    q_mercy(obj);
    if (obj->field5c == 0)
        q_no(obj);
    else
        q_close_fatal(obj);
}


/* q_close_friend -- armv7 0x00053158, 32 bytes.  **Complete.**
 *
 *      get_x_dist(obj)
 *      if (obj->field28 > 0x70) q_no(obj); else q_friend(obj);
 *
 * The near end of a family of three. See q_ind_friend for the rest. */
void q_close_friend(MK3OBJ *obj)
{
    get_x_dist(obj);
    if ((long)obj->field28 > 0x70)
        q_no(obj);
    else
        q_friend(obj);
}

/* q_ind_friend -- armv7 0x000530f8, 32 bytes.  **Complete.**
 *
 *      get_x_dist(obj)
 *      if (obj->field28 <= 0x9f) q_no(obj); else q_friend(obj);
 *
 * **Three routines, one shape, three thresholds -- and not all the same way
 * round.** All of them measure the horizontal distance and hand off to
 * q_friend, but q_close_friend answers yes when the gap is 0x70 or LESS while
 * this one and q_dinger_friend answer yes when it is MORE, at 0x9f and 0xbf.
 * The sense is carried by `ble` against `bgt`, one instruction, exactly as the
 * four/six button pairs are. */
void q_ind_friend(MK3OBJ *obj)
{
    get_x_dist(obj);
    if ((long)obj->field28 <= 0x9f)
        q_no(obj);
    else
        q_friend(obj);
}

/* q_dinger_friend -- armv7 0x00053138, 32 bytes.  **Complete.**
 *
 * The far end of the same family: yes above 0xbf. */
void q_dinger_friend(MK3OBJ *obj)
{
    get_x_dist(obj);
    if ((long)obj->field28 <= 0xbf)
        q_no(obj);
    else
        q_friend(obj);
}

/* q_fan_lift -- armv7 0x00050328, 32 bytes.  **Complete.**
 *
 *      get_his_p_hit(obj)
 *      if ((long)obj->field1c > 0) q_no(obj); else q_yes(obj);
 *
 * **The consumer of get_his_p_hit, and it settles where that routine leaves
 * its answer.** get_his_p_hit walks four pointers to reach the opponent's
 * proc and stores 0x44 into 0x1c; this reads 0x1c straight back. The lift is
 * allowed only while his hit count is zero or below -- one hit and the answer
 * turns to no. */
void q_fan_lift(MK3OBJ *obj)
{
    get_his_p_hit(obj);
    if ((long)obj->field1c > 0)
        q_no(obj);
    else
        q_yes(obj);
}


/* q_smoke_friend -- armv7 0x00053118, 32 bytes.  **Complete.**
 *
 * A fourth distance gate in front of q_friend, at 0xcf and the far way round.
 * With q_close_friend at 0x70, q_ind_friend at 0x9f and q_dinger_friend at
 * 0xbf, the four thresholds are all different and only the first is a near
 * test. */
void q_smoke_friend(MK3OBJ *obj)
{
    get_x_dist(obj);
    if ((long)obj->field28 <= 0xcf)
        q_no(obj);
    else
        q_friend(obj);
}

/* q_sonya_friend -- armv7 0x0005367c, 32 bytes.  **Complete.**
 *
 *      is_stick_down(obj)
 *      if (obj->field5c == 0) q_no(obj); else q_friend(obj);
 *
 * **Not every friendship gate is a distance.** The same thirty-two bytes as
 * the four that measure a gap, with a stick reading where they have
 * get_x_dist -- and the answer comes back in 0x5c instead of 0x28, so the
 * comparison disappears entirely. */
void q_sonya_friend(MK3OBJ *obj)
{
    is_stick_down(obj);
    if (obj->field5c == 0)
        q_no(obj);
    else
        q_friend(obj);
}

/* q_slide -- armv7 0x00053510, 32 bytes.  **Complete.**
 *
 *      is_stick_away(obj)
 *      if (obj->field5c == 0) q_no(obj); else q_lp_block_lk(obj);
 *
 * The stick has to be held away before the button pattern is even looked at. */
void q_slide(MK3OBJ *obj)
{
    is_stick_away(obj);
    if (obj->field5c == 0)
        q_no(obj);
    else
        q_lp_block_lk(obj);
}

/* q_mercy -- armv7 0x0005020c, 36 bytes.  **Complete.**
 *
 *      obj->field30 = (int16)*(uint16_t *)(G + 0x45a)
 *      if (*(uint16_t *)(G + 0x45a) == 0) q_no(obj); else q_yes(obj);
 *
 * **Stored signed, tested unsigned.** The halfword is loaded with `ldrh`, kept
 * for the test, and sign-extended with `sxth` only for the store -- so 0x30
 * can come out negative while the yes/no answer never can. Nothing is lost
 * either way, because both readings agree about zero. */
void q_mercy(MK3OBJ *obj)
{
    uint16_t v = *(uint16_t *)(G_BYTES + 0x45a);

    obj->field30 = (uint32_t)(int32_t)(int16_t)v;
    if (v == 0)
        q_no(obj);
    else
        q_yes(obj);
}

/* q_fatality_req -- armv7 0x000501e8, 36 bytes.  **Complete.**
 *
 *      obj->field2c = (int16)*(int16_t *)(G + 0x45c)
 *      if (obj->field2c == 3) q_yes(obj); else q_no(obj);
 *
 * The neighbouring halfword to q_mercy's, two bytes along, and this one is
 * loaded signed. Exactly three answers yes -- not "at least three".
 *
 * It is also **a writer of 0x2c**, which `tl_jax_dash_punch` reads without
 * ever writing. That does not prove the two are the same use of the field, and
 * nothing here settles it; it is noted so the next reader has somewhere to
 * start.
 *
 * G + 0x456, 0x45a and 0x45c are three halfwords in a row: the sans_repell
 * slot, the mercy flag and this. */
void q_fatality_req(MK3OBJ *obj)
{
    obj->field2c = (uint32_t)(int32_t)*(int16_t *)(G_BYTES + 0x45c);
    if (obj->field2c == 3)
        q_yes(obj);
    else
        q_no(obj);
}


/* q_kano_swipe -- armv7 0x00052fbc, 36 bytes.  **Complete.**
 *
 *      get_his_action(obj)
 *      if (obj->field20 == 0x60c) q_no(obj); else q_yes(obj);
 *
 * **The equal case is the NO.** These read as "is he not doing that", which
 * is why the yes is the fall-through and the no is the branch target.
 * q_lao_spin and q_pounce_ok_now are the same thirty-six bytes with 0x62b and
 * 0x503 -- three routines whose whole difference is one `movw`. */
void q_kano_swipe(MK3OBJ *obj)
{
    get_his_action(obj);
    if (obj->field20 == 0x60c)
        q_no(obj);
    else
        q_yes(obj);
}

/* q_lao_spin -- armv7 0x00052f98, 36 bytes.  **Complete.**  See q_kano_swipe. */
void q_lao_spin(MK3OBJ *obj)
{
    get_his_action(obj);
    if (obj->field20 == 0x62b)
        q_no(obj);
    else
        q_yes(obj);
}

/* q_pounce_ok_now -- armv7 0x00053048, 36 bytes.  **Complete.**  See
 * q_kano_swipe. */
void q_pounce_ok_now(MK3OBJ *obj)
{
    get_his_action(obj);
    if (obj->field20 == 0x503)
        q_no(obj);
    else
        q_yes(obj);
}

/* animality_xfer -- armv7 0x00054b38, 36 bytes.  **Complete.**
 *
 *      *(uint16_t *)((char *)other->field00 + 0x80) = 0
 *      q_mercy(obj)
 *      if (obj->field5c != 0) fatality_xfer(obj, other)
 *
 * **Five routines in this file clear the same halfword.** fatality_xfer,
 * animality_xfer, close_animality_xfer (through animality_xfer), airborn_xfer
 * and restricted_xfer all write zero to `other->field00 + 0x80` before doing
 * anything else, and the two that chain do it twice. The clear is
 * unconditional in every one of them; only what follows is gated.
 *
 * Both arguments are held in callee-saved registers across q_mercy and handed
 * on unchanged. */
void animality_xfer(MK3OBJ *obj, MK3OBJ *other)
{
    *(uint16_t *)((char *)other->field00 + 0x80) = 0;
    q_mercy(obj);
    if (obj->field5c != 0)
        fatality_xfer(obj, other);
}

/* restricted_xfer -- armv7 0x00054678, 36 bytes.  **Complete.**
 *
 *      *(uint16_t *)((char *)other->field00 + 0x80) = 0
 *      am_i_airborn(obj)
 *      if (obj->field5c == 0) airborn_xfer(obj, other)
 *
 * **The name and the test point opposite ways.** The transfer to
 * `airborn_xfer` happens when am_i_airborn comes back CLEAR -- on the ground,
 * not in the air. `cbnz` skips the call, so the reading is not in doubt; what
 * the two names mean together is left as it stands.
 *
 * The halfword at 0x80 is cleared here and then cleared again by
 * airborn_xfer, which is harmless and is what the two routines separately do. */
void restricted_xfer(MK3OBJ *obj, MK3OBJ *other)
{
    *(uint16_t *)((char *)other->field00 + 0x80) = 0;
    am_i_airborn(obj);
    if (obj->field5c == 0)
        airborn_xfer(obj, other);
}


/* q_fatal_dist -- armv7 0x0005322c, 40 bytes.  **Complete.**
 *
 *      get_x_dist(obj)
 *      d = obj->field28
 *      if (d < obj->field30 || d > obj->field34) q_no(obj)
 *      else q_fatality_req(obj)
 *
 * **The band comes in through 0x30 and 0x34 and the caller sets it.**
 * q_ermac_fatal writes 0x60 and 0xc0 into those two fields and then calls
 * this; q_inflate_fatal writes 0x60 and 0xe0. Reading the two sides together
 * settles what the pair is for: a near bound and a far one, inclusive at both
 * ends, tested with `blt` then `ble`. */
void q_fatal_dist(MK3OBJ *obj)
{
    long d;

    get_x_dist(obj);
    d = (long)obj->field28;
    if (d < (long)obj->field30 || d > (long)obj->field34)
        q_no(obj);
    else
        q_fatality_req(obj);
}

/* q_animal_dist -- armv7 0x00053178, 40 bytes.  **Complete.**
 *
 * The same forty bytes as q_fatal_dist with `q_animal_req` in place of
 * `q_fatality_req`. */
void q_animal_dist(MK3OBJ *obj)
{
    long d;

    get_x_dist(obj);
    d = (long)obj->field28;
    if (d < (long)obj->field30 || d > (long)obj->field34)
        q_no(obj);
    else
        q_animal_req(obj);
}

/* q_inflate_fatal -- armv7 0x00054bfc, 40 bytes.  **Complete.**
 *
 *      q_is_he_cornered(obj)
 *      if (obj->field5c != 0) q_no(obj)
 *      else { obj->field30 = 0x60; obj->field34 = 0xe0; q_fatal_dist(obj) }
 *
 * A cornered opponent is refused outright; otherwise the band is built the
 * same way q_ermac_fatal builds its own -- a constant and then an add on the
 * register that already holds it, so the two bounds cannot drift apart. The
 * far bound is 0x80 above the near one here and 0x60 above it there. */
void q_inflate_fatal(MK3OBJ *obj)
{
    q_is_he_cornered(obj);
    if (obj->field5c != 0) {
        q_no(obj);
        return;
    }
    obj->field30 = 0x60;
    obj->field34 = 0x60 + 0x80;
    q_fatal_dist(obj);
}

/* sonya_block_close -- armv7 0x000549ac, 36 bytes.  **Complete.**
 *
 *      check_sonya_legs(obj)
 *      if (obj->field5c != 0) {
 *          obj->field38 = t_do_leg_throw
 *          restricted_xfer(obj, other)
 *      }
 *
 * The handler goes into 0x38 one instruction before the transfer, which is the
 * slot that family reads -- the same order tl_do_swat_zoom uses to hand a
 * thread over. Nothing happens at all when the check comes back clear. */
void sonya_block_close(MK3OBJ *obj, MK3OBJ *other)
{
    check_sonya_legs(obj);
    if (obj->field5c != 0) {
        obj->field38 = (uint32_t)(uintptr_t)t_do_leg_throw;
        restricted_xfer(obj, other);
    }
}

/* sz_lp_close -- armv7 0x00054808, 36 bytes.  **Complete.**
 *
 *      slide_check(obj)
 *      if (obj->field5c == 0) secret_move_search(obj, arg, sm_sz_lpc)
 *
 * A member of the osm_/ind_ table family with a gate in front of it, and the
 * gate is the CLEAR case: a slide already in progress stops the search.
 * slide_check answers through 0x5c and sets it to 1 only when it started one.
 *
 * The second argument is an OBJECT: slide_check hands it to restricted_xfer as
 * one. secret_move_search is the call that wants the same value as a word. The
 * table address is passed as it stands, without the 0x48 that makes
 * osm_hk_close different from its own siblings. */
void sz_lp_close(MK3OBJ *obj, MK3OBJ *other)
{
    slide_check(obj, other);
    if (obj->field5c == 0)
        secret_move_search(obj, (uint32_t)(uintptr_t)other, sm_sz_lpc);
}


/* q_friend -- armv7 0x0005027c, 44 bytes.  **Complete.**
 *
 *      i = obj->field00->field08                     ; the strength index
 *      obj->field1c = *(uint16_t *)(G + 0x3b0 + i * 2)
 *      if (obj->field1c == 0) q_fatality_req(obj); else q_no(obj);
 *
 * **A halfword table indexed by the strength index.** The index is shifted
 * left one, not two, so the entries are sixteen bits; the base is G + 0x3b0.
 * A ZERO entry is the permissive one -- it passes the question on -- and any
 * other value refuses outright.
 *
 * This is what the whole friendship family funnels into: q_close_friend,
 * q_ind_friend, q_dinger_friend and q_smoke_friend all gate on distance and
 * then land here, and q_sonya_friend gates on the stick instead. */
void q_friend(MK3OBJ *obj)
{
    obj->field1c = *(uint16_t *)(G_BYTES + 0x3b0
                                 + obj->field00->field08 * 2);
    if (obj->field1c == 0)
        q_fatality_req(obj);
    else
        q_no(obj);
}

/* q_friend_ez -- armv7 0x00050250, 44 bytes.  **Complete.**
 *
 * The same forty-four bytes as q_friend reading the same table, answering yes
 * directly instead of passing the question to q_fatality_req. The easy form
 * skips the requirement, which is the whole difference between them. */
void q_friend_ez(MK3OBJ *obj)
{
    obj->field1c = *(uint16_t *)(G_BYTES + 0x3b0
                                 + obj->field00->field08 * 2);
    if (obj->field1c == 0)
        q_yes(obj);
    else
        q_no(obj);
}

/* q_lia_scream -- armv7 0x0005306c, 44 bytes.  **Complete.**
 *
 *      get_his_p_hit(obj)
 *      if (obj->field1c > 2) q_no(obj)
 *      else {
 *          q_is_he_a_boss(obj)
 *          if (obj->field5c != 0) q_no(obj); else q_yes(obj);
 *      }
 *
 * Two conditions, both of which have to hold: he must have been hit no more
 * than twice, and he must not be a boss. The two tests answer through
 * different fields -- 0x1c for the count, 0x5c for the boss -- and the second
 * is only asked when the first passes. */
void q_lia_scream(MK3OBJ *obj)
{
    get_his_p_hit(obj);
    if ((long)obj->field1c > 2)
        goto no;
    q_is_he_a_boss(obj);
    if (obj->field5c != 0)
        goto no;
    q_yes(obj);
    return;

no:
    q_no(obj);
}

/* q_lk_friend -- armv7 0x00052810, 44 bytes.  **Complete.**
 *
 *      obj->field1c = 0x00040000
 *      obj->field20 = 0x00400000
 *      button_bit_check(obj)
 *      if (obj->field5c == 0) q_no(obj); else q_friend(obj);
 *
 * The second mask is the first plus 0x3c0000 -- one literal and an add on the
 * register already holding it, the same way the velocity triples are built.
 * Both halves are (0x0004, 0x0000) and (0x0040, 0x0000), so unlike
 * q_both_punches and q_lp_block_lk the low halves are empty here. */
void q_lk_friend(MK3OBJ *obj)
{
    obj->field1c = 0x40000u;
    obj->field20 = 0x40000u + 0x3c0000u;
    button_bit_check(obj);
    if (obj->field5c == 0)
        q_no(obj);
    else
        q_friend(obj);
}


/* q_reptile_orb_fast_four -- armv7 0x000528ac, 44 bytes.  **Complete.**
 *
 *      if ((int16)obj->field00->field7c == 0) q_no(obj)
 *      else { obj->field1c = &G + 0x43c; qorb3(obj); }
 *
 * **A third caller writing a dead address into 0x1c.** q_reptile_orb_slow
 * stores &G + 0x438 and both fast forms store &G + 0x43c, and qorb3 overwrites
 * 0x1c with the opponent's character number before any of them is read. The
 * two addresses are adjacent words and they differ between the slow and fast
 * forms, which is worth knowing even though nothing here consumes them.
 *
 * The address is built with two adds, 0x430 then 0xc, rather than one. */
void q_reptile_orb_fast_four(MK3OBJ *obj)
{
    if ((int16_t)obj->field00->field7c == 0) {
        q_no(obj);
        return;
    }
    obj->field1c = (uint32_t)(uintptr_t)(G_BYTES + 0x430 + 0xc);
    qorb3(obj);
}

/* q_reptile_orb_fast_six -- armv7 0x000528d8, 44 bytes.  **Complete.**
 *
 * The same forty-four bytes with cbz where the four form has cbnz -- the
 * fourth pair in this file separated by exactly one instruction. */
void q_reptile_orb_fast_six(MK3OBJ *obj)
{
    if ((int16_t)obj->field00->field7c != 0) {
        q_no(obj);
        return;
    }
    obj->field1c = (uint32_t)(uintptr_t)(G_BYTES + 0x430 + 0xc);
    qorb3(obj);
}

/* q_simple_shang -- armv7 0x000501bc, 44 bytes.  **Complete.**
 *
 *      c = ((MK3OBJ *)obj->field00->him)->field24
 *      if (c <= 0x17 && c != 0xc) q_yes(obj); else q_no(obj);
 *
 * **Two conditions with no branch between them.** The compiler built the
 * whole test out of flags: a subtract that leaves 1 unless the character is
 * 0xc, then an if-then-else pair that zeroes the answer outright above 0x17
 * and otherwise keeps that bit. One branch at the end, for the result.
 *
 * The range test is signed, and the excluded character sits inside the range
 * rather than at either end. */
void q_simple_shang(MK3OBJ *obj)
{
    long c = (long)((MK3OBJ *)(uintptr_t)obj->field00->him)->field24;

    if (c <= 0x17 && c != 0xc)
        q_yes(obj);
    else
        q_no(obj);
}


/* q_bubble_fatal -- armv7 0x0005347c, 48 bytes.  **Complete.**
 *
 *      obj->field1c = 0x00040020
 *      obj->field20 = 0x00402000
 *      button_bit_check(obj)
 *      if (obj->field5c == 0) q_no(obj); else q_earth_fatal(obj);
 *
 * **The mask pairs in this file have a shape, and four of them agree on it.**
 * Each word is two 16-bit halves, and the second word is the first with the
 * high half shifted left four and the low half shifted left eight:
 *
 *      q_both_punches   (0x0001, 0x0010) -> (0x0010, 0x1000)
 *      q_lp_block_lk    (0x0003, 0x0020) -> (0x0030, 0x2000)
 *      q_bubble_fatal   (0x0004, 0x0020) -> (0x0040, 0x2000)
 *      buttons_in_a2    (0x0007, 0x0070) -> (0x0070, 0x7000)
 *
 * That is measured from the four pairs, not inferred from one. What the halves
 * select is still button_bit_check's business. */
void q_bubble_fatal(MK3OBJ *obj)
{
    obj->field1c = 0x00040020u;
    obj->field20 = 0x00402000u;
    button_bit_check(obj);
    if (obj->field5c == 0)
        q_no(obj);
    else
        q_earth_fatal(obj);
}

/* q_lk_mk_fatal -- armv7 0x000527e0, 48 bytes.  **Complete.**
 *
 * **The same forty-eight bytes as q_bubble_fatal with the same two masks**,
 * differing only in the routine it hands the yes to: q_fatality_req instead of
 * q_earth_fatal. Two moves share a button pattern and part company on what
 * they then require. */
void q_lk_mk_fatal(MK3OBJ *obj)
{
    obj->field1c = 0x00040020u;
    obj->field20 = 0x00402000u;
    button_bit_check(obj);
    if (obj->field5c == 0)
        q_no(obj);
    else
        q_fatality_req(obj);
}

/* buttons_in_a2 -- armv7 0x00052524, 48 bytes.  **Complete.**
 *
 *      obj->field24 = *(uint32_t *)(G + 0x1c)
 *      obj->field28 = 0x00070070
 *      if (obj->field00->field08 != 0) obj->field28 = 0x00707000
 *      obj->field24 &= obj->field28
 *
 * A leaf with no frame at all -- it never pushes. The live button word comes
 * out of G + 0x1c and is masked by one of two patterns chosen on the strength
 * index: the low set when it is zero, the shifted set otherwise. The two masks
 * are the same pair relationship the q_ family uses, so which player is asking
 * decides which half of the button space is looked at. */
void buttons_in_a2(MK3OBJ *obj)
{
    obj->field24 = *(uint32_t *)(G_BYTES + 0x1c);
    obj->field28 = 0x00070070u;
    if (obj->field00->field08 != 0)
        obj->field28 = 0x00707000u;
    obj->field24 = obj->field24 & obj->field28;
}


/* illegal_button_check -- armv7 0x0005277c, 48 bytes.  **Complete.**
 *
 *      buttons_in_a2(obj)                      ; fills obj->field24
 *      obj->field1c = obj->field00->field08 ? second : first
 *      obj->field1c = ~four_button_bits(obj)
 *      obj->field24 &= obj->field1c
 *      return obj->field24
 *
 * **Which of the two arguments is used depends on the strength index**, the
 * same field buttons_in_a2 uses to pick its mask one call earlier -- so the
 * player asking selects twice, once for the button word and once for the
 * pattern tested against it.
 *
 * The result of four_button_bits is inverted before it is stored and before it
 * is ANDed, so this REMOVES the bits that routine found rather than keeping
 * them: the name is about what is not allowed. The masked word goes back into
 * 0x24 and is also the return value -- a value this routine really computes,
 * with the AND that produces it. */
long illegal_button_check(MK3OBJ *obj, uint32_t first, uint32_t second)
{
    buttons_in_a2(obj);
    obj->field1c = obj->field00->field08 ? second : first;
    obj->field1c = (uint32_t)~four_button_bits(obj, obj->field1c);
    obj->field24 = obj->field24 & obj->field1c;
    return (long)obj->field24;
}

/* DoSpecial -- armv7 0x000524f0, 48 bytes.  **Complete.**
 *
 *      if (obj->field00->field10 & 0x10) return
 *      if (*(int16_t *)((char *)obj->field00 + 0x80) != 0) return
 *      DoASpecial(obj, *(uint32_t *)(G + 8 + obj->field00->field08 * 4))
 *
 * **The halfword at proc+0x80 gates every special move, and five routines in
 * this file clear it.** fatality_xfer, animality_xfer, airborn_xfer and
 * restricted_xfer all write zero there before handing a thread over; this is
 * what that clear is FOR. While it holds anything, DoSpecial returns without
 * looking anything up.
 *
 * Bit 4 of the proc's 0x10 is the other gate, tested first and read from the
 * same load. The table is words at G + 8 indexed by the strength index --
 * shifted left two, where q_friend's halfword table is shifted left one. */
void DoSpecial(MK3OBJ *obj)
{
    if ((obj->field00->field10 & 0x10u) != 0)
        return;
    if (*(int16_t *)((char *)obj->field00 + 0x80) != 0)
        return;

    DoASpecial(obj, *(uint32_t *)(G_BYTES + 8
                                  + obj->field00->field08 * 4));
}


/* button_bit_check -- armv7 0x000527ac, 52 bytes.  **Complete.**
 *
 *      if (obj->field00->field08 == 1) obj->field1c = obj->field20
 *      obj->field1c = four_button_bits(obj, obj->field1c)
 *      buttons_in_a2(obj)
 *      obj->field5c = (obj->field24 == obj->field1c)
 *
 * **This is what the whole q_ family funnels into, and it explains the mask
 * pairs.** The two words a caller puts in 0x1c and 0x20 are the same pattern
 * for the two players: index 1 swaps the second in over the first, and
 * buttons_in_a2 -- called one instruction later -- masks the live button word
 * with its own per-player window. That is why the four measured pairs are the
 * same halves at different shifts. One pattern, two players, two offsets.
 *
 * The answer is EQUALITY, not an overlap: every wanted bit present and no
 * other. `four_button_bits` takes the chosen mask in r1 and reads it there
 * (tst.w r1, #0x10000), which is how the two-argument shape was settled --
 * two call sites load r1 and the callee uses it. */
void button_bit_check(MK3OBJ *obj)
{
    if (obj->field00->field08 == 1)
        obj->field1c = obj->field20;

    obj->field1c = (uint32_t)four_button_bits(obj, obj->field1c);
    buttons_in_a2(obj);
    obj->field5c = (obj->field24 == obj->field1c) ? 1u : 0u;
}

/* q_scream_fatal -- armv7 0x0005336c, 48 bytes.  **Complete.**
 *
 * A third routine on the (0x0004, 0x0020) / (0x0040, 0x2000) pair, after
 * q_bubble_fatal and q_lk_mk_fatal. All three are the same forty-eight bytes
 * and part company only in which routine gets the yes -- q_earth_fatal,
 * q_fatality_req and q_close_fatal. */
void q_scream_fatal(MK3OBJ *obj)
{
    obj->field1c = 0x00040020u;
    obj->field20 = 0x00402000u;
    button_bit_check(obj);
    if (obj->field5c == 0)
        q_no(obj);
    else
        q_close_fatal(obj);
}

/* q_smoke_tele -- armv7 0x00052eb0, 48 bytes.  **Complete.**
 *
 *      get_his_p_hit(obj)
 *      if (obj->field1c > 1) q_no(obj)
 *      else {
 *          get_his_action(obj)
 *          if (obj->field20 == 0x616) q_no(obj); else q_yes(obj);
 *      }
 *
 * Two conditions again, and the two refusals share one instruction: the action
 * test branches back to the same q_no the count test falls into. */
void q_smoke_tele(MK3OBJ *obj)
{
    get_his_p_hit(obj);
    if ((long)obj->field1c > 1)
        goto no;
    get_his_action(obj);
    if (obj->field20 == 0x616)
        goto no;
    q_yes(obj);
    return;

no:
    q_no(obj);
}


/* q_jax_zap -- armv7 0x00052af0, 52 bytes.  **Complete.**
 *
 *      obj->field1c = &G + 0x410
 *      get_tsl_px(obj, obj)
 *      if (obj->field20 > 0x7f) q_yes(obj); else q_no(obj);
 *
 * **A third family in this file, and this one really does use 0x1c.** The
 * address goes in as an argument, get_tsl_px is handed the object twice --
 * once as itself and once as the reference -- and the answer comes back in
 * 0x20 to be compared against a threshold. Unlike the qorb3 callers, nothing
 * overwrites the address before it is read.
 *
 * q_jade_flash and q_kabal_animal are the same fifty-two bytes with different
 * addresses and different thresholds:
 *
 *      q_jax_zap        &G + 0x410    > 0x7f
 *      q_jade_flash     &G + 0x41c    > 0x7f
 *      q_kabal_animal   &G + 0x3a8    > 0x3f
 *
 * The jade_flash address is built with two adds, 0x410 then 0xc, the same way
 * the reptile orb pair builds its own. */
void q_jax_zap(MK3OBJ *obj)
{
    obj->field1c = (uint32_t)(uintptr_t)(G_BYTES + 0x410);
    get_tsl_px(obj, obj);
    if ((long)obj->field20 > 0x7f)
        q_yes(obj);
    else
        q_no(obj);
}

/* q_jade_flash -- armv7 0x00052b64, 52 bytes.  **Complete.**  See q_jax_zap. */
void q_jade_flash(MK3OBJ *obj)
{
    obj->field1c = (uint32_t)(uintptr_t)(G_BYTES + 0x410 + 0xc);
    get_tsl_px(obj, obj);
    if ((long)obj->field20 > 0x7f)
        q_yes(obj);
    else
        q_no(obj);
}

/* q_kabal_animal -- armv7 0x00052abc, 52 bytes.  **Complete.**  See q_jax_zap.
 *
 * The one member of the three with a different threshold -- half the others'
 * -- and an address 0x68 below theirs. */
void q_kabal_animal(MK3OBJ *obj)
{
    obj->field1c = (uint32_t)(uintptr_t)(G_BYTES + 0x3a8);
    get_tsl_px(obj, obj);
    if ((long)obj->field20 > 0x3f)
        q_yes(obj);
    else
        q_no(obj);
}


/* q_kano_animal -- armv7 0x000533bc, 52 bytes.  **Complete.**
 *
 * The get_tsl_px family again: &G + 0x3a8 and a threshold of 0x3f, the same
 * pair q_kabal_animal uses, handing the yes to q_close_animal instead of
 * answering directly. Two routines reading the same place with the same
 * threshold and doing different things with the result. */
void q_kano_animal(MK3OBJ *obj)
{
    obj->field1c = (uint32_t)(uintptr_t)(G_BYTES + 0x3a8);
    get_tsl_px(obj, obj);
    if ((long)obj->field20 > 0x3f)
        q_close_animal(obj);
    else
        q_no(obj);
}

/* q_kit_fan -- armv7 0x00052d18, 52 bytes.  **Complete.**  &G + 0x414, > 0x9f. */
void q_kit_fan(MK3OBJ *obj)
{
    obj->field1c = (uint32_t)(uintptr_t)(G_BYTES + 0x410 + 4);
    get_tsl_px(obj, obj);
    if ((long)obj->field20 > 0x9f)
        q_yes(obj);
    else
        q_no(obj);
}

/* q_skel_fatal -- armv7 0x00053304, 52 bytes.  **Complete.**  &G + 0x3ac,
 * > 0x3f, yes to q_close_fatal. */
void q_skel_fatal(MK3OBJ *obj)
{
    obj->field1c = (uint32_t)(uintptr_t)(G_BYTES + 0x3ac);
    get_tsl_px(obj, obj);
    if ((long)obj->field20 > 0x3f)
        q_close_fatal(obj);
    else
        q_no(obj);
}

/* q_sherip_fatal -- armv7 0x00053338, 52 bytes.  **Complete.**  &G + 0x3b4,
 * > 0x3f, yes to q_close_fatal.
 *
 * **Eight routines now share these fifty-two bytes**, and the whole family is
 * three numbers: an address in G, a threshold, and which routine gets the yes.
 * The addresses seen so far run 0x3a8, 0x3ac, 0x3b4, 0x410, 0x414, 0x41c and
 * 0x440 -- word-spaced in two clusters -- and the thresholds are 0x37, 0x3f,
 * 0x7f and 0x9f. */
void q_sherip_fatal(MK3OBJ *obj)
{
    obj->field1c = (uint32_t)(uintptr_t)(G_BYTES + 0x3b4);
    get_tsl_px(obj, obj);
    if ((long)obj->field20 > 0x3f)
        q_close_fatal(obj);
    else
        q_no(obj);
}

/* q_st_zap -- armv7 0x000529d0, 52 bytes.  **Complete.**  &G + 0x440, > 0x37 --
 * the lowest threshold in the family. */
void q_st_zap(MK3OBJ *obj)
{
    obj->field1c = (uint32_t)(uintptr_t)(G_BYTES + 0x440);
    get_tsl_px(obj, obj);
    if ((long)obj->field20 > 0x37)
        q_yes(obj);
    else
        q_no(obj);
}

/* q_pit_fatal_ez -- armv7 0x00050348, 52 bytes.  **Complete.**
 *
 *      c = RoundParam[9]
 *      if (c >= 1 && c <= 4) q_fatality_req(obj); else q_no(obj);
 *
 * **Not a get_tsl_px routine at all**, despite matching the others byte for
 * byte in length. It reads a word out of RoundParam rather than G, and the
 * range test is built entirely from flags: a subtract that leaves 1 unless the
 * value is 1, an unsigned compare that zeroes the answer for 3 and 4, and then
 * a separate equality for 2. Four values reach the yes and they are reached
 * three different ways, which is what the compiler made of one range.
 *
 * RoundParam is the pointer slot Blood.c already indexes as an array of words,
 * so 0x24 is element nine. */
void q_pit_fatal_ez(MK3OBJ *obj)
{
    long c = RoundParam[9];

    if (c >= 1 && c <= 4)
        q_fatality_req(obj);
    else
        q_no(obj);
}


/* q_swat_gun -- armv7 0x00052c98, 52 bytes.  **Complete.**  &G + 0x430,
 * > 0x4f. */
void q_swat_gun(MK3OBJ *obj)
{
    obj->field1c = (uint32_t)(uintptr_t)(G_BYTES + 0x430);
    get_tsl_px(obj, obj);
    if ((long)obj->field20 > 0x4f)
        q_yes(obj);
    else
        q_no(obj);
}

/* q_swat_zoom -- armv7 0x00052c64, 52 bytes.  **Complete.**  &G + 0x424,
 * > 0x1f -- the lowest threshold in the family so far. */
void q_swat_zoom(MK3OBJ *obj)
{
    obj->field1c = (uint32_t)(uintptr_t)(G_BYTES + 0x420 + 4);
    get_tsl_px(obj, obj);
    if ((long)obj->field20 > 0x1f)
        q_yes(obj);
    else
        q_no(obj);
}

/* q_eatit_fatal -- armv7 0x00053290, 56 bytes.  **Complete.**
 *
 *      obj->field1c = 0x00040020 ; obj->field20 = 0x00402000
 *      button_bit_check(obj)
 *      if (obj->field5c == 0) q_no(obj)
 *      else { obj->field30 = 0x70; obj->field34 = 0xa0; q_fatal_dist(obj) }
 *
 * A fourth routine on the same mask pair as q_bubble_fatal, q_lk_mk_fatal and
 * q_scream_fatal, and the only one of the four that then sets a distance band.
 * The band is the usual constant-plus-add-on-the-same-register: 0x70 and 0x30
 * more, so the two bounds cannot drift. */
void q_eatit_fatal(MK3OBJ *obj)
{
    obj->field1c = 0x00040020u;
    obj->field20 = 0x00402000u;
    button_bit_check(obj);
    if (obj->field5c == 0) {
        q_no(obj);
        return;
    }
    obj->field30 = 0x70;
    obj->field34 = 0x70 + 0x30;
    q_fatal_dist(obj);
}

/* is_master_in_finish -- armv7 0x00052594, 56 bytes.  **Complete.**
 *
 *      if (GetThreadFunc(&mytc[2]) == t_fatality_wait) q_yes(obj)
 *      else q_no(obj)
 *
 * **The 0x218 is an index, not an offset into a struct.** other.c established
 * that mytc is an array of threads with a stride of 268, and 0x218 is exactly
 * twice that -- so this asks about thread number two, whatever the object it
 * was handed happens to be. The question is about the machine's state, not
 * about either fighter.
 *
 * The handler it compares against comes through a pointer slot rather than as
 * a link-time constant, so t_fatality_wait lives in another translation unit. */
void is_master_in_finish(MK3OBJ *obj)
{
    MK3THREAD *master = (MK3THREAD *)((char *)mytc + 2 * 268);

    if (GetThreadFunc(master) == (void *)t_fatality_wait)
        q_yes(obj);
    else
        q_no(obj);
}


/* check_tsl -- armv7 0x00052904, 60 bytes.  **Complete.**
 *
 *      obj->field1c = obj->field68
 *      obj->field28 = obj->field64
 *      get_tsl_px(obj, other)
 *      if (obj->field20 <= obj->field28) return
 *      if (obj->field34 > 7) obj->field34 = 0
 *      xfer_types_table[obj->field34](obj, other)
 *
 * **A dispatch through a table of eight function pointers**, indexed by 0x34
 * -- the same field that holds a callback address elsewhere in this codebase,
 * here holding a small index instead. Out of range is not an error: anything
 * above 7 is written back as 0 and the first entry runs.
 *
 * Unlike the q_ family, get_tsl_px is handed the OTHER object as its reference
 * rather than the same one twice, and both of its inputs come out of the
 * object -- 0x68 as the argument and 0x64 as the threshold to beat, neither
 * of which the struct names, so both are reached as offsets. Nothing is
 * a constant here; the caller has set all of it up. */
void check_tsl(MK3OBJ *obj, MK3OBJ *other)
{
    obj->field1c = *(uint32_t *)((char *)obj + 0x68);
    obj->field28 = *(uint32_t *)((char *)obj + 0x64);
    get_tsl_px(obj, other);

    if ((long)obj->field20 <= (long)obj->field28)
        return;

    if (obj->field34 > 7)
        obj->field34 = 0;

    ((XFER_FN)xfer_types_table[obj->field34])(obj, other);
}

/* q_mercy_req_ez -- armv7 0x000502e0, 60 bytes.  **Complete.**
 *
 *      if (*(uint16_t *)(G + 0x45a) != 0) q_no(obj)
 *      else if (*(uint32_t *)H == 0) q_no(obj)
 *      else if (*(uint32_t *)(H + 4) == 0) q_no(obj)
 *      else q_fatality_req(obj)
 *
 * **The mercy halfword has to be ZERO here**, the opposite of what q_mercy
 * requires -- so this asks whether mercy has NOT been used. Then two words at
 * the front of H must both be non-zero, and each is stored into 0x1c as it is
 * tested, so the field ends up holding whichever value made the decision.
 *
 * Three refusals branch to one q_no. */
void q_mercy_req_ez(MK3OBJ *obj)
{
    obj->field1c = (uint32_t)(int32_t)
        (int16_t)*(uint16_t *)(G_BYTES + 0x45a);
    if (*(uint16_t *)(G_BYTES + 0x45a) != 0)
        goto no;

    obj->field1c = *(uint32_t *)H;
    if (obj->field1c == 0)
        goto no;

    obj->field1c = *(uint32_t *)(H + 4);
    if (obj->field1c == 0)
        goto no;

    q_fatality_req(obj);
    return;

no:
    q_no(obj);
}

/* robo_lp_close -- armv7 0x000546d8, 60 bytes.  **Complete.**
 *
 *      if (stick_look_lr2(obj, other, scom_robo_zap1, 0x10000, 0x100000)) {
 *          obj->field38 = t_do_robo_zap
 *          restricted_xfer(obj, other)
 *      }
 *
 * The two numbers are one literal and a subtraction off it -- 0x100000 then
 * less 0xf0000 -- so the pair is built the same way the velocity triples are,
 * and they reach stick_look_lr2 as the two words it packs into an array.
 *
 * robo_hp_close is the same sixty bytes with 0x1000 and 0x10, scom_robo_zap2
 * and t_do_robo_zap2: a factor of sixteen between the two routines' constants
 * and a different table and handler. */
void robo_lp_close(MK3OBJ *obj, MK3OBJ *other)
{
    if (stick_look_lr2(obj, (uint32_t)(uintptr_t)other,
                       (uint32_t)(uintptr_t)scom_robo_zap1,
                       0x100000u - 0xf0000u, 0x100000u)) {
        obj->field38 = (uint32_t)(uintptr_t)t_do_robo_zap;
        restricted_xfer(obj, other);
    }
}

/* robo_hp_close -- armv7 0x0005469c, 60 bytes.  **Complete.**  See
 * robo_lp_close. */
void robo_hp_close(MK3OBJ *obj, MK3OBJ *other)
{
    if (stick_look_lr2(obj, (uint32_t)(uintptr_t)other,
                       (uint32_t)(uintptr_t)scom_robo_zap2,
                       0x1000u - 0xff0u, 0x1000u)) {
        obj->field38 = (uint32_t)(uintptr_t)t_do_robo_zap2;
        restricted_xfer(obj, other);
    }
}


/* jax_lk_open -- armv7 0x00052940, 60 bytes.  **Complete.**
 *
 *      obj->field34 = 0                            ; the table index
 *      obj->field38 = t_do_quake                   ; the handler
 *      obj->field68 = &G + 0x3b8                   ; check_tsl's argument
 *      obj->field64 = other->field00->0x7e ? 5 : 0x50   ; its threshold
 *      check_tsl(obj, other)
 *
 * **The producer for check_tsl, and it sets all four of the fields that
 * routine reads.** 0x34 picks entry zero of the function table, 0x38 carries
 * the handler that entry will use, 0x68 is the address handed to get_tsl_px and
 * 0x64 the value its answer must beat. Reading the two together confirms every
 * one of them.
 *
 * **The threshold is sixteen times easier when a halfword in the opponent's
 * proc is set**: 5 instead of 0x50. That halfword sits at 0x7e, immediately
 * after the four-button gate at 0x7c, and the struct names neither. */
void jax_lk_open(MK3OBJ *obj, MK3OBJ *other)
{
    obj->field34 = 0;
    obj->field38 = (uint32_t)(uintptr_t)t_do_quake;
    *(uint32_t *)((char *)obj + 0x68) =
        (uint32_t)(uintptr_t)(G_BYTES + 0x3b8);

    if (*(int16_t *)((char *)other->field00 + 0x7e) != 0)
        *(uint32_t *)((char *)obj + 0x64) = 5;
    else
        *(uint32_t *)((char *)obj + 0x64) = 0x50;

    check_tsl(obj, other);
}

/* q_ind_axe_fatal -- armv7 0x00053254, 60 bytes.  **Complete.**
 *
 * get_tsl_px at &G + 0x3b8 over 0x3f, then a band of 0xa0 and 0x40 more before
 * q_fatal_dist. The same two-stage shape as q_eatit_fatal, reached through the
 * table instead of through the buttons. */
void q_ind_axe_fatal(MK3OBJ *obj)
{
    obj->field1c = (uint32_t)(uintptr_t)(G_BYTES + 0x3b8);
    get_tsl_px(obj, obj);
    if ((long)obj->field20 <= 0x3f) {
        q_no(obj);
        return;
    }
    obj->field30 = 0xa0;
    obj->field34 = 0xa0 + 0x40;
    q_fatal_dist(obj);
}

/* q_shang_animal -- armv7 0x000531a0, 60 bytes.  **Complete.**
 *
 * The same shape at &G + 0x3a8 with a band of 0x70 and 0x30 more, ending in
 * q_animal_dist rather than q_fatal_dist. */
void q_shang_animal(MK3OBJ *obj)
{
    obj->field1c = (uint32_t)(uintptr_t)(G_BYTES + 0x3a8);
    get_tsl_px(obj, obj);
    if ((long)obj->field20 <= 0x3f) {
        q_no(obj);
        return;
    }
    obj->field30 = 0x70;
    obj->field34 = 0x70 + 0x30;
    q_animal_dist(obj);
}

/* q_st_spike_fatal -- armv7 0x00054bc0, 60 bytes.  **Complete.**
 *
 *      obj->field1c = &G + 0x3ac ; get_tsl_px(obj, obj)
 *      if (obj->field20 <= 0x3f) q_no(obj)
 *      else {
 *          q_am_i_cornered(obj)
 *          if (obj->field5c == 0) q_close_fatal(obj); else q_no(obj);
 *      }
 *
 * Two conditions and no distance band: the table entry has to be over 0x3f and
 * the asking fighter must not be cornered himself. q_inflate_fatal asks the
 * same question about the OTHER fighter -- one routine checks where he is
 * standing, this one checks where I am. */
void q_st_spike_fatal(MK3OBJ *obj)
{
    obj->field1c = (uint32_t)(uintptr_t)(G_BYTES + 0x3ac);
    get_tsl_px(obj, obj);
    if ((long)obj->field20 <= 0x3f)
        goto no;
    q_am_i_cornered(obj);
    if (obj->field5c != 0)
        goto no;
    q_close_fatal(obj);
    return;

no:
    q_no(obj);
}


/* lao_up -- armv7 0x00054924, 64 bytes.  **Complete.**
 *
 *      if (stick_look_lr2(obj, other, scom_lao_teleport,
 *                         0x00030070, 0x00307000)) {
 *          obj->field38 = t_do_lao_tele
 *          restricted_xfer(obj, other)
 *      }
 *
 * **The pair obeys the same rule the button masks do**: (0x0003, 0x0070) and
 * (0x0030, 0x7000), the high half shifted left four and the low half left
 * eight. The robo pair does too -- 0x10 with 0x1000, and 0x10000 with
 * 0x100000 -- so whatever stick_look_lr does with its two words, it is being
 * handed the same per-player arrangement as button_bit_check. */
void lao_up(MK3OBJ *obj, MK3OBJ *other)
{
    if (stick_look_lr2(obj, (uint32_t)(uintptr_t)other,
                       (uint32_t)(uintptr_t)scom_lao_teleport,
                       0x00030070u, 0x00307000u)) {
        obj->field38 = (uint32_t)(uintptr_t)t_do_lao_tele;
        restricted_xfer(obj, other);
    }
}

/* q_scorp_tele -- armv7 0x00052a04, 64 bytes.  **Complete.**
 *
 *      get_his_p_hit(obj)
 *      if (obj->field1c > 4) q_no(obj)
 *      else {
 *          obj->field1c = &G + 0x418 ; get_tsl_px(obj, obj)
 *          if (obj->field20 > 0x4f) q_yes(obj); else q_no(obj);
 *      }
 *
 * A hit count in front of a table entry: two questions, and 0x1c carries the
 * count for the first and the table address for the second. This is what
 * q_scorp_tele_four and q_scorp_tele_six gate with the four-button test. */
void q_scorp_tele(MK3OBJ *obj)
{
    get_his_p_hit(obj);
    if ((long)obj->field1c > 4)
        goto no;
    obj->field1c = (uint32_t)(uintptr_t)(G_BYTES + 0x418);
    get_tsl_px(obj, obj);
    if ((long)obj->field20 > 0x4f) {
        q_yes(obj);
        return;
    }
    goto no;

no:
    q_no(obj);
}

/* q_ermac_slam -- armv7 0x00052b24, 64 bytes.  **Complete.**
 *
 * The same shape with a count of 3, &G + 0x434 and a threshold of 0x7f. */
void q_ermac_slam(MK3OBJ *obj)
{
    get_his_p_hit(obj);
    if ((long)obj->field1c > 3)
        goto no;
    obj->field1c = (uint32_t)(uintptr_t)(G_BYTES + 0x430 + 4);
    get_tsl_px(obj, obj);
    if ((long)obj->field20 > 0x7f) {
        q_yes(obj);
        return;
    }
    goto no;

no:
    q_no(obj);
}

/* t_do_st_2_kano -- armv7 0x00052554, 64 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field40 = ((MK3OBJ *)obj->field00->him)->field24
 *      frame[frame].handler = t_shang_morph
 *      frame[frame+1].w0 = 0
 *
 * A thread routine among the questions -- the only one in this stretch of the
 * file. It copies the OPPONENT's character number into its own 0x40 and then
 * hands over to t_shang_morph, which is what turns Shang Tsung into him.
 *
 * The guard is really there in the binary, a cbz on the slot with mvn r0, #2
 * on the other side, so this is a state-0 routine and mk3_push_handler is the
 * right helper. */
long t_do_st_2_kano(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field40 = ((MK3OBJ *)(uintptr_t)obj->field00->him)->field24;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_shang_morph);
}


/* slide_check -- armv7 0x000547c0, 72 bytes.  **Complete.**
 *
 *      is_stick_away(obj)
 *      if (obj->field5c == 0) return
 *      obj->field1c = 0x00030020 ; obj->field20 = 0x00302000
 *      button_bit_check(obj)
 *      if (obj->field5c == 0) return
 *      obj->field38 = t_do_slide
 *      restricted_xfer(obj, other)
 *      obj->field5c = 1
 *
 * **It answers through 0x5c the same way the q_ family does, and that is what
 * sz_lp_close reads.** Both early exits leave 0x5c holding the zero the failed
 * predicate wrote, and the success path sets it to 1 after the transfer -- so
 * the field means "a slide was started", which is exactly why sz_lp_close
 * refuses to search a table when it is set.
 *
 * The masks are the same pair q_lp_block_lk uses, which is the same question
 * asked directly rather than through q_slide. */
void slide_check(MK3OBJ *obj, MK3OBJ *other)
{
    is_stick_away(obj);
    if (obj->field5c == 0)
        return;

    obj->field1c = 0x00030020u;
    obj->field20 = 0x00302000u;
    button_bit_check(obj);
    if (obj->field5c == 0)
        return;

    obj->field38 = (uint32_t)(uintptr_t)t_do_slide;
    restricted_xfer(obj, other);
    obj->field5c = 1;
}

/* q_pit_fatal -- armv7 0x00054b78, 72 bytes.  **Complete.**
 *
 *      c = RoundParam[9]
 *      if (c < 1 || c > 4) q_no(obj)
 *      else {
 *          is_he_facing_me(obj)
 *          if (obj->field5c == 0) q_no(obj); else q_close_fatal_pit(obj);
 *      }
 *
 * q_pit_fatal_ez with a second condition bolted on, built from the same three
 * flag tricks for the range and then a facing check. The easy form skips
 * asking which way he is turned. */
void q_pit_fatal(MK3OBJ *obj)
{
    long c = RoundParam[9];

    if (c < 1 || c > 4)
        goto no;
    is_he_facing_me(obj);
    if (obj->field5c == 0)
        goto no;
    q_close_fatal_pit(obj);
    return;

no:
    q_no(obj);
}

/* q_floor_blade -- armv7 0x00053590, 72 bytes.  **Complete.**
 *
 *      if (CountThreads(0x206) != 0) q_no(obj)
 *      else if (CountThreads(0x207) != 0) q_no(obj)
 *      else if (obj->field1c > 2 after get_his_p_hit) q_no(obj)
 *      else if (obj->field20 == 0x617 after get_his_action) q_no(obj)
 *      else q_yes(obj)
 *
 * **Four conditions and one yes.** The first two count live threads by pid --
 * two blades already on the screen is the refusal, and the two pids are
 * consecutive -- then the opponent's hit count and his current action are
 * checked. All four refusals branch to the same two instructions. */
void q_floor_blade(MK3OBJ *obj)
{
    if (CountThreads(0x206) != 0 || CountThreads(0x207) != 0)
        goto no;

    get_his_p_hit(obj);
    if ((long)obj->field1c > 2)
        goto no;

    get_his_action(obj);
    if (obj->field20 == 0x617)
        goto no;
    q_yes(obj);
    return;

no:
    q_no(obj);
}

/* lao_hk_close -- armv7 0x000545a4, 72 bytes.  **Complete.**
 *
 *      if (!stick_look_lr2(obj, other, scom_lao_angle_kick, 0x40, 0x4000))
 *          return
 *      distance_from_ground(obj)
 *      if (obj->field1c <= 0x9f) return
 *      obj->field38 = t_do_lao_angle_kick
 *      airborn_xfer(obj, other)
 *
 * A height gate on top of the stick pattern: the kick only opens above 0x9f
 * off the ground, and the transfer goes through airborn_xfer rather than
 * restricted_xfer -- the two differ in which way round they test being in the
 * air. The pair 0x40 and 0x4000 follows the same half-shift rule as the rest. */
void lao_hk_close(MK3OBJ *obj, MK3OBJ *other)
{
    if (!stick_look_lr2(obj, (uint32_t)(uintptr_t)other,
                        (uint32_t)(uintptr_t)scom_lao_angle_kick,
                        0x4000u - 0x3fc0u, 0x4000u))
        return;

    distance_from_ground(obj);
    if ((long)obj->field1c <= 0x9f)
        return;

    obj->field38 = (uint32_t)(uintptr_t)t_do_lao_angle_kick;
    airborn_xfer(obj, other);
}


/* q_kano_upball -- armv7 0x00052a74, 72 bytes.  **Complete.**
 *
 *      obj->field1c = &G + 0x440 ; get_tsl_px(obj, obj)
 *      if (obj->field20 <= 0x3f) q_no(obj)
 *      else {
 *          obj->field1c = &G + 0x424 ; get_tsl_px(obj, obj)
 *          if (obj->field20 > 0x27) q_yes(obj); else q_no(obj);
 *      }
 *
 * **Two table entries, both of which have to pass.** The base of G is kept in
 * a callee-saved register across the first call so the second address costs
 * one add rather than another pool load -- which is how you can tell the two
 * lookups were meant as a pair.
 *
 * q_mileena_zap is the same seventy-two bytes with &G + 0x424 over 0x1f and
 * then &G + 0x3a8 over 7. */
void q_kano_upball(MK3OBJ *obj)
{
    obj->field1c = (uint32_t)(uintptr_t)(G_BYTES + 0x440);
    get_tsl_px(obj, obj);
    if ((long)obj->field20 <= 0x3f)
        goto no;

    obj->field1c = (uint32_t)(uintptr_t)(G_BYTES + 0x420 + 4);
    get_tsl_px(obj, obj);
    if ((long)obj->field20 > 0x27) {
        q_yes(obj);
        return;
    }
    goto no;

no:
    q_no(obj);
}

/* q_mileena_zap -- armv7 0x00052bc8, 72 bytes.  **Complete.**  See
 * q_kano_upball. */
void q_mileena_zap(MK3OBJ *obj)
{
    obj->field1c = (uint32_t)(uintptr_t)(G_BYTES + 0x420 + 4);
    get_tsl_px(obj, obj);
    if ((long)obj->field20 <= 0x1f)
        goto no;

    obj->field1c = (uint32_t)(uintptr_t)(G_BYTES + 0x3a8);
    get_tsl_px(obj, obj);
    if ((long)obj->field20 > 7) {
        q_yes(obj);
        return;
    }
    goto no;

no:
    q_no(obj);
}

/* lia_hk_close -- armv7 0x00054964, 72 bytes.  **Complete.**
 *
 *      if (!stick_look_lr2(obj, other, scom_fly, 0x40, 0x4000)) return
 *      q_is_he_a_boss(obj)
 *      if (obj->field5c != 0) return
 *      obj->field38 = t_do_lia_fly
 *      restricted_xfer(obj, other)
 *
 * The same stick pair as lao_hk_close with a boss check where that has a
 * height check -- and against a boss the move simply does not open. */
void lia_hk_close(MK3OBJ *obj, MK3OBJ *other)
{
    if (!stick_look_lr2(obj, (uint32_t)(uintptr_t)other,
                        (uint32_t)(uintptr_t)scom_fly,
                        0x4000u - 0x3fc0u, 0x4000u))
        return;

    q_is_he_a_boss(obj);
    if (obj->field5c != 0)
        return;

    obj->field38 = (uint32_t)(uintptr_t)t_do_lia_fly;
    restricted_xfer(obj, other);
}

/* lao_lp_close -- armv7 0x000548d8, 72 bytes.  **Complete.**
 *
 *      if (!stick_look_lr2(obj, other, scom_lao_zap, 0x10000, 0x100000))
 *          return
 *      if (CountThreads(obj->field00->field08 + 0x700) != 0) return
 *      obj->field38 = t_do_lao_zap
 *      restricted_xfer(obj, other)
 *
 * **The pid it counts is per-player.** 0x700 plus the strength index, so each
 * fighter has his own thread id in that range and one hat in flight only
 * blocks its own owner. q_floor_blade counts two fixed pids instead; this one
 * builds its pid out of who is asking. */
void lao_lp_close(MK3OBJ *obj, MK3OBJ *other)
{
    if (!stick_look_lr2(obj, (uint32_t)(uintptr_t)other,
                        (uint32_t)(uintptr_t)scom_lao_zap,
                        0x100000u - 0xf0000u, 0x100000u))
        return;

    if (CountThreads(obj->field00->field08 + 0x700) != 0)
        return;

    obj->field38 = (uint32_t)(uintptr_t)t_do_lao_zap;
    restricted_xfer(obj, other);
}


/* check_sonya_legs -- armv7 0x0005362c, 80 bytes.  **Complete.**
 *
 *      obj->field1c = 0x00010020 ; obj->field20 = 0x00102000
 *      button_bit_check(obj)
 *      if (obj->field5c == 0) return
 *      get_his_action(obj)
 *      if (obj->field20 == 0x600 || obj->field20 == 0x506) {
 *          obj->field5c = 0
 *          return
 *      }
 *      is_stick_down(obj)
 *
 * **A sixth pair on the half-shift rule**: (0x0001, 0x0020) and (0x0010,
 * 0x2000), and the second is written as an immediate rather than loaded, which
 * is why only the first needed a literal pool entry.
 *
 * The answer is left wherever the last thing to write 0x5c put it -- cleared
 * by hand for the two forbidden actions, and otherwise whatever is_stick_down
 * decides. Three different writers of one field, in one routine. */
void check_sonya_legs(MK3OBJ *obj)
{
    obj->field1c = 0x00010020u;
    obj->field20 = 0x00102000u;
    button_bit_check(obj);
    if (obj->field5c == 0)
        return;

    get_his_action(obj);
    if (obj->field20 == 0x600 || obj->field20 == 0x506) {
        obj->field5c = 0;
        return;
    }

    is_stick_down(obj);
}

/* q_jax_dash -- armv7 0x00052ee0, 76 bytes.  **Complete.**
 *
 *      obj->field1c = &G + 0x424 ; get_tsl_px(obj, obj)
 *      if (obj->field20 <= 0x2f) q_no(obj)
 *      else {
 *          get_his_action(obj)
 *          if (obj->field20 == 0x509 || obj->field20 == 0x600) q_no(obj)
 *          else q_yes(obj)
 *      }
 *
 * A table entry and then two forbidden actions. All three refusals reach the
 * same q_no. */
void q_jax_dash(MK3OBJ *obj)
{
    obj->field1c = (uint32_t)(uintptr_t)(G_BYTES + 0x420 + 4);
    get_tsl_px(obj, obj);
    if ((long)obj->field20 <= 0x2f)
        goto no;

    get_his_action(obj);
    if (obj->field20 == 0x509 || obj->field20 == 0x600)
        goto no;
    q_yes(obj);
    return;

no:
    q_no(obj);
}

/* q_sz_decoy -- armv7 0x00052e64, 76 bytes.  **Complete.**
 *
 *      get_his_p_hit(obj)
 *      if (obj->field1c > 0) q_no(obj)
 *      else {
 *          obj->field1c = &G + 0x3f8 ; get_tsl_px(obj, obj)
 *          if (obj->field20 <= 0xff) q_no(obj)
 *          else {
 *              get_his_action(obj)
 *              if (obj->field20 == 0x610) q_no(obj); else q_yes(obj);
 *          }
 *      }
 *
 * Three conditions in a row, and the threshold is 0xff -- the highest in the
 * get_tsl_px family by a wide margin. He must also be completely unhit, not
 * merely under a count. */
void q_sz_decoy(MK3OBJ *obj)
{
    get_his_p_hit(obj);
    if ((long)obj->field1c > 0)
        goto no;

    obj->field1c = (uint32_t)(uintptr_t)(G_BYTES + 0x3f8);
    get_tsl_px(obj, obj);
    if ((long)obj->field20 <= 0xff)
        goto no;

    get_his_action(obj);
    if (obj->field20 == 0x610)
        goto no;
    q_yes(obj);
    return;

no:
    q_no(obj);
}

/* q_stick_sweep -- armv7 0x00052ccc, 76 bytes.  **Complete.**
 *
 *      if (((*(uint32_t *)(Pp + 0x10) & 1) & *(uint32_t *)(Pp + 0x9c)) == 0)
 *          q_yes(obj)
 *      else {
 *          obj->field1c = &G + 0x418 ; get_tsl_px(obj, obj)
 *          if (obj->field20 <= 0x4f) q_no(obj); else q_yes(obj);
 *      }
 *
 * **The gate is one bit ANDed against a whole word.** The low bit of Pp + 0x10
 * is masked out first and then tested against Pp + 0x9c. When the two do not
 * share that bit, `beq` goes straight to the q_yes call (0x501ac) and the
 * distance is never asked; only when both have it does the sweep need the
 * opponent more than 0x4f away. (An earlier reading had the gate answering
 * no; tools/difftest caught it.) Written as the two instructions do it,
 * because which of the two words is the flag and which the mask is not
 * established. */
void q_stick_sweep(MK3OBJ *obj)
{
    if (((*(uint32_t *)(Pp + 0x10) & 1u)
         & *(uint32_t *)(Pp + 0x9c)) != 0) {
        obj->field1c = (uint32_t)(uintptr_t)(G_BYTES + 0x418);
        get_tsl_px(obj, obj);
        if ((long)obj->field20 <= 0x4f)
            goto no;
    }
    q_yes(obj);
    return;

no:
    q_no(obj);
}


/* q_tusk_blur -- armv7 0x00052e14, 80 bytes.  **Complete.**
 *
 *      obj->field1c = obj->field00->field00->field00->p_hit
 *      if (obj->field1c > 1) q_no(obj)
 *      else {
 *          get_his_action(obj)
 *          if (his action is 0x60c, 0x60b, 0x617, 0x600 or 0x509) q_no(obj)
 *          else q_yes(obj)
 *      }
 *
 * **The four-load chain is inlined here rather than called.** It is exactly
 * what get_his_p_hit does, written out instead of branched to -- eighty bytes
 * with room for it. Five forbidden actions follow, all five branching to one
 * q_no, which is the longest such list in the file. */
void q_tusk_blur(MK3OBJ *obj)
{
    obj->field1c = obj->field00->field00->field00->p_hit;
    if ((long)obj->field1c > 1)
        goto no;

    get_his_action(obj);
    if (obj->field20 == 0x60c || obj->field20 == 0x60b
        || obj->field20 == 0x617 || obj->field20 == 0x600
        || obj->field20 == 0x509)
        goto no;
    q_yes(obj);
    return;

no:
    q_no(obj);
}

/* q_mileena_roll -- armv7 0x00052c10, 84 bytes.  **Complete.**
 *
 * A hit count and then two table entries, both over 0x1f, at &G + 0x424 and
 * &G + 0x428 -- adjacent words. Three conditions, one yes. */
void q_mileena_roll(MK3OBJ *obj)
{
    get_his_p_hit(obj);
    if ((long)obj->field1c > 2)
        goto no;

    obj->field1c = (uint32_t)(uintptr_t)(G_BYTES + 0x420 + 4);
    get_tsl_px(obj, obj);
    if ((long)obj->field20 <= 0x1f)
        goto no;

    obj->field1c = (uint32_t)(uintptr_t)(G_BYTES + 0x428);
    get_tsl_px(obj, obj);
    if ((long)obj->field20 <= 0x1f)
        goto no;
    q_yes(obj);
    return;

no:
    q_no(obj);
}

/* q_spear -- armv7 0x0005297c, 84 bytes.  **Complete.**
 *
 * Two table entries and then a hit count -- the same three conditions as
 * q_mileena_roll in the opposite order. &G + 0x424 over 0x2f, &G + 0x420 over
 * 0x5f, and no more than four hits. */
void q_spear(MK3OBJ *obj)
{
    obj->field1c = (uint32_t)(uintptr_t)(G_BYTES + 0x420 + 4);
    get_tsl_px(obj, obj);
    if ((long)obj->field20 <= 0x2f)
        goto no;

    obj->field1c = (uint32_t)(uintptr_t)(G_BYTES + 0x420);
    get_tsl_px(obj, obj);
    if ((long)obj->field20 <= 0x5f)
        goto no;

    get_his_p_hit(obj);
    if ((long)obj->field1c > 4)
        goto no;
    q_yes(obj);
    return;

no:
    q_no(obj);
}

/* robo2_lp_close -- armv7 0x000543dc, 80 bytes.  **Complete.**
 *
 *      t = other->thread
 *      if (frame[t->frame].handler != t_air_sleep3) return
 *      get_y_dist(obj) ; if (obj->field28 > 0x30) return
 *      get_x_dist(obj) ; if (obj->field28 > 0x50) return
 *      obj->field38 = t_do_air_slam
 *      free_xfer(obj, other)
 *
 * **It asks what the opponent's thread is running right now.** The frame index
 * is read out of his own thread, scaled by eight, and the handler at that level
 * is compared against t_air_sleep3 -- the chase written in mkprop.c. So the
 * slam only opens while he is already being chased through the air, and then
 * only inside 0x30 vertically and 0x50 horizontally.
 *
 * Both the expected handler and the installed one come through pointer slots,
 * so both live in other translation units. This is the only routine in
 * moves.c that reaches into another object's frame array. */
void robo2_lp_close(MK3OBJ *obj, MK3OBJ *other)
{
    MK3THREAD *t = other->thread;

    if (mk3_frame(t, t->frame)[1] != (uint32_t)(uintptr_t)t_air_sleep3)
        return;

    get_y_dist(obj);
    if ((long)obj->field28 > 0x30)
        return;

    get_x_dist(obj);
    if ((long)obj->field28 > 0x50)
        return;

    obj->field38 = (uint32_t)(uintptr_t)t_do_air_slam;
    free_xfer(obj, other);
}


/* q_sz_forward_zap -- armv7 0x000535d8, 84 bytes.  **Complete.**
 *
 *      get_his_p_hit(obj)
 *      if (obj->field1c > 1) q_no(obj)
 *      else if (CountThreads(obj->field00->field08 + 0x204) != 0) q_no(obj)
 *      else if (CountThreads(obj->field00->field08 + 0x707) != 0) q_no(obj)
 *      else { get_his_action(obj)
 *             if (obj->field20 == 0x509) q_no(obj); else q_yes(obj) }
 *
 * **The first pid it counts is the decoy's.** tl_do_sz_decoy builds its new
 * thread's pid as the strength index plus 0x204 and this counts exactly that,
 * so the zap is refused while Sub-Zero's own decoy is still standing. The
 * second is the index plus 0x707, built as 0x700 and then seven more.
 *
 * Reading the two sides together turns an arbitrary-looking constant into the
 * same number in both places. */
void q_sz_forward_zap(MK3OBJ *obj)
{
    get_his_p_hit(obj);
    if ((long)obj->field1c > 1)
        goto no;

    if (CountThreads(obj->field00->field08 + 0x204) != 0
        || CountThreads(obj->field00->field08 + 0x700 + 7) != 0)
        goto no;

    get_his_action(obj);
    if (obj->field20 == 0x509)
        goto no;
    q_yes(obj);
    return;

no:
    q_no(obj);
}

/* robo1_lk_close -- armv7 0x0005444c, 88 bytes.  **Complete.**
 *
 * The stick pattern, then a hit count of at most one, then one forbidden
 * action, and the transfer goes through airborn_xfer. The pair is 0x20000 and
 * 0x200000 -- the high half shifted left four, as always. */
void robo1_lk_close(MK3OBJ *obj, MK3OBJ *other)
{
    if (!stick_look_lr2(obj, (uint32_t)(uintptr_t)other,
                        (uint32_t)(uintptr_t)scom_robo_tele,
                        0x200000u - 0x1e0000u, 0x200000u))
        return;

    get_his_p_hit(obj);
    if ((long)obj->field1c > 1)
        return;

    get_his_action(obj);
    if (obj->field20 == 0x616)
        return;

    obj->field38 = (uint32_t)(uintptr_t)t_do_robo_tele;
    airborn_xfer(obj, other);
}

/* q_floor_ice -- armv7 0x00054c24, 96 bytes.  **Complete.**
 *
 * Five conditions and one yes: his action must not be 0x610, both halves of
 * what get_his_dfe leaves in 0x30 and 0x34 must be over 0x5f, the table entry
 * at &G + 0x42c must be over 0xbf, and he must have been hit at most once.
 *
 * The two 0x5f tests read the pair get_his_dfe writes -- the same 0x30/0x34
 * pair q_fatal_dist takes as a distance band, used here as two independent
 * thresholds rather than as a range. */
void q_floor_ice(MK3OBJ *obj)
{
    get_his_action(obj);
    if (obj->field20 == 0x610)
        goto no;

    get_his_dfe(obj);
    if ((long)obj->field30 <= 0x5f || (long)obj->field34 <= 0x5f)
        goto no;

    obj->field1c = (uint32_t)(uintptr_t)(G_BYTES + 0x420 + 0xc);
    get_tsl_px(obj, obj);
    if ((long)obj->field20 <= 0xbf)
        goto no;

    get_his_p_hit(obj);
    if ((long)obj->field1c > 1)
        goto no;
    q_yes(obj);
    return;

no:
    q_no(obj);
}

/* mercy_xfer -- armv7 0x00054ac4, 96 bytes.  **Complete.**
 *
 *      *(uint16_t *)((char *)other->field00 + 0x80) = 0
 *      if (*(int16_t *)(G + 0x45c) != 3) return
 *      if (*(int16_t *)(G + 0x450) != 0) return
 *      is_he_airborn(obj)
 *      if (obj->field5c != 0) return
 *      t = other->field00->field00->thread
 *      if (frame[t->frame].handler != t_dizzy_sleep) return
 *      restricted_xfer(obj, other)
 *
 * **Four gates, and the last one asks what a thread is running.** The
 * halfword at G + 0x45c is the same one q_fatality_req tests for exactly
 * three, so mercy is only offered at that stage of the round; G + 0x450 must
 * be clear; the opponent must be on the ground; and his thread must be sitting
 * in t_dizzy_sleep.
 *
 * The thread is reached by three loads -- his proc, the object that proc points
 * at, and its 0x04 -- which is the same walk get_his_p_hit does for a field.
 * Only robo2_lp_close does anything similar in this file.
 *
 * The clear at 0x80 happens first and unconditionally, as in every other
 * member of the xfer family. */
long mercy_xfer(MK3OBJ *obj, MK3OBJ *other)
{
    MK3THREAD *t;

    *(uint16_t *)((char *)other->field00 + 0x80) = 0;

    if (*(int16_t *)(G_BYTES + 0x45c) != 3)
        return 0;
    if (*(int16_t *)(G_BYTES + 0x450) != 0)
        return 0;

    is_he_airborn(obj);
    if (obj->field5c != 0)
        return 0;

    t = other->field00->field00->thread;
    if (mk3_frame(t, t->frame)[1] != (uint32_t)(uintptr_t)t_dizzy_sleep)
        return 0;

    restricted_xfer(obj, other);
    return 0;
}


/* q_mercy_req -- armv7 0x00053098, 96 bytes.  **Complete.**
 *
 *      if (*(uint16_t *)(G + 0x45a) != 0) q_no(obj)
 *      else if (get_x_dist leaves obj->field28 <= 0x9f) q_no(obj)
 *      else if (*(uint32_t *)H == 0) q_no(obj)
 *      else if (*(uint32_t *)(H + 4) == 0) q_no(obj)
 *      else {
 *          obj->field1c = &G + 0x3cc ; get_tsl_px(obj, obj)
 *          if (obj->field20 <= 0x2f) q_no(obj); else q_fatality_req(obj)
 *      }
 *
 * **q_mercy_req_ez is this with the distance and the table entry taken out.**
 * The two share the mercy halfword and the two words at the front of H, in the
 * same order; the easy form stops there. Five conditions against three.
 *
 * 0x1c holds five different things in turn -- the sign-extended halfword, both
 * H words, then a table address -- and every one of them is stored, so the
 * field ends up carrying whichever value decided the answer. */
void q_mercy_req(MK3OBJ *obj)
{
    obj->field1c = (uint32_t)(int32_t)
        (int16_t)*(uint16_t *)(G_BYTES + 0x45a);
    if (*(uint16_t *)(G_BYTES + 0x45a) != 0)
        goto no;

    get_x_dist(obj);
    if ((long)obj->field28 <= 0x9f)
        goto no;

    obj->field1c = *(uint32_t *)H;
    if (obj->field1c == 0)
        goto no;

    obj->field1c = *(uint32_t *)(H + 4);
    if (obj->field1c == 0)
        goto no;

    obj->field1c = (uint32_t)(uintptr_t)(G_BYTES + 0x3cc);
    get_tsl_px(obj, obj);
    if ((long)obj->field20 <= 0x2f)
        goto no;
    q_fatality_req(obj);
    return;

no:
    q_no(obj);
}

/* q_scorp_airthrow -- armv7 0x00053530, 96 bytes.  **Complete.**
 *
 * **Six conditions, and two of them are about height in different senses.**
 * distance_from_ground must leave more than 0x9f -- I am high up -- and
 * is_he_airborn must come back set, so he is off the ground too. Then both
 * distances inside 0x50, and one forbidden action. Every refusal reaches the
 * same q_no. */
void q_scorp_airthrow(MK3OBJ *obj)
{
    q_is_he_a_boss(obj);
    if (obj->field5c != 0)
        goto no;

    distance_from_ground(obj);
    if ((long)obj->field1c <= 0x9f)
        goto no;

    is_he_airborn(obj);
    if (obj->field5c == 0)
        goto no;

    get_x_dist(obj);
    if ((long)obj->field28 > 0x50)
        goto no;

    get_y_dist(obj);
    if ((long)obj->field28 > 0x50)
        goto no;

    get_his_action(obj);
    if (obj->field20 == 0x507)
        goto no;
    q_yes(obj);
    return;

no:
    q_no(obj);
}

/* q_robo_net -- armv7 0x00052db0, 100 bytes.  **Complete.**
 *
 * A hit count, a forbidden action, and two table entries -- &G + 0x424 over
 * 0x2f and &G + 0x408 over 0x4f. G stays in a callee-saved register across
 * both calls, which is what makes the second address one add instead of a
 * second pool load. */
void q_robo_net(MK3OBJ *obj)
{
    get_his_p_hit(obj);
    if ((long)obj->field1c > 2)
        goto no;

    get_his_action(obj);
    if (obj->field20 == 0x607)
        goto no;

    obj->field1c = (uint32_t)(uintptr_t)(G_BYTES + 0x420 + 4);
    get_tsl_px(obj, obj);
    if ((long)obj->field20 <= 0x2f)
        goto no;

    obj->field1c = (uint32_t)(uintptr_t)(G_BYTES + 0x408);
    get_tsl_px(obj, obj);
    if ((long)obj->field20 <= 0x4f)
        goto no;
    q_yes(obj);
    return;

no:
    q_no(obj);
}


/* kano_lk_open -- armv7 0x00052d4c, 100 bytes.  **Complete.**
 *
 *      get_his_action(obj)
 *      if (obj->field20 == 0x600) return
 *      obj->field1c = &G + 0x440 ; get_tsl_px(obj, other)
 *      if (obj->field20 <= 0x3f) return
 *      obj->field34 = 0 ; obj->field38 = t_do_kano_roll
 *      obj->field68 = &G + 0x3b8
 *      obj->field64 = obj->field00->0x7e ? 0x20 : 0x50
 *      check_tsl(obj, other)
 *
 * A second producer for check_tsl, and it differs from jax_lk_open in two ways
 * worth writing down. It reads the halfword at 0x7e out of its OWN proc where
 * jax_lk_open reads it out of the opponent's, and its easy threshold is 0x20
 * rather than 5. Same field, same shape, two different subjects.
 *
 * Its own get_tsl_px call is handed the OTHER object as the reference, unlike
 * the q_ family which passes the same one twice. */
void kano_lk_open(MK3OBJ *obj, MK3OBJ *other)
{
    get_his_action(obj);
    if (obj->field20 == 0x600)
        return;

    obj->field1c = (uint32_t)(uintptr_t)(G_BYTES + 0x440);
    get_tsl_px(obj, other);
    if ((long)obj->field20 <= 0x3f)
        return;

    obj->field34 = 0;
    obj->field38 = (uint32_t)(uintptr_t)t_do_kano_roll;
    *(uint32_t *)((char *)obj + 0x68) =
        (uint32_t)(uintptr_t)(G_BYTES + 0x3b8);

    if (*(int16_t *)((char *)obj->field00 + 0x7e) != 0)
        *(uint32_t *)((char *)obj + 0x64) = 0x20;
    else
        *(uint32_t *)((char *)obj + 0x64) = 0x50;

    check_tsl(obj, other);
}

/* lia_lk_close -- armv7 0x00054308, 100 bytes.  **Complete.**
 *
 *      if (!stick_look_lr2(obj, other, scom_lia_anglez, 0x20000, 0x200000))
 *          return
 *      distance_from_ground(obj)
 *      if (obj->field1c <= 0xaf) return
 *      obj->field34 = 1 ; obj->field38 = t_do_lia_anglez
 *      obj->field68 = &G + 0x404 ; obj->field64 = 0x30
 *      check_tsl(obj, other)
 *
 * **The first routine seen to pick entry ONE of xfer_types_table.** jax_lk_open
 * and kano_lk_open both write 0 into 0x34; this writes 1, so the table has at
 * least two live entries and the index really is a choice rather than a
 * formality. Its threshold is a plain 0x30 with no halfword to soften it. */
void lia_lk_close(MK3OBJ *obj, MK3OBJ *other)
{
    if (!stick_look_lr2(obj, (uint32_t)(uintptr_t)other,
                        (uint32_t)(uintptr_t)scom_lia_anglez,
                        0x200000u - 0x1e0000u, 0x200000u))
        return;

    distance_from_ground(obj);
    if ((long)obj->field1c <= 0xaf)
        return;

    obj->field34 = 1;
    obj->field38 = (uint32_t)(uintptr_t)t_do_lia_anglez;
    *(uint32_t *)((char *)obj + 0x68) =
        (uint32_t)(uintptr_t)(G_BYTES + 0x400 + 4);
    *(uint32_t *)((char *)obj + 0x64) = 0x30;

    check_tsl(obj, other);
}

/* q_bike_req -- armv7 0x00052fe0, 104 bytes.  **Complete.**
 *
 *      get_his_action(obj)
 *      if (obj->field20 == 0x600) q_no(obj)
 *      obj->field1c = &G + 0x3b8 ; get_tsl_px(obj, obj)
 *      if (obj->field00->0x7e != 0) {
 *          if (obj->field20 <= 0x1f) q_no(obj)
 *          obj->field1c = &G + 0x444 ; get_tsl_px(obj, obj)
 *          if (obj->field20 > 0x7f) q_yes(obj); else q_no(obj)
 *      } else {
 *          if (obj->field20 > 0x7f) q_yes(obj); else q_no(obj)
 *      }
 *
 * **The halfword at 0x7e trades one hard test for two easy ones.** Clear, and
 * the single lookup has to beat 0x7f. Set, and the first only has to beat 0x1f
 * but a second lookup at another address must then beat 0x7f as well. Both
 * arms end at the same two calls, which is why the yes is reached by a branch
 * back into the middle of the first arm. */
void q_bike_req(MK3OBJ *obj)
{
    get_his_action(obj);
    if (obj->field20 == 0x600)
        goto no;

    obj->field1c = (uint32_t)(uintptr_t)(G_BYTES + 0x3b8);
    get_tsl_px(obj, obj);

    if (*(int16_t *)((char *)obj->field00 + 0x7e) != 0) {
        if ((long)obj->field20 <= 0x1f)
        goto no;
        obj->field1c = (uint32_t)(uintptr_t)(G_BYTES + 0x440 + 4);
        get_tsl_px(obj, obj);
    }

    if ((long)obj->field20 > 0x7f) {

        q_yes(obj);

        return;

    }

    goto no;

no:
    q_no(obj);
}


/* q_jade_prop -- armv7 0x00052f2c, 108 bytes.  **Complete.**
 *
 *      get_his_action(obj)
 *      if (his action is 0x509, 0x60c or 0x600) q_no(obj)
 *      else if (get_his_p_hit leaves obj->field1c > 1) q_no(obj)
 *      else {
 *          obj->field1c = &G + 0x424 ; get_tsl_px(obj, obj)
 *          if (obj->field20 <= 0x1f) q_no(obj); else q_yes(obj)
 *      }
 *
 * The first two forbidden actions are folded into flags with no branch between
 * them -- a compare, a conditional 1, then an OR of the second compare into it
 * -- and the third is an ordinary compare after. The compiler split one list
 * three ways. */
void q_jade_prop(MK3OBJ *obj)
{
    get_his_action(obj);
    if (obj->field20 == 0x509 || obj->field20 == 0x60c
        || obj->field20 == 0x600)
        goto no;

    get_his_p_hit(obj);
    if ((long)obj->field1c > 1)
        goto no;

    obj->field1c = (uint32_t)(uintptr_t)(G_BYTES + 0x420 + 4);
    get_tsl_px(obj, obj);
    if ((long)obj->field20 <= 0x1f)
        goto no;
    q_yes(obj);
    return;

no:
    q_no(obj);
}

/* free_xfer -- armv7 0x0005436c, 112 bytes.  **Complete.**
 *
 *      *(uint16_t *)((char *)other->field00 + 0x80) = 0
 *      if (get_strength(other->field00->field08) == 0) return
 *      t = other->thread
 *      fastxfer_thread(obj, t)
 *      saved = frame[t->frame].handler
 *      frame[t->frame + 2].w0 = frame[t->frame + 1].w0
 *      frame[t->frame + 1].handler = saved
 *      frame[t->frame].handler = t_local_reaction_exit
 *      frame[t->frame + 1].w0 = 0
 *      t->frame += 1
 *
 * **This is the mirror of the collapse in t_hover_sleep_1: it OPENS a level
 * rather than closing one.** The handler at the current level is lifted one
 * step up, its token goes with it, an exit is installed in the space that
 * leaves, and the index is bumped. So whatever the thread was running keeps
 * running -- one level higher -- and when it eventually returns down it lands
 * on t_local_reaction_exit instead of on whatever used to be there.
 *
 * That is how the transfer gets a guaranteed exit underneath it without the
 * caller having to know what was already on the stack.
 *
 * The two paths leave different things in r0 -- the strength on the early
 * exit, the lifted handler on the other -- so no value is computed, and both
 * callers discard it. */
void free_xfer(MK3OBJ *obj, MK3OBJ *other)
{
    MK3THREAD *t;
    uint32_t saved;

    *(uint16_t *)((char *)other->field00 + 0x80) = 0;

    if (get_strength(other->field00->field08) == 0)
        return;

    t = other->thread;
    fastxfer_thread(obj, t);

    saved = mk3_frame(t, t->frame)[1];
    *mk3_frame(t, t->frame + 2) = *mk3_frame(t, t->frame + 1);
    mk3_frame(t, t->frame + 1)[1] = saved;

    mk3_frame(t, t->frame)[1] = (uint32_t)(uintptr_t)t_local_reaction_exit;
    *mk3_frame(t, t->frame + 1) = 0;
    t->frame = t->frame + 1;
}


/* sonya_lp_close -- armv7 0x000549d0, 116 bytes.  **Complete.**
 *
 *      pair = { 0x00010000, 0x00100000 }        ; copied from a static const
 *      DbgTableDump(scom_sonya_zap + 1)
 *      if (stick_look_lr(obj, other, scom_sonya_zap, pair)) {
 *          obj->field38 = t_do_sonya_zap
 *          restricted_xfer(obj, other)
 *      } else {
 *          check_sonya_legs(obj)
 *          if (obj->field5c == 0) return
 *          obj->field38 = t_do_leg_throw
 *          restricted_xfer(obj, other)
 *      }
 *
 * **The only routine in this file that calls stick_look_lr directly**, and the
 * reason is visible in the instructions: its two words live in a compiler
 * static rather than as immediates, so they are copied with `ldm`/`stm` into
 * the stack pair instead of being built by hand. Everything else goes through
 * stick_look_lr2, which exists precisely to do that copying.
 *
 * The pair is (0x0001, 0x0000) and (0x0010, 0x0000) -- a seventh measurement
 * of the half-shift rule.
 *
 * **It also calls DbgTableDump**, with the table address plus four. That
 * confirms the parameter is a table pointer, and it means the empty scan is
 * still being made on every one of these checks: the routine walks
 * scom_sonya_zap from its second word and throws away everything it reads.
 *
 * Two different moves come out of one check -- the zap if the stick pattern
 * matched, the leg throw if check_sonya_legs says so instead. */
void sonya_lp_close(MK3OBJ *obj, MK3OBJ *other)
{
    static const uint32_t init[2] = { 0x00010000u, 0x00100000u };
    uint32_t pair[2];

    pair[0] = init[0];
    pair[1] = init[1];

    DbgTableDump(scom_sonya_zap + 1);

    if (stick_look_lr(obj, (uint32_t)(uintptr_t)other,
                      (uint32_t)(uintptr_t)scom_sonya_zap, pair)) {
        obj->field38 = (uint32_t)(uintptr_t)t_do_sonya_zap;
        restricted_xfer(obj, other);
        return;
    }

    check_sonya_legs(obj);
    if (obj->field5c == 0)
        return;

    obj->field38 = (uint32_t)(uintptr_t)t_do_leg_throw;
    restricted_xfer(obj, other);
}

/* jax_block_close -- armv7 0x00054528, 124 bytes.  **Complete.**
 *
 *      distance_from_ground(obj) ; if (obj->field1c <= 0x9f) return
 *      get_x_dist(obj) ; if (obj->field28 > 0x58) return
 *      get_y_dist(obj) ; if (obj->field28 > 0x58) return
 *      obj->field24 = (int16)((MK3OBJ *)obj->a10)->field12
 *      obj->field1c = *(uint32_t *)(G + 0xac) - obj->field24
 *      if (obj->field1c <= 0xb7) return
 *      q_is_he_a_boss(obj) ; if (obj->field5c != 0) return
 *      get_his_action(obj) ; if (obj->field20 == 0x507) return
 *      obj->field38 = t_do_back_breaker
 *      airborn_xfer(obj, other)
 *
 * **Six conditions, and the fourth is a height measured against a global
 * floor.** 0x44 is a pointer here -- the object being reached for -- its 0x12
 * is its integer y, and the difference between the floor at G + 0xac and that y
 * has to exceed 0xb7. Both intermediate values are stored on the way through,
 * so 0x24 keeps the y and 0x1c the gap.
 *
 * Every refusal is one instruction: a branch to the same pop. */
void jax_block_close(MK3OBJ *obj, MK3OBJ *other)
{
    distance_from_ground(obj);
    if ((long)obj->field1c <= 0x9f)
        return;

    get_x_dist(obj);
    if ((long)obj->field28 > 0x58)
        return;

    get_y_dist(obj);
    if ((long)obj->field28 > 0x58)
        return;

    obj->field24 = (uint32_t)(int32_t)*(int16_t *)
        ((char *)(MK3OBJ *)(uintptr_t)obj->a10 + 0x12);
    obj->field1c = *(uint32_t *)(G_BYTES + 0xac) - obj->field24;
    if ((long)obj->field1c <= 0xb7)
        return;

    q_is_he_a_boss(obj);
    if (obj->field5c != 0)
        return;

    get_his_action(obj);
    if (obj->field20 == 0x507)
        return;

    obj->field38 = (uint32_t)(uintptr_t)t_do_back_breaker;
    airborn_xfer(obj, other);
}


/* sonya_hk_close -- armv7 0x00054a44, 128 bytes.  **Complete.**
 *
 *      pair = { 0x00000040, 0x00004000 }
 *      if (!stick_look_lr(obj, other, scom_bike, pair)) return
 *      get_his_action(obj) ; if (obj->field20 == 0x61a) return
 *      if (obj->field00->0x7e != 0) {
 *          obj->field1c = &G + 0x444 ; get_tsl_px(obj, other)
 *          if (obj->field20 <= 0x7f) return
 *      }
 *      obj->field38 = t_do_bike
 *      restricted_xfer(obj, other)
 *
 * The second routine here to copy its pair out of a compiler static, and the
 * pair is (0x0000, 0x0040) and (0x0000, 0x4000) -- the low half shifted left
 * eight, an eighth measurement of the rule. It is stored with `stmdb` from
 * sp+8 downwards rather than `stm` upwards, which puts the same two words in
 * the same two places by the other route.
 *
 * **The halfword at 0x7e adds a condition rather than easing one here.** Set,
 * and a table entry must also beat 0x7f; clear, and the stick pattern alone is
 * enough. q_bike_req uses the same halfword to trade one hard test for two
 * easy ones -- same flag, three different jobs across three routines. */
void sonya_hk_close(MK3OBJ *obj, MK3OBJ *other)
{
    static const uint32_t init[2] = { 0x00000040u, 0x00004000u };
    uint32_t pair[2];

    pair[0] = init[0];
    pair[1] = init[1];

    if (!stick_look_lr(obj, (uint32_t)(uintptr_t)other,
                       (uint32_t)(uintptr_t)scom_bike, pair))
        return;

    get_his_action(obj);
    if (obj->field20 == 0x61a)
        return;

    if (*(int16_t *)((char *)obj->field00 + 0x7e) != 0) {
        obj->field1c = (uint32_t)(uintptr_t)(G_BYTES + 0x440 + 4);
        get_tsl_px(obj, other);
        if ((long)obj->field20 <= 0x7f)
            return;
    }

    obj->field38 = (uint32_t)(uintptr_t)t_do_bike;
    restricted_xfer(obj, other);
}

/* kano_block_close -- armv7 0x000544a4, 132 bytes.  **Complete.**
 *
 *      q_is_he_a_boss(obj) ; if (obj->field5c != 0) return
 *      distance_from_ground(obj) ; if (obj->field1c <= 0x9f) return
 *      get_x_dist(obj) ; if (obj->field28 > 0x50) return
 *      get_y_dist(obj) ; if (obj->field28 > 0x50) return
 *      *(uint16_t *)((char *)obj->a10 + 0x12) = (uint16_t)obj->field24
 *      obj->field1c = |*(uint32_t *)(G + 0xac) - obj->field24|
 *      if (obj->field1c <= 0xb7) return
 *      get_his_action(obj) ; if (obj->field20 == 0x507) return
 *      obj->field38 = t_do_air_slam
 *      airborn_xfer(obj, other)
 *
 * **It WRITES where jax_block_close reads.** Both work with 0x12 of whatever
 * 0x44 points at and both compare a gap against 0xb7, but jax_block_close
 * takes the y out of the target and this one puts 0x24 INTO it -- and 0x24 is
 * never set here, so the value has to come from the caller.
 *
 * The other difference is the absolute value: this takes |floor - y| where
 * jax_block_close takes the signed difference. Above or below the floor by
 * enough is acceptable here; only above counts there. */
void kano_block_close(MK3OBJ *obj, MK3OBJ *other)
{
    int32_t gap;

    q_is_he_a_boss(obj);
    if (obj->field5c != 0)
        return;

    distance_from_ground(obj);
    if ((long)obj->field1c <= 0x9f)
        return;

    get_x_dist(obj);
    if ((long)obj->field28 > 0x50)
        return;

    get_y_dist(obj);
    if ((long)obj->field28 > 0x50)
        return;

    *(uint16_t *)((char *)(MK3OBJ *)(uintptr_t)obj->a10 + 0x12) =
        (uint16_t)obj->field24;

    gap = (int32_t)(*(uint32_t *)(G_BYTES + 0xac) - obj->field24);
    obj->field1c = (uint32_t)gap;
    if (gap < 0)
        obj->field1c = (uint32_t)(-gap);
    if ((long)obj->field1c <= 0xb7)
        return;

    get_his_action(obj);
    if (obj->field20 == 0x507)
        return;

    obj->field38 = (uint32_t)(uintptr_t)t_do_air_slam;
    airborn_xfer(obj, other);
}


/* -------------------------------------------------------------- stick_look_lr
 *
 * armv7 0x0005369c, 184 bytes.  **Complete.**
 *
 * **The input matcher.** Every osm_, ind_ and _close routine in this file ends up
 * here, and this is what a "stick pattern" actually is: a table, walked
 * backwards through the player's own command queue, with a deadline on the
 * whole sequence.
 *
 *      if (illegal_button_check(obj, pair[0], pair[1]) != 0) -- no
 *      window = table[0]
 *      if (window == 0) -- YES, with nothing checked at all
 *      right = is_he_right(other)
 *      walk = &table[1]
 *      DbgTableDump(walk)
 *      if (table[1] > 5) get_jcq_next_pointer_idx(obj, strength)
 *      else              get_bcq_next_pointer_idx(obj, strength)
 *      if (right == 0) walk += 6
 *      start = *(uint32_t *)(G + 0xa8)
 *      while ((entry = *walk++) != 0) {
 *          code = four_button_switch(obj, entry)
 *          previous_q_entry(obj)
 *          if ((obj->field1c & 0xffff0000) != code << 16) -- no
 *      }
 *      obj->field1c = (uint16_t)obj->field1c
 *      if (start - obj->field1c <= (window * 3) / 2) -- yes
 *      -- no
 *
 * **table[0] is a deadline, not a pattern.** It is read first, and a zero
 * there means yes without looking at anything: the caller wanted the buttons
 * checked and nothing more. Otherwise the whole matched sequence has to have
 * happened within one and a half times that number of ticks -- the `* 3` then
 * `/ 2` is written out as a shift, an add and an arithmetic shift with the
 * round-toward-zero correction the compiler always emits for signed halving.
 *
 * **The table has two halves and which one is used depends on which way the
 * opponent is standing.** `is_he_right` coming back zero skips 0x18 bytes --
 * six words -- so the mirrored sequence sits immediately after the first. That
 * is why every caller's table is bigger than the sequence it seems to hold.
 *
 * **Entries are matched against the HIGH halfword of the queue entry.**
 * four_button_switch turns a table entry into a code, previous_q_entry steps
 * one further back through the queue, and the comparison is
 * `(obj->field1c & 0xffff0000) == code << 16`. So the low half of a queue
 * entry is a timestamp -- which is exactly what the deadline test then reads,
 * after narrowing 0x1c to sixteen bits.
 *
 * The queue is chosen by table[1] before the walk starts: over 5 uses the jump
 * queue, otherwise the button queue, and both are indexed by the strength index
 * pulled out of the OTHER object's proc.
 *
 * pair[0] and pair[1] are handed straight to illegal_button_check as its two
 * per-player masks, which is what the half-shift relationship between them was
 * for all along. */
long stick_look_lr(MK3OBJ *obj, uint32_t other_w, uint32_t table_w,
                   uint32_t *pair)
{
    MK3OBJ   *other = (MK3OBJ *)(uintptr_t)other_w;
    uint32_t *table = (uint32_t *)(uintptr_t)table_w;
    long      strength = (long)other->field00->field08;
    uint32_t *walk;
    long      window, right, code;
    uint32_t  entry, start;

    if (illegal_button_check(obj, pair[0], pair[1]) != 0) {
        obj->field5c = 0;
        return 0;
    }

    window = (long)table[0];
    if (window == 0) {                  /* no sequence at all: just the buttons */
        obj->field5c = 1;
        return 1;
    }

    right = is_he_right(other);
    walk = table + 1;
    DbgTableDump(walk);

    if ((long)table[1] > 5)
        get_jcq_next_pointer_idx(obj, strength);
    else
        get_bcq_next_pointer_idx(obj, strength);

    if (right == 0)
        walk += 6;                      /* the mirrored half */

    start = *(uint32_t *)(G_BYTES + 0xa8);

    for (;;) {
        entry = *walk;
        walk += 1;
        if (entry == 0)
            break;

        code = four_button_switch(obj, (long)entry);
        previous_q_entry(obj);
        if ((obj->field1c & 0xffff0000u) != ((uint32_t)code << 16)) {
            obj->field5c = 0;
            return 0;
        }
    }

    obj->field1c = (uint32_t)(uint16_t)obj->field1c;
    if ((long)(start - obj->field1c) <= (window * 3) / 2) {
        obj->field5c = 1;
        return 1;
    }

    obj->field5c = 0;
    return 0;
}


/* --------------------------------------------------------- secret_move_search
 *
 * armv7 0x00053754, 184 bytes.  **Complete.**
 *
 * The table walker every osm_ and ind_ routine hands its table to. **Each
 * entry is 0x48 bytes and the layout comes out of the arithmetic exactly:**
 *
 *      +0x00  word   pair[0] for stick_look_lr, and the end-of-table sentinel
 *      +0x04  word   pair[1]
 *      +0x08  word   an index into xfer_types_table
 *      +0x0c  word   an extra predicate, or zero -- called as fn(obj)
 *      +0x10  word   a handler, stored into obj->field38
 *      +0x14  13 words  the stick table handed to stick_look_lr
 *
 * **Thirteen words is not a guess: it is 0x48 - 0x14 exactly, and
 * stick_look_lr reads one deadline and then two six-word halves.** The two
 * routines agree to the word, which is what makes the whole layout certain.
 *
 * It also settles osm_hk_close, whose 0x48 looked arbitrary when it was read:
 * that routine starts its search at the SECOND entry of its own table.
 *
 *      for (e = table; e[0] != 0; e += 18) {
 *          pair[0] = e[0] ; pair[1] = e[1]
 *          if (!stick_look_lr(obj, arg, &e[5], pair)) continue
 *          if (e[3] != 0) {
 *              ((void (*)(MK3OBJ *))e[3])(obj)
 *              if (obj->field5c == 0) continue
 *          }
 *          obj->field38 = 0
 *          if (e[4] == 0) return
 *          obj->field38 = e[4]
 *          xfer_types_table[e[2]](obj, arg)
 *          return
 *      }
 *
 * **The extra predicate is how a table entry adds a condition of its own.** It
 * is called with the object and answers through 0x5c, like every other q_
 * routine, and a no simply moves on to the next entry rather than ending the
 * search.
 *
 * **Four dead comparisons.** Before the unconditional store, the handler is
 * compared against t_do_square_wave, t_do_fatality_1, t_do_fatality_2 and
 * t_do_kitana_zap, each with a conditional store of the same value to the same
 * field. The store that follows is unconditional, so all four are redundant --
 * they are in the binary and they change nothing. Transcribed as a note rather
 * than as code, because writing four ifs that do what the next line does
 * anyway would be less honest about which instruction has the effect.
 *
 * A zero handler is not a hit: 0x38 is cleared and the search stops without
 * calling anything. */
long secret_move_search(MK3OBJ *obj, uint32_t arg, uint32_t *table)
{
    uint32_t *e;
    uint32_t pair[2];

    for (e = table; e[0] != 0; e += 0x48 / 4) {
        pair[0] = e[0];
        pair[1] = e[1];

        if (!stick_look_lr(obj, arg, (uint32_t)(uintptr_t)(e + 5), pair))
            continue;

        if (e[3] != 0) {
            ((void (*)(MK3OBJ *))(uintptr_t)e[3])(obj);
            if (obj->field5c == 0)
                continue;
        }

        obj->field38 = 0;
        if (e[4] == 0)
            return 0;

        /* four conditional stores of this same value precede the
         * unconditional one in the binary; see the note above */
        obj->field38 = e[4];
        ((XFER_FN)xfer_types_table[e[2]])(obj, (MK3OBJ *)(uintptr_t)arg);
        return 0;
    }

    return 0;
}


/* smoke_block_close -- armv7 0x000545ec, 140 bytes.  **Complete.**
 *
 * **jax_block_close with a fallback.** The six conditions are the same ones and
 * in the same order -- height off the ground over 0x9f, both distances inside
 * 0x58, the gap between the floor and 0x44's y over 0xb7, not a boss, and his
 * action not 0x507 -- but where jax_block_close simply returns on any failure,
 * this one drops through to a table search.
 *
 * So every refusal is a branch to the same secret_move_search call, and the air
 * slam is only the FIRST thing tried. */
void smoke_block_close(MK3OBJ *obj, MK3OBJ *other)
{
    int32_t gap;

    distance_from_ground(obj);
    if ((long)obj->field1c <= 0x9f)
        goto search;

    get_x_dist(obj);
    if ((long)obj->field28 > 0x58)
        goto search;

    get_y_dist(obj);
    if ((long)obj->field28 > 0x58)
        goto search;

    obj->field24 = (uint32_t)(int32_t)*(int16_t *)
        ((char *)(MK3OBJ *)(uintptr_t)obj->a10 + 0x12);
    gap = (int32_t)(*(uint32_t *)(G_BYTES + 0xac) - obj->field24);
    obj->field1c = (uint32_t)gap;
    if (gap <= 0xb7)
        goto search;

    q_is_he_a_boss(obj);
    if (obj->field5c != 0)
        goto search;

    get_his_action(obj);
    if (obj->field20 == 0x507)
        goto search;

    obj->field38 = (uint32_t)(uintptr_t)t_do_air_slam;
    airborn_xfer(obj, other);
    return;

search:
    secret_move_search(obj, (uint32_t)(uintptr_t)other, sm_smoke_bc);
}

/* sz_hp_close -- armv7 0x00054844, 148 bytes.  **Complete.**
 *
 *      first = other->field00->0x7c ? scom_sky_zap_on_4but : scom_sky_zap_on
 *      if (stick_look_lr2(obj, other, first, 0x10, 0x1000))
 *          obj->field38 = t_do_sky_ice_on
 *      else if (stick_look_lr2(obj, other, scom_sky_zap_behind, 0x10, 0x1000))
 *          obj->field38 = t_do_sky_ice_behind
 *      else if (stick_look_lr2(obj, other, scom_sky_zap_front, 0x10, 0x1000))
 *          obj->field38 = t_do_sky_ice_front
 *      else return
 *      restricted_xfer(obj, other)
 *
 * **Three patterns tried in order, and the four-button gate only changes the
 * first.** All three use the same mask pair, so the buttons are identical and
 * only the stick sequence differs -- on, behind, in front. The first table is
 * swapped for a four-button variant when 0x7c is set, which is the same gate
 * the q_ family uses to pick between four and six button forms.
 *
 * All three successes reach one restricted_xfer, and the mask pair is loaded
 * once into a register that survives all three calls. */
void sz_hp_close(MK3OBJ *obj, MK3OBJ *other)
{
    uint32_t *first;

    if (*(int16_t *)((char *)other->field00 + 0x7c) != 0)
        first = scom_sky_zap_on_4but;
    else
        first = scom_sky_zap_on;

    if (stick_look_lr2(obj, (uint32_t)(uintptr_t)other,
                       (uint32_t)(uintptr_t)first, 0x10u, 0x1000u)) {
        obj->field38 = (uint32_t)(uintptr_t)t_do_sky_ice_on;
    } else if (stick_look_lr2(obj, (uint32_t)(uintptr_t)other,
                              (uint32_t)(uintptr_t)scom_sky_zap_behind,
                              0x10u, 0x1000u)) {
        obj->field38 = (uint32_t)(uintptr_t)t_do_sky_ice_behind;
    } else if (stick_look_lr2(obj, (uint32_t)(uintptr_t)other,
                              (uint32_t)(uintptr_t)scom_sky_zap_front,
                              0x10u, 0x1000u)) {
        obj->field38 = (uint32_t)(uintptr_t)t_do_sky_ice_front;
    } else {
        return;
    }

    restricted_xfer(obj, other);
}


/* ------------------------------------------------------------------ t_do_mercy
 *
 * armv7 0x000525cc, 164 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      is_master_in_finish(obj)
 *      if (obj->field5c == 0) {
 *          frame[frame].handler = t_victory_animation
 *          frame[frame+1].w0 = 0
 *          return 0
 *      }
 *      StartThreadAt(&mytc[2], t_master_proc_mercy)
 *      obj->field1c = 3
 *      *(uint16_t *)(G + 0x45c) = 3
 *      obj->field20 = 1
 *      *(uint16_t *)(G + 0x45a) = 1
 *      *(uint16_t *)(Pp + 0x80)  = 0
 *      *(uint16_t *)(Pp + 0x10c) = 0
 *      frame[frame].handler = t_mercy_start
 *      frame[frame+1].w0 = 0
 *
 * **This is the routine that grants mercy, and it writes exactly the two
 * halfwords the questions read.** G + 0x45c takes 3 -- the value
 * q_fatality_req tests for, and nothing else passes there -- and G + 0x45a
 * takes 1, which is what q_mercy reads and what q_mercy_req and
 * q_mercy_req_ez require to be zero. So offering mercy is what moves the round
 * into the stage where a fatality can be asked for, and it is what stops the
 * mercy questions answering yes a second time.
 *
 * It also clears two halfwords in Pp at 0x80 and 0x10c -- 0x8c apart, which is
 * not the thread stride -- and starts a thread at mytc[2], the same slot
 * is_master_in_finish asks about one instruction earlier.
 *
 * The frame index is scaled with `lsls r3, r1` where r1 still holds the 3
 * written into 0x1c, rather than with an immediate shift. Same result, one
 * fewer constant. */
long t_do_mercy(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    is_master_in_finish(obj);
    if (obj->field5c == 0)
        return mk3_install(thread, (MK3THREADFUNC)t_victory_animation);

    StartThreadAt((MK3THREAD *)((char *)mytc + 2 * 268),
                  (MK3THREADFUNC)t_master_proc_mercy);

    obj->field1c = 3;
    *(uint16_t *)(G_BYTES + 0x45c) = 3;
    obj->field20 = 1;
    *(uint16_t *)(G_BYTES + 0x45a) = 1;

    *(uint16_t *)(Pp + 0x80) = 0;
    *(uint16_t *)(Pp + 0x10c) = 0;

    return mk3_install(thread, (MK3THREADFUNC)t_mercy_start);
}

/* robo2_block_close -- armv7 0x00054714, 172 bytes.  **Complete.**
 *
 *      if (stick_look_lr2(obj, other, scom_robo2_tele, 0x20, 0x2000)) {
 *          obj->field38 = t_do_tele_explode
 *          airborn_xfer(obj, other)
 *          return
 *      }
 *      if (!stick_look_lr2(obj, other, scom_robo_air_grab, 0x20, 0x2000))
 *          return
 *      q_is_he_a_boss(obj) ; if (obj->field5c != 0) return
 *      is_he_airborn(obj)  ; if (obj->field5c == 0) return
 *      him = (MK3OBJ *)obj->field00->him
 *      obj->field1c = him
 *      obj->field20 = him->field1c
 *      if ((int32_t)obj->field20 >= 0) {
 *          obj->field24 = |obj->field00->field00->field00->field40
 *                          - (int16)him->field12|
 *          if (obj->field24 <= 0x2f) return
 *      }
 *      obj->field38 = t_do_robo_air_grab
 *      restricted_xfer(obj, other)
 *
 * **A rising opponent skips the height check entirely.** His vertical speed at
 * 0x1c is tested for sign, and a negative one branches straight past the
 * distance test to the transfer -- so he can be grabbed at any height while he
 * is still going up, and only on the way down does the gap have to exceed 0x2f.
 *
 * The teleport it tries first hands over t_do_tele_explode, which is Cyrax's
 * teleport written in mkprop.c: the two ends of the same move, in two files.
 *
 * The height is measured against proc+0x40 reached through three loads, the
 * same walk get_his_p_hit uses, and taken as a magnitude. */
void robo2_block_close(MK3OBJ *obj, MK3OBJ *other)
{
    MK3OBJ *him;
    int32_t gap;

    if (stick_look_lr2(obj, (uint32_t)(uintptr_t)other,
                       (uint32_t)(uintptr_t)scom_robo2_tele,
                       0x20u, 0x2000u)) {
        obj->field38 = (uint32_t)(uintptr_t)t_do_tele_explode;
        airborn_xfer(obj, other);
        return;
    }

    if (!stick_look_lr2(obj, (uint32_t)(uintptr_t)other,
                        (uint32_t)(uintptr_t)scom_robo_air_grab,
                        0x20u, 0x2000u))
        return;

    q_is_he_a_boss(obj);
    if (obj->field5c != 0)
        return;

    is_he_airborn(obj);
    if (obj->field5c == 0)
        return;

    him = (MK3OBJ *)(uintptr_t)obj->field00->him;
    obj->field1c = (uint32_t)(uintptr_t)him;
    obj->field20 = him->field1c;

    if ((int32_t)obj->field20 >= 0) {           /* on the way down */
        gap = (int32_t)(*(uint32_t *)
                        ((char *)obj->field00->field00->field00 + 0x40)
                        - (uint32_t)(int32_t)
                        *(int16_t *)((char *)him + 0x12));
        obj->field24 = (uint32_t)gap;
        if (gap < 0)
            obj->field24 = (uint32_t)(-gap);
        if ((long)obj->field24 <= 0x2f)
            return;
    }

    obj->field38 = (uint32_t)(uintptr_t)t_do_robo_air_grab;
    restricted_xfer(obj, other);
}


/* --------------------------------------------------------------- t_shang_morph
 *
 * armv7 0x00052670, 268 bytes.  **Complete.**
 *
 *      token == 0:
 *          obj->field20 = 0x401 ; init_special_act(obj)
 *          stop_me_player(obj)
 *          obj->field1c = 4 ; ochar_sound(obj)
 *          arg[f8++] = obj->field40            ; who to become
 *          if (obj->field40 > 0xe) clear_shadow_bit(obj)
 *          token := 0x1145, park 4
 *
 *      token == 0x1145:
 *          obj->field40 = arg[--f8]
 *          obj->field08->field24 = obj->field40
 *          ground_ochar(obj)
 *          obj->field00->field40 = (int16)obj->field08->field12
 *          player_normpal(obj)
 *          obj->field40 = 0 ; get_char_ani(obj)
 *          do_next_a9_frame(obj)
 *          obj->field1c = &G + 0x3fc ; update_tsl(obj)
 *          obj->field1c = obj->field08->field24 + 0x28 ; tsound_func(obj)
 *          if (thread->frame > 0) { frame -= 1; return 0 }
 *          frame[frame].handler = t_local_reaction_exit
 *
 *      otherwise: return -3
 *
 * **The character number crosses the park on the argument stack.**
 * t_do_st_2_kano copies the opponent's number into 0x40 and installs this;
 * state 0 pushes it, four frames later state 0x1145 pops it back and writes it
 * into the part's 0x24 -- which is the field q_simple_shang, qorb3 and
 * q_tusk_blur all read as "which character is that". Shang Tsung becomes him by
 * writing that one word.
 *
 * **Characters above 0xe lose their shadow.** The test is on the number itself,
 * before the push, and clear_shadow_bit is the only thing it changes -- so the
 * later half of the roster is drawn without one.
 *
 * 0x40 is written twice in the second state: the character number first, so the
 * part and the proc can be set from it, then zero before get_char_ani -- which
 * is the same field being used as an animation selector immediately after being
 * used as an identity.
 *
 * The sound is the character number plus 0x28, read back out of the part rather
 * than from the local copy, and it goes to tsound_func in r1 as well as into
 * 0x1c -- the same field-and-register pair four_button_bits uses. */
long t_shang_morph(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field20 = 0x401;
        init_special_act(obj);
        stop_me_player(obj);
        obj->field1c = 4;
        ochar_sound(obj);

        *mk3_arg(thread, thread->fieldf8) = obj->field40;
        thread->fieldf8 += 1;

        if ((long)obj->field40 > 0xe)
            clear_shadow_bit(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x1145;
        thread->fieldfc = 4;
        return 4;
    }

    if (token != 0x1145)
        return -3;

    thread->fieldf8 -= 1;
    obj->field40 = *mk3_arg(thread, thread->fieldf8);
    obj->field08->field24 = obj->field40;

    ground_ochar(obj);
    *(uint32_t *)((char *)obj->field00 + 0x40) = (uint32_t)(int32_t)
        *(int16_t *)((char *)obj->field08 + 0x12);

    player_normpal(obj);
    obj->field40 = 0;
    get_char_ani(obj);
    do_next_a9_frame(obj);

    obj->field1c = (uint32_t)(uintptr_t)(G_BYTES + 0x3fc);
    update_tsl(obj);

    obj->field1c = obj->field08->field24 + 0x28;
    tsound_func(obj, obj->field1c);

    if ((long)thread->frame > 0) {
        thread->frame -= 1;
        return 0;
    }
    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* robo2_hk_close -- armv7 0x000541fc, 268 bytes.  **Complete.**
 *
 *      obj->field1c = 0x00020040 ; obj->field20 = 0x00204000
 *      if (other->field00->0x7c == 0) {
 *          button_bit_check(obj)
 *          if (obj->field5c == 0) return
 *      }
 *      get_his_p_hit(obj) ; if (obj->field1c > 1) return
 *      get_his_action(obj) ; if (obj->field20 == 0x607) return
 *
 *      obj->field54 = t_do_bomb_full
 *      if (other->field00->0x7c != 0)
 *          ok = stick_look_lr2(obj, other, scom_robo_tele, 0x40, 0x4000)
 *      else
 *          ok = stick_look_lr2(obj, other, scom_robo_tele,
 *                              0x00020040, 0x00204000)
 *      if (!ok) {
 *          obj->field54 = t_do_bomb_mid
 *          table = other->field00->0x7c ? scom_bomb_mid_four : scom_bomb_mid
 *          if (!stick_look_lr2(obj, other, table,
 *                              0x00020040, 0x00204000)) return
 *      }
 *      obj->field34 = 0
 *      obj->field38 = obj->field54
 *      obj->field68 = &G + 0x3b8
 *      obj->field64 = other->field00->0x7c ? -1 : 0x10
 *      check_tsl(obj, other)
 *
 * **The four-button gate is consulted four separate times** -- to decide
 * whether the buttons are checked at all, which mask pair the first pattern
 * uses, which of two bomb tables the second uses, and what threshold check_tsl
 * gets. One halfword steering four decisions in one routine.
 *
 * **0x54 carries the handler across the pattern search.** It is set to
 * t_do_bomb_full before the first attempt and overwritten with t_do_bomb_mid
 * before the second, and only once something matched is it copied into 0x38 --
 * so the field that q_fan_lift's neighbours use for a saved word is a
 * scratch pointer here.
 *
 * **A threshold of -1 passes everything.** With the gate set, check_tsl's
 * comparison is against minus one, which any answer beats; with it clear the
 * threshold is a real 0x10. So the gated form still runs the table dispatch but
 * never fails its distance test.
 *
 * The masks are the pair (0x0002, 0x0040) and (0x0020, 0x4000) -- the ninth
 * measurement of the half-shift rule, and the only one where the first word had
 * to be read out of a literal pool because it does not fit an immediate. */
void robo2_hk_close(MK3OBJ *obj, MK3OBJ *other)
{
    long gate;
    uint32_t *table;

    obj->field1c = 0x00020040u;
    obj->field20 = 0x00204000u;

    gate = *(int16_t *)((char *)other->field00 + 0x7c);
    if (gate == 0) {
        button_bit_check(obj);
        if (obj->field5c == 0)
            return;
    }

    get_his_p_hit(obj);
    if ((long)obj->field1c > 1)
        return;

    get_his_action(obj);
    if (obj->field20 == 0x607)
        return;

    obj->field54 = (uint32_t)(uintptr_t)t_do_bomb_full;

    gate = *(int16_t *)((char *)other->field00 + 0x7c);
    if (gate != 0) {
        if (!stick_look_lr2(obj, (uint32_t)(uintptr_t)other,
                            (uint32_t)(uintptr_t)scom_robo_tele,
                            0x4000u - 0x3fc0u, 0x4000u))
            gate = -1;                  /* fall through to the bomb tables */
    } else if (!stick_look_lr2(obj, (uint32_t)(uintptr_t)other,
                               (uint32_t)(uintptr_t)scom_robo_tele,
                               0x00020040u, 0x00204000u)) {
        gate = -1;
    }

    if (gate == -1) {
        obj->field54 = (uint32_t)(uintptr_t)t_do_bomb_mid;
        if (*(int16_t *)((char *)other->field00 + 0x7c) != 0)
            table = scom_bomb_mid_four;
        else
            table = scom_bomb_mid;

        if (!stick_look_lr2(obj, (uint32_t)(uintptr_t)other,
                            (uint32_t)(uintptr_t)table,
                            0x00020040u, 0x00204000u))
            return;
    }

    obj->field34 = 0;
    obj->field38 = obj->field54;
    *(uint32_t *)((char *)obj + 0x68) =
        (uint32_t)(uintptr_t)(G_BYTES + 0x3b8);

    if (*(int16_t *)((char *)other->field00 + 0x7c) != 0)
        *(uint32_t *)((char *)obj + 0x64) = (uint32_t)-1;
    else
        *(uint32_t *)((char *)obj + 0x64) = 0x10;

    check_tsl(obj, other);
}


/* ----------------------------------------------------------------- DoASpecial
 *
 * armv7 0x000517f0, 3328 bytes.  **Complete.**
 *
 * The entry point every special move goes through: given a fighter and a move
 * number, it decides which thread handler to hand the fighter to and which of
 * the four transfer routines does the handing.
 *
 * **It asks its questions of a throwaway copy on the stack.** `sub sp, #0x6c`
 * reserves 108 bytes, `copy.field00` and `copy.field08` are filled from the real
 * object, and the q_ predicates below are called with `&copy`. Their answers
 * come back in `copy.field5c`, so the real object's scratch fields are never
 * disturbed and nothing in the dispatcher has to save and restore 0x1c. The
 * chosen handler goes into `copy.field38` and the copy is what the transfer
 * routine receives.
 *
 * **There are three dispatchers, not one.**
 *
 *      which - 0xd <= 6 and RoundParam[14] != 0   ->  seven finisher arms,
 *                                                     gated, drone handlers
 *      which - 0xd <= 6 and RoundParam[14] == 0   ->  seven finisher arms,
 *                                                     ungated, own handlers
 *      anything else                              ->  23 arms, one per
 *                                                     character
 *
 * The first is a `tbh` on `which - 0xd`, the second a `tbb` on the same value,
 * the third a `tbh` on the character number at `part->field24`. A character
 * above 0x16 falls out at 0x51abc with no handler and returns.
 *
 * **RoundParam[14] is the finishing window, and Blood.c settles it.** Its event
 * 17/18 -- the FINISH HIM/HER prompt -- sets `IsInFinishing = 1` and
 * `RoundParam[14] = 1` together. So the two finisher dispatchers are the same
 * seven requests in two modes, and their handler names line up one for one:
 *
 *      which   window open                     window shut
 *      0xd     t_d_background_fatal  fatality  t_do_pit_fatality
 *      0xe     t_drone_mercy         mercy     t_do_mercy
 *      0xf     t_drone_do_fatality1  fatality  t_do_fatality_1
 *      0x10    t_drone_do_fatality2  fatality  t_do_fatality_2
 *      0x11    t_drone_animality     animality t_do_animality
 *      0x12    t_drone_babality      mercy     t_do_baby
 *      0x13    t_drone_friendship    mercy     t_do_friendship
 *
 * The middle column is the transfer routine; the window-shut column uses
 * free_xfer throughout. **Only the window-open path writes proc+0x80 = 1** --
 * the halfword that gates every special move -- and only it asks a predicate.
 *
 * **Four of the seven are gated, and which predicate guards which request is
 * worth reading twice**, because two of them look wrong and are not:
 *
 *      0xd   q_pit_fatal_ez     0x11  q_mercy
 *      0xe   q_mercy_req_ez     0x12  q_friend_ez
 *                               0x13  q_friend_ez
 *
 * 0x11 is the animality, and it is guarded by q_mercy -- an animality needs a
 * mercy first, so the predicate names the requirement and not the move. 0x12 is
 * the babality and shares q_friend_ez with the friendship, which is the
 * no-punches-thrown condition the two have in common. 0xf and 0x10, the two
 * fatalities, ask nothing at all.
 *
 * **Three characters refuse some of the seven outright**, tested before the
 * table is reached and each written as a branchless predicate the compiler built
 * out of `it` blocks:
 *
 *      character 0x16   refuses 0x10, 0x11, 0x13
 *      character 0x14   refuses 0x11, 0x13
 *      character 0x15   refuses 0x10 through 0x13
 *
 * Passing the gate branches back to 0x51830, which re-tests `which - 0xd <= 6`
 * before the `tbh`. That test cannot fail on the way in, so the edge to 0x51ce4
 * is dead; 0x51ce4 returns with no handler anyway.
 *
 * **The 23 character arms are each a switch on `which`.** Eleven are inline
 * jump tables -- `adr r3, #4` or `addw r3, pc, #N`, then `add r3, r3, r1, lsl
 * #2`, then `mov pc, r3`, over an array of `b.w` -- and twelve are two or three
 * `cmp`/`beq` pairs. Both forms return when `which` is not one the character
 * has. Every arm uses free_xfer.
 *
 * **`which` 4 and a second higher number give the same move for three of
 * them** -- character 2 has t_do_jax_zap2 at 4 and 8, character 5 has
 * t_do_swat_bomb_lo at 4 and 8, character 8 has t_do_air_slam at 4 and 9 -- and
 * for three others the high number is a different move, so the doubling is per
 * character and not a rule.
 *
 * **Characters 0x12 and 0x16 have the same two moves**, t_do_scorpion_spear at
 * 0 and t_do_scorp_tele at 2, and share the code that loads them.
 *
 * **One override sits in the common tail: character 6 asking for
 * t_do_lia_forward gets a height test instead.** `(int16)part[0x12]` -- the
 * signed halfword in the high half of 0x10 -- is compared against the word at
 * proc+0x40, and below it the handler becomes t_do_lia_anglez, at or above it
 * stays t_do_lia_forward. Character 6's own `which == 0` arm jumps into the
 * middle of that test past both of the guard's comparisons; written here as
 * setting the handler the guard looks for, which reaches the same code with the
 * same values. No other path leaves that pair on the way in, so the guard is
 * only ever exercised through that jump.
 *
 * proc+0x40 is read here with `ldr`, a full word, which is the third routine to
 * do so against a field the header declares as a halfword -- reached by offset
 * for that reason, as tl_do_lao_tele and tl_do_robo_tele are.
 *
 * The tail itself is four lines: a zero handler returns having done nothing,
 * otherwise the handler goes into the copy's 0x38 and the transfer routine is
 * called with the copy and the real object.
 *
 * All four transfer routines are called through the one register, and the call
 * site discards what comes back -- but two of the four return `long` and two
 * return nothing, so no single C function-pointer type fits them. The two that
 * return a value get a shim here that drops it, which is what `blx sb` with an
 * unused result does. */
typedef void (*MK3XFER)(MK3OBJ *copy, MK3OBJ *obj);

static void mercy_xfer_void(MK3OBJ *copy, MK3OBJ *obj)
{
    (void)mercy_xfer(copy, obj);
}

static void fatality_xfer_void(MK3OBJ *copy, MK3OBJ *obj)
{
    (void)fatality_xfer(copy, obj);
}

void DoASpecial(MK3OBJ *obj, uint32_t which)
{
    MK3OBJ         copy;
    MK3OBJPROC    *proc = obj->field00;
    MK3OBJ        *part = obj->field08;
    MK3THREADFUNC  handler = 0;
    MK3XFER        xfer = free_xfer;
    uint32_t       ch;

    copy.field00 = proc;
    copy.field08 = part;

    if (which - 0xd <= 6 && RoundParam[14] != 0) {
        ch = part->field24;

        /* the three characters that refuse part of the set */
        if (ch == 0x16) {
            if (which == 0x10 || which == 0x11 || which == 0x13)
                return;
        } else if (ch == 0x14) {
            if (which == 0x11 || which == 0x13)
                return;
        } else if (ch == 0x15) {
            if (which - 0x10 <= 3)
                return;
        }

        switch (which) {
        case 0xd:
            q_pit_fatal_ez(&copy);
            if (copy.field5c == 0)
                return;
            handler = (MK3THREADFUNC)t_d_background_fatal;
            xfer    = fatality_xfer_void;
            break;

        case 0xe:
            q_mercy_req_ez(&copy);
            if (copy.field5c == 0)
                return;
            handler = (MK3THREADFUNC)t_drone_mercy;
            xfer    = mercy_xfer_void;
            break;

        case 0xf:
            handler = (MK3THREADFUNC)t_drone_do_fatality1;
            xfer    = fatality_xfer_void;
            break;

        case 0x10:
            handler = (MK3THREADFUNC)t_drone_do_fatality2;
            xfer    = fatality_xfer_void;
            break;

        case 0x11:
            q_mercy(&copy);
            if (copy.field5c == 0)
                return;
            handler = (MK3THREADFUNC)t_drone_animality;
            xfer    = animality_xfer;
            break;

        case 0x12:
            q_friend_ez(&copy);
            if (copy.field5c == 0)
                return;
            handler = (MK3THREADFUNC)t_drone_babality;
            xfer    = mercy_xfer_void;
            break;

        default:                                /* 0x13 */
            q_friend_ez(&copy);
            if (copy.field5c == 0)
                return;
            handler = (MK3THREADFUNC)t_drone_friendship;
            xfer    = mercy_xfer_void;
            break;
        }

        *(uint16_t *)((char *)proc + 0x80) = 1;
        ch = part->field24;

    } else if (which - 0xd <= 6) {
        switch (which) {
        case 0xd:  handler = (MK3THREADFUNC)t_do_pit_fatality; break;
        case 0xe:  handler = (MK3THREADFUNC)t_do_mercy;        break;
        case 0xf:  handler = (MK3THREADFUNC)t_do_fatality_1;   break;
        case 0x10: handler = (MK3THREADFUNC)t_do_fatality_2;   break;
        case 0x11: handler = (MK3THREADFUNC)t_do_animality;    break;
        case 0x12: handler = (MK3THREADFUNC)t_do_baby;         break;
        default:   handler = (MK3THREADFUNC)t_do_friendship;   break;
        }
        ch = part->field24;

    } else {
        ch = part->field24;

        switch (ch) {
        case 0x00:
            switch (which) {
            case 0: handler = (MK3THREADFUNC)t_do_kano_zap;     break;
            case 1: handler = (MK3THREADFUNC)t_do_kano_roll;    break;
            case 2: handler = (MK3THREADFUNC)t_do_kano_upball;  break;
            case 3: handler = (MK3THREADFUNC)t_do_kano_swipe;   break;
            case 5: handler = (MK3THREADFUNC)t_do_shake;        break;
            default: return;
            }
            break;

        case 0x01:
            switch (which) {
            case 0: handler = (MK3THREADFUNC)t_do_sonya_zap;    break;
            case 1: handler = (MK3THREADFUNC)tl_do_square_wave; break;
            case 2: handler = (MK3THREADFUNC)t_do_leg_throw;    break;
            case 3: handler = (MK3THREADFUNC)t_do_bike;         break;
            default: return;
            }
            break;

        case 0x02:
            switch (which) {
            case 0: handler = (MK3THREADFUNC)t_do_noogy;        break;
            case 1: handler = (MK3THREADFUNC)t_do_jax_zap1;     break;
            case 2: handler = (MK3THREADFUNC)t_jax_dash_punch;  break;
            case 3: handler = (MK3THREADFUNC)t_do_quake;        break;
            case 4:
            case 8: handler = (MK3THREADFUNC)t_do_jax_zap2;     break;
            default: return;
            }
            break;

        case 0x03:
            switch (which) {
            case 0: handler = (MK3THREADFUNC)t_do_ind_zap;      break;
            case 1: handler = (MK3THREADFUNC)t_do_ind_charge;   break;
            case 2: handler = (MK3THREADFUNC)t_do_reflect;      break;
            case 3: handler = (MK3THREADFUNC)t_do_axe_up;       break;
            default: return;
            }
            break;

        case 0x04:
            switch (which) {
            case 0: handler = (MK3THREADFUNC)t_do_sz_forward_zap; break;
            case 1: handler = (MK3THREADFUNC)t_do_sky_ice_on;     break;
            case 2: handler = (MK3THREADFUNC)t_do_sz_decoy;       break;
            case 3: handler = (MK3THREADFUNC)t_do_slide;          break;
            case 4: handler = (MK3THREADFUNC)t_do_sky_ice_behind; break;
            case 8: handler = (MK3THREADFUNC)t_do_sky_ice_front;  break;
            default: return;
            }
            break;

        case 0x05:
            switch (which) {
            case 0: handler = (MK3THREADFUNC)t_do_swat_zoom;     break;
            case 1: handler = (MK3THREADFUNC)t_do_swat_bomb_hi;  break;
            case 2: handler = (MK3THREADFUNC)t_do_swat_gun;      break;
            case 3: handler = (MK3THREADFUNC)t_do_stick_sweep;   break;
            case 4:
            case 8: handler = (MK3THREADFUNC)t_do_swat_bomb_lo;  break;
            default: return;
            }
            break;

        case 0x06:
            switch (which) {
            /* the height test in the tail decides this one */
            case 0: handler = (MK3THREADFUNC)t_do_lia_forward;  break;
            case 1: handler = (MK3THREADFUNC)t_do_lia_scream;   break;
            case 3: handler = (MK3THREADFUNC)t_do_lia_fly;      break;
            default: return;
            }
            break;

        case 0x07:
            switch (which) {
            case 0: handler = (MK3THREADFUNC)t_do_robo_zap;     break;
            case 2: handler = (MK3THREADFUNC)t_do_robo_zap2;    break;
            case 3: handler = (MK3THREADFUNC)t_do_robo_tele;    break;
            default: return;
            }
            break;

        case 0x08:
            switch (which) {
            case 0: handler = (MK3THREADFUNC)t_do_robo_net;     break;
            case 1: handler = (MK3THREADFUNC)t_do_bomb_full;    break;
            case 2: handler = (MK3THREADFUNC)t_do_bomb_mid;     break;
            case 3: handler = (MK3THREADFUNC)t_do_tele_explode; break;
            case 4:
            case 9: handler = (MK3THREADFUNC)t_do_air_slam;     break;
            default: return;
            }
            break;

        case 0x09:
            switch (which) {
            case 0: handler = (MK3THREADFUNC)t_do_lao_zap;        break;
            case 1: handler = (MK3THREADFUNC)t_do_lao_spin;       break;
            case 2: handler = (MK3THREADFUNC)t_do_lao_tele;       break;
            case 3: handler = (MK3THREADFUNC)t_do_lao_angle_kick; break;
            default: return;
            }
            break;

        case 0x0a:
            switch (which) {
            case 0: handler = (MK3THREADFUNC)t_do_tusk_zap;     break;
            case 1: handler = (MK3THREADFUNC)t_do_tusk_blur;    break;
            case 3: handler = (MK3THREADFUNC)t_do_floor_blade;  break;
            default: return;
            }
            break;

        case 0x0b:
            switch (which) {
            case 0: handler = (MK3THREADFUNC)t_do_sg_zap;       break;
            case 2: handler = (MK3THREADFUNC)t_do_sg_pounce;    break;
            case 3: handler = (MK3THREADFUNC)t_do_sg_quake;     break;
            default: return;
            }
            break;

        case 0x0c:
            switch (which) {
            case 0: handler = (MK3THREADFUNC)t_do_st_zap1;      break;
            case 1: handler = (MK3THREADFUNC)t_do_st_zap3;      break;
            case 2: handler = (MK3THREADFUNC)t_do_st_zap2;      break;
            case 3: handler = (MK3THREADFUNC)t_do_summon;       break;
            default: return;
            }
            break;

        case 0x0d:
            switch (which) {
            case 0: handler = (MK3THREADFUNC)t_lk_zap_hi;       break;
            case 1: handler = (MK3THREADFUNC)t_lk_bike_kick;    break;
            case 2: handler = (MK3THREADFUNC)t_do_super_kang;   break;
            case 3: handler = (MK3THREADFUNC)t_lk_zap_lo;       break;
            default: return;
            }
            break;

        case 0x0e:
            switch (which) {
            case 0: handler = (MK3THREADFUNC)t_do_smoke_spear;  break;
            case 2: handler = (MK3THREADFUNC)t_do_smoke_tele;   break;
            case 3: handler = (MK3THREADFUNC)t_do_inviso;       break;
            default: return;
            }
            break;

        case 0x0f:
            switch (which) {
            case 0: handler = (MK3THREADFUNC)t_do_kitana_zap;   break;
            case 1: handler = (MK3THREADFUNC)t_do_fan_lift;     break;
            case 2: handler = (MK3THREADFUNC)t_do_square_wave;  break;
            default: return;
            }
            break;

        case 0x10:
            switch (which) {
            case 0: handler = (MK3THREADFUNC)t_do_jade_prop;    break;
            case 1: handler = (MK3THREADFUNC)t_do_jade_zap_med; break;
            case 2: handler = (MK3THREADFUNC)t_do_jade_flash;   break;
            case 3: handler = (MK3THREADFUNC)t_do_jade_zap_lo;  break;
            case 7: handler = (MK3THREADFUNC)t_do_jade_zap_hi;  break;
            case 8: handler = (MK3THREADFUNC)t_do_jade_zap_ret; break;
            default: return;
            }
            break;

        case 0x11:
            switch (which) {
            case 0: handler = (MK3THREADFUNC)t_do_mileena_zap;  break;
            case 1: handler = (MK3THREADFUNC)t_do_mileena_roll; break;
            case 3: handler = (MK3THREADFUNC)t_do_mileena_tele; break;
            default: return;
            }
            break;

        case 0x12:
        case 0x16:
            switch (which) {
            case 0: handler = (MK3THREADFUNC)t_do_scorpion_spear; break;
            case 2: handler = (MK3THREADFUNC)t_do_scorp_tele;     break;
            default: return;
            }
            break;

        case 0x13:
            switch (which) {
            case 0: handler = (MK3THREADFUNC)t_do_spit;             break;
            case 1: handler = (MK3THREADFUNC)t_do_reptile_orb;      break;
            case 2: handler = (MK3THREADFUNC)t_do_reptile_orb_fast; break;
            case 3: handler = (MK3THREADFUNC)t_do_ninja_slide;      break;
            case 4: handler = (MK3THREADFUNC)t_do_reptile_dash;     break;
            case 8: handler = (MK3THREADFUNC)t_do_reptile_inv;      break;
            default: return;
            }
            break;

        case 0x14:
            switch (which) {
            case 0: handler = (MK3THREADFUNC)t_do_ermac_zap;    break;
            case 2: handler = (MK3THREADFUNC)t_do_ermac_tele;   break;
            case 3: handler = (MK3THREADFUNC)t_do_ermac_slam;   break;
            default: return;
            }
            break;

        case 0x15:
            switch (which) {
            case 0: handler = (MK3THREADFUNC)t_do_osz_zap;      break;
            case 1: handler = (MK3THREADFUNC)t_do_floor_ice;    break;
            case 3: handler = (MK3THREADFUNC)t_do_ninja_slide;  break;
            default: return;
            }
            break;

        default:                                /* character above 0x16 */
            return;
        }
    }

    /* the one override, and the tail */
    if (ch == 6 && handler == (MK3THREADFUNC)t_do_lia_forward) {
        if ((int32_t)(int16_t)MK3_FIELD12(part)
            < (int32_t)*(uint32_t *)((char *)proc + 0x40))
            handler = (MK3THREADFUNC)t_do_lia_anglez;
        else
            handler = (MK3THREADFUNC)t_do_lia_forward;
    }

    if (handler == 0)
        return;

    copy.field38 = (uint32_t)(uintptr_t)handler;
    xfer(&copy, obj);
}
