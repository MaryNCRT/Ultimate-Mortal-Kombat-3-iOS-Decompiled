/*
 * mkbonus.c -- gamecode/logic/mkbonus.c, decompiled.
 *
 * Eight functions, all of them the "who won" bookkeeping that runs after a
 * match: which player gets the round, what text and character portrait the
 * ending screen shows, the closing chord, and the wind-down back to the
 * ordinary per-fighter reaction handler.
 *
 * Three of the eight -- `t_bonus_exit`, `t_bonus_count` and
 * `t_bonus_count_draw` -- are thread handlers in the `MK3THREADLONGFUNC`
 * sense: `mk3.c`'s `mk3_update` calls them as `long (*)(MK3THREAD *thread)`,
 * and they dispatch on the token `mk3_frame(thread, thread->frame + 1)[0]`
 * the way every other driver in this directory does. The other five --
 * `get_winner_ochar`, `get_winner_text`, `play_ending_chord`,
 * `_do_winner_char`, `_do_winner_text` -- take the OBJECT the thread controls
 * (`(MK3OBJ *)thread->proc`, per mk3logic.h's note on that field), not the
 * thread itself, which is how `send_code_a3`, `tsound_func` and `create_fx`
 * are already declared elsewhere in this directory.
 *
 * ## Verification status
 *
 * This file compiles clean under `-Wall -Wextra -std=c99` and is meant to
 * pass `tools/symcheck.py` (every callee resolves to a real symbol) and
 * `tools/instck.py` (no push_handler/install confusion). It has **not** been
 * run against `tools/armrecomp/recomp.py`'s differential oracle -- that tool
 * needs `capstone`, which was not installable in the session that produced
 * this file. Every instruction below was read from `llvm-objdump`'s Mach-O
 * disassembly of the same armv7 slice `tools/macho.py` produces, and the
 * reader was cross-checked against the already-verified `_SwitchQueue`
 * (other.c) before being trusted on anything new. Treat this file as a
 * careful transcription pending the oracle, not as landed to the project's
 * usual bar.
 *
 * ## The dispatch tokens
 *
 * `t_bonus_count`'s outer switch is on a token stored by the PREVIOUS call,
 * not a state number in call order: 0, 0x73 ('s'), 0x79, 0x7e, 0x84, 0xcf.
 * None of them is sequential, which matches this directory's habit of naming
 * states as ASCII-ish tags rather than 0, 1, 2. The chain this file drives:
 *
 *      0     -- first call: read the two win counters (G+0x368, G+0x36c);
 *               tied -> install t_bonus_count_draw; otherwise show the
 *               winner's text/character/chord and set the next token to 0x73
 *      0x73  -- tsound the winner (ochar 0x19 goes through rsnd_func and
 *               0x79 instead -- see the note on that function)
 *      0x79 / 0x7e -- shared entry: if either counter hit 0xa6, play a
 *               fanfare and fall to 0x84 after a 0x5a-frame wait; otherwise
 *               fall straight into 0x84
 *      0x84  -- G+0x450 / G+0x458 pick a `fatality_animations` entry and
 *               `create_fx` it; next token 0xcf, 2-frame wait
 *      0xcf  -- install `t_bonus_exit`, which unwinds the thread's nested
 *               handler stack one level per call and finally installs
 *               `t_local_reaction_exit` (joy.c) to hand the thread back
 *
 * The return values along this chain (0x6e, 2, 0x5a, and the incoming token
 * itself on the ochar-0x19 detour) do not fit `mk3_frame`'s documented
 * 0 / negative / `MK3_THREAD_DONE` enum. They are transcribed exactly as the
 * register holds them; `thread->fieldfc`, set on every path here, is what
 * actually paces the wait, and the return value may simply not be read by
 * anything that reaches this handler. Flagged rather than silently rounded
 * to 0.
 */

#include "mk3logic.h"

void  send_code_a3(MK3OBJ *obj);
void  tsound_func(MK3OBJ *obj, uint32_t arg);
void  create_fx(MK3OBJ *obj);
void  rsnd_func(MK3OBJ *obj, uint32_t which);
void  MKEvent_Add(long type, long subtype, long param, long player);
long  t_local_reaction_exit(MK3THREAD *thread);   /* joy.c, already decompiled */

/* Per-character text, indexed by the roster id `get_winner_ochar` resolves
 * into `obj->field1c`. One pointer per character; the roster size is not
 * established by this file. */
extern const char *P32 ochar_winner_text[];

/* The bonus-round fatality-style effect, indexed by a G+0x450/0x458 value
 * minus one and handed straight to `create_fx` through `obj->field1c`. Same
 * shape as `ochar_winner_text`: one entry per something, extent not
 * established here. */
extern const uint32_t fatality_animations[];

/* `_txt_tie` -- the string `_do_winner_text` shows when nobody won. A plain
 * `extern char *`, read through one more `ldr` the way `G`/`GrObj`/`Plyr` are:
 * the literal names the SLOT, and the slot holds the string pointer. */
extern char *P32 txt_tie;

long t_bonus_count_draw(MK3THREAD *thread);
void get_winner_ochar(MK3OBJ *obj);
void get_winner_text(MK3OBJ *obj);
void play_ending_chord(MK3OBJ *obj);
void _do_winner_char(MK3OBJ *obj, long ch);
void _do_winner_text(MK3OBJ *obj);
long t_bonus_exit(MK3THREAD *thread);


/* --------------------------------------------------------------- t_bonus_exit
 *
 * armv7 0x0007cb3c, 68 bytes.
 *
 * Winds the thread's nested handler stack down one level per call, the same
 * refuse/pop shape `mk3_push_handler` documents, except that on reaching the
 * bottom it does not refuse -- it installs `t_local_reaction_exit`
 * (`0x00030060`, joy.c) and hands the thread back to the ordinary per-fighter
 * reaction system. That is the "exit" in the name: this is the last handler
 * the bonus-round thread ever runs under its own name.
 */
long t_bonus_exit(MK3THREAD *thread)
{
    uint32_t frame = thread->frame;
    uint32_t next  = *mk3_frame(thread, frame + 1);

    if (next != 0)
        return -3;                       /* the level above is still occupied */

    if ((int32_t)frame > 0) {
        thread->frame = frame - 1;
        return (long)next;               /* == 0 */
    }

    /* Bottom of the stack: hand the thread back. */
    mk3_frame(thread, frame)[1] = (uint32_t)(uintptr_t)t_local_reaction_exit;
    *mk3_frame(thread, frame + 1) = 0;
    return 0;
}


/* ----------------------------------------------------------- get_winner_ochar
 *
 * armv7 0x0007cb80, 96 bytes.
 *
 * Picks which fighter's slot to read -- `Plyr[1]`/`GrObj+0x70` unless
 * `G+0x45c` (a signed halfword) is exactly 1, in which case `Plyr[0]`/
 * `GrObj+0x24` -- and resolves that fighter's roster id off `GrObj`, 0x4c
 * (`GROBJ_STRIDE`) apart between the two offsets.
 *
 * `G+0x45c` is stashed into `obj->field54`, documented in mk3logic.h as
 * "where a computed word is parked" -- a second, non-conflicting sighting of
 * exactly that. Nothing in this file reads it back; whatever consumes it is
 * elsewhere.
 *
 * `obj->field20` is overloaded here to hold a raw pointer into `Plyr`
 * (`MK3OBJ` declares it a plain counter elsewhere in this directory) and is
 * immediately double-dereferenced: the word at `*(Plyr slot) + 0x10`. What
 * that word is is not established; bit 0x200 of it forces the roster id to
 * 0xc -- flagged, not guessed, which character or state 0xc names.
 */
void get_winner_ochar(MK3OBJ *obj)
{
    long   which = (int16_t)*(volatile uint16_t *)(void *)(G_BYTES + 0x45c);
    char  *plyr_slot;
    void  *inner;
    uint32_t flagword;

    obj->field54 = (uint32_t)which;

    if (which == 1) {
        plyr_slot = Plyr;                                  /* Plyr[0] */
        obj->field1c = *(uint32_t *)(void *)(GrObj + 0x24);
    } else {
        plyr_slot = Plyr + PLYR_STRIDE;                     /* Plyr[1] */
        obj->field1c = *(uint32_t *)(void *)(GrObj + 0x70);
    }
    /* Stored and re-read as a plain integer round trip, never through a
     * pointer-to-pointer cast, so this does not depend on strict aliasing --
     * the field is a uint32_t and a pointer is exactly 32 bits on this
     * target. */
    obj->field20 = (uint32_t)(uintptr_t)plyr_slot;

    inner    = (void *)(uintptr_t)obj->field20;   /* re-read the pointer just stored */
    /* the slot's first word is the fighter's object; its +0x10 is the flags */
    flagword = *(uint32_t *)((char *)(uintptr_t)*(uint32_t *)inner + 0x10);
    obj->field2c = flagword;
    if ((flagword & 0x200u) != 0)
        obj->field1c = 0xc;
}


/* ------------------------------------------------------------ get_winner_text
 *
 * armv7 0x0007cbe0, 28 bytes.
 *
 *      get_winner_ochar(obj);
 *      obj->field3c = ochar_winner_text[obj->field1c];
 *
 * `field3c` is `_do_winner_text`'s only input, and is documented elsewhere in
 * this directory as a per-frame fall amount (t_boomerang_call) -- overloaded
 * here to carry a `const char *` instead. Stored and read as a raw pointer
 * round trip rather than forcing the struct's declared `uint32_t` to pretend
 * it is one.
 */
void get_winner_text(MK3OBJ *obj)
{
    get_winner_ochar(obj);
    obj->field3c = (uint32_t)(uintptr_t)ochar_winner_text[obj->field1c];
}


/* --------------------------------------------------------- play_ending_chord
 *
 * armv7 0x0007cbfc, 16 bytes.  Straight-line leaf: obj->field28 = 0x99;
 * send_code_a3(obj). Same shape as mkanimal.c's `animality_tune`
 * (obj->field28 = 0x3a; send_code_a3(obj)) -- one more sound code parked at
 * 0x28 for `send_code_a3` to pick up.
 */
void play_ending_chord(MK3OBJ *obj)
{
    obj->field28 = 0x99;
    send_code_a3(obj);
}


/* ------------------------------------------------------------ _do_winner_char
 *
 * armv7 0x0007cc0c, 44 bytes.  Binary symbol `__do_winner_char` -- one
 * leading underscore is the Mach-O C prefix, the second is part of the name
 * EA gave it, and this file keeps it rather than dropping it as decoration.
 *
 * `ch` is tested only for zero/non-zero -- its value is never read past the
 * `cbz` -- so despite the name it behaves as a boolean, not a character id:
 *
 *      ch != 0:  MKEvent_Add(3, 3, -1, 0)              -- no character
 *      ch == 0:  get_winner_ochar(obj);
 *                MKEvent_Add(3, 3, obj->field1c, 0)    -- the resolved winner
 *
 * `t_bonus_count_draw` calls this with `ch = 1` (the tie path, "no character
 * portrait"); `t_bonus_count` calls it with `ch = <the outer dispatch
 * token>`, which is non-zero (0) only when... actually 0 on the winner path,
 * so that call DOES take the `get_winner_ochar` side. See `t_bonus_count`.
 */
void _do_winner_char(MK3OBJ *obj, long ch)
{
    if (ch != 0) {
        MKEvent_Add(3, 3, -1, 0);
        return;
    }

    get_winner_ochar(obj);
    MKEvent_Add(3, 3, (long)obj->field1c, 0);
}


/* ------------------------------------------------------------ _do_winner_text
 *
 * armv7 0x0007cc38, 20 bytes.  Straight-line leaf:
 *
 *      MKEvent_Add(3, 2, obj->field3c, 0)
 *
 * `obj->field3c` is the text pointer `get_winner_text` (or, on a tie,
 * `t_bonus_count_draw` directly) left there; passed through as the event's
 * `long param`, the same way this queue already carries pointers and raw
 * values interchangeably elsewhere in this directory (mk3.c's `MKEvent_Add`
 * note).
 */
void _do_winner_text(MK3OBJ *obj)
{
    MKEvent_Add(3, 2, (long)obj->field3c, 0);
}


/* ------------------------------------------------------- t_bonus_count_draw
 *
 * armv7 0x0007cc4c, 108 bytes.  Thread handler, installed by `t_bonus_count`
 * on a tie (G+0x368 == G+0x36c). Two states, the token stored the same way
 * `t_bonus_count` stores its own:
 *
 *      token == 0    : obj->field3c = txt_tie; _do_winner_text(obj);
 *                      _do_winner_char(obj, 1);         -- no portrait
 *                      next token 0xfd, 0x40-frame wait
 *      token == 0xfd : install t_bonus_exit, next token cleared to 0
 *      anything else : refuse (-3)
 */
long t_bonus_count_draw(MK3THREAD *thread)
{
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;

    if (token == 0) {
        obj->field3c = (uint32_t)(uintptr_t)txt_tie;
        _do_winner_text(obj);
        _do_winner_char(obj, 1);

        *mk3_frame(thread, frame + 1) = 0xfd;
        thread->fieldfc = 0x40;
        return 0x40;
    }

    if (token != 0xfd)
        return -3;

    mk3_frame(thread, frame)[1] = (uint32_t)(uintptr_t)t_bonus_exit;
    *mk3_frame(thread, frame + 1) = 0;
    return 0;
}


/* ------------------------------------------------------------- t_bonus_count
 *
 * armv7 0x0007ccb8, ~416 bytes including its literal pool. The main
 * bonus-round driver; see the file header for the full token chain. One
 * inline special case worth calling out on its own:
 *
 * **ochar 0x19 detours through `rsnd_func`, not `tsound_func`.** Every other
 * character's win chant is `tsound_func(obj, obj->field1c + 0x3f)`; ochar
 * 0x19's is `rsnd_func(obj, obj->field1c - 0x18)` and skips straight to the
 * 0x79/0x7e fanfare-or-not branch instead of the ordinary 0x6e-frame wait --
 * and, unlike every other exit in this function, does not touch
 * `thread->fieldfc` at all. Which roster slot 0x19 is is not established
 * here.
 */
long t_bonus_count(MK3THREAD *thread)
{
    uint32_t frame = thread->frame;
    uint32_t token = *mk3_frame(thread, frame + 1);
    MK3OBJ  *obj   = (MK3OBJ *)thread->proc;

    if (token == 0x79)
        goto fanfare_check;
    if ((int32_t)token <= 0x79)
        goto low;
    if (token == 0x84)
        goto fatality_effect;
    if (token == 0xcf)
        goto install_exit;
    if (token == 0x7e)
        goto fanfare_check;
    return -3;

low:
    if (token == 0)
        goto tie_or_winner;
    if (token != 0x73)
        return -3;

    /* token == 0x73 */
    get_winner_ochar(obj);
    if (obj->field1c == 0x19) {
        rsnd_func(obj, obj->field1c - 0x18);
        *mk3_frame(thread, frame + 1) = 0x79;
        return (long)token;                  /* == 0x73; see the note above */
    }
    tsound_func(obj, obj->field1c + 0x3f);
    *mk3_frame(thread, frame + 1) = 0x7e;
    thread->fieldfc = 0x6e;
    return 0x6e;

tie_or_winner:
    {
        uint32_t p1 = *(uint32_t *)(void *)(G_BYTES + 0x368);
        uint32_t p2 = *(uint32_t *)(void *)(G_BYTES + 0x36c);

        /* Both stored unconditionally before the comparison, the way the
         * binary does it -- field1c and field20 here are the raw round-win
         * counters, a THIRD meaning for each beyond the ones get_winner_ochar
         * and t_boomerang_call already give them. */
        obj->field1c = p1;
        obj->field20 = p2;

        if (p1 == p2) {
            mk3_frame(thread, frame)[1] = (uint32_t)(uintptr_t)t_bonus_count_draw;
            *mk3_frame(thread, frame + 1) = 0;
            return 0;
        }

        get_winner_text(obj);
        _do_winner_text(obj);
        _do_winner_char(obj, (long)token);   /* token == 0: takes the resolved-winner path */
        play_ending_chord(obj);

        *mk3_frame(thread, frame + 1) = 0x73;
        thread->fieldfc = 0x28;
        return 0x28;
    }

fanfare_check:
    {
        uint32_t c1 = *(uint32_t *)(void *)(G_BYTES + 0x368);
        uint32_t c2;

        if (c1 != 0xa6) {
            c2 = *(uint32_t *)(void *)(G_BYTES + 0x36c);
            if (c2 != 0xa6)
                goto fatality_effect;
        }

        tsound_func(obj, 0x61);
        *mk3_frame(thread, frame + 1) = 0x84;
        thread->fieldfc = 0x5a;
        return 0x5a;
    }

fatality_effect:
    {
        uint16_t h1 = *(volatile uint16_t *)(void *)(G_BYTES + 0x450);

        obj->field1c = (uint32_t)(int32_t)(int16_t)h1;
        if (h1 == 0) {
            mk3_frame(thread, frame)[1] = (uint32_t)(uintptr_t)t_bonus_exit;
            *mk3_frame(thread, frame + 1) = 0;
            return 0;
        }

        {
            uint16_t h2 = *(volatile uint16_t *)(void *)(G_BYTES + 0x458);
            obj->field1c = (uint32_t)(int32_t)(int16_t)h2;
            if (h2 == 0)
                obj->field1c = 1;
        }

        {
            uint32_t idx = obj->field1c - 1;
            obj->field1c = idx;
            obj->field1c = fatality_animations[idx];
        }

        create_fx(obj);

        *mk3_frame(thread, frame + 1) = 0xcf;
        thread->fieldfc = 2;
        return 2;
    }

install_exit:
    mk3_frame(thread, frame)[1] = (uint32_t)(uintptr_t)t_bonus_exit;
    *mk3_frame(thread, frame + 1) = 0;
    return 0;
}
