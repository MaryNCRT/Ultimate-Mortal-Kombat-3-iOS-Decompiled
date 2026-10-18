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
#ifndef _WIN32
#include <strings.h>
#ifndef _stricmp
#define _stricmp strcasecmp
#endif
#ifndef _strnicmp
#define _strnicmp strncasecmp
#endif
#endif

void *st_malloc(size_t n);
void *st_calloc(size_t n, size_t size);
void *st_realloc(void *p, size_t n);
void  st_free(void *p);

#if defined(UMK3_LOW32) && defined(UMK3_STATE_FILE)
/* The 64-bit build (UMK3_LOW32): the game's words stay 32 bits, as on the
 * device. Its code spells a word holding an address uintptr_t; here that is
 * a uint32_t, which holds every address of the game, since the build keeps
 * the exe, its data and the game's heaps below 2 GB. The library's headers
 * come first, with the real type. */
#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#include <stdarg.h>
#define uintptr_t uint32_t
#define intptr_t  int32_t
#endif

/* P32 marks a pointer the game keeps in a 32-bit word: a field of one of
 * its structures at the binary's layout, or a cast reading such a word
 * (`*(void *P32 *)(base + 0x10)`, `(char *P32 *)c`). In the 64-bit build it
 * is clang's 32-bit pointer (__ptr32, zero-extended), so the structures keep
 * the device's offsets and the code that indexes them by word still lands
 * on the right field; elsewhere it is nothing. Every file of that build
 * sees it, so a structure has one shape everywhere. mingw's headers define
 * __ptr32 away, hence the #undef. */
#if defined(UMK3_LOW32)
#undef __ptr32
#undef __uptr
#define P32 __ptr32 __uptr
#else
#define P32
#endif

#ifdef UMK3_STATE_FILE
#ifdef _WIN32
__asm__(".section .umkstb,\"bw\"");
#pragma data_seg(".umkstd")
#pragma bss_seg(".umkstb")
#else
#pragma clang section data=".umkstd" bss=".umkstb"
#endif
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

#if defined(UMK3_LOW32) && defined(UMK3_STATE_FILE) && !defined(_WIN32)
/*
 * Linux 64-bit (LP64) adaptation for 1:1 ARMv7 decompiled gamecode:
 * In ARMv7 iOS and Win32/Win64 (LLP64), sizeof(long) == 4.
 * On LP64 Linux, sizeof(long) == 8, which breaks 32-bit word offsets,
 * mesh loading, and binary file parsing.
 * Include POSIX and platform GL headers before remapping long to int.
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include "platform/gl.h"
#ifndef UMK3_NO_LONG_REMAP
#define long int
#endif
#endif

#endif

