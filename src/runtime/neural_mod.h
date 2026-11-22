/* neural_mod.h -- the DLSS 5 layer as a mod (neural_mod.c, docs/DLSS5-MOD.md).
 * Port, not in the binary. */
#ifndef UMK3_NEURAL_MOD_H
#define UMK3_NEURAL_MOD_H

/* umk3.ini dlss5_mod= (1, the default: a type=dlss5 mod that is on loads) */
extern int neural_mod_enabled;

/* Before the Direct3D 12 device is made: when a mod with type=dlss5 is on,
 * Windows, Direct3D 12 and an NVIDIA card, and the mod's files are all there
 * (NVIDIA's runtimes signed), ReShade is loaded from the mod's folder.
 * `game_dir` ends in a slash. 1 when it was loaded. Says why in the log. */
int  neural_mod_load(const char *game_dir, int renderer_is_d3d12);
int  neural_mod_loaded(void);
const char *neural_mod_state(void);    /* "loaded", "needs Direct3D 12", ... */

#endif
