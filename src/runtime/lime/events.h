/*
 * events.h -- the `.events` effect tracks, for the native runtime.
 *
 * Format from docs/EVENTS-FORMAT.md, derived from LIME_LoadEvents
 * (0x000a477c). This file reads the one part the port needs today: each track
 * names an effect and carries the TRANSFORM at which to place an instance of
 * it.
 *
 * Graveyard's file holds seven tracks, all named `gymist1`, and their
 * translations are byte-for-byte the positions of the seven EVENT_gymist*
 * nodes in the stage's `.scene`. Two independent files agreeing to the last
 * digit is what makes this a reading rather than a guess -- and the `.events`
 * file carries more: a 3x3 whose Y scale differs per instance (0.93, 0.69,
 * 0.64, 0.55, 0.36) and, on two of them, X and Y BOTH negative. Both at once
 * is not a mirror -- the determinant stays positive, 0.693 -- it is a 180
 * degree rotation about Z, so those two bands are the same band turned round.
 * That is what turns
 * one uniform sheet of mist into several bands of different thickness drifting
 * in opposite directions.
 */
#ifndef LIME_EVENTS_H
#define LIME_EVENTS_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    char  name[64];             /* the effect: "gymist1", "MUZZLEFLASH", ... */
    char  slot[64];
    float m[9];                 /* row-major 3x3, from 12.12 fixed point */
    float t[3];                 /* translation, same fixed point */
} LimeEventTrack;

typedef struct {
    LimeEventTrack *tracks;
    int32_t         count;
} LimeEvents;

bool lime_events_load(const char *path, LimeEvents *out);
void lime_events_free(LimeEvents *e);

/* Build a column-major GL matrix from a track's 3x3 and translation. */
void lime_event_matrix(const LimeEventTrack *tr, float *dst /* [16] */);

#endif
