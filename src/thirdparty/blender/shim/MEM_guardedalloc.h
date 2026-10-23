/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Stand-in for Blender's intern/guardedalloc, for the CCGSubSurf files in the
 * folder above: the same calls, on the C library's allocator. The name string
 * Blender uses for leak reports is dropped. */
#ifndef UMK3_SHIM_MEM_GUARDEDALLOC_H
#define UMK3_SHIM_MEM_GUARDEDALLOC_H

#include <stdlib.h>

/* The game forces src/runtime/savestate.h into every file, and outside the
 * state files it maps realloc() and free() to its own -- function-like
 * macros, which CCGSubSurf's allocator fields of the same names (four
 * arguments) cannot pass through. This code frees only what it allocated
 * itself, so the C library's pair is the right one here. */
#undef malloc
#undef calloc
#undef realloc
#undef free

#define MEM_mallocN(len, str)       malloc(len)
#define MEM_callocN(len, str)       calloc(1, len)
#define MEM_reallocN(ptr, len)      realloc(ptr, len)
#define MEM_freeN(ptr)              free(ptr)

#endif
