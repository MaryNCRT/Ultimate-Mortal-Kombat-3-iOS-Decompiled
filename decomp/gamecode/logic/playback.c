/*
 * playback.c -- gamecode/logic/playback.c, decompiled.
 *
 * The recorded-input player. `mk3_update` uses it for special moves: an input
 * word with bit 10 set names a sequence, `seq_lookup` finds it, and this file
 * then synthesises an ordinary joystick word every frame -- so a special move
 * is replayed as if a human had done the motion.
 *
 * Four functions. All four now written.
 *
 * ## The record is sixteen bytes
 *
 * `_Playback` is 32 bytes in the symbol table and there are two fighters, so
 * one record is 16 -- and `Playback_Begin` writes a word and four halfwords
 * inside that, which fits exactly.
 *
 *      0x00  the sequence               a word, the only thing Begin is given
 *      0x04  a halfword, cleared
 *      0x06  a halfword, cleared
 *      0x08  a halfword, cleared
 *      0x0a  the facing flag
 *      0x0c  THE OUTPUT WORD, and Begin does NOT clear it
 *
 * That last line matters. `mk3_update` reads `Playback[i].0x0c` as the
 * joystick word to use this frame, and `Playback_Begin` leaves it alone -- so
 * **the first frame of a new sequence replays whatever the previous one left
 * there**, until `Playback_Update` overwrites it. `mk3_update` does call
 * Update immediately after Begin, which is what makes that safe; a port that
 * separates the two will replay a stale input and not know why.
 */

#include <stdio.h>

#include "mk3logic.h"

/* ---------------------------------------------- Playback_Begin, Playback_Init
 *
 * armv7 0x000adb60 and 0x000adb70, 16 bytes each.  **Complete.**
 *
 *      Playback_Begin(p, seq, flip)   p->seq = seq
 *                                     p->w04 = p->w06 = p->w08 = 0
 *                                     p->flip = flip
 *
 *      Playback_Init(p)               Playback_Begin(p, 0, 0)
 *
 * The wrapper pair again, and this one is the cheapest kind: init is begin
 * with both arguments zero. A sequence of 0 is therefore "no sequence", which
 * is exactly what `mk3_update` tests -- `if (Playback[i].w0 == 0)` is how it
 * asks whether a special move is running.
 */
typedef struct MK3PLAYBACK {
    /* 0x00  **A POINTER into the sequence**, not an identifier.
     * `Playback_Begin` is handed a word and stores it; `Playback_Update`
     * fetches halfwords through it and writes the advanced pointer back. 0 is
     * "nothing is playing", which is what `mk3_update` tests. */
    const uint16_t *P32 seq;
    uint16_t field04;            /* 0x04 */
    uint16_t field06;            /* 0x06 */
    uint16_t field08;            /* 0x08 */
    uint16_t flip;               /* 0x0a  opponent_x < my_x, per mk3_update */
    uint16_t field0c;            /* 0x0c  the synthesised joystick word */
    uint16_t field0e;            /* 0x0e */
} MK3PLAYBACK;

/* `seq` arrives as a word because that is what `seq_lookup` returns and what
 * the binary stores -- `str r1, [r0]`, no type involved. It is a pointer, as
 * `Playback_Update` proves by walking it, so the cast is here and named rather
 * than hidden in the struct. */
void Playback_Begin(MK3PLAYBACK *p, long seq, long flip)
{
    p->seq     = (const uint16_t *)(uintptr_t)seq;
    p->field04 = 0;
    p->field06 = 0;
    p->field08 = 0;
    p->flip    = (uint16_t)flip;
}

void Playback_Init(MK3PLAYBACK *p)
{
    Playback_Begin(p, 0, 0);
}


/* ------------------------------------------------------------ Playback_Update
 *
 * armv7 0x000adaa8, 184 bytes.  **Complete.**
 *
 * **A bytecode interpreter, one instruction per frame.** The sequence is an
 * array of halfwords; bits 10..12 are the opcode and everything else -- bits
 * 0..9 and 13..15, the mask 0xe3ff -- is the payload.
 *
 *      0x0400   SET      sticky |= payload
 *      0x0800   CLEAR    sticky &= ~payload
 *      0x0C00   HOLD     hold = payload           (frames to stay put)
 *      0x1000   END      seq = NULL
 *      else     EMIT     transient = payload, for this frame only
 *                        and if the NEXT word repeats this one, hold 1
 *
 *      output = sticky | transient
 *
 * Every opcode costs a frame: SET and CLEAR emit and return like the rest, so
 * "hold left for five" is six words of wall-clock, not five. A port that
 * executes opcodes until it finds an EMIT will run every special move faster
 * than the original.
 *
 * The sticky word at 0x08 survives between frames and the transient at 0x06
 * does not -- it is cleared at the top of every call. That is the whole state
 * model: a button you hold is SET once, a tap is one EMIT.
 *
 * ## The flip settles which bits are the horizontal pair
 *
 * When `flip` is set the output has **bits 2 and 3 exchanged**, and nothing
 * else changes:
 *
 *      out = (out & 0xfff3) | ((out & 8) ? 4 : 0) | ((out & 4) ? 8 : 0)
 *
 * Mirroring a recorded motion swaps left and right and leaves up and down
 * alone. So **bits 2 and 3 are the horizontal pair and bits 0 and 1 are the
 * vertical one** -- which the input contract at the top of joy.c had to leave
 * open, because `joystick_in_a0` masks all four with 0xf and never separates
 * them.
 *
 * It does NOT say which of 2 and 3 is left: swapping a pair is symmetric. Same
 * for 0 and 1. The PAIRING is settled and the polarity is not.
 *
 * `flip` is read with `ldrsh` and tested against zero, and `mk3_update` sets it
 * from `opponent_x < my_x` -- so a sequence is authored facing one way and
 * mirrored when the fighter is facing the other.
 */
void Playback_Update(MK3PLAYBACK *p)
{
    uint16_t sticky, out;

    p->field06 = 0;             /* the transient bits last one frame */
    p->field0c = 0;

    if (p->seq == NULL)
        return;

    if (p->field04 != 0) {
        /* Still holding: no instruction is fetched this frame. */
        sticky = p->field08;
        p->field04 = (uint16_t)(p->field04 - 1);
    } else {
        uint16_t        word    = *p->seq;
        uint16_t        payload = (uint16_t)(word & 0xe3ffu);
        unsigned        op      = (unsigned)(word & 0x1c00u);
        const uint16_t *next    = p->seq + 1;

        p->seq = next;

        switch (op) {
        case 0x0400:                            /* SET */
            sticky = (uint16_t)(payload | p->field08);
            p->field08 = sticky;
            break;

        case 0x0800:                            /* CLEAR */
            sticky = (uint16_t)(~payload & p->field08);
            p->field08 = sticky;
            break;

        case 0x0c00:                            /* HOLD */
            sticky = p->field08;
            p->field04 = payload;
            break;

        case 0x1000:                            /* END */
            sticky = p->field08;
            p->seq = NULL;
            break;

        default:                                /* EMIT */
            sticky = p->field08;
            p->field06 = payload;
            /* A repeated word means hold it one more frame. The peek is at the
             * ALREADY ADVANCED pointer, so it compares this word with the one
             * after it. */
            if (word == *next)
                p->field04 = 1;
            break;
        }
    }

    out = (uint16_t)(sticky | p->field06);

    if ((int16_t)p->flip != 0) {
        uint16_t swapped = (uint16_t)(out & 0xfff3u);
        if (out & 8u) swapped = (uint16_t)(swapped | 4u);
        if (out & 4u) swapped = (uint16_t)(swapped | 8u);
        out = swapped;
    }

    p->field0c = out;
}


/* ------------------------------------------------------------- seq_lookup
 *
 * armv7 0x000adb80, 7,608 bytes -- the largest function in the directory by
 * a factor of three, and the reason for it: 6,702 of those bytes are DATA,
 * not code. The 315 real instructions are a two-level dispatch; everything
 * else is the two jump tables and, past the end of the function, the
 * sequences themselves.
 *
 * **The outer level is 23 sets, an ordinary `tbh`.** `set` is checked
 * against 22 (`cmp r4, #0x16 ; bhi -> return 0`) and dispatches through a
 * halfword table -- the normal, already-documented `tbh` shape.
 *
 * **The inner level is not `tbh`. It is a table of `b.w` instructions,
 * used as data.** Each of the 23 branches lands on:
 *
 *      cmp r5, #LIMIT          ; LIMIT is 0x13, 0x12 or 0x8, per set
 *      bhi ret0
 *      addw r3, pc, #N
 *      add.w r3, r3, r5, lsl #2
 *      mov pc, r3
 *      <the table: LIMIT+1 four-byte b.w instructions, in place>
 *
 * `mov pc, r3` jumps INTO the table rather than reading a pointer out of
 * it -- so every entry has to be a real, executable `b.w`, and the table is
 * exactly as many four-byte slots as `set` has valid indices. Reading it
 * wrong by two bytes (the natural mistake: `addw r3, pc, #N` looks like it
 * should point at the table, and by the letter of the encoding it does, but
 * one register-allocator artifact -- the compiler emits `mov pc, r3` as a
 * 2-byte T1 form, not the 4-byte wide form -- lands the arithmetic one
 * halfword short) decodes each entry as an ordinary 16-bit instruction
 * instead of the top half of a `b.w`, and every target comes out
 * plausible-looking and wrong. Caught by disassembling from three
 * candidate starts and keeping the one where six consecutive decodes are
 * all `b.w` to addresses inside this same function -- the other two starts
 * each produce a `rev`/`revsh`/`it`-block salad that doesn't hang together
 * as a program.
 *
 * **Every branch this table can reach lands on the same four-instruction
 * shape:**
 *
 *      ldr r0, [pc, #N]         ; a literal: the base of one sequence table
 *      lsls r3, r6, #2
 *      lsls r2, r6, #4
 *      add  r2, r3              ; r2 = mode * 20  (4 + 16)
 *      add  r0, pc              ; r0 = the literal, resolved
 *      add  r0, r2
 *      b.w  ret0                ; shared tail, returns r0
 *
 * So **the whole function is a lookup in a fixed 23-by-(up to 20) grid of
 * table addresses**, followed by `+ mode * 20`. `SEQ_TABLE` below is that
 * grid, walked out of the binary by resolving all 23 inner tables and every
 * `b.w` each one contains -- 449 entries, of which 138 are real. The rest
 * are the `bhi ret0` refusals the binary's own range checks produce, and
 * are `NULL` here for the same reason.
 *
 * **None of the 138 table addresses have a symbol.** They sit in
 * `__TEXT,__const` (0xddf2c-0xe0404) among the 229 data tables
 * `docs/PROGRESS.md` already flags as uncounted -- this is that gap, met
 * directly rather than described. What is verified is the STRUCTURE: which
 * (set, idx) pairs are valid and where each one's table starts. What each
 * table's own bytes mean -- `_Playback`'s bytecode, per `Playback_Update`
 * above -- is a question for whoever names `sm_scorp_hpc` and its 119
 * siblings.
 *
 * **One address, `0xde242`/`0xde244`, is shared by nineteen of the twenty
 * three sets at idx 14.** Not a decoding accident -- verified independently
 * per set, same address every time. Whatever idx 14 is, it is common to
 * almost every set rather than per-character, which reads as a shared
 * "neutral" or terminator sequence rather than 19 coincidences.
 *
 * **The debug print, read out of the entry block:**
 *
 *      mov r3, r6                     ; r3 = mode, saved before the format loads
 *      ldr r0, [pc, #N] ; add r0, pc  ; the format string
 *      mov r1, r4                     ; set
 *      mov r2, r5                     ; idx
 *      blx _printf
 *
 * `printf(fmt, set, idx, mode)` -- confirmed against the play-session log in
 * docs/PROGRESS.md, `seq_lookup( 18, 0, 1 )` and 365 lines like it: first
 * number is `set` (1-18 observed, this function accepts 0-22), second is
 * `idx`, third is `mode`, and `mode` is 1 in every one of the 366 lines
 * ever captured -- which is why nothing in this project has had reason to
 * read what a `mode` other than 1 would mean.
 *
 * The format string itself was read out of the literal pool byte for byte --
 * `"seq_lookup( %d, %d, %d );\n"`, at 0x0017c904 -- rather than assumed from
 * the log line's shape.
 *
 * **`landfn.sh` reports FALLA on this function, and it is the verifier that
 * is wrong, not the two lines below.** `printf` is this project's first call
 * to an imported C library function, and the first format string that
 * happens to spell its own function's name; both trip a real gap in
 * `factdiff.py`, written up as entry 7 of "Known gaps" in that file rather
 * than worked around here with something less faithful than the real call
 * and the real string. Every other fact in this function -- both switch
 * levels, all 449 table entries, the final `+ mode * 20` and the `NULL`
 * paths -- came back `1 coinciden` clean.
 */

/* Row i is seq_lookup set i; each cell is the table for one index inside
 * that set, or NULL where the binary's own switch refuses the index.
 * Walked out of the binary; see the banner above for how. */
static void *const SEQ_TABLE[23][20] = {
    { (void*)0xdecbc, (void*)0xded0a, (void*)0xded34, (void*)0xdece2, NULL, (void*)0xded5c, NULL, NULL, NULL, NULL, NULL, NULL, NULL, (void*)0xdee4a, (void*)0xde244, (void*)0xded82, (void*)0xdedac, (void*)0xdedd2, (void*)0xdedfc, (void*)0xdee22 },  /* set 0 */
    { (void*)0xde4c4, (void*)0xde4ea, (void*)0xde514, (void*)0xde53a, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, (void*)0xde62c, (void*)0xde242, (void*)0xde564, (void*)0xde58a, (void*)0xde5b4, (void*)0xde5da, (void*)0xde604 },  /* set 1 */
    { (void*)0xde6ca, (void*)0xde654, (void*)0xde6a2, (void*)0xde6f4, (void*)0xde67a, NULL, NULL, NULL, (void*)0xde67c, NULL, NULL, NULL, NULL, (void*)0xde7e2, (void*)0xde244, (void*)0xde71c, (void*)0xde744, (void*)0xde76a, (void*)0xde794, (void*)0xde7ba },  /* set 2 */
    { (void*)0xde80a, (void*)0xde884, (void*)0xde85a, (void*)0xde834, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, (void*)0xde972, (void*)0xde244, (void*)0xde8d2, (void*)0xde8ac, (void*)0xde8fa, (void*)0xde924, (void*)0xde94c },  /* set 3 */
    { (void*)0xdee72, (void*)0xdee9c, (void*)0xdef12, (void*)0xdef3c, NULL, (void*)0xdeeea, NULL, NULL, (void*)0xdeec4, NULL, NULL, NULL, NULL, (void*)0xdf02a, (void*)0xde244, (void*)0xdef8a, (void*)0xdef64, (void*)0xdefb2, (void*)0xdefdc, (void*)0xdf004 },  /* set 4 */
    { (void*)0xdf3c2, (void*)0xdf34c, (void*)0xdf39a, (void*)0xdf3ec, (void*)0xdf372, NULL, NULL, NULL, (void*)0xdf374, NULL, NULL, NULL, NULL, (void*)0xdf4da, (void*)0xde244, (void*)0xdf43a, (void*)0xdf414, (void*)0xdf462, (void*)0xdf48c, (void*)0xdf4b2 },  /* set 5 */
    { (void*)0xdf1e2, (void*)0xdf234, (void*)0xdf20a, (void*)0xdf1bc, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, (void*)0xdf322, (void*)0xde244, (void*)0xdf282, (void*)0xdf25c, (void*)0xdf2aa, (void*)0xdf2d4, (void*)0xdf2fa },  /* set 6 */
    { (void*)0xdf054, NULL, (void*)0xdf07a, (void*)0xdf0a4, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, (void*)0xdf192, (void*)0xde244, (void*)0xdf0ca, (void*)0xdf0f4, (void*)0xdf11a, (void*)0xdf144, (void*)0xdf16a },  /* set 7 */
    { (void*)0xdf502, (void*)0xdf554, (void*)0xdf52a, (void*)0xdf5a4, (void*)0xdf57a, NULL, NULL, NULL, NULL, (void*)0xdf57c, NULL, NULL, NULL, (void*)0xdf692, (void*)0xde244, (void*)0xdf5ca, (void*)0xdf5f4, (void*)0xdf61a, (void*)0xdf644, (void*)0xdf66c },  /* set 8 */
    { (void*)0xdf6ba, (void*)0xdf734, (void*)0xdf70a, (void*)0xdf6e4, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, (void*)0xdf822, (void*)0xde244, (void*)0xdf782, (void*)0xdf75c, (void*)0xdf7aa, (void*)0xdf7d4, (void*)0xdf7fa },  /* set 9 */
    { (void*)0xdf84a, (void*)0xdf874, NULL, (void*)0xdf89a, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, (void*)0xdf98c, (void*)0xde242, (void*)0xdf8c4, (void*)0xdf8ea, (void*)0xdf912, (void*)0xdf93c, (void*)0xdf962 },  /* set 10 */
    { (void*)0xdf9b2, NULL, (void*)0xdfa04, (void*)0xdf9da, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, (void*)0xdfaf4, (void*)0xde242, (void*)0xdfa2c, (void*)0xdfa52, (void*)0xdfa7c, (void*)0xdfaa4, (void*)0xdfaca },  /* set 11 */
    { (void*)0xdfb44, (void*)0xdfb92, (void*)0xdfb6c, (void*)0xdfbba, NULL, (void*)0xdfb1c, NULL, NULL, NULL, NULL, NULL, NULL, NULL, (void*)0xdfcaa, (void*)0xde244, (void*)0xdfbe2, (void*)0xdfc0c, (void*)0xdfc32, (void*)0xdfc5c, (void*)0xdfc82 },  /* set 12 */
    { (void*)0xdfcd2, (void*)0xdfd4c, (void*)0xdfd22, (void*)0xdfcfc, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, (void*)0xdfe3a, (void*)0xde244, (void*)0xdfd72, (void*)0xdfd9c, (void*)0xdfdc2, (void*)0xdfdec, (void*)0xdfe14 },  /* set 13 */
    { (void*)0xdfe64, NULL, (void*)0xdfe8a, (void*)0xdfeb4, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, (void*)0xdffa2, (void*)0xde244, (void*)0xdfeda, (void*)0xdff04, (void*)0xdff2a, (void*)0xdff54, (void*)0xdff7a },  /* set 14 */
    { (void*)0xde26a, (void*)0xde294, (void*)0xde2ba, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, (void*)0xde3ac, (void*)0xde242, (void*)0xde30c, (void*)0xde2e2, (void*)0xde334, (void*)0xde35c, (void*)0xde382 },  /* set 15 */
    { (void*)0xdea3a, (void*)0xde99c, (void*)0xdea62, (void*)0xde9ec, NULL, (void*)0xde9c2, NULL, NULL, (void*)0xdea14, NULL, NULL, NULL, NULL, (void*)0xdeb52, (void*)0xde244, (void*)0xdeab2, (void*)0xdea8c, (void*)0xdeada, (void*)0xdeb04, (void*)0xdeb2a },  /* set 16 */
    { (void*)0xdffcc, (void*)0xe001a, NULL, (void*)0xdfff4, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, (void*)0xe010a, (void*)0xde244, (void*)0xe006a, (void*)0xe0044, (void*)0xe0092, (void*)0xe00bc, (void*)0xe00e2 },  /* set 17 */
    { (void*)0xdeb7a, NULL, (void*)0xdeba4, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, (void*)0xdec92, (void*)0xde244, (void*)0xdebca, (void*)0xdebf4, (void*)0xdec1a, (void*)0xdec44, (void*)0xdec6c },  /* set 18 */
    { (void*)0xde3d4, (void*)0xde3fa, (void*)0xde424, (void*)0xde44a, NULL, (void*)0xde474, NULL, NULL, (void*)0xde49a, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },  /* set 19 */
    { (void*)0xe0132, NULL, (void*)0xe015c, (void*)0xe0182, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, (void*)0xe0224, (void*)0xde242, (void*)0xe01d4, (void*)0xe01aa, NULL, (void*)0xe01fc, NULL },  /* set 20 */
    { (void*)0xe024a, NULL, (void*)0xe0274, (void*)0xe029a, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, (void*)0xe0314, (void*)0xde242, (void*)0xe02c4, NULL, NULL, (void*)0xe02ea, NULL },  /* set 21 */
    { (void*)0xe033c, NULL, (void*)0xe0362, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, (void*)0xe03dc, (void*)0xde244, (void*)0xe038c, NULL, NULL, (void*)0xe03b2, NULL },  /* set 22 */
};

const unsigned char *umk3_seq_addr(unsigned long ios_address);

void *seq_lookup(long set, long idx, long mode)
{
    void *base;

    printf("seq_lookup( %d, %d, %d );\n", (int)set, (int)idx, (int)mode);

    if ((unsigned long)set > 22)
        return NULL;

    if ((unsigned long)idx > 19)
        return NULL;

    base = SEQ_TABLE[set][idx];
    if (base == NULL)
        return NULL;

    /* The table holds the binary's addresses; the bytes they name are
     * extracted at build time (tools/seq_data.py), and this is the one
     * place an address becomes a pointer. Handed back raw, the first
     * special move of a fight read script words from 0x000decbc. */
    return (void *)umk3_seq_addr((unsigned long)(uintptr_t)base + mode * 20);
}

