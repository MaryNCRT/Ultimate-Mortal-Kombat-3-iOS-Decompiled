/*
 * a_fn.c -- gamecode/logic/a_fn.c, decompiled.
 *
 * One function, twelve bytes, and it is the whole file.
 */

#include "mk3logic.h"

/* use_shakbod_pal -- armv7 0x0007cb30, 12 bytes.  **Complete.**
 *
 *      match_him_with_me(obj)
 *
 * A tail call and nothing else: the argument goes through untouched. So the
 * file's one function is a NAME for an existing routine, not a routine.
 *
 * That is worth stating plainly rather than treating as an oddity. This
 * directory is full of one-line wrappers -- `isa5`, `joystick_in_a0`,
 * `strike_check_a0`, `player_normpal` -- and every one of them exists so a
 * call site can say what it means. The shake-body palette and "match him with
 * me" are the same operation; only the reason differs.
 */
void match_him_with_me(MK3OBJ *obj);

void use_shakbod_pal(MK3OBJ *obj)
{
    match_him_with_me(obj);
}
