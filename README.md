<div align="center">

<img src="docs/img/banner.jpg" alt="Ultimate Mortal Kombat 3 Recomp" width="880">

# Ultimate Mortal Kombat 3 — iOS Decompilation & Native PC Port

**Complete clean-room decompilation of the 2011 iOS release of *Ultimate Mortal Kombat 3* (v1.2.59) — all 2,572 game functions transcribed to C and verified — with a native PC port for Windows, Linux, and macOS.**

[Getting Started](docs/GETTING-STARTED.md) · [How the Game Works](docs/HOW-THE-GAME-WORKS.md) · [Architecture](docs/ARCHITECTURE.md) · [Progress](docs/PROGRESS.md) · [Changelog](docs/CHANGELOG.md) · [Español](README.es.md)

</div>

---

## Status

| Component | Progress | Notes |
|---|---|---|
| **Decompilation (`src/decomp/`)** | **2,572 / 2,572 (100%)** | LIME 3D engine (`109`), Front-End & Scene (`291`), Arcade Fight Engine (`2,172`). |
| **Verification (`tools/decomp/`)** | **100%** | Differential tests against the `armrecomp` ARM→C oracle + build-time table layout verification (`ctest`). |
| **Native Port (`src/runtime/`)** | **Playable (v0.1.13-beta)** | OpenGL, Vulkan and Direct3D 12 renderers (`src/runtime/render/`), Win32 & SDL2 backends, 3:2 / 4:3 / 16:9 / 21:9 aspect ratios, HD textures, keyboard & gamepad support, local 2P (`DUELO`), LAN/Online multiplayer, and debug tools (`F2`). |

**Known problems in 0.1.13 beta:** a fight intro can still drop a frame where smoothing two fighters takes more than a tick; friendships and animalities can repeat a death scream (open: the logs show the game asking for one per fatality, so what repeats is not known yet); a fighter cannot be duplicated into a new slot yet; losing to Motaro from the debug menu softlocks (open, needs a log). Not yet played by Mary: everything new in 0.1.11, 0.1.12 and 0.1.13. **After 0.1.13 (not released, not built or played yet):** the model viewer in tabs, with derived models (babality, frozen, cut ...), layers and texture thumbnails; REPLACE MODEL from FBX, GLB or OBJ with a preview window, each material keeping its own texture (its UVs, tiling and transparency; the game draws each part, `skin_mats.c`); online always vanilla but for models and textures; the replay player can be hidden and resized; replays name and load the rival actually fought (Arcade's was the previous one); seeking a replay stops what still sounds; one fighter on both sides of the select screen keeps its smooth animations; a VSYNC option; STAGE SELECT after the fighters is still to do. an update no longer stops on "ENOTEMPTY ... update\new" left by an earlier one (it unpacks beside what Windows will not let go); a launcher of 0.1.13 or older still needs the `update` folder deleted by hand. **New in 0.1.13 (not yet played by Mary):** REPLAYS: every local fight recorded (the last 5 kept, any kept for good, exported or imported in the launcher's new REPLAYS page) and played back in the game with a player -- pause, a frame at a time, 5 s back and on, speed, a bar to click, an X to close and a list to switch to another replay in the same game (its fighters and stage load, no restart) -- that logs any tick that differs from the recording (online fights not recorded yet; going back Windows only); online over the internet: the input delay follows the ping (2 to 6 frames), inputs sent the moment they are taken, a 12-frame rollback window (Mary saw lag against another PC; not yet tried again); REAL-TIME SHADOWS' note is a row of its own (it was drawn over the label; seen right by Mary); the game's memory may use all of the first 4 GB (was 2), limeMalloc's heap reserves 2 GB (was 768 MB): no address the game keeps is sign-extended (checked by the compiler and by running the game with all its memory above 2 GB); Vulkan and Direct3D 12 on a 120/180/240 Hz screen hold each frame 2/3/4 refreshes (the stutter: a frame ending near a refresh waited one more, 22 ms then 11); COMPILAR SHADERS ANTES DE JUGAR in the launcher's PLAY and pipeline caches on disk; Sub-Zero's strap no longer stretches to his fist with SMOOTH MODELS (the weld keeps vertices of different bones apart). **New in 0.1.12:** Scorpion's fatality no longer drops to one frame a second (the smoothing budget now counted per frame, no 256 MB vertex-buffer ceiling on Direct3D 12 and Vulkan), subdivision on several cores, FLAWLESS VICTORY on match point; the engine's heaps stay below 2 GB by design (pointers in 32-bit words), ~386 MB used (below 4 GB after 0.1.12). **New in 0.1.11:** launcher updates that finish (every Windows update since 0.1.6 had ended as "could not be copied" and deleted `tools\eos\`); the game no longer crashes when its window is resized or moved several times; online over the internet in the download again, the EOS SDK in the repository (`tools/eos`); the home ring's picture with the launcher; faster loads (texture uploads batched on Direct3D 12 and Vulkan) and compiles (incremental, 13.9 s to 1.2 s with nothing changed); no lag that grows over a long session (textures found by a hash); the character editor works on Motaro; PROFILE's gradient picker and the second outfit as the player's picture, in the launcher and the same in the game's Esc menu; sharp edges for SMOOTH MODELS with two sliders, in the launcher and the game; Kung Lao's hat no longer pierced by his smoothed head. Tried by Mary for 0.1.10b: the Linux package works, and the Vulkan and Direct3D 12 renderers play. New in 0.1.10b: a VIDEO SETTINGS box on the settings screen (top right, above SOUND VOLUME): the GRAPHICS page in the game's own panel, with the on-screen controls on or off; the settings in two columns (Esc menu, debug menu, the game's panels), each section kept whole in one column, a row too long for its column drawn smaller; the controls split into FIGHT and MENU (tried by Mary before the section rule and the FIGHT / MENU split; those not yet). New in 0.1.10: kombos go on with the move that matched (issue #114: a later entry of a combo table ran the first entry's next move, likely Liu Kang's 7-hit kombo breaking; not yet tried in the game), SMOOTH ANIMATIONS and SMOOTH MODELS (Blender's weld + Catmull-Clark, in the game and the viewer), MODS > TEXTURES and exports by folder, stages exported with their animation, Linux sound, and crash reports naming the version and settings; the project is GPL-2.0-or-later from here on.

> [!IMPORTANT]
> **No copyrighted assets or binaries are distributed in this repository.**
> To build and play, you must supply your own legally obtained copy of **Ultimate Mortal Kombat 3 for iOS (v1.2.59)** (`.ipa`, `armv7` UUID `90d6f56a18e2303f8f2053b02a42742e`).

---

## Quick Build (from your `.ipa`)

### 1. Single-Command Build (`build.py`)

Place your `Ultimate_Mortal_Kombat_3_1.2.59.ipa` in the repository root (or pass its path) and run:

```bash
# Unpacks and validates the 1.2.59 .ipa, extracts the 229 binary data tables,
# compiles umk3-game, and prepares a ready-to-play folder in dist/<platform>-<arch>/
python3 build.py path/to/UMK3_1.2.59.ipa

# Cross-compile the Windows .exe from Linux (runs under Wine/Proton) and run ctest:
python3 build.py path/to/UMK3_1.2.59.ipa --platform windows --target all

# Build and run the native 32-bit headless fight simulation & table verification suite:
python3 build.py path/to/UMK3_1.2.59.ipa --target headless
```

### 2. Direct CMake Build

```bash
cmake -S . -B build -G Ninja -DUMK3_IPA=path/to/UMK3_1.2.59.ipa
cmake --build build --target umk3-game
```

### 3. Cross-Platform Desktop Launcher (Windows & Linux)

On Windows and Linux, you can run **`UMK3-Launcher`** (built from [`launcher/app/`](launcher/app/)), which provides a self-contained arcade interface: it receives your `.ipa`, downloads the pinned `zig cc` compiler automatically (zero system compiler dependencies), unpacks the game assets, compiles the native game binary, and lets you configure graphics/controls (`umk3.ini`). Its **MODS** section manages mods (`mods/<mod>/`, files named like the game's replace them; priority and on/off in `mods/mods.ini`), shows the game's fighters, stages, models and textures, replaces any texture of a fighter, a stage, the interface or the effects (TEXTURES), and exports them for Blender as FBX, USD (USDA/USDC/USDZ) or OBJ with PNG textures, stages with their animation; exports and mods keep each fighter, stage, the interface and the effects in a folder of its own.

Available distribution packages:
- **Linux:** Standalone portable `.tar.gz` (works on a Linux machine since 0.1.10b, tried by Mary; see `launcher/README-linux.md`).
- **Windows:** Portable `.zip` and `.exe`.

---

## Controls

| Action | Player 1 (Keyboard) | Gamepad |
|---|---|---|
| **Move** | `W` `A` `S` `D` or Arrow Keys | D-Pad / Left Stick |
| **High Punch / Low Punch** | `U` / `I` (or Numpad `7` / `8`) | West / North |
| **Block** | `O` (or Numpad `9`) | Right Shoulder / Trigger |
| **High Kick / Low Kick** | `J` / `K` (or Numpad `4` / `5`) | South / East |
| **Run** | `L` (or Numpad `6`) | Left Shoulder / Trigger |
| **Pause / Moves List** | `P` / `M` | `Start` / `Back` (`Select`) |
| **Settings Menu** | `Esc` | Configurable (`pad_settings`) |
| **Debug Menu** (`debug_keys=1`) | `F2` (`F3` HUD, `F9`/`F10` KO, `F11`/`F12` Match) | `L3 + R3` |

---

## Repository Layout

```text
├── build.py                 # Unified cross-platform build script from a 1.2.59 .ipa
├── CMakeLists.txt           # CMake build configuration (umk3-game & umk3-fight-headless)
├── src/
│   ├── decomp/              # 1:1 decompiled C from the iOS armv7 binary (2,572 functions)
│   │   ├── lime/            # LIME 3D engine core (matrices, meshes, scenes, fonts, camera)
│   │   ├── gamecode/        # Front-end menus, HUD, 3D stage/player rendering, audio, MP
│   │   └── gamecode/logic/  # Midway arcade fight engine (moves, AI, combos, finishers)
│   └── runtime/             # Native PC port layer surrounding the decompiled code
│       ├── platform/        # OS windowing, OpenGL context, audio & input (Win32 & SDL2)
│       └── lime/            # Native asset loaders (PVRTC, PNG, meshset, skin, scene, par)
├── tools/
│   ├── build/               # Build-time extractors (.ipa unpacker, Mach-O parser, 229 data tables)
│   └── decomp/              # Reverse-engineering tools (armrecomp, difftest, disassembler, checkers)
├── launcher/                # Electron desktop launcher, auto-updater, and release packager
├── tests/                   # Differential verification harnesses for LIME engine modules
└── docs/                    # Technical documentation
    ├── formats/             # Binary asset specifications (.meshset, .skin, .scene, .events, .pvr)
    └── engine/              # Engine internals, physics, move/fatality tables, touchHLE patch
```

---

## Documentation

- **Core Guides:** [Getting Started](docs/GETTING-STARTED.md) · [How the Game Works](docs/HOW-THE-GAME-WORKS.md) · [Architecture](docs/ARCHITECTURE.md) · [Methodology](docs/METHODOLOGY.md) · [Verification](docs/VERIFICATION.md)
- **Asset Formats (`docs/formats/`):** [MeshSet](docs/formats/MESHSET-FORMAT.md) · [Skin & Bones](docs/formats/SKIN-FORMAT.md) · [Scene](docs/formats/SCENE-FORMAT.md) · [Events](docs/formats/EVENTS-FORMAT.md) · [PVR / PVRTC](docs/formats/PVR-FORMAT.md) · [Fonts](docs/formats/FONT-FORMAT.md) · [Lighting](docs/formats/LIGHTING.md) · [Animation Streams](docs/formats/ANIMATION-STREAMS.md) · [Frame Lists](docs/formats/FRAMELISTS.md) · [X-Tables](docs/formats/X-TABLES.md)
- **Engine Reference (`docs/engine/`):** [LIME Engine](docs/engine/LIME-ENGINE.md) · [Physics](docs/engine/PHYSICS.md) · [Moves Tables](docs/engine/MOVES-TABLES.md) · [Fatality Tables](docs/engine/FATALITY-TABLES.md) · [Game Events](docs/engine/GAME-EVENTS.md) · [Roster](docs/engine/ROSTER.md) · [Stages](docs/engine/STAGES.md) · [Game Bugs](docs/engine/GAME-BUGS.md) · [Hidden Content](docs/engine/HIDDEN-CONTENT.md) · [iPad Build](docs/engine/IPAD-BUILD.md) · [MAME Arcade Reference](docs/engine/MAME-ARCADE.md) · [Mesh Viewer](docs/engine/MESH-VIEWER.md) · [touchHLE Patch](docs/engine/TOUCHHLE-PATCH.md)
- **Project Info:** [Goals](docs/GOALS.md) · [Progress](docs/PROGRESS.md) · [Changelog](docs/CHANGELOG.md) · [Contributing](docs/CONTRIBUTING.md) · [AI Disclosure](docs/AI-DISCLOSURE.md)

---

## Legal & Credits

*Ultimate Mortal Kombat 3* was created by **Midway Games** (1995) and ported to iOS by **EA Mobile** (2011). This project is an independent clean-room reverse-engineering effort for interoperability and software preservation, unaffiliated with Warner Bros. Entertainment or Electronic Arts. No copyrighted code or assets are distributed here.

The project is released under the [GNU GPL, version 2 or later](LICENSE): it contains code from [Blender](https://www.blender.org) (`src/thirdparty/blender`, the weld and Catmull-Clark of SMOOTH MODELS). Releases up to 0.1.9-beta were MIT and stay so. Banner renders by [ermaccer](https://github.com/ermaccer) ([CC BY 4.0](https://creativecommons.org/licenses/by/4.0/)); UHD logo by [u/JuananoLaGarza](https://www.reddit.com/r/MortalKombat/comments/mvm4uo/ultimate_mortal_kombat_3_logo_redone_in_uhd/).
