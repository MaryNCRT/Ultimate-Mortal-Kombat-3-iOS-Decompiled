/*
 * gfx_preset.h -- GRAPHICS QUALITY: the presets and the play-time image
 * pass (bloom, tonemap). Port, not in the binary. docs/research/GRAPHICS-OVERHAUL.md.
 *
 * A preset is not a second set of switches: it SETS the ones that exist
 * (render/rgl.h rgl_shadows, rgl_ao, rgl_contact_shadows; gfx_bloom,
 * gfx_tonemap below), and the preset shown is the one those switches match
 * -- CUSTOM when none does.
 */
#ifndef UMK3_GFX_PRESET_H
#define UMK3_GFX_PRESET_H

enum { GFX_ORIGINAL, GFX_ENHANCED, GFX_RTX, GFX_RTX_DLSS5, GFX_CUSTOM, GFX_PRESETS };

/* BLOOM, 0..100 (percent), and TONEMAP, 0 / 1 (umk3.ini bloom=, tonemap=) */
extern int gfx_bloom, gfx_tonemap;

int  gfx_preset(void);                  /* GFX_* the switches match */
void gfx_preset_set(int p);             /* GFX_ORIGINAL / GFX_ENHANCED: their switches */
int  gfx_preset_selectable(int p);      /* 1 for the ones implemented */
int  gfx_preset_parse(const char *v);   /* umk3.ini graphics_preset=, -1 unknown */
const char *gfx_preset_key(int p);      /* "original", "enhanced", ... */

/* GameCode.c Task_GameMain_frame, once the 3D scene is drawn and before
 * the HUD: the pass over the scene alone, when BLOOM or TONEMAP is on.
 * Not while PHOTO MODE is up (its own pass, photo_post, runs then). */
void gfx_scene_post(void);

#endif
