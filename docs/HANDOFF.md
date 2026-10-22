# Handoff Notes: Cross-Platform Launcher & Build Engine

**Date:** 2026-10-18  
**Relay Agent Contract:** See `AGENTS.md`.

---

## Debug menu, mouse wheel, iPhone-style boxes on Linux (2026-10-22, untested)

Port, not in the binary. Built for Windows x64 and x86; the Linux files
pass a zig `x86_64-linux-gnu` compile with the SDL2 lint header, but nobody
has run them on Linux yet. Mary has not tested any of this yet.

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

## Rollback netcode for ONLINE fights (2026-10-22, branch claude/nice-joliot-163d76, not on main)

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
- Not merged with the EOS transport work; the LAN transport is what it runs on today.

## 0. Latest (2026-10-22): Mileena's fatalities

Fixed and confirmed by the tester: the nail fatality's cup now follows Mileena's hand, and the suck-kiss absorption plays once. Details in `docs/PROGRESS.md` and `docs/CHANGELOG.md`. Still open from the same report: the fatality camera was never investigated. The EOS online work on `claude/online-eos` stays uncommitted until it is tested.

## 1. What was completed

1. **Pure Node.js Extraction Engine (`launcher/app/lib/extract/`)**:
   - The user no longer needs Python or CMake to build the game from an `.ipa`.
   - All extraction logic is implemented in pure Node.js with standard library modules (`fs`, `path`, `zlib`, `crypto`).
   - `macho.js`: 32-bit universal and thin Mach-O parser with armv7 slice detection, UUID extraction, and encryption checking (`cryptid`).
   - `zip.js`: Zero-dependency ZIP file reader supporting Store (0) and Deflate (8) decompression.
   - `logic_tables.js`: Generates `logic_tables.c` from binary `__DATA` and `logic_tables.map`. Produces byte-identical output to `tools/build/logic_tables.py` across 1,118 data objects and 4,291 pointer relocations.
   - `level_info.js`: Generates `level_info.c` (byte-identical to `tools/build/level_info.py`).
   - `seq_data.js`: Generates `seq_data.c` (byte-identical to `tools/build/seq_data.py`).
   - `low32_static.js`: Generates `gamecode_globals64.c` with `umk3_globals_fixup` (byte-identical to `tools/build/low32_static.py`).
   - `ipa.js`: High-level orchestrator extracting the 5,093 game assets into `res/` and generating all `.c` tables in ~2 seconds.

2. **Standalone Toolchain Manager (`launcher/app/lib/toolchain.js`)**:
   - Manages standalone `zig cc` (version 0.13.0).
   - Automatically downloads and extracts the official binary with SHA-256 verification if not already found in `~/.local/share/umk3-launcher/toolchain` (Linux) or `%LOCALAPPDATA%\umk3-launcher\toolchain` (Windows).

3. **Compiler Runner (`launcher/app/lib/compile.js`)**:
   - Compiles all 74 C source files in parallel using an async concurrency pool.
   - Windows: `-target x86-windows-gnu` / `x86_64-windows-gnu`, compiles `umk3_game.rc` with built-in `zig rc`.
   - Linux: Native compilation with `-DUMK3_LOW32`, `-no-pie`, `-DUMK3_NO_SDL_MIXER=1`, `-lSDL2`, `-lGL`, `-lm`.
   - Generates executable: `umk3-game` (Linux) or `umk3-game.exe` (Windows).

4. **Integration with Electron Frontend**:
   - Updated `launcher/app/lib/build.js`, `main.js`, `preload.js`, and `ini.js` to use the Node.js pipeline.
   - Added support for launching the native Linux binary (`umk3-game`) when clicking "JUGAR".
   - Removed legacy Windows-only PowerShell scripts (`launcher/build_game.ps1` and `launcher/setup_res.ps1`).

5. **Packaging & Release**:
   - Linux: `.tar.gz` and `.AppImage` via `npm run dist:linux`.
   - Windows: `.zip` and portable `.exe` via `npm run dist:win`.
   - GitHub Release `v0.2.1` published with all 4 packages attached.

6. **English Localization**:
   - All launcher progress, build errors, extraction statuses, and system dialogs translated into English.
   - Default UI language initialized to English (`EN`) with persistent configuration support.

---

## 2. Verification Results

- **`node --test launcher/app/test/*.js`**:
  - 15 passing unit tests (0 failures).
- **End-to-End Extraction & Compile**:
  - Extracted 5,093 assets and generated tables from `Ultimate_Mortal_Kombat__3_1.2.59_ios_3.0.ipa` in 1.8s.
  - Compiled and linked all 74 C source files with `zig cc` in 4.2s.
  - Verified game run on Linux: `UMK3_SHOT=10 ./umk3-game` created `umk3-game.ppm` (851x1413) without errors.
- **Packaging**:
  - Successfully produced `dist/umk3-launcher-0.1.5-alpha.tar.gz` (99MB) and `dist/UMK3 LAUNCHER-0.1.5-alpha.AppImage` (104MB).
- **Clean-room Audit**:
  - `python3 tools/decomp/check_banned_words.py`: 0 errors.
  - `bash tools/decomp/check.sh`: 0 errors.

---

## 3. How to Run / Test

- **Run Launcher in Development**:
  ```bash
  cd launcher/app
  npm start
  ```
- **Run Unit Tests**:
  ```bash
  cd launcher/app
  npm test
  ```
- **Build Linux Distribution Packages**:
  ```bash
  cd launcher/app
  npm run dist:linux
  ```
- **Build Windows Distribution Packages**:
  ```bash
  cd launcher/app
  npm run dist:win
  ```
