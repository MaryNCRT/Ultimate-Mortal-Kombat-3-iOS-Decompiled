/*
 * wav.h -- load one of the game's RIFF/WAVE files: PCM, mono, 8-bit unsigned.
 */
#ifndef UMK3_WAV_H
#define UMK3_WAV_H

/* Returns 1 and a malloc'd copy of the samples (the caller frees it), or 0 for
 * a missing file or any shape the game does not use, said on stderr. */
int wav_load_u8(const char *path, unsigned char **pcm, int *frames, int *rate);

#endif
