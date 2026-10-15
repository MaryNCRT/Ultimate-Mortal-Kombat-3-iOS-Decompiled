/*
 * savestate.h -- the debug menu's save states (a port feature, not in the
 * binary). Force-included into every file of umk3-game (-include).
 *
 * A save state is the game's whole memory at one tick: every global and
 * static of the game's own files, and every block those files allocated.
 * Loading copies both back -- the fight goes on from that exact tick, in the
 * same session, without reloading anything.
 *
 * So the game's memory has to be in two known places:
 *
 *   - Globals and statics of the game's files (decomp/, runtime/lime/, the
 *     runtime files that hold game state) are compiled with UMK3_STATE_FILE,
 *     which puts them in the sections .umkstd (initialised) and .umkstb
 *     (zeroed) of the exe.
 *   - Their malloc / calloc / realloc / free go to an arena of its own
 *     (savestate.c), whose bookkeeping lives inside it, so a copy of the
 *     arena is a consistent heap.
 *
 * Every other file (the platform, the debug menu, the main loop) keeps the
 * C library's heap, and its free / realloc check which heap a block came
 * from, because a buffer can cross over (a PNG decoded by runtime/lime and
 * freed by draw_gl.c).
 *
 * Textures are never deleted (draw_gl.c), and their cache lives outside the
 * arena, so a state keeps finding its textures. Sounds are part of the state.
 */
#ifndef UMK3_SAVESTATE_H
#define UMK3_SAVESTATE_H

/* the library's declarations first, so the macros below never rename them */
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

void *st_malloc(size_t n);
void *st_calloc(size_t n, size_t size);
void *st_realloc(void *p, size_t n);
void  st_free(void *p);

#ifdef UMK3_STATE_FILE
#pragma clang section data=".umkstd" bss=".umkstb"
#define malloc(n)      st_malloc(n)
#define calloc(n, s)   st_calloc(n, s)
#define realloc(p, n)  st_realloc(p, n)
#define free(p)        st_free(p)
#else
#define realloc(p, n)  st_realloc_any(p, n)
#define free(p)        st_free_any(p)
#endif

/* For the files outside the state: a block from either heap. */
void *st_realloc_any(void *p, size_t n);
void  st_free_any(void *p);

#define ST_SLOTS 4

/* 1 when save states can work (the arena is up and never overflowed). */
int  st_available(void);
int  st_has(int slot);
/* 0 on success; the reason in *why otherwise */
int  st_save(int slot, const char **why);
int  st_load(int slot, const char **why);
/* What a state holds, in bytes (for the menu and the log). */
unsigned long st_size(int slot);

#endif
