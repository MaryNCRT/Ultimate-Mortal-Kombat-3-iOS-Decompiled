/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * skin_mats.h -- a modded skin's own textures, one per part. Port, not in
 * the binary. See skin_mats.c. */
#ifndef UMK3_SKIN_MATS_H
#define UMK3_SKIN_MATS_H

/* LIME_LoadSkin: the skin file just loaded (its .mats beside it, if a mod
 * has one) */
void port_skin_mats_loaded(const void *skin, const char *filename);
/* a skin (either block) a mod brought: SMOOTH MODELS leaves it alone */
int  port_skin_custom(const void *skin);
/* LIME_FreeSkin */
void port_skin_mats_forget(const void *skin);
/* does this skin block draw in parts */
int  port_skin_mats_has(const void *skin);
/* RenderAnimatedCharacter: the block the next LIME_RenderMeshSingleIndexed
 * draws, ice when frozen; NULL for babality and the shadow (one texture
 * for all) */
void port_skin_mats_use(const void *skin, int ice);
/* LIME_RenderMeshSingleIndexed: draws the triangles part by part, each with
 * its own texture, and returns 1; 0 when the block has no parts */
int  port_skin_mats_draw(const unsigned short *indices, long nfaces,
                         unsigned tex_name);

/* The proportion trick (<skin>.bones): LoadAnimatedCharacter, after the
 * bones load, puts the model's offsets in them; the palette scales the
 * animations' root height by port_skin_root_y (1 without); LIME_FreeBones
 * forgets them. */
void  port_skin_bones_loaded(void *bones, const char *skinfile);
float port_skin_root_y(const void *bones);
void  port_skin_bones_forget(const void *bones);
/* an online fight: the game's lengths back on every fighter loaded */
void  port_skin_bones_online(void);

#endif
