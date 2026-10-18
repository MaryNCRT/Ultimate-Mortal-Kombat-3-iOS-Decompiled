/*
 * mkslam.c -- gamecode/logic/mkslam.c, decompiled.
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

long t_common_slam(struct MK3THREAD *thread);
long t_noob_slam(struct MK3THREAD *thread);

/* t_nj_smoke_slam -- armv7 0x00049c7c, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = t_noob_slam
 *      frame[frame+1].w0 = 0
 */

long t_nj_smoke_slam(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_noob_slam);
}

/* t_thrown_by_lao -- armv7 0x00049e28, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->a10 = 0x20
 *      frame[frame].handler = t_common_slam
 *      frame[frame+1].w0 = 0
 */

long t_thrown_by_lao(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->a10 = 0x20;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_common_slam);
}





/* --------------------------------------------------------------------
 * What the readers could prove. See tools/pushfn.py, which executes
 * a body symbolically, and tools/microfn.py, which matches whole
 * bodies against templates. Both refuse anything they cannot account
 * for instruction by instruction.
 * -------------------------------------------------------------------- */


/* t_thrown_by_jax -- armv7 0x00049df0, 56 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->a10 = 0   (the register the guard proved)
 *      frame[frame].handler = t_common_slam
 *      frame[frame+1].w0 = 0
 */

long t_thrown_by_jax(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->a10 = 0;   /* the guard proved this register */

    return mk3_push_handler(thread, (MK3THREADFUNC)t_common_slam);
}

/* --------------------------------------------------------------------
 * Straight-line leaves, read by tools/leaffn.py: stores, calls and
 * a return, with every instruction accounted for. It refuses
 * anything that branches, any return value it cannot prove, and any
 * value read from a field the function also writes -- that is a
 * saved value being put back, not a re-read.
 * -------------------------------------------------------------------- */

long do_next_a9_frame(MK3OBJ *obj);
void find_last_frame(MK3OBJ *obj);
void get_char_ani(MK3OBJ *obj);
void group_sound(MK3OBJ *obj);

/* throw_voice -- armv7 0x00049fdc, 16 bytes.  **Complete.**
 *
 *      obj->field1c = 0x4
 *      group_sound(obj)
 */
void throw_voice(MK3OBJ *obj)
{
    obj->field1c = 0x4;
    group_sound(obj);
}


/* grab_voice -- armv7 0x00049fec, 16 bytes.  **Complete.**
 *
 *      obj->field1c = 0x3
 *      group_sound(obj)
 */
void grab_voice(MK3OBJ *obj)
{
    obj->field1c = 0x3;
    group_sound(obj);
}


/* last_knockdown_frame -- armv7 0x0004b6b8, 28 bytes.  **Complete.**
 *
 *      obj->field40 = 0x1e
 *      get_char_ani(obj)
 *      find_last_frame(obj)
 *      do_next_a9_frame(obj)
 */
void last_knockdown_frame(MK3OBJ *obj)
{
    obj->field40 = 0x1e;
    get_char_ani(obj);
    find_last_frame(obj);
    do_next_a9_frame(obj);
}

/* --------------------------------------------------------------------
 * Added by a later sweep -- tools/sweep.py, running the same
 * readers again after one of them learned something. Each still
 * refuses anything it cannot account for instruction by
 * instruction; see tools/pushfn.py and tools/leaffn.py.
 * -------------------------------------------------------------------- */

long t_njsl3(struct MK3THREAD *thread);
void body_slam_init(MK3OBJ *obj);

/* t_nj_slam -- armv7 0x0004a3ac, 64 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      body_slam_init(obj)
 *      frame[frame].handler = t_njsl3
 *      frame[frame+1].w0 = 0
 */

long t_nj_slam(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    body_slam_init(obj);

    return mk3_push_handler(thread, (MK3THREADFUNC)t_njsl3);
}

/* --------------------------------------------------------------------
 * Added by a later sweep -- tools/sweep.py, running the same
 * readers again after one of them learned something. Each still
 * refuses anything it cannot account for instruction by
 * instruction; see tools/pushfn.py and tools/leaffn.py.
 * -------------------------------------------------------------------- */

long t_flight(MK3THREAD *thread);
long t_land_on_my_back(MK3THREAD *thread);
void damage_to_me(MK3OBJ *obj);

/* t_post_broken_back -- armv7 0x00049ebc, 152 bytes.  **Complete.**
 *
 *      token == 0:
 *          obj->field1c = 0x38000
 *          obj->field20 = 0xfff80000
 *          obj->field24 = 0xa000
 *          obj->field28 = 0xfff
 *          token := 0x429, then descend into t_flight
 *      token == 0x429:
 *          frame[frame].handler = t_land_on_my_back
 *      otherwise:  return -3
 */
long t_post_broken_back(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field1c = 0x38000;
        obj->field20 = 0xfff80000;
        obj->field24 = 0xa000;
        obj->field28 = 0xfff;
        *mk3_frame(thread, thread->frame + 1) = 0x429;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x429)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_land_on_my_back);
}

/* t_thrown_by_sz -- armv7 0x0004b6d4, 156 bytes.  **Complete.**
 *
 *      token == 0:
 *          obj->field1c = 0x60000
 *          obj->field20 = 0xfffa0000
 *          obj->field24 = 0xa000
 *          obj->field28 = 0xfff
 *          token := 0x413, then descend into t_flight
 *      token == 0x413:
 *          obj->a10 = 0x19
 *          damage_to_me(obj)
 *          frame[frame].handler = t_land_on_my_back
 *      otherwise:  return -3
 */
long t_thrown_by_sz(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field1c = 0x60000;
        obj->field20 = 0xfffa0000;
        obj->field24 = 0xa000;
        obj->field28 = 0xfff;
        *mk3_frame(thread, thread->frame + 1) = 0x413;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x413)
        return -3;

    obj->a10 = 0x19;
    damage_to_me(obj);
    return mk3_install(thread, (MK3THREADFUNC)t_land_on_my_back);
}


/* --------------------------------------------------------- read_lp_tick_state
 *
 * armv7 0x00049cb0, 32 bytes.  **Complete.**
 *
 *      obj->field1c =
 *          *(uint16_t *)(G + 0x3ac + obj->field00->field08 * 2)
 *
 * Two halfwords side by side, one per player, indexed by the strength index
 * doubled -- the same per-player packing the button masks and clear_combo_butn
 * use. A leaf: no frame, no calls, and 0x1c is written twice because the address
 * lands there first and the value replaces it.
 *
 * **Void, not long.** The routine ends on the store and r0 still holds the
 * argument at the `bx lr`, which is not a computed return value -- the same
 * reading applies to air_slam_init, ground_slammed_init, stick_him_with_me and
 * body_slam_init below, each of which ends on a store or a void call. If a
 * caller turns up testing r0 after one of these, that is a real puzzle and not
 * a licence to invent the value here.
 */
void read_lp_tick_state(MK3OBJ *obj)
{
    obj->field1c = *(uint16_t *)(G_BYTES + 0x3ac
                                + obj->field00->field08 * 2);
}

/* -------------------------------------------------- xfer_him_to_flipped_pause
 *
 * armv7 0x00049ffc, 44 bytes.  **Complete.**
 *
 *      saved = obj->field1c
 *      obj->field00->field18 = obj->field1c
 *      obj->field38 = t_flipped_pause
 *      takeover_him_sr(obj)
 *      obj->field1c = saved
 *      obj->field20 = (uint32_t)obj->field00->field00
 *      obj->field00->field00->field48 = saved
 *
 * **The handover puts a handler in 0x38 and lets takeover_him_sr install it on
 * the other fighter.** 0x1c is the action, copied into the proc's 0x18 before
 * the call and saved across it because the call disturbs it, and then written
 * into the OTHER object's 0x48 afterwards -- so both sides end up holding the
 * same number, one as an action and one as an argument.
 *
 * 0x20 is left holding the other object's address, which is the only place this
 * routine records who was taken over.
 */
long t_flipped_pause(MK3THREAD *thread);
void takeover_him_sr(MK3OBJ *obj);

void xfer_him_to_flipped_pause(MK3OBJ *obj)
{
    uint32_t saved = obj->field1c;

    obj->field00->field18 = obj->field1c;
    obj->field38 = (uint32_t)(uintptr_t)t_flipped_pause;
    takeover_him_sr(obj);

    obj->field1c = saved;
    obj->field20 = (uint32_t)(uintptr_t)obj->field00->field00;
    obj->field00->field00->field48 = saved;
}

/* ------------------------------------------------------------ t_do_body_slam
 *
 * armv7 0x00049f54, 68 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = body_slam_jumps[obj->field08->field24]
 *      frame[frame+1].w0 = 0
 *
 * One table lookup and one install. The table is indexed by the character
 * number, so every fighter's body slam is named here and nowhere else -- this is
 * the entry point the rest of the file hangs off.
 */
extern uint32_t body_slam_jumps[];              /* 0x00167288 */

long t_do_body_slam(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    mk3_frame(thread, thread->frame)[1] =
        body_slam_jumps[obj->field08->field24];
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}

/* ------------------------------------------------------------- t_do_air_slam
 *
 * armv7 0x00049f98, 68 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = air_slam_jumps[obj->field08->field24]
 *      frame[frame+1].w0 = 0
 *
 * The same routine as t_do_body_slam against a second table. **This is the
 * handler DoASpecial reaches through pointer slot 0x000f31a8**, given to `which`
 * 4 for characters 2 and 8 and `which` 9 for character 8 -- so an air slam
 * request lands here and is turned into the character's own version.
 */
extern uint32_t air_slam_jumps[];               /* 0x001672f0 */

long t_do_air_slam(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    mk3_frame(thread, thread->frame)[1] =
        air_slam_jumps[obj->field08->field24];
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}

/* -------------------------------------------------------------- air_slam_init
 *
 * armv7 0x0004a09c, 80 bytes.  **Complete.**
 *
 *      air_init_special(obj); lights_on_slam(obj)
 *      saved40 = obj->field40
 *      obj->field40 = obj->field48
 *      get_his_char_ani(obj)
 *      obj->a10 = obj->field00->him
 *      obj->field48 = obj->field40
 *      obj->field40 = saved40
 *      get_char_ani2(obj)
 *      obj->field1c = 0x504
 *      xfer_him_to_flipped_pause(obj)
 *      obj->field1c = 0x30
 *      *(uint16_t *)(G + 0x456) = 0x30
 *
 * **0x40 is the animation register and it is borrowed for one call.** 0x48 goes
 * into it, get_his_char_ani turns that into the opponent's animation IN 0x40,
 * the result is taken back out into 0x48, and 0x40 is restored -- so the routine
 * ends with the opponent's animation in 0x48 and its own untouched, and then
 * get_char_ani2 fills 0x40 for this fighter. Two animations resolved through one
 * field.
 *
 * The 0x30 written to G + 0x456 is the first of the three consecutive halfwords
 * at 0x456, 0x45a and 0x45c, and 0x1c is left holding the same 0x30. 0x504 is in
 * 0x1c only long enough for xfer_him_to_flipped_pause to copy it into the
 * opponent's action and 0x48.
 */
void air_init_special(MK3OBJ *obj);
void lights_on_slam(MK3OBJ *obj);
void get_his_char_ani(MK3OBJ *obj);
void get_char_ani2(MK3OBJ *obj);
void xfer_him_to_flipped_pause(MK3OBJ *obj);

void air_slam_init(MK3OBJ *obj)
{
    uint32_t saved40;

    air_init_special(obj);
    lights_on_slam(obj);

    saved40 = obj->field40;
    obj->field40 = obj->field48;
    get_his_char_ani(obj);

    obj->a10 = obj->field00->him;
    obj->field48 = obj->field40;
    obj->field40 = saved40;
    get_char_ani2(obj);

    obj->field1c = 0x504;
    xfer_him_to_flipped_pause(obj);

    obj->field1c = 0x30;
    *(uint16_t *)(G_BYTES + 0x456) = 0x30;
}

/* ------------------------------------------------------------ xfer_to_thrown
 *
 * armv7 0x0004a3ec, 20 bytes.  **Complete.**
 *
 *      takeover_him(obj)
 *      obj->field00->field00->field40 = obj->field48
 *
 * Hand the other fighter over, then put this object's 0x48 into that fighter's
 * 0x40 -- the animation register -- so what was chosen here becomes what plays
 * there. Two hops to reach it: the proc, then the proc's 0x00, which is the
 * other fighter's object.
 */
void takeover_him(MK3OBJ *obj);

void xfer_to_thrown(MK3OBJ *obj)
{
    takeover_him(obj);
    obj->field00->field00->field40 = obj->field48;
}

/* ------------------------------------------------------ t_air_slamed_by_robo2
 *
 * armv7 0x0004b87c, 80 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      inc_p_hit(obj)
 *      obj->a10 = 0x19
 *      obj->field1c = obj->field00->field54 + 0x19
 *      obj->field00->field54 = obj->field1c
 *      frame[frame].handler = t_common_slam
 *      frame[frame+1].w0 = 0
 *
 * **0x19 is the damage and it is written in two places at once**: into 0x44 as
 * the argument for whatever reads it, and added to the proc's 0x54, which is
 * where add_combo_damage accumulates. Twenty-five points, then straight into the
 * shared slam body.
 */
void inc_p_hit(MK3OBJ *obj);

long t_air_slamed_by_robo2(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    inc_p_hit(obj);
    obj->a10 = 0x19;
    obj->field1c = obj->field00->field54 + 0x19;
    obj->field00->field54 = obj->field1c;

    return mk3_install(thread, (MK3THREADFUNC)t_common_slam);
}

/* -------------------------------------------------------- ground_slammed_init
 *
 * armv7 0x0004b8cc, 48 bytes.  **Complete.**
 *
 *      saved = obj->a10
 *      ground_player(obj)
 *      obj->field40 = 0x1e
 *      find_ani_part2(obj); do_next_a9_frame(obj)
 *      obj->field1c = 2
 *      group_sound(obj); shake_n_sound(obj)
 *      obj->a10 = saved
 *
 * **0x44 is saved across the whole body and put back at the end**, so one of
 * the five calls uses it as its own argument slot and this routine will not let
 * that show. Nothing else here reads it.
 *
 * The middle three lines are the same three t_collapse_on_ground ends with --
 * animation 0x1e, find the part, advance a frame -- so a body hitting the floor
 * looks the same whether it was slammed or it fell. What differs is the tail:
 * that one nudges the corpse into place from a table, this one makes noise.
 */
void ground_player(MK3OBJ *obj);
void find_ani_part2(MK3OBJ *obj);
void shake_n_sound(MK3OBJ *obj);

void ground_slammed_init(MK3OBJ *obj)
{
    uint32_t saved = obj->a10;

    ground_player(obj);

    obj->field40 = 0x1e;
    find_ani_part2(obj);
    do_next_a9_frame(obj);

    obj->field1c = 2;
    group_sound(obj);
    shake_n_sound(obj);

    obj->a10 = saved;
}

/* --------------------------------------------------------- stick_him_with_me
 *
 * armv7 0x0004bc48, 36 bytes.  **Complete.**
 *
 *      part  = obj->field08
 *      other = (MK3OBJ *)obj->a10
 *      obj->field1c = other->field18 = part->field18
 *      other->field1c = part->field1c
 *      obj->field08 = other
 *      set_x_vel_player(obj)
 *      obj->field08 = part
 *
 * **The swap is the whole routine.** set_x_vel_player works on whatever
 * obj->field08 points at, so to apply it to the object named in 0x44 the
 * pointer is swapped, the call made, and the pointer put back. The two fields
 * copied first -- 0x18 and 0x1c, the action and the rate -- are what make the
 * two move together, which is what sticking one to the other means.
 *
 * 0x44 is read twice more after the first load rather than kept, so the
 * compiler did not assume the copies left it alone.
 */
void set_x_vel_player(MK3OBJ *obj);

void stick_him_with_me(MK3OBJ *obj)
{
    MK3OBJ *part  = obj->field08;
    MK3OBJ *other = (MK3OBJ *)(void *)(uintptr_t)obj->a10;

    obj->field1c   = part->field18;
    other->field18 = part->field18;

    other = (MK3OBJ *)(void *)(uintptr_t)obj->a10;
    other->field1c = obj->field08->field1c;

    obj->field08 = (MK3OBJ *)(void *)(uintptr_t)obj->a10;
    set_x_vel_player(obj);
    obj->field08 = part;
}


/* ------------------------------------------------------------ t_bb_fall_call
 *
 * armv7 0x00049e64, 88 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      him = (MK3OBJ *)obj->field00->him
 *      obj->field1c = (uint32_t)him
 *      him->field1c = obj->field08->field1c
 *      if (thread->frame > 0) thread->frame -= 1
 *      else frame[frame].handler = t_local_reaction_exit
 *
 * **The rate is pushed across to the other fighter and then this level goes
 * away.** The part's 0x1c -- the animation rate -- is copied into the object
 * named by the proc's `him`, so the two fall at the same speed, and the routine
 * then pops itself off the frame stack rather than installing anything. The
 * empty-stack fallback is t_local_reaction_exit, the same one t_plwins and
 * t_finish_him use.
 *
 * 0x1c is left holding the other object's ADDRESS, which is a scratch value and
 * not a rate. That is the only record of who was written to.
 */
long t_local_reaction_exit(MK3THREAD *thread);   /* pointer slot 0x000f3708 */

long t_bb_fall_call(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    MK3OBJ *him;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    him = (MK3OBJ *)(void *)(uintptr_t)obj->field00->him;
    obj->field1c = (uint32_t)(uintptr_t)him;
    him->field1c = obj->field08->field1c;

    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
        return 0;
    }

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

/* ------------------------------------------------------------- t_slam_damage
 *
 * armv7 0x0004b770, 88 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      him = (MK3OBJ *)obj->field00->him
 *      obj->field1c = obj->a10 =
 *          (int16)ochar_slam_damage[him->field24]
 *      damage_to_me(obj)
 *      frame[frame].handler = t_land_on_my_back
 *
 * **The damage is indexed by the OTHER fighter's character, not this one's.**
 * The table is signed halfwords, reached with `ldrsh` and a one-bit shift, and
 * the index comes from `him->field24` -- so how much a slam hurts is decided by
 * who threw it. The number lands in 0x1c and in 0x44 before damage_to_me is
 * called, and the thread then goes to t_land_on_my_back.
 */
extern int16_t ochar_slam_damage[];              /* 0x00167238 */

long t_slam_damage(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    MK3OBJ *him;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    him = (MK3OBJ *)(void *)(uintptr_t)obj->field00->him;
    obj->field1c = (uint32_t)(int32_t)ochar_slam_damage[him->field24];
    obj->a10     = obj->field1c;

    damage_to_me(obj);
    return mk3_install(thread, (MK3THREADFUNC)t_land_on_my_back);
}

/* --------------------------------------------------------------- t_noob_slam
 *
 * armv7 0x0004a34c, 96 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      part  = obj->field08
 *      saved = part->field24
 *      obj->field1c = part->field24 = 0x12
 *      body_slam_init(obj)
 *      obj->field1c = obj->field08->field24 = saved
 *      frame[frame].handler = t_njsl3
 *
 * **Noob Saibot borrows another character's slam by lying about who he is.**
 * The part's character number is set to 0x12 -- Scorpion's number, on
 * DoASpecial's dispatch -- body_slam_init is run so every table it consults
 * answers for Scorpion, and the real number is then put back. Nothing else in
 * the routine does any work.
 *
 * The part pointer is reloaded after the call rather than kept, so the compiler
 * did not assume body_slam_init leaves obj->field08 alone. 0x1c takes a copy of
 * both numbers in turn and ends holding the real one.
 */
long t_njsl3(MK3THREAD *thread);

long t_noob_slam(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    MK3OBJ  *part;
    uint32_t saved;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    part  = obj->field08;
    saved = part->field24;

    obj->field1c   = 0x12;
    part->field24  = 0x12;
    body_slam_init(obj);

    obj->field1c = saved;
    obj->field08->field24 = saved;

    return mk3_install(thread, (MK3THREADFUNC)t_njsl3);
}

/* ------------------------------------------------------------ t_flipped_pause
 *
 * armv7 0x0004a028, 116 bytes.  **Complete.**
 *
 *      token == 0:      lights_on_slam(obj)
 *                       token := 0x57f, park 1
 *
 *      token == 0x57f:  obj->field1c =
 *                           obj->field00->field00->field00->field18
 *                       if (that == obj->field48) token := 0x57f, park 1
 *                       else {
 *                           ground_player(obj)
 *                           frame[frame].handler = t_local_reaction_exit
 *                       }
 *
 *      otherwise:       return -3
 *
 * **It waits for the other fighter to leave the action it was given.** Three
 * hops reach the number -- this proc, the other object, its proc, its 0x18 --
 * and it is compared against 0x48, which xfer_him_to_flipped_pause loaded with
 * the action it handed over. While the two still match the routine parks for one
 * frame and asks again; the moment they differ the fighter is grounded and the
 * level exits.
 *
 * So the pair is a handshake: one routine gives the opponent an action and
 * records it, and this one blocks until the opponent is done with it. The
 * one-frame park means the check runs every frame.
 */
long t_flipped_pause(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        lights_on_slam(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x57f;
        thread->fieldfc = 1;
        return 1;
    }

    if (token != 0x57f)
        return -3;

    obj->field1c = obj->field00->field00->field00->field18;
    if (obj->field1c == obj->field48) {
        *mk3_frame(thread, thread->frame + 1) = 0x57f;
        thread->fieldfc = 1;
        return 1;
    }

    ground_player(obj);
    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

/* ---------------------------------------------------------- t_grab_animation
 *
 * armv7 0x0004a0ec, 116 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      push obj->field1c
 *      obj->field40 = 0x23
 *      get_char_ani(obj)
 *      obj->field1c = pop
 *      frame[frame].handler = t_mframew
 *
 * **The duration has to survive the animation lookup, so it goes on the
 * thread's argument stack.** get_char_ani writes through 0x1c, and the caller's
 * value is what t_mframew must wait for, so it is pushed at 0xa8 through the
 * cursor at 0xf8 and popped straight back. One push, one pop, nothing in
 * between but the one call.
 *
 * Animation 0x23 is the grab, and the same number appears in
 * t_jax_multi_slam_pose as the part it looks up -- so the two agree on which
 * animation a grab is.
 */
long t_mframew(MK3THREAD *thread);

long t_grab_animation(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t cur;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    cur = thread->fieldf8;
    *mk3_arg(thread, cur) = obj->field1c;
    thread->fieldf8 = cur + 1;

    obj->field40 = 0x23;
    get_char_ani(obj);

    cur = thread->fieldf8 - 1;
    thread->fieldf8 = cur;
    obj->field1c = *mk3_arg(thread, cur);

    return mk3_install(thread, (MK3THREADFUNC)t_mframew);
}

/* ----------------------------------------------------- t_jax_multi_slam_pose
 *
 * armv7 0x0004bc6c, 136 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      n = obj->field30
 *      obj->field40 = 0x28
 *      get_his_char_ani(obj)
 *      obj->field48 = obj->field40
 *      obj->a10 = obj->field00->him
 *      obj->field40 = 0x23
 *      find_ani_part2(obj)
 *      obj->field2c = n * 4
 *      obj->field40 = obj->field40 + n * 4
 *      obj->field30 = n
 *      obj->field48 = obj->field48 + n * 16
 *      double_next_a9(obj)
 *      obj->field1c = 3
 *      frame[frame].handler = t_jax_slam_sleep
 *
 * **The repetition count scales two animation offsets, by four and by sixteen.**
 * 0x30 holds how many slams have happened; both lookups are done first and then
 * shifted along by the count, so the second and third slam of the sequence show
 * later frames of the same two animations rather than restarting them. That is
 * the whole of what makes a multi-slam look like a sequence.
 *
 * Two animations again through one register, as in air_slam_init: 0x28 through
 * get_his_char_ani into 0x48, then 0x23 through find_ani_part2 into 0x40.
 *
 * **The store back to 0x30 is dead** -- the value was never changed -- and
 * 0x2c gets `n * 4` as a by-product of computing the first offset. Transcribed
 * as written.
 */
long t_jax_slam_sleep(MK3THREAD *thread);
long double_next_a9(MK3OBJ *obj);

long t_jax_multi_slam_pose(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    uint32_t n;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    n = obj->field30;

    obj->field40 = 0x28;
    get_his_char_ani(obj);
    obj->field48 = obj->field40;

    obj->a10 = obj->field00->him;

    obj->field40 = 0x23;
    find_ani_part2(obj);

    obj->field2c = n * 4;
    obj->field40 = obj->field40 + n * 4;
    obj->field30 = n;                       /* dead: n came from here */
    obj->field48 = obj->field48 + n * 16;

    double_next_a9(obj);
    obj->field1c = 3;

    return mk3_install(thread, (MK3THREADFUNC)t_jax_slam_sleep);
}


/* --------------------------------------------------------- t_thrown_by_sonya
 *
 * armv7 0x00049cd0, 144 bytes.  **Complete.**
 *
 *      token == 0:      obj->field20 = 0
 *                       obj->field1c = 0x80000
 *                       obj->field24 = 0x80000 - 0x7a000 = 0x6000
 *                       obj->field28 = 6
 *                       token := 0x391, descend into t_flight
 *
 *      token == 0x391:  frame[frame].handler = t_slam_damage
 *
 *      otherwise:       return -3
 *
 * **A throw is four numbers and a flight.** 0x1c, 0x20, 0x24 and 0x28 are
 * filled and t_flight is handed the thread; when it comes back the fighter takes
 * its damage. Nothing else happens here at all.
 *
 * The second magnitude is built by subtracting from the first -- `sub r3, r3,
 * #0x7a000` -- so the compiler spent one literal on the pair. Only the results
 * mean anything.
 *
 * t_thrown_by_nj below is the identical routine with the two magnitudes and the
 * token changed, which is what makes the four fields readable as a set: two
 * throws differ in nothing else. */
long t_slam_damage(MK3THREAD *thread);

long t_thrown_by_sonya(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field20 = 0;
        obj->field1c = 0x80000;
        obj->field24 = 0x80000 - 0x7a000;
        obj->field28 = 6;

        *mk3_frame(thread, thread->frame + 1) = 0x391;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x391)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_slam_damage);
}

/* ------------------------------------------------------------ t_thrown_by_nj
 *
 * armv7 0x00049d60, 144 bytes.  **Complete.**
 *
 * t_thrown_by_sonya with 0x90000 for 0x80000, 0x5000 for 0x6000 -- again built
 * as `0x90000 - 0x8b000` -- and 0x39e for the token. 0x20 = 0 and 0x28 = 6 are
 * the same in both, so those two are the fixed part of a throw and the other two
 * are what the ninja's differs by.
 */
long t_thrown_by_nj(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field20 = 0;
        obj->field1c = 0x90000;
        obj->field24 = 0x90000 - 0x8b000;
        obj->field28 = 6;

        *mk3_frame(thread, thread->frame + 1) = 0x39e;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x39e)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_slam_damage);
}

/* ------------------------------------------------------------- body_slam_init
 *
 * armv7 0x0004a160, 148 bytes.  **Complete.**
 *
 *      grab_voice(obj); init_special(obj); lights_on_slam(obj)
 *      obj->field1c = 0x30
 *      *(uint16_t *)(G + 0x456) = 0x30
 *      a = ochar_slammed_anis[obj->field08->field24]
 *      obj->field1c = obj->field40 = a
 *      get_his_char_ani(obj)
 *      obj->field48 = obj->field40
 *      obj->a10 = obj->field00->him
 *      obj->field40 = 0x23
 *      get_char_ani(obj)
 *      face_opponent_px(obj, obj->field00->field00)
 *      back_to_normal_px(obj, obj->field00->field00)
 *      obj->field1c = (uint32_t)clear_inviso
 *      call_a0_for_him(obj)
 *      obj->field1c = 0x504
 *      xfer_him_to_flipped_pause(obj)
 *      obj->field1c = 0x32
 *      create_fx(obj)
 *
 * **The same borrow-0x40 dance as air_slam_init, with the borrowed value out of
 * a per-character byte table.** ochar_slammed_anis says which animation the
 * SLAMMED fighter plays, indexed by this fighter's character number; it goes
 * into 0x40, get_his_char_ani resolves it there, and the result is moved to
 * 0x48. 0x40 is then set to 0x23 -- the grab, the same number t_grab_animation
 * and t_jax_multi_slam_pose use -- and resolved for this fighter. Unlike
 * air_slam_init, the original 0x40 is not saved: nothing here needs it back.
 *
 * **0x1c carries a FUNCTION POINTER for one call.** `clear_inviso` is loaded
 * into it and call_a0_for_him is what runs it, so a fighter being slammed has
 * its invisibility cleared by handing the routine across rather than by calling
 * it here. That is what 0x1c-as-A0 means, and this is the clearest site for it:
 * the same field is a duration two lines later and a count two lines after
 * that.
 *
 * The four values 0x1c takes in order -- 0x30, the animation, clear_inviso,
 * 0x504, 0x32 -- are each consumed by the call that follows them, which is why
 * they can share one field.
 */
extern uint8_t ochar_slammed_anis[];             /* 0x0016726a */
void init_special(MK3OBJ *obj);
void clear_inviso(MK3OBJ *obj);
void face_opponent_px(MK3OBJ *obj, MK3OBJ *target);
void back_to_normal_px(MK3OBJ *obj, MK3OBJ *target);
void call_a0_for_him(MK3OBJ *obj);
void create_fx(MK3OBJ *obj);

void body_slam_init(MK3OBJ *obj)
{
    uint32_t a;

    grab_voice(obj);
    init_special(obj);
    lights_on_slam(obj);

    obj->field1c = 0x30;
    *(uint16_t *)(G_BYTES + 0x456) = 0x30;

    a = ochar_slammed_anis[obj->field08->field24];
    obj->field1c = a;
    obj->field40 = a;
    get_his_char_ani(obj);
    obj->field48 = obj->field40;

    obj->a10 = obj->field00->him;
    obj->field40 = 0x23;
    get_char_ani(obj);

    face_opponent_px(obj, obj->field00->field00);
    back_to_normal_px(obj, obj->field00->field00);

    obj->field1c = (uint32_t)(uintptr_t)clear_inviso;
    call_a0_for_him(obj);

    obj->field1c = 0x504;
    xfer_him_to_flipped_pause(obj);

    obj->field1c = 0x32;
    create_fx(obj);
}


/* ---------------------------------------------------------- t_drop_down_land
 *
 * armv7 0x00049b30, 148 bytes.  **Complete.**
 *
 *      token == 0:      obj->field1c = 0
 *                       obj->field20 = 0x18000
 *                       obj->field24 = 0x18000 - 0x10000 = 0x8000
 *                       obj->field28 = 0xfff
 *                       token := 0x14f, descend into t_flight
 *
 *      token == 0x14f:  frame[frame].handler = t_jump_up_land_jsrp
 *
 *      otherwise:       return -3
 *
 * **The same four fields as a throw, filled the other way round.** Both
 * t_thrown_by_sonya and t_thrown_by_nj put zero in 0x20 and the magnitude in
 * 0x1c; this one puts zero in 0x1c and the magnitude in 0x20. So the two fields
 * are two components and which of them a routine zeroes says whether the flight
 * is sideways or straight down.
 *
 * 0x28 is 0xfff here against 6 in the throws -- three orders of magnitude apart,
 * so it is not a frame count in both. Recorded, not named.
 */
long t_jump_up_land_jsrp(MK3THREAD *thread);     /* pointer slot 0x000f376c */

long t_drop_down_land(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field1c = 0;
        obj->field20 = 0x18000;
        obj->field24 = 0x18000 - 0x10000;
        obj->field28 = 0xfff;

        *mk3_frame(thread, thread->frame + 1) = 0x14f;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x14f)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_jump_up_land_jsrp);
}

/* ------------------------------------------------------------- t_common_slam
 *
 * armv7 0x0004b9d0, 164 bytes.  **Complete.**
 *
 *      token == 0:      ground_slammed_init(obj)
 *                       if (obj->a10 != 0) damage_to_me(obj)
 *                       obj->field1c = 0x30000
 *                       obj->field20 = 0x30000 - 0x90000 = -0x60000
 *                       obj->field24 = -0x60000 + 0x68000 = 0x8000
 *                       obj->field28 = 4
 *                       token := 0x3f3, descend into t_flight
 *
 *      token == 0x3f3:  frame[frame].handler = t_land_on_my_back
 *
 *      otherwise:       return -3
 *
 * **0x44 is the damage and zero means none.** t_air_slamed_by_robo2 puts 0x19
 * there before installing this routine, and every other caller that wants the
 * hit to hurt does the same; a caller that leaves it clear gets the bounce
 * without the damage. That is the whole of the conditional.
 *
 * The three magnitudes are chained off one literal again -- 0x30000, minus
 * 0x90000, plus 0x68000 -- and the middle one is NEGATIVE, which is the first
 * flight in this file to be so. 0x24 lands on 0x8000, the same value
 * t_drop_down_land computes for it by a different route, so 0x24 is the one
 * field the two agree on.
 */
long t_land_on_my_back(MK3THREAD *thread);       /* pointer slot 0x000f3750 */
void ground_slammed_init(MK3OBJ *obj);

long t_common_slam(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        ground_slammed_init(obj);
        if (obj->a10 != 0)
            damage_to_me(obj);

        obj->field1c = 0x30000;
        obj->field20 = (uint32_t)(0x30000 - 0x90000);
        obj->field24 = (uint32_t)(0x30000 - 0x90000 + 0x68000);
        obj->field28 = 4;

        *mk3_frame(thread, thread->frame + 1) = 0x3f3;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x3f3)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_land_on_my_back);
}

/* ---------------------------------------------------------- t_jax_slam_sleep
 *
 * armv7 0x0004c4d8, 164 bytes.  **Complete.**
 *
 *      token == 0:      token := 0x20e, park obj->field1c
 *
 *      token == 0x20e:  obj->field1c = G + 0x3a8
 *                       get_tsl_px(obj, obj)
 *                       if (obj->field20 > 0xc) {
 *                           obj->field1c = 1
 *                           *(uint32_t *)((char *)obj->field00 + 0x2c) = 1
 *                       } else {
 *                           read_lp_tick_state(obj)
 *                           obj->field20 = obj->field00->field28
 *                           if (obj->field20 != obj->field1c) {
 *                               obj->field1c = 1
 *                               *(uint32_t *)(proc + 0x2c) = 1
 *                           }
 *                       }
 *                       pop a level, or t_local_reaction_exit at the bottom
 *
 *      otherwise:       return -3
 *
 * **This is where holding the button decides whether Jax slams again.**
 * get_tsl_px is asked with G + 0x3a8 in 0x1c and answers in 0x20; above twelve
 * the flag at the proc's 0x2c goes up unconditionally. At or below it, the
 * routine reads the per-player halfword at G + 0x3ac -- read_lp_tick_state, the
 * neighbouring four bytes -- and raises the same flag only if it DISAGREES with
 * the proc's 0x28. So a long enough press counts, and a short one counts only
 * if the tick state changed.
 *
 * The flag and the pop are shared by all three paths, which is why the equal
 * case branches past two stores into the middle of the other arm.
 *
 * 0x2c on the proc is inside a pad in this header, so it is written by offset --
 * the same expression mkprop.c uses for it. */
void get_tsl_px(MK3OBJ *obj, MK3OBJ *ref);
void read_lp_tick_state(MK3OBJ *obj);

long t_jax_slam_sleep(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    int      raise;

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x20e;
        thread->fieldfc = obj->field1c;
        return (long)obj->field1c;
    }

    if (token != 0x20e)
        return -3;

    obj->field1c = (uint32_t)(uintptr_t)(G_BYTES + 0x3a8);
    get_tsl_px(obj, obj);

    if ((long)obj->field20 > 0xc) {
        raise = 1;
    } else {
        read_lp_tick_state(obj);
        obj->field20 = obj->field00->field28;
        raise = (obj->field20 != obj->field1c);
    }

    if (raise) {
        obj->field1c = 1;
        *(uint32_t *)((char *)obj->field00 + 0x2c) = 1;
    }

    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
        return 0;
    }

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* ------------------------------------------------------------- t_indian_slam
 *
 * armv7 0x0004a2a0, 172 bytes.  **Complete.**
 *
 *      token == 0:      body_slam_init(obj)
 *                       obj->field1c = 3
 *                       token := 0x2c6, descend into t_grab_animation
 *
 *      token == 0x2c6:  token := 0x2c7, park 3
 *
 *      token == 0x2c7:  throw_voice(obj)
 *                       obj->field1c = 3
 *                       frame[frame].handler = t_slam_ani2
 *
 *      otherwise:       return -3
 *
 * **The shape every per-character slam in this file follows**: set the fighter
 * up, grab, wait, shout, throw. body_slam_init does all the table work, so what
 * is left per character is three constants -- the grab duration, the park and
 * the throw duration -- and here all three are 3.
 *
 * t_st_slam below is the same routine with 0x2d9 and 0x2da for the tokens and
 * nothing else changed, which is what makes the shape readable as a template
 * rather than a coincidence.
 */
long t_grab_animation(MK3THREAD *thread);
long t_slam_ani2(MK3THREAD *thread);

long t_indian_slam(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        body_slam_init(obj);
        obj->field1c = 3;

        *mk3_frame(thread, thread->frame + 1) = 0x2c6;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_grab_animation;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x2c6) {
        *mk3_frame(thread, thread->frame + 1) = 0x2c7;
        thread->fieldfc = 3;
        return 3;
    }

    if (token != 0x2c7)
        return -3;

    throw_voice(obj);
    obj->field1c = 3;
    return mk3_install(thread, (MK3THREADFUNC)t_slam_ani2);
}

/* ----------------------------------------------------------------- t_st_slam
 *
 * armv7 0x0004a1f4, 172 bytes.  **Complete.**
 *
 * t_indian_slam with 0x2d9 and 0x2da for the two tokens. Same three durations,
 * same four calls, same order. The only thing a character's slam carries is its
 * token pair when the durations happen to agree.
 */
long t_st_slam(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        body_slam_init(obj);
        obj->field1c = 3;

        *mk3_frame(thread, thread->frame + 1) = 0x2d9;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_grab_animation;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x2d9) {
        *mk3_frame(thread, thread->frame + 1) = 0x2da;
        thread->fieldfc = 3;
        return 3;
    }

    if (token != 0x2da)
        return -3;

    throw_voice(obj);
    obj->field1c = 3;
    return mk3_install(thread, (MK3THREADFUNC)t_slam_ani2);
}

/* ---------------------------------------------------------- t_thrown_by_kano
 *
 * armv7 0x0004b7c8, 180 bytes.  **Complete.**
 *
 *      token == 0:      obj->field1c = 0xa0000
 *                       obj->field20 = 0xa0000 - 0xb0000 = -0x10000
 *                       obj->field24 = -0x10000 + 0x16000 = 0x6000
 *                       obj->field28 = 6
 *                       token := 0x117, descend into t_flight
 *
 *      token == 0x117:  him = (MK3OBJ *)obj->field00->him
 *                       obj->field1c = obj->a10 =
 *                           (int16)ochar_slam_damage[him->field24]
 *                       damage_to_me(obj)
 *                       frame[frame].handler = t_land_on_my_back
 *
 *      otherwise:       return -3
 *
 * **t_thrown_by_sonya with t_slam_damage written out instead of installed.**
 * The second state is that routine's body line for line -- the same table, the
 * same signed halfword indexed by the other fighter's character, the same two
 * stores and the same handler afterwards -- so the compiler inlined what the
 * other two throws call. Worth knowing before assuming a throw always ends in a
 * separate damage state.
 *
 * The first state's 0x20 is negative here, as in t_common_slam, and 0x24 lands
 * on 0x6000, which is what t_thrown_by_sonya computes for it. Three throws now
 * agree that 0x24 is one of a small set of fixed magnitudes.
 */
long t_thrown_by_kano(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    MK3OBJ  *him;

    if (token == 0) {
        obj->field1c = 0xa0000;
        obj->field20 = (uint32_t)(0xa0000 - 0xb0000);
        obj->field24 = (uint32_t)(0xa0000 - 0xb0000 + 0x16000);
        obj->field28 = 6;

        *mk3_frame(thread, thread->frame + 1) = 0x117;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x117)
        return -3;

    him = (MK3OBJ *)(void *)(uintptr_t)obj->field00->him;
    obj->field1c = (uint32_t)(int32_t)ochar_slam_damage[him->field24];
    obj->a10     = obj->field1c;

    damage_to_me(obj);
    return mk3_install(thread, (MK3THREADFUNC)t_land_on_my_back);
}


/* ----------------------------------------------------- t_drop_down_land_jump
 *
 * armv7 0x00049bc4, 184 bytes.  **Complete.**
 *
 *      token == 0:      obj->field1c = 0
 *                       obj->field20 = 0x18000
 *                       obj->field24 = 0x18000 - 0x10000 = 0x8000
 *                       obj->field28 = 0xfff
 *                       token := 0x159, descend into t_flight
 *
 *      token == 0x159:  token := 0x15a, descend into t_jump_up_land_jsrp
 *
 *      token == 0x15a:  frame[frame].handler = t_local_reaction_exit
 *
 *      otherwise:       return -3
 *
 * **t_drop_down_land with one difference, and the difference is descend versus
 * install.** The four flight numbers are identical to the word. That routine
 * INSTALLS t_jump_up_land_jsrp and is finished; this one DESCENDS into it, comes
 * back at 0x15a and exits through t_local_reaction_exit. So the pair is the same
 * landing used two ways: once as the end of a thread and once as a step inside
 * one.
 */
long t_drop_down_land_jump(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field1c = 0;
        obj->field20 = 0x18000;
        obj->field24 = 0x18000 - 0x10000;
        obj->field28 = 0xfff;

        *mk3_frame(thread, thread->frame + 1) = 0x159;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x159) {
        *mk3_frame(thread, thread->frame + 1) = 0x15a;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_jump_up_land_jsrp;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x15a)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

/* --------------------------------------------------------------- t_slam_ani2
 *
 * armv7 0x0004a550, 220 bytes.  **Complete.**
 *
 *      token == 0:      token := 0x2cb, descend into t_double_mframew
 *
 *      token == 0x2cb:  obj->field38 = t_thrown_by_lao
 *                       xfer_to_thrown(obj)
 *                       token := 0x2ce, park 0xa
 *
 *      token == 0x2ce:  obj->field1c = 8
 *                       token := 0x2d1, descend into t_mframew
 *
 *      token == 0x2d1:  pop a level, or t_local_reaction_exit at the bottom
 *
 *      otherwise:       return -3
 *
 * **This is where the victim is given its behaviour.** 0x38 is loaded with
 * t_thrown_by_lao and xfer_to_thrown hands the other fighter over to it, so the
 * slammer's animation state is what decides what the slammed fighter does --
 * which is why t_thrown_by_* exists as a family and nothing installs those
 * routines on its own thread.
 *
 * The two waits either side of the handover are a double one first --
 * t_double_mframew, off pointer slot 0x000f36a8 -- and a single one after, so
 * the throw lands between them.
 *
 * Both t_indian_slam and t_st_slam install this routine, so what they share is
 * not only their shape but their whole second half.
 */
long t_double_mframew(MK3THREAD *thread);        /* pointer slot 0x000f36a8 */
void xfer_to_thrown(MK3OBJ *obj);

long t_slam_ani2(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x2cb;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_double_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x2cb) {
        obj->field38 = (uint32_t)(uintptr_t)t_thrown_by_lao;
        xfer_to_thrown(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x2ce;
        thread->fieldfc = 0xa;
        return 0xa;
    }

    if (token == 0x2ce) {
        obj->field1c = 8;
        *mk3_frame(thread, thread->frame + 1) = 0x2d1;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x2d1)
        return -3;

    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
        return 0;
    }

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* ---------------------------------------------------------------- t_lao_slam
 *
 * armv7 0x0004b0b8, 248 bytes.  **Complete.**
 *
 *      token == 0:      body_slam_init(obj)
 *                       obj->field1c = 2
 *                       token := 0xc9, descend into t_grab_animation
 *
 *      token == 0xc9:   obj->field1c = 5; ochar_sound(obj)
 *                       obj->field1c = 3
 *                       token := 0xcf, descend into t_double_mframew
 *
 *      token == 0xcf:   obj->field38 = t_thrown_by_lao
 *                       xfer_to_thrown(obj)
 *                       obj->field1c = 0xd
 *                       obj->field20 = 0xd
 *                       obj->field24 = 0x8000
 *                       obj->field28 = 2
 *                       token := 0xd8, descend into t_flight
 *
 *      token == 0xd8:   frame[frame].handler = t_jump_up_land_jsrp
 *
 *      otherwise:       return -3
 *
 * **The slam and the flight in one routine.** Where t_indian_slam hands off to
 * t_slam_ani2 and stops, this one carries the sequence through: grab, sound,
 * double wait, hand the victim to t_thrown_by_lao, then fly and land. So the
 * four flight fields are filled by the SLAMMER here, not by the victim's thread.
 *
 * **0x1c and 0x20 get the same value, 0xd.** Every other flight in this file
 * zeroes one of the two; this is the only one that sets both to one number,
 * which is another reason not to name them yet.
 *
 * 0x1c is used three times over on the 0xc9 path -- a sound number, then a
 * duration -- and once more as a flight component two states later. Each value
 * is consumed before the next is written.
 */
void ochar_sound(MK3OBJ *obj);

long t_lao_slam(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        body_slam_init(obj);
        obj->field1c = 2;

        *mk3_frame(thread, thread->frame + 1) = 0xc9;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_grab_animation;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0xc9) {
        obj->field1c = 5;
        ochar_sound(obj);
        obj->field1c = 3;

        *mk3_frame(thread, thread->frame + 1) = 0xcf;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_double_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0xcf) {
        obj->field38 = (uint32_t)(uintptr_t)t_thrown_by_lao;
        xfer_to_thrown(obj);

        obj->field1c = 0xd;
        obj->field20 = 0xd;
        obj->field24 = 0x8000;
        obj->field28 = 2;

        *mk3_frame(thread, thread->frame + 1) = 0xd8;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0xd8)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_jump_up_land_jsrp);
}

/* --------------------------------------------------------------- t_tusk_slam
 *
 * armv7 0x0004ab08, 268 bytes.  **Complete.**
 *
 *      token == 0:      body_slam_init(obj)
 *                       obj->field1c = 3
 *                       token := 0xb4, descend into t_grab_animation
 *
 *      token == 0xb4:   throw_voice(obj)
 *                       obj->field1c = 3
 *                       token := 0xb7, descend into t_double_mframew
 *
 *      token == 0xb7:   obj->field38 = t_thrown_by_lao
 *                       xfer_to_thrown(obj)
 *                       token := 0xbb, park 0xa
 *
 *      token == 0xbb:   obj->field1c = 8
 *                       token := 0xbd, descend into t_mframew
 *
 *      token == 0xbd:   pop a level, or t_local_reaction_exit at the bottom
 *
 *      otherwise:       return -3
 *
 * **t_indian_slam's first half and t_slam_ani2's second half, fused into one
 * routine.** State by state it is the same sequence the pair performs across two
 * functions: grab, shout, double wait, hand the victim over, wait, leave. So the
 * template exists in the binary both split and inlined, which is the strongest
 * evidence that it is a template and not a coincidence.
 *
 * The durations are 3, 3, 0xa and 8 -- the same four t_indian_slam and
 * t_slam_ani2 use between them.
 */
long t_tusk_slam(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        body_slam_init(obj);
        obj->field1c = 3;

        *mk3_frame(thread, thread->frame + 1) = 0xb4;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_grab_animation;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0xb4) {
        throw_voice(obj);
        obj->field1c = 3;

        *mk3_frame(thread, thread->frame + 1) = 0xb7;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_double_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0xb7) {
        obj->field38 = (uint32_t)(uintptr_t)t_thrown_by_lao;
        xfer_to_thrown(obj);
        *mk3_frame(thread, thread->frame + 1) = 0xbb;
        thread->fieldfc = 0xa;
        return 0xa;
    }

    if (token == 0xbb) {
        obj->field1c = 8;
        *mk3_frame(thread, thread->frame + 1) = 0xbd;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0xbd)
        return -3;

    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
        return 0;
    }

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* ------------------------------------------------------------ t_thrown_by_sg
 *
 * armv7 0x0004b8fc, 212 bytes.  **Complete.**
 *
 *      token == 0:      ground_slammed_init(obj); inc_p_hit(obj)
 *                       obj->a10 = 0x23
 *                       obj->field1c = obj->field00->field54 + 0x23
 *                       obj->field00->field54 = obj->field1c
 *                       damage_to_me(obj); set_half_damage(obj)
 *                       token := 0x402, park 3
 *
 *      token == 0x402:  obj->field1c = 0x40000
 *                       obj->field20 = 0x40000 - 0xe0000 = -0xa0000
 *                       obj->field24 = -0xa0000 + 0xa8000 = 0x8000
 *                       obj->field28 = 4
 *                       token := 0x407, descend into t_flight
 *
 *      token == 0x407:  obj->a10 = 0
 *                       frame[frame].handler = t_common_slam
 *
 *      otherwise:       return -3
 *
 * **Zeroing 0x44 before t_common_slam is what stops the damage being taken
 * twice**, and it confirms what that field is for. This routine has already
 * applied 0x23 -- into 0x44, added to the proc's 0x54, then damage_to_me -- and
 * t_common_slam calls damage_to_me only when 0x44 is non-zero. So the last state
 * clears it deliberately, and the two routines are written to be chained.
 *
 * set_half_damage after the hit is the only site in this file that calls it, and
 * it comes after the damage rather than before.
 */
void set_half_damage(MK3OBJ *obj);

long t_thrown_by_sg(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        ground_slammed_init(obj);
        inc_p_hit(obj);

        obj->a10 = 0x23;
        obj->field1c = obj->field00->field54 + 0x23;
        obj->field00->field54 = obj->field1c;

        damage_to_me(obj);
        set_half_damage(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x402;
        thread->fieldfc = 3;
        return 3;
    }

    if (token == 0x402) {
        obj->field1c = 0x40000;
        obj->field20 = (uint32_t)(0x40000 - 0xe0000);
        obj->field24 = (uint32_t)(0x40000 - 0xe0000 + 0xa8000);
        obj->field28 = 4;

        *mk3_frame(thread, thread->frame + 1) = 0x407;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x407)
        return -3;

    obj->a10 = 0;
    return mk3_install(thread, (MK3THREADFUNC)t_common_slam);
}

/* ------------------------------------------------------ t_air_slamed_by_kano
 *
 * armv7 0x0004ba74, 232 bytes.  **Complete.**
 *
 *      token == 0:      obj->field1c = 0x60000
 *                       obj->field20 = 0x60000 - 0x30000 = 0x30000
 *                       obj->field24 = 0x30000 - 0x28000 = 0x8000
 *                       obj->field28 = 4
 *                       token := 0x1a0, descend into t_flight
 *
 *      token == 0x1a0:  obj->field1c = 2
 *                       group_sound(obj); shake_n_sound(obj)
 *                       obj->a10 = 0x10
 *                       damage_to_me(obj)
 *                       obj->field1c = 0x30000
 *                       obj->field20 = 0x30000 - 0x90000 = -0x60000
 *                       obj->field24 = -0x60000 + 0x68000 = 0x8000
 *                       obj->field28 = 4
 *                       token := 0x1af, descend into t_flight
 *
 *      token == 0x1af:  frame[frame].handler = t_land_on_my_back
 *
 *      otherwise:       return -3
 *
 * **Two flights in one routine with the impact between them**: up, then noise
 * and sixteen points of damage, then the bounce, then land on the back. The
 * second flight's three magnitudes are the same three t_common_slam uses --
 * 0x30000, minus 0x60000, 0x8000 -- so the bounce after a hit is one shape
 * shared between routines.
 *
 * **0x24 is 0x8000 in both flights, and in every flight in this file so far
 * except the two ninja throws.** Five sites now, each reaching it by different
 * arithmetic off a different literal, which is what makes 0x8000 look like a
 * fixed parameter of a flight rather than a per-move number. 0x28 is 4 here and
 * in t_common_slam and t_thrown_by_sg, 6 in the throws, 2 in t_lao_slam and
 * 0xfff in the two drop-downs.
 */
void group_sound(MK3OBJ *obj);

long t_air_slamed_by_kano(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field1c = 0x60000;
        obj->field20 = 0x60000 - 0x30000;
        obj->field24 = 0x60000 - 0x30000 - 0x28000;
        obj->field28 = 4;

        *mk3_frame(thread, thread->frame + 1) = 0x1a0;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x1a0) {
        obj->field1c = 2;
        group_sound(obj);
        shake_n_sound(obj);

        obj->a10 = 0x10;
        damage_to_me(obj);

        obj->field1c = 0x30000;
        obj->field20 = (uint32_t)(0x30000 - 0x90000);
        obj->field24 = (uint32_t)(0x30000 - 0x90000 + 0x68000);
        obj->field28 = 4;

        *mk3_frame(thread, thread->frame + 1) = 0x1af;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x1af)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_land_on_my_back);
}


/* ------------------------------------------------------------ t_robo2_air_slam
 *
 * armv7 0x0004c694, 236 bytes.  **Complete.**
 *
 *      token == 0:      obj->field48 = 0x41
 *                       obj->field40 = 6
 *                       air_slam_init(obj)
 *                       match_him_with_me_f(obj)
 *                       double_next_a9(obj)
 *                       token := 0x141, park 6
 *
 *      token == 0x141:  obj->field1c = 3
 *                       token := 0x143, descend into t_double_mframew
 *
 *      token == 0x143:  obj->field38 = t_air_slamed_by_robo2
 *                       xfer_to_thrown(obj)
 *                       token := 0x146, park 0xa
 *
 *      token == 0x146:  frame[frame].handler = t_drop_down_land
 *
 *      otherwise:       return -3
 *
 * **The air-slam counterpart of the ground template, and it shows what
 * air_slam_init wants set first.** 0x48 is loaded with 0x41 before the call --
 * that is the value air_slam_init moves into 0x40 and resolves through
 * get_his_char_ani, so it names the animation the VICTIM will play -- and 0x40
 * gets 6, which air_slam_init saves and restores untouched.
 *
 * The victim is handed to t_air_slamed_by_robo2, which is the routine that adds
 * 0x19 to the damage accumulator and goes on to t_common_slam. So the pair
 * splits the work: this thread flies and lands, the other takes the hit.
 *
 * The park of 6 is returned as the function's value, built from the same
 * register that held it -- one constant doing two jobs, as in t_combj.
 */
void match_him_with_me_f(MK3OBJ *obj);
long t_air_slamed_by_robo2(MK3THREAD *thread);
long t_drop_down_land(MK3THREAD *thread);

long t_robo2_air_slam(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field48 = 0x41;
        obj->field40 = 6;

        air_slam_init(obj);
        match_him_with_me_f(obj);
        double_next_a9(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x141;
        thread->fieldfc = 6;
        return 6;
    }

    if (token == 0x141) {
        obj->field1c = 3;
        *mk3_frame(thread, thread->frame + 1) = 0x143;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_double_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x143) {
        obj->field38 = (uint32_t)(uintptr_t)t_air_slamed_by_robo2;
        xfer_to_thrown(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x146;
        thread->fieldfc = 0xa;
        return 0xa;
    }

    if (token != 0x146)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_drop_down_land);
}

/* --------------------------------------------------------- t_thrown_by_robo2
 *
 * armv7 0x0004bb5c, 236 bytes.  **Complete.**
 *
 *      token == 0:      ground_player(obj)
 *                       obj->field40 = 0x1e
 *                       find_ani_part2(obj); do_next_a9_frame(obj)
 *                       obj->field1c = 2; group_sound(obj)
 *                       rsnd_func(obj, 0xd)
 *                       obj->field48 = 0x00080008
 *                       shake_a11(obj); set_half_damage(obj)
 *                       obj->field1c = obj->field00->field18 = 0x62e
 *                       obj->a10 = obj->field00->field54 = 0x1a
 *                       damage_to_me(obj); inc_p_hit(obj)
 *                       obj->field1c = 0xfffd0000
 *                       obj->field20 = 0xfffd0000 - 0x70000
 *                       obj->field24 = that + 0xa8000 = 0x8000
 *                       obj->field28 = 4
 *                       token := 0x3c5, descend into t_flight
 *
 *      token == 0x3c5:  frame[frame].handler = t_land_on_my_back
 *
 *      otherwise:       return -3
 *
 * **The longest single state in the file, and it is ground_slammed_init written
 * out with extra work between the lines.** The first five calls are that
 * routine's body -- ground, animation 0x1e, find the part, advance a frame,
 * 0x1c = 2, group_sound -- and then a second sound through rsnd_func, a shake
 * with 0x00080008 in 0x48, half damage, an action, the damage, and the hit
 * count.
 *
 * **The damage accumulator is SET here, not added to.** t_air_slamed_by_robo2
 * and t_thrown_by_sg both read the proc's 0x54, add and store back; this one
 * stores 0x1a over whatever was there. Both spellings appear in the same file
 * against the same field, so the accumulation is per-routine and not a property
 * of 0x54.
 *
 * 0x1c starts at 0xfffd0000 -- negative -- and the chain 0xfffd0000 - 0x70000 +
 * 0xa8000 wraps to exactly 0x8000, which is the sixth flight in this file to
 * land 0x24 on that number by its own arithmetic.
 */
void shake_a11(MK3OBJ *obj);

long t_thrown_by_robo2(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        ground_player(obj);

        obj->field40 = 0x1e;
        find_ani_part2(obj);
        do_next_a9_frame(obj);

        obj->field1c = 2;
        group_sound(obj);
        rsnd_func(obj, 0xd);

        obj->field48 = 0x00080008;
        shake_a11(obj);
        set_half_damage(obj);

        obj->field1c = 0x62e;
        obj->field00->field18 = 0x62e;

        obj->a10 = 0x1a;
        obj->field00->field54 = 0x1a;
        damage_to_me(obj);
        inc_p_hit(obj);

        obj->field1c = 0xfffd0000u;
        obj->field20 = 0xfffd0000u - 0x70000u;
        obj->field24 = 0xfffd0000u - 0x70000u + 0xa8000u;
        obj->field28 = 4;

        *mk3_frame(thread, thread->frame + 1) = 0x3c5;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x3c5)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_land_on_my_back);
}


/* ------------------------------------------------------------ t_smoke_air_slam
 *
 * armv7 0x0004aa04, 260 bytes.  **Complete.**
 *
 *      token == 0:      obj->field40 = 6
 *                       obj->field48 = 6 + 0x2e = 0x34
 *                       air_slam_init(obj)
 *                       obj->field1c = (uint32_t)face_opponent
 *                       call_a0_for_him(obj)
 *                       obj->field1c = 4
 *                       token := 0xe9, descend into t_grab_animation
 *
 *      token == 0xe9:   token := 0xea, park 3
 *
 *      token == 0xea:   throw_voice(obj)
 *                       obj->field1c = 3
 *                       token := 0xee, descend into t_double_mframew
 *
 *      token == 0xee:   obj->field38 = t_thrown_by_lao
 *                       xfer_to_thrown(obj)
 *                       token := 0xf2, park 0xa
 *
 *      token == 0xf2:   frame[frame].handler = t_drop_down_land_jump
 *
 *      otherwise:       return -3
 *
 * **The second site for 0x1c carrying a function pointer, and it settles the
 * mechanism.** body_slam_init puts `clear_inviso` there; this one puts
 * `face_opponent`, off pointer slot 0x000f3758. Both then call call_a0_for_him,
 * so that routine takes the address out of 0x1c and runs it on the other
 * fighter -- two different routines through one field is what makes it a
 * mechanism rather than a coincidence.
 *
 * **0x40 and 0x48 are set before the call and 0x48 is the victim's animation**,
 * which air_slam_init resolves through get_his_char_ani. Here the compiler built
 * 0x48 as `0x40 + 0x2e` -- 6 then 0x34 -- and t_scorp_air_slam does the same with
 * 0xa and 0x38.
 *
 * **That relation does NOT hold across all four air slams.** t_robo2_air_slam
 * pairs 6 with 0x41 and t_kano_air_body_slam pairs 0 with 0x3d, neither of which
 * is a 0x2e step. So the chained arithmetic in these two is the compiler saving a
 * literal on a pair that happens to be 0x2e apart, not a rule about the tables.
 * Two of four is not a pattern, and this comment previously claimed it was.
 */
long t_drop_down_land_jump(MK3THREAD *thread);
void face_opponent(MK3OBJ *obj);                 /* pointer slot 0x000f3758 */

long t_smoke_air_slam(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field40 = 6;
        obj->field48 = 6 + 0x2e;
        air_slam_init(obj);

        obj->field1c = (uint32_t)(uintptr_t)face_opponent;
        call_a0_for_him(obj);

        obj->field1c = 4;
        *mk3_frame(thread, thread->frame + 1) = 0xe9;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_grab_animation;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0xe9) {
        *mk3_frame(thread, thread->frame + 1) = 0xea;
        thread->fieldfc = 3;
        return 3;
    }

    if (token == 0xea) {
        throw_voice(obj);
        obj->field1c = 3;
        *mk3_frame(thread, thread->frame + 1) = 0xee;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_double_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0xee) {
        obj->field38 = (uint32_t)(uintptr_t)t_thrown_by_lao;
        xfer_to_thrown(obj);
        *mk3_frame(thread, thread->frame + 1) = 0xf2;
        thread->fieldfc = 0xa;
        return 0xa;
    }

    if (token != 0xf2)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_drop_down_land_jump);
}

/* ------------------------------------------------------------ t_scorp_air_slam
 *
 * armv7 0x0004c780, 284 bytes.  **Complete.**
 *
 *      token == 0:      obj->field40 = 0xa
 *                       obj->field48 = 0xa + 0x2e = 0x38
 *                       air_slam_init(obj); throw_voice(obj)
 *                       match_him_with_me_f(obj); find_part2(obj)
 *                       obj->field1c = 0x4000
 *                       obj->field08->field20 = 0x4000
 *                       ((MK3OBJ *)obj->a10)->field20 = obj->field1c
 *                       obj->field1c = 3
 *                       token := 0x16c, descend into t_double_mframew
 *
 *      token == 0x16c:  obj->field38 = t_thrown_by_nj
 *                       xfer_to_thrown(obj)
 *                       obj->field1c = 0
 *                       obj->field20 = 0x18000
 *                       obj->field24 = 0x18000 - 0x10000 = 0x8000
 *                       obj->field28 = 0xfff
 *                       token := 0x175, descend into t_flight
 *
 *      token == 0x175:  obj->field40 = 0x00030021
 *                       frame[frame].handler = t_animate_a9
 *
 *      otherwise:       return -3
 *
 * **The flight is t_drop_down_land's, to the word** -- 0, 0x18000, 0x8000, 0xfff
 * -- so a scorpion air slam falls exactly as a drop-down does and only the
 * handler afterwards differs.
 *
 * **Both fighters get the same horizontal push.** 0x4000 goes into the part's
 * 0x20 and into the 0x20 of the object named by 0x44 -- the one air_slam_init put
 * there from the proc's `him` -- so slammer and victim move together, which is
 * what match_him_with_me_f a line earlier is for.
 *
 * The final 0x40 is 0x00030021, a fourth packed word for t_animate_a9 after
 * 0x0005000d, 0x00040021 and 0x00040047. **Two of the four now share the low
 * half 0x21 with different high halves**, 3 here against 4 in t_dizzy_dude,
 * which is the first direct evidence that the halves vary independently.
 */
void find_part2(MK3OBJ *obj);
long t_thrown_by_nj(MK3THREAD *thread);
long t_animate_a9(MK3THREAD *thread);            /* pointer slot 0x000f36d0 */

long t_scorp_air_slam(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field40 = 0xa;
        obj->field48 = 0xa + 0x2e;

        air_slam_init(obj);
        throw_voice(obj);
        match_him_with_me_f(obj);
        find_part2(obj);

        obj->field1c = 0x4000;
        obj->field08->field20 = 0x4000;
        ((MK3OBJ *)(void *)(uintptr_t)obj->a10)->field20 = obj->field1c;

        obj->field1c = 3;
        *mk3_frame(thread, thread->frame + 1) = 0x16c;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_double_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x16c) {
        obj->field38 = (uint32_t)(uintptr_t)t_thrown_by_nj;
        xfer_to_thrown(obj);

        obj->field1c = 0;
        obj->field20 = 0x18000;
        obj->field24 = 0x18000 - 0x10000;
        obj->field28 = 0xfff;

        *mk3_frame(thread, thread->frame + 1) = 0x175;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x175)
        return -3;

    obj->field40 = 0x00030021;
    return mk3_install(thread, (MK3THREADFUNC)t_animate_a9);
}


/* -------------------------------------------------------- t_kano_air_body_slam
 *
 * armv7 0x0004c57c, 280 bytes.  **Complete.**
 *
 *      token == 0:      obj->field40 = 0
 *                       obj->field48 = 0x3d
 *                       air_slam_init(obj)
 *                       match_him_with_me_f(obj); double_next_a9(obj)
 *                       token := 0x186, park 8
 *
 *      token == 0x186:  obj->field1c = 2
 *                       token := 0x189, descend into t_double_mframew
 *
 *      token == 0x189:  obj->field38 = t_air_slamed_by_kano
 *                       xfer_to_thrown(obj)
 *                       token := 0x18e, park 0xa
 *
 *      token == 0x18e:  obj->field1c = 0
 *                       obj->field20 = 0x18000
 *                       obj->field24 = 0x18000 - 0x10000 = 0x8000
 *                       obj->field28 = 0xfff
 *                       token := 0x197, descend into t_flight
 *
 *      token == 0x197:  frame[frame].handler = t_jump_up_land_jsrp
 *
 *      otherwise:       return -3
 *
 * **t_robo2_air_slam's five states with two things changed**: the victim goes to
 * t_air_slamed_by_kano rather than t_air_slamed_by_robo2, and the ending is a
 * flight into t_jump_up_land_jsrp instead of a bare t_drop_down_land. So the
 * air-slam template is four states of setup and handover plus one ending, and
 * the ending is where the characters differ.
 *
 * **0x40 is zero here**, which is the value air_slam_init saves and puts back
 * untouched -- so this fighter resolves no animation of its own and only the
 * victim's 0x48 matters. That is what disproves the 0x2e relation noted above:
 * 0 and 0x3d are not a 0x2e step.
 *
 * The flight is t_drop_down_land's four numbers again, which is now the third
 * routine to use exactly those.
 */
long t_air_slamed_by_kano(MK3THREAD *thread);

long t_kano_air_body_slam(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field40 = 0;
        obj->field48 = 0x3d;

        air_slam_init(obj);
        match_him_with_me_f(obj);
        double_next_a9(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x186;
        thread->fieldfc = 8;
        return 8;
    }

    if (token == 0x186) {
        obj->field1c = 2;
        *mk3_frame(thread, thread->frame + 1) = 0x189;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_double_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x189) {
        obj->field38 = (uint32_t)(uintptr_t)t_air_slamed_by_kano;
        xfer_to_thrown(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x18e;
        thread->fieldfc = 0xa;
        return 0xa;
    }

    if (token == 0x18e) {
        obj->field1c = 0;
        obj->field20 = 0x18000;
        obj->field24 = 0x18000 - 0x10000;
        obj->field28 = 0xfff;

        *mk3_frame(thread, thread->frame + 1) = 0x197;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x197)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_jump_up_land_jsrp);
}

/* ---------------------------------------------------------------- t_lia_slam
 *
 * armv7 0x0004afa0, 280 bytes.  **Complete.**
 *
 *      token == 0:      obj->field1c = 0; ochar_sound(obj)
 *                       body_slam_init(obj)
 *                       obj->field1c = 1
 *                       token := 0x30f, descend into t_grab_animation
 *
 *      token == 0x30f:  token := 0x310, park 3
 *
 *      token == 0x310:  obj->field1c = 4
 *                       token := 0x313, descend into t_double_mframew
 *
 *      token == 0x313:  obj->field38 = t_thrown_by_lao
 *                       xfer_to_thrown(obj)
 *                       token := 0x316, park 8
 *
 *      token == 0x316:  obj->field1c = 4
 *                       frame[frame].handler = t_mframew
 *
 *      otherwise:       return -3
 *
 * **t_tusk_slam's five states with a sound at the front and a different last
 * wait.** The sound is number ZERO -- 0x1c is still the token, which is zero on
 * entry, and that is what ochar_sound is handed. Whether zero means the first
 * sound or no sound at all is not settled here; the store is written as it
 * stands.
 *
 * The durations are 1, 3, 4, 8, 4 against t_tusk_slam's 3, 3, 0xa, 8. Same
 * skeleton, different numbers -- which is the whole of what a character's ground
 * slam is once body_slam_init has done the table work.
 */
long t_lia_slam(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field1c = 0;               /* the token, still zero */
        ochar_sound(obj);
        body_slam_init(obj);
        obj->field1c = 1;

        *mk3_frame(thread, thread->frame + 1) = 0x30f;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_grab_animation;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x30f) {
        *mk3_frame(thread, thread->frame + 1) = 0x310;
        thread->fieldfc = 3;
        return 3;
    }

    if (token == 0x310) {
        obj->field1c = 4;
        *mk3_frame(thread, thread->frame + 1) = 0x313;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_double_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x313) {
        obj->field38 = (uint32_t)(uintptr_t)t_thrown_by_lao;
        xfer_to_thrown(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x316;
        thread->fieldfc = 8;
        return 8;
    }

    if (token != 0x316)
        return -3;

    obj->field1c = 4;
    return mk3_install(thread, (MK3THREADFUNC)t_mframew);
}


/* -------------------------------------------------------------- t_robo1_slam
 *
 * armv7 0x0004ae7c, 292 bytes.  **Complete.**
 *
 *      token == 0:      body_slam_init(obj)
 *                       obj->field1c = 0; ochar_sound(obj)
 *                       obj->field1c = 1
 *                       token := 0x374, descend into t_grab_animation
 *
 *      token == 0x374:  obj->field1c = 2
 *                       token := 0x377, descend into t_mframew
 *
 *      token == 0x377:  obj->field1c = 1; ochar_sound(obj)
 *                       obj->field1c = 3
 *                       token := 0x37c, descend into t_double_mframew
 *
 *      token == 0x37c:  obj->field38 = t_thrown_by_lao
 *                       xfer_to_thrown(obj)
 *                       obj->field1c = 2; ochar_sound(obj)
 *                       obj->field1c = 3
 *                       frame[frame].handler = t_mframew
 *
 *      otherwise:       return -3
 *
 * **Three sounds, numbered 0, 1 and 2, one per stage.** No other slam in the
 * file makes more than one, and the numbers run in order -- so ochar_sound's
 * argument in 0x1c is an index into a per-character set and this routine walks
 * the first three of it. The first is the zero the token happens to hold on
 * entry, as in t_lia_slam.
 *
 * Each sound number is overwritten by a duration on the next line, which is the
 * 0x1c-shared-by-everything pattern again: 0 then 1, 1 then 3, 2 then 3.
 */
long t_robo1_slam(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        body_slam_init(obj);
        obj->field1c = 0;               /* the token, still zero */
        ochar_sound(obj);
        obj->field1c = 1;

        *mk3_frame(thread, thread->frame + 1) = 0x374;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_grab_animation;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x374) {
        obj->field1c = 2;
        *mk3_frame(thread, thread->frame + 1) = 0x377;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x377) {
        obj->field1c = 1;
        ochar_sound(obj);
        obj->field1c = 3;

        *mk3_frame(thread, thread->frame + 1) = 0x37c;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_double_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x37c)
        return -3;

    obj->field38 = (uint32_t)(uintptr_t)t_thrown_by_lao;
    xfer_to_thrown(obj);

    obj->field1c = 2;
    ochar_sound(obj);
    obj->field1c = 3;

    return mk3_install(thread, (MK3THREADFUNC)t_mframew);
}

/* ------------------------------------------------------------- t_kitana_slam
 *
 * armv7 0x0004b300, 304 bytes.  **Complete.**
 *
 *      token == 0:      body_slam_init(obj)
 *                       obj->field1c = 3
 *                       token := 0xa1, descend into t_grab_animation
 *
 *      token == 0xa1:   token := 0xa2, park 2
 *
 *      token == 0xa2:   throw_voice(obj)
 *                       obj->field1c = 3
 *                       token := 0xa6, descend into t_double_mframew
 *
 *      token == 0xa6:   obj->field38 = t_thrown_by_lao
 *                       xfer_to_thrown(obj)
 *                       do_next_a9_frame(obj)
 *                       token := 0xab, park 8
 *
 *      token == 0xab:   do_next_a9_frame(obj)
 *                       token := 0xad, park 8
 *
 *      token == 0xad:   pop a level, or t_local_reaction_exit at the bottom
 *
 *      otherwise:       return -3
 *
 * **t_tusk_slam's six states with the last wait replaced by two hand-advanced
 * frames.** Where the others hand the animation to t_mframew and let it run,
 * this one calls do_next_a9_frame directly, twice, with an eight-frame park after
 * each. So the two frames after the throw are stepped rather than played, which
 * is a different mechanism for the same-looking pause.
 *
 * The dispatch reads 0xa2 first and then bisects, so the five live tokens are
 * tested in the order 0xa2, 0, 0xa1, 0xab, 0xad, 0xa6 -- worth noting only
 * because a token's position in the chain says nothing about its position in the
 * sequence.
 */
long t_kitana_slam(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        body_slam_init(obj);
        obj->field1c = 3;

        *mk3_frame(thread, thread->frame + 1) = 0xa1;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_grab_animation;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0xa1) {
        *mk3_frame(thread, thread->frame + 1) = 0xa2;
        thread->fieldfc = 2;
        return 2;
    }

    if (token == 0xa2) {
        throw_voice(obj);
        obj->field1c = 3;

        *mk3_frame(thread, thread->frame + 1) = 0xa6;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_double_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0xa6) {
        obj->field38 = (uint32_t)(uintptr_t)t_thrown_by_lao;
        xfer_to_thrown(obj);
        do_next_a9_frame(obj);
        *mk3_frame(thread, thread->frame + 1) = 0xab;
        thread->fieldfc = 8;
        return 8;
    }

    if (token == 0xab) {
        do_next_a9_frame(obj);
        *mk3_frame(thread, thread->frame + 1) = 0xad;
        thread->fieldfc = 8;
        return 8;
    }

    if (token != 0xad)
        return -3;

    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
        return 0;
    }

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* --------------------------------------------------------------- t_jade_slam
 *
 * armv7 0x0004ad48, 308 bytes.  **Complete.**
 *
 *      token == 0:      body_slam_init(obj)
 *                       obj->field1c = 3
 *                       token := 0x80, descend into t_grab_animation
 *
 *      token == 0x80:   token := 0x81, park 3
 *
 *      token == 0x81:   throw_voice(obj)
 *                       obj->field1c = 2
 *                       token := 0x84, descend into t_double_mframew
 *
 *      token == 0x84:   obj->field38 = t_thrown_by_lao
 *                       xfer_to_thrown(obj)
 *                       token := 0x87, park 0xa
 *
 *      token == 0x87:   obj->field1c = 2
 *                       token := 0x89, descend into t_mframew
 *
 *      token == 0x89:   pop a level, or t_local_reaction_exit at the bottom
 *
 *      otherwise:       return -3
 *
 * **The six-state ground slam in its plainest form.** t_mileena_slam below is
 * the identical routine -- same six states, same four durations 3, 3, 2, 0xa, 2,
 * same calls in the same order -- differing in the five token values and in which
 * handler the victim is given. So at this point the file has the template three
 * ways: split across t_indian_slam and t_slam_ani2, fused in t_tusk_slam, and
 * fused again here with an extra wait.
 *
 * The victim handler is the one thing a pair like this really carries, and jade's
 * is t_thrown_by_lao where mileena's is t_thrown_by_sonya.
 */
long t_jade_slam(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        body_slam_init(obj);
        obj->field1c = 3;

        *mk3_frame(thread, thread->frame + 1) = 0x80;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_grab_animation;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x80) {
        *mk3_frame(thread, thread->frame + 1) = 0x81;
        thread->fieldfc = 3;
        return 3;
    }

    if (token == 0x81) {
        throw_voice(obj);
        obj->field1c = 2;

        *mk3_frame(thread, thread->frame + 1) = 0x84;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_double_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x84) {
        obj->field38 = (uint32_t)(uintptr_t)t_thrown_by_lao;
        xfer_to_thrown(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x87;
        thread->fieldfc = 0xa;
        return 0xa;
    }

    if (token == 0x87) {
        obj->field1c = 2;
        *mk3_frame(thread, thread->frame + 1) = 0x89;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x89)
        return -3;

    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
        return 0;
    }

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

/* ------------------------------------------------------------ t_mileena_slam
 *
 * armv7 0x0004ac14, 308 bytes.  **Complete.**
 *
 * t_jade_slam with 0x91, 0x92, 0x95, 0x98 and 0x9a for the tokens and
 * **t_thrown_by_sonya for the victim** instead of t_thrown_by_lao. Everything
 * else -- six states, four durations, four calls, the pop and its
 * t_local_reaction_exit fallback -- is the same instruction for instruction.
 */
long t_thrown_by_sonya(MK3THREAD *thread);

long t_mileena_slam(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        body_slam_init(obj);
        obj->field1c = 3;

        *mk3_frame(thread, thread->frame + 1) = 0x91;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_grab_animation;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x91) {
        *mk3_frame(thread, thread->frame + 1) = 0x92;
        thread->fieldfc = 3;
        return 3;
    }

    if (token == 0x92) {
        throw_voice(obj);
        obj->field1c = 2;

        *mk3_frame(thread, thread->frame + 1) = 0x95;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_double_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x95) {
        obj->field38 = (uint32_t)(uintptr_t)t_thrown_by_sonya;
        xfer_to_thrown(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x98;
        thread->fieldfc = 0xa;
        return 0xa;
    }

    if (token == 0x98) {
        obj->field1c = 2;
        *mk3_frame(thread, thread->frame + 1) = 0x9a;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x9a)
        return -3;

    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
        return 0;
    }

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* ------------------------------------------------------------------- t_njsl3
 *
 * armv7 0x0004a774, 320 bytes.  **Complete.**
 *
 *      token == 0:      obj->field1c = 3
 *                       token := 0x1ce, descend into t_grab_animation
 *
 *      token == 0x1ce:  token := 0x1cf, park 3
 *
 *      token == 0x1cf:  throw_voice(obj)
 *                       obj->field1c = 3
 *                       token := 0x1d3, descend into t_double_mframew
 *
 *      token == 0x1d3:  obj->field38 = t_thrown_by_nj
 *                       xfer_to_thrown(obj)
 *                       token := 0x1d7, park 8
 *
 *      token == 0x1d7:  obj->field1c = 3
 *                       token := 0x1d9, descend into t_mframew
 *
 *      token == 0x1d9:  pop a level, or t_local_reaction_exit at the bottom
 *
 *      otherwise:       return -3
 *
 * **The six-state template with its first call missing, and the reason is
 * t_noob_slam.** Every other routine of this shape opens with body_slam_init;
 * this one does not, because t_noob_slam has already called it -- with the part's
 * character number temporarily set to 0x12 -- and then installs this. So the two
 * together are one slam split so that the table work happens under a borrowed
 * identity and the rest does not.
 *
 * That also settles which half of t_noob_slam matters: the swap has to cover
 * body_slam_init only, and everything after it runs as Noob.
 */
long t_njsl3(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field1c = 3;

        *mk3_frame(thread, thread->frame + 1) = 0x1ce;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_grab_animation;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x1ce) {
        *mk3_frame(thread, thread->frame + 1) = 0x1cf;
        thread->fieldfc = 3;
        return 3;
    }

    if (token == 0x1cf) {
        throw_voice(obj);
        obj->field1c = 3;

        *mk3_frame(thread, thread->frame + 1) = 0x1d3;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_double_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x1d3) {
        obj->field38 = (uint32_t)(uintptr_t)t_thrown_by_nj;
        xfer_to_thrown(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x1d7;
        thread->fieldfc = 8;
        return 8;
    }

    if (token == 0x1d7) {
        obj->field1c = 3;
        *mk3_frame(thread, thread->frame + 1) = 0x1d9;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x1d9)
        return -3;

    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
        return 0;
    }

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

/* -------------------------------------------------------------- t_smoke_slam
 *
 * armv7 0x0004c89c, 328 bytes.  **Complete.**
 *
 *      token == 0:      obj->field40 = 6
 *                       body_slam_init(obj)
 *                       obj->field1c = 4
 *                       token := 0xfd, descend into t_grab_animation
 *
 *      token == 0xfd:   token := 0xfe, park 3
 *
 *      token == 0xfe:   throw_voice(obj)
 *                       obj->field1c = 3
 *                       token := 0x102, descend into t_double_mframew
 *
 *      token == 0x102:  obj->field38 = t_thrown_by_lao
 *                       xfer_to_thrown(obj)
 *                       token := 0x106, park 0xa
 *
 *      token == 0x106:  face_opponent(obj)
 *                       obj->field40 = 0
 *                       pose_a9_manual(obj); ground_ochar(obj)
 *                       token := 0x10c, park 2
 *
 *      token == 0x10c:  pop a level, or t_local_reaction_exit at the bottom
 *
 *      otherwise:       return -3
 *
 * **The extra state is the smoke re-appearing.** Where jade and mileena spend
 * their fifth state on one more wait, this one turns to face the opponent, poses
 * animation zero by hand through pose_a9_manual, and grounds the character. So
 * the slammer is repositioned rather than simply waiting, which is what a
 * teleporting ninja's slam has to do.
 *
 * **The 6 written into 0x40 before body_slam_init is dead.** That routine's
 * first act on 0x40 is a store, from ochar_slammed_anis, so nothing reads the 6.
 * Transcribed as written -- t_kano_air_body_slam's 0x40 = 0 is read by
 * air_slam_init and this one is not, so the same-looking store means different
 * things either side of which init is called.
 */
void pose_a9_manual(MK3OBJ *obj);
void ground_ochar(MK3OBJ *obj);

long t_smoke_slam(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field40 = 6;               /* dead: body_slam_init stores over it */
        body_slam_init(obj);
        obj->field1c = 4;

        *mk3_frame(thread, thread->frame + 1) = 0xfd;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_grab_animation;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0xfd) {
        *mk3_frame(thread, thread->frame + 1) = 0xfe;
        thread->fieldfc = 3;
        return 3;
    }

    if (token == 0xfe) {
        throw_voice(obj);
        obj->field1c = 3;

        *mk3_frame(thread, thread->frame + 1) = 0x102;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_double_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x102) {
        obj->field38 = (uint32_t)(uintptr_t)t_thrown_by_lao;
        xfer_to_thrown(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x106;
        thread->fieldfc = 0xa;
        return 0xa;
    }

    if (token == 0x106) {
        face_opponent(obj);
        obj->field40 = 0;
        pose_a9_manual(obj);
        ground_ochar(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x10c;
        thread->fieldfc = 2;
        return 2;
    }

    if (token != 0x10c)
        return -3;

    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
        return 0;
    }

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* -------------------------------------------------------------- t_sonya_slam
 *
 * armv7 0x0004a62c, 328 bytes.  **Complete.**
 *
 *      token == 0:      body_slam_init(obj)
 *                       obj->field1c = 3
 *                       token := 0x1ee, descend into t_grab_animation
 *
 *      token == 0x1ee:  token := 0x1ef, park 3
 *
 *      token == 0x1ef:  throw_voice(obj)
 *                       obj->field1c = 4
 *                       token := 0x1f4, descend into t_double_mframew
 *
 *      token == 0x1f4:  obj->field38 = t_thrown_by_sonya
 *                       xfer_to_thrown(obj)
 *                       token := 0x1f8, park 0x10
 *
 *      token == 0x1f8:  obj->field1c = 8
 *                       token := 0x1fb, descend into t_mframew
 *
 *      token == 0x1fb:  pop a level, or t_local_reaction_exit at the bottom
 *
 *      otherwise:       return -3
 *
 * The template again, with 3, 3, 4, 0x10, 8 for the durations -- the longest
 * post-throw park in the file at sixteen frames -- and the victim sent to the
 * throw named after the same character. That pairing holds for sonya and kano
 * and not for jade, mileena or the two ninjas, so a character's slam and a
 * character's throw are named together only when both exist.
 */
long t_sonya_slam(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        body_slam_init(obj);
        obj->field1c = 3;

        *mk3_frame(thread, thread->frame + 1) = 0x1ee;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_grab_animation;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x1ee) {
        *mk3_frame(thread, thread->frame + 1) = 0x1ef;
        thread->fieldfc = 3;
        return 3;
    }

    if (token == 0x1ef) {
        throw_voice(obj);
        obj->field1c = 4;

        *mk3_frame(thread, thread->frame + 1) = 0x1f4;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_double_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x1f4) {
        obj->field38 = (uint32_t)(uintptr_t)t_thrown_by_sonya;
        xfer_to_thrown(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x1f8;
        thread->fieldfc = 0x10;
        return 0x10;
    }

    if (token == 0x1f8) {
        obj->field1c = 8;
        *mk3_frame(thread, thread->frame + 1) = 0x1fb;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x1fb)
        return -3;

    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
        return 0;
    }

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

/* --------------------------------------------------------------- t_kano_slam
 *
 * armv7 0x0004b1b0, 336 bytes.  **Complete.**
 *
 *      token == 0:      body_slam_init(obj)
 *                       obj->field1c = 3
 *                       token := 0x123, descend into t_grab_animation
 *
 *      token == 0x123:  token := 0x124, park 3
 *
 *      token == 0x124:  throw_voice(obj)
 *                       obj->field1c = 4
 *                       token := 0x128, descend into t_double_mframew
 *
 *      token == 0x128:  obj->field38 = t_thrown_by_kano
 *                       xfer_to_thrown(obj)
 *                       do_next_a9_frame(obj)
 *                       token := 0x12e, park 0x10
 *
 *      token == 0x12e:  obj->field1c = 8
 *                       token := 0x131, descend into t_mframew
 *
 *      token == 0x131:  pop a level, or t_local_reaction_exit at the bottom
 *
 *      otherwise:       return -3
 *
 * t_sonya_slam with the same five durations and one added call: a single
 * do_next_a9_frame after the handover, the same nudge t_kitana_slam makes twice.
 * The victim goes to t_thrown_by_kano, which is the throw that inlines
 * t_slam_damage rather than installing it.
 */
long t_thrown_by_kano(MK3THREAD *thread);

long t_kano_slam(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        body_slam_init(obj);
        obj->field1c = 3;

        *mk3_frame(thread, thread->frame + 1) = 0x123;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_grab_animation;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x123) {
        *mk3_frame(thread, thread->frame + 1) = 0x124;
        thread->fieldfc = 3;
        return 3;
    }

    if (token == 0x124) {
        throw_voice(obj);
        obj->field1c = 4;

        *mk3_frame(thread, thread->frame + 1) = 0x128;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_double_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x128) {
        obj->field38 = (uint32_t)(uintptr_t)t_thrown_by_kano;
        xfer_to_thrown(obj);
        do_next_a9_frame(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x12e;
        thread->fieldfc = 0x10;
        return 0x10;
    }

    if (token == 0x12e) {
        obj->field1c = 8;
        *mk3_frame(thread, thread->frame + 1) = 0x131;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x131)
        return -3;

    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
        return 0;
    }

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* ----------------------------------------------------------------- t_lk_slam
 *
 * armv7 0x0004a8b4, 336 bytes.  **Complete.**
 *
 *      token == 0:      body_slam_init(obj)
 *                       obj->field1c = 3
 *                       token := 0x1ba, descend into t_grab_animation
 *
 *      token == 0x1ba:  token := 0x1bb, park 3
 *
 *      token == 0x1bb:  throw_voice(obj)
 *                       obj->field1c = 3
 *                       token := 0x1bf, descend into t_double_mframew
 *
 *      token == 0x1bf:  obj->field38 = t_thrown_by_sonya
 *                       xfer_to_thrown(obj)
 *                       token := 0x1c3, park 0xa
 *
 *      token == 0x1c3:  obj->field1c = 3
 *                       token := 0x1c5, descend into t_mframew
 *
 *      token == 0x1c5:  pop a level, or t_local_reaction_exit at the bottom
 *
 *      otherwise:       return -3
 *
 * The template with 3, 3, 3, 0xa, 3 -- the most uniform set of durations in the
 * file -- and the victim handed to t_thrown_by_sonya, which t_mileena_slam and
 * t_sonya_slam also use. Three of the eleven ground slams share that one throw,
 * so the victim handler is drawn from a small set rather than written per
 * character.
 */
long t_lk_slam(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        body_slam_init(obj);
        obj->field1c = 3;

        *mk3_frame(thread, thread->frame + 1) = 0x1ba;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_grab_animation;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x1ba) {
        *mk3_frame(thread, thread->frame + 1) = 0x1bb;
        thread->fieldfc = 3;
        return 3;
    }

    if (token == 0x1bb) {
        throw_voice(obj);
        obj->field1c = 3;

        *mk3_frame(thread, thread->frame + 1) = 0x1bf;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_double_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x1bf) {
        obj->field38 = (uint32_t)(uintptr_t)t_thrown_by_sonya;
        xfer_to_thrown(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x1c3;
        thread->fieldfc = 0xa;
        return 0xa;
    }

    if (token == 0x1c3) {
        obj->field1c = 3;
        *mk3_frame(thread, thread->frame + 1) = 0x1c5;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x1c5)
        return -3;

    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
        return 0;
    }

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

/* ----------------------------------------------------------------- t_sw_slam
 *
 * armv7 0x0004a400, 336 bytes.  **Complete.**
 *
 *      token == 0:      body_slam_init(obj)
 *                       obj->field1c = 4
 *                       token := 0x2f9, descend into t_grab_animation
 *
 *      token == 0x2f9:  token := 0x2fa, park 2
 *
 *      token == 0x2fa:  throw_voice(obj)
 *                       obj->field1c = 4
 *                       token := 0x2fe, descend into t_double_mframew
 *
 *      token == 0x2fe:  obj->field38 = t_thrown_by_lao
 *                       xfer_to_thrown(obj)
 *                       token := 0x302, park 0x10
 *
 *      token == 0x302:  obj->field1c = 8
 *                       token := 0x304, descend into t_mframew
 *
 *      token == 0x304:  pop a level, or t_local_reaction_exit at the bottom
 *
 *      otherwise:       return -3
 *
 * The template with 4, 2, 4, 0x10, 8. **Eleven ground slams are now written and
 * the six-state shape accounts for every one of them**; what a character carries
 * is five durations, five token values and one victim handler. Nothing else
 * varies, which is why body_slam_init is where all the per-character work lives.
 */
long t_sw_slam(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        body_slam_init(obj);
        obj->field1c = 4;

        *mk3_frame(thread, thread->frame + 1) = 0x2f9;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_grab_animation;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x2f9) {
        *mk3_frame(thread, thread->frame + 1) = 0x2fa;
        thread->fieldfc = 2;
        return 2;
    }

    if (token == 0x2fa) {
        throw_voice(obj);
        obj->field1c = 4;

        *mk3_frame(thread, thread->frame + 1) = 0x2fe;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_double_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x2fe) {
        obj->field38 = (uint32_t)(uintptr_t)t_thrown_by_lao;
        xfer_to_thrown(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x302;
        thread->fieldfc = 0x10;
        return 0x10;
    }

    if (token == 0x302) {
        obj->field1c = 8;
        *mk3_frame(thread, thread->frame + 1) = 0x304;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x304)
        return -3;

    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
        return 0;
    }

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* ----------------------------------------------------------------- t_sz_slam
 *
 * armv7 0x0004bec0, 368 bytes.  **Complete.**
 *
 *      token == 0:      body_slam_init(obj)
 *                       obj->field1c = 3
 *                       token := 0x2e4, descend into t_grab_animation
 *
 *      token == 0x2e4:  token := 0x2e5, park 3
 *
 *      token == 0x2e5:  double_next_a9(obj)
 *                       token := 0x2e7, park 8
 *
 *      token == 0x2e7:  me_in_back(obj); throw_voice(obj)
 *                       obj->field1c = 4
 *                       token := 0x2ec, descend into t_double_mframew
 *
 *      token == 0x2ec:  obj->field38 = t_thrown_by_sz
 *                       xfer_to_thrown(obj)
 *                       token := 0x2ef, park 0xc
 *
 *      token == 0x2ef:  do_next_a9_frame(obj)
 *                       token := 0x2f2, park 5
 *
 *      token == 0x2f2:  pop a level, or t_local_reaction_exit at the bottom
 *
 *      otherwise:       return -3
 *
 * **Seven states, the longest ground slam in the file**, and the two extra ones
 * are an animation nudge before the throw (double_next_a9, eight frames) and
 * another after it (do_next_a9_frame, five). me_in_back at the throw is the only
 * call to it in this file -- the slammer goes behind the victim before letting
 * go, which is what a Sub-Zero slam looks like.
 *
 * **r8 holds two different tokens in one routine and the dispatch decides
 * which.** It is loaded with 0x2e7 at entry and reloaded with 0x2ef only on the
 * branch taken when the token is above 0x2e7. The 0x2e5 state is reached on the
 * low side and writes 0x2e7; the 0x2ec state is reached on the high side and
 * writes 0x2ef. Two `str.w r8` instructions, two different values, decided
 * several branches earlier -- worth reading twice before transcribing.
 */
void me_in_back(MK3OBJ *obj);

long t_sz_slam(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        body_slam_init(obj);
        obj->field1c = 3;

        *mk3_frame(thread, thread->frame + 1) = 0x2e4;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_grab_animation;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x2e4) {
        *mk3_frame(thread, thread->frame + 1) = 0x2e5;
        thread->fieldfc = 3;
        return 3;
    }

    if (token == 0x2e5) {
        double_next_a9(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x2e7;
        thread->fieldfc = 8;
        return 8;
    }

    if (token == 0x2e7) {
        me_in_back(obj);
        throw_voice(obj);
        obj->field1c = 4;

        *mk3_frame(thread, thread->frame + 1) = 0x2ec;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_double_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x2ec) {
        obj->field38 = (uint32_t)(uintptr_t)t_thrown_by_sz;
        xfer_to_thrown(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x2ef;
        thread->fieldfc = 0xc;
        return 0xc;
    }

    if (token == 0x2ef) {
        do_next_a9_frame(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x2f2;
        thread->fieldfc = 5;
        return 5;
    }

    if (token != 0x2f2)
        return -3;

    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
        return 0;
    }

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* ----------------------------------------------------------------- t_sg_slam
 *
 * armv7 0x0004c030, 396 bytes.  **Complete.**
 *
 *      token == 0:      body_slam_init(obj)
 *                       obj->field1c = 3
 *                       token := 0x2ab, descend into t_grab_animation
 *
 *      token == 0x2ab:  token := 0x2ac, park 3
 *
 *      token == 0x2ac:  throw_voice(obj); me_in_back(obj)
 *                       double_next_a9(obj)
 *                       token := 0x2b2, park 4
 *
 *      token == 0x2b2:  double_next_a9(obj)
 *                       token := 0x2b4, park 4
 *
 *      token == 0x2b4:  double_next_a9(obj)
 *                       token := 0x2b6, park 0xa
 *
 *      token == 0x2b6:  obj->field1c = 3
 *                       token := 0x2b9, descend into t_double_mframew
 *
 *      token == 0x2b9:  obj->field38 = t_thrown_by_sg
 *                       xfer_to_thrown(obj)
 *                       do_next_a9_frame(obj)
 *                       token := 0x2bd, park 0xa
 *
 *      token == 0x2bd:  obj->field1c = 6
 *                       frame[frame].handler = t_mframew
 *
 *      otherwise:       return -3
 *
 * **Eight states, the longest slam in the file, and three of them do nothing but
 * double_next_a9 with a park.** 0x2ac, 0x2b2 and 0x2b4 step the animation on with
 * waits of 4, 4 and 0xa between them, so the wind-up is animated by hand three
 * times before the throw rather than handed to t_mframew.
 *
 * **r8 carries two tokens again, and this routine reassigns it mid-dispatch.**
 * It holds 0x2b2 at entry and becomes 0x2b6 only on the branch taken when the
 * token is above 0x2b2. The 0x2ac state is reached below that point and writes
 * 0x2b2; the 0x2b4 state is reached above it and writes 0x2b6. t_sz_slam does the
 * same thing with one reassignment; this one is the second site, so the idiom is
 * the compiler's and not an accident.
 *
 * The routine ends by installing t_mframew rather than popping, so the wait after
 * the throw belongs to whatever level is underneath. Every other ground slam pops
 * itself; this one and t_robo1_slam do not.
 */
long t_thrown_by_sg(MK3THREAD *thread);

long t_sg_slam(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        body_slam_init(obj);
        obj->field1c = 3;

        *mk3_frame(thread, thread->frame + 1) = 0x2ab;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_grab_animation;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x2ab) {
        *mk3_frame(thread, thread->frame + 1) = 0x2ac;
        thread->fieldfc = 3;
        return 3;
    }

    if (token == 0x2ac) {
        throw_voice(obj);
        me_in_back(obj);
        double_next_a9(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x2b2;
        thread->fieldfc = 4;
        return 4;
    }

    if (token == 0x2b2) {
        double_next_a9(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x2b4;
        thread->fieldfc = 4;
        return 4;
    }

    if (token == 0x2b4) {
        double_next_a9(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x2b6;
        thread->fieldfc = 0xa;
        return 0xa;
    }

    if (token == 0x2b6) {
        obj->field1c = 3;
        *mk3_frame(thread, thread->frame + 1) = 0x2b9;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_double_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x2b9) {
        obj->field38 = (uint32_t)(uintptr_t)t_thrown_by_sg;
        xfer_to_thrown(obj);
        do_next_a9_frame(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x2bd;
        thread->fieldfc = 0xa;
        return 0xa;
    }

    if (token != 0x2bd)
        return -3;

    obj->field1c = 6;
    return mk3_install(thread, (MK3THREADFUNC)t_mframew);
}


/* -------------------------------------------------------------- t_robo2_slam
 *
 * armv7 0x0004bcf4, 460 bytes.  **Complete.**
 *
 *      token == 0:      body_slam_init(obj)
 *                       obj->field1c = 3
 *                       token := 0x323, descend into t_grab_animation
 *
 *      token == 0x323:  token := 0x324, park 4
 *
 *      token == 0x324:  obj->field1c = 7; ochar_sound(obj)
 *                       obj->field1c = 4; init_anirate(obj)
 *                       obj->field1c = 0x20000; away_x_vel(obj)
 *                       obj->field08->field1c = 0xffe80000
 *                       obj->field1c = obj->field08->field20 = 0x1a000
 *                       stick_him_with_me(obj)
 *                       token := 0x337, park 1
 *
 *      token == 0x337:  sans_repell_3(obj); stick_him_with_me(obj)
 *                       obj->field1c = --obj->field00->field20
 *                       if (that == 0) {
 *                           obj->field00->field20 = obj->field00->field1c
 *                           obj->field1c = *(uint32_t *)obj->field40
 *                           if (that != 0) double_next_a9(obj)
 *                       }
 *                       obj->field20 = (int16)part[0x12]
 *                       obj->field1c = *(uint32_t *)(proc + 0x40) + 0x10
 *                       if (obj->field1c > obj->field20)
 *                           token := 0x337, park 1
 *                       else {
 *                           sans_repell(obj)
 *                           obj->field48 = 0x00080008; shake_a11(obj)
 *                           rsnd_func(obj, 0xd)
 *                           obj->field1c = 2; his_group_sound(obj)
 *                           stop_me_player(obj); stop_him(obj)
 *                           obj->field38 = t_thrown_by_robo2
 *                           xfer_to_thrown(obj)
 *                           token := 0x359, park 0xe
 *                       }
 *
 *      token == 0x359:  face_opponent(obj); ground_player(obj)
 *                       token := 0x35c, descend into t_do_backup
 *
 *      token == 0x35c:  frame[frame].handler = t_local_reaction_exit
 *
 *      otherwise:       return -3
 *
 * **The only slam in the file that carries the victim through the air itself.**
 * State 0x337 runs once a frame and does three things: it re-sticks the victim to
 * the slammer, it counts a frame off the proc's 0x20, and it compares the part's
 * height against the floor. While the fighter is still above the floor plus 0x10
 * it re-arms itself for another frame; the moment it is not, the whole landing
 * happens in one go -- shake, two sounds, both fighters stopped, the victim handed
 * to t_thrown_by_robo2.
 *
 * **The proc's 0x1c and 0x20 are a reload value and a counter, and this routine
 * proves it.** 0x20 is decremented every frame, and when it reaches zero it is
 * refilled from 0x1c -- exactly what the header's names for them say ("the
 * animation rate" and "its counter, normally 1"), here used as a divider so
 * something happens every 0x1c frames rather than every frame.
 *
 * **What happens on the divider is a read THROUGH 0x40.** `ldr r3, [r5, #0x40]`
 * then `ldr r3, [r3]` -- so 0x40 holds a pointer here and its first word is
 * tested, and a non-zero value means double_next_a9. Everywhere else in this
 * file 0x40 is a small animation number. Recorded as it stands; nothing here says
 * what the pointer points at.
 *
 * proc+0x40 is read with `ldr`, a full word, against a field the header declares
 * as a halfword -- the fourth routine to do so, so it is reached by offset.
 *
 * The two velocity constants are 0xffe80000 into the part's 0x1c and 0x1a000 into
 * its 0x20, with 0x20000 handed to away_x_vel first. Three magnitudes, three
 * fields, no arithmetic chaining between them.
 */
void away_x_vel(MK3OBJ *obj);
void sans_repell_3(MK3OBJ *obj);
void sans_repell(MK3OBJ *obj);
void his_group_sound(MK3OBJ *obj);
void stop_him(MK3OBJ *obj);
void init_anirate(MK3OBJ *obj);
void stick_him_with_me(MK3OBJ *obj);
void stop_me_player(MK3OBJ *obj);
long t_do_backup(MK3THREAD *thread);             /* pointer slot 0x000f37e0 */
long t_thrown_by_robo2(MK3THREAD *thread);

long t_robo2_slam(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    MK3OBJ  *part;

    if (token == 0) {
        body_slam_init(obj);
        obj->field1c = 3;

        *mk3_frame(thread, thread->frame + 1) = 0x323;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_grab_animation;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x323) {
        *mk3_frame(thread, thread->frame + 1) = 0x324;
        thread->fieldfc = 4;
        return 4;
    }

    if (token == 0x324) {
        obj->field1c = 7;
        ochar_sound(obj);

        obj->field1c = 4;
        init_anirate(obj);

        obj->field1c = 0x20000;
        away_x_vel(obj);

        obj->field08->field1c = 0xffe80000u;

        obj->field1c = 0x1a000;
        obj->field08->field20 = 0x1a000;
        stick_him_with_me(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x337;
        thread->fieldfc = 1;
        return 1;
    }

    if (token == 0x337) {
        sans_repell_3(obj);
        stick_him_with_me(obj);

        obj->field1c = obj->field00->field20 - 1;
        obj->field00->field20 = obj->field1c;

        if (obj->field1c == 0) {
            obj->field00->field20 = obj->field00->field1c;
            obj->field1c = *(uint32_t *)(void *)(uintptr_t)obj->field40;
            if (obj->field1c != 0)
                double_next_a9(obj);
        }

        part = obj->field08;
        obj->field20 = (uint32_t)(int32_t)MK3_FIELD12_S(part);
        obj->field1c = *(uint32_t *)((char *)obj->field00 + 0x40) + 0x10;

        if ((long)obj->field1c > (long)obj->field20) {
            *mk3_frame(thread, thread->frame + 1) = 0x337;
            thread->fieldfc = 1;
            return 1;
        }

        sans_repell(obj);

        obj->field48 = 0x00080008;
        shake_a11(obj);
        rsnd_func(obj, 0xd);

        obj->field1c = 2;
        his_group_sound(obj);

        stop_me_player(obj);
        stop_him(obj);

        obj->field38 = (uint32_t)(uintptr_t)t_thrown_by_robo2;
        xfer_to_thrown(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x359;
        thread->fieldfc = 0xe;
        return 0xe;
    }

    if (token == 0x359) {
        face_opponent(obj);
        ground_player(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x35c;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_do_backup;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x35c)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* --------------------------------------------------------- t_do_back_breaker
 *
 * armv7 0x0004b430, 648 bytes.  **Complete.**
 *
 *      token == 0:      air_init_special(obj)
 *                       stop_me_player(obj); stop_him(obj)
 *                       obj->field1c = clear_inviso;      call_a0_for_him(obj)
 *                       obj->field1c = clear_shadow_bit;  call_a0_for_him(obj)
 *                       clear_shadow_bit(obj)
 *                       obj->field1c = 0x38
 *                       *(uint16_t *)(G + 0x456) = 0x38
 *                       obj->field38 = t_flipped_pause
 *                       takeover_him_sr(obj)
 *                       obj->field1c = obj->field00->field18 = 0x403
 *                       obj->field20 = (uint32_t)obj->field00->field00
 *                       obj->field00->field00->field48 = 0x403
 *                       obj->field1c = 6; ochar_sound(obj)
 *                       me_in_front(obj)
 *                       obj->field40 = 5; get_char_ani2(obj)
 *                       do_next_a9_frame(obj)
 *                       obj->field1c = obj->field20 = 0
 *                       multi_adjust_xy(obj)
 *                       obj->field1c = player_normpal;        call_a0_for_him(obj)
 *                       obj->field1c = last_knockdown_frame;  call_a0_for_him(obj)
 *                       match_him_with_me(obj)
 *                       obj->field1c = 0xa
 *                       obj->field20 = 0xa - 0x75 = -0x6b
 *                       adjust_him_xy(obj)
 *                       token := 0x46b, park 6
 *
 *      token == 0x46b:  obj->field20 = 0x20000
 *                       obj->field24 = 0x20000 - 0x16000 = 0xa000
 *                       obj->field28 = 0xfff
 *                       obj->field1c = 0
 *                       obj->field34 = t_bb_fall_call
 *                       token := 0x472, descend into t_flight_call
 *
 *      token == 0x472:  stop_him(obj)
 *                       obj->field40 = 5; get_char_ani2(obj)
 *                       obj->field40 = obj->field40 + 4
 *                       do_next_a9_frame(obj)
 *                       obj->field1c = 0; obj->field20 = 0x70
 *                       adjust_him_xy(obj)
 *                       obj->field40 = 6; get_char_ani2(obj)
 *                       n = ((MK3OBJ *)obj->field00->him)->field24
 *                       obj->field1c = n
 *                       obj->field40 = n * 4 + obj->field40
 *                       *(uint32_t *)((char *)obj->field00 + 0x38) = obj->field40
 *                       obj->field1c = do_next_a9_frame; call_a0_for_him(obj)
 *                       token := 0x490, park 3
 *
 *      token == 0x490:  obj->field1c = 0xb;  ochar_sound(obj)
 *                       obj->field1c = 2;    his_group_sound(obj)
 *                       obj->field48 = 0x00050005; shake_a11(obj)
 *                       token := 0x499, park 3
 *
 *      token == 0x499:  obj->field1c = 0x1a; damage_to_him(obj)
 *                       obj->field38 = t_post_broken_back
 *                       takeover_him(obj)
 *                       token := 0x49f, park 0x18
 *
 *      token == 0x49f:  frame[frame].handler = t_do_backup
 *
 *      otherwise:       return -3
 *
 * **Five routines are handed across through 0x1c in this one function, and that
 * settles the A0 mechanism beyond argument.** clear_inviso, clear_shadow_bit,
 * player_normpal, last_knockdown_frame and do_next_a9_frame each go into 0x1c and
 * are then run on the OTHER fighter by call_a0_for_him. Two of the five --
 * clear_shadow_bit and do_next_a9_frame -- are also called directly on this
 * fighter a line or two away, so the same routine reaches both fighters by two
 * different spellings in one state.
 *
 * **0x34 holds a handler too, and t_flight_call is what reads it.** Where every
 * other flight in this file descends into t_flight, this one fills the same four
 * fields and descends into t_flight_call with t_bb_fall_call parked in 0x34 -- so
 * the call variant takes a per-frame callback and the plain one does not. That is
 * the only site in the file that writes 0x34.
 *
 * **The victim's animation is offset by its own character number.** State 0x472
 * resolves animation 6, then adds `him->field24 * 4` to it and writes the result
 * both into 0x40 and into the proc's 0x38 -- the same times-four scaling
 * t_jax_multi_slam_pose applies to a repeat count, here applied to a character
 * index instead.
 *
 * The 0x38 on the proc is inside a pad in this header, so it is written by
 * offset. Nothing else in the file touches it.
 *
 * 0x40 is read back after get_char_ani2 twice and adjusted both times -- plus 4
 * on the first, plus the scaled index on the second -- so that routine leaves its
 * answer there and the caller is free to move along the table from it.
 */
void clear_shadow_bit(MK3OBJ *obj);
void player_normpal(MK3OBJ *obj);
void match_him_with_me(MK3OBJ *obj);
void adjust_him_xy(MK3OBJ *obj);
void damage_to_him(MK3OBJ *obj);
void me_in_front(MK3OBJ *obj);
void multi_adjust_xy(MK3OBJ *obj);
long t_flight_call(MK3THREAD *thread);           /* pointer slot 0x000f37f4 */
long t_bb_fall_call(MK3THREAD *thread);
long t_post_broken_back(MK3THREAD *thread);

long t_do_back_breaker(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    uint32_t n;

    if (token == 0) {
        air_init_special(obj);
        stop_me_player(obj);
        stop_him(obj);

        obj->field1c = (uint32_t)(uintptr_t)clear_inviso;
        call_a0_for_him(obj);
        obj->field1c = (uint32_t)(uintptr_t)clear_shadow_bit;
        call_a0_for_him(obj);
        clear_shadow_bit(obj);

        obj->field1c = 0x38;
        *(uint16_t *)(G_BYTES + 0x456) = 0x38;

        obj->field38 = (uint32_t)(uintptr_t)t_flipped_pause;
        takeover_him_sr(obj);

        obj->field1c = 0x403;
        obj->field00->field18 = 0x403;
        obj->field20 = (uint32_t)(uintptr_t)obj->field00->field00;
        obj->field00->field00->field48 = 0x403;

        obj->field1c = 6;
        ochar_sound(obj);
        me_in_front(obj);

        obj->field40 = 5;
        get_char_ani2(obj);
        do_next_a9_frame(obj);

        obj->field1c = 0;
        obj->field20 = 0;
        multi_adjust_xy(obj);

        obj->field1c = (uint32_t)(uintptr_t)player_normpal;
        call_a0_for_him(obj);
        obj->field1c = (uint32_t)(uintptr_t)last_knockdown_frame;
        call_a0_for_him(obj);

        match_him_with_me(obj);

        obj->field1c = 0xa;
        obj->field20 = (uint32_t)(0xa - 0x75);
        adjust_him_xy(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x46b;
        thread->fieldfc = 6;
        return 6;
    }

    if (token == 0x46b) {
        obj->field20 = 0x20000;
        obj->field24 = 0x20000 - 0x16000;
        obj->field28 = 0xfff;
        obj->field1c = 0;
        obj->field34 = (uint32_t)(uintptr_t)t_bb_fall_call;

        *mk3_frame(thread, thread->frame + 1) = 0x472;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_flight_call;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x472) {
        stop_him(obj);

        obj->field40 = 5;
        get_char_ani2(obj);
        obj->field40 = obj->field40 + 4;
        do_next_a9_frame(obj);

        obj->field1c = 0;
        obj->field20 = 0x70;
        adjust_him_xy(obj);

        obj->field40 = 6;
        get_char_ani2(obj);

        n = ((MK3OBJ *)(void *)(uintptr_t)obj->field00->him)->field24;
        obj->field1c = n;
        obj->field40 = n * 4 + obj->field40;
        *(uint32_t *)((char *)obj->field00 + 0x38) = obj->field40;

        obj->field1c = (uint32_t)(uintptr_t)do_next_a9_frame;
        call_a0_for_him(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x490;
        thread->fieldfc = 3;
        return 3;
    }

    if (token == 0x490) {
        obj->field1c = 0xb;
        ochar_sound(obj);

        obj->field1c = 2;
        his_group_sound(obj);

        obj->field48 = 0x00050005;
        shake_a11(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x499;
        thread->fieldfc = 3;
        return 3;
    }

    if (token == 0x499) {
        obj->field1c = 0x1a;
        damage_to_him(obj);

        obj->field38 = (uint32_t)(uintptr_t)t_post_broken_back;
        takeover_him(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x49f;
        thread->fieldfc = 0x18;
        return 0x18;
    }

    if (token != 0x49f)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_do_backup);
}


/* ---------------------------------------------------------------- t_jax_slam
 *
 * armv7 0x0004c1bc, 796 bytes.  **Complete.**  The largest routine in the file.
 *
 *      token == 0:      body_slam_init(obj)
 *                       obj->field1c = set_ignore_y; call_a0_for_him(obj)
 *                       obj->field1c = 3
 *                       token := 0x246, descend into t_grab_animation
 *
 *      token == 0x246:  token := 0x247, park 3
 *
 *      token == 0x247:  read_lp_tick_state(obj)
 *                       obj->field00->field28 = obj->field1c
 *                       *(uint32_t *)(proc + 0x2c) = 0
 *                       obj->field1c = 4
 *                       *(uint32_t *)(proc + 0x30) = 4
 *                       throw_voice(obj)
 *                       -- falls into the 0x292 body --
 *
 *      token == 0x292:  double_next_a9(obj)
 *                       obj->field1c = 3
 *                       token := 0x257, descend into t_jax_slam_sleep
 *
 *      token == 0x257:  n = *(uint32_t *)obj->field40
 *                       obj->field1c = n
 *                       if (n != 0) -- back into the 0x292 body --
 *                       obj->field1c = 5;  ochar_sound(obj)
 *                       obj->field1c = 2;  his_group_sound(obj)
 *                       obj->field48 = 0x00090009; shake_a11(obj)
 *                       obj->field1c = obj->field54 = 0xa
 *                       add_combo_damage(obj); damage_to_him(obj)
 *                       inc_his_p_hit(obj); am_i_joy(obj)
 *                       if (obj->field5c != 0
 *                           && *(uint32_t *)(proc + 0x2c) != 0) {
 *                           obj->field38 = t_thrown_by_jax
 *                           xfer_to_thrown(obj)
 *                           token := 0x299, park 0xa
 *                       } else if (--*(uint32_t *)(proc + 0x30) == 0) {
 *                           the same throw
 *                       } else {
 *                           obj->field1c = 4
 *                           token := 0x280, descend into t_jax_slam_sleep
 *                       }
 *
 *      token == 0x280:  obj->field1c = 0x30
 *                       *(uint16_t *)(G + 0x456) = 0x30
 *                       obj->field30 = 4
 *                       token := 0x285, descend into t_jax_multi_slam_pose
 *
 *      token == 0x285:  obj->field30 = 3
 *                       token := 0x287, descend into t_jax_multi_slam_pose
 *
 *      token == 0x287:  obj->field30 = 2
 *                       token := 0x289, descend into t_jax_multi_slam_pose
 *
 *      token == 0x289:  flip_multi(obj)
 *                       obj->field1c = ~0xe; obj->field20 = 0
 *                       multi_adjust_xy(obj)
 *                       flip_multi_ob((MK3OBJ *)obj->a10)
 *                       obj->field30 = 2
 *                       token := 0x292, descend into t_jax_multi_slam_pose
 *
 *      token == 0x299:  obj->field54 = 3
 *                       obj->field40 = 3 + 0x20 = 0x23
 *                       find_ani_part_a14(obj)
 *                       obj->field1c = 5
 *                       token := 0x29f, descend into t_mframew
 *
 *      token == 0x29f:  pop a level, or t_local_reaction_exit at the bottom
 *
 *      otherwise:       return -3
 *
 * **Eleven states, and the shape is a loop, not a line.** 0x292 goes to
 * t_jax_slam_sleep, 0x257 reads the answer, and if the player is still holding
 * the button the thread goes back into 0x292 -- so 0x257 and 0x292 are a cycle
 * that runs once per repetition. That is what makes Jax's slam the multi-slam:
 * t_jax_slam_sleep is the routine that decides whether to go round again, and
 * 0x280 through 0x289 are the four poses one repetition draws.
 *
 * **Two counters, and they mean different things.** The proc's 0x30 counts
 * repetitions down from 4 and ends the sequence when it reaches zero; the
 * object's OWN 0x30 counts 4, 3, 2, 2 across the four poses and is what
 * t_jax_multi_slam_pose scales its two animation offsets by. Same offset on two
 * different structures, one a budget and one a frame index.
 *
 * **0x40 is read THROUGH again**, as in t_robo2_slam: `ldr r4, [r6, #0x40]` then
 * `ldr r4, [r4]`. Here the loaded word does double duty -- non-zero means go
 * round again, and zero is exactly the value the shared frame-push tail needs to
 * write into the new level's token slot, so the compiler kept it in r4 and used
 * it for both.
 *
 * **The throw is reached two ways and the code is shared.** Either the player
 * let go while the flag at proc+0x2c was set, or the repetition budget ran out;
 * both land on the same four lines. proc+0x2c is the flag t_jax_slam_sleep
 * raises, so the two conditions are "the player asked to stop" and "no
 * repetitions left".
 *
 * proc+0x2c and proc+0x30 are inside a pad in this header, so both are reached
 * by offset -- the same expression mkprop.c uses for 0x2c.
 */
void flip_multi(MK3OBJ *obj);
void flip_multi_ob(MK3OBJ *obj);
void add_combo_damage(MK3OBJ *obj);
void inc_his_p_hit(MK3OBJ *obj);
void find_ani_part_a14(MK3OBJ *obj);
void set_ignore_y(MK3OBJ *obj);
long am_i_joy(MK3OBJ *obj);
long t_thrown_by_jax(MK3THREAD *thread);
long t_jax_multi_slam_pose(MK3THREAD *thread);

long t_jax_slam(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    uint32_t n, next;
    int      throw_now;

    if (token == 0) {
        body_slam_init(obj);

        obj->field1c = (uint32_t)(uintptr_t)set_ignore_y;
        call_a0_for_him(obj);

        obj->field1c = 3;
        *mk3_frame(thread, thread->frame + 1) = 0x246;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_grab_animation;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x246) {
        *mk3_frame(thread, thread->frame + 1) = 0x247;
        thread->fieldfc = 3;
        return 3;
    }

    if (token == 0x247 || token == 0x292 || token == 0x257) {
        if (token == 0x247) {
            read_lp_tick_state(obj);
            obj->field00->field28 = obj->field1c;
            *(uint32_t *)((char *)obj->field00 + 0x2c) = 0;
            obj->field1c = 4;
            *(uint32_t *)((char *)obj->field00 + 0x30) = 4;
            throw_voice(obj);
            token = 0x292;                  /* falls into that body */
        }

        if (token == 0x257) {
            n = *(uint32_t *)(void *)(uintptr_t)obj->field40;
            obj->field1c = n;

            if (n == 0) {
                obj->field1c = 5;
                ochar_sound(obj);
                obj->field1c = 2;
                his_group_sound(obj);

                obj->field48 = 0x00090009;
                shake_a11(obj);

                obj->field1c  = 0xa;
                obj->field54  = 0xa;
                add_combo_damage(obj);
                damage_to_him(obj);
                inc_his_p_hit(obj);
                am_i_joy(obj);

                throw_now = 0;
                if (obj->field5c != 0) {
                    obj->field1c =
                        *(uint32_t *)((char *)obj->field00 + 0x2c);
                    if (obj->field1c != 0)
                        throw_now = 1;
                }

                if (!throw_now) {
                    obj->field1c =
                        *(uint32_t *)((char *)obj->field00 + 0x30) - 1;
                    if (obj->field1c == 0) {
                        throw_now = 1;
                    } else {
                        *(uint32_t *)((char *)obj->field00 + 0x30) =
                            obj->field1c;
                        obj->field1c = 4;

                        *mk3_frame(thread, thread->frame + 1) = 0x280;
                        thread->frame = thread->frame + 1;
                        mk3_frame(thread, thread->frame)[1] =
                            (uint32_t)(uintptr_t)t_jax_slam_sleep;
                        *mk3_frame(thread, thread->frame + 1) = 0;
                        return 0;
                    }
                }

                obj->field38 = (uint32_t)(uintptr_t)t_thrown_by_jax;
                xfer_to_thrown(obj);
                *mk3_frame(thread, thread->frame + 1) = 0x299;
                thread->fieldfc = 0xa;
                return 0xa;
            }
        }

        /* the 0x292 body */
        double_next_a9(obj);
        obj->field1c = 3;

        *mk3_frame(thread, thread->frame + 1) = 0x257;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_jax_slam_sleep;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x280 || token == 0x285 || token == 0x287
        || token == 0x289) {
        if (token == 0x280) {
            obj->field1c = 0x30;
            *(uint16_t *)(G_BYTES + 0x456) = 0x30;
            obj->field30 = 4;
            next = 0x285;

        } else if (token == 0x285) {
            obj->field30 = 3;
            next = 0x287;

        } else if (token == 0x287) {
            obj->field30 = 2;
            next = 0x289;

        } else {
            flip_multi(obj);
            obj->field1c = (uint32_t)~0xeu;
            obj->field20 = 0;
            multi_adjust_xy(obj);
            flip_multi_ob((MK3OBJ *)(void *)(uintptr_t)obj->a10);
            obj->field30 = 2;
            next = 0x292;
        }

        *mk3_frame(thread, thread->frame + 1) = next;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_jax_multi_slam_pose;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x299) {
        obj->field54 = 3;
        obj->field40 = 3 + 0x20;
        find_ani_part_a14(obj);
        obj->field1c = 5;

        *mk3_frame(thread, thread->frame + 1) = 0x29f;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x29f)
        return -3;

    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
        return 0;
    }

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}
