/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Stand-in for the part of Blender's BLI_utildefines.h the CCGSubSurf files
 * use. BLI_assert is off, as in a Blender release build. */
#ifndef UMK3_SHIM_BLI_UTILDEFINES_H
#define UMK3_SHIM_BLI_UTILDEFINES_H

#include <stdbool.h>
#include <string.h>     /* Blender's own header pulls it in through its includes */

#define BLI_assert(a) ((void)0)

/* BLI_compiler_compat.h */
#define BLI_INLINE static inline

#ifdef __GNUC__
#  define UNUSED(x) UNUSED_ ## x __attribute__((__unused__))
#  define LIKELY(x)   __builtin_expect(!!(x), 1)
#  define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#  define UNUSED(x) UNUSED_ ## x
#  define LIKELY(x)   (x)
#  define UNLIKELY(x) (x)
#endif

#define MIN2(a, b) ((a) < (b) ? (a) : (b))
#define MAX2(a, b) ((a) > (b) ? (a) : (b))

#endif
