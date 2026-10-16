/*
 * sound.c — src/gamecode/sound.cpp
 *
 * Eight functions in the original. Verified against the oracle by
 * tests/test_gamecode2_diff.c.
 */

#include <stdint.h>
#include <string.h>   /* strcmp, for UnLoadSoundList */

/* A table of pointers to per-character sound tables. `get_csound` indexes the
 * outer one by four and the inner by eight, so the inner entries are pairs and
 * the field wanted is the second half of each. Neither is named further: one
 * function reading a stride is not a layout. */
typedef struct CSOUNDENTRY {
    uint32_t field00;            /* 0x00 */
    uint32_t field04;            /* 0x04  what get_csound returns */
} CSOUNDENTRY;

extern CSOUNDENTRY *ochar_sound_tables[];    /* 0x0017b278 */

uint32_t get_csound(long index, long character);


/* --------------------------------------------------------------- get_csound
 *
 * armv7 0x000a7ec0, 20 bytes.
 *
 *      lsls r0, r0, #3              ; index * 8
 *      ldr.w r3, [r3, r1, lsl #2]   ; ochar_sound_tables[character]
 *      adds r0, r0, r3
 *      ldr  r0, [r0, #4]            ; entry[index].field04
 *
 * Two levels, two different scales, and the arguments are not interchangeable:
 * the SECOND selects the table and the FIRST indexes into it. Swapping them
 * compiles, runs, and returns a plausible number from the wrong character.
 */
uint32_t get_csound(long index, long character)
{
    return ochar_sound_tables[character][index].field04;
}


typedef struct SOUNDENTRY {             /* 8 bytes: the walk advances by 8 */
    const char *P32 name;                   /* 0x00 */
    long        id;                     /* 0x04, -1 when not loaded */
} SOUNDENTRY;

/* `_triple_sndtab` 0x001790b8 is a SOUNDENTRY list: LoadAllSounds (0xa82d4)
 * passes it to LoadSoundList like the other 43, whose `ldr r0,[list]` /
 * `strcmp` reads word 0 as a name. Its first word, 0x00178c04, is a
 * __cstring address. Declared `long[][2]`, the generated definition kept
 * those addresses as integers and the first strcmp on the host faulted. */
extern SOUNDENTRY triple_sndtab[];
int printf(const char *fmt, ...);


/* ---------------------------------------------------------------- get_tsound
 *
 * armv7 0x000a8538, 32 bytes.  **Complete.**
 *
 *      printf("Tsound 0x%x %d\n", id, id)
 *      r0 = triple_sndtab[id][1]
 *
 * **The printf is not debug scaffolding left in by accident -- it ships.** It
 * runs on every call, in the retail build, and prints the same value twice.
 * That is worth transcribing rather than dropping: a port that removes it is
 * fine, but a port that removes it WITHOUT noticing has quietly changed how
 * expensive this function is, and it is called from the sound path.
 *
 * The table entry is read at +4, so each row is two words and the first is
 * something this function never looks at.
 */
long get_tsound(long id)
{
    printf("Tsound 0x%x %d\n", (unsigned)id, (int)id);
    return triple_sndtab[id].id;
}


extern long  rsnd_choices[];            /* 0x0017b370, one count per id */
extern long *rsnd_table[];              /* 0x0017b330, one row array per id */


/* ---------------------------------------------------------------- get_rsound
 *
 * armv7 0x000a7ed4, 52 bytes.  **Complete.**
 *
 *      if (seed < 0) seed = -seed      <- `it lt; rsblt`
 *      seed %= rsnd_choices[id]
 *      row = rsnd_table[id]
 *      return row[seed * 2 + 1]
 *
 * A random-sound picker with the randomness supplied by the caller. The
 * negation matters: `__modsi3` on a negative dividend gives a negative
 * remainder in C, which would index backwards, so the sign is taken off first
 * rather than the result being fixed up afterwards.
 *
 * The row stride is 8 and the value read is at +4, the same shape get_tsound
 * uses on `_triple_sndtab`.
 */
long get_rsound(long id, long seed)
{
    if (seed < 0)
        seed = -seed;
    seed %= rsnd_choices[id];
    return rsnd_table[id][seed * 2 + 1];
}


extern long  group_choices[];           /* 0x0017b2e0 */
extern long *group_table[];             /* 0x0017b308 */


/* ---------------------------------------------------------------- get_gsound
 *
 * armv7 0x000a7e84, 48 bytes.  **Complete.**
 *
 *      if (seed < 0) seed = -seed
 *      n   = group_choices[group]
 *      idx = n * variant + (seed % n)
 *      return group_table[group][idx * 2 + 1]
 *
 * The same shape as get_rsound with one more dimension: `variant` selects a
 * BLOCK of `n` sounds and the seed picks within it, so the table is a flat
 * array indexed as if it were two-dimensional. `mla r1, r4, r6, r0` is that
 * whole index in one instruction.
 *
 * The negation before the modulus is the same guard get_rsound has, and for the
 * same reason: a negative remainder would index backwards.
 */
long get_gsound(long group, long variant, long seed)
{
    long n;

    if (seed < 0)
        seed = -seed;
    n = group_choices[group];
    return group_table[group][(n * variant + (seed % n)) * 2 + 1];
}


#define SOUND_UNIQUE_NAME_STRIDE 32

extern long  SoundListUniqueCounter;    /* 0x0017b3b0 */
extern long  SoundListUniqueHandle[];   /* 0x0038b8b0, one word each */
extern char  SoundListUniqueNames[];    /* 0x003878b0, stride 32 */
void limeDeleteSound(long id);


/* ------------------------------------------------------------- UnLoadSoundList
 *
 * armv7 0x000a7f08, 148 bytes.  **Complete.**
 *
 *      for each entry until name == "end_of_list":
 *          if (entry->id != -1) {
 *              for (i = 0; i < SoundListUniqueCounter; i++)
 *                  if (SoundListUniqueHandle[i] == entry->id) {
 *                      limeDeleteSound(entry->id)
 *                      SoundListUniqueNames[i * 32] = 0
 *                      SoundListUniqueHandle[i] = -1
 *                  }
 *              entry->id = -1
 *          }
 *
 * **The table is `_SoundListUniqueHandle`, the one LoadSoundList fills**:
 * 0xa7f5c-0xa7f60 build its address as 0x2e394e + pc (0xa7f62) = 0x0038b8b0.
 * This used to read a `SoundListUniqueIds` that is not in the symbol table
 * and that nothing wrote, so no match was ever found and no fight sound was
 * ever deleted: every fight leaked its whole set into limeLoadSound's 512
 * slots, the table was full after about two fights, and from then on the
 * sounds loaded for the next fight came back -1 -- silent. Mary, 2026-10-09:
 * "after two fights in a row the audio stops working properly".
 *
 * **The list ends on a STRING, not a null pointer**: every entry's name is
 * strcmp'd against "end_of_list". So a sound list is terminated by a sentinel
 * row, and a NULL name would crash rather than stop.
 *
 * **The inner loop does not break on a match.** It deletes, clears the name's
 * first byte and sets the id to -1, and then keeps scanning -- so a duplicated
 * id in the unique table would be deleted twice. Whether that can happen
 * depends on how the table is built, which this function does not say; what it
 * does say is that the original does not assume it cannot.
 *
 * Only the first BYTE of the 32-byte name slot is cleared, which is enough to
 * make it an empty C string and leaves the rest as it was.
 */
void UnLoadSoundList(SOUNDENTRY *list)
{
    long i;

    for (; strcmp(list->name, "end_of_list") != 0; list++) {
        if (list->id == -1)
            continue;

        for (i = 0; i < SoundListUniqueCounter; i++) {
            if (SoundListUniqueHandle[i] != list->id)
                continue;

            limeDeleteSound(list->id);
            SoundListUniqueNames[i * SOUND_UNIQUE_NAME_STRIDE] = 0;
            SoundListUniqueHandle[i] = -1;
            /* no break: the original keeps scanning */
        }
        list->id = -1;
    }
}


long limeLoadSound(const char *name);
int  strcmp(const char *a, const char *b);
char *strcpy(char *d, const char *s);


/* ------------------------------------------------------------- LoadSoundList
 *
 * armv7 0x000a8210, 196 bytes.  **Complete.**
 *
 * Walks a `{name, handle}` list, eight bytes an entry, loading each sound once
 * and sharing the handle with every later entry that names the same file.
 *
 * Two terminators, and they are not the same:
 *
 *      strcmp(name, <a literal>) == 0   ->  RETURN, the list is over
 *      name[0] == 0                     ->  skip this entry, keep going
 *
 * So an empty string is a hole in the list, not the end of it. Only the
 * sentinel string ends the walk.
 *
 * ### The loop break is written as a poisoned counter
 *
 * On a name that is already in the unique table it does not branch out. It sets
 * the loop counter to `2 * count` and lets the loop condition fail:
 *
 *      i = 2 * count;  i++;  if (i < count) continue;
 *      if (count == i) load;               <- now false
 *
 * The second test is what actually distinguishes "ran to the end without a
 * match" from "found one", and it only works because the poisoned value can
 * never equal `count`. Rewriting this as a `break` is safe; rewriting it as a
 * flag that happens to be set on both paths is not, and the shape is recorded
 * so nobody has to work that out twice.
 *
 * A new sound is loaded, its handle written back into the caller entry, and
 * both the name and the handle appended to the unique table -- the name with
 * `strcpy` into a 32-byte slot, unbounded.
 */
void LoadSoundList(SOUNDENTRY *list)
{
    for (;;) {
        const char *name = list->name;
        long i;

        /* 0xa821a-0xa822a: the literal is 0x00178be8, "end_of_list" -- the
         * same sentinel UnLoadSoundList stops at. Read as "", every list
         * stopped at its first unused entry or ran off its end. */
        if (strcmp(name, "end_of_list") == 0)
            return;

        if (name[0] != 0) {
            for (i = 0; i < SoundListUniqueCounter; i++) {
                if (strcmp(name,
                           &SoundListUniqueNames[i * SOUND_UNIQUE_NAME_STRIDE]) == 0) {
                    list->id = SoundListUniqueHandle[i];
                    break;              /* the poisoned counter, see above */
                }
            }

            if (i == SoundListUniqueCounter) {
                list->id = limeLoadSound(name);
                strcpy(&SoundListUniqueNames[SoundListUniqueCounter
                                             * SOUND_UNIQUE_NAME_STRIDE],
                       list->name);
                SoundListUniqueHandle[SoundListUniqueCounter] = list->id;
                SoundListUniqueCounter++;
            }
        }

        list++;
    }
}


/* The forty-four sound tables, **in the order both functions walk them** --
 * `LoadAllSounds` and `UnLoadAllSounds` use exactly the same sequence. Twenty-
 * seven generic effect tables, then seventeen per-character voice tables. */
extern SOUNDENTRY gs_death[];
extern SOUNDENTRY gs_shook[];
extern SOUNDENTRY gs_run[];
extern SOUNDENTRY gs_tripped_voice[];
extern SOUNDENTRY gs_face_hit_voice[];
extern SOUNDENTRY gs_attack[];
extern SOUNDENTRY gs_jump[];
extern SOUNDENTRY gs_grab[];
extern SOUNDENTRY gs_slam[];
extern SOUNDENTRY gs_wasted[];
extern SOUNDENTRY tab_rsnd_enemy_boom[];
extern SOUNDENTRY tab_rsnd_sk_bonus_win[];
extern SOUNDENTRY tab_rsnd_splish[];
extern SOUNDENTRY tab_rsnd_stab[];
extern SOUNDENTRY tab_rsnd_footstep[];
extern SOUNDENTRY tab_rsnd_big_block[];
extern SOUNDENTRY tab_rsnd_small_block[];
extern SOUNDENTRY tab_rsnd_smack[];
extern SOUNDENTRY tab_rsnd_med_smack[];
extern SOUNDENTRY tab_rsnd_klang[];
extern SOUNDENTRY tab_rsnd_big_smack[];
extern SOUNDENTRY tab_rsnd_rocks[];
extern SOUNDENTRY tab_rsnd_body_hit[];
extern SOUNDENTRY tab_rsnd_ground[];
extern SOUNDENTRY tab_rsnd_whoosh[];
extern SOUNDENTRY tab_rsnd_big_whoosh[];
extern SOUNDENTRY st_kano[];
extern SOUNDENTRY st_sonya[];
extern SOUNDENTRY st_jax[];
extern SOUNDENTRY st_subzero[];
extern SOUNDENTRY st_swat[];
extern SOUNDENTRY st_indian[];
extern SOUNDENTRY st_lia[];
extern SOUNDENTRY st_robo1[];
extern SOUNDENTRY st_lao[];
extern SOUNDENTRY st_tusk[];
extern SOUNDENTRY st_sg[];
extern SOUNDENTRY st_st[];
extern SOUNDENTRY st_lk[];
extern SOUNDENTRY st_motaro[];
extern SOUNDENTRY st_sk[];
extern SOUNDENTRY st_nj[];
extern SOUNDENTRY st_fn[];

void UnLoadSoundList(SOUNDENTRY *list);

/* The unique-sound cache is **512 slots**: LoadAllSounds clears index 0 and
 * then steps a byte cursor from 4 to 0x7fc in fours, which is 511 more. */
#define SOUND_UNIQUE_SLOTS  512


/* ------------------------------------------------------------- LoadAllSounds
 *
 * armv7 0x000a82d4, 612 bytes.  **Complete.**
 *
 * Clears the dedupe cache and then loads all forty-four tables in order.
 *
 * The reset is unconditional and covers **every one of the 512 slots**: the
 * name's first byte to 0 and the handle to -1. Only the first byte of each
 * 32-byte name is touched, which is enough to make it an empty string for the
 * `strcmp` in `LoadSoundList`.
 *
 * That is the whole dedupe mechanism: a name already in the cache shares its
 * handle instead of being loaded twice, and with seventeen character voice
 * tables overlapping heavily that matters.
 */
void LoadAllSounds(void)
{
    long i;

    SoundListUniqueCounter = 0;
    SoundListUniqueNames[0] = 0;
    SoundListUniqueHandle[0] = -1;

    for (i = 1; i < SOUND_UNIQUE_SLOTS; i++) {
        SoundListUniqueNames[i * SOUND_UNIQUE_NAME_STRIDE] = 0;
        SoundListUniqueHandle[i] = -1;
    }

    LoadSoundList(triple_sndtab);
    LoadSoundList(gs_death);
    LoadSoundList(gs_shook);
    LoadSoundList(gs_run);
    LoadSoundList(gs_tripped_voice);
    LoadSoundList(gs_face_hit_voice);
    LoadSoundList(gs_attack);
    LoadSoundList(gs_jump);
    LoadSoundList(gs_grab);
    LoadSoundList(gs_slam);
    LoadSoundList(gs_wasted);
    LoadSoundList(tab_rsnd_enemy_boom);
    LoadSoundList(tab_rsnd_sk_bonus_win);
    LoadSoundList(tab_rsnd_splish);
    LoadSoundList(tab_rsnd_stab);
    LoadSoundList(tab_rsnd_footstep);
    LoadSoundList(tab_rsnd_big_block);
    LoadSoundList(tab_rsnd_small_block);
    LoadSoundList(tab_rsnd_smack);
    LoadSoundList(tab_rsnd_med_smack);
    LoadSoundList(tab_rsnd_klang);
    LoadSoundList(tab_rsnd_big_smack);
    LoadSoundList(tab_rsnd_rocks);
    LoadSoundList(tab_rsnd_body_hit);
    LoadSoundList(tab_rsnd_ground);
    LoadSoundList(tab_rsnd_whoosh);
    LoadSoundList(tab_rsnd_big_whoosh);
    LoadSoundList(st_kano);
    LoadSoundList(st_sonya);
    LoadSoundList(st_jax);
    LoadSoundList(st_subzero);
    LoadSoundList(st_swat);
    LoadSoundList(st_indian);
    LoadSoundList(st_lia);
    LoadSoundList(st_robo1);
    LoadSoundList(st_lao);
    LoadSoundList(st_tusk);
    LoadSoundList(st_sg);
    LoadSoundList(st_st);
    LoadSoundList(st_lk);
    LoadSoundList(st_motaro);
    LoadSoundList(st_sk);
    LoadSoundList(st_nj);
    LoadSoundList(st_fn);
}


/* ----------------------------------------------------------- UnLoadAllSounds
 *
 * armv7 0x000a7fac, 612 bytes.  **Complete.**
 *
 * The same forty-four tables in the same order, through `UnLoadSoundList`, then
 * the cache reset.
 *
 * **The two functions clear the cache differently, and only one of them is
 * safe on its own.** This one walks `0 .. SoundListUniqueCounter - 1` and then
 * zeroes the counter; `LoadAllSounds` walks all 512 regardless. So unloading
 * leaves slots past the counter holding whatever they held -- stale names with
 * stale handles -- and it is `LoadAllSounds`' unconditional sweep that makes
 * that harmless.
 *
 * A port that keeps only one of the two resets, or that trusts the counter in
 * both, resurrects a freed handle the first time a later run happens to reach a
 * higher slot. The order matters: reset at the START of loading, not at the end
 * of unloading.
 */
void UnLoadAllSounds(void)
{
    long i;

    UnLoadSoundList(triple_sndtab);
    UnLoadSoundList(gs_death);
    UnLoadSoundList(gs_shook);
    UnLoadSoundList(gs_run);
    UnLoadSoundList(gs_tripped_voice);
    UnLoadSoundList(gs_face_hit_voice);
    UnLoadSoundList(gs_attack);
    UnLoadSoundList(gs_jump);
    UnLoadSoundList(gs_grab);
    UnLoadSoundList(gs_slam);
    UnLoadSoundList(gs_wasted);
    UnLoadSoundList(tab_rsnd_enemy_boom);
    UnLoadSoundList(tab_rsnd_sk_bonus_win);
    UnLoadSoundList(tab_rsnd_splish);
    UnLoadSoundList(tab_rsnd_stab);
    UnLoadSoundList(tab_rsnd_footstep);
    UnLoadSoundList(tab_rsnd_big_block);
    UnLoadSoundList(tab_rsnd_small_block);
    UnLoadSoundList(tab_rsnd_smack);
    UnLoadSoundList(tab_rsnd_med_smack);
    UnLoadSoundList(tab_rsnd_klang);
    UnLoadSoundList(tab_rsnd_big_smack);
    UnLoadSoundList(tab_rsnd_rocks);
    UnLoadSoundList(tab_rsnd_body_hit);
    UnLoadSoundList(tab_rsnd_ground);
    UnLoadSoundList(tab_rsnd_whoosh);
    UnLoadSoundList(tab_rsnd_big_whoosh);
    UnLoadSoundList(st_kano);
    UnLoadSoundList(st_sonya);
    UnLoadSoundList(st_jax);
    UnLoadSoundList(st_subzero);
    UnLoadSoundList(st_swat);
    UnLoadSoundList(st_indian);
    UnLoadSoundList(st_lia);
    UnLoadSoundList(st_robo1);
    UnLoadSoundList(st_lao);
    UnLoadSoundList(st_tusk);
    UnLoadSoundList(st_sg);
    UnLoadSoundList(st_st);
    UnLoadSoundList(st_lk);
    UnLoadSoundList(st_motaro);
    UnLoadSoundList(st_sk);
    UnLoadSoundList(st_nj);
    UnLoadSoundList(st_fn);

    for (i = 0; i < SoundListUniqueCounter; i++) {   /* only the used ones */
        SoundListUniqueNames[i * SOUND_UNIQUE_NAME_STRIDE] = 0;
        SoundListUniqueHandle[i] = -1;
    }
    SoundListUniqueCounter = 0;
}
