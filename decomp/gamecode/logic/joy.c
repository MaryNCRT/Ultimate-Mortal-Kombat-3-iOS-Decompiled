/*
 * joy.c -- gamecode/logic/joy.c, decompiled.
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

long t_joyd3(struct MK3THREAD *thread);

/* t_joy_getup_entry -- armv7 0x0002edfc, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = t_joyd3
 *      frame[frame+1].w0 = 0
 */

long t_joy_getup_entry(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_joyd3);
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

long t_act_mframew(struct MK3THREAD *thread);
long t_jdblk2(struct MK3THREAD *thread);
long t_jhp5(struct MK3THREAD *thread);
long t_jmp5(struct MK3THREAD *thread);
long t_unhip1(struct MK3THREAD *thread);
void find_ani_part2(MK3OBJ *obj);
void find_part2(MK3OBJ *obj);
void get_char_ani(MK3OBJ *obj);
void stop_me_player(MK3OBJ *obj);

/* t_joy_duck_block -- armv7 0x0002f220, 64 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      disable_all_buttons(obj)
 *      frame[frame].handler = t_jdblk2
 *      frame[frame+1].w0 = 0
 */

long t_joy_duck_block(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    disable_all_buttons(obj);

    return mk3_push_handler(thread, (MK3THREADFUNC)t_jdblk2);
}

/* t_joy_un_lo_punch1 -- armv7 0x0002f4a0, 68 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field40 = 0xf
 *      find_ani_part2(obj)
 *      frame[frame].handler = t_unhip1
 *      frame[frame+1].w0 = 0
 */

long t_joy_un_lo_punch1(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field40 = 0xf;
    find_ani_part2(obj);

    return mk3_push_handler(thread, (MK3THREADFUNC)t_unhip1);
}

/* t_joy_un_hi_punch1 -- armv7 0x0002f4e4, 68 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field40 = 0xe
 *      find_ani_part2(obj)
 *      frame[frame].handler = t_unhip1
 *      frame[frame+1].w0 = 0
 */

long t_joy_un_hi_punch1(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field40 = 0xe;
    find_ani_part2(obj);

    return mk3_push_handler(thread, (MK3THREADFUNC)t_unhip1);
}

/* t_joy_punch_htm1 -- armv7 0x0002f590, 96 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field40 = 0xe
 *      find_ani_part2(obj)
 *      find_part2(obj)
 *      find_part2(obj)
 *      find_part2(obj)
 *      find_part2(obj)
 *      frame[frame].handler = t_jmp5
 *      frame[frame+1].w0 = 0
 */

long t_joy_punch_htm1(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field40 = 0xe;
    find_ani_part2(obj);
    find_part2(obj);
    find_part2(obj);
    find_part2(obj);
    find_part2(obj);

    return mk3_push_handler(thread, (MK3THREADFUNC)t_jmp5);
}

/* t_joy_punch_mth1 -- armv7 0x0002f658, 96 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field40 = 0xf
 *      find_ani_part2(obj)
 *      find_part2(obj)
 *      find_part2(obj)
 *      find_part2(obj)
 *      find_part2(obj)
 *      frame[frame].handler = t_jhp5
 *      frame[frame+1].w0 = 0
 */

long t_joy_punch_mth1(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field40 = 0xf;
    find_ani_part2(obj);
    find_part2(obj);
    find_part2(obj);
    find_part2(obj);
    find_part2(obj);

    return mk3_push_handler(thread, (MK3THREADFUNC)t_jhp5);
}

/* t_joy_un_lo_punch2 -- armv7 0x0002f6b8, 76 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field40 = 0xf
 *      find_ani_part2(obj)
 *      find_part2(obj)
 *      frame[frame].handler = t_unhip1
 *      frame[frame+1].w0 = 0
 */

long t_joy_un_lo_punch2(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field40 = 0xf;
    find_ani_part2(obj);
    find_part2(obj);

    return mk3_push_handler(thread, (MK3THREADFUNC)t_unhip1);
}

/* t_joy_un_hi_punch2 -- armv7 0x0002f78c, 76 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field40 = 0xe
 *      find_ani_part2(obj)
 *      find_part2(obj)
 *      frame[frame].handler = t_unhip1
 *      frame[frame+1].w0 = 0
 */

long t_joy_un_hi_punch2(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field40 = 0xe;
    find_ani_part2(obj);
    find_part2(obj);

    return mk3_push_handler(thread, (MK3THREADFUNC)t_unhip1);
}

/* t_do_duck_block -- armv7 0x00030550, 96 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      stop_me_player(obj)
 *      face_opponent(obj)
 *      obj->field40 = 0x6
 *      get_char_ani(obj)
 *      obj->field1c = 0x3
 *      obj->field20 = 0x701
 *      frame[frame].handler = t_act_mframew
 *      frame[frame+1].w0 = 0
 */

long t_do_duck_block(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    stop_me_player(obj);
    face_opponent(obj);
    obj->field40 = 0x6;
    get_char_ani(obj);
    obj->field1c = 0x3;
    obj->field20 = 0x701;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_act_mframew);
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

void group_sound(MK3OBJ *obj);
void init_anirate(MK3OBJ *obj);
void towards_x_vel(MK3OBJ *obj);

/* run_setup -- armv7 0x00030fbc, 48 bytes.  **Complete.**
 *
 *      obj->field40 = 0x46
 *      get_char_ani(obj)
 *      obj->field1c = 0x3
 *      init_anirate(obj)
 *      obj->field1c = 0x80000
 *      towards_x_vel(obj)
 *      obj->field1c = 0x7
 *      group_sound(obj)
 */
void run_setup(MK3OBJ *obj)
{
    obj->field40 = 0x46;
    get_char_ani(obj);
    obj->field1c = 0x3;
    init_anirate(obj);
    obj->field1c = 0x80000;
    towards_x_vel(obj);
    obj->field1c = 0x7;
    group_sound(obj);
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

long t_do_body_slam(MK3THREAD *thread);
long t_joy_duck_block_loop(MK3THREAD *thread);
long t_local_reaction_exit(MK3THREAD *thread);
long t_stat_do_sweep_kick(MK3THREAD *thread);

/* t_joy_sweep_kick -- armv7 0x0002f1b4, 108 bytes.  **Complete.**
 *
 *      token == 0:
 *          token := 0x22e, then descend into t_stat_do_sweep_kick
 *      token == 0x22e:
 *          frame[frame].handler = t_local_reaction_exit
 *      otherwise:  return -3
 */
long t_joy_sweep_kick(MK3THREAD *thread)
{
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x22e;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_stat_do_sweep_kick;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x22e)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

/* t_jdblk2 -- armv7 0x0002f260, 104 bytes.  **Complete.**
 *
 *      token == 0:
 *          token := 0x253, then descend into t_do_duck_block
 *      token == 0x253:
 *          frame[frame].handler = t_joy_duck_block_loop
 *      otherwise:  return -3
 */
long t_jdblk2(MK3THREAD *thread)
{
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x253;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_do_duck_block;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x253)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_joy_duck_block_loop);
}

/* t_joy_toss -- armv7 0x0002f410, 108 bytes.  **Complete.**
 *
 *      token == 0:
 *          token := 0x7b7, then descend into t_do_body_slam
 *      token == 0x7b7:
 *          frame[frame].handler = t_local_reaction_exit
 *      otherwise:  return -3
 */
long t_joy_toss(MK3THREAD *thread)
{
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x7b7;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_do_body_slam;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x7b7)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

/* --------------------------------------------------------------------
 * Added by a later sweep -- tools/sweep.py, running the same
 * readers again after one of them learned something. Each still
 * refuses anything it cannot account for instruction by
 * instruction; see tools/pushfn.py and tools/leaffn.py.
 * -------------------------------------------------------------------- */

long t_do_duck(MK3THREAD *thread);
long t_do_flip_kick(MK3THREAD *thread);
long t_do_jumpup_punch(MK3THREAD *thread);
long t_stat_do_uppercut(MK3THREAD *thread);

/* t_joy_down -- armv7 0x0002ed80, 124 bytes.  **Complete.**
 *
 *      token == 0:
 *          disable_all_buttons(obj)
 *          token := 0x159, then descend into t_do_duck
 *      token == 0x159:
 *          frame[frame].handler = t_joy_getup_entry
 *      otherwise:  return -3
 */
long t_joy_down(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        disable_all_buttons(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x159;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_do_duck;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x159)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_joy_getup_entry);
}

/* t_joy_flip_kick -- armv7 0x0002ef60, 124 bytes.  **Complete.**
 *
 *      token == 0:
 *          disable_all_buttons(obj)
 *          token := 0x1c9, then descend into t_do_flip_kick
 *      token == 0x1c9:
 *          frame[frame].handler = t_local_reaction_exit
 *      otherwise:  return -3
 */
long t_joy_flip_kick(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        disable_all_buttons(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x1c9;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_do_flip_kick;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x1c9)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

/* t_jumpup_punch -- armv7 0x0002f054, 124 bytes.  **Complete.**
 *
 *      token == 0:
 *          disable_all_buttons(obj)
 *          token := 0x1db, then descend into t_do_jumpup_punch
 *      token == 0x1db:
 *          frame[frame].handler = t_local_reaction_exit
 *      otherwise:  return -3
 */
long t_jumpup_punch(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        disable_all_buttons(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x1db;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_do_jumpup_punch;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x1db)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

/* t_joy_uppercut -- armv7 0x0002f2c8, 124 bytes.  **Complete.**
 *
 *      token == 0:
 *          disable_all_buttons(obj)
 *          token := 0x2a1, then descend into t_stat_do_uppercut
 *      token == 0x2a1:
 *          frame[frame].handler = t_local_reaction_exit
 *      otherwise:  return -3
 */
long t_joy_uppercut(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        disable_all_buttons(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x2a1;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_stat_do_uppercut;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x2a1)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

/* --------------------------------------------------------------------
 * Added by a later sweep -- tools/sweep.py, running the same
 * readers again after one of them learned something. Each still
 * refuses anything it cannot account for instruction by
 * instruction; see tools/pushfn.py and tools/leaffn.py.
 * -------------------------------------------------------------------- */

long t_do_backup(MK3THREAD *thread);
long t_mframew(MK3THREAD *thread);

/* t_unhip1 -- armv7 0x0002f704, 136 bytes.  **Complete.**
 *
 *      token == 0:
 *          find_part2(obj)
 *          find_part2(obj)
 *          obj->field1c = 0x2
 *          token := 0x7ac, then descend into t_mframew
 *      token == 0x7ac:
 *          frame[frame].handler = t_local_reaction_exit
 *      otherwise:  return -3
 */
long t_unhip1(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        find_part2(obj);
        find_part2(obj);
        obj->field1c = 0x2;
        *mk3_frame(thread, thread->frame + 1) = 0x7ac;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x7ac)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

/* t_joy_back_up -- armv7 0x000305b0, 132 bytes.  **Complete.**
 *
 *      token == 0:
 *          disable_all_buttons(obj)
 *          face_opponent(obj)
 *          token := 0x179, then descend into t_do_backup
 *      token == 0x179:
 *          frame[frame].handler = t_local_reaction_exit
 *      otherwise:  return -3
 */
long t_joy_back_up(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        disable_all_buttons(obj);
        face_opponent(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x179;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_do_backup;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x179)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* ========================================================================
 * THE INPUT CONTRACT
 *
 * Everything a keyboard or a gamepad has to produce, and every bit of it is
 * measured rather than assumed. Three independent readings agree.
 *
 * ## One ten-bit word per player
 *
 * `mk3_update` takes `long joy[2]` and hands it to `TranslateJoybits`:
 *
 *      bit 0..3   the four directions
 *      bit 4      HP        high punch
 *      bit 5      LP        low punch
 *      bit 6      BL        block
 *      bit 7      HK        high kick
 *      bit 8      LK        low kick
 *      bit 9      RUN
 *      bit 10     a SPECIAL-MOVE REQUEST, not a button -- leave it clear
 *
 * ## How the six buttons were named
 *
 * Not guessed. Three measurements, and the last two pin the whole row:
 *
 *   1. **`bt_stance` names the moves in order.** It is a table of six words
 *      and its entries are `t_joy_hi_punch`, `t_joy_lo_punch`, `t_joy_block`,
 *      `t_joy_hi_kick`, `t_joy_lo_kick`, and nothing. So the table index runs
 *      HP, LP, BL, HK, LK, RUN.
 *
 *   2. **`check_block_bit` measures BLOCK**: it masks the translated word with
 *      0x20 for player 0 and 0x2000 for player 1. `TranslateJoybits` puts
 *      input bit 6 at exactly those two positions. **So BL is input bit 6, and
 *      it is table index 2.**
 *
 *   3. **`is_run_pressed` measures RUN**: 0x40000 and 0x400000, which
 *      `TranslateJoybits` fills from input bit 9. **So RUN is input bit 9, and
 *      it is table index 5.**
 *
 * Index 2 is input bit 6 and index 5 is input bit 9, so **index = input bit
 * minus four**, and the other four fall out: HP 4, LP 5, HK 7, LK 8. Two
 * independent constraints, both satisfied, no freedom left.
 *
 * ## Where the bits end up
 *
 * `TranslateJoybits` spreads them, and the layout looks arbitrary until the
 * names are on it:
 *
 *      HP  -> bit  4 (P1) / bit 12 (P2)      LP  -> bit 16 / bit 20
 *      BL  -> bit  5      / bit 13           LK  -> bit 17 / bit 21
 *      HK  -> bit  6      / bit 14           RUN -> bit 18 / bit 22
 *
 * The low trio is **HP, BL, HK** and the high trio is **LP, LK, RUN** -- the
 * high attacks with block, against the low attacks with run. That grouping is
 * why `buttons_in_a2` in moves.c can mask a player's whole button set with one
 * constant: 0x00070070 for player 0, 0x00707000 for player 1.
 *
 * ## The directions
 *
 * `joystick_in_a0_px` in other.c reads `G[player]` -- the per-player word
 * `mk3_update` stores at `G + 0x00` and `G + 0x04` -- and masks it with 0xf.
 * So **the four direction bits survive translation unmoved, at bits 0..3**,
 * and they are read from a different place than the buttons.
 *
 * **Bits 2 and 3 are the horizontal pair; bits 0 and 1 are the vertical one.**
 * Established by `Playback_Update` in playback.c: replaying a recorded motion
 * for a fighter facing the other way exchanges exactly bits 2 and 3 and
 * touches nothing else, and mirroring a motion swaps left and right while
 * leaving up and down alone.
 *
 * **Which of 2 and 3 is LEFT is still open**, and so is which of 0 and 1 is
 * UP -- swapping a pair is symmetric and says nothing about polarity. The
 * pairing is settled; the polarity is not, and is not guessed here.
 *
 * `mask_joystick` below shows they are then filtered by `proc->field34`, so a
 * state can forbid individual directions.
 *
 * ## What this means for the port
 *
 * There is no touch input anywhere in the fight engine and there never was.
 * The iOS build's touch layer sits above `UpdateArcadeCode`, filling `joy[2]`.
 * **Replacing it with a keyboard or a gamepad is a matter of filling two
 * ten-bit words**, and nothing below that line needs to change.
 * ======================================================================== */


/* ----------------------------------------------------------- stuff_buttons
 *
 * armv7 0x0002ec50, FOUR bytes.  **Complete.**
 *
 *      obj->field60 = table
 *
 * One store and a return, and it is the smallest function in the directory.
 *
 * **It also proves MK3OBJ reaches 0x60.** The struct as this project had it
 * ended at 0x5c; `Plyr`'s stride is 108, so there was room and nothing known to
 * be in it. 0x60 is the first field found in that space, and it is **the
 * fighter's current button table.**
 */
void stuff_buttons(MK3OBJ *obj, uint32_t table)
{
    obj->field60 = table;
}


/* ============================ enable_all_buttons, disable_all_buttons,
 *                              disable_his_buttons
 *
 * armv7 0x0002ec54, 0x0002ec68 and 0x0002ec7c; 20, 20 and 24 bytes.
 * **Complete.**
 *
 *      enable_all_buttons(obj)   stuff_buttons(obj,                  bt_stance)
 *      disable_all_buttons(obj)  stuff_buttons(obj,                  bt_null)
 *      disable_his_buttons(obj)  stuff_buttons(obj->field00->field00, bt_null)
 *
 * **"Enable all buttons" means "go back to the standing table".** Not a flag,
 * not a mask -- the standing move set IS the enabled state, and `bt_null` is a
 * table of ten zeroes that answers every button with nothing.
 *
 * So disabling input costs one pointer store and no branches anywhere else, and
 * a state that wants to take control away from the player just parks `bt_null`
 * in 0x60 until it is done. That is the whole mechanism.
 *
 * `disable_his_buttons` reaches the opponent through `obj->field00->field00`,
 * the two-hop that mk3logic.h documents as "him".
 */
extern const void *P32 bt_null;        /* 0x00165584 */
extern const void *P32 bt_angle_jump;  /* 0x001655ac */
extern const void *P32 bt_duck;        /* 0x001655d4 */
extern const void *P32 bt_stance;      /* 0x001655fc */
extern const void *P32 bt_jump;        /* 0x00165624 */

void enable_all_buttons(MK3OBJ *obj)
{
    stuff_buttons(obj, (uint32_t)(uintptr_t)&bt_stance);
}

void disable_all_buttons(MK3OBJ *obj)
{
    stuff_buttons(obj, (uint32_t)(uintptr_t)&bt_null);
}

void disable_his_buttons(MK3OBJ *obj)
{
    stuff_buttons(obj->field00->field00, (uint32_t)(uintptr_t)&bt_null);
}


/* ------------------------------------------------- me_in_front, me_in_back
 *
 * armv7 0x0002ec24 and 0x0002ec38; 20 and 24 bytes.  **Complete.**
 *
 *      me_in_front(obj)   G[0x474] =  obj->field00->field08
 *      me_in_back(obj)    G[0x474] =  obj->field00->field08 ^ 1
 *
 * `G + 0x474` is what `mk3_who_in_front` returns and `RenderLevelPlayers`
 * reads, and `mk3_init` seeds it from one random bit. **So it holds a PLAYER
 * INDEX, 0 or 1, and these two are how a fighter claims or yields the front of
 * the draw order.**
 *
 * `field08` on the proc is the side index -- the same field `buttons_in_a2`
 * uses to choose a button mask and `mk3_update` uses to set a display flag --
 * so "me" is spelled as "my side" and "him" as that XOR 1.
 *
 * Two functions, one XOR apart. The pair shape this directory is full of.
 */
void me_in_front(MK3OBJ *obj)
{
    *(uint32_t *)(void *)(G_BYTES + 0x474) = obj->field00->field08;
}

void me_in_back(MK3OBJ *obj)
{
    *(uint32_t *)(void *)(G_BYTES + 0x474) = obj->field00->field08 ^ 1u;
}


/* ----------------------------------------------------------- inc_downcount
 *
 * armv7 0x0002ec94, 20 bytes.  **Complete.**
 *
 *      obj->field00->field14 += 1
 *      obj->field1c = obj->field00->field14
 *
 * A counter on the proc, bumped and then copied back into the object's
 * scratch word so the caller can read it without a second load. **The proc is
 * re-loaded between the two** (`ldr r2, [r0]` after the store), which is the
 * compiler refusing to keep a pointer live across a store through it -- not a
 * hint that anything changed.
 */
void inc_downcount(MK3OBJ *obj)
{
    obj->field00->field14 += 1;
    obj->field1c = obj->field00->field14;
}


/* ---------------------------------------------------------- check_block_bit
 *
 * armv7 0x0002eca8, 44 bytes.  **Complete.**
 *
 *      obj->field1c = G[0x1c]
 *      mask = obj->field00->field08 ? 0x2000 : 0x20
 *      obj->field1c = G[0x1c] & mask
 *      obj->field5c = (obj->field1c != 0)
 *
 * **One of the two measurements that name the buttons.** 0x20 and 0x2000 are
 * where `TranslateJoybits` puts input bit 6 for player 0 and player 1, so
 * **block is input bit 6** -- see the input contract above.
 *
 * The answer lands in `obj->field5c`, which mk3logic.h already calls "am_i_joy's
 * isolated bit": **0x5c is this file's boolean return slot**, and three
 * functions here write it.
 *
 * `field1c` is written twice, first with the whole word and then with the
 * masked one. A spill, not two meanings.
 */
long check_block_bit(MK3OBJ *obj)
{
    uint32_t now  = *(const uint32_t *)(const void *)(G_BYTES + 0x1c);
    uint32_t mask = obj->field00->field08 ? 0x2000u : 0x20u;

    obj->field1c = now;
    obj->field1c = now & mask;
    obj->field5c = (obj->field1c != 0);
    /* The binary leaves the same 0/1 in r0 as well as in field5c, and
     * t_joyd5 is the caller that reads r0 rather than the field. */
    return (long)obj->field5c;
}


/* ------------------------------------------------------------ mask_joystick
 *
 * armv7 0x000304b8, 24 bytes.  **Complete.**
 *
 *      joystick_in_a0(obj)                  ; fills obj->field1c with G[player] & 0xf
 *      obj->field20  = obj->field00->field34
 *      obj->field1c &= obj->field20
 *
 * **`proc->field34` is a per-state direction filter.** The four direction bits
 * arrive from `joystick_in_a0` and are ANDed with whatever the proc has parked
 * at 0x34, so a state that must not accept "back" clears that bit once and
 * every read after it is already filtered.
 *
 * A port that reads the raw stick and forgets this mask will let a fighter walk
 * out of animations it is not supposed to be able to leave.
 */
long joystick_in_a0(MK3OBJ *obj);

void mask_joystick(MK3OBJ *obj)
{
    joystick_in_a0(obj);
    obj->field20  = obj->field00->field34;
    obj->field1c &= obj->field20;
}


/* ------------------------------------------------------ get_x_dist, get_y_dist
 *
 * armv7 0x0002f3a0 and 0x0002f3c0; 32 and 36 bytes.  **Complete.**
 *
 *      get_x_dist(obj)   obj->field2c = (int16_t)obj->field08->0x0e
 *                        obj->field28 = |proc->him->0x0e - obj->field2c|
 *
 *      get_y_dist(obj)   obj->field2c = (int16_t)obj->field08->0x12
 *                        obj->a10     = proc->him
 *                        obj->field28 = |proc->him->0x12 - obj->field2c|
 *
 * **These two settle a question mk3logic.h has carried open since the proc
 * struct was first written.** That header says of `field00` and `him`:
 *
 *      "the field is still called field00 and not `him`, because `him` is
 *       already taken by 0x04 ... one of the two readings is incomplete and
 *       nothing here settles which."
 *
 * **Neither is incomplete. They point at different structs.** Six functions
 * reach `obj->field00->field00` and are named for the opponent, so 0x00 is the
 * opponent's MK3OBJ. These two dereference `proc->him` at **0x0e and 0x12** --
 * the two halfword coordinates that live on a GrObj -- so **0x04 is the
 * opponent's GrObj**, their position record.
 *
 * Both are "the opponent". One is the object, one is where it is standing.
 *
 * The two functions are the same routine over the two coordinates, and the
 * absolute value is the same `cmp`/`rsblt` pair in both. `get_y_dist` also
 * parks the opponent's part in `a10` on the way past, which `get_x_dist` does
 * not -- the one asymmetry between them, and the reason `get_y_dist` is four
 * bytes longer.
 *
 * `field2c` gets MY coordinate and `field28` gets the DISTANCE, always
 * positive. A caller that needs the sign has to take it from `field2c` and the
 * opponent itself; it is thrown away here.
 */
void get_x_dist(MK3OBJ *obj)
{
    MK3OBJ *him = (MK3OBJ *)(uintptr_t)obj->field00->him;
    long    d;

    obj->field2c = (uint32_t)(long)MK3_FIELD0E_S(obj->field08);

    d = (long)MK3_FIELD0E_S(him) - (long)obj->field2c;
    obj->field28 = (uint32_t)d;
    if (d < 0)
        obj->field28 = (uint32_t)(-d);
}

void get_y_dist(MK3OBJ *obj)
{
    MK3OBJ *him;
    long    d;

    obj->field2c = (uint32_t)(long)MK3_FIELD12_S(obj->field08);

    obj->a10 = obj->field00->him;          /* the opponent's part, parked */

    him = (MK3OBJ *)(uintptr_t)obj->field00->him;
    d = (long)MK3_FIELD12_S(him) - (long)obj->field2c;
    obj->field28 = (uint32_t)d;
    if (d < 0)
        obj->field28 = (uint32_t)(-d);
}


/* ------------------------------------------------------------ ochar_begin_calls
 *
 * armv7 0x0002f48c, 20 bytes.  **Complete.**
 *
 *      if (obj->field08->0x24 == 0xc) shang_begin(obj)
 *
 * **`GrObj + 0x24` is the CHARACTER NUMBER**, and 0xc is Shang Tsung -- the
 * function called for it says so. `mk3_update` copies that same field into the
 * display record's 0x0c, so the renderer is told which character it is drawing
 * through the same byte.
 *
 * A **ninth hard-coded character number** for the port-critical table, and the
 * mild kind: the constant is right next to a function whose name identifies it,
 * the way `is_he_motaro` and `is_jade_protected` are. The dangerous ones are
 * the bare numbers.
 *
 * The name is plural and the body has one case, so this is the hook where
 * per-character round-start work goes and only one character needed any.
 */
void shang_begin(MK3OBJ *obj);

#define MK3_SHANG_TSUNG  0xc

void ochar_begin_calls(MK3OBJ *obj)
{
    if (obj->field08->field24 == MK3_SHANG_TSUNG)
        shang_begin(obj);
}


/* ------------------------------------------------------------ reset_proc_stack
 *
 * armv7 0x0002ebdc, 72 bytes.  **Complete.**
 *
 *      handler = mk3_frame(t, t->frame)[1]      ; save the current two
 *      token   = mk3_frame(t, t->frame + 1)[0]
 *
 *      t->frame   = 0
 *      t->fieldf8 = 0                           ; the argument cursor
 *      t->func    = t_local_reaction_exit       ; = mk3_frame(t, 0)[1]
 *      mk3_frame(t, 1)[0] = 0
 *
 *      t->frame = 1
 *      mk3_frame(t, 1)[1] = handler             ; put the saved pair back
 *      mk3_frame(t, 2)[0] = token
 *
 * **It throws the whole frame stack away and rebuilds a two-level one.** Level
 * 0 becomes `t_local_reaction_exit` and level 1 becomes whatever the thread was
 * doing, so however deep a fighter was nested -- a special move inside a combo
 * inside a stance -- one call collapses it to "do this, then leave the
 * reaction".
 *
 * That is what a hit does. Everything the fighter had pending is discarded, not
 * unwound, and there is no way back to it.
 *
 * **`t->func` at 0x04 IS `mk3_frame(t, 0)[1]`.** The frame array starts at the
 * thread's own address with eight-byte entries, so entry 0's handler word is
 * the struct's `func` field -- which is why mk3logic.h says the array "sits at
 * the thread's own address and overlaps them, which is why `frame` is never
 * small". This routine writes the same word both ways in nine instructions and
 * is the clearest demonstration of it in the tree.
 *
 * `fieldf8` is cleared too, so the argument stack goes with the frame stack.
 * Anything a caller pushed for a callee that is now discarded is gone, which is
 * the only safe thing to do and the reason the two are cleared together.
 */
long t_local_reaction_exit(MK3THREAD *thread);

void reset_proc_stack(MK3THREAD *thread)
{
    uint32_t handler = mk3_frame(thread, thread->frame)[1];
    uint32_t token   = *mk3_frame(thread, thread->frame + 1);

    thread->frame   = 0;
    thread->fieldf8 = 0;
    mk3_frame(thread, 0)[1] = (uint32_t)(uintptr_t)t_local_reaction_exit;
    *mk3_frame(thread, 1) = 0;

    thread->frame = 1;
    mk3_frame(thread, 1)[1] = handler;
    *mk3_frame(thread, 2) = token;
}


/* ------------------------------------------------------- t_local_reaction_exit
 *
 * armv7 0x00030060, 356 bytes.  **Complete.**
 *
 * The bottom of the frame stack `reset_proc_stack` installs. **Every hit a
 * fighter takes ends here**, and this decides what they become afterwards.
 *
 *      token == 0:
 *          c = obj->field08->0x24                   ; the character number
 *          if (c == 0xe || c == 0x12) ReallyKillProjectile(obj)
 *          airborne = am_i_airborn(obj)
 *          if (airborne) {
 *              obj->field20 = 1
 *              obj->field24 = 0x6000
 *              obj->field28 = 5
 *              obj->field1c = 0
 *              frame[frame].handler = t_fall_on_my_back
 *              frame[frame+1].w0 = 0
 *              return 0
 *          }
 *          stop_me_player(obj)
 *          back_to_normal(obj)
 *          reset_proc_stack(thread)
 *          token := 0x65b, push a level, handler = t_back_to_shang_check
 *          return 0
 *
 *      token == 0x65b:
 *          f = obj->field00->field10                ; the proc's flag word
 *          obj->field2c = f
 *          if (f & 0x40)  frame[frame].handler = t_collapse_on_ground
 *          else if (f & 1) {
 *              frame[frame].handler = plyrthread
 *              frame[frame+1].w0    = GLBL_joy_entry
 *          } else         frame[frame].handler = t_drone_entry
 *          return 0
 *
 *      otherwise: return -3
 *
 * ## Bit 0 of the proc's 0x10 is the human/AI switch, and here is where it is read
 *
 * `no_ai_hack` in mk3.c sets that bit on both fighters and its name says what
 * that means. `mk3_init` starts each fighter's thread at `plyrthread` or
 * `t_drone_begin` on bit 7 of the character argument. **This is the third site,
 * and it is the one that matters every round**: after every single reaction the
 * fighter is handed back to `plyrthread` if bit 0 is set and to
 * `t_drone_entry` if it is not.
 *
 * So the CPU flag is not just an initial condition. A fighter that loses that
 * bit mid-match stops responding to the stick the next time it is hit, and
 * one that gains it starts responding. **That is the whole mechanism behind
 * `no_ai_hack`**, and it is why that function is thirty-two bytes and needs no
 * cooperation from anything else.
 *
 * **Bit 6 (0x40) of the same word outranks it**: a fighter with that set
 * collapses on the ground instead of getting up, whether it is human-driven or
 * not. Checked first, and the human/AI question is never asked.
 *
 * ## GLBL_joy_entry
 *
 * `plyrthread` is not resumed at token 0. It is resumed at whatever
 * `_GLBL_joy_entry` holds -- a global at 0x00165580, **initialised to 0 in the
 * file** -- so the entry point of the human state machine is a variable and
 * something can change where a fighter comes back to. Nothing decompiled writes
 * it yet.
 *
 * ## Airborne is a different ending
 *
 * Being hit in the air does not reset the proc stack at all. Four fields are
 * set -- 0x20 = 1, 0x24 = 0x6000, 0x28 = 5, 0x1c = 0 -- and the handler becomes
 * `t_fall_on_my_back` in place, with the stack left as deep as it was. The
 * grounded path is the one that unwinds.
 *
 * ## Two more hard-coded character numbers
 *
 * 0xe and 0x12 get `ReallyKillProjectile` called on them before anything else,
 * and they are bare numbers with no named predicate around them -- the
 * dangerous kind. **Tenth and eleventh entries in the port-critical table.**
 * Whatever those two characters are, a hit cancels a projectile they own and no
 * other character gets that.
 */
long am_i_airborn(MK3OBJ *obj);
void ReallyKillProjectile(MK3OBJ *obj);
void stop_me_player(MK3OBJ *obj);
void back_to_normal(MK3OBJ *obj);
long t_fall_on_my_back(MK3THREAD *thread);      /* slot 0x000f38ac */
long t_collapse_on_ground(MK3THREAD *thread);   /* slot 0x000f385c */
long t_back_to_shang_check(MK3THREAD *thread);  /* slot 0x000f38c0 */
long t_drone_entry(MK3THREAD *thread);          /* slot 0x000f37ec */
long plyrthread(MK3THREAD *thread);

extern uint32_t GLBL_joy_entry;                 /* 0x00165580, starts at 0 */

#define MK3_KILLS_PROJ_A  0x0e
#define MK3_KILLS_PROJ_B  0x12

long t_local_reaction_exit(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    uint32_t c;
    long     airborne;

    if (token == 0) {
        c = obj->field08->field24;

        /* Two am_i_airborn calls in the binary, one on each side of the
         * projectile test; written the same way. */
        if (c == MK3_KILLS_PROJ_A || c == MK3_KILLS_PROJ_B) {
            ReallyKillProjectile(obj);
            airborne = am_i_airborn(obj);
        } else {
            airborne = am_i_airborn(obj);
        }

        if (airborne != 0) {
            obj->field20 = 1;
            obj->field24 = 0x6000;
            obj->field28 = 5;
            obj->field1c = 0;

            mk3_frame(thread, thread->frame)[1] =
                (uint32_t)(uintptr_t)t_fall_on_my_back;
            *mk3_frame(thread, thread->frame + 1) = 0;
            return 0;
        }

        stop_me_player(obj);
        back_to_normal(obj);
        reset_proc_stack(thread);

        *mk3_frame(thread, thread->frame + 1) = 0x65b;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_back_to_shang_check;
        *mk3_frame(thread, thread->frame + 1) = (uint32_t)airborne;  /* 0 */
        return airborne;
    }

    if (token != 0x65b)
        return -3;

    {
        uint32_t f = obj->field00->field10;
        obj->field2c = f;

        if ((f & 0x40u) != 0) {                     /* collapse outranks all */
            mk3_frame(thread, thread->frame)[1] =
                (uint32_t)(uintptr_t)t_collapse_on_ground;
            *mk3_frame(thread, thread->frame + 1) = 0;
            return 0;
        }

        if ((f & 1u) != 0) {                        /* bit 0: a human drives me */
            mk3_frame(thread, thread->frame)[1] =
                (uint32_t)(uintptr_t)plyrthread;
            *mk3_frame(thread, thread->frame + 1) = GLBL_joy_entry;
            return 0;
        }

        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_drone_entry;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }
}


/* ===================================================================== plyrthread
 *
 * armv7 0x00030fec, **2,124 bytes -- the largest function in the directory**,
 * and the one that makes a human-controlled fighter a human-controlled
 * fighter. `mk3_init` starts a thread here when bit 7 of the character number
 * is clear, and `t_local_reaction_exit` hands the fighter back here after every
 * hit.
 *
 * ## How it was read
 *
 * The dispatch is a compiled switch three levels deep with the bodies
 * interleaved between the branches, and reading that by eye gets a case wrong
 * eventually. It was not read by eye: `tools/dispatch.py` executes the compare
 * tree for every token and reports where each lands. **43 tokens over 36
 * bodies**, plus one refusal. The case list below is that output, not a
 * transcription.
 *
 * ## Four shared tails
 *
 * Almost every state ends in one of four places, and they are the file's whole
 * vocabulary:
 *
 *      RET          pop -- return whatever is in r0
 *      PARK1        token := 0x46d, fieldfc = 1, return 1
 *                   (sleep exactly one frame and come back at 0x46d)
 *      INSTALL      frame[frame].handler = H; frame[frame+1].w0 = 0; return 0
 *      PUSH_INSTALL the same, after thread->frame has already been advanced
 *
 * **`mk3_update` re-runs a thread whose handler returns 0**, so INSTALL means
 * "become this and act now", while PARK1's `return 1` ends the thread's frame.
 * The difference is one register and it is the difference between a state
 * change and a frame passing.
 *
 * ## What the states are
 *
 * Reading the tokens against what they install, the machine is:
 *
 *      0, 0x3a9      enter: mark "driven from outside", per-character begin
 *      0x3e1         stance table, arm 0x3e4
 *      0x3e4, 0x3e6  descend into t_wait_for_start
 *      0x3e8, 0x3eb  round start: GLBL_joy_entry := 0x3eb, buttons on,
 *                    reset_proc_stack, then read the stick
 *      0x3f8         clear 0x1c and the proc's 0x14, stance_setup
 *      0x3fd         am_i_facing_him? no -> t_turn_around, yes -> 0x404
 *      0x400,0x42e,
 *      0x5e3,0x5f2,
 *      0x63b         become t_local_reaction_exit (NO stop_me_player: that is 0x49a)
 *      0x403         token := 0x404, push, become t_check_winner_status
 *      0x404         G[0x44e] -- the round timer -- zero means wait
 *      0x408         check_block_bit: blocking -> t_joy_block
 *      0x417         next_anirate, then the 0x3fd facing check
 *      0x424         face_opponent, buttons off, arm 0x427
 *      0x427         bt_jump, distance_from_ground, descend t_do_jump_up
 *      0x43a, 0x446  the two walk directions
 *      0x44c, 0x455  pick the walk info, forward or back
 *      0x45c         call obj->field30 -- the walk routine -- then animate
 *      0x46c         PARK1 again
 *      0x46d         token := 0x471, push, become t_check_winner_status
 *      0x471, 0x478,
 *      0x479, 0x47b  the walk loop and its flip check
 *      0x49a, 0x4a7  reduce_turbo_bar, stop
 *      0x4a0, 0x4a3,
 *      0x4a4, 0x4a5  run: run_setup and next_anirate
 *      0x4b5         compare the walk routine against the stick
 *      0x5dc         the 0x40000/0x70000 pair and a descend into t_do_flip
 *      0x5eb         set 0x1a/0x1b, -4.0, descend into t_do_flip (the pop is
 *                    in the diagonal reads that come before it, not here)
 *      0x5fd         read the stick and branch on up+down / up+left
 *      0x617         count 0x48 down, then pop
 *      0x61f         read the stick (the push is in 0x4a4 / 0x478 before it)
 *
 * ## Two findings worth carrying out of here
 *
 * **`obj->field30` holds the fighter's WALK ROUTINE, and the state machine
 * calls it indirectly.** State 0x45c does `blx obj->field30` -- the only
 * indirect call in the function -- and states 0x44c/0x455 choose between
 * `get_walk_info_f` and `get_walk_info_b` to fill it. So walking forward and
 * walking backward are the same state with a different function pointer, and
 * 0x4b5 decides whether to keep it by comparing it against what the stick now
 * says. That is the same handover shape `obj->field34` has for projectiles.
 *
 * **The stick is tested as bit PAIRS, not as four directions.** 0x5fd masks
 * with 9 and then with 5 -- bits 0 and 3, then bits 0 and 2 -- and branches
 * only when the whole pair is present. `Playback_Update` established that bits
 * 2 and 3 are the horizontal pair; so `& 9` and `& 5` are "up and one of the
 * two horizontals", which is a diagonal. The jump states are reached from
 * exactly there.
 */
long t_turn_around(MK3THREAD *thread);        /* slot 0x000f3844 */
long t_wait_for_start(MK3THREAD *thread);     /* slot 0x000f388c */
long t_do_jump_up(MK3THREAD *thread);         /* slot 0x000f3854 */
long t_check_winner_status(MK3THREAD *thread);
long t_do_flip(MK3THREAD *thread);
long t_joy_block(MK3THREAD *thread);
long t_walk_flip_check(MK3THREAD *thread);
long t_joy_down(MK3THREAD *thread);
long t_joy_duck_entry(MK3THREAD *thread);

void get_walk_info_f(MK3OBJ *obj);            /* slot 0x000f37d4 */
void get_walk_info_b(MK3OBJ *obj);            /* slot 0x000f37d0 */

void init_anirate(MK3OBJ *obj);
void set_x_vel_player(MK3OBJ *obj);
void get_char_ani(MK3OBJ *obj);
void stance_setup(MK3OBJ *obj);
long am_i_facing_him(MK3OBJ *obj);
long next_anirate(MK3OBJ *obj);
void run_setup(MK3OBJ *obj);
void distance_from_ground(MK3OBJ *obj);
void face_opponent(MK3OBJ *obj);
void get_my_height(MK3OBJ *obj);            /* returns r0 = field20 too */
void reduce_turbo_bar(MK3OBJ *obj);
/* `is_run_pressed` is defined earlier in this file; it answers in
 * `obj->field5c`, the boolean slot, and returns it as well. */
long is_run_pressed(MK3OBJ *obj);

/* The four tails, as the binary spells them. `install` is the common one; the
 * caller has already decided which frame index it means. */
static long plyr_install(MK3THREAD *t, uint32_t frame, const void *handler)
{
    mk3_frame(t, frame)[1] = (uint32_t)(uintptr_t)handler;
    *mk3_frame(t, frame + 1) = 0;
    return 0;
}

static long plyr_park1(MK3THREAD *t)
{
    *mk3_frame(t, t->frame + 1) = 0x46d;
    t->fieldfc = 1;
    return 1;
}

long plyrthread(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    uint32_t bits;

    /* The dispatch, as `tools/dispatch.py` executes it: token -> body address.
     * Every label below is the address of the body in the binary, and every
     * body ends in a return or a goto, so what follows reads the way the
     * machine code runs. Several bodies share tails; a shared tail is one
     * label here because it is one physical site there. */
    switch (token) {
    case 0x5fd:  goto L31046;
    case 0x43a:  goto L310b8;
    case 0x4a0:  goto L310f6;
    case 0x4a3:  goto L31100;
    case 0x45c:  goto L31112;
    case 0x46c:  goto L31142;
    case 0x3e8:
    case 0x3eb:  goto L311a6;
    case 0x3f8:  goto L311ce;
    case 0x3fd:  goto L311dc;
    case 0x403:  goto L3120a;
    case 0x5eb:  goto L31264;
    case 0:
    case 0x3a9:  goto L312ce;
    case 0x3e1:  goto L312de;
    case 0x4a7:  goto L3131e;
    case 0x49a:  goto L3132c;
    case 0x400:
    case 0x42e:
    case 0x5e3:
    case 0x5f2:
    case 0x63b:  goto L31336;
    case 0x455:  goto L3135c;
    case 0x3e4:
    case 0x3e6:  goto L31372;
    case 0x47b:  goto L31392;
    case 0x617:  goto L313a8;
    case 0x46d:  goto L313c8;
    case 0x404:  goto L313e4;
    case 0x427:  goto L3140e;
    case 0x408:  goto L31448;
    case 0x479:  goto L31460;
    case 0x4a5:  goto L3147c;
    case 0x5dc:  goto L314d0;
    case 0x4b5:  goto L31506;
    case 0x4a4:  goto L31538;
    case 0x61f:  goto L3155c;
    case 0x424:  goto L315f4;
    case 0x417:  goto L31630;
    case 0x478:  goto L31648;
    case 0x471:  goto L31654;
    case 0x446:  goto L316b4;
    case 0x44c:  goto L31760;
    default:     return -3;
    }

    /* ----------------------------------------------------------- 0x5fd, 0x617 */
L31046:                                 /* 0x5fd: the diagonal read, counted */
    obj->field48 = 3;
L3104a:                                 /* 0x617 comes back here, still counting */
    joystick_in_a0(obj);
    bits = obj->field1c;
    obj->field20 = bits;
    obj->field1c = bits & 9u;
    if ((bits & 9u) == 9u)
        goto L314a4;
    bits &= 5u;
    obj->field20 = bits;
    if (bits == 5u)
        goto L31238;
    *mk3_frame(thread, thread->frame + 1) = 0x617;
    thread->fieldfc = 1;
    return 1;

    /* ----------------------------------------------------- walking, 0x43a... */
L310b8:                                 /* 0x43a, and the back-walk of the stick read */
    obj->field2c = (*(const uint32_t *)(const void *)
                      ((const char *)(const void *)obj->field08 + 0x28)
                    & 0x10u) ^ 0x10u;
    obj->field1c = 4;
    goto L310cc;

L316b4:                                 /* 0x446 and the forward walk */
    obj->field2c = *(const uint32_t *)(const void *)
                      ((const char *)(const void *)obj->field08 + 0x28)
                   & 0x10u;
    obj->field1c = 8;
    goto L310cc;

L31760:                                 /* 0x44c */
L310cc:
    obj->field00->field34 = obj->field1c;   /* the direction mask */
    obj->field24 = 0;
    obj->field00->field18 = 0;
    obj->field40 = 1;
    if (obj->field2c != 0)
        goto L31352;
    obj->field30 = (uint32_t)(uintptr_t)get_walk_info_f;
L310ee:
    if (is_run_pressed(obj) == 0)
        goto L31112;

    /* 0x4a0 comes in here; 0x4a3 comes in one step later, without the setup */
L310f6:
    run_setup(obj);
L31100:
    *mk3_frame(thread, thread->frame + 1) = 0x4a4;
    thread->fieldfc = 1;
    return 1;

    /* **The one indirect call in the function**: whatever 0x30 holds is the
     * walk routine, and 0x44c / 0x455 chose it. */
L31112:                                 /* 0x45c */
    ((void (*)(MK3OBJ *))(uintptr_t)obj->field30)(obj);
    obj->field40 = obj->field24;
    init_anirate(obj);
    obj->field1c = obj->field20;
    set_x_vel_player(obj);
    obj->field00->field28 = obj->field40;
    get_char_ani(obj);
    obj->field00->field24 = obj->field40;
L31142:                                 /* 0x46c, and everything that parks one frame */
    *mk3_frame(thread, thread->frame + 1) = 0x46d;
    thread->fieldfc = 1;
    return 1;

    /* ---------------------------------------------------------- the round start */
L311a6:                                 /* 0x3e8, 0x3eb */
    GLBL_joy_entry = 0x3eb;             /* where a reaction comes back to */
    enable_all_buttons(obj);
    reset_proc_stack(thread);
    joystick_in_a0(obj);
    if ((obj->field1c & 2u) != 0)
        goto L316ec;
L311ce:                                 /* 0x3f8 */
    obj->field1c = 0;
    obj->field00->field14 = 0;
    stance_setup(obj);
L311dc:                                 /* 0x3fd, 0x417's second half */
    if (am_i_facing_him(obj) == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x400;
        thread->frame = thread->frame + 1;
        return plyr_install(thread, thread->frame,
                            (const void *)t_turn_around);
    }
L3120a:                                 /* 0x403 */
    *mk3_frame(thread, thread->frame + 1) = 0x404;
    thread->frame = thread->frame + 1;
    return plyr_install(thread, thread->frame,
                        (const void *)t_check_winner_status);

    /* The pop: a fighter that has been pushed one level goes back down and
     * the pair above it is slid down with it. Reached from the diagonal
     * reads, and from nowhere else. */
L31238:
    if ((long)thread->frame <= 0)
        goto L31780;
L31242:
    thread->frame = thread->frame - 1;
L31248:
    {
        uint32_t f = thread->frame;
        uint32_t h = mk3_frame(thread, f + 1)[1];

        *mk3_frame(thread, f + 1) = *mk3_frame(thread, f + 2);
        mk3_frame(thread, f)[1] = h;
    }
L31264:                                 /* 0x5eb: the second flip, without the pop */
    obj->field20 = 0x1a;
    obj->field1c = 0x1a + 1;
    obj->field34 = 0;
    /* `ldr.w r3, [pc, #0x560]` at 0x0003126c, so the literal is at
     * Align(0x31270,4) + 0x560 = 0x000317d0, and the word there is
     * 0xfffc0000 -- the exact mirror of the +0x40000 the first flip loads. */
    obj->field48 = 0xfffc0000u;             /* -4.0 in 16.16 */
    *mk3_frame(thread, thread->frame + 1) = 0x5f2;
    thread->frame = thread->frame + 1;
    return plyr_install(thread, thread->frame, (const void *)t_do_flip);

    /* --------------------------------------------------------------- the entry */
L312ce:                                 /* 0, 0x3a9 */
    obj->field00->field10 |= 1;         /* driven from outside */
    ochar_begin_calls(obj);
L312de:                                 /* 0x3e1 */
    stuff_buttons(obj, (uint32_t)(uintptr_t)&bt_stance);
    obj->field00->field10 |= 1;
    *mk3_frame(thread, thread->frame + 1) = 0x3e4;
    thread->fieldfc = 1;
    return 1;

    /* ------------------------------------------------------------ the bars, stop */
L3131e:                                 /* 0x4a7 */
    reduce_turbo_bar(obj);
    if (obj->field1c != 0)
        goto L316c6;
L3132c:                                 /* 0x49a, and every way a walk ends */
    stop_me_player(obj);
L31336:                                 /* 0x400, 0x42e, 0x5e3, 0x5f2, 0x63b: no stop */
    return plyr_install(thread, thread->frame,
                        (const void *)t_local_reaction_exit);

    /* 0x455 comes in at the compare; the zero-direction path has just set
     * the backward routine and joins it. */
L31352:
    obj->field30 = (uint32_t)(uintptr_t)get_walk_info_b;
L3135c:
    if (obj->field30 != (uint32_t)(uintptr_t)get_walk_info_f)
        goto L31112;
    goto L310ee;

L31372:                                 /* 0x3e4, 0x3e6 */
    *mk3_frame(thread, thread->frame + 1) = 0x3e8;
    thread->frame = thread->frame + 1;
    return plyr_install(thread, thread->frame,
                        (const void *)t_wait_for_start);

L31392:                                 /* 0x47b */
    next_anirate(obj);
    mask_joystick(obj);
    if (obj->field1c == 0)
        goto L3132c;
    goto L31142;

L313a8:                                 /* 0x617: count down, then pop */
    obj->field48 = obj->field48 - 1;
    if ((long)obj->field48 > 0)
        goto L3104a;
    if ((long)thread->frame <= 0)
        goto L31786;
L313be:
    thread->frame = thread->frame - 1;
    return 0;

L313c8:                                 /* 0x46d: run the winner check, come back at 0x471 */
    *mk3_frame(thread, thread->frame + 1) = 0x471;
    thread->frame = thread->frame + 1;
    return plyr_install(thread, thread->frame,
                        (const void *)t_check_winner_status);

    /* **The round timer.** G + 0x44e is a halfword; zero means the round has
     * not started and the fighter goes back to waiting. */
L313e4:                                 /* 0x404 */
    obj->field1c = (uint32_t)(long)
        *(const int16_t *)(const void *)(G_BYTES + 0x44e);
    if (*(const uint16_t *)(const void *)(G_BYTES + 0x44e) == 0)
        goto L31372;
    *mk3_frame(thread, thread->frame + 1) = 0x408;
    thread->fieldfc = 1;
    return 1;

L3140e:                                 /* 0x427: the jump */
    stuff_buttons(obj, (uint32_t)(uintptr_t)&bt_jump);
    distance_from_ground(obj);
    obj->field48 = 0;
    obj->a10 = obj->field1c;
    *mk3_frame(thread, thread->frame + 1) = 0x42e;
    thread->frame = thread->frame + 1;
    return plyr_install(thread, thread->frame, (const void *)t_do_jump_up);

L31448:                                 /* 0x408 */
    if (check_block_bit(obj) != 0)
        return plyr_install(thread, thread->frame, (const void *)t_joy_block);

    /* ---------------------------------------------------------- the stick read */
    joystick_in_a0(obj);
    bits = obj->field1c;
    if ((bits & 8u) != 0 && (int16_t)obj->field00->field7e == 0)
        goto L316b4;
    if ((bits & 4u) != 0 && (int16_t)obj->field00->field7e == 0)
        goto L310b8;
    if ((bits & 1u) != 0)
        goto L315f4;
    if ((bits & 2u) == 0)
        goto L3178e;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_joy_down;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;

L31460:                                 /* 0x479 */
    *mk3_frame(thread, thread->frame + 1) = 0x47b;
    thread->frame = thread->frame + 1;
    return plyr_install(thread, thread->frame,
                        (const void *)t_walk_flip_check);

L3147c:                                 /* 0x4a5 */
    next_anirate(obj);
    *mk3_frame(thread, thread->frame + 1) = 0x4a7;
    thread->frame = thread->frame + 1;
    return plyr_install(thread, thread->frame,
                        (const void *)t_check_winner_status);

    /* The 9 pair: up and the first horizontal. The same pop, then the first
     * flip with its 0x40000 / 0x70000 pair. */
L314a4:
    if ((long)thread->frame <= 0)
        goto L31758;
L314ae:
    thread->frame = thread->frame - 1;
L314b4:
    {
        uint32_t f = thread->frame;
        uint32_t h = mk3_frame(thread, f + 1)[1];

        *mk3_frame(thread, f + 1) = *mk3_frame(thread, f + 2);
        mk3_frame(thread, f)[1] = h;
    }
L314d0:                                 /* 0x5dc: **the 0x40000 / 0x70000 pair, from one
                                         * register** -- `mov #0x40000` then `add #0x30000` */
    obj->field48 = 0x40000;
    obj->field34 = 0x40000 + 0x30000;
    obj->field1c = 0x1a;
    obj->field20 = 0x1a + 1;
    *mk3_frame(thread, thread->frame + 1) = 0x5e3;
    thread->frame = thread->frame + 1;
    return plyr_install(thread, thread->frame, (const void *)t_do_flip);

    /* **Has the stick changed since the walk routine was chosen?** 0x30 still
     * holds it, and the fighter's own 0x18 says what it is doing now. A fresh
     * run joins at 0x31100, NOT at the setup: the run is already set up. */
L31506:                                 /* 0x4b5 */
    obj->field1c = *(const uint32_t *)(const void *)
                      ((const char *)(const void *)obj->field08 + 0x18);
    if (obj->field1c == obj->field30)
        goto L3132c;
    mask_joystick(obj);
    if (obj->field1c == 0)
        goto L3132c;
    goto L31100;

L31538:                                 /* 0x4a4 */
    *mk3_frame(thread, thread->frame + 1) = 0x4a5;
L3153c:                                 /* push a level, copy the handler down */
    thread->frame = thread->frame + 1;
    *mk3_frame(thread, thread->frame + 1) = 0;
    mk3_frame(thread, thread->frame)[1] =
        mk3_frame(thread, thread->frame - 1)[1];

L3155c:                                 /* 0x61f: the read with no counter */
    joystick_in_a0(obj);
    bits = obj->field1c;
    obj->field20 = bits;
    obj->field1c = bits & 9u;
    if ((bits & 9u) == 9u)
        goto L3158e;
    bits &= 5u;
    obj->field20 = bits;
    if (bits == 5u)
        goto L315ae;
    if ((long)thread->frame > 0)
        goto L313be;
    return plyr_install(thread, thread->frame,
                        (const void *)t_local_reaction_exit);

L3158e:
    if ((long)thread->frame > 0)
        goto L314ae;
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_local_reaction_exit;
    *mk3_frame(thread, thread->frame + 1) = 0;
    goto L314b4;

L315ae:
    if ((long)thread->frame > 0)
        goto L31242;
L315b8:
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_local_reaction_exit;
    *mk3_frame(thread, thread->frame + 1) = 0;
    goto L31248;

L315f4:                                 /* 0x424: the jump, from the stick */
    face_opponent(obj);
    disable_all_buttons(obj);
    *mk3_frame(thread, thread->frame + 1) = 0x427;
    thread->frame = thread->frame + 1;
    *mk3_frame(thread, thread->frame + 1) = 0;
    mk3_frame(thread, thread->frame)[1] =
        mk3_frame(thread, thread->frame - 1)[1];
    goto L31046;

L31630:                                 /* 0x417 */
    next_anirate(obj);
    goto L311dc;

L31648:                                 /* 0x478 */
    *mk3_frame(thread, thread->frame + 1) = 0x479;
    goto L3153c;

L31654:                                 /* 0x471 */
    obj->field1c = obj->field00->field28;
    if (obj->field00->field28 != 1)
        goto L31648;
    if (is_run_pressed(obj) != 0)
        goto L310f6;
    goto L31648;

L316c6:                                 /* turbo run: -8.0 in 16.16 */
    obj->field30 = 0xfff80000u;
    joystick_in_a0(obj);
    bits = obj->field1c;
    if ((bits & 8u) != 0)
        goto L31506;
    if ((bits & 4u) != 0) {
        obj->field30 = (uint32_t)(-(long)obj->field30);
        goto L31506;
    }
    goto L3132c;

L316ec:                                 /* down on the round start: how tall are we? */
    get_my_height(obj);
    if ((int32_t)obj->field20 <= 0x60)
        goto L31772;
    return plyr_install(thread, thread->frame, (const void *)t_joy_down);

L31758:
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_local_reaction_exit;
    *mk3_frame(thread, thread->frame + 1) = 0;
    goto L314b4;

L31772:
    return plyr_install(thread, thread->frame,
                        (const void *)t_joy_duck_entry);

L31780:
    goto L315b8;

L31786:
    return plyr_install(thread, thread->frame,
                        (const void *)t_local_reaction_exit);

L3178e:                                 /* nothing pressed: the shang check */
    *mk3_frame(thread, thread->frame + 1) = 0x417;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_back_to_shang_check;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ======================================================================
 * The six that were still pending in this file.
 *
 * Transcribed instruction by instruction from the annotated armv7
 * disassembly. Two of them reach the globals through the pointer slots that
 * happen to sit at `bt_jump + 0x28` and `bt_jump + 0x2c`; those slots hold
 * `G` and `H`, which this file already names, so they are spelled that way
 * here rather than as pointer arithmetic off a button table they have
 * nothing to do with.
 * ====================================================================== */

long strike_check_a0(MK3OBJ *obj);
void get_bcq_next_pointer_idx(MK3OBJ *obj, long which);
void previous_q_entry(MK3OBJ *obj);
void MKEvent_Add(long a, long b, long c, long d);
void turbo_bar_setup(MK3OBJ *obj);
long t_joyd4(struct MK3THREAD *thread);
long t_jhp4(struct MK3THREAD *thread);
long t_jmp4(struct MK3THREAD *thread);
void get_char_ani(MK3OBJ *obj);
void do_next_a9_frame(MK3OBJ *obj);
void find_ani_part2(MK3OBJ *obj);
void find_part2(MK3OBJ *obj);
long t_stat_do_roundhouse(struct MK3THREAD *thread);   /* GOT 0x000f37f0 */
long t_do_knee(struct MK3THREAD *thread);              /* GOT 0x000f38b4 */
void get_x_dist(MK3OBJ *obj);
long is_he_airborn(MK3OBJ *obj);
long t_stat_do_hi_kick(struct MK3THREAD *thread);      /* GOT 0x000f38c4 */
long t_stat_do_lo_kick(struct MK3THREAD *thread);      /* GOT 0x000f3858 */
long t_joy_sweep_kick(struct MK3THREAD *thread);
long is_stick_away(MK3OBJ *obj);
void disable_all_buttons(MK3OBJ *obj);
long t_do_elbow(struct MK3THREAD *thread);             /* GOT 0x000f3888 */
long t_joy_lo_punch(struct MK3THREAD *thread);
long t_joy_toss(struct MK3THREAD *thread);
void q_is_he_a_boss(MK3OBJ *obj);
long am_i_facing_him(MK3OBJ *obj);
void get_his_action(MK3OBJ *obj);
void is_he_joy(MK3OBJ *obj);
void get_my_dfe(MK3OBJ *obj);
long is_he_right(MK3OBJ *obj);   /* returns r0 AND stores field5c */
long am_i_joy(MK3OBJ *obj);      /* likewise */
void find_last_frame(MK3OBJ *obj); /* 0x00055428, NOT find_ani_last_frame */
long t_joyd5(struct MK3THREAD *thread);
long t_player_1_wins(struct MK3THREAD *thread);
long t_player_2_wins(struct MK3THREAD *thread);
long t_finish_him(struct MK3THREAD *thread);
long t_do_jumpup_kick(struct MK3THREAD *thread);
long t_do_flip_punch(struct MK3THREAD *thread);
long t_flight_call(struct MK3THREAD *thread);
long t_angle_jump_land_jsrp(struct MK3THREAD *thread);
long t_angle_jump_call(struct MK3THREAD *thread);
void call_for_him(MK3OBJ *obj, void (*fn)(MK3OBJ *));
uint32_t random32(void);
void stop_me_player(MK3OBJ *obj);
void me_in_front(MK3OBJ *obj);
long t_do_block_hi(struct MK3THREAD *thread);         /* GOT 0x000f37d8 */
long t_do_unblock_hi(struct MK3THREAD *thread);       /* GOT 0x000f38a0 */
long t_duck_turnaround(struct MK3THREAD *thread);     /* GOT 0x000f38a4 */
long t_backwards_ani(struct MK3THREAD *thread);       /* GOT 0x000f37c4 */
long t_joy_block_loop(struct MK3THREAD *thread);
long t_joy_duck_block_loop(struct MK3THREAD *thread);
long t_jdblk2(struct MK3THREAD *thread);
long t_joy_back_up(struct MK3THREAD *thread);
long t_joy_down(struct MK3THREAD *thread);
long t_turn_around(struct MK3THREAD *thread);         /* GOT 0x000f3844 */
long t_check_winner_status(struct MK3THREAD *thread);
void face_opponent(MK3OBJ *obj);
void find_ani_last_frame(MK3OBJ *obj);
void inc_downcount(MK3OBJ *obj);
long t_stat_do_duck_punch(struct MK3THREAD *thread);   /* GOT 0x000f38b8 */
long t_stat_do_duck_kickh(struct MK3THREAD *thread);   /* GOT 0x000f3894 */
long t_stat_do_duck_kickl(struct MK3THREAD *thread);   /* GOT 0x000f389c */
long t_retract_strike(struct MK3THREAD *thread);       /* GOT 0x000f38c8 */
long t_post_joy_duck_kick(struct MK3THREAD *thread);
long t_act_mframew(struct MK3THREAD *thread);          /* GOT 0x000f37e8 */
long t_punch_sleep(struct MK3THREAD *thread);
long t_jhp5(struct MK3THREAD *thread);
long t_jmp5(struct MK3THREAD *thread);
long t_joy_un_hi_punch1(struct MK3THREAD *thread);
long t_joy_un_hi_punch2(struct MK3THREAD *thread);
long t_joy_un_lo_punch1(struct MK3THREAD *thread);
long t_joy_un_lo_punch2(struct MK3THREAD *thread);
long t_joy_punch_htm1(struct MK3THREAD *thread);
long t_joy_punch_mth1(struct MK3THREAD *thread);
void group_sound(MK3OBJ *obj);
/* rsnd_func is declared in mk3logic.h */

extern const void *P32 bt_duck;                /* 0x001655d4 */


/* ------------------------------------------------------------ shang_begin
 *
 * armv7 0x0002f47c, sixteen bytes.
 *
 *      v = proc->field10 | 0x200
 *      obj->field2c = v ; proc->field10 = v
 *
 * One flag, set in both the object's scratch and the proc it came from --
 * the same write-it-twice shape `reaction_start_chores` uses, and the reason
 * the scratch copy exists at all is that the caller reads it back.
 */
void shang_begin(MK3OBJ *obj)
{
    MK3OBJPROC *proc = obj->field00;
    uint32_t v = proc->field10 | 0x200u;

    obj->field2c = v;
    proc->field10 = v;
}


/* ------------------------------------------------------- punch_strike_check
 *
 * armv7 0x0002f7d8, twenty-four bytes.
 *
 *      strike_check_a0(obj)
 *      if (obj->field5c != 0) obj->field48 = -1
 *
 * `field5c` is the engine's "did that connect" flag -- `t_attk2` reads the
 * same one. The -1 into field48 is what stops the swing checking again: the
 * strike id becomes invalid, so a punch lands once however many frames it
 * stays out.
 */
void punch_strike_check(MK3OBJ *obj)
{
    strike_check_a0(obj);
    if (obj->field5c != 0)
        obj->field48 = (uint32_t)-1;
}


/* --------------------------------------------------------- get_last_button
 *
 * armv7 0x000308c4, twenty-four bytes.
 *
 *      get_bcq_next_pointer_idx(obj, obj->field00->field08)
 *      previous_q_entry(obj)
 *
 * field08 is the player index, and the bcq is the per-player button ring G
 * carries at 0x0c0 / 0x218. Walking back one entry is how a move asks what
 * was pressed before the button that started it.
 */
void get_last_button(MK3OBJ *obj)
{
    get_bcq_next_pointer_idx(obj, (long)obj->field00->field08);
    previous_q_entry(obj);
}


/* ---------------------------------------------------------- turbo_bar_setup
 *
 * armv7 0x0002f3e4, forty-four bytes.
 *
 *      p = obj->field00->field08                  ; the player index
 *      obj->field30 = &G[0x378 + p * 4]           ; the run bar itself
 *      obj->field34 = &G[0x388 + p * 4]           ; its lockout counter
 *
 * Two pointers, parked in the object so the three routines that move the bar
 * do not each have to compute them. Which array is which is settled by the
 * three that use them: `is_run_pressed` refuses when 0x378 is zero and writes
 * 40 into 0x388 on the way out, `reduce_turbo_bar` takes one off 0x378 a
 * frame while holding 0x388 at 40, and `RaiseTurboBars` counts 0x388 down
 * and only then puts 0x378 back, stopping at 48.
 */
void turbo_bar_setup(MK3OBJ *obj)
{
    uint32_t p = obj->field00->field08;

    obj->field30 = (uint32_t)(uintptr_t)(G_BYTES + 0x378 + p * 4);
    obj->field34 = (uint32_t)(uintptr_t)(G_BYTES + 0x388 + p * 4);
}


/* ------------------------------------------------------------------ t_joyd3
 *
 * armv7 0x0002ee30, seventy-two bytes.
 *
 *      if (frame[frame+1] != 0) return -3
 *      stuff_buttons(obj, &bt_duck)
 *      install t_joyd4
 *
 * The duck's button table goes in before the state that reads it, which is
 * the same order `plyrthread` installs `bt_stance`.
 */
long t_joyd3(struct MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;
    stuff_buttons(obj, (uint32_t)(uintptr_t)&bt_duck);
    return mk3_install(thread, (MK3THREADFUNC)t_joyd4);
}


/* --------------------------------------------------------- zero_turbo_bar
 *
 * armv7 0x0003087c, seventy-two bytes.
 *
 *      v = (int16_t)H[0x18] ; obj->field20 = v
 *      if (v != 0) return                        ; frozen: do nothing
 *      turbo_bar_setup(obj)
 *      *obj->field34 = 0x28                      ; lock it out for 40 frames
 *      obj->field1c = 0 ; *obj->field30 = 0      ; and empty it
 *      MKEvent_Add(3, 5, G[0x378 + p * 4], 0)    ; tell the HUD
 *
 * `H[0x18]` is the same halfword `reduce_turbo_bar` opens with, and it gates
 * both the same way: while it is non-zero the bar does not move at all. So
 * this is "empty the run bar now" -- the bar to nothing and the lockout to
 * its full 40 -- and the event is the HUD being told the new value, which is
 * zero.
 */
void zero_turbo_bar(MK3OBJ *obj)
{
    int32_t v = *(const int16_t *)(const void *)(H + 0x18);

    obj->field20 = (uint32_t)v;
    if (v != 0)
        return;

    turbo_bar_setup(obj);
    *(uint32_t *)(uintptr_t)obj->field34 = 0x28;
    obj->field1c = 0;
    *(uint32_t *)(uintptr_t)obj->field30 = 0;

    MKEvent_Add(3, 5,
                *(const uint32_t *)(const void *)
                    (G_BYTES + 0x378 + obj->field00->field08 * 4),
                0);
}


/* ---------------------------------------------------------- is_run_pressed
 *
 * armv7 0x0002f344, ninety-two bytes.
 *
 *      p = obj->field00->field08                 ; the player index
 *      obj->field28 = G[0x1c]                    ; the translated joy word
 *      obj->a10     = p ? 0x400000 : 0x40000     ; that player's run bit
 *      obj->field28 &= obj->a10
 *      if (obj->field28 == 0)  { obj->field5c = 0 ; return 0 }
 *      bar = G[0x378 + p * 4] ; obj->field1c = bar
 *      if (bar != 0)           { obj->field5c = 1 ; return 1 }
 *      obj->field1c = 0x28 ; G[0x388 + p * 4] = 0x28
 *      obj->field5c = 0 ; return 0
 *
 * Two questions in one function: is the button down, and is there anything
 * left to spend. The masks are the same bit eight places apart -- 0x40000 for
 * player one and 0x400000 for player two -- which is what `TranslateJoybits`
 * does to every button.
 *
 * The last branch is the one worth reading twice. Asking for a run on an
 * EMPTY bar does not merely refuse: it writes 40 into the lockout on the way
 * out. So hammering the button on an empty bar keeps it empty, because
 * `RaiseTurboBars` will not put anything back while that counter is running.
 */
long is_run_pressed(MK3OBJ *obj)
{
    uint32_t p = obj->field00->field08;

    obj->field28 = *(const uint32_t *)(const void *)(G_BYTES + 0x1c);
    obj->a10 = p ? 0x400000u : 0x40000u;     /* 0x44, the argument slot */
    obj->field28 = obj->field28 & obj->a10;
    if (obj->field28 == 0) {
        obj->field5c = 0;
        return 0;
    }

    {
        uint32_t bar =
            *(const uint32_t *)(const void *)(G_BYTES + 0x378 + p * 4);

        obj->field1c = bar;
        if (bar != 0) {
            obj->field5c = 1;
            return 1;
        }
    }

    obj->field1c = 0x28;
    *(uint32_t *)(void *)(G_BYTES + 0x388 + p * 4) = 0x28;
    obj->field5c = 0;
    return 0;
}


/* -------------------------------------------------------- reduce_turbo_bar
 *
 * armv7 0x00030820, ninety-two bytes.
 *
 *      obj->field1c = 1
 *      v = (int16_t)H[0x18] ; obj->field20 = v
 *      if (v != 0) return                        ; frozen
 *      turbo_bar_setup(obj)
 *      *obj->field34 = 0x28 ; obj->field1c = 0x28      ; hold the lockout AT 40
 *      bar = *obj->field30 ; obj->field1c = bar
 *      if (bar == 0) return
 *      bar -= 1 ; obj->field1c = bar ; *obj->field30 = bar
 *      MKEvent_Add(3, 5, G[0x378 + p * 4], 0)
 *
 * One unit a frame while the run is held -- and the lockout is written back
 * to its full 40 on EVERY one of those frames, not just at the end. That is
 * why an emptied bar feels so much worse than a half-used one: the forty
 * frames start counting from when you stop, not from when you ran out.
 *
 * `H[0x18]` is the same halfword `zero_turbo_bar` opens with. While it is
 * non-zero the bar does not move at all.
 */
void reduce_turbo_bar(MK3OBJ *obj)
{
    int32_t v;
    uint32_t bar;

    obj->field1c = 1;
    v = *(const int16_t *)(const void *)(H + 0x18);
    obj->field20 = (uint32_t)v;
    if (v != 0)
        return;

    turbo_bar_setup(obj);
    *(uint32_t *)(uintptr_t)obj->field34 = 0x28;
    obj->field1c = 0x28;

    bar = *(const uint32_t *)(uintptr_t)obj->field30;
    obj->field1c = bar;
    if (bar == 0)
        return;

    bar -= 1;
    obj->field1c = bar;
    *(uint32_t *)(uintptr_t)obj->field30 = bar;

    MKEvent_Add(3, 5,
                *(const uint32_t *)(const void *)
                    (G_BYTES + 0x378 + obj->field00->field08 * 4),
                0);
}


/* -------------------------------------------------------- t_joy_duck_entry
 *
 * armv7 0x000319c4, eighty-four bytes.
 *
 *      if (frame[frame+1] != 0) return -3
 *      obj->field40 = 4 ; get_char_ani(obj)      ; animation 4, SCDUCK
 *      obj->field40 += 8                         ; and skip two frames
 *      do_next_a9_frame(obj)
 *      install t_joyd3
 *
 * The `+= 8` is two stream words, and it is the whole character of ducking in
 * this game: going down does not play the crouch from its first frame, it
 * starts two in. A duck is fast because it skips the beginning of its own
 * animation, not because the animation is short.
 */
long t_joy_duck_entry(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field40 = 4;
    get_char_ani(obj);
    obj->field40 = obj->field40 + 8;
    do_next_a9_frame(obj);

    return mk3_install(thread, (MK3THREADFUNC)t_joyd3);
}


/* --------------------------------------------- t_joy_punch_htm2 / _mth2
 *
 * armv7 0x0002f528 and 0x0002f5f0, a hundred and four bytes each.
 *
 *      if (frame[frame+1] != 0) return -3
 *      obj->field40 = 0xe (htm) or 0xf (mth)     ; SCHIPUNCH / SCLOPUNCH
 *      find_ani_part2(obj)                       ; -> part 1
 *      find_part2(obj) x5                        ; -> part 6
 *      install t_jmp4 (htm) or t_jhp4 (mth)
 *
 * **A punch stream is not two parts, it is eight.** `find_part2` walks to the
 * next zero word, and these two walk six of them. Out of `nj_ani_data`,
 * animation 14:
 *
 *      part 0   5809 5812 5815          the jab going out
 *      part 1   5818 5821 5824          and coming back
 *      part 2   5821 5818 <jump>
 *      part 3   5812 5809
 *      part 4   5827
 *      part 5   5818 3137 <jump>
 *      part 6   5821 3134 <jump>        <- where these two land
 *      part 7   3132 3134 3135          which are animation 15's frames
 *
 * Part 6 holds one frame of the high punch and one of the LOW punch, and part
 * 7 is the low punch's opening outright. So the back half of the stream is a
 * combo graph: the transition frames that carry one punch into the next, with
 * an opcode-1 jump at the end of each to say where to continue.
 *
 * Which is exactly what these two do. `htm` -- high to mid -- takes the high
 * punch's stream, walks to its transition part, and hands the fighter to
 * `t_jmp4`, the LOW punch's swing. `mth` does the mirror. Neither is a move a
 * player can ask for; they are reached from the combo checker, and they are
 * the reason a two-punch string looks like one motion rather than two.
 */
long t_joy_punch_htm2(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field40 = 0xe;
    find_ani_part2(obj);
    /* unrolled, as the binary has it: 5 calls */
    find_part2(obj);
    find_part2(obj);
    find_part2(obj);
    find_part2(obj);
    find_part2(obj);

    return mk3_install(thread, (MK3THREADFUNC)t_jmp4);
}


long t_joy_punch_mth2(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field40 = 0xf;
    find_ani_part2(obj);
    /* unrolled, as the binary has it: 5 calls */
    find_part2(obj);
    find_part2(obj);
    find_part2(obj);
    find_part2(obj);
    find_part2(obj);

    return mk3_install(thread, (MK3THREADFUNC)t_jhp4);
}


/* --------------------------------------------------------- t_joy_roundhouse
 *
 * armv7 0x0002f148, a hundred and eight bytes.
 *
 *      state 0     push t_stat_do_roundhouse, return token 0x210
 *      state 0x210 install t_local_reaction_exit
 *      anything else  -3
 *
 * The plainest shape in the file: call one thing, then leave. Holding the
 * stick away turns a high kick into this, and `is_stick_away` decides that in
 * `t_joy_hi_kick` before either is reached.
 */
long t_joy_roundhouse(MK3THREAD *thread)
{
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x210;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_stat_do_roundhouse;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x210)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* ------------------------------------------------------------ t_knee_check
 *
 * armv7 0x0002f9d4, two hundred and fifty-six bytes.
 *
 *      state 0
 *          get_x_dist(obj)
 *          if (obj->field28 > 0x4a)        -> not a knee: pop and leave
 *          is_he_airborn(obj)
 *          if (airborne)                   -> not a knee: pop and leave
 *          -- close, and on the ground --
 *          pop one level, shuffle the level above down into it,
 *          then push t_do_knee with return token 0x1fe
 *      state 0x1fe   install t_local_reaction_exit
 *
 * **Seventy-four units, and what happens either side of it.** `get_x_dist`
 * (0x0002f3a0) is centre to centre with no boxes involved, and 0x4a is 74.
 * Inside that, a kick is not a kick: it is a knee. Outside it -- or against an
 * opponent who is off the ground -- this proc removes itself and the kick it
 * was checking carries on as a kick.
 *
 * The stack surgery in the middle is the part worth reading slowly. It does
 * not push the knee on top of the kick; it pops a level, moves the frame ABOVE
 * down into the one it just vacated, and only then pushes. The check erases
 * itself from the stack, so when the knee finishes it returns to whoever asked
 * for the kick rather than to a checker that has nothing left to do.
 *
 * The branch at 0x0002faa8 is the same thing at the bottom of the stack: with
 * no level below to shuffle down, it installs `t_local_reaction_exit` as the
 * caller first and then performs the identical shuffle. There is always
 * something to return to.
 */
long t_knee_check(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    uint32_t below, carried;

    if (token != 0) {
        if (token != 0x1fe)
            return -3;
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }

    get_x_dist(obj);
    if ((long)obj->field28 > 0x4a)
        goto not_a_knee;

    if (is_he_airborn(obj))
        goto not_a_knee;

    /* Close, and he is on the ground. */
    if ((long)thread->frame <= 0) {
        /* Nothing below to return to: make one. */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_local_reaction_exit;
        *mk3_frame(thread, thread->frame + 1) = 0;
    } else {
        thread->frame = thread->frame - 1;
    }

    /* Shuffle the level above down into this one, so the check leaves no
     * trace on the stack, then push the knee. */
    below   = mk3_frame(thread, thread->frame + 1)[1];
    carried = *mk3_frame(thread, thread->frame + 2);
    *mk3_frame(thread, thread->frame + 1) = carried;
    mk3_frame(thread, thread->frame)[1] = below;
    *mk3_frame(thread, thread->frame + 1) = 0x1fe;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_do_knee;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;

not_a_knee:
    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
        return 0;
    }
    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* ------------------------------------------------ t_joy_hi_kick / t_joy_lo_kick
 *
 * armv7 0x0002fe34 and 0x0002fd58.
 *
 * Two buttons, and between them four different attacks. Both ask the same two
 * questions -- is the stick held away, and is he close enough for a knee --
 * but **they ask them in opposite orders, and that is not a detail.**
 *
 *      t_joy_hi_kick                        t_joy_lo_kick
 *      -------------                        -------------
 *      disable_all_buttons                  disable_all_buttons
 *      is_stick_away ?                      push t_knee_check       (0x236)
 *          -> t_joy_roundhouse              |
 *      push t_knee_check       (0x21a)      returns 0x236:
 *      returns 0x21a:                           is_stick_away ?
 *          push t_stat_do_hi_kick (0x21e)           -> t_joy_sweep_kick
 *      returns 0x21e:                           push t_stat_do_lo_kick (0x23d)
 *          t_local_reaction_exit            returns 0x23d:
 *                                               t_local_reaction_exit
 *
 * So on the HIGH kick "away" wins over proximity: hold back next to somebody
 * and you get a roundhouse, because the knee is never asked about. On the LOW
 * kick proximity wins over "away": hold back next to somebody and you get a
 * KNEE, not a sweep, because the knee check runs first and -- when it accepts
 * -- erases itself and never comes back to be asked about the stick.
 *
 * The sweep is therefore a move you can only get from outside seventy-four
 * units, and the roundhouse is one you can get from anywhere.
 *
 * `t_knee_check` returning its token at all IS the refusal. When it accepts it
 * rewrites the stack beneath itself and this proc is never re-entered.
 */
long t_joy_hi_kick(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x21a) {
        /* The knee check declined. Throw the kick itself. */
        *mk3_frame(thread, thread->frame + 1) = 0x21e;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_stat_do_hi_kick;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x21e)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

    if (token != 0)
        return -3;

    disable_all_buttons(obj);

    if (is_stick_away(obj))
        return mk3_install(thread, (MK3THREADFUNC)t_joy_roundhouse);

    *mk3_frame(thread, thread->frame + 1) = 0x21a;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_knee_check;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


long t_joy_lo_kick(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x236) {
        /* The knee check declined. Only now does the stick matter. */
        if (is_stick_away(obj))
            return mk3_install(thread, (MK3THREADFUNC)t_joy_sweep_kick);

        *mk3_frame(thread, thread->frame + 1) = 0x23d;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_stat_do_lo_kick;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x23d)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

    if (token != 0)
        return -3;

    disable_all_buttons(obj);

    *mk3_frame(thread, thread->frame + 1) = 0x236;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_knee_check;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ----------------------------------------------------------- t_elbow_check
 *
 * armv7 0x0002f8ac, two hundred and ninety-six bytes.
 *
 *      state 0
 *          if (is_he_airborn(obj))      -> not an elbow: pop and leave
 *          get_x_dist(obj)
 *          if (obj->field28 > 0x4a)     -> not an elbow: pop and leave
 *          pop, shuffle the level above down, push t_do_elbow  (0x6ae)
 *      state 0x6ae   install t_local_reaction_exit
 *
 * The twin of t_knee_check, down to the stack surgery that erases the checker,
 * and the same seventy-four units. It asks its two questions the other way
 * round -- airborne first, then distance -- which changes nothing, since both
 * have to pass.
 */
long t_elbow_check(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    uint32_t below, carried;

    if (token != 0) {
        if (token != 0x6ae)
            return -3;
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }

    if (is_he_airborn(obj))
        goto not_an_elbow;

    get_x_dist(obj);
    if ((long)obj->field28 > 0x4a)
        goto not_an_elbow;

    if ((long)thread->frame <= 0) {
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_local_reaction_exit;
        *mk3_frame(thread, thread->frame + 1) = 0;
    } else {
        thread->frame = thread->frame - 1;
    }

    below   = mk3_frame(thread, thread->frame + 1)[1];
    carried = *mk3_frame(thread, thread->frame + 2);
    *mk3_frame(thread, thread->frame + 1) = carried;
    mk3_frame(thread, thread->frame)[1] = below;
    *mk3_frame(thread, thread->frame + 1) = 0x6ae;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_do_elbow;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;

not_an_elbow:
    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
        return 0;
    }
    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* ------------------------------------------------------------ toss_check
 *
 * armv7 0x0002ff04, two hundred and four bytes.
 *
 * **Nine ways to be told no, and one way through.** The answer goes in
 * field5c, the file's boolean slot, and the caller sets the range it wants in
 * field38 before calling -- t_joy_lo_punch asks for 0x40.
 *
 *      obj->field1c = obj->field00->him          ; the opponent
 *      q_is_he_a_boss -> yes                     -> no.  bosses are not thrown
 *      (int16_t)H[0x1a] != 0                     -> no
 *      ((MK3OBJ *)him)->field30 & 8              -> no
 *      is_he_airborn                             -> no.  not out of the air
 *      get_x_dist > obj->field38                 -> no.  out of range
 *      !am_i_facing_him                          -> no
 *      his action == 0x304                       -> no
 *      if he is NOT under joystick control:
 *          random32() % 100 > 0x32               -> no.  a coin flip
 *      get_my_dfe ; is_he_right ?
 *          he is right: keep field30
 *          he is left : field30 = field34        ; the other edge
 *      field30 <= 0x6f                           -> no.  too near the wall
 *      call_for_him(is_stick_away) -> yes        -> no.  he is holding back
 *      otherwise                                    YES
 *
 * Two of those are worth naming. The **coin flip only applies to the AI**:
 * is_he_joy asks whether the victim is a human, and only when he is not does
 * the throw have to beat a 50-in-100 roll. A player being thrown by another
 * player is never refused by chance.
 *
 * And **you cannot throw somebody into a corner.** get_my_dfe -- distance from
 * edge -- fills two numbers, and which one is used depends on which side of
 * you he is standing. There has to be 111 units of floor on the side he would
 * travel, or the throw is declined.
 */
long toss_check(MK3OBJ *obj)
{
    obj->field1c = obj->field00->him;

    q_is_he_a_boss(obj);
    if (obj->field5c != 0)
        goto refuse;

    {
        int32_t v = *(const int16_t *)(const void *)(H + 0x1a);

        obj->field20 = (uint32_t)v;
        if (v != 0)
            goto refuse;
    }

    {
        uint32_t flags =
            ((MK3OBJ *)(uintptr_t)obj->field00->him)->field30;

        obj->field1c = flags;
        if (flags & 8)
            goto refuse;
    }

    if (is_he_airborn(obj))
        goto refuse;

    get_x_dist(obj);
    if ((long)obj->field28 > (long)obj->field38)
        goto refuse;

    if (!am_i_facing_him(obj))
        goto refuse;

    get_his_action(obj);
    if (obj->field20 == 0x304)
        goto refuse;

    is_he_joy(obj);
    if (obj->field5c == 0) {
        /* Against the machine only, and only half the time. */
        if (random32() % 100u > 0x32u)
            goto refuse;
    }

    get_my_dfe(obj);
    is_he_right(obj);
    if (obj->field5c == 0)
        obj->field30 = obj->field34;

    if ((long)obj->field30 <= 0x6f)
        goto refuse;

    call_for_him(obj, (void (*)(MK3OBJ *))is_stick_away);
    if (obj->field5c != 0)
        goto refuse;

    obj->field5c = 1;
    return 1;

refuse:
    obj->field5c = 0;
    return 0;
}


/* ----------------------------------------------------------- t_joy_hi_punch
 *
 * armv7 0x0002f7f0, a hundred and eighty-eight bytes.
 *
 *      state 0
 *          if (proc->field7c != 0 && get_x_dist <= 0x40)
 *              install t_joy_lo_punch            ; become the OTHER punch
 *          disable_all_buttons ; me_in_front
 *          push t_elbow_check                    (0x72a)
 *      state 0x72a
 *          stop_me_player
 *          obj->field40 = 0xe ; get_char_ani     ; SCHIPUNCH
 *          install t_jhp4
 *
 * field7c is the header's four-button gate, and this is what it is for. In the
 * four-button scheme there is no low punch button at all -- _swtab puts LP, LK
 * and RUN in a group the four-button mode never queues -- so the game
 * synthesises one: a high punch thrown within **sixty-four** units becomes a
 * low punch instead.
 *
 * Note that is a different threshold from the elbow's seventy-four, and it is
 * asked first. So on four buttons, between 64 and 74 units a high punch is an
 * elbow, and inside 64 it is a low punch that may itself become a throw.
 */
long t_joy_hi_punch(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token != 0) {
        if (token != 0x72a)
            return -3;
        stop_me_player(obj);
        obj->field40 = 0xe;
        get_char_ani(obj);
        return mk3_install(thread, (MK3THREADFUNC)t_jhp4);
    }

    if ((int16_t)obj->field00->field7c != 0) {
        get_x_dist(obj);
        if ((long)obj->field28 <= 0x40)
            return mk3_install(thread, (MK3THREADFUNC)t_joy_lo_punch);
    }

    disable_all_buttons(obj);
    me_in_front(obj);

    *mk3_frame(thread, thread->frame + 1) = 0x72a;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_elbow_check;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ----------------------------------------------------------- t_joy_lo_punch
 *
 * armv7 0x0002ffd0, a hundred and forty-four bytes.
 *
 *      state 0
 *          stop_me_player ; disable_all_buttons
 *          obj->field38 = 0x40                   ; the throw's range
 *          if (toss_check(obj)) install t_joy_toss
 *          me_in_front
 *          obj->field40 = 0xf ; get_char_ani     ; SCLOPUNCH
 *          install t_jmp4
 *
 * The throw lives here and nowhere else: sixty-four units is the range this
 * button asks toss_check for, and everything about whether it lands is that
 * function's nine refusals. A low punch is a throw that was turned down.
 */
long t_joy_lo_punch(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    stop_me_player(obj);
    disable_all_buttons(obj);

    obj->field38 = 0x40;
    if (toss_check(obj))
        return mk3_install(thread, (MK3THREADFUNC)t_joy_toss);

    me_in_front(obj);
    obj->field40 = 0xf;
    get_char_ani(obj);
    return mk3_install(thread, (MK3THREADFUNC)t_jmp4);
}


/* ------------------------------------------------------------- t_joy_block
 *
 * armv7 0x000304d0, a hundred and twenty-eight bytes.
 *
 *      state 0      disable_all_buttons ; face_opponent
 *                   push t_do_block_hi            (0x280)
 *      state 0x280  install t_joy_block_loop
 *
 * Getting into a block is a press like any other -- QueueAndJump skips the
 * button table entirely for a release event, so bt_stance slot 2 fires on the
 * way down. Staying in it is not: that is the loop below, and it reads a
 * level.
 */
long t_joy_block(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        disable_all_buttons(obj);
        face_opponent(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x280;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_do_block_hi;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x280)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_joy_block_loop);
}


/* -------------------------------------------------------- t_joy_block_loop
 *
 * armv7 0x000301c4, two hundred and seventy-two bytes.
 *
 *      state 0       token = 0x283 ; thread->fieldfc = 1
 *      state 0x283   joystick_in_a0(obj)
 *                    if (obj->field1c & 2)      install t_joy_down
 *                    if (!am_i_facing_him)      push t_turn_around   (0x28b)
 *                    check_block_bit(obj)
 *                        held -> install t_joy_block_loop   (itself)
 *                        not  -> push t_do_unblock_hi       (0x292)
 *      state 0x28b   install t_local_reaction_exit
 *      state 0x28e   fall into the check_block_bit test
 *      state 0x292   install t_local_reaction_exit
 *
 * **A block is held, not tapped.** check_block_bit (0x0002eca8) masks the
 * translated joy word with 0x20 for player one and 0x2000 for player two --
 * the same button either way, since TranslateJoybits puts player two's bits
 * eight places up -- and hands back whether it is down at this instant. The
 * loop tail-calls ITSELF for as long as that keeps coming back non-zero, one
 * frame at a time.
 *
 * State 0x28e is the door back in from a blocked hit. Nothing in this function
 * sets it; the block reactions in _block_xfers return with it, and it lands
 * straight on the check_block_bit test without the one-frame wait. So a block
 * that took a hit resumes on the bit alone, while a block knocked out by an
 * unguarded hit has to be pressed again.
 *
 * Down while blocking hands the fighter to t_joy_down, which is how the duck
 * block is reached; there is no separate button for it.
 */
long t_joy_block_loop(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x283;
        thread->fieldfc = 1;               /* sleep one frame */
        return 1;
    }

    if (token == 0x28b || token == 0x292)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

    if (token == 0x283) {
        joystick_in_a0(obj);
        if (obj->field1c & 2)
            return mk3_install(thread, (MK3THREADFUNC)t_joy_down);

        if (!am_i_facing_him(obj)) {
            *mk3_frame(thread, thread->frame + 1) = 0x28b;
            thread->frame = thread->frame + 1;
            mk3_frame(thread, thread->frame)[1] =
                (uint32_t)(uintptr_t)t_turn_around;
            *mk3_frame(thread, thread->frame + 1) = 0;
            return 0;
        }
    } else if (token != 0x28e) {
        return -3;
    }

    /* 0x283 facing him, and 0x28e coming back from a blocked hit. */
    check_block_bit(obj);
    if (obj->field5c)
        return mk3_install(thread, (MK3THREADFUNC)t_joy_block_loop);

    *mk3_frame(thread, thread->frame + 1) = 0x292;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_do_unblock_hi;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* --------------------------------------------------- t_joy_duck_block_loop
 *
 * armv7 0x00031838, three hundred and ninety-six bytes.
 *
 *      state 0       token = 0x256 ; thread->fieldfc = 1
 *      state 0x256   am_i_facing_him ?  no  -> push t_duck_turnaround (0x25a)
 *                                       yes -> fall to the poll
 *      state 0x25a   install t_jdblk2
 *      state 0x25d   fall to the poll
 *      the poll      joystick_in_a0(obj)
 *                    down still held ? inc_downcount
 *                                      push t_check_winner_status  (0x263)
 *                    down released    ? install t_joy_back_up
 *      state 0x263   obj->field1c = 0x701 ; part->field18 = 0x701
 *                    check_block_bit ?
 *                        held -> install t_joy_duck_block_loop  (itself)
 *                        not  -> obj->field20 = 0x302 ; part->field18 = 0x302
 *                                obj->field40 = 6 ; obj->field1c = 3
 *                                push t_backwards_ani            (0x26e)
 *      state 0x26e   obj->field40 = 4 ; find_ani_last_frame ; do_next_a9_frame
 *                    install t_joyd3
 *
 * The standing loop with two things bolted on, and both are about the tag.
 *
 * **0x701 is written every frame, not once.** is_he_blocking (0x0005837c)
 * reads part->field18 back and treats 0x700 as a standing block and 0x701 as a
 * ducking one; this loop refreshes it on every pass, so anything that
 * overwrote it in between is corrected before the next hit can be tested.
 *
 * **Letting go of block does not stand you up.** It writes 0x302 -- plain
 * ducking -- and plays animation 6 BACKWARDS at rate 3 through
 * t_backwards_ani. Only releasing DOWN reaches t_joy_back_up. So the release
 * is a return to a crouch, which is the same thing the ducking attacks do when
 * they end on frame 22.
 */
long t_joy_duck_block_loop(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x256;
        thread->fieldfc = 1;               /* sleep one frame */
        return 1;
    }

    if (token == 0x25a)
        return mk3_install(thread, (MK3THREADFUNC)t_jdblk2);

    if (token == 0x26e) {
        obj->field40 = 4;
        find_ani_last_frame(obj);
        do_next_a9_frame(obj);
        return mk3_install(thread, (MK3THREADFUNC)t_joyd3);
    }

    if (token == 0x263) {
        obj->field1c = 0x701;
        obj->field00->field18 = 0x701;

        check_block_bit(obj);
        if (obj->field5c)
            return mk3_install(thread,
                               (MK3THREADFUNC)t_joy_duck_block_loop);

        obj->field20 = 0x302;
        obj->field00->field18 = 0x302;
        obj->field40 = 6;
        obj->field1c = 3;
        *mk3_frame(thread, thread->frame + 1) = 0x26e;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_backwards_ani;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x256) {
        if (!am_i_facing_him(obj)) {
            *mk3_frame(thread, thread->frame + 1) = 0x25a;
            thread->frame = thread->frame + 1;
            mk3_frame(thread, thread->frame)[1] =
                (uint32_t)(uintptr_t)t_duck_turnaround;
            *mk3_frame(thread, thread->frame + 1) = 0;
            return 0;
        }
    } else if (token != 0x25d) {
        return -3;
    }

    /* The poll: 0x256 already facing him, and 0x25d coming back in. */
    joystick_in_a0(obj);
    if (obj->field1c & 2) {
        inc_downcount(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x263;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_check_winner_status;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    return mk3_install(thread, (MK3THREADFUNC)t_joy_back_up);
}


/* ------------------------------------------------------ t_post_joy_duck_kick
 *
 * armv7 0x000302d4, a hundred and eight bytes.
 *
 *      joystick_in_a0(obj)
 *      obj->field1c & 2 ?  install t_joyd3        ; still holding down
 *                          install t_joy_back_up  ; let go: stand up
 *
 * Where both ducking kicks land when they are finished. Whether you stay in a
 * crouch is decided here and nowhere else, by asking the stick again rather
 * than by remembering what it said when the kick started.
 */
long t_post_joy_duck_kick(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    joystick_in_a0(obj);
    if (obj->field1c & 2)
        return mk3_install(thread, (MK3THREADFUNC)t_joyd3);

    return mk3_install(thread, (MK3THREADFUNC)t_joy_back_up);
}


/* ---------------------------------------------------------- t_joy_duck_punch
 *
 * armv7 0x00030340, a hundred and seventy-six bytes.
 *
 *      state 0       disable_all_buttons
 *                    push t_stat_do_duck_punch          (0x185)
 *      state 0x185   joystick_in_a0(obj)
 *                    obj->field1c & 2 ? install t_joyd3
 *                                       install t_joy_back_up
 *
 * The same ending as the ducking kicks, written out here rather than shared:
 * the punch asks the stick itself instead of going through
 * t_post_joy_duck_kick. No hit-stop and no retraction state -- the duck
 * punch's own proc does all of that and this is only the door in and out.
 */
long t_joy_duck_punch(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        disable_all_buttons(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x185;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_stat_do_duck_punch;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x185)
        return -3;

    joystick_in_a0(obj);
    if (obj->field1c & 2)
        return mk3_install(thread, (MK3THREADFUNC)t_joyd3);

    return mk3_install(thread, (MK3THREADFUNC)t_joy_back_up);
}


/* --------------------------------------------------------- t_joy_duck_kickh
 *
 * armv7 0x0002ee78, two hundred and thirty-two bytes.
 *
 *      state 0       push t_stat_do_duck_kickh              (0x194)
 *      state 0x194   part->field18 = 0x60b
 *                    obj->field1c = 8
 *                    if (obj->field5c != 0) obj->field1c = 0x10
 *                    thread->fieldfc = obj->field1c         (0x19c)
 *      state 0x19c   obj->field1c = 4 ; push t_retract_strike (0x19e)
 *      state 0x19e   part->field14 += 0x26
 *                    install t_post_joy_duck_kick
 *
 * **Eight frames on a miss, sixteen on a hit.** field5c is the file's boolean
 * slot and the swing loop leaves the "did that connect" answer in it, so this
 * state is reading its child's verdict rather than asking anything itself.
 * t_kick2 does the same thing with twelve; every attack carries its own pause.
 *
 * The retraction comes back at 4, which is neither the swing's rate nor the
 * standing kicks' 3.
 */
long t_joy_duck_kickh(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x194;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_stat_do_duck_kickh;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x194) {
        obj->field00->field18 = 0x60b;
        obj->field1c = 8;
        if (obj->field5c != 0)
            obj->field1c = 0x10;
        *mk3_frame(thread, thread->frame + 1) = 0x19c;
        thread->fieldfc = obj->field1c;
        return (long)obj->field1c;
    }

    if (token == 0x19c) {
        obj->field1c = 4;
        *mk3_frame(thread, thread->frame + 1) = 0x19e;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_retract_strike;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x19e)
        return -3;

    obj->field1c = obj->field00->field14 + 0x26;
    obj->field00->field14 = obj->field1c;
    return mk3_install(thread, (MK3THREADFUNC)t_post_joy_duck_kick);
}


/* --------------------------------------------------------- t_joy_duck_kickl
 *
 * armv7 0x0002fc5c, two hundred and fifty-two bytes.
 *
 *      state 0       push t_stat_do_duck_kickl              (0x1aa)
 *      state 0x1aa   part->field18 = 0x60b
 *                    obj->field54 = 6
 *                    is_he_joy(obj) ; if he is NOT one, obj->field54 = 0xa
 *                    thread->fieldfc = obj->field54         (0x1b3)
 *      state 0x1b3   obj->field1c = 2 ; push t_retract_strike (0x1b6)
 *      state 0x1b6   part->field14 += 8
 *                    install t_post_joy_duck_kick
 *
 * **Six frames against a person, ten against the machine.** Its twin above
 * picks its pause from whether the kick connected; this one picks it from WHO
 * was kicked. `is_he_joy` asks whether the victim is under joystick control,
 * and against the AI the attacker is left standing there four frames longer.
 *
 * That is the second assist of this shape in the file -- `toss_check` refuses
 * half of all throws against the machine and none against a player -- and both
 * are hidden inside ordinary moves rather than in anything called difficulty.
 *
 * Note the answer is parked in field54 rather than field1c, and field1c only
 * receives it at the end. The retraction rate is 2, and the step added to the
 * part is 8 where the high kick adds 0x26.
 */
long t_joy_duck_kickl(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x1aa;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_stat_do_duck_kickl;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x1aa) {
        obj->field1c = 0x60b;
        obj->field00->field18 = 0x60b;
        obj->field54 = 6;
        is_he_joy(obj);
        if (obj->field5c == 0)
            obj->field54 = 0xa;
        obj->field1c = obj->field54;
        *mk3_frame(thread, thread->frame + 1) = 0x1b3;
        thread->fieldfc = obj->field1c;
        return (long)obj->field1c;
    }

    if (token == 0x1b3) {
        obj->field1c = 2;
        *mk3_frame(thread, thread->frame + 1) = 0x1b6;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_retract_strike;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x1b6)
        return -3;

    /* in r3 only: 0x1c is not written (difftest) */
    obj->field00->field14 = obj->field00->field14 + 8;
    return mk3_install(thread, (MK3THREADFUNC)t_post_joy_duck_kick);
}


/* =========================================================================
 * t_jhp4, t_jhp5, t_jmp4, t_jmp5 -- the four punch swings
 *
 * armv7 0x00030d68, 0x00030be4, 0x00030a60 and 0x000308dc. Three hundred and
 * eighty-eight bytes each, and the same three states each:
 *
 *      state 0       get_last_button ; part->field30 = obj->field1c
 *                    part->field58 = 2 (high) / 3 (low)
 *                    group_sound(obj, 0) ; rsnd_func(obj, 0xe)
 *                    obj->field1c = 3          ; the rate
 *                    obj->field20 = tag        ; 0x101 high, 0x102 low
 *                    push t_act_mframew
 *      state A       (uint16_t)G[0x452] != 0 ? retract
 *                    obj->field48 = strike id  ; 2 high, 3 low
 *                    obj->field1c = it ; obj->field44 = 0
 *                    the strike check
 *                    obj->field44 = 5          ; five live frames
 *                    push t_punch_sleep
 *      state B       obj->field5c == 0 ? retract       ; nothing connected
 *                    switch (obj->field1c >> 16)
 *                        0    -> the OTHER swing of the same punch
 *                        1    -> the transition into the other punch
 *                        else -> retract
 *
 * **The two swings of one punch ping-pong.** t_jhp4 continues into t_jhp5 and
 * t_jhp5 continues back into t_jhp4; the low pair does the same. So a string
 * of jabs alternates between two procs, each of which plays its own part of
 * animation 14 or 15 -- which is what makes it look like one chain rather than
 * the same three frames over and over.
 *
 * Case 1 crosses to the other punch entirely: t_jhp4 to t_joy_punch_htm1,
 * t_jhp5 to t_joy_punch_htm2, and the low pair to mth1 and mth2. Those are the
 * procs that walk six zero-terminators into the stream to reach its transition
 * parts.
 *
 * **THE SELECTOR IS THE BUTTON YOU PRESSED.** This was left open when the four
 * swings first landed; it closes through three functions.
 *
 * t_punch_sleep's last act before popping is `get_last_button`, which leaves a
 * BUTTON-QUEUE ENTRY in field1c -- so the word state B shifts is that entry and
 * not anything the strike check wrote. `stick_look_lr` (0x0005369c) gives the
 * entry's layout, because it matches against `(field1c & 0xffff0000) ==
 * code << 16` and then narrows field1c to sixteen bits to read a deadline:
 *
 *      high halfword = the button code      low halfword = a timestamp
 *
 * and `four_button_switch` (0x00057274) fixes the codes at HP 0, LP 1, BL 2,
 * HK 3, LK 4, RUN 5. So `field1c >> 16` is which button, and the three cases
 * read:
 *
 *      HP (0)                  -> the other swing of the same punch
 *      LP (1)                  -> cross into the other punch
 *      BL, HK, LK, RUN (2..5)  -> retract
 *
 * and the two punches spell it out in OPPOSITE branch orders, which is what
 * makes the rule readable once both are written down:
 *
 *      t_jhp4 (high)   sel 0 -> t_jhp5            sel 1 -> t_joy_punch_htm1
 *      t_jhp5 (high)   sel 0 -> t_jhp4            sel 1 -> t_joy_punch_htm2
 *      t_jmp4 (low)    sel 0 -> t_joy_punch_mth1  sel 1 -> t_jmp5
 *      t_jmp5 (low)    sel 0 -> t_joy_punch_mth2  sel 1 -> t_jmp4
 *
 * **The button you press is the punch you get.** HP always lands in the high
 * chain and LP always in the low one; "continue" and "cross over" are the same
 * rule seen from two sides. htm is high-to-mid and mth is mid-to-high, which
 * is why the low pair's cross carries the mirrored name.
 *
 * So HP,HP,HP walks the high chain, HP,LP crosses into the low one, and a kick
 * in the middle of it drops the whole thing.
 *
 * ## Three things the two pairs do NOT share
 *
 * part->field58 is 2 for the high pair and **3** for the low one.
 *
 * The high pair runs `punch_strike_check` with a10 zeroed; the low pair runs
 * `strike_check_a0` with a10 at 1 and field48 at 3 beforehand.
 *
 * And the low pair disagrees with ITSELF about field48. t_jmp4 writes -1 when
 * the check CONNECTED; t_jmp5 writes it when the check MISSED -- `cbz` past
 * the store in one (0x00030b76) and into it in the other (0x000309ee). Since
 * t_punch_sleep only re-runs its strike check while field48 >= 0, the two
 * swings of the low punch stop checking under opposite conditions.
 * Transcribed, not reconciled.
 *
 * G[0x452] is the halfword reaction_start_chores writes a 1 into when a
 * fighter enters a reaction. A punch whose animation finishes while that is
 * set skips its strike check entirely and goes straight to the retraction.
 *
 * The rsnd index 0xe is the whoosh and group_sound 0 is the attack grunt, the
 * pair every ordinary swing in the game makes.
 * ========================================================================= */

long t_jhp4(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    uint32_t sel;

    if (token == 0) {
        get_last_button(obj);
        obj->field00->field30 = obj->field1c;
        obj->field00->field58 = 2;
        obj->field1c = 0;
        group_sound(obj);
        rsnd_func(obj, 0xe);
        obj->field1c = 3;
        obj->field20 = 0x101;
        *mk3_frame(thread, thread->frame + 1) = 0x744;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_act_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x744) {
        uint16_t busy = *(const uint16_t *)(const void *)(G_BYTES + 0x452);

        obj->field1c = (uint32_t)(int32_t)(int16_t)busy;
        if (busy != 0)
            return mk3_install(thread, (MK3THREADFUNC)t_joy_un_hi_punch1);

        obj->field48 = 2;
        obj->field1c = 2;
        obj->a10 = 0;
        punch_strike_check(obj);
        obj->a10 = 5;
        *mk3_frame(thread, thread->frame + 1) = 0x751;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_punch_sleep;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x751)
        return -3;

    if (obj->field5c == 0)
        return mk3_install(thread, (MK3THREADFUNC)t_joy_un_hi_punch1);

    /* The button code out of the queue entry get_last_button left in 0x1c:
     * HP is 0 and LP is 1, so the button you press is the punch you get. */
    sel = obj->field1c >> 16;
    obj->field1c = sel;
    if (sel == 0)
        return mk3_install(thread, (MK3THREADFUNC)t_jhp5);
    if (sel == 1)
        return mk3_install(thread, (MK3THREADFUNC)t_joy_punch_htm1);
    return mk3_install(thread, (MK3THREADFUNC)t_joy_un_hi_punch1);
}


long t_jhp5(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    uint32_t sel;

    if (token == 0) {
        get_last_button(obj);
        obj->field00->field30 = obj->field1c;
        obj->field00->field58 = 2;
        obj->field1c = 0;
        group_sound(obj);
        rsnd_func(obj, 0xe);
        obj->field1c = 3;
        obj->field20 = 0x101;
        *mk3_frame(thread, thread->frame + 1) = 0x771;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_act_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x771) {
        uint16_t busy = *(const uint16_t *)(const void *)(G_BYTES + 0x452);

        obj->field1c = (uint32_t)(int32_t)(int16_t)busy;
        if (busy != 0)
            return mk3_install(thread, (MK3THREADFUNC)t_joy_un_hi_punch2);

        obj->field48 = 2;
        obj->field1c = 2;
        obj->a10 = 0;
        punch_strike_check(obj);
        obj->a10 = 5;
        *mk3_frame(thread, thread->frame + 1) = 0x77d;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_punch_sleep;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x77d)
        return -3;

    if (obj->field5c == 0)
        return mk3_install(thread, (MK3THREADFUNC)t_joy_un_hi_punch2);

    /* The button code out of the queue entry get_last_button left in 0x1c:
     * HP is 0 and LP is 1, so the button you press is the punch you get. */
    sel = obj->field1c >> 16;
    obj->field1c = sel;
    if (sel == 0)
        return mk3_install(thread, (MK3THREADFUNC)t_jhp4);
    if (sel == 1)
        return mk3_install(thread, (MK3THREADFUNC)t_joy_punch_htm2);
    return mk3_install(thread, (MK3THREADFUNC)t_joy_un_hi_punch2);
}


long t_jmp4(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    uint32_t sel;

    if (token == 0) {
        get_last_button(obj);
        obj->field00->field30 = obj->field1c;
        obj->field00->field58 = 3;
        obj->field1c = 0;
        group_sound(obj);
        rsnd_func(obj, 0xe);
        obj->field1c = 3;
        obj->field20 = 0x102;
        *mk3_frame(thread, thread->frame + 1) = 0x7f7;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_act_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x7f7) {
        uint16_t busy = *(const uint16_t *)(const void *)(G_BYTES + 0x452);

        obj->field1c = (uint32_t)(int32_t)(int16_t)busy;
        if (busy != 0)
            return mk3_install(thread, (MK3THREADFUNC)t_joy_un_lo_punch1);

        obj->a10 = 1;
        obj->field48 = 3;
        obj->field1c = 3;
        strike_check_a0(obj);
        /* `cbz r3, ...` past the store: -1 lands only when the
         * check CONNECTED. Its twin below does the opposite. */
        if (obj->field5c != 0)
            obj->field48 = (uint32_t)-1;
        obj->a10 = 5;
        *mk3_frame(thread, thread->frame + 1) = 0x807;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_punch_sleep;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x807)
        return -3;

    if (obj->field5c == 0)
        return mk3_install(thread, (MK3THREADFUNC)t_joy_un_lo_punch1);

    /* The button code out of the queue entry get_last_button left in 0x1c:
     * HP is 0 and LP is 1, so the button you press is the punch you get. */
    sel = obj->field1c >> 16;
    obj->field1c = sel;
    if (sel == 0)
        return mk3_install(thread, (MK3THREADFUNC)t_joy_punch_mth1);
    if (sel == 1)
        return mk3_install(thread, (MK3THREADFUNC)t_jmp5);
    return mk3_install(thread, (MK3THREADFUNC)t_joy_un_lo_punch1);
}


long t_jmp5(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    uint32_t sel;

    if (token == 0) {
        get_last_button(obj);
        obj->field00->field30 = obj->field1c;
        obj->field00->field58 = 3;
        obj->field1c = 0;
        group_sound(obj);
        rsnd_func(obj, 0xe);
        obj->field1c = 3;
        obj->field20 = 0x102;
        *mk3_frame(thread, thread->frame + 1) = 0x826;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_act_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x826) {
        uint16_t busy = *(const uint16_t *)(const void *)(G_BYTES + 0x452);

        obj->field1c = (uint32_t)(int32_t)(int16_t)busy;
        if (busy != 0)
            return mk3_install(thread, (MK3THREADFUNC)t_joy_un_lo_punch2);

        obj->a10 = 1;
        obj->field48 = 3;
        obj->field1c = 3;
        strike_check_a0(obj);
        /* `cbz r3, ...` INTO the store: -1 lands only when the
         * check MISSED. The opposite of its twin above, and it
         * is transcribed rather than reconciled. */
        if (obj->field5c == 0)
            obj->field48 = (uint32_t)-1;
        obj->a10 = 5;
        *mk3_frame(thread, thread->frame + 1) = 0x837;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_punch_sleep;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x837)
        return -3;

    /* `cbnz` on 0x5c: a non-zero answer abandons the punch (difftest; the
     * condition was the other way round before) */
    if (obj->field5c != 0)
        return mk3_install(thread, (MK3THREADFUNC)t_joy_un_lo_punch2);

    /* The button code out of the queue entry get_last_button left in 0x1c:
     * HP is 0 and LP is 1, so the button you press is the punch you get. */
    sel = obj->field1c >> 16;
    obj->field1c = sel;
    if (sel == 0)
        return mk3_install(thread, (MK3THREADFUNC)t_joy_punch_mth2);
    if (sel == 1)
        return mk3_install(thread, (MK3THREADFUNC)t_jmp4);
    return mk3_install(thread, (MK3THREADFUNC)t_joy_un_lo_punch2);
}


/* ------------------------------------------------------ t_check_winner_status
 *
 * armv7 0x0002ecd4, a hundred and seventy-two bytes.
 *
 *      state 0 only
 *          obj->field1c = (int16_t)G[0x45c]
 *          0    -> pop back to the caller
 *          1    -> install t_player_1_wins
 *          2    -> install t_player_2_wins
 *          3    -> install t_finish_him
 *          else -> pop back to the caller
 *
 * **The round ends from inside whatever you happen to be doing.** This is a
 * one-frame check with no state of its own, pushed by the polling loops, and
 * the three non-zero values do not return -- they REPLACE the current handler,
 * so the fighter's whole action chain is thrown away where it stands.
 *
 * G[0x45c] is a signed halfword, and only four values are read. Everything
 * else -- including a negative -- falls through to the ordinary pop, so an
 * unrecognised status is treated as "the round is still running" rather than
 * as an error.
 *
 * The pop is the usual one: drop a level if there is one, and install
 * t_local_reaction_exit if this was the bottom.
 */
long t_check_winner_status(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint16_t raw;
    int32_t  status;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    raw    = *(const uint16_t *)(const void *)(G_BYTES + 0x45c);
    status = (int16_t)raw;
    obj->field1c = (uint32_t)status;

    if (raw != 0) {
        if (status == 2)
            return mk3_install(thread, (MK3THREADFUNC)t_player_2_wins);
        if (status == 3)
            return mk3_install(thread, (MK3THREADFUNC)t_finish_him);
        if (status == 1)
            return mk3_install(thread, (MK3THREADFUNC)t_player_1_wins);
    }

    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
        return 0;
    }
    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* ------------------------------------------------------------- t_jumpup_kick
 *
 * armv7 0x0002f0d0, a hundred and twenty bytes.
 *
 *      state 0       disable_all_buttons ; push t_do_jumpup_kick  (0x1e6)
 *      state 0x1e6   install t_local_reaction_exit
 *
 * A door and nothing else. Every frame of the move lives in t_do_jumpup_kick;
 * this exists so that the button table has something of the right shape to
 * name, and so the move ends in the file's one exit rather than in the middle
 * of the jump code.
 */
long t_jumpup_kick(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        disable_all_buttons(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x1e6;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_do_jumpup_kick;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x1e6)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* ---------------------------------------------------------- t_joy_flip_punch
 *
 * armv7 0x0002efdc, a hundred and twenty bytes.
 *
 *      state 0       disable_all_buttons ; push t_do_flip_punch   (0x1d0)
 *      state 0x1d0   install t_local_reaction_exit
 *
 * The same door, instruction for instruction, with a different token and a
 * different child. The pair is worth keeping side by side: when two procs
 * differ only in two constants, the constants are the whole content.
 */
long t_joy_flip_punch(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        disable_all_buttons(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x1d0;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_do_flip_punch;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x1d0)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* --------------------------------------------------------- t_walk_flip_check
 *
 * armv7 0x0002fb88, two hundred and twelve bytes.
 *
 *      state 0
 *          am_i_facing_him ? yes -> pop back to the caller
 *          pop, shuffle the level above down, push t_turn_around  (0x114)
 *      state 0x114   install t_local_reaction_exit
 *      state 0x116   pop back to the caller
 *
 * The third member of the family that includes t_knee_check and
 * t_elbow_check, and it does the same stack surgery: it erases ITSELF from
 * the stack before pushing the turn, so when t_turn_around finishes there is
 * no checker left underneath to return to.
 *
 * The analogy that fits is a doorman who checks you at the door and then
 * leaves: the turn is not a detour you come back from, it replaces the frame
 * the check was standing in.
 *
 * **0x116 is a door nothing here opens.** No path in this function writes it;
 * something outside returns with it, and it lands straight on the plain pop.
 * The block loop has the same shape at 0x28e. Where it comes from is not
 * traced and is not guessed at here.
 */
long t_walk_flip_check(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    uint32_t below, carried;

    if (token == 0x114)
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);

    if (token == 0x116)
        goto pop;

    if (token != 0)
        return -3;

    if (am_i_facing_him(obj))
        goto pop;

    if ((long)thread->frame <= 0) {
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_local_reaction_exit;
        *mk3_frame(thread, thread->frame + 1) = 0;
    } else {
        thread->frame = thread->frame - 1;
    }

    below   = mk3_frame(thread, thread->frame + 1)[1];
    carried = *mk3_frame(thread, thread->frame + 2);
    *mk3_frame(thread, thread->frame + 1) = carried;
    mk3_frame(thread, thread->frame)[1] = below;
    *mk3_frame(thread, thread->frame + 1) = 0x114;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_turn_around;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;

pop:
    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
        return 0;
    }
    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* ------------------------------------------------------------- t_punch_sleep
 *
 * armv7 0x00030eec, two hundred and eight bytes.
 *
 *      state 0       token = 0x67d ; thread->fieldfc = 1 ; return 1
 *      state 0x67d
 *          obj->field1c = obj->field48
 *          if (obj->field48 >= 0) punch_strike_check(obj)
 *          get_last_button(obj)                   ; rewrites field1c
 *          obj->field20 = part->field30           ; the button that started it
 *          if (part->field30 != obj->field1c) {
 *              obj->field5c = am_i_facing_him(obj)
 *              pop
 *          }
 *          if (--obj->a10 > 0) { token = 0x67d ; fieldfc = 1 ; return 1 }
 *          obj->field5c = 0 ; pop
 *
 * **The chain continues when a NEW BUTTON GETS QUEUED, and holding queues
 * nothing.** This is the loop the four punch swings push after their strike
 * check, and it is what decides whether the swing that follows is another
 * swing or a retraction.
 *
 * The comparison is not "is a different button down" -- it is a comparison of
 * two QUEUE ENTRIES, and that is a stronger statement. `get_last_button` resets
 * the ring cursor to the head and steps back exactly one, so it always answers
 * "the entry before the head". part->field30 holds that same answer from the
 * swing's first frame.
 *
 * While you hold, no entry is queued, the head does not move, and the two
 * readings are the same word -- so the loop burns one of the five frames in
 * obj->a10 and sleeps. When the five are gone field5c goes to 0 and the swing
 * above retracts.
 *
 * Press anything -- INCLUDING the same button again -- and a fresh entry goes
 * in with a fresh timestamp in its low half. The head moves, the two readings
 * differ, and field5c takes am_i_facing_him: the chain continues as long as
 * you are still turned towards him. The swing above then reads the high half
 * of that entry to decide WHICH continuation, which is the button code.
 *
 * So the rule is tap, not hold, and re-tapping the same button is the ordinary
 * case rather than the excluded one. (An earlier note here said the chain
 * needed a *different* button; that was a misreading of a queue comparison as
 * a button comparison.)
 *
 * The strike check is re-run every pass, but only while field48 is
 * non-negative. t_jmp4 sets field48 to -1 right after its own check for
 * exactly that reason: the low punch tests its hit once and this loop must not
 * test it again.
 *
 * The state token is written from r2, which still holds the frame index read
 * on entry -- so both sleeps write into the same slot and the loop never grows
 * the stack.
 */
long t_punch_sleep(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token != 0) {
        if (token != 0x67d)
            return -3;

        obj->field1c = obj->field48;
        if ((long)obj->field48 >= 0)
            punch_strike_check(obj);

        get_last_button(obj);
        obj->field20 = obj->field00->field30;

        if (obj->field00->field30 != obj->field1c) {
            obj->field5c = (uint32_t)am_i_facing_him(obj);
            goto pop;
        }

        obj->a10 = obj->a10 - 1;
        if ((long)obj->a10 <= 0) {
            obj->field5c = 0;
            goto pop;
        }
        /* still inside the window: sleep one more frame */
    }

    *mk3_frame(thread, thread->frame + 1) = 0x67d;
    thread->fieldfc = 1;
    return 1;

pop:
    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
        return 0;
    }
    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* ------------------------------------------------------------------- t_joyd4
 *
 * armv7 0x0002fad4, a hundred and eighty bytes.
 *
 *      state 0       part->field18 = 0x302 ; obj->field1c = 0x302
 *                    token = 0x162 ; thread->fieldfc = 1 ; return 1
 *      state 0x162   am_i_facing_him ? yes -> install t_joyd5
 *                                      no  -> push t_duck_turnaround  (0x167)
 *      state 0x167   install t_joyd3
 *
 * The first half of the crouch's two-frame heartbeat. t_joyd4 stamps the tag
 * and sleeps; t_joyd5 below wakes up and reads the stick. Neither does both,
 * and the pair hands control back and forth for as long as you hold down.
 *
 * **0x302 is written every single frame you spend crouched.** It is the same
 * refresh the duck-block loop does with 0x701, and for the same reason: the
 * tag is what everything else reads to know your stance, so anything that
 * overwrote it is corrected before the next frame can test a hit against it.
 *
 * Turning round while crouched does not stand you up -- t_duck_turnaround
 * runs and the fighter comes back to t_joyd3, still down.
 */
long t_joyd4(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field00->field18 = 0x302;
        obj->field1c = 0x302;
        *mk3_frame(thread, thread->frame + 1) = 0x162;
        thread->fieldfc = 1;
        return 1;
    }

    if (token == 0x167)
        return mk3_install(thread, (MK3THREADFUNC)t_joyd3);

    if (token != 0x162)
        return -3;

    if (am_i_facing_him(obj))
        return mk3_install(thread, (MK3THREADFUNC)t_joyd5);

    *mk3_frame(thread, thread->frame + 1) = 0x167;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_duck_turnaround;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------------------- t_joyd5
 *
 * armv7 0x000303f0, two hundred bytes.
 *
 *      state 0       inc_downcount(obj)
 *                    check_block_bit ? held -> install t_joy_duck_block
 *                                      not  -> push t_check_winner_status
 *                                                                    (0x16f)
 *      state 0x16f   joystick_in_a0(obj)
 *                    obj->field1c & 2 ? install t_joyd4
 *                                       install t_joy_back_up
 *
 * The other half of the heartbeat, and the three questions it asks are asked
 * in this order for a reason.
 *
 * **Block is read before the stick.** So pressing BL while crouched takes you
 * to the duck block on the same frame, without the stick ever being consulted
 * -- which is why a duck block engages even though you are still holding down.
 *
 * **The round result is read before the stick too**, and through a push rather
 * than a call, so if the round has ended the crouch is replaced outright and
 * the stick read at 0x16f never happens.
 *
 * Only if both pass does the fighter ask whether down is still held: yes goes
 * back to t_joyd4 and the loop turns over, no goes to t_joy_back_up. Note this
 * is the real stick and not a remembered one -- exactly as
 * t_post_joy_duck_kick does at the end of the ducking attacks.
 *
 * inc_downcount runs once per turn of the loop, so it counts frames spent
 * crouched, not presses.
 */
long t_joyd5(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        inc_downcount(obj);

        if (check_block_bit(obj))
            return mk3_install(thread, (MK3THREADFUNC)t_joy_duck_block);

        *mk3_frame(thread, thread->frame + 1) = 0x16f;
        thread->frame = thread->frame + 1;
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_check_winner_status;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x16f)
        return -3;

    joystick_in_a0(obj);
    if (obj->field1c & 2)
        return mk3_install(thread, (MK3THREADFUNC)t_joyd4);

    return mk3_install(thread, (MK3THREADFUNC)t_joy_back_up);
}


/* ------------------------------------------------------------ t_do_flip
 *
 * armv7 0x00030634, three hundred and forty bytes. The angle jump -- the
 * diagonal one, not the straight-up hop -- from its first frame to its last.
 *
 *      state 0
 *          thread->args[thread->fieldf8++] = obj->field1c   ; park the caller's
 *          obj->field30 = obj->field40                      ; save the ani
 *          obj->field40 = 0x39 ; get_char_ani(obj)
 *          obj->field38 = obj->field40 ; find_last_frame(obj)
 *          fall through
 *      state 0x35e
 *          obj->field1c = 1 ; group_sound(obj)              ; the jump grunt
 *          obj->field40 = obj->field1c = thread->args[--thread->fieldf8]
 *          obj->field2c = obj->field08->field28
 *          if (obj->field2c & 0x10) obj->field40 = obj->field20
 *          fall through
 *      state 0x368
 *          stop_me_player ; disable_all_buttons
 *          stuff_buttons(obj, bt_angle_jump)
 *          obj->field1c = part->field18 = 0x308
 *          part->field3c = (int16_t)(obj->field08->field0c >> 16)
 *          if (is_he_right(obj)) obj->field48 = -obj->field48
 *          fall through
 *      state 0x375
 *          obj->field1c = obj->field48         ; horizontal speed, signed
 *          obj->field34 = t_angle_jump_call    ; the per-frame callback
 *          obj->field20 = 0xfff60000           ; -10.0
 *          obj->field24 = 0x00008000           ;  +0.5
 *          obj->field28 = 3
 *          obj->field48 = 4
 *          push t_flight_call                  (0x382)
 *      state 0x382   install t_angle_jump_land_jsrp
 *
 * **The jump's physics are three constants in this function's literal pool.**
 * The header already establishes that the object's coordinates are 16.16 fixed
 * point -- integer on top, fraction underneath -- and read that way the two
 * literals are exactly -10.0 and +0.5: the launch velocity and the gravity
 * added back to it each frame. 0xfff60000 is not a flag or a handle; it is
 * minus ten, written the way this engine writes numbers.
 *
 * That is the whole arc. Ten up, half a unit of gravity, and field48 across --
 * and field48 is the one that gets negated, so the SIGN is the direction and
 * the magnitude is the same either way. You cannot jump further forwards than
 * backwards in this game because there is only one number.
 *
 * **The fall-through is the shape.** Four states with no branch between them:
 * 0 falls into 0x35e falls into 0x368 falls into 0x375, all in one frame. The
 * tokens exist so that something else can jump INTO the middle of the setup --
 * a flip that is already airborne enters at 0x368 and skips picking the
 * animation. Nothing in this file writes 0x35e, 0x368 or 0x375; they are
 * entered from elsewhere, and where is not traced here.
 *
 * The 0x10 bit of obj->field08->field28 swaps the animation for the one parked
 * in field20 -- the only place the saved-and-restored value is overridden.
 *
 * thread->args at 0xa8 with the cursor at 0xf8 is the second stack, the one
 * the header notes the striker pair uses. Here it holds a single word across
 * the get_char_ani call, which clobbers field1c.
 */
long t_do_flip(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t *args = (uint32_t *)(void *)thread->args;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x382)
        return mk3_install(thread,
                           (MK3THREADFUNC)t_angle_jump_land_jsrp);

    if (token != 0 && token != 0x35e && token != 0x368 && token != 0x375)
        return -3;

    if (token == 0) {
        args[thread->fieldf8] = obj->field1c;
        thread->fieldf8 = thread->fieldf8 + 1;

        obj->field30 = obj->field40;
        obj->field40 = 0x39;
        get_char_ani(obj);
        obj->field38 = obj->field40;
        find_last_frame(obj);
    }

    if (token == 0 || token == 0x35e) {
        uint32_t saved;

        obj->field1c = 1;
        group_sound(obj);

        thread->fieldf8 = thread->fieldf8 - 1;
        saved = args[thread->fieldf8];
        obj->field40 = saved;
        obj->field1c = saved;

        obj->field2c = obj->field08->field28;
        if (obj->field2c & 0x10)
            obj->field40 = obj->field20;
    }

    if (token == 0 || token == 0x35e || token == 0x368) {
        stop_me_player(obj);
        disable_all_buttons(obj);
        stuff_buttons(obj, (uint32_t)(uintptr_t)&bt_angle_jump);

        obj->field1c = 0x308;
        obj->field00->field18 = 0x308;
        /* ldrsh [r3, #0xe] -- the HIGH half of field0c, which the header
         * establishes is the integer part of the 16.16 x coordinate. */
        obj->field00->field3c =
            (uint32_t)(int32_t)(int16_t)(obj->field08->field0c >> 16);

        if (is_he_right(obj))
            obj->field48 = (uint32_t)(-(int32_t)obj->field48);
    }

    /* 0x375: the flight parameters, and away. */
    obj->field1c = obj->field48;
    obj->field34 = (uint32_t)(uintptr_t)t_angle_jump_call;
    obj->field20 = 0xfff60000u;            /* -10.0 in 16.16 */
    obj->field24 = 0xfff60000u + 0xa8000u; /*  +0.5 in 16.16 */
    obj->field28 = 3;
    obj->field48 = 4;

    *mk3_frame(thread, thread->frame + 1) = 0x382;
    thread->frame = thread->frame + 1;
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_flight_call;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------- t_angle_jump_call
 *
 * armv7 0x00030788, a hundred and fifty-two bytes. The callback t_do_flip
 * parks in obj->field34, run by t_flight_call once per airborne frame.
 *
 *      state 0       am_i_joy ? no -> pop.  the AI does not get this
 *                               yes -> fall through
 *      state 0x335
 *          obj->field1c = obj->field08->field1c
 *          if ((long)obj->field1c < 0) pop
 *          obj->field24 = (int16_t)obj->field08->field12
 *          obj->field1c = |part->field40 - obj->field24|
 *          if (obj->field1c <= 0x14) {
 *              disable_all_buttons(obj)
 *              obj->field34 = 0 ; part->field34 = 0
 *          }
 *          pop
 *      state 0x349   pop
 *
 * **This is the window that closes.** Every frame of the jump it measures one
 * distance, and when that distance falls to twenty or less it takes the
 * buttons away and then erases ITSELF -- field34, the very slot t_do_flip put
 * it in, goes to zero, so t_flight_call stops calling it for the rest of the
 * jump.
 *
 * The analogy is a countdown that switches itself off once it has fired: the
 * check is cheap, it runs every frame, and the instant it is satisfied it
 * makes sure it can never run again.
 *
 * **Only a human gets checked.** am_i_joy gates the whole thing on the very
 * first frame, and when the answer is no the routine pops without ever
 * measuring anything. This is the third assist of that shape in the file,
 * after toss_check's coin flip and t_joy_duck_kickl's four extra frames -- and
 * like both of those it is hidden inside an ordinary move.
 *
 * What the two quantities are is NOT pinned down. field1c of the other object
 * is only tested for being negative, and the 0x12 halfword against part->0x40
 * is only ever used as a difference. The twenty is real; what it is twenty of
 * is not traced here and is not guessed at.
 *
 * 0x349 is another door nothing in this function opens, and it does nothing
 * but pop. Same shape as 0x116 in t_walk_flip_check.
 */
long t_angle_jump_call(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token != 0x335 && token != 0x349 && token != 0)
        return -3;

    if (token == 0 && !am_i_joy(obj))
        goto pop;

    if (token == 0 || token == 0x335) {
        MK3OBJ  *other = obj->field08;
        int32_t  diff;

        obj->field1c = other->field1c;
        if ((long)obj->field1c < 0)
            goto pop;

        /* ldrsh [r2, #0x12] -- the HIGH half of field10, the integer part
         * of the other coordinate of the same 16.16 pair. */
        obj->field24 = (uint32_t)(int32_t)(int16_t)(other->field10 >> 16);

        diff = (int32_t)obj->field00->field40 - (int32_t)obj->field24;
        obj->field1c = (uint32_t)diff;
        if (diff < 0) {
            diff = -diff;
            obj->field1c = (uint32_t)diff;
        }

        if (diff <= 0x14) {
            disable_all_buttons(obj);
            obj->field34 = 0;
            obj->field00->field34 = 0;
        }
    }

pop:
    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
        return 0;
    }
    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}
