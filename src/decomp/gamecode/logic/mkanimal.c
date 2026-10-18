/*
 * mkanimal.c -- gamecode/logic/mkanimal.c, decompiled.
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

long t_r_bat_bite(struct MK3THREAD *thread);

/* t_r_kitana_decap -- armv7 0x000a0c84, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = t_r_bat_bite
 *      frame[frame+1].w0 = 0
 */

long t_r_kitana_decap(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_r_bat_bite);
}





/* --------------------------------------------------------------------
 * What the readers could prove. See tools/pushfn.py, which executes
 * a body symbolically, and tools/microfn.py, which matches whole
 * bodies against templates. Both refuse anything they cannot account
 * for instruction by instruction.
 * -------------------------------------------------------------------- */

long t_shake_ob_up(struct MK3THREAD *thread);
long t_wait_forever(struct MK3THREAD *thread);

/* t_head_pop_off -- armv7 0x000a0cb8, 52 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      frame[frame].handler = t_wait_forever
 *      frame[frame+1].w0 = 0
 */

long t_head_pop_off(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_wait_forever);
}

/* t_spider_shake_jsrp -- armv7 0x000a0cec, 68 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x6
 *      obj->field20 = 0x3
 *      obj->field24 = 0x10
 *      frame[frame].handler = t_shake_ob_up
 *      frame[frame+1].w0 = 0
 */

long t_spider_shake_jsrp(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x6;
    obj->field20 = 0x3;
    obj->field24 = 0x10;

    return mk3_push_handler(thread, (MK3THREADFUNC)t_shake_ob_up);
}

/* --------------------------------------------------------------------
 * Straight-line leaves, read by tools/leaffn.py: stores, calls and
 * a return, with every instruction accounted for. It refuses
 * anything that branches, any return value it cannot prove, and any
 * value read from a field the function also writes -- that is a
 * saved value being put back, not a re-read.
 * -------------------------------------------------------------------- */

void send_code_a3(MK3OBJ *obj);

/* animality_tune -- armv7 0x000a0fe8, 16 bytes.  **Complete.**
 *
 *      obj->field28 = 0x3a
 *      send_code_a3(obj)
 */
void animality_tune(MK3OBJ *obj)
{
    obj->field28 = 0x3a;
    send_code_a3(obj);
}

/* --------------------------------------------------------------------
 * Added by a later sweep -- tools/sweep.py, running the same
 * readers again after one of them learned something. Each still
 * refuses anything it cannot account for instruction by
 * instruction; see tools/pushfn.py and tools/leaffn.py.
 * -------------------------------------------------------------------- */

long t_victory_animation(struct MK3THREAD *thread);
void death_blow_complete(MK3OBJ *obj);
void set_inviso(MK3OBJ *obj);
void shake_a11(MK3OBJ *obj);
void tsound_func(MK3OBJ *obj, uint32_t arg);

/* t_eaten_by_snake -- armv7 0x000a2f60, 96 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      tsound_func(obj, 0x24)
 *      tsound_func(obj, 0x25)
 *      obj->field48 = 0xa000a
 *      shake_a11(obj)
 *      set_inviso(obj)
 *      frame[frame].handler = t_wait_forever
 *      frame[frame+1].w0 = 0
 */

long t_eaten_by_snake(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    tsound_func(obj, 0x24);
    tsound_func(obj, 0x25);
    obj->field48 = 0xa000a;
    shake_a11(obj);
    set_inviso(obj);

    return mk3_push_handler(thread, (MK3THREADFUNC)t_wait_forever);
}

/* t_animality_complete -- armv7 0x000a3050, 76 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      death_blow_complete(obj)
 *      player_normpal(obj)
 *      frame[frame].handler = t_victory_animation
 *      frame[frame+1].w0 = 0
 */

long t_animality_complete(MK3THREAD *thread)
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

long t_backwards_ani(MK3THREAD *thread);

/* t_spider_shake -- armv7 0x000a0d30, 104 bytes.  **Complete.**
 *
 *      token == 0:
 *          token := 0x713, then descend into t_spider_shake_jsrp
 *      token == 0x713:
 *          frame[frame].handler = t_wait_forever
 *      otherwise:  return -3
 */
long t_spider_shake(MK3THREAD *thread)
{
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x713;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_spider_shake_jsrp;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x713)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_wait_forever);
}

/* t_unmorph_and_exit -- armv7 0x000a0d98, 128 bytes.  **Complete.**
 *
 *      token == 0:
 *          obj->field1c = 0x5
 *          token := 0x836, then descend into t_backwards_ani
 *      token == 0x836:
 *          frame[frame].handler = t_animality_complete
 *      otherwise:  return -3
 */
long t_unmorph_and_exit(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field1c = 0x5;
        *mk3_frame(thread, thread->frame + 1) = 0x836;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_backwards_ani;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x836)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_animality_complete);
}

/* tl_null_animal -- armv7 0x000a0e18, 76 bytes.  **Complete.**
 *
 *      token == 0:
 *          park(token 0x8c3, duration 0x40)
 *      token == 0x8c3:
 *          frame[frame].handler = t_animality_complete
 *      otherwise:  return -3
 */
long tl_null_animal(MK3THREAD *thread)
{
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x8c3;
        thread->fieldfc = 0x40;
        return 0x40;
    }

    if (token != 0x8c3)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_animality_complete);
}

/* --------------------------------------------------------------------
 * Added by a later sweep -- tools/sweep.py, running the same
 * readers again after one of them learned something. Each still
 * refuses anything it cannot account for instruction by
 * instruction; see tools/pushfn.py and tools/leaffn.py.
 * -------------------------------------------------------------------- */

long t_flight(MK3THREAD *thread);
long t_land_on_my_back(MK3THREAD *thread);
long t_white_flash(MK3THREAD *thread);
long create_blood_proc(MK3OBJ *obj);
void create_fx(MK3OBJ *obj);
void death_scream(MK3OBJ *obj);

/* t_dino_bucked -- armv7 0x000a2b50, 184 bytes.  **Complete.**
 *
 *      token == 0:
 *          obj->field1c = 0x1
 *          create_blood_proc(obj)
 *          obj->field48 = 0x60006
 *          shake_a11(obj)
 *          death_scream(obj)
 *          rsnd_func(obj, 0x3)
 *          obj->field1c = 0x30000
 *          obj->field20 = 0xffeb0000
 *          obj->field24 = 0x5000
 *          obj->field28 = 0x4
 *          obj->field40 = 0x1e
 *          token := 0x52f, then descend into t_flight
 *      token == 0x52f:
 *          frame[frame].handler = t_land_on_my_back
 *      otherwise:  return -3
 */
long t_dino_bucked(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field1c = 0x1;
        create_blood_proc(obj);
        obj->field48 = 0x60006;
        shake_a11(obj);
        death_scream(obj);
        rsnd_func(obj, 0x3);
        obj->field1c = 0x30000;
        obj->field20 = 0xffeb0000;
        obj->field24 = 0x5000;
        obj->field28 = 0x4;
        obj->field40 = 0x1e;
        *mk3_frame(thread, thread->frame + 1) = 0x52f;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x52f)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_land_on_my_back);
}

/* t_r_egg -- armv7 0x000a2fc0, 144 bytes.  **Complete.**
 *
 *      token == 0:
 *          death_scream(obj)
 *          set_inviso(obj)
 *          obj->field1c = 0x18
 *          create_fx(obj)
 *          token := 0x295, then descend into t_white_flash
 *      token == 0x295:
 *          frame[frame].handler = t_wait_forever
 *      otherwise:  return -3
 */
long t_r_egg(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        death_scream(obj);
        set_inviso(obj);
        obj->field1c = 0x18;
        create_fx(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x295;
        thread->frame = thread->frame + 1;      /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_white_flash;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x295)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_wait_forever);
}


/* ------------------------------------------------------------ kill_and_stop_scrolling
 *
 * armv7 0x000a0fbc, 20 bytes.  **Complete.**
 *
 *      MKEvent_Add(1, 2, 0, obj->field00->field08)
 *
 * One event and nothing else. The fourth argument is the proc's 0x08 -- the
 * strength index, the player number -- so the event says which player it is about;
 * the first three are constants.
 */
void MKEvent_Add(long a, long b, long c, long d);

void kill_and_stop_scrolling(MK3OBJ *obj)
{
    MKEvent_Add(1, 2, 0, (long)obj->field00->field08);
}

/* --------------------------------------------------------------------- face_him_at_me
 *
 * armv7 0x000a0fd0, 24 bytes.  **Complete.**
 *
 *      obj->field1c = (uint32_t)face_opponent
 *      call_a0_for_him(obj)
 *
 * The A0 mechanism with nothing around it: put `face_opponent` in 0x1c and let
 * `call_a0_for_him` run it on the OTHER fighter. Same routine and same pointer slot
 * 0x000f3758 that tl_do_shake and tl_do_leg_throw hand across in mkstat.c, here as a
 * named one-line helper.
 */
void face_opponent(MK3OBJ *obj);                 /* pointer slot 0x000f3758 */
void call_a0_for_him(MK3OBJ *obj);

void face_him_at_me(MK3OBJ *obj)
{
    obj->field1c = (uint32_t)(uintptr_t)face_opponent;
    call_a0_for_him(obj);
}

/* -------------------------------------------------------------------- cutup_body_init
 *
 * armv7 0x000a1d2c, 24 bytes.  **Complete.**
 *
 *      death_scream(obj)
 *      obj->field08->field2c = obj->field08->field24 + delta
 *
 * **Two arguments.** The second arrives in r1 and is added to the part's 0x24 to
 * produce its 0x2c, so the caller supplies an offset from the character number --
 * 0x24 is the field every per-character table in this module indexes by. Only two of
 * the routines in these files take a second argument at all, and this is one.
 */
void death_scream(MK3OBJ *obj);

void cutup_body_init(MK3OBJ *obj, uint32_t delta)
{
    MK3OBJ *part;

    death_scream(obj);

    part = obj->field08;
    part->field2c = part->field24 + delta;
}

/* ----------------------------------------------------------------------- set_vel_flip
 *
 * armv7 0x000a3c28, 28 bytes.  **Complete.**
 *
 *      obj->field2c = obj->field08->field28
 *      if (obj->field2c & 0x10) obj->field1c = -obj->field1c
 *      set_x_vel_player(obj)
 *
 * **Bit 4 of the part's 0x28 mirrors the velocity.** 0x1c holds the speed the caller
 * wants; if that bit is set the sign is flipped before `set_x_vel_player` reads it, so
 * one caller can ask for "forwards" and get the right direction either way.
 *
 * The whole conditional is one `ittt ne` block -- three instructions predicated on the
 * `tst`, so there is no branch and 0x1c is left alone when the bit is clear.
 */
void set_x_vel_player(MK3OBJ *obj);

void set_vel_flip(MK3OBJ *obj)
{
    obj->field2c = obj->field08->field28;

    if ((obj->field2c & 0x10u) != 0)
        obj->field1c = (uint32_t)(-(int32_t)obj->field1c);

    set_x_vel_player(obj);
}


/* ------------------------------------------------------------------ create_fx_for_him
 *
 * armv7 0x000a115c, 32 bytes.  **Complete.**
 *
 *      saved_proc = obj->field00
 *      saved_part = obj->field08
 *      obj->field08 = saved_proc->him
 *      obj->field00 = saved_proc->field00->field00
 *      create_fx(obj)
 *      obj->field00 = saved_proc
 *      obj->field08 = saved_part
 *
 * **The swap-call-restore idiom on two fields at once.** `create_fx` works on
 * whatever obj->field00 and obj->field08 point at, so to make the effect happen at the
 * OTHER fighter this routine writes that fighter's proc and part into its own object,
 * calls, and puts both back. mkslam.c's `stick_him_with_me` does the same with one
 * field; this is the two-field version.
 *
 * The new proc comes through two hops -- `proc->field00->field00`, this proc's
 * opponent object and then that object's own proc -- while the part comes from
 * `proc->him` directly. So the two pointers are reached by different routes and the
 * routine does not assume they agree.
 */
void create_fx(MK3OBJ *obj);

void create_fx_for_him(MK3OBJ *obj)
{
    MK3OBJPROC *saved_proc = obj->field00;
    MK3OBJ     *saved_part = obj->field08;

    obj->field08 = (MK3OBJ *)(void *)(uintptr_t)saved_proc->him;
    obj->field00 = saved_proc->field00->field00;

    create_fx(obj);

    obj->field00 = saved_proc;
    obj->field08 = saved_part;
}

/* ---------------------------------------------------------------------- pengo_animate
 *
 * armv7 0x000a2c08, 32 bytes.  **Complete.**
 *
 *      next_anirate(obj)
 *      obj->field1c = obj->field00->field20
 *      if (obj->field1c == 1) rsnd_func(obj, 6)
 *
 * A sound on one exact frame. The proc's 0x20 is the animation counter, and sound 6
 * fires only on the pass where it reads 1 -- not above and not below -- so the noise
 * lands once per animation rather than once per frame.
 *
 * `next_lao_anirate` in mkstat.c is the same shape with its own counter in 0x44 and a
 * reload; this one has no counter of its own and rides the proc's.
 */
long next_anirate(MK3OBJ *obj);

void pengo_animate(MK3OBJ *obj)
{
    next_anirate(obj);

    obj->field1c = obj->field00->field20;
    if (obj->field1c == 1)
        rsnd_func(obj, 6);
}

/* --------------------------------------------------------------------------- ground_ob
 *
 * armv7 0x000a0f90, 44 bytes.  **Complete.**
 *
 *      obj->field1c = *(uint32_t *)(G + 0xac)
 *      obj->field20 = GetFrameHeight(target->field2c)
 *      obj->field1c = obj->field1c - obj->field20 - 9
 *      *(uint16_t *)((char *)target + 0x12) = obj->field1c
 *
 * **Two arguments, and it stands the second one on the floor.** The floor is the word
 * at G + 0xac, the height comes from `GetFrameHeight` on the target's 0x2c, and the
 * nine is a fixed inset. So the y written into the target's 0x12 is floor minus height
 * minus nine.
 *
 * This is the second routine in the module to read G + 0xac: mkstat.c's
 * `t_turn_into_a_baby` does the same placement but takes its height from
 * `mk3_getbbox` instead of `GetFrameHeight`, and uses no inset. Two ways to measure
 * the same thing, in two files.
 */
int GetFrameHeight(uint32_t ani);

void ground_ob(MK3OBJ *obj, MK3OBJ *target)
{
    obj->field1c = *(uint32_t *)(G_BYTES + 0xac);
    obj->field20 = (uint32_t)GetFrameHeight(target->field2c);
    obj->field1c = obj->field1c - obj->field20 - 9;

    MK3_SET_FIELD12(target, obj->field1c);
}

/* ----------------------------------------------------------------------------- q_bat_1
 *
 * armv7 0x000a359c, 32 bytes.  **Complete.**
 *
 *      get_x_dist(obj)
 *      if (obj->field28 > 0xff) q_yes(obj); else q_no(obj)
 *
 * A distance predicate in the q_ family: it answers in 0x5c through `q_yes` / `q_no`
 * like every other one, and the question is "is the opponent more than 255 away".
 */
/* Read from the binary in joy.c: nothing is deliberately left in r0.
 * The `long` this used to be declared as came from a call site that
 * ignored the result. */
void get_x_dist(MK3OBJ *obj);
void distance_off_ground(MK3OBJ *obj);
void q_yes(MK3OBJ *obj);
void q_no(MK3OBJ *obj);

void q_bat_1(MK3OBJ *obj)
{
    get_x_dist(obj);

    if ((long)obj->field28 > 0xff)
        q_yes(obj);
    else
        q_no(obj);
}

/* ----------------------------------------------------------------------------- q_bat_2
 *
 * armv7 0x000a35bc, 32 bytes.  **Complete.**
 *
 *      get_x_dist(obj)
 *      if (obj->field28 <= 0x20) q_yes(obj); else q_no(obj)
 *
 * The near test to q_bat_1's far one -- thirty-two or closer.
 */
void q_bat_2(MK3OBJ *obj)
{
    get_x_dist(obj);

    if ((long)obj->field28 <= 0x20)
        q_yes(obj);
    else
        q_no(obj);
}

/* ----------------------------------------------------------------------------- q_bat_3
 *
 * armv7 0x000a35dc, 32 bytes.  **Complete.**
 *
 * **The same routine as q_bat_1, instruction for instruction.** The two differ only in
 * their pc-relative displacements -- three bytes, at offsets 8, 0x14 and 0x1c, being
 * the `bgt` and the two `bl` encodings shifted by the 0x40 between them. Same call,
 * same field, same threshold of 0xff, same answer.
 *
 * Checked by comparing the two byte ranges rather than assumed from reading them: the
 * ranges are NOT identical, and it would have been wrong to say so. What is identical
 * is the behaviour.
 */
void q_bat_3(MK3OBJ *obj)
{
    get_x_dist(obj);

    if ((long)obj->field28 > 0xff)
        q_yes(obj);
    else
        q_no(obj);
}

/* ----------------------------------------------------------------------------- q_bat_4
 *
 * armv7 0x000a35fc, 32 bytes.  **Complete.**
 *
 *      distance_off_ground(obj)
 *      if (obj->field1c <= 3) q_yes(obj); else q_no(obj)
 *
 * **The odd one of the four**: a different call, a different field and a vertical
 * question. `distance_off_ground` answers in 0x1c where `get_x_dist` answers in 0x28,
 * so the three horizontal predicates and this one do not share a slot -- which is why
 * they cannot be collapsed into one parameterised helper.
 */
void q_bat_4(MK3OBJ *obj)
{
    distance_off_ground(obj);

    if ((long)obj->field1c <= 3)
        q_yes(obj);
    else
        q_no(obj);
}


/* ------------------------------------------------------------------------ t_kitty_spin
 *
 * armv7 0x000a0c44, 64 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field40 = obj->field48
 *      obj->field1c = 3
 *      frame[frame].handler = t_mframew
 *
 * Two stores and an install. 0x48 is where the caller left the animation and 0x40 is
 * where the animation routines read it from, so the whole routine is a move between
 * the two plus a three-frame wait.
 */
long t_mframew(MK3THREAD *thread);

long t_kitty_spin(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field40 = obj->field48;
    obj->field1c = 3;

    return mk3_install(thread, (MK3THREADFUNC)t_mframew);
}

/* ------------------------------------------------------------------------ t_odor_proc
 *
 * armv7 0x000a0bd4, 112 bytes.  **Complete.**
 *
 *      token == 0:      obj->field08->field2c = 0x11c0
 *                       obj->field48 = 0x3c
 *                       token := 0x216, park 2
 *
 *      token == 0x216:  if (--obj->field48 != 0) token := 0x216, park 2
 *                       frame[frame].handler = t_wait_forever
 *
 *      otherwise:       return -3
 *
 * Sixty passes, two frames apart -- a hundred and twenty frames of smell -- and then
 * t_wait_forever. The animation for it goes into the part's 0x2c as a single constant,
 * 0x11c0, with no table lookup, so the odour looks the same whoever produced it.
 */
long t_wait_forever(MK3THREAD *thread);

long t_odor_proc(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field08->field2c = 0x11c0;
        obj->field48 = 0x3c;

    } else if (token == 0x216) {
        obj->field48 = obj->field48 - 1;
        if (obj->field48 == 0)
            return mk3_install(thread, (MK3THREADFUNC)t_wait_forever);

    } else {
        return -3;
    }

    *mk3_frame(thread, thread->frame + 1) = 0x216;
    thread->fieldfc = 2;
    return 2;
}

/* --------------------------------------------------------------------- t_crunch_sounds
 *
 * armv7 0x000a10e8, 116 bytes.  **Complete.**
 *
 *      token == 0:      obj->a10 = 6
 *      the pair:        tsound_func(obj, 0x24)
 *                       tsound_func(obj, 0x25)
 *                       token := 0x3d2, park 0x10
 *
 *      token == 0x3d2:  if (--obj->a10 > 0) -- back to the pair --
 *                       token := 0x3d6, return 0x16462 (delete)
 *
 *      otherwise:       return -3
 *
 * **Two sounds together, six times, sixteen frames apart.** 0x24 and 0x25 are always
 * played as a pair with nothing between them, so they are one noise made of two
 * samples rather than two events.
 *
 * **Token 0x3d6 is not in the dispatch.** Reaching it would return -3, and it is safe
 * for the same reason it is safe in mkstat.c's `t_jade_flash_proc`: the park is
 * 0x16462, the DELETE sentinel -- so the state is a terminator and the thread is
 * released by mk3_update rather than merely parked. See mk3logic.h. That
 * is the second site for this pattern, which makes it an idiom rather than an oversight.
 */
long t_crunch_sounds(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->a10 = 6;

    } else if (token == 0x3d2) {
        obj->a10 = obj->a10 - 1;
        if ((long)obj->a10 <= 0) {
            *mk3_frame(thread, thread->frame + 1) = 0x3d6;
            thread->fieldfc = 0x16462;
            return 0x16462;
        }

    } else {
        return -3;
    }

    tsound_func(obj, 0x24);
    tsound_func(obj, 0x25);

    *mk3_frame(thread, thread->frame + 1) = 0x3d2;
    thread->fieldfc = 0x10;
    return 0x10;
}


/* --------------------------------------------------------------------- t_r_ermac_upcut
 *
 * armv7 0x000a2a68, 116 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field48 = 0x00080008; shake_a11(obj)
 *      rsnd_func(obj, 0xa)
 *      obj->field1c = 1; create_blood_proc(obj)
 *      cutup_body_init(obj, 0x1af4)
 *      obj->field1c = 0x10000
 *      obj->field20 = 0x10000 - 0x90000 = -0x80000
 *      frame[frame].handler = t_head_pop_off
 *
 * The victim's side of an Ermac uppercut animality: shake, sound, blood, and then the
 * body is cut up and the head sent off through `t_head_pop_off`, which is written
 * earlier in this file.
 *
 * **This is the caller that shows what `cutup_body_init`'s second argument is for.**
 * 0x1af4 is added to the part's character number to give its 0x2c, so the offset picks
 * a body-pieces animation set and the character number picks the entry within it.
 *
 * 0x48 is the shake magnitude again, 0x00080008 -- a doubled pair, like most of them.
 */
void shake_a11(MK3OBJ *obj);
long create_blood_proc(MK3OBJ *obj);
void cutup_body_init(MK3OBJ *obj, uint32_t delta);
long t_head_pop_off(MK3THREAD *thread);

long t_r_ermac_upcut(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field48 = 0x00080008;
    shake_a11(obj);
    rsnd_func(obj, 0xa);

    obj->field1c = 1;
    create_blood_proc(obj);

    cutup_body_init(obj, 0x1af4);

    obj->field1c = 0x10000;
    obj->field20 = (uint32_t)(0x10000 - 0x90000);

    return mk3_install(thread, (MK3THREADFUNC)t_head_pop_off);
}

/* ------------------------------------------------------------------ t_next_anirate_a10
 *
 * armv7 0x000a1220, 128 bytes.  **Complete.**
 *
 *      token == 0:      token := 0x819, park 1
 *
 *      token == 0x819:  next_anirate(obj)
 *                       if (--obj->a10 > 0) token := 0x819, park 1
 *                       pop a level, or t_local_reaction_exit at the bottom
 *
 *      otherwise:       return -3
 *
 * **A generic "step the animation rate for 0x44 frames" helper**, with the count
 * supplied by the caller in 0x44 and nothing else of its own. State 0 does no work at
 * all -- it exists only to get the thread onto the one-frame cadence.
 *
 * The two paths that write the token share one store site, entered with the frame
 * index either loaded at entry or reloaded after the decrement, which is why the
 * `ldrgt` sits inside the `it gt` block rather than after the branch.
 */
long t_local_reaction_exit(MK3THREAD *thread);

long t_next_anirate_a10(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x819) {
        next_anirate(obj);

        obj->a10 = obj->a10 - 1;
        if ((long)obj->a10 <= 0) {
            if ((long)thread->frame > 0) {
                thread->frame = thread->frame - 1;
                return 0;
            }
            return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
        }

    } else if (token != 0) {
        return -3;
    }

    *mk3_frame(thread, thread->frame + 1) = 0x819;
    thread->fieldfc = 1;
    return 1;
}


/* ------------------------------------------------------------------ t_animate_till_a11
 *
 * armv7 0x000a12a0, 148 bytes.  **Complete.**
 *
 *      token == 0:      token := 0x572, park 1
 *
 *      token == 0x572:  next_anirate(obj)
 *                       ((void (*)(MK3OBJ *))obj->field48)(obj)
 *                       if (obj->field5c != 0)
 *                           pop a level, or t_local_reaction_exit
 *                       frame[frame].handler = t_animate_till_a11
 *
 *      otherwise:       return -3
 *
 * **0x48 holds a PREDICATE and this routine calls it through the register.**
 * `ldr r3, [r5, #0x48]; blx r3` -- so the caller puts a function there, and every frame
 * this steps the animation rate, calls it, and looks at 0x5c. A set answer ends the
 * loop; a clear one goes round again.
 *
 * **That is what the four `q_bat_*` routines are for.** They take an object, ask a
 * distance question, and answer in 0x5c through `q_yes` / `q_no` -- exactly the shape
 * this expects. So an animality animates until whichever predicate the caller chose
 * says yes, and the predicate is a parameter rather than a branch.
 *
 * The loop reinstalls ITSELF, which restarts it at state 0 and its one-frame park --
 * the same shape mkstat.c's `t_shake_suspended` and `t_noogy_suspended` use, and the
 * same warning applies: this is not "resume at 0x572".
 *
 * 0x48 is a counter in most of this module, a ring pointer in `t_combo_air_pause`, and
 * a function pointer here. The state decides, not the field.
 */
long t_animate_till_a11(MK3THREAD *thread);

long t_animate_till_a11(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x572;
        thread->fieldfc = 1;
        return 1;
    }

    if (token != 0x572)
        return -3;

    next_anirate(obj);

    ((void (*)(MK3OBJ *))(void *)(uintptr_t)obj->field48)(obj);

    if (obj->field5c != 0) {
        if ((long)thread->frame > 0) {
            thread->frame = thread->frame - 1;
            return 0;
        }
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }

    return mk3_install(thread, (MK3THREADFUNC)t_animate_till_a11);
}

/* ------------------------------------------------------------- t_animate_till_a11_stop
 *
 * armv7 0x000a14f0, 136 bytes.  **Complete.**
 *
 *      token == 0:      token := 0x57b, descend into t_animate_till_a11
 *
 *      token == 0x57b:  stop_me_player(obj)
 *                       pop a level, or t_local_reaction_exit at the bottom
 *
 *      otherwise:       return -3
 *
 * The wrapper: run the loop above and stop the fighter when it ends. Nothing else --
 * so a caller that wants the animation to leave the fighter moving installs
 * `t_animate_till_a11` and one that wants it halted installs this.
 */
void stop_me_player(MK3OBJ *obj);

long t_animate_till_a11_stop(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x57b;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_animate_till_a11;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x57b)
        return -3;

    stop_me_player(obj);

    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
        return 0;
    }

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}


/* ---------------------------------------------------------------------- t_do_animality
 *
 * armv7 0x000a0f0c, 132 bytes.  **Complete.**
 *
 *      token == 0:      token := 0x8fb, descend into t_animality_start_pause
 *
 *      token == 0x8fb:  init_special(obj)
 *                       h = ochar_animalities[obj->field08->field24]
 *                       obj->field1c = h
 *                       frame[frame].handler = h
 *
 *      otherwise:       return -3
 *
 * **The entry point for every animality in this file, and the table is the whole
 * routine.** `ochar_animalities` is words indexed by the character number, and whatever
 * it names is installed directly -- so the twenty-odd `tl_*` routines in this module are
 * reached from here and nowhere else.
 *
 * **This is the handler DoASpecial reaches through pointer slot 0x000f31ac.** So the
 * path from a player's input to a character's animality is: DoASpecial's finisher
 * dispatch picks `t_do_animality`, that pauses, then indexes this table. Three tables in
 * three files, each indexed by the character, and this is the last of them.
 *
 * The same shape as `t_do_body_slam` and `t_do_air_slam` in mkslam.c -- one lookup and
 * one install -- except those two return early on a zero entry and this one does not
 * check, so every character must have an entry.
 */
extern uint32_t ochar_animalities[];             /* 0x0017758c */
void init_special(MK3OBJ *obj);
long t_animality_start_pause(MK3THREAD *thread);

long t_do_animality(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    uint32_t h;

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x8fb;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_animality_start_pause;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x8fb)
        return -3;

    init_special(obj);

    h = ochar_animalities[obj->field08->field24];
    obj->field1c = h;

    mk3_frame(thread, thread->frame)[1] = h;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}

/* ---------------------------------------------------------------------- t_animal_morph
 *
 * armv7 0x000a0ff8, 240 bytes.  **Complete.**
 *
 *      token == 0:      token := 0x840, park 0x20
 *
 *      token == 0x840:  do_next_a9_frame(obj)
 *                       token := 0x848, park 0x10
 *
 *      token == 0x848:  tsound_func(obj, 0x27)
 *                       obj->field1c = 5
 *                       token := 0x84c, descend into t_mframew
 *
 *      token == 0x84c:  obj->field1c = obj->a10
 *                       token := 0x84f, park obj->field1c
 *
 *      token == 0x84f:  pop a level, or t_local_reaction_exit at the bottom
 *
 *      otherwise:       return -3
 *
 * The transformation itself: thirty-two frames, one hand-advanced frame, sixteen more,
 * the morph sound and a five-frame wait, and then **a final wait the caller chooses** --
 * 0x44 is read into 0x1c and used as the park, so each animal can hold its new shape for
 * a different length before the thread unwinds.
 *
 * State 0 does no work at all beyond setting the token, which is the same
 * get-onto-the-cadence pattern `t_next_anirate_a10` opens with.
 */
long do_next_a9_frame(MK3OBJ *obj);

long t_animal_morph(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x840;
        thread->fieldfc = 0x20;
        return 0x20;
    }

    if (token == 0x840) {
        do_next_a9_frame(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x848;
        thread->fieldfc = 0x10;
        return 0x10;
    }

    if (token == 0x848) {
        tsound_func(obj, 0x27);
        obj->field1c = 5;

        *mk3_frame(thread, thread->frame + 1) = 0x84c;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x84c) {
        obj->field1c = obj->a10;
        *mk3_frame(thread, thread->frame + 1) = 0x84f;
        thread->fieldfc = obj->field1c;
        return (long)obj->field1c;
    }

    if (token != 0x84f)
        return -3;

    if ((long)thread->frame > 0) {
        thread->frame = thread->frame - 1;
        return 0;
    }

    return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
}

/* -------------------------------------------------------------- t_animality_start_pause
 *
 * armv7 0x000a0e64, 168 bytes.  **Complete.**
 *
 *      token == 0:      obj->field20 = 3
 *                       token := 0x8ec, descend into t_init_death_blow
 *
 *      token == 0x8ec:  token := 0x8ed, park 0x1e
 *
 *      token == 0x8ed:  pop a level, or t_local_reaction_exit at the bottom
 *
 *      otherwise:       return -3
 *
 * **The twin of mkstat.c's `t_baby_start_pause`, and reading the two together says what
 * 0x20 is for.** Both write a small constant into 0x20 and then descend into the same
 * `t_init_death_blow` off pointer slot 0x000f3194: this one writes 3, the babality one
 * writes 5. So **the death-blow routine is shared by every finisher kind and 0x20 is the
 * kind selector** -- the routine is not per-fatality, the number is.
 *
 * After the death blow this waits thirty frames and unwinds. `t_do_animality` descends
 * into this and then indexes `ochar_animalities`, so the wait is what separates the
 * killing blow from the animal appearing.
 *
 * The babality version poses animation zero by hand in its middle state where this one
 * only waits, which is the whole difference between the two beyond the constant.
 */
long t_init_death_blow(MK3THREAD *thread);        /* pointer slot 0x000f3194 */

long t_animality_start_pause(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0x8ec) {
        *mk3_frame(thread, frame + 1) = 0x8ed;
        thread->fieldfc = 0x1e;
        return 0x1e;
    }

    if (token == 0x8ed) {
        if ((long)frame > 0) {
            thread->frame = frame - 1;
            return 0;
        }
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }

    if (token != 0)
        return -3;

    obj->field20 = 3;

    *mk3_frame(thread, thread->frame + 1) = 0x8ec;
    thread->frame = thread->frame + 1;              /* push a level */
    mk3_frame(thread, thread->frame)[1] =
        (uint32_t)(uintptr_t)t_init_death_blow;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}

/* --------------------------------------------------------------- t_cute_animality_start
 *
 * armv7 0x000a117c, 164 bytes.  **Complete.**
 *
 *      token == 0:      face_opponent(obj)
 *                       animality_tune(obj)
 *                       token := 0x3c0, park 0x20
 *
 *      token == 0x3c0:  tsound_func(obj, 0x8c)
 *                       obj->a10 = obj->field3c
 *                       obj->field1c = 0x1e
 *                       create_fx(obj)
 *                       token := 0x3c5, park 8
 *
 *      token == 0x3c5:  pop a level, or t_local_reaction_exit at the bottom
 *
 *      otherwise:       return -3
 *
 * **The other opening**, used by the animalities that appear in a puff rather than by
 * morphing: turn to face the opponent, start the animality music, wait thirty-two
 * frames, then a sound, effect 0x1e, and eight more frames.
 *
 * `obj->a10 = obj->field3c` is copied immediately before `create_fx`, so 0x44 is an
 * argument to the effect and 0x3c is where the caller left it. `create_fx_for_him`
 * earlier in this file shows the other half of that interface -- 0x00 and 0x08 select
 * whose effect it is, 0x1c its kind, 0x44 its parameter.
 *
 * This does NOT go through `t_init_death_blow` the way `t_animality_start_pause` does, so
 * the two openings are alternatives and not stages: one kills first, this one does not.
 */
long t_cute_animality_start(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0x3c0) {
        tsound_func(obj, 0x8c);

        obj->a10 = obj->field3c;
        obj->field1c = 0x1e;
        create_fx(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x3c5;
        thread->fieldfc = 8;
        return 8;
    }

    if (token == 0x3c5) {
        if ((long)frame > 0) {
            thread->frame = frame - 1;
            return 0;
        }
        return mk3_install(thread, (MK3THREADFUNC)t_local_reaction_exit);
    }

    if (token != 0)
        return -3;

    face_opponent(obj);
    animality_tune(obj);

    *mk3_frame(thread, thread->frame + 1) = 0x3c0;
    thread->fieldfc = 0x20;
    return 0x20;
}

/* ------------------------------------------------------------------------ t_r_bat_bite
 *
 * armv7 0x000a2adc, 116 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field48 = 0x00060008; shake_a11(obj)
 *      rsnd_func(obj, 3)
 *      obj->field1c = 1; create_blood_proc(obj)
 *      cutup_body_init(obj, 0x1ab8)
 *      obj->field1c = 0x10000
 *      obj->field20 = 0x10000 - 0x30000 = -0x20000
 *      frame[frame].handler = t_head_pop_off
 *
 * **The same five steps as `t_r_ermac_upcut`, with every constant changed.** Reading the
 * pair gives the whole parameter set of a decapitation:
 *
 *      routine            shake        sound   cutup delta   x vel     y vel
 *      t_r_ermac_upcut    0x00080008   0xa     0x1af4        0x10000   -0x80000
 *      t_r_bat_bite       0x00060008   3       0x1ab8        0x10000   -0x20000
 *
 * So the two differ in how hard the screen shakes, which noise is made, which set of
 * body pieces is used, and how high the head goes -- and in nothing else. The bat throws
 * the head a quarter as high as the uppercut does, which is what you would expect from a
 * bite rather than a punch.
 *
 * **0x48 here is an asymmetric pair, 6 and 8.** That is a third asymmetric site for the
 * shake halfwords, after mkstat.c's 0x00030008 and 0x0009000e, so the two halves are
 * definitely independent and this is not a doubled constant.
 */
long t_r_bat_bite(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field48 = 0x00060008;
    shake_a11(obj);
    rsnd_func(obj, 3);

    obj->field1c = 1;
    create_blood_proc(obj);

    cutup_body_init(obj, 0x1ab8);

    obj->field1c = 0x10000;
    obj->field20 = (uint32_t)(0x10000 - 0x30000);

    return mk3_install(thread, (MK3THREADFUNC)t_head_pop_off);
}

/* ---------------------------------------------------------------------- t_stung_a_bunch
 *
 * armv7 0x000a1cac, 128 bytes.  **Complete.**
 *
 *      token == 0:      token := 0x662, park 8
 *
 *      token == 0x662:  death_scream(obj)
 *                       -- falls through --
 *
 *      token == 0x666:  obj->field40 = 0x0003001c
 *                       token := 0x666, descend into t_animate_a9
 *
 *      otherwise:       return -3
 *
 * **A loop with no exit.** The 0x666 state sets the token back to 0x666 before descending,
 * so when the child animation finishes and pops, this state runs again, sets 0x40 again,
 * and descends again -- forever, until something outside the thread replaces the handler.
 * Which is the correct behaviour for a scorpion animality: the stinging does not stop, the
 * round ending is what stops it.
 *
 * **0x662 falls through into 0x666 rather than branching to its own tail**, so the scream
 * happens once and the animation from then on is identical. One `beq` to 0xa1cde and the
 * next instruction after the call is the 0x666 entry at 0xa1ce4.
 *
 * 0x40 is the packed halfword pair again -- animation 0x1c at rate 3 -- handed to
 * `t_animate_a9` off pointer slot 0x000f36d0. Seventh site for that pair, and the first
 * one in this module.
 */
long t_animate_a9(MK3THREAD *thread);           /* pointer slot 0x000f36d0 */

long t_stung_a_bunch(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0) {
        *mk3_frame(thread, frame + 1) = 0x662;
        thread->fieldfc = 8;
        return 8;
    }

    if (token == 0x662) {
        death_scream(obj);

    } else if (token != 0x666) {
        return -3;
    }

    obj->field40 = 0x0003001c;

    *mk3_frame(thread, thread->frame + 1) = 0x666;
    thread->frame = thread->frame + 1;              /* push a level */
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_animate_a9;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}

/* ----------------------------------------------------------------------- t_bit_in_half
 *
 * armv7 0x000a29e0, 136 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field48 = 0x00060006; shake_a11(obj)
 *      death_scream(obj)
 *      rsnd_func(obj, 3)
 *      obj->field1c = 8; create_blood_proc(obj)
 *      p = &lao_ani_data[0x142c + obj->field08->field24 * 4]
 *      obj->field1c = p
 *      obj->field40 = p
 *      do_next_a9_frame(obj)
 *      frame[frame].handler = t_wait_forever
 *
 * **The third decapitation-shaped reaction, and the one that does not use
 * `cutup_body_init`.** Instead of handing the body to the pieces machinery it computes an
 * ADDRESS -- 0x142c into `lao_ani_data`, indexed by the character number -- and puts it in
 * both 0x1c and 0x40, then advances one frame by hand and waits forever.
 *
 * 0x142c is an offset INSIDE `lao_ani_data` (the symbol runs from 0x00155200 to the next
 * one, `lia_ani_data`, at 0x001567a0), so the table this indexes has no symbol of its own.
 * The computed address is 0x0015662c + char*4.
 *
 * **That confirms the pointer reading of 0x40**: `do_next_a9_frame` dereferences it twice
 * and steps it by four, so what goes in is the address of a per-character word list, not
 * an animation number. Same field, and `t_stung_a_bunch` two functions up puts a packed
 * halfword pair in it -- the caller and the callee agree, the field does not care.
 *
 * The shake pair is 0x00060006, doubled, where `t_r_bat_bite` uses 0x00060008. So the bite
 * that cuts a fighter in half shakes less than the bite that takes the head off.
 */
extern uint8_t lao_ani_data[];                   /* 0x00155200, pointer slot 0x000f3460 */

long t_bit_in_half(MK3THREAD *thread)
{
    MK3OBJ   *obj = (MK3OBJ *)thread->proc;
    uint32_t  p;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field48 = 0x00060006;
    shake_a11(obj);
    death_scream(obj);
    rsnd_func(obj, 3);

    obj->field1c = 8;
    create_blood_proc(obj);

    p = (uint32_t)(uintptr_t)&lao_ani_data[0x142c + obj->field08->field24 * 4];
    obj->field1c = p;
    obj->field40 = p;
    do_next_a9_frame(obj);

    return mk3_install(thread, (MK3THREADFUNC)t_wait_forever);
}

/* --------------------------------------------------------------------- t_eaten_by_shark
 *
 * armv7 0x000a1d44, 152 bytes.  **Complete.**
 *
 *      token == 0:      death_scream(obj)
 *                       obj->field1c = 0x20; create_fx(obj)
 *                       token := 0x453, park 2
 *
 *      token == 0x453:  obj->field1c = 0x20; create_fx(obj)
 *                       token := 0x456, park 2
 *
 *      token == 0x456:  obj->field1c = 0x20; create_fx(obj)
 *                       frame[frame].handler = t_eaten_by_snake
 *
 *      otherwise:       return -3
 *
 * **Effect 0x20 three times, two frames apart, and then it becomes a different routine.**
 * The three states are identical except for what they do next, so the bites are one
 * repeated event rather than three different ones -- but they are written out three times
 * with three tokens instead of counting, because the third one has to install rather than
 * park.
 *
 * **The install target is `t_eaten_by_snake`**, taken as a direct pc-relative address
 * rather than through a pointer slot. So the shark reaction runs the snake reaction's tail
 * -- the swallowing is shared and only the three bites are the shark's. That is worth
 * knowing before writing `t_eaten_by_snake`: it has to work as both an entry point and a
 * continuation.
 *
 * Only state 0 screams. The two later effects are silent.
 */
long t_eaten_by_snake(MK3THREAD *thread);        /* 0x000a2f60 */

long t_eaten_by_shark(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0x453) {
        obj->field1c = 0x20;
        create_fx(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x456;
        thread->fieldfc = 2;
        return 2;
    }

    if (token == 0x456) {
        obj->field1c = 0x20;
        create_fx(obj);

        return mk3_install(thread, (MK3THREADFUNC)t_eaten_by_snake);
    }

    if (token != 0)
        return -3;

    death_scream(obj);

    obj->field1c = 0x20;
    create_fx(obj);

    *mk3_frame(thread, thread->frame + 1) = 0x453;
    thread->fieldfc = 2;
    return 2;
}

/* ------------------------------------------------------------------------- t_egg_proc
 *
 * armv7 0x000a1440, 176 bytes.  **Complete.**
 *
 *      token == 0:      obj->field08->field2c = 0xb08
 *                       obj->field40 = a_egg
 *                       obj->field1c = 5
 *                       token := 0x2a8, descend into t_mframew
 *
 *      token == 0x2a8:  token := 0x2aa, park 0x20
 *
 *      token == 0x2aa:  obj->field38 = t_r_egg
 *                       takeover_him(obj)
 *                       token := 0x2ad, return 0x16462 (delete)
 *
 *      otherwise:       return -3
 *
 * **The egg's own thread.** It sets the part's animation to 0xb08, points 0x40 at the
 * named word list `a_egg` (0x00177558 -- one of the few of these tables with a symbol of
 * its own), waits five frames' worth through `t_mframew`, waits thirty-two more, and then
 * hands `t_r_egg` to the other fighter through 0x38 and `takeover_him`.
 *
 * So the egg does not hatch by running code of its own: it makes the VICTIM's thread run
 * `t_r_egg`, which is written earlier in this file, and then parks itself forever.
 *
 * **Token 0x2ad is not in the dispatch and the return is the 0x16462 delete sentinel.** Third site for that
 * pattern, after mkstat.c's `t_jade_flash_proc` and `t_crunch_sounds` in this file -- and
 * the first one where the reason is plainly visible: after the handover this thread has
 * nothing left to do, and parking forever is cheaper than unwinding.
 */
extern uint32_t a_egg[];                         /* 0x00177558 */
void takeover_him(MK3OBJ *obj);

long t_egg_proc(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0x2a8) {
        *mk3_frame(thread, frame + 1) = 0x2aa;
        thread->fieldfc = 0x20;
        return 0x20;
    }

    if (token == 0x2aa) {
        obj->field38 = (uint32_t)(uintptr_t)t_r_egg;
        takeover_him(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x2ad;
        thread->fieldfc = 0x16462;
        return 0x16462;
    }

    if (token != 0)
        return -3;

    obj->field08->field2c = 0xb08;
    obj->field40 = (uint32_t)(uintptr_t)a_egg;
    obj->field1c = 5;

    *mk3_frame(thread, thread->frame + 1) = 0x2a8;
    thread->frame = thread->frame + 1;              /* push a level */
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}

/* ----------------------------------------------------------------------- t_hit_by_bull
 *
 * armv7 0x000a3284, 208 bytes.  **Complete.**
 *
 *      token == 0:      obj->field1c = 1; create_blood_proc(obj)
 *                       obj->field48 = 0x00060006; shake_a11(obj)
 *                       death_scream(obj)
 *                       rsnd_func(obj, 3)
 *                       set_noedge(obj)
 *                       obj->field1c = 0x130000; away_x_vel(obj)
 *                       obj->field08->field18 = obj->field1c
 *                       obj->field1c = 0xd
 *                       obj->field20 = 0xfff80000
 *                       obj->field24 = 0xfff80000 + 0x86000 = 0x00006000
 *                       obj->field28 = 4
 *                       obj->field40 = 0x1e
 *                       token := 0x632, descend into t_flight
 *
 *      token == 0x632:  frame[frame].handler = t_land_on_my_back
 *
 *      otherwise:       return -3
 *
 * **The twin of `t_dino_bucked` earlier in this file**, and reading the two together
 * separates the fixed part of a knock-into-the-air from the per-animal part:
 *
 *      routine           x vel      0x20 (y vel)   0x24 (fall)   0x28   0x40
 *      t_dino_bucked     0x30000    0xffeb0000     0x5000        4      0x1e
 *      t_hit_by_bull     0xd        0xfff80000     0x6000        4      0x1e
 *
 * The blood, the shake pair, the scream and sound 3 are identical; the bounce kind and the
 * animation are identical; only the three velocities differ. So `t_flight` takes five
 * numbers and everything else is shared.
 *
 * **The 0x24 store wraps 32 bits and that is not a transcription slip.** The literal is
 * 0xfff80000, the instruction is `add.w r3, r3, #0x86000`, and the sum 0x100006000 is
 * truncated by the register to 0x00006000. Writing it out as an addition keeps the binary's
 * arithmetic; writing 0x6000 directly would hide that 0x20 and 0x24 are computed from one
 * literal and not two.
 *
 * The bull is also the only one of the pair to call `set_noedge` and `away_x_vel`, and to
 * copy the resulting x velocity into the part's 0x18 -- so the victim is pushed away from
 * the bull rather than in a fixed direction, and the screen is allowed to let them leave.
 */
void set_noedge(MK3OBJ *obj);
void away_x_vel(MK3OBJ *obj);

long t_hit_by_bull(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field1c = 1;
        create_blood_proc(obj);

        obj->field48 = 0x00060006;
        shake_a11(obj);
        death_scream(obj);
        rsnd_func(obj, 3);
        set_noedge(obj);

        obj->field1c = 0x130000;
        away_x_vel(obj);
        obj->field08->field18 = obj->field1c;

        obj->field1c = 0xd;
        obj->field20 = 0xfff80000u;
        obj->field24 = 0xfff80000u + 0x86000u;      /* wraps to 0x00006000 */
        obj->field28 = 4;
        obj->field40 = 0x1e;

        *mk3_frame(thread, thread->frame + 1) = 0x632;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_flight;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0x632)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_land_on_my_back);
}

/* -------------------------------------------------------------------------- t_r_rabbit
 *
 * armv7 0x000a3c44, 220 bytes.  **Complete.**
 *
 *      token == 0:      obj->field40 = 0x20; find_ani_part2(obj)
 *                       obj->field1c = 4; init_anirate(obj)
 *                       set_noedge(obj)
 *                       death_scream(obj)
 *                       face_opponent(obj)
 *                       obj->field1c = 0x30000; away_x_vel(obj)
 *                       NewThread(obj, t_crunch_sounds)
 *                       obj->a10 = 0x140
 *                       obj->field1c = 1
 *                       -- falls through to the tail --
 *
 *      token == 0x3eb:  next_anirate(obj)
 *                       obj->field1c = obj->field00->field28 - 1
 *                       if (obj->field1c == 0) {
 *                           obj->field1c = 5; create_blood_proc(obj)
 *                           obj->field1c = 5
 *                       }
 *                       if (--obj->a10 <= 0) {
 *                           stop_me_player(obj)
 *                           frame[frame].handler = t_wait_forever
 *                       }
 *                       -- falls through to the tail --
 *
 *      the tail:        obj->field00->field28 = obj->field1c
 *                       token := 0x3eb, park 1
 *
 *      otherwise:       return -3
 *
 * **Two counters that work differently, and one shared store.** `obj->a10` counts 0x140 --
 * three hundred and twenty frames -- straight down, and ends the routine. `proc->field28`
 * counts down too, but the tail RELOADS it from 0x1c every frame, so what 0x1c holds is
 * the reload value: state 0 leaves 1 there, so the first pass of 0x3eb sees zero and draws
 * blood, and each blood then sets 0x1c to 5, so blood comes every fifth frame after that.
 * A repeating interval built out of one countdown and one store, with no second field.
 *
 * **This is a second reading of `proc->field28`.** The header describes it as who the shake
 * is about, on the authority of the two shake routines that write `him` or the object's
 * 0x08 there. Here it is a frame counter. Nothing in this routine settles which reading is
 * the field's real purpose, so both stand; what is certain is that this one writes and
 * reads it as a number of frames.
 *
 * `create_blood_proc` clobbers 0x1c, which is why 5 is stored twice around the call -- once
 * as the effect's own argument and once to survive it. Not dead code.
 *
 * **The crunching is a separate thread.** `NewThread(obj, t_crunch_sounds)` starts the
 * routine written earlier in this file, so the six pairs of crunches run on their own
 * cadence while this loop draws blood on its own. Two independent timelines rather than one
 * interleaved state machine -- and it explains why `t_crunch_sounds` needed a never-wake
 * terminator instead of a way to unwind.
 */
void find_ani_part2(MK3OBJ *obj);
void init_anirate(MK3OBJ *obj);
MK3THREAD *NewThread(void *owner, MK3THREADFUNC func);

long t_r_rabbit(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field40 = 0x20;
        find_ani_part2(obj);

        obj->field1c = 4;
        init_anirate(obj);
        set_noedge(obj);
        death_scream(obj);
        face_opponent(obj);

        obj->field1c = 0x30000;
        away_x_vel(obj);

        NewThread(obj, (MK3THREADFUNC)t_crunch_sounds);

        obj->a10 = 0x140;
        obj->field1c = 1;

    } else if (token == 0x3eb) {
        next_anirate(obj);

        obj->field1c = obj->field00->field28 - 1;
        if (obj->field1c == 0) {
            obj->field1c = 5;
            create_blood_proc(obj);
            obj->field1c = 5;
        }

        obj->a10 = obj->a10 - 1;
        if ((long)obj->a10 <= 0) {
            stop_me_player(obj);
            return mk3_install(thread, (MK3THREADFUNC)t_wait_forever);
        }

    } else {
        return -3;
    }

    obj->field00->field28 = obj->field1c;

    *mk3_frame(thread, thread->frame + 1) = 0x3eb;
    thread->fieldfc = 1;
    return 1;
}

/* ------------------------------------------------------------------ t_stung_by_scorpion
 *
 * armv7 0x000a361c, 292 bytes.  **Complete.**
 *
 *      token == 0:      face_opponent(obj)
 *                       obj->field1c = 0x40000; away_x_vel(obj)
 *                       rsnd_func(obj, 3)
 *                       obj->field1c = 2; group_sound(obj)
 *                       obj->field40 = 0x00050020
 *                       token := 0x4fa, descend into t_animate_a9
 *
 *      token == 0x4fa:  stop_me_player(obj)
 *                       death_scream(obj)
 *                       obj->field40 = 0x48; pose_a9_manual(obj)
 *                       player_swpal(obj, 3)
 *                       obj->field1c = 3
 *                       obj->field20 = 3
 *                       obj->field24 = 3 + 0x11 = 0x14
 *                       token := 0x508, descend into t_shake_ob_up
 *
 *      token == 0x508:  set_inviso(obj)
 *                       obj->field1c = 0x15; create_fx(obj)
 *                       frame[frame].handler = t_wait_forever
 *
 *      otherwise:       return -3
 *
 * **The victim dissolves rather than coming apart**, which is why nothing in here touches
 * the blood or the body-pieces machinery. Three stages: knocked away with an animation,
 * then poisoned -- posed by hand into animation 0x48 with palette 3 -- and shaken in place,
 * and finally made invisible with effect 0x15 left where the body was.
 *
 * `player_swpal(obj, 3)` is the poison colour. It is the same call `t_r_egg` uses, so
 * swapping the palette is how this module shows a state change on a fighter it is not
 * animating.
 *
 * **`t_shake_ob_up` takes three numbers**, 0x1c, 0x20 and 0x24, and the third is computed
 * from the second with `adds r3, #0x11` rather than loaded -- so 3 and 0x14 come from one
 * literal. The same one-literal-two-fields shape as `t_hit_by_bull`'s flight constants.
 *
 * 0x40 is the packed halfword pair again -- animation 0x20 at rate 5 -- eighth site for
 * that reading, and the second in this file after `t_stung_a_bunch`.
 */
void group_sound(MK3OBJ *obj);
void pose_a9_manual(MK3OBJ *obj);
void player_swpal(MK3OBJ *obj, uint32_t frozen);
long t_shake_ob_up(MK3THREAD *thread);           /* pointer slot 0x000f36f8 */

long t_stung_by_scorpion(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x4fa) {
        stop_me_player(obj);
        death_scream(obj);

        obj->field40 = 0x48;
        pose_a9_manual(obj);
        player_swpal(obj, 3);

        obj->field1c = 3;
        obj->field20 = 3;
        obj->field24 = 3 + 0x11;

        *mk3_frame(thread, thread->frame + 1) = 0x508;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_shake_ob_up;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x508) {
        set_inviso(obj);

        obj->field1c = 0x15;
        create_fx(obj);

        return mk3_install(thread, (MK3THREADFUNC)t_wait_forever);
    }

    if (token != 0)
        return -3;

    face_opponent(obj);

    obj->field1c = 0x40000;
    away_x_vel(obj);
    rsnd_func(obj, 3);

    obj->field1c = 2;
    group_sound(obj);

    obj->field40 = 0x00050020;

    *mk3_frame(thread, thread->frame + 1) = 0x4fa;
    thread->frame = thread->frame + 1;              /* push a level */
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_animate_a9;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}

/* ----------------------------------------------------------------- t_r_scared_of_monkey
 *
 * armv7 0x000a3fe8, 304 bytes.  **Complete.**
 *
 *      token == 0:      center_around_me(obj)
 *                       death_scream(obj)
 *                       face_opponent(obj)
 *                       obj->field40 = 0x48; pose_a9_manual(obj)
 *                       obj->field1c = 3
 *                       obj->field20 = 3
 *                       obj->field24 = 3 + 5 = 8
 *                       token := 0x318, descend into t_shake_ob_up
 *
 *      token == 0x318:  token := 0x319, park 0x14
 *
 *      token == 0x319:  flip_multi(obj)
 *                       kill_and_stop_scrolling(obj)
 *                       sans_repell_for_good(obj)
 *                       no_edge_both_players()
 *                       obj->field1c = 0x80000; away_x_vel(obj)
 *                       obj->field40 = 0x46; get_char_ani(obj)
 *                       obj->field1c = 3; init_anirate(obj)
 *                       obj->a10 = 0x50
 *                       -- falls through to the tail --
 *
 *      token == 0x329:  next_anirate(obj)
 *                       if (--obj->a10 <= 0) {
 *                           stop_me_player(obj)
 *                           frame[frame].handler = t_wait_forever
 *                       }
 *                       -- falls through to the tail --
 *
 *      the tail:        token := 0x329, park 1
 *
 *      otherwise:       return -3
 *
 * **The fighter runs away.** Posed into animation 0x48 and shaken, a twenty-frame pause,
 * and then the whole arena is unlocked at once -- the camera stops following, the two
 * fighters stop repelling each other, and both have their edge limits removed -- before the
 * fighter is thrown away from the monkey at 0x80000 and animates for eighty frames.
 *
 * **`no_edge_both_players` takes no argument.** other.c defines it as `(void)`, reading
 * both fighters out of the global; the `mov r0, r4` before the call is a setup the callee
 * ignores. Transcribing it as a one-argument call would invent an interface.
 *
 * **The same `t_shake_ob_up` call as `t_stung_by_scorpion`, with one number changed**:
 * both write 3 into 0x1c and 0x20, and the third field comes from the same register with
 * `adds r3, #5` here and `adds r3, #0x11` there. So 0x24 is the only thing that varies
 * between the two shakes, 8 against 0x14, and one literal still feeds two fields.
 *
 * Animation 0x48 is posed by hand in both routines as well -- the same frightened pose is
 * shared by the monkey and the scorpion, and only what follows it differs.
 */
void center_around_me(MK3OBJ *obj);
void flip_multi(MK3OBJ *obj);
void sans_repell_for_good(MK3OBJ *obj);
void no_edge_both_players(void);
void get_char_ani(MK3OBJ *obj);

long t_r_scared_of_monkey(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0x318) {
        *mk3_frame(thread, frame + 1) = 0x319;
        thread->fieldfc = 0x14;
        return 0x14;
    }

    if (token == 0x319) {
        flip_multi(obj);
        kill_and_stop_scrolling(obj);
        sans_repell_for_good(obj);
        no_edge_both_players();

        obj->field1c = 0x80000;
        away_x_vel(obj);

        obj->field40 = 0x46;
        get_char_ani(obj);

        obj->field1c = 3;
        init_anirate(obj);

        obj->a10 = 0x50;

    } else if (token == 0x329) {
        next_anirate(obj);

        obj->a10 = obj->a10 - 1;
        if ((long)obj->a10 <= 0) {
            stop_me_player(obj);
            return mk3_install(thread, (MK3THREADFUNC)t_wait_forever);
        }

    } else if (token == 0) {
        center_around_me(obj);
        death_scream(obj);
        face_opponent(obj);

        obj->field40 = 0x48;
        pose_a9_manual(obj);

        obj->field1c = 3;
        obj->field20 = 3;
        obj->field24 = 3 + 5;

        *mk3_frame(thread, thread->frame + 1) = 0x318;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_shake_ob_up;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;

    } else {
        return -3;
    }

    *mk3_frame(thread, thread->frame + 1) = 0x329;
    thread->fieldfc = 1;
    return 1;
}

/* ------------------------------------------------------------------ tl_sheeva_scorpion
 *
 * armv7 0x000a1334, 268 bytes.  **Complete.**
 *
 *      token == 0:      animality_tune(obj)
 *                       obj->field40 = a_scorpion
 *                       obj->a10 = 0x12
 *                       token := 0x514, descend into t_animal_morph
 *
 *      token == 0x514:  obj->field1c = 5
 *                       token := 0x517, descend into t_mframew
 *
 *      token == 0x517:  obj->field38 = t_stung_by_scorpion
 *                       takeover_him(obj)
 *                       token := 0x51b, park 0x80
 *
 *      token == 0x51b:  obj->field40 = a_scorpion
 *                       frame[frame].handler = t_animality_complete
 *
 *      otherwise:       return -3
 *
 * **The first of the twenty per-character animality drivers, and the template for all of
 * them.** `t_do_animality` indexes `ochar_animalities` by the character number and installs
 * one of these; each one is four states long and every state is a call to something already
 * written:
 *
 *      state 0     start the music, point 0x40 at the animal's word list, put the morph
 *                  length in 0x44, and descend into t_animal_morph
 *      state 1     descend into t_mframew for a fixed number of frames
 *      state 2     hand the victim's reaction to the other fighter through 0x38 and
 *                  takeover_him, then park while it plays
 *      state 3     point 0x40 at the word list again and install t_animality_complete
 *
 * So a driver contributes **four numbers and two names**: the word list (`a_scorpion` at
 * 0x00177434), the morph length (0x12), the wait after the morph (5), the park while the
 * victim dies (0x80), and the two routines -- the morph and the victim's reaction
 * (`t_stung_by_scorpion`, written above).
 *
 * **0x40 is written twice with the same value**, once before the morph and once before the
 * finish. Not redundant: `t_animal_morph` walks 0x40 as a cursor, so by the time state 3
 * runs it has been advanced to the end of the list and has to be reset for
 * `t_animality_complete` to read it.
 *
 * Sheeva's animal is a scorpion, and the victim's reaction is the one written earlier in
 * this file -- so `tl_sheeva_scorpion` and `t_stung_by_scorpion` are the attacker's and the
 * victim's halves of the same finisher, and neither is complete without the other.
 */
extern uint32_t a_scorpion[];                    /* 0x00177434 */

long tl_sheeva_scorpion(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x514) {
        obj->field1c = 5;

        *mk3_frame(thread, thread->frame + 1) = 0x517;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x517) {
        obj->field38 = (uint32_t)(uintptr_t)t_stung_by_scorpion;
        takeover_him(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x51b;
        thread->fieldfc = 0x80;
        return 0x80;
    }

    if (token == 0x51b) {
        obj->field40 = (uint32_t)(uintptr_t)a_scorpion;

        return mk3_install(thread, (MK3THREADFUNC)t_animality_complete);
    }

    if (token != 0)
        return -3;

    animality_tune(obj);

    obj->field40 = (uint32_t)(uintptr_t)a_scorpion;
    obj->a10 = 0x12;

    *mk3_frame(thread, thread->frame + 1) = 0x514;
    thread->frame = thread->frame + 1;              /* push a level */
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_animal_morph;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}

/* ------------------------------------------------------------------- tl_reptile_monkey
 *
 * armv7 0x000a3eac, 316 bytes.  **Complete.**
 *
 *      token == 0:      token := 0x333, descend into t_cute_animality_start
 *
 *      token == 0x333:  obj->field08->field2c = 0xedd
 *                       mk3_getbbox(part->field2c, &part->field34, &part->field38,
 *                                   &part->field3c, &part->field40)
 *                       part->y12 = *(long *)(G + 0xac)
 *                                   - (part->field40 - part->field38)
 *                       obj->field38 = t_r_scared_of_monkey
 *                       takeover_him(obj)
 *                       token := 0x34f, park 0x50
 *
 *      token == 0x34f:  obj->field1c = 0x40000; set_vel_flip(obj)
 *                       obj->field40 = a_monkey
 *                       obj->a10 = 0x14
 *                       -- falls into the 0x358 tail --
 *
 *      token == 0x358:  if (--obj->a10 <= 0) {
 *                           death_blow_complete(obj)
 *                           -- falls into the 0x360 tail --
 *                       }
 *                       -- falls into the 0x358 tail --
 *
 *      the 0x358 tail:  frame_a9(obj); token := 0x358, park 4
 *      the 0x360 tail:  frame_a9(obj); token := 0x360, park 4
 *
 *      token == 0x360:  the 0x360 tail
 *
 *      otherwise:       return -3
 *
 * **The second driver shape, and it is not the four-state template.** Reptile's monkey
 * opens with `t_cute_animality_start` instead of the morph, places the animal on the floor
 * itself, hands the victim `t_r_scared_of_monkey`, and then animates in two phases: twenty
 * steps at four frames each, and after that **forever**. Nothing installs
 * `t_animality_complete`; the routine calls `death_blow_complete` and keeps stepping.
 *
 * So the family has at least two shapes -- morph-and-finish (`tl_sheeva_scorpion`) and
 * appear-and-never-stop (this one) -- and the shape follows the animal, not the file.
 *
 * **The placement is `ground_ob` written out by hand with a different measurement.** Both
 * put floor-minus-height into the part's 0x12; `ground_ob` gets the height from
 * `GetFrameHeight` and subtracts a nine-pixel inset, and this one measures the bounding box
 * with `mk3_getbbox` and subtracts nothing. The height comes out as bottom minus top --
 * 0x40 minus 0x38 -- which is exactly the box the header documents at 0x34..0x40. Third
 * routine in the tree to do this placement, after `ground_ob` here and
 * `t_turn_into_a_baby` in mkstat.c, and the two measurements have never been reconciled.
 *
 * The four bounding-box fields are passed to `mk3_getbbox` as OUT parameters -- the part's
 * own 0x34, 0x38, 0x3c and 0x40 are where the box is written, the fifth going through the
 * stack -- so the routine measures the animation into the part and then reads two of the
 * four back.
 */
extern uint32_t a_monkey[];                      /* 0x001771fc */
void mk3_getbbox(long ani, int *p1, int *p2, int *p3, int *p4);
void frame_a9(MK3OBJ *obj);

long tl_reptile_monkey(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0) {
        *mk3_frame(thread, frame + 1) = 0x333;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_cute_animality_start;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x333) {
        obj->field08->field2c = 0xedd;

        mk3_getbbox(obj->field08->field2c,
                    (int *)&obj->field08->field34,
                    (int *)&obj->field08->field38,
                    (int *)&obj->field08->field3c,
                    (int *)&obj->field08->field40);

        MK3_SET_FIELD12(obj->field08,
                        *(uint32_t *)(G_BYTES + 0xac)
                        - (obj->field08->field40 - obj->field08->field38));

        obj->field38 = (uint32_t)(uintptr_t)t_r_scared_of_monkey;
        takeover_him(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x34f;
        thread->fieldfc = 0x50;
        return 0x50;
    }

    if (token == 0x34f || token == 0x358) {
        if (token == 0x34f) {
            obj->field1c = 0x40000;
            set_vel_flip(obj);

            obj->field40 = (uint32_t)(uintptr_t)a_monkey;
            obj->a10 = 0x14;

        } else {
            obj->a10 = obj->a10 - 1;
            if ((long)obj->a10 <= 0) {
                death_blow_complete(obj);
                goto hold_360;
            }
        }

        frame_a9(obj);
        *mk3_frame(thread, thread->frame + 1) = 0x358;
        thread->fieldfc = 4;
        return 4;
    }

    if (token != 0x360)
        return -3;

    /* One tail in the binary, reached from the countdown's end and from
     * 0x360 itself (`beq 0xa3f88`). */
hold_360:
    frame_a9(obj);
    *mk3_frame(thread, thread->frame + 1) = 0x360;
    thread->fieldfc = 4;
    return 4;
}

/* ----------------------------------------------------------------------- t_lion_mauled
 *
 * armv7 0x000a1ddc, 320 bytes.  **Complete.**
 *
 *      token == 0:               death_scream(obj)
 *                                face_opponent(obj)
 *                                obj->field40 = 0x0003001e
 *                                token := 0x7ab, descend into t_animate_a9
 *
 *      token == 0x7ab:           obj->field1c = 3
 *                                token := 0x7ad, descend into t_mframew
 *
 *      token == 0x7ad or 0x7bf:  obj->field1c = 7; create_blood_proc(obj)
 *                                tsound_func(obj, 0x24)
 *                                obj->field1c = 3
 *                                obj->field20 = 3
 *                                obj->field24 = 3 + 2 = 5
 *                                token := 0x7b6, descend into t_shake_ob_up
 *
 *      token == 0x7b6:           obj->field1c = 7; create_blood_proc(obj)
 *                                death_scream(obj)
 *                                tsound_func(obj, 0x25)
 *                                obj->field1c = 5
 *                                obj->field20 = 5 - 2 = 3
 *                                obj->field24 = 3
 *                                token := 0x7bf, descend into t_shake_ob_up
 *
 *      otherwise:                return -3
 *
 * **Two states that never stop handing off to each other.** 0x7ad sets 0x7b6, and 0x7b6
 * sets 0x7bf -- which the dispatch routes to the same code as 0x7ad. So after the opening
 * the routine alternates between the two mauling states forever: blood and sound 0x24 and
 * a shake, then blood and a scream and sound 0x25 and a shake, and round again.
 *
 * **Two tokens for one state, and the dispatch is what says so.** 0x7ad is reached by a
 * `beq` and 0x7bf by `adds r3, #9; cmp; beq` to the same target, so writing them as separate
 * states would invent a difference. This is the mirror of the trap recorded in the handoff
 * for `t_sz_slam`: there one register carried two tokens, here two tokens reach one label.
 *
 * **Sounds 0x24 and 0x25 again, and this time they are NOT played together.** In
 * `t_crunch_sounds` they fire back to back as one noise; here they are one state apart, so
 * the pair is two usable samples and playing them together was that routine's choice, not
 * the samples'.
 *
 * The shake numbers alternate too: 3/3/5 on one side and 5/3/3 on the other, each built from
 * one literal with an `adds` or a `subs` -- the same one-literal-several-fields shape as
 * every other `t_shake_ob_up` caller in this file.
 */
long t_lion_mauled(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0x7ad || token == 0x7bf) {
        obj->field1c = 7;
        create_blood_proc(obj);
        tsound_func(obj, 0x24);

        obj->field1c = 3;
        obj->field20 = 3;
        obj->field24 = 3 + 2;

        *mk3_frame(thread, thread->frame + 1) = 0x7b6;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_shake_ob_up;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x7b6) {
        obj->field1c = 7;
        create_blood_proc(obj);
        death_scream(obj);
        tsound_func(obj, 0x25);

        obj->field1c = 5;
        obj->field20 = 5 - 2;
        obj->field24 = 5 - 2;

        *mk3_frame(thread, thread->frame + 1) = 0x7bf;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_shake_ob_up;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x7ab) {
        obj->field1c = 3;

        *mk3_frame(thread, thread->frame + 1) = 0x7ad;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token != 0)
        return -3;

    death_scream(obj);
    face_opponent(obj);

    obj->field40 = 0x0003001e;

    *mk3_frame(thread, thread->frame + 1) = 0x7ab;
    thread->frame = thread->frame + 1;              /* push a level */
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_animate_a9;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}

/* -------------------------------------------------------------------- tl_kabal_skeleton
 *
 * armv7 0x000a1578, 352 bytes.  **Complete.**
 *
 *      token == 0:      animality_tune(obj)
 *                       obj->field40 = a_skeleton
 *                       obj->a10 = 0x12
 *                       token := 0x539, descend into t_animal_morph
 *
 *      token == 0x539:  obj->field1c = 0x80000; towards_x_vel(obj)
 *                       obj->field1c = 4
 *                       token := 0x53e, descend into t_mframew
 *
 *      token == 0x53e:  obj->field38 = t_dino_bucked
 *                       takeover_him(obj)
 *                       stop_me_player(obj)
 *                       token := 0x543, park 0x10
 *
 *      token == 0x543:  obj->field1c = 5
 *                       token := 0x546, descend into t_mframew
 *
 *      token == 0x546:  token := 0x547, park 0x10
 *
 *      token == 0x547:  obj->field40 = a_skeleton
 *                       frame[frame].handler = t_unmorph_and_exit
 *
 *      otherwise:       return -3
 *
 * **The template with a charge in the middle and an unmorph at the end.** Six states where
 * Sheeva's scorpion has four, and the two extra pieces are both plain:
 * `towards_x_vel` at 0x80000 sends the skeleton at the opponent before the hit, and the
 * ending goes through `t_unmorph_and_exit` -- which runs `t_backwards_ani` and only then
 * installs `t_animality_complete` -- instead of installing it directly.
 *
 * **So there are three endings in the family so far**: install
 * `t_animality_complete` (`tl_sheeva_scorpion`), unmorph first and then complete (this one),
 * and never complete at all (`tl_reptile_monkey`). The animal that has to turn back into a
 * fighter is the one that unmorphs; the skeleton is Kabal without his skin, so it does.
 *
 * **The victim's reaction is `t_dino_bucked`**, reused unchanged from the dinosaur. Nothing
 * in this routine adapts it -- the same knock-into-the-air with the same five numbers -- so
 * the reactions in this module are a shared pool and not one per animal. That is the second
 * such reuse, after `t_eaten_by_shark` taking `t_eaten_by_snake` as its tail.
 *
 * 0x40 is set twice with the same list for the same reason as in `tl_sheeva_scorpion`: the
 * morph advances it and the ending has to read it from the start again.
 */
extern uint32_t a_skeleton[];                    /* 0x00177498 */
void towards_x_vel(MK3OBJ *obj);

long tl_kabal_skeleton(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0x539) {
        obj->field1c = 0x80000;
        towards_x_vel(obj);
        obj->field1c = 4;

        *mk3_frame(thread, thread->frame + 1) = 0x53e;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x53e) {
        obj->field38 = (uint32_t)(uintptr_t)t_dino_bucked;
        takeover_him(obj);
        stop_me_player(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x543;
        thread->fieldfc = 0x10;
        return 0x10;
    }

    if (token == 0x543) {
        obj->field1c = 5;

        *mk3_frame(thread, thread->frame + 1) = 0x546;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x546) {
        *mk3_frame(thread, frame + 1) = 0x547;
        thread->fieldfc = 0x10;
        return 0x10;
    }

    if (token == 0x547) {
        obj->field40 = (uint32_t)(uintptr_t)a_skeleton;

        return mk3_install(thread, (MK3THREADFUNC)t_unmorph_and_exit);
    }

    if (token != 0)
        return -3;

    animality_tune(obj);

    obj->field40 = (uint32_t)(uintptr_t)a_skeleton;
    obj->a10 = 0x12;

    *mk3_frame(thread, thread->frame + 1) = 0x539;
    thread->frame = thread->frame + 1;              /* push a level */
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_animal_morph;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}

/* ----------------------------------------------------------------- tl_shang_tsung_snake
 *
 * armv7 0x000a1f1c, 336 bytes.  **Complete.**
 *
 *      token == 0:      animality_tune(obj)
 *                       obj->field40 = a_snake
 *                       obj->a10 = 0x12
 *                       token := 0x4d7, descend into t_animal_morph
 *
 *      token == 0x4d7:  obj->field1c = 5
 *                       token := 0x4db, descend into t_mframew
 *
 *      token == 0x4db:  obj->field38 = t_eaten_by_snake
 *                       takeover_him(obj)
 *                       obj->field48 = 0x000a000a; shake_a11(obj)
 *                       token := 0x4e1, park 0x20
 *
 *      token == 0x4e1:  obj->field1c = 5
 *                       token := 0x4e4, descend into t_mframew
 *
 *      token == 0x4e4:  token := 0x4e6, park 0x40
 *
 *      token == 0x4e6:  obj->field40 = a_snake
 *                       frame[frame].handler = t_unmorph_and_exit
 *
 *      otherwise:       return -3
 *
 * **The same six states as `tl_kabal_skeleton`, state for state.** Both morph for 0x12,
 * wait five frames, hand the victim their reaction, wait, wait five more, park, and unmorph.
 * The only differences are what happens beside the handover -- the skeleton charges in
 * beforehand with `towards_x_vel` and stops itself afterwards, the snake shakes the screen
 * at 0x000a000a -- and the two park lengths, 0x10/0x10 against 0x20/0x40.
 *
 * So the six-state driver is a real template and not a coincidence of two routines: the
 * animal supplies its word list, its morph length, its victim reaction, its two waits, and
 * at most one extra call.
 *
 * **Both tokens that share a register are read from the right load.** 0x4d7's state stores
 * `r2` still holding 0x4db from entry, and 0x4e1's state stores `r2` reloaded with 0x4e4 by
 * the dispatch on the way past -- the same one-register-two-tokens hazard recorded for
 * `t_sz_slam`, and transcribing either store without tracing which load reaches it would
 * give two states the same token.
 *
 * The shake pair is doubled, 0xa and 0xa, so the snake's strike shakes evenly.
 */
extern uint32_t a_snake[];                       /* 0x00177378 */

long tl_shang_tsung_snake(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0x4d7) {
        obj->field1c = 5;

        *mk3_frame(thread, thread->frame + 1) = 0x4db;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x4db) {
        obj->field38 = (uint32_t)(uintptr_t)t_eaten_by_snake;
        takeover_him(obj);

        obj->field48 = 0x000a000a;
        shake_a11(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x4e1;
        thread->fieldfc = 0x20;
        return 0x20;
    }

    if (token == 0x4e1) {
        obj->field1c = 5;

        *mk3_frame(thread, thread->frame + 1) = 0x4e4;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x4e4) {
        *mk3_frame(thread, frame + 1) = 0x4e6;
        thread->fieldfc = 0x40;
        return 0x40;
    }

    if (token == 0x4e6) {
        obj->field40 = (uint32_t)(uintptr_t)a_snake;

        return mk3_install(thread, (MK3THREADFUNC)t_unmorph_and_exit);
    }

    if (token != 0)
        return -3;

    animality_tune(obj);

    obj->field40 = (uint32_t)(uintptr_t)a_snake;
    obj->a10 = 0x12;

    *mk3_frame(thread, thread->frame + 1) = 0x4d7;
    thread->frame = thread->frame + 1;              /* push a level */
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_animal_morph;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}

/* ------------------------------------------------------------------ tl_smoke_bull_shit
 *
 * armv7 0x000a2dfc, 356 bytes.  **Complete.**
 *
 *      token == 0:      animality_tune(obj)
 *                       obj->field40 = a_bull
 *                       obj->a10 = 0x12
 *                       token := 0x63b, descend into t_animal_morph
 *
 *      token == 0x63b:  sans_repell_for_good(obj)
 *                       kill_and_stop_scrolling(obj)
 *                       obj->field1c = 0xa0000; towards_x_vel(obj)
 *                       token := 0x641, park 1
 *
 *      token == 0x641:  sans_repell_for_good(obj)
 *                       obj->field1c = 4; init_anirate(obj)
 *                       token := 0x648, park 1
 *
 *      token == 0x648:  next_anirate(obj)
 *                       get_x_dist(obj)
 *                       if (obj->field28 > 0x60) { token := 0x648, park 1 }
 *                       obj->field40 = a_bull; find_part2(obj)
 *                       do_next_a9_frame(obj)
 *                       stop_me_player(obj)
 *                       obj->field48 = 0x000a000a; shake_a11(obj)
 *                       obj->field38 = t_hit_by_bull
 *                       takeover_him(obj)
 *                       token := 0x658, park 0x60
 *
 *      token == 0x658:  obj->field40 = a_bull
 *                       frame[frame].handler = t_unmorph_and_exit
 *
 *      otherwise:       return -3
 *
 * **The fourth driver shape: charge until you arrive.** Instead of a fixed wait between the
 * morph and the handover, state 0x648 measures the gap every frame with `get_x_dist` and
 * re-arms itself while the opponent is more than 0x60 away. So the bull's run is as long as
 * the arena makes it, and the hit lands on contact rather than on a schedule.
 *
 * **The distance test is written out here rather than called.** `q_bat_1` and `q_bat_3`
 * ask the same question about the same field with `get_x_dist` and answer in 0x5c; this
 * routine calls `get_x_dist` and branches on 0x28 directly. So the `q_*` helpers exist for
 * `t_animate_till_a11`, which needs a predicate it can call through a pointer, and a state
 * machine that can just branch does not use them.
 *
 * **`sans_repell_for_good` is called twice, in two consecutive states.** Reading it (24
 * bytes, mkfatal.c) it writes 0x500 into the object's 0x1c and a halfword into the global at
 * 0x456, so the second call is not idempotent bookkeeping -- it restores 0x1c to 0x500 after
 * `towards_x_vel` has used it, and only then is 0x1c reloaded with the animation rate 4.
 * Transcribed as it stands; nothing here says whether the repetition is deliberate.
 *
 * The re-arming branch lands on the token store inside the 0x641 tail rather than at the top
 * of its own state, so the charge loop and the frame after `init_anirate` share one store --
 * which is why 0x648 is written from two places and 1 is the park in both.
 */
extern uint32_t a_bull[];                        /* 0x0017745c */
void find_part2(MK3OBJ *obj);

long tl_smoke_bull_shit(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0x63b) {
        sans_repell_for_good(obj);
        kill_and_stop_scrolling(obj);

        obj->field1c = 0xa0000;
        towards_x_vel(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x641;
        thread->fieldfc = 1;
        return 1;
    }

    if (token == 0x641 || token == 0x648) {
        if (token == 0x641) {
            sans_repell_for_good(obj);

            obj->field1c = 4;
            init_anirate(obj);

        } else {
            next_anirate(obj);
            get_x_dist(obj);

            if ((long)obj->field28 <= 0x60) {
                obj->field40 = (uint32_t)(uintptr_t)a_bull;
                find_part2(obj);
                do_next_a9_frame(obj);
                stop_me_player(obj);

                obj->field48 = 0x000a000a;
                shake_a11(obj);

                obj->field38 = (uint32_t)(uintptr_t)t_hit_by_bull;
                takeover_him(obj);

                *mk3_frame(thread, thread->frame + 1) = 0x658;
                thread->fieldfc = 0x60;
                return 0x60;
            }
        }

        *mk3_frame(thread, thread->frame + 1) = 0x648;
        thread->fieldfc = 1;
        return 1;
    }

    if (token == 0x658) {
        obj->field40 = (uint32_t)(uintptr_t)a_bull;

        return mk3_install(thread, (MK3THREADFUNC)t_unmorph_and_exit);
    }

    if (token != 0)
        return -3;

    animality_tune(obj);

    obj->field40 = (uint32_t)(uintptr_t)a_bull;
    obj->a10 = 0x12;

    *mk3_frame(thread, thread->frame + 1) = 0x63b;
    thread->frame = thread->frame + 1;              /* push a level */
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_animal_morph;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}

/* ------------------------------------------------------------------ tl_liu_kang_dragon
 *
 * armv7 0x000a206c, 380 bytes.  **Complete.**
 *
 *      token == 0:      animality_tune(obj)
 *                       tsound_func(obj, 0x92)
 *                       obj->field40 = a_dragon
 *                       obj->a10 = 0x12
 *                       token := 0x4bd, descend into t_animal_morph
 *
 *      token == 0x4bd:  token := 0x4be, park 0x30
 *
 *      token == 0x4be:  obj->field1c = 4
 *                       token := 0x4c1, descend into t_mframew
 *
 *      token == 0x4c1:  obj->field38 = t_bit_in_half
 *                       takeover_him(obj)
 *                       obj->field48 = 0x000a000a; shake_a11(obj)
 *                       token := 0x4c6, park 0x40
 *
 *      token == 0x4c6:  obj->field1c = 4
 *                       token := 0x4c9, descend into t_mframew
 *
 *      token == 0x4c9:  token := 0x4cb, park 0x30
 *
 *      token == 0x4cb:  obj->field40 = a_dragon
 *                       frame[frame].handler = t_unmorph_and_exit
 *
 *      otherwise:       return -3
 *
 * **The six-state template with a park added on each side of the bite.** Seven states, and
 * the two new ones are both bare parks of 0x30 -- one after the morph and one before the
 * unmorph -- so the dragon is on screen longer than the snake without doing anything more.
 *
 * It is also the only driver so far to play a sound of its own, `tsound_func(obj, 0x92)`,
 * beside the animality music. The roar is the dragon's alone.
 *
 * **Its victim reaction is `t_bit_in_half`, which settles who reads the unnamed table.**
 * That routine indexes `lao_ani_data + 0x142c` by the character number, and this is the
 * driver that reaches it -- so the offset is a shared body-pieces list reached by whichever
 * animal cuts a fighter in half, not something belonging to Kung Lao. The symbol
 * `lao_ani_data` names only where the block starts.
 *
 * **Three tokens live in `r2` across the dispatch and each state stores a different one.**
 * 0x4be stores 0x4c1 (r2 as loaded at entry, on the `ble` path where nothing reassigns it),
 * 0x4c6 stores 0x4c9 (r2 reloaded by the dispatch on the way past), and the top of the
 * function compares against the same register. Three reads of one register with three
 * values, and every store had to be traced back to its own load.
 */
extern uint32_t a_dragon[];                      /* 0x001773fc */

long tl_liu_kang_dragon(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0x4bd) {
        *mk3_frame(thread, frame + 1) = 0x4be;
        thread->fieldfc = 0x30;
        return 0x30;
    }

    if (token == 0x4be) {
        obj->field1c = 4;

        *mk3_frame(thread, thread->frame + 1) = 0x4c1;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x4c1) {
        obj->field38 = (uint32_t)(uintptr_t)t_bit_in_half;
        takeover_him(obj);

        obj->field48 = 0x000a000a;
        shake_a11(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x4c6;
        thread->fieldfc = 0x40;
        return 0x40;
    }

    if (token == 0x4c6) {
        obj->field1c = 4;

        *mk3_frame(thread, thread->frame + 1) = 0x4c9;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x4c9) {
        *mk3_frame(thread, frame + 1) = 0x4cb;
        thread->fieldfc = 0x30;
        return 0x30;
    }

    if (token == 0x4cb) {
        obj->field40 = (uint32_t)(uintptr_t)a_dragon;

        return mk3_install(thread, (MK3THREADFUNC)t_unmorph_and_exit);
    }

    if (token != 0)
        return -3;

    animality_tune(obj);
    tsound_func(obj, 0x92);

    obj->field40 = (uint32_t)(uintptr_t)a_dragon;
    obj->a10 = 0x12;

    *mk3_frame(thread, thread->frame + 1) = 0x4bd;
    thread->frame = thread->frame + 1;              /* push a level */
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_animal_morph;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}

/* -------------------------------------------------------------------- tl_mileena_skunk
 *
 * armv7 0x000a3d20, 396 bytes.  **Complete.**
 *
 *      token == 0:      token := 0x222, descend into t_cute_animality_start
 *
 *      token == 0x222:  sans_repell_for_good(obj)
 *                       obj->field40 = 0x19; get_char_ani2(obj)
 *                       obj->field08->field2c = *(long *)obj->field40 & 0x3fff
 *                       obj->field1c = ~0x1f          (-0x20)
 *                       obj->field20 = -0x20 + 0x40 = 0x20
 *                       multi_adjust_xy(obj)
 *                       ground_ob(obj, obj->field08)
 *                       obj->a10 = obj->field40
 *                       obj->field48 = 3
 *                       -- falls into the 0x247 tail --
 *
 *      token == 0x247:  if (--obj->field48 != 0) -- the 0x247 tail --
 *                       NewThread(obj, t_odor_proc)
 *                       obj->field48 = 2
 *                       -- falls into the 0x250 tail --
 *
 *      token == 0x250:  if (--obj->field48 != 0) -- the 0x250 tail --
 *                       obj->field38 = t_r_scared_of_skunk
 *                       takeover_him(obj)
 *                       token := 0x257, park 0x30
 *
 *      the 0x247 tail:  obj->field40 = obj->a10; obj->field1c = 5
 *                       token := 0x247, descend into t_mframew
 *
 *      the 0x250 tail:  obj->field40 = obj->a10; obj->field1c = 5
 *                       token := 0x250, descend into t_mframew
 *
 *      token == 0x257:  death_blow_complete(obj)
 *                       frame[frame].handler = t_wait_forever
 *
 *      otherwise:       return -3
 *
 * **This is the routine that explains why every driver writes 0x40 twice.** `get_char_ani2`
 * turns the small index 0x19 into a POINTER, which this saves into 0x44 -- and then both
 * animation loops reload 0x40 from 0x44 before every descent into `t_mframew`, because
 * `t_mframew` walks 0x40 forward as a cursor and leaves it past the end. So **0x44 is the
 * saved copy of the 0x40 cursor**, and the other drivers' second `obj->field40 = a_<animal>`
 * is the same reset written with the constant instead of a saved copy.
 *
 * The first word of the list is masked with 0x3fff before it becomes the part's animation, so
 * the top two bits of an entry in these lists are flags and not part of the number. First
 * place in this module where that mask is visible.
 *
 * **Two counted loops, three passes then two**, both counting in 0x48 and both descending
 * into the same routine -- and the odour thread starts between them. So the skunk animates,
 * starts `t_odor_proc` as a separate thread (the second such thread in this file, after
 * `t_r_rabbit` and `t_crunch_sounds`), animates twice more, and only then frightens the
 * opponent.
 *
 * 0x48 is a plain counter here, where `shake_a11` reads it as a pair of halfwords and
 * `t_animate_till_a11` calls it as a function pointer. Third reading of that field in this
 * module; the state decides, not the field.
 *
 * The negative 0x1c is written as `mvn r3, #0x1f` and the 0x20 beside it as `adds r3, #0x40`
 * -- one literal, two fields, -0x20 and 0x20 -- the same shape as every `t_shake_ob_up`
 * caller.
 */
void get_char_ani2(MK3OBJ *obj);
void multi_adjust_xy(MK3OBJ *obj);
long t_r_scared_of_skunk(MK3THREAD *thread);     /* pointer slot 0x000f3468 */

long tl_mileena_skunk(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);
    uint32_t next;

    if (token == 0) {
        *mk3_frame(thread, frame + 1) = 0x222;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_cute_animality_start;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x257) {
        death_blow_complete(obj);

        return mk3_install(thread, (MK3THREADFUNC)t_wait_forever);
    }

    if (token == 0x222) {
        sans_repell_for_good(obj);

        obj->field40 = 0x19;
        get_char_ani2(obj);

        obj->field08->field2c =
            *(uint32_t *)(uintptr_t)obj->field40 & 0x3fff;

        obj->field1c = (uint32_t)~0x1fu;
        obj->field20 = (uint32_t)(~0x1fu + 0x40u);
        multi_adjust_xy(obj);

        ground_ob(obj, obj->field08);

        obj->a10    = obj->field40;
        obj->field48 = 3;

        next = 0x247;

    } else if (token == 0x247) {
        obj->field48 = obj->field48 - 1;
        if (obj->field48 != 0) {
            next = 0x247;
        } else {
            NewThread(obj, (MK3THREADFUNC)t_odor_proc);
            obj->field48 = 2;
            next = 0x250;
        }

    } else if (token == 0x250) {
        obj->field48 = obj->field48 - 1;
        if (obj->field48 != 0) {
            next = 0x250;
        } else {
            obj->field38 = (uint32_t)(uintptr_t)t_r_scared_of_skunk;
            takeover_him(obj);

            *mk3_frame(thread, thread->frame + 1) = 0x257;
            thread->fieldfc = 0x30;
            return 0x30;
        }

    } else {
        return -3;
    }

    obj->field40 = obj->a10;
    obj->field1c = 5;

    *mk3_frame(thread, thread->frame + 1) = next;
    thread->frame = thread->frame + 1;               /* push a level */
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}

/* ------------------------------------------------------------------- tl_scorpion_pengo
 *
 * armv7 0x000a4118, 424 bytes.  **Complete.**
 *
 *      token == 0:      token := 0x2b3, descend into t_cute_animality_start
 *
 *      token == 0x2b3:  sans_repell_for_good(obj)
 *                       obj->field1c = 0x20000; set_vel_flip(obj)
 *                       obj->field40 = a_pengo
 *                       frame_a9(obj)
 *                       ground_ob(obj, obj->field08)
 *                       token := 0x2cd, park 0x10
 *
 *      token == 0x2cd:  obj->field1c = 5; init_anirate(obj)
 *                       token := 0x2d3, park 1
 *
 *      token == 0x2d3:  pengo_animate(obj)
 *                       obj->field1c = (int16_t)part->x0e
 *                       obj->field20 = (int16_t)him->x0e - obj->field1c
 *                       if (obj->field20 < 0) obj->field20 = -obj->field20
 *                       if (obj->field20 > 8) { token := 0x2d3, park 1 }
 *                       t = NewThreadProc(obj, t_egg_proc)
 *                       t->field08->field18 = 0
 *                       center_around_him(obj)
 *                       stop_me_player(obj)
 *                       obj->a10 = 0x10
 *                       token := 0x2e6, park 1
 *
 *      token == 0x2e6:  pengo_animate(obj)
 *                       if (--obj->a10 == 0) {
 *                           obj->field1c = 0x20000; set_vel_flip(obj)
 *                           obj->a10 = 0x20
 *                           token := 0x2f0, park 1
 *                       }
 *                       token := 0x2e6, park 1
 *
 *      token == 0x2f0:  pengo_animate(obj)
 *                       if (--obj->a10 == 0) {
 *                           stop_me_player(obj)
 *                           death_blow_complete(obj)
 *                           frame[frame].handler = t_wait_forever
 *                       }
 *                       token := 0x2f0, park 1
 *
 *      otherwise:       return -3
 *
 * **The penguin walks up to the opponent and lays the egg.** State 0x2d3 measures the gap
 * itself -- the two x positions out of the halfword at 0x0e, subtracted, made positive by
 * hand -- and re-arms until it is eight or less. Then it starts `t_egg_proc` as its own
 * thread, and that thread is what hands `t_r_egg` to the victim.
 *
 * **A different arrival test from `tl_smoke_bull_shit`'s.** The bull calls `get_x_dist` and
 * reads 0x28; this reads both objects' 0x0e and does the subtraction and the absolute value
 * with its own instructions -- `rsblt`, in an `itt lt` block. Same question, three
 * spellings in this module now, and none of them shares code with the others.
 *
 * **`NewThreadProc` returns the new object and this routine writes through it**: the new
 * thread's own 0x08 has its 0x18 zeroed before it ever runs. That is the only place in the
 * file that touches another thread's fields from outside, and it is why `t_egg_proc` can
 * assume 0x18 is clear at entry.
 *
 * `obj->field1c = 0x20000` in state 0x2e6 is computed as `add r3, r3, #0x20000` on a
 * register the branch has just proved to be zero, not loaded as a literal. Transcribed as
 * the value, with this note, because the arithmetic carries no information the value does
 * not.
 *
 * The walk is two phases, 0x10 passes then 0x20, with `set_vel_flip` between them -- so the
 * penguin waddles up, lays, and waddles off the way it came.
 */
extern uint32_t a_pengo[];                       /* 0x00177544 */
void *NewThreadProc(void *owner, MK3THREADFUNC func);
void center_around_him(MK3OBJ *obj);

long tl_scorpion_pengo(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);
    MK3OBJ  *him;
    MK3OBJ  *spawn;

    if (token == 0) {
        *mk3_frame(thread, frame + 1) = 0x2b3;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_cute_animality_start;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x2b3) {
        sans_repell_for_good(obj);

        obj->field1c = 0x20000;
        set_vel_flip(obj);

        obj->field40 = (uint32_t)(uintptr_t)a_pengo;
        frame_a9(obj);

        ground_ob(obj, obj->field08);

        *mk3_frame(thread, thread->frame + 1) = 0x2cd;
        thread->fieldfc = 0x10;
        return 0x10;
    }

    if (token == 0x2cd) {
        obj->field1c = 5;
        init_anirate(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x2d3;
        thread->fieldfc = 1;
        return 1;
    }

    if (token == 0x2d3) {
        pengo_animate(obj);

        obj->field1c = (uint32_t)(int32_t)(int16_t)MK3_FIELD0E(obj->field08);

        him = (MK3OBJ *)(void *)(uintptr_t)obj->field00->him;
        obj->field20 = (uint32_t)(int32_t)(int16_t)MK3_FIELD0E(him)
                       - obj->field1c;
        if ((long)obj->field20 < 0)
            obj->field20 = (uint32_t)(-(long)obj->field20);

        if ((long)obj->field20 <= 8) {
            spawn = (MK3OBJ *)NewThreadProc(obj, (MK3THREADFUNC)t_egg_proc);
            spawn->field08->field18 = 0;

            center_around_him(obj);
            stop_me_player(obj);

            obj->a10 = 0x10;

            *mk3_frame(thread, thread->frame + 1) = 0x2e6;
            thread->fieldfc = 1;
            return 1;
        }

        *mk3_frame(thread, thread->frame + 1) = 0x2d3;
        thread->fieldfc = 1;
        return 1;
    }

    if (token == 0x2e6) {
        pengo_animate(obj);

        obj->a10 = obj->a10 - 1;
        if (obj->a10 == 0) {
            obj->field1c = 0x20000;
            set_vel_flip(obj);

            obj->a10 = 0x20;

            *mk3_frame(thread, thread->frame + 1) = 0x2f0;
            thread->fieldfc = 1;
            return 1;
        }

        *mk3_frame(thread, thread->frame + 1) = 0x2e6;
        thread->fieldfc = 1;
        return 1;
    }

    if (token != 0x2f0)
        return -3;

    pengo_animate(obj);

    obj->a10 = obj->a10 - 1;
    if (obj->a10 == 0) {
        stop_me_player(obj);
        death_blow_complete(obj);

        return mk3_install(thread, (MK3THREADFUNC)t_wait_forever);
    }

    *mk3_frame(thread, thread->frame + 1) = 0x2f0;
    thread->fieldfc = 1;
    return 1;
}

/* ------------------------------------------------------------------------- tl_jax_lion
 *
 * armv7 0x000a1904, 428 bytes.  **Complete.**
 *
 *      token == 0:      animality_tune(obj)
 *                       obj->field1c = 2
 *                       obj->field40 = a_jax_lion
 *                       obj->a10 = 0x12
 *                       token := 0x7ca, descend into t_animal_morph
 *
 *      token == 0x7ca:  sans_repell_for_good(obj)
 *                       token := 0x7cc, park 0x30
 *
 *      token == 0x7cc:  obj->field1c = 0x20000; towards_x_vel(obj)
 *                       obj->field1c = 5
 *                       token := 0x7d1, descend into t_mframew
 *
 *      token == 0x7d1:  obj->field38 = t_lion_mauled
 *                       takeover_him(obj)
 *                       stop_me_player(obj)
 *                       stop_him(obj)
 *                       obj->field1c = 0x00050020
 *                       token := 0x7db, descend into t_animate_a0_frames
 *
 *      token == 0x7db:  wfe_him(obj)
 *                       token := 0x7dd, park 8
 *
 *      token == 0x7dd:  tsound_func(obj, 0x27)
 *                       obj->field40 = a_jax_lion
 *                       obj->field1c = 5
 *                       token := 0x7e2, descend into t_backwards_ani
 *
 *      token == 0x7e2:  frame[frame].handler = t_animality_complete
 *
 *      otherwise:       return -3
 *
 * **Seven states, and the ending is the unmorph written out by hand.** `t_unmorph_and_exit`
 * is exactly "descend into t_backwards_ani, then install t_animality_complete", and states
 * 0x7dd and 0x7e2 are that pair spelled out -- because this driver has to play sound 0x27 and
 * reset 0x40 on the way in, which the shared routine has no way to do.
 *
 * **`wfe_him` is worth reading: 24 bytes that put `t_wait_forever` into 0x38 and call
 * `takeover_him`.** So state 0x7db does not stop the victim, it parks them forever -- which is
 * the right thing after `t_lion_mauled`, whose own two states never stop handing off to each
 * other. The mauling loop is ended from outside, by the attacker, and this is where.
 *
 * That answers the open question left by `t_lion_mauled`: the loop has no exit because it
 * does not need one.
 *
 * **0x1c carries a packed halfword pair here, 5 and 0x20**, handed to `t_animate_a0_frames`.
 * Every other packed pair in this module goes through 0x40, so the field that carries one is
 * chosen by the callee and not fixed.
 *
 * Three tokens ride in `r8` and one in `sl` across this dispatch -- 0x7d1 reaching the
 * mid-states, 0x7dd reaching 0x7db's store, and 0x7ca reaching state 0 -- so four stores in
 * this function had to be traced back to four different loads.
 */
extern uint32_t a_jax_lion[];                    /* 0x001774d4 */
void stop_him(MK3OBJ *obj);
void wfe_him(MK3OBJ *obj);
long t_animate_a0_frames(MK3THREAD *thread);     /* pointer slot 0x000f36b8 */

long tl_jax_lion(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0x7ca) {
        sans_repell_for_good(obj);

        *mk3_frame(thread, frame + 1) = 0x7cc;
        thread->fieldfc = 0x30;
        return 0x30;
    }

    if (token == 0x7cc) {
        obj->field1c = 0x20000;
        towards_x_vel(obj);
        obj->field1c = 5;

        *mk3_frame(thread, thread->frame + 1) = 0x7d1;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x7d1) {
        obj->field38 = (uint32_t)(uintptr_t)t_lion_mauled;
        takeover_him(obj);
        stop_me_player(obj);
        stop_him(obj);

        obj->field1c = 0x00050020;

        *mk3_frame(thread, thread->frame + 1) = 0x7db;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_animate_a0_frames;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x7db) {
        wfe_him(obj);

        *mk3_frame(thread, frame + 1) = 0x7dd;
        thread->fieldfc = 8;
        return 8;
    }

    if (token == 0x7dd) {
        tsound_func(obj, 0x27);

        obj->field40 = (uint32_t)(uintptr_t)a_jax_lion;
        obj->field1c = 5;

        *mk3_frame(thread, thread->frame + 1) = 0x7e2;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_backwards_ani;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x7e2)
        return mk3_install(thread, (MK3THREADFUNC)t_animality_complete);

    if (token != 0)
        return -3;

    animality_tune(obj);

    obj->field1c = 2;
    obj->field40 = (uint32_t)(uintptr_t)a_jax_lion;
    obj->a10 = 0x12;

    *mk3_frame(thread, thread->frame + 1) = 0x7ca;
    thread->frame = thread->frame + 1;              /* push a level */
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_animal_morph;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}

/* ------------------------------------------------------------------------ tl_swat_dino
 *
 * armv7 0x000a2830, 432 bytes.  **Complete.**
 *
 *      token == 0:      animality_tune(obj)
 *                       obj->field1c = 2
 *                       obj->field40 = a_swat_dino
 *                       obj->a10 = 0x12
 *                       token := 0x6c4, descend into t_animal_morph
 *
 *      token == 0x6c4:  sans_repell_for_good(obj)
 *                       tsound_func(obj, 0x92)
 *                       token := 0x6c8, park 0x30
 *
 *      token == 0x6c8:  obj->field1c = 5
 *                       token := 0x6cb, descend into t_mframew
 *
 *      token == 0x6cb:  obj->field48 = 0x00080008; shake_a11(obj)
 *                       obj->field38 = t_bit_in_half
 *                       takeover_him(obj)
 *                       tsound_func(obj, 0x24)
 *                       tsound_func(obj, 0x25)
 *                       token := 0x6d4, park 0x30
 *
 *      token == 0x6d4:  obj->field1c = 5
 *                       token := 0x6d7, descend into t_mframew
 *
 *      token == 0x6d7:  tsound_func(obj, 0x27)
 *                       ground_player(obj)
 *                       obj->field40 = a_swat_dino
 *                       obj->field1c = 5
 *                       token := 0x6dd, descend into t_backwards_ani
 *
 *      token == 0x6dd:  frame[frame].handler = t_animality_complete
 *
 *      otherwise:       return -3
 *
 * **The same seven states as `tl_jax_lion`, with a different middle.** Both morph, wait,
 * wait through `t_mframew`, hit, wait, then unmorph by hand through `t_backwards_ani` and
 * install `t_animality_complete`. Where the lion parks the victim with `wfe_him`, the
 * dinosaur shakes at 0x00080008 and plays the crunch pair, and where the lion animates
 * through `t_animate_a0_frames`, this one simply parks 0x30.
 *
 * **Sound 0x92 is the dragon's roar reused.** `tl_liu_kang_dragon` plays it at state 0
 * beside the music; this plays it one state later, after the morph. So 0x92 is a generic
 * large-animal noise and not the dragon's own -- worth knowing before naming it.
 *
 * **Sounds 0x24 and 0x25 back to back, third site.** `t_crunch_sounds` plays them as a pair
 * six times over, `t_lion_mauled` plays them one state apart, and this plays the pair once.
 * All three spellings now measured; the pair is a bite and the routines choose the rhythm.
 *
 * `t_bit_in_half` is the victim reaction, shared with `tl_liu_kang_dragon` -- so the
 * unnamed list at `lao_ani_data + 0x142c` is read for the dragon and for the dinosaur, and
 * "whichever animal cuts a fighter in half" is now two animals rather than one.
 *
 * Both `t_mframew` waits store their token out of `r1`, which the dispatch has reloaded
 * between them: 0x6cb on the way in and 0x6d7 on the way past. Two stores, one register,
 * two values.
 */
extern uint32_t a_swat_dino[];                   /* 0x00177518 */
void ground_player(MK3OBJ *obj);

long tl_swat_dino(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0x6c4) {
        sans_repell_for_good(obj);
        tsound_func(obj, 0x92);

        *mk3_frame(thread, frame + 1) = 0x6c8;
        thread->fieldfc = 0x30;
        return 0x30;
    }

    if (token == 0x6c8) {
        obj->field1c = 5;

        *mk3_frame(thread, thread->frame + 1) = 0x6cb;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x6cb) {
        obj->field48 = 0x00080008;
        shake_a11(obj);

        obj->field38 = (uint32_t)(uintptr_t)t_bit_in_half;
        takeover_him(obj);

        tsound_func(obj, 0x24);
        tsound_func(obj, 0x25);

        *mk3_frame(thread, frame + 1) = 0x6d4;
        thread->fieldfc = 0x30;
        return 0x30;
    }

    if (token == 0x6d4) {
        obj->field1c = 5;

        *mk3_frame(thread, thread->frame + 1) = 0x6d7;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x6d7) {
        tsound_func(obj, 0x27);
        ground_player(obj);

        obj->field40 = (uint32_t)(uintptr_t)a_swat_dino;
        obj->field1c = 5;

        *mk3_frame(thread, thread->frame + 1) = 0x6dd;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_backwards_ani;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x6dd)
        return mk3_install(thread, (MK3THREADFUNC)t_animality_complete);

    if (token != 0)
        return -3;

    animality_tune(obj);

    obj->field1c = 2;
    obj->field40 = (uint32_t)(uintptr_t)a_swat_dino;
    obj->a10 = 0x12;

    *mk3_frame(thread, thread->frame + 1) = 0x6c4;
    thread->frame = thread->frame + 1;              /* push a level */
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_animal_morph;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}

/* ------------------------------------------------------------------------- tl_sz_polar
 *
 * armv7 0x000a2678, 440 bytes.  **Complete.**
 *
 *      token == 0:      animality_tune(obj)
 *                       obj->field1c = 2
 *                       obj->field40 = a_sz_polar
 *                       obj->a10 = 0x12
 *                       token := 0x6eb, descend into t_animal_morph
 *
 *      token == 0x6eb:  sans_repell_for_good(obj)
 *                       token := 0x6ed, park 0x30
 *
 *      token == 0x6ed:  tsound_func(obj, 0x95)
 *                       obj->field1c = 0x20000; towards_x_vel(obj)
 *                       obj->field1c = 5
 *                       token := 0x6f3, descend into t_mframew
 *
 *      token == 0x6f3:  obj->field38 = t_lion_mauled
 *                       takeover_him(obj)
 *                       stop_me_player(obj)
 *                       stop_him(obj)
 *                       obj->field1c = 0x00050020
 *                       token := 0x6fb, descend into t_animate_a0_frames
 *
 *      token == 0x6fb:  wfe_him(obj)
 *                       token := 0x6fd, park 8
 *
 *      token == 0x6fd:  tsound_func(obj, 0x27)
 *                       ground_player(obj)
 *                       obj->field40 = a_sz_polar
 *                       obj->field1c = 5
 *                       token := 0x703, descend into t_backwards_ani
 *
 *      token == 0x703:  frame[frame].handler = t_animality_complete
 *
 *      otherwise:       return -3
 *
 * **`tl_jax_lion` with two calls added and every constant changed.** Seven states in the
 * same order, the same `t_animate_a0_frames` pair 0x00050020 in 0x1c, the same `wfe_him` to
 * park the victim, the same hand-written unmorph. The additions are one sound, 0x95 during
 * the charge, and `ground_player` before the unmorph -- which `tl_swat_dino` also calls at
 * the same point.
 *
 * **And the victim reaction is `t_lion_mauled` again, unchanged.** So that routine is shared
 * by the lion and the polar bear, and both of its callers end its endless loop the same way,
 * with `wfe_him`. Third reaction in this module proved to be shared rather than per-animal,
 * after `t_dino_bucked` and `t_bit_in_half`.
 *
 * The pattern across the drivers now measured is: **the reactions are a pool of about a
 * dozen, and a driver is a schedule that picks one.** Sub-Zero's polar bear and Jax's lion
 * maul identically and differ only in the animal on screen and two calls.
 *
 * Four tokens ride in `r8` and `sl` here as they do in `tl_jax_lion` -- 0x6eb into state 0,
 * 0x6f3 into the charge state's store, 0x6fd into 0x6fb's -- and each store was traced to
 * its own load.
 */
extern uint32_t a_sz_polar[];                    /* 0x00177108 */

long tl_sz_polar(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0x6eb) {
        sans_repell_for_good(obj);

        *mk3_frame(thread, frame + 1) = 0x6ed;
        thread->fieldfc = 0x30;
        return 0x30;
    }

    if (token == 0x6ed) {
        tsound_func(obj, 0x95);

        obj->field1c = 0x20000;
        towards_x_vel(obj);
        obj->field1c = 5;

        *mk3_frame(thread, thread->frame + 1) = 0x6f3;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x6f3) {
        obj->field38 = (uint32_t)(uintptr_t)t_lion_mauled;
        takeover_him(obj);
        stop_me_player(obj);
        stop_him(obj);

        obj->field1c = 0x00050020;

        *mk3_frame(thread, thread->frame + 1) = 0x6fb;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_animate_a0_frames;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x6fb) {
        wfe_him(obj);

        *mk3_frame(thread, frame + 1) = 0x6fd;
        thread->fieldfc = 8;
        return 8;
    }

    if (token == 0x6fd) {
        tsound_func(obj, 0x27);
        ground_player(obj);

        obj->field40 = (uint32_t)(uintptr_t)a_sz_polar;
        obj->field1c = 5;

        *mk3_frame(thread, thread->frame + 1) = 0x703;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_backwards_ani;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x703)
        return mk3_install(thread, (MK3THREADFUNC)t_animality_complete);

    if (token != 0)
        return -3;

    animality_tune(obj);

    obj->field1c = 2;
    obj->field40 = (uint32_t)(uintptr_t)a_sz_polar;
    obj->a10 = 0x12;

    *mk3_frame(thread, thread->frame + 1) = 0x6eb;
    thread->frame = thread->frame + 1;              /* push a level */
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_animal_morph;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}

/* ----------------------------------------------------------------------- tl_cyrax_shark
 *
 * armv7 0x000a2c28, 468 bytes.  **Complete.**
 *
 *      token == 0:      animality_tune(obj)
 *                       obj->field40 = a_shark
 *                       obj->a10 = 0x12
 *                       token := 0x466, descend into t_animal_morph
 *
 *      token == 0x466:  sans_repell_for_good(obj)
 *                       kill_and_stop_scrolling(obj)
 *                       obj->field1c = 0xa0000; towards_x_vel(obj)
 *                       token := 0x46d, park 8
 *
 *      token == 0x46d:  sans_repell_for_good(obj)
 *                       token := 0x46f, park 0x48
 *
 *      token == 0x46f:  tsound_func(obj, 0x92)
 *                       stop_me_player(obj)
 *                       match_me_with_him(obj)
 *                       ground_player(obj)
 *                       obj->field1c = 4
 *                       token := 0x477, descend into t_mframew
 *
 *      token == 0x477:  obj->field38 = t_eaten_by_shark
 *                       takeover_him(obj)
 *                       obj->field48 = 0x000a000a; shake_a11(obj)
 *                       token := 0x47e, park 4
 *
 *      token == 0x47e:  obj->field1c = 3
 *                       token := 0x480, descend into t_mframew
 *
 *      token == 0x480:  token := 0x482, park 0x30
 *
 *      token == 0x482:  obj->field40 = a_shark
 *                       frame[frame].handler = t_unmorph_and_exit
 *
 *      otherwise:       return -3
 *
 * **Eight states, and the shark swims to where the opponent stands.** `match_me_with_him`
 * before `ground_player` puts the animal at the victim's position rather than measuring a
 * distance -- so this is a fourth arrival mechanism in the module, and the only one that
 * teleports instead of moving: the bull re-arms on `get_x_dist`, the penguin does its own
 * subtraction, `q_bat_*` answers in 0x5c, and this simply matches.
 *
 * The charge is still there -- 0xa0000 through `towards_x_vel`, the same speed as the bull's
 * -- so the shark visibly travels and is then snapped into place before the bite.
 *
 * **`sans_repell_for_good` in two consecutive states again**, as in `tl_smoke_bull_shit`.
 * Both routines call it once with the charge and once in the state after; the same repetition
 * in two drivers makes it deliberate rather than an accident of one function.
 *
 * The victim reaction is `t_eaten_by_shark`, whose three bites end by installing
 * `t_eaten_by_snake` -- so this one driver reaches two reactions in sequence, and the shark's
 * own contribution is only the three bites.
 *
 * **The dispatch is two levels of signed comparison**, `ble` inside `ble`, which is why the
 * token order in the branch table is 0x46f, then {0x466, 0x46d, 0}, then 0x47e, then 0x477,
 * then 0x480 and 0x482. Read in source order the states are almost shuffled; read through
 * the table they are a straight sequence.
 */
extern uint32_t a_shark[];                       /* 0x001773b8 */
void match_me_with_him(MK3OBJ *obj);

long tl_cyrax_shark(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0x466) {
        sans_repell_for_good(obj);
        kill_and_stop_scrolling(obj);

        obj->field1c = 0xa0000;
        towards_x_vel(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x46d;
        thread->fieldfc = 8;
        return 8;
    }

    if (token == 0x46d) {
        sans_repell_for_good(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x46f;
        thread->fieldfc = 0x48;
        return 0x48;
    }

    if (token == 0x46f) {
        tsound_func(obj, 0x92);
        stop_me_player(obj);
        match_me_with_him(obj);
        ground_player(obj);

        obj->field1c = 4;

        *mk3_frame(thread, thread->frame + 1) = 0x477;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x477) {
        obj->field38 = (uint32_t)(uintptr_t)t_eaten_by_shark;
        takeover_him(obj);

        obj->field48 = 0x000a000a;
        shake_a11(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x47e;
        thread->fieldfc = 4;
        return 4;
    }

    if (token == 0x47e) {
        obj->field1c = 3;

        *mk3_frame(thread, thread->frame + 1) = 0x480;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x480) {
        *mk3_frame(thread, frame + 1) = 0x482;
        thread->fieldfc = 0x30;
        return 0x30;
    }

    if (token == 0x482) {
        obj->field40 = (uint32_t)(uintptr_t)a_shark;

        return mk3_install(thread, (MK3THREADFUNC)t_unmorph_and_exit);
    }

    if (token != 0)
        return -3;

    animality_tune(obj);

    obj->field40 = (uint32_t)(uintptr_t)a_shark;
    obj->a10 = 0x12;

    *mk3_frame(thread, thread->frame + 1) = 0x466;
    thread->frame = thread->frame + 1;              /* push a level */
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_animal_morph;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}

/* ----------------------------------------------------------------------- tl_sindel_wasp
 *
 * armv7 0x000a309c, 488 bytes.  **Complete.**
 *
 *      token == 0:      animality_tune(obj)
 *                       sans_repell_for_good(obj)
 *                       obj->field40 = a_wasp
 *                       obj->a10 = 0xa
 *                       token := 0x670, descend into t_animal_morph
 *
 *      token == 0x670:  obj->field1c = 0x00030010
 *                       token := 0x673, descend into t_animate_a0_frames
 *
 *      token == 0x673:  obj->field38 = t_stung_a_bunch
 *                       takeover_him(obj)
 *                       face_him_at_me(obj)
 *                       match_me_with_him(obj)
 *                       flip_multi(obj)
 *                       obj->field1c = ~0x27              (-0x28)
 *                       obj->field20 = -0x28 - 0x20 = -0x48
 *                       multi_adjust_xy(obj)
 *                       kill_and_stop_scrolling(obj)
 *                       no_edge_both_players()
 *                       obj->field1c = 0x30000; away_x_vel_him(obj)
 *                       obj->field1c = 0x30000; towards_x_vel(obj)
 *                       obj->field40 = a_wasp
 *                       find_part2(obj)
 *                       find_part2(obj)
 *                       obj->field1c = 0x0004001a
 *                       token := 0x68a, descend into t_animate_a0_frames
 *
 *      token == 0x68a:  call_for_him(obj, set_inviso)
 *                       stop_me_player(obj)
 *                       token := 0x68e, park 0x40
 *
 *      token == 0x68e:  death_blow_complete(obj)
 *                       sans_repell_for_good(obj)
 *                       player_normpal(obj)
 *                       obj->field40 = 0; pose_a9_manual(obj)
 *                       ground_ochar(obj)
 *                       obj->field1c = (int16_t)part->y12 + 0x90
 *                       part->y12 = obj->field1c
 *                       part->x0e = *(long *)(G + 0x468) + 0x60
 *                       obj->field1c = 0xfffe0000
 *                       part->field1c = 0xfffe0000
 *                       frame[frame].handler = t_lia_victory
 *
 *      otherwise:       return -3
 *
 * **The wasp carries the victim off, and this is the only driver in the file that does not
 * end in the animality machinery at all.** Its last state installs `t_lia_victory` -- Sindel's
 * victory routine, out of pointer slot 0x000f3464 -- so the finisher runs straight into the
 * end-of-round pose instead of `t_animality_complete` or `t_unmorph_and_exit`. Fourth ending
 * shape, and the only one that leaves this module.
 *
 * **The pick-up is nine calls in one state.** Hand the victim `t_stung_a_bunch` (the loop with
 * no exit, written earlier), turn them to face, match positions, flip, shift by -0x28/-0x48,
 * stop the camera, remove both edge limits, then push the victim away at 0x30000 and pull
 * yourself towards them at the same speed. **The two velocity calls share one register and one
 * value** -- `away_x_vel_him` then `towards_x_vel`, both reading 0x30000 out of 0x1c -- so the
 * pair is what makes the wasp and its victim move together.
 *
 * **`find_part2` is called twice in a row with nothing between the two calls.** mkstat.c's
 * `tl_stat_do_fan_lift` has exactly the same doubled call. Two independent sites make it an
 * idiom rather than a slip, and neither routine reads a result between them -- so whatever the
 * second call adds, it is a side effect of running the search again. Recorded, not explained.
 *
 * **The landing spot is read out of the global, not computed.** `*(long *)(G + 0x468) + 0x60`
 * goes into the part's x, and the y is the grounded position plus 0x90 -- so the victim is put
 * down at a fixed place in the arena rather than where the wasp happens to be. First read of
 * G + 0x468 in the tree.
 *
 * The morph is 0xa passes where every other driver uses 0x12, and `t_animate_a0_frames` gets
 * two different packed pairs, 0x00030010 for the flight out and 0x0004001a for the flight back.
 */
extern uint32_t a_wasp[];                        /* 0x00177164 */
void player_normpal(MK3OBJ *obj);
void ground_ochar(MK3OBJ *obj);
void away_x_vel_him(MK3OBJ *obj);
void call_for_him(MK3OBJ *obj, void (*fn)(MK3OBJ *));
long t_lia_victory(MK3THREAD *thread);           /* pointer slot 0x000f3464 */

long tl_sindel_wasp(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0x670) {
        obj->field1c = 0x00030010;

        *mk3_frame(thread, frame + 1) = 0x673;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_animate_a0_frames;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x673) {
        obj->field38 = (uint32_t)(uintptr_t)t_stung_a_bunch;
        takeover_him(obj);

        face_him_at_me(obj);
        match_me_with_him(obj);
        flip_multi(obj);

        obj->field1c = (uint32_t)~0x27u;
        obj->field20 = (uint32_t)(~0x27u - 0x20u);
        multi_adjust_xy(obj);

        kill_and_stop_scrolling(obj);
        no_edge_both_players();

        obj->field1c = 0x30000;
        away_x_vel_him(obj);
        obj->field1c = 0x30000;
        towards_x_vel(obj);

        obj->field40 = (uint32_t)(uintptr_t)a_wasp;
        find_part2(obj);
        find_part2(obj);

        obj->field1c = 0x0004001a;

        *mk3_frame(thread, thread->frame + 1) = 0x68a;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_animate_a0_frames;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x68a) {
        call_for_him(obj, set_inviso);
        stop_me_player(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x68e;
        thread->fieldfc = 0x40;
        return 0x40;
    }

    if (token == 0x68e) {
        death_blow_complete(obj);
        sans_repell_for_good(obj);
        player_normpal(obj);

        obj->field40 = 0;
        pose_a9_manual(obj);

        ground_ochar(obj);

        obj->field1c = (uint32_t)((int32_t)(int16_t)MK3_FIELD12(obj->field08)
                                  + 0x90);
        MK3_SET_FIELD12(obj->field08, obj->field1c);

        MK3_SET_FIELD0E(obj->field08,
                        *(uint32_t *)(G_BYTES + 0x468) + 0x60);

        obj->field1c         = 0xfffe0000u;
        obj->field08->field1c = 0xfffe0000u;

        return mk3_install(thread, (MK3THREADFUNC)t_lia_victory);
    }

    if (token != 0)
        return -3;

    animality_tune(obj);
    sans_repell_for_good(obj);

    obj->field40 = (uint32_t)(uintptr_t)a_wasp;
    obj->a10 = 0xa;

    *mk3_frame(thread, thread->frame + 1) = 0x670;
    thread->frame = thread->frame + 1;              /* push a level */
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_animal_morph;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}

/* ---------------------------------------------------------------------- tl_kitana_bunny
 *
 * armv7 0x000a3a38, 496 bytes.  **Complete.**
 *
 *      token == 0:      token := 0x3ff, descend into t_cute_animality_start
 *
 *      token == 0x3ff:  sans_repell_for_good(obj)
 *                       set_noedge(obj)
 *                       obj->field40 = 9; get_char_ani2(obj)
 *                       part->y12 = (uint16_t)*(short *)(G + 0xac)
 *                                   - GetFrameHeight(*(long *)obj->field40) - 9
 *                       obj->field1c = 8
 *                       token := 0x41d, descend into t_mframew
 *
 *      token == 0x41d:  tsound_func(obj, 0x92)
 *                       obj->field1c = 3
 *                       token := 0x421, descend into t_mframew
 *
 *      token == 0x421:  part->field1c = 0xfff80000
 *                       obj->field1c = 0xfff80000 + 0xe0000 = 0x00060000
 *                       set_proj_vel(obj)
 *                       obj->field1c = part->field18
 *                       set_x_vel_player(obj)
 *                       token := 0x42c, park 9
 *
 *      token == 0x42c:  kill_and_stop_scrolling(obj)
 *                       stop_me_player(obj)
 *                       obj->field38 = t_r_rabbit
 *                       takeover_him(obj)
 *                       obj->field1c = 4; init_anirate(obj)
 *                       obj->a10 = obj->field00->him
 *                       obj->field48 = 0xc0
 *                       -- falls into the 0x43a tail --
 *
 *      token == 0x43a:  obj->field1c = ((MK3OBJ *)obj->field00->him)->field18
 *                       set_x_vel_player(obj)
 *                       next_anirate(obj)
 *                       if (--obj->field48 <= 0) {
 *                           death_blow_complete(obj)
 *                           player_normpal(obj)
 *                           frame[frame].handler = t_wait_forever
 *                       }
 *                       -- falls into the 0x43a tail --
 *
 *      the 0x43a tail:  token := 0x43a, park 1
 *
 *      otherwise:       return -3
 *
 * **The bunny drags the victim along by copying their velocity every frame.** State 0x43a
 * reads the OTHER fighter's 0x18 and feeds it to `set_x_vel_player` on itself, once per frame
 * for 0xc0 frames -- so the two move as one without either being parented to the other. That
 * is a fifth way of coupling two fighters in this module, after the three handover mechanisms
 * and the wasp's paired velocity calls.
 *
 * **Fourth spelling of the ground placement, and the first to read the floor as a halfword.**
 * `ground_ob` reads `G + 0xac` with `ldr` and measures with `GetFrameHeight`;
 * `tl_reptile_monkey` reads the word and measures with `mk3_getbbox`; mkstat.c's
 * `t_turn_into_a_baby` uses the box and no inset. This one uses `ldrh` on the same offset, the
 * same nine-pixel inset as `ground_ob`, and takes the animation from the FIRST WORD of the
 * list `get_char_ani2` produced rather than from the part's 0x2c. Four readings of one offset
 * and none of them reconciled.
 *
 * **The 0x421 velocities wrap 32 bits, exactly as `t_hit_by_bull`'s do.** One literal,
 * 0xfff80000, goes into the part's 0x1c, and `add r3, r3, #0xe0000` truncates to 0x00060000
 * for the object's. Written as the addition so the shared literal stays visible.
 *
 * **`obj->a10 = obj->field00->him` is stored and never read back here.** State 0x43a fetches
 * the opponent pointer again from the proc instead of using 0x44. Transcribed as it stands;
 * whether the victim's own `t_r_rabbit` reads it is a question about that routine, not this
 * one, and `t_r_rabbit` uses 0x44 as its own 0x140 countdown -- so the two uses cannot both
 * be live and this store looks dead.
 */
void set_proj_vel(MK3OBJ *obj);

long tl_kitana_bunny(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);
    MK3OBJ  *him;

    if (token == 0) {
        *mk3_frame(thread, frame + 1) = 0x3ff;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_cute_animality_start;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x3ff) {
        sans_repell_for_good(obj);
        set_noedge(obj);

        obj->field40 = 9;
        get_char_ani2(obj);

        MK3_SET_FIELD12(obj->field08,
                        (uint32_t)*(uint16_t *)(G_BYTES + 0xac)
                        - (uint32_t)GetFrameHeight(
                              *(uint32_t *)(uintptr_t)obj->field40)
                        - 9);

        obj->field1c = 8;

        *mk3_frame(thread, thread->frame + 1) = 0x41d;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x41d) {
        tsound_func(obj, 0x92);

        obj->field1c = 3;

        *mk3_frame(thread, thread->frame + 1) = 0x421;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x421) {
        obj->field08->field1c = 0xfff80000u;
        obj->field1c = 0xfff80000u + 0xe0000u;      /* wraps to 0x00060000 */
        set_proj_vel(obj);

        obj->field1c = obj->field08->field18;
        set_x_vel_player(obj);

        *mk3_frame(thread, frame + 1) = 0x42c;
        thread->fieldfc = 9;
        return 9;
    }

    if (token == 0x42c) {
        kill_and_stop_scrolling(obj);
        stop_me_player(obj);

        obj->field38 = (uint32_t)(uintptr_t)t_r_rabbit;
        takeover_him(obj);

        obj->field1c = 4;
        init_anirate(obj);

        obj->a10 = obj->field00->him;
        obj->field48 = 0xc0;

    } else if (token == 0x43a) {
        him = (MK3OBJ *)(void *)(uintptr_t)obj->field00->him;
        obj->field1c = him->field18;
        set_x_vel_player(obj);

        next_anirate(obj);

        obj->field48 = obj->field48 - 1;
        if ((long)obj->field48 <= 0) {
            death_blow_complete(obj);
            player_normpal(obj);

            return mk3_install(thread, (MK3THREADFUNC)t_wait_forever);
        }

    } else {
        return -3;
    }

    *mk3_frame(thread, thread->frame + 1) = 0x43a;
    thread->fieldfc = 1;
    return 1;
}

/* ----------------------------------------------------------------------- tl_lao_cheetah
 *
 * armv7 0x000a1ab0, 508 bytes.  **Complete.**
 *
 *      token == 0:      animality_tune(obj)
 *                       obj->field40 = a_cheetah
 *                       obj->a10 = 0x12
 *                       token := 0x490, descend into t_animal_morph
 *
 *      token == 0x490:  sans_repell_for_good(obj)
 *                       token := 0x493, park 0x18
 *
 *      token == 0x493:  tsound_func(obj, 0x95)
 *                       obj->field1c = 0x30000; towards_x_vel(obj)
 *                       obj->field1c = 7
 *                       token := 0x499, descend into t_mframew
 *
 *      token == 0x499:  obj->field38 = t_lion_mauled
 *                       takeover_him(obj)
 *                       obj->field1c = 0x20000; away_x_vel_him(obj)
 *                       obj->field1c = 6
 *                       token := 0x4a0, descend into t_mframew
 *
 *      token == 0x4a0:  stop_me_player(obj)
 *                       token := 0x4a3, park 8
 *
 *      token == 0x4a3:  stop_him(obj)
 *                       obj->field1c = 0x00050010
 *                       token := 0x4a7, descend into t_animate_a0_frames
 *
 *      token == 0x4a7:  obj->field40 = a_cheetah
 *                       obj->field54 = 5; find_part_a14(obj)
 *                       do_next_a9_frame(obj)
 *                       wfe_him(obj)
 *                       token := 0x4ae, park 0x28
 *
 *      token == 0x4ae:  obj->field40 = a_cheetah
 *                       frame[frame].handler = t_unmorph_and_exit
 *
 *      otherwise:       return -3
 *
 * **Eight states, and the third driver to maul with `t_lion_mauled`.** The lion, the polar bear
 * and the cheetah all hand the victim that same endless loop and all three end it with
 * `wfe_him` -- so one reaction serves three animals, which is the clearest case yet of the
 * pool.
 *
 * **It knocks the victim away as it bites**, `away_x_vel_him` at 0x20000 in the same state as
 * the handover, where the lion and the bear stop both fighters instead. So the cheetah's
 * mauling happens while the victim is still sliding.
 *
 * **`obj->field54 = 5` before `find_part_a14` is that routine's argument.** mkprop.c has the
 * same pair with a 3, which is what settles it -- the header's "where a computed word is
 * parked" and this are the same slot used as a parameter, and two files agreeing makes it the
 * interface rather than a coincidence.
 *
 * Sound 0x95 during the charge is the polar bear's sound, in the same position in the schedule.
 * So 0x95 is the run-up noise and 0x92 the large-animal noise, both shared.
 *
 * The dispatch is `ble` inside `ble` like `tl_cyrax_shark`'s, and `r8` carries three of the
 * eight tokens -- 0x499 into state 0x493's store, 0x4a3 into 0x4a0's -- each traced to its own
 * load.
 */
extern uint32_t a_cheetah[];                     /* 0x001772e8 */
void find_part_a14(MK3OBJ *obj);

long tl_lao_cheetah(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0x490) {
        sans_repell_for_good(obj);

        *mk3_frame(thread, frame + 1) = 0x493;
        thread->fieldfc = 0x18;
        return 0x18;
    }

    if (token == 0x493) {
        tsound_func(obj, 0x95);

        obj->field1c = 0x30000;
        towards_x_vel(obj);
        obj->field1c = 7;

        *mk3_frame(thread, thread->frame + 1) = 0x499;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x499) {
        obj->field38 = (uint32_t)(uintptr_t)t_lion_mauled;
        takeover_him(obj);

        obj->field1c = 0x20000;
        away_x_vel_him(obj);
        obj->field1c = 6;

        *mk3_frame(thread, thread->frame + 1) = 0x4a0;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x4a0) {
        stop_me_player(obj);

        *mk3_frame(thread, frame + 1) = 0x4a3;
        thread->fieldfc = 8;
        return 8;
    }

    if (token == 0x4a3) {
        stop_him(obj);

        obj->field1c = 0x00050010;

        *mk3_frame(thread, thread->frame + 1) = 0x4a7;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_animate_a0_frames;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x4a7) {
        obj->field40 = (uint32_t)(uintptr_t)a_cheetah;

        obj->field54 = 5;
        find_part_a14(obj);

        do_next_a9_frame(obj);
        wfe_him(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x4ae;
        thread->fieldfc = 0x28;
        return 0x28;
    }

    if (token == 0x4ae) {
        obj->field40 = (uint32_t)(uintptr_t)a_cheetah;

        return mk3_install(thread, (MK3THREADFUNC)t_unmorph_and_exit);
    }

    if (token != 0)
        return -3;

    animality_tune(obj);

    obj->field40 = (uint32_t)(uintptr_t)a_cheetah;
    obj->a10 = 0x12;

    *mk3_frame(thread, thread->frame + 1) = 0x490;
    thread->frame = thread->frame + 1;              /* push a level */
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_animal_morph;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}

/* ----------------------------------------------------------------------- tl_indian_wolf
 *
 * armv7 0x000a16d8, 556 bytes.  **Complete.**
 *
 *      token == 0:      animality_tune(obj)
 *                       obj->field1c = 2
 *                       obj->field40 = a_indiam_wolf
 *                       obj->a10 = 0x12
 *                       token := 0x7f1, descend into t_animal_morph
 *
 *      token == 0x7f1:  sans_repell_for_good(obj)
 *                       token := 0x7f3, park 0x18
 *
 *      token == 0x7f3:  tsound_func(obj, 0x95)
 *                       obj->field1c = 0x20000; towards_x_vel(obj)
 *                       obj->field1c = 5
 *                       token := 0x7f9, descend into t_mframew
 *
 *      token == 0x7f9:  obj->field38 = t_lion_mauled
 *                       takeover_him(obj)
 *                       obj->field1c = 0x20000; away_x_vel_him(obj)
 *                       obj->field1c = 0x00060002
 *                       token := 0x801, descend into t_animate_a0_frames
 *
 *      token == 0x801:  stop_me_player(obj)
 *                       token := 0x803, park 8
 *
 *      token == 0x803:  stop_him(obj)
 *                       obj->field1c = 0x00050010
 *                       token := 0x807, descend into t_animate_a0_frames
 *
 *      token == 0x807:  obj->field40 = a_indiam_wolf
 *                       obj->field54 = 4; find_part_a14(obj)
 *                       do_next_a9_frame(obj)
 *                       wfe_him(obj)
 *                       token := 0x80d, park 0x28
 *
 *      token == 0x80d:  tsound_func(obj, 0x27)
 *                       obj->field40 = a_indiam_wolf
 *                       obj->field1c = 5
 *                       token := 0x811, descend into t_backwards_ani
 *
 *      token == 0x811:  frame[frame].handler = t_animality_complete
 *
 *      otherwise:       return -3
 *
 * **`tl_lao_cheetah` with one state inserted and the ending written out.** Nine states against
 * the cheetah's eight: everything up to the mauling is identical -- same 0x18 park, same sound
 * 0x95, same 0x20000 charge, same `t_lion_mauled` handover with `away_x_vel_him` -- and the wolf
 * then animates TWICE through `t_animate_a0_frames`, 0x00060002 while the victim is still
 * moving and 0x00050010 after `stop_him`, where the cheetah does it once.
 *
 * **Fourth driver to use `t_lion_mauled`.** Lion, polar bear, cheetah, wolf: four animals, one
 * victim routine, all four ending it with `wfe_him`. The pool is not a tendency, it is how this
 * module is built.
 *
 * The ending is `t_unmorph_and_exit` spelled out, for the same reason as `tl_jax_lion`'s and
 * `tl_swat_dino`'s -- sound 0x27 and the 0x40 reset have to happen on the way into
 * `t_backwards_ani`, and the shared routine has nowhere to put them.
 *
 * **The dispatch is three levels deep** -- `ble`, then `ble` and `bgt` inside it -- with `r8`
 * reloaded twice and `sl` once. Four of the nine tokens are stored out of a register the
 * dispatch has already overwritten by the time the state runs, so each store was traced back to
 * the load that reaches it: 0x7f1 out of `sl`, 0x801 and 0x807 out of the two `r8` loads.
 *
 * The symbol is `_a_indiam_wolf`, spelled that way in the binary's own symbol table. Kept
 * verbatim -- correcting it would break the only link between this name and the data.
 */
extern uint32_t a_indiam_wolf[];                 /* 0x001771b0, sic */

long tl_indian_wolf(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0x7f1) {
        sans_repell_for_good(obj);

        *mk3_frame(thread, frame + 1) = 0x7f3;
        thread->fieldfc = 0x18;
        return 0x18;
    }

    if (token == 0x7f3) {
        tsound_func(obj, 0x95);

        obj->field1c = 0x20000;
        towards_x_vel(obj);
        obj->field1c = 5;

        *mk3_frame(thread, thread->frame + 1) = 0x7f9;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x7f9) {
        obj->field38 = (uint32_t)(uintptr_t)t_lion_mauled;
        takeover_him(obj);

        obj->field1c = 0x20000;
        away_x_vel_him(obj);
        obj->field1c = 0x00060002;

        *mk3_frame(thread, thread->frame + 1) = 0x801;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_animate_a0_frames;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x801) {
        stop_me_player(obj);

        *mk3_frame(thread, frame + 1) = 0x803;
        thread->fieldfc = 8;
        return 8;
    }

    if (token == 0x803) {
        stop_him(obj);

        obj->field1c = 0x00050010;

        *mk3_frame(thread, thread->frame + 1) = 0x807;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_animate_a0_frames;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x807) {
        obj->field40 = (uint32_t)(uintptr_t)a_indiam_wolf;

        obj->field54 = 4;
        find_part_a14(obj);

        do_next_a9_frame(obj);
        wfe_him(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x80d;
        thread->fieldfc = 0x28;
        return 0x28;
    }

    if (token == 0x80d) {
        tsound_func(obj, 0x27);

        obj->field40 = (uint32_t)(uintptr_t)a_indiam_wolf;
        obj->field1c = 5;

        *mk3_frame(thread, thread->frame + 1) = 0x811;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_backwards_ani;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x811)
        return mk3_install(thread, (MK3THREADFUNC)t_animality_complete);

    if (token != 0)
        return -3;

    animality_tune(obj);

    obj->field1c = 2;
    obj->field40 = (uint32_t)(uintptr_t)a_indiam_wolf;
    obj->a10 = 0x12;

    *mk3_frame(thread, thread->frame + 1) = 0x7f1;
    thread->frame = thread->frame + 1;              /* push a level */
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_animal_morph;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}

/* ----------------------------------------------------------------------- tl_kano_spider
 *
 * armv7 0x000a244c, 556 bytes.  **Complete.**
 *
 *      token == 0:      wfe_him(obj)
 *                       obj->field40 = 0x48; pose_him_a9(obj)
 *                       face_him_at_me(obj)
 *                       animality_tune(obj)
 *                       obj->field1c = 2
 *                       obj->field40 = a_kano_spider
 *                       obj->a10 = 0x40
 *                       token := 0x724, descend into t_animal_morph
 *
 *      token == 0x724:  sans_repell_for_good(obj)
 *                       tsound_func(obj, 0x26)
 *                       obj->field1c = 5
 *                       token := 0x729, descend into t_mframew
 *
 *      token == 0x729:  obj->field1c = ((MK3OBJ *)proc->him)->field24
 *                       if (obj->field1c == 0xb) {
 *                           obj->field1c = 0x20
 *                           obj->field20 = 0x20 - 0x20 = 0
 *                           multi_adjust_xy(obj)
 *                       }
 *                       -- the swap --
 *                       obj->field08 = proc->him
 *                       obj->field00 = proc->field00->field00
 *                       obj->field1c = 6; create_blood_proc(obj)
 *                       obj->field08 = saved; obj->field00 = saved
 *                       obj->field38 = t_spider_shake
 *                       takeover_him(obj)
 *                       token := 0x740, descend into t_spider_shake_jsrp
 *
 *      token == 0x740:  obj->field40 = a_kano_spider
 *                       obj->field54 = 3; find_part_a14(obj)
 *                       obj->field1c = 5
 *                       token := 0x746, descend into t_mframew
 *
 *      token == 0x746:  obj->field38 = t_collapse_on_ground
 *                       takeover_him(obj)
 *                       token := 0x74a, park 0x30
 *
 *      token == 0x74a:  tsound_func(obj, 0x27)
 *                       obj->field1c = 5
 *                       token := 0x74e, descend into t_mframew
 *
 *      token == 0x74e:  frame[frame].handler = t_animality_complete
 *
 *      otherwise:       return -3
 *
 * **The only driver with a per-character fixup, and it names one fighter by number.** State
 * 0x729 reads the OPPONENT's character number out of `proc->him->field24` and, if it is 0xb and
 * only then, shifts by 0x20/0 through `multi_adjust_xy` before drawing blood. So one character
 * is the wrong height for the spider's bite and the routine corrects for that one. Every other
 * driver in the file treats all opponents alike.
 *
 * **The blood goes on the OTHER fighter, through the two-field swap.** `obj->field08 = proc->him`
 * and `obj->field00 = proc->field00->field00`, call, restore both -- which is exactly what
 * `create_fx_for_him` earlier in this file does for `create_fx`. Second site for that idiom, and
 * the first written out inline rather than wrapped in a helper: so the wrapper exists because
 * `create_fx` has several callers and `create_blood_proc` here has one.
 *
 * **Two handovers, at opposite ends.** State 0 opens with `wfe_him` -- the victim is parked
 * forever before the animality even starts -- and 0x746 replaces that with
 * `t_collapse_on_ground` off pointer slot 0x000f385c. So the victim does nothing at all while
 * the spider works, and only falls over at the end.
 *
 * The morph is 0x40 passes, the longest in the file (every other driver uses 0x12 or 0xa), and
 * 0x48 is posed on the VICTIM through `pose_him_a9` -- the same animation number
 * `t_stung_by_scorpion` and `t_r_scared_of_monkey` pose on themselves. One frightened pose,
 * three routines, and this is the only one that puts it on the other fighter.
 *
 * `obj->field1c` holds the character number only long enough to be compared; both paths
 * overwrite it before it is read again. Transcribed in that order because the compare is what
 * the store is for.
 */
extern uint32_t a_kano_spider[];                 /* 0x00177218 */
void pose_him_a9(MK3OBJ *obj);
long t_collapse_on_ground(MK3THREAD *thread);    /* pointer slot 0x000f385c */

long tl_kano_spider(MK3THREAD *thread)
{
    MK3OBJ     *obj   = (MK3OBJ *)thread->proc;
    uint32_t    frame = thread->frame;
    uint32_t    token = *mk3_frame(thread, frame + 1);
    MK3OBJPROC *saved_proc;
    MK3OBJ     *saved_part;

    if (token == 0x724) {
        sans_repell_for_good(obj);
        tsound_func(obj, 0x26);

        obj->field1c = 5;

        *mk3_frame(thread, thread->frame + 1) = 0x729;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x729) {
        obj->field1c =
            ((MK3OBJ *)(void *)(uintptr_t)obj->field00->him)->field24;

        if (obj->field1c == 0xb) {
            obj->field1c = 0x20;
            obj->field20 = 0x20 - 0x20;
            multi_adjust_xy(obj);
        }

        saved_proc = obj->field00;
        saved_part = obj->field08;

        obj->field08 = (MK3OBJ *)(void *)(uintptr_t)saved_proc->him;
        obj->field00 = saved_proc->field00->field00;

        obj->field1c = 6;
        create_blood_proc(obj);

        obj->field08 = saved_part;
        obj->field00 = saved_proc;

        obj->field38 = (uint32_t)(uintptr_t)t_spider_shake;
        takeover_him(obj);

        *mk3_frame(thread, thread->frame + 1) = 0x740;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_spider_shake_jsrp;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x740) {
        obj->field40 = (uint32_t)(uintptr_t)a_kano_spider;

        obj->field54 = 3;
        find_part_a14(obj);

        obj->field1c = 5;

        *mk3_frame(thread, thread->frame + 1) = 0x746;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x746) {
        obj->field38 = (uint32_t)(uintptr_t)t_collapse_on_ground;
        takeover_him(obj);

        *mk3_frame(thread, frame + 1) = 0x74a;
        thread->fieldfc = 0x30;
        return 0x30;
    }

    if (token == 0x74a) {
        tsound_func(obj, 0x27);

        obj->field1c = 5;

        *mk3_frame(thread, thread->frame + 1) = 0x74e;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x74e)
        return mk3_install(thread, (MK3THREADFUNC)t_animality_complete);

    if (token != 0)
        return -3;

    wfe_him(obj);

    obj->field40 = 0x48;
    pose_him_a9(obj);

    face_him_at_me(obj);
    animality_tune(obj);

    obj->field1c = 2;
    obj->field40 = (uint32_t)(uintptr_t)a_kano_spider;
    obj->a10 = 0x40;

    *mk3_frame(thread, thread->frame + 1) = 0x724;
    thread->frame = thread->frame + 1;              /* push a level */
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_animal_morph;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------------------------ tl_sektor_bat
 *
 * armv7 0x000a3354, 584 bytes.  **Complete.**
 *
 *      token == 0:      animality_tune(obj)
 *                       center_around_him(obj)
 *                       obj->field40 = a_bat
 *                       obj->a10 = 0x12
 *                       token := 0x589, descend into t_animal_morph
 *
 *      token == 0x589:  sans_repell_for_good(obj)
 *                       set_noedge(obj)
 *                       obj->field1c = 3; init_anirate(obj)
 *                       obj->field1c = 0x20; ochar_sound(obj)
 *                       part->field20 = 0xffffe000
 *                       obj->field1c = 0xffffe000 + 0x82000 = 0x00080000
 *                       towards_x_vel(obj)
 *                       obj->field48 = q_bat_1
 *                       token := 0x598, descend into t_animate_till_a11_stop
 *
 *      token == 0x598:  stop_me_player(obj)
 *                       match_me_with_him(obj)
 *                       flip_multi(obj)
 *                       obj->field1c = ~0x18e            (-0x18f)
 *                       obj->field20 = ~0x3f             (-0x40)
 *                       multi_adjust_xy(obj)
 *                       obj->field1c = 0x20; ochar_sound(obj)
 *                       obj->field1c = 0xa0000; towards_x_vel(obj)
 *                       obj->field48 = q_bat_2
 *                       token := 0x5a7, descend into t_animate_till_a11
 *
 *      token == 0x5a7:  obj->field38 = t_r_bat_bite
 *                       takeover_him(obj)
 *                       obj->field00->field28 = part->field18
 *                       obj->field48 = q_bat_3
 *                       token := 0x5ad, descend into t_animate_till_a11
 *
 *      token == 0x5ad:  token := 0x5ae, park 0x40
 *
 *      token == 0x5ae:  stop_me_player(obj)
 *                       match_me_with_him(obj)
 *                       flip_multi(obj)
 *                       obj->field1c = ~0x5f             (-0x60)
 *                       obj->field20 = -0x60 - 0x9d = -0xfd
 *                       multi_adjust_xy(obj)
 *                       obj->field1c = 0x50000
 *                       part->field1c = 0x50000
 *                       obj->field48 = q_bat_4
 *                       token := 0x5bb, descend into t_animate_till_a11_stop
 *
 *      token == 0x5bb:  ground_player(obj)
 *                       obj->field40 = a_bat
 *                       frame[frame].handler = t_unmorph_and_exit
 *
 *      otherwise:       return -3
 *
 * **This is the routine the `q_bat_*` family exists for, and it uses all four in order.** Each
 * flying state puts one predicate in 0x48 and descends into `t_animate_till_a11`, which animates
 * until that predicate sets 0x5c. So the bat has four legs of flight ended by four different
 * conditions, not four different lengths:
 *
 *      state   predicate   the question                          then
 *      0x589   q_bat_1     is the opponent more than 0xff away   stop, reposition
 *      0x598   q_bat_2     is the opponent 0x20 or closer        stop, bite
 *      0x5a7   q_bat_3     more than 0xff away again             carry on
 *      0x5ae   q_bat_4     three or less off the ground          stop, land
 *
 * **That settles two readings taken earlier on faith.** `t_animate_till_a11` calls 0x48 through
 * a register, and here 0x48 is written with the addresses of four functions -- so 0x48 really is
 * a function pointer in this path, not a counter and not a halfword pair. And `q_bat_3` being
 * behaviourally identical to `q_bat_1` while sitting at a different address is now explained:
 * the two are used by two different states, and having the same test twice costs nothing.
 *
 * The two `_stop` variants and the two plain ones are chosen by whether the leg should leave the
 * bat moving: 0x589 and 0x5ae stop, 0x598 and 0x5a7 do not. Which is why both variants exist.
 *
 * **The 0x589 velocity wraps 32 bits, third site in the file.** The literal is 0xffffe000, it
 * goes into the part 0x20 as it stands, and `add r3, r3, #0x82000` truncates to 0x00080000 for
 * the object 0x1c. Same shape as `t_hit_by_bull` and `tl_kitana_bunny`.
 *
 * `obj->field00->field28 = part->field18` in state 0x5a7 is a third reading of proc 0x28 -- the
 * header calls it the shake target, `t_r_rabbit` counts frames in it, and this parks an x
 * velocity there. Recorded; none of the three is discarded.
 *
 * The two `multi_adjust_xy` offsets are each built from one literal, `mvn`/`mvn` in one state and
 * `mvn`/`subs` in the other -- so -0x18f/-0x40 come from two literals and -0x60/-0xfd from one.
 */
extern uint32_t a_bat[];                         /* 0x00177338 */
void ochar_sound(MK3OBJ *obj);

long tl_sektor_bat(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);

    if (token == 0x589) {
        sans_repell_for_good(obj);
        set_noedge(obj);

        obj->field1c = 3;
        init_anirate(obj);

        obj->field1c = 0x20;
        ochar_sound(obj);

        obj->field08->field20 = 0xffffe000u;
        obj->field1c = 0xffffe000u + 0x82000u;      /* wraps to 0x00080000 */
        towards_x_vel(obj);

        obj->field48 = (uint32_t)(uintptr_t)q_bat_1;

        *mk3_frame(thread, thread->frame + 1) = 0x598;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_animate_till_a11_stop;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x598) {
        stop_me_player(obj);
        match_me_with_him(obj);
        flip_multi(obj);

        obj->field1c = (uint32_t)~0x18eu;
        obj->field20 = (uint32_t)~0x3fu;
        multi_adjust_xy(obj);

        obj->field1c = 0x20;
        ochar_sound(obj);

        obj->field1c = 0xa0000;
        towards_x_vel(obj);

        obj->field48 = (uint32_t)(uintptr_t)q_bat_2;

        *mk3_frame(thread, thread->frame + 1) = 0x5a7;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_animate_till_a11;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x5a7) {
        obj->field38 = (uint32_t)(uintptr_t)t_r_bat_bite;
        takeover_him(obj);

        obj->field00->field28 = obj->field08->field18;

        obj->field48 = (uint32_t)(uintptr_t)q_bat_3;

        *mk3_frame(thread, thread->frame + 1) = 0x5ad;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_animate_till_a11;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x5ad) {
        *mk3_frame(thread, frame + 1) = 0x5ae;
        thread->fieldfc = 0x40;
        return 0x40;
    }

    if (token == 0x5ae) {
        stop_me_player(obj);
        match_me_with_him(obj);
        flip_multi(obj);

        obj->field1c = (uint32_t)~0x5fu;
        obj->field20 = (uint32_t)(~0x5fu - 0x9du);
        multi_adjust_xy(obj);

        obj->field1c          = 0x50000;
        obj->field08->field1c = 0x50000;

        obj->field48 = (uint32_t)(uintptr_t)q_bat_4;

        *mk3_frame(thread, thread->frame + 1) = 0x5bb;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_animate_till_a11_stop;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x5bb) {
        ground_player(obj);

        obj->field40 = (uint32_t)(uintptr_t)a_bat;

        return mk3_install(thread, (MK3THREADFUNC)t_unmorph_and_exit);
    }

    if (token != 0)
        return -3;

    animality_tune(obj);
    center_around_him(obj);

    obj->field40 = (uint32_t)(uintptr_t)a_bat;
    obj->a10 = 0x12;

    *mk3_frame(thread, thread->frame + 1) = 0x589;
    thread->frame = thread->frame + 1;              /* push a level */
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_animal_morph;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* ----------------------------------------------------------------------- tl_sonya_eagle
 *
 * armv7 0x000a21e8, 612 bytes.  **Complete.**
 *
 *      token == 0:      animality_tune(obj)
 *                       obj->field1c = 2
 *                       obj->field40 = a_sonya_hawk
 *                       obj->a10 = 1
 *                       token := 0x766, descend into t_animal_morph
 *
 *      token == 0x766:  sans_repell_for_good(obj)
 *                       obj->field1c = 5; init_anirate(obj)
 *                       obj->field1c = 0xffffe000
 *                       part->field20 = 0xffffe000
 *                       kill_and_stop_scrolling(obj)
 *                       -- falls into the 0x771 tail --
 *
 *      token == 0x771:  next_anirate(obj)
 *                       distance_off_ground(obj)
 *                       if (obj->field1c > 0x37) {
 *                           stop_me_player(obj)
 *                           -- falls into the 0x77a tail --
 *                       }
 *                       -- falls into the 0x771 tail --
 *
 *      token == 0x77a:  obj->field1c = 0xa000; towards_x_vel(obj)
 *                       next_anirate(obj)
 *                       get_x_dist(obj)
 *                       if (obj->field28 <= 8) {
 *                           stop_me_player(obj)
 *                           obj->a10 = 0x30
 *                           token := 0x784, descend into t_next_anirate_a10
 *                       }
 *                       -- falls into the 0x77a tail --
 *
 *      token == 0x784:  obj->field1c = 0xffff0000
 *                       part->field20 = 0xffff0000
 *                       ((MK3OBJ *)proc->him)->field20 = obj->field1c
 *                       obj->a10 = 0x18
 *                       token := 0x78c, descend into t_next_anirate_a10
 *
 *      token == 0x78c:  stop_me_player(obj)
 *                       stop_him(obj)
 *                       obj->field1c = death_scream
 *                       call_a0_for_him(obj)
 *                       token := 0x792, park 0x30
 *
 *      token == 0x792:  obj->field1c = 0x1b; create_fx_for_him(obj)
 *                       obj->field48 = 0x0008000c; shake_a11(obj)
 *                       tsound_func(obj, 0x24)
 *                       tsound_func(obj, 0x25)
 *                       token := 0x79b, park 0x20
 *
 *      token == 0x79b:  tsound_func(obj, 0x24)
 *                       tsound_func(obj, 0x25)
 *                       token := 0x79f, park 0x10
 *
 *      token == 0x79f:  death_blow_complete(obj)
 *                       frame[frame].handler = t_wait_forever
 *
 *      otherwise:       return -3
 *
 * **The eagle climbs, then closes, then carries -- three loops with three different exit
 * tests, and this is the only driver in the file with two arrival conditions.** 0x771 rises
 * until `distance_off_ground` puts more than 0x37 in 0x1c; 0x77a then flies at the opponent
 * until `get_x_dist` puts 8 or less in 0x28; and only then does the carrying begin. The bat
 * does the same thing with four `q_bat_*` predicates called through 0x48; this writes both
 * tests inline. Two routines, one problem, two spellings -- and the bat's is the reusable one.
 *
 * **This is the caller `create_fx_for_him` was written for.** That helper (32 bytes, earlier in
 * this file) swaps the object 0x00 and 0x08 for the other fighter, calls `create_fx`, and puts
 * both back; state 0x792 is the only place in the module that uses it, with effect 0x1b. So the
 * helper exists for exactly one call site, which is worth knowing before assuming it is general.
 *
 * **And it is the caller that shows the 0x1c handover mechanism carrying a plain routine.**
 * `obj->field1c = death_scream; call_a0_for_him(obj)` makes the OTHER fighter scream -- the
 * third of the three ways this engine runs code on the opponent, and the first site measured
 * where what is handed across is an ordinary helper rather than a thread handler.
 *
 * **The morph is one pass.** Every other driver in the file uses 0x12, the spider uses 0x40 and
 * the wasp 0xa; the eagle uses 1, so Sonya barely changes shape before flying.
 *
 * The two rise velocities are separate literals, 0xffffe000 for the climb and 0xffff0000 for
 * the carry, and the carry writes the SAME value into three places: the object 0x1c, the part
 * 0x20 and the opponent 0x20. Which is how the victim comes up with the bird.
 *
 * **The shake pair is 0x0008000c, asymmetric.** Fourth asymmetric site in the tree, after
 * mkstat.c 0x00030008 and 0x0009000e and `t_r_bat_bite` 0x00060008 -- and the first where the
 * second half is the larger of the two.
 *
 * Sounds 0x24 and 0x25 are played as a pair twice, 0x20 frames apart. Fourth spelling of that
 * pair in the module.
 */
extern uint32_t a_sonya_hawk[];                  /* 0x001772b0 */

long tl_sonya_eagle(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);
    uint32_t next;

    if (token == 0x784) {
        obj->field1c          = 0xffff0000u;
        obj->field08->field20 = 0xffff0000u;
        ((MK3OBJ *)(void *)(uintptr_t)obj->field00->him)->field20 = obj->field1c;

        obj->a10 = 0x18;

        *mk3_frame(thread, thread->frame + 1) = 0x78c;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_next_anirate_a10;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x78c) {
        stop_me_player(obj);
        stop_him(obj);

        obj->field1c = (uint32_t)(uintptr_t)death_scream;
        call_a0_for_him(obj);

        *mk3_frame(thread, frame + 1) = 0x792;
        thread->fieldfc = 0x30;
        return 0x30;
    }

    if (token == 0x792) {
        obj->field1c = 0x1b;
        create_fx_for_him(obj);

        obj->field48 = 0x0008000c;
        shake_a11(obj);

        tsound_func(obj, 0x24);
        tsound_func(obj, 0x25);

        *mk3_frame(thread, frame + 1) = 0x79b;
        thread->fieldfc = 0x20;
        return 0x20;
    }

    if (token == 0x79b) {
        tsound_func(obj, 0x24);
        tsound_func(obj, 0x25);

        *mk3_frame(thread, frame + 1) = 0x79f;
        thread->fieldfc = 0x10;
        return 0x10;
    }

    if (token == 0x79f) {
        death_blow_complete(obj);

        return mk3_install(thread, (MK3THREADFUNC)t_wait_forever);
    }

    if (token == 0) {
        animality_tune(obj);

        obj->field1c = 2;
        obj->field40 = (uint32_t)(uintptr_t)a_sonya_hawk;
        obj->a10 = 1;

        *mk3_frame(thread, thread->frame + 1) = 0x766;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_animal_morph;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x766) {
        sans_repell_for_good(obj);

        obj->field1c = 5;
        init_anirate(obj);

        obj->field1c          = 0xffffe000u;
        obj->field08->field20 = 0xffffe000u;

        kill_and_stop_scrolling(obj);

        next = 0x771;

    } else if (token == 0x771) {
        next_anirate(obj);
        distance_off_ground(obj);

        if ((long)obj->field1c > 0x37) {
            stop_me_player(obj);
            next = 0x77a;
        } else {
            next = 0x771;
        }

    } else if (token == 0x77a) {
        obj->field1c = 0xa000;
        towards_x_vel(obj);

        next_anirate(obj);
        get_x_dist(obj);

        if ((long)obj->field28 <= 8) {
            stop_me_player(obj);

            obj->a10 = 0x30;

            *mk3_frame(thread, thread->frame + 1) = 0x784;
            thread->frame = thread->frame + 1;      /* push a level */
            mk3_frame(thread, thread->frame)[1] =
                (uint32_t)(uintptr_t)t_next_anirate_a10;
            *mk3_frame(thread, thread->frame + 1) = 0;
            return 0;
        }

        next = 0x77a;

    } else {
        return -3;
    }

    *mk3_frame(thread, thread->frame + 1) = next;
    thread->fieldfc = 1;
    return 1;
}


/* ------------------------------------------------------------------------ tl_jade_kitty
 *
 * armv7 0x000a3740, 760 bytes.  **Complete.**  The largest routine in this file, and the
 * last one written.
 *
 *      token == 0:      sans_repell_for_good(obj)
 *                       token := 0x372, descend into t_cute_animality_start
 *
 *      token == 0x372:  obj->field40 = 0xc; get_char_ani2(obj)
 *                       part->field2c = *(long *)obj->field40
 *                       obj->field20 = 0
 *                       obj->field1c = 0x60
 *                       multi_adjust_xy(obj)
 *                       part->y12 = (uint16_t)*(short *)(G + 0xac)
 *                                   - GetFrameHeight(part->field2c)
 *                       obj->field00->field28 = obj->field40
 *                       obj->field1c = 3
 *                       token := 0x397, descend into t_mframew
 *
 *      token == 0x397:  obj->field40 = obj->field00->field28
 *                       obj->field1c = 3
 *                       token := 0x39a, descend into t_mframew
 *
 *      token == 0x39a:  tsound_func(obj, 0x92)
 *                       obj->field1c = 3
 *                       token := 0x39e, descend into t_mframew
 *
 *      token == 0x39e:  obj->field48 = obj->field40
 *                       obj->field1c = 3
 *                       token := 0x3a2, descend into t_mframew
 *
 *      token == 0x3a2:  obj->field38 = t_hair_spun
 *                       takeover_him(obj)
 *                       token := 0x3a6, descend into t_kitty_spin
 *
 *      token == 0x3a6:  token := 0x3a7, descend into t_kitty_spin
 *
 *      token == 0x3a7:  token := 0x3a8, descend into t_kitty_spin
 *
 *      token == 0x3a8:  token := 0x3aa, park 0x10
 *
 *      token == 0x3aa:  obj->field1c = 3
 *                       token := 0x3ad, descend into t_mframew
 *
 *      token == 0x3ad:  obj->field40 = obj->field00->field28
 *                       obj->field1c = 5
 *                       token := 0x3b0, descend into t_mframew
 *
 *      token == 0x3b0:  death_blow_complete(obj)
 *                       -- falls into the 0x3b6 body --
 *
 *      token == 0x3b6:  obj->field40 = obj->field00->field28
 *                       obj->field1c = 5
 *                       token := 0x3b6, descend into t_mframew
 *
 *      otherwise:       return -3
 *
 * **Thirteen states, and it is the routine that finally explains proc 0x28.** State 0x372 parks
 * the pointer `get_char_ani2` produced into `proc->field28`, and three later states -- 0x397,
 * 0x3ad and 0x3b6 -- read it straight back into 0x40 before descending. So proc 0x28 is doing
 * here exactly what 0x44 does in `tl_mileena_skunk`: **it is the saved copy of the 0x40 cursor**,
 * kept because `t_mframew` advances 0x40 and leaves it past the end.
 *
 * That is the fourth reading of that offset, and the first that explains why a driver would want
 * it. The header calls proc 0x28 the shake target, `t_r_rabbit` counts frames in it,
 * `tl_sektor_bat` parks an x velocity there, and this parks a pointer. **Four uses, one word, and
 * nothing in the tree reconciles them** -- but every one of the four is a place to put something
 * that has to survive a call.
 *
 * **0x48 is the same cursor again, for `t_kitty_spin`.** State 0x39e copies 0x40 into 0x48, and
 * `t_kitty_spin` (64 bytes, earlier in this file) does nothing but `obj->field40 = obj->field48`
 * and a three-frame wait. So the spin routine is a cursor RESET with a wait attached, and the
 * three consecutive descents into it at 0x3a2/0x3a6/0x3a7 are three spins from the same starting
 * frame -- which is what makes them a repeat rather than a continuation.
 *
 * **Fifth spelling of the ground placement, and the first with no inset at all.** `ldrh` on
 * G + 0xac like `tl_kitana_bunny`, `GetFrameHeight` like `ground_ob`, and then the difference is
 * stored straight into 0x12 with no nine subtracted. Five routines in the tree place a body on
 * the floor -- `ground_ob`, `tl_reptile_monkey`, mkstat.c `t_turn_into_a_baby`,
 * `tl_kitana_bunny` and this -- with three ways of measuring the height and two of applying an
 * inset, in every combination but one. Still unreconciled, and now demonstrably not an oversight
 * in any single routine.
 *
 * **It never ends.** State 0x3b0 calls `death_blow_complete` and falls into 0x3b6, which sets its
 * own token before descending -- so the kitty animates from the same frame forever afterwards.
 * Same shape as `tl_reptile_monkey` and `t_lion_mauled`; the round ending is what stops it.
 *
 * `fp` is used as a zero register through the whole 0x372 state -- the 0x20 store, the return
 * value and the cleared token slot all come from it -- which is why that state has three
 * different-looking writes of the same nothing.
 */
long t_hair_spun(MK3THREAD *thread);             /* pointer slot 0x000f346c */

long tl_jade_kitty(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);
    uint32_t next;

    if (token == 0) {
        sans_repell_for_good(obj);

        *mk3_frame(thread, frame + 1) = 0x372;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_cute_animality_start;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x3a2 || token == 0x3a6 || token == 0x3a7) {
        if (token == 0x3a2) {
            obj->field38 = (uint32_t)(uintptr_t)t_hair_spun;
            takeover_him(obj);
            next = 0x3a6;
        } else if (token == 0x3a6) {
            next = 0x3a7;
        } else {
            next = 0x3a8;
        }

        *mk3_frame(thread, thread->frame + 1) = next;
        thread->frame = thread->frame + 1;          /* push a level */
        mk3_frame(thread, thread->frame)[1] =
            (uint32_t)(uintptr_t)t_kitty_spin;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x3a8) {
        *mk3_frame(thread, frame + 1) = 0x3aa;
        thread->fieldfc = 0x10;
        return 0x10;
    }

    if (token == 0x372) {
        obj->field40 = 0xc;
        get_char_ani2(obj);

        obj->field08->field2c = *(uint32_t *)(uintptr_t)obj->field40;

        obj->field20 = 0;
        obj->field1c = 0x60;
        multi_adjust_xy(obj);

        MK3_SET_FIELD12(obj->field08,
                        (uint32_t)*(uint16_t *)(G_BYTES + 0xac)
                        - (uint32_t)GetFrameHeight(obj->field08->field2c));

        obj->field00->field28 = obj->field40;

        obj->field1c = 3;
        next = 0x397;

    } else if (token == 0x397) {
        obj->field40 = obj->field00->field28;

        obj->field1c = 3;
        next = 0x39a;

    } else if (token == 0x39a) {
        tsound_func(obj, 0x92);

        obj->field1c = 3;
        next = 0x39e;

    } else if (token == 0x39e) {
        obj->field48 = obj->field40;

        obj->field1c = 3;
        next = 0x3a2;

    } else if (token == 0x3aa) {
        obj->field1c = 3;
        next = 0x3ad;

    } else if (token == 0x3ad) {
        obj->field40 = obj->field00->field28;

        obj->field1c = 5;
        next = 0x3b0;

    } else if (token == 0x3b0 || token == 0x3b6) {
        if (token == 0x3b0)
            death_blow_complete(obj);

        obj->field40 = obj->field00->field28;

        obj->field1c = 5;
        next = 0x3b6;

    } else {
        return -3;
    }

    *mk3_frame(thread, thread->frame + 1) = next;
    thread->frame = thread->frame + 1;               /* push a level */
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}
