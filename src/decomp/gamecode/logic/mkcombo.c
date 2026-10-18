/*
 * mkcombo.c -- gamecode/logic/mkcombo.c, decompiled.
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



/* t_comb9 -- armv7 0x00032a4c, 60 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field1c = 0x4
 *      frame[frame].handler = t_mframew
 *      frame[frame+1].w0 = 0
 *
 * The handler comes through the pointer slot at 0x000f37cc rather than as a
 * link-time constant, so it lives in another translation unit. */
long t_mframew(struct MK3THREAD *thread);

long t_comb9(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field1c = 0x4;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_mframew);
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
 * Added by a later sweep -- tools/sweep.py, running the same
 * readers again after one of them learned something. Each still
 * refuses anything it cannot account for instruction by
 * instruction; see tools/pushfn.py and tools/leaffn.py.
 * -------------------------------------------------------------------- */

long t_combo_2_late(MK3THREAD *thread);
long t_combo_exit(MK3THREAD *thread);

/* t_combo_miss -- armv7 0x000329b4, 76 bytes.  **Complete.**
 *
 *      token == 0:
 *          park(token 0x844, duration 0x10)
 *      token == 0x844:
 *          frame[frame].handler = t_combo_2_late
 *      otherwise:  return -3
 */
long t_combo_miss(MK3THREAD *thread)
{
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x844;
        thread->fieldfc = 0x10;
        return 0x10;
    }

    if (token != 0x844)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_combo_2_late);
}

/* --------------------------------------------------------------------
 * Added by a later sweep -- tools/sweep.py, running the same
 * readers again after one of them learned something. Each still
 * refuses anything it cannot account for instruction by
 * instruction; see tools/pushfn.py and tools/leaffn.py.
 * -------------------------------------------------------------------- */

/* t_combo_2_late -- armv7 0x00032a00, 76 bytes.  **Complete.**
 *
 *      token == 0:
 *          park(token 0x847, duration 0x5)
 *      token == 0x847:
 *          frame[frame].handler = t_combo_exit
 *      otherwise:  return -3
 */
long t_combo_2_late(MK3THREAD *thread)
{
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        *mk3_frame(thread, thread->frame + 1) = 0x847;
        thread->fieldfc = 0x5;
        return 0x5;
    }

    if (token != 0x847)
        return -3;

    return mk3_install(thread, (MK3THREADFUNC)t_combo_exit);
}


/* a9_combo_ani -- armv7 0x00032d40, 44 bytes.  **Complete.**
 *
 *      r = obj->field40
 *      obj->field54 = r & 0xff
 *      n = (int32_t)(uint16_t)r >> 8
 *      obj->field40 = n
 *      if (n & 0x80) { obj->field40 = n & ~0x80; find_ani2_part_a14(obj) }
 *      else find_ani_part_a14(obj)
 *
 * **0x40 arrives packed and leaves unpacked.** The low byte is a rate and goes
 * to 0x54; bits 8 to 15 are an animation number and go back into 0x40. Bit 7 of
 * that number is not part of it -- it selects the second animation routine and
 * is stripped before the call, so a number with it set means "the other
 * lookup".
 *
 * The unpacked number is stored before the bit is tested, so on the second
 * path 0x40 is written twice. */
void find_ani_part_a14(MK3OBJ *obj);
void find_ani2_part_a14(MK3OBJ *obj);

void a9_combo_ani(MK3OBJ *obj)
{
    uint32_t packed = obj->field40;
    int32_t  n;

    obj->field54 = packed & 0xffu;
    n = (int32_t)(uint32_t)(uint16_t)packed >> 8;
    obj->field40 = (uint32_t)n;

    if ((n & 0x80) != 0) {
        obj->field40 = (uint32_t)(n & ~0x80);
        find_ani2_part_a14(obj);
    } else {
        find_ani_part_a14(obj);
    }
}

/* clear_combo_butn -- armv7 0x0003269c, 60 bytes.  **Complete.**
 *
 *      obj->field24 = 0
 *      base = G + 0x3d0 + obj->field00->field08 * 2
 *      *(uint16_t *)(base + 0x00) = 0
 *      *(uint16_t *)(base + 0x04) = 0
 *      *(uint16_t *)(base + 0x08) = 0
 *      *(uint16_t *)(base + 0x0c) = 0
 *      *(uint16_t *)(base + 0x10) = 0
 *      *(uint16_t *)(base + 0x14) = 0
 *      obj->field20 = base + 0x14
 *
 * **Six halfwords, four bytes apart, offset by two per player.** The stride is
 * a word but the writes are halfwords, and the base is shifted by the strength
 * index times two -- so player zero clears the low half of six consecutive
 * words and player one the high half. The same per-player packing the button
 * masks use, applied to storage instead of to constants.
 *
 * Every store reads obj->field24 back rather than reusing the zero register,
 * which is why the zero is written to 0x24 first. A leaf: no frame at all.
 *
 * 0x20 is left pointing at the LAST of the six, not past it. */
void clear_combo_butn(MK3OBJ *obj)
{
    char *base;

    obj->field24 = 0;
    base = G_BYTES + 0x3d0 + obj->field00->field08 * 2;

    *(uint16_t *)(base + 0x00) = (uint16_t)obj->field24;
    *(uint16_t *)(base + 0x04) = (uint16_t)obj->field24;
    *(uint16_t *)(base + 0x08) = (uint16_t)obj->field24;
    *(uint16_t *)(base + 0x0c) = (uint16_t)obj->field24;
    *(uint16_t *)(base + 0x10) = (uint16_t)obj->field24;
    *(uint16_t *)(base + 0x14) = (uint16_t)obj->field24;

    obj->field20 = (uint32_t)(uintptr_t)(base + 0x14);
}

/* t_comb8 -- armv7 0x00032918, 64 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      ComboNum += 1
 *      frame[frame].handler = t_comb2
 *      frame[frame+1].w0 = 0
 *
 * The only thing it does besides installing is bump a global counter, read and
 * written straight through its own address. */
extern long ComboNum;                      /* 0x00165690 */
long t_comb2(MK3THREAD *thread);

long t_comb8(MK3THREAD *thread)
{
    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    ComboNum += 1;
    return mk3_push_handler(thread, (MK3THREADFUNC)t_comb2);
}

/* t_combo_exit -- armv7 0x00032d6c, 72 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      obj->field40 = *(uint32_t *)((char *)obj->field48 + 0xc)
 *      a9_combo_ani(obj)
 *      frame[frame].handler = t_comb9
 *      frame[frame+1].w0 = 0
 *
 * **0x48 is a pointer here and its 0xc holds the packed word a9_combo_ani
 * unpacks.** So the combo's exit animation is not a constant in the routine: it
 * comes out of whatever structure 0x48 points at, four fields in, and is taken
 * apart into a rate and a number by the routine above. */
long t_comb9(MK3THREAD *thread);

long t_combo_exit(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    obj->field40 = *(uint32_t *)((char *)(void *)(uintptr_t)obj->field48
                                 + 0xc);
    a9_combo_ani(obj);
    return mk3_push_handler(thread, (MK3THREADFUNC)t_comb9);
}


/* t_comb2 -- armv7 0x00032958, 92 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      clear_combo_butn(obj)
 *      obj->a10 = obj->field54
 *      obj->field40 = *(uint32_t *)((char *)obj->field48 + 4)
 *      if (obj->field40 == 0x1111) {
 *          h = *(uint32_t *)((char *)obj->field48 + 0x14)
 *          obj->field1c = h
 *      } else {
 *          h = t_comba
 *      }
 *      frame[frame].handler = h
 *      frame[frame+1].w0 = 0
 *
 * **The handler can come out of the table instead of out of the code.** The
 * word at 0x48 + 4 is compared against 0x1111, and on a match the routine to
 * install is read from 0x48 + 0x14 -- the same value it also puts in 0x1c.
 * Anything else installs t_comba. So 0x1111 is a sentinel meaning "this entry
 * names its own continuation".
 *
 * The two arms share the install, which is why the loaded handler and the
 * constant one both arrive in r2. */
long t_comba(MK3THREAD *thread);

long t_comb2(MK3THREAD *thread)
{
    MK3OBJ  *obj = (MK3OBJ *)thread->proc;
    char    *entry;
    uint32_t h;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    clear_combo_butn(obj);
    obj->a10 = obj->field54;

    entry = (char *)(void *)(uintptr_t)obj->field48;
    obj->field40 = *(uint32_t *)(entry + 4);

    if (obj->field40 == 0x1111) {
        h = *(uint32_t *)(entry + 0x14);
        obj->field1c = h;
    } else {
        h = (uint32_t)(uintptr_t)t_comba;
    }

    mk3_frame(thread, thread->frame)[1] = h;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}

/* t_combj -- armv7 0x00032810, 104 bytes.  **Complete.**
 *
 *      token == 0:      obj->field00->field20 = obj->field1c
 *                       token := 0x7f3, park 3
 *
 *      token == 0x7f3:  obj->field1c = obj->field00->field20 - 3
 *                       frame[frame].handler = t_comb1
 *                       frame[frame+1].w0 = 0
 *
 *      otherwise:       return -3
 *
 * **The park duration is built out of the token.** 0x7f3 goes into the slot and
 * then `sub r2, r2, #0x7f0` leaves 3, which is both the duration written to
 * 0xfc and the value returned. One constant doing two jobs, and the 0x7f0 is
 * chosen so the subtraction lands on the wait.
 *
 * **It uses the proc's 0x20 as a save slot across the park** and takes three
 * back off it on the way out -- so whatever 0x1c held is returned three lower
 * than it went in, which is the same three as the wait.
 *
 * The 0x7f3 state installs with no guard in front of it, so mk3_install and
 * not mk3_push_handler. tools/instck.py caught this one. */
long t_comb1(MK3THREAD *thread);

long t_combj(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field00->field20 = obj->field1c;
        *mk3_frame(thread, thread->frame + 1) = 0x7f3;
        thread->fieldfc = 0x7f3 - 0x7f0;
        return 0x7f3 - 0x7f0;
    }

    if (token != 0x7f3)
        return -3;

    obj->field1c = obj->field00->field20 - 3;
    return mk3_install(thread, (MK3THREADFUNC)t_comb1);
}

/* t_process_combo_table -- armv7 0x0003279c, 116 bytes.  **Complete.**
 *
 *      if (frame[frame+1].w0 != 0) return -3
 *      if (ComboNum != 0) {
 *          n = ComboNum
 *          ComboNum = 0
 *          ComboPrint = 1.5f
 *          ComboPrintNum = n
 *      }
 *      obj->a10 = 0
 *      obj->field20 = obj->field00->field18 = 0x116
 *      clear_combo_butn(obj)
 *      frame[frame].handler = t_comb0
 *      frame[frame+1].w0 = 0
 *
 * **This is where a finished combo becomes something on screen.** The counter
 * t_comb8 has been bumping is moved into ComboPrintNum, zeroed, and ComboPrint
 * is set to 0x3fc00000 -- **1.5 as a float**, so it is a display timer in
 * seconds rather than a flag or a frame count. The three globals sit
 * consecutively at 0x165688, 0x16568c and 0x165690.
 *
 * The move is skipped entirely when the counter is zero, so a combo of nothing
 * prints nothing and the timer is not restarted. */
extern float ComboPrint;                   /* 0x00165688 */
extern long  ComboPrintNum;                /* 0x0016568c */
long t_comb0(MK3THREAD *thread);

long t_process_combo_table(MK3THREAD *thread)
{
    MK3OBJ *obj = (MK3OBJ *)thread->proc;
    long    n;

    if (*mk3_frame(thread, thread->frame + 1) != 0)
        return -3;

    n = ComboNum;
    if (n != 0) {
        ComboNum = 0;
        ComboPrint = 1.5f;
        ComboPrintNum = n;
    }

    obj->a10 = 0;
    obj->field20 = 0x116;
    obj->field00->field18 = 0x116;
    clear_combo_butn(obj);

    return mk3_push_handler(thread, (MK3THREADFUNC)t_comb0);
}


/* t_comb1 -- armv7 0x00032878, 160 bytes.  **Complete.**
 *
 *      token == 0:      obj->field00->field20 = obj->field1c
 *                       token := 0x7f9, park 1
 *
 *      token == 0x7f9:  combo_scan_a11(obj)
 *                       if (obj->field5c != 0)
 *                           frame[frame].handler = t_comb8
 *                       else {
 *                           obj->field1c = obj->field00->field20 - 1
 *                           if (obj->field1c > 0)
 *                               frame[frame].handler = t_comb1
 *                           else
 *                               frame[frame].handler = t_combo_2_late
 *                       }
 *                       frame[frame+1].w0 = 0
 *
 *      otherwise:       return -3
 *
 * **The window is a countdown kept in the opponent's proc.** 0x1c goes into
 * proc+0x20 on the way in and comes back one lower every frame; while it is
 * positive the routine reinstalls itself, and when it runs out it hands over to
 * t_combo_2_late. So the same field t_combj uses as a save slot is used here as
 * a per-frame counter, and the name of the exit says what running out means.
 *
 * A hit from combo_scan_a11 -- answered in 0x5c, like the q_ family -- jumps
 * straight to t_comb8, which is the routine that bumps ComboNum. That is the
 * link between recognising a combo and counting it.
 *
 * All three installs sit on the 0x7f9 path, so none of them may carry the
 * state-0 refusal. */
long combo_scan_a11(MK3OBJ *obj);
long t_comb8(MK3THREAD *thread);
long t_combo_2_late(MK3THREAD *thread);

long t_comb1(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);

    if (token == 0) {
        obj->field00->field20 = obj->field1c;
        *mk3_frame(thread, thread->frame + 1) = 0x7f9;
        thread->fieldfc = 1;
        return 1;
    }

    if (token != 0x7f9)
        return -3;

    combo_scan_a11(obj);
    if (obj->field5c != 0)
        return mk3_install(thread, (MK3THREADFUNC)t_comb8);

    obj->field1c = obj->field00->field20 - 1;
    if ((long)obj->field1c > 0)
        return mk3_install(thread, (MK3THREADFUNC)t_comb1);

    return mk3_install(thread, (MK3THREADFUNC)t_combo_2_late);
}

/* t_comb0 -- armv7 0x00032c90, 176 bytes.  **Complete.**
 *
 *      token == 0:      n = *(uint8_t *)obj->field48
 *                       obj->field1c = (n * 3) >> 1
 *                       am_i_joy(obj)
 *                       if (obj->field5c != 0)
 *                           frame[frame].handler = t_combj
 *                       else { token := 0x7ea, park 4 }
 *
 *      token == 0x7ea:  obj->field1c = (int16)*(G + 0x44c)
 *                       if (obj->field1c > 1)
 *                           frame[frame].handler = t_comb8
 *                       else
 *                           frame[frame].handler = t_combo_exit
 *
 *      otherwise:       return -3
 *
 * **The window is one and a half times a BYTE out of the table.** 0x48 points
 * at the entry and its first byte is the length; times three, shifted right one,
 * gives the number of frames -- the same times-three-over-two shape
 * stick_look_lr uses for its deadline, here without the signed correction
 * because the value came out of a `ldrb` and cannot be negative.
 *
 * **A person and the machine take different routes from the same state.**
 * am_i_joy coming back set installs t_combj immediately; clear parks four frames
 * and then decides on a halfword at G + 0x44c -- over one goes to t_comb8 and
 * counts a combo, one or less goes to t_combo_exit. */
long am_i_joy(MK3OBJ *obj);
long t_combj(MK3THREAD *thread);
long t_combo_exit(MK3THREAD *thread);

long t_comb0(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    uint32_t n;

    if (token == 0) {
        n = *(uint8_t *)(void *)(uintptr_t)obj->field48;
        obj->field1c = (n * 3) >> 1;

        am_i_joy(obj);
        if (obj->field5c != 0)
            return mk3_install(thread, (MK3THREADFUNC)t_combj);

        *mk3_frame(thread, thread->frame + 1) = 0x7ea;
        thread->fieldfc = 4;
        return 4;
    }

    if (token != 0x7ea)
        return -3;

    obj->field1c = (uint32_t)(int32_t)*(int16_t *)(G_BYTES + 0x44c);
    if ((long)obj->field1c > 1)
        return mk3_install(thread, (MK3THREADFUNC)t_comb8);

    return mk3_install(thread, (MK3THREADFUNC)t_combo_exit);
}


/* -------------------------------------------------------------- combo_scan_a11
 *
 * armv7 0x000326d8, 196 bytes.  **Complete.**
 *
 * The recogniser. `t_comb1` calls it once a frame and reads the answer out of
 * 0x5c, and this is what decides whether the buttons pressed so far are a
 * combo.
 *
 *      base = obj->field48                     ; the list, saved for the exit
 *      e    = base
 *      for (;;) {
 *          k = *(uint8_t *)(e + 8)
 *          obj->field1c = obj->field54 = k
 *          p = *(uint32_t *)(last_switch_ram + k * 8 + 4)
 *          if (obj->field00->field08 != 0) p += 2
 *          obj->field20 = *(uint16_t *)p
 *          if (obj->field20 != 0) {
 *              ring = G + 0x3d0 + obj->field00->field08 * 2
 *              seen = 0
 *              for (i = 0; i != 5; i++) {
 *                  obj->field2c = *(uint16_t *)ring
 *                  ring += 4
 *                  if (obj->field2c != 0 && i != obj->a10) seen++
 *                  obj->field24 = seen
 *              }
 *              if (seen <= 1) -- MATCH --
 *          }
 *          if (*(uint32_t *)e & 0x8000) -- NO MATCH --
 *          e += 0x18
 *          obj->field48 = e
 *      }
 *
 *      MATCH:     n = *(uint8_t *)(e + 0x10)
 *                 obj->field1c = n ? n - 1 : (uint32_t)base
 *                 obj->field48 = base ; obj->field5c = 1
 *      NO MATCH:  obj->field48 = base ; obj->field5c = 0
 *
 * **The ring it walks is the one clear_combo_butn clears.** Six halfwords four
 * bytes apart at G + 0x3d0, offset two per player -- that routine writes zeros
 * into exactly those slots and this reads five of them back. The two agree on
 * the base, the stride and the per-player offset, which is what makes the
 * layout certain.
 *
 * **A match means at most ONE unexpected button.** The count is of ring slots
 * that are non-empty and whose index is not `obj->a10`, and the test is
 * `seen <= 1`. So the slot named by a10 is the one allowed to differ, and one
 * more stray press is tolerated on top of it -- anything beyond that and the
 * entry is rejected and the walk moves on.
 *
 * **The list is terminated by a bit, not by a zero.** Bit 15 of the entry's
 * first word ends it, and the stride is 0x18. `last_switch_ram` is indexed by
 * the byte at +8 with an eight-byte stride, and its second word points at the
 * halfwords -- plus two for the second player, the same trick again.
 *
 * 0x48 is walked forward through the whole search and restored to `base` on
 * both exits, so the caller sees it unchanged. 0x1c, 0x20, 0x24 and 0x2c are
 * used as scratch throughout and are left holding whatever the last step put
 * there -- on a match, either the count less one or the base address, which is
 * the one place two exits disagree about what 0x1c means. */
extern uint32_t last_switch_ram[];         /* pointer slot -> 0x0016f50c */

long combo_scan_a11(MK3OBJ *obj)
{
    char    *base = (char *)(void *)(uintptr_t)obj->field48;
    char    *e = base;
    char    *ring;
    char    *p;
    uint32_t k, seen, i, n;

    for (;;) {
        k = *(uint8_t *)(e + 8);
        obj->field1c = k;
        obj->field54 = k;

        p = (char *)(void *)(uintptr_t)
            *(uint32_t *)((char *)last_switch_ram + k * 8 + 4);
        obj->field1c = (uint32_t)(uintptr_t)p;
        if (obj->field00->field08 != 0) {
            p += 2;
            obj->field1c = (uint32_t)(uintptr_t)p;
        }

        obj->field20 = *(uint16_t *)p;
        if (obj->field20 != 0) {
            ring = G_BYTES + 0x3d0 + obj->field00->field08 * 2;
            seen = 0;
            obj->field24 = 0;
            obj->field1c = 0;
            obj->field20 = (uint32_t)(uintptr_t)ring;

            for (i = 0; i != 5; i++) {
                obj->field2c = *(uint16_t *)ring;
                ring += 4;
                obj->field20 = (uint32_t)(uintptr_t)ring;
                if (obj->field2c != 0 && i != obj->a10) {
                    seen += 1;
                    obj->field24 = seen;
                }
                obj->field1c = i + 1;
            }

            if (seen <= 1) {                    /* MATCH */
                n = *(uint8_t *)(e + 0x10);
                obj->field1c = n;
                if (n != 0)
                    obj->field1c = n - 1;
                obj->field5c = 1;
                obj->field1c = (uint32_t)(uintptr_t)base;
                obj->field48 = (uint32_t)(uintptr_t)base;
                return 0;
            }
        }

        obj->field1c = *(uint32_t *)e;
        if ((obj->field1c & 0x8000u) != 0) {     /* the end of the list */
            obj->field48 = (uint32_t)(uintptr_t)base;
            obj->field5c = 0;
            return 0;
        }

        e += 0x18;
        obj->field48 = (uint32_t)(uintptr_t)e;
    }
}


/* ------------------------------------------------------------------ t_do_knee
 *
 * armv7 0x00032a88, 240 bytes.  **Complete.**
 *
 *      token == 0:      init_special(obj)
 *                       obj->field1c = 1
 *                       obj->a10     = 1
 *                       obj->field20 = 1 + 0x108    = 0x109
 *                       obj->field40 = 0x109 - 0xf6 = 0x13
 *                       obj->field48 = 0x13 - 5     = 0x0e
 *                       token := 0x896, descend into t_striker
 *
 *      token == 0x896:  if (obj->field5c == 0) -- into the 0x8aa body --
 *                       n = ochar_knee_combos[obj->field08->field24]
 *                       obj->field1c = n
 *                       if (n != 0) {
 *                           obj->field48 = n
 *                           frame[frame].handler = t_process_combo_table
 *                       } else {
 *                           obj->field1c = obj->field00->field18 = 0x601
 *                           token := 0x8aa, park 0xf
 *                       }
 *
 *      token == 0x8aa:  obj->field1c = 6
 *                       frame[frame].handler = t_retract_strike
 *
 *      otherwise:       return -3
 *
 * **The knee is a strike that may or may not open a combo.** State zero sets the
 * action up and hands control down to t_striker; when t_striker comes back with
 * 0x5c set -- it connected -- the routine looks the character up in
 * ochar_knee_combos, and a non-zero entry there becomes obj->field48 and sends
 * the thread into t_process_combo_table. So the table says, per character,
 * whether a landed knee starts a combo string and which one.
 *
 * **A miss and an empty table entry both end the same way**, at t_retract_strike
 * with 0x1c = 6. The difference is that the empty entry first writes 0x601 into
 * the opponent proc 0x18 and waits fifteen frames; a clean miss retracts at
 * once.
 *
 * **The five constants are built by chained arithmetic off the first**, which is
 * how the compiler avoided five literals: 1, +0x108, -0xf6, -5. Only the results
 * mean anything -- 0x109 is the action number, and the elbow uses 0x10a, the
 * next one along.
 *
 * **The re-test of 0x5c at 0x32b4c is dead.** Reaching it already required 0x5c
 * to be non-zero and nothing in between can change it, so the beq back into the
 * 0x8aa body never fires. Transcribed as written; t_do_elbow, which is otherwise
 * the same routine, does not carry it.
 *
 * The ochar_* tables are reached pc-relative with no indirection, unlike
 * combo_strike_table below -- they are defined in this translation unit. */
void init_special(MK3OBJ *obj);
long t_striker(MK3THREAD *thread);
long t_retract_strike(MK3THREAD *thread);         /* pointer slot 0x000f38c8 */
extern uint32_t ochar_knee_combos[];              /* 0x0016645c */

long t_do_knee(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    uint32_t n;

    if (token == 0) {
        init_special(obj);
        obj->field1c = 1;
        obj->a10     = 1;
        obj->field20 = 1 + 0x108;
        obj->field40 = (1 + 0x108) - 0xf6;
        obj->field48 = ((1 + 0x108) - 0xf6) - 5;

        *mk3_frame(thread, thread->frame + 1) = 0x896;
        thread->frame = thread->frame + 1;        /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_striker;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x896 && obj->field5c != 0) {
        n = ochar_knee_combos[obj->field08->field24];
        obj->field1c = n;

        if (n != 0) {
            obj->field48 = n;
            return mk3_install(thread,
                               (MK3THREADFUNC)t_process_combo_table);
        }

        obj->field1c = 0x601;
        obj->field00->field18 = 0x601;

        if (obj->field5c != 0) {        /* always true; see the note above */
            *mk3_frame(thread, thread->frame + 1) = 0x8aa;
            thread->fieldfc = 0xf;
            return 0xf;
        }
    }

    if (token != 0x896 && token != 0x8aa)
        return -3;

    obj->field1c = 6;
    return mk3_install(thread, (MK3THREADFUNC)t_retract_strike);
}

/* ----------------------------------------------------------------- t_do_elbow
 *
 * armv7 0x00032b78, 280 bytes.  **Complete.**
 *
 *      token == 0:      init_special(obj)
 *                       a = ochar_elbow_animations[obj->field08->field24]
 *                       obj->field1c = a
 *                       if (a & 0x80) {
 *                           obj->field1c = obj->field40 = a & 0x7f
 *                           get_char_ani2(obj)
 *                       } else {
 *                           obj->field40 = a
 *                           get_char_ani(obj)
 *                       }
 *                       obj->field1c = 1
 *                       obj->field20 = 0x10a
 *                       obj->a10     = 5
 *                       obj->field48 = 5 + 0xa = 0xf
 *                       token := 0x8f5, descend into t_striker
 *
 *      token == 0x8f5:  if (obj->field5c == 0) -- into the 0x903 body --
 *                       n = ochar_elbow_combos[obj->field08->field24]
 *                       obj->field1c = n
 *                       if (n != 0) {
 *                           obj->field48 = n
 *                           frame[frame].handler = t_process_combo_table
 *                       } else {
 *                           obj->field1c = obj->field00->field18 = 0x60a
 *                           token := 0x903, park 0xa
 *                       }
 *
 *      token == 0x903:  obj->field1c = 3
 *                       obj->field20 = 0x60a
 *                       frame[frame].handler = t_retract_strike_act
 *
 *      otherwise:       return -3
 *
 * **The same routine as t_do_knee with one addition: the elbow picks its own
 * animation.** A byte out of ochar_elbow_animations, indexed by the character,
 * chooses it, and bit 7 of that byte selects the second lookup -- exactly the
 * packing a9_combo_ani uses on the high byte of 0x40, here on a byte from a
 * table. The bit is stripped before the call, so it is a selector and not part
 * of the number.
 *
 * Two tables, two strides: the animation one is ldrb -- bytes -- and the combo
 * one is ldr with lsl #2 -- words. So a character elbow animation fits in a byte
 * and its combo entry is a pointer.
 *
 * **The elbow retracts through t_retract_strike_act, not t_retract_strike**, and
 * writes 0x60a into 0x20 on the way -- the knee writes nothing there and uses
 * the plain retract. That is the only structural difference between the pair
 * apart from the animation lookup and the dead 0x5c re-test the knee carries.
 *
 * 0x60a is both the value put in the opponent proc 0x18 when the table entry is
 * empty and the one put in this object 0x20 on the retract, so the same constant
 * names the action on both sides. */
void get_char_ani(MK3OBJ *obj);
void get_char_ani2(MK3OBJ *obj);
long t_retract_strike_act(MK3THREAD *thread);     /* pointer slot 0x000f3874 */
extern uint8_t  ochar_elbow_animations[];         /* 0x001664bc */
extern uint32_t ochar_elbow_combos[];             /* 0x001663fc */

long t_do_elbow(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    uint32_t a, n;

    if (token == 0) {
        init_special(obj);

        a = ochar_elbow_animations[obj->field08->field24];
        obj->field1c = a;
        if ((a & 0x80) != 0) {
            obj->field1c = a & 0x7fu;
            obj->field40 = a & 0x7fu;
            get_char_ani2(obj);
        } else {
            obj->field40 = a;
            get_char_ani(obj);
        }

        obj->field1c = 1;
        obj->field20 = 0x10a;
        obj->a10     = 5;
        obj->field48 = 5 + 0xa;

        *mk3_frame(thread, thread->frame + 1) = 0x8f5;
        thread->frame = thread->frame + 1;        /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_striker;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x8f5 && obj->field5c != 0) {
        n = ochar_elbow_combos[obj->field08->field24];
        obj->field1c = n;

        if (n != 0) {
            obj->field48 = n;
            return mk3_install(thread,
                               (MK3THREADFUNC)t_process_combo_table);
        }

        obj->field1c = 0x60a;
        obj->field00->field18 = 0x60a;
        *mk3_frame(thread, thread->frame + 1) = 0x903;
        thread->fieldfc = 0xa;
        return 0xa;
    }

    if (token != 0x8f5 && token != 0x903)
        return -3;

    obj->field1c = 3;
    obj->field20 = 0x60a;
    return mk3_install(thread, (MK3THREADFUNC)t_retract_strike_act);
}

/* -------------------------------------------------------------------- t_comba
 *
 * armv7 0x00032db4, 284 bytes.  **Complete.**
 *
 *      token == 0:      a9_combo_ani(obj)
 *                       obj->field1c = *(int32_t *)(obj->field48 + 8) >> 8
 *                       token := 0x823, descend into t_mframew
 *
 *      token == 0x823:  k = *(int32_t *)(obj->field48 + 0x10) >> 8
 *                       obj->field1c = k
 *                       obj->field1c = combo_strike_table[k]
 *                       strike_check(obj)
 *                       if (obj->field5c == 0)
 *                           frame[frame].handler = t_combo_miss
 *                       else {
 *                           obj->field38 = obj->field48
 *                           next = *(int32_t *)(obj->field48 + 0x14)
 *                           obj->field48 = next
 *                           if (next > 0x100)
 *                               frame[frame].handler = t_comb0
 *                           else {
 *                               obj->field1c = next
 *                               token := 0x840, park next
 *                           }
 *                       }
 *
 *      token == 0x840:  frame[frame].handler = t_comb9
 *
 *      otherwise:       return -3
 *
 * **This is the body of a combo string: one entry per hit, walked through 0x48.**
 * The entry is the 0x18-byte record combo_scan_a11 walks, and this routine reads
 * three of its words -- +0x08 is the animation-and-rate word for the wait, +0x10
 * names the strike, +0x14 is what comes next.
 *
 * **Both packed words are read as a signed word and shifted right eight**, so
 * the useful part is bits 8 and up and the low byte is something else -- the
 * same packing a9_combo_ani takes apart, and the reason a9_combo_ani is called
 * first: it has already consumed 0x40 by the time these are read.
 *
 * **+0x14 is a pointer OR a duration, told apart by 0x100.** Above that it is
 * the next entry and the thread goes to t_comb0 to run it; at or below it is a
 * frame count, written to 0xfc and returned, and the thread comes back at token
 * 0x840 to finish through t_comb9. Either way the value is stored into 0x48
 * before the test, so on the short path 0x48 is left holding a small integer
 * where every other reader expects an address.
 *
 * strike_check answers in 0x5c, and a miss ends the string at t_combo_miss --
 * there is no retry. 0x38 keeps the entry that hit, which is the only record of
 * where the string was when it landed.
 *
 * combo_strike_table is reached through a pointer slot rather than pc-relative,
 * so unlike the ochar_* tables above it is defined in another translation
 * unit. */
long strike_check(MK3OBJ *obj);
extern uint32_t combo_strike_table[];     /* pointer slot -> 0x00167694 */

long t_comba(MK3THREAD *thread)
{
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;
    uint32_t token = *mk3_frame(thread, thread->frame + 1);
    int32_t  k, next;

    if (token == 0) {
        a9_combo_ani(obj);
        obj->field1c = (uint32_t)
            (*(int32_t *)((char *)(void *)(uintptr_t)obj->field48 + 8) >> 8);

        *mk3_frame(thread, thread->frame + 1) = 0x823;
        thread->frame = thread->frame + 1;        /* push a level */
        mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)t_mframew;
        *mk3_frame(thread, thread->frame + 1) = 0;
        return 0;
    }

    if (token == 0x840)
        return mk3_install(thread, (MK3THREADFUNC)t_comb9);

    if (token != 0x823)
        return -3;

    k = *(int32_t *)((char *)(void *)(uintptr_t)obj->field48 + 0x10) >> 8;
    obj->field1c = (uint32_t)k;
    obj->field1c = combo_strike_table[k];

    strike_check(obj);
    if (obj->field5c == 0)
        return mk3_install(thread, (MK3THREADFUNC)t_combo_miss);

    obj->field38 = obj->field48;
    next = *(int32_t *)((char *)(void *)(uintptr_t)obj->field48 + 0x14);
    obj->field48 = (uint32_t)next;

    if (next > 0x100)
        return mk3_install(thread, (MK3THREADFUNC)t_comb0);

    obj->field1c = (uint32_t)next;
    *mk3_frame(thread, thread->frame + 1) = 0x840;
    thread->fieldfc = obj->field1c;
    return (long)obj->field1c;
}
