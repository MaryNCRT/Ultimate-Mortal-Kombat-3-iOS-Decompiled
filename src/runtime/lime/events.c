/* events.c -- see events.h and docs/EVENTS-FORMAT.md. */
#include "events.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Every stride here comes from the loader, not from a file walk:
 *   - the track header is 268 bytes  (0x000a4934 builds the entry base as
 *     (cursor+0x5c)+0xb0 = cursor+0x10c)
 *   - an entry is 56 bytes           (0x000a49be advances by n*64 - n*8)
 *   - numEntries lives at +0x108     (0x000a490e reads [r5,#0x9c], r5 = +0x6c)
 */
#define TRACK_HEADER 268
#define ENTRY_SIZE    56
#define OFF_NAME    0x000
#define OFF_SLOT    0x0c8
#define OFF_NENT    0x108

/* 12.12: 4096 is 1.0. Graveyard's identity rows read 4096 and 4095, which is
 * the exporter rounding, not two different scales. */
#define FIXED  4096.0f

static int32_t rd_i32(const uint8_t *p)
{
    return (int32_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                     ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
}

bool lime_events_load(const char *path, LimeEvents *out)
{
    FILE *f;
    uint8_t *d;
    long size, off;
    int32_t n, i, k;

    memset(out, 0, sizeof(*out));

    f = fopen(path, "rb");
    if (!f) return false;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return false; }
    size = ftell(f);
    rewind(f);
    if (size < 4) { fclose(f); return false; }

    d = (uint8_t *)malloc((size_t)size);
    if (!d) { fclose(f); return false; }
    if (fread(d, 1, (size_t)size, f) != (size_t)size) {
        free(d); fclose(f); return false;
    }
    fclose(f);

    n = rd_i32(d);
    if (n < 0 || n > 100000) { free(d); return false; }

    /* **An empty file is a valid one.** 390 of the 545 shipped `.events` are
     * exactly four bytes holding numTracks == 0 -- a scene that declares no
     * effects. Returning false for those, which this did, reports 390 parse
     * failures that are not failures, and would have masked a real one. */
    if (n == 0) { free(d); return true; }

    out->tracks = (LimeEventTrack *)calloc((size_t)n, sizeof(LimeEventTrack));
    if (!out->tracks) { free(d); return false; }

    off = 4;
    for (i = 0; i < n; i++) {
        LimeEventTrack *tr = &out->tracks[i];
        int32_t nent;
        const uint8_t *blob;

        if (off + TRACK_HEADER > size) { lime_events_free(out); free(d); return false; }

        memcpy(tr->name, d + off + OFF_NAME, 64);
        memcpy(tr->slot, d + off + OFF_SLOT, 64);
        tr->name[63] = tr->slot[63] = '\0';
        nent = rd_i32(d + off + OFF_NENT);
        off += TRACK_HEADER;

        if (nent < 0 || off + (long)nent * ENTRY_SIZE > size) {
            lime_events_free(out); free(d); return false;
        }

        /* The transform is in the FIRST entry, after its two leading int32s:
         * twelve more int32s, nine of 3x3 then three of translation. */
        if (nent > 0) {
            blob = d + off + 8;
            for (k = 0; k < 9; k++) tr->m[k] = (float)rd_i32(blob + k * 4) / FIXED;
            for (k = 0; k < 3; k++) tr->t[k] = (float)rd_i32(blob + 36 + k * 4) / FIXED;
        } else {
            tr->m[0] = tr->m[4] = tr->m[8] = 1.0f;
        }
        off += (long)nent * ENTRY_SIZE;
        out->count = i + 1;
    }

    free(d);
    return true;
}

void lime_event_matrix(const LimeEventTrack *tr, float *dst)
{
    /* row-major 3x3 into GL's column-major 4x4 */
    dst[0]  = tr->m[0]; dst[1]  = tr->m[1]; dst[2]  = tr->m[2]; dst[3]  = 0.0f;
    dst[4]  = tr->m[3]; dst[5]  = tr->m[4]; dst[6]  = tr->m[5]; dst[7]  = 0.0f;
    dst[8]  = tr->m[6]; dst[9]  = tr->m[7]; dst[10] = tr->m[8]; dst[11] = 0.0f;
    dst[12] = tr->t[0]; dst[13] = tr->t[1]; dst[14] = tr->t[2]; dst[15] = 1.0f;
}

void lime_events_free(LimeEvents *e)
{
    if (!e) return;
    free(e->tracks);
    memset(e, 0, sizeof(*e));
}
