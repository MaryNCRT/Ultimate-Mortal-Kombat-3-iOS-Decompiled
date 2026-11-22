# The DLSS 5 mod

Port, not in the binary. Code: `src/runtime/neural_mod.c`. Experimental.

UMK3 PC can run the community **DLSS 5 neural rendering** chain
(DLSS5-Feeder + a neural consumer + NVIDIA's runtime) as a mod. You don't
need to install ReShade beside the game, open its overlay, or copy DLLs into
the game's folder. Everything lives in one folder in `mods\`, and the
MOD MANAGER (or EXTRAS > MODS) turns it on and off like any other mod.

> **Status (2026-11-22):** checked in the game on an RTX 5070 (driver
> 621.24). Our own build of ReShade 6.8.0 hooks the game's Direct3D 12
> device, loads DLSS5-Feeder 2.0.0, and compiles its shader. The feeder gets
> the colour and the scene's depth (single-sample, at the right size, not
> reversed), opens a same-device D3D12 session and initialises NGX. It looks
> for NVIDIA's DLLs in the mod's folder.
> **Not confirmed yet:** the DLSS and neural pass itself. That needs
> `nvngx_dlss.dll`, `nvngx_dlssnr.dll` and a consumer, which weren't on the
> test machine. Without them the picture is unchanged, as expected.

## What you need

- **An NVIDIA RTX 50 series card.** The feeder's authors say the neural model
  runs on RTX 50 only.
- **RENDERER: Direct3D 12** (settings > VIDEO, or `renderer=d3d12`).
- Windows 10/11 and a recent NVIDIA driver.

## The folder

```text
mods\DLSS 5\
├── mod.ini                       name=DLSS 5 / type=dlss5 / category=graphics
├── ReShade64.dll                 ReShade with full add-on support (below)
├── dlss5-feed.addon64            DLSS5-Feeder 2.0.0 (its GitHub releases)
├── renodx-dlss5.addon64          the neural consumer: renodx-dlss5*.addon64
│                                 or deep-fried-chicken.addon64 (exactly one)
├── nvngx_dlss.dll                NVIDIA DLSS Super Resolution  -- you supply it
├── nvngx_dlssnr.dll              NVIDIA DLSS neural rendering  -- you supply it
└── reshade-shaders\Shaders\
    ├── DLSS5_Feed.fx             from the DLSS5-Feeder release
    └── ReShade.fxh               crosire/reshade-shaders, slim branch (CC0)
```

From the feeder's zip, take only `dlss5-feed.addon64` and `DLSS5_Feed.fx`.
`dlss5-feed-helper.addon64`, `dlss5-feed.addon32`, `host64\` and the layer
folders are for 32-bit or Vulkan games. ReShade loads every `.addon64` in
the folder, so the game refuses the mod when the helper is there.

**ReShade.** `tools/neural/build_reshade.py <work folder>` builds ReShade
6.8.0 (crosire/reshade, BSD-3-Clause) from its source with full add-on
support and one change: its banner is never drawn. It needs git, Python and
the Visual Studio Build Tools, and it copies ReShade's licence beside the
DLL. The official "with full add-on support" build from reshade.me works
too, but it shows ReShade's banner when it starts.

On the first start the game writes `ReShade.ini` and `DLSS5-preset.ini`
into the folder when they are missing, and never overwrites them. They set:
the `DLSS5_Feed` technique on; the feeder's built-in motion estimator
(`DLSS5_MV_PROVIDER=5`); the game's depth as not reversed
(`RESHADE_DEPTH_INPUT_IS_REVERSED=0`); and every ReShade key unbound (no
overlay, screenshot or reload keys, no clock or FPS).

**Where the other files come from.** UMK3 PC does not ship or download
`nvngx_dlss.dll`, `nvngx_dlssnr.dll` or the consumer add-on. They are
NVIDIA's or their authors' files, and nobody here may redistribute them.
The DLSS5-Feeder README names where to get them.

## What the game does

Each step writes a `dlss5 mod:` line in `logs\umk3-*.log`:

| Check | If it fails |
|---|---|
| a mod with `type=dlss5` is on (`mods\mods.ini`) and `dlss5_mod=0` is not set in umk3.ini | nothing happens, no line |
| renderer is Direct3D 12 | "needs RENDERER Direct3D 12" |
| an NVIDIA card is present | "needs an NVIDIA card" |
| there is no ReShade of your own beside the game (`dxgi.dll`, `d3d12.dll`, `ReShade.ini`) | "a ReShade of your own is beside the game" |
| every file above is present, and the 32-bit helper is not | each one named |
| both `nvngx_*.dll` say NVIDIA in their version information **and** have a valid digital signature | "is not NVIDIA's" / "has no valid signature" |

Then ReShade is loaded from the mod's folder before the Direct3D 12 device
is made. ReShade's base folder is the mod's folder, so its settings, its
log, its add-ons and the feeder's log all live there. The folder is also on
the DLL search path, and the feeder's NGX session finds NVIDIA's DLLs
there.

**Antialiasing:** while the mod is loaded, the game draws without MSAA. A
multisampled depth buffer can't be read by ReShade, and DLSS antialiases
anyway. Your `antialiasing=` in umk3.ini is kept for when the mod is off.

## Seeing whether it works

All of these are in the mod's folder:

- `ReShade.log`: ReShade attaching to Direct3D 12, "Registered add-on
  DLSS 5 Feed 2.0.0", and "Successfully compiled DLSS5_Feed.fx".
- `dlss5-feed.log`: "technique found", "depth source: ... the right size",
  "opening same-device D3D12 session", then NGX.
- The consumer's log: "feature 18 created" and "inline feature 18
  evaluation succeeded" mean the neural pass really ran.

`UMK3_DLSS5_PROBE=1` (environment) loads ReShade and the feeder even
without the consumer and NVIDIA's DLLs. It is for checking the hooks and the
feed alone, and DLSS does not run.

## Known limits

- **The first start compiles the feeder's shader: 10 to 25 s.** The game
  plays meanwhile, unprocessed. Later starts take it from ReShade's cache
  (0.07 s).
- **The HUD is processed with the scene.** The feeder works on the whole
  frame. Drawing the HUD after the neural pass is planned
  (`docs/research/GRAPHICS-OVERHAUL.md`, section 4.5).
- Once ReShade is loaded it stays loaded until the game closes. Turning the
  mod off takes effect at the next start.
- Online: the mod only changes the picture, and the fight and its sync are
  the same with or without it. It has not been played online yet.
- The feeder's own panel opens with Alt+C (its settings, its cost per frame).

## Turning it off

Use the MOD MANAGER or EXTRAS > MODS (it takes effect at the next start),
or set `dlss5_mod=0` in umk3.ini, or delete the folder.
