/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * smooth_models.h -- SMOOTH MODELS, see smooth_models.c. */
#ifndef UMK3_SMOOTH_MODELS_H
#define UMK3_SMOOTH_MODELS_H

/* smooth_models=: 0 off, 1 or 2 the subdivision level */
extern int port_smooth_models;
extern float port_sharp_angle, port_sharp_strength;   /* degrees, 0..1 */

/* Draws a fighter's skin smoothed in place of LIME_RenderMeshSingleIndexed
 * (same arguments, plus the skin). `second` is the record half, 0 the body
 * and 1 the second skin, or 2 for the shadow, which reuses the last smoothed
 * body. Returns 0 when it drew nothing and the caller should draw the
 * original. */
int  port_smooth_draw(const void *skin, const void *rec, long second,
                      void *tex, float alpha, const void *fade);

/* LIME_FreeSkin: drop what was built for this skin. */
void port_smooth_forget(const void *skin);

/* What covers a skin: RenderPlayer names the skin (its first) before it
 * draws the fighter's scene and NULL after; LIME_RenderScene hands each
 * mesh it draws on a bone of that skin -- Kung Lao's hat -- with the
 * matrix it is drawn with and the scene's scale. The skin's vertices inside
 * such a piece get no correction pass (umk_subsurf_set_fit), so the
 * smoothed skin does not come through it. */
void port_smooth_cover_owner(const void *skin);
void port_smooth_cover_mesh(const void *mesh, const float *key, int bone, const void *bones,
                            float scene_scale);

#endif
