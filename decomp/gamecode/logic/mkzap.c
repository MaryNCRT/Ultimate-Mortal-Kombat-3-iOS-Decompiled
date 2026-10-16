/*
 * mkzap.c -- gamecode/logic/mkzap.c, decompiled.
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

long t_doice3(struct MK3THREAD *thread);
long t_new_scorpion_spear_proc(struct MK3THREAD *thread);
long t_new_smoke_spear_proc(struct MK3THREAD *thread);
long t_new_spear_proc(struct MK3THREAD *thread);
long t_robo_bomb_full(struct MK3THREAD *thread);
long t_robo_bomb_mid(struct MK3THREAD *thread);
long t_roc3(struct MK3THREAD *thread);
long t_scorp_rope_pull(struct MK3THREAD *thread);
long t_stz1(struct MK3THREAD *thread);
long t_lao_hat_proc(struct MK3THREAD *thread);
long tl_bomb3(struct MK3THREAD *thread);
long tl_jzap3(struct MK3THREAD *thread);
long tl_ssp2(struct MK3THREAD *thread);

long t_r_null_speared(struct MK3THREAD *thread);         /* mkreact.c */
long t_rhat_sleep(struct MK3THREAD *thread);              /* mkreact.c */
long t_tugged_in_by_spear(struct MK3THREAD *thread);
long t_scorp_waiting_sleep(struct MK3THREAD *thread);

void *GetProcFunc(MK3OBJ *obj);
void fastxfer_thread(MK3OBJ *obj, MK3THREAD *thread);
void ReallyKillHisProjectile(MK3OBJ *obj);
void takeover_him(MK3OBJ *obj);
long next_anirate(MK3OBJ *obj);
void pose_him_a0(MK3OBJ *obj);
void stop_him(MK3OBJ *obj);
void ground_him(MK3OBJ *obj);
void randu(MK3OBJ *obj);
void xfer_otherguy(MK3OBJ *obj);
void player_swpal(MK3OBJ *obj, uint32_t frozen);

extern MK3THREAD *mytc;  /* slot 0x0016566c -> 0x0038ef3c; the host variable holds the slot's contents, as in mk3.c */

/* t_new_smoke_spear_proc -- armv7 0x00074d3c, 64 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0xffffffb8
 *      obj->field20 = 0x15            (0xffffffb8 + 0x5d, wrapped)
 *
 * The second constant is reached by adding 0x5d to the first rather than
 * loading it: 0xffffffb8 + 0x5d = 0x15, with the carry falling off the end
 * of a 32-bit register. One `adds` instead of a second `mvn`.
 *      frame[frame].handler = t_new_spear_proc
 *      frame[frame+1].w0 = 0
 */

long t_new_smoke_spear_proc(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0xffffffb8;
    obj->field20 = 0x15;                /* 0xffffffb8 + 0x5d, wrapped */

    return mk3_push_handler(thread, (MK3THREADFUNC)t_new_spear_proc);
}

/* tl_do_smoke_spear -- armv7 0x00074db8, 64 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field38 = t_new_smoke_spear_proc
 *      frame[frame].handler = tl_ssp2
 *      frame[frame+1].w0 = 0
 */

long tl_do_smoke_spear(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field38 = (uint32_t)(uintptr_t)t_new_smoke_spear_proc;

    return mk3_push_handler(thread, (MK3THREADFUNC)tl_ssp2);
}

/* tl_do_scorpion_spear -- armv7 0x00074df8, 64 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field38 = t_new_scorpion_spear_proc
 *      frame[frame].handler = tl_ssp2
 *      frame[frame+1].w0 = 0
 */

long tl_do_scorpion_spear(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field38 = (uint32_t)(uintptr_t)t_new_scorpion_spear_proc;

    return mk3_push_handler(thread, (MK3THREADFUNC)tl_ssp2);
}

/* tl_do_jade_zap_ret -- armv7 0x00075030, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x1
 *      frame[frame].handler = tl_jzap3
 *      frame[frame+1].w0 = 0
 */

long tl_do_jade_zap_ret(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x1;

    return mk3_push_handler(thread, (MK3THREADFUNC)tl_jzap3);
}

/* tl_do_jade_zap_lo -- armv7 0x0007506c, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x5000
 *      frame[frame].handler = tl_jzap3
 *      frame[frame+1].w0 = 0
 */

long tl_do_jade_zap_lo(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x5000;

    return mk3_push_handler(thread, (MK3THREADFUNC)tl_jzap3);
}

/* tl_do_st_zap1 -- armv7 0x000751d0, 64 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field48 = 0x1
 *      obj->field20 = 0x10
 *      frame[frame].handler = t_stz1
 *      frame[frame+1].w0 = 0
 *
 * **This and the two below are one constant apart**, instruction for
 * instruction, and the two fields come from the same register: `movs #N` then
 * `adds #0xf`. So the variant number and the action number are welded together
 * and always differ by 0xf -- `projectile_jumps` entries 19, 20 and 21 all land
 * on `t_stz1`, which reads the two fields and does the work.
 *
 * **They also refute a guess made while reading `t_st_zap_jsrp`.** That routine
 * adds `obj->a10` to a resolved animation cursor, and these three were the
 * obvious candidates for setting it. They do not touch `a10` at all, and they
 * install `t_stz1` rather than descending into that routine. What fills `a10`
 * before it runs is still unknown, and a port must not assume zero.
 */

long tl_do_st_zap1(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field48 = 0x1;
    obj->field20 = 0x10;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_stz1);
}

/* tl_do_st_zap2 -- armv7 0x00075210, 64 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field48 = 0x2
 *      obj->field20 = 0x11
 *      frame[frame].handler = t_stz1
 *      frame[frame+1].w0 = 0
 */

long tl_do_st_zap2(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field48 = 0x2;
    obj->field20 = 0x11;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_stz1);
}

/* tl_do_st_zap3 -- armv7 0x00075250, 64 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field48 = 0x3
 *      obj->field20 = 0x12
 *      frame[frame].handler = t_stz1
 *      frame[frame+1].w0 = 0
 */

long tl_do_st_zap3(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field48 = 0x3;
    obj->field20 = 0x12;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_stz1);
}

/* t_robo_open_chest -- armv7 0x00075290, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x4
 *      frame[frame].handler = t_roc3
 *      frame[frame+1].w0 = 0
 */

long t_robo_open_chest(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x4;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_roc3);
}


/* -------------------------------------------------------------- tl_do_robo_net
 *
 * armv7 0x00079c6c, 332 bytes.  **Complete.**
 *
 * The free runs `zap_init_special_act` and descends into
 * `t_robo_open_chest` from `0xef9`. `0xef9` is the launch: `field1c = G +
 * 0x408` into `update_tsl`, the sound, `field40` saved on the argument
 * stack, `field38 = t_net_proc` into `create_proj_proc`, and then the new
 * slave's OWN `field40` is set from what `get_char_ani2` resolves this
 * object's `field40 = 1` into -- the same "propagate the pose onto the
 * slave" shape `tl_do_motaro_zap` uses, just through `get_char_ani2`
 * instead of a bare copy. `adjust_xy_a5` nudges by `0x30`
 * (`field1c = -0x18`) before the saved `field40` is restored and an
 * eleven-tick bare wait leads to `0xf24`. `0xf24` parks the thrower as a
 * sitting duck and waits twenty-six more for `0xf26`, whose own re-entry
 * installs `t_backwards_ani`.
 *
 *      slot = frame[frame+1].w0
 *                                          ; 0:    free, then 0xef9
 *                                          ; 0xef9: launch, then 0xf24 (wait)
 *                                          ; 0xf24: wait, then 0xf26 (wait)
 *                                          ; 0xf26: install t_backwards_ani
 *      if (slot == 0xef9) {
 *          obj->field1c = G + 0x408 ; update_tsl(obj)
 *          obj->field1c = 5 ; ochar_sound(obj)
 *          args[fieldf8] = obj->field40 ; fieldf8++
 *          obj->field38 = t_net_proc ; slave = create_proj_proc(obj)
 *          obj->field40 = 1 ; get_char_ani2(obj)
 *          obj->field30 = slave->field08 ; slave->field40 = obj->field40
 *          obj->field1c = -0x18 ; obj->field20 = 0x30 ; adjust_xy_a5(obj)
 *          fieldf8-- ; obj->field40 = args[fieldf8]
 *          token 0xf24 ; fieldfc = 0xb ; return 0xb
 *      }
 *      if (slot < 0xef9) {
 *          if (slot != 0) return -3
 *          obj->a10 = 0 ; obj->field20 = 0xa ; zap_init_special_act(obj)
 *          token 0xef9 ; frame++ ; install t_robo_open_chest ; return 0
 *      }
 *      if (slot == 0xf24) {
 *          i_am_a_sitting_duck(obj)
 *          token 0xf26 ; fieldfc = 0x1a ; return 0x1a
 *      }
 *      if (slot != 0xf26) return -3
 *      obj->field40 = 0 ; get_char_ani2(obj) ; obj->field1c = 4
 *      install t_backwards_ani ; return 0
 */
extern GAMESTATE *G;                       /* pointer slot 0x000f357c */
void ochar_sound(MK3OBJ *obj);
void zap_init_special_act(MK3OBJ *obj);
void update_tsl(MK3OBJ *obj);
void get_char_ani2(MK3OBJ *obj);
void adjust_xy_a5(MK3OBJ *obj);
void i_am_a_sitting_duck(MK3OBJ *obj);
MK3OBJ *create_proj_proc(MK3OBJ *obj);
long t_net_proc(struct MK3THREAD *thread);
long t_backwards_ani(struct MK3THREAD *thread);         /* pointer slot 0x000f37c4 */
long t_robo_open_chest(struct MK3THREAD *thread);

long tl_do_robo_net(MK3THREAD *thread)
{
    MK3OBJ   *obj  = (MK3OBJ *)thread->proc;
    uint32_t *args = (uint32_t *)(void *)thread->args;
    uint32_t  frame = thread->frame;
    uint32_t  slot  = *mk3_frame(thread, frame + 1);

    if (slot == 0xef9) {
        MK3OBJ *slave;

        obj->field1c = (uint32_t)(uintptr_t)((char *)G + 0x408);
        update_tsl(obj);

        obj->field1c = 5;
        ochar_sound(obj);

        args[thread->fieldf8] = obj->field40;
        thread->fieldf8 = thread->fieldf8 + 1;

        obj->field38 = (uint32_t)(uintptr_t)t_net_proc;
        slave = create_proj_proc(obj);

        obj->field40 = 1;
        get_char_ani2(obj);

        obj->field30 = (uint32_t)(uintptr_t)slave->field08;
        slave->field40 = obj->field40;

        obj->field1c = (uint32_t)~0x17;          /* -0x18 */
        obj->field20 = (uint32_t)(~0x17 + 0x48); /* 0x30 */
        adjust_xy_a5(obj);

        thread->fieldf8 = thread->fieldf8 - 1;
        obj->field40 = args[thread->fieldf8];

        *mk3_frame(thread, frame + 1) = 0xf24;
        thread->fieldfc = 0xb;
        return 0xb;
    }

    if (slot < 0xef9) {
        if (slot != 0)
            return -3;

        obj->a10    = 0;
        obj->field20 = 0xa;
        zap_init_special_act(obj);

        *mk3_frame(thread, frame + 1) = 0xef9;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_robo_open_chest;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot == 0xf24) {
        i_am_a_sitting_duck(obj);

        *mk3_frame(thread, frame + 1) = 0xf26;
        thread->fieldfc = 0x1a;
        return 0x1a;
    }

    if (slot != 0xf26)
        return -3;

    obj->field40 = 0;
    get_char_ani2(obj);
    obj->field1c = 4;

    return mk3_install(thread, (MK3THREADFUNC)t_backwards_ani);
}


/* -------------------------------------------------------------------------- t_net_proc
 *
 * armv7 0x000788f4, 368 bytes.  **Complete.**
 *
 * `tl_do_robo_net`'s own `field38` callback, the net that flies out and
 * traps the opponent. The free throws (`field08->field1c = 0x8000`,
 * `field1c = 0x60000`) and pushes `tl_projectile_flight` under `0xebf`,
 * the same launcher shape every other zap in this file uses.
 *
 * `0xebf` is the catch: `stop_a8`, `init_anirate`, and the GrObj's own
 * `field0c`/`field10` high halves copied straight from the opponent's --
 * `MK3_SET_FIELD0E`/`MK3_SET_FIELD12(obj->field08, MK3_FIELD0E/12(him))`,
 * pinning the net's part to where the opponent already is -- then
 * `multi_adjust_xy` and a re-arm under `0xedc`.
 *
 * `0xedc` is a poll: `next_anirate`, then check whether the opponent's own
 * installed proc is still `t_net_sleep` (`GetProcFunc(obj->field00->field00)`
 * -- the identity check every sleeping-latch site in this file uses). While
 * it still is, jump straight back into `0xebf`'s own catch code and wait
 * again -- the opponent hasn't started fighting the net yet. Once it isn't,
 * announce it (`ochar_sound`), reset the animation state and push
 * `t_mframew` under `0xeed`, which just installs `tl_delete_proj_and_die`:
 * the net is done once the opponent breaks free of the sleep it induces.
 *
 * `obj->field1c = obj->field00->him` right before the halfword copy is a
 * dead store -- overwritten by `-0x20` a few lines later without ever being
 * read, the same kind of leftover this file has flagged before.
 */
long t_net_sleep(struct MK3THREAD *thread);
void stop_a8(MK3OBJ *part);
void init_anirate(MK3OBJ *obj);
void multi_adjust_xy(MK3OBJ *obj);
void find_ani2_part2(MK3OBJ *obj);
void find_part2(MK3OBJ *obj);
void set_proj_vel(MK3OBJ *obj);
long tl_projectile_flight(MK3THREAD *thread);
long t_mframew(struct MK3THREAD *thread);
long tl_delete_proj_and_die(MK3THREAD *thread);

long t_net_proc(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t slot  = *mk3_frame(thread, frame + 1);
    MK3OBJ  *him;

    if (slot == 0xeed)   /* corrected: was t_mframew, the routine 0xedc pushes */
        return mk3_install(thread, (MK3THREADFUNC)tl_delete_proj_and_die);

    if (slot == 0xedc) {
        next_anirate(obj);

        if ((uintptr_t)GetProcFunc(obj->field00->field00) ==
                (uintptr_t)t_net_sleep)
            goto catch_net;

        obj->field1c = 0x15;
        ochar_sound(obj);

        obj->field40 = 1;
        find_ani2_part2(obj);

        find_part2(obj);

        obj->field1c = 2;

        *mk3_frame(thread, frame + 1) = 0xeed;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot == 0xebf) {
    catch_net:
        stop_a8(obj->field08);

        obj->field40 = 1;
        find_ani2_part2(obj);

        obj->field1c = 4;
        init_anirate(obj);

        him = (MK3OBJ *)(void *)(uintptr_t)obj->field00->him;

        obj->field1c = obj->field00->him;      /* dead: rewritten below */

        MK3_SET_FIELD0E(obj->field08, MK3_FIELD0E(him));
        MK3_SET_FIELD12(obj->field08, MK3_FIELD12(him));

        obj->field1c = (uint32_t)~0x1f;
        obj->field20 = (uint32_t)~0x1f + 0x62;
        multi_adjust_xy(obj);

        *mk3_frame(thread, frame + 1) = 0xedc;
        thread->fieldfc = 1;
        return 1;
    }

    if (slot != 0)
        return -3;

    obj->field40 = 1;
    get_char_ani2(obj);

    obj->field08->field1c = 0x8000;
    obj->field1c           = 0x8000 + 0x58000;
    obj->field20            = 2;
    set_proj_vel(obj);

    obj->field48 = 0x11;

    *mk3_frame(thread, frame + 1) = 0xebf;
    thread->frame = thread->frame + 1;   /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)tl_projectile_flight;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* tl_do_bomb_mid -- armv7 0x000752cc, 64 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field48 = t_robo_bomb_mid
 *      frame[frame].handler = tl_bomb3
 *      frame[frame+1].w0 = 0
 */

long tl_do_bomb_mid(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field48 = (uint32_t)(uintptr_t)t_robo_bomb_mid;

    return mk3_push_handler(thread, (MK3THREADFUNC)tl_bomb3);
}

/* tl_do_robo_bomb -- armv7 0x0007530c, 64 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field48 = t_robo_bomb_full
 *      frame[frame].handler = tl_bomb3
 *      frame[frame+1].w0 = 0
 */

long tl_do_robo_bomb(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field48 = (uint32_t)(uintptr_t)t_robo_bomb_full;

    return mk3_push_handler(thread, (MK3THREADFUNC)tl_bomb3);
}


/* ------------------------------------------------------------------------- tl_bomb3
 *
 * armv7 0x00079f50, 332 bytes.  **Complete.**
 *
 * `tl_do_bomb_mid`/`tl_do_robo_bomb`'s own descent target, and both callers
 * hand it `field48` pre-loaded with which "after the bomb" handler to run
 * (`t_robo_bomb_mid` or `t_robo_bomb_full`) -- read back out below as the
 * projectile's `field38` hit callback.
 *
 * The free (`field20 = 0xd`, `a10 = 0`, `zap_init_special_act`) pushes
 * `t_robo_open_chest` under `0xc3c`: the chest has to open before the bomb
 * comes out.
 *
 * `0xc3c` is a two-part traffic check. First `CountThreads(0x20)`: more than
 * one robot-class thread already running means launching now would stack
 * bombs, so it just re-poses as a sitting duck and waits 32 ticks under
 * `0xc86`. At most one, and it checks a second population --
 * `CountThreads(field00->field08 + 0x700)`, the strength-indexed pid this
 * fighter's own attack thread runs under -- and if that's still busy too, it
 * takes the exact same wait. Only when both are clear does it actually
 * launch: `field38 = field48` (the caller's chosen after-bomb handler,
 * finally read), `create_proj_proc`, `field40` saved/restored across the
 * call the same way every other spawn site in this file already does,
 * `field30 = slave->field08`, `adjust_xy_a5`, then a sitting-duck wait of
 * 22 ticks under `0xc81`.
 *
 * `0xc81` and `0xc86` both just install `t_robo_close_chest` -- the
 * traffic-check wait and the post-launch wait converge on the same close,
 * whether or not a bomb actually came out.
 */
long t_robo_open_chest(struct MK3THREAD *thread);
long t_robo_close_chest(struct MK3THREAD *thread);
long CountThreads(uint32_t pid);

long tl_bomb3(MK3THREAD *thread)
{
    MK3OBJ   *obj   = (MK3OBJ *)thread->proc;
    uint32_t *args  = (uint32_t *)(void *)thread->args;
    uint32_t  frame = thread->frame;
    uint32_t  slot  = *mk3_frame(thread, frame + 1);

    if (slot == 0xc3c) {
        obj->field28 = (uint32_t)CountThreads(0x20);

        if ((int32_t)obj->field28 > 1)
            goto busy;

        if (CountThreads(obj->field00->field08 + 0x700) > 0)
            goto busy;

        {
        MK3OBJ *slave;

        obj->field38 = obj->field48;
        slave = create_proj_proc(obj);

        args[thread->fieldf8] = obj->field40;
        thread->fieldf8 = thread->fieldf8 + 1;

        obj->field40 = 4;
        get_char_ani2(obj);

        slave->field40 = obj->field40;
        obj->field30   = (uint32_t)(uintptr_t)slave->field08;

        obj->field1c = 0;
        obj->field20 = 0x28;
        adjust_xy_a5(obj);

        thread->fieldf8 = thread->fieldf8 - 1;
        obj->field40 = args[thread->fieldf8];

        i_am_a_sitting_duck(obj);

        *mk3_frame(thread, frame + 1) = 0xc81;
        thread->fieldfc = 0x16;
        return 0x16;
        }

busy:
        i_am_a_sitting_duck(obj);

        *mk3_frame(thread, frame + 1) = 0xc86;
        thread->fieldfc = 0x20;
        return 0x20;
    }

    if (slot == 0xc81 || slot == 0xc86)
        return mk3_install(thread, (MK3THREADFUNC)t_robo_close_chest);

    if (slot != 0)
        return -3;

    obj->field20 = 0xd;
    obj->a10     = 0;
    zap_init_special_act(obj);

    *mk3_frame(thread, frame + 1) = 0xc3c;
    thread->frame = thread->frame + 1;   /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_robo_open_chest;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* tl_do_sky_ice_front -- armv7 0x000755b4, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field48 = 0xffffffa0
 *      frame[frame].handler = t_doice3
 *      frame[frame+1].w0 = 0
 */

long tl_do_sky_ice_front(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field48 = 0xffffffa0;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_doice3);
}

/* tl_do_sky_ice_behind -- armv7 0x000755f0, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field48 = 0x60
 *      frame[frame].handler = t_doice3
 *      frame[frame+1].w0 = 0
 */

long tl_do_sky_ice_behind(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field48 = 0x60;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_doice3);
}





/* --------------------------------------------------------------------
 * What the readers could prove. See tools/pushfn.py, which executes
 * a body symbolically, and tools/microfn.py, which matches whole
 * bodies against templates. Both refuse anything they cannot account
 * for instruction by instruction.
 * -------------------------------------------------------------------- */

long t_doice5(struct MK3THREAD *thread);
long tl_projectile_flight_call(struct MK3THREAD *thread);

/* t_new_scorpion_spear_proc -- armv7 0x00074d7c, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0   (the register the guard proved)
 *      obj->field20 = 0   (the register the guard proved)
 *      frame[frame].handler = t_new_spear_proc
 *      frame[frame+1].w0 = 0
 */

long t_new_scorpion_spear_proc(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0;   /* the guard proved this register */
    obj->field20 = 0;   /* the guard proved this register */

    return mk3_push_handler(thread, (MK3THREADFUNC)t_new_spear_proc);
}

/* tl_do_jade_zap_med -- armv7 0x000750e8, 56 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0   (the register the guard proved)
 *      frame[frame].handler = tl_jzap3
 *      frame[frame+1].w0 = 0
 */

long tl_do_jade_zap_med(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0;   /* the guard proved this register */

    return mk3_push_handler(thread, (MK3THREADFUNC)tl_jzap3);
}

/* tl_do_sky_ice_on -- armv7 0x0007557c, 56 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field48 = 0   (the register the guard proved)
 *      frame[frame].handler = t_doice5
 *      frame[frame+1].w0 = 0
 */

long tl_do_sky_ice_on(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field48 = 0;   /* the guard proved this register */

    return mk3_push_handler(thread, (MK3THREADFUNC)t_doice5);
}

/* tl_projectile_flight -- armv7 0x0007562c, 56 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field34 = 0   (the register the guard proved)
 *      frame[frame].handler = tl_projectile_flight_call
 *      frame[frame+1].w0 = 0
 */

long tl_projectile_flight(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field34 = 0;   /* the guard proved this register */

    return mk3_push_handler(thread, (MK3THREADFUNC)tl_projectile_flight_call);
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

long t_sai3(struct MK3THREAD *thread);

/* t_air_sai_proc -- armv7 0x00074cbc, 64 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x1a
 *      obj->field20 = 0x10
 *      frame[frame].handler = t_sai3
 *      frame[frame+1].w0 = 0
 */

long t_air_sai_proc(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x1a;
    obj->field20 = 0x10;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_sai3);
}

/* t_sai_proc -- armv7 0x00074cfc, 64 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x3a
 *      obj->field20 = 0x16
 *      frame[frame].handler = t_sai3
 *      frame[frame+1].w0 = 0
 */

long t_sai_proc(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x3a;
    obj->field20 = 0x16;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_sai3);
}


/* -------------------------------------------------------------- tl_mileena_air_zap
 *
 * armv7 0x0007b730, 276 bytes.  **Complete.**
 *
 * Five states. The free runs `zap_air_init_special` (`field20 =
 * obj->field00->field18 = 0x25`), poses animation `0x14` through
 * `find_ani2_part2`, steps one frame, and waits five ticks from `0x1de`
 * with no push. `0x1de` pushes a plain `t_mframew` wait from `0x1e1`, and
 * `0x1e1` is the launch: `field38 = t_air_sai_proc` into
 * `create_proj_proc`, then another bare five-tick wait for `0x1e7`.
 * `0x1e7` pushes `t_mframew` again from `0x1e9`, whose own re-entry
 * installs `t_drop_down_land`.
 *
 *      slot = frame[frame+1].w0
 *                                          ; 0:     free, then 0x1de (wait)
 *                                          ; 0x1de: wait, then 0x1e1
 *                                          ; 0x1e1: launch, then 0x1e7 (wait)
 *                                          ; 0x1e7: wait, then 0x1e9
 *                                          ; 0x1e9: install t_drop_down_land
 *      if (slot == 0x1e1) {
 *          obj->field38 = t_air_sai_proc ; create_proj_proc(obj)
 *          token 0x1e7 ; fieldfc = 5 ; return 5
 *      }
 *      if (slot < 0x1e1) {
 *          if (slot == 0) {
 *              obj->a10 = 0
 *              obj->field20 = obj->field00->field18 = 0x25
 *              zap_air_init_special(obj)
 *              obj->field40 = 0x14 ; find_ani2_part2(obj)
 *              do_next_a9_frame(obj)
 *              token 0x1de ; fieldfc = 5 ; return 5
 *          }
 *          if (slot != 0x1de) return -3
 *          obj->field1c = 3
 *          token 0x1e1 ; frame++ ; install t_mframew ; return 0
 *      }
 *      if (slot == 0x1e7) {
 *          obj->field1c = 3
 *          token 0x1e9 ; frame++ ; install t_mframew ; return 0
 *      }
 *      if (slot != 0x1e9) return -3
 *      install t_drop_down_land ; return 0
 */
void zap_air_init_special(MK3OBJ *obj);
void find_ani2_part2(MK3OBJ *obj);
long do_next_a9_frame(MK3OBJ *obj);
long t_mframew(struct MK3THREAD *thread);
long t_drop_down_land(struct MK3THREAD *thread);        /* pointer slot 0x000f33d4 */
MK3OBJ *create_proj_proc(MK3OBJ *obj);

long tl_mileena_air_zap(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t slot  = *mk3_frame(thread, frame + 1);

    if (slot == 0x1e1) {
        obj->field38 = (uint32_t)(uintptr_t)t_air_sai_proc;
        create_proj_proc(obj);

        *mk3_frame(thread, frame + 1) = 0x1e7;
        thread->fieldfc = 5;
        return 5;
    }

    if (slot < 0x1e1) {
        if (slot == 0) {
            obj->a10 = 0;
            obj->field20 = 0x25;
            obj->field00->field18 = 0x25;
            zap_air_init_special(obj);

            obj->field40 = 0x14;
            find_ani2_part2(obj);
            do_next_a9_frame(obj);

            *mk3_frame(thread, frame + 1) = 0x1de;
            thread->fieldfc = 5;
            return 5;
        }

        if (slot != 0x1de)
            return -3;

        obj->field1c = 3;

        *mk3_frame(thread, frame + 1) = 0x1e1;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot == 0x1e7) {
        obj->field1c = 3;

        *mk3_frame(thread, frame + 1) = 0x1e9;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot != 0x1e9)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_drop_down_land);
}


/* ------------------------------------------------------------------- tl_do_mileena_zap
 *
 * armv7 0x0007ca14, 284 bytes.  **Complete.**
 *
 * The free tests `am_i_airborn` before anything else and, when it answers
 * yes, tail-installs `tl_mileena_air_zap` on the spot -- the ground and
 * air throws share this one entry point, and the fork happens before a
 * single field is touched. Grounded, it poses through `pose2_a9_manual`
 * and waits seven ticks (no push) for `0x1fb`, which pushes a plain
 * `t_mframew` wait from `0x1fe`. `0x1fe` is the launch, `field38 =
 * t_sai_proc` into `create_proj_proc`, tagging the announced action to
 * `0x604` (the same pair-write `i_am_a_sitting_duck` and
 * `tl_do_proj_sitting_duck` use) before a thirty-seven-tick bare wait for
 * `0x208`, the ordinary pop-or-exit-at-the-bottom floor.
 *
 *      slot = frame[frame+1].w0
 *                                          ; 0:    free; airborne -> tl_mileena_air_zap
 *                                          ;       grounded, then 0x1fb (wait)
 *                                          ; 0x1fb: wait, then 0x1fe
 *                                          ; 0x1fe: launch, then 0x208 (wait)
 *                                          ; 0x208: pop, or exit at the bottom
 *      if (slot == 0x1fb) {
 *          obj->field1c = 3
 *          token 0x1fe ; frame++ ; install t_mframew ; return 0
 *      }
 *      if (slot < 0x1fb) {
 *          if (slot != 0) return -3
 *          obj->field1c = 8 ; ochar_sound(obj)
 *          if (am_i_airborn(obj)) install tl_mileena_air_zap ; return 0
 *          obj->a10 = 0 ; obj->field20 = 0x24 ; zap_init_special_act(obj)
 *          obj->field40 = 0x14 ; pose2_a9_manual(obj)
 *          token 0x1fb ; fieldfc = 7 ; return 7
 *      }
 *      if (slot == 0x1fe) {
 *          obj->field38 = t_sai_proc ; create_proj_proc(obj)
 *          obj->field1c = obj->field00->field18 = 0x604
 *          token 0x208 ; fieldfc = 0x25 ; return 0x25
 *      }
 *      if (slot != 0x208) return -3
 *      pop a level, or t_local_reaction_exit at the bottom
 */
void pose2_a9_manual(MK3OBJ *obj);
long am_i_airborn(MK3OBJ *obj);
long t_sai_proc(struct MK3THREAD *thread);
long t_local_reaction_exit(struct MK3THREAD *thread);   /* pointer slot 0x000f3708 */

long tl_do_mileena_zap(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t slot  = *mk3_frame(thread, frame + 1);

    if (slot == 0x1fb) {
        obj->field1c = 3;

        *mk3_frame(thread, frame + 1) = 0x1fe;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot < 0x1fb) {
        if (slot != 0)
            return -3;

        obj->field1c = 8;
        ochar_sound(obj);

        if (am_i_airborn(obj) != 0)
            return mk3_install(thread, (MK3THREADFUNC)tl_mileena_air_zap);

        obj->a10    = 0;
        obj->field20 = 0x24;
        zap_init_special_act(obj);

        obj->field40 = 0x14;
        pose2_a9_manual(obj);

        *mk3_frame(thread, frame + 1) = 0x1fb;
        thread->fieldfc = 7;
        return 7;
    }

    if (slot == 0x1fe) {
        obj->field38 = (uint32_t)(uintptr_t)t_sai_proc;
        create_proj_proc(obj);

        obj->field1c = 0x604;
        obj->field00->field18 = 0x604;

        *mk3_frame(thread, frame + 1) = 0x208;
        thread->fieldfc = 0x25;
        return 0x25;
    }

    if (slot != 0x208)
        return -3;

    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
        return 0;
    }
    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}





/* --------------------------------------------------------------------
 * What the readers could prove. See tools/pushfn.py, which executes
 * a body symbolically, and tools/microfn.py, which matches whole
 * bodies against templates. Both refuse anything they cannot account
 * for instruction by instruction.
 * -------------------------------------------------------------------- */

long t_backwards_ani(struct MK3THREAD *thread);
long t_rbomb4(struct MK3THREAD *thread);
long t_rocket1_proc(struct MK3THREAD *thread);
long t_rocket2_proc(struct MK3THREAD *thread);
long t_rzap3(struct MK3THREAD *thread);
long tl_bomb33(struct MK3THREAD *thread);
void get_bomb_vel(MK3OBJ *obj);
void get_char_ani2(MK3OBJ *obj);
void ochar_sound(MK3OBJ *obj);
void q_his_react_flag_set(MK3OBJ *obj);
void zap_init_special_act(MK3OBJ *obj);

/* t_robo_bomb_full -- armv7 0x00075424, 64 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      get_bomb_vel(obj)
 *      frame[frame].handler = t_rbomb4
 *      frame[frame+1].w0 = 0
 */

long t_robo_bomb_full(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    get_bomb_vel(obj);

    return mk3_push_handler(thread, (MK3THREADFUNC)t_rbomb4);
}

/* t_robo_open_chest_fast -- armv7 0x00075844, 108 bytes.  **Complete.**
 *
 * **Was missing a whole branch.** The disassembly installs one of TWO
 * handlers depending on `obj->field5c`, checked AFTER `q_his_react_flag_set`
 * runs -- and it is `mk3_install` (replaces the current level) both times,
 * not `mk3_push_handler` (which would push a new one). The version here
 * called `mk3_push_handler(t_robo_open_chest)` unconditionally, which is
 * only the `field5c == 0` half and the wrong installer besides.
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x2
 *      q_his_react_flag_set(obj)
 *      if (obj->field5c != 0)
 *          frame[frame].handler = t_roc3
 *      else
 *          frame[frame].handler = t_robo_open_chest
 *      frame[frame+1].w0 = 0
 */

long t_robo_open_chest_fast(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x2;
    q_his_react_flag_set(obj);

    if (obj->field5c != 0)
        return mk3_install(thread, (MK3THREADFUNC)t_roc3);

    return mk3_install(thread, (MK3THREADFUNC)t_robo_open_chest);
}

/* t_robo_close_chest -- armv7 0x00077eb4, 76 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field40 = 0   (the register the guard proved)
 *      get_char_ani2(obj)
 *      obj->field1c = 0x4
 *      frame[frame].handler = t_backwards_ani
 *      frame[frame+1].w0 = 0
 */

long t_robo_close_chest(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field40 = 0;   /* the guard proved this register */
    get_char_ani2(obj);
    obj->field1c = 0x4;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_backwards_ani);
}

/* tl_do_robo_zap -- armv7 0x00079bbc, 84 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field20 = 0x9
 *      obj->a10 = 0   (the register the guard proved)
 *      zap_init_special_act(obj)
 *      obj->field38 = t_rocket1_proc
 *      frame[frame].handler = t_rzap3
 *      frame[frame+1].w0 = 0
 */

long tl_do_robo_zap(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field20 = 0x9;
    obj->a10 = 0;   /* the guard proved this register */
    zap_init_special_act(obj);
    obj->field38 = (uint32_t)(uintptr_t)t_rocket1_proc;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_rzap3);
}

/* tl_do_robo_zap2 -- armv7 0x00079c10, 92 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field20 = 0x8
 *      obj->a10 = 0   (the register the guard proved)
 *      zap_init_special_act(obj)
 *      obj->field1c = 0xc
 *      ochar_sound(obj)
 *      obj->field38 = t_rocket2_proc
 *      frame[frame].handler = t_rzap3
 *      frame[frame+1].w0 = 0
 */

long tl_do_robo_zap2(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field20 = 0x8;
    obj->a10 = 0;   /* the guard proved this register */
    zap_init_special_act(obj);
    obj->field1c = 0xc;
    ochar_sound(obj);
    obj->field38 = (uint32_t)(uintptr_t)t_rocket2_proc;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_rzap3);
}

/* tl_do_swat_bomb_hi -- armv7 0x0007a704, 76 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field20 = 0x18
 *      obj->a10 = 0   (the register the guard proved)
 *      zap_init_special_act(obj)
 *      obj->field48 = 0x1
 *      frame[frame].handler = tl_bomb33
 *      frame[frame+1].w0 = 0
 */

long tl_do_swat_bomb_hi(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field20 = 0x18;
    obj->a10 = 0;   /* the guard proved this register */
    zap_init_special_act(obj);
    obj->field48 = 0x1;

    return mk3_push_handler(thread, (MK3THREADFUNC)tl_bomb33);
}

/* tl_do_swat_bomb_lo -- armv7 0x0007a750, 76 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field20 = 0x19
 *      obj->a10 = 0   (the register the guard proved)
 *      zap_init_special_act(obj)
 *      obj->field48 = 0   (the register the guard proved)
 *      frame[frame].handler = tl_bomb33
 *      frame[frame+1].w0 = 0
 */

long tl_do_swat_bomb_lo(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field20 = 0x19;
    obj->a10 = 0;   /* the guard proved this register */
    zap_init_special_act(obj);
    obj->field48 = 0;   /* the guard proved this register */

    return mk3_push_handler(thread, (MK3THREADFUNC)tl_bomb33);
}





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

void air_init_special(MK3OBJ *obj);
void init_special(MK3OBJ *obj);
void init_special_act(MK3OBJ *obj);
void leftmost_mpart_ob(MK3OBJ *out, MK3OBJ *src);
void zinit3(MK3OBJ *obj);

/* a3_leftmost_mpart_ob -- armv7 0x00075e98, 16 bytes.  **Complete.**
 *
 *      leftmost_mpart_ob(out, src)
 *      out->field28 = out->field24
 *
 * **It takes two arguments and forwards both.** Nothing in the body sets r1:
 * it arrives as this function's second argument and goes straight on to
 * `leftmost_mpart_ob`, which takes two.
 *
 * `tools/leaffn.py` wrote this with one, because it seeds only r0 with the
 * object and reads an untouched r1 as dead rather than as an argument passing
 * through. From a call site alone the two are indistinguishable; what settled
 * it was `tools/protos.py` putting this against the definition in other.c.
 */
void a3_leftmost_mpart_ob(MK3OBJ *out, MK3OBJ *src)
{
    leftmost_mpart_ob(out, src);
    out->field28 = out->field24;
}


/* zap_init_special -- armv7 0x000792fc, 20 bytes.  **Complete.**
 *
 *      init_special(obj)
 *      zinit3(obj)
 */
void zap_init_special(MK3OBJ *obj)
{
    init_special(obj);
    zinit3(obj);
}


/* zap_init_special_act -- armv7 0x00079590, 20 bytes.  **Complete.**
 *
 *      init_special_act(obj)
 *      zinit3(obj)
 */
void zap_init_special_act(MK3OBJ *obj)
{
    init_special_act(obj);
    zinit3(obj);
}


/* zap_air_init_special -- armv7 0x0007b2a4, 20 bytes.  **Complete.**
 *
 *      air_init_special(obj)
 *      zinit3(obj)
 */
void zap_air_init_special(MK3OBJ *obj)
{
    air_init_special(obj);
    zinit3(obj);
}

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

long t_sz_zap_hit(MK3THREAD *thread);
long tl_delete_proj_and_die(MK3THREAD *thread);
void create_fx(MK3OBJ *obj);
void get_char_ani(MK3OBJ *obj);
void init_anirate(MK3OBJ *obj);
void set_proj_vel(MK3OBJ *obj);

/* t_sz_zap_proc -- armv7 0x00075e08, 144 bytes.  **Complete.**
 *
 *      token == 0:
 *          obj->field1c = 0x4
 *          init_anirate(obj)
 *          obj->field1c = 0xa0000
 *          set_proj_vel(obj)
 *          obj->field48 = 0x13
 *          token := 0x810, then descend into tl_projectile_flight
 *      token == 0x810:
 *          frame[frame].handler = t_sz_zap_hit
 *      otherwise:  return -3
 */
long t_sz_zap_proc(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field1c = 0x4;
        init_anirate(obj);
        obj->field1c = 0xa0000;
        set_proj_vel(obj);
        obj->field48 = 0x13;
        *mk3_frame(thread, thread->frame + 1) = 0x810;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)tl_projectile_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x810)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_sz_zap_hit);
}

/* t_photon_proc -- armv7 0x000769b0, 164 bytes.  **Complete.**
 *
 *      token == 0:
 *          obj->field40 = 0x3f
 *          get_char_ani(obj)
 *          obj->field1c = 0xa0000
 *          obj->field20 = 0x3
 *          set_proj_vel(obj)
 *          obj->field48 = 0x11
 *          token := 0xbaa, then descend into tl_projectile_flight
 *      token == 0xbaa:
 *          obj->field1c = 0x5
 *          create_fx(obj)
 *          frame[frame].handler = tl_delete_proj_and_die
 *      otherwise:  return -3
 */
long t_photon_proc(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field40 = 0x3f;
        get_char_ani(obj);
        obj->field1c = 0xa0000;
        obj->field20 = 0x3;
        set_proj_vel(obj);
        obj->field48 = 0x11;
        *mk3_frame(thread, thread->frame + 1) = 0xbaa;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)tl_projectile_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0xbaa)
        return -3;

    obj->field1c = 0x5;
    create_fx(obj);
    return mk3_install(thread, (MK3THREADFUNC)tl_delete_proj_and_die);
}


/* ========================================================================
 * Six leaves, read one at a time because neither `pushfn.py` nor
 * `microfn.py` matches any of their shapes.
 * ======================================================================== */

/* detach_proj -- armv7 0x0007568c, 12 bytes.  **Complete.**
 *
 *      proc->slave   = 0
 *      proc->field64 = 0
 *
 * **The other half of `delete_slave`.** That routine, in mkfatal.c, hands
 * `proc->field64` to `KillProc`; this one clears both slave slots and kills
 * nothing. So a slave can be let go as well as destroyed, and the two words at
 * 0x64 and 0x68 are written and cleared together.
 *
 * It still does not say who CREATES the slave -- that is issue #30, and seven
 * `delete_slave` sites plus this one have now failed to answer it.
 *
 * The proc pointer is loaded twice, once for each store, rather than kept in a
 * register. Transcribed as two stores through the same field because that is
 * what it computes.
 */
void detach_proj(MK3OBJ *obj)
{
    obj->field00->slave   = 0;
    obj->field00->field64 = 0;
}


/* i_am_a_sitting_duck -- armv7 0x000758b0, 12 bytes.  **Complete.**
 *
 *      obj->field1c     = 0x604
 *      proc->field18    = 0x604
 *
 * One literal into two fields, and 0x18 on the proc is the action
 * `get_his_action` reads and `init_special_act` writes. So the routine
 * announces an action to whoever asks and leaves the same number in 0x1c for
 * whoever called it -- the pattern `mk_random` and the other 0x1c-returning
 * helpers use.
 */
void i_am_a_sitting_duck(MK3OBJ *obj)
{
    obj->field1c          = 0x604;
    obj->field00->field18 = 0x604;
}


/* zinit3 -- armv7 0x00075f88, 16 bytes.  **Complete.**
 *
 *      if (obj->a10 != 0) flip_multi(obj)
 *
 * A conditional tail call and nothing else. `a10` is the argument slot, so the
 * caller decides by writing 0x44 before the call rather than by picking a
 * different function -- which is how most of this engine passes a boolean.
 */
void flip_multi(MK3OBJ *obj);

void zinit3(MK3OBJ *obj)
{
    if (obj->a10 != 0)
        flip_multi(obj);
}


/* is_he_motaro -- armv7 0x00075818, 20 bytes.  **Complete.**
 *
 *      obj->field5c = (him->field24 == 0x18)
 *
 * **Motaro is character 0x18, and this is the sixth place in the tree that
 * names a fighter by number** -- but the first that does so in a routine named
 * for the question. The other five (issue #29) are anonymous exceptions buried
 * inside larger routines: 0xb in `tl_kano_spider`, `t_ripped_skelton` twice and
 * `t_remaining_skel`, and {7, 8, 0xe} in `t_kitana_kiss`.
 *
 * A named predicate is a different thing from a hidden special case, and a port
 * that renumbers the roster fixes this one by editing a single constant in a
 * function whose name says what it is for. It is listed here so the count stays
 * honest, not because it is the same hazard.
 *
 * The answer goes in 0x5c, which is where every predicate in this engine puts
 * its result.
 */
void is_he_motaro(MK3OBJ *obj)
{
    MK3OBJ *him = (MK3OBJ *)(void *)(uintptr_t)obj->field00->him;

    obj->field5c = (him->field24 == 0x18) ? 1 : 0;
}


/* q_his_react_flag_set -- armv7 0x0007582c, 24 bytes.  **Complete.**
 *
 *      obj->field2c = proc->field00->proc->field10
 *      obj->field5c = (obj->field54 >> 2) & 1
 *
 * **The value it fetches is not the value it answers with**, and that is worth
 * saying plainly rather than smoothing over. The four loads walk out to the
 * OTHER fighter's proc and bring back its 0x10 -- the word the header records
 * `isp2` as OR-ing bit 4 into -- and park it in 0x2c. The answer then comes from
 * `obj->field54`, a field this routine never writes.
 *
 * Two readings are possible and neither is settled here: the fetch is a side
 * effect the caller wants (0x2c as a place to leave the other guy's flags), or
 * 0x54 was filled by whatever ran before and the fetch is stale bookkeeping. The
 * instructions are transcribed as they stand.
 *
 * Bit 2 of 0x54 is isolated with `lsrs #2` then `and #1`, the same two-instruction
 * shape `am_i_joy` uses on 0x5c.
 *
 * **It was declared `long` earlier in this file and it is not.** That declaration
 * was written from the call site in `t_robo_bomb_full`, which discards the
 * result; the body never touches `r0`, so what a `long` caller would read back is
 * the object pointer it passed in, by accident. The answer is in 0x5c like every
 * other predicate here. The declaration has been removed rather than the
 * definition bent to match it -- the same correction `rip_ani` needed in
 * mkfatal.c, and the reason `tools/protos.py` exists.
 */
void q_his_react_flag_set(MK3OBJ *obj)
{
    obj->field2c = obj->field00->field00->field00->field10;
    obj->field5c = (obj->field54 >> 2) & 1u;
}


/* tell_world_stk -- armv7 0x00075900, 24 bytes.  **Complete.**
 *
 *      saved = obj->field1c
 *      get_char_stk(obj)
 *      proc->field84 = obj->field1c
 *      obj->field1c  = saved
 *
 * **A publish-and-restore.** `get_char_stk` answers in 0x1c like every other
 * helper here, the answer is copied into the proc's 0x84 where anything can
 * read it, and 0x1c is put back exactly as the caller left it -- so the routine
 * is invisible to whoever called it apart from the field it publishes.
 *
 * The save is a callee-saved register, not the argument stack, and the span is
 * inside one call: the rule `t_sg_pound` settled in mkfatal.c, seen again here
 * in twenty-four bytes.
 *
 * `proc->field84` had no field in the header before this; it has one now, and
 * **no reader for it has been measured anywhere in the tree**.
 */
void get_char_stk(MK3OBJ *obj);

void tell_world_stk(MK3OBJ *obj)
{
    uint32_t saved = obj->field1c;

    get_char_stk(obj);
    obj->field00->field84 = obj->field1c;

    obj->field1c = saved;
}


/* q_is_he_react_fk -- armv7 0x00074c9c, 32 bytes.  **Complete.**
 *
 *      obj->field20 = proc->field00->proc->field18
 *      obj->field5c = (obj->field20 == 0x507)
 *
 * **The twin of `q_his_react_flag_set`, and the contrast is what makes that one
 * suspicious.** Both walk the same four loads out to the OTHER fighter's proc.
 * This one parks what it found and answers about it; that one parks what it
 * found in 0x2c and then answers from `obj->field54`, which it never wrote.
 *
 * Written next to each other they are plainly meant to be the same shape, so
 * either 0x54 is filled by the caller in that case or the routine is not doing
 * what its name says. Recorded on both, decided on neither.
 *
 * 0x507 is an action number -- proc 0x18 is the field `get_his_action` reads.
 */
void q_is_he_react_fk(MK3OBJ *obj)
{
    obj->field20 = obj->field00->field00->field00->field18;
    obj->field5c = (obj->field20 == 0x507) ? 1 : 0;
}


/* get_frontmost_point -- armv7 0x00075ea8, 36 bytes.  **Complete.**
 *
 *      obj->field2c = part->field28
 *      if (part->field28 & 0x10) {
 *          leftmost_mpart_ob(obj, part)
 *          obj->field28 = obj->field24
 *      } else {
 *          rightmost_mpart_ob(obj, part)
 *      }
 *
 * **The answer always ends up in 0x28, and the copy is only on one branch
 * because the two helpers write different fields.** mkprop.c already recorded
 * that `leftmost_mpart_ob` answers in 0x24 and `rightmost_mpart_ob` in 0x28;
 * this routine normalises them, and the asymmetry in the code is exactly that
 * asymmetry in the helpers. A transcription that copied 0x24 to 0x28 on both
 * paths would clobber the right-hand answer with a stale left-hand one.
 *
 * "Frontmost" is decided by bit 4 of the part's 0x28 -- the flip bit -- so the
 * front of a fighter is their left edge when they face left and their right when
 * they face right, which is what the name means.
 */
void leftmost_mpart_ob(MK3OBJ *out, MK3OBJ *src);
void rightmost_mpart_ob(MK3OBJ *out, MK3OBJ *src);

void get_frontmost_point(MK3OBJ *obj)
{
    obj->field2c = obj->field08->field28;

    if ((obj->field2c & 0x10u) != 0) {
        leftmost_mpart_ob(obj, obj->field08);
        obj->field28 = obj->field24;            /* left answers in 0x24 */
    } else {
        rightmost_mpart_ob(obj, obj->field08);  /* right already answers in 0x28 */
    }
}


/* get_proj_obj_m -- armv7 0x0007822c, 36 bytes.  **Complete.**
 *
 *      p = NewThreadProc(obj, t_wait_forever)
 *      proc->field64 = p
 *      proc->slave   = p->field08
 *      obj->a10      = p->field08
 *
 * **This is what creates the slave, and it closes issue #30.** Eight sites
 * across mkfatal.c cleared or destroyed `proc->field64` and `proc->slave` --
 * seven `delete_slave` calls and `detach_proj` above -- and none of them made
 * one. This does: it starts a thread that does nothing at all, parks the new
 * object in 0x64 and its part in 0x68, and hands the part back in `a10`.
 *
 * So a "slave" is a second object with no behaviour of its own, driven entirely
 * by whoever owns it. `t_wait_forever` as the handler is the whole point --
 * everything the slave does is done to it.
 *
 * The three routines make a complete set: **create** here, **let go**
 * (`detach_proj`), **destroy** (`delete_slave`).
 */
long t_wait_forever(MK3THREAD *thread);
void *NewThreadProc(void *owner, MK3THREADFUNC func);

void get_proj_obj_m(MK3OBJ *obj)
{
    MK3OBJ *p = (MK3OBJ *)NewThreadProc(obj, (MK3THREADFUNC)t_wait_forever);

    obj->field00->field64 = (uint32_t)(uintptr_t)p;
    obj->field00->slave   = (uint32_t)(uintptr_t)p->field08;

    obj->a10 = (uint32_t)(uintptr_t)p->field08;
}


/* set_proj_vel -- armv7 0x00075d6c, 36 bytes.  **Complete.**
 *
 *      obj->field2c = part->field28
 *      if (part->field28 & 0x10) obj->field1c = -obj->field1c
 *      part->field18 = obj->field1c
 *      obj->field1c  = obj->field20
 *      init_anirate(obj)
 *
 * **Two arguments in two fields**: the speed in 0x1c and the animation rate in
 * 0x20. The speed is signed by the flip bit so a caller always passes a forward
 * speed and never has to know which way the fighter is facing -- which is why
 * every projectile launcher in this file writes a positive number.
 *
 * The negation is spelled as an `itett` block with the load duplicated in both
 * halves, so the flipped path also writes the negated value back into 0x1c
 * where the unflipped path leaves it alone. Both halves then share the store
 * into the part.
 *
 * 0x1c is spent and reloaded with 0x20 before `init_anirate`, which reads it --
 * the same one-field-two-callees squeeze the argument stack exists for, solved
 * here by ordering instead, because nothing needs the speed afterwards.
 */
void set_proj_vel(MK3OBJ *obj)
{
    obj->field2c = obj->field08->field28;

    if ((obj->field2c & 0x10u) != 0)
        obj->field1c = (uint32_t)(-(long)obj->field1c);

    obj->field08->field18 = obj->field1c;

    obj->field1c = obj->field20;
    init_anirate(obj);
}


/* setup_proj_obj -- armv7 0x00078250, 36 bytes.  **Complete.**
 *
 *      saved = obj->field40
 *      obj->field40 = 0x3f; get_char_ani(obj)
 *      obj->field48 = obj->field40
 *      get_proj_obj_m(obj)
 *      obj->field40 = saved
 *      obj->field1c = obj->a10
 *
 * Animation 0x3f resolved into 0x48, a slave made, the caller's 0x40 put back,
 * and the new part left in 0x1c for whoever called. **Every projectile in this
 * file starts from the same animation number**, which is what makes 0x3f worth
 * naming when someone gets to the animation tables.
 *
 * The save is a callee-saved register across two calls inside one function --
 * the rule `t_sg_pound` settled, and the third routine in this batch to use it.
 */
void get_char_ani(MK3OBJ *obj);

void setup_proj_obj(MK3OBJ *obj)
{
    uint32_t saved = obj->field40;

    obj->field40 = 0x3f;
    get_char_ani(obj);

    obj->field48 = obj->field40;

    get_proj_obj_m(obj);

    obj->field40 = saved;
    obj->field1c = obj->a10;
}


/* tl_delete_proj_and_die -- armv7 0x00075664, 40 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame+1].w0 = 0x13c6
 *      return 0x16462 (delete)                     and never wakes
 *
 * **Eleventh 0x16462 site**, and the token 0x13c6 is not in any dispatch -- the
 * routine has no second state. The name says the rest: whatever called it has
 * already deleted the projectile, and this thread is finished.
 *
 * It is reached by `mk3_install` from the projectile drivers at the end of this
 * file, so the thread that flew the projectile is the one that parks.
 */
long tl_delete_proj_and_die(MK3THREAD *thread)
{
    uint32_t frame = thread->frame;

    if (*mk3_frame(thread, frame + 1) != 0)
        return -3;

    *mk3_frame(thread, frame + 1) = 0x13c6;
    thread->fieldfc = 0x16462;              /* and never wakes */
    return 0x16462;
}


/* proj_onscreen_test -- armv7 0x00075714, 52 bytes.  **Complete.**
 * proj_onscreen_test_unsafe -- armv7 0x00075748, 48 bytes.  **Complete.**
 *
 *      x = (int16_t)part->x0e
 *      obj->field5c = (x > left - m && x < right + m)
 *      return obj->field5c
 *
 * where `m` is 0x64 for `proj_onscreen_test` and **0** for the one called
 * unsafe. `left` is `G + 0x468` and `right` is `G + 0x470`.
 *
 * **The names are the wrong way round from what they sound like.** The "unsafe"
 * one is the STRICTER test -- it fails the moment the projectile touches an
 * edge, where the other allows a hundred units of slack outside the view before
 * saying no. So "unsafe" means "will call it offscreen too eagerly", and a
 * caller that deletes a projectile on a false answer would cut it off while it
 * is still visible.
 *
 * Both **return the answer as well as leaving it in 0x5c** -- `ldr r0, [r0,
 * #0x5c]` on the way out, which almost no predicate in this engine bothers to
 * do. Declared `long` on that authority rather than by guessing.
 *
 * This is also the third and fourth routine to read `G + 0x468` and `G + 0x470`
 * as the left and right edges of the view, after `t_another_scorpion` and
 * `t_scorpion_hell` in mkfatal.c. Four sites, one reading, and the pair can now
 * be named without hedging.
 */
long proj_onscreen_test(MK3OBJ *obj)
{
    int32_t x = (int32_t)(int16_t)MK3_FIELD0E(obj->field08);

    obj->field5c =
        (x > (long)(*(uint32_t *)(G_BYTES + 0x468) - 0x64)
         && x < (long)(*(uint32_t *)(G_BYTES + 0x470) + 0x64)) ? 1 : 0;

    return (long)obj->field5c;
}

long proj_onscreen_test_unsafe(MK3OBJ *obj)
{
    int32_t x = (int32_t)(int16_t)MK3_FIELD0E(obj->field08);

    obj->field5c =
        (x > (long)*(uint32_t *)(G_BYTES + 0x468)
         && x < (long)*(uint32_t *)(G_BYTES + 0x470)) ? 1 : 0;

    return (long)obj->field5c;
}


/* proj_strike_check -- armv7 0x00075f5c, 44 bytes.  **Complete.**
 *
 *      if (him->field24 == 0x18) { obj->field5c = 0; return }
 *      saved = obj->field1c
 *      is_jade_protected(obj)
 *      obj->field1c = saved
 *      if (obj->field5c != 0) { obj->field5c = 0; return }
 *      strike_check_a0(obj)
 *
 * **Two fighters cannot be hit by a projectile: Motaro, and Jade while she is
 * protected.** The check is the same shape twice -- ask, then zero the answer
 * and leave -- and only if both say no does the real strike check run.
 *
 * **The Motaro test is inlined here as `cmp r3, #0x18`**, where
 * `local_strike_check_box` below calls `is_he_motaro` for exactly the same
 * question. So character 0x18 is named by number in two places in this file:
 * once in a routine that says so and once buried in a comparison. **That second
 * one belongs with the five in issue #29**, not with the named predicate.
 *
 * `is_jade_protected` clobbers 0x1c, so it is saved in a register across the
 * call -- inside one function, the rule `t_sg_pound` settled.
 */
void is_jade_protected(MK3OBJ *obj);
long strike_check_a0(MK3OBJ *obj);

void proj_strike_check(MK3OBJ *obj)
{
    MK3OBJ  *him = (MK3OBJ *)(void *)(uintptr_t)obj->field00->him;
    uint32_t saved;

    if (him->field24 == 0x18) {                  /* Motaro, inlined */
        obj->field5c = 0;
        return;
    }

    saved = obj->field1c;
    is_jade_protected(obj);
    obj->field1c = saved;

    if (obj->field5c != 0) {
        obj->field5c = 0;
        return;
    }

    strike_check_a0(obj);
}


/* local_strike_check_box -- armv7 0x00075f1c, 64 bytes.  **Complete.**
 *
 *      is_he_motaro(obj)
 *      if (obj->field5c != 0) { obj->field5c = 0; return }
 *      save 0x1c, 0x20 and 0x24
 *      is_jade_protected(obj)
 *      restore 0x1c, 0x20 and 0x24
 *      if (obj->field5c != 0) { obj->field5c = 0; return }
 *      strike_check_box(obj)
 *
 * **The box version of `proj_strike_check`, and the two differ in exactly two
 * places.** This one asks the question through `is_he_motaro` instead of
 * inlining the constant, and it saves **three** fields across
 * `is_jade_protected` where the other saves one -- which is why it pushes `r8`
 * and the other does not.
 *
 * Three fields is what a box needs: 0x1c, 0x20 and 0x24 are three of the four
 * coordinate slots `strike_check_box` reads, and the predicate in the middle
 * would otherwise trample them.
 *
 * Read side by side the pair says what `is_jade_protected` costs: it clobbers at
 * least 0x1c, 0x20 and 0x24, and every caller has to know that.
 */
void strike_check_box(MK3OBJ *obj);

void local_strike_check_box(MK3OBJ *obj)
{
    uint32_t s1c, s20, s24;

    is_he_motaro(obj);
    if (obj->field5c != 0) {
        obj->field5c = 0;
        return;
    }

    s1c = obj->field1c;
    s20 = obj->field20;
    s24 = obj->field24;

    is_jade_protected(obj);

    obj->field1c = s1c;
    obj->field20 = s20;
    obj->field24 = s24;

    if (obj->field5c != 0) {
        obj->field5c = 0;
        return;
    }

    strike_check_box(obj);
}


/* get_bomb_vel -- armv7 0x0007534c, 56 bytes.  **Complete.**
 *
 *      obj->field1c = (int16_t)him->x0e
 *      obj->field20 = (int16_t)part->x0e - obj->field1c
 *      if (obj->field20 < 0) obj->field20 = -obj->field20
 *      obj->field48 = 0x20
 *      obj->field20 = (obj->field20 << 16) / 0x20
 *      obj->field1c = obj->field20
 *
 * **A thirty-two step interpolation, the same idea `t_kiss_orb` uses with
 * sixteen.** The gap between the two fighters is taken in pixels, shifted into
 * 16.16 and divided by 0x20, so the bomb covers it in exactly thirty-two
 * frames whatever the distance. The divisor is published in 0x48 so the caller
 * can count the same thirty-two.
 *
 * The division is the round-toward-zero idiom again --
 * `add.w r2, r3, #0x1f` / `bics.w r3, r3, r3, asr #32` / `it hs` / `movhs` /
 * `asrs #5`, where the carry out of `asr #32` is the sign bit. Exactly C's
 * signed `/ 0x20`, and written as that.
 *
 * The distance is made absolute with the `itt lt` / `rsblt` pair, so the speed
 * comes out positive and `set_proj_vel` signs it by the flip bit afterwards.
 * The two routines are built to be used together and neither duplicates the
 * other's work.
 *
 * **It was declared `long` earlier in this file and it is not** -- the body
 * never touches `r0`, so a `long` caller reads back the object pointer it passed
 * in. Second such declaration corrected in this file, after
 * `q_his_react_flag_set`; both were written from call sites that discard the
 * result, which is the failure mode `tools/protos.py` exists to catch.
 */
void get_bomb_vel(MK3OBJ *obj)
{
    int32_t dx;

    obj->field1c = (uint32_t)(int32_t)(int16_t)
                   MK3_FIELD0E((MK3OBJ *)(void *)(uintptr_t)obj->field00->him);

    obj->field20 = (uint32_t)((int32_t)(int16_t)MK3_FIELD0E(obj->field08)
                              - (int32_t)obj->field1c);
    if ((long)obj->field20 < 0)
        obj->field20 = (uint32_t)(-(long)obj->field20);

    obj->field48 = 0x20;

    dx = (int32_t)(obj->field20 << 16);
    obj->field20 = (uint32_t)(dx / 0x20);        /* thirty-two frames */
    obj->field1c = obj->field20;
}


/* ============================================================ t_do_zap
 *
 * armv7 0x000758bc, 68 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = projectile_jumps[obj->field1c]
 *      frame[frame].handler = obj->field1c
 *      frame[frame+1].w0 = 0
 *
 * **This is the projectile dispatcher for the whole game**, and it is sixty-eight
 * bytes: take the kind out of 0x1c, index a table of thread handlers, install
 * what comes back. Every zap, spear, bomb, orb and net in UMK3 starts here.
 *
 * `projectile_jumps` is at **0x00172694, forty-five words**, each a thread entry
 * point with the Thumb bit set. The whole table, because a port needs the
 * numbering and nothing else in the tree gives it:
 *
 *       0 tl_do_kano_zap          15 tl_do_robo_bomb        30 tl_do_smoke_spear
 *       1 tl_do_sonya_zap         16 tl_do_bomb_mid         31 tl_do_motaro_zap
 *       2 tl_do_jax_zap1          17 tl_do_tusk_zap         32 tl_do_kitana_zap
 *       3 tl_do_jax_zap2          18 tl_do_summon           33 tl_do_jade_zap_med
 *       4 tl_do_ind_zap           19 tl_do_st_zap1          34 tl_do_reptile_orb
 *       5 tl_do_sky_ice_on        20 tl_do_st_zap2          35 tl_do_spit
 *       6 tl_do_sky_ice_behind    21 tl_do_st_zap3          36 tl_do_scorpion_spear
 *       7 tl_do_sky_ice_front     22 tl_lk_zap_hi           37 tl_do_jade_zap_hi
 *       8 tl_do_sw_zap            23 tl_lk_zap_lo           38 tl_do_jade_zap_lo
 *       9 tl_do_robo_zap          24 tl_do_sg_zap           39 tl_do_jade_zap_ret
 *      10 tl_do_robo_zap2         25 tl_do_swat_bomb_hi     40 tl_do_reptile_orb_fast
 *      11 tl_do_robo_net          26 tl_do_swat_bomb_lo     41 tl_do_mileena_zap
 *      12 tl_do_sz_zap            27 tl_do_lia_forward_zap  42 tl_do_osz_zap
 *      13 tl_do_lia_anglez        28 tl_do_tusk_floor       43 tl_do_floor_ice
 *      14 tl_do_lao_zap           29 tl_do_sk_zap           44 tl_do_ermac_zap
 *
 * The word after entry 44 is 0x6168636f -- ASCII "ocha" -- so forty-five is the
 * whole table and not a guess about where it stops.
 *
 * **The index is unchecked.** Nothing here compares 0x1c against 45, so a bad
 * kind reads whatever follows the table and installs it as a handler. Whatever
 * fills 0x1c is responsible, and a port that keeps this shape inherits that.
 *
 * The looked-up value is written back into 0x1c on the way past -- one register,
 * two destinations, and 0x1c ends up holding a function pointer. That is a
 * reading of 0x1c the tree has seen before, in `t_crusher_orb` and the
 * `call_a0_for_him` sites.
 */
extern uint32_t projectile_jumps[];              /* 0x00172694, 45 entries */

long t_do_zap(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;

    if (*mk3_frame(thread, frame + 1) != 0)
        return -3;

    obj->field1c = projectile_jumps[obj->field1c];   /* unchecked */

    mk3_frame(thread, frame)[1] = obj->field1c;
    *mk3_frame(thread, frame + 1) = 0;
    return 0;
}


/* ================================== t_boomerang_trail and t_rr_nothing
 *
 * armv7 0x00074f64 and 0x00075538, 68 bytes each.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      if (thread->frame > 0) { thread->frame -= 1; return 0 }
 *      frame[frame].handler = t_local_reaction_exit
 *      frame[frame+1].w0 = 0
 *
 * **Two names for one function.** The bodies are instruction-for-instruction
 * identical -- same registers, same order, same pointer slot at 0x000f3708 --
 * and they are 1,492 bytes apart. Not a tail call, not a jump: the source had
 * two routines that both do nothing but give the level back, and the compiler
 * emitted both.
 *
 * That is worth saying because it says something about the original source: a
 * do-nothing thread handler was written once per caller and named for what the
 * caller wanted, rather than shared. Expect more of these in this file.
 *
 * **`t_boom_return_check`, later in this file, says why.** It writes
 * `t_boomerang_trail` into `proc->field28` -- the per-frame callback slot a
 * projectile flight reads -- once the boomerang has turned around. So a
 * do-nothing handler is not dead code and not laziness: it is the **null
 * callback**, and it needs a name per system because the slot it goes into is
 * read by a different flight each time. `t_rr_nothing` is entry 0 of
 * `rocket_routines` for the same reason.
 *
 * The frame index is signed here -- `cmp #0` then `ble`, not `cbz` -- so a
 * negative index takes the install path, exactly as in `t_double_flame_ani`.
 */
long t_local_reaction_exit(MK3THREAD *thread); /* pointer slot 0x000f3708 */

long t_boomerang_trail(MK3THREAD *thread)
{
    uint32_t frame = thread->frame;

    if (*mk3_frame(thread, frame + 1) != 0)
        return -3;

    if ((long)frame > 0) {                  /* cmp #0 / ble: signed */
        thread->frame = frame - 1;          /* back up a level */
        return 0;
    }

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

long t_rr_nothing(MK3THREAD *thread)
{
    uint32_t frame = thread->frame;

    if (*mk3_frame(thread, frame + 1) != 0)
        return -3;

    if ((long)frame > 0) {                  /* cmp #0 / ble: signed */
        thread->frame = frame - 1;          /* back up a level */
        return 0;
    }

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* tl_do_jade_zap_hi -- armv7 0x000750a8, 64 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = -0xc000
 *      frame[frame].handler = tl_jzap3
 *      frame[frame+1].w0 = 0
 *
 * Entry 37 of `projectile_jumps`, and one of three -- `_hi`, `_lo` and `_med`
 * -- that differ only in the number they put in 0x1c before handing over to the
 * same routine. So the high, low and medium versions of Jade's zap are one
 * routine and three constants, which is the cheapest possible way to write three
 * moves.
 *
 * -0xc000 arrives as a literal pool word rather than a `mvn`, because it does
 * not fit the small-immediate encodings.
 */
long tl_do_jade_zap_hi(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;

    if (*mk3_frame(thread, frame + 1) != 0)
        return -3;

    obj->field1c = 0xffff4000u;             /* -0xc000 */

    mk3_frame(thread, frame)[1] = (uint32_t)(uintptr_t)tl_jzap3;
    *mk3_frame(thread, frame + 1) = 0;
    return 0;
}


/* make_dragon_explode -- armv7 0x00076bfc, 60 bytes.  **Complete.**
 *
 *      obj->field1c = 1; ochar_sound(obj)
 *      rightmost_mpart_ob(obj, part)          -> obj->field28
 *      leftmost_mpart_ob(obj, part)           -> obj->field24
 *      obj->field28 = obj->field28 - obj->field24
 *      if (obj->field28 < 0) obj->field28 = -obj->field28
 *      obj->field48 = obj->field28 + 0x100000
 *      make_lineup_explode(obj)
 *
 * **It measures the body and pays for the explosion by the inch.** Both edge
 * helpers are called for their answers, the difference is the width, and the
 * width goes into 0x48 with 0x10 added in the high half -- so 0x48 is a packed
 * pair again, `(0x10 << 16) | width`, and `make_lineup_explode` gets a count and
 * a span rather than a fixed number of pieces.
 *
 * `get_frontmost_point` earlier in this file calls the same two helpers to pick
 * ONE of the two answers; this calls both and subtracts. Two routines, two uses
 * of the same pair, and between them they settle that `leftmost_mpart_ob` writes
 * 0x24 and `rightmost_mpart_ob` writes 0x28 -- which is what mkprop.c recorded
 * and neither routine alone would prove.
 *
 * The absolute value is the `itt lt` / `rsblt` pair, so a mirrored body gives
 * the same width.
 */
void make_lineup_explode(MK3OBJ *obj);

void make_dragon_explode(MK3OBJ *obj)
{
    obj->field1c = 1;
    ochar_sound(obj);

    rightmost_mpart_ob(obj, obj->field08);       /* answers in 0x28 */
    leftmost_mpart_ob(obj, obj->field08);        /* answers in 0x24 */

    obj->field28 = obj->field28 - obj->field24;
    if ((long)obj->field28 < 0)
        obj->field28 = (uint32_t)(-(long)obj->field28);

    obj->field48 = obj->field28 + 0x100000;      /* (0x10 << 16) | width */

    make_lineup_explode(obj);
}


/* ===================================================== is_jade_protected
 *
 * armv7 0x00075ecc, 80 bytes.  **Complete.**
 *
 *      if (him->field24 != 0x10) { q_no(obj); return }       ; not Jade
 *      for (th = TList; th != NULL; th = th->next)
 *          if (th->pid == 0x11f && th->proc->a10 == proc->field00) {
 *              q_yes(obj);
 *              return;
 *          }
 *      q_no(obj)
 *
 * **It searches the live thread list for Jade's flash, and mkstat.c is what
 * created it.** That file's `t_jade_flash` does:
 *
 *      p = NewThreadProcPid(obj, t_jade_flash_proc, 0x11f);
 *      *(uint32_t *)((char *)p + 0x44) = (uint32_t)(uintptr_t)obj;
 *
 * -- a thread with pid 0x11f whose proc's `a10` points back at the fighter that
 * started it. This walks `TList` looking for exactly that, and answers yes when
 * the owner is the fighter being asked about. **Two files, one protocol: the pid
 * is the handshake and 0x44 is the payload.** The mkstat.c note called that 0x44
 * store "the argument slot" without knowing who reads it; this is the reader.
 *
 * So a port must keep the pid. 0x11f is not decoration -- it is how one system
 * finds another system's thread, and it is the only pid in the tree with a
 * measured reader.
 *
 * **Jade is character 0x10**, named by number here as Motaro is 0x18 in
 * `is_he_motaro`. Both are named predicates, so both are the mild form of the
 * hazard in issue #29 rather than the dangerous one.
 *
 * The list link is at offset 0 of a thread, which is where `frame[0]` sits --
 * so a thread on the list has its first frame word standing in as the next
 * pointer, or the two overlap by design. Written as a cast rather than as a
 * struct field because the header does not name it.
 *
 * **It answers only through 0x5c.** It was declared `long` in mkprop.c on the
 * theory that `q_yes` and `q_no` forward a value; they are eight bytes each that
 * write 0x5c and return, so they do not. That declaration has been corrected --
 * the third `long`-from-a-call-site fix in this batch of work, after
 * `q_his_react_flag_set` and `get_bomb_vel`.
 *
 * **There are three `q_yes`/`q_no` pairs in the binary**, at 0x000501ac
 * (moves.c), 0x00067524 (mkdrone.c) and 0x000a85cc (mkboss.c), and this routine
 * calls mkdrone.c's. Whether they are per-file duplicates or one pair the STABS
 * attributes three ways is not settled here; what matters for a port is that one
 * shared `q_yes` reproduces all three, because the body is `field5c = 1`.
 *
 * **CHECKED BY HAND, not by tools/factdiff.py: it reports one extra `q_no`
 * call, and there isn't one.** The two `q_no(obj); return;` above are both
 * real -- "not Jade" and "loop exhausted, no flash found" are genuinely
 * separate paths through this function -- but the compiler reached them
 * with a single shared `bl q_no` at 0x75ede, branched to from both places
 * (`beq` skips it on the Jade path, the loop's own exhaustion falls through
 * to it via `b 0x75ede`), disassembled and confirmed directly rather than
 * assumed. `facts_asm.py` counts `bl` INSTRUCTIONS, so one physical call
 * reached by two logical paths reads as the binary having fewer `q_no`
 * calls than a readable C that keeps the two returns separate -- the same
 * gap `t_background_death` (mkreact.c) already names for a `tbb` table.
 */
void q_yes(MK3OBJ *obj);
void q_no(MK3OBJ *obj);
extern MK3THREAD *P32 TList;                         /* 0x0038ed48 */

void is_jade_protected(MK3OBJ *obj)
{
    MK3THREAD *th;

    if (((MK3OBJ *)(void *)(uintptr_t)obj->field00->him)->field24 != 0x10)
        goto no;                                 /* not Jade */

    for (th = TList; th != NULL; th = *(MK3THREAD *P32 *)(void *)th) {
        if (th->pid != 0x11f)
            continue;

        if ((MK3OBJ *)(void *)(uintptr_t)((MK3OBJ *)th->proc)->a10
            == obj->field00->field00) {
            q_yes(obj);
            return;
        }
    }

    /* One `bl q_no` in the binary, reached from both the not-Jade test
     * and the end of the list. */
no:
    q_no(obj);
}


/* make_lineup_explode -- armv7 0x00076ac4, 72 bytes.  **Complete.**
 *
 *      w  = (int16_t)obj->field48                 ; the low half
 *      sx = part->x0e ; sy = part->x12            ; saved
 *      if (part->field28 & 0x10) w = -w
 *      part->x0e = sx + w                         ; DEAD -- see below
 *      part->x12 = part->x12 + ((int32_t)obj->field48 >> 16)
 *      obj->field1c = 0x10
 *      part->x0e = him->x0e
 *      create_fx(obj)
 *      part->x0e = sx ; part->x12 = sy            ; restored
 *
 * **The width it is given never reaches anything.** `make_dragon_explode` above
 * measures the body, packs `(0x10 << 16) | width` into 0x48 and calls this; the
 * low half is sign-flipped by the facing, added to the part's x, stored -- and
 * then **overwritten with the opponent's x eight instructions later, with no
 * call in between**. The effect is created at the opponent's x and the part's
 * shifted y, and the width is discarded.
 *
 * That is transcribed rather than tidied away, and it is the largest dead store
 * measured in the tree: a whole computation with a flip test in it, feeding a
 * field that is rewritten before anything reads it. Either the source meant
 * `strh` into a different object, or the lineup this routine is named for stopped
 * happening at some point and only the y offset survived.
 *
 * The high half of 0x48 does work: the effect is placed 0x10 above the part.
 *
 * **The part's x and y are borrowed and put back**, so `create_fx` -- which
 * reads the part's position -- can be aimed somewhere else for one call without
 * the caller noticing. Both saves are callee-saved registers across one call,
 * the rule `t_sg_pound` settled.
 */
void make_lineup_explode(MK3OBJ *obj)
{
    MK3OBJ  *part = obj->field08;
    uint16_t sx   = MK3_FIELD0E(part);
    uint16_t sy   = MK3_FIELD12(part);
    int32_t  w    = (int32_t)(int16_t)obj->field48;

    if ((part->field28 & 0x10u) != 0)
        w = -w;

    MK3_SET_FIELD0E(part, (uint32_t)((int32_t)sx + w));   /* dead: rewritten */

    MK3_SET_FIELD12(part,
                    (uint32_t)MK3_FIELD12(part)
                    + (uint32_t)((int32_t)obj->field48 >> 16));

    obj->field1c = 0x10;

    MK3_SET_FIELD0E(part,
                    MK3_FIELD0E((MK3OBJ *)(void *)(uintptr_t)obj->field00->him));

    create_fx(obj);

    MK3_SET_FIELD0E(part, sx);
    MK3_SET_FIELD12(part, sy);
}


/* -------------------------------------------------------------------- t_skull_proc
 *
 * armv7 0x00076b0c, 240 bytes.  **Complete.**
 *
 * The free poses animation 0x3f, calls `tell_world_stk`, and refuses to
 * strike a Motaro (character 0x18, inlined the same way
 * `proj_strike_check` does it) or an opponent already in action 0x402 --
 * both skip straight to the flight setup. Otherwise `local_strike_check_box`
 * runs against a box packed as raw words (`field20 = 0x150057`,
 * `field24 = 0x160052`, transcribed as loaded, not decoded further); a
 * hit tags `field48 = 0x230042` and explodes immediately, no flight at
 * all. A miss (or Motaro, or the action check) falls into the ordinary
 * `0x80000`/`4` throw and flies on `tl_projectile_flight` from `0xaa0`,
 * whose re-entry tags `field48 = 0x230092` and explodes.
 *
 *      slot = frame[frame+1].w0
 *      if (slot == 0) {
 *          obj->field40 = 0x3f ; get_char_ani(obj)
 *          obj->field1c = 0x11 ; tell_world_stk(obj)
 *          if (him->field24 != 0x18) {
 *              get_his_action(obj)
 *              if (obj->field20 != 0x402) {
 *                  obj->field1c = 0x11
 *                  obj->field20 = 0x150057 ; obj->field24 = 0x160052
 *                  local_strike_check_box(obj)
 *                  if (obj->field5c != 0) {
 *                      obj->field48 = 0x230042
 *                      goto explode
 *                  }
 *              }
 *          }
 *          obj->field1c = 0x80000 ; obj->field20 = 4 ; set_proj_vel(obj)
 *          obj->field48 = 0x11
 *          token 0xaa0 ; frame++ ; install tl_projectile_flight ; return 0
 *      }
 *      if (slot != 0xaa0) return -3
 *      obj->field48 = 0x230092
 *      explode:  make_lineup_explode(obj)
 *                obj->field1c = 0x20006 ; hob_ochar_sound(obj)
 *                install tl_delete_proj_and_die ; return 0
 */
void tell_world_stk(MK3OBJ *obj);
void get_his_action(MK3OBJ *obj);
void hob_ochar_sound(MK3OBJ *obj);
long tl_projectile_flight(struct MK3THREAD *thread);

long t_skull_proc(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t slot  = *mk3_frame(thread, frame + 1);

    if (slot == 0) {
        obj->field40 = 0x3f;
        get_char_ani(obj);

        obj->field1c = 0x11;
        tell_world_stk(obj);

        if (((MK3OBJ *)(uintptr_t)obj->field00->him)->field24 != 0x18) {
            get_his_action(obj);
            if (obj->field20 != 0x402) {
                obj->field1c = 0x11;
                obj->field20 = 0x150057;
                obj->field24 = 0x160052;
                local_strike_check_box(obj);

                if (obj->field5c != 0) {
                    obj->field48 = 0x230042;
                    goto explode;
                }
            }
        }

        obj->field1c = 0x80000;
        obj->field20 = 4;
        set_proj_vel(obj);

        obj->field48 = 0x11;

        *mk3_frame(thread, frame + 1) = 0xaa0;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)tl_projectile_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot != 0xaa0)
        return -3;

    obj->field48 = 0x230092;

explode:
    make_lineup_explode(obj);

    obj->field1c = 0x20006;
    hob_ochar_sound(obj);

    return mk3_install(thread, (MK3THREADFUNC)tl_delete_proj_and_die);
}


/* -------------------------------------------------------------------- tl_sonya_zap_proc
 *
 * armv7 0x000767b4, 304 bytes.  **Complete.**
 *
 * Sonya's zap flight/hit callback, resumed from two tokens. `0x12b7` is the
 * hit re-entry: `stop_a8` on the GrObj, `field1c = 0xd`/`field20 = 0` (a
 * bare stop, no strike-box packing here -- the strike already happened
 * upstream), then straight into the shared explode tail. `0x12ce` is the
 * explode-tail's own re-entry, which just installs `tl_delete_proj_and_die`
 * and is done. The free branch throws normally: strike test via
 * `proj_strike_check`, and on a real miss (`field5c == 0`) sets
 * `field40 = 0x3f`/`get_char_ani`, packs `0x80000`/`3`/`3` and
 * `set_proj_vel`, tags `field48 = 0x12`, and flies on
 * `tl_projectile_flight` from `0x12b7`. A hit (`field5c != 0`) skips the
 * flight setup entirely and falls straight into explode with
 * `field1c = ~0xf` (`-0x10`) instead of the miss path's throw.
 *
 * The explode tail (`multi_adjust_xy`, `create_fx` at `field1c = 0`,
 * `hob_ochar_sound` at `0x10002`, `find_ani_part2` at `field40 = 0x3f`,
 * `field1c = 4`) is reached both from the immediate hit and from
 * `tl_projectile_flight`'s own hit callback -- one physical call site,
 * merged with `goto explode`.
 *
 *      slot = frame[frame+1].w0
 *      if (slot == 0x12b7) {
 *          stop_a8(obj->field08)
 *          obj->field1c = 0xd ; obj->field20 = 0
 *          goto explode
 *      }
 *      if (slot == 0x12ce) return install(tl_delete_proj_and_die)
 *      if (slot != 0) return -3
 *
 *      obj->field1c = 0x13 ; proj_strike_check(obj)
 *      if (obj->field5c != 0) {
 *          obj->field20 = 0 ; obj->field1c = ~0xf
 *          goto explode
 *      }
 *
 *      obj->field40 = 0x3f ; get_char_ani(obj)
 *      obj->field1c = 0x80000 ; obj->field24 = 3 ; obj->field20 = 3
 *      set_proj_vel(obj)
 *      obj->field48 = 0x12
 *      token 0x12b7 ; frame++ ; install tl_projectile_flight ; return 0
 *
 *      explode:  multi_adjust_xy(obj)
 *                obj->field1c = 0 ; create_fx(obj)
 *                obj->field1c = 0x10002 ; hob_ochar_sound(obj)
 *                obj->field40 = 0x3f ; find_ani_part2(obj)
 *                obj->field1c = 4
 *                token 0x12ce ; frame++ ; install t_mframew ; return 0
 */
void stop_a8(MK3OBJ *part);
void proj_strike_check(MK3OBJ *obj);
void multi_adjust_xy(MK3OBJ *obj);
void find_ani_part2(MK3OBJ *obj);
long t_mframew(struct MK3THREAD *thread);

long tl_sonya_zap_proc(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t slot  = *mk3_frame(thread, frame + 1);

    if (slot == 0x12b7) {
        stop_a8((MK3OBJ *)(uintptr_t)obj->field08);
        obj->field1c = 0xd;
        obj->field20 = 0;
        goto explode;
    }

    if (slot == 0x12ce)
        return mk3_install(thread, (MK3THREADFUNC)tl_delete_proj_and_die);

    if (slot != 0)
        return -3;

    obj->field1c = 0x13;
    proj_strike_check(obj);

    if (obj->field5c != 0) {
        obj->field20 = 0;
        obj->field1c = (uint32_t)~0xf;
        goto explode;
    }

    obj->field40 = 0x3f;
    get_char_ani(obj);

    obj->field1c = 0x80000;
    obj->field24 = 3;
    obj->field20 = 3;
    set_proj_vel(obj);

    obj->field48 = 0x12;

    *mk3_frame(thread, frame + 1) = 0x12b7;
    thread->frame = thread->frame + 1;   /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)tl_projectile_flight;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;

explode:
    multi_adjust_xy(obj);

    obj->field1c = 0;
    create_fx(obj);

    obj->field1c = 0x10002;
    hob_ochar_sound(obj);

    obj->field40 = 0x3f;
    find_ani_part2(obj);

    obj->field1c = 4;

    *mk3_frame(thread, frame + 1) = 0x12ce;
    thread->frame = thread->frame + 1;   /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_mframew;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* t_robo_bomb_mid -- armv7 0x000753dc, 72 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      get_bomb_vel(obj)
 *      obj->field1c = (int32_t)obj->field1c >> 1
 *      frame[frame].handler = t_rbomb4
 *      frame[frame+1].w0 = 0
 *
 * **The mid bomb is the full bomb thrown half as hard.** `t_robo_bomb_full`
 * earlier in this file calls the same `get_bomb_vel` and installs the same
 * `t_rbomb4` without the shift, so the two differ in one `asrs`.
 *
 * The shift is arithmetic, so a negative velocity halves the same way -- though
 * `get_bomb_vel` makes its answer positive, so the sign only matters if someone
 * calls this with 0x1c already set.
 */
long t_rbomb4(MK3THREAD *thread);

long t_robo_bomb_mid(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;

    if (*mk3_frame(thread, frame + 1) != 0)
        return -3;

    get_bomb_vel(obj);
    obj->field1c = (uint32_t)((int32_t)obj->field1c >> 1);   /* half */

    mk3_frame(thread, frame)[1] = (uint32_t)(uintptr_t)t_rbomb4;
    *mk3_frame(thread, frame + 1) = 0;
    return 0;
}


/* tl_projectile_flight_call -- armv7 0x00075918, 76 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      proc->field28 = obj->field34
 *      obj->field1c  = obj->field48
 *      tell_world_stk(obj)
 *      frame[frame].handler = tl_pflt3
 *      frame[frame+1].w0 = 0
 *
 * **It publishes three things and hands over.** 0x34 -- the per-frame callback
 * slot `t_flight_call` reads in mkfatal.c -- is copied into the proc's 0x28, the
 * animation in 0x48 is moved into 0x1c where `tell_world_stk` will resolve it,
 * and the stick that comes back is published in `proc->field84`.
 *
 * So a projectile in flight announces both its callback and its stick before the
 * flight routine starts, and `tell_world_stk` -- which had no caller when it was
 * read four functions ago -- has one now.
 *
 * `proc->field28` is the field the header records as "who the shake is about".
 * Here it holds a function pointer, which is a second reading of that offset and
 * is left as an observation rather than reconciled.
 */
long tl_pflt3(MK3THREAD *thread);

long tl_projectile_flight_call(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;

    if (*mk3_frame(thread, frame + 1) != 0)
        return -3;

    obj->field00->field28 = obj->field34;
    obj->field1c          = obj->field48;

    tell_world_stk(obj);

    mk3_frame(thread, frame)[1] = (uint32_t)(uintptr_t)tl_pflt3;
    *mk3_frame(thread, frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------------------------- tl_pflt3
 *
 * armv7 0x00076010, 328 bytes.  **Complete.**
 *
 * `tl_projectile_flight_call`'s own install target -- a self-looping strike
 * watch that reinstalls itself (its own address, taken by name) on a miss
 * rather than re-arming a token, the same shape `t_rocket_explode` uses for
 * `field78` but here turned into the routine's own resting state.
 *
 * The free just arms token `0x1368` and sleeps one tick. `0x1368` steps a
 * frame and checks `field00->field28` -- the function pointer
 * `tl_projectile_flight_call` parked there from its own `field34` -- and if
 * it is set, plants token `0x136e` (this routine's own strike-check state)
 * for when the pushed level pops, and PUSHES that pointer directly as the
 * next level's handler. A second reading of `field28`, which the header
 * already flags as a spill slot shared with the shake-target reading.
 *
 * `0x136e` (direct, or `field00->field28 == 0`'s fallthrough) is the actual
 * watch: `proj_onscreen_test` first -- offscreen installs
 * `tl_delete_proj_and_die` outright -- then a Motaro/action-0x402 check
 * (`him->field24`, then `get_his_action`) that runs `strike_check_a0_test`
 * instead of the ordinary path when either is true; a hit there launches
 * (`benedict_arnold_projectile`, `field20 = field00->field1c`, `field1c =
 * |field08->field18|`, `set_proj_vel`) and reinstalls itself, and a miss
 * there falls into the SAME ordinary strike attempt (`tell_world_stk`,
 * `proj_strike_check`) neither-special-case reaches directly -- one
 * physical block taken two ways. A hit there pops a level, or installs
 * `t_local_reaction_exit` at the bottom; a miss reinstalls `tl_pflt3`
 * itself, same as the launch path, so the watch simply runs again next
 * tick either way.
 */
void proj_strike_check(MK3OBJ *obj);
void benedict_arnold_projectile(MK3OBJ *obj);
long strike_check_a0_test(MK3OBJ *obj);
void get_his_action(MK3OBJ *obj);
long proj_onscreen_test(MK3OBJ *obj);
long tl_delete_proj_and_die(struct MK3THREAD *thread);

long tl_pflt3(MK3THREAD *thread)
{
    MK3OBJ   *obj            = (MK3OBJ *)thread->proc;
    uint32_t  frame          = thread->frame;
    uint32_t  slot           = *mk3_frame(thread, frame + 1);
    uint32_t  install_target;

    if (slot == 0x1368) {
        next_anirate(obj);

        obj->field34 = obj->field00->field28;
        if (obj->field34 != 0) {
            *mk3_frame(thread, frame + 1) = 0x136e;
            thread->frame = thread->frame + 1;   /* push a level */
            mk3_frame(thread, thread->frame)[1] = obj->field34;
            *mk3_frame(thread, thread->frame + 1) = 0;
            return 0;
        }
        goto check_target;
    }

    if (slot == 0x136e) {
check_target:
        if (proj_onscreen_test(obj) == 0) {
            install_target = (uint32_t)(uintptr_t)tl_delete_proj_and_die;
            goto install_p1;
        }

        obj->field20 =
            ((MK3OBJ *)(void *)(uintptr_t)obj->field00->him)->field24;
        if (obj->field20 == 0x18)
            goto motaro_or_402;

        get_his_action(obj);
        if (obj->field20 == 0x402)
            goto motaro_or_402;

        goto strike_attempt;
    }

    if (slot != 0)
        return -3;

    *mk3_frame(thread, frame + 1) = 0x1368;
    thread->fieldfc = 1;
    return 1;

strike_attempt:
    obj->field1c = obj->field48;
    tell_world_stk(obj);

    proj_strike_check(obj);
    if (obj->field5c != 0) {
        if ((long)thread->frame > 0) {
            thread->frame = thread->frame - 1;   /* back up a level */
            return 0;
        }

        install_target = (uint32_t)(uintptr_t)t_local_reaction_exit;
        goto install_p2;
    }

    install_target = (uint32_t)(uintptr_t)tl_pflt3;

install_p1:
    mk3_frame(thread, thread->frame)[1] = install_target;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;

motaro_or_402:
    obj->field1c = obj->field48;
    strike_check_a0_test(obj);

    if (obj->field5c == 0)
        goto strike_attempt;

    benedict_arnold_projectile(obj);

    obj->field20 = obj->field00->field1c;
    obj->field1c = obj->field08->field18;
    if ((int32_t)obj->field1c < 0)
        obj->field1c = (uint32_t)(-(int32_t)obj->field1c);
    set_proj_vel(obj);

    install_target = (uint32_t)(uintptr_t)tl_pflt3;

install_p2:
    mk3_frame(thread, thread->frame)[1] = install_target;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ======================================== t_bomb_call and t_mot_zap_call
 *
 * armv7 0x00075178 and 0x00075120, 88 bytes each.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = part->field1c + G
 *      part->field1c = obj->field1c
 *      if (thread->frame > 0) { thread->frame -= 1; return 0 }
 *      frame[frame].handler = t_local_reaction_exit
 *
 * where **G is 0x6000 for the bomb and 0x5000 for Motaro's zap**, and that
 * single constant is the whole difference between the two functions. Everything
 * else -- registers, order, the `push {r4}` with no `lr` because nothing is
 * called -- is identical.
 *
 * **These are per-frame callbacks, the shape `t_impale_call` has in mkfatal.c**:
 * do one thing and give the level straight back, so the flight routine that
 * calls them keeps control. `t_impale_call` is the elaborate version that seizes
 * the thread when a condition is met; these two never do anything but fall.
 *
 * 0x6000 and 0x5000 are the same two numbers the `t_flight` table carries in its
 * 0x24 column for `t_hit_by_bull` and `t_dino_bucked`. So the engine has one set
 * of fall rates and two ways of applying them -- inside `t_flight`, or by a
 * callback like this. Worth knowing before someone invents a third.
 *
 * The velocity is written to both 0x1c and the part, which is this file's habit:
 * the object's 0x1c is the scratch every helper reads, and the part is where it
 * has to end up.
 */
long t_bomb_call(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;

    if (*mk3_frame(thread, frame + 1) != 0)
        return -3;

    obj->field1c          = obj->field08->field1c + 0x6000;
    obj->field08->field1c = obj->field1c;

    if ((long)thread->frame > 0) {          /* cmp #0 / ble: signed */
        thread->frame = thread->frame - 1;  /* back up a level */
        return 0;
    }

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

long t_mot_zap_call(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;

    if (*mk3_frame(thread, frame + 1) != 0)
        return -3;

    obj->field1c          = obj->field08->field1c + 0x5000;
    obj->field08->field1c = obj->field1c;

    if ((long)thread->frame > 0) {          /* cmp #0 / ble: signed */
        thread->frame = thread->frame - 1;  /* back up a level */
        return 0;
    }

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* t_bomb_gravity2 -- armv7 0x00075384, 88 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      part->field1c = obj->field1c                 ; the y velocity, as given
 *      obj->field1c  = (int32_t)part->field18 >> 1  ; half the x velocity
 *      part->field18 = obj->field1c
 *      obj->field48  = obj->field48 << 1            ; double the counter
 *      frame[frame].handler = t_bomb_gravity
 *
 * **A bounce.** The y velocity is replaced with whatever the caller left in
 * 0x1c, the x velocity is halved, and then the routine hands over to
 * `t_bomb_gravity` -- the plain version -- so the bomb carries on falling with
 * less forward speed than it had.
 *
 * **0x48 is doubled on the way past**, and `get_bomb_vel` earlier in this file
 * puts 0x20 there as the number of frames the throw is divided into. Doubling it
 * each bounce means each bounce takes twice as long as the last, which is the
 * arithmetic of a ball losing energy -- shorter hops, longer intervals.
 *
 * The x halving is arithmetic, so a bomb thrown left bounces left.
 *
 * The name says it is the second of a pair and it installs the first, so
 * `t_bomb_gravity` is the steady state and this is the one frame where the
 * bounce happens.
 */
/* ------------------------------------------------------------------- t_bomb_gravity
 *
 * armv7 0x0007641c, 280 bytes.  **Complete.**
 *
 * The bomb's steady-state fall, `t_rbomb4`'s and `t_bomb_gravity2`'s own
 * install target. The free just arms token `0xca3` and sleeps one frame --
 * all the real setup already happened in the launcher. `0xca3` adds gravity
 * (`0x8000`) to the part's `field1c` every tick, write-through to `obj`'s own
 * copy the same way every other "one register, two fields" site in this file
 * does, and keeps re-arming itself while the velocity is still negative
 * (rising) or the part hasn't reached the landing height yet
 * (`field00->field40`, the same floor `t_rbomb4` and `t_flight` seed).
 *
 * Landing toggles a parity bit through `field00->field28` (`field1c` ends up
 * 8 or 9 depending on which way it flips, a different bounce cue each time)
 * and plays it. With bounces left (`field48 >= 0`) it just pops a level, or
 * installs `t_local_reaction_exit` at the bottom -- `t_bomb_gravity2` is
 * already sitting one level up, so popping is what runs it. With none left,
 * it does the pop (or the bottom-of-stack install) itself, then goes on to
 * overwrite whatever handler that left behind with `t_bgrav9` -- a genuine
 * dead store in the binary (three consecutive writes to the same handler
 * slot, and two to the token slot above it, transcribed as found).
 */
long t_bgrav9(struct MK3THREAD *thread);

long t_bomb_gravity(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t slot  = *mk3_frame(thread, frame + 1);

    if (slot == 0)
        goto wait;

    if (slot != 0xca3)
        return -3;

    next_anirate(obj);

    obj->field48 = obj->field48 - 1;

    obj->field1c          = obj->field08->field1c + 0x8000;
    obj->field08->field1c = obj->field1c;

    if ((int32_t)obj->field1c < 0)
        goto wait;

    obj->field30 = (uint32_t)(int32_t)MK3_FIELD12_S(obj->field08);

    {
        int32_t landing = (int32_t)obj->field00->field40;
        obj->field34 = (uint32_t)landing;
        if (landing > (int32_t)obj->field30)
            goto wait;
    }

    obj->field1c = 8;

    obj->field24 = obj->field00->field28 ^ 1;
    if (obj->field24 != 0)
        obj->field1c = 9;

    obj->field00->field28 = obj->field24;
    ochar_sound(obj);

    if ((int32_t)obj->field48 >= 0) {
        if ((long)thread->frame <= 0)
            return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

        thread->frame = thread->frame - 1;   /* back up a level */
        return 0;
    }

    /* every bounce spent: pop (or install at the bottom), then overwrite
     * whatever that left behind with t_bgrav9 -- dead stores included */
    if ((long)thread->frame <= 0) {
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_local_reaction_exit;   /* dead */
        *mk3_frame(thread, thread->frame + 1) = 0;          /* dead */
    } else {
        thread->frame = thread->frame - 1;   /* back up a level */
    }

    {
        uint32_t r1            = thread->frame;
        uint32_t r3            = r1 + 1;
        uint32_t self_handler  = mk3_frame(thread, r3)[1];
        uint32_t leftover      = *mk3_frame(thread, r3 + 1);

        *mk3_frame(thread, r3) = leftover;                    /* dead */
        mk3_frame(thread, r1)[1] = self_handler;                /* dead */
        mk3_frame(thread, r1)[1] = (uint32_t)(uintptr_t)t_bgrav9;
        *mk3_frame(thread, r1 + 1) = 0;
    }
    return 0;

wait:
    *mk3_frame(thread, frame + 1) = 0xca3;
    thread->fieldfc = 1;
    return 1;
}


long t_bomb_gravity2(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;

    if (*mk3_frame(thread, frame + 1) != 0)
        return -3;

    obj->field08->field1c = obj->field1c;

    obj->field1c          = (uint32_t)((int32_t)obj->field08->field18 >> 1);
    obj->field08->field18 = obj->field1c;

    obj->field48 = obj->field48 << 1;       /* twice as long next time */

    mk3_frame(thread, frame)[1] = (uint32_t)(uintptr_t)t_bomb_gravity;
    *mk3_frame(thread, frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------------------------ t_rbomb4
 *
 * armv7 0x00077d7c, 312 bytes.  **Complete.**
 *
 * A bomb-thread launcher that hands off to the SAME gravity pair
 * (`t_bomb_gravity` / `t_bomb_gravity2`) three other functions in this file
 * already drive, and reuses its own steady-state windup twice before it does:
 * the free throw sets up the GrObj (`field08->field1c = 0`), the landing
 * height in the PROC (`field00->field40 = G+0xac - 0x15`, the same
 * `floor - constant` shape `t_flight` writes for `mkprop.c` to read), and
 * pushes `t_bomb_gravity` under token `0xd08`. `0xd08` and `0xd0b` each just
 * repack `field1c` with a different large negative literal and push
 * `t_bomb_gravity2` again; `0xd0d` installs `t_bgrav9` on the current level
 * and the hand-off is done.
 *
 * `thread->pid = 0x20` and `obj->field54 = 0x20` come from the same register
 * write, not because the two fields share a meaning -- the compiler is just
 * reusing the value it already has in `r3`.
 */
long t_bgrav9(struct MK3THREAD *thread);

long t_rbomb4(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t slot  = *mk3_frame(thread, frame + 1);

    if (slot == 0xd08) {
        obj->field1c = 0xfffb0000;

        *mk3_frame(thread, frame + 1) = 0xd0b;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_bomb_gravity2;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot == 0xd0b) {
        obj->field1c = 0xfffd0000;

        *mk3_frame(thread, frame + 1) = 0xd0d;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_bomb_gravity2;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot == 0xd0d)
        return mk3_install(thread, (MK3THREADFUNC)t_bgrav9);

    if (slot != 0)
        return -3;

    obj->field54  = 0x20;
    thread->pid   = 0x20;

    set_proj_vel(obj);

    obj->field00->field40 = *(uint32_t *)(G_BYTES + 0xac) - 0x15;

    obj->field1c = 3;
    init_anirate(obj);

    obj->field40 = 4;
    get_char_ani2(obj);

    obj->field20          = 0;
    obj->field08->field1c = 0;

    *mk3_frame(thread, frame + 1) = 0xd08;
    thread->frame = thread->frame + 1;   /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_bomb_gravity;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* t_lk_prezap_hit -- armv7 0x00077a40, 96 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      saved = obj->field08
 *      obj->field08 = proc->slave           ; drive the slave's part instead
 *      make_dragon_explode(obj)
 *      obj->field08 = saved
 *      delete_slave(obj)
 *      frame[frame].handler = t_lkzap5
 *
 * **It borrows the slave's part for one call.** `make_dragon_explode` measures
 * whatever is in `obj->field08` and blows it up; pointing 0x08 at the slave for
 * the length of that call makes it explode the slave instead of the fighter,
 * and the field goes straight back afterwards.
 *
 * Same idea as `t_kissani`'s two-field swap in mkfatal.c and mkanimal.c's
 * `create_fx_for_him`, and the third spelling of it: **a callee-saved register
 * across one call**, because nothing here has to survive a descent.
 *
 * **It also confirms which slave word is which.** `get_proj_obj_m` puts the new
 * object in `proc->field64` and its PART in `proc->slave` (0x68); this reads 0x68
 * straight into `obj->field08`, which only holds parts. So 0x64 is the object,
 * 0x68 is its part, and the two are not interchangeable.
 *
 * Then `delete_slave` -- an eighth call site, and the first outside mkfatal.c.
 */
void delete_slave(MK3OBJ *obj);
long t_lkzap5(MK3THREAD *thread);


/* --------------------------------------------------------------------- t_lk_prezap
 *
 * armv7 0x000770bc, 340 bytes.  **Complete.**
 *
 * A one-tick lookahead like `t_spit_prezap`, but with three fields saved
 * instead of two (`field4c`, `field20`, `field24`) and a slave-part check
 * instead of a plain box test. The free saves the three, steps one frame,
 * and self-resumes at `0x9ef` two ticks later. `0x9ef` restores them in
 * reverse (`field24`, `field20`, `field4c`) and, only when
 * `proc->slave` names one, borrows it into `field08` for
 * `local_strike_check_box` -- the same "point 0x08 at the slave for one
 * call" idiom `t_lk_prezap_hit` documents just below. A connect pops a
 * level when it can and installs `t_lk_prezap_hit` either way; a miss
 * either pops a level and returns, or falls into `t_local_reaction_exit`
 * at the bottom -- the ordinary idiom, just reached without an intervening
 * install on the pop side.
 *
 *      slot = frame[frame+1].w0
 *      if (slot != 0) {
 *          if (slot != 0x9ef) return -3
 *          fieldf8-- ; obj->field24 = args[fieldf8]
 *          fieldf8-- ; obj->field20 = args[fieldf8]
 *          fieldf8-- ; obj->field4c = args[fieldf8]
 *          if (proc->slave != 0) {
 *              saved = obj->field08 ; obj->field08 = proc->slave
 *              obj->field1c = 0x11 ; local_strike_check_box(obj)
 *              obj->field08 = saved
 *              if (obj->field5c != 0) {
 *                  if (frame > 0) frame -= 1
 *                  install t_lk_prezap_hit ; return 0
 *              }
 *          }
 *          if (frame > 0) { frame -= 1 ; return 0 }
 *          install t_local_reaction_exit ; return 0
 *      }
 *      args[fieldf8] = obj->field4c ; fieldf8++
 *      args[fieldf8] = obj->field20 ; fieldf8++
 *      args[fieldf8] = obj->field24 ; fieldf8++
 *      do_next_a9_frame(obj)
 *      token 0x9ef ; fieldfc = 4 ; return 4
 */
long t_lk_prezap_hit(struct MK3THREAD *thread);

long t_lk_prezap(MK3THREAD *thread)
{
    MK3OBJ   *obj  = (MK3OBJ *)thread->proc;
    uint32_t *args = (uint32_t *)(void *)thread->args;
    uint32_t  frame = thread->frame;
    uint32_t  slot  = *mk3_frame(thread, frame + 1);

    if (slot != 0) {
        if (slot != 0x9ef)
            return -3;

        thread->fieldf8 = thread->fieldf8 - 1;
        obj->field24 = args[thread->fieldf8];
        thread->fieldf8 = thread->fieldf8 - 1;
        obj->field20 = args[thread->fieldf8];
        thread->fieldf8 = thread->fieldf8 - 1;
        obj->field4c = args[thread->fieldf8];

        if (obj->field00->slave != 0) {
            MK3OBJ *saved08 = obj->field08;

            obj->field08 = (MK3OBJ *)(uintptr_t)obj->field00->slave;
            obj->field1c = 0x11;
            local_strike_check_box(obj);
            obj->field08 = saved08;

            if (obj->field5c != 0) {
                if ((long)thread->frame > 0)
                    thread->frame = thread->frame - 1;
                return mk3_install(thread, (MK3THREADFUNC)t_lk_prezap_hit);
            }
        }

        if ((long)thread->frame > 0) {
            thread->frame = thread->frame - 1;
            return 0;
        }
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }

    args[thread->fieldf8] = obj->field4c;
    thread->fieldf8 = thread->fieldf8 + 1;
    args[thread->fieldf8] = obj->field20;
    thread->fieldf8 = thread->fieldf8 + 1;
    args[thread->fieldf8] = obj->field24;
    thread->fieldf8 = thread->fieldf8 + 1;
    do_next_a9_frame(obj);

    *mk3_frame(thread, frame + 1) = 0x9ef;
    thread->fieldfc = 4;
    return 4;
}


long t_lk_prezap_hit(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    MK3OBJ  *saved;

    if (*mk3_frame(thread, frame + 1) != 0)
        return -3;

    saved = obj->field08;
    obj->field08 = (MK3OBJ *)(void *)(uintptr_t)obj->field00->slave;

    make_dragon_explode(obj);

    obj->field08 = saved;

    delete_slave(obj);

    mk3_frame(thread, frame)[1] = (uint32_t)(uintptr_t)t_lkzap5;
    *mk3_frame(thread, frame + 1) = 0;
    return 0;
}


/* t_rocket_explode_fx -- armv7 0x00076950, 96 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0xa; ochar_sound(obj)
 *      obj->a10     = (int16_t)part->x0e
 *      obj->field48 = (int16_t)part->x12
 *      obj->field1c = 5
 *      create_fx(obj)
 *      frame[frame].handler = tl_delete_proj_and_die
 *
 * **`create_fx` can be aimed two ways, and this is the other one.** Here the
 * position is copied into 0x44 and 0x48 as plain coordinates before the call;
 * `make_lineup_explode` earlier in this file instead rewrites the part's own x
 * and y, calls, and puts them back. Two mechanisms for the same helper, in one
 * file, forty functions apart.
 *
 * Which means `create_fx` reads 0x44 and 0x48 when they are set and falls back
 * to the part otherwise -- or reads them always and `make_lineup_explode` is
 * leaving them stale, which would explain why its width never mattered. **Not
 * settled here**; whoever decompiles `create_fx` at 0x00058d70 settles both.
 *
 * The coordinates are sign-extended halfwords widened into full words, so a
 * projectile off the left of the screen keeps a negative x rather than wrapping.
 */
long t_rocket_explode_fx(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;

    if (*mk3_frame(thread, frame + 1) != 0)
        return -3;

    obj->field1c = 0xa;
    ochar_sound(obj);

    obj->a10     = (uint32_t)(int32_t)(int16_t)MK3_FIELD0E(obj->field08);
    obj->field48 = (uint32_t)(int32_t)(int16_t)MK3_FIELD12(obj->field08);

    obj->field1c = 5;
    create_fx(obj);

    mk3_frame(thread, frame)[1] =
        (uint32_t)(uintptr_t)tl_delete_proj_and_die;
    *mk3_frame(thread, frame + 1) = 0;
    return 0;
}


/* ======================== tl_do_reptile_orb and tl_do_reptile_orb_fast
 *
 * armv7 0x0007ac58 and 0x0007acbc, 100 and 104 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = G + T ; update_tsl(obj)
 *      obj->field20 = A
 *      obj->a10     = 0
 *      zap_init_special_act(obj)
 *      obj->field48 = V
 *      frame[frame].handler = tl_orb3
 *
 *                          T        A       V
 *      slow            0x438     0x21   0x30000
 *      fast            0x43c     0x22   0x70000
 *
 * **Entries 34 and 40 of `projectile_jumps`, and three constants apart.** One
 * timer slot in the globals, one special-act number, one speed -- and the same
 * `tl_orb3` runs both. The fast orb is not a different move; it is the same move
 * with a different row of numbers, which is how this whole file is built.
 *
 * The four extra bytes in the fast one are the address arithmetic:
 * `add.w #0x430` then `adds #0xc` where the slow one fits `add.w #0x438` in a
 * single instruction. The shared-literal habit, applied to a pointer.
 *
 * `G + 0x438` and `G + 0x43c` are adjacent words, so the two orbs have one timer
 * slot each and cannot be in flight at the same time as themselves -- but can be
 * as each other. mkstat.c reaches `G + 0x410 + 0xc` the same way for Jade's
 * flash, so 0x410 onwards is a block of these.
 */
void update_tsl(MK3OBJ *obj);
long tl_orb3(MK3THREAD *thread);

long tl_do_reptile_orb(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;

    if (*mk3_frame(thread, frame + 1) != 0)
        return -3;

    obj->field1c = (uint32_t)(uintptr_t)(G_BYTES + 0x438);
    update_tsl(obj);

    obj->field20 = 0x21;
    obj->a10     = 0;
    zap_init_special_act(obj);

    obj->field48 = 0x30000;

    mk3_frame(thread, frame)[1] = (uint32_t)(uintptr_t)tl_orb3;
    *mk3_frame(thread, frame + 1) = 0;
    return 0;
}

long tl_do_reptile_orb_fast(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;

    if (*mk3_frame(thread, frame + 1) != 0)
        return -3;

    obj->field1c = (uint32_t)(uintptr_t)(G_BYTES + 0x430 + 0xc);
    update_tsl(obj);

    obj->field20 = 0x22;
    obj->a10     = 0;
    zap_init_special_act(obj);

    obj->field48 = 0x70000;

    mk3_frame(thread, frame)[1] = (uint32_t)(uintptr_t)tl_orb3;
    *mk3_frame(thread, frame + 1) = 0;
    return 0;
}


/* ===================================================== the four callbacks
 *
 * Four more of the shape `t_bomb_call` has: do one thing, then pop a level or
 * install `t_local_reaction_exit` at the bottom. What differs is where each one
 * keeps its counter, and between them they show the engine has three places to
 * put one.
 * ======================================================================== */

/* t_orb_calla -- armv7 0x00076534, 104 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      if (--obj->a10 == 0) {
 *          obj->a10     = 3
 *          obj->field1c = 3 + 0xf = 0x12
 *          ochar_sound(obj)
 *      }
 *      pop a level, or t_local_reaction_exit at the bottom
 *
 * **A tick every third frame**, and the counter is on the OBJECT. Sound 0x12
 * and the reload value 3 come from one register -- `adds r3, #3` on the zero the
 * branch just proved, then `adds r3, #0xf` on that -- so the period and the
 * sound number are welded together by the shared-literal habit even though they
 * have nothing to do with each other.
 */
long t_orb_calla(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;

    if (*mk3_frame(thread, frame + 1) != 0)
        return -3;

    obj->a10 = obj->a10 - 1;
    if (obj->a10 == 0) {
        obj->a10     = 3;
        obj->field1c = 3 + 0xf;             /* the same register */
        ochar_sound(obj);
    }

    if ((long)thread->frame > 0) {          /* cmp #0 / ble: signed */
        thread->frame = thread->frame - 1;  /* back up a level */
        return 0;
    }

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* -------------------------------------------------------------- t_orb_proc
 *
 * armv7 0x0007ba10, 248 bytes.  **Complete.**
 *
 * The orb's own launch/flight/land, three states over `tl_projectile_flight_call`
 * and `t_mframew`. The free poses animation 5 (`field54 = 3`,
 * `find_ani2_part_a14`), throws with whatever `a10` already held going in
 * (`field1c = a10` before `set_proj_vel`), tags `field48 = 0x11`, resets
 * `a10 = 3` (the same three-frame reload `t_orb_calla` counts down) and
 * `field34 = t_orb_calla`, then flies from token `0x4df`. `0x4df` stops the
 * GrObj (`stop_a8`), re-poses the same animation 5 at four frames this time
 * and lands on `0x4e9` through `t_mframew`; `0x4e9` is the floor, closing on
 * `tl_delete_proj_and_die`.
 *
 *      slot = frame[frame+1].w0
 *                                          ; 0:     free, then 0x4df
 *                                          ; 0x4df: land pose, then 0x4e9
 *                                          ; 0x4e9: die
 *      if (slot == 0x4df) {
 *          stop_a8(obj->field08)
 *          obj->field1c = 5 ; ochar_sound(obj)
 *          obj->field40 = 5 ; obj->field54 = 4 ; find_ani2_part_a14(obj)
 *          obj->field1c = 3
 *          token 0x4e9 ; frame++ ; install t_mframew ; return 0
 *      }
 *      if (slot == 0x4e9) install tl_delete_proj_and_die ; return 0
 *      if (slot != 0) return -3
 *      obj->field54 = 3 ; obj->field40 = 5 ; find_ani2_part_a14(obj)
 *      obj->field20 = 3 ; obj->field1c = obj->a10 ; set_proj_vel(obj)
 *      obj->field48 = 0x11 ; obj->a10 = 3 ; obj->field34 = t_orb_calla
 *      token 0x4df ; frame++ ; install tl_projectile_flight_call ; return 0
 *
 * `obj->field1c = obj->a10` in the free state reads whatever the caller left
 * in `a10` before installing this proc -- unlike every other projectile
 * launch in this file, which sets its own velocity component fresh, the orb
 * takes it from a value already sitting there. Transcribed as read; nothing
 * downstream of this routine says what put it there.
 */
void find_ani2_part_a14(MK3OBJ *obj);
void stop_a8(MK3OBJ *part);
long t_mframew(struct MK3THREAD *thread);
long t_orb_calla(MK3THREAD *thread);

long t_orb_proc(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t slot  = *mk3_frame(thread, frame + 1);

    if (slot == 0x4df) {
        stop_a8(obj->field08);

        obj->field1c = 5;
        ochar_sound(obj);

        obj->field40 = 5;
        obj->field54 = 4;
        find_ani2_part_a14(obj);

        obj->field1c = 3;

        *mk3_frame(thread, frame + 1) = 0x4e9;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot == 0x4e9)
        return mk3_install(thread, (MK3THREADFUNC)tl_delete_proj_and_die);

    if (slot != 0)
        return -3;

    obj->field54 = 3;
    obj->field40 = 5;
    find_ani2_part_a14(obj);

    obj->field20 = 3;
    obj->field1c = obj->a10;
    set_proj_vel(obj);

    obj->field48 = 0x11;
    obj->a10     = 3;
    obj->field34 = (uint32_t)(uintptr_t)t_orb_calla;

    *mk3_frame(thread, frame + 1) = 0x4df;
    thread->frame = thread->frame + 1;   /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)tl_projectile_flight_call;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------------------------- tl_orb3
 *
 * armv7 0x00075c78, 244 bytes.  **Complete.**
 *
 * `tl_do_reptile_orb`'s own descent target. The free poses `field40 = 0x30005`
 * and pushes `t_animate2_a9` (the same pointer-slot borrow every generic
 * "play this animation and come back" site in this file uses) under `0x505`,
 * self-resuming here.
 *
 * `0x505` is the actual launch: `field38 = t_orb_proc` (the usual hit
 * callback), `create_proj_proc` spawns it, and when a slave comes back its
 * `a10` is seeded from this object's own `field48`. `detach_proj`
 * follows regardless, then an eight-tick wait under `0x50f`.
 *
 * `0x50f` poses nothing new -- just `field1c = 4` and a push of `t_mframew`
 * under `0x512`, whose own re-entry is the ordinary tail every multi-level
 * descent in this file ends on: pop a level, or install
 * `t_local_reaction_exit` at the bottom.
 */
long t_animate2_a9(struct MK3THREAD *thread);            /* pointer slot 0x000f36c0 */
long t_orb_proc(MK3THREAD *thread);

long tl_orb3(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t slot  = *mk3_frame(thread, frame + 1);

    if (slot == 0x505) {
        MK3OBJ *slave;

        obj->field38 = (uint32_t)(uintptr_t)t_orb_proc;
        slave = create_proj_proc(obj);

        if (slave != NULL)
            slave->a10 = obj->field48;

        detach_proj(obj);

        *mk3_frame(thread, frame + 1) = 0x50f;
        thread->fieldfc = 8;
        return 8;
    }

    if (slot == 0x50f) {
        obj->field1c = 4;

        *mk3_frame(thread, frame + 1) = 0x512;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot == 0x512) {
        if ((long)thread->frame > 0) {
            thread->frame = thread->frame - 1;   /* back up a level */
            return 0;
        }

        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }

    if (slot != 0)
        return -3;

    obj->field40 = 0x30005;

    *mk3_frame(thread, frame + 1) = 0x505;
    thread->frame = thread->frame + 1;   /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_animate2_a9;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* t_lao_zap_call -- armv7 0x00075464, 108 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = proc->field2c - 1
 *      if (obj->field1c == 0) obj->field1c = 2
 *      proc->field2c = obj->field1c
 *      obj->field1c  = part->field1c + 0x3000
 *      part->field1c = obj->field1c
 *      pop a level, or t_local_reaction_exit at the bottom
 *
 * **The counter is on the PROC, and it never reaches zero twice.** 0x2c counts
 * 2, 1, 2, 1 for ever -- decrement, and if that made it zero put 2 back -- which
 * is a two-frame phase that nothing in this routine reads. It is kept for
 * whoever else looks at `proc->field2c`.
 *
 * That is the difference from `t_orb_calla` above: same shape, same tail, but
 * the counter lives on the proc so it outlives the object. The header now has a
 * field for it.
 *
 * The gravity is a plain 0x3000 a frame, a fourth fall rate after 0x2000, 0x5000
 * and 0x6000.
 */
long t_lao_zap_call(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;

    if (*mk3_frame(thread, frame + 1) != 0)
        return -3;

    obj->field1c = obj->field00->field2c - 1;
    if (obj->field1c == 0)
        obj->field1c = 2;
    obj->field00->field2c = obj->field1c;

    obj->field1c          = obj->field08->field1c + 0x3000;
    obj->field08->field1c = obj->field1c;

    if ((long)thread->frame > 0) {          /* cmp #0 / ble: signed */
        thread->frame = thread->frame - 1;  /* back up a level */
        return 0;
    }

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* t_jax_proj_calla -- armv7 0x000768e4, 108 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = proc->field38 - 1
 *      if (obj->field1c == 0) {
 *          obj->field1c = 0xf; create_fx(obj)
 *          obj->field1c = 3
 *      }
 *      proc->field38 = obj->field1c
 *      pop a level, or t_local_reaction_exit at the bottom
 *
 * **Effect 0xf every third frame, counted on the proc at 0x38.** Same shape as
 * `t_lao_zap_call`, a different word of the same structure, and this one does
 * something when it fires.
 *
 * So the three counters in this batch sit in three different places -- `obj->a10`
 * for the orb, `proc->field2c` for Lao's zap, `proc->field38` for Jax's -- and
 * nothing about the shape says which to use. A port reproduces all three
 * separately; there is no shared slot to factor them into.
 *
 * 0xf is `adds r3, #0xf` on the zero the branch proved, and the reload 3 is a
 * fresh `movs`. So this one does NOT weld the two constants together the way
 * `t_orb_calla` does with 3 and 0x12 -- which is worth noticing, because it means
 * the welding there is the compiler's choice and not a pattern with meaning.
 */
long t_jax_proj_calla(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;

    if (*mk3_frame(thread, frame + 1) != 0)
        return -3;

    obj->field1c = obj->field00->field38 - 1;

    if (obj->field1c == 0) {
        obj->field1c = 0xf;
        create_fx(obj);

        obj->field1c = 3;
    }

    obj->field00->field38 = obj->field1c;

    if ((long)thread->frame > 0) {          /* cmp #0 / ble: signed */
        thread->frame = thread->frame - 1;  /* back up a level */
        return 0;
    }

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* t_rr_up -- armv7 0x000754d0, 104 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      v = part->field1c
 *      obj->field24 = v                              ; DEAD
 *      obj->field20 = rocket_routines[obj->field1c * 3 + 1]
 *      obj->field24 = v - obj->field20
 *      part->field1c = obj->field24
 *      frame[frame].handler = t_rr_nothing
 *
 * **`rocket_routines` is a four-entry script at 0x00172664, twelve bytes each**,
 * and it ends exactly where `projectile_jumps` begins:
 *
 *      [0]  t_rr_nothing    0x3333   0x0008
 *      [1]  t_rr_up         0xa000   0x0010
 *      [2]  t_rocket_hunt   0x3333   0x3333
 *      [3]  0               0        0          <- terminator
 *
 * So a rocket runs a phase list: each entry names the handler for that phase, an
 * amount and something that reads like a duration. This routine is entry 1's own
 * handler and reads entry `obj->field1c`'s **amount** -- 0xa000 when it is
 * running its own phase -- and subtracts it from the y velocity, which is what
 * makes the rocket climb.
 *
 * The stride is computed as `index << 4` minus `index << 2` rather than by a
 * multiply, which is the twelve-byte giveaway.
 *
 * **`obj->field24 = v` before the table read is dead** -- overwritten four
 * instructions later with `v - amount`, nothing in between. Transcribed; tenth
 * dead store recorded.
 *
 * It hands over to `t_rr_nothing`, which is entry 0's handler and does nothing
 * at all, so a phase that has done its work parks the rocket on the do-nothing
 * entry rather than advancing the index.
 */
extern uint32_t rocket_routines[];               /* 0x00172664, 4 x 12 bytes */

long t_rr_up(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t v;

    if (*mk3_frame(thread, frame + 1) != 0)
        return -3;

    v = obj->field08->field1c;
    obj->field24 = v;                            /* dead: rewritten below */

    obj->field20 = rocket_routines[obj->field1c * 3 + 1];

    obj->field24          = v - obj->field20;
    obj->field08->field1c = obj->field24;

    mk3_frame(thread, frame)[1] = (uint32_t)(uintptr_t)t_rr_nothing;
    *mk3_frame(thread, frame + 1) = 0;
    return 0;
}


/* t_target -- armv7 0x00078274, 108 bytes.  **Complete.**
 *
 *      token == 0:      obj->field1c = 0
 *                       -- falls into the step --
 *
 *      the step:        frame_a9(obj)
 *                       obj->field1c = 5
 *                       -- falls into the placement --
 *
 *      the placement:   part->x0e = (uint16_t)him->x0e - 0x10
 *                       part->x12 = (uint16_t)him->x12 + 0x20
 *                       token := 0xfd4, park 1
 *
 *      token == 0xfd4:  if (--obj->field1c != 0) -- the placement --
 *                       -- the step --
 *
 *      otherwise:       return -3
 *
 * **A reticle that follows the opponent every frame and animates every fifth.**
 * The placement runs on all of them -- sixteen units left of the victim and
 * thirty-two below -- and the counter in 0x1c only decides when `frame_a9`
 * advances the picture.
 *
 * **The offsets are unsigned halfword arithmetic**: `ldrh`, add or subtract,
 * `strh`. So a target 0x10 to the left of a victim standing at x = 8 wraps to
 * 0xfff8 rather than going negative, and whatever draws it has to read the
 * halfword the same way. `t_chop_off_his_height` in mkfatal.c reads the same
 * class of field unsigned and `lifts3` reads it signed; this is a third site and
 * it sides with the unsigned reading.
 *
 * The park is 1 in every state, so the routine costs a frame per frame and never
 * ends on its own.
 */
void frame_a9(MK3OBJ *obj);

long t_target(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);
    MK3OBJ  *him;
    int      step;

    if (token == 0) {
        obj->field1c = 0;
        step = 1;

    } else if (token == 0xfd4) {
        obj->field1c = obj->field1c - 1;
        step = (obj->field1c == 0);

    } else {
        return -3;
    }

    if (step) {
        frame_a9(obj);
        obj->field1c = 5;
    }

    him = (MK3OBJ *)(void *)(uintptr_t)obj->field00->him;

    MK3_SET_FIELD0E(obj->field08, (uint32_t)MK3_FIELD0E(him) - 0x10);
    MK3_SET_FIELD12(obj->field08, (uint32_t)MK3_FIELD12(him) + 0x20);

    *mk3_frame(thread, frame + 1) = 0xfd4;
    thread->fieldfc = 1;
    return 1;
}


/* t_doice3 -- armv7 0x000778c4, 112 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      is_he_right(obj)
 *      if (obj->field5c == 0) obj->field48 = -obj->field48
 *      frame[frame].handler = t_doice5
 *
 * **One conditional negation and a hand-over.** The whole routine exists to
 * point 0x48 the right way before `t_doice5` runs, which is the same job
 * `set_proj_vel` does for 0x1c -- except that one reads the part's flip bit and
 * this one asks `is_he_right`.
 *
 * **Two ways to answer the same question**, then: the flip bit says which way
 * the fighter is DRAWN and `is_he_right` says where the opponent actually is.
 * They agree most of the time and a port must not substitute one for the other.
 *
 * The compiler emitted the install twice, once per branch, rather than joining
 * them -- so the function is 112 bytes for what is four instructions of work.
 */
long is_he_right(MK3OBJ *obj);
long t_doice5(MK3THREAD *thread);

long t_doice3(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;

    if (*mk3_frame(thread, frame + 1) != 0)
        return -3;

    is_he_right(obj);
    if (obj->field5c == 0)
        obj->field48 = (uint32_t)(-(long)obj->field48);

    mk3_frame(thread, frame)[1] = (uint32_t)(uintptr_t)t_doice5;
    *mk3_frame(thread, frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------------------------- t_doice5
 *
 * armv7 0x00079940, 348 bytes.  **Complete.**
 *
 * `t_doice3`'s own descent target, and a chain of borrowed generic threads
 * rather than the ordinary `field38` launch: the free arms a throw
 * (`field20=6`, `a10=0`, `zap_init_special_act`, `field1c=0`, `ochar_sound`,
 * `field40=0x30000`) and pushes `t_animate2_a9` under `0x11ad`, the same
 * pointer-slot borrow every generic "play this and come back" site in this
 * file uses.
 *
 * `0x11ad` just poses `field1c=2` and pushes `t_mframew` under `0x11b0`,
 * whose own re-entry is the real content: `delete_slave` and
 * `NewThread(obj, t_sky_ice_proc)` -- a genuinely separate falling icicle,
 * spawned as its OWN thread rather than descended into, its return value
 * discarded -- then `field1c=3` and a push of `t_mframew` under `0x11b9`.
 *
 * `0x11b9` runs `delete_slave` again, clears `field40`, sets `field1c=3`,
 * and pushes `t_backwards_ani2` under `0x11be`, which is the ordinary tail
 * every multi-level descent in this file ends on: pop a level, or install
 * `t_local_reaction_exit` at the bottom.
 */
void delete_slave(MK3OBJ *obj);
MK3THREAD *NewThread(void *owner, MK3THREADFUNC func);
long t_sky_ice_proc(struct MK3THREAD *thread);
long t_animate2_a9(struct MK3THREAD *thread);            /* pointer slot 0x000f36c0 */
long t_backwards_ani2(struct MK3THREAD *thread);         /* pointer slot 0x000f3704 */

long t_doice5(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t slot  = *mk3_frame(thread, frame + 1);

    if (slot == 0x11b0) {
        delete_slave(obj);
        NewThread(obj, (MK3THREADFUNC)t_sky_ice_proc);

        obj->field1c = 3;

        *mk3_frame(thread, frame + 1) = 0x11b9;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot == 0x11b9) {
        delete_slave(obj);

        obj->field40 = 0;
        obj->field1c = 3;

        *mk3_frame(thread, frame + 1) = 0x11be;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_backwards_ani2;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot == 0x11be) {
        if ((long)thread->frame > 0) {
            thread->frame = thread->frame - 1;   /* back up a level */
            return 0;
        }

        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }

    if (slot == 0x11ad) {
        obj->field1c = 2;

        *mk3_frame(thread, frame + 1) = 0x11b0;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot != 0)
        return -3;

    obj->field20 = 6;
    obj->a10     = 0;
    zap_init_special_act(obj);

    obj->field1c = 0;
    ochar_sound(obj);

    obj->field40 = 0x30000;

    *mk3_frame(thread, frame + 1) = 0x11ad;
    thread->frame = thread->frame + 1;   /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_animate2_a9;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* t_sg_trail_spawn -- armv7 0x00076a54, 112 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = proc->field2c - 1
 *      if (obj->field1c <= 0) {
 *          obj->field1c = 0x35; create_fx(obj)
 *          obj->field1c = 4
 *      }
 *      proc->field2c = obj->field1c
 *      pop a level, or t_local_reaction_exit at the bottom
 *
 * **The third counter-on-the-proc callback**, after `t_lao_zap_call` and
 * `t_jax_proj_calla`. It shares 0x2c with the first of those, so two different
 * projectiles cannot be counting there at once -- which is fine, because a proc
 * belongs to one fighter and a fighter has one projectile in the air.
 *
 * The test is `ble` where `t_jax_proj_calla` uses `cbz`, so a counter that
 * somehow went negative fires here and hangs there. The difference costs nothing
 * and is transcribed as `<= 0` rather than normalised to `== 0`.
 *
 * Effect 0x35 every four frames leaves the trail the name promises.
 */
long t_sg_trail_spawn(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;

    if (*mk3_frame(thread, frame + 1) != 0)
        return -3;

    obj->field1c = obj->field00->field2c - 1;

    if ((long)obj->field1c <= 0) {              /* ble, not cbz */
        obj->field1c = 0x35;
        create_fx(obj);

        obj->field1c = 4;
    }

    obj->field00->field2c = obj->field1c;

    if ((long)thread->frame > 0) {              /* cmp #0 / ble: signed */
        thread->frame = thread->frame - 1;      /* back up a level */
        return 0;
    }

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* t_rocket1_flight_call -- armv7 0x00075d90, 120 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field28 = 3
 *      v = part->field18
 *      obj->field20 = (int32_t)v >> 4
 *      obj->field1c = obj->field20 + v                  ; 17/16 of it
 *      if (obj->field1c < 0) obj->field1c = -obj->field1c
 *      if (obj->field1c > 0xe0000) obj->field1c = 0xe0000
 *      set_proj_vel(obj)
 *      pop a level, or t_local_reaction_exit at the bottom
 *
 * **The rocket accelerates by a sixteenth a frame and tops out at 0xe0000**, and
 * this is the routine that explains why `set_proj_vel` wants an unsigned speed:
 * the magnitude is computed here, clamped here, and handed over for that helper
 * to re-sign by the flip bit. Neither routine has to know which way the rocket
 * is pointing.
 *
 * **The animation rate comes out of the same division.** `set_proj_vel` moves
 * 0x20 into 0x1c and calls `init_anirate`, and 0x20 is `v >> 4` -- so the rocket
 * animates faster as it goes faster, from one `asrs`. That is worth knowing
 * before a port hard-codes a frame rate for it.
 *
 * `obj->field28 = 3` has no reader in this routine and none in `set_proj_vel`,
 * which writes 0x2c and reads 0x1c and 0x20. Whether `init_anirate` reads 0x28 is
 * not settled here, so it is transcribed rather than called dead.
 */
long t_rocket1_flight_call(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t v;

    if (*mk3_frame(thread, frame + 1) != 0)
        return -3;

    obj->field28 = 3;

    v = obj->field08->field18;
    obj->field20 = (uint32_t)((int32_t)v >> 4);       /* also the anirate */
    obj->field1c = obj->field20 + v;

    if ((long)obj->field1c < 0)
        obj->field1c = (uint32_t)(-(long)obj->field1c);

    if ((long)obj->field1c > 0xe0000)
        obj->field1c = 0xe0000;                      /* the top speed */

    set_proj_vel(obj);

    if ((long)thread->frame > 0) {          /* cmp #0 / ble: signed */
        thread->frame = thread->frame - 1;  /* back up a level */
        return 0;
    }

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* benedict_arnold_projectile -- armv7 0x00075f98, 120 bytes.  **Complete.**
 *
 *      other = proc->field00
 *      obj->field34 = other
 *      proc->him     = other->proc->him
 *      proc->field00 = other->proc->field00
 *
 *      get_frontmost_point(obj); before = obj->field28
 *      flip_multi(obj)
 *      get_frontmost_point(obj); after  = obj->field28
 *      d = -|after - before|
 *      obj->field2c = before
 *
 *      rightmost_mpart_ob(obj, part)          -> obj->field28
 *      leftmost_mpart_ob(obj, part)           -> obj->field24
 *      obj->field24 = |obj->field24 - obj->field28|
 *      obj->field1c = obj->field24 + d
 *      obj->field20 = 0
 *      multi_adjust_xy(obj)
 *
 * **The name is the whole explanation: the projectile changes sides.** The first
 * four lines copy the OTHER object's proc's `him` and `field00` into this proc's,
 * so a projectile that was aimed at the opponent is now aimed at whoever the
 * opponent was aimed at -- its own owner. Nothing else in the tree rewrites a
 * proc's `him`.
 *
 * **Then it re-centres itself for the turn.** A projectile that reverses has to
 * move, because its art is drawn from one edge: the front point is measured,
 * `flip_multi` mirrors it, the front point is measured again, and the shift is
 * the body's width less the distance the front moved. The vertical shift is
 * zero, so it only slides sideways.
 *
 * `-|after - before|` survives both edge calls in a callee-saved register, which
 * it has to, because **`rightmost_mpart_ob` overwrites 0x28** -- the field the
 * difference was stored in. That store is therefore dead, and so are two more to
 * the same field inside the `it lt` block above it. **Three dead stores to 0x28
 * in one routine**, all superseded before anything reads them; transcribed
 * because a reader diffing against the disassembly will see all three.
 *
 * `obj->field2c = before` is written between the two halves of an `it lt` pair,
 * which is the compiler interleaving an unconditional store into a conditional
 * sequence. It is not conditional.
 */
void multi_adjust_xy(MK3OBJ *obj);

void benedict_arnold_projectile(MK3OBJ *obj)
{
    MK3OBJ  *other;
    uint32_t before, after;
    int32_t  d;

    other = obj->field00->field00;
    obj->field34 = (uint32_t)(uintptr_t)other;

    obj->field00->him     = other->field00->him;
    obj->field00->field00 = other->field00->field00;

    get_frontmost_point(obj);
    before = obj->field28;

    flip_multi(obj);

    get_frontmost_point(obj);
    after = obj->field28;

    obj->field28 = after - before;              /* dead: 0x28 is clobbered */
    d = (int32_t)(after - before);
    if (d < 0) {
        d = -d;
        obj->field28 = (uint32_t)d;             /* dead as well */
    }
    obj->field2c = before;                      /* not conditional */
    d = -d;
    obj->field28 = (uint32_t)d;                 /* dead: rightmost writes it */

    rightmost_mpart_ob(obj, obj->field08);      /* answers in 0x28 */
    leftmost_mpart_ob(obj, obj->field08);       /* answers in 0x24 */

    obj->field24 = obj->field24 - obj->field28;
    if ((long)obj->field24 < 0)
        obj->field24 = (uint32_t)(-(long)obj->field24);

    obj->field28 = obj->field24 + (uint32_t)d;
    obj->field1c = obj->field28;
    obj->field20 = 0;                           /* sideways only */

    multi_adjust_xy(obj);
}


/* create_proj_proc -- armv7 0x00075964, 120 bytes.  **Complete.**
 *
 *      obj->field20 = obj->field1c
 *      strength = proc->field08
 *
 *      if (proc->field64 != 0) {
 *          StartProcAt(proc->field64, obj->field38)
 *          slave = proc->field64
 *          obj->field1c = slave->field00                ; its proc
 *          slave->field08->field30 &= ~MK3F_INVISO
 *      } else {
 *          slave = getprc_x(obj, 0)                     ; answers in 0x1c too
 *          if (slave != NULL) slave->field08->field2c = -1
 *          slave->field08->field30 &= ~MK3F_INVISO
 *      }
 *
 *      ((MK3OBJPROC *)obj->field1c)->him     = proc->him
 *      ((MK3OBJPROC *)obj->field1c)->field00 = proc->field00
 *      obj->field20   = 0
 *      proc->field84  = 0
 *      slave->thread->pid = strength + 0x700
 *
 * **The pid is computed, and this is the second formula for it.** mkprop.c's
 * `t_decoy_proc` sets `decoy->thread->pid = obj->field00->field08 + 0x204`; this
 * sets `slave->thread->pid = obj->field00->field08 + 0x700`. **Same base, two
 * per-kind constants** -- so a thread's pid is the owning fighter's strength
 * index plus a tag saying what kind of thing it is, and `FindThread` can find
 * "player 2's projectile" without a table.
 *
 * That makes three pid facts in the tree: 0x204 for decoys, 0x700 for
 * projectiles, and the literal 0x11f mkstat.c uses for Jade's flash, which
 * `is_jade_protected` searches for. The first two are derived and the third is
 * not, which is worth flagging before a port assumes one scheme.
 *
 * **The new proc inherits both halves of the fighter's view of the fight** --
 * `him` and `field00` -- so the projectile knows who it is aimed at from the
 * moment it exists, and `benedict_arnold_projectile` above is what un-does that.
 *
 * `proc->field84 = 0` clears the slot `tell_world_stk` publishes into, so a
 * fresh projectile starts with no published stick. Second writer of that field
 * and still no reader anywhere in the tree.
 *
 * **Two paths, and the second reuses an object rather than making one.** With a
 * slave already in `proc->field64` the routine restarts it in place through
 * `StartProcAt` with the handler in 0x38; otherwise `getprc_x` supplies one and
 * its part's 0x2c is set to -1, which no other routine in this file writes.
 * Both paths clear MK3F_INVISO, because a projectile that has been used before
 * was hidden rather than destroyed.
 *
 * **`getprc_x` answers in `obj->field1c` as well as in `r0`.** The code reads
 * 0x1c as the new proc four instructions after the call without writing it, so
 * the helper must fill it -- the same convention `mk_random` and every other
 * 0x1c-answering helper in this engine follows. Recorded as the reading; the
 * routine is at 0x000599b4 and is not decompiled.
 *
 * **It returns the slave, and this was first written as `void`.** The last
 * instruction before the epilogue is `mov r0, r1`, and `r1` is the slave object
 * on both paths. Nothing caught it: the two callers written at the time ignored
 * the result, so a `void` declaration compiled and ran. `t_rzap3` is what found
 * it -- that one does `ldr r3, [r0, #8]` on the way back, reading the slave's
 * part straight out of the return value.
 *
 * A return value nobody reads is invisible to the compiler, to `protos.py` and
 * to a differential test. The only thing that finds one is reading the caller.
 */
MK3OBJ *getprc_x(MK3OBJ *obj, uint32_t arg);
void StartProcAt(MK3OBJ *obj, MK3THREADFUNC func);

MK3OBJ *create_proj_proc(MK3OBJ *obj)
{
    MK3OBJPROC *proc = obj->field00;
    uint32_t    strength = proc->field08;
    MK3OBJ     *slave;

    obj->field20 = obj->field1c;

    if (proc->field64 != 0) {
        slave = (MK3OBJ *)(void *)(uintptr_t)proc->field64;

        StartProcAt(slave, (MK3THREADFUNC)(uintptr_t)obj->field38);

        slave = (MK3OBJ *)(void *)(uintptr_t)proc->field64;
        obj->field1c = (uint32_t)(uintptr_t)slave->field00;

        slave = (MK3OBJ *)(void *)(uintptr_t)proc->field64;
        slave->field08->field30 =
            slave->field08->field30 & ~(uint32_t)MK3F_INVISO;

    } else {
        slave = getprc_x(obj, 0);            /* answers in 0x1c as well */

        if (slave != NULL)
            slave->field08->field2c = 0xffffffffu;

        slave->field08->field30 =
            slave->field08->field30 & ~(uint32_t)MK3F_INVISO;
    }

    ((MK3OBJPROC *)(void *)(uintptr_t)obj->field1c)->him = proc->him;
    ((MK3OBJPROC *)(void *)(uintptr_t)obj->field1c)->field00 = proc->field00;

    obj->field20  = 0;
    proc->field84 = 0;

    slave->thread->pid = strength + 0x700;   /* the kind tag */

    return slave;                            /* `mov r0, r1` */
}


/* t_roc3 -- armv7 0x00077f00, 124 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      PUSH obj->field1c
 *      obj->field1c = 0xb; ochar_sound(obj)
 *      obj->field40 = 0; get_char_ani2(obj)
 *      POP  obj->field1c
 *      frame[frame].handler = t_mframew
 *
 * **Eighteenth argument-stack site, and the first in this file.** The caller's
 * 0x1c is the frame count `t_mframew` will use, and both `ochar_sound` and the
 * animation resolve want 0x1c for themselves -- so it goes on `args[]` and comes
 * back before the hand-over. One field, three users, in twenty-two instructions.
 *
 * The span is inside one state, so a register would have done. Both forms appear
 * in this file too -- `setup_proj_obj` and `tell_world_stk` use registers for the
 * same job -- which is the third file to show the choice is the compiler's and
 * not the code's.
 *
 * `obj->field40 = 0` comes out of the token register, which the dispatch has
 * already proved to be zero. Written as 0 because that is what it is.
 */
long t_mframew(MK3THREAD *thread);

long t_roc3(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t argc;

    if (*mk3_frame(thread, frame + 1) != 0)
        return -3;

    argc = thread->fieldf8;
    *mk3_arg(thread, argc) = obj->field1c;
    thread->fieldf8 = argc + 1;

    obj->field1c = 0xb;
    ochar_sound(obj);

    obj->field40 = 0;
    get_char_ani2(obj);

    argc = thread->fieldf8 - 1;
    thread->fieldf8 = argc;
    obj->field1c = *mk3_arg(thread, argc);

    mk3_frame(thread, frame)[1] = (uint32_t)(uintptr_t)t_mframew;
    *mk3_frame(thread, frame + 1) = 0;
    return 0;
}


/* tl_do_proj_sitting_duck -- armv7 0x00075698, 124 bytes.  **Complete.**
 *
 *      token == 0:        proc->field18 = 0x604
 *                         obj->field1c  = obj->field20
 *                         token := 0x1409, park obj->field1c
 *
 *      token == 0x1409:   detach_proj(obj)
 *                         pop a level, or t_local_reaction_exit at the bottom
 *
 *      otherwise:         return -3
 *
 * **A projectile that sits still for as long as the caller says and then lets
 * itself go.** The duration comes out of 0x20 rather than a literal, which makes
 * this the only park in the file whose length the caller chooses.
 *
 * 0x604 into `proc->field18` is the action `i_am_a_sitting_duck` announces
 * twelve hundred bytes earlier in this file -- second site for that number, and
 * it confirms 0x604 is the sitting-duck action rather than something that
 * routine invented.
 *
 * `detach_proj` clears `proc->slave` and `proc->field64` without killing
 * anything, so the projectile survives the thread that was flying it. First
 * caller for that routine.
 *
 * The park value is loaded from 0x1c twice, once for `fieldfc` and once for the
 * return, rather than kept in a register. Transcribed as two reads.
 */
long tl_do_proj_sitting_duck(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0) {
        obj->field00->field18 = 0x604;      /* the sitting-duck action */
        obj->field1c = obj->field20;

        *mk3_frame(thread, frame + 1) = 0x1409;
        thread->fieldfc = obj->field1c;
        return (long)obj->field1c;
    }

    if (token != 0x1409)
        return -3;

    detach_proj(obj);

    if ((long)thread->frame > 0) {           /* cmp #0 / ble: signed */
        thread->frame = thread->frame - 1;   /* back up a level */
        return 0;
    }

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* t_double_shaker -- armv7 0x00074ee4, 128 bytes.  **Complete.**
 *
 *      token == 0:       him->x12          += obj->field20
 *                        proc->field88->part->x12 += obj->field20
 *                        token := 0x354, park 2
 *
 *      token == 0x354:   pop a level, or t_local_reaction_exit at the bottom
 *
 *      otherwise:        return -3
 *
 * **"Double" means two bodies.** The same offset is added to the opponent's y
 * and to the y of whatever `proc->field88` points at, so both jump together and
 * two frames later the routine gives the level back.
 *
 * **`proc->field88` is new and nothing in the tree writes it.** It is
 * dereferenced twice here -- once for the object, once for its part -- so it is
 * an object pointer, and it is added to the header on that authority with the
 * gap said out loud. Whoever finds the writer should say so in the header.
 *
 * The opponent is reached as `proc->him` and the second body through `field88`,
 * which are two different words of the same structure holding two different
 * objects -- so this routine is the reason to believe 0x88 is not just another
 * name for `him`.
 *
 * Both adds are unsigned halfword arithmetic, `ldrh` / `add` / `strh`, the same
 * as `t_target`'s placement. Fourth site siding with the unsigned reading.
 */
long t_double_shaker(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);
    MK3OBJ  *him, *other;

    if (token == 0) {
        him = (MK3OBJ *)(void *)(uintptr_t)obj->field00->him;
        MK3_SET_FIELD12(him, (uint32_t)MK3_FIELD12(him) + obj->field20);

        other = obj->field00->field88;
        MK3_SET_FIELD12(other->field08,
                        obj->field20
                        + (uint32_t)MK3_FIELD12(other->field08));

        *mk3_frame(thread, frame + 1) = 0x354;
        thread->fieldfc = 2;
        return 2;
    }

    if (token != 0x354)
        return -3;

    if ((long)thread->frame > 0) {           /* cmp #0 / ble: signed */
        thread->frame = thread->frame - 1;   /* back up a level */
        return 0;
    }

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* t_summon_spawn -- armv7 0x00077aa0, 128 bytes.  **Complete.**
 *
 *      token == 0:       NewThread(obj, t_summon_proc)
 *                        token := 0xb6a, park 0x12
 *
 *      token == 0xb6a:   obj->field48 += obj->a10
 *                        pop a level, or t_local_reaction_exit at the bottom
 *
 *      otherwise:        return -3
 *
 * **Start the summon, wait eighteen frames, advance a counter, hand back.** The
 * accumulate is `0x48 += a10`, so the caller supplies both the running total and
 * the step -- a second reading of `a10` as a plain increment rather than an
 * argument or a pointer.
 *
 * `NewThread` and not `NewThreadProc`, so the return value is discarded and the
 * summoned thread is on its own from the first frame.
 */
MK3THREAD *NewThread(void *owner, MK3THREADFUNC func);
long t_summon_proc(MK3THREAD *thread);

long t_summon_spawn(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0) {
        NewThread(obj, (MK3THREADFUNC)t_summon_proc);

        *mk3_frame(thread, frame + 1) = 0xb6a;
        thread->fieldfc = 0x12;
        return 0x12;
    }

    if (token != 0xb6a)
        return -3;

    obj->field48 = obj->field48 + obj->a10;

    if ((long)thread->frame > 0) {           /* cmp #0 / ble: signed */
        thread->frame = thread->frame - 1;   /* back up a level */
        return 0;
    }

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* --------------------------------------------------------------------- t_summon_proc
 *
 * armv7 0x00078ed8, 388 bytes.  **Complete.**
 *
 * `t_summon_spawn`'s own `NewThread` target -- a genuinely separate thread,
 * not a descent, so `field48`'s value has to cross into it on the
 * arg-stack the same way every other spawn site in this file does: pushed
 * before `get_char_ani2`, restored right after (into `field08`'s own
 * `field0c`/`field10` high halves, `MK3_SET_FIELD0E`/`12`, the second one
 * from `G+0xac - 0x10` rather than the restored value).
 *
 * `0xb26` is the launch: `NewThreadProc(obj, t_summon_flame_animator)` --
 * a SECOND independent thread, this one whose return value nobody even
 * looks at -- poses animation `0x1a`, calls `find_part2` twice, tags the
 * opponent's action `0xf` (`field00->field18`), packs `field08->field1c`
 * with `0xfff80000`, and clears the phase flag `field00->field28` before
 * falling straight into `0xb4c`'s own watch loop.
 *
 * `0xb4c` reads that phase flag: zero means check whether it's time to
 * strike (`q_is_he_a_boss` first -- a boss always skips the box test and
 * falls through as a miss -- then `strike_check_a0`, which on a hit sets
 * the flag to `1`), and either way joins the SAME watch code a nonzero flag
 * runs directly: `next_anirate`, `lowest_mpart_ob`, and a floor test
 * (`field20 <= -0xc8`) that self-arms another tick while the summon is
 * still above ground and installs `tl_delete_proj_and_die` once it isn't.
 */
long t_summon_flame_animator(struct MK3THREAD *thread);
void q_is_he_a_boss(MK3OBJ *obj);
long strike_check_a0(MK3OBJ *obj);
void lowest_mpart_ob(MK3OBJ *out, MK3OBJ *src);

long t_summon_proc(MK3THREAD *thread)
{
    MK3OBJ   *obj   = (MK3OBJ *)thread->proc;
    uint32_t *args  = (uint32_t *)(void *)thread->args;
    uint32_t  frame = thread->frame;
    uint32_t  slot  = *mk3_frame(thread, frame + 1);

    if (slot == 0xb26) {
        NewThreadProc(obj, (MK3THREADFUNC)t_summon_flame_animator);

        obj->field40 = 0x1a;
        get_char_ani2(obj);

        find_part2(obj);
        find_part2(obj);

        obj->field00->field18 = 0xf;
        obj->field08->field1c = 0xfff80000;

        obj->field1c = 4;
        init_anirate(obj);

        obj->field1c = 0;
        obj->field00->field28 = 0;

        *mk3_frame(thread, frame + 1) = 0xb4c;
        thread->fieldfc = 1;
        return 1;
    }

    if (slot == 0xb4c) {
        obj->field1c = obj->field00->field28;

        if (obj->field00->field28 == 0) {
            q_is_he_a_boss(obj);
            if (obj->field5c == 0) {
                obj->field1c = 0x12;
                strike_check_a0(obj);

                if (obj->field5c != 0) {
                    obj->field1c = 1;
                    obj->field00->field28 = 1;
                }
            }
        }

        next_anirate(obj);
        lowest_mpart_ob(obj, obj->field08);

        obj->field1c = (uint32_t)~0xc7;

        if ((int32_t)obj->field20 <= (int32_t)~0xc7) {
            *mk3_frame(thread, frame + 1) = 0xb4c;
            thread->fieldfc = 1;
            return 1;
        }

        return mk3_install(thread, (MK3THREADFUNC)tl_delete_proj_and_die);
    }

    if (slot != 0)
        return -3;

    args[thread->fieldf8] = obj->field48;
    thread->fieldf8 = thread->fieldf8 + 1;

    obj->field40 = 0x1a;
    get_char_ani2(obj);

    obj->field48 = obj->field40;

    thread->fieldf8 = thread->fieldf8 - 1;
    obj->field48 = args[thread->fieldf8];

    MK3_SET_FIELD0E(obj->field08, obj->field48);
    MK3_SET_FIELD12(obj->field08, *(uint32_t *)(G_BYTES + 0xac) - 0x10);

    obj->field1c = 4;

    *mk3_frame(thread, frame + 1) = 0xb26;
    thread->frame = thread->frame + 1;   /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_mframew;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* -------------------------------------------------------- t_master_summon_proc
 *
 * armv7 0x00077934, 268 bytes.  **Complete.**
 *
 * Three `t_summon_spawn` calls back to back, states 0xb7a/0xb7b/0xb7c each
 * pushing the next, then a self-resume with no push at all -- `0xb7c` parks
 * `0xb7d` and waits `0x16462` ticks, the same "wait ninety-one thousand and
 * never answer" ending `t_friendship_speech` and `t_fx_babality` use, since
 * `0xb7d` matches none of this routine's own checks and the next call
 * answers -3. The free sets up the shared offset all three spawns add:
 * `field48` from the GrObj's signed `0x0e` halfword, `a10 = -0x60` and
 * `field34 = 0x11f`, both flipped to their positive twins when `is_he_right`
 * says no, then folded once into `field48` before the first spawn.
 *
 *      slot = frame[frame+1].w0
 *                                          ; 0:     free, then 0xb7a
 *                                          ; 0xb7a:  spawn 2, then 0xb7b
 *                                          ; 0xb7b:  spawn 3, then 0xb7c
 *                                          ; 0xb7c:  wait forever (0xb7d)
 *      if (slot == 0xb7a) {
 *          token 0xb7b ; frame++ ; install t_summon_spawn ; return 0
 *      }
 *      if (slot < 0xb7a) {
 *          if (slot != 0) return -3
 *          obj->field48 = (int16)MK3_FIELD0E(obj->field08)
 *          obj->a10 = -0x60 ; obj->field34 = 0x11f
 *          is_he_right(obj)
 *          if (obj->field5c == 0) { obj->a10 = -obj->a10 ; obj->field34 = -obj->field34 }
 *          obj->field48 = obj->field34 + obj->field48
 *          token 0xb7a ; frame++ ; install t_summon_spawn ; return 0
 *      }
 *      if (slot == 0xb7b) {
 *          token 0xb7c ; frame++ ; install t_summon_spawn ; return 0
 *      }
 *      if (slot != 0xb7c) return -3
 *      token 0xb7d ; fieldfc = 0x16462 ; return 0x16462
 */
long t_summon_spawn(struct MK3THREAD *thread);

long t_master_summon_proc(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t slot  = *mk3_frame(thread, frame + 1);

    if (slot == 0xb7a) {
        *mk3_frame(thread, frame + 1) = 0xb7b;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_summon_spawn;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot < 0xb7a) {
        if (slot != 0)
            return -3;

        obj->field48 = (uint32_t)(int32_t)MK3_FIELD0E_S(obj->field08);
        obj->a10     = (uint32_t)~0x5f;      /* -0x60 */
        obj->field34 = 0x11f;
        is_he_right(obj);

        if (obj->field5c == 0) {
            obj->a10     = (uint32_t)(-(int32_t)obj->a10);
            obj->field34 = (uint32_t)(-(int32_t)obj->field34);
        }
        obj->field48 = obj->field34 + obj->field48;

        *mk3_frame(thread, frame + 1) = 0xb7a;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_summon_spawn;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot == 0xb7b) {
        *mk3_frame(thread, frame + 1) = 0xb7c;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_summon_spawn;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot != 0xb7c)
        return -3;

    *mk3_frame(thread, frame + 1) = 0xb7d;
    thread->fieldfc = 0x16462;
    return 0x16462;
}


/* ------------------------------------------------------------------ tl_do_summon
 *
 * armv7 0x0007a1b0, 272 bytes.  **Complete.**
 *
 * `t_master_summon_proc`'s driver: the free packs `0x00030019` (rate 3,
 * animation 0x19) and descends into `t_animate2_a9` from `0xb87`. `0xb87`
 * is the actual summon -- the sound, then `NewThread(obj,
 * t_master_summon_proc)`, spawning it as an independent thread rather than
 * pushing a level onto this one -- and waits on `t_mframew` from `0xb90`.
 * `0xb90` is a bare wait, no push, sixteen ticks for `0xb91`, whose own
 * re-entry just installs `t_mframew` again.
 *
 *      slot = frame[frame+1].w0
 *                                          ; 0:    free, then 0xb87
 *                                          ; 0xb87: summon, then 0xb90
 *                                          ; 0xb90: wait, then 0xb91
 *                                          ; 0xb91: install t_mframew
 *      if (slot == 0xb87) {
 *          obj->field1c = 1 ; ochar_sound(obj)
 *          NewThread(obj, t_master_summon_proc)
 *          obj->field1c = 3
 *          token 0xb90 ; frame++ ; install t_mframew ; return 0
 *      }
 *      if (slot < 0xb87) {
 *          if (slot != 0) return -3
 *          obj->field20 = 0xf ; obj->a10 = 0 ; zap_init_special_act(obj)
 *          obj->field40 = 0x00030019
 *          token 0xb87 ; frame++ ; install t_animate2_a9 ; return 0
 *      }
 *      if (slot == 0xb90) { token 0xb91 ; fieldfc = 0x10 ; return 0x10 }
 *      if (slot != 0xb91) return -3
 *      obj->field1c = 2 ; install t_mframew ; return 0
 */
MK3THREAD *NewThread(void *owner, MK3THREADFUNC func);
long t_master_summon_proc(struct MK3THREAD *thread);
long t_animate2_a9(struct MK3THREAD *thread);            /* pointer slot 0x000f36c0 */

long tl_do_summon(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t slot  = *mk3_frame(thread, frame + 1);

    if (slot == 0xb87) {
        obj->field1c = 1;
        ochar_sound(obj);

        NewThread(obj, (MK3THREADFUNC)t_master_summon_proc);

        obj->field1c = 3;

        *mk3_frame(thread, frame + 1) = 0xb90;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot < 0xb87) {
        if (slot != 0)
            return -3;

        obj->field20 = 0xf;
        obj->a10     = 0;
        zap_init_special_act(obj);

        obj->field40 = 0x00030019;

        *mk3_frame(thread, frame + 1) = 0xb87;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_animate2_a9;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot == 0xb90) {
        *mk3_frame(thread, frame + 1) = 0xb91;
        thread->fieldfc = 0x10;
        return 0x10;
    }

    if (slot != 0xb91)
        return -3;

    obj->field1c = 2;
    return mk3_install(thread, (MK3THREADFUNC)t_mframew);
}


/* t_sz_post_zap -- armv7 0x0007b990, 128 bytes.  **Complete.**
 *
 *      token == 0:        token := 0x87d, park 0x10
 *
 *      token == 0x87d:    obj->field40 = 0x24
 *                         obj->field54 = 4
 *                         find_ani_part_a14(obj)
 *                         obj->field1c = part->field24
 *                         if (part->field24 == 0x15) {
 *                             obj->field40 = 0x24 - 4  = 0x11
 *                             obj->field54 = 0x11 - 0xe = 3
 *                             find_ani2_part_a14(obj)
 *                         }
 *                         obj->field1c = 4
 *                         frame[frame].handler = t_mframew
 *
 *      otherwise:         return -3
 *
 * **A seventh hard-coded character number, and this one is anonymous.**
 * Character 0x15 gets a different animation resolved -- `find_ani2_part_a14` with
 * 0x11 and 3 instead of `find_ani_part_a14` with 0x24 and 4 -- and nothing in the
 * routine's name says a fighter is being singled out. **It belongs with the six
 * in issue #29**, not with the named predicates.
 *
 * Note the first lookup runs unconditionally and is then thrown away for that one
 * character, so the exception costs a wasted call rather than being written as a
 * choice. That is what makes it easy to miss.
 *
 * The three constants come off one register: 0x24, then `subs #4` for 0x11, then
 * `subs #0xe` for 3. Three values, one literal, and the shared-literal habit is
 * what makes 0x11 and 3 look unrelated to 0x24 in the disassembly.
 *
 * `obj->field54` is the a14 finders' second parameter -- third site, after
 * `t_sw_plant_bomb` and `t_jade_shaker` in mkfatal.c, and the first where two
 * different finders are given two different values for it.
 */
void find_ani_part_a14(MK3OBJ *obj);
void find_ani2_part_a14(MK3OBJ *obj);

long t_sz_post_zap(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0) {
        *mk3_frame(thread, frame + 1) = 0x87d;
        thread->fieldfc = 0x10;
        return 0x10;
    }

    if (token != 0x87d)
        return -3;

    obj->field40 = 0x24;
    obj->field54 = 4;
    find_ani_part_a14(obj);

    obj->field1c = obj->field08->field24;

    if (obj->field1c == 0x15) {              /* one character, unnamed */
        /* r3 holds the character (0x15) here, not the 0x24 from above:
         * `subs r3, #4` / `subs r3, #0xe` (difftest) */
        obj->field40 = 0x15 - 4;
        obj->field54 = 0x15 - 4 - 0xe;       /* the same register */
        find_ani2_part_a14(obj);
    }

    obj->field1c = 4;

    return mk3_install(thread, (MK3THREADFUNC)t_mframew);
}


/* ------------------------------------------------------------ t_ice_collision_check
 *
 * armv7 0x0007bea8, 344 bytes.  **Complete.**
 *
 * Not a two-tick lookahead like `t_lk_prezap`/`t_spit_prezap` despite the
 * shape -- the save/restore around `do_next_a9_frame_pxob` happens WITHIN
 * the same call, protecting `field20`/`field24` from whatever that routine
 * does to them, not carrying them across a wait. The strike check runs
 * immediately after the restore, in the very first invocation. A miss
 * parks `0x83c` and waits three ticks purely to give the caller somewhere
 * to unwind to -- `0x83c`'s own re-entry does nothing but pop a level (or
 * install `t_local_reaction_exit` at the bottom); the actual check already
 * happened. A hit pops a level when it can and, either way, plays a sound
 * (`field1c = 0x40003`), launches `t_sz_zap_hit` through `create_proj_proc`,
 * and installs `t_sz_post_zap`.
 *
 *      slot = frame[frame+1].w0
 *      if (slot != 0) {
 *          if (slot != 0x83c) return -3
 *          pop a level, or t_local_reaction_exit at the bottom
 *      }
 *      args[fieldf8] = obj->field20 ; fieldf8++
 *      args[fieldf8] = obj->field24 ; fieldf8++
 *      do_next_a9_frame_pxob(obj, obj, proc->slave)
 *      fieldf8-- ; obj->field24 = args[fieldf8]
 *      fieldf8-- ; obj->field20 = args[fieldf8]
 *      obj->field1c = 0x13 ; local_strike_check_box(obj)
 *      if (obj->field5c != 0) {
 *          if (frame > 0) frame -= 1
 *          obj->field1c = 0x40003 ; hob_ochar_sound(obj)
 *          obj->field38 = t_sz_zap_hit ; create_proj_proc(obj)
 *          install t_sz_post_zap ; return 0
 *      }
 *      token 0x83c ; fieldfc = 3 ; return 3
 */
long do_next_a9_frame_pxob(MK3OBJ *obj, MK3OBJ *a, MK3OBJ *b);
void local_strike_check_box(MK3OBJ *obj);
void hob_ochar_sound(MK3OBJ *obj);
MK3OBJ *create_proj_proc(MK3OBJ *obj);
long t_sz_zap_hit(struct MK3THREAD *thread);
long t_sz_post_zap(struct MK3THREAD *thread);

long t_ice_collision_check(MK3THREAD *thread)
{
    MK3OBJ   *obj  = (MK3OBJ *)thread->proc;
    uint32_t *args = (uint32_t *)(void *)thread->args;
    uint32_t  frame = thread->frame;
    uint32_t  slot  = *mk3_frame(thread, frame + 1);

    if (slot != 0) {
        if (slot != 0x83c)
            return -3;

        if ((long)frame > 0) {
            thread->frame = frame - 1;
            return 0;
        }
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }

    args[thread->fieldf8] = obj->field20;
    thread->fieldf8 = thread->fieldf8 + 1;
    args[thread->fieldf8] = obj->field24;
    thread->fieldf8 = thread->fieldf8 + 1;

    do_next_a9_frame_pxob(obj, obj, (MK3OBJ *)(uintptr_t)obj->field00->slave);

    thread->fieldf8 = thread->fieldf8 - 1;
    obj->field24 = args[thread->fieldf8];
    thread->fieldf8 = thread->fieldf8 - 1;
    obj->field20 = args[thread->fieldf8];

    obj->field1c = 0x13;
    local_strike_check_box(obj);

    if (obj->field5c != 0) {
        if ((long)frame > 0)
            thread->frame = frame - 1;

        obj->field1c = 0x40003;
        hob_ochar_sound(obj);

        obj->field38 = (uint32_t)(uintptr_t)t_sz_zap_hit;
        create_proj_proc(obj);

        return mk3_install(thread, (MK3THREADFUNC)t_sz_post_zap);
    }

    *mk3_frame(thread, frame + 1) = 0x83c;
    thread->fieldfc = 3;
    return 3;
}


/* --------------------------------------------------------------- t_osz_forward_entry
 *
 * armv7 0x00075ae8, 400 bytes.  **Complete.**
 *
 * `tl_do_sz_zap`'s own descent target, and a chain that runs
 * `t_ice_collision_check` up to four times over, repacking `field20`/
 * `field24` with a different literal pair each pass: free (`0x868` next),
 * `0x868` (`0x86b` next), `0x86b` (`0x86e` next), and `0x86e`, which loops
 * BACK to `0x86b` -- the free's and `0x868`'s literal pairs are identical
 * (`0x2d0060`/`0x100028`), so the first two passes are the same box read
 * twice under two different physical push sites.
 *
 * `0x871` is not reached from inside this chain at all -- nothing here ever
 * plants it, so it can only be an external entry, some other caller pushing
 * straight into the tracking loop rather than starting from the free.
 *
 * `0x874` is the actual launch, and unlike every other launcher in this
 * file it does not go through `field38` alone: `field00->field64->field40`
 * (the slave's own animation cursor) is set directly to
 * `&sz_ani_data[0x1234]`, the same "reach into the slave from outside" shape
 * `tl_do_sz_zap` itself already uses at a different offset into the same
 * table. `field38 = t_sz_zap_proc` still gets set the ordinary way for the
 * hit callback, `create_proj_proc` spawns it, and this branch installs
 * `t_sz_post_zap` directly -- no push, since the chain is over.
 *
 * All four track-and-wait pushes end on one physical "install and clear the
 * token" tail; only the launch (`0x874`) skips it, since it never pushes a
 * level to begin with.
 */
long t_ice_collision_check(struct MK3THREAD *thread);
long t_sz_post_zap(struct MK3THREAD *thread);
long t_sz_zap_proc(MK3THREAD *thread);
extern uint8_t sz_ani_data[];             /* 0x00162914, slot 0x000f33d8 */

long t_osz_forward_entry(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t slot  = *mk3_frame(thread, frame + 1);
    uint32_t next_handler;

    if (slot == 0x86b) {
        obj->field20 = 0x2d0088;
        obj->field24 = 0x100050;

        *mk3_frame(thread, frame + 1) = 0x86e;
        thread->frame = thread->frame + 1;   /* push a level */
        next_handler = (uint32_t)(uintptr_t)t_ice_collision_check;
        goto install_shared;
    }

    if (slot == 0x868) {
        obj->field20 = 0x2d0060;
        obj->field24 = 0x100028;

        *mk3_frame(thread, frame + 1) = 0x86b;
        thread->frame = thread->frame + 1;   /* push a level */
        next_handler = (uint32_t)(uintptr_t)t_ice_collision_check;
        goto install_shared;
    }

    if (slot == 0x871) {
        obj->field20 = 0x2d00b5;
        obj->field24 = 0x100066;

        *mk3_frame(thread, frame + 1) = 0x874;
        thread->frame = thread->frame + 1;   /* push a level */
        next_handler = (uint32_t)(uintptr_t)t_ice_collision_check;
        goto install_shared;
    }

    if (slot == 0x874) {
        ((MK3OBJ *)(void *)(uintptr_t)obj->field00->field64)->field40 =
            (uint32_t)(uintptr_t)&sz_ani_data[0x1234];

        obj->field38 = (uint32_t)(uintptr_t)t_sz_zap_proc;
        create_proj_proc(obj);

        return mk3_install(thread, (MK3THREADFUNC)t_sz_post_zap);
    }

    if (slot == 0x86e) {
        obj->field20 = 0x2d00a4;
        obj->field24 = 0x10006c;

        *mk3_frame(thread, frame + 1) = 0x871;   /* r1, re-loaded with 0x871 by the dispatch (difftest) */
        thread->frame = thread->frame + 1;   /* push a level */
        next_handler = (uint32_t)(uintptr_t)t_ice_collision_check;
        goto install_shared;
    }

    if (slot != 0)
        return -3;

    obj->field20 = 0x2d0060;
    obj->field24 = 0x100028;

    *mk3_frame(thread, frame + 1) = 0x868;
    thread->frame = thread->frame + 1;   /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_ice_collision_check;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;

install_shared:
    mk3_frame(thread, thread->frame)[1] = next_handler;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------------------- t_sky_ice_proc
 *
 * armv7 0x000776b0, 356 bytes.  **Complete.**
 *
 * A falling icicle. The free lines up under the opponent
 * (`field0e = MK3_FIELD0E(him) + field48`) and drops the GrObj's own
 * `0x12` to `G[0x464] - 0x1b0` -- the top of the screen, offset by the
 * icicle's own height -- then waits eight ticks (no push) for `0x11df`.
 * `0x11df` starts the actual fall: a `0x100` nudge on the GrObj's own
 * `0x12`, a flat `0xb0000` fall rate into both the object and its GrObj,
 * and a target floor (`a10 = G[0xac] - 0xc0`) before a one-tick wait for
 * `0x11ee`. `0x11ee` is the loop: `proj_strike_check` every tick, and
 * while it keeps missing and the GrObj's `0x12` is still above the floor,
 * it re-parks itself and waits one more tick -- the same token, so this
 * is a genuine loop rather than a chain of distinct states. A hit or
 * reaching the floor both fall into the same close: `field1c = 3` and a
 * pushed `t_mframew` wait from `0x1208`, whose own re-entry installs
 * `tl_delete_proj_and_die`.
 *
 *      slot = frame[frame+1].w0
 *                                          ; 0:     free, then 0x11df (wait)
 *                                          ; 0x11df: start fall, then 0x11ee
 *                                          ; 0x11ee: strike-check loop, self or 0x1208
 *                                          ; 0x1208: install tl_delete_proj_and_die
 *      if (slot == 0x11df) {
 *          obj->field1c = 1 ; ochar_sound(obj)
 *          GrObj->0x12 += 0x100
 *          obj->field1c = GrObj->field1c = 0xb0000
 *          obj->a10 = G[0xac] - 0xc0
 *          repark: token 0x11ee ; fieldfc = 1 ; return 1
 *      }
 *      if (slot < 0x11df) {
 *          if (slot != 0) return -3
 *          find_part2(obj)
 *          obj->field1c = MK3_FIELD0E(him) + obj->field48
 *          GrObj->0x0e = obj->field1c
 *          do_next_a9_frame(obj)
 *          obj->field1c = G[0x464] - 0x1b0
 *          GrObj->0x12 = obj->field1c
 *          token 0x11df ; fieldfc = 8 ; return 8
 *      }
 *      if (slot == 0x11ee) {
 *          obj->field1c = 0x12 ; proj_strike_check(obj)
 *          if (obj->field5c != 0) {
 *              obj->field1c = 0x20003 ; hob_ochar_sound(obj)
 *              obj->field1c = GrObj->field1c = 0x40000
 *              goto land
 *          }
 *          obj->field1c = (int16)GrObj->0x12
 *          if (obj->field1c < obj->a10) goto repark
 *          stop_a8(GrObj) ; GrObj->0x12 = (uint16)obj->a10
 *          land: obj->field1c = 3
 *          token 0x1208 ; frame++ ; install t_mframew ; return 0
 *      }
 *      if (slot != 0x1208) return -3
 *      install tl_delete_proj_and_die ; return 0
 */
void proj_strike_check(MK3OBJ *obj);
void stop_a8(MK3OBJ *part);
void find_part2(MK3OBJ *obj);

long t_sky_ice_proc(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t slot  = *mk3_frame(thread, frame + 1);

    if (slot == 0x11df) {
        obj->field1c = 1;
        ochar_sound(obj);

        MK3_SET_FIELD12(obj->field08,
            (uint16_t)(MK3_FIELD12_S(obj->field08) + 0x100));

        obj->field1c = 0xb0000;
        obj->field08->field1c = 0xb0000;

        obj->a10 = (uint32_t)(*(int32_t *)((char *)G + 0xac) - 0xc0);

repark:
        *mk3_frame(thread, frame + 1) = 0x11ee;
        thread->fieldfc = 1;
        return 1;
    }

    if (slot < 0x11df) {
        if (slot != 0)
            return -3;

        find_part2(obj);

        obj->field1c = (uint32_t)(int32_t)
            MK3_FIELD0E_S((MK3OBJ *)(uintptr_t)obj->field00->him) +
            obj->field48;
        MK3_SET_FIELD0E(obj->field08, obj->field1c);

        do_next_a9_frame(obj);

        obj->field1c = (uint32_t)(*(int32_t *)((char *)G + 0x464) - 0x1b0);
        MK3_SET_FIELD12(obj->field08, obj->field1c);

        *mk3_frame(thread, frame + 1) = 0x11df;
        thread->fieldfc = 8;
        return 8;
    }

    if (slot == 0x11ee) {
        int32_t y;

        obj->field1c = 0x12;
        proj_strike_check(obj);

        if (obj->field5c != 0) {
            obj->field1c = 0x20003;
            hob_ochar_sound(obj);

            obj->field08->field1c = 0x40000;
            obj->field1c = 0x40000;
            goto land;
        }

        y = MK3_FIELD12_S(obj->field08);
        obj->field1c = (uint32_t)y;
        if (y < (int32_t)obj->a10)
            goto repark;

        stop_a8(obj->field08);
        MK3_SET_FIELD12(obj->field08, (uint16_t)obj->a10);

land:
        obj->field1c = 3;

        *mk3_frame(thread, frame + 1) = 0x1208;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot != 0x1208)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)tl_delete_proj_and_die);
}


/* ------------------------------------------------------------------ tl_do_jax_zap2
 *
 * armv7 0x00079818, 296 bytes.  **Complete.**
 *
 * The free runs `zap_init_special_act` and descends into `tl_jax_zap_jsrp`
 * from `0x1277`. `0x1277` steps a frame and waits six ticks (no push) for
 * `0x127a`, the launch: `field38 = t_jax_zap_proc` into `create_proj_proc`,
 * and when a slave actually exists its own proc's `field34` is tagged `1`.
 * `away_x_vel` and a step follow before another bare wait, thirty-two
 * ticks, for `0x1289` -- which stops the thrower (`stop_me_player`) and
 * installs `t_mframew`.
 *
 *      slot = frame[frame+1].w0
 *                                          ; 0:     free, then 0x1277
 *                                          ; 0x1277: wait, then 0x127a
 *                                          ; 0x127a: launch, then 0x1289 (wait)
 *                                          ; 0x1289: install t_mframew
 *      if (slot == 0x1277) {
 *          do_next_a9_frame(obj)
 *          token 0x127a ; fieldfc = 6 ; return 6
 *      }
 *      if (slot < 0x1277) {
 *          if (slot != 0) return -3
 *          obj->field20 = 4 ; obj->a10 = 0 ; zap_init_special_act(obj)
 *          token 0x1277 ; frame++ ; install tl_jax_zap_jsrp ; return 0
 *      }
 *      if (slot == 0x127a) {
 *          obj->field1c = 0 ; ochar_sound(obj)
 *          obj->field38 = t_jax_zap_proc ; slave = create_proj_proc(obj)
 *          obj->field20 = 1
 *          if (slave != NULL) slave->field00->field34 = 1
 *          obj->field1c = 0x40000 ; away_x_vel(obj) ; do_next_a9_frame(obj)
 *          token 0x1289 ; fieldfc = 0x20 ; return 0x20
 *      }
 *      if (slot != 0x1289) return -3
 *      obj->field1c = G + 0x410 ; update_tsl(obj)
 *      stop_me_player(obj) ; obj->field1c = 7
 *      install t_mframew ; return 0
 */
void stop_me_player(MK3OBJ *obj);
void away_x_vel(MK3OBJ *obj);
long tl_jax_zap_jsrp(struct MK3THREAD *thread);
long t_jax_zap_proc(struct MK3THREAD *thread);

long tl_do_jax_zap2(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t slot  = *mk3_frame(thread, frame + 1);

    if (slot == 0x1277) {
        do_next_a9_frame(obj);

        *mk3_frame(thread, frame + 1) = 0x127a;
        thread->fieldfc = 6;
        return 6;
    }

    if (slot < 0x1277) {
        if (slot != 0)
            return -3;

        obj->field20 = 4;
        obj->a10     = 0;
        zap_init_special_act(obj);

        *mk3_frame(thread, frame + 1) = 0x1277;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)tl_jax_zap_jsrp;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot == 0x127a) {
        MK3OBJ *slave;

        obj->field1c = 0;
        ochar_sound(obj);

        obj->field38 = (uint32_t)(uintptr_t)t_jax_zap_proc;
        slave = create_proj_proc(obj);

        obj->field20 = 1;
        if (slave != NULL)
            slave->field00->field34 = 1;

        obj->field1c = 0x40000;
        away_x_vel(obj);
        do_next_a9_frame(obj);

        *mk3_frame(thread, frame + 1) = 0x1289;
        thread->fieldfc = 0x20;
        return 0x20;
    }

    if (slot != 0x1289)
        return -3;

    obj->field1c = (uint32_t)(uintptr_t)((char *)G + 0x410);
    update_tsl(obj);

    stop_me_player(obj);
    obj->field1c = 7;

    return mk3_install(thread, (MK3THREADFUNC)t_mframew);
}


/* --------------------------------------------------------------------- tl_jax_zap_jsrp
 *
 * armv7 0x000772f8, 272 bytes.  **Complete.**
 *
 * `tl_do_jax_zap2`'s own descent target, and a four-stage chain of its
 * own: the free poses animation 0x24 and waits six ticks under `0x125b`
 * (no push); `0x125b` steps a frame and waits four more under `0x1260`;
 * `0x1260` is the actual launch -- `field38 = t_jax_zap_proc` into
 * `create_proj_proc`, `slave->field00->field34` cleared when a slave
 * exists, `away_x_vel` -- and pushes `t_mframew` under `0x126c`.
 *
 * `0x126c` is the ordinary tail every multi-level descent in this file
 * ends on: `stop_me_player`, then pop a level, or install
 * `t_local_reaction_exit` at the bottom.
 */
long t_jax_zap_proc(struct MK3THREAD *thread);
void do_first_a9_frame(MK3OBJ *obj);

long tl_jax_zap_jsrp(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t slot  = *mk3_frame(thread, frame + 1);

    if (slot == 0x125b) {
        obj->field1c = 0;
        ochar_sound(obj);

        do_next_a9_frame(obj);

        *mk3_frame(thread, frame + 1) = 0x1260;
        thread->fieldfc = 4;
        return 4;
    }

    if (slot < 0x125b) {
        if (slot != 0)
            return -3;

        obj->field40 = 0x24;
        do_first_a9_frame(obj);

        *mk3_frame(thread, frame + 1) = 0x125b;
        thread->fieldfc = 6;
        return 6;
    }

    if (slot == 0x1260) {
        MK3OBJ *slave;

        obj->field38 = (uint32_t)(uintptr_t)t_jax_zap_proc;
        slave = create_proj_proc(obj);

        obj->field20 = 0;
        if (slave != NULL)
            slave->field00->field34 = 0;

        obj->field1c = 0x40000;
        away_x_vel(obj);

        obj->field1c = 4;

        *mk3_frame(thread, frame + 1) = 0x126c;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot != 0x126c)
        return -3;

    stop_me_player(obj);

    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;   /* back up a level */
        return 0;
    }

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* --------------------------------------------------------------------- t_jax_zap_proc
 *
 * armv7 0x0007753c, 372 bytes.  **Complete.**
 *
 * `tl_do_jax_zap2`'s own `field38` callback. The free poses animation
 * `0x3f`, steps a frame, tags `field1c=0x13`, `tell_world_stk`s, and tries
 * `strike_check_a0`; a hit falls straight into `0x1233`'s own impact code,
 * a miss re-arms itself (token `0x1229`, self) and waits three ticks for
 * another attempt.
 *
 * `0x1229` is the launch: `field00->field38 = 3` (the same "counting 3,
 * 2, 1 between effects" slot `t_jax_proj_calla` reads, per the header's
 * own note on it), throws (`0x80000`/`4`, `set_proj_vel`), tags
 * `field48=0x12` and `field34 = t_jax_proj_calla` (the per-frame
 * callback `t_jax_proj_calla` itself is installed under), plants token
 * `0x1233` for after it pops, and pushes `tl_projectile_flight_call`.
 *
 * `0x1233` is a fork on the GrObj's own `field18`: still zero shakes
 * (`field48 = 0x50005`, `shake_a11`) and jumps back into the "already
 * turned" branch below; already turned plays `hob_ochar_sound`,
 * `stop_a8`s the part, lines it up on the opponent (`match_me_with_him`,
 * `multi_adjust_xy` at `field1c=-0xb3`), and checks `field00->field34` --
 * zero (`t_jax_proj_calla` never ran, or cleared it) falls straight into
 * the finish; nonzero repositions once more (`multi_adjust_xy` at
 * `field1c=-0x25`) and falls into the SAME finish, one physical block
 * taken two ways.
 *
 * The finish poses animation `0x3f` again, `field54=2`, and pushes
 * `t_mframew` under `0x1253`, which just installs `tl_delete_proj_and_die`.
 */
void hob_ochar_sound(MK3OBJ *obj);
void match_me_with_him(MK3OBJ *obj);
void shake_a11(MK3OBJ *obj);
long t_jax_proj_calla(MK3THREAD *thread);
long tl_projectile_flight_call(struct MK3THREAD *thread);

long t_jax_zap_proc(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t slot  = *mk3_frame(thread, frame + 1);

    if (slot == 0x1229) {
        obj->field00->field38 = 3;

        obj->field1c = 0x80000;
        obj->field20 = 4;
        set_proj_vel(obj);

        obj->field48 = 0x12;
        obj->field34 = (uint32_t)(uintptr_t)t_jax_proj_calla;

        *mk3_frame(thread, frame + 1) = 0x1233;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)tl_projectile_flight_call;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot == 0x1253)
        return mk3_install(thread, (MK3THREADFUNC)tl_delete_proj_and_die);

    if (slot == 0x1233) {
    impact:
        if (obj->field18 == 0) {
            obj->field48 = 0x50005;
            shake_a11(obj);
            goto turned;
        }

    turned:
        obj->field1c = 0x10002;
        hob_ochar_sound(obj);

        stop_a8(obj->field08);

        match_me_with_him(obj);

        obj->field20 = 0;
        obj->field1c = (uint32_t)~0xb2;
        multi_adjust_xy(obj);

        obj->field1c = obj->field00->field34;
        if (obj->field1c != 0) {
            obj->field1c = (uint32_t)~0x19;
            obj->field20 = (uint32_t)~0x19 - 0xb;
            multi_adjust_xy(obj);
        }

        obj->field40 = 0x3f;
        obj->field54 = 0x3f - 0x3d;
        find_ani_part_a14(obj);

        obj->field1c = 4;

        *mk3_frame(thread, frame + 1) = 0x1253;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot != 0)
        return -3;

    obj->field40 = 0x3f;
    get_char_ani(obj);

    do_next_a9_frame(obj);

    obj->field1c = 0x13;
    tell_world_stk(obj);

    strike_check_a0(obj);
    if (obj->field5c != 0)
        goto impact;

    *mk3_frame(thread, frame + 1) = 0x1229;
    thread->fieldfc = 3;
    return 3;
}


/* t_boomerang_call -- armv7 0x00074fa8, 136 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = proc->field3c
 *      if (obj->field1c == 0)
 *          pop a level, or t_local_reaction_exit at the bottom
 *      if (obj->field1c == 1)
 *          frame[frame].handler = t_boom_return_check
 *      obj->field20   = part->field1c + obj->field1c
 *      part->field1c  = obj->field20
 *      pop a level, or t_local_reaction_exit at the bottom
 *
 * **`proc->field3c` is a three-way mode, and the third way is the value
 * itself.** Zero does nothing, one hands the thread to `t_boom_return_check`,
 * and anything else IS the per-frame fall added to the y velocity. So the caller
 * steers the boomerang by writing one word, and two of the four billion possible
 * values mean something other than "fall this fast".
 *
 * That is a shape worth flagging for a port: a field that is a mode for two
 * values and data for the rest. Nothing in the tree writes 0x3c yet, so what
 * range the game actually uses is not known -- but 1 as a sentinel means a fall
 * of exactly one unit per frame cannot be expressed.
 *
 * The two "pop or install" tails are separate copies in the binary, one for the
 * zero case and one for the fall case, which is why the routine is 136 bytes.
 */
long t_boom_return_check(MK3THREAD *thread);

long t_boomerang_call(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;

    if (*mk3_frame(thread, frame + 1) != 0)
        return -3;

    obj->field1c = obj->field00->field3c;

    if (obj->field1c == 1)
        return mk3_install(thread, (MK3THREADFUNC)t_boom_return_check);

    if (obj->field1c != 0) {
        obj->field20          = obj->field08->field1c + obj->field1c;
        obj->field08->field1c = obj->field20;
    }

    if ((long)thread->frame > 0) {           /* cmp #0 / ble: signed */
        thread->frame = thread->frame - 1;   /* back up a level */
        return 0;
    }

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* ---------------------------------------------------------------- t_boomerang_proc
 *
 * armv7 0x000780e8, 324 bytes.  **Complete.**
 *
 * A genuine two-phase self-loop, the boomerang's out-and-back flight.
 * `0x559` (outbound) and `0x56a` (inbound) are each `proj_onscreen_test`
 * followed by either `tl_delete_proj_and_die` (off-screen) or
 * `next_anirate` and reparking the SAME token for one more tick
 * on-screen -- the shape `t_sky_ice_proc` uses with one loop token, run
 * here with two. `0x54e` is the outbound watch's own entry and the turn:
 * while `field18` (the "time to come back" flag) is still zero it just
 * falls into the outbound loop's on-screen tail; once it isn't,
 * `init_anirate`, a reversed velocity (`field20 = GrObj->field1c =
 * 0xfffd0000`, `field18` negated) and a jump straight into the INBOUND
 * loop's on-screen tail -- no `proj_onscreen_test` on that first reversed
 * tick.
 *
 *      slot = frame[frame+1].w0
 *                                          ; 0:     free, then 0x54e
 *                                          ; 0x54e:  outbound watch / turn
 *                                          ; 0x559:  outbound self-loop
 *                                          ; 0x56a:  inbound self-loop
 *      if (slot == 0x54e) {
 *          if (obj->field18 != 0) {
 *              obj->field1c = 2 ; init_anirate(obj)
 *              obj->field20 = GrObj->field1c = 0xfffd0000
 *              obj->field1c = GrObj->field18 = -GrObj->field18
 *              goto watch_back
 *          }
 *          goto watch_out
 *      }
 *      if (slot < 0x54e) {
 *          if (slot != 0) return -3
 *          proc->field2c = 2 ; obj->field1c = 2
 *          obj->field40 = 4 ; get_char_ani2(obj)
 *          obj->field1c = 0x90000 ; obj->field20 = 2 ; set_proj_vel(obj)
 *          obj->field48 = 0x14 ; obj->field34 = t_boomerang_call
 *          token 0x54e ; frame++ ; install tl_projectile_flight_call ; return 0
 *      }
 *      if (slot == 0x559) {
 *          proj_onscreen_test(obj)
 *          if (obj->field5c == 0) install tl_delete_proj_and_die ; return 0
 *          watch_out: next_anirate(obj)
 *          token 0x559 ; fieldfc = 1 ; return 1
 *      }
 *      if (slot != 0x56a) return -3
 *      proj_onscreen_test(obj)
 *      if (obj->field5c == 0) install tl_delete_proj_and_die ; return 0
 *      watch_back: next_anirate(obj)
 *      token 0x56a ; fieldfc = 1 ; return 1
 */
long t_boomerang_call(struct MK3THREAD *thread);

long t_boomerang_proc(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t slot  = *mk3_frame(thread, frame + 1);

    if (slot == 0x54e) {
        if (obj->field18 != 0) {
            obj->field1c = 2;
            init_anirate(obj);

            obj->field20 = 0xfffd0000;
            obj->field08->field1c = 0xfffd0000;

            obj->field1c = (uint32_t)(-(int32_t)obj->field08->field18);
            obj->field08->field18 = obj->field1c;
            goto watch_back;
        }
        goto watch_out;
    }

    if (slot < 0x54e) {
        if (slot != 0)
            return -3;

        obj->field00->field2c = 2;
        obj->field1c = 2;
        obj->field40 = 4;
        get_char_ani2(obj);

        obj->field1c = 0x90000;
        obj->field20 = 2;
        set_proj_vel(obj);

        obj->field48 = 0x14;
        obj->field34 = (uint32_t)(uintptr_t)t_boomerang_call;

        *mk3_frame(thread, frame + 1) = 0x54e;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)tl_projectile_flight_call;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot == 0x559) {
        proj_onscreen_test(obj);
        if (obj->field5c == 0)
            return mk3_install(thread, (MK3THREADFUNC)tl_delete_proj_and_die);

watch_out:
        next_anirate(obj);
        *mk3_frame(thread, frame + 1) = 0x559;
        thread->fieldfc = 1;
        return 1;
    }

    if (slot != 0x56a)
        return -3;

    proj_onscreen_test(obj);
    if (obj->field5c == 0)
        return mk3_install(thread, (MK3THREADFUNC)tl_delete_proj_and_die);

watch_back:
    next_anirate(obj);
    *mk3_frame(thread, frame + 1) = 0x56a;
    thread->fieldfc = 1;
    return 1;
}


/* -------------------------------------------------------------------------- tl_jzap3
 *
 * armv7 0x0007aafc, 348 bytes.  **Complete.**
 *
 * The free reads the GrObj's own `field28` bit `0x10` (the flip flag
 * `flip_multi`'s trio toggles) and nudges the GrObj's `field0e` by `0xa`
 * either way -- `+0xa` when the bit is set, `-0xa` when it isn't, a
 * facing-relative offset applied before either branch's own setup runs.
 * When the bit was set it also parks `field1c` into `field00->field3c`
 * (the mode `t_boomerang_call` reads), zeroes `a10`, sets `field20=0x20`
 * and runs `zap_init_special_act`; either way it then plays three sounds
 * (`0`, `3`, `4`), poses animation `2`, steps a frame, and waits six
 * ticks under `0x5ad`.
 *
 * `0x5ad` just poses `field1c=4` and pushes `t_mframew` under `0x5af`,
 * whose own re-entry is the launch: `field38 = t_boomerang_proc` (the
 * usual hit callback), `create_proj_proc` spawns it, and when a slave
 * comes back its `field00->field3c` is copied straight from this
 * object's own -- the mode set above survives into the boomerang's own
 * proc. `field1c` and `field00->field18` both get tagged `0x604` (the
 * announce-and-keep-a-copy convention this file uses everywhere), and it
 * waits 32 ticks under `0x5ca`.
 *
 * `0x5ca` poses nothing new and pushes `t_mframew` under `0x5cc`, which
 * is the ordinary tail every multi-level descent in this file ends on:
 * pop a level, or install `t_local_reaction_exit` at the bottom.
 */
long t_boomerang_proc(struct MK3THREAD *thread);

long tl_jzap3(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t slot  = *mk3_frame(thread, frame + 1);

    if (slot == 0x5af) {
        MK3OBJ *slave;

        obj->field38 = (uint32_t)(uintptr_t)t_boomerang_proc;
        slave = create_proj_proc(obj);

        if (slave != NULL)
            slave->field00->field3c = obj->field00->field3c;

        obj->field1c          = 0x604;
        obj->field00->field18 = 0x604;

        *mk3_frame(thread, frame + 1) = 0x5ca;
        thread->fieldfc = 0x20;
        return 0x20;
    }

    if (slot == 0x5ca) {
        obj->field1c = 3;

        *mk3_frame(thread, frame + 1) = 0x5cc;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot == 0x5cc) {
        if ((long)thread->frame > 0) {
            thread->frame = thread->frame - 1;   /* back up a level */
            return 0;
        }

        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }

    if (slot == 0x5ad) {
        obj->field1c = 4;

        *mk3_frame(thread, frame + 1) = 0x5af;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot != 0)
        return -3;

    if (obj->field08->field28 & 0x10)
        MK3_SET_FIELD0E(obj->field08, MK3_FIELD0E(obj->field08) + 0xa);
    else
        MK3_SET_FIELD0E(obj->field08, MK3_FIELD0E(obj->field08) - 0xa);

    /* both sides join here (`b #0x7ab42`; difftest) */
    obj->field00->field3c = obj->field1c;
    obj->a10               = 0;
    obj->field20            = 0x20;
    zap_init_special_act(obj);

    obj->field1c = 0;
    ochar_sound(obj);

    obj->field1c = 3;
    ochar_sound(obj);

    obj->field1c = 4;
    ochar_sound(obj);

    obj->field40 = 2;
    get_char_ani2(obj);

    do_next_a9_frame(obj);

    *mk3_frame(thread, frame + 1) = 0x5ad;
    thread->fieldfc = 6;
    return 6;
}


/* ------------------------------------------------------------------------ t_bgrav9
 *
 * armv7 0x0007905c, 320 bytes.  **Complete.**
 *
 * A ground-pound shockwave that refuses to touch a boss. The free stops
 * the GrObj and counts `field48` down from `0x40` at `0xcc8` -- a bare
 * self-loop, `next_anirate` every tick -- until it dries, then reads
 * `G[0x450]`: zero means check for a boss right away, non-zero means wait
 * essentially forever (`0x16462`, the same "never answer" idiom
 * `t_master_summon_proc`/`t_friendship_speech` use) before checking. A
 * boss dies the shockwave outright with no effect; anyone else gets
 * `create_fx`, a sound, `set_inviso`, and a five-tick `strike_check_a0`
 * loop at `0xce3` that also dies on a hit or once it dries.
 *
 *      slot = frame[frame+1].w0
 *                                          ; 0:     free, then 0xcc8 (loop)
 *                                          ; 0xcc8:  countdown loop, then boss check
 *                                          ; 0xcd0:  wait-forever's boss check
 *                                          ; 0xce3:  strike-check loop
 *      if (slot == 0xcd0) goto boss_check
 *      if (slot < 0xcc8) {
 *          if (slot != 0) return -3
 *          stop_a8(GrObj) ; obj->field48 = 0x40
 *          token 0xcc8 ; fieldfc = 1 ; return 1
 *      }
 *      if (slot == 0xcc8) {
 *          next_anirate(obj) ; obj->field48 -= 1
 *          if (obj->field48 != 0) { token 0xcc8 ; fieldfc = 1 ; return 1 }
 *          obj->field1c = (int16)G[0x450]
 *          if (G[0x450] == 0) goto boss_check
 *          token 0xcd0 ; fieldfc = 0x16462 ; return 0x16462
 *      }
 *      if (slot != 0xce3) return -3
 *      obj->field1c = 0x15 ; strike_check_a0(obj)
 *      if (obj->field5c != 0) install tl_delete_proj_and_die ; return 0
 *      obj->field48 -= 1
 *      if (obj->field48 == 0) install tl_delete_proj_and_die ; return 0
 *      goto repark_ce3
 *
 *      boss_check: q_is_he_a_boss(obj)
 *                  if (obj->field5c != 0) install tl_delete_proj_and_die ; return 0
 *                  obj->field1c = 0xa ; create_fx(obj)
 *                  obj->field1c = 0xa ; ochar_sound(obj)
 *                  set_inviso(obj)
 *                  obj->field1c = GrObj->field24 = 8
 *                  obj->field48 = 5
 *      repark_ce3: token 0xce3 ; fieldfc = 1 ; return 1
 */
void q_is_he_a_boss(MK3OBJ *obj);
void set_inviso(MK3OBJ *obj);

long t_bgrav9(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t slot  = *mk3_frame(thread, frame + 1);

    if (slot == 0xcd0)
        goto boss_check;

    if (slot < 0xcc8) {
        if (slot != 0)
            return -3;

        stop_a8(obj->field08);
        obj->field48 = 0x40;

        *mk3_frame(thread, frame + 1) = 0xcc8;
        thread->fieldfc = 1;
        return 1;
    }

    if (slot == 0xcc8) {
        next_anirate(obj);
        obj->field48 -= 1;
        if (obj->field48 != 0) {
            *mk3_frame(thread, frame + 1) = 0xcc8;
            thread->fieldfc = 1;
            return 1;
        }

        obj->field1c = (uint32_t)(int32_t)(int16_t)
            *(const uint16_t *)((const char *)G + 0x450);
        if (*(const uint16_t *)((const char *)G + 0x450) == 0)
            goto boss_check;

        *mk3_frame(thread, frame + 1) = 0xcd0;
        thread->fieldfc = 0x16462;
        return 0x16462;
    }

    if (slot != 0xce3)
        return -3;

    obj->field1c = 0x15;
    strike_check_a0(obj);
    if (obj->field5c != 0)
        return mk3_install(thread, (MK3THREADFUNC)tl_delete_proj_and_die);

    obj->field48 -= 1;
    if (obj->field48 == 0)
        return mk3_install(thread, (MK3THREADFUNC)tl_delete_proj_and_die);
    goto repark_ce3;

boss_check:
    q_is_he_a_boss(obj);
    if (obj->field5c != 0)
        return mk3_install(thread, (MK3THREADFUNC)tl_delete_proj_and_die);

    obj->field1c = 0xa;
    create_fx(obj);
    obj->field1c = 0xa;
    ochar_sound(obj);
    set_inviso(obj);
    obj->field1c = 8;
    obj->field08->field24 = 8;
    obj->field48 = 5;

repark_ce3:
    *mk3_frame(thread, frame + 1) = 0xce3;
    thread->fieldfc = 1;
    return 1;
}


/* ------------------------------------------------------------------ t_saw_strike_check
 *
 * armv7 0x0007c398, 364 bytes.  **Complete.** The last of this batch.
 *
 * A buzzsaw that refuses a boss, checks a flag bit before it can even
 * try to strike, then chases through a reaction gate before it flies.
 * The free asks `q_is_he_a_boss` first and pops (or exits at the bottom)
 * if so -- the same `pop_or_exit` tail a strike MISS reuses later, one
 * physical site for two different reasons to give up. Otherwise it reads
 * `*(obj->field00->field28 + 0x10)`: bit 2 set dies outright, no name for
 * what the bit or the pointer are. Clear, and `strike_check_a0` runs; a
 * hit pops a level (when it can) and marks `obj->thread->pid = 0x207`
 * before falling into the SAME reaction-wait `0x719` uses -- so landing a
 * hit re-checks the opponent's reaction exactly like the periodic watch
 * does, through one shared block, not two copies of it. Not reacting sets
 * a flat `0x100000` velocity and falls straight into `0x722`'s on-screen
 * watch without calling `proj_onscreen_test` that first tick, the same
 * "skip the check on the transition tick" shape `t_boomerang_proc` uses
 * for its own turn.
 *
 *      slot = frame[frame+1].w0
 *      if (slot == 0x719) {
 *          next_anirate(obj) ; q_is_he_reacting(obj)
 *          goto reacting_check
 *      }
 *      if (slot == 0x722) {
 *          next_anirate(obj) ; proj_onscreen_test(obj)
 *          goto onscreen_watch
 *      }
 *      if (slot != 0) return -3
 *      q_is_he_a_boss(obj)
 *      if (obj->field5c != 0) goto pop_or_exit
 *      obj->field1c = p = proc->field28
 *      obj->field2c = v = *(uint32_t *)(p + 0x10)
 *      if (v & 4) install tl_delete_proj_and_die ; return 0
 *      obj->field1c = 0x13 ; strike_check_a0(obj)
 *      if (obj->field5c == 0) goto pop_or_exit
 *      if (frame <= 0) install t_local_reaction_exit ; return 0
 *      frame -= 1
 *      obj->thread->pid = 0x207
 *      obj->field1c = 5 ; ochar_sound(obj)
 *      stop_a8(GrObj)
 *      reacting_check:
 *          if (obj->field5c != 0) { token 0x719 ; fieldfc = 1 ; return 1 }
 *          obj->field1c = 0x100000 ; set_proj_vel(obj)
 *      onscreen_watch:
 *          if (obj->field5c == 0) install tl_delete_proj_and_die ; return 0
 *          token 0x722 ; fieldfc = 1 ; return 1
 *      pop_or_exit:
 *          if (frame > 0) { frame -= 1 ; return 0 }
 *          install t_local_reaction_exit ; return 0
 */
void q_is_he_reacting(MK3OBJ *obj);

long t_saw_strike_check(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);

    if (slot == 0x719) {
        next_anirate(obj);
        q_is_he_reacting(obj);
        goto reacting_check;
    }

    if (slot == 0x722) {
        next_anirate(obj);
        proj_onscreen_test(obj);
        goto onscreen_watch;
    }

    if (slot != 0)
        return -3;

    q_is_he_a_boss(obj);
    if (obj->field5c != 0)
        goto pop_or_exit;

    {
        uint32_t p = obj->field00->field28;
        uint32_t v;

        obj->field1c = p;
        v = *(const uint32_t *)((uintptr_t)p + 0x10);
        obj->field2c = v;

        if ((v & 4) != 0)
            return mk3_install(thread, (MK3THREADFUNC)tl_delete_proj_and_die);
    }

    obj->field1c = 0x13;
    strike_check_a0(obj);

    if (obj->field5c == 0)
        goto pop_or_exit;

    /* A hit pops a level -- at the bottom the exit is installed instead --
     * and then slides the level above down into this one, the shape
     * mkdrone.c calls POP_AND_SLIDE; then straight to the 0x719 park, not
     * through the reacting test (difftest) */
    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
    } else {
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_local_reaction_exit;
        *mk3_frame(thread, thread->frame + 1) = 0;
    }
    {
        uint32_t f = thread->frame;
        uint32_t h = mk3_frame(thread, f + 1)[1];

        *mk3_frame(thread, f + 1) = *mk3_frame(thread, f + 2);
        mk3_frame(thread, f)[1]   = h;
    }

    obj->thread->pid = 0x207;
    obj->field1c = 5;
    ochar_sound(obj);
    stop_a8(obj->field08);
    goto park_719;

reacting_check:
    if (obj->field5c != 0) {
park_719:
        *mk3_frame(thread, thread->frame + 1) = 0x719;
        thread->fieldfc = 1;
        return 1;
    }

    /* `add r3, #0x100000` on 0x5c, which is 0 here; and set_proj_vel's
     * answer is NOT tested: the code joins the 0x722 park below the test */
    obj->field1c = 0x100000;
    set_proj_vel(obj);
    goto park_722;

onscreen_watch:
    if (obj->field5c == 0)
        return mk3_install(thread, (MK3THREADFUNC)tl_delete_proj_and_die);

park_722:
    *mk3_frame(thread, thread->frame + 1) = 0x722;
    thread->fieldfc = 1;
    return 1;

pop_or_exit:
    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
        return 0;
    }
    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* ------------------------------------------------------------------------ t_sz_zap_hit
 *
 * armv7 0x0007c000, 256 bytes.  **Complete.** `t_ice_collision_check`'s
 * own launch target.
 *
 * The free saves the GrObj's `0x12` halfword on the argument stack around
 * `match_me_with_him`/`flip_multi`/`multi_adjust_xy` (a `-0x90` nudge,
 * `field20` zeroed), restores it, poses animation `0x24` through
 * `borrow_char_ani`, and descends into `t_mframew` from `0x829`. `0x829`
 * is the floor: `tl_delete_proj_and_die`, no pop.
 *
 *      slot = frame[frame+1].w0
 *      if (slot != 0) {
 *          if (slot != 0x829) return -3
 *          install tl_delete_proj_and_die ; return 0
 *      }
 *      obj->field1c = 0x40003 ; hob_ochar_sound(obj)
 *      stop_a8(GrObj)
 *      args[fieldf8] = (int16)GrObj->0x12 ; fieldf8++
 *      match_me_with_him(obj) ; flip_multi(obj)
 *      obj->field20 = 0 ; obj->field1c = -0x90 ; multi_adjust_xy(obj)
 *      fieldf8-- ; GrObj->0x12 = obj->field38 = args[fieldf8]
 *      obj->field1c = 4 ; obj->field40 = 0x24 ; borrow_char_ani(obj)
 *      obj->field54 = 3 ; find_part_a14(obj)
 *      obj->field1c = 4
 *      token 0x829 ; frame++ ; install t_mframew ; return 0
 */
void borrow_char_ani(MK3OBJ *obj);
void find_part_a14(MK3OBJ *obj);
void match_me_with_him(MK3OBJ *obj);
void flip_multi(MK3OBJ *obj);
void multi_adjust_xy(MK3OBJ *obj);

long t_sz_zap_hit(MK3THREAD *thread)
{
    MK3OBJ   *obj  = (MK3OBJ *)thread->proc;
    uint32_t *args = (uint32_t *)(void *)thread->args;
    uint32_t  frame = thread->frame;
    uint32_t  slot  = *mk3_frame(thread, frame + 1);

    if (slot != 0) {
        if (slot != 0x829)
            return -3;
        return mk3_install(thread, (MK3THREADFUNC)tl_delete_proj_and_die);
    }

    obj->field1c = 0x40003;
    hob_ochar_sound(obj);

    stop_a8(obj->field08);

    args[thread->fieldf8] = (uint32_t)(int32_t)MK3_FIELD12_S(obj->field08);
    obj->field38 = args[thread->fieldf8];
    thread->fieldf8 = thread->fieldf8 + 1;

    match_me_with_him(obj);
    flip_multi(obj);

    obj->field20 = 0;
    obj->field1c = (uint32_t)~0x8f;      /* -0x90 */
    multi_adjust_xy(obj);

    thread->fieldf8 = thread->fieldf8 - 1;
    obj->field38 = args[thread->fieldf8];
    MK3_SET_FIELD12(obj->field08, (uint16_t)obj->field38);

    obj->field1c = 4;
    obj->field40 = 0x24;
    borrow_char_ani(obj);

    obj->field54 = 3;
    find_part_a14(obj);

    obj->field1c = 4;

    *mk3_frame(thread, frame + 1) = 0x829;
    thread->frame = thread->frame + 1;   /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_mframew;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* t_boom_return_check -- armv7 0x00075778, 160 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      proj_onscreen_test_unsafe(obj)
 *      if (obj->field5c != 0)
 *          pop a level, or t_local_reaction_exit at the bottom
 *      part->field18 = -part->field18
 *      obj->field1c   = t_boomerang_trail
 *      proc->field28  = t_boomerang_trail
 *      pop a level, or t_local_reaction_exit at the bottom
 *
 * **The boomerang turns around when it leaves the screen, and it uses the strict
 * test to decide.** `proj_onscreen_test_unsafe` fails the moment the projectile
 * touches an edge, where `proj_onscreen_test` allows a hundred units of slack --
 * so the turn happens at the visible boundary rather than off in the margin,
 * which is the whole point of having both tests. That is the first caller
 * measured for either of them and it picks the strict one deliberately.
 *
 * **Then it replaces its own callback with a do-nothing one.**
 * `tl_projectile_flight_call` puts a per-frame callback in `proc->field28`; this
 * overwrites it with `t_boomerang_trail`, which is eighty-four bytes of giving
 * the level straight back. So after the turn the boomerang stops falling and just
 * flies -- and `t_boomerang_trail` exists to be that null callback rather than to
 * do anything.
 *
 * That answers the question the `t_boomerang_trail` / `t_rr_nothing` note asked:
 * the two identical do-nothing handlers are not duplication, they are the null
 * value for two different callback slots, and each is named for the system whose
 * slot it goes into. `t_rr_nothing` is entry 0 of `rocket_routines`.
 *
 * The pointer is written into 0x1c as well as 0x28 -- one register, two
 * destinations, this file's habit -- so 0x1c holds a function pointer on the way
 * out, a reading it has in `t_do_zap` too.
 */
long t_boom_return_check(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;

    if (*mk3_frame(thread, frame + 1) != 0)
        return -3;

    proj_onscreen_test_unsafe(obj);          /* the strict test, on purpose */

    if (obj->field5c == 0) {
        obj->field08->field18 =
            (uint32_t)(-(long)obj->field08->field18);

        obj->field1c          = (uint32_t)(uintptr_t)t_boomerang_trail;
        obj->field00->field28 = obj->field1c;    /* the null callback */
    }

    if ((long)thread->frame > 0) {           /* cmp #0 / ble: signed */
        thread->frame = thread->frame - 1;   /* back up a level */
        return 0;
    }

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* t_angle_zap_call -- armv7 0x00076388, 148 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field30 = 0x88
 *      y     = (int16_t)part->x12
 *      floor = *(long *)(G + 0xac)
 *      obj->field1c = y
 *      obj->field20 = floor - y
 *      if (obj->field20 > 0x88)
 *          pop a level, or t_local_reaction_exit at the bottom
 *      obj->field20 = floor - 0x88
 *      part->x12 = (uint16_t)obj->field20            ; snapped, not eased
 *      obj->field1c = 3; ochar_sound(obj)
 *      frame[frame].handler = t_angle_zap_explode
 *
 * **A per-frame callback that catches the zap 0x88 above the floor and snaps it
 * exactly there.** Not eased, not clamped -- the y is overwritten with
 * `floor - 0x88` on the frame the gap first closes, so the last step of the
 * descent is however far it had left to travel. A port that interpolates instead
 * will draw one frame differently.
 *
 * **Tenth routine to read `G + 0xac`** as the floor, and the first in this file.
 *
 * `obj->field30 = 0x88` is the threshold written into a field before it is used
 * as a literal in the comparison, and nothing here reads it back. Whether
 * `t_angle_zap_explode` does is not settled; transcribed rather than dropped.
 *
 * The subtraction is `rsb r3, r2, r1` -- floor minus y, so a bigger number means
 * higher up. Every height test in this tree is that way round and it is worth
 * saying once.
 */
long t_angle_zap_explode(MK3THREAD *thread);

long t_angle_zap_call(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t floor;

    if (*mk3_frame(thread, frame + 1) != 0)
        return -3;

    obj->field30 = 0x88;

    obj->field1c = (uint32_t)(int32_t)(int16_t)MK3_FIELD12(obj->field08);
    floor = *(uint32_t *)(G_BYTES + 0xac);
    obj->field20 = floor - obj->field1c;

    if ((long)obj->field20 <= 0x88) {
        obj->field20 = floor - 0x88;
        MK3_SET_FIELD12(obj->field08, obj->field20);   /* snapped */

        obj->field1c = 3;
        ochar_sound(obj);

        return mk3_install(thread, (MK3THREADFUNC)t_angle_zap_explode);
    }

    if ((long)thread->frame > 0) {           /* cmp #0 / ble: signed */
        thread->frame = thread->frame - 1;   /* back up a level */
        return 0;
    }

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* t_summon_flame_animator -- armv7 0x000774a4, 152 bytes.  **Complete.**
 *
 *      token == 0:       part->field24 = 0xc
 *                        obj->field1c  = 0; ochar_sound(obj)
 *                        obj->field48  = 0x00060006; shake_a11(obj)
 *                        obj->field1c  = 4
 *                        token := 0xb0b, descend into t_mframew
 *
 *      token == 0xb0b:   frame[frame].handler = tl_delete_proj_and_die
 *
 *      otherwise:        return -3
 *
 * **The flame identifies as character 0xc, permanently.** `part->field24` is the
 * table index -- the field `borrow_ochar_sound` in other.c lends for exactly one
 * call and then puts back -- and this routine writes 0xc into it and never
 * restores it. So every table lookup this object makes for the rest of its life
 * resolves against character 0xc's row.
 *
 * That is the difference from every other site: `borrow_ochar_sound` borrows and
 * `t_hair_spun` in mkfatal.c borrows across one `get_char_ani2` using the
 * argument stack, but a summoned flame has no identity of its own to go back to,
 * so it simply takes one. **A port must not "fix" the missing restore.**
 *
 * The shake pair 0x00060006 is the symmetric one `t_r_impale_upcut` and
 * `t_st_spiked` also use -- third site for that exact value.
 *
 * `obj->field1c = 0` comes out of the token register the dispatch has proved to
 * be zero, so the sound index is 0 from character 0xc's table.
 */
void shake_a11(MK3OBJ *obj);

long t_summon_flame_animator(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0) {
        obj->field08->field24 = 0xc;         /* taken, not borrowed */

        obj->field1c = 0;
        ochar_sound(obj);

        obj->field48 = 0x00060006;
        shake_a11(obj);

        obj->field1c = 4;

        *mk3_frame(thread, frame + 1) = 0xb0b;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0xb0b)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)tl_delete_proj_and_die);
}


/* t_angle_zap_explode -- armv7 0x00077408, 156 bytes.  **Complete.**
 *
 *      token == 0:       obj->field48 = 0x00040008; shake_a11(obj)
 *                        stop_a8(part)
 *                        obj->field40 = 0x3f; find_ani_part2(obj)
 *                        obj->field1c = 2
 *                        token := 0xdf8, descend into t_mframew
 *
 *      token == 0xdf8:   frame[frame].handler = tl_delete_proj_and_die
 *
 *      otherwise:        return -3
 *
 * **The explosion reuses the projectile's own animation base.** 0x3f is the same
 * number `setup_proj_obj` resolves when the projectile is created, so the
 * explosion frames live in the same block as the flight frames and one lookup
 * covers both. Second site for 0x3f and it makes the number worth naming.
 *
 * `stop_a8` before the new animation, so the flight's frame stepping is halted
 * rather than left running underneath the explosion.
 *
 * The shake pair is 0x00040008 -- asymmetric, twice as much vertically as
 * horizontally -- and the first site in the tree for that exact value.
 *
 * `t_angle_zap_call` hands over to this and it hands over to
 * `tl_delete_proj_and_die`, so the angle zap's last three routines are a straight
 * chain: watch the floor, explode, park for ever.
 */
void stop_a8(MK3OBJ *part);
void find_ani_part2(MK3OBJ *obj);

long t_angle_zap_explode(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0) {
        obj->field48 = 0x00040008;
        shake_a11(obj);

        stop_a8(obj->field08);

        obj->field40 = 0x3f;                 /* the projectile's own base */
        find_ani_part2(obj);

        obj->field1c = 2;

        *mk3_frame(thread, frame + 1) = 0xdf8;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0xdf8)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)tl_delete_proj_and_die);
}


/* tl_do_jax_zap1 -- armv7 0x00079778, 160 bytes.  **Complete.**
 *
 *      token == 0:        obj->field20 = 3
 *                         obj->a10     = 0
 *                         zap_init_special_act(obj)
 *                         token := 0x1297, descend into tl_jax_zap_jsrp
 *
 *      token == 0x1297:   obj->field1c = G + 0x410; update_tsl(obj)
 *                         obj->field20 = 0x16
 *                         frame[frame].handler = tl_do_proj_sitting_duck
 *
 *      otherwise:         return -3
 *
 * **Entry 2 of `projectile_jumps`, and it closes `tl_do_proj_sitting_duck`.**
 * That routine parks for however long 0x20 says and then detaches the
 * projectile; here is the caller, and the number is **0x16** -- twenty-two
 * frames. So 0x20 is confirmed as the duration and the only park in this file
 * whose length the caller chooses is chosen right here.
 *
 * **`G + 0x410` is the base of the timer block.** mkstat.c reaches
 * `G + 0x410 + 0xc` for Jade's flash and the two Reptile orbs use `G + 0x438`
 * and `G + 0x43c`. So 0x410 onwards is a run of per-move timer slots, this is
 * the first of them, and `update_tsl` is what stamps one.
 *
 * The two states are the shape most of this file's `tl_do_*` entries have: set up
 * a couple of numbers, descend into the move's own routine, and on the way back
 * arrange what happens after. Nothing in between is this routine's business.
 */
long tl_jax_zap_jsrp(MK3THREAD *thread);

long tl_do_jax_zap1(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0) {
        obj->field20 = 3;
        obj->a10     = 0;
        zap_init_special_act(obj);

        *mk3_frame(thread, frame + 1) = 0x1297;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)tl_jax_zap_jsrp;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x1297)
        return -3;

    obj->field1c = (uint32_t)(uintptr_t)(G_BYTES + 0x410);
    update_tsl(obj);

    obj->field20 = 0x16;                     /* the sitting-duck duration */

    return mk3_install(thread, (MK3THREADFUNC)tl_do_proj_sitting_duck);
}


/* t_motaro_zap_proc -- armv7 0x00076c38, 160 bytes.  **Complete.**
 *
 *      token == 0:       obj->field20 = 3
 *                        obj->field1c = 0xa0000
 *                        set_proj_vel(obj)
 *                        obj->field48 = 3
 *                        obj->field34 = t_mot_zap_call
 *                        token := 0x683, descend into tl_projectile_flight_call
 *
 *      token == 0x683:   obj->field48 = 0
 *                        make_lineup_explode(obj)
 *                        frame[frame].handler = tl_delete_proj_and_die
 *
 *      otherwise:        return -3
 *
 * **This closes the callback chain.** `t_mot_zap_call` -- the eighty-eight byte
 * routine that adds 0x5000 to the y velocity and pops -- goes into `obj->field34`
 * here; `tl_projectile_flight_call` copies 0x34 into `proc->field28`; and the
 * flight calls whatever is in 0x28 once a frame. Three routines written in three
 * separate batches, and the interfaces meet with nothing left over.
 *
 * So `obj->field34` is where a driver names its per-frame callback and
 * `proc->field28` is where the flight reads it. mkfatal.c's `t_r_impale_upcut`
 * puts `t_impale_call` in the same 0x34, which makes this the second system
 * using that slot the same way.
 *
 * **`obj->field48 = 0` before the explosion, where `make_dragon_explode` passes
 * `(0x10 << 16) | width`.** Only the high half of 0x48 does any work in
 * `make_lineup_explode` -- the low half feeds a store that is overwritten before
 * anything reads it -- so passing zero means no vertical offset, and the width
 * being zero costs nothing that was ever spent. The two callers together are
 * what make that reading safe.
 *
 * `lsl.w r3, r3, r8` with `r8` holding 3 -- a register shift where every other
 * push in the tree uses `lsls r3, r3, #3`, because the 3 was already in a
 * register from `obj->field20`. Same arithmetic, and worth naming once so nobody
 * reads it as a different stride.
 */
long t_mot_zap_call(MK3THREAD *thread);
long tl_projectile_flight_call(MK3THREAD *thread);

long t_motaro_zap_proc(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0) {
        obj->field20 = 3;
        obj->field1c = 0xa0000;
        set_proj_vel(obj);

        obj->field48 = 3;
        obj->field34 = (uint32_t)(uintptr_t)t_mot_zap_call;

        *mk3_frame(thread, frame + 1) = 0x683;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)tl_projectile_flight_call;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x683)
        return -3;

    obj->field48 = 0;                        /* no vertical offset */
    make_lineup_explode(obj);

    return mk3_install(thread, (MK3THREADFUNC)tl_delete_proj_and_die);
}


/* -------------------------------------------------------------- tl_do_motaro_zap
 *
 * armv7 0x0007945c, 308 bytes.  **Complete.**
 *
 * `t_motaro_zap_proc`'s driver. The free runs `zap_init_special` (not
 * `zap_init_special_act`), poses animation 6 and waits four ticks from
 * `0x692` with no push -- a bare self-resume, same idiom as
 * `tl_do_tusk_floor`'s `0x769`. `0x693` is the launch: `obj->field40` is
 * saved on the argument stack, `find_part2` walks it, the launch sound
 * plays, `field38 = t_motaro_zap_proc` goes into `create_proj_proc`, and
 * the new slave's OWN `field40` is set from what this object's was BEFORE
 * the saved value is restored -- so the slave inherits the pose the
 * thrower had at the moment of the throw, not whatever `find_part2` left
 * behind. `detach_proj` follows, then a plain `t_mframew` wait for `0x6a0`,
 * whose re-entry just installs `t_local_reaction_exit`.
 *
 *      slot = frame[frame+1].w0
 *                                          ; 0:     free, then 0x692 (wait)
 *                                          ; 0x692:  wait, then 0x693
 *                                          ; 0x693:  launch, then 0x6a0
 *                                          ; 0x6a0:  install t_local_reaction_exit
 *      if (slot == 0x692) { token 0x693 ; fieldfc = 4 ; return 4 }
 *      if (slot < 0x692) {
 *          if (slot != 0) return -3
 *          obj->a10 = 0 ; zap_init_special(obj)
 *          obj->field40 = 6 ; get_char_ani(obj)
 *          obj->field1c = 4
 *          token 0x692 ; frame++ ; install t_mframew ; return 0
 *      }
 *      if (slot == 0x693) {
 *          args[fieldf8] = obj->field40 ; fieldf8++
 *          find_part2(obj)
 *          obj->field1c = 5 ; ochar_sound(obj)
 *          obj->field38 = t_motaro_zap_proc ; create_proj_proc(obj)
 *          proc->field64->field40 = obj->field40
 *          fieldf8-- ; obj->field40 = args[fieldf8]
 *          detach_proj(obj) ; obj->field1c = 5
 *          token 0x6a0 ; frame++ ; install t_mframew ; return 0
 *      }
 *      if (slot != 0x6a0) return -3
 *      install t_local_reaction_exit ; return 0
 */
void zap_init_special(MK3OBJ *obj);
void get_char_ani(MK3OBJ *obj);
void find_part2(MK3OBJ *obj);
void detach_proj(MK3OBJ *obj);

long tl_do_motaro_zap(MK3THREAD *thread)
{
    MK3OBJ   *obj  = (MK3OBJ *)thread->proc;
    uint32_t *args = (uint32_t *)(void *)thread->args;
    uint32_t  frame = thread->frame;
    uint32_t  slot  = *mk3_frame(thread, frame + 1);

    if (slot == 0x692) {
        *mk3_frame(thread, frame + 1) = 0x693;
        thread->fieldfc = 4;
        return 4;
    }

    if (slot < 0x692) {
        if (slot != 0)
            return -3;

        obj->a10 = 0;
        zap_init_special(obj);

        obj->field40 = 6;
        get_char_ani(obj);

        obj->field1c = 4;

        *mk3_frame(thread, frame + 1) = 0x692;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot == 0x693) {
        args[thread->fieldf8] = obj->field40;
        thread->fieldf8 = thread->fieldf8 + 1;

        find_part2(obj);

        obj->field1c = 5;
        ochar_sound(obj);

        obj->field38 = (uint32_t)(uintptr_t)t_motaro_zap_proc;
        create_proj_proc(obj);

        ((MK3OBJ *)(uintptr_t)obj->field00->field64)->field40 = obj->field40;

        thread->fieldf8 = thread->fieldf8 - 1;
        obj->field40 = args[thread->fieldf8];

        detach_proj(obj);

        obj->field1c = 5;

        *mk3_frame(thread, frame + 1) = 0x6a0;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot != 0x6a0)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* t_lk_zap_air -- armv7 0x0007b5b8, 156 bytes.  **Complete.**
 *
 *      token == 0:        obj->a10 = 0
 *                         zap_air_init_special(obj)
 *                         obj->field20  = 0x15
 *                         proc->field18 = 0x15
 *                         obj->field1c = 0; ochar_sound(obj)
 *                         obj->field40 = 1; get_char_ani2(obj)
 *                         obj->field1c = 2
 *                         token := 0xa20, descend into t_mframew
 *
 *      token == 0xa20:    frame[frame].handler = t_lk_zap_entry
 *
 *      otherwise:         return -3
 *
 * **0x15 goes into two places from one register**: `obj->field20`, where the
 * caller can read it back, and `proc->field18`, which is the action
 * `get_his_action` reports. That is the same shape `i_am_a_sitting_duck` uses
 * with 0x604 and `tl_do_proj_sitting_duck` repeats -- announce the action and
 * keep a copy -- so it is the file's convention and not a one-off.
 *
 * `obj->field1c = 0` comes out of the token register the dispatch proved to be
 * zero, so the sound index is 0.
 *
 * The whole routine is a setup and a hand-over: nothing here moves anything, and
 * `t_lk_zap_entry` is where the move actually starts.
 */
void zap_air_init_special(MK3OBJ *obj);
long t_lk_zap_entry(MK3THREAD *thread);

long t_lk_zap_air(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0) {
        obj->a10 = 0;
        zap_air_init_special(obj);

        obj->field20          = 0x15;        /* one register, two fields */
        obj->field00->field18 = 0x15;

        obj->field1c = 0;
        ochar_sound(obj);

        obj->field40 = 1;
        get_char_ani2(obj);

        obj->field1c = 2;

        *mk3_frame(thread, frame + 1) = 0xa20;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0xa20)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_lk_zap_entry);
}


/* ------------------------------------------------------------------ t_lk_zap_entry
 *
 * armv7 0x000759dc, 268 bytes.  **Complete.**
 *
 * The grounded (and, via `t_lk_zap_air`, airborne) hand-off winds through
 * `t_lk_prezap` three times in a row, each pass repacking `field20`/`field24`
 * with a different literal pair before pushing `t_lk_prezap` again under the
 * next resume token in the chain (`0` -> push under `0xa4f`; `0xa4f` -> push
 * under `0xa52`; `0xa52` -> push under `0xa55`). `field24`'s free-branch
 * value (`0x130071`) and the `0xa52` pass's value are the same word read
 * twice from the same literal-pool slot, not a coincidence worth reading
 * into.
 *
 * The final resume (`0xa55`) is the actual launch: `field38` is set to
 * `t_lk_zap_proc` -- the projectile's own hit callback, the same
 * `obj->field38 = <callback>` convention every other zap launcher in this
 * file uses -- `create_proj_proc` spawns it, and `t_lkzap5` is installed on
 * the CURRENT level (no push) to drive it from here on.
 */
MK3OBJ *create_proj_proc(MK3OBJ *obj);
long t_lk_zap_proc(struct MK3THREAD *thread);
long t_lkzap5(struct MK3THREAD *thread);

long t_lk_zap_entry(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t slot  = *mk3_frame(thread, frame + 1);

    if (slot == 0xa4f) {
        obj->field20 = 0x1000a4;
        obj->field24 = 0x13005f;

        *mk3_frame(thread, frame + 1) = 0xa52;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_lk_prezap;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot == 0xa52) {
        obj->field20 = 0x1000d0;
        obj->field24 = 0x130071;

        *mk3_frame(thread, frame + 1) = 0xa55;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_lk_prezap;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot == 0xa55) {
        obj->field38 = (uint32_t)(uintptr_t)t_lk_zap_proc;
        create_proj_proc(obj);
        return mk3_install(thread, (MK3THREADFUNC)t_lkzap5);
    }

    if (slot != 0)
        return -3;

    obj->field1c = 0x11;
    obj->field20 = 0xe0080;
    obj->field24 = 0x130071;

    *mk3_frame(thread, frame + 1) = 0xa4f;
    thread->frame = thread->frame + 1;   /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_lk_prezap;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------------------------ t_lkzap5
 *
 * armv7 0x0007919c, 352 bytes.  **Complete.**
 *
 * `t_lk_zap_entry`'s own install target, driving the zap once it's actually
 * out. The free checks `am_i_airborn`; airborne poses animation 1
 * (`get_char_ani2`), `field54 = 3`/`find_part_a14`, `field1c = 2`, plants
 * token `0xa7d` for after it pops, and pushes `t_mframew`. Grounded instead
 * runs `i_am_a_sitting_duck` and checks `am_i_short`: short re-arms itself
 * under `0xa6f` and waits 24 ticks; not short re-arms the SAME handler
 * (`0xa66`, itself) and waits 16.
 *
 * `0xa66` (the self re-arm) poses animation `0x24` (`field54 = 3`,
 * `find_ani_part_a14`), `field1c = 4`, and installs `t_mframew` directly on
 * the current level -- no push, since nothing needs to run after it pops.
 * `0xa6f` (the short re-arm) does the same with animation `0` instead of
 * `0x24`. `0xa7d` -- reached only after the airborne push pops back --
 * installs `t_drop_down_land` directly, the same pointer-slot borrow from
 * `mkslam.c` every other "let a fall resolve itself" site in this file uses.
 */
long am_i_airborn(MK3OBJ *obj);
long am_i_short(MK3OBJ *obj);
void find_part_a14(MK3OBJ *obj);
void find_ani_part_a14(MK3OBJ *obj);
long t_drop_down_land(struct MK3THREAD *thread);        /* pointer slot 0x000f33d4 */

long t_lkzap5(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t slot  = *mk3_frame(thread, frame + 1);

    if (slot == 0xa66) {
        obj->field40 = 0x24;
        obj->field54 = 0x24 - 0x21;
        find_ani_part_a14(obj);

        obj->field1c = 4;

        return mk3_install(thread, (MK3THREADFUNC)t_mframew);
    }

    if (slot == 0xa6f) {
        obj->field40 = 0;
        get_char_ani2(obj);

        obj->field54 = 3;
        find_part_a14(obj);

        obj->field1c = 4;

        return mk3_install(thread, (MK3THREADFUNC)t_mframew);
    }

    if (slot == 0xa7d)
        return mk3_install(thread, (MK3THREADFUNC)t_drop_down_land);

    if (slot != 0)
        return -3;

    am_i_airborn(obj);

    if (obj->field5c != 0) {
        obj->field40 = 1;
        get_char_ani2(obj);

        obj->field54 = 3;
        find_part_a14(obj);

        obj->field1c = 2;

        *mk3_frame(thread, frame + 1) = 0xa7d;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    i_am_a_sitting_duck(obj);
    am_i_short(obj);

    if (obj->field5c != 0) {
        *mk3_frame(thread, frame + 1) = 0xa6f;
        thread->fieldfc = 0x18;
        return 0x18;
    }

    *mk3_frame(thread, frame + 1) = 0xa66;
    thread->fieldfc = 0x10;
    return 0x10;
}


/* t_sk_zap_proc -- armv7 0x00076cd8, 160 bytes.  **Complete.**
 *
 *      token == 0:       obj->field20 = 3
 *                        obj->field1c = 0xa0000
 *                        set_proj_vel(obj)
 *                        obj->field48 = 4
 *                        token := 0x6ad, descend into tl_projectile_flight
 *
 *      token == 0x6ad:   part->x0e = (uint16_t)him->x0e
 *                        obj->field1c = 5; create_fx(obj)
 *                        frame[frame].handler = tl_delete_proj_and_die
 *
 *      otherwise:        return -3
 *
 * **A third data point on how `create_fx` is aimed, and it tilts the question.**
 * `make_lineup_explode` moves the part's x to the opponent's, calls, and puts
 * the part back; `t_rocket_explode_fx` instead fills 0x44 and 0x48 with
 * coordinates; this one moves the part to the opponent's x and **does not put it
 * back**, because the projectile is about to be deleted.
 *
 * Two of the three callers aim by moving the part. So `create_fx` most likely
 * reads the PART's position, and `t_rocket_explode_fx`'s 0x44/0x48 stores are
 * either a different parameter or dead. That would also explain why
 * `make_lineup_explode`'s width computation never mattered: it feeds an x that
 * gets overwritten with the opponent's before the call, exactly as here.
 *
 * **Still a reading, not a measurement** -- `create_fx` at 0x00058d70 is not
 * decompiled and settles it in one look. But three callers now say the same
 * thing and none of them says the other.
 *
 * The same launch numbers as `t_motaro_zap_proc`: 3 into 0x20, 0xa0000 into
 * 0x1c, `set_proj_vel`. The two differ in 0x48 -- 4 here, 3 there -- and in
 * which flight they descend into.
 */
long tl_projectile_flight(MK3THREAD *thread);

long t_sk_zap_proc(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0) {
        obj->field20 = 3;
        obj->field1c = 0xa0000;
        set_proj_vel(obj);

        obj->field48 = 4;

        *mk3_frame(thread, frame + 1) = 0x6ad;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)tl_projectile_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x6ad)
        return -3;

    MK3_SET_FIELD0E(obj->field08,                /* aimed, and not put back */
                    MK3_FIELD0E((MK3OBJ *)(void *)(uintptr_t)
                                obj->field00->him));

    obj->field1c = 5;
    create_fx(obj);

    return mk3_install(thread, (MK3THREADFUNC)tl_delete_proj_and_die);
}


/* ------------------------------------------------------------------- tl_do_sk_zap
 *
 * armv7 0x00079310, 332 bytes.  **Complete.**
 *
 * The free packs `0x00030024` into `field40` and descends into
 * `t_animate_a9` from `0x6c8`. `0x6c8` is the launch, and the odd one in
 * this session's batch: it spawns a bare `NewThreadProc(obj,
 * t_wait_forever)` first and parks the result in `proc->field64` itself,
 * before `create_proj_proc` -- so the projectile's OWN slave slot is
 * pre-seeded with a thread that never does anything (`t_wait_forever`)
 * rather than being left at zero for `create_proj_proc` to fill fresh.
 * `proc->slave` is then read off THAT thread's own part. `field38 =
 * t_sk_zap_proc`, `adjust_xy_a5` by `field1c = -0x14`/`field20 = -6`, and
 * `detach_proj` follow before a bare thirty-two-tick wait for `0x6eb`.
 * `0x6eb` restores `field40` from `field48` and pushes a plain `t_mframew`
 * wait from `0x6ee`, the ordinary pop-or-exit-at-the-bottom floor.
 *
 *      slot = frame[frame+1].w0
 *                                          ; 0:    free, then 0x6c8
 *                                          ; 0x6c8: launch, then 0x6eb (wait)
 *                                          ; 0x6eb: wait, then 0x6ee
 *                                          ; 0x6ee: pop, or exit at the bottom
 *      if (slot == 0x6c8) {
 *          obj->field48 = obj->field40
 *          obj->field1c = 1 ; ochar_sound(obj)
 *          obj->field40 = 0x17 ; get_char_ani(obj)
 *          proc->field64 = NewThreadProc(obj, t_wait_forever)
 *          proc->slave = proc->field64->field08
 *          obj->field38 = t_sk_zap_proc ; create_proj_proc(obj)
 *          obj->field30 = proc->slave
 *          obj->field1c = -0x14 ; obj->field20 = -6 ; adjust_xy_a5(obj)
 *          detach_proj(obj)
 *          token 0x6eb ; fieldfc = 0x20 ; return 0x20
 *      }
 *      if (slot < 0x6c8) {
 *          if (slot != 0) return -3
 *          obj->a10 = 0 ; zap_init_special(obj)
 *          obj->field40 = 0x00030024
 *          token 0x6c8 ; frame++ ; install t_animate_a9 ; return 0
 *      }
 *      if (slot == 0x6eb) {
 *          obj->field40 = obj->field48 ; obj->field1c = 4
 *          token 0x6ee ; frame++ ; install t_mframew ; return 0
 *      }
 *      if (slot != 0x6ee) return -3
 *      pop a level, or t_local_reaction_exit at the bottom
 */
void *NewThreadProc(void *owner, MK3THREADFUNC func);
long t_wait_forever(struct MK3THREAD *thread);          /* pointer slot 0x000f3724 */
long t_sk_zap_proc(struct MK3THREAD *thread);
void adjust_xy_a5(MK3OBJ *obj);
void detach_proj(MK3OBJ *obj);
long t_animate_a9(struct MK3THREAD *thread);            /* pointer slot 0x000f36d0 */

long tl_do_sk_zap(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t slot  = *mk3_frame(thread, frame + 1);

    if (slot == 0x6c8) {
        void *newthread_proc;

        obj->field48 = obj->field40;

        obj->field1c = 1;
        ochar_sound(obj);

        obj->field40 = 0x17;
        get_char_ani(obj);

        newthread_proc = NewThreadProc(obj, (MK3THREADFUNC)t_wait_forever);
        obj->field00->field64 = (uint32_t)(uintptr_t)newthread_proc;

        obj->field00->slave = (uint32_t)(uintptr_t)
            ((MK3OBJ *)(uintptr_t)obj->field00->field64)->field08;

        obj->field38 = (uint32_t)(uintptr_t)t_sk_zap_proc;
        create_proj_proc(obj);

        obj->field30 = obj->field00->slave;

        obj->field1c = (uint32_t)~0x13;          /* -0x14 */
        obj->field20 = (uint32_t)(~0x13 + 0xe);  /* -6 */
        adjust_xy_a5(obj);

        detach_proj(obj);

        *mk3_frame(thread, frame + 1) = 0x6eb;
        thread->fieldfc = 0x20;
        return 0x20;
    }

    if (slot < 0x6c8) {
        if (slot != 0)
            return -3;

        obj->a10 = 0;
        zap_init_special(obj);

        obj->field40 = 0x00030024;

        *mk3_frame(thread, frame + 1) = 0x6c8;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_animate_a9;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot == 0x6eb) {
        obj->field40 = obj->field48;
        obj->field1c = 4;

        *mk3_frame(thread, frame + 1) = 0x6ee;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot != 0x6ee)
        return -3;

    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
        return 0;
    }
    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* tl_do_sw_zap -- armv7 0x00079db8, 164 bytes.  **Complete.**
 *
 *      token == 0:        obj->field20 = 7
 *                         obj->a10     = 0
 *                         zap_init_special_act(obj)
 *                         obj->field40 = 0x00030024
 *                         token := 0xea2, descend into t_animate_a9
 *
 *      token == 0xea2:    obj->field38 = t_swat_proj_proc
 *                         create_proj_proc(obj)
 *                         obj->field20 = 0x18
 *                         frame[frame].handler = tl_do_proj_sitting_duck
 *
 *      otherwise:         return -3
 *
 * **This closes `create_proj_proc`.** That routine restarts an existing slave
 * through `StartProcAt(proc->field64, obj->field38)` or makes a new one, and
 * nothing had shown who fills 0x38. Here it is: the driver writes the handler
 * the projectile will run -- `t_swat_proj_proc` -- and then calls. So 0x38 is
 * the projectile's entry point, which is the same slot the fatality files use
 * to hand a routine to the other fighter. **One field, two systems, the same
 * meaning: "the code this other object is about to run".**
 *
 * `obj->field20 = 0x18` is the second duration measured for
 * `tl_do_proj_sitting_duck`, after Jax's 0x16 -- twenty-four frames against
 * twenty-two, so the field really is a per-move number and not a constant
 * dressed up.
 *
 * Entry 8 of `projectile_jumps`.
 */
long t_swat_proj_proc(MK3THREAD *thread);
MK3OBJ *create_proj_proc(MK3OBJ *obj);
long t_animate_a9(MK3THREAD *thread);            /* pointer slot 0x000f36d0 */

long tl_do_sw_zap(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0) {
        obj->field20 = 7;
        obj->a10     = 0;
        zap_init_special_act(obj);

        obj->field40 = 0x00030024;

        *mk3_frame(thread, frame + 1) = 0xea2;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_animate_a9;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0xea2)
        return -3;

    obj->field38 = (uint32_t)(uintptr_t)t_swat_proj_proc;
    create_proj_proc(obj);

    obj->field20 = 0x18;                     /* the sitting-duck duration */

    return mk3_install(thread, (MK3THREADFUNC)tl_do_proj_sitting_duck);
}


/* ---------------------------------------------------------------- t_swat_proj_proc
 *
 * armv7 0x00076e78, 408 bytes.  **Complete.**
 *
 * `tl_do_sw_zap`'s own `field38` callback. The free poses animation
 * `0x24` (`find_ani_part2`), steps a frame, tags `field1c=0x13`, and
 * tries `proj_strike_check`; a hit goes straight to the impact
 * (`0xe8a`), a miss waits three ticks under `0xe76`, which is the same
 * strike attempt with `field1c=0x14` instead of `0x13` and no re-pose --
 * a second try at the same box.
 *
 * `0xe8a` is the impact: `stop_a8`, `field20=0`, `field1c=-0xfa`,
 * `field54=3`, `field40 = -0xfa + 0x11e = 0x24` (the same animation,
 * reached by arithmetic rather than the literal), `find_ani_part_a14`,
 * `field1c=3`, and a push of `t_mframew` under `0xe97`, which just
 * installs `tl_delete_proj_and_die`.
 *
 * `0xe7d` is the second strike attempt (`0xe76` parks on it): the same
 * `field1c=0x14` check, a hit falling into the `0xe8a` impact, and a miss
 * parking three ticks on `0xe84`. `0xe84` throws (`0xa0000`/`4`,
 * `set_proj_vel`) and pushes `tl_projectile_flight` under `0xe8a` -- r6, which
 * the dispatch reloaded with 0xe8a on the way to 0xe84 -- so when the flight
 * returns, the projectile goes to the impact state. An earlier reading had
 * 0xe7d push the flight and 0xe84 re-arm 0xe7d; the differential test showed
 * otherwise.
 */
long tl_projectile_flight(MK3THREAD *thread);

long t_swat_proj_proc(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t slot  = *mk3_frame(thread, frame + 1);

    if (slot == 0xe7d) {
        do_next_a9_frame(obj);

        obj->field1c = 0x14;
        proj_strike_check(obj);

        if (obj->field5c != 0)
            goto impact;

        /* park 3 on 0xe84 (0x76fac), not a push (difftest) */
        *mk3_frame(thread, frame + 1) = 0xe84;
        thread->fieldfc = 3;
        return 3;
    }

    if (slot == 0xe8a)
        goto impact;

    if (slot == 0xe97)
        return mk3_install(thread, (MK3THREADFUNC)tl_delete_proj_and_die);

    if (slot == 0xe84) {
        obj->field1c = 0xa0000;
        obj->field20 = 4;
        set_proj_vel(obj);

        obj->field48 = 0x12;

        /* r6: the dispatch reloaded it with 0xe8a on the way here, so the
         * flight returns into the impact state (difftest) */
        *mk3_frame(thread, frame + 1) = 0xe8a;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)tl_projectile_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot == 0xe76) {
        do_next_a9_frame(obj);

        obj->field1c = 0x14;
        proj_strike_check(obj);

        if (obj->field5c != 0)
            goto impact;

        *mk3_frame(thread, frame + 1) = 0xe7d;   /* r6, still 0xe7d from the dispatch (difftest) */
        thread->fieldfc = 3;
        return 3;
    }

    if (slot != 0)
        return -3;

    obj->field40 = 0x24;
    find_ani_part2(obj);

    do_next_a9_frame(obj);

    obj->field1c = 0x13;
    proj_strike_check(obj);

    if (obj->field5c != 0)
        goto impact;

    *mk3_frame(thread, frame + 1) = 0xe76;
    thread->fieldfc = 3;
    return 3;

impact:
    stop_a8(obj->field08);

    obj->field20 = 0;
    obj->field1c = (uint32_t)~0xf9;
    obj->field54 = 3;
    obj->field40 = (uint32_t)~0xf9 + 0x11e;
    find_ani_part_a14(obj);

    obj->field1c = 3;

    *mk3_frame(thread, frame + 1) = 0xe97;
    thread->frame = thread->frame + 1;   /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_mframew;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------------- tl_do_lao_zap
 *
 * armv7 0x00079e5c, 244 bytes.  **Complete.**
 *
 * `tl_do_sw_zap`'s three-state twin -- same `field20`/`a10`/`zap_init_special_act`
 * open and the same `0x00030024` packed rate/animation into `field40` (rate 3,
 * animation 0x24, the spinning-hat clip `t_lao_hat_proc`'s own free branch
 * poses), but with a sound (`ochar_sound`) added before the descent and a
 * second resume state after it, `0xdaf`, that `t_mframew` waits through before
 * the projectile is actually let go.
 *
 *      token == 0:      obj->field20 = 0xc ; obj->a10 = 0
 *                       zap_init_special_act(obj)
 *                       obj->field1c = 1 ; ochar_sound(obj)
 *                       obj->field40 = 0x00030024
 *                       token := 0xdaa, descend into t_animate_a9
 *
 *      token == 0xdaa:  obj->field38 = t_lao_hat_proc
 *                       create_proj_proc(obj)
 *                       obj->field1c = 3
 *                       token := 0xdaf, descend into t_mframew
 *
 *      token == 0xdaf:  obj->field1c = 5
 *                       frame[frame].handler = t_mframew    ; tail, no push
 *
 *      otherwise:       return -3
 *
 * The 0xdaf state reaches its store through a plain branch with no `cbz`
 * refusal in front of it -- `mk3_install`, not `mk3_push_handler`, the same
 * tell `tools/instck.py` looks for: a dispatcher already holding a matched
 * non-zero token cannot re-test the slot for zero.
 */
long tl_do_lao_zap(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0xdaa) {
        obj->field38 = (uint32_t)(uintptr_t)t_lao_hat_proc;
        create_proj_proc(obj);

        obj->field1c = 3;

        *mk3_frame(thread, frame + 1) = 0xdaf;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0xdaf) {
        obj->field1c = 5;
        return mk3_install(thread, (MK3THREADFUNC)t_mframew);
    }

    if (token != 0)
        return -3;

    obj->field20 = 0xc;
    obj->a10     = 0;
    zap_init_special_act(obj);

    obj->field1c = 1;
    ochar_sound(obj);

    obj->field40 = 0x00030024;

    *mk3_frame(thread, frame + 1) = 0xdaa;
    thread->frame = thread->frame + 1;   /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_animate_a9;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------------ tl_ind_zap_proc
 *
 * armv7 0x00076d78, 256 bytes.  **Complete.**
 *
 * A three-state projectile: free throws animation 0x24 at three frames
 * (`find_ani_part_a14`, `do_next_a9_frame`), plays the launch sound, sets a
 * fixed 0xa0000/4 velocity through `set_proj_vel`, tags `field48 = 0x12`, and
 * flies from `0x116b` on `tl_projectile_flight`. `0x116b` re-enters on impact
 * or timeout: `field1c = 0x30004` is the argument `hob_ochar_sound` takes (an
 * unrelated packed value, not a rate -- overwritten moments later), the GrObj
 * stops (`stop_a8`), the same animation 0x24 re-poses at four frames, and the
 * rate that survives into `t_mframew` is a plain 4. `0x1176` is the floor,
 * closing on `tl_delete_proj_and_die`.
 *
 *      slot = frame[frame+1].w0
 *                                          ; 0:     free, then 0x116b
 *                                          ; 0x116b: land pose, then 0x1176
 *                                          ; 0x1176: die
 *      if (slot == 0x116b) {
 *          obj->field1c = 0x30004 ; hob_ochar_sound(obj)
 *          stop_a8(obj->field08)
 *          obj->field54 = 4 ; obj->field40 = 0x24 ; find_ani_part_a14(obj)
 *          obj->field1c = 4
 *          token 0x1176 ; frame++ ; install t_mframew ; return 0
 *      }
 *      if (slot == 0x1176) install tl_delete_proj_and_die ; return 0
 *      if (slot != 0) return -3
 *      obj->field40 = 0x24 ; obj->field54 = 3 ; find_ani_part_a14(obj)
 *      do_next_a9_frame(obj)
 *      obj->field1c = 1 ; ochar_sound(obj)
 *      obj->field1c = 0xa0000 ; obj->field20 = 4 ; set_proj_vel(obj)
 *      obj->field48 = 0x12
 *      token 0x116b ; frame++ ; install tl_projectile_flight ; return 0
 */
void hob_ochar_sound(MK3OBJ *obj);
void find_ani_part_a14(MK3OBJ *obj);
long tl_projectile_flight(struct MK3THREAD *thread);
long do_next_a9_frame(MK3OBJ *obj);

long tl_ind_zap_proc(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t slot  = *mk3_frame(thread, frame + 1);

    if (slot == 0x116b) {
        obj->field1c = 0x30004;
        hob_ochar_sound(obj);

        stop_a8(obj->field08);

        obj->field54 = 4;
        obj->field40 = 0x24;
        find_ani_part_a14(obj);

        obj->field1c = 4;

        *mk3_frame(thread, frame + 1) = 0x1176;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot == 0x1176)
        return mk3_install(thread, (MK3THREADFUNC)tl_delete_proj_and_die);

    if (slot != 0)
        return -3;

    obj->field40 = 0x24;
    obj->field54 = 3;
    find_ani_part_a14(obj);
    do_next_a9_frame(obj);

    obj->field1c = 1;
    ochar_sound(obj);

    obj->field1c = 0xa0000;
    obj->field20 = 4;
    set_proj_vel(obj);

    obj->field48 = 0x12;

    *mk3_frame(thread, frame + 1) = 0x116b;
    thread->frame = thread->frame + 1;   /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)tl_projectile_flight;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ----------------------------------------------------------------- tl_do_ind_zap
 *
 * armv7 0x00079a9c, 288 bytes.  **Complete.**
 *
 * `tl_ind_zap_proc`'s driver, in the same `tl_do_X` -> `t_X_proc` shape as
 * `tl_do_lao_zap`/`t_lao_hat_proc`. The free poses the packed `0x00030024`
 * (rate 3, animation 0x24) and asks `q_his_react_flag_set`; when it answers
 * yes the pose is swapped for `0x00010024` (rate 1, same animation) before
 * the same descent into `t_animate_a9` from `0x1189`. `0x1189` re-enters and
 * launches: `proc->field64` is saved, zeroed (forcing `create_proj_proc` to
 * spawn fresh rather than restart whatever slave that slot names), restored
 * right after the call, and the thrower parks as a sitting duck. `0x1196` is
 * the floor, installing `tl_do_proj_sitting_duck` rather than closing the
 * thread outright -- this driver, unlike most in this file, ends by handing
 * off to another wait rather than dying.
 *
 *      slot = frame[frame+1].w0
 *                                          ; 0:     free, then 0x1189
 *                                          ; 0x1189: launch, then 0x1196
 *                                          ; 0x1196: install the sitting duck
 *      if (slot == 0x1189) {
 *          saved = proc->field64 ; proc->field64 = 0
 *          obj->field38 = tl_ind_zap_proc ; create_proj_proc(obj)
 *          proc->field64 = saved
 *          i_am_a_sitting_duck(obj)
 *          obj->field1c = 3
 *          token 0x1196 ; frame++ ; install t_mframew ; return 0
 *      }
 *      if (slot == 0x1196) {
 *          obj->field20 = 1
 *          install tl_do_proj_sitting_duck ; return 0
 *      }
 *      if (slot != 0) return -3
 *      obj->field20 = 5 ; obj->a10 = 0 ; zap_init_special_act(obj)
 *      obj->field1c = 0 ; ochar_sound(obj)
 *      obj->field40 = 0x00030024
 *      q_his_react_flag_set(obj)
 *      if (obj->field5c != 0) obj->field40 = 0x00010024
 *      token 0x1189 ; frame++ ; install t_animate_a9 ; return 0
 */
long tl_do_proj_sitting_duck(MK3THREAD *thread);

long tl_do_ind_zap(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t slot  = *mk3_frame(thread, frame + 1);

    if (slot == 0x1189) {
        uint32_t saved64 = obj->field00->field64;

        obj->field00->field64 = 0;
        obj->field38 = (uint32_t)(uintptr_t)tl_ind_zap_proc;
        create_proj_proc(obj);
        obj->field00->field64 = saved64;

        i_am_a_sitting_duck(obj);

        obj->field1c = 3;

        *mk3_frame(thread, frame + 1) = 0x1196;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot == 0x1196) {
        obj->field20 = 1;
        return mk3_install(thread, (MK3THREADFUNC)tl_do_proj_sitting_duck);
    }

    if (slot != 0)
        return -3;

    obj->field20 = 5;
    obj->a10     = 0;
    zap_init_special_act(obj);

    obj->field1c = 0;
    ochar_sound(obj);

    obj->field40 = 0x00030024;
    q_his_react_flag_set(obj);
    if (obj->field5c != 0)
        obj->field40 = 0x00010024;

    *mk3_frame(thread, frame + 1) = 0x1189;
    thread->frame = thread->frame + 1;   /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_animate_a9;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------- tl_do_tusk_floor
 *
 * armv7 0x0007b190, 276 bytes.  **Complete.**
 *
 * Four states, one of them a bare wait with no push. The free tags
 * `field20 = 0x1c`, runs `init_special_act` (not `zap_init_special_act` --
 * a different routine of the same shape), poses at rate 4/animation 3
 * (`field40 = 0x00040003`) and descends into `t_animate2_a9` from `0x769`.
 * `0x769` does not push anything: it just re-parks `0x76b` and waits eight
 * ticks. `0x76b` is the launch -- `proc->slave`, `proc->field64` and
 * `obj->field40` are all saved, `slave` and `field64` zeroed (forcing
 * `create_proj_proc` to spawn fresh), `field38 = t_blade_proc`, and once
 * the new slave exists its own proc's `field28` is hooked back to this
 * object's proc before all three saved values are restored. `0x790`, a
 * plain wait rather than a push, follows for twenty-four ticks, and its
 * own re-entry just installs `t_mframew`.
 *
 *      slot = frame[frame+1].w0
 *                                          ; 0:    free, then 0x769
 *                                          ; 0x769: wait, then 0x76b
 *                                          ; 0x76b: launch, then 0x790 (wait)
 *                                          ; 0x790: install t_mframew
 *      if (slot == 0x769) { token 0x76b ; fieldfc = 8 ; return 8 }
 *      if (slot < 0x769) {
 *          if (slot != 0) return -3
 *          obj->field20 = 0x1c ; init_special_act(obj)
 *          obj->field1c = 6 ; ochar_sound(obj)
 *          obj->field40 = 0x00040003
 *          token 0x769 ; frame++ ; install t_animate2_a9 ; return 0
 *      }
 *      if (slot == 0x76b) {
 *          saved = proc->slave, proc->field64, obj->field40
 *          proc->slave = 0 ; proc->field64 = 0
 *          obj->field38 = t_blade_proc
 *          slave = create_proj_proc(obj)
 *          slave->field00->field28 = obj->field00
 *          proc->slave, proc->field64, obj->field40 = saved
 *          token 0x790 ; fieldfc = 0x18 ; return 0x18
 *      }
 *      if (slot != 0x790) return -3
 *      obj->field1c = 3 ; install t_mframew ; return 0
 */
long t_animate2_a9(struct MK3THREAD *thread);            /* pointer slot 0x000f36c0 */
long t_blade_proc(struct MK3THREAD *thread);

long tl_do_tusk_floor(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t slot  = *mk3_frame(thread, frame + 1);

    if (slot == 0x769) {
        *mk3_frame(thread, frame + 1) = 0x76b;
        thread->fieldfc = 8;
        return 8;
    }

    if (slot < 0x769) {
        if (slot != 0)
            return -3;

        obj->field20 = 0x1c;
        init_special_act(obj);

        obj->field1c = 6;
        ochar_sound(obj);

        obj->field40 = 0x00040003;

        *mk3_frame(thread, frame + 1) = 0x769;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_animate2_a9;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot == 0x76b) {
        MK3OBJPROC *proc         = obj->field00;
        uint32_t    saved_slave  = proc->slave;
        uint32_t    saved64      = proc->field64;
        uint32_t    saved40      = obj->field40;
        MK3OBJ     *slave;

        proc->slave           = 0;
        obj->field00->field64 = 0;
        obj->field38 = (uint32_t)(uintptr_t)t_blade_proc;
        slave = create_proj_proc(obj);

        slave->field00->field28 = (uint32_t)(uintptr_t)obj->field00;

        obj->field00->slave   = saved_slave;
        obj->field00->field64 = saved64;
        obj->field40          = saved40;

        *mk3_frame(thread, frame + 1) = 0x790;
        thread->fieldfc = 0x18;
        return 0x18;
    }

    if (slot != 0x790)
        return -3;

    obj->field1c = 3;
    return mk3_install(thread, (MK3THREADFUNC)t_mframew);
}


/* --------------------------------------------------------------------- t_blade_proc
 *
 * armv7 0x00077f7c, 364 bytes.  **Complete.**
 *
 * `tl_do_tusk_floor`'s own `field38` callback -- Tusk's spinning blade.
 * The free tags the GrObj's own thread (`obj->thread->pid = 0x206`, not the
 * blade's proc), reads the stage floor into both `field20` and the GrObj's
 * `field0e`... no, `field12` (`G+0xac`), flips the multi, and averages a
 * pair of `G` words (`G[0x468]` and `G[0x470]`, a signed divide-by-two with
 * the standard round-toward-zero adjustment) into the GrObj's `field0e`.
 * It repositions (`multi_adjust_xy` at `field1c=-0xc8`), poses, throws
 * (`0x80000`/`2`, `set_proj_vel`), and arms a sixteen-tick countdown in
 * `a10` before waiting one tick under `0x74f`.
 *
 * `0x74f` counts `a10` down every tick (`next_anirate` first): while it is
 * still positive it just re-arms itself; once it reaches zero it waits
 * once more under `0x757` and pushes `t_saw_strike_check` -- the same
 * strike routine `t_sz_zap_hit` reached earlier in this file, here doing
 * duty for a spinning blade instead of an ice collision.
 *
 * `0x757` pushes `t_saw_strike_check` again under `0x758`; `0x758` is the
 * exit watch: `proj_onscreen_test` -- answered through `field5c`, not the
 * return value -- installs `tl_delete_proj_and_die` the instant the blade
 * leaves the screen, or just `next_anirate`s and falls straight into
 * `0x750`'s own "counter finished" tail to re-arm `0x757` while it is
 * still in view.
 */
long t_saw_strike_check(struct MK3THREAD *thread);
long proj_onscreen_test(MK3OBJ *obj);
long tl_delete_proj_and_die(struct MK3THREAD *thread);

long t_blade_proc(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t slot  = *mk3_frame(thread, frame + 1);

    if (slot == 0x750) {
        next_anirate(obj);

        obj->a10 = obj->a10 - 1;
        if ((int32_t)obj->a10 <= 0) {
            *mk3_frame(thread, frame + 1) = 0x757;
            thread->fieldfc = 1;
            return 1;
        }

        *mk3_frame(thread, frame + 1) = 0x74f;
        thread->fieldfc = 1;
        return 1;
    }

    if (slot == 0x757) {
        *mk3_frame(thread, frame + 1) = 0x758;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_saw_strike_check;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot == 0x758) {
        proj_onscreen_test(obj);
        if (obj->field5c == 0)
            return mk3_install(thread, (MK3THREADFUNC)tl_delete_proj_and_die);

        next_anirate(obj);

        *mk3_frame(thread, frame + 1) = 0x757;
        thread->fieldfc = 1;
        return 1;
    }

    if (slot == 0x74f) {
        *mk3_frame(thread, frame + 1) = 0x750;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_saw_strike_check;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot != 0)
        return -3;

    obj->thread->pid = 0x206;

    obj->field20           = *(uint32_t *)(G_BYTES + 0xac);
    MK3_SET_FIELD12(obj->field08, obj->field20);

    flip_multi(obj);

    {
        uint32_t sum = *(uint32_t *)(G_BYTES + 0x468)
                       + *(uint32_t *)(G_BYTES + 0x470);
        uint32_t mid = (uint32_t)(((int32_t)sum + (int32_t)(sum >> 31)) >> 1);
        MK3_SET_FIELD0E(obj->field08, mid);
    }

    obj->field20 = 0;
    obj->field1c = (uint32_t)~0xc7;
    multi_adjust_xy(obj);

    obj->field40 = 2;
    get_char_ani2(obj);

    obj->field1c = 0x80000;
    obj->field20 = 2;
    set_proj_vel(obj);

    obj->field1c = 5;
    ochar_sound(obj);

    obj->a10 = 0x10;

    *mk3_frame(thread, frame + 1) = 0x74f;
    thread->fieldfc = 1;
    return 1;
}


/* --------------------------------------------------------- tl_tusk_ground_zap
 *
 * armv7 0x0007a09c, 276 bytes.  **Complete.**
 *
 * The free tags `field20 = 0xe`, zeroes `a10`, runs `zap_init_special_act`,
 * packs `field40 = 0x00030024` (rate 3, animation 0x24, the same pair
 * `tl_do_lao_zap`/`tl_do_sw_zap` use) and descends into `t_animate_a9` from
 * `0xbb9`. `0xbb9` is the launch: `field38 = t_photon_proc` then
 * `create_proj_proc`, and if that actually returned a slave (not NULL) its
 * GrObj is saved into `field30` and `adjust_xy_a5` nudges it by `0x3f`
 * (`field1c = 0x10`, `field20 = 0x10 + 0x2f`) before the launch sound and
 * `i_am_a_sitting_duck` park the thrower. `0xbd7` is the floor:
 * `delete_slave` on the way out, then `t_mframew` closes it.
 *
 *      slot = frame[frame+1].w0
 *                                          ; 0:    free, then 0xbb9
 *                                          ; 0xbb9: launch, then 0xbd7
 *                                          ; 0xbd7: delete_slave, then die
 *      if (slot == 0xbb9) {
 *          obj->field38 = t_photon_proc
 *          slave = create_proj_proc(obj)
 *          if (slave != NULL) {
 *              obj->field1c = 0x10 ; obj->field30 = slave->field08
 *              obj->field20 = 0x3f ; adjust_xy_a5(obj)
 *          }
 *          obj->field1c = 1 ; ochar_sound(obj)
 *          i_am_a_sitting_duck(obj)
 *          obj->field1c = 4
 *          token 0xbd7 ; frame++ ; install t_mframew ; return 0
 *      }
 *      if (slot == 0xbd7) {
 *          delete_slave(obj) ; obj->field1c = 5
 *          install t_mframew ; return 0
 *      }
 *      if (slot != 0) return -3
 *      obj->field20 = 0xe ; obj->a10 = 0 ; zap_init_special_act(obj)
 *      obj->field40 = 0x00030024
 *      token 0xbb9 ; frame++ ; install t_animate_a9 ; return 0
 */
void delete_slave(MK3OBJ *obj);
void adjust_xy_a5(MK3OBJ *obj);
void i_am_a_sitting_duck(MK3OBJ *obj);
long t_photon_proc(struct MK3THREAD *thread);
MK3OBJ *create_proj_proc(MK3OBJ *obj);

long tl_tusk_ground_zap(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t slot  = *mk3_frame(thread, frame + 1);

    if (slot == 0xbb9) {
        MK3OBJ *slave;

        obj->field38 = (uint32_t)(uintptr_t)t_photon_proc;
        slave = create_proj_proc(obj);
        if (slave != NULL) {
            obj->field1c = 0x10;
            obj->field30 = (uint32_t)(uintptr_t)slave->field08;
            obj->field20 = 0x10 + 0x2f;
            adjust_xy_a5(obj);
        }

        obj->field1c = 1;
        ochar_sound(obj);

        i_am_a_sitting_duck(obj);

        obj->field1c = 4;

        *mk3_frame(thread, frame + 1) = 0xbd7;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot == 0xbd7) {
        delete_slave(obj);
        obj->field1c = 5;
        return mk3_install(thread, (MK3THREADFUNC)t_mframew);
    }

    if (slot != 0)
        return -3;

    obj->field20 = 0xe;
    obj->a10     = 0;
    zap_init_special_act(obj);

    obj->field40 = 0x00030024;

    *mk3_frame(thread, frame + 1) = 0xbb9;
    thread->frame = thread->frame + 1;   /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_animate_a9;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------------------- tl_do_tusk_zap
 *
 * armv7 0x0007b45c, 348 bytes.  **Complete.**
 *
 * The ground and air forms fork at the very first check, before a single
 * field is touched -- the same shape `tl_do_mileena_zap` uses: grounded,
 * this tail-installs `tl_tusk_ground_zap` outright; airborne, it runs its
 * own pose (`zap_air_init_special`, `field20 = obj->field00->field18 =
 * 0x16`) and descends into `t_mframew` from `0xbef`. `0xbef` is the
 * launch -- `field38 = t_photon_proc` into `create_proj_proc`, nudging by
 * `field20 = 0x10` only when a slave actually exists -- then a knockback
 * (`away_x_vel` at `0x60000`) and a bare six-tick wait for `0xc09`. `0xc09`
 * knocks back again (`0x40000`) and pushes a plain `t_mframew` wait from
 * `0xc0d`, whose own re-entry installs `t_drop_down_land`.
 *
 *      slot = frame[frame+1].w0
 *                                          ; 0:    free; grounded -> tl_tusk_ground_zap
 *                                          ;       airborne, then 0xbef
 *                                          ; 0xbef: launch, then 0xc09 (wait)
 *                                          ; 0xc09: wait, then 0xc0d
 *                                          ; 0xc0d: install t_drop_down_land
 *      if (slot == 0xbef) {
 *          slave = create_proj_proc(obj) after obj->field38 = t_photon_proc
 *          if (slave != NULL) {
 *              obj->field1c = 0 ; obj->field30 = slave->field08
 *              obj->field20 = 0x10 ; adjust_xy_a5(obj)
 *          }
 *          obj->field1c = 0x60000 ; away_x_vel(obj)
 *          token 0xc09 ; fieldfc = 6 ; return 6
 *      }
 *      if (slot < 0xbef) {
 *          if (slot != 0) return -3
 *          if (!am_i_airborn(obj)) install tl_tusk_ground_zap ; return 0
 *          obj->a10 = 0 ; zap_air_init_special(obj)
 *          obj->field20 = obj->field00->field18 = 0x16
 *          obj->field1c = 1 ; ochar_sound(obj)
 *          obj->field40 = 0 ; get_char_ani2(obj) ; obj->field1c = 2
 *          token 0xbef ; frame++ ; install t_mframew ; return 0
 *      }
 *      if (slot == 0xc09) {
 *          obj->field1c = 0x40000 ; away_x_vel(obj) ; obj->field1c = 4
 *          token 0xc0d ; frame++ ; install t_mframew ; return 0
 *      }
 *      if (slot != 0xc0d) return -3
 *      install t_drop_down_land ; return 0
 */
long tl_tusk_ground_zap(MK3THREAD *thread);
long t_photon_proc(struct MK3THREAD *thread);
long t_drop_down_land(struct MK3THREAD *thread);        /* pointer slot 0x000f33d4 */

long tl_do_tusk_zap(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t slot  = *mk3_frame(thread, frame + 1);

    if (slot == 0xbef) {
        MK3OBJ *slave;

        obj->field38 = (uint32_t)(uintptr_t)t_photon_proc;
        slave = create_proj_proc(obj);

        if (slave != NULL) {
            obj->field1c = 0;
            obj->field30 = (uint32_t)(uintptr_t)slave->field08;
            obj->field20 = 0x10;
            adjust_xy_a5(obj);
        }

        obj->field1c = 0x60000;
        away_x_vel(obj);

        *mk3_frame(thread, frame + 1) = 0xc09;
        thread->fieldfc = 6;
        return 6;
    }

    if (slot < 0xbef) {
        if (slot != 0)
            return -3;

        if (am_i_airborn(obj) == 0)
            return mk3_install(thread, (MK3THREADFUNC)tl_tusk_ground_zap);

        obj->a10 = 0;
        zap_air_init_special(obj);

        obj->field20 = 0x16;
        obj->field00->field18 = 0x16;
        obj->field1c = 1;
        ochar_sound(obj);

        obj->field40 = 0;
        get_char_ani2(obj);
        obj->field1c = 2;

        *mk3_frame(thread, frame + 1) = 0xbef;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot == 0xc09) {
        obj->field1c = 0x40000;
        away_x_vel(obj);

        obj->field1c = 4;

        *mk3_frame(thread, frame + 1) = 0xc0d;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot != 0xc0d)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_drop_down_land);
}


/* ------------------------------------------------------------- t_kano_zap_proc
 *
 * armv7 0x0007667c, 312 bytes.  **Complete.**
 *
 * A melee check that only becomes a projectile when it misses. The free sets
 * up a box on `local_strike_check_box` (`field1c = 0x15`, `field20 =
 * 0x00030060`, `field24 = 0x00220044`, transcribed as the raw packed words
 * the binary loads, not decoded further) and tests it immediately, no yield
 * in between. A connect there re-tags `field1c = 0x38`/`field20 = 0x20` and
 * falls straight into the shared close/impact tail; a miss instead poses
 * animation `0x3f`, throws at a flat `0x80000`/`1` velocity, tags
 * `field48 = 0x15` (the same reload the free branch put in `field1c`), and
 * flies on `tl_projectile_flight` from `0x1301`. `0x1301` is the re-entry
 * after that flight -- `stop_a8`, `field1c = 0xd`, `field20 = 0` -- which
 * joins the SAME tail the immediate connect uses: `multi_adjust_xy`, a
 * packed `hob_ochar_sound` argument (`0x20003`), animation `0x3f` again at
 * three frames, and a `t_mframew` wait from `0x1312`. `0x1312` is the floor,
 * closing on `tl_delete_proj_and_die`.
 *
 *      slot = frame[frame+1].w0
 *                                          ; 0:     free; hit -> tail, miss -> 0x1301
 *                                          ; 0x1301: flight re-entry, then tail -> 0x1312
 *                                          ; 0x1312: die
 *      if (slot == 0x1301) {
 *          stop_a8(obj->field08)
 *          obj->field1c = 0xd ; obj->field20 = 0
 *          goto tail
 *      }
 *      if (slot == 0x1312) install tl_delete_proj_and_die ; return 0
 *      if (slot != 0) return -3
 *      obj->field1c = 0x15
 *      obj->field20 = 0x00030060 ; obj->field24 = 0x00220044
 *      local_strike_check_box(obj)
 *      if (obj->field5c != 0) {
 *          obj->field1c = 0x38 ; obj->field20 = 0x20
 *          goto tail
 *      }
 *      obj->field40 = 0x3f ; find_ani_part2(obj)
 *      obj->field1c = 0x80000 ; obj->field20 = 1 ; set_proj_vel(obj)
 *      obj->field48 = 0x15
 *      token 0x1301 ; frame++ ; install tl_projectile_flight ; return 0
 *
 *      tail:  multi_adjust_xy(obj)
 *             obj->field1c = 0x20003 ; hob_ochar_sound(obj)
 *             obj->field40 = 0x3f ; obj->field54 = 3 ; find_ani_part_a14(obj)
 *             obj->field1c = 4
 *             token 0x1312 ; frame++ ; install t_mframew ; return 0
 */
void find_ani_part2(MK3OBJ *obj);

long t_kano_zap_proc(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t slot  = *mk3_frame(thread, frame + 1);

    if (slot == 0x1301) {
        stop_a8(obj->field08);
        obj->field1c = 0xd;
        obj->field20 = 0;
        goto tail;
    }

    if (slot == 0x1312)
        return mk3_install(thread, (MK3THREADFUNC)tl_delete_proj_and_die);

    if (slot != 0)
        return -3;

    obj->field1c = 0x15;
    obj->field20 = 0x00030060;
    obj->field24 = 0x00220044;
    local_strike_check_box(obj);

    if (obj->field5c != 0) {
        obj->field1c = 0x38;
        obj->field20 = 0x20;
        goto tail;
    }

    obj->field40 = 0x3f;
    find_ani_part2(obj);

    obj->field1c = 0x80000;
    obj->field20 = 1;
    set_proj_vel(obj);

    obj->field48 = 0x15;

    *mk3_frame(thread, frame + 1) = 0x1301;
    thread->frame = thread->frame + 1;   /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)tl_projectile_flight;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;

tail:
    multi_adjust_xy(obj);

    obj->field1c = 0x20003;
    hob_ochar_sound(obj);

    obj->field40 = 0x3f;
    obj->field54 = 3;
    find_ani_part_a14(obj);

    obj->field1c = 4;

    *mk3_frame(thread, frame + 1) = 0x1312;
    thread->frame = thread->frame + 1;   /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_mframew;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------------- tl_do_kano_zap
 *
 * armv7 0x000795a4, 288 bytes.  **Complete.**
 *
 * `t_kano_zap_proc`'s driver, four states rather than the usual two or
 * three. The free poses animation 0x24 through `pose_a9_manual` and
 * self-resumes at `0x1320` after three ticks -- no push, a bare wait like
 * `tl_do_tusk_floor`'s `0x769`. `0x1320` is `setup_proj_obj` and descends
 * into `t_double_mframew` from `0x1324`; `0x1324` is the launch itself,
 * `field38 = t_kano_zap_proc` into `create_proj_proc`, then a plain
 * `t_mframew` wait from `0x1329`. `0x1329` closes it: `field1c = G + 0x440`
 * into `update_tsl`, then `tl_do_proj_sitting_duck` rather than a straight
 * die, the same ending `tl_do_ind_zap` uses.
 *
 *      slot = frame[frame+1].w0
 *                                          ; 0:     free, then 0x1320 (wait)
 *                                          ; 0x1320: setup, then 0x1324
 *                                          ; 0x1324: launch, then 0x1329
 *                                          ; 0x1329: install the sitting duck
 *      if (slot == 0x1320) {
 *          setup_proj_obj(obj) ; obj->field1c = 3
 *          token 0x1324 ; frame++ ; install t_double_mframew ; return 0
 *      }
 *      if (slot < 0x1320) {
 *          if (slot != 0) return -3
 *          obj->a10 = 0 ; obj->field20 = 1 ; zap_init_special_act(obj)
 *          obj->field1c = 1 ; ochar_sound(obj)
 *          obj->field40 = 0x24 ; pose_a9_manual(obj)
 *          token 0x1320 ; fieldfc = 3 ; return 3
 *      }
 *      if (slot == 0x1324) {
 *          obj->field38 = t_kano_zap_proc ; create_proj_proc(obj)
 *          obj->field1c = 4
 *          token 0x1329 ; frame++ ; install t_mframew ; return 0
 *      }
 *      if (slot != 0x1329) return -3
 *      obj->field1c = G + 0x440 ; update_tsl(obj)
 *      obj->field20 = 0x18
 *      install tl_do_proj_sitting_duck ; return 0
 */
void setup_proj_obj(MK3OBJ *obj);
void pose_a9_manual(MK3OBJ *obj);
long t_double_mframew(struct MK3THREAD *thread);       /* pointer slot 0x000f36a8 */

long tl_do_kano_zap(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t slot  = *mk3_frame(thread, frame + 1);

    if (slot == 0x1320) {
        setup_proj_obj(obj);
        obj->field1c = 3;

        *mk3_frame(thread, frame + 1) = 0x1324;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_double_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot < 0x1320) {
        if (slot != 0)
            return -3;

        obj->a10    = 0;
        obj->field20 = 1;
        zap_init_special_act(obj);

        obj->field1c = 1;
        ochar_sound(obj);

        obj->field40 = 0x24;
        pose_a9_manual(obj);

        *mk3_frame(thread, frame + 1) = 0x1320;
        thread->fieldfc = 3;
        return 3;
    }

    if (slot == 0x1324) {
        obj->field38 = (uint32_t)(uintptr_t)t_kano_zap_proc;
        create_proj_proc(obj);

        obj->field1c = 4;

        *mk3_frame(thread, frame + 1) = 0x1329;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot != 0x1329)
        return -3;

    obj->field1c = (uint32_t)(uintptr_t)((char *)G + 0x440);
    update_tsl(obj);

    obj->field20 = 0x18;

    return mk3_install(thread, (MK3THREADFUNC)tl_do_proj_sitting_duck);
}


/* ----------------------------------------------------------- tl_do_lia_forward_zap
 *
 * armv7 0x0007a85c, 288 bytes.  **Complete.**
 *
 * The free packs `0x00030024` into `field40` (rate 3, animation 0x24) and
 * asks `get_his_action`; a `0x507` answer (a reaction) swaps in
 * `0x00010024` and tags `field48 = 1` before the same descent into
 * `t_animate_a9` from `0x7e1`. `0x7e1` launches through `create_proj_proc`
 * with `field38 = t_lia_forward_proc`, and when a slave actually exists its
 * `field48` gets a flat `0x80000` or, when this object's OWN `field48` was
 * tagged from the reaction check, `0xa0000` instead -- the two speeds a
 * caught-mid-reaction throw and an ordinary one get. It closes on a bare
 * wait (`field1c = obj->field00->field18 = 0x604`, twenty ticks, no push)
 * before `0x800` installs `t_backwards_ani`.
 *
 *      slot = frame[frame+1].w0
 *                                          ; 0:    free, then 0x7e1
 *                                          ; 0x7e1: launch, then 0x800 (wait)
 *                                          ; 0x800: install t_backwards_ani
 *      if (slot == 0x7e1) {
 *          obj->field40 = 0x24 ; find_ani_part2(obj)
 *          obj->field38 = t_lia_forward_proc
 *          slave = create_proj_proc(obj)
 *          if (slave != NULL) {
 *              obj->field20 = 0x80000
 *              if (obj->field48 != 0) obj->field20 = 0xa0000
 *              slave->field48 = obj->field20
 *          }
 *          obj->field1c = obj->field00->field18 = 0x604
 *          token 0x800 ; fieldfc = 0x20 ; return 0x20
 *      }
 *      if (slot == 0x800) {
 *          obj->field40 = 0x24 ; obj->field1c = 4
 *          install t_backwards_ani ; return 0
 *      }
 *      if (slot != 0) return -3
 *      obj->field20 = 0x1b ; obj->a10 = 0 ; zap_init_special_act(obj)
 *      obj->field1c = 2 ; ochar_sound(obj)
 *      obj->field48 = 0 ; obj->field40 = 0x00030024
 *      get_his_action(obj)
 *      if (obj->field20 == 0x507) {
 *          obj->field48 = 1 ; obj->field40 = 0x00010024
 *      }
 *      token 0x7e1 ; frame++ ; install t_animate_a9 ; return 0
 */
void find_ani_part2(MK3OBJ *obj);
void get_his_action(MK3OBJ *obj);
long t_lia_forward_proc(struct MK3THREAD *thread);
long t_backwards_ani(struct MK3THREAD *thread);         /* pointer slot 0x000f37c4 */

long tl_do_lia_forward_zap(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t slot  = *mk3_frame(thread, frame + 1);

    if (slot == 0x7e1) {
        MK3OBJ *slave;

        obj->field40 = 0x24;
        find_ani_part2(obj);

        obj->field38 = (uint32_t)(uintptr_t)t_lia_forward_proc;
        slave = create_proj_proc(obj);

        if (slave != NULL) {
            obj->field20 = 0x80000;
            if (obj->field48 != 0)
                obj->field20 = 0xa0000;
            slave->field48 = obj->field20;
        }

        obj->field1c = 0x604;
        obj->field00->field18 = 0x604;

        *mk3_frame(thread, frame + 1) = 0x800;
        thread->fieldfc = 0x20;
        return 0x20;
    }

    if (slot == 0x800) {
        obj->field40 = 0x24;
        obj->field1c = 4;
        return mk3_install(thread, (MK3THREADFUNC)t_backwards_ani);
    }

    if (slot != 0)
        return -3;

    obj->field20 = 0x1b;
    obj->a10     = 0;
    zap_init_special_act(obj);

    obj->field1c = 2;
    ochar_sound(obj);

    obj->field48 = 0;
    obj->field40 = 0x00030024;
    get_his_action(obj);
    if (obj->field20 == 0x507) {
        obj->field48 = 1;
        obj->field40 = 0x00010024;
    }

    *mk3_frame(thread, frame + 1) = 0x7e1;
    thread->frame = thread->frame + 1;   /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_animate_a9;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------------------- t_lia_forward_proc
 *
 * armv7 0x0007c1e4, 436 bytes.  **Complete.**
 *
 * `tl_do_lia_forward_zap`'s own `field38` callback. The free lines up
 * (`multi_adjust_xy` at `field1c=8`/`field20=0x20`) and picks a fall
 * shape -- `0x30001`, or `0x10001` when `field48` isn't the standard
 * `0x80000` throw -- before pushing `t_animate_a0_frames` under `0x7a4`,
 * the same pointer-slot borrow every generic "run this and come back" site
 * in this file uses.
 *
 * `0x7a4` is the strike attempt: `local_strike_check_box` against a box
 * packed from raw literals (`field20=0x6004b`, `field24=0x130028`). A miss
 * re-arms the throw (`init_anirate`, `set_proj_vel` off the SAME `field48`
 * it's about to overwrite with `0x15`) and descends into
 * `tl_projectile_flight` under `0x7b2` for a genuine retry; a hit falls
 * straight through into `0x7b2`'s OWN code without ever re-arming the
 * throw -- one physical strike-resolution block reached two ways.
 *
 * `0x7b2` (hit or fallthrough) is the impact: lines the part up on the
 * opponent (`lineup_a0_onto_a1`), `flip_multi`, repositions
 * (`multi_adjust_xy` at `field1c=-0x80`), knocks the GrObj's own Y
 * (`MK3_SET_FIELD12`) to `G+0xac - 0xe0` -- the same landing-floor read
 * every other site uses, just for a knockback instead of a fall --
 * `shake_a11`, `stop_a8`, and poses animation `0x3f` before nudging
 * `field40` on by `0xc` (a second, later frame in the same sequence) and
 * pushing `t_mframew` under `0x7cb`, which just installs
 * `tl_delete_proj_and_die`.
 */
void local_strike_check_box(MK3OBJ *obj);
void lineup_a0_onto_a1(MK3OBJ *obj);
void shake_a11(MK3OBJ *obj);
void stop_a8(MK3OBJ *part);
long t_animate_a0_frames(MK3THREAD *thread);     /* pointer slot 0x000f36b8 */
long tl_projectile_flight(MK3THREAD *thread);

long t_lia_forward_proc(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t slot  = *mk3_frame(thread, frame + 1);

    if (slot == 0x7a4) {
        obj->field1c = 0x15;
        obj->field20 = 0x6004b;
        obj->field24 = 0x130028;
        local_strike_check_box(obj);

        if (obj->field5c == 0) {
            obj->field1c = 4;
            init_anirate(obj);

            obj->field1c = obj->field48;
            set_proj_vel(obj);

            obj->field48 = 0x15;

            *mk3_frame(thread, frame + 1) = 0x7b2;
            thread->frame = thread->frame + 1;   /* push a level */
            mk3_frame(thread, thread->frame)[1] =
                (uint32_t)(uintptr_t)tl_projectile_flight;
            *mk3_frame(thread, thread->frame + 1) = 0;
            return 0;
        }
    } else if (slot != 0x7b2) {
        if (slot == 0x7cb)
            return mk3_install(thread, (MK3THREADFUNC)tl_delete_proj_and_die);

        if (slot != 0)
            return -3;

        obj->field1c = 8;
        obj->field20 = 8 + 0x18;
        multi_adjust_xy(obj);

        obj->field1c = 0x30001;
        if (obj->field48 != 0x80000)
            obj->field1c = 0x10001;

        *mk3_frame(thread, frame + 1) = 0x7a4;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_animate_a0_frames;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    obj->field1c = 3;
    ochar_sound(obj);

    obj->field20 = obj->field00->him;
    obj->field1c = (uint32_t)(uintptr_t)obj->field08;
    lineup_a0_onto_a1(obj);

    flip_multi(obj);

    obj->field20 = 0;
    obj->field1c = (uint32_t)~0x7f;
    multi_adjust_xy(obj);

    {
        int32_t knockback = *(int32_t *)(G_BYTES + 0xac) - 0xe0;
        obj->field1c = (uint32_t)knockback;
        MK3_SET_FIELD12(obj->field08, (uint32_t)knockback);
    }

    obj->field48 = 0x40008;
    shake_a11(obj);

    stop_a8(obj->field08);

    obj->field40 = 0x3f;
    find_ani_part2(obj);

    obj->field40 = obj->field40 + 0xc;
    obj->field1c = 2;

    *mk3_frame(thread, frame + 1) = 0x7cb;
    thread->frame = thread->frame + 1;   /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_mframew;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* -------------------------------------------------------------------- point_rocket
 *
 * armv7 0x000782e0, 284 bytes.  **Complete.**
 *
 * Points a homing rocket's sprite at its own velocity. The GrObj's
 * `field18`/`field1c` (X/Y velocity) set the facing bit
 * (`field28 & 0x10`, the same flip `flip_multi` uses) from the sign of X,
 * then `atan2(|X|, -Y)` gives an angle in `[0, PI]` -- always
 * non-negative because the first argument is an absolute value -- scaled
 * to a bucket `0..8` (`angle * 8 / PI`, rounded). Each bucket writes a
 * different offset into `_robo_ani_data` (0x0015b1d0, reached through the
 * pointer slot at 0x000f33c0 -- see `docs/ANIMATION-STREAMS.md`, the same
 * table the ninja-robot animations share) into `obj->field40`; buckets 0
 * and 1 share one offset, and 7 and 8 share another, which is what a
 * mirrored pair of near-horizontal angles collapsing onto the same sprite
 * looks like.
 *
 *      grobj = obj->field08
 *      if (grobj->field18 < 0) grobj->field28 |= 0x10
 *      else                    grobj->field28 &= ~0x10
 *      angle  = atan2(|grobj->field18|, -grobj->field1c)
 *      bucket = (int)(angle * 8.0 / PI + 0.5)
 *      if ((uint)bucket > 8) return
 *      obj->field40 = robo_ani_data + { 0x1500, 0x1500, 0x1514, 0x1528,
 *                                        0x153c, 0x1578, 0x1564, 0x1550,
 *                                        0x1550 }[bucket]
 */
extern uint32_t robo_ani_data[];   /* pointer slot 0x000f33c0 -> 0x0015b1d0 */
double atan2(double y, double x);

void point_rocket(MK3OBJ *obj)
{
    MK3OBJ  *grobj = obj->field08;
    int32_t  vx    = (int32_t)grobj->field18;
    int32_t  abs_vx;
    double   angle;
    float    bucket_f;
    int32_t  bucket;
    static const uint32_t offsets[9] = {
        0x1500, 0x1500, 0x1514, 0x1528, 0x153c, 0x1578, 0x1564, 0x1550, 0x1550
    };

    if (vx < 0)
        grobj->field28 |= 0x10u;
    else
        grobj->field28 &= ~0x10u;

    abs_vx = (vx < 0) ? -vx : vx;

    angle = atan2((double)abs_vx, (double)-(int32_t)grobj->field1c);
    bucket_f = (float)((angle * 8.0) / 3.141592653589793);
    bucket = (int32_t)((double)bucket_f + 0.5);

    if ((uint32_t)bucket > 8)
        return;

    obj->field40 = (uint32_t)((uintptr_t)robo_ani_data + offsets[bucket]);
}


/* t_rocket_explode -- armv7 0x00077ccc, 176 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *
 *      obj->field20 = him->field24
 *      if (him->field24 != 0x18) {
 *          get_his_action(obj)                  ; answers in 0x20
 *          if (obj->field20 != 0x402) goto hit
 *      }
 *
 *      reflect:  obj->field1c = 0x12
 *                strike_check_a0_test(obj)
 *                if (obj->field5c == 0) goto hit
 *                KillProc(proc->field78)
 *                obj->a10 = proc->him
 *                benedict_arnold_projectile(obj)
 *                frame[frame].handler = t_rocket2_proc
 *
 *      hit:      obj->field1c = 0x12
 *                proj_strike_check(obj)
 *                KillProc(proc->field78)
 *                frame[frame].handler = t_rocket_explode_fx
 *
 * **This is what `benedict_arnold_projectile` is for: the rocket gets reflected
 * and comes back at whoever fired it.** Two conditions send it down that path --
 * the opponent is **character 0x18** (Motaro), or the opponent is performing
 * **action 0x402**. Either way the projectile's proc has its `him` and
 * `field00` swapped for the other fighter's, and it carries on flying under
 * `t_rocket2_proc`.
 *
 * So the traitor routine, read a hundred functions ago with nothing but its name
 * to go on, is the projectile-reflect. Motaro reflects by identity; everyone else
 * has to be doing 0x402 at the moment of contact.
 *
 * **An eighth hard-coded character number, and anonymous again.** 0x18 appears
 * here and in `proj_strike_check`, in two routines whose names say nothing about
 * Motaro, while `is_he_motaro` sits in the same file doing exactly this test
 * with a name. Issue #29.
 *
 * **The reflect still has to connect.** `strike_check_a0_test` is the
 * non-committing twin of `strike_check_a0` -- the pair differ only in a flag --
 * so the routine asks "would this hit?" and falls through to the ordinary
 * explosion when the answer is no. A Motaro standing out of range does not
 * reflect anything.
 *
 * `KillProc(proc->field78)` happens on **both** paths, so whatever 0x78 holds
 * dies whether the rocket is reflected or spent. It is not the slave -- that is
 * 0x64 -- and nothing in the tree writes it; the field is added to the header
 * with the gap said out loud.
 *
 * `obj->field20` carries the character number, then `get_his_action` overwrites
 * it with the action, and the second test reads it back. One field, two
 * questions, four instructions apart -- and a transcription that kept them in
 * separate variables would lose the fact that the helper answers there.
 */
void get_his_action(MK3OBJ *obj);
long strike_check_a0_test(MK3OBJ *obj);
void KillProc(MK3OBJ *obj);
void proj_strike_check(MK3OBJ *obj);
void benedict_arnold_projectile(MK3OBJ *obj);
long t_rocket_explode_fx(MK3THREAD *thread);
long t_rocket2_proc(MK3THREAD *thread);


/* ---------------------------------------------------------------------- t_rocket_hunt
 *
 * armv7 0x000783fc, 468 bytes.  **Complete.**
 *
 * The homing missile's own guidance, and the densest arithmetic in this
 * file: two `get_rough_hypotenuse` calls and four `___divsi3` runtime
 * divides (this binary has no hardware integer divide) to turn a
 * distance vector into a fixed-`5`-magnitude velocity, blended `15/16`
 * against whatever the rocket was already doing (`vx - vx/16 + nx`) so
 * the turn is smoothed rather than snapped. Close enough (rough distance
 * `<= 9`) always explodes, whichever of the three paths that reach
 * `explode` got there. `slot == 0` additionally does a pop-and-reinstall
 * of itself one level down before running the SAME tracking math --
 * transcribed with the exact dead intermediate stores the binary does,
 * the same "shared tail reached two ways" shape this session's other
 * functions kept finding, just with the tail's own effect harder to see
 * at a glance because both paths feed it the same `t_rocket_hunt`
 * self-reference either way.
 *
 *      slot = frame[frame+1].w0
 *      if (slot != 0) {
 *          if (slot != 0x1076) return -3
 *          point_rocket(obj) ; next_anirate(obj)
 *          if (G[0x450] != 0) goto explode
 *          obj->field1c = proc->field34 - 1
 *          if (obj->field1c == 0) goto explode
 *          proc->field34 = obj->field1c
 *          goto vector_track
 *      }
 *      if (frame > 0) frame -= 1
 *      else { install t_local_reaction_exit }
 *      entries[frame].w0, .w1 <- the dead intermediate move, transcribed
 *
 *      vector_track:
 *          dx = MK3_FIELD0E(him) - MK3_FIELD0E(GrObj)
 *          dy = MK3_FIELD12(him) + 0x40 - MK3_FIELD12(GrObj)
 *          get_rough_hypotenuse_of(obj, dx, dy)
 *          if (obj->field54 <= 9) goto explode
 *          hyp = get_rough_hypotenuse(obj, dx<<18, dy<<18) >> 16
 *          nx = (dx<<18) / hyp ; ny = (dy<<18) / hyp        ; ___divsi3 x2
 *          vx = GrObj->field18 - GrObj->field18/16 + nx
 *          vy = GrObj->field1c - GrObj->field1c/16 + ny
 *          hyp = get_rough_hypotenuse_of(obj, vx, vy) >> 16
 *          nx = vx / hyp ; ny = vy / hyp                    ; ___divsi3 x2
 *          GrObj->field18 = nx * 5 ; GrObj->field1c = ny * 5
 *          token 0x1076 ; fieldfc = 1 ; return 1
 *
 *      explode: install t_rocket_explode ; return 0
 */
void point_rocket(MK3OBJ *obj);
long get_rough_hypotenuse(MK3OBJ *obj);
long get_rough_hypotenuse_of(MK3OBJ *obj, int32_t dx, int32_t dy);
long t_rocket_explode(struct MK3THREAD *thread);

long t_rocket_hunt(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);

    if (slot != 0) {
        if (slot != 0x1076)
            return -3;

        point_rocket(obj);
        next_anirate(obj);

        if (*(const int16_t *)((const char *)G + 0x450) != 0)
            goto explode;

        obj->field1c = obj->field00->field34 - 1;
        if (obj->field1c == 0)
            goto explode;
        obj->field00->field34 = obj->field1c;
        goto vector_track;
    }

    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
    } else {
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_local_reaction_exit;
        *mk3_frame(thread, thread->frame + 1) = 0;
    }
    {
        /* The binary re-derives its own handler from the level it just
         * left (or, at the bottom, from whatever sits one level above --
         * dead there, since 0x604 t_local_reaction_exit was just written
         * above and this overwrites it right back with itself). Kept
         * because the store is in the instruction stream either way. */
        uint32_t r  = thread->frame;
        uint32_t r0 = mk3_frame(thread, r + 1)[1];
        uint32_t r2 = *mk3_frame(thread, r + 2);
        *mk3_frame(thread, r + 1) = r2;
        mk3_frame(thread, r)[1] = r0;
    }

vector_track:
    {
        int32_t dx, dy;

        obj->field38 = obj->field00->him;

        obj->field24 = (uint32_t)(int32_t)MK3_FIELD0E_S(obj->field08);
        obj->field2c = (uint32_t)(int32_t)MK3_FIELD12_S(obj->field08);

        dx = MK3_FIELD0E_S((MK3OBJ *)(uintptr_t)obj->field00->him) -
             (int32_t)obj->field24;
        obj->field20 = (uint32_t)dx;
        obj->field38 = (uint32_t)dx;

        dy = (MK3_FIELD12_S((MK3OBJ *)(uintptr_t)obj->field00->him) + 0x40) -
             (int32_t)obj->field2c;
        obj->field28 = (uint32_t)dy;
        obj->field54 = (uint32_t)dy;

        get_rough_hypotenuse_of(obj, dx, dy);

        if (obj->field54 <= 9)
            goto explode;

        {
            int32_t sdx = dx << 18;
            int32_t sdy = dy << 18;
            int32_t hyp, vx, vy, vx16, vy16, nx, ny;

            obj->field20 = (uint32_t)sdx;
            obj->field38 = (uint32_t)sdx;
            obj->field28 = (uint32_t)sdy;
            obj->field54 = (uint32_t)sdy;

            get_rough_hypotenuse(obj);

            hyp = (int32_t)((uint32_t)(int32_t)obj->field54 >> 16);
            obj->field54 = (uint32_t)hyp;

            nx = mk3_sdiv(sdx, hyp);              /* ___divsi3 */
            obj->field20 = (uint32_t)nx;
            ny = mk3_sdiv((int32_t)obj->field28, hyp); /* ___divsi3 */
            obj->field28 = (uint32_t)ny;

            vx = (int32_t)obj->field08->field18;
            vy = (int32_t)obj->field08->field1c;
            obj->field30 = (uint32_t)vx;

            vx16 = vx >> 4;
            vy16 = vy >> 4;
            vx = (vx - vx16) + nx;
            vy = (vy - vy16) + ny;

            obj->field24 = (uint32_t)vy16;
            obj->field38 = (uint32_t)vy;
            obj->field28 = (uint32_t)vy;
            obj->field1c = (uint32_t)vx16;
            obj->field30 = (uint32_t)vx;
            obj->field20 = (uint32_t)vx;

            get_rough_hypotenuse_of(obj, vx, vy);

            hyp = (int32_t)((uint32_t)(int32_t)obj->field54 >> 16);
            obj->field54 = (uint32_t)hyp;

            nx = mk3_sdiv(vx, hyp);                /* ___divsi3 */
            obj->field20 = (uint32_t)nx;
            ny = mk3_sdiv(vy, hyp);                 /* ___divsi3 */

            obj->field54 = 5;
            obj->field20 = (uint32_t)(nx * 5);
            obj->field28 = (uint32_t)(ny * 5);

            obj->field08->field18 = obj->field20;
            obj->field08->field1c = obj->field28;
        }

        *mk3_frame(thread, thread->frame + 1) = 0x1076;
        thread->fieldfc = 1;
        return 1;
    }

explode:
    return mk3_install(thread, (MK3THREADFUNC)t_rocket_explode);
}


long t_rocket_explode(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    int      reflect;

    if (*mk3_frame(thread, frame + 1) != 0)
        return -3;

    obj->field20 =
        ((MK3OBJ *)(void *)(uintptr_t)obj->field00->him)->field24;

    reflect = (obj->field20 == 0x18);        /* Motaro, by identity */
    if (!reflect) {
        get_his_action(obj);                 /* answers in 0x20 */
        reflect = (obj->field20 == 0x402);   /* or anyone doing this */
    }

    if (reflect) {
        obj->field1c = 0x12;
        strike_check_a0_test(obj);           /* would it connect? */

        if (obj->field5c != 0) {
            KillProc((MK3OBJ *)(void *)(uintptr_t)obj->field00->field78);

            obj->a10 = obj->field00->him;
            benedict_arnold_projectile(obj);

            return mk3_install(thread, (MK3THREADFUNC)t_rocket2_proc);
        }
    }

    obj->field1c = 0x12;
    proj_strike_check(obj);

    KillProc((MK3OBJ *)(void *)(uintptr_t)obj->field00->field78);

    return mk3_install(thread, (MK3THREADFUNC)t_rocket_explode_fx);
}


/* ---------------------------------------------------------------- t_rocket2_proc
 *
 * armv7 0x000785d0, 388 bytes.  **Complete.** The writer for `field78`
 * that the struct comment at its declaration was still waiting on:
 * `field00->field78 = NewThreadProc(obj, t_target)`, a homing target
 * `t_rocket_explode` kills on both of its own exits.
 *
 * The free launch throws (`0x60000`, `set_proj_vel`), zeroes the phase index
 * (`field00->field2c`) and its counter (`field00->field28`) by hand instead of
 * through `rocket_routines[0]` (entry 0 is `t_rr_nothing`'s all-zero-amount
 * phase, so seeding the counter from it would be seeding it with itself), then
 * spawns the target thread with `obj->field40` passed across the call on
 * `thread->args[]` the same way every other spawn site in this file does --
 * pushed before `NewThreadProc`, popped back into `field40` right after.
 *
 * `0x1011` is `t_rr_up`'s own token from the caller's side: `point_rocket`,
 * `next_anirate`, then decrement `field00->field28` (the current phase's
 * remaining count). While it is still running, it pushes a level running the
 * current phase's handler (`rocket_routines[index*3]`) under `0x101f`, which
 * parks one frame and comes back to `0x1011` -- the per-frame tick.
 * When it hits zero, the phase index advances and the NEXT entry's duration
 * (`rocket_routines[(index+1)*3 + 2]`) seeds the counter; a non-zero
 * duration just parks on `0x1011` again (the new phase's handler runs from
 * the next tick), and a zero duration -- the table's `{0,0,0}` terminator --
 * installs `t_rocket_explode` instead. (An earlier reading pushed the new
 * phase at once; the differential test showed the binary parks.)
 */
long t_target(struct MK3THREAD *thread);
void point_rocket(MK3OBJ *obj);

long t_rocket2_proc(MK3THREAD *thread)
{
    MK3OBJ   *obj   = (MK3OBJ *)thread->proc;
    uint32_t *args  = (uint32_t *)(void *)thread->args;
    uint32_t  frame = thread->frame;
    uint32_t  slot  = *mk3_frame(thread, frame + 1);

    if (slot == 0x1011) {
        MK3OBJPROC *proc;
        uint32_t    idx, dur;

        point_rocket(obj);
        next_anirate(obj);

        proc = obj->field00;
        obj->field1c = proc->field28 - 1;
        proc->field28 = obj->field1c;

        if (obj->field1c != 0) {
            obj->field1c = proc->field2c;

            *mk3_frame(thread, frame + 1) = 0x101f;
            thread->frame = thread->frame + 1;   /* push a level */
            mk3_frame(thread, thread->frame)[1] =
                rocket_routines[obj->field1c * 3];
            *mk3_frame(thread, thread->frame + 1) = 0;
            return 0;
        }

        idx = proc->field2c + 1;
        obj->field1c = idx;
        proc->field2c = idx;

        dur = rocket_routines[idx * 3 + 2];
        obj->field1c = dur;

        if (dur != 0) {
            /* b 0x78682: arm the new count and park on 0x1011 -- the next
             * routine is pushed on the following tick, not now (difftest) */
            proc->field28 = dur;
            *mk3_frame(thread, frame + 1) = 0x1011;
            thread->fieldfc = 1;
            return 1;
        }

        return mk3_install(thread, (MK3THREADFUNC)t_rocket_explode);
    }

    if (slot == 0x101f) {
        *mk3_frame(thread, frame + 1) = 0x1011;
        thread->fieldfc = 1;
        return 1;
    }

    if (slot != 0)
        return -3;

    obj->field1c = 4;
    ochar_sound(obj);

    obj->field00->field34 = 0x80;
    obj->field20           = 0x80 - 0x7d;         /* one literal */

    obj->field1c = 0x60000;
    set_proj_vel(obj);

    obj->field48 = 0x12;
    obj->field1c = 0x12;
    obj->a10     = 0;
    obj->field40 = 0;
    tell_world_stk(obj);

    obj->field1c          = 0;
    obj->field00->field30 = 0;

    obj->field00->field2c = obj->field1c;

    obj->field1c           = 0;                  /* dead: never read again */
    obj->field00->field28  = rocket_routines[2];

    args[thread->fieldf8] = obj->field40;
    thread->fieldf8 = thread->fieldf8 + 1;

    obj->field40 = 5;
    get_char_ani2(obj);

    obj->field00->field78 =
        (MK3OBJ *)NewThreadProc(obj, (MK3THREADFUNC)t_target);

    thread->fieldf8 = thread->fieldf8 - 1;
    obj->field40 = args[thread->fieldf8];

    *mk3_frame(thread, frame + 1) = 0x1011;
    thread->fieldfc = 1;
    return 1;
}


/* t_scorp_waiting_sleep -- armv7 0x00074e38, 172 bytes.  **Complete.**
 *
 *      token == 0:        token := 0x293, park 0x30
 *
 *      token == 0x293:    obj->field40 = 9
 *                         obj->field1c = 9 - 7 = 2
 *                         token := 0x298, descend into t_backwards_ani2
 *
 *      token == 0x298:    obj->field20  = 0
 *                         proc->field18 = 0
 *                         pop a level, or t_local_reaction_exit at the bottom
 *
 *      otherwise:         return -3
 *
 * **Wait, rewind, clear the action.** Forty-eight frames of nothing, the
 * animation played backwards, and then 0 into both `obj->field20` and
 * `proc->field18` -- the same pair `t_lk_zap_air` fills with 0x15 and
 * `i_am_a_sitting_duck` with 0x604. Announcing an action and clearing one use
 * the same two fields, so a port that only clears the proc leaves a stale copy
 * where the caller looks.
 *
 * The 9 and the 2 come off one register, `movs #9` then `subs #7`, which is why
 * they look unrelated.
 */
long t_backwards_ani2(MK3THREAD *thread);        /* pointer slot 0x000f3704 */

long t_scorp_waiting_sleep(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0) {
        *mk3_frame(thread, frame + 1) = 0x293;
        thread->fieldfc = 0x30;
        return 0x30;
    }

    if (token == 0x293) {
        obj->field40 = 9;
        obj->field1c = 9 - 7;                /* the same register */

        *mk3_frame(thread, frame + 1) = 0x298;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_backwards_ani2;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x298)
        return -3;

    obj->field20          = 0;
    obj->field00->field18 = 0;

    if ((long)thread->frame > 0) {           /* cmp #0 / ble: signed */
        thread->frame = thread->frame - 1;   /* back up a level */
        return 0;
    }

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* ------------------------------------------------------------------------------ tl_ssp2
 *
 * armv7 0x0007ae38, 240 bytes.  **Complete.** Scorpion/Smoke's spear
 * launcher, the driver `tl_do_smoke_spear`/`tl_do_scorpion_spear` hand off
 * to.
 *
 * The free saves `field38` on the argument stack, packs `0x00010009`
 * (rate 1, animation 9) and asks `get_his_action`: unless the answer is
 * `0x503` or `0x507` (already reacting, one way or another), it swaps in
 * `0x00020009` (rate 2) instead -- a slower throw against an opponent who
 * isn't already stumbling. Descends into `t_animate2_a9` from `0x27e`.
 * `0x27e` restores `field38`, launches through `create_proj_proc`,
 * hangs the new slave off `proc->field88` (`t_double_shaker`'s own
 * partner slot), tags the action to `0x604`, and installs
 * `t_scorp_waiting_sleep` outright.
 *
 *      slot = frame[frame+1].w0
 *      if (slot == 0x27e) {
 *          fieldf8-- ; obj->field38 = args[fieldf8]
 *          slave = create_proj_proc(obj) ; proc->field88 = slave
 *          obj->field1c = obj->field00->field18 = 0x604
 *          install t_scorp_waiting_sleep ; return 0
 *      }
 *      if (slot != 0) return -3
 *      args[fieldf8] = obj->field38 ; fieldf8++
 *      obj->a10 = 0 ; obj->field20 = 0x1d ; zap_init_special_act(obj)
 *      obj->field40 = 0x00010009 ; get_his_action(obj)
 *      if (obj->field20 != 0x503 && obj->field20 != 0x507)
 *          obj->field40 = 0x00020009
 *      token 0x27e ; frame++ ; install t_animate2_a9 ; return 0
 */
long t_scorp_waiting_sleep(struct MK3THREAD *thread);

long tl_ssp2(MK3THREAD *thread)
{
    MK3OBJ   *obj  = (MK3OBJ *)thread->proc;
    uint32_t *args = (uint32_t *)(void *)thread->args;
    uint32_t  frame = thread->frame;
    uint32_t  slot  = *mk3_frame(thread, frame + 1);

    if (slot == 0x27e) {
        MK3OBJ *slave;

        thread->fieldf8 = thread->fieldf8 - 1;
        obj->field38 = args[thread->fieldf8];

        slave = create_proj_proc(obj);
        obj->field00->field88 = slave;

        obj->field1c = 0x604;
        obj->field00->field18 = 0x604;

        return mk3_install(thread, (MK3THREADFUNC)t_scorp_waiting_sleep);
    }

    if (slot != 0)
        return -3;

    args[thread->fieldf8] = obj->field38;
    thread->fieldf8 = thread->fieldf8 + 1;

    obj->a10    = 0;
    obj->field20 = 0x1d;
    zap_init_special_act(obj);

    obj->field40 = 0x00010009;
    get_his_action(obj);

    if (obj->field20 != 0x503 && obj->field20 != 0x507)
        obj->field40 = 0x00020009;

    *mk3_frame(thread, frame + 1) = 0x27e;
    thread->frame = thread->frame + 1;   /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_animate2_a9;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* t_angle_zap_jsrp -- armv7 0x00077010, 172 bytes.  **Complete.**
 *
 *      token == 0:       do_next_a9_frame(obj)
 *                        obj->a10 = 4
 *                        -- falls into the check --
 *
 *      the check:        obj->field1c = obj->field48
 *                        strike_check_a0(obj)
 *                        if (obj->field5c != 0)
 *                            frame[frame].handler = t_angle_zap_hit
 *                        token := 0xe06, park 1
 *
 *      token == 0xe06:   if (--obj->a10 != 0) -- the check --
 *                        pop a level, or t_local_reaction_exit at the bottom
 *
 *      otherwise:        return -3
 *
 * **Four swings, one frame apart, and the first one that connects wins.** The
 * counter starts at 4 and the routine re-tests every frame until it runs out,
 * so the zap has a four-frame window to hit rather than one instant.
 *
 * That is worth a port knowing exactly: a re-implementation that checks once
 * makes the move miss where the original hits, and the difference is invisible
 * except in edge cases -- which is where fighting games live.
 *
 * **The strike parameter comes out of 0x48**, copied into 0x1c for the call, so
 * whoever launched the zap chose how hard it lands and this routine only decides
 * when.
 *
 * `strike_check_a0` and not `strike_check_a0_test` -- the committing twin -- so
 * a hit here registers rather than being asked about, which is the difference
 * `t_rocket_explode` uses the other way round.
 */
long strike_check_a0(MK3OBJ *obj);
long t_angle_zap_hit(MK3THREAD *thread);
long do_next_a9_frame(MK3OBJ *obj);

long t_angle_zap_jsrp(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0) {
        do_next_a9_frame(obj);
        obj->a10 = 4;                        /* four frames to connect */

    } else if (token == 0xe06) {
        obj->a10 = obj->a10 - 1;

        if (obj->a10 == 0) {
            if ((long)thread->frame > 0) {   /* cmp #0 / ble: signed */
                thread->frame = thread->frame - 1;
                return 0;
            }
            return mk3_install(thread,
                               (MK3THREADFUNC)t_local_reaction_exit);
        }

    } else {
        return -3;
    }

    obj->field1c = obj->field48;             /* the launcher chose the hit */
    strike_check_a0(obj);

    if (obj->field5c != 0)
        return mk3_install(thread, (MK3THREADFUNC)t_angle_zap_hit);

    *mk3_frame(thread, frame + 1) = 0xe06;
    thread->fieldfc = 1;
    return 1;
}


/* ------------------------------------------------------------------ t_angle_zap_proc
 *
 * armv7 0x00076158, 368 bytes.  **Complete.**
 *
 * Five states chained through the same `t_angle_zap_jsrp` push, one after
 * another, before the actual throw. The free poses animation 0x3f
 * (`field48 = 0x12`) and descends into `t_angle_zap_jsrp` from `0xdcd`;
 * `0xdcd` re-descends into it again from `0xdce` (`field48 = 0x13`);
 * `0xdce` re-descends a THIRD time from `0xdd0` (`multi_adjust_xy`,
 * `field1c`/`field20 = 9`); `0xdd0` finally sets up the velocity
 * (`field1c = GrObj->field1c = 0x80000`, `field20 = 4`, `set_proj_vel`,
 * `field34 = t_angle_zap_call`) and descends into
 * `tl_projectile_flight_call` from `0xddc`. `0xddc` is the floor:
 * `t_angle_zap_hit` installed outright, no pop.
 *
 *      slot = frame[frame+1].w0
 *                                          ; 0:    free, then 0xdcd
 *                                          ; 0xdcd: pose again, then 0xdce
 *                                          ; 0xdce: pose again, then 0xdd0
 *                                          ; 0xdd0: adjust, then 0xdd4
 *                                          ; 0xdd4: launch, then 0xddc
 *                                          ; 0xddc: install t_angle_zap_hit
 *      if (slot == 0xdce) {
 *          obj->field48 = 0x13
 *          token 0xdd0 ; frame++ ; install t_angle_zap_jsrp ; return 0
 *      }
 *      if (slot < 0xdce) {
 *          if (slot == 0) {
 *              obj->field40 = 0x3f ; get_char_ani(obj) ; obj->field48 = 0x12
 *              token 0xdcd ; frame++ ; install t_angle_zap_jsrp ; return 0
 *          }
 *          if (slot != 0xdcd) return -3
 *          token 0xdce ; frame++ ; install t_angle_zap_jsrp ; return 0
 *      }
 *      if (slot == 0xdd4) {
 *          obj->field48 = 0x14
 *          obj->field1c = GrObj->field1c = 0x80000
 *          obj->field20 = 4 ; set_proj_vel(obj)
 *          obj->field34 = t_angle_zap_call
 *          token 0xddc ; frame++ ; install tl_projectile_flight_call ; return 0
 *      }
 *      if (slot == 0xddc) install t_angle_zap_hit ; return 0
 *      if (slot != 0xdd0) return -3
 *      obj->field1c = obj->field20 = 9 ; multi_adjust_xy(obj)
 *      token 0xdd4 ; frame++ ; install t_angle_zap_jsrp ; return 0
 */
long t_angle_zap_jsrp(struct MK3THREAD *thread);
long t_angle_zap_call(struct MK3THREAD *thread);
long t_angle_zap_hit(struct MK3THREAD *thread);

long t_angle_zap_proc(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t slot  = *mk3_frame(thread, frame + 1);

    if (slot == 0xdce) {
        obj->field48 = 0x13;

        *mk3_frame(thread, frame + 1) = 0xdd0;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_angle_zap_jsrp;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot < 0xdce) {
        if (slot == 0) {
            obj->field40 = 0x3f;
            get_char_ani(obj);
            obj->field48 = 0x12;

            *mk3_frame(thread, frame + 1) = 0xdcd;
            thread->frame = thread->frame + 1;   /* push a level */
            mk3_frame(thread, thread->frame)[1] =
                (uint32_t)(uintptr_t)t_angle_zap_jsrp;
            *mk3_frame(thread, thread->frame + 1) = 0;
            return 0;
        }

        if (slot != 0xdcd)
            return -3;

        *mk3_frame(thread, frame + 1) = 0xdce;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_angle_zap_jsrp;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot == 0xdd4) {
        obj->field48 = 0x14;

        obj->field1c = 0x80000;
        obj->field08->field1c = 0x80000;
        obj->field20 = 4;
        set_proj_vel(obj);

        obj->field34 = (uint32_t)(uintptr_t)t_angle_zap_call;

        *mk3_frame(thread, frame + 1) = 0xddc;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)tl_projectile_flight_call;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot == 0xddc)
        return mk3_install(thread, (MK3THREADFUNC)t_angle_zap_hit);

    if (slot != 0xdd0)
        return -3;

    obj->field1c = 9;
    obj->field20 = 9;
    multi_adjust_xy(obj);

    *mk3_frame(thread, frame + 1) = 0xdd4;
    thread->frame = thread->frame + 1;   /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_angle_zap_jsrp;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------------------ tl_do_lia_anglez
 *
 * armv7 0x0007b2b8, 420 bytes.  **Complete.**
 *
 * `t_angle_zap_proc`'s driver, and the one that saves/restores BOTH
 * `proc->slave` and `proc->field64` around `create_proj_proc` -- forcing
 * a fresh spawn on both fronts, then giving the caller back its original
 * slave and object once the new one exists, the union of what
 * `tl_do_ind_zap` and `tl_do_tusk_floor` each do to only one of the pair.
 * The free saves the acting action into `proc->field38` before
 * `zap_air_init_special` overwrites it -- read back three states later to
 * choose the close. `0xe59` deletes the slave and waits; `0xe5c` reads
 * that saved action back: `0x203` installs `t_do_body_propell`, anything
 * else installs `t_land_on_yer_feet`.
 *
 *      slot = frame[frame+1].w0
 *                                          ; 0:     free, then 0xe46
 *                                          ; 0xe46:  launch, then 0xe59 (wait)
 *                                          ; 0xe59:  delete slave, then 0xe5c
 *                                          ; 0xe5c:  install by the saved action
 *      if (slot == 0xe46) {
 *          saved_slave = proc->slave ; saved64 = proc->field64
 *          obj->field48 = saved_slave ; proc->field64 = 0 ; proc->slave = 0
 *          obj->field38 = t_angle_zap_proc ; create_proj_proc(obj)
 *          proc->slave = obj->field48 ; proc->field64 = saved64
 *          obj->field1c = G + 0x404 ; update_tsl(obj)
 *          i_am_a_sitting_duck(obj) ; obj->field1c = 5
 *          token 0xe59 ; frame++ ; install t_mframew ; return 0
 *      }
 *      if (slot < 0xe46) {
 *          if (slot != 0) return -3
 *          proc->field38 = proc->field18
 *          obj->a10 = 0 ; zap_air_init_special(obj)
 *          obj->field20 = proc->field18 = 0xb ; obj->field1c = 2
 *          ochar_sound(obj)
 *          obj->field40 = 7 ; get_char_ani2(obj) ; obj->field1c = 2
 *          token 0xe46 ; frame++ ; install t_mframew ; return 0
 *      }
 *      if (slot == 0xe59) {
 *          delete_slave(obj) ; obj->field1c = 2
 *          token 0xe5c ; frame++ ; install t_mframew ; return 0
 *      }
 *      if (slot != 0xe5c) return -3
 *      obj->field1c = GrObj->field1c = 0x10000
 *      obj->field1c = proc->field38
 *      if (proc->field38 == 0x203) {
 *          obj->field1c = 0xb ; install t_do_body_propell ; return 0
 *      }
 *      install t_land_on_yer_feet ; return 0
 */
long t_angle_zap_proc(struct MK3THREAD *thread);
long t_do_body_propell(struct MK3THREAD *thread);       /* pointer slot 0x000f31a0 */
long t_land_on_yer_feet(struct MK3THREAD *thread);      /* pointer slot 0x000f3778 */

long tl_do_lia_anglez(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t slot  = *mk3_frame(thread, frame + 1);

    if (slot == 0xe46) {
        MK3OBJPROC *proc         = obj->field00;
        uint32_t    saved_slave  = proc->slave;
        uint32_t    saved64      = proc->field64;

        obj->field48   = saved_slave;
        proc->field64  = 0;
        obj->field00->slave = 0;

        obj->field38 = (uint32_t)(uintptr_t)t_angle_zap_proc;
        create_proj_proc(obj);

        obj->field00->slave   = obj->field48;
        obj->field00->field64 = saved64;

        obj->field1c = (uint32_t)(uintptr_t)((char *)G + 0x404);
        update_tsl(obj);

        i_am_a_sitting_duck(obj);
        obj->field1c = 5;

        *mk3_frame(thread, frame + 1) = 0xe59;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot < 0xe46) {
        if (slot != 0)
            return -3;

        obj->field00->field38 = obj->field00->field18;

        obj->a10 = 0;
        zap_air_init_special(obj);

        obj->field20 = 0xb;
        obj->field00->field18 = 0xb;
        obj->field1c = 2;
        ochar_sound(obj);

        obj->field40 = 7;
        get_char_ani2(obj);
        obj->field1c = 2;

        *mk3_frame(thread, frame + 1) = 0xe46;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot == 0xe59) {
        delete_slave(obj);
        obj->field1c = 2;

        *mk3_frame(thread, frame + 1) = 0xe5c;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot != 0xe5c)
        return -3;

    obj->field1c = 0x10000;
    obj->field08->field1c = 0x10000;

    obj->field1c = obj->field00->field38;

    if (obj->field00->field38 == 0x203) {
        obj->field1c = 0xb;
        return mk3_install(thread, (MK3THREADFUNC)t_do_body_propell);
    }
    return mk3_install(thread, (MK3THREADFUNC)t_land_on_yer_feet);
}


/* t_lk_zap_proc -- armv7 0x00077814, 176 bytes.  **Complete.**
 *
 *      token == 0:       find_part2(obj)
 *                        obj->field40 -= 4
 *                        obj->field1c = 0x80000
 *                        obj->field20 = 4
 *                        set_proj_vel(obj)
 *                        obj->field48 = 0x11
 *                        token := 0xa08, descend into tl_projectile_flight
 *
 *      token == 0xa08:   obj->field1c = 0x00010003
 *                        hob_ochar_sound(obj)
 *                        make_dragon_explode(obj)
 *                        frame[frame].handler = tl_delete_proj_and_die
 *
 *      otherwise:        return -3
 *
 * **`obj->field40 -= 4` right after `find_part2`** -- the cursor stepped back one
 * word by hand, the fourth site in the tree after `t_scorpion_flame`'s `-= 4`,
 * `t_mileena_nails`' `+= 0x10` and `t_st_spike`'s `+= 0x90`. All four move a
 * cursor a finder has just resolved, and none of them asks the finder for the
 * position instead.
 *
 * `hob_ochar_sound` rather than plain `ochar_sound`, and its argument is a
 * packed pair -- 0x00010003 -- where every `ochar_sound` site in the tree passes
 * a small index. So the "hob" variant takes two numbers in one word; which half
 * is which is not settled here.
 *
 * It shares `make_dragon_explode` with `t_lk_prezap_hit`, which borrows the
 * slave's part for the call; this one explodes its own. Same helper, two bodies,
 * and the difference is entirely in what 0x08 points at when it runs.
 */
void hob_ochar_sound(MK3OBJ *obj);
void find_part2(MK3OBJ *obj);

long t_lk_zap_proc(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0) {
        find_part2(obj);
        obj->field40 = obj->field40 - 4;     /* back one word, by hand */

        obj->field1c = 0x80000;
        obj->field20 = 4;
        set_proj_vel(obj);

        obj->field48 = 0x11;

        *mk3_frame(thread, frame + 1) = 0xa08;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)tl_projectile_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0xa08)
        return -3;

    obj->field1c = 0x00010003;               /* a packed pair, not an index */
    hob_ochar_sound(obj);

    make_dragon_explode(obj);

    return mk3_install(thread, (MK3THREADFUNC)tl_delete_proj_and_die);
}


/* t_angle_zap_hit -- armv7 0x00078e24, 180 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = (obj->field18 != 0) ? 4 : 3
 *      ochar_sound(obj)
 *      lowest_mpart_ob(obj, part)                  -> obj->field20
 *      PUSH obj->field20
 *      if (part->field28 & 0x10) a3_leftmost_mpart_ob(obj, part)
 *      else                      rightmost_mpart_ob(obj, part)
 *      POP  obj->field20
 *      part->x0e = (uint16_t)obj->field28
 *      part->x12 = (uint16_t)obj->field20
 *      obj->field1c = -0x80
 *      obj->field20 = -0x80
 *      multi_adjust_xy(obj)
 *      frame[frame].handler = t_angle_zap_explode
 *
 * **The impact is placed at the front-bottom corner of the body.** The vertical
 * comes from `lowest_mpart_ob` and the horizontal from whichever edge is in
 * front -- left when the flip bit is set, right otherwise -- and then both are
 * pulled back 0x80.
 *
 * **`a3_leftmost_mpart_ob` exists so the caller does not have to normalise.**
 * Plain `leftmost_mpart_ob` answers in 0x24 and `rightmost_mpart_ob` in 0x28,
 * which is why `get_frontmost_point` earlier in this file has a copy on one
 * branch and not the other. This routine reads **0x28 on both paths** without
 * any such copy, so the `a3_` variant must answer there too. That is what the
 * prefix buys: one slot, two directions, no fix-up.
 *
 * **Nineteenth argument-stack site**, and the reason is the same as always: the
 * vertical answer lands in 0x20, the horizontal helper is about to overwrite
 * 0x20 as well, and the two are needed together afterwards.
 *
 * The sound index is 3 or 4 chosen on `obj->field18`, which nothing here sets --
 * so whoever launched the zap decides which of the two impact sounds it makes.
 */
void lowest_mpart_ob(MK3OBJ *out, MK3OBJ *src);
void a3_leftmost_mpart_ob(MK3OBJ *out, MK3OBJ *src);

long t_angle_zap_hit(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t argc;

    if (*mk3_frame(thread, frame + 1) != 0)
        return -3;

    obj->field1c = (obj->field18 != 0) ? 4 : 3;
    ochar_sound(obj);

    lowest_mpart_ob(obj, obj->field08);          /* answers in 0x20 */

    argc = thread->fieldf8;
    *mk3_arg(thread, argc) = obj->field20;
    thread->fieldf8 = argc + 1;

    if ((obj->field08->field28 & 0x10u) != 0)
        a3_leftmost_mpart_ob(obj, obj->field08); /* also answers in 0x28 */
    else
        rightmost_mpart_ob(obj, obj->field08);

    argc = thread->fieldf8 - 1;
    thread->fieldf8 = argc;
    obj->field20 = *mk3_arg(thread, argc);

    MK3_SET_FIELD0E(obj->field08, obj->field28);
    MK3_SET_FIELD12(obj->field08, obj->field20);

    obj->field1c = (uint32_t)~0x7fu;             /* -0x80 */
    obj->field20 = (uint32_t)~0x7fu;
    multi_adjust_xy(obj);

    return mk3_install(thread, (MK3THREADFUNC)t_angle_zap_explode);
}


/* tl_do_sonya_zap -- armv7 0x000796c4, 180 bytes.  **Complete.**
 *
 *      token == 0:         obj->field20 = 2
 *                          obj->a10     = 0
 *                          zap_init_special_act(obj)
 *                          obj->field1c = 0; ochar_sound(obj)
 *                          obj->field40 = 0x24; get_char_ani(obj)
 *                          obj->field1c = 3
 *                          token := 0x12df, descend into t_mframew
 *
 *      token == 0x12df:    obj->field38 = tl_sonya_zap_proc
 *                          create_proj_proc(obj)
 *                          obj->field20 = 0x23
 *                          frame[frame].handler = tl_do_proj_sitting_duck
 *
 *      otherwise:          return -3
 *
 * **Entry 1 of `projectile_jumps`, and the same two states as `tl_do_sw_zap`:**
 * set up, animate, then name the projectile's handler in 0x38, call
 * `create_proj_proc` and park as a sitting duck.
 *
 * **Third duration measured for `tl_do_proj_sitting_duck`: 0x23.** With Jax's
 * 0x16 and Swat's 0x18 that is 22, 24 and 35 frames -- so the recovery after a
 * projectile is per-move and the field is carrying real data, not a constant
 * three routines happen to share.
 */
long tl_sonya_zap_proc(MK3THREAD *thread);

long tl_do_sonya_zap(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0) {
        obj->field20 = 2;
        obj->a10     = 0;
        zap_init_special_act(obj);

        obj->field1c = 0;
        ochar_sound(obj);

        obj->field40 = 0x24;
        get_char_ani(obj);

        obj->field1c = 3;

        *mk3_frame(thread, frame + 1) = 0x12df;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x12df)
        return -3;

    obj->field38 = (uint32_t)(uintptr_t)tl_sonya_zap_proc;
    create_proj_proc(obj);

    obj->field20 = 0x23;                     /* the sitting-duck duration */

    return mk3_install(thread, (MK3THREADFUNC)tl_do_proj_sitting_duck);
}


/* t_sg_zap_proc -- armv7 0x000762c8, 192 bytes.  **Complete.**
 *
 *      token == 0:       obj->field40  = a_small_explode
 *                        part->field2c = 0x6ae
 *                        proc->field18 = 0x17
 *                        obj->field1c  = 0x17 + 0x19 = 0x30
 *                        obj->field20  = 0x30
 *                        multi_adjust_xy(obj)
 *                        proc->field2c = 4
 *                        obj->field1c = 0xa0000
 *                        obj->field20 = 0xfff
 *                        set_proj_vel(obj)
 *                        obj->field48 = 0x11
 *                        obj->field34 = t_sg_trail_spawn
 *                        token := 0x987, descend into tl_projectile_flight_call
 *
 *      token == 0x987:   frame[frame].handler = tl_delete_proj_and_die
 *
 *      otherwise:        return -3
 *
 * **This closes `t_sg_trail_spawn`, and it seeds the counter that routine
 * counts down.** That callback fires effect 0x35 whenever `proc->field2c`
 * reaches zero and reloads it with 4; here is the launcher, writing **4** into
 * that same field two instructions before it hands the callback over. So the
 * trail starts on the fourth frame rather than immediately.
 *
 * Same shape as `t_cyrax_helecopter` seeding `a10 = 1` for `t_hele_sleep` in
 * mkfatal.c: the driver decides where in the cycle the effect first lands, and
 * the callback only knows the period. **Two systems, one convention.**
 *
 * The callback goes into `obj->field34` and `tl_projectile_flight_call`
 * publishes it to `proc->field28` -- the third complete instance of that chain,
 * after Motaro's zap and the impale in mkfatal.c.
 *
 * `a_small_explode` is a `__DATA,__data` array at 0x00172624, assigned to 0x40
 * as an address. The same trap `a_sb_skeleton_burn` and `a_kano_rip_skel` set:
 * a symbol beginning `a_` is data, not a routine.
 *
 * One literal three times: `movs #0x17` for the action, then `adds #0x19` gives
 * 0x30 for both halves of the `multi_adjust_xy` offset. The action number and
 * the placement have nothing to do with each other and share a register anyway.
 */
extern uint32_t a_small_explode[];               /* 0x00172624 */
long t_sg_trail_spawn(MK3THREAD *thread);

long t_sg_zap_proc(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0) {
        obj->field40          = (uint32_t)(uintptr_t)a_small_explode;
        obj->field08->field2c = 0x6ae;

        obj->field00->field18 = 0x17;
        obj->field1c          = 0x17 + 0x19;     /* the same register */
        obj->field20          = obj->field1c;
        multi_adjust_xy(obj);

        obj->field00->field2c = 4;               /* the trail's first tick */

        obj->field1c = 0xa0000;
        obj->field20 = 0xfff;
        set_proj_vel(obj);

        obj->field48 = 0x11;
        obj->field34 = (uint32_t)(uintptr_t)t_sg_trail_spawn;

        *mk3_frame(thread, frame + 1) = 0x987;
        thread->frame = thread->frame + 1;       /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)tl_projectile_flight_call;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x987)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)tl_delete_proj_and_die);
}


/* tl_do_sz_zap -- armv7 0x0007a79c, 192 bytes.  **Complete.**
 *
 *      token == 0:        obj->field20 = 0x1a
 *                         obj->a10     = 0
 *                         zap_init_special_act(obj)
 *                         obj->field1c = 0; ochar_sound(obj)
 *                         obj->field40 = 0x00030024
 *                         token := 0x856, descend into t_animate_a9
 *
 *      token == 0x856:    obj->field1c = 0x30
 *                         obj->field20 = 0x30 + 8 = 0x38
 *                         obj->field30 = proc->slave
 *                         adjust_xy_a5(obj)
 *                         proc->field64->field40 = &sz_ani_data[0x1220]
 *                         frame[frame].handler = t_osz_forward_entry
 *
 *      otherwise:         return -3
 *
 * **It reaches into the slave and sets its animation cursor directly.** Every
 * other launcher in this file hands the slave a HANDLER through `obj->field38`
 * and lets `create_proj_proc` start it; this one writes
 * `proc->field64->field40` from outside. A fourth channel into another object,
 * after 0x38 for the handler, `proc->field00->field48` for a table, and
 * `NewThreadProc`'s return value.
 *
 * **`sz_ani_data + 0x1220` is a fifth unnamed sub-table inside a named
 * animation block**, after `lao_ani_data + 0x142c`, `fn_ani_data + 0x20a4` and
 * `+ 0x1a84`, and `lia_ani_data + 0x1554`. The base arrives through pointer slot
 * 0x000f33d8. A symbol names where a block starts, not what any table in it is.
 *
 * `adjust_xy_a5` takes three fields -- 0x1c, 0x20 and 0x30 -- and 0x30 holds
 * `proc->slave`, the slave's PART. So the placement is "put this at that
 * object's position plus (0x30, 0x38)", which is the three-argument form
 * mkfatal.c's note describes.
 *
 * 0x30 and 0x38 come off one register with an `adds #8`.
 */
extern uint8_t sz_ani_data[];                    /* 0x00162914, slot 0x000f33d8 */
void adjust_xy_a5(MK3OBJ *obj);
long t_osz_forward_entry(MK3THREAD *thread);

long tl_do_sz_zap(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0) {
        obj->field20 = 0x1a;
        obj->a10     = 0;
        zap_init_special_act(obj);

        obj->field1c = 0;
        ochar_sound(obj);

        obj->field40 = 0x00030024;

        *mk3_frame(thread, frame + 1) = 0x856;
        thread->frame = thread->frame + 1;       /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_animate_a9;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x856)
        return -3;

    obj->field1c = 0x30;
    obj->field20 = 0x30 + 8;                     /* the same register */
    obj->field30 = obj->field00->slave;
    adjust_xy_a5(obj);

    ((MK3OBJ *)(void *)(uintptr_t)obj->field00->field64)->field40 =
        (uint32_t)(uintptr_t)&sz_ani_data[0x1220];

    return mk3_install(thread, (MK3THREADFUNC)t_osz_forward_entry);
}


/* t_swat_bomb_proc -- armv7 0x00078754, 196 bytes.  **Complete.**
 *
 *      token == 0:       obj->field40 = 0; find_ani2_part2(obj)
 *                        obj->field1c = -0x30000
 *                        if (obj->field48 != 0) obj->field1c = -0x60000
 *                        part->field1c = obj->field1c
 *                        obj->field1c = 0x80000
 *                        obj->field20 = 4
 *                        set_proj_vel(obj)
 *                        obj->field48 = 0x10
 *                        obj->field34 = t_bomb_call
 *                        token := 0x8d6, descend into tl_projectile_flight_call
 *
 *      token == 0x8d6:   obj->field48 = 0
 *                        make_lineup_explode(obj)
 *                        obj->field1c = 0; ochar_sound(obj)
 *                        frame[frame].handler = tl_delete_proj_and_die
 *
 *      otherwise:        return -3
 *
 * **The high and low bombs are one routine and one branch.** `projectile_jumps`
 * has `tl_do_swat_bomb_hi` at 25 and `tl_do_swat_bomb_lo` at 26; both end up
 * here, and 0x48 on entry decides the launch velocity -- **-0x30000 for the low
 * arc, -0x60000 for the high one**, exactly double. Then 0x48 is immediately
 * reused as the flight's own parameter, 0x10, so the selector is consumed the
 * instant it has been read.
 *
 * **This closes `t_bomb_call`**, the eighty-eight byte callback that adds 0x6000
 * to the y velocity every frame. Fourth complete instance of the
 * 0x34 -> `proc->field28` chain, after Motaro's zap, the SG zap and the impale.
 *
 * So a Swat bomb is: an upward kick chosen by one bit, a fixed forward speed,
 * and a constant fall -- three numbers and no special case anywhere else.
 *
 * `obj->field48 = 0` before `make_lineup_explode` again, the second launcher to
 * pass zero there after `t_motaro_zap_proc`. Only the high half of that field
 * does anything in the explosion, and both of these pass none.
 */
long t_bomb_call(MK3THREAD *thread);
void find_ani2_part2(MK3OBJ *obj);

long t_swat_bomb_proc(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0) {
        obj->field40 = 0;
        find_ani2_part2(obj);

        obj->field1c = 0xfffd0000u;              /* -0x30000, the low arc */
        if (obj->field48 != 0)
            obj->field1c = 0xfffa0000u;          /* -0x60000, the high one */

        obj->field08->field1c = obj->field1c;

        obj->field1c = 0x80000;
        obj->field20 = 4;
        set_proj_vel(obj);

        obj->field48 = 0x10;                     /* the selector, reused */
        obj->field34 = (uint32_t)(uintptr_t)t_bomb_call;

        *mk3_frame(thread, frame + 1) = 0x8d6;
        thread->frame = thread->frame + 1;       /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)tl_projectile_flight_call;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x8d6)
        return -3;

    obj->field48 = 0;
    make_lineup_explode(obj);

    obj->field1c = 0;
    ochar_sound(obj);

    return mk3_install(thread, (MK3THREADFUNC)tl_delete_proj_and_die);
}


/* ------------------------------------------------------------------------ tl_bomb33
 *
 * armv7 0x0007b844, 332 bytes.  **Complete.**
 *
 * A four-stage bomb hand-off, each stage descending into an unrelated
 * general-purpose thread borrowed by pointer slot instead of by name --
 * `t_animate_a0_frames`, `t_backwards_ani2` and `t_mframew`, the same three
 * `other.c` primitives the rest of this file reaches the same way -- so the
 * only stage with any bomb-specific work is the launch itself.
 *
 * The free poses nothing and just descends into `t_animate_a0_frames` under
 * `0x8f2` with `field1c = 0x30003` already packed. `0x8f2` is the launch:
 * `field38 = t_swat_bomb_proc` (the same "next hit callback" convention
 * every zap/bomb launcher in this file uses), `create_proj_proc` spawns the
 * slave, and -- only when a slave actually comes back -- `find_ani2_part2`
 * seeds `field30`/the slave's own `field40` and `adjust_xy_a5` lines it up
 * (`field1c = field20 = 0x20`). `field40` is saved across all of that in a
 * callee-saved register and restored either way before `i_am_a_sitting_duck`,
 * so the create-proj detour never leaks into the animation state. It descends
 * into `t_mframew` (slot 0x000f37cc) under `0x921`.
 *
 * `0x921` does no work of its own: it just re-arms `0x922` and sleeps sixteen
 * frames (`fieldfc = 0x10`) -- the only stage in the chain that is a a plain
 * wait rather than a descent. `0x922` finishes it: `field1c = 3`,
 * `field40 = 0`, `t_backwards_ani2` (slot 0x000f3704) installed on the current
 * level, no push. (The two handlers were crossed in an earlier reading; the
 * slots and the difftest settle it.)
 */
long t_animate_a0_frames(MK3THREAD *thread);     /* pointer slot 0x000f36b8 */
void group_sound(MK3OBJ *obj);

long tl_bomb33(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t slot  = *mk3_frame(thread, frame + 1);

    if (slot == 0x8f2) {
        MK3OBJ *slave;

        obj->field1c = 0;
        group_sound(obj);

        obj->field1c = 1;
        ochar_sound(obj);

        obj->field38 = (uint32_t)(uintptr_t)t_swat_bomb_proc;

        uint32_t saved_field40 = obj->field40;
        slave = create_proj_proc(obj);

        if (slave != NULL) {
            obj->field40 = 0;
            find_ani2_part2(obj);

            obj->field30    = (uint32_t)(uintptr_t)slave->field08;
            slave->field40  = obj->field40;

            obj->field1c = 0x20;
            obj->field20 = 0x20;
            adjust_xy_a5(obj);
        }

        obj->field40 = saved_field40;
        i_am_a_sitting_duck(obj);

        obj->field1c = 4;

        /* slot 0x000f37cc holds t_mframew (0x0005a25d) -- not the
         * t_backwards_ani2 that 0x922 installs (difftest) */
        *mk3_frame(thread, frame + 1) = 0x921;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot == 0x921) {
        *mk3_frame(thread, frame + 1) = 0x922;
        thread->fieldfc = 0x10;
        return 0x10;
    }

    if (slot == 0x922) {
        obj->field1c = 3;
        obj->field40 = 0;
        return mk3_install(thread, (MK3THREADFUNC)t_backwards_ani2);   /* the slot's contents (difftest) */
    }

    if (slot != 0)
        return -3;

    obj->field40 = 0;
    get_char_ani2(obj);

    obj->field1c = 0x30003;

    *mk3_frame(thread, frame + 1) = 0x8f2;
    thread->frame = thread->frame + 1;   /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_animate_a0_frames;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* tl_do_sg_zap -- armv7 0x0007a640, 196 bytes.  **Complete.**
 *
 *      token == 0:        obj->field20 = 0x17
 *                         obj->a10     = 0
 *                         zap_init_special_act(obj)
 *                         obj->field1c = 4; ochar_sound(obj)
 *                         obj->field40 = 0x24; get_char_ani(obj)
 *                         obj->field1c = 2
 *                         token := 0x996, descend into t_mframew
 *
 *      token == 0x996:    obj->field1c = 3; ochar_sound(obj)
 *                         obj->field38 = t_sg_zap_proc
 *                         create_proj_proc(obj)
 *                         do_next_a9_frame(obj)
 *                         obj->field20 = 0x2a
 *                         frame[frame].handler = tl_do_proj_sitting_duck
 *
 *      otherwise:         return -3
 *
 * Entry 24 of `projectile_jumps`, and it names `t_sg_zap_proc` in 0x38 -- so the
 * routine that seeds the trail counter is started from here, and the whole SG
 * zap is now readable end to end: launcher, projectile, trail callback.
 *
 * **A fourth sitting-duck duration: 0x2a.** With 0x16, 0x18 and 0x23 that is 22,
 * 24, 35 and 42 frames across four moves. The recovery is per-move and the
 * spread is wide enough that no port can guess it.
 *
 * Two `ochar_sound` calls with 4 and 3, one per state -- the second one after
 * the projectile exists, so the launch noise and the throw noise are separate
 * and sixteen-odd frames apart.
 */
long t_sg_zap_proc(MK3THREAD *thread);

long tl_do_sg_zap(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0) {
        obj->field20 = 0x17;
        obj->a10     = 0;
        zap_init_special_act(obj);

        obj->field1c = 4;
        ochar_sound(obj);

        obj->field40 = 0x24;
        get_char_ani(obj);

        obj->field1c = 2;

        *mk3_frame(thread, frame + 1) = 0x996;
        thread->frame = thread->frame + 1;       /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x996)
        return -3;

    obj->field1c = 3;
    ochar_sound(obj);

    obj->field38 = (uint32_t)(uintptr_t)t_sg_zap_proc;
    create_proj_proc(obj);

    do_next_a9_frame(obj);

    obj->field20 = 0x2a;                         /* the sitting-duck duration */

    return mk3_install(thread, (MK3THREADFUNC)tl_do_proj_sitting_duck);
}


/* tl_lk_zap_hi -- armv7 0x0007a4a4, 208 bytes.  **Complete.**
 *
 *      token == 0:        am_i_airborn(obj)
 *                         if (obj->field5c != 0) {
 *                             frame[frame].handler = t_lk_zap_air
 *                             return
 *                         }
 *                         obj->field20 = 0x13
 *                         obj->a10     = 0
 *                         zap_init_special_act(obj)
 *                         obj->field1c = 0; ochar_sound(obj)
 *                         obj->field40 = 0x00040024
 *                         token := 0xa40, descend into t_animate_a9
 *
 *      token == 0xa40:    frame[frame].handler = t_lk_zap_entry
 *
 *      otherwise:         return -3
 *
 * **This closes `t_lk_zap_air`**, written a few batches ago with nothing
 * pointing at it. The split is the first line: ask whether the fighter is off
 * the ground and, if so, **hand the whole move over** rather than branch inside
 * it. The grounded path then does the ordinary setup.
 *
 * So a move with an air version is two routines and one predicate at the top,
 * not one routine with a flag threaded through it. `t_lk_zap_air` sets its own
 * action (0x15) and animation and ends at `t_lk_zap_entry` -- the same place
 * this one ends -- so the two paths rejoin after the setup differs.
 *
 * Entry 22 of `projectile_jumps`; `tl_lk_zap_lo` at 23 is the twin.
 *
 * `obj->field5c` is read into a callee-saved register before the test and then
 * reused as the ZERO written into `a10` and `0x1c` on the grounded path -- the
 * predicate answered no, so the register already holds 0. Transcribed as 0,
 * because that is what it is.
 */
long am_i_airborn(MK3OBJ *obj);
long t_lk_zap_air(MK3THREAD *thread);

long tl_lk_zap_hi(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0) {
        am_i_airborn(obj);

        if (obj->field5c != 0)                   /* the whole move differs */
            return mk3_install(thread, (MK3THREADFUNC)t_lk_zap_air);

        obj->field20 = 0x13;
        obj->a10     = 0;
        zap_init_special_act(obj);

        obj->field1c = 0;
        ochar_sound(obj);

        obj->field40 = 0x00040024;

        *mk3_frame(thread, frame + 1) = 0xa40;
        thread->frame = thread->frame + 1;       /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_animate_a9;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0xa40)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_lk_zap_entry);
}


/* ----------------------------------------------------------------- t_spit_prezap
 *
 * armv7 0x00077210, 232 bytes.  **Complete.**
 *
 * A one-tick lookahead: the free saves `field20`/`field24` on the argument
 * stack, steps one animation frame, and comes back two ticks later at
 * `0x477` with a self-resume rather than a pushed level. `0x477` restores
 * the pair, runs `local_strike_check_box` at action `0x12`, and forks on
 * whether it connected: a hit tail-installs `spit_prezap_hit`; a miss backs
 * up a level, or falls into `t_local_reaction_exit` at the bottom -- the
 * same "pop or exit" idiom the whole file uses to close a check that found
 * nothing.
 *
 *      slot = frame[frame+1].w0
 *      if (slot != 0) {
 *          if (slot != 0x477) return -3
 *          fieldf8-- ; obj->field24 = args[fieldf8]
 *          fieldf8-- ; obj->field20 = args[fieldf8]
 *          obj->field1c = 0x12 ; local_strike_check_box(obj)
 *          if (obj->field5c != 0)
 *              install spit_prezap_hit ; return 0
 *          pop a level, or t_local_reaction_exit at the bottom
 *      }
 *      args[fieldf8] = obj->field20 ; fieldf8++
 *      args[fieldf8] = obj->field24 ; fieldf8++
 *      do_next_a9_frame(obj)
 *      token 0x477 ; fieldfc = 2 ; return 2
 */
void local_strike_check_box(MK3OBJ *obj);
long spit_prezap_hit(struct MK3THREAD *thread);

long t_spit_prezap(MK3THREAD *thread)
{
    MK3OBJ   *obj  = (MK3OBJ *)thread->proc;
    uint32_t *args = (uint32_t *)(void *)thread->args;
    uint32_t  slot = *mk3_frame(thread, thread->frame + 1);

    if (slot != 0) {
        if (slot != 0x477)
            return -3;

        thread->fieldf8 = thread->fieldf8 - 1;
        obj->field24 = args[thread->fieldf8];
        thread->fieldf8 = thread->fieldf8 - 1;
        obj->field20 = args[thread->fieldf8];

        obj->field1c = 0x12;
        local_strike_check_box(obj);

        if (obj->field5c != 0)
            return mk3_install(thread, (MK3THREADFUNC)spit_prezap_hit);

        if ((long)thread->frame > 0) {
            thread->frame = thread->frame - 1;
            return 0;
        }

        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }

    args[thread->fieldf8] = obj->field20;
    thread->fieldf8 = thread->fieldf8 + 1;
    args[thread->fieldf8] = obj->field24;
    thread->fieldf8 = thread->fieldf8 + 1;
    do_next_a9_frame(obj);

    *mk3_frame(thread, thread->frame + 1) = 0x477;
    thread->fieldfc = 2;
    return 2;
}


/* spit_prezap_hit -- armv7 0x0007bb08, 196 bytes.  **Complete.**
 *
 *      token == 0:        stop_a8(part)
 *                         obj->a10 = proc->him
 *                         obj->field40 = 7; get_char_ani2(obj)
 *                         obj->field1c = 0x2f; create_fx(obj)
 *                         match_me_with_him(obj)
 *                         flip_multi(obj)
 *                         obj->field20 = 0
 *                         obj->field1c = -0xc0
 *                         multi_adjust_xy(obj)
 *                         obj->field40 = 6
 *                         obj->field54 = 6 - 2 = 4
 *                         find_ani2_part_a14(obj)
 *                         obj->field1c = 3
 *                         token := 0x4a9, descend into t_mframew
 *
 *      token == 0x4a9:    frame[frame].handler = tl_delete_proj_and_die
 *
 *      otherwise:         return -3
 *
 * **The spit lands ON the victim and then moves back 0xc0.** `match_me_with_him`
 * puts it at the opponent's position, `flip_multi` mirrors it to face the same
 * way, and the shift is horizontal only -- 0 into 0x20 -- so it ends up
 * 0xc0 in front of where the opponent stands rather than on top of them.
 *
 * The order matters and a port must keep it: match, flip, THEN shift. Shifting
 * before the flip would move it the other way, because `multi_adjust_xy` reads
 * the flip bit.
 *
 * `obj->field54 = 4` for `find_ani2_part_a14` -- fourth site for 0x54 as the a14
 * finders' second parameter, and the 6 and the 4 come off one register with a
 * `subs #2`.
 *
 * `obj->a10 = proc->him` is set and never read here, so it is left for whatever
 * runs next -- the same hand-off `t_rocket_explode` makes before calling
 * `benedict_arnold_projectile`.
 */
void stop_a8(MK3OBJ *part);
void match_me_with_him(MK3OBJ *obj);

long spit_prezap_hit(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0) {
        stop_a8(obj->field08);

        obj->a10 = obj->field00->him;

        obj->field40 = 7;
        get_char_ani2(obj);

        obj->field1c = 0x2f;
        create_fx(obj);

        match_me_with_him(obj);
        flip_multi(obj);                         /* before the shift */

        obj->field20 = 0;
        obj->field1c = (uint32_t)~0xbfu;         /* -0xc0, sideways only */
        multi_adjust_xy(obj);

        obj->field40 = 6;
        obj->field54 = 6 - 2;                    /* the same register */
        find_ani2_part_a14(obj);

        obj->field1c = 3;

        *mk3_frame(thread, frame + 1) = 0x4a9;
        thread->frame = thread->frame + 1;       /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x4a9)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)tl_delete_proj_and_die);
}


/* ------------------------------------------------------------------- t_spit_proc
 *
 * armv7 0x0007bbcc, 304 bytes.  **Complete.**
 *
 * The spit's driver: two aim passes on `t_spit_prezap` before it actually
 * throws. The free poses animation 6 (`field54 = 3`, `find_ani2_part_a14`),
 * loads a first box (`field20 = 0x001c005e`, `field24 = 0x000c0034`,
 * transcribed as the raw packed words the binary loads) and descends into
 * `t_spit_prezap` from `0x488`. `0x488` re-loads a second, wider box
 * (`field20 = 0x001c0096`, same `field24`) and checks again from `0x48c`.
 * `0x48c` is where it actually launches -- a flat `0x80000` velocity,
 * `field48 = 0x12`, `field34 = 0` -- on `tl_projectile_flight_call` from
 * `0x493`, whose re-entry installs `spit_prezap_hit`.
 *
 *      slot = frame[frame+1].w0
 *                                          ; 0:     free, then 0x488
 *                                          ; 0x488:  aim pass 2, then 0x48c
 *                                          ; 0x48c:  launch, then 0x493
 *                                          ; 0x493:  install spit_prezap_hit
 *      if (slot == 0x488) {
 *          obj->field20 = 0x001c0096 ; obj->field24 = 0x000c0034
 *          token 0x48c ; frame++ ; install t_spit_prezap ; return 0
 *      }
 *      if (slot < 0x488) {
 *          if (slot != 0) return -3
 *          obj->field40 = 6 ; obj->field54 = 3 ; find_ani2_part_a14(obj)
 *          obj->field20 = 0x001c005e ; obj->field24 = 0x000c0034
 *          token 0x488 ; frame++ ; install t_spit_prezap ; return 0
 *      }
 *      if (slot == 0x493) install spit_prezap_hit ; return 0
 *      if (slot != 0x48c) return -3
 *      obj->field20 = 3 ; obj->field1c = 0x80000 ; set_proj_vel(obj)
 *      obj->field48 = 0x12 ; obj->field34 = 0
 *      token 0x493 ; frame++ ; install tl_projectile_flight_call ; return 0
 */
long t_spit_prezap(struct MK3THREAD *thread);

long t_spit_proc(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t slot  = *mk3_frame(thread, frame + 1);

    if (slot == 0x488) {
        obj->field20 = 0x001c0096;
        obj->field24 = 0x000c0034;

        *mk3_frame(thread, frame + 1) = 0x48c;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_spit_prezap;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot < 0x488) {
        if (slot != 0)
            return -3;

        obj->field40 = 6;
        obj->field54 = 3;
        find_ani2_part_a14(obj);

        obj->field20 = 0x001c005e;
        obj->field24 = 0x000c0034;

        *mk3_frame(thread, frame + 1) = 0x488;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_spit_prezap;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot == 0x493)
        return mk3_install(thread, (MK3THREADFUNC)spit_prezap_hit);

    if (slot != 0x48c)
        return -3;

    obj->field20 = 3;
    obj->field1c = 0x80000;
    set_proj_vel(obj);

    obj->field48 = 0x12;
    obj->field34 = 0;

    *mk3_frame(thread, frame + 1) = 0x493;
    thread->frame = thread->frame + 1;   /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)tl_projectile_flight_call;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ---------------------------------------------------------------------- tl_do_spit
 *
 * armv7 0x0007ad24, 276 bytes.  **Complete.**
 *
 * `t_spit_proc`'s driver. The free packs `0x00030006` (rate 3, animation
 * 6) and descends into `t_animate2_a9` from `0x4b6`. `0x4b6` launches --
 * `field38 = t_spit_proc` into `create_proj_proc`, `field1c =
 * obj->field00->field18 = 0x604` -- then a bare thirty-one-tick wait for
 * `0x4bd`, no push. `0x4bd` pushes `t_mframew` from `0x4bf`, and `0x4bf`
 * is the floor: pop a level, or `t_local_reaction_exit` at the bottom, the
 * same idiom the whole file closes on.
 *
 *      slot = frame[frame+1].w0
 *                                          ; 0:    free, then 0x4b6
 *                                          ; 0x4b6: launch, then 0x4bd (wait)
 *                                          ; 0x4bd: wait, then 0x4bf
 *                                          ; 0x4bf: pop, or exit at the bottom
 *      if (slot == 0x4b6) {
 *          obj->field38 = t_spit_proc ; create_proj_proc(obj)
 *          obj->field1c = obj->field00->field18 = 0x604
 *          token 0x4bd ; fieldfc = 0x1f ; return 0x1f
 *      }
 *      if (slot < 0x4b6) {
 *          if (slot != 0) return -3
 *          obj->field20 = 0x23 ; obj->a10 = 0 ; zap_init_special_act(obj)
 *          obj->field1c = 8 ; ochar_sound(obj)
 *          obj->field40 = 0x00030006
 *          token 0x4b6 ; frame++ ; install t_animate2_a9 ; return 0
 *      }
 *      if (slot == 0x4bd) {
 *          obj->field1c = 4
 *          token 0x4bf ; frame++ ; install t_mframew ; return 0
 *      }
 *      if (slot != 0x4bf) return -3
 *      pop a level, or t_local_reaction_exit at the bottom
 */
long t_spit_proc(struct MK3THREAD *thread);

long tl_do_spit(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t slot  = *mk3_frame(thread, frame + 1);

    if (slot == 0x4b6) {
        obj->field38 = (uint32_t)(uintptr_t)t_spit_proc;
        create_proj_proc(obj);

        obj->field1c = 0x604;
        obj->field00->field18 = 0x604;

        *mk3_frame(thread, frame + 1) = 0x4bd;
        thread->fieldfc = 0x1f;
        return 0x1f;
    }

    if (slot < 0x4b6) {
        if (slot != 0)
            return -3;

        obj->field20 = 0x23;
        obj->a10     = 0;
        zap_init_special_act(obj);

        obj->field1c = 8;
        ochar_sound(obj);

        obj->field40 = 0x00030006;

        *mk3_frame(thread, frame + 1) = 0x4b6;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_animate2_a9;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot == 0x4bd) {
        obj->field1c = 4;

        *mk3_frame(thread, frame + 1) = 0x4bf;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot != 0x4bf)
        return -3;

    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
        return 0;
    }
    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* tl_lk_zap_lo -- armv7 0x0007a574, 204 bytes.  **Complete.**
 *
 *      token == 0:        am_i_airborn(obj)
 *                         if (obj->field5c != 0) {
 *                             frame[frame].handler = t_lk_zap_air
 *                             return
 *                         }
 *                         obj->field20 = 0x14
 *                         obj->a10     = 0
 *                         zap_init_special_act(obj)
 *                         obj->field1c = 0; ochar_sound(obj)
 *                         obj->field40 = 0x40000
 *                         token := 0xa30, descend into t_animate2_a9
 *
 *      token == 0xa30:    frame[frame].handler = t_lk_zap_entry
 *
 *      otherwise:         return -3
 *
 * **The twin of `tl_lk_zap_hi`, four constants apart**: action 0x14 against
 * 0x13, animation 0x40000 against 0x00040024, `t_animate2_a9` against
 * `t_animate_a9`, token 0xa30 against 0xa40. The airborne branch and the ending
 * are identical, so the high and low versions of Liu Kang's zap share their air
 * form and their entry point and differ only in the grounded setup.
 *
 * **`obj->field40 = 0x40000` is a second large value passed to
 * `t_animate2_a9`.** mkfatal.c's `t_lia_scream_rip` note flagged the first one
 * and said not to assume the packed-pair reading of 0x40 survives. It does not:
 * the four small sites read 0x0002000c, 0x0005000a, 0x00060006 and 0x00040018,
 * and these two read 0x40000 and -- next door -- 0x00040024. **A field that
 * takes both a packed pair and a plain magnitude, like `t_shake_ob_up`'s 0x1c
 * before it.** Six sites and the reading is still not settled; it is recorded
 * as unsettled rather than picked.
 */
long t_lk_zap_entry(MK3THREAD *thread);
long t_animate2_a9(MK3THREAD *thread);           /* pointer slot 0x000f36c0 */

long tl_lk_zap_lo(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0) {
        am_i_airborn(obj);

        if (obj->field5c != 0)
            return mk3_install(thread, (MK3THREADFUNC)t_lk_zap_air);

        obj->field20 = 0x14;
        obj->a10     = 0;
        zap_init_special_act(obj);

        obj->field1c = 0;
        ochar_sound(obj);

        obj->field40 = 0x40000;              /* not a packed pair */

        *mk3_frame(thread, frame + 1) = 0xa30;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_animate2_a9;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0xa30)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_lk_zap_entry);
}


/* t_rzap3 -- armv7 0x00077b20, 212 bytes.  **Complete.**
 *
 *      token == 0:         PUSH obj->field38
 *                          token := 0x113c, descend into t_robo_open_chest_fast
 *
 *      token == 0x113c:    POP  obj->field38
 *                          slave = create_proj_proc(obj)
 *                          obj->field30 = slave->field08
 *                          obj->field1c = 7
 *                          obj->field20 = 7 + 0x26 = 0x2d
 *                          adjust_xy_a5(obj)
 *                          i_am_a_sitting_duck(obj)
 *                          token := 0x1156, park 0x20
 *
 *      token == 0x1156:    frame[frame].handler = t_robo_close_chest
 *
 *      otherwise:          return -3
 *
 * **Twentieth argument-stack site, and the clearest example of why it exists.**
 * `obj->field38` holds the handler the projectile will run, and it has to
 * survive the descent into `t_robo_open_chest_fast` -- a descent, so the routine
 * RETURNS and a register cannot carry it. That is exactly the rule `t_sg_pound`
 * settled in mkfatal.c, and here it is with nothing else in the way: push,
 * descend, pop, use.
 *
 * **This is what found `create_proj_proc`'s return value.** The instruction
 * after the call is `ldr r3, [r0, #8]` -- the slave's part, read straight out of
 * `r0`. That routine was transcribed as `void` because the two callers written
 * at the time ignored the result, which compiled and ran. Its note now says so.
 *
 * `i_am_a_sitting_duck` rather than the `tl_do_proj_sitting_duck` handler the
 * other launchers install: the twelve-byte leaf just announces the action and
 * this routine parks 0x20 itself. Two ways to be a sitting duck, and only the
 * handler form takes a per-move duration.
 *
 * `adjust_xy_a5` with the slave's part in 0x30 and (7, 0x2d) as the offset --
 * the second site for that three-argument placement, after `tl_do_sz_zap`. The
 * 7 and the 0x2d come off one register with an `adds #0x26`.
 */
long t_robo_open_chest_fast(MK3THREAD *thread);
long t_robo_close_chest(MK3THREAD *thread);
void i_am_a_sitting_duck(MK3OBJ *obj);

long t_rzap3(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);
    MK3OBJ  *slave;
    uint32_t argc;

    if (token == 0) {
        argc = thread->fieldf8;
        *mk3_arg(thread, argc) = obj->field38;   /* must cross a descent */
        thread->fieldf8 = argc + 1;

        *mk3_frame(thread, frame + 1) = 0x113c;
        thread->frame = thread->frame + 1;       /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_robo_open_chest_fast;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x113c) {
        argc = thread->fieldf8 - 1;
        thread->fieldf8 = argc;
        obj->field38 = *mk3_arg(thread, argc);

        slave = create_proj_proc(obj);
        obj->field30 = (uint32_t)(uintptr_t)slave->field08;

        obj->field1c = 7;
        obj->field20 = 7 + 0x26;                 /* the same register */
        adjust_xy_a5(obj);

        i_am_a_sitting_duck(obj);

        *mk3_frame(thread, frame + 1) = 0x1156;
        thread->fieldfc = 0x20;
        return 0x20;
    }

    if (token != 0x1156)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_robo_close_chest);
}


/* tl_do_osz_zap -- armv7 0x0007af28, 176 bytes.  **Complete.**
 *
 *      token == 0:        obj->field20 = 0x1a
 *                         obj->a10     = 0
 *                         zap_init_special_act(obj)
 *                         obj->field1c = 0xb; ochar_sound(obj)
 *                         obj->field40 = 0x00040011
 *                         token := 0x199, descend into t_animate2_a9
 *
 *      token == 0x199:    obj->field1c = 0x30
 *                         obj->field20 = 0x30 + 8 = 0x38
 *                         obj->field30 = proc->slave
 *                         adjust_xy_a5(obj)
 *                         frame[frame].handler = t_osz_forward_entry
 *
 *      otherwise:         return -3
 *
 * **`tl_do_sz_zap` without the cursor write.** The two are the same routine with
 * the same action number (0x1a), the same placement (0x30, 0x38 off
 * `proc->slave`) and the same ending -- and the Sub-Zero version additionally
 * points the slave at `sz_ani_data + 0x1220` while this one leaves whatever
 * `create_proj_proc` set.
 *
 * So the older and the classic Sub-Zero share a move and differ in one
 * assignment. Entry 42 of `projectile_jumps` against 12.
 *
 * The sound is 0xb here and 0 there, and the animation 0x00040011 against
 * 0x00030024 -- so the two do look different on screen; it is the machinery that
 * is shared, not the presentation.
 */

long tl_do_osz_zap(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0) {
        obj->field20 = 0x1a;
        obj->a10     = 0;
        zap_init_special_act(obj);

        obj->field1c = 0xb;
        ochar_sound(obj);

        obj->field40 = 0x00040011;

        *mk3_frame(thread, frame + 1) = 0x199;
        thread->frame = thread->frame + 1;       /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_animate2_a9;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x199)
        return -3;

    obj->field1c = 0x30;
    obj->field20 = 0x30 + 8;                     /* the same register */
    obj->field30 = obj->field00->slave;
    adjust_xy_a5(obj);

    return mk3_install(thread, (MK3THREADFUNC)t_osz_forward_entry);
}


/* tl_do_floor_ice -- armv7 0x0007afd8, 216 bytes.  **Complete.**
 *
 *      token == 0:        obj->field20 = 0x26
 *                         obj->a10     = 0
 *                         zap_init_special_act(obj)
 *                         obj->field1c = 0xe; ochar_sound(obj)
 *                         obj->field40 = 0x00030010
 *                         token := 0x170, descend into t_animate2_a9
 *
 *      token == 0x170:    obj->field38 = t_floor_ice_proc
 *                         slave = create_proj_proc(obj)
 *                         slave->thread->pid = proc->field08 + 0x700 + 7
 *                         obj->field1c = G + 0x420 + 0xc; update_tsl(obj)
 *                         obj->field20 = 0x20
 *                         frame[frame].handler = tl_do_proj_sitting_duck
 *
 *      otherwise:         return -3
 *
 * **It overwrites the pid `create_proj_proc` just computed.** That routine sets
 * `strength + 0x700`; this one immediately rewrites the same word as
 * `strength + 0x700 + 7`. So **0x700 is the family tag and the low bits
 * distinguish members of it** -- the floor ice is findable separately from every
 * other projectile the same fighter can have out.
 *
 * That is the first evidence in the tree that a pid carries more than "whose
 * projectile". A port that collapses the two writes into one loses the
 * distinction, and `FindThread` is how something else goes looking.
 *
 * The rewrite is done through the slave's own thread -- `slave->thread->pid` --
 * which is what `create_proj_proc`'s return value is for, and the second caller
 * to use it after `t_rzap3`.
 *
 * `G + 0x42c` is another slot in the timer block that starts at `G + 0x410`;
 * fourth measured, after Jax's 0x410, Jade's 0x41c and Reptile's 0x438/0x43c.
 * The offset arrives as `add.w #0x420` then `adds #0xc`, so it looks like 0x420
 * at a glance and is not.
 *
 * A fifth sitting-duck duration: 0x20.
 */
long t_floor_ice_proc(MK3THREAD *thread);

long tl_do_floor_ice(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);
    MK3OBJ  *slave;

    if (token == 0) {
        obj->field20 = 0x26;
        obj->a10     = 0;
        zap_init_special_act(obj);

        obj->field1c = 0xe;
        ochar_sound(obj);

        obj->field40 = 0x00030010;

        *mk3_frame(thread, frame + 1) = 0x170;
        thread->frame = thread->frame + 1;       /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_animate2_a9;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x170)
        return -3;

    obj->field38 = (uint32_t)(uintptr_t)t_floor_ice_proc;
    slave = create_proj_proc(obj);

    /* create_proj_proc just set this to strength + 0x700; the low bits are
     * what make this projectile findable apart from the others. */
    slave->thread->pid = obj->field00->field08 + 0x700 + 7;

    obj->field1c = (uint32_t)(uintptr_t)(G_BYTES + 0x420 + 0xc);
    update_tsl(obj);

    obj->field20 = 0x20;                         /* the sitting-duck duration */

    return mk3_install(thread, (MK3THREADFUNC)tl_do_proj_sitting_duck);
}


/* -------------------------------------------------------------------- t_floor_ice_proc
 *
 * armv7 0x0007bcfc, 428 bytes.  **Complete.**
 *
 * `tl_do_floor_ice`'s own `field38` callback: a puddle of ice that skates
 * along the floor. The free zeroes `field1c`/`field00->field18`, poses
 * animation `0x10`, and pushes `t_mframew` under `0x120`, whose own
 * re-entry just sets `field48 = 0x50` (a countdown) and re-arms itself
 * (token `0x124`, self) for one tick.
 *
 * `0x124` checks `q_is_he_a_boss` first; a boss skips straight to the
 * countdown check below. Otherwise it reads a signed halfword,
 * `G+0x45c`, into `field1c` -- a stage-specific slide speed, since
 * nothing here that is fixed to this move reads a variable G slot
 * otherwise -- and if it is zero, falls into the strike attempt; if it is
 * not, poses animation `0x10` again and pushes `t_mframew` under `0x161`.
 *
 * `0x161` is the exit watch: `get_his_action`, and while the opponent's
 * action stays `0x628` it just waits three ticks under itself; anything
 * else re-poses and re-arms `0x161` again -- the SAME physical block
 * `0x124`'s own "slide speed nonzero" branch reaches, one push site taken
 * two ways.
 *
 * The strike attempt (`field1c=0x18`, `proj_strike_check`) either counts
 * `field48` down and re-arms `0x124` while it is still positive (falling
 * to the pose-and-wait block once it isn't, same as a boss), or on a hit
 * reads a bounds object -- `GrObj` itself when `thread->pid == 0x707`,
 * `GrObj + GROBJ_STRIDE` (the SECOND fighter's GrObj) otherwise, the same
 * `pid == strength + 0x700 + 7` identity `tl_do_floor_ice` stamped on the
 * slave -- through `leftmost_mpart_ob`/`rightmost_mpart_ob`, and writes
 * the pair straight into the opponent's own `a10`/`field48`: a floor
 * effect that clamps where the OTHER fighter can stand, not itself.
 */
extern char *GrObj;                       /* slot 0x000f320c -> 0x0038c698 */
void q_is_he_a_boss(MK3OBJ *obj);
void get_his_action(MK3OBJ *obj);
void leftmost_mpart_ob(MK3OBJ *out, MK3OBJ *src);
void rightmost_mpart_ob(MK3OBJ *out, MK3OBJ *src);

long t_floor_ice_proc(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t slot  = *mk3_frame(thread, frame + 1);

    if (slot == 0x120) {
        obj->field48 = 0x50;

d_tail:
        *mk3_frame(thread, frame + 1) = 0x124;
        thread->fieldfc = 1;
        return 1;
    }

    if (slot == 0x124) {
        q_is_he_a_boss(obj);
        if (obj->field5c != 0)
            goto countdown;

        {
            int16_t speed = *(int16_t *)(G_BYTES + 0x45c);
            obj->field1c = (uint32_t)(int32_t)speed;
            if (speed == 0)
                goto strike_attempt;
        }

ab_shared:
        obj->field40 = 0x10;
        obj->field54 = 4;
        find_ani2_part_a14(obj);

        obj->field1c = 4;

        *mk3_frame(thread, frame + 1) = 0x161;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot == 0x156) {
        get_his_action(obj);
        if (obj->field20 != 0x628)
            goto ab_shared;

b_tail:
        *mk3_frame(thread, frame + 1) = 0x156;
        thread->fieldfc = 3;
        return 3;
    }

    if (slot == 0x161)
        return mk3_install(thread, (MK3THREADFUNC)tl_delete_proj_and_die);

    if (slot != 0)
        return -3;

    obj->field1c          = 0;
    obj->field00->field18 = 0;
    obj->field54           = 3;
    obj->field40            = 0x10;
    find_ani2_part_a14(obj);

    obj->field1c = 3;

    *mk3_frame(thread, frame + 1) = 0x120;
    thread->frame = thread->frame + 1;   /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_mframew;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;

strike_attempt:
    obj->field1c = 0x18;
    proj_strike_check(obj);

    if (obj->field5c == 0)
        goto countdown;

    {
        MK3OBJ *src;
        MK3OBJ *him;

        if (thread->pid == 0x707)
            src = (MK3OBJ *)(void *)GrObj;
        else
            src = (MK3OBJ *)(void *)(GrObj + GROBJ_STRIDE);

        leftmost_mpart_ob(obj, src);
        rightmost_mpart_ob(obj, src);

        him = (MK3OBJ *)(void *)(uintptr_t)obj->field00->field00;
        him->a10     = obj->field24;
        him->field48 = obj->field28;
    }
    goto b_tail;

countdown:
    obj->field48 = obj->field48 - 1;
    if (obj->field48 != 0)
        goto d_tail;

    goto ab_shared;
}


/* t_rocket1_proc -- armv7 0x00077bf4, 216 bytes.  **Complete.**
 *
 *      token == 0:         obj->field1c = 3; ochar_sound(obj)
 *                          obj->field40 = 0x3f; get_char_ani(obj)
 *                          obj->field54 = 4; find_part_a14(obj)
 *                          obj->field1c = 0x70000
 *                          q_his_react_flag_set(obj)
 *                          if (obj->field5c == 0) obj->field1c = 0x40000
 *                          obj->field20 = 3
 *                          set_proj_vel(obj)
 *                          obj->field1c  = 0
 *                          proc->field30 = 0
 *                          obj->field48 = 0x12
 *                          obj->field34 = t_rocket1_flight_call
 *                          token := 0x1122, descend into tl_projectile_flight_call
 *
 *      token == 0x1122:    frame[frame].handler = t_rocket_explode_fx
 *
 *      otherwise:          return -3
 *
 * **The rocket launches faster at an opponent who is reacting.**
 * `q_his_react_flag_set` is asked, and the answer chooses between 0x70000 and
 * 0x40000 -- nearly double. So a rocket fired at someone already in a reaction
 * travels at almost twice the speed of one fired at a standing opponent.
 *
 * That is the first measured USE of that predicate, and it matters because the
 * routine itself is the odd one in this file: it fetches the other fighter's
 * proc 0x10 into 0x2c and then answers from `obj->field54`, a field it never
 * writes. **And this caller sets 0x54 four instructions earlier**, for
 * `find_part_a14`. So either the predicate is reading a leftover from the a14
 * finder, or the two uses of 0x54 are unrelated and it is reading rubbish.
 *
 * Recorded, not resolved. What is certain is that the launch speed of a rocket
 * depends on a field the animation lookup just used, and that a port which
 * "tidies" either side changes how fast the rocket flies.
 *
 * **The negation is spelled as an addition.** `obj->field1c = 0x40000` is
 * `add.w r3, r3, #0x40000` on the zero the branch has just proved, not a fresh
 * load -- so the slow speed is reached from the failed comparison rather than
 * from a literal.
 *
 * It closes `t_rocket1_flight_call`, the callback that accelerates by a
 * sixteenth a frame and clamps at 0xe0000. Fifth instance of the
 * 0x34 -> `proc->field28` chain.
 */
void q_his_react_flag_set(MK3OBJ *obj);
void find_part_a14(MK3OBJ *obj);
long t_rocket1_flight_call(MK3THREAD *thread);

long t_rocket1_proc(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0) {
        obj->field1c = 3;
        ochar_sound(obj);

        obj->field40 = 0x3f;
        get_char_ani(obj);

        obj->field54 = 4;
        find_part_a14(obj);

        obj->field1c = 0x70000;
        q_his_react_flag_set(obj);
        if (obj->field5c == 0)
            obj->field1c = 0x40000;          /* not reacting: slower */

        obj->field20 = 3;
        set_proj_vel(obj);

        obj->field1c          = 0;
        obj->field00->field30 = 0;

        obj->field48 = 0x12;
        obj->field34 = (uint32_t)(uintptr_t)t_rocket1_flight_call;

        *mk3_frame(thread, frame + 1) = 0x1122;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)tl_projectile_flight_call;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x1122)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_rocket_explode_fx);
}


/* tl_kit_zap_air -- armv7 0x0007b654, 220 bytes.  **Complete.**
 *
 *      token == 0:        obj->a10 = 0
 *                         zap_air_init_special(obj)
 *                         obj->field20  = 0x1f
 *                         proc->field18 = 0x1f
 *                         obj->field40 = 0; get_char_ani2(obj)
 *                         obj->field1c = 2
 *                         token := 0x630, descend into t_mframew
 *
 *      token == 0x630:    obj->field38 = tl_fan_proc
 *                         create_proj_proc(obj)
 *                         detach_proj(obj)
 *                         obj->field1c = 3
 *                         token := 0x63f, descend into t_mframew
 *
 *      token == 0x63f:    frame[frame].handler = t_drop_down_land
 *
 *      otherwise:         return -3
 *
 * **It makes the projectile and lets it go in the same breath.**
 * `create_proj_proc` fills `proc->field64` and `proc->slave`; `detach_proj`
 * clears both, two instructions later, without killing anything. So the fan
 * exists and the fighter stops owning it immediately -- which is the first
 * caller measured for `detach_proj` and the reason that routine is not simply a
 * weaker `delete_slave`.
 *
 * A thrown thing you keep is a slave; a thrown thing you forget is a fan. The
 * difference is two instructions.
 *
 * **It ends in `t_drop_down_land`**, not in a sitting duck -- because the
 * fighter is in the air and has to come down before anything else happens. The
 * grounded zaps park; this one falls.
 *
 * The tail at 0x7b6d2 serves both an install and a descent, the same nine
 * instructions reached with a different frame index in `r2`. State 0x63f arrives
 * with the entry index and installs; state 0x630 arrives after incrementing and
 * descends.
 */
long tl_fan_proc(MK3THREAD *thread);
long t_drop_down_land(MK3THREAD *thread);        /* pointer slot 0x000f33d4 */
void detach_proj(MK3OBJ *obj);


/* ------------------------------------------------------------------ tl_do_kitana_zap
 *
 * armv7 0x0007a97c, 384 bytes.  **Complete.**
 *
 * The free plays the sound and updates the tracker (`field1c = G + 0x414`)
 * before testing `am_i_airborn` -- unlike `tl_do_mileena_zap`/
 * `tl_do_tusk_zap`, which fork on the FIRST instruction, this one always
 * does the shared setup first and only then hands off to `tl_kit_zap_air`
 * when airborne. Grounded, it packs `0x00030024` and descends into
 * `t_animate_a9` from `0x656`. `0x656` launches `tl_fan_proc` (not yet
 * decompiled) through `create_proj_proc` and waits on `t_mframew` from
 * `0x665`; `0x665` tags the action to `0x604` and self-parks `0x668` --
 * `0x604 + 0x64`, computed rather than loaded as its own literal, the only
 * site in this session's batch that derives one token from another. `0x668`
 * pushes a plain `t_mframew` wait from `0x66a`, the ordinary
 * pop-or-exit-at-the-bottom floor.
 *
 *      slot = frame[frame+1].w0
 *                                          ; 0:     free; airborne -> tl_kit_zap_air
 *                                          ;        grounded, then 0x656
 *                                          ; 0x656:  launch, then 0x665 (wait)
 *                                          ; 0x665:  tag action, then 0x668 (wait)
 *                                          ; 0x668:  wait, then 0x66a
 *                                          ; 0x66a:  pop, or exit at the bottom
 *      if (slot == 0x665) {
 *          obj->field1c = obj->field00->field18 = 0x604
 *          token 0x668 ; fieldfc = 0x20 ; return 0x20
 *      }
 *      if (slot < 0x665) {
 *          if (slot == 0) {
 *              obj->field1c = 0 ; ochar_sound(obj)
 *              obj->field1c = G + 0x414 ; update_tsl(obj)
 *              if (am_i_airborn(obj)) install tl_kit_zap_air ; return 0
 *              obj->a10 = 0 ; obj->field20 = 0x1e ; zap_init_special_act(obj)
 *              obj->field40 = 0x00030024
 *              token 0x656 ; frame++ ; install t_animate_a9 ; return 0
 *          }
 *          if (slot != 0x656) return -3
 *          obj->field38 = tl_fan_proc ; create_proj_proc(obj) ; obj->field1c = 3
 *          token 0x665 ; frame++ ; install t_mframew ; return 0
 *      }
 *      if (slot == 0x668) {
 *          obj->field1c = 3
 *          token 0x66a ; frame++ ; install t_mframew ; return 0
 *      }
 *      if (slot != 0x66a) return -3
 *      pop a level, or t_local_reaction_exit at the bottom
 */
long tl_kit_zap_air(struct MK3THREAD *thread);

long tl_do_kitana_zap(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t slot  = *mk3_frame(thread, frame + 1);

    if (slot == 0x665) {
        obj->field1c = 0x604;
        obj->field00->field18 = 0x604;

        *mk3_frame(thread, frame + 1) = 0x668;
        thread->fieldfc = 0x20;
        return 0x20;
    }

    if (slot < 0x665) {
        if (slot == 0) {
            obj->field1c = 0;
            ochar_sound(obj);

            obj->field1c = (uint32_t)(uintptr_t)((char *)G + 0x414);
            update_tsl(obj);

            if (am_i_airborn(obj) != 0)
                return mk3_install(thread, (MK3THREADFUNC)tl_kit_zap_air);

            obj->a10    = 0;
            obj->field20 = 0x1e;
            zap_init_special_act(obj);

            obj->field40 = 0x00030024;

            *mk3_frame(thread, frame + 1) = 0x656;
            thread->frame = thread->frame + 1;   /* push a level */
            mk3_frame(thread, thread->frame)[1] =
                (uint32_t)(uintptr_t)t_animate_a9;
            *mk3_frame(thread, thread->frame + 1) = 0;
            return 0;
        }

        if (slot != 0x656)
            return -3;

        obj->field38 = (uint32_t)(uintptr_t)tl_fan_proc;
        create_proj_proc(obj);
        obj->field1c = 3;

        *mk3_frame(thread, frame + 1) = 0x665;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot == 0x668) {
        obj->field1c = 3;

        *mk3_frame(thread, frame + 1) = 0x66a;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot != 0x66a)
        return -3;

    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
        return 0;
    }
    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* ----------------------------------------------------------------------- tl_fan_proc
 *
 * armv7 0x00078c64, 448 bytes.  **Complete.**
 *
 * `tl_do_kitana_zap`'s own `field38` callback. The free poses animation
 * `0x24`, throws (`field20=1`, `field1c=0x80000`, `set_proj_vel`), zeroes
 * `field34`, tags `field48=0x13`, and pushes `tl_projectile_flight_call`
 * under `0x5e2`.
 *
 * `0x5e2` is a fork on the GrObj's own `field18`: still zero (never turned)
 * plays a return sound, `stop_a8`s the part, and lines the part up on the
 * opponent right away (`aim`); already turned instead flips the throw
 * (`aim`'s own setup: `field1c=3`/`ochar_sound`, `field1c=1`/`init_anirate`,
 * `field20`/`field08->field1c=0xfffb0000`, `field08->field18` negated in
 * place) and falls into `watch` -- a re-throw with no strike this tick.
 *
 * `0x600` checks the opponent's installed proc against `t_rhat_wake` and
 * `t_rhat_sleep` -- the readied-stinger identity check every latch site in
 * this file uses. A match skips straight to `aim` (a real strike this
 * tick); neither match re-throws (`field20=5`, `field1c=0xb0000`,
 * `set_proj_vel`) and falls into the SAME flip-and-`watch` code `0x5e2`'s
 * "already turned" branch uses -- one physical block reached two ways.
 *
 * `aim` lines the part up on the opponent (`match_me_with_him`/
 * `flip_multi`, `field20` saved across the pair on `thread->args[]` the
 * same way every other such pair in this file does), repositions
 * (`multi_adjust_xy` at `field1c=0`), steps two frames, and waits one tick
 * under `0x600` again -- reached only from `0x5e2`'s "never turned" branch
 * and `0x600`'s own sleeping-stinger match, never from the flip-and-rethrow
 * path.
 *
 * `0x61b` is the exit watch: `proj_onscreen_test` -- answered directly in
 * its return value, not through `field5c` -- keeps it alive with
 * `next_anirate` and a re-arm while the fan is still on screen, and
 * installs `tl_delete_proj_and_die` the instant it isn't.
 */
long t_rhat_wake(struct MK3THREAD *thread);       /* pointer slot 0x000f33cc */
long t_rhat_sleep(struct MK3THREAD *thread);      /* pointer slot 0x000f33c4 */
long tl_projectile_flight_call(struct MK3THREAD *thread);

long tl_fan_proc(MK3THREAD *thread)
{
    MK3OBJ   *obj   = (MK3OBJ *)thread->proc;
    uint32_t *args  = (uint32_t *)(void *)thread->args;
    uint32_t  frame = thread->frame;
    uint32_t  slot  = *mk3_frame(thread, frame + 1);

    if (slot == 0x600) {
        void *installed = GetProcFunc(obj->field00->field00);

        if ((uintptr_t)installed == (uintptr_t)t_rhat_wake ||
            (uintptr_t)installed == (uintptr_t)t_rhat_sleep)
            goto aim;

        obj->field20 = 5;
        obj->field1c = 0xb0000;
        set_proj_vel(obj);

        goto strike;
    }

    if (slot == 0x5e2) {
        if (obj->field18 == 0) {
            obj->field1c = 1;
            ochar_sound(obj);

            stop_a8(obj->field08);
            goto aim;
        }

        goto strike;
    }

    if (slot == 0x61b) {
        if (proj_onscreen_test(obj) == 0)
            return mk3_install(thread, (MK3THREADFUNC)tl_delete_proj_and_die);

        goto watch;
    }

    if (slot != 0)
        return -3;

    obj->field40 = 0x24;
    obj->field54 = 4;
    find_ani_part_a14(obj);

    obj->field20 = 1;
    obj->field1c = 0x80000;
    set_proj_vel(obj);

    obj->field34 = 0;
    obj->field48 = 0x13;

    *mk3_frame(thread, frame + 1) = 0x5e2;
    thread->frame = thread->frame + 1;   /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)tl_projectile_flight_call;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;

strike:
    obj->field1c = 3;
    ochar_sound(obj);

    obj->field1c = 1;
    init_anirate(obj);

    obj->field20           = 0xfffb0000;
    obj->field08->field1c  = 0xfffb0000;

    obj->field08->field18 = (uint32_t)(-(long)obj->field08->field18);
    obj->field1c          = obj->field08->field18;

watch:
    next_anirate(obj);

    *mk3_frame(thread, frame + 1) = 0x61b;
    thread->fieldfc = 1;
    return 1;

aim:
    obj->field20 = MK3_FIELD12_S(obj->field08);

    args[thread->fieldf8] = obj->field20;
    thread->fieldf8 = thread->fieldf8 + 1;

    match_me_with_him(obj);
    flip_multi(obj);

    thread->fieldf8 = thread->fieldf8 - 1;
    MK3_SET_FIELD12(obj->field08, (uint16_t)args[thread->fieldf8]);

    obj->field1c = (uint32_t)~0x2f;
    obj->field20 = (uint32_t)~0x2f + 0x30;
    multi_adjust_xy(obj);

    do_next_a9_frame(obj);
    do_next_a9_frame(obj);

    *mk3_frame(thread, frame + 1) = 0x600;
    thread->fieldfc = 1;
    return 1;
}


long tl_kit_zap_air(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);
    uint32_t next;

    if (token == 0x63f)
        return mk3_install(thread, (MK3THREADFUNC)t_drop_down_land);

    if (token == 0) {
        obj->a10 = 0;
        zap_air_init_special(obj);

        obj->field20          = 0x1f;        /* one register, two fields */
        obj->field00->field18 = 0x1f;

        obj->field40 = 0;
        get_char_ani2(obj);

        obj->field1c = 2;

        next = 0x630;

    } else if (token == 0x630) {
        obj->field38 = (uint32_t)(uintptr_t)tl_fan_proc;
        create_proj_proc(obj);
        detach_proj(obj);                    /* made, then let go */

        obj->field1c = 3;

        next = 0x63f;

    } else {
        return -3;
    }

    *mk3_frame(thread, frame + 1) = next;
    thread->frame = thread->frame + 1;       /* push a level */
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* t_st_zap_jsrp -- armv7 0x0007659c, 224 bytes.  **Complete.**
 *
 *      token == 0:        obj->field1c = 3; ochar_sound(obj)
 *                         obj->field40 = 0x24; get_char_ani(obj)
 *                         obj->field1c = 3
 *                         obj->field40 = obj->a10 + obj->field40
 *                         token := 0xaba, descend into t_mframew
 *
 *      token == 0xaba:    obj->field38 = t_skull_proc
 *                         create_proj_proc(obj)
 *                         obj->field1c = G + 0x440; update_tsl(obj)
 *                         pop a level, or t_local_reaction_exit at the bottom
 *
 *      otherwise:         return -3
 *
 * **`obj->a10` is added to the resolved animation cursor**, which is a use of
 * that slot the tree has not seen before: not an argument, not a counter, not a
 * pointer, but a per-variant OFFSET into the animation the finder just returned.
 *
 * `projectile_jumps` entries 19, 20 and 21 -- `tl_do_st_zap1`, `_zap2` and
 * `_zap3` -- looked like the obvious source. **They are not, and the guess is
 * left here because it was wrong.** Those three were read straight after this
 * one: they are identical apart from a single constant, and it goes into
 * `obj->field48` (1, 2, 3) and `obj->field20` (that plus 0xf), never into
 * `a10`. They also install `t_stz1`, not this routine.
 *
 * So the shape was right -- three moves, one routine, one constant -- and the
 * field was wrong. **Whatever fills `a10` before this runs is still unknown**,
 * and a port must not assume it is zero: this adds it to a real animation
 * cursor.
 *
 * It ends by popping rather than installing, so whatever descended into this
 * gets control back after the projectile exists -- unlike the `tl_do_*` entries,
 * which install a sitting duck and are finished.
 *
 * `G + 0x440` is a fifth slot in the timer block after 0x410, 0x41c, 0x42c and
 * 0x438/0x43c.
 *
 * `lsl.w r3, r2, r8` with `r8` holding 3 -- the register-shift spelling of the
 * frame stride, second site after `t_motaro_zap_proc`. Same arithmetic.
 */
long t_skull_proc(MK3THREAD *thread);

long t_st_zap_jsrp(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0) {
        obj->field1c = 3;
        ochar_sound(obj);

        obj->field40 = 0x24;
        get_char_ani(obj);

        obj->field1c = 3;
        obj->field40 = obj->a10 + obj->field40;   /* the variant's offset */

        *mk3_frame(thread, frame + 1) = 0xaba;
        thread->frame = thread->frame + 1;        /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0xaba)
        return -3;

    obj->field38 = (uint32_t)(uintptr_t)t_skull_proc;
    create_proj_proc(obj);

    obj->field1c = (uint32_t)(uintptr_t)(G_BYTES + 0x440);
    update_tsl(obj);

    if ((long)thread->frame > 0) {                /* cmp #0 / ble: signed */
        thread->frame = thread->frame - 1;        /* back up a level */
        return 0;
    }

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* t_sai3 -- armv7 0x00078818, 220 bytes.  **Complete.**
 *
 *      token == 0:        multi_adjust_xy(obj)
 *                         obj->field40 = 0x15; get_char_ani2(obj)
 *                         obj->field1c = 0xb0000
 *                         obj->field20 = 4
 *                         set_proj_vel(obj)
 *                         obj->field20 = 3
 *                         obj->a10     = 3
 *                         obj->field48 = 0x17
 *                         token := 0x1ca, descend into tl_projectile_flight
 *
 *      token == 0x1ca:    stop_a8(part)
 *                         obj->field40 = 0x15; find_ani2_part2(obj)
 *                         obj->field1c = 3
 *                         token := 0x1d0, descend into t_mframew
 *
 *      token == 0x1d0:    frame[frame].handler = tl_delete_proj_and_die
 *
 *      otherwise:         return -3
 *
 * **It calls `multi_adjust_xy` with whatever 0x1c and 0x20 already hold.** Every
 * other caller in this file writes the two offsets immediately before; this one
 * uses what the caller left, and then overwrites both four instructions later
 * for `set_proj_vel`. So the launch position is the caller's business and the
 * launch velocity is this routine's.
 *
 * **Animation 0x15 twice, resolved two different ways.** `get_char_ani2` at the
 * start and `find_ani2_part2` on impact -- same number, different finder -- so
 * the sai's flight frames and its impact frames come out of one animation and
 * the two finders pick different parts of it.
 *
 * `obj->field20 = 3` and `obj->a10 = 3` from one register right after
 * `set_proj_vel` has consumed the 4 that was in 0x20. Three values through one
 * field in five instructions: the anirate for the launch, then the flight's own
 * pair.
 */

long t_sai3(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);
    MK3THREADFUNC next_handler;
    uint32_t next;

    if (token == 0x1d0)
        return mk3_install(thread, (MK3THREADFUNC)tl_delete_proj_and_die);

    if (token == 0) {
        multi_adjust_xy(obj);                /* the caller's offsets */

        obj->field40 = 0x15;
        get_char_ani2(obj);

        obj->field1c = 0xb0000;
        obj->field20 = 4;
        set_proj_vel(obj);

        obj->field20 = 3;                    /* the same register */
        obj->a10     = 3;
        obj->field48 = 0x17;

        next         = 0x1ca;
        next_handler = (MK3THREADFUNC)tl_projectile_flight;

    } else if (token == 0x1ca) {
        stop_a8(obj->field08);

        obj->field40 = 0x15;                 /* the same animation, other finder */
        find_ani2_part2(obj);

        obj->field1c = 3;

        next         = 0x1d0;
        next_handler = (MK3THREADFUNC)t_mframew;

    } else {
        return -3;
    }

    *mk3_frame(thread, frame + 1) = next;
    thread->frame = thread->frame + 1;       /* push a level */
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)next_handler;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* tl_do_ermac_zap -- armv7 0x0007b0b0, 224 bytes.  **Complete.**
 *
 *      token == 0:        obj->field20 = 0x27
 *                         obj->a10     = 0
 *                         zap_init_special_act(obj)
 *                         obj->field40 = 0x11; get_char_ani2(obj)
 *                         obj->field1c = 0x00040003
 *                         q_is_he_react_fk(obj)
 *                         if (obj->field5c != 0) obj->field1c = 0x00020003
 *                         token := 0xf1, descend into t_animate_a0_frames
 *
 *      token == 0xf1:     obj->field1c = 9; ochar_sound(obj)
 *                         obj->field1c = -0x28
 *                         obj->field20 = -0x28 + 0x18 = -0x10
 *                         obj->field30 = proc->slave
 *                         adjust_xy_a5(obj)
 *                         obj->field38 = t_ermac_zap_proc
 *                         create_proj_proc(obj)
 *                         obj->field20 = 0x23
 *                         frame[frame].handler = tl_do_proj_sitting_duck
 *
 *      otherwise:         return -3
 *
 * **The windup is shorter against an opponent who is already reacting.** Both
 * animation pairs share the low half -- 3 -- and differ in the count: **4 frames
 * normally, 2 if `q_is_he_react_fk` says yes.** So Ermac throws faster at
 * someone who cannot answer, which is the same idea `t_rocket1_proc` applies to
 * a rocket's speed with the other react predicate.
 *
 * Two predicates, two moves, one design: **ask what the opponent is doing and
 * change a number, never the structure.** A port that treats the react flags as
 * cosmetic gets every one of these subtly wrong.
 *
 * `q_is_he_react_fk` tests the other fighter's action against 0x507, so 0x507 is
 * the action number this whole mechanism turns on.
 *
 * The placement is the `adjust_xy_a5` three-argument form off `proc->slave`
 * again -- third site, after `tl_do_sz_zap` and `tl_do_osz_zap` -- and the
 * offsets are -0x28 and -0x10 from one register with an `adds #0x18`.
 *
 * Sixth sitting-duck duration: 0x23, the same as Sonya's.
 */
void q_is_he_react_fk(MK3OBJ *obj);
long t_ermac_zap_proc(MK3THREAD *thread);
long t_animate_a0_frames(MK3THREAD *thread);     /* pointer slot 0x000f36b8 */

long tl_do_ermac_zap(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0) {
        obj->field20 = 0x27;
        obj->a10     = 0;
        zap_init_special_act(obj);

        obj->field40 = 0x11;
        get_char_ani2(obj);

        obj->field1c = 0x00040003;
        q_is_he_react_fk(obj);
        if (obj->field5c != 0)
            obj->field1c = 0x00020003;       /* reacting: half the windup */

        *mk3_frame(thread, frame + 1) = 0xf1;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_animate_a0_frames;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0xf1)
        return -3;

    obj->field1c = 9;
    ochar_sound(obj);

    obj->field1c = (uint32_t)~0x27u;         /* -0x28 */
    obj->field20 = (uint32_t)~0x27u + 0x18;  /* -0x10, the same register */
    obj->field30 = obj->field00->slave;
    adjust_xy_a5(obj);

    obj->field38 = (uint32_t)(uintptr_t)t_ermac_zap_proc;
    create_proj_proc(obj);

    obj->field20 = 0x23;                     /* the sitting-duck duration */

    return mk3_install(thread, (MK3THREADFUNC)tl_do_proj_sitting_duck);
}


/* t_ermac_zap_proc -- armv7 0x0007c100, 228 bytes.  **Complete.**
 *
 *      token == 0:        obj->field40 = 0x17
 *                         obj->field1c = 0x19
 *                         borrow_char_ani(obj)
 *                         obj->field1c = 0xa0000
 *                         q_is_he_react_fk(obj)
 *                         if (obj->field5c != 0) obj->field1c = 0xd0000
 *                         obj->field20 = 3
 *                         set_proj_vel(obj)
 *                         obj->field48 = 0x19
 *                         token := 0xc7, descend into tl_projectile_flight
 *
 *      token == 0xc7:     obj->field1c = 0xd; ochar_sound(obj)
 *                         part->x0e += (obj->field2c & 0x10) ? -0x60 : +0x60
 *                         obj->field1c = 5; create_fx(obj)
 *                         frame[frame].handler = tl_delete_proj_and_die
 *
 *      otherwise:         return -3
 *
 * **The same predicate again, and this time it changes the SPEED**: 0xa0000
 * normally, 0xd0000 against a reacting opponent. So Ermac's zap both winds up
 * faster and flies faster at someone mid-reaction, and the two decisions are
 * made in two different routines from the same question.
 *
 * **The impact effect is offset by the facing, not placed by a helper.** 0x60 is
 * added to the part's x, or subtracted when bit 4 of `obj->field2c` is set --
 * and 0x2c holds whatever `set_proj_vel` last copied out of `part->field28`, so
 * the flip bit is being read from a stale copy rather than from the part. That
 * works because nothing between the two touches it, and it is the kind of thing
 * a port breaks by reordering.
 *
 * `borrow_char_ani` rather than `get_char_ani2` -- the variant that takes the
 * index in 0x1c as well as 0x40. 0x19 goes into both 0x1c here and 0x48 later,
 * from one register held across the whole routine.
 */
void borrow_char_ani(MK3OBJ *obj);

long t_ermac_zap_proc(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0) {
        obj->field40 = 0x17;
        obj->field1c = 0x19;
        borrow_char_ani(obj);

        obj->field1c = 0xa0000;
        q_is_he_react_fk(obj);
        if (obj->field5c != 0)
            obj->field1c = 0xd0000;          /* reacting: faster */

        obj->field20 = 3;
        set_proj_vel(obj);

        obj->field48 = 0x19;                 /* the same register as above */

        *mk3_frame(thread, frame + 1) = 0xc7;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)tl_projectile_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0xc7)
        return -3;

    obj->field1c = 0xd;
    ochar_sound(obj);

    /* 0x2c is set_proj_vel's copy of part->field28, read back here. */
    if ((obj->field2c & 0x10u) != 0)
        MK3_SET_FIELD0E(obj->field08,
                        (uint32_t)MK3_FIELD0E(obj->field08) - 0x60);
    else
        MK3_SET_FIELD0E(obj->field08,
                        (uint32_t)MK3_FIELD0E(obj->field08) + 0x60);

    obj->field1c = 5;
    create_fx(obj);

    return mk3_install(thread, (MK3THREADFUNC)tl_delete_proj_and_die);
}

/* ---------------------------------------------------------------- t_stz1
 *
 * armv7 0x0007a2c0, four hundred eighty-four bytes.  **Complete.**
 *
 * Sub-Zero's zap one, the standing ice wave.  Two entrance poses ride the
 * shared `t_st_zap_jsrp` animator: the free state marks `a10`, and the
 * 0xad6 charge holds the 0xc pose over a 0xadd breath that counts `field48`
 * down (re-parking itself with `fieldfc = 8` each tick) until the count dries
 * and `i_am_a_sitting_duck` wears the 0xae9 fire.  The fire rates an
 * "action + 4" pose in the 0x40 slot (`get_char_ani`, then every 0xaef
 * visit racks the field down again -- `field40 -= 8`, bump) while the 0xaf2
 * walker unwinds one frame record at a time, down to `t_local_reaction_exit`
 * when the stack reaches bottom.  Slot 0xae2 is the alternate firing pose;
 * 0xae4/0xae5 are the count's own one-beat bump before the same sitting-duck
 * park.
 *
 *      slot = frame[frame+1].w0
 *                                            ; 0:     free, then 0xad6
 *                                            ; 0xad6: charge; count -> 0xada
 *                                            ; 0xada: pose 0xc, then 0xadd
 *                                            ; 0xadd: count; dry -> sit
 *                                            ; 0xae2: pose 0xc, then 0xae4
 *                                            ; 0xae4: bump, then 0xae5
 *                                            ; 0xae5: sit, then 0xae9
 *                                            ; 0xae9: fire, then 0xaef
 *                                            ; 0xaef: wind up, then 0xaf2
 *                                            ; 0xaf2: unwind, exit at bottom
 *      if (slot == 0xae2) {
 *          obj->a10 = 0xc
 *          token 0xae4 ; frame++ ; install t_st_zap_jsrp ; return 0
 *      }
 *      if (slot < 0xae2) {
 *          if (slot == 0xad6)  count (re-park 0xada, or sit)
 *          if (slot < 0xad6) {
 *              if (slot != 0) return -3
 *              obj->a10 = 0 ; zap_init_special_act(obj) ; obj->a10 = 0
 *              token 0xad6 ; frame++ ; install t_st_zap_jsrp ; return 0
 *          }
 *          if (slot == 0xada) {
 *              obj->a10 = 0xc
 *              token 0xadd ; frame++ ; install t_st_zap_jsrp ; return 0
 *          }
 *          if (slot == 0xadd)  count (re-park 0xae2, or sit)
 *          return -3
 *      }
 *      if (slot == 0xae9) {
 *          obj->field40 = 0x24 ; get_char_ani(obj) ; obj->field40 += 4
 *          do_next_a9_frame(obj)
 *          token 0xaef ; fieldfc = 4 ; return 4
 *      }
 *      if (slot > 0xae9) {
 *          if (slot == 0xaef) {
 *              obj->field40 -= 8 ; do_next_a9_frame(obj)
 *              token 0xaf2 ; fieldfc = 4 ; return 4
 *          }
 *          if (slot == 0xaf2) {
 *              if (frame > 0) { frame -= 1 ; return 0 }
 *              frame[frame].w1 = t_local_reaction_exit
 *              frame[frame+1].w0 = 0 ; return 0
 *          }
 *          return -3
 *      }
 *      if (slot == 0xae4)  token 0xae5 ; fieldfc = 8 ; return 8
 *      if (slot == 0xae5)  sit
 *      return -3
 *
 *      count:  obj->field48 -= 1
 *              if (obj->field48 == 0)  sit
 *              token (0xada or 0xadd) ; fieldfc = 8 ; return 8
 *      sit:    i_am_a_sitting_duck(obj)
 *              token 0xae9 ; fieldfc = 8 ; return 8
 */
long t_stz1(struct MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t slot;

    slot = *mk3_frame(thread, thread->frame + 1);
    if (slot == 0xae2) {
        obj->a10 = 0xc;
        *mk3_frame(thread, thread->frame + 1) = 0xae4;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_st_zap_jsrp;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }
    if (slot < 0xae2) {
        if (slot == 0xad6) {
            obj->field48 -= 1;
            if (obj->field48 != 0) {
                *mk3_frame(thread, thread->frame + 1) = 0xada;
                thread->fieldfc = 8;
                return 8;
            }
            goto sit;
        }
        if (slot < 0xad6) {
            if (slot == 0) {
                obj->a10 = 0;
                zap_init_special_act(obj);
                obj->a10 = 0;
                *mk3_frame(thread, thread->frame + 1) = 0xad6;
                thread->frame = thread->frame + 1;
                mk3_frame(thread, thread->frame)[1] =
                    (uint32_t)(uintptr_t)t_st_zap_jsrp;
                *mk3_frame(thread, thread->frame + 1) = 0;
                return 0;
            }
            return -3;
        }
        if (slot == 0xada) {
            obj->a10 = 0xc;
            *mk3_frame(thread, thread->frame + 1) = 0xadd;
            thread->frame = thread->frame + 1;
            mk3_frame(thread, thread->frame)[1] =
                (uint32_t)(uintptr_t)t_st_zap_jsrp;
            *mk3_frame(thread, thread->frame + 1) = 0;
            return 0;
        }
        if (slot == 0xadd) {
            obj->field48 -= 1;
            if (obj->field48 != 0) {
                /* armv7 0x7a332: `str.w r2, [r0, r3, lsl #3]` re-parks
                 * whatever r2 still held from the dispatch's own
                 * `movw r2, #0xae2` at 0x7a2cc -- nothing between there
                 * and here writes r2, so the token really is 0xae2, the
                 * alternate firing chain's own entry, not this branch's
                 * own 0xadd looping on itself. */
                *mk3_frame(thread, thread->frame + 1) = 0xae2;
                thread->fieldfc = 8;
                return 8;
            }
            goto sit;
        }
        return -3;
    }
    if (slot == 0xae9) {
        obj->field40 = 0x24;
        get_char_ani(obj);
        obj->field40 += 4;
        do_next_a9_frame(obj);
        *mk3_frame(thread, thread->frame + 1) = 0xaef;
        thread->fieldfc = 4;
        return 4;
    }
    if (slot > 0xae9) {
        if (slot == 0xaef) {
            obj->field40 -= 8;
            do_next_a9_frame(obj);
            *mk3_frame(thread, thread->frame + 1) = 0xaf2;
            thread->fieldfc = 4;
            return 4;
        }
        if (slot == 0xaf2) {
            if (thread->frame > 0) {
                thread->frame = thread->frame - 1;
                return 0;
            }
            mk3_frame(thread, thread->frame)[1] =
                (uint32_t)(uintptr_t)t_local_reaction_exit;
            *mk3_frame(thread, thread->frame + 1) = 0;
            return 0;
        }
        return -3;
    }
    if (slot == 0xae4) {
        *mk3_frame(thread, thread->frame + 1) = 0xae5;
        thread->fieldfc = 8;
        return 8;
    }
    if (slot == 0xae5)
        goto sit;
    return -3;

sit:
    i_am_a_sitting_duck(obj);
    *mk3_frame(thread, thread->frame + 1) = 0xae9;
    thread->fieldfc = 8;
    return 8;
}

/* ---------------------------------------------------------- t_new_spear_proc
 *
 * armv7 0x0007c830, four hundred eighty-four bytes.  **Complete.**
 *
 * Scorpion's spear, the fresh state of the tusk.  The free serves the
 * speared tripod: `field20 += 0x18` and `multi_adjust_xy` place it, 9/3
 * poses it (`find_ani2_part_a14`) and the new animation's first word is
 * stamped onto the GrObj's field2c, then the 0xfff / 0xa0000 throw and a
 * 0x14 first-life set into the 0x22c patrol (`tl_projectile_flight_call`).
 * At 0x22c an unarmed `field18` calls turn: an opponent on the rank
 * `proc->field08` reads (the Plyr row) whose proc answers
 * `t_scorp_waiting_sleep` takes the full rope pull -- Steam Scorpion's own
 * retractor parked into field38, `fastxfer_thread` into the mytc thread the
 * same rank reads, `ReallyKillHisProjectile`, the victim GrObj's 0x18 folded
 * back -- then the 0x256 breath, which matches the opponent's 0x0e halfword
 * every visit.  Still cold, the `t_r_null_speared` hand-off is the
 * no-reaction blank; anything with a live `field18` (or a non-sleeping foe)
 * is the 0x240 ride -- three ticks of `player_normpal`, then 0x242 counts
 * `field48` with the `player_swpal(obj, 2)` re-tint under a zeroed GrObj 0x18
 * until the count dries and the stack closes on `tl_delete_proj_and_die`.
 *
 *      slot = frame[frame+1].w0
 *                                            ; 0:     free, then 0x22c
 *                                            ; 0x22c: rope / null-spear / hit
 *                                            ; 0x240: normpal, then 0x242
 *                                            ; 0x242: count field48 -> die
 *                                            ; 0x256: match the 0x0e halfword
 *      if (slot == 0x240) {
 *          player_normpal(obj)
 *          token 0x242 ; fieldfc = 3 ; return 3
 *      }
 *      if (slot < 0x240) {
 *          if (slot == 0x22c) {
 *              n = proc->field08
 *              if (obj->field18 == 0) {
 *                  if (GetProcFunc(Plyr + n * PLYR_STRIDE) == t_scorp_waiting_sleep) {
 *                      obj->field38 = t_scorp_rope_pull
 *                      fastxfer_thread(obj, mytc + n * 268)
 *                      ReallyKillHisProjectile(obj)
 *                      obj->field08->field18 = obj->field18
 *                      token 0x256 ; fieldfc = 1 ; return 1
 *                  }
 *                  if (obj->field18 != 0)  hit
 *                  args[fieldf8] = obj->a10 ; fieldf8++
 *                  obj->field38 = t_r_null_speared ; takeover_him(obj)
 *                  fieldf8-- ; obj->a10 = args[fieldf8]
 *              }
 *              hit:  obj->field48 = 3 ; obj->field08->field18 = 0
 *              player_swpal(obj, 2)
 *              token 0x240 ; fieldfc = 3 ; return 3
 *          }
 *          if (slot == 0) {
 *              obj->field20 += 0x18 ; multi_adjust_xy(obj)
 *              obj->field40 = 9 ; obj->field54 = 3 ; find_ani2_part_a14(obj)
 *              obj->field08->field2c = *(uint32_t *)obj->field40
 *              obj->field20 = 0xfff ; obj->field1c = 0xa0000 ; set_proj_vel(obj)
 *              obj->field34 = 0 ; obj->field48 = 0x14
 *              token 0x22c ; frame++ ; install tl_projectile_flight_call ; return 0
 *          }
 *          return -3
 *      }
 *      if (slot == 0x242) {
 *          obj->field48 -= 1
 *          if (obj->field48 != 0) {
 *              obj->field08->field18 = 0 ; player_swpal(obj, 2)
 *              token 0x240 ; fieldfc = 3 ; return 3
 *          }
 *          install tl_delete_proj_and_die ; return 0
 *      }
 *      if (slot != 0x256) return -3
 *      HW(field08 + 0x0e) = HW(him + 0x0e)
 *      token 0x256 ; fieldfc = 1 ; return 1
 */
long t_new_spear_proc(struct MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t *args = (uint32_t *)(void *)thread->args;
    uint32_t slot;
    uint32_t n;

    slot = *mk3_frame(thread, thread->frame + 1);
    if (slot == 0x240) {
        player_normpal(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x242;
        thread->fieldfc = 3;
        return 3;
    }
    if (slot < 0x240) {
        if (slot == 0x22c) {
            if (obj->field18 != 0)
                goto hit;
            n = obj->field00->field08;
            if ((uintptr_t)GetProcFunc(
                    (MK3OBJ *)(void *)((char *)Plyr + (uintptr_t)n * PLYR_STRIDE)) ==
                (uintptr_t)t_scorp_waiting_sleep) {
                obj->field38 = (uint32_t)(uintptr_t)t_scorp_rope_pull;
                fastxfer_thread(obj,
                    (MK3THREAD *)(void *)((char *)mytc + (uintptr_t)n * 268));
                ReallyKillHisProjectile(obj);
                obj->field08->field18 = obj->field18;
                *mk3_frame(thread, thread->frame + 1) = 0x256;
                thread->fieldfc = 1;
                return 1;
            }
            if (obj->field18 != 0)
                goto hit;
            args[thread->fieldf8] = obj->a10;
            thread->fieldf8 = thread->fieldf8 + 1;
            obj->field38 = (uint32_t)(uintptr_t)t_r_null_speared;
            takeover_him(obj);
            thread->fieldf8 = thread->fieldf8 - 1;
            obj->a10 = args[thread->fieldf8];
        }
        else if (slot == 0) {
            obj->field20 += 0x18;
            multi_adjust_xy(obj);
            obj->field40 = 9;
            obj->field54 = 3;
            find_ani2_part_a14(obj);
            obj->field08->field2c = *(uint32_t *)(uintptr_t)obj->field40;
            obj->field20 = 0xfff;
            obj->field1c = 0xa0000;
            set_proj_vel(obj);
            obj->field34 = 0;
            obj->field48 = 0x14;
            *mk3_frame(thread, thread->frame + 1) = 0x22c;
            thread->frame = thread->frame + 1;
            mk3_frame(thread, thread->frame)[1] =
                (uint32_t)(uintptr_t)tl_projectile_flight_call;
            *mk3_frame(thread, thread->frame + 1) = 0;
            return 0;
        }
        else {
            return -3;
        }

hit:
        obj->field48 = 3;
tail:
        obj->field08->field18 = 0;
        player_swpal(obj, 2);
        *mk3_frame(thread, thread->frame + 1) = 0x240;
        thread->fieldfc = 3;
        return 3;
    }
    if (slot == 0x242) {
        obj->field48 -= 1;
        if (obj->field48 != 0)
            goto tail;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)tl_delete_proj_and_die;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }
    if (slot != 0x256)
        return -3;
    *(uint16_t *)((char *)obj->field08 + 0x0e) =
        *(uint16_t *)((char *)(uintptr_t)obj->field00->him + 0x0e);
    *mk3_frame(thread, thread->frame + 1) = 0x256;
    thread->fieldfc = 1;
    return 1;
}

/* ----------------------------------------------------------- t_scorp_rope_pull
 *
 * armv7 0x0007c5fc, five hundred sixty-four bytes.  **Complete.**
 *
 * The retractor: what `t_new_spear_proc` parks into field38 when the tusk
 * bites.  The free slaps both the spear and its victim into the 0x11a /
 * 0x623 actions, pushes `field40` around the same 9/3 pose the free above
 * uses (`find_ani2_part_a14`), stamps the new animation's second word onto
 * the imported GrObj's field2c, poses himself and stops and floors the
 * victim (`pose_him_a0`, `stop_him`, `ground_him`), and winds the
 * `t_double_shaker` ride off a 6-beat `field40` count.  0x390 decrements it
 * (re-parking the shaker with a `~1` spin off `field20` each breath) and on
 * the dry walk sets `field40 = 9`, animates, and parks the 0x399 tug.  At
 * 0x399 the tug gives the opponent `t_tugged_in_by_spear` through
 * `xfer_otherguy` while parking the 0x39e beat: `randu`, an `ochar_sound`,
 * `init_anirate`, and the 0x3a9 watch.  The watch `next_anirate`s while the
 * opponent stands in the speared action -- `him->field18` still nonzero --
 * and the instant it clears, sends him back: the imported proc is killed and
 * `field88` zeroed, the object floated on `G + 0x420` (`update_tsl`), and
 * `t_local_reaction_exit` lets the victim fall back out.
 *
 *      slot = frame[frame+1].w0
 *                                            ; 0:     free, then 0x38e
 *                                            ; 0x38e: shake, then 0x390
 *                                            ; 0x390: count; dry -> 0x399
 *                                            ; 0x399: tug, then 0x39e
 *                                            ; 0x39e: randu, then 0x3a9
 *                                            ; 0x3a9: watch; exit when clear
 *      if (slot == 0x390) {
 *          obj->field40 -= 1
 *          if (obj->field40 != 0)  repark (the ~1 spin, 0x38e)
 *          obj->field40 += 9 ; get_char_ani2(obj) ; find_part2(obj)
 *          do_next_a9_frame(obj)
 *          token 0x399 ; fieldfc = 4 ; return 4
 *      }
 *      if (slot < 0x390) {
 *          if (slot == 0x38e) {
 *              obj->field20 = 2
 *              token 0x390 ; frame++ ; install t_double_shaker ; return 0
 *          }
 *          if (slot == 0) {
 *              free...
 *          }
 *          return -3
 *      }
 *      if (slot == 0x39e) {
 *          obj->field1c = 2 ; randu(obj) ; obj->field1c -= 1
 *          ochar_sound(obj) ; obj->field1c = 3 ; init_anirate(obj)
 *          token 0x3a9 ; fieldfc = 1 ; return 1
 *      }
 *      if (slot == 0x3a9) {
 *          next_anirate(obj)
 *          r6 = him->field18
 *          if (r6 != 0) { token 0x3a9 ; fieldfc = 1 ; return 1 }
 *          KillProc(field00->field88) ; field00->field88 = 0
 *          obj->field1c = G + 0x420 ; update_tsl(obj)
 *          frame[frame].w1 = t_local_reaction_exit ; frame[frame+1].w0 = 0
 *          return 0
 *      }
 *      if (slot != 0x399) return -3
 *      obj->field38 = t_tugged_in_by_spear ; xfer_otherguy(obj)
 *      token 0x39e ; fieldfc = 3 ; return 3
 *
 *      free:   obj->field20 = 0x11a ; proc->field18 = 0x11a
 *              args[fieldf8] = obj->field40 ; fieldf8++
 *              obj->field40 = 9 ; obj->field54 = 3 ; find_ani2_part_a14(obj)
 *              field00->field88->field08->field2c = *(field40 + 8)
 *              fieldf8-- ; obj->field40 = args[fieldf8]
 *              obj->field1c = 0x1e ; pose_him_a0(obj)
 *              args[fieldf8] = obj->a10 ; fieldf8++
 *              stop_him(obj) ; ground_him(obj)
 *              fieldf8-- ; obj->a10 = args[fieldf8]
 *              obj->field20 = 0x623
 *              proc->field00->field00->field18 = 0x623
 *              obj->field40 = 6
 *              repark
 *      repark: obj->field20 = ~1
 *              token 0x38e ; frame++ ; install t_double_shaker ; return 0
 */
long t_scorp_rope_pull(struct MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t *args = (uint32_t *)(void *)thread->args;
    uint32_t slot;

    slot = *mk3_frame(thread, thread->frame + 1);
    if (slot == 0x390) {
        obj->field40 -= 1;
        if (obj->field40 != 0)
            goto repark;
        obj->field40 += 9;
        get_char_ani2(obj);
        find_part2(obj);
        do_next_a9_frame(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x399;
        thread->fieldfc = 4;
        return 4;
    }
    if (slot < 0x390) {
        if (slot == 0x38e) {
            obj->field20 = 2;
            *mk3_frame(thread, thread->frame + 1) = 0x390;
            thread->frame = thread->frame + 1;
            mk3_frame(thread, thread->frame)[1] =
                (uint32_t)(uintptr_t)t_double_shaker;
            *mk3_frame(thread, thread->frame + 1) = 0;
            return 0;
        }
        if (slot == 0) {
            obj->field20 = 0x11a;
            obj->field00->field18 = 0x11a;
            args[thread->fieldf8] = obj->field40;
            thread->fieldf8 = thread->fieldf8 + 1;
            obj->field40 = 9;
            obj->field54 = 3;
            find_ani2_part_a14(obj);
            obj->field00->field88->field08->field2c =
                *(uint32_t *)((char *)(uintptr_t)obj->field40 + 8);
            thread->fieldf8 = thread->fieldf8 - 1;
            obj->field40 = args[thread->fieldf8];
            obj->field1c = 0x1e;
            pose_him_a0(obj);
            args[thread->fieldf8] = obj->a10;
            thread->fieldf8 = thread->fieldf8 + 1;
            stop_him(obj);
            ground_him(obj);
            thread->fieldf8 = thread->fieldf8 - 1;
            obj->a10 = args[thread->fieldf8];
            obj->field20 = 0x623;
            obj->field00->field00->field00->field18 = 0x623;
            obj->field40 = 6;
            goto repark;
        }
        return -3;
    }
    if (slot == 0x39e) {
        obj->field1c = 2;
        randu(obj);
        obj->field1c -= 1;
        ochar_sound(obj);
        obj->field1c = 3;
        init_anirate(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x3a9;
        thread->fieldfc = 1;
        return 1;
    }
    if (slot == 0x3a9) {
        next_anirate(obj);
        if (((MK3OBJ *)(uintptr_t)obj->field00->him)->field18 != 0) {
            *mk3_frame(thread, thread->frame + 1) = 0x3a9;
            thread->fieldfc = 1;
            return 1;
        }
        KillProc(obj->field00->field88);
        obj->field00->field88 = 0;
        obj->field1c = (uint32_t)(uintptr_t)((char *)G + 0x420);
        update_tsl(obj);
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_local_reaction_exit;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }
    if (slot != 0x399)
        return -3;
    obj->field38 = (uint32_t)(uintptr_t)t_tugged_in_by_spear;
    xfer_otherguy(obj);
    *mk3_frame(thread, thread->frame + 1) = 0x39e;
    thread->fieldfc = 3;
    return 3;

repark:
    obj->field20 = (uint32_t)~1u;
    *mk3_frame(thread, thread->frame + 1) = 0x38e;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_double_shaker;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------------- t_tugged_in_by_spear
 *
 * armv7 0x0007c504, 248 bytes.  **Complete.**
 *
 * The victim's own thread while Scorpion/Sub-Zero's spear reels them in.
 * The free launch (`0`) throws (`field1c = 0x80000`), calls `towards_x_vel`
 * and `set_no_block`, arms resume token `0x44f`, sleeps one frame
 * (`fieldfc = 1`) and returns 1.
 *
 * `0x44f` is the pull-in poll: `get_x_dist` refreshes `field28`, and while
 * it is still greater than `0x40` the token just re-arms itself and sleeps
 * again -- no re-throw. Once close enough it tags BOTH objects' action to
 * `0x623` (`field1c` on self, and `field00->field18` on the puller, i.e.
 * `get_his_action`'s field on the other side), calls `stop_me_player`,
 * poses animation `0x25` via `pose_a9_manual`, sets `field1c = 8` and calls
 * `init_anirate`, arms a `0x40`-frame countdown in `field48`, then falls
 * into the `0x45f` tail below.
 *
 * `0x45f` counts that same `field48` down by one each call
 * (`next_anirate`/`is_he_airborn` first, and `is_he_airborn` answers in
 * `field5c` the same way it does everywhere else): a nonzero `field5c`
 * (airborne) jumps straight to the exit without waiting out the rest of
 * the countdown; otherwise `field48` decrements, and while it is still
 * nonzero the token re-arms and sleeps. When either the countdown or the
 * airborne check ends it, `t_local_reaction_exit` is installed and the
 * resume token is cleared -- the ordinary way this style of function hands
 * control back to the local-reaction machinery.
 */
void towards_x_vel(MK3OBJ *obj);
void get_x_dist(MK3OBJ *obj);
long is_he_airborn(MK3OBJ *obj);

long t_tugged_in_by_spear(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);

    if (slot == 0x44f)
        goto poll;
    if (slot == 0x45f)
        goto countdown;
    if (slot != 0)
        return -3;

    obj->field1c = 0x80000;
    towards_x_vel(obj);
    set_no_block(obj);

    *mk3_frame(thread, thread->frame + 1) = 0x44f;
    thread->fieldfc = 1;
    return 1;

poll:
    get_x_dist(obj);
    if (obj->field28 > 0x40) {
        *mk3_frame(thread, thread->frame + 1) = 0x44f;
        thread->fieldfc = 1;
        return 1;
    }

    obj->field1c = 0x623;
    obj->field00->field18 = 0x623;
    stop_me_player(obj);

    obj->field40 = 0x25;
    pose_a9_manual(obj);

    obj->field1c = 8;
    init_anirate(obj);

    obj->field48 = 0x40;

countdown:
    next_anirate(obj);
    is_he_airborn(obj);

    if (obj->field5c == 0) {
        obj->field48 = obj->field48 - 1;
        if (obj->field48 != 0) {
            *mk3_frame(thread, thread->frame + 1) = 0x45f;
            thread->fieldfc = 1;
            return 1;
        }
    }

    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_local_reaction_exit;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------------- t_lao_hat_proc
 *
 * armv7 0x00078a64, five hundred twelve bytes.  **Complete.**
 *
 * Kung Lao's spinning hat (the throw that comes back).  The free poses the
 * four-frame 0x24 animation (field54 = 4), saves `field40` across
 * `do_next_a9_frame` into `a10` so the seed animation survives the step,
 * plants a half-billion fixed-point velocity on the GrObj (`field08->field1c`,
 * then `0xfffe0000 + 0xa0000` = 0x80000 with `field20 = 4` through
 * `set_proj_vel`), clears the action to the idle walk (`field1c = 1`,
 * `proc->field2c = 1`), tags `field48 = 0x11` and `field34 = t_lao_zap_call`,
 * and flies on `tl_projectile_flight_call` from 0xd53.  0xd53 sails while
 * `field18` still stands, or else dies into the 0xd72 latch; the 0xd72 check
 * dallies only for a sleeping readied stinger (`GetProcFunc(proc->field00) ==
 * t_rhat_sleep` -- park the latch, otherwise fling): the fling re-poses five
 * frames at 0x24, spins the multi off a `~0x9f / 0x98` gnudge and the GrObj
 * direction, calls `set_proj_vel` afresh, and re-enters the flight watch.
 * The latch snapahooks the GrObj's 0x12 halfword on a `match_me_with_him` /
 * `flip_multi` pair, gnudges 0x67, and parks the 0xd72 dalliance.  0xd99
 * rides `next_anirate` while `proj_onscreen_test` still shows the hat, then
 * `tl_delete_proj_and_die` offscreen.
 *
 *      slot = frame[frame+1].w0
 *                                            ; 0:     free, then 0xd53
 *                                            ; 0xd53: fling, or the 0xd72 latch
 *                                            ; 0xd72: latch, or fling
 *                                            ; 0xd99: on-screen watch
 *      if (slot == 0xd53) {
 *          if (obj->field18 == 0) {
 *              obj->field1c = 2 ; ochar_sound(obj) ; stop_a8(obj->field08)
 *              latch
 *          }
 *          fling
 *      }
 *      if (slot == 0xd72) {
 *          if (GetProcFunc(proc->field00) == t_rhat_sleep)  latch
 *          obj->field20 = 5 ; obj->field1c = 0xb0000 ; set_proj_vel(obj)
 *          fling
 *      }
 *      if (slot == 0xd99) {
 *          if (proj_onscreen_test(obj)) {
 *              next_anirate(obj)
 *              token 0xd99 ; fieldfc = 1 ; return 1
 *          }
 *          install tl_delete_proj_and_die ; return 0
 *      }
 *      if (slot == 0) {
 *          obj->field40 = 0x24 ; obj->field54 = 4 ; find_ani_part_a14(obj)
 *          obj->a10 = obj->field40 ; do_next_a9_frame(obj)
 *          obj->field40 = obj->a10
 *          obj->field08->field1c = 0xfffe0000
 *          obj->field20 = 4 ; obj->field1c = 0x00080000 ; set_proj_vel(obj)
 *          obj->field1c = 1 ; proc->field2c = 1
 *          obj->field48 = 0x11 ; obj->field34 = t_lao_zap_call
 *          token 0xd53 ; frame++ ; install tl_projectile_flight_call ; return 0
 *      }
 *      return -3
 *
 *      fling:  obj->field1c = 3 ; ochar_sound(obj)
 *              obj->field40 = 0x24 ; obj->field54 = 5 ; find_ani_part_a14(obj)
 *              obj->field1c = 4 ; init_anirate(obj)
 *              w = 0xfffb0000                 ; pool 0x78c58
 *              obj->field20 = w ; obj->field08->field1c = w
 *              flip_multi(obj)
 *              obj->field1c = ~0x9f
 *              obj->field20 = ~0x9f + 0x98    ; 0xfffffff8
 *              multi_adjust_xy(obj)
 *              s = obj->field08->field18
 *              obj->field08->field18 = -s ; obj->field1c = -s
 *              next_anirate(obj)
 *              token 0xd99 ; fieldfc = 1 ; return 1
 *      latch:  rhs = (int16)HW(field08 + 0x12)
 *              obj->field20 = rhs ; args[fieldf8] = rhs ; fieldf8++
 *              match_me_with_him(obj) ; flip_multi(obj) ; fieldf8--
 *              HW(field08 + 0x12) = (uint16)args[fieldf8]
 *              obj->field1c = ~0x67 ; obj->field20 = 0
 *              multi_adjust_xy(obj)
 *              next_anirate(obj)
 *              token 0xd72 ; fieldfc = 1 ; return 1
 */
long t_lao_hat_proc(struct MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t *args = (uint32_t *)(void *)thread->args;
    uint32_t slot;
    uint32_t wing;

    slot = *mk3_frame(thread, thread->frame + 1);
    if (slot == 0xd53) {
        if (obj->field18 == 0) {
            obj->field1c = 2;
            ochar_sound(obj);
            stop_a8(obj->field08);
            goto latch;
        }
        goto fling;
    }
    if (slot == 0xd72) {
        if ((uintptr_t)GetProcFunc(obj->field00->field00) ==
                (uintptr_t)t_rhat_sleep)
            goto latch;
        obj->field20 = 5;
        obj->field1c = 0xb0000;
        set_proj_vel(obj);
        goto fling;
    }
    if (slot == 0xd99) {
        if (proj_onscreen_test(obj) != 0)
            goto watch;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)tl_delete_proj_and_die;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }
    if (slot == 0) {
        obj->field40 = 0x24;
        obj->field54 = 4;
        find_ani_part_a14(obj);
        obj->a10 = obj->field40;
        do_next_a9_frame(obj);
        obj->field40 = obj->a10;
        wing = 0xfffe0000;          /* pool 0x78c4c */
        obj->field08->field1c = wing;
        obj->field20 = 4;
        obj->field1c = (uint32_t)(wing + 0xa0000u);   /* add.w -> 0x00080000 */
        set_proj_vel(obj);
        obj->field1c = 1;
        obj->field00->field2c = 1;
        obj->field48 = 0x11;
        obj->field34 = (uint32_t)(uintptr_t)t_lao_zap_call;
        *mk3_frame(thread, thread->frame + 1) = 0xd53;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)tl_projectile_flight_call;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }
    return -3;

fling:
    obj->field1c = 3;
    ochar_sound(obj);
    obj->field40 = 0x24;
    obj->field54 = 5;
    find_ani_part_a14(obj);
    obj->field1c = 4;
    init_anirate(obj);
    wing = 0xfffb0000;            /* pool 0x78c58 */
    obj->field20 = wing;
    obj->field08->field1c = wing;
    flip_multi(obj);
    obj->field1c = (uint32_t)~0x9f;
    obj->field20 = (uint32_t)(~0x9f + 0x98);       /* 0xfffffff8 */
    multi_adjust_xy(obj);
    wing = obj->field08->field18;
    obj->field08->field18 = (uint32_t)(-(int32_t)wing);
    obj->field1c = (uint32_t)(-(int32_t)wing);

watch:
    next_anirate(obj);
    *mk3_frame(thread, thread->frame + 1) = 0xd99;
    thread->fieldfc = 1;
    return 1;

latch:
    obj->field20 = (uint32_t)(int16_t)
        *(uint16_t *)((char *)obj->field08 + 0x12);
    args[thread->fieldf8] = obj->field20;
    thread->fieldf8 = thread->fieldf8 + 1;
    match_me_with_him(obj);
    flip_multi(obj);
    thread->fieldf8 = thread->fieldf8 - 1;
    *(uint16_t *)((char *)obj->field08 + 0x12) =
        (uint16_t)args[thread->fieldf8];
    obj->field1c = (uint32_t)~0x67;
    obj->field20 = 0;
    multi_adjust_xy(obj);
    next_anirate(obj);
    *mk3_frame(thread, thread->frame + 1) = 0xd72;
    thread->fieldfc = 1;
    return 1;
}
