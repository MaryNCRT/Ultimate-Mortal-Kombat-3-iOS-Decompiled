# Handoff: how to pick up the work

**Last updated:** 2026-11-20 (release 0.5.8 beta: KOMBO SLIDE off gentler -- other.c `away_x_vel`, the air's push kept, the ground's halved; ENHANCED LIGHTING kept through finishers with no fighter caster -- render/rgl_ff.c `shadow_build`; the trainer's sign, announcer and way back. Next: ONLINE stage 4c, spectators, designed in docs/PROGRESS.md. Before that, release 0.5.7 beta: the FATALITY TRAINER in EXTRAS -- `src/runtime/fatal_trainer.c`, FrontEnd.c FE task 52, game_main.c `ft_frame`; ranges asked of the predicates, `tools/check_finisher_ranges.py`; ANIMALITY as MERCY then animality; the checklist; DualSense triggers through WinMM; the online code search asked again for 20 s; the pad's keyboard for a player ID and the trainer's windows with a pad. Before that, release 0.5.6 beta: 0.5.5's compile stopping on "icon: error: FileNotFound" fixed -- the icon is optional now; VERIFY; REPAIR repairs only what is damaged, downloading missing or damaged sources from the repository; the pages' hint line shows PAGE alone. The FATALITY TRAINER is begun on the branch `claude/fatality-trainer`. Before that, release 0.5.5 beta: fighters drawn indexed and the scene's passes on every core -- Scorpion's fatality lag; textures staged through one kept buffer -- shorter loads; SEGA DREAMCAST icons; the launcher's REPAIR, a failed file named, another version's objects cleaned, two columns, the window kept; dynamic stage music. Before that, release 0.5.4 beta: online -- one stage on both sides, no softlock at a round's end, no desync in a session's second fight, holds averaged, a 15 s stall leaves the fight; Linux's `long` in the port's files (the Steam Deck's 6 buttons, #171/#253); pads from SDL_GameControllerDB (`pad_db.h`), player two's pad on Linux; KOMBO SLIDE OFF with kicks (#252); CI's Linux check compiles as the launcher). What each release changed: [CHANGELOG.md](CHANGELOG.md).

This is for whoever continues the project — a person or an AI agent — with
no context. It says what to read, how the pieces are built and checked,
how a release is made, and what is open. Where things stand is in
[PROGRESS.md](PROGRESS.md) (its *Current state*); what each release
changed is in [CHANGELOG.md](CHANGELOG.md).

---

## 1. Read first

1. [AGENTS.md](../AGENTS.md): the rules. The iOS binary is the only source
   of truth; a function is done only when it is checked against it; work is
   committed and the docs updated at both ends of every session.
2. [README.md](../README.md): what the project is and what the port has.
3. [PROGRESS.md](PROGRESS.md) → *Current state* and *Open problems*.
4. [METHODOLOGY.md](METHODOLOGY.md) and [VERIFICATION.md](VERIFICATION.md)
   before touching anything in `src/decomp/`.
5. [ARCHITECTURE.md](ARCHITECTURE.md) and
   [HOW-THE-GAME-WORKS.md](HOW-THE-GAME-WORKS.md) for the code's shape.

## 2. How the team works

- **Mary** is the project's owner and tester. She plays every build, reports
  problems (GitHub issues, messages, screenshots) and sends logs
  (`logs\umk3-*.log` in the game folder). Changes are not "done" until she
  has played them; the CHANGELOG says which ones she has.
- **Fix locally first.** A change is built into her test folder, she tries
  it, and only then is it released. Her test folder is
  `WORK\release-0.1.12\Test` on the project machine (updated in place).
  Its `src\` is the tree's with
  CRLF line endings: compare with `diff --strip-trailing-cr`.
- **Investigate before fixing.** A gameplay problem is traced to the
  binary's own mechanism (disassembly, the recompiled oracle, her logs)
  before any change. Several "bugs" were the original game's own rules
  (issue #114's kombo slide, for one), and the answer is then to say so.
- **The word list.** `tools/check_banned_words.py` runs in CI; some words
  may not appear in tracked files (the tester is named Mary in the repo).
- **Every session ends on `main`.** Commit, open a PR, let CI pass, merge.
  An open PR is not "updated". README, PROGRESS, HANDOFF and CHANGELOG
  are updated with every change, including what Mary saw that is not fixed.

## 3. Building

### The game, into the test folder

The test folder carries its own copy of `src/` (the launcher compiles from
the folder it lives in). Copy every changed source file there, then build
with the launcher's CLI from the repository:

```bash
cd launcher/app
node cli.js "<path to UltimateMortalKombat3v1259.ipa>" --arch x64 --root "<test folder>"
```

Check the result by its log and by a string you added (`grep -c "<text>"
umk3-game64.exe`). A build that did not take the new source compiles the
old one silently.

### The launcher's interface

```bash
cd launcher/app
npm run ui          # builds ui/ into renderer/ (not `vite build` inside ui/)
```

To put a new launcher in the test folder, the `app.asar` is repacked from a
staging folder (`WORK/asar-stage`: `main.js`, `cli.js`, `preload.js`,
`package.json`, `lib/`, `renderer/`, `build/icon.ico`):

```bash
node node_modules/@electron/asar/bin/asar.js pack <stage> "<test folder>/launcher/bin/resources/app.asar"
```

To look at the interface in an ordinary browser with real data (no
Electron), `node scripts/ui-harness.js "<game folder>" [port]` serves it
and answers the preload's calls over HTTP.

### From source without the launcher

`python3 build.py <ipa>` or CMake (see the README).

## 4. Checking

| What | How |
|---|---|
| A decompiled function against the binary | `tools/decomp/disasm_range.py <slice> <start> <end>` for the disassembly; `tools/decomp/armrecomp/recomp.py <slice> --file <file>.c --out <dir>` for the mechanical translation (the oracle); `tools/decomp/factdiff.py <oracle .c> <our .c>` compares stores, calls and handlers. Handler addresses it reports unresolved are pointer slots: resolve them with `OUTPUT/func-to-file.txt` before calling them differences. |
| Behaviour, many inputs | `tools/decomp/difftest/run.sh <stem> <rc_root> -n N` (thread functions). For a function that only reads globals, which difftest does not generate tests for, compile the oracle's `recompiled.c` with `src/runtime/arm_runtime.c` beside the C and compare both over random states (done for `repell_func`, 200,000 states). Set `UMK3_WORK` / `UMK3_SLICE` to the folder holding `UMK3.armv7`, `symbols.txt`, `func-to-file.txt`. |
| What the tools cannot reach | the game in touchHLE ([touchHLE patch](engine/TOUCHHLE-PATCH.md)). |
| The whole tree compiles | `bash tools/decomp/check.sh` |
| Launcher logic | `node --test test/*.js` and `node test/stageedit.test.mjs` in `launcher/app` |
| Renderers | `tests/rgl_test.c` on OpenGL, Vulkan and Direct3D 12 before shipping render changes |
| Words | `python3 tools/check_banned_words.py` |
| Online, two games on one PC | the host `UMK3_LOBBY_TEST=host` (with `UMK3_LOBBY_RULES`, `UMK3_LOBBY_ROOM`), the guest `UMK3_LOBBY_TEST=join` with `UMK3_EOS_NEW_DEVICE=1`, its own `UMK3_SAVE_DIR` and `UMK3_PROFILES=profiles_j.ini`; the online select screen's fighter and PLAY tapped by `UMK3_TAPS` (portrait 235,62, PLAY 240,303); scripted inputs `UMK3_KEYS`; a bad line `UMK3_NET_LOSS` / `UMK3_NET_LAG` / `UMK3_NET_JITTER`. A desync: `UMK3_RB_SYNCTEST=1` (a frame run twice from one snapshot, the differing words named) and `UMK3_RB_DUMP=<frame>` (both games' memory at one frame, compared by name). Run test games with the sound off (`music_volume=0`, `sfx_volume=0` in umk3.ini). |
| Pad layouts | `tests/pad_db_test.c` (pad_db.h against `src/runtime/platform/gamecontrollerdb.txt`) |
| Linux compile, as the launcher does | `python3 tools/ci_compile_linux.py <cc> [args]` (zig works: `zig cc -target x86_64-linux-gnu`) |
| A heavy fatality, timed | `--fight scorpion kano 5` with `UMK3_TWOUP=1` (a two-player fight: FINISH HIM comes in the last round), `UMK3_DBG_KEY=400:0;1100:0`, `UMK3_DBG_FIN=400:2` (fatality 2's row); the log's `slow frame:` lines. Loads: `UMK3_LOG_LOADS=<ms>` and its `upload ...` lines |
| The launcher's start | `UMK3_ROOT=<game folder> UMK3_LAUNCHER_SHOT=<png>` on the test folder's `launcher\bin\UMK3-Launcher.exe` (the new code packed into its `app.asar`, above): `<png>.main.log` has the times |

## 5. Releasing

A release is a GitHub pre-release `v<version>-beta` with two packages:

1. Set the version in `launcher/app/package.json` (`0.2.1-beta`; a letter
   release is `0.1.14-beta.b`, read by the updater as 0.1.14b).
2. `npm run ui` in `launcher/app`.
3. Windows: `npx electron-builder --win dir --x64`, then
   `python launcher/make_release.py launcher/app/dist/win-unpacked <out> --zip`
   → `UMK3-PC-<version>.zip`.
4. Linux: `npx electron-builder --linux dir --x64`, then
   `python launcher/make_release_linux.py launcher/app/dist/linux-unpacked <out>.tar.gz`
   → `UMK3-PC-<version>-linux-x64.tar.gz`.
   Both scripts refuse to finish without the Epic Online Services SDK
   (`tools/eos`) and `eos.ini`.
5. `gh release create v<version> --prerelease --title ... --notes ...` with
   both files. The notes say what is new, what is known broken, and that
   Mary has not played it yet when she has not.
6. CHANGELOG, README status and PROGRESS updated on `main`.
   On the project machine (0.4.4): run the packagers with `python`
   (Python312), not `python3` -- that one (3.15, the install manager's)
   does not see `%LOCALAPPDATA%\umk3-launcher\toolchain`, where the zig
   they need is; `UMK3_EOS_INI=D:\MK3 PROJECT\TOOLS\eos\eos.ini`. The
   GitHub CLI is not installed there: the release is made on the web
   (Releases > Draft a new release, tag `v<version>`, pre-release).
7. WHAT'S NEW: an entry for the version in `launcher/app/lib/whatsnew.json`
   (ES and EN, a few lines for players). The launcher shows it once per
   version (`seen_version=` in umk3.ini) and from UPDATES' button.

The packages hold no game data: the player's `.ipa` supplies it when they
press COMPILE.

## 6. Where things are

| Area | Files |
|---|---|
| Fight engine (decompiled) | `src/decomp/gamecode/logic/` (`mkreact.c` reactions, `mkcombo.c` kombos, `mkrepell.c` fighter spacing, `other.c` helpers, `moves.c`, `joy.c`, `mkdrone.c` AI) |
| Front end (decompiled) | `src/decomp/gamecode/FrontEnd.c`, `GameCode.c` (tasks, loading, fight setup) |
| Port's menus | `src/runtime/debug_menu.c` (settings / Esc menu, debug menu, STAGE SELECT), `online_ui.c`, `settings_menu.c` |
| Main loop, config, aspect | `src/runtime/game_main.c` |
| Audio | `src/runtime/lime_menu.c` (the game's calls), `platform/win32_audio.c`, `platform/sdl_audio.c` |
| Renderers | `src/runtime/render/` |
| Replays, rollback | `src/runtime/rollback*.c` |
| Pads | `src/runtime/platform/win32_gl.c` (XInput, WinMM), `sdl_gl.c` (Linux), `pad_db.h` + `gamecontrollerdb.txt` (SDL_GameControllerDB, zlib); port files name game globals with `umk3_long` (savestate.h) |
| Launcher STEAM section | `launcher/app/lib/steam.js`, `vdf.js`, `artwork.js`, `steamsync.js`, `ui/src/lib/sections/SecSteam.svelte` ([STEAM-INTEGRATION.md](STEAM-INTEGRATION.md)) |
| Mods in the game | `src/runtime/mods.c`, `skin_mats.c` |
| Launcher main process | `launcher/app/main.js`, `preload.js`, `lib/` (compile, update, mods, replays, `hub.js` the MOD HUB) |
| MOD HUB storage | the separate repository MaryNCRT/UMK3-IOS-Mod-Hub: `index.json`, covers in `mods/<id>/`, each mod a release zip; publish with its `publish.js` |
| Launcher interface | `launcher/app/ui/src/lib/sections/` (`mods/` holds the viewers and editors), `lib/mods/` (readers, exporters, `stagerun.js`, `stageedit.js`), `i18n.js` (Spanish and English) |
| Release packaging | `launcher/make_release.py`, `make_release_linux.py`, `launcher/stub/` |

## 7. Open work

The list of problems is [PROGRESS.md -> Open problems](PROGRESS.md#open-problems);
each entry says what is known and what is needed next (usually a log from
Mary of the moment it happens). What follows is work in progress and the
rules that go with it.

- **Online, next**: a match between two machines with 0.5.4 (only two games
  on one PC so far). Then the ONLINE phase's stage 5, QUICK MATCH, and
  stage 4c, spectators ([research/ONLINE-PHASE-PLAN.md](research/ONLINE-PHASE-PLAN.md)).
  Not tried yet: kick, promote, a rules change in a room, ROUND 5, a series'
  rematch. In a rollback fight anything one machine reads that the other
  does not (a touch, an overlay, a packet, a setting) must not change the
  fight: the round summary's tap and packet 9 were that.
- **Pads**: SDL_GameControllerDB's layouts on Windows (WinMM) and Linux are
  in 0.5.4 but no such pad has been tried; a log's `pad:` lines name the
  layout taken. ISteamInput is not used (it needs the Steamworks SDK and an
  AppID; the game is a non-Steam game). Update `gamecontrollerdb.txt` from
  the upstream repository now and then.
- **Steam Deck**: the 6-button fix and the pad choice wait for a Deck. A
  report's log shows `pad: open ...` for each pad and which one player one
  took.
- **Colours 3-6 and outfits that add**: 23 fighters have colours 3-6
  (`palettes.c`, `aditional textures/palettes.ini`, `--make-colors`);
  online between two machines not tried. Asked by Mary: bake a colour into a
  texture file, added outfits growing to six colours with the player's own
  textures (launcher).
- **Discord Rich Presence** ([DISCORD-PRESENCE.md](DISCORD-PRESENCE.md)):
  joining a lobby from a friend's Join and Linux not tried.
- **Mary's roadmap (2026-11-07)**: (1) the ONLINE menu's Deception /
  Armageddon finish; (2) the launcher's renewal in UMK3 Xbox 360's identity;
  (3) ONLINE infrastructure (lobbies done; matchmaking, quick match,
  parties, friends' state); (4) new fight modes: Tag Team, Simul, King of
  the Hill; (5) reusable online systems, stability tests and docs. Her rules
  for the looks: every ONLINE section keeps its own UMK3 stage; the launcher
  and the ONLINE menu complement each other.
- **Accounts server**: Mary's private repository
  (MaryNCRT-Universal-Game-Services, `umk3-accounts/`, a Cloudflare Worker;
  she deploys it). Never put server code or secrets here. The launcher's
  client is `launcher/app/lib/account.js`. Leaderboards go through the
  launcher (`leaderboards.txt`, `scores.txt`); a run with the debug menu's
  help is not posted (`PortDbgTainted`).
- **Performance**: [PERFORMANCE-AUDIT.md](PERFORMANCE-AUDIT.md). Measure
  with the slow-frame line and a scripted fight (`UMK3_ARCADE=0,0`,
  `UMK3_DBG_KEY`, `UMK3_DBG_FIN`); `UMK3_PROFILE=1` names a slow frame's
  places. A window behind another measures the machine, not the game.
- **Lighting and research**: ENHANCED LIGHTING, point lights and AO are in;
  per-stage `lighting/<scene>.ini` files are tuned by Mary one by one.
  Research only: contact shadows -- see `docs/research/`.
- **Dynamic stage music** (`src/runtime/adaptive_music.c`, umk3.ini
  `dynamic_music=`, stretches in `music.ini` beside the game,
  `docs/music.ini.example`): in 0.5.5, Mary saw it work; the stretches per
  tune wait for her ear. The log's `music:` lines name every change. It reads
  the fight and writes nothing; keep it that way (online).
- **Fighters drawn indexed** (`rgl_ff.c` tris_indexed, `draw_indexed` in
  `rgl_d3d12.c` / `rgl_vk.c`): only fighters' meshes (`SH.caster`), never the
  stage or a glow, whose lights and notes read each corner. `UMK3_NO_INDEXED=1`
  compares. Vulkan's fatality still waits ~22 ms a frame on the GPU (open);
  `tests/rgl_test.c`'s 5 shadow checks need values for ENHANCED LIGHTING.
- **The launcher's compile**: `build\last-build.log` and the summary after
  EXIT CODE say which file failed; ask a player for that log. REPAIR keeps
  the .ipa's files. The player whose compile ended "EXIT CODE: -1" after
  omp_lite.c (0.5.4, an old version compiled before) has not sent a log.
- **Updates**: an installed update leaves `update\needs-build`; the game
  cannot be started from the launcher until a compile succeeds.
- **Building into the test folder without `node_modules`**: the folder's
  own launcher runs the CLI, as `build-game.cmd` does
  (`ELECTRON_RUN_AS_NODE=1`, `launcher\bin\UMK3-Launcher.exe
  launcher\bin\resources\app.asar\cli.js --root <Test> <ipa> --arch x64`);
  from PowerShell use `Start-Process -Wait`.
- Port diagnostics in the log for open or recent problems (`kombo push:`,
  `separate us:`, `corner trap:`, `death scream:`, `buttons: ... set
  outside the menus`, `stage select: the tune ...`) stay until Mary
  confirms.
