/*
 * fight_headless.c -- the decompiled fight engine, run with no window.
 *
 *      umk3-fight-headless [p1] [p2] [frames] [--cpu1] [--cpu2] [--joy1 hex]
 *
 * Calls `mk3_init` and then `mk3_update` once per frame, the way
 * `UpdateArcadeCode` does, and prints the display list the engine hands back.
 * Nothing in the frame is supplied by this file except the input words and the
 * bounding-box callback.
 *
 * ## The bounding-box callback is NOT the game's
 *
 * The real one is `FrameID_GetBBox` in GameCode.c, which measures the loaded
 * animation frames -- it needs the game's assets on the GPU side. Here every
 * animation reports the same 40x100 box, and animation -1 (the camera) reports
 * a 400-wide window. That is enough for the engine to run; it is not enough for
 * hits to land where they would. The shell build (`umk3-test`) passes the real
 * one.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "fight_runtime.h"

extern long *RoundParam;
int PortIPadTall;
int PortPractice;

static void headless_bbox(long ani, int *left, int *top, int *right, int *bottom)
{
    if (ani == -1) {            /* the camera: see mk3_update */
        *left = 0; *top = 0; *right = 400; *bottom = 254;
        return;
    }
    *left = -20; *top = -100; *right = 20; *bottom = 0;
}

typedef struct MO {             /* mk3_update's sixteen-byte display record */
    uint32_t link;
    int16_t  x, y;
    uint16_t remap;
    uint16_t flags;
    int8_t   facing;
    uint8_t  who;
    uint16_t ani;
} MO;

int main(int argc, char **argv)
{
    long p1 = 0, p2 = 1, frames = 600, every = 60;
    long joy[2] = { 0, 0 };
    int  i, pos = 0;
    long f;

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--cpu1"))       p1 |= 0x80;
        else if (!strcmp(argv[i], "--cpu2"))  p2 |= 0x80;
        else if (!strcmp(argv[i], "--joy1") && i + 1 < argc)
            joy[0] = strtol(argv[++i], NULL, 16);
        else if (!strcmp(argv[i], "--every") && i + 1 < argc)
            every = strtol(argv[++i], NULL, 0);
        else if (pos == 0) { p1 = (p1 & 0x80) | strtol(argv[i], NULL, 0); pos++; }
        else if (pos == 1) { p2 = (p2 & 0x80) | strtol(argv[i], NULL, 0); pos++; }
        else if (pos == 2) { frames = strtol(argv[i], NULL, 0); pos++; }
    }

    fight_runtime_init();
    /* GameCodeInit's two writes, and Task_GameInit repeats them just before
     * its mk3_init: playback enabled (0x34), the finish flag clear (0x38). */
    RoundParam[0x34 / 4] = 1;
    RoundParam[0x38 / 4] = 0;
    printf("mk3_init(0x%02lx, 0x%02lx) = %ld\n", p1, p2,
           mk3_init(p1, p2, (void (*)(void))headless_bbox, 1));

    for (f = 0; f < frames; f++) {
        void *out = NULL;
        long  r   = mk3_update(joy, &out);
        if (r != 0) {
            printf("frame %ld: mk3_update returned %ld (frame aborted)\n", f, r);
            continue;
        }
        if (every && f % every == 0) {
            const MO *m = (const MO *)out;
            printf("frame %ld:", f);
            for (; m; m = (const MO *)(uintptr_t)m->link)
                printf("  [who %u ani 0x%x x %d y %d fl 0x%x]",
                       m->who, m->ani, m->x, m->y, m->flags);
            printf("\n");
        }
    }
    return 0;
}
