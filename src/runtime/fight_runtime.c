/*
 * fight_runtime.c -- what the fight engine needs from outside itself.
 *
 * `decomp/gamecode/logic` is complete and links against two things it does
 * not contain:
 *
 *   1. **Its data.** ~1,100 tables -- special-move lists, animation scripts,
 *      reaction and AI tables -- extracted from the user's binary at build time
 *      by `tools/logic_tables.py` into `logic_tables.c` in the build directory.
 *      That file also holds zero storage for the engine's runtime state
 *      (`umk3_common_G`, `umk3_common_Plyr` ...), because tables point into it.
 *
 *   2. **Its slot pointers.** The binary reaches every global through a
 *      literal-pool word holding the global's address, and the decompiled C
 *      keeps that shape: `char *Plyr`, `GAMESTATE *G`, `MK3THREAD *mytc`. This
 *      file defines those pointers and aims them at the storage above.
 *
 * Every address a slot is given here was read from the binary: the slot's own
 * address is in the comment on the extern that declares it, and its contents
 * are what `tools/logic_tables.py --report` and the symbol table say. They are
 * repeated below so this file can be checked without the decompiled sources.
 *
 * ## Shared with the front end
 *
 * `G`, `H`, `MKEventQueue` and `RoundParam` belong to the front end as well,
 * and `runtime/gamecode_globals.c` defines them when the front end is linked
 * (`UMK3_SHELL`). There they start out NULL or pointing at the front end's own
 * copy, so `fight_runtime_init` re-aims them at the one storage the tables
 * also point into. A fight that ran on two copies of `G` -- the switch words
 * `swtab` writes through and the ones `TranslateJoybits` reads -- would not
 * take input, and would give no other sign of why.
 */

#include <stdio.h>
#include <stdint.h>

#include "fight_runtime.h"
#include "../decomp/gamecode/logic/mk3logic.h"

/* ---- storage, in logic_tables.c (generated) ---- */

extern uintptr_t umk3_common_G[], umk3_common_H[], umk3_common_GrObj[];
extern uintptr_t umk3_common_MKEventQueue[], umk3_common_Playback[];
extern uintptr_t umk3_common_Plyr[], umk3_common_Pp[], umk3_common_RoundParam[];
extern uintptr_t umk3_common_mo[], umk3_common_mytc[];
extern uintptr_t switch_close_jumps[], switch_open_jumps[];
extern uintptr_t TList[];

/* ---- the slots the fight engine reads ---- */

char      *Plyr     = (char *)umk3_common_Plyr;      /* -> 0x0038cff4 */
char      *Pp       = (char *)umk3_common_Pp;        /* slot 0x000f3158 -> 0x0038dc9c */
char      *GrObj    = (char *)umk3_common_GrObj;     /* slot 0x00165668 -> 0x0038c698 */
char      *mo       = (char *)umk3_common_mo;        /* slot 0x00165680 -> 0x0038ed5c */
char      *Playback = (char *)umk3_common_Playback;  /* slot 0x00165660 -> 0x0038cfd4 */
MK3THREAD *mytc     = (MK3THREAD *)umk3_common_mytc; /* slot 0x0016566c -> 0x0038ef3c */

/* other.c: `table = flag ? SwitchTableA : SwitchTableB` */
void *P32  *SwitchTableA   = (void *P32 *)switch_close_jumps; /* slot 0x000f3204 -> 0x0016eb04 */
void *P32  *SwitchTableB   = (void *P32 *)switch_open_jumps;  /* slot 0x000f3210 -> 0x0016edd4 */
/* other.c walks the list from the head word itself, as if it were a node */
MK3THREAD *P32 *ThreadListHead = (MK3THREAD *P32 *)TList;         /* slot 0x000f3220 -> 0x0038ed48 */

/* mkstat.c flushes it between uppercut frames; the slot at 0x000f3180 is the
 * non-lazy pointer to the C library's `___stdoutp`. */
static FILE *umk3_stdoutp;
void       **uppercut_stream = (void **)&umk3_stdoutp;

#ifdef UMK3_SHELL
/* The front end owns these; they are re-aimed in fight_runtime_init. */
extern GAMESTATE *G;
extern char      *H;
extern void      *MKEventQueue;
extern long      *RoundParam;
extern long       Destiny;
/* mkboss.c's `*Difficulty`: the slot holds 0x0014e20c, which is `_Destiny`. */
int32_t *Difficulty = (int32_t *)&Destiny;
#else
GAMESTATE *G            = (GAMESTATE *)umk3_common_G;
char      *H            = (char *)umk3_common_H;
void      *MKEventQueue = umk3_common_MKEventQueue;
long      *RoundParam   = (long *)umk3_common_RoundParam;
/* `_Destiny`, 0x0014e20c, initial value -1 (the front end's ladder choice) */
long       Destiny      = -1;
int32_t   *Difficulty   = (int32_t *)&Destiny;
#endif

void fight_runtime_init(void)
{
    umk3_stdoutp = stdout;
    G            = (GAMESTATE *)umk3_common_G;
    H            = (char *)umk3_common_H;
    MKEventQueue = umk3_common_MKEventQueue;
    RoundParam   = (long *)umk3_common_RoundParam;
}

/* Port, not in the binary: the device's rand() is its C library's, and the
 * fight engine draws from it (random32 in logic/other.c, mkdrone.c). The C
 * library's own sequence lives outside the game's state and differs between
 * Windows and Linux, so the game's files get this one instead (savestate.h):
 * the Windows C runtime's formula, seeded 1 as the game never calls srand,
 * so a Windows build draws the way it did before. */
uint32_t umk3_rand_seed = 1;

/* limeRand's seed (lime_menu.c), here for the same reason: lime_menu.c holds
 * the machine's clock and sound table, which a rollback must not put back. */
int32_t rand_seed = 0x00089c84;

int umk3_rand(void)
{
    umk3_rand_seed = umk3_rand_seed * 214013u + 2531011u;
    return (int)((umk3_rand_seed >> 16) & 0x7fff);
}
