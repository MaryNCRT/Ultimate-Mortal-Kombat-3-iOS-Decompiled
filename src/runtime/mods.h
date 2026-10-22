/* mods.h -- the player's mods, laid over res/ (mods.c). Port, not in the
 * binary. */
#ifndef UMK3_MODS_H
#define UMK3_MODS_H

#include <stddef.h>

/* Reads <game>/mods/mods.ini and indexes every file of the mods it turns
 * on; `res_root` is the game's res/ folder. Called again, it starts over. */
void mods_init(const char *res_root);

/* The mod file that replaces `rel` (a path the engine asks for: "KANO.skin",
 * "res/audio/x.wav"), matched by its file name alone, any case. 1 and the
 * full path in `out`; 0 when no mod has it. */
int  mods_find(const char *rel, char *out, size_t n);

/* The mod picture for a texture named `stem` (no extension: "KANO_DIFFUSE"),
 * whatever its extension in the mod (.png or .pvr). A sheet the game wants
 * at iPhone size (X_LOW) is also served by a mod's X. */
int  mods_find_texture(const char *stem, char *out, size_t n);

/* How many mods are on and how many files they replace, for the log. */
int  mods_count(int *files);

/* each mod's data.ini over the fight engine's tables, lowest first; returns
 * how many words it wrote */
int  mods_apply_data(void);

#endif
