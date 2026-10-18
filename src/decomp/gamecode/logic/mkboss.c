/*
 * mkboss.c -- gamecode/logic/mkboss.c, decompiled.
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

long t_boss_close_attack(struct MK3THREAD *thread);
long t_boss_close_miss(struct MK3THREAD *thread);
long t_c_mil_tele_sd(struct MK3THREAD *thread);
long t_c_zoom_sd(struct MK3THREAD *thread);
long t_ease5(struct MK3THREAD *thread);
long t_motaro_hit0(struct MK3THREAD *thread);
long t_motaro_hit2(struct MK3THREAD *thread);
long t_sk_knocked_down(struct MK3THREAD *thread);

/* t_boss_finish_him -- armv7 0x000a8768, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = t_boss_close_attack
 *      frame[frame+1].w0 = 0
 */

long t_boss_finish_him(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_boss_close_attack);
}

/* t_sk_stance_pause -- armv7 0x000a885c, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = t_ease5
 *      frame[frame+1].w0 = 0
 */

long t_sk_stance_pause(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_ease5);
}

/* t_boss_post_hit -- armv7 0x000a8928, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = t_boss_close_miss
 *      frame[frame+1].w0 = 0
 */

long t_boss_post_hit(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_boss_close_miss);
}

/* t_c_robo_tele_sd -- armv7 0x000a8a04, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = t_c_mil_tele_sd
 *      frame[frame+1].w0 = 0
 */

long t_c_robo_tele_sd(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_c_mil_tele_sd);
}

/* t_c_mil_tele_sd -- armv7 0x000a8a38, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = t_c_zoom_sd
 *      frame[frame+1].w0 = 0
 */

long t_c_mil_tele_sd(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_c_zoom_sd);
}

/* t_motaro_hit3 -- armv7 0x000a8b04, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = t_motaro_hit2
 *      frame[frame+1].w0 = 0
 */

long t_motaro_hit3(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_motaro_hit2);
}

/* t_motaro_hit1 -- armv7 0x000a8b38, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = t_motaro_hit0
 *      frame[frame+1].w0 = 0
 */

long t_motaro_hit1(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_motaro_hit0);
}

/* t_sk_slided -- armv7 0x000a8c68, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = t_sk_knocked_down
 *      frame[frame+1].w0 = 0
 */

long t_sk_slided(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_sk_knocked_down);
}


/* The callees these reach, declared from what the call sites
 * pass. One written later with a different signature will
 * conflict here, which is what the check is for. */
long randper(MK3OBJ *obj);

/* bossrandper_org -- armv7 0x000ab5f0, 12 bytes.  **Complete.**
 *
 * A tail call to `randper` with the arguments untouched, so whatever the
 * caller put in r1 goes with them. */
long bossrandper_org(MK3OBJ *obj)
{
    return randper(obj);
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

long t_d_block(struct MK3THREAD *thread);
long t_motaro_grab_punch_now(struct MK3THREAD *thread);
long t_motaro_slided(struct MK3THREAD *thread);
long t_motaro_stupid_stance(struct MK3THREAD *thread);
long t_sk_kick(struct MK3THREAD *thread);
long bossrandper(MK3OBJ *obj);
void group_sound(MK3OBJ *obj);
long is_he_airborn(MK3OBJ *obj);
long motaro_randper(MK3OBJ *obj);
long q_is_this_a_joke(MK3OBJ *obj);
void randu_minimum(MK3OBJ *obj);
void shake_a11(MK3OBJ *obj);

/* t_motaro_grab_punch -- armv7 0x000a8eac, 104 bytes.  **Complete.**
 *
 * **Corrected.** The comment above this had claimed a plain
 * `mk3_push_handler` shape, but the binary does something else: a
 * token check (refusing anything but 0 with the usual -3),
 * then `is_he_airborn`, and only THEN a choice -- airborne installs
 * `t_motaro_punch` on the current level, grounded installs
 * `t_motaro_grab_punch_now` instead. No push either way.
 */
long t_motaro_punch(struct MK3THREAD *thread);

long t_motaro_grab_punch(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);

    if (slot != 0)
        return -3;

    is_he_airborn(obj);
    if (obj->field5c != 0)
        return mk3_install(thread, (MK3THREADFUNC)t_motaro_punch);

    return mk3_install(thread, (MK3THREADFUNC)t_motaro_grab_punch_now);
}

/* t_sk_knocked_down -- armv7 0x000a8fd4, 68 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x6
 *      group_sound(obj)
 *      frame[frame].handler = t_motaro_slided
 *      frame[frame+1].w0 = 0
 */

long t_sk_knocked_down(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x6;
    group_sound(obj);

    return mk3_push_handler(thread, (MK3THREADFUNC)t_motaro_slided);
}

/* t_sk_hit3 -- armv7 0x000a932c, 92 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field48 = 0x60006
 *      shake_a11(obj)
 *      rsnd_func(obj, 0xa)
 *      obj->field1c = 0x6
 *      group_sound(obj)
 *      frame[frame].handler = t_sk_slided
 *      frame[frame+1].w0 = 0
 */

long t_sk_hit3(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field48 = 0x60006;
    shake_a11(obj);
    rsnd_func(obj, 0xa);
    obj->field1c = 0x6;
    group_sound(obj);

    return mk3_push_handler(thread, (MK3THREADFUNC)t_sk_slided);
}

/* t_sk_stupid_stance -- armv7 0x000a9fc0, 108 bytes.  **Complete.**
 *
 * **Corrected.** The comment above this had claimed a plain
 * `mk3_push_handler` into `t_motaro_stupid_stance`, but the binary
 * branches on `q_is_this_a_joke`'s own answer: a hit installs `t_ss1`
 * on the current level, a miss installs `t_motaro_stupid_stance`
 * instead. Anything but token 0 is refused with -3, and there is no
 * push either way.
 */
long t_ss1(struct MK3THREAD *thread);

long t_sk_stupid_stance(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);

    if (slot != 0)
        return -3;

    obj->a10 = 0xc0;
    q_is_this_a_joke(obj);
    if (obj->field5c != 0)
        return mk3_install(thread, (MK3THREADFUNC)t_ss1);

    return mk3_install(thread, (MK3THREADFUNC)t_motaro_stupid_stance);
}

/* t_mc_sg_pounce -- armv7 0x000ab944, 68 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      motaro_randper(obj)
 *      frame[frame].handler = t_d_block
 *      frame[frame+1].w0 = 0
 */

long t_mc_sg_pounce(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    motaro_randper(obj);

    return mk3_push_handler(thread, (MK3THREADFUNC)t_d_block);
}

/* t_skc_sg_pounce_sd -- armv7 0x000abf24, 88 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x2bc
 *      bossrandper(obj)
 *      frame[frame].handler = hit ? t_sk_kick : t_return_to_beware
 *      frame[frame+1].w0 = 0
 *
 * **Corrected.** This was first written as an unconditional install of
 * `t_sk_kick`; the binary branches on the roll, and a miss installs
 * `t_return_to_beware` (pointer slot 0x000f3428).
 */

long t_return_to_beware(struct MK3THREAD *thread);   /* pointer slot 0x000f3428 */

long t_skc_sg_pounce_sd(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x2bc;
    bossrandper(obj);
    if (obj->field5c != 0)
        return mk3_install(thread, (MK3THREADFUNC)t_sk_kick);

    return mk3_install(thread, (MK3THREADFUNC)t_return_to_beware);
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

/* motaro_easy_randper -- armv7 0x000ab6e8, 16 bytes.  **Complete.**
 *
 *      obj->field1c = 0x15e
 *      bossrandper(obj)
 */
void motaro_easy_randper(MK3OBJ *obj)
{
    obj->field1c = 0x15e;
    bossrandper(obj);
}


/* motaro_joke_randper -- armv7 0x000ab7f4, 16 bytes.  **Complete.**
 *
 *      obj->field1c = 0x64
 *      bossrandper(obj)
 */
void motaro_joke_randper(MK3OBJ *obj)
{
    obj->field1c = 0x64;
    bossrandper(obj);
}


/* sk_counter_joke -- armv7 0x000abce0, 16 bytes.  **Complete.**
 *
 *      obj->field1c = 0x4b
 *      bossrandper(obj)
 */
void sk_counter_joke(MK3OBJ *obj)
{
    obj->field1c = 0x4b;
    bossrandper(obj);
}


/* q_boss_stupid -- armv7 0x000abdf0, 16 bytes.  **Complete.**
 *
 *      obj->field1c = 0x4b
 *      bossrandper(obj)
 */
void q_boss_stupid(MK3OBJ *obj)
{
    obj->field1c = 0x4b;
    bossrandper(obj);
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

long t_boss1(MK3THREAD *thread);
long t_sk_airborn_check(MK3THREAD *thread);
long t_stumble_back_vel(MK3THREAD *thread);
long t_swait_land_jsrp(MK3THREAD *thread);

/* t_boss_wait_land -- armv7 0x000a86fc, 108 bytes.  **Complete.**
 *
 *      token == 0:
 *          token := 0x1c5, then descend into t_swait_land_jsrp
 *      token == 0x1c5:
 *          frame[frame].handler = t_boss1
 *      otherwise:  return -3
 */
long t_boss_wait_land(MK3THREAD *thread)
{
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x1c5;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_swait_land_jsrp;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x1c5)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_boss1);
}

/* t_sk_stumble -- armv7 0x000a8bf0, 120 bytes.  **Complete.**
 *
 *      token == 0:
 *          token := 0x85b, then descend into t_sk_airborn_check
 *      token == 0x85b:
 *          obj->field1c = 0x40000
 *          frame[frame].handler = t_stumble_back_vel
 *      otherwise:  return -3
 */
long t_sk_stumble(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x85b;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_sk_airborn_check;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x85b)
        return -3;

    obj->field1c = 0x40000;
    return mk3_install(thread, (MK3THREADFUNC)t_stumble_back_vel);
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

long t_animate_a0_frames(MK3THREAD *thread);
long t_animate_a9(MK3THREAD *thread);
long t_flight(MK3THREAD *thread);
long t_land_on_my_back(MK3THREAD *thread);
long t_local_reaction_exit(MK3THREAD *thread);
long t_reaction_land(MK3THREAD *thread);
long t_wait_forever(MK3THREAD *thread);
long create_blood_proc(MK3OBJ *obj);
void get_char_ani(MK3OBJ *obj);
void init_special(MK3OBJ *obj);
void rsnd_ochar_sound(MK3OBJ *obj);

/* t_motaro_hit_flight -- armv7 0x000a8a6c, 152 bytes.  **Complete.**
 *
 *      token == 0:
 *          obj->field1c = 0x40000
 *          obj->field20 = 0xfffc0000
 *          obj->field24 = 0x6000
 *          obj->field28 = 0x5
 *          obj->field40 = 0x1e
 *          token := 0x79d, then descend into t_flight
 *      token == 0x79d:
 *          frame[frame].handler = t_land_on_my_back
 *      otherwise:  return -3
 */
long t_motaro_hit_flight(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field1c = 0x40000;
        obj->field20 = 0xfffc0000;
        obj->field24 = 0x6000;
        obj->field28 = 0x5;
        obj->field40 = 0x1e;
        *mk3_frame(thread, thread->frame + 1) = 0x79d;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x79d)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_land_on_my_back);
}

/* t_motaro_collapse -- armv7 0x000a8b6c, 132 bytes.  **Complete.**
 *
 *      token == 0:
 *          obj->field40 = 0x3001e
 *          token := 0x825, then descend into t_animate_a9
 *      token == 0x825:
 *          frame[frame].handler = t_wait_forever
 *      otherwise:  return -3
 */
long t_motaro_collapse(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field40 = 0x3001e;
        *mk3_frame(thread, thread->frame + 1) = 0x825;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_animate_a9;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x825)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_wait_forever);
}

/* t_motaro_slided -- armv7 0x000a8c9c, 152 bytes.  **Complete.**
 *
 *      token == 0:
 *          obj->field1c = 0x40000
 *          obj->field20 = 0xfffd0000
 *          obj->field24 = 0x5000
 *          obj->field28 = 0x5
 *          obj->field40 = 0x1e
 *          token := 0x883, then descend into t_flight
 *      token == 0x883:
 *          frame[frame].handler = t_land_on_my_back
 *      otherwise:  return -3
 */
long t_motaro_slided(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field1c = 0x40000;
        obj->field20 = 0xfffd0000;
        obj->field24 = 0x5000;
        obj->field28 = 0x5;
        obj->field40 = 0x1e;
        *mk3_frame(thread, thread->frame + 1) = 0x883;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x883)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_land_on_my_back);
}

/* t_sk_hard_comboed -- armv7 0x000a9464, 160 bytes.  **Complete.**
 *
 *      token == 0:
 *          obj->field48 = 0x60006
 *          shake_a11(obj)
 *          rsnd_func(obj, 0xa)
 *          obj->field1c = 0x6
 *          group_sound(obj)
 *          token := 0x854, then descend into t_sk_airborn_check
 *      token == 0x854:
 *          obj->field1c = 0x50000
 *          frame[frame].handler = t_stumble_back_vel
 *      otherwise:  return -3
 */
long t_sk_hard_comboed(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field48 = 0x60006;
        shake_a11(obj);
        rsnd_func(obj, 0xa);
        obj->field1c = 0x6;
        group_sound(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x854;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_sk_airborn_check;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x854)
        return -3;

    obj->field1c = 0x50000;
    return mk3_install(thread, (MK3THREADFUNC)t_stumble_back_vel);
}

/* t_sk_uppcutted -- armv7 0x000a95d0, 188 bytes.  **Complete.**
 *
 *      token == 0:
 *          obj->field1c = 0x1
 *          create_blood_proc(obj)
 *          obj->field48 = 0x60006
 *          shake_a11(obj)
 *          rsnd_func(obj, 0xa)
 *          obj->field1c = 0x2
 *          group_sound(obj)
 *          obj->field1c = 0x20000
 *          obj->field20 = 0xfff40000
 *          obj->field24 = 0x6000
 *          obj->field28 = 0x5
 *          obj->field40 = 0x1e
 *          token := 0x848, then descend into t_flight
 *      token == 0x848:
 *          frame[frame].handler = t_reaction_land
 *      otherwise:  return -3
 */
long t_sk_uppcutted(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field1c = 0x1;
        create_blood_proc(obj);
        obj->field48 = 0x60006;
        shake_a11(obj);
        rsnd_func(obj, 0xa);
        obj->field1c = 0x2;
        group_sound(obj);
        obj->field1c = 0x20000;
        obj->field20 = 0xfff40000;
        obj->field24 = 0x6000;
        obj->field28 = 0x5;
        obj->field40 = 0x1e;
        *mk3_frame(thread, thread->frame + 1) = 0x848;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x848)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_reaction_land);
}

/* t_sk_laugh -- armv7 0x000aaf68, 160 bytes.  **Complete.**
 *
 *      token == 0:
 *          init_special(obj)
 *          obj->field1c = 0x40003
 *          rsnd_ochar_sound(obj)
 *          obj->field40 = 0xb
 *          get_char_ani(obj)
 *          obj->field1c = 0x5000c
 *          token := 0x2b6, then descend into t_animate_a0_frames
 *      token == 0x2b6:
 *          frame[frame].handler = t_local_reaction_exit
 *      otherwise:  return -3
 */
long t_sk_laugh(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        init_special(obj);
        obj->field1c = 0x40003;
        rsnd_ochar_sound(obj);
        obj->field40 = 0xb;
        get_char_ani(obj);
        obj->field1c = 0x5000c;
        *mk3_frame(thread, thread->frame + 1) = 0x2b6;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_animate_a0_frames;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x2b6)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* --------------------------------------------------------------------- q_yes / q_no
 *
 * armv7 0x000a85cc / 0x000a85d4, 8 bytes each.  **Complete.**
 *
 * mkboss.c's own private copies of the answer helpers `moves.c` also
 * defines -- the same two instructions each (`field5c = 1` / `= 0`), at
 * this file's own addresses. Their symbols are local in the binary
 * (no N_EXT), so they are `static` here: the fight runtime links every
 * logic file into one program, and three files define these two names.
 * The global pair -- the one mkanimal.c, mkzap.c and mkprop.c call -- is
 * mkdrone.c's, at 0x00067524 / 0x0006752c.
 */
static void q_yes(MK3OBJ *obj)
{
    obj->field5c = 1;
}

static void q_no(MK3OBJ *obj)
{
    obj->field5c = 0;
}


/* --------------------------------------------------------------------- get_mhe_long / get_mhe_word
 *
 * armv7 0x000a8d34 / 0x000a8d4c, 24 bytes each.  **Complete.**
 *
 * Index a per-ladder-order table: `field1c` holds the table,
 * `ladderorder_a1` answers the order (1..3) in `field20`, and the entry
 * replaces the table pointer in `field1c` -- a word, or a sign-extended
 * halfword. The "mhe" tables (`mhe_sk_counter_randpers` and friends)
 * are the boss AI's difficulty-by-ladder numbers; entry 0 is never read,
 * since the order is never 0.
 */
void ladderorder_a1(MK3OBJ *obj);

void get_mhe_long(MK3OBJ *obj)
{
    ladderorder_a1(obj);
    obj->field1c = ((const uint32_t *)(uintptr_t)obj->field1c)[obj->field20];
}

void get_mhe_word(MK3OBJ *obj)
{
    ladderorder_a1(obj);
    obj->field1c = (uint32_t)(int32_t)
        ((const int16_t *)(uintptr_t)obj->field1c)[obj->field20];
}


/* --------------------------------------------------------------------- bossrandper
 *
 * armv7 0x000ab6bc, 44 bytes.  **Complete.**
 *
 * `randper` with the odds scaled to a fifth first: `field1c` goes
 * through a double multiply by `0.2` (a VFP constant in the literal
 * pool, truncated back toward zero by `vcvt.s32.f64`) and the answer is
 * `randper`'s own, both in `r0` and in `field5c`. The only floating point
 * in the boss AI.
 */
long bossrandper(MK3OBJ *obj)
{
    obj->field1c = (uint32_t)(int32_t)((double)(int32_t)obj->field1c * 0.2);
    return randper(obj);
}


/* --------------------------------------------------------------------- sk_counter_randper
 *
 * armv7 0x000abcc4, 28 bytes.  **Complete.**
 *
 * Shao Kahn's counter-attack odds: the ladder-order entry of
 * `mhe_sk_counter_randpers`, rolled through `bossrandper`.
 */
extern int16_t mhe_sk_counter_randpers[];    /* 0x0017b3d2 */

long sk_counter_randper(MK3OBJ *obj)
{
    obj->field1c = (uint32_t)(uintptr_t)mhe_sk_counter_randpers;
    get_mhe_word(obj);
    return bossrandper(obj);
}


/* --------------------------------------------------------------------- q_heading_down
 *
 * armv7 0x000a89e8, 28 bytes.  **Complete.**
 *
 * `obj->field00->him`'s own `field1c` (a signed velocity), read into
 * `field1c` and tested: negative (rising) answers NO -- `blt` goes to
 * 0xa85d4, which is q_no -- and anything else (falling or still) answers
 * yes, as the name says. (The reading was the other way round until
 * tools/difftest compared it.)
 */
void q_heading_down(MK3OBJ *obj)
{
    obj->field1c = ((MK3OBJ *)(uintptr_t)obj->field00->him)->field1c;
    if ((int32_t)obj->field1c < 0)
        q_no(obj);
    else
        q_yes(obj);
}


/* --------------------------------------------------------------------- t_b_return_to_beware_4get
 *
 * armv7 0x000a858c, 64 bytes.  **Complete.**
 *
 * State 0 only. `field1c` and `field00->field5c` both get the same
 * copy of the incoming token, then the current level installs
 * `t_return_to_beware` and clears its own token -- the same "forget
 * this state and hand off" shape as a plain `mk3_install`, transcribed
 * with the extra `field00->field5c` write it actually does.
 */
long t_return_to_beware(struct MK3THREAD *thread);   /* pointer slot 0x000f3428 */

long t_b_return_to_beware_4get(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);

    if (slot != 0)
        return -3;

    obj->field1c          = slot;
    obj->field00->field5c = slot;

    return mk3_install(thread, (MK3THREADFUNC)t_return_to_beware);
}


/* --------------------------------------------------------------------- motaro_randper
 *
 * armv7 0x000ab804, 36 bytes.  **Complete.**
 *
 * A joke round routes to `motaro_joke_randper`; otherwise `field1c =
 * 500` through `bossrandper`.
 */
long q_is_this_a_joke(MK3OBJ *obj);
void motaro_joke_randper(MK3OBJ *obj);

long motaro_randper(MK3OBJ *obj)
{
    q_is_this_a_joke(obj);
    if (obj->field5c != 0) {
        motaro_joke_randper(obj);
        return (long)obj->field5c;
    }

    obj->field1c = 0x1f4;
    return bossrandper(obj);
}


/* --------------------------------------------------------------------- q_is_he_dizzy_boss
 *
 * armv7 0x000aa02c, 36 bytes.  **Complete.**
 *
 * `get_his_action`'s own answer against `0x620`, the dizzy action.
 */
void get_his_action(MK3OBJ *obj);

void q_is_he_dizzy_boss(MK3OBJ *obj)
{
    get_his_action(obj);
    if (obj->field20 == 0x620)
        q_yes(obj);
    else
        q_no(obj);
}


/* --------------------------------------------------------------------- q_ok_motaro_sweep
 *
 * armv7 0x000a8e88, 36 bytes.  **Complete.**
 *
 * Yes when the opponent is out of Motaro's sweep range (further than
 * `0xd0`, or closer than `0x80` -- too close to sweep) and not airborne
 * in between.
 */
void get_x_dist(MK3OBJ *obj);
long is_he_airborn(MK3OBJ *obj);

void q_ok_motaro_sweep(MK3OBJ *obj)
{
    get_x_dist(obj);
    if ((int32_t)obj->field28 > 0xd0)
        goto no;
    if ((int32_t)obj->field28 <= 0x7f)
        goto no;

    is_he_airborn(obj);

no:
    q_no(obj);
}


/* --------------------------------------------------------------------- sk_randper
 *
 * armv7 0x000abcf0, 48 bytes.  **Complete.**
 *
 * A joke round routes to `sk_counter_joke`; otherwise the ladder-order
 * entry of `mhe_sk_randpers` through `bossrandper`.
 */
void sk_counter_joke(MK3OBJ *obj);
extern int16_t mhe_sk_randpers[];            /* 0x0017b3ca */

long sk_randper(MK3OBJ *obj)
{
    q_is_this_a_joke(obj);
    if (obj->field5c != 0) {
        sk_counter_joke(obj);
        return (long)obj->field5c;
    }

    obj->field1c = (uint32_t)(uintptr_t)mhe_sk_randpers;
    get_mhe_word(obj);
    return bossrandper(obj);
}


/* --------------------------------------------------------------------- q_is_he_car
 *
 * armv7 0x000a8d64, 40 bytes.  **Complete.**
 *
 * Yes only when cornered AND reacting -- `q_is_he_cornered`'s no
 * short-circuits straight to `q_no`, matching Kabal's own "CAR" combo
 * window: pinned in the corner while he's still recovering.
 */
void q_is_he_cornered(MK3OBJ *obj);
void q_is_he_reacting(MK3OBJ *obj);

void q_is_he_car(MK3OBJ *obj)
{
    q_is_he_cornered(obj);
    if (obj->field5c != 0)
        q_is_he_reacting(obj);

    if (obj->field5c == 0)
        q_no(obj);
    else
        q_yes(obj);
}


/* --------------------------------------------------------------------- t_boss_close_attack, t_motaro_far_easy/hard/med, t_sk_stupid
 *
 * armv7 0x000a879c/0xa869c/0xa863c/0xa85dc/0xa87fc, 96 bytes each.
 * **Complete.**
 *
 * Five copies of one shape: state 0 only, push `t_random_do` with a small
 * count and a table pointer in the OBJECT's own `field64`/`slave` pair
 * (`[thread->proc, #0x64/#0x68]`). **Corrected**: these were first written
 * through `field00`, the header's identically-offset pair; the binary never
 * dereferences field00 here, and every other `t_random_do` caller in this
 * file uses the object's pair too. Each local move-choice
 * table (`funcs.NNNN` in the compiler's own naming, an anonymous local
 * array rather than a named function) is picked by which entry this
 * function is.
 */
long t_random_do(struct MK3THREAD *thread);   /* not yet decompiled, mkdrone.c */
extern MK3THREADFUNC funcs_boss_close_attack[];   /* 0x0017b9b0 */
extern MK3THREADFUNC funcs_motaro_far_easy[];     /* 0x0017b9bc */
extern MK3THREADFUNC funcs_motaro_far_hard[];     /* 0x0017b9c4 */
extern MK3THREADFUNC funcs_motaro_far_med[];      /* 0x0017b9cc */
extern MK3THREADFUNC funcs_sk_stupid[];           /* 0x0017b92c */

long t_boss_close_attack(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);

    if (slot != 0)
        return -3;

    obj->slave   = (uint32_t)(uintptr_t)funcs_boss_close_attack;
    obj->field64 = 3;

    *mk3_frame(thread, thread->frame + 1) = 0x1d3;
    thread->frame = thread->frame + 1;   /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_random_do;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}

long t_motaro_far_easy(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);

    if (slot != 0)
        return -3;

    obj->slave   = (uint32_t)(uintptr_t)funcs_motaro_far_easy;
    obj->field64 = 2;

    *mk3_frame(thread, thread->frame + 1) = 0x180;
    thread->frame = thread->frame + 1;   /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_random_do;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}

long t_motaro_far_hard(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);

    if (slot != 0)
        return -3;

    obj->slave   = (uint32_t)(uintptr_t)funcs_motaro_far_hard;
    obj->field64 = 2;

    *mk3_frame(thread, thread->frame + 1) = 0x16e;
    thread->frame = thread->frame + 1;   /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_random_do;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}

long t_motaro_far_med(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);

    if (slot != 0)
        return -3;

    obj->slave   = (uint32_t)(uintptr_t)funcs_motaro_far_med;
    obj->field64 = 2;

    *mk3_frame(thread, thread->frame + 1) = 0x15b;
    thread->frame = thread->frame + 1;   /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_random_do;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}

long t_sk_stupid(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);

    if (slot != 0)
        return -3;

    obj->slave   = (uint32_t)(uintptr_t)funcs_sk_stupid;
    obj->field64 = 5;

    *mk3_frame(thread, thread->frame + 1) = 0x3e3;
    thread->frame = thread->frame + 1;   /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_random_do;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------------------- t_motaro_stupid_stance
 *
 * armv7 0x000a9898, 76 bytes.  **Complete.**
 *
 * State 0 only. Rolls a random 0x30..0x5f duration (`randu_minimum`,
 * both bounds `0x30`, answering in `field1c`), parks it in `a10`, and
 * installs `t_ss1` on the current level -- no push, so this replaces
 * its own dispatcher rather than descending into it.
 */
void randu_minimum(MK3OBJ *obj);
long t_ss1(struct MK3THREAD *thread);

long t_motaro_stupid_stance(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);

    if (slot != 0)
        return -3;

    obj->field1c = 0x30;
    obj->field20 = 0x30;
    randu_minimum(obj);

    obj->a10 = obj->field1c;

    return mk3_install(thread, (MK3THREADFUNC)t_ss1);
}


/* --------------------------------------------------------------------- q_is_this_a_joke
 *
 * armv7 0x000a9ea0, 68 bytes.  **Complete.**
 *
 * A joke round needs BOTH fighters at match wins 0 (`get_my_matchw`/
 * `get_his_matchw` both answer 0 in `field1c`) AND this object weaker
 * or equal strength AND the opponent's own strength no more than `0x53`.
 * Any of those failing answers no.
 */
void get_my_matchw(MK3OBJ *obj);
void get_his_matchw(MK3OBJ *obj);
void get_his_strength(MK3OBJ *obj);
void get_my_strength(MK3OBJ *obj);

long q_is_this_a_joke(MK3OBJ *obj)
{
    get_my_matchw(obj);
    if (obj->field1c == 0)                  /* cbz: no match word, no joke */
        goto no;

    get_his_matchw(obj);
    if (obj->field1c != 0)
        goto no;

    get_his_strength(obj);
    obj->field30 = obj->field1c;
    get_my_strength(obj);

    if ((int32_t)obj->field1c < (int32_t)obj->field30)
        goto no;
    if ((int32_t)obj->field30 > 0x53)
        goto no;

    q_yes(obj);
    return (long)(uintptr_t)obj;   /* r0 left over from the "mov r0,r4" before the call */

no:
    q_no(obj);
    return (long)(uintptr_t)obj;   /* same leftover-register shape */
}


/* --------------------------------------------------------------------- t_mc_flipk_away
 *
 * armv7 0x000a8d8c, 84 bytes.  **Complete.**
 *
 * State 0 only. Close (`field28 <= 0x80`) installs `t_d_block`
 * (pointer slot `0x000f3418`); further away installs
 * `t_return_to_beware` (`0x000f3428`) -- **corrected**: the two slots
 * were first read the wrong way round, as `t_motaro_slided` and
 * `t_d_block`, which the oracle cannot see through a pointer slot --
 * one physical install site, two literal pointers, reached from both
 * branches.
 */
long t_motaro_slided(struct MK3THREAD *thread);
long t_d_block(struct MK3THREAD *thread);             /* pointer slot 0x000f3418 */

long t_mc_flipk_away(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);
    MK3THREADFUNC handler;

    if (slot != 0)
        return -3;

    get_x_dist(obj);
    if ((int32_t)obj->field28 > 0x80)
        handler = (MK3THREADFUNC)t_return_to_beware;
    else
        handler = (MK3THREADFUNC)t_d_block;

    return mk3_install(thread, handler);
}


/* --------------------------------------------------------------------- t_skc_lk_zap_lo
 *
 * armv7 0x000a8de0, 84 bytes.  **Complete.**
 *
 * State 0 only, Shao Kahn's twin of `t_mc_flipk_away`: close (`field28
 * <= 0x6f`) installs `t_sk_air_charge`, further installs `t_sk_charge`.
 */
long t_sk_air_charge(struct MK3THREAD *thread);
long t_sk_charge(struct MK3THREAD *thread);

long t_skc_lk_zap_lo(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);
    MK3THREADFUNC handler;

    if (slot != 0)
        return -3;

    get_x_dist(obj);
    /* far: the air charge, near: the ground one (literals resolved by
     * difftest -- the two were the other way round) */
    if ((int32_t)obj->field28 <= 0x6f)
        handler = (MK3THREADFUNC)t_sk_charge;
    else
        handler = (MK3THREADFUNC)t_sk_air_charge;

    return mk3_install(thread, handler);
}


/* --------------------------------------------------------------------- t_mc_hover
 *
 * armv7 0x000ab890, 88 bytes.  **Complete.**
 *
 * State 0 only. Rolls `motaro_randper` (result unused -- a chance
 * table probably fed elsewhere), then installs `t_motaro_hop` when far
 * (`field28 > 0x6f`) or `t_motaro_punch` when close, no push either
 * way.
 */
long t_motaro_hop(struct MK3THREAD *thread);
long t_motaro_punch(struct MK3THREAD *thread);

long t_mc_hover(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);
    MK3THREADFUNC handler;

    if (slot != 0)
        return -3;

    motaro_randper(obj);

    get_x_dist(obj);
    if ((int32_t)obj->field28 > 0x6f)
        handler = (MK3THREADFUNC)t_motaro_hop;
    else
        handler = (MK3THREADFUNC)t_motaro_punch;

    return mk3_install(thread, handler);
}


/* --------------------------------------------------------------------- t_mc_propell_ls
 *
 * armv7 0x000ab8e8, 92 bytes.  **Complete.**
 *
 * State 0 only. Rolls `motaro_randper` (unused), then close (`field28
 * <= 0x70`) installs `t_d_block` (slot 0x000f3418), far installs
 * `t_b_return_to_beware_4get`.
 */
long t_mc_propell_ls(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);
    MK3THREADFUNC handler;

    if (slot != 0)
        return -3;

    motaro_randper(obj);

    get_x_dist(obj);
    if ((int32_t)obj->field28 > 0x70)
        handler = (MK3THREADFUNC)t_b_return_to_beware_4get;
    else
        handler = (MK3THREADFUNC)t_d_block;

    return mk3_install(thread, handler);
}


/* --------------------------------------------------------------------- t_skc_propell
 *
 * armv7 0x000a8e34, 84 bytes.  **Complete.**
 *
 * State 0 only, Shao Kahn's twin: close (`field28 <= 0x90`) installs
 * `t_d_block` (slot 0x000f3418), far installs `t_b_return_to_beware_4get`.
 */
long t_skc_propell(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);
    MK3THREADFUNC handler;

    if (slot != 0)
        return -3;

    get_x_dist(obj);
    if ((int32_t)obj->field28 > 0x90)
        handler = (MK3THREADFUNC)t_b_return_to_beware_4get;
    else
        handler = (MK3THREADFUNC)t_d_block;

    return mk3_install(thread, handler);
}


/* --------------------------------------------------------------------- t_skc_stationary
 *
 * armv7 0x000abd88, 104 bytes.  **Complete.**
 *
 * State 0 only. `sk_randper` answers in `field5c`; a hit installs
 * `t_return_to_beware` directly (the same pointer slot
 * `t_b_return_to_beware_4get` reaches by name). A miss falls to
 * distance: close (`field28 <= 0x70`) installs `t_d_block`, far
 * installs `t_b_return_to_beware_4get` -- one physical install site,
 * three literal targets, reached from all three branches.
 */
long t_return_to_beware(struct MK3THREAD *thread);   /* pointer slot 0x000f3428 */

long t_skc_stationary(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);
    MK3THREADFUNC handler;

    if (slot != 0)
        return -3;

    sk_randper(obj);
    if (obj->field5c == 0) {                /* cbnz goes on to the distance (difftest) */
        handler = (MK3THREADFUNC)t_return_to_beware;
        return mk3_install(thread, handler);
    }

    get_x_dist(obj);
    if ((int32_t)obj->field28 > 0x70)
        handler = (MK3THREADFUNC)t_b_return_to_beware_4get;
    else
        handler = (MK3THREADFUNC)t_d_block;

    return mk3_install(thread, handler);
}


/* --------------------------------------------------------------------- t_mc_stationary
 *
 * armv7 0x000ab828, 104 bytes.  **Complete.**
 *
 * The same shape as `t_skc_stationary` for Motaro: `motaro_randper`
 * hits install `t_return_to_beware` directly; a miss falls to distance
 * -- close (`field28 <= 0x8a`) installs `t_b_block`, far installs
 * `t_b_return_to_beware_4get`.
 */
long t_b_block(struct MK3THREAD *thread);

long t_mc_stationary(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);
    MK3THREADFUNC handler;

    if (slot != 0)
        return -3;

    motaro_randper(obj);
    if (obj->field5c == 0) {                /* cbnz goes on to the distance (difftest) */
        handler = (MK3THREADFUNC)t_return_to_beware;
        return mk3_install(thread, handler);
    }

    get_x_dist(obj);
    if ((int32_t)obj->field28 > 0x8a)
        handler = (MK3THREADFUNC)t_b_return_to_beware_4get;
    else
        handler = (MK3THREADFUNC)t_b_block;

    return mk3_install(thread, handler);
}


/* --------------------------------------------------------------------- MotaroPunchDamage
 *
 * armv7 0x000a8560, 44 bytes.  **Complete.**
 *
 * Motaro's punch damage by difficulty (`*Difficulty`, a global int at
 * `0x0014e20c` reached through pointer slot `0x000f3624`), each level
 * added to its own value rather than a plain table: `0 + 0xa`,
 * `1 + 0x13`, `2 + 0x17`, a flat `0x20` for exactly `3`, `0x28` for
 * anything else.
 */
extern int32_t *Difficulty;                  /* 0x0014e20c */

int32_t MotaroPunchDamage(void)
{
    int32_t d = *Difficulty;

    switch (d) {
    case 0:  return d + 0xa;
    case 1:  return d + 0x13;
    case 2:  return d + 0x17;
    case 3:  return 0x20;
    default: return 0x28;
    }
}


/* --------------------------------------------------------------------- t_sk_zap
 *
 * armv7 0x000a9018, 132 bytes.  **Complete.**
 *
 * State 0 only. `field1c=0`, `group_sound`, then `field1c=0x1d` and a
 * push of `mkzap.c`'s own `t_do_zap` under `0x2e1`; `0x2e1` just
 * installs `t_local_reaction_exit` on the current level.
 */
long t_do_zap(struct MK3THREAD *thread);   /* mkzap.c */

long t_sk_zap(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);

    if (slot == 0x2e1)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

    if (slot != 0)
        return -3;

    obj->field1c = 0;
    group_sound(obj);

    obj->field1c = 0x1d;

    *mk3_frame(thread, thread->frame + 1) = 0x2e1;
    thread->frame = thread->frame + 1;   /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_do_zap;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------------------- t_ease5
 *
 * armv7 0x000a9810, 136 bytes.  **Complete.**
 *
 * `t_sk_stance_pause`'s own push target. State 0 rolls a random
 * 0x10..0x1f duration (`randu_minimum`, both bounds `0x10`), parks it
 * in `a10`, and pushes `mkdrone.c`'s own `t_d_stance_pause` under
 * `0x473`; `0x473` installs `t_local_reaction_exit` on the current
 * level.
 */
long t_d_stance_pause(struct MK3THREAD *thread);   /* not yet decompiled, mkdrone.c */

long t_ease5(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);

    if (slot == 0x473)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

    if (slot != 0)
        return -3;

    obj->field1c = 0x10;
    obj->field20 = 0x10;
    randu_minimum(obj);

    obj->a10 = obj->field1c;

    *mk3_frame(thread, thread->frame + 1) = 0x473;
    thread->frame = thread->frame + 1;   /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_d_stance_pause;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------------------- t_boss_close_miss
 *
 * armv7 0x000a895c, 140 bytes.  **Complete.**
 *
 * `t_boss_post_hit`'s own push target. The free arms `0x588` and
 * sleeps 8 ticks; `0x588` (`field1c=3`) pushes `t_mframew` under
 * `0x58a`; `0x58a` installs `t_local_reaction_exit` on the current
 * level.
 */
long t_mframew(struct MK3THREAD *thread);

long t_boss_close_miss(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);

    if (slot == 0x58a)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

    if (slot == 0x588) {
        obj->field1c = 3;

        *mk3_frame(thread, thread->frame + 1) = 0x58a;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot != 0)
        return -3;

    *mk3_frame(thread, thread->frame + 1) = 0x588;
    thread->fieldfc = 8;
    return 8;
}


/* --------------------------------------------------------------------- t_motaro_hip_jump
 *
 * armv7 0x000a8890, 152 bytes.  **Complete.**
 *
 * State 0 arms `0x4c1` and pushes `joy.c`'s own `t_check_winner_status`
 * (pointer slot `0x000f37a8`), keeping the caller's own frame index
 * (`ip`) for the pop; `0x4c3` -- reached either from `0x4c1` popping
 * back (frame<=0 special-cased directly here, no shared "pop or exit"
 * helper) or from direct dispatch -- pushes `t_motaro_hip_jsrp`.
 */
long t_check_winner_status(struct MK3THREAD *thread);   /* pointer slot 0x000f37a8, joy.c */
long t_motaro_hip_jsrp(struct MK3THREAD *thread);

long t_motaro_hip_jump(MK3THREAD *thread)
{
    uint32_t frame = thread->frame;
    uint32_t slot  = *mk3_frame(thread, frame + 1);

    if (slot == 0x4c1) {
        *mk3_frame(thread, frame + 1) = 0x4c3;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_motaro_hip_jsrp;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot == 0x4c3) {
        mk3_frame(thread, frame)[1] = (uint32_t)(uintptr_t)t_local_reaction_exit;
        *mk3_frame(thread, frame + 1) = 0;
        return 0;
    }

    if (slot != 0)
        return -3;

    *mk3_frame(thread, frame + 1) = 0x4c1;
    thread->frame = thread->frame + 1;   /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_check_winner_status;   /* slot 0xf37a8 (difftest) */
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------------------- t_grab_ani
 *
 * armv7 0x000aa7f4, 116 bytes.  **Complete.**
 *
 * State 0: `adjust_him_xy`, step a frame, wait 6 ticks under `0x503`.
 * (Corrected: an invented `field1c = 6` store was removed.)
 * `0x503` is the ordinary "pop a level, or
 * install `t_local_reaction_exit` at the bottom" tail.
 */
void adjust_him_xy(MK3OBJ *obj);
long do_next_a9_frame(MK3OBJ *obj);

long t_grab_ani(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t slot  = *mk3_frame(thread, frame + 1);

    if (slot == 0x503) {
        if ((long)thread->frame > 0) {
            thread->frame = thread->frame - 1;   /* back up a level */
            return 0;
        }
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }

    if (slot != 0)
        return -3;

    adjust_him_xy(obj);
    do_next_a9_frame(obj);

    *mk3_frame(thread, frame + 1) = 0x503;
    thread->fieldfc = 6;
    return 6;
}


/* --------------------------------------------------------------------- t_boss_counter_angle
 *
 * armv7 0x000ab778, 124 bytes.  **Complete.**
 *
 * State 0: `is_towards_me`; not towards installs `t_boss_wait_land`
 * directly. Towards rolls `motaro_easy_randper`; a hit also installs
 * `t_boss_wait_land`. A miss falls to distance -- far (`field28 >
 * 0x80`) also installs `t_boss_wait_land`, close installs
 * `t_motaro_punch` -- one physical install site, four literal
 * pointers (three of them the same), reached from all four branches.
 */
long t_boss_wait_land(struct MK3THREAD *thread);
void motaro_easy_randper(MK3OBJ *obj);
long is_towards_me(MK3OBJ *obj);   /* not yet decompiled, mkdrone.c */

long t_boss_counter_angle(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);
    MK3THREADFUNC handler;

    if (slot != 0)
        return -3;

    is_towards_me(obj);
    if (obj->field5c == 0) {
        handler = (MK3THREADFUNC)t_boss_wait_land;
        return mk3_install(thread, handler);
    }

    motaro_easy_randper(obj);
    if (obj->field5c == 0) {                /* cbnz goes on to the distance (difftest) */
        handler = (MK3THREADFUNC)t_boss_wait_land;
        return mk3_install(thread, handler);
    }

    get_x_dist(obj);
    if ((int32_t)obj->field28 > 0x80)
        handler = (MK3THREADFUNC)t_boss_wait_land;
    else
        handler = (MK3THREADFUNC)t_motaro_punch;

    return mk3_install(thread, handler);
}


/* --------------------------------------------------------------------- t_ss1
 *
 * armv7 0x000ab3d8, 192 bytes.  **Complete.**
 *
 * State 0: `stance_setup`, `next_anirate`, push (manual, plants resume
 * token `0x2ce` at the level above, sets the new level's handler to
 * `joy.c`'s own `t_check_winner_status`) and return.
 *
 * `0x2ce` (winner check came back): `am_i_facing_him`; not facing
 * installs `mkdrone.c`'s `t_d_turnaround` on the current level (no
 * push); facing re-arms this SAME handler for `0x2d2` next tick
 * (token written, no push, no handler change) and sleeps one frame.
 *
 * `0x2d2`: count `obj->a10` (the argument slot) down; still positive re-runs the
 * `next_anirate`+push tail (shared with state 0's own tail, reached
 * two ways); at zero installs `t_local_reaction_exit` on the current
 * level.
 */
void stance_setup(MK3OBJ *obj);
long am_i_facing_him(MK3OBJ *obj);
long next_anirate(MK3OBJ *obj);
long t_d_turnaround(struct MK3THREAD *thread);   /* not yet decompiled, mkdrone.c */

long t_ss1(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);
    MK3THREADFUNC handler;

    if (slot == 0x2d2) {
        obj->a10 = obj->a10 - 1;
        if ((int32_t)obj->a10 != 0)
            goto push_check_winner;

        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }

    if (slot == 0x2ce) {
        am_i_facing_him(obj);
        if (obj->field5c != 0) {
            *mk3_frame(thread, thread->frame + 1) = 0x2d2;
            thread->fieldfc = 1;
            return 1;
        }

        handler = (MK3THREADFUNC)t_d_turnaround;
        return mk3_install(thread, handler);
    }

    if (slot != 0)
        return -3;

    stance_setup(obj);

push_check_winner:
    next_anirate(obj);

    *mk3_frame(thread, thread->frame + 1) = 0x2ce;   /* resume token, level above */
    thread->frame = thread->frame + 1;               /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_check_winner_status;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------------------- t_motaro_hip_jsrp
 *
 * armv7 0x000aa330, 260 bytes.  **Complete.**
 *
 * State 0: sound, pose, push (plants resume token `0x4b2` at the
 * level above, descends into `other.c`'s `t_animate_a0_frames`).
 *
 * `0x4b2` (animate came back): sets up a jump arc in `field1c/20/24/28`,
 * pushes `other.c`'s `t_flight`, resume token `0x4b7`.
 *
 * `0x4b7` (flight came back): sound, pose, installs `other.c`'s
 * `t_mframew` on the current level (no push).
 *
 * Any other token: refused with -3.
 */
void ochar_sound(MK3OBJ *obj);
void shake_n_sound(MK3OBJ *obj);
void find_ani_part2(MK3OBJ *obj);

long t_motaro_hip_jsrp(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0x4b2) {
        obj->field1c = 0x10000;
        obj->field20 = obj->field1c - 0x70000;
        obj->field24 = obj->field20 + 0x68000;
        obj->field28 = 4;

        *mk3_frame(thread, frame + 1) = 0x4b7;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x4b7) {
        shake_n_sound(obj);
        obj->field40 = 0x1a;
        find_ani_part2(obj);
        obj->field1c = 3;

        return mk3_install(thread, (MK3THREADFUNC)t_mframew);
    }

    if (token != 0)
        return -3;

    obj->field1c = token;   /* 0, the leftover token */
    ochar_sound(obj);
    obj->field40 = 0x1a;
    get_char_ani(obj);
    obj->field1c = 0x20003;

    *mk3_frame(thread, frame + 1) = 0x4b2;
    thread->frame = thread->frame + 1;   /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_animate_a0_frames;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------------------- t_motaro_punch
 *
 * armv7 0x000aa050, 224 bytes.  **Complete.**
 *
 * State 0: sound and pose, then loads a triple from `_boss_attack_info`
 * (range, count, animation) into `field40/field48/field1c`, sets
 * `a10 = 3`, and pushes `other.c`'s `t_striker` -- a real call, so
 * this level's own resume token is `0x57e`.
 *
 * `0x57e` (striker came back): `field5c` decides -- a hit re-arms this
 * same handler for `0x584` and sleeps 8 ticks; a miss installs
 * `t_boss_close_miss` on the current level (no push).
 *
 * `0x584`: installs `t_boss_post_hit` on the current level.
 *
 * Any other token: refused with -3.
 */
extern int16_t boss_attack_info[];   /* 0x0017b3c4 */
long t_striker(struct MK3THREAD *thread);

long t_motaro_punch(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0x57e) {
        if (obj->field5c != 0) {
            *mk3_frame(thread, frame + 1) = 0x584;
            thread->fieldfc = 8;
            return 8;
        }

        return mk3_install(thread, (MK3THREADFUNC)t_boss_close_miss);
    }

    if (token == 0x584)
        return mk3_install(thread, (MK3THREADFUNC)t_boss_post_hit);

    if (token != 0)
        return -3;

    obj->field1c = 2;
    ochar_sound(obj);

    obj->field1c = token;   /* 0, the leftover token */
    obj->field20 = token;
    obj->field38 = (uint32_t)(uintptr_t)boss_attack_info;
    obj->field40 = boss_attack_info[0];
    obj->field48 = boss_attack_info[1];
    obj->field1c = boss_attack_info[2];
    obj->a10     = 3;

    *mk3_frame(thread, frame + 1) = 0x57e;
    thread->frame = thread->frame + 1;   /* push a level -- a real call */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_striker;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------------------- t_motaro_hop
 *
 * armv7 0x000aa130, 224 bytes.  **Complete.**
 *
 * State 0: sound, `get_x_dist` (whose result lands in `field28`, not
 * `field1c` -- the compare right after it is against `field1c`, which
 * is still `2` from the store just above, so the "far" side is dead
 * in practice; transcribed literally, since the oracle checks the
 * binary and not what looks intended). The near side (always taken)
 * pushes `other.c`'s `t_animate_a0_frames`, resume token `0x48a`.
 *
 * `0x48a` (animate came back), and the unreachable far side both
 * converge on the same velocity pair and both install `t_mhop7` on
 * the current level (no push) -- but they load it with the two
 * halves swapped (`field20` first vs. `field1c` first), so they are
 * kept as two physically separate stores rather than merged.
 *
 * Any other token: refused with -3.
 */
long t_mhop7(struct MK3THREAD *thread);

long t_motaro_hop(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token != 0) {
        if (token != 0x48a)
            return -3;

        obj->field20 = 0xfffd0000;
        obj->field1c = obj->field20 - 0x70000;
        return mk3_install(thread, (MK3THREADFUNC)t_mhop7);
    }

    obj->field1c = 2;
    ochar_sound(obj);
    get_x_dist(obj);
    obj->field40 = 0x1a;

    if ((int32_t)obj->field1c > 0xdf) {
        obj->field1c = 0xfff60000;
        obj->field20 = obj->field1c + 0x30000;
        return mk3_install(thread, (MK3THREADFUNC)t_mhop7);
    }

    get_char_ani(obj);
    obj->field1c = 0x20003;

    *mk3_frame(thread, frame + 1) = 0x48a;   /* resume token, level above */
    thread->frame = thread->frame + 1;        /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_animate_a0_frames;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------------------- t_b_block
 *
 * armv7 0x000a9cfc, 224 bytes.  **Complete.**
 *
 * State 0: `face_opponent`, push (plants resume token `0x759` at the
 * level above, descends into `mkstat.c`'s `t_do_block_hi`).
 *
 * `0x759` (block-high came back): `a10 = 0x40`, push again -- resume
 * token `0x75b`, descends into `mkdrone.c`'s `t_d_wait_nonattack`.
 *
 * `0x75b` (wait-nonattack came back): `get_x_dist`; far installs
 * `t_local_reaction_exit` on the current level, close installs
 * `t_motaro_grab_punch` instead -- one physical install site, two
 * literal handlers, reached from both branches.
 *
 * Any other token: refused with -3.
 */
void face_opponent(MK3OBJ *obj);
long t_do_block_hi(struct MK3THREAD *thread);
long t_d_wait_nonattack(struct MK3THREAD *thread);   /* not yet decompiled, mkdrone.c */

long t_b_block(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);
    MK3THREADFUNC handler;

    if (token == 0x759) {
        obj->a10 = 0x40;

        *mk3_frame(thread, frame + 1) = 0x75b;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_d_wait_nonattack;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x75b) {
        get_x_dist(obj);
        if ((int32_t)obj->field28 > 0x7f)
            handler = (MK3THREADFUNC)t_local_reaction_exit;
        else
            handler = (MK3THREADFUNC)t_motaro_grab_punch;

        return mk3_install(thread, handler);
    }

    if (token != 0)
        return -3;

    face_opponent(obj);

    *mk3_frame(thread, frame + 1) = 0x759;   /* resume token, level above */
    thread->frame = thread->frame + 1;        /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_do_block_hi;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------------------- t_sk_charge
 *
 * armv7 0x000ab210, 456 bytes.  **Complete.**
 *
 * State 0: the usual special-move setup (`init_special`, sounds, pose,
 * `init_anirate`, a fireball via `create_fx`, `towards_x_vel`),
 * `field48 = 0x14`, `a10 = 6`, then falls straight into the `0x339`
 * re-arm tail below.
 *
 * `0x339`: `next_anirate`, count `a10` down. At `a10 == 0`: `a10 = 1`,
 * `field1c = 3`, `strike_check_a0`; a miss rejoins the `field48`
 * countdown below; a hit stops the player, poses on the last frame,
 * re-arms `0x349` and sleeps 32. Otherwise count `field48` down; still
 * positive re-arms `0x339` and sleeps 1 (the whole state's own loop).
 *
 * `field48 == 0`: `am_i_facing_him`; not facing stops the player,
 * re-arms `0x36b` and sleeps 8 -- the same tail `0x364` falls into
 * when ITS `field48` countdown reaches 0. Facing sets `field48 = 8`
 * and falls into the `0x364` setup: splits `field08->field18` (its
 * absolute value) three ways into `field20/24/1c`, `towards_x_vel`,
 * re-arms `0x364`, sleeps 1.
 *
 * `0x364`: counts `field48` down; still positive repeats the setup
 * above; at 0 shares the "not facing" tail.
 *
 * `0x349` or `0x36b`: pose, `find_ani_part2`, `field1c = 3`, pushes
 * (resume token `0x371`) `other.c`'s `t_mframew`.
 *
 * `0x371`: installs `t_local_reaction_exit` on the current level (no
 * push) -- the one state this function can also be dispatched into
 * directly, sharing that single physical install with the push above.
 *
 * Any other token: refused with -3.
 */
void init_anirate(MK3OBJ *obj);
void create_fx(MK3OBJ *obj);
void towards_x_vel(MK3OBJ *obj);
long strike_check_a0(MK3OBJ *obj);
void stop_me_player(MK3OBJ *obj);
void find_last_frame(MK3OBJ *obj);

long t_sk_charge(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);
    int32_t  r3;

    if (token == 0x349 || token == 0x36b)
        goto push_371;

    if (token > 0x349) {
        if (token == 0x371)
            return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
        if (token == 0x364)
            goto state_364;
        return -3;
    }

    if (token == 0) {
        init_special(obj);
        obj->field1c = token;   /* 0, leftover */
        group_sound(obj);
        obj->field1c = token;   /* 0, leftover, again */
        ochar_sound(obj);
        obj->field40 = 0x19;
        get_char_ani(obj);
        obj->field1c = 3;
        init_anirate(obj);
        obj->field1c = 1;
        create_fx(obj);
        obj->field1c = 0x80000;
        towards_x_vel(obj);
        obj->field48 = 0x14;
        obj->a10     = 0x14 - 0xe;
        goto rearm_339;
    }

    if (token != 0x339)
        return -3;

    next_anirate(obj);
    obj->a10 = obj->a10 - 1;
    if ((int32_t)obj->a10 == 0) {
        obj->a10 = 1;
        obj->field1c = 3;
        strike_check_a0(obj);
        if (obj->field5c == 0)
            goto decrement_field48;

        stop_me_player(obj);
        obj->field40 = 0x19;
        get_char_ani(obj);
        find_last_frame(obj);
        do_next_a9_frame(obj);

        *mk3_frame(thread, frame + 1) = 0x349;
        thread->fieldfc = 0x20;
        return 0x20;
    }

decrement_field48:
    obj->field48 = obj->field48 - 1;
    if ((int32_t)obj->field48 != 0) {
rearm_339:
        *mk3_frame(thread, frame + 1) = 0x339;
        thread->fieldfc = 1;
        return 1;
    }

    am_i_facing_him(obj);
    if (obj->field5c == 0) {
stop_and_rearm_36b:
        stop_me_player(obj);
        *mk3_frame(thread, frame + 1) = 0x36b;
        thread->fieldfc = 8;
        return 8;
    }

    obj->field48 = 8;

setup_364:
    r3 = (int32_t)obj->field08->field18;
    obj->field1c = (uint32_t)r3;
    if (r3 < 0)
        obj->field1c = (uint32_t)(-r3);

    {
        int32_t v   = (int32_t)obj->field1c;
        int32_t a   = v >> 2;
        int32_t rem = v - a;
        int32_t b   = rem >> 3;

        obj->field20 = (uint32_t)a;
        obj->field24 = (uint32_t)b;
        obj->field1c = (uint32_t)(rem - b);
    }
    towards_x_vel(obj);

    *mk3_frame(thread, frame + 1) = 0x364;
    thread->fieldfc = 1;
    return 1;

state_364:
    obj->field48 = obj->field48 - 1;
    if ((int32_t)obj->field48 != 0)
        goto setup_364;
    goto stop_and_rearm_36b;

push_371:
    obj->field40 = 0x19;
    find_ani_part2(obj);
    obj->field1c = 3;

    *mk3_frame(thread, frame + 1) = 0x371;   /* resume token, level above */
    thread->frame = thread->frame + 1;        /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_mframew;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------------------- t_sk_air_charge
 *
 * armv7 0x000ab008, 520 bytes.  **Complete.**
 *
 * State 0: special-move setup (`init_special`, sounds, `set_nocol`,
 * pose, a frame step), re-arms `0x383` and sleeps 3.
 *
 * `0x383`: a frame step, re-arms `0x385` and sleeps 3.
 *
 * `0x385`: `init_anirate`, a fireball via `create_fx` -- this one also
 * pokes `field20` and, oddly, `field08->field1c` (the OPPONENT's own
 * scratch field, not this object's) with the same constant --
 * `towards_x_vel`, `a10 = 4`, `field48 = 0x10`, re-arms `0x397` and
 * sleeps 1.
 *
 * `0x397`: `next_anirate`, counts `a10` down. At 0: `a10 = 1`,
 * `field1c = 5`, `strike_check_a0`; a miss rejoins the `field48`
 * countdown below; a hit clears `nocol`, stops the player, poses on
 * the last frame, re-arms `0x3a9` and sleeps 24. Otherwise counts
 * `field48` down; still positive loops back into `0x385`'s own
 * re-arm-and-sleep tail (same physical store, reached two ways).
 *
 * `field48 == 0`: `clear_nocol`, `am_i_facing_him`; not facing stops
 * the player and re-arms `0x3cd` -- the same tail `0x3c6` falls into
 * when ITS `field48` countdown reaches 0. Facing sets `field48 = 8`
 * and falls into the `0x3c6` setup: splits `field08->field18` (its
 * absolute value) three ways into `field20/24/1c`, `towards_x_vel`,
 * re-arms `0x3c6`, sleeps 1.
 *
 * `0x3c6`: counts `field48` down; still positive repeats the setup
 * above; at 0 shares the "not facing" tail.
 *
 * `0x3cd` or `0x3a9`: pose, `find_ani_part2`, a frame step, installs
 * `mkslam.c`'s `t_drop_down_land_jump` on the current level (no push)
 * -- one physical install, two tokens reaching it.
 *
 * Any other token: refused with -3.
 */
void set_nocol(MK3OBJ *obj);
void clear_nocol(MK3OBJ *obj);
long t_drop_down_land_jump(struct MK3THREAD *thread);

long t_sk_air_charge(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);
    int32_t  r3;

    if (token == 0x3cd || token == 0x3a9) {
        obj->field40 = 0x14;
        find_ani_part2(obj);
        do_next_a9_frame(obj);

        return mk3_install(thread, (MK3THREADFUNC)t_drop_down_land_jump);
    }

    if (token > 0x397) {
        if (token == 0x3c6)
            goto state_3c6;
        return -3;
    }

    if (token == 0x397)
        goto state_397;

    if (token == 0x383) {
        do_next_a9_frame(obj);

        *mk3_frame(thread, frame + 1) = 0x385;
        thread->fieldfc = 3;
        return 3;
    }

    if (token == 0x385) {
        obj->field1c = 4;
        init_anirate(obj);
        obj->field1c = 1;
        create_fx(obj);
        obj->field20        = 0xfffc0000;
        obj->field08->field1c = 0xfffc0000;
        obj->field1c = 0xa0000;
        towards_x_vel(obj);
        obj->a10    = 4;
        obj->field48 = 0x10;

rearm_397:
        *mk3_frame(thread, frame + 1) = 0x397;
        thread->fieldfc = 1;
        return 1;
    }

    if (token != 0)
        return -3;

    init_special(obj);
    obj->field1c = token;   /* 0, leftover */
    group_sound(obj);
    obj->field1c = token;   /* 0, leftover, again */
    ochar_sound(obj);
    set_nocol(obj);
    obj->field40 = 0x14;
    get_char_ani(obj);
    do_next_a9_frame(obj);

    *mk3_frame(thread, frame + 1) = 0x383;
    thread->fieldfc = 3;
    return 3;

state_397:
    next_anirate(obj);
    obj->a10 = obj->a10 - 1;
    if ((int32_t)obj->a10 == 0) {
        obj->a10 = 1;
        obj->field1c = 5;
        strike_check_a0(obj);
        if (obj->field5c == 0)
            goto decrement_field48_385;

        clear_nocol(obj);
        stop_me_player(obj);
        obj->field40 = 0x14;
        get_char_ani(obj);
        find_last_frame(obj);
        do_next_a9_frame(obj);

        *mk3_frame(thread, frame + 1) = 0x3a9;
        thread->fieldfc = 0x18;
        return 0x18;
    }

decrement_field48_385:
    obj->field48 = obj->field48 - 1;
    if ((int32_t)obj->field48 != 0)
        goto rearm_397;

    clear_nocol(obj);
    am_i_facing_him(obj);
    if (obj->field5c == 0) {
stop_and_rearm_3cd:
        stop_me_player(obj);
        *mk3_frame(thread, frame + 1) = 0x3cd;
        thread->fieldfc = 8;
        return 8;
    }

    obj->field48 = 8;

setup_3c6:
    r3 = (int32_t)obj->field08->field18;
    obj->field1c = (uint32_t)r3;
    if (r3 < 0)
        obj->field1c = (uint32_t)(-r3);

    {
        int32_t v   = (int32_t)obj->field1c;
        int32_t a   = v >> 2;
        int32_t rem = v - a;
        int32_t b   = rem >> 3;

        obj->field20 = (uint32_t)a;
        obj->field24 = (uint32_t)b;
        obj->field1c = (uint32_t)(rem - b);
    }
    towards_x_vel(obj);

    *mk3_frame(thread, frame + 1) = 0x3c6;
    thread->fieldfc = 1;
    return 1;

state_3c6:
    obj->field48 = obj->field48 - 1;
    if ((int32_t)obj->field48 != 0)
        goto setup_3c6;
    goto stop_and_rearm_3cd;
}


/* --------------------------------------------------------------------- t_mhop7
 *
 * armv7 0x000aa434, 188 bytes.  **Complete.**
 *
 * State 0: sets up `field24/28`, pushes `other.c`'s `t_flight`,
 * resume token `0x497`.
 *
 * `0x497` (flight came back): sound, pose, pushes `other.c`'s
 * `t_mframew`, resume token `0x49d`.
 *
 * `0x49d` (mframew came back): installs `t_local_reaction_exit` on
 * the current level -- also reachable directly, sharing that single
 * physical install with the push above.
 *
 * Any other token: refused with -3.
 */
long t_mhop7(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0x497) {
        shake_n_sound(obj);
        obj->field40 = 0x1a;
        find_ani_part2(obj);
        obj->field1c = 3;

        *mk3_frame(thread, frame + 1) = 0x49d;   /* resume token, level above */
        thread->frame = thread->frame + 1;        /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x49d)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

    if (token != 0)
        return -3;

    obj->field24 = 0x8000;
    obj->field28 = 4;

    *mk3_frame(thread, frame + 1) = 0x497;   /* resume token, level above */
    thread->frame = thread->frame + 1;        /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_flight;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------------------- t_skc_swat_gun
 *
 * armv7 0x000abd20, 104 bytes.  **Complete.**
 *
 * State 0 only: `sk_randper`; a hit installs `t_return_to_beware` on
 * the current level. A miss checks distance -- far installs `t_sk_zap`
 * (this file's own), close installs `mkdrone.c`'s `t_d_block` -- one
 * physical install site, three literal handlers, reached three ways.
 *
 * Any other token: refused with -3.
 */
long sk_randper(MK3OBJ *obj);

long t_skc_swat_gun(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);
    MK3THREADFUNC handler;

    if (slot != 0)
        return -3;

    sk_randper(obj);
    if (obj->field5c == 0) {                /* cbnz goes on to the distance (difftest) */
        handler = (MK3THREADFUNC)t_return_to_beware;
        return mk3_install(thread, handler);
    }

    get_x_dist(obj);
    if ((int32_t)obj->field28 > 0xd0)
        handler = (MK3THREADFUNC)t_sk_zap;
    else
        handler = (MK3THREADFUNC)t_d_block;

    return mk3_install(thread, handler);
}


/* --------------------------------------------------------------------- t_mc_angle_jump
 *
 * armv7 0x000ab6f8, 128 bytes.  **Complete.**
 *
 * State 0 only: `is_towards_me`; not towards installs
 * `t_return_to_beware`. Towards rolls `motaro_easy_randper`; a miss
 * also installs `t_return_to_beware`. A hit checks distance -- far
 * installs `t_return_to_beware` too, close installs `t_motaro_punch`
 * instead -- one physical install site, four converging paths.
 *
 * Any other token: refused with -3.
 */
long t_mc_angle_jump(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);
    MK3THREADFUNC handler;

    if (slot != 0)
        return -3;

    is_towards_me(obj);
    if (obj->field5c == 0) {
        handler = (MK3THREADFUNC)t_return_to_beware;
        return mk3_install(thread, handler);
    }

    motaro_easy_randper(obj);
    if (obj->field5c == 0) {
        handler = (MK3THREADFUNC)t_return_to_beware;
        return mk3_install(thread, handler);
    }

    get_x_dist(obj);
    if ((int32_t)obj->field28 > 0x80)
        handler = (MK3THREADFUNC)t_return_to_beware;
    else
        handler = (MK3THREADFUNC)t_motaro_punch;

    return mk3_install(thread, handler);
}


/* --------------------------------------------------------------------- t_sk_collapse
 *
 * armv7 0x000a968c, 180 bytes.  **Complete.**
 *
 * State 0: pose, push (plants resume token `0x830` at the level
 * above, descends into `other.c`'s `t_animate_a9`).
 *
 * `0x830` (animate came back): sound, `field1c = 3`, pushes `other.c`'s
 * `t_mframew`, resume token `0x834`.
 *
 * `0x834` (mframew came back): installs `other.c`'s `t_wait_forever`
 * on the current level.
 *
 * Any other token: refused with -3.
 */
long t_sk_collapse(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0x830) {
        shake_n_sound(obj);
        obj->field1c = 3;

        *mk3_frame(thread, frame + 1) = 0x834;   /* resume token, level above */
        thread->frame = thread->frame + 1;        /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x834)
        return mk3_install(thread, (MK3THREADFUNC)t_wait_forever);

    if (token != 0)
        return -3;

    obj->field40 = 0x3001e;

    *mk3_frame(thread, frame + 1) = 0x830;   /* resume token, level above */
    thread->frame = thread->frame + 1;        /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_animate_a9;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------------------- t_skc_zap
 *
 * armv7 0x000abf7c, 132 bytes.  **Complete.**
 *
 * State 0 only: `field1c = 0x320`, `bossrandper`; a miss installs
 * `t_return_to_beware`. A hit rolls `sk_counter_randper`; a miss there
 * installs `t_sk_block_zap`. A hit checks distance -- close installs
 * `t_sk_charge`, far installs `t_sk_zap` -- one physical install
 * site, four converging paths.
 *
 * Any other token: refused with -3.
 */
long t_sk_block_zap(struct MK3THREAD *thread);

long t_skc_zap(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);
    MK3THREADFUNC handler;

    if (slot != 0)
        return -3;

    obj->field1c = 0x320;
    bossrandper(obj);
    if (obj->field5c == 0) {
        handler = (MK3THREADFUNC)t_return_to_beware;
        return mk3_install(thread, handler);
    }

    sk_counter_randper(obj);
    if (obj->field5c == 0) {
        handler = (MK3THREADFUNC)t_sk_block_zap;
        return mk3_install(thread, handler);
    }

    get_x_dist(obj);
    if ((int32_t)obj->field28 > 0x6f)
        handler = (MK3THREADFUNC)t_sk_zap;
    else
        handler = (MK3THREADFUNC)t_sk_charge;

    return mk3_install(thread, handler);
}


/* --------------------------------------------------------------------- t_sk_airborn_check
 *
 * armv7 0x000a8f14, 192 bytes.  **Complete.**
 *
 * State 0 only: `am_i_airborn`. Not airborn just pops a level when
 * there is one to pop, or installs `t_local_reaction_exit` at the
 * bottom.
 *
 * Airborn and a level to pop: copies the level ABOVE's own resume
 * token down into this one (so whoever pops past THIS level resumes
 * where the caller above was heading), then re-homes the handler this
 * level was running one level down and immediately overwrites it with
 * `t_sk_knocked_down` -- a dead store, kept because the oracle checks
 * the binary's own instructions and not what looks minimal.
 *
 * Airborn with nothing to pop: installs `t_local_reaction_exit`, but
 * this path never reloads `r0` afterward, so the return value is
 * `field5c` (the airborn flag itself) left over from the check above,
 * not `0`.
 */
long am_i_airborn(MK3OBJ *obj);

long t_sk_airborn_check(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    uint32_t frame;
    uint32_t airborn;
    uint32_t old_handler;

    if (token != 0)
        return -3;

    am_i_airborn(obj);
    airborn = obj->field5c;
    frame   = thread->frame;

    if (airborn == 0) {
        if ((int32_t)frame <= 0) {
            mk3_frame(thread, frame)[1] = (uint32_t)(uintptr_t)t_local_reaction_exit;
            *mk3_frame(thread, frame + 1) = token;   /* 0 */
            return 0;
        }

        thread->frame = frame - 1;   /* pop a level */
        return 0;
    }

    /* At the bottom the exit is installed and then the SAME slide runs
     * (`b #0xa8f5a`), one level up from where it runs after a pop -- and the
     * result is 0 either way (difftest; this used to return field5c). */
    if ((int32_t)frame <= 0) {
        mk3_frame(thread, frame)[1] = (uint32_t)(uintptr_t)t_local_reaction_exit;
        *mk3_frame(thread, frame + 1) = token;   /* 0 */
    } else {
        thread->frame = frame - 1;   /* pop a level */
    }
    frame = thread->frame;

    old_handler                   = mk3_frame(thread, frame + 1)[1];
    *mk3_frame(thread, frame + 1) = *mk3_frame(thread, frame + 2);

    mk3_frame(thread, frame)[1] = old_handler;   /* dead, overwritten below */
    mk3_frame(thread, frame)[1] = (uint32_t)(uintptr_t)t_sk_knocked_down;
    *mk3_frame(thread, frame + 1) = 0;

    return 0;
}


/* --------------------------------------------------------------------- t_mc_dizzy
 *
 * armv7 0x000abb44, 176 bytes.  **Complete.**
 *
 * State 0: `field1c = 0x1f4`, `bossrandper`; a hit installs
 * `t_return_to_beware`. A miss sets `a10 = 0x80` and pushes (resume
 * token `0x6a5`) `mkdrone.c`'s `t_d_stance_pause` -- `r0` is never
 * reloaded on the way out, but `bossrandper`'s own return equals
 * `field5c`, which is `0` here, so the fall-through return is still a
 * plain `0`.
 *
 * `0x6a5`: installs `t_local_reaction_exit` on the current level.
 *
 * Any other token: refused with -3.
 */
long t_mc_dizzy(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0x6a5)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

    if (token != 0)
        return -3;

    obj->field1c = 0x1f4;
    bossrandper(obj);
    if (obj->field5c != 0) {
        MK3THREADFUNC handler = (MK3THREADFUNC)t_return_to_beware;
        return mk3_install(thread, handler);
    }

    obj->a10 = 0x80;

    *mk3_frame(thread, frame + 1) = 0x6a5;   /* resume token, level above */
    thread->frame = thread->frame + 1;        /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_d_stance_pause;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;   /* leftover: bossrandper's own return, == field5c == 0 here */
}


/* --------------------------------------------------------------------- t_motaro_hard_comboed
 *
 * armv7 0x000a9504, 204 bytes.  **Complete.**
 *
 * State 0: `field20 = 2`, push (plants resume token `0x807` at the
 * level above, descends into `mkreact.c`'s `t_avoid_corner_trap`).
 *
 * `0x807` (corner-trap came back): `shake_a11`, `rsnd_func(obj, 0xa)`,
 * `away_x_vel`, pose, pushes `other.c`'s `t_animate_a9`, resume token
 * `0x810`.
 *
 * `0x810` (animate came back): installs `t_motaro_stumble` on the
 * current level -- also reachable directly, sharing that single
 * physical install with the push above.
 *
 * Any other token: refused with -3.
 */
void rsnd_func(MK3OBJ *unused, uint32_t which);
void away_x_vel(MK3OBJ *obj);
long t_avoid_corner_trap(struct MK3THREAD *thread);
long t_motaro_stumble(struct MK3THREAD *thread);

long t_motaro_hard_comboed(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0x810)
        return mk3_install(thread, (MK3THREADFUNC)t_motaro_stumble);

    if (token == 0x807) {
        obj->field48 = 0x60006;
        shake_a11(obj);
        rsnd_func(obj, 0xa);
        obj->field1c = 0x40000;
        away_x_vel(obj);
        obj->field40 = 0x30020;

        *mk3_frame(thread, frame + 1) = 0x810;   /* resume token, level above */
        thread->frame = thread->frame + 1;        /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_animate_a9;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0)
        return -3;

    obj->field20 = 2;

    *mk3_frame(thread, frame + 1) = 0x807;   /* resume token, level above */
    thread->frame = thread->frame + 1;        /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_avoid_corner_trap;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------------------- t_boss_stalk
 *
 * armv7 0x000ab5fc, 192 bytes.  **Complete.**
 *
 * State 0 only: `q_is_he_car`; not cornered installs `t_boss1`
 * directly. Cornered rolls `bossrandper_org` at odds `0x12c`; a hit
 * installs `t_sk_laugh`. A miss rolls again at `0x320`; a hit installs
 * `t_boss_ease_back`, a second miss installs `t_boss1` -- one physical
 * install site, four converging paths.
 *
 * Any other token: refused with -3.
 */
long t_boss_ease_back(struct MK3THREAD *thread);

long t_boss_stalk(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);

    if (slot != 0)
        return -3;

    q_is_he_car(obj);
    if (obj->field5c == 0)
        return mk3_install(thread, (MK3THREADFUNC)t_boss1);

    obj->field1c = 0x12c;
    bossrandper_org(obj);
    if (obj->field5c != 0)
        return mk3_install(thread, (MK3THREADFUNC)t_sk_laugh);

    obj->field1c = 0x320;
    bossrandper_org(obj);
    if (obj->field5c != 0)
        return mk3_install(thread, (MK3THREADFUNC)t_boss_ease_back);

    return mk3_install(thread, (MK3THREADFUNC)t_boss1);
}


/* --------------------------------------------------------------------- t_motaro_stumble
 *
 * armv7 0x000a9740, 208 bytes.  **Complete.**
 *
 * State 0: `away_x_vel`, pose, push (plants resume token `0x816` at
 * the level above, descends into `other.c`'s `t_animate_a9`).
 *
 * `0x816` (animate came back): `back_to_normal`, `field1c = field20 =
 * 0x10`, `randu_minimum` (its answer read back from `field1c` into
 * `a10`), pushes `mkdrone.c`'s `t_d_stance_pause`, resume token `0x81e`.
 *
 * `0x81e` (beware came back): installs `t_local_reaction_exit` on the
 * current level -- also reachable directly, sharing that single
 * physical install with the push above.
 *
 * Any other token: refused with -3.
 */
void back_to_normal(MK3OBJ *obj);
long t_d_beware(struct MK3THREAD *thread);   /* not yet decompiled, mkdrone.c */

long t_motaro_stumble(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0x816) {
        back_to_normal(obj);
        obj->field1c = 0x10;
        obj->field20 = 0x10;
        randu_minimum(obj);
        obj->a10 = obj->field1c;

        *mk3_frame(thread, frame + 1) = 0x81e;   /* resume token, level above */
        thread->frame = thread->frame + 1;        /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_d_stance_pause;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x81e)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

    if (token != 0)
        return -3;

    obj->field1c = 0x40000;
    away_x_vel(obj);
    obj->field40 = 0x40020;

    *mk3_frame(thread, frame + 1) = 0x816;   /* resume token, level above */
    thread->frame = thread->frame + 1;        /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_animate_a9;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------------------- t_sk_hit1
 *
 * armv7 0x000a9388, 220 bytes.  **Complete.**
 *
 * State 0: `rsnd_func(obj, 8)`, `field48 = 0x40004`, `shake_a11`,
 * `field1c = 6`, `group_sound`, push (resume token `0x868`) into this
 * file's own `t_sk_airborn_check`.
 *
 * `0x868` (airborn check came back): `field1c = 0x40000`, `away_x_vel`,
 * pose, pushes `other.c`'s `t_animate_a9`, resume token `0x86d`.
 *
 * `0x86d` (animate came back): installs `t_local_reaction_exit` on
 * the current level -- also reachable directly, sharing that single
 * physical install with the push above.
 *
 * Any other token: refused with -3.
 */
long t_sk_hit1(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0x868) {
        obj->field1c = 0x40000;
        away_x_vel(obj);
        obj->field40 = 0x4001c;

        *mk3_frame(thread, frame + 1) = 0x86d;   /* resume token, level above */
        thread->frame = thread->frame + 1;        /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_animate_a9;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x86d)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

    if (token != 0)
        return -3;

    rsnd_func(obj, 8);
    obj->field48 = 0x40004;
    shake_a11(obj);
    obj->field1c = 6;
    group_sound(obj);

    *mk3_frame(thread, frame + 1) = 0x868;   /* resume token, level above */
    thread->frame = thread->frame + 1;        /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_sk_airborn_check;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------------------- t_mc_flipkp
 *
 * armv7 0x000a9ee4, 220 bytes.  **Complete.**
 *
 * State 0 only: `q_is_this_a_joke`; a hit installs `t_return_to_beware`.
 * A miss checks `is_towards_me` -- not towards installs
 * `t_mc_flipk_away` directly. Towards checks distance -- far installs
 * `t_b_return_to_beware_4get`, close sets `field64`/`slave` (the object's
 * OWN copy of the count-and-table pair, not `field00`'s) and pushes
 * `t_random_do`, resume token `0x711` -- three of the four paths
 * converge on the same physical install site.
 *
 * Any other token: refused with -3.
 */
extern MK3THREADFUNC funcs_mc_flipkp[];   /* 0x0017b924 */

long t_mc_flipkp(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);

    if (slot != 0)
        return -3;

    q_is_this_a_joke(obj);
    if (obj->field5c != 0)
        return mk3_install(thread, (MK3THREADFUNC)t_return_to_beware);

    is_towards_me(obj);
    if (obj->field5c == 0)
        return mk3_install(thread, (MK3THREADFUNC)t_mc_flipk_away);

    get_x_dist(obj);
    if ((int32_t)obj->field28 > 0x70)
        return mk3_install(thread, (MK3THREADFUNC)t_b_return_to_beware_4get);

    obj->slave   = (uint32_t)(uintptr_t)funcs_mc_flipkp;
    obj->field64 = 2;

    *mk3_frame(thread, thread->frame + 1) = 0x711;   /* resume token, level above */
    thread->frame = thread->frame + 1;                /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_random_do;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------------------- t_motaro_flip_kicked
 *
 * armv7 0x000a9c10, 236 bytes.  **Complete.**
 *
 * State 0: `rsnd_func(obj, 0xa)`, `rsnd_ochar_sound`, `field48 =
 * 0x60006`, `shake_a11`, `away_x_vel`, pose, push (resume token
 * `0x78c`) into `other.c`'s `t_animate_a9`.
 *
 * `0x78c` (animate came back): `field1c = field20 = 0x10`,
 * `randu_minimum` (its answer read back from `field1c` into `a10`),
 * pushes `mkdrone.c`'s `t_d_stance_pause`, resume token `0x792`.
 *
 * `0x792` (beware came back): installs `t_local_reaction_exit` on the
 * current level -- also reachable directly, sharing that single
 * physical install with the push above.
 *
 * Any other token: refused with -3.
 */
long t_motaro_flip_kicked(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0x78c) {
        obj->field1c = 0x10;
        obj->field20 = 0x10;
        randu_minimum(obj);
        obj->a10 = obj->field1c;

        *mk3_frame(thread, frame + 1) = 0x792;   /* resume token, level above */
        thread->frame = thread->frame + 1;        /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_d_stance_pause;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x792)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

    if (token != 0)
        return -3;

    rsnd_func(obj, 0xa);
    obj->field1c = 0x30002;
    rsnd_ochar_sound(obj);
    obj->field48 = 0x60006;
    shake_a11(obj);
    obj->field1c = 0x50000;
    away_x_vel(obj);
    obj->field40 = 0x40020;

    *mk3_frame(thread, frame + 1) = 0x78c;   /* resume token, level above */
    thread->frame = thread->frame + 1;        /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_animate_a9;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------------------- t_motaro_hit0
 *
 * armv7 0x000a98e4, 224 bytes.  **Complete.**
 *
 * State 0: `am_i_airborn`. Airborne installs `t_motaro_hit_flight` on
 * the current level directly. Grounded: `rsnd_func(obj, 8)`,
 * `rsnd_ochar_sound`, `away_x_vel`, pose, push (resume token `0x801`)
 * into `other.c`'s `t_animate_a9`.
 *
 * `0x801` (animate came back): installs `t_local_reaction_exit` on
 * the current level -- also reachable directly, sharing that single
 * physical install with the push above.
 *
 * Any other token: refused with -3.
 */
long t_motaro_hit0(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0x801)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

    if (token != 0)
        return -3;

    am_i_airborn(obj);
    if (obj->field5c != 0)
        return mk3_install(thread, (MK3THREADFUNC)t_motaro_hit_flight);

    rsnd_func(obj, 8);
    obj->field1c = 0x30002;
    rsnd_ochar_sound(obj);
    obj->field1c = 0x20000;
    away_x_vel(obj);
    obj->field40 = 0x3001c;

    *mk3_frame(thread, frame + 1) = 0x801;   /* resume token, level above */
    thread->frame = thread->frame + 1;        /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_animate_a9;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------------------- t_sk_hammer
 *
 * armv7 0x000aae98, 208 bytes.  **Complete.**
 *
 * State 0: sound and pose, `field48 = a10 = 2`, `field1c = 1`, pushes
 * `other.c`'s `t_striker` -- a real call, so this level's own resume
 * token is `0x2f2`.
 *
 * `0x2f2` (striker came back): `field5c` decides -- a hit re-arms this
 * same handler for `0x2f5` and sleeps 14; a miss installs
 * `t_boss_close_miss` on the current level (no push).
 *
 * `0x2f5`: installs `t_boss_post_hit` on the current level.
 *
 * Any other token: refused with -3.
 */
long t_sk_hammer(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0x2f2) {
        if (obj->field5c != 0) {
            *mk3_frame(thread, frame + 1) = 0x2f5;
            thread->fieldfc = 0xe;
            return 0xe;
        }

        return mk3_install(thread, (MK3THREADFUNC)t_boss_close_miss);
    }

    if (token == 0x2f5)
        return mk3_install(thread, (MK3THREADFUNC)t_boss_post_hit);

    if (token != 0)
        return -3;

    init_special(obj);
    obj->field1c = 0;
    group_sound(obj);
    obj->field20 = 0;
    obj->field40 = 0x1a;
    obj->field48 = 2;
    obj->a10     = 2;
    obj->field1c = 1;

    *mk3_frame(thread, frame + 1) = 0x2f2;   /* resume token, level above */
    thread->frame = thread->frame + 1;        /* push a level -- a real call */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_striker;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------------------- t_mc_fk_sd
 *
 * armv7 0x000abbf4, 208 bytes.  **Complete.**
 *
 * State 0: `field1c = 0x1f4`, `bossrandper`; a miss installs
 * `t_return_to_beware`. A hit checks distance -- far installs
 * `t_return_to_beware` too, close sets `a10 = 0x40` and `field48` to
 * `is_he_airborn`'s own address (a raw function pointer parked in a
 * counter field, the same "borrow a scratch field for something else
 * entirely" this file does everywhere), then pushes `mkdrone.c`'s
 * `t_stance_wait_no`, resume token `0x698`.
 *
 * `0x698`: installs `t_motaro_grab_punch_now` on the current level.
 *
 * Any other token: refused with -3.
 */
long t_stance_wait_no(struct MK3THREAD *thread);   /* not yet decompiled, mkdrone.c */

long t_mc_fk_sd(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0x698)
        return mk3_install(thread, (MK3THREADFUNC)t_motaro_grab_punch_now);

    if (token != 0)
        return -3;

    obj->field1c = 0x1f4;
    bossrandper(obj);
    if (obj->field5c == 0)
        return mk3_install(thread, (MK3THREADFUNC)t_return_to_beware);

    get_x_dist(obj);
    if ((int32_t)obj->field28 > 0x90)
        return mk3_install(thread, (MK3THREADFUNC)t_return_to_beware);

    obj->a10     = 0x40;
    obj->field48 = (uint32_t)(uintptr_t)is_he_airborn;

    *mk3_frame(thread, frame + 1) = 0x698;   /* resume token, level above */
    thread->frame = thread->frame + 1;        /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_stance_wait_no;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------------------- t_c_zoom_sd
 *
 * armv7 0x000ab988, 212 bytes.  **Complete.**
 *
 * State 0: `field1c = 0x2bc`, `bossrandper`; a miss installs
 * `t_return_to_beware`. A hit parks `q_heading_down`'s own address in
 * `field48` (again, a raw function pointer in a counter field),
 * `a10 = 0x40`, pushes `mkdrone.c`'s `t_stance_wait_yes`, resume
 * token `0x6ec`.
 *
 * `0x6ec`: checks distance -- close installs `t_motaro_punch`, far
 * installs `t_motaro_kick` -- one physical install site, two
 * converging paths.
 *
 * Any other token: refused with -3.
 */
long t_stance_wait_yes(struct MK3THREAD *thread);   /* not yet decompiled, mkdrone.c */
long t_motaro_kick(struct MK3THREAD *thread);

long t_c_zoom_sd(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0x6ec) {
        get_x_dist(obj);
        if ((int32_t)obj->field28 > 0x70)
            return mk3_install(thread, (MK3THREADFUNC)t_motaro_kick);

        return mk3_install(thread, (MK3THREADFUNC)t_motaro_punch);
    }

    if (token != 0)
        return -3;

    obj->field1c = 0x2bc;
    bossrandper(obj);
    if (obj->field5c == 0)
        return mk3_install(thread, (MK3THREADFUNC)t_return_to_beware);

    obj->field48 = (uint32_t)(uintptr_t)q_heading_down;
    obj->a10     = 0x40;

    *mk3_frame(thread, frame + 1) = 0x6ec;   /* resume token, level above */
    thread->frame = thread->frame + 1;        /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_stance_wait_yes;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------------------- t_sk_block_zap
 *
 * armv7 0x000a9ddc, 196 bytes.  **Complete.**
 *
 * State 0: `face_opponent`, push (resume token `0x5a7`) into
 * `mkstat.c`'s `t_do_block_hi`.
 *
 * `0x5a7`: push (resume token `0x5a8`) into `mkdrone.c`'s
 * `t_wait_proj_spawn`.
 *
 * `0x5a8`: push (resume token `0x5a9`) into `mkdrone.c`'s
 * `t_wait_proj_pass`.
 *
 * `0x5a9`: installs `t_local_reaction_exit` on the current level.
 *
 * Any other token: refused with -3.
 */
long t_wait_proj_spawn(struct MK3THREAD *thread);   /* not yet decompiled, mkdrone.c */
long t_wait_proj_pass(struct MK3THREAD *thread);    /* not yet decompiled, mkdrone.c */

long t_sk_block_zap(MK3THREAD *thread)
{
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0x5a9)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

    if (token == 0x5a8) {
        *mk3_frame(thread, frame + 1) = 0x5a9;   /* resume token, level above */
        thread->frame = thread->frame + 1;        /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_wait_proj_pass;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x5a7) {
        *mk3_frame(thread, frame + 1) = 0x5a8;   /* resume token, level above */
        thread->frame = thread->frame + 1;        /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_wait_proj_spawn;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0)
        return -3;

    face_opponent((MK3OBJ *)thread->proc);

    *mk3_frame(thread, frame + 1) = 0x5a7;   /* resume token, level above */
    thread->frame = thread->frame + 1;        /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_do_block_hi;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------------------- t_sk_punch
 *
 * armv7 0x000aacf8, 208 bytes.  **Complete.**
 *
 * State 0: sound and pose, `field40 = 0xe`, `field1c = 3`, `field48 =
 * 0`, `field20 = 0`, `a10 = 1`, pushes `other.c`'s `t_striker` -- a
 * real call, so this level's own resume token is `0x31a`.
 *
 * `0x31a` (striker came back): `field5c` decides -- a hit re-arms this
 * same handler for `0x31d` and sleeps 14; a miss installs
 * `t_boss_close_miss` on the current level (no push).
 *
 * `0x31d`: installs `t_boss_post_hit` on the current level.
 *
 * Any other token: refused with -3.
 */
long t_sk_punch(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0x31a) {
        if (obj->field5c != 0) {
            *mk3_frame(thread, frame + 1) = 0x31d;
            thread->fieldfc = 0xe;
            return 0xe;
        }

        return mk3_install(thread, (MK3THREADFUNC)t_boss_close_miss);
    }

    if (token == 0x31d)
        return mk3_install(thread, (MK3THREADFUNC)t_boss_post_hit);

    if (token != 0)
        return -3;

    init_special(obj);
    obj->field1c = 0;
    group_sound(obj);
    obj->field40 = 0xe;
    obj->field48 = 0;
    obj->field1c = 3;
    obj->field20 = 0;
    obj->a10     = 1;

    *mk3_frame(thread, frame + 1) = 0x31a;   /* resume token, level above */
    thread->frame = thread->frame + 1;        /* push a level -- a real call */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_striker;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------------------- t_sk_kick
 *
 * armv7 0x000aadc8, 208 bytes.  **Complete.**
 *
 * The striker shape again (`t_sk_punch`, `t_sk_hammer`): pose with
 * `field40 = 0x11`, `field48 = 1`, `field1c = a10 = 2`, push
 * `other.c`'s `t_striker` (resume `0x306`); a hit re-arms `0x309` and
 * sleeps 14, a miss installs `t_boss_close_miss`; `0x309` installs
 * `t_boss_post_hit`. Any other token: -3.
 */
long t_sk_kick(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0x306) {
        if (obj->field5c != 0) {
            *mk3_frame(thread, frame + 1) = 0x309;
            thread->fieldfc = 0xe;
            return 0xe;
        }

        return mk3_install(thread, (MK3THREADFUNC)t_boss_close_miss);
    }

    if (token == 0x309)
        return mk3_install(thread, (MK3THREADFUNC)t_boss_post_hit);

    if (token != 0)
        return -3;

    init_special(obj);
    obj->field1c = 0;
    group_sound(obj);
    obj->field40 = 0x11;
    obj->field20 = 0;
    obj->field48 = 1;
    obj->field1c = 2;
    obj->a10     = 2;

    *mk3_frame(thread, frame + 1) = 0x306;   /* resume token, level above */
    thread->frame = thread->frame + 1;        /* push a level -- a real call */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_striker;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------------------- t_motaro_upcutted
 *
 * armv7 0x000a9aec, 292 bytes.  **Complete.**
 *
 * `t_motaro_hit0`'s airborne check in front of `t_motaro_flip_kicked`'s
 * body. State 0: `am_i_airborn`; airborne installs
 * `t_motaro_hit_flight`. Grounded: sounds, `field48 = 0x60006`,
 * `shake_a11`, `away_x_vel`, pose, push (resume `0x7b1`) into
 * `other.c`'s `t_animate_a9`.
 *
 * `0x7b1`: `field1c = field20 = 0x10`, `randu_minimum` into `a10`,
 * push (resume `0x7b7`) into `mkdrone.c`'s `t_d_stance_pause`.
 *
 * `0x7b7`: installs `t_local_reaction_exit`. Any other token: -3.
 */
long t_motaro_upcutted(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0x7b1) {
        obj->field1c = 0x10;
        obj->field20 = 0x10;
        randu_minimum(obj);
        obj->a10 = obj->field1c;

        *mk3_frame(thread, frame + 1) = 0x7b7;   /* resume token, level above */
        thread->frame = thread->frame + 1;        /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_d_stance_pause;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x7b7)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

    if (token != 0)
        return -3;

    am_i_airborn(obj);
    if (obj->field5c != 0)
        return mk3_install(thread, (MK3THREADFUNC)t_motaro_hit_flight);

    rsnd_func(obj, 0xa);
    obj->field1c = 0x30002;
    rsnd_ochar_sound(obj);
    obj->field48 = 0x60006;
    shake_a11(obj);
    obj->field1c = 0x60000;
    away_x_vel(obj);
    obj->field40 = 0x40020;

    *mk3_frame(thread, frame + 1) = 0x7b1;   /* resume token, level above */
    thread->frame = thread->frame + 1;        /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_animate_a9;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------------------- t_skc_dizzy
 *
 * armv7 0x000abe00, 292 bytes.  **Complete.**
 *
 * State 0: `field1c = 0x1f4`, `bossrandper`; a hit installs
 * `t_return_to_beware`. A miss rolls `bossrandper_org` at `0x190`; a
 * hit installs `t_sk_laugh`. A second miss sets `a10 = 0xc0`, parks
 * `q_is_he_dizzy_boss`'s address in `field48` (the wait's own test)
 * and pushes `mkdrone.c`'s `t_stance_wait_no`, resume `0x5f6`.
 *
 * `0x5f6`: `a10 = 0x40`, push (resume `0x5f8`) into `t_d_stance_pause`.
 *
 * `0x5f8`: installs `t_local_reaction_exit`. Any other token: -3.
 */
long t_skc_dizzy(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0x5f6) {
        obj->a10 = 0x40;

        *mk3_frame(thread, frame + 1) = 0x5f8;   /* resume token, level above */
        thread->frame = thread->frame + 1;        /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_d_stance_pause;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x5f8)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

    if (token != 0)
        return -3;

    obj->field1c = 0x1f4;
    bossrandper(obj);
    if (obj->field5c != 0)
        return mk3_install(thread, (MK3THREADFUNC)t_return_to_beware);

    obj->field1c = 0x190;
    bossrandper_org(obj);
    if (obj->field5c != 0)
        return mk3_install(thread, (MK3THREADFUNC)t_sk_laugh);

    obj->a10     = 0xc0;
    obj->field48 = (uint32_t)(uintptr_t)q_is_he_dizzy_boss;

    *mk3_frame(thread, frame + 1) = 0x5f6;   /* resume token, level above */
    thread->frame = thread->frame + 1;        /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_stance_wait_no;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------------------- t_boss1
 *
 * armv7 0x000ac5e0, 260 bytes.  **Complete.**
 *
 * Motaro's top-level choice. State 0 only.
 *
 * `q_ok_motaro_sweep`; if a sweep would land, roll `bossrandper` at
 * `0x12c` and a hit installs `t_motaro_sweep`. Otherwise (or on a
 * miss) measure: within `0x9f` installs `t_boss_close`. Farther rolls
 * `bossrandper` at `0xc8`: a hit hands the object's own
 * `field64`/`slave` pair a two-entry move table and pushes
 * `t_random_do` (resume `0x147`); a miss looks the far attack up in
 * `mhe_motaro_far_attax` by ladder order and installs WHATEVER THE
 * TABLE HOLDS -- the one handler in this file that is data, not code.
 *
 * Any other token: refused with -3.
 */
long t_motaro_sweep(struct MK3THREAD *thread);
long t_boss_close(struct MK3THREAD *thread);
extern MK3THREADFUNC funcs_boss1[];              /* 0x0017b9d4 */
extern uint32_t mhe_motaro_far_attax[];          /* 0x0017b3b4 */

long t_boss1(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);

    if (slot != 0)
        return -3;

    q_ok_motaro_sweep(obj);
    if (obj->field5c != 0) {
        obj->field1c = 0x12c;
        bossrandper(obj);
        if (obj->field5c != 0)
            return mk3_install(thread, (MK3THREADFUNC)t_motaro_sweep);
    }

    get_x_dist(obj);
    if ((int32_t)obj->field28 <= 0x9f)
        return mk3_install(thread, (MK3THREADFUNC)t_boss_close);

    obj->field1c = 0xc8;
    bossrandper(obj);
    if (obj->field5c != 0) {
        obj->slave   = (uint32_t)(uintptr_t)funcs_boss1;
        obj->field64 = 2;

        *mk3_frame(thread, thread->frame + 1) = 0x147;   /* resume token, level above */
        thread->frame = thread->frame + 1;                /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_random_do;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    obj->field1c = (uint32_t)(uintptr_t)mhe_motaro_far_attax;
    get_mhe_long(obj);
    mk3_frame(thread, thread->frame)[1] = obj->field1c;   /* a handler out of a table */
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------------------- t_motaro_sweep
 *
 * armv7 0x000aa210, 288 bytes.  **Complete.**
 *
 * State 0: sound, pose (`field40 = 0x14`, `a10 = field48 = 2`,
 * `field1c = 3`), push `other.c`'s `t_striker` as a real call, resume
 * `0x41a`.
 *
 * `0x41a`: a connected sweep installs `t_mot_sweep_hit`; a miss sets
 * `field1c = 4` and pushes `t_mframew`, resume `0x41f`.
 *
 * `0x41f`: `field1c = 4` again and a second `t_mframew`, resume
 * `0x421` -- the recovery is two animation runs, not one.
 *
 * `0x421`: installs `t_local_reaction_exit`. Any other token: -3.
 */
long t_mot_sweep_hit(struct MK3THREAD *thread);

long t_motaro_sweep(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0x41a) {
        if (obj->field5c != 0)
            return mk3_install(thread, (MK3THREADFUNC)t_mot_sweep_hit);

        obj->field1c = 4;

        *mk3_frame(thread, frame + 1) = 0x41f;   /* resume token, level above */
        thread->frame = thread->frame + 1;        /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x41f) {
        obj->field1c = 4;

        *mk3_frame(thread, frame + 1) = 0x421;   /* resume token, level above */
        thread->frame = thread->frame + 1;        /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x421)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

    if (token != 0)
        return -3;

    obj->field1c = 2;
    ochar_sound(obj);
    obj->field20 = 0;
    obj->field40 = 0x14;
    obj->a10     = 2;
    obj->field48 = 2;
    obj->field1c = 3;

    *mk3_frame(thread, frame + 1) = 0x41a;   /* resume token, level above */
    thread->frame = thread->frame + 1;        /* push a level -- a real call */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_striker;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------------------- t_mot_sweep_hit
 *
 * armv7 0x000ac000, 308 bytes.  **Complete.**
 *
 * What Motaro does after a sweep connects. State 0 waits 8 ticks
 * under `0x428`; `0x428` runs `t_mframew` (resume `0x42a`); `0x42a`
 * waits 10 under `0x42b`; `0x42b` runs `t_mframew` again (resume
 * `0x42d`).
 *
 * `0x42d` decides the follow-up: `mkdrone.c`'s `d_front_me_a5` leaves
 * a distance in `field30`. Nearer than `0x120` installs
 * `t_motaro_hip_jump`; farther rolls `bossrandper` at `0x1f4`, a hit
 * installing `t_motaro_hip_jump` too and a miss setting up a hop
 * (`field1c = -10.0`, `field20 = -11.0` in 16.16, `field40 = 0x1a`)
 * and installing `t_mhop7`.
 *
 * Any other token: refused with -3.
 */
void d_front_me_a5(MK3OBJ *obj);   /* not yet decompiled, mkdrone.c */

long t_mot_sweep_hit(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0x42d) {
        d_front_me_a5(obj);
        if ((int32_t)obj->field30 < 0x120)
            return mk3_install(thread, (MK3THREADFUNC)t_motaro_hip_jump);

        obj->field1c = 0x1f4;
        bossrandper(obj);
        if (obj->field5c != 0)
            return mk3_install(thread, (MK3THREADFUNC)t_motaro_hip_jump);

        obj->field1c = 0xfff60000;
        obj->field20 = obj->field1c - 0x10000;
        obj->field40 = 0x1a;
        return mk3_install(thread, (MK3THREADFUNC)t_mhop7);
    }

    if (token == 0x42b) {
        obj->field1c = 4;

        *mk3_frame(thread, frame + 1) = 0x42d;   /* resume token, level above */
        thread->frame = thread->frame + 1;        /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x42a) {
        *mk3_frame(thread, frame + 1) = 0x42b;
        thread->fieldfc = 0xa;
        return 0xa;
    }

    if (token == 0x428) {
        obj->field1c = 4;

        *mk3_frame(thread, frame + 1) = 0x42a;   /* resume token, level above */
        thread->frame = thread->frame + 1;        /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0)
        return -3;

    *mk3_frame(thread, frame + 1) = 0x428;
    thread->fieldfc = 8;
    return 8;
}


/* --------------------------------------------------------------------- t_sk_comboed
 *
 * armv7 0x000a909c, 284 bytes.  **Complete.**
 *
 * Shao Kahn caught in a combo. State 0: `rsnd_func(obj, 0xa)`, push
 * (resume `0x889`) into `t_sk_airborn_check`.
 *
 * `0x889`: sound, `away_x_vel`, pose, push (resume `0x893`) into
 * `other.c`'s `t_animate_a0_frames`.
 *
 * `0x893`: wait 6 under `0x894`. `0x894`: `field1c = 3`, push (resume
 * `0x896`) into `t_mframew`. `0x896`: installs
 * `t_local_reaction_exit`. Any other token: -3.
 */
long t_sk_comboed(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0x896)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

    if (token == 0x894) {
        obj->field1c = 3;

        *mk3_frame(thread, frame + 1) = 0x896;   /* resume token, level above */
        thread->frame = thread->frame + 1;        /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x893) {
        *mk3_frame(thread, frame + 1) = 0x894;
        thread->fieldfc = 6;
        return 6;
    }

    if (token == 0x889) {
        obj->field1c = 6;
        group_sound(obj);
        obj->field1c = 0x30000;
        away_x_vel(obj);
        obj->field40 = 0x1c;
        get_char_ani(obj);
        obj->field1c = 0x30002;

        *mk3_frame(thread, frame + 1) = 0x893;   /* resume token, level above */
        thread->frame = thread->frame + 1;        /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_animate_a0_frames;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0)
        return -3;

    rsnd_func(obj, 0xa);

    *mk3_frame(thread, frame + 1) = 0x889;   /* resume token, level above */
    thread->frame = thread->frame + 1;        /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_sk_airborn_check;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------------------- t_motaro_hit2
 *
 * armv7 0x000a99c4, 296 bytes.  **Complete.**
 *
 * State 0: `am_i_airborn`; airborne installs `t_motaro_hit_flight`.
 * Grounded: sounds (`field1c = 0x30002` for both), `away_x_vel`, pose,
 * push (resume `0x7cc`) into `t_animate_a0_frames`.
 *
 * `0x7cc`: wait 6 under `0x7cd`. `0x7cd`: `field1c = 3`, push (resume
 * `0x7cf`) into `t_mframew`. `0x7cf`: installs
 * `t_local_reaction_exit`. Any other token: -3.
 */
long t_motaro_hit2(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0x7cf)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

    if (token == 0x7cd) {
        obj->field1c = 3;

        *mk3_frame(thread, frame + 1) = 0x7cf;   /* resume token, level above */
        thread->frame = thread->frame + 1;        /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x7cc) {
        *mk3_frame(thread, frame + 1) = 0x7cd;
        thread->fieldfc = 6;
        return 6;
    }

    if (token != 0)
        return -3;

    am_i_airborn(obj);
    if (obj->field5c != 0)
        return mk3_install(thread, (MK3THREADFUNC)t_motaro_hit_flight);

    rsnd_func(obj, 0xa);
    obj->field1c = 0x30002;
    rsnd_ochar_sound(obj);
    obj->field1c = 0x40000;
    away_x_vel(obj);
    obj->field40 = 0x1c;
    get_char_ani(obj);
    obj->field1c = 0x30002;

    *mk3_frame(thread, frame + 1) = 0x7cc;   /* resume token, level above */
    thread->frame = thread->frame + 1;        /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_animate_a0_frames;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------------------- t_motaro_comboed
 *
 * armv7 0x000a91b8, 372 bytes.  **Complete.**
 *
 * State 0: `am_i_airborn`; airborne installs `t_motaro_hit_flight`.
 * Grounded: `rsnd_func(obj, 0xa)`, `away_x_vel`, pose, push (resume
 * `0x7df`) into `t_animate_a0_frames`.
 *
 * `0x7df`: wait 6 under `0x7e0`. `0x7e0`: `field1c = 3`, push (resume
 * `0x7e2`) into `t_mframew`.
 *
 * `0x7e2` reads how many hits the combo landed (`field00->p_hit`,
 * copied into `field1c`). Two or fewer installs
 * `t_local_reaction_exit`. More hands the object's own
 * `field64`/`slave` pair a two-entry table and pushes `t_random_do`
 * under resume token `0x7ee` -- a token this function has no case
 * for, so if `t_random_do` ever pops back here the answer is -3.
 * Whether it can is `mkdrone.c`'s business.
 *
 * Any other token: refused with -3.
 */
extern MK3THREADFUNC funcs_motaro_comboed[];   /* 0x0017b91c */

long t_motaro_comboed(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0x7e2) {
        obj->field1c = obj->field00->p_hit;
        if ((int32_t)obj->field1c <= 2)
            return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

        obj->slave   = (uint32_t)(uintptr_t)funcs_motaro_comboed;
        obj->field64 = 2;

        *mk3_frame(thread, frame + 1) = 0x7ee;   /* resume token, level above */
        thread->frame = thread->frame + 1;        /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_random_do;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x7e0) {
        obj->field1c = 3;

        *mk3_frame(thread, frame + 1) = 0x7e2;   /* resume token, level above */
        thread->frame = thread->frame + 1;        /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x7df) {
        *mk3_frame(thread, frame + 1) = 0x7e0;
        thread->fieldfc = 6;
        return 6;
    }

    if (token != 0)
        return -3;

    am_i_airborn(obj);
    if (obj->field5c != 0)
        return mk3_install(thread, (MK3THREADFUNC)t_motaro_hit_flight);

    rsnd_func(obj, 0xa);
    obj->field1c = 0x10000;
    away_x_vel(obj);
    obj->field40 = 0x1c;
    get_char_ani(obj);
    obj->field1c = 0x30002;

    *mk3_frame(thread, frame + 1) = 0x7df;   /* resume token, level above */
    thread->frame = thread->frame + 1;        /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_animate_a0_frames;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------------------- t_sk_taunt
 *
 * armv7 0x000aabb0, 328 bytes.  **Complete.**
 *
 * Shao Kahn's taunt, a loop. State 0: sound, pose, `a10 = 2`, then
 * the loop head: `field1c = 3`, push (resume `0x3f2`) into `t_mframew`.
 *
 * `0x3f2`: hold for `randu(6) + 10` ticks under `0x3f7` -- the wait
 * is `field1c` itself, stored in `fieldfc` and returned.
 *
 * `0x3f7`: count `a10` down; while it is not zero go round the loop
 * head again. At zero: pose, `find_ani_part2`, `find_part2`,
 * `field1c = 3`, push (resume `0x400`) into `t_mframew`.
 *
 * `0x400`: `randu_minimum` from `0x10` into `a10`, push (resume
 * `0x406`) into `mkdrone.c`'s `t_d_stance_pause`. `0x406`: installs
 * `t_local_reaction_exit`. Any other token: refused with -3.
 */
void randu(MK3OBJ *obj);
void find_part2(MK3OBJ *obj);

long t_sk_taunt(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0x406)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

    if (token == 0x400) {
        obj->field1c = 0x10;
        obj->field20 = 0x10;
        randu_minimum(obj);
        obj->a10 = obj->field1c;

        *mk3_frame(thread, frame + 1) = 0x406;   /* resume token, level above */
        thread->frame = thread->frame + 1;        /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_d_stance_pause;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x3f7) {
        obj->a10 = obj->a10 - 1;
        if (obj->a10 != 0)
            goto loop_head;

        obj->field40 = 0x18;
        find_ani_part2(obj);
        find_part2(obj);
        obj->field1c = 3;

        *mk3_frame(thread, frame + 1) = 0x400;   /* resume token, level above */
        thread->frame = thread->frame + 1;        /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x3f2) {
        obj->field1c = 6;
        randu(obj);
        obj->field1c = obj->field1c + 0xa;
        *mk3_frame(thread, frame + 1) = 0x3f7;
        thread->fieldfc = obj->field1c;
        return (long)(int32_t)obj->field1c;
    }

    if (token != 0)
        return -3;

    obj->field1c = 0x80003;
    rsnd_ochar_sound(obj);
    obj->field40 = 0x18;
    get_char_ani(obj);
    obj->a10 = 2;

loop_head:
    obj->field1c = 3;

    *mk3_frame(thread, frame + 1) = 0x3f2;   /* resume token, level above */
    thread->frame = thread->frame + 1;        /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_mframew;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------------------- t_mc_sg_pounce_sd
 *
 * armv7 0x000aba5c, 232 bytes.  **Complete.**
 *
 * State 0: `bossrandper` at `0x2ee`; a miss, or a hit beyond `0x90`,
 * installs `t_return_to_beware`. Otherwise it measures AGAIN (a second
 * `get_x_dist`, as the binary has it) -- beyond `0x70` installs
 * `t_motaro_kick`, closer sets `a10 = 8` and starts a one-tick loop
 * under `0x6c7`.
 *
 * `0x6c7`: `face_opponent`, count `a10` down; not yet zero re-arms and
 * sleeps 1. At zero, `bossrandper` at `0x1f4` picks
 * `t_motaro_grab_punch` on a hit and `t_motaro_punch` on a miss.
 *
 * Any other token: refused with -3.
 */
long t_mc_sg_pounce_sd(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);
    MK3THREADFUNC handler;

    if (token == 0x6c7) {
        face_opponent(obj);
        obj->a10 = obj->a10 - 1;
        if (obj->a10 != 0)
            goto rearm;

        obj->field1c = 0x1f4;
        bossrandper(obj);
        if (obj->field5c == 0)
            return mk3_install(thread, (MK3THREADFUNC)t_motaro_punch);

        handler = (MK3THREADFUNC)t_motaro_grab_punch;
        goto install;
    }

    if (token != 0)
        return -3;

    obj->field1c = 0x2ee;
    bossrandper(obj);
    if (obj->field5c == 0) {
        handler = (MK3THREADFUNC)t_return_to_beware;
        goto install;
    }

    get_x_dist(obj);
    if ((int32_t)obj->field28 > 0x90) {
        handler = (MK3THREADFUNC)t_return_to_beware;
        goto install;
    }

    get_x_dist(obj);
    if ((int32_t)obj->field28 > 0x70) {
        handler = (MK3THREADFUNC)t_motaro_kick;
        goto install;
    }

    obj->a10 = 8;

rearm:
    *mk3_frame(thread, frame + 1) = 0x6c7;
    thread->fieldfc = 1;
    return 1;

    /* The one store four paths share. Its value is a register the binary
     * loads on the way in, so tools/factdiff.py reads it as `?` on both
     * sides: these four handlers are traced by eye, not by the oracle. */
install:
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)handler;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------------------- t_motaro_tele
 *
 * armv7 0x000ab498, 344 bytes.  **Complete.**
 *
 * Motaro's teleport. State 0: `init_special`, `tsound_func(obj,
 * 0x27)`, `set_nocol`, pose, push (resume `0x1fb`) into `t_mframew`.
 *
 * `0x1fb`: `set_inviso`, then `q_is_he_cornered`. Either way he is
 * moved onto the opponent (`match_me_with_him`) and offset `-0x70` in
 * x (`multi_adjust_xy` with `field1c = -0x70`, `field20 = 0`); a
 * cornered opponent also gets `flip_multi` first, so Motaro lands on
 * the open side. Then wait 3 under `0x212`.
 *
 * `0x212`: `ground_player`, `clear_inviso`, push (resume `0x216`) into
 * `t_mframew`. `0x216`: `clear_nocol`, install
 * `t_local_reaction_exit`. Any other token: refused with -3.
 */
void tsound_func(MK3OBJ *obj, uint32_t which);
void set_inviso(MK3OBJ *obj);
void clear_inviso(MK3OBJ *obj);
void match_me_with_him(MK3OBJ *obj);
void multi_adjust_xy(MK3OBJ *obj);
void ground_player(MK3OBJ *obj);
void flip_multi(MK3OBJ *obj);

long t_motaro_tele(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0x216) {
        clear_nocol(obj);
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }

    if (token == 0x212) {
        ground_player(obj);
        clear_inviso(obj);
        obj->field1c = 3;

        *mk3_frame(thread, frame + 1) = 0x216;   /* resume token, level above */
        thread->frame = thread->frame + 1;        /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x1fb) {
        set_inviso(obj);
        q_is_he_cornered(obj);
        if (obj->field5c != 0) {
            match_me_with_him(obj);
            flip_multi(obj);
            obj->field20 = 0;
            obj->field1c = (uint32_t)-0x70;
            multi_adjust_xy(obj);
        } else {
            match_me_with_him(obj);
            obj->field20 = obj->field5c;   /* 0 */
            obj->field1c = (uint32_t)-0x70;
            multi_adjust_xy(obj);
        }

        *mk3_frame(thread, frame + 1) = 0x212;
        thread->fieldfc = 3;
        return 3;
    }

    if (token != 0)
        return -3;

    init_special(obj);
    tsound_func(obj, 0x27);
    set_nocol(obj);
    obj->field40 = 5;
    get_char_ani(obj);
    obj->field1c = 3;

    *mk3_frame(thread, frame + 1) = 0x1fb;   /* resume token, level above */
    thread->frame = thread->frame + 1;        /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_mframew;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------------------- t_boss_ease_back
 *
 * armv7 0x000aaa44, 364 bytes.  **Complete.**
 *
 * Backing off. State 0: `get_walk_info_b` answers a walk animation in
 * `field24`, a speed in `field20` and an animation rate in `field1c`.
 * The rate is pushed on the thread's argument stack (`mk3_arg`, cursor
 * `fieldf8`) while `field1c` is borrowed for `set_x_vel_player` and the
 * pose is set, then popped back for `init_anirate` -- the only use of
 * that stack in this file. `a10 = 0x60`, then the loop head:
 *
 *     `am_i_facing_him`: facing waits one tick under `0x460`; not
 *     facing installs `mkdrone.c`'s `t_d_turnaround`.
 *
 * `0x460`: `get_x_dist`; beyond `0x100` installs `t_ease5`, otherwise
 * push (resume `0x465`) `joy.c`'s `t_check_winner_status`.
 *
 * `0x465`: `next_anirate`, push (resume `0x467`) `t_d_beware`.
 *
 * `0x467`: `get_x_dist` (its answer unused), count `a10` down; not yet
 * zero goes round the loop head again, zero installs
 * `t_sk_stance_pause`.
 *
 * Any other token: refused with -3.
 */
void get_walk_info_b(MK3OBJ *obj);
void set_x_vel_player(MK3OBJ *obj);

long t_boss_ease_back(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);
    MK3THREADFUNC handler;

    if (token == 0x460) {
        get_x_dist(obj);
        if ((int32_t)obj->field28 > 0x100)
            return mk3_install(thread, (MK3THREADFUNC)t_ease5);

        *mk3_frame(thread, frame + 1) = 0x465;   /* resume token, level above */
        thread->frame = thread->frame + 1;        /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_check_winner_status;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x465) {
        next_anirate(obj);

        *mk3_frame(thread, frame + 1) = 0x467;   /* resume token, level above */
        thread->frame = thread->frame + 1;        /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_d_beware;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x467) {
        get_x_dist(obj);
        obj->a10 = obj->a10 - 1;
        if (obj->a10 != 0)
            goto loop_head;

        handler = (MK3THREADFUNC)t_sk_stance_pause;
        goto install;
    }

    if (token != 0)
        return -3;

    get_walk_info_b(obj);
    *mk3_arg(thread, thread->fieldf8) = obj->field1c;   /* save the rate */
    thread->fieldf8 = thread->fieldf8 + 1;
    obj->field40 = obj->field24;
    obj->field1c = obj->field20;
    set_x_vel_player(obj);
    get_char_ani(obj);
    thread->fieldf8 = thread->fieldf8 - 1;
    obj->field1c = *mk3_arg(thread, thread->fieldf8);   /* and restore it */
    init_anirate(obj);
    obj->a10 = 0x60;

loop_head:
    am_i_facing_him(obj);
    if (obj->field5c != 0) {
        *mk3_frame(thread, frame + 1) = 0x460;
        thread->fieldfc = 1;
        return 1;
    }
    handler = (MK3THREADFUNC)t_d_turnaround;

    /* One store, two paths: the turnaround comes through pointer slot
     * 0x000f3420, the stance pause is a direct address. */
install:
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)handler;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------------------- t_motaro_kick
 *
 * armv7 0x000aa868, 452 bytes.  **Complete.**
 *
 * State 0: pose (`field40 = 0x11`), sound, `am_i_facing_him`. Facing
 * runs `t_mframew` under `0x4d5`; not facing skips straight to the
 * wind-up below. `0x4d5` steps the animation back one strip
 * (`field40 -= 0x10`) and joins it.
 *
 *     wind-up: `find_part2`, `field1c = 2`, `t_mframew` under `0x4dc`.
 *
 * `0x4dc`: `a10 = 4` strike checks, one tick apart under `0x4e3`
 * (`field1c = 1`, `strike_check_a0`). A connection re-arms `0x4fa`
 * and holds 10 ticks; four misses fall into `0x4fa` directly.
 *
 * `0x4fa`: `field1c = 3`, `t_mframew` under `0x4ea`. `0x4ea`: facing
 * goes to `0x4f2`; not facing turns (`field40 = 0x30011`,
 * `t_animate_a9` under `0x4f2`). `0x4f2`: `field40 = 0`,
 * `pose_a9_manual`, install `t_local_reaction_exit`.
 *
 * Any other token: refused with -3.
 */
void pose_a9_manual(MK3OBJ *obj);

long t_motaro_kick(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0x4f2)
        goto finish;

    if (token == 0x4ea) {
        am_i_facing_him(obj);
        if (obj->field5c != 0)
            goto finish;

        obj->field40 = 0x30011;

        *mk3_frame(thread, frame + 1) = 0x4f2;   /* resume token, level above */
        thread->frame = thread->frame + 1;        /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_animate_a9;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x4e3) {
        obj->a10 = obj->a10 - 1;
        if (obj->a10 != 0)
            goto strike;
        goto recover;
    }

    if (token == 0x4fa)
        goto recover;

    if (token == 0x4dc) {
        obj->a10 = 4;
        goto strike;
    }

    if (token == 0x4d5) {
        obj->field40 = obj->field40 - 0x10;
        goto windup;
    }

    if (token != 0)
        return -3;

    obj->field40 = 0x11;
    get_char_ani(obj);
    obj->field1c = 1;
    ochar_sound(obj);
    am_i_facing_him(obj);
    if (obj->field5c == 0)
        goto windup;

    obj->field1c = 1;

    *mk3_frame(thread, frame + 1) = 0x4d5;   /* resume token, level above */
    thread->frame = thread->frame + 1;        /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_mframew;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;

windup:
    find_part2(obj);
    obj->field1c = 2;

    *mk3_frame(thread, frame + 1) = 0x4dc;   /* resume token, level above */
    thread->frame = thread->frame + 1;        /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_mframew;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;

strike:
    obj->field1c = 1;
    strike_check_a0(obj);
    if (obj->field5c == 0) {
        *mk3_frame(thread, frame + 1) = 0x4e3;
        thread->fieldfc = 1;
        return 1;
    }
    *mk3_frame(thread, frame + 1) = 0x4fa;
    thread->fieldfc = 0xa;
    return 0xa;

recover:
    obj->field1c = 3;

    *mk3_frame(thread, frame + 1) = 0x4ea;   /* resume token, level above */
    thread->frame = thread->frame + 1;        /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_mframew;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;

finish:
    obj->field40 = 0;
    pose_a9_manual(obj);
    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* --------------------------------------------------------------------- t_boss_close
 *
 * armv7 0x000ac418, 424 bytes.  **Complete.**
 *
 * Motaro in close. State 0: `a10 = 0xc0`, `q_is_this_a_joke`; if it is
 * and `bossrandper` at `0x1f4` agrees, installs
 * `t_motaro_stupid_stance`. Otherwise `bossrandper` at `0xc8`: a hit
 * pauses (`randu_minimum` from `0x40`, floor `0x10`, into `a10`; push
 * `t_d_stance_pause` under `0x1a6`), a miss goes straight to the stalk.
 *
 *     stalk (and `0x1a6`): `a10 = 0x100`, `field48 = 0xd0`, push
 *     `mkdrone.c`'s `t_d_stalk_a11` under `0x1ac`.
 *
 * `0x1ac`: if a sweep would land and `bossrandper` at `0xc8` agrees,
 * installs `t_motaro_sweep`. Otherwise a closer stalk: `a10 = 0x100`,
 * `field48 = 0x48`, `t_d_stalk_a11` under `0x1b9`.
 *
 * `0x1b9`: opponent grounded installs `t_boss_close_attack`. Airborne,
 * `get_his_action`: action `0x308` installs `t_boss_counter_angle`,
 * anything else `t_boss_wait_land`.
 *
 * Any other token: refused with -3.
 */
long t_d_stalk_a11(struct MK3THREAD *thread);   /* not yet decompiled, mkdrone.c */
void get_his_action(MK3OBJ *obj);

long t_boss_close(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0x1a6)
        goto stalk;

    if (token == 0x1ac) {
        q_ok_motaro_sweep(obj);
        if (obj->field5c != 0) {
            obj->field1c = 0xc8;
            bossrandper(obj);
            if (obj->field5c != 0)
                return mk3_install(thread, (MK3THREADFUNC)t_motaro_sweep);
        }

        obj->a10     = 0x100;
        obj->field48 = 0x100 - 0xb8;

        *mk3_frame(thread, frame + 1) = 0x1b9;   /* resume token, level above */
        thread->frame = thread->frame + 1;        /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_d_stalk_a11;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x1b9) {
        is_he_airborn(obj);
        if (obj->field5c == 0)
            return mk3_install(thread, (MK3THREADFUNC)t_boss_close_attack);

        get_his_action(obj);
        if (obj->field20 == 0x308)
            return mk3_install(thread, (MK3THREADFUNC)t_boss_counter_angle);

        return mk3_install(thread, (MK3THREADFUNC)t_boss_wait_land);
    }

    if (token != 0)
        return -3;

    obj->a10 = 0xc0;
    q_is_this_a_joke(obj);
    if (obj->field5c != 0) {
        obj->field1c = 0x1f4;
        bossrandper(obj);
        if (obj->field5c != 0)
            return mk3_install(thread, (MK3THREADFUNC)t_motaro_stupid_stance);
    }

    obj->field1c = 0xc8;
    bossrandper(obj);
    if (obj->field5c != 0) {
        obj->field1c = 0x40;
        obj->field20 = 0x40 - 0x30;
        randu_minimum(obj);
        obj->a10 = obj->field1c;

        *mk3_frame(thread, frame + 1) = 0x1a6;   /* resume token, level above */
        thread->frame = thread->frame + 1;        /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_d_stance_pause;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

stalk:
    obj->a10     = 0x100;
    obj->field48 = 0x100 - 0x30;

    *mk3_frame(thread, frame + 1) = 0x1ac;   /* resume token, level above */
    thread->frame = thread->frame + 1;        /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_d_stalk_a11;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------------------- t_motaro_grab_punch_now
 *
 * armv7 0x000aa4f0, 736 bytes.  **Complete.**
 *
 * Motaro grabs the opponent and pounds him. Ten states, in order:
 *
 *   0      sound, `me_in_front`, `wfe_him`, `stop_him`, pose
 *          (`field40 = 0x30004`), `t_animate_a9` under `0x518`.
 *   0x518  `sans_repell_3`, `pose_him_a9` (`field40 = 0x20`); the drag
 *          speed `field38` is 0, or `0x30000` if the opponent is
 *          cornered; `away_x_vel` at that speed, `a10 = 6`, then:
 *   0x528  (one tick a pass) keep him in hand -- `match_him_with_me_f`,
 *          offset `-0x50`/`0x10`, `adjust_him_xy`, `sans_repell_3` --
 *          for six passes. Then stop, pose, `field38 = 0x60`, set
 *          `G`'s halfword at `0x456` to `0x60` as well, and push this
 *          file's `t_grab_ani` under `0x543` (offset `0x20`/`-0x10`).
 *   0x543  `t_grab_ani` again, offset `0x23`/`-0x30`, under `0x546`.
 *   0x546  `t_grab_ani` again, offset 0/0, under `0x549`.
 *   0x549  `field1c/20/24 = 4/3/2`, push `other.c`'s `t_shake_him_up`
 *          under `0x54e`.
 *   0x54e  `field38 = t_drop_down_land_jump`'s address (read from pointer
 *          slot 0x000f3424 as data -- the takeover's landing routine),
 *          `takeover_him`, one tick under `0x552`.
 *   0x552  `away_x_vel_him`, pose, `field54 = 3`, `find_ani_part_a14`,
 *          `me_in_front`, `t_mframew` under `0x55b`.
 *   0x55b  `strike_check_a0` (`field1c = 0`), `t_mframew` under `0x560`.
 *   0x560  installs `t_local_reaction_exit`.
 *
 * Any other token: refused with -3.
 */
void wfe_him(MK3OBJ *obj);
void stop_him(MK3OBJ *obj);
void sans_repell_3(MK3OBJ *obj);
void pose_him_a9(MK3OBJ *obj);
void match_him_with_me_f(MK3OBJ *obj);
void away_x_vel_him(MK3OBJ *obj);
void find_ani_part_a14(MK3OBJ *obj);
void takeover_him(MK3OBJ *obj);
long t_shake_him_up(struct MK3THREAD *thread);   /* pointer slot 0x000f36a4, other.c */

long t_motaro_grab_punch_now(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    switch (token) {
    case 0:
        obj->field1c = 2;
        ochar_sound(obj);
        me_in_front(obj);
        wfe_him(obj);
        stop_him(obj);
        obj->field40 = 0x30004;

        *mk3_frame(thread, frame + 1) = 0x518;   /* resume token, level above */
        thread->frame = thread->frame + 1;        /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_animate_a9;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;

    case 0x518:
        sans_repell_3(obj);
        obj->field40 = 0x20;
        pose_him_a9(obj);
        obj->field38 = 0;
        q_is_he_cornered(obj);
        if (obj->field5c != 0)
            obj->field38 = 0x30000;
        obj->field1c = obj->field38;
        away_x_vel(obj);
        obj->a10 = 6;
        goto hold;

    case 0x528:
        match_him_with_me_f(obj);
        obj->field1c = (uint32_t)~0x4f;
        obj->field20 = (uint32_t)~0x4f + 0x60;
        adjust_him_xy(obj);
        sans_repell_3(obj);
        obj->a10 = obj->a10 - 1;
        if (obj->a10 != 0)
            goto hold;

        stop_me_player(obj);
        obj->field40 = 4;
        find_ani_part2(obj);
        obj->field38 = 0x60;
        *(uint16_t *)(G_BYTES + 0x456) = 0x60;
        obj->field1c = 0x20;
        obj->field20 = 0x20 - 0x30;

        *mk3_frame(thread, frame + 1) = 0x543;   /* resume token, level above */
        thread->frame = thread->frame + 1;        /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_grab_ani;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;

    case 0x543:
        obj->field1c = 0x23;
        obj->field20 = 0x23 - 0x53;

        *mk3_frame(thread, frame + 1) = 0x546;   /* resume token, level above */
        thread->frame = thread->frame + 1;        /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_grab_ani;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;

    case 0x546:
        obj->field1c = 0;
        obj->field20 = 0;

        *mk3_frame(thread, frame + 1) = 0x549;   /* resume token, level above */
        thread->frame = thread->frame + 1;        /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_grab_ani;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;

    case 0x549:
        obj->field1c = 4;
        obj->field20 = 3;
        obj->field24 = 2;

        *mk3_frame(thread, frame + 1) = 0x54e;   /* resume token, level above */
        thread->frame = thread->frame + 1;        /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_shake_him_up;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;

    case 0x54e:
        obj->field38 = (uint32_t)(uintptr_t)t_drop_down_land_jump;
        takeover_him(obj);
        *mk3_frame(thread, frame + 1) = 0x552;
        thread->fieldfc = 1;
        return 1;

    case 0x552:
        obj->field1c = 0x20000;
        away_x_vel_him(obj);
        obj->field40 = 4;
        obj->field54 = 3;
        find_ani_part_a14(obj);
        me_in_front(obj);
        obj->field1c = 2;

        *mk3_frame(thread, frame + 1) = 0x55b;   /* resume token, level above */
        thread->frame = thread->frame + 1;        /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;

    case 0x55b:
        obj->field1c = 0;
        strike_check_a0(obj);
        obj->field1c = 5;

        *mk3_frame(thread, frame + 1) = 0x560;   /* resume token, level above */
        thread->frame = thread->frame + 1;        /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;

    case 0x560:
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

    default:
        return -3;
    }

hold:
    *mk3_frame(thread, frame + 1) = 0x528;
    thread->fieldfc = 1;
    return 1;
}


/* --------------------------------------------------------------------- t_sk_stalk
 *
 * armv7 0x000ac134, 740 bytes.  **Complete.**
 *
 * Shao Kahn's stalk -- the routine that decides most of what he does.
 * State 0: `q_boss_stupid` answers yes and he installs `t_sk_stupid`.
 * Otherwise, by distance (`get_x_dist`):
 *
 *     <= 0x5f   close:  see `0x24b`
 *     <= 0x8f          see `0x285`
 *     <= 0xdf          see `0x260`
 *     farther          `bossrandper` at `0xc8`: `t_sk_zap`, or stalk
 *                      in (`a10 = 0x100`, `field48 = 0xe0`,
 *                      `t_d_stalk_a11` under `0x238`)
 *
 * The stalk states step closer: `0x238` rolls `0x1f4` -- a hit goes to
 * `0x260`, a miss stalks again (`field48 = 0x90`, under `0x242`);
 * `0x242` rolls `0x1f4` -- a hit goes to `0x285`, a miss stalks again
 * (`field48 = 0x60`, under `0x24b`).
 *
 * The rest pick a move table for `t_random_do` in the object's own
 * `field64`/`slave` pair:
 *
 *   `0x24b`  cornered and reacting (`q_is_he_car`) and `bossrandper_org`
 *            at `0x2bc` agrees: `t_boss_ease_back`. Otherwise
 *            `funcs_sk_stalk_close` (4 entries), resume `0x260`.
 *   `0x260`  `field1c` still non-zero goes to `0x272`; otherwise
 *            `funcs_sk_stalk_mid` (4), resume `0x272`.
 *   `0x272`  `funcs_sk_stalk_far` (9), resume `0x285`.
 *   `0x285`  `field1c` non-zero goes to `0x297`; otherwise
 *            `funcs_sk_stalk_near` (4), resume `0x297`.
 *   `0x297`  `funcs_sk_stalk_last` (7), resume `0x2a7` -- a token with
 *            no case here, so a return lands on -3, as in
 *            `t_motaro_comboed`.
 *
 * The `field1c` tests read whatever the last odds roll left there; the
 * binary tests it, so this does too. Any other token: refused with -3.
 */
long t_sk_stupid(struct MK3THREAD *thread);
extern MK3THREADFUNC funcs_sk_stalk_close[];   /* 0x0017b9a0, funcs.5315 */
extern MK3THREADFUNC funcs_sk_stalk_mid[];     /* 0x0017b990, funcs.5319 */
extern MK3THREADFUNC funcs_sk_stalk_far[];     /* 0x0017b96c, funcs.5322 */
extern MK3THREADFUNC funcs_sk_stalk_near[];    /* 0x0017b95c, funcs.5326 */
extern MK3THREADFUNC funcs_sk_stalk_last[];    /* 0x0017b940, funcs.5329 */

long t_sk_stalk(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    switch (token) {
    case 0:     break;
    case 0x238: goto s238;
    case 0x242: goto s242;
    case 0x24b: goto s24b;
    case 0x260: goto s260;
    case 0x272: goto s272;
    case 0x285: goto s285;
    case 0x297: goto s297;
    default:    return -3;
    }

    q_boss_stupid(obj);
    if (obj->field5c != 0)
        return mk3_install(thread, (MK3THREADFUNC)t_sk_stupid);

    get_x_dist(obj);
    if ((int32_t)obj->field28 <= 0x5f)
        goto s24b;
    if ((int32_t)obj->field28 <= 0x8f)
        goto s285;
    if ((int32_t)obj->field28 <= 0xdf)
        goto s260;

    obj->field1c = 0xc8;
    bossrandper(obj);
    if (obj->field5c != 0)
        return mk3_install(thread, (MK3THREADFUNC)t_sk_zap);

    obj->a10     = 0x100;
    obj->field48 = 0x100 - 0x20;
    *mk3_frame(thread, frame + 1) = 0x238;   /* resume token, level above */
    thread->frame = thread->frame + 1;        /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_d_stalk_a11;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;

s238:
    obj->field1c = 0x1f4;
    bossrandper(obj);
    if (obj->field5c != 0)
        goto s260;
    obj->a10     = 0x100;
    obj->field48 = 0x100 - 0x70;
    *mk3_frame(thread, frame + 1) = 0x242;   /* resume token, level above */
    thread->frame = thread->frame + 1;        /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_d_stalk_a11;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;

s242:
    obj->field1c = 0x1f4;
    bossrandper(obj);
    if (obj->field5c != 0)
        goto s285;
    obj->a10     = 0x100;
    obj->field48 = 0x100 - 0xa0;
    *mk3_frame(thread, frame + 1) = 0x24b;   /* resume token, level above */
    thread->frame = thread->frame + 1;        /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_d_stalk_a11;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;

s24b:
    q_is_he_car(obj);
    if (obj->field5c != 0) {
        obj->field1c = 0x2bc;
        bossrandper_org(obj);
        if (obj->field5c != 0)
            return mk3_install(thread, (MK3THREADFUNC)t_boss_ease_back);
    }
    obj->slave   = (uint32_t)(uintptr_t)funcs_sk_stalk_close;
    obj->field64 = 4;
    *mk3_frame(thread, frame + 1) = 0x260;   /* resume token, level above */
    thread->frame = thread->frame + 1;        /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_random_do;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;

s260:
    if (obj->field1c != 0)
        goto s272;
    obj->slave   = (uint32_t)(uintptr_t)funcs_sk_stalk_mid;
    obj->field64 = 4;
    *mk3_frame(thread, frame + 1) = 0x272;   /* resume token, level above */
    thread->frame = thread->frame + 1;        /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_random_do;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;

s272:
    obj->slave   = (uint32_t)(uintptr_t)funcs_sk_stalk_far;
    obj->field64 = 9;
    *mk3_frame(thread, frame + 1) = 0x285;   /* resume token, level above */
    thread->frame = thread->frame + 1;        /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_random_do;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;

s285:
    if (obj->field1c != 0)
        goto s297;
    obj->slave   = (uint32_t)(uintptr_t)funcs_sk_stalk_near;
    obj->field64 = 4;
    *mk3_frame(thread, frame + 1) = 0x297;   /* resume token, level above */
    thread->frame = thread->frame + 1;        /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_random_do;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;

s297:
    obj->slave   = (uint32_t)(uintptr_t)funcs_sk_stalk_last;
    obj->field64 = 7;
    *mk3_frame(thread, frame + 1) = 0x2a7;   /* resume token, level above */
    thread->frame = thread->frame + 1;        /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_random_do;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;

}
