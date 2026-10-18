/*
 * wav.c -- the game's .wav files, shared by the fight's sounds and the
 * engine's limeLoadSound.
 *
 * RIFF/WAVE, and only the shape the game actually uses: PCM, one channel,
 * 8 bits. Anything else is refused by name rather than mis-decoded -- a
 * 16-bit file read as 8-bit is not a quiet bug, it is a scream.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wav.h"

int wav_load_u8(const char *path, unsigned char **pcm, int *frames, int *rate)
{
    FILE *f = fopen(path, "rb");
    long  size;
    unsigned char *d;
    int   fmt_ch, fmt_bits, fmt_rate;
    long  pos, data_off = 0, data_len = 0;

    if (!f)
        return 0;

    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 44) { fclose(f); return 0; }

    d = (unsigned char *)malloc((size_t)size);
    if (!d) { fclose(f); return 0; }
    if (fread(d, 1, (size_t)size, f) != (size_t)size) {
        fclose(f); free(d); return 0;
    }
    fclose(f);

    if (memcmp(d, "RIFF", 4) || memcmp(d + 8, "WAVE", 4)) {
        fprintf(stderr, "audio: %s is not RIFF/WAVE\n", path);
        free(d); return 0;
    }

    fmt_ch = fmt_bits = fmt_rate = 0;
    pos = 12;
    while (pos + 8 <= size) {
        long clen = (long)d[pos+4] | ((long)d[pos+5] << 8)
                  | ((long)d[pos+6] << 16) | ((long)d[pos+7] << 24);
        if (!memcmp(d + pos, "fmt ", 4) && clen >= 16) {
            int tag  = d[pos+8]  | (d[pos+9]  << 8);
            fmt_ch   = d[pos+10] | (d[pos+11] << 8);
            fmt_rate = (int)(d[pos+12] | (d[pos+13] << 8)
                             | (d[pos+14] << 16) | ((long)d[pos+15] << 24));
            fmt_bits = d[pos+22] | (d[pos+23] << 8);
            if (tag != 1) {
                fprintf(stderr, "audio: %s is compressed (tag %d)\n", path, tag);
                free(d); return 0;
            }
        } else if (!memcmp(d + pos, "data", 4)) {
            data_off = pos + 8;
            data_len = clen;
            if (data_off + data_len > size)
                data_len = size - data_off;
        }
        pos += 8 + clen + (clen & 1);
    }

    if (!data_len || fmt_ch != 1 || fmt_bits != 8) {
        fprintf(stderr, "audio: %s is %d ch %d bit -- expected mono 8\n",
                path, fmt_ch, fmt_bits);
        free(d); return 0;
    }

    *pcm = (unsigned char *)malloc((size_t)data_len);
    if (!*pcm) { free(d); return 0; }
    memcpy(*pcm, d + data_off, (size_t)data_len);
    *frames = (int)data_len;
    *rate   = fmt_rate;
    free(d);
    return 1;
}
