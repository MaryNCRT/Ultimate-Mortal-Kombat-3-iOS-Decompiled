<div align="center">

<img src="docs/img/banner.jpg" alt="Ultimate Mortal Kombat 3 Recomp" width="880">

# Ultimate Mortal Kombat 3 — iOS Decompilation & Native PC Port

**A clean-room decompilation of the 2011 iOS release of *Ultimate Mortal Kombat 3* (v1.2.59): all 2,572 game functions transcribed to C and checked against the original binary, with a native PC port for Windows and Linux built from the player's own copy of the game.**

[Getting Started](docs/GETTING-STARTED.md) · [How the Game Works](docs/HOW-THE-GAME-WORKS.md) · [Architecture](docs/ARCHITECTURE.md) · [Methodology](docs/METHODOLOGY.md) · [Progress](docs/PROGRESS.md) · [Changelog](docs/CHANGELOG.md) · [Español](README.es.md)

</div>

---

> [!IMPORTANT]
> **No copyrighted assets or binaries are distributed in this repository.**
> To build and play you need your own legally obtained copy of **Ultimate Mortal Kombat 3 for iOS (v1.2.59)** (`.ipa`, `armv7` UUID `90d6f56a18e2303f8f2053b02a42742e`). The launcher compiles the game from it on your machine.

## What this project is

The iOS game is a 2011 EA Mobile port of Midway's 1995 arcade game: Midway's fight engine (moves, AI, combos, finishers) running inside EA's own 3D engine, LIME. This repository holds:

1. **The decompilation** (`src/decomp/`): every function of the game's armv7 binary written as readable C, each one checked against the binary (see [Methodology](#how-the-work-is-done)).
2. **The native port** (`src/runtime/`): what the iPhone gave the game, written for a PC — the window, OpenGL / Vulkan / Direct3D 12 drawing, sound, keyboard and pads, files, online play — plus features the original never had (replays, mods, smoothing, wide screens).
3. **The launcher** (`launcher/`): a desktop program that takes your `.ipa`, compiles the game, configures it, updates it, and has a full set of modding tools (viewers, texture and model replacement, Blender-style editors, exports).

The only source of truth is the binary. Nothing enters the tree that is not derived from it and checked against it ([AGENTS.md](AGENTS.md) explains why, and [AI Disclosure](docs/AI-DISCLOSURE.md) who did what).

## Status

| Component | Progress | Notes |
|---|---|---|
| **Decompilation** (`src/decomp/`) | **2,572 / 2,572 (100%)** | LIME 3D engine (109), front end & scenes (291), arcade fight engine (2,172). |
| **Verification** (`tools/decomp/`) | **100%** | Differential tests against the `armrecomp` ARM→C oracle; 229 data tables checked at build time (`ctest`). |
| **PC port** (`src/runtime/`) | **Playable — v0.1.18 beta** | OpenGL, Vulkan, Direct3D 12; Windows and Linux; 3:2 / 4:3 / 16:9 / 21:9; keyboard and pads; local 2 players, LAN and online with rollback; replays; mods. |
| **Launcher** (`launcher/`) | Windows and Linux | Compiles the game with a bundled `zig cc` (nothing to install), updates itself, MODS tools. |

Every release is tested by the project's tester, Mary, before the next one builds on it; the [Changelog](docs/CHANGELOG.md) says for each change whether she has played it yet.

## Playing it

1. Download the latest release from the [Releases page](https://github.com/MaryNCRT/Ultimate-Mortal-Kombat-3-iOS-Decompiled/releases): `UMK3-PC-<version>.zip` (Windows) or `UMK3-PC-<version>-linux-x64.tar.gz` (Linux).
2. Unpack it anywhere and open **`UMK3-Launcher`**.
3. Give it your `.ipa` (**COMPILE YOUR .IPA**). It unpacks the game's data into the folder, extracts the binary's data tables, and compiles the game. Nothing of the game leaves your machine.
4. **PLAY.** Graphics, controls, language and the rest are in the launcher and in the game's own settings menu (`Esc`).

Later versions arrive through the launcher's own updater. Your mods, saves, replays and settings stay in the folder.

### Controls

| Action | Player 1 (keyboard) | Pad |
|---|---|---|
| Move | `W` `A` `S` `D` or arrows | D-pad / left stick |
| High punch / low punch | `U` / `I` (numpad `7` / `8`) | West / North |
| Block | `O` (numpad `9`) | Right shoulder / trigger |
| High kick / low kick | `J` / `K` (numpad `4` / `5`) | South / East |
| Run | `L` (numpad `6`) | Left shoulder / trigger |
| Pause / move list | `P` / `M` | Start / Back |
| Settings | `Esc` | configurable (`pad_settings`) |
| Debug menu (`debug_keys=1`) | `F2` (`F3` HUD, `F9`/`F10` KO, `F11`/`F12` match) | `L3 + R3` |

All of them can be changed in the launcher (CONTROLS) or in the game (`Esc`).

## What the port has

**The game, as on the device.** Every mode of the iOS game (arcade tower, VS, training, survival, karnage, treasures, kodes), its menus, HUD, finishers, sounds and music, from the decompiled code. The iPad build's layout is used on 4:3.

**Added by the port:**
- **Renderers:** OpenGL, Vulkan and Direct3D 12 (`src/runtime/render/`), HD textures, antialiasing, real-time shadows, VSYNC, pipeline caches compiled ahead of play.
- **Screens:** 3:2, 4:3, 16:9 and 21:9; menus and panels laid out for each.
- **SMOOTH ANIMATIONS** (frames between the game's frames) and **SMOOTH MODELS** (Blender's weld + Catmull-Clark subdivision, with sharp-edge controls).
- **STAGE SELECT** before VS, training and online fights, with a picture of every stage; the select screen's music plays on through it.
- **Online** over LAN or the internet (Epic Online Services), rollback netcode with an input delay that follows the ping; online fights always use the game's own data (only models and textures from mods).
- **REPLAYS:** every local fight recorded and played back in the game with a full player (pause, frame step, seek, speed).
- **Mods:** a folder per mod in `mods\`, files named like the game's replace them; data overrides for the fight tables (`data.ini`).
- **Debug menu** (`F2`): any fighter including the bosses, stage, round, hitboxes, save states.

**The launcher's MODS section:**
- **MOD MANAGER:** order, on/off, new mod, open folder.
- **MOD HUB:** mods published by Mary and her collaborators in [UMK3-IOS-Mod-Hub](https://github.com/MaryNCRT/UMK3-IOS-Mod-Hub), downloaded into `mods\` with a click and updated when a newer version is out.
- **CHARACTER VIEWER:** every fighter with its outfits, derived models, animations, a stage behind it; texture replacement; **REPLACE MODEL** from FBX, GLB or OBJ (humanoid rigs retargeted, weights painted and smoothed, bones posed with a Blender-style gizmo, shape keys with a keyed timeline); corrective animation layers; exports for Blender.
- **STAGE VIEWER:** a stage played as the engine plays it, with its effects; elements picked, isolated and edited as in Blender (move / rotate / scale, keyframes, a dope-sheet timeline, shape keys and sculpting), kept per stage and exported as GLB, FBX or USD; its textures seen and replaced.
- **CHARACTER EDITOR:** a fighter's attacks, kombos, data and texts.
- **TEXTURES:** every texture of the game by where it belongs, replaced into a mod.
- **EXPORT:** fighters, stages, models and textures as FBX, GLB, USD (USDA/USDC/USDZ) or OBJ with PNG textures.

## Known problems (0.1.18 beta)

- Earlier entries (a dropped intro frame, the repeated death scream, duplicating a fighter, the Motaro softlock, SCULPT's "500 Internal Error") were closed by Mary in 0.1.18: she no longer sees them.
- In a kombo the one hit slides back slightly after a few punches (issue #114). That is the original game's own rule (`t_ccp3` → `t_separate_us`), checked against the binary instruction by instruction.

## Building from source

The launcher is the normal way. Without it:

```bash
# unpack and check the .ipa, extract the 229 data tables, compile umk3-game,
# and lay out a folder ready to play in dist/<platform>-<arch>/
python3 build.py path/to/UMK3_1.2.59.ipa

# the Windows .exe cross-compiled from Linux, and ctest
python3 build.py path/to/UMK3_1.2.59.ipa --platform windows --target all

# the 32-bit headless fight simulation and the table checks
python3 build.py path/to/UMK3_1.2.59.ipa --target headless
```

or with CMake directly:

```bash
cmake -S . -B build -G Ninja -DUMK3_IPA=path/to/UMK3_1.2.59.ipa
cmake --build build --target umk3-game
```

The launcher's interface is a SvelteKit app built with `npm run ui` in `launcher/app` (see [Getting Started](docs/GETTING-STARTED.md)).

## How the work is done

- **The binary is the reference.** A function is written from its disassembly (`tools/decomp/disasm_range.py`), never from memory or from other releases of the game.
- **Each function is checked.** `tools/decomp/armrecomp/recomp.py` translates the ARM code mechanically into C (the oracle); the hand-written C is compared with it fact by fact (`factdiff.py`: stores, calls, handlers) and by running both on thousands of generated inputs (`tools/decomp/difftest/`). Data tables are checked at build time.
- **Behaviour the tools cannot reach** (indirect calls, jump tables) is checked by observation against the game running in touchHLE ([touchHLE patch](docs/engine/TOUCHHLE-PATCH.md)).
- **Port code is marked.** Anything the binary does not have is labelled `Port, not in the binary` where it is.
- **The tester plays every build** before it counts as working; problems are traced to the binary's own mechanism before anything is changed.

[Methodology](docs/METHODOLOGY.md) and [Verification](docs/VERIFICATION.md) describe all of it in detail.

## Repository layout

```text
├── build.py              one-command build from a 1.2.59 .ipa
├── CMakeLists.txt        umk3-game and umk3-fight-headless
├── src/
│   ├── decomp/           the decompiled C (2,572 functions)
│   │   ├── lime/         LIME 3D engine (matrices, meshes, scenes, fonts, camera)
│   │   ├── gamecode/     front end, HUD, stage and fighter drawing, sound, multiplayer
│   │   └── gamecode/logic/  the arcade fight engine (moves, AI, combos, finishers)
│   ├── runtime/          the PC port around it (window, renderers, audio, input,
│   │                     online, replays, mods, menus added by the port)
│   └── thirdparty/       Blender's subdivision code (SMOOTH MODELS)
├── tools/
│   ├── build/            build-time extractors (.ipa, Mach-O, the 229 data tables)
│   └── decomp/           reverse-engineering tools (armrecomp, difftest, factdiff, disassembler)
├── launcher/             the desktop launcher (Electron + SvelteKit), updater, release packager
├── tests/                differential tests of the LIME engine modules
└── docs/                 documentation (below)
```

## Documentation

- **Start here:** [Getting Started](docs/GETTING-STARTED.md) · [How the Game Works](docs/HOW-THE-GAME-WORKS.md) · [Architecture](docs/ARCHITECTURE.md)
- **How it is made:** [Methodology](docs/METHODOLOGY.md) · [Verification](docs/VERIFICATION.md) · [Contributing](docs/CONTRIBUTING.md) · [AGENTS.md](AGENTS.md)
- **Where it stands:** [Progress](docs/PROGRESS.md) (current state, open problems) · [Handoff](docs/HANDOFF.md) (how to pick up the work) · [Changelog](docs/CHANGELOG.md) (every release) · [Goals](docs/GOALS.md)
- **Asset formats** (`docs/formats/`): [MeshSet](docs/formats/MESHSET-FORMAT.md) · [Skin & Bones](docs/formats/SKIN-FORMAT.md) · [Scene](docs/formats/SCENE-FORMAT.md) · [Events](docs/formats/EVENTS-FORMAT.md) · [PVR / PVRTC](docs/formats/PVR-FORMAT.md) · [Fonts](docs/formats/FONT-FORMAT.md) · [Lighting](docs/formats/LIGHTING.md) · [Animation Streams](docs/formats/ANIMATION-STREAMS.md) · [Frame Lists](docs/formats/FRAMELISTS.md) · [X-Tables](docs/formats/X-TABLES.md)
- **Engine reference** (`docs/engine/`): [LIME Engine](docs/engine/LIME-ENGINE.md) · [Physics](docs/engine/PHYSICS.md) · [Moves Tables](docs/engine/MOVES-TABLES.md) · [Fatality Tables](docs/engine/FATALITY-TABLES.md) · [Game Events](docs/engine/GAME-EVENTS.md) · [Roster](docs/engine/ROSTER.md) · [Stages](docs/engine/STAGES.md) · [Game Bugs](docs/engine/GAME-BUGS.md) · [Hidden Content](docs/engine/HIDDEN-CONTENT.md) · [iPad Build](docs/engine/IPAD-BUILD.md) · [MAME Arcade Reference](docs/engine/MAME-ARCADE.md) · [Mesh Viewer](docs/engine/MESH-VIEWER.md) · [touchHLE Patch](docs/engine/TOUCHHLE-PATCH.md)

## Legal & credits

*Ultimate Mortal Kombat 3* was created by **Midway Games** (1995) and ported to iOS by **EA Mobile** (2011). This project is an independent clean-room reverse-engineering effort for interoperability and software preservation, unaffiliated with Warner Bros. Entertainment or Electronic Arts. No copyrighted code or assets are distributed here.

The project is released under the [GNU GPL, version 2 or later](LICENSE): it contains code from [Blender](https://www.blender.org) (`src/thirdparty/blender`, the weld and Catmull-Clark of SMOOTH MODELS). Releases up to 0.1.9-beta were MIT and stay so. Banner renders by [ermaccer](https://github.com/ermaccer) ([CC BY 4.0](https://creativecommons.org/licenses/by/4.0/)); UHD logo by [u/JuananoLaGarza](https://www.reddit.com/r/MortalKombat/comments/mvm4uo/ultimate_mortal_kombat_3_logo_redone_in_uhd/).
