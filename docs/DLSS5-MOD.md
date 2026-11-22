# The DLSS 5 mod

Port, not in the binary. Code: `src/runtime/neural_mod.c`. Experimental.

UMK3 PC can run the community **DLSS 5 neural rendering** chain
(DLSS5-Feeder + a neural consumer + NVIDIA's runtime) as a mod. You don't
need to install ReShade beside the game, open its overlay, or copy DLLs into
the game's folder. Everything lives in one folder in `mods\`, and the
MOD MANAGER (or EXTRAS > MODS) turns it on and off like any other mod.

> **Status:** the game side (finding the mod, checking it, loading it from
> its folder) has been built and tested. A full run with every component
> present has **not been confirmed yet**. Until a log shows the neural feature
> created and evaluated (below), treat it as experimental.

## What you need

- **An NVIDIA RTX 50 series card.** The feeder's authors say the neural model
  runs on RTX 50 only. On RTX 20/30/40, DLSS runs but the neural pass fails.
- **RENDERER: Direct3D 12** (settings > VIDEO, or `renderer=d3d12`).
- Windows 10/11 and a recent NVIDIA driver.

## The folder

Create `mods\DLSS 5\` (any name works) with this layout:

```text
mods\DLSS 5\
├── mod.ini                       you write it (below)
├── ReShade64.dll                 ReShade 6.8 or later WITH ADD-ON SUPPORT
├── dlss5-feed.addon64            DLSS5-Feeder (github.com/jlrouzies-fr/DLSS5-Feeder, releases)
├── renodx-dlss5.addon64          a neural consumer: renodx-dlss5*.addon64
│                                 or deep-fried-chicken.addon64 (exactly one)
├── nvngx_dlss.dll                NVIDIA DLSS Super Resolution  -- you supply it
├── nvngx_dlssnr.dll              NVIDIA DLSS neural rendering  -- you supply it
└── reshade-shaders\Shaders\
    ├── DLSS5_Feed.fx             from the DLSS5-Feeder release
    └── ReShade.fxh               from ReShade's shaders
```

`mod.ini`:

```ini
name=DLSS 5
type=dlss5
category=graphics
```

On the first start the game writes `ReShade.ini` and `DLSS5-preset.ini`
into that folder if they are missing. It never overwrites them afterwards,
so you can edit them. They turn on the `DLSS5_Feed` technique with the
feeder's built-in motion estimator (`DLSS5_MV_PROVIDER=5`).

**Where the files come from.** UMK3 PC does not ship or download any of
them:

- ReShade: the official site, reshade.me ("with full add-on support").
  BSD-3-Clause.
- DLSS5-Feeder: its GitHub releases page, which also warns about fake
  copies. MIT. The 2.0.0 zip's SHA-256 published there is
  `fa41fe2c5d3da803688775b802dc373a922daab0e721c8154a17066fcdd62584`.
- `nvngx_dlss.dll`, `nvngx_dlssnr.dll` and the consumer add-on are
  NVIDIA's or their authors' files, and nobody here may redistribute them.
  Get them through the channels the DLSS5-Feeder README names.

## What the game checks before it loads anything

Each check writes a `dlss5 mod:` line in `logs\umk3-*.log`:

| Check | If it fails |
|---|---|
| a mod with `type=dlss5` is on (`mods\mods.ini`) and `dlss5_mod=0` is not set in umk3.ini | nothing happens, no line |
| renderer is Direct3D 12 | "needs RENDERER Direct3D 12" |
| an NVIDIA card is present | "needs an NVIDIA card" |
| there is no ReShade of your own beside the game (`dxgi.dll`, `d3d12.dll`, `ReShade.ini`) | "a ReShade of your own is beside the game" |
| every file above is present | each missing file named |
| both `nvngx_*.dll` say NVIDIA in their version information **and** have a valid digital signature | "is not NVIDIA's" / "has no valid signature" |

Only when every check passes does it load ReShade from the mod's folder.
ReShade's base folder is set to the mod's folder, so its settings, its log
and its add-ons all come from there. It never picks up add-ons from
anywhere else. The mod's folder is also put on the DLL search path, so the
add-ons find NVIDIA's DLLs there.

## Seeing whether it works

- `logs\umk3-*.log`: the `dlss5 mod:` lines, ending in "ReShade loaded
  from the mod".
- `mods\DLSS 5\ReShade.log`: ReShade attaching to Direct3D 12, the add-ons
  loading, and the shader compiling.
- `dlss5-feed.log` beside the game: the feeder. Look for "feature ready"
  and "frame N delivered".
- The consumer's log: "feature 18 created" and "inline feature 18
  evaluation succeeded" mean the neural pass really ran.

## Known limits

- **The HUD is processed with the scene.** The feeder works on the whole
  frame. Drawing the HUD after the neural pass is planned
  (`docs/research/GRAPHICS-OVERHAUL.md`, section 4.5).
- Once ReShade is loaded it stays loaded until the game closes, because
  ReShade cannot be unloaded. Turning the mod off takes effect at the
  next start.
- Online: the mod only changes the picture, and the fight and its sync are
  the same with or without it. It has not been played online yet.
- `Home` opens ReShade's overlay, as ReShade always does. You don't need
  it to play.

## Turning it off

Use the MOD MANAGER or EXTRAS > MODS (it takes effect at the next start),
or set `dlss5_mod=0` in umk3.ini, or delete the folder.
