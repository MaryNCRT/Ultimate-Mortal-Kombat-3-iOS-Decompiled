/*
 * mk3logic.h -- the structs, globals and constants gamecode/logic shares.
 *
 * Every one of these was established by a function in this directory, and the
 * comment on each says which. Nothing here is a guess at a layout: a field
 * exists because an instruction loads or stores it at that offset, and the
 * pads between them are arithmetic, not padding in the C sense.
 *
 * The offsets are asserted in tests/test_logic_offsets.c against 32-bit models
 * of the same three structs. That test is what makes a `_padXX` safe to write:
 * a pad four bytes short moves every field after it, the file still compiles,
 * and the only symptom is a fight that misbehaves a long way from here.
 *
 * These declarations began in other.c and were lifted out when the second file
 * of this directory needed them. The prose came with them.
 */

#ifndef MK3LOGIC_H
#define MK3LOGIC_H

#include <stddef.h>   /* NULL */
#include <stdint.h>
#include <string.h>

/* The cooperative thread. Its fields are the ones StartThreadAt writes and
 * t_self_terminate reads; the eight-byte array they index sits at the thread's
 * own address and overlaps them, which is why `frame` is never small. */
typedef void (*P32 MK3THREADFUNC)(void);   /* a 32-bit word, as every pointer here (P32) */

/* The thread, as its writers and readers establish it. Only the offsets that
 * appear in this file are named. */
typedef struct MK3THREAD {
    struct MK3THREAD *P32 next;      /* 0x00  FindThread walks this */
    MK3THREADFUNC func;          /* 0x04  what StartThreadAt puts there */
    uint32_t      field08;       /* 0x08  cleared with it */
    uint8_t       _pad0c[0x98];
    uint32_t      frame;         /* 0xa4  the index into the frame array */
    uint8_t       args[0x50];    /* 0xa8  twenty more words, four bytes each,
                                  *       pushed by the striker pair */
    uint32_t      fieldf8;       /* 0xf8  the cursor into them; fastxfer_thread
                                  *       clears it too */
    /* 0xfc  A SLEEP COUNTER. `mk3_update` decrements it when positive and runs
     * the thread only when it reaches zero, so a handler that stores N here
     * skips the next N frames. Cleared on start. */
    uint32_t      fieldfc;       /* 0xfc */
    uint32_t      player;        /* 0x100 getobjectinsert multiplies it by
                                  *       PLYR_STRIDE to index Plyr */
    uint32_t      pid;           /* 0x104 NewThreadProcPid sets it */
    void         *P32 proc;          /* 0x108 FindThreadProc and NewThreadProc
                                  *       return it */
} MK3THREAD;

/* 268 bytes, from the stride `my_func` uses to index `_mytc`: the compiler
 * spells it `i*4 + i*64 - i` scaled by four. The last field above is at 0x108,
 * one word short of it, so the stride is the struct and not a gap between
 * elements. */
#define MK3THREAD_STRIDE  0x10c

/* **0x16462 is a SENTINEL, not a duration.** Eleven handlers across this
 * directory end with
 *
 *      thread->fieldfc = 0x16462;  return 0x16462;
 *
 * and for a long time this project could only say that the number was "the
 * park-and-never-wake duration", because `fieldfc` counts down and 0x16462 is
 * far larger than any real sleep.
 *
 * `mk3_update` settles it: the comparison is against the RETURN VALUE, and a
 * thread whose handler returns this is unlinked from `TList` and pushed back on
 * the free list on the spot -- with its owner's slave fields cleared first. So
 * those eleven lines mean **"I am finished, delete me"**, and the `fieldfc`
 * store is belt-and-braces that is never consulted.
 *
 * The sites that describe it as a duration are wrong in wording only: they all
 * correctly called the state a terminator. The mechanism is what was missing. */
#define MK3_THREAD_DONE  0x16462

/* And the shape a FRAME HANDLER really has. `MK3THREADFUNC` above is what
 * `StartThreadAt` takes; every handler stored in `mk3_frame(t, n)[1]` is
 * called by `mk3_update` as `long (*)(MK3THREAD *)` and its return value is
 * acted on -- negative abandons the frame, 0 re-runs the thread immediately,
 * and 0x16462 deletes it. The two types have coexisted because nothing called
 * a handler until mk3.c was decompiled. */
typedef long (*MK3THREADLONGFUNC)(MK3THREAD *thread);


void  KillSThread(MK3THREAD *thread);
void  StartThreadAt(MK3THREAD *thread, MK3THREADFUNC func);
void *GetThreadFunc(MK3THREAD *thread);

typedef struct MK3OBJPROC {
    /* 0x00  the other fighter's object.
     *
     * Six functions reach `obj->field00->field00` and every one of them is
     * named for the opponent -- ground_him, is_he_airborn, is_he_facing_me,
     * center_around_him, get_his_action, do_his_next_a9_frame -- so the
     * composite is what it looks like.
     *
     * The field is still called field00 and not `him`, because `him` is
     * already taken by 0x04 below, on the authority of a function name that
     * says so just as plainly. One of the two readings is incomplete and
     * nothing here settles which. */
    struct MK3OBJ *P32 field00;  /* 0x00 */
    uint32_t him;                /* 0x04  the opponent, per f_set_a10_to_him */
    uint32_t field08;            /* 0x08  the strength index, and what
                                  *       lights_on_hit passes on */
    uint8_t  _pad0c[4];
    uint32_t field10;            /* 0x10  isp2 ORs bit 4 into this */
    uint32_t field14;            /* 0x14  a counter inc_downcount bumps and
                                  *       reads straight back out */
    uint32_t field18;            /* 0x18  the action get_his_action reads and
                                  *       init_special_act writes */
    uint32_t field1c;            /* 0x1c  the animation rate */
    uint32_t field20;            /* 0x20  its counter, normally 1 */
    /* 0x24  The animation `plyrthread` has just resolved. It writes 0x28 and
     * 0x24 from the same register on the way out of every walk state, so the
     * two are the animation and its index kept together on the proc. */
    uint32_t field24;            /* 0x24 */
    uint32_t field28;            /* 0x28  who the shake is about: the shake
                                  *       pair write `him` or the object's
                                  *       0x08 here and differ in nothing
                                  *       else */
    uint32_t field2c;            /* 0x2c  t_lao_zap_call counts 2, 1, 2, 1 here
                                  *       -- a per-projectile frame counter that
                                  *       lives on the proc rather than the
                                  *       object, so it survives the object
                                  *       being reused */
    uint32_t field30;            /* 0x30  t_rocket1_proc clears it as the
                                  *       rocket launches. No other writer and
                                  *       no reader in the tree. */
    /* 0x34  A DIRECTION MASK. `mask_joystick` ANDs the four stick bits with
     * it, so a state can forbid individual directions by clearing them here
     * once instead of testing at every read. */
    uint32_t field34;
    uint32_t field38;            /* 0x38  the same idea in t_jax_proj_calla,
                                  *       counting 3, 2, 1 between effects */
    uint32_t field3c;            /* 0x3c  t_boomerang_call reads it as a mode:
                                  *       0 do nothing, 1 hand over to the
                                  *       return check, anything else use the
                                  *       value itself as a per-frame fall.
                                  *       No writer in the tree yet. */
    /* 0x40  ground_player copies it out as a HALFWORD, but
     * tl_do_lao_tele and tl_do_robo_tele both read it with `ldr`, a
     * full word, and compare the result against a signed y. Two
     * readings of one offset; neither is discarded. The word users
     * reach it as an offset rather than through this field.
     * Settled by tools/difftest: every other site in the tree is `ldr`/`str`
     * (t_another_scorpion stores an `ldrsh` result, sign-extended, and the
     * old halfword declaration truncated it), so it is a WORD.
     * ground_player's `ldrh` takes the low half, which is all
     * MK3_SET_FIELD12 keeps anyway. */
    uint32_t field40;
    uint32_t p_hit;              /* 0x44  per zero_my_p_hit */
    uint32_t field48;            /* 0x48  t_b_sweep writes it beside p_hit
                                  *       from the same register */
    uint32_t field4c;            /* 0x4c  t_block_exit writes p_hit back
                                  *       into it on the way out */
    uint32_t field50;            /* 0x50  reaction_start_chores clears it
                                  *       alongside field58 */
    uint32_t field54;            /* 0x54  add_combo_damage accumulates here */
    uint32_t field58;            /* 0x58  every punch swing writes a 2 here on
                                  *       its first frame, right after copying
                                  *       obj->field1c into field30 */
    uint32_t field5c;            /* 0x5c  t_sweep3 clears it (state 0xdea,
                                  *       field5c != 0 branch) right after
                                  *       shake_n_sound -- a single sighting */
    uint8_t  _pad60[4];
    uint32_t field64;            /* 0x64  the slave's object; delete_slave
                                  *       hands it to KillProc */
    uint32_t slave;              /* 0x68  per f_set_a10_to_slave */
    uint8_t  _pad6c[0x0c];
    struct MK3OBJ *P32 field78;      /* 0x78  the object t_rocket_explode hands to
                                  *       KillProc on both of its paths. Not
                                  *       the slave -- that is 0x64. Written
                                  *       by t_rocket2_proc's free launch:
                                  *       NewThreadProc(obj, t_target), a
                                  *       homing target thread. */
    uint16_t field7c;            /* 0x7c  the four-button gate, signed */
    uint16_t field7e;            /* 0x7e  read signed by plyrthread, beside the gate */
    uint8_t  _pad80[4];
    uint32_t field84;            /* 0x84  where tell_world_stk publishes the
                                  *       stick get_char_stk resolved, and what
                                  *       create_proj_proc clears. Two writers
                                  *       measured; no reader is in the tree. */
    struct MK3OBJ *P32 field88;      /* 0x88  a second object t_double_shaker moves
                                  *       alongside `him`. Which object is not
                                  *       settled -- nothing writes it yet. */
} MK3OBJPROC;

/* The high half of the word at 0x0c, which several routines read on its own
 * and one of them sign-extends. Little-endian: 0x0e is the top two bytes. */
#define MK3_FIELD0E(o)   ((uint16_t)((o)->field0c >> 16))

/* And the SIGNED read, which is what `gravity_n_bounds` actually does:
 * `ldrsh.w r3, [r2, #0xe]`. The coordinate is signed -- a fighter left of the
 * origin reads negative -- and that routine COMPARES it against the arena
 * bounds, where the unsigned form would wrap and clamp the wrong way.
 *
 * The unsigned macro above stays, because every other site so far only adds a
 * constant and stores the result back, where the two are identical. Any NEW
 * site that compares must use this one. */
#define MK3_FIELD0E_S(o) ((int16_t)((o)->field0c >> 16))

/* And the write. `multi_adjust_xy_ob` stores a halfword there, which on the
 * word at 0x0c is a read-modify-write of the top half. */
#define MK3_SET_FIELD0E(o, v)                                               \
    ((o)->field0c = ((o)->field0c & 0x0000ffffu)                            \
                    | ((uint32_t)(uint16_t)(v) << 16))

/* And the same pair for 0x10, whose high half is the halfword at 0x12. */
#define MK3_FIELD12(o)   ((uint16_t)((o)->field10 >> 16))
/* and signed, for the same reason MK3_FIELD0E_S exists: get_y_dist
 * subtracts two of these and takes the absolute value. */
#define MK3_FIELD12_S(o) ((int16_t)((o)->field10 >> 16))
#define MK3_SET_FIELD12(o, v)                                               \
    ((o)->field10 = ((o)->field10 & 0x0000ffffu)                            \
                    | ((uint32_t)(uint16_t)(v) << 16))

typedef struct MK3OBJ {
    MK3OBJPROC *P32 field00;         /* 0x00  ldr r2, [r4] */
    /* 0x04  the object's thread. KillProc and StartProcAt both take it out of
     * here and hand it to KillSThread / StartThreadAt, which is what makes the
     * pair the object-level spelling of the thread-level calls. */
    MK3THREAD  *P32 thread;          /* 0x04 */
    struct MK3OBJ *P32 field08;      /* 0x08  another object: player_swpal writes
                                  *       through it and ani2 passes it on */
    uint32_t    field0c;         /* 0x0c  a horizontal position; is_he_right
                                  *       compares two of them */
    /* 0x10  the second coordinate, and the same shape as 0x0c: a word whose
     * HIGH half is what almost everything reads. `match_ani_points_ob_ob` is
     * the one routine that takes the whole thing, and it takes 0x0c the same
     * way -- so the two are x and y as 16.16 fixed point, with the integer
     * part on top and a fraction underneath that only a copy preserves. */
    uint32_t    field10;         /* 0x10 */
    uint32_t    field14;         /* 0x14  cleared with 0x18 by
                                  *       strike_check_box, which is what
                                  *       makes the pair a pair */
    uint32_t    field18;         /* 0x18  the A8 pair, cleared together */
    uint32_t    field1c;         /* 0x1c  and the high bound */
    uint32_t    field20;         /* 0x20  and the low one */
    uint32_t    field24;         /* 0x24  borrowed by borrow_ochar_sound and
                                  *       used as a table index */
    uint32_t    field28;         /* 0x28  bit 4 toggled by the flip_multi trio */
    uint32_t    field2c;         /* 0x2c  receives the same OR-ed value, and
                                  *       is what GetFrameWidth is asked about */
    uint32_t    field30;         /* 0x30  the flag word the clearers mask */
    /* 0x34..0x40 is a BOUNDING BOX -- left, top, right, bottom, in the order
     * `intersect` establishes. `ani2_ob` fills all four from `mk3_getbbox` in
     * one call, and the four `*_mpart_ob` routines read exactly these: 0x34
     * and 0x3c as the horizontal pair, 0x38 as the top, 0x40 as the bottom.
     *
     * The same words are the ring buffer's base and head in `previous_q_entry`
     * and a table base in `decode_walk_table`. Spill slots, as everywhere else
     * in this file, so they keep their offset names. */
    uint32_t    field34;         /* 0x34  the box's left, and the queue base */
    uint32_t    field38;         /* 0x38  the base highest_mpart_ob adds to,
                                  *       and the ring buffer's head */
    uint32_t    field3c;         /* 0x3c  the second horizontal bound; 0x34 is
                                  *       the first */
    uint32_t    field40;         /* 0x40  and the one lowest_mpart_ob adds to */
    /* 0x44  what used to be the A10 register: the argument slot the arcade
     * loaded before a call. Three of the routines below do nothing but fill
     * it. */
    uint32_t    a10;             /* 0x44  the argument slot, and where
                                  *       get_his_a11_ani leaves the opponent */
    uint32_t    field48;         /* 0x48  shake_a11 passes it as an event, and
                                  *       get_his_a11_ani fetches through it */
    /* 0x4c  t_lk_prezap saves it on the argument stack alongside 0x20 and
     * 0x24 before stepping a frame, and restores it after -- a plain spill
     * slot, like the rest of this stretch. */
    uint32_t    field4c;         /* 0x4c */
    uint8_t     _pad50[4];
    uint32_t    field54;         /* 0x54  where a computed word is parked */
    uint8_t     _pad58[4];
    /* 0x5c  This file's BOOLEAN RETURN SLOT. am_i_joy isolates a bit into it,
     * check_block_bit and is_run_pressed leave a 0 or a 1 there. A routine that
     * answers a yes/no question writes it here rather than returning it. */
    uint32_t    field5c;         /* 0x5c */
    /* 0x60  THE BUTTON TABLE. `stuff_buttons` -- four bytes, the smallest
     * function in the directory -- does nothing but store a pointer here, and
     * the five tables it is handed are `bt_null`, `bt_stance`, `bt_duck`,
     * `bt_jump` and `bt_angle_jump`. Which moves a fighter can do is this one
     * pointer, and taking input away is `bt_null`. */
    uint32_t    field60;         /* 0x60 */
    /* 0x64/0x68  `t_mc_flipkp` writes these directly on the OBJECT rather
     * than through `field00`, the same "count and table pointer" reuse
     * `field00->field64`/`field00->slave` gets everywhere else in this
     * file -- the object apparently carries its own copy of the pair at
     * the identical offset. The struct is 0x6c bytes -- `Plyr`'s stride
     * is 108 and `no_ai_hack` reads `Plyr[1]` as `[r1, #0x6c]` -- so
     * these eight bytes exist and nothing else has been seen reading
     * them yet. */
    uint32_t    field64;         /* 0x64 */
    uint32_t    slave;           /* 0x68 */
} MK3OBJ;

/* ------------------------------------------------------------------------
 * `G`, the global game state.
 *
 * One letter, as it was on the TMS34010. Three routines here reach it through
 * the pointer slot at 0x000f357c, and the word that slot holds is 0x0038c1fc,
 * which the symbol table gives as `_G` in `__DATA,__common`.
 *
 * It is a `char` array and not a struct. Two offsets are established so far --
 * the strengths at 0x368 and a halfword at 0x456 -- and three accesses is not
 * a layout. Fields go in when there are enough of them to be a shape.
 * ------------------------------------------------------------------------ */

/* Called by isp2 and decompiled elsewhere. Declared, not defined: this file
 * owns the sequencing and not the steps. */
void face_opponent(MK3OBJ *obj);
void set_no_block(MK3OBJ *obj);
void me_in_front(MK3OBJ *obj);
void player_normpal(MK3OBJ *obj);
void rsnd_func(MK3OBJ *unused, uint32_t which);
void disable_all_buttons(MK3OBJ *obj);

/*
 * The ring buffer.
 *
 * `head` is a pointer, not an index — the original advances it with a
 * post-indexed store and wraps by comparing against the end of the array.
 * Twenty slots: the wrap check is against `q + 0x54`, the writes start at
 * `q + 4`, and each entry is four bytes.
 */
#define SWITCH_QUEUE_SLOTS  20

typedef struct SWITCHQUEUE {
    uint32_t *P32 head;                        /* +0x00 */
    uint32_t  slots[SWITCH_QUEUE_SLOTS];   /* +0x04 .. +0x53 */
} SWITCHQUEUE;

/*
 * The global game-state pointer.
 *
 * The original is NOT a function call. It loads a pointer from
 * `__DATA,__nl_symbol_ptr` at 0x000f357c, dereferences it, and reads a
 * uint16 at +0xa8:
 *
 *     ldr    r2, [pc, #0x20]     ; literal -> 0x0009d6a0
 *     add    r2, pc              ; -> 0x000f357c
 *     ldr    r2, [r2]            ; -> _G  (0x0038c1fc)
 *     ldrh.w r2, [r2, #0xa8]
 *
 * The symbol at the end of that chain is `_G`, the game's global state
 * struct. An earlier version of this file declared
 * `extern uint16_t proc_switch_counter(void)` and called it, which is
 * behaviourally equivalent under the differential test but structurally
 * wrong: there is no such function in the binary and no such name in its
 * symbol table. The name came from notes derived from a source we do not
 * use. What the binary does is read a field, so that is what this says.
 *
 * The field's purpose — a scheduling generation counter — is inferred from
 * how the value is used, and stays inferred until something confirms it.
 */
typedef struct GAMESTATE GAMESTATE;
extern GAMESTATE *G;                       /* 0x0038c1fc */

/* The second global, beside G and reached the same way -- through the pointer
 * slot at 0x000f349c, whose word is 0x0038c674 and whose symbol is `_H`. Four
 * words of it are cleared at the end of a match, and the two winner routines
 * say what they are: player 1 increments H[0] and H[2], player 2 increments
 * H[1] and H[3]. Two counters each, side by side, and the clear at the end of
 * a match resets exactly those four. */
extern char *H;                            /* 0x0038c674 */

/* The third global reached from this file, beside G and H, through the pointer
 * slot at 0x000f320c. `no_edge_both_players` is the only routine here that
 * touches it, and it sets one bit at two offsets -- so it is a byte array for
 * the same reason the other two are. */
extern char *GrObj;                        /* 0x0038c698 */

/* G carries two per-fighter blocks, 0x158 bytes apart. Four independent pairs
 * establish it and none of them alone would:
 *
 *      0x0b8 / 0x210   the x velocity, keyed by the object beside it
 *      0x0bc / 0x214   that object
 *      0x0c0 / 0x218   the bcq ring buffer
 *      0x114 / 0x26c   the jcq ring buffer
 *
 * So the offsets in this file are written as `0xb8` and `0x210` rather than as
 * `base + fighter * 0x158`: the stride is known, the base of the block is not,
 * and inventing one would put a name on the gap between 0x00 and 0xb8. */
#define G_FIGHTER_STRIDE  0x158

/* `_Plyr`, the other global array, and the two strides `StartGrObjAt` reveals.
 * Neither is written down anywhere: the GrObj one comes out of a modular
 * inverse the compiler used to avoid a divide, and the Plyr one out of a
 * shift-and-add multiply. */
extern char *Plyr;                         /* 0x0038cff4 */
#define GROBJ_STRIDE   76
#define PLYR_STRIDE   108

/* The third parallel array, and the third stride, both from
 * `Endurance_ClearPlayer` -- which reaches all three in one function and so is
 * the only place they can be compared. */
extern char *Pp;                           /* 0x0038dc9c */
#define PP_STRIDE     140

/* Declared further down as `GAMESTATE *G`, which is how GameCode.c spells it
 * too. The three accesses here are by byte offset because two reads and a
 * write are not a layout; where GAMESTATE grows fields, they replace these. */
#define G_BYTES  ((char *)G)

#define G_SWITCH_COUNTER(g)  (*(const uint16_t *)((const char *)(g) + 0xa8))

typedef struct MK3BOX {
    int32_t left;                /* 0x00 */
    int32_t top;                 /* 0x04 */
    int32_t right;               /* 0x08 */
    int32_t bottom;              /* 0x0c */
} MK3BOX;

/* ========================================================================
 * The object flags at 0x30.
 *
 * Ten routines, five clearing and five setting, all of the same five
 * instructions through `obj->field08`. The bits they name:
 *
 *      0x0001  noscroll        0x0080  ignore_y        0x0800  half_damage
 *      0x0004  no_block        0x0100  nocol           0x1000  quarter_damage
 *      0x0008  noflip          0x0400  noedge
 *      0x0010  shadow
 *      0x0020  inviso
 *
 * 0x0002, 0x0040 and 0x0200 have no routine in this file. They are left out
 * rather than filled in from their neighbours.
 * ======================================================================== */

#define MK3F_NOSCROLL        0x0001u
#define MK3F_NO_BLOCK        0x0004u
#define MK3F_NOFLIP          0x0008u
#define MK3F_SHADOW          0x0010u
#define MK3F_INVISO          0x0020u
#define MK3F_IGNORE_Y        0x0080u
#define MK3F_NOCOL           0x0100u
#define MK3F_NOEDGE          0x0400u
#define MK3F_HALF_DAMAGE     0x0800u
#define MK3F_QUARTER_DAMAGE  0x1000u

/* ========================================================================
 * The stick.
 *
 * Each writes a constant into 0x38 and asks `isa5`. The four constants are
 * 1, 2, 4 and 8 -- a bit mask, not a direction number, which one of them
 * alone could not have shown. A diagonal is therefore expressible, and
 * whether the game ever asks for one is a question for `isa5`.
 * ======================================================================== */

#define MK3_STICK_UP     1u
#define MK3_STICK_DOWN   2u
#define MK3_STICK_LEFT   4u
#define MK3_STICK_RIGHT  8u

/* The frame array lives at the thread's own address, eight bytes an entry,
 * indexed by 0xa4 -- the same arithmetic `GetThreadFunc` and
 * `t_self_terminate` use. */
static inline uint32_t *mk3_frame(MK3THREAD *thread, uint32_t n)
{
    return (uint32_t *)((char *)thread + n * 8);
}

/* The thread's SECOND stack. `t_striker` pushes two words at 0xa8 indexed by
 * 0xf8, four bytes an entry -- which fixes the whole layout, because the two
 * arrays and their two cursors tile the struct exactly:
 *
 *      0x00  frame[20], eight bytes each   ends at 0xa0
 *      0xa4  the frame index
 *      0xa8  args[20], four bytes each     ends at 0xf8
 *      0xf8  the arg cursor
 *
 * Twenty of each. The pads were sized from the offsets that had been seen
 * before this array existed, and they came out right, which is the check. */
static inline uint32_t *mk3_arg(MK3THREAD *thread, uint32_t n)
{
    return (uint32_t *)((char *)thread + 0xa8 + n * 4);
}


/* Replace the handler at the CURRENT level -- a tail call: the routine gives
 * up its place. `t_flight` and its kind INCREMENT the index instead, which is
 * a real call. The two differ by that one increment and most of this
 * directory is built out of the pair. */
static inline long mk3_push_handler(MK3THREAD *thread, MK3THREADFUNC handler)
{
    uint32_t current = thread->frame;

    if (*mk3_frame(thread, current + 1) != 0)
        return -3;

    mk3_frame(thread, current)[1] = (uint32_t)(uintptr_t)handler;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}


/* The same install with NO refusal in front of it.
 *
 * `mk3_push_handler` is really two things at once: the state-0 refusal and the
 * install. A routine whose only state is entry wants both, and the binary
 * agrees -- `t_attk5` tests the slot, returns -3 when it is dirty, and only
 * then stores. But a dispatcher that has already matched a non-zero token
 * cannot test it for zero: it IS non-zero, and the test would refuse every
 * time. So those routines branch into a shared tail that stores the handler
 * and clears the slot above, and nothing else. `t_settle_down` reaches that
 * tail from three states and `t_pounce_jsrp` from one; both jump to the same
 * six instructions the descend path falls through.
 *
 * Told apart in the disassembly by what reaches the store: a `cbz` on the slot
 * with `mvn r0, #2` on the other side is push_handler, and a plain branch in
 * is this. `tools/instck.py` looks for the sites where we picked wrong. */
static inline long mk3_install(MK3THREAD *thread, MK3THREADFUNC handler)
{
    mk3_frame(thread, thread->frame)[1] = (uint32_t)(uintptr_t)handler;
    *mk3_frame(thread, thread->frame + 1) = 0;
    return 0;
}

/* `blx ___divsi3`: signed divide with the ARM result where C has none. A zero
 * divisor gives 0 (what ARMv7 `sdiv` gives, and what the difftest oracle's
 * stub returns) and INT_MIN / -1 gives INT_MIN. Plain `/` traps on x86 in both
 * cases: t_rocket_hunt divides by a hypotenuse that can be 0. */
static inline int32_t mk3_sdiv(int32_t a, int32_t b)
{
    if (b == 0)
        return 0;
    if (b == -1)
        return (int32_t)(0u - (uint32_t)a);
    return a / b;
}

#endif /* MK3LOGIC_H */
