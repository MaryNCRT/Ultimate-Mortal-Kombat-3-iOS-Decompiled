# Project Status & Progress

**Last updated:** 2026-11-19 (release 0.5.4 beta: online -- one stage on both sides, no softlock at a round's end, no desync in a session's second fight, less stutter, no frozen fight; the Steam Deck's 6 buttons on Linux; pads from SDL_GameControllerDB; KOMBO SLIDE OFF with kicks) · Release history: [CHANGELOG.md](CHANGELOG.md) · How to pick up the work: [HANDOFF.md](HANDOFF.md) · How the game is built: [HOW-THE-GAME-WORKS.md](HOW-THE-GAME-WORKS.md)

This file has two parts. **Current state** says where everything stands
today, for someone who knows nothing of the project. **Detailed log** is
the record written as the work was done, newest first: every finding with
its binary addresses and the checks that were run. A later entry can
correct an earlier one; the current state is always the summary to trust.

---

## Current state

### The decompilation

| Area | Functions | Transcribed (`src/decomp/`) | Checked by |
|---|---:|---:|---|
| **LIME 3D engine** (`src/decomp/lime/`) | 109 | 109 (100%) | differential tests against the recompiled oracle (`tests/test_*_diff.c`) |
| **Front end & scenes** (`src/decomp/gamecode/`) | 291 | 291 (100%) | running natively (51 front-end screens, HUD, audio, blood) |
| **Arcade fight engine** (`src/decomp/gamecode/logic/`) | 2,172 | 2,172 (100%) | file-by-file differential tests (`tools/decomp/difftest/`), 229 data tables by `ctest` |
| **Total** | **2,572** | **2,572 (100%)** | `tools/decomp/progress.py` |

Every function was written from the armv7 disassembly of the player's own
copy of the game (`OUTPUT/armv7/UMK3.armv7`), never from other sources
([AGENTS.md](../AGENTS.md)). Findings made since are fixes to individual
transcriptions, each one checked against the binary before it went in --
the most recent: `repell_func` (2026-10-30, below).

### The PC port (`src/runtime/`)

Playable from start to end on Windows and Linux. What the port provides
around the decompiled game:

- **Renderers:** OpenGL, Vulkan, Direct3D 12 (`src/runtime/render/`; Vulkan and
  Direct3D 12 through one fixed-function layer, `rgl_ff.c`, fighters drawn
  indexed since 0.5.5), HD textures, MSAA, real-time shadows, VSYNC, pipeline
  caches.
- **Screens:** 3:2 (the iPhone's), 4:3 (the iPad build's layout), 16:9,
  21:9; the port's own menus (settings, debug, STAGE SELECT, online,
  replays) fit each.
- **Sound:** a Win32 mixer and an SDL one, MP3 music, the game's 497 WAVs.
- **Input:** keyboard and pads, remappable for fights and menus; LOW
  LATENCY (one frame at most between a tick and the screen).
- **Online:** LAN and internet through Epic Online Services, rollback
  netcode, input delay from the ping.
- **Replays:** every local fight recorded and played back with a player.
- **Smoothing:** SMOOTH ANIMATIONS and SMOOTH MODELS (Blender's code,
  `src/thirdparty/blender`, which makes the project GPL).
- **Mods:** `mods\<mod>\` overlays on the game's files, `data.ini` over the
  fight tables; online uses only models and textures from mods.
- **Debug menu** (`F2`): fighters (bosses included), stages, rounds,
  hitboxes, save states, logs.

### The launcher (`launcher/`)

Electron + SvelteKit (`launcher/app/ui`, built with `npm run ui`). It
compiles the game from the player's `.ipa` with a bundled `zig cc`, keeps
`umk3.ini`, updates itself from GitHub releases, and has the MODS section:
MOD MANAGER, MOD HUB (mods from the companion repository
MaryNCRT/UMK3-IOS-Mod-Hub, installed and updated), CHARACTER VIEWER (with REPLACE / EDIT MODEL: retargeting,
weight paint, Blender-style bones, shape keys, corrective animation
layers), STAGE VIEWER (the engine's stage playback, Blender-style element
editing with keyframes and shape keys, texture replacement), CHARACTER
EDITOR, TEXTURES and EXPORT (FBX, GLB, USD, OBJ). STEAM
([STEAM-INTEGRATION.md](STEAM-INTEGRATION.md)) adds the launcher to the
Steam library as "Ultimate Mortal Kombat 3 IOS Decomp", a non-Steam game
with SteamGridDB artwork (game 38752), and closes and reopens Steam
itself to do it.

### What the tester has and has not played

Mary plays every build in a test folder (`WORK\release-0.1.12\Test` on her
machine) before it is released, and reports by GitHub issues, messages and
logs; the [CHANGELOG](CHANGELOG.md) says for each release whether she has
played it. 0.5.4's online fixes were played by scripts with two games on one
PC (no second machine yet); she watched those windows. Not on a Steam Deck
or with a non-Xbox pad yet. In 0.5.5's test folder she saw the dynamic music
and Scorpion's fatality without its lag.

### Open problems

- **PHOTO MODE: a fighter behind the other drawn over it** (Mary, 2026-11-22, a
  screenshot): with the free camera behind the fighters, Sindel, further
  from the camera, shows over Scorpion. The fighters' draw order or depth
  test, not looked at yet (GameCode.c RenderLevelPlayers).
- **PHOTO MODE: no camera up / down** (Mary): the camera has height (Q / E)
  but no movement straight up or down in the free camera's own frame; to check
  what she means and add it.
- **Training: the input column shows no sideways movement** (Mary,
  2026-11-22, a screenshot): only the up and down arrows appear, with their
  frame counts; left and right are not listed. Not investigated yet; the
  column is the port's `PortTrainInput` / `PortTrainDrawInputs`.
- **DLSS 5, the neural pass:** seen working on 2026-11-22 with the
  DLSS5 ReShade AIO add-on (standalone-dlssnr 2.2.4, Apache-2.0) in place of
  the feeder: "CreateFeature(feature=18) = Success", NR=Success every frame,
  60 fps, NR 5.4 ms + DLSS SR 2.0 ms of GPU, in a 1600x900 window. It does
  not start in the borderless 1920x1081 window (it waits "for stability"
  forever). Mary saw it work and reports a strange flicker. Still to do:
  the HUD and menus left untouched (only the 3D fight processed), and a
  separate layer for the fighters.
Closed problems are in the detailed log and the CHANGELOG, not here.

| Problem | State |
|---|---|
| Online between two machines | 0.5.4's fixes (one stage, the round's end, the second fight, holds) checked with two games on one PC over a bad line made on purpose (`UMK3_NET_LOSS/LAG/JITTER`): 0 desyncs; **waits for a match between two PCs** |
| Steam Deck: the fight's 5-button layout (issues #171, #253) | **fixed in 0.5.4** (Linux read `long` game globals as 8 bytes in the port's files, `umk3_long`); waits for a Deck |
| Pads through SDL_GameControllerDB (WinMM on Windows, SDL on Linux), player two's pad on Linux | **built in 0.5.4**, `tests/pad_db_test.c` passes; waits for such a pad |
| KOMBO SLIDE OFF not enough (issue #252) | **changed in 0.5.4** (kicks too; VS RULES no longer override OFF); checked by script in training, waits for the reporter |
| MOD HUB never connecting on other players' PCs, no error | **fixed in 0.5.3** (time limits, other sources for the index); waits for an affected PC |
| Dynamic stage music (critical / ending stretches; the music follows a mid-fight stage change) | **in 0.5.5** (`adaptive_music.c`); Mary saw it working in the test folder; the stretches per tune (`music.ini`) wait for her ear |
| Scorpion's fatality lag (and finishers with many bodies) | **fixed in 0.5.5** on Direct3D 12 (indexed fighters, the scene's passes on every core): Mary saw the lag gone in the test folder. **Vulkan**: the CPU side 45 -> 14 ms, but the frames still wait ~22 ms on the GPU there; open |
| A compile on another PC ending "EXIT CODE: -1" after omp_lite.c (an old version compiled there before) | cause not known (the failed file's error was lost among the others' lines); 0.5.5 names it, keeps `build\last-build.log`, cleans another version's objects and has REPAIR. Waits for that player's log |
| SEGA DREAMCAST button icons | **in 0.5.5**; the game's pictures not yet seen with a pad connected (no pad on the test machine) |
| `tests/rgl_test.c`: 5 shadow checks fail on Vulkan and Direct3D 12 | the expected values predate ENHANCED LIGHTING's shading (the same 5 fail on the code before 0.5.5); the test needs new values |
| Screen-space contact shadows (ENHANCED LIGHTING) | research only: [research/CONTACT-SHADOWS.md](research/CONTACT-SHADOWS.md) (ambient occlusion is built) |
| A pad's X glyph over START when joining online (Mary, once) | not reproduced |
| THE STREET and other stages pause up to ~1.3 s on their first frames | open: their events load then |

### Where to look next

The [HANDOFF](HANDOFF.md) lists how to build, test, verify and release.
New work comes from Mary's reports (GitHub issues and her logs); each is
traced to the binary's own mechanism before anything is changed.

**Graphics overhaul (asked 2026-11-22):** phase 1, the audit and the
architecture, is in [research/GRAPHICS-OVERHAUL.md](research/GRAPHICS-OVERHAUL.md).
Two licence questions block phases 3-5 as asked (NVIDIA SDKs inside a GPL
program; NVIDIA's neural model is not redistributable) and wait on Mary.
Phase 2 (GRAPHICS QUALITY: presets, bloom, tonemap, contact shadows) is
built and checked in Direct3D 12 only; see the log entry below. Phase 3
(ray tracing) has not started.

---


## Detailed log (newest first)

### 2026-11-26 -- RTX materials, cheaper RTX, CPU/GPU overlap; reflections parked

- **FPS (Mary: "fps bajísimos")**: measured in Mary's own Test folder (HD pack, AA x8, DLSS 5): RTX now 17.1-17.2 ms a frame (~58 fps, ~2 late/s), the same as RTX off. Cuts: sun 1 ray, each light 1 jittered ray and only where it reaches (att > 0.06), ambient/bounce 2 rays. D3D12 present no longer waits for the GPU: the frame runs while the CPU makes the next (`settle()` before anything reuses the frame's memory: next list, upload, release, read); `UMK3_D3D12_SERIAL=1` = old way. Not separately benchmarked against serial yet.
- **Lamp globes showing polygons (Mary)**: a light's own lamp (within 2.5x the light's sphere) never shadows it; ray offsets 0.05-0.06.
- **Materials (Mary: specular and roughness maps)**: derived per pixel from each texture (no maps in the game): specular from brightness/greyness, roughness from grain (fwidth); GGX highlights for the ENHANCED LIGHTING lights (UE4-style); stage floor wet in a slow world-space noise + dark cracks; toned down after "brilla demasiado". Only active where the floor is known, i.e. with reflections on -- **so currently mostly inactive**; to be decoupled.
- **Reflections (planar, rgl_ff.c refl_pass)**: mirrored scene into a quarter-res target, triangle-level floor clip (the stage draws floor and walls together: the mirrored floor hid everything), floor height = commonest height of upward stage faces near the fighters' feet. Works (target dumped with UMK3_REFL_DUMP), but costs ~25 ms a frame in Mary's HD install for only ~36 draws/15k vertices -- a wait somewhere, not found yet. **Off unless UMK3_REFL=1.** Diagnostics: UMK3_LOG_REFL, UMK3_REFL_DUMP=<frame>, UMK3_REFL_NOCULL, UMK3_REFL_FLAT.
- Queued from Mary: floor with more relief/normal maps and a wet look; training mode left/right arrows not shown; photo mode: save created presets; photo mode camera up/down; fighter drawn over the other in photo mode.

### 2026-11-22 -- the lag: frame pacing; DLSS 5 cuts fixed; RTX soft light shadows

- **Lag (Mary: "bajos fps", "lag")**: steady state was already 60 fps; the stutter was the low-latency pacer (game_main.c pace_frame). It measured the frame's work from the CPU part only, so with RTX/DLSS 5 (5-13 ms GPU + Present inside the swap) it slept too long, missed the refresh, grew its margin 1 ms and decayed back: a 33 ms frame every second or so. Now the backend reports `rgl_present_work_ms` (D3D12: commands run + Present + GPU wait, not the refresh wait) and the pacer counts it; cap 14 ms. Measured DLSS 5 + RTX, 180 Hz screen: between swaps 16.7-17.0 ms steady (UMK3_LOG_PACE). Found with the sampling profiler: slow frames were the main thread parked in a wait (ntdll), not working. Profiler now names other modules and takes `UMK3_PROFILE_MS=<threshold>`.
- **DLSS 5 off/glitching a second into a round (Mary)**: the claude/dlss5-cuts history reset (now merged here) also fired on any camera turn > 20 degrees a frame, and the round intro's sweep did that every frame. Cuts now only on a fight entered/left or the 3D scene back after >= 10 frames away, at most one per 120 frames. Not yet confirmed by Mary.
- **RTX torch cut (Mary: the flame's shadow edge on the wall)**: light shadows traced to a sphere of a tenth of the light's reach (3 rays), stopped short of it -- soft edges, and a light is no longer cut by its own torch bowl/pool. Not yet seen by Mary.
- Still open: DLSS 5's first model creation stalls Present ~5 s (NGX feature creation, once a run, in menus after the AIO's 15 s settle); fight load 1.4-1.5 s, menus 0.9 s. Wet-floor reflections for Mary's reference look not started.

### 2026-11-22 -- RTX phase 3: ray-traced lighting (D3D12, DXR 1.1 inline ray queries)

- `rt=1` (GRAPHICS > RTX: RAY-TRACED SHADOWS, next start; needs ENHANCED LIGHTING, Direct3D 12, DXR 1.1). The casters the shadow map already gathers (rgl_ff.c, stage group 0, fighters 1..15) are committed each frame to `rt_scene` (rgl_d3d12.c): one BLAS per group + a TLAS, two sets alternating, built at the end of the frame's list; bound as root SRV t4.
- Shaders: ff.hlsl `UMK3_RT` variant compiled with dxc (vs/ps 6.5, DXIL, both stages since a PSO cannot mix DXBC/DXIL); `shaders/build.py` takes dxc as a third argument. Sun direction rides in the light block's spare words (`lb_count.yzw`), 0 = RTX off -> falls back to the shadow map.
- What it traces: the sun's shadows (2 rays, soft cone), every ENHANCED LIGHTING light's shadows, ambient occlusion (3 hemisphere rays, replaces HBAO when on) and one bounce of the lights' colour off what the rays hit (constant albedo; no material lookup yet). A fighter's own body is skipped (coarse proxy mesh, a frame old).
- Measured RTX 5070, 1080p, Kano vs Sonya: GPU 5.3 ms vs 3.7 ms without; frame time 23.4 vs 22.5 ms. Note: ~23 ms a frame (~43 fps) **without RTX too** -- the general lag Mary reports predates RTX; still to investigate, with long loading screens.
- Mary (2026-11-22): first version "no se ve un cambio" -- with a near-overhead sun only small contact shadows change. Lights/occlusion/bounce added; her reference captures (Pit stage, street stage) need **wet reflective floors** (reflections of lamps/glow/fighters), stronger coloured light and bloom -- next step: reflections. Not yet seen by Mary in game.
- Deployed to WORK\release-0.1.12\Test with rt=1.

### 2026-11-22 (afternoon) -- DLSS 5: the interface a layer over everything

Mary saw no neural effect with the ControlMask layers. AIO's lab notes say
a supplied ControlMask overrides NVIDIA's automatic mask, and that the right
way to keep the interface out is NVIDIA's UI correction (DLSSNR.UI, whose
alpha marks protected pixels, and DLSSNR.Backbuffer, the pixels put back).
AIO leaves it off, so we build AIO ourselves.
- `tools/neural/build_aio.py`: AIO v2.2.4 with its DLSS5-Feeder submodule,
  NVIDIA's public DLSS SDK headers and `nvsdk_ngx_d.lib` at a fixed commit
  (never in this repository), the Vulkan headers from ours, then
  `tools/neural/patch_aio.py`, built through a junction (cmd.exe will not
  run a batch file through "MK3 PROJECT"). `stage_dlss5.py` ships this
  add-on instead of the official one.
- patch_aio.py, with `[Standalone.DLSSNR] Umk3UiProtect=1`: UICorrection=1,
  with the input as DLSSNR.UI and DLSSNR.Backbuffer. The proxy compositor
  reads the original in RGBA and, in neural mode, draws the original pixel
  where its alpha is over 0.5, after NR and DLAA. The UI correction alone
  still left the HUD's letters damaged, by DLAA's history.
- The game: the interface mask is inverted (rgl_ff.c: interface 1, the
  fight's scene 0; a cleared picture 1). `rgl_ff_layer_fix` runs before each
  present: a full-screen draw writing alpha alone, 1 wherever the interface
  touched the 3x3 about a pixel (shader mode RGL_LAYER_FIX), so the soft
  edges of letters are covered. Measured: interface 5-9 %, scene 91-95 %,
  "between" 0 %. AIO's settings: no VORT guides, no ControlMask, NVIDIA
  optical flow (the setup Mary saw working).
- Mary, in her test folder: the neural effect on the stage and fighters,
  the HUD, texts, photo mode panel and menus intact.

### 2026-11-22 (midday) -- DLSS 5: menus left out, the 17 s exit, the preparation said

- Mary: "everything that is a menu must stay out of DLSS 5". The layers are
  on by default again (`dlss5_layers=1`). Outside a fight (`InGame` 0,
  passed to the renderer as `rgl_in_fight` from the frame loop) every draw
  is layer 0, the select screen's 3D fighters included. Measured with
  `UMK3_LAYER_PROBE`: menus 100 % layer 0; a fight 91 % stage, 4 % fighters,
  5 % HUD.
- Mary saw no neural effect with the layers. Two leads were followed. AIO's
  lab notes say a supplied ControlMask overrides NVIDIA's automatic mask.
  More concretely: in the guide path the neural pass got ReShade's depth,
  which with the game's MSAA is a wrong, nearly empty buffer, while without
  guides it gets AIO's fallback (flat, 1.0). Our DLSS5_AIO_Feed.fx now
  gives the same flat depth and no depth edges, so the mask is the only
  difference left. Not yet confirmed by Mary.
- The game froze about 17 s when closed with DLSS 5. Timed: `plat_close` took
  17,179 ms (the D3D12 device released with the AIO add-on hooked), against
  139 ms without DLSS 5, and the add-on's own unloading 5 s more. With DLSS 5
  loaded, main now ends after its own shutdown work, with `_cexit` (atexit:
  the replay being written) and then `TerminateProcess`, without
  `plat_close`. Closing now takes 0.33 s, and the replay is still saved.
- The other freeze: about 15 s after start, AIO makes NVIDIA's model inside
  a Present, 3.6-4.3 s with no new frame (`d3d12 slow present`). It cannot
  be shortened from the game. `dlss5_notice` (game_main.c) shows "DLSS 5:
  PREPARING NVIDIA'S NEURAL MODEL" from 11 s, so the held frame says so, and
  takes it down once a frame has taken over a second (logged: "the neural
  model was prepared (3.8 s without a frame, 19 s after start)").
- Seen, not understood: a run with no arguments, nobody at the keys, opened
  and closed the settings menu by itself and the game then closed.

### 2026-11-22 (late morning) -- DLSS 5 on AIO: the neural pass in the game, layers, presets

Mary chose AIO (DLSS5 ReShade AIO 2.2.4, Apache-2.0) as the shipped chain.
- Staging: AIO's official 64-bit zip (SHA-256 as published), our
  `launcher/neural/DLSS5_AIO_Feed.fx`, VORT (vort_Shaders b410b9f, MIT, with
  its Textures/), ReShade.fxh. The script stages aside and swaps the folder
  in only at the end; a network timeout had emptied launcher/dlss5 halfway.
- `neural_mod.c`: AIO's files are checked, the feeder era's are set aside
  (migrate), and the AIO settings the game owns are set at every start
  (set_key). There are presets (`dlss5_preset=`: Model 1..3 = DLSSNR.Style
  0..2, intensity, local tone and structure) and layers (`dlss5_layers=`,
  experimental, off; `dlss5_stage=` / `dlss5_fighters=`).
- Borderless no longer adds its extra row with DLSS 5 on D3D12
  (win32_gl.c borderless_row): 1920x1081 on a 1080 monitor made AIO refuse
  ("render resolution exceeds native output"). After the change: "contract
  ready", NR and DLAA at 1920x1080.
- Layers: rgl.h `rgl_frag.layer` / `rgl_layers`; rgl_ff.c submit; rgl_d3d12.c
  writes the alpha through the blend factor; ff.hlsl gives opaque draws
  alpha 1. Measured with `UMK3_LAYER_PROBE=1` in a fight: HUD 5 %, fighters
  5 %, stage 90 %.
- Our ReShade sends `reshade_reloaded_effects` again once the effects have
  loaded, after `is_loading()` is false. Before that, AIO resolved its
  handles too early and they stayed MISSING. After: "VORT=found ...
  nr_mask=found", `NR-control-mask=BOUND`.
- `lib/build.js`: DLSS 5's copy also takes newer files of the same size,
  since the patched ReShade64.dll is byte-for-byte the same size as before.
- Mary saw no neural effect with the layers on, and asked to return to the
  whole-picture setup, which is now the default. Screen captures taken here
  (DLSS 5 off / whole picture / layers) showed the pass on the stage with
  the HUD clean in the layers capture. That is to settle with her.
- Seen and not yet understood: two runs with no arguments closed by
  themselves after the settings menu opened and closed (once in each of the
  two folders); not a crash ("clean process shutdown").

### 2026-11-22 (later) -- DLSS 5 built in, not a mod

Mary: "it no longer needs to be a mod; put DLSS 5 in an update".
- `neural_mod.c`: `dlss5=` (GRAPHICS > DLSS 5) loads the chain from
  `<game>\graphics\dlss5`. A type=dlss5 mod still works when it is off.
  `check()` returns NEURAL_* (neural_mod.h); `neural_dlss5_status()` asks
  quietly for the menu, and `umk3-game --dlss5-status` asks for the launcher.
- `settings_menu.c`: a DLSS 5 row at the end of GRAPHICS, with the reason
  under it. The check runs about once a second, not on every layout.
- Launcher: `lib/dlss5.js` (status, ADD NVIDIA FILES: Authenticode through
  PowerShell's Get-AuthenticodeSignature, one consumer, `.old` for anything
  replaced), IPC `dlss5:*`, a DLSS 5 block in VIDEO > GRAPHICS, `dlss5=` in
  `lib/ini.js`, `test/dlss5.test.js`. `node --test test/*.js`: 41 pass,
  0 fail.
- Shipping: `tools/neural/stage_dlss5.py` builds ReShade, takes the feeder's
  official zip (SHA-256 matches the published fa41fe2c...62584) and
  ReShade.fxh (pinned commit, SHA-256), and stages them into `launcher/dlss5`
  (git-ignored). `make_release.py copy_dlss5` packs them, refuses without
  them (UMK3_NO_DLSS5=1) and refuses NVIDIA's files. `lib/build.js` copies
  `launcher/dlss5` to `graphics/dlss5` at every compile, copy-only, so the
  player's files survive updates.
- Antialiasing: Mary asked that DLSS 5 work with her antialiasing, so the
  MSAA-off from the morning is gone. With MSAA the feeder warns that it
  cannot read the multisampled depth and works without it.

Checked in `WORK\dlss5-test` (muted, D3D12, RTX 5070): the compile put the
7 files in graphics\dlss5; `--dlss5-status` printed "6 needs NVIDIA's
files 0"; with dlss5=1 and UMK3_DLSS5_PROBE=1 ReShade loaded from
graphics\dlss5, the feeder found its technique and opened the same-device
session, and MSAA x8 was kept. The NVIDIA driver (621.24) has no
nvngx_dlss.dll or nvngx_dlssnr.dll (DriverStore has only nvngx_dlssg and
ngx-builtins
vngx_dlssd). **Not checked:** the neural pass, the GRAPHICS row
seen on screen, and the launcher's panel seen in the app.

### 2026-11-22 (morning) -- the DLSS 5 mod in the game: our ReShade, the feed

Route A, as Mary chose: ReShade 6.8.0 built here from its source
(`tools/neural/build_reshade.py`: the v6.8.0 tag with its submodules, MSBuild
Release|64-bit = RESHADE_ADDON=2, the banner never drawn), and the
DLSS5-Feeder 2.0.0 that Mary put in `D:\MK3 PROJECT\DLSS5`. ReShade.fxh comes
from crosire/reshade-shaders' slim branch (CC0). Mary's source zip of ReShade
6.8.0 matched the tag's `source/` exactly (diff -rq).

Run in `WORK\dlss5-test` (muted, D3D12, RTX 5070, driver 621.24,
`UMK3_DLSS5_PROBE=1`). ReShade registered its hooks, installed the delayed
d3d12 hooks when rgl_d3d12.c loaded d3d12.dll, redirected D3D12CreateDevice,
loaded the add-on from the mod's folder ("Registered add-on DLSS 5 Feed
2.0.0 ... API version 20") and made its runtime from the mod's ReShade.ini.
DLSS5_Feed.fx compiled in 9.7 s the first time, 24.7 s after a definition
changed, and 0.07 s from the cache. The feeder found its technique and
textures, opened a same-device D3D12 session on the game's device and
initialised NGX (NVSDK_NGX_D3D12_Init Success). It looked for
nvngx_dlss.dll / nvngx_dlssnr.dll **in the mod's folder** (so
SetDllDirectory is not even needed for it), and stopped there because they
are absent. The picture is unchanged, which is what Mary saw.

Fixed from what the run showed:
- The game's 8x MSAA depth could not be read ("Generic Depth ... cannot read
  multisampled depth"). game_main.c render_init / render_reconfig now draw
  single-sample while the mod is loaded, and umk3.ini keeps the player's
  value. Then: "1 sample(s) ... the right size".
- The feeder took the depth as reversed (ReShade.fxh's default). The mod's
  ReShade.ini now sets RESHADE_DEPTH_INPUT_IS_REVERSED=0, and the log says
  "depth reversed=0".
- ReShade's keys are unbound in the written ReShade.ini, with no clock or FPS.
- `dlss5-feed-helper.addon64` (32-bit mode) in the folder is refused, because
  ReShade loads every .addon64 it finds.

**Not done:** the DLSS and neural pass, which needs NVIDIA's two DLLs and a
consumer add-on. The HUD is still processed with the scene.

### 2026-11-22 (later) -- graphics overhaul, phase 2: GRAPHICS QUALITY

What was built (all port code):

- `src/runtime/gfx_preset.c/.h`: `graphics_preset=original|enhanced|custom`.
  A preset sets the existing switches (`rgl_shadows`, `rgl_ao`, the new
  `rgl_contact_shadows`, `gfx_bloom`, `gfx_tonemap`); the preset shown is
  the one they match, CUSTOM otherwise. `read_config` applies the key only
  when the file gives none of the switches. RTX / RTX_DLSS5 are reserved
  values, not selectable.
- Play-time BLOOM and TONEMAPPING: `gfx_scene_post`, called from
  GameCode.c Task_GameMain_frame (a port line) after the 3D scene and
  before `DrawHUD`, runs PHOTO MODE's pass (`rgl_post_apply`). So the HUD
  is never processed. It does not run while PHOTO MODE is up (photo_post
  does then) or outside fights.
- The tonemap: a slot of the pass's settings (l1.pos w), the same in
  `ff.frag`, `ff.hlsl` and `rgl_post.c`. A first ACES curve with a
  mid-grey anchor crushed the dark stages to black (seen in the first
  D3D12 shot of THE PIT 3). It is now a shoulder: below 0.6 linear nothing
  changes; 0.6..1.4 eases into 1; the hue kept.
- CONTACT SHADOWS: `contact_at` in both shaders. 12 steps towards the
  shadow map's light, through the blurred occlusion map's distances (G, B).
  The light's direction in camera space comes from `ao_block` (rgl_ff.c,
  `SH.sun` from `shadow_build`) in `aodbg.yzw`. Needs ENHANCED LIGHTING and
  AMBIENT OCCLUSION. Mary saw a stipple on Scorpion's arms and chest with
  the first version: the march hit the fighter's own curved surface in the
  half-size map, and the per-pixel jitter made it dots. Now the fighters do
  not receive contact shadows, there is no jitter, the map's own texels
  about the start are skipped, and the bias is larger. The shot after the
  fix is clean.
- Settings menu: GRAPHICS QUALITY, CONTACT SHADOWS, BLOOM, TONEMAPPING;
  saved to umk3.ini. Launcher: `lib/ini.js` keys plus
  `graphicsPreset`/`setGraphicsPreset` (the first-start presets set them
  too), the GRAPHICS page's selectors, and a test in `test/ini.test.js`.
- `tests/rgl_test.c` did not link since PHOTO MODE (no `plat_gl_proc`); it
  now does. It was compiled, not run.

Checked: built with the launcher's CLI into a private folder
(`WORK\phase2-test`, because the shared test folder was in use by the
DLSS 5 session at the same time). Scripted D3D12 fights (muted) on stages
0, 3 and 7, ORIGINAL / lighting only / ENHANCED. The launcher's tests pass
(36, 2 skipped), and the word check passes.

Not checked: the Vulkan and OpenGL shots and the frame-time runs (Mary
stopped the tests), `rgl_test` on the three renderers, and the menus by hand.
Mary noted that the ENHANCED shots look close to the lighting-only ones:
bloom at 30% and a shoulder-only tonemap are subtle on these dark stages.
### 2026-11-22 (night) -- the DLSS 5 mod (game side)

Asked by Mary: DLSS 5 usable as a mod. `src/runtime/neural_mod.c` (new)
and `mods.c`: a mod with `type=dlss5` is not indexed as game files.
`mods_find_typed` finds the first such mod that is on, before mods_init.
Before the Direct3D 12 device is made (`platform/win32_gl.c`, just before
`rgl_open`), the game checks Direct3D 12, an NVIDIA adapter (DXGI vendor
0x10DE), that no ReShade of the player's own is beside the exe, and that
these files exist in the mod's folder: ReShade64.dll, dlss5-feed.addon64,
DLSS5_Feed.fx, ReShade.fxh, a consumer add-on, nvngx_dlss.dll and
nvngx_dlssnr.dll. The two NVIDIA DLLs must say NVIDIA as their company and
pass WinVerifyTrust (wintrust.dll and version.dll are loaded at run time, so
nothing new is linked). Only then does it set RESHADE_BASE_PATH_OVERRIDE and
SetDllDirectory to the mod's folder and LoadLibrary ReShade64.dll from it.
ReShade.ini and DLSS5-preset.ini are written only when missing. umk3.ini
`dlss5_mod=0` turns it off. These choices come from reading ReShade's
dll_main.cpp / hook_manager.cpp / addon_manager.cpp: a ReShade loaded under
its own name hooks the system d3d12/dxgi exports, and its base path comes
from that environment variable.

Checked with scripted runs in a separate folder (`WORK\dlss5-test`, muted,
D3D12, RTX 5070, driver 621.24): no mod (nothing logged); an empty mod (each
missing file named, the game runs); a Microsoft DLL named nvngx_dlssnr.dll
(refused: "not NVIDIA's") and nvapi64.dll as nvngx_dlss.dll (accepted as
signed NVIDIA, which tests only the check itself); `off=` in mods.ini
(nothing); renderer=opengl ("needs Direct3D 12"); dlss5_mod=0 (nothing).
**Not done:** a run with the real ReShade, feeder, consumer and NVIDIA DLLs.
None are on this machine. Whether the feeder and consumer find NVIDIA's DLLs
through SetDllDirectory, rather than looking only beside the exe, is
unverified. The HUD is processed with the scene.

### 2026-11-22 (later) -- graphics overhaul, phase 1: audit and architecture

Research only, nothing in `src/` changed. Audited the three renderers,
ENHANCED LIGHTING, AO, the photo-mode post pass, the config, the launcher's
GPU listing and VERIFY / REPAIR, photo mode and replay seeking; read
Melee Unlocked, DLSS5-Feeder (+ fork, AutoBuilder), LumeniteFX, vort,
ReShade and NVIDIA's Streamline / DLSS / RTXGI / RTXDI / NRD / RTXPT /
RTX-Kit. Findings and the plan: `docs/research/GRAPHICS-OVERHAUL.md`.
The main points: the renderer already tags the scene camera, the HUD's
start, each fighter (caster id) and each stage mesh by name, so the
character / environment / HUD split can use real data; the D3D12 backend
(not GL interop) is the route for the neural layer; LumeniteFX's licence
forbids bundling it, and real motion vectors from `rgl_ff` make it
unnecessary; the NVIDIA RTX SDKs licence (§4(e)) conflicts with linking
into this GPL-2.0-or-later tree, so RT is planned in-house with inline ray
queries; `nvngx_dlssnr.dll` must be supplied by the player, never shipped
or fetched.

### 2026-11-22 -- 0.6.0 beta: PHOTO MODE phases 3 and 4

Port. CHARACTER: the fighter's adjustment applied where his matrices are
made -- the body's (+0x548, built from the lime matrix in RenderLevelPlayers:
rows times Rz after the scale, then the offset) and the pose's (+0x588,
RenderLevelPlayers_Pose: glTranslatef / glRotatef after his place); the
first try turned only the pose's, which draws the events, not the body.
Hidden: PortSkipFighter for his side. SCENE: RenderLevelBG and the events and
particles left out; AVANZAR UN CUADRO lets one tick of the fight run
(GamePaused 0 for it, `photo_on` still true), the first step keeping the
frozen frame in a save slot of its own (savestate.c `st_photo_save`), put
back on the way out. Phase 4: `photo_capture_size` asks clear_frame for a
buffer of 1440p, 4K or twice the size for the photo's frame alone (at most
4 samples), the photo read from it, supersampling brought down 2x2 in the
writer's thread; presets; `photo_mode.ini`. The hints with the keys' and
the pad's pictures (FrontEnd.c PortNavDrawLineC; the glyph rings made 16
deep after a line of ten keys showed the wrong ones).

Checked by scripted runs in the test folder (muted, Direct3D 12): a fighter
turned 105 degrees, the other hidden, the stage hidden, three frames stepped
and the frame put back ("the fight put back on the frame it was frozen
on"); a 4K photo (3844x2160) and a supersampled one (3840x2158 to
1920x1079); the settings written and read back. Mary reported the camera
after the intros as fixed before it was looked into; nothing changed there.

### 2026-11-21 (night) -- PHOTO MODE phase 2: IMAGE and EFFECTS in the three renderers

Port. The effects are one pass over the picture (docs/PHOTO-MODE.md, "The
effects"): `RGL_POST` in ff.frag / ff.hlsl (`post_fx`, shaders rebuilt with
build.py), `rgl_ff_post` for Vulkan and Direct3D 12, `render/rgl_post.c`
(GLSL 1.20, its own framebuffer) for OpenGL, `photo_post` calling it before
the panel. The depth of field: AMBIENT OCCLUSION's depth pass made to run
for it too (`rgl_post_want_depth`, ao_note / ao_passes) and its target
copied, bound as the shadow map -- the backends bound the shadow map only
for a receiver, so the first try blurred nothing (the depth read white);
rgl_vk.c and rgl_d3d12.c bind it for `RGL_POST` as well. OpenGL blits the
depth beside the colour.

Checked by scripted runs in the test folder (muted): EXPOSURE, vignette,
chromatic aberration and SEPIA in Direct3D 12 with a photo written (the
PNG has the effects); the depth of field focused on player one, an A/B
against no effects (the wall behind him blurred, he sharp); the same keys in
Vulkan and OpenGL, the two pictures side by side alike. The test folder's
renderer put back to d3d12 after. Not checked: a machine whose OpenGL has no
GLSL (the panel says NOT AVAILABLE there), Linux.

### 2026-11-21 (later) -- 0.5.9 beta: PHOTO MODE, EXTRAS > REPLAYS and MODS, the replay player redone

All port, nothing of the binary changed in what it does. Checked by scripted
runs in the test folder (muted; screenshots of every page and state), and by
Mary trying each piece while it was built.

- **PHOTO MODE** (`src/runtime/photo_mode.c`, docs/PHOTO-MODE.md). The
  freeze is the game's own pause: Task_GameMain_frame skips
  UpdateArcadeCode while GamePaused is not 0 and still draws the scene;
  AnimateBG and LIME_UpdateEvents are inside UpdateArcadeCode and the
  particles read GamePaused (Particles.c). PHOTO MODE is GamePaused 3; the
  frame's FrameCount / GameCounter and the stage fatality's camera pull are
  held while it is up, so the fight goes on from the same frame. The camera:
  SetToUseCamera computes the fight's as before (LastEye/LastAt kept) and
  hands LIMEDS_SetCameraOrientation PHOTO MODE's instead; the zoom scales the
  projection's x and y (LIMEDS_Set3dMode, `PortProjScale`, set only around
  RenderGameView). The HUD and the port's windows are left out
  (`photo_hide_hud`); the panel is drawn last in the fight frame. A photo is
  the next frame drawn clean, read once presented (the render buffer when
  there is one, else the window; glReadPixels, Vulkan / D3D12 through
  rgl_ff.c) and written as PNG by a thread (RGB, per-row filters, deflate
  with fixed codes and a hash-chain LZ77 -- the repository only read PNGs).
  Inputs: PK_PHOTO and PK_PH_* (platform.h; F3 by default), pad_photo=
  (L3+R3), the sticks as they are (PLAT_PAD.ax, XInput / WinMM / SDL), the
  right and middle mouse buttons (plat_mouse_buttons). In a fight with debug
  keys PHOTO MODE comes first where the shortcut is the same.
- **The pause menu** (GameCode.c): PAUSE_SCALE 0.8 / 0.9, width 226, rows
  laid out by `PortPauseRowY` in reading order (RESUME, moves list,
  training's rows, PHOTO MODE, settings, EXIT apart); `PortOptionHitBelow`
  (FrontEnd.c DrawOptionAsButton) makes a press take one row where rows are
  closer than 40.
- **EXTRAS > REPLAYS** (FE task 55): rollback_probe.c `rp_menu_list`,
  `rp_menu_play` (REPLAYS' recording off while it plays, back on after),
  `rp_menu_stop`; game_main.c `PortReplayGo` starts it as --replay does,
  `replay_leave` goes back with the main menu and EXTRAS under it.
- **EXTRAS > MODS** (FE tasks 53, 54 the second page): mods.c `mods_list`
  (mods.ini's order, then the folders it does not name; each mod.ini's name,
  author, version, category, description, essential, type) and `mods_set`
  (mods.ini written as the launcher writes it; an essential=1 mod refused);
  mods_init turns an essential mod on whatever mods.ini says. The cards'
  pictures: draw_gl.c `lime_load_mod_picture` (cover.png / preview.png).
- **The launcher** (lib/mods.js, ModManager.svelte, store.svelte.js): MODS
  and TEXTURE PACKS pages (category textures), essential mods locked (on,
  not moved, not trashed, its category fixed); save() writes an essential
  mod on.
- **The replay player** (debug_menu.c `dbg_replay_draw`, `rp_layout`): the
  same player, redrawn -- the line of the fighters, the clock and the state,
  a finer timeline, the speed's pips, three groups, RPB_CAMERA / RPB_MOVES /
  RPB_CINEMA (rollback.h), bilingual tips, the keys' hints, all faded by
  `dbg_replay_alpha` (CINEMA, game_main.c: three seconds of nothing while it
  plays). The moves panel hidden: GameCode.c `PortReplayHideMoves` around
  PortTrainDraw. FREE CAMERA: photo_mode.c `photo_free_toggle` /
  `photo_free_tick`, the camera overridden while the replay plays on; the
  replay's keys, the pad's too, go on as they were.
- **VS Sounds**: byte for byte the Skversus.mp3 Essentials has; moved out of
  the test folder's mods (to mods_removed/), Essentials keeps the sound.

Not done: PHOTO MODE's effects (phase 2), its pose and character tools
(phase 3), 1440p/4K capture and saved settings (phase 4). The launcher's
REPLAYS page itself is as it was (its viewer is the game's player above).

### 2026-11-20 (later) -- 0.5.6 beta: the icon's FileNotFound, VERIFY, REPAIR only what is damaged

A player's REPAIR on 0.5.5 stopped with `icon: error: FileNotFound`. 0.5.5's
`cleanOtherVersion` removes `build/obj64` when another version compiled
there, so `umk3_game.res` -- until then never made again on an old folder --
was compiled again, and `zig rc` failed on that PC. Reproduced here only as
its missing-folder case (`zig rc` answers FileNotFound for an output folder
that does not exist); on that PC the folder existed (compile.js makes it), so
the likeliest cause is zig's first build of its resource compiler into its own
cache. Fix (`launcher/app/lib/compile.js`): the folder made first, rc run from
the .rc's folder, and a failure no longer fatal -- the .res left out of the
link, a warning in the log.

VERIFY and the new REPAIR (`launcher/app/lib/verify.js`, `build.js`
`verifyFolder` / `scanBroken`): GitHub's tree at `v<version>` lists each file's
git hash; a local file is hashed the same way (and again with LF line ends);
missing or differing files are listed, and REPAIR downloads them from
raw.githubusercontent.com and checks the hash before writing. Checked: the
test folder (592 files: the one damaged was `game_main.c`, which carries the
hint line not in 0.5.5 -- correct), a restore of two files into an empty
folder (one with a space in its path), the launcher's tests (35 pass, 2
skipped), the COMPILE YOUR .IPA screen captured with VERIFY beside REPAIR.
Not tested: a REPAIR that downloads on another PC, VERIFY offline.

The FATALITY TRAINER (Mary's next request) is begun on its own branch,
`claude/fatality-trainer`, not in this release.

### 2026-11-21 -- ONLINE stage 4c: spectators; no STAGE SELECT in a room

- **Spectators.** In a lobby's room, a member who is not in the fight on
  has WATCH THE FIGHT (FrontEnd.c, where WAIT YOUR TURN was). It asks the
  fight's host (the call's A) over EOS -- mp_net.c `K_SPEC` packets, taken
  with no session too (`mpnet_poll` reads EOS then and keeps only these
  and invites). rollback.c's "spectators" section: each fighter keeps every
  confirmed frame (both players' real inputs, `p1 | p2 << 16`, up to 2^17
  frames) and streams them to its watchers from what each acknowledged
  (`SP_IN` / `SP_ACK`, resent after 120 ms); the setup (`SP_SETUP`) carries
  the fighters, the level, both button settings and the rules text, from
  which the seed follows as on the fighters. The spectator starts the fight
  through game_main.c's --fight path as an online one (GameMode 1),
  `rb_online_gate` takes the setup, and `rb_frame` runs `watch_frame`:
  the next frame from the stream, a hold when it has not come, 8 or 32
  frames a frame unseen and unheard when far behind (joined late). It asks
  for up to a minute while the fight is at its select screen. Out: the
  bar's LEAVE button (GameCode.c `PortWatchBar`, at the bottom), B or Esc,
  the host gone 15 s, or the fight's end (`QuitAsWin` / `QuitAsLose` return
  at once for a spectator) -- back to the ONLINE screen and its room
  (`watch_leave`). A spectator has no heartbeat and `disconnected()` does
  nothing for it (the first test went to CONNECTION LOST at the round's end).
- **No STAGE SELECT in a room** (Mary: the stage is chosen in the lobby):
  debug_menu.c `eligible` is 0 for an online fight while in a lobby, and the
  room's IN THE FIGHT option is drawn when the fight is called
  (`PortRoomCallNext`), its line saying so.
- Tested with three copies on one PC (`UMK3_LOBBY_TEST`, the new
  `UMK3_LOBBY_WATCH=1` presses WATCH): the spectator's fight was the
  fighters' -- the same 56 sounds in order, Sub-Zero winning flawless on
  both -- and the stage came from the room with no STAGE SELECT. Not seen
  in a test yet with the last fixes (the copies were disturbed): LEAVE, the
  way back at the fight's end. Mary: "it seems to work". Palettes and
  skins are the defaults on the spectator (not sent yet); one spectator
  watches one fight, then WATCH again for the next.

### 2026-11-21 -- the special button lost one press in fifteen (finishers that did not happen)

- Mary: a fatality typed right sometimes does not happen, at random, in a
  fight. The special button (S, 0x400) is the one bit
  `GetReal6ButtonJoyBits` sets on the frame it goes down only, and the
  engine steps 0.9322 times a frame (UpdateArcadeCode's FrameCompensation):
  about one frame in fifteen takes no step, and an S pressed then never
  reached the engine. The iOS layouts' finishers go through S (DoSpecial).
  Now `PortSpecialCarry` (GameCode.c, a global so rollback's snapshots keep
  it) holds the press for the next step when a frame takes none, and a
  frame with two steps gives it to the first only.
- Measured with `UMK3_KEYS`, forty one-frame presses of S in a training
  fight, counted in `UMK3_LOG_P1`'s engine word: 38 of 40 before, 40 of 40
  after.

### 2026-11-20 -- 0.5.8 beta: KOMBO SLIDE gentler, the lighting through finishers, the trainer's sign

- **KOMBO SLIDE off** (players, through Mary: "a bit too aggressive ... the
  opponents don't move back at all when getting Kombo'd ... you can literally
  infinite juggle punch someone in mid air"). other.c `away_x_vel` zeroed the
  push of every hit from a kombo's second on (issues #114, #252), in the air
  too. Now, with the option off, a hit on someone in the air (aborn4's test:
  the GrObj's 0x12 against the proc's 0x40) keeps the original's push, and
  one on the ground is halved. `t_ccp3`'s slide apart at the threshold is
  still skipped. Not tested in a fight yet (no scripted kombo here).
- **ENHANCED LIGHTING off in some finishers** (Mary). `UMK3_LOG_SHADOW=1`
  (rgl_ff.c, new) logs when the shadow map comes and goes and why: during
  Scorpion's FATALITY 2 the scene held no fighter caster ("none: no
  fighter") -- the burning fighter is drawn another way -- and shadow_build
  dropped the map. It now keeps the last map's centre and size for up to
  600 frames. Checked: the log shows no drop through that fatality, and the
  captures keep the lighting.
- **The trainer's finisher sign** (Mary's replay of an Arcade fight, a
  fatality "not done as it should"). The replay could not show it: Arcade
  replays part from the recording (tick 410 there; tick 872 in a fresh
  --fight with UMK3_ARCADE) -- the ladder's state is not in the setup words,
  open. In the trainer, both of Scorpion's fatalities ended with no
  FATALITY sign: the sign is `t_bonus_count`'s (mkbonus.c, after "X WINS",
  `fatality_animations[G+0x458 - 1]` through create_fx -> event 4/22), some
  150 frames after the finisher, and the trainer left after 120. It now
  waits for the sign (two seconds of it, nine seconds at most). Scorpion's
  FATALITY 1 (the Scorpions) fades to black on purpose (event 3/9,
  ScorpionFade, `t_scorpion_hell`), as the binary does. What Mary saw in her
  fight is still not known.
- `UMK3_LOG_EVENTS=1` (Blood.c, new): every game event as AddNewGameEvents
  takes it. `UMK3_FT_FIRE` now counts its success as C does (a scripted
  finisher is no longer taken for a failed try).
- **Spectators (stage 4c), the design** (not started in code): the
  fighter who hosts keeps every confirmed input pair (both players' inputs
  for a frame, `rb_frame` once `g_remote_upto` passes it) and sends them to
  the room members who ask, over EOS P2P (`mpeos_send`, a new mp_net kind
  outside the session); the spectator starts the fight as an online one
  (GameMode 1) from the setup (fighters, level, buttons, rules, the seed is
  derived from them) and runs rollback.c in a watch mode -- both inputs from
  the stream, a frame held when the next has not arrived, no sending, no
  hashes -- leaving to the room at the fight's last frame or on B.

### 2026-11-20 (after 0.5.7) -- the trainer says no rounds and ends on its own screen

- Mary: the trainer's fight said the rounds, and its end showed the main
  menu for a moment before the trainer's screen. The way to FINISH HIM (ten
  times as fast under the black screen, `PortFTSkipping`) played the game's
  sound events, the announcer's "round one, fight" among them: Blood.c's
  sound events (type 2) are skipped while it lasts. The end left the fight
  for screen 0 and jumped to 52 thirty menu frames later (`g_screen_jump`);
  now `ft_leave` (game_main.c) leaves the fight for 52 itself, with MAIN
  MENU and EXTRAS under it on the task stack, so its BACK goes where it did.
  The pause menu's way out still goes by the main menu (the game's own).
- Checked in the Test folder (muted): a babality played (by Mary, on the pad,
  while the script ran) went from the fight to front-end task 52 with no
  task 0 between; no voice before the finishing window.

### 2026-11-20 -- 0.5.7 beta: the trainer merged; FIND BY CODE asked again; the pad's keyboard

- **FIND BY CODE** (Mary: the code search did not work). mp_net.c asked EOS
  for the code's lobby once (`mpeos_lookup`); EOS's lobby search lists a new
  lobby seconds after it is made, so a search made as the other player opened
  ONLINE found nothing, and nothing asked again -- NOT FOUND after ten
  seconds. Now `mpnet_poll` asks again every 2.5 s while the answer is "none",
  for 20 s with the internet (10 on the LAN alone). The code is kept as
  letters and digits only (`mpnet_find`; spaces, dashes, a leading "umk3:"
  dropped -- comparisons were already dash-blind, `same_code`), and a paste
  into a player-ID box drops "umk3:" too. Not tested between two machines
  here (one PC is one EOS user, and a lobby of one's own is skipped).
- **The pad's keyboard** (FrontEnd.c `OnOskKeys`): a player-ID box chosen
  with the pad (the highlight showing when it was pressed; a click hides it,
  `pad_nav_hide`) opens keys of its own; while they show, every other button
  is out of the input (`PortOskOn`, from the frame before) and B closes them
  before BACK leaves ONLINE. They are drawn twice: as buttons in the screen's
  pass, and again after `online_after_frame` (`PortOskLate`), over the
  portraits online_ui.c draws on top (Mary: over every layer).
- **The trainer with a pad:** `PortNavPut` (pad_nav.c) puts the highlight on
  a box: a window opens on its current tile, and the frame after it shuts the
  highlight goes back to the row that opened it. `UMK3_SCREENS` takes 52
  ("trainer"). `UMK3_LOG_NAV=2` lists every target.
- Checked by scripted pad runs and captures (Test folder, muted): the trainer's
  rows and stage window, B in a window, ONLINE's HOME down to the code box,
  typing on the keys, B closing them, the keys over the portrait.

### 2026-11-20 (trainer) -- MERCY as the ANIMALITY's first step; the checklist ticks

Branch `claude/fatality-trainer`. Mary: the MERCY did not work and a
FRIENDSHIP played instead; take MERCY off the list and make it the
animality's first step; the checklist was never saved.

- **Why the MERCY failed:** q_mercy_req (moves.c) wants both round tallies
  `H[0]` and `H[1]` set -- the deciding round -- and d > 0x9f. The trainer's
  F11 only set player one's. game_main.c now sets player two one round short
  as well.
- **Why a FRIENDSHIP played and nothing was ticked:** in the iOS 4/5-button
  layouts the finishing inputs reach DoASpecial (DoSpecial, the G+8 table),
  not secret_move_search, so `PortFTMatched` / `PortFTAllow` never saw them. A
  port hook in DoASpecial's window-open path (after its predicate) reports the
  window-shut handler of the same request. Scripted: `run+s` / `bl+s` (a
  friendship) is now refused and the window restored; `r+s` gives the mercy.
- **The two steps:** `ft_scan` keeps MERCY off the list (`ft_mercy_of`). With
  ANIMALITY chosen and no mercy yet (G+0x45a 0), ft_tick measures the mercy
  entry (`after_mercy`), the band says PASO 1/2 with the mercy's command, and
  there is no floor frame or scale (Mary: the mercy has no place, only a
  minimum). Once it is given, the fight goes on, the second FINISH HIM is
  forced, that window replaces the saved one (RESTART goes back to the
  animality), PASO 2/2.
- Checked by script (Scorpion vs Kano, Subway): the typed mercy, then the
  second window, then the typed animality at d 32..80, which played, ticked
  (`save/fatality_trainer.txt` "18 8", written with `UMK3_FT_SAVE=1`) and returned to
  the trainer's menu. Not seen by Mary yet; other fighters' mercy and
  animality not run one by one.

### 2026-11-20 (later) -- The pages' hint line: PAGE alone

Mary, from 0.5.5 with the Dreamcast icons ("C Z PAGINA SELECT B SALIR"): only
the page part stays. `game_main.c` PortPagesLine builds `<L1><R1> PAGE`; its
`with_sel` argument is kept for the callers (FrontEnd.c, GameCode.c). Built
into the test folder; not yet seen by her.

### 2026-11-20 -- 0.5.5 beta: fatality lag, loads, Dreamcast icons, the launcher's compile

**Scorpion's fatality lag (Mary: "el fatality de scorpion aun provoca lag").**
Scripted (`--fight scorpion kano 5`, `UMK3_TWOUP=1`, `UMK3_DBG_KEY=400:0;1100:0`,
`UMK3_DBG_FIN=400:2`, Direct3D 12): ~150 frames of 70-90 ms, ~50 ms of them in
`scene_flush`. Split by temporary timers: the point lights' twelve faces
19-25 ms, occlusion 10, the receivers' light coordinates 5, the backend's copies
9, and before the flush `tris_indexed` 8 and `submit` 7 -- 1.1 million vertices
a frame, each fighter expanded to three vertices a triangle (68 bytes each) and
copied, transformed and sent by the CPU for every pass. Changes (`rgl_ff.c`,
`rgl_d3d12.c`, `rgl_vk.c`, `rgl.h`):
- `rgl_backend.draw_indexed` (DrawIndexedInstanced / vkCmdDrawIndexed, the
  indices after the vertices in the frame's upload buffer). `tris_indexed`
  sends a fighter's mesh (cast or receiving, not a glow, the scene held) as its
  vertices once and the indices; `hold_draw` keeps the indices
  (`g_hi`), `scene_flush` and the occlusion depth draw them indexed,
  `shadow_cast_idx` copies the shadow triangles by index. The smoothed bodies
  come with `rgl_shadow_proxy(2)` (receive only): the first version left them
  out and changed nothing. Held vertices 1.1 M -> 120 k a frame.
  `UMK3_NO_INDEXED=1` turns it off.
- On omp_lite's threads: the point faces (16 runs a face, each its own draw),
  the key light's map, the receivers, the occlusion depth's fighters, a
  fighter's shadow triangles. A triangle wholly past a point light's far plane
  is dropped (the GPU clips it anyway).
Result: one 69 ms frame as the copies appear, the rest under the slow-frame
threshold. Pictures: two runs of the new path identical (`UMK3_SHOT=330`); the
old path differs from itself run to run (223,499 pixels: SMOOTH ANIMATIONS'
timing), and from the new by less (62,661), the same scene. Vulkan renders the
same; its fatality's CPU flush 45 -> 14 ms but `presenting` ~22 ms (the GPU):
still ~50 ms frames, open. `rgl_test`: OpenGL 0 failed; Vulkan and Direct3D 12
5 shadow checks fail, the same 5 as the code before (stale expected values).
Linux compile 86/86. Mary, after trying it: "ya parece haberse ido el lag".

**Loads (Mary: "acaba con los tiempos de carga").** `UMK3_LOG_LOADS=3`: the
menus' 3.6 s were 2.1 s in the backend's texture upload (a committed upload
resource made, zeroed and released per texture, up to 90 MB with its levels)
and 1.0 s making the levels on one core; decoding was already read ahead
(`draw_gl.c` pf_*, umk3-preload.txt). A second read-ahead per load was tried
and dropped (it duplicated that one). Now one mapped 256 MB staging buffer per
session (D3D12 and Vulkan, reset when the upload list's wait is done, freed at
close), levels and pixel conversion on omp_lite's threads, and
`lime_pvr_load` reads the 52-byte header before the file. Menus 3.6 -> 1.55 s,
fight 1.07 -> 0.82 s; what is left of a fight is decoding its HD PNGs (Kano's
132 ms).

**SEGA DREAMCAST icons.** The 29 SVGs of StrategyWiki's "Category:Sega
Dreamcast controller buttons" (its MediaWiki API; the page itself refuses
scripts), CC BY-SA (source given there: Kenney's input prompts), rendered by
headless Edge at 256 px tall into `launcher/aditional textures/glyphs/dc`.
Style 5 in `ui_alert.c` (`plat_pad_glyph`, by position: A B X Y, C Z on
L1 R1, L R on L2 R2, Start), umk3.ini `pad_icons=dreamcast`, the settings
menu's row and list, the launcher's CONTROLS (`glyphs.js`, `ini.js`,
`i18n.js`). Not yet seen in the game with a pad connected.

**The launcher's compile (a player on another PC: "EXIT CODE: -1" after
omp_lite.c, an old version compiled there before).** The first failed file
rejected `runWithPool` while the other compiles went on reporting, so the
failure's text was not where the player looked; which file failed is not
known. Now `runWithPool` returns every failure, each is compiled again alone,
and one that stays is named after EXIT CODE with its `error:` lines
(`build.lastError`); every line goes to `build\last-build.log`.
`build\built-by.txt`: another version's objects are removed before compiling
(1,113 in the test folder; the full compile after it 15 s). Compiles at once:
cores, and total memory / 1.5 GB. REPAIR (`build(..., { repair: true })`):
objects without their .rsp/.stamp/.d or of zero bytes and orphaned files
removed, the .ipa not extracted when its files are there, `aditional textures`
and `lighting` copied again, the incremental compile (3.8 s with two broken
objects). COMPILE YOUR .IPA in two columns.

**The launcher's window and start.** `window.json` beside `root.json`: size,
place (only if a screen still shows it), maximized, fullscreen; saved on close
and before `app.exit` (relaunch, update). The start measured with the shot aid:
the window at ~0.43 s, the page's start 115 -> 58 ms (the settings 80 -> 18 ms)
with `glyphs:all`, `logo:get` and `hero:get` reading in parallel without
blocking; a cold first start had waited 3.1 s for the settings behind the
synchronous picture reads.

### 2026-11-19 (later) -- Dynamic stage music; the music follows a stage change

Mary asked for the researched dynamic music ([research/DYNAMIC-STAGE-MUSIC.md](research/DYNAMIC-STAGE-MUSIC.md),
design A + D), on the iOS tunes, and for the music to change when a fight
moves to another stage.

- `plat_music_section(start, end, loop)` (platform.h): plays the tune in
  memory from `start` seconds, back to `start` at `end` when looping, else
  silent and `plat_music_playing()` 0 once played out. Negative times count
  back from the file's end. Win32 throws the queued buffers away
  (`waveOutReset`) so the jump is heard at once, and applies a section asked
  while the tune is still being decoded once it starts; the libmpg123 SDL
  build moves `g_mus_pos` between bounds (and `plat_music_playing` there now
  ends a tune that does not repeat, as on Windows); the SDL2_mixer build
  only seeks (no end bound).
- `src/runtime/adaptive_music.c`: called once a frame from the top of
  `AddNewGameEvents` (Blood.c, a port block before the early return). It
  reads FinishHimHer, IsInFinishing, WinnerMessage, MercyMessage,
  CurrentScene and the last tune asked for (`limeTuneName` /
  `limeTuneSerial`, lime_menu.c), writes no game state, and does nothing
  while rollback runs a frame again (`rb_quiet`) or with the game's music
  off (Settings[2] 0). States: normal -> critical (FINISH HIM/HER) ->
  ending (winner named, or after a finisher's tune) -> done; any of them
  back to normal, the tune from its start, once ResetFightData clears
  FinishHimHer and WinnerMessage. Mercy keeps iOS's restart of the stage
  tune. One `music:` line per change in the log.
- Stretches: `[default]` critical -13..-6 s, ending -6..end, from each
  tune's end -- Mary's Soul Chamber proposal (2:03-2:10, 2:10-2:16 of a
  ~2:16 tune) moved onto the iOS files, which differ in length; `music.ini`
  beside the game changes them per tune (`docs/music.ini.example`).
- Stage change: a two-scene level starts in its lower scene
  (CurrentScene 1); any uppercut there blasts to the upper one
  (`t_rup3`: RoundParam+8 != 0 -> `t_blast_through_anything`;
  `MaintainLevelScenes` sets CurrentScene 0). The tune becomes that of the
  level whose first scene is the same file and has no second one, found in
  LEVEL_INFO: level 4 Subway -> Street (level 5, Old.mp3), 10 Soul Chamber
  -> Balcony (0, Pit.mp3), 11 Lair -> Kave (Slow.mp3). The log names it at
  each fight's start.
- Checked with the sound off (`--fight`, `UMK3_DBG_KEY`, `UMK3_DBG_FIN`,
  `UMK3_KEYS`): Soul Chamber, F11 then no finisher -> critical, ending,
  done; Subway, F11 and a fatality -> critical, finisher (Fatal1.mp3),
  ending, done; Subway, a scripted run of uppercuts against the CPU ->
  `the fight moved to another scene, Train.mp3 -> Old.mp3`, later FINISH HIM
  and a fatality on Old.mp3. Not checked: how the stretches sound (Mary),
  Linux, online, Mercy, a draw.

### 2026-11-19 (later) -- Pads: SDL_GameControllerDB, two pads on Linux, the Steam Deck

Asked by Mary with three links: SDL_GameControllerDB, the SDL2 Gamepad Tool
and Steam's ISteamInput.

- **gamecontrollerdb.txt** (SDL_GameControllerDB, zlib licence, in
  `src/runtime/platform/` with `gamecontrollerdb-LICENSE.txt`; every package
  carries src/): about 870 pads' layouts for Windows, 750 for Linux, the
  Steam Deck's own controls (28de:1205) among them.
- **Windows** (win32_gl.c, `pad_db.h`): a WinMM pad -- anything XInput does
  not cover: PlayStation, Switch, 8BitDo in D-input, arcade sticks, generic
  USB pads -- is read through its vendor:product's line when there is one
  (buttons, the hat, triggers that are axes, the right stick; SDL's
  DirectInput axes a0..a5 = X Y Z Rx Ry Rz taken as WinMM's X Y Z U V R),
  else through the vendor tables as before. The log says which
  (`pad: 054c:05c4 laid out as "PS4 Controller" (gamecontrollerdb.txt)`).
  XInput pads are as before. Not tried with a WinMM pad here (none on this
  machine); `tests/pad_db_test.c` checks the reader against the file
  (DualShock 4 on Windows, the Steam Deck on Linux, an 8BitDo, no line, the
  wrong platform: all pass).
- **Linux** (sdl_gl.c): SDL is given the package's file, then a
  `gamecontrollerdb.txt` beside the game (the player's, the SDL2 Gamepad
  Tool's output; the last wins); SDL_GAMECONTROLLERCONFIG, which Steam sets
  for the pads it shows a game, SDL reads itself. Up to eight pads open at
  once; **player two's pad now works on Linux** (`p2pad=`, it read nothing
  before); `pad=` / `p2pad=` with a vendor:product find that pad among all
  (before, only the first pad was looked at). "auto" takes Steam's virtual
  pad (28de:11ff) first -- on a Deck in Game Mode the built-in controls
  arrive through it -- and while player one's pad is idle a press on another
  free pad makes that one player one's, so the Deck's two devices for one
  set of controls no longer matter. Each pad opened is logged with its name,
  vendor:product and GUID. Compiled for Linux here; not run on Linux.
- **ISteamInput** is not used: it needs the Steamworks SDK and the game's
  own AppID on Steam; this game is added as a non-Steam game, for which
  Steam Input gives a virtual pad and SDL_GAMECONTROLLERCONFIG, both read now.
- CI's Linux check (`tools/ci_compile_linux.py`) compiles as the launcher
  does: `UMK3_STATE_FILE` on the game's files, `UMK3_NO_LONG_REMAP` on
  text.c. Without them it read `long` as 8 bytes where the build reads 4, and
  failed on a declaration the real build accepts.

### 2026-11-19 (later) -- Online: two stages, the round's end, Linux buttons; test aids

All port changes, each marked in the source. Checked with two games on one
PC over an EOS lobby and a bad line made on purpose (`UMK3_NET_LOSS=10`,
`UMK3_NET_LAG=40`, `UMK3_NET_JITTER=20` on both: ~120 ms ping, 10% lost),
scripted inputs on both sides (`UMK3_KEYS`, a random walk with punches,
kicks, blocks and runs), best of 3 rounds of 30 s.

- **The two players on two stages** (Mary saw it in the windows; the log
  said level 5 on both). `GameInit_LoadABit` step 0 runs
  `preprocessPreloadKode` (achievements.c), where the host puts
  `*requestedLevel` -- a random stage from `getRandomLevel`, picked in the
  front end -- over `LevelSelect` and sends it in packet 9. With rollback
  the hellos' gate had already settled the stage (the room's, the host's
  STAGE SELECT, or the draw); the host loaded the random one, and the guest
  took the packet only when it came before its own loading. Now, in a
  rollback fight, the host keeps the gate's stage and packet 9 is ignored
  (limeMPGameSpecific.c); a kode that picks a stage still does, the same on
  both. So the room's stage and the host's STAGE SELECT now hold online,
  which the random level used to replace.
- **The softlock after a round.** Every test desynced at frame 2760, the end
  of round 1 (a draw on time): the guest's hash the same in every run, the
  host's not. The round summary (RoundSummaryUpdate, GameCode.c) ends its
  wait early on a tap above the bottom band -- a touch of this machine, which
  the mouse and the pad's highlight make too, not one of the inputs both run
  the fight from. The side that tapped began round 2 sooner and the fights
  parted for good. Not in a rollback fight now. For the same reason the fade
  no longer runs ten times slower while this machine's achievements overlay
  is up (two places), in a rollback fight.
- **Steam Deck / Linux: the fight always took the 5-button layout (issues
  #171, #253).** The game's files are compiled with `long` as `int` on 64-bit
  Linux (savestate.h, UMK3_STATE_FILE); the port's own files are not, and
  `game_main.c` said `extern long Player1NumButtons`: an 8-byte read of a
  4-byte variable, so `Player1NumButtons == 6` was never true and the keys
  and pad always went through the 5-button table (X and Y high punch, A and
  B high kick, word for word the report); writes clobbered the next
  variable. Shown in the assembly from `zig cc -target x86_64-linux-gnu`:
  `movq (%rax),%rax` before, `movl (%rax),%eax` after. `umk3_long`
  (savestate.h: `int` there, `long` elsewhere) is now the type of every game
  global the port's files name (game_main.c, debug_menu.c, online_ui.c,
  presence_game.c, rollback.c, settings_menu.c, smooth_models.c,
  fight_headless.c) and of the port functions the game calls with a `long`
  (mpnet_send, rb_net, rb_sound_ok, port_skin_mats_draw, port_smooth_draw,
  PortMoveGlyph, the strike hook). GameMode and GamePaused were read the same
  wrong way on Linux (START and SELECT did nothing in a fight, #171). The
  Windows build is unchanged (`long` is 4 bytes there). Not yet tried on a
  Deck.
- `mk3_init_game` (loading step 1) runs on both machines in a rollback fight
  (it already did: isParentBasedOnSpeed is 1 on both; now said there too).
- Results: best of 3 with 120 ms and 10% lost, 0 desyncs, both ending on
  frame 4305 (it desynced at 2760 before); two fights in one session with
  inputs and the bad line, 0 desyncs, both ending on the same frame; the
  guest closed mid-fight: the host back on the ONLINE screen ("the link was
  lost") in about 40 s. KOMBO SLIDE in training (`UMK3_PRACTICE=1`, the
  5-button keys): OFF drops a kick's push at the kombo's second hit
  (`punches 0, hits 2, dropped`); ORIGINAL keeps them and `separate us`.
- **Test aids** (rollback.c, mp_net.c, savestate.c): `UMK3_NET_LOSS`,
  `UMK3_NET_LAG`, `UMK3_NET_JITTER` (a bad line, on the sending side);
  `UMK3_RB_SYNCTEST=1` (each online frame run twice from one snapshot, as a
  rollback and for real, every differing word named by dbghelp from the
  .pdb; slow); `UMK3_RB_DUMP=<frame>` (both games stop at that frame, write
  their sections and arena to `logs/rbdump-<player>.bin`, the second names
  every differing variable and prints the words the desync hash reads).
  Also found by them: the drawing writes about 1.2 MB a frame inside
  `PreloadedCharacters` (skinning scratch, outside the snapshot, harmless)
  and SMOOTH ANIMATIONS writes `Players+0x520` (the pose shown, each
  machine its own).

### 2026-11-19 -- Online softlocks and stutter; issues #252 and #253

Port changes; nothing in the decompiled engine's logic changes outside the
marked port lines.

- **Second fight desync (the "known problem" of 2026-11-13) and the
  softlocks after a fight or a round.** `FrameCompensation` (GameCode.c,
  0x0014e1f0) carries the 0.9322 of an engine step each 60 Hz tick
  (Task_GameMain). The binary never resets it, so a run's second fight
  began with what the first left -- different on each machine (the other
  player may not have fought, or fought a different length). The engine
  stepped on other ticks on each side, the hashes differed from frame 60
  on, and the fights parted: one ended, the other held for inputs for good.
  Now `rb_setup` (Task_GameInit, every fight) and `start()` (rollback.c,
  the first online frame) set it to 0, and `start()` seeds both
  generators again (the loading in between runs a different number of
  frames on each side) and sets `SpeedNormal` 1.
- **send_inputs** sent the newest 32 inputs when more than 32 were
  unacknowledged; `got_inputs` takes them only in order, so the gap was
  never filled and both sides held for good. It sends the oldest now.
- **A fight with nothing from the other side for 15 s** (`RB_STALL_MS`,
  while holding) ends as a lost link (`disconnected()`, `endMP()`): the
  log says `rollback: nothing from the other side ...`.
- **Holds for being ahead** (the stutter): the rule held a frame whenever
  the raw `adv - g_remote_adv` reached 3, every 10 frames, and the two
  sides took turns (75 holds in 600 frames in an old one-PC test). Now
  half that difference averaged over ~32 frames (`g_ahead`), a hold only
  past 1.25 frames and at most every `RB_SYNC_GAP` (30) frames. The
  window-full hold is unchanged. `fight_speed()` is 1 in a rollback fight.
- **Tested** with two games on one PC (host `UMK3_LOBBY_TEST=host`,
  `UMK3_LOBBY_RULES=r2;b1;m1;t30;k0;s1;h1`, `UMK3_LOBBY_ROOM=o0;n1;e1;p4`;
  guest `join` with `UMK3_EOS_NEW_DEVICE=1`, its own save folder and
  `profiles_j.ini`; the select screen's fighter and PLAY tapped by
  `UMK3_TAPS`), two fights in one run: fight 1 ended on frame 1350 on
  both, fight 2 on 1435 on both, 0 desyncs, 0 rollbacks, holds 4 / 13 on
  the host and 0 on the guest, ping ~17 ms. No inputs were played (the
  taps only picked the fighter), so the fights were time-outs. Not yet
  between two machines. The old build was not run on the same script, so
  the desync is not shown gone by a before/after on this test.
  Seen on the way: `UMK3_LOBBY_TEST`'s fighter auto-pick (FrontEnd.c,
  15 s) did not fire on the online select screen (front-end task 42);
  taps were used instead. Not looked into.
- **Issue #252, KOMBO SLIDE OFF "does nothing"** (VanDammage87): OFF
  dropped a hit's push (`away_x_vel`, other.c) only once the punch count
  (`field4c`, punches only) reached 2, so a kombo's kicks still pushed the
  opponent away. It now also uses `p_hit` (every hit taken without getting
  back to normal, `reaction_start_chores` -> `inc_p_hit`): from the second
  hit of any kind. In GameMode 6 the VS RULES' KOMBO SLIDE (ON by default)
  overrode the settings menu's OFF; now either one's OFF turns it off.
  Online the rules alone decide, as before. Not played yet; the
  `kombo push:` log line now shows `hits`.
- **Issue #253, Steam Deck, buttons=6 played with five** (also #171): the
  game adopted any change of `Settings[4]` after boot as the player's and
  wrote it into umk3.ini (`port_ini_put_buttons`), so whatever else set it
  -- a replay's recorded settings (`rb_setup`) is one -- became the layout
  for good. Now only `PortButtonsChosen` (set by OPTIONS' NUMBER OF
  BUTTONS, FrontEnd.c, and the pause menu's, GameCode.c) or the settings
  menu (`port_buttons_set`) change it; anything else is put back to the
  chosen layout and logged (`buttons: N set outside the menus (task T)`).
  Checked here: buttons=6 in umk3.ini, a scripted fight, `keys 6`
  (`UMK3_LOG_P1`). The Deck's own cause is not known; a log from the
  reporter with the new build will name it if it was another writer.
- Lag seen in Mary's 2026-11-14 log on the select screen (100-130 ms each
  time the fighter changed) did not show in a scripted run here (four
  fighters, no slow frame after the first); not changed.

### 2026-11-14 -- 0.5.3 beta: the MOD HUB on networks that block GitHub's raw files

Mary: on other players' PCs the HUB "simply does not connect", no error.
From here the index, pictures and zips all answer, so the HUB was waiting:
`fetchIndex` and `check` (lib/hub.js) and `download` (lib/update.js) had no
time limit, and `raw.githubusercontent.com` is blocked or silent on some
networks. Now: `timed()` gives every read 8 s (ranges 30 s);
`fetchRepoFile()` takes the index and pictures from GitHub raw, jsDelivr
(`cdn.jsdelivr.net/gh/<repo>@main/`, cached up to 12 h) or the GitHub API
(`contents/<path>?ref=main`, `Accept: application/vnd.github.raw`), the
one that answered first next time; `download` stops after 30 s without an
answer or 45 s stalled. ModHub.svelte shows a failure instead of nothing.
Tested with node and a fetch that never answers for the blocked hosts: no
block 0.1 s; raw blocked 8.1 s; raw and jsDelivr blocked 17.5 s; all
three blocked, the error naming each in 36 s. The release downloads
themselves (github.com, objects.githubusercontent.com) have no mirror.


### 2026-11-14 -- One voice a sound (Reptile's orb)

Mary: Reptile's force ball "does not play the right sound" (a forum line:
it is the spit's). Checked one by one in TRAINING (`UMK3_PRACTICE=1`,
`UMK3_DBG_SPECIAL`, Reptile = character 0x13: special 0 t_do_spit, 1
t_do_reptile_orb, 2 t_do_reptile_orb_fast) and in the binary:

- `t_orb_calla` (0x76534): `a10` down by one each frame; at 0, `a10 = 3`
  and `field1c = 3 + 0xf = 0x12`, `bl 0x57be4` (ochar_sound:
  `MKEvent_Add(2, 3, (char << 8) + 0x12)`). The landing (t_orb_proc
  0x7ba10, token 0x4df) asks for 5. For Reptile, `st_nj[0x12]` is
  `Reptproj` (st_nj[5] empty); the spit asks for 8 (`Repthock`) and its
  projectile `Reptspit`. The port plays the same names: the orb is
  `Reptproj` in the binary as here, a different file from `Reptspit.wav`.
- The difference was the playing: `limePlaySound` (0x65344) sends `play`
  then `setGain:` to the sound's `Sound` object (limeLoadSound 0x65ad0:
  `[[Sound alloc] initWithFile:]`), and `-[Sound play]` (0xada08) sends
  `playing` and, when it is, `stop` before the source plays again: one
  voice a sound, restarted. The port's `play_sound` (lime_menu.c) mixed a
  new voice each time, so the orb's sound, asked for every 3 frames, piled
  up six copies. Now `plat_audio_stop_pcm` first. Every sound asked for
  again while playing is affected (as on the device).


### 2026-11-14 -- 0.5.2 beta: black after VS, loading, Jade's tower, glyphs, ENHANCED preset

Checked first on a clean copy of the 0.5.1 release (`v0.5.1-beta` zip
unpacked into a new folder, compiled with its own launcher from the .ipa):
it compiles, and `aditional textures/` (glyphs included) and `lighting/`
arrive beside the game, so ENHANCED LIGHTING's files reach a new install.

- **The black after the VS screen** (`runtime/game_main.c`): the host's
  catch-up loop ran up to 15 ticks a frame; the fight's init steps
  (TASK_GAME_INIT 1-10, each a tick) ran back to back, one frame of
  1.6-2.1 s with nothing presented. A tick longer than 150 ms now ends the
  frame; LOADING and its percentage show (captured: 0%, 44%, 80%, 100%).
- **The tune** (`platform/win32_audio.c`): the ACM decode of a stage's MP3
  (1.1-1.4 s, measured as "sounds" in the slow-frame line) runs on a thread;
  `music_update` starts it when ready; a request replaced by a later play or
  a stop is dropped (state 0/1/2 by InterlockedCompareExchange).
  `plat_music_playing` counts a tune being decoded as playing (PortKD waits
  for the end of Shao Kahn's music). Linux's `sdl_audio.c` still decodes on
  the game's thread.
- **Loading** (`draw_gl.c`, `render/rgl_ff.c`): the boot's read-ahead list
  was capped at 160 and was full with the HD mods; now 1024, with at most
  128 pictures / 768 MB read and not taken (a 4096x4096 sheet is 64 MB), and
  the game's thread may take a held-back picture to read itself. Even-sized
  textures' mip levels by a loop without edge clamps (the same bytes).
  Measured: a fight's load 2.9 s -> 1.08 s on the clean install; the boot in
  the test folder with the HD mods ~3.9 s, most of it now the uploads of
  4096x4096 sheets (levels ~1.1 s, the Direct3D 12 upload ~1.6 s in all).
- **Jade over and over in the tower** (`gamecode/GameCode.c` Load_Tower):
  `_OpponentTowerList`'s image default is character 16 on every rung of the
  first ladder; with no `towerdata` saved the tower showed it until an
  Arcade game ran PopulateTower. Port: Load_Tower with no file calls
  PopulateTower (which saves it).
- **Glyphs** (`platform/ui_alert.c`): `g_glyphs[128]` held misses too and
  filled after the CONTROLS page showed several pads' sets; every later
  glyph (the select screen's OUTFIT / COLOR keys) came back as none, so
  nothing was drawn. 1024 slots, more than the folder's pictures.
- **ENHANCED preset** (`launcher/app/lib/ini.js`): widescreen=1, aspect 16:9
  (21:9 kept on a 21:9 monitor), pad_icons / p2pad_icons auto.
- `UMK3_LOG_LOADS=<ms>`: every texture or file load slower than that, and
  each texture upload's split.


### 2026-11-14 -- Online: a MERCY no longer parts the two machines

Mary's report (a match on another PC): online, a MERCY in the final round
(round 3 of a best of three) left the fight stuck. The MERCY itself is the
engine's and was right: a local versus with the debug keys (F9, F10, F9,
then the menu's FINISHER 1 three seconds after FINISH HIM) granted it --
`t_do_mercy` (0x525cc) checked against the disassembly, it only grants it
while `mytc[2]` sits in `t_fatality_wait`, which a forced finisher at the
first frame of FINISH HIM misses -- and the round played on to its end.

The cause was online only. A rollback fight steps the engine on both
machines from both inputs; `rb_game_buttons` sets player one's button
count to the host's on both. `DrawControls` (GameCode.c, the binary's,
0x1ca44) writes `Player1NumButtons = Settings[4]` every frame -- this
machine's own count -- and `GetReal6ButtonJoyBits` decodes player one's
input with it. With two players on different counts (4/5 against 6), the
machine that is not player one read player one's presses through
`FourButtonMoves` (or not), which leaves plain hits alone but changes RUN
and the finishing inputs (the 0x2000 entries). A MERCY (RUN, final round)
happened on one machine and not on the other; the fights parted, one ended
and the other held for inputs that never came. Fix: that store is skipped
while `rb_active()` (port line, marked); the touch layout still follows
this machine's setting. Nothing changes outside an online fight.

Tested: the build in the test folder; a local versus MERCY as above,
unchanged. Mary confirmed the MERCY works online (2026-11-14).

### 2026-11-14 -- Release 0.5.1 beta

`v0.5.1-beta` (pre-release): the WinMM pad fix for issue #222 (entry
below). Packages in `WORK/release-0.5.1`. Not confirmed on an 8BitDo yet:
the notes ask reporters for the log's `pad:` lines.

### 2026-11-14 -- Issue #222 again: pads walking and jumping by themselves

Reports after 0.4.10: the 8BitDo Ultimate 2 still walks the fighter back
and jumps, in the menus too, untouched (Windows 11). The Windows build reads
pads through XInput, else WinMM (`joyGetPosEx`) -- not SDL (SDL is the
Linux build's, which already uses the GameController API with a dead
zone). 0.4.10's fix took the axes' ranges from JOYCAPS on the guess that
the pad reports 0..255; that was never seen on the device, and the
reports say nothing changed.

The symptom (up + back, held) is what any axis or hat resting at an end
gives on the WinMM path. Fix (`win32_gl.c` pad_winmm): an axis counts only
once it has been seen within 10,000 of its middle since the pad was opened,
the hat once it has read centred; a working stick and cross start there,
so nothing changes for them, and an axis resting at an end is left out.
Seen here on a DualSense (054c:0ce6, WinMM): X Y Z R and the hat counted,
U and V (its triggers) rest at 0 and are now ignored -- U was the right
stick for pads of unknown make. The log now has, once per pad, its WinMM
name, VID:PID, axes, buttons, hat, each axis' range and its resting values
(and with UMK3_LOG_PAD=1 every raw change). Not tested on an 8BitDo, an
Xbox pad, the 2.4 GHz receiver or a hot plug here: a reporter's log will
tell the device's layout. No SDL change (the upstream SDL 8BitDo HIDAPI
work does not reach this path).

### 2026-11-14 -- Release 0.5.0 beta and ESSENTIALS 1.0

`v0.5.0-beta` (pre-release): Shao Kahn's death (entry below), the select
screen's tune now stopped as the stage is picked (`debug_menu.c` st_close,
`FrontEnd.c` PortVsGate; it used to play on under the VS screen and fade,
Mary), the ESSENTIAL MODS box. Packages in `WORK
elease-0.5.0`; built
without a steamgriddb key (as before).

MOD HUB: `essentials` 1.0, `"essential": true` -- `Nomore.wav` (Mary's "No
More" track) and `Skversus.mp3` (her VS "versus"); published with
`publish.js --essential` (release `essentials-v1.0`, index.json pushed).
Mary chose to publish both tracks knowing they are arcade audio. Checked in
the test folder installed as the launcher would (mods/Essentials, on in
mods.ini, no Nomore.wav in res/audio): the fight held until the tune ended
(tick 2026), so it plays from the mod. COMPILE's install of it from the hub
was not run end to end.

### 2026-11-13 -- Shao Kahn's death as the arcade plays it (port addition)

Worked from docs/research/ARCADE-KAHN-DEATH.md, two MAME runs (the second
one frame per arcade frame through the strobe, every 0.1 s to the
treasures, with sound) and the iOS decomp only; no leaked source or
anything derived from it was read. Mary watched each build beside the
arcade and corrected it; where her account and MAME differ, hers was taken
(the stage's music stops at the hit; the scream twice, back to back;
three blasts; the beams start at once after the strobe).

`GameCode.c` PortKD (header comment there has the whole timeline): a clock
from the hit that empties Kahn's energy in the match's last round
(Health[1] -> 0, GameMode 0, Character2 0x19). The strobe draws the
fighters flat through the shadow's untextured path (`Players.c`
PortFlatGrey, `RenderMesh.c` LimePortFlatGrey, REAL-TIME SHADOWS off for
those frames, the hammer and other attachments skipped); the shake is a
screen-space shift in `LIMEDS_Set3dMode` (PortScreenShiftY); the other
stages are their pictures (`debug_menu.c` stage_picture_full) -- the
empty city first, drawn over him, then every stage of the game in turn; the burn and
sky are additive fills; the end's flashes fill the screen after the HUD;
the HUD is drawn with colour writes off; the end's lines are drawn by
PortKD_Lines. `Blood.c`: PortKD_StartBeams starts SK_ENDING at the strobe's
end, event 3/65 then leaves the scene and scream alone; event 2/0 sounds
(the announcer) are skipped during it. `RoundSummaryUpdate` waits for
PortKD_HoldRound. `lime_menu.c`: lime_port_sound (a sound by file name),
lime_port_stop_sound, lime_port_tune_exists. `win32_audio.c`: 16-bit PCM
`.wav` tunes; a non-repeating tune counts as over once played out.

Verified by captures in the test folder (`UMK3_LOG_KD=1` logs every change
of what is drawn, with its times): strobe pictures every 0.0366 s from
0.11 s, beams at 1.23 s, whites at te 0.45 / 0.75 / 1.0, three Strtbooms,
two Skdiemix, no Kitanawins, lines at te 2.3 / 5.7 / 8.9, the treasures
at ~25 s after the music. Not done / open: the stage pictures are flat
(no depth behind him); the arcade's own rumble under the beams is not
identified among the game's sounds; the Linux (SDL/mpg123) music path does
not read `.wav` tunes; Mary has not yet played the final build.

### 2026-11-13 -- ESSENTIAL MODS at compile

Mary: the essential mods are the modpack of the project's improvements,
installed when compiling, the box ticked by default. `lib/hub.js`
`essentials()` installs or updates every hub entry with `"essential": true`
after a good build (`main.js` build:start; a failure is logged, the build
stands); `essentials=1` in umk3.ini; publish.js `--essential`. Verified:
launcher tests pass (35), the box shows ticked on the compile page; not
run against a real essential mod (none published). A test mod in the
test folder holding `Skversus.wav` (a copy of one of the game's own
voices, removed after) loaded and played between the names: mods add
sounds the game lacks. The pack's "versus" voice is Mary's to supply.

### 2026-11-13 -- Release 0.4.10 beta: the local VS screen, random looks, one gallery, pads

Mary's requests in one session; details in CHANGELOG 0.4.10.

- **VS screen** (issue #224): the select screen sets `PortVsPending` (local
  versus and training, GameMode 6) and `Task_GameInit` shows
  `PortVsGate` after `stage_menu_gate` -- first tried before STAGE SELECT,
  as a pushed `FE_Task_VS_Screen`; Mary saw it vanish under STAGE SELECT,
  and that task's init also overwrites Character2 from the tower list, so
  the gate draws its own frame with the front end's pictures, which
  draw_gl.c never frees. Sound: the binary's two VS screens call no sound
  function (their BL targets listed from UMK3.armv7 0x5b74..0x6488 and
  0x16410..0x172f0); the announcer's names are the select screen's voices,
  loaded and freed by the gate. Verified: captures of SCORPION colour 4 vs
  REPTILE colour 5, log of the two voices.
- **The VS line**: the mirrored portrait (u 0..-1, GL_REPEAT, linear) took
  the far column round at its edge; `VS_EDGE` insets both portraits on all
  three VS screens. Verified by an enlarged capture.
- **CPU looks**: `cpu_look` draws colour and outfit together, every colour
  of the original and of each added outfit alike. Not measured over many
  fights.
- **One gallery**: launcher PROFILE lost the colour row (it ran under the
  gradient picker); online MY PROFILE lost FIGHTER / COLOR steppers, the
  gallery got COLOR 1..6 (`OB_GALCOL0`) and "#n:alt" art codes. Captured.
- **Fade**: `PortFadeOnTop` -- FEMain's quad is drawn by game_main.c
  (`ui_fade_layer`) after online_after_frame. Not captured mid-fade.
- **Pads** (issue #222): `axis_norm` / `pad_ranges` read WinMM axes in
  JOYCAPS' range; `g_map_8bitdo`. Not tried on an 8BitDo pad.

### 2026-11-13 -- An added outfit's colours, 1 to 6

Mary: outfits start with one colour and the player adds up to six; they do
not need six (Sonya MKX has 2). `mods.c`: `Skin.color[7]` (1..6, ice),
`color3..6=` and names ending 2..6 guessed, `skin_tex` answers
`_diffuse3.._diffuse6` (and `_LOW` / `_LITE` spellings), a missing one is
colour 1; `mods_skin_colors` counts 1 + each next colour present.
`Players.c` loads `@S<n>@<NAME>_DIFFUSE<k>.PNG` for an outfit's colour 3..6
(no recolouring) and clamps a colour past the outfit's to 1; `FrontEnd.c`
`pal_sheet_of` does the same for the select screen's model; the COLOR list
shows the outfit's count. Launcher `lib/mods.js`: the same rules, `colors`
on the mod list, `skinCreate` copies colour 1 and the frozen sheet only;
the viewer shows an outfit's own colours and "+ COLOR" for the next.

Checked: Sonya in MKX on the select screen lists COLOR 1 and 2. The test
folder's launcher was repacked (app.asar, old one `app.asar.before-
palettes.bak`) so it compiles palettes.c and writes the colours after a
build; `launcher/bin/resources/src` and its palettes.ini synced too. Not
checked: adding a colour to an outfit in the viewer.

### 2026-11-13 -- Release 0.4.9 beta; HD CHARACTERS 1.1

Released as the pre-release `v0.4.9-beta` (zip 148 MB, Linux tar.gz 129
MB) with the colours 3..6, the select screen's lists, the profile's six
colours and outfits with 1..6 colours. The MOD HUB's HD CHARACTERS
(`hd-ninjas`) is 1.1: the test folder's pack with its colours 3..6, 92
full-size pictures; its 240 `_LITE` / `_LOW` colour copies (byte-equal to
the full ones) were left out so the zip stays under GitHub's 2 GB (the
folder is 3.3 GB with them); draw_gl.c load_rgba falls back to the pack's
full one (PR 230). The hub's UPDATE replaces the folder; the next build
writes the copies again.

### 2026-11-13 -- Profile pictures in colours 1 to 6

Port work, not in the binary. Mary: the player profile's pictures with the
new colours up to 6. `struct profile.alt` was 0/1 (the second outfit); it
is now the colour less one, 0..5, in profiles.ini (an older file's 0/1
reads the same), the lobby look (FrontEnd PortLobbyMe, gamecode_stubs
port_online_friend_look) and the online info word (mp_net.c: bits 20..22;
an older game reads bit 20). profiles.c: `k_pal` names each icon's
palettes.ini fighter (the art names differ: JAXX, LUIKANG, SUBZERO_HUMAN
is Sub-Zero, SUBZERO Classic Sub-Zero, SMOKE_SECRET Human Smoke);
colour_art takes `<ART>_PROFILE<n>.png` or makes it: the base colour's
VERSUS .pvr framed as alt_art does, then pal_apply. `prof_make_files`
(after port_pal_make_files in --make-colors) writes them: 92 in the test
folder (23 fighters x 4; Noob Saibot has 2 colours). Sonya's second art is
not her second sheet's magenta, so nothing changed: `pal_known` + a
`[SONYA_ART]` section turn her first art's clothes (hue 65) instead.
Menus: the game's settings row COLOR < n >, ONLINE MY PROFILE a COLOR
stepper; the launcher's PROFILE and fighter gallery COLOR 1..6 tabs,
`artOf(icon, alt)` (app.svelte.js) for every icon, IPC profile:iconCol.
draw_gl.c load_rgba: a mod with the full-size colour n but not its
_LITE/_LOW copy gives its full one (before, res/Textures' small one won),
so the HD pack can ship one copy of each colour (its _LITE/_LOW sheets are
the same pictures). Checked: a contact sheet of all 24 x 5 pictures; the
build. Not checked: the launcher's tabs in the window, online.

### 2026-11-13 -- Scorpion's colour 5 yellow

Mary: Scorpion 5 was red (orange, hue 20). palettes.ini SCORPION 5 =
50 1.60 1.10 (golden yellow, clothes' mean 49/.87/.75); the nearest of his
other colours is 1 at delta-E 22. Test folder: his 9 colour-5 files
rewritten (vanilla and HD). Not played in a fight.

### 2026-11-13 -- Colours 3-6 told apart (Sektor and Cyrax swapped)

Mary: Sektor's extra colours were Cyrax's (yellows) and the other way
round; then colours of one fighter must not look alike, nor like another
fighter's. Measured: the mean colour of each fighter's picked pixels in
colours 1..6 (colours 3-6 by applying the ini to the base sheet), turned
into CIE Lab; pairs under delta-E 25 flagged. 15 fighters had close pairs
(Smoke, Cyrax, Jade, Kitana, Scorpion and Classic Sub-Zero many). New
values in palettes.ini for SONYA 5, JAX 5, SHANGTSUNG 5, SHEEVA 4, SMOKE,
SEKTOR, CYRAX, JADE, MILEENA, KITANA, SCORPION, REPTILE, ERMAC, OLDSUBZERO,
OLDSMOKE. Mary looked at them in the test folder and corrected: Kitana 3
was purple (Mileena's) -- back to her reference's dark teal; Mileena 5
blue (Kitana's) -- pale pink; Ermac 5/6 brown and orange -- brick and
blood red; Classic Sub-Zero 4 near-white (Smoke) -- darker ice blue;
Sektor no yellowish tones -- wine, bright red, pale pink, crimson magenta.
The rest she approved as seen. Left close (under 25): only colour 1 vs 2
pairs (EA's), Kitana 2-3 22, Ermac 3-5 22, Classic Sub-Zero 3-4 20 and
1-4 24, Sektor 3-6 19 / 2-3 21. Test folder: those fighters' colour files
deleted and rewritten by --make-colors (708 in all), in res/Textures and
mods/HD NINJAS (22 fighters there, 12 files each; Sonya has no HD sheet).
Not played in a fight.

### 2026-11-13 -- Reptile's colours greener

Mary: Reptile's colours 3-6 were too yellow and looked like Scorpion's.
palettes.ini [REPTILE] 3..6 = hue 118 / 105 / 150 / 135 (were 80 / 110 /
150 / 70). In the test folder his 36 colour files (vanilla and HD NINJAS)
were deleted and rewritten by `--make-colors` (it keeps existing files, so
an ini change needs them removed). Checked on a contact sheet beside
Scorpion's: all four green, none yellow. Not played in a fight yet.

### 2026-11-13 -- Colours 3 to 6 from a fighter's own sheets

Port work, not in the binary. Mary: colours up to 6 without new textures,
by hue / saturation / value as in Substance Painter; Kung Lao first, his
trousers green, red, purple, grey (her arcade reference).

Measured: Kung Lao's `_DIFFUSE` trousers are hue 49, S .93, V .47 and
`_DIFFUSE2`'s hue 196 at the same S and V -- EA's colour 2 is a hue turn.
His skin is hue 29, S .51, so the pick (196 +-26, S >= .30) leaves it.
The reference's trousers: green 118, red 358, purple 286 (S .86-.95,
V ~.62), grey unsaturated.

- `src/runtime/palettes.c` reads `aditional textures/palettes.ini` (and a
  `palettes.ini` beside umk3.ini over it): per fighter `base`, `hue`,
  `pick`, `sat_min`, and `3`..`6` = hue (-1 keep), saturation x, value x.
  `pal_apply` turns the picked hues by (new - hue), scales S and V, and
  feathers the edges (10 degrees, 0.6..1 x sat_min).
- `draw_gl.c`: a texture named `@P<n>@NAME` is NAME recoloured
  (`load_rgba`); `port_pal_texture(t, n)` gives a loaded texture's colour-n
  twin, cached under that name.
- `Players.c` `LoadLevelCharacters` / LAC: `PortPalLoad` recolours the
  fight's diffuse and babality. `FrontEnd.c` `pal_sheet`: the select
  screen's model (+0x528) and both VERSUS screens' art. `pal_count` sizes
  the COLOR list (2 for an added outfit); `cpu_palette` draws among all;
  the online mirror rule no longer computes 3 - host; packet 23 and
  save/palette.txt take 1..6.
- Test aid: `UMK3_PALETTE="p1,p2"` (also sets the debug fight's palettes).
- Then 20 more fighters (palettes.ini now has 23): each base sheet and
  hue measured by comparing sheet 1 with sheet 2 (the pixels whose hue
  differs are the clothes); the sheet whose clothes are furthest from the
  skin's hue is the base (Stryker and Shang Tsung from sheet 1, their red
  sheet 2 too near the skin; Liu Kang's red from sheet 1 with a narrow pick
  and sat_min .66, his skin at .51). Sektor, Cyrax, Scorpion, Reptile,
  Ermac, Classic Sub-Zero and Human Smoke have two equal sheets; Human
  Smoke's clothes are grey, so `grey = 0.15` tints the unsaturated pixels.
  The reference colours were read by eye from Mary's arcade pictures and
  turned into S and V factors of the base sheet's.
- `--make-colors` (game_main.c, before the window): draw_gl.c
  `port_pal_make_files` writes each colour of each palettes.ini fighter as
  PNG (palettes.c `pal_png_write`: Sub filter, LZ77 + fixed Huffman) beside
  its source: res/Textures and the winning mod's folder; existing files are
  kept. Names: the sheet's "2" becomes the colour (`pal_file_name`). The
  loader takes `<name><n>.png` before recolouring, and a missing _LITE /
  _LOW copy falls back to the full-size colour file. `pal_count` also
  counts `<NAME>_DIFFUSE3..6` files. launcher/app/lib/compile.js runs it
  after linking. In the test folder: 752 pictures, KUNGLAO's HD ones at
  2508 square.
- Viewer (assets.js, ModViewer.svelte): six COLOR slots for every fighter;
  an empty one shows colour 2; COLOR 3..6 named. Built (npm run ui), not
  checked in the UI harness.
- Player two's select-screen buttons: the pad's pictures with no pad two.

Checked by script: a contact sheet of every fighter's VERSUS 3..6 and the
model sheets of Liu Kang, Ermac, Shang Tsung and Human Smoke (skin kept);
Kung Lao's fight with the colour files (HD 2508 loaded). Sonya, Jax and Liu
Kang have no VERSUS sheet of their base colour, so their VS art keeps it.

Checked: `--fight kunglao kunglao` with 3,4 and 5,6 (green, red, purple,
grey); select screen COLOR 4 (red model); VERSUS2 recoloured offline in
Python with the same sums (green, grey). Not tried: by Mary in play,
online, the endings. Grey was darkened to V x0.80 after the first shot.

### 2026-11-13 -- The select screen's OUTFIT and COLOR lists

Port work, not in the binary. Mary asked for the newer games' lists in place
of the SKIN and COLOR boxes: in each top corner a gi and a palette, one above
the other, each with its button beside it; the button drops a list beside it.

- `FrontEnd.c` (the block after `PortSendMySkin`): `drop_catch` runs before
  the portraits and takes a fresh press that lands on the buttons or comes
  while a list is open (it marks `limeLastTouchScreenX`, so no portrait sees
  a fresh press); `drop_one(side)` draws the two buttons
  (`aditional textures/ui/outfit.png`, `color.png`, made by
  `tools/make_select_icons.py`) and the open list (`limeFillQuadGrad`
  slanted bars, new in `draw_gl.c`), and acts on the clicks and on
  `PortDropIn`. Player one's buttons sit at y 46 under RULES (VS) or the
  online banner, else at y 6. `palette_box` and `skin_box` are gone;
  `set_palette` keeps the colour's three paths (online packet, VS, solo).
- `game_main.c` `drop_input`: per side the pad's north / west (open), up,
  down, south, east and the keys `PK_OUTFIT` (T) / `PK_COLOR` (C), Enter,
  Backspace (player two's: the arrows, its punches, block), edge-triggered
  into `PortDropIn`; `g_drop_hold` blanks that side's pad and keys before
  `pad_nav_end` while its list is open and until ACCEPT / BACK are let go,
  so the highlight does not move nor pick a fighter. The buttons' pictures
  (`PortDropGlyph`) are that side's pad's, else the keys'.
- `PK_OUTFIT` / `PK_COLOR`: new codes (platform.h, win32 and SDL tables),
  umk3.ini `key_outfit=` / `key_color=`, Esc > CONTROLS rows.

Checked by script in the test folder: Arcade (UMK3_PADS right, right,
south, north, down, south, west: the outfit list opened, MKX put on, the
colour list opened with the outfit button still shown) and VS
(UMK3_VSRULES_PREVIEW, taps: player one under RULES, player two's list
opening to the left). Not tried: by Mary; online (only player one's side is
drawn, as before).

### 2026-11-13 -- Outfits that add (skins beside the original)

Released in **0.4.8 beta** (with the CPU wearing an outfit one fight in
two, drawn once per opponent like colour 2, and the viewer's BACK and
PANEL). MOD HUB: Sonya Blade MKX 2.0 is the outfit; 1.0 deleted.

Port work, not in the binary. Mary's ask: skins apart from colours -- a
fighter can have more outfits than the game's, each with its own model and
its own two colours, beside the original, without replacing it; the MKX
Sonya mod the test.

- **Game.** A mod with `type=skin` in its mod.ini is indexed apart
  (`mods.c` `skin_take`); its files are asked for only through a fighter
  loaded with it, under the game's names with `@S<n>@` in front
  (`Players.c` `PortSkinLoad` around `LoadAnimatedCharacter`), so the
  texture cache, the lighting bakes and `skin_mats` keep the two apart and
  the original and the outfit fight each other. A name the outfit lacks is
  the game's file (`mods_real_name` in `resolve` and `load_rgba`). Its
  files can have any names: `model=`, `color1=`, `color2=`, `ice=`, or by
  their own names (ice, ...2, the rest); a game-named file in it wins (the
  launcher writes those). `skin=` is its own name. A `.mats` part can name
  COLOR 2's sheet (sixth column, else `<sheet>2`): each colour has its own
  set of part sheets. The select screen's SKIN box above COLOR (FrontEnd.c
  `skin_box`), the preview loaded for it (`PortFESkinAnim`, freed with the
  front end's fighters). Online: packet 26 names a side's outfit by fighter
  + model size + colour 1 size; the other machine shows it only when it has
  the same, else the original.
- **Checked:** `--fight sonya sonya` with `UMK3_SKIN`: MKX against the
  original, MKX against MKX in colour 2, a two-part test outfit whose part
  turned to its own COLOR 2 sheet (red, on purpose); the select screen's
  SKIN box and preview by scripted taps (`UMK3_TAPS`). Mary played it and
  saw the MKX model drawn with the original's sheet: the launcher had
  rewritten mod.ini without `color1=` / `color2=` (its ini reader took no
  digits in key names). Fixed on both sides. Not checked: online between
  two machines.
- **Launcher.** `lib/mods.js` reads an outfit for the viewer by the game's
  rules (`skinRead`), lists mods with their `skin`, leaves outfits out of
  the mods that replace (`'*'`), and makes a NEW OUTFIT as a copy of what
  the fighter wears now (`skinCreate`). The CHARACTER VIEWER (built on the
  roster branch `claude/viewer-roster-special`, merged here): OUTFIT slots
  and COLOR 1 / 2 / FROZEN with the chosen sheet and REPLACE TEXTURE under
  it; changes go to the outfit shown. Inside a fighter (Mary's layout): the
  section header and the MODS pages bar give their room to MODELS /
  TEXTURES; his models (or the open model's textures, as pictures) beside
  the viewer, then the model chosen with the viewer and its options alone.
  Checked in the UI harness on a copy of the test folder's mods.

### 2026-11-12 -- Discord Rich Presence, and joining a lobby from Discord

The game talks to the running Discord client over its local IPC
(`src/runtime/discord_presence.c`, no SDK, no OAuth) with Mary's
application id; `src/runtime/presence_game.c` maps `CurrentTask`,
`FE_CurrentTask`, `GameMode`, `PortPractice`, `Character1/2` and the EOS
lobby to the profile's texts, with a party and a join secret in a lobby.
A Discord join opens ONLINE > LOBBIES (`FrontEnd.c PortOnlineOpenLobbies`)
and joins it from `PortLobbyTick`. `umk3.ini discord=0/1` from the
launcher's PLAY page. Checked: `tests/discord_presence_test.c` (JSON, join
parsing, `--live` against the project machine's Discord); the game built
into the test folder; **Mary saw the presence on her profile** and the
log says `discord: connected`. Not tried: Join between two accounts,
Linux. Details: [DISCORD-PRESENCE.md](DISCORD-PRESENCE.md).

### 2026-11-12 -- STEAM: the launcher as a non-Steam game, SteamGridDB artwork

A new launcher section, STEAM ([STEAM-INTEGRATION.md](STEAM-INTEGRATION.md)):
it adds or removes the launcher from the Steam library as "Ultimate Mortal
Kombat 3 IOS Decomp" by editing `shortcuts.vdf` (only while Steam is closed,
with a backup, an atomic write and no duplicates), browses SteamGridDB game
38752 with the player's own API key, downloads the originals into
`data/steam-artwork/`, and copies the chosen ones into
`Steam/userdata/<id>/config/grid/` under the shortcut's app id. Steam's
naming and the app id were checked against a shortcut on the project
machine that Steam already shows with its artwork. **Tested:** 10 unit tests
(fake Steam folder, fake SteamGridDB, the real `shortcuts.vdf` round-tripped
read only), and the page in the harness against a copy of that file
(ADD TO STEAM wrote our entry and left the other game byte-identical).
**Not tested yet:** the live SteamGridDB API (it needs Mary's key), the real
Steam client, Linux. Mary asked for it to be automatic: ADD TO STEAM now
picks SteamGridDB's best-scored image of each kind by itself, with the
project's key shipped in the release (`TOOLS/steamgriddb.key`, never
committed). The account list shows the Steam name only, never the id.
With no key at all, ADD TO STEAM uses the game page's own set (the first of
each kind in its Top order, downloaded from SteamGridDB's CDN). Mary's first
try in the test folder: the images downloaded, but the game was not added,
because Steam was still running in the tray and the button stayed disabled.
ADD / REMOVE now close Steam themselves (`-shutdown`), write, and reopen
it, and the page no longer shows the app id. Her second try still did not
add it: her launcher had been opened before the new one was put in
(probably; the code saw her Steam, account and launcher correctly). The
same `steamsync.add` the button calls was then run against her test
folder and her Steam: it closed Steam, wrote the entry (app id
3831288398, backup beside it) and the five pictures, and reopened Steam
in 4.9 s. After Steam's restart, `shortcuts.vdf` still had both entries:
the other game byte-identical, ours with its icon field. **Mary confirmed
it in her library.** The name became "Ultimate Mortal Kombat 3 IOS Decomp"
at her request. An entry under the old spelling is still recognised as
ours, because the name match is case-insensitive. Not yet seen: the
button itself doing it in a freshly opened launcher; Linux. Discord Rich Presence, the third part of the request,
is not started.

### 2026-11-13 -- ONLINE stage 4b: the room

Port, not in the binary ([research/ONLINE-PHASE-PLAN.md](research/ONLINE-PHASE-PLAN.md),
"The room", has the whole design and the phases' checklist). FrontEnd.c
`PortRoomPage` (the room's own page, three columns, the OPTIONS and stage
windows), `PortRoomOwner` (the leader's game: the line, requests, fights'
ends, the next call, the READY clock); `src/runtime/lobby_queue.c` (the
line and the options, pure logic, built inside online_rules.c because the
launchers out there compile a fixed list of files; `tests/lobby_queue_test.c`);
mp_eos.c lobby protocol 2 (`room`, `q`, `go` with two players and a
stage; members' `want`, `res`, `wins`, `vote`; `mplobby_dissolve`,
`mplobby_leave_now` on closing); rollback.c RB_VERSION 3 (the room's stage
in the hello); GameCode.c a tie on time and ROUNDS 1 as kode 0x13;
other.c `port_match_wins` (ROUNDS 5); debug_menu.c `stage_room_thumb`.

Tested with three copies of the game on one PC (ANA the leader, BETO and
CARLA by the key; `UMK3_LOBBY_TEST`, no keys or taps sent), 2026-11-12/13,
Mary watching and playing in the windows:

- the page: the three columns, the top, the stage's picture, the windows
  (screenshots);
- 1 VS 1 / ROTATION, NEXT FIGHT BY ITSELF: ANA vs BETO called by itself,
  CARLA waiting; a draw on time sent both to the back; a fight won (Mary
  played it) turned the line -- the winner stayed, the loser to the back,
  the next call ANA vs CARLA;
- the stage: A VOTE (BETO and CARLA voted the same) -- THE GRAVEYARD,
  level 2, on both; THE LEADER -- THE STREET, level 5, on both, the host
  told it by the guest's hello and no STAGE SELECT;
- the leader's game closed: it left the lobby at once ("left on closing"),
  EOS made BETO the leader, the room went on.

Seen wrong and fixed on the way: READY never reached the others (the
member attributes' read reused one buffer); the select screen locked
after the first fight (pressedPlay left at 1, the summary skipped); the
winner of a one-round fight stood still (fixed as kode 0x13, above); a tie
on time stood still; the host opened STAGE SELECT for the room's stage;
the loser saw CONNECTION LOST after a fight that ended; a call whose
fighter left showed FIGHTING for good (the call is over at once now; CALL
IT OFF is gone); the test aid picked the fighter for a player.

**Known problem, first to look at:** in one test the second fight of a
session -- BETO, who had fought the first, against CARLA -- counted
desyncs (23 by its end). Every first fight of a session had none. Two
fights in one game's run had not been tried before this stage (each
earlier test started the games anew). Something of the first fight is
left in the state rollback hashes; not found yet.

Not seen: ROUNDS 1's FINISH HIM since it is played as kode 0x13, a
series' rematch, the READY clock running out, ASK TO PASS answered,
JOINING ON REQUEST's window in play, MAX STREAK, more than three players.

### 2026-11-12 -- ONLINE stage 4: the rules checked in HELLO

Port, not in the binary ([research/ONLINE-PHASE-PLAN.md](research/ONLINE-PHASE-PLAN.md),
stage 4, has the whole design). `rollback.c` `rb_online_gate`, from
Task_GameInit's state 0 of an online fight, before STAGE SELECT: each side
sends a hello (RB_VERSION 2: version, buttons, netcode, a random number,
the lobby's rules text) and waits for the other's. Other rules, another
version, no answer in a minute or the link gone: no fight -- both go back
to the ONLINE screen with the reason (FrontEnd.c `PortOnlineRefused`).
HOST ADVANTAGE OFF: the side and the level (one of `mpLevelList`'s
fourteen) are drawn from both numbers, the same on both machines;
Character1 / Character2 swapped, `rb_local_p1()` where the game asked
`isParent()` for "this machine is player one" in a fight, the host's STAGE
SELECT not opened, a series counted by player (`rules_series_swap`). The
fighters and level now travel in `RB_PKT_SETUP` (25), which
limeMPGameSpecific.c's processPacket hands to rb_net -- the first test
stalled at the sync without it. `tests/online_rules_test.c` covers the
series by player.

Tested with two copies of the game on one PC (the second
`UMK3_EOS_NEW_DEVICE=1`, its own `UMK3_SAVE_DIR` and `UMK3_PROFILES`; the
select screen's OK tapped by `UMK3_TAPS`), 2026-11-12:

- equal rules, HOST ADVANTAGE ON: the fight started, the host player 1, the
  guest on the host's STAGE SELECT level, `desyncs 0`;
- different rules (`UMK3_HELLO_RULES` on the guest): both logged `NO FIGHT
  -- the rules differ` with both texts, both went back to LOBBIES in the
  lobby with LAS REGLAS NO COINCIDEN: NO HAY PELEA (screenshots);
- HOST ADVANTAGE OFF, four fights: both sides drew the same side and level
  every time (host P1 on 2; guest P1 on 1; guest P1 on 11; host P1 on 0),
  the swapped fights with Character1 / Character2 swapped on both,
  `desyncs 0` on all;
- the other game closed mid-fight: the one left went back to the ONLINE
  screen with CONEXION PERDIDA.

Not seen: an online fight ending by a win and coming back to the lobby
(scripted keys did not land hits; the code is the summary's same way out),
a series with drawn sides between real fights, the classic netcode, an old
game's hello, the gate's minute running out, kick / promote.

**Also in this stage (Mary, watching the test):**

- **One player's touch controls online.** `rb_apply_inputs` sets
  `P2Controls` so the engine reads both players, and DrawControls drew the
  two-players-on-one-device controls (two sticks and rows, this player's in
  the left half -- and hit-tested there). GameCode.c `DrawControls` now lays
  out and draws one player's in GameMode 1; the engine keeps P2Controls.
  Seen: one stick and one row in an online fight. It was so before stage 4.
- **Back to the lobby, not the binary's screens.** An online fight from the
  ONLINE screen ends back in it (FrontEnd.c `PortOnlineBackToLobby`, from
  the summary, FE task 0x28) instead of the summary whose QUIT led to the
  main menu; a lost link goes back to it with the notice CONEXION PERDIDA
  instead of the "connection lost" screen whose button led to the binary's
  searching lobby (FE task 3). The session ends; the host calls the next
  fight from the room.
- The fight's player names (gold plates) are PR #209's, not merged yet.

### 2026-11-12 — CHARACTER VIEWER: roster page, SPECIAL MODELS, mod filter, layers export

Mary asked (in Spanish) for: a SPECIAL MODELS category with the Raiden of
Nightwolf's Friendship; the fighters as a grid of their portraits, each
opening with everything of theirs beside it; a checkbox to see only what a
mod replaces. Then, while it was built: the roster as a page of its own
with a way back, the filter on textures and models, exporting only the
layers shown, and the editor's pages over the whole window.

- **Where the viewer is**: the launcher (`launcher/app/ui/src/lib/sections/mods/ModViewer.svelte`),
  not the game's debug menu.
- **Raiden** is not a file of its own: it is eleven meshes `Raiden001..011`
  (1708 triangles each, texture `RAYDEN_DIFFUSE`) inside
  `NIGHTWOLF_STANDARD.meshset`. `derived.js specialModels` reads every
  fighter's meshset and groups the meshes of 300+ triangles whose texture is
  not the fighter's by texture, leaving out what every file has (`WHITE`
  shells, `HIRESSKELETON`, `ROBOT_SKELETON`, `FROZEN_MANB`) and roster
  skins (`<F>_DIFFUSE`, Sonya's `SONIA DIFF`). Only Raiden's label
  (Friendship) is stated, because Mary said so; the rest are labelled "in
  the fighter's file", named from their meshes. 36 found, plus the
  `ANIMALITY_*` / `FRIENDSHIP_*` meshsets. A group whose meshes are all the
  same size is shown one mesh at a time (FRAME).
- **Mod filter**: `store.svelte.js modNames(layer)` from the mod list's files
  and what each replaces; a texture counts for its `_LOW` / `_LITE`.
- **Layers export**: `fighterDesc(..., { parts: true })` emits one mesh per
  block or per part, the order LAYERS' `b*` / `p*` keys follow; bone
  attachments (`e*`) are not exported, as in EXPORT.
- **Capture aid**: `UMK3_LAUNCHER_PICK` takes `characters/` (the roster page),
  `special/<id>[@frame]` and a leading `~<mod>~` (filter on); the shot
  window opens with `showInactive`. The shot run from the test folder's
  `launcher\bin` found the wrong root (`launcher\bin`) and captured
  nothing; Mary asked to stop the windows and look at it herself.
- Checked in `scripts/ui-harness.js` with the test folder: all of the above.
  Packed into the test folder's `app.asar` (the old one was the 0.4.4 asar;
  no open PR changes the launcher). **Not seen by Mary yet.**


### 2026-11-12 -- ONLINE stage 3: LOBBIES; a second monitor's refresh

**A second monitor.** Mary: the game ran slow on a monitor that is not the
main one. Her main runs at 180 Hz, the others at 60. `plat_refresh_hz`
(win32_gl.c) read `GetDeviceCaps(VREFRESH)` on the window's DC, the main
monitor's, once; Direct3D 12 / Vulkan then kept every frame up three
refreshes (`rgl_swap_interval` 3), 20 frames a second on a 60 Hz monitor.
Now it asks the window's monitor (`MonitorFromWindow` +
`EnumDisplaySettings`; SDL: `SDL_GetWindowDisplayIndex`), and `pace_frame`
(game_main.c) looks twice a second and resets the interval when it changes.
Checked with `UMK3_LOG_PACE=1`, the window moved 60 -> 180 -> 60 Hz: 16.7 ms
between swaps on each.

**LOBBIES** (FrontEnd.c `PortOnlineLobbies`; plan:
[research/ONLINE-PHASE-PLAN.md](research/ONLINE-PHASE-PLAN.md), stage 3).
The list (5 s, REFRESH, ALL / MY RULES), a box for a lobby's name or key,
CREATE LOBBY (name, public / private, 2..8, the rules window in a lobby
mode with HOST ADVANTAGE, debug_menu.c `lobby_rules_open`), the room
(members' banners, key and COPY, READY, LEAVE; the host: EDIT RULES, MAKE
PUBLIC / PRIVATE, FIGHT / KICK / MAKE HOST), invites by a player's code or a
friend's. In mp_eos.c: `mplobby_set_priv`; invites sent as a P2P packet
(`MPLOBBY_INV_KIND`, handed over by mp_net.c) because
`EOS_Lobby_SendInvite` answered `EOS_AccessDenied` under this product's
client policy; a promoted host no longer writes the lobby's name, rules and
key empty; an old call in a lobby is not taken on joining. The members'
looks travel in their `look` attribute (gamecode_stubs.c
`port_online_friend_look` reads them for the portraits).

Checked with two copies of the game on one PC (`UMK3_LOBBY_TEST`, the
second with `UMK3_EOS_NEW_DEVICE=1`, its own save folder and profiles
file): create, join from the list, ready, the call, the select screen and
an online fight (player 1 and 2); invite by code, accepted, the call, the
select screen. Frames from `UMK3_SHOTS`. Not tried: kick, promote, a rules
change in a room. Mary has not played it.

**The online select screen's banners** (Mary): online_ui.c draws both
players with the ONLINE screen's banner (FrontEnd.c `PortSkinBandOf`,
debug_menu.c `ui_player_banner`) on the select and versus screens; seen on
the select screen. The fight's names are PR #209's.

## ONLINE stage 2: the EOS lobby core (2026-11-12)

Port, not in the binary. `mp_eos.c` / `mp_eos.h`, the `mplobby_*` calls
([research/ONLINE-PHASE-PLAN.md](research/ONLINE-PHASE-PLAN.md) stage 2).
One lobby at a time, in the bucket `umk3-lobby`:

- **Create** (2..8 players, public or private), **search** (this
  version's open public lobbies, or the one private lobby with a key; an
  optional exact rules filter), **join by id**, **leave** (the owner alone
  destroys it; an owner with others leaves and EOS promotes one of them),
  **kick**, **promote**.
- The lobby's attributes: `ver` (MPLOBBY_PROTO "1"), `name`, `host`,
  `rules` (online_rules text), `priv`, `key` (six letters and digits, no
  0/O/1/I), `go`. Each member's: `code`, `name`, `look`, `ready`. A private
  lobby is still PUBLICADVERTISED (joining by id needs it searchable) but
  `priv=1` keeps it out of the browser's search; its key finds it.
- What the screen asks for is kept and sent from `mpeos_tick`, one
  UpdateLobby at a time (a failed one tried again after 2 s); the three
  lobby notifications re-read the lobby, its members and their attributes,
  and bump `mplobby_serial` for the screen.
- **The owner hosts**: `mplobby_call(who)` writes `go = "<n>:<EOS user>"`;
  that member's game takes the call (`mplobby_take_call`) and `mpnet_poll`
  connects to the owner with `mpnet_find_eos` -- the JOIN goes straight to
  the owner's EOS user, no code lookup; the owner answers as the code's
  owner and hosts, as in FIND MATCH.

**Tested against the real service**: `tests/eos_lobby_probe.c` (built with
mp_eos.c alone and the SDK; not a ctest) run twice on this PC, the second
as another EOS user (`UMK3_EOS_NEW_DEVICE=1`). Public lobby and private
lobby found by its key, both `PROBE OK` on both sides: create, the
lobby's and the members' attributes seen by the other, ready, the rules
changed by the owner and seen by the member, the call taken, a P2P ping
and pong between them, the member leaving, the owner destroying it. Not
tried yet: kick, promote, the owner leaving with others in (host
migration), a private lobby kept out of a public search, a search with a
rules filter. The game builds and links with it (Windows, Linux compile
of mp_eos.c with and without the SDK); nothing on screen uses it yet --
LOBBIES in the ONLINE screen is stage 3.

## 0.4.5 beta: TRAINING's screen (2026-11-12)

- **Training apart from 2 PLAYER** (Mary). Still GameMode 6 with
  `PortPractice`, but nothing two-player reaches it: `P2Controls` 0 at
  step 43 and in ReadControls (one player's touch layout; the engine gets
  `joy[1]` 0, so player two stands still as before), the pause menu's
  layout rows are player one's, `rules_use_mode` gets 0, and the RULES
  button / key and the DUELO box are not on its select screen.
- **The moves window** (GameCode.c `PortTrainDraw`, after DrawHUD, not
  while paused): left corner under the health bars (y 56), 92 units wide,
  as tall as its rows; the portrait and name as MovesList draws them and
  one section of MovesListTab (or PortKombosRead), icons through
  DrawMoveListEntry -- so with the controls hidden they are the pad's or
  the keys' pictures (PortMovePlayers gives player one alone in
  training).
- **The input column**: right corner, the word GetReal6ButtonJoyBits
  builds for player one each tick (Task_GameMain, not paused, not in the
  round summary): a new line when something is pressed that was not down
  the tick before, else the top line counts on; arrows facing-relative
  (0x800 toward, 0x1000 away), as the moves list writes them.
- **Pause menu**: in training eight rows 40 apart (PAUSE_ROW_PITCH); rows
  7 and 8 cycle MOVES (off / specials / finishers / kombos) and INPUTS ON
  SCREEN; umk3.ini `train_moves=` / `train_inputs=`.
- Checked: built; `--fight 18 3 0` with UMK3_PRACTICE and a scripted pad,
  run from a scratch folder (not Mary's test folder), the window, the
  column and the pad's pictures in a screenshot. Mary asked for it smaller,
  the moves on the other side and clear of the health bar; done, and she
  played it: "ya está perfecto". The list is the iOS game's own (the
  special button S, its 6-button notation), not the arcade's inputs.

## ONLINE phase: the rules block; VS RULES in the local versus (2026-11-12)

The plan and Mary's decisions: [research/ONLINE-PHASE-PLAN.md](research/ONLINE-PHASE-PLAN.md).

- **Rules** (`online_rules.c`, port): ROUNDS a fight 1/3/5 (`WinsNeeded`),
  a SERIES of separate fights (one, best of 3, best of 5), TIME 30/60/99/
  infinite, a VS kode, KOMBO SLIDE, HOST ADVANTAGE (stored; applied with
  the lobbies). Hooks marked in GameCode.c (WinsNeeded, ROUND 5+, coins
  past two, the kode before `preprocessPostloadKode`, the series counted in
  QuitAsWin / QuitAsLose and shown under the clock), logic/other.c
  (`t_clock3` moves a fresh 99 down to the rule's time, from the clock's
  own digits so a rollback does the same; `t_clock4` frozen for infinite)
  and game_main.c (KOMBO SLIDE).
- **VS RULES** (Mary): on the local versus's select screen SELECT (the
  pad's, or the moves key M) opens the rules in the ONLINE look
  (`debug_menu.c` `vs_rules_*`, PortSkin pieces): five rows and ORIGINAL /
  ACCEPT; kept in umk3.ini `vs_rules=`. A line at the bottom left of that
  screen says SELECT: RULES and what they are.
- Checked: `tests/online_rules_test.c` passes (ctest `online_rules`); the
  game builds into the test folder; a scripted versus (`UMK3_TWOUP=1
  UMK3_RULES=r1;b5;m1;t30;k-1;s1;h1 --fight 15 3 0`) showed the clock from
  30 and the fight going on at 2-1 and 2-2. **Not seen**: ROUND 5, the third
  coin, the series line, and the VS RULES window and its hint. A scripted
  capture of the window jumped to screen 27 outside versus and went into
  Arcade's tower in Mary's test folder (she saw it); her saves were not
  written (their times are older). Scripted runs of the menus are not used
  there again; Mary tries the window.
- **Kodes as a checklist** (Mary): the KODES row opens a second window
  where any number are ticked (P1 HALF / QUARTER and P2 HALF / QUARTER
  exclude each other); the text is `r2`, a bit a kode (`k-1` of version 1
  still read). The lowest goes to `*theKode`, the others act where the
  binary reads the kode (`rules_kode_on`): preprocessPostloadKode's three
  flags, the blackout, the plates, both players' handicaps at once.
- **Mary's first try** found the select screen's frame rate falling
  (`begin_2d` without `end_2d` in the window and its hint: the GL matrices
  were never popped) and the hint over player one's COLOR box (moved up).
  Both fixed; **Mary played it again and it works** (2026-11-12): the frame
  rate, the hint, the kodes window.
- **The RULES button** (Mary): the hint is now a button in the select
  screen's top left corner (`vs_rules_button_mouse`, a press that begins on
  it is kept from the game's touch) with the rules under it. A key of its
  own, `PK_RULES` (CONTROLS > KEYBOARD - MENU, VERSUS RULES, `key_rules=`,
  R), and a pad action of its own (17, `pad_rules=` / `p2pad_rules=`,
  SELECT); PK_MOVES no longer opens the window. **Mary played it, it
  works** (2026-11-12).
- **DUELO against the CPU** (Mary): the binary already has it --
  Task_GameInit step 43 sets `AIOn`, and `mk3_init` starts a side whose
  model has bit 7 (`AIOn == 1`: player two) at `t_drone_begin`; the drone
  reads its difficulty from RoundParam+0x0c (0..9), which the debug menu's
  CPU DIFFICULTY already overrides. Port: `PortVsCpu` /
  `PortVsCpuLevel` (GameCode.c, umk3.ini `vs_p2=` / `vs_cpu_level=`); in
  GameMode 6 step 43 sets `AIOn = 1`, `P2Controls = 0` (ReadControls too),
  step 51 the level minus one. FrontEnd.c `p2_slot`: the box left of OK
  (PLAYER 2 / CPU n, arrows for the level, a pad's tap steps the levels
  then back to a person); player two's START (`PortP2Join`: the second
  pad's, the keypad's pause key) joins and is kept from pressing OK that
  frame. With the CPU player one picks both, as in training. A replay
  keeps it in setup word 17's bits 8.. (training stays bit 0, so the
  format and older replays are unchanged). **Mary played it, it works**
  (2026-11-12).

## 0.4.4 beta (2026-11-12)

- **Released** on Mary's word ("todo listo"): she confirmed the Lost
  Treasures each fight their own opponents (with UNLOCK EVERYTHING), chose
  ENHANCED, saw WHAT'S NEW; the game's FRIENDS works for her.

- **Mary in the test folder**: she chose ENHANCED at the first start
  (`preset=enhanced`) and saw WHAT'S NEW (`seen_version=0.4.4`); the game's
  ONLINE > FRIENDS works for her (a friend's banner and RETAR).
- **First start: YOUR ACCOUNT** (Mary): after the preset, a player with no
  account, while the server answers, is offered one -- ACCOUNT's own form
  (`ProfAccount.svelte`, `compact`: make one first, log in, recover; the
  recovery code shown; no e-mail, recovery or delete boxes) in a window,
  with PLAY WITHOUT AN ACCOUNT. Once only (`account_asked=1`).
- **The launcher's FRIENDS as the game's** (Mary's screenshot of ONLINE >
  FRIENDS): the heading bar FRIENDS (n) with + ADD FRIEND (which opens the
  code and the search), and every friend, request and invite as a banner
  (`FriendBanner.svelte`): their gradient from the server's `look`
  (icon|colour|L/D/C|colour2|angle|outfit, the fields friends.txt hands the
  game), their fighter out of its left end, the name in the heading face,
  CHALLENGE / RETAR and the rest on dark steel plates. Nothing changed in
  what the buttons do.
- Checked: `npm run ui`, `node --test` (23 pass); both seen in the UI
  harness (`UMK3_HARNESS_FIRST=1` for the first start's windows,
  `UMK3_HARNESS_ACCOUNT=1` for sample friends; the harness has no
  fighters' pictures). Packed into the test folder; not seen in Electron.

- **Shao Kahn's Lost Treasures** (a GitHub report: every treasure was a
  fight against Shao Kahn; on the device each has its own line-up).
  `GameInit_LoadABit` (armv7 `0x2b940`), step 43, has a `GameMode == 5`
  block at `0x2db2a..0x2e038` that the transcription did not have, so mode
  5 kept the `Character2 = 0x19` FE_Task_Select_Character gives it. Read
  instruction by instruction, slots resolved in `symbols.txt`
  (`0xf3814` TreasurePlayed, `0xf3544` Character2, `0xf37fc` Character1,
  `0xf3584` TowerRand, `0x10ded0` theKode):
  - 2: `Character2 = PLAYER2MODEL = 0x18` (Motaro);
  - 5: `i = (limeRand() & 3) * 22 + (limeRand() & 7)`; both fighters from
    `TowerRand[i]`, `[i + 1]`, `AIOn = 2` (both CPU);
  - 6: the opponent `TowerRand[(r1 & 3) * 22 + (r2 & 7) + 1]`, `theKode = 1`;
  - 7, 8, 9: `IsEndurance = 1`, so the four draws and InitEnduranceMatch's
    queues (`0x2d9a4`);
  - 10: `Character2 = PLAYER2MODEL = 0x17` (Noob Saibot).
  Also: the endurance path goes on at `0x2ba44`, not straight to `return
  0` as transcribed -- RoundParam+0x3c is cleared and the tower's last
  stage sets it. Checked: the oracle (`recomp.py --func
  __Z17GameInit_LoadABitl`) built, but factdiff did not finish on this
  function in 25 minutes, so the check is the reading above; built into
  the test folder; **not played** -- needs Mary's treasures (or a save
  with them) to see each line-up.
- **A click moved the fighter with the on-screen controls hidden** (README's
  known problem). `DrawControls` returns early with `hide_controls=1` but
  ReadControls still hit-tested every touch (the mouse is a finger)
  against the invisible layout. ReadControls (port lines, marked) now
  skips the dial and the touch scan for both players when they are
  hidden; keys and pads go on as before. The pause corners are a
  different path and are untouched. Built; not played.
- **Debug menu: UNLOCK EVERYTHING** (MENUS page, UNLOCKS; Mary, to test the
  treasures): the four hidden fighters' flags, `TreasureGained[0..9]` and
  `EndingsGained[0..22]` set to 1 (the value the game writes for "won"),
  then `Write_SaveData` with GameMode lent as 0 (it writes nothing in
  other modes). The log says `debug menu: everything unlocked`. Port only
  (debug_menu.c). Built into the test folder (its strings in the exe);
  not tried yet.
- **Launcher** (Mary's request): a new umk3.ini gets the primary screen's
  largest listed resolution, its aspect, borderless, `hide_controls=1`
  (`ini.fitScreen`; the default `hideControls` is now true). The first
  start asks ORIGINAL (960x640 3:2, touch controls, iPhone textures, no
  ENHANCED LIGHTING) or ENHANCED (largest resolution borderless, no touch
  controls, ENHANCED LIGHTING with Direct3D 12 / Vulkan, `max` textures),
  or KEEP MY SETTINGS when the folder had a umk3.ini (`preset=`,
  `ini.applyPreset`, `FirstRun.svelte`). WHAT'S NEW
  (`lib/whatsnew.json`) shows once per version (`seen_version=`) and from
  UPDATES. Checked: `node --test` (25, two new), both windows seen in the
  UI harness; packed into the test folder's app.asar as 0.4.4-beta. Not
  seen in Electron by Claude, not tried by Mary.

## 0.4.3 beta (2026-11-07)

- The replay player drawn with the ONLINE skin (`dbg_replay_draw`,
  loading, hint), STAGE SELECT's list up to five rows with no scrolling,
  and the Esc settings menu's black box (0.4.2's icon frame drawn in
  engine pixels) replaced by the player banner (`ui_profile_banner`).
  Claude checked all three by screenshot; Mary tried them in the test
  folder and asked for the release.

## 0.4.2 beta (2026-11-07)

- Released: the launcher renewal and PROFILE rebuild (below), the shared
  Player Banner, the icon as the favourite, Esc's PROFILE page gone, and
  STAGE SELECT drawn in the ONLINE skin (`debug_menu.c` stage_menu_draw +
  FrontEnd.c `PortSkin*`), with animations. Claude captured both STAGE
  SELECT views (`UMK3_STAGE_PREVIEW`) and every launcher section; Mary
  approved the launcher. STAGE SELECT has not been played through by Mary
  in a real VS / online pick before release.

## Launcher renewal, PROFILE rebuilt, one fighter (2026-11-07)

- Mary's brief "Renovación visual integral del launcher", then her notes as
  it went: darker, bluish metal, dark fantasy; PROFILE redone; the banner's
  colour the gradient's; that banner on the ONLINE screen too; icon and
  favourite one; the fighter chosen in a window over a darkened screen;
  Esc's PROFILE page gone; the MOD MANAGER tidied, all its functions kept.
- Theme: `theme/materials.css` (generated by `ui/gen_materials.py`),
  `theme/tokens.css` (palette), `theme/base.css` (headings, inputs,
  buttons, tabs, scrollbars). Components: Page (frame + inner scroller),
  SecHead, SubNav (one row), Arrow, CmdBar, Desc, TitleBar, ProfileBadge,
  MainMenu, Backdrop; new PlayerBanner, FighterPicker, `lib/fighters.js`.
- Not changed: any IPC, account or friends call, the gradient picker's
  logic or presets, the game's online logic. `--t` (the text unit the
  comments in tokens.css name) has never been defined, so every size
  written in it falls back to the inherited size; left as it was, the new
  pieces size in `--u`.
- Seen: Claude captured every section before and after with the capture aid
  (`WORK/launcher-shots/before`, `after`). The game built in the test
  folder with the ONLINE banner and the one-fighter change; Mary has not
  yet seen them in the game.

## ONLINE: metal banners, last polish (2026-11-07)

- A fifth texture, the sheen (`OnMakeSheen`, `OnSheen`): each pixel white
  or black with its strength as alpha, so over any colour it reads as metal
  and keeps the colour -- the Player Banner's gradient (`OnBand`), the
  buttons. `OnGoldBar` relief, lit chosen buttons, engraved `OnRule`,
  `OnPale`/`OnDim` brighter, the editor windows' fills black and their old
  blue frames bronze/gold. COMING SOON (`PortOnlineLater`) lists TAG TEAM,
  SIMUL and KING OF THE HILL.
- Mary's rule: every section keeps its UMK3 stage; the finish is laid over
  the stages, never replacing them.
- Built into the test folder; not seen on screen by Claude.

## ONLINE: materials (2026-11-07)

- Mary's brief "PULIDO VISUAL FINAL DEL MENU EN LINEA" (Deception /
  Armageddon: aged gold, bronze, black steel, stone, deep red; layout
  approved, untouched).
- `limeTextureFromRGBA` (draw_gl.c; a stub in lime_platform.c's headless
  half): a texture made in memory, cached by key like a file's.
- FrontEnd.c makes four textures on the first ONLINE frame (`OnTexture`):
  a metal bar (`OnMakeMetal`: lip, groove, engraved line, brushing,
  tarnish, scratches; also turned on its side for uprights), stone
  (`OnMakeStone`, tiling), and an ornament sheet (`OnMakeOrn`: a corner
  bracket, a sigil, a chevron, a stud -- original shapes, not the series'
  dragon). Tinted by `OnMat` into gold, bronze, steel, red, black steel.
  Used by `OnPanel`, `OnGoldBar`, `OnRule`, `OnHeading`, `OnButton`,
  `OnStatus`, the title bar and the backdrop. `OnText` draws a shadow.
- Built into the test folder; Claude has not seen it on screen (Mary uses
  the computer; no scripted clicks). The brief arrived cut at its point 8
  (the Player Banner: keep it as it is), which is untouched.

## ONLINE skin, a stage per section, the menus' click (2026-11-07)

- **Skin** (FrontEnd.c): only the drawing helpers changed -- `OnPanel`
  (black, gold frame, flared top: `OnGoldBar`), `OnRule` (a fine gold
  line), `OnHeading` (a dark pill edged in gold), `OnButton` (black edged
  in gold, MAIN oxblood, gold words under the mouse), the title bar (gold
  bars, end caps, gold arrows), the palette (`OnWhite` bone, `OnPale`
  silver, `OnDim` stone, `OnAmber` aged gold). Layout untouched. The
  references Claude could open: MK9 Gearworks (fightersgeneration), MK11
  (daanishsyed.com), MKA's Armory (mortalkombatwarehouse), MK3's stages
  (fightersgeneration); Sprung Studios' page did not load; then Mary's
  screenshots of MK Deception / Armageddon's online screens, which the
  skin follows. Unused now: `OnBrushed`, `OnBevel`, `OnRivet` (kept for the
  pieces that still call them).
- **Stages**: `PortOnlineBackdrop` draws the section's CS_BG sheet
  (CS_BG2, CS_BG, CS_BG1, CS_BG4; HOME the select screen's own), loaded
  on entering ONLINE and freed when leaving it or when a fight starts.
- **Menu clicks**: `port_menu_clicks` (game_main.c, umk3.ini
  `menu_clicks=`, the settings menu's AUDIO); `limePlaySound` skips
  `SFXHandle[26]`, the one sound every menu press plays.
- Built into the test folder; not yet seen by Mary. A test run of Claude's
  clicked in Mary's browser once (the game's window did not come to the
  front): scripted clicks are not used while she is at the computer.

## 0.4.1 beta (2026-11-07) — the ONLINE screen's finish

Released as `v0.4.1-beta` (PRs 196 and 197): the skin after MK Deception /
Armageddon, a UMK3 stage per section, steel Player Banners under the
player's gradient, AUDIO > MENU CLICK SOUND. Mary approved the look in her
test folder; she has not played fights on it.

## 0.4.0 beta (2026-11-06) — the new ONLINE screen

Released as `v0.4.0-beta` (PRs 192, 193 and 194 merged): the ONLINE
redesign's stages 1-2 with PROFILE (MY PROFILE, friends, requests), Player
Banners and the gradient window; updates that always recompile; the
intros' ambient occlusion. The gradient window (FrontEnd.c
`PortGradWindow`, `PortColourPicker`) replaced the detailed in-panel editor
Mary asked for and then turned down; its CSS is read by `OnParseCss`
(what the launcher's picker takes, strictly). Mary heard two tunes once on
entering ONLINE from the banner; the log shows one (MainMenu.mp3), and two
games were open then -- not reproduced.

## MY PROFILE: the detailed gradient editor (2026-11-06)

Audit: the launcher's GradientPicker.svelte (a dial 0..359, FROM / TO,
presets, the CSS `linear-gradient(<deg>, #from, #to)` or one colour, COPY,
CLEAR) is already ported to the game's settings menu (settings_menu.c
`gp_*`, `plat_ui_gradient`); both write profiles.ini's colour, dark (C),
color2 and angle, which the launcher syncs with the account. MY PROFILE
(FrontEnd.c `PortOnlineMe`) now has, in its panel: `OnTextBox` kinds
TB_HEX / TB_NUM / TB_CSS; FROM / TO hex boxes applied live
(`OnParseHex`); the angle by 5 and typed, with a dial; `PortGradPresets`
(Mary's ten); ADVANCED replacing the rows while open: `OnParseCss` takes
what syncFromValue takes, strictly (anything else is refused and said),
`OnGradCss` writes the CSS as gp_state does (light and dark as their top
and bottom colours at 180 degrees). DEFAULT sets only the gradient
(colour 3A8EE6, light). Persistence is profiles.ini, as before; nothing
new. Checked: built; the editor and ADVANCED drawn at 1920x1080; Mary
pressed its buttons in the test window (the log's "online ui: button"
lines). Not checked by Claude: typing a hex and a CSS, the pad.

## ONLINE: PROFILE, players online, EDIT PROFILE, the session left open (2026-11-06)

- **Sections**: `OS_HOME, OS_PROFILE, OS_LOBBIES, OS_QUICK, OS_PARTY`;
  PROFILE's pages `PS_ME, PS_FRIENDS, PS_IN, PS_OUT` (FrontEnd.c).
- **friends.txt** (main.js `writeGameFriends`) now also has
  `incoming|...` and `outgoing|...` lines; gamecode_stubs.c gives each entry
  a kind (0 friend, 1 fight invite, 2 request received, 3 request sent).
  An older game reads the new lines as friends: update both together.
- **Orders from the game** (online_invite.txt, main.js `gameAsked`): `add`,
  `accept` (POST /v1/friends), `reject`, `cancel`, `remove` (DELETE
  /v1/friends), `find CODE` (GET /v1/users, answered in online_find.txt as
  `CODE|name|look|state`), `rename NAME` (account.rename, answered in
  online_result.txt). Nothing new on the server.
- **MY PROFILE** edits a copy of struct profile (`PortDraft`); SAVE writes
  profiles.ini and, for a new name, waits for the server's answer (the old
  name back if taken). The draft's banner is the picture code "*" for
  online_ui.c (`PortOnlineDraftLook`); the gallery's fighters are "#n".
  The favourite is profiles.ini's ninth field (`fav`, "-" none) in
  profiles.c and lib/profiles.js; it never touches the icon.
- **Notices**: requests and invites seen are kept by key in
  online_seen.txt (gamecode_stubs.c `port_online_seen_*`).
- **Players online**: mp_eos.c makes a second lobby per game in the bucket
  `umk3-online` while the ONLINE screen is open and counts that bucket's
  lobbies every 15 s (`mpeos_players`, max 200). Checked on one PC: the
  count showed 0 right after the login (before the lobby was made); not
  yet checked with two machines.
- **Esc / AUDIO / VIDEO refused after ONLINE**: BACK pops the screen
  deferred, so it ran a few frames more and `startMP()` opened the session
  again; settings_menu.c's `online_now()` then refused. FE_Task_Online now
  keeps `PortOnlineLeaving` (reset when a frame is missed, PortTick from
  game_main.c) and ends the session on the way out. The debug menu is
  refused and closed while `mpnet_session_exists()`.
- **Main menu**: `PortMainBanner` (FrontEnd.c, after FE_Task_Main_Menu in
  the dispatcher) replaces set_menu_badge's badge; OPTIONS' fourth entry
  EDIT PROFILE calls `PortOnlineOpenProfile`.
- Each press of the screen's buttons is logged ("online ui: button N").
- Not checked yet: the requests with a second account, the rename's
  answer, the gallery and the editor with the pad.

## ONLINE screen: the visual redesign, Player Banners, the stats load (2026-11-06)

Mary's brief: commercial Xbox 360 / PS3-era look, profiles as console
player cards, gradients and depth; then Player Banners (the fighter as a
transparent PNG out of a horizontal banner) with the profile's own gradient
over the whole banner, and HOME's profile with statistics.

- **Drawing**: `limeFillRectGrad` (draw_gl.c; counted headless in
  lime_menu.c), a quad with a colour at each corner. FrontEnd.c's
  `OnPanel`, `OnButton` (BT_NORMAL / BT_MAIN / BT_OFF; state 1 mouse, 2 keys
  or pad, 3 pressed, from `OnArea`), `OnBand`, `OnCard`, `OnStatus`,
  `OnHeading`. The profile's gradient (`OnProfileSky`) is the sky of
  `prof_icon_rgba` (profiles.c): the custom one with an angle along CSS's
  gradient line corner to corner, the others top -> middle -> bottom; both
  linear, so corner colours reproduce it exactly.
- **The fighters**: `prof_art_rgba` (profiles.c) gives the VERSUS art alone,
  256 square with its alpha (the second outfit's via `alt_art`).
  FrontEnd.c lists what to draw (`PortOnlinePicCode/Px/Kind`) and
  online_ui.c draws it over the frame with `ui_picture_at` (debug_menu.c,
  now any width and height).
- **Stats**: the binary has one block at 0x00183c84 (`_Stats`); the port had
  two, `Stats` (counted, saved by Write_Stats) and `Stats_` (filled by
  Load_Stats, read by nothing), so every start began at zero. Load_Stats
  and the per-character play counter (GameCode.c) now use `Stats`.
  Checked: after the fix HOME shows BEST WIN STREAK 7 from Mary's save,
  0 before.
- **Invites**: DECLINE writes "drop CODE" to online_invite.txt; main.js
  drops it on the server as it does a JOIN's.
- No presence on the server, so no on line / off line for friends; the
  only states shown are real ones (inviting you, invite sent, this
  player's connection). No registration date exists anywhere the game can
  read, so it is not shown. Friends' statistics are not on the server.
- Checked by running the game (Mary's permission) at 1920x1080: every
  section, mouse, keys. Not checked: 4:3, 21:9, a pad, two machines. The
  Xbox Live Dashboard reference (gamereactor.eu) answers 404.

## ONLINE redesign, stage 2: sections and HOME (2026-11-06)

`FE_Task_Online` (FrontEnd.c) is drawn again, all of it the front end's own
drawing: `limeFillRect` panels and the game's font, laid out in the menu's
units across the whole screen (`OnX`/`OnY`, not `FE_X`, which sends x <= 90
and >= 390 to the screen's edges on wide screens). Each button is
`OnArea`: TouchAreaWH's test in those units plus `PortNavTarget`, so
pad_nav.c drives it; a click is a release after a press inside. The mouse's
place, pressed or not, is `PortMouseX/Y` (game_main.c) for the hover, ignored
while the pad's highlight shows (`PortNavHover`). The code box listens only
after a click (`PortOnlineListening` starts at 0: while it listens
game_main.c gives the keys to it, so it could not stay on by default with
the keys moving between sections). online_ui.c draws this player's badge at
`PortOnlineMePx` and the friends' at `PortOnlineFrPx`; the old box outline is
gone. Rows: six (badge slots 1..6). Mary confirmed the plan's questions:
Combo Slide is KOMBO SLIDE, EOS Lobbies for lobbies/Quick Match/party, and
the special modes' rules (the VS kodes) go in the lobby rules.

Checked by running the game in the test folder (`--screen 51`, Mary's
permission) with clicks and keys sent from PowerShell and screenshots at
1920x1080: the six sections, the hover, the sliding bar, typing
AB12-CD34-EF56, Esc, the arrows and Enter. A first build lost the Spanish
accents (escapes written through a shell heredoc came out as raw bytes);
fixed and checked (CÓDIGO, RÁPIDA). Not checked: 4:3 and 21:9, a pad, a
real search between two machines.
## Updates always reach the player (2026-11-06)

Mary: players who updated from the launcher did not get the update's
changes. `lib/update.js` apply.ps1 now stops what still runs from the
folder after its wait (launcher\bin's processes and `umk3-*` in the root)
before copying, and on success deletes `build\obj` / `build\obj64` and
writes `update\needs-build` (apply.sh: the same, `kill -9` after its
minute). `main.js` exits with `app.exit(0)` after killing the game it
started. `update.needsBuild` makes `startGame` and `replays:play` answer
`{ rebuild: true }`; the page (`rebuildAfterUpdate`, app.svelte.js) goes to
BUILD and compiles; a successful `build:start` deletes the mark.
`afterUpdate` also compiles when only the mark is there (the launcher opened
by hand). Checked: launcher tests (21 pass, a new one for the mark); the
generated apply.ps1 parses with PowerShell's parser; in the test folder,
with `update\needs-build` written, the new launcher compiled on opening and
removed it. Not checked: a real update from GitHub with the forced close.
## Unreleased (after 0.3.1b)

Built in Mary's test folder; she played it on Direct3D 12 and confirmed the fix.

- **AMBIENT OCCLUSION in the intros, again**: the stage's occlusion was
  still painted over the fighters in some intros (Mary: Motaro on the
  balcony). In those intros the fighters are drawn blended, and the
  occlusion's depth pass skips blended draws, so the fighters' pixels read
  the occlusion of the wall behind them (the diagnostic log said
  `fighters' draws 1 (blended 1, ...)`). The fighters are now a layer of
  their own: the depth pass marks them in the alpha channel, the occlusion
  pass and the blur only mix samples of the same layer, and a fighter's
  pixel only takes the fighters' occlusion -- none at all when the fighter
  is not in the depth (`rgl_ff.c` `ao_passes`, `shaders/ff.frag`,
  `shaders/ff.hlsl`). `tests/rgl_test.c`: OpenGL passes; Vulkan and
  Direct3D 12 fail the same 5 shadow checks as before this change (checked
  against a build of main).

## ONLINE menu redesign, stage 1: audit (2026-11-06)

Mary asked for a full redesign of the ONLINE menu (HOME, FRIENDS,
INVITATIONS, LOBBIES, QUICK MATCH, PARTY; lobby rules; matchmaking). Stage 1
is a read of what exists, in
[research/ONLINE-MENU-AUDIT.md](research/ONLINE-MENU-AUDIT.md). No code
changed. Findings worth knowing without reading it all:

- There are no lobbies or matchmaking today: the EOS lobby is a 2-member
  directory entry named after the player's code, which nobody joins.
- Best of 1/3/5 is `WinsNeeded` 1/2/3 (GameCode.c, Task_GameInit); the
  round banners stop at "ROUND 4", so Best of 5 needs a fifth.
- Combo Slide is the port's KOMBO SLIDE (issue #114), forced to the
  original's rule online today (`game_main.c`).
- The binary's VS kodes (table at 0x00176c04: blocking off, throws off, dark
  kombat, power handicaps...) are the rules the engine already has.
- With rollback the host has no frame advantage; it has player 1's side and
  the stage. The frame advantage exists only with `UMK3_NETCODE=classic`.
- A version or setup mismatch is only logged (`rollback.c`), never blocks.
- Two of Mary's reference links return 404 (gamereactor, postimg 1f0w4wn3).

## 0.3.1b beta (2026-11-06) — frame time and LOW LATENCY

Published 2026-11-06 as the GitHub pre-release `v0.3.1-beta.b`: the fix
below and the two entries after it (no plain 0.3.1 was published). Played
by Mary in her test folder (Direct3D 12).

- **AMBIENT OCCLUSION in the intros**: the occlusion of the stage was
  painted over the fighter (Mary: Ermac in the graveyard). The intros clear
  the depth before drawing the fighters, so they stand in front of the
  stage whatever is nearer; the occlusion's depth pass skipped that clear,
  so where a piece of the stage was nearer than the fighter, the depth was
  the stage's and so was the occlusion. The pass now keeps the clear
  (`rgl_ff.c`, `ao_passes`; the log says `ambient occlusion: a depth clear
  between the scene's draws, kept`, seen in Mary's run). Also guarded, not
  seen: a fighter with a projection or viewport of his own goes into the
  depth from his own draws, each placed by its viewport.

## LOW LATENCY, Sindel's smooth idle, the moves list's mod portrait (2026-11-06, after 0.3.0)

- **Input latency** (port): the frame loop already polls the window's
  messages, reads the pads and runs the tick in one go after
  `pace_frame()`'s sleep, so nothing queues on the input side. What queued
  was the present: Direct3D 12 let DXGI hold up to three (its default
  frame latency; `wait_gpu` waits for the GPU, not the display), Vulkan's
  FIFO up to images - 1. `rgl_low_latency` (`low_latency=1`, settings menu,
  launcher): `rgl_d3d12.c` makes the swap chain with
  `DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT` (kept through
  `ResizeBuffers`), `SetMaximumFrameLatency(1)` (3 when off) and, with
  VSYNC, waits on the latency object after `Present`; `rgl_vk.c` enables
  `VK_KHR_present_id` + `VK_KHR_present_wait` when both are listed (falls
  back to the plain device if creation refuses them) and waits for the
  last present of a frame; `sdl_gl.c` adds `glFinish` after the swap. The
  swap now returns at the refresh on all three, which is what
  `pace_frame()` assumes. Design: `docs/research/INPUT-LATENCY.md`.
- **SMOOTH ANIMATIONS, Sindel's idle**: `port_smooth_pose` (GameCode.c) no
  longer applies `IsLiaProblem` (0x0005ba98). Its frames are, from
  `framelists/sindelframes.txt`, 241 LIWALK1, 248 LIWALK8, 249 LIWALK9,
  257 LIASTANCE1, 261 LIASTANCE5. A diagnostic run on Mary's replay showed
  101 of 132 changes blended, none refused, after the change. Mary: the
  idle and the walk look right.
- **Moves list portrait**: `load_picture` (draw_gl.c) asks
  `mods_find_texture` first for `Textures/` pictures, as `load_rgba` does.
  `PortMvPreloadPortraits` (GameCode.c) loads both fighters' at the end of
  Task_GameInit, before `DeleteLoadingScreenTexture`; the slow-frame line
  showed 42-100 ms on the first open with HD NINJAS' 1080x1080 PNGs.
- Seen in the same logs, not changed: opening the settings menu took
  ~150 ms over two frames (Mary reports it gone in 0.3.1b; nothing was
  changed for it, cause unknown); one 931 ms tick in a fight with nothing
  logged around it.

## After 0.3.0: frames back under 16.7 ms (2026-11-06)

Mary saw stutters in SMOOTH ANIMATIONS (Sindel) and drops in fps: fights
ran 17-19 ms a frame, and every frame past 16.7 ms runs two ticks, which
the frame blending shows. Two costs of ENHANCED LIGHTING cut
(`rgl_ff.c`): smooth normals are no longer summed every frame for a mesh
past 20,000 triangles (SMOOTH MODELS' level 2: Sindel 60,000) -- the
shaders take each face's from the derivatives, the same on so dense a
mesh; and the point lights' shadows send each cube face only the
triangles it sees, not the fighters six times a light.

## 0.3.0 beta (2026-11-06) — ENHANCED LIGHTING

Published 2026-11-06 as the GitHub pre-release `v0.3.0-beta`. Played by
Mary in her test folder through every step below (Direct3D 12); Vulkan
built and run by the same code, OpenGL without any of it.

**ENHANCED LIGHTING** (Vulkan and DirectX 12; `rt_shadows=1`, the GRAPHICS
page and the launcher), everything in the entries below since 0.2.1:
real-time shadows; relief and specular on every texture, the game's and a
pack's; mipmaps; every glow a coloured light, the two strongest casting
the fighters' shadows; each stage lit by its sky; per-stage lighting files
(`lighting/<scene>.ini`); emissive materials; AMBIENT OCCLUSION (on/off).
Also: PVRTC's block grid smoothed (every renderer); every fighter's
KOMBOS in the moves list; the launcher no longer drops the game's own
settings.

Lighting files ship for Kahn's Kave, The Rooftop, The Bridge, The Bell
Tower, Jade's Desert, The Graveyard, The Balcony and The Temple; the rest
are lit as measured.

## Since the last entry: AO over everything, emissive materials, The Temple and The Balcony (2026-11-06)

- **AMBIENT OCCLUSION** fixed and made visible. Mary's debug views (the
  debug menu's AMBIENT OCCLUSION VIEW: the occlusion after and before the
  blur, the depth, the normals, the final contribution) showed the depth
  and normals right and the occlusion all but empty: the radius was given
  in world units, but the occlusion works in the camera's, which the
  game's view scales about 5x -- it was a fifth of what it meant. Now
  converted by the view's own scale (`ao_radius` 0.6 world units); HBAO
  takes at least four directions (two left most pixels blind); the
  result is squared; it is applied last, over the dynamic lights and the
  fighters' light, a fighter as much as the stage. One on/off option in
  the menu and the launcher (`ao=` still picks Crytek/Alchemy by hand).
- **Emissive materials**: the game's fullbright meshes and a stage file's
  `emissive=` ones are drawn self-lit (no shadow, occlusion or tint) and
  each lights its surroundings in its texture's colour (`rgl_ff.c`, the
  `lime_port_mesh_fullbright` hook). Kahn's Kave's lava and fires.
- **Specular maps** were already there, made from each texture as the
  relief is.
- **The Temple**: `relief=1.6` (a new key), its four braziers warm on the
  fighters (the middle two with shadows), the openings behind its arches a
  cool green. **The Balcony**: its torches stronger.
- **The startup stall** (the GPU seconds on the first frame: the driver
  finishing the shader): the HLSL loops are no longer unrolled, the DXBC
  38% smaller. Shorter; may not be gone.
- Jade's Desert (warm daylight) and The Graveyard (night, moonlight)
  lighting files; The Bell Tower's windows are the night sky.

## AMBIENT OCCLUSION; The Bridge's and The Bell Tower's lighting (2026-11-06, after 0.2.1)

Part of ENHANCED LIGHTING (Vulkan, Direct3D 12). Mary played it: no lag
once made cheaper.

- **AMBIENT OCCLUSION** (`umk3.ini ao=0..3`, the settings menu's AMBIENT
  OCCLUSION row: off, Crytek SSAO, HBAO, Alchemy AO; HBAO by default),
  after `research/AMBIENT-OCCLUSION.md`, written from the papers. Each
  flush (`rgl_ff.c` `ao_passes`): the scene's distance into a target at
  half the 3D resolution -- the stage's held opaque draws, and the
  fighters from their base meshes (the triangles held for the key light's
  map; sending the smoothed ones again cost ~3 ms a frame, Mary saw the
  drop) -- then a full-screen occlusion pass and a 5x5 blur that keeps to
  its own depth. Every lit draw darkens its own colour by it (never the
  dynamic lights'), a fighter by 70%, less where the map's shadow already
  is, never below 0.40. Keys: `ao_radius` (1.0), `ao_intensity` (1.8),
  `ao_bias`, `ao_samples` (8), `ao_blur`, `ao_scale` (0.5), `ao_floor`.
  The backends got a fourth texture, the screen map (`screen_map`: Vulkan
  binding 4, D3D12 t3); the light block carries the projection, the
  viewport and the settings.
- **The Bridge**: its sky (`skyplane`) and far skyline (`4`) without
  relief: on the painted clouds it looked lumpy.
- **The Bell Tower**: light the colour of its picture, not its sky (warm
  amber), the night sky's cool blue through its three windows, and a warm
  light on the wooden floor from above.
- **The startup freeze** came back (3.6 s in one present before the main
  menu, all in running the frame's commands, no pipeline made). The D3D12
  diagnostic now splits it into the texture uploads since the last
  present (count, lists, MB, time run and waited for, time releasing).
  Not fixed yet.

## Each stage's sky, lighting files, smoother PVRTC (2026-11-06, after 0.2.1)

Part of ENHANCED LIGHTING unless said. Mary played each step: The Rooftop
reddish, Kahn's Kave toned down, the select screen back to normal, the
sky's grid gone.

- **Each stage's light from its own sky** (Mary: The Rooftop's red sky).
  The stage's far draws give its sky's colour, each weighed by its size
  times its saturation squared, so the dominant hue wins over a grey
  skyline (`rgl_ff.c` `sky_note`, `sky_build`). It tints everything the
  lights reach (stage and fighters alike), the hue at white's brightness
  mixed in at 0.80 from above and 85% of that from below; a grey sky adds
  nothing. Only while a stage's `_LEVEL_SCENE` is loaded
  (`lime_port_meshset_event`): the select screen's fighters came back red.
- **A dark stage lights its fighters slightly** (Mary: 0.1-0.2): up to
  0.15 of their texture's colour as the stage's mean brightness falls
  from 0.08 to 0.04 (linear).
- **Lighting files per stage**: `lighting/<scene>.ini` beside the game
  (shipped in `launcher/lighting/`, copied at extraction and in the
  release; `README.txt` has the keys): `sky_strength`, `sky_color`,
  `fighter_lift`, `glow_power`, `no_relief=<meshes>`, and up to 8
  `[light]`s (point, spot, directional) placed in the world or anchored to
  a stage mesh by name -- RenderMesh.c names each mesh as it is drawn
  (`lime_port_mesh_name`), and the log lists them with their places as a
  stage loads. `shadows=1` gives a file's point light the fighters'
  shadows ahead of the glows. Kahn's Kave: sky at 0.45, fighters' light
  0.15, a warm light at the lava, a spot on the throne. The Rooftop: sky
  at 1.5, its backdrop and swirl without relief.
- **The fighters react more to the glows' colours** as they near them:
  their response 90% (was 60%), and the falloff softer (1 + 4x^2).
- **PVRTC's block grid smoothed** (all renderers, with or without
  ENHANCED LIGHTING): Mary saw a grid of lines in The Rooftop's sky; it is
  `ROOFTOP_BACKDROP.pvr`'s own 4x4 compression blocks. At decode,
  `lime/pvr.c` `pvr_deblock` blurs (binomial 5x5) where a 5x5 window's
  colour range is under 10/255, fading out by 26: gradients only, never an
  edge.
- **The startup freeze** Mary saw (a 2.5-5 s present before the main
  menu, three runs) did not come back; `rgl_d3d12.c` prints a
  `d3d12 slow present:` breakdown and any pipeline compiled in play,
  should it.

## The other 11 fighters' kombos in the moves list (2026-11-06, after 0.2.1)

Mary played it: works.

- **The "entry kinds" 0x6 / 0xc were not kinds.** The low byte of an
  entry's first word is the input window in frames: `t_comb0` (0x00032c90)
  reads it with `ldrb` as the length and waits `n * 3 / 2` frames for the
  next button. Every tree uses 6, 9 or 0xc. With that check dropped, all
  23 fighters' trees decode.
- **Two real special cases**, both from the binary: `+4 == 0x1111` means
  +0x14 is a routine, not a list (`t_comb2`, 0x00032958); the walk ends the
  string there (only Sonya and Nightwolf have one, and both have their own
  section 3). A list that leads back to one already on the path (Sub-Zero's
  `ct_sz_elbow3`) ends the string too.
- **Duplicates:** a list can hold one button twice (Mileena's U+ kombos;
  combo_scan_a11 takes the first, the port's UP rule reaches the second).
  Rows are compared after mapping to the atlas codes and shown once, which
  also merges HP/LP rows on the 5-button layout.
- `PortKomboFromEngine` (Cyrax only) is gone: every fighter whose
  `MovesListTab` entry has `rows3 = 0` uses the trees. At most 8 rows
  (Kabal), which fits `PORT_KOMBO_ROWS`.

## Cyrax's kombos in the moves list (2026-11-06, after 0.2.1)

Mary played it: works.

- **Cause:** `MovesListTab[8]` (0x0010d918 + 8 * 52) has `rows3 = 0`, as in
  the binary, so `PortMvAllPage` drew "-". 12 of the 23 entries have no
  section 3. The table, `Cyrax_Moves5/6` and his GameText ids
  (0x1e2-0x1ee, 0x2f7-0x303) were checked against the binary and
  `english_text.dat`: all match.
- **Fix:** `PortKombosRead` (GameCode.c, port) walks the engine's kombo
  trees: `ochar_elbow_combos[c]` (0x001663fc, the list after a landed high
  punch, `t_do_elbow`) and `ochar_knee_combos[c]` (0x0016645c, high kick,
  `t_do_knee`), the 0x18-byte entries `combo_scan_a11` walks (+0 bit 15 ends
  a list, low bits 9; +4 animation >> 8; +8 low byte the button in
  `last_switch_ram` order, 0 HP 1 LP 2 BL 3 HK 4 LK 5 RUN; +0x14 the next
  list when > 0x100). Each path is one kombo. The button order was checked
  against the trees of Sonya, Jax, Jade and Scorpion, whose kombos the
  binary's lists do print: same buttons, same order. Animation 0x15 on a
  kick (`ct_*_roundh*`) is shown as B+HK, as those lists print it; the scan
  itself reads buttons only.
- Cyrax (`ct_cyrax_*`, 0x1660e4-0x1661a4): HP HP LP; HP HP HK HP HK HK;
  HK HK B+HK. Only Cyrax is switched on (`PortKomboFromEngine`); other
  fighters' trees hold entries of kind 0x6 and 0xc (and Sub-Zero's loops),
  not decoded, and a walk that meets one shows "-" rather than a guess.

## DYNAMIC LIGHTS' shadows, phase 5 (2026-11-06, after 0.2.1)

Mary played it: everything looks right, though the change is subtle.

- The two strongest point lights (power times reach) cast the fighters'
  shadows all round, as a cube map: six views a little over 90 degrees
  (tan 1.05, so a filter does not run off a face) into 512-texel tiles of
  one target, three across and two rows a light (`rgl_ff.c`
  `point_shadows`). Only the fighters cast, on the stage and the other
  fighter; a fighter skips his own texels. The texel is the key light's
  (packed depth + caster id), so the same shader mode writes it.
- The shaders (`point_shadow`) pick the face the point lies most along,
  turn its depth back into a distance, and take nine taps kept inside the
  tile.
- Backends: a third texture (`shadow_map2`: Vulkan binding 3, D3D12 t2).
- The log says `dynamic lights: point shadows on` the first time.

## DYNAMIC LIGHTS, phases 1-2 (2026-11-06, after 0.2.1)

Part of ENHANCED LIGHTING (Vulkan and Direct3D 12). Mary played it: "se ve
bien".

- **Every glow is a light.** Each additive draw of the scene (a fireball, a
  glow, sparks, a teleport) becomes a point light of its own colour where it
  is: the texture's mean colour (`tex_avg`, measured at upload) times the
  draw's vertex colour. Draws close together are merged, up to 16
  (`rgl_ff.c` `emit_note`, `lights_build`). Radius 6x the draw's size,
  2.5-12 units; power 2.5. The log prints a `dynamic lights:` line every
  600 flushes with the count, the first light's place, radius and colour.
- **Per pixel, on the stage and the fighters**, added over the game's own
  light (Blinn-Phong in linear light, a smooth window to 0 at the radius,
  spot cones by smoothstep); a stage with no glows looks as before. Spot
  and directional lights are in the shader and the block but nothing places
  them yet (a stage's lighting file, phase 4).
- **Normals.** The stage's are the `.meshset`'s own: the 12 bytes the loader
  skipped after u, v are unit normals matching the faces (checked over the
  stage and fighter files). Kept outside the engine heap, keyed by the
  mesh's vertex block (`RenderMesh.c` `pn_keep`), sent with
  `glNormalPointer` only while ENHANCED LIGHTING is on
  (`lime_port_dyn_normals`), so the GL-trace tests see the original calls.
  The fighters' light is baked per frame, so their smooth normals are made
  from their triangles in `rgl_ff.c` `tris_indexed` (a mirrored fighter's
  winding flipped by the matrix's determinant).
- **Renderer**: `rgl_vtx` gained the world normal and position (68 bytes);
  `rgl_frag` gained `lit`; the light block (`rgl_light_block`, 1088 bytes)
  is a dynamic uniform buffer in a ring in Vulkan and a root CBV in the
  upload buffer in Direct3D 12 (`lights` backend call).

Not yet: shadows from these lights (phase 5), each stage's sky colour,
lighting files per stage and fighter, OpenGL
([research/DYNAMIC-LIGHTING.md](research/DYNAMIC-LIGHTING.md)).

## ENHANCED LIGHTING (2026-11-06, after 0.2.1)

REAL-TIME SHADOWS became **ENHANCED LIGHTING** (the same `rt_shadows=1`
in umk3.ini, so a setting already on stays on), Vulkan and Direct3D 12.
Mary saw it in her test folder: the stage relief, the far textures without
moire, and the backdrop with no seams. Not yet played as a whole.

- **Relief on every texture** (Mary, after NormalmapGenerator and DK's GTA
  SA normal map plugin): the shader takes each texture as its own normal
  map as it draws -- its brightness a height, its Sobel slopes the tilt,
  plus the slopes two mip levels down for the large shapes -- the game's
  textures and a pack's alike, with nothing to prepare. Each texture's mean
  slope is measured once at upload (`rgl_ff.c` `tex_bump`) so a flat iPhone
  texture and a busy HD one come out at the same depth; a flat colour gets
  none. The stage at 1.5, a fighter's skin at 0.15 (their light is painted
  in). Not on what adds light (glows, sparks).
- **Specular** (Blinn-Phong, after the stackgl lighting walkthrough): a
  glint where a bump turns to the light, its specular map the texture's own
  (bright and grey shines; dark or strongly coloured does not); and the
  relief and the glint worked in linear light (gamma 2.2).
- Both in **the texture's own space**, lit from a light fixed there: a
  first version took the normal from each triangle's flat face and shaded
  every face on its own, a seam at each edge (Mary saw them in a backdrop).
- **Mipmaps on every 3D texture** (Mary: a far building broke into moire):
  the levels are made for every texture, and the 3D scene samples them
  trilinear and 16x anisotropic though the game asked for none. Not a
  texture changed in place (its levels would be stale).
- **The launcher kept losing the game's own settings** (Mary: "the
  launcher's settings are not saved"): every change there rewrote
  umk3.ini from the keys it knows, dropping the ones only the game writes
  (`p2key_pause`, `p2key_moves`, `p2key_ok`, `replay_scale`). `lib/ini.js`
  `save` now keeps every line it does not write.
- `rgl_frag` gained `bump` (20 bytes of push / root constants); the
  shaders were rebuilt with `shaders/build.py`.

Next: real-time dynamic lighting (point and spot lights, their shadows, on
the stage and the fighters), planned in
[research/DYNAMIC-LIGHTING.md](research/DYNAMIC-LIGHTING.md).

## Settings in one order, pause menu, corner buttons (2026-11-05, released in 0.2.1)

- **The game's panel** (FrontEnd.c `PortControlsPanel`, settings_menu.c
  `g_framed`): VIDEO SETTINGS (`set_menu_open_video`, every category but
  AUDIO) and AUDIO SETTINGS (`set_menu_open_audio`, from the SETTINGS
  screen's one AUDIO tile, which replaces `drawPage2x2BigForSettings`' two
  volume tiles) turn into each other with L1 / R1. `framed_columns` lays the
  rows out in columns of FR_COL (15) that start at a heading; two at a time,
  the pair with the selection, and a scroll bar beside them. A bar row
  (`"\003"`) is drawn as a well with a red-to-gold fill there too, its text
  in ASCII (the font has no degree sign).
- **Bars** (ui_alert.c): the label and the value on the row's upper part,
  the well under them from UI_BAR_X0 0.04 to UI_BAR_X1 0.96;
  `ui_geo_bar` takes the row's lower half only.
- **Pause menu** (GameCode.c, the six rows): with `PortHideControls` rows 3
  to 6 are MOVES LIST (`GamePaused = 2`), BUTTON SETUP, AUDIO SETTINGS and
  VIDEO SETTINGS. Online they do not open (`online_now`).
- **`corner_buttons=`** (`port_corner_buttons`, game_main.c, outside the
  save states): DrawHUD skips INFO and PAUSE; `TogglePauseMenu` ignores a
  touch unless `hud_keys` made it (`PortKeyTap`, 8 frames).
- **Moves list:** two pages (`MoveListPage ^= 1`), this side's fighter then
  the other's; the header is `PortMvPortrait` (IPAD<NAME>PORTRAIT.PNG via
  `lime_load_res_picture`, draw_gl.c, outside the game's memory), the name
  in gold, `P1 / 5 BUTTONS`. `PortMvText` and `PortMvSpacing` shrink what
  does not fit a column. Checked at 4:3, 16:9 and 21:9.
- **Invites:** `port_online_tell_pending` (gamecode_stubs.c) is 1 while
  online_invite.txt is there; once the launcher has taken it, SENT ends a
  second later.
- **Launcher:** SECTIONS has no `controls`; GFX_TABS ends in `ctl`, which
  shows SecControls (`embedded`, `Page bare`); left / right walk CONTROLS'
  own pages first. ini.js `cornerButtons`.
- **docs/SUBZERO-ICE-BALL.md:** how Sub-Zero's Ice Ball is programmed, from
  `t_do_sz_forward_zap` to `t_r_freeze`.
- Not checked by script: the launcher's new SETTINGS (Mary), the stage
  list's scroll bar.

## Online after 0.2.0, stale objects, moves list (2026-11-05, released in 0.2.1)

- **Rollback off:** Mary's online logs since the 11:33 builds had
  `savestate: arena full at 1043940768 bytes; save states off` at tick 3 one
  boot in two, then `rollback: NO SNAPSHOTS`, 0 rollbacks and hundreds of
  holds. The 84 sheets read ahead (draw_gl.c `pf_work`) decode at once, and
  their decoders' malloc (runtime/lime, a state file) was the arena's, whose
  power-of-two classes never merge. `st_heap_this_thread` (savestate.c, a
  `__thread` flag) sends those threads to the C heap. Six boots: no arena
  full.
- **Desyncs:** every hash check (one a second) had differed since Mary's
  30 October fights, against the same Windows player; on 29 October none
  did. Two games on one PC over the LAN (`--screen online`,
  `UMK3_ONLINE_FIND`, scripted keys; the guest in a folder of its own) gave
  0 desyncs over 3,600 frames, and a recorded fight replays the same 1,714
  ticks in another process: the game is deterministic on one build. The
  likely cause is the other player's build: compile.js kept an object when
  no source in its .d was newer, and an update's sources carry the
  release's dates, so after an update their exe mixed versions (the same
  player's later build failed on `rb_sound_ok`, defined in a new
  rollback.c their old rollback.o lacked). compile.js now writes a .stamp
  beside each object, every dependency's mtime and size, and keeps the
  object only on an exact match; a failed link deletes the .rsp files.
  Tested: a changed rollback.c dated 2020 is compiled again.
  `-ffp-contract=off` for all, and `-mcpu=x86_64` on Linux, where zig took
  the host CPU (AVX2, FMA) without a `-target`; Windows' `-target` was
  already the baseline. Not yet confirmed by an online fight between two
  machines.
- **Settings menu online:** `set_menu_toggle` / `set_menu_open_controls`
  refuse to open while `GameMode == 1` or an online session exists.
- **`--screen 51`** (the launcher's invites) read as -1: `parse_screen`
  allowed numbers below DBG_SCREENS (51) only.
- **Moves list:** `MovesList` (GameCode.c) page 0 is `PortMvAllPage`: three
  columns, sections 1 and 2 of `MovesListTab` and the third section
  (`rows3`, the kombos, after the other two in the same table; named KOMBO
  1..n), each entry centred by measuring it first (`PortMvMeasure`,
  `PortMvScale` in `DrawMoveListEntry`). Pages 1-3 are the basic moves; the
  original's two section pages (`DrawCharacterPage`) are no longer reached.
  Checked with Sonya and Jax at 1920x1080.
- Not found: "the X button over START when joining" (Mary); no screen
  showed it in the scripted LAN runs. Possibly the broken `--screen 51`.

## KOMBO SLIDE, AUDIO, the profiler (2026-11-05, released in 0.2.0)

- **KOMBO SLIDE** (`port_kombo_slide`, kombo_slide=): `PortNoKomboSlide`
  (mkreact.c, set by game_main.c each tick, offline outside replays only)
  makes `t_ccp3` take the plain pop instead of `t_separate_us`, and
  `away_x_vel` (other.c) drop a hit's push once the punch count (field4c)
  is 2 or more. `kombo push:` lines in the log. Mary, Liu Kang: works.
- **AUDIO**: `port_music_pct` / `port_sfx_pct` (lime_menu.c) over the
  game's own volumes; the bar rows (`"\003" + 000..100 + text`, drawn by
  ui_alert.c, `ui_geo_bar` / `ui_bar_value` in debug_menu.c, the well
  UI_BAR_X0..X1 of the row); SOUND OUTPUT and GRAPHICS CARD rows
  (`plat_audio_names`, `rgl_gpu_names`), at the next start. Launcher:
  GRAPHICS > AUDIO (AudioSliders.svelte), music_volume= / sfx_volume=.
- `UMK3_PROFILE=1` (win32_gl.c): the game's thread sampled every ms; a
  slow frame's commonest places as exe offsets in the log.

## Frame drops, load times, devices (2026-11-05, released in 0.2.0)

Mary: the frame drops other players see in fatalities and hits, the load
times, the renderers, the Steam Deck controls; then her three performance
briefs. Measured in scripted fights on her PC (`UMK3_ARCADE=0,0` so a
`--fight` has a FINISH HIM; `UMK3_DBG_KEY="600:0;1100:0"` two KOs;
`UMK3_DBG_FIN="1000:2"`). The audit and the roadmap: [PERFORMANCE-AUDIT.md](PERFORMANCE-AUDIT.md).

- **The hits' drops were the session log.** `open_session_log` set stdout
  unbuffered: every printf a disk write, ~4 ms; a hit prints its sounds and
  voices. Now a 64 KB buffer, flushed once a frame and first thing in
  `on_crash`. Before: 485-578 slow frames in a fight; after: 6.
- **SMOOTH MODELS stencils** (`umk3_subsurf.c` `umk_subsurf_make_stencils`):
  the surface and the correction pass's vertex points are linear in the
  welded control points; probed with a distance-2 colouring of the faces'
  one-ring supports (27-34 colours, three per probe), checked against the
  two syncs on a pose of its own (3.0e-7 relative) and dropped if it does
  not match. Built on the build thread (13-30 ms); dropped and rebuilt with
  `umk_subsurf_set_sharp`. `port_tick_unseen`: catch-up ticks draw plain.
  `port_smooth_wait`: a fight's second frame waits for the builds the first
  started.
- **Boot** (`draw_gl.c` read-ahead): `umk3-preload.txt` (sheets over 15 ms)
  decoded on up to 8 threads; `lime_par_for` runs a second caller's job
  alone; **the save-state arena's allocator has a lock** (`savestate.c`):
  `runtime/lime` is a state file, its decoders' malloc is the arena's, and
  unguarded the boot crashed 4 times in 5. A C-heap free skips the lock
  (the smoothing builds free thousands).
- `no_power_throttling` (win32_gl.c): EcoQoS off for the process.
- **Buttons** (`game_main.c`): the launcher's `buttons=` is applied once at
  boot; after that a change from the game's own menus is the layout and is
  written to umk3.ini (`port_ini_put_buttons`, settings_menu.c).
  `port_buttons_set` sets `Player1NumButtons` too (not online).
- **Babality** (GameCode.c, after `ArcadePosTo3dPos` in the fighters' draw):
  checked against armv7 `_t_turn_into_a_baby` 0x4fe40 and
  `_ArcadePosTo3dPos` 0x1c464 instruction by instruction, and the baby
  frames' boxes (`_FrameInfo` 0x10df1c; `_FrameInfo2` is all zero and never
  written) byte by byte: the binary places the baby at floor less its arcade
  sprite's height (44 Sub-Zero/Scorpion/Kano ... 60 Sindel). Drawn lower by
  (h - 44) / 64; the logic keeps the binary's position. Port, not in the
  binary.
- **Devices**: `gpu=` (rgl_front.c `rgl_gpu_match`; rgl_d3d12.c orders the
  adapters by name match then dedicated memory; rgl_vk.c +10 for a match;
  `NvOptimusEnablement` / `AmdPowerXpressRequestHighPerformance` for OpenGL),
  `audio_device=` (win32_audio.c waveOut by name, sdl_audio.c
  `SDL_OpenAudioDevice`), `--list-gpus` / `--list-audio` ("gpu<TAB>name",
  "audio<TAB>name"); the launcher's GRAPHICS > VIDEO lists them.
- Debug menu MODE / CPU DIFFICULTY (`dbg_fight_mode`, `port_cpu_difficulty`
  over `RoundParam[3]` in `Task_GameInit`); `UMK3_CPU_DIFF` for scripts.
- Checked: `tests/rgl_test.c` on OpenGL, Vulkan and Direct3D 12 (0 failed);
  `tools/ci_compile_linux.py` (85 of 85); 8 boots in a row without a crash.
- **Not checked:** the Steam Deck itself; the fights' remaining 1-tick
  spikes at a round's start (100-500 ms) vary from run to run with the test
  window in the background -- Mary to look in front.

## FRIENDS in the game's ONLINE menu (2026-10-31, released in 0.1.23)

Mary: the invites belong in the game's ONLINE menu, with the friends' names
and icons, and JOIN there. Played: works.

- **Launcher** (main.js): `writeGameFriends` writes `friends.txt` beside
  umk3.ini with every `pollFriends` answer ("invite|CODE|name|look",
  "friend|CODE|name|look"; removed for a guest); `gameAsked` reads the
  game's `online_invite.txt` every two seconds while it runs ("invite CODE"
  -> `POST /v1/invite`, "join CODE" -> `DELETE /v1/invite`).
- **Server** (private repo 3fdcda8): the friends' and invites' rows carry
  `look`, the synced profile's fields after the name. `deploy.cmd`: the
  tests, then `wrangler deploy`.
- **Game:** gamecode_stubs.c `port_online_friend` / `_look` / `_tell`;
  FrontEnd.c `PortOnlineFriends` (four rows, invites first; INVITE writes
  "invite", JOIN writes "join" and finds the code as FIND MATCH);
  online_ui.c draws each row's badge (slots 1..4; `ui_badge_at` now has 8).

## 0.1.22: invites, friend requests, the late press's sounds, mods' bones online (2026-10-31, released in 0.1.22)

Mary: a friend's name was not found; she wanted notices of requests,
REMOVE EMAIL and the recovery code in a window; Nightwolf "mute, only air"
when he hits; the guest's screen black after an online fight.

- **Not found:** the friend's account had no `pcode` / `pname` (his
  launcher predates 0.1.21, `claim()` never ran). The Worker now takes both
  from every `PUT /v1/progress` (`adoptProfile`), and migrates the database
  itself on the first request after a deploy (`migrate()`: columns,
  tables, then the profile of every account that lacks one). Private repo,
  commit a536d7c; no migration is run by hand any more.
- **Recovery code:** kept sealed (`accounts.recovery_seal`), `GET/POST
  /v1/recovery`; the launcher shows it in its own window (main.js,
  `recoveryWindow`). Accounts older than this have none: a new one for the
  password.
- **Requests:** `pollFriends` (main.js) every minute; `account:requests`
  sets `app.frIn` (the red dot, MainMenu and SubNav `dot`).
- **"Nightwolf mute":** an online fight, Nightwolf the other player's. His
  attack voice (voice group 2, sounds 119/120, Dfatt1/2) was asked for 32
  times and played 0: the frame that brought his late press only ever ran
  inside a rollback, and `limePlaySound` skipped every sound while
  `rb_quiet`. `rb_sound_ok` (rollback.c) keeps the sounds each frame played;
  a frame run again plays the new ones. His character sound 1 (`st_indian`
  row 1) is "" in the binary too (0x17a7cc), left as it is.
- **Desyncs:** Mary's 2026-10-30 fight: 78 desyncs; the day before, none.
  Her Sonya mod has a `.bones` (KEEP PROPORTIONS). `mods_online` already kept
  `.bones` out, but the menu's Sonya was loaded with it and the fight reused
  her. `port_skin_bones_online` (skin_mats.c, from `mods_online(1)`) puts the
  game's offsets back and drops the root height. The guest's black screen
  is likely this (the two machines ended different fights); not confirmed
  online yet.
- **The guest's STAGE SELECT:** the guest waited in `Task_GameInit`
  (`rb_host_level`) with nothing drawn. The host sends the stage it shows
  (`PORT_PKT_STAGE` 24, six frames apart, debug_menu.c `st_tell_guest`);
  the guest draws the same window without buttons (`stage_menu_watch_draw`,
  game_main.c) and waits on while packets come (two seconds without one
  ends it).
- **Invites:** the Worker's `invites` table (`POST/DELETE /v1/invite`,
  `invites` in `GET /v1/friends`, three minutes, friends only; private repo
  3c44d13). main.js `startGame`: `--screen 51` for both, `UMK3_ONLINE_FIND`
  for the one who joins; a game already running reads `online_join.txt`
  (gamecode_stubs.c `port_online_join_code`, FrontEnd.c FE_Task_Online).
  `pollFriends` every 15 s.
- **FPS:** fights froze 250 to 900 ms as SMOOTH MODELS' mesh arrived:
  `adopt` ran `umk_subsurf_set_sharp` (a full CCG sync) on the game's
  thread. It runs in `build` now. Stages' first-frame events still pause
  (STREET up to 1.3 s), not looked into.
- **Loading:** the first menu took 6.5 s; a mod's model was baked again at
  every load (`port_bake_lighting`, kept now by skin file, its time logged).
  The launcher's `init` asks for glyphs, logo and hero at once.
- **Not tried online with two players.** Published at Mary's request.

## 0.1.21: friends, live leaderboards, no help from the debug menu (2026-10-31, released in 0.1.21)

Mary asked for friends (found by profile name or by a code tied to the
account), unique profile names, the scores sent as soon as a run ends
(a friend's run had not shown until he closed the game), new profile names
shown on the boards, a run with the debug menu's help left off the boards,
and ACCOUNT and FRIENDS moved into PROFILE. She played it and confirmed.

- **Server** (private repo, commit bbb83a4; migration `0002_friends`,
  applied to the live database). `accounts.pcode` / `pname`, unique
  (`pname` under NOCASE). `PUT /v1/profile {code, name}`: the code is stored
  once and returned after; a taken one is `code_taken`; a taken name leaves
  the stored one and answers `taken`. `GET /v1/users?q=` (the code exactly,
  or a name prefix of 2+ characters, 10 rows). `GET/POST/DELETE
  /v1/friends`: a row is "src asked dst", both rows are a friendship. The
  boards show `COALESCE(pname, scores.name)`.
- **Launcher** (`lib/account.js`). `claim()` after every pull and after the
  game: sends the profile's id and name; takes the stored code back into
  profiles.ini; a `code_taken` (two accounts on one PC) gets a new id; a
  taken name goes back to the stored one. `rename()` from PROFILE's name
  field (on change). `friends`, `findPlayers`, `addFriend`, `removeFriend`.
  `live()` every 10 s while the game runs (`main.js`, `stepAside`):
  `flushScores` then `boards`, written to `leaderboards.txt.tmp` and renamed.
  UI: `SecProfile.svelte` with `PROF_TABS` (PROFILE, ACCOUNT, FRIENDS;
  `ProfAccount.svelte`, `ProfFriends.svelte`); the conflict at PLAY opens
  PROFILE > ACCOUNT.
- **Game** (Port). `gamecode_stubs.c`: `mayhem_load` reads
  `leaderboards.txt` again when its time or size changed; a post with
  `PortDbgTainted` set is dropped (and logged). `game_main.c`:
  `PortDbgTainted` set by any debug request but SAVE STATE, by `fight_key`,
  and in a fight while INFINITE HEALTH, FREEZE CLOCK or another speed is on
  (`dbg_taint_watch`, each frame); cleared when `GameMode` changes and when a
  run starts (a Karnage fight; Survival's first, `SurvivalStage == 0`).
- **Checked:** Worker tests (profile, search, friends, board names);
  launcher tests (20 pass); Mary in game and launcher.

## 0.1.20: the game's leaderboards on the account server (2026-10-31, released in 0.1.20)

Mary asked for the leaderboards back, now that accounts exist; then for the
profile name instead of the account's, the frame fixed on wide screens and
the gamepad handled as ACHIEVEMENTS does. She played it and confirmed.

- **Game** (`gamecode_stubs.c`, Port, not in the binary). The front end's
  calls into EA's Mayhem stay where they were; their answers now come from
  two files beside umk3.ini that the launcher carries to and from the
  server. `leaderboards.txt` (`user=<profile name>`, `board=<key>`, then
  `<name>|<score>` rows, best first, 100 at most) makes
  `EASDK_ConnectedToNetwork` 1; without it (a GUEST) it is 0 as before, and
  `FE_Task_Select_Leaderboard` shows its own no-internet modal.
  `EASOC_MayhemPostStatWithData` (called by the Karnage and Survival
  summaries with `shaokahn_med` / `survival_easy`) appends
  `<key> <score>` to `scores.txt` and places the score in the board in
  memory. `EASOC_MayhemGetLeaderBoard` calls
  `FE_DrawLeaderBoardEntriesCallback` once a row (ranks 1..8 on the page)
  and returns the page's row count -- 7 for a full last page, since every
  period but 5 enables Next for any full page. `game_main.c` hands the
  folder over with `mayhem_set_dir(g_ini_path)`.
- **Front end** (`FrontEnd.c`, Port). `FE_Task_LeaderboardsSK` and
  `FE_Task_Leaderboards`: the `FEBits3` frame is `FE_W(468) + PortFEExtraX`
  wide, as in `FE_Task_Achievements` (the callback puts the score 60 in from
  the screen's right edge, which fell outside a 468-wide frame on 16:9); with
  `PortHideControls` no buttons, `PortMovesTurn` for the pages,
  `PortPagesExit` to leave and `PortPagesLine` for the hints.
- **Launcher** (`lib/account.js`, `main.js`). `boards()` before PLAY
  (`/v1/scores/<key>?limit=100`, both boards; a guest's file deleted, an
  offline one kept); `flushScores()` when the game exits, before the
  progress push, the best per board posted with the profile's name; what
  fails stays in `scores.txt`. Log-out and deleting the account delete
  `leaderboards.txt`. Both files are git-ignored.
- **Server** (private repo, commit 856237c): `scores.name` (migration
  `0001_scores_name`, applied to the live database), the profile name the
  boards show, the username when there is none.
- **Checked:** Worker tests; launcher tests (20 pass); boards / flushScores
  against the live server with a throwaway account (posted, best kept,
  deleted with the account); Mary in game: boards, profile name, frame,
  gamepad.

## 0.1.19: MULTIPLAYER over LAN, accounts and cloud progress (2026-10-30, released in 0.1.19)

Mary asked for accounts (username, password, optional email, a recovery
code, hashed and encrypted secrets, progress synced across devices, the way
back to leaderboards), a GUEST mode without leaderboards, a warning against
personal emails and reused passwords, and PLAY > MULTIPLAYER working again
as ONLINE does but over the LAN alone, Radmin VPN included. She played all
of it in her test folder and confirmed.

- **MULTIPLAYER** (Port, not in the binary). `FE_Task_Multiplayer`
  (0x0001782c) is the iPhone's Bluetooth lobby; its peer list came from
  `getNumberOfPeers` / `getPeerNumber` / `connectToPeer`
  (limeMPGameSpecific.c, 0x000afbcc / 0x000afac4 / 0x000afbfc), which
  returned nothing. The PLAY menu now calls `mpnet_set_lan_only(1)` before
  `startMP` for MULTIPLAYER and `0` for ONLINE. In LAN-only mode
  `mpnet_session_create` (mp_net.c) skips EOS; each game broadcasts a
  `K_HELLO` with its profile every second and lists who it hears (dropped
  after 3.5 s of silence); `connectToPeer` asks that game by its code with
  the usual `K_JOIN`, also sent straight to the address it was heard from.
  From there it is the online fight (GameMode 1, rollback). Every broadcast
  (HELLO and JOIN, ONLINE's too) now goes to 255.255.255.255 **and** to each
  adapter's own broadcast address (`SIO_GET_INTERFACE_LIST` on Windows,
  `getifaddrs` on Linux): Windows sends the limited broadcast down one
  adapter only, which missed Radmin VPN's 26.0.0.0/8.
- **Accounts** (launcher only; the game is unchanged). `lib/account.js`
  talks to a Cloudflare Worker + D1 database at
  `https://umk3-accounts.umk3-accounts.workers.dev`, whose code is in
  Mary's private repository (MaryNCRT-Universal-Game-Services,
  `umk3-accounts/`; she asked for the server to stay out of this public
  one). Server side: PBKDF2-SHA256 (100,000 rounds, 16-byte salt) sealed with
  AES-GCM under a Worker secret for passwords and recovery codes; AES-GCM
  for emails; SHA-256 of session tokens; a 15-minute lock after ten wrong
  passwords; limits per IP; every account-bound route takes the account
  from the session, never from the request. Client side: `account.ini`
  (git-ignored, never packed) keeps the user, the session token sealed by
  Electron's safeStorage, and the last synced version and hash; no
  password. Synced: the `profile=` line of profiles.ini and the save files
  `savedata`, `achievementsdata`, `statsdata`, `survival`, `towerdata`,
  `unclaimedtreasure*`. `pull` before PLAY, `push` when the game exits;
  conflicts (both changed) are settled on the ACCOUNT page, the replaced
  side copied to `save/backup/<date>/`.
- **Checked:** the Worker's own tests (every route, against node:sqlite
  standing in for D1); the launcher client against that server on one
  machine with two simulated PCs (upload, download with the profile id,
  conflict, resolution, guest refused by the leaderboards, no password in
  account.ini); the deployed server with a throwaway account (made, email
  sealed and read back, progress uploaded, deleted). Mary: account, sync
  and MULTIPLAYER in her test folder.
- **Not done (then):** the game's own leaderboards; done on 2026-10-31,
  section above.

## 0.1.18: Noob Saibot on the roster, EXPORT ALL, settings by category (2026-10-30, released in 0.1.18)

Mary asked for five things; all played in her test folder and confirmed.

- **Noob Saibot.** `SetupLockedCharacters` (FrontEnd.c) already sets
  `CharacterAvailable[23] = 2`; `CS_Layout` / `CS_Layout2` (0x00101630,
  0x001016a0) just have no cell for 23. `drawCharacterSelection` now writes
  23 into the empty cell [3][6] when `port_noob_saibot` (umk3.ini
  `noob_saibot=`, read in game_main.c, written by the launcher's ini.js and
  the Esc menu) is on and `GameMode` is neither 1 (online: both grids must
  match) nor 2 (the one-character mode), and -1 otherwise. Port, not in the
  binary.
- **Online and character mods.** No code change: `mods_online(1)`
  (runtime/mods.c, from `rb_online_begin`, classic netcode included) puts
  every data.ini word back and serves only models and textures. The
  CHARACTER EDITOR's note (`edOnline`) said the opposite and now says this.
- **Esc menu categories** (settings_menu.c): `PAGE_VIDEO`, `PAGE_GFX`,
  `PAGE_TEX`, `PAGE_GAME` instead of one GRAPHICS page; the game's own video
  panel (`g_framed`) still shows them all on one page. GAME holds the
  interface rows and NOOB SAIBOT ON THE ROSTER.
- **Resolutions** sorted by width then height in `k_res` (settings_menu.c),
  `RES` (lib/ini.js, app.svelte.js); the ini stores width and height, so
  saved settings keep their size.
- **TEXTURES: EXPORT ALL** (ModTextures.svelte `exportMany`): the shown
  fighter's or stage's textures, or the whole group's, 16 at a time through
  `exportSave`.
- Also looked at, nothing changed: Mary's replay
  `20261030-185205_sonya_vs_smoke.umkr`, where human Smoke (CPU) only hit
  Sonya at FINISH HER and raised his arms. `t_drone_execute_fatality`
  (0x000707cc) offers mercy only with `H[0] > 0 && H[1] > 0` (both won a
  round; this fight was 2-0), else picks fatality 1 or 2 on a coin toss;
  `_ochar_fatalities2[22]` is `t_non_violent_finish`, a plain hit. The
  original's behaviour.

## 0.1.17: kombos that share a start, CHARACTER EDITOR KOMBOS (2026-10-30, released in 0.1.17)

Mary: Mileena's HK, HK, D, F + LK and HK, HK, U + LK, U + HK start alike and
the game always gives the first.

- **Traced to the binary.** `combo_scan_a11` (0x326d8) walks the 0x18-byte
  entries of the next list and stops at the first whose button (word 2's
  low byte, a switch id) was pressed; the stick is not read.
  `ct_mileena_knee3` (0x1659dc) holds two entries, both switch 4 (LK): the
  first goes on to `ct_mileena_dummy1`, the second to `ct_mileena_knee4`,
  which no input reaches. A scan of the 114 `ct_*` lists finds the same in
  `ct_tusk_knee3`, `ct_sonya_roundh`, `ct_swat_knee2`, `ct_kano_post_elbow2`
  and `ct_sonya_elbow3`. The original behaves so; nothing was mistranscribed.
- **Port rule** (`mkcombo.c`, marked in the code): on a match, with up held
  (the stick word `joystick_in_a0_px` reads, bit 1), the next entry of the
  list with the same button is taken. No proc field is touched; without up
  the scan is the binary's.
- **Switch ids**, from `_swtab` (0x16f10c) and checked against known kombos
  (Liu Kang HP, HP, BL, LK, LK, HK, LK; Jax HP, HP, BL, LP, HP): HP 0, LP 1,
  BL 2, HK 3, LK 4; 6 to 9 the stick.
- **CHARACTER EDITOR → KOMBOS** (`ModEditor.svelte`): from
  `ochar_elbow_combos` (HP starter) and `ochar_knee_combos` (HK starter),
  every path through the lists, a list running on into the next table in
  address order as the binary's does (`ct_swat_knee2` continues in
  `ct_swat_elbow2`). Each hit's button (word 2, low byte) and strike (word
  4, bits 8+, an index into `combo_strike_table`) go to `data.ini`. The
  strike's damage is shown, not edited: the `stk_combo*` rows are shared by
  every fighter. Checked against all 24 fighters with a script over
  `build/logic_tables.c`.
- **Played by Mary** in her test folder: both work.

## 0.1.16: MOD HUB (2026-10-30, released in 0.1.16)

Mary asked for a hub inside the launcher where players download the mods
that she and her collaborators publish, installed into `mods\` and
updatable.

- **Storage:** the public repository MaryNCRT/UMK3-IOS-Mod-Hub. `index.json`
  on `main` lists each mod (id, folder, name, author, version, category,
  description, thumbnail, url, size, sha256); its files are a `.zip`
  attached to the release `<id>-v<version>` (HD NINJAS is ~920 MB, past
  git's 100 MB limit; a release file may be 2 GB). Its `publish.js` zips a
  mod folder, creates the release with `gh` and writes the entry; its README
  says how collaborators publish. Published: HD NINJAS, Sonya Blade MKX,
  Rain (from Mary's test folder).
- **Launcher:** `lib/hub.js` reads the index (an entry whose download is not
  a release of that repository is dropped), downloads with
  `lib/update.js`'s `download()` (SHA-256 checked), unpacks it itself
  (zlib, each file CRC-checked: Linux's GNU tar cannot read a zip, which
  the PR's Linux CI caught),
  replaces `mods\<folder>` and adds it to `mods.ini` on, at the top (an
  update keeps its place and on/off). What came from the hub is kept in
  `mods\hub.json` (id, folder, version); UPDATE shows when the index's
  version is newer by `compareVersions`. IPC `hub:list`, `hub:install`
  (by id, from the list fetched last); page `sections/mods/ModHub.svelte`;
  the UI harness answers both.
- **A mod already here** (Mary's report: she had all three by hand and the
  HUB offered DOWNLOAD): a mod counts as installed when `hub.json` names it
  or `mods\<folder>` exists. Its files are compared with the zip's central
  directory, read with HTTP ranges (GitHub's release storage answers 206;
  it refuses suffix ranges, so the index's `size` gives the offsets): sizes
  when the list loads, CRC-32 on UPDATE / CHECK, which downloads only the
  entries that differ (each its own range, inflated and CRC-checked) and
  deletes files the zip lacks (`mod.meta.xml` kept). A folder whose
  `mod.ini` has no version is judged by its files only.
- **Small window** (Mary's report: the HUB cut off on the right): `.app`'s
  grid had no column size, so the MODS tab row, one tab longer, made the
  column wider than the window. It is `minmax(0, 1fr)` now and the tab row
  wraps when it does not fit; at full width nothing changes.
- **Checked:** `node --test test/*.js` (new `test/hub.test.js`: index
  parsing and filtering, installed/update status, `mods.ini` registration,
  a real zip installed and then updated in place, a bad hash refused).
  **Played by Mary** in her test folder (her mods detected, update, small
  window): works.

## 0.1.14 to 0.1.15: retarget, proportion trick, own materials, stage pictures, replay fixes, STAGE VIEWER, shape keys (2026-10-29 to 2026-10-30, released in 0.1.14, 0.1.14b and 0.1.15)

Branch `claude/viewer-tabs`. Built into
`WORK\release-0.1.12\UMK3-PC-0.1.12-beta` for Mary to try; **not played
yet**. Asked by Mary after trying the first round:

- **Replays were not recorded right and did not show in the launcher.**
  Two causes, found in her logs and files:
  - Arcade's rival still changed after Task_GameInit step 42 (her
    `20261029-162545_liukang_vs_stryker.umkr` keeps Stryker, 5, in its
    setup, and the log loads LIUKANG vs KITANA, 15). `rb_fight_rival` is
    now also called at step 48 with `PLAYER2MODEL`, right before
    `LoadLevelCharacters`: the recording keeps (and is named after) the
    rival loaded; a replay puts it back there.
  - The launcher's REPLAYS page accepted only `UMK3RPL3` files; every
    recording since the rival fix is `UMK3RPL4` (`lib/replays.js`).
- **Replay HIDE is a toggle**: the click that hid the player also brought
  it back on release (it only stayed hidden while held). Now a new click
  is needed (`game_main.c`, `g_rp_hide_wait`).
- **STAGE SELECT**: smaller buttons that no longer cover the names, a
  RANDOM button, and a picture for every stage: `STAGE_<SCENE>.PNG` (a mod
  or res, for pictures added later), else the game's CS_BG sheet, else the
  stage **rendered by the launcher** (`mods/stagepics.js`, once per stage
  into `aditional textures/stages/<SCENE>.png`, when the game is ready),
  else one drawn in the game (a gradient with the name).
- **REPLACE MODEL**:
  - The bar at the foot (Enter / Esc / F11) no longer shows over the
    preview window (`app.overlay`).
  - **Every material gets its own texture** (`<skin>_M<n>`, and its
    `_ICE`): a material with no picture is its colour (FBX DiffuseColor,
    GLB baseColor, else grey); THE GAME'S is a copy of the outfit's. So the
    model's UVs are kept and no texture other models share is touched
    (Mary's Sonya mod had turned other models black; the cause was not
    found in the game's draw code, this removes any shared texture from
    the path).
  - The materials' textures are listed in REPLACE TEXTURE after the model
    is saved.
  - **Fighters drawn black with a large model, found and fixed.** Mary's
    Sonya (6,009 faces) unwelds to ~18,000 vertices; the binary's skinning
    scratch holds 15,000 vertices and 3,333 faces (`SkinnedVerts`,
    `RenderIndexes`, `RenderUVs/RGBs/Verts`), so `DrawSkinnedMesh2` wrote
    past them into the animation frames and palette. And the fighter's
    baked `STATICLIGHTING/<name>.lighting` is sliced by the skin's own
    vertex count: read past its end, colours came from the heap. Now the
    scratch holds `PORT_SKIN_VERTS` (65,536, a uint16 index's reach;
    `lime.h`), and a lighting file that does not fit the skin is replaced by
    its average light (`Players.c port_fit_lighting`, logged `flat light`).
    The importer refuses a block over 65,535 vertices once unwelded.
  - **STAGE VIEWER, stages as the engine plays them (Mary 2026-10-30; built
    into the test folder, not tried by Mary yet).** The MODEL VIEWER is now
    the CHARACTER VIEWER and stages have their own page
    (`mods/stagerun.js`, `StageViewer.svelte`). What was missing: a stage's
    `.events` fire effect scenes (Scislac's whole sky is `SCISLAC_VORTEX`,
    the Street's newspapers, the Kave's lens flares, the Pit's blades), and
    the viewer, the export and STAGE SELECT's pictures only drew the scene.
    Now, read from the code: each tick AnimateBG advances the frame
    (looping or held per LEVEL_INFO +0x20) and fires the keys on it,
    LIME_UpdateEvents runs each spawned effect (step, repeat, repeat2,
    delay, cap, group, following), LIME_RenderScene's rules draw every node
    (key above 0.03; 0.97 and over opaque, ALPHA... blended without depth
    writes, ATST... clipped at 0.9; below 0.97 additive, no culling), an
    effect tinted by its track's colour and lit from its `.lighting` plus
    the level's ambient when its `.lights` says so; the Temple's 32 window
    glow slices (RenderLevelBG). Each level's place, scale (the Street's
    0.010651, not 0.0133), loop flag, ambient and second scene come from
    LEVEL_INFO in the binary. Unlit, raw colours, on black. A click picks an
    element: its properties, ISOLATE (numpad /), FRAME (F). The export
    writes everything drawn over the stage's length, each spawn its own
    object, with the material's blend (GLB alphaMode/extras + unlit, FBX
    opacity, USD opacity / threshold). STAGE SELECT's launcher pictures are
    made again this way (`.v3`). Checked by eye in a local test page:
    Scislac, Street, Temple, Pit.
  - **Stage viewer, second pass (Mary 2026-10-30):** Jade's Desert had no
    sky and Cyrax blinked. The sky: the game's projection has no far plane
    (`CreatePerspectiveMatrix` puts 2fn/(n-f) where w is taken: depth stays
    under 0.5), so its sky ~870 units out is drawn in the game; the viewer
    and pictures cut at 400. Now 100,000. Cyrax: `DESERT_CYRAX` is a
    12-mesh flipbook, and each mesh drew untextured for the frame its
    texture took to arrive: every texture of the stage and its effects is
    loaded first and meshes are kept, not rebuilt. Also as the engine: an
    effect wrapping back draws frame 0 (not the last), depth test GL_LESS
    (coplanar pieces no longer swap). All 17 stages checked by eye in a
    local page. Scislac Busorez is not in LEVEL_INFO (the game never loads
    it); its events are the main menu's vortex sphere, textured by
    DrawVortex3D's code, so it shows black. Pictures redone (`.v4`). STAGE SELECT's
    list moves with the mouse wheel (a row a notch; `stage_menu_mouse`).
    CHARACTER VIEWER: VIEW > STAGE BEHIND puts a stage behind the fighter
    as the fight has it (ModViewer.svelte `setStage`): the StageView of
    stagerun.js playing at 30 a second, on black, the floor hidden, the
    fight's camera (GAME CAMERA button), log depth for the far skies; its
    textures marked sRGB there, as the fighter's are in that view. Not
    yet seen by Mary.
  - **The slide mid-kombo (issue #114, Mary 2026-10-30: "a slight slide";
    checked against the binary, one real difference fixed in
    `mkrepell.c`; built into the test folder, not tried yet).** The issue's
    own log names the rule: `separate us: character 13, punches 3, hits 1,
    edge 0`. That is the punch chain, and every step of it is the binary's,
    read instruction by instruction: `t_cc_hi_punch` (0x415c4) / `t_cc_lo_punch`
    (0x41604) set 6 / 4 and 5 / 3; `t_ccp3` (0x477bc) takes the smaller
    number when the one hit is the machine (`am_i_joy`: proc+0x10 bit 0) or
    a player holding away, and compares the punch count at proc+0x4c, which
    `t_r_hi_punch` / `t_r_lo_punch` bump (`inc_p_block`) and only
    `back_to_normal_px` clears -- so in a kombo, where the one hit never gets
    back to normal, the 3rd low punch (4th high) against the machine sends
    him to `t_separate_us` (0x43250): `t_stumble_back_vel` at 3.0 a tick, or
    at a wall the attacker is moved instead. The corner rule
    (`t_avoid_corner_trap`, 0x47b50, 1 hit within 103 of the level limit)
    also matches. `tools/decomp/factdiff.py` over `mkreact.c`, `mkcombo.c`
    and `other.c` against `armrecomp` transcriptions, the handler
    addresses resolved by name: no store, call or handler differs but the
    port's diagnostics and hooks. So the slide is the original's (the issue
    thread found it in videos of the original too). **One real difference,
    fixed:** `repell_func` (0xa51e4), which pushes two fighters closer than
    60 apart at 3.0 a tick: when the first vertical test passed with y1 at
    or past `Pp+0x40`, the binary goes on to the second test (0xa5246 ->
    0xa52b8) and the C skipped to the arbitration, so a fighter over the
    other (a juggle) was not pushed apart. Checked with the binary's own
    code: `recompiled.c` from `armrecomp` and the C on 200,000 random
    states, 3,278 differed before, 0 after (the test is a scratch harness:
    `tools/decomp/difftest` makes no test of a function that only reads
    globals, and its build no longer compiles mk3logic.h without
    `-include savestate.h`). The note in `mkrepell.c` doubting which global
    the second test reads is settled: `_Pp`'s slot, 0x000f3158, both times.
  - **Unbroken select-screen music into STAGE SELECT, a smaller ESC menu
    on 4:3, every mod in TARGET MOD, TEXTURES in the STAGE VIEWER (Mary
    2026-10-30; built into the test folder, not tried by Mary yet).** After
    CANCEL the select screen came back silent. The tune now never stops on
    the way: `Task_FEDestroy`'s stop and the front end's fade to the fight
    are skipped when STAGE SELECT comes next (`stage_menu_keeps_tune`), the
    window only sets the volume, and on the first front-end frame after
    CANCEL `stage_menu_fe_frame` starts `CharacterSelect.mp3` again if it is
    not playing (`plat_music_playing`, new in both audio backends); the log
    says which happened (`stage select: the tune ...`). The ESC menu on 4:3
    ran off both sides: it is drawn over the whole 16:9 window while the 4:3
    picture shows the middle, so `draw_ios` now sizes the box from the
    picture's shape (`port_aspect`, game_main.c) and draws it at 90% below
    3:2. TARGET MOD was a `<datalist>`, which lists only the names that
    start with what is in the box; `ModTarget.svelte` lists every mod under
    a button (character viewer, TEXTURES). The STAGE VIEWER has a TEXTURES
    tab: the stage's textures with pictures, the one picked large, REPLACE
    TEXTURE into a mod (with LOW / LITE / ICE copies) and the stage opened
    again with it; a texture name in PROPERTIES opens it there.
  - **STAGE SELECT: the select screen's music, CANCEL goes back (Mary
    2026-10-30; built into the test folder, not tried by Mary yet).** The
    window opens after `Task_FEDestroy`, which had stopped the tune, so
    turning the volume up played nothing: `st_music` now starts
    `CharacterSelect.mp3` again at the player's volume, as `Task_FEMain`
    does, and stops it on a pick, as `Task_FEDestroy` had. CANCEL (and
    Esc / the pad's back) went into the fight on the game's own stage; it
    now goes back to the select screen: `CurrentTask = 2` (`Task_FEInit`,
    as after a fight, with the loading picture already made), the select
    screen's pop on the way out undone (`PendingPop` / `PendingPopAll`, for
    training), and the tune plays on (the front end's load latches
    `FETuneSelection` to it). Online the host's CANCEL still takes the
    game's own stage, since the guest is waiting on it.
  - **Shape keys for fighters, a fuller timeline, 4:3 fixes, stage-select
    music (Mary 2026-10-30; built into the test folder, not tried by Mary
    yet).** REPLACE / EDIT MODEL has a SHAPE KEYS tab: Basis (the model as
    the game draws it) and named keys with a value slider, a key button and
    mute; SCULPT grabs the vertices under the brush with a smooth falloff
    into the key picked, worked out in the skin's rest space so it holds in
    any pose (`ModelImport.svelte` shapeDeltas / posedDeltas); with an
    animation chosen the values are keyed per clip frame (I, Alt+I, REC,
    linear / bezier). Kept in `<skin>.import.json` (`shapes`); the game does
    not draw shape keys. `KeyTimeline.svelte` is now Blender-like:
    transport (first, previous key, play, next key, last), a frame field,
    a tick a frame where they fit, a Summary row, the playhead with its
    frame, the wheel steps a frame. The editor windows are put on the page's
    top (`lib/portal.js`), so the launcher's bar and the player's card no
    longer show over them; the menu's description box shrinks a text that
    does not fit (`Desc.svelte`; MODS' English ran out of it). Mary saw
    "500 Internal Error" after SCULPT in the stage editor: not reproduced in
    a browser with the same data (`launcher/app/scripts/ui-harness.js`
    serves the UI with the preload's calls over HTTP); the window now
    refuses any navigation (`will-navigate`) and SCULPT's drag prevents the
    native drag, the likely cause, but this is unconfirmed. Game: the ONLINE
    code box drew in the overlay's 3:2 space and ran off a 4:3 screen; the
    menu now hands over where it drew it (`PortOnlineBoxPx`, FE_X/FE_Y/
    FE_W/FE_H) and `online_ui.c` maps it as `ui_nav_frame` does. The
    settings panel in 4:3 sized its box from the last 2D pass's width (the
    whole 16:9 window): `draw_ios` now takes the width from its own
    viewport, and shrinks the panel if it is still wider. STAGE SELECT
    plays the select screen's tune at the player's volume while it is open
    (the select screen had faded it to 0) and quiets it again for the
    loading (`st_music`, debug_menu.c). Built: `npm run ui`, the game
    (x64, test folder), the test folder's app.asar.
  - **STAGE VIEWER editing, as Blender (Mary 2026-10-30; built into the
    test folder, not tried by Mary yet).** `mods/stageedit.js`,
    `StageEditor.svelte`, `KeyTimeline.svelte`. A picked stage element (the
    scene's objects and the Temple's glow; effects are spawned and die with
    their events, so they are not editable) is moved, turned and scaled in
    place with G / R / S (X / Y / Z an axis, again its own, Shift a plane,
    typed numbers, click or Enter keeps, Esc or right button drops) or the
    MOVE / ROTATE / SCALE handles. I keys the placement on the frame shown,
    Alt+I deletes the frame's keys, REC keys every change, LINEAR / BEZIER
    (Blender's auto-clamped) sets new keys and the frame's own, Ctrl+Z /
    Ctrl+Y. The play bar's slider is now a dope-sheet timeline (drag to
    scrub, a diamond row for the placement and each shape key). The edit
    rides on top of the engine's animation about the mesh's centre, as the
    CORRECTIVE ANIMATION LAYERS ride on the fighters'; a change on a keyed
    element that is not keyed goes when the frame changes. EDIT opens the
    element in a model editor window (its mesh, texture, vertices,
    wireframe, UVs over the texture, exact T / R / S fields, the same keys
    and timeline) with SHAPE KEYS: Basis plus named keys, a value slider,
    a key button and mute each; SCULPT (Tab) grabs vertices with a smooth
    proportional falloff into the key picked (radius [ ] or the wheel while
    dragging). Kept a stage in `<game>/stage edits/<ID>.json` (IPC
    `stageedits:load/save`), reloaded with the stage. The export writes them:
    placement baked into each bone's frames, the Basis edits into the
    vertices, shape keys as GLB morph targets with a `weights` animation
    (`extras.targetNames`), FBX BlendShape / BlendShapeChannel / Shape with
    an animated DeformPercent, USD BlendShape prims bound through
    `skel:blendShapes` and SkelAnimation `blendShapeWeights`. Checked:
    `npm run ui` builds; `node launcher/app/test/stageedit.test.mjs` (curves,
    keys, shapes, the three exporters' shape data). Not checked: the files
    opened in Blender or pxr (not on this machine), and the editor by hand.
  - **STAGE SELECT named the Street twice (and the Balcony, the Kave).**
    Levels 4, 10 and 11 have a second scene and the fight starts in it
    (Task_GameInit step 51, CurrentScene = 1): the Subway, the Soul Chamber,
    the Lair. The select now names and pictures a level by that scene
    (`debug_menu.c st_scene`), so all 16 are different.
  - **EDIT MODEL shows the T-pose too** (T-POSE / ORIGINAL POSE at rest, as
    the viewer); bones can be edited in either.
  - **0.1.14b beta published** (2026-10-30, pre-release `v0.1.14b-beta`,
    zip and Linux tar.gz): the 0.1.14 packages with the new `src/`,
    CMakeLists.txt and launcher (`0.1.14-beta.b`, read by the updater as
    0.1.14b). Nothing in it played by Mary yet.
  - **Liu Kang's scream that never stops: found, Mary 2026-10-30 (built
    into the test folder, not tried yet).** Her words: it repeats for ever.
    The Windows mixer (`platform/win32_audio.c`) kept each voice's place in
    the sound as 16.16 in 32 bits, which ends at 65,536 frames (4.1 s at
    16 kHz). Three of the game's 497 sounds are longer: Liu Kang's death
    scream `Lkdeath.wav` (67,093 frames, 4.19 s), `Skdiemix.wav` and
    `Vpdeath.wav`. Past the end of the range the place wrapped to 0, never
    reached the file's end, and the voice played on without stopping. The
    place is 64-bit now. Also checked on her replay's log
    (`20261030-013216_liukang_vs_shaokahn`): every voice the game asks for
    is asked once (`group_sound` 0x580a4 and `t_do_flip` 0x30634 match the
    binary instruction for instruction; the jump yell is one a diagonal
    jump, as on the device); his hit screams `Lkwast1` / `Lkwast2` are
    twice anyone's length (2.31 s / 1.94 s). Linux's SDL mixer gave a sound
    played again a second voice on top of the first; it now starts it over
    as `-[Sound play]` does and as the Windows mixer already did.
  - **Arms and hips still stretched: the file's weights; SMOOTH WEIGHTS,
    Mary 2026-10-30 (test launcher repacked, not tried yet).** Measured on
    her Sonya against the game's own, edge by edge over every frame (p99 of
    the stretch from the bind, by the bone a vertex hangs on): the bones
    and their lengths and directions match the game's within 0-3 degrees,
    and her file's weights are kept exactly, but those weights jump: no
    finger weights, the hands holding 12% of the vertices up the forearm,
    the right upper arm 0.7% (its sleeve hung on the clavicle and the
    forearm). Upper arm L 4.57 against the game's 1.95, forearm R 4.81
    against 1.50, thigh R 3.77 against 2.14, pelvis 4.97 against 3.05.
    Tried and dropped, as measured: Blender's Preserve Volume (dual
    quaternions; no gain, the stretch is not twist), copying the fighter's
    weights (Data Transfer; worse, her coat is not Sonya's shape),
    weighting again by distance (arms better, chest and neck far worse).
    Kept: SMOOTH WEIGHTS, as Blender's Smooth Vertex Weights (factor 0.5,
    neighbours by triangle and across UV seams, painted vertices held):
    3 passes bring those to 2.87, 3.20, 2.82, 3.85 with nothing worse; 8
    empties the chest. 3 by default on a file just picked; a model opened
    again keeps what it was saved with (hers: 0, so it needs the slider).
  - **The `.lighting` bake decoded; a mod's model gets its own, Mary
    2026-10-30 (built into the test folder, not tried yet).** In a fight a
    fighter is lit from `STATICLIGHTING/<X>.lighting`, not live. A REPLACE
    MODEL skin has another vertex count, so `port_fit_lighting` gave it one
    flat grey: no shading. The file turned out to be LightVert on each
    skinned normal, every frame, one byte an unwelded vertex
    (`docs/formats/LIGHTING.md`); checked byte for byte on Sonya, Scorpion
    and Liu Kang. `port_bake_lighting` (Players.c) now bakes it at load for
    a mod's skin (`port_skin_custom`): GenerateMatrices for each frame, then
    DrawSkinnedMesh2's lit path with the shipped lights, put back after.
    Automatic for every imported model, and it follows the mod's own
    `.skinanim`.
  - **Still crushed: a GLB's bone names lost their dots, Mary 2026-10-30
    (test launcher repacked, not tried yet).** Her Sonya GLB (the
    exporter's 49 bones, through Blender) showed 13/49 bones and 1688 of
    3337 vertices weighted automatically: three.js's GLTFLoader takes '.',
    ':', '/' and brackets out of node names, so clavicle.L came in as
    clavicleL and only the 13 names without a dot matched; arms, hands and
    legs were weighted again by distance. The importer now reads each
    bone's name from the file's own node (checked on her file: clavicle.L,
    upperarm.L, ... all 49). A mod saved before this needs EDIT SAVED MODEL
    and saving again.
    - REPLACE MODEL's player got the viewer's: SMOOTH (blended between
      frames), the frame number, SPEED, PLAY from the start at the end.
    - Not changed (not asked): `<skin>.source` keeps every file picked
      beside the model; hers is 135 MB of unrelated pictures.
  - **A custom model crushed at the elbows and wrists in the moves, Mary
    2026-10-30 (test launcher repacked, not tried yet).** KEEP PROPORTIONS
    put the fighter's bones at the model's joints but kept the fighter's
    bone frames: a T-pose model on an A-pose fighter played with its arms
    45 degrees off, and each twist of a forearm or hand turned about an
    axis beside the model's limb, shearing it. Now each bone's frame is
    turned with the model's limb (the turn from the fighter's segment to
    the model's; ends and bones the file lacks take their parent's), the
    skin is bound to those frames and the offsets are written in them, as
    a retarget in Blender binds a mesh to bones along its own limbs. The
    moves' turns, the same in the world as the fighter's, then carry the
    model's limbs where the fighter's go, whatever its rest (T, A, ARM
    ANGLE). Checked by hand on an arm chain; `proportions` in
    `mods/modelin.js`.
  - **Bones moved and turned as Blender does; POSE A fixed, Mary
    2026-10-30 (test launcher repacked, not tried yet).**
    - **The gizmo** on the bone picked (Blender's Move / Rotate /
      Transform tools, three.js `TransformControls`): an arrow moves
      along one axis, a square on a plane (2D), the middle freely on the
      view (3D); a ring turns about one axis, the white one about the
      view, inside it the trackball. MOVE / ROTATE / MOVE + ROTATE (the
      default) / NONE, and GLOBAL or LOCAL (the bone's own axes).
    - **G / R as Blender's modal transform:** X / Y / Z an axis, again
      the other orientation, again none; Shift+X / Y / Z the plane
      without it; a middle-button drag picks the axis; typed numbers
      (Tab the next field, - the sign); Ctrl steps of 1 unit / 5 degrees
      (Shift 0.1 / 1); Shift alone ten times finer; R twice the
      trackball; Enter, Space or a click keeps it, Esc or the right
      button puts it back. Along an axis the point under the pointer is
      taken (the closest point of the axis to the ray), as Blender does,
      not the view's plane.
    - **CLEAN A POSE** left a model without a skeleton in T: the angle was
      measured from the file's arms, which such a model has none of. It
      now measures from the fighter's arms there (the ones it turns).
  - **The file read as Blender reads it; the gizmo under the buttons, Mary
    2026-10-30 (test launcher repacked, not tried yet).**
    - **The gizmo:** a click on an axis did nothing (the click looked for
      where the ends were drawn on a fresh list that had none); it now
      looks down that axis. It sits under the view's buttons (RIG,
      FLOOR, SOLO) instead of over them (`ViewNav` `below`).
    - **FBX as Blender's importer:** each model's RotationOrder (Maya's
      ZXY and the rest; it was always XYZ), GlobalSettings' axes (a Z-up
      file stands up), every LimbNode as a bone (end bones, bones with no
      vertices; their rest from the BindPose, else the scene), and no
      made-up normals when the file has none (worked out from the faces).
      The order and axes go through `globalM` and the clusters, so a
      take imported from the same file (REPLACE ANIMATION) moves the same
      way.
    - **GLB:** shape keys at the mesh's weights, and a part hung under a
      bone without a skin (helmet, weapon) moves with that bone.
    - Checked in node: a Z-up file's up and front land on Y and +Z; ZYX
      order turns X to +Z for (90, 0, 90). The game's skeleton is fixed,
      so bones the fighter has no match for still move with the nearest
      matched bone above them.
  - **0.1.14 beta: REPLACE MODEL like Blender, Mary 2026-10-30 (built,
    not tried yet).**
    - **Published** as v0.1.14-beta (pre-release, 2026-10-30): the 0.1.13
      zip and tar.gz with the repository's src/, CMakeLists.txt and the
      new app.asar put in (the Windows and Linux asar are the same
      files); everything else as 0.1.13 shipped it. The hand repacks had
      left out `build/icon.ico` (the launcher window's icon,
      package.json `build.files`); it is back in.
    - **Bones and weights while posed:** a clip's frame shown (or
      playing: it holds while a bone is moved, paint goes on as it
      plays) no longer jumps to the rest. A move is taken back to the
      rest through the parent's turn in that frame, a turn about its
      axis through the bone's (`editDeltas`), so the game plays what
      was set.
    - **Blender's camera** in the importer and the viewer
      (`mods/viewnav.js`): the navigation gizmo (click an axis to look
      down it, again for the other side, drag to orbit), the numpad
      (1 / 3 / 7, Ctrl the other side, 9, 4 / 6 / 8 / 2, 5) or the top
      row's digits, Auto Perspective (an axis view is orthographic;
      orbiting off it goes back), the view's name bottom left. Checked
      in node with a stubbed DOM: the views keep the distance, ortho
      in and out, 9 the opposite.
    - **EDIT MODEL** in the viewer: the fighter's skin as it is (the
      mod's model, by its parts and textures; `fighterDesc` `parts`)
      opened in REPLACE MODEL's window as an exporter GLB, no file to
      pick. With a `<skin>.source` it opens that instead.
    - **The file's own rig by default:** a model with a skeleton
      (humanoid or the exporter's names) opens with KEEP PROPORTIONS,
      its own bones. With the exporter's names that now keeps its
      joints too (`proportions` for any mapped rig).
    - **A model without a skeleton:** POSE T / POSE A / DEFAULT turn
      the fighter's arms to the model's pose before its vertices are
      weighted (`bareAdjust`, `fighterAngles`).
    - **The preview as the game plays it:** the game reads the mod's
      `<skin>.bones` the same way (offsets, root height on Y only,
      `skin_mats.c` / `RenderSkinned.c`). The window played the game's
      animations, though, where the fight plays the mod's .skinanim
      (REPLACE ANIMATION, corrective layers). It now plays the mod's
      (`gameFighter`: the game's bones and skin, the mod's .skinanim).
  - **REPLACE MODEL: undo, bone rotations, clean T / A, edit a saved
    model, a model over another, Mary 2026-10-29 (built, not tried yet).**
    UNDO / REDO (Ctrl+Z, Ctrl+Y or Ctrl+Shift+Z, and two buttons): every
    change in the window (joints, rotations, paint strokes, angles, scale,
    fit / keep, turn, materials) is kept before it is made, a slider's run
    as one, up to 100. Each bone has Blender's LOCATION and ROTATION X / Y /
    Z fields (a slider and a number): the rotation turns its children
    about its joint, about the game's X, then Y, then Z, parents first
    (`modelin.js movedJoints` `o.turns`), mirrored to the other side (X
    the same, Y and Z the other way); R with an axis adds to that field.
    RESET: CLEAN T POSE / CLEAN A POSE drops every change and turns the
    arms level or 45 degrees down from the new model's own rig (its arms
    when keeping proportions or with the exporter's names, else the
    fighter's joints it is fitted to). **A model over an earlier one**
    (Mary: the rig and the weights were laid over the old ones): the
    window was given the viewer's fighter, which carried the mod's .skin
    and .bones, so the new model was fitted to the old one's bones; it is
    now given the game's own fighter (`store.svelte.js gameFighter`, layer
    ''). **EDIT SAVED MODEL**: SAVE now also keeps the model it was made
    from in the mod, `<skin>.source` (the file, what was beside it and the
    pictures chosen for its materials; `packSource` / `unpackSource`,
    checked round trip), and the viewer, on a mod that has one, opens it
    in the window again as `<skin>.import.json` says it was set, to save
    over it. `.source` counts as the mod's own file in mod.meta.xml.
  - **Bones and weights by hand in REPLACE MODEL, Mary 2026-10-29 (built,
    not tried yet).** Mary: move and turn them with the mouse, as Blender
    does. RIG tab, EDIT BONES: a click on a joint's dot picks it, a drag
    moves it; G moves the joint picked and R turns it (its children go
    round its joint, the joint itself stays), X / Y / Z keep to that axis
    (drawn through it in Blender's colours), a click or Enter keeps it,
    Esc or the right button puts it back; MOVE (G) / ROTATE (R) on the
    side do the same. Both end as BONE POSITIONS' offsets (the other side
    mirrored), so they are saved as before. WEIGHT PAINT: the bone picked
    is shown as Blender's weight colours (blue 0, red 1) and painted with
    a brush (ADD, SUBTRACT or Ctrl, SMOOTH; RADIUS in screen pixels, [ and ]
    change it; STRENGTH); the other bones are scaled so each vertex adds
    up to one, a vertex with no other bone gives the rest to the bone's
    parent. Painted weights are kept per vertex of the file
    (`buildSkin o.paint`, `res.src` maps each written vertex back to the
    file's) and saved in `<skin>.import.json`; CLEAR PAINT drops them.
    Editing and painting stop a playing clip and work at rest. The camera
    is no longer put back after each rebuild.
  - **Second pass on REPLACE MODEL's rig, Mary 2026-10-29 (built, not
    tried yet).** The side has two tabs, RIG and MATERIALS. RIG: FIT TO THE
    FIGHTER / KEEP PROPORTIONS, the file's pose, AUTOMATIC T / A POSE now
    **off by default** (Mary: it moved models that were fine), the arm and
    leg angle and roll sliders whenever the bones are known (a humanoid rig
    or the exporter's own names; with the names the model itself is
    turned), SCALE about 0,0,0, and **BONE POSITIONS**: a joint picked by
    clicking its dot on the view or from the list, moved in X/Y/Z, its
    other side mirrored (MIRROR L / R), each alone with its children left
    (`modelin.js movedJoints`; the offsets go into `<skin>.bones`), RESET
    BONE / RESET ALL, and RECOMPUTE WEIGHTS (every vertex to the nearest
    bones, the file's weights left). The rest shown is the one the skin is
    bound to (`buildSkin` returns it). The animation bar under the preview
    never appeared: `pf` was not reactive in Svelte 5 -- fixed, the game's
    clips play on the imported model. What the window was set to is saved
    beside the skin as `<skin>.import.json`.
  - **CORRECTIVE ANIMATION LAYERS** (the viewer's CORRECTIVE tab,
    `mods/corrective.js`): layers of turns on the game's animations, each
    on one clip or all, on/off and a weight; a bone picked, X/Y/Z degrees
    about its own axes, keyed on frames (KEY, DELETE KEY, the keys listed
    to jump to) or recorded as they are set (REC); straight in between,
    held outside. Shown live; SAVE TO THE MOD bakes them into the mod's
    `.skinanim` (each frame's bone quaternion turned, as the file stores
    it) and keeps `<fighter>.layers.json` and the animation they were put
    on, `<fighter>.skinanim.orig`, so they open again to edit and a later
    REPLACE ANIMATION goes into that original and is baked again. Checked
    here: a 90-degree key gives the 90-degree quaternion, a half-weight
    layer half of it.
  - **mod.meta.xml** in every mod (`lib/mods.js writeMeta`), rewritten when
    the mod changes: name, author, version, category, description, date and
    every file with its size and the game file it replaces (`own="1"` for
    the mod's own: mod.ini, the import settings, the layers). Checked on a
    test mod.
  - **EXPORT RIG / ANIMATIONS** in the import tab: the rig, or the rig with
    every clip, as FBX or GLB.
  - **Liu Kang's sounds** (Mary: they play wrong, "strange"): checked
    against the binary -- his voice group (6, `ochar_voice_groups`
    0x16f55c), the ten group tables (`group_choices` 0x17b2e0,
    `group_table` 0x17b308, every name) and the 26 character tables
    (`ochar_sound_tables` 0x17b278, every name) are the binary's, and his
    23 WAVs are the same format as every other (PCM 16 kHz 8-bit mono). Her
    18:02 log plays his voice from group 6 as the binary would. Not found;
    the `voice:` lines stay in the log. Needs what is heard and when.
    Also checked against the binary: `limePlaySound` (0x65344) is
    `[sound play]; [sound setGain:]` on one Finch `Sound` (one OpenAL
    source) a loaded file, `-[Sound play]` restarts it if it still sounds
    -- the port's mixers already do the same (one voice a sound) -- and
    `limeLoadSound` (0x65ad0) takes the first empty of 512 slots, as the
    port does. Mary: screams repeat that should not sound. The log now
    names every sound played (`sound: <file> (<slot>)`), not only the
    voices, so the next fight with it shows which file and when.
  - **REPLACE MODEL: the rig before it is put on, arm/leg angle and roll,
    T or A pose** (Mary, with gtastuff.com's viewer as the reference for
    the floor, the joints and the weight colours). Over the preview:
    TEXTURES / SOLID / WEIGHTS (each vertex coloured by its bones,
    `mods/viewkit.js`) / WIREFRAME, RIG (the bones as lines and the joints
    as dots, moving with the animation chosen) and FLOOR. A humanoid rig
    shows the file's pose (`T` above -20 degrees of arm rise, else `A`) and
    its leg spread beside the fighter's, and four sliders, mirrored on both
    sides: ARM ANGLE (raise/lower), ARM ROLL, LEG ANGLE (spread), LEG ROLL
    (`modelin.js adjustments`). Roll turns the model's limb about its own
    length, each vertex by its weights. KEEP PROPORTIONS turns the model's
    limbs before the bones are moved to its joints, and AUTOMATIC T / A POSE
    (on by default) first turns the file's arms to the fighter's rise, so an
    A-pose model plays the moves with its arms where the fighter's are.
    FIT TO THE FIGHTER puts the angle on the fighter's joints instead: the
    bones' offsets, written as `<skin>.bones` as the proportion trick's
    are; the bone frames keep the fighter's turn, so a large angle bends
    the elbow about the fighter's axis. The game's animations already play
    on the imported model in the preview (the clip list under the view).
  - **The model viewer**: FLOOR (a grid under the model, not on stages),
    SOLID, WEIGHTS (a fighter's) and the joints as dots with the bones.
  - **GLB export** (`export.js toGlb`): EXPORT's formats now include GLB,
    glTF 2.0 binary with the skeleton, the skin, one animation a clip and
    the PNG textures inside, in metres (a root node scales the game's
    centimetres). Checked here: the file's header, JSON and every accessor
    against the buffer, on a two-bone test; not opened in Blender yet.
  - **Liu Kang's repeated screams** (Mary, 2026-10-29): not found. Her
    17:19 log (Liu Kang vs Shao Kahn) shows one `death scream` from the
    game logic. The build now logs every voice it plays
    (`voice: kind K, voice group G, sound S` for a group sound,
    `voice: character C, sound N -> S` for a character's), so the next log
    of a fight where it happens shows which sound repeats and from where.
  - **Frames lost with a custom model.** A REPLACE MODEL skin is drawn a
    material at a time (`skin_mats.c`), each part a run of indices into the
    whole model's vertices, and the Direct3D 12 / Vulkan path
    (`render/rgl_ff.c tris_indexed`) transformed every vertex 0..max for
    each part -- or, when a part named few vertices of a big array, fell
    back to one transform per index and a malloc. Now only the vertices the
    indices name are transformed, once a draw (a stamp a vertex). And a skin
    a mod brings (`mods_find` on the skin file, `port_skin_custom`) is left
    alone by SMOOTH MODELS and what it does to the plain skin (the tuck
    under a hat, the cover pieces): such a model is already dense, as Mary
    asked; the log says `is a mod's model: no SMOOTH MODELS on it`. SMOOTH
    ANIMATIONS still apply (bones only, no cost in the vertices). Not
    measured here.
  - Every material is its own texture by default again (the materials as
    the model has them); REPLACES a game texture only when chosen.
  - **SCALE** in the importer: the model's size about its pivot (the floor
    under the fighter's root): mesh, every bone offset and the root height
    together, written in `<skin>.bones`, so the animations play on it.
  - **FBX**: parts with pivots (`RotationOffset/Pivot`, `ScalingOffset/
    Pivot`) and a geometric offset (`GeometricTranslation/Rotation/
    Scaling`) are placed right; a part with no material gets its own grey
    one; a part not skinned but hanging under a bone moves with that bone;
    diffuse, normal and specular maps are read (the preview shows all
    three; the game's fixed-function renderer draws the diffuse only); a
    material without pictures shows its colour (`DiffuseColor` times
    `DiffuseFactor`).
  - **STAGE SELECT pictures** rendered from the camera a round starts with
    (eye 1.5 over the floor, 8 back, looking at 1.25, 25 degrees); the
    older renders are made again once (`<ID>.auto2`, `.v2`).
  - **Each picture chooses the game texture it replaces** (REPLACES
    `SONYA_DIFFUSE`, an outfit, block 2's texture, or NEW TEXTURE): it is
    saved as that texture (and its `_ICE`), and the game draws it on the
    model's own UVs with no `.mats` part. By default each picture replaces
    its block's texture. Mary saw the model's pictures not arrive in the
    game; this is the direct route. The player's badge is hidden while the
    importer is open.
  - **Humanoid retarget** (`mods/retarget.js`): the file's bones are known
    by name from Mixamo, AccuRig / Character Creator (`CC_Base_*`),
    ValveBiped, 3ds Max Biped, Unreal, Unity / VRM, Blender / Rigify and
    Daz, and the file's **own weights** move the model on the fighter's
    bones (twist and helper bones with their limb, fingers with the hand,
    the rest with the nearest bone above). The placement is fitted on the
    body's joints (not the arms: T or A pose). Two ways, chosen in the
    preview: FIT TO THE FIGHTER (each part turned and stretched to its
    bone) or KEEP PROPORTIONS -- the **proportion trick**: the fighter's
    bone offsets become the model's, written as `<skin>.bones`; the game
    puts them in the bones it loads (`skin_mats.c`
    `port_skin_bones_loaded`) and scales the animations' root height
    (`lime_port_root_y` in `CreateMatrixPaletteForGeneratingMesh`), and the
    viewer reads them the same (`assets.js`).
- **Frozen copies of custom textures**: REPLACE TEXTURE (viewer, editor,
  TEXTURES) writes `<stem>_ICE` too when the game has one (a fighter's
  `_DIFFUSE_ICE`) or it is a replaced model's material, as the game's are
  made: brightness on pale blue (blue 255, red 113 + 0.55 L, green
  205 + 0.19 L, measured on four fighters). A box turns it off.
- **Sonya's second outfit picture** is `SONYA_2_VERSUS.pvr` (the others
  `<NAME>_VERSUS2.pvr`): the profile icon finds it, in the launcher and
  the game.

Checked here: the C files pass `gcc -fsyntax-only`; the launcher UI
builds; the bone roles were checked on 40 names from those rigs. Not
checked: any of it in the game or with a real Mixamo/AccuRig/Valve file.

## After 0.1.13: model viewer, REPLACE MODEL (FBX/GLB/OBJ, a texture per material), replays, VSYNC (2026-10-29, released in 0.1.14)

Branch `claude/viewer-tabs`. **Nothing here has been built into a game exe
or played by Mary yet.** The C files pass `gcc -fsyntax-only` with the
build's flags; the launcher UI builds (`npm run ui`).

- **Model viewer** (MODS): the right panel in tabs (VIEW, LAYERS, TEXTURES,
  IMPORT, EXPORT); each fighter lists its derived models (babality, frozen,
  cut in half, headless, ...) found by the meshsets' names
  (`ui/src/lib/mods/derived.js`); LAYERS hides or shows each mesh; the
  texture editor shows thumbnails, grouped, with a large preview.
- **REPLACE MODEL** takes FBX, GLB (three.js GLTFLoader, MIT) or OBJ
  (+ .mtl) and opens a preview window with the model on the fighter, its
  animations and its textures. **Each material keeps its own texture**:
  the skin's triangles are written grouped by material and
  `<FIGHTER>_STANDARD.skin.mats` lists the parts
  (`block first count alpha texture`). The game draws each part with its
  texture (`src/runtime/skin_mats.c`, hooked through function pointers in
  `RenderSkinned.c` / `RenderMesh.c` and `Players.c`): UVs as the file has
  them, GL_REPEAT past 0..1, transparent parts last (alpha test 0.1 +
  blend); frozen uses each part's `_ICE` copy, written by the launcher.
  GLB reads the UV set each material names and `KHR_texture_transform`.
  The preview and the viewer show each part on its own (eye, "1" = alone,
  click = large picture). A block with parts is not smoothed (SMOOTH
  MODELS) in the game or the viewer. Online loads `.mats` too (visual).
- **Online is vanilla**: `mods_online(1)` puts back every data.ini word a
  mod changed and only models and textures are taken from mods
  (`mods.c`, called from `rollback.c`).
- **Replay player**: HIDE (a click or Esc shows it again) and SIZE
  (75/100/125/150 %, umk3.ini `replay_scale`); the launcher steps aside
  while the game runs and comes back where it was.
- **Replays: the wrong rival.** Arcade's VS screen picks player two inside
  the fight's loading, after the setup was written, so a recording kept
  the previous rival (Mary: `liukang_vs_stryker` was a fight with Kitana;
  `scorpion_vs_nightwolf` replayed against Old Sub-Zero). Task_GameInit
  step 42 now calls `rb_fight_rival`: recording writes the real one into
  the setup and names the file after it; a replay (`UMK3RPL4`) puts it
  back. Old `UMK3RPL3` files still play but keep their old name.
- **Replays: seeking stops what still sounds** (`plat_audio_stop_all` in
  `rp_seek`).
- **Select screen, the same fighter on both sides**: TheFECharacters has
  one record per fighter, drawn for each side; the second
  `port_smooth_pose` call took the first one's frames for the game's blend
  and started over every draw (no smoothing, the smoothed pose remade).
  Now once a frame per PLAYER (`port_frame_no`, counted in `limeBegin`).
- **VSYNC** option: umk3.ini `vsync=0/1`, the game's options menu and the
  launcher's video page; OpenGL swap interval, Direct3D 12 interval 0,
  Vulkan mailbox/immediate. Applied at once.

**Death screams (open).** Mary's last replay
(`20261029-131108`, Liu Kang's decapitation) and the log of the fight it
recorded show **one** death scream per fatality from the game logic
(`group_sound`, kind 9); the repeats in the replay's log are Mary going
back over the end. So the logic does not ask twice; what is heard twice
is still unknown. Needs Mary to say which sound repeats and where
(fight or replay, which fatality).

- **STAGE SELECT** after the fighters in VS (GameMode 6), training (2)
  and online (1, the host; the guest takes the host's level and now waits
  up to a minute for it). `Task_GameInit` step 0 waits on
  `stage_menu_gate` (`debug_menu.c`), and the window takes the frame as
  the settings menu does, over `FE_SELECTFIGHTER`: the stage's picture
  large with arrows, or a list of all of them with their pictures (LIST,
  TAB, the pad's Y); CANCEL goes back to the select screen (online, the game's own stage),
  SELECT (Enter, A, START) picks. The pictures are the game's `CS_BG` ..
  `CS_BG4` (960x640 on a 1024 sheet): Subway, Bell Tower, Soul Chamber
  (a guess from the picture), The Graveyard, Temple. The other stages show
  their name only. A debug fight and a replay skip it.
- **Online select: START presses PLAY**, as in VS and Arcade.

## After 0.1.13: an update stopped by a leftover update\new (2026-10-29, released in 0.1.14)

Mary's update failed with "ENOTEMPTY: directory not empty, rmdir
...\update\new\UMK3-PC-0.1.11-beta\launcher\bin\resources". `unpack`
(`launcher/app/lib/update.js`) cleared `update\new` with one `fs.rmSync`.
An earlier update's copy was still there: apply.ps1 removes it with
`-ErrorAction SilentlyContinue`, and Windows would not let a file in it go
(the usual holder is an antivirus still reading what was just unpacked).
So the update threw before it began.

Now:
- `unpack` retries the removal for two seconds (`maxRetries`).
- What still will not go is left, and the update unpacks into
  `update\new-<time>` beside it.
- Every leftover `new*` is tried again at the next update.
- apply.ps1 / apply.sh remove the folder that update used.

Checks: a test case in `test/update.test.js` (a removal that throws
ENOTEMPTY, then the next update clearing both); launcher tests 14 pass.

The fix runs in the launcher doing the update, so it helps from the next
release on: a launcher of 0.1.13 or older still stops on a leftover. Until
then, close the launcher, delete the `update` folder by hand and update
again (or download the release zip).

## Release 0.1.13 beta (2026-10-29): replays, online delay by ping, Vulkan / Direct3D 12 stutters, 4 GB

Published as the GitHub pre-release `v0.1.13-beta`, packaged as 0.1.12 was:
`UMK3-PC-0.1.13-beta.zip` (`launcher/make_release.py` over `electron-builder
--win dir`) and `UMK3-PC-0.1.13-beta-linux-x64.tar.gz` (`electron-builder
--linux dir`, then `launcher/make_release_linux.py`). It carries the five
"After 0.1.12" sections below. Before publishing, the game was built from
the 1.2.59 .ipa in the unpacked Windows package. Known problems taken off at
Mary's word: Reptile's fatalities (it was not Reptile's alone) and the
Vulkan / Direct3D 12 stutters (fixed below). **Mary has not played this
build yet.**

## After 0.1.12: REPLAYS (2026-10-29, in 0.1.13)

Not yet played by Mary. Asked for so that a fight can be watched again to
find bugs and take logs of them (Melee Unlocked's replays as the model).

**Recording** (`rollback_probe.c`, `GameCode.c`). The probe's recorder
(`UMK3_REPLAY_RECORD`, which replayed a fight identically over 3038 ticks)
now records every fight but an online one, by default, to
`replays/recent/<date>_<p1>_vs_<p2>.umkr` beside the exe; only the newest 5
stay, and one shorter than a second is not kept. umk3.ini `replays=0` turns
it off. The file is the setup -- the fighters, the level, the mode, the ten
`Settings` words and training (`PortPractice`), 18 words now -- the two
seeds, then each tick's two engine words and its result's hashes. A replay
takes the recorded settings back for its one fight, since the buttons and
the difficulty change how it plays. While recording the frame step is pinned
to 1, as online: below 60 FPS the game slows instead of skipping ticks.
Online fights are not recorded yet (their ticks go through `rollback.c`).

**Playing** (`umk3-game --replay replays/<folder>/<file>`, a path inside the
exe's folder). The game goes straight into the fight, as `--fight` does, and
`rp_frame` steps it in `GameCodeMain`'s place. A panel along the bottom
(`debug_menu.c`, drawn like the launcher: violet glass, light rules, buttons
with a rim, play lit) holds the status line, a bar to click or drag, and
buttons: to the start, back 5 s, back a frame, play / pause, on a frame, on
5 s, slower / faster (x1/4 .. x4), and the settings menu; each names itself
when the mouse is over it. Keys: Enter or P play / pause, left / right 5 s
(paused, a frame; held, they repeat), up / down speed, M the start, Esc the
settings. Going back loads the nearest keyframe (a rollback snapshot,
`st_rb_*`, every 120 ticks or more, 47 at most) and runs the ticks between
silent and undrawn, 20 ms of them a frame. Pausing draws the next frame and
puts the state back. At the last tick the player stops. Each tick's result is
compared with the recorded one: a difference shows on the panel and goes to
the log (`replay: tick N DIFFERS`, then `replay: the end ... the same as the
recording` or `DIFFERS`). Windows only for going back and pausing: the
snapshots are Windows only.

**Launcher.** A REPLAYS line on the menu (the eight lines re-centred on the
ring) opens the page (`SecReplays.svelte`, `lib/replays.js`): the keep-the-
last-5 switch, the last fights and the kept ones, each with WATCH, KEEP FOR
GOOD (recent to `replays/saved/`), EXPORT… and DELETE, and IMPORT… and OPEN
FOLDER for the kept ones. `play` passes `--replay` to the game.

**The player's top corners** (asked after the first look at the plan). A
REPLAYS button at the top left opens a list of the last fights and the kept
ones (the newest 8), the one playing marked; picking another switches to it
in the same game: this replay ends (logged as `switched to another
replay`), the game leaves the fight as the debug menu's FIGHT does and the
`--fight` path starts the new one's fighters and level, behind a LOADING
screen with its name; the sticks rest and the keys and mouse do nothing on
the way (`rp_switching`). (The first version started a second copy of the
exe instead; Mary asked for no restart.) An X at
the top right closes the game. Nothing else is drawn across the top, so the
health bars stay in sight; the replay's name moved into the status line.

Checks: Windows build without warnings in the new code, Linux compile 84/84,
launcher tests pass, the page and the menu seen in launcher captures. The
panel and playback have not been seen in the game yet.

## After 0.1.12: online lag over the internet (2026-10-29, in 0.1.13)

Not yet played by Mary. She saw lag in an online fight against another PC.
No 0.1.12 online log was on hand; the last one (0.1.11, over EOS) showed, per
600 frames, 75 to 128 holds (a frame shown again while waiting for the other
side), rollbacks always at the deepest the window allowed (7), and the other
side 6 to 9 frames behind. The input delay was fixed at 2 frames, enough on a
local network, not over the internet. Changes (`rollback.c`, `mp_net.c`):

- Inputs go on the wire when they are taken (`mpnet_flush` ticks EOS), not at
  the next frame's poll: about a frame less each way.
- The input delay follows the round trip, measured from the other side's
  acknowledgements: once a second, one frame toward the delay that keeps
  rollbacks near 3 frames, between 2 and 6. Each side's delay is its own (the
  other only receives inputs by frame), so it can move mid-fight: frames it
  skips take the same input, a frame it overlaps drops the newer one.
  `UMK3_RB_DELAY` still fixes it.
- Rollback window 8 -> 12 frames (4 more snapshots, ~25 MB).
- The log's `rollback: frame ...` line and NET STATS show the ping, the delay
  and the longest rollback in ms.

Also seen in that 0.1.11 log, not touched: the second fight (this side player
2) logged a DESYNC every second. Built into Mary's 0.1.12 test folder; not
yet tried between two PCs.

## After 0.1.12: REAL-TIME SHADOWS' note on a row of its own (2026-10-29, in 0.1.13)

Mary saw the note beside REAL-TIME SHADOWS' value ("WITH VULKAN OR DIRECT3D
12") drawn over the label: the row shrinks its text to fit, down to a floor,
and this one was still too long. The note is now a dimmed row of its own under
the option, shown, not chosen (`settings_menu.c`): ONLY WITH VULKAN OR
DIRECT3D 12 on OpenGL, EXPERIMENTAL on the others. Seen right by Mary.

## After 0.1.12: the game's memory below 4 GB, limeMalloc's heap 2 GB (2026-10-29, in 0.1.13)

Not yet played by Mary.

The 64-bit build kept the game's heaps below 2 GB, because a `long` holding an
address was feared to sign-extend. That fear was checked two ways.

**The compiler.** Clang's `-Wint-to-pointer-cast` was run over every source
of the 64-bit build. It found 320 integer-to-pointer casts, and all of them
are from `uint32_t`, which zero-extends: `P32` is `__ptr32 __uptr`, and the
game's `uintptr_t` is `uint32_t`. None are from a signed type, and
`-Wint-conversion` found no implicit ones.

**Running it.** With `UMK3_HIGH_MEM=1`, a test switch in `savestate.c`'s
`reserve_low`, every region starts at 2 GB: the arena at 0x81000000 and the
heap at 0xC1000000. With all of the game's memory above 2 GB, the game:
- booted;
- fought;
- went through FINISH HIM, the continue screen, character select and the main
  menu (8,000 ticks);
- ran a 6,000-tick fight with a finisher request, with no fault.

What changed:
- `reserve_low` now searches below 4 GB, still first fit from the bottom. The
  game's memory sits where it was; only what it never used before can go past
  2 GB.
- limeMalloc's region (`lime_platform.c`) now tries 2048 MB first (was 768) and
  gets it: `lime_heap: 2048 MB`. It is a reservation, and pages are committed
  as they are used. A fight with SMOOTH MODELS uses about 386 MB in all.
- The save-state arena stays at 1024 MB.

Linux keeps `MAP_32BIT`, so it stays below 2 GB. It falls back through the
size list to what fits. Linux compile: 84/84.

## After 0.1.12: Vulkan / Direct3D 12 stutters, compiled shaders, Sub-Zero's strap (2026-10-29, in 0.1.13)

Not yet played by Mary.

**The stutters on Vulkan and Direct3D 12 (Mary's 180 Hz screen).** Each 60 Hz
tick was presented with a swap interval of one refresh (5.6 ms at 180 Hz).
The frame's work (10-12 ms) varies by a few ms, and a frame that ended just
past a refresh was shown one refresh later. The result was 22 ms, then 11 ms,
several times a second. That is what was measured on 0.1.12: 150-330 of
~2,400 frames late. Changing vsync made no difference, because the late
refresh came from where the frame ended, not from vsync.

Now, on a 120/180/240 Hz screen, each frame stays up 2/3/4 refreshes
(`rgl_swap_interval`, set in `game_main.c`'s `pace_frame`):
- Direct3D 12 presents with that sync interval.
- Vulkan's FIFO has no interval, so `rgl_vk.c` keeps a copy of the frame and
  presents it again once for each extra refresh.
- The loop is paced as on a 60 Hz screen.

Measured on Direct3D 12 and Vulkan at 180 Hz:
- Swaps hold at 16.66 ms with 0 late frames a second through a fight.
- A few frames are still late when the frame's work goes past 16.7 ms.
  `UMK3_LOG_PACE=1` now prints the late frames a second.

OpenGL is unchanged.

**Where a fight's frame time goes (measured, D3D12, SMOOTH MODELS 2).**
- `rgl_ff` (transform, index gather, hand-off): ~3 ms.
- Catmull-Clark of the two fighters: ~7 ms.
- With 1 / 4 / 8 threads, the intro had 64 / 38 / 23 frames over 13 ms, with
  smoothing averaging 12.4 / 9.9 / 9.7 ms. The pool stays at 4.
- Open: a fight intro still has frames of 15-19 ms of CPU work. The next
  step is to make the smoothing cheaper.

**COMPILE SHADERS (Khronos' pipeline_cache sample, Microsoft's
D3D12PipelineStateCache).**
- Every pipeline a backend makes is a line in `umk3-pipelines-vk.txt` /
  `umk3-pipelines-d3d12.txt` beside the game, and the driver's compiled
  pipelines are kept in `umk3-pipelines-vk.bin` (`VkPipelineCache`) /
  `umk3-pipelines-d3d12.bin` (`ID3D12PipelineLibrary`, names carrying the
  shaders' hash).
- When the backend opens, it makes them all again, from its own seed list
  (`render/rgl_pipes_*.h`: menus and two fights, 18 / 17 pipelines) and the
  file. This takes 1-7 ms, and no pipeline is compiled in the middle of a
  fight.
- `umk3-game --compile-shaders` does only that and exits.
- The launcher's PLAY has COMPILAR SHADERS ANTES DE JUGAR
  (`compile_shaders=1`), which runs it before the game.

**Sub-Zero's strap stretched to his fist (Mary, his intro, player two, with
SMOOTH MODELS).** The weld was measured on the first pose a skin is drawn in.
In Sub-Zero's intro his hand touches the strap there, and the two were welded
for good. `DrawSkinnedMesh2` (armv7 0x608d8) unwelds the skin by UV: each
record vertex is a copy of the skin vertex its corner names (SKININFO +0x18).
The weld now joins two vertices only when, besides being close, their skin
vertices have the same four bone bytes and weights (+0x20, +0x24):
`umk_subsurf_new_keyed`. For his mesh, 3 of 670 welds were refused, and the
strap stays on his back (captures at ticks 105, 108 and 159). With SMOOTH
MODELS off the strap was always right.

Checks:
- rgl_test: 0 failed on OpenGL, Vulkan and Direct3D 12.
- Linux compile: 84/84.
- Launcher tests: pass.

## Release 0.1.12 beta (2026-10-29): fatality lag, several cores, no vertex-buffer ceiling, shadows, flawless

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

**Open: random stutters on Vulkan and Direct3D 12 (Mary, RTX 4060: OpenGL
smooth).** Measured on Direct3D 12 (Liu Kang vs Sonya, practice, 40 s, a
180 Hz screen): ~150-330 of ~2,400 frames take 20-23 ms instead of 16.7,
then one ~11 ms frame (the next tick is already due); they come about every
100 ms. Ruled out: vsync (`Present(0)` changed nothing, 289 vs 318), GPU
waits (2 a frame, 0.6 ms), textures rewritten mid-frame or read back (0).
The frame work before the present is ~10-11 ms on Direct3D 12 against
~5.5 ms on OpenGL (the `pace:` log), so the CPU side of rgl_ff (transforming
and expanding every vertex) is the next place to look. Not fixed.

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

## Release 0.1.11 beta (2026-10-23): updates that finish, window resize, loads, profile picker, sharp edges

Everything below, "After 0.1.10b", plus Kung Lao's hat and the Esc menu's
picker. Not played by Mary yet.

**Kung Lao's hat pierced by his head (Mary, with a picture).** Measured in
the rest pose with the viewer's own code: 4 head points through the hat
unsmoothed (the original's), 13 at level 1, 45 at level 2; sharp edges
change nothing. The cause is SMOOTH MODELS' correction pass (added for Jax
and Sektor): it makes the surface pass through the model's vertices, and
between them a convex part bulges past the faces; the hat is a scene mesh on
the head bone and is not smoothed. Tried: smoothing the hat too made it worse
(24 / 102: the cone's tip rounds down onto the head); lowering the
correction everywhere fixed it but doubled every fighter's shrink. Done: the
skin inside a piece on its bones (its box, 2 cm wider) gets no correction --
`umk_subsurf_set_fit` per vertex; the game boxes each bone-attached scene
mesh `LIME_RenderScene` draws, for the skin `RenderPlayer` names (a frame
uses the boxes of the one before); the viewer from the attachments it shows.
Measured: 0 and 0 points through the hat, the rest of the body as before.
`tests/subsurf_test.c` I covers the per-vertex share. The game was not run:
its log says `smooth models: N vertices of a skin under a piece on its
bones` the first time it engages -- if Mary's log never shows it for Kung
Lao, the box is in the wrong space.

**The Esc menu like the launcher.** PROFILE's background row opens the
launcher's gradient picker, drawn in software by `plat_ui_gradient`
(`ui_alert.c`) from the component's CSS (colours, sizes, the dial, the 18
presets eight across, the CSS and COPY / CLEAR), over the menu as the
colour list is; the mouse drags the dial and clicks the rest, the keys and
the pad move between the controls (left / right turn the dial 5 degrees, as
the launcher's arrow keys). FROM and TO open the game's colour list. OUTFIT 1
/ 2 is a row. GRAPHICS gains SHARP EDGE ANGLE and SHARP EDGE PRESERVATION
(5 degrees and 5% a press), off with SMOOTH MODELS, as in the launcher. The
launcher's fullscreen frame picture is not in the game (it picks a file).

## After 0.1.10b: updater, window resize, load times, profile picture, sharp edges (2026-10-23, released in 0.1.11)

What Mary reported on 0.1.10b, in the order she sent it, and what was done.
Nothing here has been played by Mary yet; a test build is in
`WORK/test-after-0.1.10b/` (main checkout).

**Launcher update "deletes the game, asks to download everything again".**
Reproduced with the real 0.1.10 and 0.1.10b zips in a scratch folder.
`lib/update.js` mirrored `decomp\`, `runtime\`, `tools\` and `launcher\`:
`decomp\` and `runtime\` have not been in a release since 0.1.6, robocopy
answered 16 for them, and **every Windows update since then ended as "could
not be copied"** (Mary's own `update\apply.log` shows it); the archive stayed
in `update\` (four of them, 570 MB, in her folder) and with 0.1.10b's
self-offer the launcher asked for the same download again. Mirroring `tools\`
whole also **deleted `tools\eos\`** (the EOS SDK and `eos.ini` that 0.1.10
shipped). Now only `src\`, `launcher\` and `tools\build\` are mirrored, and
only when the release has them; the rest is copied without deleting; the
unpacked copy and every archive go either way. Checked: a 0.1.10 install with
a fake game, mods, saves and `tools\eos\` updated to 0.1.10b ends as
installed, keeps all of those, and drops a source the new release lacks.
The game itself (exe, `res\`) was never touched by the script; what Mary saw
as the game deleted is not explained beyond the failed copy -- if it happens
again, `update\apply.log` and the launcher's state are what to look at.

**Losing to Motaro in the debug menu softlocks.** Not reproduced and not
changed: no log of it on this machine, and the code paths (DBG_BOSS, the
debug fight's save/restore in `game_main.c`, `QuitAsLose` -> the continue
screen) do not show it by reading. Needs Mary's `logs\umk3-*.log` of that
session and the exact steps (NEXT IS MOTARO, or a debug fight with Motaro as
fighter 2).

**Resizing and moving the window a few times crashes.** Three causes found
and fixed, none seen crashing here (the game is Mary's to run):
- Direct3D 12: after `ResizeBuffers` the back buffer index was not read again
  (drawing into one buffer, presenting another), and a failed resize went on
  with no buffers. Now the index is re-read, a failed resize keeps the old
  buffers, and drawing is skipped while there are none.
- Vulkan: a failed swapchain re-creation left a retired handle in use; now
  there is no chain until the next frame tries again. The chain was compared
  to the surface's extent, which can be a pixel off the window's, and was
  re-made every frame.
- The render buffer (`render_alloc`, multisampled and window-sized) was made
  again on every frame of a drag; now once the size has held 0.25 s.

**EOS on other PCs.** 0.1.10b was packaged in a worktree without `TOOLS/eos`
and went out with online on the local network alone. `make_release.py` and
`make_release_linux.py` now refuse to package without the SDK and `eos.ini`
(`UMK3_EOS_DIR`, else `tools/eos`, else `TOOLS/eos`; `UMK3_NO_EOS=1` on
purpose). At Mary's request the SDK is now in the repository, only what
online needs: `tools/eos/SDK/Include` (the 45 headers `mp_eos.c` includes on
Windows and Linux, checked by compiling it for both), `tools/eos/SDK/Bin`
(`EOSSDK-Win64-Shipping.dll`, `libEOSSDK-Linux-Shipping.so`) and Epic's
`ThirdPartyNotices`. The product's `eos.ini` (its client secret) is not: the
session's safety checks refused to publish it, so the release scripts take it
from `UMK3_EOS_INI` or the main checkout's `TOOLS/eos/eos.ini`. Without it a
build plays online on the local network alone.

**The "image" file.** `launcher/aditional textures/image.png`, the home
screen ring's picture, is now in the repository (Mary: "inclúyela"), so
every release and compile carries it; the launcher also looks for it in the
release's `launcher\` folder before the first compile.

**Load times.**
- D3D12 and Vulkan: every texture upload made its own command list, submitted
  it and waited for the GPU -- hundreds of waits per load. Uploads are now
  recorded in one list that runs before the next draws.
- Load screens run 100 ms of steps per frame, not 50 (the draw and the vsync
  wait took a fifth of every load).
- The launcher's compile is incremental (an object is kept when its arguments
  match and nothing in clang's `.d` is newer); the `.ipa`'s files are written
  only when their CRC changed (`res/.extracted.json`), the generated tables
  only when their text did. A compile with nothing changed: 13.9 s -> 1.2 s
  here; extraction 3.3 -> 0.5 s, sources 10 -> 0.1 s.
- The 77 s "loading: menus" in one of Mary's logs is the settings menu left
  open during a load, not a load.

**Lag and memory over a long session.** Textures are looked up by name every
frame (the menus' button pictures, hints, the move list), and the lookup was
a `strcmp` walk of every texture loaded so far -- it grew with every stage and
fighter visited. Now a hash. Textures are still never freed (by design, see
`savestate.h`): memory grows with the distinct textures a session sees, up to
the game's set. The log gets a line every five minutes, `memory: N MB, N
textures (N MB of pixels), after N min`, to see whether anything else grows.

**Character editor error with Motaro / Shao Kahn.** Motaro's strike table
names `stk_dummy` three times and the hit box drawing keyed its boxes by name
(Svelte refuses duplicate keys): his page failed. Each strike is drawn once.

**Profile: gradient picker and the second outfit.** Mary's `GradientPicker`
(two colours, an angle dial, presets) replaces the colour / shade / second
colour boxes in PROFILE; the icon can show the fighter in his second outfit
(`<NAME>_VERSUS2.pvr`, framed as the first one's PNG -- measured: the PNG is
the .pvr shrunk 2.12 times on its bottom left; Sonya has no second). The
game draws the same icon; `profiles.ini` gains `|angle|alt` (older files and
versions read as before); online carries both in the look word's high bits.

**Sharp edges for SMOOTH MODELS.** Mary's two specifications (automatic sharp
edge detection; two sliders). In `umk3_subsurf.c`: each interior edge's
dihedral angle, measured once on the positions the mesh is made with; an edge
over the threshold gets Blender's crease (`strength * levels`), and CCG's own
crease and corner rules keep it -- fractional strength is Blender's
semi-sharp crease. Open edges keep their crease; three-face and degenerate
edges are never sharp; 180 degrees or 0% is the old subdivision exactly.
Default 80 degrees, 100%: over the fighters' 74,646 interior edges 38% turn
by more than 30 (Blender's auto smooth angle would make them blocky), 9.6% by
more than 80. Sliders in GRAPHICS (under SMOOTH MODELS) and in the model
viewer (live), `umk3.ini sharp_edge_angle= sharp_edge_strength=`; the game
reads them at start. `tests/subsurf_test.c` A-H all pass (cube corners kept
exactly, smooth tube unchanged, threshold monotonic, levels 1-3, corners,
open sheet, non-manifold/degenerate, regression).

**Tests run:** `tests/rgl_test.c` on OpenGL, Vulkan and D3D12, x64 and x86:
0 failed. `tests/subsurf_test.c`: all checks passed. Launcher tests
(`extract`, `ini`, `update`): pass. Full compile of the game with the
launcher's pipeline: OK. The game was not run.

## Release 0.1.10b beta (2026-10-23): video settings, two columns

Published as the GitHub pre-release `v0.1.10b-beta` (package version
`0.1.10-beta.b`), packaged as 0.1.10. New: the settings screen's
VIDEO SETTINGS box (`FE_Task_Settings`, port-only BOXRT at 0x145, 0x59,
answer 5, `set_menu_open_video`); two columns in every settings list
(`ui_menu_split` in debug_menu.c breaks at a "\002" heading, half way when
there is none or a column would pass 16 rows; `plat_ui_menu` and
`PortControlsPanel` take that split; `ui_geo_row` maps the mouse); a row
too long for its column is drawn smaller (`al_width`, `rich_width`);
CONTROLS split into FIGHT / MENU headings. **Mary saw the first version**
(columns split half way) and asked for whole sections and the FIGHT / MENU
split; that version is built but not yet seen by her. Left/right still
change a value; they do not move between columns.

**Tried by Mary at this release:** the Linux package works on a Linux
machine (after the Linux fixes of 0.1.6-0.1.10), and the Vulkan and
Direct3D 12 renderers play. Neither is a known problem any more.

**Launcher bug in the published 0.1.10b:** `compareVersions` cut
`0.1.10-beta.b` at the `-` and read it as 0.1.10, so this launcher offers
`v0.1.10b-beta` as an update to itself (noted in the release's known
problems). Fixed in `lib/update.js` with test cases after publishing
(PR #117); GitHub's immutable releases refuse new assets, so the fix
ships with the next version.

## Release 0.1.10 beta (2026-10-23): kombos, SMOOTH MODELS, GPL

Published as the GitHub pre-release `v0.1.10-beta`, packaged as 0.1.9 was:
`UMK3-PC-0.1.10-beta.zip` (`launcher/make_release.py` over `electron-builder
--win dir`) and `UMK3-PC-0.1.10-beta-linux-x64.tar.gz` (`electron-builder
--linux dir`, then `launcher/make_release_linux.py`). It carries everything
in the three "After 0.1.9" sections below, and the kombo fix (issue #114).
Before packaging, the game was built from the 1.2.59 .ipa in Mary's test
folder (64 and 32 bits, both succeed). **Mary has not played this build
yet**: the kombo fix, the TEXTURES page, the folders and the animated stage
export are untested by her; the repeated death screams are still open.

## After 0.1.9: smooth models fixed in the game, stage animation export, folders, TEXTURES page (2026-10-23, released in 0.1.10)

**SMOOTH MODELS: dark wedges and black spots (fixed, Mary confirmed both in
the viewer and the game).** Two causes:
- UVs: Blender 2.79's subdivided UV mesh moves a UV island's rim like any
  boundary, and where the rim bends inwards it leaves the island and samples
  other parts of the texture. Now each output UV is its face's own corner UVs
  mixed with the loop weights (`umk3_subsurf.c` build_output), so it stays in
  the face's UV triangle (Blender 2.91+ "UV Smooth: None"). The UV mesh still
  decides which output vertices are shared. Checked on a flat grid with UV =
  position: interior UVs equal positions (level 1 exact, level 2 within 0.01).
- Light: the game took the skin's triangles and positions but the frame
  record's light through the record's indices; where those differ a corner
  took another vertex's light, some near black. `smooth_models.c` now builds
  the surface from the frame record alone, as `LIME_RenderMeshSingleIndexed`
  draws it: its indices, UVs and light per record vertex, positions from
  `RenderVerts`. A cache entry is keyed by the skin and an FNV-1a hash of the
  record's indices and UVs.

**Builds into the test folder take `src/` from the test folder**, not from the
repository (`node launcher/app/cli.js <ipa> --arch x64 --root <folder>`
compiles `<folder>/src`). Copy the changed sources there first, then check the
exe holds a string from the change. Three builds this session missed their
changes this way.

**Exports and mods by place (Mary).** `launcher/app/ui/src/lib/mods/owners.js`:
- Exports: `exports/Characters/<fighter>/<FORMAT>`, `exports/Stages/<stage>/<FORMAT>`,
  `exports/Models/<model>/<FORMAT>`; a texture alone goes to its fighter's or
  stage's `Textures`, or `Interface`, or `Effects`. The viewer's texture and
  rig exports and the character editor's go to the same places.
- Mods: a replaced texture goes into the mod at the same place
  (`Stages/The Pit III/PIT1.png`); a model imported from Blender into
  `Characters/<fighter>/`. The game takes a mod's files by name from any of
  its folders (`runtime/mods.c` walks them), so nothing changed in the game.
  `lib/mods.js` putFile takes a path and removes a same-named copy elsewhere
  in the mod; exportSave takes a folder path (each part a safe name, four
  deep at most).
- A texture's owner: a texture a stage's meshset uses is that stage's (so
  JADE_SKY is Jade's Desert's, not Jade's); a `*_COMPLETEMAP` is the
  interface's; a stem naming a fighter (at its start first) is that
  fighter's; a texture another meshset uses is an effect; anything else is
  the interface's. 1582 textures sort into 27 fighters (1065), 16 stages (164),
  88 effects and 265 interface.

**TEXTURES page in MODS (Mary: stage and menu textures in their own
section).** Fighters, stages (a list each), interface and menus, effects:
every texture with SEE, REPLACE (PNG) into the chosen mod and EXPORT (PNG);
the mod's own ones are marked.

**Issue #101, animated stage export.** EXPORT > STAGES > WITH ITS ANIMATION
(FBX, USDA, USDC, USDZ): each scene object that is ever shown is a bone of its
own with its mesh bound to it; each frame's key gives the bone its palette
rotation, translation and scale relative to its first shown key, and a key at
alpha 0.03 or below scales it to nothing. FBX gets `Lcl Scaling` curves and
USD `SkelAnimation` scales samples when a pose has a scale. Checked: frame 0
equals the static export exactly; on Scorpion's Lair (1001 frames, 43
objects) every shown vertex at frames 3, 400 and 999 matches the scene's own
placement within 0.0004. **Not opened in Blender or a USD viewer yet.**

**Issue #114 (Liu Kang's 7-hit kombo, the opponent slides away): a real
difference found and fixed, not yet tried in the game.** The chain rules all
match the binary: `t_r_hi_punch` / `t_r_lo_punch` bump the punch count at
proc+0x4c (`inc_p_block`), `t_cc_hi_punch` (6, or 4 for the machine or a
player holding away) and `t_cc_lo_punch` (5 / 3) with `t_ccp3` push the pair
apart through `t_separate_us` once the count reaches the threshold, and
`back_to_normal` (from `t_local_reaction_exit`) clears it. Every writer of
proc+0x4c in the binary was listed (a scan of all `str ..., #0x4c]`):
`inc_p_block`, `t_avoid_corner_trap_b`, `t_block_exit`, `back_to_normal_px`;
the two in `t_combo_air_pause` are its literal pool.

The difference is in the combo recogniser. `combo_scan_a11` walks a combo
table (entries of 0x18 bytes, `field48` moving along) looking for the button
just pressed. On a match the binary writes the list's start to 0x1c only
(0x32780, 0x3278e) and leaves `field48` on the matched entry; the port also
put `field48` back at the start. `t_comb2` then reads the next move from
`field48 + 4` / `+ 0x14`, so in the port any kombo whose button matched a
later entry went on with the first entry's continuation. Only a player's
kombos pass through the scan (`t_comb0`: the machine goes straight to
`t_comb8`). Also fixed: `t_r_combo0` skips `set_no_block` when proc+0x10 bit
0 is set (0x45da8 branches to 0x45d32).

`tools/decomp/factdiff.py` over `mkcombo.c` and the combo and punch
reactions in `mkreact.c` (transcriptions from `armrecomp/recomp.py --func`):
the only differences left are the handler addresses stored into the frame,
which the reader does not resolve to names, and the port's `printf`
diagnostics (`separate us:`, `corner trap:`), still in place until Mary
confirms.

**Death screams in friendships and animalities (open).** `t_friendship_complete`,
`t_end_friend_proc`, `t_friend_ender` and `death_blow_complete` match the
binary. The `death scream:` line now also names the handler the thread loop is
running (`port_mk3_running`, mk3.c) and the caller of `group_sound`. Needs a
log from the current build (its PDB is kept in `WORK/pdb-0928/`).

## After 0.1.9: SMOOTH MODELS, crash report context, GPL (2026-10-23, released in 0.1.10)

**SMOOTH MODELS** (Mary): an option, OFF / LEVEL 1 / LEVEL 2, that draws the
fighters through Blender's Weld and Subdivision Surface (Catmull-Clark)
modifiers, "extracted from Blender's code". Mary chose copying Blender's code
over a clean reimplementation, knowing it makes the project GPL.

- `src/thirdparty/blender/`: Blender 2.79b's CCGSubSurf files unchanged (the
  last Blender whose own C does Catmull-Clark; later ones use OpenSubdiv),
  stand-in headers for the Blender headers they include, and
  `umk3_subsurf.c`: the weld ported from 4.2.0 (`kdtree_impl.h`
  calc_duplicates_fast, `mesh_merge_by_distance.cc` "All" mode: mean of a
  merged group, faces with a repeated vertex removed, the first of faces with
  the same vertices kept) and the subsurf glue from 2.79b `subsurf_ccg.c`
  (positions and UVs as two CCG surfaces, `BKE_mesh_uv_vert_map_create` for
  the UV vertices and seams, `get_ss_weights` for per-corner data). See its
  README for which file is which.
- Game: `src/runtime/smooth_models.c`, called from `RenderAnimatedCharacter`
  (Players.c) for the body, the second skin and the shadow, in place of
  `LIME_RenderMeshSingleIndexed` when the option is on. Positions are the
  skin's (`SkinnedVerts`, from the `DrawSkinnedMesh2` just before), topology
  and UVs the skin's own, the light the frame record's per corner. The weld
  distance is Blender's default, 0.001 m = 0.1 game units (centimetres). One
  surface per skin, dropped when `LIME_FreeSkin` frees it.
- Viewer: SMOOTH MODEL (OFF / LEVEL 1 / LEVEL 2) and SMOOTH next to LOOP for
  smooth animations: the same C as WebAssembly (`mods/subsurf.js`,
  `scripts/build-subsurf.js` regenerates `subsurf_wasm.js`; the CSP allows
  'wasm-unsafe-eval'), and frames blended like the game's GetSlerpedQ
  (`rig.js` framePoseBlend).
- Option: `smooth_models=0|1|2` in `umk3.ini`, the launcher's GRAPHICS > VIDEO,
  the in-game GRAPHICS > VIDEO row (applies at once).

**Verified:** `tests/subsurf_test.c` (a triangulated cube with seam
duplicates: the weld merges all 16, the result is closed, wound outwards,
inside the cube, UVs and corner values in range; levels 1 and 2). The
WebAssembly gives the same counts. On real skins (Kabal, Kano, Scorpion,
Ermac) no NaN, level 2 evaluates in 1.4-4.4 ms; the open edges left are the
models' own openings (none within 2 units of another shell for Kano, Scorpion,
Ermac; Kabal's 41 near ones are merged by the weld). Both exes build from
Mary's .ipa; 83 of 83 sources compile for Linux. **Not seen in the game or the
viewer yet:** Mary runs it.

**Linux had no sound at all (issue #104, fixed, not heard on Linux yet).**
The Linux package builds with `UMK3_NO_SDL_MIXER` (no library beyond SDL2),
and in that mode every audio entry point of `platform/sdl_audio.c` was empty.
Now that mode mixes on an SDL2 device itself: 16 effect voices (the mixer
path's conversion and voice stealing) and one music stream, the MP3 decoded
by libmpg123 loaded with dlopen when the first song plays (Fedora / Bazzite:
mpg123-libs). Without libmpg123 the log says so and only the music is
missing. 83 of 83 sources compile for Linux. The same issue's 6-button High
Punch on Y and the moves-list button: the SDL pad goes through the same
configured masks (`port_pad_action`, `g_pad6`) as Windows; the report is from
0.1.6, before that configuration existed. Not changed; to be checked again on
0.1.9 or later.

**SMOOTH ANIMATIONS on the select screen and in the intro (fixed, not seen
yet):** there PlayerAutoSmoothAnims blends with its own windows (40, 20)
nearly all the time, and `port_smooth_pose` left any blend of the game's
alone, so the option changed nothing there. With the option on it now blends
from the frame the game's blend is mostly on.

**Open, from Mary 2026-10-23:**
- Death screams repeat in friendships and animalities (fatalities fixed).
  Checked and ruled out: a state that screams and sleeps without moving on
  (none), the sound-number order before each sound call against the binary
  (`str [rX, #0x1c]` before `bl`: three candidates, all three false or
  faithful -- tl_bomb33 stores 0 from `sl`; the script interpreter's opcode 15
  stores into obj and calls ochar_sound(ref), as the binary does). The
  `death scream:` diagnostic names the handler: a log of a friendship and an
  animality from the 2026-10-23 09:28 build (its PDB is in the test folder)
  will name it.
- Issue #114: in Liu Kang's 7-hit kombo the opponent slides away and the
  kombo cannot be finished. Not investigated past the corner rule
  (`t_avoid_corner_trap`).
- Issue #101: export the stages with their animations and textures.

**Mary's first look at SMOOTH MODELS: holes and lag (fixed, not seen yet).**
- Holes in cloth (a fighter's trousers): the weld removed faces with the same
  three vertices everywhere; Blender's does it only among faces touching a
  welded vertex (`weld_poly_loop_ctx_alloc`, `is_vert_ctx`). The game's models
  draw cloth both ways with the same corners, so the back faces went. Now as
  Blender.
- A piece over another showing what is under it (Kano's eye plate, Sektor's
  helmet): Catmull-Clark pulls an open rim inwards. Open edges now get crease
  1.0 (Blender's own crease, creaseFactor = the level).
- Closed pieces that overlap (Jax's head in his neck): Catmull-Clark shrinks
  each, opening a gap. **Not Blender:** one correction pass in
  `umk_subsurf_eval` -- each control point moved by how far its subdivided
  self fell short, then subdivided again -- so the surface passes near the
  model's own vertices (a cube's corner within 0.13 at level 1). Sharp corners
  bulge a little in exchange.
- Lag: every frame did a full CCG sync (hash maps, allocations); now one full
  sync when the mesh is made and CCG's partial sync per frame, and an
  unchanged pose is not evaluated again. Native cost per fighter per frame,
  with the correction pass: ~1.2 ms at level 1, ~4 ms at level 2. The shadow
  draws the plain mesh again. Under Vulkan / Direct3D 12 indexed triangles
  were transformed once per index (`rgl_ff.c`); each vertex is now
  transformed once and gathered (`tris_indexed`), rgl_test 0 failed on all
  three renderers, 64 and 32 bit.

**Crash reports and the session log** start with the version (`UMK3_VERSION`,
passed by the launcher that compiled the game), 32/64-bit, the system
(Windows version by RtlGetVersion, Wine if present; uname on Linux), the
renderer drawing and the one asked, the GPU (GL_RENDERER / GL_VERSION, which
names the card under Vulkan and Direct3D 12 too), the video settings,
language, smooth animations / models and how many mods are on
(`port_report_context`, game_main.c). Linux still has no crash report of its
own; the session log carries the same block.

**Licence:** GPL-2.0-or-later from here on (`LICENSE`); 0.1.9 and earlier stay
MIT.

## After 0.1.9: SMOOTH ANIMATIONS (2026-10-22, released in 0.1.10)

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

**Crash when a fighter takes Kabal's zap (fixed).** Mary's next log printed
`csound: character 297758 out of range (event param 0x48b1e38)`. 0x48b1e38 is
`G + 0x390` in the 64-bit exe with character 10 (Kabal) added as `10 << 8`:
player 0's hit queue, which `uhq_entry` leaves in 0x1c at every reaction
start. `t_r_tusk_zap` (armv7 0x46324), the reaction to Kabal's zap, stores
`field1c = 0` BEFORE `bl his_ochar_sound` (0x46396 / 0x46398); the port had
the store after the call, so the sound number was the pointer. On iOS that
read a wrong entry quietly; on the PC it crashed. `factdiff` compares stores
as a multiset, so a store moved past a call is invisible to it.

**Fatality death screams repeat, fatalities that do not finish (open).**
Every fatality, Mary says. Ruled out: the event queue (cleared at every
`mk3_update`), the scene events (visual only), the rollback probe (off unless
set), `instck` (no -3 refusals the binary lacks). Diagnostics in the build,
to find the handler instead of guessing: `death scream: character N, handler
0x..., token 0x..., level N` (other.c group_sound, kind 9) and `mk3: tick
abandoned, handler 0x... refused token 0x...` (mk3.c). The handler address
minus 0x400000 is its offset in the 64-bit exe (llvm-symbolizer with the PDB).

**Crash when two projectiles meet.** SCORPION vs KABAL, both throwing
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
  Fixed: Mary confirmed it on 2026-10-22.
- **Reptile's fatalities** sometimes did not work fully; in one of them he
  only raised his arms. Taken off the known problems on 2026-10-29 at
  Mary's word: it was not Reptile's alone.
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
