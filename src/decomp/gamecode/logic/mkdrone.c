/*
 * mkdrone.c -- gamecode/logic/mkdrone.c, decompiled.
 *
 * The AI: what the computer opponent decides to do, and the thread it becomes
 * to do it. *
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

long t_d_slam(struct MK3THREAD *thread);
long t_swait_nonattack_jump(struct MK3THREAD *thread);
long t_return_to_beware(struct MK3THREAD *thread);
long t_diff_no_zap(struct MK3THREAD *thread);

long c_air_fan(struct MK3THREAD *thread);
long c_bike(struct MK3THREAD *thread);
long c_duck_kickh(struct MK3THREAD *thread);
long c_duck_kickl(struct MK3THREAD *thread);
long c_elbow(struct MK3THREAD *thread);
long c_fast_orb(struct MK3THREAD *thread);
long c_flypunch(struct MK3THREAD *thread);
long c_ind_charge(struct MK3THREAD *thread);
long c_jax_dash(struct MK3THREAD *thread);
long c_juppunch(struct MK3THREAD *thread);
long c_kano_roll(struct MK3THREAD *thread);
long c_kroll_sd(struct MK3THREAD *thread);
long c_kswipe_sd(struct MK3THREAD *thread);
long c_lia_scream(struct MK3THREAD *thread);
long c_mileena_tele(struct MK3THREAD *thread);
long c_proj_sd(struct MK3THREAD *thread);
long c_sbike(struct MK3THREAD *thread);
long c_sbike_sd(struct MK3THREAD *thread);
long c_screamed(struct MK3THREAD *thread);
long c_st_zap3(struct MK3THREAD *thread);
long c_superkang(struct MK3THREAD *thread);
long c_tusk_blur(struct MK3THREAD *thread);
long c_tusk_zap_air(struct MK3THREAD *thread);
long c_zoom_sd(struct MK3THREAD *thread);
long ckik3(struct MK3THREAD *thread);
long cpch3(struct MK3THREAD *thread);
long funcs_11119(struct MK3THREAD *thread);
long funcs_11165(struct MK3THREAD *thread);
long funcs_11195(struct MK3THREAD *thread);
long funcs_11229(struct MK3THREAD *thread);
long funcs_11243(struct MK3THREAD *thread);
long funcs_11298(struct MK3THREAD *thread);
long funcs_11346(struct MK3THREAD *thread);
long funcs_11380(struct MK3THREAD *thread);
long funcs_11414(struct MK3THREAD *thread);
long funcs_11449(struct MK3THREAD *thread);
long funcs_11528(struct MK3THREAD *thread);
long funcs_11623(struct MK3THREAD *thread);
long funcs_12230(struct MK3THREAD *thread);
long funcs_12891(struct MK3THREAD *thread);
long funcs_12990(struct MK3THREAD *thread);
long funcs_13044(struct MK3THREAD *thread);
long funcs_13084(struct MK3THREAD *thread);
long funcs_13115(struct MK3THREAD *thread);
long funcs_13142(struct MK3THREAD *thread);
long funcs_13191(struct MK3THREAD *thread);
long funcs_13283(struct MK3THREAD *thread);
long funcs_13351(struct MK3THREAD *thread);
long funcs_13378(struct MK3THREAD *thread);
long funcs_13464(struct MK3THREAD *thread);
long funcs_13578(struct MK3THREAD *thread);
long funcs_14131(struct MK3THREAD *thread);
long funcs_14174(struct MK3THREAD *thread);
long funcs_14207(struct MK3THREAD *thread);
long funcs_14271(struct MK3THREAD *thread);
void q_is_proj_gone(MK3OBJ *obj);
long t_cornered_attack(struct MK3THREAD *thread);
long t_counter_grounded_sd(struct MK3THREAD *thread);
long t_crossover_scan(struct MK3THREAD *thread);
long t_d_block(struct MK3THREAD *thread);
long t_d_body_propell(struct MK3THREAD *thread);
long t_d_crossover_kick(struct MK3THREAD *thread);
long t_d_fatality_abort(struct MK3THREAD *thread);
long t_d_fflip_jump(struct MK3THREAD *thread);
long t_d_fflip_scan_jump(struct MK3THREAD *thread);
long t_d_hi_kick(struct MK3THREAD *thread);
long t_d_jump_up_kick(struct MK3THREAD *thread);
long t_d_stalk_a11(struct MK3THREAD *thread);
long t_d_sweep_kick(struct MK3THREAD *thread);
long t_d_uppercut(struct MK3THREAD *thread);
long t_d_zap_jump(struct MK3THREAD *thread);
long t_diff_no_propell(struct MK3THREAD *thread);
long t_fflip_scan(struct MK3THREAD *thread);
long t_if_u_can(struct MK3THREAD *thread);
long t_jade_anti_zap(struct MK3THREAD *thread);
long t_lk_jump_up_zap(struct MK3THREAD *thread);
long t_react_jump_table_act(struct MK3THREAD *thread);
long t_run_in_close(struct MK3THREAD *thread);
long t_stance_wait_no(struct MK3THREAD *thread);
long t_tusk_jump_up_zap(struct MK3THREAD *thread);

/* t_d_stalk_a11_ntl -- armv7 0x0006777c, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->a10 = 0x7d00
 *      frame[frame].handler = t_d_stalk_a11
 *      frame[frame+1].w0 = 0
 */

long t_d_stalk_a11_ntl(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->a10 = 0x7d00;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_d_stalk_a11);
}

/* t_far_airborn -- armv7 0x00067d70, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = t_d_fflip_jump
 *      frame[frame+1].w0 = 0
 */

long t_far_airborn(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_d_fflip_jump);
}

/* t_very_close_airborn -- armv7 0x00067e28, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = t_d_jump_up_kick
 *      frame[frame+1].w0 = 0
 */

long t_very_close_airborn(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_d_jump_up_kick);
}

/* t_d_cornered -- armv7 0x00067e5c, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = t_cornered_attack
 *      frame[frame+1].w0 = 0
 */

long t_d_cornered(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_cornered_attack);
}

/* t_diff_no_zap -- armv7 0x00067e90, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = t_diff_no_propell
 *      frame[frame+1].w0 = 0
 */

long t_diff_no_zap(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_diff_no_propell);
}

/* t_stw_proj_proc -- armv7 0x0006847c, 68 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field48 = q_is_proj_gone
 *      obj->a10 = 0x50
 *      frame[frame].handler = t_stance_wait_no
 *      frame[frame+1].w0 = 0
 */

long t_stw_proj_proc(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field48 = (uint32_t)(uintptr_t)q_is_proj_gone;
    obj->a10 = 0x50;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_stance_wait_no);
}

/* t_d_crossover_kick -- armv7 0x000686c4, 64 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field34 = t_crossover_scan
 *      frame[frame].handler = t_d_fflip_scan_jump
 *      frame[frame+1].w0 = 0
 */

long t_d_crossover_kick(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field34 = (uint32_t)(uintptr_t)t_crossover_scan;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_d_fflip_scan_jump);
}

/* t_d_fflip_kick_jsrp -- armv7 0x00068704, 64 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field34 = t_fflip_scan
 *      frame[frame].handler = t_d_fflip_scan_jump
 *      frame[frame+1].w0 = 0
 */

long t_d_fflip_kick_jsrp(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field34 = (uint32_t)(uintptr_t)t_fflip_scan;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_d_fflip_scan_jump);
}

/* t_nr_sweep_if_u_can -- armv7 0x00068c44, 68 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x4
 *      obj->a10 = t_d_sweep_kick
 *      frame[frame].handler = t_if_u_can
 *      frame[frame+1].w0 = 0
 */

long t_nr_sweep_if_u_can(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x4;
    obj->a10 = (uint32_t)(uintptr_t)t_d_sweep_kick;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_if_u_can);
}

/* t_nr_uppercut_if_u_can -- armv7 0x00068c88, 68 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x8
 *      obj->a10 = t_d_uppercut
 *      frame[frame].handler = t_if_u_can
 *      frame[frame+1].w0 = 0
 */

long t_nr_uppercut_if_u_can(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x8;
    obj->a10 = (uint32_t)(uintptr_t)t_d_uppercut;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_if_u_can);
}

/* t_d_fatality_cornered -- armv7 0x000695c4, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = t_d_fatality_abort
 *      frame[frame+1].w0 = 0
 */

long t_d_fatality_cornered(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_d_fatality_abort);
}

/* t_kitana_jump_up_zap -- armv7 0x00069944, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = t_lk_jump_up_zap
 *      frame[frame+1].w0 = 0
 */

long t_kitana_jump_up_zap(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_lk_jump_up_zap);
}

/* t_lk_jump_up_zap -- armv7 0x00069978, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = t_tusk_jump_up_zap
 *      frame[frame+1].w0 = 0
 */

long t_lk_jump_up_zap(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_tusk_jump_up_zap);
}

/* c_reptile_dash -- armv7 0x0006a09c, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = c_tusk_blur
 *      frame[frame+1].w0 = 0
 */

long c_reptile_dash(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)c_tusk_blur);
}

/* c_bomb -- armv7 0x0006a0d0, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = t_run_in_close
 *      frame[frame+1].w0 = 0
 */

long c_bomb(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_run_in_close);
}

/* c_robo_bomb -- armv7 0x0006a104, 64 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      *(uint32_t *)((char *)obj + 0x68) = funcs.11119
 *      frame[frame].handler = t_react_jump_table_act
 *      frame[frame+1].w0 = 0
 */

long c_robo_bomb(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    *(uint32_t *)((char *)obj + 0x68) = (uint32_t)(uintptr_t)funcs_11119;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_react_jump_table_act);
}

/* c_st_zap2 -- armv7 0x0006a1ac, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = c_st_zap3
 *      frame[frame+1].w0 = 0
 */

long c_st_zap2(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)c_st_zap3);
}

/* c_st_zap3 -- armv7 0x0006a1e0, 64 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      *(uint32_t *)((char *)obj + 0x68) = funcs.11165
 *      frame[frame].handler = t_react_jump_table_act
 *      frame[frame+1].w0 = 0
 */

long c_st_zap3(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    *(uint32_t *)((char *)obj + 0x68) = (uint32_t)(uintptr_t)funcs_11165;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_react_jump_table_act);
}

/* c_lk_zap_lo -- armv7 0x0006a220, 64 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      *(uint32_t *)((char *)obj + 0x68) = funcs.11195
 *      frame[frame].handler = t_react_jump_table_act
 *      frame[frame+1].w0 = 0
 */

long c_lk_zap_lo(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    *(uint32_t *)((char *)obj + 0x68) = (uint32_t)(uintptr_t)funcs_11195;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_react_jump_table_act);
}

/* c_lao_zap -- armv7 0x0006a260, 64 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      *(uint32_t *)((char *)obj + 0x68) = funcs.11229
 *      frame[frame].handler = t_react_jump_table_act
 *      frame[frame+1].w0 = 0
 */

long c_lao_zap(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    *(uint32_t *)((char *)obj + 0x68) = (uint32_t)(uintptr_t)funcs_11229;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_react_jump_table_act);
}

/* c_robo_net -- armv7 0x0006a2a0, 64 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      *(uint32_t *)((char *)obj + 0x68) = funcs.11243
 *      frame[frame].handler = t_react_jump_table_act
 *      frame[frame+1].w0 = 0
 */

long c_robo_net(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    *(uint32_t *)((char *)obj + 0x68) = (uint32_t)(uintptr_t)funcs_11243;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_react_jump_table_act);
}

/* c_robo_zap2 -- armv7 0x0006a2e0, 64 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      *(uint32_t *)((char *)obj + 0x68) = funcs.11298
 *      frame[frame].handler = t_react_jump_table_act
 *      frame[frame+1].w0 = 0
 */

long c_robo_zap2(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    *(uint32_t *)((char *)obj + 0x68) = (uint32_t)(uintptr_t)funcs_11298;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_react_jump_table_act);
}

/* c_lia_anglez -- armv7 0x0006a320, 64 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      *(uint32_t *)((char *)obj + 0x68) = funcs.11346
 *      frame[frame].handler = t_react_jump_table_act
 *      frame[frame+1].w0 = 0
 */

long c_lia_anglez(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    *(uint32_t *)((char *)obj + 0x68) = (uint32_t)(uintptr_t)funcs_11346;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_react_jump_table_act);
}

/* c_swat_bomb_lo -- armv7 0x0006a360, 64 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      *(uint32_t *)((char *)obj + 0x68) = funcs.11380
 *      frame[frame].handler = t_react_jump_table_act
 *      frame[frame+1].w0 = 0
 */

long c_swat_bomb_lo(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    *(uint32_t *)((char *)obj + 0x68) = (uint32_t)(uintptr_t)funcs_11380;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_react_jump_table_act);
}

/* c_swat_bomb_hi -- armv7 0x0006a3a0, 64 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      *(uint32_t *)((char *)obj + 0x68) = funcs.11414
 *      frame[frame].handler = t_react_jump_table_act
 *      frame[frame+1].w0 = 0
 */

long c_swat_bomb_hi(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    *(uint32_t *)((char *)obj + 0x68) = (uint32_t)(uintptr_t)funcs_11414;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_react_jump_table_act);
}

/* c_sky_ice -- armv7 0x0006a490, 64 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      *(uint32_t *)((char *)obj + 0x68) = funcs.11449
 *      frame[frame].handler = t_react_jump_table_act
 *      frame[frame+1].w0 = 0
 */

long c_sky_ice(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    *(uint32_t *)((char *)obj + 0x68) = (uint32_t)(uintptr_t)funcs_11449;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_react_jump_table_act);
}

/* c_jax_zap2 -- armv7 0x0006a4d0, 64 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      *(uint32_t *)((char *)obj + 0x68) = funcs.11528
 *      frame[frame].handler = t_react_jump_table_act
 *      frame[frame+1].w0 = 0
 */

long c_jax_zap2(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    *(uint32_t *)((char *)obj + 0x68) = (uint32_t)(uintptr_t)funcs_11528;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_react_jump_table_act);
}

/* c_kano_zap -- armv7 0x0006a510, 64 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      *(uint32_t *)((char *)obj + 0x68) = funcs.11623
 *      frame[frame].handler = t_react_jump_table_act
 *      frame[frame+1].w0 = 0
 */

long c_kano_zap(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    *(uint32_t *)((char *)obj + 0x68) = (uint32_t)(uintptr_t)funcs_11623;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_react_jump_table_act);
}

/* t_lk_zap_low -- armv7 0x0006a658, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x17
 *      frame[frame].handler = t_d_zap_jump
 *      frame[frame+1].w0 = 0
 */

long t_lk_zap_low(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x17;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_d_zap_jump);
}

/* t_jade_anti_orb -- armv7 0x0006a810, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = t_jade_anti_zap
 *      frame[frame+1].w0 = 0
 */

long t_jade_anti_orb(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_jade_anti_zap);
}

/* c_reptile_orb -- armv7 0x0006a8c4, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = c_fast_orb
 *      frame[frame+1].w0 = 0
 */

long c_reptile_orb(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)c_fast_orb);
}

/* c_mil_air_zap -- armv7 0x0006a9f4, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = c_air_fan
 *      frame[frame+1].w0 = 0
 */

long c_mil_air_zap(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)c_air_fan);
}

/* c_lk_zap_air -- armv7 0x0006aa90, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = c_tusk_zap_air
 *      frame[frame+1].w0 = 0
 */

long c_lk_zap_air(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)c_tusk_zap_air);
}

/* c_floor_zap -- armv7 0x0006ab74, 64 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      *(uint32_t *)((char *)obj + 0x68) = funcs.12230
 *      frame[frame].handler = t_react_jump_table_act
 *      frame[frame+1].w0 = 0
 */

long c_floor_zap(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    *(uint32_t *)((char *)obj + 0x68) = (uint32_t)(uintptr_t)funcs_12230;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_react_jump_table_act);
}

/* c_slam_bounce -- armv7 0x0006abb4, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = t_d_hi_kick
 *      frame[frame+1].w0 = 0
 */

long c_slam_bounce(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_d_hi_kick);
}

/* c_upball_sd -- armv7 0x0006ad1c, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = c_sbike_sd
 *      frame[frame+1].w0 = 0
 */

long c_upball_sd(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)c_sbike_sd);
}

/* c_speared -- armv7 0x0006ad50, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = c_screamed
 *      frame[frame+1].w0 = 0
 */

long c_speared(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)c_screamed);
}

/* c_swat_gun_sd -- armv7 0x0006ae40, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = c_proj_sd
 *      frame[frame+1].w0 = 0
 */

long c_swat_gun_sd(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)c_proj_sd);
}

/* t_d_bike_kick -- armv7 0x0006afcc, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0xd
 *      frame[frame].handler = t_d_body_propell
 *      frame[frame+1].w0 = 0
 */

long t_d_bike_kick(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0xd;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_d_body_propell);
}

/* c_lk_bike_sd -- armv7 0x0006b0c0, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = c_zoom_sd
 *      frame[frame+1].w0 = 0
 */

long c_lk_bike_sd(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)c_zoom_sd);
}

/* c_zoom_sd -- armv7 0x0006b0f4, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = c_kroll_sd
 *      frame[frame+1].w0 = 0
 */

long c_zoom_sd(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)c_kroll_sd);
}

/* c_leg_sd -- armv7 0x0006b190, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = c_kswipe_sd
 *      frame[frame+1].w0 = 0
 */

long c_leg_sd(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)c_kswipe_sd);
}

/* c_kswipe_sd -- armv7 0x0006b1c4, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = t_counter_grounded_sd
 *      frame[frame+1].w0 = 0
 */

long c_kswipe_sd(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_counter_grounded_sd);
}

/* c_sg_pounce -- armv7 0x0006b2c8, 64 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      *(uint32_t *)((char *)obj + 0x68) = funcs.12891
 *      frame[frame].handler = t_react_jump_table_act
 *      frame[frame+1].w0 = 0
 */

long c_sg_pounce(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    *(uint32_t *)((char *)obj + 0x68) = (uint32_t)(uintptr_t)funcs_12891;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_react_jump_table_act);
}

/* c_kano_upball -- armv7 0x0006b390, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = c_sbike
 *      frame[frame+1].w0 = 0
 */

long c_kano_upball(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)c_sbike);
}

/* c_sbike -- armv7 0x0006b3c4, 64 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      *(uint32_t *)((char *)obj + 0x68) = funcs.12990
 *      frame[frame].handler = t_react_jump_table_act
 *      frame[frame+1].w0 = 0
 */

long c_sbike(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    *(uint32_t *)((char *)obj + 0x68) = (uint32_t)(uintptr_t)funcs_12990;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_react_jump_table_act);
}

/* c_lao_angle -- armv7 0x0006b44c, 64 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      *(uint32_t *)((char *)obj + 0x68) = funcs.13044
 *      frame[frame].handler = t_react_jump_table_act
 *      frame[frame+1].w0 = 0
 */

long c_lao_angle(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    *(uint32_t *)((char *)obj + 0x68) = (uint32_t)(uintptr_t)funcs_13044;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_react_jump_table_act);
}

/* c_tele_explode -- armv7 0x0006b48c, 64 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      *(uint32_t *)((char *)obj + 0x68) = funcs.13084
 *      frame[frame].handler = t_react_jump_table_act
 *      frame[frame+1].w0 = 0
 */

long c_tele_explode(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    *(uint32_t *)((char *)obj + 0x68) = (uint32_t)(uintptr_t)funcs_13084;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_react_jump_table_act);
}

/* c_robo_tele -- armv7 0x0006b544, 64 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      *(uint32_t *)((char *)obj + 0x68) = funcs.13115
 *      frame[frame].handler = t_react_jump_table_act
 *      frame[frame+1].w0 = 0
 */

long c_robo_tele(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    *(uint32_t *)((char *)obj + 0x68) = (uint32_t)(uintptr_t)funcs_13115;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_react_jump_table_act);
}

/* t_av_scorp_tele -- armv7 0x0006b584, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = t_d_block
 *      frame[frame+1].w0 = 0
 */

long t_av_scorp_tele(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_d_block);
}

/* c_scorp_tele -- armv7 0x0006b5b8, 64 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      *(uint32_t *)((char *)obj + 0x68) = funcs.13142
 *      frame[frame].handler = t_react_jump_table_act
 *      frame[frame+1].w0 = 0
 */

long c_scorp_tele(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    *(uint32_t *)((char *)obj + 0x68) = (uint32_t)(uintptr_t)funcs_13142;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_react_jump_table_act);
}

/* c_square -- armv7 0x0006b5f8, 64 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      *(uint32_t *)((char *)obj + 0x68) = funcs.13191
 *      frame[frame].handler = t_react_jump_table_act
 *      frame[frame+1].w0 = 0
 */

long c_square(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    *(uint32_t *)((char *)obj + 0x68) = (uint32_t)(uintptr_t)funcs_13191;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_react_jump_table_act);
}

/* c_mileena_roll -- armv7 0x0006b6bc, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = c_mileena_tele
 *      frame[frame+1].w0 = 0
 */

long c_mileena_roll(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)c_mileena_tele);
}

/* c_mileena_tele -- armv7 0x0006b6f0, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = c_bike
 *      frame[frame+1].w0 = 0
 */

long c_mileena_tele(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)c_bike);
}

/* c_bike -- armv7 0x0006b724, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = c_superkang
 *      frame[frame+1].w0 = 0
 */

long c_bike(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)c_superkang);
}

/* c_superkang -- armv7 0x0006b758, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = c_kano_roll
 *      frame[frame+1].w0 = 0
 */

long c_superkang(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)c_kano_roll);
}

/* c_kano_roll -- armv7 0x0006b78c, 64 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      *(uint32_t *)((char *)obj + 0x68) = funcs.13283
 *      frame[frame].handler = t_react_jump_table_act
 *      frame[frame+1].w0 = 0
 */

long c_kano_roll(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    *(uint32_t *)((char *)obj + 0x68) = (uint32_t)(uintptr_t)funcs_13283;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_react_jump_table_act);
}

/* c_jade_prop -- armv7 0x0006b8b0, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = c_ind_charge
 *      frame[frame+1].w0 = 0
 */

long c_jade_prop(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)c_ind_charge);
}

/* c_ind_charge -- armv7 0x0006b8e4, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = c_jax_dash
 *      frame[frame+1].w0 = 0
 */

long c_ind_charge(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)c_jax_dash);
}

/* c_jax_dash -- armv7 0x0006b918, 64 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      *(uint32_t *)((char *)obj + 0x68) = funcs.13351
 *      frame[frame].handler = t_react_jump_table_act
 *      frame[frame+1].w0 = 0
 */

long c_jax_dash(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    *(uint32_t *)((char *)obj + 0x68) = (uint32_t)(uintptr_t)funcs_13351;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_react_jump_table_act);
}

/* t_ct_zoom -- armv7 0x0006b958, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = t_d_block
 *      frame[frame+1].w0 = 0
 */

long t_ct_zoom(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_d_block);
}

/* c_zoom -- armv7 0x0006b98c, 64 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      *(uint32_t *)((char *)obj + 0x68) = funcs.13378
 *      frame[frame].handler = t_react_jump_table_act
 *      frame[frame+1].w0 = 0
 */

long c_zoom(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    *(uint32_t *)((char *)obj + 0x68) = (uint32_t)(uintptr_t)funcs_13378;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_react_jump_table_act);
}

/* c_flykick -- armv7 0x0006b9cc, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = c_flypunch
 *      frame[frame+1].w0 = 0
 */

long c_flykick(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)c_flypunch);
}

/* c_flypunch -- armv7 0x0006ba00, 64 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      *(uint32_t *)((char *)obj + 0x68) = funcs.13464
 *      frame[frame].handler = t_react_jump_table_act
 *      frame[frame+1].w0 = 0
 */

long c_flypunch(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    *(uint32_t *)((char *)obj + 0x68) = (uint32_t)(uintptr_t)funcs_13464;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_react_jump_table_act);
}

/* c_swat_stick -- armv7 0x0006bac4, 64 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      *(uint32_t *)((char *)obj + 0x68) = funcs.13578
 *      frame[frame].handler = t_react_jump_table_act
 *      frame[frame+1].w0 = 0
 */

long c_swat_stick(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    *(uint32_t *)((char *)obj + 0x68) = (uint32_t)(uintptr_t)funcs_13578;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_react_jump_table_act);
}

/* c_jupkick -- armv7 0x0006bbf4, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = c_juppunch
 *      frame[frame+1].w0 = 0
 */

long c_jupkick(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)c_juppunch);
}

/* c_duckpunch -- armv7 0x0006bc28, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = c_duck_kickh
 *      frame[frame+1].w0 = 0
 */

long c_duckpunch(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)c_duck_kickh);
}

/* c_duck_kickh -- armv7 0x0006bc5c, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = c_duck_kickl
 *      frame[frame+1].w0 = 0
 */

long c_duck_kickh(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)c_duck_kickl);
}

/* c_knee -- armv7 0x0006bc90, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = c_elbow
 *      frame[frame+1].w0 = 0
 */

long c_knee(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)c_elbow);
}

/* t_ct_sweep -- armv7 0x0006bd40, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = t_d_crossover_kick
 *      frame[frame+1].w0 = 0
 */

long t_ct_sweep(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_d_crossover_kick);
}

/* c_fan_lift -- armv7 0x0006bd74, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = c_lia_scream
 *      frame[frame+1].w0 = 0
 */

long c_fan_lift(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)c_lia_scream);
}

/* c_lopunch -- armv7 0x0006bda8, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field20 = 0x102
 *      frame[frame].handler = cpch3
 *      frame[frame+1].w0 = 0
 */

long c_lopunch(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field20 = 0x102;

    return mk3_push_handler(thread, (MK3THREADFUNC)cpch3);
}

/* c_hipunch -- armv7 0x0006bde4, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field20 = 0x101
 *      frame[frame].handler = cpch3
 *      frame[frame+1].w0 = 0
 */

long c_hipunch(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field20 = 0x101;

    return mk3_push_handler(thread, (MK3THREADFUNC)cpch3);
}

/* c_lokick -- armv7 0x0006be20, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field20 = 0x104
 *      frame[frame].handler = ckik3
 *      frame[frame+1].w0 = 0
 */

long c_lokick(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field20 = 0x104;

    return mk3_push_handler(thread, (MK3THREADFUNC)ckik3);
}

/* c_hikick -- armv7 0x0006bec4, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field20 = 0x103
 *      frame[frame].handler = ckik3
 *      frame[frame+1].w0 = 0
 */

long c_hikick(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field20 = 0x103;

    return mk3_push_handler(thread, (MK3THREADFUNC)ckik3);
}

/* c_noogy -- armv7 0x0006bf00, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field20 = 0x112
 *      frame[frame].handler = ckik3
 *      frame[frame+1].w0 = 0
 */

long c_noogy(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field20 = 0x112;

    return mk3_push_handler(thread, (MK3THREADFUNC)ckik3);
}

/* c_shake -- armv7 0x0006bf3c, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field20 = 0x111
 *      frame[frame].handler = ckik3
 *      frame[frame+1].w0 = 0
 */

long c_shake(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field20 = 0x111;

    return mk3_push_handler(thread, (MK3THREADFUNC)ckik3);
}

/* c_quake -- armv7 0x0006bfe0, 64 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      *(uint32_t *)((char *)obj + 0x68) = funcs.14131
 *      frame[frame].handler = t_react_jump_table_act
 *      frame[frame+1].w0 = 0
 */

long c_quake(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    *(uint32_t *)((char *)obj + 0x68) = (uint32_t)(uintptr_t)funcs_14131;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_react_jump_table_act);
}

/* c_laospin -- armv7 0x0006c020, 64 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      *(uint32_t *)((char *)obj + 0x68) = funcs.14174
 *      frame[frame].handler = t_react_jump_table_act
 *      frame[frame+1].w0 = 0
 */

long c_laospin(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    *(uint32_t *)((char *)obj + 0x68) = (uint32_t)(uintptr_t)funcs_14174;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_react_jump_table_act);
}

/* c_kano_swipe -- armv7 0x0006c060, 64 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      *(uint32_t *)((char *)obj + 0x68) = funcs.14207
 *      frame[frame].handler = t_react_jump_table_act
 *      frame[frame+1].w0 = 0
 */

long c_kano_swipe(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    *(uint32_t *)((char *)obj + 0x68) = (uint32_t)(uintptr_t)funcs_14207;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_react_jump_table_act);
}

/* c_leg_grab -- armv7 0x0006c108, 64 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      *(uint32_t *)((char *)obj + 0x68) = funcs.14271
 *      frame[frame].handler = t_react_jump_table_act
 *      frame[frame+1].w0 = 0
 */

long c_leg_grab(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    *(uint32_t *)((char *)obj + 0x68) = (uint32_t)(uintptr_t)funcs_14271;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_react_jump_table_act);
}


/* vq_no -- armv7 0x00067514, 8 bytes.  **Complete.**
 *
 *      obj->field5c = 0
 */
void vq_no(MK3OBJ *obj)
{
    obj->field5c = 0;
}

/* vq_yes -- armv7 0x0006751c, 8 bytes.  **Complete.**
 *
 *      obj->field5c = 1
 */
void vq_yes(MK3OBJ *obj)
{
    obj->field5c = 1;
}

/* t_d_leg_grab -- armv7 0x0006af20, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x9
 *      frame[frame].handler = t_do_stationary
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f3154 rather than as a
 * link-time constant, so it lives in another translation unit. */
long t_do_stationary(struct MK3THREAD *thread);

long t_d_leg_grab(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x9;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_do_stationary);
}




/* --------------------------------------------------------------------
 * What the readers could prove. See tools/pushfn.py, which executes
 * a body symbolically, and tools/microfn.py, which matches whole
 * bodies against templates. Both refuse anything they cannot account
 * for instruction by instruction.
 * -------------------------------------------------------------------- */

long t_d_bflip_scan_jsrp(struct MK3THREAD *thread);
long t_d_fflip_scan_jsrp(struct MK3THREAD *thread);
long t_d_jumpup(struct MK3THREAD *thread);

/* t_d_jumpup_nocall -- armv7 0x00068164, 56 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field48 = 0   (the register the guard proved)
 *      frame[frame].handler = t_d_jumpup
 *      frame[frame+1].w0 = 0
 */

long t_d_jumpup_nocall(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field48 = 0;   /* the guard proved this register */

    return mk3_push_handler(thread, (MK3THREADFUNC)t_d_jumpup);
}

/* t_d_bflip_noscan_jsrp -- armv7 0x00068208, 56 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field34 = 0   (the register the guard proved)
 *      frame[frame].handler = t_d_bflip_scan_jsrp
 *      frame[frame+1].w0 = 0
 */

long t_d_bflip_noscan_jsrp(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field34 = 0;   /* the guard proved this register */

    return mk3_push_handler(thread, (MK3THREADFUNC)t_d_bflip_scan_jsrp);
}

/* t_d_fflip_noscan_jsrp -- armv7 0x000682a8, 56 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field34 = 0   (the register the guard proved)
 *      frame[frame].handler = t_d_fflip_scan_jsrp
 *      frame[frame+1].w0 = 0
 */

long t_d_fflip_noscan_jsrp(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field34 = 0;   /* the guard proved this register */

    return mk3_push_handler(thread, (MK3THREADFUNC)t_d_fflip_scan_jsrp);
}

/* t_nr_hikick_if_u_can -- armv7 0x00068c00, 68 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0   (the register the guard proved)
 *      obj->a10 = t_d_hi_kick
 *      frame[frame].handler = t_if_u_can
 *      frame[frame+1].w0 = 0
 */

long t_nr_hikick_if_u_can(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0;   /* the guard proved this register */
    obj->a10 = (uint32_t)(uintptr_t)t_d_hi_kick;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_if_u_can);
}





/* --------------------------------------------------------------------
 * What the readers could prove. See tools/pushfn.py, which executes
 * a body symbolically, and tools/microfn.py, which matches whole
 * bodies against templates. Both refuse anything they cannot account
 * for instruction by instruction.
 * -------------------------------------------------------------------- */

long is_he_airborn(MK3OBJ *obj);

/* t_swait_land_jsrp -- armv7 0x0006b404, 72 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field48 = is_he_airborn
 *      obj->a10 = 0x40
 *      frame[frame].handler = t_stance_wait_no
 *      frame[frame+1].w0 = 0
 */

long t_swait_land_jsrp(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field48 = (uint32_t)(uintptr_t)is_he_airborn;
    obj->a10 = 0x40;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_stance_wait_no);
}





/* --------------------------------------------------------------------
 * What the readers could prove. See tools/pushfn.py, which executes
 * a body symbolically, and tools/microfn.py, which matches whole
 * bodies against templates. Both refuse anything they cannot account
 * for instruction by instruction.
 * -------------------------------------------------------------------- */

long t_d_punch(struct MK3THREAD *thread);

/* t_d_rapid_lo -- armv7 0x000687ac, 72 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x7
 *      obj->field20 = 0x2
 *      obj->field24 = 0x3
 *      obj->field40 = 0xf
 *      frame[frame].handler = t_d_punch
 *      frame[frame+1].w0 = 0
 */

long t_d_rapid_lo(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x7;
    obj->field20 = 0x2;
    obj->field24 = 0x3;
    obj->field40 = 0xf;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_d_punch);
}

/* t_d_rapid_hi -- armv7 0x000687f4, 68 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x7
 *      obj->field20 = 0x2
 *      obj->field24 = 0x2
 *      obj->field40 = 0xe
 *      frame[frame].handler = t_d_punch
 *      frame[frame+1].w0 = 0
 */

long t_d_rapid_hi(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x7;
    obj->field20 = 0x2;
    obj->field24 = 0x2;
    obj->field40 = 0xe;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_d_punch);
}





/* --------------------------------------------------------------------
 * What the readers could prove. See tools/pushfn.py, which executes
 * a body symbolically, and tools/microfn.py, which matches whole
 * bodies against templates. Both refuse anything they cannot account
 * for instruction by instruction.
 * -------------------------------------------------------------------- */

long funcs_13619(struct MK3THREAD *thread);
long funcs_14239(struct MK3THREAD *thread);
long rpt_cornered(struct MK3THREAD *thread);
long rpt_elbow_knee(struct MK3THREAD *thread);
long t_d_bflip_jump(struct MK3THREAD *thread);
long t_d_fflip_kick_jump(struct MK3THREAD *thread);
long t_d_zap_now(struct MK3THREAD *thread);
long t_drone_proc(struct MK3THREAD *thread);
long t_react_jump_table(struct MK3THREAD *thread);
long t_run_in_close_now(struct MK3THREAD *thread);
long t_stalk_in_close(struct MK3THREAD *thread);
long ask_mr_diff(MK3OBJ *obj);
void get_his_action(MK3OBJ *obj);
/* Read from the binary in joy.c: nothing is deliberately left in r0.
 * The `long` this used to be declared as came from a call site that
 * ignored the result. */
void get_x_dist(MK3OBJ *obj);
long is_throwing_allowed(MK3OBJ *obj);
void is_towards_me(MK3OBJ *obj);
void ochar_begin_calls(MK3OBJ *obj);
void q_am_i_cornered(MK3OBJ *obj);
long q_will_he_reach_me(MK3OBJ *obj);

/* t_d_zap -- armv7 0x00067f90, 80 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      is_throwing_allowed(obj)
 *      frame[frame].handler = t_d_zap_now
 *      frame[frame+1].w0 = 0
 */

long t_d_zap(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    is_throwing_allowed(obj);

    if (obj->field5c == 0)
        return mk3_install(thread, (MK3THREADFUNC)t_diff_no_zap);
    return mk3_install(thread, (MK3THREADFUNC)t_d_zap_now);
}

/* t_react_jump_table_act -- armv7 0x0006c5ac, 64 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      get_his_action(obj)
 *      frame[frame].handler = t_react_jump_table
 *      frame[frame+1].w0 = 0
 */

long t_react_jump_table_act(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    get_his_action(obj);

    return mk3_push_handler(thread, (MK3THREADFUNC)t_react_jump_table);
}

/* t_cornered_attack -- armv7 0x0006cd3c, 112 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = rpt_cornered
 *      ask_mr_diff(obj)
 *      frame[frame].handler = t_stalk_in_close
 *      frame[frame+1].w0 = 0
 */

long t_run_in_close(struct MK3THREAD *thread);

long t_cornered_attack(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = (uint32_t)(uintptr_t)rpt_cornered;
    ask_mr_diff(obj);

    /* Corrected, 2026-09-26: the answer picks one of two routines; this
     * was first written as an unconditional install of the second. */
    if (obj->field5c != 0)
        return mk3_install(thread, (MK3THREADFUNC)t_run_in_close);

    return mk3_install(thread, (MK3THREADFUNC)t_stalk_in_close);
}

/* t_d_avoid_elbow_knee -- armv7 0x0006d4c0, 96 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = rpt_elbow_knee
 *      ask_mr_diff(obj)
 *      get_x_dist(obj)
 *      frame[frame].handler = t_d_block
 *      frame[frame+1].w0 = 0
 */

long t_d_avoid_elbow_knee(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = (uint32_t)(uintptr_t)rpt_elbow_knee;
    ask_mr_diff(obj);
    if (obj->field5c == 0)
        return mk3_install(thread, (MK3THREADFUNC)t_d_block);

    get_x_dist(obj);
    return mk3_install(thread, (MK3THREADFUNC)t_d_slam);
}

/* c_floor_blade -- armv7 0x0006e044, 64 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      get_x_dist(obj)
 *      frame[frame].handler = t_run_in_close_now
 *      frame[frame+1].w0 = 0
 */

long c_floor_blade(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    get_x_dist(obj);

    return mk3_push_handler(thread, (MK3THREADFUNC)t_run_in_close_now);
}

/* t_ct_leg -- armv7 0x0006e9e8, 80 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      q_will_he_reach_me(obj)
 *      frame[frame].handler = t_d_block
 *      frame[frame+1].w0 = 0
 */

long t_ct_leg(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    q_will_he_reach_me(obj);
    if (obj->field5c == 0)
        return mk3_install(thread, (MK3THREADFUNC)t_swait_nonattack_jump);
    return mk3_install(thread, (MK3THREADFUNC)t_d_block);
}

/* c_axe_up -- armv7 0x0006ea38, 92 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      q_will_he_reach_me(obj)
 *      *(uint32_t *)((char *)obj + 0x68) = funcs.14239
 *      frame[frame].handler = t_react_jump_table_act
 *      frame[frame+1].w0 = 0
 */

long c_axe_up(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    q_will_he_reach_me(obj);
    if (obj->field5c == 0)
        return mk3_install(thread, (MK3THREADFUNC)t_return_to_beware);

    *(uint32_t *)((char *)obj + 0x68) = (uint32_t)(uintptr_t)funcs_14239;
    return mk3_install(thread, (MK3THREADFUNC)t_react_jump_table_act);
}

/* c_uppercut -- armv7 0x0006ebc4, 92 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      q_will_he_reach_me(obj)
 *      *(uint32_t *)((char *)obj + 0x68) = funcs.13619
 *      frame[frame].handler = t_react_jump_table_act
 *      frame[frame+1].w0 = 0
 */

long c_uppercut(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    q_will_he_reach_me(obj);
    if (obj->field5c == 0)
        return mk3_install(thread, (MK3THREADFUNC)t_return_to_beware);

    *(uint32_t *)((char *)obj + 0x68) = (uint32_t)(uintptr_t)funcs_13619;
    return mk3_install(thread, (MK3THREADFUNC)t_react_jump_table_act);
}

/* t_close_airborn -- armv7 0x00070b8c, 104 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      is_towards_me(obj)
 *      frame[frame].handler = t_d_fflip_kick_jump
 *      frame[frame+1].w0 = 0
 */

long t_d_jump_up_kick(struct MK3THREAD *thread);

long t_close_airborn(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    is_towards_me(obj);

    /* Corrected, 2026-09-26: the answer picks one of two routines; this
     * was first written as an unconditional install of the second. */
    if (obj->field5c != 0)
        return mk3_install(thread, (MK3THREADFUNC)t_d_jump_up_kick);

    return mk3_install(thread, (MK3THREADFUNC)t_d_fflip_kick_jump);
}

/* t_av_sweep -- armv7 0x00070f70, 104 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      q_am_i_cornered(obj)
 *      frame[frame].handler = t_d_bflip_jump
 *      frame[frame+1].w0 = 0
 */

long t_d_duck_block(struct MK3THREAD *thread);

long t_av_sweep(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    q_am_i_cornered(obj);

    /* Corrected, 2026-09-26: the answer picks one of two routines; this
     * was first written as an unconditional install of the second. */
    if (obj->field5c != 0)
        return mk3_install(thread, (MK3THREADFUNC)t_d_duck_block);

    return mk3_install(thread, (MK3THREADFUNC)t_d_bflip_jump);
}

/* t_sq_quake_abort -- armv7 0x0007113c, 104 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      q_am_i_cornered(obj)
 *      frame[frame].handler = t_d_bflip_jump
 *      frame[frame+1].w0 = 0
 */

long t_d_flip_punch_jump(struct MK3THREAD *thread);

long t_sq_quake_abort(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    q_am_i_cornered(obj);

    /* Corrected, 2026-09-26: the answer picks one of two routines; this
     * was first written as an unconditional install of the second. */
    if (obj->field5c != 0)
        return mk3_install(thread, (MK3THREADFUNC)t_d_flip_punch_jump);

    return mk3_install(thread, (MK3THREADFUNC)t_d_bflip_jump);
}

/* t_drone_begin -- armv7 0x000721e4, 64 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      ochar_begin_calls(obj)
 *      frame[frame].handler = t_drone_proc
 *      frame[frame+1].w0 = 0
 */

long t_drone_begin(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    ochar_begin_calls(obj);

    return mk3_push_handler(thread, (MK3THREADFUNC)t_drone_proc);
}

/* --------------------------------------------------------------------
 * Straight-line leaves, read by tools/leaffn.py: stores, calls and
 * a return, with every instruction accounted for. It refuses
 * anything that branches, any return value it cannot prove, and any
 * value read from a field the function also writes -- that is a
 * saved value being put back, not a re-read.
 * -------------------------------------------------------------------- */

void beh1(MK3OBJ *obj);
void bossck(MK3OBJ *obj, MK3OBJ * arg);
void dwset3(MK3OBJ *obj);
void get_char_ani(MK3OBJ *obj);
void get_my_dfe(MK3OBJ *obj);
void get_walk_info_b(MK3OBJ *obj);
void get_walk_info_f(MK3OBJ *obj);
void init_anirate(MK3OBJ *obj);
void q_is_he_lower(MK3OBJ *obj);
void set_x_vel_player(MK3OBJ *obj);

/* q_am_i_a_boss -- armv7 0x00068e14, 12 bytes.  **Complete.**
 *
 *      bossck(obj, obj->field08)
 */
void q_am_i_a_boss(MK3OBJ *obj)
{
    bossck(obj, obj->field08);
}


/* q_square_lower -- armv7 0x00068e64, 16 bytes.  **Complete.**
 *
 *      obj->field38 = 0x50
 *      q_is_he_lower(obj)
 */
void q_square_lower(MK3OBJ *obj)
{
    obj->field38 = 0x50;
    q_is_he_lower(obj);
}


/* d_behind_me_a5 -- armv7 0x00070f44, 20 bytes.  **Complete.**
 *
 *      get_my_dfe(obj)
 *      beh1(obj)
 */
void d_behind_me_a5(MK3OBJ *obj)
{
    get_my_dfe(obj);
    beh1(obj);
}


/* dwset3 -- armv7 0x000724a4, 32 bytes.  **Complete.**
 *
 *      obj->field40 = obj->field24
 *      init_anirate(obj)
 *      obj->field1c = obj->field20
 *      set_x_vel_player(obj)
 *      get_char_ani(obj)
 */
void dwset3(MK3OBJ *obj)
{
    obj->field40 = obj->field24;
    init_anirate(obj);
    obj->field1c = obj->field20;
    set_x_vel_player(obj);
    get_char_ani(obj);
}


/* d_walkb_setup -- armv7 0x000724c4, 20 bytes.  **Complete.**
 *
 *      get_walk_info_b(obj)
 *      dwset3(obj)
 */
void d_walkb_setup(MK3OBJ *obj)
{
    get_walk_info_b(obj);
    dwset3(obj);
}


/* d_walkf_setup -- armv7 0x00072928, 20 bytes.  **Complete.**
 *
 *      get_walk_info_f(obj)
 *      dwset3(obj)
 */
void d_walkf_setup(MK3OBJ *obj)
{
    get_walk_info_f(obj);
    dwset3(obj);
}

/* --------------------------------------------------------------------
 * Added by a later sweep -- tools/sweep.py, running the same
 * readers again after one of them learned something. Each still
 * refuses anything it cannot account for instruction by
 * instruction; see tools/pushfn.py and tools/leaffn.py.
 * -------------------------------------------------------------------- */

long c_tusk_blur_sd(MK3THREAD *thread);
long t_d_backup_jsrp(MK3THREAD *thread);
long t_d_beware_mframew(MK3THREAD *thread);
long t_d_bflip_jsrp(MK3THREAD *thread);
long t_d_fflip_jsrp(MK3THREAD *thread);
long t_d_knee(MK3THREAD *thread);
long t_d_run_a11(MK3THREAD *thread);
long t_d_slam(MK3THREAD *thread);
long t_d_stance_pause(MK3THREAD *thread);
long t_do_flip_kick(MK3THREAD *thread);
long t_do_flip_punch(MK3THREAD *thread);
long t_do_mercy(MK3THREAD *thread);
long t_do_quake(MK3THREAD *thread);
long t_do_zap(MK3THREAD *thread);
long t_dont_zap_teles(MK3THREAD *thread);
long t_drone_rfp(MK3THREAD *thread);
long t_duck_under_mproj(MK3THREAD *thread);
long t_fatality_align(MK3THREAD *thread);
long t_local_reaction_exit(MK3THREAD *thread);
long t_nr_attack_sd(MK3THREAD *thread);
long t_return_to_beware(MK3THREAD *thread);
long t_stat_do_hi_kick(MK3THREAD *thread);
long t_willy_go_round(MK3THREAD *thread);
long tl_stat_do_lia_scream(MK3THREAD *thread);

/* t_d_hi_kick -- armv7 0x0006770c, 112 bytes.  **Complete.**
 *
 *      token == 0:
 *          token := 0x189, then descend into t_stat_do_hi_kick
 *      token == 0x189:
 *          frame[frame].handler = t_local_reaction_exit
 *      otherwise:  return -3
 */
long t_d_hi_kick(MK3THREAD *thread)
{
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x189;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_stat_do_hi_kick;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x189)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

/* t_d_backup_jump -- armv7 0x000678e8, 104 bytes.  **Complete.**
 *
 *      token == 0:
 *          token := 0x269, then descend into t_d_backup_jsrp
 *      token == 0x269:
 *          frame[frame].handler = t_local_reaction_exit
 *      otherwise:  return -3
 */
long t_d_backup_jump(MK3THREAD *thread)
{
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x269;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_d_backup_jsrp;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x269)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

/* t_very_far_airborn -- armv7 0x00067d08, 104 bytes.  **Complete.**
 *
 *      token == 0:
 *          token := 0x3c6, then descend into t_dont_zap_teles
 *      token == 0x3c6:
 *          frame[frame].handler = t_d_zap
 *      otherwise:  return -3
 */
long t_very_far_airborn(MK3THREAD *thread)
{
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x3c6;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_dont_zap_teles;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x3c6)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_d_zap);
}

/* t_d_do_floor_ice -- armv7 0x0006807c, 128 bytes.  **Complete.**
 *
 *      token == 0:
 *          obj->field1c = 0x2b
 *          token := 0x53b, then descend into t_do_zap
 *      token == 0x53b:
 *          frame[frame].handler = t_local_reaction_exit
 *      otherwise:  return -3
 */
long t_d_do_floor_ice(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field1c = 0x2b;
        *mk3_frame(thread, thread->frame + 1) = 0x53b;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_do_zap;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x53b)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

/* t_d_bflip_jump -- armv7 0x000680fc, 104 bytes.  **Complete.**
 *
 *      token == 0:
 *          token := 0x54e, then descend into t_d_bflip_jsrp
 *      token == 0x54e:
 *          frame[frame].handler = t_local_reaction_exit
 *      otherwise:  return -3
 */
long t_d_bflip_jump(MK3THREAD *thread)
{
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x54e;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_d_bflip_jsrp;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x54e)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

/* t_d_fflip_scan_jump -- armv7 0x00068240, 104 bytes.  **Complete.**
 *
 *      token == 0:
 *          token := 0x56b, then descend into t_d_fflip_scan_jsrp
 *      token == 0x56b:
 *          frame[frame].handler = t_local_reaction_exit
 *      otherwise:  return -3
 */
long t_d_fflip_scan_jump(MK3THREAD *thread)
{
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x56b;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_d_fflip_scan_jsrp;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x56b)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

/* t_watch_flip_punch -- armv7 0x00068334, 112 bytes.  **Complete.**
 *
 *      token == 0:
 *          token := 0x57e, then descend into t_do_flip_punch
 *      token == 0x57e:
 *          frame[frame].handler = t_local_reaction_exit
 *      otherwise:  return -3
 */
long t_watch_flip_punch(MK3THREAD *thread)
{
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x57e;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_do_flip_punch;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x57e)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

/* t_watch_flip_kick -- armv7 0x000683a4, 112 bytes.  **Complete.**
 *
 *      token == 0:
 *          token := 0x583, then descend into t_do_flip_kick
 *      token == 0x583:
 *          frame[frame].handler = t_local_reaction_exit
 *      otherwise:  return -3
 */
long t_watch_flip_kick(MK3THREAD *thread)
{
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x583;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_do_flip_kick;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x583)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

/* t_d_fflip_jump -- armv7 0x00068414, 104 bytes.  **Complete.**
 *
 *      token == 0:
 *          token := 0x5a6, then descend into t_d_fflip_jsrp
 *      token == 0x5a6:
 *          frame[frame].handler = t_local_reaction_exit
 *      otherwise:  return -3
 */
long t_d_fflip_jump(MK3THREAD *thread)
{
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x5a6;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_d_fflip_jsrp;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x5a6)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

/* t_d_punch -- armv7 0x00068838, 104 bytes.  **Complete.**
 *
 *      token == 0:
 *          token := 0x6de, then descend into t_drone_rfp
 *      token == 0x6de:
 *          frame[frame].handler = t_local_reaction_exit
 *      otherwise:  return -3
 */
long t_d_punch(MK3THREAD *thread)
{
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x6de;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_drone_rfp;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x6de)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

/* t_drone_mercy -- armv7 0x00069364, 124 bytes.  **Complete.**
 *
 *      token == 0:
 *          obj->field1c = 0xa0
 *          token := 0xa1f, then descend into t_fatality_align
 *      token == 0xa1f:
 *          frame[frame].handler = t_do_mercy
 *      otherwise:  return -3
 */
long t_drone_mercy(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field1c = 0xa0;
        *mk3_frame(thread, thread->frame + 1) = 0xa1f;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_fatality_align;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0xa1f)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_do_mercy);
}

/* t_do_fast_orb -- armv7 0x00069708, 128 bytes.  **Complete.**
 *
 *      token == 0:
 *          obj->field1c = 0x28
 *          token := 0xc49, then descend into t_do_zap
 *      token == 0xc49:
 *          frame[frame].handler = t_local_reaction_exit
 *      otherwise:  return -3
 */
long t_do_fast_orb(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field1c = 0x28;
        *mk3_frame(thread, thread->frame + 1) = 0xc49;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_do_zap;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0xc49)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

/* t_d_lk_hi_zap_jump -- armv7 0x00069788, 128 bytes.  **Complete.**
 *
 *      token == 0:
 *          obj->field1c = 0x16
 *          token := 0xc4f, then descend into t_do_zap
 *      token == 0xc4f:
 *          frame[frame].handler = t_local_reaction_exit
 *      otherwise:  return -3
 */
long t_d_lk_hi_zap_jump(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field1c = 0x16;
        *mk3_frame(thread, thread->frame + 1) = 0xc4f;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_do_zap;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0xc4f)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

/* t_d_zap_jump -- armv7 0x00069808, 112 bytes.  **Complete.**
 *
 *      token == 0:
 *          token := 0xc54, then descend into t_do_zap
 *      token == 0xc54:
 *          frame[frame].handler = t_local_reaction_exit
 *      otherwise:  return -3
 */
long t_d_zap_jump(MK3THREAD *thread)
{
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0xc54;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_do_zap;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0xc54)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

/* t_run_in_juk -- armv7 0x00069878, 128 bytes.  **Complete.**
 *
 *      token == 0:
 *          obj->a10 = 0x30
 *          obj->field48 = 0x70
 *          token := 0xc5b, then descend into t_d_run_a11
 *      token == 0xc5b:
 *          frame[frame].handler = t_d_jump_up_kick
 *      otherwise:  return -3
 */
long t_run_in_juk(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->a10 = 0x30;
        obj->field48 = 0x70;
        *mk3_frame(thread, thread->frame + 1) = 0xc5b;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_d_run_a11;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0xc5b)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_d_jump_up_kick);
}

/* t_swat_zap_hi -- armv7 0x00069c18, 128 bytes.  **Complete.**
 *
 *      token == 0:
 *          obj->field1c = 0x19
 *          token := 0xd17, then descend into t_do_zap
 *      token == 0xd17:
 *          frame[frame].handler = t_local_reaction_exit
 *      otherwise:  return -3
 */
long t_swat_zap_hi(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field1c = 0x19;
        *mk3_frame(thread, thread->frame + 1) = 0xd17;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_do_zap;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0xd17)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

/* t_sz_sky_zap -- armv7 0x00069c98, 128 bytes.  **Complete.**
 *
 *      token == 0:
 *          obj->field1c = 0x7
 *          token := 0xd1d, then descend into t_do_zap
 *      token == 0xd1d:
 *          frame[frame].handler = t_local_reaction_exit
 *      otherwise:  return -3
 */
long t_sz_sky_zap(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field1c = 0x7;
        *mk3_frame(thread, thread->frame + 1) = 0xd1d;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_do_zap;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0xd1d)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

/* t_d_quake -- armv7 0x00069dc8, 112 bytes.  **Complete.**
 *
 *      token == 0:
 *          token := 0xd3e, then descend into t_do_quake
 *      token == 0xd3e:
 *          frame[frame].handler = t_local_reaction_exit
 *      otherwise:  return -3
 */
long t_d_quake(MK3THREAD *thread)
{
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0xd3e;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_do_quake;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0xd3e)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

/* c_icharge_sd -- armv7 0x00069fb8, 104 bytes.  **Complete.**
 *
 *      token == 0:
 *          token := 0xd6c, then descend into t_nr_uppercut_if_u_can
 *      token == 0xd6c:
 *          frame[frame].handler = c_tusk_blur_sd
 *      otherwise:  return -3
 */
long c_icharge_sd(MK3THREAD *thread)
{
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0xd6c;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_nr_uppercut_if_u_can;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0xd6c)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)c_tusk_blur_sd);
}

/* c_stzap23 -- armv7 0x0006a144, 104 bytes.  **Complete.**
 *
 *      token == 0:
 *          token := 0xd94, then descend into t_nr_sweep_if_u_can
 *      token == 0xd94:
 *          frame[frame].handler = t_duck_under_mproj
 *      otherwise:  return -3
 */
long c_stzap23(MK3THREAD *thread)
{
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0xd94;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_nr_sweep_if_u_can;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0xd94)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_duck_under_mproj);
}

/* t_indian_reflect -- armv7 0x0006a694, 128 bytes.  **Complete.**
 *
 *      token == 0:
 *          obj->field1c = 0x4
 *          token := 0xedd, then descend into t_do_stationary
 *      token == 0xedd:
 *          frame[frame].handler = t_local_reaction_exit
 *      otherwise:  return -3
 */
long t_indian_reflect(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field1c = 0x4;
        *mk3_frame(thread, thread->frame + 1) = 0xedd;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_do_stationary;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0xedd)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

/* t_do_mystic_drop -- armv7 0x0006a714, 128 bytes.  **Complete.**
 *
 *      token == 0:
 *          obj->field1c = 0x1c
 *          token := 0xef6, then descend into t_do_stationary
 *      token == 0xef6:
 *          frame[frame].handler = t_local_reaction_exit
 *      otherwise:  return -3
 */
long t_do_mystic_drop(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field1c = 0x1c;
        *mk3_frame(thread, thread->frame + 1) = 0xef6;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_do_stationary;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0xef6)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

/* t_cyrax_counter_zap -- armv7 0x0006a794, 124 bytes.  **Complete.**
 *
 *      token == 0:
 *          obj->field1c = 0xa
 *          token := 0xefc, then descend into t_d_body_propell
 *      token == 0xefc:
 *          frame[frame].handler = t_local_reaction_exit
 *      otherwise:  return -3
 */
long t_cyrax_counter_zap(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field1c = 0xa;
        *mk3_frame(thread, thread->frame + 1) = 0xefc;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_d_body_propell;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0xefc)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

/* t_jade_anti_zap -- armv7 0x0006a844, 128 bytes.  **Complete.**
 *
 *      token == 0:
 *          obj->field1c = 0x1a
 *          token := 0xf03, then descend into t_do_stationary
 *      token == 0xf03:
 *          frame[frame].handler = t_run_in_close_now
 *      otherwise:  return -3
 */
long t_jade_anti_zap(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field1c = 0x1a;
        *mk3_frame(thread, thread->frame + 1) = 0xf03;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_do_stationary;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0xf03)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_run_in_close_now);
}

/* t_run_in_fk -- armv7 0x0006a8f8, 128 bytes.  **Complete.**
 *
 *      token == 0:
 *          obj->a10 = 0x40
 *          obj->field48 = 0xd0
 *          token := 0xf46, then descend into t_d_run_a11
 *      token == 0xf46:
 *          frame[frame].handler = t_d_fflip_kick_jump
 *      otherwise:  return -3
 */
long t_run_in_fk(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->a10 = 0x40;
        obj->field48 = 0xd0;
        *mk3_frame(thread, thread->frame + 1) = 0xf46;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_d_run_a11;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0xf46)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_d_fflip_kick_jump);
}

/* t_run_in_and_slam -- armv7 0x0006a978, 124 bytes.  **Complete.**
 *
 *      token == 0:
 *          obj->a10 = 0x40
 *          obj->field48 = 0x40
 *          token := 0xf55, then descend into t_d_run_a11
 *      token == 0xf55:
 *          frame[frame].handler = t_d_slam
 *      otherwise:  return -3
 */
long t_run_in_and_slam(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->a10 = 0x40;
        obj->field48 = 0x40;
        *mk3_frame(thread, thread->frame + 1) = 0xf55;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_d_run_a11;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0xf55)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_d_slam);
}

/* t_drone_sweep_closeup_sd -- armv7 0x0006ae74, 104 bytes.  **Complete.**
 *
 *      token == 0:
 *          token := 0x10ab, then descend into t_nr_sweep_if_u_can
 *      token == 0x10ab:
 *          frame[frame].handler = t_return_to_beware
 *      otherwise:  return -3
 */
long t_drone_sweep_closeup_sd(MK3THREAD *thread)
{
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x10ab;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_nr_sweep_if_u_can;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x10ab)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_return_to_beware);
}

/* t_d_lia_scream -- armv7 0x0006af5c, 112 bytes.  **Complete.**
 *
 *      token == 0:
 *          token := 0x10da, then descend into tl_stat_do_lia_scream
 *      token == 0x10da:
 *          frame[frame].handler = t_local_reaction_exit
 *      otherwise:  return -3
 */
long t_d_lia_scream(MK3THREAD *thread)
{
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x10da;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)tl_stat_do_lia_scream;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x10da)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

/* c_kroll_sd -- armv7 0x0006b128, 104 bytes.  **Complete.**
 *
 *      token == 0:
 *          token := 0x1129, then descend into t_nr_attack_sd
 *      token == 0x1129:
 *          frame[frame].handler = t_willy_go_round
 *      otherwise:  return -3
 */
long c_kroll_sd(MK3THREAD *thread)
{
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x1129;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_nr_attack_sd;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x1129)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_willy_go_round);
}

/* t_av_robo_tele -- armv7 0x0006b4cc, 120 bytes.  **Complete.**
 *
 *      token == 0:
 *          obj->a10 = 0x6
 *          token := 0x1222, then descend into t_d_stance_pause
 *      token == 0x1222:
 *          frame[frame].handler = t_d_block
 *      otherwise:  return -3
 */
long t_av_robo_tele(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->a10 = 0x6;
        *mk3_frame(thread, thread->frame + 1) = 0x1222;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_d_stance_pause;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x1222)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_d_block);
}

/* t_kick_will_miss -- armv7 0x0006be5c, 104 bytes.  **Complete.**
 *
 *      token == 0:
 *          token := 0x1414, then descend into t_nr_sweep_if_u_can
 *      token == 0x1414:
 *          frame[frame].handler = t_return_to_beware
 *      otherwise:  return -3
 */
long t_kick_will_miss(MK3THREAD *thread)
{
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x1414;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_nr_sweep_if_u_can;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x1414)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_return_to_beware);
}

/* t_ct_quake -- armv7 0x0006bf78, 104 bytes.  **Complete.**
 *
 *      token == 0:
 *          token := 0x142b, then descend into t_nr_hikick_if_u_can
 *      token == 0x142b:
 *          frame[frame].handler = t_d_jumpup_nocall
 *      otherwise:  return -3
 */
long t_ct_quake(MK3THREAD *thread)
{
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x142b;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_nr_hikick_if_u_can;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x142b)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_d_jumpup_nocall);
}

/* t_ct_axe_up -- armv7 0x0006c0a0, 104 bytes.  **Complete.**
 *
 *      token == 0:
 *          token := 0x1465, then descend into t_nr_sweep_if_u_can
 *      token == 0x1465:
 *          frame[frame].handler = t_d_block
 *      otherwise:  return -3
 */
long t_ct_axe_up(MK3THREAD *thread)
{
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x1465;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_nr_sweep_if_u_can;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x1465)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_d_block);
}

/* t_d_land -- armv7 0x0006c1d0, 124 bytes.  **Complete.**
 *
 *      token == 0:
 *          obj->field1c = 0x2
 *          token := 0x149b, then descend into t_d_beware_mframew
 *      token == 0x149b:
 *          frame[frame].handler = t_local_reaction_exit
 *      otherwise:  return -3
 */
long t_d_land(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field1c = 0x2;
        *mk3_frame(thread, thread->frame + 1) = 0x149b;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_d_beware_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x149b)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

/* --------------------------------------------------------------------
 * Added by a later sweep -- tools/sweep.py, running the same
 * readers again after one of them learned something. Each still
 * refuses anything it cannot account for instruction by
 * instruction; see tools/pushfn.py and tools/leaffn.py.
 * -------------------------------------------------------------------- */

long t_drone_entry(MK3THREAD *thread);
long t_wait_for_start(MK3THREAD *thread);
void d_init(MK3OBJ *obj);

/* t_drone_proc -- armv7 0x00067b30, 124 bytes.  **Complete.**
 *
 *      token == 0:
 *          d_init(obj)
 *          token := 0x346, then descend into t_wait_for_start
 *      token == 0x346:
 *          frame[frame].handler = t_drone_entry
 *      otherwise:  return -3
 */
long t_drone_proc(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        d_init(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x346;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_wait_for_start;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x346)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_drone_entry);
}

/* c_tusk_blur_sd -- armv7 0x0006a020, 124 bytes.  **Complete.**
 *
 *      token == 0:
 *          obj->a10 = 0x40
 *          obj->field48 = 0x40
 *          token := 0xd71, then descend into t_d_run_a11
 *      token == 0xd71:
 *          frame[frame].handler = t_d_knee
 *      otherwise:  return -3
 */
long c_tusk_blur_sd(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->a10 = 0x40;
        obj->field48 = 0x40;
        *mk3_frame(thread, thread->frame + 1) = 0xd71;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_d_run_a11;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0xd71)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_d_knee);
}

/* --------------------------------------------------------------------
 * Added by a later sweep -- tools/sweep.py, running the same
 * readers again after one of them learned something. Each still
 * refuses anything it cannot account for instruction by
 * instruction; see tools/pushfn.py and tools/leaffn.py.
 * -------------------------------------------------------------------- */

long t_d_attack_close(MK3THREAD *thread);

/* t_stalk_in_close -- armv7 0x000677b8, 132 bytes.  **Complete.**
 *
 *      token == 0:
 *          obj->a10 = 0x1900
 *          obj->field48 = 0x40
 *          token := 0x202, then descend into t_d_stalk_a11
 *      token == 0x202:
 *          frame[frame].handler = t_d_attack_close
 *      otherwise:  return -3
 */
long t_stalk_in_close(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->a10 = 0x1900;
        obj->field48 = 0x40;
        *mk3_frame(thread, thread->frame + 1) = 0x202;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_d_stalk_a11;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x202)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_d_attack_close);
}

/* --------------------------------------------------------------------
 * Added by a later sweep -- tools/sweep.py, running the same
 * readers again after one of them learned something. Each still
 * refuses anything it cannot account for instruction by
 * instruction; see tools/pushfn.py and tools/leaffn.py.
 * -------------------------------------------------------------------- */

long t_do_jump_up(MK3THREAD *thread);
long t_stat_do_lo_kick(MK3THREAD *thread);
long t_stat_do_uppercut(MK3THREAD *thread);

/* t_d_lo_kick -- armv7 0x00067634, 108 bytes.  **Complete.**
 *
 *      token == 0:
 *          token := 0x172, then descend into t_stat_do_lo_kick
 *      token == 0x172:
 *          frame[frame].handler = t_local_reaction_exit
 *      otherwise:  return -3
 */
long t_d_lo_kick(MK3THREAD *thread)
{
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x172;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_stat_do_lo_kick;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x172)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

/* t_d_uppercut -- armv7 0x000676a0, 108 bytes.  **Complete.**
 *
 *      token == 0:
 *          token := 0x184, then descend into t_stat_do_uppercut
 *      token == 0x184:
 *          frame[frame].handler = t_local_reaction_exit
 *      otherwise:  return -3
 */
long t_d_uppercut(MK3THREAD *thread)
{
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x184;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_stat_do_uppercut;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x184)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

/* t_d_walk_in_4_combos -- armv7 0x00067ab0, 128 bytes.  **Complete.**
 *
 *      token == 0:
 *          obj->a10 = 0x30
 *          obj->field48 = 0x4a
 *          token := 0x30c, then descend into t_d_stalk_a11
 *      token == 0x30c:
 *          frame[frame].handler = t_local_reaction_exit
 *      otherwise:  return -3
 */
long t_d_walk_in_4_combos(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->a10 = 0x30;
        obj->field48 = 0x4a;
        *mk3_frame(thread, thread->frame + 1) = 0x30c;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_d_stalk_a11;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x30c)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

/* t_d_jumpup -- armv7 0x0006819c, 108 bytes.  **Complete.**
 *
 *      token == 0:
 *          token := 0x558, then descend into t_do_jump_up
 *      token == 0x558:
 *          frame[frame].handler = t_d_land
 *      otherwise:  return -3
 */
long t_d_jumpup(MK3THREAD *thread)
{
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x558;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_do_jump_up;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x558)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_d_land);
}

/* t_d_stalk_crossk -- armv7 0x000685b0, 144 bytes.  **Complete.**
 *
 *      token == 0:
 *          obj->a10 = 0x40
 *          obj->field48 = 0x40
 *          token := 0x690, then descend into t_d_stalk_a11
 *      token == 0x690:
 *          frame[frame].handler = t_crossover_scan
 *      otherwise:  return -3
 */
long t_d_stalk_crossk(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->a10 = 0x40;
        obj->field48 = 0x40;
        *mk3_frame(thread, thread->frame + 1) = 0x690;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_d_stalk_a11;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x690)
        return -3;

    if (obj->a10 == 0)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    return mk3_install(thread, (MK3THREADFUNC)t_crossover_scan);
}

/* t_d_fflip_kick_jump -- armv7 0x00068744, 104 bytes.  **Complete.**
 *
 *      token == 0:
 *          token := 0x6c0, then descend into t_d_fflip_kick_jsrp
 *      token == 0x6c0:
 *          frame[frame].handler = t_local_reaction_exit
 *      otherwise:  return -3
 */
long t_d_fflip_kick_jump(MK3THREAD *thread)
{
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x6c0;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_d_fflip_kick_jsrp;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x6c0)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* ------------------------------------------------- bossck, q_is_he_a_boss
 *
 * armv7 0x00068e00 and 0x00068e20; 20 and 16 bytes.  **Complete.**
 *
 *      bossck(obj, part)      obj->field54 = part->0x24
 *                             obj->field5c = (part->0x24 - 0x18 <= 1u)
 *
 *      q_is_he_a_boss(obj)    bossck(obj, obj->field00->him)
 *
 * **A boss is character 0x18 or 0x19**, tested with the range idiom -- subtract
 * the low end and compare unsigned against the span, so two characters cost one
 * branch.
 *
 * Both numbers were already known from elsewhere and this is the first place
 * they appear together: **0x18 is Motaro**, from `is_he_motaro` and from
 * `proj_strike_check`, and **0x19 is Shao Kahn**, which Blood.c spells out as
 * `#define SHAO_KAHN 0x19`. So "boss" means exactly those two and the engine
 * has no other notion of one.
 *
 * The character number comes from the PART's 0x24 -- third independent
 * confirmation that `GrObj + 0x24` is where a fighter's identity lives, after
 * `ochar_begin_calls` and `mk3_update`'s display record.
 *
 * `q_is_he_a_boss` asks it about `proc->him`, the opponent's part, and the
 * answer lands in `obj->field5c` -- the boolean return slot the `q_` family
 * uses. The wrapper shape this directory is full of: a core that takes the part
 * and a `q_` that supplies the opponent's.
 */
#define MK3_MOTARO     0x18
#define MK3_SHAO_KAHN  0x19

void bossck(MK3OBJ *obj, MK3OBJ *part)
{
    uint32_t c = part->field24;

    obj->field54 = c;
    obj->field5c = ((c - MK3_MOTARO) <= 1u);
}

void q_is_he_a_boss(MK3OBJ *obj)
{
    bossck(obj, (MK3OBJ *)(uintptr_t)obj->field00->him);
}

/* ----------------------------------------------------------- q_is_he_reacting
 *
 * armv7 0x0006c7fc, thirty-six bytes.  **Complete.**
 *
 * The single dry reaction probe: `get_his_action` drags the opponent's
 * action into `obj->field20`, and the queue answers whether it is the 0x503
 * reaction -- a `q_yes(obj)`/`q_no(obj)` straight from `field20`.
 *
 *      get_his_action(obj)
 *      if (obj->field20 == 0x503) q_yes(obj)
 *      else q_no(obj)
 */
long q_no(MK3OBJ *obj);   /* 0x00067524: answers 0 and returns it */
long q_yes(MK3OBJ *obj);  /* 0x0006752c: answers 1 and returns it */

long q_is_he_reacting(MK3OBJ *obj)
{
    get_his_action(obj);
    if (obj->field20 == 0x503)
        return q_yes(obj);
    return q_no(obj);
}

/* q_yes / q_no -- armv7 0x0006752c / 0x00067524: this file's own copies of the
 * answer helpers. Unlike moves.c's they leave the answer in r0 as well
 * (`mov r3, r0; movs r0, #1; str r0, [r3, #0x5c]`), and q_is_he_reacting
 * tail-calls them, so r0 is the answer to its callers. */
long q_yes(MK3OBJ *obj)
{
    obj->field5c = 1;
    return 1;
}

long q_no(MK3OBJ *obj)
{
    obj->field5c = 0;
    return 0;
}


/* ======================================================================
 * Leaf predicates and small helpers.
 *
 * Written from `tools/cd.py` listings; each is verified against the binary by
 * factdiff before it counts. The `q_*` family answers in `obj->field5c`
 * through `vq_yes`/`vq_no`; the comparison in each is signed (`ble`/`bgt`).
 * ====================================================================== */

void vq_no(MK3OBJ *obj);
void vq_yes(MK3OBJ *obj);
void call_for_him(MK3OBJ *obj, void (*fn)(MK3OBJ *));
void back_to_normal(MK3OBJ *obj);
void get_his_dog(MK3OBJ *obj);
long am_i_facing_him(MK3OBJ *obj);
void *FindThreadProc(uint32_t pid);
void get_my_dfe(MK3OBJ *obj);
void d_behind_me_a5(MK3OBJ *obj);
void is_he_body_propell(MK3OBJ *obj);
void get_his_proj_proc(MK3OBJ *obj);
void q_his_proj_proc(MK3OBJ *obj);
void beh1(MK3OBJ *obj);
long rpt_counter(struct MK3THREAD *thread);

/* get_his_y_vel -- armv7 0x00068dd8: obj->field1c = him->field1c */
void get_his_y_vel(MK3OBJ *obj)
{
    obj->field1c = ((MK3OBJ *)(uintptr_t)obj->field00->him)->field1c;
}

/* beh1 -- armv7 0x00068dc4: field2c = part->field28 (the whole word); when
 * bit 4 of it is set, field30 = field34. */
void beh1(MK3OBJ *obj)
{
    uint32_t w = *(const uint32_t *)(const void *)
                    ((const char *)(const void *)obj->field08 + 0x28);

    obj->field2c = w;
    if ((w & 0x10u) != 0)
        obj->field30 = obj->field34;
}

/* d_to_normal -- armv7 0x00072cb8 */
void d_to_normal(MK3OBJ *obj)
{
    back_to_normal(obj);
    obj->field1c = 0;
    obj->field00->field5c = 0;
}

/* q_am_i_cornered -- armv7 0x00070f58: field5c = (field30 <= 0x90) */
void q_am_i_cornered(MK3OBJ *obj)
{
    d_behind_me_a5(obj);
    obj->field5c = ((int32_t)obj->field30 > 0x90) ? 0 : 1;
}

/* q_is_he_cornered -- armv7 0x000708a8 */
void q_is_he_cornered(MK3OBJ *obj)
{
    call_for_him(obj, q_am_i_cornered);
}

/* should_i_promove -- armv7 0x0006c9f4 */
void should_i_promove(MK3OBJ *obj)
{
    obj->field1c = (uint32_t)(uintptr_t)rpt_counter;
    ask_mr_diff(obj);
}

/* d_either_edge_a5 -- armv7 0x00071328: field30 = min(field30, field34) */
void d_either_edge_a5(MK3OBJ *obj)
{
    get_my_dfe(obj);
    if ((int32_t)obj->field34 <= (int32_t)obj->field30)
        obj->field30 = obj->field34;
}

/* d_front_me_a5 -- armv7 0x00070f28: swap field30/field34, then beh1 */
void d_front_me_a5(MK3OBJ *obj)
{
    uint32_t a, b;

    get_my_dfe(obj);
    a = obj->field30;
    b = obj->field34;
    obj->field34 = a;
    obj->field30 = b;
    beh1(obj);
}

/* scan_1_entry -- armv7 0x0006c24c: read one halfword through field1c */
void scan_1_entry(MK3OBJ *obj)
{
    const int16_t *p = (const int16_t *)(uintptr_t)obj->field1c;
    int32_t v = *p++;

    obj->field1c = (uint32_t)(uintptr_t)p;
    obj->field24 = (uint32_t)v;
    if ((int32_t)obj->field20 == v)
        obj->field28 = obj->field28 + 1;
}

/* get_his_proj_proc -- armv7 0x0006ff18 */
void get_his_proj_proc(MK3OBJ *obj)
{
    obj->field1c = (uint32_t)(uintptr_t)FindThreadProc(
        (uint32_t)(obj->field00->field00->field00->field08 + 0x700));
}

void q_his_proj_proc(MK3OBJ *obj)
{
    get_his_proj_proc(obj);
    if (obj->field1c != 0)
        vq_yes(obj);
    else
        vq_no(obj);
}

/* q_is_he_dropping -- armv7 0x00068de4 */
void q_is_he_dropping(MK3OBJ *obj)
{
    obj->field1c = ((MK3OBJ *)(uintptr_t)obj->field00->him)->field1c;
    if ((int32_t)obj->field1c > 0)
        vq_yes(obj);
    else
        vq_no(obj);
}

/* q_my_back_to_him -- armv7 0x0006f1d8: field5c = 1 - field5c, or 0 if it
 * was not 0 or 1 (the `rsbs` borrows) */
void q_my_back_to_him(MK3OBJ *obj)
{
    am_i_facing_him(obj);
    obj->field5c = (obj->field5c <= 1u) ? 1u - obj->field5c : 0u;
}

/* is_he_body_propell -- armv7 0x0006c5ec */
void is_he_body_propell(MK3OBJ *obj)
{
    uint32_t w;

    get_his_action(obj);
    w = obj->field20 & ~0xffu;
    obj->field20 = w;
    obj->field5c = (w == 0x200) ? 1 : 0;
}

/* The `get_x_dist` distance tests: yes inside the limit, no beyond it. */
void q_dist_lift(MK3OBJ *obj)
{
    get_x_dist(obj);
    if ((int32_t)obj->field28 > 0xa0)
        vq_yes(obj);
    else
        vq_no(obj);
}

void q_is_he_axe_close(MK3OBJ *obj)
{
    get_x_dist(obj);
    if ((int32_t)obj->field28 > 0x70)
        vq_no(obj);
    else
        vq_yes(obj);
}

void q_is_he_bike_close(MK3OBJ *obj)
{
    get_x_dist(obj);
    if ((int32_t)obj->field28 > 0x80)
        vq_no(obj);
    else
        vq_yes(obj);
}

void q_is_he_scream_close(MK3OBJ *obj)
{
    get_x_dist(obj);
    if ((int32_t)obj->field28 > 0xb0)
        vq_no(obj);
    else
        vq_yes(obj);
}

void q_willy_uppercut(MK3OBJ *obj)
{
    get_his_dog(obj);
    if ((int32_t)obj->field1c > 0x40)
        vq_no(obj);
    else
        vq_yes(obj);
}

void q_is_proj_gone(MK3OBJ *obj)
{
    get_his_proj_proc(obj);
    if (obj->field1c != 0)
        vq_no(obj);
    else
        vq_yes(obj);
}

void q_run_then_flipk(MK3OBJ *obj)
{
    get_x_dist(obj);
    if ((int32_t)obj->field28 > 0xef)
        vq_no(obj);
    else
        vq_yes(obj);
}

void q_run_then_duck(MK3OBJ *obj)
{
    get_x_dist(obj);
    if ((int32_t)obj->field28 > 0xaf)
        q_his_proj_proc(obj);
    else
        vq_yes(obj);
}

void is_flyk_close(MK3OBJ *obj)
{
    is_he_body_propell(obj);
    if (obj->field5c == 0)
        goto yes;
    get_x_dist(obj);
    if ((int32_t)obj->field28 > 0x7f) {
        vq_no(obj);
        return;
    }
yes:
    vq_yes(obj);
}

void is_propell_close(MK3OBJ *obj)
{
    is_he_body_propell(obj);
    if (obj->field5c == 0)
        goto yes;
    get_x_dist(obj);
    if ((int32_t)obj->field28 > 0x6f) {
        vq_no(obj);
        return;
    }
yes:
    vq_yes(obj);
}

void q_corner_backf_land(MK3OBJ *obj)
{
    get_his_y_vel(obj);
    if ((int32_t)obj->field1c < 0)
        obj->field1c = (uint32_t)(-(int32_t)obj->field1c);
    if ((int32_t)obj->field1c > 0x20000)
        vq_no(obj);
    else
        vq_yes(obj);
}

void q_is_he_net_close(MK3OBJ *obj)
{
    get_his_y_vel(obj);
    if ((int32_t)obj->field1c < 0)
        obj->field1c = (uint32_t)(-(int32_t)obj->field1c);
    if ((int32_t)obj->field1c > 0x10000)
        vq_no(obj);
    else
        vq_yes(obj);
}

/* ======================================================================
 * Second batch of leaves: the zone, wait and dizzy probes.
 * ====================================================================== */

long strike_check_a0_test(MK3OBJ *obj);
long CountThreads(uint32_t pid);
long randper(MK3OBJ *obj);
void get_my_hitq(MK3OBJ *obj);
long t_dizzy_sleep(struct MK3THREAD *thread);
long rpt_counter_airborns(struct MK3THREAD *thread);

/* is_throwing_allowed -- armv7 0x00067f74: the round clock halfword G+0x44c,
 * above 1 means yes */
long is_throwing_allowed(MK3OBJ *obj)
{
    int32_t w = *(const int16_t *)(const void *)(G_BYTES + 0x44c);

    obj->field1c = (uint32_t)w;
    obj->field5c = (w > 1) ? 1 : 0;
    return (long)obj->field5c;
}

void q_jax_smash(MK3OBJ *obj)
{
    get_his_y_vel(obj);
    if ((int32_t)obj->field1c < 0)
        vq_no(obj);
    else
        vq_yes(obj);
}

void q_run_under_fk(MK3OBJ *obj)
{
    am_i_facing_him(obj);
    if (obj->field5c != 0)
        vq_no(obj);
    else
        vq_yes(obj);
}

/* d_init -- armv7 0x00067590 */
void d_init(MK3OBJ *obj)
{
    MK3OBJPROC *proc = obj->field00;
    uint32_t w = proc->field10 & ~1u;

    obj->field2c = w;
    proc->field10 = w;
    obj->field1c = 2;
    *(uint32_t *)(void *)(G_BYTES + 0x448) = 2;
}

void q_airborn_counter(MK3OBJ *obj)
{
    is_he_airborn(obj);
    if (obj->field5c != 0) {
        obj->field1c = (uint32_t)(uintptr_t)rpt_counter_airborns;
        ask_mr_diff(obj);
    }
}

void q_is_decoy_alive(MK3OBJ *obj)
{
    if (CountThreads(0x200 - obj->field00->field08 + 5) != 0)
        vq_yes(obj);
    else
        vq_no(obj);
}

void is_he_attacking(MK3OBJ *obj)
{
    uint32_t w;

    get_his_action(obj);
    w = obj->field20 & ~0xffu;
    obj->field20 = w;
    obj->field5c = (w == 0x200 || w == 0x100) ? 1 : 0;
}

/* q_will_he_reach_me -- armv7 0x0006e9c4: put the object in HIS shoes (his
 * proc, his part, his field58), run the strike test, put it back. */
long q_will_he_reach_me(MK3OBJ *obj)
{
    MK3OBJPROC *proc = obj->field00;
    MK3OBJ     *part = obj->field08;
    MK3OBJ     *him  = proc->field00;
    MK3OBJPROC *hp   = him->field00;
    long        r;

    obj->field00 = hp;
    obj->field08 = him->field08;
    obj->field1c = hp->field58;
    r = strike_check_a0_test(obj);
    obj->field08 = part;
    obj->field00 = proc;
    return r;
}

void q_backup_zap(MK3OBJ *obj)
{
    get_his_y_vel(obj);
    if ((int32_t)obj->field1c < 0)
        goto no;
    get_his_dog(obj);
    if ((int32_t)obj->field1c <= 0x70)
        goto yes;
no:
    vq_no(obj);
    return;
yes:
    vq_yes(obj);
}

/* ask_mr_diff -- armv7 0x0006c9c8: the difficulty (G+0x44c, 7 when it is out
 * of range) indexes the halfword table field1c points at */
long ask_mr_diff(MK3OBJ *obj)
{
    uint32_t d = (uint32_t)(int32_t)*(const int16_t *)(const void *)(G_BYTES + 0x44c);

    obj->field20 = d;
    if (d > 9u)
        obj->field20 = 7;
    obj->field1c = (uint32_t)(int32_t)
        ((const int16_t *)(uintptr_t)obj->field1c)[obj->field20];
    return randper(obj);
}

void lao_angle_wait(MK3OBJ *obj)
{
    get_his_action(obj);
    if (obj->field20 != 0x20c)
        goto yes;
    get_x_dist(obj);
    if ((int32_t)obj->field28 <= 0x6f)
        goto yes;
    vq_no(obj);
    return;
yes:
    vq_yes(obj);
}

/* q_is_he_below_ground -- armv7 0x00068e74: his y (halfword at +0x12) above
 * zero and under what his proc keeps at +0x40 */
void q_is_he_below_ground(MK3OBJ *obj)
{
    MK3OBJPROC *proc = obj->field00;
    int32_t y = MK3_FIELD12_S((MK3OBJ *)(uintptr_t)proc->him);
    uint32_t lim = *(const uint32_t *)(const void *)
                      ((const char *)(const void *)proc->field00->field00 + 0x40);

    obj->field1c = (uint32_t)y;
    obj->field20 = lim;
    if (y < 0)
        goto no;
    if ((int32_t)lim < y)
        goto yes;
no:
    vq_no(obj);
    return;
yes:
    vq_yes(obj);
}

/* q_is_he_dizzy -- armv7 0x00068ea0: is his frame's handler t_dizzy_sleep */
void q_is_he_dizzy(MK3OBJ *obj)
{
    MK3THREAD *t = obj->field00->field00->thread;
    uint32_t h = mk3_frame(t, t->frame)[1];

    obj->field38 = h;
    obj->field5c = (h == (uint32_t)(uintptr_t)t_dizzy_sleep) ? 1 : 0;
}

void q_is_kick_over(MK3OBJ *obj)
{
    get_his_action(obj);
    if (obj->field20 == 0x50a)
        goto yes;
    is_he_airborn(obj);
    if (obj->field5c != 0) {
        vq_no(obj);
        return;
    }
yes:
    vq_yes(obj);
}

/* count_q_repeats -- armv7 0x0006c8e0: six scans of his hit queue, unrolled */
void count_q_repeats(MK3OBJ *obj)
{
    uint32_t keep = obj->field24;

    get_my_hitq(obj);
    obj->field28 = 0;
    scan_1_entry(obj);
    scan_1_entry(obj);
    scan_1_entry(obj);
    scan_1_entry(obj);
    scan_1_entry(obj);
    scan_1_entry(obj);
    obj->field24 = keep;
}

void q_drone_zone(MK3OBJ *obj)
{
    d_either_edge_a5(obj);
    if ((int32_t)obj->field30 <= 0x4f)
        goto yes;
    get_x_dist(obj);
    if ((int32_t)obj->field28 <= 0xcf)
        goto yes;
    if ((int32_t)obj->field28 <= 0x100)
        goto no;
yes:
    vq_yes(obj);
    return;
no:
    vq_no(obj);
}

/* ======================================================================
 * Flip setups, the "forget" returns and the closeup picker.
 * ====================================================================== */

long is_he_right(MK3OBJ *obj);
void reset_proc_stack(MK3THREAD *thread);
long t_victory_animation(struct MK3THREAD *thread);
long t_return_to_beware(struct MK3THREAD *thread);
extern const uint32_t tab_react_flipk[];

/* backflip_setup / frontflip_setup -- armv7 0x00070c6c, 0x00070cf8: the same
 * body with the test inverted. Both lay out the jump (0x40000, 0x70000, 0x1a,
 * 0x1b) and mirror it when the opponent is on the other side. */
void backflip_setup(MK3OBJ *obj)
{
    obj->field48 = 0x40000;
    obj->field1c = 0x1a;
    obj->field34 = 0x40000 + 0x30000;
    obj->field20 = 0x1b;
    is_he_right(obj);
    if (obj->field5c != 0) {
        obj->field20 = 0x1a;
        obj->field1c = 0x1b;
        obj->field48 = (uint32_t)(-(int32_t)obj->field48);
        obj->field34 = (uint32_t)(-(int32_t)obj->field34);
    }
}

void frontflip_setup(MK3OBJ *obj)
{
    obj->field48 = 0x40000;
    obj->field1c = 0x1a;
    obj->field34 = 0x40000 + 0x30000;
    obj->field20 = 0x1b;
    is_he_right(obj);
    if (obj->field5c == 0) {
        obj->field20 = 0x1a;
        obj->field1c = 0x1b;
        obj->field48 = (uint32_t)(-(int32_t)obj->field48);
        obj->field34 = (uint32_t)(-(int32_t)obj->field34);
    }
}

/* q_is_he_lower -- armv7 0x00068e30 */
void q_is_he_lower(MK3OBJ *obj)
{
    MK3OBJPROC *proc = obj->field00;
    MK3OBJ     *him  = (MK3OBJ *)(uintptr_t)proc->him;
    int32_t     y, d;

    obj->field1c = him->field1c;
    if ((int32_t)obj->field1c < 0)
        goto no;
    y = MK3_FIELD12_S(him);
    obj->field20 = (uint32_t)y;
    d = (int32_t)(*(const uint32_t *)(const void *)
                    ((const char *)(const void *)proc->field00->field00 + 0x40)
                  - (uint32_t)y);
    obj->field24 = (uint32_t)d;
    if (d <= (int32_t)obj->field38)
        goto yes;
no:
    vq_no(obj);
    return;
yes:
    vq_yes(obj);
}

/* t_return_and_4get, t_return_to_beware_4get -- armv7 0x000695f8, 0x0006c148:
 * the same two bodies, clearing the animation and the proc's answer and
 * handing over to t_return_to_beware */
long t_return_and_4get(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0;
    obj->field00->field5c = 0;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_return_to_beware);
}

long t_return_to_beware_4get(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0;
    obj->field00->field5c = 0;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_return_to_beware);
}

/* is_towards_me -- armv7 0x00070940: his x speed (field18) against which side
 * he is on; zero means no */
void is_towards_me(MK3OBJ *obj)
{
    int32_t v = (int32_t)((MK3OBJ *)(uintptr_t)obj->field00->him)->field18;

    obj->field1c = (uint32_t)v;
    if (v == 0)
        goto no;
    if (v < 0) {
        is_he_right(obj);
        if (obj->field5c == 0)
            goto no;
        goto yes;
    }
    is_he_right(obj);
    if (obj->field5c != 0)
        goto no;
yes:
    obj->field5c = 1;
    return;
no:
    obj->field5c = 0;
}

/* t_attack_closeup_sd -- armv7 0x0006aedc: the handler comes out of
 * tab_react_flipk[part->field24] */
long t_attack_closeup_sd(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t h;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    h = tab_react_flipk[obj->field08->field24];
    obj->field1c = h;

    return mk3_push_handler(thread, (MK3THREADFUNC)(uintptr_t)h);
}

/* t_d_fatality_abort -- armv7 0x000703c8 */
long t_d_fatality_abort(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    reset_proc_stack(thread);

    /* the refusal was above; a second one here would read the slot of the
     * stack that was just reset */
    return mk3_install(thread, (MK3THREADFUNC)t_victory_animation);
}

/* ======================================================================
 * Return-to-beware, the tracker probe, the flip scans.
 * ====================================================================== */

long t_d_propell_attack_now(struct MK3THREAD *thread);
long t_do_flip(MK3THREAD *thread);
long t_dflip3(MK3THREAD *thread);

/* t_return_to_beware -- armv7 0x0006c184: the proc keeps a saved state at
 * 0x6c (handler), 0x70 (resume token), 0x74 and 0x78 (the two words of the
 * object it overwrote); this puts them back and resumes */
long t_return_to_beware(MK3THREAD *thread)
{
    MK3OBJ     *obj  = (MK3OBJ *)thread->proc;
    MK3OBJPROC *proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    proc = obj->field00;
    obj->a10     = *(const uint32_t *)(const void *)((const char *)(const void *)proc + 0x74);
    obj->field48 = *(const uint32_t *)(const void *)((const char *)(const void *)proc + 0x78);
    mk3_frame(thread, thread->frame)[1] =
        *(const uint32_t *)(const void *)((const char *)(const void *)proc + 0x6c);
    *mk3_frame(thread, thread->frame + 1) =
        *(const uint32_t *)(const void *)((const char *)(const void *)obj->field00 + 0x70);
    return 0;
}

/* q_is_tracker_close -- armv7 0x000701d8: his projectile's x against mine,
 * absolute, inside 0x6f. No projectile counts as close. */
void q_is_tracker_close(MK3OBJ *obj)
{
    MK3OBJ *proj;
    int32_t px, mx;

    get_his_proj_proc(obj);
    if (obj->field1c == 0)
        goto yes;
    proj = (MK3OBJ *)(uintptr_t)obj->field1c;
    px = MK3_FIELD0E_S(proj->field08);
    obj->field20 = (uint32_t)px;
    mx = MK3_FIELD0E_S(obj->field08);
    obj->field24 = (uint32_t)mx;
    obj->field20 = (uint32_t)(px - mx);
    if ((int32_t)obj->field20 < 0)
        obj->field20 = (uint32_t)(-(int32_t)obj->field20);
    if ((int32_t)obj->field20 > 0x6f) {
        vq_no(obj);
        return;
    }
yes:
    vq_yes(obj);
}

/* t_d_propell_attack -- armv7 0x00067ec4: the difficulty picks the handler */
long t_d_propell_attack(MK3THREAD *thread)
{
    MK3OBJ       *obj = (MK3OBJ *)thread->proc;
    MK3THREADFUNC h;
    int32_t       d;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    d = *(const int16_t *)(const void *)(G_BYTES + 0x44c);
    obj->field1c = (uint32_t)d;
    if (d > 2)
        h = (MK3THREADFUNC)t_d_propell_attack_now;
    else
        h = (MK3THREADFUNC)t_diff_no_propell;
    return mk3_install(thread, h);
}

/* t_d_bflip_scan_jsrp, t_d_fflip_scan_jsrp -- armv7 0x00070ca4, 0x00070e4c:
 * push field34 on the argument stack, set the flip up, hand over to t_dflip3 */
long t_d_bflip_scan_jsrp(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t n;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    n = thread->fieldf8;
    *mk3_arg(thread, n) = obj->field34;
    thread->fieldf8 = n + 1;
    backflip_setup(obj);
    return mk3_install(thread, (MK3THREADFUNC)t_dflip3);
}

long t_d_fflip_scan_jsrp(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t n;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    n = thread->fieldf8;
    *mk3_arg(thread, n) = obj->field34;
    thread->fieldf8 = n + 1;
    frontflip_setup(obj);
    return mk3_install(thread, (MK3THREADFUNC)t_dflip3);
}

/* t_dflip3 -- armv7 0x000682e0: pop field34 back, mirror it into the proc,
 * then the flip itself */
long t_dflip3(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t n, v;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    n = thread->fieldf8 - 1;
    thread->fieldf8 = n;
    v = *mk3_arg(thread, n);
    obj->field34 = v;
    obj->field00->field28 = v;
    return mk3_install(thread, (MK3THREADFUNC)t_do_flip);
}

/* ======================================================================
 * Distance deciders, the retp/ochar threads and the projectile probes.
 *
 * The deciders below read one distance or table entry, choose between two
 * handlers and install it -- ONE install site, which is why each picks into
 * a local first.
 * ====================================================================== */

long t_d_fflip_kick_jump(struct MK3THREAD *thread);
long t_d_crossover_kick(struct MK3THREAD *thread);
long t_swait_nonattack_jump(struct MK3THREAD *thread);
long t_asb2(struct MK3THREAD *thread);
long t_d_lo_kick(struct MK3THREAD *thread);
long t_stalk_in_close(struct MK3THREAD *thread);
long t_local_reaction_exit(struct MK3THREAD *thread);
long t_duck_under_proj(struct MK3THREAD *thread);
long t_d_zap_now(struct MK3THREAD *thread);
long t_drfp4(struct MK3THREAD *thread);
long t_random_do(struct MK3THREAD *thread);
long t_run_in_close_now(struct MK3THREAD *thread);
long c_froze_closer(struct MK3THREAD *thread);
long t_d_body_propell(struct MK3THREAD *thread);
long t_perhaps_flipk(struct MK3THREAD *thread);
long t_react_jump_table_act(struct MK3THREAD *thread);
long t_d_bflip_noscan_jsrp(struct MK3THREAD *thread);
long t_d_attack(struct MK3THREAD *thread);
long funcs_7674(struct MK3THREAD *thread);
long funcs_13651(struct MK3THREAD *thread);
void find_ani_part2(MK3OBJ *obj);
void stop_me_player(MK3OBJ *obj);
void find_part2(MK3OBJ *obj);
extern const int32_t ochar_props[];

long c_lkzaplo(MK3THREAD *thread)
{
    MK3OBJ       *obj = (MK3OBJ *)thread->proc;
    MK3THREADFUNC h;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    get_x_dist(obj);
    if ((int32_t)obj->field28 > 0x7f)
        h = (MK3THREADFUNC)t_d_fflip_kick_jump;
    else
        h = (MK3THREADFUNC)t_d_crossover_kick;
    return mk3_install(thread, h);
}

long t_av_sonya_bike(MK3THREAD *thread)
{
    MK3OBJ       *obj = (MK3OBJ *)thread->proc;
    MK3THREADFUNC h;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    get_x_dist(obj);
    if ((int32_t)obj->field28 > 0x6f)
        h = (MK3THREADFUNC)t_swait_nonattack_jump;
    else
        h = (MK3THREADFUNC)t_asb2;
    return mk3_install(thread, h);
}

long t_diff_no_propell(MK3THREAD *thread)
{
    MK3OBJ       *obj = (MK3OBJ *)thread->proc;
    MK3THREADFUNC h;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    get_x_dist(obj);
    if ((int32_t)obj->field28 > 0x47)
        h = (MK3THREADFUNC)t_stalk_in_close;
    else
        h = (MK3THREADFUNC)t_d_lo_kick;
    return mk3_install(thread, h);
}

long c_frozen(MK3THREAD *thread)
{
    MK3OBJ       *obj = (MK3OBJ *)thread->proc;
    MK3THREADFUNC h;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    get_x_dist(obj);
    if ((int32_t)obj->field28 > 0x6f)
        h = (MK3THREADFUNC)t_run_in_close_now;
    else
        h = (MK3THREADFUNC)c_froze_closer;
    return mk3_install(thread, h);
}

long c_tusk_zap_air(MK3THREAD *thread)
{
    MK3OBJ       *obj = (MK3OBJ *)thread->proc;
    MK3THREADFUNC h;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    get_his_dog(obj);
    if ((int32_t)obj->field1c > 0x18)
        h = (MK3THREADFUNC)t_d_zap_now;
    else
        h = (MK3THREADFUNC)t_duck_under_proj;
    return mk3_install(thread, h);
}

long t_run_in_close(MK3THREAD *thread)
{
    MK3OBJ       *obj = (MK3OBJ *)thread->proc;
    MK3THREADFUNC h;
    int32_t       d;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    d = *(const int16_t *)(const void *)(G_BYTES + 0x44c);
    obj->field1c = (uint32_t)d;
    if (d > 1)
        h = (MK3THREADFUNC)t_run_in_close_now;
    else
        h = (MK3THREADFUNC)t_perhaps_flipk;
    return mk3_install(thread, h);
}

long t_d_propell_attack_now(MK3THREAD *thread)
{
    MK3OBJ       *obj = (MK3OBJ *)thread->proc;
    MK3THREADFUNC h;
    int32_t       v;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    v = ochar_props[obj->field08->field24];
    obj->field1c = (uint32_t)v;
    if (v < 0)
        h = (MK3THREADFUNC)t_stalk_in_close;
    else
        h = (MK3THREADFUNC)t_d_body_propell;
    return mk3_install(thread, h);
}

long c_juppunch(MK3THREAD *thread)
{
    MK3OBJ       *obj = (MK3OBJ *)thread->proc;
    MK3THREADFUNC h;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    get_x_dist(obj);
    if ((int32_t)obj->field28 > 0x60) {
        h = (MK3THREADFUNC)t_return_to_beware;
    } else {
        *(uint32_t *)((char *)obj + 0x68) = (uint32_t)(uintptr_t)funcs_13651;
        h = (MK3THREADFUNC)t_react_jump_table_act;
    }
    return mk3_install(thread, h);
}

long t_d_bflip_jsrp(MK3THREAD *thread)
{
    MK3OBJ       *obj = (MK3OBJ *)thread->proc;
    MK3THREADFUNC h;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    d_behind_me_a5(obj);
    if ((int32_t)obj->field30 > 0x6f) {
        backflip_setup(obj);
        h = (MK3THREADFUNC)t_d_bflip_noscan_jsrp;
    } else {
        reset_proc_stack(thread);
        h = (MK3THREADFUNC)t_d_attack;
    }
    return mk3_install(thread, h);
}

/* t_dist_retp -- armv7 0x0006eda4: stop, then pop a level, or become
 * t_local_reaction_exit when there is none to pop */
long t_dist_retp(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    stop_me_player(obj);
    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
        return 0;
    }
    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

/* t_ochar_do -- armv7 0x00067534: this level becomes t_local_reaction_exit
 * (and field20 remembers it), and one level above it the routine field1c
 * names runs */
long t_ochar_do(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field20 = (uint32_t)(uintptr_t)t_local_reaction_exit;
    mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = obj->field1c;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}

/* t_drfp3 -- armv7 0x00071b28: peek the argument stack into field40, find
 * the animation part and two parts, hand over to t_drfp4 */
long t_drfp3(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field40 = *mk3_arg(thread, thread->fieldf8 - 1);
    find_ani_part2(obj);
    find_part2(obj);
    find_part2(obj);
    return mk3_install(thread, (MK3THREADFUNC)t_drfp4);
}

/* t_run_in_close_hard -- armv7 0x0006783c: two words in the object, then a
 * push with resume token 0x226 into t_random_do */
long t_run_in_close_hard(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    *(uint32_t *)((char *)obj + 0x68) = (uint32_t)(uintptr_t)funcs_7674;
    *(uint32_t *)((char *)obj + 0x64) = 3;
    *mk3_frame(thread, thread->frame + 1) = 0x226;
    thread->frame = thread->frame + 1;
    return mk3_install(thread, (MK3THREADFUNC)t_random_do);
}

/* his_proj_front_x -- armv7 0x0006ff34: where the front of his projectile is:
 * its part's x plus the offsets the proc keeps at +0x84, mirrored when the
 * part faces left */
void his_proj_front_x(MK3OBJ *obj)
{
    MK3OBJ         *proj;
    MK3OBJPROC     *pp;
    MK3OBJ         *part;
    const uint32_t *off;
    uint32_t        a, b, f;

    get_his_proj_proc(obj);
    proj = (MK3OBJ *)(uintptr_t)obj->field1c;
    pp   = proj->field00;
    obj->field38 = (uint32_t)(uintptr_t)proj;
    part = proj->field08;
    off  = (const uint32_t *)(uintptr_t)pp->field84;
    obj->field30 = (uint32_t)(uintptr_t)part;
    obj->field1c = (uint32_t)(uintptr_t)off;
    obj->field28 = (uint32_t)(int32_t)MK3_FIELD0E_S(part);
    if (off == 0)
        return;
    a = off[0];
    obj->field24 = a;
    b = off[2];
    obj->field2c = b;
    f = *(const uint32_t *)(const void *)((const char *)(const void *)part + 0x28);
    obj->field34 = f;
    if ((f & 0x10u) != 0) {
        obj->field24 = (uint32_t)(-(int32_t)a);
        obj->field2c = (uint32_t)(-(int32_t)b);
    }
    obj->field28 = obj->field24 + obj->field28 + obj->field2c;
}

/* ======================================================================
 * Zap/slam deciders and the drfp chain.
 * ====================================================================== */

long t_d_zap(struct MK3THREAD *thread);
long t_d_block(struct MK3THREAD *thread);
long t_do_body_propell(struct MK3THREAD *thread);
long t_run_in_and_slam(struct MK3THREAD *thread);
long t_block_orb(struct MK3THREAD *thread);
long t_d_knee(struct MK3THREAD *thread);
long t_drone_sweep_closeup_sd(struct MK3THREAD *thread);
long t_attack_closeup_sd(struct MK3THREAD *thread);
long t_run_in_close(struct MK3THREAD *thread);
long t_d_crossover_kick(struct MK3THREAD *thread);
void should_i_promove(MK3OBJ *obj);
void his_proj_front_x(MK3OBJ *obj);

long t_stsw_zap(MK3THREAD *thread)
{
    MK3OBJ       *obj = (MK3OBJ *)thread->proc;
    MK3THREADFUNC h;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    get_x_dist(obj);
    if ((int32_t)obj->field28 <= 0x7f)
        h = (MK3THREADFUNC)t_d_crossover_kick;
    else if ((int32_t)obj->field28 > 0xd0)
        h = (MK3THREADFUNC)t_d_zap;
    else
        h = (MK3THREADFUNC)t_d_block;
    return mk3_install(thread, h);
}

/* t_d_body_propell -- armv7 0x0006c264: this level becomes
 * t_local_reaction_exit (and field38 remembers it), one level above it the
 * body-propell routine runs */
long t_d_body_propell(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field38 = (uint32_t)(uintptr_t)t_local_reaction_exit;
    mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

    thread->frame = thread->frame + 1;
    return mk3_install(thread, (MK3THREADFUNC)t_do_body_propell);
}

/* t_drfp2 -- armv7 0x00071b84: t_drfp3 with a third find_part2 */
long t_drfp2(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field40 = *mk3_arg(thread, thread->fieldf8 - 1);
    find_ani_part2(obj);
    find_part2(obj);
    find_part2(obj);
    find_part2(obj);
    return mk3_install(thread, (MK3THREADFUNC)t_drfp4);
}

/* The three `_sd` deciders: ask whether to promote, then pick by distance.
 * No promotion means t_return_to_beware. */
long c_er_slam_sd(MK3THREAD *thread)
{
    MK3OBJ       *obj = (MK3OBJ *)thread->proc;
    MK3THREADFUNC h;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    should_i_promove(obj);
    if (obj->field5c == 0) {
        h = (MK3THREADFUNC)t_return_to_beware;
    } else {
        get_x_dist(obj);
        if ((int32_t)obj->field28 > 0x9f)
            h = (MK3THREADFUNC)t_run_in_close;
        else
            h = (MK3THREADFUNC)t_attack_closeup_sd;
    }
    return mk3_install(thread, h);
}

long c_proj_sd(MK3THREAD *thread)
{
    MK3OBJ       *obj = (MK3OBJ *)thread->proc;
    MK3THREADFUNC h;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    should_i_promove(obj);
    if (obj->field5c == 0) {
        h = (MK3THREADFUNC)t_return_to_beware;
    } else {
        get_x_dist(obj);
        if ((int32_t)obj->field28 > 0x9f)
            h = (MK3THREADFUNC)t_drone_sweep_closeup_sd;
        else
            h = (MK3THREADFUNC)t_attack_closeup_sd;
    }
    return mk3_install(thread, h);
}

long c_jaxdash_sd(MK3THREAD *thread)
{
    MK3OBJ       *obj = (MK3OBJ *)thread->proc;
    MK3THREADFUNC h;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    should_i_promove(obj);
    if (obj->field5c == 0) {
        h = (MK3THREADFUNC)t_return_to_beware;
    } else {
        get_x_dist(obj);
        if ((int32_t)obj->field28 > 0x56)
            h = (MK3THREADFUNC)t_run_in_close;
        else
            h = (MK3THREADFUNC)t_d_knee;
    }
    return mk3_install(thread, h);
}

long t_scorp_anti_orb(MK3THREAD *thread)
{
    MK3OBJ       *obj = (MK3OBJ *)thread->proc;
    MK3THREADFUNC h;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    get_x_dist(obj);
    if ((int32_t)obj->field28 <= 0xaf) {
        h = (MK3THREADFUNC)t_run_in_and_slam;
    } else {
        get_x_dist(obj);
        if ((int32_t)obj->field28 > 0xf0)
            h = (MK3THREADFUNC)t_d_propell_attack_now;
        else
            h = (MK3THREADFUNC)t_block_orb;
    }
    return mk3_install(thread, h);
}

/* q_proj_jclose -- armv7 0x0006ff80: is his projectile within reach of a
 * jump. The reach is 0xc0, or 0x90 unless he is character 0xe, 0x12 or
 * 0x16 (the three whose projectiles travel far). */
void q_proj_jclose(MK3OBJ *obj)
{
    uint32_t c;
    int32_t  d;

    his_proj_front_x(obj);
    obj->field30 = 0xc0;
    c = ((MK3OBJ *)(uintptr_t)obj->field00->him)->field24;
    obj->field24 = c;
    if (!(c == 0xe || c == 0x12) && c != 0x16)
        obj->field30 = 0x90;
    obj->field24 = (uint32_t)(int32_t)MK3_FIELD0E_S(obj->field08);
    d = (int32_t)obj->field24 - (int32_t)obj->field28;
    obj->field28 = (uint32_t)d;
    if (d <= (int32_t)obj->field30)
        goto yes;
    vq_no(obj);
    return;
yes:
    vq_yes(obj);
}

/* ======================================================================
 * Duck/fan/stance setups, flip watchers and the multi-state net/turn threads.
 * ====================================================================== */

long t_mframew(struct MK3THREAD *thread);
long t_scan_flip_kick(struct MK3THREAD *thread);
long t_dont_zap_towards_jumper(struct MK3THREAD *thread);
long t_very_far_airborn(struct MK3THREAD *thread);
long t_d_fflip_noscan_jsrp(struct MK3THREAD *thread);
long t_watch_flip_punch(struct MK3THREAD *thread);
long t_watch_flip_kick(struct MK3THREAD *thread);
long t_retreat_wait_yes(struct MK3THREAD *thread);
long t_d_turnaround_jsrp(struct MK3THREAD *thread);
long t_drone_zone(struct MK3THREAD *thread);
long rpt_promoves(struct MK3THREAD *thread);
long t_d_fflip_scan_jsrp(struct MK3THREAD *thread);
long t_fflip_watchout(struct MK3THREAD *thread);
void stance_setup(MK3OBJ *obj);
void do_next_a9_frame(MK3OBJ *obj);
void get_char_ani(MK3OBJ *obj);
void face_opponent(MK3OBJ *obj);
void distance_off_ground(MK3OBJ *obj);
long q_no(MK3OBJ *obj);

/* t_d_duck_fast -- armv7 0x000715b4 */
long t_d_duck_fast(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field20 = 0x302;
    obj->field00->field18 = 0x302;
    stop_me_player(obj);
    face_opponent(obj);
    obj->field40 = 4;
    get_char_ani(obj);
    obj->field1c = 1;
    return mk3_install(thread, (MK3THREADFUNC)t_mframew);
}

/* c_air_fan -- armv7 0x0006aa28: how far his y is from the floor decides */
long c_air_fan(MK3THREAD *thread)
{
    MK3OBJ       *obj = (MK3OBJ *)thread->proc;
    MK3THREADFUNC h;
    int32_t       y, d;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    y = MK3_FIELD12_S((MK3OBJ *)(uintptr_t)obj->field00->him);
    obj->field28 = (uint32_t)y;
    d = (int32_t)*(const uint32_t *)(const void *)(G_BYTES + 0xac) - y;
    obj->field1c = (uint32_t)d;
    if (d > 0xa0)
        h = (MK3THREADFUNC)t_d_zap_now;
    else
        h = (MK3THREADFUNC)t_duck_under_proj;
    return mk3_install(thread, h);
}

/* d_stance_setup -- armv7 0x00071e0c: step the animation script back one
 * word (skipping runs of 8-opcodes five words long), then scan forward to the
 * next 1 and keep the pointer one word before it; the old pointer is
 * restored when it lies between the two */
void d_stance_setup(MK3OBJ *obj)
{
    const uint32_t *p, *q, *last;
    uint32_t        v, saved;

    stop_me_player(obj);
    obj->field30 = obj->field40;
    stance_setup(obj);
    do_next_a9_frame(obj);

    p = (const uint32_t *)(uintptr_t)obj->field40;
    p = p - 1;
    obj->field40 = (uint32_t)(uintptr_t)p;
    v = *p;
    obj->field2c = v;
    while (v == 8) {
        const uint32_t *old = p;

        p = old + 5;
        obj->field40 = (uint32_t)(uintptr_t)p;
        v = *p;
        obj->field2c = v;
    }

    q = p;
    obj->field2c = (uint32_t)(uintptr_t)q;
    do {
        last = q;
        v = *q++;
        obj->field20 = v;
        obj->field2c = (uint32_t)(uintptr_t)q;
    } while (v != 1);

    saved = obj->field30;
    obj->field2c = (uint32_t)(uintptr_t)(last - 1);
    if ((int32_t)saved >= (int32_t)(uintptr_t)p &&
        (int32_t)(uintptr_t)(last - 1) >= (int32_t)saved)
        obj->field40 = saved;
}

/* t_fflip_scan -- armv7 0x0006e754: close enough scans, otherwise the level
 * pops (or becomes t_local_reaction_exit at the bottom) */
long t_fflip_scan(MK3THREAD *thread)
{
    MK3OBJ       *obj = (MK3OBJ *)thread->proc;
    MK3THREADFUNC h;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    get_x_dist(obj);
    if ((int32_t)obj->field28 <= 0x6f) {
        h = (MK3THREADFUNC)t_scan_flip_kick;
    } else {
        if ((long)thread->frame > 0) {
            thread->frame = thread->frame - 1;
            return 0;
        }
        h = (MK3THREADFUNC)t_local_reaction_exit;
    }
    return mk3_install(thread, h);
}

/* c_swat_gun -- armv7 0x0006d350 */
long c_swat_gun(MK3THREAD *thread)
{
    MK3OBJ       *obj = (MK3OBJ *)thread->proc;
    MK3THREADFUNC h;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = (uint32_t)(uintptr_t)rpt_counter;
    ask_mr_diff(obj);
    if (obj->field5c == 0) {
        h = (MK3THREADFUNC)t_return_to_beware;
    } else {
        get_x_dist(obj);
        if ((int32_t)obj->field28 > 0xd0)
            h = (MK3THREADFUNC)t_d_zap_now;
        else
            h = (MK3THREADFUNC)t_d_block;
    }
    return mk3_install(thread, h);
}

/* c_floor_ice -- armv7 0x0006dfd4 */
long c_floor_ice(MK3THREAD *thread)
{
    MK3OBJ       *obj = (MK3OBJ *)thread->proc;
    MK3THREADFUNC h;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = (uint32_t)(uintptr_t)rpt_promoves;
    ask_mr_diff(obj);
    if (obj->field5c == 0) {
        h = (MK3THREADFUNC)t_return_to_beware;
    } else {
        get_x_dist(obj);
        if ((int32_t)obj->field28 > 0x6f)
            h = (MK3THREADFUNC)t_drone_zone;
        else
            h = (MK3THREADFUNC)t_run_in_close;
    }
    return mk3_install(thread, h);
}

/* t_d_attack_very_far -- armv7 0x00070bf4: two install sites, as the binary
 * has them */
long t_d_attack_very_far(MK3THREAD *thread)
{
    MK3OBJ       *obj = (MK3OBJ *)thread->proc;
    MK3THREADFUNC h;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    is_towards_me(obj);
    if (obj->field5c == 0) {
        h = (MK3THREADFUNC)t_dont_zap_towards_jumper;
    } else {
        q_airborn_counter(obj);
        if (obj->field5c == 0)
            return mk3_install(thread, (MK3THREADFUNC)t_dont_zap_towards_jumper);
        h = (MK3THREADFUNC)t_very_far_airborn;
    }
    return mk3_install(thread, h);
}

/* t_d_fflip_jsrp -- armv7 0x00070de0 */
long t_d_fflip_jsrp(MK3THREAD *thread)
{
    MK3OBJ       *obj = (MK3OBJ *)thread->proc;
    MK3THREADFUNC h;
    int32_t       d;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    frontflip_setup(obj);
    d = *(const int16_t *)(const void *)(G_BYTES + 0x44c);
    obj->field1c = (uint32_t)d;
    if (d > 3) {
        obj->field34 = (uint32_t)(uintptr_t)t_fflip_watchout;
        h = (MK3THREADFUNC)t_d_fflip_scan_jsrp;
    } else {
        h = (MK3THREADFUNC)t_d_fflip_noscan_jsrp;
    }
    return mk3_install(thread, h);
}

/* t_fflip_watchout -- armv7 0x000705fc: refuses while he is far (over 0xa0),
 * then picks the watch by whether he is in the air */
long t_fflip_watchout(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    get_x_dist(obj);
    if ((int32_t)obj->field28 > 0xa0)
        return -3;

    reset_proc_stack(thread);
    is_he_airborn(obj);
    if (obj->field5c != 0)
        return mk3_install(thread, (MK3THREADFUNC)t_watch_flip_punch);
    return mk3_install(thread, (MK3THREADFUNC)t_watch_flip_kick);
}

/* t_tusk_jup_scan -- armv7 0x00070758 */
long t_tusk_jup_scan(MK3THREAD *thread)
{
    MK3OBJ       *obj = (MK3OBJ *)thread->proc;
    MK3THREADFUNC h;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    distance_off_ground(obj);
    if ((int32_t)obj->field1c > 0x3f) {
        reset_proc_stack(thread);
        h = (MK3THREADFUNC)t_d_zap_now;
    } else {
        q_no(obj);
        if ((long)thread->frame > 0) {
            thread->frame = thread->frame - 1;
            return 0;
        }
        h = (MK3THREADFUNC)t_local_reaction_exit;
    }
    return mk3_install(thread, h);
}

/* t_robo2_delayed_net -- armv7 0x00069a60: state 0 sets the net test and
 * waits at 0xce7 behind t_retreat_wait_yes; 0xce7 becomes t_d_zap */
long t_robo2_delayed_net(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->a10 = 0x40;
        obj->field48 = (uint32_t)(uintptr_t)q_is_he_net_close;
        *mk3_frame(thread, thread->frame + 1) = 0xce7;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_retreat_wait_yes);
    }
    if (token != 0xce7)
        return -3;
    return mk3_install(thread, (MK3THREADFUNC)t_d_zap);
}

/* t_d_turnaround -- armv7 0x00070674: push the jsrp turnaround and come back
 * at 0x1bf to reset and leave */
long t_d_turnaround(MK3THREAD *thread)
{
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x1bf;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_d_turnaround_jsrp);
    }
    if (token != 0x1bf)
        return -3;
    reset_proc_stack(thread);
    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

/* ======================================================================
 * Two-state deciders: state 0 pushes a wait and refuses (-3 abandons this
 * frame; the pushed level runs on the next), the resume token chooses.
 * ====================================================================== */

long t_d_bflip_jump(struct MK3THREAD *thread);
long t_d_open_jumpover(struct MK3THREAD *thread);
long t_do_jumpup_kick(struct MK3THREAD *thread);
long funcs_13699(struct MK3THREAD *thread);
long funcs_13782(struct MK3THREAD *thread);
long c_tusk_blur(struct MK3THREAD *thread);
long t_nr_sweep_if_u_can(struct MK3THREAD *thread);
long t_d_block_projectile(struct MK3THREAD *thread);
long t_d_flipk_over_proj(struct MK3THREAD *thread);
long t_avoid_agressive_bastards(struct MK3THREAD *thread);
long t_d_retreat_a11(struct MK3THREAD *thread);

/* t_jump_up_kick_scan -- armv7 0x000706e8 */
long t_jump_up_kick_scan(MK3THREAD *thread)
{
    MK3OBJ       *obj = (MK3OBJ *)thread->proc;
    MK3THREADFUNC h;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    get_x_dist(obj);
    if ((int32_t)obj->field28 <= 0x90) {
        reset_proc_stack(thread);
        h = (MK3THREADFUNC)t_do_jumpup_kick;
    } else {
        if ((long)thread->frame > 0) {
            thread->frame = thread->frame - 1;
            return 0;
        }
        h = (MK3THREADFUNC)t_local_reaction_exit;
    }
    return mk3_install(thread, h);
}

/* c_duck_kickl -- armv7 0x000708bc */
long c_duck_kickl(MK3THREAD *thread)
{
    MK3OBJ       *obj = (MK3OBJ *)thread->proc;
    MK3THREADFUNC h;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    q_will_he_reach_me(obj);
    if (obj->field5c == 0) {
        h = (MK3THREADFUNC)t_return_to_beware;
    } else {
        q_is_he_cornered(obj);
        if (obj->field5c != 0) {
            h = (MK3THREADFUNC)t_d_crossover_kick;
        } else {
            *(uint32_t *)((char *)obj + 0x68) = (uint32_t)(uintptr_t)funcs_13699;
            h = (MK3THREADFUNC)t_react_jump_table_act;
        }
    }
    return mk3_install(thread, h);
}

/* t_d_get_open -- armv7 0x000712b0: two candidate jumps in field24/field38,
 * ordered by which edge is nearer; the opponent's side swaps them */
long t_d_get_open(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t h, a, b;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    get_my_dfe(obj);
    a = (uint32_t)(uintptr_t)t_d_bflip_jump;
    b = (uint32_t)(uintptr_t)t_d_open_jumpover;
    obj->field24 = a;
    obj->field38 = b;
    if ((int32_t)obj->field34 <= (int32_t)obj->field30) {
        obj->field24 = b;
        obj->field38 = a;
    }
    is_he_right(obj);
    if (obj->field5c != 0) {
        h = obj->field38;
    } else {
        uint32_t x = obj->field24, y = obj->field38;

        obj->field38 = x;
        obj->field24 = y;
        h = x;
    }
    return mk3_install(thread, (MK3THREADFUNC)(uintptr_t)h);
}

/* c_react_flipk -- armv7 0x0006cb6c: the handler comes out of tab_react_flipk
 * for characters up to 0x17 */
long c_react_flipk(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t h;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    should_i_promove(obj);
    if (obj->field5c == 0) {
        h = (uint32_t)(uintptr_t)t_return_to_beware;
    } else if (((MK3OBJ *)(uintptr_t)obj->field00->him)->field24 > 0x17) {
        h = (uint32_t)(uintptr_t)t_return_to_beware;
    } else {
        h = tab_react_flipk[obj->field08->field24];
        obj->field1c = h;
    }
    return mk3_install(thread, (MK3THREADFUNC)(uintptr_t)h);
}

/* The three `_pro` / hat deciders: state 0 pushes t_nr_sweep_if_u_can and
 * refuses, the resume token measures the distance. */
long c_bombhi_pro(MK3THREAD *thread)
{
    MK3OBJ       *obj = (MK3OBJ *)thread->proc;
    MK3THREADFUNC h;
    uint32_t      token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0xe13;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_nr_sweep_if_u_can);
    }
    if (token != 0xe13)
        return -3;

    get_x_dist(obj);
    if ((int32_t)obj->field28 < 0x110)
        h = (MK3THREADFUNC)t_duck_under_proj;
    else
        h = (MK3THREADFUNC)t_run_in_close;
    return mk3_install(thread, h);
}

long c_bomblo_pro(MK3THREAD *thread)
{
    MK3OBJ       *obj = (MK3OBJ *)thread->proc;
    MK3THREADFUNC h;
    uint32_t      token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0xe05;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_nr_sweep_if_u_can);
    }
    if (token != 0xe05)
        return -3;

    get_x_dist(obj);
    if ((int32_t)obj->field28 > 0xaf)
        h = (MK3THREADFUNC)t_d_fflip_kick_jump;
    else
        h = (MK3THREADFUNC)t_duck_under_proj;
    return mk3_install(thread, h);
}

long c_hat(MK3THREAD *thread)
{
    MK3OBJ       *obj = (MK3OBJ *)thread->proc;
    MK3THREADFUNC h;
    uint32_t      token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0xdad;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_nr_sweep_if_u_can);
    }
    if (token != 0xdad)
        return -3;

    get_x_dist(obj);
    if ((int32_t)obj->field28 > 0xbf)
        h = (MK3THREADFUNC)t_d_flipk_over_proj;
    else
        h = (MK3THREADFUNC)t_d_block_projectile;
    return mk3_install(thread, h);
}

/* c_elbow -- armv7 0x0006bcc4: push t_avoid_agressive_bastards and refuse;
 * on 0x137e set the slave word and become t_react_jump_table_act */
long c_elbow(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x137e;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_avoid_agressive_bastards);
    }
    if (token != 0x137e)
        return -3;

    *(uint32_t *)((char *)obj + 0x68) = (uint32_t)(uintptr_t)funcs_13782;
    return mk3_install(thread, (MK3THREADFUNC)t_react_jump_table_act);
}

/* t_ct_kswipe -- armv7 0x0006d050: two install sites */
long t_ct_kswipe(MK3THREAD *thread)
{
    MK3OBJ       *obj = (MK3OBJ *)thread->proc;
    MK3THREADFUNC h;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    get_x_dist(obj);
    if ((int32_t)obj->field28 <= 0x8f) {
        h = (MK3THREADFUNC)t_d_block;
    } else {
        should_i_promove(obj);
        if (obj->field5c == 0)
            return mk3_install(thread, (MK3THREADFUNC)t_swait_nonattack_jump);
        h = (MK3THREADFUNC)t_d_zap;
    }
    return mk3_install(thread, h);
}

/* t_d_backoff_a_bit -- armv7 0x00067a30: state 0 sets a10/field48 (0x40,
 * 0x80), pushes t_d_retreat_a11 and refuses; 0x303 leaves */
long t_d_backoff_a_bit(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->a10     = 0x40;
        obj->field48 = 0x40 + 0x40;
        *mk3_frame(thread, thread->frame + 1) = 0x303;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_d_retreat_a11);
    }
    if (token != 0x303)
        return -3;
    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

/* ======================================================================
 * The two-state deciders, generated from one template and each checked.
 *
 * State 0 does its setup, pushes a wait routine one level up with a resume
 * token, and refuses with -3 -- which abandons this frame; the pushed level
 * runs on the next. When the wait comes back at the token, the second state
 * installs the real handler.
 * ====================================================================== */

long t_d_run_till_yes(struct MK3THREAD *thread);
long t_stance_wait_yes(struct MK3THREAD *thread);
long t_close_airborn(struct MK3THREAD *thread);
long t_nr_attack_sd(struct MK3THREAD *thread);
long t_flipp_scan(struct MK3THREAD *thread);
long t_jump_up_kick_scan(struct MK3THREAD *thread);
long t_do_jump_up(struct MK3THREAD *thread);
long t_do_flip_kick(struct MK3THREAD *thread);
long t_d_flip_punch_jump(struct MK3THREAD *thread);
long t_d_propell_attack(struct MK3THREAD *thread);
long t_nr_uppercut_if_u_can(struct MK3THREAD *thread);
long t_d_hi_kick(struct MK3THREAD *thread);
long t_d_duck(struct MK3THREAD *thread);
long t_d_duck_then_uppercut(struct MK3THREAD *thread);
long t_d_sweep_kick(struct MK3THREAD *thread);
long funcs_8223(struct MK3THREAD *thread);
long funcs_8113(struct MK3THREAD *thread);
void q_backup_zap(MK3OBJ *obj);
void q_run_then_flipk(MK3OBJ *obj);
void q_run_then_duck(MK3OBJ *obj);
void q_run_under_fk(MK3OBJ *obj);
void is_propell_close(MK3OBJ *obj);
void frontflip_setup(MK3OBJ *obj);
void d_stance_setup(MK3OBJ *obj);
void next_anirate(MK3OBJ *obj);

long t_run_then_flipk(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->a10     = 0x30;
        obj->field48 = (uint32_t)(uintptr_t)q_run_then_flipk;
        *mk3_frame(thread, thread->frame + 1) = 0xeb7;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_d_run_till_yes);
    }
    if (token != 0xeb7)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_d_fflip_kick_jump);
}

long t_run_then_duck_under(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->a10     = 0x30;
        obj->field48 = (uint32_t)(uintptr_t)q_run_then_duck;
        *mk3_frame(thread, thread->frame + 1) = 0xecf;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_d_run_till_yes);
    }
    if (token != 0xecf)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_duck_under_proj);
}

long t_run_under_flykick(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->a10     = 0x30;
        obj->field48 = (uint32_t)(uintptr_t)q_run_under_fk;
        *mk3_frame(thread, thread->frame + 1) = 0x12ef;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_d_run_till_yes);
    }
    if (token != 0x12ef)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_d_flip_punch_jump);
}

long t_ct_propell(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field48 = (uint32_t)(uintptr_t)is_propell_close;
        obj->a10     = 0x30;
        *mk3_frame(thread, thread->frame + 1) = 0x1263;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_stance_wait_yes);
    }
    if (token != 0x1263)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_d_block);
}

long t_d_backup_zap(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->a10     = 0x40;
        obj->field48 = (uint32_t)(uintptr_t)q_backup_zap;
        *mk3_frame(thread, thread->frame + 1) = 0xcfc;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_retreat_wait_yes);
    }
    if (token != 0xcfc)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_d_zap);
}

long t_attack_close_hard(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *(uint32_t *)((char *)obj + 0x68) = (uint32_t)(uintptr_t)funcs_8223;
        *(uint32_t *)((char *)obj + 0x64) = 5;
        *mk3_frame(thread, thread->frame + 1) = 0x403;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_random_do);
    }
    if (token != 0x403)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_close_airborn);
}

long t_attack_very_far_hard(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *(uint32_t *)((char *)obj + 0x68) = (uint32_t)(uintptr_t)funcs_8113;
        *(uint32_t *)((char *)obj + 0x64) = 3;
        *mk3_frame(thread, thread->frame + 1) = 0x3c0;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_random_do);
    }
    if (token != 0x3c0)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_very_far_airborn);
}

long c_sbike_sd(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    MK3THREADFUNC h;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x1082;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_nr_attack_sd);
    }
    if (token != 0x1082)
        return -3;

    get_x_dist(obj);
    if ((int32_t)obj->field28 > 0xd0)
        h = (MK3THREADFUNC)t_d_zap;
    else
        h = (MK3THREADFUNC)t_d_fflip_kick_jump;
    return mk3_install(thread, h);
}

long t_counter_grounded_sd(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    MK3THREADFUNC h;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x1130;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_nr_attack_sd);
    }
    if (token != 0x1130)
        return -3;

    get_x_dist(obj);
    if ((int32_t)obj->field28 > 0xb0)
        h = (MK3THREADFUNC)t_d_zap;
    else
        h = (MK3THREADFUNC)t_d_propell_attack;
    return mk3_install(thread, h);
}

long t_d_flip_punch_jump(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        frontflip_setup(obj);
        obj->field34 = (uint32_t)(uintptr_t)t_flipp_scan;
        *mk3_frame(thread, thread->frame + 1) = 0x4c3;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_d_fflip_scan_jsrp);
    }
    if (token != 0x4c3)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

long t_d_jump_up_kick(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field48 = (uint32_t)(uintptr_t)t_jump_up_kick_scan;
        *mk3_frame(thread, thread->frame + 1) = 0x156;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_do_jump_up);
    }
    if (token != 0x156)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

long t_scan_flip_kick(MK3THREAD *thread)
{
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        reset_proc_stack(thread);
        *mk3_frame(thread, thread->frame + 1) = 0x6a4;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_do_flip_kick);
    }
    if (token != 0x6a4)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

/* t_drone_babality -- armv7 0x00071e68: face him, set the stance, count 0x40
 * frames of next_anirate parked at 0x999, then t_do_babality */
long tl_do_babality(struct MK3THREAD *thread);

long t_drone_babality(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        face_opponent(obj);
        d_stance_setup(obj);
        obj->a10 = 0x40;
        goto park;
    }
    if (token != 0x999)
        return -3;
    obj->a10 = obj->a10 - 1;
    if ((int32_t)obj->a10 != 0)
        goto park;
    return mk3_install(thread, (MK3THREADFUNC)tl_do_babality);

park:
    next_anirate(obj);
    *mk3_frame(thread, thread->frame + 1) = 0x999;
    thread->fieldfc = 1;
    return 1;
}

/* c_ermac_slam -- armv7 0x0006ddc8 */
long c_ermac_slam(MK3THREAD *thread)
{
    MK3OBJ       *obj = (MK3OBJ *)thread->proc;
    MK3THREADFUNC h;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = (uint32_t)(uintptr_t)rpt_promoves;
    ask_mr_diff(obj);
    if (obj->field5c == 0) {
        h = (MK3THREADFUNC)t_return_to_beware;
    } else {
        get_x_dist(obj);
        if ((int32_t)obj->field28 <= 0x6f)
            h = (MK3THREADFUNC)t_run_in_close;
        else if ((int32_t)obj->field28 > 0xf0)
            h = (MK3THREADFUNC)t_d_zap_now;
        else
            h = (MK3THREADFUNC)t_d_block;
    }
    return mk3_install(thread, h);
}

/* t_ct_kicks -- armv7 0x0006e940: ducking at 0x104 else a strike test picks
 * between the uppercut and the sweep */
long t_ct_kicks(MK3THREAD *thread)
{
    MK3OBJ       *obj = (MK3OBJ *)thread->proc;
    MK3THREADFUNC h;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    get_his_action(obj);
    if (obj->field20 == 0x104) {
        h = (MK3THREADFUNC)t_d_duck;
    } else {
        obj->field1c = 8;
        strike_check_a0_test(obj);
        if (obj->field5c == 0)
            return mk3_install(thread, (MK3THREADFUNC)t_d_sweep_kick);
        h = (MK3THREADFUNC)t_d_duck_then_uppercut;
    }
    return mk3_install(thread, h);
}

/* c_robo_tele_sd -- armv7 0x0006abe8: three states, each pushing the next
 * wait and returning 0 so it runs at once */
long c_robo_tele_sd(MK3THREAD *thread)
{
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x105e) {
        *mk3_frame(thread, thread->frame + 1) = 0x105f;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_nr_uppercut_if_u_can);
    }
    if (token == 0x105f)
        return mk3_install(thread, (MK3THREADFUNC)t_d_hi_kick);
    if (token != 0)
        return -3;
    *mk3_frame(thread, thread->frame + 1) = 0x105e;
    thread->frame = thread->frame + 1;
    return mk3_install(thread, (MK3THREADFUNC)t_nr_attack_sd);
}

/* ======================================================================
 * Argument-stack deciders (ckik3, cpch3), waits and scans.
 * ====================================================================== */

long t_kick_will_miss(struct MK3THREAD *thread);
long t_react_jump_table(struct MK3THREAD *thread);
long funcs_14030(struct MK3THREAD *thread);
long funcs_13934(struct MK3THREAD *thread);
long t_d_stalk_a11_ntl(struct MK3THREAD *thread);
long t_stsw_zap(struct MK3THREAD *thread);
long t_stance_wait_no(struct MK3THREAD *thread);
long t_tusk_jup_scan(struct MK3THREAD *thread);
long t_return_to_beware_4get(struct MK3THREAD *thread);
long is_throwing_allowed(MK3OBJ *obj);
void is_he_attacking(MK3OBJ *obj);
long t_d_fflip_jsrp(struct MK3THREAD *thread);

/* ckik3 -- armv7 0x0006ea94: keep field20 on the argument stack across the
 * reach test, then react through the jump table with funcs.14030, or miss */
long ckik3(MK3THREAD *thread)
{
    MK3OBJ       *obj = (MK3OBJ *)thread->proc;
    MK3THREADFUNC h;
    uint32_t      n;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    n = thread->fieldf8;
    *mk3_arg(thread, n) = obj->field20;
    thread->fieldf8 = n + 1;
    q_will_he_reach_me(obj);
    n = thread->fieldf8 - 1;
    thread->fieldf8 = n;
    obj->field20 = *mk3_arg(thread, n);
    if (obj->field5c == 0) {
        h = (MK3THREADFUNC)t_kick_will_miss;
    } else {
        *(uint32_t *)((char *)obj + 0x68) = (uint32_t)(uintptr_t)funcs_14030;
        h = (MK3THREADFUNC)t_react_jump_table;
    }
    return mk3_install(thread, h);
}

/* cpch3 -- armv7 0x0006d1dc: the same with the distance (over 0x60 forgets) */
long cpch3(MK3THREAD *thread)
{
    MK3OBJ       *obj = (MK3OBJ *)thread->proc;
    MK3THREADFUNC h;
    uint32_t      n;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    n = thread->fieldf8;
    *mk3_arg(thread, n) = obj->field20;
    thread->fieldf8 = n + 1;
    get_x_dist(obj);
    n = thread->fieldf8 - 1;
    thread->fieldf8 = n;
    obj->field20 = *mk3_arg(thread, n);
    if ((int32_t)obj->field28 > 0x60) {
        h = (MK3THREADFUNC)t_return_to_beware_4get;
    } else {
        *(uint32_t *)((char *)obj + 0x68) = (uint32_t)(uintptr_t)funcs_13934;
        h = (MK3THREADFUNC)t_react_jump_table;
    }
    return mk3_install(thread, h);
}

/* t_d_get_close_2_u -- armv7 0x0006e69c: far (over 0xff) pushes the forward
 * flip and comes back at 0xa82; either way the close action is the same */
long t_d_get_close_2_u(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        get_x_dist(obj);
        if ((int32_t)obj->field28 > 0xff) {
            *mk3_frame(thread, thread->frame + 1) = 0xa82;
            thread->frame = thread->frame + 1;
            return mk3_install(thread, (MK3THREADFUNC)t_d_fflip_jsrp);
        }
    } else if (token != 0xa82) {
        return -3;
    }
    obj->field48 = 0x40;
    return mk3_install(thread, (MK3THREADFUNC)t_d_stalk_a11_ntl);
}

/* t_d_wait_nonattack -- armv7 0x0006c77c: park a frame at a time at 0x779
 * for a10 frames while he is attacking, then pop or leave */
long t_d_wait_nonattack(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0)
        goto park;
    if (token != 0x779)
        return -3;

    obj->a10 = obj->a10 - 1;
    if (obj->a10 != 0) {
        is_he_attacking(obj);
        if (obj->field5c != 0)
            goto park;
    }
    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
        return 0;
    }
    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

park:
    *mk3_frame(thread, thread->frame + 1) = 0x779;
    thread->fieldfc = 1;
    return 1;
}

/* t_tusk_jump_up_zap -- armv7 0x000699ac */
long t_tusk_jump_up_zap(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field48 = (uint32_t)(uintptr_t)t_tusk_jup_scan;
        *mk3_frame(thread, thread->frame + 1) = 0xcd3;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_do_jump_up);
    }
    if (token != 0xcd3)
        return -3;
    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

/* t_swait_nonattack_jump -- armv7 0x0006b308 */
long t_swait_nonattack_jump(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field48 = (uint32_t)(uintptr_t)is_he_attacking;
        obj->a10     = 0x40;
        *mk3_frame(thread, thread->frame + 1) = 0x11c8;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_stance_wait_no);
    }
    if (token != 0x11c8)
        return -3;
    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

/* t_crossover_scan -- armv7 0x00068640: my x against his, ordered by the
 * sign of my part's field18; in order it scans the flip kick, out of order
 * it pops (or leaves at the bottom) */
long t_crossover_scan(MK3THREAD *thread)
{
    MK3OBJ       *obj = (MK3OBJ *)thread->proc;
    MK3OBJ       *part = obj->field08;
    MK3OBJ       *him  = (MK3OBJ *)(uintptr_t)obj->field00->him;
    MK3THREADFUNC h;
    int32_t       mine, his, v;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    mine = MK3_FIELD0E_S(part);
    obj->field20 = (uint32_t)mine;
    his = MK3_FIELD0E_S(him);
    obj->field24 = (uint32_t)his;
    v = (int32_t)part->field18;
    obj->field28 = (uint32_t)v;
    if (v <= 0) {
        obj->field20 = (uint32_t)his;
        obj->field24 = (uint32_t)mine;
    }
    if ((int32_t)obj->field20 >= (int32_t)obj->field24) {
        h = (MK3THREADFUNC)t_scan_flip_kick;
    } else {
        if ((long)thread->frame > 0) {
            thread->frame = thread->frame - 1;
            return 0;
        }
        h = (MK3THREADFUNC)t_local_reaction_exit;
    }
    return mk3_install(thread, h);
}

/* t_ct_stick_sweep -- armv7 0x0006d57c */
long t_ct_stick_sweep(MK3THREAD *thread)
{
    MK3OBJ       *obj = (MK3OBJ *)thread->proc;
    MK3THREADFUNC h;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    is_throwing_allowed(obj);
    if (obj->field5c != 0)
        return mk3_install(thread, (MK3THREADFUNC)t_stsw_zap);

    get_x_dist(obj);
    if ((int32_t)obj->field28 > 0x7f)
        h = (MK3THREADFUNC)t_d_block;
    else
        h = (MK3THREADFUNC)t_d_crossover_kick;
    return mk3_install(thread, h);
}

/* ======================================================================
 * Three-token deciders, the finish-him chain and the zap picker.
 * ====================================================================== */

extern uint32_t *RoundParam;            /* the pointer slot, as in other.c */
extern const int32_t ochar_zaps[];
long t_boss_branch(struct MK3THREAD *thread);
long t_check_winner_status(struct MK3THREAD *thread);
long t_drone_execute_fatality(struct MK3THREAD *thread);
long t_non_violent_finish(struct MK3THREAD *thread);
long t_d_quake(struct MK3THREAD *thread);
long t_d_uppercut(struct MK3THREAD *thread);
long t_do_zap(struct MK3THREAD *thread);
long t_do_duck(struct MK3THREAD *thread);
long t_d_get_close_2_u(struct MK3THREAD *thread);
long t_stance_wait_yes(struct MK3THREAD *thread);
long t_retreat_wait_yes(struct MK3THREAD *thread);
long funcs_10049(struct MK3THREAD *thread);

/* t_drone_zone -- armv7 0x0006e8b4: state 0 arms q_drone_zone (field48) with
 * a10 = 0x80 and waits; at 0x47c the distance is measured (for its side
 * effect in field28) and the fighter runs in close */
long t_drone_zone(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field48 = (uint32_t)(uintptr_t)q_drone_zone;
        obj->a10     = 0x80;
        *mk3_frame(thread, thread->frame + 1) = 0x47c;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_stance_wait_yes);
    }
    if (token != 0x47c)
        return -3;

    get_x_dist(obj);
    return mk3_install(thread, (MK3THREADFUNC)t_run_in_close);
}

/* t_d_finish_him -- armv7 0x00068ec8: slave word 0x11, push t_boss_branch
 * and wait at 0x947; then field5c = 1 and either the fatality or, when
 * RoundParam[0x40] is set, the non-violent finish (with field5c back to 0) */
long t_d_finish_him(MK3THREAD *thread)
{
    MK3OBJ       *obj = (MK3OBJ *)thread->proc;
    MK3THREADFUNC h;
    uint32_t      token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *(uint32_t *)((char *)obj + 0x64) = 0x11;
        *mk3_frame(thread, thread->frame + 1) = 0x947;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_boss_branch);
    }
    if (token != 0x947)
        return -3;

    obj->field5c = 1;
    if (RoundParam[0x40 / 4] != 0) {
        obj->field5c = 0;
        h = (MK3THREADFUNC)t_non_violent_finish;
    } else {
        h = (MK3THREADFUNC)t_drone_execute_fatality;
    }
    return mk3_install(thread, h);
}

/* t_d_open_jumpover -- armv7 0x00068a28: 0 pushes the forward flip and waits
 * at 0x7d7; 0x7d7 pushes the winner check and waits at 0x7d8; 0x7d8 becomes
 * the backward flip */
long t_d_open_jumpover(MK3THREAD *thread)
{
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x7d7) {
        *mk3_frame(thread, thread->frame + 1) = 0x7d8;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_check_winner_status);
    }
    if (token == 0x7d8)
        return mk3_install(thread, (MK3THREADFUNC)t_d_bflip_jump);
    if (token != 0)
        return -3;
    *mk3_frame(thread, thread->frame + 1) = 0x7d7;
    thread->frame = thread->frame + 1;
    return mk3_install(thread, (MK3THREADFUNC)t_d_fflip_jsrp);
}

/* t_jax_delayed_quake -- armv7 0x0006e384 */
long t_jax_delayed_quake(MK3THREAD *thread)
{
    MK3OBJ       *obj = (MK3OBJ *)thread->proc;
    MK3THREADFUNC h;
    uint32_t      token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->a10     = 0x40;
        obj->field48 = (uint32_t)(uintptr_t)q_jax_smash;
        *mk3_frame(thread, thread->frame + 1) = 0xd38;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_retreat_wait_yes);
    }
    if (token != 0xd38)
        return -3;

    get_x_dist(obj);
    if ((int32_t)obj->field28 > 0xaf)
        h = (MK3THREADFUNC)t_d_quake;
    else
        h = (MK3THREADFUNC)t_local_reaction_exit;
    return mk3_install(thread, h);
}

/* t_non_violent_finish -- armv7 0x00068f54 */
long t_non_violent_finish(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x96b;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_d_get_close_2_u);
    }
    if (token != 0x96b)
        return -3;

    *(uint32_t *)((char *)obj + 0x68) = (uint32_t)(uintptr_t)funcs_10049;
    *(uint32_t *)((char *)obj + 0x64) = 3;
    *mk3_frame(thread, thread->frame + 1) = 0x975;
    thread->frame = thread->frame + 1;
    return mk3_install(thread, (MK3THREADFUNC)t_random_do);
}

/* c_sky_pro -- armv7 0x0006a3e0: 0 pushes t_nr_sweep_if_u_can (wait 0xe20);
 * 0xe20 arms q_is_proj_gone, a10 = 0x30, pushes t_stance_wait_no (wait
 * 0xe23); 0xe23 runs in close */
long c_sky_pro(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0xe20) {
        obj->field48 = (uint32_t)(uintptr_t)q_is_proj_gone;
        obj->a10     = 0x30;
        *mk3_frame(thread, thread->frame + 1) = 0xe23;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_stance_wait_no);
    }
    if (token == 0xe23)
        return mk3_install(thread, (MK3THREADFUNC)t_run_in_close);
    if (token != 0)
        return -3;
    *mk3_frame(thread, thread->frame + 1) = 0xe20;
    thread->frame = thread->frame + 1;
    return mk3_install(thread, (MK3THREADFUNC)t_nr_sweep_if_u_can);
}

/* t_d_duck_then_uppercut -- armv7 0x00068b5c */
long t_d_duck_then_uppercut(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x7fd) {
        obj->field1c = 0x80;
        *mk3_frame(thread, thread->frame + 1) = 0x7ff;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_d_wait_nonattack);
    }
    if (token == 0x7ff)
        return mk3_install(thread, (MK3THREADFUNC)t_d_uppercut);
    if (token != 0)
        return -3;
    *mk3_frame(thread, thread->frame + 1) = 0x7fd;
    thread->frame = thread->frame + 1;
    return mk3_install(thread, (MK3THREADFUNC)t_do_duck);
}

/* t_d_zap_now -- armv7 0x00067fe0: ochar_zaps picks by character; negative
 * means close in, otherwise this level leaves (field20 remembers it) and the
 * zap runs one level above */
long t_d_zap_now(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    int32_t v;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    v = ochar_zaps[obj->field08->field24];
    obj->field1c = (uint32_t)v;
    if (v < 0)
        return mk3_install(thread, (MK3THREADFUNC)t_stalk_in_close);

    obj->field20 = (uint32_t)(uintptr_t)t_local_reaction_exit;
    mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

    thread->frame = thread->frame + 1;
    return mk3_install(thread, (MK3THREADFUNC)t_do_zap);
}

/* ======================================================================
 * More deciders: the attack picker, the duck chain, the decoy and flipk
 * state machines.
 * ====================================================================== */

extern const uint32_t tab_counter_decoy[];
long t_sq_quake_abort(struct MK3THREAD *thread);
long t_d_stalk_a11(struct MK3THREAD *thread);
long t_d_rapid_lo(struct MK3THREAD *thread);
long t_stw_proj_proc(struct MK3THREAD *thread);
long t_flykick_towards_me(struct MK3THREAD *thread);
long t_cornered_attack(struct MK3THREAD *thread);
long t_d_attack_very_close(struct MK3THREAD *thread);
long t_d_attack_close(struct MK3THREAD *thread);
long t_d_attack_far(struct MK3THREAD *thread);
long t_d_attack_very_far(struct MK3THREAD *thread);
long t_d_backup_jump(struct MK3THREAD *thread);
long t_d_wait_nonattack(struct MK3THREAD *thread);
long t_d_fflip_kick_jump(struct MK3THREAD *thread);
long q_is_he_reacting(MK3OBJ *obj);
void q_proj_jclose(MK3OBJ *obj);
void q_am_i_cornered(MK3OBJ *obj);

/* t_d_wait_finish_react -- armv7 0x0006e810: close in (<= 0x5f) backs off a
 * bit; otherwise arm q_is_he_reacting for 0x60 frames and wait at 0x4bb. Both
 * state-0 exits refuse. */
long t_d_wait_finish_react(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        get_x_dist(obj);
        if ((int32_t)obj->field28 <= 0x5f) {
            return mk3_install(thread, (MK3THREADFUNC)t_d_backoff_a_bit);
        }
        obj->field48 = (uint32_t)(uintptr_t)q_is_he_reacting;
        obj->a10     = 0x60;
        *mk3_frame(thread, thread->frame + 1) = 0x4bb;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_stance_wait_no);
    }
    if (token != 0x4bb)
        return -3;
    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

/* t_sg_delayed_quake -- armv7 0x0006e460 */
long t_sg_delayed_quake(MK3THREAD *thread)
{
    MK3OBJ       *obj = (MK3OBJ *)thread->proc;
    MK3THREADFUNC h;
    uint32_t      token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->a10     = 0x40;
        obj->field48 = (uint32_t)(uintptr_t)q_jax_smash;
        *mk3_frame(thread, thread->frame + 1) = 0xcb6;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_retreat_wait_yes);
    }
    if (token != 0xcb6)
        return -3;

    get_x_dist(obj);
    if ((int32_t)obj->field28 > 0xbf) {
        obj->field1c = 0x14;
        h = (MK3THREADFUNC)t_do_body_propell;
    } else {
        h = (MK3THREADFUNC)t_sq_quake_abort;
    }
    return mk3_install(thread, h);
}

/* c_superk_sd -- armv7 0x0006ac78: 0 arms field48 = 0x48 / a10 = 0x30 and
 * pushes t_d_stalk_a11 (wait 0x107a); 0x107a pushes the uppercut wait (0x107b);
 * 0x107b becomes t_d_rapid_lo */
long c_superk_sd(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x107a) {
        *mk3_frame(thread, thread->frame + 1) = 0x107b;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_nr_uppercut_if_u_can);
    }
    if (token == 0x107b)
        return mk3_install(thread, (MK3THREADFUNC)t_d_rapid_lo);
    if (token != 0)
        return -3;
    obj->a10     = 0x30;
    obj->field48 = 0x30 + 0x18;
    *mk3_frame(thread, thread->frame + 1) = 0x107a;
    thread->frame = thread->frame + 1;
    return mk3_install(thread, (MK3THREADFUNC)t_d_stalk_a11);
}

/* c_sz_decoy -- armv7 0x0006ccac: tab_counter_decoy[character] is the
 * handler; at low difficulty a promotion roll can send it to
 * t_return_to_beware first */
long c_sz_decoy(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t h;
    int32_t  d;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    d = *(const int16_t *)(const void *)(G_BYTES + 0x44c);
    obj->field1c = (uint32_t)d;
    if (d <= 2) {
        obj->field1c = (uint32_t)(uintptr_t)rpt_counter;
        ask_mr_diff(obj);
        if (obj->field5c == 0)
            return mk3_install(thread, (MK3THREADFUNC)t_return_to_beware);
    }
    h = tab_counter_decoy[obj->field08->field24];
    obj->field1c = h;
    return mk3_install(thread, (MK3THREADFUNC)(uintptr_t)h);
}

/* t_d_flipk_over_proj -- armv7 0x0006aac4 */
long t_d_flipk_over_proj(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x103c) {
        obj->field48 = (uint32_t)(uintptr_t)q_proj_jclose;
        obj->a10     = 0x20;
        *mk3_frame(thread, thread->frame + 1) = 0x103f;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_stance_wait_yes);
    }
    if (token == 0x103f)
        return mk3_install(thread, (MK3THREADFUNC)t_d_fflip_kick_jump);
    if (token != 0)
        return -3;
    *mk3_frame(thread, thread->frame + 1) = 0x103c;
    thread->frame = thread->frame + 1;
    return mk3_install(thread, (MK3THREADFUNC)t_stw_proj_proc);
}

/* t_av_flyattack -- armv7 0x00070974 */
long t_av_flyattack(MK3THREAD *thread)
{
    MK3OBJ       *obj = (MK3OBJ *)thread->proc;
    MK3THREADFUNC h;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    is_towards_me(obj);
    if (obj->field5c != 0)
        return mk3_install(thread, (MK3THREADFUNC)t_flykick_towards_me);

    get_x_dist(obj);
    if ((int32_t)obj->field28 <= 0x3f) {
        h = (MK3THREADFUNC)t_d_block;
    } else {
        q_will_he_reach_me(obj);
        if (obj->field5c != 0)
            h = (MK3THREADFUNC)t_d_block;
        else
            h = (MK3THREADFUNC)t_d_zap_now;
    }
    return mk3_install(thread, h);
}

/* t_d_attack -- armv7 0x000711a4: cornered attacks; otherwise the distance
 * picks one of four ranges */
long t_d_attack(MK3THREAD *thread)
{
    MK3OBJ       *obj = (MK3OBJ *)thread->proc;
    MK3THREADFUNC h;
    int32_t       d;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    q_am_i_cornered(obj);
    if (obj->field5c != 0)
        return mk3_install(thread, (MK3THREADFUNC)t_cornered_attack);

    get_x_dist(obj);
    d = (int32_t)obj->field28;
    if (d <= 0x44)
        h = (MK3THREADFUNC)t_d_attack_very_close;
    else if (d <= 0x77)
        h = (MK3THREADFUNC)t_d_attack_close;
    else if (d > 0xef)
        h = (MK3THREADFUNC)t_d_attack_very_far;
    else
        h = (MK3THREADFUNC)t_d_attack_far;
    return mk3_install(thread, h);
}

/* t_d_duck -- armv7 0x00068ab8: same shape as t_d_duck_then_uppercut with
 * its own tokens and t_d_backup_jump at the end */
long t_d_duck(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x7f5) {
        obj->field1c = 0x80;
        *mk3_frame(thread, thread->frame + 1) = 0x7f7;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_d_wait_nonattack);
    }
    if (token == 0x7f7)
        return mk3_install(thread, (MK3THREADFUNC)t_d_backup_jump);
    if (token != 0)
        return -3;
    *mk3_frame(thread, thread->frame + 1) = 0x7f5;
    thread->frame = thread->frame + 1;
    return mk3_install(thread, (MK3THREADFUNC)t_do_duck);
}

/* ======================================================================
 * Blur/knee/corner/post-block deciders. Where state 0 installs a handler on
 * its own level and then falls into the token compare, the binary refuses
 * with -3 as well: the install is kept, the frame is abandoned.
 * ====================================================================== */

long t_d_unblock(struct MK3THREAD *thread);
long t_do_knee(struct MK3THREAD *thread);
long t_do_backup(struct MK3THREAD *thread);
long t_d_stance_pause(struct MK3THREAD *thread);
long t_flipk_scan(struct MK3THREAD *thread);
void lao_angle_wait(MK3OBJ *obj);
void count_q_repeats(MK3OBJ *obj);
void randu_minimum(MK3OBJ *obj);

/* c_tusk_blur -- armv7 0x0006cac8: three install sites */
long c_tusk_blur(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field20 = 0x213;
    count_q_repeats(obj);
    if (obj->field28 != 0)
        return mk3_install(thread, (MK3THREADFUNC)t_d_block);

    should_i_promove(obj);
    if (obj->field5c != 0)
        return mk3_install(thread, (MK3THREADFUNC)t_d_block);
    return mk3_install(thread, (MK3THREADFUNC)t_return_to_beware);
}

/* t_d_knee -- armv7 0x0006f5c0: in the air it becomes t_d_rapid_lo (and
 * refuses); on the ground it pushes t_do_knee and waits at 0x16c */
long t_d_knee(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        is_he_airborn(obj);
        if (obj->field5c != 0) {
            return mk3_install(thread, (MK3THREADFUNC)t_d_rapid_lo);
        }
        *mk3_frame(thread, thread->frame + 1) = 0x16c;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_do_knee);
    }
    if (token != 0x16c)
        return -3;
    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

/* t_caj_corner_far -- armv7 0x0006c918 */
long t_caj_corner_far(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field1c = 0x1f4;
        randper(obj);
        if (obj->field5c != 0) {
            return mk3_install(thread, (MK3THREADFUNC)t_d_zap);
        }
        obj->field1c = 0x80;
        obj->field48 = 0x80 + 0x20;
        *mk3_frame(thread, thread->frame + 1) = 0xc66;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_d_stalk_a11);
    }
    if (token != 0xc66)
        return -3;
    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

/* t_d_post_block -- armv7 0x0006c6dc */
long t_d_post_block(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        is_he_attacking(obj);
        if (obj->field5c == 0) {
            return mk3_install(thread, (MK3THREADFUNC)t_d_unblock);
        }
        obj->field00->field18 = 0x700;
        obj->field1c = 0x700 - 0x680;
        *mk3_frame(thread, thread->frame + 1) = 0x790;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_d_wait_nonattack);
    }
    if (token != 0x790)
        return -3;
    return mk3_install(thread, (MK3THREADFUNC)t_d_unblock);
}

/* t_perhaps_flipk -- armv7 0x00070d30: too near or too far (<= 0xb4 or
 * > 0xf0) closes in; in between flips and scans. Every state-0 exit
 * refuses. */
long t_perhaps_flipk(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    int32_t  d;

    if (token == 0) {
        get_x_dist(obj);
        d = (int32_t)obj->field28;
        if (d <= 0xb4) {
            return mk3_install(thread, (MK3THREADFUNC)t_stalk_in_close);
        }
        if (d > 0xf0) {
            return mk3_install(thread, (MK3THREADFUNC)t_stalk_in_close);
        }
        frontflip_setup(obj);
        obj->field34 = (uint32_t)(uintptr_t)t_flipk_scan;
        *mk3_frame(thread, thread->frame + 1) = 0x5c1;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_d_fflip_scan_jsrp);
    }
    if (token != 0x5c1)
        return -3;
    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

/* t_av_lao_angle -- armv7 0x0006c610 */
long t_av_lao_angle(MK3THREAD *thread)
{
    MK3OBJ       *obj = (MK3OBJ *)thread->proc;
    MK3THREADFUNC h;
    uint32_t      token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field48 = (uint32_t)(uintptr_t)lao_angle_wait;
        obj->a10     = 0x30;
        *mk3_frame(thread, thread->frame + 1) = 0x11fa;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_stance_wait_yes);
    }
    if (token != 0x11fa)
        return -3;

    get_his_action(obj);
    if (obj->field20 == 0x20c)
        h = (MK3THREADFUNC)t_d_block;
    else
        h = (MK3THREADFUNC)t_local_reaction_exit;
    return mk3_install(thread, h);
}

/* t_d_hang_out -- armv7 0x00072224: G+0x448 counts down (never below zero),
 * a wait of 0x80 +- randu_minimum frames behind t_d_stance_pause */
long t_d_hang_out(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    int32_t  v;

    if (token == 0) {
        v = (int32_t)*(const uint32_t *)(const void *)(G_BYTES + 0x448) - 1;
        if (v >= 0)
            *(uint32_t *)(void *)(G_BYTES + 0x448) = (uint32_t)v;
        obj->field1c = 0x80;
        obj->field20 = 0x80 - 0x50;
        randu_minimum(obj);
        obj->a10 = obj->field1c;
        *mk3_frame(thread, thread->frame + 1) = 0x2c2;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_d_stance_pause);
    }
    if (token != 0x2c2)
        return -3;
    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

/* t_drone_post_duck_hit -- armv7 0x00068984 */
long t_drone_post_duck_hit(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x770) {
        *mk3_frame(thread, thread->frame + 1) = 0x771;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_do_backup);
    }
    if (token == 0x771)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    if (token != 0)
        return -3;
    obj->field1c = 0x80;
    *mk3_frame(thread, thread->frame + 1) = 0x770;
    thread->frame = thread->frame + 1;
    return mk3_install(thread, (MK3THREADFUNC)t_d_wait_nonattack);
}

/* ======================================================================
 * Spawn waits, the random table picker and the delayed attacks.
 * ====================================================================== */

long t_drfp1(struct MK3THREAD *thread);
long t_av_sweep(struct MK3THREAD *thread);
long t_stalk_wait_yes(struct MK3THREAD *thread);
long t_do_axe_up(struct MK3THREAD *thread);
long t_do_lia_scream(struct MK3THREAD *thread);
long funcs_13831(struct MK3THREAD *thread);
long t_react_jump_table_act(struct MK3THREAD *thread);
void init_special(MK3OBJ *obj);
void randu(MK3OBJ *obj);
void q_willy_uppercut(MK3OBJ *obj);

/* t_wait_proj_spawn -- armv7 0x000700dc: wait up to a10 (0x20) frames, one
 * at a time at 0xf9b, for his projectile to appear while he is doing
 * something small (0 < action <= 0xff, and not 0x604); then pop or leave */
long t_wait_proj_spawn(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    int32_t  act;

    if (token == 0) {
        obj->a10 = 0x20;
        goto park;
    }
    if (token != 0xf9b)
        return -3;

    obj->a10 = obj->a10 - 1;
    if (obj->a10 == 0)
        goto done;
    get_his_proj_proc(obj);
    if (obj->field1c != 0)
        goto done;
    get_his_action(obj);
    act = (int32_t)obj->field20;
    if (act == 0x604 || act == 0)
        goto done;
    if (act <= 0xff)
        goto park;

done:
    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
        return 0;
    }
    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

park:
    *mk3_frame(thread, thread->frame + 1) = 0xf9b;
    thread->fieldfc = 1;
    return 1;
}

/* t_drone_rfp -- armv7 0x00071d64: push field24, 28, 2c and 40 on the
 * argument stack, hand the proc its 0x60 and 0x1c, run init_special, then
 * peek the top back into field40 and go to t_drfp1 */
long t_drone_rfp(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t n;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    n = thread->fieldf8;
    *mk3_arg(thread, n) = obj->field24;
    thread->fieldf8 = n + 1;
    *mk3_arg(thread, n + 1) = obj->field28;
    thread->fieldf8 = n + 2;
    *mk3_arg(thread, n + 2) = obj->field2c;
    thread->fieldf8 = n + 3;
    *mk3_arg(thread, n + 3) = obj->field40;
    thread->fieldf8 = n + 4;

    *(uint32_t *)((char *)obj->field00 + 0x60) = obj->field1c;
    obj->field00->field1c = obj->field20;
    init_special(obj);
    obj->field40 = *mk3_arg(thread, thread->fieldf8 - 1);
    get_char_ani(obj);
    return mk3_install(thread, (MK3THREADFUNC)t_drfp1);
}

/* t_random_do -- armv7 0x00072e4c: pop a level (or become
 * t_local_reaction_exit at the bottom), slide the pair above down, then
 * pick table[randu() - 1] from the table the object carries (count at +0x64,
 * table at +0x68) and run t_ochar_do with it in field1c */
long t_random_do(MK3THREAD *thread)
{
    MK3OBJ   *obj = (MK3OBJ *)thread->proc;
    uint32_t  f, h, i;
    const uint32_t *table;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
    } else {
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_local_reaction_exit;
        *mk3_frame(thread, thread->frame + 1) = 0;
    }

    f = thread->frame;
    h = mk3_frame(thread, f + 1)[1];
    *mk3_frame(thread, f + 1) = *mk3_frame(thread, f + 2);
    mk3_frame(thread, f)[1] = h;

    obj->field1c = *(const uint32_t *)((const char *)obj + 0x64);
    obj->field20 = *(const uint32_t *)((const char *)obj + 0x68);
    randu(obj);
    i = obj->field1c - 1;
    obj->field1c = i;
    table = (const uint32_t *)(uintptr_t)obj->field20;
    obj->field20 = (uint32_t)(uintptr_t)(table + i);
    obj->field1c = table[i];
    return mk3_install(thread, (MK3THREADFUNC)t_ochar_do);
}

/* t_willy_go_round -- armv7 0x0006b008 */
long t_willy_go_round(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x1112) {
        obj->a10     = 0x30;
        obj->field48 = (uint32_t)(uintptr_t)q_willy_uppercut;
        *mk3_frame(thread, thread->frame + 1) = 0x1116;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_stance_wait_yes);
    }
    if (token == 0x1116)
        return mk3_install(thread, (MK3THREADFUNC)t_d_uppercut);
    if (token != 0)
        return -3;
    obj->a10     = 0x30;
    obj->field48 = 0x30 + 0x10;
    *mk3_frame(thread, thread->frame + 1) = 0x1112;
    thread->frame = thread->frame + 1;
    return mk3_install(thread, (MK3THREADFUNC)t_d_stalk_a11);
}

/* c_froze_closer -- armv7 0x0006f36c */
long c_froze_closer(MK3THREAD *thread)
{
    MK3OBJ       *obj = (MK3OBJ *)thread->proc;
    MK3THREADFUNC h;
    uint32_t      token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->a10     = 0x30;
        obj->field48 = 0x30 + 0x10;
        *mk3_frame(thread, thread->frame + 1) = 0x10f6;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_d_stalk_a11);
    }
    if (token != 0x10f6)
        return -3;

    is_he_airborn(obj);
    if (obj->field5c == 0) {
        return mk3_install(thread, (MK3THREADFUNC)t_d_attack_very_close);
    }
    get_his_dog(obj);
    if ((int32_t)obj->field1c > 0xf)
        h = (MK3THREADFUNC)t_d_jump_up_kick;
    else
        h = (MK3THREADFUNC)t_d_uppercut;
    return mk3_install(thread, h);
}

/* c_sweep -- armv7 0x0006eb18: above difficulty 2 his action (0x507 or
 * 0x309) goes straight to t_av_sweep; otherwise the reach test chooses
 * between t_return_to_beware and the jump table with funcs.13831 */
long c_sweep(MK3THREAD *thread)
{
    MK3OBJ       *obj = (MK3OBJ *)thread->proc;
    MK3THREADFUNC h;
    int32_t       d;
    uint32_t      v;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    d = *(const int16_t *)(const void *)(G_BYTES + 0x44c);
    obj->field1c = (uint32_t)d;
    if (d > 2) {
        v = obj->field00->field18;
        obj->field1c = v;
        if (v == 0x507) {
            h = (MK3THREADFUNC)t_av_sweep;
            goto install;
        }
        if (v == 0x309) {
            h = (MK3THREADFUNC)t_av_sweep;
            goto install;
        }
    }
    q_will_he_reach_me(obj);
    if (obj->field5c == 0) {
        h = (MK3THREADFUNC)t_return_to_beware;
        goto install;
    }
    *(uint32_t *)((char *)obj + 0x68) = (uint32_t)(uintptr_t)funcs_13831;
    return mk3_install(thread, (MK3THREADFUNC)t_react_jump_table_act);

install:
    return mk3_install(thread, h);
}

/* t_ind_delayed_axe / t_lia_delayed_scream -- armv7 0x00069d18, 0x00069b68:
 * arm a distance probe, wait behind t_stalk_wait_yes, push the move, leave */
long t_ind_delayed_axe(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0xd2d) {
        *mk3_frame(thread, thread->frame + 1) = 0xd2e;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_do_axe_up);
    }
    if (token == 0xd2e)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    if (token != 0)
        return -3;
    obj->a10     = 0x40;
    obj->field48 = (uint32_t)(uintptr_t)q_is_he_axe_close;
    *mk3_frame(thread, thread->frame + 1) = 0xd2d;
    thread->frame = thread->frame + 1;
    return mk3_install(thread, (MK3THREADFUNC)t_stalk_wait_yes);
}

long t_lia_delayed_scream(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0xd0e) {
        *mk3_frame(thread, thread->frame + 1) = 0xd0f;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_do_lia_scream);
    }
    if (token == 0xd0f)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    if (token != 0)
        return -3;
    obj->a10     = 0x40;
    obj->field48 = (uint32_t)(uintptr_t)q_is_he_scream_close;
    *mk3_frame(thread, thread->frame + 1) = 0xd0e;
    thread->frame = thread->frame + 1;
    return mk3_install(thread, (MK3THREADFUNC)t_stalk_wait_yes);
}

/* ======================================================================
 * Block/duck/net/corner/scream/beware state machines.
 * ====================================================================== */

uint32_t random32(void);
long t_wait_proj_pass(struct MK3THREAD *thread);
long t_do_block_hi(struct MK3THREAD *thread);
long t_d_elbow(struct MK3THREAD *thread);
long t_d_beware(struct MK3THREAD *thread);
long t_d_run_a11(struct MK3THREAD *thread);
long t_nr_sweep_if_u_can(struct MK3THREAD *thread);
long t_ct_kicks(struct MK3THREAD *thread);
long t_caj_corner_far(struct MK3THREAD *thread);
void q_corner_backf_land(MK3OBJ *obj);
void init_anirate(MK3OBJ *obj);

/* c_ssd -- armv7 0x0006fe44: nr_attack, then uppercut, then sweep (waits
 * 0x106f, 0x1070, 0x1071), then a coin on bit 11 of random32: zap or
 * propell attack */
long c_ssd(MK3THREAD *thread)
{
    MK3THREADFUNC h;
    uint32_t      token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x106f;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_nr_attack_sd);
    }
    if (token == 0x106f) {
        *mk3_frame(thread, thread->frame + 1) = 0x1070;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_nr_uppercut_if_u_can);
    }
    if (token == 0x1070) {
        *mk3_frame(thread, thread->frame + 1) = 0x1071;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_nr_sweep_if_u_can);
    }
    if (token != 0x1071)
        return -3;

    if ((random32() & 0x800u) != 0)
        h = (MK3THREADFUNC)t_d_zap;
    else
        h = (MK3THREADFUNC)t_d_propell_attack;
    return mk3_install(thread, h);
}

/* t_duck_under_proj -- armv7 0x00070308: reset, duck (0xe8e), wait for his
 * projectile to spawn (0xe8f), wait for it to pass (0xe90), back up */
long t_duck_under_proj(MK3THREAD *thread)
{
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        reset_proc_stack(thread);
        *mk3_frame(thread, thread->frame + 1) = 0xe8e;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_do_duck);
    }
    if (token == 0xe8e) {
        *mk3_frame(thread, thread->frame + 1) = 0xe8f;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_wait_proj_spawn);
    }
    if (token == 0xe8f) {
        *mk3_frame(thread, thread->frame + 1) = 0xe90;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_wait_proj_pass);
    }
    if (token != 0xe90)
        return -3;
    return mk3_install(thread, (MK3THREADFUNC)t_d_backup_jump);
}

/* t_d_block -- armv7 0x0006fa20 */
long t_d_block(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x839) {
        obj->a10 = 0x80;
        *mk3_frame(thread, thread->frame + 1) = 0x83b;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_d_wait_nonattack);
    }
    if (token == 0x83b)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    if (token != 0)
        return -3;
    face_opponent(obj);
    *mk3_frame(thread, thread->frame + 1) = 0x839;
    thread->frame = thread->frame + 1;
    return mk3_install(thread, (MK3THREADFUNC)t_do_block_hi);
}

/* c_caught_net -- armv7 0x0006f420: state 0 decides but refuses; 0x10ec is
 * the follow-up */
long c_caught_net(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        should_i_promove(obj);
        if (obj->field5c == 0) {
            return mk3_install(thread, (MK3THREADFUNC)t_return_to_beware);
        }
        obj->a10     = 0x30;
        obj->field48 = 0x30 + 0x10;
        *mk3_frame(thread, thread->frame + 1) = 0x10ec;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_d_run_a11);
    }
    if (token != 0x10ec)
        return -3;

    is_he_airborn(obj);
    if (obj->field5c != 0)
        return mk3_install(thread, (MK3THREADFUNC)t_d_uppercut);
    return mk3_install(thread, (MK3THREADFUNC)t_run_in_close_now);
}

/* c_rhouse -- armv7 0x0006ecd8 */
long c_rhouse(MK3THREAD *thread)
{
    MK3OBJ       *obj = (MK3OBJ *)thread->proc;
    MK3THREADFUNC h;
    uint32_t      token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        get_x_dist(obj);
        if ((int32_t)obj->field28 > 0xd0) {
            h = (MK3THREADFUNC)t_return_to_beware;
        } else {
            q_will_he_reach_me(obj);
            if (obj->field5c == 0) {
                stop_me_player(obj);
                *mk3_frame(thread, thread->frame + 1) = 0x13f7;
                thread->frame = thread->frame + 1;
                return mk3_install(thread, (MK3THREADFUNC)t_nr_sweep_if_u_can);
            }
            h = (MK3THREADFUNC)t_ct_kicks;
        }
        return mk3_install(thread, h);
    }
    if (token != 0x13f7)
        return -3;
    return mk3_install(thread, (MK3THREADFUNC)t_d_zap);
}

/* t_caj_corner -- armv7 0x0006e500: by distance; every state-0 exit
 * refuses */
long t_caj_corner(MK3THREAD *thread)
{
    MK3OBJ       *obj = (MK3OBJ *)thread->proc;
    MK3THREADFUNC h;
    uint32_t      token = *mk3_frame(thread, thread->frame + 1);
    int32_t       d;

    if (token == 0) {
        get_x_dist(obj);
        d = (int32_t)obj->field28;
        if (d <= 0x47) {
            h = (MK3THREADFUNC)t_d_uppercut;
        } else if (d <= 0x87) {
            h = (MK3THREADFUNC)t_d_flip_punch_jump;
        } else if (d > 0xc0) {
            h = (MK3THREADFUNC)t_caj_corner_far;
        } else {
            obj->a10     = 0x40;
            obj->field48 = (uint32_t)(uintptr_t)q_corner_backf_land;
            *mk3_frame(thread, thread->frame + 1) = 0xc7f;
            thread->frame = thread->frame + 1;
            h = (MK3THREADFUNC)t_stance_wait_yes;
        }
        return mk3_install(thread, h);
    }
    if (token != 0xc7f)
        return -3;
    return mk3_install(thread, (MK3THREADFUNC)t_d_zap);
}

/* c_screamed -- armv7 0x0006ad84 */
long c_screamed(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x108f) {
        obj->a10     = 0x30;
        obj->field48 = (uint32_t)(uintptr_t)is_he_airborn;
        *mk3_frame(thread, thread->frame + 1) = 0x1093;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_stance_wait_no);
    }
    if (token == 0x1093)
        return mk3_install(thread, (MK3THREADFUNC)t_d_elbow);
    if (token != 0)
        return -3;
    obj->a10     = 0x30;
    obj->field48 = 0x30 + 0x15;
    *mk3_frame(thread, thread->frame + 1) = 0x108f;
    thread->frame = thread->frame + 1;
    return mk3_install(thread, (MK3THREADFUNC)t_d_stalk_a11);
}

/* t_d_beware_mframew -- armv7 0x0006cf94: clear the answer and the 0x48
 * word, init the animation rate, park a frame at 0x1490; there step the
 * animation and push t_d_beware (0x1492); there, while the script word at
 * field40 is nonzero, park again, else pop or leave */
long t_d_beware_mframew(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    uint32_t w;

    if (token == 0x1490) {
        next_anirate(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x1492;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_d_beware);
    }
    if (token == 0x1492) {
        w = *(const uint32_t *)(uintptr_t)obj->field40;
        obj->field20 = w;
        if (w != 0)
            goto park;
        if ((long)thread->frame > 0) {
            thread->frame = thread->frame - 1;
            return 0;
        }
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }
    if (token != 0)
        return -3;
    obj->field48 = 0;
    obj->field00->field5c = 0;
    init_anirate(obj);

park:
    *mk3_frame(thread, thread->frame + 1) = 0x1490;
    thread->fieldfc = 1;
    return 1;
}

/* ======================================================================
 * Attack-sd chains, delayed bikes, getup, sweep kick and the flip scans.
 * ====================================================================== */

long t_stat_do_sweep_kick(struct MK3THREAD *thread);
long t_do_flip_punch(struct MK3THREAD *thread);
long funcs_7789(struct MK3THREAD *thread);
void q_is_he_bike_close(MK3OBJ *obj);
void q_is_he_axe_close(MK3OBJ *obj);

/* c_stsw_sd -- armv7 0x0006da98: nr_attack, uppercut, sweep (waits 0x1054,
 * 0x1055, 0x1056), then the distance picks leave or zap */
long c_stsw_sd(MK3THREAD *thread)
{
    MK3OBJ       *obj = (MK3OBJ *)thread->proc;
    MK3THREADFUNC h;
    uint32_t      token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x1054) {
        *mk3_frame(thread, thread->frame + 1) = 0x1055;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_nr_uppercut_if_u_can);
    }
    if (token == 0x1055) {
        *mk3_frame(thread, thread->frame + 1) = 0x1056;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_nr_sweep_if_u_can);
    }
    if (token == 0x1056) {
        get_x_dist(obj);
        if ((int32_t)obj->field28 > 0x90)
            h = (MK3THREADFUNC)t_d_zap;
        else
            h = (MK3THREADFUNC)t_local_reaction_exit;
        return mk3_install(thread, h);
    }
    if (token != 0)
        return -3;
    *mk3_frame(thread, thread->frame + 1) = 0x1054;
    thread->frame = thread->frame + 1;
    return mk3_install(thread, (MK3THREADFUNC)t_nr_attack_sd);
}

/* t_if_u_can -- armv7 0x0006ec20: the strike test; a miss pops (or leaves),
 * a hit pops, slides the pair above down and then runs the routine a10
 * holds */
long t_if_u_can(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t f, h;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    strike_check_a0_test(obj);
    if (obj->field5c == 0) {
        if ((long)thread->frame > 0) {
            thread->frame = thread->frame - 1;
            return 0;
        }
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }

    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
    } else {
        mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }
    f = thread->frame;
    h = mk3_frame(thread, f + 1)[1];
    *mk3_frame(thread, f + 1) = *mk3_frame(thread, f + 2);
    mk3_frame(thread, f)[1] = h;

    mk3_frame(thread, thread->frame)[1] = obj->a10;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}

/* t_kano_upball, t_sonya_delayed_bike -- armv7 0x00069e38, 0x00069ef8: arm
 * q_is_he_bike_close and wait behind t_stalk_wait_yes, then set field1c and
 * push t_do_body_propell, then leave */
long t_kano_upball(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0xd52) {
        obj->field1c = 0x1c;
        *mk3_frame(thread, thread->frame + 1) = 0xd55;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_do_body_propell);
    }
    if (token == 0xd55)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    if (token != 0)
        return -3;
    obj->a10     = 0x40;
    obj->field48 = (uint32_t)(uintptr_t)q_is_he_bike_close;
    *mk3_frame(thread, thread->frame + 1) = 0xd52;
    thread->frame = thread->frame + 1;
    return mk3_install(thread, (MK3THREADFUNC)t_stalk_wait_yes);
}

long t_sonya_delayed_bike(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0xd5c) {
        obj->field1c = 1;
        *mk3_frame(thread, thread->frame + 1) = 0xd5e;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_do_body_propell);
    }
    if (token == 0xd5e)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    if (token != 0)
        return -3;
    obj->a10     = 0x40;
    obj->field48 = (uint32_t)(uintptr_t)q_is_he_bike_close;
    *mk3_frame(thread, thread->frame + 1) = 0xd5c;
    thread->frame = thread->frame + 1;
    return mk3_install(thread, (MK3THREADFUNC)t_stalk_wait_yes);
}

/* t_d_getup -- armv7 0x00071618: action 0x309, animation 0x21, rate 4, then
 * park a frame at a time at 0x7cc until the script word at field40 is zero */
long t_d_getup(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    uint32_t w;

    if (token == 0x7cc) {
        *mk3_frame(thread, thread->frame + 1) = 0x7cd;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_d_beware);
    }
    if (token == 0x7cd) {
        next_anirate(obj);
        w = *(const uint32_t *)(uintptr_t)obj->field40;
        obj->field1c = w;
        if (w != 0)
            goto park;
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }
    if (token != 0)
        return -3;
    obj->field00->field18 = 0x309;
    obj->field1c = 0;
    obj->field00->field5c = 0;
    obj->field40 = 0x21;
    get_char_ani(obj);
    obj->field1c = 4;
    init_anirate(obj);

park:
    *mk3_frame(thread, thread->frame + 1) = 0x7cc;
    thread->fieldfc = 1;
    return 1;
}

/* t_d_sweep_kick -- armv7 0x0006c820. `q_is_he_reacting` leaves the object
 * in r0, so the test in front of the second state is on a pointer and always
 * takes the reacting side. */
long t_d_sweep_kick(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x271;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_stat_do_sweep_kick);
    }
    if (token != 0x271)
        return -3;

    if (q_is_he_reacting(obj) == 0)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

    *(uint32_t *)((char *)obj + 0x68) = (uint32_t)(uintptr_t)funcs_7789;
    *(uint32_t *)((char *)obj + 0x64) = 2;
    *mk3_frame(thread, thread->frame + 1) = 0x278;
    thread->frame = thread->frame + 1;
    return mk3_install(thread, (MK3THREADFUNC)t_random_do);
}

/* t_flipk_scan, t_flipp_scan -- armv7 0x00070540, 0x00070484: close enough
 * (<= 0x80) resets and flips, otherwise the level pops and refuses, or
 * leaves at the bottom */
long t_flipk_scan(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        get_x_dist(obj);
        if ((int32_t)obj->field28 <= 0x80) {
            reset_proc_stack(thread);
            *mk3_frame(thread, thread->frame + 1) = 0x5b4;
            thread->frame = thread->frame + 1;
            return mk3_install(thread, (MK3THREADFUNC)t_do_flip_kick);
        }
        if ((long)thread->frame > 0) {
            thread->frame = thread->frame - 1;
            return -3;
        }
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }
    if (token != 0x5b4)
        return -3;
    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

long t_flipp_scan(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        get_x_dist(obj);
        if ((int32_t)obj->field28 <= 0x80) {
            reset_proc_stack(thread);
            *mk3_frame(thread, thread->frame + 1) = 0x683;
            thread->frame = thread->frame + 1;
            return mk3_install(thread, (MK3THREADFUNC)t_do_flip_punch);
        }
        if ((long)thread->frame > 0) {
            thread->frame = thread->frame - 1;
            return -3;
        }
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }
    if (token != 0x683)
        return -3;
    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

/* ======================================================================
 * Multi-state deciders, batch 17. Verified by tools/difftest against the
 * recompiled ARM, not by reading.
 * ====================================================================== */

long t_act_mframew(struct MK3THREAD *thread);
long t_do_elbow(struct MK3THREAD *thread);
long t_do_body_slam(struct MK3THREAD *thread);
long t_d_rapid_hi(struct MK3THREAD *thread);
long t_close_airborn(struct MK3THREAD *thread);
long t_very_close_airborn(struct MK3THREAD *thread);
long t_nr_drone_zone(struct MK3THREAD *thread);
long t_do_pit_fatality(struct MK3THREAD *thread);
long t_d_get_close_2_u(struct MK3THREAD *thread);
long t_wait_proj_spawn(struct MK3THREAD *thread);
long t_d_wait_yes_still(struct MK3THREAD *thread);
long t_swait_land_jsrp(struct MK3THREAD *thread);
long t_d_turnaround_jsrp(struct MK3THREAD *thread);
long t_d_hang_out(struct MK3THREAD *thread);
long t_chance_do(struct MK3THREAD *thread);
long funcs_8198(struct MK3THREAD *thread);
long funcs_8266(struct MK3THREAD *thread);
long is_he_short(MK3OBJ *obj);
void q_is_he_a_boss(MK3OBJ *obj);
void q_dist_lift(MK3OBJ *obj);
void q_is_tracker_close(MK3OBJ *obj);
void q_is_proj_gone(MK3OBJ *obj);
void q_is_he_dizzy(MK3OBJ *obj);
void run_setup(MK3OBJ *obj);
void reduce_turbo_bar(MK3OBJ *obj);
extern const int32_t ochar_zaps[];

/* c_lia_scream -- armv7 0x0006d260 */
long c_lia_scream(MK3THREAD *thread)
{
    MK3OBJ       *obj = (MK3OBJ *)thread->proc;
    MK3THREADFUNC h;
    uint32_t      token = *mk3_frame(thread, thread->frame + 1);
    int32_t       d;

    if (token == 0x13ce)
        return mk3_install(thread, (MK3THREADFUNC)t_d_zap_now);
    if (token != 0)
        return -3;

    obj->field1c = (uint32_t)(uintptr_t)rpt_counter;
    ask_mr_diff(obj);
    if (obj->field5c == 0)
        return mk3_install(thread, (MK3THREADFUNC)t_return_to_beware);

    get_x_dist(obj);
    d = (int32_t)obj->field28;
    if (d > 0xa0)
        return mk3_install(thread, (MK3THREADFUNC)t_d_zap_now);
    if (d > 0x7f) {
        obj->a10     = 0x40;
        obj->field48 = (uint32_t)(uintptr_t)q_dist_lift;
        *mk3_frame(thread, thread->frame + 1) = 0x13ce;
        thread->frame = thread->frame + 1;
        h = (MK3THREADFUNC)t_retreat_wait_yes;
    } else {
        h = (MK3THREADFUNC)t_d_sweep_kick;
    }
    return mk3_install(thread, h);
}

/* t_chance_do -- armv7 0x0006ced8: a chance roll; either way the level pops;
 * on success the routine at +0x68 runs in its place */
long t_chance_do(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t f, h, v;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = *(const uint32_t *)((const char *)obj + 0x64);
    randper(obj);
    if (obj->field5c == 0) {
        if ((long)thread->frame > 0) {
            thread->frame = thread->frame - 1;
            return 0;
        }
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }

    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
    } else {
        mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }
    f = thread->frame;
    h = mk3_frame(thread, f + 1)[1];
    *mk3_frame(thread, f + 1) = *mk3_frame(thread, f + 2);
    mk3_frame(thread, f)[1] = h;

    v = *(const uint32_t *)((const char *)obj + 0x68);
    obj->field1c = v;
    return mk3_install(thread, (MK3THREADFUNC)(uintptr_t)v);
}

/* t_d_attack_close -- armv7 0x0006f774 */
long t_d_attack_close(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    int32_t  d;

    if (token == 0x3f9)
        return mk3_install(thread, (MK3THREADFUNC)t_attack_close_hard);
    if (token != 0)
        return -3;

    q_airborn_counter(obj);
    if (obj->field5c != 0)
        return mk3_install(thread, (MK3THREADFUNC)t_close_airborn);

    d = *(const int16_t *)(const void *)(G_BYTES + 0x44c);
    obj->field1c = (uint32_t)d;
    if (d > 4)
        return mk3_install(thread, (MK3THREADFUNC)t_attack_close_hard);

    *(uint32_t *)((char *)obj + 0x68) = (uint32_t)(uintptr_t)funcs_8198;
    *(uint32_t *)((char *)obj + 0x64) = 6;
    *mk3_frame(thread, thread->frame + 1) = 0x3f9;
    thread->frame = thread->frame + 1;
    return mk3_install(thread, (MK3THREADFUNC)t_random_do);
}

/* t_d_attack_very_close -- armv7 0x0006f680 */
long t_d_attack_very_close(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x41c) {
        *(uint32_t *)((char *)obj + 0x68) = (uint32_t)(uintptr_t)funcs_8266;
        *(uint32_t *)((char *)obj + 0x64) = 4;
        *mk3_frame(thread, thread->frame + 1) = 0x423;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_random_do);
    }
    if (token == 0x423)
        return mk3_install(thread, (MK3THREADFUNC)t_very_close_airborn);
    if (token != 0)
        return -3;

    q_airborn_counter(obj);
    if (obj->field5c == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x41c;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_nr_drone_zone);
    }
    return mk3_install(thread, (MK3THREADFUNC)t_very_close_airborn);
}

/* t_d_elbow -- armv7 0x000723cc */
long t_d_elbow(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x164)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    if (token != 0)
        return -3;

    is_he_airborn(obj);
    if (obj->field5c != 0)
        return mk3_install(thread, (MK3THREADFUNC)t_d_rapid_lo);
    if (is_he_short(obj) != 0)
        return mk3_install(thread, (MK3THREADFUNC)t_d_knee);

    *mk3_frame(thread, thread->frame + 1) = 0x164;
    thread->frame = thread->frame + 1;
    return mk3_install(thread, (MK3THREADFUNC)t_do_elbow);
}

/* t_d_slam -- armv7 0x0006f4e4 */
long t_d_slam(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x17f)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    if (token != 0)
        return -3;

    q_is_he_a_boss(obj);
    if (obj->field5c != 0)
        return mk3_install(thread, (MK3THREADFUNC)t_d_rapid_hi);
    is_he_airborn(obj);
    if (obj->field5c != 0)
        return mk3_install(thread, (MK3THREADFUNC)t_d_rapid_hi);

    *mk3_frame(thread, thread->frame + 1) = 0x17f;
    thread->frame = thread->frame + 1;
    return mk3_install(thread, (MK3THREADFUNC)t_do_body_slam);
}

/* t_nr_attack_sd -- armv7 0x0006ca08: promote or not; either way the level
 * pops, and without a promotion t_return_to_beware takes its place */
long t_nr_attack_sd(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t f, h;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    should_i_promove(obj);
    if (obj->field5c != 0) {
        if ((long)thread->frame > 0) {
            thread->frame = thread->frame - 1;
            return 0;
        }
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }

    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
    } else {
        mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }
    f = thread->frame;
    h = mk3_frame(thread, f + 1)[1];
    *mk3_frame(thread, f + 1) = *mk3_frame(thread, f + 2);
    mk3_frame(thread, f)[1] = h;
    return mk3_install(thread, (MK3THREADFUNC)t_return_to_beware);
}

/* t_decoy_zap_wait -- armv7 0x0006b1f8 */
long t_decoy_zap_wait(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    int32_t  v;

    if (token == 0x1182)
        return mk3_install(thread, (MK3THREADFUNC)t_run_in_close_now);
    if (token == 0x117d)
        goto arm;
    if (token != 0)
        return -3;

    v = ochar_zaps[obj->field08->field24];
    obj->field1c = (uint32_t)v;
    if (v < 0)
        goto arm;
    *mk3_frame(thread, thread->frame + 1) = 0x117d;
    thread->frame = thread->frame + 1;
    return mk3_install(thread, (MK3THREADFUNC)t_do_zap);

arm:
    obj->field48 = (uint32_t)(uintptr_t)q_is_decoy_alive;
    obj->a10     = 0x80;
    *mk3_frame(thread, thread->frame + 1) = 0x1182;
    thread->frame = thread->frame + 1;
    return mk3_install(thread, (MK3THREADFUNC)t_stance_wait_no);
}

/* t_d_background_fatal -- armv7 0x000690d0 */
long t_d_background_fatal(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x988) {
        obj->field48 = (uint32_t)(uintptr_t)q_is_he_dizzy;
        obj->a10     = 0x40;
        *mk3_frame(thread, thread->frame + 1) = 0x98b;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_stance_wait_yes);
    }
    if (token == 0x98b) {
        if (obj->field5c != 0)
            return mk3_install(thread, (MK3THREADFUNC)t_do_pit_fatality);
        return mk3_install(thread, (MK3THREADFUNC)t_d_fatality_abort);
    }
    if (token != 0)
        return -3;
    *mk3_frame(thread, thread->frame + 1) = 0x988;
    thread->frame = thread->frame + 1;
    return mk3_install(thread, (MK3THREADFUNC)t_d_get_close_2_u);
}

/* t_d_duck_block -- armv7 0x000714d8 */
long t_d_duck_block(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x847) {
        obj->field1c = 0x80;
        *mk3_frame(thread, thread->frame + 1) = 0x849;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_d_wait_nonattack);
    }
    if (token == 0x849)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    if (token != 0)
        return -3;

    stop_me_player(obj);
    face_opponent(obj);
    obj->field40 = 6;
    get_char_ani(obj);
    obj->field1c = 3;
    obj->field20 = 0x701;
    *mk3_frame(thread, thread->frame + 1) = 0x847;
    thread->frame = thread->frame + 1;
    return mk3_install(thread, (MK3THREADFUNC)t_act_mframew);
}

/* t_ct_jax_dash -- armv7 0x0006b7cc */
long t_ct_jax_dash(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x1271) {
        obj->a10 = 0x40;
        goto park;
    }
    if (token == 0x1274) {
        *mk3_frame(thread, thread->frame + 1) = 0x1275;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_d_beware);
    }
    if (token == 0x1275) {
        obj->a10 = obj->a10 - 1;
        if (obj->a10 == 0)
            return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
        goto park;
    }
    if (token != 0)
        return -3;
    *mk3_frame(thread, thread->frame + 1) = 0x1271;
    thread->frame = thread->frame + 1;
    return mk3_install(thread, (MK3THREADFUNC)t_do_duck);

park:
    *mk3_frame(thread, thread->frame + 1) = 0x1274;
    thread->fieldfc = 1;
    return 1;
}

/* t_dont_zap_teles -- armv7 0x00069634: against 0x11, 0x12 and 0x14 pop,
 * slide the pair above down and run in close; against anything else pop (or
 * leave) */
long t_dont_zap_teles(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t c, f, h;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    c = ((MK3OBJ *)(uintptr_t)obj->field00->him)->field24;
    obj->field1c = c;
    if (c == 0x14 || c == 0x12 || c == 0x11) {
        if ((long)thread->frame > 0) {
            thread->frame = thread->frame - 1;
        } else {
            mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
        }
        f = thread->frame;
        h = mk3_frame(thread, f + 1)[1];
        *mk3_frame(thread, f + 1) = *mk3_frame(thread, f + 2);
        mk3_frame(thread, f)[1] = h;
        return mk3_install(thread, (MK3THREADFUNC)t_run_in_close);
    }
    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
        return 0;
    }
    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

/* t_duck_under_mproj -- armv7 0x0007021c */
long t_duck_under_mproj(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0xf83) {
        *mk3_frame(thread, thread->frame + 1) = 0xf84;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_wait_proj_spawn);
    }
    if (token == 0xf84) {
        obj->field48 = (uint32_t)(uintptr_t)q_is_proj_gone;
        obj->a10     = 0x50;
        *mk3_frame(thread, thread->frame + 1) = 0xf87;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_d_wait_yes_still);
    }
    if (token == 0xf87)
        return mk3_install(thread, (MK3THREADFUNC)t_d_backup_jump);
    if (token != 0)
        return -3;
    reset_proc_stack(thread);
    *mk3_frame(thread, thread->frame + 1) = 0xf83;
    thread->frame = thread->frame + 1;
    return mk3_install(thread, (MK3THREADFUNC)t_do_duck);
}

/* c_tracker_rocket -- armv7 0x0006e198 */
long c_tracker_rocket(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0xdd2) {
        get_x_dist(obj);
        if ((int32_t)obj->field28 <= 0xdf)
            return mk3_install(thread, (MK3THREADFUNC)t_run_in_close);
        *mk3_frame(thread, thread->frame + 1) = 0xdd7;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_stw_proj_proc);
    }
    if (token == 0xdd7) {
        obj->field48 = (uint32_t)(uintptr_t)q_is_tracker_close;
        obj->a10     = 0x70;
        *mk3_frame(thread, thread->frame + 1) = 0xddb;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_stance_wait_yes);
    }
    if (token == 0xddb)
        return mk3_install(thread, (MK3THREADFUNC)t_run_in_close);
    if (token != 0)
        return -3;
    *mk3_frame(thread, thread->frame + 1) = 0xdd2;
    thread->frame = thread->frame + 1;
    return mk3_install(thread, (MK3THREADFUNC)t_nr_sweep_if_u_can);
}

/* t_hangout_check -- armv7 0x00067950 */
long t_hangout_check(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    int32_t  d;
    uint32_t v;

    if (token == 0x2d2)
        goto pop_or_leave;
    if (token != 0)
        return -3;

    d = *(const int16_t *)(const void *)(G_BYTES + 0x44c);
    obj->field1c = (uint32_t)d;
    if (d > 4) {
        if ((long)thread->frame > 0) {
            thread->frame = thread->frame - 1;
            return 0;
        }
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }
    v = *(const uint32_t *)(const void *)(G_BYTES + 0x448);
    obj->field1c = v;
    if (v == 0)
        goto pop_or_leave;

    *(uint32_t *)((char *)obj + 0x64) = 0xc8;
    *(uint32_t *)((char *)obj + 0x68) = (uint32_t)(uintptr_t)t_d_hang_out;
    *mk3_frame(thread, thread->frame + 1) = 0x2d2;
    thread->frame = thread->frame + 1;
    return mk3_install(thread, (MK3THREADFUNC)t_chance_do);

pop_or_leave:
    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
        return 0;
    }
    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

/* t_av_pounce -- armv7 0x0006fad8 */
long t_av_pounce(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x11ac) {
        reduce_turbo_bar(obj);
        next_anirate(obj);
        obj->a10 = obj->a10 - 1;
        if (obj->a10 != 0)
            goto park;
        stop_me_player(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x11b4;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_d_turnaround_jsrp);
    }
    if (token == 0x11b4) {
        *mk3_frame(thread, thread->frame + 1) = 0x11b5;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_swait_land_jsrp);
    }
    if (token == 0x11b5)
        return mk3_install(thread, (MK3THREADFUNC)t_run_in_close_now);
    if (token != 0)
        return -3;
    run_setup(obj);
    face_opponent(obj);
    obj->a10 = 0x10;

park:
    *mk3_frame(thread, thread->frame + 1) = 0x11ac;
    thread->fieldfc = 1;
    return 1;
}

/* ======================================================================
 * The wait states and the multi-token deciders, batch 18.
 *
 * The `*_wait_*` threads run the probe the caller left in obj->field48 (a
 * function pointer) with a10 counting the frames; the probe answers in
 * field5c. Verified by tools/difftest against the recompiled ARM.
 * ====================================================================== */

typedef void (*MK3PROBE)(MK3OBJ *obj);

extern const uint32_t ochar_towards_counters[];
extern const int16_t repeat_table[];
extern const int16_t d_randpers[];
long t_far_airborn(struct MK3THREAD *thread);
long t_dist_retp(struct MK3THREAD *thread);
long t_d_cornered(struct MK3THREAD *thread);
long t_d_stance_pause(struct MK3THREAD *thread);
long t_d_beware(struct MK3THREAD *thread);
long t_d_turnaround(struct MK3THREAD *thread);
long t_d_hi_kick(struct MK3THREAD *thread);
long t_d_jump_up_kick(struct MK3THREAD *thread);
long t_d_lo_kick(struct MK3THREAD *thread);
long t_d_fflip_jump(struct MK3THREAD *thread);
long t_run_in_close_hard(struct MK3THREAD *thread);
long t_d_knee(struct MK3THREAD *thread);
long t_d_zap(struct MK3THREAD *thread);
long t_d_uppercut(struct MK3THREAD *thread);
long t_d_run_a11(struct MK3THREAD *thread);
long t_nr_uppercut_if_u_can(struct MK3THREAD *thread);
long t_nr_sweep_if_u_can(struct MK3THREAD *thread);
long t_mframew(struct MK3THREAD *thread);
long funcs_7715(struct MK3THREAD *thread);
long funcs_8089(struct MK3THREAD *thread);
long funcs_8157(struct MK3THREAD *thread);
long t_stance_wait_yes(struct MK3THREAD *thread);
long t_stance_wait_no(struct MK3THREAD *thread);
long t_nr_drone_zone(struct MK3THREAD *thread);
void d_walkf_setup(MK3OBJ *obj);
void d_walkb_setup(MK3OBJ *obj);
void MKEvent_Add(long type, long subtype, long param, long player);
extern int rand(void);
long is_he_short(MK3OBJ *obj);
long am_i_facing_him(MK3OBJ *obj);

/* the pop every wait ends with: a level down, or t_local_reaction_exit at the
 * bottom. Returns 0 either way. */
#define POP_OR_LEAVE(thread)                                                   \
    do {                                                                       \
        if ((long)(thread)->frame > 0) {                                       \
            (thread)->frame = (thread)->frame - 1;                             \
            return 0;                                                          \
        }                                                                      \
        return mk3_install((thread), (MK3THREADFUNC)t_local_reaction_exit);    \
    } while (0)

/* run the probe with a10 kept across it (pushed on the argument stack) */
#define RUN_PROBE(thread, obj)                                                 \
    do {                                                                       \
        uint32_t n_ = (thread)->fieldf8;                                       \
        *mk3_arg((thread), n_) = (obj)->a10;                                   \
        (thread)->fieldf8 = n_ + 1;                                            \
        ((MK3PROBE)(uintptr_t)(obj)->field48)(obj);                            \
        n_ = (thread)->fieldf8 - 1;                                            \
        (thread)->fieldf8 = n_;                                                \
        (obj)->a10 = *mk3_arg((thread), n_);                                   \
    } while (0)

/* t_d_wait_yes_still -- armv7 0x000684c0 */
long t_d_wait_yes_still(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x66e;
        thread->fieldfc = 1;
        return 1;
    }
    if (token != 0x66e)
        return -3;

    RUN_PROBE(thread, obj);
    if (obj->field5c != 0)
        POP_OR_LEAVE(thread);
    obj->a10 = obj->a10 - 1;
    if (obj->a10 == 0)
        POP_OR_LEAVE(thread);
    return mk3_install(thread, (MK3THREADFUNC)t_d_wait_yes_still);
}

/* t_stance_wait_yes / t_stance_wait_no -- armv7 0x00071fe4, 0x00071ee4 */
long t_stance_wait_yes(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x637) {
        RUN_PROBE(thread, obj);
        if (obj->field5c == 0) {
            *mk3_frame(thread, thread->frame + 1) = 0x63d;
            thread->fieldfc = 1;
            return 1;
        }
        POP_OR_LEAVE(thread);
    }
    if (token == 0x63d) {
        obj->a10 = obj->a10 - 1;
        if ((int32_t)obj->a10 <= 0)
            POP_OR_LEAVE(thread);
        goto again;
    }
    if (token != 0)
        return -3;
    d_stance_setup(obj);

again:
    next_anirate(obj);
    *mk3_frame(thread, thread->frame + 1) = 0x637;
    thread->frame = thread->frame + 1;
    return mk3_install(thread, (MK3THREADFUNC)t_d_beware);
}

long t_stance_wait_no(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x64d) {
        RUN_PROBE(thread, obj);
        if (obj->field5c != 0) {
            *mk3_frame(thread, thread->frame + 1) = 0x653;
            thread->fieldfc = 1;
            return 1;
        }
        POP_OR_LEAVE(thread);
    }
    if (token == 0x653) {
        obj->a10 = obj->a10 - 1;
        if ((int32_t)obj->a10 > 0)
            goto again;
        obj->field5c = 0;
        POP_OR_LEAVE(thread);
    }
    if (token != 0)
        return -3;
    d_stance_setup(obj);

again:
    next_anirate(obj);
    *mk3_frame(thread, thread->frame + 1) = 0x64d;
    thread->frame = thread->frame + 1;
    return mk3_install(thread, (MK3THREADFUNC)t_d_beware);
}

/* t_retreat_wait_yes -- armv7 0x000726e8 */
long t_retreat_wait_yes(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x60b) {
        next_anirate(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x60d;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_d_beware);
    }
    if (token == 0x60d) {
        d_either_edge_a5(obj);
        if ((int32_t)obj->field30 <= 0x4f)
            return mk3_install(thread, (MK3THREADFUNC)t_d_cornered);
        RUN_PROBE(thread, obj);
        if (obj->field5c != 0)
            POP_OR_LEAVE(thread);
        obj->a10 = obj->a10 - 1;
        if (obj->a10 == 0)
            POP_OR_LEAVE(thread);
        goto facing;
    }
    if (token != 0)
        return -3;
    face_opponent(obj);
    d_walkb_setup(obj);

facing:
    am_i_facing_him(obj);
    if (obj->field5c != 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x60b;
        thread->fieldfc = 1;
        return 1;
    }
    return mk3_install(thread, (MK3THREADFUNC)t_d_turnaround);
}

/* t_stalk_wait_yes -- armv7 0x00072a2c */
long t_stalk_wait_yes(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x5f4) {
        *mk3_frame(thread, thread->frame + 1) = 0x5f5;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_check_winner_status);
    }
    if (token == 0x5f5) {
        next_anirate(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x5f7;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_d_beware);
    }
    if (token == 0x5f7) {
        RUN_PROBE(thread, obj);
        if (obj->field5c != 0)
            POP_OR_LEAVE(thread);
        obj->a10 = obj->a10 - 1;
        if (obj->a10 == 0)
            POP_OR_LEAVE(thread);
        goto facing;
    }
    if (token != 0)
        return -3;
    face_opponent(obj);
    d_walkf_setup(obj);

facing:
    am_i_facing_him(obj);
    if (obj->field5c != 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x5f4;
        thread->fieldfc = 1;
        return 1;
    }
    return mk3_install(thread, (MK3THREADFUNC)t_d_turnaround);
}

/* t_d_run_till_yes -- armv7 0x0006fbc0 */
long t_d_run_till_yes(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x4a5) {
        reduce_turbo_bar(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x4a7;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_check_winner_status);
    }
    if (token == 0x4a7) {
        next_anirate(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x4a9;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_d_beware);
    }
    if (token == 0x4a9) {
        ((MK3PROBE)(uintptr_t)obj->field48)(obj);
        if (obj->field5c != 0)
            POP_OR_LEAVE(thread);
        obj->a10 = obj->a10 - 1;
        if ((int32_t)obj->a10 <= 0)
            POP_OR_LEAVE(thread);
        goto park;
    }
    if (token != 0)
        return -3;
    run_setup(obj);
    face_opponent(obj);

park:
    *mk3_frame(thread, thread->frame + 1) = 0x4a5;
    thread->fieldfc = 1;
    return 1;
}

/* t_d_stalk_a11 -- armv7 0x00072b94 */
long t_d_stalk_a11(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x1f0) {
        *mk3_frame(thread, thread->frame + 1) = 0x1f1;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_check_winner_status);
    }
    if (token == 0x1f1) {
        next_anirate(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x1f4;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_d_beware);
    }
    if (token == 0x1f4) {
        get_x_dist(obj);
        if ((int32_t)obj->field28 < (int32_t)obj->field48)
            return mk3_install(thread, (MK3THREADFUNC)t_dist_retp);
        obj->a10 = obj->a10 - 1;
        if ((int32_t)obj->a10 <= 0)
            return mk3_install(thread, (MK3THREADFUNC)t_dist_retp);
        goto facing;
    }
    if (token != 0)
        return -3;
    face_opponent(obj);
    d_walkf_setup(obj);

facing:
    if (am_i_facing_him(obj) != 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x1f0;
        thread->fieldfc = 1;
        return 1;
    }
    return mk3_install(thread, (MK3THREADFUNC)t_d_turnaround);
}

/* t_ct_lao_spin -- armv7 0x0006d0cc */
long t_ct_lao_spin(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    int32_t  v;

    if (token == 0x143e) {
        *mk3_frame(thread, thread->frame + 1) = 0x143f;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_nr_sweep_if_u_can);
    }
    if (token == 0x1447)
        return mk3_install(thread, (MK3THREADFUNC)t_d_zap);
    if (token == 0) {
        v = *(const int16_t *)(const void *)(G_BYTES + 0x44c);
        obj->field1c = (uint32_t)v;
        if (v <= 3) {
            obj->a10     = 0xc0;
            obj->field48 = 0xc0 - 0x50;
            *mk3_frame(thread, thread->frame + 1) = 0x143e;
            thread->frame = thread->frame + 1;
            return mk3_install(thread, (MK3THREADFUNC)t_d_stalk_a11);
        }
    } else if (token != 0x143f) {
        return -3;
    }

    get_x_dist(obj);
    if ((int32_t)obj->field28 > 0x45) {
        obj->a10 = 0x20;
        *mk3_frame(thread, thread->frame + 1) = 0x1447;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_d_stance_pause);
    }
    return mk3_install(thread, (MK3THREADFUNC)t_d_zap);
}

/* t_run_in_close_now -- armv7 0x000722c8 */
long t_run_in_close_now(MK3THREAD *thread)
{
    MK3OBJ       *obj = (MK3OBJ *)thread->proc;
    MK3THREADFUNC h;
    uint32_t      token = *mk3_frame(thread, thread->frame + 1);
    int32_t       v;

    if (token == 0) {
        obj->a10     = 0x40;
        obj->field48 = 0x40;
        *mk3_frame(thread, thread->frame + 1) = 0x236;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_d_run_a11);
    }
    if (token != 0x236)
        return -3;

    is_he_airborn(obj);
    if (obj->field5c != 0)
        return mk3_install(thread, (MK3THREADFUNC)t_d_fflip_jump);
    v = *(const int16_t *)(const void *)(G_BYTES + 0x44c);
    obj->field1c = (uint32_t)v;
    if (v > 4) {
        h = (MK3THREADFUNC)t_run_in_close_hard;
    } else if (is_he_short(obj) != 0) {
        h = (MK3THREADFUNC)t_d_knee;
    } else {
        *(uint32_t *)((char *)obj + 0x68) = (uint32_t)(uintptr_t)funcs_7715;
        *(uint32_t *)((char *)obj + 0x64) = 4;
        *mk3_frame(thread, thread->frame + 1) = 0x24c;
        thread->frame = thread->frame + 1;
        h = (MK3THREADFUNC)t_random_do;
    }
    return mk3_install(thread, h);
}

/* t_drfp4 -- armv7 0x000688a0 */
long t_drfp4(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    uint32_t n;

    if (token == 0) {
        obj->field1c = 3;
        *mk3_frame(thread, thread->frame + 1) = 0x72d;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_mframew);
    }
    if (token != 0x72d)
        return -3;

    n = thread->fieldf8;
    *mk3_arg(thread, n) = obj->field40;
    thread->fieldf8 = n + 1;
    *mk3_arg(thread, n + 1) = obj->field2c;
    thread->fieldf8 = n + 2;
    *mk3_arg(thread, n + 2) = obj->field28;
    thread->fieldf8 = n + 3;
    *mk3_arg(thread, n + 3) = obj->field24;
    thread->fieldf8 = n + 4;
    POP_OR_LEAVE(thread);
}

/* t_caj_towards_me -- armv7 0x0006e5c0 */
long t_caj_towards_me(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    MK3OBJ  *part;
    uint32_t c, v;
    int32_t  d;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    get_his_y_vel(obj);
    if ((int32_t)obj->field1c >= 0)
        return mk3_install(thread, (MK3THREADFUNC)t_return_to_beware);

    get_x_dist(obj);
    part = obj->field08;
    c = part->field24;
    obj->field1c = c;
    d = (int32_t)obj->field28;
    if (c != 0xd && d <= 0x7f)
        return mk3_install(thread, (MK3THREADFUNC)t_d_hi_kick);
    if (d <= 0x8f)
        return mk3_install(thread, (MK3THREADFUNC)t_d_jump_up_kick);

    v = ochar_towards_counters[part->field24];
    obj->field1c = v;
    if (v == 0)
        return mk3_install(thread, (MK3THREADFUNC)t_return_to_beware);
    return mk3_install(thread, (MK3THREADFUNC)(uintptr_t)v);
}

/* t_dont_zap_towards_jumper -- armv7 0x00067bac */
long t_dont_zap_towards_jumper(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    int32_t  d;

    if (token == 0x3b6)
        return mk3_install(thread, (MK3THREADFUNC)t_attack_very_far_hard);
    if (token == 0x3ac) {
        d = *(const int16_t *)(const void *)(G_BYTES + 0x44c);
        obj->field1c = (uint32_t)d;
        if (d > 5)
            return mk3_install(thread, (MK3THREADFUNC)t_attack_very_far_hard);
        *(uint32_t *)((char *)obj + 0x68) = (uint32_t)(uintptr_t)funcs_8089;
        *(uint32_t *)((char *)obj + 0x64) = 4;
        *mk3_frame(thread, thread->frame + 1) = 0x3b6;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_random_do);
    }
    if (token != 0)
        return -3;
    *mk3_frame(thread, thread->frame + 1) = 0x3ac;
    thread->frame = thread->frame + 1;
    return mk3_install(thread, (MK3THREADFUNC)t_nr_drone_zone);
}

/* t_nr_jump_over_proj -- armv7 0x0006db74 */
long t_nr_jump_over_proj(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    MK3THREADFUNC h;
    uint32_t f, w;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    get_x_dist(obj);
    if ((int32_t)obj->field28 <= 0xb5)
        POP_OR_LEAVE(thread);

    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
    } else {
        mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }
    f = thread->frame;
    w = mk3_frame(thread, f + 1)[1];
    *mk3_frame(thread, f + 1) = *mk3_frame(thread, f + 2);
    mk3_frame(thread, f)[1] = w;

    get_x_dist(obj);
    if ((int32_t)obj->field28 > 0xef)
        h = (MK3THREADFUNC)t_d_flipk_over_proj;
    else
        h = (MK3THREADFUNC)t_d_fflip_kick_jump;
    return mk3_install(thread, h);
}

/* c_lao_tele -- armv7 0x0006d71c */
long c_lao_tele(MK3THREAD *thread)
{
    MK3OBJ       *obj = (MK3OBJ *)thread->proc;
    MK3THREADFUNC h;
    uint32_t      token = *mk3_frame(thread, thread->frame + 1);
    int32_t       v;

    if (token == 0x1196) {
        get_x_dist(obj);
        if ((int32_t)obj->field28 > 0x70) {
            h = (MK3THREADFUNC)t_d_lo_kick;
        } else {
            v = *(const int16_t *)(const void *)(G_BYTES + 0x44c);
            obj->field1c = (uint32_t)v;
            h = (v > 4) ? (MK3THREADFUNC)t_d_uppercut : (MK3THREADFUNC)t_d_lo_kick;
        }
        return mk3_install(thread, h);
    }
    if (token != 0)
        return -3;

    obj->field1c = (uint32_t)(uintptr_t)rpt_counter;
    ask_mr_diff(obj);
    if (obj->field5c == 0)
        return mk3_install(thread, (MK3THREADFUNC)t_return_to_beware);

    obj->field48 = (uint32_t)(uintptr_t)q_is_he_below_ground;
    obj->a10     = 0x50;
    *mk3_frame(thread, thread->frame + 1) = 0x1196;
    thread->frame = thread->frame + 1;
    return mk3_install(thread, (MK3THREADFUNC)t_stance_wait_yes);
}

/* t_d_attack_far -- armv7 0x0006f844 */
long t_d_attack_far(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x3d2) {
        *(uint32_t *)((char *)obj + 0x68) = (uint32_t)(uintptr_t)funcs_8157;
        *(uint32_t *)((char *)obj + 0x64) = 5;
        *mk3_frame(thread, thread->frame + 1) = 0x3d8;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_random_do);
    }
    if (token == 0x3d8)
        return mk3_install(thread, (MK3THREADFUNC)t_far_airborn);
    if (token != 0)
        return -3;

    q_airborn_counter(obj);
    if (obj->field5c == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x3d2;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_nr_drone_zone);
    }
    return mk3_install(thread, (MK3THREADFUNC)t_far_airborn);
}

/* t_ct_uppercut -- armv7 0x0006bb04 */
long t_ct_uppercut(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x1330) {
        obj->field1c = 0x80;
        *mk3_frame(thread, thread->frame + 1) = 0x1332;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_d_wait_nonattack);
    }
    if (token == 0x1332) {
        *mk3_frame(thread, thread->frame + 1) = 0x1333;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_nr_uppercut_if_u_can);
    }
    if (token == 0x1333) {
        *mk3_frame(thread, thread->frame + 1) = 0x1334;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_nr_sweep_if_u_can);
    }
    if (token == 0x1334)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    if (token != 0)
        return -3;
    *mk3_frame(thread, thread->frame + 1) = 0x1330;
    thread->frame = thread->frame + 1;
    return mk3_install(thread, (MK3THREADFUNC)t_d_duck_fast);
}

/* t_react_jump_table -- armv7 0x0006cbdc: look up how often to repeat a
 * reaction (repeat_table by difficulty); at or under the count it takes the
 * second word of the table the object carries at +0x68, over it it rolls
 * (d_randpers, then the promotion roll) between t_return_to_beware and the
 * first or second word */
long t_react_jump_table(MK3THREAD *thread)
{
    MK3OBJ         *obj = (MK3OBJ *)thread->proc;
    const uint32_t *tab;
    int32_t         w, v;
    uint32_t        h;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    if ((int32_t)obj->field38 > 3)
        obj->field38 = 3;
    count_q_repeats(obj);
    w = *(const int16_t *)(const void *)(G_BYTES + 0x44c);
    obj->field54 = (uint32_t)w;
    v = repeat_table[w];
    obj->field1c = (uint32_t)v;
    if (v <= (int32_t)obj->field28) {
        tab = (const uint32_t *)(uintptr_t)*(const uint32_t *)((const char *)obj + 0x68);
        h = tab[1];
        obj->field24 = h;
        return mk3_install(thread, (MK3THREADFUNC)(uintptr_t)h);
    }

    w = *(const int16_t *)(const void *)(G_BYTES + 0x44c);
    obj->field54 = (uint32_t)w;
    obj->field1c = (uint32_t)(int32_t)d_randpers[w];
    randper(obj);
    if (obj->field5c == 0)
        return mk3_install(thread, (MK3THREADFUNC)t_return_to_beware);

    obj->field1c = (uint32_t)(uintptr_t)rpt_counter;
    ask_mr_diff(obj);
    tab = (const uint32_t *)(uintptr_t)*(const uint32_t *)((const char *)obj + 0x68);
    h = (obj->field5c == 0) ? tab[0] : tab[1];
    obj->field24 = h;
    return mk3_install(thread, (MK3THREADFUNC)(uintptr_t)h);
}

/* ======================================================================
 * The fatality family (friendship, animality, fatality 1 and 2) -- one
 * template: align at the character's distance, arm q_is_he_dizzy and wait,
 * then run the move or abort -- and the a11 walkers.
 * ====================================================================== */

extern const uint32_t ochar_friendship_distances[];
extern const uint32_t ochar_animality_distances[];
extern const uint32_t ochar_fatality_distances[];
long t_fatality_align(struct MK3THREAD *thread);
long t_do_friendship(struct MK3THREAD *thread);
long t_do_animality(struct MK3THREAD *thread);
long t_do_fatality_1(struct MK3THREAD *thread);
long t_do_fatality_2(struct MK3THREAD *thread);
long t_d_fatality_abort(struct MK3THREAD *thread);
void q_my_back_to_him(MK3OBJ *obj);

/* t_drone_friendship -- armv7 0x000691a4 */
long t_drone_friendship(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x9c7) {
        if (obj->field08->field24 == 1 && obj->field00->field18 != 0x302) {
            *mk3_frame(thread, thread->frame + 1) = 0x9cb;
            thread->frame = thread->frame + 1;
            return mk3_install(thread, (MK3THREADFUNC)t_do_duck);
        }
        return mk3_install(thread, (MK3THREADFUNC)t_do_friendship);
    }
    if (token == 0x9cb)
        return mk3_install(thread, (MK3THREADFUNC)t_do_friendship);
    if (token != 0)
        return -3;

    obj->field1c = ochar_friendship_distances[obj->field08->field24];
    *mk3_frame(thread, thread->frame + 1) = 0x9c7;
    thread->frame = thread->frame + 1;
    return mk3_install(thread, (MK3THREADFUNC)t_fatality_align);
}

/* t_drone_animality, t_drone_do_fatality1/2 -- armv7 0x00069274,
 * 0x000694d4, 0x000693e0 */
long t_drone_animality(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x9f3) {
        obj->a10     = 0x40;
        obj->field48 = (uint32_t)(uintptr_t)q_is_he_dizzy;
        *mk3_frame(thread, thread->frame + 1) = 0x9f6;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_stance_wait_yes);
    }
    if (token == 0x9f6) {
        if (obj->a10 == 0)
            return mk3_install(thread, (MK3THREADFUNC)t_d_fatality_abort);
        return mk3_install(thread, (MK3THREADFUNC)t_do_animality);
    }
    if (token != 0)
        return -3;
    obj->field1c = ochar_animality_distances[obj->field08->field24];
    *mk3_frame(thread, thread->frame + 1) = 0x9f3;
    thread->frame = thread->frame + 1;
    return mk3_install(thread, (MK3THREADFUNC)t_fatality_align);
}

long t_drone_do_fatality1(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0xa33) {
        obj->field48 = (uint32_t)(uintptr_t)q_is_he_dizzy;
        obj->a10     = 0x40;
        *mk3_frame(thread, thread->frame + 1) = 0xa36;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_stance_wait_yes);
    }
    if (token == 0xa36) {
        if (obj->field5c == 0)
            return mk3_install(thread, (MK3THREADFUNC)t_d_fatality_abort);
        return mk3_install(thread, (MK3THREADFUNC)t_do_fatality_1);
    }
    if (token != 0)
        return -3;
    obj->field1c = (uint16_t)ochar_fatality_distances[obj->field08->field24];
    *mk3_frame(thread, thread->frame + 1) = 0xa33;
    thread->frame = thread->frame + 1;
    return mk3_install(thread, (MK3THREADFUNC)t_fatality_align);
}

long t_drone_do_fatality2(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0xa27) {
        obj->a10     = 0x40;
        obj->field48 = (uint32_t)(uintptr_t)q_is_he_dizzy;
        *mk3_frame(thread, thread->frame + 1) = 0xa2a;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_stance_wait_yes);
    }
    if (token == 0xa2a) {
        if (obj->a10 == 0)
            return mk3_install(thread, (MK3THREADFUNC)t_d_fatality_abort);
        return mk3_install(thread, (MK3THREADFUNC)t_do_fatality_2);
    }
    if (token != 0)
        return -3;
    obj->field1c = (uint32_t)((int32_t)ochar_fatality_distances[obj->field08->field24] >> 16);
    *mk3_frame(thread, thread->frame + 1) = 0xa27;
    thread->frame = thread->frame + 1;
    return mk3_install(thread, (MK3THREADFUNC)t_fatality_align);
}

/* t_fatality_stalk_a11 -- armv7 0x0007293c */
long t_fatality_stalk_a11(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0xa91) {
        *mk3_frame(thread, thread->frame + 1) = 0xa92;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_check_winner_status);
    }
    if (token == 0xa92) {
        next_anirate(obj);
        get_x_dist(obj);
        if ((int32_t)obj->field28 < (int32_t)obj->field48)
            return mk3_install(thread, (MK3THREADFUNC)t_dist_retp);
        obj->a10 = obj->a10 - 1;
        if (obj->a10 == 0)
            return mk3_install(thread, (MK3THREADFUNC)t_dist_retp);
        goto facing;
    }
    if (token != 0)
        return -3;
    face_opponent(obj);
    d_walkf_setup(obj);

facing:
    am_i_facing_him(obj);
    if (obj->field5c != 0) {
        *mk3_frame(thread, thread->frame + 1) = 0xa91;
        thread->fieldfc = 1;
        return 1;
    }
    return mk3_install(thread, (MK3THREADFUNC)t_d_turnaround);
}

/* t_d_retreat_a11 -- armv7 0x00072828 */
long t_d_retreat_a11(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x2ee) {
        next_anirate(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x2f0;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_d_beware);
    }
    if (token == 0x2f0) {
        d_either_edge_a5(obj);
        if ((int32_t)obj->field30 <= 0x4f)
            return mk3_install(thread, (MK3THREADFUNC)t_d_cornered);
        get_x_dist(obj);
        if ((int32_t)obj->field28 > (int32_t)obj->field48)
            return mk3_install(thread, (MK3THREADFUNC)t_dist_retp);
        obj->a10 = obj->a10 - 1;
        if ((int32_t)obj->a10 <= 0)
            return mk3_install(thread, (MK3THREADFUNC)t_dist_retp);
        goto facing;
    }
    if (token != 0)
        return -3;
    face_opponent(obj);
    d_walkb_setup(obj);

facing:
    if (am_i_facing_him(obj) != 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x2ee;
        thread->fieldfc = 1;
        return 1;
    }
    return mk3_install(thread, (MK3THREADFUNC)t_d_turnaround);
}

/* t_av_tele_explode -- armv7 0x0006efc4 */
long t_av_tele_explode(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x120d) {
        am_i_facing_him(obj);
        if (obj->field5c == 0)
            goto turn;
        goto pause;
    }
    if (token == 0x1211)
        goto pause;
    if (token == 0x1214)
        return mk3_install(thread, (MK3THREADFUNC)t_d_hi_kick);
    if (token != 0)
        return -3;
    obj->a10     = 0x10;
    obj->field48 = (uint32_t)(uintptr_t)q_my_back_to_him;
    *mk3_frame(thread, thread->frame + 1) = 0x120d;
    thread->frame = thread->frame + 1;
    return mk3_install(thread, (MK3THREADFUNC)t_stance_wait_yes);

pause:
    obj->a10 = 0x10;
    *mk3_frame(thread, thread->frame + 1) = 0x1214;
    thread->frame = thread->frame + 1;
    return mk3_install(thread, (MK3THREADFUNC)t_d_stance_pause);

turn:
    *mk3_frame(thread, thread->frame + 1) = 0x1211;
    thread->frame = thread->frame + 1;
    return mk3_install(thread, (MK3THREADFUNC)t_d_turnaround_jsrp);
}

/* t_d_upcut_finish -- armv7 0x00068fe8 */
long t_d_upcut_finish(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x97d) {
        *mk3_frame(thread, thread->frame + 1) = 0x97e;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_do_duck);
    }
    if (token == 0x97e) {
        *mk3_frame(thread, thread->frame + 1) = 0x97f;
        thread->fieldfc = 8;
        return 8;
    }
    if (token == 0x97f) {
        q_is_he_dizzy(obj);
        if (obj->field5c != 0)
            return mk3_install(thread, (MK3THREADFUNC)t_d_uppercut);
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }
    if (token != 0)
        return -3;
    obj->a10 = 0x10;
    *mk3_frame(thread, thread->frame + 1) = 0x97d;
    thread->frame = thread->frame + 1;
    return mk3_install(thread, (MK3THREADFUNC)t_d_stance_pause);
}

/* ======================================================================
 * Block, turnaround and pause states; the orb picker. Batch 20.
 * ====================================================================== */

long t_do_block_hi(struct MK3THREAD *thread);
long t_run_in_fk(struct MK3THREAD *thread);
long t_run_in_and_slam(struct MK3THREAD *thread);
long t_jade_anti_orb(struct MK3THREAD *thread);
long t_scorp_anti_orb(struct MK3THREAD *thread);
long t_block_orb(struct MK3THREAD *thread);
long t_run_under_flykick(struct MK3THREAD *thread);
long t_d_fflip_kick_jump(struct MK3THREAD *thread);
long t_d_propell_attack_now(struct MK3THREAD *thread);
long t_d_zap_now(struct MK3THREAD *thread);
void q_is_kick_over(MK3OBJ *obj);

/* t_block_orb -- armv7 0x0006f934 */
long t_block_orb(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0xf3b) {
        *mk3_frame(thread, thread->frame + 1) = 0xf3c;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_wait_proj_spawn);
    }
    if (token == 0xf3c) {
        obj->field48 = (uint32_t)(uintptr_t)q_is_proj_gone;
        obj->a10     = 0x50;
        *mk3_frame(thread, thread->frame + 1) = 0xf3f;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_d_wait_yes_still);
    }
    if (token == 0xf3f)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    if (token != 0)
        return -3;
    face_opponent(obj);
    *mk3_frame(thread, thread->frame + 1) = 0xf3b;
    thread->frame = thread->frame + 1;
    return mk3_install(thread, (MK3THREADFUNC)t_do_block_hi);
}

/* t_d_turnaround_jsrp -- armv7 0x000716d8 */
long t_d_turnaround_jsrp(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    uint32_t w;

    if (token == 0x1b3) {
        *mk3_frame(thread, thread->frame + 1) = 0x1b4;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_d_beware);
    }
    if (token == 0x1b4) {
        next_anirate(obj);
        w = *(const uint32_t *)(uintptr_t)obj->field40;
        obj->field1c = w;
        if (w != 0)
            goto park;
        POP_OR_LEAVE(thread);
    }
    if (token != 0)
        return -3;
    if (am_i_facing_him(obj) != 0)
        POP_OR_LEAVE(thread);
    stop_me_player(obj);
    obj->field40 = 3;
    get_char_ani(obj);
    obj->field1c = 2;
    init_anirate(obj);

park:
    *mk3_frame(thread, thread->frame + 1) = 0x1b3;
    thread->fieldfc = 1;
    return 1;
}

/* t_flykick_heading_up -- armv7 0x0006edf4 */
long t_flykick_heading_up(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x12f9) {
        obj->field48 = (uint32_t)(uintptr_t)q_is_kick_over;
        obj->a10     = 0x30;
        *mk3_frame(thread, thread->frame + 1) = 0x12fc;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_d_wait_yes_still);
    }
    if (token == 0x12fc) {
        *mk3_frame(thread, thread->frame + 1) = 0x12fe;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_nr_uppercut_if_u_can);
    }
    if (token == 0x12fe)
        return mk3_install(thread, (MK3THREADFUNC)t_run_in_close_now);
    if (token != 0)
        return -3;

    get_his_dog(obj);
    if ((int32_t)obj->field1c > 0x38)
        return mk3_install(thread, (MK3THREADFUNC)t_run_under_flykick);
    *mk3_frame(thread, thread->frame + 1) = 0x12f9;
    thread->frame = thread->frame + 1;
    return mk3_install(thread, (MK3THREADFUNC)t_do_duck);
}

/* t_d_block_projectile -- armv7 0x00068ccc */
long t_d_block_projectile(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x851) {
        *mk3_frame(thread, thread->frame + 1) = 0x853;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_do_block_hi);
    }
    if (token == 0x853) {
        obj->field48 = (uint32_t)(uintptr_t)q_is_proj_gone;
        obj->a10     = 0x50;
        *mk3_frame(thread, thread->frame + 1) = 0x856;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_d_wait_yes_still);
    }
    if (token == 0x856)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    if (token != 0)
        return -3;
    obj->field48 = (uint32_t)(uintptr_t)q_is_proj_gone;
    obj->a10     = 0x30;
    *mk3_frame(thread, thread->frame + 1) = 0x851;
    thread->frame = thread->frame + 1;
    return mk3_install(thread, (MK3THREADFUNC)t_stance_wait_no);
}

/* t_d_stance_pause -- armv7 0x000720d4 */
long t_d_stance_pause(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x2aa) {
        *mk3_frame(thread, thread->frame + 1) = 0x2ab;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_check_winner_status);
    }
    if (token == 0x2ab) {
        if (am_i_facing_him(obj) != 0) {
            *mk3_frame(thread, thread->frame + 1) = 0x2af;
            thread->fieldfc = 1;
            return 1;
        }
        return mk3_install(thread, (MK3THREADFUNC)t_d_turnaround);
    }
    if (token == 0x2af) {
        obj->a10 = obj->a10 - 1;
        if ((int32_t)obj->a10 <= 0)
            POP_OR_LEAVE(thread);
        goto again;
    }
    if (token != 0)
        return -3;
    d_stance_setup(obj);

again:
    next_anirate(obj);
    *mk3_frame(thread, thread->frame + 1) = 0x2aa;
    thread->frame = thread->frame + 1;
    return mk3_install(thread, (MK3THREADFUNC)t_d_beware);
}

/* c_fast_orb -- armv7 0x0006dcb4: the character (field24 - 4, a tbb of 15)
 * picks the answer to an orb; the rest decide by distance */
long c_fast_orb(MK3THREAD *thread)
{
    MK3OBJ       *obj = (MK3OBJ *)thread->proc;
    MK3THREADFUNC h;
    uint32_t      c;
    int32_t       d;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = (uint32_t)(uintptr_t)rpt_promoves;
    ask_mr_diff(obj);
    if (obj->field5c == 0)
        return mk3_install(thread, (MK3THREADFUNC)t_return_to_beware);

    c = obj->field08->field24 - 4;
    if (c <= 0xe) {
        switch (c) {
        case 0:
        case 4:
            return mk3_install(thread, (MK3THREADFUNC)t_d_zap_now);
        case 3:
        case 7:
        case 10:
        case 13:
            return mk3_install(thread, (MK3THREADFUNC)t_d_propell_attack_now);
        case 12:
            return mk3_install(thread, (MK3THREADFUNC)t_jade_anti_orb);
        case 14:
            return mk3_install(thread, (MK3THREADFUNC)t_scorp_anti_orb);
        default:
            break;
        }
    }

    get_x_dist(obj);
    d = (int32_t)obj->field28;
    if (d > 0xf0)
        h = (MK3THREADFUNC)t_run_in_fk;
    else if (d <= 0xaf)
        h = (MK3THREADFUNC)t_run_in_and_slam;
    else if (d > 0xe0)
        h = (MK3THREADFUNC)t_d_fflip_kick_jump;
    else
        h = (MK3THREADFUNC)t_block_orb;
    return mk3_install(thread, h);
}

/* ======================================================================
 * t_avoid_agressive_bastards, t_asb2, t_d_run_a11, t_d_backup_jsrp. Batch 21.
 * ====================================================================== */

long t_duck_turnaround(struct MK3THREAD *thread);
long t_d_sweep_kick(struct MK3THREAD *thread);
void do_next_a9_frame(MK3OBJ *obj);
void is_he_attacking(MK3OBJ *obj);

/* the shared "pop one level and slide the pair above down" */
#define POP_AND_SLIDE(thread)                                                  \
    do {                                                                       \
        uint32_t f_, w_;                                                       \
        f_ = (thread)->frame;                                                  \
        w_ = mk3_frame((thread), f_ + 1)[1];                                   \
        *mk3_frame((thread), f_ + 1) = *mk3_frame((thread), f_ + 2);           \
        mk3_frame((thread), f_)[1] = w_;                                       \
    } while (0)

/* t_avoid_agressive_bastards -- armv7 0x0006d3c0 */
long t_avoid_agressive_bastards(MK3THREAD *thread)
{
    MK3OBJ       *obj = (MK3OBJ *)thread->proc;
    MK3THREADFUNC h;
    int32_t       d;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    d = *(const int16_t *)(const void *)(G_BYTES + 0x44c);
    obj->field1c = (uint32_t)d;
    if (d <= 1)
        POP_OR_LEAVE(thread);

    obj->field1c = obj->field00->field18;
    if (obj->field1c != 0x309)
        POP_OR_LEAVE(thread);

    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
    } else {
        mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }
    POP_AND_SLIDE(thread);

    get_x_dist(obj);
    if ((int32_t)obj->field28 > 0x47)
        h = (MK3THREADFUNC)t_d_sweep_kick;
    else
        h = (MK3THREADFUNC)t_d_slam;
    return mk3_install(thread, h);
}

/* t_asb2 -- armv7 0x0006f0b8 */
long t_asb2(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x11cc)
        goto park;
    if (token == 0x11ce) {
        am_i_facing_him(obj);
        if (obj->field5c == 0) {
            *mk3_frame(thread, thread->frame + 1) = 0x11d8;
            thread->frame = thread->frame + 1;
            return mk3_install(thread, (MK3THREADFUNC)t_duck_turnaround);
        }
        is_he_attacking(obj);
        if (obj->field5c != 0)
            goto park;
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }
    if (token == 0x11d8) {
        obj->field1c = 0x80;
        *mk3_frame(thread, thread->frame + 1) = 0x11da;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_d_wait_nonattack);
    }
    if (token == 0x11da)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    if (token != 0)
        return -3;
    *mk3_frame(thread, thread->frame + 1) = 0x11cc;
    thread->frame = thread->frame + 1;
    return mk3_install(thread, (MK3THREADFUNC)t_do_duck);

park:
    *mk3_frame(thread, thread->frame + 1) = 0x11ce;
    thread->fieldfc = 1;
    return 1;
}

/* t_d_run_a11 -- armv7 0x0006fcf4 */
long t_d_run_a11(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x210) {
        reduce_turbo_bar(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x212;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_check_winner_status);
    }
    if (token == 0x212) {
        next_anirate(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x215;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_d_beware);
    }
    if (token == 0x215) {
        get_x_dist(obj);
        if ((int32_t)obj->field28 < (int32_t)obj->field48)
            return mk3_install(thread, (MK3THREADFUNC)t_dist_retp);
        obj->a10 = obj->a10 - 1;
        if ((int32_t)obj->a10 <= 0)
            return mk3_install(thread, (MK3THREADFUNC)t_dist_retp);
        goto facing;
    }
    if (token != 0)
        return -3;
    run_setup(obj);

facing:
    if (am_i_facing_him(obj) != 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x210;
        thread->fieldfc = 1;
        return 1;
    }
    return mk3_install(thread, (MK3THREADFUNC)t_d_turnaround);
}

/* t_d_backup_jsrp -- armv7 0x000719fc: a four-frame backup animation played
 * in pieces with do_next_a9_frame, a10 remembering where it started */
long t_d_backup_jsrp(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x25a) {
        *mk3_frame(thread, thread->frame + 1) = 0x25b;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_d_beware);
    }
    if (token == 0x25b) {
        obj->field40 = obj->a10 + 4;
        do_next_a9_frame(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x260;
        thread->fieldfc = 2;
        return 2;
    }
    if (token == 0x260) {
        *mk3_frame(thread, thread->frame + 1) = 0x261;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_d_beware);
    }
    if (token == 0x261) {
        obj->field40 = obj->a10;
        do_next_a9_frame(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x264;
        thread->fieldfc = 2;
        return 2;
    }
    if (token == 0x264)
        POP_OR_LEAVE(thread);
    if (token != 0)
        return -3;

    obj->field20 = 0x301;
    obj->field00->field18 = 0x301;
    obj->field40 = 4;
    get_char_ani(obj);
    obj->a10 = obj->field40;
    obj->field40 = obj->field40 + 8;
    do_next_a9_frame(obj);
    *mk3_frame(thread, thread->frame + 1) = 0x25a;
    thread->fieldfc = 2;
    return 2;
}

/* ======================================================================
 * t_boss_branch, t_nr_drone_zone, t_flykick_towards_me, c_lia_zap. Batch 22.
 * ====================================================================== */

extern const uint32_t motaro_branches[];
extern const uint32_t sk_branches[];
long t_flykick_heading_up(struct MK3THREAD *thread);
long t_drone_zone(struct MK3THREAD *thread);
long t_d_bflip_jump(struct MK3THREAD *thread);
long t_stw_proj_proc(struct MK3THREAD *thread);
long t_do_jump_up(struct MK3THREAD *thread);
void is_flyk_close(MK3OBJ *obj);

/* t_boss_branch -- armv7 0x0006c2cc: for Motaro (0x18) and Shao Kahn (0x19)
 * the handler is looked up in that boss's branch table by the number the
 * object carries in +0x64; anyone else just pops */
long t_boss_branch(MK3THREAD *thread)
{
    MK3OBJ         *obj = (MK3OBJ *)thread->proc;
    const uint32_t *tab;
    uint32_t        c, h;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = *(const uint32_t *)((const char *)obj + 0x64);
    c = obj->field08->field24;
    obj->field20 = c;
    if (c != 0x18 && c != 0x19)
        POP_OR_LEAVE(thread);

    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
    } else {
        mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }
    POP_AND_SLIDE(thread);

    tab = (c == 0x18) ? motaro_branches : sk_branches;
    obj->field20 = (uint32_t)(uintptr_t)tab;
    h = tab[obj->field1c];
    obj->field1c = h;
    return mk3_install(thread, (MK3THREADFUNC)(uintptr_t)h);
}

/* t_nr_drone_zone -- armv7 0x0006cdac */
long t_nr_drone_zone(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    int32_t  v;

    if (token == 0x476)
        return mk3_install(thread, (MK3THREADFUNC)t_drone_zone);
    if (token != 0)
        return -3;

    v = *(const int16_t *)(const void *)(G_BYTES + 0x460);
    obj->field1c = (uint32_t)v;
    if (v <= 3)
        POP_OR_LEAVE(thread);

    obj->field1c = 0x12c;
    randper(obj);
    if (obj->field5c == 0)
        POP_OR_LEAVE(thread);

    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
    } else {
        mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }
    POP_AND_SLIDE(thread);

    obj->field1c = 0xc0;
    obj->field48 = 0xc0 + 0x10;
    *mk3_frame(thread, thread->frame + 1) = 0x476;
    thread->frame = thread->frame + 1;
    return mk3_install(thread, (MK3THREADFUNC)t_d_retreat_a11);
}

/* t_flykick_towards_me -- armv7 0x0006f220 */
long t_flykick_towards_me(MK3THREAD *thread)
{
    MK3OBJ       *obj = (MK3OBJ *)thread->proc;
    MK3THREADFUNC h;
    uint32_t      token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x12b4) {
        get_x_dist(obj);
        if ((int32_t)obj->field28 <= 0xa0) {
            is_he_airborn(obj);
            if (obj->field5c != 0)
                return mk3_install(thread, (MK3THREADFUNC)t_d_block);
        }
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }
    if (token != 0)
        return -3;

    q_will_he_reach_me(obj);
    if (obj->field5c != 0)
        return mk3_install(thread, (MK3THREADFUNC)t_d_block);

    get_x_dist(obj);
    if ((int32_t)obj->field28 <= 0x7f)
        return mk3_install(thread, (MK3THREADFUNC)t_d_block);

    obj->field1c = ((MK3OBJ *)(uintptr_t)obj->field00->him)->field1c;
    if ((int32_t)obj->field1c < 0) {
        h = (MK3THREADFUNC)t_flykick_heading_up;
    } else {
        get_his_dog(obj);
        if ((int32_t)obj->field1c <= 0x27) {
            h = (MK3THREADFUNC)t_flykick_heading_up;
        } else {
            obj->a10     = 0x30;
            obj->field48 = (uint32_t)(uintptr_t)is_flyk_close;
            *mk3_frame(thread, thread->frame + 1) = 0x12b4;
            thread->frame = thread->frame + 1;
            h = (MK3THREADFUNC)t_stance_wait_yes;
        }
    }
    return mk3_install(thread, h);
}

/* c_lia_zap -- armv7 0x00070fd8 */
long c_lia_zap(MK3THREAD *thread)
{
    MK3OBJ       *obj = (MK3OBJ *)thread->proc;
    MK3THREADFUNC h;
    uint32_t      token = *mk3_frame(thread, thread->frame + 1);
    int32_t       d;

    if (token == 0xdf3) {
        obj->a10     = 0x40;
        obj->field48 = 0x40;
        *mk3_frame(thread, thread->frame + 1) = 0xdf6;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_d_run_a11);
    }
    if (token == 0xdf6)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    if (token == 0xdfc)
        return mk3_install(thread, (MK3THREADFUNC)t_d_uppercut);
    if (token == 0xdf1) {
        obj->field48 = 0;
        *mk3_frame(thread, thread->frame + 1) = 0xdf3;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_do_jump_up);
    }
    if (token != 0)
        return -3;

    get_x_dist(obj);
    d = (int32_t)obj->field28;
    if (d <= 0xbf) {
        obj->a10     = 0x40;
        obj->field48 = 0x40;
        *mk3_frame(thread, thread->frame + 1) = 0xdfc;
        thread->frame = thread->frame + 1;
        h = (MK3THREADFUNC)t_d_run_a11;
    } else if (d > 0xf0) {
        h = (MK3THREADFUNC)t_d_fflip_kick_jump;
    } else {
        q_am_i_cornered(obj);
        if (obj->field5c == 0) {
            h = (MK3THREADFUNC)t_d_bflip_jump;
        } else {
            *mk3_frame(thread, thread->frame + 1) = 0xdf1;
            thread->frame = thread->frame + 1;
            h = (MK3THREADFUNC)t_stw_proj_proc;
        }
    }
    return mk3_install(thread, h);
}

/* ======================================================================
 * t_ct_square, c_angle_jump, t_drone_entry, t_standard_zap_counter. Batch 23.
 * ====================================================================== */

long t_caj_corner(struct MK3THREAD *thread);
long t_caj_towards_me(struct MK3THREAD *thread);
long t_d_zap_jump(struct MK3THREAD *thread);
long t_d_lk_hi_zap_jump(struct MK3THREAD *thread);
long t_d_attack(struct MK3THREAD *thread);
long t_d_wait_finish_react(struct MK3THREAD *thread);
long t_d_backup_jsrp(struct MK3THREAD *thread);
long t_hangout_check(struct MK3THREAD *thread);
long t_boss_branch(struct MK3THREAD *thread);
long t_return_and_4get(struct MK3THREAD *thread);
long t_indian_reflect(struct MK3THREAD *thread);
long t_lk_zap_low(struct MK3THREAD *thread);
long t_jade_anti_zap(struct MK3THREAD *thread);
long t_cyrax_counter_zap(struct MK3THREAD *thread);
long t_d_block_projectile(struct MK3THREAD *thread);
long t_run_then_flipk(struct MK3THREAD *thread);
long t_run_then_duck_under(struct MK3THREAD *thread);
long t_nr_jump_over_proj(struct MK3THREAD *thread);
long rpt_angle(struct MK3THREAD *thread);
long am_i_short(MK3OBJ *obj);
void d_to_normal(MK3OBJ *obj);
void stance_setup(MK3OBJ *obj);
void q_square_lower(MK3OBJ *obj);
void q_is_he_dropping(MK3OBJ *obj);
void q_is_he_cornered(MK3OBJ *obj);

/* t_ct_square -- armv7 0x00071340 */
long t_ct_square(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x1243)
        goto dropping;
    if (token == 0x1248) {
        get_x_dist(obj);
        if ((int32_t)obj->field28 > 0x4f)
            return mk3_install(thread, (MK3THREADFUNC)t_d_zap);
        obj->field48 = (uint32_t)(uintptr_t)q_square_lower;
        obj->a10     = 0x40;
        *mk3_frame(thread, thread->frame + 1) = 0x1252;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_stance_wait_yes);
    }
    if (token == 0x1252) {
        d_either_edge_a5(obj);
        if ((int32_t)obj->field30 > 0x4f)
            return mk3_install(thread, (MK3THREADFUNC)t_d_hi_kick);
        return mk3_install(thread, (MK3THREADFUNC)t_d_uppercut);
    }
    if (token == 0x1241) {
        *mk3_frame(thread, thread->frame + 1) = 0x1243;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_d_turnaround_jsrp);
    }
    if (token != 0)
        return -3;

    get_x_dist(obj);
    if ((int32_t)obj->field28 > 0xfc)
        goto dropping;
    obj->a10     = 0x40;
    obj->field48 = (uint32_t)(uintptr_t)q_my_back_to_him;
    *mk3_frame(thread, thread->frame + 1) = 0x1241;
    thread->frame = thread->frame + 1;
    return mk3_install(thread, (MK3THREADFUNC)t_stance_wait_yes);

dropping:
    obj->a10     = 0x40;
    obj->field48 = (uint32_t)(uintptr_t)q_is_he_dropping;
    *mk3_frame(thread, thread->frame + 1) = 0x1248;
    thread->frame = thread->frame + 1;
    return mk3_install(thread, (MK3THREADFUNC)t_stance_wait_yes);
}

/* c_angle_jump -- armv7 0x00070a18 */
long c_angle_jump(MK3THREAD *thread)
{
    MK3OBJ       *obj = (MK3OBJ *)thread->proc;
    MK3OBJ       *part;
    MK3THREADFUNC h;
    uint32_t      token = *mk3_frame(thread, thread->frame + 1);
    uint32_t      c;
    int32_t       d;

    if (token == 0xc24) {
        part = obj->field08;
        obj->field1c = 6;
        c = part->field24;
        obj->field20 = c;
        if (c == 4)
            return mk3_install(thread, (MK3THREADFUNC)t_d_zap_jump);
        obj->field1c = 0x19;
        c = part->field24;
        obj->field20 = c;
        if (c == 5)
            return mk3_install(thread, (MK3THREADFUNC)t_d_zap_jump);
        obj->field1c = 0x16;
        c = part->field24;
        obj->field20 = c;
        if (c == 0xd)
            return mk3_install(thread, (MK3THREADFUNC)t_d_lk_hi_zap_jump);
        return mk3_install(thread, (MK3THREADFUNC)t_d_zap);
    }
    if (token != 0)
        return -3;

    obj->field1c = (uint32_t)(uintptr_t)rpt_angle;
    ask_mr_diff(obj);
    if (obj->field5c == 0)
        return mk3_install(thread, (MK3THREADFUNC)t_return_to_beware);

    obj->field1c = ((MK3OBJ *)(uintptr_t)obj->field00->him)->field18;
    if (obj->field1c == 0)
        return mk3_install(thread, (MK3THREADFUNC)t_return_and_4get);

    is_towards_me(obj);
    if (obj->field5c != 0)
        return mk3_install(thread, (MK3THREADFUNC)t_caj_towards_me);
    q_is_he_cornered(obj);
    if (obj->field5c != 0)
        return mk3_install(thread, (MK3THREADFUNC)t_caj_corner);

    get_x_dist(obj);
    d = (int32_t)obj->field28;
    if (d <= 0x5f) {
        h = (MK3THREADFUNC)t_d_flip_punch_jump;
    } else if (d > 0xaf) {
        h = (MK3THREADFUNC)t_return_to_beware;
    } else {
        *mk3_frame(thread, thread->frame + 1) = 0xc24;
        thread->frame = thread->frame + 1;
        h = (MK3THREADFUNC)t_dont_zap_teles;
    }
    return mk3_install(thread, h);
}

/* t_drone_entry -- armv7 0x00072ccc */
long t_drone_entry(MK3THREAD *thread)
{
    MK3OBJ       *obj = (MK3OBJ *)thread->proc;
    MK3THREADFUNC h;
    uint32_t      token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x36f) {
        *mk3_frame(thread, thread->frame + 1) = 0x370;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_d_beware);
    }
    if (token == 0x370) {
        stance_setup(obj);
        if (am_i_facing_him(obj) == 0)
            return mk3_install(thread, (MK3THREADFUNC)t_d_turnaround);
        *(uint32_t *)((char *)obj + 0x64) = 0x10;
        *mk3_frame(thread, thread->frame + 1) = 0x378;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_boss_branch);
    }
    if (token == 0x378) {
        if (am_i_short(obj) != 0) {
            *mk3_frame(thread, thread->frame + 1) = 0x37b;
            thread->frame = thread->frame + 1;
            return mk3_install(thread, (MK3THREADFUNC)t_d_backup_jsrp);
        }
        goto hangout;
    }
    if (token == 0x37b)
        goto hangout;
    if (token == 0x381) {
        if (q_is_he_reacting(obj) != 0)
            h = (MK3THREADFUNC)t_d_wait_finish_react;
        else
            h = (MK3THREADFUNC)t_d_attack;
        return mk3_install(thread, h);
    }
    if (token != 0)
        return -3;

    reset_proc_stack(thread);
    d_to_normal(obj);
    *mk3_frame(thread, thread->frame + 1) = 0x36f;
    thread->frame = thread->frame + 1;
    return mk3_install(thread, (MK3THREADFUNC)t_check_winner_status);

hangout:
    *mk3_frame(thread, thread->frame + 1) = 0x381;
    thread->frame = thread->frame + 1;
    return mk3_install(thread, (MK3THREADFUNC)t_hangout_check);
}

/* t_standard_zap_counter -- armv7 0x0006de68: by character (a tbb of 15 from
 * character 3), otherwise by distance */
long t_standard_zap_counter(MK3THREAD *thread)
{
    MK3OBJ       *obj = (MK3OBJ *)thread->proc;
    MK3THREADFUNC h;
    uint32_t      token = *mk3_frame(thread, thread->frame + 1);
    uint32_t      c;
    int32_t       d;

    if (token == 0xe89) {
        *mk3_frame(thread, thread->frame + 1) = 0xe8a;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_nr_jump_over_proj);
    }
    if (token == 0xe8a)
        return mk3_install(thread, (MK3THREADFUNC)t_duck_under_proj);
    if (token != 0)
        return -3;

    c = obj->field08->field24 - 3;
    if (c <= 0xe) {
        switch (c) {
        case 0:
            return mk3_install(thread, (MK3THREADFUNC)t_indian_reflect);
        case 4:
        case 8:
        case 11:
        case 14:
            return mk3_install(thread, (MK3THREADFUNC)t_d_propell_attack_now);
        case 5:
            return mk3_install(thread, (MK3THREADFUNC)t_cyrax_counter_zap);
        case 10:
            return mk3_install(thread, (MK3THREADFUNC)t_lk_zap_low);
        case 13:
            return mk3_install(thread, (MK3THREADFUNC)t_jade_anti_zap);
        default:
            break;
        }
    }

    obj->field1c = ((MK3OBJ *)(uintptr_t)obj->field00->him)->field24;
    if (obj->field1c == 8)
        return mk3_install(thread, (MK3THREADFUNC)t_d_block_projectile);

    get_x_dist(obj);
    d = (int32_t)obj->field28;
    if (d <= 0x9f)
        h = (MK3THREADFUNC)t_duck_under_proj;
    else if (d > 0xf0)
        h = (MK3THREADFUNC)t_run_then_flipk;
    else if (d > 0xd0)
        h = (MK3THREADFUNC)t_run_then_duck_under;
    else {
        *mk3_frame(thread, thread->frame + 1) = 0xe89;
        thread->frame = thread->frame + 1;
        h = (MK3THREADFUNC)t_nr_sweep_if_u_can;
    }
    return mk3_install(thread, h);
}

/* ======================================================================
 * t_drfp1 and t_d_beware. Batch 24.
 * ====================================================================== */

extern const uint32_t *P32 const ochar_cat_tables[];
long t_drfp2(struct MK3THREAD *thread);
long t_drfp3(struct MK3THREAD *thread);
long t_drfp1(struct MK3THREAD *thread);
long strike_check_a0(MK3OBJ *obj);

/* t_drfp1 -- armv7 0x00071be8: strike-check the value four words down the
 * argument stack; a miss moves on to t_drfp3 / t_drfp2, a hit counts the
 * proc's repeat counter (+0x60) down and parks for field1c frames */
long t_drfp1(MK3THREAD *thread)
{
    MK3OBJ     *obj = (MK3OBJ *)thread->proc;
    MK3OBJPROC *proc;
    uint32_t    token = *mk3_frame(thread, thread->frame + 1);
    uint32_t    n;

    if (token == 0x706) {
        obj->field1c = 3;
        *mk3_frame(thread, thread->frame + 1) = 0x709;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_mframew);
    }
    if (token == 0x709) {
        obj->field1c = *mk3_arg(thread, thread->fieldf8 - 4);
        strike_check_a0(obj);
        if (obj->field5c == 0)
            return mk3_install(thread, (MK3THREADFUNC)t_drfp2);
        proc = obj->field00;
        n = *(const uint32_t *)((const char *)proc + 0x60) - 1;
        obj->field1c = n;
        if (n == 0)
            return mk3_install(thread, (MK3THREADFUNC)t_drfp2);
        *(uint32_t *)((char *)proc + 0x60) = n;
        obj->field1c = obj->field00->field1c;
        *mk3_frame(thread, thread->frame + 1) = 0x716;
        thread->fieldfc = obj->field1c;
        return (long)obj->field1c;
    }
    if (token == 0x716)
        return mk3_install(thread, (MK3THREADFUNC)t_drfp1);
    if (token == 0x6f8) {
        obj->field1c = *mk3_arg(thread, thread->fieldf8 - 4);
        strike_check_a0(obj);
        if (obj->field5c == 0)
            return mk3_install(thread, (MK3THREADFUNC)t_drfp3);
        proc = obj->field00;
        n = *(const uint32_t *)((const char *)proc + 0x60) - 1;
        obj->field1c = n;
        if (n == 0)
            return mk3_install(thread, (MK3THREADFUNC)t_drfp3);
        *(uint32_t *)((char *)proc + 0x60) = n;
        obj->field1c = obj->field00->field1c;
        *mk3_frame(thread, thread->frame + 1) = 0x706;
        thread->fieldfc = obj->field1c;
        return (long)obj->field1c;
    }
    if (token != 0)
        return -3;
    obj->field1c = 3;
    *mk3_frame(thread, thread->frame + 1) = 0x6f8;
    thread->frame = thread->frame + 1;
    return mk3_install(thread, (MK3THREADFUNC)t_mframew);
}

/* t_d_beware -- armv7 0x0006c40c: watch his action. When he starts a new
 * one that has an entry in ochar_cat_tables[character][action >> 8][action &
 * 0xff], save the level that is about to be popped (handler at proc+0x6c,
 * token at +0x70, a10 and field48 at +0x74/+0x78 -- t_return_to_beware puts
 * them back) and run that entry's routine in its place */
long t_d_beware(MK3THREAD *thread)
{
    MK3OBJ         *obj = (MK3OBJ *)thread->proc;
    MK3OBJPROC     *proc;
    MK3OBJ         *part;
    const uint32_t *row;
    const uint32_t *tab;
    uint32_t        act, last, f, h, w, tok;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    last = *(const uint16_t *)(const void *)(G_BYTES + 0x45c);
    obj->field1c = (uint32_t)(int32_t)(int16_t)last;
    if (last != 0)
        POP_OR_LEAVE(thread);

    get_his_action(obj);
    if (obj->field1c == 0)
        POP_OR_LEAVE(thread);

    proc = obj->field00;
    act  = obj->field20;
    obj->field1c = proc->field5c;
    if (proc->field5c == act)
        POP_OR_LEAVE(thread);

    proc->field5c = act;
    part = obj->field08;
    obj->field24 = (uint32_t)((int32_t)act >> 8);
    obj->field1c = part->field24;
    tab = ochar_cat_tables[part->field24];
    obj->field28 = (uint32_t)(uintptr_t)tab;
    row = (const uint32_t *)(uintptr_t)tab[(int32_t)act >> 8];
    obj->field24 = (uint32_t)(uintptr_t)row;
    if (row == 0)
        POP_OR_LEAVE(thread);

    obj->field20 = act & 0xffu;
    w = row[act & 0xffu];
    obj->field20 = w;
    if (w == 0)
        POP_OR_LEAVE(thread);

    obj->field28 = part->field24;
    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
    } else {
        mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }

    proc = obj->field00;
    h   = mk3_frame(thread, thread->frame)[1];
    tok = *mk3_frame(thread, thread->frame + 1);
    *(uint32_t *)((char *)proc + 0x6c) = h;
    *(uint32_t *)((char *)proc + 0x70) = tok;

    POP_AND_SLIDE(thread);

    *(uint32_t *)((char *)obj->field00 + 0x74) = obj->a10;
    *(uint32_t *)((char *)obj->field00 + 0x78) = obj->field48;
    mk3_frame(thread, thread->frame)[1] = obj->field20;
    *mk3_frame(thread, thread->frame + 1) = 0;
    (void)f;
    return 0;
}

/* ======================================================================
 * t_d_unblock, t_wait_proj_pass, t_fatality_align, t_drone_execute_fatality.
 * Batch 25.
 * ====================================================================== */

long t_d_backup_jump(struct MK3THREAD *thread);
long t_d_fflip_jsrp(struct MK3THREAD *thread);
long t_d_bflip_jsrp(struct MK3THREAD *thread);
long t_fatality_stalk_a11(struct MK3THREAD *thread);
long t_d_fatality_cornered(struct MK3THREAD *thread);
long t_d_fatality_abort(struct MK3THREAD *thread);
long t_drone_mercy(struct MK3THREAD *thread);
long t_drone_do_fatality1(struct MK3THREAD *thread);
long t_drone_do_fatality2(struct MK3THREAD *thread);
long t_dist_retp(struct MK3THREAD *thread);
long t_wait_proj_pass(struct MK3THREAD *thread);
void get_his_proj_proc(MK3OBJ *obj);
void d_walkb_setup(MK3OBJ *obj);
void MKEvent_Add(long type, long subtype, long param, long player);
extern int rand(void);

#define B25_S16(p, o) (*(const int16_t *)(const void *)((const char *)(p) + (o)))
#define B25_U32(p, o) (*(const uint32_t *)(const void *)((const char *)(p) + (o)))

/* "put the next token in the slot above, climb a level, run beware there" */
#define B25_PUSH_BEWARE(thread, next)                                          \
    do {                                                                       \
        *mk3_frame((thread), (thread)->frame + 1) = (next);                    \
        (thread)->frame = (thread)->frame + 1;                                 \
        return mk3_install((thread), (MK3THREADFUNC)t_d_beware);               \
    } while (0)

#define B25_PARK(thread, next)                                                 \
    do {                                                                       \
        *mk3_frame((thread), (thread)->frame + 1) = (next);                    \
        (thread)->fieldfc = 1;                                                 \
        return 1;                                                              \
    } while (0)

/* t_d_unblock -- armv7 0x000717d4: a chain of beware waits around a short
 * backup animation; the answer to am_i_short picks how it ends */
long t_d_unblock(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    switch (token) {
    case 0x7a1: B25_PARK(thread, 0x7a2);
    case 0x7a2: B25_PUSH_BEWARE(thread, 0x7a3);
    case 0x7a3: B25_PARK(thread, 0x7a4);
    case 0x7a4: B25_PUSH_BEWARE(thread, 0x7a5);
    case 0x7a5: B25_PARK(thread, 0x7a6);
    case 0x7a6: B25_PUSH_BEWARE(thread, 0x7a7);
    case 0x7a7:
        obj->field40 = obj->a10;
        do_next_a9_frame(obj);
        B25_PARK(thread, 0x7ac);
    case 0x7ac: B25_PUSH_BEWARE(thread, 0x7ad);
    case 0x7ad: B25_PARK(thread, 0x7ae);
    case 0x7ae: B25_PUSH_BEWARE(thread, 0x7af);
    case 0x7af: B25_PARK(thread, 0x7b0);
    case 0x7b0: B25_PUSH_BEWARE(thread, 0x7b1);
    case 0x7b1:
        am_i_short(obj);
        if (obj->field5c == 0)
            return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
        return mk3_install(thread, (MK3THREADFUNC)t_d_backup_jump);
    case 0:
        break;
    default:
        return -3;
    }

    obj->field20 = 0;
    obj->field00->field18 = 0;
    obj->field40 = 0xc;
    am_i_short(obj);
    if (obj->field5c != 0)
        obj->field40 = 6;
    get_char_ani(obj);
    obj->a10     = obj->field40;
    obj->field40 = obj->field40 + 4;
    do_next_a9_frame(obj);
    B25_PUSH_BEWARE(thread, 0x7a1);
}

/* t_wait_proj_pass -- armv7 0x0006ffd4: wait for his projectile to pass.
 * While it has no body (+0x84) keep waiting; once it has, compare where it
 * and I are, mirrored when it faces left (flag 0x10 in its part's +0x28);
 * if it is already by me, call it a 0x505 and leave, else wait on */
long t_wait_proj_pass(MK3THREAD *thread)
{
    MK3OBJ     *obj = (MK3OBJ *)thread->proc;
    MK3OBJ     *p;
    MK3OBJPROC *lr;
    const char *body;
    const char *part;
    int32_t     a, b, x, w;
    uint32_t    f;
    int         left;

    f = *mk3_frame(thread, thread->frame + 1);
    if (f == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0xfbf;
        thread->fieldfc = 1;
        return 1;
    }
    if (f != 0xfbf)
        return -3;

    get_his_proj_proc(obj);
    p = (MK3OBJ *)(uintptr_t)obj->field1c;
    if (p == 0)
        POP_OR_LEAVE(thread);

    lr = p->field00;
    body = (const char *)(uintptr_t)lr->field84;
    if (body == 0)
        return mk3_install(thread, (MK3THREADFUNC)t_wait_proj_pass);

    part = (const char *)p->field08;
    obj->field28 = (uint32_t)(int32_t)B25_S16(part, 0xe);
    a = (int32_t)B25_U32(body, 0);
    obj->field24 = (uint32_t)a;
    b = (int32_t)B25_U32(body, 8);
    obj->field2c = (uint32_t)b;
    w = (int32_t)B25_U32(part, 0x28);
    obj->field34 = (uint32_t)w;
    left = (w & 0x10) != 0;
    if (left) {
        a = -a;
        obj->field24 = (uint32_t)a;
        b = -b;
        obj->field2c = (uint32_t)b;
    }
    x = (int32_t)obj->field28 + (int32_t)obj->field24 - (int32_t)obj->field2c;
    obj->field28 = (uint32_t)x;
    a = (int32_t)B25_S16(obj->field08, 0xe);
    obj->field24 = (uint32_t)a;
    w = (int32_t)B25_U32(part, 0x28);
    if (w & 0x10) {
        obj->field28 = (uint32_t)a;
        obj->field24 = (uint32_t)x;
        b = a;
        a = x;
    } else {
        b = x;
    }
    obj->field34 = (uint32_t)w;
    if (a > b)
        return mk3_install(thread, (MK3THREADFUNC)t_wait_proj_pass);
    obj->field20 = 0x505;
    lr->field18 = 0x505;
    POP_OR_LEAVE(thread);
}

/* t_fatality_align -- armv7 0x000724d8: close to (or back off from) the
 * distance he was at when this started, then hand over to the fatality */
long t_fatality_align(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    int32_t  d;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0xab2) {
        if (obj->a10 != 0)
            POP_OR_LEAVE(thread);
        return mk3_install(thread, (MK3THREADFUNC)t_d_fatality_abort);
    }
    if (token == 0xac2)
        goto cornered;
    if (token == 0xac9) {
        next_anirate(obj);
        get_x_dist(obj);
        if ((int32_t)obj->field28 <= (int32_t)obj->field48)
            goto step;
        return mk3_install(thread, (MK3THREADFUNC)t_dist_retp);
    }
    if (token == 0xaae)
        goto stalk;
    if (token != 0)
        return -3;

    obj->field48 = obj->field1c;
    get_x_dist(obj);
    if ((int32_t)obj->field1c >= (int32_t)obj->field28)
        goto cornered;
    d = (int32_t)obj->field28 - (int32_t)obj->field1c;
    if (d <= 0xff)
        goto stalk;
    *mk3_frame(thread, thread->frame + 1) = 0xaae;
    thread->frame = thread->frame + 1;
    return mk3_install(thread, (MK3THREADFUNC)t_d_fflip_jsrp);

stalk:
    obj->a10 = 0xc0;
    *mk3_frame(thread, thread->frame + 1) = 0xab2;
    thread->frame = thread->frame + 1;
    return mk3_install(thread, (MK3THREADFUNC)t_fatality_stalk_a11);

cornered:
    obj->a10 = 0x80;
    q_am_i_cornered(obj);
    if (obj->field5c != 0)
        return mk3_install(thread, (MK3THREADFUNC)t_d_fatality_cornered);
    get_x_dist(obj);
    d = (int32_t)obj->field28 - (int32_t)obj->field48;
    obj->field28 = (uint32_t)d;
    if (d < 0) {
        d = -d;
        obj->field28 = (uint32_t)d;
    }
    if (d > 0xff) {
        *mk3_frame(thread, thread->frame + 1) = 0xac2;
        thread->frame = thread->frame + 1;
        return mk3_install(thread, (MK3THREADFUNC)t_d_bflip_jsrp);
    }
    face_opponent(obj);
    d_walkb_setup(obj);
    B25_PARK(thread, 0xac9);

step:
    obj->a10 = obj->a10 - 1;
    if (obj->a10 != 0)
        B25_PARK(thread, 0xac9);
    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
    } else {
        mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }
    POP_AND_SLIDE(thread);
    return mk3_install(thread, (MK3THREADFUNC)t_d_fatality_abort);
}

/* t_drone_execute_fatality -- armv7 0x000707cc: when no mercy is already
 * under way, one time in ten (when both fighters have health left) offer
 * mercy; otherwise pick fatality 1 or 2 on a coin toss */
long t_drone_execute_fatality(MK3THREAD *thread)
{
    uint32_t r;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    if (*(const int16_t *)(const void *)(G_BYTES + 0x45a) == 0) {
        r = (uint32_t)random32() % 10000u;
        if (r < 1000u && *(const int32_t *)(const void *)H > 0 &&
            *(const int32_t *)(const void *)(H + 4) > 0) {
            MKEvent_Add(4, 0x45, 0, 0);
            return mk3_install(thread, (MK3THREADFUNC)t_drone_mercy);
        }
    }
    if ((rand() & 0x80) != 0)
        return mk3_install(thread, (MK3THREADFUNC)t_drone_do_fatality1);
    return mk3_install(thread, (MK3THREADFUNC)t_drone_do_fatality2);
}
