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
| **Verification (`tools/decomp/`)** | **98.30%** | Differential tests against the `armrecomp` ARM→C oracle + build-time table layout verification (`ctest`). |
| **Native Port (`src/runtime/`)** | **Playable (v0.1.9-beta)** | OpenGL, Vulkan and Direct3D 12 renderers (`src/runtime/render/`), Win32 & SDL2 backends, 3:2 / 4:3 / 16:9 / 21:9 aspect ratios, HD textures, keyboard & gamepad support, local 2P (`DUELO`), LAN/Online multiplayer, and debug tools (`F2`). |

**Known problems in 0.1.9 beta:** Reptile's fatalities sometimes fail; the Linux package is still untested on a Linux machine (0.1.9 fixes the link error a tester hit, reads umk3.ini on Linux and adds `build-game.sh` and README-linux.md); the Vulkan and Direct3D 12 renderers are new and checked by `tests/rgl_test.c`, not yet in long play; a fighter cannot be duplicated into a new slot yet. New in 0.1.9: Vulkan and Direct3D 12 renderers beside OpenGL, the language changed from the in-game menu, the fatality page of the move list fixed, mod textures for the game's LOW/LITE copies, and many model-viewer additions. Since 0.1.9 (not released): SMOOTH ANIMATIONS, an on/off option that blends the fighters' frames when drawn.

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

On Windows and Linux, you can run **`UMK3-Launcher`** (built from [`launcher/app/`](launcher/app/)), which provides a self-contained arcade interface: it receives your `.ipa`, downloads the pinned `zig cc` compiler automatically (zero system compiler dependencies), unpacks the game assets, compiles the native game binary, and lets you configure graphics/controls (`umk3.ini`). Its **MODS** section manages mods (`mods/<mod>/`, files named like the game's replace them; priority and on/off in `mods/mods.ini`), shows the game's fighters, stages, models and textures, and exports them for Blender as FBX, USD (USDA/USDC/USDZ) or OBJ with PNG textures.

Available distribution packages:
- **Linux:** Standalone portable `.tar.gz` (0.1.9 beta: built, not yet tested on a Linux machine; see `launcher/README-linux.md`).
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

Original project code and tooling are released under the [MIT License](LICENSE). Banner renders by [ermaccer](https://github.com/ermaccer) ([CC BY 4.0](https://creativecommons.org/licenses/by/4.0/)); UHD logo by [u/JuananoLaGarza](https://www.reddit.com/r/MortalKombat/comments/mvm4uo/ultimate_mortal_kombat_3_logo_redone_in_uhd/).
