/* neural_mod.h -- DLSS 5 (community neural rendering) inside the game
 * (neural_mod.c, docs/DLSS5-MOD.md). Port, not in the binary. */
#ifndef UMK3_NEURAL_MOD_H
#define UMK3_NEURAL_MOD_H

/* umk3.ini dlss5= (GRAPHICS > DLSS 5; 0 off, the default): the chain in
 * <game>\graphics\dlss5 is loaded at the next start */
extern int neural_dlss5;
/* umk3.ini dlss5_mod= (1, the default): a mod with type=dlss5 that is on
 * loads when dlss5= is off */
extern int neural_mod_enabled;
/* umk3.ini dlss5_stage= / dlss5_fighters= (0..1, 1 the default): how much
 * of the neural pass the stage and the fighters get (the HUD none) */
extern float neural_stage, neural_fighters;
/* umk3.ini dlss5_layers= (0, the default): the layers above at all --
 * experimental, see neural_mod.c */
extern int neural_layers;
/* umk3.ini dlss5_neural= (1, the default): the neural rendering itself; 0
 * keeps DLSS 5's DLSS/DLAA alone (GRAPHICS > DLSS 5 > NEURAL RENDERING) */
extern int neural_nr;

/* GRAPHICS > DLSS 5 > PRESET (umk3.ini dlss5_preset=): the neural model and
 * its strengths, written to AIO's settings at the next start */
#define NEURAL_PRESETS 4
typedef struct {
    const char *key, *es, *en;          /* umk3.ini value; the menu's words */
    int   model;                        /* AIO's Model 1..3 (DLSSNR.Style 0..2) */
    float intensity, tone, structure;   /* AIO's Intensity, LocalTone, LocalStructure */
} neural_preset;
extern const neural_preset neural_presets[NEURAL_PRESETS];
extern int neural_preset_index;
int neural_preset_parse(const char *v);

enum {
    NEURAL_ACTIVE,                      /* loaded in this run */
    NEURAL_READY,                       /* everything there: loads at the next start */
    NEURAL_NEED_D3D12,                  /* RENDERER is not Direct3D 12 */
    NEURAL_NEED_NVIDIA_GPU,             /* no NVIDIA card */
    NEURAL_CONFLICT,                    /* a ReShade of the player's own beside the exe */
    NEURAL_NOT_INSTALLED,               /* graphics\dlss5 lacks what the game ships */
    NEURAL_NEED_FILES,                  /* nvngx_dlss.dll missing or refused */
    NEURAL_READY_DLAA                   /* DLSS (DLAA) alone: no nvngx_dlssnr.dll or consumer */
};

/* Before the Direct3D 12 device is made (win32_gl.c): when dlss5=1 (or a
 * type=dlss5 mod is on), Windows, Direct3D 12 and an NVIDIA card, and every
 * file is there (NVIDIA's runtimes signed), ReShade is loaded from the
 * folder. `game_dir` ends in a slash. 1 when it was loaded; the log says
 * why not. */
int  neural_mod_load(const char *game_dir, int renderer_is_d3d12);
int  neural_mod_loaded(void);
const char *neural_mod_state(void);    /* "loaded", "needs Direct3D 12", ... */

/* NEURAL_* for graphics\dlss5, checked quietly (the menu, --dlss5-status) */
int  neural_dlss5_status(const char *game_dir, int renderer_is_d3d12);
const char *neural_status_name(int st);

#endif
