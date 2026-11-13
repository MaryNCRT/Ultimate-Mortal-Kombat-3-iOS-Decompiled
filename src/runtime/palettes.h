/* palettes.h -- colours 3 to 6, made from a fighter's own sheets. Port, not
 * in the binary (Mary 2026-11-13).
 *
 * The game has two colours a fighter: its sheets and their "2" twins
 * (KUNGLAO_DIFFUSE / _DIFFUSE2). EA made the second by turning the hue of
 * the clothes: Kung Lao's trousers are 49 degrees in the first, 196 in the
 * second, at the same saturation and value. Colours 3..6 do the same at
 * load time: aditional textures/palettes.ini says, per fighter, which sheet
 * they start from, which hues change, and each colour's hue, saturation and
 * value. A recoloured sheet is asked for under the "2" sheet's name with
 * "@P<n>@" in front ("@P3@KUNGLAO_DIFFUSE2.PNG"); the texture loader
 * (draw_gl.c) recolours it, and the cache keeps it apart from the others. */
#ifndef UMK3_PALETTES_H
#define UMK3_PALETTES_H

#include <stddef.h>
#include <stdint.h>

#define PAL_MAX 6

/* How many colours fighter `texBase` ("KUNGLAO") has: 2, or up to 6 with
 * palettes.ini. Motaro and Shao Kahn's one is the caller's business. */
int  pal_count(const char *texBase);

/* "@P3@X" -> 3 and *rest "X"; 0 for a name without the mark */
int  pal_strip(const char *name, const char **rest);

/* The mark for colour n (3..6) in out */
void pal_prefix(int n, char *out, size_t size);

/* Recolour a sheet in place, as colour n of the fighter its file `name`
 * belongs to (the longest palettes.ini section its base name starts with).
 * Returns 0 when nothing says how (the sheet is left as it is). */
int  pal_apply(const char *name, int n, uint8_t *rgba, int w, int h);

/* The fighter's sheet to start colour n from: 1 or 2 (palettes.ini base=,
 * 2 when it does not say). */
int  pal_base(const char *texBase);
/* 1 when palettes.ini has a section of exactly this name ("SONYA_ART") */
int  pal_known(const char *name);

/* The fighters palettes.ini names, for writing their colours out */
int  pal_fighters(void);
const char *pal_fighter(int i);

/* The file name of colour n of sheet `stem` (no extension):
 * KUNGLAO_DIFFUSE2 -> KUNGLAO_DIFFUSE3. 0 when it has none. */
int  pal_file_name(const char *stem, int n, char *out, size_t size);

/* Write a picture as PNG; 0 on failure */
int  pal_png_write(const char *path, const uint8_t *rgba, int w, int h);

#endif
