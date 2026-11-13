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
 * whatever its extension in the mod (.png or .pvr). The smaller copies the
 * game takes by quality are served by a mod's full one when it has no copy
 * of its own: X_LOW and X_LITE by X, X_LITE2 and X_LOW2 by X2. */
int  mods_find_texture(const char *stem, char *out, size_t n);

/* How many mods are on and how many files they replace, for the log. */
int  mods_count(int *files);

/* each mod's data.ini over the fight engine's tables, lowest first; returns
 * how many words it wrote */
int  mods_apply_data(void);

/* 1 for an online fight: the CHARACTER EDITOR's numbers go back to the
 * game's and only models and textures are taken from the mods, so both
 * machines run the same fight. 0 after it puts the mods back. */
void mods_online(int on);

/* Skins that add instead of replacing (mod.ini type=skin, mods.c): another
 * outfit for one fighter, with its own model and sheets, beside the game's.
 *
 * mods_skin_list: the skins for the fighter whose files are named
 * `texbase` ("SONYA"), their ids (1..) in `ids`; returns how many.
 * mods_skin_title: the SKIN box's word for one.
 * mods_skin_prefix: what goes in front of a file's name ("@S1@") so that
 * mods_find and mods_find_texture answer it from that skin, and with the
 * game's own file where the skin has none.
 * mods_real_name: a name without that prefix (the game's file). */
int         mods_skin_list(const char *texbase, int *ids, int max);
const char *mods_skin_title(int id);
void        mods_skin_prefix(int id, char *out, size_t n);
const char *mods_real_name(const char *name);

/* Online: a skin named to the other machine ("sonya|MKX|540648": fighter,
 * name, model size) and the local skin that answers to such a name, 0 none
 * -- the other player is then shown in the game's. */
void        mods_skin_key(int id, char *out, size_t n);
int         mods_skin_by_key(const char *key);

#endif
