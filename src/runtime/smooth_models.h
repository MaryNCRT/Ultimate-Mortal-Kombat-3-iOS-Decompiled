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

#endif
