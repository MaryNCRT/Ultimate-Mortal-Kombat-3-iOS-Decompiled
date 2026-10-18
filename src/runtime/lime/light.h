/*
 * The engine's lighting, ported from the verified decompilation of LightVert
 * and NormaliseLDirs (armv6 0x00083ab8 / 0x00083c38).
 *
 * See docs/LIGHTING.md. Two directional lights, no ambient, a power falloff on
 * each, clamped to 1, and the result is a single MONOCHROME scalar written as
 * R = G = B -- lighting in this engine is a grey multiplier over the texture
 * and can never tint.
 *
 * Two things here are unusual and are deliberate, not slips:
 *   - the dot product is NEGATED, because the stored light vectors point from
 *     the surface toward the light;
 *   - each light goes through pow(), which is what gives the characters their
 *     hard, nearly rim-lit falloff rather than a soft Lambert ramp.
 */
#ifndef LIME_LIGHT_H
#define LIME_LIGHT_H

/* Normalise the two light directions. Call once. */
void lime_light_init(void);

/* Returns the lit intensity for a unit normal, in 0..1. */
float lime_light_vert(float nx, float ny, float nz);

#endif
