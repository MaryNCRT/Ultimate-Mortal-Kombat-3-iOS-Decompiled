/*
 * fight_runtime.h -- the bridge between the decompiled fight engine and a host.
 *
 * The engine's whole interface, per mk3.c:
 *
 *      mk3_init(p1, p2, bbox_cb, 1)   character numbers, bit 7 = CPU-driven
 *      mk3_update(joy[2], &out)       two input words in, a display list out
 *
 * `fight_runtime_init` must run once before `mk3_init`.
 */
#ifndef FIGHT_RUNTIME_H
#define FIGHT_RUNTIME_H

void fight_runtime_init(void);

long mk3_init(long p1, long p2, void (*bbox_cb)(void), long unused);
long mk3_update(const long *joy, void **out);

#endif
