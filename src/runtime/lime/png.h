/*
 * png.h -- a self-contained PNG decoder, for the 442 PNG textures the game
 * ships alongside its PVRTC ones.
 *
 * The loader only ever read .pvr, so every stage whose atlas is a PNG drew
 * untextured. The shipped files are all 8-bit and none is interlaced -- 364 are
 * RGBA (colour type 6), 64 are RGB (type 2) and 14 are palettised (type 3 with
 * PLTE and tRNS) -- so that is exactly what this decodes, and it rejects
 * anything else rather than guessing.
 *
 * It carries its own DEFLATE. The project ships no third-party code and there
 * is no zlib on the build host, so inflate is written out here: the stored,
 * fixed-Huffman and dynamic-Huffman block types, and nothing more.
 */
#ifndef LIME_PNG_H
#define LIME_PNG_H

#include <stdbool.h>
#include <stdint.h>

/* Decodes to 8-bit RGBA. out is malloc'd, width*height*4 bytes. */
bool lime_png_load(const char *path, uint8_t **out, int *width, int *height);

#endif
