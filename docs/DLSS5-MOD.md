# DLSS 5

Port, not in the binary. Code: `src/runtime/neural_mod.c` and the layers in
`src/runtime/render/rgl_ff.c` / `rgl_d3d12.c`. Experimental.

UMK3 PC runs NVIDIA's DLSS 5 neural rendering, through the community's
DLSS5 ReShade AIO add-on, as a feature of the game:

- The game ships everything except NVIDIA's two DLLs: our own build of
  ReShade, the AIO add-on, VORT's motion estimation, and our layer shader.
- The player adds NVIDIA's DLLs with one button in the launcher and turns
  DLSS 5 on in GRAPHICS.
- The HUD and the menus are left as the game draws them. The fighters and
  the stage each get their own amount of the neural pass, and there are
  presets to choose from.

There's no ReShade overlay, nothing to install beside the exe, and no DLLs
to copy by hand.

> **Status (2026-11-22):** played by Mary in her test folder (HD texture
> mods) on an RTX 5070 (driver 621.24). The default is the whole picture,
> with AIO's own NVIDIA optical flow. That is what Mary saw working:
> `CreateFeature(feature=18) = Success`, NR and DLAA on every frame, 60 fps,
> borderless 1920x1080.
>
> **The layers are experimental and off** (`dlss5_layers=1` turns them on).
> With them on, the logs had the mask bound on every frame
> (`NR-control-mask=BOUND`), and screen captures showed the neural pass on
> the stage with the HUD left clean. Mary, though, saw no effect and asked
> to go back, so they wait on her verdict.
>
> The presets' names are provisional until Mary has judged each network's
> look.

## For the player

1. **An NVIDIA RTX 50 card**, **RENDERER: Direct3D 12**, Windows 10/11.
2. In the launcher's VIDEO > GRAPHICS > DLSS 5, press **ADD NVIDIA FILES**
   and pick `nvngx_dlss.dll` and `nvngx_dlssnr.dll`. Only files with a valid
   NVIDIA signature are accepted, and any file they replace is kept as
   `.old`. UMK3 PC does not ship or download them, because NVIDIA's licence
   does not allow it.
3. In the game, **GRAPHICS > DLSS 5**: ON, then choose:
   - **PRESET**: REALISTIC, CINEMATIC, VIVID or SUBTLE. Each sets one of
     NVIDIA's three networks (`DLSSNR.Style` 0..2, AIO's Model 1..3) and
     the pass's intensity, local tone and local structure.
   - **STAGE** and **FIGHTERS**: 0 to 100 % of the neural pass for each
     layer.
   It all takes effect at the next start. The line under DLSS 5 says what
   is missing, if anything. With `nvngx_dlss.dll` alone, DLSS (DLAA) runs
   without the neural pass.

## The layers: what is and isn't touched (experimental, `dlss5_layers=1`)

With the layers on, every Direct3D 12 draw also writes a code into the
picture's alpha. The colour is never changed (`rgl_layers`). Their mask
needs AIO's VORT guide path (`VortGuides=1`, motion from VORT); with them
off the game sets AIO back to its NVIDIA optical flow and no mask. In
GRAPHICS, STAGE and FIGHTERS are greyed out while the layers are off.

| Code | What | Neural pass |
|---|---|---|
| 0 | the HUD, the menus, everything 2D (no 3D scene camera) | none: left as drawn; DLSS also takes the current frame there, so letters keep no history |
| 0.5 | a fighter (`rgl_shadow_caster`) | `dlss5_fighters=` |
| 1 | the stage (the 3D scene camera, `rgl_shadow_camera`) | `dlss5_stage=` |

The alpha is written with the blend factor: the layer code is weighted by
the draw's own coverage, and an opaque draw writes it whole. Our
`DLSS5_AIO_Feed.fx` reads that alpha from the presented picture and makes
AIO's two masks from it: the neural pass's control mask (1 applies, 0
bypasses) and DLSS's history mask. AIO's original shader is Apache-2.0;
ours lives in `launcher/neural/` and is marked as modified.

## What ships, and where

`tools/neural/stage_dlss5.py` stages the files into `launcher/dlss5/`
(git-ignored), and `launcher/make_release.py` packs them. A release refuses
to build without them, unless `UMK3_NO_DLSS5=1`, and refuses if NVIDIA's
files are there. Every compile copies `launcher\dlss5` to `graphics\dlss5`,
including files that are newer but the same size, and never deletes, so
the player's files survive every update.

| File | From | Licence |
|---|---|---|
| `ReShade64.dll` | ReShade 6.8.0, built from source by `tools/neural/build_reshade.py`, with two changes (below) | BSD-3-Clause |
| `standalone-dlssnr.addon64`, `nvngx.dll` (its caller bridge, not NVIDIA's), `StandaloneBoundary.fx`, `licenses/` | DLSS5 ReShade AIO 2.2.4, 64-bit release zip, SHA-256 `38493d80…d38ee` as published | Apache-2.0 |
| `DLSS5_AIO_Feed.fx` | ours, from AIO's | Apache-2.0 |
| `vort_Motion.fx`, `Includes/` | vortigern11/vort_Shaders at commit b410b9f, SHA-256 checked; motion vectors only (blur and TAA off, its defaults) | MIT |
| `ReShade.fxh` | crosire/reshade-shaders at a fixed commit | CC0-1.0 |

Our ReShade's two changes:
1. Its banner is never drawn.
2. `reshade_reloaded_effects` is sent again once the effects have loaded.
   ReShade sends it only at the start of a reload, before any effect
   exists, so AIO looked our textures up too early, found nothing and never
   looked again. Without this the layers never reached the neural pass.

## What the game does at start

The `dlss5:` lines in `logs\umk3-*.log` say what happened.
`umk3-game --dlss5-status` gives the launcher the same verdict.

1. It runs only with `dlss5=1` (or an old-style `type=dlss5` mod), Direct3D
   12, an NVIDIA card, and no ReShade of the player's own beside the exe.
   Every shipped file must be present, and both NVIDIA DLLs signed.
2. The first builds' DLSS5-Feeder files are set aside (`.off`) along with
   their ReShade.ini and preset (`.old`), which are written again for AIO.
3. The game sets the AIO settings it owns in `ReShade.ini`:
   - the pipeline: VortGuides, NrRejectionMask, no optical flow, no frame
     generation, no counters;
   - the preset: Model, Intensity, LocalTone, LocalStructure;
   - NeuralRendering;
   - the layers' strengths.
4. It loads ReShade from `graphics\dlss5` before the device is made, and
   turns the layers on.
5. **Borderless** is the monitor's exact size while DLSS 5 is on. The usual
   extra row (which keeps OpenGL from going exclusive) made the picture
   taller than the screen, and AIO never started.

Logs: `graphics\dlss5\ReShade.log` (with AIO's lines) and
`%LOCALAPPDATA%\RHI\Logs\standalone-dlssnr.log`.

## Known limits

- **MSAA:** the player's antialiasing is kept. ReShade cannot read
  multisampled depth, so the depth guide is weaker with MSAA on, while the
  motion comes from VORT either way.
- The flicker Mary saw with the first AIO test (frame generation on, the
  odd-height window) has not been checked again since both were changed.
- Once ReShade is loaded it stays loaded until the game closes.
- Online: the feature only changes the picture. It has not been played
  online yet.
