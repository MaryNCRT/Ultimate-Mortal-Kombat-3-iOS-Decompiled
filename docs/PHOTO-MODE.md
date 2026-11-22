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

## The effects

One pass over the game's area after the scene, before the panel
(`photo_post`, GameCode.c Task_GameMain_frame), only while a setting is not
at rest. 32 floats of settings (`photo_mode.c photo_post`, the order in the
shaders' comment).

- **Vulkan, Direct3D 12** (`rgl_ff.c rgl_ff_post`): the scene drawn out, the
  viewport blitted into a target of its own (the screen map), one
  full-screen draw of fragment mode `RGL_POST` (6) -- `post_fx` in
  `shaders/ff.frag` and `ff.hlsl` -- the settings in the light block's first
  two lights. The depth of field reads the distance AMBIENT OCCLUSION's depth
  pass packs, copied each frame while it is asked for
  (`rgl_post_want_depth`), bound as the shadow map (the backends bind it for
  `RGL_POST` as for a receiver). That pass needs ENHANCED LIGHTING.
- **OpenGL** (`render/rgl_post.c`): a GLSL 1.20 program of its own, the
  same effects line for line; the viewport blitted into a framebuffer with
  colour and depth textures (the window's depth copied when the formats do
  not agree), the depth turned into a distance with the scene's projection
  (`rgl_post_scene`, RenderGameView).
- **Depth of field**: the focus is the distance at the middle of the view
  (where the camera looks, its orbit's point) times FOCUS DISTANCE; the blur
  grows with the distance from it, times the aperture, up to 10 pixels, 24
  taps on a spiral.
- The shaders are compiled by `render/shaders/build.py` (glslang 16.6,
  fxc 5.1) into `rgl_shaders_vk.h` / `rgl_shaders_d3d12.h`.

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
2. **Done (after 0.5.9):** IMAGE and EFFECTS -- exposure, brightness,
   contrast, saturation, temperature, tint, gamma, sharpness, highlights,
   shadows; depth of field, bloom, vignette, chromatic aberration, grain,
   nine filters. See "The effects" below.
3. A fighter turned or moved for the picture, poses if the animation frames
   allow it (expressions are not possible: the models have no face rig).
4. 1440p / 4K and supersampling, presets, settings saved.
