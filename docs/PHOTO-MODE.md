# PHOTO MODE

Port, not in the binary. `src/runtime/photo_mode.c`.

## How it works

- **The freeze** is the game's own pause. `Task_GameMain_frame` skips
  `UpdateArcadeCode` while `GamePaused` is not 0 and still draws the scene;
  the stage's animation (`AnimateBG`) and the effects (`LIME_UpdateEvents`)
  are inside `UpdateArcadeCode`, and the particles read `GamePaused`.
  PHOTO MODE is `GamePaused == 3`. Nothing of the fight is copied or put
  back: it never moves.
- **The camera** is an orbit about a point (turn, tilt, distance, roll).
  `SetToUseCamera` (GameCode.c) still computes the fight's camera and keeps
  `LastEye` / `LastAt`, then hands `LIMEDS_SetCameraOrientation` PHOTO
  MODE's. The REAL-TIME SHADOWS follow, as they read that view.
- **The zoom** scales the projection's x and y (`PortProjScale`,
  LIMEDS_Misc.c), the field of view of a symmetric frustum.
- **No collision**: the game is 2D underneath and its stages carry no
  collision geometry. The camera keeps to limits: a nearest distance, above
  the floor (the fighters' feet), a box about the shot it opened on.
- **Photos**: the next frame is drawn without the panel and the HUD, read
  once presented, and written as a PNG by a thread into `photos/` beside
  the game, named `UMK3_<date>_<time>_<P1>_vs_<P2>.png`.
- **Replays**: PHOTO MODE holds the replay; its FREE CAMERA
  (`photo_free_toggle`) moves the camera while the replay plays on.

## Controls

| | Keyboard and mouse | Pad |
|---|---|---|
| In / out | F3 (key_photo=), P, Esc | L3+R3 (pad_photo=), START, B |
| Orbit | right mouse drag | right stick |
| Move | W A S D, Q / E | left stick |
| Distance | wheel | LT / RT |
| Panel | Tab (category), arrows, Enter; click | LB / RB, d-pad, A |
| Hide the panel | H | Y |
| Photo | Space | X |
| Reset the camera | R | R3 |

## Phases

1. **Done (0.5.9):** freeze, camera, zoom, roll, focus on a fighter, HUD
   hidden, grid, PNG at the game's resolution, pause menu entry, replays.
2. **Next:** the effects as shaders in each renderer (OpenGL GLSL, Vulkan,
   Direct3D 12): exposure, contrast, saturation, temperature, gamma,
   vignette, grain, chromatic aberration, filters, depth of field.
3. A fighter turned or moved for the picture, poses if the animation frames
   allow it (expressions are not possible: the models have no face rig).
4. 1440p / 4K and supersampling, presets, settings saved.
