# Handoff: how to pick up the work

**Last updated:** 2026-11-13 (RELEASE 0.4.8 beta, and the MOD HUB's Sonya Blade MKX 2.0 as an outfit that adds; before it, OUTFITS THAT ADD: a mod with `type=skin` is another outfit beside the fighter's original -- the game's SKIN box, the launcher's OUTFIT / COLOR panel and a CHARACTER VIEWER that gives a fighter the whole window; built on the unmerged `claude/viewer-roster-special`, now merged in this branch. Before it, ONLINE stage 4b: the room -- a page of its own, 1 VS 1 / ROTATION, the line, the stage picked in the room, joining on request, a READY clock, DISSOLVE / MAKE LEADER; a tie on time and ROUNDS 1 no longer freeze a fight. Open: a second fight in one session counted desyncs in one test. Next: that, then stage 4c, spectators. Before it, ONLINE stage 4: the rules checked in HELLO -- other rules, no fight, both back in the lobby with a notice; HOST ADVANTAGE OFF draws the side and the stage; one player's touch controls online; an online fight or a lost link back to the lobby, not the original's screens. Before it, ONLINE stage 3: LOBBIES in the ONLINE screen -- the list, CREATE LOBBY, the room, invites, FIGHT -- the online select screen's banners, and the game on a second monitor of another refresh rate (20 fps on a 60 Hz monitor beside a 180 Hz main, fixed). Before it, release 0.4.5 beta: TRAINING's screen -- the moves window and the inputs, one player's controls -- DUELO against the CPU, the RULES button with its key. Before it, the ONLINE phase began: the rules block, `online_rules.c`, and the local versus's VS RULES window opened by SELECT, kodes ticked in a checklist; Mary played it. Then ONLINE stage 2, the EOS lobby core (`mplobby_*`), tested against the service with two probes. Next: LOBBIES in the ONLINE screen, [research/ONLINE-PHASE-PLAN.md](research/ONLINE-PHASE-PLAN.md) stage 3. Before it, 0.4.4 beta: the Lost Treasures' opponents from the binary, a click no longer reaching hidden controls, the first start's ORIGINAL / ENHANCED choice, WHAT'S NEW, new folders at the screen's largest resolution with the controls hidden. Before it, release 0.4.3 beta: the replay player in the ONLINE skin, STAGE SELECT without scrolling, the Esc menu's black box fixed; 0.4.2: STAGE SELECT in the ONLINE skin; the launcher renewed in dark blued steel, PROFILE rebuilt around the Player Banner, icon and favourite fighter one, Esc's PROFILE page removed).

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
  `WORK\release-0.1.12\Test` on the project machine (renamed from
  `UMK3-PC-0.1.12-beta`, now `WORK\release-0.1.12\Test`; updated in place). Its `src\` is the tree's with
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
| Launcher STEAM section | `launcher/app/lib/steam.js`, `vdf.js`, `artwork.js`, `steamsync.js`, `ui/src/lib/sections/SecSteam.svelte` ([STEAM-INTEGRATION.md](STEAM-INTEGRATION.md)) |
| Mods in the game | `src/runtime/mods.c`, `skin_mats.c` |
| Launcher main process | `launcher/app/main.js`, `preload.js`, `lib/` (compile, update, mods, replays, `hub.js` the MOD HUB) |
| MOD HUB storage | the separate repository MaryNCRT/UMK3-IOS-Mod-Hub: `index.json`, covers in `mods/<id>/`, each mod a release zip; publish with its `publish.js` |
| Launcher interface | `launcher/app/ui/src/lib/sections/` (`mods/` holds the viewers and editors), `lib/mods/` (readers, exporters, `stagerun.js`, `stageedit.js`), `i18n.js` (Spanish and English) |
| Release packaging | `launcher/make_release.py`, `make_release_linux.py`, `launcher/stub/` |

## 7. Open work

- **Colours 3-6** (2026-11-13, `src/runtime/palettes.c`, `aditional
  textures/palettes.ini`): Kung Lao only so far. Asked next by Mary: bake a
  good colour into a new texture file automatically, and added outfits
  starting with one colour and growing to six with the player's own
  textures (launcher). The launcher in the test folder still compiles from
  its old `app.asar`, whose `lib/compile.js` lacks `palettes.c`: build with
  the repository's `launcher/app/cli.js` or repack the asar.

- **Select screen's OUTFIT / COLOR lists** (2026-11-13, after 0.4.8,
  FrontEnd.c `drop_one`, game_main.c `drop_input`): built in the test
  folder and checked by script in Arcade and VS; not played by Mary yet.
  New keys T / C (`PK_OUTFIT`, `PK_COLOR`), pad north / west. The icons are
  `launcher/aditional textures/ui/` (copied to the test folder's
  `aditional textures/ui/` by hand).

- **0.4.8 beta released** (2026-11-13) with outfits that add, the CPU
  wearing one as often as colour 2, and the viewer's BACK / PANEL. The MOD
  HUB's `sonya-blade-mkx` is now 2.0, an outfit (`type=skin`) in the same
  folder, so UPDATE turns 1.0 into it (checked with `lib/hub.js` on a copy:
  7 files fetched, the 11 replacing ones dropped); release 1.0 deleted.
  An update keeps a 1.0 `.source` file (the hub treats it as the mod's
  own). Mary's test folder has the outfit twice once she updates (`Sonya
  MKX Skin` and the updated `Sonya Blade MKX`).
- **Outfits that add** (2026-11-13, `src/runtime/mods.c` "skins that add",
  `skin_mats.c`, Players.c `PortSkinLoad` / `PortFESkinAnim`, FrontEnd.c
  `skin_box` / `PortSkinOf`, packet 26; launcher `lib/mods.js` `skinOf` /
  `skinRead` / `skinCreate`, ModViewer's OUTFIT / COLOR): Mary played the
  MKX Sonya as an added outfit beside the original (both in one fight,
  colours 1 and 2) and found the launcher had dropped `color1=` from its
  mod.ini (fixed: the ini reader took no digits in keys; the game also
  finds a skin's sheets by their names now). Not tried: online between two
  machines (each side shows the other's outfit only if it has the same one,
  matched by fighter + model size + colour 1 size); the launcher's outfit
  panel by Mary in the launcher itself (it was checked in the UI harness on
  a copy of her mods; the test folder's `app.asar` was repacked, the old one
  kept as `app.asar.before-outfits.bak`). Test aid: `UMK3_SKIN="p1,p2"`
  with `--fight` picks each side's outfit by its place in the list.
  Her test folder's `mods.ini` was changed for the test: `Sonya Blade MKX`
  (the replacing mod) off, `Sonya MKX Skin` (the outfit) on; the old one is
  `mods.ini.before-skins.bak`.

- **Discord Rich Presence** ([DISCORD-PRESENCE.md](DISCORD-PRESENCE.md)):
  Mary saw it on her profile (2026-11-12). Still to try: a friend's Join
  into a lobby (two accounts, two PCs), and Linux. Launching the game
  from Discord (registering the game) is not done.

- **CHARACTER VIEWER reorganised** (2026-11-12, roster page, SPECIAL
  MODELS, ONLY WHAT THE MOD REPLACES, layers export, MODS over the whole
  window): in the test folder's launcher, Mary has not looked yet. The
  launcher's shot aid needs the root found: run from the test folder's
  `launcher\bin` it took `launcher\bin` as the root. Mary also asked for
  Discord Rich Presence + Steam + SteamGridDB integration (a long spec);
  not started, it is its own task.
- **The ONLINE phase** ([research/ONLINE-PHASE-PLAN.md](research/ONLINE-PHASE-PLAN.md)):
  stage 1, the rules block, is done and in the local versus (SELECT on its
  select screen); Mary played it. Stage 2, the EOS lobby core
  (`mplobby_*` in `mp_eos.c`), is done and tested with
  `tests/eos_lobby_probe.c` (two probes on one PC, the second with
  `UMK3_EOS_NEW_DEVICE=1`); kick, promote and host migration not tried
  yet. Stage 3, LOBBIES in the ONLINE screen, is done (list, key or name,
  CREATE, the room, public / private after, invites by code over P2P, FIGHT;
  two copies on one PC reached the online fight; Mary has not played it;
  kick, promote and a rules change in a room not tried). Test aid
  `UMK3_LOBBY_TEST`; see the plan. Stage 4, the rules checked in HELLO, is
  done (`rb_online_gate`; tested with two copies on one PC: equal rules
  fight, different rules refused on both, HOST ADVANTAGE OFF the same draw
  on both in four fights; a fight won and back in the lobby not seen; test
  aids `UMK3_HELLO_RULES`, `UMK3_LOBBY_RULES`; Mary has not played it).
  Next, stage 5: QUICK MATCH. Scripted keys (`UMK3_KEYS`) did not land hits
  in an online fight in those tests; the select screen's OK is tapped with
  `UMK3_TAPS` at 240,303. The fight's names on gold plates are PR #209,
  still open.
  Not seen yet on screen: ROUND 5, the coins past two, the
  series line under the clock. Scripted runs of the menus are not used in
  Mary's test folder (one went into Arcade's tower).
- **0.4.4 released** (2026-11-12): Mary confirmed the Lost Treasures,
  ENHANCED and WHAT'S NEW. The first start's account window and the
  launcher's new FRIENDS went out without her trying them in the launcher.
- **The UI harness** (`scripts/ui-harness.js`) answers `api.account.*` now:
  `UMK3_HARNESS_ACCOUNT=1` signs in with sample friends,
  `UMK3_HARNESS_FIRST=1` shows the first start's windows again.
- **Building into the test folder without `node_modules`**: the folder's
  own launcher runs the CLI, as `build-game.cmd` does
  (`ELECTRON_RUN_AS_NODE=1`, `launcher\bin\UMK3-Launcher.exe
  launcher\bin\resources\app.asar\cli.js --root <Test> <ipa> --arch x64`);
  from PowerShell use `Start-Process -Wait`, which waits for it.
- **factdiff on `GameInit_LoadABit`** did not finish in 25 minutes (3,300
  instructions); its GameMode 5 block was checked by reading the
  disassembly, not by the tool.

- **The launcher's look (Mary, 2026-11-07)**: dark blued steel, slate and
  smoke, gold details, deep red for the chosen; the UMK3 Xbox 360 layout
  kept. The Player Banner (gradient on steel, fighter out of it) is the
  same in the launcher and the ONLINE screen. The icon is the favourite
  fighter -- one field for the player.
- **ONLINE's look (Mary's rule)**: every section keeps its own UMK3 stage
  (`PortOnlineBGLoad`); the Deception / Armageddon finish works over them,
  never replaces them. This version is the visual base: refine materials,
  buttons and panels, do not change the concept.
- **Mary's roadmap (2026-11-07)**, in order: (1) finish the ONLINE menu's
  Deception / Armageddon finish (PR #196); (2) a full renewal of the
  launcher: UMK3 Xbox 360's identity with dark fantasy, rustic materials,
  aged metal and stone, ornament, ambient light -- it and the ONLINE menu
  must complement each other; the gradient editor and the customisation
  keep working; (3) ONLINE infrastructure: real EOS lobbies, matchmaking
  and quick match, parties and invites, friends' online state, into the
  fights; (4) new fight modes: Tag Team, Simul, King of the Hill; (5)
  reusable online systems with engine adapters, ready to carry to MK Gold,
  stability tests and docs.
- **Accounts server.** It lives in Mary's private repository
  (MaryNCRT-Universal-Game-Services, `umk3-accounts/`, a Cloudflare Worker;
  its README has deploying and testing). Never put server code or secrets
  here. The launcher's client is `launcher/app/lib/account.js`; its `SERVER`
  is the deployed address.
- **Leaderboards in the game** work through the launcher: it writes
  `leaderboards.txt` before PLAY and every 10 s while the game runs, and
  posts the game's `scores.txt` as it grows (gamecode_stubs.c,
  `lib/account.js`). A run with the debug menu's help is not posted
  (`PortDbgTainted`, game_main.c).
- **Server deploys**: `deploy.cmd` in the private repo's `umk3-accounts`
  (tests, then `wrangler deploy`); Mary runs it.
- **Performance**: [PERFORMANCE-AUDIT.md](PERFORMANCE-AUDIT.md) has the audit
  and the roadmap (step 1, the smoothing stencils, is done). Measure with
  the slow-frame line and a scripted fight: `UMK3_ARCADE=0,0` (else a
  `--fight` takes the menu's mode and may chain opponents with no FINISH
  HIM), `UMK3_DBG_KEY="600:0;1100:0"`, `UMK3_DBG_FIN="1000:2"`. A run whose
  window is behind another measures the machine, not the game.
- **To check online with two players (0.1.23)**: the guest's view of the
  host's STAGE SELECT, invites, no desyncs with a model mod, the other
  player's attack voices. Stages' first-frame pauses (STREET) are open.
- **Friends and the profile on the account**: `claim()` in `lib/account.js`
  ties the profile code and name to the account (names unique); FRIENDS is
  a page of PROFILE (`ProfFriends.svelte`). A server change that needs a new
  column goes in the private repo's `migrations/`, applied before deploying.
- **Moves list kombos**: the KOMBOS column falls back to the engine's
  kombo trees (`PortKombosRead`, GameCode.c) when `MovesListTab` has no
  section 3, for all 12 such fighters. An entry's low byte is the input
  window (6, 9 or 0xc), not a kind; `+4 == 0x1111` ends a string in a
  routine. The rows show buttons only (B+ for the roundhouse), not the
  directions the hand-written section-3 lists print.
- **Dynamic lighting**: ENHANCED LIGHTING (shadows, texture relief,
  specular, mipmaps) is in, and phases 1-2 of
  [research/DYNAMIC-LIGHTING.md](research/DYNAMIC-LIGHTING.md): every glow
  is a coloured point light on the stage and the fighters (`rgl_ff.c`
  `lights_build`). The two strongest also cast the fighters' shadows (phase 5,
  `point_shadows`). Each stage's sky tints it, and `lighting/<scene>.ini`
  (`launcher/lighting/README.txt`) tunes a stage and places its own lights
  (phase 4). AMBIENT OCCLUSION is in (`ao_passes`; its radius is converted to
  the camera's units, the view scales the world ~5x), with debug views in
  the debug menu. Released in 0.3.0; 0.3.1b keeps a depth clear between
  held draws in the AO depth pass (the intros clear it before the
  fighters); after 0.3.1b the fighters are a layer of their own in it
  (blended intro fighters were not in the depth). Next: lighting files
  for more stages (Mary tunes them one by one), per-fighter files,
  contact shadows, OpenGL (research/). Open: a 3.6 s freeze in the present
  before the main menu; the `d3d12 slow present:` log line splits it. The shaders
  are rebuilt with `render/shaders/build.py` (glslang in
  `WORK/tools/glslang`, fxc from the Windows SDK).
- **Input latency**: LOW LATENCY (`rgl_low_latency`) is in for all three
  renderers ([research/INPUT-LATENCY.md](research/INPUT-LATENCY.md)).
  Vulkan's needs `VK_KHR_present_wait`; without it Vulkan still queues as
  before. Not measured with a camera or a latency tester yet; if a frame
  over 16.7 ms now shows a refresh late, that is the trade (the option
  turns it off). Opening the settings menu cost ~150 ms in 0.3.1's
  logs; Mary no longer sees that stall in 0.3.1b (not looked into, so
  the cause is unknown).
- **Ambient occlusion and contact shadows**: investigated, not built. The
  designs (a technique selector Crytek SSAO / HBAO / Alchemy AO, and
  ray-marched contact shadows sharing one depth pre-pass in `scene_flush`)
  are in [research/AMBIENT-OCCLUSION.md](research/AMBIENT-OCCLUSION.md) and
  [research/CONTACT-SHADOWS.md](research/CONTACT-SHADOWS.md). Start after
  `claude/dynamic-lighting` is merged: both touch `ff.frag` / `ff.hlsl`.
  The shabaazxh AO repo has no license: ideas only, no code.
- **Dynamic stage music (Genesis style)**: investigated, not built; the
  design and the open questions are in
  [research/DYNAMIC-STAGE-MUSIC.md](research/DYNAMIC-STAGE-MUSIC.md). Paused
  until Mary records a Soul Chamber fight on the Genesis (WAV + GYM).
- **ONLINE menu redesign**: stages 1 (audit) and 2 (sections, HOME) done, PROFILE
  (my profile, friends, requests; 2026-11-06) and the visual redesign
  (Player Banners, gradients; PROGRESS 2026-11-06)
  (`FE_Task_Online`, FrontEnd.c; see PROGRESS); next is stage 3 (friends,
  profiles, invites). The
- **Updates**: since 2026-11-06 an installed update leaves
  `update\needs-build` and deletes `build\obj`; the game cannot be started
  from the launcher until a compile succeeds (`lib/update.js`
  `needsBuild`, `main.js` `startGame`). apply.ps1 kills what still runs
  from the folder instead of waiting forever.
- **ONLINE menu redesign**: stage 1 (audit) done, nothing built. The
  report is [research/ONLINE-MENU-AUDIT.md](research/ONLINE-MENU-AUDIT.md):
  the online stack as it is, what works and what is missing, where Best of
  N (`WinsNeeded`), time (`GameTime`), the VS kodes and KOMBO SLIDE live,
  what host advantage means with rollback, the server work needed, and a
  nine-stage plan. Waiting for Mary's answers (its section 10) before
  stage 2; nothing working is replaced until she approves.

The table in [PROGRESS.md → Open problems](PROGRESS.md#open-problems) is the
list. Each entry says what is known and what is needed next (usually a log
from Mary of the moment it happens). Port diagnostics that print to the log
for those problems (`death scream:`, `corner trap:`, `separate us:`,
`stage select: the tune ...`) stay until she confirms.
