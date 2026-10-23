# Goals

What the PC port does today and what Mary has asked for next, in her order.
Kept current with every release; the detail of each change is in
[PROGRESS.md](PROGRESS.md) and [CHANGELOG.md](CHANGELOG.md).

## Working now

**The game**
- The whole game from the player's own `.ipa` (1.2.59 for iPhone): arcade,
  versus, the towers, fatalities, the front end. 32-bit and 64-bit builds.
- Windows; Linux built but not yet tested on a Linux machine (0.1.9:
  `build-game.sh`, README-linux.md, CI compile check).
- Renderers: OpenGL, Vulkan, DirectX 12 (Windows), chosen in the launcher
  or the in-game menu.
- SMOOTH ANIMATIONS on/off (launcher and in-game menu): the fighters'
  frames blended when drawn. Not yet seen by Mary.
- Window, borderless or fullscreen; resolution, 3D resolution,
  antialiasing, texture quality, aspect 3:2 / 4:3 / 16:9 / 21:9, interface
  size, on-screen controls.
- Keyboard and gamepads, rebindable, 6 or 5 button layout; local 2 players
  (DUEL).
- LAN and internet play (64-bit build), with profiles.
- In-game settings menu (Esc), every change saved to `umk3.ini` at once;
  the language changes there at once, texts and menus.
- Debug menu (F2), including Motaro and Shao Kahn as FIGHTER 1/2.
- Mods: files in `mods/` replace the game's by name; `data.ini` changes
  the fight tables (damage, hitboxes).

**The launcher**
- Compiles the game from the `.ipa`, updates itself (Windows; Linux from
  0.1.8).
- GRAPHICS, CONTROLS, PROFILE, PLAY sections.
- MODS: mod manager (priority, on/off, categories, cover, refresh), model
  viewer (fighters with animations, stages, models, textures by fighter;
  with the game alone, the mods on or one mod; full screen; texture
  quality NORMAL / LOW / LITE; playback speed), character
  editor (strikes, visual hitbox editor, specials and finishers, data,
  texts, textures), export to FBX / USD / OBJ, Blender round trip
  (replace an animation or the model from an FBX on the exported rig),
  replace a texture from any picture with its LOW / LITE copies made
  automatically.

## Next (in Mary's order)

1. Whatever Mary asks next. (0.1.9 brought the Vulkan and DirectX 12
   renderers and the Linux fixes.)
2. The Linux build to a working game on a Linux machine (0.1.9 fixes the
   link error a tester hit; next, what that tester sees).

## Later

- Duplicate a fighter into a new slot (the 27th): every per-fighter table
  of the game has to grow.
- Edit a fighter's scripts and code (specials, finishers); attacks for Noob
  Saibot, Shao Kahn and Motaro.
- Outfit 3.
- The viewer's T-pose: some fighters' corrected T-pose looks wrong, the
  ninjas most of all (Mary, 2026-10-22).

## Known problems

- FLAWLESS VICTORY text missing in the 64-bit build.
- Reptile's fatalities: in one he only raises his arms.
- Internet play is 64-bit only.
- Not yet tested by Mary: the bosses as players, a mod's damage and hitbox
  changes in a fight, a model replaced from Blender in the game.
