/*
 * Legacy PVR v2 container + PVRTC1 decoder.
 *
 * PVRTC is PowerVR hardware compression. Desktop GPUs cannot sample it, so the
 * decode happens on the CPU here exactly as it would have to in any PC port.
 *
 * Ported from tools/pvrtc.py, which measures 1.5% mean error against the PNGs
 * EA shipped alongside the compressed textures. See docs/PVR-FORMAT.md.
 */
#ifndef LIME_PVR_H
#define LIME_PVR_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int      width, height;
    uint8_t *rgba;          /* width * height * 4, caller frees */
} LimeImage;

/* Decodes a .pvr to RGBA. Handles PVRTC 4bpp and 2bpp. */
bool lime_pvr_load(const char *path, LimeImage *out);

/* Loads a .pvr by the stem of a mesh texture name. The exporter wrote a
 * literal ".???" extension into every mesh, so the stem is what identifies a
 * texture and the real extension is supplied here. */
bool lime_texture_load(const char *res_dir, const char *mesh_texture,
                       LimeImage *out);

void lime_image_free(LimeImage *img);

#endif
