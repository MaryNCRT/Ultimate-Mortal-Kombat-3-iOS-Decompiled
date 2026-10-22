# Project Status & Verification Progress

**Last updated:** 2026-10-18 · See [CHANGELOG.md](CHANGELOG.md) for release history and [HOW-THE-GAME-WORKS.md](HOW-THE-GAME-WORKS.md) for architecture.

---

## Summary

| Area | Total Functions | Transcribed (`src/decomp/`) | Status |
|---|---:|---:|---|
| **LIME 3D Engine (`src/decomp/lime/`)** | 109 | 109 (100%) | Verified against recompiled oracle (`tests/test_*_diff.c`) |
| **Game Front-End & Scene (`src/decomp/gamecode/`)** | 291 | 291 (100%) | Verified & running natively (51 front-end screens, HUD, audio, blood) |
| **Arcade Fight Engine (`src/decomp/gamecode/logic/`)** | 2,172 | 2,172 (100%) | Behaviourally tested file-by-file (`tools/decomp/difftest/`) + 229 data tables verified by `ctest` |
| **Total Game Core** | **2,572** | **2,572 (100%)** | **98.30% weighted verification score (`tools/decomp/progress.py`)** |
| **Cross-Platform Launcher** | Multiplatform (Win/Linux) | 100% pure Node.js / Electron | Standalone compilation via `zig cc` (0.13.0), zero Python/CMake runtime dependency |

---

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
