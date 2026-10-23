# Project Status & Verification Progress

**Last updated:** 2026-10-22 · See [CHANGELOG.md](CHANGELOG.md) for release history and [HOW-THE-GAME-WORKS.md](HOW-THE-GAME-WORKS.md) for architecture.

---

## Summary

| Area | Total Functions | Transcribed (`src/decomp/`) | Status |
|---|---:|---:|---|
| **LIME 3D Engine (`src/decomp/lime/`)** | 109 | 109 (100%) | Verified against recompiled oracle (`tests/test_*_diff.c`) |
| **Game Front-End & Scene (`src/decomp/gamecode/`)** | 291 | 291 (100%) | Verified & running natively (51 front-end screens, HUD, audio, blood) |
| **Arcade Fight Engine (`src/decomp/gamecode/logic/`)** | 2,172 | 2,172 (100%) | Behaviourally tested file-by-file (`tools/decomp/difftest/`) + 229 data tables verified by `ctest` |
| **Total Game Core** | **2,572** | **2,572 (100%)** | **98.30% weighted verification score (`tools/decomp/progress.py`)** |
| **Cross-Platform Launcher** | Multiplatform (Win/Linux) | 100% pure Node.js / Electron | Standalone compilation via `zig cc` (0.13.0), zero Python/CMake runtime dependency |

---

## After 0.1.9: SMOOTH ANIMATIONS (2026-10-22, not released)

Mary asked for an on/off option that makes the fighters' animations look
smoother, drawing only, without changing the animations.

- **What the game does:** a fight frame is held for several 60 Hz ticks and
  drawn as a hard cut. The iOS original ran `PlayerAutoSmoothAnims` with a
  window of 2 in fights (a delayed blend); the port sets it to 0 because the
  delay felt like input lag (game_main.c).
- **The option** (`port_smooth_anims`, `smooth_anims=` in `umk3.ini`, the
  launcher's GRAPHICS > VIDEO checkbox, the in-game GRAPHICS > VIDEO row) runs
  `port_smooth_pose` (GameCode.c) from `RenderPlayer` (the fight and the
  select screen) and before both intro draws, with one state per PLAYER; the
  fight's projectile slots are skipped. The first version only covered the
  fight, and Mary saw no change in the intro. When the game's frame changes, the drawn pose blends from the
  previous frame into the new one over as many ticks as the previous one was
  held, at most 5 (up to 4 ticks, 67 ms, behind the logic frame while
  blending). It writes only `frameB` (+0x520) and `t` (+0x524); `frameA`
  stays the game's frame, so the mesh record, the second skin and the scene
  events are the game's. No blend across a non-skinned frame, a second-skin
  change, a character/model change, 16/35..41, or IsLiaProblem /
  IsOstrichProblem; a blend the game set itself is left alone.
- **Verified:** both exes build from Mary's `.ipa`; launcher tests pass
  (ini round trip includes `smooth_anims`). **Not seen in the game yet:**
  Mary runs it.

**Crash when two projectiles meet (open).** SCORPION vs KABAL, both throwing
at once: `AddNewGameEvents` -> `get_csound` read past `ochar_sound_tables`
(26 rows) -- a character sound event (MKEvent 2/3, param = char << 8 + n)
carried a character number of 26 or more. The four senders (`ochar_sound`,
`ochar_sound_n`, `ochar_sound_c`, `his_ochar_sound`, armv7 0x57b94..0x57bfc)
match the binary, so the bad number comes from what calls them. Until the
sender is found, Blood.c skips such an event and prints `csound: character N
out of range (event param 0x...)` to the log: the next report gives the value.
Mary also reports screams repeating when a fighter takes a fatality, and
fatalities whose animation does not finish: not investigated yet.

## Release 0.1.9 beta (2026-10-22): Vulkan and Direct3D 12, Linux fixes

### Renderers (src/runtime/render)

Mary chose renderers written for the game over a translation layer (Mesa's
Zink / D3D12 drivers were offered).

- `rgl_front.c` defines every GL 1.1 call the tree makes (the list
  `platform/win32_gl_cdecl.c` had, now deleted) as plain C on every target.
  With OpenGL it forwards to the driver's entry point, fetched by name
  (opengl32.dll; libGL.so.1 by dlopen, so Linux no longer links libGL).
  `platform/gl.h` strips dllimport/stdcall on all of Windows, not only
  32-bit.
- `rgl_ff.c`: OpenGL 1.1's fixed function as the game uses it -- three
  matrix stacks, enables, blend, alpha test, depth, cull, colour mask,
  viewport and scissor, current colour, texture environment, one texture
  unit with its parameters and mipmaps (box-filtered on the CPU), client
  arrays and glBegin/glEnd, pixel store, glPushAttrib, and the framebuffer
  objects game_main.c's 3D resolution and antialiasing use (answered
  through plat_gl_proc). Draws are assembled into transformed triangles
  or lines and handed to a backend with a pipeline key.
- `rgl_vk.c` (Vulkan 1.0, loaded at run time) and `rgl_d3d12.c` (Direct3D
  12, Windows): one frame in flight (OpenGL drew with glFinish after each
  swap), one command buffer in the game's order with a flush when a texture
  in use is rewritten or pixels are read, render passes / targets that
  load, clears inside the scissor, pipelines cached by state, MSAA targets
  resolved, scaled blits (vkCmdBlitImage; a textured quad on D3D12).
- Shaders: `render/shaders/ff.vert`, `ff.frag` (glslang 16.6.0 -> SPIR-V)
  and `ff.hlsl` (fxc, SM 5.1), embedded by `shaders/build.py` into
  `rgl_shaders_vk.h` / `rgl_shaders_d3d12.h`; the game's build needs
  neither tool.
- Vulkan headers 1.3.296 in `platform/vulkan-include` (Apache-2.0 / MIT).
- Chosen with `renderer=opengl|vulkan|d3d12` (game_main.c read_config),
  GRAPHICS > VIDEO in the launcher, or Esc > GRAPHICS > RENDERER (from the
  next start). A backend that does not start falls back to OpenGL.
- **Checked:** `tests/rgl_test.c` draws the same calls on a hidden window
  with each renderer and reads the pixels back: clear, an untextured quad,
  a 2x2 texture's orientation, MODULATE / REPLACE, two blend modes, the
  alpha test, depth, culling (back and front), a scissored clear, the 3D
  resolution path (a 4x multisampled buffer resolved, then scaled onto the
  window) and three presents. 22 checks, all pass for OpenGL, Vulkan and
  Direct3D 12, 64-bit and 32-bit, on an RTX 5070. Vulkan's front face had
  to be counter-clockwise (the first guess, clockwise, failed the cull
  checks). **The game itself has not been run on them: Mary's test.**

### Move list (GameCode.c DrawCharacterPage)

A tester reported the fatality page showing the previous page's entries.
Both sections of a MovesListTab entry point at the same table (Kano:
0x103958 for both, read from the binary), and page 4 reads the notation at
`(section 1's rows + row) << 6` (armv7 0x1fd74..0x1fd7e); the port started
section 2 at row 0, so the finishers' names came with the specials'
buttons. Fixed; not yet seen in the game.

### Linux

- The game reads `umk3.ini` and `res/` beside its executable
  (`/proc/self/exe`), as on Windows; before, `res` came from the current
  folder and umk3.ini was never read, so nothing the launcher set reached
  the Linux game.
- `text.c` included no `<stdarg.h>` (Windows' headers bring it); found by
  `tools/ci_compile_linux.py`, which compiles every Linux source with the
  bundled headers only (78 of 78), and runs in `.github/workflows/linux.yml`
  with the launcher's tests. It cannot link: the data tables come from the
  .ipa.
- `launcher/app/cli.js` with `build-game.sh` / `build-game.cmd`: the
  launcher's build from a terminal, run with its own Electron as Node.
  `install-shortcut.sh` writes a .desktop entry; `README-linux.md`
  (packages by distribution, what to run, writable folder, renderer,
  internet play); `eos.ini.example`; `make_release_linux.py` packs the tar
  with the executable bits.
- From a tester's notes (Mary relayed them): AppImage was not made; the
  CI cannot build a known .ipa (it is copyrighted) or run the game under
  xvfb.

## After 0.1.8: in-game language, viewer layout, texture replacement (2026-10-22, released in 0.1.9)

What Mary asked for after 0.1.8, before the Vulkan and DirectX 12 work. The
list of what works and what comes next is now [GOALS.md](GOALS.md).
**Mary has not tried any of it yet.**

### Game

- **LANGUAGE in the settings menu** (GRAPHICS > INTERFACE, settings_menu.c):
  the system's, English, Spanish, French, German, Italian, Korean, Chinese,
  taken at once. `port_language_set` (game_main.c) sets `Language` as
  start_app does (English when Info.plist has no `TOS_URL_` for it) and
  loads the text table again as Task_LoadGeneralData does
  (`LoadTextData(LANGUAGE_TEXT_xx)`); the old table stays allocated, since a
  screen may still hold one of its strings. The port's own menus follow
  `Language` already. Written to umk3.ini as `language=`, the launcher's key.
  Compiled for both builds; not run.
- **A mod's texture now serves the game's smaller copies** (mods.c
  `mods_find_texture`): X_LITE and X_LOW by X, X_LITE2 and X_LOW2 by X2. At
  IPHONE texture quality a fighter is drawn with its 256 `_DIFFUSE_LITE`
  skin (Players.c `port_load_lite`), so a mod with only `KANO_DIFFUSE.png`
  did not show in a fight: the likely reason behind Mary's "it does not
  apply". Checked standalone against a mods folder (all seven names
  resolve).

### Launcher

- **Linux link fix** (launcher/app/lib/compile.js): Mary's Linux build got
  as far as linking and stopped at `ld.lld: error: /usr/lib/libGL.so.1 is
  incompatible with elf64-x86-64`. On Fedora and its kin /usr/lib holds the
  32-bit libraries and /usr/lib64 the 64-bit ones; the library search
  started at /usr/lib. Now the 64-bit directories come first and each
  library is checked for the build's ELF class before it is passed to the
  linker. Not yet rebuilt on Linux.
- **Checked on Mary's Rain mod** (OLDSUBZERO_DIFFUSE2.png, 1024): `putTexture`
  run in Electron wrote OLDSUBZERO_DIFFUSE2_LOW.png (512) and
  OLDSUBZERO_DIFFUSE2_LITE.png (256), the game's own sizes; the viewer's
  reader (lib/mods.js `resRead`, now with the same LOW / LITE / LITE2 /
  LOW2 fallbacks as the game) returns the mod's three files with SHOW WITH
  Rain and the game's for the rest.

- Every font size is in a new unit `--t` (tokens.css), `--u` x 0.82: all
  the text is smaller, the boxes stay the same size. The hitbox editor's
  label too.
- MODS uses the whole window (App.svelte `.stage.wide`).
- **Viewer**: the list on the left, the model in the middle, its options on
  the right (animation, outfit, bones, wireframe, the target mod, textures,
  Blender, export); FULL SCREEN (the page's fullscreen, Esc leaves);
  REFRESH FOLDERS (`reloadMods`: the mod list and every cached mod file read
  again, the open item opened again; the manager's REFRESH does the same).
- **TEXTURES by fighter**: a fighter list over the texture list
  (`^(IPAD)?<ID>(_|BABY|BABILITY|PORTRAIT|X_)`); SEE ITS TEXTURES from a
  fighter.
- **REPLACE TEXTURE** in the viewer for fighters, stages, models and
  textures, and in the editor's TEXTURES (`lib/mods/image.js`
  `putTexture`): any picture the window decodes (PNG, JPEG, WebP, GIF, BMP,
  ICO, AVIF, SVG) and TGA (own decoder, checked on plain and RLE files),
  written as the texture's PNG, and with MAKE THE LOW AND LITE COPIES (on
  by default) each smaller copy the game has, at that copy's own size.
  With it off the editor lists the copies to replace one by one.
- Viewer, fighters: TEXTURE QUALITY NORMAL / LOW / LITE shows the outfit
  with the game's smaller copy (the separate TEXTURES list there repeated
  the OUTFIT one and is gone); REPLACE TEXTURE takes the outfit shown.
  SPEED 0.1x to 3x beside the frame bar. The home menu keeps its old text
  size (Mary).
- Viewer: Reptile and Noob Saibot showed Scorpion's texture. Their meshsets
  carry a mesh textured SCORPION_DIFFUSE of the same triangle count as
  their own, and the block's texture was taken from the first match; now
  the fighter's own <NAME>_DIFFUSE wins (assets.js loadFighter).
- Viewer: Kung Lao's hat. What rides a fighter's bones is in its .scene: a
  key per frame with the visibility, a placement matrix (palette) and, in
  byte +4, the bone (RenderScene.c scene_attach_to_bone; parseScene now
  reads it). The meshset meshes it names are placed by the key's matrix
  and then the bone's, frame by frame: the hat on bone 7 (head) or in a
  hand (11, 19) when a throw has it there. ACCESSORIES turns them off.
- **COVER** takes any picture too, made a PNG no wider than 640 in the
  window. Before, the file was copied as it was and a cover over 4 MB (a
  photo) was never shown.

## Release 0.1.8 beta (2026-10-22): character editor, mods in the viewer, Blender round trip, playable bosses

Published as the GitHub pre-release `v0.1.8-beta`, packaged as 0.1.7 was
(`launcher/make_release.py` over `electron-builder --win dir`; `electron-
builder --linux tar.gz` repacked with exec bits). On top of the MODS section
below (unreleased until now), what Mary asked for next:

### Motaro and Shao Kahn as players (debug menu FIGHTER 1/2)

- Mary's crash report (`running address 0x04840a04`, SHAO KAHN vs MOTARO,
  both builds, "always been there"). Reproduced with no window: the headless
  fight simulator (`fight_headless.c`) built with a crash handler, Shao Kahn
  as player 1 holding one button. The game never hands a boss to a player,
  and the per-fighter rows a player's buttons read stop at 24 or 25 words:
  - `DoSwitchJump` (other.c) read `row[25]` past the button rows and called
    the next table as code. Port guard: no button handler for c >= 24.
  - `get_char_ani*` / `get_his_char_ani*`: the bosses' animation tables hold
    38 animations, the others 73, and Motaro's second table is not a table.
    A flip read frame data as an animation (`find_last_frame` from
    `t_do_flip`). Port guard `ani_fetch`: a boss's missing animation is its
    stance.
  - `get_char_stk`: Motaro's strike row has 7 entries and Shao Kahn's 6
    (22 to 27 for the others). Port guard: none, and the two strike checks
    treat a missing strike as no hit.
- Checked: 736 headless fights, both bosses as player 1 and 2 against four
  fighters with 46 held inputs each, none crash; ordinary fights unchanged.
  **Mary has not played it yet.** The bosses only do what the game gives them.

### Mod data: `data.ini` over the fight tables

- Both table generators (`launcher/app/lib/extract/logic_tables.js`,
  `tools/build/logic_tables.py`) end `logic_tables.c` with
  `umk3_logic_names[]`: every word table by its name in the binary, its
  address and length.
- `mods_apply_data()` (runtime/mods.c), called after the asset root is set:
  each mod's `data.ini` lines `table[word] = value`, lowest priority first.
  Unit-tested standalone (priority, signed values, unknown names reported).
- A strike record (`stk_*`, 7 words, `strike_check_regs`): reach, top,
  width, height (signed 16-bit, measured from the fighter facing right:
  left = x + reach - width), reaction, damage (high byte full, low byte chip
  on block), squeeze. Shared rows: the six ninjas use `nj_strikes`, the
  three robots `robo_strikes`, the three female ninjas `fn_strikes`.
- Data mods change the fight: online, both players need the same mods.

### Launcher

- **CHARACTER EDITOR** (`sections/mods/ModEditor.svelte`, new tab): per
  fighter, the basic strikes (damage, chip, box, reaction) with a visual
  hitbox editor (`HitboxEditor.svelte`, drag to move, corner to size); the
  special moves and finishers the move list names (`seq_<fighter>_*`), with
  the missing ones marked (Noob Saibot, Motaro and Shao Kahn have no
  specials; Reptile and the hidden ninjas lack finishers); every per-fighter
  numeric table; the game's texts (a copy of `<language>_text.dat` in the
  mod: names, move list); the fighter's textures replaced by PNG. Reads
  `build/logic_tables.c` (`gameTables` in lib/mods.js).
- **Viewer**: SHOW WITH the game alone, the mods that are on, or one mod
  (`resRead(rel, layer)`, pictures told apart by their bytes); EXPORT ITS
  TEXTURES; EXPORT RIG (FBX); REPLACE ANIMATION / REPLACE MODEL from an FBX
  (`ui/src/lib/mods/fbxin.js`): a binary FBX reader, the FBX transform
  chain, and a similarity fit from the file's cluster rests to the game's
  T-pose or bind pose, so Blender's axes and units do not matter. An
  animation replaces one clip's frames in the `.skinanim` (resampled to the
  clip's length); a model is written as a new `.skin` on the same bones
  (each material to one of the fighter's blocks). Checked through Blender
  5.2 (import and export with defaults): Kano's clip back within 0.05
  units, Sindel's model (2 blocks) within 0.14; from the T-pose rest the
  model within 3.7 (the T-pose fit's own error). **The game has not loaded
  a replaced .skin yet: Mary's test.**
- Manager: COVER (cover.png/jpg, preview.* still read), SEE IN THE VIEWER.
- Not done: duplicating a fighter into a new slot (27th). Every per-fighter
  table of the game (strike rows, animation tables, the select screen's 26
  places, names, sounds, portraits) would have to grow; a session of its own.

## After 0.1.7: MODS section, game-side mod overlay, Linux build fixes (2026-10-22, released in 0.1.8)

Mary asked for a MODS option in the launcher's menu: a mod manager, a viewer
of the game's files and an exporter for Blender. All three are in, plus the
two Linux faults she reported on 0.1.6/0.1.7.

### The game reads mods (`src/runtime/mods.c`)

- `<game>/mods/<folder>/**` is indexed by lowercased file name at start-up
  (`mods_init`, called from `set_asset_root`). A mod replaces a game file by
  carrying a file of the same name, in any sub-folder.
- `mods/mods.ini` holds the order, highest priority first, as `on=<folder>`
  / `off=<folder>` lines; folders it does not name are on, after the rest.
  The first mod in the order that has a name wins.
- Hooked in `lime_platform.c` `resolve()` (every `limeLoadFile`, sound and
  music path) and in `draw_gl.c` `load_rgba`, where textures also match by
  stem (a `.png` in a mod replaces the game's `.pvr`) and a mod's `X` also
  serves `X_LOW`.
- `mod.ini` and `preview.*` at a mod's top level are the mod's own and are
  never served. Tested standalone (priority, sub-folders, _LOW, unnamed
  folders on).

### Launcher: MODS (`ui/src/lib/sections/SecMods.svelte`, `lib/mods.js`)

- **Manager**: create a mod (name, author, category: textures, audio, music,
  models), order (up/down = priority), on/off, category filter, preview
  picture, details with every file and the game file it replaces (files that
  replace nothing are flagged), add files, open the folder, delete (a second
  press). Writes `mods/mods.ini` and `mods/<mod>/mod.ini`.
- **Viewer** (three.js 0.170): fighters, stages, other models and textures
  from `res/`. Fighters are skinned on the CPU from the game's `.skin`,
  `.bones` and `.skinanim`; one clip or all, play/pause/frame, outfits,
  bones, wireframe, T-pose or the game's own pose. Textures can be copied
  into a mod to edit.
- **Exporter**: FBX (binary 7.4: skeleton, skin, one action per clip), USD
  as USDA, USDC (crate 0.8.0, written by `ui/src/lib/mods/usd.js`) or USDZ,
  and OBJ (mesh only); textures as PNG. Fighters with every animation, one,
  or none; rest pose T-pose or bind; their other outfits as PNG. Written to
  `<game>/exports/<name>_<FORMAT>/`.
- The T-pose: the rest pose is not in `.bones`, which is why exports came
  out broken. `ui/src/lib/mods/rig.js` solves the bind pose from the clips
  (least squares over rigid fits), finds hips/spine/arms/legs from the
  skeleton's shape, faces the figure along +Z by its knees, turns the head
  to the front by symmetry (Kano's head was sideways), straightens it, and
  fits a rest mesh so Blender's skinning reproduces the game's within about
  2 mm on average. Motaro (centaur) keeps the bind pose. Sindel's hair took
  the wrong texture; blocks now take their texture from the meshset mesh
  with the same triangle count.
- Checked by importing into Blender 5.2 headless (FBX, OBJ, USDA, USDC,
  USDZ: bones, actions, textures) and by opening the USDC in pxr 26.03.

### Linux

- `fatal error: 'GL/gl.h' file not found` (Mary's 0.1.7 log): the launcher
  now ships the GL, KHR and SDL2 headers in
  `src/runtime/platform/linux-include/` (Mesa/Khronos MIT, SDL zlib; see its
  README) and links `libSDL2-2.0.so.0` / `libGL.so.1` by soname when the
  `-dev` symlinks are missing. The runtime cross-compiles for
  x86_64-linux-gnu with Zig against those headers alone; **linking on a real
  Linux machine is untested.**
- "could not unpack the update" (0.1.6): the updater took the Windows zip.
  It now picks the `linux*.tar.gz` asset, unpacks it with the system `tar`,
  and installs with a shell script (`apply.sh`) that waits for the launcher
  to exit, swaps the folders, keeps `res/`, `mods/` and the ini files, and
  restarts it. Linux users on 0.1.7 still have the old updater and must
  download the next release by hand once.

## Release 0.1.7 beta (2026-10-22)

Published as the GitHub pre-release `v0.1.7-beta`, built the same way as
0.1.6 (`launcher/make_release.py` over `electron-builder --win dir` for the
Windows zip; `electron-builder --linux dir` repacked with exec bits for the
Linux tar.gz). On top of 0.1.6: the launcher rework and the 64-bit debug
crash fixes below, the SvelteKit port of the launcher's window, and the
Linux Zig download fix. Mary asked for the release before testing the debug
fixes; the port's launcher was in her test folder (`launcher/bin`) and
opens there.

### The launcher's window is a SvelteKit app (`launcher/app/ui/`)

- Svelte 5 + SvelteKit 3 (`@sveltejs/kit` 3.1, `adapter-static`, hash
  router), built into `launcher/app/renderer/` by `npm run ui`
  (`scripts/build-ui.js`), which electron-builder also runs before it packs
  (`build.beforePack`). `renderer/` is generated and git-ignored.
- `build-ui.js` makes SvelteKit's root `/_app/` paths relative (the page is
  loaded from disk) and recomputes the Content-Security-Policy hashes of
  the inline start-up script and of SvelteKit's route announcer's style.
- SvelteKit 3 notes: the config lives in `sveltekit({...})` in
  `ui/vite.config.js` (no `svelte.config.js`); `$lib` is gone, the code
  imports `#lib/...` through `ui/package.json`'s `imports`; no page options
  with the hash router.
- One theme: `ui/src/lib/theme/tokens.css` names every colour, surface,
  finish and size; `base.css` holds the shared controls. State is one
  `$state` object in `app.svelte.js`; sections and pieces are components.
- Matched to Mary's high-quality capture of the console's main menu:
  eight-block rings (36 degrees a block, 9 between) with brushed-silver rims
  and a lavender glow, 39 units between the rings' halos; a thin ring round
  the picture; the menu's curve, text size and spacing measured off it; the
  selection orb is a glass sphere with a red M turning in it; the command
  bar higher, slanted ends, fine lines; the cloud crimson top left and
  bottom right with a blue streak down the middle; the theme in lavender,
  silver and navy, the pages' glass deeper so text reads over the traces.

### Linux: Zig download stopped at once (`EXIT CODE: -1`)

- The log said only `Build failed:` -- an error with no message, which is
  what Node's `https` gives (an AggregateError) when every address of the
  server runs out of its connect time: Node 20 tries IPv6 then IPv4 for
  250 ms each. `lib/toolchain.js` now allows 5 s per address, retries over
  IPv4 alone, and reports the parts of such an error (`describeError`).
- Checked on Windows: the Linux archive downloads and its SHA-256 matches.
  The original failure could not be reproduced here (a fast link); **not
  yet run on Linux**.

## Launcher rework (2026-10-22, not yet tested by Mary)

- Menu: smaller lines and orb; PLAY moved second, under COMPILE YOUR .IPA.
- No game compiled in either build (`root:get` -> `any`): the menu shows
  COMPILE alone; the first successful compile relaunches the launcher
  (`app:relaunch`, app.relaunch + exit) so the whole menu appears.
- A section opened from the menu fills the window, after the console
  release's online pages: its name between blue arrows (Q / E, the pad's
  triggers or bumpers step through sections), the GRAPHICS and CONTROLS
  pages in a second row between arrows (left / right, the d-pad), the
  chosen page with light lines over and under it; Esc / B returns to the
  menu, which keeps its old layout.
- Backdrop shared by both: a generated cloud of red, purple and blue light
  (`nebulaURL`), dark circuit grooves with grey edges, a big ring of short
  thick marble blocks and, on the menu only, a smaller one to the right.
- Home screen laid out as the console's main menu, in the rings' own
  1600 x 900 box (`--k` in the theme's tokens.css), so it holds at any window size:
  the title top right with a dark border, a white-blue halo and light rays
  behind it; the player's picture `aditional textures/image.png` (read at
  run time by `hero:get`, never shipped: it is game art) inside the small
  ring, tinted blue and faded towards every edge; the description box under
  it; the menu down the left 70 units a line. The version moved to the top
  left corner.

## Debug crashes on 64-bit, fixed (2026-10-22, not yet tested by Mary)

Both from Mary's crash logs, symbolized with the build's .pdb:

- Opening the debug menu, or its STAGE list: `stage_name` (debug_menu.c)
  read Level_Info's name field (+0x1c, a 32-bit word) as an 8-byte pointer
  and snprintf crashed. Now `const char *P32`.
- HITBOXES on: `hb_draw` (game_main.c) read the fighters at G +0xbc /
  +0x214 the same way and `hb_body` crashed. Now `P32` too.

Both files compile for x86 and x64. Mary also reported that the stage of
a debug fight stays chosen afterwards. Not changed: her logs show no debug
fight in the session where CAVE repeated (Arcade, then Survival, then
Karnage), and Karnage and Survival keep `LevelSelect` by the iOS game's
own rule (FrontEnd.c, modes 3/4/5). Waiting for her steps.

## Reported by Mary on 0.1.6 beta, to fix after the launcher rework (2026-10-22)

Not investigated yet; each needs its mechanism traced in the binary first.

- **64-bit:** the FLAWLESS VICTORY text is not shown (fine in 32-bit).
- **Reptile's fatalities** sometimes do not work fully; in one of them he
  only raises his arms.
- **Hitboxes** (the debug view) crash the game. Fixed, see above.
- **Linux:** compiling in the launcher fails at once:
  `1/5 Preparing compiler toolchain... Starting download of Zig 0.13.0
  compiler / Build failed: / EXIT CODE: -1` (lib/toolchain.js).

## Release 0.1.6 beta (2026-10-22)

Published as the GitHub pre-release `v0.1.6-beta`, two packages:

- `UMK3-PC-0.1.6-beta.zip` (Windows): `launcher/make_release.py` over the
  `electron-builder --win dir` output. It now copies all of `src/` (the old
  file list had lost `mp_eos.c` and `ui_alert.c` and still named the
  removed .ps1 scripts), the EOS SDK from `TOOLS/eos` (headers, the Win64
  DLL, eos.ini; git-ignored, never in the repository), and builds the stub
  with zig when llvm-mingw is missing. Checked: the release folder compiles
  umk3-game64.exe (with EOS) and umk3-game.exe from the 1.2.59 .ipa.
- `UMK3-PC-0.1.6-beta-linux-x64.tar.gz`: `electron-builder --linux tar.gz`,
  repacked so the launcher and helpers are executable (a tar made on
  Windows has no exec bits). The EOS SDK lands in `resources/tools/eos`
  (Windows merges TOOLS/ into tools/), so `findEosDir` in compile.js looks
  for both spellings. **Never run on Linux.**
- Known: the launcher's updater takes the first .zip, so a Linux launcher
  would offer the Windows zip; Linux players update by hand.
- Open question for Mary: whether the EOS SDK headers may be redistributed
  under Epic's licence, and whether eos.ini's client credentials are a
  game-client policy (they are public in the zip).

## Debug menu, mouse wheel, iPhone-style boxes on Linux (2026-10-22, tested on Windows)

Port, not in the binary. Built for Windows x64 and x86; the Linux files
pass a zig `x86_64-linux-gnu` compile with the SDL2 lint header, but nobody
has run them on Linux yet. Mary tested all of it on Windows on 2026-10-22.

- **Debug menu (F2)** is laid out like the settings menu (Esc): a side panel
  with three categories (FIGHT = fighters + round + Arcade + cheats, MENUS,
  TOOLS/OPCIONES = options + tools + save states), sections under headings,
  a list that scrolls, and a list of values that opens with Enter or a
  click. Labels are Spanish or English, following the game's language.
  Picking a SCREEN or a FINISHER from its list does it at once.
- **New debug options:**
  - INFINITE HEALTH (player 1, 2 or both) and FREEZE CLOCK. These are the
    PortDbgHealth/PortDbgClock hooks in logic/other.c's bar_reducer and
    t_clock4, beside training's. game_main.c forces them off in an online
    fight.
  - FRAMES PER SECOND and ONLINE NUMBERS (rb_stats: frame, delay, remote
    lead, rollbacks, deepest, holds, desyncs), drawn under the INFO line.
- **Mouse wheel:** plat_mouse_wheel (WM_MOUSEWHEEL, SDL_MOUSEWHEEL) moves
  the cursor in both menus and in their open lists.
- **ui_alert.c:** the iPhone OS 3 boxes now live in one file shared by both
  backends: the alerts (plat_ask), the menus, the grid, the badge, and the
  pad button pictures.
  - Windows supplies GDI's Arial and its raw keys (ui_alert.h).
  - Linux uses stb_truetype (platform/stb, public domain/MIT) with the first
    font it finds: "aditional textures/fonts/ui_bold.ttf"/"ui.ttf", then
    Liberation Sans, DejaVu, Noto, then fontconfig. Linux also gets the
    keyboard and pad pictures, and the pad's kind comes from the SDL vendor
    id.
- **Alerts with the pad:**
  - Left / right (pad or arrow keys) light a button.
  - South or START presses the lit button, or OK when none is lit.
  - East is CANCEL.

## Rollback netcode for ONLINE fights (2026-10-22, in 0.1.6 beta)

Status: **tested by Mary on one PC, two windows (LAN), 64-bit:** fights run on
both, 0 desyncs, snapshots 0.8 ms / 6.1 MB, a few holds. Not yet tested over the
internet or 64 vs 32 bits. Port, not in the binary.

Fixed while testing (2026-10-22):
- `limeMalloc` takes its blocks from the save-state arena during a rollback fight;
  its own region is not rolled back, so a re-run frame freed a block twice.
- Snapshots skip `PreloadedCharacters` (60 MB the fight only reads); the copy was
  66 MB and 11 ms a frame.
- The guest waits, before loading the fight, for the host's hello and takes its
  level. Before, each side loaded its own random level; with the classic netcode
  the host's level arrived mid-fight and the guest crashed in `RenderLevelBG`.
- Multiplayer versus screen: the kode wheels each side may press were transcribed
  wrong (armv7 0x16b98: host 0..4, guest 7..9); portraits and wheels now sit at
  the screen's edges as on `FE_Task_VS_Screen`.
- 64-bit crash reports unwind the stack (.pdata) and print exe offsets.

- `src/runtime/rollback.c`: both machines run the fight from both players' inputs;
  only inputs travel (packets 20 INPUT, 21 HELLO, 22 HASH over `mp_net`). The
  remote input is predicted as the last confirmed one; a late input that differs
  rolls the state back and re-runs the frames silent and undrawn. Input delay 2
  (`UMK3_RB_DELAY`), at most 8 frames predicted, a desync hash every 60 frames
  (`DESYNC at frame` in the log). `UMK3_NETCODE=classic` turns it off.
- `src/runtime/savestate.c`: per-frame snapshots (`st_rb_*`) of `.umkstd/.umkstb`
  plus the arena pages written during the fight (MEM_WRITE_WATCH). Files built with
  `UMK3_NO_ROLLBACK` (lime_menu, lime_platform, lime_app, wav, limeMPGameSpecific)
  keep their state out of it; `PreloadedCharacters` too.
- Determinism: `random32` uses `umk3_rand`, held in the state; the frame step is
  pinned to 1. A recorded fight replays identically on x64 over 3038 ticks; x86
  differs only in the event queue (address-derived arguments), not yet fixed.
- No pause in a rollback fight yet (a synced pause is still to do).
- Runs over the LAN transport and over EOS (0.1.6 beta).

## Mileena's fatalities (2026-10-22)

Both were reported by the tester and both are confirmed fixed in her build.

- **Nail fatality (BIG CUP in the wrong place):** checked against the binary function by function (`RenderLevelPlayers`, `RenderPlayer`, `RenderAnimatedCharacter`, the bone attach in `LIME_RenderScene`, `limeMatrixMult`). The missing piece was the `else` arm of `RenderAnimatedCharacter` at 0x5c2f4: a frame whose body is not skinned still calls `GenerateMatrices`. Without it, `MatrixPalette2` kept the other fighter's pose and the cup followed his hand.
- **Suck-kiss fatality (victim's body kept reappearing):** `RenderLevelPlayers`, the event code (`LIME_TriggerEventFromSceneH`, `LIME_UpdateEvents`, `LIME_RenderEvents`, `CountEventsMatching`, `LIME_TriggerEventsFromSceneOffsetIfFollowing`), `mk3_update`, `t_r_kiss_suck`, `t_mframew` and `fastxfer_thread` all match the binary. The divergence was a port-only block that replaced a frame-list `-1` with the last pose. The victim's frames 1399..1405 are `-1`; held at 327, an invisible fighter re-fires that frame's events every draw. The block now applies only to Classic Sub-Zero.

## What was built and verified in this session (2026-10-18)

1. **Pure Node.js Extraction Pipeline (`launcher/app/lib/extract/`)**:
   - `zip.js`: Zero-dependency PKZIP reader supporting uncompressed (0) and Deflate (8) streams using Node's standard `zlib`.
   - `macho.js`: 32-bit universal (fat) and thin Mach-O parser verifying armv7 slice, UUID (`90d6f56a18e2303f8f2053b02a42742e`), and FairPlay encryption (`cryptid == 0`).
   - `level_info.js`: Extracts the anonymous arena string pool and pointer relocations into `level_info.c` (verified 100% byte-for-byte identical to Python).
   - `seq_data.js`: Extracts playback scripts into `seq_data.c` (verified 100% byte-for-byte identical to Python).
   - `logic_tables.js`: Reads data tables and performs 4,291 pointer relocations across 1,118 symbols into `logic_tables.c` (verified 100% byte-for-byte identical to Python).
   - `low32_static.js`: Generates 64-bit low-pointer fixups (`umk3_globals_fixup`) for `gamecode_globals64.c`.
   - `ipa.js`: Orchestrates binary verification, asset unpacking (`res/`), and C table generation in ~2 seconds.

2. **Standalone Toolchain & Parallel Compiler (`launcher/app/lib/toolchain.js` & `compile.js`)**:
   - Automatic download and extraction of pinned Zig 0.13.0 with SHA-256 verification (fallback to system Zig if already present).
   - Multi-threaded compilation pool compiling 74 C source files in parallel.
   - Successful build and link of native 64-bit Linux binary (`umk3-game`) with `-DUMK3_LOW32`, `-no-pie`, and low-pointer memory mapping.
   - Verified native execution on Linux: `UMK3_SHOT=10 ./umk3-game` executed cleanly, producing valid PPM frame capture (`851x1413`).

3. **Packaging Configuration & Release**:
   - Linux: `.tar.gz` portable archive and `.AppImage` standalone executable via `electron-builder`.
   - Windows: `.zip` archive and portable executable (`UMK3-Launcher-Windows.exe`).
   - Packaged and published GitHub Release `v0.2.1` with all 4 release binaries attached.
   - Removed legacy `build_game.ps1` and `setup_res.ps1`.

4. **Full English Localization**:
   - All build stages, progress bars, compiler messages, dialog boxes, and update alerts localized to English.
   - Default UI language set to English (`EN`) while keeping the multi-language toggle for Spanish.

5. **Linux LP64 Memory Model Compatibility & Freeze Resolution**:
   - Diagnosed runtime game freeze during character mesh loading: `sizeof(long)` is 4 bytes on ARMv7 iOS and Win32/Win64 (LLP64), but 8 bytes on Linux x86_64 (LP64).
   - In `Players.c:1918`, `meshbaseFile` pointer arithmetic `base + 1` and `hdr[0]` doubled offsets, causing `meshCount` to read as 4.5 quintillion and triggering an infinite loop on `LAC: meshbase out of ram`.
   - In accordance with the project rule to never modify the 1:1 decompiled C files in `src/decomp/`, the fix was implemented in the runtime layer: `src/runtime/savestate.h` includes standard/POSIX and GL headers before defining `#define long int` for state files under Linux 64-bit builds, with `compile.js` passing `-DUMK3_NO_LONG_REMAP` for `text.c`.
   - Verified in GDB and runtime: Kano (`who=0`, `meshCount=366`) and Sonya (`who=1`) loaded successfully and the game loop proceeded into active gameplay.
   - Launcher UI dynamically detects platform (`window.umk3.platform`): displaying `LINUX` with `64 BITS (UMK3-GAME)` on Linux hosts, and targeting the native `umk3-game` executable.

---

## Verification Suites

1. **Static & Structural Checks (`bash tools/decomp/check.sh`)**
   - Compiles all `src/decomp/` and `src/runtime/` units with `-Wall -Wextra -O2`.
   - Zero errors across the entire tree.
2. **Banned Words & Clean-Room Audit (`python3 tools/decomp/check_banned_words.py`)**
   - Passes with 0 errors.
3. **Launcher Unit Tests (`node --test test/*.js` in `launcher/app/`)**
   - 15 unit tests passing covering configuration, ZIP reader, Mach-O parser, globals transformation, and launcher state.
