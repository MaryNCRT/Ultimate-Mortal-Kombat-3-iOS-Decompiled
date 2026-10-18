/*
 * a_robo.c -- gamecode/logic/a_robo.c, decompiled.
 *
 * Two functions, twelve bytes each, and they are the whole file. Both are the
 * same call with a different constant -- the pair shape this directory is full
 * of, at its smallest.
 */

#include "mk3logic.h"

/* ================================= do_robo_laugh_sound, do_beep_sound
 *
 * armv7 0x0002ebc4 and 0x0002ebd0, 12 bytes each.  **Complete.**
 *
 *      do_robo_laugh_sound(obj)   ochar_sound_n(obj, 0x1c)
 *      do_beep_sound(obj)         ochar_sound_n(obj, 0x1a)
 *
 * `ochar_sound_n` takes a sound index and plays that entry of the character's
 * own group. **0x1a and 0x1c are two of the indices into the sound-group
 * table** `tools/sounds.py` recovers from the binary -- the first per-move
 * sound indices this project has seen written down, rather than inferred.
 *
 * They are two apart, not adjacent, so 0x1b belongs to something else in the
 * same set. Whatever that is has not been read.
 */
void ochar_sound_n(MK3OBJ *obj, uint32_t which);

void do_robo_laugh_sound(MK3OBJ *obj)
{
    ochar_sound_n(obj, 0x1cu);
}

void do_beep_sound(MK3OBJ *obj)
{
    ochar_sound_n(obj, 0x1au);
}
