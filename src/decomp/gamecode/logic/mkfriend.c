/*
 * mkfriend.c -- gamecode/logic/mkfriend.c, decompiled.
 *
 * The Friendships: each character's own non-lethal finishing move, plus the
 * shared machinery that drives one to completion (the pause, the popup, the
 * closing fade) and hands control back to the ordinary post-round flow.
 *
 * Every function here is read one at a time from `tools/dumpfn.py`'s
 * disassembly, verified against `TOOLS/armrecomp/recomp.py`'s recompiled
 * oracle through `tools/landfn.sh`, and not counted as landed until that
 * passes.
 */

#include "mk3logic.h"

void ochar_sound(MK3OBJ *obj);


/* --------------------------------------------------------------------- camp_fire_sound
 *
 * armv7 0x000a5d8c, 16 bytes.  **Complete.**
 *
 * A one-line wrapper: sound index 10, played through `ochar_sound`. Small
 * enough that the name is the whole specification.
 */
void camp_fire_sound(MK3OBJ *obj)
{
    obj->field1c = 0xa;
    ochar_sound(obj);
}


/* --------------------------------------------------------------------- other_ochar_sound
 *
 * armv7 0x000a5efc, 24 bytes.  **Complete.**
 *
 * `ochar_sound` reads its sound index off `obj`, not off the part -- so to
 * play a sound as though it came from the PART instead, this swaps
 * `field08->field24` for `obj->field20`, calls it, and puts the part's own
 * value back. A callee-saved swap around one call, the same shape
 * `t_kissani`/`create_fx_for_him` use for a different field pair.
 */
void other_ochar_sound(MK3OBJ *obj)
{
    uint32_t saved = obj->field08->field24;

    obj->field08->field24 = obj->field20;
    ochar_sound(obj);
    obj->field08->field24 = saved;
}


/* --------------------------------------------------------------------- t_friendship_complete
 *
 * armv7 0x000a5990, 76 bytes.  **Complete.**
 *
 * State 0 only: `death_blow_complete` and `player_normpal` -- the same
 * pair a fatality's own close-out calls -- then installs `t_wait_forever`
 * on the current level. The friendship ends the same way a fatality does:
 * clean up, restore the palette, and park.
 */
void death_blow_complete(MK3OBJ *obj);
void player_normpal(MK3OBJ *obj);
long t_wait_forever(struct MK3THREAD *thread);

long t_friendship_complete(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    death_blow_complete(obj);
    player_normpal(obj);

    return mk3_install(thread, (MK3THREADFUNC)t_wait_forever);
}


/* --------------------------------------------------------------------- t_hat_proc
 *
 * armv7 0x000a662c, 108 bytes.  **Complete.**
 *
 * State 0 only. Tags the GrObj's own `field2c` (`0x1b39`), repositions
 * (`multi_adjust_xy` at `field1c=0x60`/`field20=0x10`), throws
 * (`field08->field1c = field1c = 0xfffc0000 + 0xe0000 = 0xa0000`,
 * `set_proj_vel`), and installs `t_wait_forever` -- a thrown hat left to
 * fly off and never come back, the same parking convention
 * `t_friendship_complete` uses to end the whole move.
 */
void multi_adjust_xy(MK3OBJ *obj);
void set_proj_vel(MK3OBJ *obj);

long t_hat_proc(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field08->field2c = 0x1b39;

    obj->field1c = 0x60;
    obj->field20 = 0x60 - 0x50;
    multi_adjust_xy(obj);

    obj->field08->field1c = 0xfffc0000;
    obj->field1c           = 0xfffc0000 + 0xe0000;
    set_proj_vel(obj);

    return mk3_install(thread, (MK3THREADFUNC)t_wait_forever);
}


/* --------------------------------------------------------------------- t_end_friend_proc
 *
 * armv7 0x000a5808, 80 bytes.  **Complete.**
 *
 * The free arms token `0x240` and sleeps `0x80` (128) ticks. `0x240` calls
 * `death_blow_complete` and parks token `0x242` under `0x16462` -- the
 * "park and never wake again" sentinel `mk3.c` documents at length: it is
 * checked against the RETURN VALUE, not read as a duration, so a handler
 * that returns it is finished for good regardless of what it wrote to
 * `fieldfc`.
 */
long t_end_friend_proc(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);

    if (slot == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x240;
        thread->fieldfc = 0x80;
        return 0x80;
    }

    if (slot != 0x240)
        return -3;

    death_blow_complete(obj);

    *mk3_frame(thread, thread->frame + 1) = 0x242;
    thread->fieldfc = 0x16462;
    return 0x16462;
}


/* --------------------------------------------------------------------- t_friend_ender
 *
 * armv7 0x000a58b0, 80 bytes.  **Complete.**
 *
 * `t_end_friend_proc`'s own twin, byte for byte the same shape with
 * different tokens: arms `0x130` and sleeps 128 ticks, then
 * `death_blow_complete` and parks `0x132` under the same `0x16462`
 * termination sentinel.
 */
long t_friend_ender(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);

    if (slot == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x130;
        thread->fieldfc = 0x80;
        return 0x80;
    }

    if (slot != 0x130)
        return -3;

    death_blow_complete(obj);

    *mk3_frame(thread, thread->frame + 1) = 0x132;
    thread->fieldfc = 0x16462;
    return 0x16462;
}


/* --------------------------------------------------------------------- t_mframew_3, _4, _5
 *
 * armv7 0x000a5584/0xa5614/0xa56a0, 144/140/144 bytes.  **Complete.**
 *
 * Three copies of the same shape, differing only in the animation-rate
 * constant they pose (3, 4, 5) and their own dispatch tokens. The free
 * poses `field1c = N` and pushes `t_mframew` under a token; that token's
 * own re-entry is the ordinary tail this whole file's dispatchers share
 * with `mkzap.c`'s: pop a level, or install `t_local_reaction_exit` at
 * the bottom.
 */
long t_local_reaction_exit(struct MK3THREAD *thread);
long t_mframew(struct MK3THREAD *thread);

long t_mframew_3(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);

    if (slot == 0) {
        obj->field1c = 3;

        *mk3_frame(thread, thread->frame + 1) = 0x7ca;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot != 0x7ca)
        return -3;

    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;   /* back up a level */
        return 0;
    }

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

long t_mframew_4(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);

    if (slot == 0) {
        obj->field1c = 4;

        *mk3_frame(thread, thread->frame + 1) = 0x7d0;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot != 0x7d0)
        return -3;

    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;   /* back up a level */
        return 0;
    }

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

long t_mframew_5(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);

    if (slot == 0) {
        obj->field1c = 5;

        *mk3_frame(thread, thread->frame + 1) = 0x7d7;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot != 0x7d7)
        return -3;

    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;   /* back up a level */
        return 0;
    }

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* --------------------------------------------------------------------- t_f_kano
 *
 * armv7 0x000a5418, 124 bytes.  **Complete.**
 *
 * Kano's own friendship: no stack frame at all (leaf-shaped, like every
 * other `t_f_*` character entry in this file), so `thread` stays in `r0`
 * throughout rather than being saved. The free points `field40` at the
 * animation stream `a_kano_friend` and pushes `t_mframew_5` under `0x1e0`;
 * `0x1e0` installs `t_friendship_complete` directly.
 */
extern uint8_t a_kano_friend[];             /* 0x001778e8 */

long t_f_kano(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);

    if (slot == 0) {
        obj->field40 = (uint32_t)(uintptr_t)a_kano_friend;

        *mk3_frame(thread, thread->frame + 1) = 0x1e0;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew_5;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot != 0x1e0)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_friendship_complete);
}


/* --------------------------------------------------------------------- t_f_scorpion
 *
 * armv7 0x000a539c, 124 bytes.  **Complete.**
 *
 * Scorpion's own friendship. The free just pushes `t_jax_n_box_start`
 * under token `0xf1` -- no `field40` setup here, unlike Kano; whatever
 * animation stream this move plays is `t_jax_n_box_start`'s own job to
 * set. `0xf1` tags `field30 = 0x9a0`, points `field40` at
 * `a_skull_in_da_box`, and installs `t_pop_up_my_toy` -- the shared
 * "spawn a toy and hijack the opponent's reaction" routine this
 * friendship reuses rather than duplicates.
 */
extern uint8_t a_skull_in_da_box[];         /* 0x00177834 */
long t_jax_n_box_start(struct MK3THREAD *thread);
long t_pop_up_my_toy(MK3THREAD *thread);

long t_f_scorpion(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);

    if (slot == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0xf1;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_jax_n_box_start;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot != 0xf1)
        return -3;

    obj->field30 = 0x9a0;
    obj->field40 = (uint32_t)(uintptr_t)a_skull_in_da_box;

    return mk3_install(thread, (MK3THREADFUNC)t_pop_up_my_toy);
}


/* --------------------------------------------------------------------- t_jax_n_box_start
 *
 * armv7 0x000a6d6c, 112 bytes.  **Complete.**
 *
 * `t_f_scorpion`'s own push target -- Jax's crank-box friendship. The
 * free calls `center_around_me`, points `field40` at the animation
 * stream `a_crank_box`, steps a frame, and waits 16 ticks under `0xea`.
 * `0xea` just poses `field1c=6` and installs `t_mframew` on the current
 * level.
 */
void center_around_me(MK3OBJ *obj);
long do_next_a9_frame(MK3OBJ *obj);
long t_mframew(struct MK3THREAD *thread);
extern uint8_t a_crank_box[];               /* 0x00177864 */

long t_jax_n_box_start(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);

    if (slot == 0) {
        center_around_me(obj);
        obj->field40 = (uint32_t)(uintptr_t)a_crank_box;

        do_next_a9_frame(obj);

        *mk3_frame(thread, thread->frame + 1) = 0xea;
        thread->fieldfc = 0x10;
        return 0x10;
    }

    if (slot != 0xea)
        return -3;

    obj->field1c = 6;

    return mk3_install(thread, (MK3THREADFUNC)t_mframew);
}


/* --------------------------------------------------------------------- t_swat_friend_proc
 *
 * armv7 0x000a57b4, 84 bytes.  **Complete.**
 *
 * The same `t_end_friend_proc` shape: arms `0x34f` and sleeps `0xa0`
 * (160) ticks, then `death_blow_complete` and parks `0x351` under the
 * `0x16462` termination sentinel.
 */
long t_swat_friend_proc(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);

    if (slot == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x34f;
        thread->fieldfc = 0xa0;
        return 0xa0;
    }

    if (slot != 0x34f)
        return -3;

    death_blow_complete(obj);

    *mk3_frame(thread, thread->frame + 1) = 0x351;
    thread->fieldfc = 0x16462;
    return 0x16462;
}


/* --------------------------------------------------------------------- t_f_null_friendship
 *
 * armv7 0x000a5858, 88 bytes.  **Complete.**
 *
 * The character with no real friendship animation: arms `0x155` and
 * sleeps `0x80` (128) ticks, then `death_blow_complete` and installs
 * `t_friendship_complete` directly -- no wait state of its own, since
 * there was never anything to animate.
 */
long t_f_null_friendship(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);

    if (slot == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x155;
        thread->fieldfc = 0x80;
        return 0x80;
    }

    if (slot != 0x155)
        return -3;

    death_blow_complete(obj);

    return mk3_install(thread, (MK3THREADFUNC)t_friendship_complete);
}


/* --------------------------------------------------------------------- t_pop_up_my_toy
 *
 * armv7 0x000a7174, 124 bytes.  **Complete.**
 *
 * State 0 only. Spawns a `t_popup` thread (`NewThreadProc`, return value
 * used), hands the new object's part `field2c` this object's own
 * `field30`, plays sound `0x92` through `tsound_func`, shakes
 * (`field48=0x60006`), sets `field38 = t_r_scared_of_monkey` -- a
 * reaction handler, not a hit callback -- and `takeover_him`s the
 * opponent with it before installing `t_wait_forever`. The toy pops up
 * and the opponent's own reaction gets hijacked to react to it.
 */
long t_popup(struct MK3THREAD *thread);
void *NewThreadProc(void *owner, MK3THREADFUNC func);
void tsound_func(MK3OBJ *obj, uint32_t arg);
void shake_a11(MK3OBJ *obj);
void takeover_him(MK3OBJ *obj);
long t_r_scared_of_monkey(struct MK3THREAD *thread);

long t_pop_up_my_toy(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    MK3OBJ *toy;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    toy = (MK3OBJ *)NewThreadProc(obj, (MK3THREADFUNC)t_popup);
    toy->field08->field2c = obj->field30;

    tsound_func(obj, 0x92);

    obj->field48 = 0x60006;
    shake_a11(obj);

    obj->field38 = (uint32_t)(uintptr_t)t_r_scared_of_monkey;
    takeover_him(obj);

    return mk3_install(thread, (MK3THREADFUNC)t_wait_forever);
}


/* --------------------------------------------------------------------- t_do_friendship
 *
 * armv7 0x000a5730, 132 bytes.  **Complete.**
 *
 * The dispatcher every `t_f_*` character entry in this file is reached
 * from: the free pushes `t_friendship_start_pause` under `0x805`; `0x805`
 * calls `init_special`, reads the fighter's own character id
 * (`field08->field24`), looks it up in `ochar_friendships` -- a
 * four-byte-stride table of function pointers, one per character -- and
 * installs whatever it finds. `obj->field1c` gets the same handler value
 * too, alongside the install; nothing downstream reads `field1c` for it,
 * so it looks like the field this file's other routines pack a table
 * pointer into (`field40`) done here with the wrong offset, or a leftover
 * from an earlier draft of the lookup. Transcribed as found.
 */
void init_special(MK3OBJ *obj);
long t_friendship_start_pause(struct MK3THREAD *thread);
extern MK3THREADFUNC ochar_friendships[];   /* 0x00177f24 */

long t_do_friendship(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);

    if (slot == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x805;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_friendship_start_pause;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot != 0x805)
        return -3;

    init_special(obj);

    {
        MK3THREADFUNC handler = ochar_friendships[obj->field08->field24];
        obj->field1c = (uint32_t)(uintptr_t)handler;
        return mk3_install(thread, handler);
    }
}


/* --------------------------------------------------------------------- t_f_reptile
 *
 * armv7 0x000a70ec, 136 bytes.  **Complete.**
 *
 * The free clears invisibility and pushes `t_jax_n_box_start` under
 * `0xfa` -- Reptile's friendship reuses Jax's crank-box thread outright,
 * the same way `t_f_scorpion` does. `0xfa` tags `field30=0x1433`, points
 * `field40` at `a_snake_in_da_box`, and installs `t_pop_up_my_toy`.
 */
void clear_inviso(MK3OBJ *obj);
extern uint8_t a_snake_in_da_box[];         /* 0x001778a4 */

long t_f_reptile(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);

    if (slot == 0) {
        clear_inviso(obj);

        *mk3_frame(thread, thread->frame + 1) = 0xfa;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_jax_n_box_start;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot != 0xfa)
        return -3;

    obj->field30 = 0x1433;
    obj->field40 = (uint32_t)(uintptr_t)a_snake_in_da_box;

    return mk3_install(thread, (MK3THREADFUNC)t_pop_up_my_toy);
}


/* --------------------------------------------------------------------- t_f_kabal
 *
 * armv7 0x000a5b04, 140 bytes.  **Complete.**
 *
 * The free spawns a SEPARATE `t_end_friend_proc` thread (`NewThread`,
 * return value discarded -- unlike `t_pop_up_my_toy`'s own spawn, this
 * one isn't kept), points `field40` at `a_tusk_friend`, and pushes
 * `t_mframew_5` under `0x4f2`, whose own re-entry installs
 * `t_friendship_complete` directly.
 */
MK3THREAD *NewThread(void *owner, MK3THREADFUNC func);
extern uint8_t a_tusk_friend[];             /* 0x00177d2c */

long t_f_kabal(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);

    if (slot == 0) {
        NewThread(obj, (MK3THREADFUNC)t_end_friend_proc);
        obj->field40 = (uint32_t)(uintptr_t)a_tusk_friend;

        *mk3_frame(thread, thread->frame + 1) = 0x4f2;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew_5;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot != 0x4f2)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_friendship_complete);
}


/* --------------------------------------------------------------------- t_f_cyrax
 *
 * armv7 0x000a5b90, 140 bytes.  **Complete.**
 *
 * `t_f_kabal`'s own twin, the same shape with different constants:
 * spawns a separate `t_end_friend_proc` thread, points `field40` at
 * `a_robo2_friend`, and pushes `t_mframew_5` under `0x427`, whose
 * re-entry also installs `t_friendship_complete` directly.
 */
extern uint8_t a_robo2_friend[];            /* 0x00177ca0 */

long t_f_cyrax(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);

    if (slot == 0) {
        NewThread(obj, (MK3THREADFUNC)t_end_friend_proc);
        obj->field40 = (uint32_t)(uintptr_t)a_robo2_friend;

        *mk3_frame(thread, thread->frame + 1) = 0x427;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew_5;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot != 0x427)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_friendship_complete);
}


/* --------------------------------------------------------------------- t_f_sz
 *
 * armv7 0x000a5e78, 132 bytes.  **Complete.**
 *
 * The free poses `field1c=0`, points `field40` at `a_sz_friend`, plays
 * `ochar_sound`, and pushes `t_mframew_5` under `0x2d3`, whose re-entry
 * installs `t_friendship_complete` directly.
 */
extern uint8_t a_sz_friend[];                /* 0x00177a7c */

long t_f_sz(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);

    if (slot == 0) {
        obj->field1c = 0;
        obj->field40 = (uint32_t)(uintptr_t)a_sz_friend;
        ochar_sound(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x2d3;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew_5;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot != 0x2d3)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_friendship_complete);
}


/* --------------------------------------------------------------------- t_football_proc
 *
 * armv7 0x000a68f8, 132 bytes.  **Complete.**
 *
 * The free reads the GrObj's own flip bit (`field08->field28 & 0x10`,
 * saved whole into `field2c`) and throws `0xa0000`, or `0xfff60000` when
 * flipped, into `field08->field18`; either way `field1c`/`field08->field1c`
 * both then get the same `0xfff60000`, `field40` points at `a_football`,
 * and `find_part2`/`init_anirate` (`field1c=2`) run before a one-tick
 * wait under `0x3b5`. `0x3b5` is a bare `next_anirate` self-loop -- no
 * further field setup, just keep animating while the kick plays out.
 */
extern uint8_t a_football[];                /* 0x00177ba4 */
void find_part2(MK3OBJ *obj);
void init_anirate(MK3OBJ *obj);
long next_anirate(MK3OBJ *obj);

long t_football_proc(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);

    if (slot == 0) {
        obj->field1c = 0xa0000;

        obj->field2c = obj->field08->field28;
        if (obj->field2c & 0x10)
            obj->field1c = 0xfff60000;

        obj->field08->field18 = obj->field1c;

        obj->field1c           = 0xfff60000;
        obj->field08->field1c  = 0xfff60000;
        obj->field40            = (uint32_t)(uintptr_t)a_football;
        find_part2(obj);

        obj->field1c = 2;
        init_anirate(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x3b5;
        thread->fieldfc = 1;
        return 1;
    }

    if (slot != 0x3b5)
        return -3;

    next_anirate(obj);

    *mk3_frame(thread, thread->frame + 1) = 0x3b5;
    thread->fieldfc = 1;
    return 1;
}


/* --------------------------------------------------------------------- t_f_jax
 *
 * armv7 0x000a6734, 152 bytes.  **Complete.**
 *
 * The free spawns a separate `t_end_friend_proc` thread, points
 * `field40` at `a_jax_friend`, steps a frame, and waits 32 ticks under
 * `0x23a`. `0x23a` pushes `t_mframew_3` under `0x23b`, whose re-entry
 * installs `t_friendship_complete` directly.
 */
extern uint8_t a_jax_friend[];               /* 0x00177a0c */
long t_mframew_3(struct MK3THREAD *thread);

long t_f_jax(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);

    if (slot == 0) {
        NewThread(obj, (MK3THREADFUNC)t_end_friend_proc);
        obj->field40 = (uint32_t)(uintptr_t)a_jax_friend;

        do_next_a9_frame(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x23a;
        thread->fieldfc = 0x20;
        return 0x20;
    }

    if (slot == 0x23a) {
        *mk3_frame(thread, thread->frame + 1) = 0x23b;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew_3;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot != 0x23b)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_friendship_complete);
}


/* --------------------------------------------------------------------- t_f_sektor
 *
 * armv7 0x000a6698, 156 bytes.  **Complete.**
 *
 * The same shape as `t_f_jax`: the free spawns a separate `t_dinger_proc`
 * thread, points `field40` at `a_robo1_friend`, steps a frame, and waits
 * 80 ticks under `0x410`. `0x410` pushes `t_mframew_5` under `0x411`,
 * whose re-entry installs `t_friendship_complete` directly.
 */
extern uint8_t a_robo1_friend[];             /* 0x00177c88 */
long t_dinger_proc(struct MK3THREAD *thread);

long t_f_sektor(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);

    if (slot == 0) {
        NewThread(obj, (MK3THREADFUNC)t_dinger_proc);
        obj->field40 = (uint32_t)(uintptr_t)a_robo1_friend;

        do_next_a9_frame(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x410;
        thread->fieldfc = 0x50;
        return 0x50;
    }

    if (slot == 0x410) {
        *mk3_frame(thread, thread->frame + 1) = 0x411;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew_5;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot != 0x411)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_friendship_complete);
}


/* --------------------------------------------------------------------- t_lao_dog_sounds
 *
 * armv7 0x000a65b0, 124 bytes.  **Complete.**
 *
 * The free just arms `0x446` and sleeps 8 ticks. `0x446` seeds `a10 = 5`
 * and falls into the SAME "play, re-arm" body `0x44b`'s own "still
 * counting" path reaches -- one physical block taken two ways: play
 * sound `0x8a` (`tsound_func`) and wait 16 ticks under `0x44b` again.
 * `0x44b` counts `a10` down each visit; once it hits zero it stops
 * making noise and parks `0x44f` under the `0x16462` termination
 * sentinel instead.
 */
void tsound_func(MK3OBJ *obj, uint32_t arg);

long t_lao_dog_sounds(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);

    if (slot == 0x446) {
        obj->a10 = 5;
        goto bark;
    }

    if (slot == 0x44b) {
        obj->a10 = obj->a10 - 1;
        if (obj->a10 == 0) {
            *mk3_frame(thread, thread->frame + 1) = 0x44f;
            thread->fieldfc = 0x16462;
            return 0x16462;
        }

    bark:
        tsound_func(obj, 0x8a);

        *mk3_frame(thread, thread->frame + 1) = 0x44b;
        thread->fieldfc = 0x10;
        return 0x10;
    }

    if (slot != 0)
        return -3;

    *mk3_frame(thread, thread->frame + 1) = 0x446;
    thread->fieldfc = 8;
    return 8;
}


/* --------------------------------------------------------------------- t_f_indian
 *
 * armv7 0x000a5f14, 180 bytes.  **Complete.**
 *
 * The free points `field40` at `a_ind_friend`, poses (`field20=0xc`,
 * `field1c=4`), plays `other_ochar_sound` (the part-voiced sound this
 * file's own helper wraps), and pushes `t_mframew_5` under `0x280`.
 * `0x280` just re-arms `0x281` and sleeps 32 ticks -- no field setup of
 * its own. `0x281` spawns a SEPARATE `t_arcade` thread (`NewThread`,
 * return value discarded) and installs `t_wait_forever`: the friendship
 * hands off to `t_arcade` entirely and parks.
 */
void other_ochar_sound(MK3OBJ *obj);
long t_arcade(struct MK3THREAD *thread);
extern uint8_t a_ind_friend[];              /* 0x00177a44 */

long t_f_indian(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);

    if (slot == 0x280) {
        *mk3_frame(thread, thread->frame + 1) = 0x281;
        thread->fieldfc = 0x20;
        return 0x20;
    }

    if (slot == 0x281) {
        NewThread(obj, (MK3THREADFUNC)t_arcade);

        return mk3_install(thread, (MK3THREADFUNC)t_wait_forever);
    }

    if (slot != 0)
        return -3;

    obj->field40 = (uint32_t)(uintptr_t)a_ind_friend;

    obj->field20 = 0xc;
    obj->field1c = 0xc - 8;
    other_ochar_sound(obj);

    *mk3_frame(thread, thread->frame + 1) = 0x280;
    thread->frame = thread->frame + 1;   /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_mframew_5;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------------------- t_f_sonya
 *
 * armv7 0x000a5c1c, 152 bytes.  **Complete.**
 *
 * The free seeds `a10 = 1` and spawns `t_sonya_flower_proc` in a genuine
 * loop counting `a10` down -- one iteration as written, since it starts
 * at 1, but a real loop and not an unrolled call -- then waits 48 ticks
 * under `0x21c`. `0x21c` finishes the same way `t_friendship_complete`
 * does (`death_blow_complete`, `player_normpal`) but pushes
 * `t_victory_animation` instead of installing `t_wait_forever` directly;
 * nothing here handles a resume under the token that push plants
 * (`0x220`), so `t_victory_animation` is expected to end the thread on
 * its own rather than ever pop back.
 */
long t_victory_animation(struct MK3THREAD *thread);
long t_sonya_flower_proc(struct MK3THREAD *thread);

long t_f_sonya(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);

    if (slot == 0) {
        obj->a10 = 1;

        do {
            NewThread(obj, (MK3THREADFUNC)t_sonya_flower_proc);
            obj->a10 = obj->a10 - 1;
        } while ((int32_t)obj->a10 > 0);

        *mk3_frame(thread, thread->frame + 1) = 0x21c;
        thread->fieldfc = 0x30;
        return 0x30;
    }

    if (slot != 0x21c)
        return -3;

    death_blow_complete(obj);
    player_normpal(obj);

    *mk3_frame(thread, thread->frame + 1) = 0x220;
    thread->frame = thread->frame + 1;   /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_victory_animation;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------------------- t_popup
 *
 * armv7 0x000a5900, 144 bytes.  **Complete.**
 *
 * `t_pop_up_my_toy`'s own `NewThreadProc` target -- the popup object's
 * own thread, separate from the fighter's. The free poses `field1c=4`
 * and pushes `t_mframew` under `0xbe`; `0xbe` waits 48 ticks under
 * `0xbf`; `0xbf` calls `death_blow_complete` and installs
 * `t_wait_forever`.
 */
long t_popup(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);

    if (slot == 0xbe) {
        *mk3_frame(thread, thread->frame + 1) = 0xbf;
        thread->fieldfc = 0x30;
        return 0x30;
    }

    if (slot == 0xbf) {
        death_blow_complete(obj);

        return mk3_install(thread, (MK3THREADFUNC)t_wait_forever);
    }

    if (slot != 0)
        return -3;

    obj->field1c = 4;

    *mk3_frame(thread, thread->frame + 1) = 0xbe;
    thread->frame = thread->frame + 1;   /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_mframew;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------------------- t_f_sheeva
 *
 * armv7 0x000a5cb4, 216 bytes.  **Complete.**
 *
 * The free spawns a separate `t_end_friend_proc` thread, points
 * `field40` at `a_sg_friend`, and pushes `t_mframew_5` under `0x515`.
 * `0x515` poses (`field1c=6`), plays `ochar_sound`, and pushes
 * `t_mframew_5` AGAIN under `0x518` -- a second, plain pose-and-wait
 * step rather than a loop, the same handler reused twice with different
 * tokens. `0x518` installs `t_friendship_complete` directly.
 */
extern uint8_t a_sg_friend[];                /* 0x00177dd8 */

long t_f_sheeva(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);

    if (slot == 0x515) {
        obj->field1c = 6;
        ochar_sound(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x518;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew_5;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot == 0x518)
        return mk3_install(thread, (MK3THREADFUNC)t_friendship_complete);

    if (slot != 0)
        return -3;

    NewThread(obj, (MK3THREADFUNC)t_end_friend_proc);
    obj->field40 = (uint32_t)(uintptr_t)a_sg_friend;

    *mk3_frame(thread, thread->frame + 1) = 0x515;
    thread->frame = thread->frame + 1;   /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_mframew_5;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------------------- t_sonya_flower_proc
 *
 * armv7 0x000a6c88, 228 bytes.  **Complete.**
 *
 * `t_f_sonya`'s own `NewThread` target: one flower. The free sleeps a
 * random 20..29 ticks (`randu` answers in `field1c`, the sleep read
 * straight back out of it) under `0x1f6`. `0x1f6` picks the flower's
 * frame (`field08->field2c = 0xe6`), rolls a random x (`randu(0x18f)
 * - 0xc7`, a spread of -199..199 left in `field1c` -- nothing here
 * reads it back, so what consumes it is outside this routine), plants
 * the part on the floor (`G+0xac` minus the frame's own height), rolls a
 * random 3..8 animation rate, points `field40` at `a_flower`, and pushes
 * `t_mframew` under `0x210`, whose re-entry parks on `t_wait_forever`.
 */
void randu(MK3OBJ *obj);
int  GetFrameHeight(uint32_t ani);
extern uint8_t a_flower[];                   /* 0x001779ec */

long t_sonya_flower_proc(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);

    if (slot == 0x1f6) {
        uint32_t floor;

        obj->field08->field2c = 0xe6;

        obj->field1c = 0xe6 + 0xa9;
        randu(obj);
        obj->field1c = obj->field1c - 0xc7;

        floor = *(uint32_t *)(G_BYTES + 0xac);
        MK3_SET_FIELD12(obj->field08,
                        floor - (uint32_t)GetFrameHeight(obj->field08->field2c));

        obj->field1c = 6;
        randu(obj);
        obj->field1c = obj->field1c + 3;

        obj->field40 = (uint32_t)(uintptr_t)a_flower;

        *mk3_frame(thread, thread->frame + 1) = 0x210;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot == 0x210)
        return mk3_install(thread, (MK3THREADFUNC)t_wait_forever);

    if (slot != 0)
        return -3;

    obj->field1c = 0xa;
    randu(obj);
    obj->field1c = obj->field1c + 0x14;

    *mk3_frame(thread, thread->frame + 1) = 0x1f6;
    thread->fieldfc = obj->field1c;
    return (long)obj->field1c;
}


/* --------------------------------------------------------------------- t_cute_lil_doggy
 *
 * armv7 0x000a5494, 240 bytes.  **Complete.**
 *
 * Leaf-shaped, like the `t_f_*` entries. The free sets the part's frame
 * (`field08->field2c = 0x1463`) and picks a side by the part's own flip
 * bit: unflipped starts the dog at `G[0x468] - 0x30` running right at
 * `0x60000`, flipped at `G[0x470] + 0x60` running left at `0xfffa0000`
 * -- the two camera edges `t_blade_proc` averages, so the dog enters
 * from whichever edge is behind the fighter. Both sides then share one
 * tail: x into the part's `field0e` (the low half of `field1c`, a
 * halfword store), y to `G+0xac - 0x20`, the velocity into
 * `field08->field18`, `field40` at `a_dog`, rate 3, and a push of
 * `t_mframew` under `0x4a6`, whose re-entry parks on `t_wait_forever`.
 */
extern uint8_t a_dog[];                      /* 0x00177d0c */

long t_cute_lil_doggy(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);

    if (slot == 0x4a6)
        return mk3_install(thread, (MK3THREADFUNC)t_wait_forever);

    if (slot != 0)
        return -3;

    obj->field08->field2c = 0x1463;

    if (obj->field08->field28 & 0x10) {
        obj->field1c = *(uint32_t *)(G_BYTES + 0x470) + 0x60;
        obj->field30 = 0xfffa0000;
    } else {
        obj->field1c = *(uint32_t *)(G_BYTES + 0x468) - 0x30;
        obj->field30 = 0x60000;
    }

    MK3_SET_FIELD0E(obj->field08, (uint16_t)obj->field1c);
    MK3_SET_FIELD12(obj->field08, *(uint32_t *)(G_BYTES + 0xac) - 0x20);
    obj->field08->field18 = obj->field30;

    obj->field40 = (uint32_t)(uintptr_t)a_dog;
    obj->field1c = 3;

    *mk3_frame(thread, thread->frame + 1) = 0x4a6;
    thread->frame = thread->frame + 1;   /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_mframew;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------------------- t_arcade
 *
 * armv7 0x000a6b94, 244 bytes.  **Complete.**
 *
 * `t_f_indian`'s own `NewThread` target -- Nightwolf's arcade cabinet.
 * The free sets the part's frame (`0x1b36`), places it (`multi_adjust_xy`
 * at `-0x70`/`-0x100`), seeds a fall (`field20 = 0x20000`, `a10 = 0x20000
 * - 0x1a000 = 0x6000`, the velocity/gravity pair it reads) and pushes
 * `mkfatal.c`'s `t_gravity_ani_ysize` under `0x26a`: the cabinet drops
 * in from above. (**Corrected**: this and `t_f_jade` were first landed
 * with their two pointer slots crossed -- 0x000f33f4 is
 * `t_gravity_ani_ysize`, 0x000f3720 is `t_flight`.)
 *
 * `0x26a` is the landing: `shake_n_sound`, then a coin flip on
 * `mk_random`'s bit 8 (`mk_random` answers in `field1c`, and the binary
 * reads the same value straight out of `r0`) posts HUD event 3/7 or 3/8
 * through `MKEvent_Add` -- which of the two "ARCADE" messages shows -- and
 * waits 64 ticks under `0x272`, which calls `death_blow_complete` and
 * parks on `t_wait_forever`.
 */
void shake_n_sound(MK3OBJ *obj);
void mk_random(MK3OBJ *obj);
void MKEvent_Add(long type, long subtype, long param, long player);
long t_gravity_ani_ysize(struct MK3THREAD *thread);   /* pointer slot 0x000f33f4 */

long t_arcade(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);

    if (slot == 0x26a) {
        shake_n_sound(obj);

        mk_random(obj);
        if (obj->field1c & 0x100)
            MKEvent_Add(3, 7, 0, 0);
        else
            MKEvent_Add(3, 8, 0, 0);

        *mk3_frame(thread, thread->frame + 1) = 0x272;
        thread->fieldfc = 0x40;
        return 0x40;
    }

    if (slot == 0x272) {
        death_blow_complete(obj);
        return mk3_install(thread, (MK3THREADFUNC)t_wait_forever);
    }

    if (slot != 0)
        return -3;

    obj->field08->field2c = 0x1b36;

    obj->field1c = (uint32_t)~0x6f;
    obj->field20 = (uint32_t)~0x6f - 0x90;
    multi_adjust_xy(obj);

    obj->field20 = 0x20000;
    obj->a10     = 0x20000 - 0x1a000;

    *mk3_frame(thread, thread->frame + 1) = 0x26a;
    thread->frame = thread->frame + 1;   /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_gravity_ani_ysize;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------------------- t_f_jade
 *
 * armv7 0x000a6ec8, 300 bytes.  **Complete.**
 *
 * The free stops the scrolling (`kill_and_stop_scrolling`), poses
 * animation `0xd`, and pushes `t_animate_a0_frames` at rate/frames
 * `0x40003` under `0x13a`; `0x13a` waits 16 ticks under `0x13b`.
 *
 * `0x13b` spawns a separate `t_friend_ender` thread and parks the
 * current animation cursor two ways -- `field48` as is, `a10` four bytes
 * back (one frame earlier in the stream) -- then falls into `0x14f`'s
 * own body: restore the cursor from `field48`, step a frame, and push
 * `other.c`'s `t_flight` with a flat throw (`field1c=0`, `field20 =
 * 0xfff60000`, `field24 = 0xfff60000 + 0xa9000`, `field28 = 0xfff`)
 * under `0x149`. `0x149` lands: sound `0xc`, cursor back to `a10`, step,
 * and three ticks later under `0x14f` it bounces again -- a loop that
 * only `t_friend_ender` ends.
 */
void kill_and_stop_scrolling(MK3OBJ *obj);
void get_char_ani2(MK3OBJ *obj);
long t_animate_a0_frames(struct MK3THREAD *thread);   /* pointer slot 0x000f36b8 */
long t_flight(struct MK3THREAD *thread);   /* pointer slot 0x000f3720 */

long t_f_jade(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);

    if (slot == 0x13b) {
        NewThread(obj, (MK3THREADFUNC)t_friend_ender);
        obj->field48 = obj->field40;
        obj->a10     = obj->field40 - 4;
        goto bounce;
    }

    if (slot < 0x13b) {
        if (slot == 0) {
            kill_and_stop_scrolling(obj);

            obj->field40 = 0xd;
            get_char_ani2(obj);

            obj->field1c = 0x40003;

            *mk3_frame(thread, thread->frame + 1) = 0x13a;
            thread->frame = thread->frame + 1;   /* push a level */
            mk3_frame(thread, thread->frame)[1] =
                (uint32_t)(uintptr_t)t_animate_a0_frames;
            *mk3_frame(thread, thread->frame + 1) = 0;
            return 0;
        }

        if (slot != 0x13a)
            return -3;

        *mk3_frame(thread, thread->frame + 1) = 0x13b;
        thread->fieldfc = 0x10;
        return 0x10;
    }

    if (slot == 0x149) {
        tsound_func(obj, 0xc);

        obj->field40 = obj->a10;
        do_next_a9_frame(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x14f;
        thread->fieldfc = 3;
        return 3;
    }

    if (slot != 0x14f)
        return -3;

bounce:
    obj->field40 = obj->field48;
    do_next_a9_frame(obj);

    obj->field1c = 0;
    obj->field20 = 0xfff60000;
    obj->field24 = 0xfff60000 + 0xa9000;
    obj->field28 = 0xfff;

    *mk3_frame(thread, thread->frame + 1) = 0x149;
    thread->frame = thread->frame + 1;   /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_flight;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------------------- t_f_kitana
 *
 * armv7 0x000a6ddc, 236 bytes.  **Complete.**
 *
 * The free runs `center_around_me` on the OPPONENT (the A0 handover:
 * the routine goes in `field1c` and `call_a0_for_him` runs it on the
 * other fighter, the same way `face_him_at_me` does), then pushes
 * `t_animate2_a9` at `0x50008` under `0x19c`. `0x19c` waits 112 ticks
 * under `0x19d`; `0x19d` deletes the slave, poses animation 6
 * (`find_ani2_part2`), and pushes `t_mframew` under `0x1a3`, whose
 * re-entry installs `t_friendship_complete`.
 */
void center_around_me(MK3OBJ *obj);
void call_a0_for_him(MK3OBJ *obj);
void delete_slave(MK3OBJ *obj);
void find_ani2_part2(MK3OBJ *obj);
long t_animate2_a9(struct MK3THREAD *thread);   /* pointer slot 0x000f36c0 */

long t_f_kitana(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);

    if (slot == 0x19c) {
        *mk3_frame(thread, thread->frame + 1) = 0x19d;
        thread->fieldfc = 0x70;
        return 0x70;
    }

    if (slot < 0x19c) {
        if (slot != 0)
            return -3;

        obj->field1c = (uint32_t)(uintptr_t)center_around_me;
        call_a0_for_him(obj);

        obj->field40 = 0x50008;

        *mk3_frame(thread, thread->frame + 1) = 0x19c;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_animate2_a9;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot == 0x19d) {
        delete_slave(obj);

        obj->field40 = 6;
        find_ani2_part2(obj);

        obj->field1c = 4;

        *mk3_frame(thread, thread->frame + 1) = 0x1a3;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot != 0x1a3)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_friendship_complete);
}


/* --------------------------------------------------------------------- t_bounce
 *
 * armv7 0x000a6374, 216 bytes.  **Complete.**
 *
 * A bouncing ball. The free rolls a launch speed (`randu_minimum` with
 * both bounds at `0x40000`, answering in `field1c`), negates it into
 * the y velocity (`field48` and the part's own `field1c`), and waits two
 * ticks under `0x524`, which just re-arms `0x526` for one tick.
 *
 * `0x526` is one physics step: velocity into the part, gravity
 * `+0x8000` onto `field48`, and a floor test -- the part's y plus `0x20`
 * against `G+0xac`. Still above it re-arms `0x526` through the SAME
 * one-tick tail `0x524` uses (one physical block, two ways in); reaching
 * it plays sound `0x71` and pops a level, or installs
 * `t_local_reaction_exit` at the bottom.
 */
void randu_minimum(MK3OBJ *obj);

long t_bounce(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);

    if (slot == 0x524)
        goto rearm;

    if (slot == 0x526) {
        obj->field08->field1c = obj->field48;
        obj->field48          = obj->field48 + 0x8000;

        obj->field1c = (uint32_t)((int32_t)MK3_FIELD12_S(obj->field08) + 0x20);
        obj->field20 = *(uint32_t *)(G_BYTES + 0xac);

        if ((int32_t)obj->field20 > (int32_t)obj->field1c)
            goto rearm;

        tsound_func(obj, 0x71);

        if ((long)thread->frame > 0) {
            thread->frame = thread->frame - 1;   /* back up a level */
            return 0;
        }
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }

    if (slot != 0)
        return -3;

    obj->field1c = 0x40000;
    obj->field20 = 0x40000;
    randu_minimum(obj);

    obj->field48          = (uint32_t)(-(int32_t)obj->field1c);
    obj->field08->field1c = obj->field48;

    *mk3_frame(thread, thread->frame + 1) = 0x524;
    thread->fieldfc = 2;
    return 2;

rearm:
    *mk3_frame(thread, thread->frame + 1) = 0x526;
    thread->fieldfc = 1;
    return 1;
}


/* --------------------------------------------------------------------- t_friendship_start_pause
 *
 * armv7 0x000a71f0, 184 bytes.  **Complete.**
 *
 * `t_do_friendship`'s own push target, the pause before the move: the
 * free sets `field20 = 4` and pushes `t_init_death_blow` -- the same
 * setup a fatality starts with -- under `0x63`. `0x63` waits 48 ticks
 * under `0x64`, `0x64` one more under `0x67`, and `0x67` tags
 * `field28 = 0x42`, sends it on with `send_code_a3`, and pops a level
 * (back to `t_do_friendship`, which then dispatches the character), or
 * installs `t_local_reaction_exit` at the bottom.
 */
void send_code_a3(MK3OBJ *obj);
long t_init_death_blow(struct MK3THREAD *thread);   /* pointer slot 0x000f3194 */

long t_friendship_start_pause(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);

    if (slot == 0x64) {
        *mk3_frame(thread, thread->frame + 1) = 0x67;
        thread->fieldfc = 1;
        return 1;
    }

    if (slot < 0x64) {
        if (slot == 0) {
            obj->field20 = 4;

            *mk3_frame(thread, thread->frame + 1) = 0x63;
            thread->frame = thread->frame + 1;   /* push a level */
            mk3_frame(thread, thread->frame)[1] =
                (uint32_t)(uintptr_t)t_init_death_blow;
            *mk3_frame(thread, thread->frame + 1) = 0;
            return 0;
        }

        if (slot != 0x63)
            return -3;

        *mk3_frame(thread, thread->frame + 1) = 0x64;
        thread->fieldfc = 0x30;
        return 0x30;
    }

    if (slot != 0x67)
        return -3;

    obj->field28 = 0x42;
    send_code_a3(obj);

    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;   /* back up a level */
        return 0;
    }
    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* --------------------------------------------------------------------- t_swat_crossing_people
 *
 * armv7 0x000a6a88, 268 bytes.  **Complete.**
 *
 * One pedestrian of Stryker's crossing-guard friendship; `field48` is
 * which one (1-based, set by the spawner). The free marks the part
 * (`field08->field48 = 1`), waits a random 32..63 ticks, then
 * `field48 * 32` more -- so the people come out staggered in order.
 *
 * `0x317` dresses the part as `swat_people[field48 - 1]` (a table of
 * part `field24` values), starts it walking right at `0x80000` from the
 * left camera edge minus `0x50`, poses animation `0x46`, and arms a
 * 160-tick walk in `a10`. `0x343` animates one tick per call until the
 * walk runs out, then parks under the `0x16462` termination sentinel.
 */
void get_char_ani(MK3OBJ *obj);
extern uint32_t swat_people[];              /* 0x00177b68 */

long t_swat_crossing_people(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);

    if (slot == 0x311) {
        obj->field1c = obj->field48 << 5;

        *mk3_frame(thread, thread->frame + 1) = 0x317;
        thread->fieldfc = obj->field1c;
        return (long)obj->field1c;
    }

    if (slot < 0x311) {
        if (slot != 0)
            return -3;

        obj->field08->field48 = 1;

        obj->field1c = 0x20;
        obj->field20 = 0x20;
        randu_minimum(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x311;
        thread->fieldfc = obj->field1c;
        return (long)obj->field1c;
    }

    if (slot == 0x317) {
        obj->field08->field24 = swat_people[obj->field48 - 1];

        obj->field38          = 0x80000;
        obj->field08->field18 = 0x80000;

        obj->field1c = *(uint32_t *)(G_BYTES + 0x468) - 0x50;
        MK3_SET_FIELD0E(obj->field08, obj->field1c);

        obj->field40 = 0x46;
        get_char_ani(obj);

        obj->field1c = 3;
        init_anirate(obj);

        obj->a10 = 0xa0;
        goto walk;
    }

    if (slot != 0x343)
        return -3;

    next_anirate(obj);

    obj->a10 = obj->a10 - 1;
    if (obj->a10 == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x348;
        thread->fieldfc = 0x16462;
        return 0x16462;
    }

walk:
    *mk3_frame(thread, thread->frame + 1) = 0x343;
    thread->fieldfc = 1;
    return 1;
}


/* --------------------------------------------------------------------- t_dinger_proc
 *
 * armv7 0x000a5d9c, 220 bytes.  **Complete.**
 *
 * `t_f_sektor`'s own `NewThread` target. Plays sound `0x21`, points
 * `field40` at `a_dinger`, and runs it at rate 4 through `t_mframew`
 * under `0x404`; `0x404` holds 64 ticks under `0x405`; `0x405` runs the
 * next stretch at rate 4 under `0x407`, whose re-entry parks on
 * `t_wait_forever`.
 */
extern uint8_t a_dinger[];                   /* 0x00177c44 */

long t_dinger_proc(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);

    if (slot == 0x404) {
        *mk3_frame(thread, thread->frame + 1) = 0x405;
        thread->fieldfc = 0x40;
        return 0x40;
    }

    if (slot < 0x404) {
        if (slot != 0)
            return -3;

        obj->field1c = 0x21;
        ochar_sound(obj);

        obj->field40 = (uint32_t)(uintptr_t)a_dinger;
        obj->field1c = 4;

        *mk3_frame(thread, thread->frame + 1) = 0x404;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot == 0x405) {
        obj->field1c = 4;

        *mk3_frame(thread, thread->frame + 1) = 0x407;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot != 0x407)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_wait_forever);
}


/* --------------------------------------------------------------------- t_wall_dragon_proc
 *
 * armv7 0x000a614c, 224 bytes.  **Complete.**
 *
 * The same shape as `t_dinger_proc`: points `field40` at
 * `a_wall_dragon`, places it (`multi_adjust_xy` at `0`/`-0x30`), runs it
 * at rate 5 under `0x599`, holds 32 ticks under `0x59a`, runs the rest
 * at rate 4 under `0x59c`, and parks on `t_wait_forever`.
 */
extern uint8_t a_wall_dragon[];              /* 0x00177e68 */

long t_wall_dragon_proc(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);

    if (slot == 0x599) {
        *mk3_frame(thread, thread->frame + 1) = 0x59a;
        thread->fieldfc = 0x20;
        return 0x20;
    }

    if (slot < 0x599) {
        if (slot != 0)
            return -3;

        obj->field1c = 0;
        obj->field40 = (uint32_t)(uintptr_t)a_wall_dragon;
        obj->field20 = (uint32_t)~0x2f;
        multi_adjust_xy(obj);

        obj->field1c = 5;

        *mk3_frame(thread, thread->frame + 1) = 0x599;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot == 0x59a) {
        obj->field1c = 4;

        *mk3_frame(thread, thread->frame + 1) = 0x59c;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot != 0x59c)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_wait_forever);
}


/* --------------------------------------------------------------------- t_fr_mileena
 *
 * armv7 0x000a6ff4, 248 bytes.  **Complete.**
 *
 * Poses animation `0x1a` at rate 5 under `0x73`, holds 96 ticks, then
 * plays sound 1, shakes (`0x70008`), runs the rest at rate 4 under
 * `0x79`, plays sound `0x66`, holds 32 ticks, and ends on
 * `t_friendship_complete`.
 */
long t_fr_mileena(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);

    if (slot == 0x74) {
        tsound_func(obj, 1);

        obj->field48 = 0x70008;
        shake_a11(obj);

        obj->field1c = 4;

        *mk3_frame(thread, thread->frame + 1) = 0x79;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot < 0x74) {
        if (slot == 0) {
            obj->field40 = 0x1a;
            get_char_ani2(obj);

            obj->field1c = 5;

            *mk3_frame(thread, thread->frame + 1) = 0x73;
            thread->frame = thread->frame + 1;   /* push a level */
            mk3_frame(thread, thread->frame)[1] =
                (uint32_t)(uintptr_t)t_mframew;
            *mk3_frame(thread, thread->frame + 1) = 0;
            return 0;
        }

        if (slot != 0x73)
            return -3;

        *mk3_frame(thread, thread->frame + 1) = 0x74;
        thread->fieldfc = 0x60;
        return 0x60;
    }

    if (slot == 0x79) {
        tsound_func(obj, 0x66);

        *mk3_frame(thread, thread->frame + 1) = 0x7b;
        thread->fieldfc = 0x20;
        return 0x20;
    }

    if (slot != 0x7b)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_friendship_complete);
}


/* --------------------------------------------------------------------- t_f_sindel
 *
 * armv7 0x000a67cc, 300 bytes.  **Complete.**
 *
 * Points `field40` at `a_lia_friend` and runs `t_mframew_5` under
 * `0x3bc`. `0x3bc` is the kick: sound 5 through `rsnd_func`, a shake
 * (`0x30003`), and the slave's GrObj restarted on `t_football_proc` --
 * then the fighter lets go of it, clearing both `field00->slave` and
 * `field00->field64` (the second through `field20`, zeroed first, the
 * register reuse `create_proj_proc`'s slave bookkeeping also shows). A
 * step, 16 ticks, another `t_mframew_5` stretch under `0x3ca`, sound 9,
 * 64 ticks under `0x3cd`, and `t_friendship_complete`.
 */
void rsnd_func(MK3OBJ *unused, uint32_t which);
void StartGrObjAt(char *grobj, MK3THREADFUNC func);
extern uint8_t a_lia_friend[];               /* 0x00177bdc */

long t_f_sindel(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);

    if (slot == 0x3c8) {
        *mk3_frame(thread, thread->frame + 1) = 0x3ca;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew_5;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot < 0x3c8) {
        if (slot == 0) {
            obj->field40 = (uint32_t)(uintptr_t)a_lia_friend;

            *mk3_frame(thread, thread->frame + 1) = 0x3bc;
            thread->frame = thread->frame + 1;   /* push a level */
            mk3_frame(thread, thread->frame)[1] =
                (uint32_t)(uintptr_t)t_mframew_5;
            *mk3_frame(thread, thread->frame + 1) = 0;
            return 0;
        }

        if (slot != 0x3bc)
            return -3;

        rsnd_func(obj, 5);

        obj->field48 = 0x30003;
        shake_a11(obj);

        StartGrObjAt((char *)(uintptr_t)obj->field00->slave,
                     (MK3THREADFUNC)t_football_proc);

        obj->field20          = 0;
        obj->field00->slave   = 0;
        obj->field00->field64 = obj->field20;

        do_next_a9_frame(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x3c8;
        thread->fieldfc = 0x10;
        return 0x10;
    }

    if (slot == 0x3ca) {
        obj->field1c = 9;
        ochar_sound(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x3cd;
        thread->fieldfc = 0x40;
        return 0x40;
    }

    if (slot != 0x3cd)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_friendship_complete);
}


/* --------------------------------------------------------------------- t_f_swat
 *
 * armv7 0x000a697c, 268 bytes.  **Complete.**
 *
 * Stryker's crossing guard. The free spawns a separate
 * `t_swat_friend_proc` thread, copies the part's flip word into
 * `field2c` (flipping the multi when bit `0x10` is set), and spawns
 * three `t_swat_crossing_people` threads in a real loop counting
 * `field48` down from 3 -- the count each pedestrian reads back as its
 * own index, since `NewThread` copies the spawner's object state. Then
 * `field40` at `a_swat_friend` through `t_mframew_5` under `0x372`,
 * sound 8 and 64 ticks under `0x375`, another `t_mframew_5` stretch
 * under `0x376`, and `t_friendship_complete`.
 */
extern uint8_t a_swat_friend[];              /* 0x00177af4 */
void flip_multi(MK3OBJ *obj);

long t_f_swat(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);

    if (slot == 0x372) {
        obj->field1c = 8;
        ochar_sound(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x375;
        thread->fieldfc = 0x40;
        return 0x40;
    }

    if (slot < 0x372) {
        if (slot != 0)
            return -3;

        NewThread(obj, (MK3THREADFUNC)t_swat_friend_proc);

        obj->field2c = obj->field08->field28;
        if (obj->field2c & 0x10)
            flip_multi(obj);

        obj->field48 = 3;
        do {
            NewThread(obj, (MK3THREADFUNC)t_swat_crossing_people);
            obj->field48 = obj->field48 - 1;
        } while (obj->field48 != 0);

        obj->field40 = (uint32_t)(uintptr_t)a_swat_friend;

        *mk3_frame(thread, thread->frame + 1) = 0x372;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew_5;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot == 0x375) {
        *mk3_frame(thread, thread->frame + 1) = 0x376;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew_5;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot != 0x376)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_friendship_complete);
}


/* --------------------------------------------------------------------- t_f_liu
 *
 * armv7 0x000a59dc, 296 bytes.  **Complete.**
 *
 * Liu Kang's dragon on the wall: spawns a separate `t_wall_dragon_proc`
 * thread, points `field40` at `a_kang_friend`, then alternates
 * `t_mframew_5` stretches and holds -- run (`0x5a4`), hold 32 (`0x5a5`),
 * run (`0x5a6`), hold 10 (`0x5a7`), run (`0x5a8`), hold 48 (`0x5a9`) --
 * and ends on `t_friendship_complete`.
 */
extern uint8_t a_kang_friend[];              /* 0x00177e2c */

long t_f_liu(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);
    uint32_t next;

    if (slot == 0x5a6) {
        *mk3_frame(thread, thread->frame + 1) = 0x5a7;
        thread->fieldfc = 0xa;
        return 0xa;
    }

    if (slot == 0x5a8) {
        *mk3_frame(thread, thread->frame + 1) = 0x5a9;
        thread->fieldfc = 0x30;
        return 0x30;
    }

    if (slot == 0x5a7) {
        next = 0x5a8;
        goto run;
    }

    if (slot == 0x5a9)
        return mk3_install(thread, (MK3THREADFUNC)t_friendship_complete);

    if (slot == 0x5a4) {
        *mk3_frame(thread, thread->frame + 1) = 0x5a5;
        thread->fieldfc = 0x20;
        return 0x20;
    }

    if (slot == 0x5a5) {
        next = 0x5a6;
        goto run;
    }

    if (slot != 0)
        return -3;

    NewThread(obj, (MK3THREADFUNC)t_wall_dragon_proc);
    obj->field40 = (uint32_t)(uintptr_t)a_kang_friend;

    *mk3_frame(thread, thread->frame + 1) = 0x5a4;
    thread->frame = thread->frame + 1;   /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_mframew_5;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;

run:
    *mk3_frame(thread, thread->frame + 1) = next;
    thread->frame = thread->frame + 1;   /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_mframew_5;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------------------- t_f_lao
 *
 * armv7 0x000a644c, 356 bytes.  **Complete.**
 *
 * Kung Lao's friendship, the busiest spawner in the file: the free
 * starts three separate threads -- `t_end_friend_proc`,
 * `t_cute_lil_doggy` and `t_lao_dog_sounds` -- points `field40` at
 * `a_lao_friend`, and runs `t_mframew_4` under `0x4b1`. `0x4b1` throws
 * the hat (`t_hat_proc`) and holds 96 ticks; `0x4b3` plays sound `0x8b`
 * and runs `t_mframew_4` again under `0x4b6`; `0x4b6` holds 48 under
 * `0x4b7`; `0x4b7` runs the last stretch at rate 8 under `0x4ba`, whose
 * re-entry installs `t_friendship_complete`.
 */
extern uint8_t a_lao_friend[];               /* 0x00177cc8 */
long t_mframew_4(struct MK3THREAD *thread);

long t_f_lao(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);

    if (slot == 0x4b3) {
        tsound_func(obj, 0x8b);

        *mk3_frame(thread, thread->frame + 1) = 0x4b6;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew_4;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot < 0x4b3) {
        if (slot == 0) {
            NewThread(obj, (MK3THREADFUNC)t_end_friend_proc);
            NewThread(obj, (MK3THREADFUNC)t_cute_lil_doggy);
            NewThread(obj, (MK3THREADFUNC)t_lao_dog_sounds);

            obj->field40 = (uint32_t)(uintptr_t)a_lao_friend;

            *mk3_frame(thread, thread->frame + 1) = 0x4b1;
            thread->frame = thread->frame + 1;   /* push a level */
            mk3_frame(thread, thread->frame)[1] =
                (uint32_t)(uintptr_t)t_mframew_4;
            *mk3_frame(thread, thread->frame + 1) = 0;
            return 0;
        }

        if (slot != 0x4b1)
            return -3;

        NewThread(obj, (MK3THREADFUNC)t_hat_proc);

        *mk3_frame(thread, thread->frame + 1) = 0x4b3;
        thread->fieldfc = 0x60;
        return 0x60;
    }

    if (slot == 0x4b7) {
        obj->field1c = 8;

        *mk3_frame(thread, thread->frame + 1) = 0x4ba;
        thread->frame = thread->frame + 1;   /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (slot == 0x4ba)
        return mk3_install(thread, (MK3THREADFUNC)t_friendship_complete);

    if (slot != 0x4b6)
        return -3;

    *mk3_frame(thread, thread->frame + 1) = 0x4b7;
    thread->fieldfc = 0x30;
    return 0x30;
}


/* --------------------------------------------------------------------- t_f_smoke_ninja
 *
 * armv7 0x000a5fc8, 388 bytes.  **Complete.**
 *
 * Robot Smoke's friendship borrows the chest routines from `mkzap.c`'s
 * bomb: spawn a separate `t_end_friend_proc`, open the chest
 * (`t_robo_open_chest`, `0x5cd`), run `a_smoke_friend` through
 * `t_mframew_5` (`0x5cf`), sound `0x22` and another stretch (`0x5d3`),
 * hold 16 ticks (`0x5d5`), a third stretch (`0x5d6`), delete the slave
 * and close the chest (`t_robo_close_chest`, `0x5d8`), and hand off to
 * `t_victory_animation` (`0x5d9`) -- which, as in `t_f_sonya`, never
 * resumes here.
 */
long t_robo_open_chest(struct MK3THREAD *thread);    /* pointer slot 0x000f36c4 */
long t_robo_close_chest(struct MK3THREAD *thread);   /* pointer slot 0x000f36bc */
extern uint8_t a_smoke_friend[];             /* 0x00177e90 */

long t_f_smoke_ninja(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);
    uint32_t next;
    uint32_t handler;

    if (slot == 0x5d3) {
        *mk3_frame(thread, thread->frame + 1) = 0x5d5;
        thread->fieldfc = 0x10;
        return 0x10;
    }

    if (slot > 0x5d3) {
        if (slot == 0x5d6) {
            delete_slave(obj);
            next    = 0x5d8;
            handler = (uint32_t)(uintptr_t)t_robo_close_chest;
            goto push;
        }
        if (slot == 0x5d8) {
            next    = 0x5d9;
            handler = (uint32_t)(uintptr_t)t_victory_animation;
            goto push;
        }
        if (slot != 0x5d5)
            return -3;

        next = 0x5d6;
        goto run;
    }

    if (slot == 0x5cd) {
        obj->field40 = (uint32_t)(uintptr_t)a_smoke_friend;
        next = 0x5cf;
        goto run;
    }

    if (slot == 0x5cf) {
        obj->field1c = 0x22;
        ochar_sound(obj);
        next = 0x5d3;
        goto run;
    }

    if (slot != 0)
        return -3;

    NewThread(obj, (MK3THREADFUNC)t_end_friend_proc);

    *mk3_frame(thread, thread->frame + 1) = 0x5cd;
    thread->frame = thread->frame + 1;   /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_robo_open_chest;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;

run:
    handler = (uint32_t)(uintptr_t)t_mframew_5;

push:
    *mk3_frame(thread, thread->frame + 1) = next;
    thread->frame = thread->frame + 1;   /* push a level */
    mk3_frame(thread, thread->frame)[1] = handler;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------------------------- t_f_shang
 *
 * armv7 0x000a622c, 328 bytes.  **Complete.**
 *
 * Shang Tsung turns into a bouncing ball. The free spawns a separate
 * `t_end_friend_proc`, turns off the fighter-pushing for good
 * (`sans_repell_for_good`), and plays effect `0x1e`; `0x539` sets the
 * part's frame (`0x1b31`) and drops it to `G+0xac - 0x20`, holding 64
 * ticks. Then `t_bounce` twice in place (`0x54a`, `0x54c`), a nudge
 * towards the opponent (`towards_x_vel` at `0x30000`) and `a10 = 5`
 * more bounces (`0x54d`), each one re-armed from `0x555` while the
 * count holds -- the same push both states share. When it runs out the
 * ball stops (`stop_me_player`) and parks on `t_wait_forever`.
 */
void sans_repell_for_good(MK3OBJ *obj);
void create_fx(MK3OBJ *obj);
void towards_x_vel(MK3OBJ *obj);
void stop_me_player(MK3OBJ *obj);

long t_f_shang(MK3THREAD *thread)
{
    MK3OBJ  *obj  = (MK3OBJ *)thread->proc;
    uint32_t slot = *mk3_frame(thread, thread->frame + 1);
    uint32_t next;

    if (slot == 0x54a) {
        next = 0x54c;
        goto bounce;
    }

    if (slot < 0x54a) {
        if (slot == 0) {
            NewThread(obj, (MK3THREADFUNC)t_end_friend_proc);
            sans_repell_for_good(obj);

            obj->field1c = 0x1e;
            create_fx(obj);

            *mk3_frame(thread, thread->frame + 1) = 0x539;
            thread->fieldfc = 8;
            return 8;
        }

        if (slot != 0x539)
            return -3;

        obj->field08->field2c = 0x1b31;
        MK3_SET_FIELD12(obj->field08, *(uint32_t *)(G_BYTES + 0xac) - 0x20);

        *mk3_frame(thread, thread->frame + 1) = 0x54a;
        thread->fieldfc = 0x40;
        return 0x40;
    }

    if (slot == 0x54d) {
        obj->field1c = 0x30000;
        towards_x_vel(obj);

        obj->a10 = 5;
        goto again;
    }

    if (slot == 0x555) {
        obj->a10 = obj->a10 - 1;
        if ((int32_t)obj->a10 < 0) {
            stop_me_player(obj);
            return mk3_install(thread, (MK3THREADFUNC)t_wait_forever);
        }
        goto again;
    }

    if (slot != 0x54c)
        return -3;

    next = 0x54d;
    goto bounce;

again:
    next = 0x555;

bounce:
    *mk3_frame(thread, thread->frame + 1) = next;
    thread->frame = thread->frame + 1;   /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_bounce;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}
