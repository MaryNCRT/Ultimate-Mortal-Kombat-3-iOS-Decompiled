# Changelog

Release history for the native PC port of *Ultimate Mortal Kombat 3* (iOS 1.2.59).

---

## After 0.1.11: fatality lag, several cores, no vertex-buffer ceiling, shadows, flawless (2026-10-29, not released yet)

**Scorpion's fatality lag (Mary: lag in that fatality only).** Measured
with a per-second log of the draws (Scorpion vs Sonya, practice, finisher 2):
in the late phase ~150 draws of 180,216 vertices a frame (the copies of
Scorpion, each a smoothed body expanded to one vertex per triangle corner),
~1 GB sent to the GPU, frames of 889-968 ms (651-679 ms in the scene's
draws). Two causes:
- SMOOTH MODELS' budget of smoothed triangles read `limeRenderedPolyCount`,
  which the game's tick clears; a slow frame runs several ticks between its
  draws, so the budget never filled (logged: 0 draws over it). Now its own
  count, cleared before each present (`port_smooth_frame`, game_main.c),
  250,000 triangles: the two fighters and a third body; copies past it are
  drawn as the model is.
- Direct3D 12 and Vulkan grew their vertex buffer up to 256 MB; past that,
  every overflow waited for the GPU and made a new 256 MB buffer, 3-4 times a
  frame. Now the buffer grows to what the frame needs with no ceiling, and the
  old one is kept until the next flush instead of waiting partway.
After: one 52 ms frame in the whole fatality, the rest under the slow-frame
threshold. rgl_test 0 failed on OpenGL, Vulkan and Direct3D 12; Linux
compile 84/84. Not yet played by Mary.

**Several cores.** zig cc ships no OpenMP runtime, so Blender's subdivision
ran on one core: `src/thirdparty/blender/omp_lite.c` implements the
`__kmpc_*` calls (a pool of at most 4 threads, measured fastest; `UMK3_THREADS`
overrides), `CCG_OMP_LIMIT` lowered to 20000, rgl_ff's indexed expansion in
parallel, a ring of 8 recent poses per smoothed mesh.

**Engine memory (Mary asked for more).** The game keeps pointers in 32-bit
words and longs that sign-extend, as on iOS, so its two heaps must sit below
2 GB: the save states' arena reserves 1024 MB and limeMalloc's heap 768 MB
(read at start). A 1024 or 896 MB heap was tried and does not fit beside the
exe; that space is full. Going past 2 GB would mean every pointer read in the
decompiled code zero-extended instead, a change across the whole game.
Measured use is ~386 MB for the whole process, so the heap is not what
limits the frame rate. Everything else (textures, vertex buffers, smoothed
meshes, shadows) is ordinary 64-bit memory with no limit of the engine's.

**Also since 0.1.11:** REAL-TIME SHADOWS drawn the same frame with a steady
light box; Kung Lao's hat in the fight, not only the intro; round end as the
binary's two blocks (FLAWLESS VICTORY on match point, DrawHUD 0x2902c /
0x290a6); Linux keys mapped by scancode (not tested on Linux);
`UMK3_PRACTICE=1` runs a `--fight` as TRAINING.

---

## 0.1.11-beta (2026-10-23) — updates that finish, window resize, loads, profile picker, sharp edges

Built for Windows (64 and 32 bits) and Linux. Not yet played by Mary.

- Launcher updates: every Windows update since 0.1.6 ended as "could not be
  copied" (robocopy on folders no release has) and deleted `tools\eos\`;
  now only `src\`, `launcher\` and `toolsuild\` are mirrored, the rest
  copied, the downloaded archives removed.
- Window resize and move: Direct3D 12 and Vulkan keep working through resizes
  that fail or change the buffers; the render buffer is remade once the size
  settles, not every frame.
- Online over the internet: the EOS SDK's headers and libraries are in the
  repository (`tools/eos`), and the release scripts refuse to package without
  them and `eos.ini` (0.1.10b went out without them).
- The home screen ring's picture (`image.png`) comes with the launcher.
- Load times: texture uploads batched on Direct3D 12 and Vulkan; load screens
  draw half as often; the launcher's compile is incremental (13.9 s to 1.2 s
  with nothing changed).
- Long sessions: textures found by a hash, not a list walk that grew all
  session; a memory line in the log every five minutes.
- Character editor: Motaro's page no longer fails (a strike named three times).
- PROFILE: a gradient picker (two colours, an angle, presets) and the
  fighter's second outfit as the player's picture, in the launcher and the game.
- SMOOTH MODELS: automatic sharp edges, with two sliders (which edges, how
  much) in GRAPHICS and the model viewer, and in the game's own GRAPHICS.
- SMOOTH MODELS: Kung Lao's hat is no longer pierced by his head -- the skin
  under a piece on its bones gets no correction pass (game and viewer).
- The game's Esc menu: PROFILE's background is the launcher's gradient picker,
  drawn the same, with the mouse, the keys and the pad; OUTFIT 1 / 2.
- Launcher version 0.1.11-beta.

## 0.1.10b-beta (2026-10-23) — video settings, settings in two columns

Built for Windows (64 and 32 bits) and Linux. Mary tried the two-column
menus and the new box before the last two changes below; those are untested.
Mary has now run the Linux package on a Linux machine, and played the
Vulkan and Direct3D 12 renderers: all three work.

- **VIDEO SETTINGS** on the settings screen, in the empty top-right box above
  SOUND VOLUME: it opens the settings menu's GRAPHICS page in the game's own
  panel, as BUTTON SETUP does, with the on-screen controls on or off.
- The settings in **two columns** side by side: the Esc menu, the debug menu
  (F2) and the game's own panels. The columns break only at a section's
  heading, so a section never runs into the other column; a row whose label
  and value would overlap is drawn smaller.
- CONTROLS: the keys and the pad's buttons split into FIGHT and MENU, each
  in its own column.
- Launcher: its own version `0.1.10-beta.b` reads as 0.1.10b, so it no
  longer offers the 0.1.10b release as an update to itself (`lib/update.js`).

## 0.1.10-beta (2026-10-23) — Kombos, SMOOTH MODELS, GPL

Built for Windows (64 and 32 bits) and Linux. Not yet played by Mary at release; the Linux package is untested on a Linux machine.

- Kombos (issue #114): when a player's button matched any entry of a combo
  table but the first, the port went on with the FIRST entry's next move.
  `combo_scan_a11` put the list back at its start on a match; the binary
  keeps the entry that matched, and `t_comb2` takes the next move from it
  (armv7 0x32776..0x3278e). Likely the cause of Liu Kang's 7-hit kombo
  breaking with the opponent sliding away; not yet tried in the game.
- Fixed: `t_r_combo0` called `set_no_block` on a path where the binary skips
  it (armv7 0x45da8).
- **SMOOTH MODELS** (off, level 1 or 2): the fighters drawn through Blender's
  Weld and Subdivision Surface (Catmull-Clark), in the game (GRAPHICS > VIDEO,
  `smooth_models=` in `umk3.ini`) and the model viewer, which can also play
  animations smoothed. The code is Blender's (`src/thirdparty/blender`).
- **The project is now GPL-2.0-or-later**, because it contains Blender's code.
  Releases up to 0.1.9-beta stay MIT.
- Crash reports and the session log name the version, the system, the
  renderer, the GPU and the settings in use.
- Linux: sound and music (the Linux game was silent); music needs libmpg123.
- SMOOTH ANIMATIONS also on the select screen and in the intro.
- Faster drawing of large models with Vulkan and DirectX 12.
- Fixed: SMOOTH MODELS drew dark wedges and black spots on some fighters
  (Jax, Sektor, Cyrax, Kano, Kitana, Mileena, Jade, Liu Kang).
- MODS > EXPORT: stages with their animation (FBX, USD); every export in its
  own folder: Characters, Stages, Models, and textures in their fighter's or
  stage's folder, Interface or Effects.
- MODS > TEXTURES: replace or export any texture of a fighter, a stage, the
  interface and menus, or the effects; a mod keeps each in that place's
  folder.

- **SMOOTH ANIMATIONS** (off by default): the launcher's GRAPHICS > VIDEO, or
  Esc > GRAPHICS > VIDEO in the game (applied at once), `smooth_anims=1` in
  `umk3.ini`. A fighter's frames are held for several ticks and drawn as hard
  cuts; with this on, each new frame is reached by blending from the one
  before. Drawing only: the game logic, hit boxes and events are unchanged.
  It applies to every fighter: fights, the select screen and the intro.
- Fixed: the game crashed when a fighter took Kabal's projectile (the
  reaction asked for a sound before setting its number; armv7 0x46396).
- Fixed: `tools/decomp/progress.py` looked one folder above the repository
  and counted every module as 0%.

## 0.1.9-beta (2026-10-22) — Vulkan and Direct3D 12, Linux Fixes

Built for Windows (64 and 32 bits) and Linux. Not yet played by Mary at release; the Linux package is untested on a Linux machine.

- **Vulkan and Direct3D 12 renderers** beside OpenGL (`src/runtime/render/`): every GL call goes through `rgl_front.c`; with Vulkan or Direct3D 12 a fixed-function layer (`rgl_ff.c`) keeps OpenGL 1.1's state and hands transformed vertices to `rgl_vk.c` or `rgl_d3d12.c`. Chosen in GRAPHICS > VIDEO or the in-game menu (`renderer=` in umk3.ini); a renderer that cannot start falls back to OpenGL. `tests/rgl_test.c` checks all three against the same calls (22 checks, 64 and 32 bits).
- **Move list:** the fatality page drew the special moves' buttons (section 2 starts after section 1's rows in the shared table, as the binary reads it).
- **Linux:** the game reads `umk3.ini` and `res/` beside its executable; it no longer links against libGL (loaded at run time); `text.c` includes `<stdarg.h>`; `build-game.sh`, `install-shortcut.sh`, `README-linux.md`, a CI job compiling every source with the bundled headers only.
- **Build from a terminal:** `launcher/app/cli.js`, run by `build-game.sh` / `build-game.cmd` with the launcher's own Electron.
- **eos.ini.example** for internet play.

- **Language in the in-game settings menu**, changed at once (texts and menus).
- **Mod textures reach the game's smaller copies** (`_LITE`, `_LOW`): a replaced fighter skin now shows at IPHONE texture quality.
- **Launcher:** smaller text everywhere; MODS fills the window; the model viewer has its options beside the model, a full screen button, REFRESH FOLDERS, textures by fighter, REPLACE TEXTURE from any picture (PNG, JPEG, WebP, GIF, BMP, ICO, AVIF, SVG, TGA) with the LOW and LITE copies made automatically; covers from any picture.
- **Linux:** the link no longer takes a 32-bit `libGL.so.1` from `/usr/lib` (Fedora and its kin): 64-bit directories first, each library checked for its width.
- **Viewer:** Kung Lao's hat (and anything else riding a fighter's bones), with ACCESSORIES to hide it; Reptile and Noob Saibot no longer show Scorpion's texture; texture quality NORMAL / LOW / LITE; playback speed.
- **[GOALS.md](GOALS.md):** what works and what comes next.

---

## 0.1.8-beta (2026-10-22) — Mods, Character Editor, Blender Round Trip

Built for Windows (64 and 32 bits) and Linux; not yet played by Mary at release. The Linux package is untested.

- **MODS in the launcher:** a mod manager (`mods/<mod>/`, files named like the game's replace them; priority, on/off, categories, covers), a model viewer that shows the game with or without mods, a character editor (each fighter's strikes with damage, chip and a visual hitbox editor, specials and finishers with the missing ones marked, per-fighter data, the game's texts, the fighter's textures), and export to FBX, USD (USDA/USDC/USDZ) and OBJ with a corrected T-pose.
- **From Blender back to the game:** an FBX animation replaces one of a fighter's clips, an FBX model on the same rig becomes a new `.skin`, both written into a mod.
- **The game reads mods:** `src/runtime/mods.c` lays `mods/` over `res/` and applies each mod's `data.ini` over the fight tables (`umk3_logic_names`, a new registry at the end of `logic_tables.c`).
- **Motaro and Shao Kahn** picked as FIGHTER 1/2 in the debug menu no longer crash the game (button rows, animation tables and strike rows stop before the bosses).
- **Linux:** bundled GL/KHR/SDL2 headers (`GL/gl.h` not found), soname link fallback, and a self-update that takes the Linux `.tar.gz`.

---

## 0.1.6-beta (2026-10-22) — Internet Play, Debug Menu, Linux Package

Tested by Mary on Windows (64 and 32 bits). The Linux package is built but untested.

- **ONLINE over the internet:** Epic Online Services (`src/runtime/mp_eos.c`), loaded at run time from the SDK beside the exe; 64-bit only. The release ships the SDK in `tools/eos` and the launcher copies it next to the game. LAN play works without it.
- **Rollback netcode** for online fights (`src/runtime/rollback.c`); each player picks their fighter's colour online.
- **Debug menu (F2)** reorganised like the settings menu: FIGHT, MENUS, TOOLS; new infinite health, freeze clock, FPS and online numbers (all off in an online fight).
- **Mouse wheel** in the debug and settings menus; **alerts answer the pad**.
- **iPhone-style boxes on Linux:** `src/runtime/platform/ui_alert.c` is shared by both backends (GDI on Windows, stb_truetype on Linux).
- **64-bit white textures:** the texture cache's entries come from below 2 GB (`umk3_alloc_low`); the C heap sometimes sat above 4 GB.
- **Linux package:** `UMK3-PC-0.1.6-beta-linux-x64.tar.gz`, the Electron launcher for Linux; needs SDL2 and OpenGL. Untested.
- **Release tooling:** `launcher/make_release.py` copies all of `src/` and the EOS SDK, and builds the stub with zig when llvm-mingw is missing.

### Also in 0.1.6 — Mileena's Fatalities

- **Nail fatality:** `RenderAnimatedCharacter` (0x5c2f4) now poses the skeleton on frames that draw no skinned body, as the binary does. Mileena's frames 310..334 are baked meshes, so her BIG CUP (bone 24) was riding the opponent's skeleton instead of her hand.
- **Suck-kiss fatality:** the port's "hold the last pose" for frames missing from a fighter's frame list now applies only to Classic Sub-Zero, the case it was written for. Held for everyone, it kept the victim on frame 327 after `set_inviso`, and the `MILEENA_SUK_<victim>` absorption replayed every eleven frames until the round ended.
- **Debug finisher on x64:** `opponent_dizzy` reads the engine's 32-bit words instead of 8-byte pointers (it crashed the scripted finisher test).

## 2026-10-18 — Repository Reorganization & Unified `.ipa` Build

- **Directory layout:** Separated source into `src/decomp/` (1:1 decompiled C) and `src/runtime/` (PC port layer), and tools into `tools/build/` (build-time `.ipa` & table extractors) and `tools/decomp/` (reverse engineering, Ghidra, `armrecomp`, `difftest`).
- **Single-command `.ipa` build (`build.py`):** Added `build.py` and `tools/build/unpack_ipa.py` + `CMakeLists.txt` (`-DUMK3_IPA=<path.ipa>`) to validate the 1.2.59 `.ipa`, unpack `res/`, extract the 229 data tables, and compile `umk3-game` and `umk3-fight-headless`.
- **Removed dead code:** Removed unused legacy prototype executables (`umk3-menu`, `umk3-fight`, `umk3-test`), old Win32 `launcher.c`, experimental `web/` wrapper, and one-off scaffolding scripts.

## 0.1.5-alpha (2026-10-16) — Online Update

- **PLAY > ONLINE:** Transcribed `limeMPGameSpecific.c` (48 functions, 14 packet types, 120-entry sprite-list queue) and added `src/runtime/mp_net.c` (UDP + LAN discovery) and `FE_Task_Online` (FE task 51).
- **Fixes:** Fixed `UpdateArcadeCode` guest/host `GameObjects` pointer, `Task_GameInit` online task transition (`Task_MultiplayerSync`), `dummyObjects` head pointer relink, and 64-bit `DrawAnimAsSprite` texture table width.

## 0.1.4-beta (2026-10-16) — 64-Bit Build & Arcade Crash Fix

- **64-bit build (`UMK3_LOW32`):** Added 32-bit pointer support (`__ptr32`) keeping game heaps and static data below 2 GB.
- **Fixes:** Fixed crash on winning Arcade / leaving stage 14 caused by non-NULL initial scene handles in `gamecode_globals.c`; added fallback animation frames for Classic Sub-Zero's elbow combo (`RPCOMBO6..9`).

## 0.1.3-beta (2026-10-16) — Controller Menu Navigation & Console Launcher

- **Menu navigation (`src/runtime/pad_nav.c`):** Full gamepad and keyboard navigation across all 51 front-end screens, two-player simultaneous character select, right-stick palette switching, and input prompt icons.
- **Settings & Launcher:** Added monitor selection (`monitor=`), player 2 pause/moves-list keys, and redesigned Electron launcher.

## 0.1.2-alpha (2026-10-15) — In-Game Settings & Player Profiles

- **In-game settings menu (`Esc`, `src/runtime/settings_menu.c`):** Live graphics reconfiguration (resolution, aspect ratio, texture quality) and live keyboard/gamepad rebinding without restarting.
- **Player profiles (`src/runtime/profiles.c`):** Persistent player ID, name, and customizable fighter badge (`profiles.ini`).

## 0.1.1-alpha (2026-10-15) — Player 2 Controls for Local Versus

- **Two-player controls:** Added Player 2 keyboard and gamepad bindings (`p2key_*`, `p2pad_*`) and dual-controller polling (`plat_pad2_read`).

## 0.1.0 / 0.1.0b-alpha (2026-10-15) — Widescreen, iPad Layout, HD Textures & Auto-Updater

- **Aspect ratios:** Added `3:2`, `4:3` (iPad camera projection), `16:9`, and `21:9` support.
- **iPad UI & DUELO mode:** Added `ui_size=ipad` (960×640 virtual layout) and local 2-player `DUELO` mode (`GameMode 6`).
- **Texture quality & fast loading:** Added `IPHONE`, `IPAD`, and `MAX` (4× Catmull-Rom upscale) texture modes, parallel PVRTC decoding (`src/runtime/lime/par.c`), and fast table-driven PNG inflate (`src/runtime/lime/png.c`).
- **Auto-updater:** Added GitHub release checker and in-place updater to the launcher.

## 0.0.8 / 0.0.9-alpha (2026-10-10 – 2026-10-15) — Gamepads, Save States, Hitboxes & All Finishers

- **Controls & zero input lag:** Added XInput/WinMM gamepad support, fixed hold-and-release special moves (`QueueAndJump`), and removed frame smoothing lag during fights.
- **Debug tools (`F2`):** Added hitbox viewer, fight speed control (10%–400%), and 4 in-memory save-state slots (`src/runtime/savestate.c`).
- **Finishers:** Fixed `MERCY` banner timing and verified all Fatalities, Animalities, Babalities, and Friendships.

## 0.0.1 – 0.0.7b-alpha (2026-10-08 – 2026-10-09) — Initial Playable Native Port

- **Full decompilation & integration:** Connected all 2,572 decompiled functions (`lime/`, `gamecode/`, `gamecode/logic/`) with the cooperative coroutine scheduler (`src/runtime/fight_runtime.c`) and the 229 extracted binary data tables (`tools/build/logic_tables.py`).
