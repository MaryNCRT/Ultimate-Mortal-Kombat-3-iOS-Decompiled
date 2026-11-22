/*
 * photo_mode.h -- PHOTO MODE. Port, not in the binary.
 *
 * The fight frozen, a camera of its own over it, the HUD hidden and the
 * picture saved as a PNG (docs/PHOTO-MODE.md). The freeze is the game's own
 * pause: Task_GameMain_frame skips UpdateArcadeCode while GamePaused is not
 * 0 and still draws the scene, so PHOTO MODE is one more value of it, 3,
 * and nothing of the fight is copied or put back -- it never moves.
 */
#ifndef UMK3_PHOTO_MODE_H
#define UMK3_PHOTO_MODE_H

#include "platform/platform.h"

#define PHOTO_PAUSED 3                  /* GamePaused while the mode is up */

/* 1 while the mode is up */
int   photo_on(void);
/* 1 where it may open: a fight on this machine (not online, watched or a
 * replay), past its intro, no fade running, the pause menu or nothing up */
int   photo_allowed(void);
/* in (from the pause menu: 1, back to it on the way out) and out */
void  photo_open(int from_menu);
void  photo_close(void);

/* game_main.c, every tick of the game: the shortcut (key_photo=, F3, and
 * the pad's pad_photo= combination, L3+R3 by default) and, while up, the
 * camera, the panel and the capture. `pad_mask` is the combination, `key`
 * game_main.c's plat_key (with UMK3_KEYS' scripted keys). */
void  photo_tick(const PLAT_PAD *pad, unsigned pad_mask, int (*key)(int));

/* GameCode.c SetToUseCamera: the camera to use instead of the fight's,
 * 1 while the mode is up */
int   photo_camera(float eye[3], float at[3], float up[3]);
/* GameCode.c RenderGameView / LIMEDS_Set3dMode: the zoom, as a scale of
 * the projection's x and y (the field of view; 1 off) */
float photo_proj_scale(void);
/* GameCode.c Task_GameMain_frame: the HUD and the port's windows left out */
int   photo_hide_hud(void);
/* GameCode.c Task_GameMain_frame, last: the panel, the hints, the grid */
void  photo_draw(void);

/* game_main.c, once the frame is on the window and before it is shown: a
 * capture asked for reads it there (the game's area: vx, vy, vw, vh) and a
 * thread writes the PNG */
void  photo_after_present(int vx, int vy, int vw, int vh);

#endif
