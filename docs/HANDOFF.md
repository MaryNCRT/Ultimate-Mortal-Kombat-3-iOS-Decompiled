# Handoff Notes: Cross-Platform Launcher & Build Engine

**Date:** 2026-10-18  
**Relay Agent Contract:** See `AGENTS.md`.

---

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
