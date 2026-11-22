# DLSS 5

Port, not in the binary. Code: `src/runtime/neural_mod.c`. Experimental.

UMK3 PC runs the community **DLSS 5 neural rendering** chain
(DLSS5-Feeder, a neural consumer and NVIDIA's runtime) as a feature of the
game. The game brings our own build of ReShade and DLSS5-Feeder. The player
adds NVIDIA's two DLLs and the consumer with a button in the launcher, then
turns DLSS 5 on in GRAPHICS. There's no ReShade overlay, nothing to install
beside the exe, and no DLLs to copy by hand.

> **Status (2026-11-22):** checked in the game on an RTX 5070 (driver
> 621.24). Our ReShade 6.8.0 hooks the game's Direct3D 12 device, loads
> DLSS5-Feeder 2.0.0 from `graphics\dlss5`, and compiles its shader. The
> feeder gets the colour, opens a same-device D3D12 session and initialises
> NGX. It looks for NVIDIA's DLLs in `graphics\dlss5`.
> **Not confirmed yet:** the DLSS and neural pass itself. The test machine
> has neither `nvngx_dlss.dll` nor `nvngx_dlssnr.dll`, nor a consumer (the
> NVIDIA driver installs neither DLL), so the picture is unchanged.

## For the player

1. **An NVIDIA RTX 50 card** (the feeder's authors say the neural model runs
   on RTX 50 only), **RENDERER: Direct3D 12**, Windows 10/11.
2. In the launcher's VIDEO > GRAPHICS, **ADD NVIDIA FILES**: pick
   `nvngx_dlss.dll`, `nvngx_dlssnr.dll` and the consumer add-on
   (`renodx-dlss5*.addon64` or `deep-fried-chicken.addon64`). The launcher
   copies them into `graphics\dlss5`:
   - NVIDIA's DLLs only with a valid NVIDIA signature;
   - only one consumer, and the old one is kept as `.old`;
   - a file it replaces is kept as `.old`;
   - any other file is refused.
   UMK3 PC does not ship or download these three files, because they may not
   be redistributed. The DLSS5-Feeder README names where they come from.
3. Turn on **DLSS 5**, in the launcher (same page) or in the game (GRAPHICS >
   DLSS 5). It takes effect at the next start. The line under it in the
   game, and STATUS in the launcher, say what is missing, if anything.

## What ships, and where

`tools/neural/stage_dlss5.py` stages the files into `launcher/dlss5/`
(git-ignored), and `launcher/make_release.py` packs them. A release refuses
to build without them, unless `UMK3_NO_DLSS5=1`, and refuses if NVIDIA's
files are there. Every compile copies `launcher\dlss5` to
`graphics\dlss5` beside the game (`launcher/app/lib/build.js`). It only
copies over and never deletes, so the files the player added survive every
update.

| File | From | Licence |
|---|---|---|
| `ReShade64.dll` | ReShade 6.8.0 built from source by `tools/neural/build_reshade.py`, full add-on support, banner left out | BSD-3-Clause (`ReShade-LICENSE.md`) |
| `dlss5-feed.addon64`, `reshade-shaders\Shaders\DLSS5_Feed.fx` | DLSS5-Feeder 2.0.0 release zip, SHA-256 `fa41fe2c…62584` as published | MIT (`DLSS5-Feeder-LICENSE.txt`) |
| `reshade-shaders\Shaders\ReShade.fxh` | crosire/reshade-shaders at a fixed commit, SHA-256 checked | CC0-1.0 |

On the first start the game writes `ReShade.ini` and `DLSS5-preset.ini` in
`graphics\dlss5` when they are missing, and never overwrites them. They set:
the `DLSS5_Feed` technique on; the built-in motion estimator
(`DLSS5_MV_PROVIDER=5`); the depth as not reversed; and every ReShade key
unbound (no overlay, screenshot or reload keys, no clock or FPS).

## What the game checks

Each step writes a `dlss5:` line in `logs\umk3-*.log`.
`umk3-game --dlss5-status` prints the same verdict for the launcher.

| Check | If it fails |
|---|---|
| `dlss5=1` in umk3.ini (or an old-style mod with `type=dlss5` is on) | nothing happens |
| renderer is Direct3D 12 | "needs RENDERER Direct3D 12" |
| an NVIDIA card is present | "needs an NVIDIA card" |
| there is no ReShade of the player's own beside the game (`dxgi.dll`, `d3d12.dll`, `ReShade.ini`) | "a ReShade of your own is beside the game" |
| the shipped files are present, and the 32-bit helper add-on is not | "not installed" |
| both `nvngx_*.dll` say NVIDIA and carry a valid signature, and there is a consumer | "needs NVIDIA's files" |

Then ReShade is loaded from `graphics\dlss5` before the Direct3D 12 device
is made. Its base folder, its add-ons and the feeder's log are all there,
and it never picks up add-ons from anywhere else.

**Antialiasing** is kept as the player set it (Mary). With MSAA on, the
feeder cannot read the scene's depth (ReShade cannot read a multisampled
depth buffer), so its motion estimator works from the picture alone. With
antialiasing off, it gets the depth too, single-sample and at the right
size.

## Seeing whether it works

All of these are in `graphics\dlss5`:

- `ReShade.log`: "Registered add-on DLSS 5 Feed 2.0.0", "Successfully
  compiled DLSS5_Feed.fx".
- `dlss5-feed.log`: "technique found", "opening same-device D3D12 session",
  then NGX.
- The consumer's log: "feature 18 created" and "inline feature 18
  evaluation succeeded" mean the neural pass really ran.

`UMK3_DLSS5_PROBE=1` (environment) loads ReShade and the feeder without
NVIDIA's files, to check the hooks and the feed alone.

## Known limits

- The first start compiles the feeder's shader in 10 to 25 s, and the game
  plays meanwhile, unprocessed. After that it comes from the cache.
- **The HUD is processed with the scene.** Drawing the HUD after the neural
  pass is planned (`docs/research/GRAPHICS-OVERHAUL.md`, section 4.5).
- Once ReShade is loaded it stays loaded until the game closes.
- Online: the feature only changes the picture. It has not been played
  online yet.
- The feeder's own panel opens with Alt+C.
